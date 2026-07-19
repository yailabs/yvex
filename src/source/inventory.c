/* Owner: source inventory.
 * Owns: canonical shard catalog, one header pass, tensor index, and derived rows.
 * Does not own: payload reads, role mapping, transforms, manifest writes, or rendering.
 * Invariants: indexed and indexless authority modes remain explicit and deterministic.
 * Boundary: header inventory is not payload trust or transform execution.
 * Purpose: build the retained shard and tensor inventory from verified metadata.
 * Inputs: source roots, pinned index facts, parsed headers, and caller outputs.
 * Effects: allocates immutable indexes and reads only index and header metadata.
 * Failure: missing, duplicate, malformed, overflowed, or inconsistent facts refuse. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <yvex/internal/core.h>
#include <yvex/internal/source.h>
#include <yvex/internal/source_payload.h>

/* Purpose: publish one typed inventory refusal without duplicating error-state transitions. */
static int inventory_refuse(yvex_error *err,
                            yvex_status status,
                            const char *where,
                            const char *message) {
    yvex_error_set(err, status, where, message);
    return status;
}

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
} source_shard_sort_row;

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

struct yvex_source_tensor_snapshot {
    yvex_native_weight_table *table;
    yvex_source_shard_snapshot *shards;
    unsigned long long shard_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long identity;
    unsigned int references;
};

/* Purpose: copies a canonical shard catalog into snapshot-owned immutable storage.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_snapshot_copy_shards(yvex_source_tensor_snapshot *snapshot,
                                       const yvex_source_shard_snapshot *shards,
                                       unsigned long long shard_count,
                                       yvex_error *err) {
    unsigned long long index;

    if (!shards || shard_count == 0u)
        return YVEX_OK;
    if (shard_count > (unsigned long long)(SIZE_MAX / sizeof(shards[0]))) {
        return inventory_refuse(err, YVEX_ERR_BOUNDS, "source_tensor_snapshot",
            "source shard catalog allocation overflow");
    }
    snapshot->shards =
        (yvex_source_shard_snapshot *)calloc((size_t)shard_count, sizeof(snapshot->shards[0]));
    if (!snapshot->shards) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_tensor_snapshot",
            "source shard catalog allocation failed");
    }
    for (index = 0u; index < shard_count; ++index) {
        char *name = yvex_core_strdup(shards[index].canonical_name);
        if (!name) {
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_tensor_snapshot",
                "source shard name allocation failed");
        }
        snapshot->shards[index] = shards[index];
        snapshot->shards[index].canonical_name = name;
    }
    return YVEX_OK;
}

/* Purpose: project snapshot hash bytes facts while preserving the canonical source inventory invariants.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static unsigned long long
source_snapshot_hash_bytes(unsigned long long hash, const void *data, size_t length) {
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;

    for (i = 0u; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Purpose: project snapshot hash u64 facts while preserving the canonical source inventory invariants.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static unsigned long long source_snapshot_hash_u64(unsigned long long hash,
                                                   unsigned long long value) {
    unsigned char bytes[8];
    unsigned int i;

    for (i = 0u; i < sizeof(bytes); ++i)
        bytes[i] = (unsigned char)((value >> (i * 8u)) & 0xffu);
    return source_snapshot_hash_bytes(hash, bytes, sizeof(bytes));
}

/* Purpose: append canonical source inventory fields to a deterministic identity stream.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static unsigned long long source_snapshot_identity(const yvex_native_weight_table *table) {
    unsigned long long hash = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0u; table && i < table->count; ++i) {
        const yvex_native_weight_info *item = &table->items[i];
        hash = source_snapshot_hash_bytes(hash, item->name, strlen(item->name) + 1u);
        hash = source_snapshot_hash_bytes(hash, item->shard_path, strlen(item->shard_path) + 1u);
        hash = source_snapshot_hash_bytes(hash,
                                          yvex_native_dtype_name(item->dtype),
                                          strlen(yvex_native_dtype_name(item->dtype)) + 1u);
        hash = source_snapshot_hash_u64(hash, item->rank);
        {
            unsigned int dimension;
            for (dimension = 0u; dimension < item->rank; ++dimension)
                hash = source_snapshot_hash_u64(hash, item->dims[dimension]);
        }
        hash = source_snapshot_hash_u64(hash, item->data_start);
        hash = source_snapshot_hash_u64(hash, item->data_end);
    }
    return hash;
}

/* Purpose: transfers a finalized native table into one immutable retained snapshot.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_tensor_snapshot_take_table(yvex_source_tensor_snapshot **out,
                                           yvex_native_weight_table **table,
                                           unsigned long long shard_count,
                                           unsigned long long header_scan_count,
                                           yvex_error *err) {
    yvex_source_tensor_snapshot *snapshot;
    int rc;

    if (!out || !table || !*table) {
        return inventory_refuse(err, YVEX_ERR_INVALID_ARG, "source_tensor_snapshot",
            "output and native tensor table are required");
    }
    *out = NULL;
    rc = yvex_native_weight_table_finalize(*table, err);
    if (rc != YVEX_OK)
        return rc;
    snapshot = (yvex_source_tensor_snapshot *)calloc(1u, sizeof(*snapshot));
    if (!snapshot) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_tensor_snapshot",
            "source tensor snapshot allocation failed");
    }
    snapshot->table = *table;
    snapshot->shard_count = shard_count;
    snapshot->header_scan_count = header_scan_count;
    snapshot->identity = source_snapshot_identity(*table);
    snapshot->references = 1u;
    *table = NULL;
    *out = snapshot;
    return YVEX_OK;
}

/* Purpose: transfers a finalized tensor table and copies its one-pass shard geometry.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_tensor_snapshot_take_table_with_shards(yvex_source_tensor_snapshot **out,
                                                       yvex_native_weight_table **table,
                                                       const yvex_source_shard_snapshot *shards,
                                                       unsigned long long shard_count,
                                                       unsigned long long header_scan_count,
                                                       yvex_error *err) {
    yvex_source_tensor_snapshot *snapshot = NULL;
    unsigned long long index;
    int rc;

    if (!shards || shard_count == 0u) {
        return inventory_refuse(err, YVEX_ERR_INVALID_ARG, "source_tensor_snapshot",
            "a nonempty canonical shard catalog is required");
    }
    for (index = 0u; index < shard_count; ++index) {
        if (!shards[index].canonical_name || shards[index].canonical_id != index ||
            (index &&
             strcmp(shards[index - 1u].canonical_name, shards[index].canonical_name) >= 0)) {
            return inventory_refuse(err, YVEX_ERR_FORMAT, "source_tensor_snapshot",
                "source shard catalog is not canonical and unique");
        }
    }
    rc = yvex_source_tensor_snapshot_take_table(
        &snapshot, table, shard_count, header_scan_count, err);
    if (rc != YVEX_OK)
        return rc;
    rc = source_snapshot_copy_shards(snapshot, shards, shard_count, err);
    if (rc != YVEX_OK) {
        yvex_source_tensor_snapshot_release(snapshot);
        return rc;
    }
    *out = snapshot;
    return YVEX_OK;
}

/* Purpose: project tensor snapshot retain facts while preserving the canonical source inventory invariants.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
void yvex_source_tensor_snapshot_retain(yvex_source_tensor_snapshot *snapshot) {
    if (snapshot && snapshot->references < UINT_MAX)
        snapshot->references++;
}

/* Purpose: release one retained immutable tensor snapshot reference.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source inventory; cleanup remains deterministic.
 * Failure: null or released source inventory handles remain harmless.
 * Boundary: header inventory is not payload trust or transform execution. */
