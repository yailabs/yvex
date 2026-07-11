/*
 * yvex_source_deepseek.h - DeepSeek source-sidecar fact extraction.
 *
 * Owner: src/source.
 * Owns: structured raw config, tokenizer, and generation sidecar facts.
 * Does not own: file discovery, provenance, architecture IR, inventory, or rendering.
 * Invariants: known fields are exact, duplicate-safe, and never defaulted.
 * Boundary: raw configuration facts are not an executable model descriptor.
 */
#ifndef YVEX_SOURCE_DEEPSEEK_H
#define YVEX_SOURCE_DEEPSEEK_H

#include "yvex_source_verify.h"

typedef enum {
    YVEX_SOURCE_DEEPSEEK_CONFIG = 0,
    YVEX_SOURCE_DEEPSEEK_TOKENIZER,
    YVEX_SOURCE_DEEPSEEK_TOKENIZER_CONFIG,
    YVEX_SOURCE_DEEPSEEK_GENERATION_CONFIG
} yvex_source_deepseek_sidecar_kind;

int yvex_source_deepseek_parse_sidecar(
    yvex_source_deepseek_sidecar_kind kind,
    const char *data,
    size_t length,
    const yvex_model_target_identity *identity,
    yvex_source_verification *out);

#endif
