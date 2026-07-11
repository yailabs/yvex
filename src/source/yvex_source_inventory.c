/*
 * yvex_source_inventory.c - source shard and header inventory owner.
 *
 * Owner: src/source.
 * Owns: upstream index/snapshot parsing, root shard geometry, one canonical
 *   safetensors header pass, tensor uniqueness, and deterministic derived rows.
 * Does not own: provider downloads, manifest writes, payload reads, role mapping, or rendering.
 * Invariants: indexed and indexless authority modes are explicit and never conflated.
 * Boundary: complete header agreement is not model execution or payload digest proof.
 */
#include "yvex_source_inventory.h"

#include "yvex_source_json.h"
#include "yvex_source_private.h"
#include "yvex_source_provenance.h"
#include "yvex_source_verify_internal.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SOURCE_INDEX_CAP (128u * 1024u * 1024u)
#define SOURCE_UPSTREAM_INVENTORY_CAP (8u * 1024u * 1024u)

typedef struct {
    char *tensor;
    char *shard;
    int seen_in_header;
} source_index_entry;

typedef struct {
    source_index_entry *items;
    size_t count;
    size_t cap;
    unsigned long long declared_total_size;
    int has_declared_total_size;
    int duplicate_tensor;
} source_index;

typedef struct {
    char **names;
    unsigned long long *sizes;
    size_t count;
    size_t cap;
    unsigned int declared_total;
} source_shards;

typedef struct {
    char *name;
    unsigned long long size;
} source_upstream_file;

typedef struct {
    source_upstream_file *items;
    size_t count;
    size_t cap;
    char repo[256];
    char revision[128];
} source_upstream_inventory;

static char *source_inventory_strdup(const char *text)
{
    size_t length;
    char *copy;

    if (!text) return NULL;
    length = strlen(text);
    copy = (char *)malloc(length + 1u);
    if (!copy) return NULL;
    memcpy(copy, text, length + 1u);
    return copy;
}

/* Appends one unique tensor-to-shard assignment with owned strings. */
static int source_index_append(source_index *index,
                               const char *tensor,
                               const char *shard,
                               yvex_error *err)
{
    source_index_entry *next;
    size_t cap;

    if (index->count == index->cap) {
        cap = index->cap ? index->cap * 2u : 256u;
        if (cap < index->cap || cap > SIZE_MAX / sizeof(index->items[0])) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "source_index",
                           "shard index allocation overflow");
            return YVEX_ERR_BOUNDS;
        }
        next = (source_index_entry *)realloc(index->items,
                                             cap * sizeof(index->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_index",
                           "shard index allocation failed");
            return YVEX_ERR_NOMEM;
        }
        index->items = next;
        index->cap = cap;
    }
    memset(&index->items[index->count], 0, sizeof(index->items[0]));
    index->items[index->count].tensor = source_inventory_strdup(tensor);
    index->items[index->count].shard = source_inventory_strdup(shard);
    if (!index->items[index->count].tensor ||
        !index->items[index->count].shard) {
        free(index->items[index->count].tensor);
        free(index->items[index->count].shard);
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_index",
                       "shard index row allocation failed");
        return YVEX_ERR_NOMEM;
    }
    index->count++;
    return YVEX_OK;
}

static void source_index_free(source_index *index)
{
    size_t i;

    if (!index) return;
    for (i = 0; i < index->count; ++i) {
        free(index->items[i].tensor);
        free(index->items[i].shard);
    }
    free(index->items);
    memset(index, 0, sizeof(*index));
}

static int source_index_compare(const void *left, const void *right)
{
    const source_index_entry *a = (const source_index_entry *)left;
    const source_index_entry *b = (const source_index_entry *)right;
    return strcmp(a->tensor, b->tensor);
}

