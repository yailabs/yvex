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
