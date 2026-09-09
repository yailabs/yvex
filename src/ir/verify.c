/* Verify symbols, SSA dominance, declared effects and state-version consumption. */
#include "src/ir/private.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int ir_attribute_valid(const yvex_ir_module *m, const yvex_ir_attribute *a)
{
    if (!yvex_ir_name_valid(a->name, 0)) return 0;
    switch (a->kind) {
    case YVEX_IR_ATTR_U64: return 1;
    case YVEX_IR_ATTR_F64: return isfinite(a->value.real);
    case YVEX_IR_ATTR_BOOL: return a->value.integer <= 1u;
    case YVEX_IR_ATTR_TYPE: return a->value.type < m->type_count;
    case YVEX_IR_ATTR_SYMBOL: return yvex_ir_name_valid(a->value.text, 0);
    case YVEX_IR_ATTR_TEXT:
        return strnlen(a->value.text, sizeof(a->value.text)) < sizeof(a->value.text);
    default: return 0;
    }
}

static int ir_attributes_verify(const yvex_ir_module *m, const yvex_ir_operation *op)
{
    uint32_t i, j;
    size_t rule;
    for (i = 0u; i < op->attribute_count; ++i) {
        const yvex_ir_attribute *a = &op->attributes[i];
        if (!ir_attribute_valid(m, a)) return 0;
        for (j = 0u; j < i; ++j)
            if (strcmp(a->name, op->attributes[j].name) == 0) return 0;
        for (rule = 0u; rule < op->definition->attribute_count; ++rule)
            if (strcmp(a->name, op->definition->attributes[rule].name) == 0) break;
        if (rule == op->definition->attribute_count ||
            a->kind != op->definition->attributes[rule].kind) return 0;
    }
    for (rule = 0u; rule < op->definition->attribute_count; ++rule) {
        if (!op->definition->attributes[rule].required) continue;
        for (i = 0u; i < op->attribute_count; ++i)
            if (strcmp(op->attributes[i].name, op->definition->attributes[rule].name) == 0)
                break;
        if (i == op->attribute_count) return 0;
    }
    return 1;
}

uint32_t yvex_ir_operation_effects(const yvex_ir_module *m, yvex_ir_id id)
{
    const yvex_ir_operation *op = yvex_ir_operation_at(m, id);
    const yvex_ir_attribute *callee;
    yvex_ir_id function;
    if (!op) return IR_EFFECT_MASK;
    if (strcmp(op->definition->name, "core.call") != 0) return op->definition->effects;
    callee = yvex_ir_attribute_get(m, id, "callee");
    if (!callee || callee->kind != YVEX_IR_ATTR_SYMBOL ||
        !yvex_ir_function_find(m, callee->value.text, &function)) return IR_EFFECT_MASK;
    return m->functions[function].effects;
}

static int ir_visible(const yvex_ir_module *m, yvex_ir_id value, yvex_ir_id use)
{
    const yvex_ir_value *v = yvex_ir_value_at(m, value);
    const yvex_ir_operation *op = yvex_ir_operation_at(m, use);
    unsigned int depth = 0u;
    if (!v || !op) return 0;
    for (;;) {
        const yvex_ir_block *block = yvex_ir_block_at(m, op->block);
        if (!block || ++depth > 64u) return 0;
        if (op->block == v->block)
            return v->definition == YVEX_IR_NONE || v->definition < use;
        /* State enters regions through explicit block arguments, never capture. */
        if (m->types[v->type].kind == YVEX_IR_STATE ||
            block->parent_operation == YVEX_IR_NONE) return 0;
        use = block->parent_operation;
        op = yvex_ir_operation_at(m, use);
        if (!op) return 0;
    }
}

