/*
 * yvex_model_target_sidecar_write.c - model-target sidecar writer boundary.
 *
 * Owner: src/model/target
 * Owns: explicit local sidecar file output boundary.
 * Does not own: CLI operator streams, rendering, runtime execution, generation, benchmark, or release readiness.
 * Invariants: this file may own explicit local file serialization but never
 *   terminal byte streams.
 * Boundary: sidecar writing is not artifact emission or runtime readiness.
 */
#include "yvex_model_target_sidecar_write.h"

typedef int yvex_model_target_sidecar_write_file_boundary;
