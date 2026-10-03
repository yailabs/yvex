/*
 * Project source dtype and canonical numeric capability policy facts.
 *
 * Qtype policy facts are planning/report-only and do not perform quantization, write GGUF
 * artifacts, or mark runtime paths ready. Qtype policy reporting is not quantization, artifact
 * emission, runtime support, generation readiness, benchmark evidence, or release readiness.
 */
#include <yvex/internal/model_target.h>

#include <yvex/internal/quant_numeric.h>

#include <stdio.h>
#include <string.h>

typedef struct {
    yvex_model_target_source_profile source;
    const char *mapping_gate_status;
    const char *top_blocker;
    const char *next_row;
    const char *status;
    const char *bracket;
} qtype_policy_state;

static const unsigned int policy_qtypes[] = {
    YVEX_GGUF_QTYPE_F16, YVEX_GGUF_QTYPE_BF16,
    YVEX_GGUF_QTYPE_F32, YVEX_GGUF_QTYPE_Q8_0,
    YVEX_GGUF_QTYPE_Q2_K, YVEX_GGUF_QTYPE_Q4_K,
    YVEX_GGUF_QTYPE_IQ2_XXS
};

#define POLICY_TEXT(key, value) {YVEX_MODEL_TARGET_ROW_LITERAL, value, 0u, key}
static const yvex_model_target_row_spec policy_prefix_rows[] = {
    POLICY_TEXT("tensor_map_status", "naming-map-profiled"),
    POLICY_TEXT("output_head_map_status", "output-head-profiled")
};

static const yvex_model_target_row_spec policy_suffix_rows[] = {
    POLICY_TEXT("missing_role_report_status", "missing-role-report-blocked"),
    POLICY_TEXT("qtype_policy_basis", "header-only-source-metadata+canonical-numeric-registry"),
    POLICY_TEXT("qtype_policy_status", "reported")
};

static const yvex_model_target_row_spec policy_downstream_rows[] = {
    POLICY_TEXT("refusal_reasons", "Q4_K:encoder-unavailable IQ2_XXS:calibration-required"),
    POLICY_TEXT("artifact_identity_status", "missing"),
    POLICY_TEXT("runtime_descriptor_status", "missing"),
    POLICY_TEXT("graph_consumer_status", "missing"),
    POLICY_TEXT("backend_residency_status", "missing"),
    POLICY_TEXT("downstream_blockers", "family_quantization_plan=missing artifact_emit=missing "
    "artifact_identity=missing runtime_descriptor=missing graph_consumer=missing "
    "backend_residency=missing generation_runtime=missing eval_benchmark=missing")
};
#undef POLICY_TEXT

static const yvex_model_target_request_rules qtype_policy_rules = {
    YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY,
    "qtype-policy-fail",
    "qtype policy report requires quant-policy command kind",
    "quant-policy",
    1
};

static void qtype_policy_block(qtype_policy_state *state,
                               const char *mapping_status,
                               const char *blocker)
{
    state->mapping_gate_status = mapping_status;
    state->top_blocker = blocker;
    state->next_row = "V010.MAP.9";
    state->status = "blocked";
    state->bracket = "blocked";
}

static void qtype_policy_build_state(const yvex_model_target_request *request,
                                     const char *family,
                                     qtype_policy_state *state)
{
    memset(state, 0, sizeof(*state));
    state->mapping_gate_status = "passed-for-artifact-planning";
    state->top_blocker = "family-quantization-plan-unimplemented";
    state->next_row = "not-scheduled";
    state->status = "policy-reported";
    state->bracket = "reported";

    yvex_model_target_probe_source_profile(request, family, &state->source);
    if (!state->source.source_requested) {
        return;
    }
    if (!state->source.source_directory_present) {
        qtype_policy_block(
            state, "blocked-missing-source",
            strcmp(family, "gemma") == 0
                ? "missing-gemma-source-path" : "missing-qwen-source-path");
        return;
    }

    if (!state->source.header_present) {
        qtype_policy_block(state, "blocked-missing-dtype",
                           "missing-source-dtype-profile");
    } else {
        if (!state->source.attention_k_present) {
            qtype_policy_block(state, "blocked-missing-runtime-roles",
                               "missing-source-role-attention-k");
        } else if (!state->source.output_head_present) {
            qtype_policy_block(state, "blocked-missing-runtime-roles",
                               "missing-output-head-tensor");
        } else if (state->source.output_head_ambiguous) {
            qtype_policy_block(state, "blocked-missing-runtime-roles",
                               "ambiguous-output-head-tensor");
        }
    }

    if (state->source.header_present && !state->source.metadata_present &&
        strcmp(state->status, "policy-reported") == 0) {
        qtype_policy_block(state, "blocked-missing-runtime-roles",
                           "missing-tokenizer-sidecars");
    }
}

