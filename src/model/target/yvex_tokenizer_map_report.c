/*
 * yvex_tokenizer_map_report.c - tokenizer map report boundary.
 *
 * Owner: src/model/target
 * Owns: tokenizer map report ownership.
 * Does not own: CLI parsing, rendering, tokenizer runtime, generation, benchmark, or release readiness.
 * Invariants: tokenizer map facts do not tokenize runtime prompts.
 * Boundary: tokenizer map reporting is not runtime generation.
 */
#include "yvex_tokenizer_map_report.h"

typedef int yvex_tokenizer_map_report_file_boundary;
