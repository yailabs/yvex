/*
 * yvex_qtype_policy_report.c - qtype policy report boundary.
 *
 * Owner: src/model/target
 * Owns: qtype policy report ownership.
 * Does not own: CLI parsing, rendering, quantization execution, artifact emission, runtime, generation, benchmark, or release readiness.
 * Invariants: qtype policy facts do not produce converted weights.
 * Boundary: qtype policy reporting does not complete quantization.
 */
#include "yvex_qtype_policy_report.h"

typedef int yvex_qtype_policy_report_file_boundary;
