// Product management calls the same typed native client as the CLI. No CLI output parsing.
use crate::{
    client,
    ffi::{self, Client, Error, raw},
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::time::{Duration, Instant};

type Result<T> = std::result::Result<T, Error>;
pub(crate) fn failure(reason: &str) -> Error {
    Error {
        code: raw::yvex_status_YVEX_ERR_STATE,
        owner: "management.runtime".into(),
        message: reason.into(),
    }
}
fn uncertain(error: Error) -> Error {
    Error {
        owner: "management.indeterminate".into(),
        ..error
    }
}
fn decode<T: serde::de::DeserializeOwned>(input: &Value) -> Result<T> {
    serde_json::from_value(input.clone()).map_err(|_| failure("invalid_input"))
}
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct RuntimeInput {
    host_instance: String,
    #[serde(default)]
    model: String,
    #[serde(default)]
    generation: u64,
    #[serde(default)]
    name: String,
    #[serde(default)]
    session_identity: String,
    #[serde(default)]
    fork_name: String,
    #[serde(default)]
    maximum_prefix_bytes: u64,
    #[serde(default)]
    profile: String,
    #[serde(default)]
    context_capacity: u64,
    #[serde(default)]
    prompt: String,
    #[serde(default)]
    maximum_new_tokens: u64,
    #[serde(default)]
    reasoning: String,
}
#[derive(Deserialize, Default)]
#[serde(deny_unknown_fields)]
struct ReadInput {
    #[serde(default)]
    model: String,
    #[serde(default)]
    generation: u64,
    #[serde(default)]
    name: String,
    #[serde(default)]
    after_sequence: u64,
    #[serde(default)]
    limit: Option<usize>,
}
fn read_input(operation: &str, input: &Value) -> Result<ReadInput> {
    let allowed: &[&str] = match operation {
        "host.get" | "engine.list" => &[],
        "session.list" => &["model", "generation"],
        "session.get" => &["model", "generation", "name"],
        "observe.events" => &["after_sequence", "limit"],
        _ => return Err(failure("unsupported_operation")),
    };
    if input
        .as_object()
        .is_none_or(|v| v.keys().any(|k| !allowed.contains(&k.as_str())))
    {
        return Err(failure("invalid_input"));
    }
    let i: ReadInput = decode(input)?;
    if operation.starts_with("session.")
        && (i.model.is_empty()
            || i.model.len() > 127
            || i.model.contains('\0')
            || i.generation == 0)
    {
        return Err(failure("exact_engine_required"));
    }
    if operation == "session.get"
        && (i.name.is_empty() || i.name.len() > 63 || i.name.contains('\0'))
    {
        return Err(failure("session_name_required"));
    }
    if i.limit.is_some_and(|n| n == 0 || n > 256) {
        return Err(failure("invalid_limit"));
    }
    Ok(i)
}

const OUTPUT_CAP: usize = 262144;
const CHANNEL_CAP: usize = 4096;
struct ChannelBuffer {
    channel: &'static str,
    text: String,
    pending: Vec<u8>,
}
#[derive(Default)]
struct OutputStream {
    channels: Vec<ChannelBuffer>,
    bytes: usize,
}
impl OutputStream {
    fn push(&mut self, channel: &'static str, bytes: &[u8]) -> Result<()> {
        if self.bytes.saturating_add(bytes.len()) > OUTPUT_CAP {
            return Err(uncertain(failure("output_retention_bound")));
        }
        if bytes.is_empty() {
            return Ok(());
        }
        if self.channels.last().is_none_or(|v| v.channel != channel) {
            if self.channels.last().is_some_and(|v| !v.pending.is_empty()) {
                return Err(uncertain(failure("incomplete_utf8_channel")));
            }
            if self.channels.len() >= CHANNEL_CAP {
                return Err(uncertain(failure("output_channel_bound")));
            }
            self.channels.push(ChannelBuffer {
                channel,
                text: String::new(),
                pending: Vec::new(),
            });
        }
        let part = self.channels.last_mut().unwrap();
        // Only an incomplete UTF-8 suffix is reparsed; prior text is never rescanned per token.
        part.pending.extend_from_slice(bytes);
        match std::str::from_utf8(&part.pending) {
            Ok(text) => {
                part.text.push_str(text);
                part.pending.clear();
            }
            Err(error) if error.error_len().is_none() => {
                let valid = error.valid_up_to();
                part.text
                    .push_str(std::str::from_utf8(&part.pending[..valid]).unwrap());
                part.pending.drain(..valid);
            }
            Err(_) => return Err(uncertain(failure("invalid_utf8"))),
        }
        self.bytes += bytes.len();
        Ok(())
    }
    fn projection(&self, complete: bool) -> Result<Value> {
        if complete && self.channels.iter().any(|p| !p.pending.is_empty()) {
            return Err(uncertain(failure("incomplete_utf8")));
        }
        Ok(json!(
            self.channels
                .iter()
                .map(|p| json!({"channel":p.channel,"text":p.text}))
                .collect::<Vec<_>>()
        ))
    }
}
fn publish_progress(output: &Value, progress: &mut dyn FnMut(&Value) -> Result<()>) -> Result<()> {
    // Native work was already dispatched. Losing its durable observation is uncertainty.
    progress(output).map_err(uncertain)
}
fn connection(expected: Option<&str>) -> Result<(Client, String)> {
    let mut c = Client::connect(None)?;
    c.timeout(5000)?;
    let id = c.host_identity()?;
    if expected.is_some_and(|value| value != id) {
        return Err(failure("stale_host_instance"));
    }
    Ok((c, id))
}
fn session_json(reply: &raw::yvex_client_message, model: &str, generation: u64) -> Value {
    json!({"name":ffi::text(&reply.session_name),"identity":ffi::text(&reply.session_identity),
        "state":ffi::session_state_name(reply.session_state),"position":reply.final_position,
        "turns":reply.turn_count,"model":model,"generation":generation,
        "context_used":reply.context_used,
        "kv_used_bytes":if reply.kv_used_available != 0 {
            Some(reply.kv_used_bytes)
        } else {
            None
        }
    })
}
pub(crate) fn read(operation: &str, input: &Value) -> Result<Value> {
    let i = read_input(operation, input)?;
    if operation == "host.get" {
        let (mut c, id) = match connection(None) {
            Ok(v) => v,
            Err(e) => {
                let stopped = ffi::default_socket()
                    .ok()
                    .is_some_and(|p| !std::path::Path::new(&p).exists());
                return Ok(json!({
                    "state":if stopped {"stopped"} else {"unavailable"},
                    "status":null,"host_instance":null,"reason":e.to_string()
                }));
            }
        };
        let request = c.request(raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_STATUS);
        c.send(&request)?;
        let reply = client::response(&mut c, &request)?;
        if reply.kind != raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_STATUS {
            return Err(failure("invalid_host_response"));
        }
        return Ok(json!({
            "state":"running","host_instance":id,
            "status":client::HostStatus::from_snapshot(reply.runtime).json(),"reason":null
        }));
    }
    let (mut c, id) = connection(None)?;
    let native = match operation {
        "engine.list" => raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LIST,
        "session.list" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_LIST,
        "session.get" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_SHOW,
        "observe.events" => raw::yvex_client_operation_YVEX_CLIENT_OP_RUNTIME_TRACE,
        _ => return Err(failure("unsupported_operation")),
    };
    let mut request = c.request(native);
    ffi::put_text(&mut request.model_alias, &i.model)?;
    ffi::put_text(&mut request.session_name, &i.name)?;
    request.engine_generation = i.generation;
    request.event_after_sequence = i.after_sequence;
    request.trace_level = raw::yvex_server_trace_level_YVEX_SERVER_TRACE_STAGES;
    c.send(&request)?;
    let limit = i.limit.unwrap_or(128);
    let mut rows = Vec::new();
    let mut cursor = i.after_sequence;
    let mut truncated = false;
    loop {
        let reply = client::response(&mut c, &request)?;
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK {
            break;
        }
        let row = match reply.kind {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE => {
                client::engine_json(&reply.engine)
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION => {
                session_json(&reply, &i.model, i.generation)
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT => {
                cursor = reply.event.sequence;
                client::event_json(&reply.event)
            }
            _ => return Err(failure("unexpected_observation")),
        };
        rows.push(row);
        if operation == "session.get" {
            break;
        }
        if rows.len() >= limit {
            truncated = true;
            break;
        }
    }
    Ok(match operation {
        "engine.list" => json!({"host_instance":id,"engines":rows,"truncated":truncated}),
        "session.list" => json!({"host_instance":id,"sessions":rows,"truncated":truncated}),
        "session.get" => json!({"host_instance":id,"session":rows.first()}),
        _ => json!({"host_instance":id,"events":rows,"next_sequence":cursor,"truncated":truncated}),
    })
}

pub(crate) fn validate(operation: &str, input: &Value) -> Result<()> {
    let i: RuntimeInput = decode(input)?;
    if !crate::management_jobs::identity(&i.host_instance) {
        return Err(failure("host_instance_required"));
    }
    let allowed: &[&str] = match operation {
        "engine.load" => &["host_instance", "profile", "context_capacity"],
        "engine.unload" => &["host_instance", "model", "generation"],
        "session.create" => &["host_instance", "model", "generation", "name"],
        "session.fork" => &[
            "host_instance",
            "model",
            "generation",
            "name",
            "session_identity",
            "fork_name",
            "maximum_prefix_bytes",
        ],
        "session.close" | "session.reset" | "generation.cancel" => &[
            "host_instance",
            "model",
            "generation",
            "name",
            "session_identity",
        ],
        "generation.start" => &[
            "host_instance",
            "model",
            "generation",
            "name",
            "session_identity",
            "prompt",
            "maximum_new_tokens",
            "reasoning",
        ],
        _ => return Err(failure("unsupported_operation")),
    };
    if input
        .as_object()
        .is_none_or(|v| v.keys().any(|k| !allowed.contains(&k.as_str())))
    {
        return Err(failure("invalid_input"));
    }
    if operation == "engine.load" {
        if i.profile.is_empty() || i.profile.len() > 127 {
            return Err(failure("profile_required"));
        }
    } else if i.model.is_empty() || i.model.len() > 127 || i.generation == 0 {
        return Err(failure("exact_engine_required"));
    }
    if (operation.starts_with("session.") || operation.starts_with("generation."))
        && (i.name.is_empty() || i.name.len() > 63)
    {
        return Err(failure("session_name_required"));
    }
    if operation != "session.create"
        && (operation.starts_with("session.") || operation.starts_with("generation."))
        && !crate::management_jobs::identity(&i.session_identity)
    {
        return Err(failure("session_identity_required"));
    }
    if operation == "session.fork" && (i.fork_name.is_empty() || i.maximum_prefix_bytes == 0) {
        return Err(failure("fork_bounds_required"));
    }
    if operation == "generation.start"
        && (i.prompt.is_empty()
            || i.prompt.len() > 65536
            || i.maximum_new_tokens == 0
            || i.maximum_new_tokens > 8192
            || !["disabled", "enabled", "low", "maximum"].contains(&i.reasoning.as_str()))
    {
        return Err(failure("generation_bounds_required"));
    }
    Ok(())
}

pub(crate) fn execute(
    operation: &str,
    input: &Value,
    progress: &mut dyn FnMut(&Value) -> Result<()>,
) -> Result<Value> {
    validate(operation, input)?;
    let i: RuntimeInput = decode(input)?;
    let (mut c, id) = connection(Some(&i.host_instance))?;
    let native = match operation {
        "engine.load" => raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_LOAD,
        "engine.unload" => raw::yvex_client_operation_YVEX_CLIENT_OP_ENGINE_UNLOAD,
        "session.create" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_NEW,
        "session.fork" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_FORK,
        "session.close" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_CLOSE,
        "session.reset" => raw::yvex_client_operation_YVEX_CLIENT_OP_SESSION_RESET,
        "generation.start" => raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_TURN,
        "generation.cancel" => raw::yvex_client_operation_YVEX_CLIENT_OP_GENERATION_CANCEL,
        _ => return Err(failure("unsupported_operation")),
    };
    if operation != "engine.load" && i.generation == 0 {
        return Err(failure("exact_generation_required"));
    }
    let mut request = c.request(native);
    ffi::put_text(
        &mut request.model_alias,
        if operation == "engine.load" {
            &i.profile
        } else {
            &i.model
        },
    )?;
    ffi::put_text(&mut request.session_name, &i.name)?;
    ffi::put_text(&mut request.fork_session_name, &i.fork_name)?;
    ffi::put_text(&mut request.expected_session_identity, &i.session_identity)?;
    request.engine_generation = i.generation;
    request.load_context_capacity = i.context_capacity;
    request.maximum_prefix_bytes = i.maximum_prefix_bytes;
    if operation == "generation.start" {
        if i.prompt.is_empty()
            || i.prompt.len() > 65536
            || i.maximum_new_tokens == 0
            || i.maximum_new_tokens > 8192
        {
            return Err(failure("generation_bounds_required"));
        }
        request.maximum_new_tokens = i.maximum_new_tokens;
        request.reasoning_policy = match i.reasoning.as_str() {
            "disabled" => raw::yvex_reasoning_policy_YVEX_REASONING_DISABLED,
            "enabled" => raw::yvex_reasoning_policy_YVEX_REASONING_ENABLED,
            "low" => raw::yvex_reasoning_policy_YVEX_REASONING_LOW,
            "maximum" => raw::yvex_reasoning_policy_YVEX_REASONING_MAXIMUM,
            _ => return Err(failure("unsupported_reasoning_policy")),
        };
    }
    c.timeout(600_000)?;
    c.send_text(&request, &i.prompt, &[]).map_err(uncertain)?;
    let mut output = json!({
        "host_instance":id,"model":i.model,"generation":i.generation,
        "session":i.name,"channels":[],"turn_identity":null,"complete":false
    });
    let mut last = Instant::now();
    let mut stream = OutputStream::default();
    loop {
        let reply = c.receive().map_err(uncertain)?;
        if reply.request_number != request.request_number {
            return Err(uncertain(failure("correlation_mismatch")));
        }
        if reply.kind == raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ERROR {
            return Err(Error {
                code: reply.status,
                owner: "management.native_refusal".into(),
                message: ffi::text(&reply.reason),
            });
        }
        match reply.kind {
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ENGINE => {
                return Ok(json!({"host_instance":id,"engine":client::engine_json(&reply.engine)}));
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_SESSION => {
                return Ok(json!({
                    "host_instance":id,"session":session_json(&reply,&i.model,i.generation)
                }));
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_ACK => {
                return Ok(json!({
                    "host_instance":id,"acknowledged":true,"reason":ffi::text(&reply.reason)
                }));
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_STARTED => {
                output["turn_identity"] = json!(ffi::text(&reply.turn_identity))
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_FRAGMENT => {
                let count = usize::try_from(reply.byte_count)
                    .map_err(|_| uncertain(failure("invalid_fragment")))?;
                if count > reply.bytes.len() {
                    return Err(uncertain(failure("output_retention_bound")));
                }
                let channel = match reply.stream_channel {
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_FINAL_TEXT => "final_text",
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_EXPLICIT_REASONING => {
                        "public_reasoning"
                    }
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_TOOL_CALL => "tool_call",
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_TOOL_RESULT => "tool_result",
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_CONTROL_EVENT => "control",
                    raw::yvex_client_stream_channel_YVEX_CLIENT_STREAM_ERROR => "error",
                    _ => return Err(uncertain(failure("unsupported_output_channel"))),
                };
                stream.push(channel, &reply.bytes[..count])?;
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_EVENT => {
                output["progress"] = client::event_json(&reply.event)
            }
            raw::yvex_client_message_kind_YVEX_CLIENT_MESSAGE_TURN_COMPLETE => {
                output["complete"] = json!(true);
                output["turn_identity"] = json!(ffi::text(&reply.turn_identity));
                output["metrics"] = json!({
                    "prompt_tokens":reply.prompt_tokens,"generated_tokens":reply.generated_tokens,
                    "reasoning_tokens":reply.reasoning_tokens,"final_tokens":reply.final_tokens,
                    "stop_reason":reply.stop_reason,
                    "total_completion_seconds":reply.total_completion_seconds,
                    "final_position":reply.final_position
                });
                output["channels"] = stream.projection(true)?;
                return Ok(output);
            }
            _ => return Err(uncertain(failure("unexpected_generation_response"))),
        }
        if last.elapsed() >= Duration::from_millis(500) {
            output["channels"] = stream.projection(false)?;
            publish_progress(&output, progress)?;
            last = Instant::now();
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn reads_require_exact_scope_and_reject_foreign_fields_before_connection() {
        for (op, value) in [
            ("host.get", json!({"model":"wrong"})),
            ("engine.list", json!({"limit":1})),
            ("session.list", json!({})),
            ("session.list", json!({"model":"m","generation":0})),
            ("session.get", json!({"model":"m","generation":1})),
            ("observe.events", json!({"limit":0})),
            ("observe.events", json!({"limit":257})),
            ("observe.events", json!({"after_sequence":-1})),
        ] {
            assert!(read_input(op, &value).is_err(), "{op}");
        }
        assert!(read_input("session.list", &json!({"model":"m","generation":1})).is_ok());
        assert!(
            read_input(
                "session.get",
                &json!({"model":"m","generation":1,"name":"s"})
            )
            .is_ok()
        );
        assert!(read_input("observe.events", &json!({"limit":256,"after_sequence":4})).is_ok());
        assert!(read_input("host.get", &json!({})).is_ok());
    }
    #[test]
    fn fragmented_utf8_and_public_channel_order_are_preserved() {
        let mut stream = OutputStream::default();
        stream.push("final_text", b"a\xe2").unwrap();
        assert_eq!(stream.projection(false).unwrap()[0]["text"], "a");
        assert!(stream.projection(true).is_err());
        stream.push("final_text", b"\x82\xac").unwrap();
        stream.push("public_reasoning", b"public").unwrap();
        stream.push("final_text", b"done").unwrap();
        let result = stream.projection(true).unwrap();
        assert_eq!(result[0]["text"], "a€");
        assert_eq!(result[1]["channel"], "public_reasoning");
        assert_eq!(result[2]["text"], "done");
        assert!(result[0].get("bytes").is_none());
    }
    #[test]
    fn stream_bounds_and_invalid_utf8_preserve_indeterminate_outcome() {
        let mut stream = OutputStream::default();
        stream.push("final_text", &vec![b'a'; OUTPUT_CAP]).unwrap();
        assert_eq!(
            stream.push("final_text", b"b").unwrap_err().owner,
            "management.indeterminate"
        );
        let mut stream = OutputStream::default();
        assert!(stream.push("final_text", &[0xff]).is_err());
        let mut stream = OutputStream::default();
        stream.push("final_text", &[0xe2]).unwrap();
        assert!(stream.push("public_reasoning", b"x").is_err());
        let mut stream = OutputStream::default();
        for n in 0..CHANNEL_CAP {
            stream
                .push(
                    if n % 2 == 0 {
                        "final_text"
                    } else {
                        "public_reasoning"
                    },
                    b"x",
                )
                .unwrap();
        }
        assert!(stream.push("final_text", b"x").is_err());
    }
    #[test]
    fn publication_failure_after_dispatch_never_becomes_definite_failure_or_retry() {
        let mut attempts = 0;
        let error = publish_progress(&json!({"complete":false}), &mut |_| {
            attempts += 1;
            Err(failure("job_publication_failed"))
        })
        .unwrap_err();
        assert_eq!(attempts, 1);
        assert_eq!(error.owner, "management.indeterminate");
        assert_eq!(error.message, "job_publication_failed");
    }
}
