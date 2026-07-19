/* Owner: src/model/target
 * Owns: typed projection of canonical DeepSeek mapping-plan facts plus legacy bounded Qwen/Gemma mapping-gate
 *   facts, blockers, and handoff rows.
 * Does not own: CLI parsing, rendering, artifact emission, quantization execution, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 * Invariants: the DeepSeek release path consumes the canonical immutable map; legacy family gates remain
 *   header/sidecar evidence. Neither path marks payload, artifact, runtime, or generation behavior ready.
 * Boundary: mapping gate status is not quantization, artifact emission, runtime readiness, generation readiness,
 *   benchmark evidence, or release readiness.
 * Purpose: evaluate typed mapping-gate evidence from bounded source facts.
 * Inputs: typed target requests and retained report facts.
 * Effects: reads bounded metadata evidence and updates report state.
 * Failure: missing or ambiguous evidence remains an explicit blocker. */
#include <yvex/internal/model_target.h>

#include <yvex/internal/compilation.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/source.h>

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

typedef struct {
    const char *status;
    const char *result;
    const char *target;
    const char *family;
    const char *metadata;
    const char *missing;
    const char *ambiguous;
    const char *next;
    int source_observed;
    int metadata_observed;
} mapping_gate_audit_facts;

#define MAPPING_LITERAL(text) \
    { YVEX_MODEL_TARGET_ROW_LITERAL, (text), 0u }
#define MAPPING_STRING(field, format) \
    { YVEX_MODEL_TARGET_ROW_STRING, (format), offsetof(mapping_gate_audit_facts, field) }
#define MAPPING_INT(field, format) \
    { YVEX_MODEL_TARGET_ROW_INT, (format), offsetof(mapping_gate_audit_facts, field) }

static const yvex_model_target_row_spec mapping_gate_audit_rows[] = {
    MAPPING_STRING(status, "tensor_mapping_gate_status: %s"),
    MAPPING_STRING(result, "tensor_mapping_gate_result: %s"),
    MAPPING_STRING(target, "tensor_mapping_gate_target_id: %s"),
    MAPPING_STRING(family, "tensor_mapping_gate_family: %s"),
    MAPPING_LITERAL("tensor_naming_map_status: naming-map-profiled"),
    MAPPING_LITERAL("output_head_map_status: output-head-profiled"),
    MAPPING_STRING(metadata, "tokenizer_metadata_map_status: %s"),
    MAPPING_LITERAL("missing_role_report_status: missing-role-report-blocked"),
    MAPPING_LITERAL("expected_source_role_count: 12"),
    MAPPING_INT(source_observed, "observed_source_role_count: %d"),
    MAPPING_LITERAL("expected_metadata_role_count: 4"),
    MAPPING_INT(metadata_observed, "observed_metadata_role_count: %d"),
    MAPPING_STRING(missing, "missing_roles: %s"),
    MAPPING_STRING(ambiguous, "ambiguous_roles: %s"),
    MAPPING_LITERAL(
        "downstream_blockers: artifact_contract=missing qtype_policy=missing "
        "runtime_descriptor=missing graph_consumer=missing backend_residency=missing "
        "logits_runtime=missing tokenizer_runtime=missing generation_runtime=missing "
        "eval_benchmark=missing"),
    MAPPING_STRING(next, "next_required_rows: %s"),
    MAPPING_LITERAL("payload_bytes_read: false"),
    MAPPING_LITERAL("artifact_emitted: false"),
    MAPPING_LITERAL("runtime_descriptor_constructed: false"),
    MAPPING_LITERAL("graph_consumer_fed: false")
};

#undef MAPPING_LITERAL
#undef MAPPING_STRING
#undef MAPPING_INT

/*
 * yvex_model_mapping_report_deepseek()
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
 *   emission, artifact support, materialization, or runtime execution. */
