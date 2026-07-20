/* Owner: src/cli/render
 * Owns: normal, table, audit, and help text rendering for graph reports.
 * Does not own: graph construction, report building, input parsing, command dispatch, backend primitive execution,
 *   reference comparison, stdout/stderr writer primitives, generation, eval, benchmark, or release
 *   decisions.
 * Invariants: all output goes through src/cli/io writer helpers.
 * Boundary: graph rendering is not graph runtime or generation readiness.
 * Purpose: provide normal, table, audit, and help text rendering for graph reports.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"
#include "src/cli/model_artifacts/private.h"

#include <string.h>

static const char *const literal_lines_0[] = {
    "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda]",
    "       yvex graph check [--suite primitives|block|layers|all] [--backend cpu|cuda]",
    "       yvex graph attention execute --target deepseek4-v4-flash --backend cpu|cuda",
    "           [--artifact FILE] [--models-root DIR] [--probe canonical] [--scope quick|full]",
    "           [--output normal|table|audit|json]",
    "       yvex graph attention execute --target deepseek4-v4-flash --compare-backends",
    "           [--artifact FILE] [--models-root DIR] [--probe canonical] [--scope quick|full]",
    "           [--output normal|table|audit|json]",
    "       yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N",
    "       yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal]",
    "       yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N",
    "       yvex graph --backend cpu|cuda --execute-op --op mlp --hidden-dim N --ffn-dim N --activation "
        "silu --gated [--experts N --expert-id N]",
    "       yvex graph --backend cpu|cuda --execute-block --block fixture --seq-len N --position N --"
        "hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]",
    "       yvex graph --backend cpu|cuda --execute-layers --layers N --block fixture --seq-len N --"
        "position N --hidden-dim N --head-dim N --ffn-dim N [--causal] [--gated]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-fixture [--fixture-token N]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-partial [--partial-token N | --"
        "tokens IDS --token-index N]",
    "       yvex graph --model FILE_OR_ALIAS --backend cpu|cuda --execute-segment --segment embedding-"
        "rmsnorm [--partial-token N | --tokens IDS --token-index N]",
    "",
    "example: yvex graph check --suite primitives --backend cpu",
    "example: yvex graph attention execute --target deepseek4-v4-flash --backend cpu --scope quick",
    "boundary: the attention command executes a canonical production probe over admitted weights; it is not "
        "prompt execution, persistent KV, transformer execution, or generation"
};

/* Purpose: Render graph render body from typed facts (`graph_render_body`). */
static const char *graph_render_body(const yvex_graph_report *report)
{
    return report && report->body ? report->body : "";
}

