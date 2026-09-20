/*
 * Turn explicit operator token IDs into a bounded, validated immutable input fact.
 *
 * Token count never exceeds fixed capacity and validated IDs remain below vocab size. Typed
 * token-input admission consumed by runtime and CLI adapters.
 */

#include <yvex/tokenizer.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/core.h>

typedef struct {
    unsigned int token_id;
    yvex_token_append_state state;
} token_sequence_row;

struct yvex_token_sequence {
    token_sequence_row *rows;
    unsigned long long count, capacity, generation;
    int transaction_active;
};

struct yvex_token_sequence_transaction {
    yvex_token_sequence *sequence;
    token_sequence_row *rows;
    unsigned long long base_count, base_generation;
    unsigned long long count, capacity, generation;
    int prepared;
};

void yvex_token_input_init(yvex_token_input *input, yvex_token_input_kind kind)
{
    if (!input) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->kind = kind;
    input->max_tokens = YVEX_TOKEN_INPUT_MAX_TOKENS;
}

const char *yvex_token_input_kind_name(yvex_token_input_kind kind)
{
    switch (kind) {
    case YVEX_TOKEN_INPUT_EXPLICIT: return "explicit";
    case YVEX_TOKEN_INPUT_PROMPT_TEXT: return "prompt-text";
    }
    return "unknown";
}

static int token_input_append(yvex_token_input *input,
                              unsigned int token,
                              yvex_error *err)
{
    if (input->token_count >= input->max_tokens ||
        input->token_count >= YVEX_TOKEN_INPUT_MAX_TOKENS) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_token_input",
                       "token-count-too-large");
        return YVEX_ERR_BOUNDS;
    }
    input->tokens[input->token_count++] = token;
    return YVEX_OK;
}

/* Construct bounded token input from a caller-supplied ID array. */
int yvex_token_input_from_ids(yvex_token_input_kind kind,
                              const unsigned int *ids,
                              unsigned long long count,
                              yvex_token_input *out,
                              yvex_error *err)
{
    unsigned long long i;

    if (!out || (!ids && count > 0)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_from_ids",
                       "ids and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (count == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_from_ids",
                       "token-list-empty");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_token_input_init(out, kind);
    for (i = 0; i < count; ++i) {
        int rc = token_input_append(out, ids[i], err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Parse comma-separated decimal token IDs into fixed-capacity storage.
 *
 * Syntax, numeric overflow, empty input, or capacity excess is refused.
 */
int yvex_token_input_parse_explicit(const char *text,
                                    yvex_token_input *out,
                                    yvex_error *err)
{
    const char *p;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                       "output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_token_input_init(out, YVEX_TOKEN_INPUT_EXPLICIT);
    if (!text || text[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                       "token-list-empty");
        return YVEX_ERR_INVALID_ARG;
    }

    p = text;
    while (*p) {
        char *end = NULL;
        unsigned long long value;

        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        if (*p == ',' || *p == '-' || *p == '+') {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                           "token-parse-invalid");
            return YVEX_ERR_INVALID_ARG;
        }

        errno = 0;
        value = strtoull(p, &end, 10);
        if (errno == ERANGE || value > 0xffffffffull) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_token_input_parse_explicit",
                           "token-id-overflow");
            return YVEX_ERR_BOUNDS;
        }
        if (end == p) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                           "token-parse-invalid");
            return YVEX_ERR_INVALID_ARG;
        }
        if (token_input_append(out, (unsigned int)value, err) != YVEX_OK) {
            return yvex_error_code(err);
        }

        p = end;
        while (*p && isspace((unsigned char)*p)) {
            ++p;
        }
        if (*p == ',') {
            ++p;
            while (*p && isspace((unsigned char)*p)) {
                ++p;
            }
            if (*p == '\0') {
                yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                               "token-parse-invalid");
                return YVEX_ERR_INVALID_ARG;
            }
            continue;
        }
        if (*p != '\0') {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                           "token-parse-invalid");
            return YVEX_ERR_INVALID_ARG;
        }
    }

    if (out->token_count == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_parse_explicit",
                       "token-list-empty");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_input_validate_bounds(yvex_token_input *input,
                                     unsigned long long vocab_size,
                                     yvex_error *err)
{
    unsigned long long i;

    if (!input) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_validate_bounds",
                       "input is required");
        return YVEX_ERR_INVALID_ARG;
    }
    input->token_bounds_checked = 1;
    input->token_bounds_valid = 0;
    input->vocab_size = vocab_size;
    if (input->token_count == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_validate_bounds",
                       "token-list-empty");
        return YVEX_ERR_INVALID_ARG;
    }
    if (vocab_size == 0) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_token_input_validate_bounds",
                       "token-vocab-unavailable");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < input->token_count; ++i) {
        if ((unsigned long long)input->tokens[i] >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_token_input_validate_bounds",
                            "token-out-of-vocab: %u >= %llu",
                            input->tokens[i], vocab_size);
            return YVEX_ERR_BOUNDS;
        }
    }
    input->token_bounds_valid = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_input_select(const yvex_token_input *input,
                            unsigned long long token_index,
                            unsigned int *out_token,
                            yvex_error *err)
{
    if (!input || !out_token) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_token_input_select",
                       "input and output token are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (token_index >= input->token_count) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_token_input_select",
                        "token-index-out-of-range: %llu >= %llu",
                        token_index, input->token_count);
        return YVEX_ERR_BOUNDS;
    }
    *out_token = input->tokens[token_index];
    yvex_error_clear(err);
    return YVEX_OK;
}

