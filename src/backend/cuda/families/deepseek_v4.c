/* Owner: src/backend/cuda
 * Owns: bounded Driver-API allocation/transfer/launch lifecycle for encoded DeepSeek attention weights and the
 *   admitted device numerical stages.
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
#include <yvex/quant.h>

#include "src/backend/cuda/private.h"

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

/* Purpose: populate and return one typed CUDA attention refusal.
 * Inputs: optional failure/error storage plus stable code, stage, and evidence.
 * Effects: resets failure detail and writes the canonical error.
 * Failure: returns the supplied status even when output storage is absent.
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

/* Purpose: convert an element count and width into representable host bytes.
 * Inputs: count, nonzero width, and required size output.
 * Effects: writes bytes only when representable.
 * Failure: returns false for invalid output, zero width, or size_t overflow.
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

/* Purpose: acquire one raw device allocation and optionally initialize it.
 * Inputs: resource owner, output pointer, bytes, optional source, zero policy, stage.
 * Effects: allocates/tracks device memory and may copy or zero its bytes.
 * Failure: typed argument, budget, allocation, copy, or injected refusal; owner retains cleanup.
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

/* Purpose: release every tracked temporary device allocation in reverse order.
 * Inputs: resource owner and optional error storage.
 * Effects: frees pointers, clears slots, and decrements owned-byte accounting.
 * Failure: returns the first cleanup failure after attempting every release.
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

/* Purpose: launch one admitted CUDA attention kernel stage.
 * Inputs: resource owner, function, launch geometry, parameters, stage, failure storage.
 * Effects: enqueues one kernel and increments launch evidence.
 * Failure: typed injected or Driver-API launch refusal.
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

/* Purpose: launch one encoded qtype matrix-vector projection.
 * Inputs: resource owner, encoded weight geometry, row slice, vector/output/status.
 * Effects: enqueues the canonical direct-encoded matvec kernel.
 * Failure: typed geometry or launch refusal.
 * Boundary: device compute only; no host decode fallback. */
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

/* Purpose: decode one admitted encoded weight row directly on device.
 * Inputs: resource owner, encoded weight, row/count, output/status, stage.
 * Effects: enqueues bounded decode into caller-selected device storage.
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

/* Purpose: apply one encoded learned-weight RMS normalization on device.
 * Inputs: values, exact count, encoded one-row weight, epsilon, status, stage.
 * Effects: enqueues in-place weighted normalization.
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

/* Purpose: unit-normalize an admitted batch of device vectors.
 * Inputs: values, vector count, width, epsilon, status, stage.
 * Effects: enqueues in-place normalization for each vector.
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

/* Purpose: apply or invert the admitted RoPE/YaRN position transform.
 * Inputs: device vectors, geometry, position facts, direction, status, stage.
 * Effects: enqueues an in-place position transform.
 * Failure: typed geometry/launch-extent or launch refusal.
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

/* Purpose: apply the admitted activation Hadamard/fake-quant policy on device.
 * Inputs: vectors, geometry, immutable activation policy, status, stage.
 * Effects: conditionally enqueues in-place activation transformation.
 * Failure: typed block geometry or launch refusal.
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

/* Purpose: validate immutable host job/output geometry before device allocation.
 * Inputs: immutable attention job, caller output views, failure/error storage.
 * Effects: reads geometry and records only a typed refusal on failure.
 * Failure: missing required jobs, weights, histories, or inconsistent CSA cardinality.
 * Boundary: admission only; no allocation, transfer, or launch. */
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

