/* Private stream/graph primitives shared by encoded attention and MoE execution.
 * This owner validates and submits; callers retain transactional publication. */
#include "src/backend/cuda/attention_ops.h"
#include "src/backend/cuda/transformer_ops.h"
#include <yvex/internal/graph_state.h>
#include <yvex/internal/transformer.h>
#include <yvex/quant.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CUDA_ATTENTION_BLOCK 256u
static int cuda_work_launch(yvex_cuda_work *work,
                            CUfunction function,
                            unsigned int grid,
                            unsigned int block,
                            unsigned int shared_bytes,
                            void **params,
                            const char *stage,
                            yvex_error *err)
{
    int rc;
    if (!work || !work->backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage, "CUDA work owner is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (work->prepare_only)
        return yvex_cuda_graph_kernel_update(
            work->backend, work->variant, function, grid, block, shared_bytes,
            params, stage, err);
    rc = yvex_cuda_launch(work->backend, work->variant, function, grid, block,
                          shared_bytes, params, stage, err);
    if (rc == YVEX_OK && !yvex_cuda_capture_active(work->backend))
        work->launches++;
    return rc;
}
static int attention_fail(yvex_backend_attention_failure *failure,
                             yvex_backend_attention_failure_code code,
                             const char *stage,
                             unsigned long long expected,
                             unsigned long long actual,
                             yvex_error *err,
                             yvex_status status,
                             const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->stage = stage;
        failure->expected = expected;
        failure->actual = actual;
    }
    if (!yvex_error_is_set(err)) yvex_error_set(err, status, stage, message);
    return status;
}
static int attention_account_transfer(
    unsigned long long count, size_t width, unsigned long long *total,
    const char *stage, yvex_backend_attention_failure *failure, yvex_error *err)
{
    size_t bytes;
    unsigned long long next;
    if (!total ||
        !yvex_cuda_work_checked_bytes(count, (unsigned long long)width, &bytes) ||
        !yvex_core_u64_add(*total, (unsigned long long)bytes, &next))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage, ULLONG_MAX,
            count, err, YVEX_ERR_BOUNDS,
            "CUDA attention transfer accounting overflowed");
    *total = next;
    return YVEX_OK;
}
/* Acquire one bounded, family-neutral attention transaction range. */
static int attention_allocate(yvex_cuda_work *work,
                                 CUdeviceptr *out,
                                 size_t bytes,
                                 const void *source,
                                 int zero,
                                 const char *stage,
                                 yvex_backend_attention_failure *failure,
                                 yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    yvex_cuda_work_failure work_failure = YVEX_CUDA_WORK_FAILURE_NONE;
    yvex_backend_attention_failure_code code;
    const char *message;
    int rc;
    if (injected && strcmp(injected, "allocation") == 0)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION, stage, bytes,
            0ull, err, YVEX_ERR_NOMEM, "injected CUDA attention allocation failure");
    if (injected && source && strcmp(injected, "copy-input") == 0)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, YVEX_ERR_BACKEND, "injected CUDA attention input copy failure");
    rc = yvex_cuda_work_allocate(work, out, bytes, source, zero, stage,
                                 &work_failure, err);
    if (rc == YVEX_OK) return YVEX_OK;
    code = work_failure == YVEX_CUDA_WORK_FAILURE_BUDGET
        ? YVEX_BACKEND_ATTENTION_FAILURE_BUDGET
        : work_failure == YVEX_CUDA_WORK_FAILURE_COPY
              ? YVEX_BACKEND_ATTENTION_FAILURE_COPY
              : YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION;
    message = code == YVEX_BACKEND_ATTENTION_FAILURE_BUDGET
        ? "CUDA attention reusable device budget is insufficient"
        : code == YVEX_BACKEND_ATTENTION_FAILURE_COPY
              ? "CUDA attention device initialization failed"
              : "CUDA attention device allocation failed";
    rc = attention_fail(failure, code, stage, bytes, 0ull, err,
                        (yvex_status)rc, message);
    if (code == YVEX_BACKEND_ATTENTION_FAILURE_BUDGET &&
        work->backend->workspace_device_tensor)
        yvex_error_setf(
            err, (yvex_status)rc, stage,
            "CUDA reusable workspace needs %zu bytes at cursor %llu of %llu",
            bytes, work->backend->workspace_cursor,
            work->backend->workspace_bytes);
    return rc;
}
static int attention_initialize(yvex_cuda_work *work, CUdeviceptr target,
                                   size_t bytes, const void *source, int zero,
                                   const char *stage,
                                   yvex_backend_attention_failure *failure,
                                   yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    int rc;
    if (!work || !work->backend)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, YVEX_ERR_INVALID_ARG, "CUDA attention initialization is invalid");
    rc = backend_dispatch_admit(work->backend, stage, err);
    if (rc != YVEX_OK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, (yvex_status)rc, "CUDA attention backend is cleanup-only");
    if (injected && source && strcmp(injected, "copy-input") == 0)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, YVEX_ERR_BACKEND, "injected CUDA attention input copy failure");
    rc = yvex_cuda_work_initialize(work, target, bytes, source, zero, stage, err);
    return rc == YVEX_OK ? YVEX_OK : attention_fail(
        failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull, err,
        (yvex_status)rc, "CUDA attention range initialization failed");
}
static int attention_download(yvex_cuda_work *work, void *target,
                                 CUdeviceptr source, size_t bytes,
                                 const char *stage,
                                 yvex_backend_attention_failure *failure,
                                 yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    CUstream stream;
    int rc;
    if (!work || !work->state || !target || !source || !bytes)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, YVEX_ERR_INVALID_ARG, "CUDA attention download is invalid");
    rc = backend_dispatch_admit(work->backend, stage, err);
    if (rc != YVEX_OK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, (yvex_status)rc, "CUDA attention backend is cleanup-only");
    if (work->prepare_only) return YVEX_OK;
    if (injected && (strcmp(injected, "copy-output") == 0 ||
                     strcmp(injected, stage) == 0))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, YVEX_ERR_BACKEND, "injected CUDA attention output copy failure");
    stream = yvex_cuda_launch_stream(work->backend);
    if (stream && !work->state->driver.cuMemcpyDtoHAsync_v2)
        rc = YVEX_ERR_UNSUPPORTED;
    else
        rc = yvex_cuda_status(
            &work->state->driver,
            stream ? work->state->driver.cuMemcpyDtoHAsync_v2(
                         target, source, bytes, stream)
                   : work->state->driver.cuMemcpyDtoH_v2(target, source, bytes),
            stage, err);
    return rc == YVEX_OK ? YVEX_OK : attention_fail(
        failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull, err,
        (yvex_status)rc, "CUDA attention staged output copy failed");
}
/* Launch only; the family transaction retains synchronization and publication. */
static int attention_launch(yvex_cuda_work *work,
                               CUfunction function,
                               unsigned int grid,
                               unsigned int block,
                               unsigned int shared_bytes,
                               void **params,
                               const char *stage,
                               yvex_backend_attention_failure *failure,
                               yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    int rc;
    if (!work || !work->backend)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, YVEX_ERR_INVALID_ARG, "CUDA attention launch is invalid");
    rc = backend_dispatch_admit(work->backend, stage, err);
    if (rc != YVEX_OK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, (yvex_status)rc, "CUDA attention backend is cleanup-only");
    if (injected && strcmp(injected, stage) == 0)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, YVEX_ERR_BACKEND, "injected CUDA attention kernel launch failure");
    rc = cuda_work_launch(work, function, grid, block, shared_bytes,
                          params, stage, err);
    if (rc != YVEX_OK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, (yvex_status)rc, "CUDA attention kernel launch failed");
    return YVEX_OK;
}
static int attention_round_bf16(
    yvex_cuda_work *work, CUdeviceptr values, unsigned long long count,
    CUdeviceptr status, const char *stage,
    yvex_backend_attention_failure *failure, yvex_error *err)
{
    unsigned int grid;
    if (!count || count > UINT_MAX * (unsigned long long)CUDA_ATTENTION_BLOCK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            UINT_MAX, count, err, YVEX_ERR_BOUNDS,
            "CUDA attention BF16-round geometry is invalid");
    grid = (unsigned int)((count + CUDA_ATTENTION_BLOCK - 1ull) /
                          CUDA_ATTENTION_BLOCK);
    {
        void *params[] = {&values, &count, &status};
        return attention_launch(
            work, work->state->attention_bf16_round_function, grid,
            CUDA_ATTENTION_BLOCK, 0u, params, stage, failure, err);
    }
}
static int attention_mxfp4_q8_rows_geometry(
    const yvex_backend_attention_weight *weight, unsigned long long rows,
    unsigned long long input_rows, unsigned int *grid,
    unsigned int *block, unsigned int *shared_bytes)
{
    unsigned long long row_blocks, launch_blocks, activation_bytes;
    if (!weight || weight->qtype != YVEX_GGUF_QTYPE_MXFP4 ||
        rows < 4096ull || weight->row_width != 1024ull ||
        !input_rows || input_rows > 8ull ||
        !grid || !block || !shared_bytes)
        return 0;
    row_blocks = (rows + 7ull) / 8ull;
    if (!yvex_core_u64_mul(row_blocks, input_rows, &launch_blocks) ||
        !yvex_core_u64_mul(weight->row_width / 256ull, 292ull,
                           &activation_bytes) ||
        !launch_blocks || launch_blocks > UINT_MAX ||
        activation_bytes > UINT_MAX)
        return 0;
    *grid = (unsigned int)launch_blocks;
    *block = CUDA_ATTENTION_BLOCK;
    *shared_bytes = (unsigned int)activation_bytes;
    return 1;
}
static int attention_matvec(yvex_cuda_work *work,
                               const yvex_backend_attention_weight *weight,
                               CUdeviceptr device_weight,
                               unsigned long long start_row,
                               unsigned long long rows,
                               unsigned long long input_rows,
                               CUdeviceptr vector,
                               CUdeviceptr out,
                               int output_bf16,
                               CUdeviceptr status,
                               const char *stage,
                               yvex_backend_attention_failure *failure,
                               yvex_error *err)
{
    CUdeviceptr additive = 0ull;
    unsigned long long group_count = 1ull, group_rows = rows;
    int block_row = 0, q8_path, q8_input = 0, shared_q8_path, tensorcore_path;
    unsigned int matvec_grid, matvec_block, tensorcore_grid = 0u, tensorcore_block = 0u;
    unsigned int shared_q8_grid = 0u, shared_q8_block = 0u, shared_q8_bytes = 0u;
    q8_path = weight && work->activation_q8 && !work->forensic_numeric &&
              weight->row_width % 256ull == 0ull &&
              yvex_cuda_q8_activation_eligible(weight->qtype) &&
              work->state->q8_quantize_function && work->state->qtype_matvec_function;
    tensorcore_path = q8_path && work->state->qtype_tensorcore_rows_function &&
                      cuda_qtype_tensorcore_eligible(input_rows);
    shared_q8_path = q8_path && !tensorcore_path &&
                     work->state->mxfp4_q8_rows_function &&
                     attention_mxfp4_q8_rows_geometry(
                         weight, rows, input_rows, &shared_q8_grid,
                         &shared_q8_block, &shared_q8_bytes);
    if (!weight || !weight->present || !device_weight || !vector || !out ||
        !rows || !input_rows || start_row > weight->row_count ||
        rows > weight->row_count - start_row ||
        !yvex_cuda_qtype_matvec_geometry(
            rows, weight ? weight->row_width : 0ull, input_rows,
            weight ? weight->qtype : 0u, 1, !q8_path, &matvec_grid, &matvec_block,
            &block_row))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_count : 0ull, start_row + rows, err,
            YVEX_ERR_BOUNDS, "CUDA attention matvec geometry is invalid");
    if (tensorcore_path && !yvex_cuda_qtype_tensorcore_geometry(
                               rows, input_rows, &tensorcore_grid,
                               &tensorcore_block))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            UINT_MAX, tensorcore_grid, err, YVEX_ERR_BOUNDS,
            "CUDA Tensor Core attention grid exceeds launch bounds");
    if (q8_path) {
        unsigned long long blocks = weight->row_width / 256ull;
        unsigned long long quantize_tasks, quantized_bytes;
        CUdeviceptr quantized;
        int rc;
        if (!yvex_core_u64_mul(blocks, input_rows, &quantize_tasks) ||
            !yvex_core_u64_mul(quantize_tasks, 292ull, &quantized_bytes) ||
            quantize_tasks > UINT_MAX)
            return attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
                UINT_MAX, quantize_tasks, err, YVEX_ERR_BOUNDS,
                "CUDA Q8 activation geometry is invalid");
        rc = YVEX_OK;
        if (quantized_bytes > work->q8_capacity) {
            rc = yvex_cuda_work_allocate(work, &work->q8_input,
                                         (size_t)quantized_bytes, NULL, 0,
                                         "cuda.q8-activation", NULL, err);
            if (rc == YVEX_OK) work->q8_capacity = quantized_bytes;
        }
        quantized = work->q8_input;
        if (rc == YVEX_OK) {
            void *params[] = {&quantized, &vector, (void *)&weight->row_width,
                              &input_rows, &group_count,
                              (void *)&weight->row_width, &status};
            rc = attention_launch(work, work->state->q8_quantize_function,
                                  (unsigned int)quantize_tasks, CUDA_ATTENTION_BLOCK, 0u,
                                  params, "cuda.q8-activation", failure, err);
        }
        if (rc == YVEX_OK && tensorcore_path) {
            void *params[] = {
                &device_weight, (void *)&weight->row_bytes,
                (void *)&weight->row_width, &start_row, &rows, &input_rows,
                &group_count, &group_rows, (void *)&weight->qtype, &quantized,
                &additive, &out, &output_bf16, &status};
            rc = attention_launch(
                work, cuda_qtype_tensorcore_function(work->state, rows, input_rows, weight->qtype),
                tensorcore_grid, tensorcore_block, 0u, params, stage,
                failure, err);
            if (rc == YVEX_OK) work->tensor_core_launches++;
        } else if (rc == YVEX_OK && shared_q8_path) {
            void *params[] = {
                &device_weight, (void *)&weight->row_bytes,
                (void *)&weight->row_width, &start_row, &rows, &input_rows,
                &quantized, &out, &rows, &output_bf16, &status};
            rc = attention_launch(
                work, work->state->mxfp4_q8_rows_function,
                shared_q8_grid, shared_q8_block, shared_q8_bytes, params,
                stage, failure, err);
        } else if (rc == YVEX_OK) {
            q8_input = 1;
            void *params[] = {
                &device_weight, (void *)&weight->row_bytes,
                (void *)&weight->row_width, &start_row, &rows, &input_rows,
                (void *)&weight->qtype, &quantized,
                (void *)&weight->row_width, &q8_input, &block_row,
                &work->forensic_numeric, &additive, &out, &rows,
                &output_bf16, &status};
            rc = attention_launch(work, work->state->qtype_matvec_function,
                                  matvec_grid, matvec_block, 0u, params, stage,
                                  failure, err);
        }
        return rc;
    }
    if (!work->forensic_numeric && work->activation_q8 &&
        work->state->decoded_prepare_function && work->state->decoded_rows_function &&
        weight->qtype == YVEX_GGUF_QTYPE_BF16 && weight->row_width <= 4096ull &&
        weight->row_width % 32ull == 0ull && rows <= 1024ull && input_rows >= 32ull &&
        yvex_cuda_decoded_workspace(input_rows))
        return yvex_cuda_decoded_rows(work, weight, device_weight, start_row,
            rows, input_rows, vector, out, output_bf16, status, err);
    {
        void *params[] = {&device_weight, (void *)&weight->row_bytes,
            (void *)&weight->row_width, &start_row, &rows,
            &input_rows, (void *)&weight->qtype, &vector,
            (void *)&weight->row_width, &q8_input,
            &block_row, &work->forensic_numeric, &additive, &out,
            &rows, &output_bf16, &status
        };
        return attention_launch(
            work, work->state->qtype_matvec_function,
            matvec_grid, matvec_block, 0u, params, stage, failure, err);
    }
}
static int attention_matvec_grouped(
    yvex_cuda_work *work, const yvex_backend_attention_weight *weight,
    CUdeviceptr device_weight, unsigned long long groups, unsigned long long group_rows,
    unsigned long long input_rows, CUdeviceptr vector, unsigned long long input_stride,
    CUdeviceptr out, unsigned long long output_stride, int output_bf16,
    CUdeviceptr status, const char *stage, yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    CUdeviceptr additive = 0ull;
    unsigned long long rows = 0ull, input_width, quantized_rows, quantize_tasks;
    unsigned long long grouped_grid, quantized_bytes;
    int block_row = 0, q8_input = 0, q8_path;
    unsigned int grid, block, tensorcore_grid = 0u, tensorcore_block = 0u;
    if (!weight || !weight->present || !device_weight || !vector || !out ||
        !groups || !group_rows || !input_rows || groups > ULLONG_MAX / group_rows ||
        !yvex_core_u64_mul(groups, group_rows, &rows) ||
        !yvex_core_u64_mul(groups, weight->row_width, &input_width) ||
        rows != weight->row_count || input_stride < input_width ||
        output_stride < rows ||
        !yvex_cuda_qtype_matvec_geometry(group_rows, weight->row_width, input_rows,
                                         weight->qtype, 1, 1, &grid, &block, &block_row))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_count : 0ull, rows, err, YVEX_ERR_BOUNDS,
            "CUDA grouped attention matvec geometry is invalid");
    /* Weight rows are group-major while activations stay token-major. Quantizing
     * token-by-group views lets one MMA launch preserve both physical layouts. */
    q8_path = work->activation_q8 && !work->forensic_numeric &&
              weight->row_width % 256ull == 0ull && group_rows % 16ull == 0ull &&
              yvex_cuda_q8_activation_eligible(weight->qtype) &&
              work->state->q8_quantize_function &&
              work->state->qtype_tensorcore_rows_function &&
              cuda_qtype_tensorcore_eligible(input_rows);
    if (q8_path) {
        unsigned long long blocks = weight->row_width / 256ull;
        CUdeviceptr quantized;
        int rc;
        if (!yvex_core_u64_mul(input_rows, groups, &quantized_rows) ||
            !yvex_core_u64_mul(quantized_rows, blocks, &quantize_tasks) ||
            !yvex_core_u64_mul(quantize_tasks, 292ull, &quantized_bytes) ||
            !yvex_cuda_qtype_tensorcore_geometry(
                rows, input_rows, &tensorcore_grid, &tensorcore_block) ||
            quantize_tasks > UINT_MAX ||
            quantized_bytes > SIZE_MAX)
            return attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
                UINT_MAX, quantize_tasks, err, YVEX_ERR_BOUNDS,
                "CUDA grouped Tensor Core geometry exceeds launch bounds");
        rc = YVEX_OK;
        if (quantized_bytes > work->q8_capacity) {
            rc = yvex_cuda_work_allocate(work, &work->q8_input,
                                         (size_t)quantized_bytes, NULL, 0,
                                         "cuda.q8-activation", NULL, err);
            if (rc == YVEX_OK) work->q8_capacity = quantized_bytes;
        }
        quantized = work->q8_input;
        if (rc == YVEX_OK) {
            void *params[] = {&quantized, &vector, (void *)&weight->row_width,
                              &quantized_rows, &groups, &input_stride, &status};
            rc = attention_launch(work, work->state->q8_quantize_function,
                                  (unsigned int)quantize_tasks,
                                  CUDA_ATTENTION_BLOCK, 0u, params,
                                  "cuda.q8-activation", failure, err);
        }
        if (rc == YVEX_OK) {
            unsigned long long start_row = 0ull;
            void *params[] = {
                &device_weight, (void *)&weight->row_bytes,
                (void *)&weight->row_width, &start_row, &rows, &input_rows,
                &groups, &group_rows, (void *)&weight->qtype, &quantized,
                &additive, &out, &output_bf16, &status};
            rc = attention_launch(
                work, cuda_qtype_tensorcore_function(work->state, rows, input_rows, weight->qtype),
                tensorcore_grid, tensorcore_block, 0u, params, stage,
                failure, err);
            if (rc == YVEX_OK) work->tensor_core_launches++;
        }
        return rc;
    }
    if (!work->forensic_numeric && !block_row &&
        work->state->qtype_grouped_rows_function) {
        if (weight->qtype == YVEX_GGUF_QTYPE_MXFP4 &&
            weight->row_width <= 8192ull && weight->row_width % 32ull == 0ull &&
            input_rows <= 16ull && groups <= 8ull && group_rows <= 16384ull &&
            input_stride == input_width && output_stride == rows &&
            work->state->decoded_prepare_function && work->state->decoded_mxfp4_function)
            return yvex_cuda_decoded_mxfp4(work, weight, device_weight,
                groups, group_rows, input_rows, vector, out, output_bf16, status, err);
        unsigned long long blocks_per_group = grid;
        if (!yvex_core_u64_mul(groups, blocks_per_group, &grouped_grid) ||
            grouped_grid > UINT_MAX)
            return attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
                UINT_MAX, grouped_grid, err, YVEX_ERR_BOUNDS,
                "CUDA grouped rows grid exceeds launch bounds");
        {
            void *params[] = {
                &device_weight, (void *)&weight->row_bytes,
                (void *)&weight->row_width, &groups, &group_rows,
                &blocks_per_group, &input_rows, (void *)&weight->qtype, &vector,
                &input_stride, &out, &output_stride, &output_bf16, &status};
            return attention_launch(
                work, work->state->qtype_grouped_rows_function,
                (unsigned int)grouped_grid, block, 0u, params, stage,
                failure, err);
        }
    }
    for (unsigned long long group = 0ull; group < groups; ++group) {
        unsigned long long start_row = group * group_rows;
        CUdeviceptr input = vector + group * weight->row_width * sizeof(float);
        CUdeviceptr output = out + group * group_rows * sizeof(float);
        void *params[] = {
            &device_weight, (void *)&weight->row_bytes, (void *)&weight->row_width,
            &start_row, &group_rows, &input_rows, (void *)&weight->qtype, &input,
            &input_stride, &q8_input, &block_row, &work->forensic_numeric,
            &additive, &output, &output_stride, &output_bf16, &status};
        int rc = attention_launch(work, work->state->qtype_matvec_function,
                                  grid, block, 0u, params, stage,
                                  failure, err);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}
