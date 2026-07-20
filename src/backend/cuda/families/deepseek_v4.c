/* Owner: src/backend/cuda
 * Owns: bounded Driver-API allocation, transfer, and launch lifecycle for encoded DeepSeek attention.
 * Does not own: artifact IO, logical tensor lookup, architecture construction, graph identity, persistent KV,
 *   independent reference arithmetic, CLI output, transformer orchestration, or generation.
 * Invariants: encoded tensors remain encoded on device; every numerical stage is a generated-PTX kernel; host
 *   outputs remain unchanged until all device work and status checks complete; every temporary is
 *   released on every path.
 * Boundary: device-complete attention for one stateful token is not a decode loop.
 * Purpose: execute admitted encoded DeepSeek attention stages through generated CUDA kernels.
 * Inputs: an admitted CUDA backend, immutable one-token job, and caller-owned output views.
 * Effects: allocates temporary device ranges, transfers bounded data, launches, synchronizes, copies.
 * Failure: typed validation, budget, allocation, copy, launch, numeric, synchronization, and cleanup. */
#include <yvex/backend.h>
#include <yvex/qtype.h>
#include <yvex/quant.h>
#include "src/backend/cuda/private.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define YVEX_CUDA_ATTN_MAX_ALLOCATIONS 96u
#define YVEX_CUDA_ATTN_BLOCK 256u
#define YVEX_CUDA_ATTN_CSA_RATIO 4ull
#define YVEX_CUDA_ATTN_HCA_RATIO 128ull
#define YVEX_CUDA_ATTN_TRANSFERS 16u
enum { CUDA_ROLLING_MAIN = 0, CUDA_ROLLING_INDEX, CUDA_ROLLING_COUNT };
typedef struct {
    CUdeviceptr kv, score, ape;
    CUdeviceptr before_kv, before_score;
    CUdeviceptr after_kv, after_score;
    CUdeviceptr value, positions;
    unsigned long long extent, value_count, value_extent;
} cuda_attention_rolling_run;
typedef struct {
    yvex_backend *backend;
    yvex_cuda_backend_state *state;
    CUdeviceptr pointers[YVEX_CUDA_ATTN_MAX_ALLOCATIONS];
    unsigned long long sizes[YVEX_CUDA_ATTN_MAX_ALLOCATIONS];
    unsigned int count;
    unsigned long long current_bytes, peak_bytes, budget, launches;
} cuda_attention_resources;
typedef struct {
    CUdeviceptr *device;
    const void *host_source;
    void *output, *staged;
    unsigned long long capacity, *used;
    size_t width;
    const char *stage;
} cuda_attention_transfer;
typedef enum {
    CUDA_DIM_ONE = 0, CUDA_DIM_HIDDEN, CUDA_DIM_Q_RANK, CUDA_DIM_QUERY_WIDTH,
    CUDA_DIM_KV_WIDTH, CUDA_DIM_QUERY_HEADS,
    CUDA_DIM_LOW_COUNT, CUDA_DIM_OUTPUT_GROUP_INPUT,
    CUDA_DIM_MAIN_STATE_WIDTH, CUDA_DIM_MAIN_RATIO, CUDA_DIM_HEAD,
    CUDA_DIM_INDEX_STATE_WIDTH, CUDA_DIM_INDEX_RATIO, CUDA_DIM_INDEX_HEAD,
    CUDA_DIM_INDEX_QUERY, CUDA_DIM_INDEX_HEADS
} cuda_attention_dimension;
typedef struct {
    yvex_backend_attention_weight_slot slot;
    cuda_attention_dimension rows, width;
    unsigned int class_mask;
} cuda_attention_weight_shape;
static const cuda_attention_weight_shape cuda_attention_weight_shapes[] = {
    {YVEX_BACKEND_ATTENTION_WEIGHT_Q_A, CUDA_DIM_Q_RANK, CUDA_DIM_HIDDEN, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM, CUDA_DIM_ONE, CUDA_DIM_Q_RANK, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_Q_B, CUDA_DIM_QUERY_WIDTH, CUDA_DIM_Q_RANK, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_KV, CUDA_DIM_KV_WIDTH, CUDA_DIM_HIDDEN, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM, CUDA_DIM_ONE, CUDA_DIM_KV_WIDTH, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_SINKS, CUDA_DIM_ONE, CUDA_DIM_QUERY_HEADS, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A, CUDA_DIM_LOW_COUNT, CUDA_DIM_OUTPUT_GROUP_INPUT, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B, CUDA_DIM_HIDDEN, CUDA_DIM_LOW_COUNT, 7u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV, CUDA_DIM_MAIN_STATE_WIDTH, CUDA_DIM_HIDDEN, 6u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE, CUDA_DIM_MAIN_STATE_WIDTH, CUDA_DIM_HIDDEN, 6u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE, CUDA_DIM_MAIN_RATIO, CUDA_DIM_MAIN_STATE_WIDTH, 6u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM, CUDA_DIM_ONE, CUDA_DIM_HEAD, 6u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV, CUDA_DIM_INDEX_STATE_WIDTH, CUDA_DIM_HIDDEN, 2u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE, CUDA_DIM_INDEX_STATE_WIDTH, CUDA_DIM_HIDDEN, 2u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE, CUDA_DIM_INDEX_RATIO, CUDA_DIM_INDEX_STATE_WIDTH, 2u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM, CUDA_DIM_ONE, CUDA_DIM_INDEX_HEAD, 2u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q, CUDA_DIM_INDEX_QUERY, CUDA_DIM_Q_RANK, 2u},
    {YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION, CUDA_DIM_INDEX_HEADS, CUDA_DIM_HIDDEN, 2u}
};
/* Purpose: record one typed refusal.
 * Inputs: optional storage, code, stage, and evidence.
 * Effects: resets detail and writes the error.
 * Failure: returns the supplied status.
 * Boundary: records failure only; it does not release device ownership. */
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
/* Purpose: derive host bytes.
 * Inputs: count, width, and size output.
 * Effects: writes representable bytes.
 * Failure: false on invalid input or overflow.
 * Boundary: pure allocation arithmetic. */
static int cuda_attention_checked_bytes(unsigned long long count,
                                        unsigned long long width,
                                        size_t *out)
{
    if (!out || width == 0ull || count > (unsigned long long)SIZE_MAX / width)
        return 0;
    *out = (size_t)(count * width);
    return 1;
}
/* Purpose: acquire one device range.
 * Inputs: owner, size, source/zero policy, and stage.
 * Effects: allocates, tracks, and initializes.
 * Failure: typed budget/allocation/copy refusal.
 * Boundary: one temporary device range within the attention transaction. */
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
/* Purpose: release tracked device ranges.
 * Inputs: owner and error storage.
 * Effects: frees in reverse order and clears accounting.
 * Failure: first cleanup error.
 * Boundary: idempotent transaction cleanup; it does not free caller output storage. */
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
/* Purpose: launch one admitted kernel.
 * Inputs: owner, function, geometry, parameters, stage.
 * Effects: enqueues work and counts it.
 * Failure: typed injected or Driver refusal.
 * Boundary: launch only; synchronization is owned by final transaction completion. */
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
/* Purpose: launch encoded matvec.
 * Inputs: owner, weight slice, vector, output, status.
 * Effects: enqueues direct-encoded compute.
 * Failure: typed geometry or launch refusal.
 * Boundary: device compute only; no host decode fallback. */