void yvex_source_tensor_snapshot_release(yvex_source_tensor_snapshot *snapshot) {
    unsigned long long index;

    if (!snapshot || snapshot->references == 0u)
        return;
    snapshot->references--;
    if (snapshot->references != 0u)
        return;
    for (index = 0u; snapshot->shards && index < snapshot->shard_count; ++index)
        free((char *)snapshot->shards[index].canonical_name);
    free(snapshot->shards);
    yvex_native_weight_table_close(snapshot->table);
    free(snapshot);
}

/* Purpose: returns one borrowed immutable shard fact in canonical order.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
const yvex_source_shard_snapshot *
yvex_source_tensor_snapshot_shard_at(const yvex_source_tensor_snapshot *snapshot,
                                     unsigned long long index) {
    return snapshot && snapshot->shards && index < snapshot->shard_count ? &snapshot->shards[index]
                                                                         : NULL;
}

/* Purpose: binary-searches the immutable canonical shard catalog without allocation.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
const yvex_source_shard_snapshot *
yvex_source_tensor_snapshot_shard_find(const yvex_source_tensor_snapshot *snapshot,
                                       const char *canonical_name) {
    unsigned long long lower = 0u;
    unsigned long long upper;

    if (!snapshot || !snapshot->shards || !canonical_name)
        return NULL;
    upper = snapshot->shard_count;
    while (lower < upper) {
        unsigned long long middle = lower + (upper - lower) / 2u;
        int order = strcmp(snapshot->shards[middle].canonical_name, canonical_name);
        if (order == 0)
            return &snapshot->shards[middle];
        if (order < 0)
            lower = middle + 1u;
        else
            upper = middle;
    }
    return NULL;
}

/* Purpose: reports whether geometry came from the canonical retained header pass.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_tensor_snapshot_has_shard_catalog(const yvex_source_tensor_snapshot *snapshot) {
    return snapshot && snapshot->shards && snapshot->shard_count != 0u;
}

/* Purpose: return the immutable source inventory entry at a checked ordinal.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
const yvex_native_weight_info *
yvex_source_tensor_snapshot_at(const yvex_source_tensor_snapshot *snapshot,
                               unsigned long long index) {
    return snapshot ? yvex_native_weight_table_at(snapshot->table, index) : NULL;
}

/* Purpose: locate the source inventory entry associated with a canonical key.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
const yvex_native_weight_info *
yvex_source_tensor_snapshot_find(const yvex_source_tensor_snapshot *snapshot, const char *name) {
    return snapshot ? yvex_native_weight_table_find(snapshot->table, name) : NULL;
}

/* Purpose: resolve the stable ordinal for one canonical source inventory key.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_tensor_snapshot_find_index(const yvex_source_tensor_snapshot *snapshot,
                                           const char *name,
                                           unsigned long long *index) {
    const yvex_native_weight_info *item;

    if (!snapshot || !name || !index)
        return 0;
    item = yvex_native_weight_table_find(snapshot->table, name);
    if (!item)
        return 0;
    *index = (unsigned long long)(item - snapshot->table->items);
    return 1;
}

/* Purpose: project tensor snapshot facts get facts while preserving the canonical source inventory invariants.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_tensor_snapshot_facts_get(const yvex_source_tensor_snapshot *snapshot,
                                          yvex_source_tensor_snapshot_facts *out,
                                          yvex_error *err) {
    if (!snapshot || !out) {
        return inventory_refuse(err, YVEX_ERR_INVALID_ARG, "source_tensor_snapshot",
            "snapshot and facts output are required");
    }
    memset(out, 0, sizeof(*out));
    out->tensor_count = snapshot->table->count;
    out->shard_count = snapshot->shard_count;
    out->header_scan_count = snapshot->header_scan_count;
    out->payload_bytes_read = snapshot->payload_bytes_read;
    out->lookup_count = snapshot->table->lookup_count;
    out->collision_count = snapshot->table->collision_count;
    out->maximum_probe = snapshot->table->maximum_probe;
    out->identity = snapshot->identity;
    return YVEX_OK;
}

/* Purpose: appends one unique tensor-to-shard assignment with owned strings.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int
source_index_append(source_index *index, const char *tensor, const char *shard, yvex_error *err) {
    source_index_entry *next;
    size_t cap;

    if (index->count == index->cap) {
        cap = index->cap ? index->cap * 2u : 256u;
        if (cap < index->cap || cap > SIZE_MAX / sizeof(index->items[0])) {
            return inventory_refuse(err, YVEX_ERR_BOUNDS, "source_index", "shard index allocation overflow");
        }
        next = (source_index_entry *)realloc(index->items, cap * sizeof(index->items[0]));
        if (!next) {
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_index", "shard index allocation failed");
        }
        index->items = next;
        index->cap = cap;
    }
    memset(&index->items[index->count], 0, sizeof(index->items[0]));
    index->items[index->count].tensor = yvex_core_strdup(tensor);
    index->items[index->count].shard = yvex_core_strdup(shard);
    if (!index->items[index->count].tensor || !index->items[index->count].shard) {
        free(index->items[index->count].tensor);
        free(index->items[index->count].shard);
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_index", "shard index row allocation failed");
    }
    index->count++;
    return YVEX_OK;
}

/* Purpose: release parsed upstream tensor-name index rows.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source inventory; cleanup remains deterministic.
 * Failure: null or released source inventory handles remain harmless.
 * Boundary: header inventory is not payload trust or transform execution. */
