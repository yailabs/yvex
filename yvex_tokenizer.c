/*
 * YVEX - Tokenizer lifecycle
 *
 * File: yvex_tokenizer.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Builds and owns tokenizer metadata state from GGUF metadata. tokenizer layer supports
 *   vocabulary extraction for supported metadata and encode/decode only for
 *   the controlled yvex-fixture-simple tokenizer kind.
 *
 * Implements:
 *   - yvex_tokenizer_from_gguf
 *   - yvex_tokenizer_close
 *   - tokenizer kind/support accessors
 *
 * Invariants:
 *   - no external tokenizer dependency is used
 *   - real Llama/GPT2 tokenizers are metadata/vocab-only in tokenizer layer
 *   - tokenizer state owns copied vocabulary strings
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#include "yvex_tokenizer_internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char *copy_string_value(const yvex_gguf_value *value, unsigned long long *out_len)
{
    const char *data;
    unsigned long long len;
    char *copy;

    if (out_len) {
        *out_len = 0;
    }
    if (!value || yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK) {
        return NULL;
    }
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
    if (out_len) {
        *out_len = len;
    }
    return copy;
}

static yvex_tokenizer_kind kind_from_model(const char *model)
{
    if (!model) {
        return YVEX_TOKENIZER_KIND_UNKNOWN;
    }
    if (strcmp(model, "llama") == 0) return YVEX_TOKENIZER_KIND_GGML_LLAMA;
    if (strcmp(model, "gpt2") == 0) return YVEX_TOKENIZER_KIND_GGML_GPT2;
    if (strcmp(model, "replit") == 0) return YVEX_TOKENIZER_KIND_GGML_REPLIT;
    if (strcmp(model, "rwkv") == 0) return YVEX_TOKENIZER_KIND_GGML_RWKV;
    if (strcmp(model, "huggingface-json") == 0) return YVEX_TOKENIZER_KIND_HUGGINGFACE_JSON;
    if (strcmp(model, "yvex-fixture-simple") == 0) return YVEX_TOKENIZER_KIND_FIXTURE_SIMPLE;
    return YVEX_TOKENIZER_KIND_UNKNOWN;
}

static yvex_tokenizer_support support_for_kind(yvex_tokenizer_kind kind)
{
    switch (kind) {
    case YVEX_TOKENIZER_KIND_FIXTURE_SIMPLE:
        return YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE;
    case YVEX_TOKENIZER_KIND_GGML_LLAMA:
    case YVEX_TOKENIZER_KIND_GGML_GPT2:
    case YVEX_TOKENIZER_KIND_GGML_REPLIT:
    case YVEX_TOKENIZER_KIND_GGML_RWKV:
        return YVEX_TOKENIZER_SUPPORT_VOCAB_ONLY;
    case YVEX_TOKENIZER_KIND_HUGGINGFACE_JSON:
        return YVEX_TOKENIZER_SUPPORT_METADATA_ONLY;
    case YVEX_TOKENIZER_KIND_UNKNOWN:
        return YVEX_TOKENIZER_SUPPORT_UNSUPPORTED;
    }
    return YVEX_TOKENIZER_SUPPORT_UNSUPPORTED;
}

int yvex_tokenizer_from_gguf(yvex_tokenizer **out,
                             const yvex_gguf *gguf,
                             const yvex_model_descriptor *model,
                             yvex_error *err)
{
    yvex_tokenizer *tokenizer;
    const yvex_gguf_value *value;
    int rc;

    (void)model;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tokenizer_from_gguf", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!gguf) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_tokenizer_from_gguf", "gguf is required");
        return YVEX_ERR_INVALID_ARG;
    }

    tokenizer = (yvex_tokenizer *)calloc(1, sizeof(*tokenizer));
    if (!tokenizer) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_tokenizer_from_gguf", "failed to allocate tokenizer");
        return YVEX_ERR_NOMEM;
    }

    value = yvex_gguf_metadata_find(gguf, "tokenizer.ggml.model");
    tokenizer->model_name = copy_string_value(value, NULL);
    tokenizer->kind = kind_from_model(tokenizer->model_name);
    tokenizer->support = support_for_kind(tokenizer->kind);

    value = yvex_gguf_metadata_find(gguf, "tokenizer.chat_template");
    tokenizer->chat_template = copy_string_value(value, &tokenizer->chat_template_len);

    value = yvex_gguf_metadata_find(gguf, "tokenizer.huggingface.json");
    tokenizer->has_huggingface_json = value && yvex_gguf_value_type_of(value) == YVEX_GGUF_VALUE_STRING;

    rc = yvex_tokenizer_load_vocab(tokenizer, gguf, err);
    if (rc != YVEX_OK) {
        yvex_tokenizer_close(tokenizer);
        return rc;
    }

    rc = yvex_tokenizer_load_specials(tokenizer, gguf, err);
    if (rc != YVEX_OK) {
        yvex_tokenizer_close(tokenizer);
        return rc;
    }

    *out = tokenizer;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_tokenizer_close(yvex_tokenizer *tokenizer)
{
    if (!tokenizer) {
        return;
    }
    yvex_tokenizer_free_vocab(tokenizer);
    yvex_tokenizer_free_metadata(tokenizer);
    free(tokenizer);
}

yvex_tokenizer_kind yvex_tokenizer_kind_of(const yvex_tokenizer *tokenizer)
{
    return tokenizer ? tokenizer->kind : YVEX_TOKENIZER_KIND_UNKNOWN;
}

yvex_tokenizer_support yvex_tokenizer_support_of(const yvex_tokenizer *tokenizer)
{
    return tokenizer ? tokenizer->support : YVEX_TOKENIZER_SUPPORT_UNSUPPORTED;
}

const char *yvex_tokenizer_kind_name(yvex_tokenizer_kind kind)
{
    switch (kind) {
    case YVEX_TOKENIZER_KIND_UNKNOWN: return "unknown";
    case YVEX_TOKENIZER_KIND_GGML_LLAMA: return "llama";
    case YVEX_TOKENIZER_KIND_GGML_GPT2: return "gpt2";
    case YVEX_TOKENIZER_KIND_GGML_REPLIT: return "replit";
    case YVEX_TOKENIZER_KIND_GGML_RWKV: return "rwkv";
    case YVEX_TOKENIZER_KIND_HUGGINGFACE_JSON: return "huggingface-json";
    case YVEX_TOKENIZER_KIND_FIXTURE_SIMPLE: return "yvex-fixture-simple";
    }
    return "unknown";
}

const char *yvex_tokenizer_support_name(yvex_tokenizer_support support)
{
    switch (support) {
    case YVEX_TOKENIZER_SUPPORT_METADATA_ONLY: return "metadata-only";
    case YVEX_TOKENIZER_SUPPORT_VOCAB_ONLY: return "vocab-only";
    case YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE: return "fixture-encode-decode";
    case YVEX_TOKENIZER_SUPPORT_UNSUPPORTED: return "unsupported";
    }
    return "unsupported";
}

int yvex_tokenizer_chat_template(const yvex_tokenizer *tokenizer,
                                 const char **data,
                                 unsigned long long *len)
{
    if (!tokenizer || !data || !len) {
        return YVEX_ERR_INVALID_ARG;
    }
    if (!tokenizer->chat_template) {
        *data = NULL;
        *len = 0;
        return YVEX_ERR_UNSUPPORTED;
    }
    *data = tokenizer->chat_template;
    *len = tokenizer->chat_template_len;
    return YVEX_OK;
}

void yvex_tokenizer_free_metadata(yvex_tokenizer *tokenizer)
{
    if (!tokenizer) {
        return;
    }
    free(tokenizer->model_name);
    tokenizer->model_name = NULL;
    free(tokenizer->chat_template);
    tokenizer->chat_template = NULL;
    tokenizer->chat_template_len = 0;
}
