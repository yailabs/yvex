/*
 * cuda/cuda_ops.c - CUDA host launch wrappers for backend ops.
 *
 * This file validates backend tensors and launches CUDA kernels. It does not
 * expose CUDA syntax to the plain C runtime.
 */

#include "cuda_internal.h"
#include <limits.h>
#include <stdint.h>
#include "cuda_kernels.h"


#ifdef YVEX_HAVE_CUDA_KERNEL_PTX
#else
/*
 * Fallback embedded PTX.
 *
 * The canonical kernel source is cuda_kernels.cu. This fallback keeps the
 * baseline build usable on hosts without nvcc.
 */
static const char yvex_cuda_kernels_ptx[] =
".version 6.4\n"
".target sm_30\n"
".address_size 64\n"
".visible .entry yvex_embed_f32(\n"
"    .param .u64 p_embedding,\n"
"    .param .u64 p_token_ids,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_hidden_size,\n"
"    .param .u64 p_vocab_size,\n"
"    .param .u64 p_token_count\n"
")\n"
"{\n"
"    .reg .pred %p<3>;\n"
"    .reg .b32 %r<6>;\n"
"    .reg .b64 %rd<24>;\n"
"    .reg .f32 %f<2>;\n"
"\n"
"    ld.param.u64 %rd1, [p_embedding];\n"
"    ld.param.u64 %rd2, [p_token_ids];\n"
"    ld.param.u64 %rd3, [p_out];\n"
"    ld.param.u64 %rd4, [p_hidden_size];\n"
"    ld.param.u64 %rd5, [p_vocab_size];\n"
"    ld.param.u64 %rd6, [p_token_count];\n"
"\n"
"    mov.u32 %r1, %tid.x;\n"
"    mov.u32 %r2, %ctaid.x;\n"
"    mov.u32 %r3, %ntid.x;\n"
"    mad.lo.u32 %r4, %r2, %r3, %r1;\n"
"    cvt.u64.u32 %rd7, %r4;\n"
"    mul.lo.u64 %rd8, %rd4, %rd6;\n"
"    setp.ge.u64 %p1, %rd7, %rd8;\n"
"    @%p1 bra DONE;\n"
"\n"
"    div.u64 %rd9, %rd7, %rd4;\n"
"    rem.u64 %rd10, %rd7, %rd4;\n"
"    mul.lo.u64 %rd11, %rd9, 4;\n"
"    add.u64 %rd12, %rd2, %rd11;\n"
"    ld.global.u32 %r5, [%rd12];\n"
"    cvt.u64.u32 %rd13, %r5;\n"
"    setp.ge.u64 %p2, %rd13, %rd5;\n"
"    @%p2 bra DONE;\n"
"\n"
"    mul.lo.u64 %rd14, %rd13, %rd4;\n"
"    add.u64 %rd15, %rd14, %rd10;\n"
"    mul.lo.u64 %rd16, %rd15, 4;\n"
"    add.u64 %rd17, %rd1, %rd16;\n"
"    ld.global.f32 %f1, [%rd17];\n"
"    mul.lo.u64 %rd18, %rd7, 4;\n"
"    add.u64 %rd19, %rd3, %rd18;\n"
"    st.global.f32 [%rd19], %f1;\n"
"\n"
"DONE:\n"
"    ret;\n"
"}\n"
".visible .entry yvex_embed_f16_to_f32(\n"
"    .param .u64 p_embedding,\n"
"    .param .u64 p_token_ids,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_hidden_size,\n"
"    .param .u64 p_vocab_size,\n"
"    .param .u64 p_token_count\n"
")\n"
"{\n"
"    ret;\n"
"}\n"
".visible .entry yvex_rms_norm_f32_weight_f32(\n"
"    .param .u64 p_input,\n"
"    .param .u64 p_weight,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_hidden_size,\n"
"    .param .f32 p_epsilon\n"
")\n"
"{\n"
"    ret;\n"
"}\n"
".visible .entry yvex_rms_norm_f32_weight_f16(\n"
"    .param .u64 p_input,\n"
"    .param .u64 p_weight,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_hidden_size,\n"
"    .param .f32 p_epsilon\n"
")\n"
"{\n"
"    ret;\n"
"}\n"
".visible .entry yvex_rope_f32(\n"
"    .param .u64 p_input,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_head_dim,\n"
"    .param .u64 p_position,\n"
"    .param .f32 p_inverse_root\n"
")\n"
"{\n"
"    ret;\n"
"}\n"
".visible .entry yvex_matmul_f32(\n"
"    .param .u64 p_input,\n"
"    .param .u64 p_weight,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_m,\n"
"    .param .u64 p_k,\n"
"    .param .u64 p_n\n"
")\n"
"{\n"
"    ret;\n"
"}\n"
".visible .entry yvex_attention_f32(\n"
"    .param .u64 p_query,\n"
"    .param .u64 p_keys,\n"
"    .param .u64 p_values,\n"
"    .param .u64 p_score_scratch,\n"
"    .param .u64 p_probability_scratch,\n"
"    .param .u64 p_out,\n"
"    .param .u64 p_seq_len,\n"
"    .param .u64 p_position,\n"
"    .param .u64 p_head_dim,\n"
"    .param .f32 p_scale,\n"
"    .param .u32 p_causal\n"
")\n"
"{\n"
"    ret;\n"
"}\n";
#endif

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

