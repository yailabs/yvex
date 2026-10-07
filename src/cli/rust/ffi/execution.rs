// Synchronous typed engineering operations retain native computational and cleanup ownership.
use super::{Error, argument, borrowed_text, error, raw, text};
use std::{
    ffi::CString,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
};

pub(crate) fn checked(status: i32, failure: &raw::yvex_error) -> Result<(), Error> {
    if status == 0 {
        Ok(())
    } else {
        Err(error(status, failure))
    }
}

pub(crate) fn copy_text<const N: usize>(destination: &mut [std::ffi::c_char; N], value: &str) {
    destination.fill(0);
    for (slot, byte) in destination
        .iter_mut()
        .take(N.saturating_sub(1))
        .zip(value.bytes())
    {
        *slot = byte as std::ffi::c_char;
    }
}

pub(crate) struct Family {
    target: CString,
    pub key: String,
    pub artifact: String,
    pub manifest: String,
    pub name: String,
}

impl Family {
    pub(crate) fn find(target: &str) -> Result<Option<Self>, Error> {
        let target = argument(Some(target))?.expect("required target");
        let Some(adapter) =
            (unsafe { raw::yvex_graph_execution_find(0, 0, target.as_ptr()).as_ref() })
        else {
            return Ok(None);
        };
        // Family registrations are immutable native data. No vtable pointer escapes the FFI seam.
        Ok(Some(Self {
            key: unsafe { borrowed_text(adapter.operator_family_key)? },
            artifact: unsafe { borrowed_text(adapter.operator_artifact_filename)? },
            manifest: if adapter.source_manifest_filename.is_null() {
                String::new()
            } else {
                unsafe { borrowed_text(adapter.source_manifest_filename)? }
            },
            name: unsafe { borrowed_text(adapter.family_name)? },
            target,
        }))
    }

    pub(crate) fn select(&self, token: &str) -> Result<u64, Error> {
        let token = argument(Some(token))?.expect("selection token");
        let adapter = unsafe { &*raw::yvex_graph_execution_find(0, 0, self.target.as_ptr()) };
        let select = unsafe { adapter.api.as_ref() }
            .and_then(|api| api.selection_key_resolve)
            .ok_or_else(|| Error {
                code: raw::yvex_status_YVEX_ERR_STATE,
                owner: "graph.family".into(),
                message: "runtime family lacks selection-key resolution".into(),
            })?;
        let mut key = 0;
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { select(token.as_ptr(), &mut key, &mut failure) },
            &failure,
        )?;
        Ok(key)
    }

    pub(crate) fn prepare(&self, paths: [Option<&str>; 9]) -> Result<String, Error> {
        let inputs = paths
            .into_iter()
            .map(argument)
            .collect::<Result<Vec<_>, _>>()?;
        let pointer = |index: usize| {
            inputs[index]
                .as_ref()
                .map_or(std::ptr::null(), |v| v.as_ptr())
        };
        let adapter = unsafe { &*raw::yvex_graph_execution_find(0, 0, self.target.as_ptr()) };
        if adapter.compiler.is_null() {
            return Err(Error {
                code: raw::yvex_status_YVEX_ERR_UNSUPPORTED,
                owner: "graph.prepare".into(),
                message: "target has no preparation adapter".into(),
            });
        }
        let request = raw::yvex_compilation_runtime_binding_request {
            source_path: pointer(0),
            models_root: pointer(1),
            source_manifest_path: pointer(2),
            artifact_path: pointer(3),
            directory: pointer(4),
            quant_policy_path: pointer(5),
            quant_preset_name: pointer(6),
            imatrix_path: pointer(7),
            physical_variant_plan_path: pointer(8),
            family_adapter_id: adapter.adapter_id,
            family_adapter_version: adapter.adapter_version,
            ..Default::default()
        };
        let mut path = [0; raw::YVEX_PATH_CAP as usize];
        let mut published = 0;
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe {
                raw::yvex_runtime_binding_compile_publish(
                    adapter.compiler,
                    &request,
                    path.as_mut_ptr(),
                    &mut published,
                    &mut failure,
                )
            },
            &failure,
        )?;
        Ok(text(&path))
    }
}

