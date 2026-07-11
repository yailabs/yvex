/*
 * yvex_gguf_tensor_info.c - GGUF tensor_info ABI boundary facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   tensor_info rank/name/raw qtype id/shape boundary facts and rank refusal
 *   state.
 *
 * Does not own:
 *   runtime role mapping, qtype byte geometry, concrete materialization,
 *   residency, backend binding, or generation.
 *
 * Invariants:
 *   tensor_info rank validation stays separate from role and layout mapping.
 *
 * Boundary:
 *   tensor_info ABI facts do not imply role mapping, materialization, graph
 *   binding, or runtime support.
 */
#include "yvex_gguf_private.h"

#include <stddef.h>

static const yvex_gguf_boundary_fact tensor_info_boundary = {
    "src/gguf/yvex_gguf_tensor_info.c",
    "GGUF tensor_info ABI",
    YVEX_GGUF_BOUNDARY_OPERATIONAL,
    "tensor_info facts are owned by the canonical structural reader",
    YVEX_GGUF_ABI_NEXT_ROW
};

/* Contract: exposes tensor_info ownership facts without mutation or IO. */
const yvex_gguf_boundary_fact *yvex_gguf_tensor_info_boundary(void)
{
    return &tensor_info_boundary;
}

/* Contract: initializes tensor_info ABI facts to fail-closed not-evaluated state. */
void yvex_gguf_tensor_info_abi_init(yvex_gguf_tensor_info_abi *abi)
{
    if (!abi) return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->tensor_count = 0ull;
    abi->max_rank = 0u;
    abi->rank_one_tensor_count = 0ull;
    abi->named_tensor_count = 0ull;
    abi->qtype_known_tensor_count = 0ull;
    abi->qtype_refused_tensor_count = 0ull;
    abi->reason = "GGUF tensor_info ABI not evaluated";
}

/* Contract: validates GGUF tensor rank limits only, not semantic role mapping. */
int yvex_gguf_tensor_info_rank_supported(unsigned int rank, const char **reason)
{
    if (rank > 0u && rank <= 4u) {
        if (reason) *reason = "tensor_info rank accepted by GGUF ABI boundary";
        return 1;
    }
    if (reason) *reason = "unsupported GGUF tensor_info rank";
    return 0;
}

/* Contract: validates parsed tensor_info rows as ABI facts, not runtime roles. */
int yvex_gguf_tensor_info_abi_from_gguf(const yvex_gguf *gguf,
                                        yvex_gguf_tensor_info_abi *abi,
                                        const char **reason)
{
    unsigned long long i;
    unsigned long long count;

    yvex_gguf_tensor_info_abi_init(abi);
    if (!gguf || !abi) {
        if (reason) *reason = "GGUF tensor_info requires a parsed GGUF view";
        return 0;
    }

    count = yvex_gguf_tensor_count(gguf);
    abi->tensor_count = count;

    for (i = 0ull; i < count; ++i) {
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        unsigned int d;

        if (!tensor || !tensor->name || tensor->name[0] == '\0') {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = "tensor_info entry is missing a tensor name";
            if (reason) *reason = abi->reason;
            return 0;
        }
        abi->named_tensor_count += 1ull;

        if (!yvex_gguf_tensor_info_rank_supported(tensor->rank, reason)) {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = reason ? *reason : "tensor_info rank unsupported";
            return 0;
        }
        if (tensor->rank > abi->max_rank) {
            abi->max_rank = tensor->rank;
        }

        for (d = 0u; d < tensor->rank; ++d) {
            if (tensor->dims[d] == 0ull) {
                abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
                abi->reason = "tensor_info dimension is zero";
                if (reason) *reason = abi->reason;
                return 0;
            }
        }
        if (tensor->rank == 1u) {
            abi->rank_one_tensor_count += 1ull;
        }

        if (yvex_gguf_qtype_supported_for_storage(tensor->ggml_type, NULL)) {
            abi->qtype_known_tensor_count += 1ull;
        } else {
            abi->qtype_refused_tensor_count += 1ull;
        }
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF tensor_info ABI accepted";
    if (reason) *reason = abi->reason;
    return 1;
}