static int ir_state_verify(const yvex_ir_module *m, yvex_ir_id id,
                           unsigned char *consumed, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    uint32_t index, other, effects = yvex_ir_operation_effects(m, id);
    int transfer = op->definition->terminator || op->definition->regions ||
                   strcmp(op->definition->name, "core.call") == 0;
    for (index = 0u; index < op->operand_count; ++index) {
        yvex_ir_id value = op->operands[index];
        const yvex_ir_type *type = &m->types[m->values[value].type];
        if (type->kind != YVEX_IR_STATE) continue;
        if (consumed[value] || (!transfer && !(effects & YVEX_IR_READ_STATE)))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state version is stale or read effect is undeclared");
        if (!(effects & YVEX_IR_WRITE_STATE) && !transfer) continue;
        for (other = 0u; other < index; ++other)
            if (op->operands[other] == value)
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state version cannot be consumed twice");
    }
    for (index = 0u; index < op->result_count; ++index) {
        const yvex_ir_type *type = &m->types[m->values[op->results[index]].type];
        if (type->kind == YVEX_IR_STATE && !transfer && !(effects & YVEX_IR_WRITE_STATE))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "state result requires a declared write effect");
    }
    if ((effects & YVEX_IR_WRITE_STATE) || transfer)
        for (index = 0u; index < op->operand_count; ++index) {
            yvex_ir_id value = op->operands[index];
            if (m->types[m->values[value].type].kind == YVEX_IR_STATE) consumed[value] = 1u;
        }
    return YVEX_OK;
}

static int ir_block_verify(const yvex_ir_module *m, yvex_ir_id block_id,
                           unsigned int depth, unsigned char *visited,
                           unsigned char *consumed, yvex_error *err)
{
    const yvex_ir_block *block = yvex_ir_block_at(m, block_id);
    yvex_ir_id id, last = YVEX_IR_NONE;
    uint32_t index;
    if (!block || depth > 64u || block->function >= m->function_count ||
        block->first_operation == YVEX_IR_NONE)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "block is missing a body or exceeds nesting limit");
    for (index = 0u; index < block->argument_count; ++index) {
        const yvex_ir_value *arg = yvex_ir_value_at(m, block->arguments[index]);
        if (!arg || arg->type >= m->type_count || arg->block != block_id ||
            arg->definition != YVEX_IR_NONE || arg->ordinal != index)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "block argument has invalid ownership");
    }
    for (id = block->first_operation; id != YVEX_IR_NONE; id = m->operations[id].next) {
        const yvex_ir_operation *op = yvex_ir_operation_at(m, id);
        int rc;
        if (!op || visited[id] || op->block != block_id ||
            op->region_count != op->definition->regions ||
            !ir_attributes_verify(m, op) ||
            (yvex_ir_operation_effects(m, id) & ~m->functions[block->function].effects))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operation ownership, attributes or effects are invalid");
        visited[id] = 1u;
        for (index = 0u; index < op->operand_count; ++index)
            if (!ir_visible(m, op->operands[index], id))
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operand definition does not dominate this use");
        for (index = 0u; index < op->result_count; ++index) {
            const yvex_ir_value *result = yvex_ir_value_at(m, op->results[index]);
            if (!result || result->type >= m->type_count || result->block != block_id ||
                result->definition != id || result->ordinal != index)
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operation result has multiple or invalid definitions");
        }
        rc = op->definition->verify(m, id, err);
        if (rc == YVEX_OK) rc = ir_state_verify(m, id, consumed, err);
        if (rc != YVEX_OK) return rc;
        for (index = 0u; index < op->region_count; ++index) {
            const yvex_ir_block *region = yvex_ir_block_at(m, op->regions[index]);
            if (!region || region->parent_operation != id || region->function != block->function)
                return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "region has invalid parent ownership");
            rc = ir_block_verify(m, op->regions[index], depth + 1u, visited, consumed, err);
            if (rc != YVEX_OK) return rc;
        }
        if (op->definition->terminator && op->next != YVEX_IR_NONE)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "block terminator must be last");
        last = id;
    }
    if (last != block->last_operation || !m->operations[last].definition->terminator)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "block has no unique final terminator");
    return YVEX_OK;
}

static int ir_call_visit(const yvex_ir_module *, yvex_ir_id, unsigned char *, unsigned int);

static int ir_block_calls(const yvex_ir_module *m, yvex_ir_id block,
                           unsigned char *colors, unsigned int depth)
{
    yvex_ir_id id;
    if (depth > 64u) return 0;
    for (id = m->blocks[block].first_operation; id != YVEX_IR_NONE; id = m->operations[id].next) {
        const yvex_ir_operation *op = &m->operations[id];
        const yvex_ir_attribute *callee;
        yvex_ir_id target;
        uint32_t index;
        if (strcmp(op->definition->name, "core.call") == 0) {
            callee = yvex_ir_attribute_get(m, id, "callee");
            if (!callee || !yvex_ir_function_find(m, callee->value.text, &target) ||
                !ir_call_visit(m, target, colors, depth + 1u)) return 0;
        }
        for (index = 0u; index < op->region_count; ++index)
            if (!ir_block_calls(m, op->regions[index], colors, depth + 1u)) return 0;
    }
    return 1;
}