static void qtype_policy_prepare(const yvex_model_target_request *request,
                                 const qtype_policy_state *state,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "";
    const char *family = yvex_model_target_family_key(target);
    const yvex_model_target_report_profile profile = {
        .status = state->status, .target_id = target, .family = family,
        .stage = "report-only", .qtype_policy_status = state->status,
        .artifact_status = "missing", .runtime_status = "unsupported",
        .generation_status = "unsupported-full-model", .benchmark_status = "not-measured",
        .next_row = state->next_row, .reason = state->top_blocker,
        .boundary = "report-only; no quantization/artifact/runtime"
    };

    yvex_model_target_report_prepare(report, request, &profile);
}

static int qtype_policy_validate(const yvex_model_target_request *request,
                                 yvex_model_target_report *report)
{
    const yvex_model_target_record *record;
    const char *target = request->target_id;
    const char *family;

    if (!yvex_model_target_validate_request_shape(
            request, report, &qtype_policy_rules, request->release)) {
        return 1;
    }

    record = yvex_model_target_find(target);
    family = yvex_model_target_family_key(target);
    if (!record) {
        report->status = "unsupported-target";
        report->exit_code = 2;
        if (request->output_contract[0]) {
            yvex_model_target_report_fact_text(report, "status", "unsupported-target");
            return 1;
        }
        yvex_model_target_report_fact_text(report, "qtype-policy", target);
        yvex_model_target_report_fact_text(report, "top_blocker", "unsupported-target");
        yvex_model_target_report_add_error(report, "unsupported target: %s", target);
        return 1;
    }
    if (strcmp(family, "qwen") != 0 && strcmp(family, "gemma") != 0) {
        report->status = strcmp(family, "deepseek") == 0 ? "blocked" : "unsupported";
        yvex_model_target_report_fact_text(report, "qtype-policy", target);
        yvex_model_target_report_fact_text(report, "family", family);
        yvex_model_target_report_fact_text(report, "top_blocker", strcmp(family, "deepseek") == 0
                ? "unsupported-target-class"
                : "unsupported-family");
        if (strcmp(family, "deepseek") != 0) {
            report->status = "unsupported-family";
        }
        return 1;
    }
    return 0;
}

static void qtype_policy_add_contract(const yvex_model_target_request *request,
                                      yvex_model_target_report *report)
{
    if (!request->output_contract[0]) {
        return;
    }
    if (strcmp(request->output_contract, "missing") == 0) {
        report->status = "parser-error";
        report->exit_code = 2;
        yvex_model_target_report_fact_text(report, "status", "parser-error");
        return;
    }
    if (strcmp(request->output_contract, "normal") != 0 &&
        strcmp(request->output_contract, "table") != 0 &&
        strcmp(request->output_contract, "audit") != 0) {
        report->status = "unsupported-mode";
        report->exit_code = 2;
        yvex_model_target_report_fact_text(report, "status", "unsupported-mode");
        return;
    }
    yvex_model_target_report_add_output_contract(
        report, "qtype-policy", request->output_contract);
}

