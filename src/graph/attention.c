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

typedef struct {
    float *kv, *score;
    unsigned long long kv_stride, score_stride, kv_extent, score_extent;
} attention_rolling_output_storage;

_Static_assert(sizeof(yvex_attention_rolling_state_view) ==
                   sizeof(yvex_attention_rolling_state_output),
               "rolling view/output layouts must remain representation-compatible");
_Static_assert(offsetof(yvex_attention_rolling_state_view, attention_plan_identity) ==
                   offsetof(yvex_attention_rolling_state_output, attention_plan_identity),
               "rolling view/output identity offsets must remain compatible");

/* Purpose: publish graph-attention failures that carry no tensor-specific identity. */
static int attention_refuse(yvex_attention_failure *failure, yvex_attention_failure_code code,
                            unsigned long long layer, unsigned long long expected,
                            unsigned long long actual, yvex_error *error, int status,
                            const char *reason) {
    return yvex_attention_reject(failure, code, NULL, layer, YVEX_TENSOR_ROLE_UNKNOWN, expected,
                                 actual, error, status, reason);
}

// Purpose: Return the admitted segment row fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const float *yvex_attention_segment_row(const float *history, unsigned long long history_count,
                                        unsigned long long history_stride, const float *current,
                                        unsigned long long current_count,
                                        unsigned long long current_stride,
                                        unsigned long long index) {
    if (index < history_count)
        return history + index * history_stride;
    index -= history_count;
    if (index >= current_count)
        return NULL;
    return current + index * current_stride;
}

// Purpose: Return the admitted segment position fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned long long yvex_attention_segment_position(const unsigned long long *history,
                                                   unsigned long long history_count,
                                                   const unsigned long long *current,
                                                   unsigned long long current_count,
                                                   unsigned long long index) {
    if (index < history_count)
        return history[index];
    index -= history_count;
    if (index >= current_count)
        return ULLONG_MAX;
    return current[index];
}

/* Purpose: sample one borrowed cooperative-cancellation predicate at a named safe point.
 * Inputs: optional borrowed predicate, layer identity, stable safe-point text, and diagnostics.
 * Effects: invokes only the caller-owned predicate and otherwise mutates diagnostics on refusal.
 * Failure: malformed predicates refuse as invalid arguments; requested cancellation is typed.
 * Boundary: observes cancellation but never owns, resets, or releases the caller's token. */
int yvex_attention_cancel_check(const yvex_attention_cancellation *cancellation,
                                unsigned long long layer_index, const char *safe_point,
                                yvex_attention_failure *failure, yvex_error *err) {
    if (!cancellation)
        return YVEX_OK;
    if (!cancellation->requested)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
                                layer_index, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
                                "attention cancellation requires a borrowed predicate");
    if (cancellation->requested(cancellation->context))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_CANCELLED, layer_index,
                                0ull, 1ull, err, YVEX_ERR_CANCELLED,
                                safe_point ? safe_point : "attention execution cancelled");
    return YVEX_OK;
}

/* Purpose: validate one family-selected attention class before payload access or state mutation.
 * Inputs: immutable layer geometry and the family's exact CSA/HCA compression ratios.
 * Effects: reads geometry only and clears no admitted identity or state.
 * Failure: inconsistent common, SWA, CSA, or HCA geometry returns a typed dimension refusal.
 * Boundary: validates generic class invariants while the family supplies its ratio policy. */
int yvex_attention_class_geometry_validate(const yvex_attention_layer_plan *layer,
                                           unsigned long long csa_ratio,
                                           unsigned long long hca_ratio,
                                           yvex_attention_failure *failure, yvex_error *err) {
    unsigned long long query_width, indexer_width = 0ull, output_group_width;
    unsigned long long output_width, output_low_width;
    int valid = layer && layer->sliding_window && layer->query_heads && layer->kv_heads &&
                layer->head_dimension && layer->hidden_dimension &&
                layer->rope_head_dimension <= layer->head_dimension &&
                layer->query_heads % layer->kv_heads == 0ull && layer->output_groups &&
                layer->query_heads % layer->output_groups == 0ull && layer->output_lora_rank &&
                yvex_core_u64_mul(layer->query_heads, layer->head_dimension, &query_width) &&
                yvex_core_u64_mul(layer->query_heads / layer->output_groups, layer->head_dimension,
                                  &output_group_width) &&
                yvex_core_u64_mul(layer->output_groups, output_group_width, &output_width) &&
                output_width == query_width &&
                yvex_core_u64_mul(layer->output_groups, layer->output_lora_rank, &output_low_width);

    if (!valid)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                                layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER, 1ull, 0ull,
                                err, YVEX_ERR_FORMAT,
                                "attention common head, position, or output geometry is invalid");
    switch (layer->attention_class) {
    case YVEX_ATTENTION_CLASS_SWA:
        valid = layer->compression_ratio == 0ull && !layer->compressor_required &&
                !layer->indexer_required && !layer->indexer_heads &&
                !layer->indexer_head_dimension && !layer->indexer_topk &&
                !layer->sparse_topk.required && !layer->sparse_topk.k;
        break;
    case YVEX_ATTENTION_CLASS_CSA:
        valid = csa_ratio && layer->compression_ratio == csa_ratio && layer->compressor_required &&
                layer->indexer_required && layer->indexer_heads && layer->indexer_head_dimension &&
                yvex_core_u64_mul(layer->indexer_heads, layer->indexer_head_dimension,
                                  &indexer_width) &&
                indexer_width && layer->indexer_topk && layer->sparse_topk.required &&
                layer->sparse_topk.k == layer->indexer_topk;
        break;
    case YVEX_ATTENTION_CLASS_HCA:
        valid = hca_ratio && layer->compression_ratio == hca_ratio && layer->compressor_required &&
                !layer->indexer_required && !layer->indexer_heads &&
                !layer->indexer_head_dimension && !layer->indexer_topk &&
                !layer->sparse_topk.required && !layer->sparse_topk.k;
        break;
    default:
        valid = 0;
        break;
    }
    if (!valid)
        return attention_refuse(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION, layer->layer_index,
            layer->attention_class == YVEX_ATTENTION_CLASS_HCA ? hca_ratio : csa_ratio,
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
    const yvex_attention_layer_plan *layer, const yvex_attention_history_view *history,
    const float *current_indexer, unsigned long long current_indexer_count,
    unsigned long long current_indexer_stride, const unsigned long long *current_indexer_positions,
    const float *index_query, const float *index_weights, unsigned long long query_position,
    unsigned long long *selected, unsigned long long *selected_count,
    unsigned long long *valid_count, yvex_attention_scratch_budget *scratch,
    yvex_attention_failure *failure, yvex_error *err) {
    unsigned long long total;
    float *scores = NULL;
    unsigned long long *ordinals = NULL, *valid_indexes = NULL, *ranked = NULL;
    unsigned long long candidate, valid = 0ull, ranked_count = 0ull;
    size_t base_reserved = 0u, ranked_reserved = 0u;
    int rc = YVEX_OK;

    if (selected_count)
        *selected_count = 0ull;
    if (valid_count)
        *valid_count = 0ull;
    if (!layer || !history || !index_query || !index_weights || !selected || !selected_count ||
        !valid_count)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
                                layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER, 1ull, 0ull,
                                err, YVEX_ERR_INVALID_ARG,
                                "CSA selection requires history, query, weights, and outputs");
    if (history->indexer_entry_count > ULLONG_MAX - current_indexer_count)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION,
                                layer->layer_index, ULLONG_MAX, history->indexer_entry_count, err,
                                YVEX_ERR_BOUNDS, "CSA candidate count overflowed");
    total = history->indexer_entry_count + current_indexer_count;
    if (!total)
        return YVEX_OK;
    if (!yvex_attention_scratch_reserve(
            scratch, total, sizeof(*scores) + sizeof(*ordinals) + sizeof(*valid_indexes),
            &base_reserved))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
                                layer->layer_index, scratch ? scratch->limit_bytes : 0ull,
                                scratch ? (unsigned long long)scratch->live_bytes : 0ull, err,
                                YVEX_ERR_BOUNDS,
                                "CSA selection exceeds the attention scratch budget");
    scores = (float *)yvex_attention_calloc_array(total, sizeof(*scores));
    ordinals = (unsigned long long *)yvex_attention_calloc_array(total, sizeof(*ordinals));
    valid_indexes =
        (unsigned long long *)yvex_attention_calloc_array(total, sizeof(*valid_indexes));
    if (!scores || !ordinals || !valid_indexes) {
        rc = attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                              layer->layer_index, total, 0ull, err, YVEX_ERR_NOMEM,
                              "CSA selection scratch allocation failed");
        goto cleanup;
    }
    for (candidate = 0ull; candidate < total; ++candidate) {
        const float *key = yvex_attention_segment_row(
            history->indexer_kv, history->indexer_entry_count, history->indexer_kv_stride,
            current_indexer, current_indexer_count, current_indexer_stride, candidate);
        unsigned long long position = yvex_attention_segment_position(
            history->indexer_positions, history->indexer_entry_count, current_indexer_positions,
            current_indexer_count, candidate);
        unsigned long long head;
        double score = 0.0;

        if (!key || position == ULLONG_MAX || position > query_position ||
            position > ULLONG_MAX - layer->compression_ratio + 1ull ||
            position + layer->compression_ratio - 1ull > query_position)
            continue;
        for (head = 0ull; head < layer->indexer_heads; ++head) {
            const float *q = index_query + head * layer->indexer_head_dimension;
            unsigned long long lane;
            double dot = 0.0;
            for (lane = 0ull; lane < layer->indexer_head_dimension; ++lane) {
                if (!isfinite(q[lane]) || !isfinite(key[lane]))
                    goto numeric;
                dot += (double)q[lane] * (double)key[lane];
            }
            if (dot < 0.0)
                dot = 0.0;
            if (!isfinite(index_weights[head]))
                goto numeric;
            score += dot * (double)index_weights[head];
        }
        score *= 1.0 / sqrt((double)layer->indexer_head_dimension);
        score *= 1.0 / sqrt((double)layer->indexer_heads);
        if (!isfinite(score))
            goto numeric;
        scores[valid] = (float)score;
        ordinals[valid] = position;
        valid_indexes[valid] = candidate;
        valid++;
    }
    if (valid) {
        unsigned long long ranked_capacity = yvex_attention_min_u64(valid, layer->sparse_topk.k);
        unsigned long long index;

        if (!yvex_attention_scratch_reserve(scratch, ranked_capacity, sizeof(*ranked),
                                            &ranked_reserved)) {
            rc = attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH,
                                  layer->layer_index, scratch ? scratch->limit_bytes : 0ull,
                                  scratch ? (unsigned long long)scratch->live_bytes : 0ull, err,
                                  YVEX_ERR_BOUNDS,
                                  "CSA ranked selection exceeds the attention scratch budget");
            goto cleanup;
        }
        ranked =
            (unsigned long long *)yvex_attention_calloc_array(ranked_capacity, sizeof(*ranked));
        if (!ranked) {
            rc = attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION,
                                  layer->layer_index, valid, 0ull, err, YVEX_ERR_NOMEM,
                                  "CSA ranked-selection scratch allocation failed");
            goto cleanup;
        }
        rc = yvex_attention_topk_select(scores, ordinals, valid, layer->sparse_topk.k, ranked,
                                        &ranked_count, failure, err);
        if (rc != YVEX_OK)
            goto cleanup;
        for (index = 0ull; index < ranked_count; ++index)
            selected[index] = valid_indexes[ranked[index]];
        *selected_count = ranked_count;
    }
    *valid_count = valid;
