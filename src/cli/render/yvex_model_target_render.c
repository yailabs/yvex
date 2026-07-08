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
 *   CLI argument parsing, command dispatch, target catalogs, report
 *   construction, sidecar writing, runtime execution, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   this typed renderer writes only through src/cli/io helpers and renders
 *   typed report rows supplied by model-target report builders.
 *
 * Boundary:
 *   model-target rendering serializes existing report-only facts and does not
 *   prove quantization, artifact emission, runtime execution, generation,
 *   evaluation, benchmark, throughput, or release readiness.
 */
#include "yvex_model_target_render.h"

#include "yvex_cli_out.h"

static int model_target_render_rows(FILE *fp,
                                    const yvex_model_target_text_value *rows,
                                    unsigned long count)
{
    unsigned long i;
    int rc = 0;

    for (i = 0; i < count; ++i) {
        rc = yvex_cli_out_writef(fp, "%s\n", rows[i].value);
        if (rc < 0) {
            return rc;
        }
    }
    return rc;
}

static int model_target_render_table_rows(FILE *fp,
                                          const yvex_model_target_report *report)
{
    unsigned long r;
    int rc = 0;

    for (r = 0; r < report->table_row_count; ++r) {
        const yvex_model_target_table_row *row = &report->table_rows[r];
        unsigned int c;

        for (c = 0; c < row->column_count; ++c) {
            rc = yvex_cli_out_writef(fp, "%s%s",
                                     c == 0 ? "" : "  ",
                                     row->columns[c]);
            if (rc < 0) {
                return rc;
            }
        }
        rc = yvex_cli_out_writef(fp, "\n");
        if (rc < 0) {
            return rc;
        }
    }
    return rc;
}

/*
 * yvex_model_target_render()
 *
 * Purpose:
 *   render typed model-target report rows.
 *
 * Inputs:
 *   fp is an explicit output stream; mode selects normal/table/audit handling;
 *   report is borrowed.
 *
 * Effects:
 *   writes report rows through CLI IO helpers.
 *
 * Failure:
 *   returns CLI writer status.
 *
 * Boundary:
 *   rendering does not build reports, write sidecars, execute runtime paths, or
 *   create capability claims.
 */
int yvex_model_target_render(FILE *fp,
                             yvex_model_target_render_mode mode,
                             const yvex_model_target_report *report)
{
    int rc;

    if (!report) {
        return 0;
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE && report->table_row_count > 0) {
        rc = model_target_render_table_rows(fp, report);
        if (rc < 0) {
            return rc;
        }
    }
    return model_target_render_rows(fp, report->rows, report->row_count);
}

int yvex_model_target_render_errors(FILE *fp,
                                    const yvex_model_target_report *report)
{
    if (!report) {
        return 0;
    }
    return model_target_render_rows(fp, report->error_rows,
                                    report->error_row_count);
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
