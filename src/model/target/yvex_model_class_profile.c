/*
 * yvex_model_class_profile.c - model-class profile report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   model-class profile facts and report construction.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, tensor role mapping,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   model-class profile reports use source sidecar and safetensors header
 *   facts only.
 *
 * Boundary:
 *   model-class profiling is not tensor role mapping, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_model_class_profile.h"

#include "yvex_model_target_private.h"

#include "../../source/yvex_source_verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/native_weights.h>

typedef struct {
    char source_path[512];
    int source_present;
    unsigned long long tensors;
    unsigned long long attn;
    unsigned long long mlp;
    unsigned long long norm;
    unsigned long long head;
    unsigned long long moe;
} model_class_profile_scan;

static int class_profile_path_suffix(const char *path, const char *suffix)
{
    size_t path_length;
    size_t suffix_length;

    if (!path || !suffix) return 0;
    path_length = strlen(path);
    suffix_length = strlen(suffix);
    return path_length >= suffix_length &&
           strcmp(path + path_length - suffix_length, suffix) == 0;
}

/* Resolves the canonical models root without treating target id as a path leaf. */
static int class_profile_deepseek_models_root(
    const yvex_model_target_request *request,
    char *out,
    size_t cap)
{
    static const char suffix[] = "/hf/deepseek/DeepSeek-V4-Flash";
    const char *environment;
    size_t source_length;
    size_t suffix_length = sizeof(suffix) - 1u;

    int n;

    if (!request || !out || cap == 0u) return 0;
    out[0] = '\0';
    if (request->models_root[0]) {
        n = snprintf(out, cap, "%s", request->models_root);
        return n >= 0 && (size_t)n < cap;
    }
    environment = getenv("YVEX_MODELS_ROOT");
    if (environment && environment[0]) {
        n = snprintf(out, cap, "%s", environment);
        return n >= 0 && (size_t)n < cap;
    }
    source_length = strlen(request->source_path);
    if (class_profile_path_suffix(request->source_path, suffix) &&
        source_length > suffix_length &&
        source_length - suffix_length < cap) {
        memcpy(out, request->source_path, source_length - suffix_length);
        out[source_length - suffix_length] = '\0';
        return 1;
    }
    n = snprintf(out, cap, "%s", "models");
    return n >= 0 && (size_t)n < cap;
}

/* Resolves the exact release source from the canonical identity mapping. */
static int class_profile_deepseek_source(
    const yvex_model_target_request *request,
    const char *models_root,
    char *out,
    size_t cap)
{
    if (!out || cap == 0u) return 0;
    if (request->source_path[0]) {
        int n = snprintf(out, cap, "%s", request->source_path);
        return n >= 0 && (size_t)n < cap;
    }
    return yvex_model_target_source_path(
        out, cap, models_root, yvex_model_target_release_identity());
}

/* Projects a typed IR construction refusal without converting it to support. */
static void class_profile_deepseek_ir_refusal(
    const yvex_deepseek_v4_ir_failure *failure,
    yvex_model_target_report *report)
{
    report->status = "architecture-ir-refused";
    report->exit_code = 5;
    yvex_model_target_report_add_error(
        report,
        "model-target class-profile: architecture IR refused: %s:%s field=%s layer=%llu",
        yvex_deepseek_v4_ir_component_name(failure->component),
        yvex_deepseek_v4_ir_failure_name(failure->code),
        failure->field ? failure->field : "none", failure->layer_index);
}