cleanup:
    free(ranked);
    free(scores);
    free(ordinals);
    free(valid_indexes);
    yvex_attention_scratch_release(scratch, ranked_reserved);
    yvex_attention_scratch_release(scratch, base_reserved);
    return rc;

numeric:
    rc = attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                          layer->layer_index, 1ull, candidate, err, YVEX_ERR_FORMAT,
                          "CSA index scoring produced non-finite values");
    goto cleanup;
}

// Purpose: Return the admitted rolling geometry fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_rolling_geometry(const yvex_attention_layer_plan *layer,
                                    yvex_attention_rolling_kind kind, unsigned long long *ratio,
                                    unsigned long long *head_dim, unsigned long long *state_width,
                                    unsigned long long *state_slots, int *overlap, int *rotated) {
    unsigned long long coeff;

    if (!layer || !ratio || !head_dim || !state_width || !state_slots || !overlap || !rotated ||
        layer->compression_ratio == 0ull)
        return 0;
    if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN) {
        *ratio = layer->compression_ratio;
        *head_dim = layer->head_dimension;
        *overlap = layer->attention_class == YVEX_ATTENTION_CLASS_CSA ? 1 : 0;
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
    const float *kv_state, const float *score_state, unsigned long long kv_stride,
    unsigned long long score_stride, unsigned long long head_dim, unsigned long long ratio,
    unsigned long long previous_fill, unsigned long long current_fill, int overlap) {
    unsigned long long slot;
    unsigned long long lane;

    if (!kv_state || !score_state)
        return 0;
    /* The previous half participates in the first overlap emission even
     * before it contains a completed group.  Its fail-closed score sentinel
     * is therefore part of the admitted state, not unused storage. */
    if (overlap && previous_fill == 0ull) {
        for (slot = 0ull; slot < ratio; ++slot) {
            for (lane = 0ull; lane < head_dim; ++lane) {
                float score = score_state[slot * score_stride + lane];
                if (!isinf(score) || !signbit(score))
                    return 0;
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

static int attention_rolling_state_validate(const yvex_attention_layer_plan *layer,
                                            const yvex_attention_rolling_state_view *state,
                                            yvex_attention_rolling_kind kind,
                                            yvex_attention_failure *failure, yvex_error *err) {
    unsigned long long ratio, head_dim, state_width, state_slots, required_extent;
    unsigned long long expected_cursor, expected_previous_fill;
    int overlap, rotated;

    if (!layer || !state || !state->present)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER, 1ull, 0ull,
                                err, YVEX_ERR_INVALID_ARG,
                                "DeepSeek attention rolling state is missing");
    if (!yvex_attention_rolling_geometry(layer, kind, &ratio, &head_dim, &state_width, &state_slots,
                                         &overlap, &rotated))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_UNSUPPORTED,
                                "DeepSeek attention rolling state is not used by this class");
    if (state->schema_version != YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1 ||
        state->kind != kind || state->attention_class != layer->attention_class ||
        state->layer_index != layer->layer_index || state->ratio != ratio ||
        state->head_dimension != head_dim || state->state_width != state_width ||
        state->state_slots != state_slots || state->overlap != overlap || state->rotated != rotated)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, state_width, state->state_width, err,
                                YVEX_ERR_FORMAT,
                                "DeepSeek attention rolling state identity or geometry mismatch");
    if (state->cursor >= ratio || state->previous_fill > ratio || state->current_fill > ratio ||
        (!overlap && state->previous_fill))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, ratio, state->cursor, err, YVEX_ERR_BOUNDS,
                                "DeepSeek attention rolling state cursor or fill is invalid");
    expected_cursor = state->next_token_position % ratio;
    expected_previous_fill = overlap && state->next_token_position >= ratio ? ratio : 0ull;
    if (state->cursor != expected_cursor || state->current_fill != expected_cursor ||
        state->previous_fill != expected_previous_fill)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, expected_cursor, state->cursor, err,
                                YVEX_ERR_STATE,
                                "DeepSeek attention rolling state does not match its token position");
    if (state->kv_state_stride < state_width || state->score_state_stride < state_width ||
        !state->kv_state || !state->score_state)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, state_width, state->kv_state_stride, err,
                                YVEX_ERR_FORMAT,
                                "DeepSeek attention rolling state storage is incomplete");
    if (!yvex_core_u64_mul(state_slots, state->kv_state_stride, &required_extent) ||
        state->kv_state_extent < required_extent ||
        !yvex_core_u64_mul(state_slots, state->score_state_stride, &required_extent) ||
        state->score_state_extent < required_extent)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, state_slots, state->kv_state_extent, err,
                                YVEX_ERR_BOUNDS,
                                "DeepSeek attention rolling state extent is too small");
    if (!attention_rolling_active_values_are_finite(
            state->kv_state, state->score_state, state->kv_state_stride, state->score_state_stride,
            head_dim, ratio, state->previous_fill, state->current_fill, overlap))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention rolling state contains non-finite active values");
    return yvex_attention_accept(failure, err);
}

