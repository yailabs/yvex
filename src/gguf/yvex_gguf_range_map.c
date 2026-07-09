/*
 * yvex_gguf_range_map.c - GGUF absolute range map facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   absolute byte range validation, tensor span overflow checks, and alignment
 *   refusal facts.
 *
 * Does not own:
 *   file reading, payload materialization, residency, backend tensor binding,
 *   or runtime execution.
 *
 * Invariants:
 *   range checks are pure arithmetic and never read tensor payload bytes.
 *
 * Boundary:
 *   a valid range map is not materialization, residency, graph binding, or
 *   generation support.
 */
#include "yvex_gguf_private.h"

/* Contract: validates a single absolute tensor byte range without file IO. */
int yvex_gguf_range_map_validate(unsigned long long offset,
                                 unsigned long long size,
                                 unsigned long long file_size,
                                 unsigned long long alignment,
                                 const char **reason)
{
    unsigned long long end;

    if (alignment == 0ull) {
        if (reason) *reason = "invalid GGUF alignment";
        return 0;
    }
    if ((offset % alignment) != 0ull) {
        if (reason) *reason = "GGUF tensor range is misaligned";
        return 0;
    }
    end = offset + size;
    if (end < offset) {
        if (reason) *reason = "GGUF tensor range overflows";
        return 0;
    }
    if (end > file_size) {
        if (reason) *reason = "GGUF tensor range exceeds file size";
        return 0;
    }
    if (reason) *reason = "GGUF tensor range accepted";
    return 1;
}