pub(crate) fn binding(path: &str) -> Result<raw::yvex_runtime_binding_summary, Error> {
    Ok(binding_lineage(path)?.summary)
}

pub(crate) struct BindingLineage {
    pub summary: raw::yvex_runtime_binding_summary,
    pub tokenizer_conversation: Option<String>,
}

pub(crate) fn binding_lineage(path: &str) -> Result<BindingLineage, Error> {
    let path = argument(Some(path))?.expect("required binding");
    let mut binding = std::ptr::null_mut();
    let mut summary = raw::yvex_runtime_binding_summary::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_binding_failure::default();
    let status = unsafe {
        raw::yvex_runtime_binding_open(
            &mut binding,
            path.as_ptr(),
            &mut summary,
            std::ptr::null_mut(),
            &mut reason,
            &mut failure,
        )
    };
    // Borrow only while the authenticated binding is alive. No pointer crosses FFI.
    let tokenizer_conversation = if status == 0 && !binding.is_null() {
        let policy = unsafe { raw::yvex_runtime_binding_tokenizer_policy(binding) };
        if policy.is_null() {
            None
        } else {
            let identity = text(unsafe { &(*policy).policy_identity });
            (!identity.is_empty()).then_some(identity)
        }
    } else {
        None
    };
    // Close partial opens as well; the summary is a pointer-free copied record.
    unsafe { raw::yvex_runtime_binding_close(binding) };
    checked(status, &failure)?;
    Ok(BindingLineage {
        summary,
        tokenizer_conversation,
    })
}

pub(crate) struct Run {
    pub result: Box<raw::yvex_graph_attention_operator_result>,
    pub failure: Option<Error>,
    lease: *mut raw::yvex_runtime_cleanup_lease,
    _inputs: Vec<Option<CString>>,
    _cancel: Arc<AtomicBool>,
}

impl Run {
    pub(crate) fn finish(&mut self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_runtime_cleanup_lease_close(&mut self.lease, &mut failure) },
            &failure,
        )
    }
}

impl Drop for Run {
    fn drop(&mut self) {
        if !self.lease.is_null() {
            let mut failure = raw::yvex_error::default();
            // Normal command publication calls checked finish first. This guard also
            // retires partial work when later projection/benchmark code returns early.
            unsafe { raw::yvex_runtime_cleanup_lease_close(&mut self.lease, &mut failure) };
        }
    }
}

extern "C" fn cancelled(context: *mut std::ffi::c_void) -> i32 {
    // The Arc storage outlives the synchronous native call and its retained cleanup.
    unsafe { &*context.cast::<AtomicBool>() }
        .load(Ordering::Relaxed)
        .into()
}

struct Progress<'a>(&'a mut dyn FnMut(u32, u64, u64) -> bool);
extern "C" fn progress(context: *mut std::ffi::c_void, phase: u32, done: u64, total: u64) -> i32 {
    let context = unsafe { &mut *context.cast::<Progress<'_>>() };
    // Panics must never unwind into C. Failed output requests ordinary cancellation.
    let ok = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        (context.0)(phase, done, total)
    }))
    .unwrap_or(false);
    ok.into()
}

pub(crate) fn selection(request: &raw::yvex_graph_attention_operator_request) -> Result<(), Error> {
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_graph_attention_operator_selection_validate(request, &mut failure) },
        &failure,
    )
}

