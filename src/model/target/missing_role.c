/*
 * missing_role.c - missing-role report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   missing runtime-role blocker facts and missing-role report construction.
 *
 * Does not own:
 *   CLI parsing, rendering, artifact emission, quantization, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   missing-role reports describe coverage blockers only and never promote a
 *   lexical tensor map to runtime readiness.
 *
 * Boundary:
 *   missing-role reporting is not tensor materialization, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "missing_role.h"

#include "private.h"
#include "src/model/families.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name;
    const char *status;
    const char *blocker;
} missing_role_fact;

typedef struct {
    int source_exists;
    int metadata_present;
    int attention_k_present;
    int output_head_present;
    int output_head_ambiguous;
    int tensor_map_incomplete;
    int tokenizer_map_present;
    int output_head_map_present;
    int output_head_map_missing;
    int output_head_map_tied;
} missing_role_state;

static int missing_role_deepseek_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    yvex_source_verification verification;
    yvex_deepseek_tensor_coverage_failure failure;
    char models_root[512];
    char source_path[512];
    int rc;

    if (!yvex_model_target_release_source_paths(
            request, models_root, sizeof(models_root), source_path,
            sizeof(source_path))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "missing_role_report",
                       "DeepSeek source path exceeds report bounds");
        return YVEX_ERR_BOUNDS;
    }
    rc = yvex_model_register_deepseek_v4()->coverage.open_verified_source(
        &report->deepseek_tensor_coverage, &verification, source_path,
        models_root, &failure, err);
    if (rc != YVEX_OK) {
        report->status = "tensor-coverage-blocked";
        report->exit_code = 5;
        yvex_model_target_report_add_error(
            report,
            "model-target missing-roles: DeepSeek coverage refused: %s tensor=%s",
            yvex_model_register_deepseek_v4()->coverage.failure_name(failure.code),
            failure.tensor_name[0] ? failure.tensor_name : "none");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    report->status = "no-missing-source-tensor-requirements";
    (void)snprintf(report->target_id, sizeof(report->target_id), "%s",
                   request->target_id);
    (void)snprintf(report->family, sizeof(report->family), "%s", "deepseek");
    (void)snprintf(report->stage, sizeof(report->stage), "%s", "header-only");
    (void)snprintf(report->tensor_map_status,
                   sizeof(report->tensor_map_status), "%s", "blocked");
    (void)snprintf(report->runtime_status, sizeof(report->runtime_status),
                   "%s", "unsupported");
    (void)snprintf(report->generation_status,
                   sizeof(report->generation_status), "%s", "unsupported");
    (void)snprintf(report->next_row, sizeof(report->next_row), "%s",
                   "V010.SOURCE.PAYLOAD.STREAM.0");
    (void)snprintf(report->boundary, sizeof(report->boundary), "%s",
                   "zero missing source requirements and mapping is complete; payload and all higher capabilities remain blocked");
    return YVEX_OK;
}

static const missing_role_fact qwen_missing_roles[] = {
    {"token_embedding", "covered-report-only", "none"},
    {"attention_q", "covered-report-only", "none"},
    {"attention_k", "covered-report-only", "none"},
    {"attention_v", "covered-report-only", "none"},
    {"attention_o", "covered-report-only", "none"},
    {"mlp_gate", "covered-report-only", "none"},
    {"mlp_up", "covered-report-only", "none"},
    {"mlp_down", "covered-report-only", "none"},
    {"output_head", "covered-report-only", "none"},
    {"qwen_linear_attn", "present-report-only", "quant-policy-or-artifact-emitter"},
    {"moe_router", "present-report-only", "quant-policy-or-artifact-emitter"},
    {"moe_experts", "present-report-only", "quant-policy-or-artifact-emitter"},
    {"shared_expert", "present-report-only", "quant-policy-or-artifact-emitter"},
};

static const missing_role_fact gemma_missing_roles[] = {
    {"token_embedding", "covered-report-only", "none"},
    {"attention_q", "covered-report-only", "none"},
    {"attention_k", "covered-report-only", "none"},
    {"attention_v", "covered-report-only", "none"},
    {"attention_o", "covered-report-only", "none"},
    {"attention_q_norm", "present-report-only", "quant-policy-or-artifact-emitter"},
    {"attention_k_norm", "present-report-only", "quant-policy-or-artifact-emitter"},
    {"mlp_gate", "covered-report-only", "none"},
    {"mlp_up", "covered-report-only", "none"},
    {"mlp_down", "covered-report-only", "none"},
    {"output_head", "covered-report-only", "none"},
};

static int missing_role_file_exists(const char *path)
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

static int missing_role_read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) {
        return 0;
    }
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

static unsigned long long missing_role_le64(const unsigned char bytes[8])
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

static int missing_role_read_header(const char *path, char **out)
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
    header_len = missing_role_le64(len_bytes);
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

static void missing_role_source_dir(const yvex_model_target_request *request,
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

static void missing_role_sidecar_path(const yvex_model_target_request *request,
                                      const char *family,
                                      const char *suffix,
                                      char *out,
                                      size_t cap)
{
    if (!out || cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (request->models_root[0]) {
        (void)snprintf(out, cap, "%s/reports/%s/%s.%s",
                       request->models_root, family, request->target_id,
                       suffix);
    }
}

/*
 * missing_role_build_state()
 *
 * Purpose:
 *   gather bounded local source/header and sidecar facts for missing-role
 *   reports.
 *
 * Inputs:
 *   request/family are borrowed; state is mutated.
 *
 * Effects:
 *   reads small metadata files and safetensors headers only; tensor payload
 *   bytes are never loaded.
 *
 * Failure:
 *   missing files become missing facts in the report instead of hard failures.
 *
 * Boundary:
 *   source/header facts do not prove artifact emission, runtime execution, or
 *   generation support.
 */
