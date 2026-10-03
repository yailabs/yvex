// Operator control of durable Source-owned acquisition, never a PID-only kill.
use crate::{
    Output, catalog,
    ffi::{self, acquisition as native, raw},
    presentation,
    registry::{Invocation, Refusal},
};
use rustix::process::{self, Pid, Signal};
use serde_json::json;
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use std::{
    fs,
    io::{self, IsTerminal, Write},
    path::{Component, Path, PathBuf},
};
pub(crate) type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;
pub(crate) type Provenance = raw::yvex_source_acquisition_provenance;

fn refusal(message: &str) -> Box<dyn std::error::Error> {
    Box::new(Refusal {
        reason: message.into(),
        hint: None,
    })
}

pub(crate) fn now() -> Result<u64> {
    Ok(SystemTime::now().duration_since(UNIX_EPOCH)?.as_secs())
}

fn selected(invocation: &Invocation<'_>) -> Result<Box<Provenance>> {
    let root = invocation.value("--models-root");
    let mut provenance = if invocation.operation.operation_id.starts_with("model.") {
        let (_library, _index, snapshot) = catalog::selected_model(invocation)?;
        let sources = snapshot
            .sources
            .iter()
            .filter(|source| {
                ffi::text(&source.provider) == "huggingface"
                    && source.repository[0] != 0
                    && source.revision[0] != 0
            })
            .collect::<Vec<_>>();
        if sources.len() != 1 {
            return Err(refusal(
                "model acquisition control requires exactly one provider acquisition",
            ));
        }
        native::seed(root, &ffi::text(&sources[0].name))?
    } else {
        crate::acquire::identity(invocation, true)?
    };
    if invocation
        .value("--revision")
        .is_some_and(|revision| revision != ffi::text(&provenance.revision))
        || invocation
            .value("--repo")
            .is_some_and(|repo| repo != ffi::text(&provenance.repo_id))
    {
        return Err(refusal(
            "requested acquisition identity differs from the retained record",
        ));
    }
    if provenance.found == 0 {
        let includes = native::patterns(false)?;
        let excludes = native::patterns(true)?;
        native::configure(root, &mut provenance, &includes, &excludes, false)?;
    }
    Ok(provenance)
}

pub(crate) fn unchanged(current: &native::Operation, expected: &native::Operation) -> bool {
    current.generation == expected.generation && current.operation_id == expected.operation_id
}

// Validate selected storage against Source's canonical path formation. A forged
// retained record is never authority to traverse or remove an arbitrary path.
pub(crate) fn guarded(provenance: &Provenance, root: &str) -> Result<()> {
    let source = ffi::text(&provenance.local_source_dir);
    let patterns = |exclude| -> Result<Vec<String>> {
        let (rows, count) = if exclude {
            (&provenance.excludes, provenance.exclude_count)
        } else {
            (&provenance.includes, provenance.include_count)
        };
        if count as usize > rows.len() {
            return Err(refusal("invalid retained selection extent"));
        }
        if count == 0 {
            return native::patterns(exclude).map_err(Into::into);
        }
        Ok(rows[..count as usize]
            .iter()
            .map(|row| ffi::text(row))
            .collect())
    };
    let includes = patterns(false)?;
    let excludes = patterns(true)?;
    let mut expected = Box::new(*provenance);
    expected.found = 0;
    native::configure(Some(root), &mut expected, &includes, &excludes, false)?;
    if ffi::text(&expected.local_source_dir) != source {
        native::configure(Some(root), &mut expected, &includes, &excludes, true)?;
    }
    if ffi::text(&expected.local_source_dir) != source
        || expected.registry_path != provenance.registry_path
        || expected.download_report_path != provenance.download_report_path
        || expected.operation_path != provenance.operation_path
    {
        return Err(refusal(
            "unsafe-source-path: retained storage differs from canonical identity",
        ));
    }
    match fs::symlink_metadata(&source) {
        Ok(value) if !value.is_dir() || value.file_type().is_symlink() => {
            return Err(refusal(
                "unsafe-source-path: source root is not a real directory",
            ));
        }
        Ok(_) => (),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => (),
        Err(error) => return Err(error.into()),
    }
    for path in [
        &source,
        &ffi::text(&provenance.registry_path),
        &ffi::text(&provenance.operation_path),
    ] {
        guarded_parents(Path::new(path), Path::new(root))?;
    }
    Ok(())
}

