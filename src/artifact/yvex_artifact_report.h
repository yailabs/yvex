/*
 * yvex_artifact_report.h - typed artifact boundary report facts.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   internal report records for descriptor, materialization, and roundtrip
 *   gate summaries.
 *
 * Does not own:
 *   CLI rendering, file writing, runtime execution, eval, benchmark, or
 *   release claims.
 *
 * Invariants:
 *   reports carry facts only and never serialize operator output.
 *
 * Boundary:
 *   artifact reports do not create artifact capability.
 */
#ifndef YVEX_ARTIFACT_REPORT_H
#define YVEX_ARTIFACT_REPORT_H

typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_artifact_report_fact;

void yvex_artifact_report_fact_init(yvex_artifact_report_fact *fact,
                                    const char *kind,
                                    const char *status,
                                    const char *reason,
                                    const char *next_row);

#endif
