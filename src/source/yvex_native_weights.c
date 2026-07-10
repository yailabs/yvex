/*
 * yvex_native_weights.c - native safetensors metadata inventory.
 *
 * Owner: src/source.
 * Owns: native safetensors table lifecycle and header-only directory scanning.
 * Does not own: tensor payload loading, CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: reads safetensors headers only and never loads tensor payload bytes.
 * Boundary: native inventory is not role mapping, artifact emission, or runtime readiness.
 */
#define _XOPEN_SOURCE 700
#include "yvex_native_weights.h"
#include "yvex_source_private.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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
    if (table->count == ULLONG_MAX || table->summary.tensor_count == ULLONG_MAX ||
        ULLONG_MAX - table->summary.total_tensor_bytes < row->data_bytes) {
        free((char *)row->name);
        free((char *)row->shard_path);
        free((char *)row->dtype_name);
        memset(row, 0, sizeof(*row));
        yvex_error_set(err, YVEX_ERR_BOUNDS, "native_weight_add",
                       "native tensor inventory overflow");
        return YVEX_ERR_BOUNDS;
    }
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
