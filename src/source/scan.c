/* Owner: source footprint scanning.
 * Owns: deterministic local directory walking and file-list summaries.
 * Does not own: source verification, manifest writing, artifacts, or runtime.
 * Invariants: scans metadata only and never reads model payload contents.
 * Boundary: footprint discovery does not create trust.
 * Purpose: build deterministic source footprint rows beneath one root.
 * Inputs: source root, bounded file-list storage, and caller summary.
 * Effects: reads directory entries and stat facts only.
 * Failure: path, allocation, traversal, or stat failure releases partial rows. */
#define _XOPEN_SOURCE 700
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <yvex/internal/source_payload.h>

/* Purpose: publish one typed scan refusal without duplicating error-state transitions. */
static int scan_refuse(yvex_error *err,
                       yvex_status status,
                       const char *where,
                       const char *message) {
    yvex_error_set(err, status, where, message);
    return status;
}

/* Purpose: project name starts ci facts while preserving the canonical source footprint invariants. */
static int scan_name_starts_ci(const char *s, const char *prefix) {
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

/* Purpose: project basename facts while preserving the canonical source footprint invariants. */
static const char *scan_basename(const char *path) {
    const char *slash;

    if (!path) {
        return "";
    }
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Purpose: project kind for path facts while preserving the canonical source footprint invariants.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source footprint scanning state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: footprint discovery does not create trust. */
static const char *scan_kind_for_path(const char *rel_path) {
    const char *base = scan_basename(rel_path);

    if (yvex_source_ends_with(base, ".safetensors")) {
        return "safetensors";
    }
    if (strcmp(base, "config.json") == 0 || strcmp(base, "generation_config.json") == 0 ||
        scan_name_starts_ci(base, "config")) {
        return "config";
    }
    if (scan_name_starts_ci(base, "tokenizer") || yvex_source_ends_with(base, ".model")) {
        return "tokenizer";
    }
    if (scan_name_starts_ci(base, "readme")) {
        return "readme";
    }
    if (scan_name_starts_ci(base, "license") || scan_name_starts_ci(base, "copying")) {
        return "license";
    }
    if (yvex_source_ends_with(base, ".json") || yvex_source_ends_with(base, ".txt") ||
        yvex_source_ends_with(base, ".md")) {
        return "metadata";
    }
    return "other";
}

/* Purpose: project append file facts while preserving the canonical source footprint invariants.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source footprint scanning state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: footprint discovery does not create trust. */
static int scan_append_file(yvex_source_manifest_file_list *list,
                            const char *rel_path,
                            unsigned long long size_bytes,
                            yvex_error *err) {
    yvex_source_manifest_file *next;
    const char *kind;
    size_t new_cap;

    if (list->count == list->cap) {
        new_cap = list->cap == 0 ? 16u : list->cap * 2u;
        next = (yvex_source_manifest_file *)realloc(list->items, new_cap * sizeof(list->items[0]));
        if (!next) {
            yvex_error_set(
                err, YVEX_ERR_NOMEM, "source_manifest_scan", "file list allocation failed");
            return YVEX_ERR_NOMEM;
        }
        list->items = next;
        list->cap = new_cap;
    }

    kind = scan_kind_for_path(rel_path);
    list->items[list->count].path = yvex_core_strdup(rel_path);
    if (!list->items[list->count].path) {
        return scan_refuse(err, YVEX_ERR_NOMEM, "source_manifest_scan", "file path allocation failed");
    }
    list->items[list->count].size_bytes = size_bytes;
    list->items[list->count].kind = kind;
    list->count++;

    if (list->summary.file_count == ULLONG_MAX ||
        ULLONG_MAX - list->summary.total_size_bytes < size_bytes) {
        free(list->items[list->count - 1u].path);
        memset(&list->items[list->count - 1u], 0, sizeof(list->items[0]));
        list->count--;
        return scan_refuse(err, YVEX_ERR_BOUNDS, "source_manifest_scan", "source footprint overflow");
    }
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

/* Purpose: define deterministic ordering for source footprint records. */
static int scan_file_compare(const void *a, const void *b) {
    const yvex_source_manifest_file *fa = (const yvex_source_manifest_file *)a;
    const yvex_source_manifest_file *fb = (const yvex_source_manifest_file *)b;

    return strcmp(fa->path, fb->path);
}

/* Purpose: project dir facts while preserving the canonical source footprint invariants.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source footprint scanning state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: footprint discovery does not create trust. */
static int scan_dir(const char *root,
                    const char *rel_dir,
                    int include_files,
                    yvex_source_manifest_file_list *out,
                    yvex_error *err) {
    char *abs_dir;
    DIR *dir;
    struct dirent *ent;
    int rc = YVEX_OK;

    abs_dir = rel_dir && rel_dir[0] != '\0' ? yvex_source_path_alloc(root, rel_dir) : yvex_core_strdup(root);
    if (!abs_dir) {
        yvex_error_set(
            err, YVEX_ERR_NOMEM, "source_manifest_scan", "directory path allocation failed");
        return YVEX_ERR_NOMEM;
    }

    dir = opendir(abs_dir);
    if (!dir) {
        yvex_error_setf(
            err, YVEX_ERR_IO, "source_manifest_scan", "cannot open directory: %s", abs_dir);
        free(abs_dir);
        return YVEX_ERR_IO;
    }

    for (;;) {
        char *rel_path;
        char *abs_path;
        struct stat st;

        errno = 0;
        ent = readdir(dir);
        if (!ent) {
            if (errno != 0 && rc == YVEX_OK) {
                yvex_error_setf(
                    err, YVEX_ERR_IO, "source_manifest_scan", "cannot read directory: %s", abs_dir);
                rc = YVEX_ERR_IO;
            }
            break;
        }

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        rel_path = rel_dir && rel_dir[0] != '\0' ? yvex_source_path_alloc(rel_dir, ent->d_name)
                                                 : yvex_core_strdup(ent->d_name);
        if (!rel_path) {
            yvex_error_set(
                err, YVEX_ERR_NOMEM, "source_manifest_scan", "relative path allocation failed");
            rc = YVEX_ERR_NOMEM;
            break;
        }
        abs_path = yvex_source_path_alloc(root, rel_path);
        if (!abs_path) {
            free(rel_path);
            yvex_error_set(
                err, YVEX_ERR_NOMEM, "source_manifest_scan", "absolute path allocation failed");
            rc = YVEX_ERR_NOMEM;
            break;
        }

        if (lstat(abs_path, &st) != 0) {
            yvex_error_setf(
                err, YVEX_ERR_IO, "source_manifest_scan", "cannot stat path: %s", abs_path);
            free(abs_path);
            free(rel_path);
            rc = YVEX_ERR_IO;
            break;
        }

        if (S_ISDIR(st.st_mode)) {
            rc = scan_dir(root, rel_path, include_files, out, err);
        } else if (S_ISREG(st.st_mode)) {
            rc = scan_append_file(out, rel_path, (unsigned long long)st.st_size, err);
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

    if (closedir(dir) != 0 && rc == YVEX_OK) {
        yvex_error_setf(
            err, YVEX_ERR_IO, "source_manifest_scan", "cannot close directory: %s", abs_dir);
        rc = YVEX_ERR_IO;
    }
    free(abs_dir);
    return rc;
}

/* Purpose: initialize source footprint state to its canonical empty or default value.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source footprint scanning state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: footprint discovery does not create trust. */
void yvex_source_manifest_file_list_init(yvex_source_manifest_file_list *list) {
    if (!list) {
        return;
    }
    memset(list, 0, sizeof(*list));
}

/* Purpose: release resources owned by one source footprint object and clear its observable state.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source footprint scanning; cleanup remains deterministic.
 * Failure: null or released source footprint scanning handles remain harmless.
 * Boundary: footprint discovery does not create trust. */
void yvex_source_manifest_file_list_free(yvex_source_manifest_file_list *list) {
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

/* Purpose: enumerate deterministic source footprint rows beneath one admitted root.
 * Inputs: typed source footprint scanning arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source footprint scanning state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: footprint discovery does not create trust. */
int yvex_source_manifest_scan_files(const char *local_path,
                                    int include_files,
                                    yvex_source_manifest_file_list *out,
                                    yvex_error *err) {
    struct stat st;
    int rc;

    if (!local_path || !out) {
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "source_manifest_scan", "local_path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (lstat(local_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err,
                        YVEX_ERR_IO,
                        "source_manifest_scan",
                        "local path is not a directory: %s",
                        local_path);
        return YVEX_ERR_IO;
    }

    rc = scan_dir(local_path, "", include_files, out, err);
    if (rc == YVEX_OK && include_files && out->count > 1) {
        qsort(out->items, out->count, sizeof(out->items[0]), scan_file_compare);
    }
    return rc;
}
