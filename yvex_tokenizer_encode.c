/*
 * YVEX - Fixture tokenizer encode
 *
 * File: yvex_tokenizer_encode.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Implements deterministic greedy longest-prefix tokenization for the
 *   controlled yvex-fixture-simple tokenizer. Other tokenizer kinds return
 *   explicit unsupported status in tokenizer layer.
 *
 * Implements:
 *   - yvex_tokenize_text
 *   - yvex_tokens_clear
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#include "yvex_tokenizer_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void yvex_tokens_clear(yvex_tokens *tokens)
{
    if (!tokens) {
        return;
    }
    tokens->ids = NULL;
    tokens->len = 0;
    tokens->cap = 0;
}

static int tokens_append(yvex_tokens *tokens, unsigned int id, yvex_error *err)
{
    unsigned int *next;
    unsigned long long next_cap;

    if (tokens->len == tokens->cap) {
        next_cap = tokens->cap == 0 ? 8 : tokens->cap * 2u;
        if (next_cap < tokens->cap || next_cap > (unsigned long long)(SIZE_MAX / sizeof(*tokens->ids))) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenize_text", "token buffer too large");
            return YVEX_ERR_NOMEM;
        }
        next = (unsigned int *)realloc(tokens->ids, (size_t)next_cap * sizeof(*tokens->ids));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenize_text", "failed to grow token buffer");
            return YVEX_ERR_NOMEM;
        }
        tokens->ids = next;
        tokens->cap = next_cap;
    }

    tokens->ids[tokens->len++] = id;
    return YVEX_OK;
}

static const yvex_token_info *find_longest_match(const yvex_tokenizer *tokenizer,
                                                 const char *text,
                                                 unsigned long long remaining)
{
    const yvex_token_info *best = NULL;
    unsigned long long i;

    for (i = 0; i < tokenizer->vocab_size; ++i) {
        const yvex_token_info *token = &tokenizer->tokens[i];
        if (token->type == YVEX_TOKEN_TYPE_CONTROL || token->type == YVEX_TOKEN_TYPE_UNUSED) {
            continue;
        }
        if (token->text_len == 0 || token->text_len > remaining) {
            continue;
        }
        if (memcmp(text, token->text, (size_t)token->text_len) == 0 &&
            (!best || token->text_len > best->text_len)) {
            best = token;
        }
    }

    return best;
}

int yvex_tokenize_text(const yvex_tokenizer *tokenizer,
                       const char *text,
                       yvex_tokens *out,
                       yvex_error *err)
{
    unsigned long long len;
    unsigned long long offset = 0;
    unsigned int unk_id;
    int has_unk;
    int rc;

    if (!tokenizer || !text || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tokenize_text", "tokenizer, text and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_tokens_clear(out);

    if (tokenizer->support != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_tokenize_text",
                        "tokenizer kind %s is not executable in tokenizer layer",
                        yvex_tokenizer_kind_name(tokenizer->kind));
        return YVEX_ERR_UNSUPPORTED;
    }

    len = (unsigned long long)strlen(text);
    has_unk = yvex_tokenizer_unk_id(tokenizer, &unk_id) == YVEX_OK;

    while (offset < len) {
        const yvex_token_info *match = find_longest_match(tokenizer, text + offset, len - offset);
        if (match) {
            rc = tokens_append(out, match->id, err);
            if (rc != YVEX_OK) {
                yvex_tokens_free(out);
                return rc;
            }
            offset += match->text_len;
        } else if (has_unk) {
            rc = tokens_append(out, unk_id, err);
            if (rc != YVEX_OK) {
                yvex_tokens_free(out);
                return rc;
            }
            offset += 1;
        } else {
            yvex_tokens_free(out);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_tokenize_text", "no token matched and no unknown token exists");
            return YVEX_ERR_UNSUPPORTED;
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
