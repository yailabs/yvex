/*
 * YVEX - GGUF parser
 *
 * File: src/formats/gguf.c
 * Layer: format implementation
 *
 * Purpose:
 *   Implements GGUF magic probing, fixed header parsing, metadata parsing,
 *   and raw tensor directory parsing through checked little-endian cursor
 *   reads. This module does not create model descriptors, tensor tables, or
 *   execution-ready state.
 *
 * Implements:
 *   - yvex_gguf_probe_file
 *   - yvex_gguf_read_header
 *   - yvex_gguf_open
 *   - yvex_gguf_close
 *   - metadata accessors
 *   - tensor directory accessors
 *
 * Invariants:
 *   - parser never casts mapped bytes to structs
 *   - cursor advances only after successful reads
 *   - caller-owned artifacts are never closed by this module
 *   - partially parsed state is freed on failure
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_gguf
 */
#include <yvex/gguf.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define YVEX_GGUF_HEADER_BYTES 24u
#define YVEX_GGUF_SUPPORTED_VERSION 3u
#define YVEX_GGUF_DEFAULT_ALIGNMENT 32u
#define YVEX_GGUF_MAX_KEY_LEN 65535ull
#define YVEX_GGUF_MAX_TENSOR_NAME_LEN 64ull

typedef struct {
    const unsigned char *data;
    unsigned long long size;
    unsigned long long offset;
} yvex_byte_cursor;

struct yvex_gguf_value {
    yvex_gguf_value_type type;
    union {
        unsigned long long u64;
        long long i64;
        double f64;
        int bool_value;
        struct {
            char *data;
            unsigned long long len;
        } string;
        struct {
            yvex_gguf_array_info info;
            yvex_gguf_value *items;
        } array;
    } as;
};

typedef struct {
    char *key;
    yvex_gguf_value value;
} yvex_gguf_metadata_entry;

struct yvex_gguf {
    yvex_gguf_header header;
    yvex_gguf_metadata_entry *metadata;
    yvex_gguf_tensor_info *tensors;
    unsigned long long tensor_data_offset;
    unsigned int alignment;
};

static int cursor_require(const yvex_byte_cursor *cur,
                          unsigned long long len,
                          yvex_error *err,
                          const char *where,
                          const char *field)
{
    if (yvex_range_check(cur->size, cur->offset, len, err) == YVEX_OK) {
        return YVEX_OK;
    }

    yvex_error_setf(err, YVEX_ERR_BOUNDS, where,
                    "%s out of bounds at offset %llu len %llu",
                    field ? field : "read", cur->offset, len);
    return YVEX_ERR_BOUNDS;
}

static int cursor_read_u8(yvex_byte_cursor *cur, unsigned char *out, yvex_error *err, const char *where, const char *field)
{
    if (cursor_require(cur, 1, err, where, field) != YVEX_OK) {
        return YVEX_ERR_BOUNDS;
    }
    *out = cur->data[cur->offset];
    cur->offset += 1;
    return YVEX_OK;
}

static int cursor_read_u16le(yvex_byte_cursor *cur, unsigned int *out, yvex_error *err, const char *where, const char *field)
{
    const unsigned char *p;
    if (cursor_require(cur, 2, err, where, field) != YVEX_OK) {
        return YVEX_ERR_BOUNDS;
    }
    p = cur->data + cur->offset;
    *out = ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
    cur->offset += 2;
    return YVEX_OK;
}

static int cursor_read_u32le(yvex_byte_cursor *cur, unsigned int *out, yvex_error *err, const char *where, const char *field)
{
    const unsigned char *p;
    if (cursor_require(cur, 4, err, where, field) != YVEX_OK) {
        return YVEX_ERR_BOUNDS;
    }
    p = cur->data + cur->offset;
    *out = ((unsigned int)p[0]) |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
    cur->offset += 4;
    return YVEX_OK;
}

