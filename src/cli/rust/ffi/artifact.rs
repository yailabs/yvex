// GGUF emission and template lifetimes stay in the native artifact owners.
use super::{Error, ModelView, argument, borrowed_text, error, extent_error, pointer, raw};

#[derive(serde::Serialize)]
pub(crate) struct MaterializeFacts {
    pub materialization_status: String,
    pub materialization_gate: String,
    pub materialization_phase: String,
    pub shape_status: String,
    pub range_status: String,
    pub backend_status: String,
    pub cleanup_status: String,
    pub allocation_attempted: bool,
    pub transfer_attempted: bool,
    pub cleanup_attempted: bool,
    pub tensors_total: u64,
    pub tensors_materialized: u64,
    pub tensors_failed: u64,
    pub bytes_total: u64,
    pub bytes_materialized: u64,
    pub backend_allocated_bytes: u64,
    pub bytes_planned: u64,
    pub bytes_allocated: u64,
    pub bytes_transferred: u64,
    pub execution_ready: bool,
    pub resource_retirement: String,
}

pub(crate) struct Materialization {
    pub facts: MaterializeFacts,
    pub failure: Option<Error>,
    pub complete: bool,
}

pub(crate) struct AllocationProof {
    pub tensors: u64,
    pub bytes: u64,
    pub cleanup: bool,
    pub failure: Option<Error>,
}

struct ProofResources {
    resources: WeightResources,
    tensors: Vec<*mut raw::yvex_device_tensor>,
}

impl ProofResources {
    fn retire(&mut self) -> Result<(), Error> {
        let mut first_failure = None;
        for tensor in self.tensors.iter_mut().rev() {
            if tensor.is_null() {
                continue;
            }
            let mut failure = raw::yvex_error::default();
            let status = unsafe {
                raw::yvex_backend_tensor_release(self.resources.backend, tensor, &mut failure)
            };
            if first_failure.is_none() {
                if status != 0 {
                    first_failure = Some(error(status, &failure));
                } else if !tensor.is_null() {
                    first_failure = Some(extent_error());
                }
            }
        }
        if let Some(failure) = first_failure {
            return Err(failure);
        }
        self.tensors.clear();
        Ok(())
    }
}

impl Drop for ProofResources {
    fn drop(&mut self) {
        // A best-effort retry is not successful observed retirement evidence.
        let _ = self.retire();
    }
}