typedef struct {
    cuda_attention_resources resources;
    yvex_backend *backend;
    yvex_cuda_backend_state *state;
    const yvex_backend_attention_job *job;
    yvex_backend_attention_output *output;
    yvex_backend_attention_failure *failure;
    yvex_error *err;
    yvex_backend_attention_output committed;
    CUdeviceptr weight[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    CUdeviceptr input;
    CUdeviceptr q_low;
    CUdeviceptr query;
    CUdeviceptr raw_kv;
    CUdeviceptr sinks;
    CUdeviceptr attention;
    CUdeviceptr low;
    CUdeviceptr final_output;
    CUdeviceptr main_kv;
    CUdeviceptr main_score;
    CUdeviceptr main_ape;
    CUdeviceptr main_before_kv;
    CUdeviceptr main_before_score;
    CUdeviceptr main_after_kv;
    CUdeviceptr main_after_score;
    CUdeviceptr compressed;
    CUdeviceptr compressed_positions;
    CUdeviceptr index_kv;
    CUdeviceptr index_score;
    CUdeviceptr index_ape;
    CUdeviceptr index_before_kv;
    CUdeviceptr index_before_score;
    CUdeviceptr index_after_kv;
    CUdeviceptr index_after_score;
    CUdeviceptr indexer;
    CUdeviceptr indexer_positions;
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
    unsigned long long current_compressed_count;
    unsigned long long current_indexer_count;
    unsigned long long candidate_capacity;
    unsigned long long topk_capacity;
    unsigned long long main_extent;
    unsigned long long index_extent;
    unsigned long long low_count;
    unsigned long long local_extent;
    unsigned long long compressed_extent;
    unsigned long long history_index_extent;
    unsigned long long index_query_extent;
    unsigned long long emission_position;
    int host_status;
} cuda_attention_run;

/* Purpose: allocate a typed device value range using checked byte geometry.
 * Inputs: active run, target slot, count/width, optional source, zero policy, stage.
 * Effects: delegates acquisition/initialization to the transaction resource owner.
 * Failure: typed byte overflow, budget, allocation, copy, or initialization refusal.
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
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET, stage,
            ULLONG_MAX, count, run->err, YVEX_ERR_BOUNDS,
            "CUDA attention allocation size overflowed");
    return cuda_attention_allocate(&run->resources, target, bytes, source, zero,
                                   stage, run->failure, run->err);
}

/* Purpose: admit backend/job facts and derive every checked logical extent.
 * Inputs: initialized run with immutable job and caller output views.
 * Effects: admits capability/context and records derived extents/resource budget.
 * Failure: typed backend, capability, job geometry, context, or extent refusal.
 * Boundary: no device allocation or numerical execution. */
static int cuda_attention_prepare(cuda_attention_run *run)
{
    int rc;

    if (!run->backend || !run->state)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.execute", 1ull, 0ull, run->err,
            YVEX_ERR_INVALID_ARG, "CUDA attention backend is required");
    rc = cuda_attention_validate_job(run->job, run->output, run->failure,
                                     run->err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_cuda_require_capability(
        run->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.capability", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
            "cuda.deepseek_attention.capability", 1ull, 0ull, run->err,
            (yvex_status)rc,
            "CUDA DeepSeek attention kernel bundle is not admitted");
    rc = yvex_cuda_set_current(run->backend, "cuda.deepseek_attention.context",
                               run->err);
    if (rc != YVEX_OK) return rc;
    run->resources.backend = run->backend;
    run->resources.state = run->state;
    run->resources.budget = run->job->max_device_bytes;
    if (!yvex_core_u64_mul(run->job->query_heads,
                                    run->job->head_dimension,
                                    &run->query_width) ||
        !yvex_core_u64_mul(run->job->output_groups,
                                    run->job->output_rank,
                                    &run->low_count) ||
        !yvex_core_u64_mul(run->job->local_count,
                                    run->job->local_stride,
                                    &run->local_extent) ||
        !yvex_core_u64_mul(run->job->compressed_count,
                                    run->job->compressed_stride,
                                    &run->compressed_extent) ||
        !yvex_core_u64_mul(run->job->indexer_count,
                                    run->job->indexer_stride,
                                    &run->history_index_extent) ||
        !yvex_core_u64_mul(run->job->indexer_heads,
                                    run->job->indexer_head_dimension,
                                    &run->index_query_extent))
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
            "cuda.deepseek_attention.validate.extent", ULLONG_MAX, 0ull,
            run->err, YVEX_ERR_BOUNDS,
            "CUDA attention logical extent overflowed");
    return YVEX_OK;
}

/* Purpose: acquire encoded weights, workspaces, counters, and histories in canonical order.
 * Inputs: admitted run and precomputed extents.
 * Effects: allocates/copies every base device range under one resource owner.
 * Failure: first typed size, budget, allocation, initialization, or copy refusal.
 * Boundary: allocation phase only; no numerical kernels launch. */
