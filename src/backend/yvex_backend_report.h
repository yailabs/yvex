/*
 * yvex_backend_report.h - backend report facts.
 *
 * Owner:
 *   src/backend
 *
 * Owns:
 *   typed backend capability and qtype report records.
 *
 * Does not own:
 *   CLI rendering, graph execution, quantization, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   backend reports carry facts only and do not serialize operator output.
 *
 * Boundary:
 *   backend reports are not backend runtime generation.
 */
#ifndef YVEX_BACKEND_REPORT_H
#define YVEX_BACKEND_REPORT_H

typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_backend_report_fact;

void yvex_backend_report_fact_init(yvex_backend_report_fact *fact,
                                   const char *kind,
                                   const char *status,
                                   const char *reason,
                                   const char *next_row);

#endif
