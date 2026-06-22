/*
 * YVEX - Prompt renderer
 *
 * File: src/tokenizer/prompt.c
 * Layer: tokenizer implementation
 *
 * Purpose:
 *   Implements the bounded E0 default prompt renderer for explicit role
 *   messages. This module does not execute arbitrary Jinja chat templates.
 *
 * Implements:
 *   - yvex_prompt_role_name
 *   - yvex_prompt_render
 *   - yvex_rendered_prompt_free
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_prompt
 */
#include <yvex/prompt.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *data;
    unsigned long long len;
    unsigned long long cap;
} prompt_builder;

const char *yvex_prompt_role_name(yvex_prompt_role role)
{
    switch (role) {
    case YVEX_PROMPT_ROLE_SYSTEM: return "system";
    case YVEX_PROMPT_ROLE_USER: return "user";
    case YVEX_PROMPT_ROLE_ASSISTANT: return "assistant";
    case YVEX_PROMPT_ROLE_TOOL: return "tool";
    }
    return "unknown";
}

static int builder_reserve(prompt_builder *builder, unsigned long long add, yvex_error *err)
{
    char *next;
    unsigned long long need;
    unsigned long long cap;

    if (builder->len > ULLONG_MAX - add - 1u) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_prompt_render", "prompt length overflow");
        return YVEX_ERR_BOUNDS;
    }
    need = builder->len + add + 1u;
    if (need <= builder->cap) {
        return YVEX_OK;
    }

    cap = builder->cap == 0 ? 128 : builder->cap;
    while (cap < need) {
        if (cap > ULLONG_MAX / 2u) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_prompt_render", "prompt capacity overflow");
            return YVEX_ERR_BOUNDS;
        }
        cap *= 2u;
    }
    if (cap > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_prompt_render", "prompt too large to allocate");
        return YVEX_ERR_NOMEM;
    }

    next = (char *)realloc(builder->data, (size_t)cap);
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_prompt_render", "failed to grow prompt buffer");
        return YVEX_ERR_NOMEM;
    }
    builder->data = next;
    builder->cap = cap;
    return YVEX_OK;
}

static int builder_append(prompt_builder *builder, const char *text, yvex_error *err)
{
    unsigned long long len;
    int rc;

    if (!text) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_prompt_render", "message content is null");
        return YVEX_ERR_INVALID_ARG;
    }
    len = (unsigned long long)strlen(text);
    rc = builder_reserve(builder, len, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (len > 0) {
        memcpy(builder->data + builder->len, text, (size_t)len);
    }
    builder->len += len;
    builder->data[builder->len] = '\0';
    return YVEX_OK;
}

int yvex_prompt_render(yvex_rendered_prompt *out,
                       const yvex_tokenizer *tokenizer,
                       const yvex_prompt_message *messages,
                       unsigned long long message_count,
                       const yvex_prompt_options *options,
                       yvex_error *err)
{
    yvex_prompt_options defaults;
    prompt_builder builder;
    unsigned long long i;
    int rc;

    (void)tokenizer;

    if (!out || !messages || message_count == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_prompt_render", "messages are required");
        return YVEX_ERR_INVALID_ARG;
    }

    out->text = NULL;
    out->len = 0;
    defaults.add_bos = 0;
    defaults.add_eos = 0;
    defaults.add_generation_prompt = 1;
    if (!options) {
        options = &defaults;
    }

    memset(&builder, 0, sizeof(builder));

    for (i = 0; i < message_count; ++i) {
        const char *role = yvex_prompt_role_name(messages[i].role);
        rc = builder_append(&builder, "<", err);
        if (rc == YVEX_OK) rc = builder_append(&builder, role, err);
        if (rc == YVEX_OK) rc = builder_append(&builder, ">\n", err);
        if (rc == YVEX_OK) rc = builder_append(&builder, messages[i].content, err);
        if (rc == YVEX_OK) rc = builder_append(&builder, "\n</", err);
        if (rc == YVEX_OK) rc = builder_append(&builder, role, err);
        if (rc == YVEX_OK) rc = builder_append(&builder, ">\n", err);
        if (rc != YVEX_OK) {
            free(builder.data);
            return rc;
        }
    }

    if (options->add_generation_prompt) {
        rc = builder_append(&builder, "<assistant>\n", err);
        if (rc != YVEX_OK) {
            free(builder.data);
            return rc;
        }
    }

    out->text = builder.data;
    out->len = builder.len;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_rendered_prompt_free(yvex_rendered_prompt *prompt)
{
    if (!prompt) {
        return;
    }
    free(prompt->text);
    prompt->text = NULL;
    prompt->len = 0;
}