static int cuda_attention_allocate_base(cuda_attention_run *run)
{
    unsigned int slot;
    int rc;

#define ALLOC(target, count, type, source, zero, stage) do {                  \
        rc = cuda_attention_alloc_values(run, &(target), (count),             \
                                         sizeof(type), (source), (zero),       \
                                         (stage));                             \
        if (rc != YVEX_OK) return rc;                                         \
    } while (0)
    ALLOC(run->device_status, 1ull, int, NULL, 1,
          "cuda.deepseek_attention.alloc.status");
    for (slot = 0u; slot < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++slot) {
        const yvex_backend_attention_weight *host = &run->job->weights[slot];
        if (!host->present) continue;
        rc = cuda_attention_allocate(
            &run->resources, &run->weight[slot], host->encoded_bytes,
            host->encoded, 0, "cuda.deepseek_attention.alloc.weight",
            run->failure, run->err);
        if (rc != YVEX_OK) return rc;
    }
    ALLOC(run->input, run->job->hidden_width, float, run->job->input, 0,
          "cuda.deepseek_attention.alloc.input");
    ALLOC(run->q_low, run->job->q_rank, float, NULL, 1,
          "cuda.deepseek_attention.alloc.q_low");
    ALLOC(run->query, run->query_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.query");
    ALLOC(run->raw_kv, run->job->kv_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.raw_kv");
    ALLOC(run->sinks, run->job->query_heads, float, NULL, 1,
          "cuda.deepseek_attention.alloc.sinks");
    ALLOC(run->attention, run->query_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.attention");
    ALLOC(run->low, run->low_count, float, NULL, 1,
          "cuda.deepseek_attention.alloc.output_low");
    ALLOC(run->final_output, run->job->hidden_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.output");
    ALLOC(run->selected_count, 1ull, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.selected_count");
    ALLOC(run->valid_count, 1ull, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.valid_count");
    if (run->job->local_count) {
        ALLOC(run->history_local, run->local_extent, float, run->job->local_kv,
              0, "cuda.deepseek_attention.alloc.local_history");
        ALLOC(run->history_local_positions, run->job->local_count,
              unsigned long long, run->job->local_positions, 0,
              "cuda.deepseek_attention.alloc.local_positions");
    }
    if (run->job->compressed_count) {
        ALLOC(run->history_compressed, run->compressed_extent, float,
              run->job->compressed_kv, 0,
              "cuda.deepseek_attention.alloc.compressed_history");
        ALLOC(run->history_compressed_positions, run->job->compressed_count,
              unsigned long long, run->job->compressed_positions, 0,
              "cuda.deepseek_attention.alloc.compressed_positions");
    }
    if (run->job->indexer_count) {
        ALLOC(run->history_indexer, run->history_index_extent, float,
              run->job->indexer_kv, 0,
              "cuda.deepseek_attention.alloc.indexer_history");
        ALLOC(run->history_indexer_positions, run->job->indexer_count,
              unsigned long long, run->job->indexer_positions, 0,
              "cuda.deepseek_attention.alloc.indexer_positions");
    }
#undef ALLOC
    return YVEX_OK;
}

/* Purpose: produce normalized/positioned current-token Q/KV values and sinks.
 * Inputs: allocated run, encoded projection/norm/sink weights, immutable policy.
 * Effects: enqueues projection, normalization, RoPE, activation, and decode stages.
 * Failure: first typed geometry, numerical-stage launch, or backend refusal.
 * Boundary: current-token projection only; no history reduction or host copy. */
static int cuda_attention_project(cuda_attention_run *run)
{
    int rc;

    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_Q_A], 0ull,
        run->job->q_rank, run->input, run->q_low, run->device_status,
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
        run->query_width, run->q_low, run->query, run->device_status,
        "cuda.deepseek_attention.q_b", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_KV],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_KV], 0ull,
        run->job->kv_width, run->input, run->raw_kv, run->device_status,
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

/* Purpose: advance the main rolling compressor and optional emitted vector.
 * Inputs: allocated run and immutable main rolling recurrence.
 * Effects: allocates state ranges, enqueues recurrence/norm/RoPE/activation, records emission.
 * Failure: typed missing state, size/budget/allocation, copy, geometry, or launch refusal.
 * Boundary: main compressor transaction; caller state is not published here. */
static int cuda_attention_main_rolling(cuda_attention_run *run)
{
    const yvex_backend_attention_rolling *rolling = &run->job->main_rolling;
    void *params[17];
    int emit;
    int rc;

#define ALLOC(target, count, type, source, zero, stage) do {                  \
        rc = cuda_attention_alloc_values(run, &(target), (count),             \
                                         sizeof(type), (source), (zero),       \
                                         (stage));                             \
        if (rc != YVEX_OK) return rc;                                         \
    } while (0)
    if (!rolling->present || !rolling->kv_state || !rolling->score_state)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.main_rolling", 1ull, 0ull, run->err,
            YVEX_ERR_FORMAT, "CUDA attention main rolling state is absent");
    run->main_extent = rolling->state_width * rolling->state_slots;
    ALLOC(run->main_kv, rolling->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.main_kv");
    ALLOC(run->main_score, rolling->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.main_score");
    ALLOC(run->main_ape, rolling->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.main_ape");
    ALLOC(run->main_before_kv, run->main_extent, float, rolling->kv_state, 0,
          "cuda.deepseek_attention.alloc.main_before_kv");
    ALLOC(run->main_before_score, run->main_extent, float,
          rolling->score_state, 0,
          "cuda.deepseek_attention.alloc.main_before_score");
    ALLOC(run->main_after_kv, run->main_extent, float, NULL, 1,
          "cuda.deepseek_attention.alloc.main_after_kv");
    ALLOC(run->main_after_score, run->main_extent, float, NULL, 1,
          "cuda.deepseek_attention.alloc.main_after_score");
    ALLOC(run->compressed, rolling->head_dimension, float, NULL, 1,
          "cuda.deepseek_attention.alloc.compressed");
    ALLOC(run->compressed_positions, 1ull, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.current_compressed_position");
#undef ALLOC
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV], 0ull,
        rolling->state_width, run->input, run->main_kv, run->device_status,
        "cuda.deepseek_attention.main_kv", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE], 0ull,
        rolling->state_width, run->input, run->main_score,
        run->device_status, "cuda.deepseek_attention.main_gate",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_decode(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE],
        run->job->token_position % rolling->ratio, rolling->state_width,
        run->main_ape, run->device_status,
        "cuda.deepseek_attention.main_ape", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    emit = ((run->job->token_position + 1ull) % rolling->ratio) == 0ull;
    if (emit) {
        run->emission_position =
            run->job->token_position + 1ull - rolling->ratio;
        run->current_compressed_count = 1ull;
        rc = yvex_cuda_status(
            &run->state->driver,
            run->state->driver.cuMemcpyHtoD_v2(
                run->compressed_positions, &run->emission_position,
                sizeof(run->emission_position)),
            "cuda.deepseek_attention.copy.compressed_position", run->err);
        if (rc != YVEX_OK) return rc;
    }
    params[0] = &run->main_before_kv;
    params[1] = &run->main_before_score;
    params[2] = &run->main_kv;
    params[3] = &run->main_score;
    params[4] = &run->main_ape;
    params[5] = &run->main_after_kv;
    params[6] = &run->main_after_score;
    params[7] = &run->compressed;
    params[8] = (void *)&rolling->ratio;
    params[9] = (void *)&rolling->head_dimension;
    params[10] = (void *)&rolling->state_width;
    params[11] = (void *)&rolling->state_slots;
    params[12] = (void *)&rolling->cursor;
    params[13] = (void *)&rolling->overlap;
    params[14] = &emit;
    params[15] = &run->device_status;
    rc = cuda_attention_launch(
        &run->resources, run->state->deepseek_rolling_function, 1u,
        YVEX_CUDA_ATTN_BLOCK, 0u, params,
        "cuda.deepseek_attention.main_rolling", run->failure, run->err);
    if (rc != YVEX_OK || !emit) return rc;
    rc = cuda_attention_weighted_norm(
        &run->resources, run->compressed, rolling->head_dimension,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM],
        run->job->rms_epsilon, run->device_status,
        "cuda.deepseek_attention.main_emission_norm", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->compressed, 1ull, rolling->head_dimension,
        run->emission_position, &run->job->position, 0, run->device_status,
        "cuda.deepseek_attention.main_emission_rope", run->failure, run->err);
    if (rc != YVEX_OK ||
        rolling->head_dimension <= run->job->position.rope_dimensions)
        return rc;
    return cuda_attention_activation(
        &run->resources, run->compressed, 1ull,
        rolling->head_dimension - run->job->position.rope_dimensions,
        &run->job->compressor_activation, run->device_status,
        "cuda.deepseek_attention.main_emission_activation",
        run->failure, run->err);
}

/* Purpose: advance the CSA indexer rolling compressor and optional emitted vector.
 * Inputs: allocated run and immutable indexer recurrence.
 * Effects: allocates state ranges, enqueues recurrence/norm/RoPE/activation, records emission.
 * Failure: typed missing state, size/budget/allocation, copy, geometry, or launch refusal.
 * Boundary: indexer recurrence only; sparse selection is a separate stage. */
static int cuda_attention_index_rolling(cuda_attention_run *run)
{
    const yvex_backend_attention_rolling *index = &run->job->indexer_rolling;
    void *params[17];
    int emit;
    int rc;

#define ALLOC(target, count, type, source, zero, stage) do {                  \
        rc = cuda_attention_alloc_values(run, &(target), (count),             \
                                         sizeof(type), (source), (zero),       \
                                         (stage));                             \
        if (rc != YVEX_OK) return rc;                                         \
    } while (0)
    if (!index->present || !index->kv_state || !index->score_state)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
            "cuda.deepseek_attention.index_rolling", 1ull, 0ull, run->err,
            YVEX_ERR_FORMAT, "CUDA CSA indexer rolling state is absent");
    run->index_extent = index->state_width * index->state_slots;
    ALLOC(run->index_kv, index->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_kv");
    ALLOC(run->index_score, index->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_score");
    ALLOC(run->index_ape, index->state_width, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_ape");
    ALLOC(run->index_before_kv, run->index_extent, float, index->kv_state, 0,
          "cuda.deepseek_attention.alloc.index_before_kv");
    ALLOC(run->index_before_score, run->index_extent, float,
          index->score_state, 0,
          "cuda.deepseek_attention.alloc.index_before_score");
    ALLOC(run->index_after_kv, run->index_extent, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_after_kv");
    ALLOC(run->index_after_score, run->index_extent, float, NULL, 1,
          "cuda.deepseek_attention.alloc.index_after_score");
    ALLOC(run->indexer, index->head_dimension, float, NULL, 1,
          "cuda.deepseek_attention.alloc.current_indexer");
    ALLOC(run->indexer_positions, 1ull, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.current_indexer_position");
#undef ALLOC
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV], 0ull,
        index->state_width, run->input, run->index_kv, run->device_status,
        "cuda.deepseek_attention.index_kv", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE], 0ull,
        index->state_width, run->input, run->index_score, run->device_status,
        "cuda.deepseek_attention.index_gate", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_decode(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE],
        run->job->token_position % index->ratio, index->state_width,
        run->index_ape, run->device_status,
        "cuda.deepseek_attention.index_ape", run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    emit = ((run->job->token_position + 1ull) % index->ratio) == 0ull;
    if (emit) {
        run->current_indexer_count = 1ull;
        rc = yvex_cuda_status(
            &run->state->driver,
            run->state->driver.cuMemcpyHtoD_v2(
                run->indexer_positions, &run->emission_position,
                sizeof(run->emission_position)),
            "cuda.deepseek_attention.copy.indexer_position", run->err);
        if (rc != YVEX_OK) return rc;
    }
    params[0] = &run->index_before_kv;
    params[1] = &run->index_before_score;
    params[2] = &run->index_kv;
    params[3] = &run->index_score;
    params[4] = &run->index_ape;
    params[5] = &run->index_after_kv;
    params[6] = &run->index_after_score;
    params[7] = &run->indexer;
    params[8] = (void *)&index->ratio;
    params[9] = (void *)&index->head_dimension;
    params[10] = (void *)&index->state_width;
    params[11] = (void *)&index->state_slots;
    params[12] = (void *)&index->cursor;
    params[13] = (void *)&index->overlap;
    params[14] = &emit;
    params[15] = &run->device_status;
    rc = cuda_attention_launch(
        &run->resources, run->state->deepseek_rolling_function, 1u,
        YVEX_CUDA_ATTN_BLOCK, 0u, params,
        "cuda.deepseek_attention.index_rolling", run->failure, run->err);
    if (rc != YVEX_OK || !emit) return rc;
    rc = cuda_attention_weighted_norm(
        &run->resources, run->indexer, index->head_dimension,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM],
        run->job->rms_epsilon, run->device_status,
        "cuda.deepseek_attention.index_emission_norm",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_rope(
        &run->resources, run->indexer, 1ull, index->head_dimension,
        run->emission_position, &run->job->position, 0, run->device_status,
        "cuda.deepseek_attention.index_emission_rope",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    return cuda_attention_activation(
        &run->resources, run->indexer, 1ull, index->head_dimension,
        &run->job->compressor_rotated_activation, run->device_status,
        "cuda.deepseek_attention.index_emission_activation",
        run->failure, run->err);
}

/* Purpose: project CSA scoring inputs and launch deterministic sparse top-k.
 * Inputs: run with completed index recurrence and immutable history/policy.
 * Effects: allocates query/score/top-k scratch and enqueues projection/selection kernels.
 * Failure: typed projection, extent, capacity, allocation, activation, or launch refusal.
 * Boundary: selection only; selected values enter the later reduction kernel. */
static int cuda_attention_index_topk(cuda_attention_run *run)
{
    void *params[22];
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
        run->device_status, "cuda.deepseek_attention.index_query",
        run->failure, run->err);
    if (rc != YVEX_OK) return rc;
    rc = cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION], 0ull,
        run->job->indexer_heads, run->input, run->index_weights,
        run->device_status, "cuda.deepseek_attention.index_weights",
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
    if (!yvex_core_u64_add(run->job->indexer_count,
                                    run->current_indexer_count,
                                    &run->candidate_capacity))
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
            "cuda.deepseek_attention.topk.extent", ULLONG_MAX,
            run->job->indexer_count, run->err, YVEX_ERR_BOUNDS,
            "CUDA attention top-k candidate count overflowed");
    if (!run->candidate_capacity) run->candidate_capacity = 1ull;
    run->topk_capacity =
        run->job->indexer_topk < run->candidate_capacity
            ? run->job->indexer_topk
            : run->candidate_capacity;
    if (!run->topk_capacity) run->topk_capacity = 1ull;
    ALLOC(run->selected, run->topk_capacity, unsigned long long, NULL, 1,
          "cuda.deepseek_attention.alloc.selected");
    ALLOC(run->selected_positions, run->topk_capacity, unsigned long long,
          NULL, 1, "cuda.deepseek_attention.alloc.selected_positions");
    ALLOC(run->topk_scores, run->candidate_capacity, float, NULL, 1,
          "cuda.deepseek_attention.alloc.topk_scores");
    ALLOC(run->valid_indexes, run->candidate_capacity, unsigned long long,
          NULL, 1, "cuda.deepseek_attention.alloc.valid_indexes");
#undef ALLOC
    params[0] = &run->index_query;
    params[1] = &run->index_weights;
    params[2] = &run->history_indexer;
    params[3] = &run->history_indexer_positions;
    params[4] = (void *)&run->job->indexer_count;
    params[5] = (void *)&run->job->indexer_stride;
    params[6] = &run->indexer;
    params[7] = &run->indexer_positions;
    params[8] = &run->current_indexer_count;
    params[9] = (void *)&run->job->indexer_head_dimension;
    params[10] = (void *)&run->job->indexer_heads;
    params[11] = (void *)&run->job->indexer_head_dimension;
    params[12] = (void *)&run->job->compression_ratio;
    params[13] = (void *)&run->job->token_position;
    params[14] = (void *)&run->job->indexer_topk;
    params[15] = &run->selected;
    params[16] = &run->selected_positions;
    params[17] = &run->selected_count;
    params[18] = &run->valid_count;
    params[19] = &run->topk_scores;
    params[20] = &run->valid_indexes;
    params[21] = &run->device_status;
    return cuda_attention_launch(
        &run->resources, run->state->deepseek_topk_function, 1u, 1u, 0u,
        params, "cuda.deepseek_attention.topk", run->failure, run->err);
}

/* Purpose: compose only the rolling/sparse stages required by the attention class.
 * Inputs: run after current-token projection.
 * Effects: conditionally advances main and CSA indexer/top-k stages.
 * Failure: propagates the first typed compressor or sparse-selection refusal.
 * Boundary: class dispatch only; no CPU fallback or output publication. */
static int cuda_attention_compress(cuda_attention_run *run)
{
    int rc;

    if (run->job->attention_class == 0u) return YVEX_OK;
    rc = cuda_attention_main_rolling(run);
    if (rc != YVEX_OK || run->job->attention_class != 1u) return rc;
    rc = cuda_attention_index_rolling(run);
    if (rc != YVEX_OK) return rc;
    return cuda_attention_index_topk(run);
}

/* Purpose: reduce admitted histories and apply grouped low-rank output projection.
 * Inputs: run with projected token, histories, optional compression, and selection.
 * Effects: enqueues reduction, inverse RoPE, grouped OUT_A, and final OUT_B kernels.
 * Failure: first typed reduction, position, projection-geometry, or launch refusal.
 * Boundary: final device numerical stage; host output remains uncommitted. */
static int cuda_attention_reduce(cuda_attention_run *run)
{
    void *params[27];
    unsigned long long group;
    int rc;

    params[0] = &run->query;
    params[1] = &run->history_local;
    params[2] = &run->history_local_positions;
    params[3] = (void *)&run->job->local_count;
    params[4] = (void *)&run->job->local_stride;
    params[5] = &run->raw_kv;
    params[6] = (void *)&run->job->kv_width;
    params[7] = &run->history_compressed;
    params[8] = &run->history_compressed_positions;
    params[9] = (void *)&run->job->compressed_count;
    params[10] = (void *)&run->job->compressed_stride;
    params[11] = &run->compressed;
    params[12] = &run->compressed_positions;
    params[13] = &run->current_compressed_count;
    params[14] = (void *)&run->job->head_dimension;
    params[15] = &run->selected;
    params[16] = &run->selected_count;
    params[17] = &run->sinks;
    params[18] = (void *)&run->job->query_heads;
    params[19] = (void *)&run->job->head_dimension;
    params[20] = (void *)&run->job->sliding_window;
    params[21] = (void *)&run->job->compression_ratio;
    params[22] = (void *)&run->job->attention_class;
    params[23] = (void *)&run->job->token_position;
    params[24] = &run->attention;
    params[25] = &run->device_status;
    rc = cuda_attention_launch(
        &run->resources, run->state->deepseek_reduce_function,
        (unsigned int)run->job->query_heads, YVEX_CUDA_ATTN_BLOCK,
        YVEX_CUDA_ATTN_BLOCK * (unsigned int)sizeof(double), params,
        "cuda.deepseek_attention.reduce", run->failure, run->err);
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
            group_input, group_output, run->device_status,
            "cuda.deepseek_attention.output_a", run->failure, run->err);
        if (rc != YVEX_OK) return rc;
    }
    return cuda_attention_matvec(
        &run->resources,
        &run->job->weights[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B],
        run->weight[YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B], 0ull,
        run->job->hidden_width, run->low, run->final_output,
        run->device_status, "cuda.deepseek_attention.output_b",
        run->failure, run->err);
}