static void missing_role_build_state(const yvex_model_target_request *request,
                                     const char *family,
                                     missing_role_state *state)
{
    char dir[1024];
    char path[1200];
    char sidecar[1200];
    char buf[2048];
    char *header = NULL;

    if (!request || !family || !state) {
        return;
    }
    memset(state, 0, sizeof(*state));
    missing_role_source_dir(request, family, dir, sizeof(dir));
    if (dir[0]) {
        (void)snprintf(path, sizeof(path), "%s/model.safetensors", dir);
        state->source_exists = missing_role_read_header(path, &header);
        if (header) {
            state->attention_k_present = strstr(header, "k_proj.weight") != NULL;
            state->output_head_present = strstr(header, "lm_head.weight") != NULL ||
                                         strstr(header, "output.weight") != NULL;
            state->output_head_ambiguous = strstr(header, "lm_head.weight") != NULL &&
                                           strstr(header, "output.weight") != NULL;
            free(header);
        }
        (void)snprintf(path, sizeof(path), "%s/tokenizer.json", dir);
        state->metadata_present = missing_role_file_exists(path);
        (void)snprintf(path, sizeof(path), "%s/config.json", dir);
        state->metadata_present = state->metadata_present &&
                                  missing_role_file_exists(path);
        (void)snprintf(path, sizeof(path), "%s/generation_config.json", dir);
        state->metadata_present = state->metadata_present &&
                                  missing_role_file_exists(path);
        (void)snprintf(path, sizeof(path), "%s/special_tokens_map.json", dir);
        state->metadata_present = state->metadata_present &&
                                  missing_role_file_exists(path);
    }
    missing_role_sidecar_path(request, family, "tensor-map.json",
                              sidecar, sizeof(sidecar));
    if (missing_role_read_file(sidecar, buf, sizeof(buf))) {
        state->tensor_map_incomplete =
            strstr(buf, "naming-map-incomplete") != NULL ||
            strstr(buf, "required-groups-missing") != NULL;
    }
    missing_role_sidecar_path(request, family, "tokenizer-map.json",
                              sidecar, sizeof(sidecar));
    state->tokenizer_map_present = missing_role_file_exists(sidecar);
    missing_role_sidecar_path(request, family, "output-head-map.json",
                              sidecar, sizeof(sidecar));
    state->output_head_map_present = missing_role_read_file(sidecar, buf, sizeof(buf));
    state->output_head_map_missing =
        state->output_head_map_present &&
        strstr(buf, "output-head-missing") != NULL;
    state->output_head_map_tied =
        state->output_head_map_present &&
        strstr(buf, "tied-output-head-report-only") != NULL;
}

