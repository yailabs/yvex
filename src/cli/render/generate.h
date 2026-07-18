/*
 * generate.h - typed generate report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal/audit/help rendering declarations for generation CLI reports.
 *
 * Does not own:
 *   generation report construction, argv parsing, runtime execution, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   renderers serialize typed report facts only and do not mutate counters.
 *
 * Boundary:
 *   rendering diagnostic generation facts is not generation support.
 */
#ifndef YVEX_GENERATE_RENDER_H
#define YVEX_GENERATE_RENDER_H

#include <stdio.h>

#include "src/cli/input/generate.h"

int yvex_generate_render(FILE *fp,
                         yvex_generate_render_mode mode,
                         const yvex_generation_report *report);
int yvex_generate_render_normal(FILE *fp,
                                const yvex_generation_report *report);
int yvex_generate_render_audit(FILE *fp,
                               const yvex_generation_report *report);
int yvex_generate_render_trace(FILE *fp,
                               const yvex_generation_report *report);
int yvex_generate_render_help(FILE *fp);

#endif
