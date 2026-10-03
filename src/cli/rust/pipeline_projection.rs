// Machine projections copy typed computational facts; human views select information by intent.
use crate::{
    ffi::{
        self,
        pipeline::{Record, Run},
        raw,
    },
    presentation,
};
use serde_json::{Value, json};
type Fields = Vec<(&'static str, Value)>;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn nine_digits(value: f32) -> f64 {
    // Existing row schemas publish F32 values with nine significant decimal digits.
    format!("{value:.8e}")
        .parse()
        .expect("finite native row value")
}

fn decode_step(step: &raw::yvex_runtime_decode_step_result) -> Value {
    json!({"ordinal": step.step_ordinal, "token_id": step.token_id,
        "position_before": step.position_before, "position_after": step.position_after,
        "generation_before": step.generation_before, "generation_after": step.generation_after,
        "hidden_digest": ffi::text(&step.normalized_hidden_digest),
        "state_digest": ffi::text(&step.persistent_state_digest),
        "transformer_execution_identity": ffi::text(&step.transformer_execution_identity),
        "decode_step_identity": ffi::text(&step.decode_step_identity)})
}

fn source_phase(phase: raw::yvex_logits_source_phase) -> &'static str {
    if phase == raw::yvex_logits_source_phase_YVEX_LOGITS_SOURCE_PREFILL {
        "prefill"
    } else {
        "decode"
    }
}

fn logits_row(index: usize, row: &raw::yvex_runtime_logits_row_result) -> Value {
    json!({"ordinal": index, "source_phase": source_phase(row.source_phase),
        "source_position": row.source_position, "logits_count": row.logits_count, "finite_count": row.finite_count,
        "minimum_logit": nine_digits(row.minimum_logit), "maximum_logit": nine_digits(row.maximum_logit),
        "raw_logits_digest": ffi::text(&row.raw_logits_digest),
        "output_head_residency_identity": ffi::text(&row.output_head_residency_identity),
        "backend_execution_identity": ffi::text(&row.backend_execution_identity),
        "logits_row_identity": ffi::text(&row.logits_row_identity)})
}

fn sample_row(index: usize, row: &raw::yvex_runtime_sampling_result) -> Value {
    json!({"ordinal": index, "phase": source_phase(row.source_phase),
        "position": row.source_position, "token_id": row.selected_token_id,
        "logit": nine_digits(row.selected_logit), "probability": row.selected_probability,
        "log_probability": row.selected_log_probability, "candidates": row.final_candidate_count,
        "source_identity": ffi::text(&row.source_identity),
        "candidate_identity": ffi::text(&row.candidate_set_identity),
        "rng_before": ffi::text(&row.rng_state_before_identity), "rng_after": ffi::text(&row.rng_state_after_identity),
        "identity": ffi::text(&row.selected_token_identity)})
}

pub(crate) fn fields(run: &Run) -> Result<Fields> {
    Ok(match &run.record {
        Record::Moe(v) => moe(v),
        Record::Generation(v) => crate::generation_projection::fields(v, &run.tokens()?)?,
        Record::Transformer(v) => transformer(v),
        Record::Decode(v) => {
            let mut fields = decode(v);
            fields.push((
                "steps",
                json!(run.steps()?.iter().map(decode_step).collect::<Vec<_>>()),
            ));
            fields
        }
        Record::Logits(v) => {
            let mut fields = logits(v);
            fields.push((
                "rows",
                json!(
                    run.rows()?
                        .iter()
                        .enumerate()
                        .map(|(i, v)| logits_row(i, v))
                        .collect::<Vec<_>>()
                ),
            ));
            fields
        }
        Record::Sample(v) => {
            let mut fields = sampling(v);
            fields.push((
                "selected_tokens",
                json!(
                    run.samples()?
                        .iter()
                        .enumerate()
                        .map(|(i, v)| sample_row(i, v))
                        .collect::<Vec<_>>()
                ),
            ));
            fields
        }
    })
}

fn value_text(value: &Value) -> String {
    value
        .as_str()
        .map(str::to_owned)
        .unwrap_or_else(|| value.to_string())
}

