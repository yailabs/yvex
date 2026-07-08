/*
 * yvex_tensor_collection_report.c - tensor collection report boundary.
 *
 * Owner: src/model/target
 * Owns: tensor collection report ownership.
 * Does not own: CLI parsing, rendering, tensor payload loading, runtime, generation, benchmark, or release readiness.
 * Invariants: collection facts do not read tensor payload bytes.
 * Boundary: tensor collection reporting is not runtime support.
 */
#include "yvex_tensor_collection_report.h"

typedef int yvex_tensor_collection_report_file_boundary;
