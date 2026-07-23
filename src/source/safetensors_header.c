/* Owner: source safetensors header parsing.
 * Owns: bounded header-length and tensor metadata parsing.
 * Does not own: payload loading, source admission, role mapping, or runtime.
 * Invariants: declared shapes and offsets are checked against file payload size.
 * Boundary: header parsing does not prove source completeness.
 * Purpose: parse safetensors headers into checked tensor metadata.
 * Inputs: one shard or in-memory header, payload size, and caller rows.
 * Effects: reads the fixed prefix and JSON header only.
 * Failure: truncation, JSON, shape, offset, overflow, or allocation refuses. */
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yvex/internal/core.h>
#include <yvex/internal/source_payload.h>

static int safetensors_parse_header(const char *json,
                                    unsigned long long payload_bytes,
                                    const char *shard_path,
                                    yvex_native_weight_table *table,
                                    yvex_error *err);

/* Purpose: publish one typed header refusal without duplicating error-state transitions. */
static int header_refuse(yvex_error *err,
                         yvex_status status,
                         const char *where,
                         const char *message) {
    yvex_error_set(err, status, where, message);
    return status;
}

/* Purpose: project le64 facts while preserving the canonical safetensors metadata invariants. */
static unsigned long long st_le64(const unsigned char b[8]) {
    return ((unsigned long long)b[0]) | ((unsigned long long)b[1] << 8) |
           ((unsigned long long)b[2] << 16) | ((unsigned long long)b[3] << 24) |
           ((unsigned long long)b[4] << 32) | ((unsigned long long)b[5] << 40) |
           ((unsigned long long)b[6] << 48) | ((unsigned long long)b[7] << 56);
}

/* Purpose: reads exactly one bounded header and returns immutable file geometry facts.
 * Inputs: typed safetensors header parsing arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned safetensors header parsing state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header parsing does not prove source completeness. */