fn significant(value: f64, digits: usize) -> String {
    if value == 0.0 || !value.is_finite() {
        return value.to_string();
    }
    let scientific = format!("{:.*e}", digits - 1, value);
    let (mantissa, exponent) = scientific.split_once('e').expect("scientific format");
    let exponent: i32 = exponent.parse().expect("scientific exponent");
    if exponent < -4 || exponent >= digits as i32 {
        return format!(
            "{}e{}{:02}",
            mantissa.trim_end_matches('0').trim_end_matches('.'),
            if exponent >= 0 { "+" } else { "-" },
            exponent.unsigned_abs()
        );
    }
    let sign = if value < 0.0 { "-" } else { "" };
    let digits = mantissa.trim_start_matches('-').replace('.', "");
    let point = exponent + 1;
    let decimal = if point <= 0 {
        format!("0.{}{}", "0".repeat((-point) as usize), digits)
    } else if point as usize >= digits.len() {
        format!("{}{}", digits, "0".repeat(point as usize - digits.len()))
    } else {
        format!(
            "{}.{}",
            &digits[..point as usize],
            &digits[point as usize..]
        )
    };
    format!(
        "{sign}{}",
        if decimal.contains('.') {
            decimal.trim_end_matches('0').trim_end_matches('.')
        } else {
            &decimal
        }
    )
}

fn csv_scalar(value: &Value) -> String {
    if value.is_f64() {
        significant(value.as_f64().expect("number"), 17)
    } else {
        value_text(value)
    }
}

fn csv_row(group: &str, index: usize, row: &Value) -> (String, String) {
    let text = |key| {
        if key == "logit" {
            significant(row[key].as_f64().expect("row logit"), 9)
        } else {
            csv_scalar(&row[key])
        }
    };
    match group {
        "generated_tokens" => (
            format!("token.{index}"),
            format!(
                "id={} committed={} terminal={} text_bytes={}",
                text("token_id"),
                text("model_committed"),
                text("terminal"),
                text("text_bytes")
            ),
        ),
        "steps" => (
            format!("step.{index}"),
            format!(
                "token={} position={}:{} generation={}:{} hidden={} state={} identity={}",
                text("token_id"),
                text("position_before"),
                text("position_after"),
                text("generation_before"),
                text("generation_after"),
                text("hidden_digest"),
                text("state_digest"),
                text("decode_step_identity")
            ),
        ),
        "rows" => (
            format!("row.{index}"),
            format!(
                "phase={} position={} values={} finite={} digest={} identity={}",
                text("source_phase"),
                text("source_position"),
                text("logits_count"),
                text("finite_count"),
                text("raw_logits_digest"),
                text("logits_row_identity")
            ),
        ),
        _ => (
            format!("sample.{index}"),
            format!(
                concat!(
                    "phase={} position={} token={} logit={} probability={} log_probability={} ",
                    "candidates={} source={} candidate={} rng_before={} rng_after={} identity={}"
                ),
                text("phase"),
                text("position"),
                text("token_id"),
                text("logit"),
                text("probability"),
                text("log_probability"),
                text("candidates"),
                text("source_identity"),
                text("candidate_identity"),
                text("rng_before"),
                text("rng_after"),
                text("identity")
            ),
        ),
    }
}

pub(crate) fn present(fields: &Fields, output: &str, width: usize, styled: bool) -> Result<String> {
    if output == "json" {
        let object = fields
            .iter()
            .map(|(key, value)| ((*key).to_owned(), value.clone()))
            .collect::<serde_json::Map<_, _>>();
        return Ok(format!("{}\n", Value::Object(object)));
    }
    if output == "csv" {
        let mut result = String::from("field,value\n");
        for (key, value) in fields {
            let rows = value
                .as_array()
                .map(|rows| {
                    rows.iter()
                        .enumerate()
                        .map(|(i, v)| csv_row(key, i, v))
                        .collect()
                })
                .unwrap_or_else(|| vec![((*key).to_owned(), csv_scalar(value))]);
            for (key, value) in rows {
                result.push_str(&format!("\"{key}\",\"{}\"\n", value.replace('"', "\"\"")));
            }
        }
        return Ok(result);
    }
    let title = fields
        .iter()
        .find(|(k, _)| *k == "command")
        .map(|(_, v)| value_text(v))
        .unwrap_or_else(|| "EXECUTION".into());
    let audit = output == "audit";
    let selected = fields
        .iter()
        .filter(|(key, value)| {
            *key != "command"
                && (audit
                    || matches!(
                        *key,
                        "status"
                            | "target"
                            | "family"
                            | "backend"
                            | "logits_backend"
                            | "phase"
                            | "layers"
                            | "tokens"
                            | "token_count"
                            | "samples"
                            | "prompt_tokens"
                            | "sampled_tokens"
                            | "model_committed_tokens"
                            | "generated_text_bytes"
                            | "stop_reason"
                            | "execution_mode"
                            | "decode_steps_completed"
                            | "logits_rows_completed"
                            | "reason"
                            | "completed"
                    ) && !value.as_str().is_some_and(str::is_empty))
        })
        .map(|(key, value)| ((*key).to_owned(), value_text(value)))
        .collect::<Vec<_>>();
    let pairs = selected
        .iter()
        .map(|(k, v)| (k.as_str(), v.as_str()))
        .collect::<Vec<_>>();
    Ok(presentation::record(&title, &pairs, width, styled)?)
}

