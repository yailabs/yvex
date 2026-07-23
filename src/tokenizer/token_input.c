/* Owner: tokenizer.token_input.
 * Owns: bounded explicit-token parsing, vocabulary-bound validation, and indexed selection.
 * Does not own: text tokenization, prefill, decode, logits, sampling, or generation.
 * Invariants: token count never exceeds fixed capacity and validated IDs remain below vocab size.
 * Boundary: typed token-input admission consumed by runtime and CLI adapters.
 * Purpose: turn explicit operator token IDs into a bounded, validated immutable input fact.
 * Inputs: typed ID arrays or decimal list text plus an admitted vocabulary cardinality.
 * Effects: mutates only caller-owned fixed-capacity token-input structures.
 * Failure: rejects empty, malformed, overflowing, excess, or out-of-vocabulary inputs. */

#include <yvex/tokenizer.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Purpose: initialize a caller-owned token-input structure with explicit kind and capacity.
 * Inputs: nullable output and typed input kind.
 * Effects: clears prior scalar state without freeing external storage.
 * Failure: none; NULL is ignored.
 * Boundary: fixed-storage initialization. */
void yvex_token_input_init(yvex_token_input *input, yvex_token_input_kind kind)
{
    if (!input) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->kind = kind;
    input->max_tokens = YVEX_TOKEN_INPUT_MAX_TOKENS;
}

/* Purpose: map token-input kind to stable diagnostic wording.
 * Inputs: typed input-kind enumeration.
 * Effects: none; returned storage is static.
 * Failure: unrecognized values map to "unknown".
 * Boundary: label projection only. */
const char *yvex_token_input_kind_name(yvex_token_input_kind kind)
{
    switch (kind) {
    case YVEX_TOKEN_INPUT_EXPLICIT: return "explicit";
    case YVEX_TOKEN_INPUT_PROMPT_TEXT: return "prompt-text";
    }
    return "unknown";
}

/* Purpose: append one ID while enforcing both configured and physical capacities.
 * Inputs: mutable input, token ID, and error output.
 * Effects: writes one slot and increments count only on success.
 * Failure: full input returns typed bounds failure without mutation.
 * Boundary: local fixed-buffer mutation. */
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

/* Purpose: construct bounded token input from a caller-supplied ID array.
 * Inputs: kind, immutable IDs/count, output structure, and error output.
 * Effects: initializes and fills the caller-owned result.
 * Failure: empty, invalid, or excess input aborts with typed refusal.
 * Boundary: syntax-free token admission; vocabulary checking remains separate. */
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

/* Purpose: parse comma-separated decimal token IDs into fixed-capacity storage.
 * Inputs: immutable text, output structure, and error output.
 * Effects: initializes and appends each exact parsed ID.
 * Failure: syntax, numeric overflow, empty input, or capacity excess is refused.
 * Boundary: operator syntax parsing only. */
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

/* Purpose: bind parsed token IDs to an admitted vocabulary cardinality.
 * Inputs: mutable input, nonzero vocabulary size, and error output.
 * Effects: records checked/valid flags and vocabulary size after exhaustive validation.
 * Failure: empty input, absent vocabulary, or any out-of-range ID leaves valid false.
 * Boundary: token-range admission, not model execution. */
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

/* Purpose: retrieve one admitted token ID by logical input index.
 * Inputs: immutable input, index, output token, and error output.
 * Effects: writes exactly one scalar token on success.
 * Failure: invalid output or index outside token count returns typed refusal.
 * Boundary: immutable token-input lookup. */
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