static const yvex_cli_field_spec graph_guard_fields[] = {
    {"graph_integrity_guard", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, guard_status), "fail"},
    {"graph_execution_phase", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, phase), "preflight"},
    {"graph_kind", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, graph_kind), "unknown"},
    {"integrity_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, integrity_status), "unchecked"},
    {"identity_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, identity_status), "unregistered"},
    {"metadata_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, metadata_status), "unregistered"},
    {"shape_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, shape_status), "unchecked"},
    {"range_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, range_status), "unchecked"},
    {"slice_range_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, slice_range_status), "unchecked"},
    {"backend_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, backend_status), "not-opened"},
    {"backend_op_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, backend_op_status), "unchecked"},
    {"dispatch_attempted", YVEX_CLI_FIELD_BOOL, offsetof(yvex_cli_graph_guard_report, dispatch_attempted), NULL},
    {"reference_read_attempted", YVEX_CLI_FIELD_BOOL,
     offsetof(yvex_cli_graph_guard_report, reference_read_attempted), NULL},
    {"output_allocation_attempted", YVEX_CLI_FIELD_BOOL,
     offsetof(yvex_cli_graph_guard_report, output_allocation_attempted), NULL},
    {"cleanup_attempted", YVEX_CLI_FIELD_BOOL, offsetof(yvex_cli_graph_guard_report, cleanup_attempted), NULL},
    {"cleanup_status", YVEX_CLI_FIELD_TEXT, offsetof(yvex_cli_graph_guard_report, cleanup_status), "not-needed"},
    {"output_bytes_planned", YVEX_CLI_FIELD_U64,
     offsetof(yvex_cli_graph_guard_report, output_bytes_planned), NULL},
    {"output_bytes_allocated", YVEX_CLI_FIELD_U64,
     offsetof(yvex_cli_graph_guard_report, output_bytes_allocated), NULL},
    {"reference_bytes_planned", YVEX_CLI_FIELD_U64,
     offsetof(yvex_cli_graph_guard_report, reference_bytes_planned), NULL},
};

/* Purpose: Render graph guard.
 * Inputs: report.
 * Effects: writes CLI fields.
 * Failure: stream state.
 * Boundary: CLI presentation. */
void yvex_cli_graph_guard_print(const yvex_cli_graph_guard_report *report)
{
    if (!report) return;
    (void)yvex_cli_out_fields(stdout, report, graph_guard_fields,
                              sizeof(graph_guard_fields) / sizeof(graph_guard_fields[0]));
}

/* Purpose: Render graph report.
 * Inputs: stream, mode, report.
 * Effects: writes report body.
 * Failure: typed I/O refusal.
 * Boundary: CLI presentation. */
int yvex_graph_render(FILE *fp,
                      yvex_graph_report_mode mode,
                      const yvex_graph_report *report)
{
    (void)mode;
    return yvex_cli_out_writef(fp, "%s", graph_render_body(report)) < 0 ? YVEX_ERR_IO : YVEX_OK;
}

#define ATTENTION_FIELD(KEY, KIND, MEMBER) \
    {KEY, KIND, offsetof(yvex_graph_attention_operator_result, MEMBER), ""}
#define FIELD_COUNT(FIELDS) (sizeof(FIELDS) / sizeof((FIELDS)[0]))

static const yvex_cli_field_spec attention_base_fields[] = {
    ATTENTION_FIELD("command", YVEX_CLI_FIELD_TEXT_ARRAY, command),
    ATTENTION_FIELD("status", YVEX_CLI_FIELD_TEXT_ARRAY, status),
    ATTENTION_FIELD("target", YVEX_CLI_FIELD_TEXT_ARRAY, target),
    ATTENTION_FIELD("backend", YVEX_CLI_FIELD_TEXT_ARRAY, backend),
    ATTENTION_FIELD("scope", YVEX_CLI_FIELD_TEXT_ARRAY, scope),
    ATTENTION_FIELD("artifact_path", YVEX_CLI_FIELD_TEXT_ARRAY, artifact_path),
};
static const yvex_cli_field_spec attention_target_fields[] = {
    ATTENTION_FIELD("family", YVEX_CLI_FIELD_TEXT_ARRAY, family),
    ATTENTION_FIELD("input_class", YVEX_CLI_FIELD_TEXT_ARRAY, input_class),
};
static const yvex_cli_field_spec attention_admission_fields[] = {
    ATTENTION_FIELD("execution_class", YVEX_CLI_FIELD_TEXT_ARRAY, execution_class),
    ATTENTION_FIELD("weights_class", YVEX_CLI_FIELD_TEXT_ARRAY, weights_class),
    ATTENTION_FIELD("artifact_identity", YVEX_CLI_FIELD_TEXT_ARRAY, artifact_identity),
    ATTENTION_FIELD("artifact_bytes_hashed", YVEX_CLI_FIELD_U64, artifact_bytes_hashed),
    ATTENTION_FIELD("artifact_identity_verified", YVEX_CLI_FIELD_BOOL,
                    artifact_identity_verified),
    ATTENTION_FIELD("materialization_identity", YVEX_CLI_FIELD_TEXT_ARRAY, materialization_identity),
    ATTENTION_FIELD("logical_model_identity", YVEX_CLI_FIELD_TEXT_ARRAY, logical_model_identity),
    ATTENTION_FIELD("runtime_numeric_identity", YVEX_CLI_FIELD_TEXT_ARRAY, runtime_numeric_identity),
    ATTENTION_FIELD("runtime_descriptor_identity", YVEX_CLI_FIELD_TEXT_ARRAY, runtime_descriptor_identity),
    ATTENTION_FIELD("attention_plan_identity", YVEX_CLI_FIELD_TEXT_ARRAY, attention_plan_identity),
    ATTENTION_FIELD("main_layers_total", YVEX_CLI_FIELD_U64, main_layers_total),
    ATTENTION_FIELD("bindings_total", YVEX_CLI_FIELD_U64, bindings_total),
    ATTENTION_FIELD("attention_execution_supported", YVEX_CLI_FIELD_BOOL, attention_execution_supported),
    ATTENTION_FIELD("attention_cuda_execution_ready", YVEX_CLI_FIELD_BOOL, attention_cuda_execution_ready),
};
static const yvex_cli_field_spec attention_execution_fields[] = {
    ATTENTION_FIELD("attention_execution_identity", YVEX_CLI_FIELD_TEXT_ARRAY, attention_execution_identity),
    ATTENTION_FIELD("layers_executed", YVEX_CLI_FIELD_U64, layers_executed),
    ATTENTION_FIELD("bindings_executed", YVEX_CLI_FIELD_U64, bindings_executed),
    ATTENTION_FIELD("swa_layers_executed", YVEX_CLI_FIELD_U64, swa_layers_executed),
    ATTENTION_FIELD("csa_layers_executed", YVEX_CLI_FIELD_U64, csa_layers_executed),
    ATTENTION_FIELD("hca_layers_executed", YVEX_CLI_FIELD_U64, hca_layers_executed),
    ATTENTION_FIELD("topk_selected", YVEX_CLI_FIELD_U64, topk_selected),
    ATTENTION_FIELD("hca_ratio", YVEX_CLI_FIELD_U64, hca_ratio),
    ATTENTION_FIELD("output_digest", YVEX_CLI_FIELD_TEXT_ARRAY, output_digest),
};
static const yvex_cli_field_spec attention_cuda_fields[] = {
    ATTENTION_FIELD("cuda_device", YVEX_CLI_FIELD_TEXT_ARRAY, cuda_device),
    ATTENTION_FIELD("compute_capability_major", YVEX_CLI_FIELD_I32, cuda_compute_capability_major),
    ATTENTION_FIELD("compute_capability_minor", YVEX_CLI_FIELD_I32, cuda_compute_capability_minor),
    ATTENTION_FIELD("kernel_launches", YVEX_CLI_FIELD_U64, kernel_launches),
    ATTENTION_FIELD("peak_device_bytes", YVEX_CLI_FIELD_U64, peak_device_bytes),
};
static const yvex_cli_field_spec attention_comparison_fields[] = {
    ATTENTION_FIELD("cpu_output_digest", YVEX_CLI_FIELD_TEXT_ARRAY, cpu_output_digest),
    ATTENTION_FIELD("cuda_output_digest", YVEX_CLI_FIELD_TEXT_ARRAY, cuda_output_digest),
    ATTENTION_FIELD("comparison_contract_identity", YVEX_CLI_FIELD_TEXT_ARRAY, comparison_contract_identity),
    ATTENTION_FIELD("comparison_values", YVEX_CLI_FIELD_U64, comparison_values),
    ATTENTION_FIELD("comparison_finite_values", YVEX_CLI_FIELD_U64, comparison_finite_values),
    ATTENTION_FIELD("comparison_nonfinite_values", YVEX_CLI_FIELD_U64, comparison_nonfinite_values),
    ATTENTION_FIELD("comparison_maximum_absolute_error", YVEX_CLI_FIELD_DOUBLE,
                    comparison_maximum_absolute_error),
    ATTENTION_FIELD("comparison_maximum_relative_error", YVEX_CLI_FIELD_DOUBLE,
                    comparison_maximum_relative_error),
    ATTENTION_FIELD("comparison_rmse", YVEX_CLI_FIELD_DOUBLE, comparison_rmse),
    ATTENTION_FIELD("comparison_passed", YVEX_CLI_FIELD_BOOL, comparison_passed),
    ATTENTION_FIELD("bitwise_equality_observed", YVEX_CLI_FIELD_BOOL, bitwise_equality_observed),
    ATTENTION_FIELD("bitwise_equality_required", YVEX_CLI_FIELD_BOOL, bitwise_equality_required),
};
static const yvex_cli_field_spec attention_failure_fields[] = {
    ATTENTION_FIELD("first_failing_layer", YVEX_CLI_FIELD_U64, first_failing_layer),
    ATTENTION_FIELD("first_failing_coordinate", YVEX_CLI_FIELD_U64, first_failing_coordinate),
};
static const yvex_cli_field_spec attention_provenance_fields[] = {
    ATTENTION_FIELD("source_snapshot_identity", YVEX_CLI_FIELD_TEXT_ARRAY, source_snapshot_identity),
    ATTENTION_FIELD("payload_identity", YVEX_CLI_FIELD_TEXT_ARRAY, payload_identity),
    ATTENTION_FIELD("artifact_transform_identity", YVEX_CLI_FIELD_TEXT_ARRAY, artifact_transform_identity),
    ATTENTION_FIELD("transform_identity", YVEX_CLI_FIELD_TEXT_ARRAY, transform_identity),
    ATTENTION_FIELD("payload_bytes_read", YVEX_CLI_FIELD_U64, payload_bytes_read),
};
static const yvex_cli_field_spec attention_compatibility_fields[] = {
    ATTENTION_FIELD("current_writer_plan_identity", YVEX_CLI_FIELD_TEXT_ARRAY, current_writer_plan_identity),
    ATTENTION_FIELD("payload_plan_identity", YVEX_CLI_FIELD_TEXT_ARRAY, payload_plan_identity),
    ATTENTION_FIELD("payload_byte_identity", YVEX_CLI_FIELD_TEXT_ARRAY, payload_byte_identity),
    ATTENTION_FIELD("physical_payload_compatible", YVEX_CLI_FIELD_BOOL, physical_payload_compatible),
    ATTENTION_FIELD("artifact_rebuild_required", YVEX_CLI_FIELD_BOOL, artifact_rebuild_required),
    ATTENTION_FIELD("materialization_rebuild_required", YVEX_CLI_FIELD_BOOL,
                    materialization_rebuild_required),
    ATTENTION_FIELD("tensor_inventory_equal", YVEX_CLI_FIELD_BOOL, tensor_inventory_equal),
    ATTENTION_FIELD("qtype_equal", YVEX_CLI_FIELD_BOOL, qtype_equal),
    ATTENTION_FIELD("layout_equal", YVEX_CLI_FIELD_BOOL, layout_equal),
    ATTENTION_FIELD("offset_equal", YVEX_CLI_FIELD_BOOL, offset_equal),
    ATTENTION_FIELD("payload_digest_equal", YVEX_CLI_FIELD_BOOL, payload_digest_equal),
};
static const yvex_cli_field_spec attention_reachability_fields[] = {
    ATTENTION_FIELD("operator_command_available", YVEX_CLI_FIELD_BOOL, operator_command_available),
    ATTENTION_FIELD("production_api_available", YVEX_CLI_FIELD_BOOL, production_api_available),
    ATTENTION_FIELD("internal_live_runner_available", YVEX_CLI_FIELD_BOOL, internal_live_runner_available),
    ATTENTION_FIELD("end_user_generation_available", YVEX_CLI_FIELD_BOOL, end_user_generation_available),
};
static const yvex_cli_field_spec attention_reason_field[] = {
    ATTENTION_FIELD("failure_code", YVEX_CLI_FIELD_TEXT_ARRAY, failure_code),
    ATTENTION_FIELD("failure_where", YVEX_CLI_FIELD_TEXT_ARRAY, failure_where),
    ATTENTION_FIELD("reason", YVEX_CLI_FIELD_TEXT_ARRAY, reason),
};
static const yvex_cli_field_spec attention_final_field[] = {
    ATTENTION_FIELD("runtime_generation_ready", YVEX_CLI_FIELD_BOOL, runtime_generation_ready),
};

typedef enum {
    ATTENTION_FIELDS_ALWAYS,
    ATTENTION_FIELDS_TARGET,
    ATTENTION_FIELDS_ADMITTED,
    ATTENTION_FIELDS_COMPLETED,
    ATTENTION_FIELDS_CUDA,
    ATTENTION_FIELDS_COMPARISON,
    ATTENTION_FIELDS_COMPARISON_FAILURE,
    ATTENTION_FIELDS_DETAILED_ADMISSION,
    ATTENTION_FIELDS_COMPATIBILITY,
    ATTENTION_FIELDS_REASON
} attention_field_condition;

typedef struct {
    const yvex_cli_field_spec *fields;
    size_t count;
    attention_field_condition condition;
    int final;
} attention_field_group;

#define ATTENTION_GROUP(FIELDS, CONDITION) \
    {FIELDS, FIELD_COUNT(FIELDS), CONDITION, 0}

static const attention_field_group attention_field_groups[] = {
    ATTENTION_GROUP(attention_base_fields, ATTENTION_FIELDS_ALWAYS),
    ATTENTION_GROUP(attention_target_fields, ATTENTION_FIELDS_TARGET),
    ATTENTION_GROUP(attention_admission_fields, ATTENTION_FIELDS_ADMITTED),
    ATTENTION_GROUP(attention_execution_fields, ATTENTION_FIELDS_COMPLETED),
    ATTENTION_GROUP(attention_cuda_fields, ATTENTION_FIELDS_CUDA),
    ATTENTION_GROUP(attention_comparison_fields, ATTENTION_FIELDS_COMPARISON),
    ATTENTION_GROUP(attention_failure_fields, ATTENTION_FIELDS_COMPARISON_FAILURE),
    ATTENTION_GROUP(attention_provenance_fields, ATTENTION_FIELDS_DETAILED_ADMISSION),
    ATTENTION_GROUP(attention_compatibility_fields, ATTENTION_FIELDS_COMPATIBILITY),
    ATTENTION_GROUP(attention_reachability_fields, ATTENTION_FIELDS_ALWAYS),
    ATTENTION_GROUP(attention_reason_field, ATTENTION_FIELDS_REASON),
    {attention_final_field, FIELD_COUNT(attention_final_field), ATTENTION_FIELDS_ALWAYS, 1},
};

#undef ATTENTION_GROUP

/* Purpose: Select attention fields.
 * Inputs: condition, result, detail flag.
 * Effects: none.
 * Failure: returns false.
 * Boundary: presentation availability only. */
static int graph_attention_group_visible(attention_field_condition condition,
                                         const yvex_graph_attention_operator_result *result,
                                         int detailed)
{
    int admitted = result->attention_plan_identity[0] != '\0';

    switch (condition) {
    case ATTENTION_FIELDS_ALWAYS: return 1;
    case ATTENTION_FIELDS_TARGET: return strcmp(result->family, "unavailable") != 0;
    case ATTENTION_FIELDS_ADMITTED: return admitted;
    case ATTENTION_FIELDS_COMPLETED: return result->completed;
    case ATTENTION_FIELDS_CUDA: return result->cuda_device[0] != '\0';
    case ATTENTION_FIELDS_COMPARISON: return result->comparison_available;
    case ATTENTION_FIELDS_COMPARISON_FAILURE:
        return result->comparison_available && !result->comparison_passed;
    case ATTENTION_FIELDS_DETAILED_ADMISSION: return detailed && admitted;
    case ATTENTION_FIELDS_COMPATIBILITY:
        return detailed && result->current_writer_plan_identity[0] != '\0';
    case ATTENTION_FIELDS_REASON: return result->reason[0] != '\0';
    }
    return 0;
}

/* Purpose: Emit a field group. Inputs: stream/schema/result. Effects: writes. Failure: typed I/O.
 * Boundary: projection only; capability and availability stay runtime-owned. */
static int graph_attention_emit(FILE *fp,
                                int json,
                                const yvex_graph_attention_operator_result *result,
                                const yvex_cli_field_spec *fields,
                                size_t count,
                                int comma)
{
    int rc = json ? yvex_cli_json_fields(fp, result, fields, count, comma)
                  : yvex_cli_out_fields(fp, result, fields, count);
    return rc < 0 ? YVEX_ERR_IO : rc;
}

/* Purpose: Render attention fields. Inputs: stream/mode/result. Effects: writes. Failure: typed I/O.
 * Boundary: omits unavailable measurements without deriving capability. */
static int graph_attention_render_fields(FILE *fp,
                                         yvex_graph_report_mode mode,
                                         const yvex_graph_attention_operator_result *result)
{
    int json = mode == YVEX_GRAPH_REPORT_MODE_JSON;
    int detailed = json || mode == YVEX_GRAPH_REPORT_MODE_AUDIT;
    size_t index;
    int rc = YVEX_OK;

    if (json) yvex_cli_json_begin(fp);
    for (index = 0; rc == YVEX_OK && index < FIELD_COUNT(attention_field_groups); ++index) {
        const attention_field_group *group = &attention_field_groups[index];

        if (graph_attention_group_visible(group->condition, result, detailed))
            rc = graph_attention_emit(fp, json, result, group->fields, group->count, !group->final);
    }
    if (json) yvex_cli_json_end(fp);
    return rc < 0 || ferror(fp) ? YVEX_ERR_IO : rc;
}

/* Purpose: Render attention result.
 * Inputs: stream, mode, result.
 * Effects: writes fields.
 * Failure: typed I/O refusal.
 * Boundary: presentation only; no graph math. */
int yvex_graph_attention_render(FILE *fp,
                                yvex_graph_report_mode mode,
                                const yvex_graph_attention_operator_result *result)
{
    if (!fp || !result) return YVEX_ERR_INVALID_ARG;
    return graph_attention_render_fields(fp, mode, result);
}

/* Purpose: Render graph help.
 * Inputs: stream.
 * Effects: writes CLI text.
 * Failure: stream state.
 * Boundary: CLI presentation. */
int yvex_graph_render_help(FILE *fp)
{
    yvex_cli_out_lines(fp, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    return YVEX_OK;
}

static const char *const literal_lines_1[] = { "graph_requirements_status: blocked",
    "required_graph_ops: embedding-lookup,rmsnorm,q-projection,k-projection,v-projection,rope-position,"
        "attention-score,causal-mask,softmax,attention-value-accumulation,o-projection,residual-add,mlp-gate-"
        "up-down,activation,moe-router,expert-dispatch,expert-accumulation,final-norm,output-head-projection",
    "unsupported_graph_ops: full-transformer-attention,real-layer-scheduler,real-moe-router,real-expert-"
        "dispatch,real-output-head-projection",
    "required_backend_ops: tensor-read,rmsnorm,matmul,rope,attention,softmax,activation,residual-add,kv-read,kv-write",
    "unsupported_backend_ops: full-transformer-runtime-integration,real-attention-backed-kv,real-output-head-logits"};

static const char *const literal_lines_2[] = {
    "generation_ready: false", "generation: unsupported-full-model", "benchmark_status: not-measured"};

static const char *const literal_lines_3[] = {
    "prefill_descriptor: unsupported-full-transformer-prefill", "prefill.requires_embedding: true",
    "prefill.requires_attention_qkv: true", "prefill.requires_real_kv_writes: true",
    "prefill.requires_mlp_or_moe: true", "prefill.requires_layer_scheduler: true",
    "prefill.current_status: unsupported", "prefill.blocker: real transformer prefill not implemented",
    "decode_descriptor: unsupported-full-model-decode", "decode.mode_required: baseline-autoregressive",
    "decode.requires_prefill_state: true", "decode.requires_kv_read: true", "decode.requires_layer_execution: true",
    "decode.current_status: unsupported", "decode.blocker: full model decode not implemented",
    "logits_descriptor: unsupported-real-output-head-logits",
    "sampling_descriptor: unsupported-real-vocabulary-sampling", "residency_requirements_status: planned",
    "residency_plan: descriptor-only-no-allocation"};

static const char *const literal_lines_4[] = {
    "ssd_staged_required_bytes: planned", "kv_required_bytes: planned", "scratch_required_bytes: planned",
    "context_requirements_status: planned", "max_context: metadata-or-unknown", "requested_context: not-requested",
    "context_policy: planned", "position_policy: rope-or-family-specific-planned", "rope_policy: planned",
    "kv_requirements_status: unsupported", "kv_layout: planned", "kv_dtype: planned", "kv_layers: unknown",
    "kv_heads: unknown", "kv_head_dim: unknown", "kv_capacity_status: unsupported-full-transformer-kv",
    "kv.required: true", "kv.real_attention_writes: false", "kv.runtime_status: unsupported",
    "kv_write_ready: false", "kv_read_ready: false", "logits_requirements_status: unsupported"};

static const char *const literal_lines_5[] = {
    "logits_buffer_required: true", "real_output_head_logits: false", "logits_ready: false",
    "logits.blocker: real output-head logits runtime unsupported"};

static const char *const literal_lines_6[] = {
    "special_token_policy: planned", "eos_backed_stop: unsupported", "stop_token_text_matching: unsupported",
    "tokenizer_quality_generation: unsupported"};

static const char *const literal_lines_7[] = {
    "backend.primitive_rope: implemented-fixture", "backend.primitive_attention: implemented-fixture",
    "backend.primitive_matmul: implemented-fixture", "backend.primitive_mlp: implemented-fixture",
    "backend.full_transformer_integration: unsupported", "backend_allocation_attempted: false"};

static const char *const literal_lines_8[] = {
    "prefill_ready: false", "decode_ready: false", "sampling_ready: false", "cleanup_attempted: false",
    "cleanup_status: not-needed"};

static const char *const literal_pair_15[] = { "full_runtime_model: false", "full_model_execution: unsupported"};

static const char *const literal_pair_16[] = { "fullmodel: descriptor", "status: fullmodel-descriptor"};

static const char *const literal_pair_17[] = {
    "graph.residual_add: planned", "graph.mlp_primitive: implemented-fixture"};

static const char *const literal_pair_18[] = {
    "graph.rope_position_op: implemented-primitive", "graph.attention_primitive: implemented-fixture"};

/* Purpose: Map a role to its display collection.
 * Inputs: role text.
 * Effects: none.
 * Failure: returns unknown.
 * Boundary: presentation classification. */
static const char *fullmodel_descriptor_role_collection(const char *role)
{
    if (!role) return "unknown";
    if (strcmp(role, "token_embedding") == 0) return "embedding";
    if (strcmp(role, "attention_norm") == 0 ||
        strcmp(role, "post_attention_norm") == 0 ||
        strcmp(role, "final_norm") == 0) return "normalization";
    if (strcmp(role, "q_projection") == 0 ||
        strcmp(role, "k_projection") == 0 ||
        strcmp(role, "v_projection") == 0 ||
        strcmp(role, "o_projection") == 0) return "attention";
    if (strcmp(role, "mlp_gate") == 0 ||
        strcmp(role, "mlp_up") == 0 ||
        strcmp(role, "mlp_down") == 0) return "mlp";
    if (strcmp(role, "moe_router") == 0 ||
        strcmp(role, "moe_expert_gate") == 0 ||
        strcmp(role, "moe_expert_up") == 0 ||
        strcmp(role, "moe_expert_down") == 0) return "moe";
    if (strcmp(role, "output_head") == 0) return "output";
    if (strcmp(role, "tokenizer_metadata") == 0) return "tokenizer-runtime-input";
    return "unknown";
}

/* Purpose: Compute fullmodel descriptor role residency for its CLI invariant
 *   (`fullmodel_descriptor_role_residency`). */
static const char *fullmodel_descriptor_role_residency(const char *role,
                                                       const char *backend,
                                                       int present)
{
    if (!present) return "not-planned";
    if (role && strcmp(role, "tokenizer_metadata") == 0) return "host-runtime-metadata";
    return backend && strcmp(backend, "cuda") == 0 ? "cuda-resident-planned" : "cpu-resident-planned";
}

/* Purpose: Render a descriptor role.
 * Inputs: model, collection, role, backend.
 * Effects: writes CLI fields.
 * Failure: stream state.
 * Boundary: descriptor presentation only. */
static void fullmodel_print_descriptor_role(yvex_model_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *backend)
{
    const yvex_tensor_info *tensor = NULL;
    char dims[128];
    int present = 0;

    if (role && strcmp(role, "tokenizer_metadata") == 0) {
        present = collections && collections->has_tokenizer_metadata;
    } else {
        tensor = fullmodel_descriptor_find_tensor(ctx, role);
        present = tensor != NULL;
    }
    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    yvex_cli_out_writef(stdout, "role.%s.status: %s\n", role ? role : "unknown", present ? "present" : "missing");
    yvex_cli_out_writef(stdout, "role.%s.tensor: %s\n", role ? role : "unknown",
           tensor && tensor->name ? tensor->name : present ? "metadata" : "none");
    yvex_cli_out_writef(stdout, "role.%s.shape: %s\n", role ? role : "unknown",
           tensor ? dims : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.dtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.qtype: %s\n", role ? role : "unknown",
           tensor ? yvex_dtype_name(tensor->dtype) : present ? "metadata" : "unknown");
    yvex_cli_out_writef(stdout, "role.%s.bytes: %llu\n", role ? role : "unknown",
           tensor ? tensor->storage_bytes : 0ull);
    yvex_cli_out_writef(stdout, "role.%s.collection: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_collection(role));
    yvex_cli_out_writef(stdout, "role.%s.residency_expectation: %s\n", role ? role : "unknown",
           fullmodel_descriptor_role_residency(role, backend, present));
    yvex_cli_out_writef(stdout, "role.%s.runtime_consumer: %s\n", role ? role : "unknown",
           present ? "planned" : "blocked-missing-role");
}

/* Purpose: Render one descriptor collection and its exact runtime requirements.
 * Inputs: Borrowed collection identity, accounting, requirements, and blocker facts.
 * Effects: Writes ordered descriptor fields through CLI I/O.
 * Failure: CLI write failures remain owned by the output boundary.
 * Boundary: Rendering does not make the collection resident or executable. */
static void fullmodel_print_descriptor_collection(const char *name,
                                                  unsigned long long count,
                                                  unsigned long long bytes,
                                                  int required_for_prefill,
                                                  int required_for_decode,
                                                  int required_for_logits,
                                                  int required_for_generation,
                                                  const char *runtime_consumer,
                                                  const char *blocker)
{
    yvex_cli_out_writef(stdout, "collection.%s.status: %s\n", name, count > 0ull ? "present" : "missing");
    yvex_cli_out_writef(stdout, "collection.%s.tensor_count: %llu\n", name, count);
    yvex_cli_out_writef(stdout, "collection.%s.byte_count: %llu\n", name, bytes);
    yvex_cli_out_writef(stdout, "collection.%s.required_for_prefill: %s\n", name,
        required_for_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_decode: %s\n", name,
        required_for_decode ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_logits: %s\n", name,
        required_for_logits ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.required_for_generation: %s\n", name,
        required_for_generation ? "true" : "false");
    yvex_cli_out_writef(stdout, "collection.%s.runtime_consumer: %s\n", name,
        runtime_consumer ? runtime_consumer : "planned");
    yvex_cli_out_writef(stdout, "collection.%s.blocker: %s\n", name, blocker && blocker[0] ? blocker : "none");
}

typedef enum {
    DESCRIPTOR_PHASE_PASS,
    DESCRIPTOR_PHASE_ROLE,
    DESCRIPTOR_PHASE_COLLECTION,
    DESCRIPTOR_PHASE_PLANNED,
    DESCRIPTOR_PHASE_BLOCKED,
    DESCRIPTOR_PHASE_FAILURE_MARKER
} descriptor_phase_kind;

typedef struct {
    const char *name;
    descriptor_phase_kind kind;
} descriptor_phase_spec;

static const descriptor_phase_spec descriptor_phases[] = {
    {"preflight", DESCRIPTOR_PHASE_PASS}, {"resolve-model", DESCRIPTOR_PHASE_PASS},
    {"artifact-identity", DESCRIPTOR_PHASE_PASS}, {"tensor-inventory", DESCRIPTOR_PHASE_PASS},
    {"role-map", DESCRIPTOR_PHASE_ROLE}, {"collection-map", DESCRIPTOR_PHASE_COLLECTION},
    {"shape-requirements", DESCRIPTOR_PHASE_PASS},
    {"residency-requirements", DESCRIPTOR_PHASE_PLANNED},
    {"graph-requirements", DESCRIPTOR_PHASE_PLANNED},
    {"prefill-requirements", DESCRIPTOR_PHASE_BLOCKED},
    {"kv-requirements", DESCRIPTOR_PHASE_BLOCKED},
    {"decode-requirements", DESCRIPTOR_PHASE_BLOCKED},
    {"logits-requirements", DESCRIPTOR_PHASE_BLOCKED},
    {"sampling-requirements", DESCRIPTOR_PHASE_BLOCKED},
    {"tokenizer-requirements", DESCRIPTOR_PHASE_PASS},
    {"backend-requirements", DESCRIPTOR_PHASE_PLANNED},
    {"blocker-report", DESCRIPTOR_PHASE_PASS}, {"descriptor-build", DESCRIPTOR_PHASE_PASS},
    {"complete", DESCRIPTOR_PHASE_PASS}, {"failed", DESCRIPTOR_PHASE_FAILURE_MARKER},
    {"cleanup", DESCRIPTOR_PHASE_PASS},
};

/* Purpose: select one phase status from immutable phase kind and caller-owned outcomes. */
static const char *descriptor_phase_status(descriptor_phase_kind kind,
                                           const char *role_status,
                                           const char *collection_status,
                                           int failed_seen,
                                           int failure_here,
                                           int has_failure)
{
    if (failure_here) return "fail";
    if (failed_seen) return "skipped";
    if (kind == DESCRIPTOR_PHASE_ROLE) return role_status ? role_status : "partial";
    if (kind == DESCRIPTOR_PHASE_COLLECTION)
        return collection_status ? collection_status : "partial";
    if (kind == DESCRIPTOR_PHASE_PLANNED) return "planned";
    if (kind == DESCRIPTOR_PHASE_BLOCKED) return "blocked";
    if (kind == DESCRIPTOR_PHASE_FAILURE_MARKER && !has_failure) return "skipped";
    return "pass";
}

/* Purpose: render the declared descriptor lifecycle with exact failure cutover.
 * Inputs: role and collection status plus optional failing phase name.
 * Effects: writes ordered phase facts through CLI I/O.
 * Failure: unknown failure names leave the ordinary phase sequence intact.
 * Boundary: rendering never changes descriptor admission. */
void fullmodel_print_descriptor_phases(const char *role_status,
                                       const char *collection_status,
                                       const char *failure_phase)
{
    size_t index;
    int failed_seen = 0;

    for (index = 0; index < sizeof(descriptor_phases) / sizeof(descriptor_phases[0]); ++index) {
        const descriptor_phase_spec *phase = &descriptor_phases[index];
        int failure_here = failure_phase && strcmp(failure_phase, phase->name) == 0;
        const char *status = descriptor_phase_status(phase->kind, role_status, collection_status,
                                                     failed_seen, failure_here, failure_phase != NULL);
        model_phase_print("descriptor_phase", (unsigned int)index, phase->name, status, "planned");
        failed_seen |= failure_here;
    }
}

/* Purpose: Render graph requirements.
 * Inputs: collections.
 * Effects: writes CLI fields.
 * Failure: stream state.
 * Boundary: descriptor presentation only. */
static void fullmodel_print_descriptor_graph_requirements(const yvex_fullmodel_collections *collections)
{
    int has_attention = fullmodel_has_attention_collection(collections);
    int has_mlp = fullmodel_has_mlp_collection(collections);

    yvex_cli_out_lines(stdout, literal_lines_1, sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
    yvex_cli_out_writef(stdout, "graph.embedding_lookup: %s\n",
           collections && collections->has_token_embedding ? "planned-real-tensor" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.rmsnorm: %s\n",
           fullmodel_has_normalization_collection(collections) ? "implemented-selected-segment" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.q_projection: %s\n",
        collections && collections->has_attention_q ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.k_projection: %s\n",
        collections && collections->has_attention_k ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.v_projection: %s\n",
        collections && collections->has_attention_v ? "planned" : "missing-tensor");
    yvex_cli_out_lines(stdout, literal_pair_18, sizeof(literal_pair_18) / sizeof(literal_pair_18[0]));
    yvex_cli_out_writef(stdout, "graph.full_transformer_attention: %s\n",
        has_attention ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.o_projection: %s\n",
        collections && collections->has_attention_out ? "planned" : "missing-tensor");
    yvex_cli_out_lines(stdout, literal_pair_17, sizeof(literal_pair_17) / sizeof(literal_pair_17[0]));
    yvex_cli_out_writef(stdout, "graph.full_transformer_mlp: %s\n", has_mlp ? "unsupported" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.moe_router: %s\n",
           collections && collections->has_moe_router ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.expert_dispatch: %s\n",
           collections && collections->has_moe_expert ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph.output_head_projection: %s\n",
           collections && collections->has_output_head ? "planned" : "missing-tensor");
}

typedef enum descriptor_blocker_rule {
    DESCRIPTOR_BLOCKER_COUNT,
    DESCRIPTOR_BLOCKER_ATTENTION,
    DESCRIPTOR_BLOCKER_MLP,
    DESCRIPTOR_BLOCKER_TOKENIZER,
    DESCRIPTOR_BLOCKER_UNKNOWN,
    DESCRIPTOR_BLOCKER_FIXED
} descriptor_blocker_rule;

typedef struct descriptor_collection_spec {
    const char *name;
    size_t count_offset;
    size_t bytes_offset;
    unsigned int required_mask;
    descriptor_blocker_rule blocker_rule;
    const char *missing_blocker;
    const char *runtime_consumer;
} descriptor_collection_spec;

#define COLLECTION_OFF(member_) offsetof(yvex_fullmodel_collections, member_)
#define REQUIRED_MASK(prefill_, decode_, logits_, generation_) \
    ((unsigned int)(prefill_) | ((unsigned int)(decode_) << 1u) | \
     ((unsigned int)(logits_) << 2u) | ((unsigned int)(generation_) << 3u))

static const char *const descriptor_roles[] = {
    "token_embedding", "attention_norm", "post_attention_norm", "final_norm",
    "q_projection", "k_projection", "v_projection", "o_projection",
    "mlp_gate", "mlp_up", "mlp_down", "moe_router", "moe_expert_gate",
    "moe_expert_up", "moe_expert_down", "output_head", "tokenizer_metadata", "unknown"};

static const descriptor_collection_spec descriptor_collections[] = {
    {"embedding", COLLECTION_OFF(embedding), COLLECTION_OFF(embedding_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_COUNT,
     "embedding collection missing", "planned"},
    {"normalization", COLLECTION_OFF(normalization), COLLECTION_OFF(normalization_bytes),
     REQUIRED_MASK(1, 1, 1, 1), DESCRIPTOR_BLOCKER_COUNT,
     "normalization collection missing", "planned"},
    {"attention", COLLECTION_OFF(attention), COLLECTION_OFF(attention_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_ATTENTION,
     "attention Q/K/V/O tensors missing", "planned"},
    {"mlp", COLLECTION_OFF(mlp), COLLECTION_OFF(mlp_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_MLP, "MLP tensors missing", "planned"},
    {"moe", COLLECTION_OFF(moe), COLLECTION_OFF(moe_bytes),
     REQUIRED_MASK(1, 1, 0, 1), DESCRIPTOR_BLOCKER_COUNT,
     "MoE tensors missing or not identified", "planned"},
    {"output", COLLECTION_OFF(output), COLLECTION_OFF(output_bytes),
     REQUIRED_MASK(0, 1, 1, 1), DESCRIPTOR_BLOCKER_COUNT, "output head missing", "planned"},
    {"tokenizer-runtime-input", COLLECTION_OFF(tokenizer), COLLECTION_OFF(tokenizer_bytes),
     REQUIRED_MASK(1, 1, 1, 1), DESCRIPTOR_BLOCKER_TOKENIZER,
     "tokenizer metadata missing", "planned"},
    {"kv-cache-runtime", (size_t)-1, (size_t)-1, REQUIRED_MASK(1, 1, 0, 1),
     DESCRIPTOR_BLOCKER_FIXED, "real attention-backed KV writes unsupported", "unsupported"},
    {"unknown", COLLECTION_OFF(unknown), COLLECTION_OFF(unknown_bytes),
     REQUIRED_MASK(0, 0, 0, 0), DESCRIPTOR_BLOCKER_UNKNOWN, "unknown tensor role", "unsupported"},
};

#undef REQUIRED_MASK
#undef COLLECTION_OFF

/* Read one count field selected by an immutable renderer table. */
/* Purpose: Compute collection value for its CLI invariant (`collection_value`). */
static unsigned long long collection_value(const yvex_fullmodel_collections *collections,
                                           size_t offset)
{
    if (!collections || offset == (size_t)-1) return 0ull;
    return *(const unsigned long long *)((const unsigned char *)collections + offset);
}

/* Resolve whether a descriptor collection satisfies its exact role contract. */
/* Purpose: Resolve collection readiness.
 * Inputs: schema, collections, count.
 * Effects: none.
 * Failure: returns false.
 * Boundary: descriptor presentation only. */
static int descriptor_collection_ready(const descriptor_collection_spec *spec,
                                       const yvex_fullmodel_collections *collections,
                                       unsigned long long count)
{
    switch (spec->blocker_rule) {
    case DESCRIPTOR_BLOCKER_ATTENTION: return fullmodel_has_attention_collection(collections);
    case DESCRIPTOR_BLOCKER_MLP: return fullmodel_has_mlp_collection(collections);
    case DESCRIPTOR_BLOCKER_TOKENIZER:
        return collections && collections->has_tokenizer_metadata;
    case DESCRIPTOR_BLOCKER_UNKNOWN: return count == 0ull;
    case DESCRIPTOR_BLOCKER_FIXED: return 0;
    case DESCRIPTOR_BLOCKER_COUNT: return count > 0ull;
    }
    return 0;
}

/* Purpose: Render inventory.
 * Inputs: model, collections, backend.
 * Effects: writes CLI fields.
 * Failure: stream state.
 * Boundary: descriptor presentation only. */
static void fullmodel_print_descriptor_inventory(
    yvex_model_context *ctx,
    const yvex_fullmodel_collections *collections,
    const char *backend)
{
    size_t i;

    for (i = 0; i < sizeof(descriptor_roles) / sizeof(descriptor_roles[0]); ++i) {
        fullmodel_print_descriptor_role(ctx, collections, descriptor_roles[i], backend);
    }

yvex_cli_out_writef(stdout, "embedding_descriptor: %s\n",
    collections && collections->embedding > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "normalization_descriptor: %s\n",
    collections && collections->normalization > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "attention_descriptor: %s\n",
    fullmodel_has_attention_collection(collections) ? "present" : "missing");
yvex_cli_out_writef(stdout, "mlp_descriptor: %s\n", fullmodel_has_mlp_collection(collections) ? "present" : "missing");
yvex_cli_out_writef(stdout, "moe_descriptor: %s\n",
    collections && collections->moe > 0ull ? "present" : "planned-or-missing");
yvex_cli_out_writef(stdout, "output_descriptor: %s\n",
    collections && collections->output > 0ull ? "present" : "missing");
yvex_cli_out_writef(stdout, "tokenizer_descriptor: %s\n",
    collections && collections->has_tokenizer_metadata ? "present" : "missing");
yvex_cli_out_writef(stdout, "kv_descriptor: unsupported-real-attention-backed-kv\n");

    for (i = 0; i < sizeof(descriptor_collections) / sizeof(descriptor_collections[0]); ++i) {
        const descriptor_collection_spec *spec = &descriptor_collections[i];
        unsigned long long count = collection_value(collections, spec->count_offset);
        unsigned long long bytes = collection_value(collections, spec->bytes_offset);
        int ready = descriptor_collection_ready(spec, collections, count);

        fullmodel_print_descriptor_collection(
            spec->name, count, bytes, (spec->required_mask & 1u) != 0u,
            (spec->required_mask & 2u) != 0u, (spec->required_mask & 4u) != 0u,
            (spec->required_mask & 8u) != 0u, spec->runtime_consumer,
            ready ? "none" : spec->missing_blocker);
    }
}

/* Purpose: Render fullmodel descriptor.
 * Inputs: admitted report facts.
 * Effects: writes CLI report.
 * Failure: stream state.
 * Boundary: presentation does not promote runtime capability. */
void fullmodel_print_descriptor_report(const yvex_cli_fullmodel_options *options,
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
    const char *backend = options && options->backend ? options->backend : "cpu";
    int descriptor_complete = role_coverage &&
                              (strcmp(role_coverage, "complete") == 0 ||
                               strcmp(role_coverage, "observed") == 0);
    const char *descriptor_status = selected_target ? "partial" :
                                    (descriptor_complete ? "complete" : "partial");
    const char *materialization_plan_status = selected_target ? "partial" : "ready";
    const char *materialization_proof_status = selected_target ? "refused-selected-runtime-slice" :
                                               (descriptor_complete
                                                    ? "available-controlled-tiny-proof"
                                                    : "blocked-missing-roles");
    const char *full_materialization = selected_target ? "refused-selected-runtime-slice" :
                                      (descriptor_complete
                                           ? "controlled-tiny-proof-available"
                                           : "planned");
    unsigned long long cuda_bytes = strcmp(backend, "cuda") == 0 ? total_tensor_bytes : 0ull;
    unsigned long long cpu_bytes = strcmp(backend, "cuda") == 0 ? 0ull : total_tensor_bytes;

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    yvex_cli_out_lines(stdout, literal_pair_16, sizeof(literal_pair_16) / sizeof(literal_pair_16[0]));
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "format: %s\n", options && options->format ? options->format : "text");
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "materialization_plan_status: %s\n", materialization_plan_status);
    yvex_cli_out_writef(stdout, "materialization_proof_status: %s\n", materialization_proof_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_descriptor_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "runtime_descriptor_kind: fullmodel-planning\n");
    yvex_cli_out_writef(stdout, "family: %s\n", fullmodel_family_from_arch(arch));
    yvex_cli_out_writef(stdout, "architecture: %s\n", yvex_arch_name(arch));
    yvex_cli_out_writef(stdout, "model_class: %s\n",
        selected_target ? "selected-runtime-slice" : "descriptor-only-candidate");
    yvex_cli_out_lines(stdout, literal_pair_15, sizeof(literal_pair_15) / sizeof(literal_pair_15[0]));
    yvex_cli_out_writef(stdout, "full_model_materialization: %s\n", full_materialization);
    yvex_cli_out_lines(stdout, literal_lines_2, sizeof(literal_lines_2) / sizeof(literal_lines_2[0]));
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "tensor_role_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "tensor_collection_map_status: %s\n", descriptor_status);
    yvex_cli_out_writef(stdout, "required_role_coverage: %s\n",
        descriptor_complete ? "complete" : (role_coverage ? role_coverage : "partial"));
    yvex_cli_out_writef(stdout, "missing_required_roles: %s\n", missing_roles ? missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unsupported_required_roles: %s\n", unsupported_roles ? unsupported_roles : "unknown");
    yvex_cli_out_writef(stdout, "unknown_role_count: %llu\n", collections ? collections->unknown : 0ull);

    fullmodel_print_descriptor_inventory(ctx, collections, backend);

    fullmodel_print_descriptor_graph_requirements(collections);

    yvex_cli_out_lines(stdout, literal_lines_3, sizeof(literal_lines_3) / sizeof(literal_lines_3[0]));
    yvex_cli_out_writef(stdout, "cpu_resident_required_bytes: %llu\n", cpu_bytes);
    yvex_cli_out_writef(stdout, "cuda_resident_required_bytes: %llu\n", cuda_bytes);
    yvex_cli_out_writef(stdout, "host_staged_required_bytes: %llu\n", strcmp(backend,
        "cuda") == 0 ? total_tensor_bytes : 0ull);
    yvex_cli_out_lines(stdout, literal_lines_4, sizeof(literal_lines_4) / sizeof(literal_lines_4[0]));
    yvex_cli_out_writef(stdout, "output_head_present: %s\n",
        collections && collections->has_output_head ? "true" : "false");
    yvex_cli_out_writef(stdout, "output_head_tensor: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? fullmodel_descriptor_find_tensor(ctx, "output_head")->name
               : "none");
    yvex_cli_out_writef(stdout, "output_head_dtype: %s\n",
           fullmodel_descriptor_find_tensor(ctx, "output_head")
               ? yvex_dtype_name(fullmodel_descriptor_find_tensor(ctx, "output_head")->dtype)
               : "unknown");
    yvex_cli_out_writef(stdout, "vocab_size: %s\n",
        collections && collections->has_output_head ? "from-output-head-shape" : "unknown");
    yvex_cli_out_lines(stdout, literal_lines_5, sizeof(literal_lines_5) / sizeof(literal_lines_5[0]));

    yvex_cli_out_writef(stdout, "tokenizer_requirements_status: %s\n",
           collections && collections->has_tokenizer_metadata ? "partial" : "blocked");
    yvex_cli_out_writef(stdout, "tokenizer_metadata_present: %s\n",
           collections && collections->has_tokenizer_metadata ? "true" : "false");
    yvex_cli_out_lines(stdout, literal_lines_6, sizeof(literal_lines_6) / sizeof(literal_lines_6[0]));

    yvex_cli_out_writef(stdout, "backend_requirements_status: %s\n", fit.available ? "planned" : "unsupported");
    yvex_cli_out_writef(stdout, "backend.cpu.available: true\n");
    yvex_cli_out_writef(stdout, "backend.cuda.context_available: %s\n",
        yvex_backend_cuda_context_available() ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.memory_known: %s\n", fit.memory_known ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend.required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend.fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend.fit_reason: %s\n", fit.fit_reason);
    yvex_cli_out_lines(stdout, literal_lines_7, sizeof(literal_lines_7) / sizeof(literal_lines_7[0]));

    yvex_cli_out_writef(stdout, "runtime_blockers: %s\n",
           selected_target
               ? "full runtime tensor set incomplete; attention Q/K/V/O tensors missing; MLP/MoE tensors "
                   "missing; output head missing; real transformer prefill unsupported; real attention-"
                   "backed KV writes unsupported; full model decode unsupported; real output-head logits "
                   "unsupported; real vocabulary sampling unsupported; full model execution unsupported"
               : "real transformer prefill unsupported; real attention-backed KV writes unsupported; full "
                   "model decode unsupported; real output-head logits runtime unsupported; real vocabulary "
                   "sampling unsupported; full model execution unsupported");
    yvex_cli_out_writef(stdout, "descriptor_blockers: %s\n",
           selected_target
               ? "selected-runtime-slice is partial descriptor only"
               : "runtime family adapter boundary remains planned");
    yvex_cli_out_lines(stdout, literal_lines_8, sizeof(literal_lines_8) / sizeof(literal_lines_8[0]));
    fullmodel_print_descriptor_phases(descriptor_status, descriptor_status, NULL);
}
