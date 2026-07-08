/*
 * yvex_graph_cli.c - graph command adapter.
 *
 * Owner:
 *   src/cli/commands
 *
 * Owns:
 *   graph command dispatch from typed input parser to report builder and
 *   renderer.
 *
 * Does not own:
 *   graph construction, memory planning, backend probing, primitive execution,
 *   guard facts, report construction, rendering internals, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   adapter stays thin: parse input, call one graph report API, render a typed
 *   report, and return an exit code.
 *
 * Boundary:
 *   command dispatch is not graph runtime support.
 */
#include "yvex_graph_args.h"
#include "yvex_graph_render.h"
#include "yvex_cli_out.h"

#include <stdio.h>
#include <string.h>

static int graph_cli_exit_for_status(int status)
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

static int graph_cli_print_parse_error(const yvex_error *err)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "%s\n",
                        yvex_error_message(err));
    return 2;
}

static int graph_cli_print_runtime_error(const yvex_error *err, int exit_code)
{
    yvex_cli_out_writef(yvex_cli_out_stderr(), "yvex: %s: %s\n",
                        yvex_error_where(err),
                        yvex_error_message(err));
    return exit_code;
}

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
                            : graph_cli_exit_for_status(rc);
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

void yvex_graph_help(FILE *fp)
{
    (void)yvex_graph_render_help(fp);
}
