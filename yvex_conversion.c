/* ===== inlined yvex_weight_mapping_internal.h ===== */

/*
 * YVEX - Weight mapping internals
 *
 * File: yvex_weight_mapping_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_WEIGHT_MAPPING_INTERNAL_H
#define YVEX_WEIGHT_MAPPING_INTERNAL_H

#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>
#include <yvex/weight_mapping.h>

struct yvex_weight_mapping_table {
    yvex_native_weight_table *native;
    yvex_artifact *template_artifact;
    yvex_gguf *template_gguf;
    yvex_tensor_table *template_tensors;
    yvex_weight_mapping_info *items;
    unsigned long long count;
    unsigned long long cap;
};

int yvex_weight_mapping_table_add(yvex_weight_mapping_table *table,
                                  const yvex_native_weight_info *native,
                                  const char *architecture,
                                  const char *target_name,
                                  yvex_tensor_role role,
                                  yvex_weight_mapping_status status,
                                  yvex_weight_mapping_issue_kind issue,
                                  const yvex_tensor_info *target,
                                  int requires_transpose,
                                  yvex_error *err);

void yvex_weight_mapping_print_shape(const unsigned long long *dims, unsigned int rank);

#endif /* YVEX_WEIGHT_MAPPING_INTERNAL_H */

/* ===== implementation ===== */

/* ===== inlined yvex_conversion_internal.h ===== */

/*
 * YVEX - Conversion bridge internals
 */
#ifndef YVEX_CONVERSION_INTERNAL_H
#define YVEX_CONVERSION_INTERNAL_H

#include <stdio.h>

#include <yvex/conversion.h>
#include <yvex/native_weights.h>
#include <yvex/tensor.h>

typedef struct {
    char native_name[256];
    char target_name[256];
    yvex_tensor_role role;
    yvex_convert_tensor_status status;
    yvex_convert_transform_kind transform;
    const yvex_native_weight_info *native;
    const char *target_qtype;
    unsigned int ggml_type;
    unsigned int target_scalar_bytes;
} yvex_conversion_tensor_plan;

const char *yvex_conversion_default_qtype_for_role(yvex_tensor_role role);
int yvex_conversion_map_tensor(const char *arch,
                               const yvex_native_weight_info *native,
                               const char *target_qtype,
                               yvex_conversion_tensor_plan *out,
                               yvex_error *err);
int yvex_conversion_read_payload(const char *native_source_dir,
                                 const yvex_native_weight_info *native,
                                 unsigned char **out,
                                 unsigned long long *out_len,
                                 yvex_error *err);
int yvex_conversion_convert_payload(const unsigned char *src,
                                    unsigned long long src_len,
                                    yvex_native_dtype src_dtype,
                                    const yvex_conversion_tensor_plan *plan,
                                    unsigned char **out,
                                    unsigned long long *out_len,
                                    yvex_error *err);
int yvex_conversion_write_single_gguf(const yvex_conversion_options *options,
                                      const yvex_conversion_tensor_plan *plan,
                                      const unsigned char *payload,
                                      unsigned long long payload_len,
                                      yvex_conversion_summary *summary,
                                      yvex_error *err);
int yvex_conversion_validate_roundtrip(const char *path, yvex_error *err);
int yvex_conversion_report_plan_json(FILE *fp,
                                     const yvex_conversion_options *options,
                                     const yvex_conversion_summary *summary,
                                     const yvex_conversion_tensor_plan *plans,
                                     unsigned long long plan_count,
                                     yvex_error *err);

#endif /* YVEX_CONVERSION_INTERNAL_H */

/* ===== implementation ===== */

/*
 * YVEX - compressed implementation unit
 *
 * This file groups related implementation sections that used to live in
 * smaller root source fragments. Public API declarations remain under
 * include/yvex/.
 */


/* ===== yvex_conversion.c ===== */

#include "yvex_deepseek_adapter.h"
#include "yvex_qwen_adapter.h"

#include <stdio.h>
#include <string.h>

#include <yvex/artifact_naming.h>
#include <yvex/qtype_support.h>

const char *yvex_conversion_status_name(yvex_conversion_status status)
{
    switch (status) {
    case YVEX_CONVERSION_STATUS_UNKNOWN: return "conversion-unknown";
    case YVEX_CONVERSION_STATUS_PLANNED: return "conversion-planned";
    case YVEX_CONVERSION_STATUS_EMITTED: return "conversion-emitted";
    case YVEX_CONVERSION_STATUS_PARTIAL: return "conversion-partial";
    case YVEX_CONVERSION_STATUS_FAILED: return "conversion-failed";
    }
    return "conversion-unknown";
}

