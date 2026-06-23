/*
 * YVEX - Artifact byte view
 *
 * File: src/artifact/artifact.c
 * Layer: artifact implementation
 *
 * Purpose:
 *   Implements read-only artifact opening for artifact layer. The `map` option is accepted
 *   as a future policy flag, but artifact layer uses an owned in-memory read buffer and
 *   makes no mmap support claim.
 *
 * Implements:
 *   - yvex_artifact_open
 *   - yvex_artifact_close
 *   - yvex_artifact_path
 *   - yvex_artifact_size
 *   - yvex_artifact_data
 *
 * Invariants:
 *   - artifact data remains valid until yvex_artifact_close
 *   - library code does not print to stderr
 *   - partial reads fail explicitly
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_artifact
 */
#include <yvex/artifact.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct yvex_artifact {
    char path[YVEX_ARTIFACT_PATH_CAP];
    unsigned long long size;
    unsigned char *data;
};

static int copy_path(char *dst, const char *src, yvex_error *err)
{
    int n;

    if (!src || src[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_open", "path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(dst, YVEX_ARTIFACT_PATH_CAP, "%s", src);
    if (n < 0 || n >= YVEX_ARTIFACT_PATH_CAP) {
        dst[YVEX_ARTIFACT_PATH_CAP - 1] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_open", "artifact path exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }

    return YVEX_OK;
}

int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *options, yvex_error *err)
{
    FILE *fp;
    long end_pos;
    size_t read_n;
    yvex_artifact *artifact;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (!options || !options->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_open", "options.path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    artifact = (yvex_artifact *)calloc(1, sizeof(*artifact));
    if (!artifact) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_artifact_open", "failed to allocate artifact");
        return YVEX_ERR_NOMEM;
    }

    rc = copy_path(artifact->path, options->path, err);
    if (rc != YVEX_OK) {
        free(artifact);
        return rc;
    }

    (void)options->readonly;
    (void)options->map;

    fp = fopen(options->path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to open %s: %s", options->path, strerror(errno));
        free(artifact);
        return YVEX_ERR_IO;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to seek %s: %s", options->path, strerror(errno));
        fclose(fp);
        free(artifact);
        return YVEX_ERR_IO;
    }

    end_pos = ftell(fp);
    if (end_pos < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to determine size for %s: %s", options->path, strerror(errno));
        fclose(fp);
        free(artifact);
        return YVEX_ERR_IO;
    }
    artifact->size = (unsigned long long)end_pos;

    if (fseek(fp, 0, SEEK_SET) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to rewind %s: %s", options->path, strerror(errno));
        fclose(fp);
        free(artifact);
        return YVEX_ERR_IO;
    }

    artifact->data = (unsigned char *)malloc((size_t)(artifact->size ? artifact->size : 1));
    if (!artifact->data) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_artifact_open", "failed to allocate artifact data");
        fclose(fp);
        free(artifact);
        return YVEX_ERR_NOMEM;
    }

    read_n = fread(artifact->data, 1, (size_t)artifact->size, fp);
    if (read_n != (size_t)artifact->size) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "short read for %s: expected %llu bytes, read %lu",
                        options->path, artifact->size, (unsigned long)read_n);
        fclose(fp);
        yvex_artifact_close(artifact);
        return YVEX_ERR_IO;
    }

    fclose(fp);
    *out = artifact;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_artifact_close(yvex_artifact *artifact)
{
    if (!artifact) {
        return;
    }

    free(artifact->data);
    artifact->data = NULL;
    free(artifact);
}

const char *yvex_artifact_path(const yvex_artifact *artifact)
{
    if (!artifact) {
        return "";
    }
    return artifact->path;
}

unsigned long long yvex_artifact_size(const yvex_artifact *artifact)
{
    if (!artifact) {
        return 0;
    }
    return artifact->size;
}

const unsigned char *yvex_artifact_data(const yvex_artifact *artifact)
{
    if (!artifact) {
        return NULL;
    }
    return artifact->data;
}
