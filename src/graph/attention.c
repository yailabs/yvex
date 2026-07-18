/*
 * attention.c - generic attention protocol and state transaction owner.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   public status/failure names, identity-chain validation, checked helper
 *   primitives, state transactions, and the hard execution-support gate.
 *
 * Does not own:
 *   family scheduling, numerical execution, CUDA kernels, persistent KV,
 *   generation, CLI output, or release claims.
 *
 * Invariants:
 *   the execution gate covers only the complete attention equation; it never
 *   promotes persistent KV, transformer, prefill, decode, or generation.
 *
 * Boundary:
 *   scoped attention execution support is not persistent KV, transformer, or
 *   runtime-generation capability.
 */
#include "src/graph/private.h"

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

/* Contract: exposes the reopened execution gate without allocating or executing. */
int yvex_attention_execute_supported(const char **reason)
{
    if (reason)
        *reason = "attention-execution-incomplete";
    return 0;
}

/* Identity and validation primitives remain private to the graph owner. */

#include "src/model/families.h"

static void attention_failure_set(
    yvex_attention_failure *failure,
    yvex_attention_failure_code code,
    const yvex_runtime_tensor_binding *binding,
    unsigned long long layer_index,
    yvex_tensor_role role,
    unsigned long long expected,
    unsigned long long actual,
    const char *reason)
{
    if (!failure) return;
    memset(failure, 0, sizeof(*failure));
    failure->code = code;
    failure->layer_index = layer_index;
    failure->role = role;
    failure->expected = expected;
    failure->actual = actual;
    failure->reason = reason;
    if (binding && binding->binding)
        (void)snprintf(failure->tensor_name, sizeof(failure->tensor_name),
                       "%s", binding->binding->name);
}

/* Contract: records one typed refusal and leaves caller-owned outputs unset. */
int yvex_attention_reject(yvex_attention_failure *failure,
                            yvex_attention_failure_code code,
                            const yvex_runtime_tensor_binding *binding,
                            unsigned long long layer_index,
                            yvex_tensor_role role,
                            unsigned long long expected,
                            unsigned long long actual,
                            yvex_error *err,
                            yvex_status err_code,
                            const char *reason)
{
    attention_failure_set(failure, code, binding, layer_index, role, expected,
                          actual, reason);
    yvex_error_set(err, err_code, "yvex_deepseek_attention", reason);
    return err_code;
}

int yvex_attention_hash_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;

    for (i = 0u; i < 8u; ++i)
        bytes[7u - i] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

int yvex_attention_hash_text(yvex_sha256 *hash, const char *text)
{
    return yvex_sha256_update(hash, text ? text : "", text ? strlen(text) : 0u);
}


int yvex_attention_checked_mul_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out)
{
    if (!out || (left != 0ull && right > ULLONG_MAX / left)) return 0;
    *out = left * right;
    return 1;
}

/* Contract: adds two unsigned geometry values and refuses overflow. */
int yvex_attention_checked_add_u64(unsigned long long left,
                              unsigned long long right,
                              unsigned long long *out)
{
    if (!out || left > ULLONG_MAX - right) return 0;
    *out = left + right;
    return 1;
}

int yvex_attention_checked_size(unsigned long long count,
                                  unsigned long long width,
                                  size_t *out)
{
    unsigned long long bytes;

    if (!out || !yvex_attention_checked_mul_u64(count, width, &bytes) ||
        bytes > (unsigned long long)SIZE_MAX)
        return 0;
    *out = (size_t)bytes;
    return 1;
}

unsigned long long yvex_attention_min_u64(unsigned long long a,
                                            unsigned long long b)
{
    return a < b ? a : b;
}

void *yvex_attention_calloc_array(unsigned long long count,
                                    unsigned long long width)
{
    size_t bytes;

    if (!yvex_attention_checked_size(count, width, &bytes)) return NULL;
    return calloc(1u, bytes);
}

/* Contract: binds one execution call to the exact sealed logical, runtime,
 * materialization, and attention-plan identities before any payload read. */
int yvex_attention_context_validate(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    const yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    const yvex_attention_summary *attention;
    const yvex_runtime_descriptor_summary *runtime;
    const yvex_materialization_summary *materialization;
    char logical_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];

    if (!plan || !ir || !session || !descriptor)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "attention execution identity validation requires all owners");
    attention = yvex_graph_lower_deepseek_v4()->plan_summary(plan);
    runtime = yvex_runtime_descriptor_summary_get(descriptor);
    materialization = yvex_materialization_session_summary(session);
    if (!attention || !runtime || !materialization ||
        !materialization->committed ||
        !yvex_model_register_deepseek_v4()->transform.architecture_identity(ir, logical_identity))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "attention execution requires sealed identity-bearing owners");
    if (strcmp(logical_identity, runtime->logical_model_identity) != 0 ||
        strcmp(logical_identity, attention->logical_model_identity) != 0 ||
        strcmp(runtime->runtime_numeric_identity,
               attention->runtime_numeric_identity) != 0 ||
        strcmp(runtime->runtime_descriptor_identity,
               attention->runtime_descriptor_identity) != 0 ||
        strcmp(materialization->plan_identity,
               runtime->materialization_plan_identity) != 0 ||
        strcmp(materialization->plan_identity,
               attention->materialization_plan_identity) != 0)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_DESCRIPTOR, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_STATE,
            "attention execution refused a stale or mismatched identity chain");
    return YVEX_OK;
}

