/*
 * qtype_policy.c - qtype policy report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   qtype policy facts, qtype refusal facts, and policy report construction.
 *
 * Does not own:
 *   CLI parsing, rendering, quantization execution, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   qtype policy facts are planning/report-only and do not perform
 *   quantization, write GGUF artifacts, or mark runtime paths ready.
 *
 * Boundary:
 *   qtype policy reporting is not quantization, artifact emission, runtime
 *   support, generation readiness, benchmark evidence, or release readiness.
 */
#include "qtype_policy.h"

#include "private.h"
#include "src/gguf/quant_numeric.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct {
    int source_requested;
    int source_dir_exists;
    int header_exists;
    unsigned long f32_count;
    unsigned long f16_count;
    unsigned long bf16_count;
    unsigned long other_count;
    unsigned long tensor_count;
    int attention_k_present;
    int output_head_present;
    int output_head_ambiguous;
    int metadata_present;
    const char *mapping_gate_status;
    const char *top_blocker;
    const char *next_row;
    const char *status;
    const char *bracket;
} qtype_policy_state;

/*
 * qtype_policy_dir_exists()
 *
 * Purpose:
 *   classify whether the resolved source directory exists locally.
 *
 * Inputs:
 *   path is borrowed.
 *
 * Effects:
 *   performs a local stat only; no output is written.
 *
 * Failure:
 *   missing/unstatable paths return false.
 *
 * Boundary:
 *   directory presence is not source verification.
 */
