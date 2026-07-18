/*
 * yvex_deepseek_attention_cuda.c - DeepSeek attention CUDA graph binding.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   stale-context validation, typed role-to-device-job projection, trusted
 *   materialized weight reads, transactional CUDA trace publication, and
 *   graph-level execution facts for one stateful token.
 *
 * Does not own:
 *   CUDA device arithmetic, qtype geometry, artifact or source admission,
 *   architecture policy, independent reference arithmetic, persistent KV,
 *   prefill, decode, logits, sampling, generation, or operator output.
 *
 * Invariants:
 *   no CUDA work starts before all upstream identities and history validate;
 *   tensor names are never parsed; a failure releases all host/device scratch
 *   and leaves the caller trace/result unset.
 *
 * Boundary:
 *   one complete attention token is graph evidence, not generation.
 */
#include "yvex_deepseek_attention_internal.h"

#include "src/backend/cuda/cuda_deepseek_attention.h"

#include <stdint.h>

typedef struct {
    unsigned char *owned[YVEX_CUDA_DEEPSEEK_WEIGHT_COUNT];
    unsigned long long payload_bytes_read;
} attention_cuda_weights;

static const yvex_runtime_tensor_binding *attention_cuda_find_binding(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    unsigned long long layer_index)
{
    return yvex_runtime_descriptor_find_role(
        descriptor, role, YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
        layer_index, YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

static void attention_cuda_weights_release(attention_cuda_weights *weights)
{
    unsigned int i;
    if (!weights) return;
    for (i = 0u; i < YVEX_CUDA_DEEPSEEK_WEIGHT_COUNT; ++i) {
        free(weights->owned[i]);
        weights->owned[i] = NULL;
    }
    weights->payload_bytes_read = 0ull;
}

/* Contract: reads one complete admitted encoded tensor into bounded host scratch. */
static int attention_cuda_load_weight(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    yvex_cuda_deepseek_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_cuda_deepseek_attention_job *job,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    yvex_cuda_deepseek_weight *weight;
    unsigned long long blocks;
    unsigned long long row_bytes;
    unsigned long long expected;
    int rc;

    if (!session || !runtime_binding || !runtime_binding->binding || !owned ||
        !job || slot >= YVEX_CUDA_DEEPSEEK_WEIGHT_COUNT)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention weight load requires a typed binding and slot");
    binding = runtime_binding->binding;
    if (!binding->row_width || !binding->row_count || !binding->block_size ||
        !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull ||
        binding->encoded_bytes > (unsigned long long)SIZE_MAX)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->block_size, binding->row_width, err, YVEX_ERR_BOUNDS,
            "CUDA attention encoded tensor geometry is invalid");
    blocks = binding->row_width / binding->block_size;
    if (!attention_checked_mul_u64(blocks, binding->bytes_per_block,
                                   &row_bytes) ||
        !attention_checked_mul_u64(row_bytes, binding->row_count, &expected) ||
        expected != binding->encoded_bytes)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, expected, err, YVEX_ERR_FORMAT,
            "CUDA attention encoded tensor range is not row-exact");
    owned->owned[slot] = (unsigned char *)malloc((size_t)binding->encoded_bytes);
    if (!owned->owned[slot])
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "CUDA attention encoded weight allocation failed");
    rc = yvex_materialization_session_read(
        session, binding, 0ull, owned->owned[slot],
        (size_t)binding->encoded_bytes, NULL, err);
    if (rc != YVEX_OK)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ, runtime_binding,
            binding->layer_index, binding->role, binding->encoded_bytes, 0ull,
            err, (yvex_status)rc,
            "CUDA attention failed to read admitted encoded weight");
    if (!attention_checked_add_u64(owned->payload_bytes_read,
                                   binding->encoded_bytes,
                                   &owned->payload_bytes_read))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role, ULLONG_MAX,
            binding->encoded_bytes, err, YVEX_ERR_BOUNDS,
            "CUDA attention payload-byte accounting overflowed");
    weight = &job->weights[slot];
    weight->encoded = owned->owned[slot];
    weight->encoded_bytes = (size_t)binding->encoded_bytes;
    weight->row_bytes = row_bytes;
    weight->row_width = binding->row_width;
    weight->row_count = binding->row_count;
    weight->qtype = binding->qtype;
    weight->present = 1;
    return YVEX_OK;
}

