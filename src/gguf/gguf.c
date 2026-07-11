/*
 * gguf/gguf.c - file-backed GGUF v3 structural parser.
 *
 * Owner:
 *   src/gguf native reader
 *
 * Owns:
 *   GGUF v3 byte decoding, immutable metadata/tensor storage, name indexes,
 *   canonical qtype projection, addressable tensor ranges, and view cleanup.
 *
 * Does not own:
 *   global tensor order/overlap/padding admission, model-family metadata,
 *   payload reads, writer emission, materialization, or runtime execution.
 *
 * Invariants:
 *   parsing uses exact positioned structural reads; payload bytes read remains
 *   zero; every allocation and declared count is budgeted; failed opens leave
 *   the output null and release all partial state.
 *
 * Boundary:
 *   a parsed structural view is not a complete or supported model artifact.
 */
#include "yvex_gguf_private.h"

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
    unsigned long long *slots;
    size_t capacity;
} yvex_name_index;

typedef struct {
    const yvex_artifact *artifact;
    unsigned long long size;
    unsigned long long offset;
    const yvex_gguf_reader_options *options;
    yvex_gguf_reader_stats *stats;
    yvex_gguf_parse_result *result;
    yvex_error *err;
    yvex_gguf_parse_section section;
    unsigned long long record_index;
} yvex_file_cursor;

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
    unsigned long long key_len;
    yvex_gguf_value value;
} yvex_gguf_metadata_entry;

struct yvex_gguf {
    yvex_gguf_header header;
    yvex_gguf_metadata_entry *metadata;
    yvex_gguf_tensor_info *tensors;
    yvex_name_index metadata_index;
    yvex_name_index tensor_index;
    unsigned long long tensor_data_offset;
    unsigned int alignment;
    yvex_gguf_reader_stats stats;
};

static int cursor_fail(yvex_file_cursor *cur,
                       yvex_gguf_parse_code code,
                       yvex_gguf_parse_section section,
                       const char *where,
                       const char *reason)
{
    return yvex_gguf_reader_fail(cur ? cur->result : NULL,
                                 code,
                                 section,
                                 cur ? cur->offset : 0ull,
                                 cur ? cur->record_index : ULLONG_MAX,
                                 cur ? cur->err : NULL,
                                 where,
                                 reason);
}

/* Contract: reserves owned parser memory using checked cumulative accounting. */
static int reserve_owned(yvex_file_cursor *cur, unsigned long long bytes)
{
    if (bytes > ULLONG_MAX - cur->stats->owned_bytes ||
        cur->stats->owned_bytes + bytes > cur->options->max_owned_bytes) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF owned-memory budget exceeded");
    }
    cur->stats->owned_bytes += bytes;
    return YVEX_OK;
}

/* Contract: performs one exact bounded structural read and updates read metrics. */
static int cursor_read_exact(yvex_file_cursor *cur,
                             void *dst,
                             size_t len,
                             const char *where,
                             const char *field)
{
    unsigned long long amount = (unsigned long long)len;
    int rc;

    if (amount > cur->size - (cur->offset <= cur->size ? cur->offset : cur->size)) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_SHORT_READ, cur->section, where,
                           field ? field : "GGUF structural read is truncated");
    }
    if (amount > ULLONG_MAX - cur->stats->structural_bytes_read ||
        cur->stats->structural_bytes_read + amount > cur->options->max_structural_bytes) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF structural-read budget exceeded");
    }

    rc = yvex_artifact_read_at(cur->artifact, cur->offset, dst, len, cur->err);
    if (rc != YVEX_OK) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_SHORT_READ, cur->section, where,
                           field ? field : "GGUF positioned read failed");
    }
    cur->offset += amount;
    cur->stats->structural_bytes_read += amount;
    cur->stats->read_calls += 1ull;
    return YVEX_OK;
}

static int cursor_read_u8(yvex_file_cursor *cur, unsigned char *out,
                          const char *where, const char *field)
{
    return cursor_read_exact(cur, out, 1u, where, field);
}

static int cursor_read_u16le(yvex_file_cursor *cur, unsigned int *out,
                             const char *where, const char *field)
{
    unsigned char bytes[2];
    int rc = cursor_read_exact(cur, bytes, sizeof(bytes), where, field);
    if (rc != YVEX_OK) return rc;
    *out = (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
    return YVEX_OK;
}

static int cursor_read_u32le(yvex_file_cursor *cur, unsigned int *out,
                             const char *where, const char *field)
{
    unsigned char bytes[4];
    int rc = cursor_read_exact(cur, bytes, sizeof(bytes), where, field);
    if (rc != YVEX_OK) return rc;
    *out = (unsigned int)bytes[0] |
           ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) |
           ((unsigned int)bytes[3] << 24);
    return YVEX_OK;
}

static int cursor_read_u64le(yvex_file_cursor *cur, unsigned long long *out,
                             const char *where, const char *field)
{
    unsigned char bytes[8];
    int rc = cursor_read_exact(cur, bytes, sizeof(bytes), where, field);
    if (rc != YVEX_OK) return rc;
    *out = (unsigned long long)bytes[0] |
           ((unsigned long long)bytes[1] << 8) |
           ((unsigned long long)bytes[2] << 16) |
           ((unsigned long long)bytes[3] << 24) |
           ((unsigned long long)bytes[4] << 32) |
           ((unsigned long long)bytes[5] << 40) |
           ((unsigned long long)bytes[6] << 48) |
           ((unsigned long long)bytes[7] << 56);
    return YVEX_OK;
}

/* Contract: validates one length-delimited UTF-8 sequence without reading past len. */
static int utf8_valid(const unsigned char *text, size_t len)
{
    size_t i = 0u;
    while (i < len) {
        unsigned char first = text[i++];
        unsigned int need;
        unsigned int codepoint;
        unsigned int minimum;
        unsigned int j;

        if (first < 0x80u) continue;
        if (first >= 0xc2u && first <= 0xdfu) {
            need = 1u;
            codepoint = (unsigned int)(first & 0x1fu);
            minimum = 0x80u;
        } else if (first >= 0xe0u && first <= 0xefu) {
            need = 2u;
            codepoint = (unsigned int)(first & 0x0fu);
            minimum = 0x800u;
        } else if (first >= 0xf0u && first <= 0xf4u) {
            need = 3u;
            codepoint = (unsigned int)(first & 0x07u);
            minimum = 0x10000u;
        } else {
            return 0;
        }
        if (need > len - i) return 0;
        for (j = 0u; j < need; ++j) {
            unsigned char continuation = text[i++];
            if ((continuation & 0xc0u) != 0x80u) return 0;
            codepoint = (codepoint << 6) | (unsigned int)(continuation & 0x3fu);
        }
        if (codepoint < minimum || codepoint > 0x10ffffu ||
            (codepoint >= 0xd800u && codepoint <= 0xdfffu)) {
            return 0;
        }
    }
    return 1;
}

