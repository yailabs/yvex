// Acquire/resume policy belongs to the shell; source identity stays native.
use crate::{
    Output,
    acquisition::{self, Provenance, Result},
    acquisition_report, acquisition_worker,
    ffi::{self, acquisition as native, raw},
    presentation,
    registry::Invocation,
};
use std::{
    cell::Cell,
    fs,
    os::unix::process::CommandExt,
    process::{Command, Stdio},
    time::Duration,
};

#[derive(Clone, Copy, Default)]
pub(crate) struct ProviderOutcome {
    pub exit: Option<i32>,
    pub interrupt: Option<i32>,
    pub forced: bool,
    pub cleared_locks: usize,
    pub ticks: u64,
}

pub(crate) struct Request {
    pub provenance: Box<Provenance>,
    pub selection: String,
    pub root: String,
    pub observation: raw::yvex_account_observation,
    pub account: ffi::Account,
    pub auth: String,
    pub workers: u64,
    pub timeout: u64,
    pub tick: u64,
    pub stall: u64,
    pub asset: Option<String>,
    pub resume: bool,
    pub dry: bool,
    pub no_manifest: bool,
    pub no_inventory: bool,
    pub outcome: Cell<ProviderOutcome>,
    pub progress: String,
    pub yes: bool,
    pub force_sidecars: bool,
    pub root_source: String,
    pub clear_stale_locks: bool,
}

pub(crate) fn invalid(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
        owner: "source.acquire".into(),
        message: message.into(),
    }
}
pub(crate) fn state_error(message: &str) -> ffi::Error {
    ffi::Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "source.acquisition".into(),
        message: message.into(),
    }
}
fn number(invocation: &Invocation<'_>, flag: &str, default: u64) -> Result<u64> {
    let value = invocation
        .value(flag)
        .map(str::parse::<u64>)
        .transpose()
        .map_err(|_| invalid(&format!("{flag} requires a positive integer")))?
        .unwrap_or(default);
    if value == 0 {
        return Err(invalid(&format!("{flag} requires a positive integer")).into());
    }
    Ok(value)
}

pub(crate) fn identity(invocation: &Invocation<'_>, control: bool) -> Result<Box<Provenance>> {
    let root = invocation.value("--models-root");
    let mut record = if let Some(repository) = invocation.value("--repo") {
        if !invocation.positionals.is_empty() {
            return Err(invalid("select either TARGET or --repo, not both").into());
        }
        let name = invocation
            .value("--name")
            .or(invocation.value("--asset-name"))
            .unwrap_or(repository.rsplit('/').next().unwrap_or(""));
        let provider = invocation.value("--provider").unwrap_or("huggingface");
        // An explicit repository/revision/selection identifies a new acquisition,
        // not a request to resolve a potentially ambiguous retained short name.
        // Reconstruct it identically in the supervisor; configure/start still
        // fence the exact durable path, operation identity and generation.
        let explicit_selection = !control
            && invocation.has("--family")
            && invocation.has("--revision")
            && (invocation.has("--include") || invocation.has("--exclude"));
        let mut record = if explicit_selection {
            Box::<Provenance>::default()
        } else {
            ffi::catalog::acquired_target(root, name)?.unwrap_or_default()
        };
        let retained_family = ffi::text(&record.family);
        let family = if matches!(provider, "gh" | "github") {
            "github"
        } else {
            invocation
                .value("--family")
                .or_else(|| (record.found != 0).then_some(retained_family.as_str()))
                .ok_or_else(|| invalid("--repo requires --family without retained provenance"))?
        };
        if record.found != 0
            && (ffi::text(&record.repo_id) != repository || ffi::text(&record.family) != family)
        {
            return Err(state_error("retained target belongs to another repository/family").into());
        }
        ffi::put_text(&mut record.target_id, name)?;
        ffi::put_text(&mut record.local_name, name)?;
        ffi::put_text(&mut record.family, family)?;
        ffi::put_text(&mut record.repo_id, repository)?;
        ffi::put_text(
            &mut record.provider,
            if matches!(provider, "gh" | "github") {
                "github"
            } else if matches!(provider, "hf" | "huggingface") {
                "huggingface"
            } else {
                return Err(invalid("provider requires hf|huggingface|gh|github").into());
            },
        )?;
        if record.found == 0 {
            ffi::put_text(
                &mut record.revision,
                invocation
                    .value("--revision")
                    .or(invocation.value("--release"))
                    .unwrap_or(if matches!(provider, "gh" | "github") {
                        "latest"
                    } else {
                        "main"
                    }),
            )?;
        }
        record
    } else {
        native::seed(
            root,
            invocation
                .positionals
                .first()
                .ok_or_else(|| invalid("acquire requires TARGET or --repo OWNER/NAME"))?,
        )?
    };
    if let Some(revision) = invocation
        .value("--revision")
        .or(invocation.value("--release"))
    {
        if revision != ffi::text(&record.revision) {
            record.found = 0;
            record.source_payload_digest.fill(0);
        }
        ffi::put_text(&mut record.revision, revision)?;
    }
    if matches!(ffi::text(&record.provider).as_str(), "hf" | "huggingface") {
        ffi::put_text(&mut record.provider, "huggingface")?;
    }
    if !control && ffi::text(&record.provider) == "github" && invocation.value("--asset").is_none()
    {
        return Err(invalid("GitHub acquisition requires --asset GLOB").into());
    }
    Ok(record)
}

