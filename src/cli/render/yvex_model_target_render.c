/*
 * yvex_model_target_render.c - model-target typed report rendering.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal/table/audit/help rendering for typed model-target reports.
 *
 * Does not own:
 *   argv parsing, command dispatch, target catalogs, report construction,
 *   sidecar writing, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   this file writes only through src/cli/io helpers and never formats domain
 *   facts from raw state.
 *
 * Boundary:
 *   model-target rendering serializes existing report-only facts and does not
 *   prove quantization, artifact emission, runtime execution, generation,
 *   evaluation, benchmark, throughput, or release readiness.
 */
#include "yvex_model_target_render.h"

#include "yvex_cli_out.h"

static int model_target_render_text(FILE *fp, const char *text)
{
    return yvex_cli_out_writef(fp, "%s", text ? text : "");
}

/*
 * yvex_model_target_render()
 *
 * Purpose:
 *   render the primary typed model-target text segment.
 *
 * Inputs:
 *   fp is an explicit output stream; mode is retained for typed renderer
 *   dispatch; report is borrowed.
 *
 * Effects:
 *   writes report primary text through CLI IO helpers.
 *
 * Failure:
 *   returns CLI writer status.
 *
 * Boundary:
 *   rendering does not build reports, write sidecars, execute runtime paths, or
 *   create capability claims.
 */
int yvex_model_target_render(FILE *fp,
                             yvex_model_target_output_mode mode,
                             const yvex_model_target_report *report)
{
    (void)mode;
    if (!report) {
        return 0;
    }
    return model_target_render_text(fp, report->primary_text);
}

int yvex_model_target_render_errors(FILE *fp,
                                    const yvex_model_target_report *report)
{
    if (!report) {
        return 0;
    }
    return model_target_render_text(fp, report->diagnostic_text);
}

/*
 * yvex_model_target_render_help()
 *
 * Purpose:
 *   render model-target help as a typed report.
 *
 * Inputs:
 *   fp is an explicit output stream.
 *
 * Effects:
 *   builds the help report and writes it through CLI IO helpers.
 *
 * Failure:
 *   returns non-zero if help report construction fails.
 *
 * Boundary:
 *   help rendering describes report-only surfaces and does not create runtime
 *   or generation capability.
 */
int yvex_model_target_render_help(FILE *fp)
{
    yvex_model_target_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_model_target_help_report_build(&report, &err);
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(fp, "model-target help unavailable\n");
        return rc;
    }
    (void)yvex_model_target_render(fp, YVEX_MODEL_TARGET_OUTPUT_NORMAL, &report);
    yvex_model_target_report_close(&report);
    return 0;
}
