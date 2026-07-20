/* Owner: runtime graph execution evidence.
 * Owns: fixture, partial, segment, and operator attention lifecycle orchestration plus deterministic accounting.
 * Does not own: graph equations, family policy, backend kernels, rendering, persistent KV, or generation support.
 * Invariants: execution borrows an admitted engine and materialized weights; every failure releases temporary
 *   tensors and buffers; results never promote diagnostic execution to transformer support.
 * Boundary: selected graph evidence is not full transformer or generation execution.
 * Purpose: execute the existing bounded runtime graph proof independently of engine/session lifecycle coordination.
 * Inputs: an opened engine, explicit fixture/segment requests, and owned output result structures.
 * Effects: allocates bounded scratch, invokes admitted backend primitives, and writes only the supplied result
 *   structure.
 * Failure: returns typed errors after deterministic scratch and tensor cleanup. */

#include <yvex/internal/core.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/gguf_writer.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/source_payload.h>

#include <stddef.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Purpose: publish one stable typed runtime refusal and return its status. */
static int runtime_refuse(yvex_error *err, yvex_status status,
                          const char *where, const char *message)
{
    yvex_error_set(err, status, where, message);
    return status;
}

/* Purpose: hash bounded diagnostic bytes without creating a semantic identity. */
static unsigned long long fixture_checksum_bytes(const unsigned char *data, unsigned long long len)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        hash ^= (unsigned long long)data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Purpose: decode one admitted little-endian F16 scalar through the canonical codec.
 * Inputs: an immutable two-byte scalar.
 * Effects: none.
 * Failure: the canonical codec defines every bit pattern.
 * Boundary: scalar decoding does not admit a tensor or execution path. */
static float runtime_decode_f16(const unsigned char *bytes)
{
    unsigned short bits = (unsigned short)bytes[0] | (unsigned short)((unsigned short)bytes[1] << 8);

    return yvex_quant_f16_decode(bits);
}

/* Purpose: decode one validated F16 embedding slice for the diagnostic reference.
 * Inputs: admitted artifact/ranges and a width-sized caller buffer.
 * Effects: writes only the caller buffer and reads no bytes outside the validated slice.
 * Failure: rejects absent owners or invalid ranges before decoding.
 * Boundary: bounded graph evidence, not model execution admission. */
static int build_f16_embedding_reference(const yvex_artifact *artifact,
                                         const yvex_tensor_range *range,
                                         const yvex_tensor_slice_range *slice, float *out,
                                         yvex_error *err)
{
    const unsigned char *data;
    unsigned long long hidden_size;
    unsigned long long slice_offset;
    unsigned long long i;

    if (!artifact || !range || !slice || !out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                              "artifact, range, slice, and reference output are required");
    if (!range->range_valid || !slice->range_valid)
        return runtime_refuse(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                              "partial reference slice range is invalid");
    hidden_size = range->dims[0];
    slice_offset = slice->slice_absolute_offset;
    data = yvex_artifact_data(artifact);
    if (!data)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                              "artifact data is unavailable");

    for (i = 0; i < hidden_size; ++i) {
        out[i] = runtime_decode_f16(data + slice_offset + (i * 2ull));
    }
    return YVEX_OK;
}

/* Purpose: compute deterministic positive square roots for the scalar RMSNorm oracle. */
static double runtime_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

/* Purpose: resolve the first admitted RMSNorm epsilon metadata key.
 * Inputs: immutable GGUF metadata and empty key/value outputs.
 * Effects: borrows the canonical key and publishes its positive scalar value.
 * Failure: rejects malformed or absent metadata.
 * Boundary: metadata lookup only; it does not infer model topology. */
static int runtime_find_rmsnorm_epsilon(const yvex_gguf *gguf, const char **key_out,
                                        double *epsilon_out, yvex_error *err)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon", "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    if (!gguf || !key_out || !epsilon_out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                              "GGUF, key output, and epsilon output are required");

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (!value) {
            continue;
        }
        if (yvex_gguf_value_as_f64(value, &epsilon) != YVEX_OK || epsilon <= 0.0) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                            "rmsnorm-epsilon-invalid: %s", keys[i]);
            return YVEX_ERR_FORMAT;
        }
        *key_out = keys[i];
        *epsilon_out = epsilon;
        return YVEX_OK;
    }

    return runtime_refuse(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                          "rmsnorm-epsilon-missing");
}

/* Purpose: find the first explicitly named or role-admitted RMSNorm tensor.
 * Inputs: an immutable admitted tensor table.
 * Effects: returns one borrowed descriptor.
 * Failure: returns null when no RMSNorm role is present.
 * Boundary: lookup does not infer or mutate topology. */
static const yvex_tensor_info *runtime_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight", "blk.0.attention_norm.weight", "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

/* Purpose: decode one validated scalar RMSNorm weight for the diagnostic oracle.
 * Inputs: admitted artifact/range and a dimension-sized caller buffer.
 * Effects: reads only the validated tensor range and writes the caller buffer.
 * Failure: rejects invalid geometry, unsupported scalar storage, or absent data.
 * Boundary: scalar diagnostic reference, not a production backend path. */
static int build_rmsnorm_weight_reference(const yvex_artifact *artifact,
                                          const yvex_tensor_range *range, float *out,
                                          yvex_error *err)
{
    const unsigned char *data;
    unsigned long long i;

    if (!artifact || !range || !out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                              "artifact, range, and RMSNorm reference output are required");
    if (!range->range_valid || range->rank != 1)
        return runtime_refuse(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                              "RMSNorm reference tensor range is invalid");
    if (range->dtype != YVEX_DTYPE_F16 && range->dtype != YVEX_DTYPE_F32)
        return runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                              "rmsnorm-dtype-invalid");
    data = yvex_artifact_data(artifact);
    if (!data)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                              "artifact data is unavailable");
    data += range->tensor_absolute_offset;
    for (i = 0; i < range->dims[0]; ++i) {
        if (range->dtype == YVEX_DTYPE_F16) {
            out[i] = runtime_decode_f16(data + (i * 2ull));
        } else {
            memcpy(&out[i], data + (i * (unsigned long long)sizeof(float)), sizeof(float));
        }
    }
    return YVEX_OK;
}

/* Purpose: evaluate the scalar RMSNorm diagnostic with deterministic accumulation order.
 * Inputs: immutable embedding/weight rows, positive epsilon, and caller output.
 * Effects: writes exactly hidden-size F32 values.
 * Failure: geometry is admitted by the caller before this total operation.
 * Boundary: diagnostic oracle only, not a backend implementation. */
static void build_rmsnorm_reference(const float *embedding, const float *weight,
                                    unsigned long long hidden_size, double epsilon, float *out)
{
    unsigned long long i;
    double sum_squares = 0.0;
    double inv_rms;

    for (i = 0; i < hidden_size; ++i) {
        sum_squares += (double)embedding[i] * (double)embedding[i];
    }
    inv_rms = 1.0 / runtime_sqrt_double((sum_squares / (double)hidden_size) + epsilon);
    for (i = 0; i < hidden_size; ++i) {
        out[i] = (float)((double)embedding[i] * inv_rms * (double)weight[i]);
    }
}

/* Purpose: measure the maximum absolute difference across two bounded F32 vectors. */
static double max_abs_diff_f32(const float *a, const float *b, unsigned long long count)
{
    yvex_graph_f32_comparison comparison;

    if (yvex_graph_f32_compare(a, b, count, 0.0, 0.0, &comparison, NULL) != YVEX_OK ||
        comparison.nonfinite_value_count)
        return INFINITY;
    return comparison.maximum_absolute_error;
}

/* Purpose: copy the bounded diagnostic prefix and return its published element count. */
static unsigned long long runtime_sample_f32(float *destination, unsigned long long capacity,
                                             const float *source, unsigned long long count)
{
    unsigned long long index;
    unsigned long long copied = count < capacity ? count : capacity;

    for (index = 0; index < copied; ++index)
        destination[index] = source[index];
    return copied;
}

/* Purpose: test one planned operation without leaking graph traversal into executors. */
static int runtime_graph_has_op(const yvex_graph *graph, yvex_op_kind kind)
{
    unsigned long long index;

    for (index = 0; graph && index < yvex_graph_op_count(graph); ++index) {
        const yvex_graph_op_info *candidate = yvex_graph_op_at(graph, index);

        if (candidate && candidate->kind == kind) {
            return 1;
        }
    }
    return 0;
}

