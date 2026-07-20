/* Owner: graph execution memory and transaction lifecycle.
 * Owns: compressor scratch, rolling storage, traces, CUDA staging, state transactions, and memory sink.
 * Does not own: family schedules, numeric policy, backend execution, persistent KV, or generation.
 * Invariants: active state is transactional; committed views outlive no owning sink or trace.
 * Boundary: bounded execution memory does not establish complete attention or runtime support.
 * Purpose: centralize allocation, publication, rollback, and evidence ownership for attention execution.
 * Inputs: admitted plans, histories, bindings, payload sessions, and caller budgets.
 * Effects: allocates owned scratch and publishes state only through explicit commit.
 * Failure: every partial allocation, load, or transaction is released or aborted deterministically. */
#include "src/graph/private.h"
#include <limits.h>
#include <stdlib.h>
static int attention_rolling_view_init(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long next_token_position, float *kv_state,
    float *score_state, yvex_attention_rolling_state_view *out);
static const yvex_attention_memory_sink_options memory_sink_defaults = {
    .fail_acquire_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT,
    .fail_seal_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT
};
// Purpose: Return the admitted cuda weights release fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_cuda_weights_release(attention_cuda_weights *weights)
{
    unsigned int i;
    if (!weights) return;
    for (i = 0u; i < YVEX_BACKEND_ATTENTION_WEIGHT_COUNT; ++i) {
        free(weights->owned[i]);
        weights->owned[i] = NULL;
    }
    weights->payload_bytes_read = 0ull;
}
// Purpose: Implement the graph-local cuda load weight semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_cuda_load_weight(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime_binding,
    yvex_backend_attention_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_backend_attention_job *job,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_materialized_tensor_binding *binding;
    yvex_backend_attention_weight *weight;
    unsigned long long blocks;
    unsigned long long row_bytes;
    unsigned long long expected;
    int rc;
    if (!session || !runtime_binding || !runtime_binding->binding || !owned ||
        !job || slot >= YVEX_BACKEND_ATTENTION_WEIGHT_COUNT)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
            runtime_binding, YVEX_ATTENTION_NO_LAYER,
            runtime_binding ? runtime_binding->role : YVEX_TENSOR_ROLE_UNKNOWN,
            1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention weight load requires a typed binding and slot");
    binding = runtime_binding->binding;
    if (!binding->row_width || !binding->row_count || !binding->block_size ||
        !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull ||
        binding->encoded_bytes > (unsigned long long)SIZE_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->block_size, binding->row_width, err, YVEX_ERR_BOUNDS,
            "CUDA attention encoded tensor geometry is invalid");
    blocks = binding->row_width / binding->block_size;
    if (!yvex_core_u64_mul(blocks, binding->bytes_per_block,
                                   &row_bytes) ||
        !yvex_core_u64_mul(row_bytes, binding->row_count, &expected) ||
        expected != binding->encoded_bytes)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, expected, err, YVEX_ERR_FORMAT,
            "CUDA attention encoded tensor range is not row-exact");
    owned->owned[slot] = (unsigned char *)malloc((size_t)binding->encoded_bytes);
    if (!owned->owned[slot])
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
            runtime_binding, binding->layer_index, binding->role,
            binding->encoded_bytes, 0ull, err, YVEX_ERR_NOMEM,
            "CUDA attention encoded weight allocation failed");
    rc = yvex_materialization_session_read(
        session, binding, 0ull, owned->owned[slot],
        (size_t)binding->encoded_bytes, NULL, err);
    if (rc != YVEX_OK)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_READ, runtime_binding,
            binding->layer_index, binding->role, binding->encoded_bytes, 0ull,
            err, (yvex_status)rc,
            "CUDA attention failed to read admitted encoded weight");
    if (!yvex_core_u64_add(owned->payload_bytes_read,
                                   binding->encoded_bytes,
                                   &owned->payload_bytes_read))
        return yvex_attention_reject(
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
// Purpose: Return the admitted cuda role load fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_cuda_role_load(
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    unsigned long long layer_index,
    yvex_tensor_role role,
    yvex_backend_attention_weight_slot slot,
    attention_cuda_weights *owned,
    yvex_backend_attention_job *job,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_runtime_tensor_binding *binding =
        yvex_attention_binding_find(descriptor, role, layer_index);
    if (!binding)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING, NULL,
            layer_index, role, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "CUDA attention required typed role binding is absent");
    return attention_cuda_load_weight(
        session, binding, slot, owned, job, failure, err);
}
// Purpose: Return the admitted cuda activation project fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_cuda_activation_project(
    const yvex_attention_activation_policy *source,
    yvex_backend_attention_activation *out)
{
    memset(out, 0, sizeof(*out));
    if (!source || !source->required) return;
    out->required = 1;
    out->block_width = source->block_width;
    out->quantization = (unsigned int)source->quantization;
    out->hadamard = source->pre_transform ==
        YVEX_ATTENTION_TRANSFORM_DAO_FHT_V1_1_0_POST2;
}
// Purpose: Return the admitted cuda rolling project fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_cuda_rolling_project(
    const yvex_attention_rolling_state_view *source,
    yvex_backend_attention_rolling *out)
{
    if (!source || !source->present || !out) return 0;
    *out = (yvex_backend_attention_rolling){
        .present = 1,
        .next_token_position = source->next_token_position,
        .ratio = source->ratio,
        .head_dimension = source->head_dimension,
        .state_width = source->state_width,
        .state_slots = source->state_slots,
        .cursor = source->cursor,
        .previous_fill = source->previous_fill,
        .current_fill = source->current_fill,
        .kv_state = source->kv_state,
        .kv_state_capacity = source->kv_state_extent,
        .score_state = source->score_state,
        .score_state_capacity = source->score_state_extent,
        .overlap = source->overlap,
    };
    return 1;
}
// Purpose: Accumulate one typed trace extent without losing overflow evidence.
static int attention_trace_bytes_add(unsigned long long count,
                                     unsigned long long width,
                                     unsigned long long *total)
{
    unsigned long long bytes;
    return yvex_core_u64_mul(count, width, &bytes) &&
           yvex_core_u64_add(*total, bytes, total);
}
// Purpose: Return the admitted cuda trace allocate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_cuda_trace_allocate(
    yvex_attention_execution_trace *trace,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    unsigned long long token_position,
    const float *input,
    unsigned long long limit_bytes,
    unsigned long long *owned_bytes,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long query_width;
    unsigned long long index_query_width = 0ull;
    unsigned long long topk_capacity = 0ull;
    unsigned long long main_extent = 0ull;
    unsigned long long index_extent = 0ull;
    unsigned long long float_count = 0ull;
    unsigned long long u64_count = 0ull;
    unsigned long long total_bytes = 0ull;
    unsigned long long candidate_count;
    if (!owned_bytes)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention trace byte output is required");
    *owned_bytes = 0ull;
    if (!trace || !layer || !history || !input ||
        !yvex_core_u64_mul(layer->query_heads,
                                   layer->head_dimension, &query_width))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CUDA attention trace allocation requires plan and input");
    if (!attention_trace_bytes_add(layer->hidden_dimension, 2ull, &float_count) ||
        !attention_trace_bytes_add(layer->query_lora_rank, 1ull, &float_count) ||
        !attention_trace_bytes_add(query_width, 2ull, &float_count) ||
        !attention_trace_bytes_add(layer->head_dimension, 1ull, &float_count))
        goto budget_failure;
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        if (!yvex_core_u64_mul(history->main_rolling_state.state_width,
                               history->main_rolling_state.state_slots,
                               &main_extent) ||
            !attention_trace_bytes_add(layer->head_dimension, 1ull, &float_count) ||
            !attention_trace_bytes_add(main_extent, 2ull, &float_count) ||
            !attention_trace_bytes_add(1ull, sizeof(unsigned long long),
                                       &total_bytes))
            goto budget_failure;
    }
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        if (!yvex_core_u64_mul(layer->indexer_heads,
                               layer->indexer_head_dimension,
                               &index_query_width) ||
            !yvex_core_u64_add(history->compressed_entry_count, 1ull,
                               &candidate_count))
            goto budget_failure;
        topk_capacity = yvex_attention_min_u64(candidate_count,
                                               layer->sparse_topk.k);
        if (!yvex_core_u64_mul(history->indexer_rolling_state.state_width,
                               history->indexer_rolling_state.state_slots,
                               &index_extent) ||
            !attention_trace_bytes_add(layer->indexer_head_dimension, 1ull,
                                       &float_count) ||
            !attention_trace_bytes_add(index_query_width, 1ull, &float_count) ||
            !attention_trace_bytes_add(layer->indexer_heads, 1ull, &float_count) ||
            !attention_trace_bytes_add(index_extent, 2ull, &float_count) ||
            !yvex_core_u64_add(topk_capacity, 2ull, &u64_count))
            goto budget_failure;
    }
    if (!attention_trace_bytes_add(float_count, sizeof(float), &total_bytes) ||
        !attention_trace_bytes_add(u64_count, sizeof(unsigned long long),
                                   &total_bytes) ||
        total_bytes > limit_bytes || total_bytes > (unsigned long long)SIZE_MAX)
        goto budget_failure;
    memset(trace, 0, sizeof(*trace));