struct WeightResources {
    backend: *mut raw::yvex_backend,
    weights: *mut raw::yvex_weight_table,
}
impl WeightResources {
    fn open(name: &str) -> Result<Self, Error> {
        let name = argument(Some(name))?.unwrap();
        let mut kind = raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU;
        let mut failure = raw::yvex_error::default();
        let status =
            unsafe { raw::yvex_backend_kind_parse(name.as_ptr(), &mut kind, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        let mut resources = Self {
            backend: std::ptr::null_mut(),
            weights: std::ptr::null_mut(),
        };
        let options = raw::yvex_backend_options {
            kind,
            ..Default::default()
        };
        let status =
            unsafe { raw::yvex_backend_open(&mut resources.backend, &options, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        if resources.backend.is_null() {
            return Err(extent_error());
        }
        Ok(resources)
    }

    fn retire(&mut self, baseline: u64) -> Result<(), Error> {
        // Weights borrow their backend. Retire them before checking/closing that owner.
        unsafe { raw::yvex_weight_table_close(self.weights) };
        self.weights = std::ptr::null_mut();
        let mut stats = raw::yvex_backend_memory_stats::default();
        let mut failure = raw::yvex_error::default();
        let status =
            unsafe { raw::yvex_backend_get_memory_stats(self.backend, &mut stats, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        if stats.allocated_bytes != baseline {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "model.materialization.retirement".into(),
                message: "backend allocation accounting did not return to its baseline".into(),
            });
        }
        let status = unsafe { raw::yvex_backend_close_checked(&mut self.backend, &mut failure) };
        if status != 0 {
            return Err(error(status, &failure));
        }
        Ok(())
    }
}
impl Drop for WeightResources {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_weight_table_close(self.weights);
            if !self.backend.is_null() {
                let mut failure = raw::yvex_error::default();
                raw::yvex_backend_close_checked(&mut self.backend, &mut failure);
            }
        }
    }
}

impl ModelView {
    pub(crate) fn plan_proof(
        &self,
        sequence: u64,
        context: u64,
        backend: &str,
    ) -> Result<(), Error> {
        struct Plan(*mut raw::yvex_plan);
        impl Drop for Plan {
            fn drop(&mut self) {
                unsafe { raw::yvex_plan_close(self.0) };
            }
        }
        let backend = argument(Some(backend))?.expect("required backend");
        let options = raw::yvex_plan_options {
            sequence_length: sequence,
            context_length: context,
            backend_name: backend.as_ptr(),
        };
        let mut plan = Plan(std::ptr::null_mut());
        let mut failure = raw::yvex_error::default();
        super::execution::checked(
            unsafe {
                raw::yvex_plan_create(
                    &mut plan.0,
                    self.native.model,
                    self.native.table,
                    &options,
                    &mut failure,
                )
            },
            &failure,
        )?;
        if plan.0.is_null() {
            return Err(extent_error());
        }
        Ok(())
    }
    pub(crate) fn allocation_proof(
        &self,
        backend: &str,
        indices: &[usize],
    ) -> Result<AllocationProof, Error> {
        let mut resources = ProofResources {
            resources: WeightResources::open(backend)?,
            tensors: Vec::new(),
        };
        resources
            .tensors
            .try_reserve_exact(indices.len())
            .map_err(|_| Error {
                code: raw::yvex_status_YVEX_ERR_NOMEM,
                owner: "model.allocation.proof".into(),
                message: "allocation-owner list cannot be reserved".into(),
            })?;
        let mut stats = raw::yvex_backend_memory_stats::default();
        let mut failure = raw::yvex_error::default();
        super::execution::checked(
            unsafe {
                raw::yvex_backend_get_memory_stats(
                    resources.resources.backend,
                    &mut stats,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let mut report = AllocationProof {
            tensors: 0,
            bytes: 0,
            cleanup: false,
            failure: None,
        };
        for &index in indices {
            let tensor =
                unsafe { raw::yvex_tensor_table_at(self.native.table, index as u64).as_ref() }
                    .ok_or_else(extent_error)?;
            if tensor.rank as usize > tensor.dims.len() {
                return Err(extent_error());
            }
            let descriptor = raw::yvex_backend_tensor_desc {
                name: tensor.name,
                dtype: tensor.dtype,
                rank: tensor.rank,
                dims: tensor.dims,
                bytes: tensor.storage_bytes,
            };
            let mut allocated = std::ptr::null_mut();
            let status = unsafe {
                raw::yvex_backend_tensor_alloc(
                    resources.resources.backend,
                    &descriptor,
                    &mut allocated,
                    &mut failure,
                )
            };
            if !allocated.is_null() {
                resources.tensors.push(allocated);
            }
            if status != 0 {
                report.failure = Some(error(status, &failure));
                break;
            }
            if allocated.is_null() {
                report.failure = Some(extent_error());
                break;
            }
            report.tensors += 1;
            report.bytes = report
                .bytes
                .checked_add(tensor.storage_bytes)
                .ok_or_else(extent_error)?;
        }
        match resources
            .retire()
            .and_then(|()| resources.resources.retire(stats.allocated_bytes))
        {
            Ok(()) => report.cleanup = true,
            Err(failure) => report.failure = Some(failure),
        }
        Ok(report)
    }

    pub(crate) fn materialize(
        &self,
        backend: &str,
        require_all: bool,
        allow_unsupported: bool,
    ) -> Result<Materialization, Error> {
        let name = argument(Some(backend))?.unwrap();
        let mut resources = WeightResources::open(backend)?;
        let mut stats = raw::yvex_backend_memory_stats::default();
        let mut failure = raw::yvex_error::default();
        let status = unsafe {
            raw::yvex_backend_get_memory_stats(resources.backend, &mut stats, &mut failure)
        };
        if status != 0 {
            return Err(error(status, &failure));
        }
        let options = raw::yvex_materialize_options {
            backend_name: name.as_ptr(),
            require_all_tensors: require_all.into(),
            allow_unsupported_dtype: allow_unsupported.into(),
        };
        let mut summary = raw::yvex_materialize_summary::default();
        let status = unsafe {
            raw::yvex_weight_table_materialize_report(
                &mut resources.weights,
                self.native.artifact,
                self.native.gguf,
                self.native.table,
                resources.backend,
                &options,
                &mut summary,
                &mut failure,
            )
        };
        let mut failure = (status != 0).then(|| error(status, &failure));
        let retirement = resources.retire(stats.allocated_bytes);
        let resource_retirement = if retirement.is_ok() { "pass" } else { "fail" }.into();
        if let Err(cleanup) = retirement {
            failure = Some(cleanup);
        }
        // All native strings are static or borrowed from the retained request, not the table.
        let facts = unsafe {
            MaterializeFacts {
                materialization_status: borrowed_text(raw::yvex_weight_status_name(
                    summary.status,
                ))?,
                materialization_gate: optional_text(summary.materialization_gate)?,
                materialization_phase: optional_text(summary.materialization_phase)?,
                shape_status: optional_text(summary.shape_status)?,
                range_status: optional_text(summary.range_status)?,
                backend_status: optional_text(summary.backend_status)?,
                cleanup_status: optional_text(summary.cleanup_status)?,
                allocation_attempted: summary.allocation_attempted != 0,
                transfer_attempted: summary.transfer_attempted != 0,
                cleanup_attempted: summary.cleanup_attempted != 0,
                tensors_total: summary.tensors_total,
                tensors_materialized: summary.tensors_materialized,
                tensors_failed: summary.tensors_failed,
                bytes_total: summary.bytes_total,
                bytes_materialized: summary.bytes_materialized,
                backend_allocated_bytes: summary.backend_allocated_bytes,
                bytes_planned: summary.bytes_planned,
                bytes_allocated: summary.bytes_allocated,
                bytes_transferred: summary.bytes_transferred,
                execution_ready: summary.execution_ready != 0,
                resource_retirement,
            }
        };
        Ok(Materialization {
            facts,
            failure,
            complete: summary.status == raw::yvex_weight_status_YVEX_WEIGHT_STATUS_MATERIALIZED,
        })
    }
}

unsafe fn optional_text(value: *const std::ffi::c_char) -> Result<String, Error> {
    if value.is_null() {
        Ok(String::new())
    } else {
        unsafe { borrowed_text(value) }
    }
}

pub(crate) struct GateExpected<'a> {
    pub name: &'a str,
    pub dtype: &'a str,
    pub dims: &'a [u64],
    pub bytes: u64,
}

pub(crate) struct GateRequest<'a> {
    pub path: &'a str,
    pub label: &'a str,
    pub family: &'a str,
    pub digest: Option<&'a str>,
    pub metadata: &'a str,
    pub scope: raw::yvex_materialize_scope,
    pub expected: Option<GateExpected<'a>>,
    pub cpu: bool,
    pub cuda: bool,
    pub require_cpu: bool,
    pub require_cuda: bool,
    pub repeat: u32,
    pub cleanup: bool,
    pub materialization: bool,
}

pub(crate) struct GateResult {
    pub fields: std::collections::BTreeMap<String, serde_json::Value>,
    pub failure: Option<Error>,
    pub passed: bool,
}

pub(crate) fn artifact_gate(request: GateRequest<'_>) -> Result<GateResult, Error> {
    let strings = [
        Some(request.path),
        Some(request.label),
        Some(request.family),
        request.digest,
        Some(request.metadata),
        request.expected.as_ref().map(|expected| expected.name),
        request.expected.as_ref().map(|expected| expected.dtype),
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let mut expected = raw::yvex_materialize_expected_tensor::default();
    if let Some(spec) = request.expected.as_ref() {
        if spec.dims.is_empty() || spec.dims.len() > expected.dims.len() {
            return Err(extent_error());
        }
        expected.name = pointer(&strings[5]);
        expected.dtype = pointer(&strings[6]);
        expected.rank = spec.dims.len() as u32;
        expected.dims[..spec.dims.len()].copy_from_slice(spec.dims);
        expected.bytes = spec.bytes;
    }
    let options = raw::yvex_materialize_gate_options {
        model_path: pointer(&strings[0]),
        label: pointer(&strings[1]),
        family: pointer(&strings[2]),
        sha256: pointer(&strings[3]),
        metadata_status: pointer(&strings[4]),
        scope: request.scope,
        expected_tensors: if request.expected.is_some() {
            &expected
        } else {
            std::ptr::null()
        },
        expected_tensor_count: request.expected.is_some().into(),
        check_cpu: request.cpu.into(),
        check_cuda: request.cuda.into(),
        require_cpu: request.require_cpu.into(),
        require_cuda: request.require_cuda.into(),
        repeat_count: request.repeat,
        check_cleanup: request.cleanup.into(),
        ..Default::default()
    };
    let mut result = if request.materialization {
        materialization_gate(&options)?
    } else {
        model_gate(&options, &expected)?
    };
    if let Some(spec) = request.expected {
        for (name, value) in [
            ("expected_tensor", serde_json::json!(spec.name)),
            ("expected_rank", serde_json::json!(spec.dims.len())),
            ("expected_dims", serde_json::json!(spec.dims)),
            ("expected_dtype", serde_json::json!(spec.dtype)),
            ("expected_bytes", serde_json::json!(spec.bytes)),
        ] {
            result.fields.insert(name.into(), value);
        }
    }
    Ok(result)
}

fn materialization_gate(options: &raw::yvex_materialize_gate_options) -> Result<GateResult, Error> {
    let mut summary = raw::yvex_materialize_gate_summary::default();
    let mut failure = raw::yvex_error::default();
    let code = unsafe { raw::yvex_materialize_gate_check(options, &mut summary, &mut failure) };
    let mut fields = std::collections::BTreeMap::new();
    for (name, value) in [
        ("label", summary.label),
        ("family", summary.family),
        ("model", summary.model_path),
        ("expected_sha256", summary.expected_sha256),
        ("digest_status", summary.digest_status),
        ("identity_status", summary.identity_status),
        ("metadata_status", summary.metadata_status),
        ("materialization_gate", summary.materialization_gate),
        ("materialization_phase", summary.materialization_phase),
        ("integrity_status", summary.integrity_status),
        ("shape_status", summary.shape_status),
        ("range_status", summary.range_status),
        ("backend_status", summary.backend_status),
        ("cleanup_status", summary.cleanup_status),
    ] {
        fields.insert(
            name.into(),
            serde_json::json!(unsafe { optional_text(value) }?),
        );
    }
    for (name, value) in [
        ("file_bytes", summary.file_bytes),
        ("tensor_count", summary.tensor_count),
        ("expected_tensor_matches", summary.expected_tensor_matches),
        (
            "expected_tensor_mismatches",
            summary.expected_tensor_mismatches,
        ),
        ("bytes_materialized_cpu", summary.bytes_materialized_cpu),
        ("bytes_materialized_cuda", summary.bytes_materialized_cuda),
        ("bytes_planned", summary.bytes_planned),
        ("bytes_allocated", summary.bytes_allocated),
        ("bytes_transferred", summary.bytes_transferred),
        ("repeat_count", summary.repeat_count.into()),
    ] {
        fields.insert(name.into(), serde_json::json!(value));
    }
    for (name, value) in [
        ("allocation_attempted", summary.allocation_attempted),
        ("transfer_attempted", summary.transfer_attempted),
        ("cleanup_attempted", summary.cleanup_attempted),
        ("cleanup_verified", summary.cleanup_verified),
        ("execution_ready", summary.execution_ready),
    ] {
        fields.insert(name.into(), serde_json::json!(value != 0));
    }
    for (name, value) in unsafe {
        [
            ("scope", raw::yvex_materialize_scope_name(summary.scope)),
            (
                "failure_class",
                raw::yvex_materialize_failure_class_name(summary.failure_class),
            ),
            (
                "cpu",
                raw::yvex_materialize_backend_status_name(summary.cpu_status),
            ),
            (
                "cuda",
                raw::yvex_materialize_backend_status_name(summary.cuda_status),
            ),
            (
                "status",
                raw::yvex_materialize_gate_status_name(summary.status),
            ),
        ]
    } {
        fields.insert(
            name.into(),
            serde_json::json!(unsafe { borrowed_text(value) }?),
        );
    }
    fields.insert(
        "actual_sha256".into(),
        serde_json::json!(super::text(&summary.actual_sha256)),
    );
    Ok(GateResult {
        fields,
        failure: (code != 0).then(|| error(code, &failure)),
        passed: code == 0
            && summary.status == raw::yvex_materialize_gate_status_YVEX_MATERIALIZE_GATE_PASS,
    })
}

fn model_gate(
    options: &raw::yvex_materialize_gate_options,
    expected: &raw::yvex_materialize_expected_tensor,
) -> Result<GateResult, Error> {
    let expected = raw::yvex_model_gate_expected_tensor {
        name: expected.name,
        dtype: expected.dtype,
        rank: expected.rank,
        dims: expected.dims,
        bytes: expected.bytes,
    };
    let options = raw::yvex_model_gate_options {
        model_path: options.model_path,
        model_label: options.label,
        family: options.family,
        artifact_sha256: options.sha256,
        expected_tensors: &expected,
        expected_tensor_count: 1,
        check_cpu: options.check_cpu,
        check_cuda: options.check_cuda,
        require_cpu: options.require_cpu,
        require_cuda: options.require_cuda,
    };
    let mut summary = raw::yvex_model_gate_summary::default();
    let mut failure = raw::yvex_error::default();
    let code = unsafe { raw::yvex_model_gate_check(&options, &mut summary, &mut failure) };
    let mut fields = std::collections::BTreeMap::new();
    for (name, value) in [
        ("label", summary.model_label),
        ("family", summary.family),
        ("model", summary.model_path),
        ("expected_sha256", summary.expected_sha256),
        ("digest_status", summary.digest_status),
        ("identity_status", summary.identity_status),
    ] {
        fields.insert(
            name.into(),
            serde_json::json!(unsafe { optional_text(value) }?),
        );
    }
    for (name, value) in [
        ("file_bytes", summary.file_bytes),
        ("tensor_count", summary.tensor_count),
        ("expected_tensor_matches", summary.expected_tensor_matches),
        (
            "expected_tensor_mismatches",
            summary.expected_tensor_mismatches,
        ),
    ] {
        fields.insert(name.into(), serde_json::json!(value));
    }
    for (name, value) in unsafe {
        [
            (
                "support_level",
                raw::yvex_model_support_level_name(summary.support_level),
            ),
            (
                "cpu",
                raw::yvex_model_gate_backend_status_name(summary.cpu_status),
            ),
            (
                "cuda",
                raw::yvex_model_gate_backend_status_name(summary.cuda_status),
            ),
            ("status", raw::yvex_model_gate_status_name(summary.status)),
        ]
    } {
        fields.insert(
            name.into(),
            serde_json::json!(unsafe { borrowed_text(value) }?),
        );
    }
    fields.insert(
        "actual_sha256".into(),
        serde_json::json!(super::text(&summary.actual_sha256)),
    );
    fields.insert(
        "execution_ready".into(),
        serde_json::json!(summary.execution_ready != 0),
    );
    Ok(GateResult {
        fields,
        failure: (code != 0).then(|| error(code, &failure)),
        passed: code == 0 && summary.status == raw::yvex_model_gate_status_YVEX_MODEL_GATE_PASS,
    })
}

pub(crate) struct ConversionRequest<'a> {
    pub architecture: &'a str,
    pub source: &'a str,
    pub manifest: Option<&'a str>,
    pub template: Option<&'a str>,
    pub policy: Option<&'a str>,
    pub imatrix: Option<&'a str>,
    pub out: Option<&'a str>,
    pub plan: Option<&'a str>,
    pub tensor: Option<&'a str>,
    pub qtype: Option<&'a str>,
    pub limit: u64,
    pub overwrite: bool,
    pub allow_unsupported: bool,
    pub require_all: bool,
}

#[derive(serde::Serialize)]
pub(crate) struct ConversionReport {
    pub native_tensors: u64,
    pub planned_tensors: u64,
    pub emitted_tensors: u64,
    pub skipped_tensors: u64,
    pub unmapped_tensors: u64,
    pub unsupported_qtypes: u64,
    pub bytes_read: u64,
    pub bytes_written: u64,
    pub roundtrip_validated: bool,
    pub execution_ready: bool,
}

pub(crate) fn artifact_convert(request: ConversionRequest<'_>) -> Result<ConversionReport, Error> {
    let strings = [
        Some(request.architecture),
        Some(request.source),
        request.manifest,
        request.template,
        request.policy,
        request.imatrix,
        request.out,
        request.tensor,
        request.qtype,
        request.plan,
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let options = raw::yvex_conversion_options {
        architecture: pointer(&strings[0]),
        native_source_dir: pointer(&strings[1]),
        source_manifest_path: pointer(&strings[2]),
        template_path: pointer(&strings[3]),
        quant_policy_path: pointer(&strings[4]),
        imatrix_manifest_path: pointer(&strings[5]),
        out_path: pointer(&strings[6]),
        tensor_name: pointer(&strings[7]),
        target_qtype: pointer(&strings[8]),
        limit_tensors: request.limit,
        plan_only: request.plan.is_some().into(),
        overwrite: request.overwrite.into(),
        allow_unsupported_qtype: request.allow_unsupported.into(),
        require_all: request.require_all.into(),
    };
    let mut summary = raw::yvex_conversion_summary::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe {
        if request.plan.is_some() {
            raw::yvex_conversion_plan_write_json(
                &options,
                pointer(&strings[9]),
                &mut summary,
                &mut failure,
            )
        } else {
            raw::yvex_conversion_emit_gguf(&options, &mut summary, &mut failure)
        }
    };
    if status != 0 {
        return Err(error(status, &failure));
    }
    Ok(ConversionReport {
        native_tensors: summary.native_tensor_count,
        planned_tensors: summary.planned_tensor_count,
        emitted_tensors: summary.emitted_tensor_count,
        skipped_tensors: summary.skipped_tensor_count,
        unmapped_tensors: summary.unmapped_tensor_count,
        unsupported_qtypes: summary.unsupported_qtype_count,
        bytes_read: summary.bytes_read,
        bytes_written: summary.bytes_written,
        roundtrip_validated: summary.roundtrip_validated != 0,
        execution_ready: summary.execution_ready != 0,
    })
}

pub(crate) struct QtypeSupport {
    pub name: String,
    pub policy: bool,
    pub storage: bool,
    pub emit: bool,
    pub quantize: Option<bool>,
    pub compute: bool,
    pub notes: String,
}

pub(crate) fn qtype_support() -> Result<Vec<QtypeSupport>, Error> {
    let count = unsafe { raw::yvex_qtype_support_count() };
    let mut rows = Vec::new();
    for index in 0..count {
        let row = unsafe { raw::yvex_qtype_support_at(index).as_ref() }.ok_or_else(extent_error)?;
        unsafe {
            rows.push(QtypeSupport {
                name: borrowed_text(raw::yvex_qtype_support_name(row))?,
                policy: row.policy_supported != 0,
                storage: raw::yvex_qtype_support_storage_supported(row) != 0,
                emit: row.emit_supported != 0,
                quantize: if row.ggml_type == raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_F32 {
                    None
                } else {
                    Some(row.quantize_supported != 0)
                },
                compute: row.compute_supported != 0,
                notes: optional_text(row.notes)?,
            });
        }
    }
    Ok(rows)
}

pub(crate) struct EmitRequest<'a> {
    pub out: &'a str,
    pub template: Option<&'a str>,
    pub source: Option<&'a str>,
    pub tensor: &'a str,
    pub target: &'a str,
    pub qtype: Option<&'a str>,
    pub name: &'a str,
    pub architecture: &'a str,
    pub overwrite: bool,
}

pub(crate) struct ArtifactEmission {
    pub out: String,
    pub architecture: String,
    pub name: String,
    pub status: String,
    pub metadata_count: u64,
    pub tensor_count: u64,
    pub payload_bytes: u64,
    pub bytes_written: u64,
    pub alignment: u64,
    pub roundtrip_validated: bool,
}

pub(crate) fn artifact_emit(request: EmitRequest<'_>) -> Result<ArtifactEmission, Error> {
    let strings = [
        Some(request.out),
        request.template,
        request.source,
        Some(request.tensor),
        Some(request.target),
        request.qtype,
        Some(request.name),
        Some(request.architecture),
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let options = raw::yvex_gguf_emit_options {
        out_path: pointer(&strings[0]),
        template_path: pointer(&strings[1]),
        native_source_dir: pointer(&strings[2]),
        tensor_name: pointer(&strings[3]),
        target_name: pointer(&strings[4]),
        target_qtype: pointer(&strings[5]),
        model_name: pointer(&strings[6]),
        architecture: pointer(&strings[7]),
        transpose_2d: 1,
        overwrite: request.overwrite.into(),
    };
    let mut summary = raw::yvex_gguf_emit_summary::default();
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_gguf_emit_controlled(&options, &mut summary, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    // Summary strings can borrow request values; copy before they leave scope.
    unsafe {
        Ok(ArtifactEmission {
            out: borrowed_text(summary.out_path)?,
            architecture: borrowed_text(summary.architecture)?,
            name: borrowed_text(summary.model_name)?,
            status: borrowed_text(raw::yvex_gguf_emit_status_name(summary.status))?,
            metadata_count: summary.metadata_count,
            tensor_count: summary.tensor_count,
            payload_bytes: summary.tensor_payload_bytes,
            bytes_written: summary.bytes_written,
            alignment: summary.alignment,
            roundtrip_validated: summary.roundtrip_validated != 0,
        })
    }
}

pub(crate) struct TemplateIssue {
    pub kind: String,
    pub tensor: String,
    pub message: String,
}

pub(crate) struct TemplateReport {
    pub architecture: String,
    pub name: String,
    pub status: String,
    pub metadata_count: u64,
    pub tensor_count: u64,
    pub known_roles: u64,
    pub unknown_roles: u64,
    pub native_tensor_count: u64,
    pub matched: u64,
    pub missing: u64,
    pub mismatched: u64,
    pub has_tokenizer: bool,
    pub issues: Vec<TemplateIssue>,
}
struct Template(*mut raw::yvex_gguf_template);
impl Drop for Template {
    fn drop(&mut self) {
        unsafe {
            raw::yvex_gguf_template_close(self.0);
        }
    }
}

pub(crate) struct MappingRequest<'a> {
    pub architecture: &'a str,
    pub source: &'a str,
    pub template: Option<&'a str>,
    pub require_native: bool,
    pub require_template: bool,
}

pub(crate) struct MappingRow {
    pub native: String,
    pub target: String,
    pub role: String,
    pub status: String,
    pub issue: String,
    pub native_dims: Vec<u64>,
    pub target_dims: Vec<u64>,
    pub transpose: bool,
    pub mapped: bool,
    pub shape_mismatch: bool,
}

struct Mapping(*mut raw::yvex_weight_mapping_table);
impl Drop for Mapping {
    fn drop(&mut self) {
        unsafe { raw::yvex_weight_mapping_table_close(self.0) };
    }
}

pub(crate) fn tensor_mapping(request: MappingRequest<'_>) -> Result<Vec<MappingRow>, Error> {
    let architecture = argument(Some(request.architecture))?.expect("required architecture");
    let source = argument(Some(request.source))?.expect("required native source");
    let template = argument(request.template)?;
    let options = raw::yvex_weight_mapping_options {
        architecture: architecture.as_ptr(),
        native_source_dir: source.as_ptr(),
        template_path: pointer(&template),
        compare_template: template.is_some().into(),
        require_all_native_mapped: request.require_native.into(),
        require_all_template_matched: request.require_template.into(),
    };
    let mut table = Mapping(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status =
        unsafe { raw::yvex_weight_mapping_table_build(&mut table.0, &options, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let count = unsafe { raw::yvex_weight_mapping_table_count(table.0) };
    let mut rows = Vec::new();
    for index in 0..count {
        let row = unsafe { raw::yvex_weight_mapping_table_at(table.0, index).as_ref() }
            .ok_or_else(extent_error)?;
        let native = row
            .native_dims
            .get(..row.native_rank as usize)
            .ok_or_else(extent_error)?;
        let target = row
            .target_dims
            .get(..row.target_rank as usize)
            .ok_or_else(extent_error)?;
        unsafe {
            rows.push(MappingRow {
                native: borrowed_text(row.native_name)?,
                target: optional_text(row.target_name)?,
                role: borrowed_text(raw::yvex_tensor_role_name(row.role))?,
                status: borrowed_text(raw::yvex_weight_mapping_status_name(row.status))?,
                issue: borrowed_text(raw::yvex_weight_mapping_issue_kind_name(row.issue))?,
                native_dims: native.to_vec(),
                target_dims: target.to_vec(),
                transpose: row.requires_transpose != 0,
                mapped: row.status
                    == raw::yvex_weight_mapping_status_YVEX_WEIGHT_MAPPING_STATUS_MAPPED,
                shape_mismatch: row.status
                    == raw::yvex_weight_mapping_status_YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH,
            });
        }
    }
    Ok(rows)
}

pub(crate) fn artifact_template(
    path: &str,
    source: Option<&str>,
    compare: bool,
    require_all: bool,
) -> Result<TemplateReport, Error> {
    let path = argument(Some(path))?.expect("required template");
    let source = argument(source)?;
    let options = raw::yvex_gguf_template_options {
        template_path: path.as_ptr(),
        native_source_dir: pointer(&source),
        compare_native: compare.into(),
        require_tokenizer: 0,
        require_all_template_tensors_in_native: require_all.into(),
    };
    let mut template = Template(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    let status = unsafe { raw::yvex_gguf_template_open(&mut template.0, &options, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let mut summary = raw::yvex_gguf_template_summary::default();
    let status =
        unsafe { raw::yvex_gguf_template_get_summary(template.0, &mut summary, &mut failure) };
    if status != 0 {
        return Err(error(status, &failure));
    }
    let count = unsafe { raw::yvex_gguf_template_issue_count(template.0) };
    if count != summary.issue_count {
        return Err(extent_error());
    }
    let mut issues = Vec::new();
    for index in 0..count {
        let issue = unsafe { raw::yvex_gguf_template_issue_at(template.0, index).as_ref() }
            .ok_or_else(extent_error)?;
        unsafe {
            issues.push(TemplateIssue {
                kind: borrowed_text(raw::yvex_gguf_template_issue_kind_name(issue.kind))?,
                tensor: optional_text(issue.tensor_name)?,
                message: optional_text(issue.message)?,
            });
        }
    }
    unsafe {
        Ok(TemplateReport {
            architecture: optional_text(summary.architecture)?,
            name: optional_text(summary.model_name)?,
            status: borrowed_text(raw::yvex_gguf_template_status_name(summary.status))?,
            metadata_count: summary.metadata_count,
            tensor_count: summary.tensor_count,
            known_roles: summary.known_role_count,
            unknown_roles: summary.unknown_role_count,
            native_tensor_count: summary.native_tensor_count,
            matched: summary.matched_exact,
            missing: summary.missing_in_native,
            mismatched: summary.shape_mismatch,
            has_tokenizer: summary.has_tokenizer != 0,
            issues,
        })
    }
}