pub(crate) fn guarded_parents(path: &Path, root: &Path) -> Result<()> {
    if !root.is_absolute()
        || path == root
        || !path.starts_with(root)
        || path
            .components()
            .any(|part| !matches!(part, Component::RootDir | Component::Normal(_)))
    {
        return Err(refusal("unsafe-source-path: exact managed child required"));
    }
    let mut prefix = PathBuf::new();
    for part in path.parent().expect("managed child").components() {
        prefix.push(part.as_os_str());
        match fs::symlink_metadata(&prefix) {
            Ok(value) if !value.is_dir() || value.file_type().is_symlink() => {
                return Err(refusal(
                    "unsafe-source-path: parent is not a real directory",
                ));
            }
            Ok(_) => (),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => break,
            Err(error) => return Err(error.into()),
        }
    }
    Ok(())
}

fn cache_path(provenance: &Provenance, root: &str) -> Option<String> {
    (ffi::text(&provenance.provider) == "huggingface").then(|| {
        let source = ffi::text(&provenance.local_source_dir);
        format!(
            "{root}/cache/hf/{}/{}",
            ffi::text(&provenance.repo_id),
            Path::new(&source)
                .file_name()
                .expect("managed source")
                .to_string_lossy()
        )
    })
}

fn lock_candidates(
    path: &Path,
    depth: usize,
    seen: &mut usize,
    out: &mut Vec<PathBuf>,
) -> Result<()> {
    *seen += 1;
    if depth > 64 || *seen > 1_000_000 {
        return Err(refusal("cleanup observation bound exceeded"));
    }
    let metadata = match fs::symlink_metadata(path) {
        Ok(value) => value,
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => return Ok(()),
        Err(error) => return Err(error.into()),
    };
    if metadata.file_type().is_symlink() {
        return Ok(());
    }
    if metadata.is_dir() {
        for entry in fs::read_dir(path)? {
            lock_candidates(&entry?.path(), depth + 1, seen, out)?;
        }
    } else if metadata.is_file() && path.extension().is_some_and(|value| value == "lock") {
        out.push(path.to_owned());
    }
    Ok(())
}

pub(crate) fn stale_locks(provenance: &Provenance, root: &str, clear: bool) -> Result<usize> {
    guarded(provenance, root)?;
    let source = ffi::text(&provenance.local_source_dir);
    let alias = Path::new(&source).join(".cache");
    let mut paths = Vec::new();
    let mut seen = 0;
    if let Some(cache) = cache_path(provenance, root) {
        guarded_parents(Path::new(&cache), Path::new(root))?;
        lock_candidates(Path::new(&cache), 0, &mut seen, &mut paths)?;
        if fs::symlink_metadata(&alias).is_ok_and(|value| value.file_type().is_symlink())
            && fs::read_link(&alias)? != Path::new(&cache)
        {
            return Err(refusal("unsafe provider cache alias"));
        }
    }
    lock_candidates(&alias, 0, &mut seen, &mut paths)?;
    paths.sort();
    paths.dedup();
    if clear {
        for path in &paths {
            fs::remove_file(path)?;
        }
    }
    Ok(paths.len())
}

fn legacy(
    invocation: &Invocation<'_>,
    provenance: &Provenance,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let source = ffi::text(&provenance.local_source_dir);
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let root = ffi::text(&paths.operator.models_root);
    guarded(provenance, &root)?;
    let active = native::process_count(&source)? != 0;
    if invocation.operation.operation_id.ends_with(".stop") && active {
        return Err(crate::acquire::state_error(
            "legacy provider lacks durable boot/start provenance; stop through its owning terminal",
        )
        .into());
    }
    let scan = native::scan(&source, cache_path(provenance, &root).as_deref())?;
    let report = ffi::text(&provenance.download_report_path);
    let base = report
        .strip_suffix(".download-report.json")
        .ok_or_else(|| refusal("invalid report path"))?;
    let value = json!({"schema": "yvex.model.acquisition.status.v1", "model": ffi::text(&provenance.target_id),
        "provider": ffi::text(&provenance.provider), "repository": ffi::text(&provenance.repo_id),
        "revision": ffi::text(&provenance.revision), "location": source, "active": active,
        "resume_available": !active, "stop_available": false, "files": scan.files,
        "partial_files": scan.partials, "bytes": scan.bytes, "provider_pid": 0, "provider_pgid": 0,
        "receipt": format!("{base}.download.active.json")});
    if invocation.has("--json") || invocation.value("--output") == Some("json") {
        return Ok(Output::standard(format!("{value}\n"), 0));
    }
    let mut text = presentation::record(
        "ACQUISITION  legacy observation",
        &[
            ("target", &ffi::text(&provenance.target_id)),
            ("active", if active { "true" } else { "false" }),
            ("files", &scan.files.to_string()),
            ("bytes", &scan.bytes.to_string()),
            ("stop", "requires authenticated durable operation"),
        ],
        width,
        styled,
    )?;
    if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
        text.push_str(&source_audit(provenance, &root, active, width, styled)?);
    }
    Ok(Output::standard(text, 0))
}

