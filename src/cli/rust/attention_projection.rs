// Complete machine projections of the native attention result; no human C renderer crosses FFI.
use crate::ffi::{self, raw};
use serde_json::{Value, json};

type Fields = Vec<(&'static str, Value)>;

fn base(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("command", json!(ffi::text(&result.command))));
    fields.push(("status", json!(ffi::text(&result.status))));
    fields.push(("target", json!(ffi::text(&result.target))));
    fields.push(("backend", json!(ffi::text(&result.backend))));
    fields.push(("scope", json!(ffi::text(&result.scope))));
    fields.push(("operation_scope", json!(ffi::text(&result.operation_scope))));
    fields.push(("phase", json!(ffi::text(&result.phase))));
    fields.push(("trace_policy", json!(ffi::text(&result.trace_policy))));
    fields.push(("requested_mode", json!(ffi::text(&result.requested_mode))));
    fields.push(("selected_mode", json!(ffi::text(&result.selected_mode))));
    fields.push((
        "selection_reason",
        json!(ffi::text(&result.selection_reason)),
    ));
    fields.push(("artifact_path", json!(ffi::text(&result.artifact_path))));
    fields.push((
        "runtime_binding_path",
        json!(ffi::text(&result.runtime_binding_path)),
    ));
}

fn target(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("family", json!(ffi::text(&result.family))));
    fields.push(("input_class", json!(ffi::text(&result.input_class))));
}

fn admission(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("execution_class", json!(ffi::text(&result.execution_class))));
    fields.push(("weights_class", json!(ffi::text(&result.weights_class))));
    fields.push((
        "artifact_identity",
        json!(ffi::text(&result.artifact_identity)),
    ));
    fields.push((
        "runtime_binding_identity",
        json!(ffi::text(&result.runtime_binding_identity)),
    ));
    fields.push((
        "runtime_model_identity",
        json!(ffi::text(&result.runtime_model_identity)),
    ));
    fields.push((
        "activation_input_identity",
        json!(ffi::text(&result.activation_input_identity)),
    ));
    fields.push(("artifact_bytes_hashed", json!(result.artifact_bytes_hashed)));
    fields.push((
        "artifact_identity_verified",
        json!(result.artifact_identity_verified != 0),
    ));
    fields.push((
        "materialization_identity",
        json!(ffi::text(&result.materialization_identity)),
    ));
    fields.push((
        "logical_model_identity",
        json!(ffi::text(&result.logical_model_identity)),
    ));
    fields.push((
        "runtime_numeric_identity",
        json!(ffi::text(&result.runtime_numeric_identity)),
    ));
    fields.push((
        "runtime_descriptor_identity",
        json!(ffi::text(&result.runtime_descriptor_identity)),
    ));
    fields.push((
        "attention_plan_identity",
        json!(ffi::text(&result.attention_plan_identity)),
    ));
    fields.push((
        "semantic_graph_identity",
        json!(ffi::text(&result.semantic_graph_identity)),
    ));
    fields.push((
        "executable_graph_identity",
        json!(ffi::text(&result.executable_graph_identity)),
    ));
    fields.push(("main_layers_total", json!(result.main_layers_total)));
    fields.push(("bindings_total", json!(result.bindings_total)));
    fields.push((
        "attention_execution_supported",
        json!(result.capabilities.attention_core_ready != 0),
    ));
    fields.push((
        "attention_cuda_execution_ready",
        json!(result.attention_cuda_execution_ready != 0),
    ));
    fields.push((
        "attention_state_delta_ready",
        json!(result.capabilities.attention_state_delta_ready != 0),
    ));
    fields.push((
        "attention_weight_residency_ready",
        json!(result.capabilities.attention_weight_residency_ready != 0),
    ));
    fields.push((
        "attention_trace_ready",
        json!(result.capabilities.attention_trace_ready != 0),
    ));
    fields.push((
        "attention_profile_ready",
        json!(result.capabilities.attention_profile_ready != 0),
    ));
    fields.push((
        "attention_benchmark_ready",
        json!(result.capabilities.attention_benchmark_ready != 0),
    ));
}

