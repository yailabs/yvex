/*
 * core.c - artifact byte views and operator artifact surfaces.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   artifact IO, artifact lifetime, artifact metadata, checked tensor byte
 *   ranges, inspect/metadata/tensor command surfaces, and selected
 *   materialization gates.
 *
 * Does not own:
 *   source verification, model runtime support, graph execution, generation,
 *   eval, benchmark, throughput, or release decisions.
 *
 * Invariants:
 *   artifact bytes are opened through explicit paths or model references; range
 *   and integrity checks precede materialization surfaces; artifact parsing
 *   remains separate from runtime claims.
 *
 * Boundary:
 *   artifact presence, parsing, identity, or selected materialization is not
 *   runtime generation, eval evidence, benchmark evidence, throughput, or
 *   release readiness.
 */

#include <stdint.h>
#include <yvex/artifact.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>



struct yvex_artifact {
    char path[YVEX_ARTIFACT_PATH_CAP];
    unsigned long long size;
    yvex_artifact_snapshot snapshot;
    int fd;
    unsigned char *mapping;
    size_t mapping_len;
};

/* Contract: projects regular-file stat identity without allocation or IO. */
static void snapshot_from_stat(const struct stat *st, yvex_artifact_snapshot *out)
{
    if (!st || !out) return;
    memset(out, 0, sizeof(*out));
    out->device = (unsigned long long)st->st_dev;
    out->inode = (unsigned long long)st->st_ino;
    out->size = (unsigned long long)st->st_size;
    out->mtime_seconds = (long long)st->st_mtim.tv_sec;
    out->mtime_nanoseconds = (long long)st->st_mtim.tv_nsec;
    out->ctime_seconds = (long long)st->st_ctim.tv_sec;
    out->ctime_nanoseconds = (long long)st->st_ctim.tv_nsec;
}

/* Contract: compares every captured identity field without filesystem IO. */
static int snapshot_equal(const yvex_artifact_snapshot *a,
                          const yvex_artifact_snapshot *b)
{
    return a && b &&
           a->device == b->device &&
           a->inode == b->inode &&
           a->size == b->size &&
           a->mtime_seconds == b->mtime_seconds &&
           a->mtime_nanoseconds == b->mtime_nanoseconds &&
           a->ctime_seconds == b->ctime_seconds &&
           a->ctime_nanoseconds == b->ctime_nanoseconds;
}

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

/*
 * yvex_artifact_open()
 *
 * Purpose:
 *   open a read-only artifact handle and optionally map it for explicit payload
 *   access.
 *
 * Inputs:
 *   options is borrowed and must provide a path; out receives owned artifact
 *   storage.
 *
 * Effects:
 *   opens a file descriptor, reads filesystem metadata, and optionally creates
 *   a private read-only mapping. No file bytes are copied.
 *
 * Failure:
 *   returns invalid-arg, bounds, IO, or allocation errors and releases partial
 *   artifact storage.
 *
 * Boundary:
 *   opening artifact bytes is not GGUF validation, runtime support, generation,
 *   eval evidence, benchmark evidence, or release readiness.
 */