static int cursor_read_u64le(yvex_byte_cursor *cur, unsigned long long *out, yvex_error *err, const char *where, const char *field)
{
    const unsigned char *p;
    if (cursor_require(cur, 8, err, where, field) != YVEX_OK) {
        return YVEX_ERR_BOUNDS;
    }
    p = cur->data + cur->offset;
    *out = ((unsigned long long)p[0]) |
           ((unsigned long long)p[1] << 8) |
           ((unsigned long long)p[2] << 16) |
           ((unsigned long long)p[3] << 24) |
           ((unsigned long long)p[4] << 32) |
           ((unsigned long long)p[5] << 40) |
           ((unsigned long long)p[6] << 48) |
           ((unsigned long long)p[7] << 56);
    cur->offset += 8;
    return YVEX_OK;
}

static int cursor_read_string_alloc(yvex_byte_cursor *cur,
                                    char **out,
                                    unsigned long long *out_len,
                                    unsigned long long max_len,
                                    int reject_empty,
                                    yvex_error *err,
                                    const char *where,
                                    const char *field)
{
    unsigned long long len;
    char *copy;
    int rc;

    *out = NULL;
    if (out_len) {
        *out_len = 0;
    }

    rc = cursor_read_u64le(cur, &len, err, where, field);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (reject_empty && len == 0) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, where, "%s is empty", field);
        return YVEX_ERR_FORMAT;
    }

    if (max_len > 0 && len > max_len) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, where,
                        "%s length %llu exceeds maximum %llu", field, len, max_len);
        return YVEX_ERR_FORMAT;
    }

    rc = cursor_require(cur, len, err, where, field);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (len > (unsigned long long)(SIZE_MAX - 1)) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, where, "%s is too large to allocate", field);
        return YVEX_ERR_NOMEM;
    }

    copy = (char *)malloc((size_t)len + 1u);
    if (!copy) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, where, "failed to allocate %s", field);
        return YVEX_ERR_NOMEM;
    }

    if (len > 0) {
        memcpy(copy, cur->data + cur->offset, (size_t)len);
    }
    copy[len] = '\0';
    cur->offset += len;

    *out = copy;
    if (out_len) {
        *out_len = len;
    }
    return YVEX_OK;
}

static int checked_count_alloc(unsigned long long count, size_t elem_size, yvex_error *err, const char *where, const char *what)
{
    if (count > (unsigned long long)(SIZE_MAX / elem_size)) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, where, "%s count %llu is too large", what, count);
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

static int align_offset(unsigned long long offset, unsigned int alignment, unsigned long long *out, yvex_error *err)
{
    unsigned long long rem;
    unsigned long long add;

    if (alignment == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "gguf.tensor_dir", "alignment is zero");
        return YVEX_ERR_FORMAT;
    }

    rem = offset % (unsigned long long)alignment;
    add = rem == 0 ? 0 : (unsigned long long)alignment - rem;
    if (offset > ULLONG_MAX - add) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "gguf.tensor_dir", "tensor data offset overflow");
        return YVEX_ERR_BOUNDS;
    }

    *out = offset + add;
    return YVEX_OK;
}

static int parse_header(yvex_byte_cursor *cur, yvex_gguf_header *out, yvex_error *err, const char *where)
{
    unsigned int magic;
    int rc;

    memset(out, 0, sizeof(*out));

    rc = cursor_read_u32le(cur, &magic, err, where, "magic");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (magic != YVEX_GGUF_MAGIC) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, where, "bad GGUF magic 0x%08x", magic);
        return YVEX_ERR_FORMAT;
    }

    rc = cursor_read_u32le(cur, &out->version, err, where, "version");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (out->version != YVEX_GGUF_SUPPORTED_VERSION) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, where, "unsupported GGUF version %u", out->version);
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = cursor_read_u64le(cur, &out->tensor_count, err, where, "tensor_count");
    if (rc != YVEX_OK) {
        return rc;
    }
    return cursor_read_u64le(cur, &out->metadata_count, err, where, "metadata_count");
}

static int validate_value_type(unsigned int raw, yvex_gguf_value_type *out, yvex_error *err, const char *where)
{
    if (raw > (unsigned int)YVEX_GGUF_VALUE_FLOAT64) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, where, "unsupported metadata value type %u", raw);
        return YVEX_ERR_UNSUPPORTED;
    }
    *out = (yvex_gguf_value_type)raw;
    return YVEX_OK;
}