fn capability(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "attention_semantics_ready",
        json!(result.capabilities.attention_semantics_ready != 0),
    ));
    fields.push((
        "attention_core_ready",
        json!(result.capabilities.attention_core_ready != 0),
    ));
    fields.push((
        "attention_envelope_ready",
        json!(result.capabilities.attention_envelope_ready != 0),
    ));
    fields.push((
        "cpu_prefill_eager_ready",
        json!(result.capabilities.cpu_prefill_eager_ready != 0),
    ));
    fields.push((
        "cpu_decode_eager_ready",
        json!(result.capabilities.cpu_decode_eager_ready != 0),
    ));
    fields.push((
        "cuda_prefill_eager_ready",
        json!(result.capabilities.cuda_prefill_eager_ready != 0),
    ));
    fields.push((
        "cuda_decode_eager_ready",
        json!(result.capabilities.cuda_decode_eager_ready != 0),
    ));
    fields.push((
        "cuda_prefill_piecewise_graph_ready",
        json!(result.capabilities.cuda_prefill_piecewise_graph_ready != 0),
    ));
    fields.push((
        "cuda_decode_piecewise_graph_ready",
        json!(result.capabilities.cuda_decode_piecewise_graph_ready != 0),
    ));
    fields.push((
        "cuda_prefill_full_graph_ready",
        json!(result.capabilities.cuda_prefill_full_graph_ready != 0),
    ));
    fields.push((
        "cuda_decode_full_graph_ready",
        json!(result.capabilities.cuda_decode_full_graph_ready != 0),
    ));
    fields.push((
        "attention_workspace_ready",
        json!(result.capabilities.attention_workspace_ready != 0),
    ));
    fields.push((
        "attention_operator_ready",
        json!(result.capabilities.attention_operator_ready != 0),
    ));
    fields.push((
        "mixed_attention_ready",
        json!(result.capabilities.mixed_attention_ready != 0),
    ));
    fields.push((
        "speculative_attention_ready",
        json!(result.capabilities.speculative_attention_ready != 0),
    ));
}

fn execution(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "execution_descriptor_identity",
        json!(ffi::text(&result.execution_descriptor_identity)),
    ));
    fields.push((
        "attention_execution_identity",
        json!(ffi::text(&result.probe.attention_execution_identity)),
    ));
    fields.push(("layers_executed", json!(result.probe.layers_executed)));
    fields.push(("bindings_executed", json!(result.probe.bindings_executed)));
    fields.push((
        "swa_layers_executed",
        json!(result.probe.swa_layers_executed),
    ));
    fields.push((
        "csa_layers_executed",
        json!(result.probe.csa_layers_executed),
    ));
    fields.push((
        "hca_layers_executed",
        json!(result.probe.hca_layers_executed),
    ));
    fields.push(("topk_selected", json!(result.probe.topk_selected)));
    fields.push(("hca_ratio", json!(result.probe.hca_ratio)));
    fields.push(("warmup_count", json!(result.warmup_count)));
    fields.push(("repeat_count", json!(result.repeat_count)));
    fields.push((
        "tensor_output_digest",
        json!(ffi::text(&result.probe.tensor_output_digest)),
    ));
    fields.push((
        "state_delta_digest",
        json!(ffi::text(&result.probe.state_delta_digest)),
    ));
    fields.push((
        "execution_evidence_digest",
        json!(ffi::text(&result.execution_evidence_digest)),
    ));
    fields.push((
        "execution_identity",
        json!(ffi::text(&result.execution_identity)),
    ));
    fields.push((
        "execution_dispatch_count",
        json!(result.execution_dispatch_count),
    ));
    fields.push(("prefill_chunk_count", json!(result.prefill_chunk_count)));
    fields.push(("committed_prefix", json!(result.committed_prefix)));
    fields.push(("trace_stage_count", json!(result.trace_stage_count)));
    fields.push(("trace_value_count", json!(result.trace_value_count)));
}

