// Machine acquisition receipts project native source observations and preserve non-claims.
use crate::{
    acquire::{self, Request},
    acquisition::Result,
    ffi::{self, acquisition as native, raw},
};
use serde_json::{Value, json};

/// Audit is a presentation of the same retained facts, not a second receipt.
pub(crate) fn human(report: &Value, width: usize, styled: bool) -> Result<String> {
    let mut fields = Vec::new();
    for (key, value) in report
        .as_object()
        .ok_or_else(|| acquire::state_error("invalid report object"))?
    {
        if let Some(children) = value.as_object() {
            for (child, fact) in children {
                fields.push((format!("{key}.{child}"), display(fact)));
            }
        } else {
            fields.push((key.clone(), display(value)));
        }
    }
    let borrowed = fields
        .iter()
        .map(|(key, value)| (key.as_str(), value.as_str()))
        .collect::<Vec<_>>();
    Ok(crate::presentation::record(
        "SOURCE  acquisition audit",
        &borrowed,
        width,
        styled,
    )?)
}

fn display(value: &Value) -> String {
    value
        .as_str()
        .map(str::to_owned)
        .unwrap_or_else(|| value.to_string())
}

pub(crate) fn projection(
    request: &Request,
    operation: Option<&native::Operation>,
    status: &str,
    representation: Option<&raw::yvex_source_representation_fact>,
) -> Result<Value> {
    let source = &request.provenance;
    let scan = native::scan(
        &ffi::text(&source.local_source_dir),
        acquisition_cache(request).as_deref(),
    )?;
    let observation = &request.observation;
    let includes = source.includes[..source.include_count as usize]
        .iter()
        .map(|value| ffi::text(value))
        .collect::<Vec<_>>();
    let excludes = source.excludes[..source.exclude_count as usize]
        .iter()
        .map(|value| ffi::text(value))
        .collect::<Vec<_>>();
    let mut report = json!({"schema": "yvex.model_download.report.v1", "status": status,
        "target_id": ffi::text(&source.target_id), "family": ffi::text(&source.family),
        "provider": ffi::text(&source.provider),
        "repo_id": ffi::text(&source.repo_id), "revision": ffi::text(&source.revision),
        "local_source_dir": ffi::text(&source.local_source_dir), "models_root": request.root,
        "include_patterns": includes, "exclude_patterns": excludes, "auth_mode": request.auth,
        "provider_cli_path": ffi::text(&observation.cli_path),
        "provider_cli_status": ffi::text(&observation.cli_status),
        "credential_source": ffi::text(&observation.credential_source),
        "account_hint": ffi::text(&observation.account_hint),
        "auth_state": ffi::text(&observation.auth_state), "accounts_state_path": ffi::text(&observation.state_path),
        "token_value_redacted": observation.token_value_redacted != 0,
        "top_blocker": if representation.is_some() { "none".into() } else {
            operation.map_or_else(|| ffi::text(&observation.top_blocker),
                |operation| ffi::text(&operation.reason)) },
        "provider_pid": operation.map_or(0, |operation| operation.provider_process.pid),
        "provider_process_group": operation.map_or(0, |operation| operation.provider_process.process_group),
        "file_count": scan.files, "safetensors_count": scan.shards, "gguf_count": scan.gguf_files,
        "config_present": scan.config_present != 0, "tokenizer_present": scan.tokenizer_present != 0,
        "total_regular_file_bytes": scan.bytes, "largest_file_name": ffi::text(&scan.largest_file),
        "largest_file_bytes": scan.largest_bytes, "manifest_path": ffi::text(&source.manifest_path),
        "native_inventory_path": ffi::text(&source.native_inventory_path),
        "source_payload_digest": representation.map_or_else(
            || ffi::text(&source.source_payload_digest), |value| ffi::text(&value.digest)),
        "representation_format": representation.map_or(String::new(), |value| ffi::text(&value.format)),
        "representation_precision": representation.map_or(String::new(), |value| ffi::text(&value.precision)),
        "remote_lookup_performed": ffi::text(&source.provider) == "huggingface",
        "upstream_identity_verified": ffi::text(&source.provider) == "huggingface",
        "payload_hash_verified": false, "partial_source_preserved": true,
        "lock_files_deleted": request.outcome.get().cleared_locks != 0,
        "boundary": {"source_download": if request.dry { "dry-run" } else if representation.is_some() { "performed" }
            else if status == "model-download-running" { "in-progress" } else { "failed" },
            "source_manifest_written": representation.is_some() && !request.no_manifest,
            "native_inventory_written": representation.is_some() && !request.no_inventory,
            "payload_loaded": false, "gguf_created": false, "materialized": false, "runtime_ready": false,
            "generation": "unsupported", "eval": "unsupported", "benchmark": "not-measured"}});
    report.as_object_mut().expect("report object").extend(
        diagnostics(request, operation)?
            .as_object()
            .expect("diagnostics object")
            .clone(),
    );
    Ok(report)
}

