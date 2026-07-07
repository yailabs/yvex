/*
 * yvex_source_scan.c - local source footprint scanner.
 *
 * Owner: src/source.
 * Owns: directory walking and source manifest file-list summary facts.
 * Does not own: JSON writing, CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: scans file metadata only and never loads tensor payload bytes.
 * Boundary: footprint scanning is not source verification or runtime readiness.
 */
#define _XOPEN_SOURCE 700
#include "yvex_source_scan.h"
#include "yvex_source_private.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
