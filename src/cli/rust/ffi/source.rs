// Borrowed source reports are pinned, bounds-checked and copied at the FFI boundary.
use super::{Error, argument, borrowed_text, error, extent_error, pointer, raw};
use std::ffi::CString;

pub(crate) struct ManifestRequest<'a> {
    pub repository: &'a str,
    pub revision: &'a str,
    pub local: &'a str,
    pub out: &'a str,
    pub license: Option<&'a str>,
    pub card: Option<&'a str>,
    pub node: Option<&'a str>,
    pub dry_run: Option<&'a str>,
    pub log: Option<&'a str>,
    pub pid: Option<&'a str>,
    pub command: Option<&'a str>,
    pub status: raw::yvex_source_status,
}

pub(crate) struct NativeTensor {
    pub name: String,
    pub shard: String,
    pub dtype: String,
    pub dimensions: Vec<u64>,
    pub bytes: u64,
    pub begin: u64,
    pub end: u64,
}
pub(crate) struct NativeWeights {
    pub summary: raw::yvex_native_weight_summary,
    pub tensors: Vec<NativeTensor>,
}
struct NativeTable(*mut raw::yvex_native_weight_table);
impl Drop for NativeTable {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_native_weight_table_close(self.0);
        }
    }
}

fn native_tensor(row: &raw::yvex_native_weight_info) -> Result<NativeTensor, Error> {
    if row.rank as usize > row.dims.len() {
        return Err(extent_error());
    }
    Ok(NativeTensor {
        name: unsafe { borrowed_text(row.name)? },
        shard: unsafe { borrowed_text(row.shard_path)? },
        dtype: unsafe { borrowed_text(raw::yvex_native_dtype_name(row.dtype))? },
        dimensions: row.dims[..row.rank as usize].to_vec(),
        bytes: row.data_bytes,
        begin: row.data_start,
        end: row.data_end,
    })
}

