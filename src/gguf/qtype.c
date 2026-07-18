/*
 * qtype.c - canonical GGUF qtype storage ABI.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   pinned GGUF qtype identity and admission, scalar/block storage geometry,
 *   row-aware tensor byte calculation, and typed storage refusal facts.
 *
 * Does not own:
 *   reference decoders, quantizers, qtype policy,
 *   artifact emission, backend arithmetic, materialization, or runtime.
 *
 * Invariants:
 *   dimension zero is the GGML row width; block rows divide exactly; storage
 *   admission never implies a decoder, emitter, quantizer, or compute kernel.
 *
 * Boundary:
 *   this owner reads tensor geometry only and never reads tensor payload bytes.
 */
#include "private.h"
#include "quant_numeric.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static const yvex_gguf_qtype_geometry qtype_geometry[] = {
    {0u,  "F32",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,     1u,   4u, 4u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {1u,  "F16",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,     1u,   2u, 2u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {2u,  "Q4_0",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  18u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {3u,  "Q4_1",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  20u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {4u,  "Q4_2",         YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {5u,  "Q4_3",         YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {6u,  "Q5_0",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  22u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {7u,  "Q5_1",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  24u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {8u,  "Q8_0",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  34u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {9u,  "Q8_1",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  36u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {10u, "Q2_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 84u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {11u, "Q3_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 110u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {12u, "Q4_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 144u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {13u, "Q5_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 176u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {14u, "Q6_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 210u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {15u, "Q8_K",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 292u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {16u, "IQ2_XXS",      YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 66u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {17u, "IQ2_XS",       YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 74u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {18u, "IQ3_XXS",      YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 98u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {19u, "IQ1_S",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 50u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {20u, "IQ4_NL",       YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  18u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {21u, "IQ3_S",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 110u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {22u, "IQ2_S",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 82u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {23u, "IQ4_XS",       YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 136u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {24u, "I8",           YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,   1u,   1u, 1u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {25u, "I16",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,   1u,   2u, 2u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {26u, "I32",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,   1u,   4u, 4u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {27u, "I64",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER,   1u,   8u, 8u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {28u, "F64",          YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,     1u,   8u, 8u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {29u, "IQ1_M",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 56u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {30u, "BF16",         YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT,     1u,   2u, 2u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {31u, "Q4_0_4_4",     YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {32u, "Q4_0_4_8",     YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {33u, "Q4_0_8_8",     YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {34u, "TQ1_0",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 54u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {35u, "TQ2_0",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 256u, 66u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {36u, "IQ4_NL_4_4",   YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {37u, "IQ4_NL_4_8",   YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {38u, "IQ4_NL_8_8",   YVEX_GGUF_QTYPE_IDENTITY_REMOVED,          YVEX_GGUF_QTYPE_STORAGE_UNKNOWN,          0u,   0u, 0u, 0, "qtype ID was removed from the pinned GGUF ABI"},
    {39u, "MXFP4",        YVEX_GGUF_QTYPE_IDENTITY_ADMITTED,         YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 32u,  17u, 0u, 0, "storage geometry admitted by pinned GGUF ABI"},
    {40u, "NVFP4",        YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 64u,  36u, 0u, 0, "qtype identity is newer than the pinned GGUF on-disk baseline"},
    {41u, "Q1_0",         YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 128u, 18u, 0u, 0, "qtype identity is newer than the pinned GGUF on-disk baseline"},
    {42u, "Q2_0",         YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE, YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED, 64u,  18u, 0u, 0, "qtype identity is newer than the pinned GGUF on-disk baseline"}
};

_Static_assert(sizeof(qtype_geometry) / sizeof(qtype_geometry[0]) == 43u,
               "pinned qtype registry must account for IDs 0 through 42");

/* Contract: returns the number of pinned implementation identity records. */
size_t yvex_gguf_qtype_geometry_count(void)
{
    return sizeof(qtype_geometry) / sizeof(qtype_geometry[0]);
}

/* Contract: borrows one immutable registry row or returns NULL on bounds miss. */
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_at(size_t index)
{
    if (index >= yvex_gguf_qtype_geometry_count()) return NULL;
    return &qtype_geometry[index];
}

/* Contract: borrows one immutable registry row by numeric identity. */
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_find(unsigned int qtype)
{
    if (qtype > YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID) return NULL;
    return &qtype_geometry[qtype];
}

/* Contract: borrows one immutable registry row by exact canonical name. */
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_find_by_name(const char *name)
{
    size_t i;

    if (!name) return NULL;
    for (i = 0u; i < yvex_gguf_qtype_geometry_count(); ++i) {
        if (strcmp(qtype_geometry[i].name, name) == 0) return &qtype_geometry[i];
    }
    return NULL;
}

/* Contract: returns the canonical pinned name without allocating or inferring. */
const char *yvex_gguf_qtype_name(unsigned int qtype)
{
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(qtype);

    return geometry ? geometry->name : "UNKNOWN";
}

/* Contract: names identity admission without changing storage capability. */
const char *yvex_gguf_qtype_identity_status_name(yvex_gguf_qtype_identity_status status)
{
    switch (status) {
    case YVEX_GGUF_QTYPE_IDENTITY_ADMITTED: return "admitted";
    case YVEX_GGUF_QTYPE_IDENTITY_REMOVED: return "removed";
    case YVEX_GGUF_QTYPE_IDENTITY_RESERVED: return "reserved";
    case YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE: return "outside-baseline";
    case YVEX_GGUF_QTYPE_IDENTITY_UNKNOWN: return "unknown";
    }
    return "unknown";
}

/* Contract: names structural storage class without compute claims. */
const char *yvex_gguf_qtype_storage_class_name(yvex_gguf_qtype_storage_class storage_class)
{
    switch (storage_class) {
    case YVEX_GGUF_QTYPE_STORAGE_SCALAR_FLOAT: return "scalar-floating";
    case YVEX_GGUF_QTYPE_STORAGE_SCALAR_INTEGER: return "scalar-integer";
    case YVEX_GGUF_QTYPE_STORAGE_BLOCK_QUANTIZED: return "block-quantized";
    case YVEX_GGUF_QTYPE_STORAGE_UNKNOWN: return "unknown";
    }
    return "unknown";
}

/* Contract: names typed storage results without rendering or side effects. */
const char *yvex_gguf_qtype_storage_status_name(yvex_gguf_qtype_storage_status status)
{
    switch (status) {
    case YVEX_GGUF_QTYPE_STORAGE_OK: return "ok";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID: return "unknown-qtype-id";
    case YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID: return "removed-qtype-id";
    case YVEX_GGUF_QTYPE_STORAGE_RESERVED_ID: return "reserved-qtype-id";
    case YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE: return "outside-on-disk-baseline";
    case YVEX_GGUF_QTYPE_STORAGE_GEOMETRY_UNAVAILABLE: return "storage-geometry-unavailable";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK: return "invalid-rank";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION: return "invalid-dimension";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH: return "row-width-block-mismatch";
    case YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW: return "element-count-overflow";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW: return "row-byte-overflow";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW: return "row-count-overflow";
    case YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW: return "total-byte-overflow";
    case YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH: return "expected-actual-storage-mismatch";
    }
    return "unknown-storage-status";
}

static yvex_gguf_qtype_storage_status qtype_admission_status(
    const yvex_gguf_qtype_geometry *geometry)
{
    if (!geometry) return YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID;
    switch (geometry->identity_status) {
    case YVEX_GGUF_QTYPE_IDENTITY_ADMITTED:
        break;
    case YVEX_GGUF_QTYPE_IDENTITY_REMOVED:
        return YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID;
    case YVEX_GGUF_QTYPE_IDENTITY_RESERVED:
        return YVEX_GGUF_QTYPE_STORAGE_RESERVED_ID;
    case YVEX_GGUF_QTYPE_IDENTITY_OUTSIDE_BASELINE:
        return YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE;
    case YVEX_GGUF_QTYPE_IDENTITY_UNKNOWN:
        return YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID;
    }
    if (geometry->storage_class == YVEX_GGUF_QTYPE_STORAGE_UNKNOWN ||
        geometry->block_size == 0u || geometry->bytes_per_block == 0u) {
        return YVEX_GGUF_QTYPE_STORAGE_GEOMETRY_UNAVAILABLE;
    }
    return YVEX_GGUF_QTYPE_STORAGE_OK;
}

static const char *storage_status_reason(yvex_gguf_qtype_storage_status status)
{
    switch (status) {
    case YVEX_GGUF_QTYPE_STORAGE_OK:
        return "GGUF tensor row geometry accepted";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT:
        return "qtype storage requires dimensions and an output result";
    case YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID:
        return "unknown GGUF qtype ID";
    case YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID:
        return "GGUF qtype ID was removed from the pinned on-disk ABI";
    case YVEX_GGUF_QTYPE_STORAGE_RESERVED_ID:
        return "GGUF qtype ID is reserved by the pinned on-disk ABI";
    case YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE:
        return "GGUF qtype ID is outside the pinned on-disk ABI";
    case YVEX_GGUF_QTYPE_STORAGE_GEOMETRY_UNAVAILABLE:
        return "admitted GGUF qtype has no storage geometry";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK:
        return "GGUF tensor rank is outside the admitted range";
    case YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION:
        return "GGUF tensor dimension is zero";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH:
        return "GGUF row width is not divisible by qtype block width";
    case YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW:
        return "GGUF tensor element count overflows";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW:
        return "GGUF qtype row byte count overflows";
    case YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW:
        return "GGUF tensor row count overflows";
    case YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW:
        return "GGUF tensor total byte count overflows";
    case YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH:
        return "GGUF tensor storage bytes do not match qtype row geometry";
    }
    return "unknown GGUF qtype storage refusal";
}

static yvex_gguf_qtype_storage_status set_storage_status(
    yvex_gguf_qtype_storage_result *out,
    yvex_gguf_qtype_storage_status status)
{
    if (out) {
        out->status = status;
        out->reason = storage_status_reason(status);
    }
    return status;
}

static int checked_multiply(unsigned long long a,
                            unsigned long long b,
                            unsigned long long *out)
{
    if (!out) return 0;
    if (a != 0ull && b > ULLONG_MAX / a) return 0;
    *out = a * b;
    return 1;
}

/*
 * Contract: calculates one contiguous tensor from ne[0] row width and the
 * product of remaining dimensions. It mutates only out, performs no IO or
 * allocation, and returns a typed refusal for every invalid identity, shape,
 * row, or arithmetic boundary.
 */
yvex_gguf_qtype_storage_status yvex_gguf_qtype_tensor_storage(
    unsigned int qtype,
    const unsigned long long *dims,
    unsigned int rank,
    yvex_gguf_qtype_storage_result *out)
{
    const yvex_gguf_qtype_geometry *geometry;
    yvex_gguf_qtype_storage_status status;
    unsigned long long blocks;
    unsigned int i;

    if (!out) return YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT;
    memset(out, 0, sizeof(*out));
    out->qtype = qtype;
    out->rank = rank;
    out->status = YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT;
    out->reason = storage_status_reason(out->status);
    if (!dims) return out->status;

    geometry = yvex_gguf_qtype_geometry_find(qtype);
    status = qtype_admission_status(geometry);
    if (status != YVEX_GGUF_QTYPE_STORAGE_OK) {
        return set_storage_status(out, status);
    }
    if (rank == 0u || rank > YVEX_GGUF_QTYPE_MAX_DIMS) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK);
    }
    for (i = 0u; i < rank; ++i) {
        if (dims[i] == 0ull) {
            return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION);
        }
    }

    out->row_width = dims[0];
    if ((out->row_width % geometry->block_size) != 0ull) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH);
    }
    blocks = out->row_width / geometry->block_size;
    if (!checked_multiply(blocks, geometry->bytes_per_block, &out->row_bytes)) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW);
    }

    out->row_count = 1ull;
    for (i = 1u; i < rank; ++i) {
        if (!checked_multiply(out->row_count, dims[i], &out->row_count)) {
            return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW);
        }
    }
    if (!checked_multiply(out->row_width, out->row_count, &out->element_count)) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW);
    }
    if (!checked_multiply(out->row_bytes, out->row_count, &out->total_bytes)) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW);
    }
    return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_OK);
}

