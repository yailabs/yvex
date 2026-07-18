/*
 * cuda_deepseek_attention.c - complete one-token DeepSeek CUDA executor.
 *
 * Owner:
 *   src/backend/cuda
 *
 * Owns:
 *   bounded Driver-API allocation/transfer/launch lifecycle for encoded
 *   DeepSeek attention weights and the admitted device numerical stages.
 *
 * Does not own:
 *   artifact IO, logical tensor lookup, architecture construction, graph
 *   identity, persistent KV, independent reference arithmetic, CLI output,
 *   transformer orchestration, or generation.
 *
 * Invariants:
 *   encoded tensors remain encoded on device; every numerical stage is a
 *   generated-PTX kernel; host outputs remain unchanged until all device work
 *   and status checks complete; every temporary is released on every path.
 *
 * Boundary:
 *   device-complete attention for one stateful token is not a decode loop.
 */
#include <yvex/backend.h>

#include "driver.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define YVEX_CUDA_ATTN_MAX_ALLOCATIONS 96u
#define YVEX_CUDA_ATTN_BLOCK 256u

typedef struct {
    yvex_backend *backend;
    yvex_cuda_backend_state *state;
    CUdeviceptr pointers[YVEX_CUDA_ATTN_MAX_ALLOCATIONS];
    unsigned long long sizes[YVEX_CUDA_ATTN_MAX_ALLOCATIONS];
    unsigned int count;
    unsigned long long current_bytes;
    unsigned long long peak_bytes;
    unsigned long long budget;
    unsigned long long launches;
} cuda_attention_resources;

static int cuda_attention_fail(yvex_backend_attention_failure *failure,
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
    yvex_error_set(err, status, stage, message);
    return status;
}

static int cuda_attention_checked_bytes(unsigned long long count,
                                        unsigned long long width,
                                        size_t *out)
{
    if (!out || width == 0ull || count > (unsigned long long)SIZE_MAX / width)
        return 0;
    *out = (size_t)(count * width);
    return 1;
}

static int cuda_attention_checked_mul(unsigned long long left,
                                      unsigned long long right,
                                      unsigned long long *out)
{
    if (!out || (left && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

static int cuda_attention_checked_add(unsigned long long left,
                                      unsigned long long right,
                                      unsigned long long *out)
{
    if (!out || right > ULLONG_MAX - left) return 0;
    *out = left + right;
    return 1;
}

/* Contract: owns one raw device allocation and optionally initializes it. */
static int cuda_attention_allocate(cuda_attention_resources *resources,
                                   CUdeviceptr *out,
                                   size_t bytes,
                                   const void *source,
                                   int zero,
                                   const char *stage,
                                   yvex_backend_attention_failure *failure,
                                   yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    unsigned long long next;
    int rc;

    if (!resources || !out || bytes == 0u ||
        resources->count >= YVEX_CUDA_ATTN_MAX_ALLOCATIONS)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention allocation request is invalid");
    if (resources->current_bytes > ULLONG_MAX - (unsigned long long)bytes)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage, ULLONG_MAX,
            resources->current_bytes, err, YVEX_ERR_BOUNDS,
            "CUDA attention device-byte accounting overflowed");
    next = resources->current_bytes + (unsigned long long)bytes;
    if (resources->budget && next > resources->budget)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage,
            resources->budget, next, err, YVEX_ERR_NOMEM,
            "CUDA attention device memory budget exceeded");
    if (injected && strcmp(injected, "allocation") == 0)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION, stage, bytes,
            0ull, err, YVEX_ERR_NOMEM,
            "injected CUDA attention allocation failure");
    rc = yvex_cuda_status(
        &resources->state->driver,
        resources->state->driver.cuMemAlloc_v2(out, bytes), stage, err);
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION, stage, bytes,
            0ull, err, (yvex_status)rc,
            "CUDA attention device allocation failed");
    resources->pointers[resources->count] = *out;
    resources->sizes[resources->count] = (unsigned long long)bytes;
    resources->count++;
    resources->current_bytes = next;
    if (next > resources->peak_bytes) resources->peak_bytes = next;
    if (source) {
        if (injected && strcmp(injected, "copy-input") == 0)
            return cuda_attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
                err, YVEX_ERR_BACKEND,
                "injected CUDA attention input copy failure");
        rc = yvex_cuda_status(
            &resources->state->driver,
            resources->state->driver.cuMemcpyHtoD_v2(*out, source, bytes),
            stage, err);
    } else if (zero) {
        rc = yvex_cuda_status(
            &resources->state->driver,
            resources->state->driver.cuMemsetD8_v2(*out, 0u, bytes), stage,
            err);
    } else {
        rc = YVEX_OK;
    }
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            err, (yvex_status)rc,
            "CUDA attention device initialization failed");
    return YVEX_OK;
}

/* Contract: releases every owned device pointer in reverse acquisition order. */
static int cuda_attention_cleanup(cuda_attention_resources *resources,
                                  yvex_error *err)
{
    int result = YVEX_OK;
    yvex_error cleanup_error;

    if (!resources) return YVEX_OK;
    while (resources->count) {
        unsigned int index = --resources->count;
        int rc = yvex_cuda_temporary_free(
            resources->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            resources->pointers[index], "cuda.deepseek_attention.cleanup",
            &cleanup_error);
        if (resources->current_bytes >= resources->sizes[index])
            resources->current_bytes -= resources->sizes[index];
        else
            resources->current_bytes = 0ull;
        resources->pointers[index] = 0u;
        if (rc != YVEX_OK && result == YVEX_OK) {
            result = rc;
            if (err) *err = cleanup_error;
        }
    }
    return result;
}

