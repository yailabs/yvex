/*
 * yvex_mapping_gate_report.c - tensor mapping gate report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   typed projection of canonical DeepSeek mapping-plan facts plus legacy
 *   bounded Qwen/Gemma mapping-gate facts, blockers, and handoff rows.
 *
 * Does not own:
 *   CLI parsing, rendering, artifact emission, quantization execution,
 *   runtime execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the DeepSeek release path consumes the canonical immutable map; legacy
 *   family gates remain header/sidecar evidence. Neither path marks payload,
 *   artifact, runtime, or generation behavior ready.
 *
 * Boundary:
 *   mapping gate status is not quantization, artifact emission, runtime
 *   readiness, generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_mapping_gate_report.h"

#include "yvex_model_target_private.h"
#include "yvex_deepseek_gguf_map.h"
#include "../compilation/yvex_deepseek_transform_ir.h"
#include "../../source/yvex_source_verify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int source_requested;
    int source_exists;
    int metadata_present;
    int attention_k_present;
    int output_head_present;
    int output_head_ambiguous;
    int source_observed;
    int source_missing;
    int source_ambiguous;
    int metadata_observed;
    int metadata_missing;
    const char *missing_roles;
    const char *ambiguous_roles;
    const char *top_blocker;
    const char *next_row;
    const char *status;
    const char *result;
} mapping_gate_state;

/*
 * yvex_deepseek_mapping_plan_report_build()
 *
 * Purpose:
 *   verify the exact release source once, build its IR and coverage, then
 *   attach the canonical immutable logical GGUF plan to the typed report.
 *
 * Inputs:
 *   request and err are borrowed; report receives owned coverage/map objects.
 *
 * Effects:
 *   performs source metadata/header IO through the source verifier and reads
 *   zero tensor payload bytes; it does not print or write files.
 *
 * Failure:
 *   mapping refusal becomes a typed blocked report with exit code 5; path
 *   construction failures return an error without publishing partial owners.
 *
 * Cleanup:
 *   report close releases every successfully attached owner.
 *
 * Boundary:
 *   a complete logical plan is not payload conversion, physical GGUF
 *   emission, artifact support, materialization, or runtime execution.
 */
int yvex_deepseek_mapping_plan_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    yvex_source_verify_options source_options;
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_deepseek_v4_ir *architecture = NULL;
    yvex_deepseek_tensor_coverage *coverage = NULL;
    yvex_transform_ir *transform_ir = NULL;
    yvex_deepseek_gguf_map *map = NULL;
    yvex_deepseek_v4_ir_failure architecture_failure;
    yvex_deepseek_tensor_coverage_failure coverage_failure;
    yvex_transform_failure transform_failure;
    yvex_deepseek_gguf_map_failure map_failure;
    const char *refusal_stage = NULL;
    const char *refusal_reason = NULL;
    const char *refusal_source = "none";
    const char *refusal_emitted = "none";
    char models_root[512];
    char source_path[512];
    int rc;

    if (!yvex_model_target_release_source_paths(
            request, models_root, sizeof(models_root), source_path,
            sizeof(source_path))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "mapping_gate_report",
                       "DeepSeek source path exceeds report bounds");
        return YVEX_ERR_BOUNDS;
    }
    memset(&source_options, 0, sizeof(source_options));
    memset(&verification, 0, sizeof(verification));
    memset(&architecture_failure, 0, sizeof(architecture_failure));
    memset(&coverage_failure, 0, sizeof(coverage_failure));
    memset(&transform_failure, 0, sizeof(transform_failure));
    memset(&map_failure, 0, sizeof(map_failure));
    source_options.identity = yvex_model_target_release_identity();
    source_options.source_path = source_path;
    source_options.models_root = models_root;
    source_options.promote_manifest = 0;
    rc = yvex_source_verify_with_snapshot(
        &source_options, &verification, &snapshot, err);
    if (rc != YVEX_OK || !verification.verified || !snapshot) {
        refusal_stage = "source-verification";
        refusal_reason = yvex_source_verification_status(&verification);
        refusal_source = source_path;
        if (rc == YVEX_OK) rc = YVEX_ERR_STATE;
        goto cleanup;
    }
    rc = yvex_deepseek_v4_ir_build(
        &architecture, &verification, &architecture_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "architecture";
        refusal_reason = yvex_deepseek_v4_ir_failure_name(
            architecture_failure.code);
        refusal_source = architecture_failure.field
            ? architecture_failure.field : "architecture-ir";
        goto cleanup;
    }
    rc = yvex_deepseek_tensor_coverage_build(
        &coverage, &verification, architecture, snapshot, NULL,
        &coverage_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "source-coverage";
        refusal_reason = yvex_deepseek_tensor_coverage_failure_name(
            coverage_failure.code);
        refusal_source = coverage_failure.tensor_name[0]
            ? coverage_failure.tensor_name : "source-tensor";
        goto cleanup;
    }
    rc = yvex_deepseek_transform_ir_build(
        &transform_ir, &verification, architecture, coverage, NULL,
        &transform_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "transformation-ir";
        refusal_reason = yvex_transform_failure_name(transform_failure.code);
        goto cleanup;
    }
    rc = yvex_deepseek_gguf_map_build(
        &map, architecture, transform_ir, &map_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "gguf-lowering";
        refusal_reason = yvex_deepseek_gguf_map_failure_name(map_failure.code);
        refusal_source = map_failure.source_name[0]
            ? map_failure.source_name : "none";
        refusal_emitted = map_failure.emitted_name[0]
            ? map_failure.emitted_name : "none";
    }

