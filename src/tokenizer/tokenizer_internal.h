/*
 * YVEX - tokenizer internal state
 *
 * File: src/tokenizer/tokenizer_internal.h
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Shares the private tokenizer object layout across E0 tokenizer modules.
 *   This header is internal to src/tokenizer and is not installed as public API.
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#ifndef YVEX_TOKENIZER_INTERNAL_H
#define YVEX_TOKENIZER_INTERNAL_H

#include <yvex/tokenizer.h>

typedef struct {
    int present;
    unsigned int id;
} yvex_tokenizer_special_id;

struct yvex_tokenizer {
    yvex_tokenizer_kind kind;
    yvex_tokenizer_support support;
    char *model_name;
    yvex_token_info *tokens;
    unsigned long long vocab_size;
    yvex_tokenizer_special_id bos;
    yvex_tokenizer_special_id eos;
    yvex_tokenizer_special_id unk;
    yvex_tokenizer_special_id pad;
    yvex_tokenizer_special_id sep;
    char *chat_template;
    unsigned long long chat_template_len;
    int has_huggingface_json;
};

int yvex_tokenizer_load_vocab(yvex_tokenizer *tokenizer, const yvex_gguf *gguf, yvex_error *err);
void yvex_tokenizer_free_vocab(yvex_tokenizer *tokenizer);
int yvex_tokenizer_load_specials(yvex_tokenizer *tokenizer, const yvex_gguf *gguf, yvex_error *err);
void yvex_tokenizer_free_metadata(yvex_tokenizer *tokenizer);

#endif /* YVEX_TOKENIZER_INTERNAL_H */