typedef struct {
    const yvex_materialized_weight *weight;
    const yvex_tensor_info *tensor;
    const yvex_device_tensor *device;
    yvex_selected_embedding_shape shape;
} runtime_embedding_binding;

/* Purpose: bind one admitted token embedding for fixture or artifact-backed graph execution.
 * Inputs: admitted engine, token identity, required dtype/descriptor policy, and empty binding.
 * Effects: writes borrowed canonical owners and validated geometry only.
 * Failure: returns typed graph, weight, dtype, state, shape, or bounds refusal.
 * Boundary: validates existing owners; it neither reads payload nor dispatches work. */
static int runtime_bind_embedding(yvex_engine *engine, unsigned int token_id,
                                  yvex_dtype dtype, int require_descriptor,
                                  const char *where, runtime_embedding_binding *binding,
                                  yvex_error *err)
{
    int status;

    if (!engine || !binding)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, where, "engine and binding are required");
    memset(binding, 0, sizeof(*binding));
    if (!engine->graph)
        return runtime_refuse(err, YVEX_ERR_STATE, where, "runtime graph requires a built graph");
    if (!engine->weight_backend || !engine->weights || (require_descriptor && !engine->tensors))
        return runtime_refuse(err, YVEX_ERR_STATE, where,
                              "runtime graph requires attached weights");
    if (!runtime_graph_has_op(engine->graph, YVEX_OP_EMBED))
        return runtime_refuse(err, YVEX_ERR_UNSUPPORTED, where,
                              "runtime graph requires a planned embed node");
    binding->weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    binding->tensor = engine->tensors ? yvex_tensor_table_find(engine->tensors, "token_embd.weight")
                          : NULL;
    if (!binding->weight || (require_descriptor && !binding->tensor))
        return runtime_refuse(err, YVEX_ERR_UNSUPPORTED, where,
                              "required tensor not found: token_embd.weight");
    if (yvex_weight_dtype(binding->weight) != dtype) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, where,
                        "runtime embedding requires %s token_embd.weight", yvex_dtype_name(dtype));
        return YVEX_ERR_UNSUPPORTED;
    }
    binding->device = yvex_weight_device_tensor(binding->weight);
    if (!binding->device)
        return runtime_refuse(err, YVEX_ERR_STATE, where,
                              "attached token embedding has no backend tensor");
    if (!require_descriptor) {
        if (yvex_device_tensor_rank(binding->device) != 2)
            return runtime_refuse(err, YVEX_ERR_FORMAT, where,
                                  "fixture token embedding geometry is invalid");
        binding->shape.hidden_size = yvex_device_tensor_dims(binding->device)[0];
        binding->shape.vocab_size = yvex_device_tensor_dims(binding->device)[1];
        binding->shape.output_count = binding->shape.hidden_size;
        if (!binding->shape.hidden_size || !binding->shape.vocab_size)
            return runtime_refuse(err, YVEX_ERR_FORMAT, where,
                                  "fixture token embedding dimensions must be non-zero");
        if ((unsigned long long)token_id >= binding->shape.vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, where,
                            "fixture token id %u exceeds embedding vocab size %llu",
                            token_id, binding->shape.vocab_size);
            return YVEX_ERR_BOUNDS;
        }
        if (!yvex_core_u64_mul(binding->shape.hidden_size, sizeof(float),
                               &binding->shape.output_bytes) ||
            binding->shape.output_bytes > (unsigned long long)SIZE_MAX)
            return runtime_refuse(err, YVEX_ERR_BOUNDS, where,
                                  "fixture graph output is too large for host readback");
        binding->shape.slice_bytes = binding->shape.output_bytes;
        binding->shape.shape_valid = 1;
        return YVEX_OK;
    }
    status = yvex_selected_embedding_shape_validate(binding->tensor, token_id,
                                                     &binding->shape, err);
    if (status != YVEX_OK) return status;
    if (yvex_device_tensor_rank(binding->device) != 2 || binding->tensor->rank != 2 ||
        yvex_device_tensor_dims(binding->device)[0] != binding->shape.hidden_size ||
        yvex_device_tensor_dims(binding->device)[1] != binding->shape.vocab_size ||
        binding->tensor->dims[0] != binding->shape.hidden_size ||
        binding->tensor->dims[1] != binding->shape.vocab_size ||
        binding->tensor->storage_bytes != yvex_weight_bytes(binding->weight))
        return runtime_refuse(err, YVEX_ERR_FORMAT, where,
                              "attached token embedding shape does not match tensor descriptor");
    return YVEX_OK;
}

/* Purpose: validate one token slice against the admitted artifact and materialized weight. */
static int runtime_bind_embedding_range(yvex_engine *engine,
                                        const runtime_embedding_binding *binding,
                                        unsigned int token_id, yvex_tensor_range *range,
                                        yvex_tensor_slice_range *slice, yvex_error *err)
{
    int status = yvex_tensor_range_validate(
        engine->artifact, engine->gguf, binding->tensor, range, err);

    if (status != YVEX_OK) {
        return status;
    }
    if (range->tensor_bytes != yvex_weight_bytes(binding->weight))
        return runtime_refuse(err, YVEX_ERR_FORMAT, "runtime.embedding.range",
                              "attached token embedding byte count does not match validated range");
    return yvex_tensor_embedding_slice_range_validate(range, token_id, slice, err);
}

/* Purpose: Implement the canonical mark graph cleanup mechanism owned by the runtime boundary. */
static void runtime_mark_graph_cleanup(int *attempted, const char **status)
{
    if (attempted) {
        *attempted = 1;
    }
    if (status) {
        *status = "pass";
    }
}

/* Purpose: Release the resources owned by free output tensor without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void runtime_free_output_tensor(yvex_backend *backend, yvex_device_tensor **tensor)
{
    if (!backend || !tensor || !*tensor) {
        return;
    }
    yvex_backend_tensor_free(backend, *tensor);
    *tensor = NULL;
}

/* Purpose: release one optional owned host buffer and clear its published address.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void runtime_free_float_buffer(float **buffer)
{
    if (buffer) {
        free(*buffer);
        *buffer = NULL;
    }
}

static const yvex_fixture_graph_result fixture_graph_default = {
    .backend_name = "none", .graph_integrity_guard = "fail", .graph_execution_phase = "preflight",
    .graph_kind = "fixture-embedding", .shape_status = "unchecked", .range_status = "unchecked",
    .slice_range_status = "not-needed", .backend_status = "unchecked",
    .backend_op_status = "unchecked", .cleanup_status = "not-needed", .op_name = "embed",
    .weight_name = "token_embd.weight",
};

static const yvex_partial_graph_result partial_graph_default = {
    .backend_name = "none", .graph_integrity_guard = "fail", .graph_execution_phase = "preflight",
    .graph_kind = "selected-embedding-partial", .shape_status = "unchecked",
    .range_status = "unchecked", .slice_range_status = "unchecked", .backend_status = "unchecked",
    .backend_op_status = "unchecked", .cleanup_status = "not-needed",
    .segment_name = "token-embedding", .weight_name = "token_embd.weight", .weight_dtype = "F16",
    .output_dtype = "F32",
};

static const yvex_segment_graph_result segment_graph_default = {
    .backend_name = "none", .graph_integrity_guard = "fail", .graph_execution_phase = "preflight",
    .graph_kind = "selected-embedding-rmsnorm", .shape_status = "unchecked",
    .range_status = "unchecked", .slice_range_status = "unchecked", .backend_status = "unchecked",
    .backend_op_status = "unchecked", .cleanup_status = "not-needed",
    .segment_name = "embedding-rmsnorm", .token_tensor_name = "token_embd.weight",
    .token_tensor_dtype = "F16", .rmsnorm_tensor_name = "", .rmsnorm_tensor_dtype = "",
    .rmsnorm_epsilon_key = "",
};

/* Selected fixture, partial, and segment graph execution. */

typedef struct {
    yvex_engine *engine;
    const yvex_device_tensor *embedding;
    const yvex_backend_tensor_desc *output_desc;
    unsigned int token_id;
    unsigned long long output_bytes;
    float **readback;
    const char *where;
    const char *readback_error;
    const char **phase;
    int *allocation_attempted;
    unsigned long long *bytes_allocated;
    int *dispatch_attempted;
    int *cleanup_attempted;
    const char **cleanup_status;
} runtime_embedding_execution;