fn state(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "state_layout_identity",
        json!(ffi::text(&result.state_layout_identity)),
    ));
    fields.push((
        "state_content_identity",
        json!(ffi::text(&result.state_content_identity)),
    ));
    fields.push((
        "state_residency_identity",
        json!(ffi::text(&result.state_residency_identity)),
    ));
    fields.push(("state_layer_count", json!(result.state_layer_count)));
    fields.push((
        "state_prepared_layer_count",
        json!(result.state_prepared_layer_count),
    ));
    fields.push(("state_allocated_bytes", json!(result.state_allocated_bytes)));
    fields.push(("state_capacity", json!(result.state_capacity)));
    fields.push((
        "state_committed_sequence_length",
        json!(result.state_committed_sequence_length),
    ));
    fields.push(("state_next_position", json!(result.state_next_position)));
    fields.push(("state_generation", json!(result.state_generation)));
    fields.push((
        "state_residency_generation",
        json!(result.state_residency_generation),
    ));
    fields.push(("state_device_bytes", json!(result.state_device_bytes)));
    fields.push(("state_upload_bytes", json!(result.state_upload_bytes)));
    fields.push(("state_upload_count", json!(result.state_upload_count)));
    fields.push(("state_commit_count", json!(result.state_commit_count)));
    fields.push(("state_abort_count", json!(result.state_abort_count)));
    fields.push((
        "state_cancellation_count",
        json!(result.state_cancellation_count),
    ));
    fields.push(("state_reset_count", json!(result.state_reset_count)));
    fields.push(("state_sealed", json!(result.state_sealed != 0)));
    fields.push(("state_persistent", json!(result.state_persistent != 0)));
    fields.push((
        "state_position_consistent",
        json!(result.state_position_consistent != 0),
    ));
    fields.push(("state_cuda_ready", json!(result.state_cuda_ready != 0)));
    fields.push((
        "state_transaction_active",
        json!(result.state_transaction_active != 0),
    ));
    fields.push((
        "state_validation_passed",
        json!(result.state_validation_passed != 0),
    ));
    fields.push((
        "state_read_after_write_verified",
        json!(result.state_read_after_write_verified != 0),
    ));
    fields.push((
        "state_clear_reuse_verified",
        json!(result.state_clear_reuse_verified != 0),
    ));
}

fn runtime(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("artifact_hash_passes", json!(result.artifact_hash_passes)));
    fields.push((
        "warm_artifact_hash_passes",
        json!(result.warm_artifact_hash_passes),
    ));
    fields.push((
        "runtime_source_headers_read",
        json!(result.runtime_source_headers_read),
    ));
    fields.push((
        "runtime_source_payload_bytes_read",
        json!(result.runtime_source_payload_bytes_read),
    ));
    fields.push((
        "runtime_transform_plans_built",
        json!(result.runtime_transform_plans_built),
    ));
    fields.push((
        "runtime_quant_plans_built",
        json!(result.runtime_quant_plans_built),
    ));
    fields.push((
        "runtime_writer_plans_built",
        json!(result.runtime_writer_plans_built),
    ));
}

fn timing(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "artifact_open_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_ARTIFACT_OPEN as usize]
        ),
    ));
    fields.push((
        "artifact_hash_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_ARTIFACT_HASH as usize]
        ),
    ));
    fields.push((
        "artifact_admission_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_ARTIFACT_ADMISSION
                    as usize]
        ),
    ));
    fields.push((
        "binding_open_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_BINDING_OPEN as usize]
        ),
    ));
    fields.push((
        "materialization_open_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_MATERIALIZATION_OPEN
                    as usize]
        ),
    ));
    fields.push((
        "runtime_model_seal_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_MODEL_SEAL as usize]
        ),
    ));
    fields.push((
        "resident_weight_prepare_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_RESIDENCY as usize]
        ),
    ));
    fields.push((
        "backend_open_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_BACKEND_OPEN as usize]
        ),
    ));
    fields.push((
        "workspace_prepare_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_WORKSPACE_PREPARE
                    as usize]
        ),
    ));
    fields.push((
        "graph_warmup_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_GRAPH_WARMUP as usize]
        ),
    ));
    fields.push((
        "graph_capture_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_GRAPH_CAPTURE as usize]
        ),
    ));
    fields.push((
        "graph_instantiate_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_GRAPH_INSTANTIATE
                    as usize]
        ),
    ));
    fields.push((
        "execution_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_EXECUTION as usize]
        ),
    ));
    fields.push((
        "publication_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_PUBLICATION as usize]
        ),
    ));
    fields.push((
        "cleanup_seconds",
        json!(
            result.lifecycle_seconds
                [raw::yvex_runtime_lifecycle_phase_YVEX_RUNTIME_LIFECYCLE_CLEANUP as usize]
        ),
    ));
    fields.push(("total_seconds", json!(result.total_seconds)));
}