static yvex_gguf_parse_code string_parse_code(const yvex_file_cursor *cur,
                                              unsigned long long format_max)
{
    if (format_max == 0ull) return YVEX_GGUF_PARSE_MALFORMED_STRING;
    return cur->section == YVEX_GGUF_PARSE_SECTION_METADATA
               ? YVEX_GGUF_PARSE_MALFORMED_KEY
               : YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME;
}

static yvex_gguf_parse_code empty_identifier_parse_code(const yvex_file_cursor *cur)
{
    return cur->section == YVEX_GGUF_PARSE_SECTION_METADATA
               ? YVEX_GGUF_PARSE_EMPTY_METADATA_KEY
               : YVEX_GGUF_PARSE_EMPTY_TENSOR_NAME;
}

/* Contract: reads one owned length-delimited string under cumulative budgets. */
static int cursor_read_string(yvex_file_cursor *cur,
                              char **out,
                              unsigned long long *out_len,
                              unsigned long long format_max,
                              int reject_empty,
                              int reject_nul,
                              const char *where,
                              const char *field)
{
    unsigned long long len;
    char *copy;
    int rc;

    *out = NULL;
    if (out_len) *out_len = 0ull;
    rc = cursor_read_u64le(cur, &len, where, field);
    if (rc != YVEX_OK) return rc;
    if (reject_empty && len == 0ull) {
        return cursor_fail(cur, empty_identifier_parse_code(cur), cur->section,
                           where, "GGUF identifier string is empty");
    }
    if (format_max && len > format_max) {
        return cursor_fail(cur, cur->section == YVEX_GGUF_PARSE_SECTION_METADATA
                                    ? YVEX_GGUF_PARSE_MALFORMED_KEY
                                    : YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME,
                           cur->section, where,
                           "GGUF identifier exceeds the pinned format limit");
    }
    if (len > cur->options->max_string_bytes) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF string exceeds configured limit");
    }
    if (len > (unsigned long long)(SIZE_MAX - 1u) ||
        len > ULLONG_MAX - cur->stats->total_string_bytes ||
        cur->stats->total_string_bytes + len > cur->options->max_total_string_bytes) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF cumulative string budget exceeded");
    }
    rc = reserve_owned(cur, len + 1ull);
    if (rc != YVEX_OK) return rc;
    copy = (char *)malloc((size_t)len + 1u);
    if (!copy) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "failed to allocate GGUF string");
    }
    if (len > 0ull) {
        rc = cursor_read_exact(cur, copy, (size_t)len, where, field);
        if (rc != YVEX_OK) {
            free(copy);
            return cursor_fail(cur, string_parse_code(cur, format_max),
                               cur->section, where,
                               field ? field : "GGUF string is truncated");
        }
    }
    if (reject_nul && memchr(copy, '\0', (size_t)len) != NULL) {
        free(copy);
        return cursor_fail(cur, cur->section == YVEX_GGUF_PARSE_SECTION_METADATA
                                    ? YVEX_GGUF_PARSE_MALFORMED_KEY
                                    : YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME,
                           cur->section, where,
                           "GGUF identifier contains an embedded null byte");
    }
    if (!utf8_valid((const unsigned char *)copy, (size_t)len)) {
        free(copy);
        return cursor_fail(cur, string_parse_code(cur, format_max),
                           cur->section, where, "GGUF string is not valid UTF-8");
    }
    copy[len] = '\0';
    cur->stats->total_string_bytes += len;
    *out = copy;
    if (out_len) *out_len = len;
    return YVEX_OK;
}

/* Contract: validates the pinned GGUF metadata-key grammar without mutation. */
static int metadata_key_valid(const char *key, unsigned long long len)
{
    unsigned long long i;
    int segment_start = 1;
    int previous_underscore = 0;

    if (!key || len == 0ull) return 0;
    for (i = 0ull; i < len; ++i) {
        unsigned char ch = (unsigned char)key[i];
        if (ch >= 128u) return 0;
        if (ch == '.') {
            if (segment_start || previous_underscore || i + 1ull == len) return 0;
            segment_start = 1;
            previous_underscore = 0;
            continue;
        }
        if (segment_start) {
            if (ch < 'a' || ch > 'z') return 0;
            segment_start = 0;
            previous_underscore = 0;
            continue;
        }
        if (ch == '_') {
            if (previous_underscore) return 0;
            previous_underscore = 1;
        } else {
            if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9'))) return 0;
            previous_underscore = 0;
        }
    }
    return !segment_start && !previous_underscore;
}

static unsigned long long name_hash(const char *name, unsigned long long len)
{
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;
    for (i = 0ull; i < len; ++i) {
        hash ^= (unsigned char)name[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Contract: allocates an O(1)-average duplicate/lookup index under parser budgets. */
static int name_index_allocate(yvex_file_cursor *cur,
                               unsigned long long count,
                               yvex_name_index *index)
{
    unsigned long long wanted;
    size_t capacity = 8u;
    unsigned long long bytes;
    int rc;

    if (count == 0ull) return YVEX_OK;
    if (count > ULLONG_MAX / 2ull) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_COUNT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF identifier count overflows index sizing");
    }
    wanted = count * 2ull;
    while ((unsigned long long)capacity < wanted) {
        if (capacity > SIZE_MAX / 2u) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                               YVEX_GGUF_PARSE_SECTION_RESOURCE,
                               "gguf.resource", "GGUF identifier index exceeds address space");
        }
        capacity *= 2u;
    }
    if (capacity > SIZE_MAX / sizeof(*index->slots)) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF identifier index allocation overflows");
    }
    bytes = (unsigned long long)capacity * (unsigned long long)sizeof(*index->slots);
    rc = reserve_owned(cur, bytes);
    if (rc != YVEX_OK) return rc;
    index->slots = (unsigned long long *)calloc(capacity, sizeof(*index->slots));
    if (!index->slots) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "failed to allocate GGUF identifier index");
    }
    index->capacity = capacity;
    return YVEX_OK;
}