static int attention_decode(yvex_cuda_work *work,
                               const yvex_backend_attention_weight *weight,
                               CUdeviceptr device_weight,
                               unsigned long long row,
                               unsigned long long count,
                               CUdeviceptr out,
                               CUdeviceptr status,
                               const char *stage,
                               yvex_backend_attention_failure *failure,
                               yvex_error *err)
{
    CUdeviceptr encoded;
    unsigned int grid;
    if (!weight || !weight->present || row >= weight->row_count ||
        count != weight->row_width ||
        count > UINT_MAX * (unsigned long long)CUDA_ATTENTION_BLOCK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_width : 0ull, count, err, YVEX_ERR_BOUNDS,
            "CUDA attention decode geometry is invalid");
    encoded = device_weight + row * weight->row_bytes;
    grid = (unsigned int)((count + CUDA_ATTENTION_BLOCK - 1ull) /
                          CUDA_ATTENTION_BLOCK);
    {
        void *params[] = {
            &encoded, &count, (void *)&weight->qtype, &out, &status
        };
        return attention_launch(
            work, work->state->encoded_row_decode_function, grid,
            CUDA_ATTENTION_BLOCK, 0u, params, stage, failure, err);
    }
}
static int attention_rolling_phase(
    yvex_cuda_work *work, const yvex_cuda_attention_rolling_phase *phase,
    const char *stage, yvex_backend_attention_failure *failure, yvex_error *err)
{
    const yvex_backend_attention_rolling *rolling = phase ? phase->rolling : NULL;
    unsigned long long extent;
    int rc;
    if (!rolling || !phase->weights || !phase->device_weights || !phase->rows ||
        phase->rows > UINT_MAX || !rolling->head_dimension || rolling->head_dimension > UINT_MAX ||
        !rolling->ratio || rolling->cursor >= rolling->ratio ||
        (phase->checkpoints != 0 && phase->checkpoints != 1) ||
        !yvex_core_u64_mul(rolling->state_width, rolling->state_slots, &extent))
        return attention_fail(failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            stage, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG, "invalid rolling phase geometry");
    rc = attention_matvec(work, &phase->weights[0], phase->device_weights[0], 0ull,
        rolling->state_width, phase->rows, phase->input, phase->kv, 0,
        phase->status, stage, failure, err);
    if (rc == YVEX_OK)
        rc = attention_matvec(work, &phase->weights[1], phase->device_weights[1], 0ull,
            rolling->state_width, phase->rows, phase->input, phase->score, 0,
            phase->status, stage, failure, err);
    if (rc != YVEX_OK) return rc;
    if (phase->rows > 1ull) {
        void *params[] = {(void *)&phase->state_kv, (void *)&phase->state_score,
            (void *)&phase->kv, (void *)&phase->score, (void *)&phase->device_weights[2],
            (void *)&phase->weights[2].row_bytes, (void *)&phase->weights[2].qtype,
            (void *)&phase->emissions, (void *)&rolling->ratio, (void *)&rolling->head_dimension,
            (void *)&phase->rows, (void *)&rolling->cursor, (void *)&rolling->overlap,
            (void *)&phase->checkpoints, (void *)&phase->status};
        return attention_launch(work, work->state->attention_rolling_rows_function,
            (unsigned int)((rolling->head_dimension + CUDA_ATTENTION_BLOCK - 1ull) / CUDA_ATTENTION_BLOCK),
            CUDA_ATTENTION_BLOCK, 0u, params, stage, failure, err);
    }
    rc = attention_decode(work, &phase->weights[2], phase->device_weights[2],
        rolling->cursor, rolling->state_width, phase->ape, phase->status, stage, failure, err);
    if (rc == YVEX_OK) {
        CUdeviceptr after_kv = phase->state_kv + (phase->checkpoints ? extent * sizeof(float) : 0ull);
        CUdeviceptr after_score = phase->state_score + (phase->checkpoints ? extent * sizeof(float) : 0ull);
        int emit = rolling->cursor + 1ull == rolling->ratio;
        void *params[] = {(void *)&phase->state_kv, (void *)&phase->state_score,
            (void *)&phase->kv, (void *)&phase->score, (void *)&phase->ape,
            &after_kv, &after_score, (void *)&phase->emissions, (void *)&rolling->ratio,
            (void *)&rolling->head_dimension, (void *)&rolling->state_width,
            (void *)&rolling->state_slots, (void *)&rolling->cursor,
            (void *)&rolling->overlap, &emit, (void *)&phase->status};
        rc = attention_launch(work, work->state->attention_rolling_state_function,
            1u, CUDA_ATTENTION_BLOCK, 0u, params, stage, failure, err);
    }
    return rc;
}

