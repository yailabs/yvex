// Detached provider lifetime: authenticated process control, durable state and reaping.
use crate::{
    acquire::{self, Request},
    acquisition::{self, Result},
    acquisition_report,
    ffi::{self, acquisition as native, raw},
};
use rustix::process::{self, Signal};
use std::{
    fs::{self, File, OpenOptions},
    os::unix::{
        fs::{OpenOptionsExt, symlink},
        process::ExitStatusExt,
    },
    time::{Duration, Instant},
};

pub(crate) fn private_log(path: &str) -> Result<File> {
    acquire::parent(path)?;
    Ok(OpenOptions::new()
        .create(true)
        .append(true)
        .mode(0o600)
        .custom_flags(rustix::fs::OFlags::NOFOLLOW.bits() as i32)
        .open(path)?)
}

fn cache(request: &Request) -> Result<Option<String>> {
    let cache = acquisition_report::acquisition_cache(request);
    let Some(path) = &cache else {
        return Ok(None);
    };
    acquisition::guarded(&request.provenance, &request.root)?;
    acquisition::guarded_parents(
        std::path::Path::new(path),
        std::path::Path::new(&request.root),
    )?;
    if fs::symlink_metadata(path)
        .is_ok_and(|value| !value.is_dir() || value.file_type().is_symlink())
    {
        return Err(acquire::state_error("provider cache is not a real directory").into());
    }
    fs::create_dir_all(path)?;
    fs::create_dir_all(ffi::text(&request.provenance.local_source_dir))?;
    let link =
        std::path::Path::new(&ffi::text(&request.provenance.local_source_dir)).join(".cache");
    match fs::symlink_metadata(&link) {
        Ok(metadata)
            if metadata.file_type().is_symlink()
                && fs::read_link(&link)? == std::path::Path::new(path) => {}
        Ok(_) => {
            return Err(acquire::state_error(
                "existing provider cache requires explicit storage migration",
            )
            .into());
        }
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => symlink(path, link)?,
        Err(error) => return Err(error.into()),
    }
    Ok(cache)
}

fn current(request: &Request, operation: &native::Operation) -> Result<()> {
    if !acquisition::unchanged(&native::read(&request.provenance)?, operation) {
        return Err(acquire::state_error("worker lost acquisition generation ownership").into());
    }
    Ok(())
}

fn execute(request: &Request, operation: &mut native::Operation) -> Result<bool> {
    let cache = cache(request)?;
    let source = &request.provenance;
    let events = ffi::text(&source.provider_event_path);
    match fs::remove_file(&events) {
        Ok(()) => (),
        Err(error) if error.kind() == std::io::ErrorKind::NotFound => (),
        Err(error) => return Err(error.into()),
    }
    let log_base = ffi::text(&source.supervisor_log_path)
        .strip_suffix(".acquisition.supervisor.log")
        .ok_or_else(|| acquire::state_error("invalid supervisor log path"))?
        .to_owned();
    let stdout = private_log(&format!("{log_base}.download.stdout.log"))?;
    let stderr = private_log(&format!("{log_base}.download.stderr.log"))?;
    let mut signals = signal_hook::iterator::Signals::new([
        signal_hook::consts::SIGINT,
        signal_hook::consts::SIGTERM,
    ])?;
    let mut child = acquire::provider(request)?
        .stdout(stdout)
        .stderr(stderr)
        .spawn()?;
    let identity = match native::capture(child.id().try_into()?, child.id().try_into()?) {
        Ok(identity) => identity,
        Err(error) => {
            let _ = child.kill();
            let _ = child.wait();
            return Err(error.into());
        }
    };
    operation.provider_process = identity;
    operation.lifecycle =
        raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_DOWNLOADING;
    let work = (|| -> Result<bool> {
        current(request, operation)?;
        native::publish(source, operation)?;
        let mut stopped = false;
        let mut deadline = None;
        let mut last = Instant::now()
            .checked_sub(Duration::from_secs(request.tick))
            .unwrap_or_else(Instant::now);
        loop {
            if let Some(exit) = child.try_wait()? {
                let mut outcome = request.outcome.get();
                outcome.exit = exit
                    .code()
                    .or_else(|| exit.signal().map(|signal| 128 + signal));
                request.outcome.set(outcome);
                native::observe(source, cache.as_deref(), operation, acquisition::now()?)?;
                if !exit.success() && !stopped {
                    return Err(acquire::state_error(
                        "provider download exited nonzero; partial source preserved",
                    )
                    .into());
                }
                return Ok(stopped);
            }
            if let Some(signal) = signals.pending().next().filter(|_| !stopped) {
                acquisition::signal(&identity, true, Signal::TERM)?;
                stopped = true;
                let mut outcome = request.outcome.get();
                outcome.interrupt = Some(signal);
                request.outcome.set(outcome);
                deadline = Some(Instant::now() + Duration::from_secs(request.timeout));
            }
            if deadline.is_some_and(|deadline| Instant::now() >= deadline) {
                acquisition::signal(&identity, true, Signal::KILL)?;
                let exit = child.wait()?;
                let mut outcome = request.outcome.get();
                outcome.forced = true;
                outcome.exit = exit
                    .code()
                    .or_else(|| exit.signal().map(|signal| 128 + signal));
                request.outcome.set(outcome);
                native::observe(source, cache.as_deref(), operation, acquisition::now()?)?;
                return Ok(true);
            }
            if last.elapsed() >= Duration::from_secs(request.tick) {
                let mut outcome = request.outcome.get();
                outcome.ticks += 1;
                request.outcome.set(outcome);
                current(request, operation)?;
                let now = acquisition::now()?;
                native::observe(source, cache.as_deref(), operation, now)?;
                native::reconcile(operation, now)?;
                operation.updated_unix = now;
                native::publish(source, operation)?;
                last = Instant::now();
            }
            std::thread::sleep(Duration::from_millis(50));
        }
    })();
    if work.is_err() {
        // Any publication/observation failure retires this worker's child before
        // leaving scope. A stale durable identity never authorizes another PID.
        acquisition::signal(&identity, true, Signal::KILL)?;
        child.wait()?;
    }
    work
}