/* Purpose: synchronize once, import device status, and apply copy fault injection.
 * Inputs: completed device run with status allocation.
 * Effects: synchronizes the backend and copies one status word to host.
 * Failure: typed synchronize, status-copy, numerical, or injected output-copy refusal.
 * Boundary: completion admission before any result ranges are copied. */
static int cuda_attention_synchronize(cuda_attention_run *run)
{
    const char *copy_failure;
    int rc;

    rc = yvex_cuda_synchronize(
        run->backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        "cuda.deepseek_attention.synchronize", run->err);
    if (rc != YVEX_OK)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE,
            "cuda.deepseek_attention.synchronize", 1ull, 0ull, run->err,
            (yvex_status)rc,
            "CUDA attention final synchronization failed");
    rc = yvex_cuda_status(
        &run->state->driver,
        run->state->driver.cuMemcpyDtoH_v2(
            &run->host_status, run->device_status, sizeof(run->host_status)),
        "cuda.deepseek_attention.copy.status", run->err);
    if (rc != YVEX_OK) return rc;
    if (run->host_status != 0)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
            "cuda.deepseek_attention.numeric", 0ull,
            (unsigned long long)run->host_status, run->err, YVEX_ERR_FORMAT,
            "CUDA attention device numerical stage refused its input");
    copy_failure = getenv("YVEX_TEST_CUDA_ATTENTION_FAILURE");
    if (copy_failure && strcmp(copy_failure, "copy-output") == 0)
        return cuda_attention_fail(
            run->failure, YVEX_BACKEND_ATTENTION_FAILURE_COPY,
            "cuda.deepseek_attention.copy.output", run->job->hidden_width,
            0ull, run->err, YVEX_ERR_BACKEND,
            "injected CUDA attention output copy failure");
    return YVEX_OK;
}