/* Purpose: construct one checked F32 row descriptor shared by bounded graph probes. */
static yvex_backend_tensor_desc runtime_f32_row(const char *name, unsigned long long width,
                                                unsigned long long bytes)
{
    yvex_backend_tensor_desc descriptor = {0};

    descriptor.name = name;
    descriptor.dtype = YVEX_DTYPE_F32;
    descriptor.rank = 2;
    descriptor.dims[0] = 1;
    descriptor.dims[1] = width;
    descriptor.bytes = bytes;
    return descriptor;
}

/* Purpose: bind common embedding transaction telemetry to its owning result. */
static runtime_embedding_execution runtime_embedding_job(
    yvex_engine *engine, const runtime_embedding_binding *binding,
    const yvex_backend_tensor_desc *descriptor, unsigned int token_id,
    float **readback, const char *where, const char *readback_error,
    const char **phase, int *allocation_attempted,
    unsigned long long *bytes_allocated, int *dispatch_attempted,
    int *cleanup_attempted, const char **cleanup_status)
{
    runtime_embedding_execution job = {
        engine, binding->device, descriptor, token_id, descriptor->bytes, readback,
        where, readback_error, phase, allocation_attempted, bytes_allocated,
        dispatch_attempted, cleanup_attempted, cleanup_status,
    };

    return job;
}

