// Bounded engineering generation borrows prompts and owns the native result until publication.
use super::{
    Error, argument,
    execution::checked,
    pipeline::{Record, Run, cancelled},
    raw,
};
use std::sync::{Arc, atomic::AtomicBool};

pub(crate) struct Input<'a> {
    pub target: &'a str,
    pub artifact: &'a str,
    pub binding: &'a str,
    pub text: Option<&'a str>,
    pub system: Option<&'a str>,
    pub user: Option<&'a str>,
    pub backend: raw::yvex_backend_kind,
    pub mode: raw::yvex_runtime_generation_mode,
    pub policy: raw::yvex_runtime_sampling_policy,
    pub context: u64,
    pub chunk: u64,
    pub new_tokens: u64,
    pub output_bytes: u64,
    pub host_bytes: u64,
    pub device_bytes: u64,
}

pub(crate) fn run(input: Input<'_>, cancel: Arc<AtomicBool>) -> Result<Run, Error> {
    let supplied = [
        Some(input.target),
        Some(input.artifact),
        Some(input.binding),
        input.text,
        input.system,
        input.user,
    ];
    let mut inputs = Vec::new();
    let mut indexes = Vec::new();
    for value in supplied {
        let index = if let Some(value) = argument(value)? {
            inputs.push(value);
            Some(inputs.len() - 1)
        } else {
            None
        };
        indexes.push(index);
    }
    let pointer = |i: usize| indexes[i].map_or(std::ptr::null(), |j| inputs[j].as_ptr());
    let mut messages = Vec::new();
    for (index, role) in [
        (4, raw::yvex_prompt_role_YVEX_PROMPT_ROLE_SYSTEM),
        (5, raw::yvex_prompt_role_YVEX_PROMPT_ROLE_USER),
    ] {
        if let Some(i) = indexes[index] {
            messages.push(raw::yvex_prompt_message {
                schema_version: raw::YVEX_PROMPT_MESSAGE_SCHEMA_V1,
                role,
                content: inputs[i].as_ptr(),
                content_len: inputs[i].as_bytes().len() as u64,
                ..Default::default()
            });
        }
    }
    let text_bytes = input.text.map_or(0, str::len) as u64;
    let request = raw::yvex_generation_operator_request {
        target: pointer(0),
        artifact_path: pointer(1),
        runtime_binding_path: pointer(2),
        backend: input.backend,
        mode: input.mode,
        input_kind: if input.text.is_some() {
            raw::yvex_runtime_generation_input_kind_YVEX_GENERATION_INPUT_TEXT
        } else {
            raw::yvex_runtime_generation_input_kind_YVEX_GENERATION_INPUT_MESSAGES
        },
        text: pointer(3).cast(),
        text_bytes,
        messages: if messages.is_empty() {
            std::ptr::null()
        } else {
            messages.as_ptr()
        },
        message_count: messages.len() as u64,
        prompt_options: raw::yvex_prompt_options {
            mode: raw::yvex_prompt_mode_YVEX_PROMPT_MODE_CHAT,
            add_generation_prompt: input.text.is_none().into(),
            drop_thinking: input.text.is_none().into(),
            ..Default::default()
        },
        encode_options: raw::yvex_tokenizer_encode_options {
            allow_special_tokens: 1,
            maximum_tokens: input.context,
            ..Default::default()
        },
        context_capacity: input.context,
        prefill_chunk_tokens: input.chunk,
        maximum_new_tokens: input.new_tokens,
        maximum_output_bytes: input.output_bytes,
        maximum_host_bytes: input.host_bytes,
        maximum_device_bytes: input.device_bytes,
        sampling_policy: input.policy,
        cancel_requested: Some(cancelled),
        cancel_context: Arc::as_ptr(&cancel).cast_mut().cast(),
    };
    let mut result = Box::<raw::yvex_generation_operator_result>::default();
    let mut failure = raw::yvex_error::default();
    let mut lease = std::ptr::null_mut();
    let status = unsafe {
        raw::yvex_runtime_generation_operator_execute(
            &request,
            result.as_mut(),
            &mut lease,
            &mut failure,
        )
    };
    Ok(Run {
        record: Record::Generation(result),
        failure: checked(status, &failure).err(),
        lease,
        maximum_rows: input.new_tokens,
        _inputs: inputs,
        _messages: messages,
        _cancel: cancel,
    })
}

pub(crate) fn profile_names() -> (Vec<String>, Vec<String>) {
    let name = |p| unsafe { super::borrowed_text(p).unwrap_or_else(|_| "unknown".into()) };
    let phases = (0..raw::yvex_runtime_profile_phase_YVEX_RUNTIME_PROFILE_PHASE_COUNT)
        .map(|i| name(unsafe { raw::yvex_runtime_profile_phase_name(i) }))
        .collect();
    let counters = (0..raw::yvex_runtime_profile_counter_YVEX_RUNTIME_PROFILE_COUNTER_COUNT)
        .map(|i| name(unsafe { raw::yvex_runtime_profile_counter_name(i) }))
        .collect();
    (phases, counters)
}

pub(crate) fn profile_mode(mode: raw::yvex_runtime_profile_mode) -> String {
    unsafe {
        super::borrowed_text(raw::yvex_runtime_profile_mode_name(mode))
            .unwrap_or_else(|_| "unknown".into())
    }
}

pub(crate) fn stop_reason(reason: raw::yvex_runtime_generation_stop_reason) -> String {
    unsafe {
        super::borrowed_text(raw::yvex_runtime_generation_stop_reason_name(reason))
            .unwrap_or_else(|_| "unknown".into())
    }
}
