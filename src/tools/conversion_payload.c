/*
 * YVEX - Safetensors selected payload reader and scalar casts
 */
#include "conversion_internal.h"

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
