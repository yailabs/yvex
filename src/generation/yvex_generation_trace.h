/*
 * yvex_generation_trace.h - diagnostic generation trace accounting.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   trace-section selection and read-only record count accounting for
 *   diagnostic generation reports.
 *
 * Does not own:
 *   CLI rendering, argument parsing, command dispatch, model execution, provider
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   trace counters are derived from report facts before rendering; rendering
 *   must not mutate trace totals.
 *
 * Boundary:
 *   trace accounting is diagnostic evidence only and not generation support.
 */
#ifndef YVEX_GENERATION_TRACE_H
#define YVEX_GENERATION_TRACE_H

#include "yvex_generation_report.h"

int yvex_generation_trace_wants_tokens(yvex_generation_trace_level level);
int yvex_generation_trace_wants_steps(yvex_generation_trace_level level);
int yvex_generation_trace_wants_kv(yvex_generation_trace_level level);
int yvex_generation_trace_wants_logits(yvex_generation_trace_level level);
int yvex_generation_trace_wants_sampling(yvex_generation_trace_level level);
void yvex_generation_trace_account(yvex_generation_report *report);

#endif
