/*
 * generate.c - generate command adapter.
 *
 * Owner: src/cli/commands.
 * Owns: generate command dispatch from typed input to generation report and renderer.
 * Does not own: generation report facts, runtime state, trace rendering internals, eval, or benchmark.
 * Invariants: adapter stays thin and does not hide domain behavior.
 * Boundary: command dispatch is not full-model generation.
 *
 * Purpose: bind generate CLI input to the typed diagnostic generation API.
 * Inputs: argv from yvex generate.
 * Effects: renders help, parser errors, or diagnostic generation reports.
 * Failure: returns parser, runtime, or renderer exit codes.
 */
#include "src/cli/input/generate.h"
#include "src/cli/render/generate.h"
#include "src/cli/io/out.h"

#include <stdio.h>

static int generate_cli_exit_for_status(int status)
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

static int generate_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

static int generate_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return generate_cli_exit_for_status(status);
}

int yvex_generate_command(int argc, char **argv)
{
    yvex_generate_args args;
    yvex_generation_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_generate_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return generate_cli_print_parse_error(&err);
    }
    if (args.help_requested) {
        return yvex_generate_render_help(yvex_cli_out_stdout());
    }

    rc = yvex_generation_run_diagnostic(&args.request, &report, &err);
    if (report.loop_created) {
        (void)yvex_generate_render(yvex_cli_out_stdout(),
                                   args.render_mode,
                                   &report);
    }
    if (rc != YVEX_OK) {
        return generate_cli_print_runtime_error(&err, rc);
    }
    return 0;
}

void yvex_generate_help(FILE *fp)
{
    (void)yvex_generate_render_help(fp);
}
