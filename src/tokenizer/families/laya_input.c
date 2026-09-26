/* Exact bounded choice construction from the immutable Laya typed-decisions policy. */
#include <yvex/internal/families/laya_input.h>
#include <yvex/internal/tokenizer.h>
#include <yvex/internal/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAYA_TOKENIZER_ID "6c8aaa9a542084f2457eab775d4eeb51f92a70c0fd9de28d5edb0ddec3c08d30"
#define LAYA_SOURCE_ID "4fa56de72383a9d3efa9cfa78955733c81b9fc8067a587ca4beb82c78107a24e"
#define LAYA_TOKENIZER_CONFIG_ID "08d4cf3ac4dca381759441b85b91a6d40e688471dcd33d15d6649eb0a9a854d1"
#define LAYA_CLS 50281u
#define LAYA_SEP 50282u
#define LAYA_MASK 50284u
#define LAYA_DUAL_ROPE_INPUT_FORMAT_V1 1u

typedef struct { yvex_tokenizer *tokenizer; } laya_input;

static int laya_refuse(yvex_error *err, yvex_status code, const char *message)
{
    yvex_error_set(err, code, "model.laya.input", message);
    return code;
}

static int laya_open(void **out, const char *weight_path,
    const yvex_finite_input_admission *admission, yvex_error *err)
{
    char tokenizer_path[1024];
    const char *slash = strrchr(weight_path, '/');
    if (out) *out = NULL;
    if (!out || !slash || admission->marker_token_id != LAYA_MASK ||
        admission->token_domain_size != 50368u || admission->type_domain_size != 3u ||
        strcmp(admission->tokenizer_identity, LAYA_TOKENIZER_ID))
        return laya_refuse(err, YVEX_ERR_FORMAT, "Laya input policy differs from admitted binding");
    size_t root_bytes = (size_t)(slash - weight_path);
    if (root_bytes + sizeof("/tokenizer/tokenizer.json") >= sizeof(tokenizer_path))
        return laya_refuse(err, YVEX_ERR_BOUNDS, "tokenizer source path exceeds bound");
    if (root_bytes + sizeof("/tokenizer/tokenizer_config.json") >= sizeof(tokenizer_path))
        return laya_refuse(err, YVEX_ERR_BOUNDS, "tokenizer configuration path exceeds bound");
    memcpy(tokenizer_path, weight_path, root_bytes);
    strcpy(tokenizer_path + root_bytes, "/tokenizer/tokenizer_config.json");
    size_t config_bytes = 0u;
    char *config = yvex_read_bounded_file(tokenizer_path, 8u * 1024u * 1024u,
        &config_bytes, err);
    if (!config) return yvex_error_code(err);
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char observed[YVEX_SHA256_HEX_CAP];
    yvex_sha256_init(&hash);
    int config_valid = yvex_sha256_update(&hash, config, config_bytes) &&
        yvex_sha256_final(&hash, digest);
    free(config);
    if (!config_valid) return laya_refuse(err, YVEX_ERR_STATE, "tokenizer configuration identity unavailable");
    yvex_sha256_hex(digest, observed);
    if (strcmp(observed, LAYA_TOKENIZER_CONFIG_ID))
        return laya_refuse(err, YVEX_ERR_FORMAT, "tokenizer configuration differs from admitted source");
    strcpy(tokenizer_path + root_bytes, "/tokenizer/tokenizer.json");
    size_t json_bytes = 0u;
    char *json = yvex_read_bounded_file(tokenizer_path, 8u * 1024u * 1024u, &json_bytes, err);
    if (!json) return yvex_error_code(err);
    laya_input *state = calloc(1u, sizeof(*state));
    if (!state) { free(json); return laya_refuse(err, YVEX_ERR_NOMEM, "input policy allocation failed"); }
    int rc = yvex_tokenizer_from_hf_json(&state->tokenizer, json, json_bytes,
        admission->tokenizer_identity, admission->token_domain_size, err);
    free(json);
    if (rc != YVEX_OK) { free(state); return rc; }
    *out = state;
    return YVEX_OK;
}

static int laya_text_encode(laya_input *state, const char *text,
    yvex_tokenizer_encode_result *encoded, yvex_error *err)
{
    size_t length = strlen(text);
    char sanitized[1024];
    size_t out = 0u;
    for (size_t i = 0u; i < length; ++i) {
        if (!strncmp(text + i, "[MASK]", 6u)) { sanitized[out++] = ' '; i += 5u; }
        else sanitized[out++] = text[i];
        if (out >= sizeof(sanitized) - 1u)
            return laya_refuse(err, YVEX_ERR_BOUNDS, "bounded input text exceeded");
    }
    sanitized[out] = 0;
    yvex_tokenizer_encode_options options = {.maximum_tokens = 65u,
        .allow_special_tokens = 0};
    return yvex_tokenizer_encode(state->tokenizer, (const unsigned char *)sanitized,
        out, &options, encoded, err);
}

