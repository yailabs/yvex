/*
 * yvex_model_artifacts_render.c - model artifact typed report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   help and typed normal/table/audit rendering for model artifact reports.
 *
 * Does not own:
 *   report building, registry lookup, model gate checks, backend calls,
 *   file writing, artifact emission, runtime generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   this renderer formats typed facts only and uses src/cli/io writers.
 *
 * Boundary:
 *   rendered reports do not imply artifact emission, runtime generation,
 *   benchmark evidence, or release readiness.
 */
#include "yvex_model_artifacts_render.h"
#include "yvex_cli_out.h"

static const char *artifact_kind_name(yvex_model_artifact_report_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_ARTIFACT_REPORT_LIST: return "list";
    case YVEX_MODEL_ARTIFACT_REPORT_CHECK: return "check";
    case YVEX_MODEL_ARTIFACT_REPORT_STATUS:
    default: return "status";
    }
}

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

int yvex_model_artifacts_render_help(FILE *fp)
{
    yvex_cli_out_line(fp, "usage: yvex models <action> [TARGET] [options]");
    yvex_cli_out_line(fp, "actions: list, current, use, add, remove, scan, check, prepare, artifacts");
    yvex_cli_out_line(fp, "boundary: model artifact registry reports are not runtime generation");
    return 0;
}
