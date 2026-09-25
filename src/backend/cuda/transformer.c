#include "src/backend/cuda/private.h"
#include "src/backend/cuda/transformer_ops.h"
#include <yvex/internal/component.h>
#include <yvex/quant.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum {
    TRANSFORMER_BLOCK = 128u,
    MHC_PRE_BLOCK = 256u,
    WEIGHTED_RMS_BLOCK = 256u,
    GQA_HEAD_DIMENSION_MAX = 256u,
    GQA_QUERIES_PER_BLOCK = 4u,
    GQA_BLAS_BLOCK = 256u,
    GQA_WORK_ALIGNMENT = 256u,
    /* One in-place F32 score tile keeps the canonical 768p workspace below the superseded
       pack-and-two-score-buffer path without fragmenting execution into 64-row submissions. */
    GQA_BLAS_QUERY_CHUNK = 512u
};
#define GQA_BLAS_OP_N 0
#define GQA_BLAS_OP_T 1
#define GQA_BLAS_R_32F 0
#define GQA_BLAS_R_16BF 14
#define GQA_BLAS_COMPUTE_32F 68
#define GQA_BLAS_DEFAULT -1

static int status_storage_ensure(yvex_backend *backend, yvex_cuda_backend_state *state,
                                 const char *stage, yvex_error *err)
{
    int rc;
    if (state->transformer_status) return YVEX_OK;
    rc = yvex_backend_memory_can_add(backend, sizeof(int), "CUDA", stage, err);
    if (rc == YVEX_OK)
        rc = yvex_cuda_status(&state->driver,
                              state->driver.cuMemAlloc_v2(
                                  &state->transformer_status, sizeof(int)),
                              stage, err);
    if (rc == YVEX_OK) backend_memory_acquire(backend, sizeof(int));
    return rc;
}