/* Contract: launches one admitted stage without synchronizing intermediate work. */
static int cuda_attention_launch(cuda_attention_resources *resources,
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

    if (injected && strcmp(injected, stage) == 0)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, YVEX_ERR_BACKEND,
            "injected CUDA attention kernel launch failure");
    rc = yvex_cuda_launch(
        resources->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        function, grid, block, shared_bytes, params, stage, err);
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH, stage, 1ull, 0ull,
            err, (yvex_status)rc,
            "CUDA attention kernel launch failed");
    resources->launches++;
    return YVEX_OK;
}

static int cuda_attention_matvec(cuda_attention_resources *resources,
                                 const yvex_backend_attention_weight *weight,
                                 CUdeviceptr device_weight,
                                 unsigned long long start_row,
                                 unsigned long long rows,
                                 CUdeviceptr vector,
                                 CUdeviceptr out,
                                 CUdeviceptr status,
                                 const char *stage,
                                 yvex_backend_attention_failure *failure,
                                 yvex_error *err)
{
    void *params[9];
    if (!weight || !weight->present || !device_weight || !vector || !out ||
        !rows || start_row > weight->row_count ||
        rows > weight->row_count - start_row || rows > UINT_MAX)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_count : 0ull, start_row + rows, err,
            YVEX_ERR_BOUNDS, "CUDA attention matvec geometry is invalid");
    params[0] = &device_weight;
    params[1] = (void *)&weight->row_bytes;
    params[2] = (void *)&weight->row_width;
    params[3] = &start_row;
    params[4] = &rows;
    params[5] = (void *)&weight->qtype;
    params[6] = &vector;
    params[7] = &out;
    params[8] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_qtype_matvec_function,
        (unsigned int)rows, YVEX_CUDA_ATTN_BLOCK,
        YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
        failure, err);
}

static int cuda_attention_decode(cuda_attention_resources *resources,
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
    void *params[5];
    if (!weight || !weight->present || row >= weight->row_count ||
        count != weight->row_width || count > UINT_MAX *
            (unsigned long long)YVEX_CUDA_ATTN_BLOCK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_width : 0ull, count, err, YVEX_ERR_BOUNDS,
            "CUDA attention decode geometry is invalid");
    encoded = device_weight + row * weight->row_bytes;
    grid = (unsigned int)((count + YVEX_CUDA_ATTN_BLOCK - 1ull) /
                          YVEX_CUDA_ATTN_BLOCK);
    params[0] = &encoded;
    params[1] = &count;
    params[2] = (void *)&weight->qtype;
    params[3] = &out;
    params[4] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_decode_function, grid,
        YVEX_CUDA_ATTN_BLOCK, 0u, params, stage, failure, err);
}

static int cuda_attention_weighted_norm(
    cuda_attention_resources *resources,
    CUdeviceptr values,
    unsigned long long count,
    const yvex_backend_attention_weight *weight,
    CUdeviceptr device_weight,
    double epsilon,
    CUdeviceptr status,
    const char *stage,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    void *params[6];
    if (!weight || !weight->present || weight->row_count != 1ull ||
        weight->row_width != count)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            count, weight ? weight->row_width : 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention normalization weight shape is invalid");
    params[0] = &values;
    params[1] = &count;
    params[2] = &device_weight;
    params[3] = (void *)&weight->qtype;
    params[4] = &epsilon;
    params[5] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_weighted_norm_function, 1u,
        YVEX_CUDA_ATTN_BLOCK,
        YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
        failure, err);
}

static int cuda_attention_unit_norm(cuda_attention_resources *resources,
                                    CUdeviceptr values,
                                    unsigned long long vectors,
                                    unsigned long long width,
                                    double epsilon,
                                    CUdeviceptr status,
                                    const char *stage,
                                    yvex_backend_attention_failure *failure,
                                    yvex_error *err)
{
    void *params[6];
    if (!vectors || vectors > UINT_MAX || !width)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            1ull, vectors, err, YVEX_ERR_BOUNDS,
            "CUDA attention unit-norm geometry is invalid");
    params[0] = &values;
    params[1] = &vectors;
    params[2] = &width;
    params[3] = &epsilon;
    params[4] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_unit_norm_function,
        (unsigned int)vectors, YVEX_CUDA_ATTN_BLOCK,
        YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
        failure, err);
}

static int cuda_attention_rope(cuda_attention_resources *resources,
                               CUdeviceptr values,
                               unsigned long long vectors,
                               unsigned long long width,
                               unsigned long long token_position,
                               const yvex_backend_attention_position *position,
                               int inverse,
                               CUdeviceptr status,
                               const char *stage,
                               yvex_backend_attention_failure *failure,
                               yvex_error *err)
{
    unsigned long long total;
    unsigned int grid;
    void *params[12];
    if (!position || !position->rope_dimensions ||
        position->rope_dimensions > width ||
        vectors > ULLONG_MAX / (position->rope_dimensions / 2ull))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            width, position ? position->rope_dimensions : 0ull, err,
            YVEX_ERR_BOUNDS, "CUDA attention RoPE geometry is invalid");
    total = vectors * (position->rope_dimensions / 2ull);
    if (!total || total > UINT_MAX * (unsigned long long)YVEX_CUDA_ATTN_BLOCK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            UINT_MAX, total, err, YVEX_ERR_BOUNDS,
            "CUDA attention RoPE launch extent is invalid");
    grid = (unsigned int)((total + YVEX_CUDA_ATTN_BLOCK - 1ull) /
                          YVEX_CUDA_ATTN_BLOCK);
    params[0] = &values;
    params[1] = &vectors;
    params[2] = &width;
    params[3] = (void *)&position->rope_dimensions;
    params[4] = &token_position;
    params[5] = (void *)&position->theta;
    params[6] = (void *)&position->scaling_factor;
    params[7] = (void *)&position->original_context;
    params[8] = (void *)&position->beta_fast;
    params[9] = (void *)&position->beta_slow;
    params[10] = &inverse;
    params[11] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_rope_function, grid,
        YVEX_CUDA_ATTN_BLOCK, 0u, params, stage, failure, err);
}