fn moe_0(result: &raw::yvex_moe_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("backend", json!(ffi::text(&result.backend))));
    fields.push((
        "artifact_identity",
        json!(ffi::text(&result.artifact_identity)),
    ));
    fields.push((
        "runtime_binding_identity",
        json!(ffi::text(&result.runtime_binding_identity)),
    ));
    fields.push((
        "runtime_descriptor_identity",
        json!(ffi::text(&result.runtime_descriptor_identity)),
    ));
    fields.push((
        "runtime_numeric_identity",
        json!(ffi::text(&result.runtime_numeric_identity)),
    ));
    fields.push((
        "moe_plan_identity",
        json!(ffi::text(&result.moe_plan_identity)),
    ));
    fields.push(("layers", json!(result.layer_count)));
    fields.push(("tokens", json!(result.token_count)));
    fields.push(("hash_router_layers", json!(result.hash_router_count)));
    fields.push(("learned_router_layers", json!(result.learned_router_count)));
    fields.push(("routed_experts", json!(result.routed_experts)));
    fields.push(("shared_experts", json!(result.shared_experts)));
    fields.push(("experts_per_token", json!(result.experts_per_token)));
    fields.push(("layers_executed", json!(result.execution.layers_executed)));
    fields.push((
        "hash_router_executions",
        json!(result.execution.hash_router_executions),
    ));
    fields.push((
        "learned_router_executions",
        json!(result.execution.learned_router_executions),
    ));
}

fn moe_1(result: &raw::yvex_moe_operator_result, fields: &mut Fields) {
    fields.push((
        "routed_expert_executions",
        json!(result.execution.routed_expert_executions),
    ));
    fields.push((
        "shared_expert_executions",
        json!(result.execution.shared_expert_executions),
    ));
    fields.push((
        "expert_subviews_accessed",
        json!(result.execution.expert_subviews_accessed),
    ));
    fields.push((
        "encoded_bytes_read",
        json!(result.execution.encoded_bytes_read),
    ));
    fields.push(("h2d_bytes", json!(result.execution.host_to_device_bytes)));
    fields.push(("d2h_bytes", json!(result.execution.device_to_host_bytes)));
    fields.push(("kernel_launches", json!(result.execution.kernel_launches)));
    fields.push(("upload_count", json!(result.execution.upload_count)));
    fields.push((
        "selected_expert_cache_hits",
        json!(result.execution.cache_hits),
    ));
    fields.push((
        "selected_expert_cache_misses",
        json!(result.execution.cache_misses),
    ));
    fields.push((
        "qtype_f32_weight_accesses",
        json!(result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_F32 as usize]),
    ));
    fields.push((
        "qtype_q8_0_weight_accesses",
        json!(result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_Q8_0 as usize]),
    ));
    fields.push((
        "qtype_q2_k_weight_accesses",
        json!(result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_Q2_K as usize]),
    ));
    fields.push((
        "qtype_iq2_xxs_weight_accesses",
        json!(
            result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_IQ2_XXS as usize]
        ),
    ));
    fields.push((
        "qtype_bf16_weight_accesses",
        json!(result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_BF16 as usize]),
    ));
    fields.push((
        "qtype_i32_weight_accesses",
        json!(result.execution.qtype_counts[raw::yvex_gguf_qtype_id_YVEX_GGUF_QTYPE_I32 as usize]),
    ));
    fields.push((
        "input_identity",
        json!(ffi::text(&result.execution.input_identity)),
    ));
    fields.push((
        "routing_digest",
        json!(ffi::text(&result.execution.routing_digest)),
    ));
    fields.push((
        "routed_digest",
        json!(ffi::text(&result.execution.routed_digest)),
    ));
    fields.push((
        "shared_digest",
        json!(ffi::text(&result.execution.shared_digest)),
    ));
}

