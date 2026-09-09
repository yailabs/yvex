/* Shared prompt realization for generation, retained-prefix admission and public preflight. */
#include <yvex/internal/generation.h>
#include <yvex/internal/core.h>
#include <string.h>

int yvex_runtime_prompt_budget(
    unsigned long long input_tokens, unsigned long long sequence_capacity,
    unsigned long long output_capacity, unsigned long long requested_output,
    yvex_execution_preflight *out, yvex_error *err)
{
    unsigned long long remaining, maximum;
    if (!out || !sequence_capacity || !output_capacity) return YVEX_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));
    out->schema_version = YVEX_EXECUTION_PREFLIGHT_SCHEMA_V1;
    out->input_tokens = input_tokens;
    out->sequence_capacity = sequence_capacity;
    out->output_capacity = output_capacity;
    out->requested_output_tokens = requested_output;
    maximum = requested_output ? requested_output : output_capacity;
    if (input_tokens > sequence_capacity)
        out->violations |= YVEX_EXECUTION_INPUT_CAPACITY_EXCEEDED;
    if (maximum > output_capacity)
        out->violations |= YVEX_EXECUTION_OUTPUT_CAPACITY_EXCEEDED;
    remaining = input_tokens < sequence_capacity ? sequence_capacity - input_tokens : 0ull;
    out->effective_output_tokens = out->violations ? 0ull :
        (remaining < maximum ? remaining : maximum);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int prompt_refuse(yvex_error *err, yvex_status code, const char *reason)
{
    yvex_error_set(err, code, "runtime.prompt", reason);
    return code;
}

int yvex_runtime_prompt_encode(
    const yvex_tokenizer *tokenizer, unsigned long long token_limit,
    const yvex_runtime_generation_request *request,
    yvex_rendered_prompt *rendered, yvex_tokenizer_encode_result *encoded,
    char prompt_identity[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    yvex_tokenizer_encode_options encode;
    int rc;
    if (!tokenizer || !rendered || !encoded || !prompt_identity || !request ||
        request->schema_version != YVEX_RUNTIME_GENERATION_SCHEMA_V3 ||
        request->kind > YVEX_GENERATION_INPUT_PROVIDER)
        return prompt_refuse(err, YVEX_ERR_INVALID_ARG,
                                 "typed text or message input is required");
    memset(rendered, 0, sizeof(*rendered));
    memset(encoded, 0, sizeof(*encoded));
    encode = request->encode_options;
    encode.maximum_tokens = token_limit;
    if (request->kind == YVEX_GENERATION_INPUT_TEXT) {
        if (!request->text || !request->text_bytes)
            return prompt_refuse(err, YVEX_ERR_INVALID_ARG,
                                     "nonempty explicit-length prompt text is required");
        rc = yvex_tokenizer_encode(tokenizer, request->text,
                                   request->text_bytes, &encode, encoded, err);
        if (rc == YVEX_OK)
            yvex_core_text_copy(prompt_identity, YVEX_SHA256_HEX_CAP,
                                       encoded->input_identity);
    } else if (request->kind == YVEX_GENERATION_INPUT_MESSAGES) {
        if (!request->messages || !request->message_count)
            return prompt_refuse(err, YVEX_ERR_INVALID_ARG,
                                     "nonempty typed prompt messages are required");
        rc = yvex_tokenizer_encode_prompt(
            tokenizer, request->messages, request->message_count,
            &request->prompt_options, &encode, rendered, encoded, err);
        if (rc == YVEX_OK)
            yvex_core_text_copy(prompt_identity, YVEX_SHA256_HEX_CAP,
                                       rendered->prompt_identity);
    } else {
        if (!request->provider_request)
            return prompt_refuse(err, YVEX_ERR_INVALID_ARG,
                                     "sealed provider request is required");
        rc = yvex_tokenizer_encode_provider_prompt(
            tokenizer, request->provider_request, &encode,
            rendered, encoded, err);
        if (rc == YVEX_OK)
            yvex_core_text_copy(prompt_identity, YVEX_SHA256_HEX_CAP,
                                       rendered->prompt_identity);
    }
    if (rc == YVEX_ERR_TOKEN_CAPACITY) {
        yvex_error_setf(err, YVEX_ERR_INPUT_CAPACITY, "runtime.prompt",
                        "input token capacity exceeded: limit=%llu; includes template and tools",
                        token_limit);
        return YVEX_ERR_INPUT_CAPACITY;
    }
    if (rc == YVEX_OK && (!encoded->completed || !encoded->tokens.len))
        rc = prompt_refuse(err, YVEX_ERR_INVALID_ARG, "encoded prompt is empty");
    return rc;
}