/* Purpose: execute the shared bounded embedding allocation/dispatch/read transaction.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int runtime_embedding_execute(runtime_embedding_execution *job, int allocate_readback,
                                     yvex_error *err)
{
    yvex_device_tensor *output = NULL;
    int status;

    *job->phase = "output";
    *job->allocation_attempted = 1;
    status = yvex_backend_tensor_alloc(job->engine->weight_backend, job->output_desc, &output, err);
    if (status != YVEX_OK) {
        return status;
    }
    *job->bytes_allocated = job->output_bytes;
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC")) {
        status = YVEX_ERR_BACKEND;
        yvex_error_set(err, status, job->where, "test graph failure after output allocation");
        goto fail;
    }
    *job->phase = "dispatch";
    *job->dispatch_attempted = 1;
    status = yvex_backend_op_embed(job->engine->weight_backend, job->embedding,
                                   &job->token_id, 1, output, err);
    if (status != YVEX_OK) {
        goto fail;
    }
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH")) {
        status = YVEX_ERR_BACKEND;
        yvex_error_set(err, status, job->where, "test graph failure after dispatch");
        goto fail;
    }
    if (allocate_readback) {
        *job->readback = (float *)malloc((size_t)job->output_bytes);
        if (!*job->readback) {
            status = YVEX_ERR_NOMEM;
            yvex_error_set(err, status, job->where, job->readback_error);
            goto fail;
        }
    }
    status = yvex_backend_tensor_read(job->engine->weight_backend, output,
                                      *job->readback, job->output_bytes, err);
    if (status != YVEX_OK) {
        goto fail;
    }
    runtime_free_output_tensor(job->engine->weight_backend, &output);
    return YVEX_OK;

fail:
    runtime_free_output_tensor(job->engine->weight_backend, &output);
    runtime_mark_graph_cleanup(job->cleanup_attempted, job->cleanup_status);
    return status;
}

/* Purpose: Execute the typed engine execute fixture graph operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_execute_fixture_graph(yvex_engine *engine,
                                      const yvex_fixture_graph_options *options,
                                      yvex_fixture_graph_result *out, yvex_error *err)
{
    runtime_embedding_binding binding;
    yvex_backend_tensor_desc output_desc;
    runtime_embedding_execution execution;
    unsigned int token_id = 0;
    unsigned long long output_count;
    unsigned long long output_bytes;
    float *readback = NULL;
    int rc;

    if (!engine || !out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_fixture_graph",
                              "engine and out are required");
    *out = fixture_graph_default;

    if (options) {
        token_id = options->token_id;
    }
    out->token_id = token_id;

    rc = runtime_bind_embedding(engine, token_id, YVEX_DTYPE_F32, 0,
                                "yvex_engine_execute_fixture_graph", &binding, err);
    if (rc != YVEX_OK) return rc;
    out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    out->backend_status = yvex_backend_status_name(yvex_backend_status_of(engine->weight_backend));
    out->shape_status = "pass";
    out->range_status = "pass";
    output_count = binding.shape.output_count;
    output_bytes = binding.shape.output_bytes;
    out->output_bytes_planned = output_bytes;
    output_desc = runtime_f32_row("fixture.embed.output", binding.shape.hidden_size, output_bytes);

    if (!yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        out->backend_op_status = "unsupported";
        return runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                              "fixture graph backend does not support embed op");
    }
    out->backend_op_status = "supported";
    execution = runtime_embedding_job( engine, &binding, &output_desc, token_id, &readback,
        "yvex_engine_execute_fixture_graph", "failed to allocate fixture output readback",
        &out->graph_execution_phase, &out->output_allocation_attempted,
        &out->output_bytes_allocated, &out->dispatch_attempted,
        &out->cleanup_attempted, &out->cleanup_status);
    rc = runtime_embedding_execute(&execution, 1, err);
    if (rc != YVEX_OK) {
        free(readback);
        return rc;
    }

    out->executed = 1;
    out->node_count = 1;
    out->output_count = output_count;
    out->output_bytes = output_bytes;
    out->output_checksum = fixture_checksum_bytes((const unsigned char *)readback, output_bytes);
    out->output_value_count = runtime_sample_f32(out->output_values,
                                                 YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES,
                                                 readback, output_count);

    free(readback);
    out->graph_integrity_guard = "pass";
    out->graph_execution_phase = "complete";
    out->cleanup_status = "not-needed";
    yvex_error_clear(err);
    return YVEX_OK;
}

typedef struct {
    yvex_engine *engine;
    yvex_partial_graph_result *out;
    yvex_error *err;
    runtime_embedding_binding embedding;
    yvex_backend_tensor_desc output_desc;
    yvex_device_tensor *output;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    float *readback;
    float *reference;
    unsigned int token_id;
    unsigned long long hidden_size;
    unsigned long long output_count;
    unsigned long long output_bytes;
} partial_graph_ctx;

/* Purpose: Release the resources owned by partial graph release without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_release(partial_graph_ctx *ctx, int status, int mark_cleanup)
{
    runtime_free_output_tensor(ctx->engine->weight_backend, &ctx->output);
    runtime_free_float_buffer(&ctx->readback);
    runtime_free_float_buffer(&ctx->reference);
    if (mark_cleanup)
        runtime_mark_graph_cleanup(&ctx->out->cleanup_attempted, &ctx->out->cleanup_status);
    return status;
}

/* Purpose: Implement the canonical partial graph prepare mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_prepare(partial_graph_ctx *ctx)
{
    int status;

    status = runtime_bind_embedding( ctx->engine, ctx->token_id, YVEX_DTYPE_F16, 1,
        "yvex_engine_execute_partial_graph", &ctx->embedding, ctx->err);
    if (status != YVEX_OK) {
        ctx->out->shape_status = "fail";
        ctx->out->slice_range_status = "fail";
        if (strstr(yvex_error_message(ctx->err), "token-out-of-range"))
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                            "partial token out of range: %u >= %llu",
                            ctx->token_id, ctx->embedding.shape.vocab_size);
        return status;
    }
    ctx->out->backend_name =
        yvex_backend_kind_name(yvex_backend_kind_of(ctx->engine->weight_backend));
    ctx->out->backend_status =
        yvex_backend_status_name(yvex_backend_status_of(ctx->engine->weight_backend));
    ctx->out->shape_status = "pass";
    ctx->hidden_size = ctx->embedding.shape.hidden_size;
    status = runtime_bind_embedding_range( ctx->engine, &ctx->embedding, ctx->token_id,
        &ctx->tensor_range, &ctx->slice_range, ctx->err);
    if (status != YVEX_OK) {
        if (!ctx->tensor_range.range_valid) {
            ctx->out->range_status = "fail";
        } else {
            ctx->out->range_status = "pass";
            ctx->out->slice_range_status = "fail";
        }
        if ((unsigned long long)ctx->token_id >= ctx->embedding.shape.vocab_size)
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                            "partial token out of range: %u >= %llu",
                            ctx->token_id, ctx->embedding.shape.vocab_size);
        return status;
    }
    ctx->out->range_status = "pass";
    ctx->out->slice_range_status = "pass";
    if (ctx->embedding.shape.output_bytes > (unsigned long long)SIZE_MAX)
        return runtime_refuse(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                              "partial graph output is too large for host readback");
    ctx->output_count = ctx->embedding.shape.output_count;
    ctx->output_bytes = ctx->embedding.shape.output_bytes;
    ctx->out->output_bytes_planned = ctx->output_bytes;
    ctx->out->reference_bytes_planned = ctx->slice_range.slice_bytes;
    ctx->output_desc = runtime_f32_row("partial.token_embedding.output", ctx->hidden_size,
                                       ctx->output_bytes);
    if (!yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        ctx->out->backend_op_status = "unsupported";
        return runtime_refuse(ctx->err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_partial_graph",
                              "real partial graph backend does not support embed op");
    }
    ctx->out->backend_op_status = "supported";
    return YVEX_OK;
}

/* Purpose: Execute the typed partial graph execute operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_execute(partial_graph_ctx *ctx)
{
    runtime_embedding_execution execution;
    int status;

    ctx->readback = (float *)malloc((size_t)ctx->output_bytes);
    ctx->reference = (float *)malloc((size_t)ctx->output_bytes);
    if (!ctx->readback || !ctx->reference) {
        status = runtime_refuse(ctx->err, YVEX_ERR_NOMEM, "yvex_engine_execute_partial_graph",
                                "failed to allocate partial graph readback buffers");
        return partial_graph_release(ctx, status, 0);
    }
    ctx->out->graph_execution_phase = "reference";
    ctx->out->reference_read_attempted = 1;
    status = build_f16_embedding_reference(
        ctx->engine->artifact, &ctx->tensor_range, &ctx->slice_range, ctx->reference, ctx->err);
    if (status != YVEX_OK)
        return partial_graph_release(ctx, status, 0);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_REFERENCE_READ")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_partial_graph",
                       "test graph failure after reference read");
        return partial_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    execution = runtime_embedding_job(
        ctx->engine, &ctx->embedding, &ctx->output_desc, ctx->token_id, &ctx->readback,
        "yvex_engine_execute_partial_graph", "failed to allocate partial graph readback buffer",
        &ctx->out->graph_execution_phase, &ctx->out->output_allocation_attempted,
        &ctx->out->output_bytes_allocated, &ctx->out->dispatch_attempted,
        &ctx->out->cleanup_attempted, &ctx->out->cleanup_status);
    status = runtime_embedding_execute(&execution, 0, ctx->err);
    if (status != YVEX_OK) {
        return partial_graph_release(ctx, status, 0);
    }
    ctx->out->executed = 1;
    ctx->out->node_count = 1;
    ctx->out->output_count = ctx->output_count;
    ctx->out->output_bytes = ctx->output_bytes;
    ctx->out->output_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->readback, ctx->output_bytes);
    ctx->out->reference_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->reference, ctx->output_bytes);
    ctx->out->max_abs_diff = max_abs_diff_f32( ctx->readback, ctx->reference, ctx->output_count);
    if (ctx->out->max_abs_diff != 0.0) {
        yvex_error_setf(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_partial_graph",
                        "partial graph reference comparison failed: max_abs_diff %.9g",
                        ctx->out->max_abs_diff);
        return partial_graph_release(ctx, YVEX_ERR_FORMAT, 1);
    }
    ctx->out->output_value_count = runtime_sample_f32(
        ctx->out->output_values, YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES,
        ctx->readback, ctx->output_count);
    partial_graph_release(ctx, YVEX_OK, 0);
    ctx->out->graph_integrity_guard = "pass";
    ctx->out->graph_execution_phase = "complete";
    ctx->out->cleanup_status = "not-needed";
    yvex_error_clear(ctx->err);
    return YVEX_OK;
}

/* Purpose: Execute the typed engine execute partial graph operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_execute_partial_graph(yvex_engine *engine,
                                      const yvex_partial_graph_options *options,
                                      yvex_partial_graph_result *out, yvex_error *err)
{
    partial_graph_ctx ctx;
    int status;

    if (!engine || !out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                              "engine and out are required");
    memset(&ctx, 0, sizeof(ctx));
    *out = partial_graph_default;
    ctx.engine = engine;
    ctx.out = out;
    ctx.err = err;
    ctx.token_id = options ? options->token_id : 0u;
    out->token_id = ctx.token_id;
    status = partial_graph_prepare(&ctx);
    return status == YVEX_OK ? partial_graph_execute(&ctx) : status;
}

typedef struct {
    yvex_engine *engine;
    yvex_segment_graph_result *out;
    yvex_error *err;
    runtime_embedding_binding embedding;
    const yvex_materialized_weight *norm_weight;
    const yvex_tensor_info *norm_tensor;
    const yvex_device_tensor *norm_weight_tensor;
    yvex_backend_tensor_desc embed_desc;
    yvex_backend_tensor_desc output_desc;
    yvex_device_tensor *embed_output;
    yvex_device_tensor *segment_output;
    yvex_tensor_range token_range;
    yvex_tensor_range norm_range;
    yvex_tensor_slice_range slice_range;
    const char *epsilon_key;
    double epsilon;
    unsigned int token_id;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_bytes;
    unsigned long long planned_alloc_bytes;
    float *embedding_reference;
    float *norm_reference;
    float *segment_reference;
    float *readback;
} segment_graph_ctx;

enum { SEGMENT_GRAPH_GEOMETRY_FIELDS = 9 };

_Static_assert(offsetof(yvex_segment_graph_result, segment_reference_bytes) +
                       sizeof(unsigned long long) -
                       offsetof(yvex_segment_graph_result, hidden_size) ==
                   SEGMENT_GRAPH_GEOMETRY_FIELDS * sizeof(unsigned long long),
               "segment graph geometry must remain a contiguous typed projection");

/* Purpose: Release the resources owned by segment graph release without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_release(segment_graph_ctx *ctx, int status, int mark_cleanup)
{
    runtime_free_output_tensor(ctx->engine->weight_backend, &ctx->segment_output);
    runtime_free_output_tensor(ctx->engine->weight_backend, &ctx->embed_output);
    runtime_free_float_buffer(&ctx->embedding_reference);
    runtime_free_float_buffer(&ctx->norm_reference);
    runtime_free_float_buffer(&ctx->segment_reference);
    runtime_free_float_buffer(&ctx->readback);
    if (mark_cleanup)
        runtime_mark_graph_cleanup(&ctx->out->cleanup_attempted, &ctx->out->cleanup_status);
    return status;
}

/* Purpose: Implement the canonical segment graph admit mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_admit(segment_graph_ctx *ctx, const char *segment_name)
{
    int status;

    if (segment_name && strcmp(segment_name, "embedding-rmsnorm") != 0)
        return runtime_refuse(ctx->err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                              "unsupported segment; expected embedding-rmsnorm");
    status = runtime_bind_embedding( ctx->engine, ctx->token_id, YVEX_DTYPE_F16, 1,
        "yvex_engine_execute_segment_graph", &ctx->embedding, ctx->err);
    if (status != YVEX_OK) {
        int out_of_range = strstr(yvex_error_message(ctx->err), "token-out-of-range") != NULL;
        ctx->out->shape_status = out_of_range ? "pass" : "fail";
        ctx->out->slice_range_status = "fail";
        if (out_of_range)
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                            "partial token out of range: %u >= %llu",
                            ctx->token_id, ctx->embedding.shape.vocab_size);
        return status;
    }
    ctx->out->backend_name =
        yvex_backend_kind_name(yvex_backend_kind_of(ctx->engine->weight_backend));
    ctx->out->backend_status =
        yvex_backend_status_name(yvex_backend_status_of(ctx->engine->weight_backend));
    ctx->hidden_size = ctx->embedding.shape.hidden_size;
    ctx->vocab_size = ctx->embedding.shape.vocab_size;
    return YVEX_OK;
}

/* Purpose: Implement the canonical segment graph bind norm mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_bind_norm(segment_graph_ctx *ctx)
{
    ctx->norm_tensor = runtime_find_first_rmsnorm_tensor(ctx->engine->tensors);
    if (!ctx->norm_tensor) {
        ctx->out->shape_status = "fail";
        return runtime_refuse(ctx->err, YVEX_ERR_UNSUPPORTED,
                              "yvex_engine_execute_segment_graph", "rmsnorm-tensor-missing");
    }
    ctx->out->rmsnorm_tensor_name = ctx->norm_tensor->name;
    ctx->out->rmsnorm_tensor_dtype = yvex_dtype_name(ctx->norm_tensor->dtype);
    ctx->norm_weight = yvex_weight_table_find(ctx->engine->weights, ctx->norm_tensor->name);
    if (!ctx->norm_weight) {
        ctx->out->shape_status = "fail";
        return runtime_refuse(ctx->err, YVEX_ERR_UNSUPPORTED,
                              "yvex_engine_execute_segment_graph", "rmsnorm-tensor-missing");
    }
    ctx->norm_weight_tensor = yvex_weight_device_tensor(ctx->norm_weight);
    if (!ctx->norm_weight_tensor)
        return runtime_refuse(ctx->err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                              "attached RMSNorm weight has no backend tensor");
    if (ctx->norm_tensor->rank != 1 || ctx->norm_tensor->dims[0] != ctx->hidden_size ||
        yvex_device_tensor_rank(ctx->norm_weight_tensor) != 1 ||
        yvex_device_tensor_dims(ctx->norm_weight_tensor)[0] != ctx->hidden_size) {
        ctx->out->shape_status = "fail";
        return runtime_refuse(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                              "rmsnorm-shape-invalid");
    }
    if (ctx->norm_tensor->dtype != YVEX_DTYPE_F16 && ctx->norm_tensor->dtype != YVEX_DTYPE_F32) {
        ctx->out->shape_status = "fail";
        return runtime_refuse(ctx->err, YVEX_ERR_UNSUPPORTED,
                              "yvex_engine_execute_segment_graph", "rmsnorm-dtype-invalid");
    }
    ctx->out->shape_status = "pass";
    return YVEX_OK;
}

/* Purpose: Implement the canonical segment graph plan mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_plan(segment_graph_ctx *ctx)
{
    int status;

    status = runtime_bind_embedding_range( ctx->engine, &ctx->embedding, ctx->token_id,
        &ctx->token_range, &ctx->slice_range, ctx->err);
    if (status == YVEX_OK) {
        status = yvex_tensor_range_validate(ctx->engine->artifact, ctx->engine->gguf,
                                            ctx->norm_tensor, &ctx->norm_range, ctx->err);
    }
    if (status != YVEX_OK) {
        if (!ctx->token_range.range_valid || !ctx->norm_range.range_valid) {
            ctx->out->range_status = "fail";
        } else {
            ctx->out->range_status = "pass";
            ctx->out->slice_range_status = "fail";
        }
        return status;
    }
    if (ctx->norm_range.tensor_bytes != yvex_weight_bytes(ctx->norm_weight)) {
        ctx->out->range_status = "fail";
        return runtime_refuse(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                              "attached tensor byte count does not match validated range");
    }
    ctx->out->range_status = "pass";
    ctx->out->slice_range_status = "pass";
    status = runtime_find_rmsnorm_epsilon(
        ctx->engine->gguf, &ctx->epsilon_key, &ctx->epsilon, ctx->err);
    if (status != YVEX_OK)
        return status;
    ctx->out->rmsnorm_epsilon_key = ctx->epsilon_key;
    ctx->out->rmsnorm_epsilon = ctx->epsilon;
    if (yvex_core_test_flag("YVEX_TEST_SEGMENT_MEMORY_PLAN_OVERFLOW") ||
        !yvex_core_u64_mul(ctx->hidden_size, sizeof(float), &ctx->output_bytes) ||
        ctx->output_bytes > (unsigned long long)SIZE_MAX ||
        !yvex_core_u64_add(ctx->output_bytes, ctx->output_bytes, &ctx->planned_alloc_bytes)) {
        return runtime_refuse(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                              "segment-memory-plan-overflow");
    }
    const unsigned long long geometry[SEGMENT_GRAPH_GEOMETRY_FIELDS] = {
        ctx->hidden_size, ctx->vocab_size, 2, 1, ctx->output_bytes,
        ctx->hidden_size, ctx->output_bytes, ctx->output_bytes, ctx->output_bytes,
    };
    memcpy(&ctx->out->hidden_size, geometry, sizeof(geometry));
    ctx->out->output_bytes_planned = ctx->planned_alloc_bytes;
    ctx->out->reference_bytes_planned = ctx->output_bytes;
    if (!yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        !yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_RMS_NORM) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        ctx->out->backend_op_status = "unsupported";
        return runtime_refuse(ctx->err, YVEX_ERR_UNSUPPORTED,
                              "yvex_engine_execute_segment_graph", "backend-op-unsupported");
    }
    ctx->out->backend_op_status = "supported";
    return YVEX_OK;
}

/* Purpose: Implement the canonical segment graph reference mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_reference(segment_graph_ctx *ctx)
{
    int status;

    ctx->embedding_reference = (float *)malloc((size_t)ctx->output_bytes);
    ctx->norm_reference = (float *)malloc((size_t)ctx->output_bytes);
    ctx->segment_reference = (float *)malloc((size_t)ctx->output_bytes);
    ctx->readback = (float *)malloc((size_t)ctx->output_bytes);
    if (!ctx->embedding_reference || !ctx->norm_reference ||
        !ctx->segment_reference || !ctx->readback) {
        yvex_error_set(ctx->err, YVEX_ERR_NOMEM, "yvex_engine_execute_segment_graph",
                       "failed to allocate segment reference/readback buffers");
        return segment_graph_release(ctx, YVEX_ERR_NOMEM, 0);
    }
    ctx->out->graph_execution_phase = "reference";
    ctx->out->reference_read_attempted = 1;
    status = build_f16_embedding_reference(
        ctx->engine->artifact, &ctx->token_range, &ctx->slice_range,
        ctx->embedding_reference, ctx->err);
    if (status == YVEX_OK)
        status = build_rmsnorm_weight_reference(
            ctx->engine->artifact, &ctx->norm_range, ctx->norm_reference, ctx->err);
    if (status != YVEX_OK)
        return segment_graph_release(ctx, status, 0);
    build_rmsnorm_reference(ctx->embedding_reference, ctx->norm_reference,
                            ctx->hidden_size, ctx->epsilon, ctx->segment_reference);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_REFERENCE_READ")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after reference read");
        return segment_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    return YVEX_OK;
}

/* Purpose: Reserve budgeted storage for segment graph allocate outputs with checked size accounting.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_allocate_outputs(segment_graph_ctx *ctx)
{
    int status;

    ctx->embed_desc = runtime_f32_row("segment.embedding.output", ctx->hidden_size,
                                      ctx->output_bytes);
    ctx->output_desc = runtime_f32_row("segment.rmsnorm.output", ctx->hidden_size,
                                       ctx->output_bytes);
    ctx->out->graph_execution_phase = "output";
    ctx->out->output_allocation_attempted = 1;
    status = yvex_backend_tensor_alloc(ctx->engine->weight_backend,
                                       &ctx->embed_desc, &ctx->embed_output, ctx->err);
    if (status != YVEX_OK)
        return segment_graph_release(ctx, status, 0);
    ctx->out->output_bytes_allocated = ctx->output_bytes;
    status = yvex_backend_tensor_alloc(ctx->engine->weight_backend,
                                       &ctx->output_desc, &ctx->segment_output, ctx->err);
    if (status != YVEX_OK)
        return segment_graph_release(ctx, status, 1);
    ctx->out->output_bytes_allocated = ctx->planned_alloc_bytes;
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_OUTPUT_ALLOC")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after output allocation");
        return segment_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    return YVEX_OK;
}

/* Purpose: Implement the canonical segment graph dispatch mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_dispatch(segment_graph_ctx *ctx)
{
    int status;

    ctx->out->graph_execution_phase = "dispatch";
    ctx->out->dispatch_attempted = 1;
    status = yvex_backend_op_embed(ctx->engine->weight_backend, ctx->embedding.device,
                                   &ctx->token_id, 1, ctx->embed_output, ctx->err);
    if (status != YVEX_OK)
        return segment_graph_release(ctx, status, 1);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_OP0") ||
        yvex_core_test_flag("YVEX_TEST_FAIL_SEGMENT_AFTER_OP0")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after segment op 0");
        return segment_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    status = yvex_backend_op_rms_norm(
        ctx->engine->weight_backend, ctx->embed_output, ctx->norm_weight_tensor,
        (float)ctx->epsilon, ctx->segment_output, ctx->err);
    if (status != YVEX_OK)
        return segment_graph_release(ctx, status, 1);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_DISPATCH")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_segment_graph",
                       "test graph failure after dispatch");
        return segment_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    status = yvex_backend_tensor_read(ctx->engine->weight_backend,
                                      ctx->segment_output, ctx->readback,
                                      ctx->output_bytes, ctx->err);
    return status == YVEX_OK ? YVEX_OK : segment_graph_release(ctx, status, 1);
}

/* Purpose: Implement the canonical segment graph finish mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_finish(segment_graph_ctx *ctx)
{
    ctx->out->executed = 1;
    ctx->out->node_count = 2;
    ctx->out->output_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->readback, ctx->output_bytes);
    ctx->out->reference_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->segment_reference, ctx->output_bytes);
    ctx->out->max_abs_diff = max_abs_diff_f32(
        ctx->readback, ctx->segment_reference, ctx->hidden_size);
    if (ctx->out->max_abs_diff > 0.0001) {
        yvex_error_setf(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                        "segment graph reference comparison failed: max_abs_diff %.9g",
                        ctx->out->max_abs_diff);
        return segment_graph_release(ctx, YVEX_ERR_FORMAT, 1);
    }
    ctx->out->output_value_count = runtime_sample_f32(
        ctx->out->output_values, YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES,
        ctx->readback, ctx->hidden_size);
    segment_graph_release(ctx, YVEX_OK, 0);
    ctx->out->graph_integrity_guard = "pass";
    ctx->out->graph_execution_phase = "complete";
    ctx->out->cleanup_status = "not-needed";
    yvex_error_clear(ctx->err);
    return YVEX_OK;
}

/* Purpose: Execute the typed engine execute segment graph operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_execute_segment_graph(yvex_engine *engine,
                                      const yvex_segment_graph_options *options,
                                      yvex_segment_graph_result *out, yvex_error *err)
{
    segment_graph_ctx ctx;
    const char *segment_name = options ? options->segment_name : NULL;
    int status;

    if (!engine || !out)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                              "engine and out are required");
    memset(&ctx, 0, sizeof(ctx));
    *out = segment_graph_default;
    ctx.engine = engine;
    ctx.out = out;
    ctx.err = err;
    ctx.token_id = options ? options->token_id : 0u;
    out->token_id = ctx.token_id;
    status = segment_graph_admit(&ctx, segment_name);
    if (status == YVEX_OK) status = segment_graph_bind_norm(&ctx);
    if (status == YVEX_OK) status = segment_graph_plan(&ctx);
    if (status == YVEX_OK) status = segment_graph_reference(&ctx);
    if (status == YVEX_OK) status = segment_graph_allocate_outputs(&ctx);
    if (status == YVEX_OK) status = segment_graph_dispatch(&ctx);
    return status == YVEX_OK ? segment_graph_finish(&ctx) : status;
}

typedef struct {
    const yvex_model_family_api *model;
    const yvex_graph_family_api *graph;
    yvex_deepseek_payload_handoff *handoff;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_complete_artifact_admission admission;
    yvex_materialization_plan *materialization_plan;
    yvex_materialization_session *session;
    yvex_deepseek_v4_ir *architecture;
    yvex_runtime_descriptor *descriptor;
    yvex_attention_plan *attention_plan;
    yvex_quant_plan *quant_plan;
    yvex_gguf_writer_plan *writer_plan;
    yvex_artifact_physical_compatibility compatibility;
    yvex_artifact_compatibility_failure compatibility_failure;
    yvex_attention_failure attention_failure;
    yvex_attention_probe_result probe_result;
    yvex_deepseek_payload_handoff_options payload_options;
    yvex_deepseek_payload_failure payload_failure;
    yvex_artifact_options artifact_options;
    yvex_artifact_admission_failure admission_failure;
    yvex_materialization_options materialization_options;
    yvex_materialization_failure materialization_failure;
    yvex_runtime_descriptor_failure descriptor_failure;
    yvex_deepseek_v4_ir_failure architecture_failure;
    yvex_quant_failure quant_failure;
    yvex_gguf_writer_failure writer_failure;
    yvex_gguf_writer_plan_options writer_options;
} runtime_attention_context;

enum {
    RUNTIME_ATTENTION_COMPATIBILITY_BYTES =
        offsetof(yvex_artifact_physical_compatibility, payload_digest_equal) + sizeof(int) -
        offsetof(yvex_artifact_physical_compatibility, physical_payload_compatible),
    RUNTIME_ATTENTION_PROBE_IDENTITY_BYTES = offsetof(yvex_attention_probe_result, cuda_device) -
        offsetof(yvex_attention_probe_result, attention_execution_identity),
    RUNTIME_ATTENTION_PROBE_METRIC_BYTES =
        offsetof(yvex_attention_probe_result, bitwise_equality_required) + sizeof(int) -
        offsetof(yvex_attention_probe_result, swa_layers_executed),
};

_Static_assert( offsetof(yvex_graph_attention_operator_result, attention_execution_supported) -
            offsetof(yvex_graph_attention_operator_result, physical_payload_compatible) ==
        RUNTIME_ATTENTION_COMPATIBILITY_BYTES,
    "physical compatibility result layout must remain aligned");
_Static_assert( offsetof(yvex_graph_attention_operator_result, current_writer_plan_identity) -
            offsetof(yvex_graph_attention_operator_result, attention_execution_identity) ==
        RUNTIME_ATTENTION_PROBE_IDENTITY_BYTES, "probe identity result layout must remain aligned");
_Static_assert( offsetof(yvex_graph_attention_operator_result, physical_payload_compatible) -
            offsetof(yvex_graph_attention_operator_result, swa_layers_executed) ==
        RUNTIME_ATTENTION_PROBE_METRIC_BYTES, "probe metric result layout must remain aligned");
_Static_assert(offsetof(yvex_graph_attention_operator_request, cancel_context) + sizeof(void *) -
                       offsetof(yvex_graph_attention_operator_request, backend) ==
                   sizeof(yvex_attention_probe_request),
               "operator request must preserve the production probe projection");

/* Purpose: release every operator attention dependency in reverse order.
 * Inputs: partially or fully initialized context.
 * Effects: closes only objects owned by the context and clears every pointer.
 * Failure: close contracts remain deterministic and never remove source/artifact files.
 * Boundary: cleanup publishes no higher capability. */