pub(crate) fn attention(
    mut request: raw::yvex_graph_attention_operator_request,
    paths: [Option<&str>; 6],
    cancel: Arc<AtomicBool>,
    feedback: &mut dyn FnMut(u32, u64, u64) -> bool,
) -> Result<Run, Error> {
    let inputs = paths
        .into_iter()
        .map(argument)
        .collect::<Result<Vec<_>, _>>()?;
    let pointer = |index: usize| {
        inputs[index]
            .as_ref()
            .map_or(std::ptr::null(), |v| v.as_ptr())
    };
    request.target = pointer(0);
    request.artifact_path = pointer(1);
    request.runtime_binding_path = pointer(2);
    request.activation_input_path = pointer(3);
    request.capture_bucket = pointer(4);
    request.attention_class = pointer(5);
    let mut report = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_graph_attention_operator_selection_validate(&request, &mut report) },
        &report,
    )?;
    let mut result = Box::<raw::yvex_graph_attention_operator_result>::default();
    let mut lease = std::ptr::null_mut();
    let mut feedback = Progress(feedback);
    request.progress = Some(progress);
    request.progress_context = (&mut feedback as *mut Progress<'_>).cast();
    request.cancel_requested = Some(cancelled);
    request.cancel_context = Arc::as_ptr(&cancel).cast_mut().cast();
    let status = unsafe {
        raw::yvex_graph_attention_operator_execute(
            &request,
            result.as_mut(),
            &mut lease,
            &mut report,
        )
    };
    let failure = checked(status, &report).err();
    Ok(Run {
        result,
        failure,
        lease,
        _inputs: inputs,
        _cancel: cancel,
    })
}

pub(crate) fn baseline_from_attention(
    result: &raw::yvex_graph_attention_operator_result,
) -> Result<raw::yvex_runtime_benchmark_baseline, Error> {
    let mut baseline = raw::yvex_runtime_benchmark_baseline::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_benchmark_failure::default();
    checked(
        unsafe {
            raw::yvex_runtime_benchmark_baseline_from_attention(
                result,
                &mut baseline,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(baseline)
}

pub(crate) fn baseline_open(path: &str) -> Result<raw::yvex_runtime_benchmark_baseline, Error> {
    let path = argument(Some(path))?.expect("required baseline");
    let mut baseline = raw::yvex_runtime_benchmark_baseline::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_benchmark_failure::default();
    checked(
        unsafe {
            raw::yvex_runtime_benchmark_baseline_open(
                path.as_ptr(),
                &mut baseline,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(baseline)
}

pub(crate) fn baseline_write(
    path: &str,
    current: &raw::yvex_runtime_benchmark_baseline,
) -> Result<raw::yvex_runtime_benchmark_publication, Error> {
    let path = argument(Some(path))?.expect("required baseline destination");
    let mut result = raw::yvex_runtime_benchmark_publication::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_benchmark_failure::default();
    checked(
        unsafe {
            raw::yvex_runtime_benchmark_baseline_write(
                path.as_ptr(),
                current,
                &mut result,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(result)
}

pub(crate) fn baseline_compare(
    current: &raw::yvex_runtime_benchmark_baseline,
    baseline: &raw::yvex_runtime_benchmark_baseline,
    threshold: Option<u64>,
) -> Result<raw::yvex_runtime_benchmark_comparison, Error> {
    let policy = raw::yvex_runtime_benchmark_regression_policy {
        enabled: threshold.is_some().into(),
        basis_points: threshold.unwrap_or(0),
    };
    let mut result = raw::yvex_runtime_benchmark_comparison::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_benchmark_failure::default();
    checked(
        unsafe {
            raw::yvex_runtime_benchmark_compare(
                current,
                baseline,
                &policy,
                &mut result,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(result)
}

pub(crate) fn chart(
    path: &str,
    current: &raw::yvex_runtime_benchmark_baseline,
    baseline: Option<&raw::yvex_runtime_benchmark_baseline>,
) -> Result<raw::yvex_runtime_benchmark_chart_result, Error> {
    let path = argument(Some(path))?.expect("required chart destination");
    let request = raw::yvex_runtime_benchmark_chart_request {
        path: path.as_ptr(),
        current,
        baseline: baseline.map_or(std::ptr::null(), |v| v),
        file_faults: std::ptr::null(),
    };
    let mut result = raw::yvex_runtime_benchmark_chart_result::default();
    let mut failure = raw::yvex_error::default();
    let mut reason = raw::yvex_runtime_benchmark_failure::default();
    checked(
        unsafe {
            raw::yvex_runtime_benchmark_chart_write(
                &request,
                &mut result,
                &mut reason,
                &mut failure,
            )
        },
        &failure,
    )?;
    Ok(result)
}