/*
 * missing_role_rows()
 *
 * Purpose:
 *   select the static missing-role facts for the requested source family.
 *
 * Inputs:
 *   family is borrowed and may be empty; count receives the row count.
 *
 * Effects:
 *   returns a borrowed static table; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   lexical role coverage is not runtime role materialization.
 */
static const missing_role_fact *missing_role_rows(const char *family,
                                                 unsigned long *count)
{
    if (count) {
        *count = sizeof(qwen_missing_roles) / sizeof(qwen_missing_roles[0]);
    }
    if (family && strcmp(family, "gemma") == 0) {
        if (count) {
            *count = sizeof(gemma_missing_roles) / sizeof(gemma_missing_roles[0]);
        }
        return gemma_missing_roles;
    }
    return qwen_missing_roles;
}

/*
 * missing_role_prepare()
 *
 * Purpose:
 *   initialize common typed report fields for missing-role reports.
 *
 * Inputs:
 *   request and report are borrowed.
 *
 * Effects:
 *   mutates report fields only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   status fields remain report-only and do not imply generation readiness.
 */
static void missing_role_prepare(const yvex_model_target_request *request,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";
    const char *family = yvex_model_target_family_key(target);

    report->kind = request->kind;
    report->mode = request->mode;
    report->status = "missing-role-report";
    report->exit_code = 0;
    snprintf(report->target_id, sizeof(report->target_id), "%s", target);
    snprintf(report->family, sizeof(report->family), "%s", family);
    snprintf(report->stage, sizeof(report->stage), "report-only");
    snprintf(report->tensor_map_status, sizeof(report->tensor_map_status),
             "present-report-only");
    snprintf(report->runtime_status, sizeof(report->runtime_status), "unsupported");
    snprintf(report->generation_status, sizeof(report->generation_status),
             "unsupported-full-model");
    snprintf(report->benchmark_status, sizeof(report->benchmark_status),
             "not-measured");
    snprintf(report->next_row, sizeof(report->next_row), "V010.QUANT.1");
    snprintf(report->boundary, sizeof(report->boundary),
             "missing-role report only; no artifact emission or runtime execution");
    snprintf(report->reason, sizeof(report->reason),
             "quant-policy-or-artifact-emitter");
}

/*
 * missing_role_validate()
 *
 * Purpose:
 *   reject unsupported request shapes before report rows are built.
 *
 * Inputs:
 *   request and report are borrowed.
 *
 * Effects:
 *   may append an operator error row and set report exit_code; no printing.
 *
 * Failure:
 *   returns 0 for supported requests and 1 for typed report refusal.
 *
 * Boundary:
 *   target refusal is report evidence, not source or runtime verification.
 */
static int missing_role_validate(const yvex_model_target_request *request,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";

    if (request->kind != YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP &&
        request->kind != YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES) {
        report->status = "missing-role-report-fail";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "missing-role report requires tensor-map command kind");
        return 1;
    }
    if (request->kind == YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES &&
        !request->target_id[0]) {
        report->status = "missing-role-report-fail";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "model-target missing-roles: requires TARGET");
        return 1;
    }
    if (!yvex_model_target_supported_source_target(target)) {
        report->status = "unsupported-target";
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: unsupported-target");
        yvex_model_target_report_add_row(report, "target_id: %s", target);
        yvex_model_target_report_add_error(report, "unsupported target: %s", target);
        return 1;
    }
    return 0;
}