const char *yvex_convert_tensor_status_name(yvex_convert_tensor_status status)
{
    switch (status) {
    case YVEX_CONVERT_TENSOR_STATUS_UNKNOWN: return "unknown";
    case YVEX_CONVERT_TENSOR_STATUS_READY: return "ready";
    case YVEX_CONVERT_TENSOR_STATUS_EMITTED: return "emitted";
    case YVEX_CONVERT_TENSOR_STATUS_SKIPPED: return "skipped";
    case YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE: return "unsupported_qtype";
    case YVEX_CONVERT_TENSOR_STATUS_UNMAPPED: return "unmapped";
    case YVEX_CONVERT_TENSOR_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_convert_transform_kind_name(yvex_convert_transform_kind transform)
{
    switch (transform) {
    case YVEX_CONVERT_TRANSFORM_NONE: return "none";
    case YVEX_CONVERT_TRANSFORM_TRANSPOSE_2D: return "transpose";
    case YVEX_CONVERT_TRANSFORM_DTYPE_CAST: return "dtype_cast";
    case YVEX_CONVERT_TRANSFORM_QUANTIZE: return "quantize";
    case YVEX_CONVERT_TRANSFORM_UNSUPPORTED: return "unsupported";
    }
    return "unknown";
}

const char *yvex_conversion_default_qtype_for_role(yvex_tensor_role role)
{
    switch (role) {
    case YVEX_TENSOR_ROLE_OUTPUT_NORM:
        return "F32";
    case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
        return "F16";
    default:
        return "Q8_0";
    }
}

int yvex_conversion_suggest_artifact_name(char *out,
                                          unsigned long long out_size,
                                          const char *family,
                                          const char *model,
                                          const char *scope,
                                          const char *artifact_class,
                                          const char *qprofile,
                                          const char *calibration,
                                          const char *producer,
                                          const char *schema,
                                          yvex_error *err)
{
    return yvex_artifact_name_suggest(out,
                                      (size_t)out_size,
                                      family,
                                      model,
                                      scope,
                                      artifact_class,
                                      qprofile,
                                      calibration,
                                      producer,
                                      schema,
                                      err);
}

static int qtype_to_ggml(const char *qtype, unsigned int *ggml, unsigned int *scalar)
{
    if (!qtype) return 0;
    if (strcmp(qtype, "F32") == 0) {
        *ggml = 0u; *scalar = 4u; return 1;
    }
    if (strcmp(qtype, "F16") == 0) {
        *ggml = 1u; *scalar = 2u; return 1;
    }
    if (strcmp(qtype, "BF16") == 0) {
        *ggml = 30u; *scalar = 2u; return 1;
    }
    return 0;
}

int yvex_conversion_map_tensor(const char *arch,
                               const yvex_native_weight_info *native,
                               const char *target_qtype,
                               yvex_conversion_tensor_plan *out,
                               yvex_error *err)
{
    yvex_weight_mapping_issue_kind issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    int mapped;
    const yvex_qtype_support_info *support;

    if (!arch || !native || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_map", "arch, native and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->native_name, sizeof(out->native_name), "%s", native->name ? native->name : "");
    out->native = native;

    if (strcmp(arch, "qwen3") == 0 || strcmp(arch, "qwen") == 0) {
        mapped = yvex_qwen_adapter_map_name(native->name, out->target_name, sizeof(out->target_name),
                                            &out->role, &issue);
    } else if (strcmp(arch, "deepseek4") == 0 || strcmp(arch, "deepseek") == 0) {
        mapped = yvex_deepseek_adapter_map_name(native->name, out->target_name, sizeof(out->target_name),
                                                &out->role, &issue);
    } else {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "conversion_map", "unsupported architecture: %s", arch);
        return YVEX_ERR_INVALID_ARG;
    }

    if (!mapped) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNMAPPED;
        snprintf(out->target_name, sizeof(out->target_name), "%s", "unknown");
        return YVEX_OK;
    }

    out->target_qtype = target_qtype ? target_qtype : yvex_conversion_default_qtype_for_role(out->role);
    support = yvex_qtype_support_by_name(out->target_qtype);
    if (!support || !support->emit_supported) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE;
        out->transform = YVEX_CONVERT_TRANSFORM_UNSUPPORTED;
        return YVEX_OK;
    }
    if (!qtype_to_ggml(out->target_qtype, &out->ggml_type, &out->target_scalar_bytes)) {
        out->status = YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE;
        out->transform = YVEX_CONVERT_TRANSFORM_UNSUPPORTED;
        return YVEX_OK;
    }

    out->status = YVEX_CONVERT_TENSOR_STATUS_READY;
    out->transform = YVEX_CONVERT_TRANSFORM_DTYPE_CAST;
    if ((strcmp(out->target_qtype, "F16") == 0 && native->dtype == YVEX_NATIVE_DTYPE_F16) ||
        (strcmp(out->target_qtype, "BF16") == 0 && native->dtype == YVEX_NATIVE_DTYPE_BF16) ||
        (strcmp(out->target_qtype, "F32") == 0 && native->dtype == YVEX_NATIVE_DTYPE_F32)) {
        out->transform = YVEX_CONVERT_TRANSFORM_NONE;
    }
    return YVEX_OK;
}

/* ===== yvex_conversion_emit.c ===== */


#include <stdlib.h>
#include <string.h>

int yvex_conversion_emit_gguf(const yvex_conversion_options *options,
                              yvex_conversion_summary *summary_out,
                              yvex_error *err)
{
    yvex_native_weight_options no;
    yvex_native_weight_table *native = NULL;
    const yvex_native_weight_info *info;
    yvex_conversion_tensor_plan plan;
    unsigned char *raw = NULL;
    unsigned char *converted = NULL;
    unsigned long long raw_len = 0;
    unsigned long long converted_len = 0;
    int rc;

    if (!options || !summary_out || !options->architecture ||
        !options->native_source_dir || !options->tensor_name || !options->out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "arch, native-source, tensor and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(summary_out, 0, sizeof(*summary_out));
    summary_out->architecture = options->architecture;
    summary_out->out_path = options->out_path;
    summary_out->execution_ready = 0;

    memset(&no, 0, sizeof(no));
    no.source_dir = options->native_source_dir;
    no.recursive = 1;
    rc = yvex_native_weight_table_open(&native, &no, err);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->native_tensor_count = yvex_native_weight_table_count(native);
    info = yvex_native_weight_table_find(native, options->tensor_name);
    if (!info) {
        yvex_native_weight_table_close(native);
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "selected tensor not found");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_conversion_map_tensor(options->architecture, info, options->target_qtype, &plan, err);
    if (rc == YVEX_OK && plan.status == YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        summary_out->unsupported_qtype_count = 1;
        yvex_native_weight_table_close(native);
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "conversion_emit",
                        "target qtype %s emit/quantize not implemented",
                        options->target_qtype ? options->target_qtype : plan.target_qtype);
        return YVEX_ERR_UNSUPPORTED;
    }
    if (rc == YVEX_OK && plan.status != YVEX_CONVERT_TENSOR_STATUS_READY) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        yvex_native_weight_table_close(native);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_emit", "selected tensor is not convertible");
        return YVEX_ERR_INVALID_ARG;
    }
    if (rc == YVEX_OK) rc = yvex_conversion_read_payload(options->native_source_dir, info, &raw, &raw_len, err);
    if (rc == YVEX_OK) rc = yvex_conversion_convert_payload(raw, raw_len, info->dtype, &plan, &converted, &converted_len, err);
    if (rc == YVEX_OK) {
        summary_out->planned_tensor_count = 1;
        summary_out->bytes_read = raw_len;
        rc = yvex_conversion_write_single_gguf(options, &plan, converted, converted_len, summary_out, err);
    }
    free(raw);
    free(converted);
    yvex_native_weight_table_close(native);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->status = YVEX_CONVERSION_STATUS_EMITTED;
    summary_out->emitted_tensor_count = 1;
    return YVEX_OK;
}

/* ===== yvex_conversion_payload.c ===== */


#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int read_u16le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static unsigned int float_to_f16_bits(float f)
{
    uint32_t x;
    uint32_t sign;
    int exp;
    uint32_t mant;

    memcpy(&x, &f, sizeof(x));
    sign = (x >> 16) & 0x8000u;
    exp = (int)((x >> 23) & 0xffu) - 127 + 15;
    mant = x & 0x7fffffu;
    if (exp <= 0) return sign;
    if (exp >= 31) return sign | 0x7c00u;
    return sign | ((unsigned int)exp << 10) | (mant >> 13);
}

static float f16_bits_to_float(unsigned int h)
{
    uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exp = (h >> 10) & 0x1fu;
    uint32_t mant = h & 0x3ffu;
    uint32_t x;
    float f;
    if (exp == 0) {
        x = sign;
    } else if (exp == 31) {
        x = sign | 0x7f800000u | (mant << 13);
    } else {
        x = sign | ((exp - 15u + 127u) << 23) | (mant << 13);
    }
    memcpy(&f, &x, sizeof(f));
    return f;
}

