/* Owner: graph.attention protocol.
 * Owns: history validation, rolling recurrence, segment selection, and sparse top-k.
 * Does not own: family scheduling, encoded reads, output projection, transactions, CUDA, or KV.
 * Invariants: history is immutable and causal selection is deterministic.
 * Boundary: protocol truth does not promote complete attention, persistent KV, or generation.
 * Purpose: provide reusable state and selection mechanisms to graph-family recipes.
 * Inputs: admitted layer plans, immutable history, positions, and bounded numeric views.
 * Effects: mutates only caller-owned outputs and explicitly owned rolling state.
 * Failure: typed validation or numeric refusal leaves capability flags unchanged. */
#include "src/graph/private.h"
// Purpose: Return the admitted segment row fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const float *yvex_attention_segment_row(
    const float *history,
    unsigned long long history_count,
    unsigned long long history_stride,
    const float *current,
    unsigned long long current_count,
    unsigned long long current_stride,
    unsigned long long index)
{
    if (index < history_count)
        return history + index * history_stride;
    index -= history_count;
    if (index >= current_count) return NULL;
    return current + index * current_stride;
}

// Purpose: Return the admitted segment position fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_attention_segment_position(
    const unsigned long long *history,
    unsigned long long history_count,
    const unsigned long long *current,
    unsigned long long current_count,
    unsigned long long index)
{
    if (index < history_count) return history[index];
    index -= history_count;
    if (index >= current_count) return ULLONG_MAX;
    return current[index];
}

/* Purpose: sample one borrowed cooperative-cancellation predicate at a named safe point.
 * Inputs: optional borrowed predicate, layer identity, stable safe-point text, and diagnostics.
 * Effects: invokes only the caller-owned predicate and otherwise mutates diagnostics on refusal.
 * Failure: malformed predicates refuse as invalid arguments; requested cancellation is typed.
 * Boundary: observes cancellation but never owns, resets, or releases the caller's token. */
int yvex_attention_cancel_check(
    const yvex_attention_cancellation *cancellation,
    unsigned long long layer_index,
    const char *safe_point,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!cancellation) return YVEX_OK;
    if (!cancellation->requested)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "attention cancellation requires a borrowed predicate");
    if (cancellation->requested(cancellation->context))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED, NULL,
            layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull, err,
            YVEX_ERR_CANCELLED,
            safe_point ? safe_point : "attention execution cancelled");
    return YVEX_OK;
}

/* Purpose: validate one family-selected attention class before payload access or state mutation.
 * Inputs: immutable layer geometry and the family's exact CSA/HCA compression ratios.
 * Effects: reads geometry only and clears no admitted identity or state.
 * Failure: inconsistent common, SWA, CSA, or HCA geometry returns a typed dimension refusal.
 * Boundary: validates generic class invariants while the family supplies its ratio policy. */
