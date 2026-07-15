/*
 * yvex_qtype_role_support_report.c - qtype role-support report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   dtype/qtype support matrix facts, role-support blockers, and qtype gate
 *   handoff facts.
 *
 * Does not own:
 *   CLI parsing, rendering, quantization execution, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   role-support matrices are report-only facts and hand off incomplete
 *   quantization work to later rows.
 *
 * Boundary:
 *   qtype role-support reporting is not qtype support completion,
 *   quantization, artifact emission, runtime readiness, generation readiness,
 *   benchmark evidence, or release readiness.
 */
#include "yvex_qtype_role_support_report.h"

#include "yvex_model_target_private.h"
#include "../../gguf/yvex_quant_numeric.h"

#include <stdio.h>
#include <string.h>

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
    {"deepseek", "deepseek4-v4-flash", "blocked", "gguf-writer-missing", "V010.GGUF.WRITER.1"},
    {"qwen", "qwen3-6-35b-a3b", "blocked", "family-quantization-plan-unimplemented", "not-scheduled"},
    {"gemma", "gemma-4-31b-it", "blocked", "family-quantization-plan-unimplemented", "not-scheduled"},
};

/* Projects current arithmetic truth exclusively from TRACK.QUANT registry. */
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

/*
 * qtype_role_rows()
 *
 * Purpose:
 *   select the static qtype role-support rows for a source family.
 *
 * Inputs:
 *   family is borrowed and may be empty; count receives row count.
 *
 * Effects:
 *   returns a borrowed static table; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   none.
 *
 * Boundary:
 *   selected rows are support facts, not quantization execution.
 */
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

/*
 * qtype_role_prepare()
 *
 * Purpose:
 *   initialize common typed fields for qtype role-support reports.
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
 *   fields remain report-only and do not imply qtype completion.
 */
static void qtype_role_prepare(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    const char *target = request->target_id[0] ? request->target_id : "qwen3-8b";
    const char *family = yvex_model_target_family_key(target);

    report->kind = request->kind;
    report->mode = request->mode;
    report->status = request->gate[0] ? "qtype-role-support-gate-blocked" :
                     "qtype-role-support-blocked";
    report->exit_code = 0;
    snprintf(report->target_id, sizeof(report->target_id), "%s", target);
    snprintf(report->family, sizeof(report->family), "%s", family);
    snprintf(report->stage, sizeof(report->stage), "report-only");
    snprintf(report->qtype_policy_status, sizeof(report->qtype_policy_status),
             "blocked");
    snprintf(report->artifact_status, sizeof(report->artifact_status), "missing");
    snprintf(report->runtime_status, sizeof(report->runtime_status), "unsupported");
    snprintf(report->generation_status, sizeof(report->generation_status),
             "unsupported-full-model");
    snprintf(report->benchmark_status, sizeof(report->benchmark_status),
             "not-measured");
    snprintf(report->next_row, sizeof(report->next_row), "%s",
             strcmp(family, "deepseek") == 0
                 ? "V010.GGUF.WRITER.1" : "not-scheduled");
    snprintf(report->reason, sizeof(report->reason), "%s",
             strcmp(family, "deepseek") == 0
                 ? "gguf-writer-missing"
                 : "family-quantization-plan-unimplemented");
    snprintf(report->boundary, sizeof(report->boundary),
             "qtype role-support report only; no quantization or artifact emission");
}

/*
 * qtype_role_validate()
 *
 * Purpose:
 *   reject impossible typed request shapes for role-support reports.
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
static int qtype_role_validate(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    if (request->kind != YVEX_MODEL_TARGET_COMMAND_QUANT_POLICY) {
        report->status = "qtype-role-support-fail";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report,
                                           "qtype role-support report requires quant-policy command kind");
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
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        report->status = "unsupported-output-mode";
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "JSON output is unsupported");
        return 1;
    }
    return 0;
}

/*
 * qtype_gate_add_table()
 *
 * Purpose:
 *   append typed table rows for the qtype role-support gate.
 *
 * Inputs:
 *   report is mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   gate rows do not imply qtype completion.
 */
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

/*
 * qtype_gate_add_audit()
 *
 * Purpose:
 *   append typed audit rows for the qtype role-support gate.
 *
 * Inputs:
 *   report is mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   gate audit facts do not implement quantization.
 */
static void qtype_gate_add_audit(yvex_model_target_report *report)
{
    unsigned long i;

    yvex_model_target_report_add_row(report, "report: qtype-role-support-gate");
    yvex_model_target_report_add_row(report, "status: qtype-role-support-gate-blocked");
    yvex_model_target_report_add_row(report, "release: v0.1.0");
    for (i = 0; i < sizeof(qtype_gate_rows) / sizeof(qtype_gate_rows[0]); ++i) {
        yvex_model_target_report_add_row(report, "family.%lu.name: %s", i,
                                         qtype_gate_rows[i].family);
        yvex_model_target_report_add_row(report, "family.%lu.target_id: %s", i,
                                         qtype_gate_rows[i].target_id);
        yvex_model_target_report_add_row(report, "family.%lu.status: %s", i,
                                         qtype_gate_rows[i].status);
        yvex_model_target_report_add_row(report, "family.%lu.top_blocker: %s", i,
                                         qtype_gate_rows[i].top_blocker);
        yvex_model_target_report_add_row(report, "family.%lu.next: %s", i,
                                         qtype_gate_rows[i].next_row);
    }
    yvex_model_target_report_common_tail(report);
}

