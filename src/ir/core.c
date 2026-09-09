/* Core control/value dialect. Neural operations extend static dialect contracts. */
#include "src/ir/private.h"

#include <string.h>

static yvex_ir_id core_operand_type(const yvex_ir_module *m,
                                   const yvex_ir_operation *op, uint32_t index)
{
    return m->values[op->operands[index]].type;
}

static yvex_ir_id core_result_type(const yvex_ir_module *m,
                                  const yvex_ir_operation *op, uint32_t index)
{
    return m->values[op->results[index]].type;
}

static int core_identity(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    if (core_operand_type(m, op, 0u) != core_result_type(m, op, 0u))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "identity cannot change a value type");
    return YVEX_OK;
}

static int core_parameter(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_attribute *source = yvex_ir_attribute_get(m, id, "source");
    const yvex_ir_type *type = &m->types[core_result_type(m, op, 0u)];
    if (type->kind != YVEX_IR_TENSOR || !source ||
        !yvex_sha256_hex_valid(source->value.text))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "parameter requires logical tensor and source identity");
    return YVEX_OK;
}

static int core_index(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_type *type = &m->types[core_result_type(m, &m->operations[id], 0u)];
    if (type->kind != YVEX_IR_SCALAR || type->scalar != YVEX_IR_INDEX)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "index literal requires index type");
    return YVEX_OK;
}

static int core_return(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_block *block = &m->blocks[op->block];
    const yvex_ir_function *function = &m->functions[block->function];
    uint32_t index;
    if (block->parent_operation != YVEX_IR_NONE || op->operand_count != function->result_count)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "return does not match function signature");
    for (index = 0u; index < op->operand_count; ++index)
        if (core_operand_type(m, op, index) != function->results[index])
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "return type mismatch");
    return YVEX_OK;
}

static int core_yield(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_operation *parent =
        yvex_ir_operation_at(m, m->blocks[op->block].parent_operation);
    uint32_t index;
    if (!parent || op->operand_count != parent->result_count)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "yield does not match region result signature");
    for (index = 0u; index < op->operand_count; ++index)
        if (core_operand_type(m, op, index) != core_result_type(m, parent, index))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "yield type mismatch");
    return YVEX_OK;
}

static int core_call(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_attribute *symbol = yvex_ir_attribute_get(m, id, "callee");
    const yvex_ir_function *function;
    const yvex_ir_block *body;
    yvex_ir_id target;
    uint32_t index;
    if (!symbol || !yvex_ir_function_find(m, symbol->value.text, &target))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "call target is unresolved");
    function = &m->functions[target];
    body = &m->blocks[function->body];
    if (op->operand_count != body->argument_count || op->result_count != function->result_count)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "call signature arity mismatch");
    for (index = 0u; index < op->operand_count; ++index)
        if (core_operand_type(m, op, index) != m->values[body->arguments[index]].type)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "call argument type mismatch");
    for (index = 0u; index < op->result_count; ++index)
        if (core_result_type(m, op, index) != function->results[index])
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "call result type mismatch");
    return YVEX_OK;
}

/* loop(count, carried...) has one region(index, carried...) -> carried... .
 * if(condition, arguments...) has two regions(arguments...) -> results... .
 * Loop trip count is explicit computational input, not scheduler work policy. */
static int core_regions(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    int loop = strcmp(op->definition->name, "core.loop") == 0;
    const yvex_ir_type *control = &m->types[core_operand_type(m, op, 0u)];
    uint32_t region, index;
    if (control->kind != YVEX_IR_SCALAR || control->scalar != (loop ? YVEX_IR_INDEX : YVEX_IR_BOOL) ||
        (loop && op->result_count + 1u != op->operand_count))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "region control or loop-carried arity is invalid");
    for (region = 0u; region < op->region_count; ++region) {
        const yvex_ir_block *body = &m->blocks[op->regions[region]];
        uint32_t offset = loop ? 0u : 1u;
        if (body->argument_count != op->operand_count - offset)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "region argument arity mismatch");
        for (index = 0u; index < body->argument_count; ++index)
            if (m->values[body->arguments[index]].type != core_operand_type(m, op, index + offset))
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "region argument type mismatch");
    }
    if (loop)
        for (index = 0u; index < op->result_count; ++index)
            if (core_result_type(m, op, index) != core_operand_type(m, op, index + 1u))
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "loop-carried type changes across iteration");
    return YVEX_OK;
}