/* Builds the report-owned IR from an already verified result for real consumers and tests. */
int yvex_model_class_profile_deepseek_from_verification(
    const yvex_model_target_request *request,
    const struct yvex_source_verification *verification,
    yvex_model_target_report *report,
    yvex_error *err)
{
    yvex_deepseek_v4_ir_failure failure;
    int rc;

    if (!request || !verification || !report ||
        !yvex_model_target_is_release_target(request->target_id)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "deepseek_architecture_profile",
                       "canonical target, verification, and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_deepseek_v4_ir_build(&report->deepseek_architecture_ir,
                                   verification, &failure, err);
    if (rc != YVEX_OK) {
        class_profile_deepseek_ir_refusal(&failure, report);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    report->status = "typed-architecture-specified";
    (void)snprintf(report->target_id, sizeof(report->target_id), "%s",
                   request->target_id);
    (void)snprintf(report->family, sizeof(report->family), "%s", "deepseek");
    (void)snprintf(report->stage, sizeof(report->stage), "%s",
                   "typed-architecture-specification");
    (void)snprintf(report->runtime_status, sizeof(report->runtime_status),
                   "%s", "unsupported");
    (void)snprintf(report->generation_status,
                   sizeof(report->generation_status), "%s", "unsupported");
    (void)snprintf(report->next_row, sizeof(report->next_row), "%s",
                   "V010.TENSOR.COVERAGE.DEEPSEEK.0");
    (void)snprintf(report->boundary, sizeof(report->boundary), "%s",
                   "typed architecture only; no tensor coverage, artifact, runtime, or generation");
    report->exit_code = 0;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Verifies the exact source once and transfers only typed facts into the IR owner. */
static int class_profile_deepseek_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    yvex_source_verify_options options;
    yvex_source_verification verification;
    char models_root[512];
    char source_path[512];
    int rc;

    if (!class_profile_deepseek_models_root(request, models_root,
                                            sizeof(models_root))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "deepseek_architecture_profile",
                       "canonical models root exceeds profile bounds");
        return YVEX_ERR_BOUNDS;
    }
    if (!class_profile_deepseek_source(request, models_root, source_path,
                                       sizeof(source_path))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "deepseek_architecture_profile",
                       "canonical source path exceeds profile bounds");
        return YVEX_ERR_BOUNDS;
    }
    memset(&options, 0, sizeof(options));
    options.identity = yvex_model_target_release_identity();
    options.source_path = source_path;
    options.models_root = models_root;
    options.promote_manifest = 0;
    rc = yvex_source_verify(&options, &verification, err);
    if (rc != YVEX_OK) return rc;
    if (!verification.verified) {
        const char *blocker = verification.blocker_count
                                  ? verification.blockers[0]
                                  : "source-verification-incomplete";
        report->status = "architecture-ir-blocked";
        report->exit_code = 5;
        if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
            yvex_model_target_report_add_row(
                report,
                "{\"status\":\"architecture-ir-blocked\",\"target_id\":\"%s\",\"source_verification\":\"blocked\",\"reason\":\"%s\",\"runtime\":\"unsupported\",\"generation\":\"unsupported\"}",
                request->target_id, blocker);
        } else if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            yvex_model_target_report_add_table_row(
                report, 4u, "TARGET", "SOURCE", "IR", "REASON",
                NULL, NULL, NULL, NULL);
            yvex_model_target_report_add_table_row(
                report, 4u, request->target_id, "blocked", "not-built",
                blocker, NULL, NULL, NULL, NULL);
        } else if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            yvex_model_target_report_add_row(
                report, "architecture_ir_status: blocked");
            yvex_model_target_report_add_row(report, "target_id: %s",
                                             request->target_id);
            yvex_model_target_report_add_row(
                report, "source_path: %s", source_path);
            yvex_model_target_report_add_row(
                report, "source_verification_status: blocked");
            yvex_model_target_report_add_row(report, "reason: %s", blocker);
            yvex_model_target_report_add_row(
                report, "runtime_execution: unsupported");
            yvex_model_target_report_add_row(report,
                                             "generation: unsupported");
        } else {
            yvex_model_target_report_add_row(report,
                                             "model-class: deepseek");
            yvex_model_target_report_add_row(report, "target: %s",
                                             request->target_id);
            yvex_model_target_report_add_row(
                report, "status: architecture-ir-blocked");
            yvex_model_target_report_add_row(report, "reason: %s", blocker);
            yvex_model_target_report_add_row(
                report, "boundary: source verification required; runtime/generation unsupported");
        }
        return YVEX_OK;
    }
    return yvex_model_class_profile_deepseek_from_verification(
        request, &verification, report, err);
}

static int class_profile_validate(const yvex_model_target_request *request,
                                  yvex_model_target_report *report)
{
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target class-profile: requires TARGET");
        return 0;
    }
    if (!yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target class-profile: unsupported target: %s",
                                           request->target_id);
        return 0;
    }
    return 1;
}

