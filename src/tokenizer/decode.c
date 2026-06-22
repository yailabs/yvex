/*
 * YVEX - Fixture tokenizer decode
 *
 * File: src/tokenizer/decode.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Implements byte-safe detokenization for the controlled fixture tokenizer.
 *   Control and unused tokens are skipped; normal, user-defined, byte and unk
 *   tokens emit their stored token text.
 *
 * Implements:
 *   - yvex_detokenize_ids
 *   - yvex_tokens_free
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#include "tokenizer_internal.h"

#include <stdlib.h>
#include <string.h>

void yvex_tokens_free(yvex_tokens *tokens)
{
    if (!tokens) {
        return;
    }
    free(tokens->ids);
    yvex_tokens_clear(tokens);
}

static int token_emits_text(yvex_token_type type)
{
    return type == YVEX_TOKEN_TYPE_NORMAL ||
           type == YVEX_TOKEN_TYPE_UNK ||
           type == YVEX_TOKEN_TYPE_USER_DEFINED ||
           type == YVEX_TOKEN_TYPE_BYTE;
}

int yvex_detokenize_ids(const yvex_tokenizer *tokenizer,
                        const unsigned int *ids,
                        unsigned long long len,
                        char *out,
                        unsigned long long cap,
                        yvex_error *err)
{
    unsigned long long i;
    unsigned long long used = 0;

    if (!tokenizer || (!ids && len > 0) || !out || cap == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_detokenize_ids", "tokenizer, ids and output buffer are required");
        return YVEX_ERR_INVALID_ARG;
    }
    out[0] = '\0';

    if (tokenizer->support != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_detokenize_ids",
                        "tokenizer kind %s is not executable in E0",
                        yvex_tokenizer_kind_name(tokenizer->kind));
        return YVEX_ERR_UNSUPPORTED;
    }

    for (i = 0; i < len; ++i) {
        const yvex_token_info *token = yvex_tokenizer_token_at(tokenizer, ids[i]);
        if (!token) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_detokenize_ids",
                            "token id %u is outside vocabulary", ids[i]);
            return YVEX_ERR_BOUNDS;
        }
        if (!token_emits_text(token->type)) {
            continue;
        }
        if (token->text_len > cap - used - 1u) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_detokenize_ids", "output buffer too small");
            return YVEX_ERR_BOUNDS;
        }
        if (token->text_len > 0) {
            memcpy(out + used, token->text, (size_t)token->text_len);
        }
        used += token->text_len;
        out[used] = '\0';
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
