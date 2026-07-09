/*
 * yvex_gguf_container.c - GGUF container ABI boundary facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   GGUF magic/version/container ABI facts and version refusal state.
 *
 * Does not own:
 *   metadata key/value ABI, tensor_info tables, writer emission, roundtrip,
 *   artifact descriptors, runtime descriptors, or tensor payload loading.
 *
 * Invariants:
 *   unsupported container versions refuse with a concrete reason.
 *
 * Boundary:
 *   container ABI facts do not imply metadata, tensor_info, writer, roundtrip,
 *   materialization, runtime, or generation support.
 */
#include "yvex_gguf_private.h"

static const yvex_gguf_boundary_fact gguf_container_boundary = {
    "src/gguf/yvex_gguf_container.c",
    "GGUF container ABI",
    YVEX_GGUF_BOUNDARY_REPORT_ONLY,
    "container facts are separated from metadata and tensor_info ownership",
    "V010.GGUF.ARTIFACT.ABI.0"
};

/* Contract: returns a stable name for internal GGUF boundary status values. */
const char *yvex_gguf_boundary_status_name(yvex_gguf_boundary_status status)
{
    switch (status) {
        case YVEX_GGUF_BOUNDARY_REPORT_ONLY:
            return "report-only";
        case YVEX_GGUF_BOUNDARY_UNSUPPORTED:
            return "unsupported";
        case YVEX_GGUF_BOUNDARY_REFUSED:
            return "refused";
    }
    return "refused";
}

/* Contract: exposes the container ABI boundary fact without allocation or IO. */
const yvex_gguf_boundary_fact *yvex_gguf_container_boundary(void)
{
    return &gguf_container_boundary;
}

/* Contract: validates only the container version boundary and reports refusal. */
int yvex_gguf_container_version_supported(unsigned int version, const char **reason)
{
    if (version == 2u || version == 3u) {
        if (reason) *reason = "container version accepted by current parser boundary";
        return 1;
    }
    if (reason) *reason = "unsupported GGUF container version";
    return 0;
}