/* Parses the upstream declared payload size with duplicate-field refusal. */
static int source_parse_index_metadata(yvex_source_json *json,
                                       source_index *index)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return 1;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "total_size") == 0) {
            if (index->has_declared_total_size || !yvex_source_json_u64(
                    json, &index->declared_total_size)) return 0;
            index->has_declared_total_size = 1;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses all unique tensor-to-shard assignments from the upstream weight map. */
static int source_parse_weight_map(yvex_source_json *json,
                                   source_index *index,
                                   yvex_error *err)
{
    char tensor[YVEX_SOURCE_JSON_KEY_CAP];
    char shard[YVEX_PATH_CAP];

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        int rc;
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return index->count > 0u;
        }
        if (!yvex_source_json_string(json, tensor, sizeof(tensor))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':' ||
            !yvex_source_json_string(json, shard, sizeof(shard))) return 0;
        rc = source_index_append(index, tensor, shard, err);
        if (rc != YVEX_OK) return -1;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses a complete upstream index and requires metadata plus a nonempty map. */
static int source_parse_index_json(const char *data,
                                   size_t length,
                                   source_index *index,
                                   yvex_error *err)
{
    yvex_source_json json;
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    unsigned int seen = 0u;
    size_t i;

    yvex_source_json_init(&json, data, length);
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        int parsed = 1;
        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "metadata") == 0) {
            if ((seen & 1u) || !source_parse_index_metadata(&json, index)) return 0;
            seen |= 1u;
        } else if (strcmp(key, "weight_map") == 0) {
            if (seen & 2u) return 0;
            parsed = source_parse_weight_map(&json, index, err);
            if (parsed < 0) return -1;
            if (!parsed) return 0;
            seen |= 2u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    if (!yvex_source_json_complete(&json) || !(seen & 2u)) return 0;
    qsort(index->items, index->count, sizeof(index->items[0]),
          source_index_compare);
    for (i = 1u; i < index->count; ++i) {
        if (strcmp(index->items[i - 1u].tensor,
                   index->items[i].tensor) == 0) {
            index->duplicate_tensor = 1;
            return 0;
        }
    }
    return 1;
}

static source_index_entry *source_index_find(source_index *index,
                                             const char *tensor)
{
    size_t low = 0u;
    size_t high = index ? index->count : 0u;

    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        int cmp = strcmp(tensor, index->items[mid].tensor);
        if (cmp == 0) return &index->items[mid];
        if (cmp < 0) high = mid;
        else low = mid + 1u;
    }
    return NULL;
}

/* Adds one unique root shard and its checked local file size. */
static int source_shards_append(source_shards *shards,
                                const char *name,
                                unsigned long long size,
                                yvex_error *err)
{
    char **next_names;
    unsigned long long *next_sizes;
    size_t cap;

    if (shards->count == shards->cap) {
        cap = shards->cap ? shards->cap * 2u : 64u;
        next_names = (char **)realloc(shards->names,
                                     cap * sizeof(shards->names[0]));
        if (!next_names) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                           "shard name allocation failed");
            return YVEX_ERR_NOMEM;
        }
        shards->names = next_names;
        next_sizes = (unsigned long long *)realloc(
            shards->sizes, cap * sizeof(shards->sizes[0]));
        if (!next_sizes) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                           "shard size allocation failed");
            return YVEX_ERR_NOMEM;
        }
        shards->sizes = next_sizes;
        shards->cap = cap;
    }
    shards->names[shards->count] = source_inventory_strdup(name);
    if (!shards->names[shards->count]) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_shards",
                       "shard name copy failed");
        return YVEX_ERR_NOMEM;
    }
    shards->sizes[shards->count] = size;
    shards->count++;
    return YVEX_OK;
}

static void source_shards_free(source_shards *shards)
{
    size_t i;

    if (!shards) return;
    for (i = 0; i < shards->count; ++i) free(shards->names[i]);
    free(shards->names);
    free(shards->sizes);
    memset(shards, 0, sizeof(*shards));
}

