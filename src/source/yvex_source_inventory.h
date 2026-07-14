/*
 * yvex_source_inventory.h - source shard and header inventory owner.
 *
 * Owner: src/source.
 * Owns: upstream index parsing, indexless snapshot matching, shard geometry,
 *   one-pass safetensors header verification, and deterministic derived rows.
 * Does not own: source downloads, manifest publication, payload reads, or rendering.
 * Invariants: every shard and tensor assignment is unique and fail-closed.
 * Boundary: header inventory is not tensor payload trust or role mapping.
 */
#ifndef YVEX_SOURCE_INVENTORY_H
#define YVEX_SOURCE_INVENTORY_H

#include "yvex_source_verify.h"

#include <yvex/native_weights.h>

typedef struct {
    char *tensor;
    char *shard;
} yvex_source_inventory_row;

typedef struct {
    yvex_source_inventory_row *rows;
    size_t count;
} yvex_source_derived_inventory;

typedef struct yvex_source_tensor_snapshot yvex_source_tensor_snapshot;

typedef struct {
    unsigned long long canonical_id;
    const char *canonical_name;
    unsigned long long file_bytes;
    unsigned long long data_region_offset;
    unsigned long long payload_bytes;
} yvex_source_shard_snapshot;

typedef struct {
    unsigned long long tensor_count;
    unsigned long long shard_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long lookup_count;
    unsigned long long collision_count;
    unsigned long long maximum_probe;
    unsigned long long identity;
} yvex_source_tensor_snapshot_facts;

void yvex_source_derived_inventory_free(
    yvex_source_derived_inventory *inventory);
int yvex_source_inventory_verify(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_source_derived_inventory *derived,
    yvex_source_tensor_snapshot **snapshot,
    yvex_error *err);

void yvex_source_tensor_snapshot_retain(yvex_source_tensor_snapshot *snapshot);
void yvex_source_tensor_snapshot_release(yvex_source_tensor_snapshot *snapshot);
const yvex_native_weight_info *yvex_source_tensor_snapshot_at(
    const yvex_source_tensor_snapshot *snapshot,
    unsigned long long index);
const yvex_native_weight_info *yvex_source_tensor_snapshot_find(
    const yvex_source_tensor_snapshot *snapshot,
    const char *name);
int yvex_source_tensor_snapshot_find_index(
    const yvex_source_tensor_snapshot *snapshot,
    const char *name,
    unsigned long long *index);
int yvex_source_tensor_snapshot_facts_get(
    const yvex_source_tensor_snapshot *snapshot,
    yvex_source_tensor_snapshot_facts *out,
    yvex_error *err);
int yvex_source_tensor_snapshot_take_table(
    yvex_source_tensor_snapshot **out,
    yvex_native_weight_table **table,
    unsigned long long shard_count,
    unsigned long long header_scan_count,
    yvex_error *err);
int yvex_source_tensor_snapshot_take_table_with_shards(
    yvex_source_tensor_snapshot **out,
    yvex_native_weight_table **table,
    const yvex_source_shard_snapshot *shards,
    unsigned long long shard_count,
    unsigned long long header_scan_count,
    yvex_error *err);
const yvex_source_shard_snapshot *yvex_source_tensor_snapshot_shard_at(
    const yvex_source_tensor_snapshot *snapshot,
    unsigned long long index);
const yvex_source_shard_snapshot *yvex_source_tensor_snapshot_shard_find(
    const yvex_source_tensor_snapshot *snapshot,
    const char *canonical_name);
int yvex_source_tensor_snapshot_has_shard_catalog(
    const yvex_source_tensor_snapshot *snapshot);

#endif