static int attention_weighted_norm(
    yvex_cuda_work *work, CUdeviceptr values, unsigned long long count,
    unsigned long long vectors,
    const yvex_backend_attention_weight *weight, CUdeviceptr device_weight,
    double epsilon, CUdeviceptr status, const char *stage,
    yvex_backend_attention_failure *failure, yvex_error *err)
{
    if (!weight || !weight->present || !vectors || vectors > UINT_MAX ||
        weight->row_count != 1ull || weight->row_width != count)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            count, weight ? weight->row_width : 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention normalization weight shape is invalid");
    {
        void *params[] = {
            &values, &count, &device_weight, (void *)&weight->qtype,
            &epsilon, &vectors, &status
        };
        return attention_launch(
            work, work->state->attention_weighted_norm_function, (unsigned int)vectors,
            CUDA_ATTENTION_BLOCK, CUDA_ATTENTION_BLOCK * sizeof(double),
            params, stage, failure, err);
    }
}
static int attention_unit_norm(yvex_cuda_work *work,
                                  CUdeviceptr values,
                                  unsigned long long vectors,
                                  unsigned long long width,
                                  double epsilon,
                                  CUdeviceptr status,
                                  const char *stage,
                                  yvex_backend_attention_failure *failure,
                                  yvex_error *err)
{
    if (!vectors || vectors > UINT_MAX || !width)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            1ull, vectors, err, YVEX_ERR_BOUNDS,
            "CUDA attention unit-norm geometry is invalid");
    {
        void *params[] = {&values, &vectors, &width, &epsilon, &status};
        return attention_launch(
            work, work->state->attention_unit_norm_function,
            (unsigned int)vectors, CUDA_ATTENTION_BLOCK,
            CUDA_ATTENTION_BLOCK * (unsigned int)sizeof(double), params, stage,
            failure, err);
    }
}
static int attention_rope(yvex_cuda_work *work,
                             CUdeviceptr values,
                             unsigned long long vectors_per_token,
                             unsigned long long token_count,
                                  unsigned long long width,
                                  unsigned long long token_position,
                                  unsigned long long position_step,
                                  const yvex_backend_attention_position *position,
                             int inverse,
                             CUdeviceptr status,
                             const char *stage,
                             yvex_backend_attention_failure *failure,
                             yvex_error *err)
{
    unsigned long long total_vectors, total;
    unsigned int grid;
    if (!position || !position_step || !token_count ||
        token_count - 1ull > (ULLONG_MAX - token_position) / position_step ||
        !position->rope_dimensions ||
        position->rope_dimensions > width ||
        !yvex_core_u64_mul(vectors_per_token, token_count, &total_vectors) ||
        total_vectors > ULLONG_MAX / (position->rope_dimensions / 2ull))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            width, position ? position->rope_dimensions : 0ull, err,
            YVEX_ERR_BOUNDS, "CUDA attention RoPE geometry is invalid");
    total = total_vectors * (position->rope_dimensions / 2ull);
    if (!total || total > UINT_MAX * (unsigned long long)CUDA_ATTENTION_BLOCK)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            UINT_MAX, total, err, YVEX_ERR_BOUNDS,
            "CUDA attention RoPE launch extent is invalid");
    grid = (unsigned int)((total + CUDA_ATTENTION_BLOCK - 1ull) /
                          CUDA_ATTENTION_BLOCK);
    {
        void *params[] = {
            &values, &vectors_per_token, &token_count, &width,
            (void *)&position->rope_dimensions, &token_position, &position_step,
            (void *)&position->theta,
            (void *)&position->scaling_factor,
            (void *)&position->original_context, (void *)&position->beta_fast,
            (void *)&position->beta_slow, &inverse, &status
        };
        return attention_launch(
            work, work->state->attention_yarn_rope_function, grid,
            CUDA_ATTENTION_BLOCK, 0u, params, stage, failure, err);
    }
}
static int attention_activation(
    yvex_cuda_work *work, CUdeviceptr values, unsigned long long vectors,
    unsigned long long width, unsigned long long stride,
    const yvex_backend_attention_activation *policy,
    CUdeviceptr status, const char *stage,
    yvex_backend_attention_failure *failure, yvex_error *err)
{
    if (!policy || !policy->required) return YVEX_OK;
    if (!vectors || vectors > UINT_MAX || !width || stride < width ||
        !policy->block_width || width % policy->block_width)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            policy->block_width, width, err, YVEX_ERR_BOUNDS,
            "CUDA attention activation geometry is invalid");
    {
        void *params[] = {
            &values, &vectors, &width, &stride, (void *)&policy->block_width,
            (void *)&policy->quantization, (void *)&policy->hadamard, &status
        };
        return attention_launch(
            work, work->state->attention_activation_quantize_function,
            (unsigned int)vectors, CUDA_ATTENTION_BLOCK, 0u, params, stage,
            failure, err);
    }
}
static int attention_validate_job(yvex_backend_attention_job *job,
                                  yvex_backend_attention_output *output,
                                  yvex_backend_attention_failure *failure,
                                  yvex_error *err)
{
    unsigned long long input_width =
        job && job->operation_scope == YVEX_BACKEND_ATTENTION_SCOPE_ENVELOPE
            ? job->residual_expanded_width : job ? job->hidden_width : 0ull;
    if (job && !job->local_count && !job->local_stride)
        job->local_stride = job->head_dimension;
    if (job && job->attention_class != YVEX_BACKEND_ATTENTION_SWA &&
        !job->compressed_count && !job->compressed_stride)
        job->compressed_stride = job->head_dimension;
    if (job && job->attention_class == YVEX_BACKEND_ATTENTION_CSA &&
        !job->indexer_count && !job->indexer_stride)
        job->indexer_stride = job->indexer_head_dimension;
    if (!job || !output || job->schema != YVEX_BACKEND_ATTENTION_JOB_SCHEMA ||
        (job->history_capacity_known != 0 && job->history_capacity_known != 1) ||
        (job->history_capacity_known &&
         ((job->local_count > job->local_capacity &&
           (!job->candidate_block_visible || job->local_count - job->local_capacity > job->token_count)) ||
          job->compressed_count > job->compressed_capacity ||
          job->indexer_count > job->indexer_capacity)) ||
        (!job->input && !job->device_input) || !job->token_count || !input_width ||
        job->input_stride < input_width ||
        job->token_position > ULLONG_MAX - job->token_count ||
        (job->phase == YVEX_BACKEND_ATTENTION_PHASE_DECODE &&
         job->token_count != 1ull) ||
        (job->phase != YVEX_BACKEND_ATTENTION_PHASE_DECODE &&
         job->phase != YVEX_BACKEND_ATTENTION_PHASE_PREFILL &&
         job->phase != YVEX_BACKEND_ATTENTION_PHASE_SPECULATIVE_DRAFT &&
         job->phase != YVEX_BACKEND_ATTENTION_PHASE_SPECULATIVE_VERIFY) ||
        (job->candidate_block_visible != 0 &&
         job->candidate_block_visible != 1) ||
        (job->candidate_block_visible &&
         (job->phase != YVEX_BACKEND_ATTENTION_PHASE_SPECULATIVE_DRAFT ||
          job->attention_class != YVEX_BACKEND_ATTENTION_SWA ||
          job->token_count < 2ull)) ||
        !job->hidden_width || !job->q_rank || !job->query_heads ||
        !job->head_dimension || !job->kv_width || !job->max_device_bytes ||
        job->query_heads > ULLONG_MAX / job->head_dimension ||
        job->query_heads * job->head_dimension >
            (unsigned long long)SIZE_MAX / sizeof(float) ||
        (job->cancellation && !job->cancellation->requested))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate", 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention job and output geometry are required");
    if (job->operation_scope != YVEX_BACKEND_ATTENTION_SCOPE_CORE &&
        job->operation_scope != YVEX_BACKEND_ATTENTION_SCOPE_ENVELOPE)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.scope", YVEX_BACKEND_ATTENTION_SCOPE_ENVELOPE,
            job->operation_scope, err, YVEX_ERR_FORMAT,
            "CUDA attention operation scope is invalid");
    if (job->local_stride != job->head_dimension ||
        (job->local_count && (!job->local_kv || !job->local_positions)))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.local_history", job->head_dimension,
            job->local_stride, err, YVEX_ERR_FORMAT,
            "CUDA attention local history is incomplete");
    if (job->compressed_count &&
        (!job->compressed_kv || !job->compressed_positions ||
         job->compressed_stride != job->head_dimension))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.compressed_history", job->head_dimension,
            job->compressed_stride, err, YVEX_ERR_FORMAT,
            "CUDA attention compressed history is incomplete");
    if (job->compute_contract != YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.compute_contract",
            YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
            job->compute_contract, err, YVEX_ERR_UNSUPPORTED,
            "CUDA attention compute contract is unavailable");
    if (job->attention_class < YVEX_BACKEND_ATTENTION_SWA ||
        job->attention_class > YVEX_BACKEND_ATTENTION_HCA)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.class", YVEX_BACKEND_ATTENTION_HCA,
            job->attention_class, err, YVEX_ERR_FORMAT,
            "CUDA attention class is not admitted");
    if (job->attention_class == YVEX_BACKEND_ATTENTION_CSA &&
        job->indexer_count != job->compressed_count)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.index_history", job->compressed_count,
            job->indexer_count, err, YVEX_ERR_FORMAT,
            "CUDA CSA indexer/compressed history cardinality differs");
    if (job->evidence_level > 3u)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.evidence", 3ull, job->evidence_level,
            err, YVEX_ERR_FORMAT, "CUDA attention evidence level is invalid");
    if (job->native_execution != 0 && job->native_execution != 1)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.native_execution", 1ull,
            (unsigned long long)job->native_execution, err, YVEX_ERR_FORMAT,
            "CUDA attention native-execution admission is invalid");
    return YVEX_OK;
}
static int attention_validate_weight(const yvex_backend_attention_weight *weight,
                                     unsigned long long rows,
                                     unsigned long long width,
                                     yvex_backend_attention_failure *failure,
                                     yvex_error *err)
{
    const yvex_quant_numeric_capability *capability;
    unsigned long long row_bytes = 0ull, total_bytes = 0ull;
    const char *reason = NULL;
    if (!weight || !weight->present || !weight->encoded ||
        weight->row_count != rows || weight->row_width != width)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.attention.validate.weight_shape", rows,
            weight ? weight->row_count : 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention encoded weight shape is invalid");
    capability = yvex_quant_numeric_capability_at(weight->qtype);
    if (!capability || !capability->dedicated_cuda_compute_available ||
        !yvex_gguf_qtype_storage_bytes(weight->qtype, width, &row_bytes, &reason) ||
        row_bytes != weight->row_bytes ||
        !yvex_core_u64_mul(rows, row_bytes, &total_bytes) ||
        total_bytes != (unsigned long long)weight->encoded_bytes)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.attention.validate.weight_encoding", row_bytes,
            weight->row_bytes, err, YVEX_ERR_UNSUPPORTED,
            reason ? reason : "CUDA attention encoded weight capability is unavailable");
    return YVEX_OK;
}
static int attention_validate_activation(
    const yvex_backend_attention_activation *policy, unsigned long long width,
    const char *stage, yvex_backend_attention_failure *failure, yvex_error *err)
{
    if (!policy || !policy->required) return YVEX_OK;
    if (!width || !policy->block_width || width % policy->block_width ||
        (policy->quantization != 1u && policy->quantization != 2u) ||
        (policy->hadamard &&
         ((width & (width - 1ull)) != 0ull || width > 1024ull)))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            policy->block_width, width, err, YVEX_ERR_FORMAT,
            "CUDA attention activation policy and width are incompatible");
    return YVEX_OK;
}
static int attention_validate_rolling(
    const yvex_backend_attention_job *job,
    const yvex_backend_attention_rolling *rolling, unsigned long long ratio,
    unsigned long long head_dimension, int overlap, unsigned long long *extent,
    const char *stage, yvex_backend_attention_failure *failure, yvex_error *err)
{
    unsigned long long factor = overlap ? 2ull : 1ull, state_width, state_slots;
    if (!job || !rolling || !extent ||
        !yvex_core_u64_mul(head_dimension, factor, &state_width) ||
        !yvex_core_u64_mul(ratio, factor, &state_slots) ||
        !yvex_core_u64_mul(state_width, state_slots, extent) ||
        !rolling->present || rolling->next_token_position != job->token_position ||
        rolling->ratio != ratio || rolling->head_dimension != head_dimension ||
        rolling->state_width != state_width || rolling->state_slots != state_slots ||
        rolling->overlap != overlap || rolling->cursor != job->token_position % ratio ||
        rolling->current_fill != job->token_position % ratio ||
        rolling->previous_fill !=
            (overlap && job->token_position >= ratio ? ratio : 0ull) ||
        !rolling->kv_state || rolling->kv_state_capacity < *extent ||
        !rolling->score_state || rolling->score_state_capacity < *extent)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            ratio, rolling ? rolling->ratio : 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention rolling-state geometry is invalid");
    return YVEX_OK;
}
static int attention_spans_disjoint(const yvex_cuda_host_span *writes,
                                    size_t write_count,
                                    const yvex_cuda_host_span *reads,
                                    size_t read_count)
{
    uintptr_t write_first, write_last, other_first, other_last;
    size_t i, j, bytes;
    for (i = 0u; i < write_count; ++i) {
        if (!writes[i].data ||
            !yvex_cuda_work_checked_bytes(writes[i].count, writes[i].width, &bytes) ||
            (uintptr_t)writes[i].data > UINTPTR_MAX - bytes)
            return 0;
        write_first = (uintptr_t)writes[i].data;
        write_last = write_first + bytes;
        for (j = i + 1u; j < write_count; ++j) {
            if (!writes[j].data ||
                !yvex_cuda_work_checked_bytes(writes[j].count, writes[j].width, &bytes) ||
                (uintptr_t)writes[j].data > UINTPTR_MAX - bytes)
                return 0;
            other_first = (uintptr_t)writes[j].data;
            other_last = other_first + bytes;
            if (write_first < other_last && other_first < write_last) return -1;
        }
        for (j = 0u; j < read_count; ++j) {
            if (!reads[j].count) continue;
            if (!reads[j].data ||
                !yvex_cuda_work_checked_bytes(reads[j].count, reads[j].width, &bytes) ||
                (uintptr_t)reads[j].data > UINTPTR_MAX - bytes)
                return 0;
            other_first = (uintptr_t)reads[j].data;
            other_last = other_first + bytes;
            if (write_first < other_last && other_first < write_last) return -1;
        }
    }
    return 1;
}
static int attention_validate_alias(
    const yvex_backend_attention_job *job,
    const yvex_cuda_attention_transfer *transfers, size_t transfer_count,
    unsigned long long local_extent, unsigned long long compressed_extent,
    unsigned long long index_extent, unsigned long long main_rolling_extent,
    unsigned long long index_rolling_extent)
{
    yvex_cuda_host_span writes[YVEX_CUDA_WORK_MAX_RANGES], reads[40];
    unsigned long long input_width, input_count;
    size_t i, read_count = 0u;
    if (!job || transfer_count > YVEX_CUDA_WORK_MAX_RANGES) return 0;
    input_width = job->operation_scope == YVEX_BACKEND_ATTENTION_SCOPE_ENVELOPE
                      ? job->residual_expanded_width : job->hidden_width;
    if (!yvex_core_u64_mul(job->token_count - 1ull, job->input_stride, &input_count) ||
        !yvex_core_u64_add(input_count, input_width, &input_count))
        return 0;
    for (i = 0u; i < transfer_count; ++i)
        writes[i] = (yvex_cuda_host_span){transfers[i].output,
                                         transfers[i].output_capacity,
                                         transfers[i].width};
#define READ(data_, count_, width_) \
    (reads[read_count++] = (yvex_cuda_host_span){(data_), (count_), (width_)})
    if (!job->device_input) READ(job->input, input_count, sizeof(float));
    READ(job->local_kv, local_extent, sizeof(float));
    READ(job->local_positions, job->local_count, sizeof(unsigned long long));
    READ(job->compressed_kv, compressed_extent, sizeof(float));
    READ(job->compressed_positions, job->compressed_count, sizeof(unsigned long long));
    READ(job->indexer_kv, index_extent, sizeof(float));
    READ(job->indexer_positions, job->indexer_count, sizeof(unsigned long long));
    READ(job->main_rolling.kv_state, main_rolling_extent, sizeof(float));
    READ(job->main_rolling.score_state, main_rolling_extent, sizeof(float));
    READ(job->indexer_rolling.kv_state, index_rolling_extent, sizeof(float));
    READ(job->indexer_rolling.score_state, index_rolling_extent, sizeof(float));
    for (i = 0u; i < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++i)
        READ(job->weights[i].encoded,
             job->weights[i].present ? job->weights[i].encoded_bytes : 0u,
             sizeof(unsigned char));
#undef READ
    return attention_spans_disjoint(writes, transfer_count, reads, read_count);
}
static int attention_cancel(yvex_backend *backend,
                            const yvex_backend_attention_job *job,
                            const char *stage, int pending,
                            yvex_backend_attention_failure *failure,
                            yvex_error *err)
{
    int rc;
    if (!job || !job->cancellation ||
        !job->cancellation->requested(job->cancellation->context))
        return YVEX_OK;
    if (pending) {
        rc = yvex_cuda_synchronize(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, stage, err);
        if (rc != YVEX_OK)
            return attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE, stage,
                1ull, 0ull, err, (yvex_status)rc,
                "CUDA attention cancellation synchronization failed");
    }
    return attention_fail(
        failure, YVEX_BACKEND_ATTENTION_FAILURE_CANCELLED, stage, 0ull, 1ull,
        err, YVEX_ERR_CANCELLED,
        "CUDA attention execution was cancelled before publication");
}
/*
 * Acquire one exact reusable host-staging span for attention execution.
 *
 * Generic staging ownership; family layout remains caller-defined.
 */