static int status_transaction_open(yvex_backend *backend, yvex_cuda_work *work,
                                      int begin, const char *stage, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;
    if (!backend || !state || !work || (begin != 0 && begin != 1)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA status transaction owner is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    *work = (yvex_cuda_work){.backend = backend, .state = state,
                             .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    if (!begin && state->status_transaction_active) {
        work->status = state->transformer_status;
        work->status_deferred = 1;
        return YVEX_OK;
    }
    if (begin && backend->workspace_device_tensor) {
        rc = status_storage_ensure(backend, state, stage, err);
        if (rc != YVEX_OK) return rc;
        work->status = state->transformer_status;
        rc = yvex_cuda_work_initialize(work, work->status, sizeof(int), NULL, 1, stage, err);
        state->status_transaction_active = rc == YVEX_OK;
        work->status_deferred = rc == YVEX_OK;
        return rc;
    }
    /* A standalone operation does not own the enclosing executor's arena.
     * Its status must survive independently until this operation completes;
     * neither consume nor rewind another producer's workspace cursor. */
    work->raw_only = 1;
    return yvex_cuda_work_allocate(
        work, &work->status, sizeof(int), NULL, 1, stage, NULL, err);
}
static int status_transaction_close(yvex_cuda_work *work, int wait, int complete, int rc,
                                       yvex_backend_operation_facts *facts,
                                       const char *stage, yvex_error *err)
{
    yvex_cuda_backend_state *state;
    yvex_error cleanup;
    int host_status = 0, cleanup_rc, waited = 0, device_wide = 0;
    if (!work || !work->backend || !facts ||
        (wait != 0 && wait != 1) || (complete != 0 && complete != 1)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA status transaction completion is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    state = work->state;
    if (rc == YVEX_OK && wait) {
        rc = yvex_cuda_launch_synchronize(
            work->backend, work->variant, &device_wide, stage, err);
        waited = rc == YVEX_OK;
    }
    if (rc == YVEX_OK && wait)
        rc = yvex_cuda_status(&state->driver,
                              state->driver.cuMemcpyDtoH_v2(
                                  &host_status, work->status, sizeof(host_status)),
                              stage, err);
    if ((complete || rc != YVEX_OK) && work->status_deferred)
        state->status_transaction_active = 0;
    if (rc == YVEX_OK && host_status) {
        yvex_error_set(err, YVEX_ERR_FORMAT, stage,
                       "CUDA status transaction reported invalid numerics");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_cuda_work_cleanup(work, &cleanup);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) {
        rc = cleanup_rc;
        if (err) *err = cleanup;
    }
    facts->temporary_bytes = sizeof(host_status);
    if (waited) {
        facts->d2h_bytes += sizeof(host_status);
        facts->download_count++;
        if (device_wide) facts->device_synchronizations++;
        else facts->queue_synchronizations++;
    }
    return rc;
}
static int cuda_transformer_refuse(yvex_error *err, yvex_status status,
                                   const char *where, const char *reason)
{
    yvex_error_set(err, status, where, reason);
    return status;
}
/*
 * Decode selected encoded embedding rows and initialize repeated mHC streams on CUDA.
 *
 * Writes device embedding and expanded tensors; allocates only transaction scratch. Ownership,
 * launch, copy, status, sync, or cleanup refusal leaves output unadmitted. Transformer embedding
 * initialization only; no tokenizer or host numerical fallback.
 */
int yvex_cuda_transformer_initial(
    yvex_backend *backend, const yvex_device_tensor *encoded, unsigned int qtype,
    unsigned long long token_count, unsigned long long hidden_width,
    unsigned long long residual_streams, yvex_device_tensor *embedding,
    yvex_device_tensor *expanded, yvex_backend_operation_facts *facts,
    yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(qtype);
    yvex_cuda_work work;
    CUstream stream = yvex_cuda_launch_stream(backend);
    CUdeviceptr encoded_ptr, embedding_ptr, expanded_ptr;
    unsigned long long count, expanded_count, encoded_required, token, residual_stream;
    size_t embedding_bytes, expanded_bytes, row_bytes;
    unsigned long long activation_bytes;
    unsigned int grid;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !geometry || !geometry->block_size || !geometry->bytes_per_block || !facts ||
        !encoded || !embedding || !expanded || !token_count || !hidden_width ||
        !residual_streams || !yvex_core_u64_mul(token_count, hidden_width, &count) ||
        !yvex_core_u64_mul(count, residual_streams, &expanded_count) ||
        !yvex_core_u64_mul(count / geometry->block_size,
                           geometry->bytes_per_block, &encoded_required) ||
        !yvex_cuda_work_checked_bytes(count, sizeof(float), &embedding_bytes) ||
        !yvex_cuda_work_checked_bytes(expanded_count, sizeof(float), &expanded_bytes) ||
        !yvex_cuda_work_checked_bytes(hidden_width, sizeof(float), &row_bytes) ||
        !yvex_core_u64_add((unsigned long long)embedding_bytes,
                           (unsigned long long)expanded_bytes, &activation_bytes) ||
        !backend_tensor_owner_is(backend, encoded) ||
        count % geometry->block_size ||
        encoded->bytes < encoded_required ||
        !backend_tensor_owner_is(backend, embedding) || embedding->dtype != YVEX_DTYPE_F32 ||
        embedding->bytes < embedding_bytes ||
        !backend_tensor_owner_is(backend, expanded) || expanded->dtype != YVEX_DTYPE_F32 ||
        expanded->bytes < expanded_bytes ||
        expanded_count > UINT_MAX * (unsigned long long)TRANSFORMER_BLOCK ||
        !state->attention_bf16_round_function)
        return cuda_transformer_refuse(err, YVEX_ERR_FORMAT, "cuda.transformer.initial",
                                       "CUDA transformer embedding geometry is incompatible");
    rc = status_transaction_open(
        backend, &work, 1, "cuda.transformer.initial.status", err);
    encoded_ptr = (CUdeviceptr)encoded->data;
    embedding_ptr = (CUdeviceptr)embedding->data;
    expanded_ptr = (CUdeviceptr)expanded->data;
    grid = (unsigned int)((count + TRANSFORMER_BLOCK - 1ull) / TRANSFORMER_BLOCK);
    if (rc == YVEX_OK) {
        void *params[] = {&encoded_ptr, &count, &qtype, &embedding_ptr, &work.status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              state->encoded_row_decode_function, grid, TRANSFORMER_BLOCK,
                              0u, params, "cuda.transformer.embedding", err);
    }
    for (token = 0ull; rc == YVEX_OK && token < token_count; ++token)
        for (residual_stream = 0ull;
             rc == YVEX_OK && residual_stream < residual_streams; ++residual_stream) {
            CUdeviceptr source = embedding_ptr + token * hidden_width * sizeof(float);
            CUdeviceptr target = expanded_ptr +
                (token * residual_streams + residual_stream) * hidden_width * sizeof(float);
            CUresult copied = stream && state->driver.cuMemcpyDtoDAsync_v2
                                  ? state->driver.cuMemcpyDtoDAsync_v2(
                                        target, source, row_bytes, stream)
                                  : !stream ? state->driver.cuMemcpyDtoD_v2(
                                        target, source, row_bytes) : (CUresult)1;
            rc = yvex_cuda_status(&state->driver, copied,
                                  "cuda.transformer.initial.repeat", err);
        }
    /* The embedding row remains decoded F32. The repeated residual is the
     * admitted BF16 publication, matching the portable initial program. */
    if (rc == YVEX_OK) {
        grid = (unsigned int)((expanded_count + TRANSFORMER_BLOCK - 1ull) /
                              TRANSFORMER_BLOCK);
        void *params[] = {&expanded_ptr, &expanded_count, &work.status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              state->attention_bf16_round_function, grid,
                              TRANSFORMER_BLOCK, 0u, params,
                              "cuda.transformer.initial.round", err);
    }
    rc = status_transaction_close(
        &work, !work.status_deferred, 0, rc, facts,
        "cuda.transformer.initial.status", err);
    if (rc == YVEX_OK) {
        embedding->is_written = 1;
        expanded->is_written = 1;
        facts->d2d_bytes = expanded_bytes;
        facts->kernel_launches = 2ull;
        facts->active_weight_bytes = encoded_required;
        facts->activation_bytes = activation_bytes;
        facts->compulsory_memory_facts_available = 1;
        yvex_error_clear(err);
    }
    return rc;
}

/*
 * Collapse target-feature residual streams on CUDA and publish compact and optional resident rows.
 *
 * The caller supplies reusable device storage and may request compact host evidence. Bounded
 * status and resident rows become visible together; expanded input never leaves the device.
 */
int yvex_cuda_transformer_feature_mean(
    yvex_backend *backend, const yvex_device_tensor *expanded,
    unsigned long long token_count, unsigned long long hidden_width,
    unsigned long long residual_streams, yvex_device_tensor *device_output,
    yvex_device_tensor *resident_output, unsigned long long resident_row_offset,
    unsigned long long resident_row_stride, unsigned long long resident_column_offset,
    float *host_output, yvex_backend_operation_facts *facts,
    yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    CUdeviceptr input_ptr, output_ptr, resident_ptr = 0ull;
    unsigned long long input_count, output_count, activation_count, resident_rows, resident_elements;
    size_t output_bytes, activation_bytes;
    unsigned int grid;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !state->transformer_feature_mean_function || !facts ||
        !token_count || !hidden_width || !residual_streams ||
        !yvex_core_u64_mul(token_count, hidden_width, &output_count) ||
        !yvex_core_u64_mul(output_count, residual_streams, &input_count) ||
        !yvex_core_u64_add(input_count, output_count, &activation_count) ||
        (resident_output &&
         (!yvex_core_u64_add(activation_count, output_count, &activation_count) ||
          !resident_row_stride || resident_column_offset > resident_row_stride ||
          hidden_width > resident_row_stride - resident_column_offset ||
          !yvex_core_u64_add(resident_row_offset, token_count, &resident_rows) ||
          !yvex_core_u64_mul(resident_rows, resident_row_stride, &resident_elements) ||
          resident_elements > ULLONG_MAX / sizeof(float) ||
          !backend_tensor_owner_is(backend, resident_output) ||
          resident_output->dtype != YVEX_DTYPE_F32 ||
          resident_output->data == expanded->data || resident_output->data == device_output->data ||
          resident_output->bytes < resident_elements * sizeof(float))) ||
        (!resident_output &&
         (resident_row_offset || resident_row_stride || resident_column_offset)) ||
        !yvex_cuda_work_checked_bytes(output_count, sizeof(float), &output_bytes) ||
        !yvex_cuda_work_checked_bytes(activation_count, sizeof(float), &activation_bytes) ||
        output_count > UINT_MAX * (unsigned long long)TRANSFORMER_BLOCK ||
        !backend_tensor_f32_elements(expanded, input_count) ||
        !backend_tensor_f32_elements(device_output, output_count))
        return cuda_transformer_refuse(
            err, YVEX_ERR_FORMAT, "cuda.transformer.feature-mean",
            "CUDA transformer feature geometry is incompatible");
    device_output->is_written = 0;
    if (resident_output) resident_output->is_written = 0;
    rc = status_transaction_open(
        backend, &work, 0, "cuda.transformer.feature-mean.status", err);
    input_ptr = (CUdeviceptr)expanded->data;
    output_ptr = (CUdeviceptr)device_output->data;
    if (resident_output) resident_ptr = (CUdeviceptr)resident_output->data;
    grid = (unsigned int)((output_count + TRANSFORMER_BLOCK - 1ull) /
                          TRANSFORMER_BLOCK);
    if (rc == YVEX_OK) {
        void *params[] = {
            &input_ptr, &token_count, &residual_streams, &hidden_width,
            &output_ptr, &resident_ptr, &resident_row_offset,
            &resident_row_stride, &resident_column_offset, &work.status};
        rc = yvex_cuda_launch(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->transformer_feature_mean_function, grid, TRANSFORMER_BLOCK,
            0u, params, "cuda.transformer.feature-mean", err);
    }
    rc = status_transaction_close(
        &work, !work.status_deferred || host_output, 0, rc, facts,
        "cuda.transformer.feature-mean.status", err);
    if (rc == YVEX_OK && host_output)
        rc = yvex_cuda_status(
            &state->driver,
            state->driver.cuMemcpyDtoH_v2(host_output, output_ptr, output_bytes),
            "cuda.transformer.feature-mean.output", err);
    if (rc == YVEX_OK) {
        device_output->is_written = 1;
        if (resident_output) resident_output->is_written = 1;
        facts->d2h_bytes += host_output ? output_bytes : 0u;
        facts->kernel_launches = 1ull;
        facts->download_count += host_output != NULL;
        facts->activation_bytes = activation_bytes;
        facts->compulsory_memory_facts_available = 1;
        yvex_error_clear(err);
    }
    return rc;
}

int yvex_cuda_residual_pre(yvex_backend *backend, const yvex_mhc_device_request *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    int rc = yvex_mhc_pre_admit(backend, r, facts, err);
    if (rc != YVEX_OK) return rc;
    unsigned long long streams = r->geometry.streams, width = r->geometry.width;
    unsigned long long mixing = (streams + 2u) * streams, rows = r->rows;
    if (!state || rows > UINT_MAX || streams > MHC_PRE_BLOCK)
        return cuda_transformer_refuse(err, YVEX_ERR_UNSUPPORTED, "cuda.mhc-pre",
            "mHC geometry has no admitted launch implementation");
    /* The retained numerical kernel rounds its input in place. Operate on
     * prepared scratch so the pure IR operation never mutates an SSA operand. */
    rc = yvex_backend_tensor_copy(backend, r->workspace, r->inputs[0], err);
    if (rc != YVEX_OK) return rc;
    rc = status_transaction_open(backend, &work, 0, "cuda.mhc-pre.status", err);
    CUdeviceptr residual = (CUdeviceptr)r->workspace->data, mix = (CUdeviceptr)r->inputs[1]->data;
    CUdeviceptr scale = (CUdeviceptr)r->inputs[2]->data, base = (CUdeviceptr)r->inputs[3]->data;
    CUdeviceptr collapsed = (CUdeviceptr)r->outputs[0]->data, post = (CUdeviceptr)r->outputs[1]->data;
    CUdeviceptr matrix = (CUdeviceptr)r->outputs[2]->data;
    double epsilon = r->geometry.rms_epsilon, mhc = r->geometry.epsilon, multiplier = r->geometry.post_multiplier;
    unsigned long long iterations = r->geometry.sinkhorn_iterations;
    if (rc == YVEX_OK) {
        void *args[] = {&residual, &mix, &scale, &base, &streams, &width, &mixing, &iterations,
            &epsilon, &mhc, &multiplier, &collapsed, &post, &matrix, &rows, &work.status};
        unsigned int shared_bytes = (unsigned int)((streams + 1u + MHC_PRE_BLOCK) * sizeof(double));
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->residual_mhc_pre_function, (unsigned int)rows, MHC_PRE_BLOCK,
            shared_bytes, args, "cuda.mhc-pre", err);
    }
    rc = status_transaction_close(&work, 1, 0, rc, facts, "cuda.mhc-pre.status", err);
    if (rc == YVEX_OK) {
        for (size_t i = 0u; i < 3u; ++i) r->outputs[i]->is_written = 1;
        facts->kernel_launches = 1u;
        facts->d2d_bytes += r->inputs[0]->bytes;
    }
    return rc;
}