static int attention_cuda_load_role(
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_cuda_deepseek_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_cuda_deepseek_attention_job *job,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        attention_cuda_find_binding(descriptor, role, layer_index);
    if (!binding)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention required typed role binding is absent");
    return attention_cuda_load_weight(
        session, binding, slot, owned, job, failure, err);
}

static void attention_cuda_activation_project(
    const yvex_deepseek_v4_runtime_activation_policy *source,
    yvex_cuda_deepseek_activation *out)
{
    memset(out, 0, sizeof(*out));
    if (!source || !source->required) return;
    out->required = 1;
    out->block_width = source->block_width;
    out->quantization = (unsigned int)source->quantization;
    out->hadamard = source->pre_transform ==
        YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2;
}

static int attention_cuda_rolling_project(
    const yvex_deepseek_attention_rolling_state_view *source,
    yvex_cuda_deepseek_rolling *out)
{
    if (!source || !source->present || !out) return 0;
    *out = (yvex_cuda_deepseek_rolling){
        .present = 1,
        .ratio = source->ratio,
        .head_dimension = source->head_dimension,
        .state_width = source->state_width,
        .state_slots = source->state_slots,
        .cursor = source->cursor,
        .previous_fill = source->previous_fill,
        .current_fill = source->current_fill,
        .kv_state = source->kv_state,
        .score_state = source->score_state,
        .overlap = source->overlap,
    };
    return 1;
}

static int attention_cuda_trace_allocate(
    yvex_deepseek_attention_execution_trace *trace,
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_history_view *history,
    unsigned long long token_position,
    const float *input,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long query_width;
    unsigned long long index_query_width = 0ull;
    unsigned long long topk_capacity = 0ull;
    unsigned long long extent;

    if (!trace || !layer || !history || !input ||
        !attention_checked_mul_u64(layer->query_heads,
                                   layer->head_dimension, &query_width))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention trace allocation requires plan and input");
    memset(trace, 0, sizeof(*trace));
#define ALLOC_TRACE(field, count, type) do {                                  \
        trace->field = (type *)attention_calloc_array((count), sizeof(type));  \
        if ((count) && !trace->field) goto allocation_failure;                \
    } while (0)
    ALLOC_TRACE(input, layer->hidden_dimension, float);
    ALLOC_TRACE(q_low, layer->query_lora_rank, float);
    ALLOC_TRACE(query, query_width, float);
    ALLOC_TRACE(raw_kv, layer->head_dimension, float);
    ALLOC_TRACE(attention_values, query_width, float);
    ALLOC_TRACE(output, layer->hidden_dimension, float);
    memcpy(trace->input, input,
           (size_t)layer->hidden_dimension * sizeof(*trace->input));
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        ALLOC_TRACE(compressed_kv, layer->head_dimension, float);
        ALLOC_TRACE(compressed_positions, 1ull, unsigned long long);
        if (!attention_checked_mul_u64(
                history->main_rolling_state.state_width,
                history->main_rolling_state.state_slots, &extent))
            goto allocation_failure;
        ALLOC_TRACE(next_main_rolling_state.kv_state, extent, float);
        ALLOC_TRACE(next_main_rolling_state.score_state, extent, float);
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        if (!attention_checked_mul_u64(
                layer->indexer_heads, layer->indexer_head_dimension,
                &index_query_width))
            goto allocation_failure;
        ALLOC_TRACE(indexer_kv, layer->indexer_head_dimension, float);
        ALLOC_TRACE(indexer_positions, 1ull, unsigned long long);
        ALLOC_TRACE(index_query, index_query_width, float);
        ALLOC_TRACE(index_weights, layer->indexer_heads, float);
        if (!attention_checked_add_u64(history->compressed_entry_count, 1ull,
                                       &extent))
            goto allocation_failure;
        topk_capacity = attention_min_u64(extent, layer->sparse_topk.k);
        ALLOC_TRACE(topk_counts, 1ull, unsigned long long);
        ALLOC_TRACE(topk_positions, topk_capacity, unsigned long long);
        if (!attention_checked_mul_u64(
                history->indexer_rolling_state.state_width,
                history->indexer_rolling_state.state_slots, &extent))
            goto allocation_failure;
        ALLOC_TRACE(next_indexer_rolling_state.kv_state, extent, float);
        ALLOC_TRACE(next_indexer_rolling_state.score_state, extent, float);
    }
