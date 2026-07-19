/* Owner: runtime graph execution evidence.
 * Owns: fixture, partial, and segment graph transactions plus their local scalar comparison and deterministic
 *   accounting helpers.
 * Does not own: engine/session lifecycle, graph planning, production model execution, backend kernels, rendering,
 *   or generation support.
 * Invariants: execution borrows an admitted engine and materialized weights; every failure releases temporary
 *   tensors and buffers; results never promote diagnostic execution to transformer support.
 * Boundary: selected graph evidence is not full transformer or generation execution.
 * Purpose: execute the existing bounded runtime graph proof independently of engine/session lifecycle coordination.
 * Inputs: an opened engine, explicit fixture/segment requests, and owned output result structures.
 * Effects: allocates bounded scratch, invokes admitted backend primitives, and writes only the supplied result
 *   structure.
 * Failure: returns typed errors after deterministic scratch and tensor cleanup. */

#include <yvex/internal/core.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/runtime.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Selected runtime graph reference helpers. */

/* Purpose: Implement the canonical fixture checksum bytes mechanism owned by the runtime boundary. */
static unsigned long long fixture_checksum_bytes(const unsigned char *data,
                                                 unsigned long long len)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        hash ^= (unsigned long long)data[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Purpose: decode admitted little-endian F16 bytes through the canonical codec.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Produces only the deterministic scalar result defined for the admitted numeric domain.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static float runtime_decode_f16(const unsigned char *bytes)
{
    unsigned short bits = (unsigned short)bytes[0] | (unsigned short)((unsigned short)bytes[1] << 8);

    return yvex_quant_f16_decode(bits);
}

/* Purpose: Compute the bounded build F16 embedding reference primitive under the declared dtype and shape contract.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int build_f16_embedding_reference(const yvex_artifact *artifact,
                                         const yvex_tensor_range *range,
                                         const yvex_tensor_slice_range *slice,
                                         float *out,
                                         yvex_error *err)
{
    const unsigned char *data;
    unsigned long long hidden_size;
    unsigned long long slice_offset;
    unsigned long long i;

    if (!artifact || !range || !slice || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "artifact, range, slice, and reference output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!range->range_valid || !slice->range_valid) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                       "partial reference slice range is invalid");
        return YVEX_ERR_BOUNDS;
    }
    hidden_size = range->dims[0];
    slice_offset = slice->slice_absolute_offset;
    data = yvex_artifact_data(artifact);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "artifact data is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }

    for (i = 0; i < hidden_size; ++i) {
        out[i] = runtime_decode_f16(data + slice_offset + (i * 2ull));
    }
    return YVEX_OK;
}

/* Purpose: Implement the canonical sqrt double mechanism owned by the runtime boundary. */
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

/* Purpose: Resolve find rmsnorm epsilon through the canonical indexed ownership boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int runtime_find_rmsnorm_epsilon(const yvex_gguf *gguf,
                                        const char **key_out,
                                        double *epsilon_out,
                                        yvex_error *err)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    if (!gguf || !key_out || !epsilon_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "GGUF, key output, and epsilon output are required");
        return YVEX_ERR_INVALID_ARG;
    }

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

    yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                   "rmsnorm-epsilon-missing");
    return YVEX_ERR_FORMAT;
}

/* Purpose: Resolve find first rmsnorm tensor through the canonical indexed ownership boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static const yvex_tensor_info *runtime_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
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

/* Purpose: Compute the bounded build rmsnorm weight reference primitive under the declared dtype and shape contract.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int build_rmsnorm_weight_reference(const yvex_artifact *artifact,
                                          const yvex_tensor_range *range,
                                          float *out,
                                          yvex_error *err)
{
    const unsigned char *data;
    unsigned long long i;

    if (!artifact || !range || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "artifact, range, and RMSNorm reference output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!range->range_valid || range->rank != 1) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                       "RMSNorm reference tensor range is invalid");
        return YVEX_ERR_BOUNDS;
    }
    if (range->dtype != YVEX_DTYPE_F16 && range->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_segment_graph",
                       "rmsnorm-dtype-invalid");
        return YVEX_ERR_UNSUPPORTED;
    }
    data = yvex_artifact_data(artifact);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "artifact data is unavailable");
        return YVEX_ERR_INVALID_ARG;
    }
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

/* Purpose: Compute the bounded build rmsnorm reference primitive under the declared dtype and shape contract.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void build_rmsnorm_reference(const float *embedding,
                                    const float *weight,
                                    unsigned long long hidden_size,
                                    double epsilon,
                                    float *out)
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

/* Purpose: Implement the canonical max abs diff F32 mechanism owned by the runtime boundary. */
static double max_abs_diff_f32(const float *a, const float *b, unsigned long long count)
{
    unsigned long long i;
    double max_diff = 0.0;

    for (i = 0; i < count; ++i) {
        double diff = (double)a[i] - (double)b[i];
        if (diff < 0.0) {
            diff = -diff;
        }
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
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

/* Purpose: bind the selected F16 token embedding once for every artifact-backed diagnostic graph path.
 * Inputs: admitted engine, token identity, caller label, and empty binding.
 * Effects: writes borrowed canonical owners and validated geometry only.
 * Failure: returns typed graph, weight, dtype, state, shape, or bounds refusal.
 * Boundary: validates existing owners; it neither reads payload nor dispatches work. */
static int runtime_bind_f16_embedding(yvex_engine *engine,
                                      unsigned int token_id,
                                      const char *where,
                                      runtime_embedding_binding *binding,
                                      yvex_error *err)
{
    int status;

    if (!engine || !binding) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "engine and binding are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(binding, 0, sizeof(*binding));
    if (!engine->graph) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "runtime graph requires a built graph");
        return YVEX_ERR_STATE;
    }
    if (!engine->weight_backend || !engine->weights || !engine->tensors) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "runtime graph requires attached weights");
        return YVEX_ERR_STATE;
    }
    if (!runtime_graph_has_op(engine->graph, YVEX_OP_EMBED)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "runtime graph requires a planned embed node");
        return YVEX_ERR_UNSUPPORTED;
    }
    binding->weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    binding->tensor = yvex_tensor_table_find(engine->tensors, "token_embd.weight");
    if (!binding->weight || !binding->tensor) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (yvex_weight_dtype(binding->weight) != YVEX_DTYPE_F16) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "runtime embedding requires F16 token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    binding->device = yvex_weight_device_tensor(binding->weight);
    if (!binding->device) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "attached token embedding has no backend tensor");
        return YVEX_ERR_STATE;
    }
    status = yvex_selected_embedding_shape_validate(
        binding->tensor, token_id, &binding->shape, err);
    if (status != YVEX_OK) {
        return status;
    }
    if (yvex_device_tensor_rank(binding->device) != 2 || binding->tensor->rank != 2 ||
        yvex_device_tensor_dims(binding->device)[0] != binding->shape.hidden_size ||
        yvex_device_tensor_dims(binding->device)[1] != binding->shape.vocab_size ||
        binding->tensor->dims[0] != binding->shape.hidden_size ||
        binding->tensor->dims[1] != binding->shape.vocab_size ||
        binding->tensor->storage_bytes != yvex_weight_bytes(binding->weight)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attached token embedding shape does not match tensor descriptor");
        return YVEX_ERR_FORMAT;
    }
    return YVEX_OK;
}

