/*
 * yvex_gguf_report.c - typed GGUF report facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   typed GGUF report construction for ABI/report-only and unsupported
 *   writer/roundtrip/materialization states.
 *
 * Does not own:
 *   CLI rendering, operator byte output, file writing, runtime generation,
 *   eval, benchmark, or release claims.
 *
 * Invariants:
 *   reports carry facts only and do not serialize user-visible output.
 *
 * Boundary:
 *   GGUF reports do not imply writer, roundtrip, materialization, or runtime
 *   capability.
 */
#include "yvex_gguf_private.h"

/* Contract: initializes a GGUF report fact without allocation or rendering. */
void yvex_gguf_report_fact_init(yvex_gguf_report_fact *report,
                                const char *kind,
                                const char *status,
                                const char *reason,
                                const char *next_row)
{
    if (!report) return;
    report->kind = kind ? kind : "gguf";
    report->status = status ? status : "unsupported";
    report->reason = reason ? reason : "GGUF capability is future-owned";
    report->next_row = next_row ? next_row : "V010.GGUF.ARTIFACT.ABI.0";
}
