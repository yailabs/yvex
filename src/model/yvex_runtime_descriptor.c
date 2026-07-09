/*
 * yvex_runtime_descriptor.c - runtime descriptor projection facts.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   artifact descriptor to runtime descriptor projection boundary and missing
 *   descriptor refusal facts.
 *
 * Does not own:
 *   graph binding, graph execution, backend tensor binding, runtime
 *   generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   projection cannot proceed without accepted artifact descriptor facts.
 *
 * Boundary:
 *   projection facts do not execute a model graph.
 */
#include "yvex_runtime_descriptor.h"

/* Contract: initializes a refused runtime descriptor projection fact. */
void yvex_runtime_descriptor_refuse(yvex_runtime_descriptor_fact *fact)
{
    if (!fact) return;
    fact->status = "unsupported";
    fact->artifact_status = "missing";
    fact->reason = "runtime descriptor projection requires accepted artifact descriptor facts";
    fact->next_row = "V010.RUNTIME.DESCRIPTOR.GGUF.0";
}

/* Contract: reports projection support state from artifact descriptor evidence. */
int yvex_runtime_descriptor_projection_supported(int artifact_descriptor_ok,
                                                 const char **reason)
{
    if (artifact_descriptor_ok) {
        if (reason) *reason = "artifact descriptor facts present for projection boundary";
        return 1;
    }
    if (reason) *reason = "runtime descriptor projection blocked by missing artifact descriptor";
    return 0;
}
