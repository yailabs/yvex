/*
 * sampling.h - typed sampling report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, audit, and help rendering entrypoints for sampling reports.
 *
 * Does not own:
 *   report construction, input parsing, command dispatch, sampler execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   renderers consume typed sampling reports only.
 *
 * Boundary:
 *   rendering sampling facts is not real vocabulary sampling support.
 */
#ifndef YVEX_SAMPLING_RENDER_H
#define YVEX_SAMPLING_RENDER_H

#include "src/generation/sampling_report.h"

#include <stdio.h>

int yvex_sampling_render(FILE *fp,
                         yvex_sampling_report_mode mode,
                         const yvex_sampling_report *report);

int yvex_sampling_render_normal(FILE *fp,
                                const yvex_sampling_report *report);

int yvex_sampling_render_audit(FILE *fp,
                               const yvex_sampling_report *report);

int yvex_sampling_render_help(FILE *fp);

#endif
