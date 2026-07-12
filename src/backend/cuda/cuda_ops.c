/*
 * cuda/cuda_ops.c - CUDA host launch bindings for admitted primitives.
 *
 * Owner: src/backend/cuda.
 * Owns: exact dtype/shape/parameter validation, Driver API launch parameters,
 * bounded grid arithmetic, synchronization, and output-written transitions.
 * Does not own: bundle admission, device kernel source, CPU references, graph
 * semantics, model-family behavior, CLI output, qtype compute, or generation.
 * Invariants: every op requires an exact admitted variant; launch and final
 * synchronization must succeed before any output is marked written.
 * Boundary: bounded primitive execution is not transformer or model runtime.
 */

#include "cuda_internal.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static int tensor_is_f32_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / sizeof(float)) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

static int tensor_is_f16_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / 2ull) &&
           tensor->bytes == elements * 2ull;
}

/* Contract: converts a non-zero one-dimensional launch extent without truncation. */
static int cuda_grid_1d(unsigned long long elements,
                        unsigned int block_size,
                        unsigned int *out,
                        const char *where,
                        yvex_error *err)
{
    unsigned long long blocks;

    if (!out || block_size == 0u || elements == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where,
                       "CUDA launch extent and block size must be non-zero");
        return YVEX_ERR_INVALID_ARG;
    }
    blocks = ((elements - 1ull) / (unsigned long long)block_size) + 1ull;
    if (blocks > (unsigned long long)UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "CUDA grid dimension exceeds Driver API range");
        return YVEX_ERR_BOUNDS;
    }
    *out = (unsigned int)blocks;
    return YVEX_OK;
}