int yvex_attention_class_geometry_validate(
    const yvex_attention_layer_plan *layer,
    unsigned long long csa_ratio,
    unsigned long long hca_ratio,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long query_width;
    unsigned long long indexer_width = 0ull;
    unsigned long long output_group_width;
    unsigned long long output_width;
    unsigned long long output_low_width;
    int valid = layer && layer->sliding_window && layer->query_heads &&
        layer->kv_heads && layer->head_dimension && layer->hidden_dimension &&
        layer->rope_head_dimension <= layer->head_dimension &&
        layer->query_heads % layer->kv_heads == 0ull && layer->output_groups &&
        layer->query_heads % layer->output_groups == 0ull &&
        layer->output_lora_rank &&
        yvex_core_u64_mul(layer->query_heads, layer->head_dimension,
                          &query_width) &&
        yvex_core_u64_mul(layer->query_heads / layer->output_groups,
                          layer->head_dimension, &output_group_width) &&
        yvex_core_u64_mul(layer->output_groups, output_group_width,
                          &output_width) &&
        output_width == query_width &&
        yvex_core_u64_mul(layer->output_groups, layer->output_lora_rank,
                          &output_low_width);

    if (!valid)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_FORMAT,
            "attention common head, position, or output geometry is invalid");
    switch (layer->attention_class) {
    case YVEX_ATTENTION_CLASS_SWA:
        valid = layer->compression_ratio == 0ull &&
            !layer->compressor_required && !layer->indexer_required &&
            !layer->indexer_heads && !layer->indexer_head_dimension &&
            !layer->indexer_topk && !layer->sparse_topk.required &&
            !layer->sparse_topk.k;
        break;
    case YVEX_ATTENTION_CLASS_CSA:
        valid = csa_ratio && layer->compression_ratio == csa_ratio &&
            layer->compressor_required && layer->indexer_required &&
            layer->indexer_heads && layer->indexer_head_dimension &&
            yvex_core_u64_mul(layer->indexer_heads,
                              layer->indexer_head_dimension,
                              &indexer_width) && indexer_width &&
            layer->indexer_topk && layer->sparse_topk.required &&
            layer->sparse_topk.k == layer->indexer_topk;
        break;
    case YVEX_ATTENTION_CLASS_HCA:
        valid = hca_ratio && layer->compression_ratio == hca_ratio &&
            layer->compressor_required && !layer->indexer_required &&
            !layer->indexer_heads && !layer->indexer_head_dimension &&
            !layer->indexer_topk && !layer->sparse_topk.required &&
            !layer->sparse_topk.k;
        break;
    default:
        valid = 0;
        break;
    }
    if (!valid)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->attention_class == YVEX_ATTENTION_CLASS_HCA
                ? hca_ratio : csa_ratio,
            layer->compression_ratio, err, YVEX_ERR_FORMAT,
            "attention class geometry does not match the family contract");
    return yvex_attention_accept(failure, err);
}

