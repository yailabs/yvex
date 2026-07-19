/* Owner: src/core.
 * Owns: linear canonical admission and deterministic logarithmic key lookup.
 * Does not own: entry allocation, key lifetime, files, payloads, or rendering.
 * Invariants: canonical IDs equal iteration positions and keys increase strictly.
 * Boundary: admitted keys establish indexing only, never shard or artifact trust.
 * Purpose: admit immutable shard-key arrays and provide deterministic indexed lookup.
 * Inputs: borrowed sorted entries, explicit count/budget, and optional lookup counters.
 * Effects: binds only the caller-owned index view and updates explicit counters.
 * Failure: invalid, duplicate, unordered, or over-budget entries leave the index empty. */
#include <yvex/internal/core.h>

#include <string.h>

/* Borrows a canonical entry array after one O(n) uniqueness/budget pass. */
/* Purpose: Construct the owned shard index init state (`yvex_shard_index_init`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
yvex_shard_index_result yvex_shard_index_init(
    yvex_shard_index *index,
    const yvex_shard_index_entry *entries,
    unsigned long long count,
    unsigned long long maximum_entries)
{
    unsigned long long ordinal;

    if (index) memset(index, 0, sizeof(*index));
    if (!index || !entries || count == 0u || maximum_entries == 0u)
        return YVEX_SHARD_INDEX_INVALID;
    if (count > maximum_entries) return YVEX_SHARD_INDEX_BUDGET;
    for (ordinal = 0u; ordinal < count; ++ordinal) {
        if (!entries[ordinal].canonical_key ||
            !entries[ordinal].canonical_key[0])
            return YVEX_SHARD_INDEX_INVALID;
        if (entries[ordinal].canonical_id != ordinal)
            return YVEX_SHARD_INDEX_DUPLICATE_ID;
        if (ordinal != 0u) {
            int order = strcmp(entries[ordinal - 1u].canonical_key,
                               entries[ordinal].canonical_key);
            if (order == 0) return YVEX_SHARD_INDEX_DUPLICATE_KEY;
            if (order > 0) return YVEX_SHARD_INDEX_NONCANONICAL_ORDER;
        }
    }
    index->entries = entries;
    index->count = count;
    return YVEX_SHARD_INDEX_OK;
}

/* Projects one canonical ID in O(1) without allocation or mutation. */
/* Purpose: Compute shard index at for its core invariant (`yvex_shard_index_at`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const yvex_shard_index_entry *yvex_shard_index_at(
    const yvex_shard_index *index,
    unsigned long long canonical_id)
{
    return index && index->entries && canonical_id < index->count
               ? &index->entries[canonical_id] : NULL;
}

/* Binary-searches one borrowed key and optionally returns comparison cost. */
/* Purpose: Compute shard index find for its core invariant (`yvex_shard_index_find`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
const yvex_shard_index_entry *yvex_shard_index_find(
    const yvex_shard_index *index,
    const char *canonical_key,
    unsigned long long *comparisons)
{
    unsigned long long lower = 0u;
    unsigned long long upper;

    if (comparisons) *comparisons = 0u;
    if (!index || !index->entries || !canonical_key) return NULL;
    upper = index->count;
    while (lower < upper) {
        unsigned long long middle = lower + (upper - lower) / 2u;
        int order;

        if (comparisons) (*comparisons)++;
        order = strcmp(index->entries[middle].canonical_key, canonical_key);
        if (order == 0) return &index->entries[middle];
        if (order < 0) lower = middle + 1u;
        else upper = middle;
    }
    return NULL;
}

/* Forgets borrowed storage; the caller remains responsible for its lifetime. */
/* Purpose: Release or reset owned shard index reset state (`yvex_shard_index_reset`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_shard_index_reset(yvex_shard_index *index)
{
    if (index) memset(index, 0, sizeof(*index));
}