fn moe_2(result: &raw::yvex_moe_operator_result, fields: &mut Fields) {
    fields.push((
        "combined_output_digest",
        json!(ffi::text(&result.execution.combined_output_digest)),
    ));
    fields.push((
        "execution_identity",
        json!(ffi::text(&result.execution.execution_identity)),
    ));
    fields.push(("moe_plan_ready", json!(result.moe_plan_ready != 0)));
    fields.push(("moe_router_ready", json!(result.moe_router_ready != 0)));
    fields.push((
        "moe_routed_expert_ready",
        json!(result.moe_routed_expert_ready != 0),
    ));
    fields.push((
        "moe_shared_expert_ready",
        json!(result.moe_shared_expert_ready != 0),
    ));
    fields.push(("moe_block_ready", json!(result.moe_block_ready != 0)));
    fields.push((
        "moe_prefill_composed",
        json!(result.moe_prefill_composed != 0),
    ));
    fields.push((
        "moe_decode_composed",
        json!(result.moe_decode_composed != 0),
    ));
    fields.push(("transformer_ready", json!(result.transformer_ready != 0)));
    fields.push(("generation_ready", json!(result.generation_ready != 0)));
    fields.push(("reason", json!(ffi::text(&result.reason))));
    fields.push(("completed", json!(result.completed != 0)));
}

fn moe(result: &raw::yvex_moe_operator_result) -> Fields {
    let mut fields = Vec::new();
    moe_0(result, &mut fields);
    moe_1(result, &mut fields);
    moe_2(result, &mut fields);
    fields
}

fn transformer_0(result: &raw::yvex_transformer_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("backend", json!(ffi::text(&result.backend))));
    fields.push(("phase", json!(ffi::text(&result.phase))));
    fields.push((
        "artifact_identity",
        json!(ffi::text(&result.artifact_identity)),
    ));
    fields.push((
        "runtime_binding_identity",
        json!(ffi::text(&result.runtime_binding_identity)),
    ));
    fields.push((
        "transformer_plan_identity",
        json!(ffi::text(&result.transformer_plan_identity)),
    ));
    fields.push((
        "input_identity",
        json!(ffi::text(&result.execution.input_identity)),
    ));
    fields.push(("token_start", json!(result.execution.token_start)));
    fields.push(("token_count", json!(result.execution.token_count)));
    fields.push(("chunk_count", json!(result.execution.chunk_count)));
    fields.push(("committed_prefix", json!(result.execution.committed_prefix)));
    fields.push(("position_before", json!(result.execution.position_before)));
    fields.push(("position_after", json!(result.execution.position_after)));
    fields.push((
        "generation_before",
        json!(result.execution.generation_before),
    ));
    fields.push(("generation_after", json!(result.execution.generation_after)));
    fields.push(("hidden_width", json!(result.hidden_width)));
    fields.push(("expanded_width", json!(result.expanded_width)));
}

fn transformer_1(result: &raw::yvex_transformer_operator_result, fields: &mut Fields) {
    fields.push(("layers", json!(result.layer_count)));
    fields.push(("embedding_rows", json!(result.execution.embedding_rows)));
    fields.push(("embedding_bytes", json!(result.execution.embedding_bytes)));
    fields.push(("layers_executed", json!(result.execution.layers_executed)));
    fields.push(("swa_layers", json!(result.execution.swa_layers)));
    fields.push(("csa_layers", json!(result.execution.csa_layers)));
    fields.push(("hca_layers", json!(result.execution.hca_layers)));
    fields.push((
        "hash_router_executions",
        json!(result.execution.hash_routers),
    ));
    fields.push((
        "learned_router_executions",
        json!(result.execution.learned_routers),
    ));
    fields.push((
        "routed_expert_executions",
        json!(result.execution.routed_experts),
    ));
    fields.push((
        "shared_expert_executions",
        json!(result.execution.shared_experts),
    ));
    fields.push(("h2d_bytes", json!(result.execution.h2d_bytes)));
    fields.push(("d2h_bytes", json!(result.execution.d2h_bytes)));
    fields.push(("kernel_launches", json!(result.execution.kernel_launches)));
    fields.push((
        "embedding_digest",
        json!(ffi::text(&result.execution.embedding_digest)),
    ));
    fields.push((
        "routing_digest",
        json!(ffi::text(&result.execution.routing_digest)),
    ));
    fields.push((
        "layer_digest",
        json!(ffi::text(&result.execution.layer_digest)),
    ));
    fields.push((
        "final_expanded_digest",
        json!(ffi::text(&result.execution.final_expanded_digest)),
    ));
    fields.push((
        "normalized_hidden_digest",
        json!(ffi::text(&result.execution.normalized_hidden_digest)),
    ));
    fields.push((
        "persistent_state_digest",
        json!(ffi::text(&result.execution.persistent_state_digest)),
    ));
}