int yvex_model_mapping_report_deepseek(
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
    source_options.identity = yvex_source_release_identity();
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
    rc = yvex_model_register_deepseek_v4()->ir.build(
        &architecture, &verification, &architecture_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "architecture";
        refusal_reason = yvex_model_register_deepseek_v4()->ir.failure_name(
            architecture_failure.code);
        refusal_source = architecture_failure.field
            ? architecture_failure.field : "architecture-ir";
        goto cleanup;
    }
    rc = yvex_model_register_deepseek_v4()->coverage.build(
        &coverage, &verification, architecture, snapshot, NULL,
        &coverage_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "source-coverage";
        refusal_reason = yvex_model_register_deepseek_v4()->coverage.failure_name(
            coverage_failure.code);
        refusal_source = coverage_failure.tensor_name[0]
            ? coverage_failure.tensor_name : "source-tensor";
        goto cleanup;
    }
    rc = yvex_model_register_deepseek_v4()->transform.build(
        &transform_ir, &verification, architecture, coverage, NULL,
        &transform_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "transformation-ir";
        refusal_reason = yvex_transform_failure_name(transform_failure.code);
        goto cleanup;
    }
    rc = yvex_model_register_deepseek_v4()->lowering.build(
        &map, architecture, transform_ir, &map_failure, err);
    if (rc != YVEX_OK) {
        refusal_stage = "gguf-lowering";
        refusal_reason = yvex_model_register_deepseek_v4()->lowering.failure_name(map_failure.code);
        refusal_source = map_failure.source_name[0]
            ? map_failure.source_name : "none";
        refusal_emitted = map_failure.emitted_name[0]
            ? map_failure.emitted_name : "none";
    }

cleanup:
    yvex_transform_ir_release(&transform_ir);
    yvex_model_register_deepseek_v4()->ir.close(architecture);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) {
        yvex_model_register_deepseek_v4()->lowering.close(map);
        yvex_model_register_deepseek_v4()->coverage.close(coverage);
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
    report->family_coverage = coverage;
    report->family_lowering = map;
    {
        const yvex_model_target_report_profile profile = {
            .status = "deepseek-gguf-mapping-complete",
            .target_id = request->target_id, .family = "deepseek", .stage = "header-only",
            .tensor_map_status = "complete", .runtime_status = "unsupported",
            .generation_status = "unsupported", .next_row = "V010.SOURCE.PAYLOAD.STREAM.0",
            .boundary = "logical GGUF names, shapes, source contributions, transforms, "
                        "and metadata are complete; no payload, writer, artifact, or runtime claim"
        };

        yvex_model_target_report_prepare(report, request, &profile);
    }
    return YVEX_OK;
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
 *   runtime readiness. */
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

    (void)yvex_model_target_probe_source_path(
        request, family, NULL, dir, sizeof(dir));
    state->source_requested = dir[0] != '\0';
    if (!state->source_requested) {
        return;
    }

    (void)snprintf(path, sizeof(path), "%s/model.safetensors", dir);
    state->source_exists = yvex_model_target_probe_header(path, &header);
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
    state->metadata_present = yvex_model_target_probe_file(path);
    (void)snprintf(path, sizeof(path), "%s/config.json", dir);
    state->metadata_present = state->metadata_present &&
                              yvex_model_target_probe_file(path);
    (void)snprintf(path, sizeof(path), "%s/generation_config.json", dir);
    state->metadata_present = state->metadata_present &&
                              yvex_model_target_probe_file(path);
    (void)snprintf(path, sizeof(path), "%s/special_tokens_map.json", dir);
    state->metadata_present = state->metadata_present &&
                              yvex_model_target_probe_file(path);

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
 *   common fields remain report-only and do not imply quantization readiness. */
static void mapping_gate_prepare(const yvex_model_target_request *request,
                                 const mapping_gate_state *state,
                                 yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";
    const char *family = yvex_model_target_family_key(target);
    const yvex_model_target_report_profile profile = {
        .status = state->status, .target_id = target, .family = family,
        .stage = "report-only",
        .eligibility = strcmp(state->result, "pass") == 0 ? "report-pass" : "blocked",
        .artifact_status = "missing", .tensor_map_status = "naming-map-profiled",
        .qtype_policy_status = strcmp(state->result, "pass") == 0 ? "missing" : "blocked",
        .runtime_status = "unsupported", .generation_status = "unsupported-full-model",
        .benchmark_status = "not-measured", .next_row = state->next_row,
        .boundary = "report-only; no artifact/runtime/generation",
        .reason = state->top_blocker
    };

    yvex_model_target_report_prepare(report, request, &profile);
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
 *   refusal rows do not inspect payloads or perform mapping. */
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
 *   table rows report blockers only. */
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
 *   audit rows do not prove quantization, artifacts, or runtime paths. */
static void mapping_gate_add_audit(const mapping_gate_state *state,
                                   yvex_model_target_report *report)
{
    mapping_gate_audit_facts facts = {
        state->status, state->result, report->target_id, report->family,
        state->metadata_present ? "present-report-only" : "missing",
        state->missing_roles, state->ambiguous_roles, state->next_row,
        state->source_observed, state->metadata_observed
    };

    yvex_model_target_report_project_rows(
        report, mapping_gate_audit_rows,
        sizeof(mapping_gate_audit_rows) / sizeof(mapping_gate_audit_rows[0]),
        &facts);
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
 *   mapping gate reporting is not quantization or runtime readiness. */
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
    if (yvex_source_is_release_target(request->target_id)) {
        memset(&state, 0, sizeof(state));
        state.status = "mapping-plan-check";
        state.result = "block";
        state.next_row = "V010.SOURCE.PAYLOAD.STREAM.0";
        state.top_blocker = "mapping-plan-not-evaluated";
        mapping_gate_prepare(request, &state, report);
        if (mapping_gate_validate(request, report)) return YVEX_OK;
        return yvex_model_mapping_report_deepseek(request, report, err);
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
