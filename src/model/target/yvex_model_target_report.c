/*
 * yvex_model_target_report.c - model-target report coordinator.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   unified model-target report dispatch, request routing, and shared report
 *   cleanup entry points.
 *
 * Does not own:
 *   target catalogs, target decisions, candidate facts, class profiles, tensor
 *   collection, tensor naming, output-head maps, tokenizer maps, missing-role
 *   facts, mapping gates, qtype facts, sidecar file writing, CLI parsing,
 *   command dispatch, rendering, stdout/stderr byte emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the coordinator routes typed requests to specialized model-target modules;
 *   it does not render, open operator streams, or contain report-specific
 *   static catalogs.
 *
 * Boundary:
 *   model-target reports are report-only facts. This coordinator does not
 *   implement quantization, artifact emission, runtime execution, generation,
 *   eval, benchmark, throughput, or release readiness.
 */
#include "yvex_model_target_report.h"

#include "yvex_model_class_profile.h"
#include "yvex_model_target_candidates.h"
#include "yvex_model_target_catalog.h"
#include "yvex_model_target_decision.h"
#include "yvex_tensor_collection_report.h"
#include "yvex_deepseek_tensor_coverage.h"

#include "yvex_mapping_gate_report.h"
#include "yvex_missing_role_report.h"
#include "yvex_output_head_map_report.h"
#include "yvex_qtype_policy_report.h"
#include "yvex_qtype_role_support_report.h"
#include "yvex_tensor_naming_report.h"
#include "yvex_tokenizer_map_report.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int model_target_report_store(yvex_model_target_text_value *rows,
                                     unsigned long cap,
                                     unsigned long *count,
                                     const char *fmt,
                                     va_list ap)
{
    int n;

    if (!rows || !count || !fmt || *count >= cap) {
        return 0;
    }
    n = vsnprintf(rows[*count].value, sizeof(rows[*count].value), fmt, ap);
    if (n < 0) {
        rows[*count].value[0] = '\0';
        return 0;
    }
    rows[*count].value[sizeof(rows[*count].value) - 1u] = '\0';
    (*count)++;
    return 1;
}

int yvex_model_target_report_add_row(yvex_model_target_report *report,
                                     const char *fmt,
                                     ...)
{
    va_list ap;
    int ok;

    if (!report || !fmt) {
        return 0;
    }
    va_start(ap, fmt);
    ok = model_target_report_store(report->rows,
                                   YVEX_MODEL_TARGET_ROW_CAP,
                                   &report->row_count,
                                   fmt,
                                   ap);
    va_end(ap);
    return ok;
}

int yvex_model_target_report_add_error(yvex_model_target_report *report,
                                       const char *fmt,
                                       ...)
{
    va_list ap;
    int ok;

    if (!report || !fmt) {
        return 0;
    }
    va_start(ap, fmt);
    ok = model_target_report_store(report->error_rows,
                                   sizeof(report->error_rows) /
                                       sizeof(report->error_rows[0]),
                                   &report->error_row_count,
                                   fmt,
                                   ap);
    va_end(ap);
    return ok;
}

int yvex_model_target_report_add_table_row(yvex_model_target_report *report,
                                           unsigned int column_count,
                                           const char *c0,
                                           const char *c1,
                                           const char *c2,
                                           const char *c3,
                                           const char *c4,
                                           const char *c5,
                                           const char *c6,
                                           const char *c7)
{
    const char *cols[YVEX_MODEL_TARGET_TABLE_COL_CAP];
    yvex_model_target_table_row *row;
    unsigned int i;

    if (!report || column_count > YVEX_MODEL_TARGET_TABLE_COL_CAP ||
        report->table_row_count >= YVEX_MODEL_TARGET_TABLE_ROW_CAP) {
        return 0;
    }
    cols[0] = c0;
    cols[1] = c1;
    cols[2] = c2;
    cols[3] = c3;
    cols[4] = c4;
    cols[5] = c5;
    cols[6] = c6;
    cols[7] = c7;
    row = &report->table_rows[report->table_row_count++];
    memset(row, 0, sizeof(*row));
    row->column_count = column_count;
    for (i = 0; i < column_count; ++i) {
        snprintf(row->columns[i], sizeof(row->columns[i]), "%s",
                 cols[i] ? cols[i] : "");
    }
    return 1;
}

