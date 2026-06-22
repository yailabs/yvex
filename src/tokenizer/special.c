/*
 * YVEX - Tokenizer special tokens
 *
 * File: src/tokenizer/special.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Extracts and validates tokenizer special-token IDs from GGUF metadata and
 *   exposes stable accessors for callers.
 *
 * Implements:
 *   - yvex_tokenizer_load_specials
 *   - yvex_tokenizer_bos_id
 *   - yvex_tokenizer_eos_id
 *   - yvex_tokenizer_unk_id
 *   - yvex_tokenizer_pad_id
 *   - yvex_tokenizer_sep_id
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#include "tokenizer_internal.h"

static int load_special(const yvex_gguf *gguf,
                        const char *key,
                        yvex_tokenizer_special_id *slot,
                        unsigned long long vocab_size,
                        yvex_error *err)
{
    const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, key);
    unsigned long long id;
    int rc;

    slot->present = 0;
    slot->id = 0;
    if (!value) {
        return YVEX_OK;
    }

    rc = yvex_gguf_value_as_u64(value, &id);
    if (rc != YVEX_OK) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_tokenizer_load_specials",
                        "%s must be an unsigned integer", key);
        return YVEX_ERR_FORMAT;
    }
    if (id >= vocab_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_tokenizer_load_specials",
                        "%s value %llu is outside vocab size %llu", key, id, vocab_size);
        return YVEX_ERR_BOUNDS;
    }

    slot->present = 1;
    slot->id = (unsigned int)id;
    return YVEX_OK;
}

int yvex_tokenizer_load_specials(yvex_tokenizer *tokenizer, const yvex_gguf *gguf, yvex_error *err)
{
    int rc;

    rc = load_special(gguf, "tokenizer.ggml.bos_token_id", &tokenizer->bos, tokenizer->vocab_size, err);
    if (rc != YVEX_OK) return rc;
    rc = load_special(gguf, "tokenizer.ggml.eos_token_id", &tokenizer->eos, tokenizer->vocab_size, err);
    if (rc != YVEX_OK) return rc;
    rc = load_special(gguf, "tokenizer.ggml.unknown_token_id", &tokenizer->unk, tokenizer->vocab_size, err);
    if (rc != YVEX_OK) return rc;
    rc = load_special(gguf, "tokenizer.ggml.padding_token_id", &tokenizer->pad, tokenizer->vocab_size, err);
    if (rc != YVEX_OK) return rc;
    return load_special(gguf, "tokenizer.ggml.separator_token_id", &tokenizer->sep, tokenizer->vocab_size, err);
}

static int special_id_get(const yvex_tokenizer *tokenizer,
                          const yvex_tokenizer_special_id *slot,
                          unsigned int *out)
{
    if (!tokenizer || !slot || !out) {
        return YVEX_ERR_INVALID_ARG;
    }
    if (!slot->present) {
        return YVEX_ERR_UNSUPPORTED;
    }
    *out = slot->id;
    return YVEX_OK;
}

int yvex_tokenizer_bos_id(const yvex_tokenizer *tokenizer, unsigned int *out)
{
    return special_id_get(tokenizer, tokenizer ? &tokenizer->bos : 0, out);
}

int yvex_tokenizer_eos_id(const yvex_tokenizer *tokenizer, unsigned int *out)
{
    return special_id_get(tokenizer, tokenizer ? &tokenizer->eos : 0, out);
}

int yvex_tokenizer_unk_id(const yvex_tokenizer *tokenizer, unsigned int *out)
{
    return special_id_get(tokenizer, tokenizer ? &tokenizer->unk : 0, out);
}

int yvex_tokenizer_pad_id(const yvex_tokenizer *tokenizer, unsigned int *out)
{
    return special_id_get(tokenizer, tokenizer ? &tokenizer->pad : 0, out);
}

int yvex_tokenizer_sep_id(const yvex_tokenizer *tokenizer, unsigned int *out)
{
    return special_id_get(tokenizer, tokenizer ? &tokenizer->sep : 0, out);
}