static float bf16_bits_to_float(unsigned int b)
{
    uint32_t x = (uint32_t)b << 16;
    float f;
    memcpy(&f, &x, sizeof(f));
    return f;
}

static void write_u16le(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
}

static void write_f32le(unsigned char *p, float f)
{
    uint32_t v;
    memcpy(&v, &f, sizeof(v));
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)((v >> 8) & 0xffu);
    p[2] = (unsigned char)((v >> 16) & 0xffu);
    p[3] = (unsigned char)((v >> 24) & 0xffu);
}

int yvex_conversion_read_payload(const char *native_source_dir,
                                 const yvex_native_weight_info *native,
                                 unsigned char **out,
                                 unsigned long long *out_len,
                                 yvex_error *err)
{
    char path[4096];
    FILE *fp;
    unsigned long long offset;
    unsigned char *buf;

    if (!native_source_dir || !native || !out || !out_len) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_payload", "source, native and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    *out_len = 0;
    if (snprintf(path, sizeof(path), "%s/%s", native_source_dir, native->shard_path) >= (int)sizeof(path)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "conversion_payload", "shard path too long");
        return YVEX_ERR_BOUNDS;
    }
    if (native->data_bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_payload", "payload too large to allocate");
        return YVEX_ERR_NOMEM;
    }
    buf = (unsigned char *)malloc((size_t)native->data_bytes);
    if (!buf) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_payload", "payload allocation failed");
        return YVEX_ERR_NOMEM;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        free(buf);
        yvex_error_setf(err, YVEX_ERR_IO, "conversion_payload", "cannot open shard %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }
    offset = native->data_start;
    /* Safetensors offsets are relative to the byte after the JSON header. */
    {
        unsigned char len_bytes[8];
        unsigned long long header_len = 0;
        unsigned int i;
        if (fread(len_bytes, 1, 8, fp) != 8) {
            fclose(fp); free(buf);
            yvex_error_set(err, YVEX_ERR_FORMAT, "conversion_payload", "cannot read safetensors header length");
            return YVEX_ERR_FORMAT;
        }
        for (i = 0; i < 8u; ++i) header_len |= ((unsigned long long)len_bytes[i]) << (i * 8u);
        offset += 8ull + header_len;
    }
    if (offset > (unsigned long long)LONG_MAX || fseek(fp, (long)offset, SEEK_SET) != 0) {
        fclose(fp); free(buf);
        yvex_error_set(err, YVEX_ERR_BOUNDS, "conversion_payload", "payload offset seek failed");
        return YVEX_ERR_BOUNDS;
    }
    if (fread(buf, 1, (size_t)native->data_bytes, fp) != (size_t)native->data_bytes) {
        fclose(fp); free(buf);
        yvex_error_set(err, YVEX_ERR_IO, "conversion_payload", "short payload read");
        return YVEX_ERR_IO;
    }
    fclose(fp);
    *out = buf;
    *out_len = native->data_bytes;
    return YVEX_OK;
}

int yvex_conversion_convert_payload(const unsigned char *src,
                                    unsigned long long src_len,
                                    yvex_native_dtype src_dtype,
                                    const yvex_conversion_tensor_plan *plan,
                                    unsigned char **out,
                                    unsigned long long *out_len,
                                    yvex_error *err)
{
    unsigned long long elems;
    unsigned long long i;
    unsigned char *dst;

    if (!src || !plan || !out || !out_len) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_cast", "src, plan and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    *out_len = 0;
    if (src_dtype != YVEX_NATIVE_DTYPE_F32 &&
        src_dtype != YVEX_NATIVE_DTYPE_F16 &&
        src_dtype != YVEX_NATIVE_DTYPE_BF16) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "conversion_cast", "source dtype unsupported for payload conversion");
        return YVEX_ERR_UNSUPPORTED;
    }
    elems = src_dtype == YVEX_NATIVE_DTYPE_F32 ? src_len / 4ull : src_len / 2ull;
    if (elems > ULLONG_MAX / plan->target_scalar_bytes ||
        elems * plan->target_scalar_bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_cast", "converted payload too large");
        return YVEX_ERR_NOMEM;
    }
    *out_len = elems * plan->target_scalar_bytes;
    dst = (unsigned char *)malloc((size_t)*out_len);
    if (!dst) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_cast", "converted payload allocation failed");
        return YVEX_ERR_NOMEM;
    }

    if ((strcmp(plan->target_qtype, "F16") == 0 && src_dtype == YVEX_NATIVE_DTYPE_F16) ||
        (strcmp(plan->target_qtype, "BF16") == 0 && src_dtype == YVEX_NATIVE_DTYPE_BF16) ||
        (strcmp(plan->target_qtype, "F32") == 0 && src_dtype == YVEX_NATIVE_DTYPE_F32)) {
        memcpy(dst, src, (size_t)src_len);
        *out = dst;
        return YVEX_OK;
    }

    for (i = 0; i < elems; ++i) {
        float f;
        if (src_dtype == YVEX_NATIVE_DTYPE_F32) {
            memcpy(&f, src + i * 4ull, sizeof(f));
        } else if (src_dtype == YVEX_NATIVE_DTYPE_F16) {
            f = f16_bits_to_float(read_u16le(src + i * 2ull));
        } else {
            f = bf16_bits_to_float(read_u16le(src + i * 2ull));
        }
        if (strcmp(plan->target_qtype, "F32") == 0) {
            write_f32le(dst + i * 4ull, f);
        } else if (strcmp(plan->target_qtype, "F16") == 0) {
            write_u16le(dst + i * 2ull, float_to_f16_bits(f));
        } else if (strcmp(plan->target_qtype, "BF16") == 0) {
            uint32_t x;
            memcpy(&x, &f, sizeof(x));
            write_u16le(dst + i * 2ull, (unsigned int)(x >> 16));
        } else {
            free(dst);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "conversion_cast", "target qtype conversion unsupported");
            return YVEX_ERR_UNSUPPORTED;
        }
    }
    *out = dst;
    return YVEX_OK;
}

