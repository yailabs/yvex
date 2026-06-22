/*
 * YVEX - Tensor table
 *
 * File: include/yvex/tensor.h
 * Layer: public model API
 *
 * Purpose:
 *   Defines the YVEX tensor info table built from parsed GGUF tensor directory
 *   records. This is an inspectable model-layout surface, not an executable
 *   backend tensor API.
 *
 * Owns:
 *   - yvex_tensor_info
 *   - yvex_tensor_table
 *   - tensor lookup
 *   - tensor role names/classification surface
 *
 * Does not own:
 *   - tokenizer state
 *   - backend allocation
 *   - graph execution
 *
 * Used by:
 *   - model descriptor
 *   - yvex inspect/tensors
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_tensor_table
 */
#ifndef YVEX_TENSOR_H
#define YVEX_TENSOR_H

#include <yvex/dtype.h>
#include <yvex/gguf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_TENSOR_MAX_DIMS 4u

typedef enum {
    YVEX_TENSOR_ROLE_UNKNOWN = 0,
    YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
    YVEX_TENSOR_ROLE_OUTPUT_NORM,
    YVEX_TENSOR_ROLE_OUTPUT_HEAD,
    YVEX_TENSOR_ROLE_ATTENTION_NORM,
    YVEX_TENSOR_ROLE_ATTENTION_Q,
    YVEX_TENSOR_ROLE_ATTENTION_K,
    YVEX_TENSOR_ROLE_ATTENTION_V,
    YVEX_TENSOR_ROLE_ATTENTION_OUT,
    YVEX_TENSOR_ROLE_FFN_NORM,
    YVEX_TENSOR_ROLE_FFN_GATE,
    YVEX_TENSOR_ROLE_FFN_UP,
    YVEX_TENSOR_ROLE_FFN_DOWN,
    YVEX_TENSOR_ROLE_MOE_ROUTER,
    YVEX_TENSOR_ROLE_MOE_EXPERT_GATE,
    YVEX_TENSOR_ROLE_MOE_EXPERT_UP,
    YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN
} yvex_tensor_role;

typedef struct {
    const char *name;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    yvex_dtype dtype;
    unsigned int ggml_type;
    yvex_tensor_role role;
    unsigned long long element_count;
    unsigned long long storage_bytes;
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
} yvex_tensor_info;

typedef struct yvex_tensor_table yvex_tensor_table;

int yvex_tensor_table_from_gguf(yvex_tensor_table **out,
                                const yvex_gguf *gguf,
                                yvex_error *err);

void yvex_tensor_table_close(yvex_tensor_table *table);

unsigned long long yvex_tensor_table_count(const yvex_tensor_table *table);
const yvex_tensor_info *yvex_tensor_table_at(const yvex_tensor_table *table,
                                             unsigned long long index);
const yvex_tensor_info *yvex_tensor_table_find(const yvex_tensor_table *table,
                                               const char *name);

const char *yvex_tensor_role_name(yvex_tensor_role role);
yvex_tensor_role yvex_tensor_role_classify(const char *architecture,
                                           const char *tensor_name,
                                           unsigned int rank,
                                           const unsigned long long *dims,
                                           yvex_dtype dtype);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_TENSOR_H */
