/*
 * YVEX - Dtype and qtype registry
 *
 * File: include/yvex/dtype.h
 * Layer: public model API
 *
 * Purpose:
 *   Defines the YVEX-owned dtype/qtype vocabulary and storage-accounting
 *   helpers for raw tensor records. This is byte accounting only; it does not
 *   imply CPU, CUDA, or backend execution support.
 *
 * Owns:
 *   - yvex_dtype
 *   - yvex_dtype_info
 *   - yvex_dtype_get_info
 *   - yvex_dtype_from_ggml_type
 *   - yvex_dtype_storage_bytes
 *
 * Does not own:
 *   - backend kernels
 *   - tensor allocation
 *   - model execution
 *
 * Used by:
 *   - tensor table
 *   - model descriptor
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_dtype
 */
#ifndef YVEX_DTYPE_H
#define YVEX_DTYPE_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_DTYPE_UNKNOWN = 0,

    YVEX_DTYPE_F32,
    YVEX_DTYPE_F16,
    YVEX_DTYPE_BF16,
    YVEX_DTYPE_F64,

    YVEX_DTYPE_I8,
    YVEX_DTYPE_I16,
    YVEX_DTYPE_I32,
    YVEX_DTYPE_I64,

    YVEX_DTYPE_Q4_0,
    YVEX_DTYPE_Q4_1,
    YVEX_DTYPE_Q5_0,
    YVEX_DTYPE_Q5_1,
    YVEX_DTYPE_Q8_0,
    YVEX_DTYPE_Q8_1,

    YVEX_DTYPE_Q2_K,
    YVEX_DTYPE_Q3_K,
    YVEX_DTYPE_Q4_K,
    YVEX_DTYPE_Q5_K,
    YVEX_DTYPE_Q6_K,
    YVEX_DTYPE_Q8_K,

    YVEX_DTYPE_IQ2_XXS,
    YVEX_DTYPE_IQ2_XS,
    YVEX_DTYPE_IQ3_XXS,
    YVEX_DTYPE_IQ1_S,
    YVEX_DTYPE_IQ4_NL,
    YVEX_DTYPE_IQ3_S,
    YVEX_DTYPE_IQ2_S,
    YVEX_DTYPE_IQ4_XS,
    YVEX_DTYPE_IQ1_M,

    YVEX_DTYPE_TQ1_0,
    YVEX_DTYPE_TQ2_0,
    YVEX_DTYPE_MXFP4
} yvex_dtype;

typedef struct {
    yvex_dtype dtype;
    const char *name;
    unsigned int ggml_type;
    unsigned int block_elems;
    unsigned int block_bytes;
    unsigned int scalar_bytes;
    int is_quantized;
    int is_supported_for_storage_accounting;
} yvex_dtype_info;

const yvex_dtype_info *yvex_dtype_get_info(yvex_dtype dtype);
const yvex_dtype_info *yvex_dtype_from_ggml_type(unsigned int ggml_type);
const char *yvex_dtype_name(yvex_dtype dtype);

int yvex_dtype_storage_bytes(yvex_dtype dtype,
                             unsigned long long element_count,
                             unsigned long long *out,
                             yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_DTYPE_H */