static void runtime_attention_context_close(runtime_attention_context *context)
{
    if (!context) return;
    yvex_gguf_writer_plan_release(&context->writer_plan);
    yvex_quant_plan_release(&context->quant_plan);
    context->graph->plan_close(context->attention_plan);
    yvex_runtime_descriptor_close(context->descriptor);
    context->model->ir.close(context->architecture);
    yvex_materialization_session_close(context->session);
    yvex_materialization_plan_close(context->materialization_plan);
    yvex_tensor_table_close(context->tensors);
    yvex_gguf_close(context->gguf);
    yvex_artifact_close(context->artifact);
    context->model->payload.close(context->handoff);
    memset(context, 0, sizeof(*context));
}

/* Purpose: validate explicit operator attention inputs before filesystem access.
 * Inputs: immutable request.
 * Effects: none.
 * Failure: rejects malformed target, path, probe, scope, or backend.
 * Boundary: validation never substitutes a backend, scope, or artifact. */
static int runtime_attention_request_validate( const yvex_graph_attention_operator_request *request,
    yvex_error *err)
{
    if (!request || !request->target)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "runtime.attention",
                              "attention target is required");
    if (!yvex_source_is_release_target(request->target)) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "runtime.attention",
                        "unsupported attention target: %s", request->target);
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!request->source_path || !request->models_root || !request->source_manifest_path ||
        !request->artifact_path || !request->source_path[0] || !request->models_root[0] ||
        !request->source_manifest_path[0] || !request->artifact_path[0])
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "runtime.attention",
                              "explicit canonical source and artifact paths are required");
    if (request->probe != YVEX_ATTENTION_PROBE_CANONICAL ||
        (request->scope != YVEX_ATTENTION_PROBE_SCOPE_QUICK &&
         request->scope != YVEX_ATTENTION_PROBE_SCOPE_FULL))
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "runtime.attention",
                              "canonical probe and quick/full scope are required");
    if (!request->compare_backends && request->backend != YVEX_BACKEND_KIND_CPU &&
        request->backend != YVEX_BACKEND_KIND_CUDA)
        return runtime_refuse(err, YVEX_ERR_UNSUPPORTED, "runtime.attention",
                              "attention backend must be cpu or cuda");
    return YVEX_OK;
}

