/*
 * yvex_gguf_reader.c - GGUF reader boundary over parser facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   reader status facts and parse-refusal propagation for existing parser
 *   results.
 *
 * Does not own:
 *   writer emission, roundtrip, artifact emission, materialization, runtime
 *   descriptor projection, or generation.
 *
 * Invariants:
 *   reader state mirrors parser outcomes and does not invent capability.
 *
 * Boundary:
 *   reader acceptance is not writer roundtrip, materialization, or runtime
 *   support.
 */
#include "yvex_gguf_private.h"

static const yvex_gguf_boundary_fact reader_boundary = {
    "src/gguf/yvex_gguf_reader.c",
    "GGUF reader",
    YVEX_GGUF_BOUNDARY_REPORT_ONLY,
    "reader facts wrap existing parser outcomes",
    "V010.GGUF.ARTIFACT.ABI.0"
};

/* Contract: exposes reader boundary facts without opening files. */
const yvex_gguf_boundary_fact *yvex_gguf_reader_boundary(void)
{
    return &reader_boundary;
}

/* Contract: maps parser return codes to reader refusal state. */
int yvex_gguf_reader_parse_refusal(int parse_rc, const char **reason)
{
    if (parse_rc == 0) {
        if (reason) *reason = "GGUF parser accepted directory facts";
        return 0;
    }
    if (reason) *reason = "GGUF parser refused input";
    return 1;
}