/* ===== yvex_conversion_plan.c ===== */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int yvex_conversion_plan_write_json(const yvex_conversion_options *options,
                                    const char *plan_out_path,
                                    yvex_conversion_summary *summary_out,
                                    yvex_error *err)
{
    yvex_native_weight_options no;
    yvex_native_weight_table *native = NULL;
    yvex_conversion_tensor_plan *plans = NULL;
    unsigned long long native_count;
    unsigned long long plan_count = 0;
    unsigned long long i;
    FILE *fp;
    int rc;

    if (!options || !summary_out || !options->architecture ||
        !options->native_source_dir || !plan_out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_plan", "arch, native-source and out-plan are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(summary_out, 0, sizeof(*summary_out));
    summary_out->architecture = options->architecture;
    summary_out->out_path = plan_out_path;
    summary_out->execution_ready = 0;
    memset(&no, 0, sizeof(no));
    no.source_dir = options->native_source_dir;
    no.recursive = 1;
    rc = yvex_native_weight_table_open(&native, &no, err);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    native_count = yvex_native_weight_table_count(native);
    summary_out->native_tensor_count = native_count;
    plans = (yvex_conversion_tensor_plan *)calloc((size_t)(native_count ? native_count : 1), sizeof(*plans));
    if (!plans) {
        yvex_native_weight_table_close(native);
        yvex_error_set(err, YVEX_ERR_NOMEM, "conversion_plan", "plan allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (i = 0; i < native_count; ++i) {
        const yvex_native_weight_info *info = yvex_native_weight_table_at(native, i);
        yvex_conversion_tensor_plan plan;
        const char *qtype = NULL;
        if (options->tensor_name && strcmp(options->tensor_name, info->name) != 0) continue;
        if (options->limit_tensors && plan_count >= options->limit_tensors) break;
        rc = yvex_conversion_map_tensor(options->architecture, info, qtype, &plan, err);
        if (rc != YVEX_OK) break;
        plans[plan_count++] = plan;
        if (plan.status == YVEX_CONVERT_TENSOR_STATUS_READY) summary_out->planned_tensor_count++;
        else if (plan.status == YVEX_CONVERT_TENSOR_STATUS_UNMAPPED) summary_out->unmapped_tensor_count++;
        else if (plan.status == YVEX_CONVERT_TENSOR_STATUS_UNSUPPORTED_QTYPE) summary_out->unsupported_qtype_count++;
    }
    if (rc == YVEX_OK) {
        fp = fopen(plan_out_path, "wb");
        if (!fp) {
            yvex_error_setf(err, YVEX_ERR_IO, "conversion_plan", "cannot open plan output: %s", strerror(errno));
            rc = YVEX_ERR_IO;
        } else {
            rc = yvex_conversion_report_plan_json(fp, options, summary_out, plans, plan_count, err);
            if (fclose(fp) != 0 && rc == YVEX_OK) rc = YVEX_ERR_IO;
        }
    }
    free(plans);
    yvex_native_weight_table_close(native);
    if (rc != YVEX_OK) {
        summary_out->status = YVEX_CONVERSION_STATUS_FAILED;
        return rc;
    }
    summary_out->status = summary_out->unsupported_qtype_count || summary_out->unmapped_tensor_count
        ? YVEX_CONVERSION_STATUS_PARTIAL
        : YVEX_CONVERSION_STATUS_PLANNED;
    return YVEX_OK;
}

/* ===== yvex_conversion_report.c ===== */


#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/gguf.h>
#include <yvex/weights.h>

#define CV_ALIGN 32ull

static int exists_path(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    fclose(fp);
    return 1;
}

static int w32(FILE *fp, unsigned int v, yvex_error *err)
{
    unsigned char b[4];
    b[0] = (unsigned char)(v & 0xffu);
    b[1] = (unsigned char)((v >> 8) & 0xffu);
    b[2] = (unsigned char)((v >> 16) & 0xffu);
    b[3] = (unsigned char)((v >> 24) & 0xffu);
    if (fwrite(b, 1, 4, fp) != 4) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int w64(FILE *fp, unsigned long long v, yvex_error *err)
{
    unsigned char b[8];
    unsigned int i;
    for (i = 0; i < 8u; ++i) b[i] = (unsigned char)((v >> (i * 8u)) & 0xffull);
    if (fwrite(b, 1, 8, fp) != 8) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int wstr(FILE *fp, const char *s, yvex_error *err)
{
    unsigned long long len;
    if (!s) s = "";
    len = (unsigned long long)strlen(s);
    if (w64(fp, len, err) != YVEX_OK) return YVEX_ERR_IO;
    if (len && fwrite(s, 1, (size_t)len, fp) != (size_t)len) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "string write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int meta_string(FILE *fp, const char *key, const char *value, yvex_error *err)
{
    if (wstr(fp, key, err) != YVEX_OK) return YVEX_ERR_IO;
    if (w32(fp, 8u, err) != YVEX_OK) return YVEX_ERR_IO;
    return wstr(fp, value, err);
}

static int meta_u32(FILE *fp, const char *key, unsigned int value, yvex_error *err)
{
    if (wstr(fp, key, err) != YVEX_OK) return YVEX_ERR_IO;
    if (w32(fp, 4u, err) != YVEX_OK) return YVEX_ERR_IO;
    return w32(fp, value, err);
}

static int pad(FILE *fp, yvex_error *err)
{
    long pos = ftell(fp);
    unsigned long long rem;
    unsigned long long n;
    unsigned char z[32];
    memset(z, 0, sizeof(z));
    if (pos < 0) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "ftell failed");
        return YVEX_ERR_IO;
    }
    rem = ((unsigned long long)pos) % CV_ALIGN;
    n = rem == 0 ? 0 : CV_ALIGN - rem;
    if (n && fwrite(z, 1, (size_t)n, fp) != (size_t)n) {
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "padding write failed");
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_conversion_validate_roundtrip(const char *path, yvex_error *err)
{
    yvex_artifact_options ao;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_materialize_options mo;
    int rc;

    memset(&ao, 0, sizeof(ao));
    memset(&mo, 0, sizeof(mo));
    ao.path = path;
    ao.readonly = 1;
    mo.backend_name = "cpu";
    rc = yvex_artifact_open(&artifact, &ao, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc == YVEX_OK) rc = yvex_backend_open_cpu(&backend, err);
    if (rc == YVEX_OK) rc = yvex_weight_table_materialize(&weights, artifact, gguf, tensors, backend, &mo, err);
    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

int yvex_conversion_write_single_gguf(const yvex_conversion_options *options,
                                      const yvex_conversion_tensor_plan *plan,
                                      const unsigned char *payload,
                                      unsigned long long payload_len,
                                      yvex_conversion_summary *summary,
                                      yvex_error *err)
{
    FILE *fp;
    const yvex_native_weight_info *native;
    unsigned int i;
    long bytes;
    const char *arch_meta;

    if (!options || !plan || !payload || !summary || !options->out_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_gguf", "invalid emit arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!options->overwrite && exists_path(options->out_path)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_gguf", "refusing to overwrite output");
        return YVEX_ERR_INVALID_ARG;
    }
    native = plan->native;
    fp = fopen(options->out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "conversion_gguf", "cannot open output: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    arch_meta = (options->architecture && strncmp(options->architecture, "qwen", 4) == 0) ? "qwen" : "deepseek";
    if (w32(fp, YVEX_GGUF_MAGIC, err) != YVEX_OK ||
        w32(fp, 3u, err) != YVEX_OK ||
        w64(fp, 1ull, err) != YVEX_OK ||
        w64(fp, 5ull, err) != YVEX_OK ||
        meta_string(fp, "general.architecture", arch_meta, err) != YVEX_OK ||
        meta_string(fp, "general.name", "yvex-converted-selected-tensor", err) != YVEX_OK ||
        meta_u32(fp, "general.alignment", 32u, err) != YVEX_OK ||
        meta_u32(fp, "general.file_type", plan->ggml_type, err) != YVEX_OK ||
        meta_u32(fp, "qwen.context_length", 32768u, err) != YVEX_OK ||
        wstr(fp, plan->target_name, err) != YVEX_OK ||
        w32(fp, native->rank, err) != YVEX_OK) {
        fclose(fp);
        return YVEX_ERR_IO;
    }
    for (i = 0; i < native->rank; ++i) {
        unsigned int src_i = native->rank == 2 ? 1u - i : i;
        if (w64(fp, native->dims[src_i], err) != YVEX_OK) {
            fclose(fp);
            return YVEX_ERR_IO;
        }
    }
    if (w32(fp, plan->ggml_type, err) != YVEX_OK ||
        w64(fp, 0ull, err) != YVEX_OK ||
        pad(fp, err) != YVEX_OK) {
        fclose(fp);
        return YVEX_ERR_IO;
    }
    if (payload_len && fwrite(payload, 1, (size_t)payload_len, fp) != (size_t)payload_len) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_IO, "conversion_gguf", "payload write failed");
        return YVEX_ERR_IO;
    }
    fflush(fp);
    bytes = ftell(fp);
    fclose(fp);
    summary->bytes_written = bytes > 0 ? (unsigned long long)bytes : payload_len;
    if (yvex_conversion_validate_roundtrip(options->out_path, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    summary->roundtrip_validated = 1;
    return YVEX_OK;
}

static void json_escape(FILE *fp, const char *s)
{
    fputc('"', fp);
    for (; s && *s; ++s) {
        if (*s == '"' || *s == '\\') fputc('\\', fp);
        fputc(*s, fp);
    }
    fputc('"', fp);
}

int yvex_conversion_report_plan_json(FILE *fp,
                                     const yvex_conversion_options *options,
                                     const yvex_conversion_summary *summary,
                                     const yvex_conversion_tensor_plan *plans,
                                     unsigned long long plan_count,
                                     yvex_error *err)
{
    unsigned long long i;
    if (!fp || !options || !summary) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "conversion_plan_json", "invalid JSON arguments");
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "{\n  \"schema\": \"yvex.conversion_plan.v1\",\n");
    fprintf(fp, "  \"architecture\": ");
    json_escape(fp, options->architecture);
    fprintf(fp, ",\n  \"native_tensors\": %llu,\n", summary->native_tensor_count);
    fprintf(fp, "  \"planned_tensors\": %llu,\n", summary->planned_tensor_count);
    fprintf(fp, "  \"unmapped_tensors\": %llu,\n", summary->unmapped_tensor_count);
    fprintf(fp, "  \"unsupported_qtypes\": %llu,\n", summary->unsupported_qtype_count);
    fprintf(fp, "  \"tensors\": [\n");
    for (i = 0; i < plan_count; ++i) {
        fprintf(fp, "    {\"native\": ");
        json_escape(fp, plans[i].native_name);
        fprintf(fp, ", \"target\": ");
        json_escape(fp, plans[i].target_name);
        fprintf(fp, ", \"role\": ");
        json_escape(fp, yvex_tensor_role_name(plans[i].role));
        fprintf(fp, ", \"qtype\": ");
        json_escape(fp, plans[i].target_qtype ? plans[i].target_qtype : "");
        fprintf(fp, ", \"status\": ");
        json_escape(fp, yvex_convert_tensor_status_name(plans[i].status));
        fprintf(fp, "}%s\n", i + 1 == plan_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    return ferror(fp) ? YVEX_ERR_IO : YVEX_OK;
}

/* ===== yvex_weight_mapping.c ===== */

#include "yvex_deepseek_adapter.h"
#include "yvex_qwen_adapter.h"

#include <stdlib.h>
#include <string.h>

static char *wm_strdup(const char *s)
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

static int wm_supported_arch(const char *architecture)
{
    return architecture &&
           (strcmp(architecture, "deepseek4") == 0 ||
            strcmp(architecture, "deepseek") == 0 ||
            strcmp(architecture, "qwen3") == 0 ||
            strcmp(architecture, "qwen") == 0);
}

const char *yvex_weight_mapping_status_name(yvex_weight_mapping_status status)
{
    switch (status) {
    case YVEX_WEIGHT_MAPPING_STATUS_UNKNOWN: return "unknown";
    case YVEX_WEIGHT_MAPPING_STATUS_MAPPED: return "mapped";
    case YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED: return "unmapped";
    case YVEX_WEIGHT_MAPPING_STATUS_AMBIGUOUS: return "ambiguous";
    case YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH: return "shape_mismatch";
    case YVEX_WEIGHT_MAPPING_STATUS_UNSUPPORTED_ARCH: return "unsupported_arch";
    }
    return "unknown";
}

const char *yvex_weight_mapping_issue_kind_name(yvex_weight_mapping_issue_kind issue)
{
    switch (issue) {
    case YVEX_WEIGHT_MAPPING_ISSUE_NONE: return "none";
    case YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME: return "unknown_native_name";
    case YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME: return "unknown_template_name";
    case YVEX_WEIGHT_MAPPING_ISSUE_ROLE_UNSUPPORTED: return "role_unsupported";
    case YVEX_WEIGHT_MAPPING_ISSUE_SHAPE_MISMATCH: return "shape_mismatch";
    case YVEX_WEIGHT_MAPPING_ISSUE_ARCH_UNSUPPORTED: return "arch_unsupported";
    case YVEX_WEIGHT_MAPPING_ISSUE_MOE_EXPERT_UNPARSED: return "moe_expert_unparsed";
    }
    return "unknown_native_name";
}

static int wm_same_shape_native_target(const yvex_native_weight_info *native,
                                       const yvex_tensor_info *target,
                                       int *requires_transpose)
{
    unsigned int i;

    if (requires_transpose) *requires_transpose = 0;
    if (!native || !target || native->rank != target->rank) {
        return 0;
    }
    for (i = 0; i < native->rank; ++i) {
        if (native->dims[i] != target->dims[i]) {
            break;
        }
    }
    if (i == native->rank) {
        return 1;
    }
    if (native->rank == 2 &&
        native->dims[0] == target->dims[1] &&
        native->dims[1] == target->dims[0]) {
        if (requires_transpose) *requires_transpose = 1;
        return 1;
    }
    return 0;
}

static const yvex_tensor_info *wm_find_by_role_shape(const yvex_tensor_table *table,
                                                     yvex_tensor_role role,
                                                     const yvex_native_weight_info *native,
                                                     int *requires_transpose)
{
    unsigned long long i;

    if (!table || role == YVEX_TENSOR_ROLE_UNKNOWN || !native) {
        return NULL;
    }
    for (i = 0; i < yvex_tensor_table_count(table); ++i) {
        const yvex_tensor_info *candidate = yvex_tensor_table_at(table, i);
        int transpose = 0;

        if (!candidate || candidate->role != role) continue;
        if (wm_same_shape_native_target(native, candidate, &transpose)) {
            if (requires_transpose) *requires_transpose = transpose;
            return candidate;
        }
    }
    return NULL;
}

int yvex_weight_mapping_table_add(yvex_weight_mapping_table *table,
                                  const yvex_native_weight_info *native,
                                  const char *architecture,
                                  const char *target_name,
                                  yvex_tensor_role role,
                                  yvex_weight_mapping_status status,
                                  yvex_weight_mapping_issue_kind issue,
                                  const yvex_tensor_info *target,
                                  int requires_transpose,
                                  yvex_error *err)
{
    yvex_weight_mapping_info *next;
    yvex_weight_mapping_info *row;
    unsigned int i;

    if (!table || !native || !architecture) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "weight_mapping_add", "invalid mapping row");
        return YVEX_ERR_INVALID_ARG;
    }
    if (table->count == table->cap) {
        unsigned long long cap = table->cap == 0 ? 64u : table->cap * 2u;
        next = (yvex_weight_mapping_info *)realloc(table->items, (size_t)cap * sizeof(table->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_add", "mapping table allocation failed");
            return YVEX_ERR_NOMEM;
        }
        table->items = next;
        table->cap = cap;
    }
    row = &table->items[table->count];
    memset(row, 0, sizeof(*row));
    row->native_name = wm_strdup(native->name);
    row->target_name = wm_strdup(target ? target->name : target_name);
    row->architecture = wm_strdup(architecture);
    if (!row->native_name || !row->target_name || !row->architecture) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_add", "mapping row allocation failed");
        free((char *)row->native_name);
        free((char *)row->target_name);
        free((char *)row->architecture);
        memset(row, 0, sizeof(*row));
        return YVEX_ERR_NOMEM;
    }
    row->role = role;
    row->status = status;
    row->issue = issue;
    row->native_rank = native->rank;
    for (i = 0; i < native->rank && i < YVEX_WEIGHT_MAPPING_MAX_DIMS; ++i) {
        row->native_dims[i] = native->dims[i];
    }
    if (target) {
        row->target_rank = target->rank;
        for (i = 0; i < target->rank && i < YVEX_WEIGHT_MAPPING_MAX_DIMS; ++i) {
            row->target_dims[i] = target->dims[i];
        }
    }
    row->requires_transpose = requires_transpose;
    table->count++;
    return YVEX_OK;
}

static int wm_open_template(yvex_weight_mapping_table *table,
                            const char *template_path,
                            yvex_error *err)
{
    yvex_artifact_options artifact_options;
    int rc;

    if (!template_path) return YVEX_OK;
    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&table->template_artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&table->template_gguf, table->template_artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&table->template_tensors, table->template_gguf, err);
    return rc;
}

static int wm_map_native_row(yvex_weight_mapping_table *table,
                             const yvex_native_weight_info *native,
                             const yvex_weight_mapping_options *options,
                             yvex_error *err)
{
    char target_candidate[256];
    yvex_tensor_role role = YVEX_TENSOR_ROLE_UNKNOWN;
    yvex_weight_mapping_issue_kind issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    yvex_weight_mapping_status status = YVEX_WEIGHT_MAPPING_STATUS_MAPPED;
    const yvex_tensor_info *target = NULL;
    int requires_transpose = 0;
    int mapped;

    if (strcmp(options->architecture, "qwen3") == 0 ||
        strcmp(options->architecture, "qwen") == 0) {
        mapped = yvex_qwen_adapter_map_name(native->name, target_candidate, sizeof(target_candidate),
                                            &role, &issue);
    } else {
        mapped = yvex_deepseek_adapter_map_name(native->name, target_candidate, sizeof(target_candidate),
                                                &role, &issue);
    }
    if (!mapped) {
        return yvex_weight_mapping_table_add(table, native, options->architecture, "unknown",
                                             YVEX_TENSOR_ROLE_UNKNOWN,
                                             YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED,
                                             issue, NULL, 0, err);
    }

    if (table->template_tensors) {
        target = yvex_tensor_table_find(table->template_tensors, target_candidate);
        if (target) {
            if (!wm_same_shape_native_target(native, target, &requires_transpose)) {
                status = YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH;
                issue = YVEX_WEIGHT_MAPPING_ISSUE_SHAPE_MISMATCH;
            }
        } else {
            target = wm_find_by_role_shape(table->template_tensors, role, native, &requires_transpose);
            if (!target) {
                status = YVEX_WEIGHT_MAPPING_STATUS_UNMAPPED;
                issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME;
            }
        }
    }

    return yvex_weight_mapping_table_add(table, native, options->architecture, target_candidate,
                                         role, status, issue, target, requires_transpose, err);
}

int yvex_weight_mapping_table_build(yvex_weight_mapping_table **out,
                                    const yvex_weight_mapping_options *options,
                                    yvex_error *err)
{
    yvex_weight_mapping_table *table;
    yvex_native_weight_options native_options;
    unsigned long long i;
    int rc;

    if (!out || !options || !options->architecture || !options->native_source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "weight_mapping_build", "architecture and native_source_dir are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!wm_supported_arch(options->architecture)) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "weight_mapping_build",
                        "unsupported architecture: %s", options->architecture);
        return YVEX_ERR_INVALID_ARG;
    }

    table = (yvex_weight_mapping_table *)calloc(1, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "weight_mapping_build", "mapping table allocation failed");
        return YVEX_ERR_NOMEM;
    }

    memset(&native_options, 0, sizeof(native_options));
    native_options.source_dir = options->native_source_dir;
    native_options.recursive = 1;
    native_options.include_metadata = 0;
    rc = yvex_native_weight_table_open(&table->native, &native_options, err);
    if (rc == YVEX_OK && (options->compare_template || options->template_path)) {
        rc = wm_open_template(table, options->template_path, err);
    }
    if (rc != YVEX_OK) {
        yvex_weight_mapping_table_close(table);
        return rc;
    }

    for (i = 0; i < yvex_native_weight_table_count(table->native); ++i) {
        const yvex_native_weight_info *native = yvex_native_weight_table_at(table->native, i);
        rc = wm_map_native_row(table, native, options, err);
        if (rc != YVEX_OK) {
            yvex_weight_mapping_table_close(table);
            return rc;
        }
    }

    if (options->require_all_native_mapped) {
        for (i = 0; i < table->count; ++i) {
            if (table->items[i].status != YVEX_WEIGHT_MAPPING_STATUS_MAPPED) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "weight_mapping_build", "unmapped native tensor present");
                yvex_weight_mapping_table_close(table);
                return YVEX_ERR_FORMAT;
            }
        }
    }
    if (options->require_all_template_matched && table->template_tensors) {
        unsigned long long j;
        for (i = 0; i < yvex_tensor_table_count(table->template_tensors); ++i) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(table->template_tensors, i);
            int matched = 0;
            for (j = 0; j < table->count; ++j) {
                if (tensor && table->items[j].target_name &&
                    strcmp(tensor->name, table->items[j].target_name) == 0 &&
                    table->items[j].status == YVEX_WEIGHT_MAPPING_STATUS_MAPPED) {
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                yvex_error_set(err, YVEX_ERR_FORMAT, "weight_mapping_build", "unmatched template tensor present");
                yvex_weight_mapping_table_close(table);
                return YVEX_ERR_FORMAT;
            }
        }
    }

    *out = table;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_weight_mapping_table_close(yvex_weight_mapping_table *table)
{
    unsigned long long i;

    if (!table) return;
    for (i = 0; i < table->count; ++i) {
        free((char *)table->items[i].native_name);
        free((char *)table->items[i].target_name);
        free((char *)table->items[i].architecture);
    }
    free(table->items);
    yvex_tensor_table_close(table->template_tensors);
    yvex_gguf_close(table->template_gguf);
    yvex_artifact_close(table->template_artifact);
    yvex_native_weight_table_close(table->native);
    free(table);
}