/* Purpose: open verified source ownership and the admitted external artifact.
 * Inputs: validated request and empty context.
 * Effects: owns handoff, artifact, GGUF reader, tensor table, and admission.
 * Failure: partial context remains reverse-release safe and artifact bytes are unchanged.
 * Boundary: source verification supplies typed truth, not prompt input. */
static int runtime_attention_open_inputs( runtime_attention_context *context,
    const yvex_graph_attention_operator_request *request, yvex_error *err)
{
    int rc;

    context->payload_options.source_path = request->source_path;
    context->payload_options.models_root = request->models_root;
    context->payload_options.manifest_path = request->source_manifest_path;
    yvex_source_payload_budget_default(&context->payload_options.budget);
    context->payload_options.budget.maximum_open_handles = 32u;
    context->payload_options.budget.maximum_streams = 16u;
    context->payload_options.budget.maximum_inflight_host_bytes =
        context->payload_options.budget.chunk_bytes *
        context->payload_options.budget.maximum_streams;
    context->payload_options.chunk_bytes = context->payload_options.budget.chunk_bytes;
    context->payload_options.page_bytes = context->payload_options.budget.page_bytes;
    rc = context->model->payload.open(
        &context->handoff, &context->payload_options, &context->payload_failure, err);
    if (rc != YVEX_OK) return rc;
    context->artifact_options.path = request->artifact_path;
    context->artifact_options.readonly = 1;
    rc = yvex_artifact_open(&context->artifact, &context->artifact_options, err);
    if (rc == YVEX_OK && yvex_artifact_size(context->artifact) !=
            YVEX_SELECTED_DEEPSEEK_FILE_BYTES) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "runtime.attention",
                        "selected artifact size mismatch: expected %llu, got %llu",
                        YVEX_SELECTED_DEEPSEEK_FILE_BYTES, yvex_artifact_size(context->artifact));
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK)
        rc = yvex_gguf_open(&context->gguf, context->artifact, err);
    if (rc == YVEX_OK)
        rc = yvex_tensor_table_from_gguf(&context->tensors, context->gguf, err);
    if (rc == YVEX_OK)
        rc = yvex_artifact_admit_deepseek(
            context->artifact, &context->admission, &context->admission_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_artifact_admission_identity_verify(
            context->artifact, &context->admission, &context->admission_failure, err);
    return rc;
}