#undef ALLOC_TRACE
    trace->owned = 1;
    trace->layer_index = layer->layer_index;
    trace->attention_class = layer->attention_class;
    trace->token_position = token_position;
    trace->token_count = 1ull;
    trace->hidden_width = layer->hidden_dimension;
    trace->q_rank = layer->query_lora_rank;
    trace->query_width = query_width;
    trace->kv_width = layer->head_dimension;
    trace->compressed_stride = layer->head_dimension;
    trace->indexer_stride = layer->indexer_head_dimension;
    trace->index_query_stride = layer->attention_class ==
        YVEX_DEEPSEEK_V4_ATTENTION_CSA ? index_query_width : 0ull;
    trace->index_weight_stride = layer->indexer_heads;
    trace->topk_stride = topk_capacity;
    return YVEX_OK;

allocation_failure:
#undef ALLOC_TRACE
    yvex_deepseek_attention_execution_trace_release(trace);
    return attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
        YVEX_ERR_NOMEM, "CUDA attention trace allocation failed");
}

static void attention_cuda_rolling_commit(
    const yvex_deepseek_attention_rolling_state_view *before,
    unsigned long long token_position,
    yvex_deepseek_attention_rolling_state_output *after)
{
    int emitted;
    if (!before || !before->present || !after) return;
    emitted = ((token_position + 1ull) % before->ratio) == 0ull;
    after->present = 1;
    after->schema_version = before->schema_version;
    after->kind = before->kind;
    after->attention_class = before->attention_class;
    after->layer_index = before->layer_index;
    after->next_token_position = token_position + 1ull;
    after->ratio = before->ratio;
    after->head_dimension = before->head_dimension;
    after->state_width = before->state_width;
    after->state_slots = before->state_slots;
    after->previous_fill = emitted
        ? (before->overlap ? before->ratio : 0ull) : before->previous_fill;
    after->current_fill = emitted ? 0ull :
        (before->current_fill < before->cursor + 1ull
            ? before->cursor + 1ull : before->current_fill);
    after->cursor = emitted ? 0ull : (before->cursor + 1ull) % before->ratio;
    after->kv_state_stride = before->state_width;
    after->score_state_stride = before->state_width;
    if (!attention_checked_mul_u64(before->state_width, before->state_slots,
                                   &after->kv_state_extent))
        after->kv_state_extent = 0ull;
    after->score_state_extent = after->kv_state_extent;
    after->overlap = before->overlap;
    after->rotated = before->rotated;
    memcpy(after->attention_plan_identity, before->attention_plan_identity,
           sizeof(after->attention_plan_identity));
}

static double attention_cuda_checksum(const float *values,
                                      unsigned long long count)
{
    double checksum = 0.0;
    unsigned long long i;
    for (i = 0ull; values && i < count; ++i)
        checksum += (double)values[i] * (double)((i % 17ull) + 1ull);
    return checksum;
}

