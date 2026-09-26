/* Instantiate an exact standalone Hugging Face ByteLevel BPE source. */
#define _POSIX_C_SOURCE 200809L
#include "src/tokenizer/private.h"
#include <yvex/internal/core.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int hf_refuse(yvex_error *err, yvex_status code, const char *message)
{
    yvex_error_set(err, code, "tokenizer.hf-json", message);
    return code;
}

static int source_digest(const char *json, size_t count, const char *expected)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char observed[YVEX_SHA256_HEX_CAP];
    if (!yvex_sha256_hex_valid(expected)) return 0;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, json, count) || !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, observed);
    return strcmp(observed, expected) == 0;
}

static int vocabulary_read(yvex_tokenizer *tokenizer, const char *json,
                           unsigned long long expected, yvex_error *err)
{
    const char *source = yvex_json_probe_field_value(json, "vocab");
    yvex_json cursor;
    yvex_json_iter entries;
    yvex_json_item item;
    char key[2048];
    unsigned long long seen = 0u, id;
    if (!source || expected > 100000u || expected < 256u)
        return hf_refuse(err, YVEX_ERR_FORMAT, "bounded vocabulary is unavailable");
    tokenizer->tokens = calloc((size_t)expected, sizeof(*tokenizer->tokens));
    if (!tokenizer->tokens) return hf_refuse(err, YVEX_ERR_NOMEM, "vocabulary allocation failed");
    tokenizer->vocab_size = expected;
    yvex_json_init(&cursor, source, strlen(source));
    if (!yvex_json_iter_begin(&cursor, &entries, YVEX_JSON_COLLECTION_OBJECT))
        return hf_refuse(err, YVEX_ERR_FORMAT, "vocabulary object required");
    while ((item = yvex_json_object_member(&entries, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        if (!yvex_json_u64(&cursor, &id) || id >= expected || tokenizer->tokens[id].text ||
            !(tokenizer->tokens[id].text = strdup(key)))
            return hf_refuse(err, YVEX_ERR_FORMAT, "vocabulary ID or text is invalid");
        tokenizer->tokens[id].id = (unsigned int)id;
        tokenizer->tokens[id].text_len = strlen(key);
        tokenizer->tokens[id].type = YVEX_TOKEN_TYPE_NORMAL;
        seen++;
    }
    if (item != YVEX_JSON_ITEM_END || seen >= expected) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "tokenizer.hf-json",
            "vocabulary population malformed: parsed=%llu expected=%llu cursor=%d",
            seen, expected, (int)item);
        return YVEX_ERR_FORMAT;
    }
    tokenizer->plan.base_vocabulary_size = seen;
    return YVEX_OK;
}

static int added_entry(yvex_tokenizer *tokenizer, yvex_json *cursor, yvex_error *err)
{
    yvex_json_iter fields;
    yvex_json_item item;
    char key[64], *content = NULL;
    unsigned long long id = ULLONG_MAX;
    int special = 0, have_special = 0;
    if (!yvex_json_iter_begin(cursor, &fields, YVEX_JSON_COLLECTION_OBJECT))
        return hf_refuse(err, YVEX_ERR_FORMAT, "added token object required");
    while ((item = yvex_json_object_member(&fields, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        if (!strcmp(key, "id")) {
            if (!yvex_json_u64(cursor, &id)) break;
        } else if (!strcmp(key, "content")) {
            content = yvex_json_string_dup(cursor, 2048u);
            if (!content) break;
        } else if (!strcmp(key, "special")) {
            if (!yvex_json_bool(cursor, &special)) break;
            have_special = 1;
        } else if (!yvex_json_skip_value(cursor)) break;
    }
    if (item != YVEX_JSON_ITEM_END || id >= tokenizer->vocab_size ||
        !content || !*content || !have_special) {
        free(content);
        return hf_refuse(err, YVEX_ERR_FORMAT, "added token is incomplete");
    }
    yvex_token_info *token = &tokenizer->tokens[id];
    if ((token->type && token->type != YVEX_TOKEN_TYPE_NORMAL) ||
        (token->text && strcmp(token->text, content))) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "tokenizer.hf-json",
            "added token id=%llu conflicts with vocabulary", id);
        free(content);
        return YVEX_ERR_FORMAT;
    }
    free((char *)token->text);
    token->id = (unsigned int)id;
    token->text = content;
    token->text_len = strlen(content);
    token->type = special ? YVEX_TOKEN_TYPE_CONTROL : YVEX_TOKEN_TYPE_USER_DEFINED;
    return YVEX_OK;
}

