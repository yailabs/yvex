/*
 * yvex_gguf_qtype.c - GGUF qtype byte geometry facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   GGUF qtype ABI names, storage classes, block sizes, bytes-per-block,
 *   scalar widths, storage byte calculation, and qtype refusal facts.
 *
 * Does not own:
 *   backend arithmetic capability, CUDA kernels, qtype policy selection,
 *   source tensor conversion, runtime execution, or generation.
 *
 * Invariants:
 *   byte geometry is structural; execution capability must be proven elsewhere.
 *
 * Boundary:
 *   qtype byte geometry does not imply source conversion, backend execution,
 *   artifact emission, or runtime generation.
 */
#include "yvex_gguf_private.h"

#include <limits.h>
#include <stddef.h>

static const yvex_gguf_qtype_geometry qtype_geometry[] = {
    {0u,  "F32",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,    1u,  4u, 4u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar float width known"},
    {1u,  "F16",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,    1u,  2u, 2u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar float width known"},
    {2u,  "Q4_0",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 18u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {3u,  "Q4_1",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 20u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {6u,  "Q5_0",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 22u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {7u,  "Q5_1",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 24u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {8u,  "Q8_0",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 34u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {9u,  "Q8_1",    YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u, 40u, 0u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF block geometry known"},
    {10u, "Q2_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {11u, "Q3_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {12u, "Q4_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {13u, "Q5_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {14u, "Q6_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {15u, "Q8_K",    YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {16u, "IQ2_XXS", YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {17u, "IQ2_XS",  YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {18u, "IQ3_XXS", YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {19u, "IQ1_S",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {20u, "IQ4_NL",  YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {21u, "IQ3_S",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {22u, "IQ2_S",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {23u, "IQ4_XS",  YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {24u, "I8",      YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,  1u,  1u, 1u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar integer width known"},
    {25u, "I16",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,  1u,  2u, 2u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar integer width known"},
    {26u, "I32",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,  1u,  4u, 4u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar integer width known"},
    {27u, "I64",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,  1u,  8u, 8u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar integer width known"},
    {28u, "F64",     YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,    1u,  8u, 8u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar float width known"},
    {29u, "IQ1_M",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {30u, "BF16",    YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,    1u,  2u, 2u, YVEX_GGUF_QTYPE_STATUS_KNOWN,   "GGUF scalar float width known"},
    {34u, "TQ1_0",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {35u, "TQ2_0",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"},
    {39u, "MXFP4",   YVEX_GGUF_QTYPE_STORAGE_REFUSED,          0u,  0u, 0u, YVEX_GGUF_QTYPE_STATUS_REFUSED, "GGUF qtype byte geometry is not defined in current YVEX ABI table"}
};

/* Contract: names storage classes without allocation, IO, or execution claims. */
const char *yvex_gguf_qtype_storage_class_name(yvex_gguf_qtype_storage_class storage_class)
{
    switch (storage_class) {
    case YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT: return "scalar-floating";
    case YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER: return "scalar-integer";
    case YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED: return "block-quantized";
    case YVEX_GGUF_QTYPE_STORAGE_REFUSED: return "refused";
    case YVEX_GGUF_QTYPE_STORAGE_UNKNOWN:
    default: return "unknown";
    }
}

/* Contract: names qtype geometry status without promoting capability. */
const char *yvex_gguf_qtype_status_name(yvex_gguf_qtype_status status)
{
    switch (status) {
    case YVEX_GGUF_QTYPE_STATUS_KNOWN: return "known-storage-geometry";
    case YVEX_GGUF_QTYPE_STATUS_REFUSED: return "refused";
    case YVEX_GGUF_QTYPE_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_GGUF_QTYPE_STATUS_INVALID: return "invalid-geometry";
    default: return "unknown";
    }
}

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

/* Contract: returns the canonical GGUF qtype name, or UNKNOWN for misses. */
const char *yvex_gguf_qtype_name(unsigned int qtype)
{
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(qtype);

    return geometry && geometry->name ? geometry->name : "UNKNOWN";
}

static int qtype_geometry_valid(const yvex_gguf_qtype_geometry *geometry,
                                const char **reason)
{
    if (!geometry) {
        if (reason) *reason = "unknown GGUF qtype id";
        return 0;
    }
    if (!geometry->name || geometry->name[0] == '\0') {
        if (reason) *reason = "GGUF qtype name is missing";
        return 0;
    }
    if (geometry->status != YVEX_GGUF_QTYPE_STATUS_KNOWN) {
        if (reason) *reason = geometry->reason ? geometry->reason : "GGUF qtype storage geometry refused";
        return 0;
    }
    if (geometry->block_size == 0u) {
        if (reason) *reason = "GGUF qtype block size is zero";
        return 0;
    }
    if (geometry->bytes_per_block == 0u) {
        if (reason) *reason = "GGUF qtype bytes per block is zero";
        return 0;
    }
    if ((geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT ||
         geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER) &&
        geometry->scalar_width == 0u) {
        if (reason) *reason = "GGUF scalar qtype width is zero";
        return 0;
    }
    if (reason) *reason = "GGUF qtype byte geometry accepted";
    return 1;
}

/* Contract: reports whether a qtype has exact storage byte geometry. */
int yvex_gguf_qtype_supported_for_storage(unsigned int qtype, const char **reason)
{
    return qtype_geometry_valid(yvex_gguf_qtype_geometry_find(qtype), reason);
}

/* Contract: computes storage bytes using GGUF scalar width or ceil-block geometry. */
int yvex_gguf_qtype_storage_bytes(unsigned int qtype,
                                  unsigned long long element_count,
                                  unsigned long long *out,
                                  const char **reason)
{
    const yvex_gguf_qtype_geometry *geometry;
    unsigned long long blocks;

    if (!out) {
        if (reason) *reason = "output byte counter is required";
        return 0;
    }
    *out = 0ull;

    geometry = yvex_gguf_qtype_geometry_find(qtype);
    if (!qtype_geometry_valid(geometry, reason)) {
        return 0;
    }

    if (geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT ||
        geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER) {
        if (element_count > ULLONG_MAX / geometry->bytes_per_block) {
            if (reason) *reason = "GGUF scalar qtype storage byte overflow";
            return 0;
        }
        *out = element_count * geometry->bytes_per_block;
        if (reason) *reason = "GGUF scalar qtype storage bytes accepted";
        return 1;
    }

    if (geometry->storage_class != YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED) {
        if (reason) *reason = "GGUF qtype storage class is not byte-countable";
        return 0;
    }

    blocks = element_count / geometry->block_size;
    if ((element_count % geometry->block_size) != 0ull) {
        blocks += 1ull;
    }
    if (blocks > ULLONG_MAX / geometry->bytes_per_block) {
        if (reason) *reason = "GGUF block qtype storage byte overflow";
        return 0;
    }

    *out = blocks * geometry->bytes_per_block;
    if (reason) {
        *reason = (element_count % geometry->block_size) == 0ull
            ? "GGUF block qtype storage bytes accepted"
            : "GGUF block qtype storage bytes accepted with ceil-block padding";
    }
    return 1;
}

/* Contract: compares ABI-visible tensor range bytes against expected storage bytes. */
int yvex_gguf_qtype_validate_tensor_storage(unsigned int qtype,
                                            unsigned long long element_count,
                                            unsigned long long actual_storage_bytes,
                                            unsigned long long *expected_storage_bytes,
                                            const char **reason)
{
    unsigned long long expected = 0ull;

    if (!yvex_gguf_qtype_storage_bytes(qtype, element_count, &expected, reason)) {
        if (expected_storage_bytes) *expected_storage_bytes = 0ull;
        return 0;
    }
    if (expected_storage_bytes) *expected_storage_bytes = expected;
    if (actual_storage_bytes != expected) {
        if (reason) *reason = "GGUF tensor storage byte range does not match qtype geometry";
        return 0;
    }
    if (reason) *reason = "GGUF tensor storage byte range matches qtype geometry";
    return 1;
}

/* Contract: returns a concrete refusal reason for unknown or uncountable qtypes. */
const char *yvex_gguf_qtype_refusal_reason(unsigned int qtype)
{
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(qtype);

    if (!geometry) return "unknown GGUF qtype id";
    if (geometry->status == YVEX_GGUF_QTYPE_STATUS_KNOWN) {
        return "GGUF qtype byte geometry is known";
    }
    return geometry->reason ? geometry->reason : "GGUF qtype byte geometry refused";
}

/* Contract: initializes qtype ABI facts without allocation or rendering. */
void yvex_gguf_qtype_abi_init(yvex_gguf_qtype_abi *abi)
{
    if (!abi) return;
    abi->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    abi->checked_tensor_count = 0ull;
    abi->known_tensor_count = 0ull;
    abi->refused_tensor_count = 0ull;
    abi->total_storage_bytes = 0ull;
    abi->first_refused_qtype = 0u;
    abi->reason = "GGUF qtype ABI not evaluated";
    abi->next_row = YVEX_GGUF_QTYPE_ABI_NEXT_ROW;
}

static int tensor_element_count(const yvex_gguf_tensor_info *tensor,
                                unsigned long long *out,
                                const char **reason)
{
    unsigned long long elements = 1ull;
    unsigned int i;

    if (!tensor || !out) {
        if (reason) *reason = "GGUF tensor_info row is required for qtype storage bytes";
        return 0;
    }
    for (i = 0u; i < tensor->rank; ++i) {
        if (tensor->dims[i] == 0ull) {
            if (reason) *reason = "GGUF tensor dimension is zero";
            return 0;
        }
        if (elements > ULLONG_MAX / tensor->dims[i]) {
            if (reason) *reason = "GGUF tensor element count overflows";
            return 0;
        }
        elements *= tensor->dims[i];
    }
    *out = elements;
    if (reason) *reason = "GGUF tensor element count accepted";
    return 1;
}

/* Contract: summarizes qtype byte geometry over parsed tensor_info rows only. */
int yvex_gguf_qtype_abi_from_gguf(const yvex_gguf *gguf,
                                  yvex_gguf_qtype_abi *abi,
                                  const char **reason)
{
    unsigned long long i;
    unsigned long long count;

    yvex_gguf_qtype_abi_init(abi);
    if (!gguf || !abi) {
        if (reason) *reason = "GGUF qtype ABI requires a parsed GGUF view";
        return 0;
    }

    count = yvex_gguf_tensor_count(gguf);
    for (i = 0ull; i < count; ++i) {
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        unsigned long long elements = 0ull;
        unsigned long long bytes = 0ull;

        if (!tensor || !tensor_element_count(tensor, &elements, reason)) {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = reason ? *reason : "GGUF tensor element count refused";
            return 0;
        }
        abi->checked_tensor_count += 1ull;

        if (!yvex_gguf_qtype_storage_bytes(tensor->ggml_type, elements, &bytes, reason)) {
            abi->refused_tensor_count += 1ull;
            if (abi->first_refused_qtype == 0u) {
                abi->first_refused_qtype = tensor->ggml_type;
            }
            abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            abi->reason = reason ? *reason : yvex_gguf_qtype_refusal_reason(tensor->ggml_type);
            return 0;
        }

        if (abi->total_storage_bytes > ULLONG_MAX - bytes) {
            abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            abi->reason = "GGUF qtype total storage byte count overflows";
            if (reason) *reason = abi->reason;
            return 0;
        }
        abi->total_storage_bytes += bytes;
        abi->known_tensor_count += 1ull;
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF qtype byte geometry accepted";
    if (reason) *reason = abi->reason;
    return 1;
}

/* Contract: copies one qtype geometry row into typed report facts. */
void yvex_gguf_qtype_report_row_from_geometry(const yvex_gguf_qtype_geometry *geometry,
                                              unsigned long long element_count,
                                              yvex_gguf_qtype_report_row *row)
{
    const char *reason = NULL;

    if (!row) return;
    row->qtype = geometry ? geometry->qtype : 0u;
    row->name = geometry && geometry->name ? geometry->name : "UNKNOWN";
    row->storage_class = geometry
        ? yvex_gguf_qtype_storage_class_name(geometry->storage_class)
        : "unknown";
    row->block_size = geometry ? geometry->block_size : 0u;
    row->bytes_per_block = geometry ? geometry->bytes_per_block : 0u;
    row->scalar_width = geometry ? geometry->scalar_width : 0u;
    row->status = geometry
        ? yvex_gguf_qtype_status_name(geometry->status)
        : "unsupported";
    row->expected_storage_bytes = 0ull;
    if (geometry) {
        (void)yvex_gguf_qtype_storage_bytes(geometry->qtype,
                                            element_count,
                                            &row->expected_storage_bytes,
                                            &reason);
    }
    row->reason = reason ? reason : (geometry ? yvex_gguf_qtype_refusal_reason(geometry->qtype)
                                              : "unknown GGUF qtype id");
    row->next_row = YVEX_GGUF_QTYPE_ABI_NEXT_ROW;
}
