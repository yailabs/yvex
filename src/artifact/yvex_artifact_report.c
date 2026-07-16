/*
 * yvex_artifact_report.c - typed artifact report construction.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   descriptor, materialization, and roundtrip gate summary report facts.
 *
 * Does not own:
 *   CLI rendering, operator byte output, file writing, runtime execution,
 *   eval, benchmark, or release claims.
 *
 * Invariants:
 *   report facts preserve explicit unsupported/refusal status.
 *
 * Boundary:
 *   artifact report construction is not artifact emission.
 */
#include "yvex_artifact_report.h"

/* Contract: initializes a typed artifact report fact without allocation or IO. */
void yvex_artifact_report_fact_init(yvex_artifact_report_fact *fact,
                                    const char *kind,
                                    const char *status,
                                    const char *reason,
                                    const char *next_row)
{
    if (!fact) return;
    fact->kind = kind ? kind : "artifact";
    fact->status = status ? status : "unsupported";
    fact->reason = reason ? reason : "artifact capability is future-owned";
    fact->next_row = next_row ? next_row : "V010.ARTIFACT.MATERIALIZE.0";
}
