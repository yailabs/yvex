// Engineering generation publishes native token, profile and roofline evidence without deriving support.
use crate::ffi::{self, generation, raw};
use serde_json::{Value, json};
type Fields = Vec<(&'static str, Value)>;
type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

fn decimal9(value: f64) -> f64 {
    format!("{value:.9}").parse().expect("native scalar")
}

fn counts(run: &raw::yvex_runtime_generation_result) -> Fields {
    vec![
        ("prompt_tokens", json!(run.prompt_token_count)),
        ("prefill_chunks", json!(run.prefill_chunk_count)),
        ("sampled_tokens", json!(run.sampled_token_count)),
        (
            "model_committed_tokens",
            json!(run.model_committed_token_count),
        ),
        ("decode_steps", json!(run.decode_step_count)),
        ("logits_projections", json!(run.logits_projection_count)),
        ("sampling_draws", json!(run.sampling_draw_count)),
        ("generated_text_bytes", json!(run.generated_text_bytes)),
        ("draft_cycles", json!(run.draft_cycle_count)),
        ("draft_forwards", json!(run.draft_forward_count)),
        ("proposed_tokens", json!(run.proposed_token_count)),
        (
            "selected_verification_tokens",
            json!(run.selected_verification_token_count),
        ),
        ("target_verifications", json!(run.target_verification_count)),
        (
            "accepted_draft_tokens",
            json!(run.accepted_draft_token_count),
        ),
        (
            "rejected_draft_tokens",
            json!(run.rejected_draft_token_count),
        ),
        (
            "discarded_draft_tokens",
            json!(run.discarded_draft_token_count),
        ),
        (
            "target_correction_or_bonus_tokens",
            json!(run.target_correction_or_bonus_token_count),
        ),
        (
            "maximum_accepted_prefix",
            json!(run.maximum_accepted_prefix),
        ),
        (
            "mean_accepted_prefix",
            json!(decimal9(run.mean_accepted_prefix)),
        ),
        ("confidence_logit_count", json!(run.confidence_logit_count)),
        (
            "confidence_logit_minimum",
            json!(decimal9(run.confidence_logit_minimum)),
        ),
        (
            "confidence_logit_maximum",
            json!(decimal9(run.confidence_logit_maximum)),
        ),
        (
            "confidence_logit_mean",
            json!(decimal9(run.confidence_logit_mean)),
        ),
        ("draft_seconds", json!(decimal9(run.draft_ns as f64 / 1e9))),
        (
            "verification_seconds",
            json!(decimal9(run.verification_ns as f64 / 1e9)),
        ),
        (
            "speculative_commit_seconds",
            json!(decimal9(run.speculative_commit_ns as f64 / 1e9)),
        ),
        (
            "effective_committed_tokens_per_second",
            json!(decimal9(run.effective_committed_tokens_per_second)),
        ),
        ("final_position", json!(run.final_position)),
        ("final_generation", json!(run.final_persistent_generation)),
    ]
}

fn profile(value: &raw::yvex_runtime_profile_record) -> Value {
    let (phases, counters) = generation::profile_names();
    let phases = phases
        .into_iter()
        .zip(value.phase_ns)
        .map(|(k, v)| (k, json!(v)))
        .collect::<serde_json::Map<_, _>>();
    let counters = counters
        .into_iter()
        .zip(value.counters)
        .map(|(k, v)| (k, json!(v)))
        .collect::<serde_json::Map<_, _>>();
    json!({"schema": value.schema_version, "mode": generation::profile_mode(value.mode),
        "identity": ffi::text(&value.profile_identity), "phases_ns": phases, "counters": counters})
}

fn roofline_phase(name: &str, phase: &raw::yvex_execution_phase_roofline) -> Value {
    let value = &phase.measurement;
    json!({"name": name, "available": phase.available != 0, "roofline_available": phase.roofline_available != 0,
        "fact_mask": value.fact_mask, "missing_fact_mask": phase.missing_fact_mask,
        "active_weight_bytes": value.active_weight_bytes, "state_bytes": value.state_bytes,
        "activation_bytes": value.activation_bytes, "temporary_bytes": value.temporary_bytes,
        "h2d_bytes": value.h2d_bytes, "d2h_bytes": value.d2h_bytes, "d2d_bytes": value.d2d_bytes,
        "kernel_count": value.kernel_count, "synchronization_count": value.synchronization_count,
        "occupancy_parts_per_million": value.occupancy_parts_per_million,
        "minimum_memory_time_ns": phase.minimum_memory_time_ns, "duration_ns": value.measured_duration_ns,
        "work_units": value.work_units, "committed_tokens": value.committed_tokens,
        "active_device_bytes": phase.active_device_bytes, "transfer_bytes": phase.transfer_bytes,
        "measured_bytes_per_second": phase.measured_bytes_per_second,
        "roofline_utilization_ppm": phase.roofline_utilization_parts_per_million,
        "optimization_headroom_ns": phase.optimization_headroom_ns, "priority": phase.optimization_priority})
}

fn roofline(evidence: &raw::yvex_runtime_generation_evidence) -> Result<Value> {
    if evidence.roofline_available == 0 {
        return Ok(Value::Null);
    }
    let value = &evidence.roofline;
    let names = [
        "prefill-layer",
        "decode-layer",
        "verify-sweep",
        "draft-sweep",
        "output-head",
        "state-promotion",
        "batched-decode",
    ];
    if value.phase_count > value.phases.len() as u64 || value.phase_count > names.len() as u64 {
        return Err(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "generation.projection".into(),
            message: "native roofline exceeds its published phase bound".into(),
        }
        .into());
    }
    let phases = names
        .iter()
        .zip(&value.phases)
        .take(value.phase_count as usize)
        .map(|(name, phase)| roofline_phase(name, phase))
        .collect::<Vec<_>>();
    Ok(
        json!({"schema": value.schema_version, "identity": ffi::text(&value.identity),
        "measured_phase_mask": value.measured_phase_mask, "missing_phase_mask": value.missing_phase_mask,
        "rooflined_phase_mask": value.rooflined_phase_mask, "priority_provisional": value.priority_provisional != 0,
        "phases": phases}),
    )
}