#define ALLOC_TRACE(field, count, type) do {                                  \
        trace->field = (type *)yvex_attention_calloc_array((count), sizeof(type));  \
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
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        ALLOC_TRACE(compressed_kv, layer->head_dimension, float);
        ALLOC_TRACE(compressed_positions, 1ull, unsigned long long);
        ALLOC_TRACE(next_main_rolling_state.kv_state, main_extent, float);
        ALLOC_TRACE(next_main_rolling_state.score_state, main_extent, float);
    }
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        ALLOC_TRACE(indexer_kv, layer->indexer_head_dimension, float);
        ALLOC_TRACE(indexer_positions, 1ull, unsigned long long);
        ALLOC_TRACE(index_query, index_query_width, float);
        ALLOC_TRACE(index_weights, layer->indexer_heads, float);
        ALLOC_TRACE(topk_counts, 1ull, unsigned long long);
        ALLOC_TRACE(topk_positions, topk_capacity, unsigned long long);
        ALLOC_TRACE(next_indexer_rolling_state.kv_state, index_extent, float);
        ALLOC_TRACE(next_indexer_rolling_state.score_state, index_extent, float);
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
        YVEX_ATTENTION_CLASS_CSA ? index_query_width : 0ull;
    trace->index_weight_stride = layer->indexer_heads;
    trace->topk_stride = topk_capacity;
    *owned_bytes = total_bytes;
    return YVEX_OK;