static int added_read(yvex_tokenizer *tokenizer, const char *json, yvex_error *err)
{
    const char *source = yvex_json_probe_field_value(json, "added_tokens");
    yvex_json cursor;
    yvex_json_iter entries;
    yvex_json_item item;
    unsigned long long count = 0u;
    if (!source) return hf_refuse(err, YVEX_ERR_FORMAT, "added token inventory missing");
    yvex_json_init(&cursor, source, strlen(source));
    if (!yvex_json_iter_begin(&cursor, &entries, YVEX_JSON_COLLECTION_ARRAY))
        return hf_refuse(err, YVEX_ERR_FORMAT, "added token array required");
    while ((item = yvex_json_array_value(&entries)) == YVEX_JSON_ITEM_READY) {
        if (++count > 1024u || added_entry(tokenizer, &cursor, err) != YVEX_OK)
            return YVEX_ERR_FORMAT;
    }
    if (item != YVEX_JSON_ITEM_END || !count)
        return hf_refuse(err, YVEX_ERR_FORMAT, "added token array malformed");
    for (unsigned long long id = 0u; id < tokenizer->vocab_size; ++id)
        if (!tokenizer->tokens[id].text)
            return hf_refuse(err, YVEX_ERR_FORMAT, "vocabulary contains an unbound token ID");
    tokenizer->compiled_policy.added_token_count = count;
    return YVEX_OK;
}

int yvex_tokenizer_from_hf_json(yvex_tokenizer **out, const char *json,
    size_t json_bytes, const char *expected_identity,
    unsigned long long expected_vocabulary, yvex_error *err)
{
    if (out) *out = NULL;
    if (!out || !json || !json_bytes || json_bytes > 8u * 1024u * 1024u ||
        !source_digest(json, json_bytes, expected_identity))
        return hf_refuse(err, YVEX_ERR_FORMAT, "exact immutable tokenizer JSON required");
    const char *model = yvex_json_probe_field_value(json, "model");
    const char *normalizer = yvex_json_probe_field_value(json, "normalizer");
    const char *pre = yvex_json_probe_field_value(json, "pre_tokenizer");
    char kind[32], normalizer_kind[32], pre_kind[32];
    if (!model || !normalizer || !pre ||
        !yvex_json_probe_string_field(model, "type", kind, sizeof(kind)) ||
        !yvex_json_probe_string_field(normalizer, "type", normalizer_kind, sizeof(normalizer_kind)) ||
        !yvex_json_probe_string_field(pre, "type", pre_kind, sizeof(pre_kind)) ||
        strcmp(kind, "BPE") || strcmp(normalizer_kind, "NFC") ||
        strcmp(pre_kind, "ByteLevel"))
        return hf_refuse(err, YVEX_ERR_UNSUPPORTED, "source tokenizer algorithm is not admitted");
    yvex_tokenizer *tokenizer = calloc(1u, sizeof(*tokenizer));
    if (!tokenizer) return hf_refuse(err, YVEX_ERR_NOMEM, "tokenizer allocation failed");
    tokenizer->kind = YVEX_TOKENIZER_KIND_HUGGINGFACE_JSON;
    tokenizer->compiled_policy.model_policy = YVEX_TOKENIZER_MODEL_BPE_BYTELEVEL;
    strcpy(tokenizer->compiled_policy.tokenizer_pre, "bytelevel-nfc");
    tokenizer->plan.schema_version = YVEX_TOKENIZER_PLAN_SCHEMA_V5;
    tokenizer->plan.model_policy = YVEX_TOKENIZER_MODEL_BPE_BYTELEVEL;
    tokenizer->plan.prompt_policy = YVEX_TOKENIZER_PROMPT_VERBATIM;
    tokenizer->plan.vocabulary_size = expected_vocabulary;
    yvex_core_text_copy(tokenizer->plan.tokenizer_json_identity,
        sizeof(tokenizer->plan.tokenizer_json_identity), expected_identity);
    int rc = vocabulary_read(tokenizer, json, expected_vocabulary, err);
    if (rc == YVEX_OK) rc = added_read(tokenizer, json, err);
    if (rc == YVEX_OK) rc = yvex_tokenizer_execution_seal_hf_json(tokenizer, json, json_bytes, err);
    if (rc != YVEX_OK) {
        yvex_tokenizer_close(tokenizer);
        return rc;
    }
    *out = tokenizer;
    yvex_error_clear(err);
    return YVEX_OK;
}