fn source_audit(
    provenance: &Provenance,
    root: &str,
    active: bool,
    width: usize,
    styled: bool,
) -> Result<String> {
    let source = ffi::text(&provenance.local_source_dir);
    let scan = native::inspect(&source, cache_path(provenance, root).as_deref())?;
    let report = ffi::text(&provenance.download_report_path);
    let base = report
        .strip_suffix(".download-report.json")
        .ok_or_else(|| refusal("invalid report path"))?;
    let receipt = format!("{base}.download.active.json");
    let receipt_status = if !Path::new(&receipt).try_exists()? {
        "none"
    } else if active {
        "active"
    } else {
        "stale-active-receipt"
    };
    let extent = if scan.checked_shards == 0 {
        "not-checked"
    } else if scan.truncated_shards != 0 {
        "truncated"
    } else if scan.invalid_shards != 0 {
        "invalid-header"
    } else {
        "ok"
    };
    let value = json!({"status": "model-download-status", "target_id": ffi::text(&provenance.target_id),
        "family": ffi::text(&provenance.family), "repo_id": ffi::text(&provenance.repo_id),
        "local_source_dir": source, "receipt_status": receipt_status,
        "safetensors_count": scan.shards, "gguf_count": scan.gguf_files,
        "source_file_count": scan.files, "total_regular_file_bytes": scan.bytes,
        "safetensors_header_checked": scan.checked_shards != 0, "safetensors_size_status": extent,
        "payload_hash_verified": false});
    crate::acquisition_report::human(&value, width, styled)
}

pub(crate) struct Progress {
    output: Option<replai::OutputSession>,
    enabled: bool,
    width: usize,
    styled: bool,
    last: Instant,
    target: String,
}
fn live_feedback(progress: &str, tty: bool, styled: bool) -> bool {
    tty && styled && ["auto", "live"].contains(&progress)
}
impl Progress {
    pub(crate) fn open(
        request: &crate::acquire::Request,
        width: usize,
        styled: bool,
    ) -> Result<Self> {
        let tty = io::stdin().is_terminal() && io::stderr().is_terminal();
        let enabled = request.progress != "off" && (request.progress != "auto" || tty);
        let terminal_feedback = !replai::Theme::from_environment(styled)
            .sequence(replai::Role::Accent)
            .is_empty();
        let live = enabled && live_feedback(&request.progress, tty, terminal_feedback);
        let output = if live {
            Some(replai::OutputSession::open(&io::stdin(), &io::stderr())?)
        } else {
            None
        };
        Ok(Self {
            output,
            enabled,
            width,
            styled,
            target: ffi::text(&request.provenance.target_id),
            last: Instant::now()
                .checked_sub(Duration::from_secs(3600))
                .unwrap_or_else(Instant::now),
        })
    }

    pub(crate) fn observe(
        &mut self,
        provenance: &Provenance,
        operation: &native::Operation,
        tick: u64,
    ) -> Result<()> {
        if !self.enabled || self.last.elapsed() < Duration::from_secs(tick) {
            return Ok(());
        }
        let value = native::projection(provenance, operation)?;
        let known = |key| {
            value[key]
                .as_u64()
                .map_or("unknown".into(), |value| value.to_string())
        };
        let text = format!(
            "ACQUIRE  {} · {} · {} B committed · {} files · {} partials",
            self.target,
            value["lifecycle"].as_str().unwrap_or("unknown"),
            known("committed_bytes"),
            known("completed_files"),
            known("provider_partial_objects")
        );
        if let Some(output) = &mut self.output {
            output.feedback(presentation::safe_text(&text, replai::Role::Dim)?)?;
        } else {
            io::stderr()
                .lock()
                .write_all(presentation::lines(&[text], self.width, self.styled)?.as_bytes())?;
        }
        self.last = Instant::now();
        Ok(())
    }

