/*
 * generate_trace.h - typed generate trace renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   diagnostic generation trace rendering declarations.
 *
 * Does not own:
 *   trace accounting, generation report construction, argv parsing, runtime
 *   execution, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   trace renderers are read-only over reports; trace counters are owned by
 *   the generation report builder.
 *
 * Boundary:
 *   trace rendering is diagnostic evidence only and not generation support.
 */
#ifndef YVEX_GENERATE_TRACE_RENDER_H
#define YVEX_GENERATE_TRACE_RENDER_H

#include <stdio.h>

#include "src/generation/report.h"

int yvex_generate_render_trace(FILE *fp,
                               const yvex_generation_report *report);

#endif
