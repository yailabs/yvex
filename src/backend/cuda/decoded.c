/* Own decoded-matrix execution under the explicitly admitted numerical class. */
#include "src/backend/cuda/component_ops.h"
#include <limits.h>

/* Explicit F32 accumulation, never a replacement for ordered row-dot. The
 * active execution stream and zero-workspace policy belong to the backend;
 * callers retain input/output lifetime and finite-result publication. */
int yvex_cuda_f32_matrix(yvex_backend *backend, CUdeviceptr weights,
    unsigned long long rows, unsigned long long width, unsigned long long inputs,
    CUdeviceptr input, CUdeviceptr output, int additive, int inline_rows, yvex_error *err)
{
    enum { OP_N = 0, OP_T = 1, R32F = 0, COMPUTE32F = 68, DEFAULT_ALGO = -1 };
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    const float alpha = 1.0f, beta = additive ? 1.0f : 0.0f;
    if (!state || !weights || !input || !output || !rows || !width || !inputs ||
        rows > INT_MAX || width > INT_MAX || inputs > INT_MAX ||
        (additive != 0 && additive != 1) || (inline_rows != 0 && inline_rows != 1)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.f32-matrix", "invalid F32 contraction geometry");
        return YVEX_ERR_BOUNDS;
    }
    /* Narrow F32 contractions are ordinary owned kernels: they can be captured
     * and rebound without submitting an opaque library operation each token.
     * This reduction is legal only for F32 accumulation, never ordered F64. */
    if (inline_rows && inputs <= YVEX_CUDA_F32_INLINE_ROWS && rows <= UINT_MAX / inputs) {
        void *parameters[] = {&weights, &input, &output, &rows, &width, &inputs, &additive};
        if (state->parameter_update_owner)
            return yvex_cuda_graph_kernel_update(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                state->f32_matrix_function, (unsigned int)(rows * inputs), 128u, 0u,
                parameters, "cuda.f32-matrix.rows", err);
        return yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->f32_matrix_function, (unsigned int)(rows * inputs), 128u, 0u,
            parameters, "cuda.f32-matrix.rows", err);
    }
    int rc = yvex_cuda_blas_bind_launch_stream(backend, "cuda.f32-matrix.stream", err);
    if (rc != YVEX_OK) return rc;
    int status = state->blas.gemm_ex(state->blas.handle, OP_T, OP_N,
        (int)rows, (int)inputs, (int)width, &alpha,
        (const void *)(uintptr_t)weights, R32F, (int)width,
        (const void *)(uintptr_t)input, R32F, (int)width,
        &beta, (void *)(uintptr_t)output, R32F, (int)rows, COMPUTE32F, DEFAULT_ALGO);
    if (status) {
        yvex_error_setf(err, YVEX_ERR_BACKEND, "cuda.f32-matrix", "F32 contraction refused: %d", status);
        return YVEX_ERR_BACKEND;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int decoded_launch(yvex_cuda_work *work, CUfunction function,
                          unsigned grid, unsigned block, void **parameters,
                          yvex_error *err)
{
    int rc;
    if (work->prepare_only)
        return yvex_cuda_graph_kernel_update(work->backend, work->variant,
            function, grid, block, 0u, parameters, "cuda.decoded-matrix", err);
    rc = yvex_cuda_launch(work->backend, work->variant, function, grid, block,
        0u, parameters, "cuda.decoded-matrix", err);
    if (rc == YVEX_OK && !yvex_cuda_capture_active(work->backend)) work->launches++;
    return rc;
}

/* Bounded replacement tiles share the existing decoded arena. The encoded
 * artifact remains mapped and immutable; only one tile is prepared at a time.
 * Stream order prevents overwrite until the preceding contraction has consumed
 * it. No model-sized duplicate, lazy allocation outside admission, or BLAS
 * workspace. Library nodes are deliberately excluded from owned-node replay. */
int yvex_cuda_prepared_matrix(yvex_cuda_work *work,
    const yvex_backend_attention_weight *weight, CUdeviceptr encoded,
    unsigned long long start_row, unsigned long long row_count,
    unsigned long long input_rows, CUdeviceptr input, CUdeviceptr output,
    int output_bf16, CUdeviceptr status, yvex_error *err)
{
    unsigned long long bytes = yvex_cuda_decoded_workspace(input_rows);
    if (!work || !work->state || work->matrix_projection != 2 ||
        work->prepare_only || yvex_cuda_capture_active(work->backend) ||
        !work->state->blas.ready || work->backend->device_info.compute_capability_major < 8 ||
        !weight || !encoded || !input || !output || !status ||
        !row_count || row_count > 131072ull || input_rows < 256ull ||
        input_rows > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
        !weight->row_width || weight->row_width > 8192ull || weight->row_width % 32ull ||
        (weight->qtype != YVEX_GGUF_QTYPE_BF16 && weight->qtype != YVEX_GGUF_QTYPE_MXFP4) ||
        weight->row_bytes != (weight->qtype == YVEX_GGUF_QTYPE_BF16
            ? weight->row_width * 2ull : weight->row_width / 32ull * 17ull) ||
        start_row > weight->row_count || row_count > weight->row_count - start_row ||
        (output_bf16 != 0 && output_bf16 != 1)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cuda.prepared-matrix",
            "prepared BF16/F32 class requires bounded wide geometry and stream submission");
        return YVEX_ERR_UNSUPPORTED;
    }
    unsigned width = (unsigned)weight->row_width, inputs = (unsigned)input_rows;
    unsigned long long input_bytes = input_rows * width * 2ull;
    if (input_bytes >= bytes || bytes - input_bytes < width * 2ull) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.prepared-matrix",
            "activation and one prepared row exceed the admitted decoded arena");
        return YVEX_ERR_BOUNDS;
    }
    /* Physical tile geometry follows the already admitted arena and real K/N,
     * not a universal 512-row cap. Stream-ordered replacement still retains
     * only one weight tile alongside the packed input. Regular 64-row tiles
     * do not invent rows: the final contraction consumes its exact tail. */
    unsigned long long fitting_rows = (bytes - input_bytes) / (width * 2ull);
    if (fitting_rows >= 64ull) fitting_rows -= fitting_rows % 64ull;
    unsigned tile = (unsigned)(row_count < fitting_rows ? row_count : fitting_rows);
    int rc = YVEX_OK;
    if (bytes > work->decoded_capacity) {
        rc = yvex_cuda_work_allocate(work, &work->decoded_workspace, (size_t)bytes,
            NULL, 0, "cuda.prepared-matrix", NULL, err);
        if (rc == YVEX_OK) work->decoded_capacity = bytes;
    }
    if (rc == YVEX_OK) rc = yvex_cuda_blas_bind_launch_stream(work->backend, "cuda.prepared-matrix.stream", err);
    if (rc != YVEX_OK) return rc;
    CUdeviceptr packed_input = work->decoded_workspace, packed_weight = packed_input + input_bytes;
    unsigned input_qtype = YVEX_GGUF_QTYPE_F32;
    unsigned long long input_stride = (unsigned long long)width * sizeof(float);
    void *prepare_input[] = {&input, &input_stride, &width, &inputs, &input_qtype, &packed_input, &status};
    rc = decoded_launch(work, work->state->bf16_prepare_function,
        (unsigned)((input_rows * width + 255ull) / 256ull), 256u, prepare_input, err);
    encoded += start_row * weight->row_bytes;
    const float alpha = 1.0f, beta = 0.0f;
    for (unsigned long long first = 0ull; rc == YVEX_OK && first < row_count; first += tile) {
        unsigned count = (unsigned)(row_count - first < tile ? row_count - first : tile);
        CUdeviceptr source = encoded + first * weight->row_bytes;
        void *prepare_weight[] = {&source, (void *)&weight->row_bytes, &width, &count,
            (void *)&weight->qtype, &packed_weight, &status};
        rc = decoded_launch(work, work->state->bf16_prepare_function,
            (unsigned)(((unsigned long long)count * width + 255ull) / 256ull), 256u, prepare_weight, err);
        if (rc != YVEX_OK) break;
        int result = work->state->blas.gemm_ex(work->state->blas.handle, 1, 0,
            (int)count, (int)inputs, (int)width, &alpha,
            (const void *)(uintptr_t)packed_weight, 14, (int)width,
            (const void *)(uintptr_t)packed_input, 14, (int)width, &beta,
            (void *)(uintptr_t)(output + first * sizeof(float)), 0, (int)row_count, 68, -1);
        if (result) {
            yvex_error_setf(err, YVEX_ERR_BACKEND, "cuda.prepared-matrix", "BF16 contraction refused: %d", result);
            return YVEX_ERR_BACKEND;
        }
        /* BLAS is one accelerated operation; its private kernel population is
         * available only through profiling, not invented in launch telemetry. */
        work->tensor_core_launches++;
    }
    unsigned long long elements = row_count * input_rows;
    void *finish[] = {&output, &elements, &output_bf16, &status};
    if (rc == YVEX_OK) rc = decoded_launch(work, work->state->matrix_finish_function,
        (unsigned)((elements + 255ull) / 256ull), 256u, finish, err);
    return rc;
}