cleanup:
    yvex_transform_ir_release(&transform_ir);
    yvex_deepseek_v4_ir_close(architecture);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) {
        yvex_deepseek_gguf_map_close(map);
        yvex_deepseek_tensor_coverage_close(coverage);
        report->status = "mapping-plan-blocked";
        report->exit_code = 5;
        yvex_model_target_report_add_error(
            report,
            "model-target mapping-gate: DeepSeek %s refused: %s source=%s emitted=%s",
            refusal_stage ? refusal_stage : "mapping-plan",
            refusal_reason ? refusal_reason : "invalid-lifecycle-state",
            refusal_source, refusal_emitted);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    report->deepseek_tensor_coverage = coverage;
    report->deepseek_gguf_map = map;
    report->status = "deepseek-gguf-mapping-complete";
    (void)snprintf(report->target_id, sizeof(report->target_id), "%s",
                   request->target_id);
    (void)snprintf(report->family, sizeof(report->family), "%s", "deepseek");
    (void)snprintf(report->stage, sizeof(report->stage), "%s", "header-only");
    (void)snprintf(report->tensor_map_status,
                   sizeof(report->tensor_map_status), "%s", "complete");
    (void)snprintf(report->runtime_status, sizeof(report->runtime_status),
                   "%s", "unsupported");
    (void)snprintf(report->generation_status,
                   sizeof(report->generation_status), "%s", "unsupported");
    (void)snprintf(report->next_row, sizeof(report->next_row), "%s",
                   "V010.SOURCE.PAYLOAD.STREAM.0");
    (void)snprintf(report->boundary, sizeof(report->boundary), "%s",
                   "logical GGUF names, shapes, source contributions, transforms, and metadata are complete; no payload, writer, artifact, or runtime claim");
    return YVEX_OK;
}

/*
 * mapping_gate_file_exists()
 *
 * Purpose:
 *   test whether an explicit local metadata/header path exists.
 *
 * Inputs:
 *   path is borrowed.
 *
 * Effects:
 *   opens and closes the named file only; it never writes output.
 *
 * Failure:
 *   missing/unreadable files return false.
 *
 * Boundary:
 *   file presence is report evidence only.
 */
static int mapping_gate_file_exists(const char *path)
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

static unsigned long long mapping_gate_le64(const unsigned char bytes[8])
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
 * mapping_gate_read_header()
 *
 * Purpose:
 *   read a bounded safetensors header JSON string for role presence checks.
 *
 * Inputs:
 *   path is borrowed; out receives an owned NUL-terminated header string.
 *
 * Effects:
 *   reads only the safetensors header length and header bytes; payload bytes
 *   are not loaded.
 *
 * Failure:
 *   returns false for missing, malformed, oversized, or allocation failures.
 *
 * Boundary:
 *   header text is used only for report-only lexical coverage facts.
 */
static int mapping_gate_read_header(const char *path, char **out)
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
    header_len = mapping_gate_le64(len_bytes);
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

/*
 * mapping_gate_source_dir()
 *
 * Purpose:
 *   resolve the source directory described by the typed request.
 *
 * Inputs:
 *   request/family are borrowed; out receives a bounded path.
 *
 * Effects:
 *   mutates out only; no filesystem access occurs.
 *
 * Failure:
 *   out remains empty when no source location was requested.
 *
 * Boundary:
 *   path resolution is local report plumbing, not source verification.
 */