/* Purpose: validate one token slice against the admitted artifact and materialized weight. */
static int runtime_bind_embedding_range(yvex_engine *engine,
                                        const runtime_embedding_binding *binding,
                                        unsigned int token_id,
                                        yvex_tensor_range *range,
                                        yvex_tensor_slice_range *slice,
                                        yvex_error *err)
{
    int status = yvex_tensor_range_validate(
        engine->artifact, engine->gguf, binding->tensor, range, err);

    if (status != YVEX_OK) {
        return status;
    }
    if (range->tensor_bytes != yvex_weight_bytes(binding->weight)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "runtime.embedding.range",
                       "attached token embedding byte count does not match validated range");
        return YVEX_ERR_FORMAT;
    }
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

/* Purpose: Release the resources owned by free two output tensors without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void runtime_free_two_output_tensors(yvex_backend *backend,
                                            yvex_device_tensor **first,
                                            yvex_device_tensor **second)
{
    runtime_free_output_tensor(backend, second);
    runtime_free_output_tensor(backend, first);
}

/* Purpose: Release the resources owned by free partial buffers without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void runtime_free_partial_buffers(float **readback, float **reference)
{
    if (readback) {
        free(*readback);
        *readback = NULL;
    }
    if (reference) {
        free(*reference);
        *reference = NULL;
    }
}

/* Purpose: Release the resources owned by free segment buffers without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static void runtime_free_segment_buffers(float **embedding_reference,
                                         float **rmsnorm_weight_reference,
                                         float **segment_reference,
                                         float **readback)
{
    if (embedding_reference) {
        free(*embedding_reference);
        *embedding_reference = NULL;
    }
    if (rmsnorm_weight_reference) {
        free(*rmsnorm_weight_reference);
        *rmsnorm_weight_reference = NULL;
    }
    if (segment_reference) {
        free(*segment_reference);
        *segment_reference = NULL;
    }
    if (readback) {
        free(*readback);
        *readback = NULL;
    }
}

static const yvex_fixture_graph_result fixture_graph_default = {
    .backend_name = "none",
    .graph_integrity_guard = "fail",
    .graph_execution_phase = "preflight",
    .graph_kind = "fixture-embedding",
    .shape_status = "unchecked",
    .range_status = "unchecked",
    .slice_range_status = "not-needed",
    .backend_status = "unchecked",
    .backend_op_status = "unchecked",
    .cleanup_status = "not-needed",
    .op_name = "embed",
    .weight_name = "token_embd.weight",
};

static const yvex_partial_graph_result partial_graph_default = {
    .backend_name = "none",
    .graph_integrity_guard = "fail",
    .graph_execution_phase = "preflight",
    .graph_kind = "selected-embedding-partial",
    .shape_status = "unchecked",
    .range_status = "unchecked",
    .slice_range_status = "unchecked",
    .backend_status = "unchecked",
    .backend_op_status = "unchecked",
    .cleanup_status = "not-needed",
    .segment_name = "token-embedding",
    .weight_name = "token_embd.weight",
    .weight_dtype = "F16",
    .output_dtype = "F32",
};

static const yvex_segment_graph_result segment_graph_default = {
    .backend_name = "none",
    .graph_integrity_guard = "fail",
    .graph_execution_phase = "preflight",
    .graph_kind = "selected-embedding-rmsnorm",
    .shape_status = "unchecked",
    .range_status = "unchecked",
    .slice_range_status = "unchecked",
    .backend_status = "unchecked",
    .backend_op_status = "unchecked",
    .cleanup_status = "not-needed",
    .segment_name = "embedding-rmsnorm",
    .token_tensor_name = "token_embd.weight",
    .token_tensor_dtype = "F16",
    .rmsnorm_tensor_name = "",
    .rmsnorm_tensor_dtype = "",
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

/* Purpose: execute the shared bounded embedding allocation/dispatch/read transaction.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int runtime_embedding_execute(runtime_embedding_execution *job,
                                     int allocate_readback,
                                     yvex_error *err)
{
    yvex_device_tensor *output = NULL;
    int status;

    *job->phase = "output";
    *job->allocation_attempted = 1;
    status = yvex_backend_tensor_alloc(job->engine->weight_backend, job->output_desc,
                                       &output, err);
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
                                      yvex_fixture_graph_result *out,
                                      yvex_error *err)
{
    const yvex_materialized_weight *weight;
    const yvex_device_tensor *embedding;
    yvex_backend_tensor_desc output_desc;
    runtime_embedding_execution execution;
    unsigned int token_id = 0;
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long output_count;
    unsigned long long output_bytes;
    unsigned long long i;
    float *readback = NULL;
    int rc;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_fixture_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = fixture_graph_default;

    if (options) {
        token_id = options->token_id;
    }
    out->token_id = token_id;

    if (!engine->graph) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires a built graph");
        return YVEX_ERR_STATE;
    }
    if (!runtime_graph_has_op(engine->graph, YVEX_OP_EMBED)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires a planned embed node");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!engine->weight_backend || !engine->weights) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "fixture graph requires attached weights");
        return YVEX_ERR_STATE;
    }
    out->backend_name = yvex_backend_kind_name(yvex_backend_kind_of(engine->weight_backend));
    out->backend_status = yvex_backend_status_name(
        yvex_backend_status_of(engine->weight_backend));

    weight = yvex_weight_table_find(engine->weights, "token_embd.weight");
    if (!weight) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (yvex_weight_dtype(weight) != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph embed execution requires F32 token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    embedding = yvex_weight_device_tensor(weight);
    if (!embedding) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_engine_execute_fixture_graph",
                       "attached token embedding has no backend tensor");
        return YVEX_ERR_STATE;
    }
    if (yvex_device_tensor_rank(embedding) != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_fixture_graph",
                       "fixture graph token embedding must have rank 2");
        return YVEX_ERR_FORMAT;
    }

    hidden_size = yvex_device_tensor_dims(embedding)[0];
    vocab_size = yvex_device_tensor_dims(embedding)[1];
    if (hidden_size == 0 || vocab_size == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_engine_execute_fixture_graph",
                       "fixture graph token embedding dimensions must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    out->shape_status = "pass";
    out->range_status = "pass";
    if ((unsigned long long)token_id >= vocab_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_fixture_graph",
                        "fixture token id %u exceeds embedding vocab size %llu",
                        token_id, vocab_size);
        return YVEX_ERR_BOUNDS;
    }
    if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_engine_execute_fixture_graph",
                       "fixture graph output is too large for host readback");
        return YVEX_ERR_BOUNDS;
    }

    output_count = hidden_size;
    output_bytes = output_count * (unsigned long long)sizeof(float);
    out->output_bytes_planned = output_bytes;
    memset(&output_desc, 0, sizeof(output_desc));
    output_desc.name = "fixture.embed.output";
    output_desc.dtype = YVEX_DTYPE_F32;
    output_desc.rank = 2;
    output_desc.dims[0] = 1;
    output_desc.dims[1] = hidden_size;
    output_desc.bytes = output_bytes;

    if (!yvex_backend_supports(engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        out->backend_op_status = "unsupported";
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_engine_execute_fixture_graph",
                       "fixture graph backend does not support embed op");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->backend_op_status = "supported";
    memset(&execution, 0, sizeof(execution));
    execution.engine = engine;
    execution.embedding = embedding;
    execution.output_desc = &output_desc;
    execution.token_id = token_id;
    execution.output_bytes = output_bytes;
    execution.readback = &readback;
    execution.where = "yvex_engine_execute_fixture_graph";
    execution.readback_error = "failed to allocate fixture output readback";
    execution.phase = &out->graph_execution_phase;
    execution.allocation_attempted = &out->output_allocation_attempted;
    execution.bytes_allocated = &out->output_bytes_allocated;
    execution.dispatch_attempted = &out->dispatch_attempted;
    execution.cleanup_attempted = &out->cleanup_attempted;
    execution.cleanup_status = &out->cleanup_status;
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
    out->output_value_count = output_count > YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES
                                  ? YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES
                                  : output_count;
    for (i = 0; i < out->output_value_count; ++i) {
        out->output_values[i] = readback[i];
    }

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

/* Releases every partial-graph temporary and records cleanup only after an
 * execution transaction has acquired a backend output. */
