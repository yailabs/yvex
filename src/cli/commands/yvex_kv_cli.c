/*
 * yvex_kv_cli.c - KV command adapter.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   KV command dispatch from typed input to KV report builders and renderer.
 *
 * Does not own:
 *   KV cache internals, model scanning, report fact construction, rendering
 *   internals, attention execution, decode, logits, sampling, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   adapter stays thin: parse input, call one report API, render typed report,
 *   return the report or parser/runtime exit code.
 *
 * Boundary:
 *   command dispatch is not runtime KV support.
 *
 * Purpose:
 *   bind KV CLI input to typed KV report APIs.
 *
 * Inputs:
 *   argc/argv from yvex kv.
 *
 * Effects:
 *   renders help, parser errors, runtime errors, or typed KV reports.
 *
 * Failure:
 *   returns parser, report-build, or renderer exit codes.
 */
#include "yvex_kv_args.h"
#include "yvex_kv_render.h"
#include "yvex_cli_out.h"

#include <stdio.h>
#include <string.h>

static int kv_cli_exit_for_status(int status)
{
    switch (status) {
    case YVEX_OK:
        return 0;
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
    case YVEX_ERR_NOMEM:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

static int kv_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

static int kv_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return kv_cli_exit_for_status(status);
}

/*
 * yvex_kv_command()
 *
 * Purpose:
 *   dispatch the KV command through parser, domain/report builder, and typed
 *   renderer.
 *
 * Inputs:
 *   argc/argv are borrowed command input from the top-level CLI.
 *
 * Effects:
 *   writes only through KV renderer or CLI error writer helpers.
 *
 * Failure:
 *   parser failures return 2; report builder failures return mapped YVEX exit
 *   codes after rendering any typed failure report.
 *
 * Boundary:
 *   dispatch does not allocate KV directly or inspect model artifacts directly.
 */
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

/*
 * yvex_kv_help()
 *
 * Purpose:
 *   expose KV help for top-level yvex help dispatch.
 *
 * Inputs:
 *   fp is a borrowed output stream.
 *
 * Effects:
 *   renders help text through the KV renderer.
 *
 * Failure:
 *   stream failures are left to the stream state.
 *
 * Boundary:
 *   help text is not KV runtime support.
 */
void yvex_kv_help(FILE *fp)
{
    (void)yvex_kv_render_help(fp);
}
