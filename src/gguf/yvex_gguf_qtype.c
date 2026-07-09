/*
 * yvex_gguf_qtype.c - GGUF qtype byte geometry facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   GGUF qtype ABI names, block sizes, bytes-per-block, and qtype refusal
 *   facts for artifact byte geometry.
 *
 * Does not own:
 *   backend compute support, quantization kernels, qtype policy, runtime
 *   execution, or generation.
 *
 * Invariants:
 *   byte geometry is structural; compute support must be proven elsewhere.
 *
 * Boundary:
 *   qtype byte geometry does not imply quantization, backend execution, or
 *   runtime generation.
 */
#include "yvex_gguf_private.h"

static const yvex_gguf_qtype_geometry qtype_geometry[] = {
    {0u, "F32", 1u, 4u, YVEX_GGUF_BOUNDARY_REPORT_ONLY},
    {1u, "F16", 1u, 2u, YVEX_GGUF_BOUNDARY_REPORT_ONLY},
    {2u, "Q4_0", 32u, 18u, YVEX_GGUF_BOUNDARY_REPORT_ONLY},
    {3u, "Q4_1", 32u, 20u, YVEX_GGUF_BOUNDARY_REPORT_ONLY},
    {8u, "Q8_0", 32u, 34u, YVEX_GGUF_BOUNDARY_REPORT_ONLY}
};

/* Contract: returns the number of structural qtype geometry records. */
size_t yvex_gguf_qtype_geometry_count(void)
{
    return sizeof(qtype_geometry) / sizeof(qtype_geometry[0]);
}

/* Contract: returns a qtype geometry record by index, or zero on bounds miss. */
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_at(size_t index)
{
    if (index >= yvex_gguf_qtype_geometry_count()) return 0;
    return &qtype_geometry[index];
}

/* Contract: looks up structural qtype geometry without compute claims. */
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_find(unsigned int qtype)
{
    size_t i;

    for (i = 0u; i < yvex_gguf_qtype_geometry_count(); ++i) {
        if (qtype_geometry[i].qtype == qtype) return &qtype_geometry[i];
    }
    return 0;
}