    pub(crate) fn close(&mut self) -> Result<()> {
        if let Some(mut output) = self.output.take() {
            output.close(false)?;
        }
        Ok(())
    }
}

pub(crate) fn cleanup(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let provenance = selected(invocation)?;
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let root = ffi::text(&paths.operator.models_root);
    guarded(&provenance, &root)?;
    let dry = invocation.has("--dry-run");
    let execute = invocation.has("--yes") && !dry;
    let _control = if execute {
        Some(ffi::preparation::lock(
            Some(&root),
            &ffi::text(&provenance.operation_path),
        )?)
    } else {
        None
    };
    let operation_path = ffi::text(&provenance.operation_path);
    if Path::new(&operation_path).try_exists()? {
        let operation = native::read(&provenance)?;
        if native::matches(&operation.supervisor)
            || native::matches(&operation.provider_process)
            || (!native::terminal(&operation) && now()?.saturating_sub(operation.created_unix) < 10)
        {
            return Err(crate::acquire::state_error("cleanup refuses active acquisition").into());
        }
    }
    if native::process_count(&ffi::text(&provenance.local_source_dir))? != 0 {
        return Err(crate::acquire::state_error(
            "cleanup refuses a process using the selected source",
        )
        .into());
    }
    let _transfer = if execute {
        Some(native::try_transfer_lock(
            &root,
            &ffi::text(&provenance.repo_id),
            &ffi::text(&provenance.revision),
        )?)
    } else {
        None
    };
    let mut candidates = Vec::<PathBuf>::new();
    let mut sidecars = Vec::<PathBuf>::new();
    let mut logs = Vec::<PathBuf>::new();
    let source = PathBuf::from(ffi::text(&provenance.local_source_dir));
    let mut seen = 0;
    if invocation.has("--stale-locks") {
        if let Some(cache) = cache_path(&provenance, &root) {
            guarded_parents(Path::new(&cache), Path::new(&root))?;
            lock_candidates(Path::new(&cache), 0, &mut seen, &mut candidates)?;
        }
        lock_candidates(
            &Path::new(&ffi::text(&provenance.local_source_dir)).join(".cache"),
            0,
            &mut seen,
            &mut candidates,
        )?;
    }
    if invocation.has("--all-provider-cache") {
        candidates.push(Path::new(&ffi::text(&provenance.local_source_dir)).join(".cache"));
    }
    let report = ffi::text(&provenance.download_report_path);
    let base = report
        .strip_suffix(".download-report.json")
        .ok_or_else(|| refusal("invalid report path"))?;
    if invocation.has("--failed-partials") {
        candidates.push(ffi::text(&provenance.local_source_dir).into());
    }
    if invocation.has("--receipts") || invocation.has("--failed-partials") {
        sidecars.extend(
            [
                ffi::text(&provenance.download_report_path),
                ffi::text(&provenance.manifest_path),
                ffi::text(&provenance.native_inventory_path),
                ffi::text(&provenance.registry_path),
                operation_path,
                ffi::text(&provenance.provider_event_path),
                format!("{base}.download.receipt"),
                format!("{base}.download.active.json"),
                format!("{base}.download.last.json"),
            ]
            .into_iter()
            .map(PathBuf::from),
        );
    }
    if invocation.has("--logs") || invocation.has("--failed-partials") {
        let log = ffi::text(&provenance.supervisor_log_path);
        let base = log
            .strip_suffix(".acquisition.supervisor.log")
            .ok_or_else(|| refusal("invalid log path"))?;
        logs.extend(
            [
                log.clone(),
                format!("{base}.download.stdout.log"),
                format!("{base}.download.stderr.log"),
            ]
            .into_iter()
            .map(PathBuf::from),
        );
    }
    candidates.extend(sidecars.iter().cloned());
    candidates.extend(logs.iter().cloned());
    candidates.sort();
    candidates.dedup();
    for path in &candidates {
        guarded_parents(path, Path::new(&root))?;
    }
    let mut deleted = 0;
    let mut missing = 0;
    let mut deleted_sidecars = 0;
    let mut deleted_logs = 0;
    let mut deleted_source_paths = 0;
    for path in &candidates {
        match fs::symlink_metadata(path) {
            Ok(metadata) if execute => {
                if metadata.is_dir() && !metadata.file_type().is_symlink() {
                    fs::remove_dir_all(path)?;
                } else {
                    fs::remove_file(path)?;
                }
                deleted += 1;
                deleted_sidecars += usize::from(sidecars.contains(path));
                deleted_logs += usize::from(logs.contains(path));
                deleted_source_paths += usize::from(path == &source);
            }
            Ok(_) => (),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => missing += 1,
            Err(error) => return Err(error.into()),
        }
    }
    let value = json!({"schema": "yvex.source.cleanup.v1", "target": ffi::text(&provenance.target_id),
        "status": if dry { "model-download-cleanup-dry-run" } else if execute { "model-download-cleanup" }
            else { "model-download-cleanup-confirmation-required" }, "dry_run": dry, "yes": invocation.has("--yes"),
        "delete_candidates": candidates, "deleted_paths": deleted, "missing": missing});
    let machine = invocation.has("--json") || invocation.value("--output") == Some("json");
    let mut text = if machine {
        format!("{value}\n")
    } else {
        presentation::record(
            "ACQUISITION  cleanup",
            &[
                ("target", &ffi::text(&provenance.target_id)),
                ("status", value["status"].as_str().expect("status")),
                ("candidates", &candidates.len().to_string()),
                ("deleted paths", &deleted.to_string()),
                ("missing", &missing.to_string()),
            ],
            width,
            styled,
        )?
    };
    if !machine && (invocation.has("--audit") || invocation.value("--output") == Some("audit")) {
        text.push_str(&cleanup_audit(
            invocation,
            deleted_sidecars,
            deleted_logs,
            deleted_source_paths,
            width,
            styled,
        )?);
    }
    Ok(Output::standard(text, 0))
}