static int sequence_identity(const yvex_token_sequence *sequence,
                             yvex_token_sequence_summary *summary)
{
    yvex_sha256 ids_hash, state_hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&ids_hash);
    yvex_sha256_init(&state_hash);
    if (!yvex_sha256_update_text(&ids_hash, "yvex.tokenizer.append.ids.v1") ||
        !yvex_sha256_update_text(&state_hash, "yvex.tokenizer.append.state.v1") ||
        !yvex_sha256_update_u64_be(&ids_hash, sequence->count) ||
        !yvex_sha256_update_u64_be(&state_hash, sequence->count) ||
        !yvex_sha256_update_u64_be(&state_hash, sequence->generation))
        return 0;
    for (index = 0u; index < sequence->count; ++index)
        if (!yvex_sha256_update_u64_be(
                &ids_hash, sequence->rows[index].token_id) ||
            !yvex_sha256_update_u64_be(
                &state_hash, sequence->rows[index].token_id) ||
            !yvex_sha256_update_u64_be(
                &state_hash, sequence->rows[index].state))
            return 0;
    if (!yvex_sha256_final(&ids_hash, digest)) return 0;
    yvex_sha256_hex(digest, summary->token_ids_identity);
    if (!yvex_sha256_final(&state_hash, digest)) return 0;
    yvex_sha256_hex(digest, summary->state_identity);
    return 1;
}