fn transformer_2(result: &raw::yvex_transformer_operator_result, fields: &mut Fields) {
    fields.push((
        "execution_identity",
        json!(ffi::text(&result.execution.execution_identity)),
    ));
    fields.push(("embedding_ready", json!(result.embedding_ready != 0)));
    fields.push((
        "transformer_plan_ready",
        json!(result.transformer_plan_ready != 0),
    ));
    fields.push((
        "transformer_block_ready",
        json!(result.transformer_block_ready != 0),
    ));
    fields.push((
        "transformer_stack_ready",
        json!(result.transformer_stack_ready != 0),
    ));
    fields.push((
        "transformer_final_head_ready",
        json!(result.transformer_final_head_ready != 0),
    ));
    fields.push((
        "transformer_final_norm_ready",
        json!(result.transformer_final_norm_ready != 0),
    ));
    fields.push((
        "transformer_hidden_state_ready",
        json!(result.transformer_hidden_state_ready != 0),
    ));
    fields.push((
        "full_model_prefill_ready",
        json!(result.full_model_prefill_ready != 0),
    ));
    fields.push(("transformer_ready", json!(result.transformer_ready != 0)));
    fields.push((
        "single_token_transformer_component_ready",
        json!(result.single_token_transformer_component_ready != 0),
    ));
    fields.push(("model_decode_ready", json!(result.model_decode_ready != 0)));
    fields.push(("logits_ready", json!(result.logits_ready != 0)));
    fields.push(("sampling_ready", json!(result.sampling_ready != 0)));
    fields.push((
        "tokenizer_runtime_ready",
        json!(result.tokenizer_runtime_ready != 0),
    ));
    fields.push(("generation_ready", json!(result.generation_ready != 0)));
    fields.push((
        "model_behavior_evaluation_ready",
        json!(result.model_behavior_evaluation_ready != 0),
    ));
    fields.push((
        "release_qualification_ready",
        json!(result.release_qualification_ready != 0),
    ));
    fields.push(("reason", json!(ffi::text(&result.reason))));
    fields.push(("completed", json!(result.completed != 0)));
}

fn transformer(result: &raw::yvex_transformer_operator_result) -> Fields {
    let mut fields = Vec::new();
    transformer_0(result, &mut fields);
    transformer_1(result, &mut fields);
    transformer_2(result, &mut fields);
    fields
}

fn decode_0(result: &raw::yvex_decode_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("backend", json!(ffi::text(&result.backend))));
    fields.push(("phase", json!(ffi::text(&result.phase))));
    fields.push((
        "artifact_identity",
        json!(ffi::text(&result.artifact_identity)),
    ));
    fields.push((
        "runtime_binding_identity",
        json!(ffi::text(&result.runtime_binding_identity)),
    ));
    fields.push((
        "transformer_plan_identity",
        json!(ffi::text(&result.transformer_plan_identity)),
    ));
    fields.push(("hidden_width", json!(result.hidden_width)));
    fields.push(("layers", json!(result.layer_count)));
    fields.push((
        "prefill_tokens_committed",
        json!(result.prefill_tokens_committed),
    ));
    fields.push((
        "input_identity",
        json!(ffi::text(&result.decode.input_identity)),
    ));
    fields.push((
        "decode_steps_requested",
        json!(result.decode.requested_steps),
    ));
    fields.push((
        "decode_steps_completed",
        json!(result.decode.completed_steps),
    ));
    fields.push(("partial", json!(result.decode.partial != 0)));
    fields.push((
        "has_incomplete_step",
        json!(result.decode.has_incomplete_step != 0),
    ));
    fields.push((
        "first_incomplete_step",
        json!(result.decode.first_incomplete_step),
    ));
    fields.push((
        "initial_committed_prefix",
        json!(result.decode.initial_committed_prefix),
    ));
    fields.push((
        "final_committed_prefix",
        json!(result.decode.final_committed_prefix),
    ));
}