int yvex_cuda_transformer_final(
    yvex_backend *backend, const yvex_device_tensor *expanded,
    const yvex_device_tensor *function, const yvex_device_tensor *base,
    const yvex_device_tensor *scale, const yvex_device_tensor *norm,
    unsigned long long token_count, unsigned long long hidden_width,
    unsigned long long residual_streams, double epsilon, double mhc_epsilon,
    yvex_device_tensor *pre_normalized, yvex_device_tensor *output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    CUdeviceptr input_ptr, function_ptr, base_ptr, scale_ptr, norm_ptr;
    CUdeviceptr pre_output_ptr = 0ull, output_ptr;
    unsigned long long expanded_width, expanded_count, function_count, output_count;
    unsigned long long weight_count, activation_count;
    size_t weight_bytes, activation_bytes;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !facts || !token_count || !hidden_width || !residual_streams ||
        !yvex_core_u64_mul(hidden_width, residual_streams, &expanded_width) ||
        !yvex_core_u64_mul(token_count, expanded_width, &expanded_count) ||
        !yvex_core_u64_mul(residual_streams, expanded_width, &function_count) ||
        !yvex_core_u64_mul(token_count, hidden_width, &output_count) ||
        !yvex_core_u64_add(function_count, residual_streams, &weight_count) ||
        !yvex_core_u64_add(weight_count, 1ull, &weight_count) ||
        !yvex_core_u64_add(weight_count, hidden_width, &weight_count) ||
        !yvex_core_u64_add(expanded_count, output_count, &activation_count) ||
        (pre_normalized &&
         !yvex_core_u64_add(activation_count, output_count, &activation_count)) ||
        !yvex_cuda_work_checked_bytes(weight_count, sizeof(float), &weight_bytes) ||
        !yvex_cuda_work_checked_bytes(activation_count, sizeof(float), &activation_bytes) ||
        token_count > UINT_MAX || !backend_tensor_f32_elements(expanded, expanded_count) ||
        !backend_tensor_f32_elements(function, function_count) ||
        !backend_tensor_f32_elements(base, residual_streams) ||
        !backend_tensor_f32_elements(scale, 1ull) ||
        !backend_tensor_f32_elements(norm, hidden_width) ||
        !backend_tensor_f32_elements(output, output_count) ||
        (pre_normalized &&
         (!backend_tensor_f32_elements(pre_normalized, output_count) ||
          pre_normalized->data == output->data)))
        return cuda_transformer_refuse(err, YVEX_ERR_FORMAT, "cuda.transformer.final",
                                       "CUDA transformer final geometry is incompatible");
    rc = status_transaction_open(
        backend, &work, 0, "cuda.transformer.final.status", err);
    input_ptr = (CUdeviceptr)expanded->data; function_ptr = (CUdeviceptr)function->data;
    base_ptr = (CUdeviceptr)base->data; scale_ptr = (CUdeviceptr)scale->data;
    norm_ptr = (CUdeviceptr)norm->data;
    if (pre_normalized) pre_output_ptr = (CUdeviceptr)pre_normalized->data;
    output_ptr = (CUdeviceptr)output->data;
    if (rc == YVEX_OK) {
        void *params[] = {&input_ptr, &function_ptr, &base_ptr, &scale_ptr, &norm_ptr,
                          &token_count, &residual_streams, &hidden_width, &epsilon,
                          &mhc_epsilon, &pre_output_ptr, &output_ptr, &work.status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->transformer_final_function, (unsigned int)token_count,
            TRANSFORMER_BLOCK, TRANSFORMER_BLOCK * (unsigned int)sizeof(double),
            params, "cuda.transformer.final", err);
    }
    rc = status_transaction_close(
        &work, 1, 1, rc, facts, "cuda.transformer.final.status", err);
    if (rc == YVEX_OK) {
        if (pre_normalized) pre_normalized->is_written = 1;
        output->is_written = 1;
        facts->kernel_launches = 1ull;
        facts->active_weight_bytes = weight_bytes;
        facts->activation_bytes = activation_bytes;
        facts->compulsory_memory_facts_available = 1;
        yvex_error_clear(err);
    }
    return rc;
}


static int transformer_tensor(const yvex_backend *backend, const yvex_device_tensor *tensor,
                              unsigned long long elements, int require_written)
{
    return backend_tensor_owner_is(backend, tensor) &&
           (!require_written || tensor->is_written) && tensor->dtype == YVEX_DTYPE_F32 &&
           backend_tensor_f32_elements(tensor, elements);
}

int yvex_cuda_weighted_rms_bf16(yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *weight, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, double epsilon,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    unsigned long long count;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !facts || !rows || rows > UINT_MAX || !width ||
        !isfinite(epsilon) || epsilon <= 0.0 || !yvex_core_u64_mul(rows, width, &count) ||
        !transformer_tensor(backend, input, count, 1) ||
        !transformer_tensor(backend, weight, width, 1) ||
        !transformer_tensor(backend, output, count, 0))
        return cuda_transformer_refuse(err, YVEX_ERR_FORMAT, "cuda.weighted-rms",
            "exact written F32 storage and positive F64 epsilon required");
    const yvex_device_tensor *sources[] = {input, weight};
    for (size_t i = 0u; i < 2u; ++i) {
        uintptr_t a = (uintptr_t)output->data, b = (uintptr_t)sources[i]->data;
        if (a <= b ? b - a < output->bytes : a - b < sources[i]->bytes)
            return cuda_transformer_refuse(err, YVEX_ERR_FORMAT, "cuda.weighted-rms",
                "pure normalization cannot alias an operand");
    }
    output->is_written = 0;
    int rc = yvex_backend_tensor_copy(backend, output, input, err);
    output->is_written = 0;
    if (rc != YVEX_OK) return rc;
    rc = status_transaction_open(backend, &work, 0, "cuda.weighted-rms.status", err);
    if (rc != YVEX_OK) return rc;
    CUdeviceptr values = (CUdeviceptr)output->data, weights = (CUdeviceptr)weight->data;
    unsigned int qtype = YVEX_GGUF_QTYPE_F32;
    void *args[] = {&values, &width, &weights, &qtype, &epsilon, &rows, &work.status};
    /* Same admitted 256-lane F64 square reduction, F64 inverse/scale and
     * BF16 publication as encoded weighted norm. */
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        state->attention_weighted_norm_function, (unsigned int)rows, WEIGHTED_RMS_BLOCK,
        WEIGHTED_RMS_BLOCK * sizeof(double), args, "cuda.weighted-rms", err);
    rc = status_transaction_close(&work, 1, 0, rc, facts, "cuda.weighted-rms.status", err);
    if (rc == YVEX_OK) {
        output->is_written = 1;
        facts->kernel_launches = 1u;
        facts->d2d_bytes += input->bytes;
        facts->active_weight_bytes = weight->bytes;
        facts->activation_bytes = input->bytes + output->bytes;
        facts->compulsory_memory_facts_available = 1;
    }
    return rc;
}

static int transformer_launch(yvex_backend *backend, CUfunction function,
                              unsigned int grid, unsigned int shared_bytes,
                              void **parameters, const char *stage,
                              yvex_backend_operation_facts *facts, yvex_error *err)
{
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!backend || !function || !grid || !parameters || !facts) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "complete CUDA transformer launch facts are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                          function, grid, TRANSFORMER_BLOCK, shared_bytes,
                          parameters, stage, err);
    if (rc == YVEX_OK)
        rc = yvex_cuda_synchronize(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, stage, err);
    if (rc == YVEX_OK) {
        facts->kernel_launches = 1ull;
        facts->device_synchronizations = 1ull;
        facts->compulsory_memory_facts_available = 1;
    }
    return rc;
}

