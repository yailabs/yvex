/*
 * yvex_source.c - source manifests, pressure reports, and native headers.
 *
 * Owner:
 *   src/source
 *
 * Owns:
 *   source manifests, source pressure reports, source evidence, provenance
 *   summaries, footprint summaries, and native safetensors header inventory.
 *
 * Does not own:
 *   tensor payload loading unless a row explicitly owns it, artifact emission,
 *   runtime support, generation, eval, benchmark, throughput, or release
 *   decisions.
 *
 * Invariants:
 *   header-only paths remain header-only; source reports may inspect local
 *   metadata, sidecars, and safetensors headers but must not verify remotes,
 *   hash payloads, or infer runtime readiness.
 *
 * Boundary:
 *   source intake is not source verification, artifact emission, model support,
 *   generation support, eval evidence, benchmark evidence, or release
 *   readiness.
 */

#include "yvex_operator_private.h"

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
#include <limits.h>
#include <unistd.h>


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
    unsigned long long header_read_count;
    unsigned long long header_error_count;
    unsigned long long header_bytes;
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

/*
 * yvex_source_manifest_scan_local()
 *
 * Purpose:
 *   summarize a local source directory into source-manifest footprint facts.
 *
 * Inputs:
 *   local_path is borrowed; out receives by-value summary fields.
 *
 * Effects:
 *   scans local directory entries and sizes through manifest file-list helpers;
 *   it does not hash files, read tensor payloads, or contact remotes.
 *
 * Failure:
 *   returns invalid-arg, IO, allocation, or scan errors from the manifest
 *   helpers.
 *
 * Boundary:
 *   local footprint scanning is not source verification, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
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

/*
 * yvex_source_manifest_write_json_file()
 *
 * Purpose:
 *   write source manifest summary and file-list facts to JSON.
 *
 * Inputs:
 *   output path, manifest options, and file list are borrowed.
 *
 * Effects:
 *   opens a local JSON file, writes escaped metadata and footprint fields, and
 *   closes the file; it does not create artifacts or hash tensor payloads.
 *
 * Failure:
 *   returns invalid-arg or IO errors for missing inputs, open/write/close
 *   failures, and path issues.
 *
 * Boundary:
 *   writing a source manifest is not source verification, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
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

#define YVEX_SOURCE_MANIFEST_PROBE_CAP 8192u

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
    const char *source_artifact_format;
    const char *source_artifact_origin;
    const char *source_artifact_authority;
    const char *source_tensor_container;
    const char *target_artifact_class;
    const char *target_artifact_origin;
    const char *target_artifact_required;
    const char *external_reference_status;
    const char *yvex_produced_artifact_status;
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
    "missing-qwen-tensor-role-map",
    "missing-qwen-tensor-map",
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
    "missing-gemma-tensor-role-map",
    "missing-gemma-tensor-map",
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
        "qwen3-8b",
        "source-model-candidate",
        "Qwen3-8B",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "safetensors+config-tokenizer-sidecars",
        "official",
        "upstream-official",
        "safetensors",
        "future-YVEX-produced-GGUF",
        "planned",
        "true",
        "false",
        "planned",
        "backend-neutral-qwen-source-model-target",
        "causal-decoder-candidate-pending-config",
        "backend-selection-deferred",
        "metal-planned",
        "pending source/config verification",
        "missing-qwen-source-path",
        "missing-qwen-source-manifest",
        "missing-qwen-native-inventory",
        "missing-qwen-source-config",
        "missing-qwen-tokenizer-files",
        "missing-qwen-tensor-role-map",
        "V010.MAP.8",
        qwen_source_tail_blockers,
        sizeof(qwen_source_tail_blockers) / sizeof(qwen_source_tail_blockers[0]),
    },
    {
        "gemma",
        "Gemma",
        "gemma-source-pressure",
        "gemma-4-12b-it",
        "source-model-candidate",
        "Gemma-4-12B-it",
        "profiled",
        "present",
        "official-source-tensors-planned",
        "safetensors+config-tokenizer-sidecars",
        "official",
        "upstream-official",
        "safetensors",
        "future-YVEX-produced-GGUF",
        "planned",
        "true",
        "false",
        "planned",
        "backend-neutral-gemma-source-model-target",
        "dense-candidate-pending-source-config",
        "backend-selection-deferred",
        "cpu-cuda-baseline-planned",
        "pending source/config verification",
        "missing-gemma-source-path",
        "missing-gemma-source-manifest",
        "missing-gemma-native-inventory",
        "missing-gemma-source-config",
        "missing-gemma-tokenizer-files",
        "missing-gemma-tensor-role-map",
        "V010.MAP.8",
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
    char resolved_target[128];
    char resolved_model[128];
    int include_files;
    int include_config;
    int include_blockers;
    int include_next;
    int include_tensors;
    unsigned long long tensor_limit;
    yvex_source_report_output_mode output_mode;
} yvex_qwen_source_report_options;

#define YVEX_SOURCE_TENSOR_SAMPLE_CAP 20u
#define YVEX_SOURCE_TENSOR_SHAPE_CAP 128u

typedef struct {
    char name[192];
    char file[192];
    char dtype[24];
    char shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];
    unsigned long long rank;
    unsigned long long elements;
    unsigned long long declared_bytes;
} yvex_source_tensor_sample;

typedef struct {
    const char *status;
    const char *source_state;
    const char *top_blocker;
    const char *next_row;
    char identity_target_id[128];
    char identity_model[128];
    char identity_family[32];
    char identity_repo_id[256];
    char identity_revision[128];
    char identity_local_source_dir[YVEX_PATH_CAP];
    char download_registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    int download_registry_exists;
    int download_report_exists;
    int tensor_map_exists;
    int output_head_map_exists;
    int tokenizer_map_exists;
    int tensor_map_incomplete;
    int output_head_map_missing;
    int source_identity_from_path;
    int source_identity_from_download_sidecar;
    char source_path[YVEX_PATH_CAP];
    char source_path_source[64];
    char manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    int source_exists;
    int config_exists;
    int generation_config_exists;
    int tokenizer_json_exists;
    int tokenizer_config_exists;
    int readme_exists;
    int license_exists;
    int manifest_exists;
    int manifest_probe_checked;
    int manifest_probe_error;
    int manifest_has_schema;
    int manifest_schema_matches;
    int manifest_has_family;
    int manifest_family_matches;
    int manifest_has_target;
    int manifest_target_matches;
    int manifest_has_artifact_class;
    int manifest_has_footprint;
    int manifest_has_provenance;
    int manifest_has_native_inventory;
    int manifest_has_tensor_metadata;
    char manifest_schema_version[64];
    int native_inventory_exists;
    unsigned long long source_file_count;
    unsigned long long source_regular_file_count;
    unsigned long long safetensors_count;
    unsigned long long bin_count;
    unsigned long long dat_count;
    unsigned long long json_count;
    unsigned long long tokenizer_file_count;
    unsigned long long config_file_count;
    unsigned long long total_size_bytes;
    unsigned long long safetensors_size_bytes;
    unsigned long long sidecar_size_bytes;
    unsigned long long other_size_bytes;
    unsigned long long largest_source_file_bytes;
    char largest_source_file_name[YVEX_PATH_CAP];
    unsigned long long native_safetensors_count;
    unsigned long long native_safetensors_opened;
    unsigned long long native_safetensors_header_read_count;
    unsigned long long native_safetensors_header_error_count;
    unsigned long long native_safetensors_header_bytes;
    unsigned long long native_tensor_count;
    unsigned long long native_declared_data_bytes;
    unsigned long long native_declared_tensor_bytes;
    unsigned long long native_max_rank;
    unsigned long long native_max_tensor_elements;
    char native_largest_tensor_name[YVEX_PATH_CAP];
    unsigned long long native_largest_tensor_bytes;
    unsigned long long native_dtype_f16_count;
    unsigned long long native_dtype_bf16_count;
    unsigned long long native_dtype_f32_count;
    unsigned long long native_dtype_i8_count;
    unsigned long long native_dtype_i16_count;
    unsigned long long native_dtype_i32_count;
    unsigned long long native_dtype_i64_count;
    unsigned long long native_dtype_u8_count;
    unsigned long long native_dtype_other_count;
    unsigned long long native_invalid_file_count;
    unsigned long long native_inventory_error_count;
    unsigned long long source_tensor_count;
    unsigned long long source_tensor_name_count;
    unsigned long long source_tensor_file_count;
    unsigned long long source_tensor_dtype_count;
    unsigned long long source_tensor_rank_count;
    unsigned long long source_tensor_shape_count;
    unsigned long long source_tensor_declared_data_bytes;
    unsigned long long source_tensor_declared_tensor_bytes;
    unsigned long long source_tensor_total_elements;
    unsigned long long source_tensor_max_rank;
    unsigned long long source_tensor_max_elements;
    char source_tensor_largest_name[YVEX_PATH_CAP];
    char source_tensor_largest_file[YVEX_PATH_CAP];
    char source_tensor_largest_dtype[24];
    unsigned long long source_tensor_largest_rank;
    char source_tensor_largest_shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];
    unsigned long long source_tensor_largest_elements;
    unsigned long long source_tensor_largest_declared_bytes;
    unsigned long long source_tensor_dtype_f16_count;
    unsigned long long source_tensor_dtype_bf16_count;
    unsigned long long source_tensor_dtype_f32_count;
    unsigned long long source_tensor_dtype_i8_count;
    unsigned long long source_tensor_dtype_i16_count;
    unsigned long long source_tensor_dtype_i32_count;
    unsigned long long source_tensor_dtype_i64_count;
    unsigned long long source_tensor_dtype_u8_count;
    unsigned long long source_tensor_dtype_other_count;
    unsigned long long source_tensor_rank_0_count;
    unsigned long long source_tensor_rank_1_count;
    unsigned long long source_tensor_rank_2_count;
    unsigned long long source_tensor_rank_3_count;
    unsigned long long source_tensor_rank_4_count;
    unsigned long long source_tensor_rank_other_count;
    unsigned long long source_tensor_metadata_error_count;
    unsigned long long source_tensor_name_embed_count;
    unsigned long long source_tensor_name_attn_count;
    unsigned long long source_tensor_name_mlp_count;
    unsigned long long source_tensor_name_norm_count;
    unsigned long long source_tensor_name_lm_head_count;
    unsigned long long source_tensor_name_other_count;
    yvex_source_tensor_sample source_tensor_samples[YVEX_SOURCE_TENSOR_SAMPLE_CAP];
    unsigned long long source_tensor_sample_count;
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

static int qwen_source_parse_positive_ull(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
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
    if ((strcmp(profile->family_key, "qwen") == 0 &&
         strncmp(target, "qwen", 4) == 0) ||
        (strcmp(profile->family_key, "gemma") == 0 &&
         strncmp(target, "gemma", 5) == 0)) {
        return 1;
    }
    if (strcmp(profile->family_key, "qwen") == 0) {
        return strcmp(target, "qwen3-8b") == 0 ||
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

static const char *qwen_source_path_basename(const char *path)
{
    const char *slash;

    if (!path || !path[0]) return NULL;
    slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static int qwen_source_target_matches_family_name(const char *family,
                                                  const char *target)
{
    if (!family || !target) return 0;
    if (strcmp(family, "qwen") == 0) {
        return strncmp(target, "qwen", 4) == 0;
    }
    if (strcmp(family, "gemma") == 0) {
        return strncmp(target, "gemma", 5) == 0;
    }
    return 0;
}

static int qwen_source_read_small_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) return 0;
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) return 0;
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

static int qwen_source_json_string_field(const char *text,
                                         const char *key,
                                         char *out,
                                         size_t cap)
{
    char needle[96];
    const char *p;
    const char *q;
    size_t len;

    if (out && cap > 0u) out[0] = '\0';
    if (!text || !key || !out || cap == 0u) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return 0;
    p++;
    q = strchr(p, '"');
    if (!q) return 0;
    len = (size_t)(q - p);
    if (len >= cap) len = cap - 1u;
    memcpy(out, p, len);
    out[len] = '\0';
    return 1;
}

static int qwen_source_json_u64_field(const char *text,
                                      const char *key,
                                      unsigned long long *out)
{
    char needle[96];
    const char *p;
    unsigned long long value = 0;
    int seen = 0;

    if (out) *out = 0;
    if (!text || !key || !out) return 0;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(text, needle);
    if (!p) return 0;
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    while (*p && isdigit((unsigned char)*p)) {
        value = value * 10ull + (unsigned long long)(*p - '0');
        seen = 1;
        p++;
    }
    if (!seen) return 0;
    *out = value;
    return 1;
}

static const char *qwen_source_repo_basename(const char *repo)
{
    const char *slash = repo ? strrchr(repo, '/') : NULL;
    return slash && slash[1] ? slash + 1 : repo;
}

static void qwen_source_copy_model_display(char *out,
                                           size_t cap,
                                           const char *family,
                                           const char *model_name)
{
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (!model_name || !model_name[0]) return;
    snprintf(out, cap, "%s", model_name);
    if (family && strcmp(family, "gemma") == 0 &&
        strncmp(out, "gemma-", 6) == 0) {
        out[0] = 'G';
    }
}

static const char *qwen_source_model_name(const yvex_qwen_source_report_options *options,
                                          const yvex_qwen_source_pressure_report *report)
{
    if (report && report->identity_model[0]) return report->identity_model;
    if (options && options->resolved_model[0]) return options->resolved_model;
    return options && options->profile ? options->profile->model : "unknown";
}

static int qwen_source_probe_download_identity_file(
    const char *path,
    const char *target,
    const char *family,
    yvex_qwen_source_pressure_report *report)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    char parsed_target[128];
    char parsed_family[32];
    char repo_id[256];
    char revision[128];
    char source_dir[YVEX_PATH_CAP];
    const char *model_name;

    if (!path || !path[0] || !target || !family || !report) return 0;
    if (access(path, F_OK) != 0) return 0;
    if (!qwen_source_read_small_file(path, buf, sizeof(buf))) return 0;

    memset(parsed_target, 0, sizeof(parsed_target));
    memset(parsed_family, 0, sizeof(parsed_family));
    memset(repo_id, 0, sizeof(repo_id));
    memset(revision, 0, sizeof(revision));
    memset(source_dir, 0, sizeof(source_dir));
    qwen_source_json_string_field(buf, "target_id", parsed_target,
                                  sizeof(parsed_target));
    if (parsed_target[0] && strcmp(parsed_target, target) != 0) return 0;
    qwen_source_json_string_field(buf, "family", parsed_family,
                                  sizeof(parsed_family));
    if (parsed_family[0] && strcmp(parsed_family, family) != 0) return 0;
    qwen_source_json_string_field(buf, "repo_id", repo_id, sizeof(repo_id));
    if (!repo_id[0]) {
        qwen_source_json_string_field(buf, "repo", repo_id, sizeof(repo_id));
    }
    qwen_source_json_string_field(buf, "revision", revision, sizeof(revision));
    qwen_source_json_string_field(buf, "local_source_dir", source_dir,
                                  sizeof(source_dir));
    if (!source_dir[0]) {
        qwen_source_json_string_field(buf, "path", source_dir, sizeof(source_dir));
    }

    snprintf(report->identity_target_id, sizeof(report->identity_target_id), "%s",
             parsed_target[0] ? parsed_target : target);
    snprintf(report->identity_family, sizeof(report->identity_family), "%s",
             parsed_family[0] ? parsed_family : family);
    snprintf(report->identity_repo_id, sizeof(report->identity_repo_id), "%s",
             repo_id[0] ? repo_id : "unknown");
    snprintf(report->identity_revision, sizeof(report->identity_revision), "%s",
             revision[0] ? revision : "main");
    if (source_dir[0]) {
        snprintf(report->identity_local_source_dir,
                 sizeof(report->identity_local_source_dir), "%s", source_dir);
    }
    model_name = qwen_source_repo_basename(repo_id);
    if (model_name && model_name[0]) {
        qwen_source_copy_model_display(report->identity_model,
                                       sizeof(report->identity_model),
                                       family,
                                       model_name);
    }
    report->source_identity_from_download_sidecar = 1;
    return 1;
}

static void qwen_source_probe_map_sidecars(yvex_qwen_source_pressure_report *report)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    char status[64];
    char coverage[64];
    unsigned long long unmapped = 0;

    if (!report) return;
    if (report->tensor_map_exists &&
        qwen_source_read_small_file(report->tensor_map_path, buf, sizeof(buf))) {
        if (qwen_source_json_string_field(buf, "required_role_coverage_status",
                                          coverage, sizeof(coverage))) {
            if (strcmp(coverage, "required-groups-present") != 0) {
                report->tensor_map_incomplete = 1;
            }
        } else if (qwen_source_json_u64_field(buf, "unmapped_unknown_count",
                                              &unmapped) &&
                   unmapped > 0ull) {
            report->tensor_map_incomplete = 1;
        }
    }
    if (report->output_head_map_exists &&
        qwen_source_read_small_file(report->output_head_map_path, buf, sizeof(buf)) &&
        qwen_source_json_string_field(buf, "output_head_status", status,
                                      sizeof(status)) &&
        strcmp(status, "present") != 0) {
        report->output_head_map_missing = 1;
    }
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

static int qwen_source_manifest_file_exists(char *out,
                                            size_t cap,
                                            const char *dir,
                                            const char *name)
{
    char candidate[YVEX_PATH_CAP];

    if (!out || cap == 0 || !dir || dir[0] == '\0' || !name) {
        return 0;
    }
    if (!qwen_source_path_format(candidate, sizeof(candidate), "%s/%s", dir, name)) {
        return 0;
    }
    if (!qwen_source_stat_kind(candidate, 0)) {
        return 0;
    }
    snprintf(out, cap, "%s", candidate);
    return 1;
}

static int qwen_source_check_file(const char *dir, const char *name)
{
    char path[YVEX_PATH_CAP];

    if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, name)) {
        return 0;
    }
    return qwen_source_stat_kind(path, 0);
}

static int qwen_source_is_config_file(const char *name)
{
    return name && (strcmp(name, "config.json") == 0 ||
                    strcmp(name, "generation_config.json") == 0);
}

static int qwen_source_is_tokenizer_file(const char *name)
{
    return name && (strcmp(name, "tokenizer.json") == 0 ||
                    strcmp(name, "tokenizer_config.json") == 0);
}

static int qwen_source_is_sidecar_file(const char *name)
{
    if (!name) {
        return 0;
    }
    return qwen_source_is_config_file(name) ||
           qwen_source_is_tokenizer_file(name) ||
           strcmp(name, "README.md") == 0 ||
           yvex_sm_ends_with(name, ".json");
}

static unsigned long long qwen_source_stat_size_bytes(const struct stat *st)
{
    if (!st || st->st_size <= 0) {
        return 0;
    }
    return (unsigned long long)st->st_size;
}

static void qwen_source_scan_top_footprint(const char *dir,
                                           yvex_qwen_source_pressure_report *report)
{
    DIR *dp;
    struct dirent *ent;

    if (!dir || dir[0] == '\0' || !report) {
        return;
    }
    dp = opendir(dir);
    if (!dp) {
        return;
    }
    while ((ent = readdir(dp)) != NULL) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        unsigned long long size_bytes;
        int is_safetensors;
        int is_sidecar;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, ent->d_name)) {
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }

        size_bytes = qwen_source_stat_size_bytes(&st);
        is_safetensors = yvex_sm_ends_with(ent->d_name, ".safetensors");
        is_sidecar = qwen_source_is_sidecar_file(ent->d_name);

        report->source_file_count++;
        report->source_regular_file_count++;
        report->total_size_bytes += size_bytes;

        if (is_safetensors) {
            report->safetensors_count++;
            report->safetensors_size_bytes += size_bytes;
        } else if (is_sidecar) {
            report->sidecar_size_bytes += size_bytes;
        } else {
            report->other_size_bytes += size_bytes;
        }
        if (yvex_sm_ends_with(ent->d_name, ".bin")) {
            report->bin_count++;
        }
        if (yvex_sm_ends_with(ent->d_name, ".dat")) {
            report->dat_count++;
        }
        if (yvex_sm_ends_with(ent->d_name, ".json")) {
            report->json_count++;
        }
        if (qwen_source_is_tokenizer_file(ent->d_name)) {
            report->tokenizer_file_count++;
        }
        if (qwen_source_is_config_file(ent->d_name)) {
            report->config_file_count++;
        }
        if (size_bytes > report->largest_source_file_bytes) {
            report->largest_source_file_bytes = size_bytes;
            snprintf(report->largest_source_file_name,
                     sizeof(report->largest_source_file_name),
                     "%s",
                     ent->d_name);
        }
    }
    closedir(dp);
}

static unsigned long long qwen_source_native_tensor_elements(
    const yvex_native_weight_info *info)
{
    unsigned long long elements = 1;
    unsigned int i;

    if (!info) {
        return 0;
    }
    for (i = 0; i < info->rank; ++i) {
        if (info->dims[i] == 0) {
            return 0;
        }
        if (elements > ULLONG_MAX / info->dims[i]) {
            return ULLONG_MAX;
        }
        elements *= info->dims[i];
    }
    return elements;
}

static void qwen_source_tensor_shape_string(const yvex_native_weight_info *info,
                                            char *out,
                                            size_t cap)
{
    size_t used = 0;
    unsigned int i;

    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';
    if (!info) {
        snprintf(out, cap, "[]");
        return;
    }
    used += (size_t)snprintf(out + used, cap - used, "[");
    for (i = 0; i < info->rank && used < cap; ++i) {
        int n = snprintf(out + used, cap - used, "%s%llu",
                         i == 0 ? "" : ",",
                         info->dims[i]);
        if (n < 0) {
            break;
        }
        if ((size_t)n >= cap - used) {
            out[cap - 1] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used < cap) {
        snprintf(out + used, cap - used, "]");
    } else {
        out[cap - 1] = '\0';
    }
}

static int qwen_source_tensor_shape_same(const yvex_native_weight_info *a,
                                         const yvex_native_weight_info *b)
{
    unsigned int i;

    if (!a || !b || a->rank != b->rank) {
        return 0;
    }
    for (i = 0; i < a->rank; ++i) {
        if (a->dims[i] != b->dims[i]) {
            return 0;
        }
    }
    return 1;
}

static int qwen_source_tensor_shape_first_seen(const yvex_native_weight_table *table,
                                               unsigned long long index)
{
    unsigned long long i;

    if (!table || index >= table->count) {
        return 0;
    }
    for (i = 0; i < index; ++i) {
        if (qwen_source_tensor_shape_same(&table->items[i], &table->items[index])) {
            return 0;
        }
    }
    return 1;
}

static int qwen_source_native_shard_first_seen(const yvex_native_weight_table *table,
                                               unsigned long long index)
{
    unsigned long long i;
    const char *shard;

    if (!table || index >= table->count) {
        return 0;
    }
    shard = table->items[index].shard_path;
    for (i = 0; i < index; ++i) {
        if (strcmp(table->items[i].shard_path, shard) == 0) {
            return 0;
        }
    }
    return 1;
}

static unsigned long long qwen_source_native_shard_max_data_end(
    const yvex_native_weight_table *table,
    const char *shard)
{
    unsigned long long i;
    unsigned long long max_end = 0;

    if (!table || !shard) {
        return 0;
    }
    for (i = 0; i < table->count; ++i) {
        if (strcmp(table->items[i].shard_path, shard) == 0 &&
            table->items[i].data_end > max_end) {
            max_end = table->items[i].data_end;
        }
    }
    return max_end;
}

static int qwen_source_name_contains_ci(const char *name, const char *needle)
{
    size_t needle_len;
    size_t i;

    if (!name || !needle) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0) {
        return 1;
    }
    for (i = 0; name[i] != '\0'; ++i) {
        size_t j;
        for (j = 0; j < needle_len; ++j) {
            if (name[i + j] == '\0' ||
                tolower((unsigned char)name[i + j]) !=
                    tolower((unsigned char)needle[j])) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static const char *qwen_source_file_label(const char *path)
{
    const char *slash;

    if (!path || path[0] == '\0') {
        return "unknown";
    }
    slash = strrchr(path, '/');
    return slash && slash[1] != '\0' ? slash + 1 : path;
}

static void qwen_source_native_count_dtype(yvex_qwen_source_pressure_report *report,
                                           yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F16:
        report->native_dtype_f16_count++;
        break;
    case YVEX_NATIVE_DTYPE_BF16:
        report->native_dtype_bf16_count++;
        break;
    case YVEX_NATIVE_DTYPE_F32:
        report->native_dtype_f32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I8:
        report->native_dtype_i8_count++;
        break;
    case YVEX_NATIVE_DTYPE_I16:
        report->native_dtype_i16_count++;
        break;
    case YVEX_NATIVE_DTYPE_I32:
        report->native_dtype_i32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I64:
        report->native_dtype_i64_count++;
        break;
    case YVEX_NATIVE_DTYPE_U8:
        report->native_dtype_u8_count++;
        break;
    default:
        report->native_dtype_other_count++;
        break;
    }
}

static void qwen_source_metadata_count_dtype(yvex_qwen_source_pressure_report *report,
                                             yvex_native_dtype dtype)
{
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F16:
        report->source_tensor_dtype_f16_count++;
        break;
    case YVEX_NATIVE_DTYPE_BF16:
        report->source_tensor_dtype_bf16_count++;
        break;
    case YVEX_NATIVE_DTYPE_F32:
        report->source_tensor_dtype_f32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I8:
        report->source_tensor_dtype_i8_count++;
        break;
    case YVEX_NATIVE_DTYPE_I16:
        report->source_tensor_dtype_i16_count++;
        break;
    case YVEX_NATIVE_DTYPE_I32:
        report->source_tensor_dtype_i32_count++;
        break;
    case YVEX_NATIVE_DTYPE_I64:
        report->source_tensor_dtype_i64_count++;
        break;
    case YVEX_NATIVE_DTYPE_U8:
        report->source_tensor_dtype_u8_count++;
        break;
    default:
        report->source_tensor_dtype_other_count++;
        break;
    }
}

static void qwen_source_metadata_count_rank(yvex_qwen_source_pressure_report *report,
                                            unsigned long long rank)
{
    if (rank == 0) report->source_tensor_rank_0_count++;
    else if (rank == 1) report->source_tensor_rank_1_count++;
    else if (rank == 2) report->source_tensor_rank_2_count++;
    else if (rank == 3) report->source_tensor_rank_3_count++;
    else if (rank == 4) report->source_tensor_rank_4_count++;
    else report->source_tensor_rank_other_count++;
}

static void qwen_source_metadata_count_name(yvex_qwen_source_pressure_report *report,
                                            const char *name)
{
    if (qwen_source_name_contains_ci(name, "embed") ||
        qwen_source_name_contains_ci(name, "embd")) {
        report->source_tensor_name_embed_count++;
    } else if (qwen_source_name_contains_ci(name, "attn") ||
               qwen_source_name_contains_ci(name, "attention")) {
        report->source_tensor_name_attn_count++;
    } else if (qwen_source_name_contains_ci(name, "mlp") ||
               qwen_source_name_contains_ci(name, "ffn") ||
               qwen_source_name_contains_ci(name, "feed_forward")) {
        report->source_tensor_name_mlp_count++;
    } else if (qwen_source_name_contains_ci(name, "norm")) {
        report->source_tensor_name_norm_count++;
    } else if (qwen_source_name_contains_ci(name, "lm_head") ||
               qwen_source_name_contains_ci(name, "output.weight")) {
        report->source_tensor_name_lm_head_count++;
    } else {
        report->source_tensor_name_other_count++;
    }
}

static unsigned long long qwen_source_metadata_count_distinct_dtypes(
    const yvex_qwen_source_pressure_report *report)
{
    unsigned long long count = 0;

    if (!report) {
        return 0;
    }
    if (report->source_tensor_dtype_f16_count) count++;
    if (report->source_tensor_dtype_bf16_count) count++;
    if (report->source_tensor_dtype_f32_count) count++;
    if (report->source_tensor_dtype_i8_count) count++;
    if (report->source_tensor_dtype_i16_count) count++;
    if (report->source_tensor_dtype_i32_count) count++;
    if (report->source_tensor_dtype_i64_count) count++;
    if (report->source_tensor_dtype_u8_count) count++;
    if (report->source_tensor_dtype_other_count) count++;
    return count;
}

static unsigned long long qwen_source_metadata_count_distinct_ranks(
    const yvex_qwen_source_pressure_report *report)
{
    unsigned long long count = 0;

    if (!report) {
        return 0;
    }
    if (report->source_tensor_rank_0_count) count++;
    if (report->source_tensor_rank_1_count) count++;
    if (report->source_tensor_rank_2_count) count++;
    if (report->source_tensor_rank_3_count) count++;
    if (report->source_tensor_rank_4_count) count++;
    if (report->source_tensor_rank_other_count) count++;
    return count;
}

static void qwen_source_metadata_add_sample(
    yvex_qwen_source_pressure_report *report,
    const yvex_native_weight_info *info,
    unsigned long long elements)
{
    yvex_source_tensor_sample *sample;

    if (!report || !info ||
        report->source_tensor_sample_count >= YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        return;
    }
    sample = &report->source_tensor_samples[report->source_tensor_sample_count++];
    snprintf(sample->name, sizeof(sample->name), "%s", info->name ? info->name : "unknown");
    snprintf(sample->file, sizeof(sample->file), "%s", qwen_source_file_label(info->shard_path));
    snprintf(sample->dtype, sizeof(sample->dtype), "%s", yvex_native_dtype_name(info->dtype));
    qwen_source_tensor_shape_string(info, sample->shape, sizeof(sample->shape));
    sample->rank = info->rank;
    sample->elements = elements;
    sample->declared_bytes = info->data_bytes;
}

static void qwen_source_native_collect_table(
    yvex_qwen_source_pressure_report *report,
    const yvex_native_weight_table *table)
{
    unsigned long long i;

    if (!report || !table) {
        return;
    }
    report->native_safetensors_header_read_count = table->header_read_count;
    report->native_safetensors_header_error_count = table->header_error_count;
    report->native_safetensors_header_bytes = table->header_bytes;
    report->native_tensor_count = table->count;
    report->native_declared_tensor_bytes = table->summary.total_tensor_bytes;
    report->native_invalid_file_count = table->header_error_count;
    report->native_inventory_error_count = table->header_error_count;
    report->source_tensor_count = table->count;
    report->source_tensor_name_count = table->count;
    report->source_tensor_declared_tensor_bytes = table->summary.total_tensor_bytes;
    report->source_tensor_metadata_error_count = table->header_error_count;

    for (i = 0; i < table->count; ++i) {
        const yvex_native_weight_info *info = &table->items[i];
        unsigned long long elements = qwen_source_native_tensor_elements(info);
        char shape[YVEX_SOURCE_TENSOR_SHAPE_CAP];

        qwen_source_native_count_dtype(report, info->dtype);
        qwen_source_metadata_count_dtype(report, info->dtype);
        qwen_source_metadata_count_rank(report, info->rank);
        qwen_source_metadata_count_name(report, info->name);
        if (elements != ULLONG_MAX &&
            report->source_tensor_total_elements <= ULLONG_MAX - elements) {
            report->source_tensor_total_elements += elements;
        } else {
            report->source_tensor_total_elements = ULLONG_MAX;
        }
        if (info->rank > report->native_max_rank) {
            report->native_max_rank = info->rank;
        }
        if (info->rank > report->source_tensor_max_rank) {
            report->source_tensor_max_rank = info->rank;
        }
        if (elements > report->native_max_tensor_elements) {
            report->native_max_tensor_elements = elements;
        }
        if (elements > report->source_tensor_max_elements) {
            report->source_tensor_max_elements = elements;
        }
        if (qwen_source_tensor_shape_first_seen(table, i)) {
            report->source_tensor_shape_count++;
        }
        qwen_source_metadata_add_sample(report, info, elements);
        if (info->data_bytes > report->native_largest_tensor_bytes) {
            report->native_largest_tensor_bytes = info->data_bytes;
            snprintf(report->native_largest_tensor_name,
                     sizeof(report->native_largest_tensor_name),
                     "%s",
                     info->name ? info->name : "unknown");
        }
        if (info->data_bytes > report->source_tensor_largest_declared_bytes) {
            qwen_source_tensor_shape_string(info, shape, sizeof(shape));
            report->source_tensor_largest_declared_bytes = info->data_bytes;
            report->source_tensor_largest_rank = info->rank;
            report->source_tensor_largest_elements = elements;
            snprintf(report->source_tensor_largest_name,
                     sizeof(report->source_tensor_largest_name),
                     "%s",
                     info->name ? info->name : "unknown");
            snprintf(report->source_tensor_largest_file,
                     sizeof(report->source_tensor_largest_file),
                     "%s",
                     qwen_source_file_label(info->shard_path));
            snprintf(report->source_tensor_largest_dtype,
                     sizeof(report->source_tensor_largest_dtype),
                     "%s",
                     yvex_native_dtype_name(info->dtype));
            snprintf(report->source_tensor_largest_shape,
                     sizeof(report->source_tensor_largest_shape),
                     "%s",
                     shape);
        }
        if (qwen_source_native_shard_first_seen(table, i)) {
            unsigned long long shard_bytes =
                qwen_source_native_shard_max_data_end(table, info->shard_path);
            report->native_declared_data_bytes += shard_bytes;
            report->source_tensor_declared_data_bytes += shard_bytes;
            report->source_tensor_file_count++;
        }
    }
    report->source_tensor_dtype_count =
        qwen_source_metadata_count_distinct_dtypes(report);
    report->source_tensor_rank_count =
        qwen_source_metadata_count_distinct_ranks(report);
}

static int qwen_source_scan_native_inventory(const char *dir,
                                             yvex_qwen_source_pressure_report *report)
{
    const unsigned long long max_safetensors_files = 1024;
    yvex_native_weight_table *table;
    DIR *dp;
    struct dirent *ent;
    int fatal_rc = 0;
    unsigned long long scan_error_count = 0;

    if (!dir || !report || !report->source_exists) {
        return 0;
    }
    table = (yvex_native_weight_table *)calloc(1, sizeof(*table));
    if (!table) {
        return 3;
    }
    dp = opendir(dir);
    if (!dp) {
        free(table);
        return 0;
    }
    while ((ent = readdir(dp)) != NULL) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        yvex_error err;
        int rc;

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0 ||
            !yvex_sm_ends_with(ent->d_name, ".safetensors")) {
            continue;
        }
        if (!qwen_source_path_format(path, sizeof(path), "%s/%s", dir, ent->d_name)) {
            continue;
        }
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        if (report->native_safetensors_count >= max_safetensors_files) {
            scan_error_count++;
            break;
        }

        report->native_safetensors_count++;
        report->native_safetensors_opened++;
        table->summary.shard_count++;
        yvex_error_clear(&err);
        {
            unsigned long long error_count_before = table->header_error_count;
            rc = yvex_safetensors_read_header_file(path, ent->d_name, table, &err);
            if (rc != YVEX_OK && table->header_error_count == error_count_before) {
                table->header_error_count++;
            }
        }
        if (rc == YVEX_ERR_NOMEM) {
            fatal_rc = 3;
            break;
        }
    }
    closedir(dp);
    qwen_source_native_collect_table(report, table);
    report->native_safetensors_header_error_count += scan_error_count;
    report->native_invalid_file_count += scan_error_count;
    report->native_inventory_error_count += scan_error_count;
    report->source_tensor_metadata_error_count += scan_error_count;
    yvex_native_weight_table_close(table);
    return fatal_rc;
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
        if (strcmp(kind, "manifest") == 0 &&
            (qwen_source_manifest_file_exists(out, cap, source_path, "source_manifest.json") ||
             qwen_source_manifest_file_exists(out, cap, source_path, "source-manifest.json"))) {
            if (out_exists) *out_exists = 1;
            return;
        }
        if (strcmp(kind, "manifest") != 0 &&
            qwen_source_manifest_file_exists(out, cap, source_path, "native-inventory.json")) {
            if (out_exists) *out_exists = 1;
            return;
        }
    }

    if (reports_root && reports_root[0] != '\0') {
        {
            char target_prefix[YVEX_PATH_CAP];

            n = snprintf(target_prefix, sizeof(target_prefix), "%s/%s/%s",
                         reports_root, profile->family_key, target);
            if (n >= 0 && (size_t)n < sizeof(target_prefix) &&
                qwen_source_path_format(candidate, sizeof(candidate), "%s.%s",
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
        n = snprintf(out, cap, "%s/%s/%s.%s",
                     reports_root,
                     profile->family_key,
                     target,
                     strcmp(kind, "manifest") == 0
                         ? "source-manifest.json"
                         : "native-inventory.json");
        if (n < 0 || (size_t)n >= cap) {
            out[cap - 1] = '\0';
        }
    }
}

static int qwen_source_manifest_blob_has_field(const char *blob, const char *field)
{
    char quoted[96];
    int n;

    if (!blob || !field) {
        return 0;
    }
    n = snprintf(quoted, sizeof(quoted), "\"%s\"", field);
    if (n < 0 || (size_t)n >= sizeof(quoted)) {
        return 0;
    }
    return strstr(blob, quoted) != NULL;
}

static int qwen_source_manifest_blob_has_value(const char *blob, const char *value)
{
    return blob && value && value[0] != '\0' && strstr(blob, value) != NULL;
}

static void qwen_source_probe_manifest(yvex_qwen_source_pressure_report *report,
                                       const yvex_source_family_profile *profile,
                                       const char *target)
{
    char buf[YVEX_SOURCE_MANIFEST_PROBE_CAP + 1u];
    FILE *fp;
    size_t nread;

    if (!report || !profile || !target || !report->manifest_exists ||
        report->manifest_path[0] == '\0') {
        return;
    }
    fp = fopen(report->manifest_path, "rb");
    if (!fp) {
        report->manifest_probe_error = 1;
        return;
    }
    nread = fread(buf, 1, YVEX_SOURCE_MANIFEST_PROBE_CAP, fp);
    if (ferror(fp)) {
        report->manifest_probe_error = 1;
        fclose(fp);
        return;
    }
    fclose(fp);
    buf[nread] = '\0';
    report->manifest_probe_checked = 1;
    report->manifest_has_schema =
        qwen_source_manifest_blob_has_field(buf, "schema");
    report->manifest_schema_matches =
        qwen_source_manifest_blob_has_value(buf, "yvex.source_manifest.v1");
    snprintf(report->manifest_schema_version,
             sizeof(report->manifest_schema_version),
             "%s",
             report->manifest_schema_matches ? "yvex.source_manifest.v1" : "unknown");
    report->manifest_has_family =
        qwen_source_manifest_blob_has_field(buf, "family") ||
        qwen_source_manifest_blob_has_value(buf, profile->family_key) ||
        qwen_source_manifest_blob_has_value(buf, profile->display_family);
    report->manifest_family_matches =
        qwen_source_manifest_blob_has_value(buf, profile->family_key) ||
        qwen_source_manifest_blob_has_value(buf, profile->display_family);
    report->manifest_has_target =
        qwen_source_manifest_blob_has_field(buf, "target") ||
        qwen_source_manifest_blob_has_value(buf, target);
    report->manifest_target_matches =
        qwen_source_manifest_blob_has_value(buf, target);
    report->manifest_has_artifact_class =
        qwen_source_manifest_blob_has_field(buf, "artifact_class") ||
        qwen_source_manifest_blob_has_field(buf, "source_artifact_class") ||
        qwen_source_manifest_blob_has_value(buf, profile->source_artifact_class);
    report->manifest_has_footprint =
        qwen_source_manifest_blob_has_field(buf, "footprint") ||
        qwen_source_manifest_blob_has_field(buf, "summary");
    report->manifest_has_provenance =
        qwen_source_manifest_blob_has_field(buf, "provenance") ||
        qwen_source_manifest_blob_has_field(buf, "source");
    report->manifest_has_native_inventory =
        qwen_source_manifest_blob_has_field(buf, "native_inventory") ||
        qwen_source_manifest_blob_has_field(buf, "native");
    report->manifest_has_tensor_metadata =
        qwen_source_manifest_blob_has_field(buf, "tensor_metadata") ||
        qwen_source_manifest_blob_has_field(buf, "tensors");
}

static void qwen_source_add_blocker(yvex_qwen_source_pressure_report *report,
                                    const char *blocker)
{
    if (!report || !blocker || report->blocker_count >= 32) {
        return;
    }
    report->blockers[report->blocker_count++] = blocker;
}

static unsigned long long qwen_source_tensor_print_limit(
    const yvex_qwen_source_report_options *options,
    const yvex_qwen_source_pressure_report *report)
{
    unsigned long long limit = options && options->tensor_limit
                                   ? options->tensor_limit
                                   : 20ull;

    if (!report) {
        return 0;
    }
    if (limit > YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        limit = YVEX_SOURCE_TENSOR_SAMPLE_CAP;
    }
    if (limit > report->source_tensor_sample_count) {
        limit = report->source_tensor_sample_count;
    }
    return limit;
}

static void qwen_source_print_tensor_rows(
    const yvex_qwen_source_report_options *options,
    const yvex_qwen_source_pressure_report *report)
{
    unsigned long long limit;
    unsigned long long display_limit;
    unsigned long long i;

    if (!options || !report || !options->include_tensors) {
        return;
    }
    limit = qwen_source_tensor_print_limit(options, report);
    display_limit = options->tensor_limit ? options->tensor_limit : 20ull;
    if (display_limit > YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        display_limit = YVEX_SOURCE_TENSOR_SAMPLE_CAP;
    }
    fprintf(stdout, "\nTENSORS  limit=%llu\n\n", display_limit);
    fprintf(stdout, "%-32s  %-32s  %-6s  %4s  %-18s  %8s  %8s\n",
           "NAME", "FILE", "DTYPE", "RANK", "SHAPE", "ELEMENTS", "BYTES");
    for (i = 0; i < limit; ++i) {
        const yvex_source_tensor_sample *sample = &report->source_tensor_samples[i];
        fprintf(stdout, "%-32s  %-32s  %-6s  %4llu  %-18s  %8llu  %8llu\n",
               sample->name,
               sample->file,
               sample->dtype,
               sample->rank,
               sample->shape,
               sample->elements,
               sample->declared_bytes);
    }
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

static const char *qwen_source_native_inventory_report_status(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->native_inventory_exists ? "available-report-only" : "missing";
}

static const char *qwen_source_tensor_map_report_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->tensor_map_exists) return "missing";
    return report->tensor_map_incomplete ? "incomplete-report-only" : "available-report-only";
}

static const char *qwen_source_tensor_role_map_report_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->tensor_map_exists) return "missing";
    return report->tensor_map_incomplete ? "incomplete-report-only" : "available-report-only";
}

static const char *qwen_source_output_head_map_report_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->output_head_map_exists) return "missing";
    return report->output_head_map_missing ? "missing-in-report" : "available-report-only";
}

static const char *qwen_source_tokenizer_map_report_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->tokenizer_map_exists) return "missing";
    return "available-report-only";
}

static int qwen_source_tail_blocker_is_tensor_map(const char *blocker)
{
    return blocker &&
           (strstr(blocker, "tensor-role-map") != NULL ||
            strstr(blocker, "tensor-map") != NULL);
}

static const char *qwen_source_native_inventory_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "missing";
    }
    if (report->native_safetensors_count == 0) {
        return "no-safetensors";
    }
    if (report->native_safetensors_header_error_count > 0) {
        return "header-error";
    }
    if (report->native_safetensors_header_read_count > 0) {
        return "header-only";
    }
    return "unknown";
}

static const char *qwen_source_native_inventory_source(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->source_exists ? "source-path" : "not-present";
}

static const char *qwen_source_tensor_metadata_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "missing";
    }
    if (report->native_safetensors_count == 0) {
        return "no-safetensors";
    }
    if (report->source_tensor_metadata_error_count > 0) {
        return "header-error";
    }
    if (report->source_tensor_count > 0 ||
        report->native_safetensors_header_read_count > 0) {
        return "header-only";
    }
    return "unknown";
}

static const char *qwen_source_tensor_metadata_source(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->source_exists ? "source-path" : "not-present";
}

static const char *qwen_source_native_tensor_metadata_status(
    const yvex_qwen_source_pressure_report *report)
{
    const char *status = qwen_source_native_inventory_status(report);

    if (strcmp(status, "header-only") == 0) {
        return "header-only";
    }
    if (strcmp(status, "header-error") == 0) {
        return report && report->native_tensor_count > 0
                   ? "partial-header-only"
                   : "header-error";
    }
    if (strcmp(status, "missing") == 0 ||
        strcmp(status, "no-safetensors") == 0) {
        return "not-present";
    }
    return "unknown";
}

static const char *qwen_source_native_tensor_payload_status(
    const yvex_qwen_source_pressure_report *report)
{
    const char *status = qwen_source_native_inventory_status(report);

    if (strcmp(status, "missing") == 0 ||
        strcmp(status, "no-safetensors") == 0) {
        return "not-present";
    }
    return "not-loaded";
}

static const char *qwen_source_sidecar_status(const yvex_qwen_source_pressure_report *report)
{
    int has_config;
    int has_tokenizer;

    if (!report || !report->source_exists) {
        return "missing";
    }
    has_config = report->config_exists || report->generation_config_exists;
    has_tokenizer = report->tokenizer_json_exists || report->tokenizer_config_exists;
    if (has_config && has_tokenizer) {
        return "present";
    }
    if (has_config || has_tokenizer) {
        return "partial";
    }
    return "missing";
}

static const char *qwen_source_tensor_payload_status(const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists || report->safetensors_count == 0) {
        return "not-present";
    }
    return "present-not-loaded";
}

static const char *qwen_source_target_artifact_status(const yvex_source_family_profile *profile)
{
    (void)profile;
    return "planned";
}

static const char *qwen_source_footprint_class(const yvex_qwen_source_pressure_report *report)
{
    const unsigned long long mib = 1024ULL * 1024ULL;
    const unsigned long long gib = 1024ULL * mib;

    if (!report || !report->source_exists) {
        return "missing";
    }
    if (report->source_regular_file_count == 0) {
        return "empty";
    }
    if (report->total_size_bytes < 100ULL * mib) {
        return "tiny";
    }
    if (report->total_size_bytes < 5ULL * gib) {
        return "small";
    }
    if (report->total_size_bytes < 30ULL * gib) {
        return "medium";
    }
    if (report->total_size_bytes < 200ULL * gib) {
        return "large";
    }
    return "huge";
}

static const char *qwen_source_footprint_status(const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "missing";
    }
    return "report-only";
}

static const char *qwen_source_provenance_origin_normal(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->source_exists ? "local-path" : "planned-official";
}

static const char *qwen_source_provenance_origin_audit(
    const yvex_qwen_source_report_options *options,
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "planned-official";
    }
    if (options && options->source) {
        return "explicit-source-path";
    }
    return "configured-models-root";
}

static const char *qwen_source_provenance_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "missing";
    }
    return "local-unverified";
}

static const char *qwen_source_identity_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->source_exists) {
        return "not-present";
    }
    if (report->source_identity_from_download_sidecar) {
        return "download-sidecar";
    }
    if (report->source_identity_from_path) {
        return "inferred-from-path";
    }
    return "not-verified";
}

static const char *qwen_source_authority(const yvex_qwen_source_pressure_report *report)
{
    return report && report->source_exists
               ? "local-unverified"
               : "upstream-official-planned";
}

static const char *qwen_source_authority_status(const yvex_qwen_source_pressure_report *report)
{
    return report && report->source_exists ? "local-unverified" : "planned";
}

static const char *qwen_source_manifest_provenance_status(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->manifest_exists ? "manifest-present" : "manifest-missing";
}

static const char *qwen_source_manifest_authority(
    const yvex_qwen_source_pressure_report *report)
{
    return report && report->manifest_exists ? "local-unverified" : "unknown";
}

static const char *qwen_source_manifest_schema_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->manifest_exists) {
        return "not-checked";
    }
    if (report->manifest_probe_error) {
        return "unreadable";
    }
    if (report->manifest_schema_matches) {
        return "matched";
    }
    if (report->manifest_has_schema) {
        return "present-unrecognized";
    }
    return "not-declared";
}

static const char *qwen_source_manifest_match_status(
    const yvex_qwen_source_pressure_report *report,
    int has_field,
    int matches)
{
    if (!report || !report->manifest_exists) {
        return "not-checked";
    }
    if (report->manifest_probe_error) {
        return "unreadable";
    }
    if (matches) {
        return "matched";
    }
    if (has_field) {
        return "mismatch";
    }
    return "not-declared";
}

static const char *qwen_source_manifest_decl_status(
    const yvex_qwen_source_pressure_report *report,
    int present)
{
    if (!report || !report->manifest_exists) {
        return "not-checked";
    }
    if (report->manifest_probe_error) {
        return "unreadable";
    }
    return present ? "declared" : "not-declared";
}

static const char *qwen_source_manifest_family_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_match_status(report,
                                            report ? report->manifest_has_family : 0,
                                            report ? report->manifest_family_matches : 0);
}

static const char *qwen_source_manifest_target_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_match_status(report,
                                            report ? report->manifest_has_target : 0,
                                            report ? report->manifest_target_matches : 0);
}

static const char *qwen_source_manifest_artifact_class_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_decl_status(report,
                                           report ? report->manifest_has_artifact_class : 0);
}

static const char *qwen_source_manifest_footprint_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_decl_status(report,
                                           report ? report->manifest_has_footprint : 0);
}

static const char *qwen_source_manifest_native_inventory_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_decl_status(report,
                                           report ? report->manifest_has_native_inventory : 0);
}

static const char *qwen_source_manifest_tensor_metadata_status(
    const yvex_qwen_source_pressure_report *report)
{
    return qwen_source_manifest_decl_status(report,
                                           report ? report->manifest_has_tensor_metadata : 0);
}

static const char *qwen_source_manifest_consistency_status(
    const yvex_qwen_source_pressure_report *report)
{
    if (!report || !report->manifest_exists) {
        return "not-checked";
    }
    if (report->manifest_probe_error) {
        return "report-only";
    }
    if (report->manifest_schema_matches &&
        report->manifest_family_matches &&
        report->manifest_target_matches) {
        return "partial";
    }
    return "report-only";
}

static const char *qwen_source_presence_verification_status(int present)
{
    return present ? "present-unverified" : "not-present";
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
    snprintf(report->identity_target_id, sizeof(report->identity_target_id),
             "%s", options->target);
    snprintf(report->identity_family, sizeof(report->identity_family),
             "%s", options->profile->family_key);
    if (options->source) {
        const char *base = qwen_source_path_basename(report->source_path);
        if (base && strcmp(base, options->target) == 0 &&
            qwen_source_target_matches_family_name(options->profile->family_key, base)) {
            report->source_identity_from_path = 1;
        }
    }
    if (!report->identity_model[0] &&
        strcmp(options->target, options->profile->target_id) != 0) {
        snprintf(report->identity_model, sizeof(report->identity_model), "%s",
                 options->target);
    }
    {
        char registry_family_dir[YVEX_PATH_CAP];
        char reports_family_dir[YVEX_PATH_CAP];
        char file_name[192];

        if (qwen_source_path_format(registry_family_dir, sizeof(registry_family_dir),
                                    "%s/%s",
                                    operator_paths.registry_root,
                                    options->profile->family_key) &&
            qwen_source_path_format(reports_family_dir, sizeof(reports_family_dir),
                                    "%s/%s",
                                    operator_paths.reports_root,
                                    options->profile->family_key)) {
            snprintf(file_name, sizeof(file_name), "%s.download.json", options->target);
            (void)qwen_source_path_format(report->download_registry_path,
                                          sizeof(report->download_registry_path),
                                          "%s/%s", registry_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.download-report.json", options->target);
            (void)qwen_source_path_format(report->download_report_path,
                                          sizeof(report->download_report_path),
                                          "%s/%s", reports_family_dir, file_name);
            report->download_registry_exists =
                report->download_registry_path[0] &&
                access(report->download_registry_path, F_OK) == 0;
            report->download_report_exists =
                report->download_report_path[0] &&
                access(report->download_report_path, F_OK) == 0;
            snprintf(file_name, sizeof(file_name), "%s.tensor-map.json", options->target);
            (void)qwen_source_path_format(report->tensor_map_path,
                                          sizeof(report->tensor_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.output-head-map.json", options->target);
            (void)qwen_source_path_format(report->output_head_map_path,
                                          sizeof(report->output_head_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            snprintf(file_name, sizeof(file_name), "%s.tokenizer-map.json", options->target);
            (void)qwen_source_path_format(report->tokenizer_map_path,
                                          sizeof(report->tokenizer_map_path),
                                          "%s/%s", reports_family_dir, file_name);
            report->tensor_map_exists =
                report->tensor_map_path[0] &&
                access(report->tensor_map_path, F_OK) == 0;
            report->output_head_map_exists =
                report->output_head_map_path[0] &&
                access(report->output_head_map_path, F_OK) == 0;
            report->tokenizer_map_exists =
                report->tokenizer_map_path[0] &&
                access(report->tokenizer_map_path, F_OK) == 0;
            qwen_source_probe_map_sidecars(report);
            if (report->download_registry_exists) {
                (void)qwen_source_probe_download_identity_file(
                    report->download_registry_path,
                    options->target,
                    options->profile->family_key,
                    report);
            }
            if (!report->source_identity_from_download_sidecar &&
                report->download_report_exists) {
                (void)qwen_source_probe_download_identity_file(
                    report->download_report_path,
                    options->target,
                    options->profile->family_key,
                    report);
            }
        }
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
        report->readme_exists = qwen_source_check_file(report->source_path, "README.md");
        report->license_exists =
            qwen_source_check_file(report->source_path, "LICENSE") ||
            qwen_source_check_file(report->source_path, "LICENSE.txt") ||
            qwen_source_check_file(report->source_path, "COPYING");
        qwen_source_scan_top_footprint(report->source_path, report);
        rc = qwen_source_scan_native_inventory(report->source_path, report);
        if (rc != 0) {
            return rc;
        }
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
    qwen_source_probe_manifest(report, options->profile, options->target);

    report->source_state = report->source_exists ? "present" : "missing";
    if (!report->source_exists) {
        report->status = "source-target-profiled";
        report->top_blocker = options->profile->source_manifest_blocker;
        report->next_row = options->profile->model_class_next;
    } else if (!report->manifest_exists) {
        report->status = "source-present-report-only";
        report->top_blocker = options->profile->source_manifest_blocker;
        report->next_row = options->profile->model_class_next;
    } else {
        report->status = "source-profile-incomplete";
        report->top_blocker = options->profile->model_class_blocker;
        report->next_row = options->profile->model_class_next;
    }
    if (report->source_exists &&
        report->tensor_map_exists &&
        !report->tensor_map_incomplete &&
        report->output_head_map_exists &&
        !report->output_head_map_missing) {
        if (report->tokenizer_map_exists) {
            report->top_blocker = "quant-policy-or-artifact-emitter";
            report->next_row = "V010.QUANT.1";
        } else {
            report->top_blocker = options->profile->tokenizer_blocker;
            report->next_row = "V010.MAP.7";
        }
    }

    if (!report->source_exists) {
        qwen_source_add_blocker(report, options->profile->source_path_blocker);
    }
    if (!report->manifest_exists) {
        qwen_source_add_blocker(report, options->profile->source_manifest_blocker);
    }
    if (!report->config_exists) {
        qwen_source_add_blocker(report, options->profile->source_config_blocker);
    }
    if (!(report->tokenizer_json_exists || report->tokenizer_config_exists)) {
        qwen_source_add_blocker(report, options->profile->tokenizer_blocker);
    }
    for (i = 0; i < options->profile->tail_blocker_count; ++i) {
        if (report->source_exists &&
            report->tensor_map_exists &&
            !report->tensor_map_incomplete &&
            qwen_source_tail_blocker_is_tensor_map(
                options->profile->tail_blockers[i])) {
            continue;
        }
        qwen_source_add_blocker(report, options->profile->tail_blockers[i]);
    }
    return 0;
}

static void qwen_source_print_normal(const yvex_qwen_source_report_options *options,
                                     const yvex_qwen_source_pressure_report *report)
{
    fprintf(stdout, "report: %s\n", options->profile->report_name);
    fprintf(stdout, "status: %s\n", report->status);
    fprintf(stdout, "family: %s\n", options->profile->family_key);
    fprintf(stdout, "target: %s\n", options->target);
    fprintf(stdout, "source: %s  status=%s\n",
           options->profile->source_artifact_class,
           report->source_state);
    fprintf(stdout, "artifact: %s  status=%s\n",
           options->profile->target_artifact_class,
           qwen_source_target_artifact_status(options->profile));
    fprintf(stdout, "files: %llu  safetensors: %llu  bytes: %llu  footprint: %s\n",
           report->source_file_count,
           report->safetensors_count,
           report->total_size_bytes,
           qwen_source_footprint_class(report));
    fprintf(stdout, "provenance: %s status=%s revision=unknown\n",
           qwen_source_provenance_origin_normal(report),
           qwen_source_provenance_status(report));
    fprintf(stdout, "native: %s  files=%llu  tensors=%llu  header_bytes=%llu\n",
           qwen_source_native_inventory_status(report),
           report->native_safetensors_count,
           report->native_tensor_count,
           report->native_safetensors_header_bytes);
    fprintf(stdout, "metadata: %s  tensors=%llu  dtypes=%llu  max_rank=%llu\n",
           qwen_source_tensor_metadata_status(report),
           report->source_tensor_count,
           report->source_tensor_dtype_count,
           report->source_tensor_max_rank);
    fprintf(stdout, "manifest: %s  consistency=%s\n",
           qwen_source_manifest_status(report),
           qwen_source_manifest_consistency_status(report));
    fprintf(stdout, "top_blocker: %s\n", report->top_blocker);
    fprintf(stdout, "next: %s\n", report->next_row);
    fprintf(stdout, "boundary: source report only; no artifact/runtime/generation/benchmark\n");
    qwen_source_print_tensor_rows(options, report);
}

static void qwen_source_print_table(const yvex_qwen_source_report_options *options,
                                    const yvex_qwen_source_pressure_report *report)
{
    fprintf(stdout, "SOURCE PRESSURE  release=%s\n\n", options->release);
    fprintf(stdout, "%-6s  %-24s  %-7s  %7s  %-8s  %-11s  %s\n",
           "FAMILY", "TARGET", "SOURCE", "TENSORS", "MANIFEST",
           "CONSISTENCY", "NEXT");
    fprintf(stdout, "%-6s  %-24s  %-7s  %7llu  %-8s  %-11s  %s\n",
           options->profile->family_key,
           options->target,
           report->source_state,
           report->source_tensor_count,
           qwen_source_manifest_status(report),
           qwen_source_manifest_consistency_status(report),
           report->next_row);
    qwen_source_print_tensor_rows(options, report);
}

static void qwen_source_print_audit(const yvex_qwen_source_report_options *options,
                                    const yvex_qwen_source_pressure_report *report)
{
    unsigned long i;

    fprintf(stdout, "source-report: %s\n", options->profile->family_key);
    fprintf(stdout, "status: %s\n", report->status);
    fprintf(stdout, "release: %s\n", options->release);
    fprintf(stdout, "family: %s\n", options->profile->display_family);
    fprintf(stdout, "family_key: %s\n", options->profile->family_key);
    fprintf(stdout, "model: %s\n", qwen_source_model_name(options, report));
    fprintf(stdout, "target_id: %s\n", options->target);
    fprintf(stdout, "target_class: %s\n", options->profile->target_class);
    fprintf(stdout, "source_target_status: %s\n", options->profile->source_target_status);
    fprintf(stdout, "source_family_profile_status: %s\n",
           options->profile->source_family_profile_status);
    fprintf(stdout, "source_artifact_class: %s\n", options->profile->source_artifact_class);
    fprintf(stdout, "source_artifact_status: %s\n", report->source_state);
    fprintf(stdout, "source_artifact_format: %s\n", options->profile->source_artifact_format);
    fprintf(stdout, "source_artifact_origin: %s\n", options->profile->source_artifact_origin);
    fprintf(stdout, "source_artifact_authority: %s\n",
           options->profile->source_artifact_authority);
    fprintf(stdout, "source_sidecar_status: %s\n", qwen_source_sidecar_status(report));
    fprintf(stdout, "source_tensor_container: %s\n", options->profile->source_tensor_container);
    fprintf(stdout, "source_tensor_payload_status: %s\n",
           qwen_source_tensor_payload_status(report));
    fprintf(stdout, "target_artifact_class: %s\n", options->profile->target_artifact_class);
    fprintf(stdout, "target_artifact_status: %s\n",
           qwen_source_target_artifact_status(options->profile));
    fprintf(stdout, "target_artifact_origin: %s\n", options->profile->target_artifact_origin);
    fprintf(stdout, "target_artifact_required: %s\n",
           options->profile->target_artifact_required);
    fprintf(stdout, "external_reference_status: %s\n",
           options->profile->external_reference_status);
    fprintf(stdout, "yvex_produced_artifact_status: %s\n",
           options->profile->yvex_produced_artifact_status);
    fprintf(stdout, "pressure_purpose: %s\n", options->profile->pressure_purpose);
    fprintf(stdout, "runtime_shape: %s\n", options->profile->runtime_shape);
    fprintf(stdout, "hardware_lane: %s\n", options->profile->hardware_lane);
    fprintf(stdout, "backend_lane: %s\n", options->profile->backend_lane);
    fprintf(stdout, "source_class: %s\n", options->profile->source_class);
    fprintf(stdout, "source_provenance_status: %s\n", qwen_source_provenance_status(report));
    fprintf(stdout, "source_origin: %s\n", qwen_source_provenance_origin_audit(options, report));
    fprintf(stdout, "source_authority: %s\n", qwen_source_authority(report));
    fprintf(stdout, "source_authority_status: %s\n", qwen_source_authority_status(report));
    fprintf(stdout, "source_path: %s\n", report->source_path);
    fprintf(stdout, "source_path_source: %s\n", report->source_path_source);
    fprintf(stdout, "source_path_status: %s\n", report->source_state);
    fprintf(stdout, "source_exists: %s\n", report->source_exists ? "true" : "false");
    fprintf(stdout, "download_registry_path: %s\n",
           report->download_registry_path[0] ? report->download_registry_path : "unknown");
    fprintf(stdout, "download_registry_status: %s\n",
           report->download_registry_exists ? "present" : "missing");
    fprintf(stdout, "download_report_path: %s\n",
           report->download_report_path[0] ? report->download_report_path : "unknown");
    fprintf(stdout, "download_report_status: %s\n",
           report->download_report_exists ? "present" : "missing");
    fprintf(stdout, "download_repo_id: %s\n",
           report->identity_repo_id[0] ? report->identity_repo_id : "unknown");
    fprintf(stdout, "download_revision: %s\n",
           report->identity_revision[0] ? report->identity_revision : "unknown");
    fprintf(stdout, "source_file_count: %llu\n", report->source_file_count);
    fprintf(stdout, "source_regular_file_count: %llu\n", report->source_regular_file_count);
    fprintf(stdout, "source_safetensors_count: %llu\n", report->safetensors_count);
    fprintf(stdout, "source_bin_count: %llu\n", report->bin_count);
    fprintf(stdout, "source_dat_count: %llu\n", report->dat_count);
    fprintf(stdout, "source_json_count: %llu\n", report->json_count);
    fprintf(stdout, "source_tokenizer_file_count: %llu\n", report->tokenizer_file_count);
    fprintf(stdout, "source_config_file_count: %llu\n", report->config_file_count);
    fprintf(stdout, "source_total_size_bytes: %llu\n", report->total_size_bytes);
    fprintf(stdout, "source_safetensors_size_bytes: %llu\n", report->safetensors_size_bytes);
    fprintf(stdout, "source_sidecar_size_bytes: %llu\n", report->sidecar_size_bytes);
    fprintf(stdout, "source_other_size_bytes: %llu\n", report->other_size_bytes);
    fprintf(stdout, "source_footprint_class: %s\n", qwen_source_footprint_class(report));
    fprintf(stdout, "source_footprint_status: %s\n", qwen_source_footprint_status(report));
    fprintf(stdout, "source_count_scope: top-level-regular-files\n");
    fprintf(stdout, "source_payload_loaded: false\n");
    fprintf(stdout, "largest_source_file_bytes: %llu\n", report->largest_source_file_bytes);
    fprintf(stdout, "largest_source_file_name: %s\n",
           report->largest_source_file_name[0]
               ? report->largest_source_file_name
               : "none");
    fprintf(stdout, "config_status: %s\n", qwen_source_present_missing(report->config_exists));
    fprintf(stdout, "tokenizer_status: %s\n", qwen_source_tokenizer_status(report));
    fprintf(stdout, "generation_config_status: %s\n",
           qwen_source_present_missing(report->generation_config_exists));
    fprintf(stdout, "safetensors_status: %s\n", qwen_source_safetensors_status(report));
    fprintf(stdout, "safetensors_count: %llu\n", report->safetensors_count);
    fprintf(stdout, "source_manifest_expected: true\n");
    fprintf(stdout, "source_manifest_status: %s\n", qwen_source_manifest_status(report));
    fprintf(stdout, "source_manifest_path: %s\n",
           report->manifest_path[0] ? report->manifest_path : "unknown");
    fprintf(stdout, "source_manifest_schema_status: %s\n",
           qwen_source_manifest_schema_status(report));
    fprintf(stdout, "source_manifest_schema_version: %s\n",
           report->manifest_schema_version[0]
               ? report->manifest_schema_version
               : "unknown");
    fprintf(stdout, "source_manifest_family: %s\n", options->profile->family_key);
    fprintf(stdout, "source_manifest_family_status: %s\n",
           qwen_source_manifest_family_status(report));
    fprintf(stdout, "source_manifest_target_id: %s\n", options->target);
    fprintf(stdout, "source_manifest_target_status: %s\n",
           qwen_source_manifest_target_status(report));
    fprintf(stdout, "source_manifest_source_path_status: %s\n", report->source_state);
    fprintf(stdout, "source_manifest_artifact_class_status: %s\n",
           qwen_source_manifest_artifact_class_status(report));
    fprintf(stdout, "source_manifest_footprint_status: %s\n",
           qwen_source_manifest_footprint_status(report));
    fprintf(stdout, "source_manifest_authority: %s\n",
           qwen_source_manifest_authority(report));
    fprintf(stdout, "source_manifest_provenance_status: %s\n",
           qwen_source_manifest_provenance_status(report));
    fprintf(stdout, "source_manifest_native_inventory_status: %s\n",
           qwen_source_manifest_native_inventory_status(report));
    fprintf(stdout, "source_manifest_tensor_metadata_status: %s\n",
           qwen_source_manifest_tensor_metadata_status(report));
    fprintf(stdout, "source_manifest_consistency_status: %s\n",
           qwen_source_manifest_consistency_status(report));
    fprintf(stdout, "source_manifest_hardening_status: report-only\n");
    fprintf(stdout, "source_manifest_creation_performed: false\n");
    fprintf(stdout, "source_manifest_payload_loaded: false\n");
    fprintf(stdout, "source_manifest_remote_checked: false\n");
    fprintf(stdout, "source_manifest_hash_computed: false\n");
    fprintf(stdout, "source_revision: unknown\n");
    fprintf(stdout, "source_revision_status: unknown\n");
    fprintf(stdout, "source_commit: unknown\n");
    fprintf(stdout, "source_commit_status: unknown\n");
    fprintf(stdout, "source_tag: unknown\n");
    fprintf(stdout, "source_tag_status: unknown\n");
    fprintf(stdout, "source_license_status: %s\n",
           qwen_source_presence_verification_status(report->license_exists));
    fprintf(stdout, "source_readme_status: %s\n",
           qwen_source_presence_verification_status(report->readme_exists));
    fprintf(stdout, "source_identity_status: %s\n",
           qwen_source_identity_status(report));
    fprintf(stdout, "source_digest_status: not-computed\n");
    fprintf(stdout, "source_hash_status: not-computed\n");
    fprintf(stdout, "source_verification_status: not-verified\n");
    fprintf(stdout, "source_remote_checked: false\n");
    fprintf(stdout, "native_inventory_status: %s\n",
           qwen_source_native_inventory_status(report));
    fprintf(stdout, "native_inventory_scope: top-level-safetensors-headers\n");
    fprintf(stdout, "native_inventory_source: %s\n",
           qwen_source_native_inventory_source(report));
    fprintf(stdout, "native_safetensors_count: %llu\n", report->native_safetensors_count);
    fprintf(stdout, "native_safetensors_opened: %llu\n", report->native_safetensors_opened);
    fprintf(stdout, "native_safetensors_header_read_count: %llu\n",
           report->native_safetensors_header_read_count);
    fprintf(stdout, "native_safetensors_header_error_count: %llu\n",
           report->native_safetensors_header_error_count);
    fprintf(stdout, "native_safetensors_header_bytes: %llu\n",
           report->native_safetensors_header_bytes);
    fprintf(stdout, "native_safetensors_payload_loaded: false\n");
    fprintf(stdout, "native_safetensors_payload_bytes_read: 0\n");
    fprintf(stdout, "native_tensor_count: %llu\n", report->native_tensor_count);
    fprintf(stdout, "native_tensor_metadata_status: %s\n",
           qwen_source_native_tensor_metadata_status(report));
    fprintf(stdout, "native_tensor_payload_status: %s\n",
           qwen_source_native_tensor_payload_status(report));
    fprintf(stdout, "native_declared_data_bytes: %llu\n",
           report->native_declared_data_bytes);
    fprintf(stdout, "native_declared_tensor_bytes: %llu\n",
           report->native_declared_tensor_bytes);
    fprintf(stdout, "native_max_rank: %llu\n", report->native_max_rank);
    fprintf(stdout, "native_max_tensor_elements: %llu\n",
           report->native_max_tensor_elements);
    fprintf(stdout, "native_largest_tensor_name: %s\n",
           report->native_largest_tensor_name[0]
               ? report->native_largest_tensor_name
               : "none");
    fprintf(stdout, "native_largest_tensor_bytes: %llu\n",
           report->native_largest_tensor_bytes);
    fprintf(stdout, "native_dtype_f16_count: %llu\n", report->native_dtype_f16_count);
    fprintf(stdout, "native_dtype_bf16_count: %llu\n", report->native_dtype_bf16_count);
    fprintf(stdout, "native_dtype_f32_count: %llu\n", report->native_dtype_f32_count);
    fprintf(stdout, "native_dtype_i8_count: %llu\n", report->native_dtype_i8_count);
    fprintf(stdout, "native_dtype_i16_count: %llu\n", report->native_dtype_i16_count);
    fprintf(stdout, "native_dtype_i32_count: %llu\n", report->native_dtype_i32_count);
    fprintf(stdout, "native_dtype_i64_count: %llu\n", report->native_dtype_i64_count);
    fprintf(stdout, "native_dtype_u8_count: %llu\n", report->native_dtype_u8_count);
    fprintf(stdout, "native_dtype_other_count: %llu\n", report->native_dtype_other_count);
    fprintf(stdout, "native_invalid_file_count: %llu\n", report->native_invalid_file_count);
    fprintf(stdout, "native_inventory_error_count: %llu\n",
           report->native_inventory_error_count);
    fprintf(stdout, "native_inventory_report_status: %s\n",
           qwen_source_native_inventory_report_status(report));
    fprintf(stdout, "native_inventory_path: %s\n",
           report->native_inventory_path[0] ? report->native_inventory_path : "unknown");
    fprintf(stdout, "source_tensor_metadata_status: %s\n",
           qwen_source_tensor_metadata_status(report));
    fprintf(stdout, "source_tensor_metadata_scope: safetensors-header\n");
    fprintf(stdout, "source_tensor_metadata_source: %s\n",
           qwen_source_tensor_metadata_source(report));
    fprintf(stdout, "source_tensor_metadata_payload_loaded: false\n");
    fprintf(stdout, "source_tensor_metadata_payload_bytes_read: 0\n");
    fprintf(stdout, "source_tensor_count: %llu\n", report->source_tensor_count);
    fprintf(stdout, "source_tensor_name_count: %llu\n", report->source_tensor_name_count);
    fprintf(stdout, "source_tensor_file_count: %llu\n", report->source_tensor_file_count);
    fprintf(stdout, "source_tensor_dtype_count: %llu\n", report->source_tensor_dtype_count);
    fprintf(stdout, "source_tensor_rank_count: %llu\n", report->source_tensor_rank_count);
    fprintf(stdout, "source_tensor_shape_count: %llu\n", report->source_tensor_shape_count);
    fprintf(stdout, "source_tensor_declared_data_bytes: %llu\n",
           report->source_tensor_declared_data_bytes);
    fprintf(stdout, "source_tensor_declared_tensor_bytes: %llu\n",
           report->source_tensor_declared_tensor_bytes);
    fprintf(stdout, "source_tensor_total_elements: %llu\n",
           report->source_tensor_total_elements);
    fprintf(stdout, "source_tensor_max_rank: %llu\n", report->source_tensor_max_rank);
    fprintf(stdout, "source_tensor_max_elements: %llu\n", report->source_tensor_max_elements);
    fprintf(stdout, "source_tensor_largest_name: %s\n",
           report->source_tensor_largest_name[0]
               ? report->source_tensor_largest_name
               : "none");
    fprintf(stdout, "source_tensor_largest_file: %s\n",
           report->source_tensor_largest_file[0]
               ? report->source_tensor_largest_file
               : "none");
    fprintf(stdout, "source_tensor_largest_dtype: %s\n",
           report->source_tensor_largest_dtype[0]
               ? report->source_tensor_largest_dtype
               : "none");
    fprintf(stdout, "source_tensor_largest_rank: %llu\n",
           report->source_tensor_largest_rank);
    fprintf(stdout, "source_tensor_largest_shape: %s\n",
           report->source_tensor_largest_shape[0]
               ? report->source_tensor_largest_shape
               : "[]");
    fprintf(stdout, "source_tensor_largest_elements: %llu\n",
           report->source_tensor_largest_elements);
    fprintf(stdout, "source_tensor_largest_declared_bytes: %llu\n",
           report->source_tensor_largest_declared_bytes);
    fprintf(stdout, "source_tensor_dtype_f16_count: %llu\n",
           report->source_tensor_dtype_f16_count);
    fprintf(stdout, "source_tensor_dtype_bf16_count: %llu\n",
           report->source_tensor_dtype_bf16_count);
    fprintf(stdout, "source_tensor_dtype_f32_count: %llu\n",
           report->source_tensor_dtype_f32_count);
    fprintf(stdout, "source_tensor_dtype_i8_count: %llu\n",
           report->source_tensor_dtype_i8_count);
    fprintf(stdout, "source_tensor_dtype_i16_count: %llu\n",
           report->source_tensor_dtype_i16_count);
    fprintf(stdout, "source_tensor_dtype_i32_count: %llu\n",
           report->source_tensor_dtype_i32_count);
    fprintf(stdout, "source_tensor_dtype_i64_count: %llu\n",
           report->source_tensor_dtype_i64_count);
    fprintf(stdout, "source_tensor_dtype_u8_count: %llu\n",
           report->source_tensor_dtype_u8_count);
    fprintf(stdout, "source_tensor_dtype_other_count: %llu\n",
           report->source_tensor_dtype_other_count);
    fprintf(stdout, "source_tensor_rank_0_count: %llu\n",
           report->source_tensor_rank_0_count);
    fprintf(stdout, "source_tensor_rank_1_count: %llu\n",
           report->source_tensor_rank_1_count);
    fprintf(stdout, "source_tensor_rank_2_count: %llu\n",
           report->source_tensor_rank_2_count);
    fprintf(stdout, "source_tensor_rank_3_count: %llu\n",
           report->source_tensor_rank_3_count);
    fprintf(stdout, "source_tensor_rank_4_count: %llu\n",
           report->source_tensor_rank_4_count);
    fprintf(stdout, "source_tensor_rank_other_count: %llu\n",
           report->source_tensor_rank_other_count);
    fprintf(stdout, "source_tensor_name_pattern_status: lexical-only\n");
    fprintf(stdout, "source_tensor_name_embed_count: %llu\n",
           report->source_tensor_name_embed_count);
    fprintf(stdout, "source_tensor_name_attn_count: %llu\n",
           report->source_tensor_name_attn_count);
    fprintf(stdout, "source_tensor_name_mlp_count: %llu\n",
           report->source_tensor_name_mlp_count);
    fprintf(stdout, "source_tensor_name_norm_count: %llu\n",
           report->source_tensor_name_norm_count);
    fprintf(stdout, "source_tensor_name_lm_head_count: %llu\n",
           report->source_tensor_name_lm_head_count);
    fprintf(stdout, "source_tensor_name_other_count: %llu\n",
           report->source_tensor_name_other_count);
    fprintf(stdout, "source_tensor_metadata_error_count: %llu\n",
           report->source_tensor_metadata_error_count);
    fprintf(stdout, "source_tensor_sample_count: %llu\n",
           report->source_tensor_sample_count);
    for (i = 0; i < report->source_tensor_sample_count; ++i) {
        const yvex_source_tensor_sample *sample = &report->source_tensor_samples[i];
        fprintf(stdout, "source_tensor_%lu_name: %s\n", i, sample->name);
        fprintf(stdout, "source_tensor_%lu_file: %s\n", i, sample->file);
        fprintf(stdout, "source_tensor_%lu_dtype: %s\n", i, sample->dtype);
        fprintf(stdout, "source_tensor_%lu_rank: %llu\n", i, sample->rank);
        fprintf(stdout, "source_tensor_%lu_shape: %s\n", i, sample->shape);
        fprintf(stdout, "source_tensor_%lu_elements: %llu\n", i, sample->elements);
        fprintf(stdout, "source_tensor_%lu_declared_bytes: %llu\n",
               i,
               sample->declared_bytes);
    }
    fprintf(stdout, "model_class_profile_status: %s\n",
           strcmp(options->profile->family_key, "qwen") == 0 ||
           strcmp(options->profile->family_key, "gemma") == 0
               ? "command-visible"
               : "missing");
    fprintf(stdout, "tensor_map_path: %s\n",
           report->tensor_map_path[0] ? report->tensor_map_path : "unknown");
    fprintf(stdout, "tensor_map_status: %s\n",
           qwen_source_tensor_map_report_status(report));
    fprintf(stdout, "tensor_role_map_status: %s\n",
           qwen_source_tensor_role_map_report_status(report));
    fprintf(stdout, "output_head_map_path: %s\n",
           report->output_head_map_path[0] ? report->output_head_map_path : "unknown");
    fprintf(stdout, "output_head_map_status: %s\n",
           qwen_source_output_head_map_report_status(report));
    fprintf(stdout, "tokenizer_map_path: %s\n",
           report->tokenizer_map_path[0] ? report->tokenizer_map_path : "unknown");
    fprintf(stdout, "tokenizer_map_status: %s\n",
           qwen_source_tokenizer_map_report_status(report));
    fprintf(stdout, "artifact_status: missing\n");
    fprintf(stdout, "runtime_claim: unsupported\n");
    fprintf(stdout, "generation: unsupported-full-model\n");
    fprintf(stdout, "benchmark_status: not-measured\n");
    fprintf(stdout, "release_ready: false\n");
    fprintf(stdout, "blocker_count: %lu\n", report->blocker_count);
    for (i = 0; i < report->blocker_count; ++i) {
        fprintf(stdout, "blocker_%lu: %s\n", i, report->blockers[i]);
    }
    fprintf(stdout, "next_required_rows: %s\n", report->next_row);
    fprintf(stdout, "boundary: source report only; no artifact/runtime/generation/benchmark\n");
}

static void qwen_source_report_usage(FILE *fp)
{
    fprintf(fp, "usage: " "yvex source-manifest report --family qwen|gemma --release v0.1.0 [options]\n");
}

static void qwen_source_report_help(FILE *fp)
{
    qwen_source_report_usage(fp);
    fprintf(fp, "\nOptions:\n");
    fprintf(fp, "  --family qwen|gemma\n");
    fprintf(fp, "  --release v0.1.0\n");
    fprintf(fp, "  --models-root DIR\n");
    fprintf(fp, "  --source DIR\n");
    fprintf(fp, "  --target qwen3-8b|qwen-small|qwen-medium|gemma-4-12b-it\n");
    fprintf(fp, "  --" "include-files --" "include-config --" "include-blockers --" "include-next\n");
    fprintf(fp, "  --" "include-tensors [--tensor-limit N]\n");
    fprintf(fp, "  --" "audit | --" "output normal|table|audit\n\n");
    fprintf(fp, "Report fields include source artifact class, target artifact class, source footprint, and source provenance evidence.\n");
    fprintf(fp, "Source footprint reports count top-level regular files and bytes without loading tensor payloads.\n");
    fprintf(fp, "Source provenance fields classify local/planned state only; they do not verify upstream identity, hash files, or prove source readiness.\n");
    fprintf(fp, "Native safetensors inventory reads safetensors headers only and never loads tensor payload bytes.\n");
    fprintf(fp, "Source tensor metadata inventory is derived from safetensors headers only and does not map tensors to runtime roles.\n");
    fprintf(fp, "Source manifest hardening is shallow/report-only; it does not create manifests, check remotes, hash files, load payloads, or prove source readiness.\n");
    fprintf(fp, "The source pressure report inspects source-path readiness only. It does not download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}

/*
 * yvex_source_manifest_report_command()
 *
 * Purpose:
 *   parse and render Qwen/Gemma source pressure report commands.
 *
 * Inputs:
 *   arg_count/args are borrowed CLI arguments.
 *
 * Effects:
 *   resolves local source/report paths, reads bounded metadata/header evidence,
 *   builds report-only source facts, and prints normal/table/audit output.
 *
 * Failure:
 *   returns parser failures for invalid options and report-builder failures for
 *   malformed local source evidence.
 *
 * Boundary:
 *   source pressure reports do not download, verify, hash payloads, emit GGUF,
 *   materialize tensors, execute runtime paths, generate, evaluate, benchmark,
 *   or mark release ready.
 */