static int qtype_policy_dir_exists(const char *path)
{
    struct stat st;

    if (!path || !path[0]) {
        return 0;
    }
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int qtype_policy_file_exists(const char *path)
{
    FILE *fp;

    if (!path || !path[0]) {
        return 0;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

static unsigned long long qtype_policy_le64(const unsigned char bytes[8])
{
    return ((unsigned long long)bytes[0]) |
           ((unsigned long long)bytes[1] << 8) |
           ((unsigned long long)bytes[2] << 16) |
           ((unsigned long long)bytes[3] << 24) |
           ((unsigned long long)bytes[4] << 32) |
           ((unsigned long long)bytes[5] << 40) |
           ((unsigned long long)bytes[6] << 48) |
           ((unsigned long long)bytes[7] << 56);
}

/*
 * qtype_policy_read_header()
 *
 * Purpose:
 *   read bounded safetensors header JSON for dtype and role evidence.
 *
 * Inputs:
 *   path is borrowed; out receives an owned string.
 *
 * Effects:
 *   reads only header bytes and never tensor payload bytes.
 *
 * Failure:
 *   returns false for missing, malformed, oversized, or allocation failures.
 *
 * Boundary:
 *   header evidence is report-only and not tensor materialization.
 */
static int qtype_policy_read_header(const char *path, char **out)
{
    FILE *fp;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    char *json;

    if (!path || !out) {
        return 0;
    }
    *out = NULL;
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    if (fread(len_bytes, 1u, sizeof(len_bytes), fp) != sizeof(len_bytes)) {
        fclose(fp);
        return 0;
    }
    header_len = qtype_policy_le64(len_bytes);
    if (header_len == 0ull || header_len > 1024ull * 1024ull) {
        fclose(fp);
        return 0;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        return 0;
    }
    if (fread(json, 1u, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    json[header_len] = '\0';
    *out = json;
    return 1;
}

static unsigned long qtype_policy_count_substr(const char *text, const char *needle)
{
    unsigned long count = 0;
    const char *p = text;

    if (!text || !needle || !needle[0]) {
        return 0;
    }
    while ((p = strstr(p, needle)) != NULL) {
        count++;
        p += strlen(needle);
    }
    return count;
}

static void qtype_policy_source_dir(const yvex_model_target_request *request,
                                    const char *family,
                                    char *out,
                                    size_t cap)
{
    if (!out || cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (request->source_path[0]) {
        (void)snprintf(out, cap, "%s", request->source_path);
    } else if (request->models_root[0]) {
        (void)snprintf(out, cap, "%s/hf/%s/%s",
                       request->models_root, family, request->target_id);
    }
}

/*
 * qtype_policy_build_state()
 *
 * Purpose:
 *   gather qtype policy inputs from bounded source/header metadata.
 *
 * Inputs:
 *   request/family are borrowed; state is mutated.
 *
 * Effects:
 *   reads metadata file presence and safetensors headers only; no output or
 *   payload loading occurs.
 *
 * Failure:
 *   missing source/header facts become typed blockers.
 *
 * Boundary:
 *   qtype planning facts do not execute quantization or emit artifacts.
 */
static void qtype_policy_build_state(const yvex_model_target_request *request,
                                     const char *family,
                                     qtype_policy_state *state)
{
    char dir[1024];
    char path[1200];
    char *header = NULL;

    memset(state, 0, sizeof(*state));
    state->mapping_gate_status = "passed-for-artifact-planning";
    state->top_blocker = "family-quantization-plan-unimplemented";
    state->next_row = "not-scheduled";
    state->status = "policy-reported";
    state->bracket = "reported";

    qtype_policy_source_dir(request, family, dir, sizeof(dir));
    state->source_requested = dir[0] != '\0';
    if (!state->source_requested) {
        return;
    }
    state->source_dir_exists = qtype_policy_dir_exists(dir);
    if (!state->source_dir_exists) {
        state->mapping_gate_status = "blocked-missing-source";
        state->top_blocker = strcmp(family, "gemma") == 0
                                 ? "missing-gemma-source-path"
                                 : "missing-qwen-source-path";
        state->next_row = "V010.MAP.9";
        state->status = "blocked";
        state->bracket = "blocked";
        return;
    }

    (void)snprintf(path, sizeof(path), "%s/model.safetensors", dir);
    state->header_exists = qtype_policy_read_header(path, &header);
    if (!state->header_exists) {
        state->mapping_gate_status = "blocked-missing-dtype";
        state->top_blocker = "missing-source-dtype-profile";
        state->next_row = "V010.MAP.9";
        state->status = "blocked";
        state->bracket = "blocked";
    } else {
        state->f32_count = qtype_policy_count_substr(header, "\"dtype\":\"F32\"");
        state->f16_count = qtype_policy_count_substr(header, "\"dtype\":\"F16\"");
        state->bf16_count = qtype_policy_count_substr(header, "\"dtype\":\"BF16\"");
        state->tensor_count = state->f32_count + state->f16_count +
                              state->bf16_count;
        state->other_count = qtype_policy_count_substr(header, "\"dtype\":") -
                             state->tensor_count;
        state->attention_k_present = strstr(header, "k_proj.weight") != NULL;
        state->output_head_present = strstr(header, "lm_head.weight") != NULL ||
                                     strstr(header, "output.weight") != NULL;
        state->output_head_ambiguous = strstr(header, "lm_head.weight") != NULL &&
                                       strstr(header, "output.weight") != NULL;
        free(header);
        if (!state->attention_k_present) {
            state->mapping_gate_status = "blocked-missing-runtime-roles";
            state->top_blocker = "missing-source-role-attention-k";
            state->next_row = "V010.MAP.9";
            state->status = "blocked";
            state->bracket = "blocked";
        } else if (!state->output_head_present) {
            state->mapping_gate_status = "blocked-missing-runtime-roles";
            state->top_blocker = "missing-output-head-tensor";
            state->next_row = "V010.MAP.9";
            state->status = "blocked";
            state->bracket = "blocked";
        } else if (state->output_head_ambiguous) {
            state->mapping_gate_status = "blocked-missing-runtime-roles";
            state->top_blocker = "ambiguous-output-head-tensor";
            state->next_row = "V010.MAP.9";
            state->status = "blocked";
            state->bracket = "blocked";
        }
    }

    (void)snprintf(path, sizeof(path), "%s/tokenizer.json", dir);
    state->metadata_present = qtype_policy_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/config.json", dir);
    state->metadata_present = state->metadata_present &&
                              qtype_policy_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/generation_config.json", dir);
    state->metadata_present = state->metadata_present &&
                              qtype_policy_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/special_tokens_map.json", dir);
    state->metadata_present = state->metadata_present &&
                              qtype_policy_file_exists(path);
    if (state->header_exists && !state->metadata_present &&
        strcmp(state->status, "policy-reported") == 0) {
        state->mapping_gate_status = "blocked-missing-runtime-roles";
        state->top_blocker = "missing-tokenizer-sidecars";
        state->next_row = "V010.MAP.9";
        state->status = "blocked";
        state->bracket = "blocked";
    }
}

/*
 * qtype_policy_prepare()
 *
 * Purpose:
 *   initialize common fields for qtype policy reports.
 *
 * Inputs:
 *   request, state, and report are borrowed.
 *
 * Effects:
 *   mutates report fields only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   policy fields remain report-only facts.
 */
static void qtype_policy_prepare(const yvex_model_target_request *request,
                                 const qtype_policy_state *state,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "";
    const char *family = yvex_model_target_family_key(target);

    report->kind = request->kind;
    report->mode = request->mode;
    report->status = state->status;
    report->exit_code = 0;
    snprintf(report->target_id, sizeof(report->target_id), "%s", target);
    snprintf(report->family, sizeof(report->family), "%s", family);
    snprintf(report->stage, sizeof(report->stage), "report-only");
    snprintf(report->qtype_policy_status, sizeof(report->qtype_policy_status),
             "%s", state->status);
    snprintf(report->artifact_status, sizeof(report->artifact_status), "missing");
    snprintf(report->runtime_status, sizeof(report->runtime_status), "unsupported");
    snprintf(report->generation_status, sizeof(report->generation_status),
             "unsupported-full-model");
    snprintf(report->benchmark_status, sizeof(report->benchmark_status),
             "not-measured");
    snprintf(report->next_row, sizeof(report->next_row), "%s", state->next_row);
    snprintf(report->reason, sizeof(report->reason), "%s", state->top_blocker);
    snprintf(report->boundary, sizeof(report->boundary),
             "report-only; no quantization/artifact/runtime");
}

/*
 * qtype_policy_validate()
 *
 * Purpose:
 *   reject impossible typed request shapes for qtype policy reports.
 *
 * Inputs:
 *   request and report are borrowed.
 *
 * Effects:
 *   may append typed error rows and set exit_code; no printing occurs.
 *
 * Failure:
 *   returns 1 when a typed refusal has been populated.
 *
 * Boundary:
 *   refusal rows do not run quantization or inspect payload bytes.
 */
static int qtype_policy_validate(const yvex_model_target_request *request,
                                 yvex_model_target_report *report)
{
    const yvex_model_target_record *record;
    const char *target = request->target_id;
    const char *family;

    if (request->kind != YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY) {
        report->status = "qtype-policy-fail";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "qtype policy report requires quant-policy command kind");
        return 1;
    }
    if (!target[0]) {
        report->status = "parser-error";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "model-target quant-policy: requires TARGET");
        return 1;
    }
    if (request->release[0] && strcmp(request->release, "v0.1.0") != 0) {
        report->status = "unsupported-release";
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: unsupported-release");
        yvex_model_target_report_add_row(report, "release: %s", request->release);
        yvex_model_target_report_add_error(report, "unsupported release: %s",
                                           request->release);
        return 1;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        report->status = "unsupported-output-mode";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "JSON output is unsupported");
        return 1;
    }

    record = yvex_model_target_find(target);
    family = yvex_model_target_family_key(target);
    if (!record) {
        report->status = "unsupported-target";
        report->exit_code = 2;
        if (request->output_contract[0]) {
            yvex_model_target_report_add_row(report, "status: unsupported-target");
            return 1;
        }
        yvex_model_target_report_add_row(report, "qtype-policy: %s [unsupported]",
                                         target);
        yvex_model_target_report_add_row(report, "top_blocker: unsupported-target");
        yvex_model_target_report_add_error(report, "unsupported target: %s", target);
        return 1;
    }
    if (strcmp(family, "qwen") != 0 && strcmp(family, "gemma") != 0) {
        report->status = strcmp(family, "deepseek") == 0 ? "blocked" : "unsupported";
        yvex_model_target_report_add_row(
            report, "qtype-policy: %s [%s]", target,
            strcmp(family, "deepseek") == 0 ? "blocked" : "unsupported");
        yvex_model_target_report_add_row(report, "family: %s", family);
        yvex_model_target_report_add_row(
            report, "top_blocker: %s",
            strcmp(family, "deepseek") == 0
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
        yvex_model_target_report_add_row(report, "status: parser-error");
        return;
    }
    if (strcmp(request->output_contract, "normal") != 0 &&
        strcmp(request->output_contract, "table") != 0 &&
        strcmp(request->output_contract, "audit") != 0) {
        report->status = "unsupported-mode";
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: unsupported-mode");
        return;
    }
    yvex_model_target_report_add_output_contract(
        report, "qtype-policy", request->output_contract);
}

/* Projects policy candidates through the canonical numeric registry. */
static void qtype_policy_numeric_lists(char candidates[96],
                                       char refused[96])
{
    static const unsigned int policy_qtypes[] = {
        YVEX_GGUF_QTYPE_F16, YVEX_GGUF_QTYPE_BF16,
        YVEX_GGUF_QTYPE_F32, YVEX_GGUF_QTYPE_Q8_0,
        YVEX_GGUF_QTYPE_Q2_K, YVEX_GGUF_QTYPE_Q4_K,
        YVEX_GGUF_QTYPE_IQ2_XXS
    };
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
        report->target_id, report->family, state->f32_count, state->f16_count,
        state->bf16_count, state->other_count, candidates, refused,
        state->status, state->next_row);
}

static void qtype_policy_add_audit(const qtype_policy_state *state,
                                   yvex_model_target_report *report)
{
    const yvex_quant_numeric_capability *q8 =
        yvex_quant_numeric_capability_at(YVEX_GGUF_QTYPE_Q8_0);
    const yvex_quant_numeric_capability *q2 =
        yvex_quant_numeric_capability_at(YVEX_GGUF_QTYPE_Q2_K);
    yvex_model_target_report_add_row(report, "source_dtype_profile_status: %s",
                                     state->header_exists ? "profiled" : "missing");
    yvex_model_target_report_add_row(report, "source_dtype_counts: F32=%lu,F16=%lu,BF16=%lu",
                                     state->f32_count, state->f16_count,
                                     state->bf16_count);
    yvex_model_target_report_add_row(report, "source_tensor_count: %lu",
                                     state->tensor_count);
    yvex_model_target_report_add_row(report, "mapping_gate_status: %s",
                                     state->mapping_gate_status);
    yvex_model_target_report_add_row(report, "tensor_map_status: naming-map-profiled");
    yvex_model_target_report_add_row(report, "output_head_map_status: output-head-profiled");
    yvex_model_target_report_add_row(
        report, "tokenizer_metadata_map_status: %s",
        state->metadata_present ? "present-report-only" : "missing");
    yvex_model_target_report_add_row(report,
                                     "missing_role_report_status: missing-role-report-blocked");
    yvex_model_target_report_add_row(
        report,
        "qtype_policy_basis: header-only-source-metadata+canonical-numeric-registry");
    yvex_model_target_report_add_row(report, "qtype_policy_status: reported");
    yvex_model_target_report_add_row(
        report,
        "numeric_capability.Q8_0: encoder=%s decoder=%s cpu=%s cuda=%s calibration=%s",
        q8 && q8->encoder_available ? "available" : "unavailable",
        q8 && q8->reference_decoder_available ? "available" : "unavailable",
        q8 && q8->dedicated_cpu_compute_available ? "available" : "unavailable",
        q8 && q8->dedicated_cuda_compute_available ? "available" : "unavailable",
        q8 ? yvex_quant_calibration_name(q8->calibration) : "unknown");
    yvex_model_target_report_add_row(
        report,
        "numeric_capability.Q2_K: encoder=%s decoder=%s cpu=%s cuda=%s calibration=%s",
        q2 && q2->encoder_available ? "available" : "unavailable",
        q2 && q2->reference_decoder_available ? "available" : "unavailable",
        q2 && q2->dedicated_cpu_compute_available ? "available" : "unavailable",
        q2 && q2->dedicated_cuda_compute_available ? "available" : "unavailable",
        q2 ? yvex_quant_calibration_name(q2->calibration) : "unknown");
    yvex_model_target_report_add_row(
        report,
        "refusal_reasons: Q4_K:encoder-unavailable IQ2_XXS:encoder-unavailable");
    yvex_model_target_report_add_row(report, "artifact_identity_status: missing");
    yvex_model_target_report_add_row(report, "runtime_descriptor_status: missing");
    yvex_model_target_report_add_row(report, "graph_consumer_status: missing");
    yvex_model_target_report_add_row(report, "backend_residency_status: missing");
    yvex_model_target_report_add_row(
        report,
        "downstream_blockers: family_quantization_plan=missing artifact_emit=missing artifact_identity=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing generation_runtime=missing eval_benchmark=missing");
    yvex_model_target_report_add_row(report, "next_required_rows: %s",
                                     state->next_row);
    yvex_model_target_report_common_tail(report);
}

/*
 * yvex_qtype_policy_report_build()
 *
 * Purpose:
 *   build a typed qtype policy report.
 *
 * Inputs:
 *   request is borrowed; report receives typed rows; err receives invalid
 *   argument failures.
 *
 * Effects:
 *   mutates report only; it does not parse CLI arguments, run quantization,
 *   write artifacts, or render output.
 *
 * Failure:
 *   returns invalid-arg for impossible command routing; typed unsupported
 *   releases are returned through report exit_code.
 *
 * Boundary:
 *   qtype policy reporting is not quantization.
 */
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

    yvex_model_target_report_add_row(report, "qtype-policy: %s [%s]",
                                     report->target_id, state.bracket);
    yvex_model_target_report_add_row(report, "family: %s  mapping_gate: %s",
                                     report->family, state.mapping_gate_status);
    yvex_model_target_report_add_row(
        report, "source_dtype: F32=%lu F16=%lu BF16=%lu other=%lu",
        state.f32_count, state.f16_count, state.bf16_count, state.other_count);
    if (strcmp(state.status, "policy-reported") == 0) {
        qtype_policy_numeric_lists(candidates, refused);
        yvex_model_target_report_add_row(report,
                                         "policy: artifact-planning-storage-policy");
        yvex_model_target_report_add_row(report, "preferred: F16");
        yvex_model_target_report_add_row(report, "candidates: %s",
                                         candidates);
        yvex_model_target_report_add_row(report, "refused: %s", refused);
    }
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     state.top_blocker);
    yvex_model_target_report_add_row(report, "next: %s", state.next_row);
    if (strcmp(state.status, "policy-reported") == 0) {
        yvex_model_target_report_add_row(report,
                                         "boundary: report-only; no quantization/artifact/runtime");
    }
    return YVEX_OK;
}