static int transformer_rotary_half(
    yvex_backend *backend, yvex_device_tensor *values,
    const yvex_device_tensor *cosines, const yvex_device_tensor *sines,
    unsigned long long tokens, unsigned long long heads, unsigned long long head_dim,
    unsigned long long rotary_dim, CUfunction function, const char *stage,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    unsigned long long vectors, elements, table_elements, tasks;
    CUdeviceptr value_ptr, cosine_ptr, sine_ptr;
    unsigned int grid;
    int rc;
    if (!function || !stage || !tokens || !heads || !head_dim || !rotary_dim ||
        rotary_dim > head_dim || (rotary_dim & 1ull) ||
        !yvex_core_u64_mul(tokens, heads, &vectors) ||
        !yvex_core_u64_mul(vectors, head_dim, &elements) ||
        !yvex_core_u64_mul(tokens, rotary_dim, &table_elements) ||
        !yvex_core_u64_mul(vectors, rotary_dim / 2ull, &tasks) ||
        !transformer_tensor(backend, values, elements, 1) ||
        !transformer_tensor(backend, cosines, table_elements, 1) ||
        !transformer_tensor(backend, sines, table_elements, 1) ||
        tasks > (unsigned long long)UINT_MAX * TRANSFORMER_BLOCK) {
        yvex_error_set(err, YVEX_ERR_FORMAT, stage,
                       "packed F32 vectors and explicit rotary tables are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)((tasks + TRANSFORMER_BLOCK - 1ull) / TRANSFORMER_BLOCK);
    value_ptr = yvex_cuda_tensor_ptr(values);
    cosine_ptr = yvex_cuda_tensor_ptr(cosines);
    sine_ptr = yvex_cuda_tensor_ptr(sines);
    {
        void *parameters[] = {
            &value_ptr, &cosine_ptr, &sine_ptr, &tokens, &heads, &head_dim, &rotary_dim,
        };
        rc = transformer_launch(backend, function, grid, 0u,
                                parameters, stage, facts, err);
    }
    return rc;
}

int yvex_cuda_transformer_rotary_half(
    yvex_backend *backend, yvex_device_tensor *values,
    const yvex_device_tensor *cosines, const yvex_device_tensor *sines,
    unsigned long long tokens, unsigned long long heads, unsigned long long head_dim,
    unsigned long long rotary_dim, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    return transformer_rotary_half(
        backend, values, cosines, sines, tokens, heads, head_dim, rotary_dim,
        state ? state->rotary_half_function : NULL, "cuda.transformer.rotary-bf16", facts, err);
}

int yvex_cuda_transformer_rotary_half_f32(
    yvex_backend *backend, yvex_device_tensor *values,
    const yvex_device_tensor *cosines, const yvex_device_tensor *sines,
    unsigned long long tokens, unsigned long long heads, unsigned long long head_dim,
    unsigned long long rotary_dim, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    return transformer_rotary_half(
        backend, values, cosines, sines, tokens, heads, head_dim, rotary_dim,
        state ? state->rotary_half_plain_function : NULL, "cuda.transformer.rotary-f32", facts, err);
}

static int gqa_enqueue(yvex_backend *backend, CUfunction function, unsigned int grid,
                       unsigned int block, unsigned int shared_bytes, void **parameters,
                       const char *stage, yvex_error *err)
{
    if (!function || !grid || !block || !parameters) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "complete tiled-attention launch facts are required");
        return YVEX_ERR_INVALID_ARG;
    }
    return yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                            function, grid, block, shared_bytes, parameters, stage, err);
}

typedef struct {
    yvex_backend *backend;
    yvex_cuda_backend_state *state;
    CUdeviceptr query, key, value, output, scores, status;
    unsigned long long query_tokens, key_value_tokens, query_start;
    unsigned long long heads, head_dim;
    int causal;
} gqa_tile_context;
static int gqa_tile_execute(gqa_tile_context *chunk, unsigned long long local_query_start,
                            unsigned long long query_rows, yvex_error *err)
{
    unsigned long long score_stride, matrix_stride, query_offset, softmax_rows;
    unsigned long long absolute_query_start;
    CUdeviceptr query_chunk, output_chunk;
    const float scale = 1.0f / sqrtf((float)chunk->head_dim), zero = 0.0f, one = 1.0f;
    int blas_status, rc = YVEX_OK;
    if (!yvex_core_u64_mul(query_rows, chunk->key_value_tokens, &score_stride) ||
        !yvex_core_u64_mul(chunk->heads, chunk->head_dim, &matrix_stride) ||
        !yvex_core_u64_mul(local_query_start, matrix_stride, &query_offset) ||
        !yvex_core_u64_mul(query_offset, sizeof(float), &query_offset) ||
        !yvex_core_u64_mul(chunk->heads, query_rows, &softmax_rows) ||
        !yvex_core_u64_add(chunk->query_start, local_query_start,
                           &absolute_query_start) ||
        score_stride > LLONG_MAX || matrix_stride > INT_MAX || softmax_rows > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.transformer.gqa.blas",
                       "tiled attention launch geometry overflowed");
        return YVEX_ERR_BOUNDS;
    }
    query_chunk = chunk->query + query_offset;
    output_chunk = chunk->output + query_offset;
    blas_status = chunk->state->blas.gemm_strided_batched_ex(
        chunk->state->blas.handle, GQA_BLAS_OP_T, GQA_BLAS_OP_N,
        (int)chunk->key_value_tokens, (int)query_rows, (int)chunk->head_dim, &scale,
        (const void *)(uintptr_t)chunk->key, GQA_BLAS_R_32F,
        (int)matrix_stride, (long long)chunk->head_dim,
        (const void *)(uintptr_t)query_chunk, GQA_BLAS_R_32F, (int)matrix_stride,
        (long long)chunk->head_dim, &zero,
        (void *)(uintptr_t)chunk->scores, GQA_BLAS_R_32F,
        (int)chunk->key_value_tokens,
        (long long)score_stride, (int)chunk->heads, GQA_BLAS_COMPUTE_32F,
        GQA_BLAS_DEFAULT);
    if (blas_status != 0) {
        yvex_error_setf(err, YVEX_ERR_BACKEND, "cuda.transformer.gqa.qk",
                        "batched F32 Q/K projection failed with status %d", blas_status);
        rc = YVEX_ERR_BACKEND;
    }
    if (rc == YVEX_OK) {
        CUfunction function = chunk->key_value_tokens <= 1024ull
                                  ? chunk->state->gqa_softmax_warp_function
                                  : chunk->state->gqa_softmax_function;
        unsigned int block = chunk->key_value_tokens <= 1024ull
                                 ? 32u : GQA_BLAS_BLOCK;
        unsigned int shared = chunk->key_value_tokens <= 1024ull
                                  ? 0u : GQA_BLAS_BLOCK * sizeof(float);
        void *parameters[] = {
            &chunk->scores, &chunk->scores, &query_rows,
            &chunk->key_value_tokens,
            &absolute_query_start, &chunk->causal, &chunk->status,
        };
        rc = gqa_enqueue(chunk->backend, function, (unsigned int)softmax_rows, block, shared,
                         parameters, "cuda.transformer.gqa.softmax", err);
    }
    if (rc == YVEX_OK) {
        blas_status = chunk->state->blas.gemm_strided_batched_ex(
            chunk->state->blas.handle, GQA_BLAS_OP_N, GQA_BLAS_OP_N,
            (int)chunk->head_dim, (int)query_rows,
            (int)chunk->key_value_tokens, &one,
            (const void *)(uintptr_t)chunk->value, GQA_BLAS_R_32F,
            (int)matrix_stride, (long long)chunk->head_dim,
            (const void *)(uintptr_t)chunk->scores, GQA_BLAS_R_32F,
            (int)chunk->key_value_tokens, (long long)score_stride, &zero,
            (void *)(uintptr_t)output_chunk, GQA_BLAS_R_32F, (int)matrix_stride,
            (long long)chunk->head_dim, (int)chunk->heads,
            GQA_BLAS_COMPUTE_32F, GQA_BLAS_DEFAULT);
        if (blas_status != 0) {
            yvex_error_setf(err, YVEX_ERR_BACKEND, "cuda.transformer.gqa.pv",
                            "batched F32 probability/value projection failed with status %d",
                            blas_status);
            rc = YVEX_ERR_BACKEND;
        }
    }
    return rc;
}