fn decode_1(result: &raw::yvex_decode_operator_result, fields: &mut Fields) {
    fields.push(("generation_before", json!(result.decode.generation_before)));
    fields.push(("generation_after", json!(result.decode.generation_after)));
    fields.push(("layers_executed", json!(result.decode.layers_executed)));
    fields.push(("swa_layers", json!(result.decode.swa_layers)));
    fields.push(("csa_layers", json!(result.decode.csa_layers)));
    fields.push(("hca_layers", json!(result.decode.hca_layers)));
    fields.push(("hash_router_executions", json!(result.decode.hash_routers)));
    fields.push((
        "learned_router_executions",
        json!(result.decode.learned_routers),
    ));
    fields.push((
        "routed_expert_executions",
        json!(result.decode.routed_experts),
    ));
    fields.push((
        "shared_expert_executions",
        json!(result.decode.shared_experts),
    ));
    fields.push(("h2d_bytes", json!(result.decode.h2d_bytes)));
    fields.push(("d2h_bytes", json!(result.decode.d2h_bytes)));
    fields.push(("kernel_launches", json!(result.decode.kernel_launches)));
    fields.push((
        "aggregate_hidden_digest",
        json!(ffi::text(&result.decode.aggregate_hidden_digest)),
    ));
    fields.push((
        "aggregate_state_digest",
        json!(ffi::text(&result.decode.aggregate_state_digest)),
    ));
    fields.push((
        "decode_execution_identity",
        json!(ffi::text(&result.decode.decode_execution_identity)),
    ));
    fields.push(("decode_step_ready", json!(result.decode_step_ready != 0)));
    fields.push((
        "decode_repeat_ready",
        json!(result.decode_repeat_ready != 0),
    ));
    fields.push((
        "decode_hidden_state_ready",
        json!(result.decode_hidden_state_ready != 0),
    ));
    fields.push((
        "decode_partial_progress_ready",
        json!(result.decode_partial_progress_ready != 0),
    ));
}

fn decode_2(result: &raw::yvex_decode_operator_result, fields: &mut Fields) {
    fields.push((
        "moe_decode_composed",
        json!(result.moe_decode_composed != 0),
    ));
    fields.push(("model_decode_ready", json!(result.model_decode_ready != 0)));
    fields.push(("logits_ready", json!(result.logits_ready != 0)));
    fields.push(("output_head_ready", json!(result.output_head_ready != 0)));
    fields.push(("sampling_ready", json!(result.sampling_ready != 0)));
    fields.push((
        "tokenizer_runtime_ready",
        json!(result.tokenizer_runtime_ready != 0),
    ));
    fields.push(("generation_ready", json!(result.generation_ready != 0)));
    fields.push((
        "model_behavior_evaluation_ready",
        json!(result.model_behavior_evaluation_ready != 0),
    ));
    fields.push((
        "full_model_benchmark_ready",
        json!(result.full_model_benchmark_ready != 0),
    ));
    fields.push((
        "release_qualification_ready",
        json!(result.release_qualification_ready != 0),
    ));
    fields.push(("reason", json!(ffi::text(&result.reason))));
    fields.push(("completed", json!(result.completed != 0)));
}

fn decode(result: &raw::yvex_decode_operator_result) -> Fields {
    let mut fields = Vec::new();
    decode_0(result, &mut fields);
    decode_1(result, &mut fields);
    decode_2(result, &mut fields);
    fields
}

fn logits_0(result: &raw::yvex_logits_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("backend", json!(ffi::text(&result.backend))));
    fields.push((
        "artifact_identity",
        json!(ffi::text(&result.artifact_identity)),
    ));
    fields.push((
        "runtime_binding_identity",
        json!(ffi::text(&result.runtime_binding_identity)),
    ));
    fields.push((
        "transformer_plan_identity",
        json!(ffi::text(&result.transformer_plan_identity)),
    ));
    fields.push((
        "output_head_plan_identity",
        json!(ffi::text(&result.plan.output_head_plan_identity)),
    ));
    fields.push((
        "output_head_tensor_id",
        json!(result.plan.output_head_tensor_id),
    ));
    fields.push(("output_head_qtype", json!(result.plan.qtype)));
    fields.push(("hidden_width", json!(result.plan.hidden_width)));
    fields.push(("vocabulary_size", json!(result.plan.vocabulary_size)));
    fields.push(("output_head_rows", json!(result.plan.row_count)));
    fields.push(("output_head_columns", json!(result.plan.row_width)));
    fields.push(("output_head_row_bytes", json!(result.plan.row_bytes)));
    fields.push((
        "output_head_encoded_bytes",
        json!(result.plan.encoded_bytes),
    ));
    fields.push((
        "output_head_host_resident_bytes",
        json!(result.output_head_host_bytes),
    ));
    fields.push((
        "output_head_device_resident_bytes",
        json!(result.output_head_device_bytes),
    ));
    fields.push((
        "output_head_upload_bytes",
        json!(result.output_head_upload_bytes),
    ));
}

