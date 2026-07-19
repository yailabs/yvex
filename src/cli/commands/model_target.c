/* Owner: src/cli/commands
 * Owns: model-target command dispatch only.
 * Does not own: target catalogs, candidate facts, profile specs, tensor/qtype/source scanning, report construction,
 *   sidecar writing, rendering fields, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 * Invariants: adapter flow is parse argv, build typed report, render typed report, return exit code; usage/help
 *   text is rendered through the model-target renderer.
 * Boundary: command dispatch does not create capability, quantization, artifact emission, runtime support,
 *   generation support, benchmark evidence, or release readiness.
 * Purpose: provide model-target command dispatch only.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"

#include <stdio.h>
#include <string.h>

/* Purpose: dispatch model-target CLI requests through typed input, report, and render APIs.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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
    if (args.parse_failed) {
        yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                            args.error_message[0]
                                ? args.error_message
                                : "model-target: invalid arguments");
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

/* Purpose: render model-target domain help through the typed renderer.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_model_target_help(FILE *fp)
{
    (void)yvex_model_target_render_help(fp);
}