int yvex_source_manifest_report_command(int arg_count, char **args)
{
    yvex_qwen_source_report_options options;
    yvex_qwen_source_pressure_report report;
    int i;
    int rc;

    memset(&options, 0, sizeof(options));
    options.output_mode = YVEX_SOURCE_REPORT_OUTPUT_NORMAL;

    if (arg_count == 4 && (strcmp(args[3], "--help") == 0 || strcmp(args[3], "-h") == 0)) {
        qwen_source_report_help(stdout);
        return 0;
    }

    for (i = 3; i < arg_count; ++i) {
        if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            qwen_source_report_help(stdout);
            return 0;
        } else if (strcmp(args[i], "--family") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --family requires qwen|gemma\n");
                return 2;
            }
            options.family = args[++i];
        } else if (strcmp(args[i], "--release") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --release requires VERSION\n");
                return 2;
            }
            options.release = args[++i];
        } else if (strcmp(args[i], "--models-root") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --models-root requires DIR\n");
                return 2;
            }
            options.models_root = args[++i];
        } else if (strcmp(args[i], "--source") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --source requires DIR\n");
                return 2;
            }
            options.source = args[++i];
        } else if (strcmp(args[i], "--target") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --target requires TARGET\n");
                return 2;
            }
            options.target = args[++i];
        } else if (strcmp(args[i], "--" "include-files") == 0) {
            options.include_files = 1;
        } else if (strcmp(args[i], "--" "include-config") == 0) {
            options.include_config = 1;
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            options.include_blockers = 1;
        } else if (strcmp(args[i], "--" "include-next") == 0) {
            options.include_next = 1;
        } else if (strcmp(args[i], "--" "include-tensors") == 0) {
            options.include_tensors = 1;
        } else if (strcmp(args[i], "--tensor-limit") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --tensor-limit requires N\n");
                return 2;
            }
            if (!qwen_source_parse_positive_ull(args[++i], &options.tensor_limit)) {
                fprintf(stderr, "source-manifest report: --tensor-limit requires a positive integer\n");
                return 2;
            }
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options.output_mode = YVEX_SOURCE_REPORT_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            if (i + 1 >= arg_count) {
                fprintf(stderr, "source-manifest report: --" "output requires normal|table|audit\n");
                return 2;
            }
            if (!qwen_source_output_mode_parse(args[++i], &options.output_mode)) {
                fprintf(stderr, "source-manifest report: unsupported output mode: %s\n", args[i]);
                return 2;
            }
        } else if (strcmp(args[i], "--" "json") == 0) {
            fprintf(stderr, "source-manifest report: JSON output is unsupported; use --" "output normal|table|audit\n");
            return 2;
        } else {
            fprintf(stderr, "source-manifest report: unknown option: %s\n", args[i]);
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
    if (!options.target && options.source) {
        const char *base = qwen_source_path_basename(options.source);
        if (base &&
            qwen_source_target_matches_family_name(options.profile->family_key, base) &&
            qwen_source_path_format(options.resolved_target,
                                    sizeof(options.resolved_target),
                                    "%s", base, NULL)) {
            options.target = options.resolved_target;
        }
    }
    if (!options.target) {
        options.target = options.profile->target_id;
    } else if (!options.resolved_target[0]) {
        (void)qwen_source_path_format(options.resolved_target,
                                      sizeof(options.resolved_target),
                                      "%s", options.target, NULL);
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

/*
 * yvex_native_weight_table_open()
 *
 * Purpose:
 *   build a native safetensors header inventory for a source directory.
 *
 * Inputs:
 *   options is borrowed and must name a source directory; out receives an owned
 *   native weight table.
 *
 * Effects:
 *   scans source files and reads safetensors headers only, allocating table
 *   rows for tensor metadata; tensor payload bytes are not loaded.
 *
 * Failure:
 *   returns invalid-arg, IO, allocation, or malformed-header errors with
 *   partial table cleanup.
 *
 * Boundary:
 *   native header inventory is not tensor payload loading, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
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
    fprintf(stdout, "{\n");
    fprintf(stdout, "  \"schema\": \"yvex.native_weights.v1\",\n");
    fprintf(stdout, "  \"source\": \"%s\",\n", source ? source : "");
    fprintf(stdout, "  \"summary\": {\n");
    fprintf(stdout, "    \"shard_count\": %llu,\n", summary.shard_count);
    fprintf(stdout, "    \"tensor_count\": %llu,\n", summary.tensor_count);
    fprintf(stdout, "    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
    fprintf(stdout, "    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
    fprintf(stdout, "    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
    fprintf(stdout, "  }\n");
    fprintf(stdout, "}\n");
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
        table->header_error_count++;
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
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    header_len = st_le64(len_bytes);
    if (header_len == 0 || header_len > file_size - 8 || header_len > 64ull * 1024ull * 1024ull) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_FORMAT, "safetensors_header", "invalid safetensors header length: %s", shard_path);
        table->summary.malformed_shard_count++;
        table->header_error_count++;
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
        table->header_error_count++;
        return YVEX_ERR_FORMAT;
    }
    fclose(fp);
    json[header_len] = '\0';
    table->header_read_count++;
    table->header_bytes += header_len;
    payload_bytes = file_size - 8 - header_len;
    rc = yvex_safetensors_parse_header(json, payload_bytes, shard_path, table, err);
    if (rc != YVEX_OK) {
        table->summary.malformed_shard_count++;
        table->header_error_count++;
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

/*
 * yvex_safetensors_parse_header()
 *
 * Purpose:
 *   parse safetensors header JSON into native tensor metadata records.
 *
 * Inputs:
 *   json, payload byte count, shard path, and output table are borrowed.
 *
 * Effects:
 *   parses dtype, shape, and data offset metadata, validates declared byte
 *   ranges against payload size, and appends table rows; it never reads tensor
 *   payload bytes.
 *
 * Failure:
 *   returns invalid-arg, format, bounds, or allocation errors through the JSON
 *   parser and native table append path.
 *
 * Boundary:
 *   safetensors header parsing is not source verification, tensor payload
 *   loading, role mapping, artifact emission, runtime support, or generation.
 */
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