/* Purpose: Release the resources owned by partial graph release without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_release(partial_graph_ctx *ctx, int status, int mark_cleanup)
{
    runtime_free_output_tensor(ctx->engine->weight_backend, &ctx->output);
    runtime_free_partial_buffers(&ctx->readback, &ctx->reference);
    if (mark_cleanup)
        runtime_mark_graph_cleanup(&ctx->out->cleanup_attempted,
                                   &ctx->out->cleanup_status);
    return status;
}

/* Resolves the planned embedding node, admitted weight/range, output geometry,
 * and backend capability without allocating execution scratch. */
/* Purpose: Implement the canonical partial graph prepare mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_prepare(partial_graph_ctx *ctx)
{
    int status;

    status = runtime_bind_f16_embedding(
        ctx->engine, ctx->token_id, "yvex_engine_execute_partial_graph",
        &ctx->embedding, ctx->err);
    if (status != YVEX_OK) {
        ctx->out->shape_status = "fail";
        ctx->out->slice_range_status = "fail";
        if (strstr(yvex_error_message(ctx->err), "token-out-of-range"))
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS,
                            "yvex_engine_execute_partial_graph",
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
    status = runtime_bind_embedding_range(
        ctx->engine, &ctx->embedding, ctx->token_id,
        &ctx->tensor_range, &ctx->slice_range, ctx->err);
    if (status != YVEX_OK) {
        if (!ctx->tensor_range.range_valid) {
            ctx->out->range_status = "fail";
        } else {
            ctx->out->range_status = "pass";
            ctx->out->slice_range_status = "fail";
        }
        if ((unsigned long long)ctx->token_id >= ctx->embedding.shape.vocab_size)
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS,
                            "yvex_engine_execute_partial_graph",
                            "partial token out of range: %u >= %llu",
                            ctx->token_id, ctx->embedding.shape.vocab_size);
        return status;
    }
    ctx->out->range_status = "pass";
    ctx->out->slice_range_status = "pass";
    if (ctx->embedding.shape.output_bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_partial_graph",
                       "partial graph output is too large for host readback");
        return YVEX_ERR_BOUNDS;
    }
    ctx->output_count = ctx->embedding.shape.output_count;
    ctx->output_bytes = ctx->embedding.shape.output_bytes;
    ctx->out->output_bytes_planned = ctx->output_bytes;
    ctx->out->reference_bytes_planned = ctx->slice_range.slice_bytes;
    ctx->output_desc.name = "partial.token_embedding.output";
    ctx->output_desc.dtype = YVEX_DTYPE_F32;
    ctx->output_desc.rank = 2;
    ctx->output_desc.dims[0] = 1;
    ctx->output_desc.dims[1] = ctx->hidden_size;
    ctx->output_desc.bytes = ctx->output_bytes;
    if (!yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        ctx->out->backend_op_status = "unsupported";
        yvex_error_set(ctx->err, YVEX_ERR_UNSUPPORTED,
                       "yvex_engine_execute_partial_graph",
                       "real partial graph backend does not support embed op");
        return YVEX_ERR_UNSUPPORTED;
    }
    ctx->out->backend_op_status = "supported";
    return YVEX_OK;
}

/* Executes and compares the admitted embedding transaction while preserving
 * the historical fault-injection and cleanup boundaries. */