/* Contract: validates an ABI-visible span against the same row-aware result. */
yvex_gguf_qtype_storage_status yvex_gguf_qtype_validate_tensor_storage(
    unsigned int qtype,
    const unsigned long long *dims,
    unsigned int rank,
    unsigned long long actual_storage_bytes,
    yvex_gguf_qtype_storage_result *out)
{
    yvex_gguf_qtype_storage_status status;

    status = yvex_gguf_qtype_tensor_storage(qtype, dims, rank, out);
    if (!out || status != YVEX_GGUF_QTYPE_STORAGE_OK) return status;
    out->actual_bytes = actual_storage_bytes;
    if (out->total_bytes != actual_storage_bytes) {
        return set_storage_status(out, YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH);
    }
    return status;
}

/* Contract: reports whether the pinned on-disk ABI admits exact geometry. */
int yvex_gguf_qtype_supported_for_storage(unsigned int qtype, const char **reason)
{
    yvex_gguf_qtype_storage_status status =
        qtype_admission_status(yvex_gguf_qtype_geometry_find(qtype));

    if (reason) *reason = storage_status_reason(status);
    return status == YVEX_GGUF_QTYPE_STORAGE_OK;
}

/* Contract: reports YVEX decoder availability independently from geometry. */
int yvex_gguf_qtype_reference_dequantization_supported(unsigned int qtype)
{
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_at(qtype);

    return capability && capability->reference_decoder_available;
}