/* Purpose: copy one optional completed device range into caller-owned host storage.
 * Inputs: run, optional host/device endpoints, count/width, and stage.
 * Effects: performs one bounded DtoH copy when the range is present.
 * Failure: returns bounds or Driver-API copy refusal.
 * Boundary: copy primitive; it does not alter transaction completion facts. */
static int cuda_attention_copy(cuda_attention_run *run,
                               void *host,
                               CUdeviceptr device,
                               unsigned long long count,
                               size_t width,
                               const char *stage)
{
    size_t bytes;

    if (!host || !device || !count) return YVEX_OK;
    if (!cuda_attention_checked_bytes(count, (unsigned long long)width, &bytes))
        return YVEX_ERR_BOUNDS;
    return yvex_cuda_status(
        &run->state->driver,
        run->state->driver.cuMemcpyDtoH_v2(host, device, bytes),
        stage, run->err);
}

/* Purpose: import every result/state range in the historical transaction order.
 * Inputs: synchronized run and caller-owned output views.
 * Effects: copies outputs, sparse evidence, emissions, and rolling after-state.
 * Failure: first typed bounds, copy, or top-k cardinality refusal.
 * Boundary: writes caller buffers but does not publish the output summary. */
static int cuda_attention_copy_outputs(cuda_attention_run *run)
{
    int rc;

#define COPY(host, device, count, type, stage) do {                           \
        rc = cuda_attention_copy(run, (host), (device), (count),              \
                                 sizeof(type), (stage));                        \
        if (rc != YVEX_OK) return rc;                                         \
    } while (0)
    COPY(run->output->q_low, run->q_low, run->job->q_rank, float,
         "cuda.deepseek_attention.copy.q_low");
    COPY(run->output->query, run->query, run->query_width, float,
         "cuda.deepseek_attention.copy.query");
    COPY(run->output->raw_kv, run->raw_kv, run->job->kv_width, float,
         "cuda.deepseek_attention.copy.raw_kv");
    COPY(run->output->attention_values, run->attention, run->query_width, float,
         "cuda.deepseek_attention.copy.attention");
    COPY(run->output->output, run->final_output, run->job->hidden_width, float,
         "cuda.deepseek_attention.copy.output");
    if (run->current_compressed_count) {
        COPY(run->output->compressed_kv, run->compressed,
             run->job->head_dimension, float,
             "cuda.deepseek_attention.copy.compressed");
        run->output->compressed_positions[0] = run->emission_position;
    }
    if (run->current_indexer_count) {
        COPY(run->output->indexer_kv, run->indexer,
             run->job->indexer_head_dimension, float,
             "cuda.deepseek_attention.copy.indexer");
        run->output->indexer_positions[0] = run->emission_position;
    }
    if (run->job->attention_class == 1u) {
        COPY(run->output->index_query, run->index_query,
             run->index_query_extent, float,
             "cuda.deepseek_attention.copy.index_query");
        COPY(run->output->index_weights, run->index_weights,
             run->job->indexer_heads, float,
             "cuda.deepseek_attention.copy.index_weights");
        COPY(&run->committed.topk_count, run->selected_count, 1ull,
             unsigned long long,
             "cuda.deepseek_attention.copy.selected_count");
        COPY(&run->committed.valid_candidate_count, run->valid_count, 1ull,
             unsigned long long, "cuda.deepseek_attention.copy.valid_count");
        if (run->committed.topk_count > run->topk_capacity)
            return cuda_attention_fail(
                run->failure, YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
                "cuda.deepseek_attention.copy.topk", run->topk_capacity,
                run->committed.topk_count, run->err, YVEX_ERR_BOUNDS,
                "CUDA attention top-k output exceeded its allocation");
        COPY(run->output->topk_positions, run->selected_positions,
             run->committed.topk_count, unsigned long long,
             "cuda.deepseek_attention.copy.topk_positions");
    }
    if (run->main_extent) {
        COPY(run->output->main_kv_state, run->main_after_kv,
             run->main_extent, float,
             "cuda.deepseek_attention.copy.main_kv_state");
        COPY(run->output->main_score_state, run->main_after_score,
             run->main_extent, float,
             "cuda.deepseek_attention.copy.main_score_state");
    }
    if (run->index_extent) {
        COPY(run->output->indexer_kv_state, run->index_after_kv,
             run->index_extent, float,
             "cuda.deepseek_attention.copy.index_kv_state");
        COPY(run->output->indexer_score_state, run->index_after_score,
             run->index_extent, float,
             "cuda.deepseek_attention.copy.index_score_state");
    }
