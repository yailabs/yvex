/*
 * yvex_deepseek_attention_compressor.c - DeepSeek-V4 attention compressor-state owner.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   rolling-state geometry, CSA/HCA rolling validation, production CPU
 *   transition, history validation, and compressor-state math.
 *
 * Does not own:
 *   source parsing, GGUF mapping, materialization ownership, persistent KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, release
 *   claims, CLI parsing, or rendering.
 *
 * Invariants:
 *   compressor state is caller-owned and transactional; this module never owns persistent KV lifetime.
 *
 * Boundary:
 *   rolling compression state is not persistent KV cache ownership.
 */
#include "yvex_deepseek_attention_internal.h"

int attention_rolling_geometry(
    const yvex_deepseek_attention_layer_plan *layer,
    yvex_deepseek_attention_rolling_kind kind,
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
            layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA ? 1 : 0;
        *rotated = 0;
    } else if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER) {
        if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_CSA)
            return 0;
        *ratio = layer->compression_ratio;
        *head_dim = layer->indexer_head_dimension;
        *overlap = 1;
        *rotated = 1;
    } else {
        return 0;
    }
    coeff = *overlap ? 2ull : 1ull;
    if (!attention_checked_mul_u64(*head_dim, coeff, state_width) ||
        !attention_checked_mul_u64(*ratio, coeff, state_slots))
        return 0;
    return *ratio != 0ull && *head_dim != 0ull;
}

/* Contract: checks active rolling slots without requiring unused score slots. */
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

