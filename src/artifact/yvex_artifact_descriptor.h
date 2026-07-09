/*
 * yvex_artifact_descriptor.h - YVEX artifact descriptor boundary.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   internal artifact descriptor status records projected from GGUF facts.
 *
 * Does not own:
 *   runtime descriptors, graph execution, backend binding, generation, eval,
 *   benchmark, or release claims.
 *
 * Invariants:
 *   descriptor acceptance must carry an explicit blocker when refused.
 *
 * Boundary:
 *   artifact descriptors are not runtime descriptors.
 */
#ifndef YVEX_ARTIFACT_DESCRIPTOR_H
#define YVEX_ARTIFACT_DESCRIPTOR_H

typedef enum {
    YVEX_ARTIFACT_DESCRIPTOR_REFUSED = 0,
    YVEX_ARTIFACT_DESCRIPTOR_REPORT_ONLY = 1
} yvex_artifact_descriptor_status;

typedef struct {
    yvex_artifact_descriptor_status status;
    const char *format;
    const char *reason;
    const char *next_row;
} yvex_artifact_descriptor_fact;

void yvex_artifact_descriptor_refuse_missing_gguf(yvex_artifact_descriptor_fact *fact);
const char *yvex_artifact_descriptor_status_name(yvex_artifact_descriptor_status status);

#endif
