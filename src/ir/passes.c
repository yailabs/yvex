/* Deterministic verified rewrites, with transactional publication per pipeline. */
#include "src/ir/private.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    const yvex_ir_module *input;
    yvex_ir_module *output;
    yvex_ir_id *values, *types;
    const unsigned char *live;
    int aliases, inline_calls;
} ir_rewrite;

static int ir_rewrite_block(ir_rewrite *r, yvex_ir_id source, yvex_ir_id target,
                             unsigned int depth, int inlined, yvex_error *err);

static int ir_rewrite_call(ir_rewrite *r, const yvex_ir_operation *call,
                            yvex_ir_id target, unsigned int depth, yvex_error *err)
{
    const yvex_ir_attribute *callee = NULL;
    const yvex_ir_block *body;
    const yvex_ir_operation *returned;
    yvex_ir_id function;
    uint32_t index;
    int rc;
    for (index = 0u; index < call->attribute_count; ++index)
        if (!strcmp(call->attributes[index].name, "callee")) callee = &call->attributes[index];
    if (!callee || !yvex_ir_function_find(r->input, callee->value.text, &function))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "call legalization requires a resolved function");
    body = &r->input->blocks[r->input->functions[function].body];
    for (index = 0u; index < body->argument_count; ++index)
        r->values[body->arguments[index]] = r->values[call->operands[index]];
    rc = ir_rewrite_block(r, r->input->functions[function].body, target, depth + 1u, 1, err);
    if (rc != YVEX_OK) return rc;
    returned = &r->input->operations[body->last_operation];
    for (index = 0u; index < call->result_count; ++index)
        r->values[call->results[index]] = r->values[returned->operands[index]];
    return YVEX_OK;
}

static int ir_rewrite_block(ir_rewrite *r, yvex_ir_id source, yvex_ir_id target,
                             unsigned int depth, int inlined, yvex_error *err)
{
    const yvex_ir_block *block = &r->input->blocks[source];
    yvex_ir_id id;
    uint32_t index;
    if (depth > 64u)
        return yvex_ir_refuse(err, YVEX_ERR_BOUNDS, "call/region expansion exceeds the compiler nesting bound");
    if (!inlined)
        for (index = 0u; index < block->argument_count; ++index)
            r->values[block->arguments[index]] = r->output->blocks[target].arguments[index];
    for (id = block->first_operation; id != YVEX_IR_NONE; id = r->input->operations[id].next) {
        const yvex_ir_operation *op = &r->input->operations[id];
        yvex_ir_attribute attributes[256];
        yvex_ir_id *operands = NULL, *types = NULL, created;
        yvex_ir_operation_request request = {.operation = op->definition->name,
            .operand_count = op->operand_count, .result_count = op->result_count,
            .attributes = attributes, .attribute_count = op->attribute_count};
        int rc;
        if (r->live && !r->live[id]) continue;
        if (inlined && !strcmp(op->definition->name, "core.return")) continue;
        if (r->inline_calls && !strcmp(op->definition->name, "core.call")) {
            rc = ir_rewrite_call(r, op, target, depth, err);
            if (rc != YVEX_OK) return rc;
            continue;
        }
        if (r->aliases && strcmp(op->definition->name, "core.identity") == 0) {
            r->values[op->results[0]] = r->values[op->operands[0]];
            continue;
        }
        if (op->operand_count) operands = malloc(op->operand_count * sizeof(*operands));
        if (op->result_count) types = malloc(op->result_count * sizeof(*types));
        if ((op->operand_count && !operands) || (op->result_count && !types)) {
            free(operands);
            free(types);
            return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "pass operand allocation failed");
        }
        for (index = 0u; index < op->operand_count; ++index) operands[index] = r->values[op->operands[index]];
        for (index = 0u; index < op->result_count; ++index)
            types[index] = r->types[r->input->values[op->results[index]].type];
        for (index = 0u; index < op->attribute_count; ++index) {
            attributes[index] = op->attributes[index];
            if (attributes[index].kind == YVEX_IR_ATTR_TYPE)
                attributes[index].value.type = r->types[attributes[index].value.type];
        }
        request.operands = operands;
        request.result_types = types;
        rc = yvex_ir_operation_add(r->output, target, &request, &created, err);
        free(types);
        free(operands);
        if (rc != YVEX_OK) return rc;
        for (index = 0u; index < op->result_count; ++index)
            r->values[op->results[index]] = r->output->operations[created].results[index];
        for (index = 0u; index < op->region_count; ++index) {
            const yvex_ir_block *region = &r->input->blocks[op->regions[index]];
            yvex_ir_id *arguments = NULL, body;
            uint32_t arg;
            if (region->argument_count) arguments = malloc(region->argument_count * sizeof(*arguments));
            if (region->argument_count && !arguments)
                return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "pass region allocation failed");
            for (arg = 0u; arg < region->argument_count; ++arg)
                arguments[arg] = r->types[r->input->values[region->arguments[arg]].type];
            rc = yvex_ir_region_add(r->output, created, arguments, region->argument_count, &body, err);
            free(arguments);
            if (rc == YVEX_OK) rc = ir_rewrite_block(r, op->regions[index], body, depth + 1u, 0, err);
            if (rc != YVEX_OK) return rc;
        }
    }
    return YVEX_OK;
}

