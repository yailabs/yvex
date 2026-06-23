/*
 * YVEX - GGUF emitter metadata writer
 */
#include "gguf_emit_internal.h"

#include <stdint.h>
#include <string.h>

#define GGUF_VALUE_UINT32 4u
#define GGUF_VALUE_INT32 5u
#define GGUF_VALUE_FLOAT32 6u
#define GGUF_VALUE_STRING 8u
#define GGUF_VALUE_ARRAY 9u

int yvex_gguf_emit_write_u32(FILE *fp, unsigned int value, yvex_error *err, const char *field)
{
    unsigned char b[4];
    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    b[2] = (unsigned char)((value >> 16) & 0xffu);
    b[3] = (unsigned char)((value >> 24) & 0xffu);
    if (fwrite(b, 1u, sizeof(b), fp) != sizeof(b)) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_gguf_emit_write_u64(FILE *fp, unsigned long long value, yvex_error *err, const char *field)
{
    unsigned char b[8];
    unsigned int i;
    for (i = 0; i < 8u; ++i) {
        b[i] = (unsigned char)((value >> (i * 8u)) & 0xffull);
    }
    if (fwrite(b, 1u, sizeof(b), fp) != sizeof(b)) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_gguf_emit_write_i32(FILE *fp, int value, yvex_error *err, const char *field)
{
    return yvex_gguf_emit_write_u32(fp, (unsigned int)(uint32_t)(int32_t)value, err, field);
}

int yvex_gguf_emit_write_f32(FILE *fp, float value, yvex_error *err, const char *field)
{
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return yvex_gguf_emit_write_u32(fp, raw, err, field);
}

int yvex_gguf_emit_write_string(FILE *fp, const char *value, yvex_error *err, const char *field)
{
    unsigned long long len;
    if (!value) {
        value = "";
    }
    len = (unsigned long long)strlen(value);
    if (yvex_gguf_emit_write_u64(fp, len, err, field) != YVEX_OK) {
        return YVEX_ERR_IO;
    }
    if (len > 0 && fwrite(value, 1u, (size_t)len, fp) != (size_t)len) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int write_key_type(FILE *fp, const char *key, unsigned int type, yvex_error *err)
{
    if (yvex_gguf_emit_write_string(fp, key, err, "metadata key") != YVEX_OK) return YVEX_ERR_IO;
    return yvex_gguf_emit_write_u32(fp, type, err, "metadata type");
}

static int write_string_meta(FILE *fp, const char *key, const char *value, yvex_error *err)
{
    if (write_key_type(fp, key, GGUF_VALUE_STRING, err) != YVEX_OK) return YVEX_ERR_IO;
    return yvex_gguf_emit_write_string(fp, value, err, key);
}

static int write_u32_meta(FILE *fp, const char *key, unsigned int value, yvex_error *err)
{
    if (write_key_type(fp, key, GGUF_VALUE_UINT32, err) != YVEX_OK) return YVEX_ERR_IO;
    return yvex_gguf_emit_write_u32(fp, value, err, key);
}

static int write_i32_array(FILE *fp, const char *key, const int *values, unsigned long long count, yvex_error *err)
{
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, GGUF_VALUE_INT32, err, "array element type") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, count, err, "array count") != YVEX_OK) return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (yvex_gguf_emit_write_i32(fp, values[i], err, key) != YVEX_OK) return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int write_f32_array(FILE *fp, const char *key, const float *values, unsigned long long count, yvex_error *err)
{
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, GGUF_VALUE_FLOAT32, err, "array element type") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, count, err, "array count") != YVEX_OK) return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (yvex_gguf_emit_write_f32(fp, values[i], err, key) != YVEX_OK) return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int write_string_array(FILE *fp, const char *key, const char *const *values, unsigned long long count, yvex_error *err)
{
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, GGUF_VALUE_STRING, err, "array element type") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, count, err, "array count") != YVEX_OK) return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (yvex_gguf_emit_write_string(fp, values[i], err, key) != YVEX_OK) return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_gguf_emit_write_metadata(FILE *fp,
                                  const yvex_gguf_emit_plan_data *plan,
                                  yvex_error *err)
{
    const char *tokens[8] = {"<unk>", "<s>", "</s>", "a", "b", "c", "d", "e"};
    float scores[8] = {0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    int token_types[8] = {2, 3, 3, 1, 1, 1, 1, 1};

    if (write_string_meta(fp, "general.architecture", plan->architecture, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_string_meta(fp, "general.name", plan->model_name, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "llama.context_length", 8u, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "general.file_type", 0u, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "general.alignment", (unsigned int)YVEX_GGUF_EMIT_ALIGNMENT, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_string_meta(fp, "tokenizer.ggml.model", "llama", err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_string_array(fp, "tokenizer.ggml.tokens", tokens, 8ull, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_f32_array(fp, "tokenizer.ggml.scores", scores, 8ull, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_i32_array(fp, "tokenizer.ggml.token_type", token_types, 8ull, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.bos_token_id", 1u, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.eos_token_id", 2u, err) != YVEX_OK) return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.unknown_token_id", 0u, err) != YVEX_OK) return YVEX_ERR_IO;
    return YVEX_OK;
}