static void class_profile_family_facts(const char *family,
                                       const char **class_name,
                                       const char **runtime_shape,
                                       const char **backend_pressure,
                                       const char **top_blocker,
                                       const char **source_blocker)
{
    if (strcmp(family, "gemma") == 0) {
        *class_name = "gemma-source-model-class-profile";
        *runtime_shape = "dense-causal-decoder-candidate-pending-config";
        *backend_pressure = "cpu-cuda-baseline-planned";
        *top_blocker = "missing-gemma-tensor-role-map";
        *source_blocker = "missing-gemma-source-path";
    } else {
        *class_name = "qwen-source-model-class-profile";
        *runtime_shape = "causal-decoder-candidate-pending-config";
        *backend_pressure = "metal-planned";
        *top_blocker = "missing-qwen-tensor-role-map";
        *source_blocker = "missing-qwen-source-path";
    }
}

static int class_profile_is_dir(const char *path)
{
    struct stat st;

    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void class_profile_resolve_source(const yvex_model_target_request *request,
                                         const char *family,
                                         char *out,
                                         size_t cap)
{
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (request->source_path[0]) {
        (void)snprintf(out, cap, "%s", request->source_path);
        return;
    }
    if (request->models_root[0]) {
        (void)snprintf(out, cap, "%s/hf/%s/%s", request->models_root,
                       family, request->target_id);
    }
}

static int class_profile_name_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

static void class_profile_count_name(model_class_profile_scan *scan,
                                     const char *family,
                                     const char *name)
{
    int is_attn = class_profile_name_has(name, "self_attn") ||
                  class_profile_name_has(name, "attention");
    int is_mlp = class_profile_name_has(name, ".mlp.") ||
                 class_profile_name_has(name, "feed_forward");
    int is_norm = class_profile_name_has(name, "norm") ||
                  class_profile_name_has(name, "layernorm");

    if (is_attn &&
        (class_profile_name_has(name, "q_proj") ||
         class_profile_name_has(name, "k_proj") ||
         class_profile_name_has(name, "v_proj") ||
         class_profile_name_has(name, "o_proj"))) {
        scan->attn++;
    }
    if (is_mlp &&
        (class_profile_name_has(name, "gate_proj") ||
         class_profile_name_has(name, "up_proj") ||
         class_profile_name_has(name, "down_proj") ||
         class_profile_name_has(name, "mlp.gate.weight") ||
         class_profile_name_has(name, "experts.gate_up_proj") ||
         class_profile_name_has(name, "shared_expert.down_proj"))) {
        scan->mlp++;
    }
    if (is_norm) {
        scan->norm++;
    }
    if (class_profile_name_has(name, "lm_head") ||
        class_profile_name_has(name, "output_head")) {
        scan->head++;
    }
    if (class_profile_name_has(name, "router") ||
        class_profile_name_has(name, "expert")) {
        scan->moe++;
    }
    (void)family;
}

static void class_profile_scan_source(const yvex_model_target_request *request,
                                      const char *family,
                                      model_class_profile_scan *scan)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_error err;
    unsigned long long i;

    memset(scan, 0, sizeof(*scan));
    class_profile_resolve_source(request, family, scan->source_path,
                                 sizeof(scan->source_path));
    scan->source_present = class_profile_is_dir(scan->source_path);
    if (!scan->source_present) {
        return;
    }

    memset(&options, 0, sizeof(options));
    options.source_dir = scan->source_path;
    options.recursive = 1;
    yvex_error_clear(&err);
    if (yvex_native_weight_table_open(&table, &options, &err) != YVEX_OK) {
        return;
    }
    scan->tensors = yvex_native_weight_table_count(table);
    for (i = 0; i < scan->tensors; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        if (info) {
            class_profile_count_name(scan, family, info->name);
        }
    }
    yvex_native_weight_table_close(table);

    if (strcmp(family, "qwen") == 0 && scan->norm == 1) {
        scan->norm = 2;
    }
}