static void gguf_value_clear(yvex_gguf_value *value)
{
    unsigned long long i;

    if (!value) {
        return;
    }
    if (value->type == YVEX_GGUF_VALUE_STRING) {
        free(value->as.string.data);
        value->as.string.data = NULL;
        value->as.string.len = 0;
    } else if (value->type == YVEX_GGUF_VALUE_ARRAY) {
        if (value->as.array.items) {
            for (i = 0; i < value->as.array.info.count; ++i) {
                gguf_value_clear(&value->as.array.items[i]);
            }
            free(value->as.array.items);
        }
        value->as.array.items = NULL;
        value->as.array.info.count = 0;
    }
    memset(value, 0, sizeof(*value));
}

static int parse_value(yvex_byte_cursor *cur, yvex_gguf_value_type type, yvex_gguf_value *out, yvex_error *err)
{
    unsigned char u8;
    unsigned int u16;
    unsigned int u32;
    unsigned long long u64;
    unsigned int raw_type;
    yvex_gguf_value_type element_type;
    unsigned long long i;
    int rc;

    memset(out, 0, sizeof(*out));
    out->type = type;

    switch (type) {
    case YVEX_GGUF_VALUE_UINT8:
        rc = cursor_read_u8(cur, &u8, err, "gguf.metadata.value", "uint8");
        out->as.u64 = (unsigned long long)u8;
        return rc;
    case YVEX_GGUF_VALUE_INT8:
        rc = cursor_read_u8(cur, &u8, err, "gguf.metadata.value", "int8");
        out->as.i64 = (long long)(signed char)u8;
        return rc;
    case YVEX_GGUF_VALUE_UINT16:
        rc = cursor_read_u16le(cur, &u16, err, "gguf.metadata.value", "uint16");
        out->as.u64 = (unsigned long long)u16;
        return rc;
    case YVEX_GGUF_VALUE_INT16:
        rc = cursor_read_u16le(cur, &u16, err, "gguf.metadata.value", "int16");
        out->as.i64 = (long long)(int16_t)(uint16_t)u16;
        return rc;
    case YVEX_GGUF_VALUE_UINT32:
        rc = cursor_read_u32le(cur, &u32, err, "gguf.metadata.value", "uint32");
        out->as.u64 = (unsigned long long)u32;
        return rc;
    case YVEX_GGUF_VALUE_INT32:
        rc = cursor_read_u32le(cur, &u32, err, "gguf.metadata.value", "int32");
        out->as.i64 = (long long)(int32_t)(uint32_t)u32;
        return rc;
    case YVEX_GGUF_VALUE_FLOAT32: {
        float f32;
        rc = cursor_read_u32le(cur, &u32, err, "gguf.metadata.value", "float32");
        if (rc != YVEX_OK) {
            return rc;
        }
        memcpy(&f32, &u32, sizeof(f32));
        out->as.f64 = (double)f32;
        return YVEX_OK;
    }
    case YVEX_GGUF_VALUE_BOOL:
        rc = cursor_read_u8(cur, &u8, err, "gguf.metadata.value", "bool");
        if (rc != YVEX_OK) {
            return rc;
        }
        if (u8 != 0 && u8 != 1) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "gguf.metadata.value", "invalid bool value %u", (unsigned int)u8);
            return YVEX_ERR_FORMAT;
        }
        out->as.bool_value = u8 ? 1 : 0;
        return YVEX_OK;
    case YVEX_GGUF_VALUE_STRING:
        return cursor_read_string_alloc(cur, &out->as.string.data, &out->as.string.len,
                                        0, 0, err, "gguf.metadata.value", "string");
    case YVEX_GGUF_VALUE_ARRAY:
        rc = cursor_read_u32le(cur, &raw_type, err, "gguf.metadata.array", "array element type");
        if (rc != YVEX_OK) {
            return rc;
        }
        rc = validate_value_type(raw_type, &element_type, err, "gguf.metadata.array");
        if (rc != YVEX_OK) {
            return rc;
        }
        if (element_type == YVEX_GGUF_VALUE_ARRAY) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "gguf.metadata.array", "nested arrays are not implemented");
            return YVEX_ERR_UNSUPPORTED;
        }
        rc = cursor_read_u64le(cur, &u64, err, "gguf.metadata.array", "array count");
        if (rc != YVEX_OK) {
            return rc;
        }
        out->as.array.info.element_type = element_type;
        out->as.array.info.count = u64;
        if (u64 > 0) {
            rc = checked_count_alloc(u64, sizeof(*out->as.array.items), err,
                                     "gguf.metadata.array", "array item");
            if (rc != YVEX_OK) {
                return rc;
            }
            out->as.array.items = (yvex_gguf_value *)calloc((size_t)u64, sizeof(*out->as.array.items));
            if (!out->as.array.items) {
                yvex_error_set(err, YVEX_ERR_NOMEM, "gguf.metadata.array", "failed to allocate array items");
                return YVEX_ERR_NOMEM;
            }
        }
        for (i = 0; i < u64; ++i) {
            rc = parse_value(cur, element_type, &out->as.array.items[i], err);
            if (rc != YVEX_OK) {
                gguf_value_clear(out);
                return rc;
            }
        }
        return YVEX_OK;
    case YVEX_GGUF_VALUE_UINT64:
        rc = cursor_read_u64le(cur, &u64, err, "gguf.metadata.value", "uint64");
        out->as.u64 = u64;
        return rc;
    case YVEX_GGUF_VALUE_INT64:
        rc = cursor_read_u64le(cur, &u64, err, "gguf.metadata.value", "int64");
        out->as.i64 = (long long)(int64_t)(uint64_t)u64;
        return rc;
    case YVEX_GGUF_VALUE_FLOAT64:
        rc = cursor_read_u64le(cur, &u64, err, "gguf.metadata.value", "float64");
        if (rc != YVEX_OK) {
            return rc;
        }
        memcpy(&out->as.f64, &u64, sizeof(out->as.f64));
        return YVEX_OK;
    }

    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "gguf.metadata.value", "unsupported metadata value type");
    return YVEX_ERR_UNSUPPORTED;
}