fn residency(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "residency_identity",
        json!(ffi::text(&result.residency_identity)),
    ));
    fields.push((
        "workspace_identity",
        json!(ffi::text(&result.workspace_identity)),
    ));
    fields.push((
        "resident_binding_count",
        json!(result.resident_binding_count),
    ));
    fields.push((
        "resident_encoded_bytes",
        json!(result.resident_encoded_bytes),
    ));
    fields.push(("host_resident_bytes", json!(result.host_resident_bytes)));
    fields.push(("device_resident_bytes", json!(result.device_resident_bytes)));
    fields.push(("workspace_bytes", json!(result.workspace_bytes)));
    fields.push(("pinned_host_bytes", json!(result.pinned_host_bytes)));
    fields.push((
        "pinned_host_peak_bytes",
        json!(result.pinned_host_peak_bytes),
    ));
    fields.push((
        "pinned_host_residency",
        json!(result.pinned_host_residency != 0),
    ));
    fields.push(("upload_bytes", json!(result.upload_bytes)));
    fields.push(("upload_count", json!(result.upload_count)));
    fields.push((
        "warm_weight_artifact_reads",
        json!(result.warm_weight_artifact_reads),
    ));
    fields.push((
        "warm_weight_upload_bytes",
        json!(result.warm_weight_upload_bytes),
    ));
    fields.push(("warm_h2d_bytes", json!(result.warm_h2d_bytes)));
    fields.push(("warm_d2h_bytes", json!(result.warm_d2h_bytes)));
    fields.push(("warm_host_allocations", json!(result.warm_host_allocations)));
    fields.push((
        "warm_device_allocations",
        json!(result.warm_device_allocations),
    ));
    fields.push(("warm_device_frees", json!(result.warm_device_frees)));
}

fn benchmark(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "benchmark_sample_count",
        json!(result.benchmark_sample_count),
    ));
    fields.push((
        "benchmark_first_execution_seconds",
        json!(result.benchmark_first_execution_seconds),
    ));
    fields.push((
        "benchmark_minimum_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MINIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_p50_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P50 as usize]
        ),
    ));
    fields.push((
        "benchmark_p90_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P90 as usize]
        ),
    ));
    fields.push((
        "benchmark_p95_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P95 as usize]
        ),
    ));
    fields.push((
        "benchmark_p99_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P99 as usize]
        ),
    ));
    fields.push((
        "benchmark_maximum_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MAXIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_mean_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MEAN as usize]
        ),
    ));
    fields.push((
        "benchmark_standard_deviation_seconds",
        json!(
            result.benchmark_host_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION
                    as usize]
        ),
    ));
    fields.push((
        "benchmark_device_timing_available",
        json!(result.benchmark_device_timing_available != 0),
    ));
    fields.push((
        "benchmark_identity",
        json!(ffi::text(&result.benchmark.identity)),
    ));
    fields.push((
        "benchmark_current_commit",
        json!(ffi::text(&result.benchmark.current_commit)),
    ));
    fields.push((
        "benchmark_current_source_state",
        json!(ffi::text(&result.benchmark.current_source_state)),
    ));
}

fn benchmark_device(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "benchmark_device_minimum_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MINIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p50_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P50 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p90_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P90 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p95_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P95 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p99_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P99 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_maximum_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MAXIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_device_mean_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MEAN as usize]
        ),
    ));
    fields.push((
        "benchmark_device_standard_deviation_seconds",
        json!(
            result.benchmark_device_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION
                    as usize]
        ),
    ));
}

fn benchmark_publication(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("benchmark_path", json!(ffi::text(&result.benchmark.path))));
    fields.push((
        "benchmark_baseline_written",
        json!(result.benchmark.baseline_written != 0),
    ));
    fields.push(("benchmark_file_bytes", json!(result.benchmark.file_bytes)));
}

fn benchmark_baseline(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "benchmark_baseline_identity",
        json!(ffi::text(&result.benchmark.baseline_identity)),
    ));
    fields.push((
        "benchmark_baseline_commit",
        json!(ffi::text(&result.benchmark.baseline_commit)),
    ));
    fields.push((
        "benchmark_baseline_source_state",
        json!(ffi::text(&result.benchmark.baseline_source_state)),
    ));
    fields.push(("benchmark_path", json!(ffi::text(&result.benchmark.path))));
    fields.push((
        "benchmark_baseline_compatible",
        json!(result.benchmark.baseline_compatible != 0),
    ));
    fields.push((
        "benchmark_cold_delta_seconds",
        json!(result.benchmark.cold_delta_seconds),
    ));
    fields.push((
        "benchmark_minimum_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MINIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_p50_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P50 as usize]
        ),
    ));
    fields.push((
        "benchmark_p90_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P90 as usize]
        ),
    ));
    fields.push((
        "benchmark_p95_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P95 as usize]
        ),
    ));
    fields.push((
        "benchmark_p99_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P99 as usize]
        ),
    ));
    fields.push((
        "benchmark_maximum_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MAXIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_mean_delta_seconds",
        json!(
            result.benchmark.host_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MEAN as usize]
        ),
    ));
}

