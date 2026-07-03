/*
 * yvex_source.c - Source manifests, native inventory, and safetensors headers.
 *
 * This file owns source-file manifests and lightweight native-weight metadata.
 * It does not commit or materialize external model artifacts.
 */

#include "yvex_console_private.h"

#include <stddef.h>
#include <yvex/fs.h>
#include <yvex/source_manifest.h>
#include <yvex/native_weights.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>


typedef struct {
    char *path;
    unsigned long long size_bytes;
    const char *kind;
} yvex_source_manifest_file;

typedef struct {
    yvex_source_manifest_file *items;
    size_t count;
    size_t cap;
    yvex_source_manifest_summary summary;
} yvex_source_manifest_file_list;

void yvex_source_manifest_file_list_init(yvex_source_manifest_file_list *list);
void yvex_source_manifest_file_list_free(yvex_source_manifest_file_list *list);

int yvex_source_manifest_scan_files(const char *local_path,
                                    int include_files,
                                    yvex_source_manifest_file_list *out,
                                    yvex_error *err);

int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err);


/* Native weights */


struct yvex_native_weight_table {
    yvex_native_weight_info *items;
    unsigned long long count;
    unsigned long long cap;
    yvex_native_weight_summary summary;
};

int yvex_native_weight_table_add(yvex_native_weight_table *table,
                                 const char *name,
                                 const char *shard_path,
                                 const char *dtype_name,
                                 unsigned int rank,
                                 const unsigned long long *dims,
                                 unsigned long long data_start,
                                 unsigned long long data_end,
                                 yvex_error *err);

int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err);

int yvex_safetensors_parse_header(const char *json,
                                  unsigned long long payload_bytes,
                                  const char *shard_path,
                                  yvex_native_weight_table *table,
                                  yvex_error *err);

int yvex_native_weight_report_json(const char *source,
                                   const yvex_native_weight_table *table,
                                   yvex_error *err);





const char *yvex_source_status_name(yvex_source_status status)
{
    switch (status) {
    case YVEX_SOURCE_STATUS_UNKNOWN:
        return "unknown";
    case YVEX_SOURCE_STATUS_IN_PROGRESS:
        return "in-progress";
    case YVEX_SOURCE_STATUS_INCOMPLETE:
        return "incomplete";
    case YVEX_SOURCE_STATUS_COMPLETE:
        return "complete";
    case YVEX_SOURCE_STATUS_FAILED:
        return "failed";
    }
    return "unknown";
}

int yvex_source_manifest_scan_local(const char *local_path,
                                    yvex_source_manifest_summary *out,
                                    yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    if (!local_path || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_scan", "local_path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(local_path, 0, &files, err);
    if (rc == YVEX_OK) {
        *out = files.summary;
    }
    yvex_source_manifest_file_list_free(&files);
    return rc;
}

int yvex_source_manifest_write_json(const char *out_path,
                                    const yvex_source_manifest_options *options,
                                    yvex_source_manifest_summary *summary_out,
                                    yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    if (!out_path || !options || !options->repo || !options->revision ||
        !options->local_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_write", "out_path, repo, revision, and local_path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(options->local_path, options->include_files, &files, err);
    if (rc == YVEX_OK) {
        rc = yvex_source_manifest_write_json_file(out_path, options, &files, err);
    }
    if (rc == YVEX_OK && summary_out) {
        *summary_out = files.summary;
    }
    yvex_source_manifest_file_list_free(&files);
    return rc;
}



static void yvex_sm_json_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
        case '"':
            fputs("\\\"", fp);
            break;
        case '\\':
            fputs("\\\\", fp);
            break;
        case '\b':
            fputs("\\b", fp);
            break;
        case '\f':
            fputs("\\f", fp);
            break;
        case '\n':
            fputs("\\n", fp);
            break;
        case '\r':
            fputs("\\r", fp);
            break;
        case '\t':
            fputs("\\t", fp);
            break;
        default:
            if (ch < 32) {
                fprintf(fp, "\\u%04x", (unsigned int)ch);
            } else {
                fputc((int)ch, fp);
            }
            break;
        }
    }
    fputc('"', fp);
}

static void yvex_sm_json_field(FILE *fp, const char *name, const char *value, int comma)
{
    fprintf(fp, "    ");
    yvex_sm_json_string(fp, name);
    fputs(": ", fp);
    yvex_sm_json_string(fp, value);
    fputs(comma ? ",\n" : "\n", fp);
}

int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err)
{
    FILE *fp;
    size_t i;

    if (!out_path || !options || !files) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_json", "out_path, options, and files are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "cannot open output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }

    fprintf(fp, "{\n");
    yvex_sm_json_field(fp, "schema", "yvex.source_manifest.v1", 1);
    yvex_sm_json_field(fp, "status", yvex_source_status_name(options->status), 1);
    fprintf(fp, "  \"source\": {\n");
    yvex_sm_json_field(fp, "kind", "huggingface", 1);
    yvex_sm_json_field(fp, "repo", options->repo, 1);
    yvex_sm_json_field(fp, "revision", options->revision, 1);
    yvex_sm_json_field(fp, "license", options->license, 1);
    yvex_sm_json_field(fp, "model_card", options->model_card, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"local\": {\n");
    yvex_sm_json_field(fp, "path", options->local_path, 1);
    yvex_sm_json_field(fp, "node_role", "provider", 1);
    yvex_sm_json_field(fp, "node_name", options->node_name, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"download\": {\n");
    yvex_sm_json_field(fp, "command", options->download_command, 1);
    yvex_sm_json_field(fp, "dry_run_log", options->dry_run_log, 1);
    yvex_sm_json_field(fp, "download_log", options->download_log, 1);
    yvex_sm_json_field(fp, "pid_file", options->pid_file, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"files\": [\n");
    for (i = 0; i < files->count; ++i) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"path\": ");
        yvex_sm_json_string(fp, files->items[i].path);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"size_bytes\": %llu,\n", files->items[i].size_bytes);
        fprintf(fp, "      \"kind\": ");
        yvex_sm_json_string(fp, files->items[i].kind);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"sha256\": null\n");
        fprintf(fp, "    }%s\n", i + 1u == files->count ? "" : ",");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"summary\": {\n");
    fprintf(fp, "    \"file_count\": %llu,\n", files->summary.file_count);
    fprintf(fp, "    \"safetensors_count\": %llu,\n", files->summary.safetensors_count);
    fprintf(fp, "    \"total_size_bytes\": %llu,\n", files->summary.total_size_bytes);
    fprintf(fp, "    \"has_config\": %s,\n", files->summary.has_config ? "true" : "false");
    fprintf(fp, "    \"has_tokenizer\": %s,\n", files->summary.has_tokenizer ? "true" : "false");
    fprintf(fp, "    \"has_safetensors\": %s\n", files->summary.has_safetensors ? "true" : "false");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "failed closing output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

#define _XOPEN_SOURCE 700



static char *yvex_sm_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) {
        s = "";
    }
    len = strlen(s);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static int yvex_sm_ends_with(const char *s, const char *suffix)
{
    size_t slen;
    size_t tlen;

    if (!s || !suffix) {
        return 0;
    }
    slen = strlen(s);
    tlen = strlen(suffix);
    if (tlen > slen) {
        return 0;
    }
    return strcmp(s + slen - tlen, suffix) == 0;
}