/* Purpose: require the unique complete local and compressed history prefix.
 * Inputs: admitted layer semantics and one immutable next-token history view.
 * Effects: reads positions and cardinalities only; publishes no state.
 * Failure: returns typed missing, extra, unordered, or overflow refusal.
 * Boundary: validates attention-local history, never persistent runtime KV. */
static int attention_history_positions_validate(const yvex_attention_layer_plan *layer,
                                                const yvex_attention_history_view *history,
                                                yvex_attention_failure *failure, yvex_error *err) {
    unsigned long long local_capacity, expected_local, local_start, i;
    unsigned long long expected_compressed = 0ull;

    if (!layer->sliding_window)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention history requires a nonzero sliding window");
    local_capacity = layer->sliding_window - 1ull;
    expected_local = history->token_count < local_capacity ? history->token_count : local_capacity;
    local_start = history->token_count - expected_local;
    if (history->local_tail_count != expected_local)
        return attention_refuse(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, layer->layer_index, expected_local,
            history->local_tail_count, err,
            history->local_tail_count > expected_local ? YVEX_ERR_BOUNDS : YVEX_ERR_FORMAT,
            "DeepSeek attention local history is not the complete window suffix");
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA) {
        if (!layer->compression_ratio)
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                    layer->layer_index, 1ull, 0ull, err, YVEX_ERR_FORMAT,
                                    "compressed attention history requires a nonzero ratio");
        expected_compressed = history->token_count / layer->compression_ratio;
    }
    if (history->compressed_entry_count != expected_compressed)
        return attention_refuse(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, layer->layer_index,
            expected_compressed, history->compressed_entry_count, err, YVEX_ERR_FORMAT,
            "DeepSeek attention compressed history is not the complete prefix");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        history->indexer_entry_count != expected_compressed)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, expected_compressed,
                                history->indexer_entry_count, err, YVEX_ERR_FORMAT,
                                "CSA indexer history is not the complete compressed prefix");
    for (i = 0ull; i < expected_local; ++i) {
        if (history->local_positions[i] != local_start + i)
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                    layer->layer_index, local_start + i,
                                    history->local_positions[i], err, YVEX_ERR_FORMAT,
                                    "local history does not contain the exact contiguous suffix");
    }
    for (i = 0ull; i < expected_compressed; ++i) {
        unsigned long long expected_position = 0ull;

        if (!yvex_core_u64_mul(i, layer->compression_ratio, &expected_position) ||
            history->compressed_positions[i] != expected_position ||
            (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
             history->indexer_positions[i] != expected_position))
            return attention_refuse(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, layer->layer_index,
                expected_position, history->compressed_positions[i], err, YVEX_ERR_FORMAT,
                "compressed history does not contain every completed ratio group");
    }
    return yvex_attention_accept(failure, err);
}

// Purpose: Apply the checked graph-local rolling copy state invariant.

static void attention_rolling_copy_state(const yvex_attention_rolling_state_view *before,
                                         yvex_attention_rolling_state_output *after) {
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

static int attention_rolling_emit(const yvex_attention_rolling_state_output *state,
                                  unsigned long long head_dim, unsigned long long ratio,
                                  int overlap, float *compressed_out,
                                  unsigned long long compressed_out_count) {
    unsigned long long lane;
    unsigned long long slot;

    if (!state || !compressed_out || compressed_out_count < head_dim)
        return 0;
    for (lane = 0ull; lane < head_dim; ++lane) {
        double max_score = -HUGE_VAL;
        double denom = 0.0;
        double value = 0.0;
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride + lane];
            if (overlap) {
                double score2 =
                    state
                        ->score_state[(ratio + slot) * state->score_state_stride + lane + head_dim];
                if (score2 > max_score)
                    max_score = score2;
            }
            if (score > max_score)
                max_score = score;
        }
        for (slot = 0ull; slot < ratio; ++slot) {
            double score = state->score_state[slot * state->score_state_stride + lane];
            double weight = exp(score - max_score);
            denom += weight;
            value += weight * (double)state->kv_state[slot * state->kv_state_stride + lane];
            if (overlap) {
                double score2 =
                    state
                        ->score_state[(ratio + slot) * state->score_state_stride + lane + head_dim];
                double weight2 = exp(score2 - max_score);
                denom += weight2;
                value +=
                    weight2 *
                    (double)
                        state->kv_state[(ratio + slot) * state->kv_state_stride + lane + head_dim];
            }
        }
        if (!isfinite(denom) || denom <= 0.0 || !isfinite(value))
            return 0;
        compressed_out[lane] = (float)(value / denom);
    }
    return 1;
}