/* Parses the exact model-N-of-M shard grammar into sequence facts. */
static int source_shard_name_parse(const char *name,
                                   unsigned int *index_out,
                                   unsigned int *total_out)
{
    unsigned int index = 0u;
    unsigned int total = 0u;
    size_t i;

    if (!name || strlen(name) != strlen("model-00000-of-00000.safetensors") ||
        strncmp(name, "model-", 6u) != 0 ||
        strncmp(name + 11u, "-of-", 4u) != 0 ||
        strcmp(name + 20u, ".safetensors") != 0) return 0;
    for (i = 6u; i < 11u; ++i) {
        if (!isdigit((unsigned char)name[i])) return 0;
        index = index * 10u + (unsigned int)(name[i] - '0');
    }
    for (i = 15u; i < 20u; ++i) {
        if (!isdigit((unsigned char)name[i])) return 0;
        total = total * 10u + (unsigned int)(name[i] - '0');
    }
    if (index == 0u || total == 0u || index > total) return 0;
    if (index_out) *index_out = index;
    if (total_out) *total_out = total;
    return 1;
}

static int source_shard_compare(const void *left, const void *right)
{
    const char *const *a = (const char *const *)left;
    const char *const *b = (const char *const *)right;
    return strcmp(*a, *b);
}

/* Sorts shard names and keeps their corresponding byte sizes paired. */
static void source_shards_sort(source_shards *shards)
{
    size_t i;
    size_t j;

    for (i = 0u; i < shards->count; ++i) {
        for (j = i + 1u; j < shards->count; ++j) {
            if (source_shard_compare(&shards->names[i],
                                     &shards->names[j]) > 0) {
                char *name = shards->names[i];
                unsigned long long size = shards->sizes[i];
                shards->names[i] = shards->names[j];
                shards->sizes[i] = shards->sizes[j];
                shards->names[j] = name;
                shards->sizes[j] = size;
            }
        }
    }
}

static long source_shards_find(const source_shards *shards, const char *name)
{
    size_t i;

    for (i = 0; shards && i < shards->count; ++i) {
        if (strcmp(shards->names[i], name) == 0) return (long)i;
    }
    return -1;
}

/* Inventories only root safetensors shards and rejects unexpected shard forms. */
static int source_scan_root(const char *source_path,
                            source_shards *shards,
                            yvex_source_verification *out,
                            yvex_error *err)
{
    DIR *dir;
    struct dirent *entry;

    dir = opendir(source_path);
    if (!dir) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_inventory_root",
                        "cannot inspect source directory: %s", source_path);
        return YVEX_ERR_IO;
    }
    for (;;) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        unsigned int shard_index = 0u;
        unsigned int shard_total = 0u;
        size_t i;
        int rc;

        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno != 0) {
                yvex_source_verification_add_blocker(
                    out, "source-directory-read-failed");
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) continue;
        if (!yvex_source_path_join(path, sizeof(path), source_path,
                                   entry->d_name)) {
            yvex_source_verification_add_blocker(
                out, "source-entry-path-overflow");
            continue;
        }
        if (lstat(path, &st) != 0) {
            yvex_source_verification_add_blocker(out, "source-entry-unreadable");
            continue;
        }
        if (!S_ISREG(st.st_mode) || st.st_size < 0) continue;
        if (strlen(entry->d_name) <= strlen(".safetensors") ||
            strcmp(entry->d_name + strlen(entry->d_name) -
                       strlen(".safetensors"), ".safetensors") != 0) continue;
        if (!source_shard_name_parse(entry->d_name, &shard_index,
                                     &shard_total)) {
            yvex_source_verification_add_blocker(out, "unexpected-shard");
            continue;
        }
        if (!shards->declared_total) {
            shards->declared_total = shard_total;
        } else if (shards->declared_total != shard_total) {
            yvex_source_verification_add_blocker(out,
                                                 "inconsistent-shard-set");
        }
        for (i = 0; i < shards->count; ++i) {
            unsigned int prior_index = 0u;
            if (source_shard_name_parse(shards->names[i], &prior_index, NULL) &&
                prior_index == shard_index) {
                yvex_source_verification_add_blocker(
                    out, "duplicate-source-shard");
            }
        }
        rc = source_shards_append(shards, entry->d_name,
                                  (unsigned long long)st.st_size, err);
        if (rc != YVEX_OK) {
            closedir(dir);
            return rc;
        }
        if (!yvex_source_checked_add_u64(
                &out->shard_bytes, (unsigned long long)st.st_size)) {
            out->footprint_overflow = 1;
            yvex_source_verification_add_blocker(out,
                                                 "source-footprint-overflow");
        }
    }
    if (closedir(dir) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "source-directory-close-failed");
    }
    source_shards_sort(shards);
    out->shard_count = (unsigned long long)shards->count;
    if (!shards->count) {
        yvex_source_verification_add_blocker(out, "missing-source-shards");
    }
    if (shards->declared_total &&
        shards->count != (size_t)shards->declared_total) {
        yvex_source_verification_add_blocker(out, "incomplete-shard-set");
    }
    for (size_t i = 0u; i < shards->count; ++i) {
        unsigned int index = 0u;
        if (!source_shard_name_parse(shards->names[i], &index, NULL) ||
            index != i + 1u) {
            yvex_source_verification_add_blocker(out,
                                                 "discontinuous-shard-set");
            break;
        }
    }
    return YVEX_OK;
}