/* Purpose: Execute the typed partial graph execute operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int partial_graph_execute(partial_graph_ctx *ctx)
{
    runtime_embedding_execution execution;
    unsigned long long index;
    int status;

    ctx->readback = (float *)malloc((size_t)ctx->output_bytes);
    ctx->reference = (float *)malloc((size_t)ctx->output_bytes);
    if (!ctx->readback || !ctx->reference) {
        yvex_error_set(ctx->err, YVEX_ERR_NOMEM, "yvex_engine_execute_partial_graph",
                       "failed to allocate partial graph readback buffers");
        return partial_graph_release(ctx, YVEX_ERR_NOMEM, 0);
    }
    ctx->out->graph_execution_phase = "reference";
    ctx->out->reference_read_attempted = 1;
    status = build_f16_embedding_reference(
        ctx->engine->artifact, &ctx->tensor_range, &ctx->slice_range,
        ctx->reference, ctx->err);
    if (status != YVEX_OK)
        return partial_graph_release(ctx, status, 0);
    if (yvex_core_test_flag("YVEX_TEST_FAIL_GRAPH_AFTER_REFERENCE_READ")) {
        yvex_error_set(ctx->err, YVEX_ERR_BACKEND, "yvex_engine_execute_partial_graph",
                       "test graph failure after reference read");
        return partial_graph_release(ctx, YVEX_ERR_BACKEND, 1);
    }
    memset(&execution, 0, sizeof(execution));
    execution.engine = ctx->engine;
    execution.embedding = ctx->embedding.device;
    execution.output_desc = &ctx->output_desc;
    execution.token_id = ctx->token_id;
    execution.output_bytes = ctx->output_bytes;
    execution.readback = &ctx->readback;
    execution.where = "yvex_engine_execute_partial_graph";
    execution.readback_error = "failed to allocate partial graph readback buffer";
    execution.phase = &ctx->out->graph_execution_phase;
    execution.allocation_attempted = &ctx->out->output_allocation_attempted;
    execution.bytes_allocated = &ctx->out->output_bytes_allocated;
    execution.dispatch_attempted = &ctx->out->dispatch_attempted;
    execution.cleanup_attempted = &ctx->out->cleanup_attempted;
    execution.cleanup_status = &ctx->out->cleanup_status;
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
    ctx->out->max_abs_diff = max_abs_diff_f32(
        ctx->readback, ctx->reference, ctx->output_count);
    if (ctx->out->max_abs_diff != 0.0) {
        yvex_error_setf(ctx->err, YVEX_ERR_FORMAT,
                        "yvex_engine_execute_partial_graph",
                        "partial graph reference comparison failed: max_abs_diff %.9g",
                        ctx->out->max_abs_diff);
        return partial_graph_release(ctx, YVEX_ERR_FORMAT, 1);
    }
    ctx->out->output_value_count =
        ctx->output_count > YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES
            ? YVEX_PARTIAL_GRAPH_MAX_OUTPUT_VALUES : ctx->output_count;
    for (index = 0; index < ctx->out->output_value_count; ++index)
        ctx->out->output_values[index] = ctx->readback[index];
    partial_graph_release(ctx, YVEX_OK, 0);
    ctx->out->graph_integrity_guard = "pass";
    ctx->out->graph_execution_phase = "complete";
    ctx->out->cleanup_status = "not-needed";
    yvex_error_clear(ctx->err);
    return YVEX_OK;
}

/* Runs the bounded one-node embedding proof; the function owns only its local
 * transaction context and publishes completion after exact comparison. */