// Purpose: Return the admitted rolling state step cpu fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_rolling_state_step_cpu(const yvex_attention_layer_plan *layer,
                                          const yvex_attention_rolling_state_view *before,
                                          const float *token_kv, const float *token_score,
                                          const float *ape_row,
                                          yvex_attention_rolling_state_output *after,
                                          float *compressed_out,
                                          unsigned long long compressed_out_count, int *emitted,
                                          yvex_attention_failure *failure, yvex_error *err) {
    unsigned long long ratio, head_dim, state_width, state_slots, required_extent;
    unsigned long long slot, lane;
    attention_rolling_output_storage storage;
    int overlap, rotated, should_emit, rc;

    if (emitted)
        *emitted = 0;
    rc = attention_rolling_state_validate(
        layer, before, before ? before->kind : YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE, failure, err);
    if (rc != YVEX_OK)
        return rc;
    if (!token_kv || !token_score || !ape_row || !after)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_INVALID_ARG,
                                "DeepSeek attention rolling transition requires token vectors and output state");
    if (before->next_token_position == ULLONG_MAX)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, ULLONG_MAX, before->next_token_position, err,
                                YVEX_ERR_BOUNDS,
                                "DeepSeek attention rolling token position would overflow");
    if (!yvex_attention_rolling_geometry(layer, before->kind, &ratio, &head_dim, &state_width,
                                         &state_slots, &overlap, &rotated))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_UNSUPPORTED,
                                "DeepSeek attention rolling transition lacks geometry");
    if (!after->kv_state || !after->score_state || after->kv_state_stride < state_width ||
        after->score_state_stride < state_width ||
        !yvex_core_u64_mul(state_slots, after->kv_state_stride, &required_extent) ||
        after->kv_state_extent < required_extent ||
        !yvex_core_u64_mul(state_slots, after->score_state_stride, &required_extent) ||
        after->score_state_extent < required_extent)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, state_width,
                                after ? after->kv_state_stride : 0ull, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention rolling output storage is incomplete");
    if (before->cursor != (before->next_token_position % ratio))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, before->next_token_position % ratio,
                                before->cursor, err, YVEX_ERR_STATE,
                                "DeepSeek attention rolling cursor is stale for token position");
    storage = (attention_rolling_output_storage){after->kv_state, after->score_state,
                                                 after->kv_state_stride,
                                                 after->score_state_stride,
                                                 after->kv_state_extent,
                                                 after->score_state_extent};
    for (lane = 0ull; lane < state_width; ++lane) {
        if (!isfinite(token_kv[lane]) || !isfinite(token_score[lane]) || !isfinite(ape_row[lane]))
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                                    layer->layer_index, 1ull, lane, err, YVEX_ERR_FORMAT,
                                    "DeepSeek attention rolling transition input is non-finite");
    }
    memcpy(after, before, sizeof(*after));
    after->next_token_position = before->next_token_position + 1ull;
    after->cursor = (before->cursor + 1ull) % ratio;
    after->kv_state = storage.kv;
    after->score_state = storage.score;
    after->kv_state_stride = storage.kv_stride;
    after->score_state_stride = storage.score_stride;
    after->kv_state_extent = storage.kv_extent;
    after->score_state_extent = storage.score_extent;
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
        if (!attention_rolling_emit(after, head_dim, ratio, overlap, compressed_out,
                                    compressed_out_count))
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC,
                                    layer->layer_index, head_dim, compressed_out_count, err,
                                    YVEX_ERR_FORMAT,
                                    "DeepSeek attention rolling compression emitted invalid values");
        if (overlap) {
            for (slot = 0ull; slot < ratio; ++slot) {
                memcpy(after->kv_state + slot * after->kv_state_stride,
                       after->kv_state + (ratio + slot) * after->kv_state_stride,
                       (size_t)(state_width * sizeof(float)));
                memcpy(after->score_state + slot * after->score_state_stride,
                       after->score_state + (ratio + slot) * after->score_state_stride,
                       (size_t)(state_width * sizeof(float)));
            }
            after->previous_fill = ratio;
        } else {
            after->previous_fill = 0ull;
        }
        after->current_fill = 0ull;
        after->cursor = 0ull;
        if (emitted)
            *emitted = 1;
    }
    return yvex_attention_accept(failure, err);
}