fn selection(
    invocation: &Invocation<'_>,
    record: &Provenance,
    exclude: bool,
) -> Result<Vec<String>> {
    let flag = if exclude { "--exclude" } else { "--include" };
    if let Some(values) = invocation.flags.get(flag) {
        if values
            .iter()
            .any(|value| value.is_empty() || value.len() >= 1024)
        {
            return Err(
                invalid("acquisition patterns must be nonempty and fit the source record").into(),
            );
        }
        return Ok(values.clone());
    }
    if record.found != 0 && !invocation.has("--include") && !invocation.has("--exclude") {
        let (rows, count) = if exclude {
            (&record.excludes, record.exclude_count)
        } else {
            (&record.includes, record.include_count)
        };
        if count as usize > rows.len() {
            return Err(state_error("retained selection exceeds bounds").into());
        }
        if count == 0 {
            return native::patterns(exclude).map_err(Into::into);
        }
        return Ok(rows[..count as usize]
            .iter()
            .map(|row| ffi::text(row))
            .collect());
    }
    native::patterns(exclude).map_err(Into::into)
}

fn request(invocation: &Invocation<'_>) -> Result<Request> {
    let auth = invocation.value("--auth").unwrap_or("auto");
    if !["auto", "required", "never"].contains(&auth) {
        return Err(invalid("invalid acquisition auth policy").into());
    }
    if invocation
        .value("--source")
        .is_some_and(|source| source != "hf")
        || invocation
            .value("--github-source")
            .is_some_and(|source| source != "release-asset")
    {
        return Err(invalid("unadmitted source transport").into());
    }
    if invocation
        .value("--progress")
        .is_some_and(|value| !["auto", "live", "plain", "log", "off"].contains(&value))
        || invocation
            .value("--output")
            .is_some_and(|value| !["normal", "table", "audit", "json"].contains(&value))
    {
        return Err(invalid("invalid presentation mode").into());
    }
    let paths = ffi::Paths::with_models_root(invocation.value("--models-root"))?;
    let mut provenance = identity(invocation, false)?;
    let includes = selection(invocation, &provenance, false)?;
    let excludes = selection(invocation, &provenance, true)?;
    // Native target/family admission must precede provider interaction. Final
    // source paths are formed only after resolving the immutable revision.
    native::validate_identity(&paths.operator, &provenance)?;
    let workers = number(invocation, "--max-workers", 8)?.min(64);
    let timeout = number(invocation, "--timeout-seconds", 10)?;
    let tick = number(invocation, "--tick-seconds", 2)?;
    let stall = number(invocation, "--stall-seconds", 120)?;
    let account = ffi::Account::open(
        &ffi::text(&provenance.provider),
        invocation.value("--cli"),
        invocation.value("--token-env"),
    )?;
    let observation = if auth == "never" {
        account.observe()?
    } else {
        account.ensure(
            raw::yvex_account_interactive_mode_YVEX_ACCOUNT_INTERACTIVE_AUTO,
            auth == "required",
        )?
    };
    if observation.cli_present == 0
        || (auth != "never"
            && !["logged-in", "env-token-present"]
                .contains(&ffi::text(&observation.auth_state).as_str()))
    {
        return Err(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
            owner: "source.acquire.account".into(),
            message: format!(
                "{}: {}",
                ffi::text(&observation.top_blocker),
                ffi::text(&observation.next)
            ),
        }
        .into());
    }
    if ffi::text(&provenance.provider) == "huggingface" {
        let current = ffi::text(&provenance.revision);
        // A resumed authenticated immutable record does not need branch resolution.
        let revision = if provenance.found != 0 {
            current
        } else {
            native::resolve_revision(&ffi::text(&provenance.repo_id), &current, auth == "never")?
        };
        ffi::put_text(&mut provenance.revision, &revision)?;
    }
    let selection = native::configure(
        Some(&ffi::text(&paths.operator.models_root)),
        &mut provenance,
        &includes,
        &excludes,
        invocation.has("--include") || invocation.has("--exclude"),
    )?;
    Ok(Request {
        provenance,
        selection,
        root: ffi::text(&paths.operator.models_root),
        observation,
        account,
        auth: auth.into(),
        workers,
        timeout,
        tick,
        stall,
        asset: invocation.value("--asset").map(str::to_owned),
        resume: invocation.operation.operation_id == "source.resume",
        dry: invocation.has("--dry-run"),
        no_manifest: invocation.has("--no-manifest"),
        no_inventory: invocation.has("--no-native-inventory"),
        outcome: Cell::default(),
        progress: if invocation.has("--no-progress") {
            "off"
        } else {
            invocation.value("--progress").unwrap_or("auto")
        }
        .into(),
        yes: invocation.has("--yes"),
        force_sidecars: invocation.has("--force-sidecars"),
        root_source: ffi::text(&paths.operator.models_root_source),
        clear_stale_locks: invocation.has("--clear-stale-locks"),
    })
}

