/*
 * yvex_runtime_descriptor.h - runtime descriptor projection boundary.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   internal artifact-descriptor to runtime-descriptor projection status.
 *
 * Does not own:
 *   graph binding, graph execution, backend tensor allocation, generation,
 *   eval, benchmark, or release claims.
 *
 * Invariants:
 *   runtime descriptor projection refuses when required artifact facts are
 *   missing.
 *
 * Boundary:
 *   runtime descriptor projection is not graph execution.
 */
#ifndef YVEX_RUNTIME_DESCRIPTOR_H
#define YVEX_RUNTIME_DESCRIPTOR_H

typedef struct {
    const char *status;
    const char *artifact_status;
    const char *reason;
    const char *next_row;
} yvex_runtime_descriptor_fact;

void yvex_runtime_descriptor_refuse(yvex_runtime_descriptor_fact *fact);
int yvex_runtime_descriptor_projection_supported(int artifact_descriptor_ok,
                                                 const char **reason);

#endif