static int gqa_tiled_execute(
    yvex_backend *backend, const yvex_device_tensor *query,
    const yvex_device_tensor *key, const yvex_device_tensor *value,
    yvex_device_tensor *output, unsigned long long query_tokens,
    unsigned long long key_value_tokens, unsigned long long query_start,
    unsigned long long heads, unsigned long long head_dim, int causal,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {0};
    CUdeviceptr scores = 0ull, status = 0ull;
    unsigned long long score_elements, output_elements, validation_tasks;
    unsigned long long chunks = 0ull, query_tile;
    unsigned long long score_bytes;
    unsigned long long temporary_bytes = 0ull;
    unsigned long long local_query_start;
    int host_status = 0, rc = YVEX_OK, cleanup_rc;
    yvex_error cleanup;
    query_tile = query_tokens < GQA_BLAS_QUERY_CHUNK
                     ? query_tokens : GQA_BLAS_QUERY_CHUNK;
    if (!state || !state->blas.ready || !state->blas.gemm_strided_batched_ex ||
        !state->gqa_softmax_function || !state->gqa_softmax_warp_function ||
        !state->attention_validate_function || query_tokens > INT_MAX ||
        key_value_tokens > INT_MAX ||
        heads > INT_MAX || head_dim > INT_MAX ||
        !yvex_core_u64_mul(heads, query_tile, &score_elements) ||
        !yvex_core_u64_mul(score_elements, key_value_tokens, &score_elements) ||
        !yvex_core_u64_mul(score_elements, sizeof(float), &score_bytes) ||
        !yvex_core_u64_mul(query_tokens, heads, &output_elements) ||
        !yvex_core_u64_mul(output_elements, head_dim, &output_elements) ||
        !yvex_core_u64_add(output_elements, GQA_BLAS_BLOCK - 1ull, &validation_tasks) ||
        score_elements > LLONG_MAX || score_bytes > SIZE_MAX ||
        validation_tasks / GQA_BLAS_BLOCK > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.transformer.gqa.blas",
                       "tiled exact attention exceeds admitted integer geometry");
        return YVEX_ERR_BOUNDS;
    }
    backend_workspace_reset(backend);
    work.backend = backend;
    work.state = state;
    work.variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED;
#define GQA_ALLOC(pointer, bytes, zeroed, label) \
    do { \
        if (rc == YVEX_OK) \
            rc = yvex_cuda_work_allocate(&work, &(pointer), (size_t)(bytes), NULL, (zeroed), \
                                         "cuda.transformer.gqa.alloc." label, NULL, err); \
    } while (0)
    GQA_ALLOC(scores, score_bytes, 0, "scores");
    GQA_ALLOC(status, sizeof(host_status), 1, "status");
#undef GQA_ALLOC
    if (rc == YVEX_OK)
        rc = yvex_cuda_blas_bind_launch_stream(backend, "cuda.transformer.gqa.stream", err);
    if (rc == YVEX_OK)
        temporary_bytes = backend->workspace_device_tensor
                              ? backend->workspace_cursor
                              : work.peak_bytes;
    {
        gqa_tile_context chunk = {
            backend, state, yvex_cuda_tensor_ptr(query), yvex_cuda_tensor_ptr(key),
            yvex_cuda_tensor_ptr(value), yvex_cuda_tensor_ptr(output), scores, status,
            query_tokens, key_value_tokens, query_start, heads, head_dim, causal};
        for (local_query_start = 0ull;
             rc == YVEX_OK && local_query_start < query_tokens;
             local_query_start += GQA_BLAS_QUERY_CHUNK) {
            unsigned long long query_rows = query_tokens - local_query_start;
            if (query_rows > GQA_BLAS_QUERY_CHUNK) query_rows = GQA_BLAS_QUERY_CHUNK;
            rc = gqa_tile_execute(&chunk, local_query_start, query_rows, err);
            ++chunks;
        }
    }
    if (rc == YVEX_OK) {
        CUdeviceptr output_ptr = yvex_cuda_tensor_ptr(output);
        void *parameters[] = {&output_ptr, &output_elements, &status};
        rc = gqa_enqueue(
            backend, state->attention_validate_function,
            (unsigned int)(validation_tasks / GQA_BLAS_BLOCK), GQA_BLAS_BLOCK, 0u,
            parameters, "cuda.transformer.gqa.validate", err);
    }
    if (rc == YVEX_OK)
        rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                                   "cuda.transformer.gqa.sync", err);
    if (rc == YVEX_OK)
        rc = yvex_cuda_status(&state->driver,
                              state->driver.cuMemcpyDtoH_v2(&host_status, status,
                                                            sizeof(host_status)),
                              "cuda.transformer.gqa.status", err);
    if (rc == YVEX_OK && host_status) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.gqa.status",
                       "tiled exact attention produced invalid numerics");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_cuda_work_cleanup(&work, &cleanup);
    backend_workspace_reset(backend);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) {
        rc = cleanup_rc;
        if (err) *err = cleanup;
    }
    if (rc != YVEX_OK) return rc;
    facts->kernel_launches = chunks * 3ull + 1ull;
    facts->download_count = 1ull;
    facts->d2h_bytes = sizeof(host_status);
    facts->temporary_bytes = temporary_bytes;
    facts->accelerated_matrix_launches = chunks * 2ull;
    facts->device_synchronizations = 1ull;
    facts->compulsory_memory_facts_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int gqa_workspace_add(unsigned long long *cursor, unsigned long long bytes)
{
    unsigned long long aligned;
    if (!cursor || !bytes ||
        !yvex_core_u64_add(*cursor, GQA_WORK_ALIGNMENT - 1ull, &aligned))
        return 0;
    aligned &= ~(GQA_WORK_ALIGNMENT - 1ull);
    return yvex_core_u64_add(aligned, bytes, cursor);
}

