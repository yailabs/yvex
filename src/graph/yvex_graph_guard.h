/*
 * yvex_graph_guard.h - graph guard report facts.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   graph guard private include surface.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr output,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   guard code returns facts and never writes operator output.
 *
 * Boundary:
 *   graph guard facts are not generation readiness.
 */
#ifndef YVEX_GRAPH_GUARD_H
#define YVEX_GRAPH_GUARD_H

#include "yvex_graph_report.h"

#endif