pub(crate) fn provider(request: &Request) -> Result<Command> {
    let record = &request.provenance;
    let mut command = Command::new(ffi::text(&request.observation.cli_path));
    if ffi::text(&record.provider) == "huggingface" {
        command.args([
            "download",
            &ffi::text(&record.repo_id),
            "--revision",
            &ffi::text(&record.revision),
        ]);
        if !request.dry {
            command.args(["--local-dir", &ffi::text(&record.local_source_dir)]);
        }
        for value in &record.includes[..record.include_count as usize] {
            command.args(["--include", &ffi::text(value)]);
        }
        for value in &record.excludes[..record.exclude_count as usize] {
            command.args(["--exclude", &ffi::text(value)]);
        }
        command.args(["--max-workers", &request.workers.to_string()]);
        if request.dry {
            command.arg("--dry-run");
        }
        for (variable, suffix) in [("HF_HUB_CACHE", "hub"), ("HF_XET_CACHE", "xet")] {
            let value = std::env::var_os(variable)
                .filter(|value| !value.is_empty())
                .unwrap_or_else(|| {
                    format!("{}/cache/provider/huggingface/{suffix}", request.root).into()
                });
            command.env(variable, value);
        }
    } else {
        command.args([
            "release",
            "download",
            &ffi::text(&record.revision),
            "--repo",
            &ffi::text(&record.repo_id),
            "--pattern",
            request
                .asset
                .as_deref()
                .ok_or_else(|| invalid("missing GitHub asset"))?,
            "--dir",
            &ffi::text(&record.local_source_dir),
            "--skip-existing",
        ]);
    }
    if request.auth == "never" {
        for variable in [
            "HF_TOKEN",
            "HUGGING_FACE_HUB_TOKEN",
            "GH_TOKEN",
            "GITHUB_TOKEN",
        ] {
            command.env_remove(variable);
        }
        command.env("HF_HUB_DISABLE_IMPLICIT_TOKEN", "1");
    } else if let Some(token) =
        std::env::var_os(request.account.token_environment()).filter(|value| !value.is_empty())
    {
        command.env(
            if ffi::text(&record.provider) == "huggingface" {
                "HF_TOKEN"
            } else {
                "GH_TOKEN"
            },
            token,
        );
    }
    command.env(
        "YVEX_PROVIDER_EVENT_PATH",
        ffi::text(&record.provider_event_path),
    );
    command.stdin(Stdio::null()).process_group(0);
    Ok(command)
}