static int ir_rewrite_functions(ir_rewrite *r, yvex_error *err)
{
    size_t index;
    for (index = 0u; index < r->input->function_count; ++index) {
        const yvex_ir_function *f = &r->input->functions[index];
        const yvex_ir_block *block = &r->input->blocks[f->body];
        yvex_ir_id *arguments = NULL, *results = NULL, function;
        uint32_t arg;
        int rc;
        if (block->argument_count) arguments = malloc(block->argument_count * sizeof(*arguments));
        if (f->result_count) results = malloc(f->result_count * sizeof(*results));
        if ((block->argument_count && !arguments) || (f->result_count && !results)) {
            free(arguments);
            free(results);
            return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "pass signature allocation failed");
        }
        for (arg = 0u; arg < block->argument_count; ++arg)
            arguments[arg] = r->types[r->input->values[block->arguments[arg]].type];
        for (arg = 0u; arg < f->result_count; ++arg) results[arg] = r->types[f->results[arg]];
        rc = yvex_ir_function_add(r->output, f->symbol, arguments, block->argument_count,
                                  results, f->result_count, f->effects, &function, err);
        free(results);
        free(arguments);
        if (rc != YVEX_OK) return rc;
    }
    for (index = 0u; index < r->input->function_count; ++index) {
        int rc = ir_rewrite_block(r, r->input->functions[index].body,
                                  r->output->functions[index].body, 0u, 0, err);
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int ir_rewrite_module(const yvex_ir_module *m, const unsigned char *live,
                              int aliases, int inline_calls, yvex_ir_module **out, yvex_error *err)
{
    ir_rewrite rewrite = {.input = m, .live = live, .aliases = aliases, .inline_calls = inline_calls};
    size_t index;
    int rc = yvex_ir_verify(m, err);
    *out = NULL;
    if (rc != YVEX_OK) return rc;
    rc = yvex_ir_module_open(&rewrite.output, m->name, m->source_identity,
                              m->dialects, m->dialect_count, err);
    if (rc != YVEX_OK) return rc;
    rewrite.values = malloc((m->value_count ? m->value_count : 1u) * sizeof(*rewrite.values));
    rewrite.types = malloc((m->type_count ? m->type_count : 1u) * sizeof(*rewrite.types));
    if (!rewrite.values || !rewrite.types)
        rc = yvex_ir_refuse(err, YVEX_ERR_NOMEM, "pass mapping allocation failed");
    for (index = 0u; rc == YVEX_OK && index < m->value_count; ++index) rewrite.values[index] = YVEX_IR_NONE;
    for (index = 0u; rc == YVEX_OK && index < m->dimension_count; ++index) {
        yvex_ir_id id;
        rc = yvex_ir_dimension_add(rewrite.output, &m->dimensions[index], &id, err);
    }
    for (index = 0u; rc == YVEX_OK && index < m->type_count; ++index) {
        yvex_ir_type type = m->types[index];
        uint32_t member;
        for (member = 0u; member < type.member_count; ++member)
            type.members[member] = rewrite.types[type.members[member]];
        rc = yvex_ir_type_intern(rewrite.output, &type, &rewrite.types[index], err);
    }
    if (rc == YVEX_OK) rc = ir_rewrite_functions(&rewrite, err);
    if (rc == YVEX_OK) rc = yvex_ir_seal(rewrite.output, err);
    free(rewrite.types);
    free(rewrite.values);
    if (rc == YVEX_OK) *out = rewrite.output;
    else yvex_ir_module_close(&rewrite.output);
    return rc;
}

static int ir_canonicalize(const yvex_ir_module *m, yvex_ir_module **out, yvex_error *err)
{
    return ir_rewrite_module(m, NULL, 1, 0, out, err);
}

static int ir_inline_calls(const yvex_ir_module *m, yvex_ir_module **out, yvex_error *err)
{
    return ir_rewrite_module(m, NULL, 0, 1, out, err);
}

static void ir_mark(yvex_ir_id id, unsigned char *live, yvex_ir_id *stack, size_t *count)
{
    if (id != YVEX_IR_NONE && !live[id]) {
        live[id] = 1u;
        stack[(*count)++] = id;
    }
}

static int ir_dead_code(const yvex_ir_module *m, yvex_ir_module **out, yvex_error *err)
{
    unsigned char *live;
    yvex_ir_id *stack;
    size_t index, count = 0u;
    int rc = yvex_ir_verify(m, err);
    *out = NULL;
    if (rc != YVEX_OK) return rc;
    live = calloc(m->operation_count, 1u);
    stack = malloc(m->operation_count * sizeof(*stack));
    if (!live || !stack) {
        free(live);
        free(stack);
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "liveness allocation failed");
    }
    for (index = 0u; index < m->operation_count; ++index) {
        const yvex_ir_operation *op = &m->operations[index];
        uint32_t operand;
        if (op->definition->terminator || yvex_ir_operation_effects(m, (yvex_ir_id)index))
            ir_mark((yvex_ir_id)index, live, stack, &count);
        for (operand = 0u; operand < op->operand_count; ++operand)
            if (m->types[m->values[op->operands[operand]].type].kind == YVEX_IR_STATE)
                ir_mark((yvex_ir_id)index, live, stack, &count);
    }
    while (count) {
        const yvex_ir_operation *op = &m->operations[stack[--count]];
        uint32_t operand;
        ir_mark(m->blocks[op->block].parent_operation, live, stack, &count);
        for (operand = 0u; operand < op->operand_count; ++operand)
            ir_mark(m->values[op->operands[operand]].definition, live, stack, &count);
    }
    rc = ir_rewrite_module(m, live, 0, 0, out, err);
    free(stack);
    free(live);
    return rc;
}

