/*
 * yvex_model_target_candidates.c - target candidate report boundary.
 *
 * Owner: src/model/target
 * Owns: full-runtime, dense, and Qwen/Metal candidate report ownership.
 * Does not own: CLI parsing, rendering, sidecar writing, runtime, generation, benchmark, or release readiness.
 * Invariants: candidate rows are pressure facts only.
 * Boundary: candidate reporting is not model support.
 */
#include "yvex_model_target_candidates.h"

typedef int yvex_model_target_candidates_file_boundary;