static double cuda_nth_root_double(double x, unsigned long long n)
{
    double lo = 1.0;
    double hi = x > 1.0 ? x : 1.0;
    unsigned int iter;

    if (x <= 0.0 || n == 0) {
        return 0.0;
    }
    if (n == 1) {
        return x;
    }
    for (iter = 0; iter < 96u; ++iter) {
        double mid = 0.5 * (lo + hi);
        double acc = 1.0;
        unsigned long long i;

        for (i = 0; i < n; ++i) {
            acc *= mid;
            if (acc > x) {
                break;
            }
        }
        if (acc > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

static int cuda_rope_head_dim(const yvex_device_tensor *tensor,
                              unsigned long long *out,
                              yvex_error *err)
{
    unsigned long long head_dim;

    if (!tensor || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "tensor and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tensor->rank == 1) {
        head_dim = tensor->dims[0];
    } else if (tensor->rank == 2 && tensor->dims[0] == 1) {
        head_dim = tensor->dims[1];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE input must be rank 1 or dims [1, head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (head_dim == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "rope-head-dim-zero");
        return YVEX_ERR_FORMAT;
    }
    if ((head_dim & 1ull) != 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "rope-head-dim-odd");
        return YVEX_ERR_FORMAT;
    }
    *out = head_dim;
    return YVEX_OK;
}

static int cuda_attention_validate(const yvex_backend *backend,
                                   const yvex_device_tensor *query,
                                   const yvex_device_tensor *keys,
                                   const yvex_device_tensor *values,
                                   unsigned long long seq_len,
                                   unsigned long long position,
                                   yvex_device_tensor *score_scratch,
                                   yvex_device_tensor *probability_scratch,
                                   yvex_device_tensor *out,
                                   unsigned long long *head_dim_out,
                                   unsigned long long *kv_elements_out,
                                   const char *where,
                                   yvex_error *err)
{
    unsigned long long head_dim;
    unsigned long long kv_elements;

    if (!yvex_backend_tensor_owner_is(backend, query) ||
        !yvex_backend_tensor_owner_is(backend, keys) ||
        !yvex_backend_tensor_owner_is(backend, values) ||
        !yvex_backend_tensor_owner_is(backend, score_scratch) ||
        !yvex_backend_tensor_owner_is(backend, probability_scratch) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "Q/K/V, scratches, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (query->dtype != YVEX_DTYPE_F32 || keys->dtype != YVEX_DTYPE_F32 ||
        values->dtype != YVEX_DTYPE_F32 || score_scratch->dtype != YVEX_DTYPE_F32 ||
        probability_scratch->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "attention primitive supports F32 Q/K/V, scratches, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (seq_len == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "attention-seq-len-zero");
        return YVEX_ERR_FORMAT;
    }
    if (position >= seq_len) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "position-out-of-range");
        return YVEX_ERR_BOUNDS;
    }
    if (query->rank != 1 || query->dims[0] == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention query must be rank 1 with non-zero head_dim");
        return YVEX_ERR_FORMAT;
    }
    head_dim = query->dims[0];
    if (seq_len > ULLONG_MAX / head_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "attention-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    kv_elements = seq_len * head_dim;
    if (keys->rank != 2 || keys->dims[0] != seq_len || keys->dims[1] != head_dim ||
        values->rank != 2 || values->dims[0] != seq_len || values->dims[1] != head_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention keys and values must have dims [seq_len, head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (score_scratch->rank != 1 || score_scratch->dims[0] != seq_len ||
        probability_scratch->rank != 1 || probability_scratch->dims[0] != seq_len) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention score/probability scratches must have dims [seq_len]");
        return YVEX_ERR_FORMAT;
    }
    if (out->rank != 1 || out->dims[0] != head_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention output must have dims [head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (!tensor_is_f32_bytes(query, head_dim) ||
        !tensor_is_f32_bytes(keys, kv_elements) ||
        !tensor_is_f32_bytes(values, kv_elements) ||
        !tensor_is_f32_bytes(score_scratch, seq_len) ||
        !tensor_is_f32_bytes(probability_scratch, seq_len) ||
        !tensor_is_f32_bytes(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "attention tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *head_dim_out = head_dim;
    *kv_elements_out = kv_elements;
    return YVEX_OK;
}

static int cuda_matmul_validate(const yvex_backend *backend,
                                const yvex_device_tensor *input,
                                const yvex_device_tensor *weight,
                                const yvex_device_tensor *out,
                                unsigned long long *m_out,
                                unsigned long long *k_out,
                                unsigned long long *n_out,
                                const char *where,
                                yvex_error *err)
{
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long input_elements;
    unsigned long long weight_elements;
    unsigned long long output_elements;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 ||
        weight->dtype != YVEX_DTYPE_F32 ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "matmul primitive supports F32 input, weight, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (input->rank != 2 || weight->rank != 2 || out->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul tensors must be rank 2");
        return YVEX_ERR_FORMAT;
    }
    m = input->dims[0];
    k = input->dims[1];
    n = weight->dims[1];
    if (m == 0 || k == 0 || n == 0 || weight->dims[0] == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "matmul-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (weight->dims[0] != k) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul input/weight inner dimensions must match");
        return YVEX_ERR_FORMAT;
    }
    if (out->dims[0] != m || out->dims[1] != n) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul output must have dims [m, n]");
        return YVEX_ERR_FORMAT;
    }
    if (m > ULLONG_MAX / k ||
        k > ULLONG_MAX / n ||
        m > ULLONG_MAX / n) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "matmul-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    input_elements = m * k;
    weight_elements = k * n;
    output_elements = m * n;
    if (!tensor_is_f32_bytes(input, input_elements) ||
        !tensor_is_f32_bytes(weight, weight_elements) ||
        !tensor_is_f32_bytes(out, output_elements)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "matmul tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *m_out = m;
    *k_out = k;
    *n_out = n;
    return YVEX_OK;
}

static int cuda_mlp_mul3(unsigned long long a,
                         unsigned long long b,
                         unsigned long long c,
                         unsigned long long *out)
{
    unsigned long long ab;

    if (a > ULLONG_MAX / b) {
        return 0;
    }
    ab = a * b;
    if (ab > ULLONG_MAX / c) {
        return 0;
    }
    *out = ab * c;
    return 1;
}

static int cuda_mlp_validate(const yvex_backend *backend,
                             const yvex_device_tensor *input,
                             const yvex_device_tensor *gate_weight,
                             const yvex_device_tensor *up_weight,
                             const yvex_device_tensor *down_weight,
                             const yvex_mlp_options *options,
                             const yvex_device_tensor *intermediate,
                             const yvex_device_tensor *out,
                             unsigned long long *batch_out,
                             unsigned long long *hidden_dim_out,
                             unsigned long long *ffn_dim_out,
                             const char *where,
                             yvex_error *err)
{
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long input_elements;
    unsigned long long intermediate_elements;
    unsigned long long output_elements;
    unsigned long long up_elements;
    unsigned long long down_elements;
    unsigned long long routed_up_elements;
    unsigned long long routed_down_elements;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, gate_weight) ||
        !yvex_backend_tensor_owner_is(backend, up_weight) ||
        !yvex_backend_tensor_owner_is(backend, down_weight) ||
        !yvex_backend_tensor_owner_is(backend, intermediate) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "input, weights, intermediate, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 ||
        gate_weight->dtype != YVEX_DTYPE_F32 ||
        up_weight->dtype != YVEX_DTYPE_F32 ||
        down_weight->dtype != YVEX_DTYPE_F32 ||
        intermediate->dtype != YVEX_DTYPE_F32 ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "MLP primitive supports F32 input, weights, intermediate, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!options || options->batch == 0 || options->hidden_dim == 0 ||
        options->ffn_dim == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "mlp-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (!options->gated || !options->activation ||
        strcmp(options->activation, "silu") != 0) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "mlp-unsupported-activation");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (options->routed_expert_mode &&
        (options->expert_count == 0 || options->expert_id >= options->expert_count)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-expert-id-out-of-range");
        return YVEX_ERR_BOUNDS;
    }

    batch = options->batch;
    hidden_dim = options->hidden_dim;
    ffn_dim = options->ffn_dim;
    if (batch > ULLONG_MAX / hidden_dim ||
        batch > ULLONG_MAX / ffn_dim ||
        hidden_dim > ULLONG_MAX / ffn_dim ||
        ffn_dim > ULLONG_MAX / hidden_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    input_elements = batch * hidden_dim;
    intermediate_elements = batch * ffn_dim;
    output_elements = input_elements;
    up_elements = hidden_dim * ffn_dim;
    down_elements = ffn_dim * hidden_dim;

    if (input->rank != 2 || input->dims[0] != batch || input->dims[1] != hidden_dim ||
        intermediate->rank != 2 || intermediate->dims[0] != batch ||
        intermediate->dims[1] != ffn_dim ||
        out->rank != 2 || out->dims[0] != batch || out->dims[1] != hidden_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "mlp input/intermediate/output shape mismatch");
        return YVEX_ERR_FORMAT;
    }
    if (options->routed_expert_mode) {
        if (!cuda_mlp_mul3(options->expert_count, hidden_dim, ffn_dim,
                           &routed_up_elements) ||
            !cuda_mlp_mul3(options->expert_count, ffn_dim, hidden_dim,
                           &routed_down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
            return YVEX_ERR_BOUNDS;
        }
        if (gate_weight->rank != 3 || up_weight->rank != 3 || down_weight->rank != 3 ||
            gate_weight->dims[0] != options->expert_count ||
            gate_weight->dims[1] != hidden_dim || gate_weight->dims[2] != ffn_dim ||
            up_weight->dims[0] != options->expert_count ||
            up_weight->dims[1] != hidden_dim || up_weight->dims[2] != ffn_dim ||
            down_weight->dims[0] != options->expert_count ||
            down_weight->dims[1] != ffn_dim || down_weight->dims[2] != hidden_dim) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp routed weights must have dims [experts, hidden_dim, ffn_dim] and [experts, ffn_dim, hidden_dim]");
            return YVEX_ERR_FORMAT;
        }
        if (!tensor_is_f32_bytes(gate_weight, routed_up_elements) ||
            !tensor_is_f32_bytes(up_weight, routed_up_elements) ||
            !tensor_is_f32_bytes(down_weight, routed_down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp routed weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
    } else {
        if (gate_weight->rank != 2 || up_weight->rank != 2 || down_weight->rank != 2 ||
            gate_weight->dims[0] != hidden_dim || gate_weight->dims[1] != ffn_dim ||
            up_weight->dims[0] != hidden_dim || up_weight->dims[1] != ffn_dim ||
            down_weight->dims[0] != ffn_dim || down_weight->dims[1] != hidden_dim) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp dense weights must have dims [hidden_dim, ffn_dim] and [ffn_dim, hidden_dim]");
            return YVEX_ERR_FORMAT;
        }
        if (!tensor_is_f32_bytes(gate_weight, up_elements) ||
            !tensor_is_f32_bytes(up_weight, up_elements) ||
            !tensor_is_f32_bytes(down_weight, down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp dense weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
    }
    if (!tensor_is_f32_bytes(input, input_elements) ||
        !tensor_is_f32_bytes(intermediate, intermediate_elements) ||
        !tensor_is_f32_bytes(out, output_elements)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "mlp tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *batch_out = batch;
    *hidden_dim_out = hidden_dim;
    *ffn_dim_out = ffn_dim;
    return YVEX_OK;
}

int yvex_cuda_op_embed(yvex_backend *backend,
                       const yvex_device_tensor *embedding,
                       const unsigned int *token_ids,
                       unsigned long long token_count,
                       yvex_device_tensor *out,
                       yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr token_ids_device = 0;
    CUdeviceptr embedding_ptr;
    CUdeviceptr out_ptr;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long total_elements;
    size_t token_bytes;
    unsigned int block_size = 128;
    unsigned int grid_size;
    void *params[6];
    yvex_backend_operation_variant variant;
    yvex_error cleanup_error;
    int cleanup_rc;
    int rc;
    unsigned long long i;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if ((embedding->dtype != YVEX_DTYPE_F32 && embedding->dtype != YVEX_DTYPE_F16) ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "CUDA backend embed supports F32 and F16 embeddings with F32 output");
        return YVEX_ERR_UNSUPPORTED;
    }
    variant = embedding->dtype == YVEX_DTYPE_F16
                  ? YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32
                  : YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
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
    if (embedding->dtype == YVEX_DTYPE_F32 && !tensor_is_f32_bytes(embedding, hidden_size * vocab_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    if (embedding->dtype == YVEX_DTYPE_F16 && !tensor_is_f16_bytes(embedding, hidden_size * vocab_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F16 dims");
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
    total_elements = token_count * hidden_size;
    if (!tensor_is_f32_bytes(out, total_elements)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < token_count; ++i) {
        if ((unsigned long long)token_ids[i] >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                            "token id %u exceeds embedding vocab size %llu",
                            token_ids[i], vocab_size);
            return YVEX_ERR_BOUNDS;
        }
    }
    if (token_count > (unsigned long long)(SIZE_MAX / sizeof(unsigned int))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "token id buffer is too large");
        return YVEX_ERR_BOUNDS;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_embed", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    token_bytes = (size_t)(token_count * sizeof(unsigned int));
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemAlloc_v2(&token_ids_device, token_bytes),
                          "cuda.embed.token_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemcpyHtoD_v2(token_ids_device,
                                                         token_ids,
                                                         token_bytes),
                          "cuda.embed.token_copy", err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&cleanup_error);
        cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                              "cuda.embed.token_cleanup",
                                              &cleanup_error);
        if (cleanup_rc != YVEX_OK) {
            if (err) {
                *err = cleanup_error;
            }
            return cleanup_rc;
        }
        return rc;
    }

    rc = cuda_grid_1d(total_elements, block_size, &grid_size,
                      "cuda.embed.grid", err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&cleanup_error);
        cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                              "cuda.embed.token_cleanup",
                                              &cleanup_error);
        if (cleanup_rc != YVEX_OK) {
            if (err) {
                *err = cleanup_error;
            }
            return cleanup_rc;
        }
        return rc;
    }
    embedding_ptr = yvex_cuda_tensor_ptr(embedding);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &embedding_ptr;
    params[1] = &token_ids_device;
    params[2] = &out_ptr;
    params[3] = &hidden_size;
    params[4] = &vocab_size;
    params[5] = &token_count;
    rc = yvex_cuda_launch(backend, variant,
                          embedding->dtype == YVEX_DTYPE_F16
                              ? state->embed_f16_function : state->embed_function,
                          grid_size, block_size, 0, params,
                          "cuda.embed.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.embed.sync", err);
    }
    yvex_error_clear(&cleanup_error);
    cleanup_rc = yvex_cuda_temporary_free(backend, variant, token_ids_device,
                                          "cuda.embed.token_cleanup",
                                          &cleanup_error);
    if (cleanup_rc != YVEX_OK) {
        if (err) {
            *err = cleanup_error;
        }
        return cleanup_rc;
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_op_rms_norm(yvex_backend *backend,
                          const yvex_device_tensor *input,
                          const yvex_device_tensor *weight,
                          float epsilon,
                          yvex_device_tensor *out,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr weight_ptr;
    CUdeviceptr out_ptr;
    unsigned long long hidden_size;
    unsigned int block_size = 256u;
    unsigned int shared_bytes = block_size * (unsigned int)sizeof(float);
    void *params[5];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rms_norm",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rms_norm",
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32 ||
        (weight->dtype != YVEX_DTYPE_F32 && weight->dtype != YVEX_DTYPE_F16)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rms_norm",
                       "CUDA RMSNorm supports F32 input/output with F16 or F32 weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    variant = weight->dtype == YVEX_DTYPE_F16
                  ? YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16
                  : YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!isfinite(epsilon) || epsilon <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rms_norm",
                       "epsilon must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (input->rank == 2 && input->dims[0] == 1) {
        hidden_size = input->dims[1];
    } else if (input->rank == 1) {
        hidden_size = input->dims[0];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm input must be rank 1 or dims [1, hidden]");
        return YVEX_ERR_FORMAT;
    }
    if (hidden_size == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm hidden size must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    if (weight->rank != 1 || weight->dims[0] != hidden_size) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm weight must be rank 1 and match hidden size");
        return YVEX_ERR_FORMAT;
    }
    if (out->rank != input->rank ||
        out->dims[0] != input->dims[0] ||
        (input->rank == 2 && out->dims[1] != input->dims[1])) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!tensor_is_f32_bytes(input, hidden_size) || !tensor_is_f32_bytes(out, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm input/output bytes must match F32 hidden size");
        return YVEX_ERR_BOUNDS;
    }
    if (weight->dtype == YVEX_DTYPE_F32 && !tensor_is_f32_bytes(weight, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm F32 weight bytes must match hidden size");
        return YVEX_ERR_BOUNDS;
    }
    if (weight->dtype == YVEX_DTYPE_F16 && !tensor_is_f16_bytes(weight, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm F16 weight bytes must match hidden size");
        return YVEX_ERR_BOUNDS;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_rms_norm", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &weight_ptr;
    params[2] = &out_ptr;
    params[3] = &hidden_size;
    params[4] = &epsilon;

    rc = yvex_cuda_launch(backend, variant,
                          weight->dtype == YVEX_DTYPE_F16
                              ? state->rms_norm_f16_function
                              : state->rms_norm_f32_function,
                          1, block_size, shared_bytes, params,
                          "cuda.rms_norm.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.rms_norm.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_op_rope(yvex_backend *backend,
                      const yvex_device_tensor *input,
                      unsigned long long position,
                      float rope_base,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr out_ptr;
    unsigned long long head_dim;
    unsigned long long pair_count;
    unsigned int block_size = 128u;
    unsigned int grid_size;
    float inverse_root;
    void *params[5];
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "input and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "CUDA RoPE supports F32 input/output");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                                      "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!yvex_backend_tensor_same_shape(input, out)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!isfinite(rope_base) || rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be finite and greater than 1");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = cuda_rope_head_dim(input, &head_dim, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!tensor_is_f32_bytes(input, head_dim) || !tensor_is_f32_bytes(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rope",
                       "RoPE input/output bytes must match F32 head_dim");
        return YVEX_ERR_BOUNDS;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_rope", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    pair_count = head_dim / 2ull;
    inverse_root = (float)(1.0 / cuda_nth_root_double((double)rope_base, pair_count));
    rc = cuda_grid_1d(pair_count, block_size, &grid_size, "cuda.rope.grid", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &out_ptr;
    params[2] = &head_dim;
    params[3] = &position;
    params[4] = &inverse_root;

    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                          state->rope_function, grid_size, block_size, 0,
                          params, "cuda.rope.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_ROPE_F32,
                                   "cuda.rope.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_op_matmul(yvex_backend *backend,
                        const yvex_device_tensor *input,
                        const yvex_device_tensor *weight,
                        yvex_device_tensor *out,
                        yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr weight_ptr;
    CUdeviceptr out_ptr;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long output_elements;
    unsigned int block_size = 128u;
    unsigned int grid_size;
    void *params[6];
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_matmul",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    rc = cuda_matmul_validate(backend, input, weight, out,
                              &m, &k, &n, "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                                      "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    output_elements = m * n;
    rc = cuda_grid_1d(output_elements, block_size, &grid_size,
                      "cuda.matmul.grid", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &weight_ptr;
    params[2] = &out_ptr;
    params[3] = &m;
    params[4] = &k;
    params[5] = &n;

    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                          state->matmul_function, grid_size, block_size, 0,
                          params, "cuda.matmul.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_MATMUL_F32,
                                   "cuda.matmul.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_op_mlp(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     const yvex_device_tensor *gate_weight,
                     const yvex_device_tensor *up_weight,
                     const yvex_device_tensor *down_weight,
                     const yvex_mlp_options *options,
                     yvex_device_tensor *intermediate,
                     yvex_device_tensor *out,
                     yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr;
    CUdeviceptr gate_ptr;
    CUdeviceptr up_ptr;
    CUdeviceptr down_ptr;
    CUdeviceptr intermediate_ptr;
    CUdeviceptr out_ptr;
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long expert_count;
    unsigned long long expert_id;
    unsigned int block_size = 128u;
    int routed;
    void *params[12];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_mlp",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    rc = cuda_mlp_validate(backend, input, gate_weight, up_weight, down_weight,
                           options, intermediate, out, &batch, &hidden_dim,
                           &ffn_dim, "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    variant = options->routed_expert_mode
                  ? YVEX_BACKEND_VARIANT_MLP_ROUTED_F32
                  : YVEX_BACKEND_VARIANT_MLP_DENSE_F32;
    intermediate->is_written = 0;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    gate_ptr = yvex_cuda_tensor_ptr(gate_weight);
    up_ptr = yvex_cuda_tensor_ptr(up_weight);
    down_ptr = yvex_cuda_tensor_ptr(down_weight);
    intermediate_ptr = yvex_cuda_tensor_ptr(intermediate);
    out_ptr = yvex_cuda_tensor_ptr(out);
    expert_count = options->expert_count;
    expert_id = options->expert_id;
    routed = options->routed_expert_mode ? 1 : 0;
    params[0] = &input_ptr;
    params[1] = &gate_ptr;
    params[2] = &up_ptr;
    params[3] = &down_ptr;
    params[4] = &intermediate_ptr;
    params[5] = &out_ptr;
    params[6] = &batch;
    params[7] = &hidden_dim;
    params[8] = &ffn_dim;
    params[9] = &expert_count;
    params[10] = &expert_id;
    params[11] = &routed;

    rc = yvex_cuda_launch(backend, variant, state->mlp_function,
                          1, block_size, 0, params, "cuda.mlp.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.mlp.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    intermediate->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_op_attention(yvex_backend *backend,
                           const yvex_device_tensor *query,
                           const yvex_device_tensor *keys,
                           const yvex_device_tensor *values,
                           unsigned long long seq_len,
                           unsigned long long position,
                           float scale,
                           int causal,
                           yvex_device_tensor *score_scratch,
                           yvex_device_tensor *probability_scratch,
                           yvex_device_tensor *out,
                           yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr query_ptr;
    CUdeviceptr keys_ptr;
    CUdeviceptr values_ptr;
    CUdeviceptr score_ptr;
    CUdeviceptr probability_ptr;
    CUdeviceptr out_ptr;
    unsigned long long head_dim;
    unsigned long long kv_elements;
    unsigned int block_size = 128u;
    int causal_flag = causal ? 1 : 0;
    void *params[11];
    yvex_backend_operation_variant variant;
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (!isfinite(scale) || scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = cuda_attention_validate(backend, query, keys, values, seq_len, position,
                                 score_scratch, probability_scratch, out,
                                 &head_dim, &kv_elements,
                                 "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)kv_elements;
    variant = causal ? YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32
                     : YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32;
    score_scratch->is_written = 0;
    probability_scratch->is_written = 0;
    out->is_written = 0;
    rc = yvex_cuda_require_capability(backend, variant,
                                      "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    query_ptr = yvex_cuda_tensor_ptr(query);
    keys_ptr = yvex_cuda_tensor_ptr(keys);
    values_ptr = yvex_cuda_tensor_ptr(values);
    score_ptr = yvex_cuda_tensor_ptr(score_scratch);
    probability_ptr = yvex_cuda_tensor_ptr(probability_scratch);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &query_ptr;
    params[1] = &keys_ptr;
    params[2] = &values_ptr;
    params[3] = &score_ptr;
    params[4] = &probability_ptr;
    params[5] = &out_ptr;
    params[6] = &seq_len;
    params[7] = &position;
    params[8] = &head_dim;
    params[9] = &scale;
    params[10] = &causal_flag;

    rc = yvex_cuda_launch(backend, variant, state->attention_function,
                          1, block_size, 0, params,
                          "cuda.attention.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_synchronize(backend, variant, "cuda.attention.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    score_scratch->is_written = 1;
    probability_scratch->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}
