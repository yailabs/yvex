/*
 * layout_map.c - emitted GGUF layout plan facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   runtime-role to emitted GGUF layout/range plan boundary and qtype-layout
 *   compatibility blockers.
 *
 * Does not own:
 *   concrete byte emission, materialization, residency, backend binding,
 *   graph binding, or generation.
 *
 * Invariants:
 *   dimensions are logical GGML axis order; forced MXFP4 storage is admitted
 *   by the canonical row-aware qtype ABI.
 *
 * Boundary:
 *   layout-map facts do not emit bytes or materialize tensors.
 */
#include "map.h"

#include <yvex/gguf_qtype.h>

/* Admits one logical shape and any source-forced qtype without byte offsets. */
int yvex_gguf_layout_map_shape_supported(yvex_tensor_role role,
                                         unsigned int qtype,
                                         unsigned int rank,
                                         const unsigned long long *dims,
                                         const char **reason)
{
    unsigned int i;

    if (role <= YVEX_TENSOR_ROLE_UNKNOWN || role >= YVEX_TENSOR_ROLE_COUNT ||
        !dims || rank == 0u || rank > YVEX_TENSOR_MAX_DIMS) {
        if (reason) *reason = "invalid role, rank, or dimensions";
        return 0;
    }
    for (i = 0u; i < rank; ++i) {
        if (!dims[i]) {
            if (reason) *reason = "logical GGML dimension is zero";
            return 0;
        }
    }
    if (qtype != YVEX_GGUF_NO_FORCED_QTYPE) {
        yvex_gguf_qtype_storage_result storage;
        if (yvex_gguf_qtype_tensor_storage(qtype, dims, rank, &storage) !=
            YVEX_GGUF_QTYPE_STORAGE_OK) {
            if (reason) *reason = storage.reason;
            return 0;
        }
    }
    if (reason) *reason = "admitted logical GGML shape";
    return 1;
}

/* Contract: reports layout-map support state without building byte ranges. */
int yvex_gguf_layout_map_supported(const char **reason)
{
    if (reason) *reason = "shape admission requires a typed descriptor";
    return 0;
}