static int yvex_sm_name_starts_ci(const char *s, const char *prefix)
{
    size_t i;

    if (!s || !prefix) {
        return 0;
    }
    for (i = 0; prefix[i] != '\0'; ++i) {
        if (s[i] == '\0') {
            return 0;
        }
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i])) {
            return 0;
        }
    }
    return 1;
}

static const char *yvex_sm_basename(const char *path)
{
    const char *slash;

    if (!path) {
        return "";
    }
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static const char *yvex_sm_kind_for_path(const char *rel_path)
{
    const char *base = yvex_sm_basename(rel_path);

    if (yvex_sm_ends_with(base, ".safetensors")) {
        return "safetensors";
    }
    if (strcmp(base, "config.json") == 0 || strcmp(base, "generation_config.json") == 0 ||
        yvex_sm_name_starts_ci(base, "config")) {
        return "config";
    }
    if (yvex_sm_name_starts_ci(base, "tokenizer") || yvex_sm_ends_with(base, ".model")) {
        return "tokenizer";
    }
    if (yvex_sm_name_starts_ci(base, "readme")) {
        return "readme";
    }
    if (yvex_sm_name_starts_ci(base, "license") || yvex_sm_name_starts_ci(base, "copying")) {
        return "license";
    }
    if (yvex_sm_ends_with(base, ".json") || yvex_sm_ends_with(base, ".txt") ||
        yvex_sm_ends_with(base, ".md")) {
        return "metadata";
    }
    return "other";
}

static char *yvex_sm_join2(const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    int need_slash = alen > 0 && a[alen - 1] != '/';
    char *out = (char *)malloc(alen + (need_slash ? 1u : 0u) + blen + 1u);

    if (!out) {
        return NULL;
    }
    memcpy(out, a, alen);
    if (need_slash) {
        out[alen] = '/';
        memcpy(out + alen + 1, b, blen + 1);
    } else {
        memcpy(out + alen, b, blen + 1);
    }
    return out;
}

static int yvex_sm_append_file(yvex_source_manifest_file_list *list,
                               const char *rel_path,
                               unsigned long long size_bytes,
                               yvex_error *err)
{
    yvex_source_manifest_file *next;
    const char *kind;
    size_t new_cap;

    if (list->count == list->cap) {
        new_cap = list->cap == 0 ? 16u : list->cap * 2u;
        next = (yvex_source_manifest_file *)realloc(list->items, new_cap * sizeof(list->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_manifest_scan", "file list allocation failed");
            return YVEX_ERR_NOMEM;
        }
        list->items = next;
        list->cap = new_cap;
    }

    kind = yvex_sm_kind_for_path(rel_path);
    list->items[list->count].path = yvex_sm_strdup(rel_path);
    if (!list->items[list->count].path) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_manifest_scan", "file path allocation failed");
        return YVEX_ERR_NOMEM;
    }
    list->items[list->count].size_bytes = size_bytes;
    list->items[list->count].kind = kind;
    list->count++;

    list->summary.file_count++;
    list->summary.total_size_bytes += size_bytes;
    if (strcmp(kind, "safetensors") == 0) {
        list->summary.safetensors_count++;
        list->summary.has_safetensors = 1;
    } else if (strcmp(kind, "config") == 0) {
        list->summary.has_config = 1;
    } else if (strcmp(kind, "tokenizer") == 0) {
        list->summary.has_tokenizer = 1;
    }
    return YVEX_OK;
}

static int yvex_sm_file_cmp(const void *a, const void *b)
{
    const yvex_source_manifest_file *fa = (const yvex_source_manifest_file *)a;
    const yvex_source_manifest_file *fb = (const yvex_source_manifest_file *)b;

    return strcmp(fa->path, fb->path);
}

static int yvex_sm_scan_dir(const char *root,
                            const char *rel_dir,
                            int include_files,
                            yvex_source_manifest_file_list *out,
                            yvex_error *err)
{
    char *abs_dir;
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    abs_dir = rel_dir && rel_dir[0] != '\0' ? yvex_sm_join2(root, rel_dir) : yvex_sm_strdup(root);
    if (!abs_dir) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_manifest_scan", "directory path allocation failed");
        return YVEX_ERR_NOMEM;
    }

    dir = opendir(abs_dir);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_scan", "cannot open directory: %s", abs_dir);
        free(abs_dir);
        return YVEX_ERR_IO;
    }

    while ((ent = readdir(dir)) != NULL) {
        char *rel_path;
        char *abs_path;
        struct stat st;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        rel_path = rel_dir && rel_dir[0] != '\0' ? yvex_sm_join2(rel_dir, ent->d_name) : yvex_sm_strdup(ent->d_name);
        if (!rel_path) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_manifest_scan", "relative path allocation failed");
            rc = YVEX_ERR_NOMEM;
            break;
        }
        abs_path = yvex_sm_join2(root, rel_path);
        if (!abs_path) {
            free(rel_path);
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_manifest_scan", "absolute path allocation failed");
            rc = YVEX_ERR_NOMEM;
            break;
        }

        if (lstat(abs_path, &st) != 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_scan", "cannot stat path: %s", abs_path);
            free(abs_path);
            free(rel_path);
            rc = YVEX_ERR_IO;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            rc = yvex_sm_scan_dir(root, rel_path, include_files, out, err);
        } else if (S_ISREG(st.st_mode)) {
            rc = yvex_sm_append_file(out, rel_path, (unsigned long long)st.st_size, err);
            if (!include_files && rc == YVEX_OK) {
                free(out->items[out->count - 1].path);
                out->items[out->count - 1].path = NULL;
                out->count--;
            }
        }

        free(abs_path);
        free(rel_path);
        if (rc != YVEX_OK) {
            break;
        }
    }

    closedir(dir);
    free(abs_dir);
    return rc;
}

void yvex_source_manifest_file_list_init(yvex_source_manifest_file_list *list)
{
    if (!list) {
        return;
    }
    memset(list, 0, sizeof(*list));
}

