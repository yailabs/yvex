/*
 * yvex_artifact_materialize.c - artifact materialization refusal facts.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   artifact materialization boundary and explicit unsupported status.
 *
 * Does not own:
 *   backend tensor allocation, graph binding, graph execution, runtime
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   this owner does not read tensor payload bytes in the target seed row.
 *
 * Boundary:
 *   materialization support waits for V010.ARTIFACT.MATERIALIZE.0.
 */
#include "yvex_artifact_materialize.h"

/* Contract: initializes a fail-closed materialization fact. */
void yvex_artifact_materialize_refuse(yvex_artifact_materialize_fact *fact)
{
    if (!fact) return;
    fact->status = "unsupported";
    fact->reason = "artifact materialization is future-owned";
    fact->next_row = "V010.ARTIFACT.MATERIALIZE.0";
}

/* Contract: reports materialization support state without touching bytes. */
int yvex_artifact_materialize_supported(const char **reason)
{
    if (reason) *reason = "artifact materialization is future-owned by V010.ARTIFACT.MATERIALIZE.0";
    return 0;
}
