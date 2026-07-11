/*
 * YVEX - Model artifact byte view
 *
 * File: include/yvex/artifact.h
 * Layer: public artifact API
 *
 * Purpose:
 *   Defines a read-only artifact file handle with 64-bit size, bounded
 *   positioned reads, and an optional explicit whole-file mapping for payload
 *   consumers.
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
 *   - GGUF structural read policy
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

#include <stddef.h>

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_ARTIFACT_PATH_CAP 4096

typedef struct yvex_artifact yvex_artifact;

typedef struct {
    const char *path;
    /* Read-only access is the only admitted mode. A false value is refused. */
    int readonly;
    /* Whole-file mapping is opt-in and intended only for explicit payload users. */
    int map;
} yvex_artifact_options;

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *options, yvex_error *err);
void yvex_artifact_close(yvex_artifact *artifact);

const char *yvex_artifact_path(const yvex_artifact *artifact);
unsigned long long yvex_artifact_size(const yvex_artifact *artifact);
int yvex_artifact_is_mapped(const yvex_artifact *artifact);
/* The returned mapping is borrowed until yvex_artifact_close, or null if map=0. */
const unsigned char *yvex_artifact_data(const yvex_artifact *artifact);
/* Reads exactly len bytes without changing shared file position or mapping the file. */
int yvex_artifact_read_at(const yvex_artifact *artifact,
                          unsigned long long offset,
                          void *dst,
                          size_t len,
                          yvex_error *err);

int yvex_range_check(unsigned long long file_size,
                     unsigned long long offset,
                     unsigned long long len,
                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_H */