int yvex_safetensors_read_header_file_with_facts(const char *abs_path,
                                                 const char *shard_path,
                                                 yvex_native_weight_table *table,
                                                 yvex_safetensors_file_facts *facts,
                                                 yvex_error *err) {
    FILE *fp;
    int fd;
    struct stat st;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    unsigned long long file_size;
    unsigned long long payload_bytes;
    char *json;
    int rc;

    if (!abs_path || !shard_path || !table || !facts) {
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "safetensors_header", "path, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fd = open(abs_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 8) {
        if (fd >= 0)
            close(fd);
        yvex_error_setf(
            err, YVEX_ERR_FORMAT, "safetensors_header", "short safetensors file: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    memset(facts, 0, sizeof(*facts));
    file_size = (unsigned long long)st.st_size;
    fp = fdopen(fd, "rb");
    if (!fp) {
        close(fd);
        yvex_error_setf(
            err, YVEX_ERR_IO, "safetensors_header", "cannot open safetensors file: %s", shard_path);
        return YVEX_ERR_IO;
    }
    if (fread(len_bytes, 1, 8, fp) != 8) {
        fclose(fp);
        yvex_error_setf(err,
                        YVEX_ERR_FORMAT,
                        "safetensors_header",
                        "cannot read safetensors header length: %s",
                        shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    header_len = st_le64(len_bytes);
    if (header_len == 0 || header_len > file_size - 8 || header_len > 64ull * 1024ull * 1024ull) {
        fclose(fp);
        yvex_error_setf(err,
                        YVEX_ERR_FORMAT,
                        "safetensors_header",
                        "invalid safetensors header length: %s",
                        shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        return header_refuse(err, YVEX_ERR_NOMEM, "safetensors_header", "header allocation failed");
    }
    if (fread(json, 1, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        yvex_error_setf(err,
                        YVEX_ERR_FORMAT,
                        "safetensors_header",
                        "cannot read safetensors header: %s",
                        shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    fclose(fp);
    json[header_len] = '\0';
    table->header_read_count++;
    table->header_bytes += header_len;
    payload_bytes = file_size - 8 - header_len;
    rc = safetensors_parse_header(json, payload_bytes, shard_path, table, err);
    if (rc != YVEX_OK) {
        table->summary.malformed_shard_count++;
        table->header_error_count++;
    }
    if (rc == YVEX_OK) {
        facts->file_bytes = file_size;
        facts->header_json_bytes = header_len;
        facts->data_region_offset = 8u + header_len;
        facts->payload_bytes = payload_bytes;
        yvex_core_execution_observation_record(
            YVEX_CORE_OBSERVE_SOURCE_HEADER, 1ull);
    }
    free(json);
    return rc;
}

/* Purpose: preserves the legacy header-only ABI while discarding returned geometry.
 * Inputs: typed safetensors header parsing arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned safetensors header parsing state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header parsing does not prove source completeness. */
int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err) {
    yvex_safetensors_file_facts facts;

    return yvex_safetensors_read_header_file_with_facts(abs_path, shard_path, table, &facts, err);
}

/* Purpose: publish a format refusal that preserves the offending shard identity. */
static int safetensors_json_refuse(yvex_error *err, const char *shard, const char *message) {
    yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_json", "%s in %s", message, shard);
    return YVEX_ERR_FORMAT;
}

/* Purpose: consume one exact structural delimiter after insignificant whitespace. */
static int safetensors_json_expect(yvex_json *json, char token) {
    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor != token)
        return 0;
    json->cursor++;
    return 1;
}

/* Purpose: parse a possibly empty tensor shape without exceeding the native rank limit.
 * Inputs: mutable bounded JSON cursor, fixed dimension storage, and rank output.
 * Effects: advances the cursor and replaces rank only after a complete array.
 * Failure: malformed, oversized, or truncated arrays return false.
 * Boundary: syntax parsing does not admit tensor geometry. */
static int safetensors_shape(yvex_json *json,
                             unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS],
                             unsigned int *rank) {
    unsigned long long count = 0u;

    if (!safetensors_json_expect(json, '['))
        return 0;
    yvex_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') {
        json->cursor++;
        *rank = 0u;
        return 1;
    }
    while (count < YVEX_NATIVE_WEIGHT_MAX_DIMS && yvex_json_u64(json, &dims[count])) {
        count++;
        yvex_json_space(json);
        if (json->cursor >= json->end)
            return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            *rank = (unsigned int)count;
            return 1;
        }
        if (*json->cursor++ != ',')
            return 0;
    }
    return 0;
}

/* Purpose: parse exactly two ordered tensor data offsets. */
static int safetensors_offsets(yvex_json *json,
                               unsigned long long *start,
                               unsigned long long *end) {
    return safetensors_json_expect(json, '[') && yvex_json_u64(json, start) &&
           safetensors_json_expect(json, ',') && yvex_json_u64(json, end) &&
           safetensors_json_expect(json, ']') && *start <= *end;
}

/* Purpose: parse one tensor object and append it only after all required facts are valid.
 * Inputs: bounded JSON cursor, tensor and shard identities, payload size, and table.
 * Effects: advances the cursor and appends one fully validated tensor row.
 * Failure: malformed fields, invalid geometry, or allocation returns typed refusal.
 * Boundary: parsed metadata does not read or trust payload bytes. */
static int safetensors_tensor(yvex_json *json,
                              const char *name,
                              unsigned long long payload_bytes,
                              const char *shard_path,
                              yvex_native_weight_table *table,
                              yvex_error *err) {
    char *dtype = NULL;
    char key[YVEX_JSON_KEY_CAP];
    unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS] = {0};
    unsigned long long start = 0u, end = 0u;
    unsigned int rank = 0u, seen = 0u;
    yvex_json_iter iter;
    yvex_json_item item = YVEX_JSON_ITEM_ERROR;
    int valid = yvex_json_iter_begin(json, &iter, YVEX_JSON_COLLECTION_OBJECT);

    while (valid && (item = yvex_json_object_member(&iter, key, sizeof(key))) ==
                        YVEX_JSON_ITEM_READY) {
        if (strcmp(key, "dtype") == 0) {
            free(dtype);
            dtype = yvex_json_string_dup(json, 64u);
            valid = dtype != NULL;
            if (valid) seen |= 1u;
        } else if (strcmp(key, "shape") == 0) {
            valid = safetensors_shape(json, dims, &rank);
            if (valid) seen |= 2u;
        } else if (strcmp(key, "data_offsets") == 0) {
            valid = safetensors_offsets(json, &start, &end);
            if (valid) seen |= 4u;
        } else {
            valid = yvex_json_skip_value(json);
        }
    }
    if (item != YVEX_JSON_ITEM_END) valid = 0;
    if (!valid || seen != 7u || end > payload_bytes) {
        free(dtype);
        return safetensors_json_refuse(err,
                                       shard_path,
                                       seen != 7u ? "tensor entry missing dtype, shape, or data_offsets"
                                                  : "invalid tensor shape or data offsets");
    }
    valid = yvex_native_weight_table_add(
        table, name, shard_path, dtype, rank, dims, start, end, err);
    free(dtype);
    return valid;
}

/* Purpose: parse safetensors header JSON into native tensor metadata records.
 * Inputs: typed safetensors header parsing arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned safetensors header parsing state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header parsing does not prove source completeness. */
static int safetensors_parse_header(const char *json,
                                    unsigned long long payload_bytes,
                                    const char *shard_path,
                                    yvex_native_weight_table *table,
                                    yvex_error *err) {
    yvex_json parser;
    yvex_json_iter iter;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP];

    if (!json || !shard_path || !table) {
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "safetensors_json", "json, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_json_init(&parser, json, strlen(json));
    if (!yvex_json_iter_begin(&parser, &iter, YVEX_JSON_COLLECTION_OBJECT))
        return safetensors_json_refuse(err, shard_path, "unexpected JSON token");
    while ((item = yvex_json_object_member(&iter, key, sizeof(key))) ==
           YVEX_JSON_ITEM_READY) {
        int rc;

        if (strcmp(key, "__metadata__") == 0) {
            rc = yvex_json_skip_value(&parser)
                     ? YVEX_OK
                     : safetensors_json_refuse(err, shard_path, "malformed metadata value");
        } else {
            rc = safetensors_tensor(&parser, key, payload_bytes, shard_path, table, err);
        }
        if (rc != YVEX_OK)
            return rc;
    }
    if (item != YVEX_JSON_ITEM_END || iter.trailing_separator)
        return safetensors_json_refuse(err, shard_path, "malformed safetensors header");
    if (iter.count == 0u)
        return safetensors_json_refuse(err, shard_path, "empty safetensors header");
    return yvex_json_complete(&parser)
               ? YVEX_OK
               : safetensors_json_refuse(err, shard_path, "trailing JSON bytes");
}