fn benchmark_device_baseline(
    result: &raw::yvex_graph_attention_operator_result,
    fields: &mut Fields,
) {
    fields.push((
        "benchmark_device_minimum_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MINIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p50_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P50 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p90_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P90 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p95_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P95 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_p99_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_P99 as usize]
        ),
    ));
    fields.push((
        "benchmark_device_maximum_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MAXIMUM as usize]
        ),
    ));
    fields.push((
        "benchmark_device_mean_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_MEAN as usize]
        ),
    ));
    fields.push((
        "benchmark_device_standard_deviation_delta_seconds",
        json!(
            result.benchmark.device_delta_seconds
                [raw::yvex_runtime_benchmark_statistic_YVEX_RUNTIME_BENCHMARK_STANDARD_DEVIATION
                    as usize]
        ),
    ));
}

fn benchmark_chart(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "benchmark_chart_generated",
        json!(result.benchmark.chart_generated != 0),
    ));
    fields.push((
        "benchmark_chart_path",
        json!(ffi::text(&result.benchmark.chart_path)),
    ));
    fields.push((
        "benchmark_chart_identity",
        json!(ffi::text(&result.benchmark.chart_identity)),
    ));
    fields.push((
        "benchmark_chart_file_bytes",
        json!(result.benchmark.chart_file_bytes),
    ));
}

fn benchmark_context(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("benchmark_scope", json!(ffi::text(&result.benchmark_scope))));
    fields.push(("attention_class", json!(ffi::text(&result.attention_class))));
    fields.push(("requested_token_count", json!(result.requested_token_count)));
    fields.push((
        "requested_history_tokens",
        json!(result.requested_history_tokens),
    ));
    fields.push(("requested_layer_start", json!(result.requested_layer_start)));
    fields.push(("requested_layer_count", json!(result.requested_layer_count)));
    fields.push((
        "component_benchmark_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_COMPONENT_BENCHMARK
                    as usize]
        )),
    ));
    fields.push((
        "correctness_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_CORRECTNESS as usize]
        )),
    ));
    fields.push((
        "structural_runtime_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_STRUCTURAL as usize]
        )),
    ));
    fields.push((
        "performance_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_PERFORMANCE as usize]
        )),
    ));
    fields.push((
        "benchmark_correctness_precondition_passed",
        json!(result.benchmark_correctness_precondition_passed != 0),
    ));
    fields.push((
        "benchmark_runtime_precondition_passed",
        json!(result.benchmark_runtime_precondition_passed != 0),
    ));
}

fn benchmark_regression(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "benchmark_regression_policy_enabled",
        json!(result.benchmark.regression_policy_enabled != 0),
    ));
    fields.push((
        "benchmark_regression_policy_identity",
        json!(ffi::text(&result.benchmark.regression_policy_identity)),
    ));
    fields.push((
        "benchmark_comparison_identity",
        json!(ffi::text(&result.benchmark.comparison_identity)),
    ));
    fields.push((
        "benchmark_regression_basis_points",
        json!(result.benchmark.regression_basis_points),
    ));
    fields.push((
        "benchmark_performance_passed",
        json!(result.benchmark.performance_passed != 0),
    ));
}

fn qualification(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "qualification_identity",
        json!(ffi::text(&result.qualification_identity)),
    ));
    fields.push((
        "quality_matrix_identity",
        json!(ffi::text(&result.quality_matrix_identity)),
    ));
    fields.push((
        "software_contract_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_SOFTWARE as usize]
        )),
    ));
    fields.push((
        "numerical_conformance_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_NUMERICAL as usize]
        )),
    ));
    fields.push((
        "runtime_qualification_status",
        json!(ffi::text(
            &result.quality_status
                [raw::yvex_runtime_quality_status_YVEX_RUNTIME_QUALITY_QUALIFICATION as usize]
        )),
    ));
    fields.push((
        "model_behavior_evaluation_available",
        json!(result.model_behavior_evaluation_available != 0),
    ));
    fields.push((
        "model_quality_evaluation_available",
        json!(result.model_quality_evaluation_available != 0),
    ));
    fields.push((
        "agent_runtime_available",
        json!(result.agent_runtime_available != 0),
    ));
    fields.push((
        "agent_evaluation_available",
        json!(result.agent_evaluation_available != 0),
    ));
    fields.push((
        "release_qualification_available",
        json!(result.release_qualification_available != 0),
    ));
}