static size_t name_index_start(const yvex_name_index *index,
                               const char *name,
                               unsigned long long len)
{
    return (size_t)(name_hash(name, len) & (unsigned long long)(index->capacity - 1u));
}

/* Contract: inserts one metadata key and refuses duplicates in expected O(1). */
static int metadata_index_insert(yvex_gguf *gguf,
                                 yvex_file_cursor *cur,
                                 unsigned long long entry_index)
{
    yvex_gguf_metadata_entry *entry = &gguf->metadata[entry_index];
    size_t slot = name_index_start(&gguf->metadata_index, entry->key, entry->key_len);

    while (gguf->metadata_index.slots[slot] != 0ull) {
        unsigned long long prior_index = gguf->metadata_index.slots[slot] - 1ull;
        yvex_gguf_metadata_entry *prior = &gguf->metadata[prior_index];
        if (prior->key_len == entry->key_len &&
            memcmp(prior->key, entry->key, (size_t)entry->key_len) == 0) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_DUPLICATE_METADATA_KEY,
                               YVEX_GGUF_PARSE_SECTION_METADATA,
                               "gguf.metadata.key", "duplicate GGUF metadata key");
        }
        slot = (slot + 1u) & (gguf->metadata_index.capacity - 1u);
    }
    gguf->metadata_index.slots[slot] = entry_index + 1ull;
    return YVEX_OK;
}

/* Contract: inserts one tensor name and refuses duplicates in expected O(1). */
static int tensor_index_insert(yvex_gguf *gguf,
                               yvex_file_cursor *cur,
                               unsigned long long tensor_index)
{
    yvex_gguf_tensor_info *tensor = &gguf->tensors[tensor_index];
    size_t slot = name_index_start(&gguf->tensor_index, tensor->name, tensor->name_len);

    while (gguf->tensor_index.slots[slot] != 0ull) {
        unsigned long long prior_index = gguf->tensor_index.slots[slot] - 1ull;
        yvex_gguf_tensor_info *prior = &gguf->tensors[prior_index];
        if (prior->name_len == tensor->name_len &&
            memcmp(prior->name, tensor->name, (size_t)tensor->name_len) == 0) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_DUPLICATE_TENSOR_NAME,
                               YVEX_GGUF_PARSE_SECTION_TENSOR_INFO,
                               "gguf.tensor.name", "duplicate GGUF tensor name");
        }
        slot = (slot + 1u) & (gguf->tensor_index.capacity - 1u);
    }
    gguf->tensor_index.slots[slot] = tensor_index + 1ull;
    return YVEX_OK;
}

static void gguf_value_clear(yvex_gguf_value *value)
{
    unsigned long long i;
    if (!value) return;
    if (value->type == YVEX_GGUF_VALUE_STRING) {
        free(value->as.string.data);
    } else if (value->type == YVEX_GGUF_VALUE_ARRAY) {
        for (i = 0ull; i < value->as.array.info.count; ++i) {
            gguf_value_clear(&value->as.array.items[i]);
        }
        free(value->as.array.items);
    }
    memset(value, 0, sizeof(*value));
    value->type = YVEX_GGUF_VALUE_INVALID;
}

static int parse_value_type(yvex_file_cursor *cur,
                            unsigned int raw,
                            yvex_gguf_value_type *out)
{
    if (raw > (unsigned int)YVEX_GGUF_VALUE_FLOAT64) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE,
                           YVEX_GGUF_PARSE_SECTION_METADATA,
                           "gguf.metadata.type", "unsupported GGUF metadata value type");
    }
    *out = (yvex_gguf_value_type)raw;
    return YVEX_OK;
}

