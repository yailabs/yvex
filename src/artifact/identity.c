/* Owner: artifact identity.
 * Owns: SHA-256 over exact artifact bytes and bounded streaming state.
 * Does not own: GGUF semantics, provenance, completeness, or materialization.
 * Invariants: the identity binds every byte and exact length; partial reads never publish.
 * Boundary: physical identity does not prove semantic completeness or support.
 * Purpose: compute canonical identities over exact artifact byte sequences.
 * Inputs: bounded bytes or read callbacks and caller-owned digest storage.
 * Effects: updates hash state and reads only explicitly requested artifact spans.
 * Failure: short read, drift, malformed digest, or I/O publishes no identity. */

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <yvex/artifact.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/core.h>

/* Purpose: compare every stable artifact snapshot fact without filesystem I/O.
 * Inputs: two immutable captured snapshots.
 * Effects: none.
 * Failure: absent input or any identity difference returns false.
 * Boundary: snapshot equality does not validate GGUF semantics or file bytes. */
int yvex_artifact_snapshot_equal(const yvex_artifact_snapshot *left,
                                 const yvex_artifact_snapshot *right)
{
    return left && right && left->device == right->device &&
           left->inode == right->inode && left->size == right->size &&
           left->mtime_seconds == right->mtime_seconds &&
           left->mtime_nanoseconds == right->mtime_nanoseconds &&
           left->ctime_seconds == right->ctime_seconds &&
           left->ctime_nanoseconds == right->ctime_nanoseconds;
}

