/* Owner: GGUF tooling domain.
 * Owns: template comparison and controlled selected-tensor emission algorithms.
 * Does not own: command argument parsing, operator rendering, full artifact writing, or runtime.
 * Invariants: command grammar and stdout/stderr never enter this owner.
 * Boundary: typed tool APIs and explicit artifact-file IO only.
 * Purpose: implement bounded proof-artifact emission and immutable GGUF template comparison.
 * Inputs: typed emit/template options, canonical qtype facts, and admitted file paths.
 * Effects: owns explicit proof files or read-only template lifecycles and typed summaries.
 * Failure: I/O, geometry, structural, allocation, or comparison refusal cleans owned resources. */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/gguf.h>
#include <yvex/internal/core.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/model.h>
#include <yvex/qtype.h>
#include <yvex/source.h>
#include <yvex/tokenizer.h>

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

static int template_validate(yvex_gguf_template *tmpl,
                                       const yvex_gguf_template_options *options, yvex_error *err);

static int template_compare_native(yvex_gguf_template *tmpl,
                                             const yvex_gguf_template_options *options,
                                             yvex_error *err);

static int template_add_issue(yvex_gguf_template *tmpl,
                                        yvex_gguf_template_issue_kind kind, const char *tensor_name,
                                        const char *message, yvex_error *err);

/* Controlled GGUF emission */

#define YVEX_GGUF_EMIT_ALIGNMENT 32ull
#define YVEX_GGUF_EMIT_METADATA_COUNT 12ull
#define YVEX_GGUF_EMIT_TENSOR_COUNT 1ull
#define YVEX_GGUF_EMIT_NATIVE_ROWS 8u
#define YVEX_GGUF_EMIT_NATIVE_COLS 4u
#define YVEX_GGUF_EMIT_TENSOR_FLOATS 32u

static const char *const emit_status_names[] = {
    "gguf-unknown", "gguf-planned", "gguf-written", "gguf-failed",
};
static const char *const template_status_names[] = {
    "template-unknown", "template-valid", "template-partial", "template-invalid",
};
static const char *const template_issue_names[] = {
    "none", "missing_architecture", "missing_model_name", "missing_tokenizer",
    "empty_tensor_directory", "unknown_tensor_role", "native_missing_tensor",
    "native_shape_mismatch", "unsupported_dtype", "format",
};

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

static int emit_write_metadata(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                         yvex_error *err);
static int emit_write_tensor_dir(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                           yvex_error *err);
static int emit_write_tensor_payload(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                               yvex_error *err);

static int emit_write_u32(FILE *fp, unsigned int value, yvex_error *err,
                                    const char *field);
static int emit_write_u64(FILE *fp, unsigned long long value, yvex_error *err,
                                    const char *field);
static int emit_write_i32(FILE *fp, int value, yvex_error *err, const char *field);
static int emit_write_f32(FILE *fp, float value, yvex_error *err, const char *field);
static int emit_write_u16(FILE *fp, unsigned int value, yvex_error *err,
                                    const char *field);
static int emit_write_string(FILE *fp, const char *value, yvex_error *err,
                                       const char *field);
static int emit_pad_to_alignment(FILE *fp, unsigned long long alignment, yvex_error *err);

/* Purpose: test whether a candidate controlled-emission destination already exists.
 * Inputs: immutable path.
 * Effects: opens and closes a read-only stream when present.
 * Failure: any open failure reports absent to the caller's overwrite policy.
 * Boundary: this probe does not admit path safety or replace files. */
static int path_exists(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    fclose(fp);
    return 1;
}

/* Purpose: derive the admitted scalar qtype and byte width for controlled fixture emission.
 * Inputs: optional qtype name and writable numeric outputs.
 * Effects: writes outputs only for canonical F32 or F16 scalar geometry.
 * Failure: unknown, refused, block, or widthless qtypes return false.
 * Boundary: selection is restricted to the historical one-tensor proof path. */