/*
 * qtype_role_add_table()
 *
 * Purpose:
 *   append typed role-support table rows.
 *
 * Inputs:
 *   family is borrowed; report is mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   role rows are support facts only.
 */
static void qtype_role_add_table(const char *family,
                                 yvex_model_target_report *report)
{
    const qtype_role_fact *rows;
    unsigned long count;
    unsigned long i;

    yvex_model_target_report_add_row(report, "QTYPE ROLE SUPPORT");
    yvex_model_target_report_add_row(report,
                                     "ROLE  SRC_DTYPE  ARTIFACT_QTYPE  STORAGE  COMPUTE  CALIBRATION  STATUS");
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

/*
 * qtype_role_add_audit()
 *
 * Purpose:
 *   append typed role-support audit rows.
 *
 * Inputs:
 *   family is borrowed; report is mutated.
 *
 * Effects:
 *   appends bounded rows only; no allocation, IO, or printing occurs.
 *
 * Failure:
 *   row-cap exhaustion truncates through the shared row helper.
 *
 * Boundary:
 *   audit rows are not quantization or artifact evidence.
 */
static void qtype_role_add_audit(const char *family,
                                 yvex_model_target_report *report)
{
    const qtype_role_fact *rows;
    unsigned long count;
    unsigned long i;
    int selected_slice = strcmp(family, "deepseek") == 0;

    yvex_model_target_report_add_row(report, "report: qtype-role-support");
    yvex_model_target_report_add_row(report, "status: qtype-role-support-blocked");
    yvex_model_target_report_add_row(report, "target_id: %s", report->target_id);
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "source_dtype: %s",
                                     selected_slice ? "selected-slice" : "BF16");
    if (selected_slice) {
        yvex_model_target_report_add_row(report,
                                         "selected_slice_evidence_only: true");
        yvex_model_target_report_add_row(report,
                                         "full_family_artifact_status: missing");
    }
    rows = qtype_role_rows(family, &count);
    for (i = 0; i < count; ++i) {
        yvex_model_target_report_add_row(report, "role.%lu.role_name: %s", i,
                                         rows[i].role_name);
        yvex_model_target_report_add_row(
            report, "role.%lu.source_dtype: %s", i,
            selected_slice ? "selected-slice" : rows[i].source_dtype);
        if (selected_slice) {
            yvex_model_target_report_add_row(
                report,
                "role.%lu.role_status: selected-slice-evidence-only", i);
        }
        yvex_model_target_report_add_row(report,
                                         "role.%lu.compute_support_status: %s", i,
                                         qtype_role_compute_status(
                                             rows[i].source_dtype));
        yvex_model_target_report_add_row(report,
                                         "role.%lu.artifact_emission_allowed: false", i);
        yvex_model_target_report_add_row(report,
                                         "role.%lu.artifact_emission_blocker: %s", i,
                                         selected_slice
                                             ? "gguf-writer-missing"
                                             : rows[i].blocker);
    }
    yvex_model_target_report_add_row(report, "payload_bytes_read: false");
    yvex_model_target_report_add_row(report, "quantization_performed: false");
    yvex_model_target_report_add_row(report, "gguf_emitted: false");
    yvex_model_target_report_common_tail(report);
}

/*
 * yvex_qtype_role_support_report_build()
 *
 * Purpose:
 *   build typed qtype role-support or qtype gate report facts.
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
 *   output/release refusals are returned through report exit_code.
 *
 * Boundary:
 *   qtype role-support reporting is not qtype support completion.
 */
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
            yvex_model_target_report_add_row(report, "qtype-role-support-gate: v0.1.0");
            yvex_model_target_report_add_row(report, "status: qtype-role-support-gate-blocked");
            yvex_model_target_report_add_row(report, "family_count: 3");
            yvex_model_target_report_add_row(report,
                                             "top_blocker: gguf-writer-missing");
            yvex_model_target_report_add_row(report, "next: V010.GGUF.WRITER.1");
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
    yvex_model_target_report_add_row(report, "qtype-role-support: %s", report->target_id);
    yvex_model_target_report_add_row(report, "status: blocked");
    yvex_model_target_report_add_row(report, "family: %s", family);
    yvex_model_target_report_add_row(report, "source_dtype: %s",
                                     strcmp(family, "deepseek") == 0
                                         ? "selected-slice"
                                         : "BF16");
    yvex_model_target_report_add_row(report, "preferred_artifact_qtype: unresolved");
    yvex_model_target_report_add_row(report, "supported_roles: %lu", role_count);
    yvex_model_target_report_add_row(report, "blocked_roles: %lu", role_count);
    yvex_model_target_report_add_row(
        report,
        "top_blocker: %s",
        strcmp(family, "deepseek") == 0
            ? "gguf-writer-missing"
            : "family-quantization-plan-unimplemented");
    yvex_model_target_report_add_row(
        report, "next: %s", strcmp(family, "deepseek") == 0
            ? "V010.GGUF.WRITER.1" : "not-scheduled");
    yvex_model_target_report_add_row(report,
                                     "boundary: qtype role report only; no quantization/GGUF/runtime/generation");
    return YVEX_OK;
}
