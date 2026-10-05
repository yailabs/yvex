/* Test transport for exact upstream prompt/token references; no model execution. */
#include <yvex/api.h>
#include <yvex/internal/runtime.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int authority_refusals(const yvex_gguf *gguf,
                               const yvex_tokenizer_family_policy *admitted,
                               yvex_error *err)
{
    yvex_conversation_protocol authority;
    yvex_tokenizer_family_policy changed;
    unsigned int index;
    for (index = 0u; index < 3u; ++index) {
        yvex_tokenizer *tokenizer = NULL;
        const yvex_tokenizer_plan_summary *plan;
        int rc;
        if (!yvex_tokenizer_family_policy_conversation(admitted, &authority))
            return YVEX_ERR_FORMAT;
        if (index == 0u) authority.source_revision = "wrong-pinned-revision";
        else if (index == 1u) authority.source_encoding_identity =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        else authority.tokenizer_config_identity =
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        rc = yvex_tokenizer_family_policy_compile(&changed, &authority,
            admitted->tokenizer_kind, admitted->model_policy,
            admitted->prompt_policy, err);
        if (rc == YVEX_OK)
            rc = yvex_tokenizer_from_compiled_gguf(&tokenizer, gguf, &changed, err);
        plan = yvex_tokenizer_plan_summary_get(tokenizer);
        if (rc == YVEX_OK && plan && plan->sealed) {
            yvex_tokenizer_close(tokenizer);
            return YVEX_ERR_STATE;
        }
        yvex_tokenizer_close(tokenizer);
    }
    changed = *admitted;
    changed.text[changed.text_offsets[YVEX_TOKENIZER_POLICY_SOURCE_ENCODING_IDENTITY]] ^= 1;
    if (yvex_tokenizer_family_policy_validate(&changed, err) != YVEX_ERR_FORMAT)
        return YVEX_ERR_STATE;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int render(const yvex_tokenizer *tokenizer, int argc, char **argv,
                   yvex_error *err)
{
    yvex_prompt_message messages[32] = {0};
    yvex_prompt_options options = {.add_bos = 1, .add_generation_prompt = 1,
        .drop_thinking = 1, .mode = YVEX_PROMPT_MODE_CHAT,
        .reasoning_policy = YVEX_REASONING_DISABLED};
    yvex_rendered_prompt rendered = {0};
    unsigned int count = 0u;
    int index, rc;
    if (argc < 7 || (argc - 4) % 3 || argc > 100) return YVEX_ERR_INVALID_ARG;
    if (!strcmp(argv[3], "thinking")) {
        options.mode = YVEX_PROMPT_MODE_THINKING;
        options.reasoning_policy = YVEX_REASONING_ENABLED;
    } else if (!strcmp(argv[3], "transcript")) options.add_generation_prompt = 0;
    else if (strcmp(argv[3], "chat")) return YVEX_ERR_INVALID_ARG;
    for (index = 4; index < argc; index += 3) {
        yvex_prompt_message *message = &messages[count++];
        message->schema_version = YVEX_PROMPT_MESSAGE_SCHEMA_V1;
        if (!strcmp(argv[index], "system")) message->role = YVEX_PROMPT_ROLE_SYSTEM;
        else if (!strcmp(argv[index], "user")) message->role = YVEX_PROMPT_ROLE_USER;
        else if (!strcmp(argv[index], "assistant")) message->role = YVEX_PROMPT_ROLE_ASSISTANT;
        else if (!strcmp(argv[index], "tool")) message->role = YVEX_PROMPT_ROLE_TOOL;
        else return YVEX_ERR_INVALID_ARG;
        message->content = argv[index + 1];
        message->reasoning_content = argv[index + 2];
    }
    rc = yvex_prompt_render(&rendered, tokenizer, messages, count, &options, err);
    if (rc == YVEX_OK && fwrite(rendered.text, 1u, (size_t)rendered.len, stdout) != rendered.len)
        rc = YVEX_ERR_IO;
    yvex_rendered_prompt_free(&rendered);
    return rc;
}

int main(int argc, char **argv)
{
    yvex_model_context context = {0};
    yvex_runtime_binding *binding = NULL;
    yvex_runtime_binding_summary summary = {0};
    yvex_runtime_binding_failure failure = {0};
    yvex_complete_artifact_admission admission = {0};
    yvex_error err;
    const yvex_tokenizer_plan_summary *plan;
    int rc;
    if (argc < 4) return 2;
    rc = yvex_model_context_open(argv[1], &context, &err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_binding_open(&binding, argv[2], &summary, &admission, &failure, &err);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_from_compiled_gguf(&context.tokenizer, context.gguf,
            yvex_runtime_binding_tokenizer_policy(binding), &err);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_bind_runtime(context.tokenizer, summary.artifact_identity,
            summary.logical_model_identity, summary.runtime_descriptor_identity, &err);
    plan = yvex_tokenizer_plan_summary_get(context.tokenizer);
    if (rc == YVEX_OK && (!plan || !plan->runtime_bound || !plan->sealed ||
        plan->vocabulary_size != 248070u || plan->eos_token_id != 248046u ||
        plan->default_reasoning_policy != YVEX_REASONING_DISABLED)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK && !strcmp(argv[3], "negative")) {
        rc = authority_refusals(context.gguf, yvex_runtime_binding_tokenizer_policy(binding), &err);
        if (rc == YVEX_OK) puts("authority_refusals=4");
    } else if (rc == YVEX_OK && !strcmp(argv[3], "encode") && argc == 5) {
        yvex_tokenizer_encode_options options = {0, 0, 1, 65536u};
        yvex_tokenizer_encode_result encoded = {0};
        unsigned long long index;
        rc = yvex_tokenizer_encode(context.tokenizer, (const unsigned char *)argv[4],
            strlen(argv[4]), &options, &encoded, &err);
        for (index = 0u; rc == YVEX_OK && index < encoded.tokens.len; ++index)
            if (printf("%u%c", encoded.tokens.ids[index],
                index + 1u == encoded.tokens.len ? '\n' : ' ') < 0) rc = YVEX_ERR_IO;
        yvex_tokenizer_encode_result_clear(&encoded);
    } else if (rc == YVEX_OK) rc = render(context.tokenizer, argc, argv, &err);
    if (rc != YVEX_OK) fprintf(stderr, "status=%d where=%s reason=%s\n", rc,
        yvex_error_where(&err), yvex_error_message(&err));
    yvex_runtime_binding_close(binding);
    yvex_model_context_close(&context);
    return rc == YVEX_OK ? 0 : 1;
}
