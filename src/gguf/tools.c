/*
 * gguf/tools.c - GGUF template, emission, intake, and quant command surfaces.
 *
 * This file owns GGUF template comparison, selected-tensor GGUF writing, and
 * operator command surfaces for GGUF/intake/quant tooling. It does not own
 * runtime execution or generation claims.
 */

#include "yvex_operator_private.h"
#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/gguf_emit.h>
#include <yvex/gguf_template.h>
#include <yvex/model.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>
#include <yvex/tokenizer.h>
#include <yvex/yvex.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_gguf_template {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_native_weight_table *native;
    yvex_gguf_template_summary summary;
    char *architecture;
    char *model_name;
    yvex_gguf_template_issue *issues;
    unsigned long long issue_count;
    unsigned long long issue_cap;
    unsigned long long native_tensor_count;
    unsigned long long matched_exact;
    unsigned long long missing_in_native;
    unsigned long long shape_mismatch;
};

int yvex_gguf_template_validate(yvex_gguf_template *tmpl,
                                const yvex_gguf_template_options *options,
                                yvex_error *err);

int yvex_gguf_template_compare_native(yvex_gguf_template *tmpl,
                                      const yvex_gguf_template_options *options,
                                      yvex_error *err);

int yvex_gguf_template_add_issue(yvex_gguf_template *tmpl,
                                 yvex_gguf_template_issue_kind kind,
                                 const char *tensor_name,
                                 const char *message,
                                 yvex_error *err);

void yvex_gguf_template_print_summary(const yvex_gguf_template *tmpl,
                                      const char *mode,
                                      const char *template_path);


/* Controlled GGUF emission */


#define YVEX_GGUF_EMIT_ALIGNMENT 32ull
#define YVEX_GGUF_EMIT_METADATA_COUNT 12ull
#define YVEX_GGUF_EMIT_TENSOR_COUNT 1ull
#define YVEX_GGUF_EMIT_NATIVE_ROWS 8u
#define YVEX_GGUF_EMIT_NATIVE_COLS 4u
#define YVEX_GGUF_EMIT_TENSOR_FLOATS 32u
#define YVEX_GGUF_EMIT_PAYLOAD_F32_BYTES 128ull
#define YVEX_GGUF_EMIT_PAYLOAD_F16_BYTES 64ull

typedef struct {
    const char *out_path;
    const char *template_path;
    const char *model_name;
    const char *architecture;
    const char *tensor_name;
    const char *target_name;
    const char *target_qtype;
    unsigned int ggml_type;
    unsigned long long scalar_bytes;
    int transpose_2d;
    int overwrite;
} yvex_gguf_emit_plan_data;

int yvex_gguf_emit_write_metadata(FILE *fp,
                                  const yvex_gguf_emit_plan_data *plan,
                                  yvex_error *err);
int yvex_gguf_emit_write_tensor_dir(FILE *fp,
                                    const yvex_gguf_emit_plan_data *plan,
                                    yvex_error *err);
int yvex_gguf_emit_write_tensor_payload(FILE *fp,
                                        const yvex_gguf_emit_plan_data *plan,
                                        yvex_error *err);
int yvex_gguf_emit_print_summary(const yvex_gguf_emit_summary *summary);

int yvex_gguf_emit_write_u32(FILE *fp, unsigned int value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_u64(FILE *fp, unsigned long long value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_i32(FILE *fp, int value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_f32(FILE *fp, float value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_u16(FILE *fp, unsigned int value, yvex_error *err, const char *field);
int yvex_gguf_emit_write_string(FILE *fp, const char *value, yvex_error *err, const char *field);
int yvex_gguf_emit_pad_to_alignment(FILE *fp, unsigned long long alignment, yvex_error *err);




static int path_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

static unsigned int controlled_float_to_f16_bits(float f)
{
    uint32_t raw;
    unsigned int sign;
    int exp;
    unsigned int mant;

    memcpy(&raw, &f, sizeof(raw));
    sign = (raw >> 16) & 0x8000u;
    exp = (int)((raw >> 23) & 0xffu) - 127 + 15;
    mant = raw & 0x7fffffu;

    if (exp <= 0) {
        return sign;
    }
    if (exp >= 31) {
        return sign | 0x7c00u;
    }
    return sign | ((unsigned int)exp << 10) | (mant >> 13);
}

static int controlled_qtype_to_plan(const char *qtype,
                                    unsigned int *ggml_type,
                                    unsigned long long *scalar_bytes)
{
    if (!qtype || strcmp(qtype, "F32") == 0) {
        *ggml_type = 0u;
        *scalar_bytes = 4ull;
        return 1;
    }
    if (strcmp(qtype, "F16") == 0) {
        *ggml_type = 1u;
        *scalar_bytes = 2ull;
        return 1;
    }
    return 0;
}

static long file_size(FILE *fp)
{
    long pos;
    long end;

    pos = ftell(fp);
    if (pos < 0) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        return -1;
    }
    end = ftell(fp);
    if (fseek(fp, pos, SEEK_SET) != 0) {
        return -1;
    }
    return end;
}

static int validate_roundtrip(const char *path, yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_model_descriptor *model = NULL;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_materialize_options materialize_options;
    int rc;

    memset(&artifact_options, 0, sizeof(artifact_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    artifact_options.path = path;
    artifact_options.readonly = 1;
    materialize_options.backend_name = "cpu";

    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc == YVEX_OK) rc = yvex_model_descriptor_from_gguf(&model, gguf, tensors, err);
    if (rc == YVEX_OK) rc = yvex_backend_open_cpu(&backend, err);
    if (rc == YVEX_OK) {
        rc = yvex_weight_table_materialize(&weights,
                                           artifact,
                                           gguf,
                                           tensors,
                                           backend,
                                           &materialize_options,
                                           err);
    }

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_model_descriptor_close(model);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

int yvex_gguf_emit_controlled(const yvex_gguf_emit_options *options,
                              yvex_gguf_emit_summary *summary_out,
                              yvex_error *err)
{
    yvex_gguf_emit_plan_data plan;
    yvex_gguf_emit_summary summary;
    FILE *fp;
    long bytes;
    int rc;

    if (!options || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_emit_controlled",
                       "options and summary_out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->out_path || options->out_path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_emit_controlled",
                       "out_path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->overwrite && path_exists(options->out_path)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_emit_controlled",
                        "refusing to overwrite existing file: %s", options->out_path);
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&plan, 0, sizeof(plan));
    plan.out_path = options->out_path;
    plan.template_path = options->template_path;
    plan.model_name = options->model_name ? options->model_name : "yvex-owned-gguf-test";
    plan.architecture = options->architecture ? options->architecture : "llama";
    plan.tensor_name = options->tensor_name ? options->tensor_name : "embed.weight";
    plan.target_name = options->target_name ? options->target_name : "token_embd.weight";
    plan.target_qtype = options->target_qtype ? options->target_qtype : "F32";
    plan.transpose_2d = options->transpose_2d ? 1 : 1;
    plan.overwrite = options->overwrite;
    if (!controlled_qtype_to_plan(plan.target_qtype, &plan.ggml_type, &plan.scalar_bytes)) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_gguf_emit_controlled",
                        "controlled emit target qtype is unsupported: %s", plan.target_qtype);
        return YVEX_ERR_UNSUPPORTED;
    }

    memset(&summary, 0, sizeof(summary));
    summary.status = YVEX_GGUF_EMIT_STATUS_PLANNED;
    summary.out_path = plan.out_path;
    summary.template_path = plan.template_path;
    summary.model_name = plan.model_name;
    summary.architecture = plan.architecture;
    summary.metadata_count = YVEX_GGUF_EMIT_METADATA_COUNT;
    summary.tensor_count = YVEX_GGUF_EMIT_TENSOR_COUNT;
    summary.tensor_payload_bytes = (unsigned long long)YVEX_GGUF_EMIT_TENSOR_FLOATS * plan.scalar_bytes;
    summary.alignment = YVEX_GGUF_EMIT_ALIGNMENT;

    fp = fopen(plan.out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_gguf_emit_controlled",
                        "failed to open output %s: %s", plan.out_path, strerror(errno));
        summary.status = YVEX_GGUF_EMIT_STATUS_FAILED;
        *summary_out = summary;
        return YVEX_ERR_IO;
    }

    rc = yvex_gguf_emit_write_u32(fp, YVEX_GGUF_MAGIC, err, "magic");
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_u32(fp, 3u, err, "version");
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_u64(fp, YVEX_GGUF_EMIT_TENSOR_COUNT, err, "tensor count");
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_u64(fp, YVEX_GGUF_EMIT_METADATA_COUNT, err, "metadata count");
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_metadata(fp, &plan, err);
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_tensor_dir(fp, &plan, err);
    if (rc == YVEX_OK) rc = yvex_gguf_emit_pad_to_alignment(fp, YVEX_GGUF_EMIT_ALIGNMENT, err);
    if (rc == YVEX_OK) rc = yvex_gguf_emit_write_tensor_payload(fp, &plan, err);

    if (fflush(fp) != 0 && rc == YVEX_OK) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_gguf_emit_controlled",
                        "failed to flush output %s: %s", plan.out_path, strerror(errno));
        rc = YVEX_ERR_IO;
    }
    bytes = file_size(fp);
    if (fclose(fp) != 0 && rc == YVEX_OK) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_gguf_emit_controlled",
                        "failed to close output %s: %s", plan.out_path, strerror(errno));
        rc = YVEX_ERR_IO;
    }

    if (rc != YVEX_OK) {
        summary.status = YVEX_GGUF_EMIT_STATUS_FAILED;
        *summary_out = summary;
        return rc;
    }

    summary.bytes_written = bytes > 0 ? (unsigned long long)bytes : 0ull;
    rc = validate_roundtrip(plan.out_path, err);
    if (rc != YVEX_OK) {
        summary.status = YVEX_GGUF_EMIT_STATUS_FAILED;
        *summary_out = summary;
        return rc;
    }

    summary.status = YVEX_GGUF_EMIT_STATUS_WRITTEN;
    summary.roundtrip_validated = 1;
    *summary_out = summary;
    yvex_error_clear(err);
    return YVEX_OK;
}



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