void yvex_source_manifest_file_list_free(yvex_source_manifest_file_list *list)
{
    size_t i;

    if (!list) {
        return;
    }
    for (i = 0; i < list->count; ++i) {
        free(list->items[i].path);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

int yvex_source_manifest_scan_files(const char *local_path,
                                    int include_files,
                                    yvex_source_manifest_file_list *out,
                                    yvex_error *err)
{
    struct stat st;
    int rc;

    if (!local_path || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_scan", "local_path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (lstat(local_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_scan", "local path is not a directory: %s", local_path);
        return YVEX_ERR_IO;
    }

    rc = yvex_sm_scan_dir(local_path, "", include_files, out, err);
    if (rc == YVEX_OK && include_files && out->count > 1) {
        qsort(out->items, out->count, sizeof(out->items[0]), yvex_sm_file_cmp);
    }
    return rc;
}

typedef enum {
    YVEX_SOURCE_REPORT_OUTPUT_NORMAL = 0,
    YVEX_SOURCE_REPORT_OUTPUT_TABLE,
    YVEX_SOURCE_REPORT_OUTPUT_AUDIT
} yvex_source_report_output_mode;

typedef struct {
    const char *family_key;
    const char *display_family;
    const char *report_name;
    const char *target_id;
    const char *target_class;
    const char *model;
    const char *source_target_status;
    const char *source_family_profile_status;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *pressure_purpose;
    const char *runtime_shape;
    const char *hardware_lane;
    const char *backend_lane;
    const char *source_class;
    const char *source_path_blocker;
    const char *source_manifest_blocker;
    const char *native_inventory_blocker;
    const char *source_config_blocker;
    const char *tokenizer_blocker;
    const char *model_class_blocker;
    const char *model_class_next;
    const char *const *tail_blockers;
    unsigned long tail_blocker_count;
} yvex_source_family_profile;

static const char *qwen_source_tail_blockers[] = {
    "missing-qwen-model-class-profile",
    "missing-qwen-tensor-map",
    "missing-qwen-tokenizer-map",
    "missing-qwen-output-head-map",
    "missing-qwen-yvex-artifact",
    "missing-qwen-artifact-identity",
    "missing-metal-hardware-profile",
    "missing-metal-backend-feasibility",
    "missing-unified-memory-residency-plan",
    "missing-real-prefill",
    "missing-real-kv-path",
    "missing-real-decode",
    "missing-real-output-head-logits",
    "missing-real-vocabulary-sampling",
    "missing-generation-loop-over-real-state",
    "missing-eval-path",
    "missing-benchmark-path",
};

static const char *gemma_source_tail_blockers[] = {
    "missing-gemma-model-class-profile",
    "missing-gemma-tensor-map",
    "missing-gemma-tokenizer-map",
    "missing-gemma-output-head-map",
    "missing-gemma-yvex-artifact",
    "missing-gemma-artifact-identity",
    "missing-gemma-real-prefill",
    "missing-gemma-real-kv-path",
    "missing-gemma-real-decode",
    "missing-gemma-real-output-head-logits",
    "missing-gemma-real-vocabulary-sampling",
    "missing-gemma-generation-loop-over-real-state",
    "missing-gemma-eval-path",
    "missing-gemma-benchmark-path",
};

static const yvex_source_family_profile source_family_profiles[] = {
    {
        "qwen",
        "Qwen",
        "qwen-source-pressure",
        "qwen-metal-portability",
        "metal-reduced-full-runtime-pressure",
        "pending-source-config",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "future-YVEX-produced-GGUF",
        "reduced-scale-portability-and-full-runtime-pressure",
        "dense-or-dense-like-candidate-pending-source-config",
        "apple-silicon-metal",
        "metal-planned",
        "pending source/config verification",
        "missing-qwen-source-path",
        "missing-qwen-source-manifest",
        "missing-qwen-native-inventory",
        "missing-qwen-source-config",
        "missing-qwen-tokenizer-files",
        "missing-qwen-model-class-profile",
        "MODEL.CLASS.QWEN.0",
        qwen_source_tail_blockers,
        sizeof(qwen_source_tail_blockers) / sizeof(qwen_source_tail_blockers[0]),
    },
    {
        "gemma",
        "Gemma",
        "gemma-source-pressure",
        "gemma-dense-portability",
        "reduced-dense-full-runtime-pressure",
        "dense-candidate-pending-source-config",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "future-YVEX-produced-GGUF",
        "dense-candidate-pending-source-config-pressure",
        "dense-candidate-pending-source-config",
        "hardware-lane-unselected",
        "backend-lane-unselected",
        "pending source/config verification",
        "missing-gemma-source-path",
        "missing-gemma-source-manifest",
        "missing-gemma-native-inventory",
        "missing-gemma-source-config",
        "missing-gemma-tokenizer-files",
        "missing-gemma-model-class-profile",
        "MODEL.CLASS.GEMMA.0",
        gemma_source_tail_blockers,
        sizeof(gemma_source_tail_blockers) / sizeof(gemma_source_tail_blockers[0]),
    },
};

static const unsigned long source_family_profile_count =
    sizeof(source_family_profiles) / sizeof(source_family_profiles[0]);

typedef struct {
    const char *family;
    const char *release;
    const char *models_root;
    const char *source;
    const char *target;
    const yvex_source_family_profile *profile;
    int include_files;
    int include_config;
    int include_blockers;
    int include_next;
    yvex_source_report_output_mode output_mode;
} yvex_qwen_source_report_options;

typedef struct {
    const char *status;
    const char *source_state;
    const char *top_blocker;
    const char *next_row;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[64];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    int source_exists;
    int config_exists;
    int generation_config_exists;
    int tokenizer_json_exists;
    int tokenizer_config_exists;
    int manifest_exists;
    int native_inventory_exists;
    unsigned long long safetensors_count;
    const char *blockers[32];
    unsigned long blocker_count;
} yvex_qwen_source_pressure_report;

static int qwen_source_output_mode_parse(const char *value,
                                         yvex_source_report_output_mode *mode)
{
    if (!value || !mode) {
        return 0;
    }
    if (strcmp(value, "normal") == 0) {
        *mode = YVEX_SOURCE_REPORT_OUTPUT_NORMAL;
        return 1;
    }
    if (strcmp(value, "table") == 0) {
        *mode = YVEX_SOURCE_REPORT_OUTPUT_TABLE;
        return 1;
    }
    if (strcmp(value, "audit") == 0) {
        *mode = YVEX_SOURCE_REPORT_OUTPUT_AUDIT;
        return 1;
    }
    return 0;
}

static const yvex_source_family_profile *qwen_source_find_profile(const char *family)
{
    unsigned long i;

    if (!family) {
        return NULL;
    }
    for (i = 0; i < source_family_profile_count; ++i) {
        if (strcmp(source_family_profiles[i].family_key, family) == 0) {
            return &source_family_profiles[i];
        }
    }
    return NULL;
}

static int qwen_source_target_is_supported(const yvex_source_family_profile *profile,
                                           const char *target)
{
    if (!profile || !target) {
        return 0;
    }
    if (strcmp(profile->family_key, "qwen") == 0) {
        return strcmp(target, "qwen-metal-portability") == 0 ||
               strcmp(target, "qwen-small") == 0 ||
               strcmp(target, "qwen-medium") == 0;
    }
    return strcmp(target, profile->target_id) == 0;
}

static int qwen_source_path_format(char *out, size_t cap, const char *fmt,
                                   const char *a, const char *b)
{
    int n;

    if (!out || cap == 0 || !fmt) {
        return 0;
    }
    if (b) {
        n = snprintf(out, cap, fmt, a ? a : "", b);
    } else {
        n = snprintf(out, cap, fmt, a ? a : "");
    }
    if (n < 0 || (size_t)n >= cap) {
        out[cap - 1] = '\0';
        return 0;
    }
    return 1;
}

static int qwen_source_stat_kind(const char *path, int want_dir)
{
    struct stat st;

    if (!path || path[0] == '\0') {
        return 0;
    }
    if (stat(path, &st) != 0) {
        return 0;
    }
    return want_dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}

static int qwen_source_check_file(const char *dir, const char *name)
{
    char path[YVEX_PATH_CAP];

    if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, name)) {
        return 0;
    }
    return qwen_source_stat_kind(path, 0);
}

static unsigned long long qwen_source_count_top_safetensors(const char *dir)
{
    DIR *dp;
    struct dirent *ent;
    unsigned long long count = 0;

    if (!dir || dir[0] == '\0') {
        return 0;
    }
    dp = opendir(dir);
    if (!dp) {
        return 0;
    }
    while ((ent = readdir(dp)) != NULL) {
        char path[YVEX_PATH_CAP];

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!yvex_sm_ends_with(ent->d_name, ".safetensors")) {
            continue;
        }
        if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, ent->d_name)) {
            continue;
        }
        if (qwen_source_stat_kind(path, 0)) {
            count++;
        }
    }
    closedir(dp);
    return count;
}

