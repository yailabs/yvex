/* Owner: src/cli/commands
 * Owns: graph command dispatch from typed input parser to report builder and renderer.
 * Does not own: graph construction, memory planning, backend probing, primitive execution, guard facts, report
 *   construction, rendering internals, generation, eval, benchmark, or release decisions.
 * Invariants: adapter stays thin: parse input, call one graph report API, render a typed report, and return an exit
 *   code.
 * Boundary: command dispatch is not graph runtime support.
 * Purpose: provide graph command dispatch from typed input parser to report builder and renderer.
 * Inputs: typed command arguments and borrowed domain APIs.
 * Effects: dispatches domain calls and routes operator bytes only through CLI I/O.
 * Failure: returns a stable CLI status while preserving domain ownership. */
#include "src/cli/input/private.h"
#include "src/cli/render/private.h"
#include "src/cli/io/private.h"

#include <stdio.h>
#include <string.h>

/* Purpose: Parse graph print parse error into typed CLI state (`graph_cli_print_parse_error`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

/* Purpose: Render graph print runtime error from typed facts (`graph_cli_print_runtime_error`). */
static int graph_cli_print_runtime_error(const yvex_error *err, int exit_code)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return exit_code;
}

/* Purpose: Construct the owned graph build report state (`graph_cli_build_report`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static int graph_cli_build_report(const yvex_graph_report_request *request,
                                  yvex_graph_report *report,
                                  yvex_error *err)
{
    if (request->action == YVEX_GRAPH_ACTION_DUMP) {
        return yvex_graph_report_build(request, report, err);
    }
    if (request->action == YVEX_GRAPH_ACTION_CHECK ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_FIXTURE ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_PARTIAL ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_SEGMENT ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_OP ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_BLOCK ||
        request->action == YVEX_GRAPH_ACTION_EXECUTE_LAYERS) {
        return yvex_graph_primitive_report_build(request, report, err);
    }
    return yvex_graph_report_build(request, report, err);
}

/* Purpose: Orchestrate the typed graph command request (`yvex_graph_command`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_graph_command(int argc, char **argv)
{
    yvex_graph_args args;
    yvex_graph_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&report, 0, sizeof(report));

    rc = yvex_graph_args_parse(argc, argv, &args, &err);
    if (rc != YVEX_OK) {
        return graph_cli_print_parse_error(&err);
    }

    if (args.help_requested) {
        (void)yvex_graph_render_help(yvex_cli_out_stdout());
        return args.help_exit_code;
    }

    rc = graph_cli_build_report(&args.request, &report, &err);
    if (rc != YVEX_OK) {
        int exit_code = report.exit_code
                            ? report.exit_code
                            : exit_for_status(rc);
        if (report.body) {
            (void)yvex_graph_render(yvex_cli_out_stdout(),
                                    args.render_mode,
                                    &report);
        }
        yvex_graph_report_clear(&report);
        return graph_cli_print_runtime_error(&err, exit_code);
    }

    (void)yvex_graph_render(yvex_cli_out_stdout(),
                            args.render_mode,
                            &report);
    rc = report.exit_code;
    yvex_graph_report_clear(&report);
    return rc;
}

/* Purpose: Render graph help from typed facts (`yvex_graph_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_graph_help(FILE *fp)
{
    (void)yvex_graph_render_help(fp);
}
