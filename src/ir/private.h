/* The IR module alone owns mutable compiler storage; consumers use immutable views. */
#ifndef YVEX_IR_PRIVATE_H
#define YVEX_IR_PRIVATE_H

#include <yvex/internal/ir.h>
#include <stdatomic.h>

#define IR_OBJECT_LIMIT 1048576u
#define IR_EFFECT_MASK 31u
#define IR_STORAGE_LIMIT (256u * 1024u * 1024u)

struct yvex_ir_module {
    atomic_uint references;
    char name[YVEX_IR_NAME_CAP], source_identity[YVEX_SHA256_HEX_BYTES];
    char identity[YVEX_SHA256_HEX_BYTES];
    yvex_ir_dialect *dialects;
    size_t dialect_count;
    yvex_ir_dimension *dimensions;
    yvex_ir_type *types;
    yvex_ir_value *values;
    yvex_ir_operation *operations;
    yvex_ir_block *blocks;
    yvex_ir_function *functions;
    size_t dimension_count, type_count, value_count, operation_count;
    size_t block_count, function_count;
    size_t dimension_capacity, type_capacity, value_capacity, operation_capacity;
    size_t block_capacity, function_capacity;
    size_t storage_bytes;
    int sealed;
};

int yvex_ir_refuse(yvex_error *, yvex_status, const char *);
int yvex_ir_name_valid(const char *, int qualified);
int yvex_ir_type_valid(const yvex_ir_module *, const yvex_ir_type *);
const yvex_ir_operation_definition *yvex_ir_definition_find(
    const yvex_ir_module *, const char *);

#endif
