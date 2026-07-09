/*
 * yvex_gguf_tensor_info.c - GGUF tensor_info ABI boundary facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   tensor_info rank/name/dtype/shape boundary facts and rank refusal state.
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

static const yvex_gguf_boundary_fact tensor_info_boundary = {
    "src/gguf/yvex_gguf_tensor_info.c",
    "GGUF tensor_info ABI",
    YVEX_GGUF_BOUNDARY_REPORT_ONLY,
    "tensor_info facts are structural only",
    "V010.GGUF.ARTIFACT.ABI.0"
};

/* Contract: exposes tensor_info ownership facts without mutation or IO. */
const yvex_gguf_boundary_fact *yvex_gguf_tensor_info_boundary(void)
{
    return &tensor_info_boundary;
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