pub(crate) fn parent(path: &str) -> Result<()> {
    fs::create_dir_all(
        std::path::Path::new(path)
            .parent()
            .ok_or_else(|| invalid("path has no parent"))?,
    )?;
    Ok(())
}

pub(crate) struct StartOptions {
    pub worker_words: Vec<String>,
    pub expected_bytes: Option<u64>,
    pub selected_shards: Option<u64>,
    pub expected_generation: Option<(String, u64)>,
}

pub(crate) fn start_request(request: &Request, options: StartOptions) -> Result<native::Operation> {
    let record = &request.provenance;
    parent(&ffi::text(&record.operation_path))?;
    parent(&ffi::text(&record.supervisor_log_path))?;
    let _lease = ffi::preparation::lock(Some(&request.root), &ffi::text(&record.operation_path))?;
    let previous = if std::path::Path::new(&ffi::text(&record.operation_path)).try_exists()? {
        Some(native::read(record)?)
    } else {
        None
    };
    if let Some((identity, generation)) = &options.expected_generation
        && !previous.as_ref().is_some_and(|previous| {
            ffi::text(&previous.operation_id) == *identity && previous.generation == *generation
        })
    {
        return Err(state_error("stale_acquisition_generation").into());
    }
    if let Some(previous) = &previous {
        if ffi::text(&previous.selection_identity) != request.selection {
            return Err(state_error("existing operation has another source selection").into());
        }
        if native::matches(&previous.supervisor) && !native::terminal(previous) {
            return Ok(*previous);
        }
        if native::matches(&previous.provider_process) {
            return Err(state_error(
                "authenticated orphan provider remains active; stop it before resume",
            )
            .into());
        }
    }
    acquisition::guarded(record, &request.root)?;
    let locks = acquisition::stale_locks(record, &request.root, false)?;
    if locks != 0 && !request.clear_stale_locks {
        return Err(state_error(
            "stale-lock-candidates: inspect or explicitly clear before resume",
        )
        .into());
    }
    {
        let _transfer = native::try_transfer_lock(
            &request.root,
            &ffi::text(&record.repo_id),
            &ffi::text(&record.revision),
        )?;
        if native::process_count(&ffi::text(&record.local_source_dir))? != 0 {
            return Err(state_error("another process uses the selected source").into());
        }
        if let Some(previous) = &previous
            && previous.lifecycle
                == raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_COMPLETE
            && std::path::Path::new(&ffi::text(&record.local_source_dir)).try_exists()?
        {
            // Resume preserves verified completed bytes and operation identity,
            // but not at the expense of active-owner or stale-lock safeguards.
            acquisition_report::read(request)?;
            if request.clear_stale_locks {
                let cleared = acquisition::stale_locks(record, &request.root, true)?;
                let mut outcome = request.outcome.get();
                outcome.cleared_locks = cleared;
                request.outcome.set(outcome);
            }
            return Ok(*previous);
        }
    }
    let generation = previous
        .as_ref()
        .map_or(Some(1), |previous| previous.generation.checked_add(1))
        .ok_or_else(|| state_error("acquisition generation exhausted"))?;
    let mut operation = native::create(
        record,
        &request.selection,
        generation,
        acquisition::now()?,
        request.stall,
    )?;
    for (value, fact) in [
        (
            options.expected_bytes,
            &mut operation.progress.expected_bytes,
        ),
        (
            options.selected_shards,
            &mut operation.progress.selected_shards,
        ),
    ] {
        if let Some(value) = value {
            *fact = raw::yvex_source_acquisition_u64 { value, known: 1 };
        }
    }
    native::publish(record, &operation)?;
    acquisition_report::retain(request, None, "model-download-running", None)?;
    let log = acquisition_worker::private_log(&ffi::text(&record.supervisor_log_path))?;
    let words = options.worker_words;
    // Spawn the loaded product image, not a pathname Cargo/install may replace.
    // On Linux the child inherits this executable identity across fork/exec.
    let child = Command::new(worker_program()?)
        .args(words)
        .env("YVEX_SOURCE_ACQUISITION_WORKER", "1")
        .env(
            "YVEX_SOURCE_ACQUISITION_OPERATION",
            ffi::text(&record.operation_path),
        )
        .env(
            "YVEX_SOURCE_ACQUISITION_ID",
            ffi::text(&operation.operation_id),
        )
        .stdin(Stdio::null())
        .stdout(log.try_clone()?)
        .stderr(log)
        .process_group(0)
        .spawn();
    let mut child = match child {
        Ok(child) => child,
        Err(error) => {
            operation.lifecycle =
                raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_FAILED;
            ffi::put_text(&mut operation.reason, "supervisor-start-failed")?;
            native::publish(record, &operation)?;
            return Err(error.into());
        }
    };
    // Detached from terminal lifetime, but still reaped by its creator when present.
    std::thread::spawn(move || {
        let _ = child.wait();
    });
    Ok(operation)
}