fn cuda(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("cuda_device", json!(ffi::text(&result.probe.cuda_device))));
    fields.push(("cuda_driver", json!(ffi::text(&result.cuda_driver))));
    fields.push((
        "cuda_build_identity",
        json!(ffi::text(&result.cuda_build_identity)),
    ));
    fields.push(("capture_bucket", json!(ffi::text(&result.capture_bucket))));
    fields.push((
        "compute_capability_major",
        json!(result.probe.cuda_compute_capability_major),
    ));
    fields.push((
        "compute_capability_minor",
        json!(result.probe.cuda_compute_capability_minor),
    ));
    fields.push(("kernel_launches", json!(result.probe.kernel_launches)));
    fields.push(("peak_device_bytes", json!(result.probe.peak_device_bytes)));
    fields.push(("h2d_bytes", json!(result.probe.h2d_bytes)));
    fields.push(("d2h_bytes", json!(result.probe.d2h_bytes)));
    fields.push((
        "cuda_launch_graph_identity",
        json!(ffi::text(&result.cuda_launch_graph_identity)),
    ));
    fields.push((
        "cuda_graph_exec_identity",
        json!(ffi::text(&result.cuda_graph_exec_identity)),
    ));
    fields.push(("cuda_graph_count", json!(result.cuda_graph_count)));
    fields.push((
        "cuda_graph_piece_count",
        json!(result.cuda_graph_piece_count),
    ));
    fields.push((
        "cuda_graph_capture_count",
        json!(result.cuda_graph_capture_count),
    ));
    fields.push((
        "cuda_graph_instantiate_count",
        json!(result.cuda_graph_instantiate_count),
    ));
    fields.push((
        "cuda_graph_replay_count",
        json!(result.cuda_graph_replay_count),
    ));
    fields.push((
        "cuda_graph_launch_count",
        json!(result.cuda_graph_launch_count),
    ));
    fields.push(("cuda_graph_node_count", json!(result.cuda_graph_node_count)));
    fields.push((
        "cuda_graph_kernel_node_count",
        json!(result.cuda_graph_kernel_node_count),
    ));
    fields.push((
        "cuda_graph_memcpy_node_count",
        json!(result.cuda_graph_memcpy_node_count),
    ));
    fields.push((
        "cuda_graph_memset_node_count",
        json!(result.cuda_graph_memset_node_count),
    ));
    fields.push((
        "cuda_graph_update_count",
        json!(result.cuda_graph_update_count),
    ));
    fields.push((
        "cuda_graph_update_pending_count",
        json!(result.cuda_graph_update_pending_count),
    ));
    fields.push((
        "cuda_graph_registry_scope",
        json!(ffi::text(&result.cuda_graph_registry_scope)),
    ));
    fields.push((
        "cuda_graph_registry_count",
        json!(result.cuda_graph_registry_count),
    ));
    fields.push((
        "cuda_graph_registry_index",
        json!(result.cuda_graph_registry_index),
    ));
    fields.push((
        "cuda_graph_registry_affected_count",
        json!(result.cuda_graph_registry_affected_count),
    ));
    fields.push((
        "cuda_graph_entry_compatibility_identity",
        json!(ffi::text(&result.cuda_graph_entry_compatibility_identity)),
    ));
    fields.push((
        "cuda_graph_entry_state",
        json!(result.cuda_graph_entry_state),
    ));
    fields.push((
        "cuda_graph_entry_reason",
        json!(result.cuda_graph_entry_reason),
    ));
    fields.push((
        "cuda_graph_entry_capture_mode",
        json!(result.cuda_graph_entry_capture_mode),
    ));
    fields.push((
        "cuda_graph_entry_uploaded",
        json!(result.cuda_graph_entry_uploaded != 0),
    ));
    fields.push((
        "cuda_graph_entry_update_requested",
        json!(result.cuda_graph_entry_update_requested != 0),
    ));
    fields.push((
        "cuda_graph_capture_elapsed_ns",
        json!(result.cuda_graph_capture_elapsed_ns),
    ));
    fields.push((
        "cuda_graph_instantiate_elapsed_ns",
        json!(result.cuda_graph_instantiate_elapsed_ns),
    ));
    fields.push((
        "cuda_graph_last_update_elapsed_ns",
        json!(result.cuda_graph_last_update_elapsed_ns),
    ));
    fields.push((
        "cuda_graph_last_replay_elapsed_ns",
        json!(result.cuda_graph_last_replay_elapsed_ns),
    ));
    fields.push((
        "cuda_graph_invalidation_count",
        json!(result.cuda_graph_invalidation_count),
    ));
}

