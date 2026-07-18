/*
 * yvex_deepseek_attention_sink.c - DeepSeek-V4 attention transactional sink owner.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   memory-backed state transaction preflight, component staging, seal validation, commit/abort, and delta identity.
 *
 * Does not own:
 *   source parsing, GGUF mapping, materialization ownership, persistent KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, release
 *   claims, CLI parsing, or rendering.
 *
 * Invariants:
 *   no component becomes visible before commit; abort never mutates committed state.
 *
 * Boundary:
 *   transactional state output is not persistent KV storage.
 */
#include "yvex_deepseek_attention_internal.h"

static void attention_component_release(
    yvex_deepseek_attention_component_span *span)
{
    if (!span) return;
    free(span->data);
    memset(span, 0, sizeof(*span));
}

/* Contract: releases every staged component in one in-flight transaction. */
static void attention_transaction_release_staging(
    yvex_deepseek_attention_state_transaction *transaction)
{
    unsigned int i;

    if (!transaction) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&transaction->components[i]);
}

/* Contract: initializes one required F32 component with checked extents. */
static int attention_component_prepare_f32(
    yvex_deepseek_attention_component_span *span,
    yvex_deepseek_attention_component_kind kind,
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
        if (!attention_checked_mul_u64(dim0, stride, &elements))
            return 0;
    } else {
        elements = dim0;
    }
    if (!attention_checked_mul_u64(elements, sizeof(float), &bytes) ||
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

/* Contract: counts compressor emissions in one absolute token interval. */
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

/* Contract: allocates staging for each required component after preflight. */
static int attention_transaction_allocate_staging(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    unsigned int i;

    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        yvex_deepseek_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        span->data = calloc(1u, (size_t)span->byte_extent);
        if (!span->data) {
            attention_transaction_release_staging(transaction);
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
                transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                span->byte_extent, 0ull, err, YVEX_ERR_NOMEM,
                "DeepSeek attention state transaction staging allocation failed");
        }
    }
    return YVEX_OK;
}

/* Contract: creates one semantic content identity for a committed delta. */
static int attention_transaction_identity(
    const yvex_deepseek_attention_state_transaction *transaction,
    char out[YVEX_DEEPSEEK_ATTENTION_IDENTITY_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned int i;

    if (!transaction || !out) return 0;
    yvex_sha256_init(&hash);
    if (!attention_hash_text(&hash, "yvex.deepseek.attention.delta.v1") ||
        !attention_hash_u64(&hash, transaction->layer_index) ||
        !attention_hash_u64(&hash, transaction->attention_class) ||
        !attention_hash_u64(&hash, transaction->token_position) ||
        !attention_hash_u64(&hash, transaction->token_count) ||
        !attention_hash_text(&hash, transaction->previous_state_identity))
        return 0;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_deepseek_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        if (!attention_hash_u64(&hash, span->kind) ||
            !attention_hash_u64(&hash, span->rank) ||
            !attention_hash_u64(&hash, span->dims[0]) ||
            !attention_hash_u64(&hash, span->dims[1]) ||
            !attention_hash_u64(&hash, span->stride) ||
            !attention_hash_u64(&hash, span->produced_elements) ||
            !attention_hash_u64(&hash, span->byte_extent) ||
            !yvex_sha256_update(&hash, span->data, (size_t)span->byte_extent))
            return 0;
    }
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}

/* Contract: checks F32 component contents before publication. */
static int attention_component_values_are_finite(
    const yvex_deepseek_attention_component_span *span)
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

void yvex_deepseek_attention_memory_sink_options_default(
    yvex_deepseek_attention_memory_sink_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->fail_acquire_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
    options->fail_seal_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
}