int yvex_gguf_emit_write_u16(FILE *fp, unsigned int value, yvex_error *err, const char *field)
{
    unsigned char b[2];
    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    if (fwrite(b, 1u, sizeof(b), fp) != sizeof(b)) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
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
    if (write_u32_meta(fp, "general.file_type", plan->ggml_type, err) != YVEX_OK) return YVEX_ERR_IO;
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



const char *yvex_gguf_emit_status_name(yvex_gguf_emit_status status)
{
    switch (status) {
    case YVEX_GGUF_EMIT_STATUS_UNKNOWN: return "gguf-unknown";
    case YVEX_GGUF_EMIT_STATUS_PLANNED: return "gguf-planned";
    case YVEX_GGUF_EMIT_STATUS_WRITTEN: return "gguf-written";
    case YVEX_GGUF_EMIT_STATUS_FAILED: return "gguf-failed";
    }
    return "gguf-unknown";
}

int yvex_gguf_emit_print_summary(const yvex_gguf_emit_summary *summary)
{
    if (!summary) {
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(stdout, "gguf emit: controlled\n");
    fprintf(stdout, "out: %s\n", summary->out_path ? summary->out_path : "");
    fprintf(stdout, "architecture: %s\n", summary->architecture ? summary->architecture : "");
    fprintf(stdout, "model_name: %s\n", summary->model_name ? summary->model_name : "");
    fprintf(stdout, "metadata_count: %llu\n", summary->metadata_count);
    fprintf(stdout, "tensor_count: %llu\n", summary->tensor_count);
    fprintf(stdout, "tensor_payload_bytes: %llu\n", summary->tensor_payload_bytes);
    fprintf(stdout, "alignment: %llu\n", summary->alignment);
    fprintf(stdout, "roundtrip_validated: %s\n", summary->roundtrip_validated ? "yes" : "no");
    fprintf(stdout, "status: %s\n", yvex_gguf_emit_status_name(summary->status));
    return YVEX_OK;
}



int yvex_gguf_emit_pad_to_alignment(FILE *fp, unsigned long long alignment, yvex_error *err)
{
    long pos;
    unsigned long long rem;
    unsigned long long pad;
    unsigned char zero[32];

    pos = ftell(fp);
    if (pos < 0) {
        yvex_error_set(err, YVEX_ERR_IO, "gguf_emit.align", "failed to query file offset");
        return YVEX_ERR_IO;
    }
    rem = ((unsigned long long)pos) % alignment;
    pad = rem == 0 ? 0 : alignment - rem;
    memset(zero, 0, sizeof(zero));
    while (pad > 0) {
        size_t chunk = pad > sizeof(zero) ? sizeof(zero) : (size_t)pad;
        if (fwrite(zero, 1u, chunk, fp) != chunk) {
            yvex_error_set(err, YVEX_ERR_IO, "gguf_emit.align", "failed to write alignment padding");
            return YVEX_ERR_IO;
        }
        pad -= (unsigned long long)chunk;
    }
    return YVEX_OK;
}

int yvex_gguf_emit_write_tensor_dir(FILE *fp,
                                    const yvex_gguf_emit_plan_data *plan,
                                    yvex_error *err)
{
    if (yvex_gguf_emit_write_string(fp, plan->target_name, err, "tensor name") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, 2u, err, "tensor rank") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, 4ull, err, "tensor dim 0") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u64(fp, 8ull, err, "tensor dim 1") != YVEX_OK) return YVEX_ERR_IO;
    if (yvex_gguf_emit_write_u32(fp, plan->ggml_type, err, "tensor ggml type") != YVEX_OK) return YVEX_ERR_IO;
    return yvex_gguf_emit_write_u64(fp, 0ull, err, "tensor relative offset");
}

