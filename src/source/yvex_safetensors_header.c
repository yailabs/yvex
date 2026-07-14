/*
 * yvex_safetensors_header.c - safetensors header parser.
 *
 * Owner: src/source.
 * Owns: safetensors header-length reading and bounded JSON metadata parsing.
 * Does not own: tensor payload loading, CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: validates declared tensor metadata against payload size without reading payload bytes.
 * Boundary: header parsing is not source verification, role mapping, or runtime readiness.
 */
#define _GNU_SOURCE
#include "yvex_safetensors_header.h"
#include "yvex_source_private.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

static unsigned long long st_le64(const unsigned char b[8])
{
    return ((unsigned long long)b[0]) |
           ((unsigned long long)b[1] << 8) |
           ((unsigned long long)b[2] << 16) |
           ((unsigned long long)b[3] << 24) |
           ((unsigned long long)b[4] << 32) |
           ((unsigned long long)b[5] << 40) |
           ((unsigned long long)b[6] << 48) |
           ((unsigned long long)b[7] << 56);
}

/* Reads exactly one bounded header and returns immutable file geometry facts. */
int yvex_safetensors_read_header_file_with_facts(
    const char *abs_path,
    const char *shard_path,
    yvex_native_weight_table *table,
    yvex_safetensors_file_facts *facts,
    yvex_error *err)
{
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
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "safetensors_header", "path, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fd = open(abs_path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0 || fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) ||
        st.st_size < 8) {
        if (fd >= 0) close(fd);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "short safetensors file: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    memset(facts, 0, sizeof(*facts));
    file_size = (unsigned long long)st.st_size;
    fp = fdopen(fd, "rb");
    if (!fp) {
        close(fd);
        yvex_error_setf(err, YVEX_ERR_IO, "safetensors_header", "cannot open safetensors file: %s", shard_path);
        return YVEX_ERR_IO;
    }
    if (fread(len_bytes, 1, 8, fp) != 8) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    header_len = st_le64(len_bytes);
    if (header_len == 0 || header_len > file_size - 8 || header_len > 64ull * 1024ull * 1024ull) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "invalid safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "safetensors_header", "header allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(json, 1, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    fclose(fp);
    json[header_len] = '\0';
    table->header_read_count++;
    table->header_bytes += header_len;
    payload_bytes = file_size - 8 - header_len;
    rc = yvex_safetensors_parse_header(json, payload_bytes, shard_path, table, err);
    if (rc != YVEX_OK) {
        table->summary.malformed_shard_count++;
        table->header_error_count++;
    }
    if (rc == YVEX_OK) {
        facts->file_bytes = file_size;
        facts->header_json_bytes = header_len;
        facts->data_region_offset = 8u + header_len;
        facts->payload_bytes = payload_bytes;
    }
    free(json);
    return rc;
}

/* Preserves the legacy header-only ABI while discarding returned geometry. */
int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err)
{
    yvex_safetensors_file_facts facts;

    return yvex_safetensors_read_header_file_with_facts(
        abs_path, shard_path, table, &facts, err);
}



typedef struct {
    const char *p;
    const char *end;
    const char *shard;
    yvex_error *err;
} st_json;

static void sj_skip_ws(st_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) {
        j->p++;
    }
}

static int sj_fail(st_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "safetensors_json", "%s in %s", msg, j->shard);
    return YVEX_ERR_FORMAT;
}

static int sj_expect(st_json *j, char ch)
{
    sj_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) {
        return sj_fail(j, "unexpected JSON token");
    }
    j->p++;
    return YVEX_OK;
}

static char *sj_string(st_json *j)
{
    const char *start;
    char *out;
    size_t cap;
    size_t n = 0;

    sj_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        sj_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    start = j->p;
    cap = (size_t)(j->end - start) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "safetensors_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                sj_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') {
                out[n++] = ch;
            } else if (ch == 'n') {
                out[n++] = '\n';
            } else if (ch == 'r') {
                out[n++] = '\r';
            } else if (ch == 't') {
                out[n++] = '\t';
            } else {
                free(out);
                sj_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    sj_fail(j, "unterminated string");
    return NULL;
}

static int sj_u64(st_json *j, unsigned long long *out)
{
    unsigned long long v = 0;

    sj_skip_ws(j);
    if (j->p >= j->end || !isdigit((unsigned char)*j->p)) {
        return sj_fail(j, "expected unsigned integer");
    }
    while (j->p < j->end && isdigit((unsigned char)*j->p)) {
        unsigned int d = (unsigned int)(*j->p - '0');
        if (v > (18446744073709551615ull - d) / 10ull) {
            return sj_fail(j, "integer overflow");
        }
        v = v * 10ull + d;
        j->p++;
    }
    *out = v;
    return YVEX_OK;
}

static int sj_skip_value(st_json *j);

static int sj_skip_array(st_json *j)
{
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = sj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed array");
    }
    return sj_fail(j, "unterminated array");
}

static int sj_skip_object(st_json *j)
{
    int rc = sj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = sj_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = sj_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = sj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed object");
    }
    return sj_fail(j, "unterminated object");
}

static int sj_skip_literal(st_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) {
        return sj_fail(j, "unexpected literal");
    }
    j->p += n;
    return YVEX_OK;
}