/* Adds one unique official snapshot file and its declared metadata size. */
static int source_upstream_append(source_upstream_inventory *inventory,
                                  const char *name,
                                  unsigned long long size,
                                  yvex_error *err)
{
    source_upstream_file *next;
    size_t cap;

    if (inventory->count == inventory->cap) {
        cap = inventory->cap ? inventory->cap * 2u : 64u;
        next = (source_upstream_file *)realloc(
            inventory->items, cap * sizeof(inventory->items[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_upstream_inventory",
                           "upstream inventory allocation failed");
            return YVEX_ERR_NOMEM;
        }
        inventory->items = next;
        inventory->cap = cap;
    }
    inventory->items[inventory->count].name = source_inventory_strdup(name);
    if (!inventory->items[inventory->count].name) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_upstream_inventory",
                       "upstream inventory name allocation failed");
        return YVEX_ERR_NOMEM;
    }
    inventory->items[inventory->count].size = size;
    inventory->count++;
    return YVEX_OK;
}

static void source_upstream_free(source_upstream_inventory *inventory)
{
    size_t i;

    for (i = 0u; inventory && i < inventory->count; ++i) {
        free(inventory->items[i].name);
    }
    if (inventory) {
        free(inventory->items);
        memset(inventory, 0, sizeof(*inventory));
    }
}