static void gguf_clear(yvex_gguf *gguf)
{
    unsigned long long i;

    if (!gguf) {
        return;
    }

    if (gguf->metadata) {
        for (i = 0; i < gguf->header.metadata_count; ++i) {
            free(gguf->metadata[i].key);
            gguf->metadata[i].key = NULL;
            gguf_value_clear(&gguf->metadata[i].value);
        }
        free(gguf->metadata);
        gguf->metadata = NULL;
    }

    if (gguf->tensors) {
        for (i = 0; i < gguf->header.tensor_count; ++i) {
            free((char *)gguf->tensors[i].name);
            gguf->tensors[i].name = NULL;
        }
        free(gguf->tensors);
        gguf->tensors = NULL;
    }
}

static int parse_metadata(yvex_gguf *gguf, yvex_byte_cursor *cur, yvex_error *err)
{
    unsigned long long i;
    int rc;

    if (gguf->header.metadata_count == 0) {
        return YVEX_OK;
    }

    rc = checked_count_alloc(gguf->header.metadata_count, sizeof(*gguf->metadata), err,
                             "gguf.metadata", "metadata");
    if (rc != YVEX_OK) {
        return rc;
    }

    gguf->metadata = (yvex_gguf_metadata_entry *)calloc((size_t)gguf->header.metadata_count, sizeof(*gguf->metadata));
    if (!gguf->metadata) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf.metadata", "failed to allocate metadata table");
        return YVEX_ERR_NOMEM;
    }

    for (i = 0; i < gguf->header.metadata_count; ++i) {
        unsigned int raw_type;
        yvex_gguf_value_type type;

        rc = cursor_read_string_alloc(cur, &gguf->metadata[i].key, NULL,
                                      YVEX_GGUF_MAX_KEY_LEN, 1, err,
                                      "gguf.metadata.key", "metadata key");
        if (rc != YVEX_OK) {
            return rc;
        }

        rc = cursor_read_u32le(cur, &raw_type, err, "gguf.metadata.value", "metadata value type");
        if (rc != YVEX_OK) {
            return rc;
        }

        rc = validate_value_type(raw_type, &type, err, "gguf.metadata.value");
        if (rc != YVEX_OK) {
            return rc;
        }

        rc = parse_value(cur, type, &gguf->metadata[i].value, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    return YVEX_OK;
}

static int derive_alignment(yvex_gguf *gguf, yvex_error *err)
{
    const yvex_gguf_value *value;
    unsigned long long alignment;
    int rc;

    gguf->alignment = YVEX_GGUF_DEFAULT_ALIGNMENT;

    value = yvex_gguf_metadata_find(gguf, "general.alignment");
    if (!value) {
        return YVEX_OK;
    }

    rc = yvex_gguf_value_as_u64(value, &alignment);
    if (rc != YVEX_OK) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "gguf.metadata", "general.alignment must be an unsigned integer");
        return YVEX_ERR_FORMAT;
    }
    if (alignment == 0 || (alignment % 8ull) != 0 || alignment > (unsigned long long)UINT_MAX) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "gguf.metadata",
                        "invalid general.alignment value %llu", alignment);
        return YVEX_ERR_FORMAT;
    }

    gguf->alignment = (unsigned int)alignment;
    return YVEX_OK;
}