// Purpose: Return the admitted csa select fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_csa_select(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    const float *current_indexer,
    unsigned long long current_indexer_count,
    unsigned long long current_indexer_stride,
    const unsigned long long *current_indexer_positions,
    const float *index_query,
    const float *index_weights,
    unsigned long long query_position,
    unsigned long long *selected,
    unsigned long long *selected_count,
    unsigned long long *valid_count,
    yvex_attention_scratch_budget *scratch,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long total;
    float *scores = NULL;
    unsigned long long *ordinals = NULL;
    unsigned long long *valid_indexes = NULL;
    unsigned long long candidate;
    unsigned long long valid = 0ull;
    size_t base_reserved = 0u;
    size_t ranked_reserved = 0u;
    int rc;

    if (selected_count) *selected_count = 0ull;
    if (valid_count) *valid_count = 0ull;
    if (!layer || !history || !index_query || !index_weights || !selected ||
        !selected_count || !valid_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
            "CSA selection requires history, query, weights, and outputs");
    if (history->indexer_entry_count >
        ULLONG_MAX - current_indexer_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            history->indexer_entry_count, err, YVEX_ERR_BOUNDS,
            "CSA candidate count overflowed");
    total = history->indexer_entry_count + current_indexer_count;
    if (!total) return YVEX_OK;
    if (!yvex_attention_scratch_reserve(
            scratch, total,
            sizeof(*scores) + sizeof(*ordinals) + sizeof(*valid_indexes),
            &base_reserved))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            scratch ? scratch->limit_bytes : 0ull,
            scratch ? (unsigned long long)scratch->live_bytes : 0ull,
            err, YVEX_ERR_BOUNDS,
            "CSA selection exceeds the attention scratch budget");
    scores = (float *)yvex_attention_calloc_array(total, sizeof(*scores));
    ordinals = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*ordinals));
    valid_indexes = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*valid_indexes));
    if (!scores || !ordinals || !valid_indexes) {
        free(scores);
        free(ordinals);
        free(valid_indexes);
        yvex_attention_scratch_release(scratch, base_reserved);
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, total, 0ull, err,
            YVEX_ERR_NOMEM, "CSA selection scratch allocation failed");
    }
    for (candidate = 0ull; candidate < total; ++candidate) {
        const float *key = yvex_attention_segment_row(
            history->indexer_kv, history->indexer_entry_count,
            history->indexer_kv_stride, current_indexer,
            current_indexer_count, current_indexer_stride, candidate);
        unsigned long long position = yvex_attention_segment_position(
            history->indexer_positions, history->indexer_entry_count,
            current_indexer_positions, current_indexer_count, candidate);
        unsigned long long head;
        double score = 0.0;

        if (!key || position == ULLONG_MAX || position > query_position ||
            position > ULLONG_MAX - layer->compression_ratio + 1ull ||
            position + layer->compression_ratio - 1ull > query_position)
            continue;
        for (head = 0ull; head < layer->indexer_heads; ++head) {
            const float *q = index_query +
                             head * layer->indexer_head_dimension;
            unsigned long long lane;
            double dot = 0.0;
            for (lane = 0ull; lane < layer->indexer_head_dimension; ++lane) {
                if (!isfinite(q[lane]) || !isfinite(key[lane])) goto numeric;
                dot += (double)q[lane] * (double)key[lane];
            }
            if (dot < 0.0) dot = 0.0;
            if (!isfinite(index_weights[head])) goto numeric;
            score += dot * (double)index_weights[head];
        }
        score *= 1.0 / sqrt((double)layer->indexer_head_dimension);
        score *= 1.0 / sqrt((double)layer->indexer_heads);
        if (!isfinite(score)) goto numeric;
        scores[valid] = (float)score;
        ordinals[valid] = position;
        valid_indexes[valid] = candidate;
        valid++;
    }
    if (valid) {
        unsigned long long ranked_capacity =
            yvex_attention_min_u64(valid, layer->sparse_topk.k);
        unsigned long long ranked_count = 0ull;
        unsigned long long *ranked;
        if (!yvex_attention_scratch_reserve(
                scratch, ranked_capacity, sizeof(*ranked),
                &ranked_reserved)) {
            free(scores);
            free(ordinals);
            free(valid_indexes);
            yvex_attention_scratch_release(scratch, base_reserved);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                scratch ? scratch->limit_bytes : 0ull,
                scratch ? (unsigned long long)scratch->live_bytes : 0ull,
                err, YVEX_ERR_BOUNDS,
                "CSA ranked selection exceeds the attention scratch budget");
        }
        ranked = (unsigned long long *)yvex_attention_calloc_array(
            ranked_capacity, sizeof(*ranked));
        unsigned long long i;
        if (!ranked) {
            free(scores);
            free(ordinals);
            free(valid_indexes);
            yvex_attention_scratch_release(scratch, ranked_reserved);
            yvex_attention_scratch_release(scratch, base_reserved);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, valid, 0ull,
                err, YVEX_ERR_NOMEM,
                "CSA ranked-selection scratch allocation failed");
        }
        rc = yvex_attention_topk_select(
            scores, ordinals, valid, layer->sparse_topk.k, ranked,
            &ranked_count, failure, err);
        if (rc != YVEX_OK) {
            free(ranked);
            free(scores);
            free(ordinals);
            free(valid_indexes);
            yvex_attention_scratch_release(scratch, ranked_reserved);
            yvex_attention_scratch_release(scratch, base_reserved);
            return rc;
        }
        for (i = 0ull; i < ranked_count; ++i)
            selected[i] = valid_indexes[ranked[i]];
        *selected_count = ranked_count;
        free(ranked);
        yvex_attention_scratch_release(scratch, ranked_reserved);
    }
    *valid_count = valid;
    free(scores);
    free(ordinals);
    free(valid_indexes);
    yvex_attention_scratch_release(scratch, base_reserved);
    return YVEX_OK;