/* Contract: returns a stable identity/admission refusal without calculation. */
const char *yvex_gguf_qtype_refusal_reason(unsigned int qtype)
{
    yvex_gguf_qtype_storage_status status =
        qtype_admission_status(yvex_gguf_qtype_geometry_find(qtype));

    return storage_status_reason(status);
}

/*
 * Contract: compatibility calculation for one logical row. Multidimensional
 * callers must use yvex_gguf_qtype_tensor_storage; partial blocks fail closed.
 */
int yvex_gguf_qtype_storage_bytes(unsigned int qtype,
                                  unsigned long long row_element_count,
                                  unsigned long long *out,
                                  const char **reason)
{
    yvex_gguf_qtype_storage_result result;
    yvex_gguf_qtype_storage_status status;

    if (!out) {
        if (reason) *reason = storage_status_reason(YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT);
        return 0;
    }
    *out = 0ull;
    status = yvex_gguf_qtype_tensor_storage(qtype, &row_element_count, 1u, &result);
    if (reason) *reason = result.reason;
    if (status != YVEX_GGUF_QTYPE_STORAGE_OK) return 0;
    *out = result.total_bytes;
    return 1;
}

/* Contract: initializes qtype ABI aggregation without allocation or IO. */
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

/* Contract: aggregates canonical row-aware geometry over parsed tensor infos. */
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
        yvex_gguf_qtype_storage_result storage;

        if (!tensor) {
            abi->status = YVEX_GGUF_ABI_SECTION_MALFORMED;
            abi->reason = "GGUF tensor_info row is missing";
            if (reason) *reason = abi->reason;
            return 0;
        }
        abi->checked_tensor_count += 1ull;
        if (yvex_gguf_qtype_tensor_storage(tensor->ggml_type,
                                           tensor->dims,
                                           tensor->rank,
                                           &storage) != YVEX_GGUF_QTYPE_STORAGE_OK) {
            abi->refused_tensor_count += 1ull;
            if (abi->refused_tensor_count == 1ull) {
                abi->first_refused_qtype = tensor->ggml_type;
            }
            abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            abi->reason = storage.reason;
            if (reason) *reason = abi->reason;
            return 0;
        }
        if (abi->total_storage_bytes > ULLONG_MAX - storage.total_bytes) {
            abi->status = YVEX_GGUF_ABI_SECTION_REFUSED;
            abi->reason = "GGUF aggregate qtype storage bytes overflow";
            if (reason) *reason = abi->reason;
            return 0;
        }
        abi->total_storage_bytes += storage.total_bytes;
        abi->known_tensor_count += 1ull;
    }

    abi->status = YVEX_GGUF_ABI_SECTION_OK;
    abi->reason = "GGUF qtype row-aware storage ABI accepted";
    if (reason) *reason = abi->reason;
    return 1;
}

