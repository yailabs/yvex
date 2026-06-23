/*
 * YVEX - CPU reference ops
 *
 * File: src/backend/cpu_ops.c
 * Layer: backend implementation
 *
 * Purpose:
 *   Implements the first CPU reference op over backend tensors. backend layer supports a
 *   minimal F32 embedding lookup to prove backend ABI dispatch beyond raw
 *   allocation/read/write.
 *
 * Implements:
 *   - yvex_cpu_op_embed
 *
 * Invariants:
 *   - only F32 embedding tensors are supported in backend layer
 *   - embedding dims use [hidden_size, vocab_size]
 *   - output dims use [token_count, hidden_size]
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_ops
 */
#include "backend_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static int tensor_is_f32_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / sizeof(float)) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

int yvex_cpu_op_embed(yvex_backend *backend,
                      const yvex_device_tensor *embedding,
                      const unsigned int *token_ids,
                      unsigned long long token_count,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long e;
    unsigned long long t;
    const float *embedding_data;
    float *out_data;

    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (embedding->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "backend layer CPU embed supports F32 tensors only");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (embedding->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_embed",
                       "embedding tensor must have rank 2");
        return YVEX_ERR_FORMAT;
    }
    hidden_size = embedding->dims[0];
    vocab_size = embedding->dims[1];
    if (hidden_size == 0 || vocab_size == 0 || hidden_size > ULLONG_MAX / vocab_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding dimensions overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!tensor_is_f32_bytes(embedding, hidden_size * vocab_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    if (token_count > ULLONG_MAX / hidden_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output dimensions overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (out->rank != 2 || out->dims[0] != token_count || out->dims[1] != hidden_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output tensor must have dims [token_count, hidden_size]");
        return YVEX_ERR_BOUNDS;
    }
    if (!tensor_is_f32_bytes(out, token_count * hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }

    embedding_data = (const float *)embedding->data;
    out_data = (float *)out->data;
    for (t = 0; t < token_count; ++t) {
        unsigned int token_id = token_ids[t];
        if ((unsigned long long)token_id >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                            "token id %u exceeds embedding vocab size %llu",
                            token_id, vocab_size);
            return YVEX_ERR_BOUNDS;
        }
        for (e = 0; e < hidden_size; ++e) {
            out_data[(t * hidden_size) + e] =
                embedding_data[((unsigned long long)token_id * hidden_size) + e];
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}