static int attention_stage_acquire(
    yvex_backend *backend, size_t bytes, int require_pinned, int injected,
    unsigned char **out, int *reused, yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    yvex_backend_host_workspace_summary summary;
    int acquired;
    if (!backend || !bytes || !out || !reused || injected)
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
            "cuda.attention.host_stage", bytes, 0ull, err,
            injected ? YVEX_ERR_NOMEM : YVEX_ERR_INVALID_ARG,
            "CUDA attention host-staging acquisition failed");
    if (!yvex_backend_host_workspace_summary_get(backend, &summary))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
            "cuda.attention.host_stage", bytes, 0ull, err, YVEX_ERR_STATE,
            "CUDA attention host workspace state is unavailable");
    *reused = summary.peak != 0ull;
    acquired = yvex_backend_host_workspace_acquire(
        backend, bytes, 8ull, (void **)out);
    if (acquired != YVEX_BACKEND_RESIDENT_HIT || !*out)
        return attention_fail(
            failure, acquired == YVEX_BACKEND_RESIDENT_MISS
                         ? YVEX_BACKEND_ATTENTION_FAILURE_BUDGET
                         : YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
            "cuda.attention.host_stage", bytes, summary.capacity, err,
            acquired == YVEX_BACKEND_RESIDENT_INVALID ? YVEX_ERR_BOUNDS
                                                      : YVEX_ERR_NOMEM,
            "CUDA attention host workspace capacity is insufficient");
    if (require_pinned &&
        (!yvex_backend_host_workspace_summary_get(backend, &summary) ||
         !summary.pinned))
        return attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.attention.host_stage.pinned", 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "CUDA graph attention requires page-locked stable host staging");
    return YVEX_OK;
}

