/* Owner: src/cli/render
 * Owns: help and typed normal/table/audit rendering for model artifact reports.
 * Does not own: report building, registry lookup, model gate checks, backend calls, file writing, artifact
 *   emission, runtime generation, eval, benchmark, or release decisions.
 * Invariants: this renderer formats typed facts only and uses src/cli/io writers.
 * Boundary: rendered reports do not imply artifact emission, runtime generation, benchmark evidence, or release
 *   readiness.
 * Purpose: provide help and typed normal/table/audit rendering for model artifact reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"
#include "src/cli/model_artifacts/private.h"
#include "src/cli/io/private.h"

typedef struct {
    const char *key;
    const char *role;
} fullmodel_role_projection;

static const fullmodel_role_projection family_runtime_roles[] = {
    {"token_embedding_role", "token_embedding"},
    {"attention_norm_role", "attention_norm"},
    {"post_attention_norm_role", "post_attention_norm"},
    {"final_norm_role", "final_norm"},
    {"q_projection_role", "q_projection"},
    {"k_projection_role", "k_projection"},
    {"v_projection_role", "v_projection"},
    {"o_projection_role", "o_projection"},
    {"mlp_gate_role", "mlp_gate"},
    {"mlp_up_role", "mlp_up"},
    {"mlp_down_role", "mlp_down"},
    {"moe_router_role", "moe_router"},
    {"output_head_role", "output_head"},
    {"tokenizer_metadata_role", "tokenizer_metadata"},
};

static const yvex_render_field_spec prepare_identity_fields[] = {
    {"target_id", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, target_id), "unknown"},
    {"family", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, family), "unknown"},
    {"provider", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, provider), "unknown"},
    {"repo_id", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, repo_id), "unknown"},
    {"revision", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, revision), "unknown"},
    {"models_root", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, models_root), "unknown"},
    {"source_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, source_path), "unknown"},
    {"source_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, source_status), "unknown"},
    {"source_manifest_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, source_manifest_path), "unknown"},
    {"native_inventory_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, native_inventory_path), "unknown"},
    {"model_class_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, model_class_status), "unknown"},
    {"tensor_map_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, tensor_map_path), "unknown"},
    {"tensor_map_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, tensor_map_status), "unknown"},
    {"output_head_map_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, output_head_map_path), "unknown"},
    {"output_head_map_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, output_head_map_status), "unknown"},
    {"tokenizer_map_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, tokenizer_map_path), "unknown"},
    {"tokenizer_map_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, tokenizer_map_status), "unknown"},
    {"artifact_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, artifact_status), "unknown"},
    {"expected_artifact_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, expected_artifact_path), "unknown"},
    {"artifact_plan_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, artifact_plan_status), "unknown"},
    {"artifact_emission_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, artifact_emission_status), "unknown"},
    {"artifact_identity_status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, artifact_identity_status), "unknown"},
    {"prepare_blocker_count", YVEX_RENDER_FIELD_U32,
     offsetof(yvex_models_prepare_source_report, blocker_count), NULL},
    {"top_blocker", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, top_blocker), "unknown"},
    {"download_registry_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, download_registry_path), "unknown"},
    {"download_report_path", YVEX_RENDER_FIELD_TEXT_ARRAY,
     offsetof(yvex_models_prepare_source_report, download_report_path), "unknown"},
    {"downloaded_target_resolved", YVEX_RENDER_FIELD_BOOL,
     offsetof(yvex_models_prepare_source_report, downloaded_target_resolved), NULL},
};

static const yvex_render_field_spec prepare_result_fields[] = {
    {"reason", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, reason), "unknown"},
    {"next", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, next), "unknown"},
    {"status", YVEX_RENDER_FIELD_TEXT,
     offsetof(yvex_models_prepare_source_report, final_status), "unknown"},
};

#define MATERIALIZE_REPORT_FIELD(key_, kind_, member_, fallback_) \
    {key_, kind_, offsetof(fullmodel_materialize_report, member_), fallback_}

static const yvex_render_field_spec materialize_admission_fields[] = {
    MATERIALIZE_REPORT_FIELD("artifact_identity_status", YVEX_RENDER_FIELD_TEXT,
                             artifact_identity_status, "not-checked"),
    MATERIALIZE_REPORT_FIELD("tensor_inventory_status", YVEX_RENDER_FIELD_TEXT,
                             tensor_inventory_status, "unknown"),
    MATERIALIZE_REPORT_FIELD("required_role_coverage", YVEX_RENDER_FIELD_TEXT,
                             required_role_coverage, "partial"),
    MATERIALIZE_REPORT_FIELD("missing_required_roles", YVEX_RENDER_FIELD_TEXT,
                             missing_required_roles, "unknown"),
    MATERIALIZE_REPORT_FIELD("unsupported_required_roles", YVEX_RENDER_FIELD_TEXT,
                             unsupported_required_roles,
                             "runtime-family-adapter,real-transformer-prefill,real-attention-backed-KV,"
                                 "real-DeepSeek-decode,real-output-head-logits,real-vocabulary-sampling"),
    MATERIALIZE_REPORT_FIELD("placement_plan_status", YVEX_RENDER_FIELD_TEXT,
                             placement_plan_status, "unknown"),
    MATERIALIZE_REPORT_FIELD("memory_budget_status", YVEX_RENDER_FIELD_TEXT,
                             memory_budget_status, "unknown"),
    MATERIALIZE_REPORT_FIELD("backend_preflight_status", YVEX_RENDER_FIELD_TEXT,
                             backend_preflight_status, "unknown"),
    MATERIALIZE_REPORT_FIELD("materialization_mode", YVEX_RENDER_FIELD_TEXT,
                             materialization_mode, "none"),
    MATERIALIZE_REPORT_FIELD("full_model_materialization", YVEX_RENDER_FIELD_TEXT,
                             full_model_materialization, "failed"),
    MATERIALIZE_REPORT_FIELD("full_model_materialization_proof", YVEX_RENDER_FIELD_TEXT,
                             full_model_materialization_proof, "fail"),
};

static const yvex_render_field_spec materialize_lifecycle_fields[] = {
    MATERIALIZE_REPORT_FIELD("phase", YVEX_RENDER_FIELD_TEXT, phase, "failed"),
    MATERIALIZE_REPORT_FIELD("failed_phase", YVEX_RENDER_FIELD_TEXT, failed_phase, "none"),
    MATERIALIZE_REPORT_FIELD("failed_reason", YVEX_RENDER_FIELD_TEXT, failed_reason, "none"),
    MATERIALIZE_REPORT_FIELD("cleanup_attempted", YVEX_RENDER_FIELD_TEXT,
                             cleanup_attempted, "false"),
    MATERIALIZE_REPORT_FIELD("cleanup_status", YVEX_RENDER_FIELD_TEXT,
                             cleanup_status, "not-needed"),
    MATERIALIZE_REPORT_FIELD("cleanup_idempotent", YVEX_RENDER_FIELD_TEXT,
                             cleanup_idempotent, "true"),
    MATERIALIZE_REPORT_FIELD("owned_state_released", YVEX_RENDER_FIELD_TEXT,
                             owned_state_released, "true"),
    MATERIALIZE_REPORT_FIELD("partial_materialization", YVEX_RENDER_FIELD_TEXT,
                             partial_materialization, "false"),
};

static const yvex_render_field_spec materialize_accounting_fields[] = {
    MATERIALIZE_REPORT_FIELD("materialized_tensor_count", YVEX_RENDER_FIELD_U64,
                             materialized_tensor_count, NULL),
    MATERIALIZE_REPORT_FIELD("materialized_tensor_bytes", YVEX_RENDER_FIELD_U64,
                             materialized_tensor_bytes, NULL),
    MATERIALIZE_REPORT_FIELD("refused_tensor_count", YVEX_RENDER_FIELD_U64,
                             refused_tensor_count, NULL),
    MATERIALIZE_REPORT_FIELD("skipped_tensor_count", YVEX_RENDER_FIELD_U64,
                             skipped_tensor_count, NULL),
    MATERIALIZE_REPORT_FIELD("required_tensor_count", YVEX_RENDER_FIELD_U64,
                             required_tensor_count, NULL),
    MATERIALIZE_REPORT_FIELD("required_tensor_bytes", YVEX_RENDER_FIELD_U64,
                             required_tensor_bytes, NULL),
    MATERIALIZE_REPORT_FIELD("peak_planned_bytes", YVEX_RENDER_FIELD_U64,
                             peak_planned_bytes, NULL),
    MATERIALIZE_REPORT_FIELD("cpu_resident_bytes", YVEX_RENDER_FIELD_U64,
                             cpu_resident_bytes, NULL),
    MATERIALIZE_REPORT_FIELD("cuda_resident_bytes", YVEX_RENDER_FIELD_U64,
                             cuda_resident_bytes, NULL),
    MATERIALIZE_REPORT_FIELD("residency_plan", YVEX_RENDER_FIELD_TEXT,
                             residency_plan, "not-planned"),
    MATERIALIZE_REPORT_FIELD("runtime_blockers", YVEX_RENDER_FIELD_TEXT,
                             runtime_blockers, "runtime family adapter not implemented"),
};

#undef MATERIALIZE_REPORT_FIELD

static const char *const literal_pair_0[] = { "family: unknown", "family_detected: unknown"};

static const char *const literal_pair_1[] = { "family_runtime: report", "status: fullmodel-family-runtime-fail"};

static const char *const literal_pair_2[] = { "fullmodel: descriptor", "status: fullmodel-descriptor-fail"};

static const char *const literal_pair_3[] = { "family: unknown", "family_detected: unknown"};

static const char *const literal_pair_4[] = { "family_runtime: report", "status: fullmodel-family-runtime-fail"};

static const char *const literal_pair_5[] = { "fullmodel: descriptor", "status: fullmodel-descriptor-fail"};

static const char *const literal_pair_6[] = { "family: glm", "family_detected: glm"};

static const char *const literal_pair_7[] = {
    "family_runtime: report", "status: fullmodel-family-runtime-unsupported"};

static const char *const literal_pair_8[] = { "fullmodel: descriptor", "status: fullmodel-descriptor-unsupported"};

static const char *const literal_pair_9[] = {
    "plan_kind: full-model-materialization", "plan_source: source-target-without-YVEX-GGUF"};

static const char *const literal_pair_10[] = {
    "fullmodel: materialization-plan", "status: fullmodel-materialization-plan-unsupported"};

static const char *const literal_pair_11[] = { "fullmodel: report", "status: fullmodel-report-unsupported"};

static const char *const literal_pair_12[] = {
    "boundary: descriptor report-only, no runtime execution", "status: fullmodel-descriptor"};

static const char *const literal_pair_13[] = {
    "boundary: plan-only, no materialization", "status: fullmodel-materialization-plan"};

static const char *const literal_pair_14[] = {
    "graph_requirement_status: blocked", "runtime_blocker_status: blocked"};

static const char *const literal_lines_0[] = {
    "prefill_ready: false", "decode_ready: false", "logits_ready: false", "sampling_ready: false",
    "full_model_execution: unsupported", "full_model_materialization: planned", "full_runtime_descriptor: planned",
    "generation_ready: false", "generation: unsupported-full-model", "benchmark_status: not-measured"};

static const char *const literal_lines_9[] = {
    "family_adapter: unsupported", "family_adapter_status: unsupported", "family_runtime_stage: report-only",
    "runtime_claim: unsupported", "generation: unsupported-full-model", "benchmark_status: not-measured",
    "descriptor_status: unavailable", "descriptor_source: fullmodel-descriptor-facts", "full_runtime_model: false",
    "full_model_execution: unsupported", "generation_ready: false", "runtime_execution_ready: false",
    "runtime_blockers: unsupported or unknown runtime family adapter",
    "next_required_rows: FAMILY.RUNTIME.0,ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,family-"
        "specific-runtime-target",
    "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_10[] = {
    "family_runtime_stage: report-only", "runtime_claim: unsupported", "generation: unsupported-full-model",
    "benchmark_status: not-measured"};

static const char *const literal_lines_11[] = {
    "descriptor_source: fullmodel-descriptor-facts", "full_runtime_model: false",
    "full_model_execution: unsupported", "generation_ready: false"};

static const char *const literal_lines_12[] = {
    "position_rule_status: planned", "kv_rule_status: blocked", "moe_rule_status: blocked"};

static const char *const literal_lines_13[] = {
    "attention_family: deepseek-family-attention-planned", "attention_type: unknown-family-specific",
    "attention_heads: unknown", "kv_heads: unknown", "head_dim: unknown", "attention_q_required: true",
    "attention_k_required: true", "attention_v_required: true", "attention_o_required: true", "rope_required: true",
    "rope_status: planned", "rope_base: unknown", "rope_scaling: unknown", "mask_required: true",
    "mask_rule: causal-or-family-specific-planned", "context_policy: planned", "attention_runtime_ready: false",
    "kv_required: true", "kv_layout: planned", "kv_dtype: planned",
    "kv_capacity_status: unsupported-full-transformer-kv", "kv_write_ready: false", "kv_read_ready: false",
    "moe_required: family-specific-planned", "router_required: family-specific-planned"};

static const char *const literal_lines_14[] = {
    "moe_expert_count: unknown", "expert_count: unknown", "moe_active_expert_count: unknown",
    "active_expert_count: unknown", "moe_shared_experts: unknown", "shared_expert_status: planned",
    "moe_dispatch_ready: false",
    "moe_blockers: router logits, top-k routing, expert dispatch, and expert accumulation are not implemented",
    "output_head_required: true"};

static const char *const literal_lines_15[] = { "logits_projection_ready: false", "real_output_head_logits: false",
    "logits_blockers: real output-head logits runtime unsupported",
    "required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,"
        "attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp,moe-"
        "router,expert-dispatch,final-norm,output-head-projection",
    "implemented_graph_primitives: rope,attention-fixture,matmul,mlp-fixture,controlled-block,controlled-"
        "layers,selected-embedding,selected-rmsnorm-segment",
    "unsupported_graph_ops: full-attention-from-model-tensors,full-transformer-block-from-model-tensors,"
        "full-layer-stack,real-moe-router,real-expert-dispatch,real-output-head-projection",
    "graph.rope_primitive: implemented", "graph.attention_primitive: implemented-fixture",
    "graph.matmul_primitive: implemented", "graph.mlp_primitive: implemented-fixture",
    "graph.full_attention_from_model_tensors: unsupported",
    "graph.full_transformer_block_from_model_tensors: unsupported", "graph.full_layer_stack: unsupported"};

static const char *const literal_lines_16[] = {
    "prefill_ready: false", "decode_ready: false", "logits_ready: false", "sampling_ready: false",
    "runtime_execution_ready: false"};

static const char *const literal_lines_17[] = {
    "next_required_rows: ATTENTION.CLASS.0,CONTEXT.CLASS.0,KV.CACHE.0,MOE.CLASS.0,"
        "FAMILY.RUNTIME.DeepSeek.detail,real-transformer-prefill,real-decode,real-output-head-logits,real-"
        "vocabulary-sampling,GEN.DEEPSEEK.0",
    "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_18[] = {
    "full_model_execution: unsupported", "generation_ready: false", "generation: unsupported-full-model",
    "benchmark_status: not-measured"};

static const char *const literal_lines_19[] = {
    "target_class: official-source-huge-model", "source_artifact_class: official safetensors",
    "target_artifact_class: future YVEX-produced GGUF", "artifact_exists: false", "artifact_bytes: 0",
    "artifact_identity_status: not-applicable", "tensor_count: 0",
    "tensor_inventory_status: not-performed-source-only-target", "metadata_status: not-performed",
    "architecture: glm", "family: glm", "model_class: huge-MoE-source-target",
    "fullmodel_inventory: unsupported-source-only", "qtype_summary: none", "dtype_summary: none",
    "total_tensor_bytes: 0", "estimated_cpu_resident_bytes: unknown", "estimated_cuda_resident_bytes: unknown",
    "estimated_kv_bytes: planned", "estimated_scratch_bytes: planned", "estimated_total_runtime_bytes: unknown"};

static const char *const literal_lines_20[] = {
    "backend_placement_status: not-performed", "cpu_placement: unsupported-source-only",
    "cuda_placement: unsupported-source-only"};

static const char *const literal_lines_21[] = {
    "cuda_memory_status: unavailable", "residency_plan: future-YVEX-produced-artifact-required",
    "tensor_collections_status: not-performed", "collection_detected: no", "collection_supported: false",
    "runtime_consumer: unsupported", "embedding_tensors: 0", "normalization_tensors: 0", "attention_tensors: 0",
    "kv_cache_requirements: planned", "mlp_tensors: 0", "moe_tensors: 0", "output_tensors: 0",
    "tokenizer_tensors: 0", "required_role_coverage: none", "missing_required_roles: YVEX-produced-GGUF-artifact",
    "unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution",
    "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported"};

static const char *const literal_lines_22[] = {
    "target_class: official-source-huge-model", "artifact_exists: false", "artifact_bytes: 0",
    "artifact_identity_status: not-applicable", "tensor_inventory_status: not-performed-source-only-target",
    "tensor_count: 0", "total_tensor_bytes: 0"};

static const char *const literal_lines_23[] = {
    "plan_status: unsupported", "materialization_plan_ready: false", "materialization_attempted: false",
    "full_materialization_proof: false", "full_model_execution: unsupported", "generation_ready: false",
    "generation: unsupported-full-model", "benchmark_status: not-measured"};

static const char *const literal_lines_24[] = {
    "plan_tensor_count: 0", "plan_tensor_bytes: 0", "plan_collection_count: 0", "plan_phase_count: 1",
    "plan_blocker_count: 1", "plan_cleanup_required: false", "plan_cleanup_phases: none",
    "backend_available: unknown", "backend_memory_known: false", "backend_memory_total_bytes: unknown",
    "backend_memory_available_bytes: unknown", "backend_required_bytes: 0", "backend_fit_status: unsupported",
    "backend_fit_reason: source-only target has no YVEX-produced GGUF tensor inventory",
    "backend_allocation_attempted: false"};

static const char *const literal_lines_25[] = {
    "cleanup_plan_required: false", "cleanup_plan_phases: none", "cleanup_idempotent_required: true",
    "cleanup_failure_policy: preserve-failure-report", "next_required_row: FULLMODEL.2",
    "proof_ready_for_fullmodel_2: false",
    "fullmodel_2_blockers: YVEX-produced GGUF artifact missing; full tensor inventory unavailable"};

static const char *const literal_lines_26[] = { "format: text", "artifact_identity_status: not-applicable",
    "tensor_inventory_status: not-performed-source-only-target",
    "materialization_plan_status: unsupported-source-only", "materialization_proof_status: unsupported-source-only",
    "runtime_descriptor: report-only", "runtime_descriptor_status: unsupported",
    "runtime_descriptor_kind: fullmodel-planning", "family: glm", "architecture: glm",
    "model_class: huge-MoE-source-target", "full_runtime_model: false", "full_model_execution: unsupported",
    "full_model_materialization: unsupported-source-only", "generation_ready: false",
    "generation: unsupported-full-model", "benchmark_status: not-measured", "tensor_role_map_status: not-performed",
    "tensor_collection_map_status: not-performed", "required_role_coverage: none",
    "missing_required_roles: YVEX-produced-GGUF-artifact",
    "unsupported_required_roles: GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-runtime-execution",
    "unknown_role_count: 0", "embedding_descriptor: not-performed-source-only",
    "normalization_descriptor: not-performed-source-only", "attention_descriptor: not-performed-source-only",
    "mlp_descriptor: not-performed-source-only", "moe_descriptor: not-performed-source-only",
    "output_descriptor: not-performed-source-only", "tokenizer_descriptor: not-performed-source-only",
    "kv_descriptor: unsupported-source-only", "prefill_descriptor: unsupported-source-only",
    "decode_descriptor: unsupported-source-only", "logits_descriptor: unsupported-source-only",
    "sampling_descriptor: unsupported-source-only", "graph_requirements_status: unsupported-source-only",
    "required_graph_ops: planned-after-YVEX-produced-GGUF", "unsupported_graph_ops: GLM-full-transformer-runtime",
    "required_backend_ops: planned-after-YVEX-produced-GGUF", "unsupported_backend_ops: GLM-runtime-execution",
    "residency_requirements_status: unsupported-source-only",
    "residency_plan: future-YVEX-produced-artifact-required", "cpu_resident_required_bytes: unknown",
    "cuda_resident_required_bytes: unknown", "host_staged_required_bytes: unknown",
    "ssd_staged_required_bytes: planned", "kv_required_bytes: planned", "scratch_required_bytes: planned",
    "context_requirements_status: planned", "max_context: unknown", "requested_context: not-requested",
    "context_policy: planned", "position_policy: planned", "rope_policy: planned",
    "kv_requirements_status: unsupported-source-only", "kv_layout: planned", "kv_dtype: planned",
    "kv_layers: unknown", "kv_heads: unknown", "kv_head_dim: unknown",
    "kv_capacity_status: unsupported-source-only", "kv_write_ready: false", "kv_read_ready: false",
    "logits_requirements_status: unsupported-source-only", "output_head_present: false",
    "output_head_dtype: unknown", "vocab_size: unknown", "logits_buffer_required: true",
    "real_output_head_logits: false", "logits_ready: false",
    "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime unsupported",
    "descriptor_blockers: source-only target has no YVEX-produced GGUF tensor inventory", "prefill_ready: false",
    "decode_ready: false", "sampling_ready: false", "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_27[] = {
    "family_adapter: unsupported-source-only", "family_adapter_status: unsupported",
    "family_runtime_stage: report-only", "runtime_claim: unsupported", "generation: unsupported-full-model",
    "benchmark_status: not-measured", "descriptor_status: unsupported-source-only",
    "descriptor_source: not-performed-source-only-target", "full_runtime_model: false",
    "full_model_execution: unsupported", "generation_ready: false", "role_adapter_status: not-performed",
    "collection_adapter_status: not-performed", "attention_rule_status: unsupported-source-only",
    "position_rule_status: unsupported-source-only", "kv_rule_status: unsupported-source-only",
    "moe_rule_status: unsupported-source-only", "mlp_rule_status: unsupported-source-only",
    "output_head_rule_status: unsupported-source-only", "tokenizer_rule_status: unsupported-source-only",
    "graph_requirement_status: unsupported-source-only", "runtime_blocker_status: blocked",
    "tensor_inventory_status: not-performed-source-only-target", "token_embedding_role: not-performed-source-only",
    "attention_norm_role: not-performed-source-only", "q_projection_role: not-performed-source-only",
    "k_projection_role: not-performed-source-only", "v_projection_role: not-performed-source-only",
    "o_projection_role: not-performed-source-only", "output_head_role: not-performed-source-only",
    "tokenizer_metadata_role: not-performed-source-only", "attention_family: glm-family-planned",
    "attention_type: unknown-source-only", "attention_runtime_ready: false", "kv_required: true",
    "kv_layout: planned", "kv_dtype: planned", "kv_capacity_status: unsupported-source-only",
    "kv_write_ready: false", "kv_read_ready: false", "moe_required: true", "router_required: true",
    "router_present: false", "expert_tensors_present: false", "moe_expert_count: unknown",
    "moe_active_expert_count: unknown", "moe_shared_experts: unknown", "moe_dispatch_ready: false",
    "output_head_required: true", "output_head_present: false", "output_head_tensor: none", "vocab_size: unknown",
    "logits_projection_ready: false", "real_output_head_logits: false",
    "required_graph_ops: planned-after-YVEX-produced-GGUF",
    "implemented_graph_primitives: none-for-source-only-target",
    "unsupported_graph_ops: GLM-full-transformer-runtime", "graph.full_attention_from_model_tensors: unsupported",
    "graph.full_transformer_block_from_model_tensors: unsupported", "graph.full_layer_stack: unsupported",
    "full_transformer_graph_ready: false", "prefill_ready: false", "decode_ready: false", "logits_ready: false",
    "sampling_ready: false", "runtime_execution_ready: false",
    "runtime_blockers: source-only target; YVEX-produced GGUF emission planned; GLM runtime family mapping "
        "planned; GLM runtime unsupported",
    "next_required_rows: OWI.HUGE.0,MODEL.CLASS.3,TENSOR.COLLECTION.2,ATTENTION.CLASS.0,KV.CACHE.0,"
        "MOE.CLASS.0,GLM-YVEX-produced-GGUF,GLM-runtime-family-mapping",
    "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_28[] = {
    "plan_status: failed", "materialization_plan_ready: false", "materialization_attempted: false",
    "full_materialization_proof: false", "full_model_execution: unsupported", "generation_ready: false",
    "generation: unsupported-full-model", "benchmark_status: not-measured", "plan_id: unavailable",
    "plan_kind: full-model-materialization", "plan_source: tensor-inventory"};

static const char *const literal_lines_29[] = {
    "plan_tensor_count: 0", "plan_tensor_bytes: 0", "plan_collection_count: 0", "plan_phase_count: 1",
    "plan_blocker_count: 1", "plan_cleanup_required: false", "plan_cleanup_phases: none",
    "backend_available: unknown", "backend_memory_known: false", "backend_memory_total_bytes: unknown",
    "backend_memory_available_bytes: unknown", "backend_required_bytes: 0", "backend_fit_status: unknown",
    "backend_fit_reason: inventory failed before backend fit preflight", "backend_allocation_attempted: false"};

static const char *const literal_lines_30[] = {
    "cleanup_plan_required: false", "cleanup_plan_phases: none", "cleanup_idempotent_required: true",
    "cleanup_failure_policy: preserve-failure-report", "next_required_row: FULLMODEL.2",
    "proof_ready_for_fullmodel_2: false",
    "fullmodel_2_blockers: tensor inventory unavailable; materialization plan unavailable"};

static const char *const literal_lines_31[] = {
    "artifact_identity_status: unavailable", "tensor_inventory_status: failed",
    "materialization_plan_status: unavailable", "materialization_proof_status: unavailable",
    "runtime_descriptor: report-only", "runtime_descriptor_status: fail",
    "runtime_descriptor_kind: fullmodel-planning", "family: unknown", "architecture: unknown",
    "model_class: unresolved", "full_runtime_model: false", "full_model_execution: unsupported",
    "full_model_materialization: unavailable", "generation_ready: false", "generation: unsupported-full-model",
    "benchmark_status: not-measured", "tensor_role_map_status: unavailable",
    "tensor_collection_map_status: unavailable", "required_role_coverage: none", "missing_required_roles: artifact",
    "unsupported_required_roles: full-runtime-model", "runtime_blockers: artifact path missing",
    "descriptor_blockers: artifact path missing", "prefill_ready: false", "decode_ready: false",
    "logits_ready: false", "sampling_ready: false", "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_32[] = {
    "family_adapter: unavailable", "family_adapter_status: failed", "family_runtime_stage: report-only",
    "runtime_claim: unsupported", "generation: unsupported-full-model", "benchmark_status: not-measured",
    "descriptor_status: fail", "descriptor_source: unavailable", "full_runtime_model: false",
    "full_model_execution: unsupported", "generation_ready: false", "runtime_execution_ready: false",
    "runtime_blockers: artifact path missing", "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_33[] = {
    "target_class: unresolved-artifact", "source_artifact_class: unknown", "target_artifact_class: GGUF artifact",
    "artifact_exists: false", "artifact_bytes: 0", "artifact_identity_status: unavailable", "tensor_count: 0",
    "tensor_inventory_status: failed", "metadata_status: failed", "architecture: unknown", "family: unknown",
    "model_class: unresolved", "fullmodel_inventory: unavailable", "qtype_summary: none", "dtype_summary: none",
    "total_tensor_bytes: 0", "estimated_cpu_resident_bytes: unknown", "estimated_cuda_resident_bytes: unknown",
    "estimated_kv_bytes: planned", "estimated_scratch_bytes: planned", "estimated_total_runtime_bytes: unknown"};

static const char *const literal_lines_34[] = {
    "backend_placement_status: failed-missing-artifact", "cpu_placement: unavailable",
    "cuda_placement: unavailable"};

static const char *const literal_lines_35[] = {
    "cuda_memory_status: unavailable", "residency_plan: unavailable", "tensor_collections_status: unavailable",
    "collection_detected: no", "collection_supported: false", "runtime_consumer: unsupported",
    "embedding_tensors: 0", "normalization_tensors: 0", "attention_tensors: 0", "kv_cache_requirements: planned",
    "mlp_tensors: 0", "moe_tensors: 0", "output_tensors: 0", "tokenizer_tensors: 0", "required_role_coverage: none",
    "missing_required_roles: artifact", "unsupported_required_roles: full-runtime-model",
    "runtime_blockers: artifact path missing"};

static const char *const literal_lines_36[] = {
    "artifact_identity_status: not-checked", "tensor_inventory_status: failed",
    "materialization_plan_status: failed", "materialization_proof_status: unavailable",
    "runtime_descriptor: report-only", "runtime_descriptor_status: fail",
    "runtime_descriptor_kind: fullmodel-planning", "family: unknown", "architecture: unknown",
    "model_class: parse-failed", "full_runtime_model: false", "full_model_execution: unsupported",
    "full_model_materialization: unavailable", "generation_ready: false", "generation: unsupported-full-model",
    "benchmark_status: not-measured", "tensor_role_map_status: unavailable",
    "tensor_collection_map_status: unavailable", "required_role_coverage: none",
    "missing_required_roles: parseable-GGUF-tensor-directory", "unsupported_required_roles: full-runtime-model",
    "runtime_blockers: GGUF metadata or tensor directory parse failed",
    "descriptor_blockers: GGUF metadata or tensor directory parse failed", "prefill_ready: false",
    "decode_ready: false", "logits_ready: false", "sampling_ready: false", "cleanup_attempted: false",
    "cleanup_status: not-needed"};

static const char *const literal_lines_37[] = {
    "family_adapter: unavailable", "family_adapter_status: failed", "family_runtime_stage: report-only",
    "runtime_claim: unsupported", "generation: unsupported-full-model", "benchmark_status: not-measured",
    "descriptor_status: fail", "descriptor_source: fullmodel-descriptor-facts", "full_runtime_model: false",
    "full_model_execution: unsupported", "generation_ready: false", "tensor_inventory_status: failed",
    "runtime_execution_ready: false", "runtime_blockers: GGUF metadata or tensor directory parse failed",
    "cleanup_attempted: false", "cleanup_status: not-needed"};

static const char *const literal_lines_38[] = {
    "target_class: GGUF-artifact", "source_artifact_class: unknown", "target_artifact_class: GGUF artifact",
    "artifact_exists: true"};

static const char *const literal_lines_39[] = {
    "artifact_identity_status: not-checked", "tensor_count: 0", "tensor_inventory_status: failed",
    "metadata_status: failed", "architecture: unknown", "family: unknown", "model_class: parse-failed",
    "fullmodel_inventory: unavailable", "qtype_summary: none", "dtype_summary: none", "total_tensor_bytes: 0",
    "estimated_cpu_resident_bytes: unknown", "estimated_cuda_resident_bytes: unknown",
    "estimated_kv_bytes: planned", "estimated_scratch_bytes: planned", "estimated_total_runtime_bytes: unknown"};

static const char *const literal_lines_40[] = {
    "backend_placement_status: failed-parse", "cpu_placement: unavailable", "cuda_placement: unavailable"};

static const char *const literal_lines_41[] = {
    "cuda_memory_status: unavailable", "residency_plan: unavailable", "tensor_collections_status: failed",
    "collection_detected: no", "collection_supported: false", "runtime_consumer: unsupported",
    "embedding_tensors: 0", "normalization_tensors: 0", "attention_tensors: 0", "kv_cache_requirements: planned",
    "mlp_tensors: 0", "moe_tensors: 0", "output_tensors: 0", "tokenizer_tensors: 0", "required_role_coverage: none",
    "missing_required_roles: parseable-GGUF-tensor-directory", "unsupported_required_roles: full-runtime-model",
    "runtime_blockers: GGUF metadata or tensor directory parse failed"};

static const char *const literal_lines_42[] = {
    "alias: yvex fullmodel plan --model FILE_OR_ALIAS [options]", "\nExamples:",
    "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed --backend cpu",
    "  yvex fullmodel report --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --limit-tensors 8",
    "  yvex fullmodel report --model ./candidate.gguf --target deepseek4-v4-flash --backend cuda",
    "  yvex fullmodel materialization-plan --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu "
        "--residency resident",
    "  yvex fullmodel materialization-plan --model ./candidate.gguf --target deepseek4-v4-flash --backend "
        "cuda --residency hybrid",
    "  yvex fullmodel materialize --model ./tiny-fullish.gguf --backend cpu --limit-bytes 1048576",
    "  yvex fullmodel materialize --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu",
    "  yvex fullmodel descriptor --model ./candidate.gguf --target deepseek4-v4-flash --backend cpu --limit-tensors 40",
    "  yvex fullmodel family-runtime --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu",
    "\nfullmodel report:", "  inventory and placement pressure report."};

static const char *const literal_lines_43[] = {
    "\nfullmodel materialization-plan:", "  planned placement phases and materialization preflight only.",
    "\nfullmodel materialization proof:",
    "  controlled proof over a tiny full-ish GGUF tensor set, or a clean refusal for selected/runtime-"
        "slice and incomplete artifacts.",
    "\nfullmodel descriptor:",
    "  planning/reporting boundary for tensor roles, tensor collections, residency expectations, graph "
        "requirements, prefill/KV/decode/logits/sampling requirements, output-head/tokenizer requirements, "
        "backend requirements, and blockers.",
    "\nfullmodel family-runtime:",
    "  maps descriptor facts into model-family runtime adapter facts. DeepSeek is the first concrete "
        "family report target. Qwen/Metal remains planned unless separately implemented.",
    "\nFullmodel report reads GGUF metadata and tensor-directory facts only. Materialization-plan reuses "
        "those inventory facts to plan collection placement, residency, backend fit, blockers, and cleanup. "
        "Materialize allocates and releases only the bounded required proof tensors that pass role coverage "
        "and byte-limit checks. Descriptor builds a runtime requirement report only. Family-runtime maps "
        "descriptor facts into family-specific tensor roles, collection adapters, attention/KV/MoE/output "
        "requirements, graph requirements, blockers, and next-row dependencies. These reports do not execute "
        "the model, materialize full weights, run graph execution, write real KV, produce real logits, sample "
        "real vocabulary tokens, generate, evaluate, benchmark, or report throughput. They report why full "
        "transformer prefill, decode, logits, and generation remain unsupported.",
    "Boundary: no full model execution, no inference readiness, no DeepSeek generation, no provider "
        "generation, no streaming generation, no eval, no benchmark, no throughput."};

/* Purpose: Compute artifact kind name for its CLI invariant (`artifact_kind_name`). */
static const char *artifact_kind_name(yvex_model_artifact_report_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_ARTIFACT_REPORT_LIST: return "list";
    case YVEX_MODEL_ARTIFACT_REPORT_CHECK: return "check";
    case YVEX_MODEL_ARTIFACT_REPORT_STATUS:
    default: return "status";
    }
}