fn cleanup_audit(
    invocation: &Invocation<'_>,
    deleted_sidecars: usize,
    deleted_logs: usize,
    deleted_source_paths: usize,
    width: usize,
    styled: bool,
) -> Result<String> {
    let flag = |value| if value { "true" } else { "false" };
    Ok(presentation::record(
        "ACQUISITION  cleanup audit",
        &[
            (
                "cleanup_failed_partials",
                flag(invocation.has("--failed-partials")),
            ),
            (
                "cleanup_sidecars",
                flag(invocation.has("--failed-partials") || invocation.has("--receipts")),
            ),
            (
                "cleanup_logs",
                flag(invocation.has("--failed-partials") || invocation.has("--logs")),
            ),
            ("deleted_sidecars", &deleted_sidecars.to_string()),
            ("deleted_logs", &deleted_logs.to_string()),
            ("deleted_source_paths", &deleted_source_paths.to_string()),
        ],
        width,
        styled,
    )?)
}

fn state(provenance: &Provenance) -> Result<native::Operation> {
    let mut operation = native::read(provenance)?;
    if native::matches(&operation.supervisor)
        || native::terminal(&operation)
        || (operation.lifecycle
            == raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STARTING
            && operation.supervisor.present == 0
            && now()?.saturating_sub(operation.created_unix) < 10)
    {
        return Ok(operation);
    }
    let before = (operation.lifecycle, operation.health);
    native::reconcile(&mut operation, now()?)?;
    if before != (operation.lifecycle, operation.health) {
        let current = native::read(provenance)?;
        if !unchanged(&current, &operation) {
            return Ok(current);
        }
        native::publish(provenance, &operation)?;
    }
    Ok(operation)
}

pub(crate) fn signal(identity: &native::Process, group: bool, signal: Signal) -> Result<()> {
    // Recheck the native boot/start identity immediately before signalling.
    // Do not target init, our own process, our own group, or a group whose
    // leader's present group differs from the authenticated durable record.
    if !native::matches(identity) {
        return Ok(());
    }
    let pid = Pid::from_raw(identity.pid).ok_or_else(|| refusal("invalid acquisition process"))?;
    if identity.pid <= 1 || pid == process::getpid() {
        return Err(refusal("unsafe acquisition process identity"));
    }
    if group {
        let pgid = Pid::from_raw(identity.process_group)
            .ok_or_else(|| refusal("invalid acquisition group"))?;
        if identity.process_group <= 1
            || pgid == process::getpgrp()
            || process::getpgid(Some(pid))? != pgid
        {
            return Err(refusal("unsafe or changed acquisition process group"));
        }
        process::kill_process_group(pgid, signal)?;
    } else {
        process::kill_process(pid, signal)?;
    }
    Ok(())
}

