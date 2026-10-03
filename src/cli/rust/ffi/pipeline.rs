// Typed computational operators keep native allocation, cancellation and cleanup ownership.
use super::{Error, argument, execution::checked, raw};
use std::{
    ffi::CString,
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Kind {
    Moe,
    Transformer,
    Decode,
    Logits,
    Sample,
}

pub(crate) fn backend(name: &str) -> Result<raw::yvex_backend_kind, Error> {
    let name = argument(Some(name))?.expect("required backend");
    let mut kind = raw::yvex_backend_kind_YVEX_BACKEND_KIND_CPU;
    let mut failure = raw::yvex_error::default();
    checked(
        unsafe { raw::yvex_backend_kind_parse(name.as_ptr(), &mut kind, &mut failure) },
        &failure,
    )?;
    Ok(kind)
}

pub(crate) fn sampling_defaults() -> raw::yvex_runtime_sampling_policy {
    let mut defaults = raw::yvex_provider_sampling::default();
    unsafe { raw::yvex_provider_sampling_default(&mut defaults) };
    raw::yvex_runtime_sampling_policy {
        schema_version: raw::YVEX_RUNTIME_SAMPLING_SCHEMA_V1,
        strategy: raw::yvex_sampling_strategy_YVEX_SAMPLING_STRATEGY_GREEDY,
        temperature: defaults.temperature,
        top_k: defaults.top_k,
        top_p: defaults.top_p,
        min_p: defaults.min_p,
        typical_p: defaults.typical_p,
        ..Default::default()
    }
}

pub(crate) struct Input<'a> {
    pub kind: Kind,
    pub paths: [&'a str; 4],
    pub backend: raw::yvex_backend_kind,
    pub chunk: u64,
    pub prefill: u64,
    pub context: u64,
    pub host_bytes: u64,
    pub device_bytes: u64,
    pub policy: raw::yvex_runtime_sampling_policy,
}

pub(crate) enum Record {
    Moe(Box<raw::yvex_moe_operator_result>),
    Transformer(Box<raw::yvex_transformer_operator_result>),
    Decode(Box<raw::yvex_decode_operator_result>),
    Logits(Box<raw::yvex_logits_operator_result>),
    Sample(Box<raw::yvex_sampling_operator_result>),
    Generation(Box<raw::yvex_generation_operator_result>),
}

pub(crate) struct Run {
    pub record: Record,
    pub failure: Option<Error>,
    pub(super) lease: *mut raw::yvex_runtime_cleanup_lease,
    pub(super) maximum_rows: u64,
    pub(super) _inputs: Vec<CString>,
    pub(super) _messages: Vec<raw::yvex_prompt_message>,
    pub(super) _cancel: Arc<AtomicBool>,
}

impl Run {
    pub(crate) fn finish(&mut self) -> Result<(), Error> {
        let mut failure = raw::yvex_error::default();
        checked(
            unsafe { raw::yvex_runtime_cleanup_lease_close(&mut self.lease, &mut failure) },
            &failure,
        )
    }

    pub(crate) fn steps(&self) -> Result<Vec<raw::yvex_runtime_decode_step_result>, Error> {
        match &self.record {
            Record::Decode(result) => {
                copied_rows(result.steps, result.step_count, self.maximum_rows)
            }
            _ => Ok(Vec::new()),
        }
    }

    pub(crate) fn rows(&self) -> Result<Vec<raw::yvex_runtime_logits_row_result>, Error> {
        match &self.record {
            Record::Logits(result) => {
                let rows = copied_rows(result.rows, result.row_count, self.maximum_rows)?;
                if rows
                    .iter()
                    .any(|v| !v.minimum_logit.is_finite() || !v.maximum_logit.is_finite())
                {
                    return Err(projection_error(
                        "native logits evidence contains non-finite scalars",
                    ));
                }
                Ok(rows)
            }
            _ => Ok(Vec::new()),
        }
    }

    pub(crate) fn samples(&self) -> Result<Vec<raw::yvex_runtime_sampling_result>, Error> {
        match &self.record {
            Record::Sample(result) => {
                let rows = copied_rows(result.samples, result.sample_count, self.maximum_rows)?;
                if rows.iter().any(|v| {
                    !v.selected_logit.is_finite()
                        || !v.selected_probability.is_finite()
                        || !v.selected_log_probability.is_finite()
                }) {
                    return Err(projection_error(
                        "native sampling evidence contains non-finite scalars",
                    ));
                }
                Ok(rows)
            }
            _ => Ok(Vec::new()),
        }
    }

    pub(crate) fn tokens(&self) -> Result<Vec<raw::yvex_runtime_generation_token_result>, Error> {
        match &self.record {
            Record::Generation(result) => {
                copied_rows(result.tokens, result.token_count, self.maximum_rows)
            }
            _ => Ok(Vec::new()),
        }
    }

    pub(crate) fn completed(&self) -> bool {
        match &self.record {
            Record::Moe(v) => v.completed != 0,
            Record::Transformer(v) => v.completed != 0,
            Record::Decode(v) => v.completed != 0,
            Record::Logits(v) => v.completed != 0,
            Record::Sample(v) => v.completed != 0,
            Record::Generation(v) => v.completed != 0,
        }
    }

    pub(crate) fn command(&mut self, command: &str) {
        let target = match &mut self.record {
            Record::Moe(v) => &mut v.command,
            Record::Transformer(v) => &mut v.command,
            Record::Decode(v) => &mut v.command,
            Record::Logits(v) => &mut v.command,
            Record::Sample(v) => &mut v.command,
            Record::Generation(v) => &mut v.command,
        };
        super::execution::copy_text(target, command);
    }

    pub(crate) fn refuse(&mut self, failure: Error) {
        let (completed, status, reason) = match &mut self.record {
            Record::Moe(v) => (&mut v.completed, &mut v.status, &mut v.reason),
            Record::Transformer(v) => (&mut v.completed, &mut v.status, &mut v.reason),
            Record::Decode(v) => (&mut v.completed, &mut v.status, &mut v.reason),
            Record::Logits(v) => (&mut v.completed, &mut v.status, &mut v.reason),
            Record::Sample(v) => (&mut v.completed, &mut v.status, &mut v.reason),
            Record::Generation(v) => (&mut v.completed, &mut v.status, &mut v.reason),
        };
        *completed = 0;
        super::execution::copy_text(status, "refused");
        super::execution::copy_text(reason, &failure.message);
        self.failure = Some(failure);
    }
}

impl Drop for Run {
    fn drop(&mut self) {
        if !self.lease.is_null() {
            let mut failure = raw::yvex_error::default();
            unsafe { raw::yvex_runtime_cleanup_lease_close(&mut self.lease, &mut failure) };
        }
        // Copied projections never own native buffers. Release each native result once,
        // including partial results and errors during Rust projection/publication.
        unsafe {
            match &mut self.record {
                Record::Decode(v) => raw::yvex_runtime_decode_operator_result_release(v.as_mut()),
                Record::Logits(v) => raw::yvex_runtime_logits_operator_result_release(v.as_mut()),
                Record::Sample(v) => raw::yvex_runtime_sampling_operator_result_release(v.as_mut()),
                Record::Generation(v) => {
                    raw::yvex_runtime_generation_operator_result_release(v.as_mut())
                }
                Record::Moe(_) | Record::Transformer(_) => {}
            }
        }
    }
}

fn copied_rows<T: Copy>(pointer: *const T, count: u64, maximum: u64) -> Result<Vec<T>, Error> {
    let count = usize::try_from(count)
        .ok()
        .filter(|v| {
            *v as u64 <= maximum
                && v.checked_mul(std::mem::size_of::<T>())
                    .is_some_and(|n| n <= isize::MAX as usize)
        })
        .ok_or_else(|| projection_error("native result exceeds its admitted row bound"))?;
    if count == 0 {
        return Ok(Vec::new());
    }
    if pointer.is_null() {
        return Err(projection_error("native result has rows without storage"));
    }
    // The synchronous native result retains this allocation until Run drops.
    Ok(unsafe { std::slice::from_raw_parts(pointer, count) }.to_vec())
}

fn projection_error(message: &str) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "pipeline.result".into(),
        message: message.into(),
    }
}

