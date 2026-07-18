/*
 * Owner: abi.tokenizer (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Tokenizer metadata and fixture tokenizer
 *
 * File: include/yvex/tokenizer.h
 * Layer: public tokenizer API
 *
 * Purpose:
 *   Defines tokenizer metadata, vocabulary, special-token, and controlled
 *   fixture encode/decode APIs built from GGUF metadata. This header does not
 *   claim generic SentencePiece, GPT-2 BPE, HuggingFace tokenizer execution,
 *   model execution, or backend support.
 *
 * Owns:
 *   - yvex_tokenizer
 *   - yvex_token_info
 *   - yvex_tokens
 *   - tokenizer metadata and special-token accessors
 *   - fixture tokenizer encode/decode helpers
 *
 * Does not own:
 *   - arbitrary real-world tokenizer algorithms
 *   - prompt rendering
 *   - model execution
 *   - backend/session state
 *
 * Used by:
 *   - prompt renderer
 *   - yvex CLI
 *   - tokenizer tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_tokenizer
 */
#ifndef YVEX_TOKENIZER_H
#define YVEX_TOKENIZER_H

#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_tokenizer yvex_tokenizer;

typedef enum {
    YVEX_TOKENIZER_KIND_UNKNOWN = 0,
    YVEX_TOKENIZER_KIND_GGML_LLAMA,
    YVEX_TOKENIZER_KIND_GGML_GPT2,
    YVEX_TOKENIZER_KIND_GGML_REPLIT,
    YVEX_TOKENIZER_KIND_GGML_RWKV,
    YVEX_TOKENIZER_KIND_HUGGINGFACE_JSON,
    YVEX_TOKENIZER_KIND_FIXTURE_SIMPLE
} yvex_tokenizer_kind;

typedef enum {
    YVEX_TOKENIZER_SUPPORT_METADATA_ONLY = 0,
    YVEX_TOKENIZER_SUPPORT_VOCAB_ONLY,
    YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE,
    YVEX_TOKENIZER_SUPPORT_UNSUPPORTED
} yvex_tokenizer_support;

typedef enum {
    YVEX_TOKEN_TYPE_UNKNOWN = 0,
    YVEX_TOKEN_TYPE_NORMAL = 1,
    YVEX_TOKEN_TYPE_UNK = 2,
    YVEX_TOKEN_TYPE_CONTROL = 3,
    YVEX_TOKEN_TYPE_USER_DEFINED = 4,
    YVEX_TOKEN_TYPE_UNUSED = 5,
    YVEX_TOKEN_TYPE_BYTE = 6
} yvex_token_type;

typedef struct {
    unsigned int id;
    const char *text;
    unsigned long long text_len;
    float score;
    yvex_token_type type;
} yvex_token_info;

typedef struct {
    unsigned int *ids;
    unsigned long long len;
    unsigned long long cap;
} yvex_tokens;

int yvex_tokenizer_from_gguf(yvex_tokenizer **out,
                             const yvex_gguf *gguf,
                             const yvex_model_descriptor *model,
                             yvex_error *err);

void yvex_tokenizer_close(yvex_tokenizer *tokenizer);

yvex_tokenizer_kind yvex_tokenizer_kind_of(const yvex_tokenizer *tokenizer);
yvex_tokenizer_support yvex_tokenizer_support_of(const yvex_tokenizer *tokenizer);
const char *yvex_tokenizer_kind_name(yvex_tokenizer_kind kind);
const char *yvex_tokenizer_support_name(yvex_tokenizer_support support);

unsigned long long yvex_tokenizer_vocab_size(const yvex_tokenizer *tokenizer);
const yvex_token_info *yvex_tokenizer_token_at(const yvex_tokenizer *tokenizer,
                                               unsigned long long id);

int yvex_tokenizer_bos_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_eos_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_unk_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_pad_id(const yvex_tokenizer *tokenizer, unsigned int *out);
int yvex_tokenizer_sep_id(const yvex_tokenizer *tokenizer, unsigned int *out);

int yvex_tokenizer_chat_template(const yvex_tokenizer *tokenizer,
                                 const char **data,
                                 unsigned long long *len);

int yvex_tokenize_text(const yvex_tokenizer *tokenizer,
                       const char *text,
                       yvex_tokens *out,
                       yvex_error *err);

int yvex_detokenize_ids(const yvex_tokenizer *tokenizer,
                        const unsigned int *ids,
                        unsigned long long len,
                        char *out,
                        unsigned long long cap,
                        yvex_error *err);

void yvex_tokens_clear(yvex_tokens *tokens);
void yvex_tokens_free(yvex_tokens *tokens);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_TOKENIZER_H */
