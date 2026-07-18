/*
 * provenance.h - exact source and manifest provenance owner.
 *
 * Owner: src/source.
 * Owns: structured manifest provenance, Hugging Face metadata, and pinned index identity.
 * Does not own: model config parsing, shard/header inventory, manifest publication, or rendering.
 * Invariants: revisions and upstream index OIDs are exact; aliases such as main are not final proof.
 * Boundary: metadata provenance is not tensor payload digest verification.
 */
#ifndef YVEX_SOURCE_PROVENANCE_H
#define YVEX_SOURCE_PROVENANCE_H

#include "verify.h"

int yvex_source_provenance_manifest_read(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_error *err);
int yvex_source_provenance_verify_file(
    const yvex_source_verify_options *options,
    const char *name,
    int verify_upstream_index,
    yvex_source_verification *out,
    yvex_error *err);
void yvex_source_provenance_finalize(
    const yvex_source_verify_options *options,
    yvex_source_verification *out);
int yvex_source_provenance_manifest_matches(
    const yvex_source_verify_options *options,
    const yvex_source_verification *out);
int yvex_source_git_blob_oid_file(const char *path,
                                  char out_hex[41],
                                  yvex_error *err);

typedef struct {
    int available;
    int revision_matches;
    char algorithm[24];
    char authority[40];
    char expected_digest[65];
} yvex_source_payload_digest_fact;

typedef struct {
    char canonical_name[YVEX_PATH_CAP];
    char revision[65];
    char expected_git_blob_oid[41];
    char observed_git_blob_oid[41];
    unsigned long long file_bytes;
    int revision_matches;
    int identity_verified;
} yvex_source_metadata_identity_fact;

typedef struct {
    yvex_source_metadata_identity_fact identity;
    unsigned char *bytes;
    size_t byte_count;
} yvex_source_metadata_blob;

int yvex_source_provenance_payload_digest(
    const yvex_source_verification *verification,
    const char *canonical_name,
    yvex_source_payload_digest_fact *out,
    yvex_error *err);
int yvex_source_provenance_metadata_identity(
    const yvex_source_verification *verification,
    const char *canonical_name,
    yvex_source_metadata_identity_fact *out,
    yvex_error *err);
int yvex_source_provenance_metadata_read(
    const yvex_source_verification *verification,
    const char *canonical_name,
    size_t maximum_bytes,
    yvex_source_metadata_blob *out,
    yvex_error *err);
void yvex_source_metadata_blob_release(yvex_source_metadata_blob *blob);

#endif
