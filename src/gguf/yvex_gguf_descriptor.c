/*
 * yvex_gguf_descriptor.c - GGUF artifact descriptor facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   GGUF-contained descriptor facts and refusal when required ABI facts are
 *   incomplete.
 *
 * Does not own:
 *   YVEX artifact acceptance, runtime descriptor projection, backend binding,
 *   graph binding, or generation.
 *
 * Invariants:
 *   descriptor acceptance requires container, metadata, and tensor_info facts.
 *
 * Boundary:
 *   a GGUF descriptor is not a runtime descriptor or execution plan.
 */
#include "yvex_gguf_private.h"

static const yvex_gguf_boundary_fact descriptor_boundary = {
    "src/gguf/yvex_gguf_descriptor.c",
    "GGUF descriptor",
    YVEX_GGUF_BOUNDARY_UNSUPPORTED,
    "descriptor acceptance waits for full GGUF ABI closure",
    "V010.GGUF.ARTIFACT.ABI.0"
};

/* Contract: exposes descriptor boundary facts without allocation or IO. */
const yvex_gguf_boundary_fact *yvex_gguf_descriptor_boundary(void)
{
    return &descriptor_boundary;
}

/* Contract: accepts only complete ABI facts and otherwise refuses precisely. */
int yvex_gguf_descriptor_accepts_abi(int container_ok,
                                     int metadata_ok,
                                     int tensor_info_ok,
                                     const char **reason)
{
    if (container_ok && metadata_ok && tensor_info_ok) {
        if (reason) *reason = "GGUF ABI facts complete for descriptor projection";
        return 1;
    }
    if (reason) *reason = "GGUF descriptor requires container, metadata, and tensor_info facts";
    return 0;
}
