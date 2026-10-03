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
        };
        let execution = unsafe { raw::yvex_graph_execution_find(0, 0, native.target_id).as_ref() };
        let api = if let Some(execution) = execution {
            unsafe { execution.compiler.as_ref() }.and_then(|compiler| compiler.physical_variant)
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
        if api.schema_version != raw::YVEX_PHYSICAL_VARIANT_SESSION_SCHEMA_V1
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

    pub(crate) fn emit(&self, path: &str, destination: &str) -> Result<VariantEmission, Error> {
        self.validate_plan(path)?;
        let view = self.view()?;
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
        Ok(VariantEmission {
            emission,
            roundtrip,
        })
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