static int attention_stage_range(unsigned char *base, size_t *cursor,
                                 unsigned long long count, size_t width, void **out)
{
    size_t aligned, bytes;
    if (!cursor || !out) return 0;
    *out = NULL;
    if (!count) return 1;
    if (*cursor > SIZE_MAX - 7u ||
        !yvex_cuda_work_checked_bytes(count, (unsigned long long)width, &bytes))
        return 0;
    aligned = (*cursor + 7u) & ~(size_t)7u;
    if (aligned > SIZE_MAX - bytes) return 0;
    if (base) *out = base + aligned;
    *cursor = aligned + bytes;
    return 1;
}

static int attention_stage_layout(
    unsigned char *base, yvex_cuda_attention_upload *uploads, size_t upload_count,
    yvex_cuda_attention_transfer *transfers, size_t transfer_count,
    unsigned long long csa_tokens, int **status, unsigned long long **selected,
    unsigned long long **candidates, size_t *total, size_t *download_total)
{
    size_t cursor = 0u, i;
    if (!uploads || !transfers || !status || !selected || !candidates || !total ||
        !download_total)
        return 0;
    if (!attention_stage_range(base, &cursor, 1ull, sizeof(int), (void **)status) ||
        !attention_stage_range(base, &cursor, csa_tokens, sizeof(**selected),
                               (void **)selected) ||
        !attention_stage_range(base, &cursor, csa_tokens, sizeof(**candidates),
                               (void **)candidates))
        return 0;
    for (i = 0u; i < transfer_count; ++i)
        if (!attention_stage_range(base, &cursor, transfers[i].capacity,
                                   transfers[i].width, &transfers[i].staged))
            return 0;
    *download_total = cursor;
    for (i = 0u; i < upload_count; ++i)
        if (!attention_stage_range(base, &cursor, uploads[i].count,
                                   uploads[i].width, &uploads[i].staged))
            return 0;
    *total = cursor;
    return 1;
}

