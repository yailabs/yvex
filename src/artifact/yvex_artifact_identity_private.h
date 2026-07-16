/*
 * yvex_artifact_identity_private.h - internal streaming artifact identity.
 *
 * Owner: TRACK.ARTIFACT artifact-identity cell.
 * Owns: incremental exact-file SHA-256 state shared with bounded validators.
 * Does not own: file IO, GGUF semantics, payload validation, or admission.
 * Invariants: callers supply bytes in exact file order and finalize once with
 *   the expected physical length; partial identities are never published.
 * Boundary: a byte digest is identity evidence, not artifact completeness.
 */
#ifndef YVEX_ARTIFACT_IDENTITY_PRIVATE_H
#define YVEX_ARTIFACT_IDENTITY_PRIVATE_H

#include "src/core/yvex_sha256.h"

#include <yvex/artifact_identity.h>

typedef struct {
    yvex_sha256 hash;
    unsigned long long bytes;
    int active;
} yvex_artifact_identity_stream;

void yvex_artifact_identity_stream_init(
    yvex_artifact_identity_stream *stream);
int yvex_artifact_identity_stream_update(
    yvex_artifact_identity_stream *stream,
    const unsigned char *bytes,
    size_t byte_count,
    yvex_error *err);
int yvex_artifact_identity_stream_final(
    yvex_artifact_identity_stream *stream,
    unsigned long long expected_bytes,
    char out_hex[YVEX_SHA256_HEX_CAP],
    yvex_error *err);

#endif
