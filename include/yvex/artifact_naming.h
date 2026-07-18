/*
 * Owner: abi.artifact_naming (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Artifact naming helpers
 *
 * File: include/yvex/artifact_naming.h
 * Layer: public tool/support API
 *
 * Purpose:
 *   Defines the canonical generated GGUF artifact filename grammar used by
 *   YVEX reports, examples, and produced external artifacts.
 */
#ifndef YVEX_ARTIFACT_NAMING_H
#define YVEX_ARTIFACT_NAMING_H

#include <stddef.h>

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

int yvex_artifact_name_suggest(char *out,
                               size_t out_size,
                               const char *family,
                               const char *model,
                               const char *scope,
                               const char *artifact_class,
                               const char *qprofile,
                               const char *calibration,
                               const char *producer,
                               const char *schema,
                               yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_NAMING_H */
