/*
 * yvex_source_provenance.h - exact source and manifest provenance owner.
 *
 * Owner: src/source.
 * Owns: structured manifest provenance, Hugging Face metadata, and pinned index identity.
 * Does not own: model config parsing, shard/header inventory, manifest publication, or rendering.
 * Invariants: revisions and upstream index OIDs are exact; aliases such as main are not final proof.
 * Boundary: metadata provenance is not tensor payload digest verification.
 */
#ifndef YVEX_SOURCE_PROVENANCE_H
#define YVEX_SOURCE_PROVENANCE_H

#include "yvex_source_verify.h"

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

#endif
