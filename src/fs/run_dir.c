/*
 * YVEX - Run directory skeleton
 *
 * File: src/fs/run_dir.c
 * Layer: runtime filesystem implementation
 *
 * Purpose:
 *   Implements runtime filesystem run ID creation, run-directory path preparation, and
 *   recursive directory creation. This module creates directories only; it
 *   does not write model, metrics, trace, receipt, or command artifacts.
 *
 * Implements:
 *   - yvex_run_id_make
 *   - yvex_run_dir_prepare
 *   - yvex_run_dir_create
 *   - yvex_run_dir_print
 *
 * Invariants:
 *   - run IDs use run_YYYYMMDD_HHMMSS_pid shape
 *   - existing directories are tolerated
 *   - existing non-directory path collisions fail clearly
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_fs
 */
#define _POSIX_C_SOURCE 200809L

#include <yvex/fs.h>

#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static int yvex_path_format(char *dst, size_t cap, yvex_error *err, const char *where, const char *fmt, ...)
{
    va_list ap;
    int n;

    if (!dst || cap == 0 || !fmt) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "invalid path format argument");
        return YVEX_ERR_INVALID_ARG;
    }

    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);

    if (n < 0 || (size_t)n >= cap) {
        dst[cap - 1] = '\0';
        yvex_error_setf(err, YVEX_ERR_BOUNDS, where, "path exceeds capacity %lu", (unsigned long)cap);
        return YVEX_ERR_BOUNDS;
    }

    return YVEX_OK;
}

static const char *yvex_env_or_null(const char *name)
{
    const char *value = getenv(name);
    if (!value || value[0] == '\0') {
        return NULL;
    }
    return value;
}

static int yvex_path_is_dir(const char *path, int *out_is_dir, yvex_error *err, const char *where)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        if (errno == ENOENT) {
            *out_is_dir = 0;
            return YVEX_OK;
        }
        yvex_error_setf(err, YVEX_ERR_IO, where, "stat failed for %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }

    *out_is_dir = S_ISDIR(st.st_mode) ? 1 : -1;
    return YVEX_OK;
}

static int yvex_mkdir_one(const char *path, yvex_error *err)
{
    int is_dir;
    int rc;

    if (mkdir(path, 0777) == 0) {
        return YVEX_OK;
    }

    if (errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_dir_create", "mkdir failed for %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }

    rc = yvex_path_is_dir(path, &is_dir, err, "yvex_run_dir_create");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (is_dir == 1) {
        return YVEX_OK;
    }

    yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_dir_create", "path exists and is not a directory: %s", path);
    return YVEX_ERR_IO;
}

static int yvex_mkdir_p(const char *path, yvex_error *err)
{
    char tmp[YVEX_PATH_CAP];
    char *p;
    size_t len;
    int rc;

    if (!path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_dir_create", "directory path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_path_format(tmp, sizeof(tmp), err, "yvex_run_dir_create", "%s", path);
    if (rc != YVEX_OK) {
        return rc;
    }

    len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        --len;
    }

    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (tmp[0] != '\0') {
                rc = yvex_mkdir_one(tmp, err);
                if (rc != YVEX_OK) {
                    return rc;
                }
            }
            *p = '/';
        }
    }

    return yvex_mkdir_one(tmp, err);
}

int yvex_run_id_make(char *out, unsigned long cap, yvex_error *err)
{
    time_t now;
    struct tm tm_buf;
    char stamp[32];
    int n;

    if (!out || cap == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_id_make", "out and cap are required");
        return YVEX_ERR_INVALID_ARG;
    }

    now = time(NULL);
    if (now == (time_t)-1) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_run_id_make", "time() failed");
        return YVEX_ERR_STATE;
    }

    if (!localtime_r(&now, &tm_buf)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_run_id_make", "localtime_r() failed");
        return YVEX_ERR_STATE;
    }

    if (strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm_buf) == 0) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_run_id_make", "timestamp does not fit run id buffer");
        return YVEX_ERR_BOUNDS;
    }

    n = snprintf(out, (size_t)cap, "run_%s_%lu", stamp, (unsigned long)getpid());
    if (n < 0 || (unsigned long)n >= cap) {
        out[cap - 1] = '\0';
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_run_id_make", "run id exceeds capacity %lu", cap);
        return YVEX_ERR_BOUNDS;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_run_dir_prepare(yvex_run_dir *out, const yvex_paths *paths, const char *run_id, yvex_error *err)
{
    char generated[YVEX_RUN_ID_CAP];
    char run_root[YVEX_PATH_CAP];
    const char *resolved_run_id;
    const char *env_run_dir;
    int rc;

    if (!out || !paths) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_dir_prepare", "out and paths are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    if (run_id && run_id[0] != '\0') {
        resolved_run_id = run_id;
    } else {
        rc = yvex_run_id_make(generated, sizeof(generated), err);
        if (rc != YVEX_OK) {
            return rc;
        }
        resolved_run_id = generated;
    }

    rc = yvex_path_format(out->run_id, sizeof(out->run_id), err, "yvex_run_dir_prepare", "%s", resolved_run_id);
    if (rc != YVEX_OK) {
        return rc;
    }

    env_run_dir = yvex_env_or_null("YVEX_RUN_DIR");
    if (env_run_dir) {
        rc = yvex_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s", env_run_dir);
    } else if (paths->project_dir[0] != '\0') {
        rc = yvex_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s/runs", paths->project_dir);
    } else {
        rc = yvex_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s/runs", paths->state_dir);
    }
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_path_format(out->root, sizeof(out->root), err, "yvex_run_dir_prepare", "%s/%s", run_root, out->run_id);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->command_path, sizeof(out->command_path), err, "yvex_run_dir_prepare", "%s/command.txt", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->stdout_path, sizeof(out->stdout_path), err, "yvex_run_dir_prepare", "%s/stdout.log", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->stderr_path, sizeof(out->stderr_path), err, "yvex_run_dir_prepare", "%s/stderr.log", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->metrics_path, sizeof(out->metrics_path), err, "yvex_run_dir_prepare", "%s/metrics.json", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->trace_path, sizeof(out->trace_path), err, "yvex_run_dir_prepare", "%s/trace.jsonl", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->receipt_path, sizeof(out->receipt_path), err, "yvex_run_dir_prepare", "%s/receipt.json", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_run_dir_create(const yvex_run_dir *run, yvex_error *err)
{
    int rc;

    if (!run || run->root[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_dir_create", "run root is required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_mkdir_p(run->root, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_run_dir_print(const yvex_run_dir *run, FILE *fp, yvex_error *err)
{
    if (!run || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_dir_print", "run and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "run_id: %s\n", run->run_id);
    fprintf(fp, "root: %s\n", run->root);
    fprintf(fp, "command: %s\n", run->command_path);
    fprintf(fp, "stdout: %s\n", run->stdout_path);
    fprintf(fp, "stderr: %s\n", run->stderr_path);
    fprintf(fp, "metrics: %s\n", run->metrics_path);
    fprintf(fp, "trace: %s\n", run->trace_path);
    fprintf(fp, "receipt: %s\n", run->receipt_path);

    yvex_error_clear(err);
    return YVEX_OK;
}