static int cuda_attention_activation(
    cuda_attention_resources *resources,
    CUdeviceptr values,
    unsigned long long vectors,
    unsigned long long width,
    const yvex_backend_attention_activation *policy,
    CUdeviceptr status,
    const char *stage,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    void *params[7];
    if (!policy || !policy->required) return YVEX_OK;
    if (!vectors || vectors > UINT_MAX || !width || !policy->block_width ||
        width % policy->block_width)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            policy->block_width, width, err, YVEX_ERR_BOUNDS,
            "CUDA attention activation geometry is invalid");
    params[0] = &values;
    params[1] = &vectors;
    params[2] = &width;
    params[3] = (void *)&policy->block_width;
    params[4] = (void *)&policy->quantization;
    params[5] = (void *)&policy->hadamard;
    params[6] = &status;
    return cuda_attention_launch(
        resources, resources->state->deepseek_activation_function,
        (unsigned int)vectors, 1u, 0u, params, stage, failure, err);
}

/* Contract: validates immutable host job geometry before any device allocation. */
static int cuda_attention_validate_job(
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    const yvex_backend_attention_weight_slot required[] = {
        YVEX_BACKEND_ATTENTION_WEIGHT_Q_A,
        YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM,
        YVEX_BACKEND_ATTENTION_WEIGHT_Q_B,
        YVEX_BACKEND_ATTENTION_WEIGHT_KV,
        YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM,
        YVEX_BACKEND_ATTENTION_WEIGHT_SINKS,
        YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A,
        YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B
    };
    unsigned int i;

    if (!job || !output || !job->input || !job->hidden_width || !job->q_rank ||
        !job->query_heads || !job->head_dimension || !job->kv_width ||
        job->query_heads > ULLONG_MAX / job->head_dimension ||
        job->query_heads * job->head_dimension > (unsigned long long)SIZE_MAX /
            sizeof(float) || !output->q_low || !output->query ||
        !output->raw_kv || !output->attention_values || !output->output)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate", 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "CUDA attention job and output geometry are required");
    for (i = 0u; i < sizeof(required) / sizeof(required[0]); ++i) {
        const yvex_backend_attention_weight *weight = &job->weights[required[i]];
        if (!weight->present || !weight->encoded || !weight->encoded_bytes ||
            !weight->row_bytes || !weight->row_width || !weight->row_count)
            return cuda_attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                "cuda.deepseek_attention.validate.weight", 1ull, required[i],
                err, YVEX_ERR_FORMAT,
                "CUDA attention required encoded weight is absent");
    }
    if (job->local_count &&
        (!job->local_kv || !job->local_positions ||
         job->local_stride < job->head_dimension))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.local_history",
            job->head_dimension, job->local_stride, err, YVEX_ERR_FORMAT,
            "CUDA attention local history is incomplete");
    if (job->compressed_count &&
        (!job->compressed_kv || !job->compressed_positions ||
         job->compressed_stride < job->head_dimension))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.compressed_history",
            job->head_dimension, job->compressed_stride, err,
            YVEX_ERR_FORMAT,
            "CUDA attention compressed history is incomplete");
    if (job->attention_class == 1u && job->indexer_count != job->compressed_count)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.index_history",
            job->compressed_count, job->indexer_count, err, YVEX_ERR_FORMAT,
            "CUDA CSA indexer/compressed history cardinality differs");
    return YVEX_OK;
}