/* Parses one file row from a pinned upstream snapshot inventory. */
static int source_upstream_parse_file(yvex_source_json *json,
                                      source_upstream_inventory *inventory,
                                      yvex_error *err)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    char name[YVEX_PATH_CAP] = "";
    unsigned long long size = 0u;
    unsigned int seen = 0u;

    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 3u &&
                   source_upstream_append(inventory, name, size, err) == YVEX_OK;
        }
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (strcmp(key, "path") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    json, name, sizeof(name))) return 0;
            seen |= 1u;
        } else if (strcmp(key, "size_bytes") == 0) {
            if ((seen & 2u) || !yvex_source_json_u64(json, &size)) return 0;
            seen |= 2u;
        } else if (!yvex_source_json_skip_value(json)) {
            return 0;
        }
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') continue;
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses the bounded official file list without accepting duplicates. */
static int source_upstream_parse_files(yvex_source_json *json,
                                       source_upstream_inventory *inventory,
                                       yvex_error *err)
{
    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (!source_upstream_parse_file(json, inventory, err)) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            return inventory->count > 0u;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Parses and verifies repository/revision identity for an indexless snapshot. */
static int source_upstream_parse(const char *data,
                                 size_t length,
                                 source_upstream_inventory *inventory,
                                 yvex_error *err)
{
    yvex_source_json json;
    char key[YVEX_SOURCE_JSON_KEY_CAP];
    char schema[64];
    unsigned int seen = 0u;

    yvex_source_json_init(&json, data, length);
    yvex_source_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{') return 0;
    for (;;) {
        yvex_source_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_source_json_string(&json, key, sizeof(key))) return 0;
        yvex_source_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':') return 0;
        if (strcmp(key, "schema") == 0) {
            if ((seen & 1u) || !yvex_source_json_string(
                    &json, schema, sizeof(schema)) ||
                strcmp(schema, "yvex.source_upstream_inventory.v1") != 0) return 0;
            seen |= 1u;
        } else if (strcmp(key, "repository") == 0) {
            if ((seen & 2u) || !yvex_source_json_string(
                    &json, inventory->repo, sizeof(inventory->repo))) return 0;
            seen |= 2u;
        } else if (strcmp(key, "revision") == 0) {
            if ((seen & 4u) || !yvex_source_json_string(
                    &json, inventory->revision,
                    sizeof(inventory->revision))) return 0;
            seen |= 4u;
        } else if (strcmp(key, "files") == 0) {
            if ((seen & 8u) || !source_upstream_parse_files(
                    &json, inventory, err)) return 0;
            seen |= 8u;
        } else if (!yvex_source_json_skip_value(&json)) {
            return 0;
        }
        yvex_source_json_space(&json);
        if (json.cursor >= json.end) return 0;
        if (*json.cursor == '}') continue;
        if (*json.cursor++ != ',') return 0;
    }
    return yvex_source_json_complete(&json) && seen == 15u;
}

/* Verifies the official index file, provider identity, and declared shard map. */
static int source_verify_index(const yvex_source_verify_options *options,
                               source_index *index,
                               yvex_source_verification *out,
                               yvex_error *err)
{
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    int parsed;
    int rc;

    if (!yvex_source_path_join(path, sizeof(path), options->source_path,
                               options->identity->upstream_index_path) ||
        !yvex_source_regular_file(path, NULL)) {
        yvex_source_verification_add_blocker(out, "missing-shard-index");
        return YVEX_OK;
    }
    out->shard_index_present = 1;
    data = yvex_source_read_bounded_file(path, SOURCE_INDEX_CAP, &length, err);
    if (!data) {
        yvex_source_verification_add_blocker(out, "malformed-shard-index");
        return yvex_error_code(err) == YVEX_ERR_NOMEM ? YVEX_ERR_NOMEM : YVEX_OK;
    }
    parsed = source_parse_index_json(data, length, index, err);
    free(data);
    if (parsed < 0) return yvex_error_code(err);
    if (!parsed) {
        yvex_source_verification_add_blocker(
            out, index->duplicate_tensor ? "duplicate-index-tensor"
                                         : "malformed-shard-index");
        return YVEX_OK;
    }
    out->shard_index_valid = 1;
    out->indexed_tensor_count = (unsigned long long)index->count;
    rc = yvex_source_provenance_verify_file(
        options, options->identity->upstream_index_path, 1, out, err);
    if (rc != YVEX_OK) return rc;
    snprintf(out->inventory_authority, sizeof(out->inventory_authority),
             "%s", "upstream-index");
    return YVEX_OK;
}

