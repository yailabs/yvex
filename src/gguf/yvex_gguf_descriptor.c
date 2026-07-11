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
 *   descriptor acceptance requires container, metadata, tensor_info, qtype,
 *   and range facts.
 *
 * Boundary:
 *   a GGUF descriptor is not a runtime descriptor or execution plan.
 */
#include "yvex_gguf_private.h"

static const yvex_gguf_boundary_fact descriptor_boundary = {
    "src/gguf/yvex_gguf_descriptor.c",
    "GGUF descriptor",
    YVEX_GGUF_BOUNDARY_OPERATIONAL,
    "structural descriptor facts project the canonical parsed view without runtime promotion",
    YVEX_GGUF_ABI_NEXT_ROW
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

/* Contract: derives a GGUF-file-contained descriptor fact from ABI sections. */
void yvex_gguf_descriptor_abi_from_sections(const yvex_gguf_container_abi *container,
                                            const yvex_gguf_metadata_abi *metadata,
                                            const yvex_gguf_tensor_info_abi *tensor_info,
                                            const yvex_gguf_qtype_abi *qtype,
                                            const yvex_gguf_range_fact *range,
                                            yvex_gguf_descriptor_abi *descriptor)
{
    if (!descriptor) return;
    descriptor->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    descriptor->reason = "GGUF descriptor not evaluated";

    if (!container || !metadata || !tensor_info || !qtype || !range) {
        descriptor->status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        descriptor->reason = "GGUF descriptor requires all ABI sections";
        return;
    }

    if (container->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = container->status;
        descriptor->reason = "GGUF descriptor blocked by container ABI";
        return;
    }
    if (metadata->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = metadata->status;
        descriptor->reason = "GGUF descriptor blocked by metadata ABI";
        return;
    }
    if (tensor_info->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = tensor_info->status;
        descriptor->reason = "GGUF descriptor blocked by tensor_info ABI";
        return;
    }
    if (qtype->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = qtype->status;
        descriptor->reason = "GGUF descriptor blocked by qtype byte geometry ABI";
        return;
    }
    if (range->status != YVEX_GGUF_ABI_SECTION_OK) {
        descriptor->status = range->status;
        descriptor->reason = "GGUF descriptor blocked by range ABI";
        return;
    }

    descriptor->status = YVEX_GGUF_ABI_SECTION_OK;
    descriptor->reason = "GGUF structural descriptor accepted; complete artifact integrity remains blocked";
}