/* Only the compiler-selected BF16/F32 class may call this consumer. No
 * persistent derived weights or allocation: each CTA owns its decoded tile. */
int yvex_cuda_bf16_matrix(yvex_cuda_work *work,
    const yvex_backend_attention_weight *weight, CUdeviceptr encoded,
    unsigned long long start_row, unsigned long long row_count,
    unsigned long long input_rows, CUdeviceptr input, CUdeviceptr output,
    int output_bf16, CUdeviceptr status, yvex_error *err)
{
    if (!work || !work->state || !work->matrix_projection || work->matrix_projection > 2 ||
        !weight || weight->qtype != YVEX_GGUF_QTYPE_BF16 || !encoded || !input || !output || !status ||
        !row_count || row_count > 16384ull || !input_rows ||
        input_rows > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
        !weight->row_width || weight->row_width > 16384ull || weight->row_width % 32ull ||
        weight->row_bytes != weight->row_width * 2ull || start_row > weight->row_count ||
        row_count > weight->row_count - start_row || (output_bf16 != 0 && output_bf16 != 1) ||
        work->backend->device_info.compute_capability_major < 8) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cuda.bf16-matrix",
                       "explicit BF16/F32 class and bounded SM80 matrix geometry are required");
        return YVEX_ERR_UNSUPPORTED;
    }
    unsigned width = (unsigned)weight->row_width, rows = (unsigned)row_count, inputs = (unsigned)input_rows;
    encoded += start_row * weight->row_bytes;
    void *parameters[] = {&encoded, &width, &rows, &inputs, &input, &output, &output_bf16, &status};
    unsigned grid = inputs < 32u ? ((rows + 7u) / 8u) * inputs
                                : ((rows + 63u) / 64u) * ((inputs + 31u) / 32u);
    int rc = decoded_launch(work, work->state->bf16_matrix_function,
        grid, 256u, parameters, err);
    if (rc == YVEX_OK && inputs >= 32u) work->tensor_core_launches++;
    return rc;
}