numeric:
    free(scores);
    free(ordinals);
    free(valid_indexes);
    yvex_attention_scratch_release(scratch, base_reserved);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, candidate, err,
        YVEX_ERR_FORMAT, "CSA index scoring produced non-finite values");
}

// Purpose: Return the admitted rolling geometry fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_rolling_geometry(
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    unsigned long long *ratio,
    unsigned long long *head_dim,
    unsigned long long *state_width,
    unsigned long long *state_slots,
    int *overlap,
    int *rotated)
{
    unsigned long long coeff;

    if (!layer || !ratio || !head_dim || !state_width || !state_slots ||
        !overlap || !rotated || layer->compression_ratio == 0ull)
        return 0;
    if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN) {
        *ratio = layer->compression_ratio;
        *head_dim = layer->head_dimension;
        *overlap =
            layer->attention_class == YVEX_ATTENTION_CLASS_CSA ? 1 : 0;
        *rotated = 0;
    } else if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER) {
        if (layer->attention_class != YVEX_ATTENTION_CLASS_CSA)
            return 0;
        *ratio = layer->compression_ratio;
        *head_dim = layer->indexer_head_dimension;
        *overlap = 1;
        *rotated = 1;
    } else {
        return 0;
    }
    coeff = *overlap ? 2ull : 1ull;
    if (!yvex_core_u64_mul(*head_dim, coeff, state_width) ||
        !yvex_core_u64_mul(*ratio, coeff, state_slots))
        return 0;
    return *ratio != 0ull && *head_dim != 0ull;
}

// Purpose: Initialize rolling active values are finite to its canonical fail-closed state.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

static int attention_rolling_active_values_are_finite(
    const float *kv_state,
    const float *score_state,
    unsigned long long kv_stride,
    unsigned long long score_stride,
    unsigned long long head_dim,
    unsigned long long ratio,
    unsigned long long previous_fill,
    unsigned long long current_fill,
    int overlap)
{
    unsigned long long slot;
    unsigned long long lane;

    if (!kv_state || !score_state) return 0;
    /* The previous half participates in the first overlap emission even
     * before it contains a completed group.  Its fail-closed score sentinel
     * is therefore part of the admitted state, not unused storage. */
    if (overlap && previous_fill == 0ull) {
        for (slot = 0ull; slot < ratio; ++slot) {
            for (lane = 0ull; lane < head_dim; ++lane) {
                float score = score_state[slot * score_stride + lane];
                if (!isinf(score) || !signbit(score)) return 0;
            }
        }
    }
    for (slot = 0ull; overlap && slot < previous_fill; ++slot) {
        for (lane = 0ull; lane < head_dim; ++lane) {
            if (!isfinite(kv_state[slot * kv_stride + lane]) ||
                !isfinite(score_state[slot * score_stride + lane]))
                return 0;
        }
    }
    for (slot = 0ull; slot < current_fill; ++slot) {
        unsigned long long base = overlap ? ratio + slot : slot;
        unsigned long long lane_offset = overlap ? head_dim : 0ull;
        for (lane = 0ull; lane < head_dim; ++lane) {
            if (!isfinite(kv_state[base * kv_stride + lane + lane_offset]) ||
                !isfinite(score_state[base * score_stride + lane + lane_offset]))
                return 0;
        }
    }
    return 1;
}

// Purpose: Return the admitted rolling state validate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

