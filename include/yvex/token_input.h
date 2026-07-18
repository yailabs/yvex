/*
 * Owner: abi.token_input (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Token input boundary
 *
 * File: include/yvex/token_input.h
 * Layer: public runtime input API
 *
 * Purpose:
 *   Defines a bounded token input object for explicit token sequences and
 *   prompt-derived token sequences before prefill exists.
 *
 * Owns:
 *   - yvex_token_input
 *   - explicit token list parsing
 *   - token bounds validation
 *   - selected-token lookup by input index
 *
 * Does not own:
 *   - tokenizer algorithms
 *   - prefill/KV/decode/logits
 *   - graph execution
 *   - generation
 */
#ifndef YVEX_TOKEN_INPUT_H
#define YVEX_TOKEN_INPUT_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_TOKEN_INPUT_MAX_TOKENS 1024ull

typedef enum {
    YVEX_TOKEN_INPUT_EXPLICIT = 1,
    YVEX_TOKEN_INPUT_PROMPT_TEXT = 2
} yvex_token_input_kind;

typedef struct {
    yvex_token_input_kind kind;
    unsigned long long token_count;
    unsigned int tokens[YVEX_TOKEN_INPUT_MAX_TOKENS];
    unsigned long long max_tokens;
    int token_bounds_checked;
    int token_bounds_valid;
    unsigned long long vocab_size;
} yvex_token_input;

void yvex_token_input_init(yvex_token_input *input, yvex_token_input_kind kind);

const char *yvex_token_input_kind_name(yvex_token_input_kind kind);

int yvex_token_input_parse_explicit(const char *text,
                                    yvex_token_input *out,
                                    yvex_error *err);

int yvex_token_input_from_ids(yvex_token_input_kind kind,
                              const unsigned int *ids,
                              unsigned long long count,
                              yvex_token_input *out,
                              yvex_error *err);

int yvex_token_input_validate_bounds(yvex_token_input *input,
                                     unsigned long long vocab_size,
                                     yvex_error *err);

int yvex_token_input_select(const yvex_token_input *input,
                            unsigned long long token_index,
                            unsigned int *out_token,
                            yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_TOKEN_INPUT_H */
