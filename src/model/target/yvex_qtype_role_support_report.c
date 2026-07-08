/*
 * yvex_qtype_role_support_report.c - qtype role-support report boundary.
 *
 * Owner: src/model/target
 * Owns: qtype role-support report ownership.
 * Does not own: CLI parsing, rendering, quantization execution, runtime, generation, benchmark, or release readiness.
 * Invariants: support facts preserve deferred compute/calibration/imatrix states.
 * Boundary: qtype role-support reporting does not complete quantization.
 */
#include "yvex_qtype_role_support_report.h"

typedef int yvex_qtype_role_support_report_file_boundary;
