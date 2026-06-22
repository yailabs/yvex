/*
 * YVEX - Dtype and qtype registry
 *
 * File: src/model/dtype.c
 * Layer: model implementation
 *
 * Purpose:
 *   Implements YVEX dtype/qtype name mapping, GGML raw type mapping, and
 *   storage byte accounting for the initial supported storage formulas.
 *
 * Implements:
 *   - yvex_dtype_get_info
 *   - yvex_dtype_from_ggml_type
 *   - yvex_dtype_name
 *   - yvex_dtype_storage_bytes
 *
 * Invariants:
 *   - byte accounting does not imply execution support
 *   - unsupported formulas fail explicitly
 *   - overflow is checked before multiplication
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_dtype
 */
#include <yvex/dtype.h>

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

static const yvex_dtype_info dtype_table[] = {
    {YVEX_DTYPE_UNKNOWN, "UNKNOWN",  UINT_MAX, 0,   0,   0, 0, 0},
    {YVEX_DTYPE_F32,     "F32",      0,        0,   0,   4, 0, 1},
    {YVEX_DTYPE_F16,     "F16",      1,        0,   0,   2, 0, 1},
    {YVEX_DTYPE_BF16,    "BF16",     30,       0,   0,   2, 0, 1},
    {YVEX_DTYPE_F64,     "F64",      28,       0,   0,   8, 0, 1},
    {YVEX_DTYPE_I8,      "I8",       24,       0,   0,   1, 0, 1},
    {YVEX_DTYPE_I16,     "I16",      25,       0,   0,   2, 0, 1},
    {YVEX_DTYPE_I32,     "I32",      26,       0,   0,   4, 0, 1},
    {YVEX_DTYPE_I64,     "I64",      27,       0,   0,   8, 0, 1},
    {YVEX_DTYPE_Q4_0,    "Q4_0",     2,        32,  18,  0, 1, 1},
    {YVEX_DTYPE_Q4_1,    "Q4_1",     3,        32,  20,  0, 1, 0},
    {YVEX_DTYPE_Q5_0,    "Q5_0",     6,        32,  22,  0, 1, 0},
    {YVEX_DTYPE_Q5_1,    "Q5_1",     7,        32,  24,  0, 1, 0},
    {YVEX_DTYPE_Q8_0,    "Q8_0",     8,        32,  34,  0, 1, 1},
    {YVEX_DTYPE_Q8_1,    "Q8_1",     9,        32,  40,  0, 1, 0},
    {YVEX_DTYPE_Q2_K,    "Q2_K",     10,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q3_K,    "Q3_K",     11,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q4_K,    "Q4_K",     12,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q5_K,    "Q5_K",     13,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q6_K,    "Q6_K",     14,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_Q8_K,    "Q8_K",     15,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_XXS, "IQ2_XXS",  16,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_XS,  "IQ2_XS",   17,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ3_XXS, "IQ3_XXS",  18,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ1_S,   "IQ1_S",    19,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ4_NL,  "IQ4_NL",   20,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ3_S,   "IQ3_S",    21,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ2_S,   "IQ2_S",    22,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ4_XS,  "IQ4_XS",   23,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_IQ1_M,   "IQ1_M",    29,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_TQ1_0,   "TQ1_0",    34,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_TQ2_0,   "TQ2_0",    35,       0,   0,   0, 1, 0},
    {YVEX_DTYPE_MXFP4,   "MXFP4",    39,       0,   0,   0, 1, 0},
};

static const unsigned long dtype_table_count = sizeof(dtype_table) / sizeof(dtype_table[0]);

const yvex_dtype_info *yvex_dtype_get_info(yvex_dtype dtype)
{
    unsigned long i;

    for (i = 0; i < dtype_table_count; ++i) {
        if (dtype_table[i].dtype == dtype) {
            return &dtype_table[i];
        }
    }

    return &dtype_table[0];
}

const yvex_dtype_info *yvex_dtype_from_ggml_type(unsigned int ggml_type)
{
    unsigned long i;

    for (i = 0; i < dtype_table_count; ++i) {
        if (dtype_table[i].ggml_type == ggml_type) {
            return &dtype_table[i];
        }
    }

    return &dtype_table[0];
}

const char *yvex_dtype_name(yvex_dtype dtype)
{
    return yvex_dtype_get_info(dtype)->name;
}

int yvex_dtype_storage_bytes(yvex_dtype dtype,
                             unsigned long long element_count,
                             unsigned long long *out,
                             yvex_error *err)
{
    const yvex_dtype_info *info;
    unsigned long long blocks;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_dtype_storage_bytes", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = 0;

    info = yvex_dtype_get_info(dtype);
    if (!info || info->dtype == YVEX_DTYPE_UNKNOWN || !info->is_supported_for_storage_accounting) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_dtype_storage_bytes",
                        "storage accounting unsupported for dtype %s", yvex_dtype_name(dtype));
        return YVEX_ERR_UNSUPPORTED;
    }

    if (info->scalar_bytes > 0) {
        if (element_count > ULLONG_MAX / info->scalar_bytes) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_dtype_storage_bytes",
                            "storage byte overflow for dtype %s", info->name);
            return YVEX_ERR_BOUNDS;
        }
        *out = element_count * info->scalar_bytes;
        yvex_error_clear(err);
        return YVEX_OK;
    }

    if (info->block_elems == 0 || info->block_bytes == 0) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_dtype_storage_bytes",
                        "block formula missing for dtype %s", info->name);
        return YVEX_ERR_UNSUPPORTED;
    }

    blocks = element_count / info->block_elems;
    if ((element_count % info->block_elems) != 0) {
        blocks += 1;
    }
    if (blocks > ULLONG_MAX / info->block_bytes) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_dtype_storage_bytes",
                        "block storage byte overflow for dtype %s", info->name);
        return YVEX_ERR_BOUNDS;
    }

    *out = blocks * info->block_bytes;
    yvex_error_clear(err);
    return YVEX_OK;
}