fn comparison(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "cpu_output_digest",
        json!(ffi::text(&result.probe.cpu_output_digest)),
    ));
    fields.push((
        "cuda_output_digest",
        json!(ffi::text(&result.probe.cuda_output_digest)),
    ));
    fields.push((
        "cpu_state_delta_digest",
        json!(ffi::text(&result.probe.cpu_state_delta_digest)),
    ));
    fields.push((
        "cuda_state_delta_digest",
        json!(ffi::text(&result.probe.cuda_state_delta_digest)),
    ));
    fields.push((
        "comparison_contract_identity",
        json!(ffi::text(&result.probe.comparison_contract_identity)),
    ));
    fields.push(("comparison_values", json!(result.probe.comparison_values)));
    fields.push((
        "comparison_output_values",
        json!(result.probe.comparison_output_values),
    ));
    fields.push((
        "comparison_state_values",
        json!(result.probe.comparison_state_values),
    ));
    fields.push((
        "comparison_finite_values",
        json!(result.probe.comparison_finite_values),
    ));
    fields.push((
        "comparison_nonfinite_values",
        json!(result.probe.comparison_nonfinite_values),
    ));
    fields.push((
        "comparison_maximum_absolute_error",
        json!(result.probe.comparison_maximum_absolute_error),
    ));
    fields.push((
        "comparison_maximum_relative_error",
        json!(result.probe.comparison_maximum_relative_error),
    ));
    fields.push(("comparison_rmse", json!(result.probe.comparison_rmse)));
    fields.push((
        "comparison_passed",
        json!(result.probe.comparison_passed != 0),
    ));
    fields.push((
        "output_digest_equal",
        json!(result.probe.output_digest_equal != 0),
    ));
    fields.push((
        "state_delta_digest_equal",
        json!(result.probe.state_delta_digest_equal != 0),
    ));
    fields.push((
        "bitwise_equality_observed",
        json!(result.probe.bitwise_equality_observed != 0),
    ));
    fields.push((
        "bitwise_equality_required",
        json!(result.probe.bitwise_equality_required != 0),
    ));
}

fn failure(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "first_failing_stage",
        json!(ffi::text(&result.first_failing_stage)),
    ));
    fields.push((
        "first_failing_layer",
        json!(result.probe.first_failing_layer),
    ));
    fields.push((
        "first_failing_coordinate",
        json!(result.probe.first_failing_coordinate),
    ));
}

fn provenance(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "source_snapshot_identity",
        json!(ffi::text(&result.source_snapshot_identity)),
    ));
    fields.push((
        "payload_identity",
        json!(ffi::text(&result.payload_identity)),
    ));
    fields.push((
        "artifact_transform_identity",
        json!(ffi::text(&result.artifact_transform_identity)),
    ));
    fields.push((
        "logical_transform_identity",
        json!(ffi::text(&result.logical_transform_identity)),
    ));
    fields.push(("payload_bytes_read", json!(result.probe.payload_bytes_read)));
}

fn compatibility(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "current_writer_plan_identity",
        json!(ffi::text(&result.current_writer_plan_identity)),
    ));
    fields.push((
        "payload_plan_identity",
        json!(ffi::text(&result.payload_plan_identity)),
    ));
    fields.push((
        "payload_byte_identity",
        json!(ffi::text(&result.payload_byte_identity)),
    ));
    fields.push((
        "physical_payload_compatible",
        json!(result.physical_payload_compatible != 0),
    ));
    fields.push((
        "artifact_rebuild_required",
        json!(result.artifact_rebuild_required != 0),
    ));
    fields.push((
        "materialization_rebuild_required",
        json!(result.materialization_rebuild_required != 0),
    ));
    fields.push((
        "tensor_inventory_equal",
        json!(result.tensor_inventory_equal != 0),
    ));
    fields.push(("qtype_equal", json!(result.qtype_equal != 0)));
    fields.push(("layout_equal", json!(result.layout_equal != 0)));
    fields.push(("offset_equal", json!(result.offset_equal != 0)));
    fields.push((
        "payload_digest_equal",
        json!(result.payload_digest_equal != 0),
    ));
}