budget_failure:
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
        layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
        YVEX_TENSOR_ROLE_UNKNOWN, limit_bytes, total_bytes, err,
        YVEX_ERR_BOUNDS, "CUDA attention trace exceeds its host budget");
allocation_failure:
#undef ALLOC_TRACE
    yvex_attention_execution_trace_release(trace);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
        YVEX_ERR_NOMEM, "CUDA attention trace allocation failed");
}
// Purpose: Return the admitted cuda rolling commit fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_cuda_rolling_commit(
    const yvex_attention_rolling_state_view *before,
    unsigned long long token_position,
    yvex_attention_rolling_state_output *after)
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
    if (!yvex_core_u64_mul(before->state_width, before->state_slots,
                                   &after->kv_state_extent))
        after->kv_state_extent = 0ull;
    after->score_state_extent = after->kv_state_extent;
    after->overlap = before->overlap;
    after->rotated = before->rotated;
    memcpy(after->attention_plan_identity, before->attention_plan_identity,
           sizeof(after->attention_plan_identity));
}
// Purpose: Return the admitted cuda checksum fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
double yvex_attention_cuda_checksum(const float *values,
                                      unsigned long long count)
{
    double checksum = 0.0;
    unsigned long long i;
    for (i = 0ull; values && i < count; ++i)
        checksum += (double)values[i] * (double)((i % 17ull) + 1ull);
    return checksum;
}
// Purpose: Return the admitted cuda output identity fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_cuda_output_identity(
    const yvex_attention_plan *plan,
    const yvex_attention_execution_trace *trace,
    char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    const yvex_attention_summary *summary =
        yvex_attention_plan_summary(plan);
    size_t bytes;
    if (!summary || !trace || !out ||
        !yvex_attention_checked_size(trace->hidden_width, sizeof(float), &bytes))
        return 0;
    yvex_sha256_init(&hash);
    if (!yvex_attention_hash_text(&hash, "yvex.deepseek.attention.cuda.output.v1") ||
        !yvex_attention_hash_text(&hash, summary->attention_plan_identity) ||
        !yvex_attention_hash_u64(&hash, trace->layer_index) ||
        !yvex_attention_hash_u64(&hash, trace->token_position) ||
        !yvex_sha256_update(&hash, trace->output, bytes) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}
// Purpose: Return the admitted rolling storage allocate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_rolling_storage_allocate(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long token_position,
    float **kv_state_out,
    float **score_state_out,
    yvex_attention_rolling_state_view *view_out,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long extent;
    float *kv_state = NULL;
    float *score_state = NULL;
    yvex_attention_rolling_state_view view;
    int overlap;
    int rotated;
    int rc;

    if (kv_state_out) *kv_state_out = NULL;
    if (score_state_out) *score_state_out = NULL;
    if (view_out) memset(view_out, 0, sizeof(*view_out));
    if (!layer || !kv_state_out || !score_state_out || !view_out ||
        !yvex_attention_rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated) ||
        !yvex_core_u64_mul(state_slots, state_width, &extent))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention could not derive empty rolling-state geometry");
    kv_state = (float *)yvex_attention_calloc_array(extent, sizeof(float));
    score_state = (float *)yvex_attention_calloc_array(extent, sizeof(float));
    if (!kv_state || !score_state) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, extent, 0ull, err,
            YVEX_ERR_NOMEM,
            "DeepSeek attention empty rolling-state allocation failed");
        goto fail;
    }
    if (!attention_rolling_view_init(layer, kind, token_position,
                                     kv_state, score_state, &view)) {
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, extent, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention empty rolling-state initialization failed");
        goto fail;
    }
    *kv_state_out = kv_state;
    *score_state_out = score_state;
    *view_out = view;
    return YVEX_OK;