pub(crate) fn native_weights(
    source: &str,
    selected: Option<&str>,
    limit: u64,
) -> Result<NativeWeights, Error> {
    let source = argument(Some(source))?.expect("required native source");
    let selected = argument(selected)?;
    let options = raw::yvex_native_weight_options {
        source_dir: source.as_ptr(),
        recursive: 1,
        include_metadata: 0,
    };
    let mut table = NativeTable(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_native_weight_table_open(&mut table.0, &options, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let mut summary = raw::yvex_native_weight_summary::default();
    let status =
        unsafe { raw::yvex_native_weight_table_summary(table.0, &mut summary, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let mut tensors = Vec::new();
    if let Some(selected) = &selected {
        let row =
            unsafe { raw::yvex_native_weight_table_find(table.0, selected.as_ptr()).as_ref() }
                .ok_or_else(|| Error {
                    code: -4,
                    owner: "native_weights".into(),
                    message: format!("native tensor not found: {}", selected.to_string_lossy()),
                })?;
        tensors.push(native_tensor(row)?);
    } else {
        let count = unsafe { raw::yvex_native_weight_table_count(table.0) };
        for index in 0..count.min(limit) {
            let row = unsafe { raw::yvex_native_weight_table_at(table.0, index).as_ref() }
                .ok_or_else(extent_error)?;
            tensors.push(native_tensor(row)?);
        }
    }
    Ok(NativeWeights { summary, tensors })
}

pub(crate) fn source_manifest(
    request: ManifestRequest<'_>,
) -> Result<raw::yvex_source_manifest_summary, Error> {
    let values = [
        Some(request.repository),
        Some(request.revision),
        Some(request.local),
        Some(request.out),
        request.license,
        request.card,
        request.node,
        request.dry_run,
        request.log,
        request.pid,
        request.command,
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let options = raw::yvex_source_manifest_options {
        repo: pointer(&values[0]),
        revision: pointer(&values[1]),
        local_path: pointer(&values[2]),
        license: pointer(&values[4]),
        model_card: pointer(&values[5]),
        node_name: pointer(&values[6]),
        dry_run_log: pointer(&values[7]),
        download_log: pointer(&values[8]),
        pid_file: pointer(&values[9]),
        download_command: pointer(&values[10]),
        status: request.status,
        include_files: 1,
    };
    let mut summary = raw::yvex_source_manifest_summary::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        raw::yvex_source_manifest_write_json(
            pointer(&values[3]),
            &options,
            &mut summary,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(summary)
}

// Resolve verifier identity from its native repository/revision catalog, never a UI alias.
pub(crate) fn source_verification_target(
    repository: &str,
    revision: &str,
) -> Result<String, Error> {
    let repository = argument(Some(repository))?;
    let identity =
        unsafe { raw::yvex_source_target_identity_find_repository(pointer(&repository)) };
    if identity.is_null() {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "source_verify".into(),
            message: "source_verification_contract_unavailable".into(),
        });
    }
    // Native catalog entries are immutable process-lifetime records.
    let identity = unsafe { &*identity };
    if unsafe { borrowed_text(identity.upstream_revision)? } != revision {
        return Err(Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "source_verify".into(),
            message: "source_verification_revision_unsupported".into(),
        });
    }
    unsafe { borrowed_text(identity.target_id) }
}

pub(crate) fn source_verify(
    source: &str,
    root: &str,
    manifest: &str,
    target: Option<&str>,
) -> Result<(Box<raw::yvex_source_payload_verification_result>, String), Error> {
    let target = target.unwrap_or_else(|| {
        std::str::from_utf8(raw::YVEX_SOURCE_RELEASE_TARGET_ID)
            .expect("native ASCII identity")
            .trim_end_matches('\0')
    });
    let values = [Some(source), Some(root), Some(manifest), Some(target)]
        .into_iter()
        .map(argument)
        .collect::<Result<Vec<_>, _>>()?;
    let identity = unsafe { raw::yvex_source_target_identity_find(pointer(&values[3])) };
    if identity.is_null() {
        return Err(Error {
            code: -10,
            owner: "source_verify".into(),
            message: format!(
                "unsupported target: {target}; use {}",
                std::str::from_utf8(raw::YVEX_SOURCE_RELEASE_TARGET_ID)
                    .unwrap()
                    .trim_end_matches('\0')
            ),
        });
    }
    let options = raw::yvex_source_verify_options {
        identity,
        source_path: pointer(&values[0]),
        models_root: pointer(&values[1]),
        manifest_path: pointer(&values[2]),
        promote_manifest: 1,
        ..Default::default()
    };
    let mut budget = raw::yvex_source_payload_budget::default();
    unsafe {
        raw::yvex_source_payload_budget_default(&mut budget);
    }
    // A local-only seal must not be promoted into upstream-authoritative payload trust.
    budget.allow_local_snapshot_seal = 0;
    let mut result = Box::<raw::yvex_source_payload_verification_result>::default();
    let mut failure = raw::yvex_error::default();
    let mut detail = raw::yvex_source_payload_failure::default();
    let status = unsafe {
        raw::yvex_source_payload_verify_snapshot(
            &options,
            &budget,
            result.as_mut(),
            &mut detail,
            &mut failure,
        )
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let trust = unsafe {
        borrowed_text(raw::yvex_source_payload_trust_class_name(
            result.payload.trust_class,
        ))?
    };
    Ok((result, trust))
}

pub(crate) struct ReportRequest<'a> {
    pub family: &'a str,
    pub release: &'a str,
    pub root: Option<&'a str>,
    pub source: Option<&'a str>,
    pub target: Option<&'a str>,
    pub files: bool,
    pub config: bool,
    pub blockers: bool,
    pub next: bool,
    pub tensors: bool,
    pub strict: bool,
    pub limit: u64,
}

// Named owned projections replace C pointers; they do not invent source semantics.
macro_rules! owned_text {
    ($name:ident, $native:ty, [$($field:ident),* $(,)?]) => {
        #[derive(serde::Serialize)]
        pub(crate) struct $name { $(pub $field: String),* }
        impl $name {
            fn copy(native: &$native) -> Result<Self, Error> {
                Ok(Self { $($field: if native.$field.is_null() { String::new() }
                    else { unsafe { borrowed_text(native.$field)? } }),* })
            }
        }
    };
}
owned_text!(
    SourceSemantics,
    raw::yvex_source_report_semantics,
    [
        verification_status,
        repository_status,
        revision_status,
        config_identity_status,
        tokenizer_verification_status,
        generation_config_status,
        shard_index_status,
        inventory_authority,
        upstream_index_identity_status,
        model_name,
        config_presence,
        generation_config_presence,
        tokenizer_json_presence,
        tokenizer_config_presence,
        tokenizer_status,
        safetensors_status,
        manifest_status,
        native_inventory_report_status,
        tensor_map_report_status,
        tensor_role_map_report_status,
        output_head_map_report_status,
        tokenizer_map_report_status,
        native_inventory_status,
        native_inventory_source,
        tensor_metadata_status,
        tensor_metadata_source,
        native_tensor_metadata_status,
        native_tensor_payload_status,
        sidecar_status,
        tensor_payload_status,
        target_artifact_status,
        footprint_class,
        footprint_status,
        provenance_origin_normal,
        provenance_origin_audit,
        provenance_status,
        identity_status,
        authority,
        authority_status,
        manifest_provenance_status,
        manifest_authority,
        manifest_schema_status,
        manifest_family_status,
        manifest_target_status,
        manifest_artifact_class_status,
        manifest_footprint_status,
        manifest_native_inventory_status,
        manifest_tensor_metadata_status,
        manifest_consistency_status,
        manifest_hardening_status,
    ]
);
owned_text!(
    SourceProfile,
    raw::yvex_source_family_profile,
    [
        family_key,
        report_name,
        target_id,
        target_class,
        source_target_status,
        source_family_profile_status,
        source_artifact_class,
        source_artifact_format,
        source_artifact_origin,
        source_artifact_authority,
        source_tensor_container,
        target_artifact_class,
        target_artifact_origin,
        target_artifact_required,
        external_reference_status,
        pressure_purpose,
        runtime_shape,
        source_class,
        yvex_produced_artifact_status,
        hardware_lane,
        backend_lane,
    ]
);

pub(crate) struct SourceReport {
    pub native: Box<raw::yvex_source_report>,
    pub semantics: SourceSemantics,
    pub profile: SourceProfile,
    pub status: String,
    pub state: String,
    pub top_blocker: String,
    pub next: String,
    pub target: String,
    pub blockers: Vec<String>,
    // C request pointers borrow CString allocations, not Rust object layouts.
    _arguments: Vec<Option<CString>>,
}

pub(crate) fn source_report(request: ReportRequest<'_>) -> Result<SourceReport, Error> {
    let values = [
        Some(request.family),
        Some(request.release),
        request.root,
        request.source,
        request.target,
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let mut selected = Box::new(raw::yvex_source_report_request {
        family: pointer(&values[0]),
        release: pointer(&values[1]),
        models_root: pointer(&values[2]),
        source: pointer(&values[3]),
        target: pointer(&values[4]),
        include_files: request.files.into(),
        include_config: request.config.into(),
        include_blockers: request.blockers.into(),
        include_next: request.next.into(),
        include_tensors: request.tensors.into(),
        strict: request.strict.into(),
        tensor_limit: request.limit,
        ..Default::default()
    });
    let mut failure = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_source_report_request_prepare(selected.as_mut(), &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let mut native = Box::<raw::yvex_source_report>::default();
    let status =
        unsafe { raw::yvex_source_report_build(selected.as_ref(), native.as_mut(), &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    if native.blocker_count as usize > native.blockers.len()
        || native.source_tensor_sample_count as usize > native.source_tensor_samples.len()
        || native.verification.compress_ratio_count as usize
            > native.verification.compress_ratios.len()
    {
        return Err(extent_error());
    }
    let semantics = SourceSemantics::copy(&native.semantics)?;
    let profile = unsafe { native.profile.as_ref() }.ok_or_else(extent_error)?;
    let profile = SourceProfile::copy(profile)?;
    let blockers = native.blockers[..native.blocker_count as usize]
        .iter()
        .map(|&blocker| unsafe { borrowed_text(blocker) })
        .collect::<Result<Vec<_>, _>>()?;
    Ok(SourceReport {
        semantics,
        profile,
        status: unsafe { borrowed_text(native.status)? },
        state: unsafe { borrowed_text(native.source_state)? },
        top_blocker: unsafe { borrowed_text(native.top_blocker)? },
        next: unsafe { borrowed_text(native.next_row)? },
        target: unsafe { borrowed_text(native.request.target)? },
        native,
        blockers,
        _arguments: values,
    })
}