/* Render one source-backed prepare refusal from typed report facts. */
/* Purpose: Render model prepare source report render from typed facts (`model_prepare_source_report_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void model_prepare_source_report_render(
    const yvex_models_prepare_source_report *report,
    yvex_models_output_mode mode)
{
    yvex_render_out out;

    if (!report) return;
    if (mode != YVEX_MODELS_OUTPUT_AUDIT) {
        render_out_init(&out, stdout, YVEX_RENDER_MODE_PORCELAIN);
        render_report_title(&out, "models prepare", report->target_id, "blocked");
        render_fields3(&out, "family", report->family,
                       "source", report->source_status,
                       "artifact", report->artifact_status);
        render_fields2(&out, "plan", "full-gguf planned",
                       "emission", report->artifact_emission_status);
        render_top_blocker(&out, report->top_blocker);
        render_next(&out, report->next);
        render_kv(&out, "boundary",
                  "prepare dry-run only; no artifact emission/runtime/generation");
        return;
    }
    render_out_init(&out, stdout, YVEX_RENDER_MODE_AUDIT);
    render_section(&out, "models: prepare");
    render_object_fields(stdout, report, prepare_identity_fields,
                         sizeof(prepare_identity_fields) /
                             sizeof(prepare_identity_fields[0]));
    model_stage_print("target", "unsupported");
    model_print_runtime_generation("not-performed");
    render_object_fields(stdout, report, prepare_result_fields,
                         sizeof(prepare_result_fields) /
                             sizeof(prepare_result_fields[0]));
}

/* Purpose: Render model artifacts render from typed facts (`yvex_model_artifacts_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_model_artifacts_render(FILE *fp,
                                yvex_model_artifact_render_mode mode,
                                const yvex_model_artifact_report *report)
{
    unsigned long i;

    (void)mode;
    if (!report) return 1;
    yvex_cli_out_kv_str(fp, "report", "model-artifacts");
    yvex_cli_out_kv_str(fp, "kind", artifact_kind_name(report->kind));
    yvex_cli_out_kv_str(fp, "status", report->status ? report->status : "unknown");
    yvex_cli_out_kv_str(fp, "alias", report->alias ? report->alias : "");
    yvex_cli_out_kv_str(fp, "path", report->path ? report->path : "");
    yvex_cli_out_kv_bool(fp, "execution_ready", report->execution_ready);
    for (i = 0; i < report->row_count; ++i) {
        yvex_cli_out_writef(fp, "row_%lu: %s %s %s\n", i,
                            report->rows[i].name ? report->rows[i].name : "",
                            report->rows[i].status ? report->rows[i].status : "",
                            report->rows[i].detail ? report->rows[i].detail : "");
    }
    yvex_cli_out_kv_str(fp, "reason", report->reason ? report->reason : "");
    yvex_cli_out_kv_str(fp, "boundary", report->boundary ? report->boundary : "");
    yvex_cli_out_kv_str(fp, "next_row", report->next_row ? report->next_row : "");
    return 0;
}

/* Purpose: Render model artifacts render help from typed facts (`yvex_model_artifacts_render_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_model_artifacts_render_help(FILE *fp)
{
    yvex_cli_out_line(fp, "usage: yvex models <action> [TARGET] [options]");
    yvex_cli_out_line(fp, "actions: list, current, use, add, remove, scan, check, prepare, artifacts");
    yvex_cli_out_line(fp, "boundary: model artifact registry reports are not runtime generation");
    return 0;
}

/* Purpose: Render print fullmodel common boundaries from typed facts (`print_fullmodel_common_boundaries`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void print_fullmodel_common_boundaries(void)
{
    yvex_cli_out_lines(stdout, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
}

/* Purpose: Compute fullmodel descriptor role collection for its CLI invariant (`fullmodel_descriptor_role_collection`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
/* Purpose: Render fullmodel print family runtime phases from typed facts (`fullmodel_print_family_runtime_phases`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_print_family_runtime_phases(const char *adapter_status,
                                                  const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-descriptor",
        "family-profile",
        "role-adapter",
        "collection-adapter",
        "attention-rules",
        "position-rules",
        "kv-rules",
        "moe-rules",
        "mlp-rules",
        "output-head-rules",
        "tokenizer-rules",
        "graph-requirements",
        "runtime-phase-blockers",
        "adapter-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "skipped";
        } else if (strcmp(phases[i], "role-adapter") == 0 ||
                   strcmp(phases[i], "collection-adapter") == 0) {
            status = adapter_status ? adapter_status : "partial";
        } else if (strcmp(phases[i], "attention-rules") == 0 ||
                   strcmp(phases[i], "position-rules") == 0 ||
                   strcmp(phases[i], "kv-rules") == 0 ||
                   strcmp(phases[i], "moe-rules") == 0 ||
                   strcmp(phases[i], "mlp-rules") == 0 ||
                   strcmp(phases[i], "output-head-rules") == 0 ||
                   strcmp(phases[i], "tokenizer-rules") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "runtime-phase-blockers") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "skipped";
        }
        model_phase_print("family_runtime_phase", i, phases[i], status, "planned");
    }
}

/* Purpose: Compute fullmodel attention rule status for its CLI invariant (`fullmodel_attention_rule_status`). */
static const char *fullmodel_attention_rule_status(const yvex_fullmodel_collections *collections)
{
    if (!collections ||
        !collections->has_attention_q ||
        !collections->has_attention_k ||
        !collections->has_attention_v ||
        !collections->has_attention_out) {
        return "blocked-missing-qkv";
    }
    return "blocked-full-transformer-integration";
}

