/*
 * yvex_fs.c - Runtime path resolution and run directories.
 *
 * This file owns local filesystem paths, run identifiers, and run directory
 * creation. It does not own model artifacts.
 */

#define _POSIX_C_SOURCE 200809L

#include <yvex/fs.h>
#include "yvex_console_private.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static int yvex_path_format(char *dst,
                            size_t cap,
                            yvex_error *err,
                            const char *where,
                            const char *fmt,
                            ...)
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

static int yvex_operator_config_path(const yvex_paths *paths,
                                     char *out,
                                     size_t cap,
                                     yvex_error *err)
{
    if (!paths || !out || cap == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "paths and output are required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (paths->project_dir[0] != '\0') {
        return yvex_path_format(out, cap, err, "operator_paths", "%s/operator-paths.conf", paths->project_dir);
    }
    return yvex_path_format(out, cap, err, "operator_paths", ".yvex/operator-paths.conf");
}

static int yvex_operator_config_dir(const yvex_paths *paths,
                                    char *out,
                                    size_t cap,
                                    yvex_error *err)
{
    if (!paths || !out || cap == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "paths and output are required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (paths->project_dir[0] != '\0') {
        return yvex_path_format(out, cap, err, "operator_paths", "%s", paths->project_dir);
    }
    return yvex_path_format(out, cap, err, "operator_paths", ".yvex");
}

static int yvex_path_has_newline(const char *value)
{
    return value && (strchr(value, '\n') || strchr(value, '\r'));
}

static int yvex_operator_normalize_root(const char *value,
                                        char *out,
                                        size_t cap,
                                        yvex_error *err,
                                        const char *where)
{
    const char *home;
    char cwd[YVEX_PATH_CAP];

    if (!value || value[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "models root is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_path_has_newline(value)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "models root must not contain a newline");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(value, "/") == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "models root must not be filesystem root");
        return YVEX_ERR_INVALID_ARG;
    }

    if (value[0] == '/') {
        return yvex_path_format(out, cap, err, where, "%s", value);
    }

    if (value[0] == '~' && value[1] == '/') {
        home = getenv("HOME");
        if (!home || home[0] == '\0') {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "HOME is required to expand ~/ paths");
            return YVEX_ERR_INVALID_ARG;
        }
        return yvex_path_format(out, cap, err, where, "%s/%s", home, value + 2);
    }

    if (!getcwd(cwd, sizeof(cwd))) {
        yvex_error_setf(err, YVEX_ERR_IO, where, "getcwd failed: %s", strerror(errno));
        return YVEX_ERR_IO;
    }
    return yvex_path_format(out, cap, err, where, "%s/%s", cwd, value);
}

static int yvex_operator_fill(yvex_operator_paths *out,
                              const char *source,
                              const char *models_root,
                              const char *config_path,
                              yvex_error *err)
{
    int rc;

    if (!out || !source || !models_root || !config_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "operator path fields are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    rc = yvex_path_format(out->models_root_source, sizeof(out->models_root_source), err,
                          "operator_paths", "%s", source);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->models_root, sizeof(out->models_root), err,
                          "operator_paths", "%s", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->hf_root, sizeof(out->hf_root), err,
                          "operator_paths", "%s/hf", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->gguf_root, sizeof(out->gguf_root), err,
                          "operator_paths", "%s/gguf", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->reports_root, sizeof(out->reports_root), err,
                          "operator_paths", "%s/reports", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->reference_root, sizeof(out->reference_root), err,
                          "operator_paths", "%s/reference", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_path_format(out->registry_root, sizeof(out->registry_root), err,
                          "operator_paths", "%s/registry", models_root);
    if (rc != YVEX_OK) {
        return rc;
    }
    return yvex_path_format(out->config_path, sizeof(out->config_path), err,
                            "operator_paths", "%s", config_path);
}