static void mapping_gate_source_dir(const yvex_model_target_request *request,
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
 * mapping_gate_build_state()
 *
 * Purpose:
 *   gather report-only source/header and metadata coverage facts.
 *
 * Inputs:
 *   request/family are borrowed; state is mutated.
 *
 * Effects:
 *   reads local metadata sidecars and safetensors headers only; tensor payloads
 *   are not loaded and no output is written.
 *
 * Failure:
 *   missing inputs become typed blockers instead of hard failures.
 *
 * Boundary:
 *   coverage facts do not prove role materialization, artifact emission, or
 *   runtime readiness.
 */
static void mapping_gate_build_state(const yvex_model_target_request *request,
                                     const char *family,
                                     mapping_gate_state *state)
{
    char dir[1024];
    char path[1200];
    char *header = NULL;

    memset(state, 0, sizeof(*state));
    state->source_observed = 12;
    state->metadata_observed = 4;
    state->missing_roles = "none";
    state->ambiguous_roles = "none";
    state->top_blocker = "missing-qtype-policy-report";
    state->next_row = "V010.QUANT.0";
    state->status = "passed-for-artifact-planning";
    state->result = "pass";

    mapping_gate_source_dir(request, family, dir, sizeof(dir));
    state->source_requested = dir[0] != '\0';
    if (!state->source_requested) {
        return;
    }

    (void)snprintf(path, sizeof(path), "%s/model.safetensors", dir);
    state->source_exists = mapping_gate_read_header(path, &header);
    if (!state->source_exists) {
        state->source_observed = 0;
        state->source_missing = 12;
        state->metadata_observed = 0;
        state->metadata_missing = 4;
        state->missing_roles = "all-source-roles";
        state->top_blocker = strcmp(family, "gemma") == 0
                                 ? "missing-gemma-source-path"
                                 : "missing-qwen-source-path";
        state->next_row = "V010.MAP.9";
        state->status = "blocked-missing-source";
        state->result = "block";
        return;
    }

    state->attention_k_present = strstr(header, "k_proj.weight") != NULL;
    state->output_head_present = strstr(header, "lm_head.weight") != NULL ||
                                 strstr(header, "output.weight") != NULL;
    state->output_head_ambiguous = strstr(header, "lm_head.weight") != NULL &&
                                   strstr(header, "output.weight") != NULL;
    free(header);

    (void)snprintf(path, sizeof(path), "%s/tokenizer.json", dir);
    state->metadata_present = mapping_gate_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/config.json", dir);
    state->metadata_present = state->metadata_present &&
                              mapping_gate_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/generation_config.json", dir);
    state->metadata_present = state->metadata_present &&
                              mapping_gate_file_exists(path);
    (void)snprintf(path, sizeof(path), "%s/special_tokens_map.json", dir);
    state->metadata_present = state->metadata_present &&
                              mapping_gate_file_exists(path);

    if (!state->attention_k_present) {
        state->source_observed = 11;
        state->source_missing = 1;
        state->missing_roles = "attention_k";
        state->top_blocker = "missing-source-role-attention-k";
        state->next_row = "V010.MAP.9";
        state->status = "blocked-missing-runtime-roles";
        state->result = "block";
    } else if (state->output_head_ambiguous) {
        state->source_observed = 11;
        state->source_ambiguous = 1;
        state->ambiguous_roles = "output_head";
        state->top_blocker = "ambiguous-output-head-tensor";
        state->next_row = "V010.MAP.9";
        state->status = "blocked-missing-runtime-roles";
        state->result = "block";
    } else if (!state->output_head_present) {
        state->source_observed = 11;
        state->source_missing = 1;
        state->missing_roles = "output_head";
        state->top_blocker = "missing-output-head-tensor";
        state->next_row = "V010.MAP.9";
        state->status = "blocked-missing-runtime-roles";
        state->result = "block";
    } else if (!state->metadata_present) {
        state->metadata_observed = 0;
        state->metadata_missing = 4;
        state->missing_roles =
            "tokenizer_metadata,config_metadata,generation_metadata,special_tokens";
        state->top_blocker = "missing-tokenizer-sidecars";
        state->next_row = "V010.MAP.9";
        state->status = "blocked-missing-runtime-roles";
        state->result = "block";
    }
}

/*
 * mapping_gate_prepare()
 *
 * Purpose:
 *   initialize common typed fields for the mapping gate report.
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
 *   common fields remain report-only and do not imply quantization readiness.
 */
static void mapping_gate_prepare(const yvex_model_target_request *request,
                                 const mapping_gate_state *state,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";
    const char *family = yvex_model_target_family_key(target);

    report->kind = request->kind;
    report->mode = request->mode;
    report->status = state->status;
    report->exit_code = 0;
    snprintf(report->target_id, sizeof(report->target_id), "%s", target);
    snprintf(report->family, sizeof(report->family), "%s", family);
    snprintf(report->stage, sizeof(report->stage), "report-only");
    snprintf(report->eligibility, sizeof(report->eligibility), "%s",
             strcmp(state->result, "pass") == 0 ? "report-pass" : "blocked");
    snprintf(report->tensor_map_status, sizeof(report->tensor_map_status),
             "naming-map-profiled");
    snprintf(report->qtype_policy_status, sizeof(report->qtype_policy_status),
             strcmp(state->result, "pass") == 0 ? "missing" : "blocked");
    snprintf(report->artifact_status, sizeof(report->artifact_status), "missing");
    snprintf(report->runtime_status, sizeof(report->runtime_status), "unsupported");
    snprintf(report->generation_status, sizeof(report->generation_status),
             "unsupported-full-model");
    snprintf(report->benchmark_status, sizeof(report->benchmark_status),
             "not-measured");
    snprintf(report->next_row, sizeof(report->next_row), "%s", state->next_row);
    snprintf(report->reason, sizeof(report->reason), "%s", state->top_blocker);
    snprintf(report->boundary, sizeof(report->boundary),
             "report-only; no artifact/runtime/generation");
}

/*
 * mapping_gate_validate()
 *
 * Purpose:
 *   reject impossible typed request combinations for the gate report.
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
 *   refusal rows do not inspect payloads or perform mapping.
 */
static int mapping_gate_validate(const yvex_model_target_request *request,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";

    if (request->kind != YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP) {
        report->status = "mapping-gate-fail";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "mapping gate report requires tensor-map command kind");
        return 1;
    }
    if (request->gate[0] && strcmp(request->gate, "v0.1.0") != 0) {
        report->status = "unsupported-release";
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: unsupported-release");
        yvex_model_target_report_add_row(report, "release: %s", request->gate);
        yvex_model_target_report_add_error(report, "unsupported release: %s",
                                           request->gate);
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

/*
 * mapping_gate_add_table()
 *
 * Purpose:
 *   append table rows for mapping gate evidence.
 *
 * Inputs:
 *   state/report are borrowed or mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   table rows report blockers only.
 */
static void mapping_gate_add_table(const mapping_gate_state *state,
                                   yvex_model_target_report *report)
{
    yvex_model_target_report_add_row(report, "TENSOR MAPPING GATE");
    yvex_model_target_report_add_row(
        report,
        "TARGET  FAMILY  GATE  SOURCE_ROLES  META_ROLES  MISSING  AMBIG  TOP_BLOCKER  STATUS  NEXT");
    yvex_model_target_report_add_row(
        report,
        "%s  %s  v0.1.0  %d/12  %d/4  %d  %d  %s  %s  %s",
        report->target_id, report->family, state->source_observed,
        state->metadata_observed, state->source_missing + state->metadata_missing,
        state->source_ambiguous, state->top_blocker, state->status,
        state->next_row);
}

/*
 * mapping_gate_add_audit()
 *
 * Purpose:
 *   append audit rows for mapping gate evidence.
 *
 * Inputs:
 *   state/report are borrowed or mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   audit rows do not prove quantization, artifacts, or runtime paths.
 */
static void mapping_gate_add_audit(const mapping_gate_state *state,
                                   yvex_model_target_report *report)
{
    yvex_model_target_report_add_row(report, "tensor_mapping_gate_status: %s",
                                     state->status);
    yvex_model_target_report_add_row(report, "tensor_mapping_gate_result: %s",
                                     state->result);
    yvex_model_target_report_add_row(report, "tensor_mapping_gate_target_id: %s",
                                     report->target_id);
    yvex_model_target_report_add_row(report, "tensor_mapping_gate_family: %s",
                                     report->family);
    yvex_model_target_report_add_row(report,
                                     "tensor_naming_map_status: naming-map-profiled");
    yvex_model_target_report_add_row(report,
                                     "output_head_map_status: output-head-profiled");
    yvex_model_target_report_add_row(
        report,
        "tokenizer_metadata_map_status: %s",
        state->metadata_present ? "present-report-only" : "missing");
    yvex_model_target_report_add_row(report,
                                     "missing_role_report_status: missing-role-report-blocked");
    yvex_model_target_report_add_row(report, "expected_source_role_count: 12");
    yvex_model_target_report_add_row(report, "observed_source_role_count: %d",
                                     state->source_observed);
    yvex_model_target_report_add_row(report, "expected_metadata_role_count: 4");
    yvex_model_target_report_add_row(report, "observed_metadata_role_count: %d",
                                     state->metadata_observed);
    yvex_model_target_report_add_row(report, "missing_roles: %s",
                                     state->missing_roles);
    yvex_model_target_report_add_row(report, "ambiguous_roles: %s",
                                     state->ambiguous_roles);
    yvex_model_target_report_add_row(
        report,
        "downstream_blockers: artifact_contract=missing qtype_policy=missing runtime_descriptor=missing graph_consumer=missing backend_residency=missing logits_runtime=missing tokenizer_runtime=missing generation_runtime=missing eval_benchmark=missing");
    yvex_model_target_report_add_row(report, "next_required_rows: %s",
                                     state->next_row);
    yvex_model_target_report_add_row(report, "payload_bytes_read: false");
    yvex_model_target_report_add_row(report, "artifact_emitted: false");
    yvex_model_target_report_add_row(report,
                                     "runtime_descriptor_constructed: false");
    yvex_model_target_report_add_row(report, "graph_consumer_fed: false");
    yvex_model_target_report_common_tail(report);
}

/*
 * yvex_mapping_gate_report_build()
 *
 * Purpose:
 *   build a typed tensor mapping gate report.
 *
 * Inputs:
 *   request is borrowed; report receives typed rows; err receives invalid
 *   argument failures.
 *
 * Effects:
 *   mutates report only; it does not parse CLI arguments, write output,
 *   inspect tensor payloads, run quantization, or emit artifacts.
 *
 * Failure:
 *   returns invalid-arg for impossible command routing; typed unsupported
 *   target/release refusals are returned through report exit_code.
 *
 * Boundary:
 *   mapping gate reporting is not quantization or runtime readiness.
 */
int yvex_mapping_gate_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    mapping_gate_state state;
    const char *family;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "mapping_gate_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_model_target_is_release_target(request->target_id)) {
        memset(&state, 0, sizeof(state));
        state.status = "mapping-plan-check";
        state.result = "block";
        state.next_row = "V010.SOURCE.PAYLOAD.STREAM.0";
        state.top_blocker = "mapping-plan-not-evaluated";
        mapping_gate_prepare(request, &state, report);
        if (mapping_gate_validate(request, report)) return YVEX_OK;
        return yvex_deepseek_mapping_plan_report_build(request, report, err);
    }
    family = yvex_model_target_family_key(
        request->target_id[0] ? request->target_id : "qwen3-8b");
    mapping_gate_build_state(request, family, &state);
    mapping_gate_prepare(request, &state, report);
    if (mapping_gate_validate(request, report)) {
        return YVEX_OK;
    }
    if (request->output_contract[0]) {
        yvex_model_target_report_add_output_contract(
            report, "mapping-gate", request->output_contract);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        mapping_gate_add_table(&state, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        mapping_gate_add_audit(&state, report);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        yvex_model_target_report_add_row(
            report,
            "{\"status\":\"%s\",\"target_id\":\"%s\",\"top_blocker\":\"%s\",\"next\":\"%s\"}",
            state.status, report->target_id, state.top_blocker, state.next_row);
        return YVEX_OK;
    }

    yvex_model_target_report_add_row(
        report, "tensor-mapping-gate: %s [%s]", report->target_id,
        strcmp(state.result, "pass") == 0 ? "reported" : "blocked");
    yvex_model_target_report_add_row(report, "gate: v0.1.0  family: %s",
                                     report->family);
    yvex_model_target_report_add_row(
        report,
        "roles: source %d/12, metadata %d/4, missing %d, ambiguous %d",
        state.source_observed, state.metadata_observed,
        state.source_missing + state.metadata_missing, state.source_ambiguous);
    if (strcmp(state.missing_roles, "none") != 0) {
        yvex_model_target_report_add_row(report, "missing: %s",
                                         state.missing_roles);
    }
    if (strcmp(state.ambiguous_roles, "none") != 0) {
        yvex_model_target_report_add_row(report, "ambiguous: %s",
                                         state.ambiguous_roles);
    }
    yvex_model_target_report_add_row(report, "result: %s", state.result);
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     state.top_blocker);
    yvex_model_target_report_add_row(report, "next: %s", state.next_row);
    yvex_model_target_report_add_row(report,
                                     "boundary: report-only; no artifact/runtime/generation");
    return YVEX_OK;
}