// Purpose: Return the admitted history validate fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_history_validate(const yvex_attention_layer_plan *layer,
                                    const yvex_attention_history_view *history,
                                    yvex_attention_failure *failure, yvex_error *err) {
    int rc;

    if (!layer || !history)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT,
                                layer ? layer->layer_index : YVEX_ATTENTION_NO_LAYER, 1ull, 0ull,
                                err, YVEX_ERR_INVALID_ARG,
                                "DeepSeek attention history validation requires layer and history");
    if (!history->immutable)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, 1ull, 0ull, err, YVEX_ERR_STATE,
                                "DeepSeek attention history view must be immutable");
    if (history->local_tail_count && (!history->local_kv || !history->local_positions ||
                                      history->local_kv_stride < layer->head_dimension))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, layer->head_dimension,
                                history->local_kv_stride, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention local history lacks raw KV storage");
    if (history->compressed_entry_count &&
        (!history->compressed_kv || !history->compressed_positions ||
         history->compressed_kv_stride < layer->head_dimension))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, layer->head_dimension,
                                history->compressed_kv_stride, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention compressed history lacks KV storage");
    if (history->indexer_entry_count &&
        (!history->indexer_kv || !history->indexer_positions ||
         history->indexer_kv_stride < layer->indexer_head_dimension))
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, layer->indexer_head_dimension,
                                history->indexer_kv_stride, err, YVEX_ERR_FORMAT,
                                "DeepSeek attention indexer history lacks KV storage");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        history->compressed_entry_count != history->indexer_entry_count)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, history->compressed_entry_count,
                                history->indexer_entry_count, err, YVEX_ERR_FORMAT,
                                "CSA compressed and indexer history cardinalities differ");
    rc = attention_history_positions_validate(layer, history, failure, err);
    if (rc != YVEX_OK)
        return rc;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA &&
        (history->compressed_entry_count || history->indexer_entry_count))
        return attention_refuse(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, layer->layer_index, 0ull,
            history->compressed_entry_count + history->indexer_entry_count, err, YVEX_ERR_FORMAT,
            "SWA history may not carry compressed or indexer entries");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA && history->indexer_entry_count)
        return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                layer->layer_index, 0ull, history->indexer_entry_count, err,
                                YVEX_ERR_FORMAT, "HCA history may not carry CSA indexer entries");
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA) {
        if (history->main_rolling_state.present || history->indexer_rolling_state.present)
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                    layer->layer_index, 0ull, 1ull, err, YVEX_ERR_FORMAT,
                                    "SWA history may not carry compressor rolling state");
    } else {
        rc = attention_rolling_state_validate(layer, &history->main_rolling_state,
                                              YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, failure, err);
        if (rc != YVEX_OK)
            return rc;
        if (history->main_rolling_state.next_token_position != history->token_count)
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                    layer->layer_index, history->token_count,
                                    history->main_rolling_state.next_token_position, err,
                                    YVEX_ERR_STATE, "main rolling state token position is stale");
        if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
            rc = attention_rolling_state_validate(layer, &history->indexer_rolling_state,
                                                  YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, failure,
                                                  err);
            if (rc != YVEX_OK)
                return rc;
            if (history->indexer_rolling_state.next_token_position != history->token_count)
                return attention_refuse(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, layer->layer_index,
                    history->token_count, history->indexer_rolling_state.next_token_position, err,
                    YVEX_ERR_STATE, "indexer rolling state token position is stale");
        } else if (history->indexer_rolling_state.present) {
            return attention_refuse(failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY,
                                    layer->layer_index, 0ull, 1ull, err, YVEX_ERR_FORMAT,
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

int yvex_attention_execute_supported(const char **reason) {
    if (reason)
        *reason = NULL;
    return 1;
}

/* Purpose: refuse the retired controlled-layer fixture at the graph boundary.
 * Inputs: optional fixture options and writable result/error facts.
 * Effects: clears the result and publishes an explicit unsupported status.
 * Failure: always returns the stable unsupported exit status.
 * Boundary: historical fixtures are test-owned and never runtime evidence. */
int yvex_graph_execute_layer_fixture(const yvex_graph_layer_fixture_options *options,
                                     yvex_graph_layer_fixture_result *out, yvex_error *err) {
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

enum {
    ATTENTION_PROBE_CPU = 0,
    ATTENTION_PROBE_CUDA,
    ATTENTION_PROBE_BACKEND_COUNT,
    ATTENTION_PROBE_CLASS_COUNT = 3,
    ATTENTION_PROBE_OWNED_BUFFERS = 6
};

enum {
    ATTENTION_COMPARISON_SCHEMA_VERSION = 2,
    ATTENTION_COMPARISON_TOLERANCE_NUMERATOR = 5,
    ATTENTION_COMPARISON_TOLERANCE_DENOMINATOR = 10000,
    ATTENTION_COMPARISON_SCALE_MAX_ABS = 1,
    ATTENTION_COMPARISON_NONFINITE_REFUSE = 1,
    ATTENTION_COMPARISON_RMSE_FINITE_ONLY = 1,
    ATTENTION_COMPARISON_BITWISE_MEMCMP = 1,
    ATTENTION_COMPARISON_BITWISE_NOT_REQUIRED = 0,
    ATTENTION_COMPARISON_TRAVERSAL_LAYER_COORDINATE = 1
};

static const double attention_comparison_absolute_tolerance = 5.0e-4;
static const double attention_comparison_relative_tolerance = 5.0e-4;

static const char *const probe_output_domains[] = {"yvex.attention.operator.cpu.output.v1",
                                                   "yvex.attention.operator.cuda.output.v1"};

typedef struct {
    yvex_attention_history_view view;
    void *owned[ATTENTION_PROBE_OWNED_BUFFERS];
} attention_probe_history;

typedef struct {
    yvex_sha256 hash[ATTENTION_PROBE_BACKEND_COUNT];
    double squared_error;
} attention_probe_metrics;

typedef struct {
    yvex_attention_cpu_result evidence;
    yvex_attention_publication publication;
} attention_probe_backend;

typedef struct {
    const char *text;
    unsigned long long number;
} attention_probe_identity_field;

static const attention_probe_identity_field attention_comparison_identity_fields[] = {
    {NULL, ATTENTION_COMPARISON_SCHEMA_VERSION},
    {NULL, ATTENTION_COMPARISON_TOLERANCE_NUMERATOR},
    {NULL, ATTENTION_COMPARISON_TOLERANCE_DENOMINATOR},
    {NULL, ATTENTION_COMPARISON_SCALE_MAX_ABS},
    {NULL, ATTENTION_COMPARISON_NONFINITE_REFUSE},
    {NULL, ATTENTION_COMPARISON_RMSE_FINITE_ONLY},
    {NULL, ATTENTION_COMPARISON_BITWISE_MEMCMP},
    {NULL, ATTENTION_COMPARISON_BITWISE_NOT_REQUIRED},
    {NULL, ATTENTION_COMPARISON_TRAVERSAL_LAYER_COORDINATE},
};

typedef struct {
    unsigned long long count;
    unsigned long long width;
    unsigned long long seed;
    unsigned long long extent;
    float *values;
} attention_probe_segment;

typedef struct {
    const yvex_graph_family_api *family;
    const yvex_attention_plan *plan;
    const void *family_ir;
    yvex_materialization_session *session;
    const yvex_runtime_descriptor *descriptor;
    const yvex_attention_probe_request *request;
    const yvex_attention_summary *summary;
    yvex_attention_probe_result candidate;
    yvex_attention_failure *failure;
    yvex_error *error;
    yvex_backend *cuda_backend;
    attention_probe_metrics metrics;
} attention_probe_context;

/* Purpose: publish one canonical probe failure without repeating its owner tag. */
static int attention_probe_fail(yvex_error *error, int code, const char *message) {
    yvex_error_set(error, code, "attention.probe", message);
    return code;
}

/* Purpose: release probe history.
 * Inputs: one owned history envelope.
 * Effects: frees and clears all buffers.
 * Failure: cannot fail.
 * Boundary: never releases borrowed graph state. */
static void attention_probe_history_release(attention_probe_history *history) {
    unsigned int index;

    if (!history)
        return;
    for (index = 0u; index < ATTENTION_PROBE_OWNED_BUFFERS; ++index)
        free(history->owned[index]);
    memset(history, 0, sizeof(*history));
}

/* Purpose: produce deterministic probe values and optional derived scores. */
static void attention_probe_fill(float *values, float *scores, unsigned long long count,
                                 unsigned long long seed) {
    unsigned long long index;

    for (index = 0ull; index < count; ++index) {
        unsigned long long code = (index * 37ull + seed * 19ull + 11ull) % 257ull;
        values[index] = (float)((long long)code - 128ll) / 256.0f;
        if (scores)
            scores[index] = values[index] * 0.5f;
    }
}

/* Purpose: construct one release-safe rolling state at its exact probe position.
 * Inputs: admitted layer geometry, rolling kind, position, and plan identity.
 * Effects: records both allocations in the history envelope.
 * Failure: allocation or geometry refusal remains release-safe.
 * Boundary: bounded attention history, never persistent KV. */
static int attention_probe_rolling_init(attention_probe_history *history,
                                        yvex_attention_rolling_state_view *state,
                                        const yvex_attention_layer_plan *layer,
                                        yvex_attention_rolling_kind kind,
                                        unsigned long long position, const char *plan_identity) {
    yvex_attention_failure failure = {0};
    yvex_error error;
    float *values = NULL, *scores = NULL;
    unsigned long long offset, extent;
    unsigned int owner = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ? 4u : 2u;
    int rc;

    yvex_error_clear(&error);
    rc = yvex_attention_rolling_storage_allocate(layer, kind, position, &values, &scores, state,
                                                 &failure, &error);
    history->owned[owner] = values;
    history->owned[owner + 1u] = scores;
    if (rc != YVEX_OK)
        return 0;
    state->current_fill = state->cursor;
    state->previous_fill = state->overlap && position >= state->ratio ? state->ratio : 0ull;
    extent = state->previous_fill * state->state_width;
    if (extent)
        attention_probe_fill(values, scores, extent,
                             layer->layer_index + (unsigned long long)kind + 17ull);
    offset = state->overlap ? state->ratio * state->state_width : 0ull;
    extent = state->current_fill * state->state_width;
    if (extent)
        attention_probe_fill(values + offset, scores + offset, extent,
                             layer->layer_index + (unsigned long long)kind + 31ull);
    (void)snprintf(state->attention_plan_identity, sizeof(state->attention_plan_identity), "%s",
                   plan_identity);
    return 1;
}

/* Purpose: build real-geometry local, compressed, indexer, and rolling probe history.
 * Inputs: admitted layer, summary, and causal position.
 * Effects: owns all backing arrays in one release envelope.
 * Failure: checked geometry or allocation refusal publishes no view.
 * Boundary: deterministic attention probe state, not prompt, prefill, or KV. */
static int attention_probe_history_init(attention_probe_history *history,
                                        const yvex_attention_layer_plan *layer,
                                        const yvex_attention_summary *summary,
                                        unsigned long long position) {
    attention_probe_segment segments[3];
    unsigned long long value_count = 0ull, position_count, offset = 0ull, index;
    unsigned long long *positions, *local_positions, *compressed_positions;
    float *values;
    unsigned int segment;

    if (!history || !layer || !summary || !layer->sliding_window ||
        (layer->attention_class != YVEX_ATTENTION_CLASS_SWA && !layer->compression_ratio))
        return 0;
    memset(history, 0, sizeof(*history));
    segments[0] = (attention_probe_segment){
        position < layer->sliding_window - 1ull ? position : layer->sliding_window - 1ull,
        layer->head_dimension, layer->layer_index + 101ull, 0ull, NULL};
    segments[1] = (attention_probe_segment){
        layer->attention_class == YVEX_ATTENTION_CLASS_SWA
            ? 0ull
            : position / layer->compression_ratio,
        layer->head_dimension, layer->layer_index + 211ull, 0ull, NULL};
    segments[2] = (attention_probe_segment){
        layer->attention_class == YVEX_ATTENTION_CLASS_CSA ? segments[1].count : 0ull,
        layer->indexer_head_dimension, layer->layer_index + 307ull, 0ull, NULL};
    for (segment = 0u; segment < 3u; ++segment) {
        if (!yvex_core_u64_mul(segments[segment].count, segments[segment].width,
                               &segments[segment].extent) ||
            !yvex_core_u64_add(value_count, segments[segment].extent, &value_count))
            return 0;
    }
    if (!yvex_core_u64_add(segments[0].count, segments[1].count, &position_count))
        return 0;
    values = value_count ? yvex_attention_calloc_array(value_count, sizeof(*values)) : NULL;
    positions =
        position_count ? yvex_attention_calloc_array(position_count, sizeof(*positions)) : NULL;
    history->owned[0] = values;
    history->owned[1] = positions;
    if ((value_count && !values) || (position_count && !positions))
        goto fail;
    for (segment = 0u; segment < 3u; ++segment) {
        segments[segment].values = segments[segment].count ? values + offset : NULL;
        if (segments[segment].count)
            attention_probe_fill(segments[segment].values, NULL, segments[segment].extent,
                                 segments[segment].seed);
        offset += segments[segment].extent;
    }
    local_positions = positions;
    compressed_positions = segments[0].count ? positions + segments[0].count : positions;
    for (index = 0ull; index < segments[0].count; ++index)
        local_positions[index] = position - segments[0].count + index;
    for (index = 0ull; index < segments[1].count; ++index)
            compressed_positions[index] = index * layer->compression_ratio;
    history->view = (yvex_attention_history_view){
        .immutable = 1,
        .token_count = position,
        .local_tail_count = segments[0].count,
        .local_kv = segments[0].values,
        .local_positions = segments[0].count ? local_positions : NULL,
        .local_kv_stride = segments[0].count ? segments[0].width : 0ull,
        .compressed_entry_count = segments[1].count,
        .compressed_kv = segments[1].values,
        .compressed_positions = segments[1].count ? compressed_positions : NULL,
        .compressed_kv_stride = segments[1].count ? segments[1].width : 0ull,
        .indexer_entry_count = segments[2].count,
        .indexer_kv = segments[2].values,
        .indexer_positions = segments[2].count ? compressed_positions : NULL,
        .indexer_kv_stride = segments[2].count ? segments[2].width : 0ull,
    };
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA &&
        !attention_probe_rolling_init(history, &history->view.main_rolling_state, layer,
                                      YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, position,
                                      summary->attention_plan_identity))
        goto fail;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        !attention_probe_rolling_init(history, &history->view.indexer_rolling_state, layer,
                                      YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, position,
                                      summary->attention_plan_identity))
        goto fail;
    return 1;
fail:
    attention_probe_history_release(history);
    return 0;
}

/* Purpose: encode one ordered evidence identity without hashing object memory.
 * Inputs: domain, canonical text/number fields, and fixed-width output.
 * Effects: writes only the resulting SHA-256 text.
 * Failure: returns false on canonical hash update or finalization failure.
 * Boundary: execution evidence is distinct from artifact and model identity. */
static int attention_probe_identity(const char *domain,
                                    const attention_probe_identity_field *fields,
                                    size_t field_count, char output[YVEX_SHA256_HEX_CAP]) {
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    size_t index;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, domain))
        return 0;
    for (index = 0u; index < field_count; ++index) {
        int ok = fields[index].text ? yvex_sha256_update_text(&hash, fields[index].text)
                                    : yvex_sha256_update_u64(&hash, fields[index].number);
        if (!ok)
            return 0;
    }
    if (!yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

/* Purpose: bind every CPU/CUDA comparison rule to one canonical identity.
 * Inputs: fixed-width destination.
 * Effects: writes only the versioned comparison identity.
 * Failure: returns false if canonical encoding fails.
 * Boundary: the identity describes numeric admission, not backend output bytes. */
static int attention_probe_comparison_identity(char output[YVEX_SHA256_HEX_CAP]) {
    return attention_probe_identity(
        "yvex.attention.cpu-cuda.comparison.v2", attention_comparison_identity_fields,
        sizeof(attention_comparison_identity_fields) /
            sizeof(attention_comparison_identity_fields[0]),
        output);
}

/* Purpose: hash one publication.
 * Inputs: hash state and complete output.
 * Effects: advances the digest.
 * Failure: rejects incomplete geometry or hashing.
 * Boundary: produces execution evidence only. */
static int attention_probe_hash_output(yvex_sha256 *hash,
                                       const yvex_attention_publication *output) {
    unsigned long long count;
    size_t bytes;

    return output && output->complete && output->output &&
           yvex_core_u64_mul(output->token_count, output->hidden_width, &count) &&
           yvex_attention_checked_size(count, sizeof(float), &bytes) &&
           yvex_sha256_update_u64(hash, output->layer_index) &&
           yvex_sha256_update_u64(hash, output->attention_class) &&
           yvex_sha256_update_u64(hash, output->token_position) &&
           yvex_sha256_update_u64(hash, output->token_count) &&
           yvex_sha256_update(hash, output->output, bytes);
}

/* Purpose: compare one CPU/CUDA output pair.
 * Inputs: complete publications and probe context.
 * Effects: accumulates numeric metrics.
 * Failure: rejects geometry drift.
 * Boundary: does not use the test oracle. */
static int attention_probe_compare(attention_probe_context *context,
                                   const yvex_attention_publication *cpu,
                                   const yvex_attention_publication *cuda) {
    yvex_attention_probe_result *result = &context->candidate;
    yvex_graph_f32_comparison comparison;
    unsigned long long count;
    int rc;

    if (!cpu || !cuda || !cpu->complete || !cuda->complete ||
        cpu->layer_index != cuda->layer_index || cpu->token_count != cuda->token_count ||
        cpu->hidden_width != cuda->hidden_width ||
        !yvex_core_u64_mul(cpu->token_count, cpu->hidden_width, &count))
        return YVEX_ERR_FORMAT;
    rc = yvex_graph_f32_compare(cpu->output, cuda->output, count,
                                attention_comparison_absolute_tolerance,
                                attention_comparison_relative_tolerance,
                                &comparison, context->error);
    if (rc != YVEX_OK)
        return rc;
    result->bitwise_equality_observed &= comparison.bitwise_equal;
    if (comparison.maximum_absolute_error > result->comparison_maximum_absolute_error)
        result->comparison_maximum_absolute_error = comparison.maximum_absolute_error;
    if (comparison.maximum_relative_error > result->comparison_maximum_relative_error)
        result->comparison_maximum_relative_error = comparison.maximum_relative_error;
    context->metrics.squared_error += comparison.squared_error_sum;
    result->comparison_values += count;
    result->comparison_finite_values += comparison.finite_value_count;
    result->comparison_nonfinite_values += comparison.nonfinite_value_count;
    if (!comparison.within_tolerance &&
        result->first_failing_layer == YVEX_ATTENTION_NO_LAYER) {
        result->first_failing_layer = cpu->layer_index;
        result->first_failing_coordinate = comparison.first_failing_coordinate;
    }
    return YVEX_OK;
}

/* Purpose: execute one real-geometry layer through each requested production backend.
 * Inputs: admitted operator context, layer, and deterministic position.
 * Effects: advances aggregate evidence only after complete publications.
 * Failure: releases every publication and history allocation without partial state.
 * Boundary: production attention only; no oracle or persistent KV. */
static int attention_probe_layer_execute(attention_probe_context *context,
                                         const yvex_attention_layer_plan *layer,
                                         unsigned long long position) {
    yvex_attention_cpu_options options;
    yvex_attention_cancellation cancellation = {context->request->cancel_requested,
                                                context->request->cancel_context};
    attention_probe_backend backend[ATTENTION_PROBE_BACKEND_COUNT] = {0};
    attention_probe_history history = {0};
    float *input = yvex_attention_calloc_array(layer->hidden_dimension, sizeof(*input));
    unsigned int index;
    int rc = YVEX_OK;

    if (!input)
        return attention_probe_fail(context->error, YVEX_ERR_NOMEM,
                                    "canonical attention input allocation failed");
    attention_probe_fill(input, NULL, layer->hidden_dimension,
                         layer->layer_index + position + 1009ull);
    if (position && !attention_probe_history_init(&history, layer, context->summary, position)) {
        rc = attention_probe_fail(context->error, YVEX_ERR_NOMEM,
                                  "canonical attention history allocation failed");
        goto cleanup;
    }
    context->family->cpu_options_default(&options);
    options.layer_index = layer->layer_index;
    options.token_position = position;
    options.token_count = 1ull;
    options.input = input;
    options.input_stride = layer->hidden_dimension;
    options.history = position ? &history.view : NULL;
    options.cancellation = context->request->cancel_requested ? &cancellation : NULL;
    for (index = 0u; index < ATTENTION_PROBE_BACKEND_COUNT; ++index) {
        attention_probe_backend *run = &backend[index];
        int cuda = index == ATTENTION_PROBE_CUDA;

        if (!context->request->compare_backends &&
            context->request->backend != (cuda ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU))
            continue;
        options.publication = &run->publication;
        rc = cuda ? context->family->cuda_token_execute(
                        context->plan, context->family_ir, context->session, context->descriptor,
                        context->cuda_backend, &options, &run->evidence, context->failure,
                        context->error)
                  : context->family->cpu_chunk_execute(
                        context->plan, context->family_ir, context->session, context->descriptor,
                        &options, &run->evidence, context->failure, context->error);
        options.publication = NULL;
        if (rc != YVEX_OK)
            goto cleanup;
        if (!(cuda ? run->evidence.cuda_executed : run->evidence.executed) ||
            !attention_probe_hash_output(&context->metrics.hash[index], &run->publication)) {
            rc = attention_probe_fail(context->error, YVEX_ERR_STATE,
                                      cuda ? "CUDA attention publication was incomplete"
                                           : "CPU attention publication was incomplete");
            goto cleanup;
        }
        context->candidate.payload_bytes_read += run->evidence.payload_bytes_read;
        context->candidate.kernel_launches += run->evidence.cuda_kernel_launches;
        if (run->evidence.cuda_peak_device_bytes > context->candidate.peak_device_bytes)
            context->candidate.peak_device_bytes = run->evidence.cuda_peak_device_bytes;
        if (run->evidence.topk_selected > context->candidate.topk_selected)
            context->candidate.topk_selected = run->evidence.topk_selected;
    }
    if (context->request->compare_backends) {
        rc = attention_probe_compare(context, &backend[ATTENTION_PROBE_CPU].publication,
                                     &backend[ATTENTION_PROBE_CUDA].publication);
        if (rc != YVEX_OK) {
            if (!yvex_error_is_set(context->error))
                (void)attention_probe_fail(context->error, YVEX_ERR_FORMAT,
                                           "CPU/CUDA attention output geometry disagrees");
            goto cleanup;
        }
    }
    context->candidate.bindings_executed += layer->required_binding_count;
    context->candidate.layers_executed++;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA)
        context->candidate.hca_ratio = layer->compression_ratio;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA)
        ++context->candidate.swa_layers_executed;
    else if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
        ++context->candidate.csa_layers_executed;
    else
        ++context->candidate.hca_layers_executed;