static int attention_rolling_state_validate(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_rolling_state_view *state,
    yvex_attention_rolling_kind kind,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long required_extent;
    unsigned long long expected_cursor;
    unsigned long long expected_previous_fill;
    int overlap;
    int rotated;

    if (!layer || !state || !state->present)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling state is missing");
    if (!yvex_attention_rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention rolling state is not used by this class");
    if (state->schema_version !=
            YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1 ||
        state->kind != kind || state->attention_class != layer->attention_class ||
        state->layer_index != layer->layer_index || state->ratio != ratio ||
        state->head_dimension != head_dim || state->state_width != state_width ||
        state->state_slots != state_slots || state->overlap != overlap ||
        state->rotated != rotated)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->state_width, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state identity or geometry mismatch");
    if (state->cursor >= ratio || state->previous_fill > ratio ||
        state->current_fill > ratio || (!overlap && state->previous_fill))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ratio,
            state->cursor, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state cursor or fill is invalid");
    expected_cursor = state->next_token_position % ratio;
    expected_previous_fill =
        overlap && state->next_token_position >= ratio ? ratio : 0ull;
    if (state->cursor != expected_cursor ||
        state->current_fill != expected_cursor ||
        state->previous_fill != expected_previous_fill)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            expected_cursor, state->cursor, err, YVEX_ERR_STATE,
            "DeepSeek attention rolling state does not match its token position");
    if (state->kv_state_stride < state_width ||
        state->score_state_stride < state_width || !state->kv_state ||
        !state->score_state)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->kv_state_stride, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state storage is incomplete");
    if (!yvex_core_u64_mul(state_slots, state->kv_state_stride,
                                   &required_extent) ||
        state->kv_state_extent < required_extent ||
        !yvex_core_u64_mul(state_slots, state->score_state_stride,
                                   &required_extent) ||
        state->score_state_extent < required_extent)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_slots,
            state->kv_state_extent, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state extent is too small");
    if (!attention_rolling_active_values_are_finite(
            state->kv_state, state->score_state, state->kv_state_stride,
            state->score_state_stride, head_dim, ratio, state->previous_fill,
            state->current_fill, overlap))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state contains non-finite active values");
    return yvex_attention_accept(failure, err);
}

/* Purpose: require the unique complete local and compressed history prefix.
 * Inputs: admitted layer semantics and one immutable next-token history view.
 * Effects: reads positions and cardinalities only; publishes no state.
 * Failure: returns typed missing, extra, unordered, or overflow refusal.
 * Boundary: validates attention-local history, never persistent runtime KV. */
static int attention_history_positions_validate(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long local_capacity;
    unsigned long long expected_local;
    unsigned long long local_start;
    unsigned long long expected_compressed = 0ull;
    unsigned long long i;

    if (!layer->sliding_window)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention history requires a nonzero sliding window");
    local_capacity = layer->sliding_window - 1ull;
    expected_local = history->token_count < local_capacity
        ? history->token_count : local_capacity;
    local_start = history->token_count - expected_local;
    if (history->local_tail_count != expected_local)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, expected_local,
            history->local_tail_count, err,
            history->local_tail_count > expected_local
                ? YVEX_ERR_BOUNDS : YVEX_ERR_FORMAT,
            "DeepSeek attention local history is not the complete window suffix");
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        if (!layer->compression_ratio)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
                err, YVEX_ERR_FORMAT,
                "compressed attention history requires a nonzero ratio");
        expected_compressed =
            history->token_count / layer->compression_ratio;
    }
    if (history->compressed_entry_count != expected_compressed)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            expected_compressed, history->compressed_entry_count, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention compressed history is not the complete prefix");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        history->indexer_entry_count != expected_compressed)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            expected_compressed, history->indexer_entry_count, err,
            YVEX_ERR_FORMAT,
            "CSA indexer history is not the complete compressed prefix");
    for (i = 0ull; i < expected_local; ++i) {
        if (history->local_positions[i] != local_start + i)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                local_start + i, history->local_positions[i], err,
                YVEX_ERR_FORMAT,
                "local history does not contain the exact contiguous suffix");
    }
    for (i = 0ull; i < expected_compressed; ++i) {
        unsigned long long expected_position = 0ull;

        if (!yvex_core_u64_mul(i, layer->compression_ratio,
                               &expected_position) ||
            history->compressed_positions[i] != expected_position ||
            (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
             history->indexer_positions[i] != expected_position))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                expected_position, history->compressed_positions[i], err,
                YVEX_ERR_FORMAT,
                "compressed history does not contain every completed ratio group");
    }
    return yvex_attention_accept(failure, err);
}