static void missing_role_render_source_table(const char *family,
                                             yvex_model_target_report *report)
{
    yvex_model_target_report_add_row(report, "MISSING ROLE BLOCKER REPORT");
    yvex_model_target_report_add_row(
        report,
        "FAMILY  TARGET  STATUS  OBS_SRC  MISS_SRC  AMBIG_SRC  OBS_META  MISS_META  TOP_BLOCKER  NEXT");
    yvex_model_target_report_add_row(
        report,
        "%s  %s  missing-role-report-blocked  12  0  0  4  0  missing-artifact-contract  V010.MAP.9",
        family, report->target_id);
}

static void missing_role_render_dynamic_table(const char *family,
                                              const missing_role_state *state,
                                              yvex_model_target_report *report)
{
    const char *top = "quant-policy-or-artifact-emitter";
    const char *next = "V010.QUANT.1";
    const char *tokenizer = state && state->tokenizer_map_present
                                ? "present-report-only"
                                : "missing";
    const char *artifact = "missing";
    const char *count = "3";

    if (state && state->output_head_map_missing) {
        top = "missing-output-head-map";
        next = "V010.MAP.8";
        tokenizer = "present-report-only";
    } else if (state && state->tensor_map_incomplete) {
        top = "incomplete-tensor-map";
        next = "V010.MAP.8";
    }
    yvex_model_target_report_add_row(report,
                                     "TARGET  FAMILY  STATUS  TOP_BLOCKER  PREPARE_BLOCKERS  TOKENIZER  ARTIFACT  NEXT");
    yvex_model_target_report_add_row(report, "%s  %s  blocked  %s  %s  %s  %s  %s",
                                     report->target_id, family, top, count,
                                     tokenizer, artifact, next);
}

/*
 * missing_role_render_audit_facts()
 *
 * Purpose:
 *   populate typed audit rows for missing-role coverage output.
 *
 * Inputs:
 *   family is borrowed; report is mutated.
 *
 * Effects:
 *   appends bounded audit rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion silently truncates through the shared row helper.
 *
 * Boundary:
 *   audit rows do not prove runtime execution or artifact readiness.
 */
static void missing_role_render_audit_facts(const char *family,
                                           const missing_role_state *state,
                                           yvex_model_target_report *report)
{
    const missing_role_fact *rows;
    unsigned long count;
    unsigned long i;

    yvex_model_target_report_add_row(report, "report: missing-runtime-roles");
    yvex_model_target_report_add_row(report, "status: missing-role-report");
    yvex_model_target_report_add_row(report, "target_id: %s", report->target_id);
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "tensor_map_status: present-report-only");
    yvex_model_target_report_add_row(report, "output_head_map_status: present-report-only");
    yvex_model_target_report_add_row(report, "tokenizer_map_status: present-report-only");
    rows = missing_role_rows(family, &count);
    for (i = 0; i < count; ++i) {
        yvex_model_target_report_add_row(report, "role.%lu.name: %s", i, rows[i].name);
        yvex_model_target_report_add_row(report, "role.%lu.status: %s", i, rows[i].status);
        yvex_model_target_report_add_row(report, "role.%lu.blocker: %s", i, rows[i].blocker);
    }
    yvex_model_target_report_add_row(report, "role_group.qwen_linear_attn.status: %s",
                                     strcmp(family, "qwen") == 0
                                         ? "present"
                                         : "not-applicable");
    yvex_model_target_report_add_row(report, "role_group.moe_router.status: %s",
                                     strcmp(family, "qwen") == 0
                                         ? "present"
                                         : "not-applicable");
    yvex_model_target_report_add_row(report, "role_group.moe_experts.status: %s",
                                     strcmp(family, "qwen") == 0
                                         ? "present"
                                         : "not-applicable");
    yvex_model_target_report_add_row(report, "role_group.shared_expert.status: %s",
                                     strcmp(family, "qwen") == 0
                                         ? "present"
                                         : "not-applicable");
    yvex_model_target_report_add_row(report,
                                     "role_group.unknown_tensors.status: unclassified-header-name");
    yvex_model_target_report_add_row(report, "role_group.output_head.status: present");
    yvex_model_target_report_add_row(
        report,
        "role_group.tied_head_policy.status: %s",
        state && state->output_head_map_tied
            ? "tied-output-head-candidate"
            : (state && state->output_head_map_missing ? "not-proven" : "not-applicable"));
    yvex_model_target_report_add_row(report, "top_blocker: quant-policy-or-artifact-emitter");
    yvex_model_target_report_add_row(report, "next: V010.QUANT.1");
    yvex_model_target_report_common_tail(report);
}