fn reachability(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "operator_command_available",
        json!(result.operator_command_available != 0),
    ));
    fields.push((
        "production_api_available",
        json!(result.production_api_available != 0),
    ));
    fields.push((
        "internal_live_runner_available",
        json!(result.internal_live_runner_available != 0),
    ));
    fields.push((
        "end_user_generation_available",
        json!(result.end_user_generation_available != 0),
    ));
}

fn reason(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push(("failure_code", json!(ffi::text(&result.failure_code))));
    fields.push(("failure_where", json!(ffi::text(&result.failure_where))));
    fields.push(("reason", json!(ffi::text(&result.reason))));
}

fn final_fields(result: &raw::yvex_graph_attention_operator_result, fields: &mut Fields) {
    fields.push((
        "activation_prefill_ready",
        json!(result.activation_prefill_ready != 0),
    ));
    fields.push((
        "prefill_persistent_state_ready",
        json!(result.prefill_persistent_state_ready != 0),
    ));
    fields.push((
        "full_model_prefill_ready",
        json!(result.full_model_prefill_ready != 0),
    ));
    fields.push((
        "persistent_kv_ready",
        json!(result.capabilities.persistent_kv_ready != 0),
    ));
    fields.push((
        "transformer_ready",
        json!(result.capabilities.transformer_ready != 0),
    ));
    fields.push((
        "runtime_generation_ready",
        json!(result.capabilities.generation_ready != 0),
    ));
}

pub(crate) fn fields(result: &raw::yvex_graph_attention_operator_result) -> Fields {
    let mut fields = Vec::new();
    base(result, &mut fields);
    if ffi::text(&result.family) != "unavailable" {
        target(result, &mut fields);
    }
    if result.attention_plan_identity[0] != 0 {
        admission(result, &mut fields);
        capability(result, &mut fields);
        runtime(result, &mut fields);
        provenance(result, &mut fields);
    }
    if result.completed != 0 {
        execution(result, &mut fields);
    }
    if result.benchmark_sample_count != 0 {
        timing(result, &mut fields);
        benchmark(result, &mut fields);
    }
    if result.residency_identity[0] != 0 {
        residency(result, &mut fields);
    }
    if result.state_layout_identity[0] != 0 {
        state(result, &mut fields);
    }
    if result.benchmark_device_timing_available != 0 {
        benchmark_device(result, &mut fields);
    }
    if result.benchmark.baseline_written != 0 {
        benchmark_publication(result, &mut fields);
    }
    if result.benchmark.baseline_identity[0] != 0 {
        benchmark_baseline(result, &mut fields);
    }
    if result.benchmark.device_timing_available != 0 {
        benchmark_device_baseline(result, &mut fields);
    }
    if result.benchmark.chart_generated != 0 {
        benchmark_chart(result, &mut fields);
    }
    if result.benchmark_scope[0] != 0 {
        benchmark_context(result, &mut fields);
    }
    if result.benchmark.comparison_identity[0] != 0 {
        benchmark_regression(result, &mut fields);
    }
    if result.qualification_identity[0] != 0 {
        qualification(result, &mut fields);
    }
    if result.probe.cuda_device[0] != 0 {
        cuda(result, &mut fields);
    }
    if result.probe.comparison_available != 0 {
        comparison(result, &mut fields);
        if result.probe.comparison_passed == 0 {
            failure(result, &mut fields);
        }
    }
    if result.current_writer_plan_identity[0] != 0 {
        compatibility(result, &mut fields);
    }
    reachability(result, &mut fields);
    if result.reason[0] != 0 {
        reason(result, &mut fields);
    }
    final_fields(result, &mut fields);
    fields
}

pub(crate) fn json(result: &raw::yvex_graph_attention_operator_result) -> String {
    let object = fields(result)
        .into_iter()
        .map(|(key, value)| (key.to_owned(), value))
        .collect::<serde_json::Map<_, _>>();
    format!("{}\n", Value::Object(object))
}

pub(crate) fn csv(result: &raw::yvex_graph_attention_operator_result) -> String {
    let mut text = String::from("field,value\n");
    for (key, value) in fields(result) {
        let value = value
            .as_str()
            .map(str::to_owned)
            .unwrap_or_else(|| value.to_string());
        text.push_str(&format!("\"{key}\",\"{}\"\n", value.replace('"', "\"\"")));
    }
    text
}