static int cuda_attention_matvec(cuda_attention_resources *resources,
                                 const yvex_backend_attention_weight *weight,
                                 CUdeviceptr device_weight,
                                 unsigned long long start_row,
                                 unsigned long long rows,
                                 CUdeviceptr vector,
                                 CUdeviceptr out,
                                 int output_bf16,
                                 CUdeviceptr status,
                                 const char *stage,
                                 yvex_backend_attention_failure *failure,
                                 yvex_error *err)
{
    if (!weight || !weight->present || !device_weight || !vector || !out ||
        !rows || start_row > weight->row_count ||
        rows > weight->row_count - start_row || rows > UINT_MAX)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            weight ? weight->row_count : 0ull, start_row + rows, err,
            YVEX_ERR_BOUNDS, "CUDA attention matvec geometry is invalid");
    {
        void *params[] = {
            &device_weight, (void *)&weight->row_bytes,
            (void *)&weight->row_width, &start_row, &rows,
            (void *)&weight->qtype, &vector, &out, &output_bf16, &status
        };
        return cuda_attention_launch(
            resources, resources->state->deepseek_qtype_matvec_function,
            (unsigned int)rows, YVEX_CUDA_ATTN_BLOCK,
            YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
            failure, err);
    }
}
/* Purpose: decode one encoded row.
 * Inputs: owner, weight, row/count, output, status.
 * Effects: enqueues bounded decode.
 * Failure: typed row geometry or launch refusal.
 * Boundary: device reference primitive; no host output publication. */
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
    {
        void *params[] = {
            &encoded, &count, (void *)&weight->qtype, &out, &status
        };
        return cuda_attention_launch(
            resources, resources->state->deepseek_decode_function, grid,
            YVEX_CUDA_ATTN_BLOCK, 0u, params, stage, failure, err);
    }
}
/* Purpose: apply learned RMS norm.
 * Inputs: values, weight, count, epsilon, status.
 * Effects: enqueues in-place normalization.
 * Failure: typed shape or launch refusal.
 * Boundary: one numerical stage within the transaction. */
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
    if (!weight || !weight->present || weight->row_count != 1ull ||
        weight->row_width != count)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            count, weight ? weight->row_width : 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention normalization weight shape is invalid");
    {
        void *params[] = {
            &values, &count, &device_weight, (void *)&weight->qtype,
            &epsilon, &status
        };
        return cuda_attention_launch(
            resources, resources->state->deepseek_weighted_norm_function, 1u,
            YVEX_CUDA_ATTN_BLOCK,
            YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
            failure, err);
    }
}
/* Purpose: unit-normalize device vectors.
 * Inputs: values, geometry, epsilon, status.
 * Effects: enqueues in-place normalization.
 * Failure: typed geometry or launch refusal.
 * Boundary: device numerical primitive only. */
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
    if (!vectors || vectors > UINT_MAX || !width)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            1ull, vectors, err, YVEX_ERR_BOUNDS,
            "CUDA attention unit-norm geometry is invalid");
    {
        void *params[] = {&values, &vectors, &width, &epsilon, &status};
        return cuda_attention_launch(
            resources, resources->state->deepseek_unit_norm_function,
            (unsigned int)vectors, YVEX_CUDA_ATTN_BLOCK,
            YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params, stage,
            failure, err);
    }
}
/* Purpose: apply/invert RoPE/YaRN.
 * Inputs: vectors, geometry, position policy, status.
 * Effects: enqueues in-place rotation.
 * Failure: typed geometry or launch refusal.
 * Boundary: device position primitive; policy comes from the immutable job. */
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
    {
        void *params[] = {
            &values, &vectors, &width, (void *)&position->rope_dimensions,
            &token_position, (void *)&position->theta,
            (void *)&position->scaling_factor,
            (void *)&position->original_context, (void *)&position->beta_fast,
            (void *)&position->beta_slow, &inverse, &status
        };
        return cuda_attention_launch(
            resources, resources->state->deepseek_rope_function, grid,
            YVEX_CUDA_ATTN_BLOCK, 0u, params, stage, failure, err);
    }
}
/* Purpose: apply activation fake quantization.
 * Inputs: vectors, policy, status, stage.
 * Effects: conditionally enqueues in-place work.
 * Failure: typed geometry/launch refusal.
 * Boundary: no-op when policy says activation quantization is not required. */
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
    if (!policy || !policy->required) return YVEX_OK;
    if (!vectors || vectors > UINT_MAX || !width || !policy->block_width ||
        width % policy->block_width)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            policy->block_width, width, err, YVEX_ERR_BOUNDS,
            "CUDA attention activation geometry is invalid");
    {
        void *params[] = {
            &values, &vectors, &width, (void *)&policy->block_width,
            (void *)&policy->quantization, (void *)&policy->hadamard, &status
        };
        return cuda_attention_launch(
            resources, resources->state->deepseek_activation_function,
            (unsigned int)vectors, 1u, 0u, params, stage, failure, err);
    }
}
/* Purpose: validate base host geometry.
 * Inputs: job, output views, failure storage.
 * Effects: reads only.
 * Failure: missing/inconsistent weights, histories, or class facts.
 * Boundary: admission only; no allocation, transfer, or launch. */
static int cuda_attention_validate_job(
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    if (!job || !output || !job->input || !job->hidden_width || !job->q_rank ||
        !job->query_heads || !job->head_dimension || !job->kv_width ||
        !job->max_device_bytes ||
        job->query_heads > ULLONG_MAX / job->head_dimension ||
        job->query_heads * job->head_dimension > (unsigned long long)SIZE_MAX /
            sizeof(float) ||
        (job->cancellation && !job->cancellation->requested))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate", 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "CUDA attention job and output geometry are required");
    if (job->local_count &&
        (!job->local_kv || !job->local_positions ||
         job->local_stride != job->head_dimension))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.local_history",
            job->head_dimension, job->local_stride, err, YVEX_ERR_FORMAT,
            "CUDA attention local history is incomplete");
    if (job->compressed_count &&
        (!job->compressed_kv || !job->compressed_positions ||
         job->compressed_stride != job->head_dimension))
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.compressed_history",
            job->head_dimension, job->compressed_stride, err,
            YVEX_ERR_FORMAT,
            "CUDA attention compressed history is incomplete");
    if (job->compute_contract !=
        YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.compute_contract",
            YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
            job->compute_contract, err, YVEX_ERR_UNSUPPORTED,
            "CUDA attention compute contract is unavailable");
    if (job->attention_class < YVEX_BACKEND_ATTENTION_SWA ||
        job->attention_class > YVEX_BACKEND_ATTENTION_HCA)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.class",
            YVEX_BACKEND_ATTENTION_HCA, job->attention_class, err,
            YVEX_ERR_FORMAT, "CUDA attention class is not admitted");
    if (job->attention_class == YVEX_BACKEND_ATTENTION_CSA &&
        job->indexer_count != job->compressed_count)
        return cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.index_history",
            job->compressed_count, job->indexer_count, err, YVEX_ERR_FORMAT,
            "CUDA CSA indexer/compressed history cardinality differs");
    return YVEX_OK;
}
typedef struct {
    cuda_attention_resources resources;
    yvex_backend *backend;
    yvex_cuda_backend_state *state;
    const yvex_backend_attention_job *job;
    yvex_backend_attention_output *output;
    yvex_backend_attention_failure *failure;
    yvex_error *err;
    unsigned char *host_stage;
    size_t host_stage_bytes;
    cuda_attention_transfer transfers[YVEX_CUDA_ATTN_TRANSFERS];
    size_t transfer_count;
    CUdeviceptr weight[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    CUdeviceptr input, q_low, query, raw_kv;
    CUdeviceptr sinks, attention, low, final_output;
    cuda_attention_rolling_run rolling[CUDA_ROLLING_COUNT];
    CUdeviceptr index_query;
    CUdeviceptr index_weights;
    CUdeviceptr history_local;
    CUdeviceptr history_local_positions;
    CUdeviceptr history_compressed;
    CUdeviceptr history_compressed_positions;
    CUdeviceptr history_indexer;
    CUdeviceptr history_indexer_positions;
    CUdeviceptr selected;
    CUdeviceptr selected_positions;
    CUdeviceptr selected_count;
    CUdeviceptr valid_count;
    CUdeviceptr topk_scores;
    CUdeviceptr valid_indexes;
    CUdeviceptr device_status;
    unsigned long long query_width;
    unsigned long long candidate_capacity;
    unsigned long long topk_capacity;
    unsigned long long low_count;
    unsigned long long local_extent;
    unsigned long long compressed_extent;
    unsigned long long history_index_extent;
    unsigned long long index_query_extent;
    unsigned long long emission_position;
    unsigned long long staged_topk_count;
    unsigned long long staged_valid_count;
    int host_status;
} cuda_attention_run;
static int cuda_attention_stage_layout(cuda_attention_run *run,
                                       unsigned char *base,
                                       size_t *total);
/* Purpose: record a run-scoped typed refusal without repeating transaction plumbing.
 * Inputs: run, failure classification, stage, evidence, status, and stable message.
 * Effects: writes only the run's failure and error views.
 * Failure: returns the supplied status.
 * Boundary: failure projection only; it performs no cleanup or publication. */
static int cuda_attention_run_fail(
    cuda_attention_run *run,
    yvex_backend_attention_failure_code code,
    const char *stage,
    unsigned long long expected,
    unsigned long long actual,
    yvex_status status,
    const char *message)
{
    return cuda_attention_fail(
        run->failure, code, stage, expected, actual, run->err, status, message);
}
/* Purpose: refuse cancellation safely.
 * Inputs: run, stage, pending-work fact.
 * Effects: may synchronize pending work.
 * Failure: typed cancel/synchronize error.
 * Boundary: Never publishes caller-visible output. */
static int cuda_attention_cancel(cuda_attention_run *run,
                                 const char *stage,
                                 int device_work_pending)
{
    int rc;
    if (!run->job->cancellation ||
        !run->job->cancellation->requested(run->job->cancellation->context))
        return YVEX_OK;
    if (device_work_pending) {
        rc = yvex_cuda_synchronize(
            run->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, stage,
            run->err);
        if (rc != YVEX_OK)
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE, stage, 1ull,
                0ull, (yvex_status)rc,
                "CUDA attention cancellation synchronization failed");
    }
    return cuda_attention_run_fail(
        run, YVEX_BACKEND_ATTENTION_FAILURE_CANCELLED, stage, 0ull, 1ull,
        YVEX_ERR_CANCELLED,
        "CUDA attention execution was cancelled before publication");
}
/* Purpose: validate encoded weight.
 * Inputs: run, slot, rows, width.
 * Effects: reads only.
 * Failure: typed shape, qtype, or byte refusal.
 * Boundary: Admits canonical qtype geometry only. */