pub(super) extern "C" fn cancelled(context: *mut std::ffi::c_void) -> i32 {
    unsafe { &*context.cast::<AtomicBool>() }
        .load(Ordering::Relaxed)
        .into()
}

pub(crate) fn run(input: Input<'_>, cancel: Arc<AtomicBool>) -> Result<Run, Error> {
    let inputs = input
        .paths
        .into_iter()
        .map(|v| argument(Some(v)).map(Option::unwrap))
        .collect::<Result<Vec<_>, _>>()?;
    let pointer = |index: usize| inputs[index].as_ptr();
    let cancel_context = Arc::as_ptr(&cancel).cast_mut().cast();
    let mut failure = raw::yvex_error::default();
    let mut lease = std::ptr::null_mut();
    let (record, status) = match input.kind {
        Kind::Moe => {
            let request = raw::yvex_moe_operator_request {
                target: pointer(0),
                artifact_path: pointer(1),
                runtime_binding_path: pointer(2),
                input_path: pointer(3),
                backend: input.backend,
                maximum_host_bytes: input.host_bytes,
                maximum_device_bytes: input.device_bytes,
                cancel_requested: Some(cancelled),
                cancel_context,
            };
            let mut result = Box::<raw::yvex_moe_operator_result>::default();
            let status = unsafe {
                raw::yvex_runtime_moe_operator_execute(
                    &request,
                    result.as_mut(),
                    &mut lease,
                    &mut failure,
                )
            };
            (Record::Moe(result), status)
        }
        Kind::Transformer => {
            let request = raw::yvex_transformer_operator_request {
                target: pointer(0),
                artifact_path: pointer(1),
                runtime_binding_path: pointer(2),
                input_path: pointer(3),
                backend: input.backend,
                chunk_tokens: input.chunk,
                context_capacity: input.context,
                maximum_host_bytes: input.host_bytes,
                maximum_device_bytes: input.device_bytes,
                cancel_requested: Some(cancelled),
                cancel_context,
            };
            let mut result = Box::<raw::yvex_transformer_operator_result>::default();
            let status = unsafe {
                raw::yvex_transformer_operator_execute(
                    &request,
                    result.as_mut(),
                    &mut lease,
                    &mut failure,
                )
            };
            (Record::Transformer(result), status)
        }
        Kind::Decode => {
            let request = raw::yvex_decode_operator_request {
                target: pointer(0),
                artifact_path: pointer(1),
                runtime_binding_path: pointer(2),
                input_path: pointer(3),
                backend: input.backend,
                prefill_tokens: input.prefill,
                prefill_chunk_tokens: input.chunk,
                context_capacity: input.context,
                maximum_host_bytes: input.host_bytes,
                maximum_device_bytes: input.device_bytes,
                cancel_requested: Some(cancelled),
                cancel_context,
            };
            let mut result = Box::<raw::yvex_decode_operator_result>::default();
            let status = unsafe {
                raw::yvex_runtime_decode_operator_execute(
                    &request,
                    result.as_mut(),
                    &mut lease,
                    &mut failure,
                )
            };
            (Record::Decode(result), status)
        }
        Kind::Logits | Kind::Sample => {
            let logits = raw::yvex_logits_operator_request {
                target: pointer(0),
                artifact_path: pointer(1),
                runtime_binding_path: pointer(2),
                input_path: pointer(3),
                backend: input.backend,
                prefill_tokens: input.prefill,
                prefill_chunk_tokens: input.chunk,
                context_capacity: input.context,
                maximum_host_bytes: input.host_bytes,
                maximum_device_bytes: input.device_bytes,
            };
            if input.kind == Kind::Logits {
                let mut result = Box::<raw::yvex_logits_operator_result>::default();
                let status = unsafe {
                    raw::yvex_runtime_logits_operator_execute(
                        &logits,
                        result.as_mut(),
                        &mut lease,
                        &mut failure,
                    )
                };
                (Record::Logits(result), status)
            } else {
                let request = raw::yvex_sampling_operator_request {
                    logits,
                    policy: input.policy,
                    maximum_sampling_host_bytes: input.host_bytes,
                    cancel_requested: Some(cancelled),
                    cancel_context,
                };
                let mut result = Box::<raw::yvex_sampling_operator_result>::default();
                let status = unsafe {
                    raw::yvex_runtime_sampling_operator_execute(
                        &request,
                        result.as_mut(),
                        &mut lease,
                        &mut failure,
                    )
                };
                (Record::Sample(result), status)
            }
        }
    };
    Ok(Run {
        record,
        failure: checked(status, &failure).err(),
        lease,
        maximum_rows: input.context,
        _inputs: inputs,
        _messages: Vec::new(),
        _cancel: cancel,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn partial_native_rows_remain_bounded_and_null_safe() {
        let values = [1u32, 2];
        assert!(copied_rows::<u32>(std::ptr::null(), 1, 2).is_err());
        assert!(copied_rows(values.as_ptr(), 3, 2).is_err());
        assert_eq!(copied_rows(values.as_ptr(), 2, 2).unwrap(), values);
        assert!(
            copied_rows::<u32>(std::ptr::null(), 0, 2)
                .unwrap()
                .is_empty()
        );
        assert!(copied_rows(values.as_ptr(), u64::MAX, u64::MAX).is_err());
    }
}