const yvex_ir_pass *yvex_ir_canonical_pass(void)
{
    static const yvex_ir_pass pass = {"canonical.value_aliases", 1u, ir_canonicalize};
    return &pass;
}

const yvex_ir_pass *yvex_ir_dead_code_pass(void)
{
    static const yvex_ir_pass pass = {"canonical.dead_pure_values", 1u, ir_dead_code};
    return &pass;
}

const yvex_ir_pass *yvex_ir_inline_pass(void)
{
    static const yvex_ir_pass pass = {"legalize.direct_calls", 1u, ir_inline_calls};
    return &pass;
}

int yvex_ir_pass_pipeline(const yvex_ir_module *input, const yvex_ir_pass *passes,
                           size_t count, yvex_ir_module **out,
                           yvex_ir_pass_evidence *evidence, yvex_error *err)
{
    yvex_ir_module *current = NULL;
    const yvex_ir_module *previous = input;
    yvex_ir_pass_evidence *receipt;
    size_t index;
    int rc = YVEX_OK;
    if (out) *out = NULL;
    if (!input || !input->sealed || !out || !passes || !count || count > 64u)
        return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "pipeline requires sealed input and bounded ordered passes");
    receipt = calloc(count, sizeof(*receipt));
    if (!receipt) return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "pass receipt allocation failed");
    for (index = 0u; index < count; ++index) {
        yvex_ir_module *next = NULL;
        const yvex_ir_pass *pass = &passes[index];
        if (!yvex_ir_name_valid(pass->name, 1) || !pass->version || !pass->run) {
            rc = yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "invalid static pass contract");
            break;
        }
        rc = pass->run(previous, &next, err);
        if (next == previous || next == input) {
            rc = yvex_ir_refuse(err, YVEX_ERR_STATE, "pass must publish distinct immutable ownership");
            next = NULL;
        }
        if (rc == YVEX_OK) rc = yvex_ir_seal(next, err);
        if (rc != YVEX_OK) {
            yvex_ir_module_close(&next);
            break;
        }
        yvex_core_text_copy(receipt[index].pass, sizeof(receipt[index].pass), pass->name);
        receipt[index].version = pass->version;
        yvex_core_text_copy(receipt[index].input_identity, sizeof(receipt[index].input_identity), previous->identity);
        yvex_core_text_copy(receipt[index].output_identity, sizeof(receipt[index].output_identity), next->identity);
        receipt[index].input_operations = previous->operation_count;
        receipt[index].output_operations = next->operation_count;
        yvex_ir_module_close(&current);
        current = next;
        previous = current;
    }
    if (rc == YVEX_OK) {
        if (evidence) memcpy(evidence, receipt, count * sizeof(*receipt));
        *out = current;
    } else yvex_ir_module_close(&current);
    free(receipt);
    return rc;
}