int yvex_cuda_transformer_gqa_workspace_required(
    unsigned long long query_tokens, unsigned long long key_value_tokens,
    unsigned long long query_heads,
    unsigned long long kv_heads, unsigned long long head_dim,
    unsigned long long *bytes, yvex_error *err)
{
    unsigned long long score_elements, scores, query_tile;
    unsigned long long total = 0ull;
    if (bytes) *bytes = 0ull;
    if (!bytes || !query_tokens || !key_value_tokens || !query_heads ||
        !kv_heads || !head_dim ||
        query_heads % kv_heads) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.transformer.gqa.workspace",
                       "bounded grouped-query attention geometry is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (query_heads != kv_heads) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    query_tile = query_tokens < GQA_BLAS_QUERY_CHUNK
                     ? query_tokens : GQA_BLAS_QUERY_CHUNK;
    if (!yvex_core_u64_mul(query_heads, query_tile, &score_elements) ||
        !yvex_core_u64_mul(score_elements, key_value_tokens, &score_elements) ||
        !yvex_core_u64_mul(score_elements, sizeof(float), &scores) ||
        !gqa_workspace_add(&total, scores) ||
        !gqa_workspace_add(&total, sizeof(int)) || total > SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "cuda.transformer.gqa.workspace",
                       "tiled exact-attention workspace geometry overflowed");
        return YVEX_ERR_BOUNDS;
    }
    *bytes = total;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cuda_transformer_gqa_strided(
    yvex_backend *backend, const yvex_device_tensor *query,
    const yvex_device_tensor *key, const yvex_device_tensor *value,
    yvex_device_tensor *output, unsigned long long query_tokens,
    unsigned long long key_value_tokens, unsigned long long query_start,
    unsigned long long query_heads, unsigned long long kv_heads,
    unsigned long long head_dim, unsigned long long query_stride,
    unsigned long long key_stride, unsigned long long value_stride, int causal,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long query_elements, key_elements, value_elements, output_elements;
    unsigned long long query_width, kv_width;
    unsigned long long rows, grid_rows, query_end;
    float scale;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !facts || !query_tokens || !key_value_tokens ||
        !yvex_core_u64_add(query_start, query_tokens, &query_end) ||
        query_end > key_value_tokens || !query_heads || !kv_heads ||
        query_heads % kv_heads ||
        (causal != 0 && causal != 1) ||
        !head_dim || head_dim > GQA_HEAD_DIMENSION_MAX ||
        !yvex_core_u64_mul(query_tokens, query_heads, &rows) ||
        !yvex_core_u64_mul(query_heads, head_dim, &query_width) ||
        !yvex_core_u64_mul(kv_heads, head_dim, &kv_width) ||
        (query_stride && query_stride < query_width) ||
        (key_stride && key_stride < kv_width) ||
        (value_stride && value_stride < kv_width) ||
        !(query_stride = query_stride ? query_stride : query_width) ||
        !(key_stride = key_stride ? key_stride : kv_width) ||
        !(value_stride = value_stride ? value_stride : kv_width) ||
        !yvex_core_u64_mul(query_tokens - 1ull, query_stride, &query_elements) ||
        !yvex_core_u64_add(query_elements, query_width, &query_elements) ||
        !yvex_core_u64_mul(query_tokens, query_width, &output_elements) ||
        !yvex_core_u64_mul(key_value_tokens - 1ull, key_stride, &key_elements) ||
        !yvex_core_u64_add(key_elements, kv_width, &key_elements) ||
        !yvex_core_u64_mul(key_value_tokens - 1ull, value_stride, &value_elements) ||
        !yvex_core_u64_add(value_elements, kv_width, &value_elements) ||
        !yvex_core_u64_mul(query_tokens / GQA_QUERIES_PER_BLOCK +
                               (query_tokens % GQA_QUERIES_PER_BLOCK != 0ull),
                           query_heads, &grid_rows) || grid_rows > UINT_MAX ||
        !transformer_tensor(backend, query, query_elements, 1) ||
        !transformer_tensor(backend, key, key_elements, 1) ||
        !transformer_tensor(backend, value, value_elements, 1) ||
        !transformer_tensor(backend, output, output_elements, 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.gqa",
                       "bounded Q/K/V views matching the declared token strides are required");
        return YVEX_ERR_FORMAT;
    }
    if (query_heads == kv_heads && query_stride == query_width &&
        key_stride == kv_width && value_stride == kv_width && state->blas.ready &&
        state->blas.gemm_strided_batched_ex && state->gqa_softmax_function &&
        state->gqa_softmax_warp_function && state->attention_validate_function &&
        !yvex_cuda_capture_active(backend)) {
        rc = gqa_tiled_execute(
            backend, query, key, value, output, query_tokens,
            key_value_tokens, query_start, query_heads, head_dim,
            causal, facts, err);
        if (rc == YVEX_OK) output->is_written = 1;
        return rc;
    }
    backend_workspace_reset(backend);
    CUdeviceptr query_ptr = yvex_cuda_tensor_ptr(query), key_ptr = yvex_cuda_tensor_ptr(key);
    CUdeviceptr value_ptr = yvex_cuda_tensor_ptr(value), output_ptr = yvex_cuda_tensor_ptr(output);
    scale = 1.0f / sqrtf((float)head_dim);
    {
        void *parameters[] = {
            &query_ptr, &key_ptr, &value_ptr, &output_ptr,
            &query_tokens, &key_value_tokens, &query_start,
            &query_heads, &kv_heads, &head_dim, &query_stride, &key_stride,
            &value_stride, &scale, &causal,
        };
        rc = transformer_launch(
            backend, head_dim > TRANSFORMER_BLOCK ? state->gqa_wide_function
                                                  : state->gqa_function,
            (unsigned int)grid_rows,
            2u * (unsigned int)head_dim * sizeof(float), parameters,
            "cuda.transformer.gqa", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_silu_product_bf16(
    yvex_backend *backend, const yvex_device_tensor *gate,
    const yvex_device_tensor *up, yvex_device_tensor *output,
    unsigned long long count, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr gate_ptr, up_ptr, output_ptr;
    unsigned long long tasks;
    unsigned int grid;
    int rc;
    if (!state || !count || !transformer_tensor(backend, gate, count, 1) ||
        !transformer_tensor(backend, up, count, 1) ||
        !transformer_tensor(backend, output, count, 0) ||
        !yvex_core_u64_add(count, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.silu-product-bf16",
                       "equal F32 storage with BF16 gate policy is required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    gate_ptr = yvex_cuda_tensor_ptr(gate);
    up_ptr = yvex_cuda_tensor_ptr(up);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {&gate_ptr, &up_ptr, &output_ptr, &count};
        rc = transformer_launch(
            backend, state->silu_product_function, grid, 0u, parameters,
            "cuda.transformer.silu-product-bf16", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_clamped_swiglu_bf16(yvex_backend *backend, const yvex_device_tensor *gate,
    const yvex_device_tensor *up, yvex_device_tensor *output, double limit,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    if (output) output->is_written = 0;
    if (facts) memset(facts, 0, sizeof(*facts));
    const yvex_device_tensor *inputs[] = {gate, up};
    int rc = yvex_neural_elementwise_admit(backend, inputs, 2u, output, facts, err);
    if (rc != YVEX_OK) return rc;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long count = output->bytes / sizeof(float), tasks;
    if (!state || !state->moe_swiglu_function || !isfinite(limit) || limit <= 0.0 ||
        !yvex_core_u64_add(count, TRANSFORMER_BLOCK - 1u, &tasks) || tasks / TRANSFORMER_BLOCK > UINT_MAX)
        return cuda_transformer_refuse(err, YVEX_ERR_UNSUPPORTED, "cuda.clamped-swiglu",
            "positive finite limit and admitted launch required");
    yvex_cuda_work work;
    rc = status_transaction_open(backend, &work, 0, "cuda.clamped-swiglu.status", err);
    CUdeviceptr g = yvex_cuda_tensor_ptr(gate), u = yvex_cuda_tensor_ptr(up), y = yvex_cuda_tensor_ptr(output);
    float weight = 1.0f;
    if (rc == YVEX_OK) {
        void *args[] = {&g, &u, &count, &limit, &weight, &y, &work.status};
        rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, state->moe_swiglu_function,
            (unsigned int)(tasks / TRANSFORMER_BLOCK), TRANSFORMER_BLOCK, 0u, args, "cuda.clamped-swiglu", err);
    }
    rc = status_transaction_close(&work, !work.status_deferred, 0, rc, facts, "cuda.clamped-swiglu.status", err);
    if (rc == YVEX_OK) { output->is_written = 1; facts->kernel_launches = 1u; }
    return rc;
}

int yvex_cuda_transformer_silu(
    yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *output, unsigned long long count, int bf16_output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, output_ptr;
    unsigned long long tasks;
    unsigned int grid;
    int rc;
    if (!state || !count || (bf16_output != 0 && bf16_output != 1) ||
        !transformer_tensor(backend, input, count, 1) ||
        !transformer_tensor(backend, output, count, 0) ||
        !yvex_core_u64_add(count, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.silu",
                       "bounded F32 activation and explicit output policy are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {&input_ptr, &output_ptr, &count, &bf16_output};
        rc = transformer_launch(backend, state->silu_function, grid, 0u,
                                parameters, "cuda.transformer.silu", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_timestep_embedding(
    yvex_backend *backend, const yvex_device_tensor *timesteps,
    yvex_device_tensor *output, unsigned long long rows,
    unsigned long long half_width, float maximum_period,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr timestep_ptr, output_ptr;
    unsigned long long output_width, output_values, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !half_width || !isfinite(maximum_period) ||
        maximum_period <= 1.0f || !yvex_core_u64_mul(half_width, 2ull, &output_width) ||
        !yvex_core_u64_mul(rows, output_width, &output_values) ||
        !transformer_tensor(backend, timesteps, rows, 1) ||
        !transformer_tensor(backend, output, output_values, 0) ||
        !yvex_core_u64_add(rows * half_width, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.timestep-embedding",
                       "bounded F32 timestep embedding geometry is required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    timestep_ptr = yvex_cuda_tensor_ptr(timesteps);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {&timestep_ptr, &output_ptr, &rows,
                              &half_width, &maximum_period};
        rc = transformer_launch(
            backend, state->timestep_embedding_function, grid, 0u, parameters,
            "cuda.transformer.timestep-embedding", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_split_three(
    yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *first, yvex_device_tensor *second, yvex_device_tensor *third,
    unsigned long long rows, unsigned long long width,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, first_ptr, second_ptr, third_ptr;
    unsigned long long output_elements, input_elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || !yvex_core_u64_mul(rows, width, &output_elements) ||
        !yvex_core_u64_mul(output_elements, 3ull, &input_elements) ||
        !yvex_core_u64_add(output_elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, input_elements, 1) ||
        !transformer_tensor(backend, first, output_elements, 0) ||
        !transformer_tensor(backend, second, output_elements, 0) ||
        !transformer_tensor(backend, third, output_elements, 0) ||
        first == second || first == third || second == third || input == first ||
        input == second || input == third) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.split-three",
                       "one packed input and three independent outputs are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    first_ptr = yvex_cuda_tensor_ptr(first);
    second_ptr = yvex_cuda_tensor_ptr(second);
    third_ptr = yvex_cuda_tensor_ptr(third);
    {
        void *parameters[] = {
            &input_ptr, &first_ptr, &second_ptr, &third_ptr, &rows, &width,
        };
        rc = transformer_launch(backend, state->split_three_function, grid, 0u,
                                parameters, "cuda.transformer.split-three", facts, err);
    }
    if (rc == YVEX_OK) first->is_written = second->is_written = third->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_split_interleaved_three(
    yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *first, yvex_device_tensor *second, yvex_device_tensor *third,
    unsigned long long rows, unsigned long long heads, unsigned long long head_dim,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, first_ptr, second_ptr, third_ptr;
    unsigned long long width, output_elements, input_elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !heads || !head_dim ||
        !yvex_core_u64_mul(heads, head_dim, &width) ||
        !yvex_core_u64_mul(rows, width, &output_elements) ||
        !yvex_core_u64_mul(output_elements, 3ull, &input_elements) ||
        !yvex_core_u64_add(output_elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, input_elements, 1) ||
        !transformer_tensor(backend, first, output_elements, 0) ||
        !transformer_tensor(backend, second, output_elements, 0) ||
        !transformer_tensor(backend, third, output_elements, 0) ||
        first == second || first == third || second == third || input == first ||
        input == second || input == third) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.split-interleaved",
                       "interleaved per-head Q/K/V input and independent outputs are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    first_ptr = yvex_cuda_tensor_ptr(first);
    second_ptr = yvex_cuda_tensor_ptr(second);
    third_ptr = yvex_cuda_tensor_ptr(third);
    {
        void *parameters[] = {
            &input_ptr, &first_ptr, &second_ptr, &third_ptr, &rows, &heads, &head_dim,
        };
        rc = transformer_launch(
            backend, state->split_interleaved_function, grid, 0u, parameters,
            "cuda.transformer.split-interleaved", facts, err);
    }
    if (rc == YVEX_OK) first->is_written = second->is_written = third->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_swiglu_split_bf16(
    yvex_backend *backend, const yvex_device_tensor *input, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, output_ptr;
    unsigned long long output_elements, input_elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || !yvex_core_u64_mul(rows, width, &output_elements) ||
        !yvex_core_u64_mul(output_elements, 2ull, &input_elements) ||
        !yvex_core_u64_add(output_elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, input_elements, 1) ||
        !transformer_tensor(backend, output, output_elements, 0) || input == output) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.swiglu-split-bf16",
                       "packed BF16-policy SwiGLU storage is required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {&input_ptr, &output_ptr, &rows, &width};
        rc = transformer_launch(backend, state->swiglu_split_function, grid, 0u,
                                parameters, "cuda.transformer.swiglu-split-bf16", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_swiglu_split_f32(
    yvex_backend *backend, const yvex_device_tensor *input, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, int gate_first,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, output_ptr;
    unsigned long long output_elements, input_elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || (gate_first != 0 && gate_first != 1) ||
        !yvex_core_u64_mul(rows, width, &output_elements) ||
        !yvex_core_u64_mul(output_elements, 2ull, &input_elements) ||
        !yvex_core_u64_add(output_elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, input_elements, 1) ||
        !transformer_tensor(backend, output, output_elements, 0) || input == output) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.swiglu-split-f32",
                       "packed F32 SwiGLU storage and explicit gate order are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {&input_ptr, &output_ptr, &rows, &width, &gate_first};
        rc = transformer_launch(
            backend, state->swiglu_split_f32_function, grid, 0u,
            parameters, "cuda.transformer.swiglu-split-f32", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

static int transformer_indices_validate(const unsigned int *indices,
                                        unsigned long long rows,
                                        unsigned long long table_rows)
{
    unsigned long long row;
    if (!indices || !rows || !table_rows) return 0;
    for (row = 0ull; row < rows; ++row)
        if ((unsigned long long)indices[row] >= table_rows) return 0;
    return 1;
}

int yvex_cuda_transformer_modulate_bf16(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *table, const unsigned int *row_indices,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width,
    unsigned long long table_rows, unsigned long long parameters,
    unsigned int shift_slot, unsigned int scale_slot,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {0};
    CUdeviceptr input_ptr, table_ptr, indices_ptr = 0ull, output_ptr;
    unsigned long long elements, table_elements, index_bytes, tasks;
    unsigned int grid;
    int rc, cleanup_rc;
    yvex_error cleanup;
    if (!state || !rows || !width || !parameters || shift_slot >= parameters ||
        scale_slot >= parameters || !transformer_indices_validate(row_indices, rows, table_rows) ||
        !yvex_core_u64_mul(rows, width, &elements) ||
        !yvex_core_u64_mul(table_rows, parameters, &table_elements) ||
        !yvex_core_u64_mul(table_elements, width, &table_elements) ||
        !yvex_core_u64_mul(rows, sizeof(*row_indices), &index_bytes) || index_bytes > SIZE_MAX ||
        !yvex_core_u64_add(elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, elements, 1) ||
        !transformer_tensor(backend, table, table_elements, 1) ||
        !transformer_tensor(backend, output, elements, 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.modulate-bf16",
                       "bounded modulation table and row selection are required");
        return YVEX_ERR_FORMAT;
    }
    work.backend = backend;
    work.state = state;
    work.variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED;
    rc = yvex_cuda_work_allocate(&work, &indices_ptr, (size_t)index_bytes, row_indices, 0,
                                 "cuda.transformer.modulate-bf16.indices", NULL, err);
    input_ptr = yvex_cuda_tensor_ptr(input);
    table_ptr = yvex_cuda_tensor_ptr(table);
    output_ptr = yvex_cuda_tensor_ptr(output);
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    if (rc == YVEX_OK) {
        void *arguments[] = {
            &input_ptr, &table_ptr, &indices_ptr, &output_ptr, &rows, &width,
            &table_rows, &parameters, &shift_slot, &scale_slot,
        };
        rc = transformer_launch(backend, state->modulation_function, grid, 0u,
                                arguments, "cuda.transformer.modulate-bf16", facts, err);
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_cuda_work_cleanup(&work, &cleanup);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (rc == YVEX_OK) {
        output->is_written = 1;
        facts->h2d_bytes = index_bytes;
        facts->temporary_bytes = index_bytes;
        facts->upload_count = 1ull;
    }
    return rc;
}

int yvex_cuda_transformer_gated_residual_bf16(
    yvex_backend *backend, const yvex_device_tensor *residual,
    const yvex_device_tensor *table, const unsigned int *row_indices,
    const yvex_device_tensor *update, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, unsigned long long table_rows,
    unsigned long long parameters, unsigned int gate_slot,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {0};
    CUdeviceptr residual_ptr, table_ptr, indices_ptr = 0ull, update_ptr, output_ptr;
    unsigned long long elements, table_elements, index_bytes, tasks;
    unsigned int grid;
    int rc, cleanup_rc;
    yvex_error cleanup;
    if (!state || !rows || !width || !parameters || gate_slot >= parameters ||
        !transformer_indices_validate(row_indices, rows, table_rows) ||
        !yvex_core_u64_mul(rows, width, &elements) ||
        !yvex_core_u64_mul(table_rows, parameters, &table_elements) ||
        !yvex_core_u64_mul(table_elements, width, &table_elements) ||
        !yvex_core_u64_mul(rows, sizeof(*row_indices), &index_bytes) || index_bytes > SIZE_MAX ||
        !yvex_core_u64_add(elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, residual, elements, 1) ||
        !transformer_tensor(backend, table, table_elements, 1) ||
        !transformer_tensor(backend, update, elements, 1) ||
        !transformer_tensor(backend, output, elements, 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.gated-residual-bf16",
                       "bounded residual, update, gate table, and row selection are required");
        return YVEX_ERR_FORMAT;
    }
    work.backend = backend;
    work.state = state;
    work.variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED;
    rc = yvex_cuda_work_allocate(&work, &indices_ptr, (size_t)index_bytes, row_indices, 0,
                                 "cuda.transformer.gated-residual-bf16.indices", NULL, err);
    residual_ptr = yvex_cuda_tensor_ptr(residual);
    table_ptr = yvex_cuda_tensor_ptr(table);
    update_ptr = yvex_cuda_tensor_ptr(update);
    output_ptr = yvex_cuda_tensor_ptr(output);
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    if (rc == YVEX_OK) {
        void *arguments[] = {
            &residual_ptr, &table_ptr, &indices_ptr, &update_ptr, &output_ptr,
            &rows, &width, &table_rows, &parameters, &gate_slot,
        };
        rc = transformer_launch(backend, state->gated_residual_function, grid, 0u,
                                arguments, "cuda.transformer.gated-residual-bf16", facts, err);
    }
    yvex_error_clear(&cleanup);
    cleanup_rc = yvex_cuda_work_cleanup(&work, &cleanup);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) { rc = cleanup_rc; if (err) *err = cleanup; }
    if (rc == YVEX_OK) {
        output->is_written = 1;
        facts->h2d_bytes = index_bytes;
        facts->temporary_bytes = index_bytes;
        facts->upload_count = 1ull;
    }
    return rc;
}

int yvex_cuda_transformer_bias(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *bias, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, int bf16_output,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, bias_ptr, output_ptr;
    unsigned long long elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || (bf16_output != 0 && bf16_output != 1) ||
        !yvex_core_u64_mul(rows, width, &elements) ||
        !yvex_core_u64_add(elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, input, elements, 1) ||
        !transformer_tensor(backend, bias, width, 1) ||
        !transformer_tensor(backend, output, elements, 0) || bias == output) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.bias",
                       "bounded row-major activation and independent bias are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    input_ptr = yvex_cuda_tensor_ptr(input);
    bias_ptr = yvex_cuda_tensor_ptr(bias);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *arguments[] = {
            &input_ptr, &bias_ptr, &output_ptr, &rows, &width, &bf16_output,
        };
        rc = transformer_launch(backend, state->bias_function, grid, 0u,
                                arguments, "cuda.transformer.bias", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_add_bf16(
    yvex_backend *backend, const yvex_device_tensor *left,
    const yvex_device_tensor *right, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr left_ptr, right_ptr, output_ptr;
    unsigned long long elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || !yvex_core_u64_mul(rows, width, &elements) ||
        !yvex_core_u64_add(elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, left, elements, 1) ||
        !transformer_tensor(backend, right, elements, 1) ||
        !transformer_tensor(backend, output, elements, 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.add-bf16",
                       "bounded BF16-compatible activation rows are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    left_ptr = yvex_cuda_tensor_ptr(left);
    right_ptr = yvex_cuda_tensor_ptr(right);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *arguments[] = {
            &left_ptr, &right_ptr, &output_ptr, &rows, &width,
        };
        rc = transformer_launch(backend, state->add_bf16_function, grid, 0u,
                                arguments, "cuda.transformer.add-bf16", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_scaled_residual_f32(
    yvex_backend *backend, const yvex_device_tensor *residual,
    const yvex_device_tensor *update, const yvex_device_tensor *scale,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr residual_ptr, update_ptr, scale_ptr, output_ptr;
    unsigned long long elements, tasks;
    unsigned int grid;
    int rc;
    if (!state || !rows || !width || !yvex_core_u64_mul(rows, width, &elements) ||
        !yvex_core_u64_add(elements, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX ||
        !transformer_tensor(backend, residual, elements, 1) ||
        !transformer_tensor(backend, update, elements, 1) ||
        !transformer_tensor(backend, scale, width, 1) ||
        !transformer_tensor(backend, output, elements, 0) || scale == output) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.scaled-residual-f32",
                       "bounded F32 residual, update, and scale tensors are required");
        return YVEX_ERR_FORMAT;
    }
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    residual_ptr = yvex_cuda_tensor_ptr(residual);
    update_ptr = yvex_cuda_tensor_ptr(update);
    scale_ptr = yvex_cuda_tensor_ptr(scale);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *arguments[] = {
            &residual_ptr, &update_ptr, &scale_ptr, &output_ptr, &rows, &width,
        };
        rc = transformer_launch(
            backend, state->scaled_residual_f32_function, grid, 0u, arguments,
            "cuda.transformer.scaled-residual-f32", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_layer_norm_f32(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *weight, const yvex_device_tensor *bias,
    yvex_device_tensor *output, unsigned long long rows, unsigned long long width,
    float epsilon, yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUdeviceptr input_ptr, weight_ptr, bias_ptr, output_ptr;
    unsigned long long elements;
    int rc;
    if (!state || !rows || rows > UINT_MAX || !width || epsilon <= 0.0f ||
        !yvex_core_u64_mul(rows, width, &elements) ||
        !transformer_tensor(backend, input, elements, 1) ||
        !transformer_tensor(backend, weight, width, 1) ||
        !transformer_tensor(backend, bias, width, 1) ||
        !transformer_tensor(backend, output, elements, 0) ||
        weight == output || bias == output) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.layer-norm-f32",
                       "bounded F32 rows and independent affine vectors are required");
        return YVEX_ERR_FORMAT;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    bias_ptr = yvex_cuda_tensor_ptr(bias);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *arguments[] = {
            &input_ptr, &weight_ptr, &bias_ptr, &output_ptr, &rows, &width, &epsilon,
        };
        rc = transformer_launch(
            backend, state->layer_norm_f32_function, (unsigned int)rows,
            TRANSFORMER_BLOCK * sizeof(float), arguments,
            "cuda.transformer.layer-norm-f32", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}

int yvex_cuda_transformer_bf16_round(
    yvex_backend *backend, yvex_device_tensor *values, unsigned long long count,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    CUdeviceptr values_ptr;
    unsigned long long tasks;
    unsigned int grid;
    int rc;
    if (facts) memset(facts, 0, sizeof(*facts));
    if (!state || !facts || !count || !transformer_tensor(backend, values, count, 1) ||
        !yvex_core_u64_add(count, TRANSFORMER_BLOCK - 1ull, &tasks) ||
        tasks / TRANSFORMER_BLOCK > UINT_MAX) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.bf16-round",
                       "one finite bounded F32 activation is required");
        return YVEX_ERR_FORMAT;
    }
    rc = status_transaction_open(
        backend, &work, 1, "cuda.transformer.bf16-round.status", err);
    values_ptr = yvex_cuda_tensor_ptr(values);
    grid = (unsigned int)(tasks / TRANSFORMER_BLOCK);
    if (rc == YVEX_OK) {
        void *parameters[] = {&values_ptr, &count, &work.status};
        rc = yvex_cuda_launch(
            backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            state->attention_bf16_round_function, grid, TRANSFORMER_BLOCK, 0u,
            parameters, "cuda.transformer.bf16-round", err);
    }
    rc = status_transaction_close(
        &work, 1, 1, rc, facts, "cuda.transformer.bf16-round.status", err);
    if (rc == YVEX_OK) {
        facts->kernel_launches = 1ull;
        facts->compulsory_memory_facts_available = 1;
    }
    return rc;
}
int yvex_cuda_transformer_rms_norm_bf16(
    yvex_backend *backend, const yvex_device_tensor *input,
    const yvex_device_tensor *weight, yvex_device_tensor *output,
    unsigned long long rows, unsigned long long width, float epsilon,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long elements;
    CUdeviceptr input_ptr, weight_ptr, output_ptr;
    int rc;
    if (!state || !rows || !width || rows > UINT_MAX || !isfinite(epsilon) || epsilon <= 0.0f ||
        !yvex_core_u64_mul(rows, width, &elements) ||
        !transformer_tensor(backend, input, elements, 1) ||
        !transformer_tensor(backend, weight, width, 1) ||
        !transformer_tensor(backend, output, elements, 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.transformer.rms-norm-bf16",
                       "packed F32 storage with exact BF16 normalization policy is required");
        return YVEX_ERR_FORMAT;
    }
    input_ptr = yvex_cuda_tensor_ptr(input);
    weight_ptr = yvex_cuda_tensor_ptr(weight);
    output_ptr = yvex_cuda_tensor_ptr(output);
    {
        void *parameters[] = {
            &input_ptr, &weight_ptr, &output_ptr, &width, &rows, &epsilon,
        };
        rc = transformer_launch(backend, state->rms_norm_bf16_policy_function,
                                (unsigned int)rows, 4u * sizeof(float), parameters,
                                "cuda.transformer.rms-norm-bf16", facts, err);
    }
    if (rc == YVEX_OK) output->is_written = 1;
    return rc;
}
