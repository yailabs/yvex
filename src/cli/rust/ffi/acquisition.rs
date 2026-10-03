// Durable acquisition records and process identities remain Source-owned.
use super::{
    Error, Paths, argument, borrowed_text, error, execution::checked, put_text, raw, text,
};
use serde_json::{Value, json};
use std::os::fd::{FromRawFd, OwnedFd};

pub(crate) type Operation = raw::yvex_source_acquisition_operation;
pub(crate) type Process = raw::yvex_source_acquisition_process;

pub(crate) fn reopen(
    record: &raw::yvex_source_acquisition_provenance,
    root: &str,
) -> Result<raw::yvex_source_representation_fact, Error> {
    let path = argument(Some(&text(&record.download_report_path)))?.expect("report");
    let root = argument(Some(root))?.expect("root");
    let repository = argument(Some(&text(&record.repo_id)))?.expect("repository");
    let revision = argument(Some(&text(&record.revision)))?.expect("revision");
    let patterns = |values: &[[std::ffi::c_char; 1024]]| {
        values
            .iter()
            .map(|value| argument(Some(&text(value))).map(Option::unwrap))
            .collect::<Result<Vec<_>, Error>>()
    };
    let includes = patterns(&record.includes[..record.include_count as usize])?;
    let excludes = patterns(&record.excludes[..record.exclude_count as usize])?;
    let includes = includes
        .iter()
        .map(|value| value.as_ptr())
        .collect::<Vec<_>>();
    let excludes = excludes
        .iter()
        .map(|value| value.as_ptr())
        .collect::<Vec<_>>();
    let mut fact = raw::yvex_source_representation_fact::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_reopen(
                path.as_ptr(),
                root.as_ptr(),
                repository.as_ptr(),
                revision.as_ptr(),
                includes.as_ptr(),
                includes.len() as u32,
                excludes.as_ptr(),
                excludes.len() as u32,
                &mut fact,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(fact)
}

pub(crate) fn try_transfer_lock(
    root: &str,
    repository: &str,
    revision: &str,
) -> Result<OwnedFd, Error> {
    let root = argument(Some(root))?.expect("root");
    let repository = argument(Some(repository))?.expect("repository");
    let revision = argument(Some(revision))?.expect("revision");
    let mut descriptor = -1;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_try_lock(
                root.as_ptr(),
                repository.as_ptr(),
                revision.as_ptr(),
                &mut descriptor,
                &mut failure,
            )
        },
        &failure,
    )?;
    if descriptor < 0 {
        return Err(super::extent_error());
    }
    Ok(unsafe { OwnedFd::from_raw_fd(descriptor) })
}

pub(crate) fn observe(
    provenance: &raw::yvex_source_acquisition_provenance,
    cache: Option<&str>,
    operation: &mut Operation,
    now: u64,
) -> Result<(), Error> {
    let source = argument(Some(&text(&provenance.local_source_dir)))?.expect("source");
    let event = argument(Some(&text(&provenance.provider_event_path)))?.expect("event");
    let cache = argument(cache)?;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_observe(
                source.as_ptr(),
                super::pointer(&cache),
                event.as_ptr(),
                operation,
                now,
                &mut failure,
            )
        },
        &failure,
    )
}

pub(crate) fn scan(
    source: &str,
    cache: Option<&str>,
) -> Result<raw::yvex_source_acquisition_tree, Error> {
    tree(source, cache, false)
}

pub(crate) fn inspect(
    source: &str,
    cache: Option<&str>,
) -> Result<raw::yvex_source_acquisition_tree, Error> {
    tree(source, cache, true)
}

fn tree(
    source: &str,
    cache: Option<&str>,
    inspect: bool,
) -> Result<raw::yvex_source_acquisition_tree, Error> {
    let source = argument(Some(source))?.expect("source");
    let cache = argument(cache)?;
    let mut out = raw::yvex_source_acquisition_tree::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            let observe = if inspect {
                raw::yvex_source_acquisition_inspect
            } else {
                raw::yvex_source_acquisition_scan
            };
            observe(
                source.as_ptr(),
                super::pointer(&cache),
                &mut out,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(out)
}

pub(crate) fn process_count(source: &str) -> Result<u64, Error> {
    let source = argument(Some(source))?.expect("source");
    let mut count = 0;
    if unsafe { raw::yvex_platform_process_argument_count(source.as_ptr(), &mut count) } != 0 {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "source.acquisition".into(),
            message: "cannot establish absence of selected-source processes".into(),
        });
    }
    Ok(count)
}

pub(crate) fn timestamp() -> String {
    let mut value = [0; 64];
    unsafe { raw::yvex_core_timestamp_utc(value.as_mut_ptr(), value.len()) };
    text(&value)
}