int yvex_token_sequence_open(yvex_token_sequence **out,
                             unsigned long long capacity,
                             yvex_error *err)
{
    yvex_token_sequence *sequence;
    if (!out || !capacity || capacity > SIZE_MAX / sizeof(token_sequence_row)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tokenizer.append.open",
                       "bounded nonzero capacity is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    sequence = calloc(1u, sizeof(*sequence));
    if (sequence)
        sequence->rows = calloc((size_t)capacity, sizeof(*sequence->rows));
    if (!sequence || !sequence->rows) {
        free(sequence ? sequence->rows : NULL);
        free(sequence);
        yvex_error_set(err, YVEX_ERR_NOMEM, "tokenizer.append.open",
                       "token directory allocation failed");
        return YVEX_ERR_NOMEM;
    }
    sequence->capacity = capacity;
    *out = sequence;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_append(yvex_token_sequence *sequence,
                               unsigned int token_id,
                               unsigned long long vocabulary_size,
                               unsigned long long *ordinal,
                               yvex_error *err)
{
    unsigned long long next_count, next_generation;
    if (!sequence || !ordinal || sequence->transaction_active ||
        token_id >= vocabulary_size || sequence->count >= sequence->capacity) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "tokenizer.append",
                       "token ID or directory capacity is invalid");
        return YVEX_ERR_BOUNDS;
    }
    if (!yvex_core_u64_add(sequence->count, 1u, &next_count) ||
        !yvex_core_u64_add(sequence->generation, 1u, &next_generation)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "tokenizer.append",
                       "token directory counter overflow");
        return YVEX_ERR_BOUNDS;
    }
    *ordinal = sequence->count;
    sequence->rows[sequence->count].token_id = token_id;
    sequence->rows[sequence->count].state = YVEX_TOKEN_APPEND_PROPOSED;
    sequence->count = next_count;
    sequence->generation = next_generation;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_transaction_begin(
    yvex_token_sequence *sequence, unsigned long long maximum_rows,
    yvex_token_sequence_transaction **out, yvex_error *err)
{
    yvex_token_sequence_transaction *transaction;
    if (out) *out = NULL;
    if (!sequence || !out || !maximum_rows || sequence->transaction_active ||
        maximum_rows > sequence->capacity - sequence->count ||
        maximum_rows > SIZE_MAX / sizeof(token_sequence_row)) {
        yvex_error_set(err, YVEX_ERR_STATE,
                       "tokenizer.append.transaction.begin",
                       "token sequence cannot admit the requested transaction");
        return YVEX_ERR_STATE;
    }
    transaction = calloc(1u, sizeof(*transaction));
    if (transaction)
        transaction->rows = calloc((size_t)maximum_rows,
                                   sizeof(*transaction->rows));
    if (!transaction || !transaction->rows) {
        free(transaction ? transaction->rows : NULL);
        free(transaction);
        yvex_error_set(err, YVEX_ERR_NOMEM,
                       "tokenizer.append.transaction.begin",
                       "token transaction allocation failed");
        return YVEX_ERR_NOMEM;
    }
    transaction->sequence = sequence;
    transaction->base_count = sequence->count;
    transaction->base_generation = sequence->generation;
    transaction->generation = sequence->generation;
    transaction->capacity = maximum_rows;
    sequence->transaction_active = 1;
    *out = transaction;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_transaction_append(
    yvex_token_sequence_transaction *transaction, unsigned int token_id,
    unsigned long long vocabulary_size, unsigned long long *ordinal,
    yvex_error *err)
{
    unsigned long long next_generation;
    if (!transaction || transaction->prepared || !ordinal ||
        token_id >= vocabulary_size ||
        transaction->count >= transaction->capacity ||
        !yvex_core_u64_add(
            transaction->generation, 1ull, &next_generation)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "tokenizer.append.transaction.append",
                       "staged token or transaction extent is invalid");
        return YVEX_ERR_BOUNDS;
    }
    *ordinal = transaction->base_count + transaction->count;
    transaction->rows[transaction->count].token_id = token_id;
    transaction->rows[transaction->count].state = YVEX_TOKEN_APPEND_PROPOSED;
    transaction->count++;
    transaction->generation = next_generation;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_transaction_transition(
    yvex_token_sequence_transaction *transaction, unsigned long long ordinal,
    yvex_token_append_state expected, yvex_token_append_state next,
    yvex_error *err)
{
    unsigned long long local, next_generation;
    if (!transaction || transaction->prepared ||
        ordinal < transaction->base_count) {
        yvex_error_set(err, YVEX_ERR_STATE,
                       "tokenizer.append.transaction.transition",
                       "staged append transition is stale");
        return YVEX_ERR_STATE;
    }
    local = ordinal - transaction->base_count;
    if (local >= transaction->count ||
        transaction->rows[local].state != expected ||
        next != (yvex_token_append_state)(expected + 1) ||
        !yvex_core_u64_add(
            transaction->generation, 1ull, &next_generation)) {
        yvex_error_set(err, YVEX_ERR_STATE,
                       "tokenizer.append.transaction.transition",
                       "staged append transition is non-contiguous");
        return YVEX_ERR_STATE;
    }
    transaction->rows[local].state = next;
    transaction->generation = next_generation;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_transaction_prepare(
    yvex_token_sequence_transaction *transaction, yvex_error *err)
{
    yvex_token_sequence *sequence = transaction ? transaction->sequence : NULL;
    if (!transaction || transaction->prepared || !transaction->count ||
        !sequence || !sequence->transaction_active ||
        sequence->count != transaction->base_count ||
        sequence->generation != transaction->base_generation ||
        transaction->count > sequence->capacity - sequence->count) {
        yvex_error_set(err, YVEX_ERR_STATE,
                       "tokenizer.append.transaction.prepare",
                       "token transaction no longer matches its base state");
        return YVEX_ERR_STATE;
    }
    transaction->prepared = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_token_sequence_transaction_publish(
    yvex_token_sequence_transaction **transaction)
{
    yvex_token_sequence_transaction *owner = transaction ? *transaction : NULL;
    yvex_token_sequence *sequence;
    if (!owner || !owner->prepared) return;
    sequence = owner->sequence;
    memcpy(sequence->rows + owner->base_count, owner->rows,
           (size_t)owner->count * sizeof(*owner->rows));
    sequence->count = owner->base_count + owner->count;
    sequence->generation = owner->generation;
    sequence->transaction_active = 0;
    free(owner->rows);
    memset(owner, 0, sizeof(*owner));
    free(owner);
    *transaction = NULL;
}

void yvex_token_sequence_transaction_abort(
    yvex_token_sequence_transaction **transaction)
{
    yvex_token_sequence_transaction *owner = transaction ? *transaction : NULL;
    if (!owner) return;
    if (owner->sequence) owner->sequence->transaction_active = 0;
    free(owner->rows);
    memset(owner, 0, sizeof(*owner));
    free(owner);
    *transaction = NULL;
}

int yvex_token_sequence_transition(
    yvex_token_sequence *sequence, unsigned long long ordinal,
    yvex_token_append_state expected, yvex_token_append_state next,
    yvex_error *err)
{
    unsigned long long next_generation;
    if (!sequence || sequence->transaction_active ||
        ordinal >= sequence->count || sequence->rows[ordinal].state != expected ||
        next != (yvex_token_append_state)(expected + 1)) {
        yvex_error_set(err, YVEX_ERR_STATE, "tokenizer.append.transition",
                       "append transition is non-contiguous or stale");
        return YVEX_ERR_STATE;
    }
    if (!yvex_core_u64_add(sequence->generation, 1u, &next_generation)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "tokenizer.append.transition",
                       "token directory generation overflow");
        return YVEX_ERR_BOUNDS;
    }
    sequence->rows[ordinal].state = next;
    sequence->generation = next_generation;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_summary_get(
    const yvex_token_sequence *sequence, yvex_token_sequence_summary *summary,
    yvex_error *err)
{
    if (!sequence || !summary) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tokenizer.append.summary",
                       "sequence and summary are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(summary, 0, sizeof(*summary));
    summary->schema_version = YVEX_TOKENIZER_APPEND_SCHEMA_V1;
    summary->count = sequence->count;
    summary->capacity = sequence->capacity;
    summary->generation = sequence->generation;
    if (!sequence_identity(sequence, summary)) {
        yvex_error_set(err, YVEX_ERR_STATE, "tokenizer.append.summary",
                       "append identity derivation failed");
        return YVEX_ERR_STATE;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_token_sequence_reset(yvex_token_sequence *sequence, yvex_error *err)
{
    unsigned long long next_generation;
    if (!sequence || sequence->transaction_active) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tokenizer.append.reset",
                       "token directory is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_core_u64_add(sequence->generation, 1u, &next_generation)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "tokenizer.append.reset",
                       "token directory generation overflow");
        return YVEX_ERR_BOUNDS;
    }
    memset(sequence->rows, 0,
           (size_t)sequence->capacity * sizeof(*sequence->rows));
    sequence->count = 0u;
    sequence->generation = next_generation;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_token_sequence_close(yvex_token_sequence **sequence)
{
    if (!sequence || !*sequence) return;
    free((*sequence)->rows);
    memset(*sequence, 0, sizeof(**sequence));
    free(*sequence);
    *sequence = NULL;
}