#undef COPY
    return YVEX_OK;
}

/* Purpose: seal completion counters and restore caller-owned view pointers.
 * Inputs: successfully copied run and zeroed committed summary.
 * Effects: populates the immutable-to-publish summary only.
 * Failure: none; allocation and copy admission completed earlier.
 * Boundary: local transaction seal; public output assignment follows cleanup. */
static void cuda_attention_commit(cuda_attention_run *run)
{
    run->committed.compressed_count = run->current_compressed_count;
    run->committed.indexer_count = run->current_indexer_count;
    run->committed.device_bytes = run->resources.current_bytes;
    run->committed.peak_device_bytes = run->resources.peak_bytes;
    run->committed.kernel_launches = run->resources.launches;
    run->committed.q_low = run->output->q_low;
    run->committed.query = run->output->query;
    run->committed.raw_kv = run->output->raw_kv;
    run->committed.compressed_kv = run->output->compressed_kv;
    run->committed.indexer_kv = run->output->indexer_kv;
    run->committed.index_query = run->output->index_query;
    run->committed.index_weights = run->output->index_weights;
    run->committed.attention_values = run->output->attention_values;
    run->committed.output = run->output->output;
    run->committed.compressed_positions = run->output->compressed_positions;
    run->committed.indexer_positions = run->output->indexer_positions;
    run->committed.topk_positions = run->output->topk_positions;
    run->committed.main_kv_state = run->output->main_kv_state;
    run->committed.main_score_state = run->output->main_score_state;
    run->committed.indexer_kv_state = run->output->indexer_kv_state;
    run->committed.indexer_score_state = run->output->indexer_score_state;
}