/* Purpose: derive committed materialization plus semantic and physical execution plans.
 * Inputs: admitted artifact context and verified DeepSeek handoff.
 * Effects: owns the session, architecture, descriptor, attention, quant, and writer plans.
 * Failure: caller unwinds all partially constructed owners.
 * Boundary: planning reads no payload and does not create persistent runtime KV or a new GGUF. */
static int runtime_attention_open_plans(runtime_attention_context *context, yvex_error *err)
{
    int rc;

    yvex_materialization_options_default(&context->materialization_options);
    context->materialization_options.require_deepseek_map = 1;
    context->materialization_options.max_chunk_bytes = 16ull * 1024ull * 1024ull;
    context->materialization_options.cache_budget_bytes = 256ull * 1024ull * 1024ull;
    context->materialization_options.future_graph_scratch_reserve_bytes =
        2ull * 1024ull * 1024ull * 1024ull;
    context->materialization_options.future_kv_reserve_bytes = 2ull * 1024ull * 1024ull * 1024ull;
    rc = yvex_materialization_plan_build( &context->materialization_plan, &context->admission,
        context->artifact, context->gguf, context->tensors,
        context->model->payload.map(context->handoff),
        &context->materialization_options, &context->materialization_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_materialization_session_open( &context->session, context->materialization_plan,
            context->artifact, &context->materialization_options,
            &context->materialization_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_materialization_session_commit(
            context->session, &context->materialization_failure, err);
    if (rc == YVEX_OK)
        rc = context->model->ir.build( &context->architecture,
            context->model->payload.verification(context->handoff),
            &context->architecture_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_descriptor_build_deepseek(
            &context->descriptor, &context->admission, context->session,
            context->model->payload.map(context->handoff),
            context->architecture, &context->descriptor_failure, err);
    if (rc == YVEX_OK)
        rc = context->graph->plan_build(
            &context->attention_plan, context->architecture, context->session,
            context->descriptor, &context->attention_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_quant_plan_build_deepseek_profile(
            &context->quant_plan, context->model->payload.transform_ir(context->handoff),
            context->model->payload.binding(context->handoff),
            context->model->payload.map(context->handoff),
            YVEX_QUANT_PROFILE_RELEASE_Q8_Q2, NULL, &context->quant_failure, err);
    if (rc == YVEX_OK) {
        yvex_gguf_writer_plan_options_default(&context->writer_options);
        context->writer_options.required_execution_identity =
            YVEX_SELECTED_DEEPSEEK_EXECUTION_IDENTITY;
        rc = yvex_gguf_writer_build_deepseek( &context->writer_plan, context->quant_plan,
            context->model->payload.map(context->handoff),
            context->model->payload.verification(context->handoff),
            &context->writer_options, &context->writer_failure, err);
    }
    return rc;
}

/* Purpose: copy one bounded runtime fact while preserving termination. */
static void runtime_attention_copy_text(char *destination, size_t capacity, const char *source)
{
    (void)snprintf(destination, capacity, "%s", source ? source : "");
}

/* Purpose: copy one canonical SHA-256 identity with its fixed contract width. */
static void runtime_attention_copy_identity(char destination[YVEX_SHA256_HEX_CAP],
                                            const char *source)
{
    runtime_attention_copy_text(destination, YVEX_SHA256_HEX_CAP, source);
}

static const yvex_graph_attention_operator_result runtime_attention_result_default = {
    .status = "refused", .command = "graph attention execute", .target = "unavailable",
    .family = "unavailable", .backend = "unavailable", .scope = "unavailable",
    .input_class = "unavailable", .execution_class = "unavailable", .weights_class = "unavailable",
    .first_failing_layer = YVEX_ATTENTION_NO_LAYER,
    .first_failing_coordinate = YVEX_ATTENTION_NO_LAYER, .production_api_available = 1,
    .internal_live_runner_available = 1, .operator_command_available = 1,
};

/* Purpose: seed stable request/reachability facts before I/O.
 * Inputs: optional request and caller-owned result.
 * Effects: publishes a refused default with no fabricated measurements.
 * Failure: bounded formatting leaves a defined result.
 * Boundary: command availability is distinct from execution success. */
static void runtime_attention_result_initialize(
    const yvex_graph_attention_operator_request *request,
    yvex_graph_attention_operator_result *result)
{
    int target_supported;

    *result = runtime_attention_result_default;
    if (!request) return;
    target_supported = request->target && yvex_source_is_release_target(request->target);
    if (request->target)
        runtime_attention_copy_text(result->target, sizeof(result->target), request->target);
    if (target_supported) {
        runtime_attention_copy_text(result->family, sizeof(result->family), "deepseek-v4-flash");
        if (request->probe == YVEX_ATTENTION_PROBE_CANONICAL)
            runtime_attention_copy_text(result->input_class, sizeof(result->input_class),
                                        "canonical_attention_probe");
    }
    if (request->compare_backends)
        runtime_attention_copy_text(result->backend, sizeof(result->backend), "compare");
    else if (request->backend == YVEX_BACKEND_KIND_CPU ||
             request->backend == YVEX_BACKEND_KIND_CUDA)
        runtime_attention_copy_text(result->backend, sizeof(result->backend),
                                    yvex_backend_kind_name(request->backend));
    if (request->scope == YVEX_ATTENTION_PROBE_SCOPE_QUICK ||
        request->scope == YVEX_ATTENTION_PROBE_SCOPE_FULL)
        runtime_attention_copy_text(result->scope, sizeof(result->scope),
                                    request->scope == YVEX_ATTENTION_PROBE_SCOPE_FULL
                                        ? "full" : "quick");
    if (request->artifact_path)
        runtime_attention_copy_text(result->artifact_path, sizeof(result->artifact_path),
                                    request->artifact_path);
}

/* Purpose: bind admitted identities and physical compatibility before execution.
 * Inputs: complete context and successful compatibility proof.
 * Effects: copies immutable summaries and performs zero payload reads.
 * Failure: incomplete summaries return false.
 * Boundary: artifact and current semantic identities remain distinct. */
static int runtime_attention_result_bind( const runtime_attention_context *context,
    const yvex_artifact_physical_compatibility *compatibility,
    yvex_graph_attention_operator_result *result)
{
    const yvex_deepseek_payload_handoff_summary *handoff =
        context->model->payload.summary(context->handoff);
    const yvex_materialization_summary *materialization =
        yvex_materialization_session_summary(context->session);
    const yvex_runtime_descriptor_summary *descriptor =
        yvex_runtime_descriptor_summary_get(context->descriptor);
    const yvex_attention_summary *attention = context->graph->plan_summary(context->attention_plan);
    const yvex_gguf_writer_plan_summary *writer =
        yvex_gguf_writer_plan_summary_get(context->writer_plan);

    if (!handoff || !handoff->complete || !materialization ||
        !materialization->committed || !descriptor || !attention || !writer ||
        !writer->complete || !compatibility || !compatibility->physical_payload_compatible)
        return 0;
    (void)snprintf(result->source_snapshot_identity,
                   sizeof(result->source_snapshot_identity), "%016llx",
                   handoff->source_snapshot_identity);
    runtime_attention_copy_identity(result->payload_identity, context->admission.payload_identity);
    runtime_attention_copy_identity(result->artifact_identity, context->admission.artifact_identity);
    runtime_attention_copy_identity(result->artifact_transform_identity,
                                    context->admission.transform_identity);
    runtime_attention_copy_identity(result->transform_identity, handoff->transform_identity);
    runtime_attention_copy_identity(result->materialization_identity, materialization->plan_identity);
    runtime_attention_copy_identity(result->logical_model_identity, descriptor->logical_model_identity);
    runtime_attention_copy_identity(result->runtime_numeric_identity, descriptor->runtime_numeric_identity);
    runtime_attention_copy_identity(result->runtime_descriptor_identity,
                                    descriptor->runtime_descriptor_identity);
    runtime_attention_copy_identity(result->attention_plan_identity, attention->attention_plan_identity);
    runtime_attention_copy_identity(result->current_writer_plan_identity, writer->writer_plan_identity);
    runtime_attention_copy_identity(result->payload_plan_identity,
                                    compatibility->payload_plan_identity);
    runtime_attention_copy_identity(result->payload_byte_identity,
                                    compatibility->payload_byte_identity);
    result->main_layers_total = attention->layer_count;
    result->bindings_total = attention->required_binding_count;
    memcpy(&result->physical_payload_compatible, &compatibility->physical_payload_compatible,
           RUNTIME_ATTENTION_COMPATIBILITY_BYTES);
    runtime_attention_copy_text(result->execution_class,
                                sizeof(result->execution_class), "production");
    runtime_attention_copy_text(result->weights_class,
                                sizeof(result->weights_class), "admitted_external_artifact");
    result->attention_execution_supported = attention->full_execution_ready;
    result->attention_cuda_execution_ready = attention->cuda_execution_ready;
    result->runtime_generation_ready = descriptor->generation_ready;
    result->artifact_bytes_hashed = context->admission.artifact_bytes_hashed;
    result->artifact_identity_verified = context->admission.artifact_identity_verified;
    return 1;
}

/* Purpose: copy complete graph numerical evidence into the operator result.
 * Inputs: immutable graph probe result and mutable aggregate.
 * Effects: copies identities, counters, CUDA facts, and comparison metrics.
 * Failure: none after graph completion.
 * Boundary: execution evidence remains distinct from artifact provenance. */
static void runtime_attention_result_apply_probe( const yvex_attention_probe_result *probe,
    yvex_graph_attention_operator_result *result)
{
    memcpy(result->attention_execution_identity, probe->attention_execution_identity,
           RUNTIME_ATTENTION_PROBE_IDENTITY_BYTES);
    result->layers_executed = probe->layers_executed;
    result->bindings_executed = probe->bindings_executed;
    memcpy(&result->swa_layers_executed, &probe->swa_layers_executed,
           RUNTIME_ATTENTION_PROBE_METRIC_BYTES);
    memcpy(result->cuda_device, probe->cuda_device, sizeof(result->cuda_device));
}

/* Purpose: retain a typed refusal while leaving execution uncommitted.
 * Inputs: result and authoritative error.
 * Effects: copies a bounded reason and clears completion.
 * Failure: absent error receives a stable generic reason.
 * Boundary: rendering and exit mapping remain CLI-owned. */
static void runtime_attention_result_refuse( yvex_graph_attention_operator_result *result,
    int status, const yvex_error *err)
{
    const char *message = err ? yvex_error_message(err) : "";
    const char *where = err ? yvex_error_where(err) : "";
    yvex_status code = err && yvex_error_is_set(err) ? yvex_error_code(err) : (yvex_status)status;

    result->completed = 0;
    runtime_attention_copy_text(result->status, sizeof(result->status), "refused");
    runtime_attention_copy_text(result->failure_code, sizeof(result->failure_code),
                                yvex_status_name(code));
    runtime_attention_copy_text(result->failure_where, sizeof(result->failure_where),
                                where[0] ? where : "runtime.attention");
    (void)snprintf(result->reason, sizeof(result->reason), "%s %s: %s",
                   yvex_status_name(code), result->failure_where,
                   message[0] ? message : "attention execution refused");
}

/* Purpose: execute operator-reachable attention through admitted production owners.
 * Inputs: explicit paths, backend, canonical probe, and quick/full scope.
 * Effects: owns transient runtime state and publishes only complete graph evidence.
 * Failure: reverse-order cleanup preserves source, artifact, and prior committed state.
 * Boundary: attention probe only; no prompt, persistent KV, transformer, or generation. */
int yvex_graph_attention_operator_execute( const yvex_graph_attention_operator_request *request,
    yvex_graph_attention_operator_result *result, yvex_error *err)
{
    runtime_attention_context context;
    yvex_attention_probe_request probe_request;
    int rc;

    if (!result)
        return runtime_refuse(err, YVEX_ERR_INVALID_ARG, "runtime.attention",
                              "attention result output is required");
    runtime_attention_result_initialize(request, result);
    rc = runtime_attention_request_validate(request, err);
    if (rc != YVEX_OK) {
        runtime_attention_result_refuse(result, rc, err);
        return rc;
    }
    memset(&context, 0, sizeof(context));
    context.model = yvex_model_register_deepseek_v4();
    context.graph = yvex_graph_lower_deepseek_v4();
    memcpy(&probe_request, &request->backend, sizeof(probe_request));
    context.probe_result.first_failing_layer = YVEX_ATTENTION_NO_LAYER;
    context.probe_result.first_failing_coordinate = YVEX_ATTENTION_NO_LAYER;
    rc = runtime_attention_open_inputs(&context, request, err);
    if (rc == YVEX_OK) rc = runtime_attention_open_plans(&context, err);
    if (rc == YVEX_OK)
        rc = yvex_artifact_physical_compatibility_validate(
            context.writer_plan, &context.admission, context.artifact,
            context.gguf, &context.compatibility, &context.compatibility_failure, err);
    if (rc == YVEX_OK && !runtime_attention_result_bind(&context, &context.compatibility, result)) {
        yvex_error_set(err, YVEX_ERR_STATE, "runtime.attention",
                       "admitted attention summaries are incomplete");
        rc = YVEX_ERR_STATE;
    }
    if (rc == YVEX_OK) {
        rc = yvex_attention_probe_execute( context.graph, context.attention_plan,
            context.architecture, context.session, context.descriptor,
            &probe_request, &context.probe_result, &context.attention_failure, err);
        runtime_attention_result_apply_probe(&context.probe_result, result);
    }
    if (rc == YVEX_OK) {
        result->completed = 1;
        runtime_attention_copy_text(result->status, sizeof(result->status), "complete");
        result->reason[0] = '\0';
        yvex_error_clear(err);
    } else {
        runtime_attention_result_refuse(result, rc, err);
    }
    runtime_attention_context_close(&context);
    return rc;
}
