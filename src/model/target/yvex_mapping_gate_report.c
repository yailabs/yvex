/*
 * yvex_mapping_gate_report.c - tensor mapping gate report boundary.
 *
 * Owner: src/model/target
 * Owns: tensor mapping gate report ownership.
 * Does not own: CLI parsing, rendering, artifact emission, runtime, generation, benchmark, or release readiness.
 * Invariants: gate facts remain conservative blockers until explicitly promoted.
 * Boundary: mapping gate reporting does not emit artifacts.
 */
#include "yvex_mapping_gate_report.h"

typedef int yvex_mapping_gate_report_file_boundary;