fn diagnostics(request: &Request, _operation: Option<&native::Operation>) -> Result<Value> {
    let log = ffi::text(&request.provenance.supervisor_log_path);
    let base = log
        .strip_suffix(".acquisition.supervisor.log")
        .ok_or_else(|| acquire::state_error("invalid source log identity"))?;
    let stdout = format!("{base}.download.stdout.log");
    let stderr = format!("{base}.download.stderr.log");
    let extent = |path: &str| -> Result<Option<u64>> {
        match std::fs::symlink_metadata(path) {
            Ok(value) if value.is_file() => Ok(Some(value.len())),
            Ok(_) => Err(acquire::state_error("source log is not a regular file").into()),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(error) => Err(error.into()),
        }
    };
    let cache = |key: &str, suffix: &str| {
        let override_value = std::env::var(key).ok().filter(|value| !value.is_empty());
        let source = if override_value.is_some() {
            "environment"
        } else {
            "models-root"
        };
        (
            override_value
                .unwrap_or_else(|| format!("{}/cache/provider/huggingface/{suffix}", request.root)),
            source,
        )
    };
    let (hub, hub_source) = cache("HF_HUB_CACHE", "hub");
    let (xet, xet_source) = cache("HF_XET_CACHE", "xet");
    let outcome = request.outcome.get();
    let interrupted = outcome.interrupt.is_some();
    let child_status = if outcome.exit.is_none() {
        "unknown"
    } else if interrupted
        && (outcome.exit == Some(0) || outcome.exit == outcome.interrupt.map(|signal| 128 + signal))
    {
        "interrupted"
    } else if outcome.exit == Some(0) {
        "exited"
    } else {
        "terminated"
    };
    let signal = match outcome.interrupt {
        Some(2) => "SIGINT",
        Some(15) => "SIGTERM",
        None => "none",
        _ => "other",
    };
    Ok(json!({"models_root_source": request.root_source,
        "account_provider_stage": if request.auth == "never" { "skipped" } else { "pass" },
        "hf_hub_cache": hub, "hf_hub_cache_source": hub_source,
        "hf_xet_cache": xet, "hf_xet_cache_source": xet_source,
        "hf_xet_high_performance": std::env::var("HF_XET_HIGH_PERFORMANCE").unwrap_or_default(),
        "hf_cli_path": if ffi::text(&request.provenance.provider) == "huggingface" {
            ffi::text(&request.observation.cli_path) } else { String::new() },
        "hf_exit_code": outcome.exit, "provider_exit_code": outcome.exit,
        "progress_mode": request.progress, "tick_seconds": request.tick,
        "tick_count": outcome.ticks,
        "stdout_streamed": false, "stderr_streamed": false,
        "stdout_bytes": extent(&stdout)?, "stderr_bytes": extent(&stderr)?,
        "interrupted": interrupted, "interrupt_signal": signal, "signal_forwarded": interrupted,
        "child_terminated": outcome.exit.is_some(), "child_killed_after_timeout": outcome.forced,
        "child_exit_status": child_status, "orphan_check_performed": outcome.exit.is_some(),
        "orphan_check_status": if outcome.exit.is_some() { "child-reaped" } else { "unknown" },
        "stdout_log": stdout, "stderr_log": stderr,
        "created_at": native::timestamp(), "yvex_version": ffi::version(), "dry_run": request.dry,
        "force_sidecars": request.force_sidecars, "yes": request.yes}))
}

pub(crate) fn retain(
    request: &Request,
    operation: Option<&native::Operation>,
    status: &str,
    representation: Option<&raw::yvex_source_representation_fact>,
) -> Result<Value> {
    let source = &request.provenance;
    let mut report = projection(request, operation, status, representation)?;
    let path = ffi::text(&source.download_report_path);
    acquire::parent(&path)?;
    ffi::publish(&path, &serde_json::to_vec_pretty(&report)?, true)?;
    report["schema"] = json!("yvex.model_download.registry.v1");
    let path = ffi::text(&source.registry_path);
    acquire::parent(&path)?;
    ffi::publish(&path, &serde_json::to_vec_pretty(&report)?, true)?;
    report["schema"] = json!("yvex.model_download.report.v1");
    Ok(report)
}

pub(crate) fn acquisition_cache(request: &Request) -> Option<String> {
    (ffi::text(&request.provenance.provider) == "huggingface").then(|| {
        let source = ffi::text(&request.provenance.local_source_dir);
        let leaf = std::path::Path::new(&source)
            .file_name()
            .expect("bounded source path")
            .to_string_lossy();
        format!(
            "{}/cache/hf/{}/{leaf}",
            request.root,
            ffi::text(&request.provenance.repo_id)
        )
    })
}