/* Reconciles every indexed shard assignment with the root shard inventory. */
static void source_verify_index_shards(source_index *index,
                                       const source_shards *shards,
                                       yvex_source_verification *out)
{
    size_t i;
    unsigned long long unique = 0u;

    if (!out->shard_index_valid) return;
    for (i = 0; i < index->count; ++i) {
        size_t j;
        int first = 1;
        if (source_shards_find(shards, index->items[i].shard) < 0) {
            yvex_source_verification_add_blocker(out,
                                                 "missing-referenced-shard");
        }
        for (j = 0; j < i; ++j) {
            if (strcmp(index->items[j].shard, index->items[i].shard) == 0) {
                first = 0;
                break;
            }
        }
        if (first) unique++;
    }
    out->referenced_shard_count = unique;
    for (i = 0; i < shards->count; ++i) {
        size_t j;
        int found = 0;
        for (j = 0; j < index->count; ++j) {
            if (strcmp(shards->names[i], index->items[j].shard) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            yvex_source_verification_add_blocker(out, "unexpected-shard");
        }
    }
}

/* Verifies a deliberately indexless official snapshot before deriving a map. */
static int source_verify_upstream_indexless(
    const yvex_source_verify_options *options,
    const source_shards *shards,
    yvex_source_verification *out,
    yvex_error *err)
{
    source_upstream_inventory upstream;
    char *data;
    size_t length;
    size_t i;

    memset(&upstream, 0, sizeof(upstream));
    if (!options->upstream_inventory_path ||
        !yvex_source_regular_file(options->upstream_inventory_path, NULL)) {
        yvex_source_verification_add_blocker(out,
                                             "missing-upstream-inventory");
        return YVEX_OK;
    }
    data = yvex_source_read_bounded_file(options->upstream_inventory_path,
                                         SOURCE_UPSTREAM_INVENTORY_CAP,
                                         &length, err);
    if (!data) {
        yvex_source_verification_add_blocker(out,
                                             "malformed-upstream-inventory");
        return yvex_error_code(err) == YVEX_ERR_NOMEM ? YVEX_ERR_NOMEM : YVEX_OK;
    }
    if (!source_upstream_parse(data, length, &upstream, err)) {
        free(data);
        source_upstream_free(&upstream);
        yvex_source_verification_add_blocker(out,
                                             "malformed-upstream-inventory");
        return YVEX_OK;
    }
    free(data);
    if (strcmp(upstream.repo, options->identity->upstream_repo_id) != 0 ||
        strcmp(upstream.revision, options->identity->upstream_revision) != 0) {
        yvex_source_verification_add_blocker(out,
                                             "stale-upstream-inventory");
    }
    for (i = 0u; i < upstream.count; ++i) {
        long local = source_shards_find(shards, upstream.items[i].name);
        if (local < 0 || shards->sizes[local] != upstream.items[i].size) {
            yvex_source_verification_add_blocker(out,
                                                 "upstream-local-inventory-drift");
        }
    }
    if (upstream.count != shards->count) {
        yvex_source_verification_add_blocker(out,
                                             "upstream-local-inventory-drift");
    }
    if (!yvex_source_verification_has_blocker(
            out, "upstream-local-inventory-drift") &&
        !yvex_source_verification_has_blocker(out,
                                              "stale-upstream-inventory")) {
        snprintf(out->inventory_authority, sizeof(out->inventory_authority),
                 "%s", "header-derived");
    }
    source_upstream_free(&upstream);
    return YVEX_OK;
}

static int source_native_info_compare(const void *left, const void *right)
{
    const yvex_native_weight_info *const *a =
        (const yvex_native_weight_info *const *)left;
    const yvex_native_weight_info *const *b =
        (const yvex_native_weight_info *const *)right;
    return strcmp((*a)->name, (*b)->name);
}

/* Builds deterministic owned tensor-to-shard rows from the canonical header table. */
static int source_derived_build(const yvex_native_weight_table *table,
                                yvex_source_derived_inventory *derived,
                                yvex_error *err)
{
    const yvex_native_weight_info **sorted;
    size_t i;

    if (!derived) return YVEX_OK;
    sorted = (const yvex_native_weight_info **)calloc(
        (size_t)table->count, sizeof(sorted[0]));
    if (!sorted) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_derived_inventory",
                       "derived inventory sort allocation failed");
        return YVEX_ERR_NOMEM;
    }
    derived->rows = (yvex_source_inventory_row *)calloc(
        (size_t)table->count, sizeof(derived->rows[0]));
    if (!derived->rows) {
        free(sorted);
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_derived_inventory",
                       "derived inventory row allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (i = 0u; i < (size_t)table->count; ++i) sorted[i] = &table->items[i];
    qsort(sorted, (size_t)table->count, sizeof(sorted[0]),
          source_native_info_compare);
    for (i = 0u; i < (size_t)table->count; ++i) {
        derived->rows[i].tensor = source_inventory_strdup(sorted[i]->name);
        derived->rows[i].shard = source_inventory_strdup(sorted[i]->shard_path);
        if (!derived->rows[i].tensor || !derived->rows[i].shard) {
            derived->count = i + 1u;
            free(sorted);
            yvex_source_derived_inventory_free(derived);
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_derived_inventory",
                           "derived inventory string allocation failed");
            return YVEX_ERR_NOMEM;
        }
    }
    derived->count = (size_t)table->count;
    free(sorted);
    return YVEX_OK;
}

