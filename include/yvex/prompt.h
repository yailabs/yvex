/*
 * YVEX - Prompt rendering
 *
 * File: include/yvex/prompt.h
 * Layer: public prompt API
 *
 * Purpose:
 *   Defines bounded message and prompt rendering helpers for E0. The renderer
 *   uses a deterministic YVEX default format and does not execute arbitrary
 *   Jinja templates.
 *
 * Owns:
 *   - yvex_prompt_role
 *   - yvex_prompt_message
 *   - yvex_prompt_options
 *   - yvex_rendered_prompt
 *   - yvex_prompt_render
 *
 * Does not own:
 *   - tokenizer algorithms
 *   - session state
 *   - model execution
 *   - chat runtime
 *
 * Used by:
 *   - yvex CLI
 *   - prompt tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_prompt
 */
#ifndef YVEX_PROMPT_H
#define YVEX_PROMPT_H

#include <yvex/error.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_PROMPT_ROLE_SYSTEM = 0,
    YVEX_PROMPT_ROLE_USER,
    YVEX_PROMPT_ROLE_ASSISTANT,
    YVEX_PROMPT_ROLE_TOOL
} yvex_prompt_role;

typedef struct {
    yvex_prompt_role role;
    const char *content;
} yvex_prompt_message;

typedef struct {
    int add_bos;
    int add_eos;
    int add_generation_prompt;
} yvex_prompt_options;

typedef struct {
    char *text;
    unsigned long long len;
} yvex_rendered_prompt;

const char *yvex_prompt_role_name(yvex_prompt_role role);

int yvex_prompt_render(yvex_rendered_prompt *out,
                       const yvex_tokenizer *tokenizer,
                       const yvex_prompt_message *messages,
                       unsigned long long message_count,
                       const yvex_prompt_options *options,
                       yvex_error *err);

void yvex_rendered_prompt_free(yvex_rendered_prompt *prompt);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_PROMPT_H */