static int yvex_operator_read_config(const yvex_paths *paths,
                                     char *out_root,
                                     size_t cap,
                                     int *out_found,
                                     char *out_config_path,
                                     size_t config_cap,
                                     yvex_error *err)
{
    FILE *fp;
    char line[YVEX_PATH_CAP + 32];
    char *value;
    size_t len;
    int rc;

    if (!out_root || !out_found || !out_config_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "config outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }

    *out_found = 0;
    out_root[0] = '\0';
    rc = yvex_operator_config_path(paths, out_config_path, config_cap, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    fp = fopen(out_config_path, "r");
    if (!fp) {
        if (errno == ENOENT) {
            return YVEX_OK;
        }
        yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot read %s: %s",
                        out_config_path, strerror(errno));
        return YVEX_ERR_IO;
    }

    if (!fgets(line, sizeof(line), fp)) {
        if (ferror(fp)) {
            yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot read %s: %s",
                            out_config_path, strerror(errno));
            fclose(fp);
            return YVEX_ERR_IO;
        }
        fclose(fp);
        return YVEX_OK;
    }
    fclose(fp);

    value = line;
    if (strncmp(value, "models_root=", 12) != 0) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "operator_paths", "invalid operator path config: %s", out_config_path);
        return YVEX_ERR_FORMAT;
    }
    value += 12;
    len = strlen(value);
    while (len > 0 && (value[len - 1] == '\n' || value[len - 1] == '\r')) {
        value[--len] = '\0';
    }
    if (value[0] == '\0' || yvex_path_has_newline(value)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "operator_paths", "configured models root is invalid");
        return YVEX_ERR_FORMAT;
    }

    rc = yvex_path_format(out_root, cap, err, "operator_paths", "%s", value);
    if (rc != YVEX_OK) {
        return rc;
    }
    *out_found = 1;
    return YVEX_OK;
}