static int ir_call_visit(const yvex_ir_module *m, yvex_ir_id function,
                         unsigned char *colors, unsigned int depth)
{
    if (depth > 64u || colors[function] == 1u) return 0;
    if (colors[function] == 2u) return 1;
    colors[function] = 1u;
    if (!ir_block_calls(m, m->functions[function].body, colors, depth)) return 0;
    colors[function] = 2u;
    return 1;
}

typedef struct {
    const char *source, *symbol;
    yvex_ir_id type;
} ir_parameter;

static int ir_parameter_compare(const void *left, const void *right)
{
    const ir_parameter *a = left, *b = right;
    int order = strcmp(a->source, b->source);
    return order ? order : strcmp(a->symbol, b->symbol);
}

static int ir_parameters_verify(const yvex_ir_module *m, yvex_error *err)
{
    ir_parameter *parameters;
    size_t index, count = 0u;
    int rc = YVEX_OK;
    for (index = 0u; index < m->operation_count; ++index)
        if (strcmp(m->operations[index].definition->name, "core.parameter") == 0) count++;
    if (!count) return YVEX_OK;
    parameters = malloc(count * sizeof(*parameters));
    if (!parameters) return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "parameter symbol verification allocation failed");
    count = 0u;
    for (index = 0u; index < m->operation_count; ++index) {
        const yvex_ir_operation *op = &m->operations[index];
        if (strcmp(op->definition->name, "core.parameter") != 0) continue;
        parameters[count++] = (ir_parameter){
            yvex_ir_attribute_get(m, (yvex_ir_id)index, "source")->value.text,
            yvex_ir_attribute_get(m, (yvex_ir_id)index, "parameter")->value.text,
            m->values[op->results[0]].type};
    }
    qsort(parameters, count, sizeof(*parameters), ir_parameter_compare);
    for (index = 1u; index < count; ++index)
        if (!ir_parameter_compare(&parameters[index - 1u], &parameters[index]) &&
            parameters[index - 1u].type != parameters[index].type) {
            rc = yvex_ir_refuse(err, YVEX_ERR_FORMAT, "one source parameter cannot declare incompatible logical types");
            break;
        }
    free(parameters);
    return rc;
}

int yvex_ir_verify(const yvex_ir_module *m, yvex_error *err)
{
    unsigned char *visited = NULL, *consumed = NULL, *colors = NULL;
    size_t index;
    int rc = YVEX_OK;
    if (!m || !m->function_count || !m->operation_count)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "module must contain computational entrypoints");
    for (index = 0u; index < m->type_count; ++index)
        if (!yvex_ir_type_valid(m, &m->types[index]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "module contains invalid type geometry");
    visited = calloc(m->operation_count, 1u);
    consumed = calloc(m->value_count ? m->value_count : 1u, 1u);
    colors = calloc(m->function_count, 1u);
    if (!visited || !consumed || !colors)
        rc = yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR verification allocation failed");
    for (index = 0u; rc == YVEX_OK && index < m->function_count; ++index) {
        const yvex_ir_function *function = &m->functions[index];
        const yvex_ir_block *body = yvex_ir_block_at(m, function->body);
        if (!body || body->function != index || body->parent_operation != YVEX_IR_NONE)
            rc = yvex_ir_refuse(err, YVEX_ERR_FORMAT, "function body ownership is invalid");
        else rc = ir_block_verify(m, function->body, 0u, visited, consumed, err);
    }
    for (index = 0u; rc == YVEX_OK && index < m->operation_count; ++index)
        if (!visited[index]) rc = yvex_ir_refuse(err, YVEX_ERR_FORMAT, "orphan operation");
    if (rc == YVEX_OK) rc = ir_parameters_verify(m, err);
    for (index = 0u; rc == YVEX_OK && index < m->function_count; ++index)
        if (!ir_call_visit(m, (yvex_ir_id)index, colors, 0u))
            rc = yvex_ir_refuse(err, YVEX_ERR_FORMAT, "recursive calls require explicit bounded regions");
    free(colors);
    free(consumed);
    free(visited);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}
