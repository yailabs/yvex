/* Owner: src/cli/commands
 * Owns: sampling command dispatch from typed input to report builder and renderer.
 * Does not own: sampling internals, model resolution, token input validation, graph guard preflight, engine open,
 *   report fact construction, rendering internals, generation, eval, benchmark, or release decisions.
 * Invariants: adapter stays thin: parse input, call one report API, render typed report, and return a mapped exit
 *   code.
 * Boundary: command dispatch is not real vocabulary sampling support.
 * Purpose: bind sample CLI input to the typed sampling report API.
 * Inputs: argc/argv are borrowed command input from the top-level CLI.
 * Effects: writes help, parser errors, runtime errors, or typed sampling reports.
 * Failure: parser failures return 2; report failures return mapped YVEX exit codes. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"

#include <stdio.h>
#include <string.h>

/* Purpose: Parse sampling print parse error into typed CLI state (`sampling_cli_print_parse_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int sampling_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

/* Purpose: Render sampling print runtime error from typed facts (`sampling_cli_print_runtime_error`). */
static int sampling_cli_print_runtime_error(const yvex_error *err, int status)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return exit_for_status(status);
}

/* Purpose: Orchestrate the typed sample command request (`yvex_sample_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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

/* Purpose: Render sample help from typed facts (`yvex_sample_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_sample_help(FILE *fp)
{
    (void)yvex_sampling_render_help(fp);
}