unsigned long long yvex_weight_mapping_table_count(const yvex_weight_mapping_table *table)
{
    return table ? table->count : 0;
}

const yvex_weight_mapping_info *yvex_weight_mapping_table_at(const yvex_weight_mapping_table *table,
                                                             unsigned long long index)
{
    if (!table || index >= table->count) return NULL;
    return &table->items[index];
}

const yvex_weight_mapping_info *yvex_weight_mapping_table_find_native(const yvex_weight_mapping_table *table,
                                                                      const char *native_name)
{
    unsigned long long i;

    if (!table || !native_name) return NULL;
    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].native_name, native_name) == 0) {
            return &table->items[i];
        }
    }
    return NULL;
}

/* ===== yvex_weight_mapping_report.c ===== */


#include <stdio.h>

void yvex_weight_mapping_print_shape(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    if (!dims || rank == 0) {
        printf("unknown");
        return;
    }
    printf("[");
    for (i = 0; i < rank; ++i) {
        if (i) printf(",");
        printf("%llu", dims[i]);
    }
    printf("]");
}

/* ===== yvex_deepseek_adapter.c ===== */

#include "yvex_deepseek_adapter.h"

#include <stdio.h>
#include <string.h>

static int ds_set(char *target, size_t target_cap,
                  yvex_tensor_role *role,
                  yvex_weight_mapping_issue_kind *issue,
                  yvex_tensor_role value,
                  const char *name)
{
    if (!target || target_cap == 0 || !role || !issue || !name) {
        return 0;
    }
    if (snprintf(target, target_cap, "%s", name) >= (int)target_cap) {
        *role = YVEX_TENSOR_ROLE_UNKNOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_TEMPLATE_NAME;
        return 0;
    }
    *role = value;
    *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    return 1;
}