typedef struct {
    const void *host;
    CUdeviceptr source, target;
    unsigned long long count;
    size_t width;
} cuda_state_span;

static int cuda_state_source_offset(CUdeviceptr base, unsigned long long count,
                                    size_t width, CUdeviceptr *out)
{
    unsigned long long bytes;
    if (!out || !yvex_core_u64_mul(count, (unsigned long long)width, &bytes) ||
        base > ULLONG_MAX - bytes)
        return 0;
    *out = base + bytes;
    return 1;
}

/* Publish complete non-prefix state into the already admitted candidate bank. */
static int attention_state_stage(
    yvex_backend *backend, const yvex_backend_attention_job *job,
    const yvex_cuda_attention_state_sources *sources,
    size_t *copied_bytes, int *staged, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    cuda_state_span spans[10];
    CUstream stream;
    unsigned long long total_local, local_count, local_offset;
    unsigned long long compressed_count, indexer_count, rolling_checkpoint;
    CUdeviceptr local, local_positions, main_kv, main_score, index_kv, index_score;
    size_t bytes = 0u, index;
    unsigned int hits = 0u, misses = 0u;
    int rc;
    if (copied_bytes) *copied_bytes = 0u;
    if (staged) *staged = 0;
    if (!backend || !job || !sources || !copied_bytes || !staged || !state) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.attention.state.stage",
                       "CUDA attention state publication arguments are incomplete");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_core_u64_add(sources->initial_local, job->token_count, &total_local) ||
        !yvex_core_u64_add(sources->initial_compressed,
                           sources->emitted_compressed, &compressed_count) ||
        !yvex_core_u64_add(sources->initial_indexer,
                           sources->emitted_indexer, &indexer_count) ||
        job->local_stride > SIZE_MAX / sizeof(float) ||
        job->compressed_stride > SIZE_MAX / sizeof(float) ||
        job->indexer_stride > SIZE_MAX / sizeof(float)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.state.stage",
                       "CUDA attention state publication extent overflowed");
        return YVEX_ERR_BOUNDS;
    }
    local_count = total_local < sources->local_capacity
                      ? total_local : sources->local_capacity;
    local_offset = total_local - local_count;
    rolling_checkpoint = job->retain_prefix_checkpoints ? job->token_count : 0ull;
    if (!cuda_state_source_offset(
            sources->local, local_offset,
            sizeof(float) * (size_t)job->local_stride, &local) ||
        !cuda_state_source_offset(
            sources->local_positions, local_offset,
            sizeof(unsigned long long), &local_positions) ||
        !cuda_state_source_offset(
            sources->main_kv, rolling_checkpoint,
            sizeof(float) * (size_t)sources->main_extent, &main_kv) ||
        !cuda_state_source_offset(
            sources->main_score, rolling_checkpoint,
            sizeof(float) * (size_t)sources->main_extent, &main_score) ||
        !cuda_state_source_offset(
            sources->index_kv, rolling_checkpoint,
            sizeof(float) * (size_t)sources->index_extent, &index_kv) ||
        !cuda_state_source_offset(
            sources->index_score, rolling_checkpoint,
            sizeof(float) * (size_t)sources->index_extent, &index_score)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.state.stage",
                       "CUDA attention state publication extent overflowed");
        return YVEX_ERR_BOUNDS;
    }
    spans[0] = (cuda_state_span){job->local_kv, local, 0ull, local_count,
                                 sizeof(float) * (size_t)job->local_stride};
    spans[1] = (cuda_state_span){job->local_positions, local_positions, 0ull,
                                 local_count, sizeof(unsigned long long)};
    spans[2] = (cuda_state_span){job->compressed_kv, sources->compressed, 0ull,
                                 compressed_count,
                                 sizeof(float) * (size_t)job->compressed_stride};
    spans[3] = (cuda_state_span){job->compressed_positions,
                                 sources->compressed_positions, 0ull,
                                 compressed_count, sizeof(unsigned long long)};
    spans[4] = (cuda_state_span){job->indexer_kv, sources->indexer, 0ull,
                                 indexer_count,
                                 sizeof(float) * (size_t)job->indexer_stride};
    spans[5] = (cuda_state_span){job->indexer_positions, sources->indexer_positions, 0ull,
                                 indexer_count, sizeof(unsigned long long)};
    spans[6] = (cuda_state_span){job->main_rolling.kv_state, main_kv, 0ull,
                                 job->main_rolling.kv_state_capacity, sizeof(float)};
    spans[7] = (cuda_state_span){job->main_rolling.score_state, main_score, 0ull,
                                 job->main_rolling.score_state_capacity, sizeof(float)};
    spans[8] = (cuda_state_span){job->indexer_rolling.kv_state, index_kv, 0ull,
                                 job->indexer_rolling.kv_state_capacity, sizeof(float)};
    spans[9] = (cuda_state_span){job->indexer_rolling.score_state, index_score, 0ull,
                                 job->indexer_rolling.score_state_capacity, sizeof(float)};
    for (index = 0u; index < sizeof(spans) / sizeof(spans[0]); ++index) {
        size_t span_bytes;
        unsigned long long target = 0ull;
        int resolved;
        if (!spans[index].count) continue;
        if (!spans[index].host || !spans[index].source ||
            !yvex_cuda_work_checked_bytes(
                spans[index].count, spans[index].width, &span_bytes)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.state.stage",
                           "CUDA attention state span is invalid");
            return YVEX_ERR_BOUNDS;
        }
        resolved = yvex_backend_state_residency_resolve(
            backend, spans[index].host, span_bytes, &target);
        if (resolved == YVEX_BACKEND_RESIDENT_INVALID) {
            yvex_error_set(err, YVEX_ERR_STATE, "cuda.attention.state.stage",
                           "CUDA attention state residency is invalidated");
            return YVEX_ERR_STATE;
        }
        if (resolved == YVEX_BACKEND_RESIDENT_HIT) ++hits;
        else ++misses;
        spans[index].target = (CUdeviceptr)target;
        if (span_bytes > SIZE_MAX - bytes) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.state.stage",
                           "CUDA attention state byte accounting overflowed");
            return YVEX_ERR_BOUNDS;
        }
        bytes += span_bytes;
    }
    if (!hits) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (misses) {
        yvex_error_set(err, YVEX_ERR_STATE, "cuda.attention.state.stage",
                       "CUDA attention state residency resolved only a partial publication");
        return YVEX_ERR_STATE;
    }
    rc = yvex_cuda_set_current(backend, "cuda.attention.state.stage", err);
    if (rc != YVEX_OK) return rc;
    stream = yvex_cuda_launch_stream(backend);
    for (index = 0u; index < sizeof(spans) / sizeof(spans[0]); ++index) {
        size_t span_bytes = 0u;
        CUresult copied;
        if (!spans[index].count) continue;
        if (!yvex_cuda_work_checked_bytes(
                spans[index].count, spans[index].width, &span_bytes)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.attention.state.stage",
                           "CUDA attention state span drifted after admission");
            return YVEX_ERR_BOUNDS;
        }
        copied = stream && state->driver.cuMemcpyDtoDAsync_v2
                     ? state->driver.cuMemcpyDtoDAsync_v2(
                           spans[index].target, spans[index].source,
                           span_bytes, stream)
                     : !stream ? state->driver.cuMemcpyDtoD_v2(
                           spans[index].target, spans[index].source, span_bytes)
                               : (CUresult)1;
        rc = yvex_cuda_status(&state->driver, copied,
                              "cuda.attention.state.stage", err);
        if (rc != YVEX_OK) return rc;
    }
    *copied_bytes = bytes;
    *staged = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}