static void qwen_source_choose_report_file(char *out, size_t cap,
                                           const yvex_source_family_profile *profile,
                                           const char *reports_root,
                                           const char *source_path,
                                           const char *target,
                                           const char *kind,
                                           int *out_exists)
{
    char candidate[YVEX_PATH_CAP];
    char file_name[96];
    int n;

    if (out_exists) {
        *out_exists = 0;
    }
    if (!out || cap == 0 || !profile || !kind) {
        return;
    }
    out[0] = '\0';

    if (source_path && source_path[0] != '\0') {
        if (qwen_source_path_format(candidate, sizeof(candidate), "%s/%s",
                                    source_path,
                                    strcmp(kind, "manifest") == 0
                                        ? "source-manifest.json"
                                        : "native-inventory.json") &&
            qwen_source_stat_kind(candidate, 0)) {
            snprintf(out, cap, "%s", candidate);
            if (out_exists) *out_exists = 1;
            return;
        }
    }

    if (reports_root && reports_root[0] != '\0') {
        n = snprintf(file_name, sizeof(file_name), "%s-%s",
                     profile->family_key,
                     strcmp(kind, "manifest") == 0
                         ? "source-manifest.json"
                         : "native-inventory.json");
        if (n < 0 || (size_t)n >= sizeof(file_name)) {
            return;
        }
        n = snprintf(candidate, sizeof(candidate), "%s/%s/%s",
                     reports_root, profile->family_key, file_name);
        if (n >= 0 && (size_t)n < sizeof(candidate) &&
            qwen_source_stat_kind(candidate, 0)) {
            snprintf(out, cap, "%s", candidate);
            if (out_exists) *out_exists = 1;
            return;
        }
        {
            char target_prefix[YVEX_PATH_CAP];

            n = snprintf(target_prefix, sizeof(target_prefix), "%s/%s/%s",
                         reports_root, profile->family_key, target);
            if (n >= 0 && (size_t)n < sizeof(target_prefix) &&
                qwen_source_path_format(candidate, sizeof(candidate), "%s-%s",
                                        target_prefix,
                                        strcmp(kind, "manifest") == 0
                                            ? "source-manifest.json"
                                            : "native-inventory.json") &&
                qwen_source_stat_kind(candidate, 0)) {
                snprintf(out, cap, "%s", candidate);
                if (out_exists) *out_exists = 1;
                return;
            }
        }
        n = snprintf(out, cap, "%s/%s/%s", reports_root, profile->family_key, file_name);
        if (n < 0 || (size_t)n >= cap) {
            out[cap - 1] = '\0';
        }
    }
}

static void qwen_source_add_blocker(yvex_qwen_source_pressure_report *report,
                                    const char *blocker)
{
    if (!report || !blocker || report->blocker_count >= 32) {
        return;
    }
    report->blockers[report->blocker_count++] = blocker;
}

static const char *qwen_source_present_missing(int present)
{
    return present ? "present" : "missing";
}

static const char *qwen_source_tokenizer_status(const yvex_qwen_source_pressure_report *report)
{
    return report && (report->tokenizer_json_exists || report->tokenizer_config_exists)
               ? "present"
               : "missing";
}

static const char *qwen_source_safetensors_status(const yvex_qwen_source_pressure_report *report)
{
    return report && report->safetensors_count > 0 ? "present" : "missing";
}

static const char *qwen_source_manifest_status(const yvex_qwen_source_pressure_report *report)
{
    return report && report->manifest_exists ? "present" : "missing";
}

static const char *qwen_source_native_inventory_status(const yvex_qwen_source_pressure_report *report)
{
    return report && report->native_inventory_exists ? "available-report-only" : "missing";
}

static const char *qwen_source_native_inventory_table_status(const yvex_qwen_source_pressure_report *report)
{
    return report && report->native_inventory_exists ? "present" : "missing";
}

static int qwen_source_build_report(const yvex_qwen_source_report_options *options,
                                    yvex_qwen_source_pressure_report *report)
{
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_error err;
    int rc;
    int n;
    unsigned long i;

    if (!options || !report || !options->profile) {
        return 3;
    }

    memset(report, 0, sizeof(*report));
    yvex_error_clear(&err);
    rc = yvex_paths_default(&paths, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }
    rc = yvex_operator_paths_resolve(&paths, options->models_root, &operator_paths, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return rc == YVEX_ERR_INVALID_ARG ? 2 : 3;
    }

    if (options->source) {
        if (!qwen_source_path_format(report->source_path, sizeof(report->source_path),
                                     "%s", options->source, NULL)) {
            fprintf(stderr, "source-manifest report: source path is too long\n");
            return 2;
        }
        snprintf(report->source_path_source, sizeof(report->source_path_source),
                 "%s", "explicit-source");
    } else {
        n = snprintf(report->source_path, sizeof(report->source_path),
                     "%s/hf/%s/%s",
                     operator_paths.models_root,
                     options->profile->family_key,
                     options->target);
        if (n < 0 || (size_t)n >= sizeof(report->source_path)) {
            fprintf(stderr, "source-manifest report: source path is too long\n");
            return 2;
        }
        snprintf(report->source_path_source, sizeof(report->source_path_source),
                 "%s", operator_paths.models_root_source);
    }

    report->source_exists = qwen_source_stat_kind(report->source_path, 1);
    if (report->source_exists) {
        report->config_exists = qwen_source_check_file(report->source_path, "config.json");
        report->generation_config_exists =
            qwen_source_check_file(report->source_path, "generation_config.json");
        report->tokenizer_json_exists =
            qwen_source_check_file(report->source_path, "tokenizer.json");
        report->tokenizer_config_exists =
            qwen_source_check_file(report->source_path, "tokenizer_config.json");
        report->safetensors_count =
            qwen_source_count_top_safetensors(report->source_path);
    }

    qwen_source_choose_report_file(report->manifest_path, sizeof(report->manifest_path),
                                   options->profile,
                                   operator_paths.reports_root,
                                   report->source_path,
                                   options->target,
                                   "manifest",
                                   &report->manifest_exists);
    qwen_source_choose_report_file(report->native_inventory_path,
                                   sizeof(report->native_inventory_path),
                                   options->profile,
                                   operator_paths.reports_root,
                                   report->source_path,
                                   options->target,
                                   "inventory",
                                   &report->native_inventory_exists);

    report->source_state = report->source_exists ? "present" : "missing";
    if (!report->source_exists) {
        report->status = "source-target-profiled";
        report->top_blocker = options->profile->source_path_blocker;
        report->next_row = "V010.SOURCE.2";
    } else if (!report->manifest_exists) {
        report->status = "source-present-report-only";
        report->top_blocker = options->profile->source_manifest_blocker;
        report->next_row = "V010.SOURCE.2";
    } else if (!report->native_inventory_exists) {
        report->status = "source-present-report-only";
        report->top_blocker = options->profile->native_inventory_blocker;
        report->next_row = "V010.SOURCE.5";
    } else {
        report->status = "source-profile-incomplete";
        report->top_blocker = options->profile->model_class_blocker;
        report->next_row = options->profile->model_class_next;
    }

    if (!report->source_exists) {
        qwen_source_add_blocker(report, options->profile->source_path_blocker);
    }
    if (!report->manifest_exists) {
        qwen_source_add_blocker(report, options->profile->source_manifest_blocker);
    }
    if (!report->native_inventory_exists) {
        qwen_source_add_blocker(report, options->profile->native_inventory_blocker);
    }
    if (!report->config_exists) {
        qwen_source_add_blocker(report, options->profile->source_config_blocker);
    }
    if (!(report->tokenizer_json_exists || report->tokenizer_config_exists)) {
        qwen_source_add_blocker(report, options->profile->tokenizer_blocker);
    }
    for (i = 0; i < options->profile->tail_blocker_count; ++i) {
        qwen_source_add_blocker(report, options->profile->tail_blockers[i]);
    }
    return 0;
}

