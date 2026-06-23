/*
 * YVEX - Runtime path resolution
 *
 * File: src/fs/paths.c
 * Layer: runtime filesystem implementation
 *
 * Purpose:
 *   Implements runtime filesystem path construction for user-default and project-local
 *   runtime filesystem locations. This module reads environment overrides
 *   and builds bounded, null-terminated paths.
 *
 * Implements:
 *   - yvex_paths_default
 *   - yvex_paths_project
 *   - yvex_paths_print
 *
 * Invariants:
 *   - every output path is null-terminated
 *   - HOME is required for default path resolution
 *   - path truncation returns YVEX_ERR_BOUNDS
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_fs
 */
#include <yvex/fs.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

static int yvex_copy_path(char *dst, const char *value, yvex_error *err, const char *where)
{
    return yvex_path_format(dst, YVEX_PATH_CAP, err, where, "%s", value);
}

int yvex_paths_default(yvex_paths *out, yvex_error *err)
{
    const char *home;
    const char *env;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_paths_default", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    home = getenv("HOME");
    if (!home || home[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_paths_default", "HOME is not set; cannot resolve default YVEX paths");
        return YVEX_ERR_STATE;
    }

    env = yvex_env_or_null("YVEX_CONFIG_DIR");
    rc = env ? yvex_copy_path(out->config_dir, env, err, "yvex_paths_default")
             : yvex_path_format(out->config_dir, YVEX_PATH_CAP, err, "yvex_paths_default", "%s/.config/yvex", home);
    if (rc != YVEX_OK) {
        return rc;
    }

    env = yvex_env_or_null("YVEX_CACHE_DIR");
    rc = env ? yvex_copy_path(out->cache_dir, env, err, "yvex_paths_default")
             : yvex_path_format(out->cache_dir, YVEX_PATH_CAP, err, "yvex_paths_default", "%s/.cache/yvex", home);
    if (rc != YVEX_OK) {
        return rc;
    }

    env = yvex_env_or_null("YVEX_STATE_DIR");
    rc = env ? yvex_copy_path(out->state_dir, env, err, "yvex_paths_default")
             : yvex_path_format(out->state_dir, YVEX_PATH_CAP, err, "yvex_paths_default", "%s/.local/state/yvex", home);
    if (rc != YVEX_OK) {
        return rc;
    }

    env = yvex_env_or_null("YVEX_DATA_DIR");
    rc = env ? yvex_copy_path(out->data_dir, env, err, "yvex_paths_default")
             : yvex_path_format(out->data_dir, YVEX_PATH_CAP, err, "yvex_paths_default", "%s/.local/share/yvex", home);
    if (rc != YVEX_OK) {
        return rc;
    }

    out->project_dir[0] = '\0';
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_paths_project(yvex_paths *out, const char *project_root, yvex_error *err)
{
    int rc;

    if (!out || !project_root || project_root[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_paths_project", "out and project_root are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));

    rc = yvex_path_format(out->project_dir, YVEX_PATH_CAP, err, "yvex_paths_project", "%s/.yvex", project_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->config_dir, YVEX_PATH_CAP, err, "yvex_paths_project", "%s", out->project_dir);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->cache_dir, YVEX_PATH_CAP, err, "yvex_paths_project", "%s/cache", out->project_dir);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->state_dir, YVEX_PATH_CAP, err, "yvex_paths_project", "%s/state", out->project_dir);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->data_dir, YVEX_PATH_CAP, err, "yvex_paths_project", "%s/data", out->project_dir);
    if (rc != YVEX_OK) {
        return rc;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_paths_print(const yvex_paths *paths, FILE *fp, yvex_error *err)
{
    if (!paths || !fp) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_paths_print", "paths and fp are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "config: %s\n", paths->config_dir);
    fprintf(fp, "cache: %s\n", paths->cache_dir);
    fprintf(fp, "state: %s\n", paths->state_dir);
    fprintf(fp, "data: %s\n", paths->data_dir);
    fprintf(fp, "project: %s\n", paths->project_dir);

    yvex_error_clear(err);
    return YVEX_OK;
}