/* Contract: initializes one caller-owned memory sink with no publication. */
int yvex_deepseek_attention_memory_sink_init(
    yvex_deepseek_attention_memory_sink *sink,
    const yvex_deepseek_attention_memory_sink_options *options,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_attention_memory_sink_options defaults;

    if (!sink)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention memory sink requires caller storage");
    yvex_deepseek_attention_memory_sink_options_default(&defaults);
    memset(sink, 0, sizeof(*sink));
    sink->initialized = 1;
    sink->options = options ? *options : defaults;
    if (!options) sink->options = defaults;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

void yvex_deepseek_attention_memory_sink_release(
    yvex_deepseek_attention_memory_sink *sink)
{
    unsigned int i;

    if (!sink) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&sink->committed[i]);
    memset(sink, 0, sizeof(*sink));
}

/* Contract: preflights and begins one complete output/state transaction. */
int yvex_deepseek_attention_state_transaction_begin(
    yvex_deepseek_attention_memory_sink *sink,
    const yvex_deepseek_attention_layer_plan *layer,
    const yvex_deepseek_attention_history_view *history,
    unsigned long long token_position,
    unsigned long long token_count,
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long compressed_count = 0ull;
    int rc;

    if (transaction) memset(transaction, 0, sizeof(*transaction));
    if (!sink || !sink->initialized || !layer || !history || !transaction ||
        token_count == 0ull)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, token_count, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention state transaction requires sink, layer, history, and tokens");
    if (sink->options.fail_begin)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected begin failure");
    rc = yvex_deepseek_attention_history_validate(layer, history, failure, err);
    if (rc != YVEX_OK) return rc;
    if (history->token_count != token_position)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, history->token_count,
            token_position, err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction token position is not contiguous");
    if (token_position > ULLONG_MAX - token_count)
        return attention_reject(
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
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, token_count, 0ull,
            err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state transaction output extent overflowed");
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
        const yvex_deepseek_attention_rolling_state_view *main_state =
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
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                main_state->state_slots, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention main rolling-state extent overflowed");
        if (!attention_emission_count(token_position, token_count,
                                      layer->compression_ratio,
                                      &compressed_count))
            return attention_reject(
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
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                compressed_count, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention compressed KV extent overflowed");
    }
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
        const yvex_deepseek_attention_rolling_state_view *index_state =
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
            return attention_reject(
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
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                compressed_count, 0ull, err, YVEX_ERR_BOUNDS,
                "DeepSeek attention indexer KV extent overflowed");
    }
    rc = attention_transaction_allocate_staging(transaction, failure, err);
    if (rc != YVEX_OK) return rc;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: grants one writable staging span exactly once per transaction. */
int yvex_deepseek_attention_state_transaction_acquire(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    yvex_deepseek_attention_component_span *out,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_attention_component_span *span;

    if (out) memset(out, 0, sizeof(*out));
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction acquire requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->data || span->acquired)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            span->acquired, err, YVEX_ERR_STATE,
            "DeepSeek attention state component is absent or already acquired");
    if (transaction->sink &&
        transaction->sink->options.fail_acquire_kind == kind)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected acquire failure");
    span->acquired = 1;
    if (out) *out = *span;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: copies one complete component into transaction staging storage. */
int yvex_deepseek_attention_state_transaction_write(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    const void *bytes,
    size_t byte_count,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_attention_component_span *span;

    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction write requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->acquired || span->sealed || !span->data ||
        !bytes || byte_count != (size_t)span->byte_extent)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            span->byte_extent, byte_count, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state component write has wrong extent");
    memcpy(span->data, bytes, byte_count);
    span->written = 1;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: seals one staged component after exact production. */
int yvex_deepseek_attention_state_transaction_seal(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_component_kind kind,
    unsigned long long produced_elements,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_attention_component_span *span;

    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction seal requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->acquired || span->sealed ||
        produced_elements != span->expected_elements)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
            span->expected_elements, produced_elements, err, YVEX_ERR_BOUNDS,
            "DeepSeek attention state component seal has wrong element count");
    if (transaction->sink && transaction->sink->options.fail_seal_kind == kind)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected seal failure");
    if (!attention_component_values_are_finite(span))
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_FORMAT,
            "DeepSeek attention state component contains non-finite values");
    span->produced_elements = produced_elements;
    span->written = 1;
    span->sealed = 1;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: atomically publishes every sealed component to caller-owned sink. */