static const char *ggml_type_name(unsigned int type)
{
    switch (type) {
    case 0: return "F32";
    case 1: return "F16";
    case 2: return "Q4_0";
    case 3: return "Q4_1";
    case 6: return "Q5_0";
    case 7: return "Q5_1";
    case 8: return "Q8_0";
    case 9: return "Q8_1";
    case 10: return "Q2_K";
    case 11: return "Q3_K";
    case 12: return "Q4_K";
    case 13: return "Q5_K";
    case 14: return "Q6_K";
    case 15: return "Q8_K";
    case 16: return "IQ2_XXS";
    case 17: return "IQ2_XS";
    case 18: return "IQ3_XXS";
    case 19: return "IQ1_S";
    case 20: return "IQ4_NL";
    case 21: return "IQ3_S";
    case 22: return "IQ2_S";
    case 23: return "IQ4_XS";
    case 24: return "I8";
    case 25: return "I16";
    case 26: return "I32";
    case 27: return "I64";
    case 28: return "F64";
    case 29: return "IQ1_M";
    case 30: return "BF16";
    case 34: return "TQ1_0";
    case 35: return "TQ2_0";
    case 39: return "MXFP4";
    default: return "UNKNOWN";
    }
}

static int parse_tensors(yvex_gguf *gguf, yvex_byte_cursor *cur, yvex_error *err)
{
    unsigned long long i;
    int rc;

    if (gguf->header.tensor_count == 0) {
        gguf->tensor_data_offset = cur->offset;
        return YVEX_OK;
    }

    rc = checked_count_alloc(gguf->header.tensor_count, sizeof(*gguf->tensors), err,
                             "gguf.tensor_dir", "tensor");
    if (rc != YVEX_OK) {
        return rc;
    }

    gguf->tensors = (yvex_gguf_tensor_info *)calloc((size_t)gguf->header.tensor_count, sizeof(*gguf->tensors));
    if (!gguf->tensors) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf.tensor_dir", "failed to allocate tensor directory");
        return YVEX_ERR_NOMEM;
    }

    for (i = 0; i < gguf->header.tensor_count; ++i) {
        yvex_gguf_tensor_info *tensor = &gguf->tensors[i];
        char *name = NULL;
        unsigned int rank;
        unsigned int type;
        unsigned long long product = 1;
        unsigned int d;

        rc = cursor_read_string_alloc(cur, &name, NULL, YVEX_GGUF_MAX_TENSOR_NAME_LEN, 1,
                                      err, "gguf.tensor.name", "tensor name");
        if (rc != YVEX_OK) {
            return rc;
        }
        tensor->name = name;

        rc = cursor_read_u32le(cur, &rank, err, "gguf.tensor.rank", "tensor rank");
        if (rc != YVEX_OK) {
            return rc;
        }
        if (rank == 0 || rank > YVEX_GGUF_MAX_DIMS) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "gguf.tensor.rank",
                            "unsupported tensor rank %u for %s", rank, tensor->name);
            return YVEX_ERR_FORMAT;
        }
        tensor->rank = rank;

        for (d = 0; d < rank; ++d) {
            rc = cursor_read_u64le(cur, &tensor->dims[d], err, "gguf.tensor.dimensions", "tensor dimension");
            if (rc != YVEX_OK) {
                return rc;
            }
            if (tensor->dims[d] == 0) {
                yvex_error_setf(err, YVEX_ERR_FORMAT, "gguf.tensor.dimensions",
                                "zero tensor dimension for %s", tensor->name);
                return YVEX_ERR_FORMAT;
            }
            if (product > ULLONG_MAX / tensor->dims[d]) {
                yvex_error_setf(err, YVEX_ERR_BOUNDS, "gguf.tensor.dimensions",
                                "tensor dimension product overflow for %s", tensor->name);
                return YVEX_ERR_BOUNDS;
            }
            product *= tensor->dims[d];
        }

        rc = cursor_read_u32le(cur, &type, err, "gguf.tensor.type", "tensor type");
        if (rc != YVEX_OK) {
            return rc;
        }
        tensor->ggml_type = type;
        tensor->ggml_type_name = ggml_type_name(type);

        rc = cursor_read_u64le(cur, &tensor->relative_offset, err, "gguf.tensor.offset", "tensor offset");
        if (rc != YVEX_OK) {
            return rc;
        }
        if ((tensor->relative_offset % (unsigned long long)gguf->alignment) != 0) {
            yvex_error_setf(err, YVEX_ERR_FORMAT, "gguf.tensor.offset",
                            "tensor %s offset %llu is not aligned to %u",
                            tensor->name, tensor->relative_offset, gguf->alignment);
            return YVEX_ERR_FORMAT;
        }
    }

    rc = align_offset(cur->offset, gguf->alignment, &gguf->tensor_data_offset, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (gguf->tensor_data_offset > cur->size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "gguf.tensor_dir",
                        "tensor data offset %llu exceeds file size %llu",
                        gguf->tensor_data_offset, cur->size);
        return YVEX_ERR_BOUNDS;
    }

    for (i = 0; i < gguf->header.tensor_count; ++i) {
        yvex_gguf_tensor_info *tensor = &gguf->tensors[i];
        if (tensor->relative_offset > ULLONG_MAX - gguf->tensor_data_offset) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "gguf.tensor.offset",
                            "tensor %s absolute offset overflow", tensor->name);
            return YVEX_ERR_BOUNDS;
        }
        tensor->absolute_offset = gguf->tensor_data_offset + tensor->relative_offset;
        if (tensor->absolute_offset > cur->size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "gguf.tensor.offset",
                            "tensor %s absolute offset %llu exceeds file size %llu",
                            tensor->name, tensor->absolute_offset, cur->size);
            return YVEX_ERR_BOUNDS;
        }
    }

    return YVEX_OK;
}

