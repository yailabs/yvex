//! Typed YVEX OpenAI-compatible capacity extension. The caller owns its
//! qualified HTTP transport and admission; this module interprets only the
//! producer's exact public catalog/preflight contract.

use serde_json::Value;

pub const PROFILE: &str = "yvex.openai.compat.v3";
pub const CAPACITY_SCHEMA: &str = "yvex.execution.capacity.v1";

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CapacityObservation {
    pub model_id: String,
    pub engine_generation: u64,
    pub runtime_binding_identity: String,
    pub runtime_model_identity: String,
    pub capacity_plan_identity: String,
    pub tokenizer_identity: Option<String>,
    pub prompt_identity: Option<String>,
    pub provider_request_identity: Option<String>,
    pub http_body_limit_bytes: u64,
    pub input_tokens: Option<u64>,
    pub input_capacity_tokens: u64,
    pub sequence_capacity_tokens: u64,
    pub requested_output_tokens: Option<u64>,
    pub effective_output_tokens: Option<u64>,
    pub full_requested_output_fits: Option<bool>,
    pub token_capacity_compatible: Option<bool>,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CapacityAssessment {
    pub capacity: Option<CapacityObservation>,
    pub refusal: Option<&'static str>,
}

pub struct HttpResponse {
    pub status: u16,
    pub body: Vec<u8>,
}

fn text(value: &Value, key: &str) -> Result<String, String> {
    value[key]
        .as_str()
        .filter(|text| !text.is_empty() && text.len() <= 256)
        .map(str::to_string)
        .ok_or_else(|| format!("capacity_field_invalid:{key}"))
}

fn number(value: &Value, key: &str) -> Result<u64, String> {
    value[key]
        .as_u64()
        .ok_or_else(|| format!("capacity_field_invalid:{key}"))
}

fn boolean(value: &Value, key: &str) -> Result<bool, String> {
    value[key]
        .as_bool()
        .ok_or_else(|| format!("capacity_field_invalid:{key}"))
}

pub fn advertised(row: &Value, model: &str) -> Result<CapacityObservation, String> {
    let limits = &row["yvex_capacity"];
    if row["yvex_profile"] != PROFILE
        || limits["schema"] != CAPACITY_SCHEMA
        || limits["input_accounting"] != "exact_tokenizer_including_template_and_tools"
        || limits["resource_reservation"] != false
    {
        return Err("capacity_contract_invalid".into());
    }
    Ok(CapacityObservation {
        model_id: model.into(),
        engine_generation: number(row, "engine_generation")?,
        runtime_binding_identity: text(row, "runtime_binding_identity")?,
        runtime_model_identity: text(row, "runtime_model_identity")?,
        capacity_plan_identity: text(row, "capacity_plan_identity")?,
        tokenizer_identity: None,
        prompt_identity: None,
        provider_request_identity: None,
        http_body_limit_bytes: number(limits, "http_body_bytes")?,
        input_tokens: None,
        input_capacity_tokens: number(limits, "runtime_input_tokens")?,
        sequence_capacity_tokens: number(limits, "runtime_sequence_tokens")?,
        requested_output_tokens: None,
        effective_output_tokens: None,
        full_requested_output_fits: None,
        token_capacity_compatible: None,
    })
}

pub fn qualify_preflight(
    value: &Value,
    model: &str,
    expected: &CapacityObservation,
) -> Result<CapacityObservation, String> {
    if value["object"] != "yvex.execution.preflight"
        || value["model"] != model
        || value["scope"] != "complete_stateless_chat_request"
        || value["execution_or_resources_qualified"] != false
    {
        return Err("capacity_preflight_identity_mismatch".into());
    }
    let mut result = advertised(value, model)?;
    if &result != expected {
        return Err("capacity_deployment_changed".into());
    }
    result.tokenizer_identity = Some(text(value, "tokenizer_identity")?);
    result.prompt_identity = Some(text(value, "prompt_identity")?);
    result.provider_request_identity = Some(text(value, "provider_request_identity")?);
    let input = number(value, "input_tokens")?;
    let output = number(value, "effective_output_tokens")?;
    let compatible = boolean(value, "token_capacity_compatible")?;
    let input_exceeded = boolean(value, "input_capacity_exceeded")?;
    let output_exceeded = boolean(value, "output_capacity_exceeded")?;
    if compatible
        && (input_exceeded
            || output_exceeded
            || input > result.input_capacity_tokens
            || input
                .checked_add(output)
                .is_none_or(|count| count > result.sequence_capacity_tokens))
    {
        return Err("capacity_preflight_inconsistent".into());
    }
    result.input_tokens = Some(input);
    result.requested_output_tokens = Some(number(value, "requested_output_tokens")?);
    result.effective_output_tokens = Some(output);
    result.full_requested_output_fits = Some(boolean(value, "full_requested_output_fits")?);
    result.token_capacity_compatible = Some(compatible);
    Ok(result)
}

/// Fetch uses one caller-qualified HTTP origin/locality. It receives an exact
/// method/path/body and must not rewrite or retry the body behind this API.
pub fn assess_capacity<F>(
    model: &str,
    chat_path: &str,
    body: &[u8],
    mut fetch: F,
) -> Result<CapacityAssessment, String>
where
    F: FnMut(&str, &str, &[u8]) -> Result<HttpResponse, String>,
{
    let prefix = chat_path
        .strip_suffix("/chat/completions")
        .ok_or("capacity_endpoint_not_supported")?;
    let catalog = fetch("GET", &format!("{prefix}/models"), &[])?;
    if catalog.status != 200 {
        return Err("capacity_catalog_unavailable".into());
    }
    let catalog: Value =
        serde_json::from_slice(&catalog.body).map_err(|_| "capacity_catalog_invalid")?;
    let rows = catalog["data"]
        .as_array()
        .filter(|rows| rows.len() <= 128)
        .ok_or("capacity_catalog_invalid")?;
    let rows = rows
        .iter()
        .filter(|row| row["id"].as_str() == Some(model))
        .collect::<Vec<_>>();
    if rows.len() != 1 {
        return Err("capacity_exact_model_unavailable".into());
    }
    let row = rows[0];
    if row["yvex_profile"] != PROFILE {
        return Ok(CapacityAssessment {
            capacity: None,
            refusal: None,
        });
    }
    let expected = advertised(row, model)?;
    if body.len() as u64 > expected.http_body_limit_bytes {
        return Ok(CapacityAssessment {
            capacity: Some(expected),
            refusal: Some("http_body_capacity_exceeded"),
        });
    }
    let route = format!("{chat_path}/preflight");
    if row["yvex_capacity"]["preflight"].as_str() != Some(route.as_str()) {
        return Err("capacity_preflight_route_mismatch".into());
    }
    let response = fetch("POST", &route, body)?;
    if response.status != 200 {
        return Err("capacity_preflight_unavailable".into());
    }
    let value: Value =
        serde_json::from_slice(&response.body).map_err(|_| "capacity_preflight_invalid")?;
    let capacity = qualify_preflight(&value, model, &expected)?;
    let refusal =
        (capacity.token_capacity_compatible != Some(true)).then_some("token_capacity_exceeded");
    Ok(CapacityAssessment {
        capacity: Some(capacity),
        refusal,
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    fn row() -> Value {
        json!({"id":"model:one","yvex_profile":PROFILE,"engine_generation":1,
            "runtime_binding_identity":"binding:one","runtime_model_identity":"runtime:one",
            "capacity_plan_identity":"capacity:one","yvex_capacity":{"schema":CAPACITY_SCHEMA,
            "input_accounting":"exact_tokenizer_including_template_and_tools",
            "resource_reservation":false,"http_body_bytes":1000,
            "runtime_input_tokens":32,"runtime_sequence_tokens":64,
            "preflight":"/v1/chat/completions/preflight"}})
    }

    fn preflight() -> Value {
        let mut value = row();
        value.as_object_mut().unwrap().extend(
            json!({
                "object":"yvex.execution.preflight","model":"model:one",
                "scope":"complete_stateless_chat_request","execution_or_resources_qualified":false,
                "input_tokens":10,"requested_output_tokens":20,"effective_output_tokens":20,
                "full_requested_output_fits":true,"input_capacity_exceeded":false,
                "output_capacity_exceeded":false,"token_capacity_compatible":true,
                "tokenizer_identity":"tokenizer:one","prompt_identity":"prompt:one",
                "provider_request_identity":"request:one"
            })
            .as_object()
            .unwrap()
            .clone(),
        );
        value
    }

    #[test]
    fn exact_public_capacity_and_preflight_are_qualified() {
        let expected = advertised(&row(), "model:one").unwrap();
        let qualified = qualify_preflight(&preflight(), "model:one", &expected).unwrap();
        assert_eq!(qualified.input_tokens, Some(10));
        assert_eq!(qualified.effective_output_tokens, Some(20));
        for (field, value) in [
            ("model", json!("other")),
            ("engine_generation", json!(2)),
            ("input_tokens", json!(100)),
            ("tokenizer_identity", Value::Null),
        ] {
            let mut changed = preflight();
            changed[field] = value;
            assert!(
                qualify_preflight(&changed, "model:one", &expected).is_err(),
                "{field}"
            );
        }
    }

    #[test]
    fn transport_composition_preserves_exact_body_and_absence() {
        let body = br#"{"messages":[{"role":"user","content":"exact"}]}"#;
        let mut calls = Vec::new();
        let result = assess_capacity(
            "model:one",
            "/v1/chat/completions",
            body,
            |method, path, sent| {
                calls.push((method.to_string(), path.to_string(), sent.to_vec()));
                let value = if method == "GET" {
                    json!({"data":[row()]})
                } else {
                    preflight()
                };
                Ok(HttpResponse {
                    status: 200,
                    body: value.to_string().into_bytes(),
                })
            },
        )
        .unwrap();
        assert_eq!(result.refusal, None);
        assert_eq!(calls[0].0, "GET");
        assert_eq!(calls[1].1, "/v1/chat/completions/preflight");
        assert_eq!(calls[1].2, body);

        calls.clear();
        let large = vec![b'x'; 1001];
        let result = assess_capacity(
            "model:one",
            "/v1/chat/completions",
            &large,
            |method, path, sent| {
                calls.push((method.to_string(), path.to_string(), sent.to_vec()));
                Ok(HttpResponse {
                    status: 200,
                    body: json!({"data":[row()]}).to_string().into_bytes(),
                })
            },
        )
        .unwrap();
        assert_eq!(result.refusal, Some("http_body_capacity_exceeded"));
        assert_eq!(calls.len(), 1);
    }
}