fn start(invocation: &Invocation<'_>, request: &Request) -> Result<native::Operation> {
    let record = &request.provenance;
    let mut words = invocation.operation.command_path.clone();
    words.extend(invocation.positionals.clone());
    for (flag, value) in &invocation.ordered_flags {
        if ["--revision", "--release", "--models-root"].contains(&flag.as_str()) {
            continue;
        }
        words.push(flag.clone());
        if invocation
            .operation
            .flags
            .iter()
            .find(|entry| entry.name == *flag)
            .is_some_and(|entry| entry.takes_value)
        {
            words.push(value.clone());
        }
    }
    words.extend([
        "--models-root".into(),
        request.root.clone(),
        if ffi::text(&record.provider) == "github" {
            "--release"
        } else {
            "--revision"
        }
        .into(),
        ffi::text(&record.revision),
    ]);
    start_request(
        request,
        StartOptions {
            worker_words: words,
            expected_generation: None,
            expected_bytes: invocation
                .has("--expected-bytes")
                .then(|| number(invocation, "--expected-bytes", 1))
                .transpose()?,
            selected_shards: invocation
                .has("--selected-shards")
                .then(|| number(invocation, "--selected-shards", 1))
                .transpose()?,
        },
    )
}

fn worker_program() -> Result<std::path::PathBuf> {
    #[cfg(target_os = "linux")]
    {
        Ok("/proc/self/exe".into())
    }
    #[cfg(not(target_os = "linux"))]
    {
        Ok(std::env::current_exe()?)
    }
}