/* Contract: recursively decodes one metadata value under depth and allocation budgets. */
static int parse_value(yvex_file_cursor *cur,
                       yvex_gguf_value_type type,
                       yvex_gguf_value *out,
                       unsigned int depth)
{
    unsigned char u8;
    unsigned int u16;
    unsigned int u32;
    unsigned long long u64;
    int rc;

    memset(out, 0, sizeof(*out));
    out->type = type;
    switch (type) {
    case YVEX_GGUF_VALUE_UINT8:
        rc = cursor_read_u8(cur, &u8, "gguf.metadata.value", "truncated uint8 metadata value");
        if (rc == YVEX_OK) out->as.u64 = (unsigned long long)u8;
        return rc;
    case YVEX_GGUF_VALUE_INT8:
        rc = cursor_read_u8(cur, &u8, "gguf.metadata.value", "truncated int8 metadata value");
        if (rc == YVEX_OK) out->as.i64 = (long long)(int8_t)u8;
        return rc;
    case YVEX_GGUF_VALUE_UINT16:
        rc = cursor_read_u16le(cur, &u16, "gguf.metadata.value", "truncated uint16 metadata value");
        if (rc == YVEX_OK) out->as.u64 = (unsigned long long)u16;
        return rc;
    case YVEX_GGUF_VALUE_INT16:
        rc = cursor_read_u16le(cur, &u16, "gguf.metadata.value", "truncated int16 metadata value");
        if (rc == YVEX_OK) out->as.i64 = (long long)(int16_t)(uint16_t)u16;
        return rc;
    case YVEX_GGUF_VALUE_UINT32:
        rc = cursor_read_u32le(cur, &u32, "gguf.metadata.value", "truncated uint32 metadata value");
        if (rc == YVEX_OK) out->as.u64 = (unsigned long long)u32;
        return rc;
    case YVEX_GGUF_VALUE_INT32:
        rc = cursor_read_u32le(cur, &u32, "gguf.metadata.value", "truncated int32 metadata value");
        if (rc == YVEX_OK) out->as.i64 = (long long)(int32_t)(uint32_t)u32;
        return rc;
    case YVEX_GGUF_VALUE_FLOAT32: {
        float f32;
        rc = cursor_read_u32le(cur, &u32, "gguf.metadata.value", "truncated float32 metadata value");
        if (rc != YVEX_OK) return rc;
        memcpy(&f32, &u32, sizeof(f32));
        out->as.f64 = (double)f32;
        return YVEX_OK;
    }
    case YVEX_GGUF_VALUE_BOOL:
        rc = cursor_read_u8(cur, &u8, "gguf.metadata.value", "truncated bool metadata value");
        if (rc != YVEX_OK) return rc;
        if (u8 > 1u) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_MALFORMED_VALUE,
                               YVEX_GGUF_PARSE_SECTION_METADATA,
                               "gguf.metadata.value", "GGUF bool metadata value must be zero or one");
        }
        out->as.bool_value = u8 ? 1 : 0;
        return YVEX_OK;
    case YVEX_GGUF_VALUE_STRING:
        return cursor_read_string(cur, &out->as.string.data, &out->as.string.len,
                                  0ull, 0, 0, "gguf.metadata.value",
                                  "truncated GGUF metadata string");
    case YVEX_GGUF_VALUE_ARRAY: {
        unsigned int raw_type;
        yvex_gguf_value_type element_type;
        unsigned long long count;
        unsigned long long i;
        unsigned long long bytes;

        if (depth >= cur->options->max_array_depth) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                               YVEX_GGUF_PARSE_SECTION_RESOURCE,
                               "gguf.resource", "GGUF array nesting depth exceeds configured limit");
        }
        rc = cursor_read_u32le(cur, &raw_type, "gguf.metadata.array",
                               "truncated GGUF array element type");
        if (rc != YVEX_OK) return rc;
        rc = parse_value_type(cur, raw_type, &element_type);
        if (rc != YVEX_OK) return rc;
        rc = cursor_read_u64le(cur, &count, "gguf.metadata.array",
                               "truncated GGUF array count");
        if (rc != YVEX_OK) return rc;
        if (count > cur->options->max_array_entries ||
            count > ULLONG_MAX - cur->stats->total_array_entries ||
            cur->stats->total_array_entries + count > cur->options->max_total_array_entries) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                               YVEX_GGUF_PARSE_SECTION_RESOURCE,
                               "gguf.resource", "GGUF array entry budget exceeded");
        }
        if (count > (unsigned long long)(SIZE_MAX / sizeof(*out->as.array.items))) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                               YVEX_GGUF_PARSE_SECTION_RESOURCE,
                               "gguf.resource", "GGUF array allocation exceeds address space");
        }
        out->as.array.info.element_type = element_type;
        out->as.array.info.count = count;
        cur->stats->total_array_entries += count;
        if (count == 0ull) return YVEX_OK;
        bytes = count * (unsigned long long)sizeof(*out->as.array.items);
        rc = reserve_owned(cur, bytes);
        if (rc != YVEX_OK) return rc;
        out->as.array.items = (yvex_gguf_value *)calloc((size_t)count,
                                                        sizeof(*out->as.array.items));
        if (!out->as.array.items) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                               YVEX_GGUF_PARSE_SECTION_RESOURCE,
                               "gguf.resource", "failed to allocate GGUF array items");
        }
        for (i = 0ull; i < count; ++i) {
            out->as.array.items[i].type = YVEX_GGUF_VALUE_INVALID;
            rc = parse_value(cur, element_type, &out->as.array.items[i], depth + 1u);
            if (rc != YVEX_OK) {
                gguf_value_clear(out);
                return rc;
            }
        }
        return YVEX_OK;
    }
    case YVEX_GGUF_VALUE_UINT64:
        rc = cursor_read_u64le(cur, &u64, "gguf.metadata.value", "truncated uint64 metadata value");
        if (rc == YVEX_OK) out->as.u64 = u64;
        return rc;
    case YVEX_GGUF_VALUE_INT64:
        rc = cursor_read_u64le(cur, &u64, "gguf.metadata.value", "truncated int64 metadata value");
        if (rc == YVEX_OK) out->as.i64 = (long long)(int64_t)(uint64_t)u64;
        return rc;
    case YVEX_GGUF_VALUE_FLOAT64:
        rc = cursor_read_u64le(cur, &u64, "gguf.metadata.value", "truncated float64 metadata value");
        if (rc == YVEX_OK) memcpy(&out->as.f64, &u64, sizeof(out->as.f64));
        return rc;
    case YVEX_GGUF_VALUE_INVALID:
        break;
    }
    return cursor_fail(cur, YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE,
                       YVEX_GGUF_PARSE_SECTION_METADATA,
                       "gguf.metadata.type", "unsupported GGUF metadata value type");
}

/* Contract: parses and validates the fixed GGUF v3 container header. */
static int parse_header(yvex_file_cursor *cur, yvex_gguf_header *out)
{
    unsigned int magic;
    int rc;
    cur->section = YVEX_GGUF_PARSE_SECTION_CONTAINER;
    cur->record_index = ULLONG_MAX;
    memset(out, 0, sizeof(*out));
    rc = cursor_read_u32le(cur, &magic, "gguf.container", "truncated GGUF magic");
    if (rc != YVEX_OK) return rc;
    if (magic != YVEX_GGUF_MAGIC) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_MAGIC,
                           YVEX_GGUF_PARSE_SECTION_CONTAINER,
                           "gguf.container", "invalid GGUF magic");
    }
    rc = cursor_read_u32le(cur, &out->version, "gguf.container", "truncated GGUF version");
    if (rc != YVEX_OK) return rc;
    if (out->version != YVEX_GGUF_SUPPORTED_VERSION) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_UNSUPPORTED_VERSION,
                           YVEX_GGUF_PARSE_SECTION_CONTAINER,
                           "gguf.container", "unsupported GGUF container version");
    }
    rc = cursor_read_u64le(cur, &out->tensor_count, "gguf.container", "truncated GGUF tensor count");
    if (rc != YVEX_OK) return rc;
    rc = cursor_read_u64le(cur, &out->metadata_count, "gguf.container", "truncated GGUF metadata count");
    if (rc != YVEX_OK) return rc;
    if (out->metadata_count > cur->options->max_metadata_entries ||
        out->tensor_count > cur->options->max_tensor_entries) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF directory count exceeds configured limit");
    }
    return YVEX_OK;
}

