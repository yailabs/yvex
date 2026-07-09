/*
 * yvex_gguf_layout_map.c - emitted GGUF layout plan facts.
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
 *   layout planning stays separate from writer byte emission.
 *
 * Boundary:
 *   layout-map facts do not emit bytes or materialize tensors.
 */
#include "yvex_gguf_private.h"

/* Contract: reports layout-map support state without building byte ranges. */
int yvex_gguf_layout_map_supported(const char **reason)
{
    if (reason) *reason = "GGUF layout map is future-owned by V010.MAP.GGUF.LAYOUT.0";
    return 0;
}
