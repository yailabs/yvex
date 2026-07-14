/*
 * yvex_shard_index.h - private reusable immutable shard-key index contract.
 *
 * Owner: src/core.
 * Owns: canonical shard-key ordering, uniqueness admission, and binary lookup.
 * Does not own: key strings, entry storage, files, source/artifact policy, or IO.
 * Invariants: caller-owned entries remain immutable and alive until index reset.
 * Boundary: this is source/artifact shard-index foundation, not a payload reader.
 */
#ifndef YVEX_SHARD_INDEX_PRIVATE_H
#define YVEX_SHARD_INDEX_PRIVATE_H

#include <stddef.h>

typedef enum {
    YVEX_SHARD_INDEX_OK = 0,
    YVEX_SHARD_INDEX_INVALID,
    YVEX_SHARD_INDEX_BUDGET,
    YVEX_SHARD_INDEX_DUPLICATE_ID,
    YVEX_SHARD_INDEX_DUPLICATE_KEY,
    YVEX_SHARD_INDEX_NONCANONICAL_ORDER
} yvex_shard_index_result;

typedef struct {
    unsigned long long canonical_id;
    const char *canonical_key;
} yvex_shard_index_entry;

typedef struct {
    const yvex_shard_index_entry *entries;
    unsigned long long count;
} yvex_shard_index;

yvex_shard_index_result yvex_shard_index_init(
    yvex_shard_index *index,
    const yvex_shard_index_entry *entries,
    unsigned long long count,
    unsigned long long maximum_entries);
const yvex_shard_index_entry *yvex_shard_index_at(
    const yvex_shard_index *index,
    unsigned long long canonical_id);
const yvex_shard_index_entry *yvex_shard_index_find(
    const yvex_shard_index *index,
    const char *canonical_key,
    unsigned long long *comparisons);
void yvex_shard_index_reset(yvex_shard_index *index);

#endif