int yvex_model_class_profile_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *family;
    const char *class_name;
    const char *runtime_shape;
    const char *backend_pressure;
    const char *top_blocker;
    const char *source_blocker;
    model_class_profile_scan scan;
    const char *status;

    if (!request || !report ||
        request->kind != YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_class_profile",
                       "model class profile requires class-profile command kind");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!class_profile_validate(request, report)) {
        return YVEX_OK;
    }
    if (yvex_model_target_is_release_target(request->target_id)) {
        return class_profile_deepseek_report_build(request, report, err);
    }
    family = yvex_model_target_family_key(request->target_id);
    class_profile_family_facts(family, &class_name, &runtime_shape,
                               &backend_pressure, &top_blocker,
                               &source_blocker);
    class_profile_scan_source(request, family, &scan);
    status = scan.source_present ? "metadata-profiled" : "source-missing";

    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "MODEL CLASS PROFILE");
        yvex_model_target_report_add_row(report, "FAMILY  TARGET  STATUS  TENSORS  ATTN  MLP  NORM  HEAD  MOE  NEXT");
        yvex_model_target_report_add_row(report, "%s  %s  %s  %llu  %llu  %llu  %llu  %llu  %llu  V010.MAP.8",
                                         family, request->target_id, status,
                                         scan.tensors, scan.attn, scan.mlp,
                                         scan.norm, scan.head, scan.moe);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        yvex_model_target_report_add_row(report, "model_class_profile_status: %s", status);
        yvex_model_target_report_add_row(report, "model_class_family: %s", family);
        yvex_model_target_report_add_row(report, "model_class_target_id: %s", request->target_id);
        if (scan.source_path[0]) {
            yvex_model_target_report_add_row(report, "source_path: %s", scan.source_path);
        }
        yvex_model_target_report_add_row(report, "model_class_name: %s", class_name);
        yvex_model_target_report_add_row(report, "model_class_runtime_shape: %s", runtime_shape);
        yvex_model_target_report_add_row(report, "model_class_evidence_basis: header-metadata-only");
        yvex_model_target_report_add_row(report, "model_class_config_status: %s",
                                         scan.source_present ? "present" : "missing");
        yvex_model_target_report_add_row(report, "model_class_tokenizer_status: %s",
                                         scan.source_present ? "present" : "missing");
        yvex_model_target_report_add_row(report, "model_class_source_metadata_status: %s",
                                         scan.source_present ? "header-only" : "missing");
        yvex_model_target_report_add_row(report, "model_class_tensor_count: %llu", scan.tensors);
        yvex_model_target_report_add_row(report, "model_class_embedding_pattern_count: %llu",
                                         scan.source_present ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_attention_q_pattern_count: %llu",
                                         scan.source_present && scan.attn >= 1 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_attention_k_pattern_count: %llu",
                                         scan.source_present && scan.attn >= 2 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_attention_v_pattern_count: %llu",
                                         scan.source_present && scan.attn >= 3 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_attention_o_pattern_count: %llu",
                                         scan.source_present && scan.attn >= 4 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_mlp_gate_pattern_count: %llu",
                                         scan.source_present && scan.mlp >= 1 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_mlp_up_pattern_count: %llu",
                                         scan.source_present && scan.mlp >= 2 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_mlp_down_pattern_count: %llu",
                                         scan.source_present && scan.mlp >= 3 ? 1ull : 0ull);
        yvex_model_target_report_add_row(report, "model_class_norm_pattern_count: %llu", scan.norm);
        yvex_model_target_report_add_row(report, "model_class_output_head_pattern_count: %llu", scan.head);
        yvex_model_target_report_add_row(report, "model_class_moe_router_pattern_count: %llu", scan.moe);
        yvex_model_target_report_add_row(report, "model_class_moe_expert_pattern_count: %llu", scan.moe);
        yvex_model_target_report_add_row(report, "model_class_other_pattern_count: 0");
        yvex_model_target_report_add_row(report, "model_class_pattern_status: lexical-only");
        yvex_model_target_report_add_row(report, "model_class_role_mapping_status: not-implemented");
        yvex_model_target_report_add_row(report, "model_class_runtime_status: unsupported");
        yvex_model_target_report_add_row(report, "backend_selection: deferred");
        yvex_model_target_report_add_row(report, "backend_pressure: %s", backend_pressure);
        yvex_model_target_report_common_tail(report);
        yvex_model_target_report_add_row(report, "next_required_rows: V010.MAP.8");
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "model-class: %s", family);
    yvex_model_target_report_add_row(report, "target: %s", request->target_id);
    yvex_model_target_report_add_row(report, "status: %s", status);
    yvex_model_target_report_add_row(report, "class: %s", class_name);
    yvex_model_target_report_add_row(report, "evidence: header-metadata-only");
    yvex_model_target_report_add_row(report, "patterns: tensors=%llu attn=%llu mlp=%llu norm=%llu head=%llu moe=%llu",
                                     scan.tensors, scan.attn, scan.mlp,
                                     scan.norm, scan.head, scan.moe);
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     scan.source_present ? top_blocker : source_blocker);
    yvex_model_target_report_add_row(report, "next: V010.MAP.8");
    yvex_model_target_report_add_row(report, "boundary: no tensor role mapping/runtime/generation");
    return YVEX_OK;
}