/* Performs the sole header pass and reconciles unique tensors with its authority. */
static int source_verify_headers(
    const yvex_source_verify_options *options,
    const source_shards *shards,
    source_index *index,
    yvex_source_verification *out,
    yvex_source_derived_inventory *derived,
    yvex_error *err)
{
    yvex_native_weight_table *table;
    const yvex_native_weight_info **sorted = NULL;
    size_t i;
    int mismatch = 0;
    int indexed = strcmp(out->inventory_authority, "upstream-index") == 0;
    int rc = YVEX_OK;

    table = (yvex_native_weight_table *)calloc(1u, sizeof(*table));
    if (!table) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_headers",
                       "native header table allocation failed");
        return YVEX_ERR_NOMEM;
    }
    out->header_scan_count++;
    for (i = 0; i < shards->count; ++i) {
        char path[YVEX_PATH_CAP];
        yvex_error header_error;
        unsigned long long before = table->count;
        unsigned long long row;

        rc = yvex_source_provenance_verify_file(
            options, shards->names[i], 0, out, err);
        if (rc != YVEX_OK) goto cleanup;
        if (!yvex_source_path_join(path, sizeof(path), options->source_path,
                                   shards->names[i])) {
            yvex_source_verification_add_blocker(
                out, "invalid-safetensors-header");
            continue;
        }
        yvex_error_clear(&header_error);
        rc = yvex_safetensors_read_header_file(path, shards->names[i], table,
                                               &header_error);
        if (rc == YVEX_ERR_NOMEM) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_headers",
                           "native header inventory allocation failed");
            goto cleanup;
        }
        if (rc != YVEX_OK) {
            if (strcmp(yvex_error_where(&header_error),
                       "native_weight_add") == 0 &&
                strncmp(yvex_error_message(&header_error),
                        "duplicate tensor name:",
                        strlen("duplicate tensor name:")) == 0) {
                yvex_source_verification_add_blocker(
                    out, "duplicate-header-tensor");
                mismatch = 1;
            } else {
                yvex_source_verification_add_blocker(
                    out, "invalid-safetensors-header");
            }
            rc = YVEX_OK;
            continue;
        }
        for (row = before; row < table->count && indexed; ++row) {
            source_index_entry *entry = source_index_find(
                index, table->items[row].name);
            if (!entry || strcmp(entry->shard, shards->names[i]) != 0) {
                mismatch = 1;
            } else {
                entry->seen_in_header = 1;
            }
        }
    }
    sorted = (const yvex_native_weight_info **)calloc(
        (size_t)table->count, sizeof(sorted[0]));
    if (!sorted) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_verify_headers",
                       "header uniqueness allocation failed");
        rc = YVEX_ERR_NOMEM;
        goto cleanup;
    }
    for (i = 0u; i < (size_t)table->count; ++i) sorted[i] = &table->items[i];
    qsort(sorted, (size_t)table->count, sizeof(sorted[0]),
          source_native_info_compare);
    for (i = 1u; i < (size_t)table->count; ++i) {
        if (strcmp(sorted[i - 1u]->name, sorted[i]->name) == 0) {
            yvex_source_verification_add_blocker(out,
                                                 "duplicate-header-tensor");
            mismatch = 1;
        }
    }
    if (indexed) {
        for (i = 0; i < index->count; ++i) {
            if (!index->items[i].seen_in_header) mismatch = 1;
        }
        if (mismatch || table->count != index->count) {
            yvex_source_verification_add_blocker(
                out, "shard-index-header-mismatch");
        } else {
            out->shard_index_headers_match = 1;
        }
    } else if (!mismatch) {
        out->shard_index_headers_match = 1;
        out->indexed_tensor_count = table->count;
        out->referenced_shard_count = shards->count;
        rc = source_derived_build(table, derived, err);
        if (rc != YVEX_OK) goto cleanup;
    }
    for (i = 0; i < (size_t)table->count; ++i) {
        const yvex_native_weight_info *info = &table->items[i];
        if (info->rank > out->max_tensor_rank) out->max_tensor_rank = info->rank;
        switch (info->dtype) {
        case YVEX_NATIVE_DTYPE_F16: out->dtype_f16_count++; break;
        case YVEX_NATIVE_DTYPE_BF16: out->dtype_bf16_count++; break;
        case YVEX_NATIVE_DTYPE_F32: out->dtype_f32_count++; break;
        case YVEX_NATIVE_DTYPE_I64: out->dtype_i64_count++; break;
        case YVEX_NATIVE_DTYPE_I8: out->dtype_i8_count++; break;
        case YVEX_NATIVE_DTYPE_FP4: out->dtype_fp4_count++; break;
        case YVEX_NATIVE_DTYPE_F8_E4M3:
        case YVEX_NATIVE_DTYPE_F8_E5M2: out->dtype_f8_count++; break;
        default:
            if (strcmp(info->dtype_name, "F8_E8M0") == 0) {
                out->dtype_f8_e8m0_count++;
            } else {
                out->dtype_other_count++;
            }
            break;
        }
    }
    out->header_shard_count = table->header_read_count;
    out->header_tensor_count = table->count;
    out->header_bytes = table->header_bytes;
    out->declared_tensor_bytes = table->summary.total_tensor_bytes;
    if (index->has_declared_total_size &&
        index->declared_total_size != out->declared_tensor_bytes) {
        yvex_source_verification_add_blocker(out,
                                             "shard-index-size-mismatch");
    }
    rc = YVEX_OK;
