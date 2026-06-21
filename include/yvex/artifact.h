/*
 * YVEX - Model artifact byte view
 *
 * File: include/yvex/artifact.h
 * Layer: public artifact API
 *
 * Purpose:
 *   Defines the implemented C0 artifact byte-view API. Artifacts are opened
 *   read-only, loaded into an owned byte buffer, and exposed through bounded
 *   size/data accessors for format parsers.
 *
 * Owns:
 *   - yvex_artifact
 *   - yvex_artifact_options
 *   - yvex_artifact_open
 *   - yvex_artifact_close
 *   - yvex_range_check
 *
 * Does not own:
 *   - GGUF metadata parsing
 *   - tensor directories
 *   - mmap support claims
 *   - model execution
 *
 * Used by:
 *   - GGUF probe/header parser
 *   - yvex inspect
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_artifact
 */
#ifndef YVEX_ARTIFACT_H
#define YVEX_ARTIFACT_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_ARTIFACT_PATH_CAP 4096

typedef struct yvex_artifact yvex_artifact;

typedef struct {
    const char *path;
    int readonly;
    int map;
} yvex_artifact_options;

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *options, yvex_error *err);
void yvex_artifact_close(yvex_artifact *artifact);

const char *yvex_artifact_path(const yvex_artifact *artifact);
unsigned long long yvex_artifact_size(const yvex_artifact *artifact);
const unsigned char *yvex_artifact_data(const yvex_artifact *artifact);

int yvex_range_check(unsigned long long file_size,
                     unsigned long long offset,
                     unsigned long long len,
                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_H */