int yvex_operator_paths_resolve(const yvex_paths *paths,
                                const char *explicit_models_root,
                                yvex_operator_paths *out,
                                yvex_error *err)
{
    char config_path[YVEX_PATH_CAP];
    char configured[YVEX_PATH_CAP];
    char resolved[YVEX_PATH_CAP];
    char builtin[YVEX_PATH_CAP];
    const char *env;
    const char *home;
    int found;
    int rc;

    if (!paths || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "paths and output are required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_operator_config_path(paths, config_path, sizeof(config_path), err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (explicit_models_root) {
        rc = yvex_operator_normalize_root(explicit_models_root, resolved, sizeof(resolved), err, "operator_paths");
        if (rc != YVEX_OK) {
            return rc;
        }
        return yvex_operator_fill(out, "explicit", resolved, config_path, err);
    }

    rc = yvex_operator_read_config(paths, configured, sizeof(configured), &found,
                                   config_path, sizeof(config_path), err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (found) {
        return yvex_operator_fill(out, "configured", configured, config_path, err);
    }

    env = yvex_env_or_null("YVEX_MODELS_ROOT");
    if (env) {
        rc = yvex_operator_normalize_root(env, resolved, sizeof(resolved), err, "operator_paths");
        if (rc != YVEX_OK) {
            return rc;
        }
        return yvex_operator_fill(out, "environment", resolved, config_path, err);
    }

    home = getenv("HOME");
    if (home && home[0] != '\0') {
        rc = yvex_path_format(builtin, sizeof(builtin), err, "operator_paths", "%s/lab/models", home);
    } else {
        rc = yvex_operator_normalize_root("./models", builtin, sizeof(builtin), err, "operator_paths");
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    return yvex_operator_fill(out, "builtin", builtin, config_path, err);
}

int yvex_operator_paths_configure(const yvex_paths *paths,
                                  const char *models_root,
                                  int create_dirs,
                                  yvex_operator_paths *out,
                                  yvex_error *err)
{
    char config_dir[YVEX_PATH_CAP];
    FILE *fp;
    int rc;

    rc = yvex_operator_paths_resolve(paths, models_root, out, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_operator_config_dir(paths, config_dir, sizeof(config_dir), err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_mkdir_p(config_dir, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    fp = fopen(out->config_path, "w");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot write %s: %s",
                        out->config_path, strerror(errno));
        return YVEX_ERR_IO;
    }
    if (fprintf(fp, "models_root=%s\n", out->models_root) < 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot write %s: %s",
                        out->config_path, strerror(errno));
        fclose(fp);
        return YVEX_ERR_IO;
    }
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot close %s: %s",
                        out->config_path, strerror(errno));
        return YVEX_ERR_IO;
    }

    if (create_dirs) {
        rc = yvex_operator_paths_create(out, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    return YVEX_OK;
}

int yvex_operator_paths_reset(const yvex_paths *paths,
                              int *out_removed,
                              yvex_operator_paths *out,
                              yvex_error *err)
{
    char config_path[YVEX_PATH_CAP];
    int rc;

    if (!out_removed) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "removed output is required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_operator_config_path(paths, config_path, sizeof(config_path), err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (unlink(config_path) == 0) {
        *out_removed = 1;
    } else if (errno == ENOENT) {
        *out_removed = 0;
    } else {
        yvex_error_setf(err, YVEX_ERR_IO, "operator_paths", "cannot remove %s: %s",
                        config_path, strerror(errno));
        return YVEX_ERR_IO;
    }

    return yvex_operator_paths_resolve(paths, NULL, out, err);
}

int yvex_operator_paths_create(const yvex_operator_paths *operator_paths, yvex_error *err)
{
    const char *dirs[13];
    char family_dir[YVEX_PATH_CAP];
    char glm_dir[YVEX_PATH_CAP];
    char gguf_deepseek[YVEX_PATH_CAP];
    char gguf_glm[YVEX_PATH_CAP];
    char reports_deepseek[YVEX_PATH_CAP];
    char reports_glm[YVEX_PATH_CAP];
    char reference_deepseek[YVEX_PATH_CAP];
    char reference_glm[YVEX_PATH_CAP];
    int rc;
    int i;

    if (!operator_paths || operator_paths->models_root[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "operator paths are required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_path_format(family_dir, sizeof(family_dir), err, "operator_paths", "%s/deepseek", operator_paths->hf_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(glm_dir, sizeof(glm_dir), err, "operator_paths", "%s/glm", operator_paths->hf_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(gguf_deepseek, sizeof(gguf_deepseek), err, "operator_paths", "%s/deepseek", operator_paths->gguf_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(gguf_glm, sizeof(gguf_glm), err, "operator_paths", "%s/glm", operator_paths->gguf_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(reports_deepseek, sizeof(reports_deepseek), err, "operator_paths", "%s/deepseek", operator_paths->reports_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(reports_glm, sizeof(reports_glm), err, "operator_paths", "%s/glm", operator_paths->reports_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(reference_deepseek, sizeof(reference_deepseek), err, "operator_paths", "%s/deepseek", operator_paths->reference_root);
    if (rc != YVEX_OK) return rc;
    rc = yvex_path_format(reference_glm, sizeof(reference_glm), err, "operator_paths", "%s/glm", operator_paths->reference_root);
    if (rc != YVEX_OK) return rc;

    dirs[0] = operator_paths->models_root;
    dirs[1] = operator_paths->hf_root;
    dirs[2] = family_dir;
    dirs[3] = glm_dir;
    dirs[4] = operator_paths->gguf_root;
    dirs[5] = gguf_deepseek;
    dirs[6] = gguf_glm;
    dirs[7] = operator_paths->reports_root;
    dirs[8] = reports_deepseek;
    dirs[9] = reports_glm;
    dirs[10] = operator_paths->reference_root;
    dirs[11] = reference_deepseek;
    dirs[12] = reference_glm;

    for (i = 0; i < 13; ++i) {
        rc = yvex_mkdir_p(dirs[i], err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    return yvex_mkdir_p(operator_paths->registry_root, err);
}

int yvex_operator_paths_resolve_target(const yvex_operator_paths *operator_paths,
                                       const char *family,
                                       const char *kind,
                                       char *out,
                                       size_t cap,
                                       int *out_exists,
                                       yvex_error *err)
{
    struct stat st;
    int rc;

    if (!operator_paths || !family || !kind || !out || !out_exists) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "family, kind and outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(family, "deepseek") != 0 && strcmp(family, "glm") != 0) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator_paths", "unknown family: %s", family);
        return YVEX_ERR_INVALID_ARG;
    }

    if (strcmp(kind, "source") == 0) {
        rc = yvex_path_format(out, cap, err, "operator_paths", "%s/hf/%s/%s",
                              operator_paths->models_root,
                              family,
                              strcmp(family, "deepseek") == 0 ? "DeepSeek-V4-Flash" : "GLM-5.2");
    } else if (strcmp(kind, "gguf") == 0) {
        rc = yvex_path_format(out, cap, err, "operator_paths", "%s/gguf/%s", operator_paths->models_root, family);
    } else if (strcmp(kind, "reports") == 0) {
        rc = yvex_path_format(out, cap, err, "operator_paths", "%s/reports/%s", operator_paths->models_root, family);
    } else if (strcmp(kind, "reference") == 0) {
        rc = yvex_path_format(out, cap, err, "operator_paths", "%s/reference/%s", operator_paths->models_root, family);
    } else if (strcmp(kind, "registry") == 0) {
        rc = yvex_path_format(out, cap, err, "operator_paths", "%s", operator_paths->registry_root);
    } else {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator_paths", "unknown kind: %s", kind);
        return YVEX_ERR_INVALID_ARG;
    }
    if (rc != YVEX_OK) {
        return rc;
    }

    *out_exists = stat(out, &st) == 0 ? 1 : 0;
    return YVEX_OK;
}

int yvex_operator_paths_print(const yvex_operator_paths *operator_paths,
                              FILE *fp,
                              const char *status,
                              int created,
                              int include_created,
                              yvex_error *err)
{
    if (!operator_paths || !fp || !status) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths", "operator paths, status and file are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "status: %s\n", status);
    fprintf(fp, "models_root_source: %s\n", operator_paths->models_root_source);
    fprintf(fp, "models_root: %s\n", operator_paths->models_root);
    fprintf(fp, "hf_root: %s\n", operator_paths->hf_root);
    fprintf(fp, "gguf_root: %s\n", operator_paths->gguf_root);
    fprintf(fp, "reports_root: %s\n", operator_paths->reports_root);
    fprintf(fp, "reference_root: %s\n", operator_paths->reference_root);
    fprintf(fp, "registry_root: %s\n", operator_paths->registry_root);
    fprintf(fp, "operator_config_path: %s\n", operator_paths->config_path);
    if (include_created) {
        fprintf(fp, "created: %s\n", created ? "true" : "false");
    }
    yvex_error_clear(err);
    return YVEX_OK;
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

/* Domain-owned command surface moved out of yvex_runtime.c. */

typedef enum {
    YVEX_PATHS_OUTPUT_NORMAL = 0,
    YVEX_PATHS_OUTPUT_AUDIT
} yvex_paths_output_mode;

static int parse_paths_output_mode(const char *value, yvex_paths_output_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_PATHS_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_PATHS_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static int print_operator_paths_normal(const yvex_operator_paths *operator_paths,
                                       const char *status,
                                       yvex_error *err)
{
    if (!operator_paths || !status) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths",
                       "operator paths and status are required");
        return YVEX_ERR_INVALID_ARG;
    }

    printf("paths: %s\n", status);
    printf("models_root_source: %s\n", operator_paths->models_root_source);
    printf("models_root: %s\n", operator_paths->models_root);
    printf("hf_root: %s\n", operator_paths->hf_root);
    printf("gguf_root: %s\n", operator_paths->gguf_root);
    printf("reports_root: %s\n", operator_paths->reports_root);
    printf("registry_root: %s\n", operator_paths->registry_root);
    printf("hint: use --audit for project/cache/state paths\n");
    yvex_error_clear(err);
    return YVEX_OK;
}

static int command_paths(int argc, char **argv)
{
    const char *project_root = NULL;
    const char *models_root = NULL;
    const char *family = NULL;
    const char *kind = NULL;
    int want_run = 0;
    int want_create = 0;
    int want_reset = 0;
    yvex_paths_output_mode output_mode = YVEX_PATHS_OUTPUT_NORMAL;
    int i;
    int rc;
    int removed = 0;
    int exists = 0;
    char resolved_path[YVEX_PATH_CAP];
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_run_dir run;
    yvex_error err;

    yvex_error_clear(&err);

    i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--project") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --project requires a path\n");
                return 2;
            }
            project_root = argv[++i];
            ++i;
        } else {
            break;
        }
    }

    rc = project_root ? yvex_paths_project(&paths, project_root, &err) : yvex_paths_default(&paths, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (i < argc && strcmp(argv[i], "configure") == 0) {
        ++i;
        while (i < argc) {
            if (strcmp(argv[i], "--models-root") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "yvex: --models-root requires a path\n");
                    return 2;
                }
                models_root = argv[++i];
            } else if (strcmp(argv[i], "--create") == 0) {
                want_create = 1;
            } else if (strcmp(argv[i], "--reset") == 0) {
                want_reset = 1;
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                yvex_paths_help(stdout);
                return 0;
            } else {
                fprintf(stderr, "yvex: unknown paths configure option: %s\n", argv[i]);
                fprintf(stderr, "Try 'yvex help paths' for usage.\n");
                return 2;
            }
            ++i;
        }
        if (want_reset && (models_root || want_create)) {
            fprintf(stderr, "yvex: paths configure --reset does not accept --models-root or --create\n");
            return 2;
        }
        if (!want_reset && !models_root) {
            fprintf(stderr, "yvex: paths configure requires --models-root DIR or --reset\n");
            return 2;
        }

        if (want_reset) {
            rc = yvex_operator_paths_reset(&paths, &removed, &operator_paths, &err);
            if (rc != YVEX_OK) {
                return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
            }
            printf("status: paths-reset\n");
            printf("config_path: %s\n", operator_paths.config_path);
            printf("removed: %s\n", removed ? "true" : "false");
            printf("models_root_source: %s\n", operator_paths.models_root_source);
            printf("models_root: %s\n", operator_paths.models_root);
            return 0;
        }

        rc = yvex_operator_paths_configure(&paths, models_root, want_create, &operator_paths, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        printf("status: paths-configured\n");
        printf("models_root_source: %s\n", operator_paths.models_root_source);
        printf("models_root: %s\n", operator_paths.models_root);
        printf("hf_root: %s\n", operator_paths.hf_root);
        printf("gguf_root: %s\n", operator_paths.gguf_root);
        printf("reports_root: %s\n", operator_paths.reports_root);
        printf("reference_root: %s\n", operator_paths.reference_root);
        printf("registry_root: %s\n", operator_paths.registry_root);
        printf("config_path: %s\n", operator_paths.config_path);
        printf("created: %s\n", want_create ? "true" : "false");
        return 0;
    }

    if (i < argc && strcmp(argv[i], "resolve") == 0) {
        ++i;
        while (i < argc) {
            if (strcmp(argv[i], "--family") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "yvex: --family requires a value\n");
                    return 2;
                }
                family = argv[++i];
            } else if (strcmp(argv[i], "--kind") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "yvex: --kind requires a value\n");
                    return 2;
                }
                kind = argv[++i];
            } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
                yvex_paths_help(stdout);
                return 0;
            } else {
                fprintf(stderr, "yvex: unknown paths resolve option: %s\n", argv[i]);
                fprintf(stderr, "Try 'yvex help paths' for usage.\n");
                return 2;
            }
            ++i;
        }
        if (!family) {
            fprintf(stderr, "yvex: paths resolve requires --family\n");
            return 2;
        }
        if (!kind) {
            fprintf(stderr, "yvex: paths resolve requires --kind\n");
            return 2;
        }
        rc = yvex_operator_paths_resolve(&paths, NULL, &operator_paths, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        rc = yvex_operator_paths_resolve_target(&operator_paths, family, kind,
                                                resolved_path, sizeof(resolved_path), &exists, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        printf("status: paths-resolve\n");
        printf("models_root_source: %s\n", operator_paths.models_root_source);
        printf("family: %s\n", family);
        printf("kind: %s\n", kind);
        printf("path: %s\n", resolved_path);
        printf("exists: %s\n", exists ? "true" : "false");
        return 0;
    }

    for (; i < argc; ++i) {
        if (strcmp(argv[i], "--run") == 0) {
            want_run = 1;
        } else if (strcmp(argv[i], "--create") == 0) {
            want_create = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            output_mode = YVEX_PATHS_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex paths: --output requires normal|audit\n");
                return 2;
            }
            if (!parse_paths_output_mode(argv[++i], &output_mode)) {
                fprintf(stderr, "yvex paths: unsupported output mode: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            yvex_paths_help(stdout);
            return 0;
        } else {
            fprintf(stderr, "yvex: unknown paths option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help paths' for usage.\n");
            return 2;
        }
    }

    if (!want_run) {
        rc = yvex_operator_paths_resolve(&paths, NULL, &operator_paths, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        if (want_create) {
            rc = yvex_operator_paths_create(&operator_paths, &err);
            if (rc != YVEX_OK) {
                return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
            }
            if (output_mode == YVEX_PATHS_OUTPUT_AUDIT) {
                rc = yvex_paths_print(&paths, stdout, &err);
                if (rc != YVEX_OK) {
                    return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
                }
                return yvex_operator_paths_print(&operator_paths, stdout, "paths-created", 1, 1, &err);
            }
            return print_operator_paths_normal(&operator_paths, "created", &err);
        }
        if (output_mode == YVEX_PATHS_OUTPUT_AUDIT) {
            rc = yvex_paths_print(&paths, stdout, &err);
            if (rc != YVEX_OK) {
                return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
            }
            rc = yvex_operator_paths_print(&operator_paths, stdout, "paths", 0, 0, &err);
        } else {
            rc = print_operator_paths_normal(&operator_paths, "normal", &err);
        }
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        return 0;
    }

    rc = yvex_run_dir_prepare(&run, &paths, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (want_create) {
        rc = yvex_run_dir_create(&run, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
    }

    rc = yvex_run_dir_print(&run, stdout, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }
    return 0;
}

int yvex_paths_command(int argc, char **argv)
{
    return command_paths(argc, argv);
}

void yvex_paths_help(FILE *fp)
{
    fprintf(fp, "usage: yvex paths [--project DIR] [--create] [--audit | --output normal|audit]\n");
    fprintf(fp, "       yvex paths [--project DIR] --run [--create]\n");
    fprintf(fp, "       yvex paths [--project DIR] configure --models-root DIR [--create]\n");
    fprintf(fp, "       yvex paths [--project DIR] configure --reset\n");
    fprintf(fp, "       yvex paths [--project DIR] resolve --family deepseek|glm --kind source|gguf|reports|reference|registry\n\n");
    fprintf(fp, "Path configuration records operator-local storage only; it does not download models, create artifacts, register aliases, or claim runtime support.\n");
}
