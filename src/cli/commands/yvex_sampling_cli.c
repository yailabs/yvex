/*
 * yvex_sampling_cli.c - sampling command adapter.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   sampling command dispatch from typed input to report builder and renderer.
 *
 * Does not own:
 *   sampling internals, model resolution, token input validation, graph guard
 *   preflight, engine open, report fact construction, rendering internals,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   adapter stays thin: parse input, call one report API, render typed report,
 *   and return a mapped exit code.
 *
 * Boundary:
 *   command dispatch is not real vocabulary sampling support.
 *
 * Purpose:
 *   bind sample CLI input to the typed sampling report API.
 *
 * Inputs:
 *   argc/argv are borrowed command input from the top-level CLI.
 *
 * Effects:
 *   writes help, parser errors, runtime errors, or typed sampling reports.
 *
 * Failure:
 *   parser failures return 2; report failures return mapped YVEX exit codes.
 */
#include "yvex_sampling_args.h"
#include "yvex_sampling_render.h"
#include "yvex_cli_out.h"

#include <stdio.h>
#include <string.h>

static int sampling_cli_exit_for_status(int status)
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

static int sampling_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

static int sampling_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return sampling_cli_exit_for_status(status);
}

int yvex_sample_command(int argc, char **argv)
{
    yvex_sampling_args args;
    yvex_sampling_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&report, 0, sizeof(report));

    rc = yvex_sampling_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return sampling_cli_print_parse_error(&err);
    }

    if (args.help_requested) {
        (void)yvex_sampling_render_help(yvex_cli_out_stdout());
        return args.help_exit_code;
    }

    rc = yvex_sampling_report_build(&args.request, &report, &err);
    if (rc != YVEX_OK) {
        if (report.status) {
            (void)yvex_sampling_render(yvex_cli_out_stdout(),
                                       args.render_mode,
                                       &report);
        }
        return sampling_cli_print_runtime_error(&err, rc);
    }

    (void)yvex_sampling_render(yvex_cli_out_stdout(),
                               args.render_mode,
                               &report);
    return report.exit_code;
}

void yvex_sample_help(FILE *fp)
{
    (void)yvex_sampling_render_help(fp);
}