static int append(yvex_finite_input_compiled *out, const unsigned int *tokens,
    unsigned long long count, yvex_error *err)
{
    if (count > 64u - out->token_count)
        return laya_refuse(err, YVEX_ERR_BOUNDS, "constructed input exceeds admitted 64 tokens");
    for (unsigned long long i = 0u; i < count; ++i)
        out->tokens[out->token_count++] = tokens[i];
    return YVEX_OK;
}

static int laya_build(void *opaque, const yvex_finite_producer_request *request,
    yvex_finite_input_compiled *out, yvex_error *err)
{
    laya_input *state = opaque;
    yvex_tokenizer_encode_result head = {0}, context = {0};
    yvex_tokenizer_encode_result options[YVEX_FINITE_PRODUCER_MAX_CANDIDATES] = {{0}};
    char text[1024];
    unsigned long long option_tokens = 0u;
    int rc = YVEX_OK;
    if (!state || !state->tokenizer || !request || !out)
        return laya_refuse(err, YVEX_ERR_INVALID_ARG, "admitted input policy required");
    int n = snprintf(text, sizeof(text), "choice question: %s", request->question);
    if (n <= 0 || n >= (int)sizeof(text))
        return laya_refuse(err, YVEX_ERR_BOUNDS, "question exceeds input construction bound");
    rc = laya_text_encode(state, text, &head, err);
    for (unsigned long long i = 0u; rc == YVEX_OK && i < request->candidate_count; ++i) {
        n = snprintf(text, sizeof(text), " %c: %s", (int)('A' + i), request->candidates[i].text);
        if (n <= 0 || n >= (int)sizeof(text)) {
            rc = laya_refuse(err, YVEX_ERR_BOUNDS, "candidate exceeds input construction bound");
            break;
        }
        rc = laya_text_encode(state, text, &options[i], err);
        if (rc == YVEX_OK && options[i].tokens.len > 48u)
            rc = laya_refuse(err, YVEX_ERR_BOUNDS, "candidate requires unqualified truncation");
        option_tokens += 1u + options[i].tokens.len;
    }
    if (rc == YVEX_OK && (option_tokens > 32u || head.tokens.len > 48u - option_tokens))
        rc = laya_refuse(err, YVEX_ERR_BOUNDS, "question/frontier requires unqualified truncation");
    if (rc == YVEX_OK) rc = laya_text_encode(state, request->context, &context, err);
    if (rc == YVEX_OK) {
        const unsigned int cls = LAYA_CLS, sep = LAYA_SEP, mask = LAYA_MASK;
        rc = append(out, &cls, 1u, err);
        if (rc == YVEX_OK) rc = append(out, head.tokens.ids, head.tokens.len, err);
        if (rc == YVEX_OK) rc = append(out, &sep, 1u, err);
        for (unsigned long long i = 0u; rc == YVEX_OK && i < request->candidate_count; ++i) {
            out->candidates[i].candidate_id = request->candidates[i].id;
            out->candidates[i].marker_position = out->token_count;
            rc = append(out, &mask, 1u, err);
            if (rc == YVEX_OK) rc = append(out, options[i].tokens.ids, options[i].tokens.len, err);
        }
        if (rc == YVEX_OK) rc = append(out, &sep, 1u, err);
        if (rc == YVEX_OK) rc = append(out, context.tokens.ids, context.tokens.len, err);
        if (rc == YVEX_OK) rc = append(out, &sep, 1u, err);
        out->input_type_id = 0u; /* Compiler-authenticated choice head. */
    }
    yvex_tokenizer_encode_result_clear(&head);
    yvex_tokenizer_encode_result_clear(&context);
    for (unsigned long long i = 0u; i < request->candidate_count; ++i)
        yvex_tokenizer_encode_result_clear(&options[i]);
    return rc;
}

static void laya_close(void **opaque)
{
    if (!opaque || !*opaque) return;
    laya_input *state = *opaque;
    yvex_tokenizer_close(state->tokenizer);
    free(state);
    *opaque = NULL;
}

const yvex_finite_input_provider *yvex_laya_finite_input_provider(void)
{
    static const yvex_finite_input_provider provider = {
        .policy_name = "laya.typed-decisions.choice.v1;cls-sep-mask;head48;rows64;no-truncation",
        .source_identity = LAYA_SOURCE_ID,
        .tokenizer_identity = LAYA_TOKENIZER_ID,
        .policy_source_identity = LAYA_TOKENIZER_CONFIG_ID,
        .input_format = LAYA_DUAL_ROPE_INPUT_FORMAT_V1,
        .open = laya_open, .build = laya_build, .close = laya_close};
    return &provider;
}