/* Contract: validates one immutable rolling compressor state view. */
int yvex_deepseek_attention_rolling_state_validate(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_rolling_state_view *state,
    yvex_deepseek_attention_rolling_kind kind,
    yvex_deepseek_attention_failure *failure,
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
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling state is missing");
    if (!attention_rolling_geometry(layer, kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return attention_reject(
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
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->state_width, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state identity or geometry mismatch");
    if (state->cursor >= ratio || state->previous_fill > ratio ||
        state->current_fill > ratio || (!overlap && state->previous_fill))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ratio,
            state->cursor, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state cursor or fill is invalid");
    if (state->kv_state_stride < state_width ||
        state->score_state_stride < state_width || !state->kv_state ||
        !state->score_state)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            state->kv_state_stride, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state storage is incomplete");
    if (!attention_checked_mul_u64(state_slots, state->kv_state_stride,
                                   &required_extent) ||
        state->kv_state_extent < required_extent ||
        !attention_checked_mul_u64(state_slots, state->score_state_stride,
                                   &required_extent) ||
        state->score_state_extent < required_extent)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_slots,
            state->kv_state_extent, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling state extent is too small");
    if (!attention_rolling_active_values_are_finite(
            state->kv_state, state->score_state, state->kv_state_stride,
            state->score_state_stride, head_dim, ratio, state->previous_fill,
            state->current_fill, overlap))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention rolling state contains non-finite active values");
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: copies one rolling-state buffer preserving caller-owned strides. */
static void attention_rolling_copy_state(
    const yvex_deepseek_attention_rolling_state_view *before,
    yvex_deepseek_attention_rolling_state_output *after)
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

/* Contract: computes a stable softmax-weighted compressed vector. */
static int attention_rolling_emit(
    const yvex_deepseek_attention_rolling_state_output *state,
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

/* Contract: executes one production CPU compressor-state transition. */
int yvex_deepseek_attention_rolling_state_step_cpu(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_rolling_state_view *before,
    const float *token_kv,
    const float *token_score,
    const float *ape_row,
    yvex_deepseek_attention_rolling_state_output *after,
    float *compressed_out,
    unsigned long long compressed_out_count,
    int *emitted,
    yvex_deepseek_attention_failure *failure,
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
    rc = yvex_deepseek_attention_rolling_state_validate(
        layer, before, before ? before->kind : YVEX_DEEPSEEK_ATTENTION_ROLLING_NONE,
        failure, err);
    if (rc != YVEX_OK) return rc;
    if (!token_kv || !token_score || !ape_row || !after)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention rolling transition requires token vectors and output state");
    if (before->next_token_position == ULLONG_MAX)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, ULLONG_MAX,
            before->next_token_position, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention rolling token position would overflow");
    if (!attention_rolling_geometry(layer, before->kind, &ratio, &head_dim,
                                    &state_width, &state_slots, &overlap,
                                    &rotated))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_UNSUPPORTED,
            "DeepSeek attention rolling transition lacks geometry");
    if (!after->kv_state || !after->score_state ||
        after->kv_state_stride < state_width ||
        after->score_state_stride < state_width ||
        !attention_checked_mul_u64(state_slots, after->kv_state_stride,
                                   &required_extent) ||
        after->kv_state_extent < required_extent ||
        !attention_checked_mul_u64(state_slots, after->score_state_stride,
                                   &required_extent) ||
        after->score_state_extent < required_extent)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, state_width,
            after ? after->kv_state_stride : 0ull, err, YVEX_ERR_FORMAT,
            "DeepSeek attention rolling output storage is incomplete");
    if (before->cursor != (before->next_token_position % ratio))
        return attention_reject(
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
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, lane, err,
                YVEX_ERR_FORMAT,
                "DeepSeek attention rolling transition input is non-finite");
    }
    *after = (yvex_deepseek_attention_rolling_state_output){
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
            return attention_reject(
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

int yvex_deepseek_attention_history_validate(
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_history_view *history,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    if (!layer || !history)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention history validation requires layer and history");
    if (!history->immutable)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention history view must be immutable");
    if (history->local_tail_count > layer->sliding_window)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->sliding_window, history->local_tail_count, err,
            YVEX_ERR_BOUNDS,
            "DeepSeek attention history exceeds sliding-window boundary");
    if (history->local_tail_count &&
        (!history->local_kv || !history->local_positions ||
         history->local_kv_stride < layer->head_dimension))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->local_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention local history lacks raw KV storage");
    if (history->compressed_entry_count &&
        (!history->compressed_kv || !history->compressed_positions ||
         history->compressed_kv_stride < layer->head_dimension))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->head_dimension, history->compressed_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention compressed history lacks KV storage");
    if (history->indexer_entry_count &&
        (!history->indexer_kv || !history->indexer_positions ||
         history->indexer_kv_stride < layer->indexer_head_dimension))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            layer->indexer_head_dimension, history->indexer_kv_stride, err,
            YVEX_ERR_FORMAT,
            "DeepSeek attention indexer history lacks KV storage");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
        history->compressed_entry_count != history->indexer_entry_count)
        return attention_reject(
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
                return attention_reject(
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
                return attention_reject(
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
                (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
                 history->indexer_positions[i] !=
                     history->compressed_positions[i]))
                return attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count, history->indexer_positions[i], err,
                    YVEX_ERR_FORMAT,
                    "indexer history positions do not bind compressed history");
        }
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA &&
        (history->compressed_entry_count || history->indexer_entry_count))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->compressed_entry_count + history->indexer_entry_count,
            err, YVEX_ERR_FORMAT,
            "SWA history may not carry compressed or indexer entries");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_HCA &&
        history->indexer_entry_count)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull,
            history->indexer_entry_count, err, YVEX_ERR_FORMAT,
            "HCA history may not carry CSA indexer entries");
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        if (history->main_rolling_state.present ||
            history->indexer_rolling_state.present)
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 0ull, 1ull,
                err, YVEX_ERR_FORMAT,
                "SWA history may not carry compressor rolling state");
    } else {
        int rc = yvex_deepseek_attention_rolling_state_validate(
            layer, &history->main_rolling_state,
            YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN, failure, err);
        if (rc != YVEX_OK) return rc;
        if (history->main_rolling_state.next_token_position !=
            history->token_count)
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                history->token_count,
                history->main_rolling_state.next_token_position, err,
                YVEX_ERR_STATE,
                "main rolling state token position is stale");
        if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
            rc = yvex_deepseek_attention_rolling_state_validate(
                layer, &history->indexer_rolling_state,
                YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER, failure, err);
            if (rc != YVEX_OK) return rc;
            if (history->indexer_rolling_state.next_token_position !=
                history->token_count)
                return attention_reject(
                    failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
                    layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                    history->token_count,
                    history->indexer_rolling_state.next_token_position, err,
                    YVEX_ERR_STATE,
                    "indexer rolling state token position is stale");
        } else if (history->indexer_rolling_state.present) {
            return attention_reject(
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
