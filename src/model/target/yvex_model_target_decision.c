/*
 * yvex_model_target_decision.c - target decision report boundary.
 *
 * Owner: src/model/target
 * Owns: target-decision report ownership boundary.
 * Does not own: CLI parsing, rendering, sidecar writing, runtime, generation, benchmark, or release readiness.
 * Invariants: decision facts are report-only and do not promote model support.
 * Boundary: target decision reporting is not runtime readiness.
 */
#include "yvex_model_target_decision.h"

typedef int yvex_model_target_decision_file_boundary;
