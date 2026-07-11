/*
 * yvex_artifact_descriptor.c - YVEX artifact descriptor facts.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   YVEX artifact descriptor projection boundary and descriptor blockers.
 *
 * Does not own:
 *   runtime descriptor projection, graph execution, backend binding, runtime
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   refused descriptors carry a concrete reason and next row.
 *
 * Boundary:
 *   artifact descriptor facts do not make an artifact executable.
 */
#include "yvex_artifact_descriptor.h"

/* Contract: names artifact descriptor states for typed reports. */
const char *yvex_artifact_descriptor_status_name(yvex_artifact_descriptor_status status)
{
    switch (status) {
        case YVEX_ARTIFACT_DESCRIPTOR_REFUSED:
            return "refused";
        case YVEX_ARTIFACT_DESCRIPTOR_REPORT_ONLY:
            return "report-only";
    }
    return "refused";
}

/* Contract: refuses descriptor support until complete-artifact admission closes. */
void yvex_artifact_descriptor_refuse_missing_gguf(yvex_artifact_descriptor_fact *fact)
{
    if (!fact) return;
    fact->status = YVEX_ARTIFACT_DESCRIPTOR_REFUSED;
    fact->format = "gguf";
    fact->reason = "artifact descriptor support requires complete-artifact admission";
    fact->next_row = "V010.ARTIFACT.SUPPORT.CUTOVER.0";
}