static int ensure_kernel_module(yvex_cuda_backend_state *state, const char *where, yvex_error *err)
{
    int rc;

    if (state->module_loaded) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleLoadData(&state->module,
                                                         yvex_cuda_kernels_ptx),
                          where, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    state->module_loaded = 1;
    return YVEX_OK;
}

static int ensure_embed_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    rc = ensure_kernel_module(state, "cuda.kernels.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (state->embed_function && state->embed_f16_function) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->embed_function,
                                                            state->module,
                                                            "yvex_embed_f32"),
                          "cuda.embed.load_function", err);
    if (rc != YVEX_OK) {
        (void)state->driver.cuModuleUnload(state->module);
        state->module = NULL;
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->embed_f16_function,
                                                            state->module,
                                                            "yvex_embed_f16_to_f32"),
                          "cuda.embed.load_f16_function", err);
    if (rc != YVEX_OK) {
        (void)state->driver.cuModuleUnload(state->module);
        state->module = NULL;
        state->embed_function = NULL;
        return rc;
    }
    return YVEX_OK;
}

static int ensure_rms_norm_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    rc = ensure_kernel_module(state, "cuda.kernels.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (state->rms_norm_f32_function && state->rms_norm_f16_function) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->rms_norm_f32_function,
                                                            state->module,
                                                            "yvex_rms_norm_f32_weight_f32"),
                          "cuda.rms_norm.load_f32_function", err);
    if (rc != YVEX_OK) {
        state->rms_norm_f32_function = NULL;
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->rms_norm_f16_function,
                                                            state->module,
                                                            "yvex_rms_norm_f32_weight_f16"),
                          "cuda.rms_norm.load_f16_function", err);
    if (rc != YVEX_OK) {
        state->rms_norm_f16_function = NULL;
        return rc;
    }
    return YVEX_OK;
}

static int ensure_rope_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    rc = ensure_kernel_module(state, "cuda.kernels.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (state->rope_function) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->rope_function,
                                                            state->module,
                                                            "yvex_rope_f32"),
                          "cuda.rope.load_function", err);
    if (rc != YVEX_OK) {
        state->rope_function = NULL;
        return rc;
    }
    return YVEX_OK;
}