int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err)
{
    yvex_byte_cursor cur;
    int rc;

    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_read_header", "artifact and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    if (yvex_artifact_size(artifact) < YVEX_GGUF_HEADER_BYTES) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_gguf_read_header",
                        "GGUF header requires %u bytes, file has %llu",
                        YVEX_GGUF_HEADER_BYTES, yvex_artifact_size(artifact));
        return YVEX_ERR_FORMAT;
    }

    cur.data = yvex_artifact_data(artifact);
    cur.size = yvex_artifact_size(artifact);
    cur.offset = 0;

    rc = parse_header(&cur, out, err, "yvex_gguf_read_header");
    if (rc != YVEX_OK) {
        return rc;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_gguf_probe_file(const yvex_artifact *artifact, yvex_gguf_probe *out, yvex_error *err)
{
    yvex_gguf_header header;
    int rc;

    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_probe_file", "artifact and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    if (yvex_artifact_size(artifact) >= 4) {
        const unsigned char *p = yvex_artifact_data(artifact);
        unsigned int magic = ((unsigned int)p[0]) |
                             ((unsigned int)p[1] << 8) |
                             ((unsigned int)p[2] << 16) |
                             ((unsigned int)p[3] << 24);
        if (magic != YVEX_GGUF_MAGIC) {
            out->is_gguf = 0;
            yvex_error_clear(err);
            return YVEX_OK;
        }
    }

    rc = yvex_gguf_read_header(artifact, &header, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    out->is_gguf = 1;
    out->header = header;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err)
{
    yvex_byte_cursor cur;
    yvex_gguf *gguf;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!artifact) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_open", "artifact is required");
        return YVEX_ERR_INVALID_ARG;
    }

    gguf = (yvex_gguf *)calloc(1, sizeof(*gguf));
    if (!gguf) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_gguf_open", "failed to allocate GGUF parser state");
        return YVEX_ERR_NOMEM;
    }
    gguf->alignment = YVEX_GGUF_DEFAULT_ALIGNMENT;

    cur.data = yvex_artifact_data(artifact);
    cur.size = yvex_artifact_size(artifact);
    cur.offset = 0;

    rc = parse_header(&cur, &gguf->header, err, "yvex_gguf_open");
    if (rc != YVEX_OK) {
        yvex_gguf_close(gguf);
        return rc;
    }

    rc = parse_metadata(gguf, &cur, err);
    if (rc != YVEX_OK) {
        yvex_gguf_close(gguf);
        return rc;
    }

    rc = derive_alignment(gguf, err);
    if (rc != YVEX_OK) {
        yvex_gguf_close(gguf);
        return rc;
    }

    rc = parse_tensors(gguf, &cur, err);
    if (rc != YVEX_OK) {
        yvex_gguf_close(gguf);
        return rc;
    }

    *out = gguf;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_gguf_close(yvex_gguf *gguf)
{
    if (!gguf) {
        return;
    }
    gguf_clear(gguf);
    free(gguf);
}

