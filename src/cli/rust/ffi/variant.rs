// Physical compilation stays behind its admitted native session/vtable and sealed plans.
use super::{Error, argument, error, extent_error, raw, text};
use std::ffi::CString;

pub(crate) struct VariantRequest<'a> {
    pub target: &'a str,
    pub source: &'a str,
    pub models_root: Option<&'a str>,
    pub manifest: Option<&'a str>,
    pub preset: Option<&'a str>,
    pub policy: Option<&'a str>,
    pub imatrix: Option<&'a str>,
    pub backend: Option<&'a str>,
    pub component: Option<&'a str>,
}

pub(crate) struct Variant {
    api: raw::yvex_physical_variant_api,
    session: *mut raw::yvex_physical_variant_session,
    _inputs: Vec<Option<CString>>,
}

impl Drop for Variant {
    fn drop(&mut self) {
        // Input storage survives the native close even if the producer borrows it.
        unsafe { self.api.close.expect("admitted close")(&mut self.session) };
    }
}

fn checked(status: i32, failure: &raw::yvex_error) -> Result<(), Error> {
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, failure))
    }
}

fn refusal(code: i32, message: &str) -> Error {
    Error {
        code,
        owner: "physical.variant".into(),
        message: message.into(),
    }
}

impl Variant {
    pub(crate) fn open(request: VariantRequest<'_>) -> Result<Self, Error> {
        let inputs = [
            Some(request.target),
            Some(request.source),
            request.models_root,
            request.manifest,
            request.preset,
            request.policy,
            request.imatrix,
            request.backend,
            request.component,
        ]
        .into_iter()
        .map(argument)
        .collect::<Result<Vec<_>, _>>()?;
        let pointer = |index: usize| {
            inputs[index]
                .as_ref()
                .map_or(std::ptr::null(), |value| value.as_ptr())
        };
        let native = raw::yvex_physical_variant_request {
            target_id: pointer(0),
            source_path: pointer(1),
            models_root: pointer(2),
            source_manifest_path: pointer(3),
            quant_preset_name: pointer(4),
            quant_policy_path: pointer(5),
            imatrix_path: pointer(6),
            backend: pointer(7),
            component_id: pointer(8),
            worker_count: if request.component.is_some() { 1 } else { 16 },
            quant_policy: std::ptr::null(),
        };
        let execution = unsafe { raw::yvex_graph_execution_find(0, 0, native.target_id).as_ref() };
        let api = if let Some(execution) = execution {
            unsafe { execution.compiler.as_ref() }
                .filter(|compiler| compiler.schema_version == raw::YVEX_FAMILY_COMPILER_SCHEMA_V3)
                .and_then(|compiler| compiler.physical_variant)
        } else {
            unsafe { raw::yvex_graph_component_variant_find(native.target_id).as_ref() }
                .and_then(|component| component.physical_variant)
        };
        let api = api
            .and_then(|entry| unsafe { entry().as_ref() })
            .ok_or_else(|| {
                refusal(
                    raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                    "target has no physical-variant compiler adapter",
                )
            })?;
        if api.schema_version != raw::YVEX_PHYSICAL_VARIANT_API_SCHEMA_V2
            || api.open.is_none()
            || api.close.is_none()
            || api.view.is_none()
        {
            return Err(refusal(
                raw::yvex_status_YVEX_ERR_STATE,
                "target published an invalid physical-variant compiler API",
            ));
        }
        let mut context = Self {
            api: *api,
            session: std::ptr::null_mut(),
            _inputs: inputs,
        };
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { context.api.open.unwrap()(&mut context.session, &native, &mut failure) },
            &failure,
        )?;
        context.view()?;
        Ok(context)
    }

    fn view(&self) -> Result<&raw::yvex_physical_variant_view, Error> {
        let view =
            unsafe { self.api.view.unwrap()(self.session).as_ref() }.ok_or_else(extent_error)?;
        if view.summary.is_null() || view.plan.is_null() || view.writer.is_null() {
            return Err(refusal(
                raw::yvex_status_YVEX_ERR_STATE,
                "physical-variant session did not publish immutable plans",
            ));
        }
        Ok(view)
    }

    pub(crate) fn report(&self) -> Result<VariantReport, Error> {
        let view = self.view()?;
        let source = unsafe { *view.summary };
        let plan = unsafe { raw::yvex_quant_plan_summary_get(view.plan).as_ref() }
            .ok_or_else(extent_error)?;
        let writer = unsafe { raw::yvex_gguf_writer_plan_summary_get(view.writer).as_ref() }
            .ok_or_else(extent_error)?;
        let count = usize::try_from(plan.decision_count).map_err(|_| extent_error())?;
        let mut decisions = Vec::new();
        decisions.try_reserve_exact(count).map_err(|_| {
            refusal(
                raw::yvex_status_YVEX_ERR_NOMEM,
                "decision projection allocation failed",
            )
        })?;
        for index in 0..plan.decision_count {
            let decision = unsafe { raw::yvex_quant_plan_decision_at(view.plan, index).as_ref() }
                .ok_or_else(extent_error)?;
            decisions.push(*decision);
        }
        Ok(VariantReport {
            source,
            plan: *plan,
            writer: *writer,
            decisions,
        })
    }

    pub(crate) fn write_plan(&self, path: &str) -> Result<(), Error> {
        let path = argument(Some(path))?.expect("required plan destination");
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_quant_plan_file_write(path.as_ptr(), self.view()?.plan, &mut failure)
            },
            &failure,
        )
    }

    pub(crate) fn validate_plan(&self, path: &str) -> Result<(), Error> {
        let path = argument(Some(path))?.expect("required sealed plan");
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_quant_plan_file_validate(path.as_ptr(), self.view()?.plan, &mut failure)
            },
            &failure,
        )
    }

    pub(crate) fn probe(&self, path: &str, tensor: &str) -> Result<VariantProbe, Error> {
        self.validate_plan(path)?;
        let view = self.view()?;
        let summary = unsafe { raw::yvex_quant_plan_summary_get(view.plan).as_ref() }
            .ok_or_else(extent_error)?;
        let mut selected = None;
        for ordinal in 0..summary.decision_count {
            let decision = unsafe { raw::yvex_quant_plan_decision_at(view.plan, ordinal).as_ref() }
                .ok_or_else(extent_error)?;
            if text(&decision.physical_tensor_name) == tensor {
                if selected.is_some() {
                    return Err(refusal(
                        raw::yvex_status_YVEX_ERR_FORMAT,
                        "probe tensor selector is not unique",
                    ));
                }
                selected = Some(decision);
            }
        }
        let selected = selected.ok_or_else(|| {
            refusal(
                raw::yvex_status_YVEX_ERR_FORMAT,
                "probe tensor is absent from the sealed physical plan",
            )
        })?;
        let mut digest = Digest(std::ptr::null_mut());
        let mut quant_failure = raw::yvex_quant_failure::default();
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_quant_digest_sink_create(
                    &mut digest.0,
                    view.plan,
                    summary.required_payload_identity.as_ptr(),
                    &mut quant_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let mut sink = raw::yvex_quant_output_sink::default();
        let mut options = raw::yvex_quant_executor_options::default();
        let mut execution = Box::<raw::yvex_quant_execution_summary>::default();
        unsafe {
            raw::yvex_quant_digest_sink_adapter(digest.0, &mut sink);
            raw::yvex_quant_executor_options_default(&mut options);
        }
        options.worker_count = 1;
        options.maximum_owned_bytes = 64 * 1024 * 1024;
        options.first_terminal = selected.terminal_ordinal;
        options.terminal_count = 1;
        options.imatrix = view.imatrix;
        let start = std::time::Instant::now();
        checked(
            unsafe {
                raw::yvex_quant_execute(
                    view.plan,
                    &sink,
                    &options,
                    execution.as_mut(),
                    &mut quant_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let seconds = start.elapsed().as_secs_f64();
        let metrics = *execution
            .role_metrics
            .get(selected.role as usize)
            .ok_or_else(extent_error)?;
        Ok(VariantProbe {
            decision: *selected,
            metrics,
            execution,
            seconds,
        })
    }

    pub(crate) fn emit(
        &self,
        path: &str,
        destination: &str,
        binding_directory: Option<&str>,
    ) -> Result<VariantEmission, Error> {
        self.validate_plan(path)?;
        let view = self.view()?;
        if binding_directory.is_some() {
            let target = self._inputs[0].as_ref().ok_or_else(extent_error)?;
            let execution =
                unsafe { raw::yvex_graph_execution_find(0, 0, target.as_ptr()).as_ref() }
                    .ok_or_else(extent_error)?;
            let compiler = unsafe { execution.compiler.as_ref() }.ok_or_else(extent_error)?;
            if self._inputs[8].is_some()
                || compiler.binding_pipeline.is_null()
                || compiler.binding_compile.is_none()
            {
                return Err(refusal(
                    raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                    "binding publication requires an admitted complete-model compiler",
                ));
            }
        }
        let destination = argument(Some(destination))?.expect("required artifact destination");
        let mut sink = FileSink(std::ptr::null_mut());
        let mut options = raw::yvex_gguf_file_sink_options::default();
        let mut file_failure = raw::yvex_gguf_file_failure::default();
        let mut failure = raw::yvex_error::default();
        unsafe { raw::yvex_gguf_file_sink_options_default(&mut options) };
        options.destination_path = destination.as_ptr();
        checked(
            unsafe {
                raw::yvex_gguf_file_sink_create(
                    &mut sink.0,
                    view.writer,
                    view.plan,
                    &options,
                    &mut file_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let mut output = raw::yvex_quant_output_sink::default();
        let mut executor = raw::yvex_quant_executor_options::default();
        let mut execution = Box::<raw::yvex_quant_execution_summary>::default();
        let mut quant_failure = raw::yvex_quant_failure::default();
        unsafe {
            raw::yvex_gguf_file_sink_adapter(sink.0, &mut output);
            raw::yvex_quant_executor_options_default(&mut executor);
        }
        executor.worker_count = unsafe { (*view.summary).worker_count };
        executor.maximum_owned_bytes = 64 * 1024 * 1024;
        executor.imatrix = view.imatrix;
        checked(
            unsafe {
                raw::yvex_quant_execute(
                    view.plan,
                    &output,
                    &executor,
                    execution.as_mut(),
                    &mut quant_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let mut emission = raw::yvex_gguf_file_sink_summary::default();
        checked(
            unsafe {
                raw::yvex_gguf_file_sink_finalize(
                    sink.0,
                    &mut emission,
                    &mut file_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let mut roundtrip_options = raw::yvex_gguf_roundtrip_options::default();
        let mut roundtrip = raw::yvex_gguf_roundtrip_summary::default();
        let mut roundtrip_failure = raw::yvex_gguf_roundtrip_failure::default();
        unsafe { raw::yvex_gguf_roundtrip_options_default(&mut roundtrip_options) };
        checked(
            unsafe {
                raw::yvex_gguf_roundtrip_validate(
                    raw::yvex_gguf_file_sink_temporary_path(sink.0),
                    view.writer,
                    raw::yvex_gguf_file_sink_digest(sink.0),
                    &roundtrip_options,
                    &mut roundtrip,
                    &mut roundtrip_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let official = if binding_directory.is_some() {
            Some(super::gguf_reference::verify(
                unsafe {
                    std::ffi::CStr::from_ptr(raw::yvex_gguf_file_sink_temporary_path(sink.0))
                },
                unsafe { raw::yvex_gguf_writer_plan_summary_get(view.writer).as_ref() }
                    .ok_or_else(extent_error)?,
            )?)
        } else {
            None
        };
        checked(
            unsafe {
                raw::yvex_gguf_file_sink_publish(
                    sink.0,
                    &roundtrip,
                    &mut emission,
                    &mut file_failure,
                    &mut failure,
                )
            },
            &failure,
        )?;
        let binding = binding_directory.map(|directory| {
            self.publish_binding(
                path,
                destination.as_c_str(),
                directory,
                &emission,
                &roundtrip,
                official
                    .as_ref()
                    .expect("verification precedes publication"),
            )
        });
        Ok(VariantEmission {
            emission,
            roundtrip,
            binding,
        })
    }

    fn publish_binding(
        &self,
        plan: &str,
        artifact: &std::ffi::CStr,
        directory: &str,
        emission: &raw::yvex_gguf_file_sink_summary,
        roundtrip: &raw::yvex_gguf_roundtrip_summary,
        official: &raw::yvex_artifact_official_reader_fact,
    ) -> BindingPublication {
        let result = (|| {
            let plan = argument(Some(plan))?.ok_or_else(extent_error)?;
            let directory = argument(Some(directory))?.ok_or_else(extent_error)?;
            let pointer = |i: usize| {
                self._inputs[i]
                    .as_ref()
                    .map_or(std::ptr::null(), |v| v.as_ptr())
            };
            let execution = unsafe { raw::yvex_graph_execution_find(0, 0, pointer(0)).as_ref() }
                .ok_or_else(extent_error)?;
            let production = raw::yvex_artifact_admission_request {
                artifact_path: artifact.as_ptr(),
                writer_plan: self.view()?.writer,
                emission,
                native_roundtrip: roundtrip,
                official_reader: official,
            };
            let request = raw::yvex_compilation_runtime_binding_request {
                source_path: pointer(1),
                models_root: pointer(2),
                source_manifest_path: pointer(3),
                artifact_path: artifact.as_ptr(),
                directory: directory.as_ptr(),
                quant_preset_name: pointer(4),
                quant_policy_path: pointer(5),
                imatrix_path: pointer(6),
                physical_variant_plan_path: plan.as_ptr(),
                source_stream_count: 4,
                family_adapter_id: execution.adapter_id,
                family_adapter_version: execution.adapter_version,
                artifact_production: &production,
                ..Default::default()
            };
            let mut binding = [0; raw::YVEX_PATH_CAP as usize];
            let mut published = 0;
            let mut error = raw::yvex_error::default();
            checked(
                unsafe {
                    raw::yvex_runtime_binding_compile_publish(
                        execution.compiler,
                        &request,
                        binding.as_mut_ptr(),
                        &mut published,
                        &mut error,
                    )
                },
                &error,
            )?;
            Ok::<_, Error>((text(&binding), published != 0))
        })();
        match result {
            Ok((path, newly_published)) => BindingPublication {
                path: Some(path),
                newly_published,
                failure: None,
            },
            Err(failure) => BindingPublication {
                path: None,
                newly_published: false,
                failure: Some(failure),
            },
        }
    }
}

pub(crate) struct VariantReport {
    pub source: raw::yvex_physical_variant_summary,
    pub plan: raw::yvex_quant_plan_summary,
    pub writer: raw::yvex_gguf_writer_plan_summary,
    pub decisions: Vec<raw::yvex_quant_decision>,
}
pub(crate) struct VariantProbe {
    pub decision: raw::yvex_quant_decision,
    pub metrics: raw::yvex_quant_metrics,
    pub execution: Box<raw::yvex_quant_execution_summary>,
    pub seconds: f64,
}
pub(crate) struct VariantEmission {
    pub emission: raw::yvex_gguf_file_sink_summary,
    pub roundtrip: raw::yvex_gguf_roundtrip_summary,
    pub binding: Option<BindingPublication>,
}
pub(crate) struct BindingPublication {
    pub path: Option<String>,
    pub newly_published: bool,
    pub failure: Option<Error>,
}
struct Digest(*mut raw::yvex_quant_digest_sink);
impl Drop for Digest {
    fn drop(&mut self) {
        unsafe { raw::yvex_quant_digest_sink_release(&mut self.0) };
    }
}
struct FileSink(*mut raw::yvex_gguf_file_sink);
impl Drop for FileSink {
    fn drop(&mut self) {
        unsafe { raw::yvex_gguf_file_sink_release(&mut self.0) };
    }
}

pub(crate) struct OptimizationInput<'a> {
    pub target: &'a str,
    pub source: &'a str,
    pub models_root: &'a str,
    pub manifest: &'a str,
    pub policy: Option<&'a str>,
    pub imatrix: Option<&'a str>,
    pub select: Option<&'a str>,
    pub out_policy: Option<&'a str>,
    pub request: raw::yvex_optimization_request,
    pub weight_budget: Option<u64>,
    pub search_states: u32,
}

pub(crate) struct OptimizationReport {
    pub total_memory: u64,
    pub available_memory: u64,
    pub process_limited: bool,
    pub compute_major: u32,
    pub compute_minor: u32,
    pub rows: Vec<raw::yvex_optimization_candidate>,
    pub context: raw::yvex_optimization_context,
}

struct Optimization(*mut raw::yvex_optimization_search);
impl Drop for Optimization {
    fn drop(&mut self) {
        unsafe { raw::yvex_optimization_search_close(&mut self.0) };
    }
}

pub(crate) fn optimize(input: OptimizationInput<'_>) -> Result<OptimizationReport, Error> {
    let inputs = [
        Some(input.target),
        Some(input.source),
        Some(input.models_root),
        Some(input.manifest),
        input.policy,
        input.imatrix,
    ]
    .into_iter()
    .map(argument)
    .collect::<Result<Vec<_>, _>>()?;
    let pointer = |i: usize| inputs[i].as_ref().map_or(std::ptr::null(), |x| x.as_ptr());
    let mut request = input.request;
    request.target_id = pointer(0);
    request.source_path = pointer(1);
    request.models_root = pointer(2);
    request.source_manifest_path = pointer(3);
    request.policy_path = pointer(4);
    request.imatrix_path = pointer(5);
    let mut process_limited = 0;
    if unsafe {
        raw::yvex_runtime_private_memory_capacity(
            &mut request.system_memory_bytes,
            &mut request.available_memory_bytes,
            &mut process_limited,
        )
    } == 0
    {
        return Err(refusal(
            raw::yvex_status_YVEX_ERR_UNSUPPORTED,
            "system memory observation unavailable",
        ));
    }
    if request.backend == raw::yvex_backend_kind_YVEX_BACKEND_KIND_CUDA {
        let device = super::backend_report(&raw::yvex_backend_report_request {
            kind: raw::yvex_backend_report_kind_YVEX_BACKEND_REPORT_CUDA_INFO,
            backend_kind: request.backend,
        })?;
        if device.available == 0 || device.has_device_info == 0 {
            return Err(refusal(
                raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                "CUDA device facts unavailable",
            ));
        }
        request.compute_major = device
            .device_info
            .compute_capability_major
            .try_into()
            .map_err(|_| extent_error())?;
        request.compute_minor = device
            .device_info
            .compute_capability_minor
            .try_into()
            .map_err(|_| extent_error())?;
    }
    let mut search = Optimization(std::ptr::null_mut());
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_optimization_search_run(
                &mut search.0,
                &request,
                if input.weight_budget.is_some() {
                    c"source-retention-allocation-v1".as_ptr()
                } else {
                    c"fixed-recipes-v1".as_ptr()
                },
                input.weight_budget.unwrap_or(0),
                if input.weight_budget.is_some() {
                    input.search_states
                } else {
                    0
                },
                &mut failure,
            )
        },
        &failure,
    )?;
    let context = *unsafe { raw::yvex_optimization_search_context(search.0).as_ref() }
        .ok_or_else(extent_error)?;
    let mut rows = Vec::new();
    for i in 0..unsafe { raw::yvex_optimization_search_count(search.0) } {
        rows.push(
            *unsafe { raw::yvex_optimization_search_at(search.0, i).as_ref() }
                .ok_or_else(extent_error)?,
        );
    }
    if let (Some(identity), Some(destination)) = (input.select, input.out_policy) {
        let identity = argument(Some(identity))?.ok_or_else(extent_error)?;
        let destination = argument(Some(destination))?.ok_or_else(extent_error)?;
        let mut policy = std::ptr::null();
        checked(
            unsafe {
                raw::yvex_optimization_search_policy(
                    search.0,
                    identity.as_ptr(),
                    &mut policy,
                    &mut failure,
                )
            },
            &failure,
        )?;
        checked(
            unsafe {
                raw::yvex_quant_policy_write_json(destination.as_ptr(), policy, &mut failure)
            },
            &failure,
        )?;
    }
    Ok(OptimizationReport {
        total_memory: request.system_memory_bytes,
        available_memory: request.available_memory_bytes,
        process_limited: process_limited != 0,
        compute_major: request.compute_major,
        compute_minor: request.compute_minor,
        rows,
        context,
    })
}

pub(crate) fn optimization_techniques() -> Result<Vec<serde_json::Value>, Error> {
    let mut rows = Vec::new();
    for index in 0..unsafe { raw::yvex_optimization_technique_count() } {
        let method = unsafe { raw::yvex_optimization_technique_at(index).as_ref() }
            .ok_or_else(extent_error)?;
        if method.schema_version != raw::YVEX_OPTIMIZATION_TECHNIQUE_SCHEMA_V1 {
            return Err(extent_error());
        }
        rows.push(serde_json::json!({
            "identity": unsafe { super::borrowed_text(method.identity)? },
            "name": unsafe { super::borrowed_text(method.name)? },
            "inputs": unsafe { super::borrowed_text(method.inputs)? },
            "objective": unsafe { super::borrowed_text(method.objective)? },
            "unearned_evidence": unsafe { super::borrowed_text(method.unearned_evidence)? },
        }));
    }
    Ok(rows)
}

pub(crate) fn optimization_state(state: raw::yvex_optimization_state) -> String {
    unsafe { super::borrowed_text(raw::yvex_optimization_state_name(state)) }
        .unwrap_or_else(|_| "unknown".into())
}

pub(crate) fn optimization_priority(goal: raw::yvex_optimization_goal) -> String {
    unsafe { super::borrowed_text(raw::yvex_optimization_priority_basis(goal)) }
        .unwrap_or_else(|_| "unknown".into())
}

pub(crate) struct DeploymentAssessment {
    pub candidate: String,
    pub binding: String,
    pub artifact: String,
    pub status: i32,
    pub reason: String,
    pub required: u64,
    pub available: u64,
    pub plan: Option<raw::yvex_execution_capacity_plan>,
}

struct CapacityResources {
    binding: *mut raw::yvex_runtime_binding,
    backend: *mut raw::yvex_backend,
}
impl Drop for CapacityResources {
    fn drop(&mut self) {
        unsafe {
            if !self.backend.is_null() {
                let mut failure = raw::yvex_error::default();
                raw::yvex_backend_close_checked(&mut self.backend, &mut failure);
            }
            if !self.binding.is_null() {
                raw::yvex_runtime_binding_close(self.binding);
            }
        }
    }
}

/// Existing runtime capacity owner, without opening an engine or its weights.
pub(crate) fn deployment_assessment(
    path: &str,
    candidates: &[raw::yvex_optimization_candidate],
    request: &raw::yvex_optimization_request,
    speculative: bool,
) -> Result<DeploymentAssessment, Error> {
    let path = argument(Some(path))?.ok_or_else(extent_error)?;
    let mut resources = CapacityResources {
        binding: std::ptr::null_mut(),
        backend: std::ptr::null_mut(),
    };
    let mut summary = raw::yvex_runtime_binding_summary::default();
    let mut admission = raw::yvex_complete_artifact_admission::default();
    let mut binding_failure = raw::yvex_runtime_binding_failure::default();
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe {
            raw::yvex_runtime_binding_open(
                &mut resources.binding,
                path.as_ptr(),
                &mut summary,
                &mut admission,
                &mut binding_failure,
                &mut failure,
            )
        },
        &failure,
    )?;
    let candidate = candidates
        .iter()
        .find(|c| {
            c.failure_status == 0
                && text(&c.physical_variant_identity) == text(&summary.profile_identity)
                && text(&c.transform_identity) == text(&summary.logical_transform_identity)
        })
        .ok_or_else(|| {
            refusal(
                raw::yvex_status_YVEX_ERR_STATE,
                "binding does not match an exact compiled physical variant and transformation",
            )
        })?;
    let backend_options = raw::yvex_backend_options {
        kind: request.backend,
        ..Default::default()
    };
    checked(
        unsafe { raw::yvex_backend_open(&mut resources.backend, &backend_options, &mut failure) },
        &failure,
    )?;
    let options = raw::yvex_runtime_capacity_options {
        backend: request.backend,
        mode: if speculative {
            raw::yvex_execution_generation_mode_YVEX_EXECUTION_GENERATION_SPECULATIVE
        } else {
            raw::yvex_execution_generation_mode_YVEX_EXECUTION_GENERATION_TARGET_ONLY
        },
        context_capacity: request.context_tokens,
        prefill_chunk_tokens: request.prefill_tokens,
        concurrent_sequences: request.concurrent_sequences,
        maximum_host_bytes: request.memory_limit_bytes,
        maximum_device_bytes: request.memory_limit_bytes,
        ..Default::default()
    };
    let mut capacity = raw::yvex_runtime_capacity::default();
    let (mut required, mut available) = (0, 0);
    let status = unsafe {
        raw::yvex_runtime_capacity_preflight(
            resources.binding,
            resources.backend,
            &options,
            &mut required,
            &mut available,
            &mut capacity,
            &mut failure,
        )
    };
    let reason = if status == 0 {
        "capacity preflight passed; not residency, model execution or a reservation".into()
    } else {
        error(status, &failure).message
    };
    let plan =
        (!text(&capacity.capacity_plan.identity).is_empty()).then_some(capacity.capacity_plan);
    checked(
        unsafe { raw::yvex_backend_close_checked(&mut resources.backend, &mut failure) },
        &failure,
    )?;
    Ok(DeploymentAssessment {
        candidate: text(&candidate.candidate_identity),
        binding: text(&summary.identity),
        artifact: text(&summary.artifact_identity),
        status,
        reason,
        required,
        available,
        plan,
    })
}