fn wait_process(identity: &native::Process, timeout: Duration) -> bool {
    let deadline = Instant::now() + timeout;
    while native::matches(identity) && Instant::now() < deadline {
        std::thread::sleep(Duration::from_millis(50));
    }
    !native::matches(identity)
}

fn stop(provenance: &Provenance, timeout: Duration, force: bool) -> Result<native::Operation> {
    let mut operation = native::read(provenance)?;
    let supervisor_active = native::matches(&operation.supervisor);
    if native::matches(&operation.provider_process) && !supervisor_active {
        let provider = operation.provider_process;
        signal(&provider, true, Signal::TERM)?;
        if !wait_process(&provider, timeout) {
            if !force {
                return Err(refusal(
                    "authenticated orphan provider did not stop within the bounded wait",
                ));
            }
            signal(&provider, true, Signal::KILL)?;
            if !wait_process(&provider, timeout) {
                return Err(refusal(
                    "force-stop did not retire the authenticated provider",
                ));
            }
        }
        let current = native::read(provenance)?;
        if !unchanged(&current, &operation) {
            return Err(refusal("acquisition generation changed during stop"));
        }
        operation.lifecycle =
            raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STOPPED;
        operation.health =
            raw::yvex_source_acquisition_health_YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE;
        operation.updated_unix = now()?;
        operation.provider_process = native::Process::default();
        ffi::put_text(&mut operation.reason, "orphan-provider-stop")?;
        ffi::put_text(&mut operation.result, "interrupted-resumable")?;
        native::publish(provenance, &operation)?;
        return Ok(operation);
    }
    if native::terminal(&operation) {
        return Ok(operation);
    }
    if !supervisor_active {
        return state(provenance);
    }
    signal(&operation.supervisor, false, Signal::TERM)?;
    let deadline = Instant::now() + timeout;
    loop {
        let observed = state(provenance)?;
        if !unchanged(&observed, &operation) {
            return Err(refusal("acquisition generation changed during stop"));
        }
        if native::terminal(&observed) {
            return Ok(observed);
        }
        if Instant::now() >= deadline {
            return Err(refusal(
                "supervisor accepted stop but did not publish terminal state in time",
            ));
        }
        std::thread::sleep(Duration::from_millis(50));
    }
}

pub(crate) fn render(
    invocation: &Invocation<'_>,
    provenance: &Provenance,
    operation: &native::Operation,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let value = native::projection(provenance, operation)?;
    if invocation.has("--json") || invocation.value("--output") == Some("json") {
        return Ok(Output::standard(format!("{value}\n"), 0));
    }
    let audit = invocation.has("--audit") || invocation.value("--output") == Some("audit");
    let fields = value
        .as_object()
        .expect("typed acquisition projection")
        .iter()
        .filter(|(key, _)| {
            audit
                || [
                    "lifecycle",
                    "health",
                    "committed_bytes",
                    "expected_bytes",
                    "completed_files",
                    "selected_files",
                    "reason",
                    "resume_available",
                ]
                .contains(&key.as_str())
        })
        .map(|(key, value)| {
            (
                key.as_str(),
                value
                    .as_str()
                    .map(str::to_owned)
                    .unwrap_or_else(|| value.to_string()),
            )
        })
        .collect::<Vec<_>>();
    let pairs = fields
        .iter()
        .map(|(key, value)| (*key, value.as_str()))
        .collect::<Vec<_>>();
    Ok(Output::standard(
        presentation::record(
            &format!("ACQUISITION  {}", ffi::text(&provenance.target_id)),
            &pairs,
            width,
            styled,
        )?,
        0,
    ))
}