/* The in-memory sink stages all components before an atomic commit. */

static void attention_component_release(
    yvex_attention_component_span *span)
{
    if (!span) return;
    free(span->data);
    memset(span, 0, sizeof(*span));
}

/* Contract: releases every staged component in one in-flight transaction. */
static void attention_transaction_release_staging(
    yvex_attention_state_transaction *transaction)
{
    unsigned int i;

    if (!transaction) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&transaction->components[i]);
}

/* Contract: initializes one required F32 component with checked extents. */
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
        if (!yvex_attention_checked_mul_u64(dim0, stride, &elements))
            return 0;
    } else {
        elements = dim0;
    }
    if (!yvex_attention_checked_mul_u64(elements, sizeof(float), &bytes) ||
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

/* Contract: creates one semantic content identity for a committed delta. */
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

/* Contract: checks F32 component contents before publication. */
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

void yvex_attention_memory_sink_options_default(
    yvex_attention_memory_sink_options *options)
{
    if (!options) return;
    memset(options, 0, sizeof(*options));
    options->fail_acquire_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
    options->fail_seal_kind = YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT;
}

/* Contract: initializes one caller-owned memory sink with no publication. */
int yvex_attention_memory_sink_init(
    yvex_attention_memory_sink *sink,
    const yvex_attention_memory_sink_options *options,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    yvex_attention_memory_sink_options defaults;

    if (!sink)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "DeepSeek attention memory sink requires caller storage");
    yvex_attention_memory_sink_options_default(&defaults);
    memset(sink, 0, sizeof(*sink));
    sink->initialized = 1;
    sink->options = options ? *options : defaults;
    if (!options) sink->options = defaults;
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

void yvex_attention_memory_sink_release(
    yvex_attention_memory_sink *sink)
{
    unsigned int i;

    if (!sink) return;
    for (i = 0u; i < YVEX_DEEPSEEK_ATTENTION_COMPONENT_COUNT; ++i)
        attention_component_release(&sink->committed[i]);
    memset(sink, 0, sizeof(*sink));
}

/* Contract: preflights and begins one complete output/state transaction. */
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
            layer ? layer->layer_index : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, 1ull, token_count, err,
            YVEX_ERR_INVALID_ARG,
            "DeepSeek attention state transaction requires sink, layer, history, and tokens");
    if (sink->options.fail_begin)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_STATE_DELTA, NULL,
            layer->layer_index, YVEX_TENSOR_ROLE_UNKNOWN, 1ull, 0ull, err,
            YVEX_ERR_STATE,
            "DeepSeek attention memory sink injected begin failure");
    rc = yvex_graph_lower_deepseek_v4()->history_validate(layer, history, failure, err);
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
    if (layer->attention_class != YVEX_DEEPSEEK_V4_ATTENTION_SWA) {
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
    if (layer->attention_class == YVEX_DEEPSEEK_V4_ATTENTION_CSA) {
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: grants one writable staging span exactly once per transaction. */
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
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: copies one complete component into transaction staging storage. */
int yvex_attention_state_transaction_write(
    yvex_attention_state_transaction *transaction,
    yvex_attention_component_kind kind,
    const void *bytes,
    size_t byte_count,
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
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
            YVEX_TENSOR_ROLE_UNKNOWN, YVEX_DEEPSEEK_ATTENTION_TRANSACTION_BEGUN,
            transaction ? transaction->status
                        : YVEX_DEEPSEEK_ATTENTION_TRANSACTION_EMPTY,
            err, YVEX_ERR_STATE,
            "DeepSeek attention state transaction write requires begun state");
    span = &transaction->components[kind];
    if (!span->required || !span->acquired || span->sealed || !span->data ||
        !bytes || byte_count != (size_t)span->byte_extent)
        return yvex_attention_reject(
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
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: atomically publishes every sealed component to caller-owned sink. */
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
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

/* Contract: discards all staged state without modifying committed output. */
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
                        : YVEX_DEEPSEEK_V4_IR_NO_LAYER,
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
    yvex_error_clear(err);
    if (failure) memset(failure, 0, sizeof(*failure));
    return YVEX_OK;
}

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

const char *yvex_attention_memory_sink_identity(
    const yvex_attention_memory_sink *sink)
{
    if (!sink || !sink->initialized || !sink->has_committed) return NULL;
    return sink->committed_identity;
}

int yvex_attention_state_delta_begin(
    const yvex_attention_layer_plan *layer,
    unsigned long long token_position,
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!layer || !delta)
        return yvex_attention_reject(
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

int yvex_attention_state_delta_commit(
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!delta || delta->status != YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING)
        return yvex_attention_reject(
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

int yvex_attention_state_delta_abort(
    yvex_attention_state_delta *delta,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    if (!delta || delta->status != YVEX_DEEPSEEK_ATTENTION_DELTA_PENDING)
        return yvex_attention_reject(
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