pub(crate) fn seed(
    root: Option<&str>,
    selector: &str,
) -> Result<Box<raw::yvex_source_acquisition_provenance>, Error> {
    if let Some(record) = super::catalog::acquired_target(root, selector)? {
        return Ok(record);
    }
    let name = argument(Some(selector))?.expect("target selector");
    let row = unsafe { raw::yvex_source_acquisition_target_find(name.as_ptr()).as_ref() }
        .ok_or_else(|| Error {
            code: raw::yvex_status_YVEX_ERR_INVALID_ARG,
            owner: "source.acquire".into(),
            message: "unknown source acquisition target".into(),
        })?;
    let mut record = Box::<raw::yvex_source_acquisition_provenance>::default();
    macro_rules! copy { ($($destination:ident => $source:ident),*) => { $(
        put_text(&mut record.$destination, &unsafe { borrowed_text(row.$source)? })?;
    )* }; }
    copy!(target_id => target_id, family => family_key, provider => provider,
        repo_id => repository, revision => default_reference, local_name => source_dir_leaf);
    Ok(record)
}

pub(crate) fn patterns(exclude: bool) -> Result<Vec<String>, Error> {
    let mut count = 0;
    let values =
        unsafe { raw::yvex_source_acquisition_default_patterns(exclude.into(), &mut count) };
    if count > raw::YVEX_SOURCE_ACQUISITION_PATTERN_CAP as usize || values.is_null() {
        return Err(super::extent_error());
    }
    unsafe { std::slice::from_raw_parts(values, count) }
        .iter()
        .map(|value| unsafe { borrowed_text(*value) })
        .collect()
}

struct Remote(*mut raw::yvex_remote_catalog);
impl Drop for Remote {
    fn drop(&mut self) {
        unsafe { raw::yvex_remote_catalog_close(self.0) };
    }
}

pub(crate) fn resolve_revision(
    repository: &str,
    revision: &str,
    anonymous: bool,
) -> Result<String, Error> {
    let repository = argument(Some(repository))?.expect("repository");
    let revision = argument(Some(revision))?.expect("revision");
    let options = raw::yvex_remote_inspect_options {
        provider: raw::yvex_account_provider_YVEX_ACCOUNT_PROVIDER_HUGGINGFACE,
        repository: repository.as_ptr(),
        revision: revision.as_ptr(),
    };
    let mut catalog = Remote(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_model_remote_inspect_policy(
                &mut catalog.0,
                &options,
                anonymous.into(),
                &mut failure,
            )
        },
        &failure,
    )?;
    let row = unsafe { raw::yvex_remote_catalog_at(catalog.0, 0).as_ref() }
        .ok_or_else(super::extent_error)?;
    Ok(text(&row.resolved_revision))
}

pub(crate) fn validate_identity(
    paths: &raw::yvex_operator_paths,
    record: &raw::yvex_source_acquisition_provenance,
) -> Result<(), Error> {
    let target = argument(Some(&text(&record.target_id)))?.expect("target");
    let family = argument(Some(&text(&record.family)))?.expect("family");
    let mut scratch = Box::new(*record);
    let mut failure = raw::yvex_error::default();
    // Native path formation validates bounded identifiers without opening
    // files or invoking provider tooling. Do not overwrite retained paths.
    let accepted = unsafe {
        raw::yvex_source_acquisition_provenance_paths(
            target.as_ptr(),
            family.as_ptr(),
            paths,
            scratch.as_mut(),
            &mut failure,
        )
    };
    if accepted == 0 {
        return Err(error(failure.code, &failure));
    }
    Ok(())
}