// Purpose: Apply the checked graph-local rolling copy state invariant.

static void attention_rolling_copy_state(
    const yvex_attention_rolling_state_view *before,
    yvex_attention_rolling_state_output *after)
{
    unsigned long long slot;

    for (slot = 0ull; slot < before->state_slots; ++slot) {
        memcpy(after->kv_state + slot * after->kv_state_stride,
               before->kv_state + slot * before->kv_state_stride,
               (size_t)(before->state_width * sizeof(float)));
        memcpy(after->score_state + slot * after->score_state_stride,
               before->score_state + slot * before->score_state_stride,
               (size_t)(before->state_width * sizeof(float)));
    }
}

// Purpose: Implement the graph-local rolling emit semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

static int attention_rolling_emit(
    const yvex_attention_rolling_state_output *state,
    unsigned long long head_dim,
    unsigned long long ratio,
    int overlap,
    float *compressed_out,
    unsigned long long compressed_out_count)
{
    unsigned long long lane;
    unsigned long long slot;

    if (!state || !compressed_out || compressed_out_count < head_dim)
        return 0;
    for (lane = 0ull; lane < head_dim; ++lane) {
        double max_score = -HUGE_VAL;
        double denom = 0.0;
        double value = 0.0;
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride +
                                              lane];
            if (overlap) {
                double score2 =
                    state->score_state[(ratio + slot) *
                                           state->score_state_stride +
                                       lane + head_dim];
                if (score2 > max_score) max_score = score2;
            }
            if (score > max_score) max_score = score;
        }
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride +
                                              lane];
            double weight = exp(score - max_score);
            denom += weight;
            value += weight *
                     (double)state->kv_state[slot * state->kv_state_stride +
                                             lane];
            if (overlap) {
                double score2 =
                    state->score_state[(ratio + slot) *
                                           state->score_state_stride +
                                       lane + head_dim];
                double weight2 = exp(score2 - max_score);
                denom += weight2;
                value +=
                    weight2 *
                    (double)state->kv_state[(ratio + slot) *
                                                state->kv_state_stride +
                                            lane + head_dim];
            }
        }
        if (!isfinite(denom) || denom <= 0.0 || !isfinite(value)) return 0;
        compressed_out[lane] = (float)(value / denom);
    }
    return 1;
}

