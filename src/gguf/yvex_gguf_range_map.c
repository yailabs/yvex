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

#include <limits.h>
#include <stddef.h>

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

/* Contract: initializes range facts without reading tensor payload bytes. */
void yvex_gguf_range_fact_init(yvex_gguf_range_fact *fact)
{
    if (!fact) return;
    fact->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    fact->checked_tensor_count = 0ull;
    fact->tensor_data_offset = 0ull;
    fact->file_size = 0ull;
    fact->total_expected_storage_bytes = 0ull;
    fact->first_expected_storage_bytes = 0ull;
    fact->first_actual_available_bytes = 0ull;
    fact->qtype_checked_tensor_count = 0ull;
    fact->alignment = 0u;
    fact->reason = "GGUF range map not evaluated";
}

/* Contract: validates parser-visible offsets plus qtype-derived storage spans. */
int yvex_gguf_range_fact_from_gguf(const yvex_gguf *gguf,
                                   yvex_gguf_range_fact *fact,
                                   const char **reason)
{
    unsigned long long i;
    unsigned long long count;

    yvex_gguf_range_fact_init(fact);
    if (!gguf || !fact) {
        if (reason) *reason = "GGUF range map requires a parsed GGUF view";
        return 0;
    }

    fact->tensor_data_offset = yvex_gguf_tensor_data_offset(gguf);
    fact->file_size = yvex_gguf_file_size(gguf);
    fact->alignment = yvex_gguf_alignment(gguf);

    if (!yvex_gguf_range_map_validate(fact->tensor_data_offset,
                                      0ull,
                                      fact->file_size,
                                      fact->alignment ? fact->alignment : 1u,
                                      reason)) {
        fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
        fact->reason = reason ? *reason : "GGUF tensor data offset refused";
        return 0;
    }

    count = yvex_gguf_tensor_count(gguf);
    for (i = 0ull; i < count; ++i) {
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        unsigned long long expected_storage_bytes;
        unsigned long long available_bytes = 0ull;
        if (!tensor) {
            fact->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            fact->reason = "GGUF tensor range row is missing";
            if (reason) *reason = fact->reason;
            return 0;
        }
        if (!yvex_gguf_range_map_validate(tensor->absolute_offset,
                                          0ull,
                                          fact->file_size,
                                          fact->alignment ? fact->alignment : 1u,
                                          reason)) {
            fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            fact->reason = reason ? *reason : "GGUF tensor absolute offset refused";
            return 0;
        }

        expected_storage_bytes = tensor->storage_bytes;

        if (tensor->absolute_offset <= fact->file_size) {
            available_bytes = fact->file_size - tensor->absolute_offset;
        }
        if (fact->qtype_checked_tensor_count == 0ull) {
            fact->first_expected_storage_bytes = expected_storage_bytes;
            fact->first_actual_available_bytes = available_bytes;
        }
        if (!yvex_gguf_range_map_validate(tensor->absolute_offset,
                                          expected_storage_bytes,
                                          fact->file_size,
                                          fact->alignment ? fact->alignment : 1u,
                                          reason)) {
            fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            fact->reason = reason ? *reason : "GGUF qtype storage span exceeds artifact range";
            return 0;
        }
        if (fact->total_expected_storage_bytes > ULLONG_MAX - expected_storage_bytes) {
            fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            fact->reason = "GGUF expected tensor storage total overflows";
            if (reason) *reason = fact->reason;
            return 0;
        }
        fact->total_expected_storage_bytes += expected_storage_bytes;
        fact->qtype_checked_tensor_count += 1ull;
        fact->checked_tensor_count += 1ull;
    }

    fact->status = YVEX_GGUF_ABI_SECTION_OK;
    fact->reason = "GGUF ABI-visible ranges accepted";
    if (reason) *reason = fact->reason;
    return 1;
}