static void source_index_free(source_index *index) {
    size_t i;

    if (!index)
        return;
    for (i = 0; i < index->count; ++i) {
        free(index->items[i].tensor);
        free(index->items[i].shard);
    }
    free(index->items);
    memset(index, 0, sizeof(*index));
}

/* Purpose: order upstream tensor-name index rows for deterministic binary search. */
static int source_index_compare(const void *left, const void *right) {
    const source_index_entry *a = (const source_index_entry *)left;
    const source_index_entry *b = (const source_index_entry *)right;
    return strcmp(a->tensor, b->tensor);
}

/* Purpose: parses the upstream declared payload size with duplicate-field refusal.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_parse_index_metadata(yvex_json *json, source_index *index) {
    char key[YVEX_JSON_KEY_CAP];

    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{')
        return 0;
    for (;;) {
        yvex_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return 1;
        }
        if (!yvex_json_string(json, key, sizeof(key)))
            return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':')
            return 0;
        if (strcmp(key, "total_size") == 0) {
            if (index->has_declared_total_size || !yvex_json_u64(json, &index->declared_total_size))
                return 0;
            index->has_declared_total_size = 1;
        } else if (!yvex_json_skip_value(json)) {
            return 0;
        }
        yvex_json_space(json);
        if (json->cursor >= json->end)
            return 0;
        if (*json->cursor == '}')
            continue;
        if (*json->cursor++ != ',')
            return 0;
    }
}

/* Purpose: parses all unique tensor-to-shard assignments from the upstream weight map.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_parse_weight_map(yvex_json *json, source_index *index, yvex_error *err) {
    char tensor[YVEX_JSON_KEY_CAP];
    char shard[YVEX_PATH_CAP];

    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{')
        return 0;
    for (;;) {
        int rc;
        yvex_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return index->count > 0u;
        }
        if (!yvex_json_string(json, tensor, sizeof(tensor)))
            return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':' ||
            !yvex_json_string(json, shard, sizeof(shard)))
            return 0;
        rc = source_index_append(index, tensor, shard, err);
        if (rc != YVEX_OK)
            return -1;
        yvex_json_space(json);
        if (json->cursor >= json->end)
            return 0;
        if (*json->cursor == '}')
            continue;
        if (*json->cursor++ != ',')
            return 0;
    }
}

/* Purpose: parses a complete upstream index and requires metadata plus a nonempty map.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int
source_parse_index_json(const char *data, size_t length, source_index *index, yvex_error *err) {
    yvex_json json;
    char key[YVEX_JSON_KEY_CAP];
    unsigned int seen = 0u;
    size_t i;

    yvex_json_init(&json, data, length);
    yvex_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{')
        return 0;
    for (;;) {
        int parsed = 1;
        yvex_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_json_string(&json, key, sizeof(key)))
            return 0;
        yvex_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':')
            return 0;
        if (strcmp(key, "metadata") == 0) {
            if ((seen & 1u) || !source_parse_index_metadata(&json, index))
                return 0;
            seen |= 1u;
        } else if (strcmp(key, "weight_map") == 0) {
            if (seen & 2u)
                return 0;
            parsed = source_parse_weight_map(&json, index, err);
            if (parsed < 0)
                return -1;
            if (!parsed)
                return 0;
            seen |= 2u;
        } else if (!yvex_json_skip_value(&json)) {
            return 0;
        }
        yvex_json_space(&json);
        if (json.cursor >= json.end)
            return 0;
        if (*json.cursor == '}')
            continue;
        if (*json.cursor++ != ',')
            return 0;
    }
    if (!yvex_json_complete(&json) || !(seen & 2u))
        return 0;
    qsort(index->items, index->count, sizeof(index->items[0]), source_index_compare);
    for (i = 1u; i < index->count; ++i) {
        if (strcmp(index->items[i - 1u].tensor, index->items[i].tensor) == 0) {
            index->duplicate_tensor = 1;
            return 0;
        }
    }
    return 1;
}

/* Purpose: locate the source inventory entry associated with a canonical key. */
static source_index_entry *source_index_find(source_index *index, const char *tensor) {
    size_t low = 0u;
    size_t high = index ? index->count : 0u;

    while (low < high) {
        size_t mid = low + (high - low) / 2u;
        int cmp = strcmp(tensor, index->items[mid].tensor);
        if (cmp == 0)
            return &index->items[mid];
        if (cmp < 0)
            high = mid;
        else
            low = mid + 1u;
    }
    return NULL;
}