static int sj_skip_value(st_json *j)
{
    char *s;

    sj_skip_ws(j);
    if (j->p >= j->end) return sj_fail(j, "expected value");
    if (*j->p == '{') return sj_skip_object(j);
    if (*j->p == '[') return sj_skip_array(j);
    if (*j->p == '"') {
        s = sj_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (isdigit((unsigned char)*j->p)) {
        unsigned long long ignored;
        return sj_u64(j, &ignored);
    }
    if (*j->p == 't') return sj_skip_literal(j, "true");
    if (*j->p == 'f') return sj_skip_literal(j, "false");
    if (*j->p == 'n') return sj_skip_literal(j, "null");
    return sj_fail(j, "unsupported JSON value");
}

static int sj_parse_shape(st_json *j, unsigned long long *dims, unsigned int *rank)
{
    unsigned int n = 0;
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        *rank = 0;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        if (n >= YVEX_NATIVE_WEIGHT_MAX_DIMS) {
            return sj_fail(j, "rank exceeds native weight limit");
        }
        rc = sj_u64(j, &dims[n++]);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            *rank = n;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed shape");
    }
    return sj_fail(j, "unterminated shape");
}

static int sj_parse_offsets(st_json *j, unsigned long long *start, unsigned long long *end)
{
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    rc = sj_u64(j, start);
    if (rc != YVEX_OK) return rc;
    rc = sj_expect(j, ',');
    if (rc != YVEX_OK) return rc;
    rc = sj_u64(j, end);
    if (rc != YVEX_OK) return rc;
    rc = sj_expect(j, ']');
    if (rc != YVEX_OK) return rc;
    if (*end < *start) {
        return sj_fail(j, "invalid data_offsets order");
    }
    return YVEX_OK;
}

static int sj_parse_tensor(st_json *j, const char *name, unsigned long long payload_bytes,
                           const char *shard_path, yvex_native_weight_table *table)
{
    char *dtype = NULL;
    unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS];
    unsigned int rank = 0;
    unsigned long long start = 0;
    unsigned long long end = 0;
    int have_dtype = 0;
    int have_shape = 0;
    int have_offsets = 0;
    int rc = sj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    memset(dims, 0, sizeof(dims));
    sj_skip_ws(j);
    while (j->p < j->end && *j->p != '}') {
        char *key = sj_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = sj_expect(j, ':');
        if (rc == YVEX_OK && strcmp(key, "dtype") == 0) {
            free(dtype);
            dtype = sj_string(j);
            if (!dtype) rc = yvex_error_code(j->err);
            have_dtype = rc == YVEX_OK;
        } else if (rc == YVEX_OK && strcmp(key, "shape") == 0) {
            rc = sj_parse_shape(j, dims, &rank);
            have_shape = rc == YVEX_OK;
        } else if (rc == YVEX_OK && strcmp(key, "data_offsets") == 0) {
            rc = sj_parse_offsets(j, &start, &end);
            have_offsets = rc == YVEX_OK;
        } else if (rc == YVEX_OK) {
            rc = sj_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) {
            free(dtype);
            return rc;
        }
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            sj_skip_ws(j);
            continue;
        }
        break;
    }
    rc = sj_expect(j, '}');
    if (rc != YVEX_OK) {
        free(dtype);
        return rc;
    }
    if (!have_dtype || !have_shape || !have_offsets) {
        free(dtype);
        return sj_fail(j, "tensor entry missing dtype, shape, or data_offsets");
    }
    if (end > payload_bytes) {
        free(dtype);
        return sj_fail(j, "data_offsets exceed payload size");
    }
    rc = yvex_native_weight_table_add(table, name, shard_path, dtype, rank, dims, start, end, j->err);
    free(dtype);
    return rc;
}

/*
 * yvex_safetensors_parse_header()
 *
 * Purpose:
 *   parse safetensors header JSON into native tensor metadata records.
 *
 * Inputs:
 *   json, payload byte count, shard path, and output table are borrowed.
 *
 * Effects:
 *   parses dtype, shape, and data offset metadata, validates declared byte
 *   ranges against payload size, and appends table rows; it never reads tensor
 *   payload bytes.
 *
 * Failure:
 *   returns invalid-arg, format, bounds, or allocation errors through the JSON
 *   parser and native table append path.
 *
 * Boundary:
 *   safetensors header parsing is not source verification, tensor payload
 *   loading, role mapping, artifact emission, runtime support, or generation.
 */
int yvex_safetensors_parse_header(const char *json,
                                  unsigned long long payload_bytes,
                                  const char *shard_path,
                                  yvex_native_weight_table *table,
                                  yvex_error *err)
{
    st_json j;
    int rc;

    if (!json || !shard_path || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "safetensors_json", "json, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    j.p = json;
    j.end = json + strlen(json);
    j.shard = shard_path;
    j.err = err;
    rc = sj_expect(&j, '{');
    if (rc != YVEX_OK) return rc;
    sj_skip_ws(&j);
    if (j.p < j.end && *j.p == '}') {
        return sj_fail(&j, "empty safetensors header");
    }
    while (j.p < j.end) {
        char *key = sj_string(&j);
        if (!key) return yvex_error_code(err);
        rc = sj_expect(&j, ':');
        if (rc == YVEX_OK && strcmp(key, "__metadata__") == 0) {
            rc = sj_skip_value(&j);
        } else if (rc == YVEX_OK) {
            rc = sj_parse_tensor(&j, key, payload_bytes, shard_path, table);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') {
            j.p++;
            continue;
        }
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            sj_skip_ws(&j);
            if (j.p != j.end) {
                return sj_fail(&j, "trailing JSON bytes");
            }
            return YVEX_OK;
        }
        return sj_fail(&j, "malformed safetensors header");
    }
    return sj_fail(&j, "unterminated safetensors header");
}