static int ds_starts_with(const char *text, const char *prefix)
{
    size_t len;

    if (!text || !prefix) return 0;
    len = strlen(prefix);
    return strncmp(text, prefix, len) == 0;
}

static int ds_ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;

    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    return suffix_len <= text_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int ds_layer_suffix(const char *native_name,
                           const char *suffix,
                           unsigned int *layer_out)
{
    unsigned int layer;
    int consumed;

    if (sscanf(native_name, "model.layers.%u.%n", &layer, &consumed) != 1) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    return 1;
}

static int ds_plain_layer_suffix(const char *native_name,
                                 const char *suffix,
                                 unsigned int *layer_out)
{
    unsigned int layer;
    int consumed;

    if (sscanf(native_name, "layers.%u.%n", &layer, &consumed) != 1) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    return 1;
}

static int ds_expert_suffix(const char *native_name,
                            const char *suffix,
                            unsigned int *layer_out,
                            unsigned int *expert_out)
{
    unsigned int layer;
    unsigned int expert;
    int consumed;

    if (sscanf(native_name, "model.layers.%u.mlp.experts.%u.%n",
               &layer, &expert, &consumed) != 2) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    *expert_out = expert;
    return 1;
}

static int ds_plain_expert_suffix(const char *native_name,
                                  const char *suffix,
                                  unsigned int *layer_out,
                                  unsigned int *expert_out)
{
    unsigned int layer;
    unsigned int expert;
    int consumed;

    if (sscanf(native_name, "layers.%u.ffn.experts.%u.%n",
               &layer, &expert, &consumed) != 2) {
        return 0;
    }
    if (strcmp(native_name + consumed, suffix) != 0) {
        return 0;
    }
    *layer_out = layer;
    *expert_out = expert;
    return 1;
}

