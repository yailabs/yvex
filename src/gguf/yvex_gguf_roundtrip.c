/*
 * yvex_gguf_roundtrip.c - GGUF writer-reader equivalence boundary.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   roundtrip refusal facts until writer output and reader acceptance can be
 *   compared.
 *
 * Does not own:
 *   writer byte emission, parser implementation, artifact registration,
 *   materialization, runtime descriptor projection, or generation.
 *
 * Invariants:
 *   roundtrip cannot pass while writer support is unavailable.
 *
 * Boundary:
 *   roundtrip refusal is not artifact readiness or runtime readiness.
 */
#include "yvex_gguf_private.h"

/* Contract: reports GGUF roundtrip support state without parsing or writing. */
int yvex_gguf_roundtrip_supported(const char **reason)
{
    if (reason) *reason = "GGUF roundtrip requires writer support first";
    return 0;
}