fn complete(request: &Request, operation: &mut native::Operation, stopped: bool) -> Result<()> {
    if !stopped {
        operation.lifecycle =
            raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_FINALIZING;
        current(request, operation)?;
        native::publish(&request.provenance, operation)?;
        acquisition_report::finalize(request, operation)?;
    } else {
        acquisition_report::retain(request, Some(operation), "model-download-interrupted", None)?;
    }
    operation.lifecycle = if stopped {
        raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STOPPED
    } else {
        raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_COMPLETE
    };
    operation.health =
        raw::yvex_source_acquisition_health_YVEX_SOURCE_ACQUISITION_HEALTH_NOT_APPLICABLE;
    ffi::put_text(
        &mut operation.reason,
        if stopped {
            "operator-stop"
        } else {
            "selected-source-complete"
        },
    )?;
    ffi::put_text(
        &mut operation.result,
        if stopped {
            "interrupted-resumable"
        } else {
            "pass"
        },
    )?;
    operation.updated_unix = acquisition::now()?;
    operation.provider_process = native::Process::default();
    current(request, operation)?;
    native::publish(&request.provenance, operation)?;
    Ok(())
}

pub(crate) fn run(request: &Request) -> Result<()> {
    let source = &request.provenance;
    let mut operation = native::read(source)?;
    if std::env::var("YVEX_SOURCE_ACQUISITION_OPERATION")? != ffi::text(&source.operation_path)
        || std::env::var("YVEX_SOURCE_ACQUISITION_ID")? != ffi::text(&operation.operation_id)
        || operation.lifecycle
            != raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_STARTING
        || ffi::text(&operation.selection_identity) != request.selection
    {
        return Err(
            acquire::state_error("worker invocation does not own the durable operation").into(),
        );
    }
    operation.supervisor = native::capture(
        process::getpid().as_raw_nonzero().get(),
        process::getpgrp().as_raw_nonzero().get(),
    )?;
    native::publish(source, &operation)?;
    let work = (|| -> Result<()> {
        let _transfer = native::try_transfer_lock(
            &request.root,
            &ffi::text(&source.repo_id),
            &ffi::text(&source.revision),
        )?;
        let count = acquisition::stale_locks(source, &request.root, request.clear_stale_locks)?;
        if count != 0 && !request.clear_stale_locks {
            return Err(
                acquire::state_error("stale-lock-candidates: explicit clearance required").into(),
            );
        }
        let mut outcome = request.outcome.get();
        outcome.cleared_locks = if request.clear_stale_locks { count } else { 0 };
        request.outcome.set(outcome);
        execute(request, &mut operation)
            .and_then(|stopped| complete(request, &mut operation, stopped))
    })();
    if let Err(error) = &work {
        if current(request, &operation).is_ok() {
            operation.lifecycle =
                raw::yvex_source_acquisition_lifecycle_YVEX_SOURCE_ACQUISITION_FAILED;
            operation.health =
                raw::yvex_source_acquisition_health_YVEX_SOURCE_ACQUISITION_HEALTH_DEGRADED;
            operation.provider_process = native::Process::default();
            ffi::put_text(&mut operation.reason, "acquisition-failed")?;
            ffi::put_text(&mut operation.result, "failed-resumable")?;
            operation.updated_unix = acquisition::now()?;
            // A secondary report failure must not leave the owned operation running.
            let receipt =
                acquisition_report::retain(request, Some(&operation), "model-download-fail", None);
            native::publish(source, &operation)?;
            if let Err(error) = receipt {
                eprintln!("yvex: acquisition failure receipt unavailable: {error}");
            }
        }
        eprintln!("yvex: source acquisition failed: {error}");
    }
    work
}