int yvex_gguf_emit_write_tensor_payload(FILE *fp,
                                        const yvex_gguf_emit_plan_data *plan,
                                        yvex_error *err)
{
    float native[YVEX_GGUF_EMIT_NATIVE_ROWS][YVEX_GGUF_EMIT_NATIVE_COLS];
    unsigned int row;
    unsigned int col;

    for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
        for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
            native[row][col] = (float)(row * YVEX_GGUF_EMIT_NATIVE_COLS + col);
        }
    }

    if (plan->transpose_2d) {
        for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
            for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
                if (plan->ggml_type == 1u) {
                    if (yvex_gguf_emit_write_u16(fp, controlled_float_to_f16_bits(native[row][col]), err, "tensor payload") != YVEX_OK) {
                        return YVEX_ERR_IO;
                    }
                } else if (yvex_gguf_emit_write_f32(fp, native[row][col], err, "tensor payload") != YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    } else {
        for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
            for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
                if (plan->ggml_type == 1u) {
                    if (yvex_gguf_emit_write_u16(fp, controlled_float_to_f16_bits(native[row][col]), err, "tensor payload") != YVEX_OK) {
                        return YVEX_ERR_IO;
                    }
                } else if (yvex_gguf_emit_write_f32(fp, native[row][col], err, "tensor payload") != YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    }

    return YVEX_OK;
}



static char *gt_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

static int gt_open_artifact(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;
    options.map = 1;
    return yvex_artifact_open(artifact, &options, err);
}

const char *yvex_gguf_template_status_name(yvex_gguf_template_status status)
{
    switch (status) {
    case YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN: return "template-unknown";
    case YVEX_GGUF_TEMPLATE_STATUS_VALID: return "template-valid";
    case YVEX_GGUF_TEMPLATE_STATUS_PARTIAL: return "template-partial";
    case YVEX_GGUF_TEMPLATE_STATUS_INVALID: return "template-invalid";
    }
    return "template-unknown";
}

const char *yvex_gguf_template_issue_kind_name(yvex_gguf_template_issue_kind kind)
{
    switch (kind) {
    case YVEX_GGUF_TEMPLATE_ISSUE_NONE: return "none";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE: return "missing_architecture";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME: return "missing_model_name";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER: return "missing_tokenizer";
    case YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY: return "empty_tensor_directory";
    case YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE: return "unknown_tensor_role";
    case YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR: return "native_missing_tensor";
    case YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH: return "native_shape_mismatch";
    case YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE: return "unsupported_dtype";
    case YVEX_GGUF_TEMPLATE_ISSUE_FORMAT: return "format";
    }
    return "format";
}

int yvex_gguf_template_add_issue(yvex_gguf_template *tmpl,
                                 yvex_gguf_template_issue_kind kind,
                                 const char *tensor_name,
                                 const char *message,
                                 yvex_error *err)
{
    yvex_gguf_template_issue *next;
    yvex_gguf_template_issue *issue;

    if (!tmpl) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_issue", "template is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tmpl->issue_count == tmpl->issue_cap) {
        unsigned long long cap = tmpl->issue_cap == 0 ? 8u : tmpl->issue_cap * 2u;
        next = (yvex_gguf_template_issue *)realloc(tmpl->issues, (size_t)cap * sizeof(tmpl->issues[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_issue", "issue allocation failed");
            return YVEX_ERR_NOMEM;
        }
        tmpl->issues = next;
        tmpl->issue_cap = cap;
    }
    issue = &tmpl->issues[tmpl->issue_count];
    memset(issue, 0, sizeof(*issue));
    issue->kind = kind;
    issue->tensor_name = gt_strdup(tensor_name);
    issue->message = gt_strdup(message);
    if (!issue->tensor_name || !issue->message) {
        free((char *)issue->tensor_name);
        free((char *)issue->message);
        memset(issue, 0, sizeof(*issue));
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_issue", "issue string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    tmpl->issue_count++;
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}

int yvex_gguf_template_open(yvex_gguf_template **out,
                            const yvex_gguf_template_options *options,
                            yvex_error *err)
{
    yvex_gguf_template *tmpl;
    int rc;

    if (!out || !options || !options->template_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_open", "out and template_path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    tmpl = (yvex_gguf_template *)calloc(1, sizeof(*tmpl));
    if (!tmpl) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_open", "template allocation failed");
        return YVEX_ERR_NOMEM;
    }
    tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN;

    rc = gt_open_artifact(options->template_path, &tmpl->artifact, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&tmpl->gguf, tmpl->artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tmpl->tensors, tmpl->gguf, err);
    if (rc == YVEX_OK) rc = yvex_model_descriptor_from_gguf(&tmpl->model, tmpl->gguf, tmpl->tensors, err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    rc = yvex_gguf_template_validate(tmpl, options, err);
    if (rc == YVEX_OK && options->compare_native) {
        rc = yvex_gguf_template_compare_native(tmpl, options, err);
    }
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    *out = tmpl;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_gguf_template_close(yvex_gguf_template *tmpl)
{
    unsigned long long i;

    if (!tmpl) return;
    for (i = 0; i < tmpl->issue_count; ++i) {
        free((char *)tmpl->issues[i].tensor_name);
        free((char *)tmpl->issues[i].message);
    }
    free(tmpl->issues);
    free(tmpl->architecture);
    free(tmpl->model_name);
    yvex_native_weight_table_close(tmpl->native);
    yvex_tokenizer_close(tmpl->tokenizer);
    yvex_model_descriptor_close(tmpl->model);
    yvex_tensor_table_close(tmpl->tensors);
    yvex_gguf_close(tmpl->gguf);
    yvex_artifact_close(tmpl->artifact);
    free(tmpl);
}

int yvex_gguf_template_get_summary(const yvex_gguf_template *tmpl,
                                   yvex_gguf_template_summary *out,
                                   yvex_error *err)
{
    if (!tmpl || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_summary", "template and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = tmpl->summary;
    return YVEX_OK;
}

unsigned long long yvex_gguf_template_issue_count(const yvex_gguf_template *tmpl)
{
    return tmpl ? tmpl->issue_count : 0;
}

const yvex_gguf_template_issue *yvex_gguf_template_issue_at(const yvex_gguf_template *tmpl,
                                                            unsigned long long index)
{
    if (!tmpl || index >= tmpl->issue_count) return NULL;
    return &tmpl->issues[index];
}



static int gt_same_shape(const yvex_tensor_info *tensor, const yvex_native_weight_info *native)
{
    unsigned int i;

    if (tensor->rank != native->rank) {
        return 0;
    }
    for (i = 0; i < tensor->rank; ++i) {
        if (tensor->dims[i] != native->dims[i]) {
            return 0;
        }
    }
    return 1;
}

int yvex_gguf_template_compare_native(yvex_gguf_template *tmpl,
                                      const yvex_gguf_template_options *options,
                                      yvex_error *err)
{
    yvex_native_weight_options native_options;
    yvex_native_weight_summary native_summary;
    unsigned long long i;
    int rc;

    if (!options->native_source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_compare", "native_source_dir is required");
        return YVEX_ERR_INVALID_ARG;
    }
    native_options.source_dir = options->native_source_dir;
    native_options.recursive = 1;
    native_options.include_metadata = 0;
    rc = yvex_native_weight_table_open(&tmpl->native, &native_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_native_weight_table_summary(tmpl->native, &native_summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    tmpl->native_tensor_count = native_summary.tensor_count;

    for (i = 0; i < yvex_tensor_table_count(tmpl->tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tmpl->tensors, i);
        const yvex_native_weight_info *native;

        if (!tensor) continue;
        native = yvex_native_weight_table_find(tmpl->native, tensor->name);
        if (!native) {
            tmpl->missing_in_native++;
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR,
                                              tensor->name, "template tensor missing in native inventory; open-weight intake mapping may be required", err);
            if (rc != YVEX_OK) return rc;
            continue;
        }
        tmpl->matched_exact++;
        if (!gt_same_shape(tensor, native)) {
            char message[192];
            tmpl->shape_mismatch++;
            snprintf(message, sizeof(message), "native/template shape mismatch for exact-name tensor");
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH,
                                              tensor->name, message, err);
            if (rc != YVEX_OK) return rc;
        }
    }

    if ((tmpl->missing_in_native > 0 || tmpl->shape_mismatch > 0) &&
        tmpl->summary.status == YVEX_GGUF_TEMPLATE_STATUS_VALID) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_PARTIAL;
    }
    if (options->require_all_template_tensors_in_native &&
        (tmpl->missing_in_native > 0 || tmpl->shape_mismatch > 0)) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_INVALID;
    }
    tmpl->summary.native_tensor_count = tmpl->native_tensor_count;
    tmpl->summary.matched_exact = tmpl->matched_exact;
    tmpl->summary.missing_in_native = tmpl->missing_in_native;
    tmpl->summary.shape_mismatch = tmpl->shape_mismatch;
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}



void yvex_gguf_template_print_summary(const yvex_gguf_template *tmpl,
                                      const char *mode,
                                      const char *template_path)
{
    yvex_gguf_template_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_gguf_template_get_summary(tmpl, &summary, &err) != YVEX_OK) {
        return;
    }
    fprintf(stdout, "gguf template: %s\n", mode);
    fprintf(stdout, "template: %s\n", template_path ? template_path : "");
    fprintf(stdout, "architecture: %s\n", summary.architecture ? summary.architecture : "");
    fprintf(stdout, "model_name: %s\n", summary.model_name ? summary.model_name : "");
    fprintf(stdout, "metadata_count: %llu\n", summary.metadata_count);
    fprintf(stdout, "tensor_count: %llu\n", summary.tensor_count);
    fprintf(stdout, "has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
    fprintf(stdout, "known_roles: %llu\n", summary.known_role_count);
    fprintf(stdout, "unknown_roles: %llu\n", summary.unknown_role_count);
    fprintf(stdout, "status: %s\n", yvex_gguf_template_status_name(summary.status));
}



static char *gt_copy_string_value(const yvex_gguf *gguf, const char *key)
{
    const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, key);
    const char *data;
    unsigned long long len;
    char *out;

    if (!value || yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK) {
        return NULL;
    }
    out = (char *)malloc((size_t)len + 1u);
    if (!out) return NULL;
    memcpy(out, data, (size_t)len);
    out[len] = '\0';
    return out;
}

static unsigned long long gt_tokenizer_metadata_count(const yvex_gguf *gguf)
{
    unsigned long long i;
    unsigned long long n = 0;

    for (i = 0; i < yvex_gguf_metadata_count(gguf); ++i) {
        const char *key = yvex_gguf_metadata_key(gguf, i);
        if (key && strncmp(key, "tokenizer.", 10) == 0) {
            n++;
        }
    }
    return n;
}

int yvex_gguf_template_validate(yvex_gguf_template *tmpl,
                                const yvex_gguf_template_options *options,
                                yvex_error *err)
{
    unsigned long long i;
    int fatal = 0;
    int partial = 0;
    int rc;

    tmpl->summary.metadata_count = yvex_gguf_metadata_count(tmpl->gguf);
    tmpl->summary.tensor_count = yvex_tensor_table_count(tmpl->tensors);
    tmpl->summary.has_tensor_directory = tmpl->summary.tensor_count > 0;
    tmpl->summary.tokenizer_metadata_count = gt_tokenizer_metadata_count(tmpl->gguf);
    tmpl->summary.has_tokenizer =
        yvex_gguf_metadata_find(tmpl->gguf, "tokenizer.ggml.model") != NULL &&
        yvex_gguf_metadata_find(tmpl->gguf, "tokenizer.ggml.tokens") != NULL;

    tmpl->architecture = gt_copy_string_value(tmpl->gguf, "general.architecture");
    tmpl->model_name = gt_copy_string_value(tmpl->gguf, "general.name");
    tmpl->summary.architecture = tmpl->architecture ? tmpl->architecture : "";
    tmpl->summary.model_name = tmpl->model_name ? tmpl->model_name : "";
    tmpl->summary.has_architecture = tmpl->architecture && tmpl->architecture[0] != '\0';

    if (!tmpl->summary.has_architecture) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE,
                                          "", "general.architecture missing", err);
        if (rc != YVEX_OK) return rc;
        fatal = 1;
    }
    if (!tmpl->model_name || tmpl->model_name[0] == '\0') {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME,
                                          "", "general.name missing", err);
        if (rc != YVEX_OK) return rc;
        partial = 1;
    }
    if (!tmpl->summary.has_tokenizer) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER,
                                          "", "tokenizer.ggml.model or tokenizer.ggml.tokens missing", err);
        if (rc != YVEX_OK) return rc;
        if (options->require_tokenizer) {
            fatal = 1;
        } else {
            partial = 1;
        }
    }
    if (!tmpl->summary.has_tensor_directory) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY,
                                          "", "tensor directory is empty", err);
        if (rc != YVEX_OK) return rc;
        fatal = 1;
    }

    for (i = 0; i < yvex_tensor_table_count(tmpl->tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tmpl->tensors, i);
        if (!tensor) continue;
        if (tensor->role == YVEX_TENSOR_ROLE_UNKNOWN) {
            tmpl->summary.unknown_role_count++;
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE,
                                              tensor->name, "tensor role is unknown before open-weight intake mapping", err);
            if (rc != YVEX_OK) return rc;
            partial = 1;
        } else {
            tmpl->summary.known_role_count++;
        }
        if (tensor->storage_bytes == 0) {
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE,
                                              tensor->name, "tensor dtype has unsupported storage accounting", err);
            if (rc != YVEX_OK) return rc;
            partial = 1;
        }
    }

    if (fatal) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_INVALID;
    } else if (partial || tmpl->issue_count > 0) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_PARTIAL;
    } else {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_VALID;
    }
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}