// Purpose: Return the admitted rolling state step cpu fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_rolling_state_step_cpu(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_rolling_state_view *before,
    const float *token_kv,
    const float *token_score,
    const float *ape_row,
    yvex_attention_rolling_state_output *after,
    float *compressed_out,
    unsigned long long compressed_out_count,
    int *emitted,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long ratio;
    unsigned long long head_dim;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long required_extent;
    unsigned long long slot;
    unsigned long long lane;
    float *after_kv_state;
    float *after_score_state;
    unsigned long long after_kv_stride;
    unsigned long long after_score_stride;
    unsigned long long after_kv_extent;
    unsigned long long after_score_extent;
    int overlap;
    int rotated;
    int should_emit;
    int rc;

    if (emitted) *emitted = 0;
    rc = attention_rolling_state_validate(
        layer, before, before ? before->kind : YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE,
        failure, err);
    if (rc != YVEX_OK) return rc;
    if (!token_kv || !token_score || !ape_row || !after)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling transition requires token vectors and output state");
    if (before->next_token_position == ULLONG_MAX)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            before->next_token_position, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling token position would overflow");
    if (!yvex_attention_rolling_geometry(
            layer, before->kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention rolling transition lacks geometry");
    if (!after->kv_state || !after->score_state ||
        after->kv_state_stride < state_width ||
        after->score_state_stride < state_width ||
        !yvex_core_u64_mul(state_slots, after->kv_state_stride,
                                   &required_extent) ||
        after->kv_state_extent < required_extent ||
        !yvex_core_u64_mul(state_slots, after->score_state_stride,
                                   &required_extent) ||
        after->score_state_extent < required_extent)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            after ? after->kv_state_stride : 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling output storage is incomplete");
    if (before->cursor != (before->next_token_position % ratio))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            before->next_token_position % ratio, before->cursor, err,
            YVEX_ERR_STATE,
            "DeepSeek attention rolling cursor is stale for token position");
    after_kv_state = after->kv_state;
    after_score_state = after->score_state;
    after_kv_stride = after->kv_state_stride;
    after_score_stride = after->score_state_stride;
    after_kv_extent = after->kv_state_extent;
    after_score_extent = after->score_state_extent;
    for (lane = 0ull; lane < state_width; ++lane) {
        if (!isfinite(token_kv[lane]) || !isfinite(token_score[lane]) ||
            !isfinite(ape_row[lane]))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, lane, err,
                YVEX_ERR_FORMAT,
                "DeepSeek attention rolling transition input is non-finite");
    }
    *after = (yvex_attention_rolling_state_output){
        .present = 1,
        .schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1,
        .kind = before->kind,
        .attention_class = before->attention_class,
        .layer_index = before->layer_index,
        .next_token_position = before->next_token_position + 1ull,
        .ratio = ratio,
        .head_dimension = head_dim,
        .state_width = state_width,
        .state_slots = state_slots,
        .previous_fill = before->previous_fill,
        .current_fill = before->current_fill,
        .cursor = (before->cursor + 1ull) % ratio,
        .kv_state_stride = after_kv_stride,
        .score_state_stride = after_score_stride,
        .kv_state_extent = after_kv_extent,
        .score_state_extent = after_score_extent,
        .kv_state = after_kv_state,
        .score_state = after_score_state,
        .overlap = overlap,
        .rotated = rotated,
    };
    memcpy(after->attention_plan_identity, before->attention_plan_identity,
           sizeof(after->attention_plan_identity));
    attention_rolling_copy_state(before, after);
    slot = overlap ? ratio + before->cursor : before->cursor;
    for (lane = 0ull; lane < state_width; ++lane) {
        after->kv_state[slot * after->kv_state_stride + lane] = token_kv[lane];
        after->score_state[slot * after->score_state_stride + lane] =
            token_score[lane] + ape_row[lane];
    }
    if (after->current_fill < before->cursor + 1ull)
        after->current_fill = before->cursor + 1ull;
    should_emit = ((before->next_token_position + 1ull) % ratio) == 0ull;
    if (should_emit) {
        if (!attention_rolling_emit(after, head_dim, ratio, overlap,
                                    compressed_out, compressed_out_count))
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, head_dim,
                compressed_out_count, err, YVEX_ERR_FORMAT,
                "DeepSeek attention rolling compression emitted invalid values");
        if (overlap) {
            for (slot = 0ull; slot < ratio; ++slot) {
                memcpy(after->kv_state + slot * after->kv_state_stride,
                       after->kv_state + (ratio + slot) * after->kv_state_stride,
                       (size_t)(state_width * sizeof(float)));
                memcpy(after->score_state + slot * after->score_state_stride,
                       after->score_state +
                           (ratio + slot) * after->score_state_stride,
                       (size_t)(state_width * sizeof(float)));
            }
            after->previous_fill = ratio;
        } else {
            after->previous_fill = 0ull;
        }
        after->current_fill = 0ull;
        after->cursor = 0ull;
        if (emitted) *emitted = 1;
    }
    return yvex_attention_accept(failure, err);
}