pub(crate) fn read(request: &Request) -> Result<Value> {
    let bytes = ffi::snapshot(
        &ffi::text(&request.provenance.download_report_path),
        256 * 1024,
    )?;
    let mut value = serde_json::from_slice::<Value>(&bytes)?;
    if value["schema"] != "yvex.model_download.report.v1"
        || value["revision"] != ffi::text(&request.provenance.revision)
        || value["local_source_dir"] != ffi::text(&request.provenance.local_source_dir)
    {
        return Err(acquire::state_error("acquisition result identity mismatch").into());
    }
    if ["model-download-pass", "model-download-resume-pass"]
        .contains(&value["status"].as_str().unwrap_or(""))
    {
        let fact = native::reopen(&request.provenance, &request.root)?;
        if value["source_payload_digest"] != ffi::text(&fact.digest) {
            return Err(
                acquire::state_error("acquisition payload no longer matches receipt").into(),
            );
        }
    }
    if request.resume && value["status"] == "model-download-pass" {
        // Command observation only: retained generation and receipt stay intact.
        value["status"] = json!("model-download-resume-pass");
    }
    Ok(value)
}

/// Command result remains distinct from the complete retained acquisition report.
pub(crate) fn command(request: &Request, report: &Value, reason: &str) -> Value {
    let source = &request.provenance;
    let projection = json!({"schema": "yvex.model.pull.v1", "status": report["status"],
        "model": ffi::text(&source.local_name), "family": ffi::text(&source.family),
        "provider": ffi::text(&source.provider), "repository": ffi::text(&source.repo_id),
        "revision": ffi::text(&source.revision), "location": ffi::text(&source.local_source_dir),
        "format": report["representation_format"], "precision": report["representation_precision"],
        "local_content_digest": report["source_payload_digest"], "files": report["file_count"],
        "partial_files": report["partial_files"].as_u64().unwrap_or(0),
        "safetensors_files": report["safetensors_count"], "gguf_files": report["gguf_count"],
        "bytes": report["total_regular_file_bytes"],
        "upstream_identity_verified": report["upstream_identity_verified"],
        "payload_hash_verified": report["payload_hash_verified"], "interrupted": report["interrupted"],
        "report": ffi::text(&source.download_report_path), "reason": reason});
    let mut result = report.clone();
    result
        .as_object_mut()
        .expect("validated report object")
        .extend(projection.as_object().expect("command object").clone());
    result
}

pub(crate) fn finalize(request: &Request, operation: &native::Operation) -> Result<()> {
    let source = &request.provenance;
    let mut precision = String::new();
    if !request.no_manifest {
        ffi::source_manifest(ffi::ManifestRequest {
            repository: &ffi::text(&source.repo_id),
            revision: &ffi::text(&source.revision),
            local: &ffi::text(&source.local_source_dir),
            out: &ffi::text(&source.manifest_path),
            license: None,
            card: None,
            node: Some(&ffi::text(&source.local_name)),
            dry_run: None,
            log: None,
            pid: None,
            command: Some("provider download; credentials redacted"),
            status: raw::yvex_source_status_YVEX_SOURCE_STATUS_IN_PROGRESS,
        })?;
    }
    if !request.no_inventory {
        let inventory = ffi::native_weights(&ffi::text(&source.local_source_dir), None, u64::MAX)?;
        // Project the native inventory's exact dtype labels, as the prior
        // receipt did; do not infer precision from provider/model names.
        if let Some(first) = inventory.tensors.first() {
            precision = if inventory
                .tensors
                .iter()
                .any(|tensor| tensor.dtype != first.dtype)
            {
                "MIXED".into()
            } else {
                first.dtype.clone()
            };
        }
        let value = json!({"schema": "yvex.native_inventory.v1", "source_dir": ffi::text(&source.local_source_dir),
            "payload_loaded": false, "summary": {"shard_count": inventory.summary.shard_count,
                "tensor_count": inventory.summary.tensor_count,
                "total_tensor_bytes": inventory.summary.total_tensor_bytes,
                "unknown_dtype_count": inventory.summary.unknown_dtype_count,
                "malformed_shard_count": inventory.summary.malformed_shard_count},
            "tensors": inventory.tensors.iter().map(|tensor|
                json!({"name": tensor.name, "shard_path": tensor.shard, "dtype": tensor.dtype,
                    "rank": tensor.dimensions.len(), "shape": tensor.dimensions, "data_bytes": tensor.bytes,
                    "data_start": tensor.begin, "data_end": tensor.end})).collect::<Vec<_>>()});
        ffi::publish(
            &ffi::text(&source.native_inventory_path),
            &serde_json::to_vec(&value)?,
            true,
        )?;
    }
    let mut representation = native::verify(source, Some(&request.root))?;
    ffi::put_text(&mut representation.precision, &precision)?;
    let scan = native::scan(
        &ffi::text(&source.local_source_dir),
        acquisition_cache(request).as_deref(),
    )?;
    if scan.bytes != representation.size_bytes {
        return Err(acquire::state_error("source changed while recording content identity").into());
    }
    retain(
        request,
        Some(operation),
        if request.resume {
            "model-download-resume-pass"
        } else {
            "model-download-pass"
        },
        Some(&representation),
    )?;
    Ok(())
}
