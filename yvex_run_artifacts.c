/*
 * YVEX - Run artifact paths
 *
 * File: yvex_run_artifacts.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Prepares observability layer metrics/trace/profile artifact paths for run/chat commands.
 *   It creates run directories when requested and writes command.txt only.
 */
#define _POSIX_C_SOURCE 200809L

#include "yvex_metrics_internal.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

static int path_format(char *dst, size_t cap, yvex_error *err, const char *where, const char *fmt, ...)
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
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int mkdir_one(const char *path, yvex_error *err)
{
    struct stat st;

    if (mkdir(path, 0777) == 0) {
        return YVEX_OK;
    }
    if (errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_prepare",
                        "mkdir failed for %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_prepare",
                        "path exists but is not a directory: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int mkdir_p(const char *path, yvex_error *err)
{
    char tmp[YVEX_PATH_CAP];
    char *p;
    size_t len;
    int rc;

    if (!path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_artifacts_prepare",
                       "directory path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = path_format(tmp, sizeof(tmp), err, "yvex_run_artifacts_prepare", "%s", path);
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
            rc = mkdir_one(tmp, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            *p = '/';
        }
    }
    return mkdir_one(tmp, err);
}

static int copy_optional_path(char *dst, const char *src, yvex_error *err, const char *where)
{
    if (!src || src[0] == '\0') {
        dst[0] = '\0';
        return YVEX_OK;
    }
    return path_format(dst, YVEX_PATH_CAP, err, where, "%s", src);
}

int yvex_run_artifacts_prepare(yvex_run_artifacts *out,
                               int save_run,
                               const char *run_dir,
                               const char *metrics_out,
                               const char *trace_out,
                               const char *profile_out,
                               yvex_error *err)
{
    yvex_paths paths;
    yvex_run_dir prepared;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_artifacts_prepare", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    rc = yvex_run_id_make(out->run_id, sizeof(out->run_id), err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (save_run) {
        if (run_dir && run_dir[0] != '\0') {
            rc = path_format(out->run_dir, sizeof(out->run_dir), err,
                             "yvex_run_artifacts_prepare", "%s", run_dir);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = mkdir_p(out->run_dir, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = path_format(out->command_path, sizeof(out->command_path), err,
                             "yvex_run_artifacts_prepare", "%s/command.txt", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->metrics_path, sizeof(out->metrics_path), err,
                             "yvex_run_artifacts_prepare", "%s/metrics.json", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->trace_path, sizeof(out->trace_path), err,
                             "yvex_run_artifacts_prepare", "%s/trace.jsonl", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->profile_path, sizeof(out->profile_path), err,
                             "yvex_run_artifacts_prepare", "%s/profile.json", out->run_dir);
            if (rc != YVEX_OK) return rc;
        } else {
            rc = yvex_paths_default(&paths, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = yvex_run_dir_prepare(&prepared, &paths, out->run_id, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = yvex_run_dir_create(&prepared, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = path_format(out->run_dir, sizeof(out->run_dir), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.root);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->command_path, sizeof(out->command_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.command_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->metrics_path, sizeof(out->metrics_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.metrics_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->trace_path, sizeof(out->trace_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.trace_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->profile_path, sizeof(out->profile_path), err,
                             "yvex_run_artifacts_prepare", "%s/profile.json", prepared.root);
            if (rc != YVEX_OK) return rc;
        }
        out->has_run_dir = 1;
        out->has_metrics = 1;
        out->has_trace = 1;
        out->has_profile = 1;
    }

    if (metrics_out && metrics_out[0] != '\0') {
        rc = copy_optional_path(out->metrics_path, metrics_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_metrics = 1;
    }
    if (trace_out && trace_out[0] != '\0') {
        rc = copy_optional_path(out->trace_path, trace_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_trace = 1;
    }
    if (profile_out && profile_out[0] != '\0') {
        rc = copy_optional_path(out->profile_path, profile_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_profile = 1;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts,
                                     int argc,
                                     char **argv,
                                     yvex_error *err)
{
    FILE *fp;
    int i;

    if (!artifacts || !artifacts->has_run_dir || artifacts->command_path[0] == '\0') {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    fp = fopen(artifacts->command_path, "w");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_write_command",
                        "cannot open command file %s", artifacts->command_path);
        return YVEX_ERR_IO;
    }
    for (i = 0; i < argc; ++i) {
        if (i > 0) {
            fputc(' ', fp);
        }
        fputs(argv[i] ? argv[i] : "", fp);
    }
    fputc('\n', fp);
    fclose(fp);

    yvex_error_clear(err);
    return YVEX_OK;
}