static void qwen_source_print_normal(const yvex_qwen_source_report_options *options,
                                     const yvex_qwen_source_pressure_report *report)
{
    printf("report: %s\n", options->profile->report_name);
    printf("status: %s\n", report->status);
    printf("family: %s\n", options->profile->family_key);
    printf("target: %s\n", options->target);
    printf("source: %s\n", report->source_state);
    printf("top_blocker: %s\n", report->top_blocker);
    printf("next: %s\n", report->next_row);
    printf("boundary: source report only; no artifact/runtime/generation/benchmark\n");
}

static void qwen_source_print_table(const yvex_qwen_source_report_options *options,
                                    const yvex_qwen_source_pressure_report *report)
{
    printf("SOURCE PRESSURE  release=%s\n\n", options->release);
    printf("%-6s  %-24s  %-7s  %-8s  %-9s  %s\n",
           "FAMILY", "TARGET", "SOURCE", "MANIFEST", "INVENTORY", "NEXT");
    printf("%-6s  %-24s  %-7s  %-8s  %-9s  %s\n",
           options->profile->family_key,
           options->target,
           report->source_state,
           qwen_source_manifest_status(report),
           qwen_source_native_inventory_table_status(report),
           report->next_row);
}

static void qwen_source_print_audit(const yvex_qwen_source_report_options *options,
                                    const yvex_qwen_source_pressure_report *report)
{
    unsigned long i;

    printf("source-report: %s\n", options->profile->family_key);
    printf("status: %s\n", report->status);
    printf("release: %s\n", options->release);
    printf("family: %s\n", options->profile->display_family);
    printf("family_key: %s\n", options->profile->family_key);
    printf("model: %s\n", options->profile->model);
    printf("target_id: %s\n", options->target);
    printf("target_class: %s\n", options->profile->target_class);
    printf("source_target_status: %s\n", options->profile->source_target_status);
    printf("source_family_profile_status: %s\n",
           options->profile->source_family_profile_status);
    printf("source_artifact_class: %s\n", options->profile->source_artifact_class);
    printf("target_artifact_class: %s\n", options->profile->target_artifact_class);
    printf("pressure_purpose: %s\n", options->profile->pressure_purpose);
    printf("runtime_shape: %s\n", options->profile->runtime_shape);
    printf("hardware_lane: %s\n", options->profile->hardware_lane);
    printf("backend_lane: %s\n", options->profile->backend_lane);
    printf("source_class: %s\n", options->profile->source_class);
    printf("source_path: %s\n", report->source_path);
    printf("source_path_source: %s\n", report->source_path_source);
    printf("source_path_status: %s\n", report->source_state);
    printf("source_exists: %s\n", report->source_exists ? "true" : "false");
    printf("config_status: %s\n", qwen_source_present_missing(report->config_exists));
    printf("tokenizer_status: %s\n", qwen_source_tokenizer_status(report));
    printf("generation_config_status: %s\n",
           qwen_source_present_missing(report->generation_config_exists));
    printf("safetensors_status: %s\n", qwen_source_safetensors_status(report));
    printf("safetensors_count: %llu\n", report->safetensors_count);
    printf("source_manifest_status: %s\n", qwen_source_manifest_status(report));
    printf("source_manifest_path: %s\n",
           report->manifest_path[0] ? report->manifest_path : "unknown");
    printf("native_inventory_status: %s\n",
           qwen_source_native_inventory_status(report));
    printf("native_inventory_path: %s\n",
           report->native_inventory_path[0] ? report->native_inventory_path : "unknown");
    printf("model_class_profile_status: missing\n");
    printf("tensor_map_status: missing\n");
    printf("artifact_status: missing\n");
    printf("runtime_claim: unsupported\n");
    printf("generation: unsupported-full-model\n");
    printf("benchmark_status: not-measured\n");
    printf("release_ready: false\n");
    printf("blocker_count: %lu\n", report->blocker_count);
    for (i = 0; i < report->blocker_count; ++i) {
        printf("blocker_%lu: %s\n", i, report->blockers[i]);
    }
    printf("next_required_rows: %s\n", report->next_row);
    printf("boundary: source report only; no artifact/runtime/generation/benchmark\n");
}

static void qwen_source_report_usage(FILE *fp)
{
    fprintf(fp, "usage: yvex source-manifest report --family qwen|gemma --release v0.1.0 [options]\n");
}

static void qwen_source_report_help(FILE *fp)
{
    qwen_source_report_usage(fp);
    fprintf(fp, "\nOptions:\n");
    fprintf(fp, "  --family qwen|gemma\n");
    fprintf(fp, "  --release v0.1.0\n");
    fprintf(fp, "  --models-root DIR\n");
    fprintf(fp, "  --source DIR\n");
    fprintf(fp, "  --target qwen-metal-portability|qwen-small|qwen-medium|gemma-dense-portability\n");
    fprintf(fp, "  --include-files --include-config --include-blockers --include-next\n");
    fprintf(fp, "  --audit | --output normal|table|audit\n\n");
    fprintf(fp, "The source pressure report inspects source-path readiness only. It does not download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

int yvex_source_manifest_report_command(int argc, char **argv)
{
    yvex_qwen_source_report_options options;
    yvex_qwen_source_pressure_report report;
    int i;
    int rc;

    memset(&options, 0, sizeof(options));
    options.output_mode = YVEX_SOURCE_REPORT_OUTPUT_NORMAL;

    if (argc == 4 && (strcmp(argv[3], "--help") == 0 || strcmp(argv[3], "-h") == 0)) {
        qwen_source_report_help(stdout);
        return 0;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            qwen_source_report_help(stdout);
            return 0;
        } else if (strcmp(argv[i], "--family") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --family requires qwen|gemma\n");
                return 2;
            }
            options.family = argv[++i];
        } else if (strcmp(argv[i], "--release") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --release requires VERSION\n");
                return 2;
            }
            options.release = argv[++i];
        } else if (strcmp(argv[i], "--models-root") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --models-root requires DIR\n");
                return 2;
            }
            options.models_root = argv[++i];
        } else if (strcmp(argv[i], "--source") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --source requires DIR\n");
                return 2;
            }
            options.source = argv[++i];
        } else if (strcmp(argv[i], "--target") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --target requires TARGET\n");
                return 2;
            }
            options.target = argv[++i];
        } else if (strcmp(argv[i], "--include-files") == 0) {
            options.include_files = 1;
        } else if (strcmp(argv[i], "--include-config") == 0) {
            options.include_config = 1;
        } else if (strcmp(argv[i], "--include-blockers") == 0) {
            options.include_blockers = 1;
        } else if (strcmp(argv[i], "--include-next") == 0) {
            options.include_next = 1;
        } else if (strcmp(argv[i], "--audit") == 0) {
            options.output_mode = YVEX_SOURCE_REPORT_OUTPUT_AUDIT;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "source-manifest report: --output requires normal|table|audit\n");
                return 2;
            }
            if (!qwen_source_output_mode_parse(argv[++i], &options.output_mode)) {
                fprintf(stderr, "source-manifest report: unsupported output mode: %s\n", argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "--json") == 0) {
            fprintf(stderr, "source-manifest report: JSON output is unsupported; use --output normal|table|audit\n");
            return 2;
        } else {
            fprintf(stderr, "source-manifest report: unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    if (!options.family || options.family[0] == '\0') {
        fprintf(stderr, "source-manifest report: --family is required\n");
        qwen_source_report_usage(stderr);
        return 2;
    }
    options.profile = qwen_source_find_profile(options.family);
    if (!options.profile) {
        fprintf(stderr, "source-manifest report: unsupported family: %s\n", options.family);
        return 2;
    }
    if (!options.target) {
        options.target = options.profile->target_id;
    }
    if (!options.release || options.release[0] == '\0') {
        fprintf(stderr, "source-manifest report: --release is required\n");
        qwen_source_report_usage(stderr);
        return 2;
    }
    if (strcmp(options.release, "v0.1.0") != 0) {
        fprintf(stderr, "source-manifest report: unsupported release: %s\n", options.release);
        return 2;
    }
    if (!qwen_source_target_is_supported(options.profile, options.target)) {
        fprintf(stderr, "source-manifest report: unsupported target: %s\n", options.target);
        return 2;
    }

    rc = qwen_source_build_report(&options, &report);
    if (rc != 0) {
        return rc;
    }

    if (options.output_mode == YVEX_SOURCE_REPORT_OUTPUT_TABLE) {
        qwen_source_print_table(&options, &report);
    } else if (options.output_mode == YVEX_SOURCE_REPORT_OUTPUT_AUDIT) {
        qwen_source_print_audit(&options, &report);
    } else {
        qwen_source_print_normal(&options, &report);
    }
    return 0;
}