static int cuda_attention_weight_validate(
    cuda_attention_run *run,
    yvex_backend_attention_weight_slot slot,
    unsigned long long rows,
    unsigned long long width)
{
    const yvex_backend_attention_weight *weight = &run->job->weights[slot];
    const yvex_quant_numeric_capability *capability;
    unsigned long long row_bytes = 0ull;
    unsigned long long total_bytes = 0ull;
    const char *reason = NULL;
    if (!weight->present || !weight->encoded || weight->row_count != rows ||
        weight->row_width != width)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.weight_shape", rows,
            weight->row_count, YVEX_ERR_FORMAT,
            "CUDA attention encoded weight shape is invalid");
    capability = yvex_quant_numeric_capability_at(weight->qtype);
    if (!capability || !capability->dedicated_cuda_compute_available ||
        !yvex_gguf_qtype_storage_bytes(weight->qtype, width, &row_bytes,
                                       &reason) ||
        row_bytes != weight->row_bytes ||
        !yvex_core_u64_mul(rows, row_bytes, &total_bytes) ||
        total_bytes != (unsigned long long)weight->encoded_bytes)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.deepseek_attention.validate.weight_encoding", row_bytes,
            weight->row_bytes, YVEX_ERR_UNSUPPORTED,
            reason ? reason :
                "CUDA attention encoded weight capability is unavailable");
    return YVEX_OK;
}
/* Purpose: validate rolling state.
 * Inputs: run, state, expected geometry, extent output.
 * Effects: derives checked extent.
 * Failure: typed geometry refusal.
 * Boundary: Admits caller-owned state without mutation. */
static int cuda_attention_rolling_validate(
    cuda_attention_run *run,
    const yvex_backend_attention_rolling *rolling,
    unsigned long long ratio,
    unsigned long long head_dimension,
    int overlap,
    unsigned long long *extent,
    const char *stage)
{
    unsigned long long factor = overlap ? 2ull : 1ull;
    unsigned long long state_width;
    unsigned long long state_slots;
    if (!rolling || !extent || !yvex_core_u64_mul(head_dimension, factor,
                                                   &state_width) ||
        !yvex_core_u64_mul(ratio, factor, &state_slots) ||
        !yvex_core_u64_mul(state_width, state_slots, extent) ||
        !rolling->present || rolling->next_token_position !=
            run->job->token_position || rolling->ratio != ratio ||
        rolling->head_dimension != head_dimension ||
        rolling->state_width != state_width ||
        rolling->state_slots != state_slots || rolling->overlap != overlap ||
        rolling->cursor != run->job->token_position % ratio ||
        rolling->current_fill != run->job->token_position % ratio ||
        rolling->previous_fill !=
            (overlap && run->job->token_position >= ratio ? ratio : 0ull) ||
        !rolling->kv_state || rolling->kv_state_capacity < *extent ||
        !rolling->score_state || rolling->score_state_capacity < *extent)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            ratio, rolling ? rolling->ratio : 0ull, YVEX_ERR_FORMAT,
            "CUDA attention rolling-state geometry is invalid");
    return YVEX_OK;
}
/* Purpose: validate history ordering.
 * Inputs: immutable histories and positions.
 * Effects: reads only.
 * Failure: typed ordering/window/group refusal.
 * Boundary: Performs no numerical work. */
static int cuda_attention_history_validate(cuda_attention_run *run)
{
    unsigned long long required_local;
    unsigned long long required_compressed;
    unsigned long long first_local;
    unsigned long long i;
#define ORDERED(values, count, stage) do {                                     \
        for (i = 0ull; i < (count); ++i) {                                     \
            if ((values)[i] >= run->job->token_position ||                     \
                (i && (values)[i - 1ull] >= (values)[i]))                       \
                return cuda_attention_run_fail(                                \
                    run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,      \
                    (stage), run->job->token_position, (values)[i],             \
                    YVEX_ERR_FORMAT,                                            \
                    "CUDA attention history positions are invalid");          \
        }                                                                       \
    } while (0)
    if (run->job->local_count)
        ORDERED(run->job->local_positions, run->job->local_count,
                "cuda.deepseek_attention.validate.local_positions");
    if (run->job->compressed_count)
        ORDERED(run->job->compressed_positions, run->job->compressed_count,
                "cuda.deepseek_attention.validate.compressed_positions");
    if (run->job->indexer_count)
        ORDERED(run->job->indexer_positions, run->job->indexer_count,
                "cuda.deepseek_attention.validate.index_positions");
#undef ORDERED
    required_local = run->job->token_position < run->job->sliding_window - 1ull
        ? run->job->token_position : run->job->sliding_window - 1ull;
    first_local = run->job->token_position - required_local;
    if (run->job->local_count != required_local)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.local_window",
            required_local, run->job->local_count, YVEX_ERR_FORMAT,
            "CUDA attention local history is incomplete");
    for (i = 0ull; i < required_local; ++i)
        if (run->job->local_positions[i] != first_local + i)
            return cuda_attention_run_fail(
                run,
                YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                "cuda.deepseek_attention.validate.local_suffix",
                first_local + i, run->job->local_positions[i], YVEX_ERR_FORMAT,
                "CUDA attention local history is not the complete suffix");
    required_compressed = run->job->attention_class ==
        YVEX_BACKEND_ATTENTION_SWA ? 0ull :
        run->job->token_position / run->job->compression_ratio;
    if (run->job->compressed_count != required_compressed ||
        (run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA &&
         run->job->indexer_count != required_compressed) ||
        (run->job->attention_class != YVEX_BACKEND_ATTENTION_CSA &&
         run->job->indexer_count != 0ull))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.compressed_coverage",
            required_compressed, run->job->compressed_count, YVEX_ERR_FORMAT,
            "CUDA attention compressed history is incomplete");
    for (i = 0ull; i < run->job->compressed_count; ++i)
        if (run->job->compressed_positions[i] !=
            i * run->job->compression_ratio)
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                "cuda.deepseek_attention.validate.compressed_position",
                i * run->job->compression_ratio,
                run->job->compressed_positions[i], YVEX_ERR_FORMAT,
                "CUDA attention compressed history has a missing group");
    if (run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA) {
        for (i = 0ull; i < run->job->indexer_count; ++i) {
            if (run->job->indexer_positions[i] !=
                run->job->compressed_positions[i])
                return cuda_attention_run_fail(
                    run,
                    YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                    "cuda.deepseek_attention.validate.index_alignment",
                    run->job->compressed_positions[i],
                    run->job->indexer_positions[i], YVEX_ERR_FORMAT,
                    "CUDA CSA compressed/indexer positions differ");
        }
    }
    return YVEX_OK;
}
typedef struct {
    uintptr_t first;
    uintptr_t last;
} cuda_attention_host_range;
typedef struct {
    const void *data;
    unsigned long long count;
    size_t width;
} cuda_attention_host_input;
/* Purpose: register one caller-visible transactional output.
 * Inputs: run, device/host source, output span, extent, optional used count, and type width.
 * Effects: appends one canonical descriptor.
 * Failure: typed capacity, range, or budget refusal.
 * Boundary: registration only; it performs no allocation, copy, or publication. */
static int cuda_attention_transfer_add(
    cuda_attention_run *run,
    CUdeviceptr *device,
    const void *host_source,
    void *output,
    unsigned long long output_capacity,
    unsigned long long capacity,
    unsigned long long *used,
    size_t width,
    const char *stage)
{
    size_t bytes;
    cuda_attention_transfer *transfer;
    if (!capacity) return YVEX_OK;
    if (run->transfer_count >= YVEX_CUDA_ATTN_TRANSFERS ||
        !output || output_capacity < capacity ||
        !cuda_attention_checked_bytes(capacity, width, &bytes) ||
        (uintptr_t)output > UINTPTR_MAX - bytes)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.output", capacity,
            output_capacity, YVEX_ERR_BOUNDS,
            "CUDA attention output span is absent, invalid, or undersized");
    transfer = &run->transfers[run->transfer_count++];
    *transfer = (cuda_attention_transfer){
        device, host_source, output, NULL, capacity, used, width, stage
    };
    return YVEX_OK;
}
/* Purpose: describe every caller-visible output once.
 * Inputs: run with class-dependent extents already derived.
 * Effects: installs the reusable transfer table.
 * Failure: first typed span or budget refusal.
 * Boundary: no caller output or device state is mutated. */
static int cuda_attention_transfer_plan(cuda_attention_run *run)
{
    const int csa = run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA;
    int rc;
#define ADD(device, source, member, count, used, type, label) do {             \
        rc = cuda_attention_transfer_add(                                      \
            run, (device), (source), run->output->member.data,                 \
            run->output->member.capacity, (count), (used), sizeof(type),       \
            "cuda.deepseek_attention.copy." label);                           \
        if (rc != YVEX_OK) return rc;                                           \
    } while (0)
    ADD(&run->q_low, NULL, q_low, run->job->q_rank, NULL, float, "q_low");
    ADD(&run->query, NULL, query, run->query_width, NULL, float, "query");
    ADD(&run->raw_kv, NULL, raw_kv, run->job->kv_width, NULL, float, "raw_kv");
    ADD(&run->attention, NULL, attention_values, run->query_width, NULL, float,
        "attention");
    ADD(&run->final_output, NULL, output, run->job->hidden_width, NULL, float,
        "output");
    ADD(&run->rolling[CUDA_ROLLING_MAIN].value, NULL, compressed_kv,
        run->rolling[CUDA_ROLLING_MAIN].value_extent, NULL, float, "compressed");
    ADD(NULL, &run->emission_position, compressed_positions,
        run->rolling[CUDA_ROLLING_MAIN].value_count, NULL, unsigned long long,
        "compressed_positions");
    ADD(&run->rolling[CUDA_ROLLING_INDEX].value, NULL, indexer_kv,
        run->rolling[CUDA_ROLLING_INDEX].value_extent, NULL, float, "indexer");
    ADD(NULL, &run->emission_position, indexer_positions,
        run->rolling[CUDA_ROLLING_INDEX].value_count, NULL, unsigned long long,
        "indexer_positions");
    ADD(&run->index_query, NULL, index_query, csa ? run->index_query_extent : 0ull,
        NULL, float, "index_query");
    ADD(&run->index_weights, NULL, index_weights,
        csa ? run->job->indexer_heads : 0ull, NULL, float, "index_weights");
    ADD(&run->selected_positions, NULL, topk_positions,
        csa ? run->topk_capacity : 0ull, &run->staged_topk_count,
        unsigned long long, "topk_positions");
    ADD(&run->rolling[CUDA_ROLLING_MAIN].after_kv, NULL, main_kv_state,
        run->rolling[CUDA_ROLLING_MAIN].extent, NULL, float, "main_kv_state");
    ADD(&run->rolling[CUDA_ROLLING_MAIN].after_score, NULL, main_score_state,
        run->rolling[CUDA_ROLLING_MAIN].extent, NULL, float, "main_score_state");
    ADD(&run->rolling[CUDA_ROLLING_INDEX].after_kv, NULL, indexer_kv_state,
        run->rolling[CUDA_ROLLING_INDEX].extent, NULL, float, "index_kv_state");
    ADD(&run->rolling[CUDA_ROLLING_INDEX].after_score, NULL, indexer_score_state,
        run->rolling[CUDA_ROLLING_INDEX].extent, NULL, float,
        "index_score_state");