/* Contract: parses metadata once, validates key grammar, and builds its hash index. */
static int parse_metadata(yvex_gguf *gguf, yvex_file_cursor *cur)
{
    unsigned long long count = gguf->header.metadata_count;
    unsigned long long bytes;
    unsigned long long i;
    int rc;

    cur->section = YVEX_GGUF_PARSE_SECTION_METADATA;
    if (count == 0ull) return YVEX_OK;
    if (count > (unsigned long long)(SIZE_MAX / sizeof(*gguf->metadata))) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_COUNT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF metadata count exceeds address space");
    }
    bytes = count * (unsigned long long)sizeof(*gguf->metadata);
    rc = reserve_owned(cur, bytes);
    if (rc != YVEX_OK) return rc;
    gguf->metadata = (yvex_gguf_metadata_entry *)calloc((size_t)count,
                                                        sizeof(*gguf->metadata));
    if (!gguf->metadata) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "failed to allocate GGUF metadata table");
    }
    rc = name_index_allocate(cur, count, &gguf->metadata_index);
    if (rc != YVEX_OK) return rc;

    for (i = 0ull; i < count; ++i) {
        unsigned int raw_type;
        yvex_gguf_value_type type;
        cur->record_index = i;
        rc = cursor_read_string(cur, &gguf->metadata[i].key,
                                &gguf->metadata[i].key_len,
                                YVEX_GGUF_MAX_KEY_LEN, 1, 1,
                                "gguf.metadata.key", "truncated GGUF metadata key");
        if (rc != YVEX_OK) return rc;
        if (!metadata_key_valid(gguf->metadata[i].key, gguf->metadata[i].key_len)) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_MALFORMED_KEY,
                               YVEX_GGUF_PARSE_SECTION_METADATA,
                               "gguf.metadata.key", "GGUF metadata key violates pinned key grammar");
        }
        rc = metadata_index_insert(gguf, cur, i);
        if (rc != YVEX_OK) return rc;
        rc = cursor_read_u32le(cur, &raw_type, "gguf.metadata.type",
                               "truncated GGUF metadata value type");
        if (rc != YVEX_OK) return rc;
        rc = parse_value_type(cur, raw_type, &type);
        if (rc != YVEX_OK) return rc;
        gguf->metadata[i].value.type = YVEX_GGUF_VALUE_INVALID;
        rc = parse_value(cur, type, &gguf->metadata[i].value, 0u);
        if (rc != YVEX_OK) return rc;
    }
    cur->record_index = ULLONG_MAX;
    return YVEX_OK;
}

/* Contract: derives and validates local alignment policy from parsed metadata. */
static int derive_alignment(yvex_gguf *gguf, yvex_file_cursor *cur)
{
    const yvex_gguf_value *value;
    unsigned long long alignment;
    gguf->alignment = YVEX_GGUF_DEFAULT_ALIGNMENT;
    value = yvex_gguf_metadata_find(gguf, "general.alignment");
    if (!value) return YVEX_OK;
    if (yvex_gguf_value_type_of(value) != YVEX_GGUF_VALUE_UINT32 ||
        yvex_gguf_value_as_u64(value, &alignment) != YVEX_OK ||
        alignment == 0ull || alignment > (unsigned long long)UINT_MAX ||
        (alignment % 8ull) != 0ull) {
        cur->section = YVEX_GGUF_PARSE_SECTION_METADATA;
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_ALIGNMENT,
                           YVEX_GGUF_PARSE_SECTION_METADATA,
                           "gguf.metadata.alignment",
                           "general.alignment must be uint32 and a nonzero multiple of eight");
    }
    gguf->alignment = (unsigned int)alignment;
    return YVEX_OK;
}

static int align_offset(yvex_file_cursor *cur,
                        unsigned long long offset,
                        unsigned int alignment,
                        unsigned long long *out)
{
    unsigned long long remainder;
    unsigned long long padding;

    if (alignment == 0u) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_ALIGNMENT,
                           YVEX_GGUF_PARSE_SECTION_RANGE,
                           "gguf.range", "GGUF alignment is zero");
    }
    remainder = offset % (unsigned long long)alignment;
    padding = remainder == 0ull ? 0ull : (unsigned long long)alignment - remainder;
    if (offset > ULLONG_MAX - padding) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_OFFSET_OVERFLOW,
                           YVEX_GGUF_PARSE_SECTION_RANGE,
                           "gguf.range", "GGUF tensor-data alignment overflows");
    }
    *out = offset + padding;
    return YVEX_OK;
}

static yvex_gguf_parse_code qtype_parse_code(yvex_gguf_qtype_storage_status status)
{
    if (status == YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK) return YVEX_GGUF_PARSE_INVALID_RANK;
    if (status == YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION) return YVEX_GGUF_PARSE_INVALID_DIMENSION;
    if (status == YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW) return YVEX_GGUF_PARSE_ELEMENT_COUNT_OVERFLOW;
    if (status == YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW) return YVEX_GGUF_PARSE_ROW_COUNT_OVERFLOW;
    if (status == YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW) return YVEX_GGUF_PARSE_ROW_BYTE_OVERFLOW;
    if (status == YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW) return YVEX_GGUF_PARSE_TOTAL_BYTE_OVERFLOW;
    return YVEX_GGUF_PARSE_REFUSED_QTYPE;
}

