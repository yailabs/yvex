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

#include <yvex/artifact.h>

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

static int yvex_run_path_format(char *dst, size_t cap, yvex_error *err, const char *where, const char *fmt, ...)
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

static const char *yvex_run_env_or_null(const char *name)
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

    rc = yvex_run_path_format(tmp, sizeof(tmp), err, "yvex_run_dir_create", "%s", path);
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

    rc = yvex_run_path_format(out->run_id, sizeof(out->run_id), err, "yvex_run_dir_prepare", "%s", resolved_run_id);
    if (rc != YVEX_OK) {
        return rc;
    }

    env_run_dir = yvex_run_env_or_null("YVEX_RUN_DIR");
    if (env_run_dir) {
        rc = yvex_run_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s", env_run_dir);
    } else if (paths->project_dir[0] != '\0') {
        rc = yvex_run_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s/runs", paths->project_dir);
    } else {
        rc = yvex_run_path_format(run_root, sizeof(run_root), err, "yvex_run_dir_prepare", "%s/runs", paths->state_dir);
    }
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_run_path_format(out->root, sizeof(out->root), err, "yvex_run_dir_prepare", "%s/%s", run_root, out->run_id);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->command_path, sizeof(out->command_path), err, "yvex_run_dir_prepare", "%s/command.txt", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->stdout_path, sizeof(out->stdout_path), err, "yvex_run_dir_prepare", "%s/stdout.log", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->stderr_path, sizeof(out->stderr_path), err, "yvex_run_dir_prepare", "%s/stderr.log", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->metrics_path, sizeof(out->metrics_path), err, "yvex_run_dir_prepare", "%s/metrics.json", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->trace_path, sizeof(out->trace_path), err, "yvex_run_dir_prepare", "%s/trace.jsonl", out->root);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_run_path_format(out->receipt_path, sizeof(out->receipt_path), err, "yvex_run_dir_prepare", "%s/receipt.json", out->root);
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
