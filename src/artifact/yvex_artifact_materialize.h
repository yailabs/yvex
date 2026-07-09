/*
 * yvex_artifact_materialize.h - artifact materialization boundary.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   internal materialization input contract and refusal status.
 *
 * Does not own:
 *   backend tensor binding, graph execution, runtime generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   materialization refusal is explicit until concrete proof exists.
 *
 * Boundary:
 *   materialization is not backend execution.
 */
#ifndef YVEX_ARTIFACT_MATERIALIZE_H
#define YVEX_ARTIFACT_MATERIALIZE_H

typedef struct {
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_artifact_materialize_fact;

void yvex_artifact_materialize_refuse(yvex_artifact_materialize_fact *fact);
int yvex_artifact_materialize_supported(const char **reason);

#endif
