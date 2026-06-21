/*
 * YVEX - GGUF header parser
 *
 * File: src/formats/gguf.c
 * Layer: format implementation
 *
 * Purpose:
 *   Implements C0 GGUF magic probing and fixed header parsing using checked
 *   little-endian cursor reads. Metadata entries and tensor directories are
 *   intentionally not parsed in C0.
 *
 * Implements:
 *   - yvex_gguf_probe_file
 *   - yvex_gguf_read_header
 *
 * Invariants:
 *   - parser never casts mapped bytes to structs
 *   - cursor advances only after successful reads
 *   - unsupported GGUF versions fail explicitly
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_gguf
 */
#include <yvex/gguf.h>

#include <string.h>

#define YVEX_GGUF_HEADER_BYTES 24u
#define YVEX_GGUF_SUPPORTED_VERSION 3u

typedef struct {
    const unsigned char *data;
    unsigned long long size;
    unsigned long long offset;
} yvex_byte_cursor;

static int cursor_require(yvex_byte_cursor *cur, unsigned long long len, yvex_error *err, const char *where)
{
    return yvex_range_check(cur->size, cur->offset, len, err) == YVEX_OK
               ? YVEX_OK
               : (yvex_error_setf(err, YVEX_ERR_FORMAT, where,
                                  "truncated GGUF header at offset %llu", cur->offset),
                  YVEX_ERR_FORMAT);
}

static int read_u32le(yvex_byte_cursor *cur, unsigned int *out, yvex_error *err, const char *where)
{
    const unsigned char *p;
    if (cursor_require(cur, 4, err, where) != YVEX_OK) {
        return YVEX_ERR_FORMAT;
    }
    p = cur->data + cur->offset;
    *out = ((unsigned int)p[0]) |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
    cur->offset += 4;
    return YVEX_OK;
}

static int read_u64le(yvex_byte_cursor *cur, unsigned long long *out, yvex_error *err, const char *where)
{
    const unsigned char *p;
    if (cursor_require(cur, 8, err, where) != YVEX_OK) {
        return YVEX_ERR_FORMAT;
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

int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err)
{
    yvex_byte_cursor cur;
    unsigned int magic;
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

    rc = read_u32le(&cur, &magic, err, "yvex_gguf_read_header");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (magic != YVEX_GGUF_MAGIC) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "yvex_gguf_read_header",
                        "bad GGUF magic 0x%08x", magic);
        return YVEX_ERR_FORMAT;
    }

    rc = read_u32le(&cur, &out->version, err, "yvex_gguf_read_header");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (out->version != YVEX_GGUF_SUPPORTED_VERSION) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_gguf_read_header",
                        "unsupported GGUF version %u", out->version);
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = read_u64le(&cur, &out->tensor_count, err, "yvex_gguf_read_header");
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = read_u64le(&cur, &out->metadata_count, err, "yvex_gguf_read_header");
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