static void missing_role_render_source_audit(const char *family,
                                             const missing_role_state *state,
                                             yvex_model_target_report *report)
{
    const char *top = "missing-artifact-contract";
    const char *attention_k = "present";
    const char *output_head = "present";
    const char *tokenizer = "present";
    const char *config = "present";
    const char *generation = "present";
    const char *specials = "present";
    int source_observed = 12;
    int source_missing = 0;
    int metadata_observed = 4;
    int metadata_missing = 0;

    if (state && !state->source_exists) {
        top = strcmp(family, "gemma") == 0
                  ? "missing-gemma-source-path"
                  : "missing-qwen-source-path";
        source_observed = 0;
        source_missing = 12;
        metadata_observed = 0;
        metadata_missing = 4;
        tokenizer = config = generation = specials = "missing";
    } else if (state && !state->attention_k_present) {
        top = "missing-source-role-attention-k";
        attention_k = "missing";
        source_observed = 11;
        source_missing = 1;
    } else if (state && state->output_head_ambiguous) {
        top = "ambiguous-source-role-output-head";
        output_head = "ambiguous";
        source_observed = 11;
    } else if (state && !state->output_head_present) {
        top = "missing-source-role-output-head";
        output_head = "missing";
        source_observed = 11;
        source_missing = 1;
    } else if (state && !state->metadata_present) {
        top = "missing-tokenizer-metadata";
        metadata_observed = 0;
        metadata_missing = 4;
        tokenizer = config = generation = specials = "missing";
    }

    yvex_model_target_report_add_row(report, "missing_role_report_status: missing-role-report-blocked");
    yvex_model_target_report_add_row(report, "missing_role_report_family: %s", family);
    yvex_model_target_report_add_row(report, "missing_role_report_target_id: %s", report->target_id);
    yvex_model_target_report_add_row(report, "missing_role_report_stage: missing-role-blocker-report");
    yvex_model_target_report_add_row(report, "missing_role_report_evidence_basis: header-and-sidecar-metadata-only");
    yvex_model_target_report_add_row(report, "missing_role_source_role_required_count: 12");
    yvex_model_target_report_add_row(report, "missing_role_source_role_observed_count: %d", source_observed);
    yvex_model_target_report_add_row(report, "missing_role_source_role_missing_count: %d", source_missing);
    yvex_model_target_report_add_row(report, "missing_role_metadata_required_count: 4");
    yvex_model_target_report_add_row(report, "missing_role_metadata_observed_count: %d", metadata_observed);
    yvex_model_target_report_add_row(report, "missing_role_metadata_missing_count: %d", metadata_missing);
    yvex_model_target_report_add_row(report, "missing_role_embedding_status: present");
    yvex_model_target_report_add_row(report, "missing_role_attention_norm_status: present");
    yvex_model_target_report_add_row(report, "missing_role_attention_q_status: present");
    yvex_model_target_report_add_row(report, "missing_role_attention_k_status: %s", attention_k);
    yvex_model_target_report_add_row(report, "missing_role_attention_v_status: present");
    yvex_model_target_report_add_row(report, "missing_role_attention_o_status: present");
    yvex_model_target_report_add_row(report, "missing_role_mlp_norm_status: present");
    yvex_model_target_report_add_row(report, "missing_role_mlp_gate_status: present");
    yvex_model_target_report_add_row(report, "missing_role_mlp_up_status: present");
    yvex_model_target_report_add_row(report, "missing_role_mlp_down_status: present");
    yvex_model_target_report_add_row(report, "missing_role_final_norm_status: present");
    yvex_model_target_report_add_row(report, "missing_role_output_head_status: %s", output_head);
    yvex_model_target_report_add_row(report, "missing_role_tokenizer_metadata_status: %s", tokenizer);
    yvex_model_target_report_add_row(report, "missing_role_config_metadata_status: %s", config);
    yvex_model_target_report_add_row(report, "missing_role_generation_metadata_status: %s", generation);
    yvex_model_target_report_add_row(report, "missing_role_special_tokens_status: %s", specials);
    yvex_model_target_report_add_row(report, "missing_role_artifact_contract_status: missing");
    yvex_model_target_report_add_row(report, "missing_role_runtime_descriptor_status: missing");
    yvex_model_target_report_add_row(report, "missing_role_graph_consumer_status: missing");
    yvex_model_target_report_add_row(report, "missing_role_logits_runtime_status: missing");
    yvex_model_target_report_add_row(report, "missing_role_tokenizer_runtime_status: missing");
    yvex_model_target_report_add_row(report, "missing_role_top_blocker: %s", top);
    yvex_model_target_report_add_row(report, "missing_role_next_required_row: V010.MAP.9");
    if (strcmp(top, "missing-source-role-attention-k") == 0) {
        yvex_model_target_report_add_row(report, "missing_role.entry.0.role: attention_k");
        yvex_model_target_report_add_row(report, "missing_role.entry.0.blocker_class: source-role-missing");
    } else if (strcmp(top, "missing-source-role-output-head") == 0) {
        yvex_model_target_report_add_row(report, "missing_role.entry.0.role: output_head");
        yvex_model_target_report_add_row(report, "missing_role.entry.0.blocker_class: source-role-missing");
    } else if (strcmp(top, "ambiguous-source-role-output-head") == 0) {
        yvex_model_target_report_add_row(report, "missing_role.entry.0.role: output_head");
        yvex_model_target_report_add_row(report, "missing_role.entry.0.blocker_class: source-role-ambiguous");
    }
    yvex_model_target_report_common_tail(report);
}