#define _XOPEN_SOURCE 700



static char *nw_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) {
        s = "";
    }
    len = strlen(s);
    out = (char *)malloc(len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, s, len + 1);
    return out;
}

static int nw_ends_with(const char *s, const char *suffix)
{
    size_t slen;
    size_t tlen;

    if (!s || !suffix) {
        return 0;
    }
    slen = strlen(s);
    tlen = strlen(suffix);
    return tlen <= slen && strcmp(s + slen - tlen, suffix) == 0;
}

static char *nw_join2(const char *a, const char *b)
{
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    int slash = alen > 0 && a[alen - 1] != '/';
    char *out = (char *)malloc(alen + (slash ? 1u : 0u) + blen + 1u);

    if (!out) {
        return NULL;
    }
    memcpy(out, a, alen);
    if (slash) {
        out[alen] = '/';
        memcpy(out + alen + 1, b, blen + 1);
    } else {
        memcpy(out + alen, b, blen + 1);
    }
    return out;
}

const char *yvex_native_dtype_name(yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_UNKNOWN:
        return "UNKNOWN";
    case YVEX_NATIVE_DTYPE_F64:
        return "F64";
    case YVEX_NATIVE_DTYPE_F32:
        return "F32";
    case YVEX_NATIVE_DTYPE_F16:
        return "F16";
    case YVEX_NATIVE_DTYPE_BF16:
        return "BF16";
    case YVEX_NATIVE_DTYPE_I64:
        return "I64";
    case YVEX_NATIVE_DTYPE_I32:
        return "I32";
    case YVEX_NATIVE_DTYPE_I16:
        return "I16";
    case YVEX_NATIVE_DTYPE_I8:
        return "I8";
    case YVEX_NATIVE_DTYPE_U8:
        return "U8";
    case YVEX_NATIVE_DTYPE_BOOL:
        return "BOOL";
    case YVEX_NATIVE_DTYPE_F8_E4M3:
        return "F8_E4M3";
    case YVEX_NATIVE_DTYPE_F8_E5M2:
        return "F8_E5M2";
    case YVEX_NATIVE_DTYPE_FP4:
        return "FP4";
    case YVEX_NATIVE_DTYPE_OTHER:
        return "OTHER";
    }
    return "UNKNOWN";
}

static yvex_native_dtype nw_dtype_from_name(const char *name)
{
    if (!name) {
        return YVEX_NATIVE_DTYPE_UNKNOWN;
    }
    if (strcmp(name, "F64") == 0) return YVEX_NATIVE_DTYPE_F64;
    if (strcmp(name, "F32") == 0) return YVEX_NATIVE_DTYPE_F32;
    if (strcmp(name, "F16") == 0) return YVEX_NATIVE_DTYPE_F16;
    if (strcmp(name, "BF16") == 0) return YVEX_NATIVE_DTYPE_BF16;
    if (strcmp(name, "I64") == 0) return YVEX_NATIVE_DTYPE_I64;
    if (strcmp(name, "I32") == 0) return YVEX_NATIVE_DTYPE_I32;
    if (strcmp(name, "I16") == 0) return YVEX_NATIVE_DTYPE_I16;
    if (strcmp(name, "I8") == 0) return YVEX_NATIVE_DTYPE_I8;
    if (strcmp(name, "U8") == 0) return YVEX_NATIVE_DTYPE_U8;
    if (strcmp(name, "BOOL") == 0) return YVEX_NATIVE_DTYPE_BOOL;
    if (strcmp(name, "F8_E4M3") == 0 || strcmp(name, "F8_E4M3FN") == 0) return YVEX_NATIVE_DTYPE_F8_E4M3;
    if (strcmp(name, "F8_E5M2") == 0) return YVEX_NATIVE_DTYPE_F8_E5M2;
    if (strcmp(name, "FP4") == 0 || strcmp(name, "F4") == 0) return YVEX_NATIVE_DTYPE_FP4;
    return YVEX_NATIVE_DTYPE_OTHER;
}

int yvex_native_weight_table_add(yvex_native_weight_table *table,
                                 const char *name,
                                 const char *shard_path,
                                 const char *dtype_name,
                                 unsigned int rank,
                                 const unsigned long long *dims,
                                 unsigned long long data_start,
                                 unsigned long long data_end,
                                 yvex_error *err)
{
    yvex_native_weight_info *next;
    yvex_native_weight_info *row;
    unsigned int i;

    if (!table || !name || !shard_path || !dtype_name || rank > YVEX_NATIVE_WEIGHT_MAX_DIMS ||
        data_end < data_start) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "native_weight_add", "invalid native tensor row");
        return YVEX_ERR_INVALID_ARG;
    }
    if (yvex_native_weight_table_find(table, name)) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "native_weight_add", "duplicate tensor name: %s", name);
        return YVEX_ERR_FORMAT;
    }
    if (table->count == table->cap) {
        unsigned long long cap = table->cap == 0 ? 64u : table->cap * 2u;
        next = (yvex_native_weight_info *)realloc(table->items, (size_t)cap * sizeof(table->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "native_weight_add", "native tensor table allocation failed");
            return YVEX_ERR_NOMEM;
        }
        table->items = next;
        table->cap = cap;
    }
    row = &table->items[table->count];
    memset(row, 0, sizeof(*row));
    row->name = nw_strdup(name);
    row->shard_path = nw_strdup(shard_path);
    row->dtype_name = nw_strdup(dtype_name);
    if (!row->name || !row->shard_path || !row->dtype_name) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "native_weight_add", "native tensor row allocation failed");
        free((char *)row->name);
        free((char *)row->shard_path);
        free((char *)row->dtype_name);
        memset(row, 0, sizeof(*row));
        return YVEX_ERR_NOMEM;
    }
    row->dtype = nw_dtype_from_name(dtype_name);
    row->rank = rank;
    for (i = 0; i < rank; ++i) {
        row->dims[i] = dims[i];
    }
    row->data_start = data_start;
    row->data_end = data_end;
    row->data_bytes = data_end - data_start;
    table->count++;
    table->summary.tensor_count++;
    table->summary.total_tensor_bytes += row->data_bytes;
    if (row->dtype == YVEX_NATIVE_DTYPE_UNKNOWN || row->dtype == YVEX_NATIVE_DTYPE_OTHER) {
        table->summary.unknown_dtype_count++;
    }
    return YVEX_OK;
}