int yvex_artifact_open(yvex_artifact **out, const yvex_artifact_options *options, yvex_error *err)
{
    struct stat st;
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

    artifact->fd = -1;
    rc = copy_path(artifact->path, options->path, err);
    if (rc != YVEX_OK) {
        free(artifact);
        return rc;
    }

    if (!options->readonly) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_artifact_open",
                       "artifact opens require readonly mode");
        free(artifact);
        return YVEX_ERR_UNSUPPORTED;
    }

    artifact->fd = open(options->path, O_RDONLY | O_CLOEXEC);
    if (artifact->fd < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to open %s: %s", options->path, strerror(errno));
        free(artifact);
        return YVEX_ERR_IO;
    }

    if (fstat(artifact->fd, &st) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                        "failed to stat %s: %s", options->path, strerror(errno));
        yvex_artifact_close(artifact);
        return YVEX_ERR_IO;
    }
    if (!S_ISREG(st.st_mode) || st.st_size < 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_artifact_open",
                       "artifact path must name a regular file");
        yvex_artifact_close(artifact);
        return YVEX_ERR_FORMAT;
    }
    artifact->size = (unsigned long long)st.st_size;
    snapshot_from_stat(&st, &artifact->snapshot);

    if (options->map && artifact->size > 0ull) {
        if (artifact->size > (unsigned long long)SIZE_MAX) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_open",
                           "artifact is too large for this address space mapping");
            yvex_artifact_close(artifact);
            return YVEX_ERR_BOUNDS;
        }
        artifact->mapping_len = (size_t)artifact->size;
        artifact->mapping = (unsigned char *)mmap(NULL,
                                                  artifact->mapping_len,
                                                  PROT_READ,
                                                  MAP_PRIVATE,
                                                  artifact->fd,
                                                  0);
        if (artifact->mapping == MAP_FAILED) {
            artifact->mapping = NULL;
            artifact->mapping_len = 0u;
            yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_open",
                            "failed to map %s: %s", options->path, strerror(errno));
            yvex_artifact_close(artifact);
            return YVEX_ERR_IO;
        }
    }
    *out = artifact;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_artifact_close(yvex_artifact *artifact)
{
    if (!artifact) {
        return;
    }

    if (artifact->mapping) {
        (void)munmap(artifact->mapping, artifact->mapping_len);
        artifact->mapping = NULL;
        artifact->mapping_len = 0u;
    }
    if (artifact->fd >= 0) {
        (void)close(artifact->fd);
        artifact->fd = -1;
    }
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

int yvex_artifact_is_mapped(const yvex_artifact *artifact)
{
    return artifact && artifact->mapping ? 1 : 0;
}

const unsigned char *yvex_artifact_data(const yvex_artifact *artifact)
{
    if (!artifact) {
        return NULL;
    }
    return artifact->mapping;
}

/*
 * yvex_artifact_read_at()
 *
 * Purpose:
 *   read one exact bounded file range without changing shared file position.
 *
 * Inputs:
 *   artifact is borrowed; dst receives len bytes and may be null only for a
 *   zero-length read.
 *
 * Effects:
 *   performs positioned read IO only; it does not map, allocate, or retain dst.
 *
 * Failure:
 *   returns invalid-arg, bounds, or IO and leaves artifact ownership unchanged.
 *
 * Boundary:
 *   reading a requested range is not GGUF parsing or payload trust.
 */
int yvex_artifact_read_at(const yvex_artifact *artifact,
                          unsigned long long offset,
                          void *dst,
                          size_t len,
                          yvex_error *err)
{
    unsigned char *out = (unsigned char *)dst;
    size_t done = 0u;

    if (!artifact || (!dst && len != 0u)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_read_at",
                       "artifact and destination are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_range_check(artifact->size, offset, (unsigned long long)len, err) != YVEX_OK) {
        return YVEX_ERR_BOUNDS;
    }

    while (done < len) {
        unsigned long long current = offset + (unsigned long long)done;
        ssize_t n;
        if (current > (unsigned long long)INT64_MAX) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_artifact_read_at",
                           "artifact offset exceeds positioned IO range");
            return YVEX_ERR_BOUNDS;
        }
        n = pread(artifact->fd, out + done, len - done, (off_t)current);
        if (n < 0) {
            if (errno == EINTR) continue;
            yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_read_at",
                            "positioned read failed at offset %llu: %s",
                            current, strerror(errno));
            return YVEX_ERR_IO;
        }
        if (n == 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "yvex_artifact_read_at",
                            "short read at offset %llu", current);
            return YVEX_ERR_IO;
        }
        done += (size_t)n;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: copies open-time identity and performs no IO or allocation. */
int yvex_artifact_snapshot_get(const yvex_artifact *artifact,
                               yvex_artifact_snapshot *out,
                               yvex_error *err)
{
    if (!artifact || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_snapshot_get",
                       "artifact and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = artifact->snapshot;
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Contract: stats both the borrowed open descriptor and its path, mutates no
 * state, and fails closed when either identity differs from open time.
 */
int yvex_artifact_snapshot_validate(const yvex_artifact *artifact,
                                    yvex_artifact_snapshot *current,
                                    yvex_error *err)
{
    struct stat open_st;
    struct stat path_st;
    yvex_artifact_snapshot open_snapshot;
    yvex_artifact_snapshot path_snapshot;

    if (!artifact) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_artifact_snapshot_validate",
                       "artifact is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (fstat(artifact->fd, &open_st) != 0 || !S_ISREG(open_st.st_mode)) {
        yvex_error_set(err, YVEX_ERR_IO, "yvex_artifact_snapshot_validate",
                       "artifact-identity-drift: opened file cannot be restated");
        return YVEX_ERR_IO;
    }
    snapshot_from_stat(&open_st, &open_snapshot);
    if (current) *current = open_snapshot;
    if (!snapshot_equal(&artifact->snapshot, &open_snapshot)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_artifact_snapshot_validate",
                       "artifact-identity-drift: opened file changed after admission");
        return YVEX_ERR_FORMAT;
    }
    if (stat(artifact->path, &path_st) != 0 || !S_ISREG(path_st.st_mode)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_artifact_snapshot_validate",
                       "artifact-identity-drift: artifact path no longer names the opened file");
        return YVEX_ERR_FORMAT;
    }
    snapshot_from_stat(&path_st, &path_snapshot);
    if (!snapshot_equal(&artifact->snapshot, &path_snapshot)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_artifact_snapshot_validate",
                       "artifact-identity-drift: artifact path identity changed");
        return YVEX_ERR_FORMAT;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_range_check(unsigned long long file_size,
                     unsigned long long offset,
                     unsigned long long len,
                     yvex_error *err)
{
    if (offset > file_size) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_range_check",
                        "offset %llu exceeds file size %llu", offset, file_size);
        return YVEX_ERR_BOUNDS;
    }

    if (len > file_size - offset) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_range_check",
                        "range offset=%llu len=%llu exceeds file size %llu",
                        offset, len, file_size);
        return YVEX_ERR_BOUNDS;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}