pub(crate) fn configure(
    root: Option<&str>,
    record: &mut raw::yvex_source_acquisition_provenance,
    includes: &[String],
    excludes: &[String],
    selected: bool,
) -> Result<String, Error> {
    let paths = Paths::with_models_root(root)?;
    let repository = argument(Some(&text(&record.repo_id)))?.expect("repository");
    let revision = argument(Some(&text(&record.revision)))?.expect("revision");
    let root_value = argument(Some(&text(&paths.operator.models_root)))?.expect("models root");
    let family = argument(Some(&text(&record.family)))?.expect("family");
    let include_values = includes
        .iter()
        .map(|value| argument(Some(value)).map(Option::unwrap))
        .collect::<Result<Vec<_>, _>>()?;
    let exclude_values = excludes
        .iter()
        .map(|value| argument(Some(value)).map(Option::unwrap))
        .collect::<Result<Vec<_>, _>>()?;
    let include_pointers = include_values
        .iter()
        .map(|value| value.as_ptr())
        .collect::<Vec<_>>();
    let exclude_pointers = exclude_values
        .iter()
        .map(|value| value.as_ptr())
        .collect::<Vec<_>>();
    let mut digest = [0 as std::ffi::c_char; 65];
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_selection_identity(
                include_pointers.as_ptr(),
                includes.len(),
                exclude_pointers.as_ptr(),
                excludes.len(),
                digest.as_mut_ptr(),
                &mut failure,
            )
        },
        &failure,
    )?;
    if includes.len() > record.includes.len() || excludes.len() > record.excludes.len() {
        return Err(super::extent_error());
    }
    for (destination, value) in record.includes.iter_mut().zip(includes) {
        put_text(destination, value)?;
    }
    for (destination, value) in record.excludes.iter_mut().zip(excludes) {
        put_text(destination, value)?;
    }
    record.include_count = includes
        .len()
        .try_into()
        .map_err(|_| super::extent_error())?;
    record.exclude_count = excludes
        .len()
        .try_into()
        .map_err(|_| super::extent_error())?;
    // A retained explicit selection has its own immutable path/stem. Resuming
    // without a replacement selection must not redirect bytes into the default.
    if record.found != 0 && !selected {
        return Ok(text(&digest));
    }
    if text(&record.provider) == "huggingface" {
        let success = unsafe {
            raw::yvex_source_provider_path(
                record.local_source_dir.as_mut_ptr(),
                record.local_source_dir.len(),
                root_value.as_ptr(),
                repository.as_ptr(),
                revision.as_ptr(),
            )
        };
        if success == 0 {
            return Err(super::extent_error());
        }
    } else {
        // Release coordinates are path components, not a claim of immutable provider bytes.
        let repo = text(&record.repo_id);
        let rev = text(&record.revision);
        if repo.split('/').count() != 2
            || repo
                .split('/')
                .any(|value| value.is_empty() || value == "." || value == "..")
            || rev.contains(['/', '\\'])
            || ["", ".", ".."].contains(&rev.as_str())
        {
            return Err(super::extent_error());
        }
        put_text(
            &mut record.local_source_dir,
            &format!(
                "{}/source/github/{repo}/{rev}",
                text(&paths.operator.models_root)
            ),
        )?;
    }
    let stem = if selected {
        let stem = format!(
            "{}-{}-{}",
            text(&record.target_id),
            text(&record.revision),
            text(&digest)
        );
        let selected_path = format!("{}-{}", text(&record.local_source_dir), text(&digest));
        put_text(&mut record.local_source_dir, &selected_path)?;
        stem
    } else {
        text(&record.target_id)
    };
    let stem = argument(Some(&stem))?.expect("selected target");
    if unsafe {
        raw::yvex_source_acquisition_provenance_paths(
            stem.as_ptr(),
            family.as_ptr(),
            &paths.operator,
            record,
            &mut failure,
        )
    } == 0
    {
        return Err(error(failure.code, &failure));
    }
    Ok(text(&digest))
}

pub(crate) fn verify(
    record: &raw::yvex_source_acquisition_provenance,
    root: Option<&str>,
) -> Result<raw::yvex_source_representation_fact, Error> {
    let paths = Paths::with_models_root(root)?;
    let root = argument(Some(&text(&paths.operator.models_root)))?.expect("root");
    let locator = super::catalog::locator(&text(&record.local_source_dir))?;
    let expected = argument(
        (record.source_payload_digest[0] != 0)
            .then(|| text(&record.source_payload_digest))
            .as_deref(),
    )?;
    let mut result = raw::yvex_source_representation_fact::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_representation_verify_local(
                &locator,
                root.as_ptr(),
                super::pointer(&expected),
                &mut result,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(result)
}

pub(crate) fn read(
    provenance: &raw::yvex_source_acquisition_provenance,
) -> Result<Operation, Error> {
    let path = argument(Some(&text(&provenance.operation_path)))?.expect("operation path");
    let mut operation = Operation::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_operation_read(path.as_ptr(), &mut operation, &mut failure)
        },
        &failure,
    )?;
    validate(&operation, provenance)?;
    Ok(operation)
}

pub(crate) fn validate(
    operation: &Operation,
    provenance: &raw::yvex_source_acquisition_provenance,
) -> Result<(), Error> {
    if text(&operation.provider) != text(&provenance.provider)
        || text(&operation.repository) != text(&provenance.repo_id)
        || text(&operation.revision) != text(&provenance.revision)
        || text(&operation.source_path) != text(&provenance.local_source_dir)
    {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "source.acquisition".into(),
            message: "durable operation target identity mismatch".into(),
        });
    }
    Ok(())
}

pub(crate) fn publish(
    provenance: &raw::yvex_source_acquisition_provenance,
    operation: &Operation,
) -> Result<(), Error> {
    validate(operation, provenance)?;
    let path = argument(Some(&text(&provenance.operation_path)))?.expect("operation path");
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_operation_publish(path.as_ptr(), operation, &mut failure)
        },
        &failure,
    )
}

