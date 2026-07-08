/*
 * yvex_missing_role_report.c - missing-role report boundary.
 *
 * Owner: src/model/target
 * Owns: missing runtime-role report ownership.
 * Does not own: CLI parsing, rendering, runtime execution, generation, benchmark, or release readiness.
 * Invariants: missing-role facts are blockers, not readiness claims.
 * Boundary: missing-role reports do not implement role consumers.
 */
#include "yvex_missing_role_report.h"

typedef int yvex_missing_role_report_file_boundary;