/* Contract: parses tensor_info rows once and computes local addressable ranges. */
static int parse_tensors(yvex_gguf *gguf, yvex_file_cursor *cur)
{
    unsigned long long count = gguf->header.tensor_count;
    unsigned long long bytes;
    unsigned long long i;
    int rc;

    cur->section = YVEX_GGUF_PARSE_SECTION_TENSOR_INFO;
    if (count == 0ull) {
        gguf->stats.directory_end_offset = cur->offset;
        return align_offset(cur, cur->offset, gguf->alignment,
                            &gguf->tensor_data_offset);
    }
    if (count > (unsigned long long)(SIZE_MAX / sizeof(*gguf->tensors))) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_COUNT,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "GGUF tensor count exceeds address space");
    }
    bytes = count * (unsigned long long)sizeof(*gguf->tensors);
    rc = reserve_owned(cur, bytes);
    if (rc != YVEX_OK) return rc;
    gguf->tensors = (yvex_gguf_tensor_info *)calloc((size_t)count,
                                                    sizeof(*gguf->tensors));
    if (!gguf->tensors) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                           YVEX_GGUF_PARSE_SECTION_RESOURCE,
                           "gguf.resource", "failed to allocate GGUF tensor directory");
    }
    rc = name_index_allocate(cur, count, &gguf->tensor_index);
    if (rc != YVEX_OK) return rc;

    for (i = 0ull; i < count; ++i) {
        yvex_gguf_tensor_info *tensor = &gguf->tensors[i];
        char *name = NULL;
        unsigned int rank;
        unsigned int type;
        unsigned int d;
        yvex_gguf_qtype_storage_result storage;
        yvex_gguf_qtype_storage_status storage_status;

        cur->record_index = i;
        rc = cursor_read_string(cur, &name, &tensor->name_len,
                                YVEX_GGUF_MAX_TENSOR_NAME_LEN, 1, 1,
                                "gguf.tensor.name", "truncated GGUF tensor name");
        if (rc != YVEX_OK) return rc;
        tensor->name = name;
        rc = tensor_index_insert(gguf, cur, i);
        if (rc != YVEX_OK) return rc;
        rc = cursor_read_u32le(cur, &rank, "gguf.tensor.rank", "truncated GGUF tensor rank");
        if (rc != YVEX_OK) return rc;
        if (rank == 0u || rank > YVEX_GGUF_MAX_DIMS) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_RANK,
                               YVEX_GGUF_PARSE_SECTION_TENSOR_INFO,
                               "gguf.tensor.rank", "GGUF tensor rank is outside the admitted range");
        }
        tensor->rank = rank;
        for (d = 0u; d < rank; ++d) {
            rc = cursor_read_u64le(cur, &tensor->dims[d], "gguf.tensor.dimension",
                                   "truncated GGUF tensor dimension");
            if (rc != YVEX_OK) return rc;
            if (tensor->dims[d] == 0ull) {
                return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_DIMENSION,
                                   YVEX_GGUF_PARSE_SECTION_TENSOR_INFO,
                                   "gguf.tensor.dimension", "GGUF tensor dimension is zero");
            }
        }
        rc = cursor_read_u32le(cur, &type, "gguf.tensor.qtype", "truncated GGUF tensor qtype");
        if (rc != YVEX_OK) return rc;
        tensor->ggml_type = type;
        tensor->ggml_type_name = yvex_gguf_qtype_name(type);
        storage_status = yvex_gguf_qtype_tensor_storage(type, tensor->dims,
                                                        tensor->rank, &storage);
        if (storage_status != YVEX_GGUF_QTYPE_STORAGE_OK) {
            cur->section = YVEX_GGUF_PARSE_SECTION_QTYPE;
            return cursor_fail(cur, qtype_parse_code(storage_status),
                               YVEX_GGUF_PARSE_SECTION_QTYPE,
                               "gguf.tensor.qtype", storage.reason);
        }
        tensor->storage_bytes = storage.total_bytes;
        cur->section = YVEX_GGUF_PARSE_SECTION_TENSOR_INFO;
        rc = cursor_read_u64le(cur, &tensor->relative_offset, "gguf.tensor.offset",
                               "truncated GGUF tensor relative offset");
        if (rc != YVEX_OK) return rc;
        if ((tensor->relative_offset & ((unsigned long long)gguf->alignment - 1ull)) != 0ull) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_INVALID_ALIGNMENT,
                               YVEX_GGUF_PARSE_SECTION_TENSOR_INFO,
                               "gguf.tensor.offset", "GGUF tensor relative offset is misaligned");
        }
    }

    gguf->stats.directory_end_offset = cur->offset;
    cur->section = YVEX_GGUF_PARSE_SECTION_RANGE;
    cur->record_index = ULLONG_MAX;
    rc = align_offset(cur, cur->offset, gguf->alignment, &gguf->tensor_data_offset);
    if (rc != YVEX_OK) return rc;
    if (gguf->tensor_data_offset > cur->size) {
        return cursor_fail(cur, YVEX_GGUF_PARSE_INCOMPLETE_DIRECTORY,
                           YVEX_GGUF_PARSE_SECTION_RANGE,
                           "gguf.range", "GGUF tensor-data boundary exceeds file size");
    }
    for (i = 0ull; i < count; ++i) {
        yvex_gguf_tensor_info *tensor = &gguf->tensors[i];
        cur->record_index = i;
        if (tensor->relative_offset > ULLONG_MAX - gguf->tensor_data_offset) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_OFFSET_OVERFLOW,
                               YVEX_GGUF_PARSE_SECTION_RANGE,
                               "gguf.range", "GGUF tensor absolute offset overflows");
        }
        tensor->absolute_offset = gguf->tensor_data_offset + tensor->relative_offset;
        if (tensor->storage_bytes > ULLONG_MAX - tensor->absolute_offset) {
            return cursor_fail(cur, YVEX_GGUF_PARSE_OFFSET_OVERFLOW,
                               YVEX_GGUF_PARSE_SECTION_RANGE,
                               "gguf.range", "GGUF tensor absolute range overflows");
        }
        tensor->absolute_end_offset = tensor->absolute_offset + tensor->storage_bytes;
        tensor->range_addressable = tensor->absolute_end_offset <= cur->size ? 1 : 0;
    }
    cur->record_index = ULLONG_MAX;
    return YVEX_OK;
}

/* Contract: releases every owned string, array, table, and lookup index. */
static void gguf_clear(yvex_gguf *gguf)
{
    unsigned long long i;
    if (!gguf) return;
    for (i = 0ull; gguf->metadata && i < gguf->header.metadata_count; ++i) {
        free(gguf->metadata[i].key);
        gguf_value_clear(&gguf->metadata[i].value);
    }
    free(gguf->metadata);
    for (i = 0ull; gguf->tensors && i < gguf->header.tensor_count; ++i) {
        free((char *)gguf->tensors[i].name);
    }
    free(gguf->tensors);
    free(gguf->metadata_index.slots);
    free(gguf->tensor_index.slots);
    gguf->metadata = NULL;
    gguf->tensors = NULL;
    gguf->metadata_index.slots = NULL;
    gguf->tensor_index.slots = NULL;
}

static unsigned int decode_u32le(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
           ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) |
           ((unsigned int)bytes[3] << 24);
}

static unsigned long long decode_u64le(const unsigned char *bytes)
{
    return (unsigned long long)bytes[0] |
           ((unsigned long long)bytes[1] << 8) |
           ((unsigned long long)bytes[2] << 16) |
           ((unsigned long long)bytes[3] << 24) |
           ((unsigned long long)bytes[4] << 32) |
           ((unsigned long long)bytes[5] << 40) |
           ((unsigned long long)bytes[6] << 48) |
           ((unsigned long long)bytes[7] << 56);
}

