// Versioned product management. Restricted enrollment is separate from v1 reads
// and from the finite producer. Native domains retain all computational truth.
use crate::{
    Output, ffi, management, management_jobs as jobs, management_models as models,
    management_runtime as runtime, registry::Invocation,
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::io::{self, BufRead, Read};
const REQUEST_CAP: usize = 131072;
const RESPONSE_CAP: usize = 1048576;
pub(crate) const OPERATIONS: &[(&str, &str)] = &[
    ("profile.scan", "job"),
    ("profile.verify", "job"),
    ("profile.create", "job"),
    ("profile.remove", "job"),
    ("source.cleanup", "job"),
    ("registry.accounts", "read"),
    ("model.storage", "read"),
    ("source.verify", "job"),
    ("package.verify", "job"),
    ("model.evict", "job"),
    ("management.capabilities", "read"),
    ("model.list", "read"),
    ("model.get", "read"),
    ("model.search", "read"),
    ("model.inspect", "read"),
    ("acquisition.start", "job"),
    ("acquisition.get", "read"),
    ("acquisition.cancel", "job"),
    ("acquisition.resume", "job"),
    ("build.start", "job"),
    ("package.get", "read"),
    ("host.get", "read"),
    ("engine.list", "read"),
    ("engine.load", "job"),
    ("engine.unload", "job"),
    ("session.list", "read"),
    ("session.get", "read"),
    ("session.create", "job"),
    ("session.fork", "job"),
    ("session.close", "job"),
    ("session.reset", "job"),
    ("generation.start", "job"),
    ("generation.cancel", "job"),
    ("job.list", "read"),
    ("job.get", "read"),
    ("observe.events", "read"),
];
#[derive(Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    schema: String,
    request_id: String,
    operation: String,
    input: Value,
}
fn parse(bytes: &[u8]) -> std::result::Result<Request, &'static str> {
    if bytes.len() > REQUEST_CAP || !bytes.ends_with(b"\n") {
        return Err("malformed_request");
    }
    let r: Request = serde_json::from_slice(bytes).map_err(|_| "malformed_request")?;
    if r.schema != "yvex.management.request.v2"
        || !jobs::identity(&r.request_id)
        || !r.input.is_object()
    {
        return Err("malformed_request");
    }
    Ok(r)
}
fn capability() -> Value {
    let operations = OPERATIONS
        .iter()
        .map(|(op, kind)| json!({"operation":op,"kind":kind}))
        .collect::<Vec<_>>();
    json!({
        "protocol":"yvex.management.v2","producer_version":ffi::version(),
        "local_protocol":ffi::LOCAL_PROTOCOL_VERSION,
        "grant":"product-management","operations":operations,
        "limits":{
            "request_bytes":REQUEST_CAP,"response_bytes":RESPONSE_CAP,
            "receipt_retention":512,"page_size":256,
            "generation_output_bytes":262144,"generation_channel_segments":4096
        },
        "automatic_retry":false,"training":false
    })
}
fn result(r: &Request, peer: &str) -> std::result::Result<Value, ffi::Error> {
    let kind = OPERATIONS
        .iter()
        .find(|(op, _)| *op == r.operation)
        .map(|(_, kind)| *kind)
        .ok_or_else(|| runtime::failure("unsupported_operation"))?;
    if kind == "job" {
        if r.operation.starts_with("engine.")
            || r.operation.starts_with("session.")
            || r.operation.starts_with("generation.")
        {
            runtime::validate(&r.operation, &r.input)?;
        } else {
            models::validate(&r.operation, &r.input)?;
        }
        return serde_json::to_value(jobs::submit(peer, &r.request_id, &r.operation, &r.input)?)
            .map_err(|_| runtime::failure("job_encoding_failed"));
    }
    match r.operation.as_str() {
        "management.capabilities" => {
            if !r.input.as_object().unwrap().is_empty() {
                return Err(runtime::failure("invalid_input"));
            }
            Ok(capability())
        }
        "job.get" => {
            if r.input.as_object().unwrap().len() != 1 {
                return Err(runtime::failure("invalid_input"));
            }
            let id = r.input["job_id"]
                .as_str()
                .ok_or_else(|| runtime::failure("invalid_job_identity"))?;
            serde_json::to_value(jobs::get(peer, id)?)
                .map_err(|_| runtime::failure("job_encoding_failed"))
        }
        "job.list" => {
            let object = r.input.as_object().unwrap();
            if object.keys().any(|key| key != "limit") {
                return Err(runtime::failure("invalid_input"));
            }
            let limit = match object.get("limit") {
                None => 128,
                Some(value) => value
                    .as_u64()
                    .filter(|v| *v > 0 && *v <= 256)
                    .ok_or_else(|| runtime::failure("invalid_limit"))?
                    as usize,
            };
            jobs::list(peer, limit)
        }
        "host.get" | "engine.list" | "session.list" | "session.get" | "observe.events" => {
            runtime::read(&r.operation, &r.input)
        }
        _ => models::read(&r.operation, &r.input),
    }
}
fn reply(request: &Request, device: &str, peer: &str, journal: &str) -> Value {
    let mut out = json!({"schema":"yvex.management.response.v2","request_id":request.request_id,
        "device_identity":device,
        "authenticated_peer":peer,
        "status":"ok","data":null,"reason":null});
    match result(request, journal) {
        Ok(value) => out["data"] = value,
        Err(error) => {
            out["status"] = json!(if error.message == "unsupported_operation" {
                "unsupported"
            } else if error.owner == "management.indeterminate"
                || error.code == ffi::raw::yvex_status_YVEX_ERR_IO
                || error.code == ffi::raw::yvex_status_YVEX_ERR_TIMEOUT
            {
                "unavailable"
            } else {
                "refused"
            });
            out["reason"] = json!(error.to_string());
        }
    }
    bound_reply(out)
}
fn bound_reply(mut out: Value) -> Value {
    // Include the mandatory newline in the same bound enforced by the SDK.
    if out.to_string().len() + 1 > RESPONSE_CAP {
        out["data"] = Value::Null;
        out["status"] = json!("unavailable");
        out["reason"] = json!("response_bound_observe_exact_job");
    }
    out
}
// Authenticated transports share one operation dispatcher and durable receipt owner.
pub(crate) fn network_request(
    bytes: &[u8],
    device: &str,
    peer: &str,
    journal: &str,
) -> std::result::Result<Value, &'static str> {
    let mut framed = bytes.to_vec();
    if !framed.ends_with(b"\n") {
        framed.push(b'\n');
    }
    let request = parse(&framed)?;
    Ok(reply(&request, device, peer, journal))
}
pub(crate) fn protocol(invocation: &Invocation<'_>) -> std::result::Result<Output, &'static str> {
    let (device, peer) = match management::authenticate(invocation, "product-protocol") {
        Ok(v) => v,
        Err("restricted_ssh_required") => return Err("restricted_ssh_required"),
        Err(_) => {
            return Ok(Output::standard(
                format!(
                    "{}\n",
                    json!({
                        "schema":"yvex.management.response.v2","request_id":null,
                        "device_identity":null,"authenticated_peer":null,
                        "status":"refused","data":null,
                        "reason":"peer_revoked_or_authority_unavailable"
                    })
                ),
                0,
            ));
        }
    };
    let mut bytes = Vec::new();
    io::stdin()
        .lock()
        .take(REQUEST_CAP as u64)
        .read_until(b'\n', &mut bytes)
        .map_err(|_| "request_read_failed")?;
    let out = match parse(&bytes) {
        Ok(r) => reply(
            &r,
            &format!("ssh-ed25519:sha256:{device}"),
            &format!("ssh-ed25519:sha256:{peer}"),
            &peer,
        ),
        Err(reason) => {
            json!({
                "schema":"yvex.management.response.v2","request_id":null,
                "status":"refused","data":null,"reason":reason
            })
        }
    };
    Ok(Output::standard(format!("{out}\n"), 0))
}
pub(crate) fn worker(invocation: &Invocation<'_>) -> std::result::Result<Output, &'static str> {
    jobs::run(&invocation.positionals[0], &invocation.positionals[1])
        .map_err(|_| "product_worker_failed")?;
    Ok(Output::standard(String::new(), 0))
}
#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn strict_request() {
        let value = json!({
            "schema":"yvex.management.request.v2","request_id":"a".repeat(64),
            "operation":"host.get","input":{}
        });
        assert!(parse(format!("{value}\n").as_bytes()).is_ok());
        assert!(parse(value.to_string().as_bytes()).is_err());
        let mut bad = value.clone();
        bad["token"] = json!("forbidden");
        assert!(parse(format!("{bad}\n").as_bytes()).is_err());
    }
    #[test]
    fn framing_bounds_include_the_newline() {
        let value = json!({
            "schema":"yvex.management.request.v2","request_id":"a".repeat(64),
            "operation":"host.get","input":{}
        });
        let mut bytes = value.to_string().into_bytes();
        bytes.resize(REQUEST_CAP - 1, b' ');
        bytes.push(b'\n');
        assert!(parse(&bytes).is_ok());
        bytes.insert(bytes.len() - 1, b' ');
        assert!(parse(&bytes).is_err());
        let mut response = json!({"data":"","status":"ok","reason":null});
        let overhead = response.to_string().len() + 1;
        response["data"] = json!("x".repeat(RESPONSE_CAP - overhead));
        assert_eq!(bound_reply(response.clone())["status"], "ok");
        response["data"] = json!("x".repeat(RESPONSE_CAP - overhead + 1));
        let bounded = bound_reply(response);
        assert_eq!(bounded["status"], "unavailable");
        assert!(bounded["data"].is_null() && bounded.to_string().len() < RESPONSE_CAP);
    }
    #[test]
    fn no_speculative_training_or_finite_grant() {
        assert!(
            !OPERATIONS
                .iter()
                .any(|(op, _)| op.starts_with("training.") || op.starts_with("finite."))
        );
    }
}
