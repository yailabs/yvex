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

#include <string.h>

/* Contract: names artifact descriptor states for typed reports. */
const char *yvex_artifact_descriptor_status_name(yvex_artifact_descriptor_status status)
{
    switch (status) {
        case YVEX_ARTIFACT_DESCRIPTOR_REFUSED:
            return "refused";
        case YVEX_ARTIFACT_DESCRIPTOR_REPORT_ONLY:
            return "report-only";
        case YVEX_ARTIFACT_DESCRIPTOR_COMPLETE_ADMITTED:
            return "complete-artifact-admitted";
    }
    return "refused";
}

/* Contract: refuses descriptor projection without canonical admission evidence. */
void yvex_artifact_descriptor_refuse_missing_gguf(yvex_artifact_descriptor_fact *fact)
{
    if (!fact) return;
    memset(fact, 0, sizeof(*fact));
    fact->status = YVEX_ARTIFACT_DESCRIPTOR_REFUSED;
    fact->format = "gguf";
    fact->reason = "artifact descriptor support requires complete-artifact admission";
    fact->next_row = "V010.ARTIFACT.MATERIALIZE.0";
}

/* Projects only canonical complete admission into materialization intake. */
int yvex_artifact_descriptor_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_artifact_descriptor_fact *fact)
{
    if (!fact) return 0;
    memset(fact, 0, sizeof(*fact));
    if (!admission || !admission->complete ||
        admission->artifact_class != YVEX_ARTIFACT_CLASS_COMPLETE_YVEX ||
        !admission->materialization_input_ready ||
        !admission->artifact_identity[0]) {
        yvex_artifact_descriptor_refuse_missing_gguf(fact);
        return 0;
    }
    fact->status = YVEX_ARTIFACT_DESCRIPTOR_COMPLETE_ADMITTED;
    fact->format = "gguf";
    fact->reason = "complete YVEX artifact passed canonical admission";
    fact->next_row = "V010.ARTIFACT.MATERIALIZE.0";
    fact->artifact_identity = admission->artifact_identity;
    fact->tensor_count = admission->tensor_count;
    fact->materialization_input_ready = 1;
    fact->runtime_supported = 0;
    return 1;
}