/* Purpose: starts one empty exact-file identity stream without allocation or IO.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
void yvex_artifact_identity_stream_init(yvex_artifact_identity_stream *stream) {
    if (!stream)
        return;
    memset(stream, 0, sizeof(*stream));
    yvex_sha256_init(&stream->hash);
    stream->active = 1;
}

/* Purpose: appends one ordered byte range and checks aggregate length arithmetic.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_identity_stream_update(yvex_artifact_identity_stream *stream,
                                         const unsigned char *bytes,
                                         size_t byte_count,
                                         yvex_error *err) {
    if (!stream || !stream->active || (!bytes && byte_count)) {
        yvex_error_set(err,
                       YVEX_ERR_INVALID_ARG,
                       "artifact_identity.stream.update",
                       "active stream and byte range are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (stream->bytes > ULLONG_MAX - (unsigned long long)byte_count ||
        !yvex_sha256_update(&stream->hash, bytes, byte_count)) {
        stream->active = 0;
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "artifact_identity.stream.update",
                       "artifact identity byte count overflowed");
        return YVEX_ERR_BOUNDS;
    }
    stream->bytes += (unsigned long long)byte_count;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: finalizes only exact expected coverage and clears mutable hash state.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_identity_stream_final(yvex_artifact_identity_stream *stream,
                                        unsigned long long expected_bytes,
                                        char out_hex[YVEX_SHA256_HEX_CAP],
                                        yvex_error *err) {
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];

    if (out_hex)
        out_hex[0] = '\0';
    if (!stream || !stream->active || !out_hex || stream->bytes != expected_bytes ||
        !yvex_sha256_final(&stream->hash, digest)) {
        if (stream)
            stream->active = 0;
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "artifact_identity.stream.final",
                       "artifact identity requires exact complete byte coverage");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    memset(stream, 0, sizeof(*stream));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: project sha256 hex bytes facts while preserving the canonical artifact identity invariants.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_sha256_hex_bytes(const unsigned char *data,
                                   unsigned long long len,
                                   char out_hex[YVEX_SHA256_HEX_CAP],
                                   yvex_error *err) {
    yvex_sha256 ctx;
    unsigned char digest[32];

    if (!data || !out_hex) {
        yvex_error_set(err,
                       YVEX_ERR_INVALID_ARG,
                       "yvex_artifact_sha256_hex_bytes",
                       "data and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_sha256_init(&ctx);
    if (len > (unsigned long long)SIZE_MAX || !yvex_sha256_update(&ctx, data, (size_t)len) ||
        !yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "yvex_artifact_sha256_hex_bytes",
                       "input length exceeds SHA-256 addressable bounds");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: hash the complete artifact byte sequence while detecting short reads and replacement.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_compute_sha256(const char *path,
                                 char out_hex[YVEX_SHA256_HEX_CAP],
                                 yvex_error *err) {
    FILE *fp;
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];

    if (!path || !path[0] || !out_hex) {
        yvex_error_set(err,
                       YVEX_ERR_INVALID_ARG,
                       "yvex_artifact_compute_sha256",
                       "path and out_hex are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err,
                        YVEX_ERR_IO,
                        "yvex_artifact_compute_sha256",
                        "cannot open artifact for sha256: %s",
                        path);
        return YVEX_ERR_IO;
    }

    yvex_sha256_init(&ctx);
    for (;;) {
        size_t n = fread(buf, 1u, sizeof(buf), fp);
        if (n > 0u) {
            if (!yvex_sha256_update(&ctx, buf, n)) {
                fclose(fp);
                yvex_error_set(err,
                               YVEX_ERR_BOUNDS,
                               "yvex_artifact_compute_sha256",
                               "artifact SHA-256 length overflow");
                return YVEX_ERR_BOUNDS;
            }
        }
        if (n < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err,
                                YVEX_ERR_IO,
                                "yvex_artifact_compute_sha256",
                                "cannot read artifact for sha256: %s",
                                path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    if (!yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "yvex_artifact_compute_sha256",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out_hex);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: hash one exact artifact span through a caller-supplied bounded reader.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned artifact identity state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_identity_read(const char *path,
                                yvex_artifact_file_identity *out,
                                yvex_error *err) {
    FILE *fp;
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];
    unsigned long long size = 0ull;
    int n;

    if (!path || !path[0] || !out) {
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "yvex_artifact_identity_read", "path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    n = snprintf(out->path, sizeof(out->path), "%s", path);
    if (n < 0 || (unsigned int)n >= sizeof(out->path)) {
        yvex_error_set(
            err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read", "artifact path too long");
        return YVEX_ERR_BOUNDS;
    }

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err,
                        YVEX_ERR_IO,
                        "yvex_artifact_identity_read",
                        "cannot open artifact for identity: %s",
                        path);
        return YVEX_ERR_IO;
    }

    yvex_sha256_init(&ctx);
    for (;;) {
        size_t got = fread(buf, 1u, sizeof(buf), fp);
        if (got > 0u) {
            if (!yvex_sha256_update(&ctx, buf, got)) {
                fclose(fp);
                yvex_error_set(err,
                               YVEX_ERR_BOUNDS,
                               "yvex_artifact_identity_read",
                               "artifact SHA-256 length overflow");
                return YVEX_ERR_BOUNDS;
            }
            if (ULLONG_MAX - size < (unsigned long long)got) {
                fclose(fp);
                yvex_error_set(err,
                               YVEX_ERR_BOUNDS,
                               "yvex_artifact_identity_read",
                               "artifact file size overflow");
                return YVEX_ERR_BOUNDS;
            }
            size += (unsigned long long)got;
        }
        if (got < sizeof(buf)) {
            if (ferror(fp)) {
                fclose(fp);
                yvex_error_setf(err,
                                YVEX_ERR_IO,
                                "yvex_artifact_identity_read",
                                "cannot read artifact for identity: %s",
                                path);
                return YVEX_ERR_IO;
            }
            break;
        }
    }
    fclose(fp);

    if (!yvex_sha256_final(&ctx, digest)) {
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "yvex_artifact_identity_read",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out->sha256);
    out->file_size = size;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: hash the exact borrowed handle between pre-read and post-read identity checks.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned artifact identity state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_artifact_identity_read_open(const yvex_artifact *artifact,
                                     yvex_artifact_file_identity *out,
                                     yvex_error *err) {
    yvex_sha256 ctx;
    unsigned char digest[32];
    unsigned char buf[65536];
    unsigned long long offset = 0ull;
    unsigned long long size;
    int n;
    int rc;

    if (!artifact || !out) {
        yvex_error_set(err,
                       YVEX_ERR_INVALID_ARG,
                       "yvex_artifact_identity_read_open",
                       "artifact and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    rc = yvex_artifact_snapshot_validate(artifact, NULL, err);
    if (rc != YVEX_OK)
        return rc;

    n = snprintf(out->path, sizeof(out->path), "%s", yvex_artifact_path(artifact));
    if (n < 0 || (unsigned int)n >= sizeof(out->path)) {
        yvex_error_set(
            err, YVEX_ERR_BOUNDS, "yvex_artifact_identity_read_open", "artifact path too long");
        return YVEX_ERR_BOUNDS;
    }
    size = yvex_artifact_size(artifact);
    yvex_sha256_init(&ctx);
    while (offset < size) {
        unsigned long long remaining = size - offset;
        size_t take = remaining > (unsigned long long)sizeof(buf) ? sizeof(buf) : (size_t)remaining;
        rc = yvex_artifact_read_at(artifact, offset, buf, take, err);
        if (rc != YVEX_OK) {
            memset(out, 0, sizeof(*out));
            return rc;
        }
        if (!yvex_sha256_update(&ctx, buf, take)) {
            memset(out, 0, sizeof(*out));
            yvex_error_set(err,
                           YVEX_ERR_BOUNDS,
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
        yvex_error_set(err,
                       YVEX_ERR_BOUNDS,
                       "yvex_artifact_identity_read_open",
                       "artifact SHA-256 finalization failed");
        return YVEX_ERR_BOUNDS;
    }
    yvex_sha256_hex(digest, out->sha256);
    out->file_size = size;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: check structural validity of the supplied artifact identity facts.
 * Inputs: typed artifact identity arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned artifact identity state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: physical identity does not prove semantic completeness or support. */
int yvex_sha256_hex_is_valid(const char *hex) {
    return yvex_sha256_hex_valid(hex);
}