#undef ADD
    return YVEX_OK;
}
/* Purpose: append one checked host range.
 * Inputs: range table, used count, data, element count, and element width.
 * Effects: appends a half-open range. Failure: false for missing/non-representable input.
 * Boundary: pure host-address validation. */
static int cuda_attention_host_range_add(cuda_attention_host_range *ranges,
                                         size_t *used,
                                         const void *data,
                                         unsigned long long count,
                                         size_t width)
{
    size_t bytes;
    if (!count) return 1;
    if (!data || !cuda_attention_checked_bytes(count, width, &bytes) ||
        (uintptr_t)data > UINTPTR_MAX - bytes)
        return 0;
    ranges[*used].first = (uintptr_t)data;
    ranges[(*used)++].last = (uintptr_t)data + bytes;
    return 1;
}
/* Purpose: refuse unsafe host aliasing.
 * Inputs: derived extents and output spans.
 * Effects: reads only.
 * Failure: invalid range or any non-empty overlap.
 * Boundary: Host alias admission; no device or caller mutation. */
static int cuda_attention_alias_validate(cuda_attention_run *run)
{
    cuda_attention_host_range writes[16];
    cuda_attention_host_range reads[32];
    const cuda_attention_host_input inputs[] = {
        {run->job->input, run->job->hidden_width, sizeof(float)},
        {run->job->local_kv, run->local_extent, sizeof(float)},
        {run->job->local_positions, run->job->local_count, sizeof(unsigned long long)},
        {run->job->compressed_kv, run->compressed_extent, sizeof(float)},
        {run->job->compressed_positions, run->job->compressed_count,
         sizeof(unsigned long long)},
        {run->job->indexer_kv, run->history_index_extent, sizeof(float)},
        {run->job->indexer_positions, run->job->indexer_count,
         sizeof(unsigned long long)},
        {run->job->main_rolling.kv_state,
         run->rolling[CUDA_ROLLING_MAIN].extent, sizeof(float)},
        {run->job->main_rolling.score_state,
         run->rolling[CUDA_ROLLING_MAIN].extent, sizeof(float)},
        {run->job->indexer_rolling.kv_state,
         run->rolling[CUDA_ROLLING_INDEX].extent, sizeof(float)},
        {run->job->indexer_rolling.score_state,
         run->rolling[CUDA_ROLLING_INDEX].extent, sizeof(float)}
    };
    size_t write_count = 0u;
    size_t read_count = 0u;
    size_t i;
    size_t j;
    for (i = 0u; i < run->transfer_count; ++i)
        if (!cuda_attention_host_range_add(
                writes, &write_count, run->transfers[i].output,
                run->transfers[i].capacity, run->transfers[i].width))
            goto invalid_range;
    for (i = 0u; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
        if (!cuda_attention_host_range_add(
                reads, &read_count, inputs[i].data, inputs[i].count,
                inputs[i].width))
            goto invalid_range;
    for (i = 0u; i < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++i)
        if (!cuda_attention_host_range_add(
                reads, &read_count, run->job->weights[i].encoded,
                run->job->weights[i].present ?
                    run->job->weights[i].encoded_bytes : 0u,
                sizeof(unsigned char)))
            goto invalid_range;
    for (i = 0u; i < write_count; ++i) {
        for (j = i + 1u; j < write_count; ++j)
            if (writes[i].first < writes[j].last &&
                writes[j].first < writes[i].last)
                goto overlap;
        for (j = 0u; j < read_count; ++j)
            if (writes[i].first < reads[j].last &&
                reads[j].first < writes[i].last)
                goto overlap;
    }
    return YVEX_OK;
invalid_range:
    return cuda_attention_run_fail(
        run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
        "cuda.deepseek_attention.validate.alias.range", 1ull, 0ull,
        YVEX_ERR_BOUNDS, "CUDA attention host range is not representable");
overlap:
    return cuda_attention_run_fail(
        run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
        "cuda.deepseek_attention.validate.alias", 0ull, 1ull,
        YVEX_ERR_INVALID_ARG,
        "CUDA attention output aliases another output or immutable input");
}
/* Purpose: validate one runtime activation policy.
 * Inputs: immutable policy, exact vector width, and refusal storage.
 * Effects: reads policy only.
 * Failure: returns a typed incompatibility refusal.
 * Boundary: pre-dispatch numerical-contract admission. */
static int cuda_attention_activation_validate(
    cuda_attention_run *run,
    const yvex_backend_attention_activation *policy,
    unsigned long long width,
    const char *stage)
{
    if (!policy->required) return YVEX_OK;
    if (!width || !policy->block_width || width % policy->block_width ||
        (policy->quantization != 1u && policy->quantization != 2u) ||
        (policy->hadamard &&
         ((width & (width - 1ull)) != 0ull || width > 1024ull)))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT, stage,
            policy->block_width, width, YVEX_ERR_FORMAT,
            "CUDA attention activation policy and width are incompatible");
    return YVEX_OK;
}
/* Purpose: project one declarative weight-shape dimension from admitted runtime facts.
 * Inputs: immutable run and closed dimension selector.
 * Effects: none. Failure: invalid selectors return zero for later shape refusal.
 * Boundary: weight validation geometry only. */
static unsigned long long cuda_attention_dimension_value(
    const cuda_attention_run *run,
    cuda_attention_dimension dimension)
{
    const unsigned long long values[] = {
        1ull, run->job->hidden_width, run->job->q_rank, run->query_width, run->job->kv_width,
        run->job->query_heads, run->low_count, run->job->output_group_input_width,
        run->job->main_rolling.state_width, run->job->main_rolling.ratio, run->job->head_dimension,
        run->job->indexer_rolling.state_width, run->job->indexer_rolling.ratio,
        run->job->indexer_rolling.head_dimension, run->index_query_extent, run->job->indexer_heads
    };
    return (unsigned int)dimension < sizeof(values) / sizeof(values[0]) ? values[dimension] : 0ull;
}
/* Purpose: enforce derived admission.
 * Inputs: run with checked common extents.
 * Effects: derives class extents.
 * Failure: typed interdependent-contract refusal.
 * Boundary: Completes validation before dispatch. */