static int ensure_matmul_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    rc = ensure_kernel_module(state, "cuda.kernels.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (state->matmul_function) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->matmul_function,
                                                            state->module,
                                                            "yvex_matmul_f32"),
                          "cuda.matmul.load_function", err);
    if (rc != YVEX_OK) {
        state->matmul_function = NULL;
        return rc;
    }
    return YVEX_OK;
}

static int ensure_attention_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    rc = ensure_kernel_module(state, "cuda.kernels.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (state->attention_function) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleGetFunction(&state->attention_function,
                                                            state->module,
                                                            "yvex_attention_f32"),
                          "cuda.attention.load_function", err);
    if (rc != YVEX_OK) {
        state->attention_function = NULL;
        return rc;
    }
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
    rc = ensure_embed_kernel(state, err);
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
        (void)state->driver.cuMemFree_v2(token_ids_device);
        return rc;
    }

    grid_size = (unsigned int)((total_elements + block_size - 1u) / block_size);
    embedding_ptr = yvex_cuda_tensor_ptr(embedding);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &embedding_ptr;
    params[1] = &token_ids_device;
    params[2] = &out_ptr;
    params[3] = &hidden_size;
    params[4] = &vocab_size;
    params[5] = &token_count;
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(embedding->dtype == YVEX_DTYPE_F16
                                                           ? state->embed_f16_function
                                                           : state->embed_function,
                                                       grid_size, 1, 1,
                                                       block_size, 1, 1,
                                                       0, NULL, params, NULL),
                          "cuda.embed.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                              "cuda.embed.sync", err);
    }
    (void)state->driver.cuMemFree_v2(token_ids_device);
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
    if (epsilon <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rms_norm",
                       "epsilon must be positive");
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
    rc = ensure_rms_norm_kernel(state, err);
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

    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(weight->dtype == YVEX_DTYPE_F16
                                                           ? state->rms_norm_f16_function
                                                           : state->rms_norm_f32_function,
                                                       1, 1, 1,
                                                       block_size, 1, 1,
                                                       shared_bytes, NULL, params, NULL),
                          "cuda.rms_norm.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                              "cuda.rms_norm.sync", err);
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
    if (!yvex_backend_tensor_same_shape(input, out)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be greater than 1");
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
    rc = ensure_rope_kernel(state, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    pair_count = head_dim / 2ull;
    inverse_root = (float)(1.0 / cuda_nth_root_double((double)rope_base, pair_count));
    grid_size = (unsigned int)((pair_count + (unsigned long long)block_size - 1ull) /
                               (unsigned long long)block_size);
    input_ptr = yvex_cuda_tensor_ptr(input);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &out_ptr;
    params[2] = &head_dim;
    params[3] = &position;
    params[4] = &inverse_root;

    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(state->rope_function,
                                                       grid_size, 1, 1,
                                                       block_size, 1, 1,
                                                       0, NULL, params, NULL),
                          "cuda.rope.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
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

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = ensure_matmul_kernel(state, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    output_elements = m * n;
    grid_size = (unsigned int)((output_elements + (unsigned long long)block_size - 1ull) /
                               (unsigned long long)block_size);
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    out_ptr = yvex_cuda_tensor_ptr(out);
    params[0] = &input_ptr;
    params[1] = &weight_ptr;
    params[2] = &out_ptr;
    params[3] = &m;
    params[4] = &k;
    params[5] = &n;

    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(state->matmul_function,
                                                       grid_size, 1, 1,
                                                       block_size, 1, 1,
                                                       0, NULL, params, NULL),
                          "cuda.matmul.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                              "cuda.matmul.sync", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
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
    int causal_flag = causal ? 1 : 0;
    void *params[11];
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be positive");
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

    rc = yvex_cuda_set_current(backend, "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = ensure_attention_kernel(state, err);
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

    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(state->attention_function,
                                                       1, 1, 1,
                                                       1, 1, 1,
                                                       0, NULL, params, NULL),
                          "cuda.attention.launch", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                              "cuda.attention.sync", err);
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
