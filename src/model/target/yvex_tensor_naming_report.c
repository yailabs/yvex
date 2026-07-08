/*
 * yvex_tensor_naming_report.c - tensor naming report boundary.
 *
 * Owner: src/model/target
 * Owns: tensor naming report ownership.
 * Does not own: CLI parsing, rendering, tensor payload loading, runtime, generation, benchmark, or release readiness.
 * Invariants: naming facts are report-only and preserve unmapped states.
 * Boundary: tensor naming reporting is not qtype or runtime support.
 */
#include "yvex_tensor_naming_report.h"

typedef int yvex_tensor_naming_report_file_boundary;
