/*
 * yvex_transform_ir_internal.h - private Transformation IR owner state.
 *
 * Owner: src/model/compilation.
 * Owns: builder storage, sealed arrays, indexes, allocation accounting, and
 *   validation/identity coordination shared by compilation translation units.
 * Does not own: family construction policy, payload IO, physical lowering, or rendering.
 * Invariants: every allocation is owned by exactly one builder or sealed IR.
 * Boundary: private mutable state is never exposed to downstream consumers.
 */
#ifndef YVEX_TRANSFORM_IR_INTERNAL_H
#define YVEX_TRANSFORM_IR_INTERNAL_H

#include "yvex_transform_ir.h"

typedef struct {
    unsigned long long hash;
    unsigned long long value_plus_one;
} yvex_transform_index_slot;

typedef struct {
    yvex_transform_node node;
    unsigned long long provisional_id;
} yvex_transform_builder_node;

struct yvex_transform_builder {
    yvex_transform_ir_state state;
    yvex_transform_allocator allocator;
    yvex_transform_budget budget;
    yvex_transform_header header;
    char logical_model_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    char required_payload_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    char payload_trust_class[40];
    yvex_transform_source_value *sources;
    yvex_transform_value *values;
    yvex_transform_builder_node *nodes;
    unsigned long long *edges;
    unsigned long long source_count;
    unsigned long long source_capacity;
    unsigned long long value_count;
    unsigned long long value_capacity;
    unsigned long long node_count;
    unsigned long long node_capacity;
    unsigned long long edge_count;
    unsigned long long edge_capacity;
    unsigned long long terminal_count;
    size_t owned_bytes;
    size_t peak_bytes;
};

struct yvex_transform_ir {
    yvex_transform_allocator allocator;
    yvex_transform_source_value *sources;
    yvex_transform_value *values;
    yvex_transform_node *nodes;
    unsigned long long *edges;
    unsigned long long *topological_order;
    unsigned long long *terminal_values;
    yvex_transform_index_slot *source_index;
    yvex_transform_index_slot *terminal_index;
    unsigned long long source_index_capacity;
    unsigned long long terminal_index_capacity;
    yvex_transform_ir_summary summary;
};

void *yvex_transform_allocate_zero(yvex_transform_allocator *allocator,
                                   size_t size);
unsigned long long yvex_transform_hash_bytes(unsigned long long hash,
                                             const void *data,
                                             size_t length);
unsigned long long yvex_transform_hash_string(const char *text);
unsigned long long yvex_transform_hash_logical_key(
    const yvex_transform_logical_key *key);
unsigned long long yvex_transform_index_capacity(unsigned long long count);
int yvex_transform_index_insert(yvex_transform_index_slot *slots,
                                unsigned long long capacity,
                                unsigned long long hash,
                                unsigned long long value);
int yvex_transform_fail(yvex_transform_failure *failure,
                        yvex_transform_failure_code code,
                        unsigned long long value_id,
                        unsigned long long node_id,
                        unsigned long long source_index,
                        unsigned long long terminal_ordinal,
                        unsigned long long input_index,
                        unsigned long long expected,
                        unsigned long long actual,
                        unsigned int axis,
                        yvex_error *err,
                        const char *where);
int yvex_transform_ir_validate_and_seal(
    yvex_transform_builder *builder,
    yvex_transform_ir **out,
    yvex_transform_failure *failure,
    yvex_error *err);
int yvex_transform_ir_compute_identity(yvex_transform_ir *ir,
                                       yvex_transform_failure *failure,
                                       yvex_error *err);

#endif