/* Purpose: Execute the typed engine execute partial graph operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_execute_partial_graph(yvex_engine *engine,
                                      const yvex_partial_graph_options *options,
                                      yvex_partial_graph_result *out,
                                      yvex_error *err)
{
    partial_graph_ctx ctx;
    int status;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_partial_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
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

/* Releases the complete segment transaction, recording cleanup only after
 * execution-side resources became observable in the result. */
/* Purpose: Release the resources owned by segment graph release without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_release(segment_graph_ctx *ctx, int status, int mark_cleanup)
{
    runtime_free_two_output_tensors(ctx->engine->weight_backend,
                                    &ctx->embed_output, &ctx->segment_output);
    runtime_free_segment_buffers(&ctx->embedding_reference, &ctx->norm_reference,
                                 &ctx->segment_reference, &ctx->readback);
    if (mark_cleanup)
        runtime_mark_graph_cleanup(&ctx->out->cleanup_attempted,
                                   &ctx->out->cleanup_status);
    return status;
}

/* Admits the engine, requested segment, embed node, and token embedding
 * without allocating scratch or reading artifact bytes. */
/* Purpose: Implement the canonical segment graph admit mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_admit(segment_graph_ctx *ctx, const char *segment_name)
{
    int status;

    if (segment_name && strcmp(segment_name, "embedding-rmsnorm") != 0) {
        yvex_error_set(ctx->err, YVEX_ERR_INVALID_ARG,
                       "yvex_engine_execute_segment_graph",
                       "unsupported segment; expected embedding-rmsnorm");
        return YVEX_ERR_INVALID_ARG;
    }
    status = runtime_bind_f16_embedding(
        ctx->engine, ctx->token_id, "yvex_engine_execute_segment_graph",
        &ctx->embedding, ctx->err);
    if (status != YVEX_OK) {
        int out_of_range = strstr(yvex_error_message(ctx->err), "token-out-of-range") != NULL;
        ctx->out->shape_status = out_of_range ? "pass" : "fail";
        ctx->out->slice_range_status = "fail";
        if (out_of_range)
            yvex_error_setf(ctx->err, YVEX_ERR_BOUNDS,
                            "yvex_engine_execute_segment_graph",
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

/* Resolves and validates the first admitted RMSNorm weight that matches the
 * segment hidden width. */
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
        yvex_error_set(ctx->err, YVEX_ERR_UNSUPPORTED,
                       "yvex_engine_execute_segment_graph", "rmsnorm-tensor-missing");
        return YVEX_ERR_UNSUPPORTED;
    }
    ctx->out->rmsnorm_tensor_name = ctx->norm_tensor->name;
    ctx->out->rmsnorm_tensor_dtype = yvex_dtype_name(ctx->norm_tensor->dtype);
    ctx->norm_weight = yvex_weight_table_find(ctx->engine->weights, ctx->norm_tensor->name);
    if (!ctx->norm_weight) {
        ctx->out->shape_status = "fail";
        yvex_error_set(ctx->err, YVEX_ERR_UNSUPPORTED,
                       "yvex_engine_execute_segment_graph", "rmsnorm-tensor-missing");
        return YVEX_ERR_UNSUPPORTED;
    }
    ctx->norm_weight_tensor = yvex_weight_device_tensor(ctx->norm_weight);
    if (!ctx->norm_weight_tensor) {
        yvex_error_set(ctx->err, YVEX_ERR_STATE, "yvex_engine_execute_segment_graph",
                       "attached RMSNorm weight has no backend tensor");
        return YVEX_ERR_STATE;
    }
    if (ctx->norm_tensor->rank != 1 || ctx->norm_tensor->dims[0] != ctx->hidden_size ||
        yvex_device_tensor_rank(ctx->norm_weight_tensor) != 1 ||
        yvex_device_tensor_dims(ctx->norm_weight_tensor)[0] != ctx->hidden_size) {
        ctx->out->shape_status = "fail";
        yvex_error_set(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                       "rmsnorm-shape-invalid");
        return YVEX_ERR_FORMAT;
    }
    if (ctx->norm_tensor->dtype != YVEX_DTYPE_F16 &&
        ctx->norm_tensor->dtype != YVEX_DTYPE_F32) {
        ctx->out->shape_status = "fail";
        yvex_error_set(ctx->err, YVEX_ERR_UNSUPPORTED,
                       "yvex_engine_execute_segment_graph", "rmsnorm-dtype-invalid");
        return YVEX_ERR_UNSUPPORTED;
    }
    ctx->out->shape_status = "pass";
    return YVEX_OK;
}