pub(crate) fn control(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let provenance = selected(invocation)?;
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    guarded(&provenance, &ffi::text(&paths.operator.models_root))?;
    if !Path::new(&ffi::text(&provenance.operation_path)).try_exists()? {
        return legacy(invocation, &provenance, width, styled);
    }
    if invocation.has("--dry-run") {
        // A dry control observes only: no lease, reconciliation publication, or signal.
        return render(
            invocation,
            &provenance,
            &native::read(&provenance)?,
            width,
            styled,
        );
    }
    // This is the short-lived control lease, not the transfer lease held by
    // the provider: status/stop must not wait for a download to finish.
    let _control = ffi::preparation::lock(
        invocation.value("--models-root"),
        &ffi::text(&provenance.operation_path),
    )?;
    let operation = if invocation.operation.operation_id.ends_with(".stop") {
        let timeout = invocation
            .value("--timeout-seconds")
            .or(invocation.value("--timeout"))
            .map(str::parse::<u64>)
            .transpose()?
            .unwrap_or(10);
        if !(1..=3600).contains(&timeout) {
            return Err(refusal("stop timeout must be in 1..=3600 seconds"));
        }
        stop(
            &provenance,
            Duration::from_secs(timeout),
            invocation.has("--force"),
        )?
    } else {
        state(&provenance)?
    };
    let mut output = render(invocation, &provenance, &operation, width, styled)?;
    if !invocation.has("--json")
        && invocation.value("--output") != Some("json")
        && (invocation.has("--audit") || invocation.value("--output") == Some("audit"))
    {
        output.text.push_str(&source_audit(
            &provenance,
            &ffi::text(&paths.operator.models_root),
            native::matches(&operation.provider_process),
            width,
            styled,
        )?);
    }
    Ok(output)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn plain_or_redirected_progress_does_not_acquire_cursor_feedback() {
        for mode in ["auto", "live", "plain", "log", "off"] {
            assert!(!live_feedback(mode, true, false));
            assert!(!live_feedback(mode, false, true));
            assert_eq!(
                live_feedback(mode, true, true),
                ["auto", "live"].contains(&mode)
            );
        }
    }

    fn provenance(directory: &std::path::Path) -> Box<Provenance> {
        let mut provenance = Box::<Provenance>::default();
        ffi::put_text(&mut provenance.target_id, "fixture").unwrap();
        ffi::put_text(&mut provenance.family, "fixture").unwrap();
        ffi::put_text(&mut provenance.provider, "huggingface").unwrap();
        ffi::put_text(&mut provenance.repo_id, "fixture/model").unwrap();
        ffi::put_text(&mut provenance.revision, &"1".repeat(40)).unwrap();
        ffi::put_text(
            &mut provenance.local_source_dir,
            directory.to_str().unwrap(),
        )
        .unwrap();
        ffi::put_text(
            &mut provenance.operation_path,
            directory.join("operation.json").to_str().unwrap(),
        )
        .unwrap();
        provenance
    }

    #[test]
    fn unknown_progress_and_stale_identity_remain_fail_closed() {
        let directory =
            std::env::temp_dir().join(format!("yvex-rust-acquisition-{}", std::process::id()));
        std::fs::create_dir(&directory).unwrap();
        let source = provenance(&directory);
        let mut operation =
            native::create(&source, &"2".repeat(64), 1, now().unwrap(), 60).unwrap();
        let self_identity = native::capture(
            std::process::id().try_into().unwrap(),
            process::getpgrp().as_raw_nonzero().get(),
        )
        .unwrap();
        assert!(signal(&self_identity, false, Signal::TERM).is_err());
        let mut stale = self_identity;
        stale.start_ticks += 1;
        assert!(!native::matches(&stale));
        assert!(signal(&stale, true, Signal::TERM).is_ok());
        native::publish(&source, &operation).unwrap();
        let value = native::projection(&source, &operation).unwrap();
        assert!(value["expected_bytes"].is_null() && value["committed_bytes"].is_null());
        assert_eq!(value["bytes"], 0);
        assert_eq!(value["active"], false);
        operation.lifecycle =
            raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STOPPED;
        native::publish(&source, &operation).unwrap();
        assert!(native::terminal(
            &stop(&source, Duration::from_millis(10), false).unwrap()
        ));
        let mut foreign = *source;
        ffi::put_text(&mut foreign.repo_id, "another/model").unwrap();
        assert!(native::read(&foreign).is_err());
        std::fs::remove_file(directory.join("operation.json")).unwrap();
        std::fs::remove_dir(directory).unwrap();
    }
}
