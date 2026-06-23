/*
 * YVEX - Tokenizer vocabulary
 *
 * File: yvex_tokenizer_vocab.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Extracts tokenizer vocabulary records from GGUF metadata arrays. This
 *   module validates token, score, and token-type array counts and copies all
 *   token strings into owned tokenizer state.
 *
 * Implements:
 *   - yvex_tokenizer_load_vocab
 *   - yvex_tokenizer_vocab_size
 *   - yvex_tokenizer_token_at
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#include "yvex_tokenizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int read_array_info(const yvex_gguf_value *value,
                           yvex_gguf_value_type expected,
                           yvex_gguf_array_info *out,
                           yvex_error *err,
                           const char *where,
                           const char *name)
{
    if (!value || yvex_gguf_value_array_info(value, out) != YVEX_OK) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, where, "%s must be an array", name);
        return YVEX_ERR_FORMAT;
    }
    if (out->element_type != expected) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, where, "%s has wrong element type", name);
        return YVEX_ERR_FORMAT;
    }
    return YVEX_OK;
}

static char *copy_text(const char *data, unsigned long long len)
{
    char *copy;

    if (len > (unsigned long long)(SIZE_MAX - 1)) {
        return NULL;
    }
    copy = (char *)malloc((size_t)len + 1u);
    if (!copy) {
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, data, (size_t)len);
    }
    copy[len] = '\0';
    return copy;
}

int yvex_tokenizer_load_vocab(yvex_tokenizer *tokenizer, const yvex_gguf *gguf, yvex_error *err)
{
    const yvex_gguf_value *tokens_value;
    const yvex_gguf_value *scores_value;
    const yvex_gguf_value *types_value;
    yvex_gguf_array_info tokens_info;
    yvex_gguf_array_info scores_info;
    yvex_gguf_array_info types_info;
    unsigned long long i;
    int has_scores = 0;
    int has_types = 0;
    int rc;

    if (!tokenizer || !gguf) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tokenizer_load_vocab", "tokenizer and gguf are required");
        return YVEX_ERR_INVALID_ARG;
    }

    tokens_value = yvex_gguf_metadata_find(gguf, "tokenizer.ggml.tokens");
    if (!tokens_value) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_tokenizer_load_vocab", "tokenizer.ggml.tokens is missing");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = read_array_info(tokens_value, YVEX_GGUF_VALUE_STRING, &tokens_info, err,
                         "yvex_tokenizer_load_vocab", "tokenizer.ggml.tokens");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (tokens_info.count == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "tokenizer vocabulary is empty");
        return YVEX_ERR_FORMAT;
    }
    if (tokens_info.count > (unsigned long long)(SIZE_MAX / sizeof(*tokenizer->tokens))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenizer_load_vocab", "vocabulary too large");
        return YVEX_ERR_NOMEM;
    }

    scores_value = yvex_gguf_metadata_find(gguf, "tokenizer.ggml.scores");
    if (scores_value) {
        rc = read_array_info(scores_value, YVEX_GGUF_VALUE_FLOAT32, &scores_info, err,
                             "yvex_tokenizer_load_vocab", "tokenizer.ggml.scores");
        if (rc != YVEX_OK) {
            return rc;
        }
        if (scores_info.count != tokens_info.count) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "tokenizer score count does not match tokens");
            return YVEX_ERR_FORMAT;
        }
        has_scores = 1;
    }

    types_value = yvex_gguf_metadata_find(gguf, "tokenizer.ggml.token_type");
    if (types_value) {
        rc = read_array_info(types_value, YVEX_GGUF_VALUE_INT32, &types_info, err,
                             "yvex_tokenizer_load_vocab", "tokenizer.ggml.token_type");
        if (rc != YVEX_OK) {
            return rc;
        }
        if (types_info.count != tokens_info.count) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "token type count does not match tokens");
            return YVEX_ERR_FORMAT;
        }
        has_types = 1;
    }

    tokenizer->tokens = (yvex_token_info *)calloc((size_t)tokens_info.count, sizeof(*tokenizer->tokens));
    if (!tokenizer->tokens) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenizer_load_vocab", "failed to allocate vocabulary");
        return YVEX_ERR_NOMEM;
    }
    tokenizer->vocab_size = tokens_info.count;

    for (i = 0; i < tokens_info.count; ++i) {
        const yvex_gguf_value *item = yvex_gguf_value_array_at(tokens_value, i);
        const char *text;
        unsigned long long len;
        yvex_token_info *token = &tokenizer->tokens[i];

        if (!item || yvex_gguf_value_as_string(item, &text, &len) != YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "token entry is not a string");
            return YVEX_ERR_FORMAT;
        }
        token->text = copy_text(text, len);
        if (!token->text) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenizer_load_vocab", "failed to copy token text");
            return YVEX_ERR_NOMEM;
        }
        token->id = (unsigned int)i;
        token->text_len = len;
        token->score = 0.0f;
        token->type = YVEX_TOKEN_TYPE_NORMAL;

        if (has_scores) {
            double score;
            item = yvex_gguf_value_array_at(scores_value, i);
            if (!item || yvex_gguf_value_as_f64(item, &score) != YVEX_OK) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "score entry is invalid");
                return YVEX_ERR_FORMAT;
            }
            token->score = (float)score;
        }

        if (has_types) {
            long long type;
            item = yvex_gguf_value_array_at(types_value, i);
            if (!item || yvex_gguf_value_as_i64(item, &type) != YVEX_OK ||
                type < YVEX_TOKEN_TYPE_NORMAL || type > YVEX_TOKEN_TYPE_BYTE) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_vocab", "token type entry is invalid");
                return YVEX_ERR_FORMAT;
            }
            token->type = (yvex_token_type)type;
        }
    }

    return YVEX_OK;
}

void yvex_tokenizer_free_vocab(yvex_tokenizer *tokenizer)
{
    unsigned long long i;

    if (!tokenizer || !tokenizer->tokens) {
        return;
    }
    for (i = 0; i < tokenizer->vocab_size; ++i) {
        free((char *)tokenizer->tokens[i].text);
        tokenizer->tokens[i].text = NULL;
    }
    free(tokenizer->tokens);
    tokenizer->tokens = NULL;
    tokenizer->vocab_size = 0;
}

unsigned long long yvex_tokenizer_vocab_size(const yvex_tokenizer *tokenizer)
{
    return tokenizer ? tokenizer->vocab_size : 0;
}

const yvex_token_info *yvex_tokenizer_token_at(const yvex_tokenizer *tokenizer,
                                               unsigned long long id)
{
    if (!tokenizer || id >= tokenizer->vocab_size) {
        return NULL;
    }
    return &tokenizer->tokens[id];
}