fn attach(
    invocation: &Invocation<'_>,
    request: &Request,
    expected: &native::Operation,
    width: usize,
    styled: bool,
) -> Result<Output> {
    let mut signals = signal_hook::iterator::Signals::new([
        signal_hook::consts::SIGINT,
        signal_hook::consts::SIGTERM,
    ])?;
    let mut progress = acquisition::Progress::open(request, width, styled)?;
    loop {
        let mut observed = native::read(&request.provenance)?;
        if !acquisition::unchanged(&observed, expected) {
            return Err(state_error("acquisition generation changed while observing").into());
        }
        if let Some(signal) = signals.pending().next() {
            // This process only observes the independently supervised transfer.
            // Explicit source/model stop owns authenticated producer cancellation.
            progress.close()?;
            return Ok(Output {
                text: crate::presentation::record(
                    "ACQUISITION observation detached",
                    &[
                        ("target", &ffi::text(&request.provenance.target_id)),
                        ("state", "supervised acquisition continues independently"),
                        (
                            "next",
                            "use source status to observe or source stop to cancel",
                        ),
                    ],
                    width,
                    styled,
                )?,
                exit: (128 + signal) as u8,
                diagnostic: true,
            });
        }
        if native::terminal(&observed) {
            progress.close()?;
            let text = if invocation.has("--json") || invocation.value("--output") == Some("json") {
                let result = acquisition_report::read(request)?;
                let result =
                    acquisition_report::command(request, &result, &ffi::text(&observed.reason));
                format!("{result}\n")
            } else if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
                let result = acquisition_report::read(request)?;
                acquisition_report::human(&result, width, styled)?
            } else {
                acquisition::render(invocation, &request.provenance, &observed, width, styled)?.text
            };
            return Ok(Output::standard(
                text,
                if observed.lifecycle
                    == raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_COMPLETE
                {
                    0
                } else {
                    1
                },
            ));
        }
        progress.observe(&request.provenance, &observed, request.tick)?;
        if !native::matches(&observed.supervisor)
            && acquisition::now()?.saturating_sub(observed.created_unix) >= 10
        {
            native::reconcile(&mut observed, acquisition::now()?)?;
            native::publish(&request.provenance, &observed)?;
            if native::terminal(&observed) {
                return Err(state_error("supervisor disappeared during acquisition").into());
            }
        }
        std::thread::sleep(Duration::from_millis(100));
    }
}

pub(crate) fn dispatch(invocation: &Invocation<'_>, width: usize, styled: bool) -> Result<Output> {
    let request = match request(invocation) {
        Ok(request) => request,
        Err(error)
            if (invocation.has("--json") || invocation.value("--output") == Some("json"))
                && error
                    .downcast_ref::<ffi::Error>()
                    .is_some_and(|e| e.code != raw::yvex_status_YVEX_ERR_INVALID_ARG) =>
        {
            let native = error
                .downcast_ref::<ffi::Error>()
                .expect("typed native error");
            let value = serde_json::json!({"schema": "yvex.model.pull.v1", "status": "model-download-fail",
                "model": invocation.value("--name").or_else(|| invocation.positionals.first().map(String::as_str)),
                "repository": invocation.value("--repo"), "revision": invocation.value("--revision"),
                "reason": error.to_string(), "error": ffi::status_name(native.code),
                "state_changed": false, "upstream_identity_verified": false, "payload_hash_verified": false});
            return Ok(Output::standard(
                format!("{value}\n"),
                crate::native_exit(native.code),
            ));
        }
        Err(error) => return Err(error),
    };
    if std::env::var_os("YVEX_SOURCE_ACQUISITION_WORKER").is_some() {
        return acquisition_worker::run(&request).map(|()| Output::standard(String::new(), 0));
    }
    if request.dry {
        let exit = if ffi::text(&request.provenance.provider) == "huggingface" {
            provider(&request)?
                .stdout(Stdio::null())
                .stderr(Stdio::null())
                .status()?
                .code()
                .unwrap_or(1)
        } else {
            0
        };
        let mut outcome = request.outcome.get();
        outcome.exit = Some(exit);
        request.outcome.set(outcome);
        let data = acquisition_report::projection(&request, None, "model-download-dry-run", None)?;
        let text = if invocation.has("--json") || invocation.value("--output") == Some("json") {
            format!("{data}\n")
        } else if invocation.has("--audit") || invocation.value("--output") == Some("audit") {
            acquisition_report::human(&data, width, styled)?
        } else {
            presentation::record(
                "SOURCE  planned",
                &[
                    ("target", &ffi::text(&request.provenance.target_id)),
                    ("revision", &ffi::text(&request.provenance.revision)),
                    ("status", "model-download-dry-run"),
                ],
                width,
                styled,
            )?
        };
        return Ok(Output::standard(text, if exit == 0 { 0 } else { 1 }));
    }
    let operation = start(invocation, &request)?;
    attach(invocation, &request, &operation, width, styled)
}