static int controlled_qtype_to_plan(const char *qtype, unsigned int *ggml_type,
                                    unsigned long long *scalar_bytes) {
    const yvex_gguf_qtype_geometry *geometry =
        yvex_gguf_qtype_geometry_find_by_name(qtype ? qtype : "F32");

    if (!geometry || !yvex_gguf_qtype_supported_for_storage(geometry->qtype, NULL) ||
        geometry->scalar_width == 0u ||
        (geometry->qtype != YVEX_GGUF_QTYPE_F32 && geometry->qtype != YVEX_GGUF_QTYPE_F16))
        return 0;
    *ggml_type = geometry->qtype;
    *scalar_bytes = geometry->scalar_width;
    return 1;
}

/* Purpose: measure a stream length while restoring its original position.
 * Inputs: open seekable stream.
 * Effects: performs bounded seek/tell operations and restores position.
 * Failure: any tell or seek failure returns -1.
 * Boundary: long-based measurement is confined to the tiny controlled fixture. */
static long file_size(FILE *fp) {
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

/* Purpose: reopen and materialize the controlled one-tensor artifact as a bounded roundtrip proof.
 * Inputs: emitted fixture path and diagnostics.
 * Effects: owns and deterministically closes artifact, reader, descriptor, backend, and weights.
 * Failure: the first lifecycle refusal is returned after complete cleanup.
 * Boundary: fixture roundtrip is not complete-model artifact or runtime support. */
static int validate_roundtrip(const char *path, yvex_error *err) {
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
    if (rc == YVEX_OK)
        rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK)
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc == YVEX_OK)
        rc = yvex_model_descriptor_from_gguf(&model, gguf, tensors, err);
    if (rc == YVEX_OK)
        rc = yvex_backend_open_cpu(&backend, err);
    if (rc == YVEX_OK) {
        rc = yvex_weight_table_materialize(&weights, artifact, gguf, tensors, backend,
                                           &materialize_options, err);
    }

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_model_descriptor_close(model);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

/* Purpose: emit one deterministic selected-tensor GGUF proof through the typed tool API.
 * Inputs: controlled options plus writable summary and diagnostics.
 * Effects: creates/writes the requested proof file, flushes it, and validates one roundtrip.
 * Failure: unsafe overwrite, unsupported qtype, I/O, or roundtrip refusal leaves failed summary.
 * Boundary: controlled emission cannot enter the complete-artifact support path. */
int yvex_gguf_emit_controlled(const yvex_gguf_emit_options *options,
                              yvex_gguf_emit_summary *summary_out, yvex_error *err) {
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
    summary.tensor_payload_bytes =
        (unsigned long long)YVEX_GGUF_EMIT_TENSOR_FLOATS * plan.scalar_bytes;
    summary.alignment = YVEX_GGUF_EMIT_ALIGNMENT;

    fp = fopen(plan.out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_gguf_emit_controlled",
                        "failed to open output %s: %s", plan.out_path, strerror(errno));
        summary.status = YVEX_GGUF_EMIT_STATUS_FAILED;
        *summary_out = summary;
        return YVEX_ERR_IO;
    }

    rc = emit_write_u32(fp, YVEX_GGUF_MAGIC, err, "magic");
    if (rc == YVEX_OK)
        rc = emit_write_u32(fp, 3u, err, "version");
    if (rc == YVEX_OK)
        rc = emit_write_u64(fp, YVEX_GGUF_EMIT_TENSOR_COUNT, err, "tensor count");
    if (rc == YVEX_OK)
        rc = emit_write_u64(fp, YVEX_GGUF_EMIT_METADATA_COUNT, err, "metadata count");
    if (rc == YVEX_OK)
        rc = emit_write_metadata(fp, &plan, err);
    if (rc == YVEX_OK)
        rc = emit_write_tensor_dir(fp, &plan, err);
    if (rc == YVEX_OK)
        rc = emit_pad_to_alignment(fp, YVEX_GGUF_EMIT_ALIGNMENT, err);
    if (rc == YVEX_OK)
        rc = emit_write_tensor_payload(fp, &plan, err);

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