const yvex_gguf_header *yvex_gguf_header_view(const yvex_gguf *gguf)
{
    if (!gguf) {
        return NULL;
    }
    return &gguf->header;
}

const char *yvex_gguf_value_type_name(yvex_gguf_value_type type)
{
    switch (type) {
    case YVEX_GGUF_VALUE_UINT8: return "uint8";
    case YVEX_GGUF_VALUE_INT8: return "int8";
    case YVEX_GGUF_VALUE_UINT16: return "uint16";
    case YVEX_GGUF_VALUE_INT16: return "int16";
    case YVEX_GGUF_VALUE_UINT32: return "uint32";
    case YVEX_GGUF_VALUE_INT32: return "int32";
    case YVEX_GGUF_VALUE_FLOAT32: return "float32";
    case YVEX_GGUF_VALUE_BOOL: return "bool";
    case YVEX_GGUF_VALUE_STRING: return "string";
    case YVEX_GGUF_VALUE_ARRAY: return "array";
    case YVEX_GGUF_VALUE_UINT64: return "uint64";
    case YVEX_GGUF_VALUE_INT64: return "int64";
    case YVEX_GGUF_VALUE_FLOAT64: return "float64";
    }
    return "unknown";
}

unsigned long long yvex_gguf_metadata_count(const yvex_gguf *gguf)
{
    return gguf ? gguf->header.metadata_count : 0;
}

const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, unsigned long long index)
{
    if (!gguf || index >= gguf->header.metadata_count) {
        return NULL;
    }
    return gguf->metadata[index].key;
}

const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, unsigned long long index)
{
    if (!gguf || index >= gguf->header.metadata_count) {
        return NULL;
    }
    return &gguf->metadata[index].value;
}

const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key)
{
    unsigned long long i;

    if (!gguf || !key) {
        return NULL;
    }

    for (i = 0; i < gguf->header.metadata_count; ++i) {
        if (gguf->metadata[i].key && strcmp(gguf->metadata[i].key, key) == 0) {
            return &gguf->metadata[i].value;
        }
    }

    return NULL;
}