fail:
    free(kv_state);
    free(score_state);
    return rc;
}
// Purpose: Initialize rolling view init to its canonical fail-closed state.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_rolling_view_init(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long next_token_position,
    float *kv_state,
    float *score_state,
    yvex_attention_rolling_state_view *out)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long extent;
    unsigned long long i;
    int overlap;
    int rotated;
    if (!layer || !kv_state || !score_state || !out ||
        !yvex_attention_rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated) ||
        !yvex_core_u64_mul(state_slots, state_width, &extent))
        return 0;
    for (i = 0ull; i < extent; ++i) {
        kv_state[i] = 0.0f;
        score_state[i] = -INFINITY;
    }
    memset(out, 0, sizeof(*out));
    out->present = 1;
    out->schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    out->kind = kind;
    out->attention_class = layer->attention_class;
    out->layer_index = layer->layer_index;
    out->next_token_position = next_token_position;
    out->ratio = ratio;
    out->head_dimension = head_dim;
    out->state_width = state_width;
    out->state_slots = state_slots;
    out->cursor = next_token_position % ratio;
    out->kv_state_stride = state_width;
    out->score_state_stride = state_width;
    out->kv_state_extent = extent;
    out->score_state_extent = extent;
    out->kv_state = kv_state;
    out->score_state = score_state;
    out->overlap = overlap;
    out->rotated = rotated;
    return 1;
}
// Purpose: Return the admitted rolling output bind fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_rolling_output_bind(
    const yvex_attention_rolling_state_output *out,
    yvex_attention_rolling_state_view *view)
{
    memset(view, 0, sizeof(*view));
    view->present = out->present;
    view->schema_version = out->schema_version;
    view->kind = out->kind;
    view->attention_class = out->attention_class;
    view->layer_index = out->layer_index;
    view->next_token_position = out->next_token_position;
    view->ratio = out->ratio;
    view->head_dimension = out->head_dimension;
    view->state_width = out->state_width;
    view->state_slots = out->state_slots;
    view->previous_fill = out->previous_fill;
    view->current_fill = out->current_fill;
    view->cursor = out->cursor;
    view->kv_state_stride = out->kv_state_stride;
    view->score_state_stride = out->score_state_stride;
    view->kv_state_extent = out->kv_state_extent;
    view->score_state_extent = out->score_state_extent;
    view->kv_state = out->kv_state;
    view->score_state = out->score_state;
    view->overlap = out->overlap;
    view->rotated = out->rotated;
    memcpy(view->attention_plan_identity, out->attention_plan_identity,
           sizeof(view->attention_plan_identity));
}
// Purpose: Return the admitted execution trace release fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_execution_trace_release(
    yvex_attention_execution_trace *trace)
{
    if (!trace) return;
    free(trace->input);
    free(trace->q_low);
    free(trace->query);
    free(trace->raw_kv);
    free(trace->compressed_kv);
    free(trace->indexer_kv);
    free(trace->index_query);
    free(trace->index_weights);
    free(trace->attention_values);
    free(trace->output);
    free(trace->compressed_positions);
    free(trace->indexer_positions);
    free(trace->topk_counts);
    free(trace->topk_positions);
    free(trace->next_main_rolling_state.kv_state);
    free(trace->next_main_rolling_state.score_state);
    free(trace->next_indexer_rolling_state.kv_state);
    free(trace->next_indexer_rolling_state.score_state);
    memset(trace, 0, sizeof(*trace));
}
// Purpose: Apply the checked graph-local trace copy float invariant.
static int attention_trace_copy_float(float **out,
                                      const float *values,
                                      unsigned long long count)
{
    float *copy;
    if (!out || (count && !values)) return 0;
    *out = NULL;
    if (!count) return 1;
    copy = (float *)yvex_attention_calloc_array(count, sizeof(*copy));
    if (!copy) return 0;
    memcpy(copy, values, (size_t)count * sizeof(*copy));
    *out = copy;
    return 1;
}
// Purpose: Return the admitted trace capture fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_trace_capture(
    yvex_attention_execution_trace *trace,
    unsigned long long layer_index,
    yvex_attention_class attention_class,
    unsigned long long token_position,
    unsigned long long token_count,
    unsigned long long hidden_width,
    unsigned long long q_rank,
    unsigned long long query_width,
    unsigned long long kv_width,
    const float *input,
    const float *q_low,
    const float *query,
    const float *raw_kv,
    const float *compressed_kv,
    unsigned long long compressed_count,
    unsigned long long compressed_stride,
    const unsigned long long *compressed_positions,
    const float *indexer_kv,
    unsigned long long indexer_count,
    unsigned long long indexer_stride,
    const unsigned long long *indexer_positions,
    const float *index_query,
    unsigned long long index_query_stride,
    const float *index_weights,
    unsigned long long index_weight_stride,
    const float *attention_values,
    const float *output,
    const unsigned long long *topk_counts,
    const unsigned long long *topk_positions,
    unsigned long long topk_stride,
    const yvex_attention_rolling_state_output *main_state,
    const float *main_state_kv,
    const float *main_state_score,
    const yvex_attention_rolling_state_output *index_state,
    const float *index_state_kv,
    const float *index_state_score)
{
    unsigned long long count;
    if (!trace || trace->owned || !token_count || !hidden_width || !q_rank ||
        !query_width || !kv_width ||
        (compressed_count && !compressed_positions) ||
        (indexer_count && !indexer_positions) ||
        (topk_stride && (!topk_counts || !topk_positions)))
        return 0;
    memset(trace, 0, sizeof(*trace));
    trace->owned = 1;
    trace->layer_index = layer_index;
    trace->attention_class = attention_class;
    trace->token_position = token_position;
    trace->token_count = token_count;
    trace->hidden_width = hidden_width;
    trace->q_rank = q_rank;
    trace->query_width = query_width;
    trace->kv_width = kv_width;
    trace->compressed_count = compressed_count;
    trace->compressed_stride = compressed_stride;
    trace->indexer_count = indexer_count;
    trace->indexer_stride = indexer_stride;
    trace->index_query_stride = index_query_stride;
    trace->index_weight_stride = index_weight_stride;
    trace->topk_stride = topk_stride;
    if (!yvex_core_u64_mul(token_count, hidden_width, &count) ||
        !attention_trace_copy_float(&trace->input, input, count) ||
        !yvex_core_u64_mul(token_count, q_rank, &count) ||
        !attention_trace_copy_float(&trace->q_low, q_low, count) ||
        !yvex_core_u64_mul(token_count, query_width, &count) ||
        !attention_trace_copy_float(&trace->query, query, count) ||
        !yvex_core_u64_mul(token_count, kv_width, &count) ||
        !attention_trace_copy_float(&trace->raw_kv, raw_kv, count) ||
        !yvex_core_u64_mul(compressed_count, compressed_stride,
                                   &count) ||
        !attention_trace_copy_float(&trace->compressed_kv, compressed_kv,
                                    count) ||
        !yvex_core_u64_mul(indexer_count, indexer_stride, &count) ||
        !attention_trace_copy_float(&trace->indexer_kv, indexer_kv, count) ||
        !yvex_core_u64_mul(token_count, index_query_stride, &count) ||
        !attention_trace_copy_float(&trace->index_query, index_query, count) ||
        !yvex_core_u64_mul(token_count, index_weight_stride, &count) ||
        !attention_trace_copy_float(&trace->index_weights, index_weights,
                                    count) ||
        !yvex_core_u64_mul(token_count, query_width, &count) ||
        !attention_trace_copy_float(&trace->attention_values,
                                    attention_values, count) ||
        !yvex_core_u64_mul(token_count, hidden_width, &count) ||
        !attention_trace_copy_float(&trace->output, output, count)) {
        yvex_attention_execution_trace_release(trace);
        return 0;
    }
    if (compressed_count) {
        trace->compressed_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                compressed_count, sizeof(*trace->compressed_positions));
        if (!trace->compressed_positions) goto fail;
        memcpy(trace->compressed_positions, compressed_positions,
               (size_t)compressed_count *
                   sizeof(*trace->compressed_positions));
    }
    if (indexer_count) {
        trace->indexer_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                indexer_count, sizeof(*trace->indexer_positions));
        if (!trace->indexer_positions) goto fail;
        memcpy(trace->indexer_positions, indexer_positions,
               (size_t)indexer_count * sizeof(*trace->indexer_positions));
    }
    if (topk_stride) {
        trace->topk_counts =
            (unsigned long long *)yvex_attention_calloc_array(
                token_count, sizeof(*trace->topk_counts));
        if (!yvex_core_u64_mul(token_count, topk_stride, &count))
            goto fail;
        trace->topk_positions =
            (unsigned long long *)yvex_attention_calloc_array(
                count, sizeof(*trace->topk_positions));
        if (!trace->topk_counts || !trace->topk_positions) goto fail;
        memcpy(trace->topk_counts, topk_counts,
               (size_t)token_count * sizeof(*trace->topk_counts));
        memcpy(trace->topk_positions, topk_positions,
               (size_t)count * sizeof(*trace->topk_positions));
    }
    if (main_state && main_state->present) {
        trace->next_main_rolling_state = *main_state;
        trace->next_main_rolling_state.kv_state = NULL;
        trace->next_main_rolling_state.score_state = NULL;
        if (!attention_trace_copy_float(
                &trace->next_main_rolling_state.kv_state, main_state_kv,
                main_state->kv_state_extent) ||
            !attention_trace_copy_float(
                &trace->next_main_rolling_state.score_state,
                main_state_score, main_state->score_state_extent))
            goto fail;
    }
    if (index_state && index_state->present) {
        trace->next_indexer_rolling_state = *index_state;
        trace->next_indexer_rolling_state.kv_state = NULL;
        trace->next_indexer_rolling_state.score_state = NULL;
        if (!attention_trace_copy_float(
                &trace->next_indexer_rolling_state.kv_state, index_state_kv,
                index_state->kv_state_extent) ||
            !attention_trace_copy_float(
                &trace->next_indexer_rolling_state.score_state,
                index_state_score, index_state->score_state_extent))
            goto fail;
    }
    trace->complete = 1;
    return 1;
