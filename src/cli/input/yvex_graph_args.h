/*
 * yvex_graph_args.h - graph command typed input parser API.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   typed argv parse result for the graph command.
 *
 * Does not own:
 *   graph construction, graph reports, backend probes, primitive execution,
 *   rendering, stdout/stderr output, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   parsing produces a borrowed, typed request and never opens artifacts or
 *   backends.
 *
 * Boundary:
 *   graph input parsing is not graph execution support.
 */
#ifndef YVEX_GRAPH_ARGS_H
#define YVEX_GRAPH_ARGS_H

#include "yvex_graph_report.h"

typedef struct {
    yvex_graph_report_request request;
    yvex_graph_report_mode render_mode;
    int help_requested;
    int help_exit_code;
} yvex_graph_args;

int yvex_graph_args_parse(int argc,
                          char **argv,
                          yvex_graph_args *out,
                          yvex_error *err);

#endif