static void qtype_policy_numeric_lists(char candidates[96],
                                       char refused[96])
{
    unsigned int index;

    candidates[0] = '\0';
    refused[0] = '\0';
    for (index = 0u; index < sizeof(policy_qtypes) /
                              sizeof(policy_qtypes[0]); ++index) {
        const yvex_quant_numeric_capability *capability =
            yvex_quant_numeric_capability_at(policy_qtypes[index]);
        const char *name = yvex_gguf_qtype_name(policy_qtypes[index]);
        char *target = capability && capability->encoder_available &&
                capability->reference_decoder_available &&
                capability->dedicated_cpu_compute_available &&
                capability->dedicated_cuda_compute_available
            ? candidates : refused;
        size_t capacity = 96u;
        size_t used = strlen(target);

        if (used < capacity)
            (void)snprintf(target + used, capacity - used, "%s%s",
                           used ? "," : "", name);
    }
}

static void qtype_policy_add_table(const qtype_policy_state *state,
                                   yvex_model_target_report *report)
{
    char candidates[96];
    char refused[96];

    qtype_policy_numeric_lists(candidates, refused);
    yvex_model_target_report_add_row(report, "QTYPE POLICY");
    yvex_model_target_report_add_row(
        report,
        "TARGET  FAMILY  SOURCE_DTYPE  POLICY  PREFERRED  CANDIDATES  REFUSED  STATUS  NEXT");
    yvex_model_target_report_add_row(
        report,
        "%s  %s  F32=%lu F16=%lu BF16=%lu other=%lu  artifact-planning-storage-policy  F16  %s  %s  %s  %s",
        report->target_id, report->family, state->source.f32_count,
        state->source.f16_count, state->source.bf16_count,
        state->source.other_count, candidates, refused,
        state->status, state->next_row);
}

static void qtype_policy_capability_facts(yvex_model_target_report *report,
    const char *qtype, const yvex_quant_numeric_capability *capability)
{
    char key[128];
#define CAPABILITY_FACT(field, value) \
    snprintf(key, sizeof(key), "numeric_capability.%s." field, qtype); \
    yvex_model_target_report_fact_text(report, key, value)
    CAPABILITY_FACT("encoder", capability && capability->encoder_available ? "available" : "unavailable");
    CAPABILITY_FACT("decoder", capability && capability->reference_decoder_available ? "available" : "unavailable");
    CAPABILITY_FACT("cpu", capability && capability->dedicated_cpu_compute_available ? "available" : "unavailable");
    CAPABILITY_FACT("cuda", capability && capability->dedicated_cuda_compute_available ? "available" : "unavailable");
    CAPABILITY_FACT("calibration", capability ? yvex_quant_calibration_name(capability->calibration) : "unknown");
#undef CAPABILITY_FACT
}

static void qtype_policy_dtype_facts(yvex_model_target_report *report,
    const yvex_model_target_source_profile *source)
{
    yvex_model_target_report_fact_u64(report, "source_dtype.F32", source->f32_count);
    yvex_model_target_report_fact_u64(report, "source_dtype.F16", source->f16_count);
    yvex_model_target_report_fact_u64(report, "source_dtype.BF16", source->bf16_count);
    yvex_model_target_report_fact_u64(report, "source_dtype.other", source->other_count);
}

static void qtype_policy_add_audit(const qtype_policy_state *state,
                                   yvex_model_target_report *report)
{
    const yvex_quant_numeric_capability *q8 =
        yvex_quant_numeric_capability_at(YVEX_GGUF_QTYPE_Q8_0);
    const yvex_quant_numeric_capability *q2 =
        yvex_quant_numeric_capability_at(YVEX_GGUF_QTYPE_Q2_K);
    yvex_model_target_report_fact_text(report, "source_dtype_profile_status",
                                       state->source.header_present ? "profiled" : "missing");
    qtype_policy_dtype_facts(report, &state->source);
    yvex_model_target_report_fact_u64(report, "source_tensor_count", (unsigned long long)(state->source.tensor_count));
    yvex_model_target_report_fact_text(report, "mapping_gate_status", state->mapping_gate_status);
    yvex_model_target_report_project_rows(
        report, policy_prefix_rows,
        sizeof(policy_prefix_rows) / sizeof(policy_prefix_rows[0]), report);
    yvex_model_target_report_fact_text(report, "tokenizer_metadata_map_status",
                                       state->source.metadata_present ? "present-report-only" : "missing");
    yvex_model_target_report_project_rows(
        report, policy_suffix_rows,
        sizeof(policy_suffix_rows) / sizeof(policy_suffix_rows[0]), report);
    qtype_policy_capability_facts(report, "Q8_0", q8);
    qtype_policy_capability_facts(report, "Q2_K", q2);
    yvex_model_target_report_project_rows(
        report, policy_downstream_rows,
        sizeof(policy_downstream_rows) / sizeof(policy_downstream_rows[0]), report);
    yvex_model_target_report_fact_text(report, "next_required_rows", state->next_row);
    yvex_model_target_report_common_tail(report);
}