/* Contract: reads only the fixed 24-byte header through positioned IO. */
int yvex_gguf_read_header(const yvex_artifact *artifact,
                          yvex_gguf_header *out,
                          yvex_error *err)
{
    unsigned char bytes[YVEX_GGUF_HEADER_BYTES];
    unsigned int magic;
    int rc;
    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_read_header",
                       "artifact and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (yvex_artifact_size(artifact) < YVEX_GGUF_HEADER_BYTES) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_gguf_read_header",
                       "GGUF header is truncated");
        return YVEX_ERR_FORMAT;
    }
    rc = yvex_artifact_read_at(artifact, 0ull, bytes, sizeof(bytes), err);
    if (rc != YVEX_OK) return rc;
    magic = decode_u32le(bytes);
    if (magic != YVEX_GGUF_MAGIC) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_gguf_read_header",
                       "invalid GGUF magic");
        return YVEX_ERR_FORMAT;
    }
    out->version = decode_u32le(bytes + 4u);
    if (out->version != YVEX_GGUF_SUPPORTED_VERSION) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_gguf_read_header",
                       "unsupported GGUF container version");
        return YVEX_ERR_UNSUPPORTED;
    }
    out->tensor_count = decode_u64le(bytes + 8u);
    out->metadata_count = decode_u64le(bytes + 16u);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: probes GGUF magic and the fixed header without allocating parser state. */
int yvex_gguf_probe_file(const yvex_artifact *artifact,
                         yvex_gguf_probe *out,
                         yvex_error *err)
{
    unsigned char bytes[4];
    int rc;
    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_probe_file",
                       "artifact and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    if (yvex_artifact_size(artifact) >= 4ull) {
        rc = yvex_artifact_read_at(artifact, 0ull, bytes, sizeof(bytes), err);
        if (rc != YVEX_OK) return rc;
        if (decode_u32le(bytes) != YVEX_GGUF_MAGIC) {
            yvex_error_clear(err);
            return YVEX_OK;
        }
    }
    rc = yvex_gguf_read_header(artifact, &out->header, err);
    if (rc != YVEX_OK) return rc;
    out->is_gguf = 1;
    return YVEX_OK;
}

/*
 * Contract: parses one immutable structural view; artifact is borrowed only
 * during this call and all accessor storage remains valid until close.
 */
int yvex_gguf_open_ex(yvex_gguf **out,
                      const yvex_artifact *artifact,
                      const yvex_gguf_reader_options *options,
                      yvex_gguf_parse_result *result,
                      yvex_error *err)
{
    yvex_gguf_reader_options defaults;
    yvex_gguf_reader_options effective;
    yvex_gguf_parse_result local_result;
    yvex_file_cursor cur;
    yvex_gguf *gguf;
    int rc;

    if (!result) result = &local_result;
    yvex_gguf_parse_result_reset(result);
    if (!out) {
        return yvex_gguf_reader_fail(result, YVEX_GGUF_PARSE_INVALID_ARGUMENT,
                                     YVEX_GGUF_PARSE_SECTION_NONE, 0ull, ULLONG_MAX,
                                     err, "yvex_gguf_open_ex", "GGUF output pointer is required");
    }
    *out = NULL;
    if (!artifact) {
        return yvex_gguf_reader_fail(result, YVEX_GGUF_PARSE_INVALID_ARGUMENT,
                                     YVEX_GGUF_PARSE_SECTION_FILE, 0ull, ULLONG_MAX,
                                     err, "yvex_gguf_open_ex", "artifact handle is required");
    }
    yvex_gguf_reader_options_default(&defaults);
    effective = options ? *options : defaults;
    if (effective.max_metadata_entries == 0ull ||
        effective.max_tensor_entries == 0ull ||
        effective.max_array_entries == 0ull ||
        effective.max_total_array_entries == 0ull ||
        effective.max_string_bytes == 0ull ||
        effective.max_total_string_bytes == 0ull ||
        effective.max_owned_bytes == 0ull ||
        effective.max_structural_bytes == 0ull ||
        effective.max_array_depth == 0u) {
        return yvex_gguf_reader_fail(result, YVEX_GGUF_PARSE_INVALID_ARGUMENT,
                                     YVEX_GGUF_PARSE_SECTION_RESOURCE, 0ull, ULLONG_MAX,
                                     err, "yvex_gguf_open_ex", "GGUF reader limits must be nonzero");
    }

    gguf = (yvex_gguf *)calloc(1, sizeof(*gguf));
    if (!gguf) {
        return yvex_gguf_reader_fail(result, YVEX_GGUF_PARSE_ALLOCATION_FAILURE,
                                     YVEX_GGUF_PARSE_SECTION_RESOURCE, 0ull, ULLONG_MAX,
                                     err, "yvex_gguf_open_ex", "failed to allocate GGUF view");
    }
    gguf->alignment = YVEX_GGUF_DEFAULT_ALIGNMENT;
    gguf->stats.file_size = yvex_artifact_size(artifact);
    gguf->stats.owned_bytes = (unsigned long long)sizeof(*gguf);
    if (gguf->stats.owned_bytes > effective.max_owned_bytes) {
        yvex_gguf_close(gguf);
        return yvex_gguf_reader_fail(result, YVEX_GGUF_PARSE_RESOURCE_LIMIT,
                                     YVEX_GGUF_PARSE_SECTION_RESOURCE, 0ull, ULLONG_MAX,
                                     err, "yvex_gguf_open_ex", "GGUF view exceeds owned-memory budget");
    }

    memset(&cur, 0, sizeof(cur));
    cur.artifact = artifact;
    cur.size = gguf->stats.file_size;
    cur.options = &effective;
    cur.stats = &gguf->stats;
    cur.result = result;
    cur.err = err;
    cur.record_index = ULLONG_MAX;

    rc = parse_header(&cur, &gguf->header);
    if (rc == YVEX_OK) rc = parse_metadata(gguf, &cur);
    if (rc == YVEX_OK) rc = derive_alignment(gguf, &cur);
    if (rc == YVEX_OK) rc = parse_tensors(gguf, &cur);
    if (rc != YVEX_OK) {
        yvex_gguf_close(gguf);
        return rc;
    }
    result->code = YVEX_GGUF_PARSE_OK;
    result->section = YVEX_GGUF_PARSE_SECTION_NONE;
    result->byte_offset = cur.offset;
    result->record_index = ULLONG_MAX;
    result->reason = "GGUF structural reader accepted input";
    gguf->stats.payload_bytes_read = 0ull;
    *out = gguf;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: preserves the legacy entry point as default-budget structural parsing. */
int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err)
{
    yvex_gguf_parse_result result;
    return yvex_gguf_open_ex(out, artifact, NULL, &result, err);
}

