/* Own bounded lossless decoded-matrix preparation and certified projection. */
#include "src/backend/cuda/private.h"

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