static int cuda_attention_validate_derived(cuda_attention_run *run)
{
    unsigned long long ratio = run->job->compression_ratio;
    unsigned long long candidate_count = 0ull;
    unsigned long long group_width = 0ull;
    size_t i;
    int rc;
    if (run->job->token_position == ULLONG_MAX)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.position", ULLONG_MAX - 1ull,
            run->job->token_position, YVEX_ERR_BOUNDS,
            "CUDA attention token position cannot advance safely");
    if (run->job->kv_width != run->job->head_dimension ||
        !run->job->sliding_window || !run->job->output_groups ||
        !run->job->output_group_input_width || !run->job->output_rank ||
        !yvex_core_u64_mul(run->job->output_groups,
                           run->job->output_group_input_width, &group_width) ||
        group_width != run->query_width || run->job->rms_epsilon <= 0.0 ||
        run->job->position.theta <= 1ull ||
        !run->job->position.scaling_factor ||
        (run->job->attention_class == YVEX_BACKEND_ATTENTION_SWA
             ? run->job->position.original_context != 0ull
             : run->job->position.original_context == 0ull) ||
        !run->job->position.beta_slow ||
        run->job->position.beta_fast <= run->job->position.beta_slow ||
        !run->job->position.rope_dimensions ||
        (run->job->position.rope_dimensions & 1ull) ||
        run->job->position.rope_dimensions > run->job->head_dimension ||
        run->job->local_count > run->job->sliding_window)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.geometry", run->query_width,
            group_width, YVEX_ERR_FORMAT,
            "CUDA attention interdependent geometry is invalid");
    if ((run->job->attention_class == YVEX_BACKEND_ATTENTION_SWA && ratio != 0ull) ||
        (run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA &&
         ratio != YVEX_CUDA_ATTN_CSA_RATIO) ||
        (run->job->attention_class == YVEX_BACKEND_ATTENTION_HCA &&
         ratio != YVEX_CUDA_ATTN_HCA_RATIO))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.ratio",
            run->job->attention_class == YVEX_BACKEND_ATTENTION_HCA
                ? YVEX_CUDA_ATTN_HCA_RATIO
                : run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA
                    ? YVEX_CUDA_ATTN_CSA_RATIO : 0ull,
            ratio, YVEX_ERR_FORMAT,
            "CUDA attention class/compression ratio is invalid");
    if (run->job->attention_class != YVEX_BACKEND_ATTENTION_SWA) {
        rc = cuda_attention_rolling_validate(
            run, &run->job->main_rolling, ratio, run->job->head_dimension,
            run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA,
            &run->rolling[CUDA_ROLLING_MAIN].extent,
            "cuda.deepseek_attention.validate.main_state");
        if (rc != YVEX_OK) return rc;
        run->rolling[CUDA_ROLLING_MAIN].value_count =
            ((run->job->token_position + 1ull) % ratio) == 0ull ? 1ull : 0ull;
        if (!yvex_core_u64_mul(run->rolling[CUDA_ROLLING_MAIN].value_count,
                               run->job->head_dimension,
                               &run->rolling[CUDA_ROLLING_MAIN].value_extent))
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
                "cuda.deepseek_attention.validate.compressed_extent",
                ULLONG_MAX, run->job->head_dimension, YVEX_ERR_BOUNDS,
                "CUDA attention compressed output extent overflowed");
    } else if (run->job->main_rolling.present || run->job->indexer_rolling.present ||
               run->job->compressed_count || run->job->indexer_count) {
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.swa_state", 0ull, 1ull,
            YVEX_ERR_FORMAT,
            "CUDA SWA cannot consume compressed rolling state");
    }
    if (run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA) {
        if (!run->job->indexer_heads || !run->job->indexer_head_dimension ||
            !run->job->indexer_topk ||
            run->job->position.rope_dimensions >
                run->job->indexer_head_dimension ||
            (run->job->compressed_count &&
             run->job->compressed_stride != run->job->head_dimension) ||
            (run->job->indexer_count &&
             run->job->indexer_stride != run->job->indexer_head_dimension))
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                "cuda.deepseek_attention.validate.index_geometry",
                run->job->head_dimension, run->job->compressed_stride,
                YVEX_ERR_FORMAT,
                "CUDA CSA indexer and compressed geometry is invalid");
        rc = cuda_attention_rolling_validate(
            run, &run->job->indexer_rolling, ratio,
            run->job->indexer_head_dimension, 1,
            &run->rolling[CUDA_ROLLING_INDEX].extent,
            "cuda.deepseek_attention.validate.index_state");
        if (rc != YVEX_OK) return rc;
        run->rolling[CUDA_ROLLING_INDEX].value_count =
            run->rolling[CUDA_ROLLING_MAIN].value_count;
        if (!yvex_core_u64_mul(run->rolling[CUDA_ROLLING_INDEX].value_count,
                               run->job->indexer_head_dimension,
                               &run->rolling[CUDA_ROLLING_INDEX].value_extent))
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
                "cuda.deepseek_attention.validate.indexer_extent",
                ULLONG_MAX, run->job->indexer_head_dimension, YVEX_ERR_BOUNDS,
                "CUDA attention indexer output extent overflowed");
        if (!yvex_core_u64_add(run->job->indexer_count,
                               run->rolling[CUDA_ROLLING_INDEX].value_count,
                               &candidate_count))
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
                "cuda.deepseek_attention.validate.candidates", ULLONG_MAX,
                run->job->indexer_count, YVEX_ERR_BOUNDS,
                "CUDA attention candidate count overflowed");
        run->candidate_capacity = candidate_count ? candidate_count : 1ull;
        run->topk_capacity = run->job->indexer_topk < candidate_count
            ? run->job->indexer_topk : candidate_count;
        if (!run->topk_capacity) run->topk_capacity = 1ull;
    } else if (run->job->indexer_rolling.present || run->job->indexer_count) {
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.validate.index_state", 0ull, 1ull,
            YVEX_ERR_FORMAT,
            "CUDA non-CSA attention cannot consume indexer state");
    }
    for (i = 0u;
         i < sizeof(cuda_attention_weight_shapes) /
                 sizeof(cuda_attention_weight_shapes[0]);
         ++i) {
        const cuda_attention_weight_shape *shape = &cuda_attention_weight_shapes[i];
        if (!(shape->class_mask & (1u << run->job->attention_class))) continue;
        rc = cuda_attention_weight_validate(
            run, shape->slot, cuda_attention_dimension_value(run, shape->rows),
            cuda_attention_dimension_value(run, shape->width));
        if (rc != YVEX_OK) return rc;
    }
    rc = cuda_attention_activation_validate(
        run, &run->job->attention_kv_activation,
        run->job->kv_width - run->job->position.rope_dimensions,
        "cuda.deepseek_attention.validate.kv_activation");
    if (rc == YVEX_OK && run->job->attention_class != YVEX_BACKEND_ATTENTION_SWA)
        rc = cuda_attention_activation_validate(
            run, &run->job->compressor_activation,
            run->job->head_dimension - run->job->position.rope_dimensions,
            "cuda.deepseek_attention.validate.compressor_activation");
    if (rc == YVEX_OK && run->job->attention_class != YVEX_BACKEND_ATTENTION_SWA)
        rc = cuda_attention_activation_validate(
            run, &run->job->compressor_rotated_activation,
            run->job->indexer_head_dimension,
            "cuda.deepseek_attention.validate.compressor_rotated_activation");
    if (rc == YVEX_OK && run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA)
        rc = cuda_attention_activation_validate(
            run, &run->job->indexer_query_activation,
            run->job->indexer_head_dimension,
            "cuda.deepseek_attention.validate.indexer_activation");
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_history_validate(run);
    if (rc == YVEX_OK) rc = cuda_attention_transfer_plan(run);
    if (rc == YVEX_OK) rc = cuda_attention_alias_validate(run);
    return rc;
}
/* Purpose: allocate typed device values.
 * Inputs: run, count/width, source/zero policy.
 * Effects: delegates to the resource owner.
 * Failure: typed size/allocation/copy refusal.
 * Boundary: one temporary range; cleanup remains transaction-owned. */
static int cuda_attention_alloc_values(cuda_attention_run *run,
                                       CUdeviceptr *target,
                                       unsigned long long count,
                                       size_t width,
                                       const void *source,
                                       int zero,
                                       const char *stage)
{
    size_t bytes;
    if (!cuda_attention_checked_bytes(count, (unsigned long long)width, &bytes))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage, ULLONG_MAX,
            count, YVEX_ERR_BOUNDS,
            "CUDA attention allocation size overflowed");
    return cuda_attention_allocate(&run->resources, target, bytes, source, zero,
                                   stage, run->failure, run->err);
}
typedef struct {
    CUdeviceptr *target;
    unsigned long long count;
    size_t width;
    const void *source;
    int zero;
    const char *stage;
} cuda_attention_allocation;
/* Purpose: prepare an execution.
 * Inputs: initialized run, immutable job, output views.
 * Effects: derives extents and admits context.
 * Failure: typed geometry/capability refusal.
 * Boundary: no device allocation or numerical execution. */
static int cuda_attention_prepare(cuda_attention_run *run)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    int rc;
    if (!run->backend || !run->state)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.execute", 1ull, 0ull,
            YVEX_ERR_INVALID_ARG, "CUDA attention backend is required");
    rc = cuda_attention_validate_job(run->job, run->output, run->failure,
                                     run->err);
    if (rc != YVEX_OK) return rc;
    {
        struct {
            unsigned long long left, right, *result;
        } extents[] = {
            {run->job->query_heads, run->job->head_dimension, &run->query_width},
            {run->job->output_groups, run->job->output_rank, &run->low_count},
            {run->job->local_count, run->job->local_stride, &run->local_extent},
            {run->job->compressed_count, run->job->compressed_stride,
             &run->compressed_extent},
            {run->job->indexer_count, run->job->indexer_stride,
             &run->history_index_extent},
            {run->job->indexer_heads, run->job->indexer_head_dimension,
             &run->index_query_extent}
        };
        size_t i;
        for (i = 0u; i < sizeof(extents) / sizeof(extents[0]); ++i)
            if (!yvex_core_u64_mul(
                    extents[i].left, extents[i].right, extents[i].result))
                return cuda_attention_run_fail(
                    run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
                    "cuda.deepseek_attention.validate.extent", ULLONG_MAX, 0ull,
                    YVEX_ERR_BOUNDS,
                    "CUDA attention logical extent overflowed");
    }
    rc = cuda_attention_validate_derived(run);
    if (rc != YVEX_OK) return rc;
    if (!cuda_attention_stage_layout(run, NULL, &run->host_stage_bytes) ||
        !run->host_stage_bytes ||
        run->host_stage_bytes > run->job->max_host_bytes)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
            "cuda.deepseek_attention.validate.host_budget",
            run->job->max_host_bytes, run->host_stage_bytes, YVEX_ERR_NOMEM,
            "CUDA attention host staging exceeds its explicit budget");
    rc = cuda_attention_cancel(
        run, "cuda.deepseek_attention.cancel.before_dispatch", 0);
    if (rc != YVEX_OK) return rc;
    rc = yvex_cuda_require_capability(
        run->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.capability", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.deepseek_attention.capability", 1ull, 0ull,
            (yvex_status)rc,
            "CUDA DeepSeek attention kernel bundle is not admitted");
    rc = injected && strcmp(injected, "context") == 0
        ? YVEX_ERR_BACKEND
        : yvex_cuda_set_current(
              run->backend, "cuda.deepseek_attention.context", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.deepseek_attention.context", 1ull, 0ull,
            (yvex_status)rc, "CUDA attention context activation failed");
    run->resources.backend = run->backend;
    run->resources.state = run->state;
    run->resources.budget = run->job->max_device_bytes;
    return YVEX_OK;
}
/* Purpose: acquire base device ranges.
 * Inputs: admitted run and checked extents.
 * Effects: allocates/copies under one owner.
 * Failure: first typed resource refusal.
 * Boundary: allocation phase only; no numerical kernels launch. */
