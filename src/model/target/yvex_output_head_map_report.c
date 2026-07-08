/*
 * yvex_output_head_map_report.c - output-head map report boundary.
 *
 * Owner: src/model/target
 * Owns: output-head map report ownership.
 * Does not own: CLI parsing, rendering, logits execution, runtime, generation, benchmark, or release readiness.
 * Invariants: map facts do not execute logits or consume tensor payloads.
 * Boundary: output-head reports are not generation support.
 */
#include "yvex_output_head_map_report.h"

typedef int yvex_output_head_map_report_file_boundary;