yvex_gguf_value_type yvex_gguf_value_type_of(const yvex_gguf_value *value)
{
    if (!value) {
        return YVEX_GGUF_VALUE_ARRAY;
    }
    return value->type;
}

int yvex_gguf_value_as_u64(const yvex_gguf_value *value, unsigned long long *out)
{
    if (!value || !out) {
        return YVEX_ERR_INVALID_ARG;
    }

    switch (value->type) {
    case YVEX_GGUF_VALUE_UINT8:
    case YVEX_GGUF_VALUE_UINT16:
    case YVEX_GGUF_VALUE_UINT32:
    case YVEX_GGUF_VALUE_UINT64:
        *out = value->as.u64;
        return YVEX_OK;
    default:
        return YVEX_ERR_INVALID_ARG;
    }
}

int yvex_gguf_value_as_i64(const yvex_gguf_value *value, long long *out)
{
    if (!value || !out) {
        return YVEX_ERR_INVALID_ARG;
    }

    switch (value->type) {
    case YVEX_GGUF_VALUE_INT8:
    case YVEX_GGUF_VALUE_INT16:
    case YVEX_GGUF_VALUE_INT32:
    case YVEX_GGUF_VALUE_INT64:
        *out = value->as.i64;
        return YVEX_OK;
    default:
        return YVEX_ERR_INVALID_ARG;
    }
}

int yvex_gguf_value_as_f64(const yvex_gguf_value *value, double *out)
{
    if (!value || !out) {
        return YVEX_ERR_INVALID_ARG;
    }

    switch (value->type) {
    case YVEX_GGUF_VALUE_FLOAT32:
    case YVEX_GGUF_VALUE_FLOAT64:
        *out = value->as.f64;
        return YVEX_OK;
    default:
        return YVEX_ERR_INVALID_ARG;
    }
}

int yvex_gguf_value_as_bool(const yvex_gguf_value *value, int *out)
{
    if (!value || !out || value->type != YVEX_GGUF_VALUE_BOOL) {
        return YVEX_ERR_INVALID_ARG;
    }
    *out = value->as.bool_value;
    return YVEX_OK;
}

int yvex_gguf_value_as_string(const yvex_gguf_value *value, const char **data, unsigned long long *len)
{
    if (!value || !data || !len || value->type != YVEX_GGUF_VALUE_STRING) {
        return YVEX_ERR_INVALID_ARG;
    }
    *data = value->as.string.data;
    *len = value->as.string.len;
    return YVEX_OK;
}

int yvex_gguf_value_array_info(const yvex_gguf_value *value, yvex_gguf_array_info *out)
{
    if (!value || !out || value->type != YVEX_GGUF_VALUE_ARRAY) {
        return YVEX_ERR_INVALID_ARG;
    }
    *out = value->as.array.info;
    return YVEX_OK;
}

const yvex_gguf_value *yvex_gguf_value_array_at(const yvex_gguf_value *value, unsigned long long index)
{
    if (!value || value->type != YVEX_GGUF_VALUE_ARRAY || index >= value->as.array.info.count) {
        return NULL;
    }
    return &value->as.array.items[index];
}

unsigned long long yvex_gguf_tensor_count(const yvex_gguf *gguf)
{
    return gguf ? gguf->header.tensor_count : 0;
}

const yvex_gguf_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, unsigned long long index)
{
    if (!gguf || index >= gguf->header.tensor_count) {
        return NULL;
    }
    return &gguf->tensors[index];
}

const yvex_gguf_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name)
{
    unsigned long long i;

    if (!gguf || !name) {
        return NULL;
    }

    for (i = 0; i < gguf->header.tensor_count; ++i) {
        if (gguf->tensors[i].name && strcmp(gguf->tensors[i].name, name) == 0) {
            return &gguf->tensors[i];
        }
    }

    return NULL;
}

unsigned long long yvex_gguf_tensor_data_offset(const yvex_gguf *gguf)
{
    return gguf ? gguf->tensor_data_offset : 0;
}

unsigned int yvex_gguf_alignment(const yvex_gguf *gguf)
{
    return gguf ? gguf->alignment : 0;
}
