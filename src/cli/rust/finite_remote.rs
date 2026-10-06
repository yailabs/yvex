// Separately versioned public finite computation over the enrolled SSH substrate.
use crate::{Output, ffi, management, registry::Invocation};
use serde::Deserialize;
use serde_json::{Value, json};
use std::io::{self, BufRead, Read};

const REQUEST_CAP: usize = 32768;
const RESPONSE_SCHEMA: &str = "yvex.finite.response.v1";

#[derive(Debug, Deserialize)]
#[serde(deny_unknown_fields)]
struct Request {
    schema: String,
    request_id: String,
    operation: String,
    input: ffi::finite::Request,
}

fn parse_request(bytes: &[u8]) -> Result<Request, &'static str> {
    if bytes.len() > REQUEST_CAP || !bytes.ends_with(b"\n") {
        return Err("malformed_request");
    }
    let request: Request = serde_json::from_slice(bytes).map_err(|_| "malformed_request")?;
    if request.schema != "yvex.finite.request.v1"
        || request.request_id.len() != 64
        || !request
            .request_id
            .bytes()
            .all(|c| c.is_ascii_digit() || (b'a'..=b'f').contains(&c))
    {
        return Err("malformed_request");
    }
    if request.operation != "finite.decision.execute" {
        return Err("unsupported_operation");
    }
    request.input.native().map_err(|_| "malformed_request")?;
    Ok(request)
}

fn refused(reason: &str) -> Value {
    json!({"schema":RESPONSE_SCHEMA,"request_id":null,"status":"refused",
           "dispatch_state":"not_dispatched","reason":reason})
}

fn execute(request: Request, device: &str, peer: &str) -> Value {
    let mut response = json!({"schema":RESPONSE_SCHEMA,"request_id":request.request_id,
        "operation":request.operation,"model_alias":request.input.model_alias,
        "device_identity":format!("ssh-ed25519:sha256:{device}"),
        "authenticated_peer":format!("ssh-ed25519:sha256:{peer}")});
    match ffi::finite::execute(&request.input) {
        Ok(result) => {
            response["status"] = json!("ok");
            response["dispatch_state"] = json!("completed");
            response["result"] = serde_json::to_value(result).expect("validated finite C result");
        }
        Err(error) => {
            response["status"] = json!("error");
            response["dispatch_state"] = json!("outcome_unavailable");
            response["reason"] = json!("producer_error");
            response["error"] =
                json!({"code":error.code,"name":ffi::status_name(error.code),"owner":error.owner});
        }
    }
    response
}

pub(crate) fn protocol(invocation: &Invocation<'_>) -> Result<Output, &'static str> {
    let response = match management::authenticate(invocation, "finite-protocol") {
        Err(reason) => refused(reason),
        Ok((device, peer)) => {
            let mut bytes = Vec::new();
            let read = io::stdin()
                .lock()
                .take((REQUEST_CAP + 1) as u64)
                .read_until(b'\n', &mut bytes);
            let parsed = read
                .map_err(|_| "request_read_failed")
                .and_then(|_| parse_request(&bytes));
            match parsed {
                Err(reason) => refused(reason),
                Ok(request) => execute(request, &device, &peer),
            }
        }
    };
    Ok(Output::standard(format!("{response}\n"), 0))
}

#[cfg(test)]
mod tests {
    use super::*;
    fn good() -> String {
        format!(
            "{}\n",
            json!({"schema":"yvex.finite.request.v1","request_id":"1".repeat(64),
            "operation":"finite.decision.execute", "input":{"model_alias":"finite", "expected_generation":1,
            "question":"Which?", "context":"", "candidates":[{"id":"yes","text":"yes"}]}})
        )
    }
    #[test]
    fn schema_and_nested_population_are_bounded_and_duplicate_safe() {
        let good = good();
        assert!(parse_request(good.as_bytes()).is_ok());
        for bad in [
            good.trim_end().into(),
            " ".repeat(REQUEST_CAP + 1),
            good.replace("finite.decision.execute", "model.load"),
            good.replace("yvex.finite.request.v1", "yvex.management.request.v1"),
            good.replace("\"context\":", "\"extra\":true,\"context\":"),
            good.replace("\"schema\":", "\"schema\":\"wrong\",\"schema\":"),
            good.replace("\"expected_generation\":1", "\"expected_generation\":0"),
        ] {
            assert!(parse_request(bad.as_bytes()).is_err());
        }
    }
}
