/*
 * yvex_gguf_layout_integrity.h - private checked layout arithmetic.
 *
 * Owner: src/gguf global layout integrity.
 * Owns: testable checked interval and aggregate arithmetic used by validator.
 * Does not own: parsing, payload IO, emitted layout maps, or public API policy.
 * Invariants: every helper is pure and returns a typed layout code.
 * Boundary: arithmetic success alone is not global layout admission.
 */
#ifndef YVEX_GGUF_LAYOUT_INTEGRITY_PRIVATE_H
#define YVEX_GGUF_LAYOUT_INTEGRITY_PRIVATE_H

#include <yvex/gguf_layout.h>

yvex_gguf_layout_code yvex_gguf_layout_interval_measure(
    unsigned long long relative_offset,
    unsigned long long raw_size,
    unsigned int alignment,
    unsigned long long *raw_end,
    unsigned long long *padded_end);

yvex_gguf_layout_code yvex_gguf_layout_sum_checked(
    unsigned long long current,
    unsigned long long addition,
    unsigned long long *out);

#endif