fail:
    yvex_attention_execution_trace_release(trace);
    return 0;
}
// Purpose: Release graph-owned resources held by component release.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void attention_component_release(
    yvex_attention_component_span *span)
{
    if (!span) return;
    free(span->data);
    memset(span, 0, sizeof(*span));
}
// Purpose: Release graph-owned resources held by transaction release staging.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static void attention_transaction_release_staging(
    yvex_attention_state_transaction *transaction)
{
    unsigned int i;
    if (!transaction) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&transaction->components[i]);
}
// Purpose: Construct component prepare f32 with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_component_prepare_f32(
    yvex_attention_component_span *span,
    yvex_attention_component_kind kind,
    unsigned int rank,
    unsigned long long dim0,
    unsigned long long dim1,
    unsigned long long stride,
    unsigned long long position_start,
    unsigned long long position_count)
{
    unsigned long long elements;
    unsigned long long bytes;
    if (!span || rank == 0u || rank > 4u || dim0 == 0ull ||
        (rank > 1u && dim1 == 0ull) || stride < (rank > 1u ? dim1 : dim0))
        return 0;
    if (rank > 1u) {
        if (!yvex_core_u64_mul(dim0, stride, &elements))
            return 0;
    } else {
        elements = dim0;
    }
    if (!yvex_core_u64_mul(elements, sizeof(float), &bytes) ||
        bytes > (unsigned long long)SIZE_MAX)
        return 0;
    memset(span, 0, sizeof(*span));
    span->kind = kind;
    span->storage = YVEX_DEEPSEEK_ATTENTION_COMPONENT_STORAGE_F32;
    span->rank = rank;
    span->dims[0] = dim0;
    span->dims[1] = rank > 1u ? dim1 : 0ull;
    span->stride = stride;
    span->expected_elements = elements;
    span->byte_extent = bytes;
    span->position_start = position_start;
    span->position_count = position_count;
    span->required = 1;
    return 1;
}
// Purpose: Return the admitted emission count fact without transferring ownership.
static int attention_emission_count(
    unsigned long long token_position,
    unsigned long long token_count,
    unsigned long long ratio,
    unsigned long long *out)
{
    unsigned long long i;
    unsigned long long count = 0ull;
    if (!out || ratio == 0ull) return 0;
    for (i = 0ull; i < token_count; ++i) {
        if (token_position > ULLONG_MAX - i - 1ull) return 0;
        if (((token_position + i + 1ull) % ratio) == 0ull) {
            if (count == ULLONG_MAX) return 0;
            count++;
        }
    }
    *out = count;
    return 1;
}
// Purpose: Construct transaction allocate staging with checked geometry and explicit ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_transaction_allocate_staging(
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned int i;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        yvex_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        span->data = calloc(1u, (size_t)span->byte_extent);
        if (!span->data) {
            attention_transaction_release_staging(transaction);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                span->byte_extent, 0ull, err, YVEX_ERR_NOMEM,
                "DeepSeek attention state transaction staging allocation failed");
        }
    }
    return YVEX_OK;
}
// Purpose: Derive the deterministic transaction identity identity from explicit semantic fields.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_transaction_identity(
    const yvex_attention_state_transaction *transaction,
    char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned int i;
    if (!transaction || !out) return 0;
    yvex_sha256_init(&hash);
    if (!yvex_attention_hash_text(&hash, "yvex.deepseek.attention.delta.v1") ||
        !yvex_attention_hash_u64(&hash, transaction->layer_index) ||
        !yvex_attention_hash_u64(&hash, transaction->attention_class) ||
        !yvex_attention_hash_u64(&hash, transaction->token_position) ||
        !yvex_attention_hash_u64(&hash, transaction->token_count) ||
        !yvex_attention_hash_text(&hash, transaction->previous_state_identity))
        return 0;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        if (!yvex_attention_hash_u64(&hash, span->kind) ||
            !yvex_attention_hash_u64(&hash, span->rank) ||
            !yvex_attention_hash_u64(&hash, span->dims[0]) ||
            !yvex_attention_hash_u64(&hash, span->dims[1]) ||
            !yvex_attention_hash_u64(&hash, span->stride) ||
            !yvex_attention_hash_u64(&hash, span->produced_elements) ||
            !yvex_attention_hash_u64(&hash, span->byte_extent) ||
            !yvex_sha256_update(&hash, span->data, (size_t)span->byte_extent))
            return 0;
    }
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}
// Purpose: Initialize component values are finite to its canonical fail-closed state.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static int attention_component_values_are_finite(
    const yvex_attention_component_span *span)
{
    const float *values;
    unsigned long long i;
    if (!span || span->storage != YVEX_DEEPSEEK_ATTENTION_COMPONENT_STORAGE_F32 ||
        !span->data)
        return 0;
    if (span->kind == YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE ||
        span->kind == YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE)
        return 1;
    values = (const float *)span->data;
    for (i = 0ull; i < span->expected_elements; ++i) {
        if (!isfinite(values[i])) return 0;
    }
    return 1;
}
// Purpose: Return the admitted memory sink init fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_memory_sink_init(
    yvex_attention_memory_sink *sink,
    const yvex_attention_memory_sink_options *options,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!sink)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention memory sink requires caller storage");
    memset(sink, 0, sizeof(*sink));
    sink->initialized = 1;
    sink->options = options ? *options : memory_sink_defaults;
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted memory sink release fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
void yvex_attention_memory_sink_release(
    yvex_attention_memory_sink *sink)
{
    unsigned int i;
    if (!sink) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&sink->committed[i]);
    memset(sink, 0, sizeof(*sink));
}
// Purpose: Return the admitted state transaction begin fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_state_transaction_begin(
    yvex_attention_memory_sink *sink,
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    unsigned long long token_position,
    unsigned long long token_count,
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long compressed_count = 0ull;
    int rc;
    if (transaction) memset(transaction, 0, sizeof(*transaction));
    if (!sink || !sink->initialized || !layer || !history || !transaction ||
        token_count == 0ull)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, token_count, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention state transaction requires sink, layer, history, and tokens");
    if (sink->options.fail_begin)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected begin failure");
    rc = yvex_attention_history_validate(layer, history, failure, err);
    if (rc != YVEX_OK) return rc;
    if (history->token_count != token_position)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, history->token_count,
            token_position, err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction token position is not contiguous");
    if (token_position > ULLONG_MAX - token_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            token_position, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state transaction token range overflows");
    memset(transaction, 0, sizeof(*transaction));
    transaction->status = YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN;
    transaction->sink = sink;
    transaction->layer_index = layer->layer_index;
    transaction->attention_class = layer->attention_class;
    transaction->token_position = token_position;
    transaction->token_count = token_count;
    (void)snprintf(transaction->previous_state_identity,
                   sizeof(transaction->previous_state_identity), "%s",
                   sink->has_committed ? sink->committed_identity : "initial");
    if (!attention_component_prepare_f32(
            &transaction->components[
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT],
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_ATTENTION_OUTPUT, 2u,
            token_count, layer->hidden_dimension,
            layer->hidden_dimension, token_position,
            token_count) ||
        !attention_component_prepare_f32(
            &transaction->components[
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV],
            YVEX_DEEPSEEK_ATTENTION_COMPONENT_RAW_LOCAL_KV, 2u,
            token_count, layer->head_dimension, layer->head_dimension,
            token_position, token_count))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, token_count, 0ull,
            err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state transaction output extent overflowed");
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        const yvex_attention_rolling_state_view *main_state =
            &history->main_rolling_state;
        if (!attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_KV_STATE, 2u,
                main_state->state_slots, main_state->state_width,
                main_state->kv_state_stride, token_position, token_count) ||
            !attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_MAIN_SCORE_STATE, 2u,
                main_state->state_slots, main_state->state_width,
                main_state->score_state_stride, token_position, token_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                main_state->state_slots, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention main rolling-state extent overflowed");
        if (!attention_emission_count(token_position, token_count,
                                      layer->compression_ratio,
                                      &compressed_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                layer->compression_ratio, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention compression emission count overflowed");
        if (compressed_count &&
            !attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_COMPRESSED_MAIN_KV, 2u,
                compressed_count, layer->head_dimension, layer->head_dimension,
                token_position, compressed_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                compressed_count, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention compressed KV extent overflowed");
    }
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        const yvex_attention_rolling_state_view *index_state =
            &history->indexer_rolling_state;
        if (!attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV_STATE, 2u,
                index_state->state_slots, index_state->state_width,
                index_state->kv_state_stride, token_position, token_count) ||
            !attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_SCORE_STATE, 2u,
                index_state->state_slots, index_state->state_width,
                index_state->score_state_stride, token_position, token_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                index_state->state_slots, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention indexer rolling-state extent overflowed");
        if (compressed_count &&
            !attention_component_prepare_f32(
                &transaction->components[
                    YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV],
                YVEX_DEEPSEEK_ATTENTION_COMPONENT_INDEXER_KV, 2u,
                compressed_count, layer->indexer_head_dimension,
                layer->indexer_head_dimension, token_position,
                compressed_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                compressed_count, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention indexer KV extent overflowed");
    }
    rc = attention_transaction_allocate_staging(transaction, failure, err);
    if (rc != YVEX_OK) return rc;
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted state transaction acquire fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_state_transaction_acquire(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    yvex_attention_component_span *out,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_component_span *span;
    if (out) memset(out, 0, sizeof(*out));
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction acquire requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->data || span->acquired)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            span->acquired, err, YVEX_ERR_STATE,
            "DeepSeek attention state component is absent or already acquired");
    if (transaction->sink &&
        transaction->sink->options.fail_acquire_kind == kind)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected acquire failure");
    span->acquired = 1;
    if (out) *out = *span;
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted state transaction seal fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_state_transaction_seal(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    unsigned long long produced_elements,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_component_span *span;
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction seal requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->acquired || span->sealed ||
        produced_elements != span->expected_elements)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            span->expected_elements, produced_elements, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state component seal has wrong element count");
    if (transaction->sink && transaction->sink->options.fail_seal_kind == kind)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected seal failure");
    if (!attention_component_values_are_finite(span))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_FORMAT,
            "DeepSeek attention state component contains non-finite values");
    span->produced_elements = produced_elements;
    span->written = 1;
    span->sealed = 1;
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted state transaction commit fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_state_transaction_commit(
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_component_span next[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    unsigned int i;
    memset(next, 0, sizeof(next));
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        !transaction->sink || !transaction->sink->initialized)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction commit requires begun state");
    if (transaction->sink->options.fail_commit)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected commit failure");
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        if (!span->acquired || !span->written || !span->sealed ||
            span->produced_elements != span->expected_elements)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                span->expected_elements, span->produced_elements, err,
                YVEX_ERR_STATE,
                "DeepSeek attention state transaction cannot commit incomplete component");
    }
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        next[i] = *span;
        next[i].data = malloc((size_t)span->byte_extent);
        if (!next[i].data) {
            unsigned int j;
            for (j = 0u; j < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++j)
                attention_component_release(&next[j]);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                span->byte_extent, 0ull, err, YVEX_ERR_NOMEM,
                "DeepSeek attention state commit allocation failed");
        }
        memcpy(next[i].data, span->data, (size_t)span->byte_extent);
    }
    if (!attention_transaction_identity(transaction,
                                        transaction->transaction_identity)) {
        for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
            attention_component_release(&next[i]);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_FORMAT,
            "DeepSeek attention state transaction identity failed");
    }
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        attention_component_release(&transaction->sink->committed[i]);
        transaction->sink->committed[i] = next[i];
        memset(&next[i], 0, sizeof(next[i]));
    }
    (void)snprintf(transaction->sink->committed_identity,
                   sizeof(transaction->sink->committed_identity), "%s",
                   transaction->transaction_identity);
    transaction->sink->has_committed = 1;
    attention_transaction_release_staging(transaction);
    transaction->status = YVEX_DEEPSEEK_ATTENTION_TRANSACTION_COMMITTED;
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted state transaction abort fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_state_transaction_abort(
    yvex_attention_state_transaction *transaction,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction abort requires begun state");
    attention_transaction_release_staging(transaction);
    transaction->status = YVEX_DEEPSEEK_ATTENTION_TRANSACTION_ABORTED;
    if (transaction->sink && transaction->sink->options.fail_abort)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected abort failure");
    return yvex_attention_accept(failure, err);
}
// Purpose: Return the admitted memory sink committed component fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const yvex_attention_component_span *
yvex_attention_memory_sink_committed_component(
    const yvex_attention_memory_sink *sink,
    yvex_attention_component_kind kind)
{
    if (!sink || !sink->initialized || !sink->has_committed ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT ||
        !sink->committed[kind].required)
        return NULL;
    return &sink->committed[kind];
}
// Purpose: Return the admitted memory sink identity fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const char *yvex_attention_memory_sink_identity(
    const yvex_attention_memory_sink *sink)
{
    if (!sink || !sink->initialized || !sink->has_committed) return NULL;
    return sink->committed_identity;
}