fn logits_1(result: &raw::yvex_logits_operator_result, fields: &mut Fields) {
    fields.push((
        "output_head_upload_count",
        json!(result.output_head_upload_count),
    ));
    fields.push(("prefill_logits_rows", json!(result.prefill_logits_rows)));
    fields.push(("decode_logits_rows", json!(result.decode_logits_rows)));
    fields.push((
        "logits_rows_requested",
        json!(result.execution.requested_rows),
    ));
    fields.push((
        "logits_rows_completed",
        json!(result.execution.completed_rows),
    ));
    fields.push((
        "first_incomplete_row",
        json!(result.execution.first_incomplete_row),
    ));
    fields.push((
        "final_source_position",
        json!(result.execution.final_source_position),
    ));
    fields.push(("partial", json!(result.execution.partial != 0)));
    fields.push((
        "aggregate_logits_digest",
        json!(ffi::text(&result.execution.aggregate_logits_digest)),
    ));
    fields.push((
        "logits_execution_identity",
        json!(ffi::text(&result.execution.execution_identity)),
    ));
    fields.push((
        "output_head_binding_ready",
        json!(result.output_head_binding_ready != 0),
    ));
    fields.push((
        "output_head_residency_ready",
        json!(result.output_head_residency_ready != 0),
    ));
    fields.push(("logits_cpu_ready", json!(result.logits_cpu_ready != 0)));
    fields.push(("logits_cuda_ready", json!(result.logits_cuda_ready != 0)));
    fields.push((
        "logits_prefill_ready",
        json!(result.logits_prefill_ready != 0),
    ));
    fields.push((
        "logits_decode_ready",
        json!(result.logits_decode_ready != 0),
    ));
    fields.push((
        "logits_full_vocabulary_ready",
        json!(result.logits_full_vocabulary_ready != 0),
    ));
    fields.push((
        "logits_hidden_contract_ready",
        json!(result.logits_hidden_contract_ready != 0),
    ));
    fields.push((
        "logits_partial_progress_ready",
        json!(result.logits_partial_progress_ready != 0),
    ));
    fields.push(("logits_ready", json!(result.logits_ready != 0)));
}

fn logits_2(result: &raw::yvex_logits_operator_result, fields: &mut Fields) {
    fields.push(("sampling_ready", json!(result.sampling_ready != 0)));
    fields.push((
        "tokenizer_runtime_ready",
        json!(result.tokenizer_runtime_ready != 0),
    ));
    fields.push(("generation_ready", json!(result.generation_ready != 0)));
    fields.push((
        "model_behavior_evaluation_ready",
        json!(result.model_behavior_evaluation_ready != 0),
    ));
    fields.push((
        "full_model_benchmark_ready",
        json!(result.full_model_benchmark_ready != 0),
    ));
    fields.push((
        "release_qualification_ready",
        json!(result.release_qualification_ready != 0),
    ));
    fields.push(("reason", json!(ffi::text(&result.reason))));
    fields.push(("completed", json!(result.completed != 0)));
}

fn logits(result: &raw::yvex_logits_operator_result) -> Fields {
    let mut fields = Vec::new();
    logits_0(result, &mut fields);
    logits_1(result, &mut fields);
    logits_2(result, &mut fields);
    fields
}

fn sampling_0(result: &raw::yvex_sampling_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("logits_backend", json!(ffi::text(&result.logits_backend))));
    fields.push((
        "sampling_execution_kind",
        json!(ffi::text(&result.sampling_execution_kind)),
    ));
    fields.push(("samples", json!(result.sample_count)));
    fields.push(("prefill_samples", json!(result.prefill_samples)));
    fields.push(("decode_samples", json!(result.decode_samples)));
    fields.push(("strategy", json!(ffi::text(&result.strategy))));
    fields.push(("temperature", json!(result.policy.temperature)));
    fields.push(("top_k", json!(result.policy.top_k)));
    fields.push(("top_p", json!(result.policy.top_p)));
    fields.push(("min_p", json!(result.policy.min_p)));
    fields.push(("typical_p", json!(result.policy.typical_p)));
    fields.push(("seed", json!(result.policy.seed)));
    fields.push(("rng_algorithm", json!(result.policy.rng_algorithm)));
    fields.push(("rng_version", json!(result.policy.rng_version)));
    fields.push((
        "filter_order_version",
        json!(result.policy.filter_order_version),
    ));
    fields.push((
        "policy_identity",
        json!(ffi::text(&result.policy.policy_identity)),
    ));
}

