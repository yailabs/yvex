/*
 * yvex_artifact_roundtrip_gate.c - emitted artifact roundtrip gate.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   roundtrip dependency facts and emitted-artifact refusal state.
 *
 * Does not own:
 *   GGUF writer byte emission, GGUF parser implementation, materialization,
 *   runtime descriptor projection, graph execution, or generation.
 *
 * Invariants:
 *   roundtrip gates block while writer or reader facts are incomplete.
 *
 * Boundary:
 *   passing this gate later will not imply runtime generation.
 */
#include "yvex_artifact_roundtrip_gate.h"

/* Contract: initializes a roundtrip gate refusal fact. */
void yvex_artifact_roundtrip_gate_refuse(yvex_artifact_roundtrip_gate_fact *fact)
{
    if (!fact) return;
    fact->status = "blocked";
    fact->writer_status = "unsupported";
    fact->reader_status = "report-only";
    fact->reason = "artifact roundtrip requires writer and reader equivalence";
    fact->next_row = "V010.GGUF.ROUNDTRIP.0";
}

/* Contract: accepts only when writer and reader preconditions both hold. */
int yvex_artifact_roundtrip_gate_allows(int writer_ok, int reader_ok, const char **reason)
{
    if (writer_ok && reader_ok) {
        if (reason) *reason = "writer and reader preconditions present";
        return 1;
    }
    if (reason) *reason = "artifact roundtrip gate blocked by missing writer or reader proof";
    return 0;
}
