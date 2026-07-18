/*
 * tokenizer_metadata.h - bounded artifact tokenizer projection.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: exact verified tokenizer sidecar decoding into GGUF token, token-type,
 *   merge, special-token, and embedded-source metadata views.
 * Does not own: source verification, tokenizer execution, GGUF serialization,
 *   model architecture, artifact admission, runtime, or generation.
 * Invariants: views are immutable, ID-indexed, duplicate-free, bounded, and
 *   tied to provider Git blob identities at the pinned source revision.
 * Boundary: complete tokenizer material is artifact input, not tokenizer
 *   execution or generation support.
 */
#ifndef YVEX_GGUF_TOKENIZER_METADATA_H
#define YVEX_GGUF_TOKENIZER_METADATA_H

#include "src/source/provenance.h"

#define YVEX_GGUF_TOKENIZER_SHA256_CAP 65u

typedef enum {
    YVEX_GGUF_TOKENIZER_OK = 0,
    YVEX_GGUF_TOKENIZER_INVALID_ARGUMENT,
    YVEX_GGUF_TOKENIZER_SOURCE_IDENTITY,
    YVEX_GGUF_TOKENIZER_RESOURCE_LIMIT,
    YVEX_GGUF_TOKENIZER_ALLOCATION,
    YVEX_GGUF_TOKENIZER_MALFORMED_JSON,
    YVEX_GGUF_TOKENIZER_UNSUPPORTED_JSON,
    YVEX_GGUF_TOKENIZER_DUPLICATE_TOKEN_ID,
    YVEX_GGUF_TOKENIZER_MISSING_TOKEN_ID,
    YVEX_GGUF_TOKENIZER_TOKEN_MISMATCH,
    YVEX_GGUF_TOKENIZER_CARDINALITY,
    YVEX_GGUF_TOKENIZER_SPECIAL_TOKEN
} yvex_gguf_tokenizer_code;

typedef struct {
    yvex_gguf_tokenizer_code code;
    unsigned long long record_index;
    unsigned long long expected;
    unsigned long long actual;
    char field[64];
} yvex_gguf_tokenizer_failure;

typedef struct {
    unsigned long long tokenizer_json_bytes;
    unsigned long long tokenizer_config_bytes;
    unsigned long long token_count;
    unsigned long long merge_count;
    unsigned long long added_token_count;
    unsigned long long owned_bytes;
    unsigned long long decoded_string_bytes;
    unsigned int bos_token_id;
    unsigned int eos_token_id;
    unsigned int pad_token_id;
    int add_bos_token;
    int add_eos_token;
    int chat_template_present;
    char pre_tokenizer[32];
    char tokenizer_json_sha256[YVEX_GGUF_TOKENIZER_SHA256_CAP];
    char tokenizer_config_sha256[YVEX_GGUF_TOKENIZER_SHA256_CAP];
    char tokenizer_json_git_oid[41];
    char tokenizer_config_git_oid[41];
    int complete;
} yvex_gguf_tokenizer_summary;

typedef struct yvex_gguf_tokenizer_metadata yvex_gguf_tokenizer_metadata;

int yvex_gguf_tokenizer_metadata_load(
    yvex_gguf_tokenizer_metadata **out,
    const yvex_source_verification *verification,
    unsigned long long expected_vocab_size,
    const char *pre_tokenizer,
    size_t maximum_owned_bytes,
    yvex_gguf_tokenizer_failure *failure,
    yvex_error *err);
void yvex_gguf_tokenizer_metadata_release(
    yvex_gguf_tokenizer_metadata **metadata);

const yvex_gguf_tokenizer_summary *yvex_gguf_tokenizer_summary_get(
    const yvex_gguf_tokenizer_metadata *metadata);
int yvex_gguf_tokenizer_token_at(
    const yvex_gguf_tokenizer_metadata *metadata,
    unsigned long long index,
    const unsigned char **bytes,
    size_t *byte_count,
    int *token_type);
int yvex_gguf_tokenizer_merge_at(
    const yvex_gguf_tokenizer_metadata *metadata,
    unsigned long long index,
    const unsigned char **bytes,
    size_t *byte_count);
int yvex_gguf_tokenizer_raw_json(
    const yvex_gguf_tokenizer_metadata *metadata,
    const unsigned char **bytes,
    size_t *byte_count);
int yvex_gguf_tokenizer_raw_config(
    const yvex_gguf_tokenizer_metadata *metadata,
    const unsigned char **bytes,
    size_t *byte_count);
const char *yvex_gguf_tokenizer_code_name(yvex_gguf_tokenizer_code code);

#endif