/* Purpose: adds one unique root shard and its checked local file size.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_shards_append(source_shards *shards,
                                const char *name,
                                unsigned long long size,
                                yvex_error *err) {
    char **next_names;
    unsigned long long *next_sizes;
    size_t cap;

    if (shards->count == shards->cap) {
        cap = shards->cap ? shards->cap * 2u : 64u;
        next_names = (char **)realloc(shards->names, cap * sizeof(shards->names[0]));
        if (!next_names) {
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_shards", "shard name allocation failed");
        }
        shards->names = next_names;
        next_sizes = (unsigned long long *)realloc(shards->sizes, cap * sizeof(shards->sizes[0]));
        if (!next_sizes) {
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_shards", "shard size allocation failed");
        }
        shards->sizes = next_sizes;
        shards->cap = cap;
    }
    shards->names[shards->count] = yvex_core_strdup(name);
    if (!shards->names[shards->count]) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_shards", "shard name copy failed");
    }
    shards->sizes[shards->count] = size;
    shards->count++;
    return YVEX_OK;
}

/* Purpose: release the canonical shard catalog and its owned names.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source inventory; cleanup remains deterministic.
 * Failure: null or released source inventory handles remain harmless.
 * Boundary: header inventory is not payload trust or transform execution. */
static void source_shards_free(source_shards *shards) {
    size_t i;

    if (!shards)
        return;
    for (i = 0; i < shards->count; ++i)
        free(shards->names[i]);
    free(shards->names);
    free(shards->sizes);
    memset(shards, 0, sizeof(*shards));
}

/* Purpose: parses the exact model-N-of-M shard grammar into sequence facts.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int
source_shard_name_parse(const char *name, unsigned int *index_out, unsigned int *total_out) {
    unsigned int index = 0u;
    unsigned int total = 0u;
    size_t i;

    if (!name || strlen(name) != strlen("model-00000-of-00000.safetensors") ||
        strncmp(name, "model-", 6u) != 0 || strncmp(name + 11u, "-of-", 4u) != 0 ||
        strcmp(name + 20u, ".safetensors") != 0)
        return 0;
    for (i = 6u; i < 11u; ++i) {
        if (!isdigit((unsigned char)name[i]))
            return 0;
        index = index * 10u + (unsigned int)(name[i] - '0');
    }
    for (i = 15u; i < 20u; ++i) {
        if (!isdigit((unsigned char)name[i]))
            return 0;
        total = total * 10u + (unsigned int)(name[i] - '0');
    }
    if (index == 0u || total == 0u || index > total)
        return 0;
    if (index_out)
        *index_out = index;
    if (total_out)
        *total_out = total;
    return 1;
}

/* Purpose: order canonical shard rows by numeric shard identity and relative name. */
static int source_shard_compare(const void *left, const void *right) {
    const source_shard_sort_row *a = (const source_shard_sort_row *)left;
    const source_shard_sort_row *b = (const source_shard_sort_row *)right;
    return strcmp(a->name, b->name);
}

/* Purpose: sorts shard names and keeps their corresponding byte sizes paired.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_shards_sort(source_shards *shards, yvex_error *err) {
    source_shard_sort_row *rows;
    size_t i;

    if (shards->count < 2u)
        return YVEX_OK;
    rows = (source_shard_sort_row *)calloc(shards->count, sizeof(*rows));
    if (!rows) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_shards_sort", "shard sort allocation failed");
    }
    for (i = 0u; i < shards->count; ++i) {
        rows[i].name = shards->names[i];
        rows[i].size = shards->sizes[i];
    }
    qsort(rows, shards->count, sizeof(*rows), source_shard_compare);
    for (i = 0u; i < shards->count; ++i) {
        shards->names[i] = rows[i].name;
        shards->sizes[i] = rows[i].size;
    }
    free(rows);
    return YVEX_OK;
}

/* Purpose: binary-searches the canonical sorted shard table. */
static long source_shards_find(const source_shards *shards, const char *name) {
    size_t lower = 0u;
    size_t upper;

    if (!shards || !name)
        return -1;
    upper = shards->count;
    while (lower < upper) {
        size_t middle = lower + (upper - lower) / 2u;
        int order = strcmp(shards->names[middle], name);
        if (order == 0)
            return (long)middle;
        if (order < 0)
            lower = middle + 1u;
        else
            upper = middle;
    }
    return -1;
}