// Purpose: Return the admitted history validate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_history_validate(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    int rc;

    if (!layer || !history)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention history validation requires layer and history");
    if (!history->immutable)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention history view must be immutable");
    if (history->local_tail_count &&
        (!history->local_kv || !history->local_positions ||
         history->local_kv_stride < layer->head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->local_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention local history lacks raw KV storage");
    if (history->compressed_entry_count &&
        (!history->compressed_kv || !history->compressed_positions ||
         history->compressed_kv_stride < layer->head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->compressed_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention compressed history lacks KV storage");
    if (history->indexer_entry_count &&
        (!history->indexer_kv || !history->indexer_positions ||
         history->indexer_kv_stride < layer->indexer_head_dimension))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->indexer_head_dimension, history->indexer_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention indexer history lacks KV storage");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        history->compressed_entry_count != history->indexer_entry_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            history->compressed_entry_count, history->indexer_entry_count,
            err, YVEX_ERR_FORMAT,
            "CSA compressed and indexer history cardinalities differ");
    rc = attention_history_positions_validate(layer, history, failure, err);
    if (rc != YVEX_OK) return rc;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA &&
        (history->compressed_entry_count || history->indexer_entry_count))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->compressed_entry_count + history->indexer_entry_count,
            err, YVEX_ERR_FORMAT,
            "SWA history may not carry compressed or indexer entries");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA &&
        history->indexer_entry_count)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->indexer_entry_count, err, YVEX_ERR_FORMAT,
            "HCA history may not carry CSA indexer entries");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA) {
        if (history->main_rolling_state.present ||
            history->indexer_rolling_state.present)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
                err, YVEX_ERR_FORMAT,
                "SWA history may not carry compressor rolling state");
    } else {
        rc = attention_rolling_state_validate(
            layer, &history->main_rolling_state,
            YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, failure, err);
        if (rc != YVEX_OK) return rc;
        if (history->main_rolling_state.next_token_position !=
            history->token_count)
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                history->token_count,
                history->main_rolling_state.next_token_position, err,
                YVEX_ERR_STATE,
                "main rolling state token position is stale");
        if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
            rc = attention_rolling_state_validate(
                layer, &history->indexer_rolling_state,
                YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, failure, err);
            if (rc != YVEX_OK) return rc;
            if (history->indexer_rolling_state.next_token_position !=
                history->token_count)
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count,
                    history->indexer_rolling_state.next_token_position, err,
                    YVEX_ERR_STATE,
                    "indexer rolling state token position is stale");
        } else if (history->indexer_rolling_state.present) {
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
                err, YVEX_ERR_FORMAT,
                "HCA history may not carry indexer rolling state");
        }
    }
    return yvex_attention_accept(failure, err);
}

/* Execution composes admitted generic operations without owning their math. */

#define YVEX_ATTENTION_PI 3.14159265358979323846264338327950288

// Purpose: Return the admitted attention-execution capability fact.
// Inputs: Optional writable refusal-reason slot.
// Effects: Clears the refusal reason and performs no I/O.
// Failure: This immutable capability query cannot fail.
// Boundary: Admits attention only; persistent KV and generation remain separate.

int yvex_attention_execute_supported(const char **reason)
{
    if (reason) *reason = NULL;
    return 1;
}

/* Purpose: refuse the retired controlled-layer fixture at the graph boundary.
 * Inputs: optional fixture options and writable result/error facts.
 * Effects: clears the result and publishes an explicit unsupported status.
 * Failure: always returns the stable unsupported exit status.
 * Boundary: historical fixtures are test-owned and never runtime evidence. */
int yvex_graph_execute_layer_fixture(
    const yvex_graph_layer_fixture_options *options,
    yvex_graph_layer_fixture_result *out,
    yvex_error *err)
{
    (void)options;
    if (out) {
        memset(out, 0, sizeof(*out));
        out->status = "fixture-retired";
        out->graph_integrity_guard = "refused";
        out->graph_execution_phase = "admission";
        out->backend_status = "not-opened";
        out->backend_op_status = "unsupported";
        out->cleanup_status = "not-needed";
    }
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_layer_fixture",
                   "controlled layer fixtures are test-owned");
    return yvex_graph_exit_for_status(YVEX_ERR_UNSUPPORTED);
}