fn sampling_1(result: &raw::yvex_sampling_operator_result, fields: &mut Fields) {
    fields.push((
        "sampling_requested_samples",
        json!(result.execution.requested_samples),
    ));
    fields.push((
        "sampling_completed_samples",
        json!(result.execution.completed_samples),
    ));
    fields.push((
        "sampling_first_incomplete_sample",
        json!(result.execution.first_incomplete_sample),
    ));
    fields.push(("sampling_partial", json!(result.execution.partial != 0)));
    fields.push((
        "sampling_identity",
        json!(ffi::text(&result.execution.aggregate_sampling_identity)),
    ));
    fields.push(("workspace_bytes", json!(result.workspace_bytes)));
    fields.push(("workspace_generation", json!(result.workspace_generation)));
    fields.push((
        "warm_sampling_allocations",
        json!(result.warm_workspace_allocations),
    ));
    fields.push((
        "sampling_source_contract_ready",
        json!(result.sampling_source_contract_ready != 0),
    ));
    fields.push((
        "sampling_policy_ready",
        json!(result.sampling_policy_ready != 0),
    ));
    fields.push((
        "sampling_greedy_ready",
        json!(result.sampling_greedy_ready != 0),
    ));
    fields.push((
        "sampling_temperature_ready",
        json!(result.sampling_temperature_ready != 0),
    ));
    fields.push((
        "sampling_top_k_ready",
        json!(result.sampling_top_k_ready != 0),
    ));
    fields.push((
        "sampling_top_p_ready",
        json!(result.sampling_top_p_ready != 0),
    ));
    fields.push((
        "sampling_min_p_ready",
        json!(result.sampling_min_p_ready != 0),
    ));
    fields.push((
        "sampling_typical_ready",
        json!(result.sampling_typical_ready != 0),
    ));
    fields.push((
        "sampling_stochastic_ready",
        json!(result.sampling_stochastic_ready != 0),
    ));
    fields.push((
        "sampling_seed_reproducibility_ready",
        json!(result.sampling_seed_reproducibility_ready != 0),
    ));
    fields.push((
        "sampling_real_logits_ready",
        json!(result.sampling_real_logits_ready != 0),
    ));
    fields.push((
        "sampling_partial_progress_ready",
        json!(result.sampling_partial_progress_ready != 0),
    ));
}

fn sampling_2(result: &raw::yvex_sampling_operator_result, fields: &mut Fields) {
    fields.push(("sampling_ready", json!(result.sampling_ready != 0)));
    fields.push((
        "persistent_state_unchanged",
        json!(result.persistent_state_unchanged != 0),
    ));
    fields.push(("token_append_ready", json!(result.token_append_ready != 0)));
    fields.push((
        "tokenizer_runtime_ready",
        json!(result.tokenizer_runtime_ready != 0),
    ));
    fields.push(("eos_policy_ready", json!(result.eos_policy_ready != 0)));
    fields.push(("stop_policy_ready", json!(result.stop_policy_ready != 0)));
    fields.push((
        "detokenization_ready",
        json!(result.detokenization_ready != 0),
    ));
    fields.push(("generation_ready", json!(result.generation_ready != 0)));
    fields.push((
        "device_sampling_ready",
        json!(result.device_sampling_ready != 0),
    ));
    fields.push((
        "model_behavior_evaluation_ready",
        json!(result.model_behavior_evaluation_ready != 0),
    ));
    fields.push((
        "full_model_benchmark_ready",
        json!(result.full_model_benchmark_ready != 0),
    ));
    fields.push((
        "release_qualification_ready",
        json!(result.release_qualification_ready != 0),
    ));
    fields.push(("reason", json!(ffi::text(&result.reason))));
    fields.push(("completed", json!(result.completed != 0)));
}

fn sampling(result: &raw::yvex_sampling_operator_result) -> Fields {
    let mut fields = Vec::new();
    sampling_0(result, &mut fields);
    sampling_1(result, &mut fields);
    sampling_2(result, &mut fields);
    fields
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn dynamic_row_projection_retains_published_precision_and_identities() {
        let step = raw::yvex_runtime_decode_step_result {
            step_ordinal: 3,
            token_id: 42,
            position_before: 9,
            position_after: 10,
            ..Default::default()
        };
        assert_eq!(decode_step(&step)["ordinal"], 3);
        assert_eq!(decode_step(&step)["position_after"], 10);
        assert_eq!(nine_digits(1.2345678), 1.23456776);
        assert_eq!(significant(0.1, 17), "0.10000000000000001");
        assert_eq!(significant(1.0, 17), "1");
        assert_eq!(significant(1e17, 17), "1e+17");
        assert_eq!(significant(-1e-5, 9), "-1e-05");
        let values = vec![
            ("command", json!("execution")),
            ("reason", json!("unsafe\u{1b}[31m<&\"")),
            ("steps", json!([decode_step(&step)])),
        ];
        let json = present(&values, "json", 40, true).unwrap();
        assert!(!json.contains('\u{1b}'));
        let csv = present(&values, "csv", 40, false).unwrap();
        assert!(csv.contains("\"step.0\",\"token=42 position=9:10"));
        assert!(
            !present(&values, "normal", 40, false)
                .unwrap()
                .contains('\u{1b}')
        );
    }
}