static int nw_scan_dir(const char *root, const char *rel_dir, int recursive,
                       yvex_native_weight_table *table, yvex_error *err)
{
    char *abs_dir = rel_dir && rel_dir[0] ? nw_join2(root, rel_dir) : nw_strdup(root);
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    if (!abs_dir) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "native_weights", "directory path allocation failed");
        return YVEX_ERR_NOMEM;
    }
    dir = opendir(abs_dir);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "native_weights", "cannot open source directory: %s", abs_dir);
        free(abs_dir);
        return YVEX_ERR_IO;
    }
    while ((ent = readdir(dir)) != NULL) {
        char *rel_path;
        char *abs_path;
        struct stat st;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        rel_path = rel_dir && rel_dir[0] ? nw_join2(rel_dir, ent->d_name) : nw_strdup(ent->d_name);
        abs_path = rel_path ? nw_join2(root, rel_path) : NULL;
        if (!rel_path || !abs_path) {
            free(rel_path);
            free(abs_path);
            yvex_error_set(err, YVEX_ERR_NOMEM, "native_weights", "scan path allocation failed");
            rc = YVEX_ERR_NOMEM;
            break;
        }
        if (lstat(abs_path, &st) != 0) {
            yvex_error_setf(err, YVEX_ERR_IO, "native_weights", "cannot stat path: %s", abs_path);
            rc = YVEX_ERR_IO;
        } else if (S_ISDIR(st.st_mode) && recursive) {
            rc = nw_scan_dir(root, rel_path, recursive, table, err);
        } else if (S_ISREG(st.st_mode) && nw_ends_with(rel_path, ".safetensors")) {
            table->summary.shard_count++;
            rc = yvex_safetensors_read_header_file(abs_path, rel_path, table, err);
        }
        free(abs_path);
        free(rel_path);
        if (rc != YVEX_OK) {
            break;
        }
    }
    closedir(dir);
    free(abs_dir);
    return rc;
}

int yvex_native_weight_table_open(yvex_native_weight_table **out,
                                  const yvex_native_weight_options *options,
                                  yvex_error *err)
{
    yvex_native_weight_table *table;
    struct stat st;
    int rc;

    if (!out || !options || !options->source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "native_weights_open", "out and source_dir are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (lstat(options->source_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err, YVEX_ERR_IO, "native_weights_open", "source directory not found: %s", options->source_dir);
        return YVEX_ERR_IO;
    }
    table = (yvex_native_weight_table *)calloc(1, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "native_weights_open", "native weight table allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rc = nw_scan_dir(options->source_dir, "", options->recursive, table, err);
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(table);
        return rc;
    }
    *out = table;
    return YVEX_OK;
}

void yvex_native_weight_table_close(yvex_native_weight_table *table)
{
    unsigned long long i;

    if (!table) {
        return;
    }
    for (i = 0; i < table->count; ++i) {
        free((char *)table->items[i].name);
        free((char *)table->items[i].shard_path);
        free((char *)table->items[i].dtype_name);
    }
    free(table->items);
    free(table);
}

unsigned long long yvex_native_weight_table_count(const yvex_native_weight_table *table)
{
    return table ? table->count : 0;
}

const yvex_native_weight_info *yvex_native_weight_table_at(const yvex_native_weight_table *table,
                                                           unsigned long long index)
{
    if (!table || index >= table->count) {
        return NULL;
    }
    return &table->items[index];
}

const yvex_native_weight_info *yvex_native_weight_table_find(const yvex_native_weight_table *table,
                                                             const char *name)
{
    unsigned long long i;

    if (!table || !name) {
        return NULL;
    }
    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].name, name) == 0) {
            return &table->items[i];
        }
    }
    return NULL;
}

int yvex_native_weight_table_summary(const yvex_native_weight_table *table,
                                     yvex_native_weight_summary *out,
                                     yvex_error *err)
{
    if (!table || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "native_weights_summary", "table and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = table->summary;
    return YVEX_OK;
}



int yvex_native_weight_report_json(const char *source,
                                   const yvex_native_weight_table *table,
                                   yvex_error *err)
{
    yvex_native_weight_summary summary;

    if (yvex_native_weight_table_summary(table, &summary, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    printf("{\n");
    printf("  \"schema\": \"yvex.native_weights.v1\",\n");
    printf("  \"source\": \"%s\",\n", source ? source : "");
    printf("  \"summary\": {\n");
    printf("    \"shard_count\": %llu,\n", summary.shard_count);
    printf("    \"tensor_count\": %llu,\n", summary.tensor_count);
    printf("    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
    printf("    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
    printf("    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
    printf("  }\n");
    printf("}\n");
    return YVEX_OK;
}



static unsigned long long st_le64(const unsigned char b[8])
{
    return ((unsigned long long)b[0]) |
           ((unsigned long long)b[1] << 8) |
           ((unsigned long long)b[2] << 16) |
           ((unsigned long long)b[3] << 24) |
           ((unsigned long long)b[4] << 32) |
           ((unsigned long long)b[5] << 40) |
           ((unsigned long long)b[6] << 48) |
           ((unsigned long long)b[7] << 56);
}

int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err)
{
    FILE *fp;
    struct stat st;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    unsigned long long file_size;
    unsigned long long payload_bytes;
    char *json;
    int rc;

    if (!abs_path || !shard_path || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "safetensors_header", "path, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (stat(abs_path, &st) != 0 || st.st_size < 8) {
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "short safetensors file: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    file_size = (unsigned long long)st.st_size;
    fp = fopen(abs_path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "safetensors_header", "cannot open safetensors file: %s", shard_path);
        return YVEX_ERR_IO;
    }
    if (fread(len_bytes, 1, 8, fp) != 8) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    header_len = st_le64(len_bytes);
    if (header_len == 0 || header_len > file_size - 8 || header_len > 64ull * 1024ull * 1024ull) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "invalid safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "safetensors_header", "header allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(json, 1, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "cannot read safetensors header: %s", shard_path);
        table->summary.malformed_shard_count++;
        return YVEX_ERR_FORMAT;
    }
    fclose(fp);
    json[header_len] = '\0';
    payload_bytes = file_size - 8 - header_len;
    rc = yvex_safetensors_parse_header(json, payload_bytes, shard_path, table, err);
    if (rc != YVEX_OK) {
        table->summary.malformed_shard_count++;
    }
    free(json);
    return rc;
}



typedef struct {
    const char *p;
    const char *end;
    const char *shard;
    yvex_error *err;
} st_json;

static void sj_skip_ws(st_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) {
        j->p++;
    }
}

static int sj_fail(st_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "safetensors_json", "%s in %s", msg, j->shard);
    return YVEX_ERR_FORMAT;
}

static int sj_expect(st_json *j, char ch)
{
    sj_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) {
        return sj_fail(j, "unexpected JSON token");
    }
    j->p++;
    return YVEX_OK;
}

static char *sj_string(st_json *j)
{
    const char *start;
    char *out;
    size_t cap;
    size_t n = 0;

    sj_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        sj_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    start = j->p;
    cap = (size_t)(j->end - start) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "safetensors_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                sj_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') {
                out[n++] = ch;
            } else if (ch == 'n') {
                out[n++] = '\n';
            } else if (ch == 'r') {
                out[n++] = '\r';
            } else if (ch == 't') {
                out[n++] = '\t';
            } else {
                free(out);
                sj_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    sj_fail(j, "unterminated string");
    return NULL;
}

static int sj_u64(st_json *j, unsigned long long *out)
{
    unsigned long long v = 0;

    sj_skip_ws(j);
    if (j->p >= j->end || !isdigit((unsigned char)*j->p)) {
        return sj_fail(j, "expected unsigned integer");
    }
    while (j->p < j->end && isdigit((unsigned char)*j->p)) {
        unsigned int d = (unsigned int)(*j->p - '0');
        if (v > (18446744073709551615ull - d) / 10ull) {
            return sj_fail(j, "integer overflow");
        }
        v = v * 10ull + d;
        j->p++;
    }
    *out = v;
    return YVEX_OK;
}

static int sj_skip_value(st_json *j);

static int sj_skip_array(st_json *j)
{
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = sj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed array");
    }
    return sj_fail(j, "unterminated array");
}

