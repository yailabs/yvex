/*
 * yvex_model_target_cli.c - model-target command adapter.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   model-target command dispatch only.
 *
 * Does not own:
 *   target catalogs, candidate facts, profile specs, tensor/qtype/source
 *   scanning, report construction, sidecar writing, rendering fields, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   adapter flow is parse argv, build typed report, render typed report, return
 *   exit code; usage/help text is rendered through the model-target renderer.
 *
 * Boundary:
 *   command dispatch does not create capability, quantization, artifact
 *   emission, runtime support, generation support, benchmark evidence, or
 *   release readiness.
 */
#include "yvex_model_target_args.h"
#include "yvex_model_target_render.h"

#include "yvex_cli_out.h"

#include <stdio.h>
#include <string.h>

/*
 * yvex_model_target_command()
 *
 * Purpose:
 *   dispatch model-target CLI requests through typed input, report, and render
 *   APIs.
 *
 * Inputs:
 *   argc/argv are borrowed CLI arguments.
 *
 * Effects:
 *   parses model-target args, builds one typed model-target report, renders
 *   primary/diagnostic report segments through CLI renderers, and returns the
 *   report exit code.
 *
 * Failure:
 *   returns parser/report failure codes while preserving existing report output.
 *
 * Boundary:
 *   the adapter performs no target fact construction, source/native scanning,
 *   sidecar writing, runtime execution, generation, eval, benchmark, or release
 *   readiness work.
 */
int yvex_model_target_command(int argc, char **argv)
{
    yvex_model_target_args args;
    yvex_model_target_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&report, 0, sizeof(report));

    rc = yvex_model_target_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return 2;
    }

    rc = yvex_model_target_report_build(&args.request, &report, &err);
    if (rc != YVEX_OK) {
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    (void)yvex_model_target_render(yvex_cli_out_stdout(),
                                   args.request.mode,
                                   &report);
    (void)yvex_model_target_render_errors(yvex_cli_out_stderr(), &report);
    rc = report.exit_code;
    yvex_model_target_report_close(&report);
    return rc;
}

/*
 * yvex_model_target_help()
 *
 * Purpose:
 *   render model-target domain help through the typed renderer.
 *
 * Inputs:
 *   fp is an explicit output stream.
 *
 * Effects:
 *   writes model-target help using CLI IO from the renderer.
 *
 * Failure:
 *   none surfaced to callers because top-level help callbacks are void.
 *
 * Boundary:
 *   help rendering describes report-only commands and does not create runtime,
 *   generation, benchmark, or release capability.
 */
void yvex_model_target_help(FILE *fp)
{
    (void)yvex_model_target_render_help(fp);
}
