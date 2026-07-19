/* Owner: graph.attention protocol.
 * Owns: history validation, rolling recurrence, segment selection, sparse top-k, and status names.
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
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long total;
    float *scores = NULL;
    unsigned long long *ordinals = NULL;
    unsigned long long *valid_indexes = NULL;
    unsigned long long candidate;
    unsigned long long valid = 0ull;
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
    scores = (float *)yvex_attention_calloc_array(total, sizeof(*scores));
    ordinals = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*ordinals));
    valid_indexes = (unsigned long long *)yvex_attention_calloc_array(
        total, sizeof(*valid_indexes));
    if (!scores || !ordinals || !valid_indexes) {
        free(scores);
        free(ordinals);
        free(valid_indexes);
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
        unsigned long long ranked_count = 0ull;
        unsigned long long *ranked = (unsigned long long *)yvex_attention_calloc_array(
            yvex_attention_min_u64(valid, layer->sparse_topk.k), sizeof(*ranked));
        unsigned long long i;
        if (!ranked) {
            free(scores);
            free(ordinals);
            free(valid_indexes);
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
            return rc;
        }
        for (i = 0ull; i < ranked_count; ++i)
            selected[i] = valid_indexes[ranked[i]];
        *selected_count = ranked_count;
        free(ranked);
    }
    *valid_count = valid;
    free(scores);
    free(ordinals);
    free(valid_indexes);
    return YVEX_OK;

numeric:
    free(scores);
    free(ordinals);
    free(valid_indexes);
    return yvex_attention_reject(
        failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
        layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, candidate, err,
        YVEX_ERR_FORMAT, "CSA index scoring produced non-finite values");
}

// Purpose: Return the admitted softmax probe fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_softmax_probe(const float *query,
                                   unsigned long long query_count,
                                   const float *kv,
                                   unsigned long long kv_count,
                                   unsigned long long local_entries,
                                   unsigned long long compressed_entries,
                                   yvex_attention_class class_id,
                                   unsigned long long topk_limit,
                                   yvex_attention_cpu_result *result,
                                   yvex_attention_failure *failure,
                                   yvex_error *err)
{
    unsigned long long dim = yvex_attention_min_u64(query_count, kv_count);
    unsigned long long candidates = compressed_entries;
    unsigned long long selected = compressed_entries;
    float *candidate_scores = NULL;
    unsigned long long *candidate_ordinals = NULL;
    unsigned long long *selected_indices = NULL;
    unsigned long long entries;
    unsigned long long i;
    unsigned long long d;
    double max_score = -HUGE_VAL;
    double sum = 0.0;
    double output = 0.0;
    double scale;

    if (!query || !kv || !result || dim == 0ull || local_entries == 0ull)
        return 0;
    if (class_id == YVEX_ATTENTION_CLASS_CSA) {
        if (topk_limit == 0ull) topk_limit = 1ull;
        if (candidates <= topk_limit) candidates = topk_limit + 8ull;
        candidate_scores = (float *)yvex_attention_calloc_array(
            candidates, sizeof(*candidate_scores));
        candidate_ordinals = (unsigned long long *)yvex_attention_calloc_array(
            candidates, sizeof(*candidate_ordinals));
        selected_indices = (unsigned long long *)yvex_attention_calloc_array(
            topk_limit, sizeof(*selected_indices));
        if (!candidate_scores || !candidate_ordinals || !selected_indices)
            goto cleanup_fail;
        scale = 1.0 / sqrt((double)dim);
        for (i = 0ull; i < candidates; ++i) {
            double score = 0.0;
            double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
            for (d = 0ull; d < dim; ++d)
                score += (double)query[d] * (double)kv[d] * history_scale;
            score *= scale;
            if (!isfinite(score)) goto cleanup_fail;
            candidate_scores[i] = (float)score;
            candidate_ordinals[i] = i;
        }
        if (yvex_attention_topk_select(
                candidate_scores, candidate_ordinals, candidates, topk_limit,
                selected_indices, &selected, failure, err) != YVEX_OK)
            goto cleanup_fail;
    } else if (class_id == YVEX_ATTENTION_CLASS_SWA) {
        candidates = 0ull;
        selected = 0ull;
    }
    entries = local_entries + selected;
    if (entries == 0ull) return 0;
    scale = 1.0 / sqrt((double)dim);
    for (i = 0ull; i < entries; ++i) {
        double score = 0.0;
        double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
        for (d = 0ull; d < dim; ++d)
            score += (double)query[d] * (double)kv[d] * history_scale;
        score *= scale;
        if (!isfinite(score)) return 0;
        if (score > max_score) max_score = score;
    }
    for (i = 0ull; i < entries; ++i) {
        double score = 0.0;
        double history_scale = 1.0 + (double)(i % 11ull) / 32.0;
        double probability;
        for (d = 0ull; d < dim; ++d)
            score += (double)query[d] * (double)kv[d] * history_scale;
        probability = exp((score * scale) - max_score);
        if (!isfinite(probability)) return 0;
        sum += probability;
        output += probability * history_scale;
    }
    if (!isfinite(sum) || sum <= 0.0 || !isfinite(output)) return 0;
    result->topk_candidates = candidates;
    result->topk_selected = selected;
    result->local_entries = local_entries;
    result->compressed_entries = compressed_entries;
    result->deduplicated_entries = entries;
    result->attention_checksum = output / sum;
    result->output_checksum = result->attention_checksum;
    free(candidate_scores);
    free(candidate_ordinals);
    free(selected_indices);
    return 1;

cleanup_fail:
    free(candidate_scores);
    free(candidate_ordinals);
    free(selected_indices);
    return 0;
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

int yvex_attention_rolling_state_validate(
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
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
    rc = yvex_attention_rolling_state_validate(
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
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
    if (history->local_tail_count > layer->sliding_window)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->sliding_window, history->local_tail_count, err,
            YVEX_ERR_BOUNDS,
            "DeepSeek attention history exceeds sliding-window boundary");
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
    {
        unsigned long long i;
        for (i = 0ull; i < history->local_tail_count; ++i) {
            if (history->local_positions[i] >= history->token_count ||
                (i && history->local_positions[i - 1ull] >=
                          history->local_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count, history->local_positions[i], err,
                    YVEX_ERR_FORMAT,
                    "local history positions are stale or not strictly ordered");
        }
        for (i = 0ull; i < history->compressed_entry_count; ++i) {
            if (history->compressed_positions[i] >= history->token_count ||
                (i && history->compressed_positions[i - 1ull] >=
                          history->compressed_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count,
                    history->compressed_positions[i], err, YVEX_ERR_FORMAT,
                    "compressed history positions are stale or not strictly ordered");
        }
        for (i = 0ull; i < history->indexer_entry_count; ++i) {
            if (history->indexer_positions[i] >= history->token_count ||
                (i && history->indexer_positions[i - 1ull] >=
                          history->indexer_positions[i]) ||
                (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
                 history->indexer_positions[i] !=
                     history->compressed_positions[i]))
                return yvex_attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count, history->indexer_positions[i], err,
                    YVEX_ERR_FORMAT,
                    "indexer history positions do not bind compressed history");
        }
    }
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
        int rc = yvex_attention_rolling_state_validate(
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
            rc = yvex_attention_rolling_state_validate(
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Execution composes admitted generic operations without owning their math. */