static int cuda_attention_allocate_base(cuda_attention_run *run)
{
    cuda_attention_allocation values[] = {
        {&run->device_status, 1ull, sizeof(int), NULL, 1,
         "cuda.deepseek_attention.alloc.status"},
        {&run->input, run->job->hidden_width, sizeof(float), run->job->input, 0,
         "cuda.deepseek_attention.alloc.input"},
        {&run->q_low, run->job->q_rank, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.q_low"},
        {&run->query, run->query_width, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.query"},
        {&run->raw_kv, run->job->kv_width, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.raw_kv"},
        {&run->sinks, run->job->query_heads, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.sinks"},
        {&run->attention, run->query_width, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.attention"},
        {&run->low, run->low_count, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.output_low"},
        {&run->final_output, run->job->hidden_width, sizeof(float), NULL, 1,
         "cuda.deepseek_attention.alloc.output"},
        {&run->selected_count, 1ull, sizeof(unsigned long long), NULL, 1,
         "cuda.deepseek_attention.alloc.selected_count"},
        {&run->valid_count, 1ull, sizeof(unsigned long long), NULL, 1,
         "cuda.deepseek_attention.alloc.valid_count"},
        {&run->history_local, run->local_extent, sizeof(float), run->job->local_kv,
         0, "cuda.deepseek_attention.alloc.local_history"},
        {&run->history_local_positions, run->job->local_count,
         sizeof(unsigned long long), run->job->local_positions, 0,
         "cuda.deepseek_attention.alloc.local_positions"},
        {&run->history_compressed, run->compressed_extent, sizeof(float),
         run->job->compressed_kv, 0,
         "cuda.deepseek_attention.alloc.compressed_history"},
        {&run->history_compressed_positions, run->job->compressed_count,
         sizeof(unsigned long long), run->job->compressed_positions, 0,
         "cuda.deepseek_attention.alloc.compressed_positions"},
        {&run->history_indexer, run->history_index_extent, sizeof(float),
         run->job->indexer_kv, 0,
         "cuda.deepseek_attention.alloc.indexer_history"},
        {&run->history_indexer_positions, run->job->indexer_count,
         sizeof(unsigned long long), run->job->indexer_positions, 0,
         "cuda.deepseek_attention.alloc.indexer_positions"}
    };
    size_t i;
    unsigned int slot;
    int rc = cuda_attention_alloc_values(
        run, values[0].target, values[0].count, values[0].width,
        values[0].source, values[0].zero, values[0].stage);
    if (rc != YVEX_OK) return rc;
    for (slot = 0u; slot < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++slot) {
        const yvex_backend_attention_weight *host = &run->job->weights[slot];
        if (!host->present) continue;
        rc = cuda_attention_allocate(
            &run->resources, &run->weight[slot], host->encoded_bytes,
            host->encoded, 0, "cuda.deepseek_attention.alloc.weight",
            run->failure, run->err);
        if (rc != YVEX_OK) return rc;
    }
    for (i = 1u; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (!values[i].count) continue;
        rc = cuda_attention_alloc_values(
            run, values[i].target, values[i].count, values[i].width,
            values[i].source, values[i].zero, values[i].stage);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}
/* Purpose: produce current Q/KV and sinks.
 * Inputs: allocated run, weights, policy.
 * Effects: enqueues projection/norm/RoPE/activation.
 * Failure: first typed stage error.
 * Boundary: current-token projection only; no history reduction or host copy. */
static int cuda_attention_project(cuda_attention_run *run)
{
    int rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A], 0ull,
        run->job->q_rank, run->input, run->q_low, 1, run->device_status,
        "cuda.deepseek_attention.q_a", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_weighted_norm(
        &run->resources, run->q_low, run->job->q_rank,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM],
        run->job->rms_epsilon, run->device_status,
        "cuda.deepseek_attention.q_norm", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_B],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_B], 0ull,
        run->query_width, run->q_low, run->query, 1, run->device_status,
        "cuda.deepseek_attention.q_b", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_KV],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_KV], 0ull,
        run->job->kv_width, run->input, run->raw_kv, 1, run->device_status,
        "cuda.deepseek_attention.kv", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_weighted_norm(
        &run->resources, run->raw_kv, run->job->kv_width,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM],
        run->job->rms_epsilon, run->device_status,
        "cuda.deepseek_attention.kv_norm", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_unit_norm(
        &run->resources, run->query, run->job->query_heads,
        run->job->head_dimension, run->job->rms_epsilon, run->device_status,
        "cuda.deepseek_attention.query_unit_norm", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->query, run->job->query_heads,
        run->job->head_dimension, run->job->token_position,
        &run->job->position, 0, run->device_status,
        "cuda.deepseek_attention.query_rope", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->raw_kv, 1ull, run->job->kv_width,
        run->job->token_position, &run->job->position, 0,
        run->device_status, "cuda.deepseek_attention.kv_rope",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    if (run->job->kv_width > run->job->position.rope_dimensions) {
        rc = cuda_attention_activation(
            &run->resources, run->raw_kv, 1ull,
            run->job->kv_width - run->job->position.rope_dimensions,
            &run->job->attention_kv_activation, run->device_status,
            "cuda.deepseek_attention.kv_activation", run->failure, run->err);
        if (rc != YVEX_OK) return rc;
    }
    return cuda_attention_decode(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_SINKS],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_SINKS], 0ull,
        run->job->query_heads, run->sinks, run->device_status,
        "cuda.deepseek_attention.sinks", run->failure, run->err);
}
/* Purpose: execute rolling compression.
 * Inputs: allocated run and state selector.
 * Effects: enqueues recurrence pipeline.
 * Failure: typed geometry/resource/stage error.
 * Boundary: Updates private candidate state only; caller state remains unpublished. */
static int cuda_attention_rolling_execute(cuda_attention_run *run, unsigned int kind)
{
    const int index = kind == CUDA_ROLLING_INDEX;
    const yvex_backend_attention_rolling *rolling = index
        ? &run->job->indexer_rolling : &run->job->main_rolling;
    cuda_attention_rolling_run *device = &run->rolling[kind];
    yvex_backend_attention_weight_slot base = index
        ? YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV
        : YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV;
    const yvex_backend_attention_activation *activation = index
        ? &run->job->compressor_rotated_activation
        : &run->job->compressor_activation;
    const char *stage = index ? "cuda.deepseek_attention.index_rolling"
                              : "cuda.deepseek_attention.main_rolling";
    cuda_attention_allocation rows[] = {
        {&device->kv, rolling->state_width, sizeof(float), NULL, 1, stage},
        {&device->score, rolling->state_width, sizeof(float), NULL, 1, stage},
        {&device->ape, rolling->state_width, sizeof(float), NULL, 1, stage},
        {&device->before_kv, device->extent, sizeof(float), rolling->kv_state, 0, stage},
        {&device->before_score, device->extent, sizeof(float), rolling->score_state, 0, stage},
        {&device->after_kv, device->extent, sizeof(float), NULL, 1, stage},
        {&device->after_score, device->extent, sizeof(float), NULL, 1, stage},
        {&device->value, rolling->head_dimension, sizeof(float), NULL, 1, stage},
        {&device->positions, 1ull, sizeof(unsigned long long), NULL, 1, stage}
    };
    unsigned long long activation_width = rolling->head_dimension;
    size_t row;
    int emit;
    int rc;
    for (row = 0u; row < sizeof(rows) / sizeof(rows[0]); ++row) {
        rc = cuda_attention_alloc_values(
            run, rows[row].target, rows[row].count, rows[row].width,
            rows[row].source, rows[row].zero, rows[row].stage);
        if (rc != YVEX_OK) return rc;
    }
    rc = cuda_attention_matvec(
        &run->resources, &run->job->weights[base], run->weight[base], 0ull,
        rolling->state_width, run->input, device->kv, 0, run->device_status,
        stage, run->failure, run->err);
    if (rc == YVEX_OK)
        rc = cuda_attention_matvec(
            &run->resources, &run->job->weights[base + 1],
            run->weight[base + 1], 0ull, rolling->state_width, run->input,
            device->score, 0, run->device_status, stage, run->failure,
            run->err);
    if (rc == YVEX_OK)
        rc = cuda_attention_decode(
            &run->resources, &run->job->weights[base + 2],
            run->weight[base + 2], run->job->token_position % rolling->ratio,
            rolling->state_width, device->ape, run->device_status, stage,
            run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    emit = device->value_count != 0ull;
    if (!index && emit)
        run->emission_position = run->job->token_position + 1ull - rolling->ratio;
    if (emit) {
        rc = yvex_cuda_status(
            &run->state->driver,
            run->state->driver.cuMemcpyHtoD_v2(
                device->positions, &run->emission_position,
                sizeof(run->emission_position)), stage, run->err);
        if (rc != YVEX_OK)
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, 1ull, 0ull,
                (yvex_status)rc,
                "CUDA attention rolling-position copy failed");
    }
    {
        void *params[] = {
            &device->before_kv, &device->before_score, &device->kv,
            &device->score, &device->ape, &device->after_kv,
            &device->after_score, &device->value, (void *)&rolling->ratio,
            (void *)&rolling->head_dimension, (void *)&rolling->state_width,
            (void *)&rolling->state_slots, (void *)&rolling->cursor,
            (void *)&rolling->overlap, &emit, &run->device_status
        };
        rc = cuda_attention_launch(
            &run->resources, run->state->deepseek_rolling_function, 1u,
            YVEX_CUDA_ATTN_BLOCK, 0u, params, stage, run->failure, run->err);
    }
    if (rc != YVEX_OK || !emit) return rc;
    rc = cuda_attention_weighted_norm(
        &run->resources, device->value, rolling->head_dimension,
        &run->job->weights[base + 3], run->weight[base + 3],
        run->job->rms_epsilon, run->device_status, stage, run->failure,
        run->err);
    if (rc == YVEX_OK)
        rc = cuda_attention_rope(
            &run->resources, device->value, 1ull, rolling->head_dimension,
            run->emission_position, &run->job->position, 0,
            run->device_status, stage, run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    if (!index) {
        if (activation_width <= run->job->position.rope_dimensions)
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
                stage, run->job->position.rope_dimensions + 1ull,
                activation_width, YVEX_ERR_FORMAT,
                "CUDA main compressor requires non-RoPE dimensions");
        activation_width -= run->job->position.rope_dimensions;
    }
    return cuda_attention_activation(
        &run->resources, device->value, 1ull, activation_width, activation,
        run->device_status, stage, run->failure, run->err);
}
/* Purpose: execute CSA selection.
 * Inputs: indexed run, history, and policy.
 * Effects: allocates scratch and enqueues top-k.
 * Failure: typed geometry/resource/stage error.
 * Boundary: selection only; selected values enter the later reduction kernel. */
static int cuda_attention_index_topk(cuda_attention_run *run)
{
    int rc;
#define ALLOC(target, count, type, source, zero, stage) do {                  \
        rc = cuda_attention_alloc_values(run, &(target), (count),             \
                                         sizeof(type), (source), (zero),       \
                                         (stage));                             \
        if (rc != YVEX_OK) return rc;                                         \
    } while (0)
    ALLOC(run->index_query, run->index_query_extent, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_query");
    ALLOC(run->index_weights, run->job->indexer_heads, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_weights");
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q], 0ull,
        run->index_query_extent, run->q_low, run->index_query,
        1, run->device_status, "cuda.deepseek_attention.index_query",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION], 0ull,
        run->job->indexer_heads, run->input, run->index_weights,
        1, run->device_status, "cuda.deepseek_attention.index_weights",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->index_query, run->job->indexer_heads,
        run->job->indexer_head_dimension, run->job->token_position,
        &run->job->position, 0, run->device_status,
        "cuda.deepseek_attention.index_query_rope",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_activation(
        &run->resources, run->index_query, run->job->indexer_heads,
        run->job->indexer_head_dimension, &run->job->indexer_query_activation,
        run->device_status, "cuda.deepseek_attention.index_query_activation",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    ALLOC(run->selected, run->topk_capacity, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.selected");
    ALLOC(run->selected_positions, run->topk_capacity, unsigned long long,
          NULL, 1, "cuda.deepseek_attention.alloc.selected_positions");
    ALLOC(run->topk_scores, run->candidate_capacity, float, NULL, 1,
          "cuda.deepseek_attention.alloc.topk_scores");
    ALLOC(run->valid_indexes, run->candidate_capacity, unsigned long long,
          NULL, 1, "cuda.deepseek_attention.alloc.valid_indexes");
#undef ALLOC
    {
        void *params[] = {
            &run->index_query, &run->index_weights, &run->history_indexer,
            &run->history_indexer_positions, (void *)&run->job->indexer_count,
            (void *)&run->job->indexer_stride,
            &run->rolling[CUDA_ROLLING_INDEX].value,
            &run->rolling[CUDA_ROLLING_INDEX].positions,
            &run->rolling[CUDA_ROLLING_INDEX].value_count,
            (void *)&run->job->indexer_head_dimension,
            (void *)&run->job->indexer_heads,
            (void *)&run->job->indexer_head_dimension,
            (void *)&run->job->compression_ratio,
            (void *)&run->job->token_position, (void *)&run->job->indexer_topk,
            &run->selected, &run->selected_positions, &run->selected_count,
            &run->valid_count, &run->topk_scores, &run->valid_indexes,
            &run->device_status
        };
        return cuda_attention_launch(
            &run->resources, run->state->deepseek_topk_function, 1u, 1u, 0u,
            params, "cuda.deepseek_attention.topk", run->failure, run->err);
    }
}
/* Purpose: compose class compression.
 * Inputs: run after current projection.
 * Effects: enqueues rolling/top-k. Failure: first typed stage refusal.
 * Boundary: class dispatch only; no CPU fallback or output publication. */
static int cuda_attention_compress(cuda_attention_run *run)
{
    int rc;
    if (run->job->attention_class == YVEX_BACKEND_ATTENTION_SWA) return YVEX_OK;
    rc = cuda_attention_rolling_execute(run, CUDA_ROLLING_MAIN);
    if (rc != YVEX_OK ||
        run->job->attention_class != YVEX_BACKEND_ATTENTION_CSA) return rc;
    rc = cuda_attention_rolling_execute(run, CUDA_ROLLING_INDEX);
    return rc == YVEX_OK ? cuda_attention_index_topk(run) : rc;
}
/* Purpose: reduce and project output.
 * Inputs: projected run, histories, selection.
 * Effects: enqueues reduction/OUT_A/OUT_B.
 * Failure: typed geometry/launch refusal.
 * Boundary: final device numerical stage; host output remains uncommitted. */
static int cuda_attention_reduce(cuda_attention_run *run)
{
    unsigned long long group;
    unsigned int attention_class = (unsigned int)run->job->attention_class;
    int rc;
    {
        void *params[] = {
            &run->query, &run->history_local, &run->history_local_positions,
            (void *)&run->job->local_count, (void *)&run->job->local_stride,
            &run->raw_kv, (void *)&run->job->kv_width,
            &run->history_compressed, &run->history_compressed_positions,
            (void *)&run->job->compressed_count,
            (void *)&run->job->compressed_stride,
            &run->rolling[CUDA_ROLLING_MAIN].value,
            &run->rolling[CUDA_ROLLING_MAIN].positions,
            &run->rolling[CUDA_ROLLING_MAIN].value_count,
            (void *)&run->job->head_dimension, &run->selected,
            &run->selected_count, &run->sinks,
            (void *)&run->job->query_heads, (void *)&run->job->head_dimension,
            (void *)&run->job->sliding_window,
            (void *)&run->job->compression_ratio, &attention_class,
            (void *)&run->job->token_position, &run->attention,
            &run->device_status
        };
        rc = cuda_attention_launch(
            &run->resources, run->state->deepseek_reduce_function,
            (unsigned int)run->job->query_heads, YVEX_CUDA_ATTN_BLOCK,
            YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params,
            "cuda.deepseek_attention.reduce", run->failure, run->err);
    }
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->attention, run->job->query_heads,
        run->job->head_dimension, run->job->token_position,
        &run->job->position, 1, run->device_status,
        "cuda.deepseek_attention.output_inverse_rope",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    for (group = 0ull; group < run->job->output_groups; ++group) {
        CUdeviceptr group_input =
            run->attention +
            group * run->job->output_group_input_width * sizeof(float);
        CUdeviceptr group_output =
            run->low + group * run->job->output_rank * sizeof(float);
        rc = cuda_attention_matvec(
            &run->resources,
            &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A],
            run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A],
            group * run->job->output_rank, run->job->output_rank,
            group_input, group_output, 1, run->device_status,
            "cuda.deepseek_attention.output_a", run->failure, run->err);
        if (rc != YVEX_OK) return rc;
    }
    return cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B], 0ull,
        run->job->hidden_width, run->low, run->final_output,
        1, run->device_status, "cuda.deepseek_attention.output_b",
        run->failure, run->err);
}
/* Purpose: finish device work.
 * Inputs: completed run and status allocation.
 * Effects: synchronizes/imports status.
 * Failure: typed sync/copy/numeric refusal.
 * Boundary: completion admission before any result ranges are copied. */