int yvex_deepseek_attention_state_transaction_commit(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    yvex_deepseek_attention_component_span next[
        YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT];
    unsigned int i;

    memset(next, 0, sizeof(next));
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN ||
        !transaction->sink || !transaction->sink->initialized)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction commit requires begun state");
    if (transaction->sink->options.fail_commit)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected commit failure");
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_deepseek_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        if (!span->acquired || !span->written || !span->sealed ||
            span->produced_elements != span->expected_elements)
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
                transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN,
                span->expected_elements, span->produced_elements, err,
                YVEX_ERR_STATE,
                "DeepSeek attention state transaction cannot commit incomplete component");
    }
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i) {
        const yvex_deepseek_attention_component_span *span =
            &transaction->components[i];
        if (!span->required) continue;
        next[i] = *span;
        next[i].data = malloc((size_t)span->byte_extent);
        if (!next[i].data) {
            unsigned int j;
            for (j = 0u; j < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++j)
                attention_component_release(&next[j]);
            return attention_reject(
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
        return attention_reject(
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: discards all staged state without modifying committed output. */
int yvex_deepseek_attention_state_transaction_abort(
    yvex_deepseek_attention_state_transaction *transaction,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    if (!transaction ||
        transaction->status != YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction ? transaction->layer_index
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction abort requires begun state");
    attention_transaction_release_staging(transaction);
    transaction->status = YVEX_DEEPSEEK_ATTENTION_TRANSACTION_ABORTED;
    if (transaction->sink && transaction->sink->options.fail_abort)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            transaction->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull,
            err, YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected abort failure");
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

const yvex_deepseek_attention_component_span *
yvex_deepseek_attention_memory_sink_committed_component(
    const yvex_deepseek_attention_memory_sink *sink,
    yvex_deepseek_attention_component_kind kind)
{
    if (!sink || !sink->initialized || !sink->has_committed ||
        kind >= YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT ||
        !sink->committed[kind].required)
        return NULL;
    return &sink->committed[kind];
}

const char *yvex_deepseek_attention_memory_sink_identity(
    const yvex_deepseek_attention_memory_sink *sink)
{
    if (!sink || !sink->initialized || !sink->has_committed) return NULL;
    return sink->committed_identity;
}

int yvex_deepseek_attention_state_delta_begin(
    const yvex_deepseek_attention_layer_plan *layer,
    unsigned long long token_position,
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    if (!layer || !delta)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention state delta requires layer and destination");
    memset(delta, 0, sizeof(*delta));
    delta->status = YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING;
    delta->layer_index = layer->layer_index;
    delta->attention_class = layer->attention_class;
    delta->token_position = token_position;
    delta->raw_kv_entries = 1ull;
    if (layer->compression_ratio &&
        ((token_position + 1ull) % layer->compression_ratio) == 0ull)
        delta->compressed_kv_entries = 1ull;
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA &&
        delta->compressed_kv_entries)
        delta->indexer_entries = 1ull;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

int yvex_deepseek_attention_state_delta_commit(
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    if (!delta || delta->status != YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            delta ? delta->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN,
            YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING,
            delta ? delta->status : YVEX_DEEPSEEK_ATTENTION_DELTA_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state delta must be pending before commit");
    delta->status = YVEX_DEEPSEEK_ATTENTION_DELTA_COMMITTED;
    delta->published = 1;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

int yvex_deepseek_attention_state_delta_abort(
    yvex_deepseek_attention_state_delta *delta,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    if (!delta || delta->status != YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_HISTORY, NULL,
            delta ? delta->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN,
            YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING,
            delta ? delta->status : YVEX_DEEPSEEK_ATTENTION_DELTA_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state delta must be pending before abort");
    delta->status = YVEX_DEEPSEEK_ATTENTION_DELTA_ABORTED;
    delta->published = 0;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}
