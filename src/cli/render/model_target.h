/*
 * model_target.h - model-target CLI renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   typed model-target report rendering declarations.
 *
 * Does not own:
 *   CLI argument parsing, command dispatch, target catalogs, report
 *   construction, sidecar writing, runtime execution, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   renderers accept typed model-target reports and serialize them through
 *   src/cli/io writer helpers.
 *
 * Boundary:
 *   rendering model-target facts does not create runtime, generation, benchmark,
 *   or release capability.
 */
#ifndef YVEX_MODEL_TARGET_OUTPUT_H
#define YVEX_MODEL_TARGET_OUTPUT_H

#include <stdio.h>

#include <report.h>

int yvex_model_target_render(FILE *fp,
                             yvex_model_target_render_mode mode,
                             const yvex_model_target_report *report);

int yvex_model_target_render_errors(FILE *fp,
                                    const yvex_model_target_report *report);

int yvex_model_target_render_help(FILE *fp);

#endif