fn token(value: &raw::yvex_runtime_generation_token_result) -> Value {
    json!({"ordinal": value.ordinal, "token_id": value.sampled_token_id, "decode_input_id": value.decode_input_token_id,
        "decode_submitted": value.decode_submitted != 0, "model_committed": value.model_committed != 0,
        "terminal": value.terminal != 0, "suppressed": value.suppressed != 0, "position_before": value.position_before,
        "position_after": value.position_after, "text_bytes": value.text_byte_count,
        "identity": ffi::text(&value.token_step_identity)})
}

pub(crate) fn fields(
    result: &raw::yvex_generation_operator_result,
    tokens: &[raw::yvex_runtime_generation_token_result],
) -> Result<Fields> {
    let run = &result.execution;
    if [
        run.mean_accepted_prefix,
        run.confidence_logit_minimum,
        run.confidence_logit_maximum,
        run.confidence_logit_mean,
        run.effective_committed_tokens_per_second,
    ]
    .iter()
    .any(|v| !v.is_finite())
    {
        return Err(ffi::Error {
            code: raw::yvex_status_YVEX_ERR_STATE,
            owner: "generation.projection".into(),
            message: "native generation evidence contains non-finite scalars".into(),
        }
        .into());
    }
    let mut fields = vec![
        ("command", json!(ffi::text(&result.command))),
        ("status", json!(ffi::text(&result.status))),
        ("reason", json!(ffi::text(&result.reason))),
        ("target", json!(ffi::text(&result.target))),
        ("family", json!(ffi::text(&result.family))),
        ("backend", json!(ffi::text(&result.backend))),
        (
            "host_workspace",
            if result.host_workspace_available != 0 {
                let w = &result.host_workspace;
                json!({"capacity_bytes": w.capacity, "used_bytes": w.used,
                    "peak_used_bytes": w.peak, "allocation_count": w.allocation_count,
                    "attached": w.attached != 0, "owned": w.owned != 0, "pinned": w.pinned != 0,
                    "scope": "backend staging after execution, before cleanup; not total physical memory"})
            } else {
                Value::Null
            },
        ),
        // Copy the admitted C plan. Do not infer lineage from command names,
        // artifact paths or a separately configured benchmark profile.
        (
            "runtime_model_identity",
            json!(ffi::text(&result.plan.runtime_model_identity)),
        ),
        (
            "runtime_binding_identity",
            json!(ffi::text(&result.plan.runtime_binding_identity)),
        ),
        (
            "tokenizer_plan_identity",
            json!(ffi::text(&result.plan.tokenizer_plan_identity)),
        ),
        (
            "prompt_policy_identity",
            json!(ffi::text(&result.plan.prompt_policy_identity)),
        ),
        (
            "kernel_bundle_identity",
            json!(ffi::text(&result.plan.kernel_bundle_identity)),
        ),
        (
            "execution_profile_identity",
            json!(ffi::text(&result.plan.execution_profile_identity)),
        ),
        ("context_capacity", json!(result.plan.context_capacity)),
        (
            "prefill_chunk_tokens",
            json!(result.plan.prefill_chunk_tokens),
        ),
        ("maximum_new_tokens", json!(result.plan.maximum_new_tokens)),
        ("execution_class", json!(result.plan.execution_class)),
        ("evidence_profile", json!(result.plan.evidence_profile)),
        (
            "generation_plan_identity",
            json!(ffi::text(&result.plan.generation_plan_identity)),
        ),
        (
            "generation_execution_identity",
            json!(ffi::text(&run.generation_execution_identity)),
        ),
        ("prompt_identity", json!(ffi::text(&run.prompt_identity))),
        (
            "prompt_token_identity",
            json!(ffi::text(&run.prompt_token_identity)),
        ),
        (
            "execution_mode",
            json!(if run.execution_mode
                == raw::yvex_runtime_generation_mode_YVEX_GENERATION_MODE_SPECULATIVE
            {
                "dspark"
            } else {
                "target-only"
            }),
        ),
        (
            "speculation_policy_identity",
            json!(ffi::text(&run.speculation_policy_identity)),
        ),
    ];
    fields.extend(counts(run));
    fields.extend([
        (
            "generated_text_digest",
            json!(ffi::text(&run.generated_text_digest)),
        ),
        (
            "persistent_state_digest",
            json!(ffi::text(&run.final_persistent_state_digest)),
        ),
        (
            "stop_reason",
            json!(generation::stop_reason(run.stop_reason)),
        ),
        ("generation_ready", json!(result.generation_ready != 0)),
        ("cli_generate_ready", json!(result.cli_generate_ready != 0)),
        ("profile", profile(&result.evidence.profile)),
        ("roofline", roofline(&result.evidence)?),
        (
            "generated_tokens",
            json!(tokens.iter().map(token).collect::<Vec<_>>()),
        ),
    ]);
    Ok(fields)
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn admitted_plan_geometry_is_copied_not_reconstructed() {
        let mut result = raw::yvex_generation_operator_result::default();
        result.plan.context_capacity = 4096;
        result.plan.prefill_chunk_tokens = 64;
        result.plan.maximum_new_tokens = 256;
        ffi::put_text(&mut result.plan.runtime_binding_identity, "native-binding").unwrap();
        let projected = fields(&result, &[]).unwrap();
        for (name, expected) in [
            ("context_capacity", json!(4096)),
            ("prefill_chunk_tokens", json!(64)),
            ("maximum_new_tokens", json!(256)),
            ("runtime_binding_identity", json!("native-binding")),
        ] {
            assert_eq!(
                projected.iter().find(|(key, _)| *key == name).unwrap().1,
                expected
            );
        }
    }
    #[test]
    fn unknown_and_unavailable_measurement_is_not_a_zero_cost_claim() {
        let mut evidence = raw::yvex_runtime_generation_evidence::default();
        assert!(roofline(&evidence).unwrap().is_null());
        evidence.roofline_available = 1;
        evidence.roofline.phase_count = u64::MAX;
        assert!(roofline(&evidence).is_err());
        let result = raw::yvex_generation_operator_result::default();
        let fields = fields(&result, &[]).unwrap();
        assert!(
            fields
                .iter()
                .find(|(k, _)| *k == "host_workspace")
                .unwrap()
                .1
                .is_null()
        );
        assert_eq!(
            fields
                .iter()
                .find(|(k, _)| *k == "prompt_token_identity")
                .unwrap()
                .1,
            ""
        );
        assert_eq!(
            fields
                .iter()
                .find(|(k, _)| *k == "generation_ready")
                .unwrap()
                .1,
            false
        );
        assert_eq!(
            fields
                .iter()
                .find(|(k, _)| *k == "generated_tokens")
                .unwrap()
                .1,
            json!([])
        );
    }

    #[test]
    fn staging_capacity_and_actual_high_water_are_different_facts() {
        let mut result = raw::yvex_generation_operator_result {
            host_workspace_available: 1,
            ..Default::default()
        };
        result.host_workspace.capacity = 1024;
        result.host_workspace.used = 16;
        result.host_workspace.peak = 32;
        result.host_workspace.pinned = 1;
        let fields = fields(&result, &[]).unwrap();
        let value = &fields
            .iter()
            .find(|(k, _)| *k == "host_workspace")
            .unwrap()
            .1;
        assert_eq!(value["capacity_bytes"], 1024);
        assert_eq!(value["used_bytes"], 16);
        assert_eq!(value["peak_used_bytes"], 32);
        assert_eq!(value["pinned"], true);
    }
}
