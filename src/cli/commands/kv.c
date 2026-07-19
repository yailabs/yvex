/* Owner: src/cli/commands
 * Owns: KV command dispatch from typed input to KV report builders and renderer.
 * Does not own: KV cache internals, model scanning, report fact construction, rendering internals, attention
 *   execution, decode, logits, sampling, generation, eval, benchmark, or release decisions.
 * Invariants: adapter stays thin: parse input, call one report API, render typed report, return the report or
 *   parser/runtime exit code.
 * Boundary: command dispatch is not runtime KV support.
 * Purpose: bind KV CLI input to typed KV report APIs.
 * Inputs: argc/argv from yvex kv.
 * Effects: renders help, parser errors, runtime errors, or typed KV reports.
 * Failure: returns parser, report-build, or renderer exit codes. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"

#include <stdio.h>
#include <string.h>

/* Purpose: Parse kv print parse error into typed CLI state (`kv_cli_print_parse_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int kv_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

/* Purpose: Render kv print runtime error from typed facts (`kv_cli_print_runtime_error`). */
static int kv_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return exit_for_status(status);
}

/* Purpose: dispatch the KV command through parser, domain/report builder, and typed renderer.
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_kv_command(int argc, char **argv)
{
    yvex_kv_args args;
    yvex_kv_report report;
    yvex_error err;
    int rc;

    memset(&report, 0, sizeof(report));
    yvex_error_clear(&err);

    rc = yvex_kv_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return kv_cli_print_parse_error(&err);
    }
    if (args.help_requested) {
        return yvex_kv_render_help(yvex_cli_out_stdout());
    }

    if (args.request.kind == YVEX_KV_REQUEST_REPORT) {
        rc = yvex_kv_report_build(&args.request, &report, &err);
    } else {
        rc = yvex_kv_ownership_report_build(&args.request, &report, &err);
    }

    if (rc != YVEX_OK) {
        if (report.status) {
            (void)yvex_kv_render(yvex_cli_out_stdout(),
                                 args.request.report_mode,
                                 &report);
        }
        return kv_cli_print_runtime_error(&err, rc);
    }

    (void)yvex_kv_render(yvex_cli_out_stdout(),
                         args.request.report_mode,
                         &report);
    return report.exit_code;
}

/* Purpose: expose KV help for top-level yvex help dispatch.
 * Inputs: Borrowed typed facts.
 * Effects: CLI-local effects only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_kv_help(FILE *fp)
{
    (void)yvex_kv_render_help(fp);
}
