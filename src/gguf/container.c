/*
 * container.c - GGUF container ABI boundary facts.
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
#include "private.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static const yvex_gguf_boundary_fact gguf_container_boundary = {
    "src/gguf/container.c",
    "GGUF container ABI",
    YVEX_GGUF_BOUNDARY_OPERATIONAL,
    "container facts are consumed by the canonical structural reader",
    YVEX_GGUF_ABI_NEXT_ROW
};

/* Contract: returns a stable name for internal GGUF boundary status values. */
const char *yvex_gguf_boundary_status_name(yvex_gguf_boundary_status status)
{
    switch (status) {
        case YVEX_GGUF_BOUNDARY_OPERATIONAL:
            return "operational";
        case YVEX_GGUF_BOUNDARY_REPORT_ONLY:
            return "report-only";
        case YVEX_GGUF_BOUNDARY_UNSUPPORTED:
            return "unsupported";
        case YVEX_GGUF_BOUNDARY_REFUSED:
            return "refused";
    }
    return "refused";
}

/* Contract: returns stable GGUF ABI section status names for reports/tests. */
const char *yvex_gguf_abi_section_status_name(yvex_gguf_abi_section_status status)
{
    switch (status) {
        case YVEX_GGUF_ABI_SECTION_NOT_EVALUATED:
            return "not-evaluated";
        case YVEX_GGUF_ABI_SECTION_OK:
            return "ok";
        case YVEX_GGUF_ABI_SECTION_REPORT_ONLY:
            return "report-only";
        case YVEX_GGUF_ABI_SECTION_REFUSED:
            return "refused";
        case YVEX_GGUF_ABI_SECTION_UNSUPPORTED:
            return "unsupported";
        case YVEX_GGUF_ABI_SECTION_MALFORMED:
            return "malformed";
        case YVEX_GGUF_ABI_SECTION_NOT_PRESENT:
            return "not-present";
    }
    return "refused";
}

/* Contract: exposes the container ABI boundary fact without allocation or IO. */
const yvex_gguf_boundary_fact *yvex_gguf_container_boundary(void)
{
    return &gguf_container_boundary;
}

/* Contract: initializes container ABI facts to fail-closed not-evaluated state. */
void yvex_gguf_container_abi_init(yvex_gguf_container_abi *abi)
{
    if (!abi) return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->magic = 0u;
    abi->version = 0u;
    abi->metadata_count = 0ull;
    abi->tensor_count = 0ull;
    abi->reason = "GGUF container ABI not evaluated";
}

/* Contract: validates only the GGUF magic ABI field. */
int yvex_gguf_container_magic_supported(unsigned int magic, const char **reason)
{
    if (magic == YVEX_GGUF_MAGIC) {
        if (reason) *reason = "GGUF magic accepted";
        return 1;
    }
    if (reason) *reason = "invalid GGUF magic";
    return 0;
}

/* Contract: validates only the container version boundary and reports refusal. */
int yvex_gguf_container_version_supported(unsigned int version, const char **reason)
{
    if (version == 3u) {
        if (reason) *reason = "container version accepted by current parser boundary";
        return 1;
    }
    if (reason) *reason = "unsupported GGUF container version";
    return 0;
}

/* Contract: validates parser-visible count fields against allocation overflow. */
int yvex_gguf_container_counts_supported(unsigned long long metadata_count,
                                         unsigned long long tensor_count,
                                         const char **reason)
{
    if (metadata_count > (unsigned long long)(SIZE_MAX / sizeof(void *))) {
        if (reason) *reason = "metadata count exceeds parser allocation boundary";
        return 0;
    }
    if (tensor_count > (unsigned long long)(SIZE_MAX / sizeof(void *))) {
        if (reason) *reason = "tensor count exceeds parser allocation boundary";
        return 0;
    }
    if (reason) *reason = "GGUF metadata and tensor counts accepted";
    return 1;
}

/* Contract: projects a parsed GGUF header into typed container ABI facts. */
void yvex_gguf_container_abi_from_header(const yvex_gguf_header *header,
                                         yvex_gguf_container_abi *abi)
{
    const char *reason = NULL;

    yvex_gguf_container_abi_init(abi);
    if (!abi) return;
    if (!header) {
        abi->status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        abi->reason = "GGUF header not present";
        return;
    }

    abi->magic = YVEX_GGUF_MAGIC;
    abi->version = header->version;
    abi->metadata_count = header->metadata_count;
    abi->tensor_count = header->tensor_count;

    if (!yvex_gguf_container_version_supported(header->version, &reason)) {
        abi->status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
        abi->reason = reason;
        return;
    }
    if (!yvex_gguf_container_counts_supported(header->metadata_count, header->tensor_count, &reason)) {
        abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
        abi->reason = reason;
        return;
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = reason;
}