cleanup:
    for (index = 0u; index < ATTENTION_PROBE_BACKEND_COUNT; ++index)
        context->family->publication_release(&backend[index].publication);
    attention_probe_history_release(&history);
    free(input);
    return rc;
}

/* Purpose: select each class's deep state probe once and shallow positions thereafter. */
static unsigned long long attention_probe_position(const yvex_attention_layer_plan *layer,
                                                   int selected[ATTENTION_PROBE_CLASS_COUNT]) {
    unsigned int kind = (unsigned int)layer->attention_class;

    if (kind >= ATTENTION_PROBE_CLASS_COUNT || selected[kind])
        return layer->attention_class == YVEX_ATTENTION_CLASS_SWA ? 0ull : 1ull;
    selected[kind] = 1;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_SWA)
        return layer->sliding_window;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA)
        return layer->compression_ratio * (layer->indexer_topk + 1ull);
    return layer->compression_ratio - 1ull;
}

/* Purpose: finalize output, execution, and comparison-contract identities.
 * Inputs: complete aggregate metrics in canonical layer order.
 * Effects: consumes backend hash states and fills copied evidence.
 * Failure: comparison or identity refusal leaves the result incomplete.
 * Boundary: execution evidence is not artifact or generation identity. */
static int attention_probe_finalize(attention_probe_context *context) {
    yvex_attention_probe_result *result = &context->candidate;
    const yvex_attention_probe_request *request = context->request;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char *backend_digest[] = {result->cpu_output_digest, result->cuda_output_digest};
    attention_probe_identity_field fields[6];
    const char *selected_digest;
    unsigned int index;

    if (!attention_probe_comparison_identity(result->comparison_contract_identity))
        return attention_probe_fail(context->error, YVEX_ERR_STATE,
                                    "comparison contract identity encoding failed");

    result->comparison_rmse =
        result->comparison_finite_values
            ? sqrt(context->metrics.squared_error / (double)result->comparison_finite_values)
            : 0.0;
    result->comparison_passed =
        request->compare_backends && result->comparison_values != 0ull &&
        result->comparison_finite_values == result->comparison_values &&
        result->comparison_nonfinite_values == 0ull &&
        isfinite(result->comparison_maximum_absolute_error) &&
        isfinite(result->comparison_maximum_relative_error) && isfinite(result->comparison_rmse) &&
        result->first_failing_layer == YVEX_ATTENTION_NO_LAYER;
    if (request->compare_backends && !result->comparison_passed)
        return attention_probe_fail(context->error, YVEX_ERR_FORMAT,
                                    "CPU/CUDA comparison contract failed");
    for (index = 0u; index < ATTENTION_PROBE_BACKEND_COUNT; ++index) {
        int selected = request->compare_backends ||
                       request->backend == (index ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU);
        if (selected && !yvex_sha256_final(&context->metrics.hash[index], digest))
            goto identity_failure;
        if (selected)
            yvex_sha256_hex(digest, backend_digest[index]);
    }
    if (request->compare_backends) {
        fields[0] = (attention_probe_identity_field){result->cpu_output_digest, 0ull};
        fields[1] = (attention_probe_identity_field){result->cuda_output_digest, 0ull};
        if (!attention_probe_identity("yvex.attention.operator.compare.output.v1", fields, 2u,
                                      result->output_digest))
            goto identity_failure;
    } else {
        selected_digest = request->backend == YVEX_BACKEND_KIND_CPU ? result->cpu_output_digest
                                                                    : result->cuda_output_digest;
        (void)snprintf(result->output_digest, sizeof(result->output_digest), "%s", selected_digest);
    }
    selected_digest = result->output_digest;
    fields[0] = (attention_probe_identity_field){context->summary->attention_plan_identity, 0ull};
    fields[1] = (attention_probe_identity_field){NULL, request->scope};
    fields[2] = (attention_probe_identity_field){
        NULL, request->compare_backends ? 2ull : (unsigned long long)request->backend};
    fields[3] = (attention_probe_identity_field){NULL, result->layers_executed};
    fields[4] = (attention_probe_identity_field){NULL, result->bindings_executed};
    fields[5] = (attention_probe_identity_field){selected_digest, 0ull};
    if (!attention_probe_identity("yvex.attention.operator.execution.v1", fields, 6u,
                                  result->attention_execution_identity))
        goto identity_failure;
    return YVEX_OK;
identity_failure:
    return attention_probe_fail(context->error, YVEX_ERR_STATE,
                                "attention execution identity encoding failed");
}