void yvex_gguf_close(yvex_gguf *gguf)
{
    if (!gguf) return;
    gguf_clear(gguf);
    free(gguf);
}

const yvex_gguf_header *yvex_gguf_header_view(const yvex_gguf *gguf)
{
    return gguf ? &gguf->header : NULL;
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
    case YVEX_GGUF_VALUE_INVALID: return "invalid";
    }
    return "invalid";
}

unsigned long long yvex_gguf_metadata_count(const yvex_gguf *gguf)
{
    return gguf ? gguf->header.metadata_count : 0ull;
}

const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, unsigned long long index)
{
    return gguf && index < gguf->header.metadata_count ? gguf->metadata[index].key : NULL;
}

unsigned long long yvex_gguf_metadata_key_len(const yvex_gguf *gguf,
                                               unsigned long long index)
{
    return gguf && index < gguf->header.metadata_count ? gguf->metadata[index].key_len : 0ull;
}

const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf,
                                                 unsigned long long index)
{
    return gguf && index < gguf->header.metadata_count ? &gguf->metadata[index].value : NULL;
}

const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key)
{
    unsigned long long len;
    size_t slot;
    if (!gguf || !key || !gguf->metadata_index.capacity) return NULL;
    len = (unsigned long long)strlen(key);
    slot = name_index_start(&gguf->metadata_index, key, len);
    while (gguf->metadata_index.slots[slot] != 0ull) {
        unsigned long long index = gguf->metadata_index.slots[slot] - 1ull;
        const yvex_gguf_metadata_entry *entry = &gguf->metadata[index];
        if (entry->key_len == len && memcmp(entry->key, key, (size_t)len) == 0) {
            return &entry->value;
        }
        slot = (slot + 1u) & (gguf->metadata_index.capacity - 1u);
    }
    return NULL;
}

yvex_gguf_value_type yvex_gguf_value_type_of(const yvex_gguf_value *value)
{
    return value ? value->type : YVEX_GGUF_VALUE_INVALID;
}

int yvex_gguf_value_as_u64(const yvex_gguf_value *value, unsigned long long *out)
{
    if (!value || !out) return YVEX_ERR_INVALID_ARG;
    if (value->type == YVEX_GGUF_VALUE_UINT8 || value->type == YVEX_GGUF_VALUE_UINT16 ||
        value->type == YVEX_GGUF_VALUE_UINT32 || value->type == YVEX_GGUF_VALUE_UINT64) {
        *out = value->as.u64;
        return YVEX_OK;
    }
    return YVEX_ERR_INVALID_ARG;
}

int yvex_gguf_value_as_i64(const yvex_gguf_value *value, long long *out)
{
    if (!value || !out) return YVEX_ERR_INVALID_ARG;
    if (value->type == YVEX_GGUF_VALUE_INT8 || value->type == YVEX_GGUF_VALUE_INT16 ||
        value->type == YVEX_GGUF_VALUE_INT32 || value->type == YVEX_GGUF_VALUE_INT64) {
        *out = value->as.i64;
        return YVEX_OK;
    }
    return YVEX_ERR_INVALID_ARG;
}

int yvex_gguf_value_as_f64(const yvex_gguf_value *value, double *out)
{
    if (!value || !out) return YVEX_ERR_INVALID_ARG;
    if (value->type == YVEX_GGUF_VALUE_FLOAT32 || value->type == YVEX_GGUF_VALUE_FLOAT64) {
        *out = value->as.f64;
        return YVEX_OK;
    }
    return YVEX_ERR_INVALID_ARG;
}

int yvex_gguf_value_as_bool(const yvex_gguf_value *value, int *out)
{
    if (!value || !out || value->type != YVEX_GGUF_VALUE_BOOL) return YVEX_ERR_INVALID_ARG;
    *out = value->as.bool_value;
    return YVEX_OK;
}

int yvex_gguf_value_as_string(const yvex_gguf_value *value,
                              const char **data,
                              unsigned long long *len)
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
    if (!value || !out || value->type != YVEX_GGUF_VALUE_ARRAY) return YVEX_ERR_INVALID_ARG;
    *out = value->as.array.info;
    return YVEX_OK;
}

const yvex_gguf_value *yvex_gguf_value_array_at(const yvex_gguf_value *value,
                                                 unsigned long long index)
{
    if (!value || value->type != YVEX_GGUF_VALUE_ARRAY ||
        index >= value->as.array.info.count) return NULL;
    return &value->as.array.items[index];
}

unsigned long long yvex_gguf_tensor_count(const yvex_gguf *gguf)
{
    return gguf ? gguf->header.tensor_count : 0ull;
}

const yvex_gguf_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf,
                                                  unsigned long long index)
{
    return gguf && index < gguf->header.tensor_count ? &gguf->tensors[index] : NULL;
}

const yvex_gguf_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf,
                                                    const char *name)
{
    unsigned long long len;
    size_t slot;
    if (!gguf || !name || !gguf->tensor_index.capacity) return NULL;
    len = (unsigned long long)strlen(name);
    slot = name_index_start(&gguf->tensor_index, name, len);
    while (gguf->tensor_index.slots[slot] != 0ull) {
        unsigned long long index = gguf->tensor_index.slots[slot] - 1ull;
        const yvex_gguf_tensor_info *tensor = &gguf->tensors[index];
        if (tensor->name_len == len && memcmp(tensor->name, name, (size_t)len) == 0) {
            return tensor;
        }
        slot = (slot + 1u) & (gguf->tensor_index.capacity - 1u);
    }
    return NULL;
}

unsigned long long yvex_gguf_tensor_data_offset(const yvex_gguf *gguf)
{
    return gguf ? gguf->tensor_data_offset : 0ull;
}

unsigned int yvex_gguf_alignment(const yvex_gguf *gguf)
{
    return gguf ? gguf->alignment : 0u;
}

unsigned long long yvex_gguf_file_size(const yvex_gguf *gguf)
{
    return gguf ? gguf->stats.file_size : 0ull;
}

const yvex_gguf_reader_stats *yvex_gguf_reader_stats_view(const yvex_gguf *gguf)
{
    return gguf ? &gguf->stats : NULL;
}