/* Contract: executes every admitted DeepSeek attention numerical stage on CUDA. */
int yvex_backend_attention_execute(
    yvex_backend *backend,
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    cuda_attention_resources resources;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_backend_attention_output committed;
    CUdeviceptr weight[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    CUdeviceptr input = 0u, q_low = 0u, query = 0u, raw_kv = 0u;
    CUdeviceptr sinks = 0u, attention = 0u, low = 0u, final_output = 0u;
    CUdeviceptr main_kv = 0u, main_score = 0u, main_ape = 0u;
    CUdeviceptr main_before_kv = 0u, main_before_score = 0u;
    CUdeviceptr main_after_kv = 0u, main_after_score = 0u;
    CUdeviceptr compressed = 0u, compressed_positions = 0u;
    CUdeviceptr index_kv = 0u, index_score = 0u, index_ape = 0u;
    CUdeviceptr index_before_kv = 0u, index_before_score = 0u;
    CUdeviceptr index_after_kv = 0u, index_after_score = 0u;
    CUdeviceptr indexer = 0u, indexer_positions = 0u;
    CUdeviceptr index_query = 0u, index_weights = 0u;
    CUdeviceptr history_local = 0u, history_local_positions = 0u;
    CUdeviceptr history_compressed = 0u, history_compressed_positions = 0u;
    CUdeviceptr history_indexer = 0u, history_indexer_positions = 0u;
    CUdeviceptr selected = 0u, selected_positions = 0u;
    CUdeviceptr selected_count = 0u, valid_count = 0u;
    CUdeviceptr topk_scores = 0u, valid_indexes = 0u, device_status = 0u;
    unsigned long long query_width;
    unsigned long long current_compressed_count = 0ull;
    unsigned long long current_indexer_count = 0ull;
    unsigned long long candidate_capacity = 1ull;
    unsigned long long topk_capacity = 1ull;
    unsigned long long main_extent = 0ull, index_extent = 0ull;
    unsigned long long low_count = 0ull;
    unsigned long long local_extent = 0ull;
    unsigned long long compressed_extent = 0ull;
    unsigned long long history_index_extent = 0ull;
    unsigned long long index_query_extent = 0ull;
    unsigned long long emission_position = 0ull;
    unsigned int slot;
    int host_status = 0;
    int rc;
    int cleanup_rc;
    size_t bytes;
    const char *copy_failure;

    memset(&resources, 0, sizeof(resources));
    memset(&committed, 0, sizeof(committed));
    memset(weight, 0, sizeof(weight));
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    if (!backend || !state)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.execute", 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG, "CUDA attention backend is required");
    rc = cuda_attention_validate_job(job, output, failure, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_cuda_require_capability(
        backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.capability", err);
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.deepseek_attention.capability", 1ull, 0ull, err,
            (yvex_status)rc,
            "CUDA DeepSeek attention kernel bundle is not admitted");
    rc = yvex_cuda_set_current(backend, "cuda.deepseek_attention.context", err);
    if (rc != YVEX_OK) return rc;
    resources.backend = backend;
    resources.state = state;
    resources.budget = job->max_device_bytes;
    if (!cuda_attention_checked_mul(job->query_heads, job->head_dimension,
                                    &query_width) ||
        !cuda_attention_checked_mul(job->output_groups, job->output_rank,
                                    &low_count) ||
        !cuda_attention_checked_mul(job->local_count, job->local_stride,
                                    &local_extent) ||
        !cuda_attention_checked_mul(job->compressed_count,
                                    job->compressed_stride,
                                    &compressed_extent) ||
        !cuda_attention_checked_mul(job->indexer_count, job->indexer_stride,
                                    &history_index_extent) ||
        !cuda_attention_checked_mul(job->indexer_heads,
                                    job->indexer_head_dimension,
                                    &index_query_extent))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
            "cuda.deepseek_attention.validate.extent", ULLONG_MAX, 0ull,
            err, YVEX_ERR_BOUNDS,
            "CUDA attention logical extent overflowed");

#define ALLOC_VALUE(target, count, type, source, zero, stage) do {             \
        if (!cuda_attention_checked_bytes((count), sizeof(type), &bytes)) {     \
            rc = cuda_attention_fail(failure,                                  \
                YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage, ULLONG_MAX, count,   \
                err, YVEX_ERR_BOUNDS, "CUDA attention allocation size overflowed"); \
            goto cleanup;                                                      \
        }                                                                      \
        rc = cuda_attention_allocate(&resources, &(target), bytes, source,     \
                                     zero, stage, failure, err);                \
        if (rc != YVEX_OK) goto cleanup;                                       \
    } while (0)

    ALLOC_VALUE(device_status, 1ull, int, NULL, 1,
                "cuda.deepseek_attention.alloc.status");
    for (slot = 0u; slot < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++slot) {
        const yvex_backend_attention_weight *host = &job->weights[slot];
        if (!host->present) continue;
        rc = cuda_attention_allocate(
            &resources, &weight[slot], host->encoded_bytes, host->encoded, 0,
            "cuda.deepseek_attention.alloc.weight", failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    ALLOC_VALUE(input, job->hidden_width, float, job->input, 0,
                "cuda.deepseek_attention.alloc.input");
    ALLOC_VALUE(q_low, job->q_rank, float, NULL, 1,
                "cuda.deepseek_attention.alloc.q_low");
    ALLOC_VALUE(query, query_width, float, NULL, 1,
                "cuda.deepseek_attention.alloc.query");
    ALLOC_VALUE(raw_kv, job->kv_width, float, NULL, 1,
                "cuda.deepseek_attention.alloc.raw_kv");
    ALLOC_VALUE(sinks, job->query_heads, float, NULL, 1,
                "cuda.deepseek_attention.alloc.sinks");
    ALLOC_VALUE(attention, query_width, float, NULL, 1,
                "cuda.deepseek_attention.alloc.attention");
    ALLOC_VALUE(low, low_count, float, NULL, 1,
                "cuda.deepseek_attention.alloc.output_low");
    ALLOC_VALUE(final_output, job->hidden_width, float, NULL, 1,
                "cuda.deepseek_attention.alloc.output");
    ALLOC_VALUE(selected_count, 1ull, unsigned long long, NULL, 1,
                "cuda.deepseek_attention.alloc.selected_count");
    ALLOC_VALUE(valid_count, 1ull, unsigned long long, NULL, 1,
                "cuda.deepseek_attention.alloc.valid_count");

    if (job->local_count) {
        ALLOC_VALUE(history_local, local_extent,
                    float, job->local_kv, 0,
                    "cuda.deepseek_attention.alloc.local_history");
        ALLOC_VALUE(history_local_positions, job->local_count,
                    unsigned long long, job->local_positions, 0,
                    "cuda.deepseek_attention.alloc.local_positions");
    }
    if (job->compressed_count) {
        ALLOC_VALUE(history_compressed,
                    compressed_extent, float,
                    job->compressed_kv, 0,
                    "cuda.deepseek_attention.alloc.compressed_history");
        ALLOC_VALUE(history_compressed_positions, job->compressed_count,
                    unsigned long long, job->compressed_positions, 0,
                    "cuda.deepseek_attention.alloc.compressed_positions");
    }
    if (job->indexer_count) {
        ALLOC_VALUE(history_indexer, history_index_extent,
                    float, job->indexer_kv, 0,
                    "cuda.deepseek_attention.alloc.indexer_history");
        ALLOC_VALUE(history_indexer_positions, job->indexer_count,
                    unsigned long long, job->indexer_positions, 0,
                    "cuda.deepseek_attention.alloc.indexer_positions");
    }

    rc = cuda_attention_matvec(
        &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A], 0ull, job->q_rank, input,
        q_low, device_status, "cuda.deepseek_attention.q_a", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_weighted_norm(
        &resources, q_low, job->q_rank,
        &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM], job->rms_epsilon,
        device_status, "cuda.deepseek_attention.q_norm", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_matvec(
        &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_B],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_B], 0ull, query_width, q_low,
        query, device_status, "cuda.deepseek_attention.q_b", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_matvec(
        &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_KV],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_KV], 0ull, job->kv_width, input,
        raw_kv, device_status, "cuda.deepseek_attention.kv", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_weighted_norm(
        &resources, raw_kv, job->kv_width,
        &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM], job->rms_epsilon,
        device_status, "cuda.deepseek_attention.kv_norm", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_unit_norm(
        &resources, query, job->query_heads, job->head_dimension,
        job->rms_epsilon, device_status,
        "cuda.deepseek_attention.query_unit_norm", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_rope(
        &resources, query, job->query_heads, job->head_dimension,
        job->token_position, &job->position, 0, device_status,
        "cuda.deepseek_attention.query_rope", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    rc = cuda_attention_rope(
        &resources, raw_kv, 1ull, job->kv_width, job->token_position,
        &job->position, 0, device_status,
        "cuda.deepseek_attention.kv_rope", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    if (job->kv_width > job->position.rope_dimensions) {
        rc = cuda_attention_activation(
            &resources, raw_kv, 1ull,
            job->kv_width - job->position.rope_dimensions,
            &job->attention_kv_activation, device_status,
            "cuda.deepseek_attention.kv_activation", failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = cuda_attention_decode(
        &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_SINKS],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_SINKS], 0ull, job->query_heads,
        sinks, device_status, "cuda.deepseek_attention.sinks", failure, err);
    if (rc != YVEX_OK) goto cleanup;

    if (job->attention_class != 0u) {
        int emit;
        const yvex_backend_attention_rolling *rolling = &job->main_rolling;
        void *params[17];
        if (!rolling->present || !rolling->kv_state || !rolling->score_state)
            {
                rc = cuda_attention_fail(
                    failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                    "cuda.deepseek_attention.main_rolling", 1ull, 0ull, err,
                    YVEX_ERR_FORMAT,
                    "CUDA attention main rolling state is absent");
                goto cleanup;
            }
        main_extent = rolling->state_width * rolling->state_slots;
        ALLOC_VALUE(main_kv, rolling->state_width, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.main_kv");
        ALLOC_VALUE(main_score, rolling->state_width, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.main_score");
        ALLOC_VALUE(main_ape, rolling->state_width, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.main_ape");
        ALLOC_VALUE(main_before_kv, main_extent, float, rolling->kv_state, 0,
                    "cuda.deepseek_attention.alloc.main_before_kv");
        ALLOC_VALUE(main_before_score, main_extent, float,
                    rolling->score_state, 0,
                    "cuda.deepseek_attention.alloc.main_before_score");
        ALLOC_VALUE(main_after_kv, main_extent, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.main_after_kv");
        ALLOC_VALUE(main_after_score, main_extent, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.main_after_score");
        ALLOC_VALUE(compressed, rolling->head_dimension, float, NULL, 1,
                    "cuda.deepseek_attention.alloc.compressed");
        ALLOC_VALUE(compressed_positions, 1ull, unsigned long long, NULL, 1,
                    "cuda.deepseek_attention.alloc.current_compressed_position");
        rc = cuda_attention_matvec(
            &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV],
            weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV], 0ull,
            rolling->state_width, input, main_kv, device_status,
            "cuda.deepseek_attention.main_kv", failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = cuda_attention_matvec(
            &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE],
            weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE], 0ull,
            rolling->state_width, input, main_score, device_status,
            "cuda.deepseek_attention.main_gate", failure, err);
        if (rc != YVEX_OK) goto cleanup;
        rc = cuda_attention_decode(
            &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE],
            weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE],
            job->token_position % rolling->ratio, rolling->state_width,
            main_ape, device_status, "cuda.deepseek_attention.main_ape",
            failure, err);
        if (rc != YVEX_OK) goto cleanup;
        emit = ((job->token_position + 1ull) % rolling->ratio) == 0ull;
        if (emit) {
            emission_position = job->token_position + 1ull - rolling->ratio;
            current_compressed_count = 1ull;
            rc = yvex_cuda_status(
                &state->driver,
                state->driver.cuMemcpyHtoD_v2(
                    compressed_positions, &emission_position,
                    sizeof(emission_position)),
                "cuda.deepseek_attention.copy.compressed_position", err);
            if (rc != YVEX_OK) goto cleanup;
        }
        params[0] = &main_before_kv;
        params[1] = &main_before_score;
        params[2] = &main_kv;
        params[3] = &main_score;
        params[4] = &main_ape;
        params[5] = &main_after_kv;
        params[6] = &main_after_score;
        params[7] = &compressed;
        params[8] = (void *)&rolling->ratio;
        params[9] = (void *)&rolling->head_dimension;
        params[10] = (void *)&rolling->state_width;
        params[11] = (void *)&rolling->state_slots;
        params[12] = (void *)&rolling->cursor;
        params[13] = (void *)&rolling->overlap;
        params[14] = &emit;
        params[15] = &device_status;
        rc = cuda_attention_launch(
            &resources, state->deepseek_rolling_function, 1u,
            YVEX_CUDA_ATTN_BLOCK, 0u, params,
            "cuda.deepseek_attention.main_rolling", failure, err);
        if (rc != YVEX_OK) goto cleanup;
        if (emit) {
            rc = cuda_attention_weighted_norm(
                &resources, compressed, rolling->head_dimension,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM],
                job->rms_epsilon, device_status,
                "cuda.deepseek_attention.main_emission_norm", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_rope(
                &resources, compressed, 1ull, rolling->head_dimension,
                emission_position, &job->position, 0, device_status,
                "cuda.deepseek_attention.main_emission_rope", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (rolling->head_dimension > job->position.rope_dimensions) {
                rc = cuda_attention_activation(
                    &resources, compressed, 1ull,
                    rolling->head_dimension - job->position.rope_dimensions,
                    &job->compressor_activation, device_status,
                    "cuda.deepseek_attention.main_emission_activation",
                    failure, err);
                if (rc != YVEX_OK) goto cleanup;
            }
        }

        if (job->attention_class == 1u) {
            const yvex_backend_attention_rolling *index = &job->indexer_rolling;
            int index_emit;
            if (!index->present || !index->kv_state || !index->score_state) {
                rc = cuda_attention_fail(
                    failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                    "cuda.deepseek_attention.index_rolling", 1ull, 0ull,
                    err, YVEX_ERR_FORMAT,
                    "CUDA CSA indexer rolling state is absent");
                goto cleanup;
            }
            index_extent = index->state_width * index->state_slots;
            ALLOC_VALUE(index_kv, index->state_width, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_kv");
            ALLOC_VALUE(index_score, index->state_width, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_score");
            ALLOC_VALUE(index_ape, index->state_width, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_ape");
            ALLOC_VALUE(index_before_kv, index_extent, float, index->kv_state,
                        0, "cuda.deepseek_attention.alloc.index_before_kv");
            ALLOC_VALUE(index_before_score, index_extent, float,
                        index->score_state, 0,
                        "cuda.deepseek_attention.alloc.index_before_score");
            ALLOC_VALUE(index_after_kv, index_extent, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_after_kv");
            ALLOC_VALUE(index_after_score, index_extent, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_after_score");
            ALLOC_VALUE(indexer, index->head_dimension, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.current_indexer");
            ALLOC_VALUE(indexer_positions, 1ull, unsigned long long, NULL, 1,
                        "cuda.deepseek_attention.alloc.current_indexer_position");
            rc = cuda_attention_matvec(
                &resources,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV], 0ull,
                index->state_width, input, index_kv, device_status,
                "cuda.deepseek_attention.index_kv", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_matvec(
                &resources,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE], 0ull,
                index->state_width, input, index_score, device_status,
                "cuda.deepseek_attention.index_gate", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_decode(
                &resources,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE],
                job->token_position % index->ratio, index->state_width,
                index_ape, device_status,
                "cuda.deepseek_attention.index_ape", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            index_emit = ((job->token_position + 1ull) % index->ratio) == 0ull;
            if (index_emit) {
                current_indexer_count = 1ull;
                rc = yvex_cuda_status(
                    &state->driver,
                    state->driver.cuMemcpyHtoD_v2(
                        indexer_positions, &emission_position,
                        sizeof(emission_position)),
                    "cuda.deepseek_attention.copy.indexer_position", err);
                if (rc != YVEX_OK) goto cleanup;
            }
            params[0] = &index_before_kv;
            params[1] = &index_before_score;
            params[2] = &index_kv;
            params[3] = &index_score;
            params[4] = &index_ape;
            params[5] = &index_after_kv;
            params[6] = &index_after_score;
            params[7] = &indexer;
            params[8] = (void *)&index->ratio;
            params[9] = (void *)&index->head_dimension;
            params[10] = (void *)&index->state_width;
            params[11] = (void *)&index->state_slots;
            params[12] = (void *)&index->cursor;
            params[13] = (void *)&index->overlap;
            params[14] = &index_emit;
            params[15] = &device_status;
            rc = cuda_attention_launch(
                &resources, state->deepseek_rolling_function, 1u,
                YVEX_CUDA_ATTN_BLOCK, 0u, params,
                "cuda.deepseek_attention.index_rolling", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            if (index_emit) {
                rc = cuda_attention_weighted_norm(
                    &resources, indexer, index->head_dimension,
                    &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM],
                    weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM],
                    job->rms_epsilon, device_status,
                    "cuda.deepseek_attention.index_emission_norm", failure,
                    err);
                if (rc != YVEX_OK) goto cleanup;
                rc = cuda_attention_rope(
                    &resources, indexer, 1ull, index->head_dimension,
                    emission_position, &job->position, 0, device_status,
                    "cuda.deepseek_attention.index_emission_rope", failure,
                    err);
                if (rc != YVEX_OK) goto cleanup;
                rc = cuda_attention_activation(
                    &resources, indexer, 1ull, index->head_dimension,
                    &job->compressor_rotated_activation, device_status,
                    "cuda.deepseek_attention.index_emission_activation",
                    failure, err);
                if (rc != YVEX_OK) goto cleanup;
            }
            ALLOC_VALUE(index_query, index_query_extent, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_query");
            ALLOC_VALUE(index_weights, job->indexer_heads, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.index_weights");
            rc = cuda_attention_matvec(
                &resources,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q], 0ull,
                index_query_extent, q_low,
                index_query, device_status,
                "cuda.deepseek_attention.index_query", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_matvec(
                &resources,
                &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION],
                weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION], 0ull,
                job->indexer_heads, input, index_weights, device_status,
                "cuda.deepseek_attention.index_weights", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_rope(
                &resources, index_query, job->indexer_heads,
                job->indexer_head_dimension, job->token_position,
                &job->position, 0, device_status,
                "cuda.deepseek_attention.index_query_rope", failure, err);
            if (rc != YVEX_OK) goto cleanup;
            rc = cuda_attention_activation(
                &resources, index_query, job->indexer_heads,
                job->indexer_head_dimension, &job->indexer_query_activation,
                device_status,
                "cuda.deepseek_attention.index_query_activation", failure,
                err);
            if (rc != YVEX_OK) goto cleanup;
            if (!cuda_attention_checked_add(job->indexer_count,
                                            current_indexer_count,
                                            &candidate_capacity)) {
                rc = cuda_attention_fail(
                    failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
                    "cuda.deepseek_attention.topk.extent", ULLONG_MAX,
                    job->indexer_count, err, YVEX_ERR_BOUNDS,
                    "CUDA attention top-k candidate count overflowed");
                goto cleanup;
            }
            if (!candidate_capacity) candidate_capacity = 1ull;
            topk_capacity = job->indexer_topk < candidate_capacity
                ? job->indexer_topk : candidate_capacity;
            if (!topk_capacity) topk_capacity = 1ull;
            ALLOC_VALUE(selected, topk_capacity, unsigned long long, NULL, 1,
                        "cuda.deepseek_attention.alloc.selected");
            ALLOC_VALUE(selected_positions, topk_capacity,
                        unsigned long long, NULL, 1,
                        "cuda.deepseek_attention.alloc.selected_positions");
            ALLOC_VALUE(topk_scores, candidate_capacity, float, NULL, 1,
                        "cuda.deepseek_attention.alloc.topk_scores");
            ALLOC_VALUE(valid_indexes, candidate_capacity,
                        unsigned long long, NULL, 1,
                        "cuda.deepseek_attention.alloc.valid_indexes");
            {
                void *topk_params[21];
                topk_params[0] = &index_query;
                topk_params[1] = &index_weights;
                topk_params[2] = &history_indexer;
                topk_params[3] = &history_indexer_positions;
                topk_params[4] = (void *)&job->indexer_count;
                topk_params[5] = (void *)&job->indexer_stride;
                topk_params[6] = &indexer;
                topk_params[7] = &indexer_positions;
                topk_params[8] = &current_indexer_count;
                topk_params[9] = (void *)&job->indexer_head_dimension;
                topk_params[10] = (void *)&job->indexer_heads;
                topk_params[11] = (void *)&job->indexer_head_dimension;
                topk_params[12] = (void *)&job->compression_ratio;
                topk_params[13] = (void *)&job->token_position;
                topk_params[14] = (void *)&job->indexer_topk;
                topk_params[15] = &selected;
                topk_params[16] = &selected_positions;
                topk_params[17] = &selected_count;
                topk_params[18] = &valid_count;
                topk_params[19] = &topk_scores;
                topk_params[20] = &valid_indexes;
                /* status is appended by replacing the final scratch parameter
                 * in the fixed kernel ABI below. */
                {
                    void *params_with_status[22];
                    memcpy(params_with_status, topk_params,
                           sizeof(topk_params));
                    params_with_status[21] = &device_status;
                    rc = cuda_attention_launch(
                        &resources, state->deepseek_topk_function, 1u, 1u,
                        0u, params_with_status,
                        "cuda.deepseek_attention.topk", failure, err);
                }
                if (rc != YVEX_OK) goto cleanup;
            }
        }
    }

    {
        void *params[27];
        params[0] = &query;
        params[1] = &history_local;
        params[2] = &history_local_positions;
        params[3] = (void *)&job->local_count;
        params[4] = (void *)&job->local_stride;
        params[5] = &raw_kv;
        params[6] = (void *)&job->kv_width;
        params[7] = &history_compressed;
        params[8] = &history_compressed_positions;
        params[9] = (void *)&job->compressed_count;
        params[10] = (void *)&job->compressed_stride;
        params[11] = &compressed;
        params[12] = &compressed_positions;
        params[13] = &current_compressed_count;
        params[14] = (void *)&job->head_dimension;
        params[15] = &selected;
        params[16] = &selected_count;
        params[17] = &sinks;
        params[18] = (void *)&job->query_heads;
        params[19] = (void *)&job->head_dimension;
        params[20] = (void *)&job->sliding_window;
        params[21] = (void *)&job->compression_ratio;
        params[22] = (void *)&job->attention_class;
        params[23] = (void *)&job->token_position;
        params[24] = &attention;
        params[25] = &device_status;
        rc = cuda_attention_launch(
            &resources, state->deepseek_reduce_function,
            (unsigned int)job->query_heads, YVEX_CUDA_ATTN_BLOCK,
            YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params,
            "cuda.deepseek_attention.reduce", failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = cuda_attention_rope(
        &resources, attention, job->query_heads, job->head_dimension,
        job->token_position, &job->position, 1, device_status,
        "cuda.deepseek_attention.output_inverse_rope", failure, err);
    if (rc != YVEX_OK) goto cleanup;
    for (unsigned long long group = 0ull; group < job->output_groups; ++group) {
        CUdeviceptr group_input =
            attention + group * job->output_group_input_width * sizeof(float);
        CUdeviceptr group_output = low + group * job->output_rank * sizeof(float);
        rc = cuda_attention_matvec(
            &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A],
            weight[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A],
            group * job->output_rank, job->output_rank, group_input,
            group_output, device_status,
            "cuda.deepseek_attention.output_a", failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    rc = cuda_attention_matvec(
        &resources, &job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B],
        weight[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B], 0ull, job->hidden_width,
        low, final_output, device_status,
        "cuda.deepseek_attention.output_b", failure, err);
    if (rc != YVEX_OK) goto cleanup;

    rc = yvex_cuda_synchronize(
        backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.synchronize", err);
    if (rc != YVEX_OK) {
        rc = cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE,
            "cuda.deepseek_attention.synchronize", 1ull, 0ull, err,
            (yvex_status)rc,
            "CUDA attention final synchronization failed");
        goto cleanup;
    }
    rc = yvex_cuda_status(
        &state->driver,
        state->driver.cuMemcpyDtoH_v2(&host_status, device_status,
                                      sizeof(host_status)),
        "cuda.deepseek_attention.copy.status", err);
    if (rc != YVEX_OK) goto cleanup;
    if (host_status != 0) {
        rc = cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
            "cuda.deepseek_attention.numeric", 0ull,
            (unsigned long long)host_status, err, YVEX_ERR_FORMAT,
            "CUDA attention device numerical stage refused its input");
        goto cleanup;
    }
    copy_failure = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    if (copy_failure && strcmp(copy_failure, "copy-output") == 0) {
        rc = cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY,
            "cuda.deepseek_attention.copy.output", job->hidden_width, 0ull,
            err, YVEX_ERR_BACKEND,
            "injected CUDA attention output copy failure");
        goto cleanup;
    }
#define COPY_OUT(host, device, count, type, stage) do {                       \
        if ((host) && (device) && (count)) {                                  \
            if (!cuda_attention_checked_bytes((count), sizeof(type), &bytes)) {\
                rc = YVEX_ERR_BOUNDS; goto cleanup;                            \
            }                                                                 \
            rc = yvex_cuda_status(&state->driver,                             \
                state->driver.cuMemcpyDtoH_v2((host), (device), bytes), stage, \
                err);                                                         \
            if (rc != YVEX_OK) goto cleanup;                                  \
        }                                                                     \
    } while (0)
    COPY_OUT(output->q_low, q_low, job->q_rank, float,
             "cuda.deepseek_attention.copy.q_low");
    COPY_OUT(output->query, query, query_width, float,
             "cuda.deepseek_attention.copy.query");
    COPY_OUT(output->raw_kv, raw_kv, job->kv_width, float,
             "cuda.deepseek_attention.copy.raw_kv");
    COPY_OUT(output->attention_values, attention, query_width, float,
             "cuda.deepseek_attention.copy.attention");
    COPY_OUT(output->output, final_output, job->hidden_width, float,
             "cuda.deepseek_attention.copy.output");
    if (current_compressed_count) {
        COPY_OUT(output->compressed_kv, compressed, job->head_dimension,
                 float, "cuda.deepseek_attention.copy.compressed");
        output->compressed_positions[0] = emission_position;
    }
    if (current_indexer_count) {
        COPY_OUT(output->indexer_kv, indexer, job->indexer_head_dimension,
                 float, "cuda.deepseek_attention.copy.indexer");
        output->indexer_positions[0] = emission_position;
    }
    if (job->attention_class == 1u) {
        COPY_OUT(output->index_query, index_query,
                 index_query_extent, float,
                 "cuda.deepseek_attention.copy.index_query");
        COPY_OUT(output->index_weights, index_weights, job->indexer_heads,
                 float, "cuda.deepseek_attention.copy.index_weights");
        COPY_OUT(&committed.topk_count, selected_count, 1ull,
                 unsigned long long,
                 "cuda.deepseek_attention.copy.selected_count");
        COPY_OUT(&committed.valid_candidate_count, valid_count, 1ull,
                 unsigned long long,
                 "cuda.deepseek_attention.copy.valid_count");
        if (committed.topk_count > topk_capacity) {
            rc = cuda_attention_fail(
                failure, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
                "cuda.deepseek_attention.copy.topk", topk_capacity,
                committed.topk_count, err, YVEX_ERR_BOUNDS,
                "CUDA attention top-k output exceeded its allocation");
            goto cleanup;
        }
        COPY_OUT(output->topk_positions, selected_positions,
                 committed.topk_count, unsigned long long,
                 "cuda.deepseek_attention.copy.topk_positions");
    }
    if (main_extent) {
        COPY_OUT(output->main_kv_state, main_after_kv, main_extent, float,
                 "cuda.deepseek_attention.copy.main_kv_state");
        COPY_OUT(output->main_score_state, main_after_score, main_extent,
                 float, "cuda.deepseek_attention.copy.main_score_state");
    }
    if (index_extent) {
        COPY_OUT(output->indexer_kv_state, index_after_kv, index_extent,
                 float, "cuda.deepseek_attention.copy.index_kv_state");
        COPY_OUT(output->indexer_score_state, index_after_score, index_extent,
                 float, "cuda.deepseek_attention.copy.index_score_state");
    }
#undef COPY_OUT
    committed.compressed_count = current_compressed_count;
    committed.indexer_count = current_indexer_count;
    committed.device_bytes = resources.current_bytes;
    committed.peak_device_bytes = resources.peak_bytes;
    committed.kernel_launches = resources.launches;
    committed.q_low = output->q_low;
    committed.query = output->query;
    committed.raw_kv = output->raw_kv;
    committed.compressed_kv = output->compressed_kv;
    committed.indexer_kv = output->indexer_kv;
    committed.index_query = output->index_query;
    committed.index_weights = output->index_weights;
    committed.attention_values = output->attention_values;
    committed.output = output->output;
    committed.compressed_positions = output->compressed_positions;
    committed.indexer_positions = output->indexer_positions;
    committed.topk_positions = output->topk_positions;
    committed.main_kv_state = output->main_kv_state;
    committed.main_score_state = output->main_score_state;
    committed.indexer_kv_state = output->indexer_kv_state;
    committed.indexer_score_state = output->indexer_score_state;
    rc = YVEX_OK;

cleanup:
    cleanup_rc = cuda_attention_cleanup(&resources, err);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK) {
        rc = cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_CLEANUP,
            "cuda.deepseek_attention.cleanup", 0ull,
            resources.current_bytes, err, (yvex_status)cleanup_rc,
            "CUDA attention temporary cleanup failed");
    }
    if (rc == YVEX_OK) {
        *output = committed;
        if (failure) memset(failure, 0, sizeof(*failure));
        yvex_error_clear(err);
    }
#undef ALLOC_VALUE
    return rc;
}
