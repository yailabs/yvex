/*
 * identity.c - canonical local artifact identity owner.
 *
 * Owner: TRACK.ARTIFACT artifact-identity cell.
 * Owns: SHA-256 over exact local artifact bytes, including bounded streaming
 *   state used by validators that already perform file-order reads.
 * Does not own: GGUF semantics, payload correctness, provenance, admission,
 *   materialization, runtime, rendering, or supply-chain trust.
 * Invariants: identities bind every byte and exact file length; partial or
 *   drifted reads publish no successful identity.
 * Boundary: artifact identity alone does not prove artifact completeness.
 */

#include <yvex/artifact_identity.h>

#include "identity.h"
#include "src/core/sha256.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Starts one empty exact-file identity stream without allocation or IO. */
void yvex_artifact_identity_stream_init(
    yvex_artifact_identity_stream *stream)
{
    if (!stream) return;
    memset(stream, 0, sizeof(*stream));
    yvex_sha256_init(&stream->hash);
    stream->active = 1;
}

/* Appends one ordered byte range and checks aggregate length arithmetic. */
int yvex_artifact_identity_stream_update(
    yvex_artifact_identity_stream *stream,
    const unsigned char *bytes,
    size_t byte_count,
    yvex_error *err)
{
    if (!stream || !stream->active || (!bytes && byte_count)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG,
                       "artifact_identity.stream.update",
                       "active stream and byte range are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (stream->bytes > ULLONG_MAX - (unsigned long long)byte_count ||
        !yvex_sha256_update(&stream->hash, bytes, byte_count)) {
        stream->active = 0;
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "artifact_identity.stream.update",
                       "artifact identity byte count overflowed");
        return YVEX_ERR_BOUNDS;
    }
    stream->bytes += (unsigned long long)byte_count;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Finalizes only exact expected coverage and clears mutable hash state. */
int yvex_artifact_identity_stream_final(
    yvex_artifact_identity_stream *stream,
    unsigned long long expected_bytes,
    char out_hex[YVEX_SHA256_HEX_CAP],
    yvex_error *err)
{
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];

    if (out_hex) out_hex[0] = '\0';
    if (!stream || !stream->active || !out_hex ||
        stream->bytes != expected_bytes ||
        !yvex_sha256_final(&stream->hash, digest)) {
        if (stream) stream->active = 0;
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "artifact_identity.stream.final",
                       "artifact identity requires exact complete byte coverage");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    memset(stream, 0, sizeof(*stream));
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_sha256_hex_bytes(const unsigned char *data,
                                   unsigned long long len,
                                   char out_hex[YVEX_SHA256_HEX_CAP],
                                   yvex_error *err)
{
    yvex_sha256 ctx;
    unsigned char digest[32];

    if (!data || !out_hex) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_sha256_hex_bytes",
                       "data and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_sha256_init(&ctx);
    if (len > (unsigned long long)SIZE_MAX ||
        !yvex_sha256_update(&ctx, data, (size_t)len) ||
        !yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_artifact_sha256_hex_bytes",
                       "input length exceeds SHA-256 addressable bounds");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_compute_sha256(const char *path,
                                 char out_hex[YVEX_SHA256_HEX_CAP],
                                 yvex_error *err)
{
    FILE *fp;
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];

    if (!path || !path[0] || !out_hex) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_compute_sha256",
                       "path and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_compute_sha256",
                        "cannot open artifact for sha256: %s", path);
        return YVEX_ERR_IO;
    }

    yvex_sha256_init(&ctx);
    for (;;) {
        size_t n = fread(buf, 1u, sizeof(buf), fp);
        if (n > 0u) {
            if (!yvex_sha256_update(&ctx, buf, n)) {
                fclose(fp);
                yvex_error_set(err, YVEX_ERR_BOUNDS,
                               "yvex_artifact_compute_sha256",
                               "artifact SHA-256 length overflow");
                return YVEX_ERR_BOUNDS;
            }
        }
        if (n < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_compute_sha256",
                                "cannot read artifact for sha256: %s", path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    if (!yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_artifact_compute_sha256",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_artifact_identity_read(const char *path,
                                yvex_artifact_file_identity *out,
                                yvex_error *err)
{
    FILE *fp;
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];
    unsigned long long size = 0ull;
    int n;

    if (!path || !path[0] || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_identity_read",
                       "path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    n = snprintf(out->path, sizeof(out->path), "%s", path);
    if (n < 0 || (unsigned int)n >= sizeof(out->path)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read",
                       "artifact path too long");
        return YVEX_ERR_BOUNDS;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_identity_read",
                        "cannot open artifact for identity: %s", path);
        return YVEX_ERR_IO;
    }

    yvex_sha256_init(&ctx);
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), fp);
        if (got > 0u) {
            if (!yvex_sha256_update(&ctx, buf, got)) {
                fclose(fp);
                yvex_error_set(err, YVEX_ERR_BOUNDS,
                               "yvex_artifact_identity_read",
                               "artifact SHA-256 length overflow");
                return YVEX_ERR_BOUNDS;
            }
            if (ULLONG_MAX - size < (unsigned long long)got) {
                fclose(fp);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read",
                               "artifact file size overflow");
                return YVEX_ERR_BOUNDS;
            }
            size += (unsigned long long)got;
        }
        if (got < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_identity_read",
                                "cannot read artifact for identity: %s", path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    if (!yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out->sha256);
    out->file_size = size;
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Contract: hashes the exact borrowed artifact handle, validates its open-time
 * identity before and after IO, owns no file handle, and leaves output empty on
 * failure.
 */
int yvex_artifact_identity_read_open(const yvex_artifact *artifact,
                                     yvex_artifact_file_identity *out,
                                     yvex_error *err)
{
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];
    unsigned long long offset = 0ull;
    unsigned long long size;
    int n;
    int rc;

    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_identity_read_open",
                       "artifact and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK) return rc;

    n = snprintf(out->path, sizeof(out->path), "%s", yvex_artifact_path(artifact));
    if (n < 0 || (unsigned int)n >= sizeof(out->path)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read_open",
                       "artifact path too long");
        return YVEX_ERR_BOUNDS;
    }
    size = yvex_artifact_size(artifact);
    yvex_sha256_init(&ctx);
    while (offset < size) {
        unsigned long long remaining = size - offset;
        size_t take = remaining > (unsigned long long)sizeof(buf)
                          ? sizeof(buf) : (size_t)remaining;
        rc = yvex_artifact_read_at(artifact, offset, buf, take, err);
        if (rc != YVEX_OK) {
            memset(out, 0, sizeof(*out));
            return rc;
        }
        if (!yvex_sha256_update(&ctx, buf, take)) {
            memset(out, 0, sizeof(*out));
            yvex_error_set(err, YVEX_ERR_BOUNDS,
                           "yvex_artifact_identity_read_open",
                           "artifact SHA-256 length overflow");
            return YVEX_ERR_BOUNDS;
        }
        offset += (unsigned long long)take;
    }
    rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK) {
        memset(out, 0, sizeof(*out));
        return rc;
    }
    if (!yvex_sha256_final(&ctx, digest)) {
        memset(out, 0, sizeof(*out));
        yvex_error_set(err, YVEX_ERR_BOUNDS,
                       "yvex_artifact_identity_read_open",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out->sha256);
    out->file_size = size;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_sha256_hex_is_valid(const char *hex)
{
    return yvex_sha256_hex_valid(hex);
}