int yvex_qtype_policy_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    qtype_policy_state state;
    char candidates[96];
    char refused[96];
    const char *family;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "qtype_policy_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    family = yvex_model_target_family_key(request->target_id);
    qtype_policy_build_state(request, family, &state);
    qtype_policy_prepare(request, &state, report);
    if (qtype_policy_validate(request, report)) {
        return YVEX_OK;
    }
    if (request->output_contract[0]) {
        qtype_policy_add_contract(request, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        qtype_policy_add_table(&state, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        qtype_policy_add_audit(&state, report);
        return YVEX_OK;
    }

    yvex_model_target_report_fact_text(report, "qtype-policy", report->target_id);
    yvex_model_target_report_fact_text(report, "mapping_gate", state.mapping_gate_status);
    qtype_policy_dtype_facts(report, &state.source);
    if (strcmp(state.status, "policy-reported") == 0) {
        qtype_policy_numeric_lists(candidates, refused);
        yvex_model_target_report_fact_text(report, "policy", "artifact-planning-storage-policy");
        yvex_model_target_report_fact_text(report, "preferred", "F16");
        yvex_model_target_report_fact_text(report, "candidates", candidates);
        yvex_model_target_report_fact_text(report, "refused", refused);
    }
    yvex_model_target_report_fact_text(report, "top_blocker", state.top_blocker);
    yvex_model_target_report_fact_text(report, "next", state.next_row);
    if (strcmp(state.status, "policy-reported") == 0) {
        yvex_model_target_report_fact_text(report, "boundary", "report-only; no quantization/artifact/runtime");
    }
    return YVEX_OK;
}

/*
 * Project canonical qtype capability facts by tensor role.
 *
 * Role-support matrices are report-only facts and hand off incomplete quantization work to later
 * rows. Qtype role-support reporting is not qtype support completion, quantization, artifact
 * emission, runtime readiness, generation readiness, benchmark evidence, or release readiness.
 */
typedef struct {
    const char *role_name;
    const char *source_dtype;
    const char *storage_status;
    const char *blocker;
} qtype_role_fact;

typedef struct {
    const char *family;
    const char *target_id;
    const char *status;
    const char *top_blocker;
    const char *next_row;
} qtype_gate_family_fact;

typedef struct {
    const char *target;
    const char *family;
    const char *source_dtype;
    const char *top_blocker;
    const char *next_row;
    unsigned long role_count;
} qtype_role_summary;

#define QTYPE_ROLE_TEXT(key, value) { YVEX_MODEL_TARGET_ROW_LITERAL, (value), 0u, key }
#define QTYPE_ROLE_LITERAL(text) \
    { YVEX_MODEL_TARGET_ROW_LITERAL, (text), 0u, NULL }
#define QTYPE_ROLE_STRING(field, key, format) \
    { YVEX_MODEL_TARGET_ROW_STRING, (format), offsetof(qtype_role_summary, field), key }
#define QTYPE_ROLE_ULONG(field, key, format) \
    { YVEX_MODEL_TARGET_ROW_ULONG, (format), offsetof(qtype_role_summary, field), key }

static const yvex_model_target_row_spec qtype_role_summary_rows[] = {
    QTYPE_ROLE_STRING(target, "qtype-role-support", "qtype-role-support: %s"),
    QTYPE_ROLE_TEXT("status", "blocked"),
    QTYPE_ROLE_STRING(family, "family", "family: %s"),
    QTYPE_ROLE_STRING(source_dtype, "source_dtype", "source_dtype: %s"),
    QTYPE_ROLE_TEXT("preferred_artifact_qtype", "unresolved"),
    QTYPE_ROLE_ULONG(role_count, "supported_roles", "supported_roles: %lu"),
    QTYPE_ROLE_ULONG(role_count, "blocked_roles", "blocked_roles: %lu"),
    QTYPE_ROLE_STRING(top_blocker, "top_blocker", "top_blocker: %s"),
    QTYPE_ROLE_STRING(next_row, "next", "next: %s"),
    QTYPE_ROLE_TEXT("boundary", "qtype role report only; no quantization/GGUF/runtime/generation")
};

static const char *const qtype_role_table_prefix[] = {
    "QTYPE ROLE SUPPORT",
    "ROLE  SRC_DTYPE  ARTIFACT_QTYPE  STORAGE  COMPUTE  CALIBRATION  STATUS"
};

static const yvex_model_target_request_rules qtype_role_rules = {
    YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY,
    "qtype-role-support-fail",
    "qtype role-support report requires quant-policy command kind",
    NULL,
    1
};

#undef QTYPE_ROLE_LITERAL
#undef QTYPE_ROLE_STRING
#undef QTYPE_ROLE_ULONG

static const qtype_role_fact qwen_role_facts[] = {
    {"token_embedding", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_q", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_k", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_v", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_o", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"qwen_linear_attn_A_log", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"moe_expert_gate_up", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"output_head", "BF16", "source-native", "artifact-emitter-missing"},
    {"tokenizer_metadata", "metadata", "metadata-sidecar", "artifact-emitter-missing"},
};

static const qtype_role_fact gemma_role_facts[] = {
    {"token_embedding", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_q_norm", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_k_norm", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_q", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_k", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_v", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"attention_o", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"pre_feedforward_layernorm", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"layer_scalar", "BF16", "source-native", "family-quantization-plan-unimplemented"},
    {"output_head_tied_embedding", "BF16", "source-native", "artifact-emitter-missing"},
    {"tokenizer_metadata", "metadata", "metadata-sidecar", "artifact-emitter-missing"},
};

static const qtype_gate_family_fact qtype_gate_rows[] = {
    {"deepseek", "deepseek4-v4-flash-dspark", "blocked",
     "artifact-materialization-unimplemented", "V010.ARTIFACT.MATERIALIZE.0"},
    {"qwen", "qwen3-6-35b-a3b", "blocked", "family-quantization-plan-unimplemented", "not-scheduled"},
    {"gemma", "gemma-4-31b-it", "blocked", "family-quantization-plan-unimplemented", "not-scheduled"},
};

static const char *qtype_role_compute_status(const char *source_dtype)
{
    const yvex_quant_numeric_capability *capability;
    unsigned int qtype;

    if (source_dtype && strcmp(source_dtype, "metadata") == 0)
        return "not-applicable";
    if (source_dtype && strcmp(source_dtype, "F32") == 0)
        qtype = YVEX_GGUF_QTYPE_F32;
    else if (source_dtype && strcmp(source_dtype, "F16") == 0)
        qtype = YVEX_GGUF_QTYPE_F16;
    else if (source_dtype && strcmp(source_dtype, "BF16") == 0)
        qtype = YVEX_GGUF_QTYPE_BF16;
    else
        return "unresolved-source-dtype";
    capability = yvex_quant_numeric_capability_at(qtype);
    return capability && capability->dedicated_cpu_compute_available &&
           capability->dedicated_cuda_compute_available
        ? "cpu-cuda-available" : "unavailable";
}

static const qtype_role_fact *qtype_role_rows(const char *family,
                                             unsigned long *count)
{
    if (family && strcmp(family, "gemma") == 0) {
        if (count) {
            *count = sizeof(gemma_role_facts) / sizeof(gemma_role_facts[0]);
        }
        return gemma_role_facts;
    }
    if (count) {
        *count = sizeof(qwen_role_facts) / sizeof(qwen_role_facts[0]);
    }
    return qwen_role_facts;
}

static void qtype_role_prepare(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";
    const char *family = yvex_model_target_family_key(target);
    int deepseek = strcmp(family, "deepseek") == 0;
    const yvex_model_target_report_profile profile = {
        .status = request->gate[0] ? "qtype-role-support-gate-blocked"
                                   : "qtype-role-support-blocked",
        .target_id = target, .family = family, .stage = "report-only",
        .qtype_policy_status = "blocked", .artifact_status = "missing",
        .runtime_status = "unsupported", .generation_status = "unsupported-full-model",
        .benchmark_status = "not-measured",
        .next_row = deepseek ? "V010.ARTIFACT.MATERIALIZE.0" : "not-scheduled",
        .reason = deepseek ? "complete-artifact-admission-required"
                           : "family-quantization-plan-unimplemented",
        .boundary = "qtype role-support report only; no quantization or artifact emission"
    };

    yvex_model_target_report_prepare(report, request, &profile);
}

static int qtype_role_validate(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    if (!yvex_model_target_validate_request_shape(
            request, report, &qtype_role_rules, request->gate)) {
        return 1;
    }
    return 0;
}

static void qtype_gate_add_table(yvex_model_target_report *report)
{
    unsigned long i;

    yvex_model_target_report_add_row(report, "QTYPE ROLE SUPPORT GATE");
    yvex_model_target_report_add_table_row(report, 7u, "FAMILY", "TARGET",
                                           "STATUS", "ROLES", "BLOCKED",
                                           "TOP_BLOCKER", "NEXT", NULL);
    for (i = 0; i < sizeof(qtype_gate_rows) / sizeof(qtype_gate_rows[0]); ++i) {
        yvex_model_target_report_add_table_row(report, 7u, qtype_gate_rows[i].family,
                                               qtype_gate_rows[i].target_id,
                                               qtype_gate_rows[i].status,
                                               "1",
                                               "1",
                                               qtype_gate_rows[i].top_blocker,
                                               qtype_gate_rows[i].next_row,
                                               NULL);
    }
}

static void qtype_gate_add_audit(yvex_model_target_report *report)
{
    unsigned long i;

    yvex_model_target_report_fact_text(report, "report", "qtype-role-support-gate");
    yvex_model_target_report_fact_text(report, "status", "qtype-role-support-gate-blocked");
    yvex_model_target_report_fact_text(report, "release", "v0.1.0");
    for (i = 0; i < sizeof(qtype_gate_rows) / sizeof(qtype_gate_rows[0]); ++i) {
        char name[128];
#define GATE_FACT(field, key) \
        snprintf(name, sizeof(name), "family.%lu." key, i); \
        yvex_model_target_report_fact_text(report, name, qtype_gate_rows[i].field)
        GATE_FACT(family, "name");
        GATE_FACT(target_id, "target_id");
        GATE_FACT(status, "status");
        GATE_FACT(top_blocker, "top_blocker");
        GATE_FACT(next_row, "next");
#undef GATE_FACT
    }
    yvex_model_target_report_common_tail(report);
}

static void qtype_role_add_table(const char *family,
                                 yvex_model_target_report *report)
{
    const qtype_role_fact *rows;
    unsigned long count;
    unsigned long i;

    yvex_model_target_report_add_rows(
        report, qtype_role_table_prefix,
        sizeof(qtype_role_table_prefix) / sizeof(qtype_role_table_prefix[0]));
    rows = qtype_role_rows(family, &count);
    for (i = 0; i < count; ++i) {
        yvex_model_target_report_add_row(report,
                                         "%s  %s  unresolved  header-storage-profiled  %s  deferred  present",
                                         rows[i].role_name,
                                         rows[i].source_dtype,
                                         qtype_role_compute_status(
                                             rows[i].source_dtype));
    }
}

static void qtype_role_add_audit(const char *family,
                                 yvex_model_target_report *report)
{
    const qtype_role_fact *rows;
    unsigned long count;
    unsigned long i;
    int selected_slice = strcmp(family, "deepseek") == 0;

    yvex_model_target_report_fact_text(report, "report", "qtype-role-support");
    yvex_model_target_report_fact_text(report, "status", "qtype-role-support-blocked");
    yvex_model_target_report_fact_text(report, "target_id", report->target_id);
    yvex_model_target_report_fact_text(report, "family", family);
    yvex_model_target_report_fact_text(report, "source_dtype", selected_slice ? "selected-slice" : "BF16");
    if (selected_slice) {
        yvex_model_target_report_fact_text(report, "selected_slice_evidence_only", "true");
        yvex_model_target_report_fact_text(report, "full_family_artifact_status", "missing");
    }
    rows = qtype_role_rows(family, &count);
    for (i = 0; i < count; ++i) {
        char name[128];
#define ROLE_FACT(key, value) \
        snprintf(name, sizeof(name), "role.%lu." key, i); \
        yvex_model_target_report_fact_text(report, name, value)
        ROLE_FACT("role_name", rows[i].role_name);
        ROLE_FACT("source_dtype", selected_slice ? "selected-slice" : rows[i].source_dtype);
        if (selected_slice) {
            ROLE_FACT("role_status", "selected-slice-evidence-only");
        }
        ROLE_FACT("compute_support_status", qtype_role_compute_status(rows[i].source_dtype));
        ROLE_FACT("artifact_emission_allowed", "false");
        ROLE_FACT("artifact_emission_blocker", selected_slice
            ? "complete-artifact-admission-required" : rows[i].blocker);
#undef ROLE_FACT
    }
    yvex_model_target_report_fact_text(report, "payload_bytes_read", "false");
    yvex_model_target_report_fact_text(report, "quantization_performed", "false");
    yvex_model_target_report_fact_text(report, "gguf_emitted", "false");
    yvex_model_target_report_common_tail(report);
}

int yvex_qtype_role_support_report_build(const yvex_model_target_request *request,
                                         yvex_model_target_report *report,
                                         yvex_error *err)
{
    const char *family;
    unsigned long role_count = 0;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "qtype_role_support_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    qtype_role_prepare(request, report);
    if (qtype_role_validate(request, report)) {
        return YVEX_OK;
    }
    family = report->family[0] ? report->family : "qwen";
    (void)qtype_role_rows(family, &role_count);
    if (request->gate[0]) {
        if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            qtype_gate_add_table(report);
        } else if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            qtype_gate_add_audit(report);
        } else {
            yvex_model_target_report_fact_text(report, "qtype-role-support-gate", "v0.1.0");
            yvex_model_target_report_fact_text(report, "status", "qtype-role-support-gate-blocked");
            yvex_model_target_report_fact_u64(report, "family_count", 3u);
            yvex_model_target_report_fact_text(report, "top_blocker", "artifact-materialization-unimplemented");
            yvex_model_target_report_fact_text(report, "next", "V010.ARTIFACT.MATERIALIZE.0");
            yvex_model_target_report_common_tail(report);
        }
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        qtype_role_add_table(family, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        qtype_role_add_audit(family, report);
        return YVEX_OK;
    }
    {
        int deepseek = strcmp(family, "deepseek") == 0;
        const qtype_role_summary summary = {
            report->target_id, family, deepseek ? "selected-slice" : "BF16",
            deepseek ? "complete-artifact-admission-required"
                     : "family-quantization-plan-unimplemented",
            deepseek ? "V010.ARTIFACT.MATERIALIZE.0" : "not-scheduled",
            role_count
        };

        yvex_model_target_report_project_rows(
            report, qtype_role_summary_rows,
            sizeof(qtype_role_summary_rows) / sizeof(qtype_role_summary_rows[0]),
            &summary);
    }
    return YVEX_OK;
}