static int sj_skip_object(st_json *j)
{
    int rc = sj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = sj_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = sj_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = sj_skip_value(j);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed object");
    }
    return sj_fail(j, "unterminated object");
}

static int sj_skip_literal(st_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) {
        return sj_fail(j, "unexpected literal");
    }
    j->p += n;
    return YVEX_OK;
}

static int sj_skip_value(st_json *j)
{
    char *s;

    sj_skip_ws(j);
    if (j->p >= j->end) return sj_fail(j, "expected value");
    if (*j->p == '{') return sj_skip_object(j);
    if (*j->p == '[') return sj_skip_array(j);
    if (*j->p == '"') {
        s = sj_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (isdigit((unsigned char)*j->p)) {
        unsigned long long ignored;
        return sj_u64(j, &ignored);
    }
    if (*j->p == 't') return sj_skip_literal(j, "true");
    if (*j->p == 'f') return sj_skip_literal(j, "false");
    if (*j->p == 'n') return sj_skip_literal(j, "null");
    return sj_fail(j, "unsupported JSON value");
}

static int sj_parse_shape(st_json *j, unsigned long long *dims, unsigned int *rank)
{
    unsigned int n = 0;
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    sj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        *rank = 0;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        if (n >= YVEX_NATIVE_WEIGHT_MAX_DIMS) {
            return sj_fail(j, "rank exceeds native weight limit");
        }
        rc = sj_u64(j, &dims[n++]);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            *rank = n;
            return YVEX_OK;
        }
        return sj_fail(j, "malformed shape");
    }
    return sj_fail(j, "unterminated shape");
}

static int sj_parse_offsets(st_json *j, unsigned long long *start, unsigned long long *end)
{
    int rc = sj_expect(j, '[');

    if (rc != YVEX_OK) return rc;
    rc = sj_u64(j, start);
    if (rc != YVEX_OK) return rc;
    rc = sj_expect(j, ',');
    if (rc != YVEX_OK) return rc;
    rc = sj_u64(j, end);
    if (rc != YVEX_OK) return rc;
    rc = sj_expect(j, ']');
    if (rc != YVEX_OK) return rc;
    if (*end < *start) {
        return sj_fail(j, "invalid data_offsets order");
    }
    return YVEX_OK;
}

static int sj_parse_tensor(st_json *j, const char *name, unsigned long long payload_bytes,
                           const char *shard_path, yvex_native_weight_table *table)
{
    char *dtype = NULL;
    unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS];
    unsigned int rank = 0;
    unsigned long long start = 0;
    unsigned long long end = 0;
    int have_dtype = 0;
    int have_shape = 0;
    int have_offsets = 0;
    int rc = sj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    memset(dims, 0, sizeof(dims));
    sj_skip_ws(j);
    while (j->p < j->end && *j->p != '}') {
        char *key = sj_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = sj_expect(j, ':');
        if (rc == YVEX_OK && strcmp(key, "dtype") == 0) {
            free(dtype);
            dtype = sj_string(j);
            if (!dtype) rc = yvex_error_code(j->err);
            have_dtype = rc == YVEX_OK;
        } else if (rc == YVEX_OK && strcmp(key, "shape") == 0) {
            rc = sj_parse_shape(j, dims, &rank);
            have_shape = rc == YVEX_OK;
        } else if (rc == YVEX_OK && strcmp(key, "data_offsets") == 0) {
            rc = sj_parse_offsets(j, &start, &end);
            have_offsets = rc == YVEX_OK;
        } else if (rc == YVEX_OK) {
            rc = sj_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) {
            free(dtype);
            return rc;
        }
        sj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            sj_skip_ws(j);
            continue;
        }
        break;
    }
    rc = sj_expect(j, '}');
    if (rc != YVEX_OK) {
        free(dtype);
        return rc;
    }
    if (!have_dtype || !have_shape || !have_offsets) {
        free(dtype);
        return sj_fail(j, "tensor entry missing dtype, shape, or data_offsets");
    }
    if (end > payload_bytes) {
        free(dtype);
        return sj_fail(j, "data_offsets exceed payload size");
    }
    rc = yvex_native_weight_table_add(table, name, shard_path, dtype, rank, dims, start, end, j->err);
    free(dtype);
    return rc;
}

int yvex_safetensors_parse_header(const char *json,
                                  unsigned long long payload_bytes,
                                  const char *shard_path,
                                  yvex_native_weight_table *table,
                                  yvex_error *err)
{
    st_json j;
    int rc;

    if (!json || !shard_path || !table) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "safetensors_json", "json, shard, and table are required");
        return YVEX_ERR_INVALID_ARG;
    }
    j.p = json;
    j.end = json + strlen(json);
    j.shard = shard_path;
    j.err = err;
    rc = sj_expect(&j, '{');
    if (rc != YVEX_OK) return rc;
    sj_skip_ws(&j);
    if (j.p < j.end && *j.p == '}') {
        return sj_fail(&j, "empty safetensors header");
    }
    while (j.p < j.end) {
        char *key = sj_string(&j);
        if (!key) return yvex_error_code(err);
        rc = sj_expect(&j, ':');
        if (rc == YVEX_OK && strcmp(key, "__metadata__") == 0) {
            rc = sj_skip_value(&j);
        } else if (rc == YVEX_OK) {
            rc = sj_parse_tensor(&j, key, payload_bytes, shard_path, table);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        sj_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') {
            j.p++;
            continue;
        }
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            sj_skip_ws(&j);
            if (j.p != j.end) {
                return sj_fail(&j, "trailing JSON bytes");
            }
            return YVEX_OK;
        }
        return sj_fail(&j, "malformed safetensors header");
    }
    return sj_fail(&j, "unterminated safetensors header");
}
