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

typedef struct {
    char *tensor;
    char *shard;
} yvex_source_inventory_row;

typedef struct {
    yvex_source_inventory_row *rows;
    size_t count;
} yvex_source_derived_inventory;

void yvex_source_derived_inventory_free(
    yvex_source_derived_inventory *inventory);
int yvex_source_inventory_verify(
    const yvex_source_verify_options *options,
    yvex_source_verification *out,
    yvex_source_derived_inventory *derived,
    yvex_error *err);

#endif