/* Purpose: admit CUDA attention.
 * Inputs: operator execution context.
 * Effects: opens one owned backend.
 * Failure: returns a typed device or capability refusal.
 * Boundary: performs no CPU fallback or attention dispatch. */
static int attention_probe_cuda_open(attention_probe_context *context) {
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_capability_result capability = {0};
    yvex_backend_device_info device = {0};
    int rc = yvex_backend_open(&context->cuda_backend, &options, context->error);

    if (rc == YVEX_OK)
        rc = yvex_backend_query_capability(context->cuda_backend,
                                           YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, &capability,
                                           context->error);
    if (rc == YVEX_OK && capability.state != YVEX_BACKEND_CAPABILITY_SUPPORTED) {
        rc = attention_probe_fail(context->error, YVEX_ERR_UNSUPPORTED,
                                  "device-complete encoded attention is unavailable");
    }
    if (rc == YVEX_OK)
        rc = yvex_backend_get_device_info(context->cuda_backend, &device, context->error);
    if (rc == YVEX_OK) {
        (void)snprintf(context->candidate.cuda_device, sizeof(context->candidate.cuda_device), "%s",
                       device.name ? device.name : "unknown");
        context->candidate.cuda_compute_capability_major = device.compute_capability_major;
        context->candidate.cuda_compute_capability_minor = device.compute_capability_minor;
    }
    return rc;
}

