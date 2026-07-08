/*
 * yvex_model_target_sidecar_write.c - model-target sidecar writer boundary.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   explicit local sidecar writer entry points.
 *
 * Does not own:
 *   CLI operator streams, command dispatch, rendering, runtime execution,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   sidecar writer APIs write explicit local files only and never process
 *   operator streams.
 *
 * Boundary:
 *   sidecar writer availability does not create artifact emission capability,
 *   runtime support, generation support, benchmark evidence, or release
 *   readiness.
 */
#include "yvex_model_target_sidecar_write.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int model_target_sidecar_writer_file_scope(void)
{
    return 1;
}

static int model_target_sidecar_writer_operator_scope(void)
{
    return 0;
}

static int sidecar_mkdir_parent(const char *path)
{
    char buf[1024];
    char *slash;
    char *p;

    if (!path || strlen(path) >= sizeof(buf)) return 0;
    strcpy(buf, path);
    slash = strrchr(buf, '/');
    if (!slash) return 1;
    *slash = '\0';
    if (!buf[0]) return 1;
    for (p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0775) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    return mkdir(buf, 0775) == 0 || errno == EEXIST;
}

static void sidecar_json_string(FILE *fp, const char *s)
{
    if (!s) s = "";
    fputc('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static int sidecar_open_tmp(const char *path, char *tmp, size_t tmp_cap, FILE **out)
{
    int n;

    if (!path || !tmp || tmp_cap == 0 || !out) return 0;
    *out = NULL;
    if (!sidecar_mkdir_parent(path)) return 0;
    n = snprintf(tmp, tmp_cap, "%s.tmp", path);
    if (n < 0 || (size_t)n >= tmp_cap) return 0;
    *out = fopen(tmp, "wb");
    return *out != NULL;
}

static int sidecar_close_tmp(FILE *fp, const char *tmp, const char *path)
{
    if (!fp || !tmp || !path) return 0;
    if (fclose(fp) != 0) {
        remove(tmp);
        return 0;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        return 0;
    }
    return 1;
}

int yvex_model_target_sidecar_writer_available(void)
{
    return model_target_sidecar_writer_file_scope() &&
           !model_target_sidecar_writer_operator_scope();
}

int yvex_model_target_write_tensor_map_sidecar(const char *path,
                                               const char *target_id,
                                               const char *family,
                                               const char *status,
                                               const char *coverage)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.source.tensor_map.v1\",\n");
    fprintf(fp, "  \"row\": \"MODELS.SOURCE.MAP.HANDOFF.0\",\n");
    fprintf(fp, "  \"target_id\": ");
    sidecar_json_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    sidecar_json_string(fp, family);
    fprintf(fp, ",\n  \"tensor_map_status\": ");
    sidecar_json_string(fp, status);
    fprintf(fp, ",\n  \"required_role_coverage_status\": ");
    sidecar_json_string(fp, coverage);
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}

int yvex_model_target_write_output_head_sidecar(const char *path,
                                                const char *target_id,
                                                const char *family,
                                                const char *status)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.source.output_head_map.v1\",\n");
    fprintf(fp, "  \"row\": \"MODELS.SOURCE.MAP.HANDOFF.0\",\n");
    fprintf(fp, "  \"target_id\": ");
    sidecar_json_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    sidecar_json_string(fp, family);
    fprintf(fp, ",\n  \"output_head_map_status\": ");
    sidecar_json_string(fp, status);
    fprintf(fp, ",\n  \"output_head_status\": ");
    sidecar_json_string(fp,
                        strcmp(status, "output-head-missing") == 0
                            ? "missing"
                            : "present");
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}

int yvex_model_target_write_tokenizer_sidecar(const char *path,
                                              const char *target_id,
                                              const char *family,
                                              const char *status)
{
    char tmp[1024];
    FILE *fp;

    if (!path || !path[0]) return 1;
    if (!sidecar_open_tmp(path, tmp, sizeof(tmp), &fp)) return 0;
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema_version\": \"yvex.source.tokenizer_map.v1\",\n");
    fprintf(fp, "  \"row\": \"V010.MAP.7\",\n");
    fprintf(fp, "  \"target_id\": ");
    sidecar_json_string(fp, target_id);
    fprintf(fp, ",\n  \"family\": ");
    sidecar_json_string(fp, family);
    fprintf(fp, ",\n  \"tokenizer_map_status\": ");
    sidecar_json_string(fp, status);
    fprintf(fp, ",\n  \"status\": ");
    sidecar_json_string(fp, status);
    fprintf(fp, "\n}\n");
    return sidecar_close_tmp(fp, tmp, path);
}