cleanup:
    free(sorted);
    yvex_native_weight_table_close(table);
    return rc;
}

void yvex_source_derived_inventory_free(
    yvex_source_derived_inventory *inventory)
{
    size_t i;

    if (!inventory) return;
    for (i = 0u; i < inventory->count; ++i) {
        free(inventory->rows[i].tensor);
        free(inventory->rows[i].shard);
    }
    free(inventory->rows);
    memset(inventory, 0, sizeof(*inventory));
}

/* Coordinates indexed or indexless inventory verification without payload reads. */
int yvex_source_inventory_verify(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_source_derived_inventory *derived,
    yvex_error *err)
{
    source_shards shards;
    source_index index;
    int rc;

    if (!options || !out || !options->identity) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_inventory_verify",
                       "source verification options and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&shards, 0, sizeof(shards));
    memset(&index, 0, sizeof(index));
    if (derived) memset(derived, 0, sizeof(*derived));
    rc = source_scan_root(options->source_path, &shards, out, err);
    if (rc != YVEX_OK) goto cleanup;
    if (strcmp(options->identity->upstream_inventory_authority,
               "upstream-index") == 0) {
        rc = source_verify_index(options, &index, out, err);
        if (rc != YVEX_OK) goto cleanup;
        source_verify_index_shards(&index, &shards, out);
    } else if (strcmp(options->identity->upstream_inventory_authority,
                      "header-derived") == 0) {
        rc = source_verify_upstream_indexless(options, &shards, out, err);
        if (rc != YVEX_OK) goto cleanup;
    } else {
        yvex_source_verification_add_blocker(out,
                                             "unsupported-inventory-authority");
    }
    rc = source_verify_headers(options, &shards, &index, out, derived, err);
cleanup:
    source_index_free(&index);
    source_shards_free(&shards);
    return rc;
}
