/*
 * yvex_artifact_roundtrip_gate.h - artifact roundtrip gate facts.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   emitted-artifact acceptance/refusal gate records and roundtrip
 *   precondition facts.
 *
 * Does not own:
 *   GGUF writer, GGUF reader, materialization, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   gate state remains blocked until writer and reader equivalence exists.
 *
 * Boundary:
 *   roundtrip gate facts are not artifact emission.
 */
#ifndef YVEX_ARTIFACT_ROUNDTRIP_GATE_H
#define YVEX_ARTIFACT_ROUNDTRIP_GATE_H

typedef struct {
    const char *status;
    const char *writer_status;
    const char *reader_status;
    const char *reason;
    const char *next_row;
} yvex_artifact_roundtrip_gate_fact;

void yvex_artifact_roundtrip_gate_refuse(yvex_artifact_roundtrip_gate_fact *fact);
int yvex_artifact_roundtrip_gate_allows(int writer_ok, int reader_ok, const char **reason);

#endif
