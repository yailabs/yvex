/*
 * tokenizer_map.h - tokenizer map report boundary.
 *
 * Owner: src/model/target
 * Owns: tokenizer map report declarations.
 * Does not own: CLI parsing, rendering, tokenizer runtime, generation, benchmark, or release readiness.
 * Invariants: tokenizer map facts inspect metadata sidecars only.
 * Boundary: tokenizer mapping is not tokenizer runtime support.
 */
#ifndef YVEX_TOKENIZER_MAP_REPORT_H
#define YVEX_TOKENIZER_MAP_REPORT_H

#include "report.h"

int yvex_tokenizer_map_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

#endif