/* Validates physical ranges, scalar metadata, memory geometry, and backend
 * capability before any reference or backend execution allocation. */
/* Purpose: Implement the canonical segment graph plan mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_plan(segment_graph_ctx *ctx)
{
    int status;

    status = runtime_bind_embedding_range(
        ctx->engine, &ctx->embedding, ctx->token_id,
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
        yvex_error_set(ctx->err, YVEX_ERR_FORMAT, "yvex_engine_execute_segment_graph",
                       "attached tensor byte count does not match validated range");
        return YVEX_ERR_FORMAT;
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
        !yvex_core_u64_add(ctx->output_bytes, ctx->output_bytes,
                                 &ctx->planned_alloc_bytes)) {
        yvex_error_set(ctx->err, YVEX_ERR_BOUNDS, "yvex_engine_execute_segment_graph",
                       "segment-memory-plan-overflow");
        return YVEX_ERR_BOUNDS;
    }
    ctx->out->hidden_size = ctx->hidden_size;
    ctx->out->vocab_size = ctx->vocab_size;
    ctx->out->segment_ops = 2;
    ctx->out->segment_intermediate_count = 1;
    ctx->out->segment_intermediate_bytes = ctx->output_bytes;
    ctx->out->segment_output_count = ctx->hidden_size;
    ctx->out->segment_output_bytes = ctx->output_bytes;
    ctx->out->segment_scratch_bytes = ctx->output_bytes;
    ctx->out->segment_reference_bytes = ctx->output_bytes;
    ctx->out->output_bytes_planned = ctx->planned_alloc_bytes;
    ctx->out->reference_bytes_planned = ctx->output_bytes;
    if (!yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        !yvex_backend_supports(ctx->engine->weight_backend, YVEX_BACKEND_CAP_OP_RMS_NORM) ||
        yvex_core_test_flag("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        ctx->out->backend_op_status = "unsupported";
        yvex_error_set(ctx->err, YVEX_ERR_UNSUPPORTED,
                       "yvex_engine_execute_segment_graph", "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    ctx->out->backend_op_status = "supported";
    return YVEX_OK;
}

/* Builds the independent embedding/RMSNorm scalar reference and preserves the
 * reference-read fault boundary. */
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