/* Prepare exact activation digits once for all grouped weight rows. The arena
 * is session/stream-owned; no host values or artifact bytes are modified. */
int yvex_cuda_decoded_mxfp4(yvex_cuda_work *work,
    const yvex_backend_attention_weight *weight, CUdeviceptr encoded,
    unsigned long long group_count, unsigned long long group_rows,
    unsigned long long input_rows, CUdeviceptr input, CUdeviceptr output,
    int output_bf16, CUdeviceptr status, yvex_error *err)
{
    unsigned long long bytes = yvex_cuda_decoded_workspace(input_rows);
    if (!work || !work->state || !weight || !encoded || !input || !output || !status ||
        !group_count || group_count > 8ull || !group_rows || group_rows > 16384ull ||
        !input_rows || input_rows > 16ull || !weight->row_width || weight->row_width > 8192ull ||
        weight->row_width % 32ull || weight->row_count != group_count * group_rows ||
        weight->row_bytes != weight->row_width / 32ull * 17ull ||
        weight->qtype != YVEX_GGUF_QTYPE_MXFP4 || (output_bf16 != 0 && output_bf16 != 1)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.decoded-mxfp4",
                       "lossless activation preparation requires bounded grouped rows");
        return YVEX_ERR_INVALID_ARG;
    }
    unsigned width = (unsigned)weight->row_width, groups = (unsigned)group_count;
    unsigned rows = (unsigned)group_rows, inputs = (unsigned)input_rows;
    unsigned prepared_rows = inputs * groups, f32 = YVEX_GGUF_QTYPE_F32;
    unsigned long long elements = (unsigned long long)prepared_rows * width;
    if (elements * 25ull / 8ull + prepared_rows * sizeof(int) > bytes) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.decoded-mxfp4",
                       "lossless activation preparation exceeds the admitted workspace");
        return YVEX_ERR_BOUNDS;
    }
    int rc = YVEX_OK;
    if (bytes > work->decoded_capacity) {
        rc = yvex_cuda_work_allocate(work, &work->decoded_workspace, (size_t)bytes,
            NULL, 0, "cuda.decoded-mxfp4", NULL, err);
        if (rc == YVEX_OK) work->decoded_capacity = bytes;
    }
    if (rc != YVEX_OK) return rc;
    CUdeviceptr digits = work->decoded_workspace, metadata = digits + elements * 3ull;
    CUdeviceptr eligibility = metadata + elements / 8ull;
    void *prepare[] = {&input, &f32, &width, &prepared_rows, &digits, &metadata, &status, &eligibility};
    void *parameters[] = {&encoded, &input, &digits, &metadata, &width,
        &rows, &groups, &inputs, &output, &output_bf16, &status, &eligibility};
    rc = decoded_launch(work, work->state->decoded_prepare_function,
        prepared_rows, 256u, prepare, err);
    if (rc == YVEX_OK)
        rc = decoded_launch(work, work->state->decoded_mxfp4_function,
            (rows * groups * inputs + 7u) / 8u, 256u, parameters, err);
    return rc;
}