#define YVEX_ATTENTION_PI 3.14159265358979323846264338327950288

// Purpose: Project the stable textual ABI label for an attention lifecycle status.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const char *yvex_attention_status_name(
    yvex_attention_status status)
{
    switch (status) {
    case YVEX_DEEPSEEK_ATTENTION_STATUS_REFUSED: return "refused";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_PLANNED: return "planned";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_UNSUPPORTED:
        return "execution-unsupported";
    case YVEX_DEEPSEEK_ATTENTION_STATUS_EXECUTION_READY:
        return "execution-ready";
    default: return "unknown";
    }
}
// Purpose: Project the stable textual ABI label for failure name.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
const char *yvex_attention_failure_name(
    yvex_attention_failure_code code)
{
    switch (code) {
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_NONE: return "none";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT:
        return "invalid-argument";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_ARCHITECTURE: return "architecture";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_MATERIALIZATION:
        return "materialization";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR: return "descriptor";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_MISSING_BINDING:
        return "missing-binding";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_QTYPE: return "qtype";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_DIMENSION: return "dimension";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY: return "history";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_EXECUTION_UNSUPPORTED:
        return "execution-unsupported";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_READ: return "read";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC: return "numeric";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA:
        return "state-delta";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION: return "allocation";
    case YVEX_DEEPSEEK_ATTENTION_FAILURE_BACKEND: return "backend";
    default: return "unknown";
    }
}
// Purpose: Return the admitted execute supported fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_execute_supported(const char **reason)
{
    if (reason)
        *reason = "attention-execution-incomplete";
    return 0;
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