static int cuda_attention_synchronize(cuda_attention_run *run)
{
    int rc;
    rc = yvex_cuda_synchronize(
        run->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.synchronize", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE,
            "cuda.deepseek_attention.synchronize", 1ull, 0ull,
            (yvex_status)rc,
            "CUDA attention final synchronization failed");
    rc = yvex_cuda_status(
        &run->state->driver,
        run->state->driver.cuMemcpyDtoH_v2(
            &run->host_status, run->device_status, sizeof(run->host_status)),
        "cuda.deepseek_attention.copy.status", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_COPY,
            "cuda.deepseek_attention.copy.status", 1ull, 0ull,
            (yvex_status)rc, "CUDA attention status copy failed");
    if (run->host_status != 0)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
            "cuda.deepseek_attention.numeric", 0ull,
            (unsigned long long)run->host_status, YVEX_ERR_FORMAT,
            "CUDA attention device numerical stage refused its input");
    return YVEX_OK;
}
/* Purpose: reserve staging.
 * Inputs: base, cursor, count, width, output.
 * Effects: advances checked cursor.
 * Failure: false on overflow.
 * Boundary: Owns host staging layout only. */
static int cuda_attention_stage_range(
    unsigned char *base,
    size_t *cursor,
    unsigned long long count,
    size_t width,
    void **out)
{
    size_t aligned;
    size_t bytes;
    if (!cursor || !out) return 0;
    *out = NULL;
    if (!count) return 1;
    if (*cursor > SIZE_MAX - 7u ||
        !cuda_attention_checked_bytes(count, (unsigned long long)width, &bytes))
        return 0;
    aligned = (*cursor + 7u) & ~(size_t)7u;
    if (aligned > SIZE_MAX - bytes) return 0;
    if (base) *out = base + aligned;
    *cursor = aligned + bytes;
    return 1;
}
/* Purpose: derive/bind staging.
 * Inputs: run, optional base, total output.
 * Effects: binds private spans.
 * Failure: false on overflow.
 * Boundary: Owns host staging layout only. */