/* Purpose: preserve comparison diagnostics.
 * Inputs: failed candidate and caller result.
 * Effects: copies measured fields only.
 * Failure: cannot fail.
 * Boundary: never publishes success. */
static void attention_probe_comparison_publish(yvex_attention_probe_result *result,
                                               const yvex_attention_probe_result *candidate) {
    size_t counts = offsetof(yvex_attention_probe_result, cuda_compute_capability_major) -
                    offsetof(yvex_attention_probe_result, comparison_values);
    size_t metrics = sizeof(*result) -
                     offsetof(yvex_attention_probe_result, comparison_maximum_absolute_error);

    memcpy(&result->comparison_values, &candidate->comparison_values, counts);
    memcpy(&result->comparison_maximum_absolute_error,
           &candidate->comparison_maximum_absolute_error, metrics);
    result->comparison_passed = 0;
}

/* Purpose: execute the canonical operator probe through admitted production owners.
 * Inputs: sealed plan, materialization, descriptor, scope, and backend.
 * Effects: publishes complete evidence or comparison-only refusal diagnostics.
 * Failure: reverse-order cleanup preserves graph state and caller-owned inputs.
 * Boundary: no oracle, prompt, persistent KV, transformer, or generation work. */
int yvex_attention_probe_execute(const yvex_graph_family_api *family,
                                 const yvex_attention_plan *plan, const void *family_ir,
                                 yvex_materialization_session *session,
                                 const yvex_runtime_descriptor *descriptor,
                                 const yvex_attention_probe_request *request,
                                 yvex_attention_probe_result *result,
                                 yvex_attention_failure *failure, yvex_error *err) {
    attention_probe_context context = {.family = family,
                                       .plan = plan,
                                       .family_ir = family_ir,
                                       .session = session,
                                       .descriptor = descriptor,
                                       .request = request,
                                       .failure = failure,
                                       .error = err};
    unsigned long long index;
    int selected[ATTENTION_PROBE_CLASS_COUNT] = {0};
    int rc = YVEX_OK;

    if (!family || !plan || !family_ir || !session || !descriptor || !request || !result ||
        !family->plan_summary || !family->plan_layer_count || !family->plan_layer_at ||
        !family->cpu_options_default || !family->cpu_chunk_execute || !family->cuda_token_execute ||
        !family->publication_release) {
        return attention_probe_fail(err, YVEX_ERR_INVALID_ARG,
                                    "sealed attention owners and execution API are required");
    }
    context.summary = family->plan_summary(plan);
    if (!context.summary || !context.summary->full_execution_ready ||
        !context.summary->cpu_reference_ready ||
        ((request->compare_backends || request->backend == YVEX_BACKEND_KIND_CUDA) &&
         !context.summary->cuda_execution_ready)) {
        return attention_probe_fail(err, YVEX_ERR_UNSUPPORTED,
                                    "requested production attention capability is not admitted");
    }
    memset(&context.candidate, 0, sizeof(context.candidate));
    context.candidate.comparison_available = request->compare_backends;
    context.candidate.first_failing_layer = YVEX_ATTENTION_NO_LAYER;
    context.candidate.first_failing_coordinate = YVEX_ATTENTION_NO_LAYER;
    context.candidate.bitwise_equality_observed = request->compare_backends;
    for (index = 0ull; index < ATTENTION_PROBE_BACKEND_COUNT; ++index) {
        yvex_sha256_init(&context.metrics.hash[index]);
        if (!yvex_sha256_update_text(&context.metrics.hash[index], probe_output_domains[index]))
            return attention_probe_fail(err, YVEX_ERR_STATE,
                                        "attention output identity initialization failed");
    }
    if (request->compare_backends || request->backend == YVEX_BACKEND_KIND_CUDA) {
        rc = attention_probe_cuda_open(&context);
        if (rc != YVEX_OK)
            goto cleanup;
    }
    for (index = 0ull; index < family->plan_layer_count(plan); ++index) {
        const yvex_attention_layer_plan *layer = family->plan_layer_at(plan, index);
        int include;

        if (!layer) {
            rc = attention_probe_fail(err, YVEX_ERR_STATE,
                                      "attention layer disappeared during traversal");
            goto cleanup;
        }
        include = request->scope == YVEX_ATTENTION_PROBE_SCOPE_FULL ||
                  ((unsigned int)layer->attention_class < ATTENTION_PROBE_CLASS_COUNT &&
                   !selected[(unsigned int)layer->attention_class]);
        if (!include)
            continue;
        rc = attention_probe_layer_execute(&context, layer,
                                           attention_probe_position(layer, selected));
        if (rc != YVEX_OK)
            goto cleanup;
    }
    if ((request->scope == YVEX_ATTENTION_PROBE_SCOPE_QUICK &&
         (context.candidate.layers_executed != 3ull || !selected[0] || !selected[1] ||
          !selected[2])) ||
        (request->scope == YVEX_ATTENTION_PROBE_SCOPE_FULL &&
         (context.candidate.layers_executed != context.summary->layer_count ||
          context.candidate.bindings_executed != context.summary->required_binding_count))) {
        rc = attention_probe_fail(err, YVEX_ERR_STATE,
                                  "requested attention scope did not execute completely");
        goto cleanup;
    }
    rc = attention_probe_finalize(&context);
    if (rc == YVEX_ERR_FORMAT && request->compare_backends)
        attention_probe_comparison_publish(result, &context.candidate);
    if (rc == YVEX_OK) {
        *result = context.candidate;
        yvex_error_clear(err);
    }
cleanup:
    yvex_backend_close(context.cuda_backend);
    return rc;
}