static int ds_template_style(const char *native_name,
                             char *target,
                             size_t target_cap,
                             yvex_tensor_role *role,
                             yvex_weight_mapping_issue_kind *issue)
{
    if (strcmp(native_name, "token_embd.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "token_embd.weight");
    }
    if (strcmp(native_name, "output_norm.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_NORM, "output_norm.weight");
    }
    if (strcmp(native_name, "output.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_HEAD, "output.weight");
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_q.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_Q, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_k.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_K, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_v.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_V, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_output.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_OUT, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".attn_norm.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_ATTENTION_NORM, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_norm.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_NORM, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_gate.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_GATE, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_up.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_UP, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_down.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_FFN_DOWN, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && ds_ends_with(native_name, ".ffn_gate_inp.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_ROUTER, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".gate.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_GATE, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".up.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_UP, native_name);
    }
    if (ds_starts_with(native_name, "blk.") && strstr(native_name, ".ffn.experts.") &&
        ds_ends_with(native_name, ".down.weight")) {
        return ds_set(target, target_cap, role, issue, YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN, native_name);
    }
    return 0;
}

int yvex_deepseek_adapter_map_name(const char *native_name,
                                   char *target,
                                   size_t target_cap,
                                   yvex_tensor_role *role,
                                   yvex_weight_mapping_issue_kind *issue)
{
    unsigned int layer;
    unsigned int expert;

    if (!native_name || !target || target_cap == 0 || !role || !issue) {
        return 0;
    }
    target[0] = '\0';
    *role = YVEX_TENSOR_ROLE_UNKNOWN;
    *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;

    if (ds_template_style(native_name, target, target_cap, role, issue)) {
        return 1;
    }
    if (strcmp(native_name, "embed.weight") == 0 ||
        strcmp(native_name, "model.embed_tokens.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_TOKEN_EMBEDDING, "token_embd.weight");
    }
    if (strcmp(native_name, "norm.weight") == 0 ||
        strcmp(native_name, "model.norm.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_NORM, "output_norm.weight");
    }
    if (strcmp(native_name, "lm_head.weight") == 0 ||
        strcmp(native_name, "output.weight") == 0) {
        return ds_set(target, target_cap, role, issue,
                      YVEX_TENSOR_ROLE_OUTPUT_HEAD, "output.weight");
    }
    if (ds_layer_suffix(native_name, "self_attn.q_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_q.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_Q;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.k_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_k.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_K;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.v_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_v.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_V;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "self_attn.o_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_output.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_OUT;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "input_layernorm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "attn_norm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.attn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "post_attention_layernorm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "ffn_norm.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_norm.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_NORM;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.gate_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.up_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_up.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.down_proj.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_down.weight", layer);
        *role = YVEX_TENSOR_ROLE_FFN_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_layer_suffix(native_name, "mlp.gate.weight", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate_inp.weight", layer);
        *role = YVEX_TENSOR_ROLE_MOE_ROUTER;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_layer_suffix(native_name, "ffn.gate.weight", &layer) ||
        ds_plain_layer_suffix(native_name, "ffn.gate.bias", &layer)) {
        snprintf(target, target_cap, "blk.%u.ffn_gate_inp.weight", layer);
        *role = YVEX_TENSOR_ROLE_MOE_ROUTER;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "gate_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.gate.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "up_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.up.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_expert_suffix(native_name, "down_proj.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.down.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w1.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.gate.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_GATE;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w2.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.down.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }
    if (ds_plain_expert_suffix(native_name, "w3.weight", &layer, &expert)) {
        snprintf(target, target_cap, "blk.%u.ffn.experts.%u.up.weight", layer, expert);
        *role = YVEX_TENSOR_ROLE_MOE_EXPERT_UP;
        *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
        return 1;
    }

    return 0;
}

/* ===== yvex_qwen_adapter.c ===== */

#include "yvex_qwen_adapter.h"

#include <stdio.h>
#include <string.h>

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len;
    size_t suffix_len;
    if (!text || !suffix) return 0;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    return suffix_len <= text_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int extract_layer(const char *name, unsigned int *layer)
{
    return name && sscanf(name, "model.layers.%u.", layer) == 1;
}

static int set_target(char *target, size_t cap, const char *fmt, unsigned int layer)
{
    int n = snprintf(target, cap, fmt, layer);
    return n > 0 && (size_t)n < cap;
}

int yvex_qwen_adapter_map_name(const char *native_name,
                               char *target,
                               size_t target_cap,
                               yvex_tensor_role *role,
                               yvex_weight_mapping_issue_kind *issue)
{
    unsigned int layer = 0;

    if (role) *role = YVEX_TENSOR_ROLE_UNKNOWN;
    if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_NONE;
    if (!native_name || !target || target_cap == 0) {
        if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
        return 0;
    }

    if (strcmp(native_name, "model.embed_tokens.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_TOKEN_EMBEDDING;
        snprintf(target, target_cap, "%s", "token_embd.weight");
        return 1;
    }
    if (strcmp(native_name, "model.norm.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_OUTPUT_NORM;
        snprintf(target, target_cap, "%s", "output_norm.weight");
        return 1;
    }
    if (strcmp(native_name, "lm_head.weight") == 0) {
        if (role) *role = YVEX_TENSOR_ROLE_OUTPUT_HEAD;
        snprintf(target, target_cap, "%s", "output.weight");
        return 1;
    }

    if (!extract_layer(native_name, &layer)) {
        if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
        return 0;
    }

    if (ends_with(native_name, ".self_attn.q_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_Q;
        return set_target(target, target_cap, "blk.%u.attn_q.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.k_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_K;
        return set_target(target, target_cap, "blk.%u.attn_k.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.v_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_V;
        return set_target(target, target_cap, "blk.%u.attn_v.weight", layer);
    }
    if (ends_with(native_name, ".self_attn.o_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_OUT;
        return set_target(target, target_cap, "blk.%u.attn_output.weight", layer);
    }
    if (ends_with(native_name, ".input_layernorm.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_ATTENTION_NORM;
        return set_target(target, target_cap, "blk.%u.attn_norm.weight", layer);
    }
    if (ends_with(native_name, ".post_attention_layernorm.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_NORM;
        return set_target(target, target_cap, "blk.%u.ffn_norm.weight", layer);
    }
    if (ends_with(native_name, ".mlp.gate_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_GATE;
        return set_target(target, target_cap, "blk.%u.ffn_gate.weight", layer);
    }
    if (ends_with(native_name, ".mlp.up_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_UP;
        return set_target(target, target_cap, "blk.%u.ffn_up.weight", layer);
    }
    if (ends_with(native_name, ".mlp.down_proj.weight")) {
        if (role) *role = YVEX_TENSOR_ROLE_FFN_DOWN;
        return set_target(target, target_cap, "blk.%u.ffn_down.weight", layer);
    }

    if (issue) *issue = YVEX_WEIGHT_MAPPING_ISSUE_UNKNOWN_NATIVE_NAME;
    return 0;
}

/* ===== yvex_qtype_support.c ===== */

#include <string.h>

#include <yvex/qtype_support.h>

static const yvex_qtype_support_info qtype_rows[] = {
    {"F32",      1, 1, 1, 0, 1, "scalar emit supported; CPU/CUDA materialization only"},
    {"F16",      1, 1, 1, 1, 0, "scalar emit and cast supported; no compute claim"},
    {"BF16",     1, 1, 1, 1, 0, "scalar emit and cast supported; no compute claim"},
    {"Q8_0",     1, 1, 0, 0, 0, "storage known; quantizer/emitter not enabled in open-weight intake"},
    {"Q4_K",     1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
    {"Q2_K",     1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
    {"IQ2_XXS",  1, 0, 0, 0, 0, "policy vocabulary only; no emitter"},
};

const yvex_qtype_support_info *yvex_qtype_support_by_name(const char *qtype)
{
    unsigned long long i;
    if (!qtype) return NULL;
    for (i = 0; i < yvex_qtype_support_count(); ++i) {
        if (strcmp(qtype_rows[i].qtype, qtype) == 0) {
            return &qtype_rows[i];
        }
    }
    return NULL;
}

unsigned long long yvex_qtype_support_count(void)
{
    return (unsigned long long)(sizeof(qtype_rows) / sizeof(qtype_rows[0]));
}

const yvex_qtype_support_info *yvex_qtype_support_at(unsigned long long index)
{
    if (index >= yvex_qtype_support_count()) return NULL;
    return &qtype_rows[index];
}