/* Purpose: execute every admitted one-token DeepSeek attention stage on CUDA.
 * Inputs: admitted backend, immutable encoded job, caller-owned result views.
 * Effects: allocates temporary device ranges, launches kernels, and commits host outputs.
 * Failure: returns typed admission, allocation, launch, numeric, copy, or cleanup errors.
 * Boundary: no CPU fallback, persistent KV ownership, decode loop, or generation claim. */
int yvex_backend_attention_execute(
    yvex_backend *backend,
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err)
{
    cuda_attention_run run;
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
    rc = cuda_attention_allocate_base(&run);
    if (rc == YVEX_OK) rc = cuda_attention_project(&run);
    if (rc == YVEX_OK) rc = cuda_attention_compress(&run);
    if (rc == YVEX_OK) rc = cuda_attention_reduce(&run);
    if (rc == YVEX_OK) rc = cuda_attention_synchronize(&run);
    if (rc == YVEX_OK) rc = cuda_attention_copy_outputs(&run);
    if (rc == YVEX_OK) cuda_attention_commit(&run);
    cleanup_rc = cuda_attention_cleanup(&run.resources, err);
    if (rc == YVEX_OK && cleanup_rc != YVEX_OK)
        rc = cuda_attention_fail(
            failure, YVEX_BACKEND_ATTENTION_FAILURE_CLEANUP,
            "cuda.deepseek_attention.cleanup", 0ull,
            run.resources.current_bytes, err, (yvex_status)cleanup_rc,
            "CUDA attention temporary cleanup failed");
    if (rc == YVEX_OK) {
        *output = run.committed;
        if (failure) memset(failure, 0, sizeof(*failure));
        yvex_error_clear(err);
    }
    return rc;
}
