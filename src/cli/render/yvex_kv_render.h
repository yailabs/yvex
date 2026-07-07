/*
 * yvex_kv_render.h - typed KV report renderer.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal, table, audit, and help rendering declarations for KV reports.
 *
 * Does not own:
 *   KV report construction, input parsing, command dispatch, runtime behavior,
 *   attention execution, decode, logits, sampling, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   renderers serialize typed KV report facts only.
 *
 * Boundary:
 *   rendering KV facts is not runtime KV support.
 */
#ifndef YVEX_KV_RENDER_H
#define YVEX_KV_RENDER_H

#include <stdio.h>

#include "yvex_kv_report.h"

int yvex_kv_render(FILE *fp,
                   yvex_kv_report_mode mode,
                   const yvex_kv_report *report);
int yvex_kv_render_normal(FILE *fp,
                          const yvex_kv_report *report);
int yvex_kv_render_table(FILE *fp,
                         const yvex_kv_report *report);
int yvex_kv_render_audit(FILE *fp,
                         const yvex_kv_report *report);
int yvex_kv_render_help(FILE *fp);

#endif