/* Purpose: write one canonical little-endian u32 field to the controlled stream.
 * Inputs: stream, value, diagnostics, and semantic field name.
 * Effects: writes exactly four bytes or records I/O refusal.
 * Failure: partial/hard write returns I/O status.
 * Boundary: primitive serialization owns no stream lifecycle. */
static int emit_write_u32(FILE *fp, unsigned int value, yvex_error *err,
                                    const char *field) {
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

/* Purpose: write one canonical little-endian u64 field to the controlled stream.
 * Inputs: stream, value, diagnostics, and semantic field name.
 * Effects: writes exactly eight bytes or records I/O refusal.
 * Failure: partial/hard write returns I/O status.
 * Boundary: primitive serialization owns no offset policy. */
static int emit_write_u64(FILE *fp, unsigned long long value, yvex_error *err,
                                    const char *field) {
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

/* Purpose: serialize one signed I32 through the canonical unsigned byte primitive.
 * Inputs: stream, value, diagnostics, and field name.
 * Effects: writes exactly four little-endian bytes.
 * Failure: delegated primitive write refusal returns I/O status.
 * Boundary: conversion preserves the two's-complement bit pattern. */
static int emit_write_i32(FILE *fp, int value, yvex_error *err, const char *field) {
    return emit_write_u32(fp, (unsigned int)(uint32_t)(int32_t)value, err, field);
}

/* Purpose: serialize one F32 bit pattern through the canonical unsigned byte primitive.
 * Inputs: stream, scalar, diagnostics, and field name.
 * Effects: writes exactly four IEEE bit-pattern bytes.
 * Failure: delegated primitive write refusal returns I/O status.
 * Boundary: no numeric conversion or finite policy is applied. */
static int emit_write_f32(FILE *fp, float value, yvex_error *err, const char *field) {
    uint32_t raw;
    memcpy(&raw, &value, sizeof(raw));
    return emit_write_u32(fp, raw, err, field);
}

/* Purpose: write one canonical little-endian u16 field to the controlled stream.
 * Inputs: stream, bounded value, diagnostics, and semantic field name.
 * Effects: writes exactly two bytes.
 * Failure: partial/hard write returns I/O status.
 * Boundary: the caller owns scalar interpretation. */
static int emit_write_u16(FILE *fp, unsigned int value, yvex_error *err,
                                    const char *field) {
    unsigned char b[2];
    b[0] = (unsigned char)(value & 0xffu);
    b[1] = (unsigned char)((value >> 8) & 0xffu);
    if (fwrite(b, 1u, sizeof(b), fp) != sizeof(b)) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: write one GGUF length-delimited string to the controlled stream.
 * Inputs: stream, optional text, diagnostics, and semantic field name.
 * Effects: writes canonical u64 length followed by exact bytes.
 * Failure: length or content write refusal returns I/O status.
 * Boundary: string validation belongs to the caller's metadata contract. */
static int emit_write_string(FILE *fp, const char *value, yvex_error *err,
                                       const char *field) {
    unsigned long long len;
    if (!value) {
        value = "";
    }
    len = (unsigned long long)strlen(value);
    if (emit_write_u64(fp, len, err, field) != YVEX_OK) {
        return YVEX_ERR_IO;
    }
    if (len > 0 && fwrite(value, 1u, (size_t)len, fp) != (size_t)len) {
        yvex_error_setf(err, YVEX_ERR_IO, "gguf_emit.write", "failed to write %s", field);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: write one GGUF metadata key and its scalar or array type tag.
 * Inputs: stream, canonical key, type ID, and diagnostics.
 * Effects: writes one length-delimited key followed by u32 type.
 * Failure: either primitive write refusal returns I/O status.
 * Boundary: caller owns type/key semantic admission. */
static int write_key_type(FILE *fp, const char *key, unsigned int type, yvex_error *err) {
    if (emit_write_string(fp, key, err, "metadata key") != YVEX_OK)
        return YVEX_ERR_IO;
    return emit_write_u32(fp, type, err, "metadata type");
}

/* Purpose: serialize one string-valued metadata entry in canonical order.
 * Inputs: stream, key, value, and diagnostics.
 * Effects: writes key/type then length-delimited value.
 * Failure: delegated write refusal returns I/O status.
 * Boundary: this helper does not deduplicate metadata keys. */
static int write_string_meta(FILE *fp, const char *key, const char *value, yvex_error *err) {
    if (write_key_type(fp, key, GGUF_VALUE_STRING, err) != YVEX_OK)
        return YVEX_ERR_IO;
    return emit_write_string(fp, value, err, key);
}

/* Purpose: serialize one u32-valued metadata entry in canonical order.
 * Inputs: stream, key, value, and diagnostics.
 * Effects: writes key/type then exact u32 value.
 * Failure: delegated write refusal returns I/O status.
 * Boundary: caller owns metadata value semantics. */
static int write_u32_meta(FILE *fp, const char *key, unsigned int value, yvex_error *err) {
    if (write_key_type(fp, key, GGUF_VALUE_UINT32, err) != YVEX_OK)
        return YVEX_ERR_IO;
    return emit_write_u32(fp, value, err, key);
}

/* Purpose: serialize one fixed-count I32 metadata array.
 * Inputs: stream, key, values/count, and diagnostics.
 * Effects: writes array type/count followed by every ordered element.
 * Failure: the first primitive write refusal aborts the entry.
 * Boundary: caller owns array cardinality and metadata policy. */
static int write_i32_array(FILE *fp, const char *key, const int *values, unsigned long long count,
                           yvex_error *err) {
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u32(fp, GGUF_VALUE_INT32, err, "array element type") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u64(fp, count, err, "array count") != YVEX_OK)
        return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (emit_write_i32(fp, values[i], err, key) != YVEX_OK)
            return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: serialize one fixed-count F32 metadata array.
 * Inputs: stream, key, values/count, and diagnostics.
 * Effects: writes array type/count followed by exact scalar bit patterns.
 * Failure: the first primitive write refusal aborts the entry.
 * Boundary: no numeric conversion is performed. */
static int write_f32_array(FILE *fp, const char *key, const float *values, unsigned long long count,
                           yvex_error *err) {
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u32(fp, GGUF_VALUE_FLOAT32, err, "array element type") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u64(fp, count, err, "array count") != YVEX_OK)
        return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (emit_write_f32(fp, values[i], err, key) != YVEX_OK)
            return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: serialize one fixed-count string metadata array.
 * Inputs: stream, key, ordered strings/count, and diagnostics.
 * Effects: writes array type/count and length-delimited elements.
 * Failure: the first primitive write refusal aborts the entry.
 * Boundary: this helper owns no tokenizer semantics. */
static int write_string_array(FILE *fp, const char *key, const char *const *values,
                              unsigned long long count, yvex_error *err) {
    unsigned long long i;
    if (write_key_type(fp, key, GGUF_VALUE_ARRAY, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u32(fp, GGUF_VALUE_STRING, err, "array element type") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u64(fp, count, err, "array count") != YVEX_OK)
        return YVEX_ERR_IO;
    for (i = 0; i < count; ++i) {
        if (emit_write_string(fp, values[i], err, key) != YVEX_OK)
            return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: emit the closed metadata set required by the controlled one-tensor fixture.
 * Inputs: stream, immutable emit plan, and diagnostics.
 * Effects: writes twelve deterministic metadata entries in canonical order.
 * Failure: any child serialization failure stops emission.
 * Boundary: fixture metadata is not the complete DeepSeek metadata contract. */
static int emit_write_metadata(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                         yvex_error *err) {
    const char *tokens[8] = {"<unk>", "<s>", "</s>", "a", "b", "c", "d", "e"};
    float scores[8] = {0.0f, 0.0f, 0.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f};
    int token_types[8] = {2, 3, 3, 1, 1, 1, 1, 1};

    if (write_string_meta(fp, "general.architecture", plan->architecture, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_string_meta(fp, "general.name", plan->model_name, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "llama.context_length", 8u, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "general.file_type", plan->ggml_type, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "general.alignment", (unsigned int)YVEX_GGUF_EMIT_ALIGNMENT, err) !=
        YVEX_OK)
        return YVEX_ERR_IO;
    if (write_string_meta(fp, "tokenizer.ggml.model", "llama", err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_string_array(fp, "tokenizer.ggml.tokens", tokens, 8ull, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_f32_array(fp, "tokenizer.ggml.scores", scores, 8ull, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_i32_array(fp, "tokenizer.ggml.token_type", token_types, 8ull, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.bos_token_id", 1u, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.eos_token_id", 2u, err) != YVEX_OK)
        return YVEX_ERR_IO;
    if (write_u32_meta(fp, "tokenizer.ggml.unknown_token_id", 0u, err) != YVEX_OK)
        return YVEX_ERR_IO;
    return YVEX_OK;
}

/* Purpose: render controlled-emission lifecycle status as stable diagnostic text.
 * Inputs: emit-status enum.
 * Effects: none.
 * Failure: out-of-range values yield gguf-unknown.
 * Boundary: status text cannot classify an artifact. */
const char *yvex_gguf_emit_status_name(yvex_gguf_emit_status status) {
    return status >= YVEX_GGUF_EMIT_STATUS_UNKNOWN &&
                   (size_t)status < sizeof(emit_status_names) / sizeof(emit_status_names[0])
               ? emit_status_names[status]
               : emit_status_names[YVEX_GGUF_EMIT_STATUS_UNKNOWN];
}

/* Purpose: write zero bytes through the next requested alignment boundary.
 * Inputs: current stream, nonzero alignment, and diagnostics.
 * Effects: appends bounded zero chunks until aligned.
 * Failure: offset query or write failure returns I/O status.
 * Boundary: the caller owns alignment admission and final layout validation. */
static int emit_pad_to_alignment(FILE *fp, unsigned long long alignment,
                                           yvex_error *err) {
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
            yvex_error_set(err, YVEX_ERR_IO, "gguf_emit.align",
                           "failed to write alignment padding");
            return YVEX_ERR_IO;
        }
        pad -= (unsigned long long)chunk;
    }
    return YVEX_OK;
}

/* Purpose: write the single canonical tensor-directory row for the controlled fixture.
 * Inputs: stream, immutable emit plan, and diagnostics.
 * Effects: writes name, rank, dimensions, qtype, and zero relative offset.
 * Failure: any primitive write refusal aborts the directory.
 * Boundary: directory shape is fixed proof data, not family lowering truth. */
static int emit_write_tensor_dir(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                           yvex_error *err) {
    if (emit_write_string(fp, plan->target_name, err, "tensor name") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u32(fp, 2u, err, "tensor rank") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u64(fp, 4ull, err, "tensor dim 0") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u64(fp, 8ull, err, "tensor dim 1") != YVEX_OK)
        return YVEX_ERR_IO;
    if (emit_write_u32(fp, plan->ggml_type, err, "tensor ggml type") != YVEX_OK)
        return YVEX_ERR_IO;
    return emit_write_u64(fp, 0ull, err, "tensor relative offset");
}

/* Purpose: write deterministic controlled tensor values in the selected physical orientation.
 * Inputs: stream, scalar F32/F16 emit plan, and diagnostics.
 * Effects: generates and writes exactly 32 fixture values.
 * Failure: scalar write refusal aborts payload emission.
 * Boundary: generated proof values are never model weights. */
static int emit_write_tensor_payload(FILE *fp, const yvex_gguf_emit_plan_data *plan,
                                               yvex_error *err) {
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
                if (plan->ggml_type == YVEX_GGUF_QTYPE_F16) {
                    if (emit_write_u16(fp, yvex_quant_f16_encode(native[row][col]),
                                                 err, "tensor payload") != YVEX_OK) {
                        return YVEX_ERR_IO;
                    }
                } else if (emit_write_f32(fp, native[row][col], err, "tensor payload") !=
                           YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    } else {
        for (row = 0; row < YVEX_GGUF_EMIT_NATIVE_ROWS; ++row) {
            for (col = 0; col < YVEX_GGUF_EMIT_NATIVE_COLS; ++col) {
                if (plan->ggml_type == YVEX_GGUF_QTYPE_F16) {
                    if (emit_write_u16(fp, yvex_quant_f16_encode(native[row][col]),
                                                 err, "tensor payload") != YVEX_OK) {
                        return YVEX_ERR_IO;
                    }
                } else if (emit_write_f32(fp, native[row][col], err, "tensor payload") !=
                           YVEX_OK) {
                    return YVEX_ERR_IO;
                }
            }
        }
    }

    return YVEX_OK;
}

/* Purpose: open one template artifact as a read-only mapped snapshot.
 * Inputs: template path, writable artifact owner, and diagnostics.
 * Effects: delegates one artifact lifecycle acquisition.
 * Failure: path/admission/I/O refusal leaves output unowned.
 * Boundary: mapping does not parse or validate GGUF content. */
static int gt_open_artifact(const char *path, yvex_artifact **artifact, yvex_error *err) {
    yvex_artifact_options options;

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;
    options.map = 1;
    return yvex_artifact_open(artifact, &options, err);
}

/* Purpose: render template lifecycle status as stable diagnostic text.
 * Inputs: template-status enum.
 * Effects: none.
 * Failure: out-of-range values yield template-unknown.
 * Boundary: text does not create template admission. */
const char *yvex_gguf_template_status_name(yvex_gguf_template_status status) {
    return status >= YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN &&
                   (size_t)status < sizeof(template_status_names) / sizeof(template_status_names[0])
               ? template_status_names[status]
               : template_status_names[YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN];
}

/* Purpose: render one typed template issue kind as stable diagnostic text.
 * Inputs: issue-kind enum.
 * Effects: none.
 * Failure: out-of-range values yield format.
 * Boundary: issue text remains a projection of typed facts. */
const char *yvex_gguf_template_issue_kind_name(yvex_gguf_template_issue_kind kind) {
    return kind >= YVEX_GGUF_TEMPLATE_ISSUE_NONE &&
                   (size_t)kind < sizeof(template_issue_names) / sizeof(template_issue_names[0])
               ? template_issue_names[kind]
               : template_issue_names[YVEX_GGUF_TEMPLATE_ISSUE_FORMAT];
}

/* Purpose: append one independently owned typed issue to a template result.
 * Inputs: template, issue kind, optional tensor name/message, and diagnostics.
 * Effects: grows issue storage and owns copied strings on success.
 * Failure: invalid template, capacity overflow, or allocation leaves count unchanged.
 * Boundary: issue accumulation cannot promote template status. */
static int template_add_issue(yvex_gguf_template *tmpl,
                                        yvex_gguf_template_issue_kind kind, const char *tensor_name,
                                        const char *message, yvex_error *err) {
    yvex_gguf_template_issue *next;
    yvex_gguf_template_issue *issue;

    if (!tmpl) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_issue", "template is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tmpl->issue_count == tmpl->issue_cap) {
        unsigned long long cap = tmpl->issue_cap == 0 ? 8u : tmpl->issue_cap * 2u;
        next = (yvex_gguf_template_issue *)realloc(tmpl->issues,
                                                   (size_t)cap * sizeof(tmpl->issues[0]));
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
    issue->tensor_name = yvex_core_strdup(tensor_name);
    issue->message = yvex_core_strdup(message);
    if (!issue->tensor_name || !issue->message) {
        free((char *)issue->tensor_name);
        free((char *)issue->message);
        memset(issue, 0, sizeof(*issue));
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_issue",
                       "issue string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    tmpl->issue_count++;
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}

/* Purpose: open, parse, validate, and optionally compare one GGUF template snapshot.
 * Inputs: output owner slot, immutable template options, and diagnostics.
 * Effects: owns artifact, GGUF, tensor/model/tokenizer/native views until close.
 * Failure: any acquisition/validation refusal unwinds all partial ownership.
 * Boundary: template admission remains engineering evidence, not a supported artifact. */
int yvex_gguf_template_open(yvex_gguf_template **out, const yvex_gguf_template_options *options,
                            yvex_error *err) {
    yvex_gguf_template *tmpl;
    int rc;

    if (!out || !options || !options->template_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_open",
                       "out and template_path are required");
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
    if (rc == YVEX_OK)
        rc = yvex_gguf_open(&tmpl->gguf, tmpl->artifact, err);
    if (rc == YVEX_OK)
        rc = yvex_tensor_table_from_gguf(&tmpl->tensors, tmpl->gguf, err);
    if (rc == YVEX_OK)
        rc = yvex_model_descriptor_from_gguf(&tmpl->model, tmpl->gguf, tmpl->tensors, err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    rc = template_validate(tmpl, options, err);
    if (rc == YVEX_OK && options->compare_native) {
        rc = template_compare_native(tmpl, options, err);
    }
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    *out = tmpl;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: release the complete template lifecycle and every accumulated issue.
 * Inputs: optional owned template.
 * Effects: frees issue strings/arrays and closes all nested model/artifact owners.
 * Failure: none; null close is safe.
 * Boundary: no external path or native source ownership is retained. */
void yvex_gguf_template_close(yvex_gguf_template *tmpl) {
    unsigned long long i;

    if (!tmpl)
        return;
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

/* Purpose: copy the immutable template summary into caller-owned storage.
 * Inputs: template, writable summary, and diagnostics.
 * Effects: replaces the output summary.
 * Failure: null inputs return invalid argument.
 * Boundary: summary projection transfers no nested ownership. */
int yvex_gguf_template_get_summary(const yvex_gguf_template *tmpl, yvex_gguf_template_summary *out,
                                   yvex_error *err) {
    if (!tmpl || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_summary",
                       "template and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = tmpl->summary;
    return YVEX_OK;
}

/* Purpose: expose accumulated template issue cardinality.
 * Inputs: optional immutable template.
 * Effects: none.
 * Failure: null template yields zero.
 * Boundary: the count includes fatal and advisory typed issues. */
unsigned long long yvex_gguf_template_issue_count(const yvex_gguf_template *tmpl) {
    return tmpl ? tmpl->issue_count : 0;
}

/* Purpose: borrow one immutable template issue by ordinal.
 * Inputs: template and zero-based issue index.
 * Effects: none.
 * Failure: null template or out-of-range index returns null.
 * Boundary: issue text remains template-owned. */
const yvex_gguf_template_issue *yvex_gguf_template_issue_at(const yvex_gguf_template *tmpl,
                                                            unsigned long long index) {
    if (!tmpl || index >= tmpl->issue_count)
        return NULL;
    return &tmpl->issues[index];
}

/* Purpose: compare one parsed template tensor shape with one native inventory shape. */
static int gt_same_shape(const yvex_tensor_info *tensor, const yvex_native_weight_info *native) {
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

/* Purpose: compare every template tensor with the exact-name native source inventory.
 * Inputs: mutable template owner, comparison options, and diagnostics.
 * Effects: opens native inventory, records counts/issues, and updates template status.
 * Failure: missing source, inventory failure, or issue allocation returns typed status.
 * Boundary: comparison reads headers only and does not map or convert payload bytes. */
static int template_compare_native(yvex_gguf_template *tmpl,
                                             const yvex_gguf_template_options *options,
                                             yvex_error *err) {
    yvex_native_weight_options native_options;
    yvex_native_weight_summary native_summary;
    unsigned long long i;
    int rc;

    if (!options->native_source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_compare",
                       "native_source_dir is required");
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

        if (!tensor)
            continue;
        native = yvex_native_weight_table_find(tmpl->native, tensor->name);
        if (!native) {
            tmpl->missing_in_native++;
            rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR,
                                              tensor->name,
                                              "template tensor missing in native inventory; "
                                              "open-weight intake mapping may be required",
                                              err);
            if (rc != YVEX_OK)
                return rc;
            continue;
        }
        tmpl->matched_exact++;
        if (!gt_same_shape(tensor, native)) {
            char message[192];
            tmpl->shape_mismatch++;
            snprintf(message, sizeof(message),
                     "native/template shape mismatch for exact-name tensor");
            rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH,
                                              tensor->name, message, err);
            if (rc != YVEX_OK)
                return rc;
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

/* Purpose: copy one GGUF string metadata value into independent template ownership.
 * Inputs: parsed GGUF view and exact metadata key.
 * Effects: allocates and copies the string when present and well typed.
 * Failure: missing/type mismatch/allocation returns null.
 * Boundary: caller owns returned storage. */
static char *gt_copy_string_value(const yvex_gguf *gguf, const char *key) {
    const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, key);
    const char *data;
    unsigned long long len;
    char *out;

    if (!value || yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK) {
        return NULL;
    }
    out = (char *)malloc((size_t)len + 1u);
    if (!out)
        return NULL;
    memcpy(out, data, (size_t)len);
    out[len] = '\0';
    return out;
}

/* Purpose: count tokenizer-prefixed metadata keys in one parsed GGUF view. */
static unsigned long long gt_tokenizer_metadata_count(const yvex_gguf *gguf) {
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

/* Purpose: validate template architecture, tokenizer, directory, roles, and admitted dtypes.
 * Inputs: mutable template owner, validation options, and diagnostics.
 * Effects: accumulates issues and seals valid/partial/invalid summary status.
 * Failure: issue allocation or malformed required content returns typed refusal.
 * Boundary: structural template validity is not complete model support. */
static int template_validate(yvex_gguf_template *tmpl,
                                       const yvex_gguf_template_options *options, yvex_error *err) {
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
        rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE, "",
                                          "general.architecture missing", err);
        if (rc != YVEX_OK)
            return rc;
        fatal = 1;
    }
    if (!tmpl->model_name || tmpl->model_name[0] == '\0') {
        rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME, "",
                                          "general.name missing", err);
        if (rc != YVEX_OK)
            return rc;
        partial = 1;
    }
    if (!tmpl->summary.has_tokenizer) {
        rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER, "",
                                          "tokenizer.ggml.model or tokenizer.ggml.tokens missing",
                                          err);
        if (rc != YVEX_OK)
            return rc;
        if (options->require_tokenizer) {
            fatal = 1;
        } else {
            partial = 1;
        }
    }
    if (!tmpl->summary.has_tensor_directory) {
        rc = template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY, "",
                                          "tensor directory is empty", err);
        if (rc != YVEX_OK)
            return rc;
        fatal = 1;
    }

    for (i = 0; i < yvex_tensor_table_count(tmpl->tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tmpl->tensors, i);
        if (!tensor)
            continue;
        if (tensor->role == YVEX_TENSOR_ROLE_UNKNOWN) {
            tmpl->summary.unknown_role_count++;
            rc = template_add_issue(
                tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE, tensor->name,
                "tensor role is unknown before open-weight intake mapping", err);
            if (rc != YVEX_OK)
                return rc;
            partial = 1;
        } else {
            tmpl->summary.known_role_count++;
        }
        if (tensor->storage_bytes == 0) {
            rc = template_add_issue(
                tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE, tensor->name,
                "tensor dtype has unsupported storage accounting", err);
            if (rc != YVEX_OK)
                return rc;
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