/* Purpose: inventories only root safetensors shards and rejects unexpected shard forms.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_scan_root(const char *source_path,
                            source_shards *shards,
                            yvex_source_verification *out,
                            yvex_error *err) {
    DIR *dir;
    struct dirent *entry;

    dir = opendir(source_path);
    if (!dir) {
        yvex_error_setf(err,
                        YVEX_ERR_IO,
                        "source_inventory_root",
                        "cannot inspect source directory: %s",
                        source_path);
        return YVEX_ERR_IO;
    }
    for (;;) {
        char path[YVEX_PATH_CAP];
        struct stat st;
        unsigned int shard_index = 0u;
        unsigned int shard_total = 0u;
        int rc;

        errno = 0;
        entry = readdir(dir);
        if (!entry) {
            if (errno != 0) {
                yvex_source_verification_add_blocker(out, "source-directory-read-failed");
            }
            break;
        }
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (!yvex_source_path_join(path, sizeof(path), source_path, entry->d_name)) {
            yvex_source_verification_add_blocker(out, "source-entry-path-overflow");
            continue;
        }
        if (lstat(path, &st) != 0) {
            yvex_source_verification_add_blocker(out, "source-entry-unreadable");
            continue;
        }
        if (!S_ISREG(st.st_mode) || st.st_size < 0)
            continue;
        if (strlen(entry->d_name) <= strlen(".safetensors") ||
            strcmp(entry->d_name + strlen(entry->d_name) - strlen(".safetensors"),
                   ".safetensors") != 0)
            continue;
        if (!source_shard_name_parse(entry->d_name, &shard_index, &shard_total)) {
            yvex_source_verification_add_blocker(out, "unexpected-shard");
            continue;
        }
        if (!shards->declared_total) {
            shards->declared_total = shard_total;
        } else if (shards->declared_total != shard_total) {
            yvex_source_verification_add_blocker(out, "inconsistent-shard-set");
        }
        rc = source_shards_append(shards, entry->d_name, (unsigned long long)st.st_size, err);
        if (rc != YVEX_OK) {
            closedir(dir);
            return rc;
        }
        if (!yvex_source_checked_add_u64(&out->shard_bytes, (unsigned long long)st.st_size)) {
            out->footprint_overflow = 1;
            yvex_source_verification_add_blocker(out, "source-footprint-overflow");
        }
    }
    if (closedir(dir) != 0) {
        yvex_source_verification_add_blocker(out, "source-directory-close-failed");
    }
    {
        int rc = source_shards_sort(shards, err);
        if (rc != YVEX_OK)
            return rc;
    }
    out->shard_count = (unsigned long long)shards->count;
    if (!shards->count) {
        yvex_source_verification_add_blocker(out, "missing-source-shards");
    }
    if (shards->declared_total && shards->count != (size_t)shards->declared_total) {
        yvex_source_verification_add_blocker(out, "incomplete-shard-set");
    }
    for (size_t i = 0u; i < shards->count; ++i) {
        unsigned int index = 0u;
        unsigned int prior_index = 0u;
        if (i > 0u && source_shard_name_parse(shards->names[i - 1u], &prior_index, NULL) &&
            source_shard_name_parse(shards->names[i], &index, NULL) && prior_index == index) {
            yvex_source_verification_add_blocker(out, "duplicate-source-shard");
        }
        if (!source_shard_name_parse(shards->names[i], &index, NULL) || index != i + 1u) {
            yvex_source_verification_add_blocker(out, "discontinuous-shard-set");
            break;
        }
    }
    return YVEX_OK;
}

/* Purpose: adds one unique official snapshot file and its declared metadata size.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_upstream_append(source_upstream_inventory *inventory,
                                  const char *name,
                                  unsigned long long size,
                                  yvex_error *err) {
    source_upstream_file *next;
    size_t cap;

    if (inventory->count == inventory->cap) {
        cap = inventory->cap ? inventory->cap * 2u : 64u;
        next = (source_upstream_file *)realloc(inventory->items, cap * sizeof(inventory->items[0]));
        if (!next) {
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_upstream_inventory",
                "upstream inventory allocation failed");
        }
        inventory->items = next;
        inventory->cap = cap;
    }
    inventory->items[inventory->count].name = yvex_core_strdup(name);
    if (!inventory->items[inventory->count].name) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_upstream_inventory",
            "upstream inventory name allocation failed");
    }
    inventory->items[inventory->count].size = size;
    inventory->count++;
    return YVEX_OK;
}

/* Purpose: release parsed upstream inventory rows and their owned tensor names.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source inventory; cleanup remains deterministic.
 * Failure: null or released source inventory handles remain harmless.
 * Boundary: header inventory is not payload trust or transform execution. */
static void source_upstream_free(source_upstream_inventory *inventory) {
    size_t i;

    for (i = 0u; inventory && i < inventory->count; ++i) {
        free(inventory->items[i].name);
    }
    if (inventory) {
        free(inventory->items);
        memset(inventory, 0, sizeof(*inventory));
    }
}