/* All normalized emissions through this tile already exist on the stream.
 * Per-query visibility still filters their exact positions, so batching is
 * submission/layout only: no future row enters a causal selected population. */
static int attention_selection_phase(yvex_cuda_work *work,
    const yvex_cuda_attention_selection_phase *p, const char *stage,
    yvex_backend_attention_failure *failure, yvex_error *err)
{
    unsigned long long extent, grid, query_elements, zero = 0ull;
    CUdeviceptr absent = 0ull;
    if (!work || !work->state || !p || !p->rows ||
        p->rows > YVEX_CUDA_ATTENTION_SELECTION_ROWS ||
        !p->query || !p->weights || !p->selected || !p->selected_positions ||
        !p->selected_count || !p->valid_count || !p->scores || !p->indexes || !p->status ||
        (p->candidates && (!p->indexer || !p->positions)) ||
        !p->heads || !p->dimension || !p->ratio || !p->k ||
        p->indexer_stride < p->dimension ||
        !yvex_core_u64_mul(p->heads, p->dimension, &query_elements) ||
        !yvex_core_u64_mul(query_elements, p->rows, &query_elements) ||
        p->first_position > ULLONG_MAX - (p->rows - 1ull) ||
        !yvex_core_power_of_two_capacity(p->candidates, 1ull, 1ull, 1ull, &extent) ||
        extent > p->candidate_capacity || extent > p->score_grid ||
        p->score_grid > p->candidate_capacity ||
        !yvex_core_u64_mul(p->rows, p->score_grid, &grid) || grid > UINT_MAX ||
        !p->selected_stride ||
        (p->selected_stride < p->k && p->selected_stride < p->candidates))
        return attention_fail(failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            stage, UINT_MAX, p ? p->candidates : 0ull, err, YVEX_ERR_BOUNDS,
            "bounded attention selection row geometry is invalid");
    void *scores[] = {(void *)&p->query, (void *)&p->weights, (void *)&p->indexer,
        (void *)&p->positions, (void *)&p->candidates, (void *)&p->indexer_stride,
        &absent, &absent, &zero, (void *)&p->dimension, (void *)&p->heads,
        (void *)&p->dimension, (void *)&p->ratio, (void *)&p->first_position,
        (void *)&p->scores, (void *)&p->status, (void *)&p->rows, (void *)&p->score_grid};
    int rc = attention_launch(work, work->state->attention_candidate_scores_function,
        (unsigned int)grid, CUDA_ATTENTION_BLOCK, CUDA_ATTENTION_BLOCK * sizeof(double),
        scores, stage, failure, err);
    if (rc != YVEX_OK) return rc;
    void *rank[] = {(void *)&p->positions, (void *)&p->candidates, &absent, &zero,
        (void *)&p->ratio, (void *)&p->first_position, (void *)&p->k,
        (void *)&p->selected, (void *)&p->selected_positions, (void *)&p->selected_count,
        (void *)&p->valid_count, (void *)&p->scores, (void *)&p->indexes, &extent,
        (void *)&p->status, (void *)&p->rows, (void *)&p->score_grid, (void *)&p->selected_stride};
    return attention_launch(work, work->state->attention_topk_function,
        (unsigned int)p->rows, CUDA_ATTENTION_BLOCK, 0u, rank, stage, failure, err);
}

/*
 * Expose the single private encoded-attention operation boundary.
 *
 * None; returns immutable process-lifetime methods.
 */
const yvex_cuda_attention_operations *yvex_cuda_attention_operations_get(void)
{
    static const yvex_cuda_attention_operations operations = {
        attention_fail, attention_account_transfer, attention_validate_job,
        attention_validate_weight,
        attention_validate_activation, attention_validate_rolling,
        attention_validate_alias, attention_cancel, attention_stage_acquire,
        attention_stage_layout,
        attention_allocate, attention_initialize, attention_download,
        attention_launch, attention_round_bf16, attention_matvec,
        attention_matvec_grouped, attention_decode, attention_rolling_phase, attention_selection_phase,
        attention_weighted_norm, attention_unit_norm, attention_rope,
        attention_activation, attention_state_stage
    };
    return &operations;
}
