/*
 * gguf/tools.c - GGUF template validation and controlled emission.
 *
 * This file owns GGUF template comparison and selected-tensor GGUF writing.
 */

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
    printf("gguf emit: controlled\n");
    printf("out: %s\n", summary->out_path ? summary->out_path : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("model_name: %s\n", summary->model_name ? summary->model_name : "");
    printf("metadata_count: %llu\n", summary->metadata_count);
    printf("tensor_count: %llu\n", summary->tensor_count);
    printf("tensor_payload_bytes: %llu\n", summary->tensor_payload_bytes);
    printf("alignment: %llu\n", summary->alignment);
    printf("roundtrip_validated: %s\n", summary->roundtrip_validated ? "yes" : "no");
    printf("status: %s\n", yvex_gguf_emit_status_name(summary->status));
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
    printf("gguf template: %s\n", mode);
    printf("template: %s\n", template_path ? template_path : "");
    printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
    printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
    printf("known_roles: %llu\n", summary.known_role_count);
    printf("unknown_roles: %llu\n", summary.unknown_role_count);
    printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
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