/* Purpose: parses one file row from a pinned upstream snapshot inventory.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int
source_upstream_parse_file(yvex_json *json, source_upstream_inventory *inventory, yvex_error *err) {
    char key[YVEX_JSON_KEY_CAP];
    char name[YVEX_PATH_CAP] = "";
    unsigned long long size = 0u;
    unsigned int seen = 0u;

    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '{')
        return 0;
    for (;;) {
        yvex_json_space(json);
        if (json->cursor < json->end && *json->cursor == '}') {
            json->cursor++;
            return seen == 3u && source_upstream_append(inventory, name, size, err) == YVEX_OK;
        }
        if (!yvex_json_string(json, key, sizeof(key)))
            return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':')
            return 0;
        if (strcmp(key, "path") == 0) {
            if ((seen & 1u) || !yvex_json_string(json, name, sizeof(name)))
                return 0;
            seen |= 1u;
        } else if (strcmp(key, "size_bytes") == 0) {
            if ((seen & 2u) || !yvex_json_u64(json, &size))
                return 0;
            seen |= 2u;
        } else if (!yvex_json_skip_value(json)) {
            return 0;
        }
        yvex_json_space(json);
        if (json->cursor >= json->end)
            return 0;
        if (*json->cursor == '}')
            continue;
        if (*json->cursor++ != ',')
            return 0;
    }
}

/* Purpose: parses the bounded official file list without accepting duplicates.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_upstream_parse_files(yvex_json *json,
                                       source_upstream_inventory *inventory,
                                       yvex_error *err) {
    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[')
        return 0;
    yvex_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']')
        return 0;
    for (;;) {
        if (!source_upstream_parse_file(json, inventory, err))
            return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end)
            return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            return inventory->count > 0u;
        }
        if (*json->cursor++ != ',')
            return 0;
    }
}

/* Purpose: parses and verifies repository/revision identity for an indexless snapshot.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_upstream_parse(const char *data,
                                 size_t length,
                                 source_upstream_inventory *inventory,
                                 yvex_error *err) {
    yvex_json json;
    char key[YVEX_JSON_KEY_CAP];
    char schema[64];
    unsigned int seen = 0u;

    yvex_json_init(&json, data, length);
    yvex_json_space(&json);
    if (json.cursor >= json.end || *json.cursor++ != '{')
        return 0;
    for (;;) {
        yvex_json_space(&json);
        if (json.cursor < json.end && *json.cursor == '}') {
            json.cursor++;
            break;
        }
        if (!yvex_json_string(&json, key, sizeof(key)))
            return 0;
        yvex_json_space(&json);
        if (json.cursor >= json.end || *json.cursor++ != ':')
            return 0;
        if (strcmp(key, "schema") == 0) {
            if ((seen & 1u) || !yvex_json_string(&json, schema, sizeof(schema)) ||
                strcmp(schema, "yvex.source_upstream_inventory.v1") != 0)
                return 0;
            seen |= 1u;
        } else if (strcmp(key, "repository") == 0) {
            if ((seen & 2u) || !yvex_json_string(&json, inventory->repo, sizeof(inventory->repo)))
                return 0;
            seen |= 2u;
        } else if (strcmp(key, "revision") == 0) {
            if ((seen & 4u) ||
                !yvex_json_string(&json, inventory->revision, sizeof(inventory->revision)))
                return 0;
            seen |= 4u;
        } else if (strcmp(key, "files") == 0) {
            if ((seen & 8u) || !source_upstream_parse_files(&json, inventory, err))
                return 0;
            seen |= 8u;
        } else if (!yvex_json_skip_value(&json)) {
            return 0;
        }
        yvex_json_space(&json);
        if (json.cursor >= json.end)
            return 0;
        if (*json.cursor == '}')
            continue;
        if (*json.cursor++ != ',')
            return 0;
    }
    return yvex_json_complete(&json) && seen == 15u;
}

/* Purpose: verifies the official index file, provider identity, and declared shard map.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_verify_index(const yvex_source_verify_options *options,
                               source_index *index,
                               yvex_source_verification *out,
                               yvex_error *err) {
    char path[YVEX_PATH_CAP];
    char *data;
    size_t length;
    int parsed;
    int rc;

    if (!yvex_source_path_join(
            path, sizeof(path), options->source_path, options->identity->upstream_index_path) ||
        !yvex_source_regular_file(path, NULL)) {
        yvex_source_verification_add_blocker(out, "missing-shard-index");
        return YVEX_OK;
    }
    out->shard_index_present = 1;
    data = yvex_read_bounded_file(path, SOURCE_INDEX_CAP, &length, err);
    if (!data) {
        yvex_source_verification_add_blocker(out, "malformed-shard-index");
        return yvex_error_code(err) == YVEX_ERR_NOMEM ? YVEX_ERR_NOMEM : YVEX_OK;
    }
    parsed = source_parse_index_json(data, length, index, err);
    free(data);
    if (parsed < 0)
        return yvex_error_code(err);
    if (!parsed) {
        yvex_source_verification_add_blocker(
            out, index->duplicate_tensor ? "duplicate-index-tensor" : "malformed-shard-index");
        return YVEX_OK;
    }
    out->shard_index_valid = 1;
    out->indexed_tensor_count = (unsigned long long)index->count;
    rc = yvex_source_provenance_verify_file(
        options, options->identity->upstream_index_path, 1, out, err);
    if (rc != YVEX_OK)
        return rc;
    snprintf(out->inventory_authority, sizeof(out->inventory_authority), "%s", "upstream-index");
    return YVEX_OK;
}

/* Purpose: reconciles every indexed shard assignment with the root shard inventory.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
static void source_verify_index_shards(source_index *index,
                                       const source_shards *shards,
                                       yvex_source_verification *out) {
    unsigned char *referenced;
    size_t i;
    unsigned long long unique = 0u;

    if (!out->shard_index_valid)
        return;
    referenced = (unsigned char *)calloc(shards->count ? shards->count : 1u, sizeof(*referenced));
    if (!referenced) {
        yvex_source_verification_add_blocker(out, "shard-reference-allocation-failed");
        return;
    }
    for (i = 0; i < index->count; ++i) {
        long shard_index = source_shards_find(shards, index->items[i].shard);
        if (shard_index < 0) {
            yvex_source_verification_add_blocker(out, "missing-referenced-shard");
        } else if (!referenced[(size_t)shard_index]) {
            referenced[(size_t)shard_index] = 1u;
            unique++;
        }
    }
    out->referenced_shard_count = unique;
    for (i = 0; i < shards->count; ++i) {
        if (!referenced[i]) {
            yvex_source_verification_add_blocker(out, "unexpected-shard");
        }
    }
    free(referenced);
}

/* Purpose: verifies a deliberately indexless official snapshot before deriving a map.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_verify_upstream_indexless(const yvex_source_verify_options *options,
                                            const source_shards *shards,
                                            yvex_source_verification *out,
                                            yvex_error *err) {
    source_upstream_inventory upstream;
    char *data;
    size_t length;
    size_t i;

    memset(&upstream, 0, sizeof(upstream));
    if (!options->upstream_inventory_path ||
        !yvex_source_regular_file(options->upstream_inventory_path, NULL)) {
        yvex_source_verification_add_blocker(out, "missing-upstream-inventory");
        return YVEX_OK;
    }
    data = yvex_read_bounded_file(
        options->upstream_inventory_path, SOURCE_UPSTREAM_INVENTORY_CAP, &length, err);
    if (!data) {
        yvex_source_verification_add_blocker(out, "malformed-upstream-inventory");
        return yvex_error_code(err) == YVEX_ERR_NOMEM ? YVEX_ERR_NOMEM : YVEX_OK;
    }
    if (!source_upstream_parse(data, length, &upstream, err)) {
        free(data);
        source_upstream_free(&upstream);
        yvex_source_verification_add_blocker(out, "malformed-upstream-inventory");
        return YVEX_OK;
    }
    free(data);
    if (strcmp(upstream.repo, options->identity->upstream_repo_id) != 0 ||
        strcmp(upstream.revision, options->identity->upstream_revision) != 0) {
        yvex_source_verification_add_blocker(out, "stale-upstream-inventory");
    }
    for (i = 0u; i < upstream.count; ++i) {
        long local = source_shards_find(shards, upstream.items[i].name);
        if (local < 0 || shards->sizes[local] != upstream.items[i].size) {
            yvex_source_verification_add_blocker(out, "upstream-local-inventory-drift");
        }
    }
    if (upstream.count != shards->count) {
        yvex_source_verification_add_blocker(out, "upstream-local-inventory-drift");
    }
    if (!yvex_source_verification_has_blocker(out, "upstream-local-inventory-drift") &&
        !yvex_source_verification_has_blocker(out, "stale-upstream-inventory")) {
        snprintf(
            out->inventory_authority, sizeof(out->inventory_authority), "%s", "header-derived");
    }
    source_upstream_free(&upstream);
    return YVEX_OK;
}

/* Purpose: order native tensor facts by canonical tensor name for coverage comparison. */
static int source_native_info_compare(const void *left, const void *right) {
    const yvex_native_weight_info *const *a = (const yvex_native_weight_info *const *)left;
    const yvex_native_weight_info *const *b = (const yvex_native_weight_info *const *)right;
    return strcmp((*a)->name, (*b)->name);
}