static int cuda_attention_stage_layout(cuda_attention_run *run,
                                       unsigned char *base,
                                       size_t *total)
{
    size_t cursor = 0u;
    size_t i;
    for (i = 0u; i < run->transfer_count; ++i)
        if (!cuda_attention_stage_range(
                base, &cursor, run->transfers[i].capacity,
                run->transfers[i].width, &run->transfers[i].staged))
            return 0;
    *total = cursor;
    return 1;
}
/* Purpose: acquire staging.
 * Inputs: synchronized run and planned layout.
 * Effects: owns one host arena.
 * Failure: typed allocation/layout refusal.
 * Boundary: Does not publish caller output. */
static int cuda_attention_stage_allocate(cuda_attention_run *run)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    size_t planned = run->host_stage_bytes;
    size_t actual = planned;
    if (injected && strcmp(injected, "host-staging") == 0)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
            "cuda.deepseek_attention.host_stage", run->host_stage_bytes, 0ull,
            YVEX_ERR_NOMEM,
            "injected CUDA attention host-staging allocation failure");
    run->host_stage = (unsigned char *)malloc(planned);
    if (!run->host_stage ||
        !cuda_attention_stage_layout(run, run->host_stage, &actual) ||
        actual != planned)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
            "cuda.deepseek_attention.host_stage", run->host_stage_bytes, 0ull,
            YVEX_ERR_NOMEM,
            "CUDA attention host-staging allocation failed");
    return YVEX_OK;
}
/* Purpose: stage a device range.
 * Inputs: run, endpoints, geometry, stage.
 * Effects: copies to private memory.
 * Failure: typed copy refusal.
 * Boundary: Does not mutate caller output. */
static int cuda_attention_copy(cuda_attention_run *run,
                               void *host,
                               CUdeviceptr device,
                               unsigned long long count,
                               size_t width,
                               const char *stage)
{
    const char *injected = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    size_t bytes;
    int rc;
    if (!count) return YVEX_OK;
    if (!host || !device ||
        !cuda_attention_checked_bytes(count, (unsigned long long)width, &bytes))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, count, 0ull,
            YVEX_ERR_BOUNDS,
            "CUDA attention staged copy geometry is invalid");
    if (injected &&
        (strcmp(injected, "copy-output") == 0 || strcmp(injected, stage) == 0))
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            YVEX_ERR_BACKEND,
            "injected CUDA attention staged output copy failure");
    rc = yvex_cuda_status(
        &run->state->driver,
        run->state->driver.cuMemcpyDtoH_v2(host, device, bytes), stage,
        run->err);
    if (rc != YVEX_OK)
        return cuda_attention_run_fail(
            run, YVEX_BACKEND_ATTENTION_FAILURE_COPY, stage, bytes, 0ull,
            (yvex_status)rc,
            "CUDA attention staged output copy failed");
    return YVEX_OK;
}
/* Purpose: stage all results.
 * Inputs: synchronized run and private arena.
 * Effects: fills staging.
 * Failure: typed copy or numeric refusal.
 * Boundary: Does not publish caller output. */
static int cuda_attention_copy_outputs(cuda_attention_run *run)
{
    int rc = cuda_attention_stage_allocate(run);
    size_t i;
    if (rc != YVEX_OK) return rc;
    if (run->job->attention_class == YVEX_BACKEND_ATTENTION_CSA) {
        rc = cuda_attention_copy(
            run, &run->staged_topk_count, run->selected_count, 1ull,
            sizeof(unsigned long long),
            "cuda.deepseek_attention.copy.selected_count");
        if (rc == YVEX_OK)
            rc = cuda_attention_copy(
                run, &run->staged_valid_count, run->valid_count, 1ull,
                sizeof(unsigned long long),
                "cuda.deepseek_attention.copy.valid_count");
        if (rc != YVEX_OK) return rc;
        if (run->staged_topk_count > run->topk_capacity)
            return cuda_attention_run_fail(
                run, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
                "cuda.deepseek_attention.copy.topk", run->topk_capacity,
                run->staged_topk_count, YVEX_ERR_BOUNDS,
                "CUDA attention top-k output exceeded its allocation");
    }
    for (i = 0u; i < run->transfer_count; ++i) {
        cuda_attention_transfer *transfer = &run->transfers[i];
        unsigned long long count = transfer->used ?
            *transfer->used : transfer->capacity;
        if (transfer->host_source) {
            if (count) memcpy(transfer->staged, transfer->host_source,
                              (size_t)count * transfer->width);
            continue;
        }
        rc = cuda_attention_copy(
            run, transfer->staged, *transfer->device, count, transfer->width,
            transfer->stage);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}
/* Purpose: publish results.
 * Inputs: validated run, spans, counters.
 * Effects: commits caller ranges.
 * Failure: none after admission.
 * Boundary: Performs the final transactional commit. */
static void cuda_attention_publish(cuda_attention_run *run)
{
    size_t i;
    for (i = 0u; i < run->transfer_count; ++i) {
        const cuda_attention_transfer *transfer = &run->transfers[i];
        unsigned long long count = transfer->used ?
            *transfer->used : transfer->capacity;
        if (count) memcpy(transfer->output, transfer->staged,
                          (size_t)count * transfer->width);
    }
    run->output->compressed_count = run->rolling[CUDA_ROLLING_MAIN].value_count;
    run->output->indexer_count = run->rolling[CUDA_ROLLING_INDEX].value_count;
    run->output->topk_count = run->staged_topk_count;
    run->output->valid_candidate_count = run->staged_valid_count;
    run->output->host_bytes = 0ull;
    run->output->peak_host_bytes = run->host_stage_bytes;
    run->output->device_bytes = 0ull;
    run->output->peak_device_bytes = run->resources.peak_bytes;
    run->output->kernel_launches = run->resources.launches;
}
typedef struct {
    int (*execute)(cuda_attention_run *run);
    const char *cancel_stage;
    int device_work_pending;
} cuda_attention_phase;
static const cuda_attention_phase cuda_attention_phases[] = {
    {cuda_attention_allocate_base,
     "cuda.deepseek_attention.cancel.after_allocation", 0},
    {cuda_attention_project,
     "cuda.deepseek_attention.cancel.after_projection", 1},
    {cuda_attention_compress,
     "cuda.deepseek_attention.cancel.after_compression", 1},
    {cuda_attention_reduce, "cuda.deepseek_attention.cancel.before_commit", 1},
    {cuda_attention_synchronize,
     "cuda.deepseek_attention.cancel.after_synchronize", 0},
    {cuda_attention_copy_outputs,
     "cuda.deepseek_attention.cancel.after_copy", 0}
};
/* Purpose: execute one-token CUDA attention.
 * Inputs: backend, encoded job, outputs.
 * Effects: runs transactional device work.
 * Failure: typed admission/resource/stage error.
 * Boundary: no CPU fallback, persistent KV ownership, decode loop, or generation claim. */
int yvex_backend_attention_execute(
    yvex_backend *backend,
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    cuda_attention_run run;
    size_t phase;
    int cleanup_rc;
    int rc;
    memset(&run, 0, sizeof(run));
    run.backend = backend;
    run.state = yvex_cuda_state(backend);
    run.job = job;
    run.output = output;
    run.failure = failure;
    run.err = err;
    run.candidate_capacity = 1ull;
    run.topk_capacity = 1ull;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    rc = cuda_attention_prepare(&run);
    if (rc != YVEX_OK) return rc;
    for (phase = 0u;
         phase < sizeof(cuda_attention_phases) /
                     sizeof(cuda_attention_phases[0]);
         ++phase) {
        rc = cuda_attention_phases[phase].execute(&run);
        if (rc == YVEX_OK)
            rc = cuda_attention_cancel(
                &run, cuda_attention_phases[phase].cancel_stage,
                cuda_attention_phases[phase].device_work_pending);
        if (rc != YVEX_OK) break;
    }
    cleanup_rc = cuda_attention_cleanup(&run.resources, err);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK)
        rc = cuda_attention_run_fail(
            &run, YVEX_BACKEND_ATTENTION_FAILURE_CLEANUP,
            "cuda.deepseek_attention.cleanup", 0ull,
            run.resources.current_bytes, (yvex_status)cleanup_rc,
            "CUDA attention temporary cleanup failed");
    if (rc == YVEX_OK)
        rc = cuda_attention_cancel(
            &run, "cuda.deepseek_attention.cancel.publish", 0);
    if (rc == YVEX_OK) {
        cuda_attention_publish(&run);
        if (failure) memset(failure, 0, sizeof(*failure));
        yvex_error_clear(err);
    }
    free(run.host_stage);
    return rc;
}
