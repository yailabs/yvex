/*
 * yvex_source_payload_identity.c - canonical payload snapshot identity owner.
 *
 * Owner: src/source.
 * Owns: deterministic aggregate payload identity and trust-class projection.
 * Does not own: file IO, digest calculation, manifest serialization, or mapping.
 * Invariants: identity excludes local paths, timestamps, and mutable file identity.
 * Boundary: local sealing remains distinct from upstream payload verification.
 */
#include "yvex_source_payload_internal.h"

#include "yvex_sha256.h"
#include "yvex_source_write.h"

#include <limits.h>
#include <string.h>

static int payload_identity_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int index;

    for (index = 0u; index < 8u; ++index)
        bytes[index] = (unsigned char)((value >> (index * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

static int payload_identity_text(yvex_sha256 *hash, const char *text)
{
    size_t length = text ? strlen(text) : 0u;

    return payload_identity_u64(hash, (unsigned long long)length) &&
           yvex_sha256_update(hash, text, length);
}

/* Computes stable identity only after every shard owns a verified/sealed digest. */
int yvex_source_payload_identity_compute(
    yvex_source_payload_session *session,
    yvex_source_payload_failure *failure,
    yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    char identity[YVEX_SOURCE_PAYLOAD_IDENTITY_CAP];
    yvex_source_payload_trust_class aggregate =
        YVEX_SOURCE_PAYLOAD_TRUST_UPSTREAM_VERIFIED;
    unsigned long long index;

    if (!session || !session->target_id[0] || !session->family_key[0] ||
        !session->repository_id[0]) {
        yvex_source_payload_fail(
            failure, YVEX_SOURCE_PAYLOAD_FAILURE_INVALID_ARGUMENT,
            ULLONG_MAX, ULLONG_MAX, 0u, 0u, 0, err, YVEX_ERR_INVALID_ARG,
            "source_payload_identity", "payload session identity facts are required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_sha256_init(&hash);
    if (!payload_identity_text(&hash, "yvex.source_manifest.v3") ||
        !payload_identity_text(
            &hash, session->target_id) ||
        !payload_identity_text(&hash, session->repository_id) ||
        !payload_identity_text(&hash, session->verification.revision) ||
        !payload_identity_u64(
            &hash, session->verification.source_snapshot_identity) ||
        !payload_identity_u64(&hash, session->shard_count)) goto overflow;
    for (index = 0u; index < session->shard_count; ++index) {
        yvex_source_payload_owned_shard *shard = &session->shards[index];
        const char *trust = yvex_source_payload_trust_class_name(
            shard->public_fact.trust_class);

        if (!shard->observed_digest[0] ||
            shard->public_fact.trust_class == YVEX_SOURCE_PAYLOAD_TRUST_NONE) {
            yvex_source_payload_fail(
                failure, YVEX_SOURCE_PAYLOAD_FAILURE_PAYLOAD_NOT_TRUSTED,
                index, ULLONG_MAX, 0u, 0u, 0, err, YVEX_ERR_STATE,
                "source_payload_identity", "all shard digests must complete before identity");
            return YVEX_ERR_STATE;
        }
        if (shard->public_fact.trust_class ==
            YVEX_SOURCE_PAYLOAD_TRUST_LOCAL_SNAPSHOT_SEALED)
            aggregate = YVEX_SOURCE_PAYLOAD_TRUST_LOCAL_SNAPSHOT_SEALED;
        if (!payload_identity_u64(&hash, index) ||
            !payload_identity_text(&hash, shard->name) ||
            !payload_identity_u64(&hash, shard->public_fact.file_bytes) ||
            !payload_identity_text(&hash, shard->digest_algorithm) ||
            !payload_identity_text(&hash, shard->observed_digest) ||
            !payload_identity_text(&hash, trust)) goto overflow;
    }
    if (!yvex_sha256_final(&hash, digest)) goto overflow;
    yvex_sha256_hex(digest, identity);
    if (session->verification.manifest_payload_trusted &&
        session->verification.manifest_payload_identity[0] &&
        strcmp(identity,
               session->verification.manifest_payload_identity) != 0) {
        pthread_mutex_lock(&session->mutex);
        session->state = YVEX_SOURCE_PAYLOAD_STATE_POISONED;
        session->facts.state = session->state;
        pthread_mutex_unlock(&session->mutex);
        yvex_source_payload_fail(
            failure, YVEX_SOURCE_PAYLOAD_FAILURE_PAYLOAD_IDENTITY_MISMATCH,
            ULLONG_MAX, ULLONG_MAX, 0u, 0u, 0, err, YVEX_ERR_FORMAT,
            "source_payload_identity",
            "observed payload set does not match the published snapshot identity");
        return YVEX_ERR_FORMAT;
    }
    memcpy(session->facts.payload_identity, identity, sizeof(identity));
    session->facts.trust_class = aggregate;
    session->facts.trusted_shard_count = session->shard_count;
    for (index = 0u; index < session->tensor_count; ++index) {
        session->ranges[index].payload_identity =
            session->facts.payload_identity;
        session->ranges[index].trust_class = aggregate;
    }
    yvex_error_clear(err);
    return YVEX_OK;
overflow:
    yvex_source_payload_fail(
        failure, YVEX_SOURCE_PAYLOAD_FAILURE_RANGE_OVERFLOW,
        ULLONG_MAX, ULLONG_MAX, 0u, 0u, 0, err, YVEX_ERR_BOUNDS,
        "source_payload_identity", "canonical payload identity input overflow");
    return YVEX_ERR_BOUNDS;
}

/* Delegates atomic v3 publication to the canonical source writer owner. */
int yvex_source_payload_manifest_publish(
    const yvex_source_payload_session *session,
    yvex_error *err)
{
    if (!session || !session->manifest_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_payload_manifest",
                       "payload session and manifest path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    return yvex_source_manifest_publish_payload(
        session->manifest_path, &session->verification, session, err);
}