static int core_state_read(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *state = &m->types[core_operand_type(m, op, 0u)];
    const yvex_ir_type *result = &m->types[core_result_type(m, op, 0u)];
    uint32_t index;
    if (state->kind != YVEX_IR_STATE || result->kind != YVEX_IR_TENSOR ||
        state->scalar != result->scalar || state->rank != result->rank)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state read requires matching logical tensor geometry");
    for (index = 0u; index < state->rank; ++index)
        if (!yvex_ir_extent_equal(state->shape[index], result->shape[index]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state read shape mismatch");
    return YVEX_OK;
}

static int core_state_update(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *state = &m->types[core_operand_type(m, op, 0u)];
    const yvex_ir_type *value = &m->types[core_operand_type(m, op, 1u)];
    uint32_t index;
    if (state->kind != YVEX_IR_STATE || value->kind != YVEX_IR_TENSOR ||
        core_result_type(m, op, 0u) != core_operand_type(m, op, 0u) ||
        state->scalar != value->scalar || state->rank != value->rank)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state update changes semantic domain or type");
    for (index = 0u; index < state->rank; ++index)
        if (!yvex_ir_extent_equal(state->shape[index], value->shape[index]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state update shape mismatch");
    return YVEX_OK;
}

const yvex_ir_dialect *yvex_ir_core_dialect(void)
{
    static const yvex_ir_attribute_rule parameter[] = {
        {"parameter", YVEX_IR_ATTR_SYMBOL, 1}, {"source", YVEX_IR_ATTR_TEXT, 1}};
    static const yvex_ir_attribute_rule callee[] = {{"callee", YVEX_IR_ATTR_SYMBOL, 1}};
    static const yvex_ir_attribute_rule integer[] = {{"value", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_operation_definition operations[] = {
        {"core.identity", 1u, 1u, 1u, 1u, 1u, 0u, YVEX_IR_PURE, NULL, 0u, 0, core_identity},
        {"core.parameter", 1u, 0u, 0u, 1u, 1u, 0u, YVEX_IR_PURE, parameter, 2u, 0, core_parameter},
        {"core.index", 1u, 0u, 0u, 1u, 1u, 0u, YVEX_IR_PURE, integer, 1u, 0, core_index},
        {"core.call", 1u, 0u, IR_OBJECT_LIMIT, 0u, IR_OBJECT_LIMIT,
         0u, 0u, callee, 1u, 0, core_call},
        {"core.return", 1u, 0u, IR_OBJECT_LIMIT, 0u, 0u, 0u, 0u, NULL, 0u, 1, core_return},
        {"core.yield", 1u, 0u, IR_OBJECT_LIMIT, 0u, 0u, 0u, 0u, NULL, 0u, 1, core_yield},
        {"core.loop", 1u, 1u, IR_OBJECT_LIMIT, 0u, IR_OBJECT_LIMIT, 1u, YVEX_IR_ORDERED, NULL, 0u, 0, core_regions},
        {"core.if", 1u, 1u, IR_OBJECT_LIMIT, 0u, IR_OBJECT_LIMIT, 2u, YVEX_IR_ORDERED, NULL, 0u, 0, core_regions},
        {"state.read", 1u, 1u, 1u, 1u, 1u, 0u, YVEX_IR_READ_STATE, NULL, 0u, 0, core_state_read},
        {"state.update", 1u, 2u, 2u, 1u, 1u, 0u,
         YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, NULL, 0u, 0, core_state_update}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