/*
 * yvex_model_target_report_add_output_contract()
 *
 * Purpose:
 *   append the shared report-only output-contract probe facts.
 *
 * Inputs:
 *   report is mutated; report_name/mode are borrowed strings.
 *
 * Effects:
 *   appends bounded rows only; it does not render, write streams, inspect
 *   artifacts, execute runtime code, or claim readiness.
 *
 * Failure:
 *   row-cap exhaustion is handled by the shared bounded row helper.
 *
 * Boundary:
 *   output-contract probes prove only CLI field shape, not runtime,
 *   generation, benchmark, or release capability.
 */
void yvex_model_target_report_add_output_contract(yvex_model_target_report *report,
                                                  const char *report_name,
                                                  const char *mode)
{
    yvex_model_target_report_add_row(report, "status: pass");
    yvex_model_target_report_add_row(report, "report: %s",
                                     report_name ? report_name : "unknown");
    yvex_model_target_report_add_row(report, "mode: %s",
                                     mode ? mode : "unknown");
    yvex_model_target_report_add_row(report, "runtime_claim: unsupported");
    yvex_model_target_report_add_row(report,
                                     "generation: unsupported-full-model");
    yvex_model_target_report_add_row(report, "benchmark_status: not-measured");
    yvex_model_target_report_add_row(report, "release_ready: false");
    yvex_model_target_report_add_row(
        report,
        "boundary: output-contract check only; no runtime/generation claim");
}

int yvex_model_target_report_build(const yvex_model_target_request *request,
                                   yvex_model_target_report *report,
                                   yvex_error *err)
{
    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_target_report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(report, 0, sizeof(*report));
    report->kind = request->kind;
    report->mode = request->mode;
    report->exit_code = 0;

    switch (request->kind) {
    case YVEX_MODEL_TARGET_COMMAND_HELP:
        return yvex_model_target_help_report_build(report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASSES:
    case YVEX_MODEL_TARGET_COMMAND_LIST:
    case YVEX_MODEL_TARGET_COMMAND_INSPECT:
        return yvex_model_target_catalog_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_DECISION:
        return yvex_model_target_decision_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_DENSE_CANDIDATE:
    case YVEX_MODEL_TARGET_COMMAND_QWEN_METAL:
        return yvex_model_target_candidate_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_CLASS_PROFILE:
        return yvex_model_class_profile_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION:
        return yvex_tensor_collection_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP:
        if (request->gate[0]) {
            return yvex_mapping_gate_report_build(request, report, err);
        }
        if (strcmp(request->role, "output-head") == 0) {
            return yvex_output_head_map_report_build(request, report, err);
        }
        if (strcmp(request->role, "tokenizer") == 0) {
            return yvex_tokenizer_map_report_build(request, report, err);
        }
        if (strcmp(request->role, "missing-roles") == 0) {
            return yvex_missing_role_report_build(request, report, err);
        }
        return yvex_tensor_naming_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_TOKENIZER_MAP:
        return yvex_tokenizer_map_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_MISSING_ROLES:
        return yvex_missing_role_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY:
        if (request->gate[0] || request->include_requirements) {
            return yvex_qtype_role_support_report_build(request, report, err);
        }
        return yvex_qtype_policy_report_build(request, report, err);
    case YVEX_MODEL_TARGET_COMMAND_UNKNOWN:
    default:
        return yvex_model_target_catalog_report_build(request, report, err);
    }
}

int yvex_model_target_help_report_build(yvex_model_target_report *report,
                                        yvex_error *err)
{
    return yvex_model_target_catalog_help_report_build(report, err);
}

void yvex_model_target_report_close(yvex_model_target_report *report)
{
    if (!report) {
        return;
    }
    yvex_deepseek_tensor_coverage_close(report->deepseek_tensor_coverage);
    yvex_deepseek_v4_ir_close(report->deepseek_architecture_ir);
    memset(report, 0, sizeof(*report));
}