/* Purpose: builds deterministic owned tensor-to-shard rows from the canonical header table.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source inventory state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_derived_build(const yvex_native_weight_table *table,
                                yvex_source_derived_inventory *derived,
                                yvex_error *err) {
    const yvex_native_weight_info **sorted;
    size_t i;

    if (!derived)
        return YVEX_OK;
    sorted = (const yvex_native_weight_info **)calloc((size_t)table->count, sizeof(sorted[0]));
    if (!sorted) {
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_derived_inventory",
            "derived inventory sort allocation failed");
    }
    derived->rows =
        (yvex_source_inventory_row *)calloc((size_t)table->count, sizeof(derived->rows[0]));
    if (!derived->rows) {
        free(sorted);
        return inventory_refuse(err, YVEX_ERR_NOMEM, "source_derived_inventory",
            "derived inventory row allocation failed");
    }
    for (i = 0u; i < (size_t)table->count; ++i)
        sorted[i] = &table->items[i];
    qsort(sorted, (size_t)table->count, sizeof(sorted[0]), source_native_info_compare);
    for (i = 0u; i < (size_t)table->count; ++i) {
        derived->rows[i].tensor = yvex_core_strdup(sorted[i]->name);
        derived->rows[i].shard = yvex_core_strdup(sorted[i]->shard_path);
        if (!derived->rows[i].tensor || !derived->rows[i].shard) {
            derived->count = i + 1u;
            free(sorted);
            yvex_source_derived_inventory_free(derived);
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_derived_inventory",
                "derived inventory string allocation failed");
        }
    }
    derived->count = (size_t)table->count;
    free(sorted);
    return YVEX_OK;
}

/* Purpose: performs the sole header pass and reconciles unique tensors with its authority.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
static int source_verify_headers(const yvex_source_verify_options *options,
                                 const source_shards *shards,
                                 source_index *index,
                                 yvex_source_verification *out,
                                 yvex_source_derived_inventory *derived,
                                 yvex_source_tensor_snapshot **snapshot,
                                 yvex_error *err) {
    yvex_native_weight_table *table;
    yvex_source_shard_snapshot *shard_facts = NULL;
    size_t i;
    int mismatch = 0;
    int shard_catalog_complete = 1;
    int indexed = strcmp(out->inventory_authority, "upstream-index") == 0;
    int rc = YVEX_OK;

    table = (yvex_native_weight_table *)calloc(1u, sizeof(*table));
    if (!table) {
        yvex_error_set(
            err, YVEX_ERR_NOMEM, "source_verify_headers", "native header table allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (snapshot) {
        shard_facts = (yvex_source_shard_snapshot *)calloc(shards->count, sizeof(shard_facts[0]));
        if (!shard_facts) {
            yvex_native_weight_table_close(table);
            return inventory_refuse(err, YVEX_ERR_NOMEM, "source_verify_headers",
                "retained shard geometry allocation failed");
        }
    }
    out->header_scan_count++;
    for (i = 0; i < shards->count; ++i) {
        char path[YVEX_PATH_CAP];
        yvex_error header_error;
        yvex_safetensors_file_facts file_facts;
        unsigned long long before = table->count;
        unsigned long long row;

        rc = yvex_source_provenance_verify_file(options, shards->names[i], 0, out, err);
        if (rc != YVEX_OK)
            goto cleanup;
        if (!yvex_source_path_join(path, sizeof(path), options->source_path, shards->names[i])) {
            yvex_source_verification_add_blocker(out, "invalid-safetensors-header");
            shard_catalog_complete = 0;
            continue;
        }
        yvex_error_clear(&header_error);
        memset(&file_facts, 0, sizeof(file_facts));
        rc = yvex_safetensors_read_header_file_with_facts(
            path, shards->names[i], table, &file_facts, &header_error);
        if (rc == YVEX_ERR_NOMEM) {
            yvex_error_set(err,
                           YVEX_ERR_NOMEM,
                           "source_verify_headers",
                           "native header inventory allocation failed");
            goto cleanup;
        }
        if (rc != YVEX_OK) {
            if (strcmp(yvex_error_where(&header_error), "native_weight_add") == 0 &&
                strncmp(yvex_error_message(&header_error),
                        "duplicate tensor name:",
                        strlen("duplicate tensor name:")) == 0) {
                yvex_source_verification_add_blocker(out, "duplicate-header-tensor");
                mismatch = 1;
            } else {
                yvex_source_verification_add_blocker(out, "invalid-safetensors-header");
            }
            shard_catalog_complete = 0;
            rc = YVEX_OK;
            continue;
        }
        if (shard_facts) {
            shard_facts[i].canonical_id = (unsigned long long)i;
            shard_facts[i].canonical_name = shards->names[i];
            shard_facts[i].file_bytes = file_facts.file_bytes;
            shard_facts[i].data_region_offset = file_facts.data_region_offset;
            shard_facts[i].payload_bytes = file_facts.payload_bytes;
        }
        for (row = before; row < table->count && indexed; ++row) {
            source_index_entry *entry = source_index_find(index, table->items[row].name);
            if (!entry || strcmp(entry->shard, shards->names[i]) != 0) {
                mismatch = 1;
            } else {
                entry->seen_in_header = 1;
            }
        }
    }
    rc = yvex_native_weight_table_finalize(table, err);
    if (rc != YVEX_OK)
        goto cleanup;
    if (indexed) {
        for (i = 0; i < index->count; ++i) {
            if (!index->items[i].seen_in_header)
                mismatch = 1;
        }
        if (mismatch || table->count != index->count) {
            yvex_source_verification_add_blocker(out, "shard-index-header-mismatch");
        } else {
            out->shard_index_headers_match = 1;
        }
    } else if (!mismatch) {
        out->shard_index_headers_match = 1;
        out->indexed_tensor_count = table->count;
        out->referenced_shard_count = shards->count;
        rc = source_derived_build(table, derived, err);
        if (rc != YVEX_OK)
            goto cleanup;
    }
    for (i = 0; i < (size_t)table->count; ++i) {
        const yvex_native_weight_info *info = &table->items[i];
        if (info->rank > out->max_tensor_rank)
            out->max_tensor_rank = info->rank;
        switch (info->dtype) {
        case YVEX_NATIVE_DTYPE_F16:
            out->dtype_f16_count++;
            break;
        case YVEX_NATIVE_DTYPE_BF16:
            out->dtype_bf16_count++;
            break;
        case YVEX_NATIVE_DTYPE_F32:
            out->dtype_f32_count++;
            break;
        case YVEX_NATIVE_DTYPE_I64:
            out->dtype_i64_count++;
            break;
        case YVEX_NATIVE_DTYPE_I8:
            out->dtype_i8_count++;
            break;
        case YVEX_NATIVE_DTYPE_FP4:
            out->dtype_fp4_count++;
            break;
        case YVEX_NATIVE_DTYPE_F8_E4M3:
        case YVEX_NATIVE_DTYPE_F8_E5M2:
            out->dtype_f8_count++;
            break;
        case YVEX_NATIVE_DTYPE_F8_E8M0:
            out->dtype_f8_e8m0_count++;
            break;
        default:
            out->dtype_other_count++;
            break;
        }
    }
    out->header_shard_count = table->header_read_count;
    out->header_tensor_count = table->count;
    out->header_bytes = table->header_bytes;
    out->declared_tensor_bytes = table->summary.total_tensor_bytes;
    if (index->has_declared_total_size &&
        index->declared_total_size != out->declared_tensor_bytes) {
        yvex_source_verification_add_blocker(out, "shard-index-size-mismatch");
    }
    if (snapshot && shard_catalog_complete) {
        rc = yvex_source_tensor_snapshot_take_table_with_shards(snapshot,
                                                                &table,
                                                                shard_facts,
                                                                (unsigned long long)shards->count,
                                                                out->header_scan_count,
                                                                err);
        if (rc != YVEX_OK)
            goto cleanup;
    }
    rc = YVEX_OK;
cleanup:
    free(shard_facts);
    yvex_native_weight_table_close(table);
    return rc;
}

/* Purpose: release the complete derived inventory, retained snapshot, and all owned rows.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: releases only resources owned by source inventory; cleanup remains deterministic.
 * Failure: null or released source inventory handles remain harmless.
 * Boundary: header inventory is not payload trust or transform execution. */