/* Purpose: Render fullmodel print unsupported family from typed facts (`fullmodel_print_unsupported_family`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int fullmodel_print_unsupported_family(
    const yvex_cli_fullmodel_options *options,
    const yvex_model_ref *ref,
    const char *target_id,
    const char *target_class,
    const char *backend,
    const char *requested,
    const char *detected)
{
    const char *status = strcmp(detected, "unknown") == 0
                             ? "fullmodel-family-runtime-fail"
                             : "fullmodel-family-runtime-unsupported";

    yvex_cli_out_writef(stdout, "status: %s\n", status);
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n",
                        target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "family: %s\n", detected);
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected);
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_lines(stdout, literal_lines_9, sizeof(literal_lines_9) / sizeof(literal_lines_9[0]));
    fullmodel_print_family_runtime_phases("unsupported", "resolve-family");
    yvex_cli_out_writef(stdout,
                        "reason: requested family is not supported by family-runtime report\n");
    return 5;
}

/* Purpose: Render fullmodel print family runtime report from typed facts (`fullmodel_print_family_runtime_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int fullmodel_print_family_runtime_report(const yvex_cli_fullmodel_options *options,
                                                 yvex_model_ref *ref,
                                                 yvex_model_context *ctx,
                                                 const char *target_id,
                                                 const char *target_class,
                                                 unsigned long long artifact_bytes,
                                                 yvex_arch arch,
                                                 unsigned long long tensor_count,
                                                 unsigned long long total_tensor_bytes,
                                                 const yvex_fullmodel_collections *collections,
                                                 const char *role_coverage,
                                                 const char *missing_roles,
                                                 const char *unsupported_roles,
                                                 int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    size_t role_index;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = model_requested_family(options ? options->family : NULL);
    const char *detected = fullmodel_detect_family(options, arch, target_id);
    const char *adapter_status = selected_target ? "partial" :
                                 (role_coverage && strcmp(role_coverage, "complete") == 0
                                      ? "complete"
                                      : "partial");
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);
    int has_output = collections && collections->has_output_head;
    int supported_family = fullmodel_family_request_matches(requested, detected) &&
                           strcmp(detected, "deepseek") == 0;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    yvex_cli_out_writef(stdout, "family_runtime: report\n");
    if (!supported_family) {
        return fullmodel_print_unsupported_family(options, ref, target_id,
                                                   target_class, backend,
                                                   requested, detected);
    }

    yvex_cli_out_writef(stdout, "status: fullmodel-family-runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "family: deepseek\n");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected);
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_writef(stdout, "family_adapter: deepseek-runtime-report\n");
    yvex_cli_out_writef(stdout, "family_adapter_status: %s\n", adapter_status);
    yvex_cli_out_lines(stdout, literal_lines_10, sizeof(literal_lines_10) / sizeof(literal_lines_10[0]));

    yvex_cli_out_writef(stdout, "descriptor_status: %s\n", adapter_status);
    yvex_cli_out_lines(stdout, literal_lines_11, sizeof(literal_lines_11) / sizeof(literal_lines_11[0]));
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));

    yvex_cli_out_writef(stdout, "role_adapter_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "collection_adapter_status: %s\n", adapter_status);
    yvex_cli_out_writef(stdout, "attention_rule_status: %s\n", fullmodel_attention_rule_status(collections));
    yvex_cli_out_writef(stdout, "attention_rules: %s\n", fullmodel_attention_rule_status(collections));
    yvex_cli_out_lines(stdout, literal_lines_12, sizeof(literal_lines_12) / sizeof(literal_lines_12[0]));
    yvex_cli_out_writef(stdout, "mlp_rule_status: %s\n",
        has_mlp ? "blocked-full-transformer-integration" : "blocked-missing-mlp");
    yvex_cli_out_writef(stdout, "output_head_rule_status: %s\n",
        has_output ? "blocked-logits-runtime" : "blocked-missing-output-head");
    yvex_cli_out_writef(stdout, "tokenizer_rule_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked-missing-tokenizer-metadata");
    yvex_cli_out_lines(stdout, literal_pair_14, sizeof(literal_pair_14) / sizeof(literal_pair_14[0]));

    for (role_index = 0u;
         role_index < sizeof(family_runtime_roles) / sizeof(family_runtime_roles[0]);
         ++role_index) {
        const fullmodel_role_projection *role = &family_runtime_roles[role_index];
        yvex_cli_out_writef(stdout, "%s: %s\n", role->key,
                            fullmodel_role_status_from_tensor(ctx, collections, role->role));
    }
    yvex_cli_out_writef(stdout, "moe_expert_roles: %s\n",
           collections && collections->has_moe_expert ? "present" : "missing");

    yvex_cli_out_lines(stdout, literal_lines_13, sizeof(literal_lines_13) / sizeof(literal_lines_13[0]));
    yvex_cli_out_writef(stdout, "router_present: %s\n", collections && collections->has_moe_router ? "true" : "false");
    yvex_cli_out_writef(stdout, "moe_router_present: %s\n",
        collections && collections->has_moe_router ? "true" : "false");
    yvex_cli_out_writef(stdout, "expert_tensors_present: %s\n",
        collections && collections->has_moe_expert ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_14, sizeof(literal_lines_14) / sizeof(literal_lines_14[0]));
    yvex_cli_out_writef(stdout, "output_head_present: %s\n", has_output ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    yvex_cli_out_writef(stdout, "vocab_size: %s\n", has_output ? "from-output-head-shape" : "unknown");
    yvex_cli_out_lines(stdout, literal_lines_15, sizeof(literal_lines_15) / sizeof(literal_lines_15[0]));
    yvex_cli_out_writef(stdout, "graph.full_transformer_attention: %s\n",
        has_attention ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "full_transformer_graph_ready: false\n");

    yvex_cli_out_writef(stdout, "backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    yvex_cli_out_writef(stdout, "backend_available: %s\n", fit.available ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_memory_known: %s\n", fit.memory_known ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend_fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");

    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    yvex_cli_out_lines(stdout, literal_lines_16, sizeof(literal_lines_16) / sizeof(literal_lines_16[0]));
    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n",
           selected_target
               ? "selected runtime slice is incomplete; attention Q/K/V/O tensors missing; output head "
                   "missing; real transformer prefill unsupported; real attention-backed KV unsupported; "
                   "real DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary "
                   "sampling unsupported"
               : "real transformer prefill unsupported; real attention-backed KV unsupported; real "
                   "DeepSeek decode unsupported; real output-head logits unsupported; real vocabulary "
                   "sampling unsupported");
    yvex_cli_out_lines(stdout, literal_lines_17, sizeof(literal_lines_17) / sizeof(literal_lines_17[0]));
    fullmodel_print_family_runtime_phases(adapter_status, NULL);
    return 0;
}

/* Purpose: Render fullmodel print materialize phase set from typed facts (`fullmodel_print_materialize_phase_set`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void fullmodel_print_materialize_phase_set(const char *terminal_phase,
                                                  const char *failed_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "artifact-identity",
        "tensor-inventory",
        "role-coverage",
        "placement-plan",
        "memory-budget",
        "backend-preflight",
        "materialize-embedding",
        "materialize-normalization",
        "materialize-attention",
        "materialize-mlp",
        "materialize-moe",
        "materialize-output",
        "materialize-tokenizer",
        "cleanup",
        "complete"
    };
    int failed_seen = 0;
    unsigned int i;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "planned";
        if (failed_phase && failed_phase[0] && strcmp(failed_phase, phases[i]) == 0) {
            status = "fail";
            failed_seen = 1;
        } else if (!failed_seen &&
                   terminal_phase &&
                   (strcmp(terminal_phase, "complete") == 0 ||
                    strcmp(terminal_phase, phases[i]) == 0)) {
            status = "pass";
        } else if (failed_seen) {
            status = "skipped";
        }
        if (strcmp(phases[i], "materialize-moe") == 0 && !failed_seen) {
            status = "skipped";
        }
        model_phase_print("materialization_phase", i, phases[i], status, "planned");
    }
}

/* Purpose: Render fullmodel print materialize report from typed facts (`fullmodel_print_materialize_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_materialize_report(const fullmodel_materialize_report *report)
{
    static const fullmodel_materialize_report empty_report;
    const yvex_cli_fullmodel_options *options;

    if (!report) report = &empty_report;
    options = report->options;

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "fullmodel-materialize: %s model=%s backend=%s\n",
               report && report->status ? report->status : "fullmodel-materialize-fail",
               options->model ? options->model : "",
               options->backend ? options->backend : "cpu");
        yvex_cli_out_writef(stdout, "reason: %s\n", report && report->failed_reason ? report->failed_reason : "none");
        yvex_cli_out_writef(stdout, "bytes: materialized=%llu required=%llu\n",
               report ? report->materialized_tensor_bytes : 0ull,
               report ? report->required_tensor_bytes : 0ull);
        yvex_cli_out_writef(stdout, "cleanup: %s\n",
            report && report->cleanup_status ? report->cleanup_status : "not-needed");
        yvex_cli_out_writef(stdout, "boundary: bounded proof/refusal only, no full model execution\n");
        yvex_cli_out_writef(stdout, "status: %s\n",
            report && report->status ? report->status : "fullmodel-materialize-fail");
        return;
    }

    yvex_cli_out_writef(stdout, "fullmodel: materialize\n");
    yvex_cli_out_writef(stdout, "status: %s\n",
                        report->status ? report->status : "fullmodel-materialize-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n",
                        report->model_resolved_path ? report->model_resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", report->target_id ? report->target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n",
                        report->target_class ? report->target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "dry_run: %s\n", options && options->dry_run ? "true" : "false");
    yvex_cli_out_writef(stdout, "plan_only: %s\n", options && options->plan_only ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_dir: %s\n", options && options->report_dir ? options->report_dir : "none");
    render_object_fields(stdout, report, materialize_admission_fields,
                         sizeof(materialize_admission_fields) /
                             sizeof(materialize_admission_fields[0]));
    yvex_cli_out_lines(stdout, literal_lines_18, sizeof(literal_lines_18) / sizeof(literal_lines_18[0]));
    render_object_fields(stdout, report, materialize_lifecycle_fields,
                         sizeof(materialize_lifecycle_fields) /
                             sizeof(materialize_lifecycle_fields[0]));
    render_object_fields(stdout, report, materialize_accounting_fields,
                         sizeof(materialize_accounting_fields) /
                             sizeof(materialize_accounting_fields[0]));
    fullmodel_print_materialize_phase_set(report->phase ? report->phase : "failed",
                                          report->failed_phase);
}

/* Purpose: Render fullmodel print report normal from typed facts (`fullmodel_print_report_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_report_normal(const yvex_cli_fullmodel_options *options,
                                          const char *status,
                                          const char *target_id,
                                          const char *target_class,
                                          const char *role_coverage,
                                          const char *top_blocker,
                                          const char *next)
{
    yvex_cli_out_writef(stdout, "fullmodel: report model=%s backend=%s\n",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "report-only");
    yvex_cli_out_writef(stdout, "target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
        top_blocker ? top_blocker : "missing-full-runtime-tensor-coverage");
    yvex_cli_out_writef(stdout, "next: %s\n", next ? next : "tensor/source/artifact row required");
    yvex_cli_out_writef(stdout, "boundary: report-only, no full model execution\n");
}

/* Purpose: Render fullmodel print plan normal from typed facts (`fullmodel_print_plan_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_plan_normal(const yvex_cli_fullmodel_options *options,
                                        const char *status,
                                        const char *target_class,
                                        const char *fit,
                                        const char *top_blocker)
{
    yvex_cli_out_writef(stdout, "materialization-plan: %s model=%s backend=%s\n",
           status ? status : "blocked",
           options && options->model ? options->model : "",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "residency: %s\n", options && options->residency ? options->residency : "resident");
    yvex_cli_out_writef(stdout, "class: %s\n", target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "fit: %s\n", fit ? fit : "unknown");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
        top_blocker ? top_blocker : "full-runtime candidate artifact required");
    yvex_cli_out_lines(stdout, literal_pair_13, sizeof(literal_pair_13) / sizeof(literal_pair_13[0]));
}

/* Purpose: Render fullmodel print descriptor normal from typed facts (`fullmodel_print_descriptor_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_descriptor_normal(const yvex_cli_fullmodel_options *options,
                                              const char *target_id,
                                              const char *target_class,
                                              const char *role_coverage,
                                              const char *missing_roles)
{
    yvex_cli_out_writef(stdout, "report: fullmodel-descriptor\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "backend: %s target=%s class=%s\n",
           options && options->backend ? options->backend : "cpu",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "role_coverage: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime integration missing"
               : "missing-full-runtime-tensor-coverage");
    yvex_cli_out_lines(stdout, literal_pair_12, sizeof(literal_pair_12) / sizeof(literal_pair_12[0]));
}

/* Purpose: Render fullmodel print family runtime normal from typed facts (`fullmodel_print_family_runtime_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void fullmodel_print_family_runtime_normal(const yvex_cli_fullmodel_options *options,
                                                  const char *target_id,
                                                  const char *target_class,
                                                  const char *role_coverage,
                                                  const char *missing_roles)
{
    yvex_cli_out_writef(stdout, "report: family-runtime\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "family: %s backend=%s\n",
           options && options->family ? options->family : "auto",
           options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "target: %s class=%s\n",
           target_id ? target_id : "path",
           target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "status: %s\n", role_coverage ? role_coverage : "partial");
    yvex_cli_out_writef(stdout, "top_blocker: %s\n",
           missing_roles && strcmp(missing_roles, "none") == 0
               ? "runtime family adapter missing"
               : "missing family runtime tensor roles");
    yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
}

/* Purpose: Render print fullmodel source only report from typed facts (`print_fullmodel_source_only_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_source_only_report(const char *target,
                                              const char *backend)
{
    yvex_cli_out_lines(stdout, literal_pair_11, sizeof(literal_pair_11) / sizeof(literal_pair_11[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_lines(stdout, literal_lines_19, sizeof(literal_lines_19) / sizeof(literal_lines_19[0]));
    yvex_cli_out_writef(stdout, "backend: %s\n", backend ? backend : "cpu");
    yvex_cli_out_lines(stdout, literal_lines_20, sizeof(literal_lines_20) / sizeof(literal_lines_20[0]));
    yvex_cli_out_writef(stdout, "cuda_context_available: %s\n",
        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_21, sizeof(literal_lines_21) / sizeof(literal_lines_21[0]));
    print_fullmodel_common_boundaries();
    return 0;
}

/* Purpose: Render print fullmodel source only plan from typed facts (`print_fullmodel_source_only_plan`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_source_only_plan(const yvex_cli_fullmodel_options *options,
                                            const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    yvex_cli_out_lines(stdout, literal_pair_10, sizeof(literal_pair_10) / sizeof(literal_pair_10[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_lines(stdout, literal_lines_22, sizeof(literal_lines_22) / sizeof(literal_lines_22[0]));
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "residency: %s\n", residency);
    yvex_cli_out_lines(stdout, literal_lines_23, sizeof(literal_lines_23) / sizeof(literal_lines_23[0]));
    yvex_cli_out_writef(stdout, "plan_id: fullmodel-materialization:%s:%s:%s\n",
           target ? target : "source-only-target", backend, residency);
    yvex_cli_out_lines(stdout, literal_pair_9, sizeof(literal_pair_9) / sizeof(literal_pair_9[0]));
    yvex_cli_out_writef(stdout, "plan_backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "plan_residency: %s\n", residency);
    yvex_cli_out_lines(stdout, literal_lines_24, sizeof(literal_lines_24) / sizeof(literal_lines_24[0]));
    fullmodel_print_phase(0u, "preflight", "unsupported",
                          0ull, 0ull, "not-applicable", 1, 1,
                          "YVEX-produced GGUF artifact required before planning");
    (void)fullmodel_print_blocker(0u, "artifact", "fatal",
                                  "YVEX-produced GGUF artifact required before materialization planning",
                                  1, 1);
    yvex_cli_out_lines(stdout, literal_lines_25, sizeof(literal_lines_25) / sizeof(literal_lines_25[0]));
    return 5;
}

/* Purpose: Render print fullmodel source only materialize from typed facts (`print_fullmodel_source_only_materialize`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_source_only_materialize(const yvex_cli_fullmodel_options *options,
                                                   const char *target)
{
    fullmodel_materialize_report report;

    memset(&report, 0, sizeof(report));
    report.options = options;
    report.status = "fullmodel-materialize-unsupported";
    report.model_resolved_path = "source-only-target";
    report.target_id = target ? target : "source-only-target";
    report.target_class = "official-source-huge-model";
    report.artifact_identity_status = "not-applicable";
    report.tensor_inventory_status = "not-performed-source-only-target";
    report.required_role_coverage = "none";
    report.missing_required_roles = "YVEX-produced-GGUF-artifact";
    report.unsupported_required_roles = "GLM-runtime-family-mapping,GLM-YVEX-produced-GGUF-emission,GLM-"
        "runtime-execution";
    report.placement_plan_status = "unsupported";
    report.memory_budget_status = "not-performed";
    report.backend_preflight_status = "not-performed";
    report.materialization_mode = "source-only-refusal";
    report.full_model_materialization = "unsupported-source-only";
    report.full_model_materialization_proof = "unsupported";
    report.phase = "failed";
    report.failed_phase = "resolve-model";
    report.failed_reason = "YVEX-produced-GGUF-artifact-missing";
    report.cleanup_attempted = "false";
    report.cleanup_status = "not-needed";
    report.cleanup_idempotent = "true";
    report.owned_state_released = "true";
    report.partial_materialization = "false";
    report.residency_plan = "future-YVEX-produced-artifact-required";
    report.runtime_blockers = "source-only target; YVEX-produced GGUF emission planned; full model runtime unsupported";
    fullmodel_print_materialize_report(&report);
    return 5;
}

/* Purpose: Render print fullmodel source only descriptor from typed facts (`print_fullmodel_source_only_descriptor`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_source_only_descriptor(const yvex_cli_fullmodel_options *options,
                                                  const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";

    yvex_cli_out_lines(stdout, literal_pair_8, sizeof(literal_pair_8) / sizeof(literal_pair_8[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_lines(stdout, literal_lines_26, sizeof(literal_lines_26) / sizeof(literal_lines_26[0]));
    fullmodel_print_descriptor_phases("unsupported", "unsupported", "resolve-model");
    return 5;
}

/* Purpose: Render print fullmodel source only family runtime from typed facts
 * (`print_fullmodel_source_only_family_runtime`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_source_only_family_runtime(const yvex_cli_fullmodel_options *options,
                                                      const char *target)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = model_requested_family(options ? options->family : NULL);

    yvex_cli_out_lines(stdout, literal_pair_7, sizeof(literal_pair_7) / sizeof(literal_pair_7[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : target ? target : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: source-only-target\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target ? target : "");
    yvex_cli_out_writef(stdout, "target_class: official-source-huge-model\n");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_lines(stdout, literal_pair_6, sizeof(literal_pair_6) / sizeof(literal_pair_6[0]));
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_lines(stdout, literal_lines_27, sizeof(literal_lines_27) / sizeof(literal_lines_27[0]));
    fullmodel_print_family_runtime_phases("unsupported", "resolve-model");
    return 5;
}

/* Purpose: Render print fullmodel failed plan fields from typed facts (`print_fullmodel_failed_plan_fields`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void print_fullmodel_failed_plan_fields(const yvex_cli_fullmodel_options *options,
                                               const char *phase,
                                               const char *reason,
                                               unsigned long long artifact_bytes)
{
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *residency = options && options->residency ? options->residency : "resident";

    yvex_cli_out_writef(stdout, "residency: %s\n", residency);
    yvex_cli_out_lines(stdout, literal_lines_28, sizeof(literal_lines_28) / sizeof(literal_lines_28[0]));
    yvex_cli_out_writef(stdout, "plan_backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "plan_residency: %s\n", residency);
    yvex_cli_out_lines(stdout, literal_lines_29, sizeof(literal_lines_29) / sizeof(literal_lines_29[0]));
    fullmodel_print_phase(0u, phase ? phase : "preflight", "blocked",
                          0ull, artifact_bytes, "not-applicable", 1, 1,
                          reason ? reason : "inventory failed");
    (void)fullmodel_print_blocker(0u, "inventory", "fatal",
                                  reason ? reason : "inventory failed",
                                  1, 1);
    yvex_cli_out_lines(stdout, literal_lines_30, sizeof(literal_lines_30) / sizeof(literal_lines_30[0]));
}

/* Purpose: Render print fullmodel missing report from typed facts (`print_fullmodel_missing_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_missing_report(const yvex_cli_fullmodel_options *options,
                                          const char *resolved_path)
{
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    if (is_materialize) {
        fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = resolved_path ? resolved_path : "";
        report.target_id = options && options->target ? options->target : "unknown";
        report.target_class = "unresolved-artifact";
        report.artifact_identity_status = "unavailable";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "artifact";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "resolve-model";
        report.failed_reason = "artifact path does not exist";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "artifact path missing";
        fullmodel_print_materialize_report(&report);
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_descriptor) {
        yvex_cli_out_lines(stdout, literal_pair_5, sizeof(literal_pair_5) / sizeof(literal_pair_5[0]));
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
        yvex_cli_out_writef(stdout, "target_class: unresolved-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_lines(stdout, literal_lines_31, sizeof(literal_lines_31) / sizeof(literal_lines_31[0]));
        fullmodel_print_descriptor_phases("fail", "fail", "resolve-model");
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    if (is_family_runtime) {
        yvex_cli_out_lines(stdout, literal_pair_4, sizeof(literal_pair_4) / sizeof(literal_pair_4[0]));
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
        yvex_cli_out_writef(stdout, "target_class: unresolved-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_lines(stdout, literal_pair_3, sizeof(literal_pair_3) / sizeof(literal_pair_3[0]));
        yvex_cli_out_writef(stdout, "family_requested: %s\n", model_requested_family(options ? options->family : NULL));
        yvex_cli_out_lines(stdout, literal_lines_32, sizeof(literal_lines_32) / sizeof(literal_lines_32[0]));
        fullmodel_print_family_runtime_phases("failed", "resolve-model");
        yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
        return exit_for_status(YVEX_ERR_IO);
    }

    yvex_cli_out_writef(stdout, "fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    yvex_cli_out_writef(stdout, "status: %s\n",
        is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", options && options->target ? options->target : "unknown");
    yvex_cli_out_lines(stdout, literal_lines_33, sizeof(literal_lines_33) / sizeof(literal_lines_33[0]));
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_lines(stdout, literal_lines_34, sizeof(literal_lines_34) / sizeof(literal_lines_34[0]));
    yvex_cli_out_writef(stdout, "cuda_context_available: %s\n",
        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_35, sizeof(literal_lines_35) / sizeof(literal_lines_35[0]));
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "preflight",
                                           "artifact path does not exist",
                                           0ull);
    }
    yvex_cli_out_writef(stdout, "reason: artifact path does not exist\n");
    return exit_for_status(YVEX_ERR_IO);
}

/* Purpose: Parse print fullmodel parse failure report into typed CLI state (`print_fullmodel_parse_failure_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int print_fullmodel_parse_failure_report(const yvex_cli_fullmodel_options *options,
                                                const yvex_model_ref *ref,
                                                const char *reason,
                                                int rc)
{
    unsigned long long artifact_bytes = 0ull;
    int is_plan = options &&
                  options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZATION_PLAN;
    int is_materialize = options &&
                         options->command == YVEX_FULLMODEL_COMMAND_MATERIALIZE;
    int is_descriptor = options &&
                        options->command == YVEX_FULLMODEL_COMMAND_DESCRIPTOR;
    int is_family_runtime = options &&
                            options->command == YVEX_FULLMODEL_COMMAND_FAMILY_RUNTIME;

    fullmodel_file_size(ref && ref->path ? ref->path : "", &artifact_bytes);
    if (is_materialize) {
        fullmodel_materialize_report report;
        memset(&report, 0, sizeof(report));
        report.options = options;
        report.status = "fullmodel-materialize-fail";
        report.model_resolved_path = ref && ref->path ? ref->path : "";
        report.target_id = options && options->target
            ? options->target
            : (ref && ref->alias && ref->alias[0] ? ref->alias : "path");
        report.target_class = "GGUF-artifact";
        report.artifact_identity_status = "not-checked";
        report.tensor_inventory_status = "failed";
        report.required_role_coverage = "none";
        report.missing_required_roles = "parseable-GGUF-tensor-directory";
        report.unsupported_required_roles = "full-runtime-model";
        report.placement_plan_status = "failed";
        report.memory_budget_status = "not-performed";
        report.backend_preflight_status = "not-performed";
        report.materialization_mode = "none";
        report.full_model_materialization = "failed";
        report.full_model_materialization_proof = "fail";
        report.phase = "failed";
        report.failed_phase = "tensor-inventory";
        report.failed_reason = reason ? reason : "parse failed";
        report.cleanup_attempted = "false";
        report.cleanup_status = "not-needed";
        report.cleanup_idempotent = "true";
        report.owned_state_released = "true";
        report.partial_materialization = "false";
        report.residency_plan = "unavailable";
        report.runtime_blockers = "GGUF metadata or tensor directory parse failed";
        fullmodel_print_materialize_report(&report);
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_descriptor) {
        yvex_cli_out_lines(stdout, literal_pair_2, sizeof(literal_pair_2) / sizeof(literal_pair_2[0]));
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n",
            options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        yvex_cli_out_writef(stdout, "target_class: GGUF-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_lines(stdout, literal_lines_36, sizeof(literal_lines_36) / sizeof(literal_lines_36[0]));
        fullmodel_print_descriptor_phases("fail", "fail", "tensor-inventory");
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    if (is_family_runtime) {
        yvex_cli_out_lines(stdout, literal_pair_1, sizeof(literal_pair_1) / sizeof(literal_pair_1[0]));
        yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
        yvex_cli_out_writef(stdout, "target_id: %s\n",
            options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
        yvex_cli_out_writef(stdout, "target_class: GGUF-artifact\n");
        yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
        yvex_cli_out_lines(stdout, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
        yvex_cli_out_writef(stdout, "family_requested: %s\n", model_requested_family(options ? options->family : NULL));
        yvex_cli_out_lines(stdout, literal_lines_37, sizeof(literal_lines_37) / sizeof(literal_lines_37[0]));
        fullmodel_print_family_runtime_phases("failed", "load-descriptor");
        yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
        yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
        return exit_for_status(rc);
    }
    yvex_cli_out_writef(stdout, "fullmodel: %s\n", is_plan ? "materialization-plan" : "report");
    yvex_cli_out_writef(stdout, "status: %s\n",
        is_plan ? "fullmodel-materialization-plan-fail" : "fullmodel-report-fail");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n",
        options && options->target ? options->target : (ref && ref->alias && ref->alias[0] ? ref->alias : "path"));
    yvex_cli_out_lines(stdout, literal_lines_38, sizeof(literal_lines_38) / sizeof(literal_lines_38[0]));
    yvex_cli_out_writef(stdout, "artifact_bytes: %llu\n", artifact_bytes);
    yvex_cli_out_lines(stdout, literal_lines_39, sizeof(literal_lines_39) / sizeof(literal_lines_39[0]));
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_lines(stdout, literal_lines_40, sizeof(literal_lines_40) / sizeof(literal_lines_40[0]));
    yvex_cli_out_writef(stdout, "cuda_context_available: %s\n",
        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_41, sizeof(literal_lines_41) / sizeof(literal_lines_41[0]));
    print_fullmodel_common_boundaries();
    if (is_plan) {
        print_fullmodel_failed_plan_fields(options,
                                           "tensor-directory",
                                           reason ? reason : "parse failed",
                                           artifact_bytes);
    }
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "parse failed");
    return exit_for_status(rc);
}

/* Purpose: Render model artifacts surface fullmodel help from typed facts
 * (`yvex_model_artifacts_surface_fullmodel_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_model_artifacts_surface_fullmodel_help(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex fullmodel report --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--"
            "limit-tensors N] [--registry FILE] [--audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp,
        "usage: yvex fullmodel materialization-plan --model FILE_OR_ALIAS [--backend cpu|cuda] [--"
            "residency resident|host-staged|ssd-staged|hybrid] [--target TARGET] [--limit-tensors N] [--"
            "registry FILE] [--audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp,
        "usage: yvex fullmodel materialize --model FILE_OR_ALIAS [--backend cpu|cuda] [--registry FILE] [--"
            "dry-run] [--plan-only] [--require-role ROLE] [--require-collection COLLECTION] [--limit-bytes N] "
            "[--fail-after-phase PHASE] [--report-dir DIR] [--audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp,
        "usage: yvex fullmodel descriptor --model FILE_OR_ALIAS [--backend cpu|cuda] [--target TARGET] [--"
            "format text] [--limit-tensors N] [--require-role ROLE] [--require-collection COLLECTION] [--"
            "include-blockers] [--include-placement] [--include-graph] [--include-kv] [--include-logits] [--"
            "audit | --output normal|table|audit]\n");
    yvex_cli_out_writef(fp,
        "usage: yvex fullmodel family-runtime --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--"
            "backend cpu|cuda] [--include-blockers] [--include-roles] [--include-graph] [--include-kv] [--"
            "include-moe] [--include-output] [--audit | --output normal|table|audit]\n");
    yvex_cli_out_lines(fp, literal_lines_42, sizeof(literal_lines_42) / sizeof(literal_lines_42[0]));
    yvex_cli_out_writef(fp, "  Default output is compact. Use --audit for full diagnostic fields.\n");
    yvex_cli_out_lines(fp, literal_lines_43, sizeof(literal_lines_43) / sizeof(literal_lines_43[0]));
}