int yvex_cuda_decoded_rows(yvex_cuda_work *work,
    const yvex_backend_attention_weight *weight, CUdeviceptr encoded,
    unsigned long long start_row, unsigned long long row_count,
    unsigned long long input_rows, CUdeviceptr input, CUdeviceptr output,
    int output_bf16, CUdeviceptr status, yvex_error *err)
{
    unsigned width = weight ? (unsigned)weight->row_width : 0u, rows = (unsigned)row_count;
    unsigned inputs = (unsigned)input_rows, f32 = YVEX_GGUF_QTYPE_F32;
    unsigned long long weight_elements, input_elements;
    unsigned long long bytes = yvex_cuda_decoded_workspace(input_rows);
    CUdeviceptr left, lm, right, rm;
    int rc = YVEX_OK;
    if (!work || !weight || !work->state || !encoded || !input || !output || !status || !bytes ||
        weight->row_width > 4096ull || row_count > 1024ull ||
        !width || width > 4096u || width % 32u || !rows || rows > 1024u ||
        weight->qtype != YVEX_GGUF_QTYPE_BF16 || weight->row_bytes != width * 2ull ||
        start_row > weight->row_count || row_count > weight->row_count - start_row ||
        (output_bf16 != 0 && output_bf16 != 1)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.decoded-matrix",
                       "decoded matrix preparation requires bounded typed rows");
        return YVEX_ERR_INVALID_ARG;
    }
    weight_elements = row_count * weight->row_width;
    input_elements = input_rows * weight->row_width;
    /* The small-row envelope is sized for grouped MXFP4 preparation. A
     * direct BF16 caller may still request a larger weight-side layout;
     * refuse before allocation or launch instead of overflowing that arena. */
    if ((weight_elements + input_elements) * 25ull / 8ull > bytes) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.decoded-matrix",
                       "decoded preparation exceeds the admitted workspace");
        return YVEX_ERR_BOUNDS;
    }
    if (bytes > work->decoded_capacity) {
        rc = yvex_cuda_work_allocate(work, &work->decoded_workspace, (size_t)bytes,
            NULL, 0, "cuda.decoded-matrix", NULL, err);
        if (rc == YVEX_OK) work->decoded_capacity = bytes;
    }
    if (rc != YVEX_OK) return rc;
    encoded += start_row * weight->row_bytes;
    left = work->decoded_workspace;
    lm = left + weight_elements * 3ull;
    right = lm + weight_elements / 8ull;
    rm = right + input_elements * 3ull;
    CUdeviceptr no_eligibility = 0ull;
    void *weight_parameters[] = {&encoded, (void *)&weight->qtype, &width,
        &rows, &left, &lm, &status, &no_eligibility};
    void *input_parameters[] = {&input, &f32, &width, &inputs, &right, &rm, &status, &no_eligibility};
    void *parameters[] = {&left, &lm, &right, &rm, &width, &rows, &inputs,
        &output, &encoded, &input, (void *)&weight->qtype, &output_bf16, &status};
    rc = decoded_launch(work, work->state->decoded_prepare_function,
        (unsigned)((weight_elements + 255ull) / 256ull), 256u, weight_parameters, err);
    if (rc == YVEX_OK)
        rc = decoded_launch(work, work->state->decoded_prepare_function,
            (unsigned)((input_elements + 255ull) / 256ull), 256u, input_parameters, err);
    if (rc == YVEX_OK)
        rc = decoded_launch(work, work->state->decoded_rows_function,
            ((rows + 15u) / 16u) * ((inputs + 31u) / 32u), 128u, parameters, err);
    if (rc == YVEX_OK) work->tensor_core_launches++;
    return rc;
}