void yvex_source_derived_inventory_free(yvex_source_derived_inventory *inventory) {
    size_t i;

    if (!inventory)
        return;
    for (i = 0u; i < inventory->count; ++i) {
        free(inventory->rows[i].tensor);
        free(inventory->rows[i].shard);
    }
    free(inventory->rows);
    memset(inventory, 0, sizeof(*inventory));
}

/* Purpose: coordinates indexed or indexless inventory verification without payload reads.
 * Inputs: typed source inventory arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source inventory state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: header inventory is not payload trust or transform execution. */
int yvex_source_inventory_verify(const yvex_source_verify_options *options,
                                 yvex_source_verification *out,
                                 yvex_source_derived_inventory *derived,
                                 yvex_source_tensor_snapshot **snapshot,
                                 yvex_error *err) {
    source_shards shards;
    source_index index;
    int rc;

    if (!options || !out || !options->identity) {
        return inventory_refuse(err, YVEX_ERR_INVALID_ARG, "source_inventory_verify",
            "source verification options and output are required");
    }
    memset(&shards, 0, sizeof(shards));
    memset(&index, 0, sizeof(index));
    if (derived)
        memset(derived, 0, sizeof(*derived));
    if (snapshot)
        *snapshot = NULL;
    rc = source_scan_root(options->source_path, &shards, out, err);
    if (rc != YVEX_OK)
        goto cleanup;
    if (strcmp(options->identity->upstream_inventory_authority, "upstream-index") == 0) {
        rc = source_verify_index(options, &index, out, err);
        if (rc != YVEX_OK)
            goto cleanup;
        source_verify_index_shards(&index, &shards, out);
    } else if (strcmp(options->identity->upstream_inventory_authority, "header-derived") == 0) {
        rc = source_verify_upstream_indexless(options, &shards, out, err);
        if (rc != YVEX_OK)
            goto cleanup;
    } else {
        yvex_source_verification_add_blocker(out, "unsupported-inventory-authority");
    }
    rc = source_verify_headers(options, &shards, &index, out, derived, snapshot, err);
cleanup:
    source_index_free(&index);
    source_shards_free(&shards);
    return rc;
}