static int cli_parse_gguf_template_options(int arg_count, char **args, int start,
                                           const char **template_path,
                                           const char **native_source,
                                           int *require_all)
{
    int i = start;

    *template_path = NULL;
    *native_source = NULL;
    *require_all = 0;
    while (i < arg_count) {
        if (strcmp(args[i], "--require-all-template-tensors-in-native") == 0) {
            *require_all = 1;
            i++;
            continue;
        }
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: gguf-template option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--template") == 0) {
            *template_path = args[i + 1];
        } else if (strcmp(args[i], "--native-source") == 0) {
            *native_source = args[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown gguf-template option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void cli_print_template_issues(const yvex_gguf_template *tmpl)
{
    unsigned long long i;
    unsigned long long count = yvex_gguf_template_issue_count(tmpl);

    for (i = 0; i < count; ++i) {
        const yvex_gguf_template_issue *issue = yvex_gguf_template_issue_at(tmpl, i);
        if (!issue) {
            continue;
        }
        if (issue->tensor_name && issue->tensor_name[0] != '\0') {
            fprintf(stdout, "issue %llu %s tensor=\"%s\" message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->tensor_name,
                   issue->message ? issue->message : "");
        } else {
            fprintf(stdout, "issue %llu %s message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->message ? issue->message : "");
        }
    }
}

static int command_gguf_template(int arg_count, char **args)
{
    yvex_gguf_template_options options;
    yvex_gguf_template *tmpl = NULL;
    yvex_gguf_template_summary summary;
    yvex_error err;
    const char *template_path;
    const char *native_source;
    int require_all;
    int rc;

    yvex_error_clear(&err);
    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_gguf_template_help(stdout);
        return 0;
    }
    if (arg_count < 3) {
        fprintf(stderr, "yvex: gguf-template requires inspect, validate, or compare\n");
        fprintf(stderr, "usage: " "yvex gguf-template inspect|validate --template FILE\n");
        return 2;
    }
    if (strcmp(args[2], "inspect") != 0 && strcmp(args[2], "validate") != 0 &&
        strcmp(args[2], "compare") != 0) {
        fprintf(stderr, "yvex: unknown gguf-template subcommand: %s\n", args[2]);
        return 2;
    }
    rc = cli_parse_gguf_template_options(arg_count, args, 3, &template_path, &native_source, &require_all);
    if (rc != 0) {
        return rc;
    }
    if (!template_path) {
        fprintf(stderr, "yvex: gguf-template requires --template FILE\n");
        return 2;
    }
    if (strcmp(args[2], "compare") == 0 && !native_source) {
        fprintf(stderr, "yvex: gguf-template compare requires --native-source DIR\n");
        return 2;
    }

    memset(&options, 0, sizeof(options));
    options.template_path = template_path;
    options.native_source_dir = native_source;
    options.compare_native = strcmp(args[2], "compare") == 0;
    options.require_all_template_tensors_in_native = require_all;

    rc = yvex_gguf_template_open(&tmpl, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_gguf_template_get_summary(tmpl, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (strcmp(args[2], "compare") == 0) {
        fprintf(stdout, "gguf template: compare\n");
        fprintf(stdout, "template: %s\n", template_path);
        fprintf(stdout, "native_source: %s\n", native_source);
        fprintf(stdout, "template_tensors: %llu\n", summary.tensor_count);
        fprintf(stdout, "native_tensors: %llu\n", summary.native_tensor_count);
        fprintf(stdout, "matched_exact: %llu\n", summary.matched_exact);
        fprintf(stdout, "missing_in_native: %llu\n", summary.missing_in_native);
        fprintf(stdout, "shape_mismatch: %llu\n", summary.shape_mismatch);
        fprintf(stdout, "status: template-compare-%s\n",
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_VALID ? "valid" :
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_INVALID ? "invalid" : "partial");
        if (summary.missing_in_native > 0 || summary.shape_mismatch > 0) {
            fprintf(stdout, "reason: architecture adapter mapping requires open-weight intake\n");
        }
        cli_print_template_issues(tmpl);
    } else if (strcmp(args[2], "validate") == 0) {
        fprintf(stdout, "gguf template: validate\n");
        fprintf(stdout, "template: %s\n", template_path);
        fprintf(stdout, "status: %s\n", yvex_gguf_template_status_name(summary.status));
        fprintf(stdout, "issues: %llu\n", summary.issue_count);
        cli_print_template_issues(tmpl);
    } else {
        fprintf(stdout, "gguf template: inspect\n");
        fprintf(stdout, "template: %s\n", template_path);
        fprintf(stdout, "architecture: %s\n", summary.architecture ? summary.architecture : "");
        fprintf(stdout, "model_name: %s\n", summary.model_name ? summary.model_name : "");
        fprintf(stdout, "metadata_count: %llu\n", summary.metadata_count);
        fprintf(stdout, "tensor_count: %llu\n", summary.tensor_count);
        fprintf(stdout, "has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
        fprintf(stdout, "known_roles: %llu\n", summary.known_role_count);
        fprintf(stdout, "unknown_roles: %llu\n", summary.unknown_role_count);
        fprintf(stdout, "status: %s\n", yvex_gguf_template_status_name(summary.status));
    }

    yvex_gguf_template_close(tmpl);
    return 0;
}

static int command_gguf_emit(int arg_count, char **args)
{
    yvex_gguf_emit_options options;
    yvex_gguf_emit_summary summary;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_gguf_emit_help(stdout);
        return 0;
    }
    if (arg_count < 3 || strcmp(args[2], "controlled") != 0) {
        fprintf(stderr, "yvex: gguf-emit requires subcommand controlled\n");
        fprintf(stderr, "usage: " "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
        return 2;
    }

    options.tensor_name = "embed.weight";
    options.target_name = "token_embd.weight";
    options.model_name = "yvex-owned-gguf-test";
    options.architecture = "llama";
    options.transpose_2d = 1;

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: gguf-emit option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--out") == 0) {
            options.out_path = args[++i];
        } else if (strcmp(args[i], "--template") == 0) {
            options.template_path = args[++i];
        } else if (strcmp(args[i], "--native-source") == 0) {
            options.native_source_dir = args[++i];
        } else if (strcmp(args[i], "--tensor-name") == 0) {
            options.tensor_name = args[++i];
        } else if (strcmp(args[i], "--target-name") == 0) {
            options.target_name = args[++i];
        } else if (strcmp(args[i], "--target-qtype") == 0) {
            options.target_qtype = args[++i];
        } else if (strcmp(args[i], "--model-name") == 0) {
            options.model_name = args[++i];
        } else if (strcmp(args[i], "--arch") == 0) {
            options.architecture = args[++i];
        } else {
            fprintf(stderr, "yvex: unknown gguf-emit option: %s\n", args[i]);
            fprintf(stderr, "Try '" "yvex help gguf-emit' for usage.\n");
            return 2;
        }
    }

    if (!options.out_path) {
        fprintf(stderr, "yvex: gguf-emit controlled requires --out FILE\n");
        fprintf(stderr, "usage: " "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
        return 2;
    }

    rc = yvex_gguf_emit_controlled(&options, &summary, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    fprintf(stdout, "gguf emit: controlled\n");
    fprintf(stdout, "out: %s\n", summary.out_path ? summary.out_path : "");
    fprintf(stdout, "architecture: %s\n", summary.architecture ? summary.architecture : "");
    fprintf(stdout, "model_name: %s\n", summary.model_name ? summary.model_name : "");
    fprintf(stdout, "metadata_count: %llu\n", summary.metadata_count);
    fprintf(stdout, "tensor_count: %llu\n", summary.tensor_count);
    fprintf(stdout, "tensor_payload_bytes: %llu\n", summary.tensor_payload_bytes);
    fprintf(stdout, "alignment: %llu\n", summary.alignment);
    fprintf(stdout, "roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    fprintf(stdout, "status: %s\n", yvex_gguf_emit_status_name(summary.status));
    return 0;
}

static int command_qtype_support(int arg_count, char **args)
{
    unsigned long long i;

    if (arg_count == 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_qtype_support_help(stdout);
        return 0;
    }
    if (arg_count != 2) {
        fprintf(stderr, "yvex: qtype-support takes no arguments\n");
        return 2;
    }
    fprintf(stdout, "qtype support:\n");
    for (i = 0; i < yvex_qtype_support_count(); ++i) {
        const yvex_qtype_support_info *row = yvex_qtype_support_at(i);
        fprintf(stdout, "  %s policy=%s storage=%s emit=%s quantize=%s compute=%s notes=%s\n",
               row->qtype,
               row->policy_supported ? "yes" : "no",
               row->storage_supported ? "yes" : "no",
               row->emit_supported ? "yes" : "no",
               strcmp(row->qtype, "F32") == 0 ? "n/a" : (row->quantize_supported ? "yes" : "no"),
               row->compute_supported ? "partial" : "no",
               row->notes ? row->notes : "");
    }
    fprintf(stdout, "status: qtype-support\n");
    return 0;
}

static int command_convert(int arg_count, char **args)
{
    yvex_conversion_options options;
    yvex_conversion_summary summary;
    yvex_error err;
    const char *out_plan = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_convert_help(stdout);
        return 0;
    }
    if (arg_count < 3 || (strcmp(args[2], "plan") != 0 && strcmp(args[2], "emit") != 0)) {
        fprintf(stderr, "yvex: convert requires plan or emit\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (strcmp(args[i], "--allow-unsupported-qtype") == 0) {
            options.allow_unsupported_qtype = 1;
            continue;
        }
        if (strcmp(args[i], "--require-all") == 0) {
            options.require_all = 1;
            continue;
        }
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: convert option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--arch") == 0) options.architecture = args[++i];
        else if (strcmp(args[i], "--source-manifest") == 0) options.source_manifest_path = args[++i];
        else if (strcmp(args[i], "--native-source") == 0) options.native_source_dir = args[++i];
        else if (strcmp(args[i], "--template") == 0) options.template_path = args[++i];
        else if (strcmp(args[i], "--quant-policy") == 0) options.quant_policy_path = args[++i];
        else if (strcmp(args[i], "--imatrix-manifest") == 0) options.imatrix_manifest_path = args[++i];
        else if (strcmp(args[i], "--out") == 0) options.out_path = args[++i];
        else if (strcmp(args[i], "--out-plan") == 0) out_plan = args[++i];
        else if (strcmp(args[i], "--tensor") == 0) options.tensor_name = args[++i];
        else if (strcmp(args[i], "--target-qtype") == 0) options.target_qtype = args[++i];
        else if (strcmp(args[i], "--limit") == 0) {
            char *end = NULL;
            options.limit_tensors = strtoull(args[++i], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid convert limit\n");
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown convert option: %s\n", args[i]);
            return 2;
        }
    }

    if (strcmp(args[2], "plan") == 0) {
        if (!options.architecture || !options.native_source_dir || !out_plan) {
            fprintf(stderr, "yvex: convert plan requires --arch --native-source --out-plan\n");
            return 2;
        }
        rc = yvex_conversion_plan_write_json(&options, out_plan, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        fprintf(stdout, "conversion plan: written\n");
        fprintf(stdout, "architecture: %s\n", options.architecture);
        fprintf(stdout, "native_tensors: %llu\n", summary.native_tensor_count);
        fprintf(stdout, "planned_tensors: %llu\n", summary.planned_tensor_count);
        fprintf(stdout, "unmapped_tensors: %llu\n", summary.unmapped_tensor_count);
        fprintf(stdout, "unsupported_qtypes: %llu\n", summary.unsupported_qtype_count);
        fprintf(stdout, "out: %s\n", out_plan);
        fprintf(stdout, "status: conversion-plan-written\n");
        return 0;
    }

    if (!options.architecture || !options.native_source_dir || !options.tensor_name ||
        !options.target_qtype || !options.out_path) {
        fprintf(stderr, "yvex: convert emit requires --arch --native-source --tensor --target-qtype --out\n");
        return 2;
    }
    rc = yvex_conversion_emit_gguf(&options, &summary, &err);
    if (rc != YVEX_OK) {
        fprintf(stdout, "conversion emit: gguf\n");
        fprintf(stdout, "architecture: %s\n", options.architecture);
        fprintf(stdout, "source_tensor: %s\n", options.tensor_name);
        fprintf(stdout, "target_qtype: %s\n", options.target_qtype);
        fprintf(stdout, "status: conversion-failed\n");
        fprintf(stderr, "reason: %s\n", yvex_error_message(&err));
        return exit_for_status(rc);
    }
    fprintf(stdout, "conversion emit: gguf\n");
    fprintf(stdout, "architecture: %s\n", options.architecture);
    fprintf(stdout, "source_tensor: %s\n", options.tensor_name);
    fprintf(stdout, "target_qtype: %s\n", options.target_qtype);
    fprintf(stdout, "out: %s\n", options.out_path);
    fprintf(stdout, "bytes_read: %llu\n", summary.bytes_read);
    fprintf(stdout, "bytes_written: %llu\n", summary.bytes_written);
    fprintf(stdout, "roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    fprintf(stdout, "execution_ready: false\n");
    fprintf(stdout, "status: conversion-gguf-written\n");
    return 0;
}

static int parse_imatrix_create_options(int arg_count, char **args,
                                        yvex_imatrix_manifest_options *options,
                                        const char **out_path)
{
    int i = 3;

    while (i < arg_count) {
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--name") == 0) options->name = args[i + 1];
        else if (strcmp(args[i], "--arch") == 0) options->architecture = args[i + 1];
        else if (strcmp(args[i], "--source-manifest") == 0) options->source_manifest_path = args[i + 1];
        else if (strcmp(args[i], "--quant-policy") == 0) options->quant_policy_path = args[i + 1];
        else if (strcmp(args[i], "--imatrix") == 0) options->imatrix_path = args[i + 1];
        else if (strcmp(args[i], "--format") == 0) options->format = yvex_imatrix_format_from_name(args[i + 1]);
        else if (strcmp(args[i], "--status") == 0) options->status = yvex_imatrix_status_from_name(args[i + 1]);
        else if (strcmp(args[i], "--dataset") == 0) options->calibration_dataset = args[i + 1];
        else if (strcmp(args[i], "--command") == 0) options->calibration_command = args[i + 1];
        else if (strcmp(args[i], "--producer") == 0) options->producer = args[i + 1];
        else if (strcmp(args[i], "--out") == 0) *out_path = args[i + 1];
        else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int parse_imatrix_manifest_option(int arg_count, char **args, const char **manifest_path)
{
    int i = 3;

    while (i < arg_count) {
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--manifest") == 0) {
            *manifest_path = args[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void print_imatrix_summary(const char *mode,
                                  const char *manifest_path,
                                  const yvex_imatrix_summary *summary)
{
    fprintf(stdout, "imatrix: %s\n", mode);
    if (manifest_path) fprintf(stdout, "manifest: %s\n", manifest_path);
    fprintf(stdout, "name: %s\n", summary->name ? summary->name : "");
    fprintf(stdout, "architecture: %s\n", summary->architecture ? summary->architecture : "");
    fprintf(stdout, "format: %s\n", yvex_imatrix_format_name(summary->format));
    fprintf(stdout, "status: %s\n", yvex_imatrix_status_name(summary->status));
    fprintf(stdout, "file_exists: %s\n", summary->file_exists ? "yes" : "no");
    fprintf(stdout, "source_manifest: %s\n", summary->source_manifest_path ? summary->source_manifest_path : "");
    fprintf(stdout, "quant_policy: %s\n", summary->quant_policy_path ? summary->quant_policy_path : "");
    fprintf(stdout, "imatrix: %s\n", summary->imatrix_path ? summary->imatrix_path : "");
}

static int command_imatrix(int arg_count, char **args)
{
    yvex_error err;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_imatrix_summary summary;
    int rc;

    yvex_error_clear(&err);
    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_imatrix_help(stdout);
        return 0;
    }
    if (arg_count < 3) {
        fprintf(stderr, "yvex: imatrix requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(args[2], "create") == 0) {
        yvex_imatrix_manifest_options options;
        const char *out_path = NULL;

        memset(&options, 0, sizeof(options));
        rc = parse_imatrix_create_options(arg_count, args, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.imatrix_path || !out_path ||
            options.format == YVEX_IMATRIX_FORMAT_UNKNOWN ||
            options.status == YVEX_IMATRIX_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: imatrix create requires --name --arch --imatrix --format --status --out\n");
            return 2;
        }
        rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_imatrix_manifest_validate(manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_write_json(out_path, manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        fprintf(stdout, "imatrix manifest: written\n");
        fprintf(stdout, "name: %s\n", summary.name);
        fprintf(stdout, "architecture: %s\n", summary.architecture);
        fprintf(stdout, "format: %s\n", yvex_imatrix_format_name(summary.format));
        fprintf(stdout, "status: %s\n", yvex_imatrix_status_name(summary.status));
        fprintf(stdout, "file_exists: %s\n", summary.file_exists ? "yes" : "no");
        fprintf(stdout, "out: %s\n", out_path);
        fprintf(stdout, "status: imatrix-manifest-written\n");
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    if (strcmp(args[2], "inspect") == 0 || strcmp(args[2], "validate") == 0) {
        const char *manifest_path = NULL;

        rc = parse_imatrix_manifest_option(arg_count, args, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: imatrix %s requires --manifest FILE\n", args[2]);
            return 2;
        }
        rc = yvex_imatrix_manifest_open(&manifest, manifest_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(args[2], "validate") == 0) {
            rc = yvex_imatrix_manifest_validate(manifest, &err);
            if (rc != YVEX_OK) {
                yvex_imatrix_manifest_close(manifest);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        print_imatrix_summary(args[2], manifest_path, &summary);
        if (strcmp(args[2], "validate") == 0) {
            fprintf(stdout, "issues: %llu\n", summary.issue_count);
            fprintf(stdout, "requires_imatrix_rules: %llu\n", summary.requires_imatrix_rule_count);
            fprintf(stdout, "covered_rules: %llu\n", summary.covered_rule_count);
            fprintf(stdout, "uncovered_rules: %llu\n", summary.uncovered_rule_count);
            fprintf(stdout, "status: imatrix-%s\n",
                   summary.issue_count == 0 ? "valid" :
                   (summary.file_exists ? "partial" : "invalid"));
        } else {
            fprintf(stdout, "status: imatrix-manifest\n");
        }
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    fprintf(stderr, "yvex: unknown imatrix subcommand: %s\n", args[2]);
    return 2;
}

static int command_native_weights(int arg_count, char **args)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_summary summary;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.recursive = 1;

    if (arg_count == 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_native_weights_help(stdout);
        return 0;
    }

    i = 2;
    while (i < arg_count) {
        if (strcmp(args[i], "--" "json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: native-weights option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--source") == 0) {
            options.source_dir = args[i + 1];
        } else if (strcmp(args[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(args[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid native-weights limit: %s\n", args[i + 1]);
                return 2;
            }
        } else if (strcmp(args[i], "--tensor") == 0) {
            tensor_name = args[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown native-weights option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.source_dir) {
        fprintf(stderr, "yvex: native-weights requires --source DIR\n");
        return 2;
    }

    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_native_weight_table_summary(table, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(table);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (json) {
        fprintf(stdout, "{\n");
        fprintf(stdout, "  \"schema\": \"yvex.native_weights.v1\",\n");
        fprintf(stdout, "  \"source\": \"%s\",\n", options.source_dir);
        fprintf(stdout, "  \"summary\": {\n");
        fprintf(stdout, "    \"shard_count\": %llu,\n", summary.shard_count);
        fprintf(stdout, "    \"tensor_count\": %llu,\n", summary.tensor_count);
        fprintf(stdout, "    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
        fprintf(stdout, "    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
        fprintf(stdout, "    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
        fprintf(stdout, "  }\n");
        fprintf(stdout, "}\n");
        yvex_native_weight_table_close(table);
        return 0;
    }

    fprintf(stdout, "native weights: safetensors\n");
    fprintf(stdout, "source: %s\n", options.source_dir);
    fprintf(stdout, "shards: %llu\n", summary.shard_count);
    fprintf(stdout, "tensors: %llu\n", summary.tensor_count);
    fprintf(stdout, "total_tensor_bytes: %llu\n", summary.total_tensor_bytes);
    fprintf(stdout, "unknown_dtype_count: %llu\n", summary.unknown_dtype_count);
    fprintf(stdout, "malformed_shard_count: %llu\n", summary.malformed_shard_count);
    fprintf(stdout, "\n");

    if (tensor_name) {
        const yvex_native_weight_info *row = yvex_native_weight_table_find(table, tensor_name);
        if (!row) {
            yvex_native_weight_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        fprintf(stdout, "0 %s shard=%s dtype=%s rank=%u shape=",
               row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
        print_native_dims(row->dims, row->rank);
        fprintf(stdout, " bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
    } else {
        unsigned long long count = yvex_native_weight_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;
        for (idx = 0; idx < n; ++idx) {
            const yvex_native_weight_info *row = yvex_native_weight_table_at(table, idx);
            fprintf(stdout, "%llu %s shard=%s dtype=%s rank=%u shape=",
                   idx, row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
            print_native_dims(row->dims, row->rank);
            fprintf(stdout, " bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
        }
    }
    fprintf(stdout, "status: %s\n", summary.shard_count == 0 ? "native-weights-empty" : "native-weights");
    yvex_native_weight_table_close(table);
    return 0;
}

static int command_tensor_map(int arg_count, char **args)
{
    yvex_weight_mapping_options options;
    yvex_weight_mapping_table *table = NULL;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    unsigned long long mapped = 0;
    unsigned long long unmapped = 0;
    unsigned long long shape_mismatch = 0;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));

    if (arg_count == 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_tensor_map_help(stdout);
        return 0;
    }

    i = 2;
    while (i < arg_count) {
        if (strcmp(args[i], "--" "json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (strcmp(args[i], "--require-all-native-mapped") == 0) {
            options.require_all_native_mapped = 1;
            i++;
            continue;
        }
        if (strcmp(args[i], "--require-all-template-matched") == 0) {
            options.require_all_template_matched = 1;
            i++;
            continue;
        }
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: tensor-map option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--arch") == 0) {
            options.architecture = args[i + 1];
        } else if (strcmp(args[i], "--native-source") == 0) {
            options.native_source_dir = args[i + 1];
        } else if (strcmp(args[i], "--template") == 0) {
            options.template_path = args[i + 1];
            options.compare_template = 1;
        } else if (strcmp(args[i], "--tensor") == 0) {
            tensor_name = args[i + 1];
        } else if (strcmp(args[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(args[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid tensor-map limit: %s\n", args[i + 1]);
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown tensor-map option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.architecture || !options.native_source_dir) {
        fprintf(stderr, "yvex: tensor-map requires --arch NAME and --native-source DIR\n");
        return 2;
    }

    rc = yvex_weight_mapping_table_build(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    for (i = 0; (unsigned long long)i < yvex_weight_mapping_table_count(table); ++i) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, (unsigned long long)i);
        if (!row) continue;
        if (row->status == YVEX_WEIGHT_MAPPING_STATUS_MAPPED) mapped++;
        else if (row->status == YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH) shape_mismatch++;
        else unmapped++;
    }

    if (json) {
        fprintf(stdout, "{\n");
        fprintf(stdout, "  \"schema\": \"yvex.tensor_map.v1\",\n");
        fprintf(stdout, "  \"architecture\": \"%s\",\n", options.architecture);
        fprintf(stdout, "  \"native_source\": \"%s\",\n", options.native_source_dir);
        fprintf(stdout, "  \"native_tensors\": %llu,\n", yvex_weight_mapping_table_count(table));
        fprintf(stdout, "  \"mapped\": %llu,\n", mapped);
        fprintf(stdout, "  \"unmapped\": %llu,\n", unmapped);
        fprintf(stdout, "  \"shape_mismatch\": %llu\n", shape_mismatch);
        fprintf(stdout, "}\n");
        yvex_weight_mapping_table_close(table);
        return 0;
    }

    fprintf(stdout, "tensor map: %s\n", options.architecture);
    fprintf(stdout, "native_source: %s\n", options.native_source_dir);
    if (options.template_path) {
        fprintf(stdout, "template: %s\n", options.template_path);
    }
    fprintf(stdout, "native_tensors: %llu\n", yvex_weight_mapping_table_count(table));
    fprintf(stdout, "mapped: %llu\n", mapped);
    fprintf(stdout, "unmapped: %llu\n", unmapped);
    fprintf(stdout, "shape_mismatch: %llu\n", shape_mismatch);
    fprintf(stdout, "\n");

    if (tensor_name) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_find_native(table, tensor_name);
        if (!row) {
            yvex_weight_mapping_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        fprintf(stdout, "0 native=%s role=%s target=%s status=%s native_shape=",
               row->native_name, yvex_tensor_role_name(row->role), row->target_name,
               yvex_weight_mapping_status_name(row->status));
        print_native_dims(row->native_dims, row->native_rank);
        fprintf(stdout, " target_shape=");
        if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
        else fprintf(stdout, "unknown");
        fprintf(stdout, " transform=%s", row->requires_transpose ? "transpose" : "none");
        if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
            fprintf(stdout, " issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
        }
        fprintf(stdout, "\n");
    } else {
        unsigned long long count = yvex_weight_mapping_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;

        for (idx = 0; idx < n; ++idx) {
            const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, idx);
            fprintf(stdout, "%llu native=%s role=%s target=%s status=%s native_shape=",
                   idx, row->native_name, yvex_tensor_role_name(row->role), row->target_name,
                   yvex_weight_mapping_status_name(row->status));
            print_native_dims(row->native_dims, row->native_rank);
            fprintf(stdout, " target_shape=");
            if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
            else fprintf(stdout, "unknown");
            fprintf(stdout, " transform=%s", row->requires_transpose ? "transpose" : "none");
            if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
                fprintf(stdout, " issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
            }
            fprintf(stdout, "\n");
        }
    }
    fprintf(stdout, "status: tensor-map\n");
    yvex_weight_mapping_table_close(table);
    return 0;
}

static void print_quant_policy_rules(const yvex_quant_policy *policy)
{
    unsigned long long i;

    for (i = 0; i < yvex_quant_policy_rule_count(policy); ++i) {
        const yvex_quant_policy_rule *rule = yvex_quant_policy_rule_at(policy, i);
        if (!rule) continue;
        fprintf(stdout, "%llu selector=%s:%s qtype=%s storage_supported=%s compute_supported=%s requires_imatrix=%s\n",
               i,
               yvex_quant_selector_kind_name(rule->selector_kind),
               rule->selector,
               yvex_quant_qtype_name(rule->qtype),
               rule->storage_supported ? "yes" : "no",
               rule->compute_supported ? "yes" : "no",
               rule->requires_imatrix ? "yes" : "no");
    }
}

static int parse_quant_policy_common(int arg_count, char **args, int start,
                                     const char **policy_path,
                                     const char **template_path,
                                     const char **arch,
                                     const char **out_path)
{
    int i = start;

    while (i < arg_count) {
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: quant-policy option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--policy") == 0) {
            *policy_path = args[i + 1];
        } else if (strcmp(args[i], "--template") == 0) {
            *template_path = args[i + 1];
        } else if (strcmp(args[i], "--arch") == 0) {
            *arch = args[i + 1];
        } else if (strcmp(args[i], "--out") == 0) {
            *out_path = args[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown quant-policy option: %s\n", args[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int command_quant_policy(int arg_count, char **args)
{
    const char *policy_path = NULL;
    const char *template_path = NULL;
    const char *arch = NULL;
    const char *out_path = NULL;
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_quant_policy_help(stdout);
        return 0;
    }
    if (arg_count < 3) {
        fprintf(stderr, "yvex: quant-policy requires inspect, validate, or derive\n");
        return 2;
    }
    rc = parse_quant_policy_common(arg_count, args, 3, &policy_path, &template_path, &arch, &out_path);
    if (rc != 0) return rc;

    if (strcmp(args[2], "derive") == 0) {
        if (!template_path || !arch || !out_path) {
            fprintf(stderr, "yvex: quant-policy derive requires --template FILE --arch NAME --out FILE\n");
            return 2;
        }
        rc = yvex_quant_policy_create_from_template(&policy, template_path, arch, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_quant_policy_write_json(out_path, policy, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        fprintf(stdout, "quant policy: derived\n");
        fprintf(stdout, "architecture: %s\n", summary.architecture);
        fprintf(stdout, "template: %s\n", template_path);
        fprintf(stdout, "rules: %llu\n", summary.rule_count);
        fprintf(stdout, "requires_imatrix: %llu\n", summary.requires_imatrix_count);
        fprintf(stdout, "out: %s\n", out_path);
        fprintf(stdout, "status: quant-policy-written\n");
        yvex_quant_policy_close(policy);
        return 0;
    }

    if (strcmp(args[2], "inspect") == 0 || strcmp(args[2], "validate") == 0) {
        if (!policy_path) {
            fprintf(stderr, "yvex: quant-policy %s requires --policy FILE\n", args[2]);
            return 2;
        }
        rc = yvex_quant_policy_open(&policy, policy_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(args[2], "validate") == 0) {
            rc = yvex_quant_policy_validate(policy, template_path, &err);
            if (rc != YVEX_OK) {
                yvex_quant_policy_close(policy);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        fprintf(stdout, "quant policy: %s\n", args[2]);
        fprintf(stdout, "policy: %s\n", policy_path);
        if (template_path) fprintf(stdout, "template: %s\n", template_path);
        fprintf(stdout, "name: %s\n", summary.name);
        fprintf(stdout, "architecture: %s\n", summary.architecture);
        fprintf(stdout, "rules: %llu\n", summary.rule_count);
        fprintf(stdout, "issues: %llu\n", summary.issue_count);
        fprintf(stdout, "requires_imatrix: %llu\n", summary.requires_imatrix_count);
        fprintf(stdout, "storage_supported: %llu\n", summary.storage_supported_count);
        fprintf(stdout, "compute_supported: %llu\n", summary.compute_supported_count);
        fprintf(stdout, "\n");
        if (strcmp(args[2], "inspect") == 0) {
            print_quant_policy_rules(policy);
        }
        fprintf(stdout, "status: %s\n", yvex_quant_policy_status_name(summary.status));
        yvex_quant_policy_close(policy);
        return 0;
    }

    fprintf(stderr, "yvex: unknown quant-policy subcommand: %s\n", args[2]);
    return 2;
}

static int parse_quant_job_create_options(int arg_count, char **args,
                                          yvex_quant_job_options *options,
                                          const char **out_path)
{
    int i;

    memset(options, 0, sizeof(*options));
    *out_path = NULL;
    for (i = 3; i < arg_count; ++i) {
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--name") == 0) options->name = args[++i];
        else if (strcmp(args[i], "--arch") == 0) options->architecture = args[++i];
        else if (strcmp(args[i], "--tool") == 0) options->tool = yvex_quant_job_tool_from_name(args[++i]);
        else if (strcmp(args[i], "--tool-path") == 0) options->tool_path = args[++i];
        else if (strcmp(args[i], "--source-manifest") == 0) options->source_manifest_path = args[++i];
        else if (strcmp(args[i], "--native-source") == 0) options->native_source_dir = args[++i];
        else if (strcmp(args[i], "--template") == 0) options->template_path = args[++i];
        else if (strcmp(args[i], "--quant-policy") == 0) options->quant_policy_path = args[++i];
        else if (strcmp(args[i], "--imatrix-manifest") == 0) options->imatrix_manifest_path = args[++i];
        else if (strcmp(args[i], "--imatrix") == 0) options->imatrix_path = args[++i];
        else if (strcmp(args[i], "--out-gguf") == 0) options->out_gguf_path = args[++i];
        else if (strcmp(args[i], "--log") == 0) options->log_path = args[++i];
        else if (strcmp(args[i], "--status") == 0) options->status = yvex_quant_job_status_from_name(args[++i]);
        else if (strcmp(args[i], "--command") == 0) options->command = args[++i];
        else if (strcmp(args[i], "--out") == 0) *out_path = args[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", args[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_quant_job_manifest_option(int arg_count, char **args, const char **manifest_path)
{
    int i;

    *manifest_path = NULL;
    for (i = 3; i < arg_count; ++i) {
        if (i + 1 >= arg_count) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--manifest") == 0) *manifest_path = args[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", args[i]);
            return 2;
        }
    }
    return 0;
}

static void print_quant_job_summary(const char *mode,
                                    const char *path,
                                    const yvex_quant_job_summary *summary)
{
    fprintf(stdout, "quant job: %s\n", mode);
    if (path) fprintf(stdout, "manifest: %s\n", path);
    fprintf(stdout, "name: %s\n", summary->name ? summary->name : "");
    fprintf(stdout, "architecture: %s\n", summary->architecture ? summary->architecture : "");
    fprintf(stdout, "tool: %s\n", yvex_quant_job_tool_name(summary->tool));
    fprintf(stdout, "tool_path: %s\n", summary->tool_path ? summary->tool_path : "");
    fprintf(stdout, "native_source: %s\n", summary->native_source_dir ? summary->native_source_dir : "");
    fprintf(stdout, "template: %s\n", summary->template_path ? summary->template_path : "");
    fprintf(stdout, "out_gguf: %s\n", summary->out_gguf_path ? summary->out_gguf_path : "");
    fprintf(stdout, "log: %s\n", summary->log_path ? summary->log_path : "");
    fprintf(stdout, "tool_exists: %s\n", summary->tool_exists ? "yes" : "no");
    fprintf(stdout, "source_exists: %s\n", summary->source_exists ? "yes" : "no");
    fprintf(stdout, "template_exists: %s\n", summary->template_exists ? "yes" : "no");
    fprintf(stdout, "imatrix_exists: %s\n", summary->imatrix_exists ? "yes" : "no");
    fprintf(stdout, "output_exists: %s\n", summary->output_exists ? "yes" : "no");
    fprintf(stdout, "status: %s\n", yvex_quant_job_status_name(summary->status));
}

static int command_quant_job(int arg_count, char **args)
{
    yvex_quant_job_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&summary, 0, sizeof(summary));

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_quant_job_help(stdout);
        return 0;
    }
    if (arg_count < 3 || (strcmp(args[2], "create") != 0 &&
                     strcmp(args[2], "inspect") != 0 &&
                     strcmp(args[2], "validate") != 0)) {
        fprintf(stderr, "yvex: quant-job requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(args[2], "create") == 0) {
        yvex_quant_job_options options;
        const char *out_path = NULL;

        rc = parse_quant_job_create_options(arg_count, args, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.tool_path ||
            !options.native_source_dir || !options.template_path ||
            !options.out_gguf_path || !options.log_path || !options.command ||
            !out_path || options.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: quant-job create requires --name --arch --tool --tool-path --native-source --template --out-gguf --log --status --command --out\n");
            return 2;
        }
        rc = yvex_quant_job_write_json(out_path, &options, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        fprintf(stdout, "quant job: written\n");
        fprintf(stdout, "name: %s\n", summary.name);
        fprintf(stdout, "architecture: %s\n", summary.architecture);
        fprintf(stdout, "tool: %s\n", yvex_quant_job_tool_name(summary.tool));
        fprintf(stdout, "tool_exists: %s\n", summary.tool_exists ? "yes" : "no");
        fprintf(stdout, "source_exists: %s\n", summary.source_exists ? "yes" : "no");
        fprintf(stdout, "template_exists: %s\n", summary.template_exists ? "yes" : "no");
        fprintf(stdout, "imatrix_exists: %s\n", summary.imatrix_exists ? "yes" : "no");
        fprintf(stdout, "output_exists: %s\n", summary.output_exists ? "yes" : "no");
        fprintf(stdout, "status: %s\n", yvex_quant_job_status_name(summary.status));
        fprintf(stdout, "out: %s\n", out_path);
        fprintf(stdout, "status: quant-job-written\n");
        return 0;
    }

    {
        const char *manifest_path = NULL;

        rc = parse_quant_job_manifest_option(arg_count, args, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: quant-job %s requires --manifest FILE\n", args[2]);
            return 2;
        }
        rc = yvex_quant_job_validate(manifest_path, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        print_quant_job_summary(args[2], manifest_path, &summary);
        fprintf(stdout, "status: quant-job-%s\n", strcmp(args[2], "validate") == 0 ? "valid" : "manifest");
        return 0;
    }
}

int yvex_convert_command(int arg_count, char **args)
{
    return command_convert(arg_count, args);
}

int yvex_gguf_template_command(int arg_count, char **args)
{
    return command_gguf_template(arg_count, args);
}

int yvex_gguf_emit_command(int arg_count, char **args)
{
    return command_gguf_emit(arg_count, args);
}

int yvex_imatrix_command(int arg_count, char **args)
{
    return command_imatrix(arg_count, args);
}

int yvex_native_weights_command(int arg_count, char **args)
{
    return command_native_weights(arg_count, args);
}

int yvex_tensor_map_command(int arg_count, char **args)
{
    return command_tensor_map(arg_count, args);
}

int yvex_quant_job_command(int arg_count, char **args)
{
    return command_quant_job(arg_count, args);
}

int yvex_quant_policy_command(int arg_count, char **args)
{
    return command_quant_policy(arg_count, args);
}

int yvex_qtype_support_command(int arg_count, char **args)
{
    return command_qtype_support(arg_count, args);
}

void yvex_convert_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex convert plan --arch ARCH --native-source DIR --out-plan FILE\n");
    fprintf(fp, "       yvex convert emit --arch ARCH --native-source DIR --tensor NAME --target-qtype QTYPE --out FILE [--overwrite]\n");
    fprintf(fp, "\nConvert plans or emits selected open-weight GGUF tensor artifacts. It does not infer, execute a full model, or claim generation support.\n");
}

void yvex_gguf_template_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex gguf-template inspect|validate --template FILE\n");
    fprintf(fp, "       yvex gguf-template compare --template FILE --native-source DIR\n");
    fprintf(fp, "\nGGUF template validates metadata, tokenizer metadata, tensor directory, tensor roles, and optional exact-name native inventory comparison.\n");
}

void yvex_gguf_emit_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n\nGGUF emit writes a controlled YVEX-owned tensor artifact and validates the emitted file.\n");
}

void yvex_imatrix_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex imatrix create --name NAME --arch NAME --imatrix FILE --format FORMAT --status STATUS --out FILE\n");
    fprintf(fp, "       yvex imatrix inspect|validate --manifest FILE\n");
    fprintf(fp, "\nImatrix handles calibration artifact manifests. It does not generate imatrix data, calibrate, quantize, emit GGUF, materialize, or infer.\n");
}

void yvex_native_weights_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex native-weights --source DIR [--limit N] [--tensor NAME] [--" "json]\n\nNative weights reads safetensors headers and reports metadata only.\n");
}

void yvex_tensor_map_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--" "json]\n\nTensor map maps native safetensors names to canonical YVEX roles and proposed GGUF/template names.\n");
}

void yvex_quant_job_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex quant-job create --name NAME --arch ARCH --tool TOOL --tool-path FILE --native-source DIR --template FILE --out-gguf FILE --log FILE --status STATUS --command TEXT --out FILE\n");
    fprintf(fp, "       yvex quant-job inspect|validate --manifest FILE\n");
    fprintf(fp, "\nQuant job records an external quantization/conversion job manifest without running arbitrary tools.\n");
}

void yvex_quant_policy_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex quant-policy inspect|validate --policy FILE [--template FILE]\n");
    fprintf(fp, "       yvex quant-policy derive --template FILE --arch NAME --out FILE\n");
    fprintf(fp, "\nQuant policy handles declarative qtype policy manifests. It does not quantize tensors or infer.\n");
}

void yvex_qtype_support_help(FILE *fp)
{
    fprintf(fp, "usage: " "yvex qtype-support\n\nReports policy/storage/emit/quantize/compute support separately. Compute support is not implied by conversion support.\n");
}