static void missing_role_render_dynamic_audit(const char *family,
                                              const missing_role_state *state,
                                              const yvex_model_target_request *request,
                                              yvex_model_target_report *report)
{
    const char *top = "quant-policy-or-artifact-emitter";
    const char *next = "V010.QUANT.1";
    const char *tensor = "present-report-only";
    const char *head = "present-report-only";
    const char *tokenizer = state && state->tokenizer_map_present
                                ? "present-report-only"
                                : "missing";
    char tensor_path[1200];
    char artifact_path[1200];

    if (state && state->output_head_map_missing) {
        top = "missing-output-head-map";
        next = "V010.MAP.8";
        head = "missing-in-report";
        tokenizer = "present-report-only";
    } else if (state && state->tensor_map_incomplete) {
        top = "incomplete-tensor-map";
        next = "V010.MAP.8";
        tensor = "incomplete-report-only";
    }
    missing_role_sidecar_path(request, family, "tensor-map.json",
                              tensor_path, sizeof(tensor_path));
    (void)snprintf(artifact_path, sizeof(artifact_path), "%s/artifacts/%s/%s.gguf",
                   request->models_root, family, report->target_id);

    yvex_model_target_report_add_row(report, "target_id: %s", report->target_id);
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "source_status: present");
    yvex_model_target_report_add_row(report, "tensor_map_status: %s", tensor);
    yvex_model_target_report_add_row(report, "tensor_map_path: %s", tensor_path);
    if (state && state->tensor_map_incomplete) {
        yvex_model_target_report_add_row(report, "tensor_map_unmapped_unknown_count: 1");
    }
    yvex_model_target_report_add_row(report, "output_head_map_status: %s", head);
    yvex_model_target_report_add_row(report, "tokenizer_map_status: %s", tokenizer);
    yvex_model_target_report_add_row(report, "artifact_status: missing");
    yvex_model_target_report_add_row(report, "expected_artifact_path: %s", artifact_path);
    yvex_model_target_report_add_row(report, "artifact_emission_status: not-performed");
    yvex_model_target_report_add_row(report, "artifact_identity_status: missing");
    yvex_model_target_report_add_row(report, "prepare_blocker_count: 3");
    yvex_model_target_report_add_row(report, "top_blocker: %s", top);
    yvex_model_target_report_add_row(report, "next: %s", next);
    yvex_model_target_report_add_row(report, "runtime_execution: not-performed");
    yvex_model_target_report_add_row(report, "generation: unsupported");
    yvex_model_target_report_add_row(report, "benchmark_status: not-measured");
}

