/*
 * graph.h - typed graph CLI renderer API.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   graph normal/table/audit/help rendering entrypoints.
 *
 * Does not own:
 *   graph report construction, input parsing, command dispatch, backend
 *   primitives, reference checks, stdout/stderr primitives, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   renderers accept typed reports and write only through src/cli/io helpers.
 *
 * Boundary:
 *   rendering graph facts is not transformer execution support.
 */
#ifndef YVEX_GRAPH_RENDER_H
#define YVEX_GRAPH_RENDER_H

#include "src/graph/report.h"

#include <stdio.h>

int yvex_graph_render(FILE *fp,
                      yvex_graph_report_mode mode,
                      const yvex_graph_report *report);
int yvex_graph_render_normal(FILE *fp,
                             const yvex_graph_report *report);
int yvex_graph_render_table(FILE *fp,
                            const yvex_graph_report *report);
int yvex_graph_render_audit(FILE *fp,
                            const yvex_graph_report *report);
int yvex_graph_render_help(FILE *fp);

#endif
