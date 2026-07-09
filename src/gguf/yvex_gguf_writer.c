/*
 * yvex_gguf_writer.c - GGUF writer future boundary.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   writer capability refusal while concrete GGUF emission is future-owned.
 *
 * Does not own:
 *   current artifact emission, roundtrip, materialization, runtime descriptor
 *   projection, backend binding, or generation.
 *
 * Invariants:
 *   writer calls fail closed until V010.GGUF.WRITER.0 implements byte emission.
 *
 * Boundary:
 *   this owner records writer refusal only and emits no artifact bytes.
 */
#include "yvex_gguf_private.h"

/* Contract: reports writer support state without writing bytes or files. */
int yvex_gguf_writer_supported(const char **reason)
{
    if (reason) *reason = "GGUF writer is future-owned by V010.GGUF.WRITER.0";
    return 0;
}