/* Contract: projects canonical geometry and one borrowed shape into report facts. */
void yvex_gguf_qtype_report_row_from_geometry(const yvex_gguf_qtype_geometry *geometry,
                                              const unsigned long long *dims,
                                              unsigned int rank,
                                              yvex_gguf_qtype_report_row *row)
{
    yvex_gguf_qtype_storage_result storage;

    if (!row) return;
    memset(row, 0, sizeof(*row));
    row->qtype = geometry ? geometry->qtype : 0u;
    row->name = geometry ? geometry->name : "UNKNOWN";
    row->identity_status = geometry
        ? yvex_gguf_qtype_identity_status_name(geometry->identity_status)
        : "unknown";
    row->storage_class = geometry
        ? yvex_gguf_qtype_storage_class_name(geometry->storage_class)
        : "unknown";
    row->block_size = geometry ? geometry->block_size : 0u;
    row->bytes_per_block = geometry ? geometry->bytes_per_block : 0u;
    row->scalar_width = geometry ? geometry->scalar_width : 0u;
    row->reference_dequantization = geometry &&
        yvex_gguf_qtype_reference_dequantization_supported(geometry->qtype)
        ? "available"
        : "unavailable";
    if (!geometry) {
        row->storage_status = "unknown-qtype-id";
        row->reason = storage_status_reason(YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID);
    } else {
        (void)yvex_gguf_qtype_tensor_storage(geometry->qtype, dims, rank, &storage);
        row->storage_status = yvex_gguf_qtype_storage_status_name(storage.status);
        row->expected_storage_bytes = storage.total_bytes;
        row->reason = storage.reason;
    }
    row->next_row = YVEX_GGUF_QTYPE_ABI_NEXT_ROW;
}