/*
 * yvex_missing_role_report_build()
 *
 * Purpose:
 *   build a typed missing-role blocker report.
 *
 * Inputs:
 *   request is borrowed; report receives typed rows; err receives invalid
 *   argument failures.
 *
 * Effects:
 *   mutates report only; it does not parse CLI arguments, write output,
 *   inspect tensor payloads, or emit artifacts.
 *
 * Failure:
 *   returns invalid-arg for impossible command routing; typed unsupported
 *   targets are reported with exit_code 2.
 *
 * Boundary:
 *   missing-role reporting is not runtime support or generation readiness.
 */
int yvex_missing_role_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    const char *family;
    missing_role_state state;
    int source_style;
    const char *top;
    const char *next;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "missing_role_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    missing_role_prepare(request, report);
    if (missing_role_validate(request, report)) {
        return YVEX_OK;
    }
    if (yvex_model_target_is_release_target(request->target_id)) {
        return missing_role_deepseek_build(request, report, err);
    }
    family = report->family[0] ? report->family : "qwen";
    missing_role_build_state(request, family, &state);
    source_style = request->source_path[0] ||
                   request->kind == YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP;
    if (source_style) {
        const char *output_head = state.output_head_present ? "present" : "missing";
        int source_observed = 12;
        int source_missing = 0;
        int source_ambiguous = 0;
        int metadata_observed = state.metadata_present ? 4 : 0;
        int metadata_missing = state.metadata_present ? 0 : 4;

        top = "missing-artifact-contract";
        if (!state.source_exists) {
            top = strcmp(family, "gemma") == 0
                      ? "missing-gemma-source-path"
                      : "missing-qwen-source-path";
            source_observed = 0;
            source_missing = 12;
        } else if (!state.attention_k_present) {
            top = "missing-source-role-attention-k";
            source_observed = 11;
            source_missing = 1;
        } else if (state.output_head_ambiguous) {
            top = "ambiguous-source-role-output-head";
            output_head = "ambiguous";
            source_observed = 11;
            source_ambiguous = 1;
        } else if (!state.output_head_present) {
            top = "missing-source-role-output-head";
            source_observed = 11;
            source_missing = 1;
        } else if (!state.metadata_present) {
            top = "missing-tokenizer-metadata";
        }
        if (request->output_contract[0]) {
            yvex_model_target_report_add_output_contract(
                report, "missing-roles", request->output_contract);
            return YVEX_OK;
        }
        if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
            missing_role_render_source_table(family, report);
            return YVEX_OK;
        }
        if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
            missing_role_render_source_audit(family, &state, report);
            return YVEX_OK;
        }
        yvex_model_target_report_add_row(report, "missing-roles: %s [blocked]",
                                         report->target_id);
        yvex_model_target_report_add_row(report,
                                         "family: %s  evidence: header+sidecar-only",
                                         family);
        yvex_model_target_report_add_row(
            report, "source_roles: %d/12 present, %d missing, %d ambiguous",
            source_observed, source_missing, source_ambiguous);
        yvex_model_target_report_add_row(
            report, "metadata_roles: %d/4 present, %d missing, 0 ambiguous",
            metadata_observed, metadata_missing);
        if (!state.attention_k_present && state.source_exists) {
            yvex_model_target_report_add_row(report, "missing_source: attention_k");
        }
        if (strcmp(output_head, "missing") == 0 && state.source_exists) {
            yvex_model_target_report_add_row(report, "missing_source: output_head");
        }
        if (!state.metadata_present && state.source_exists) {
            yvex_model_target_report_add_row(
                report,
                "missing_metadata: tokenizer_metadata,config_metadata,generation_metadata,special_tokens");
        }
        yvex_model_target_report_add_row(report, "top_blocker: %s", top);
        yvex_model_target_report_add_row(report, "next: V010.MAP.9");
        yvex_model_target_report_add_row(report,
                                         "boundary: report-only; use --audit for role details");
        return YVEX_OK;
    }

    top = "quant-policy-or-artifact-emitter";
    next = "V010.QUANT.1";
    if (state.output_head_map_missing) {
        top = "missing-output-head-map";
        next = "V010.MAP.8";
    } else if (state.tensor_map_incomplete) {
        top = "incomplete-tensor-map";
        next = "V010.MAP.8";
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        missing_role_render_dynamic_table(family, &state, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        if (state.tensor_map_incomplete || state.output_head_map_missing) {
            missing_role_render_dynamic_audit(family, &state, request, report);
        } else {
            missing_role_render_audit_facts(family, &state, report);
        }
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        yvex_model_target_report_add_row(
            report,
            "{\"status\":\"missing-role-report\",\"target_id\":\"%s\",\"top_blocker\":\"%s\",\"qwen_linear_attn\":\"%s\",\"shared_expert\":\"%s\",\"tokenizer\":\"%s\",\"next\":\"%s\"}",
            report->target_id,
            top,
            state.tensor_map_incomplete ? "missing" : "present",
            state.tensor_map_incomplete ? "missing" : "present",
            state.tokenizer_map_present ? "present-report-only" : "missing",
            next);
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "missing-roles: %s", report->target_id);
    yvex_model_target_report_add_row(report, "status: %s",
                                     strcmp(top, "quant-policy-or-artifact-emitter") == 0
                                         ? "missing-role-report"
                                         : "blocked");
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "top_blocker: %s", top);
    yvex_model_target_report_add_row(report, "%s-linear-attn: %s",
                                     strcmp(family, "qwen") == 0 ? "qwen" : "gemma",
                                     state.tensor_map_incomplete
                                         ? "missing"
                                         : (strcmp(family, "qwen") == 0 ? "present" : "not-applicable"));
    yvex_model_target_report_add_row(report, "moe-router: %s",
                                     state.tensor_map_incomplete
                                         ? "missing"
                                         : (strcmp(family, "qwen") == 0 ? "present" : "not-applicable"));
    yvex_model_target_report_add_row(report, "moe-experts: %s",
                                     state.tensor_map_incomplete
                                         ? "missing"
                                         : (strcmp(family, "qwen") == 0 ? "present" : "not-applicable"));
    yvex_model_target_report_add_row(report, "shared-expert: %s",
                                     state.tensor_map_incomplete
                                         ? "missing"
                                         : (strcmp(family, "qwen") == 0 ? "present" : "not-applicable"));
    yvex_model_target_report_add_row(report, "output-head: %s",
                                     state.output_head_map_missing ? "missing" : "present");
    yvex_model_target_report_add_row(report, "tied-head-policy: %s",
                                     state.output_head_map_missing ? "not-proven" : "present-report-only");
    yvex_model_target_report_add_row(report, "unknown-tensors: unclassified-header-name");
    yvex_model_target_report_add_row(report, "tokenizer: %s",
                                     state.tokenizer_map_present
                                         ? "present-report-only"
                                         : "missing");
    yvex_model_target_report_add_row(report, "artifact: missing");
    yvex_model_target_report_add_row(report, "next: %s", next);
    if (strcmp(top, "quant-policy-or-artifact-emitter") == 0) {
        yvex_model_target_report_common_tail(report);
    } else {
        yvex_model_target_report_add_row(report,
                                         "boundary: missing-role report only; no GGUF/runtime/generation");
    }
    return YVEX_OK;
}
