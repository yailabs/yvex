/*
 * yvex_runtime_descriptor_report.h - runtime descriptor report facts.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   typed runtime descriptor projection report records.
 *
 * Does not own:
 *   CLI rendering, graph execution, runtime generation, eval, benchmark, or
 *   release claims.
 *
 * Invariants:
 *   descriptor reports preserve missing-blocker state.
 *
 * Boundary:
 *   reporting descriptor projection is not runtime execution.
 */
#ifndef YVEX_RUNTIME_DESCRIPTOR_REPORT_H
#define YVEX_RUNTIME_DESCRIPTOR_REPORT_H

typedef struct {
    const char *status;
    const char *blocker;
    const char *next_row;
} yvex_runtime_descriptor_report_fact;

void yvex_runtime_descriptor_report_init(yvex_runtime_descriptor_report_fact *fact,
                                         const char *status,
                                         const char *blocker,
                                         const char *next_row);

#endif
