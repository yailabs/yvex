/*
 * yvex_gguf_range_map.c - GGUF local range and canonical layout projections.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   bounded single-range arithmetic and projection of an accepted canonical
 *   layout into legacy GGUF range facts.
 *
 * Does not own:
 *   global tensor order, padding admission, overlap policy, file reading,
 *   payload materialization, residency, backend tensor binding, or runtime
 *   execution.
 *
 * Invariants:
 *   local range checks are pure arithmetic; global facts are copied only from
 *   the canonical layout result and never reconstructed here.
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

/* Contract: projects accepted canonical layout facts without recalculation. */
int yvex_gguf_range_fact_from_layout(const yvex_gguf_layout_result *layout,
                                     yvex_gguf_range_fact *fact,
                                     const char **reason)
{
    yvex_gguf_range_fact_init(fact);
    if (!layout || !fact) {
        if (reason) *reason = "GGUF range projection requires a layout result";
        return 0;
    }
    fact->tensor_data_offset = layout->tensor_data_offset;
    fact->file_size = layout->actual_file_size;
    fact->alignment = layout->alignment;
    fact->checked_tensor_count = layout->tensors_validated;
    fact->qtype_checked_tensor_count = layout->tensors_validated;
    fact->total_expected_storage_bytes = layout->raw_tensor_bytes;
    if (!layout->accepted || layout->code != YVEX_GGUF_LAYOUT_OK) {
        fact->status = YVEX_GGUF_ABI_SECTION_REFUSED;
        fact->reason = layout->reason;
        if (reason) *reason = fact->reason;
        return 0;
    }
    fact->status = YVEX_GGUF_ABI_SECTION_OK;
    fact->reason = "GGUF ranges project the canonical global layout result";
    if (reason) *reason = fact->reason;
    return 1;
}