pub(crate) fn matches(process: &Process) -> bool {
    unsafe { raw::yvex_source_acquisition_process_matches(process) != 0 }
}

pub(crate) fn terminal(operation: &Operation) -> bool {
    unsafe { raw::yvex_source_acquisition_terminal(operation.lifecycle) != 0 }
}

pub(crate) fn reconcile(operation: &mut Operation, now: u64) -> Result<(), Error> {
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_reconcile(
                operation,
                now,
                operation.stall_window_seconds,
                &mut failure,
            )
        },
        &failure,
    )
}

pub(crate) fn projection(
    provenance: &raw::yvex_source_acquisition_provenance,
    operation: &Operation,
) -> Result<Value, Error> {
    validate(operation, provenance)?;
    let active = matches(&operation.supervisor) || matches(&operation.provider_process);
    let lifecycle = unsafe {
        borrowed_text(raw::yvex_source_acquisition_lifecycle_name(
            operation.lifecycle,
        ))?
    };
    let health =
        unsafe { borrowed_text(raw::yvex_source_acquisition_health_name(operation.health))? };
    let mut result = json!({"schema": "yvex.model.acquisition.status.v2",
        "operation_id": text(&operation.operation_id), "generation": operation.generation,
        "stall_window_seconds": operation.stall_window_seconds,
        "provider": text(&operation.provider), "repository": text(&operation.repository),
        "target_id": text(&provenance.target_id), "family": text(&provenance.family),
        "revision": text(&operation.revision), "selection_identity": text(&operation.selection_identity),
        "lifecycle": lifecycle, "health": health,
        "current_object": optional(&operation.progress.current_object),
        "supervisor_pid": operation.supervisor.pid, "provider_pid": operation.provider_process.pid,
        "active": active, "stop_available": active,
        "resume_available": matches!(operation.lifecycle,
            raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STOPPED |
            raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_FAILED),
        "bytes": if operation.progress.committed_bytes.known != 0 {
            operation.progress.committed_bytes.value } else { 0 },
        "partial_files": if operation.progress.provider_partial_objects.known != 0 {
            operation.progress.provider_partial_objects.value } else { 0 },
        "reason": text(&operation.reason), "result": optional(&operation.result),
        "legacy": {"active_semantics": "process-liveness", "bytes_semantics": "committed-canonical-bytes",
            "partial_files_semantics": "provider-internal-objects"}});
    macro_rules! fact {
        ($($name:ident),*) => { $(result[stringify!($name)] = if operation.progress.$name.known != 0 {
            json!(operation.progress.$name.value)
        } else { Value::Null };)* };
    }
    fact!(
        expected_bytes,
        committed_bytes,
        provider_activity_bytes,
        inflight_selected_bytes,
        selected_files,
        completed_files,
        incomplete_files,
        selected_shards,
        completed_shards,
        selected_sidecars,
        completed_sidecars,
        provider_partial_objects,
        provider_lock_objects,
        provider_activity_current_bytes_per_second,
        provider_activity_rolling_bytes_per_second,
        current_rate_bytes_per_second,
        rolling_rate_bytes_per_second,
        last_progress_unix,
        last_provider_event_unix,
        provider_event_sequence,
        retry_count
    );
    Ok(result)
}

fn optional(value: &[std::ffi::c_char]) -> Value {
    if value[0] == 0 {
        Value::Null
    } else {
        json!(text(value))
    }
}

pub(crate) fn capture(pid: i32, group: i32) -> Result<Process, Error> {
    let mut process = Process::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_source_acquisition_process_capture(pid, group, &mut process, &mut failure)
    };
    if status == 0 {
        Ok(process)
    } else {
        Err(error(status, &failure))
    }
}

pub(crate) fn create(
    provenance: &raw::yvex_source_acquisition_provenance,
    selection: &str,
    generation: u64,
    now: u64,
    stall_window: u64,
) -> Result<Operation, Error> {
    let provider = argument(Some(&text(&provenance.provider)))?.expect("provider");
    let repository = argument(Some(&text(&provenance.repo_id)))?.expect("repository");
    let revision = argument(Some(&text(&provenance.revision)))?.expect("revision");
    let path = argument(Some(&text(&provenance.local_source_dir)))?.expect("source");
    let selection = argument(Some(selection))?.expect("selection");
    let options = raw::yvex_source_acquisition_create_options {
        provider: provider.as_ptr(),
        repository: repository.as_ptr(),
        revision: revision.as_ptr(),
        source_path: path.as_ptr(),
        selection_identity: selection.as_ptr(),
        generation,
        now_unix: now,
        stall_window_seconds: stall_window,
        ..Default::default()
    };
    let mut operation = Operation::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_source_acquisition_operation_create(&options, &mut operation, &mut failure)
        },
        &failure,
    )?;
    Ok(operation)
}