/* Allocates the two backend outputs as one logical transaction and publishes
 * byte accounting only after each successful acquisition. */
/* Purpose: Reserve budgeted storage for segment graph allocate outputs with checked size accounting.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_allocate_outputs(segment_graph_ctx *ctx)
{
    int status;

    ctx->embed_desc.name = "segment.embedding.output";
    ctx->embed_desc.dtype = YVEX_DTYPE_F32;
    ctx->embed_desc.rank = 2;
    ctx->embed_desc.dims[0] = 1;
    ctx->embed_desc.dims[1] = ctx->hidden_size;
    ctx->embed_desc.bytes = ctx->output_bytes;
    ctx->output_desc.name = "segment.rmsnorm.output";
    ctx->output_desc.dtype = YVEX_DTYPE_F32;
    ctx->output_desc.rank = 2;
    ctx->output_desc.dims[0] = 1;
    ctx->output_desc.dims[1] = ctx->hidden_size;
    ctx->output_desc.bytes = ctx->output_bytes;
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

/* Dispatches the two-node backend segment and reads the terminal output while
 * retaining the original per-node fault boundaries. */
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

/* Compares, accounts, and atomically publishes the completed segment proof. */
/* Purpose: Implement the canonical segment graph finish mechanism owned by the runtime boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
static int segment_graph_finish(segment_graph_ctx *ctx)
{
    unsigned long long index;

    ctx->out->executed = 1;
    ctx->out->node_count = 2;
    ctx->out->output_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->readback, ctx->output_bytes);
    ctx->out->reference_checksum = fixture_checksum_bytes(
        (const unsigned char *)ctx->segment_reference, ctx->output_bytes);
    ctx->out->max_abs_diff = max_abs_diff_f32(
        ctx->readback, ctx->segment_reference, ctx->hidden_size);
    if (ctx->out->max_abs_diff > 0.0001) {
        yvex_error_setf(ctx->err, YVEX_ERR_FORMAT,
                        "yvex_engine_execute_segment_graph",
                        "segment graph reference comparison failed: max_abs_diff %.9g",
                        ctx->out->max_abs_diff);
        return segment_graph_release(ctx, YVEX_ERR_FORMAT, 1);
    }
    ctx->out->output_value_count =
        ctx->hidden_size > YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES
            ? YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES : ctx->hidden_size;
    for (index = 0; index < ctx->out->output_value_count; ++index)
        ctx->out->output_values[index] = ctx->readback[index];
    segment_graph_release(ctx, YVEX_OK, 0);
    ctx->out->graph_integrity_guard = "pass";
    ctx->out->graph_execution_phase = "complete";
    ctx->out->cleanup_status = "not-needed";
    yvex_error_clear(ctx->err);
    return YVEX_OK;
}

/* Executes the admitted two-node embedding/RMSNorm evidence segment without
 * promoting it to full graph or generation capability. */
/* Purpose: Execute the typed engine execute segment graph operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed runtime refusal and publishes no partial success state.
 * Boundary: Runtime ownership and dispatch; does not bypass artifact admission or materialization identity. */
int yvex_engine_execute_segment_graph(yvex_engine *engine,
                                      const yvex_segment_graph_options *options,
                                      yvex_segment_graph_result *out,
                                      yvex_error *err)
{
    segment_graph_ctx ctx;
    const char *segment_name = options ? options->segment_name : NULL;
    int status;

    if (!engine || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_engine_execute_segment_graph",
                       "engine and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
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