static int attention_cuda_output_identity(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_attention_execution_trace *trace,
    char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    const yvex_deepseek_attention_summary *summary =
        yvex_deepseek_attention_plan_summary(plan);
    size_t bytes;
    if (!summary || !trace || !out ||
        !attention_checked_size(trace->hidden_width, sizeof(float), &bytes))
        return 0;
    yvex_sha256_init(&hash);
    if (!attention_hash_text(&hash, "yvex.deepseek.attention.cuda.output.v1") ||
        !attention_hash_text(&hash, summary->attention_plan_identity) ||
        !attention_hash_u64(&hash, trace->layer_index) ||
        !attention_hash_u64(&hash, trace->token_position) ||
        !yvex_sha256_update(&hash, trace->output, bytes) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}

/* Contract: executes one complete stateful attention token through CUDA only. */
int yvex_deepseek_attention_cuda_token_execute(
    const yvex_deepseek_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_backend *backend,
    const yvex_deepseek_attention_cpu_options *options,
    yvex_deepseek_attention_cpu_result *result,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    static const struct {
        yvex_tensor_role role;
        yvex_cuda_deepseek_weight_slot slot;
    } base_roles[] = {
        {YVEX_TENSOR_ROLE_ATTENTION_Q_A, YVEX_CUDA_DEEPSEEK_WEIGHT_Q_A},
        {YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
         YVEX_CUDA_DEEPSEEK_WEIGHT_Q_A_NORM},
        {YVEX_TENSOR_ROLE_ATTENTION_Q_B, YVEX_CUDA_DEEPSEEK_WEIGHT_Q_B},
        {YVEX_TENSOR_ROLE_ATTENTION_KV, YVEX_CUDA_DEEPSEEK_WEIGHT_KV},
        {YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
         YVEX_CUDA_DEEPSEEK_WEIGHT_KV_NORM},
        {YVEX_TENSOR_ROLE_ATTENTION_SINKS,
         YVEX_CUDA_DEEPSEEK_WEIGHT_SINKS},
        {YVEX_TENSOR_ROLE_ATTENTION_OUT_A, YVEX_CUDA_DEEPSEEK_WEIGHT_OUT_A},
        {YVEX_TENSOR_ROLE_ATTENTION_OUT_B, YVEX_CUDA_DEEPSEEK_WEIGHT_OUT_B},
    };
    static const struct {
        yvex_tensor_role role;
        yvex_cuda_deepseek_weight_slot slot;
    } compressor_roles[] = {
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV,
         YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_KV},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE,
         YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_GATE},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE,
         YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_APE},
        {YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM,
         YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_NORM},
    };
    static const struct {
        yvex_tensor_role role;
        yvex_cuda_deepseek_weight_slot slot;
    } index_roles[] = {
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_KV},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_GATE},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_APE},
        {YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_NORM},
        {YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_Q},
        {YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
         YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_PROJECTION},
    };
    yvex_deepseek_attention_cpu_options defaults;
    const yvex_deepseek_attention_cpu_options *opts = options;
    const yvex_deepseek_attention_layer_plan *layer;
    const yvex_deepseek_v4_layer_spec *architecture;
    yvex_deepseek_attention_history_view empty_history;
    const yvex_deepseek_attention_history_view *history;
    attention_cuda_weights weights;
    yvex_cuda_deepseek_attention_job job;
    yvex_cuda_deepseek_attention_output cuda_output;
    yvex_cuda_deepseek_failure cuda_failure;
    yvex_deepseek_attention_execution_trace trace;
    unsigned int i;
    int rc;

    if (result) memset(result, 0, sizeof(*result));
    memset(&weights, 0, sizeof(weights));
    memset(&job, 0, sizeof(job));
    memset(&cuda_output, 0, sizeof(cuda_output));
    memset(&trace, 0, sizeof(trace));
    memset(&empty_history, 0, sizeof(empty_history));
    if (!opts) {
        yvex_deepseek_attention_cpu_options_default(&defaults);
        opts = &defaults;
    }
    if (!plan || !ir || !session || !descriptor || !backend || !result ||
        !opts->input || opts->input_stride == 0ull ||
        (opts->token_count && opts->token_count != 1ull))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            opts ? opts->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention requires one explicit input token and backend");
    rc = attention_execution_context_validate(
        plan, ir, session, descriptor, failure, err);
    if (rc != YVEX_OK) return rc;
    if (opts->trace && opts->trace->owned)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull, err,
            YVEX_ERR_STATE,
            "CUDA attention trace must be released before reuse");
    layer = yvex_deepseek_attention_plan_layer_at(plan, opts->layer_index);
    architecture = yvex_deepseek_v4_ir_layer_at(ir, opts->layer_index);
    if (!layer || !architecture || opts->input_stride < layer->hidden_dimension)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer ? layer->hidden_dimension : 1ull, opts->input_stride, err,
            YVEX_ERR_BOUNDS,
            "CUDA attention layer or input stride is invalid");
    history = opts->history ? opts->history : &empty_history;
    if (history->token_count != opts->token_position)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            opts->token_position, history->token_count, err, YVEX_ERR_STATE,
            "CUDA attention history is not contiguous");
    if (opts->history) {
        rc = yvex_deepseek_attention_history_validate(
            layer, history, failure, err);
        if (rc != YVEX_OK) return rc;
    } else if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            opts->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "compressed CUDA attention requires explicit rolling history");
    }
    rc = attention_cuda_trace_allocate(
        &trace, layer, history, opts->token_position, opts->input, failure,
        err);
    if (rc != YVEX_OK) return rc;
    job.attention_class = (unsigned int)layer->attention_class;
    job.token_position = opts->token_position;
    job.hidden_width = layer->hidden_dimension;
    job.q_rank = layer->query_lora_rank;
    job.query_heads = layer->query_heads;
    job.head_dimension = layer->head_dimension;
    job.kv_width = layer->head_dimension;
    job.sliding_window = layer->sliding_window;
    job.compression_ratio = layer->compression_ratio;
    job.output_groups = layer->output_groups;
    job.output_group_input_width = architecture->output_group_input_width;
    job.output_rank = layer->output_lora_rank;
    job.indexer_heads = layer->indexer_heads;
    job.indexer_head_dimension = layer->indexer_head_dimension;
    job.indexer_topk = layer->sparse_topk.k;
    job.rms_epsilon = architecture->rms_norm_epsilon;
    job.position.theta = architecture->position.theta;
    job.position.scaling_factor = architecture->position.scaling_factor;
    job.position.original_context = architecture->position.original_context;
    job.position.beta_fast = architecture->position.beta_fast;
    job.position.beta_slow = architecture->position.beta_slow;
    job.position.rope_dimensions = architecture->rope_head_dimension;
    attention_cuda_activation_project(
        &layer->attention_kv_activation, &job.attention_kv_activation);
    attention_cuda_activation_project(
        &layer->compressor_activation, &job.compressor_activation);
    attention_cuda_activation_project(
        &layer->compressor_rotated_activation,
        &job.compressor_rotated_activation);
    attention_cuda_activation_project(
        &layer->indexer_query_activation, &job.indexer_query_activation);
    job.input = opts->input;
    job.local_kv = history->local_kv;
    job.local_positions = history->local_positions;
    job.local_count = history->local_tail_count;
    job.local_stride = history->local_kv_stride;
    job.compressed_kv = history->compressed_kv;
    job.compressed_positions = history->compressed_positions;
    job.compressed_count = history->compressed_entry_count;
    job.compressed_stride = history->compressed_kv_stride;
    job.indexer_kv = history->indexer_kv;
    job.indexer_positions = history->indexer_positions;
    job.indexer_count = history->indexer_entry_count;
    job.indexer_stride = history->indexer_kv_stride;
    if (history->main_rolling_state.present)
        (void)attention_cuda_rolling_project(
            &history->main_rolling_state, &job.main_rolling);
    if (history->indexer_rolling_state.present)
        (void)attention_cuda_rolling_project(
            &history->indexer_rolling_state, &job.indexer_rolling);
    job.max_device_bytes = 1024ull * 1024ull * 1024ull;
    for (i = 0u; i < sizeof(base_roles) / sizeof(base_roles[0]); ++i) {
        rc = attention_cuda_load_role(
            session, descriptor, layer->layer_index, base_roles[i].role,
            base_roles[i].slot, &weights, &job, failure, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        for (i = 0u; i < sizeof(compressor_roles) /
                              sizeof(compressor_roles[0]); ++i) {
            rc = attention_cuda_load_role(
                session, descriptor, layer->layer_index,
                compressor_roles[i].role, compressor_roles[i].slot,
                &weights, &job, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        for (i = 0u; i < sizeof(index_roles) / sizeof(index_roles[0]); ++i) {
            rc = attention_cuda_load_role(
                session, descriptor, layer->layer_index, index_roles[i].role,
                index_roles[i].slot, &weights, &job, failure, err);
            if (rc != YVEX_OK) goto cleanup;
        }
    }
    cuda_output.q_low = trace.q_low;
    cuda_output.query = trace.query;
    cuda_output.raw_kv = trace.raw_kv;
    cuda_output.compressed_kv = trace.compressed_kv;
    cuda_output.indexer_kv = trace.indexer_kv;
    cuda_output.index_query = trace.index_query;
    cuda_output.index_weights = trace.index_weights;
    cuda_output.attention_values = trace.attention_values;
    cuda_output.output = trace.output;
    cuda_output.compressed_positions = trace.compressed_positions;
    cuda_output.indexer_positions = trace.indexer_positions;
    cuda_output.topk_positions = trace.topk_positions;
    cuda_output.main_kv_state = trace.next_main_rolling_state.kv_state;
    cuda_output.main_score_state = trace.next_main_rolling_state.score_state;
    cuda_output.indexer_kv_state = trace.next_indexer_rolling_state.kv_state;
    cuda_output.indexer_score_state = trace.next_indexer_rolling_state.score_state;
    rc = yvex_cuda_deepseek_attention_execute(
        backend, &job, &cuda_output, &cuda_failure, err);
    if (rc != YVEX_OK) {
        rc = attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            cuda_failure.expected, cuda_failure.actual, err,
            (yvex_status)rc,
            cuda_failure.stage ? cuda_failure.stage :
                "CUDA attention backend execution failed");
        goto cleanup;
    }
    trace.compressed_count = cuda_output.compressed_count;
    trace.indexer_count = cuda_output.indexer_count;
    trace.compressed_stride = trace.compressed_count
        ? layer->head_dimension : 0ull;
    trace.indexer_stride = trace.indexer_count
        ? layer->indexer_head_dimension : 0ull;
    if (trace.topk_counts) trace.topk_counts[0] = cuda_output.topk_count;
    if (history->main_rolling_state.present)
        attention_cuda_rolling_commit(
            &history->main_rolling_state, opts->token_position,
            &trace.next_main_rolling_state);
    if (history->indexer_rolling_state.present)
        attention_cuda_rolling_commit(
            &history->indexer_rolling_state, opts->token_position,
            &trace.next_indexer_rolling_state);
    trace.complete = 1;
    result->executed = 1;
    result->full_attention = 1;
    result->reference_independent = 0;
    result->cuda_executed = 1;
    result->layer_index = layer->layer_index;
    result->attention_class = layer->attention_class;
    result->token_position = opts->token_position;
    result->q_a_rows = layer->query_lora_rank;
    result->q_b_rows = layer->query_heads * layer->head_dimension;
    result->kv_rows = layer->head_dimension;
    result->topk_candidates = cuda_output.valid_candidate_count;
    result->topk_selected = cuda_output.topk_count;
    result->local_entries = history->local_tail_count + 1ull;
    result->compressed_entries = cuda_output.compressed_count;
    result->deduplicated_entries = history->local_tail_count + 1ull;
    result->payload_bytes_read = weights.payload_bytes_read;
    result->state_raw_entries = 1ull;
    result->state_compressed_entries = cuda_output.compressed_count;
    result->state_indexer_entries = cuda_output.indexer_count;
    result->cuda_kernel_launches = cuda_output.kernel_launches;
    result->cuda_peak_device_bytes = cuda_output.peak_device_bytes;
    result->q_projection_checksum = attention_cuda_checksum(
        trace.q_low, trace.q_rank);
    result->kv_projection_checksum = attention_cuda_checksum(
        trace.raw_kv, trace.kv_width);
    result->rope_checksum = attention_cuda_checksum(
        trace.query, trace.query_width);
    result->attention_checksum = attention_cuda_checksum(
        trace.attention_values, trace.query_width);
    result->output_checksum = attention_cuda_checksum(
        trace.output, trace.hidden_width);
    if (!attention_cuda_output_identity(plan, &trace,
                                        result->output_identity)) {
        rc = attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "CUDA attention output identity construction failed");
        goto cleanup;
    }
    if (opts->trace) {
        *opts->trace = trace;
        memset(&trace, 0, sizeof(trace));
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    rc = YVEX_OK;

cleanup:
    attention_cuda_weights_release(&weights);
    yvex_deepseek_attention_execution_trace_release(&trace);
    if (rc != YVEX_OK && result) memset(result, 0, sizeof(*result));
    return rc;
}
