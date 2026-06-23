/*
 * YVEX - CUDA reference ops
 *
 *
 * Purpose:
 *   Implements the first scoped CUDA op in CUDA backend: an F32 embedding lookup that
 *   matches the backend layer CPU reference behavior for parity tests.
 */
#include "cuda_internal.h"

#include <limits.h>
#include <stdint.h>

#ifdef YVEX_HAVE_CUDA_KERNEL_PTX
#include "cuda_kernels.h"
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
"}\n";
#endif

static int tensor_is_f32_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / sizeof(float)) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

static int ensure_embed_kernel(yvex_cuda_backend_state *state, yvex_error *err)
{
    int rc;

    if (state->module_loaded) {
        return YVEX_OK;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuModuleLoadData(&state->module,
                                                         yvex_cuda_kernels_ptx),
                          "cuda.embed.load_module", err);
    if (rc != YVEX_OK) {
        return rc;
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
    state->module_loaded = 1;
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
    if (embedding->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "CUDA backend CUDA embed supports F32 tensors only");
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
                          state->driver.cuLaunchKernel(state->embed_function,
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
