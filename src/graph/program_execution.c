/* Lower verified computational dependencies into canonical entry-local work. */
#include <yvex/internal/program.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

#define PROGRAM_EXECUTION_LIMIT 1048576u

struct yvex_program_execution {
    yvex_ir_module *module;
    yvex_program_entry *entries;
    size_t count;
    char identity[YVEX_SHA256_HEX_BYTES];
};

typedef struct {
    yvex_program_value *values;
    yvex_program_step *steps;
    yvex_ir_id *map, *marks;
    size_t used_values;
    yvex_ir_id last_effect;
} execution_builder;

static int execution_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.program.execution", reason);
    return status;
}

static int execution_entry_order(const void *a, const void *b)
{
    return strcmp(((const yvex_program_entry *)a)->symbol, ((const yvex_program_entry *)b)->symbol);
}

static int execution_id_order(const void *a, const void *b)
{
    yvex_ir_id left = *(const yvex_ir_id *)a, right = *(const yvex_ir_id *)b;
    return left < right ? -1 : left > right;
}

static yvex_ir_id execution_value(execution_builder *b, yvex_ir_id semantic, yvex_ir_id step)
{
    yvex_ir_id value = (yvex_ir_id)b->used_values++;
    b->map[semantic] = value;
    b->values[value] = (yvex_program_value){semantic, step, YVEX_IR_NONE};
    return value;
}

static int execution_step(execution_builder *b, const yvex_ir_module *module,
                            yvex_ir_id semantic, yvex_ir_id index, yvex_error *err)
{
    const yvex_ir_operation *op = yvex_ir_operation_at(module, semantic);
    yvex_program_step *step = &b->steps[index];
    yvex_ir_id *storage, *operands, *results, *dependencies;
    size_t item, dependencies_used = 0u;
    uint32_t effects = yvex_ir_operation_effects(module, semantic);
    size_t count = (size_t)op->operand_count * 2u + op->result_count + 1u;
    storage = malloc(count * sizeof(*storage));
    if (!storage) return execution_refuse(err, YVEX_ERR_NOMEM, "step dependency allocation failed");
    operands = storage;
    results = operands + op->operand_count;
    dependencies = results + op->result_count;
    *step = (yvex_program_step){.semantic_operation = semantic, .operands = operands, .results = results,
        .dependencies = dependencies, .operand_count = op->operand_count, .result_count = op->result_count};
    for (item = 0u; item < op->operand_count; ++item) {
        yvex_ir_id value = b->map[op->operands[item]], producer;
        if (value == YVEX_IR_NONE || value >= b->used_values)
            return execution_refuse(err, YVEX_ERR_FORMAT, "execution operand has no local definition");
        operands[item] = value;
        producer = b->values[value].definition;
        b->values[value].last_use = index;
        if (producer != YVEX_IR_NONE && b->marks[producer] != index) {
            b->marks[producer] = index;
            dependencies[dependencies_used++] = producer;
        }
    }
    if ((effects || op->definition->terminator) && b->last_effect != YVEX_IR_NONE &&
        b->marks[b->last_effect] != index)
        dependencies[dependencies_used++] = b->last_effect;
    if (effects) b->last_effect = index;
    qsort(dependencies, dependencies_used, sizeof(*dependencies), execution_id_order);
    step->dependency_count = dependencies_used;
    for (item = 0u; item < op->result_count; ++item)
        results[item] = execution_value(b, op->results[item], index);
    return YVEX_OK;
}

static int execution_entry_compile(yvex_program_entry *entry, const yvex_ir_module *module,
                                    yvex_ir_id function, yvex_error *err)
{
    const yvex_ir_function *f = yvex_ir_function_at(module, function);
    const yvex_ir_block *body = yvex_ir_block_at(module, f->body);
    execution_builder b = {.last_effect = YVEX_IR_NONE};
    yvex_ir_id id, index = 0u, maximum_value = 0u;
    size_t values = body->argument_count, steps = 0u, references = 0u, item;
    int rc = YVEX_OK;
    for (item = 0u; item < body->argument_count; ++item)
        if (body->arguments[item] > maximum_value) maximum_value = body->arguments[item];
    for (id = body->first_operation; id != YVEX_IR_NONE; id = yvex_ir_operation_at(module, id)->next) {
        const yvex_ir_operation *op = yvex_ir_operation_at(module, id);
        if (op->region_count || !strcmp(op->definition->name, "core.call"))
            return execution_refuse(err, YVEX_ERR_UNSUPPORTED, "calls/regions require explicit execution legalization");
        if (op->definition->terminator && strcmp(op->definition->name, "core.return"))
            return execution_refuse(err, YVEX_ERR_UNSUPPORTED, "entry requires an admitted return terminator");
        steps++;
        values += op->result_count;
        references += op->operand_count + op->result_count;
        if (steps > PROGRAM_EXECUTION_LIMIT || values > PROGRAM_EXECUTION_LIMIT ||
            references > PROGRAM_EXECUTION_LIMIT)
            return execution_refuse(err, YVEX_ERR_BOUNDS, "execution entry exceeds bounded work population");
        for (item = 0u; item < op->result_count; ++item)
            if (op->results[item] > maximum_value) maximum_value = op->results[item];
    }
    b.values = calloc(values ? values : 1u, sizeof(*b.values));
    b.steps = calloc(steps, sizeof(*b.steps));
    b.map = malloc(((size_t)maximum_value + 1u) * sizeof(*b.map));
    b.marks = malloc(steps * sizeof(*b.marks));
    *entry = (yvex_program_entry){.symbol = f->symbol, .values = b.values, .steps = b.steps,
        .input_count = body->argument_count, .value_count = values, .step_count = b.steps ? steps : 0u};
    if (!b.values || !b.steps || !b.map || !b.marks)
        rc = execution_refuse(err, YVEX_ERR_NOMEM, "execution entry allocation failed");
    if (rc == YVEX_OK) {
        for (item = 0u; item <= maximum_value; ++item) b.map[item] = YVEX_IR_NONE;
        for (item = 0u; item < steps; ++item) b.marks[item] = YVEX_IR_NONE;
        for (item = 0u; item < body->argument_count; ++item)
            (void)execution_value(&b, body->arguments[item], YVEX_IR_NONE);
        for (id = body->first_operation; id != YVEX_IR_NONE && rc == YVEX_OK;
             id = yvex_ir_operation_at(module, id)->next)
            rc = execution_step(&b, module, id, index++, err);
    }
    if (rc == YVEX_OK) {
        entry->results = b.steps[steps - 1u].operands;
        entry->result_count = b.steps[steps - 1u].operand_count;
    }
    free(b.marks);
    free(b.map);
    return rc;
}

static int execution_text(yvex_core_bytes *bytes, const char *format, ...)
{
    char text[512];
    va_list args;
    int count;
    va_start(args, format);
    count = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return count >= 0 && (size_t)count < sizeof(text) && yvex_core_bytes_append(bytes, text, (size_t)count);
}

static int execution_list(yvex_core_bytes *bytes, const char *prefix,
                           const yvex_ir_id *ids, size_t count)
{
    size_t item;
    for (item = 0u; item < count; ++item)
        if (!execution_text(bytes, "%s%s%u", item ? "," : "", prefix, ids[item])) return 0;
    return 1;
}

int yvex_program_execution_print(const yvex_program_execution *execution,
                                  yvex_core_bytes *out, yvex_error *err)
{
    yvex_core_bytes text = {.maximum = 64u * 1024u * 1024u, .initial_capacity = 4096u};
    size_t entry, value, step;
    int ok;
    if (!execution || !out)
        return execution_refuse(err, YVEX_ERR_INVALID_ARG, "sealed execution form and text output are required");
    ok = execution_text(&text, "execution.v1 semantic=%s\n", yvex_ir_identity(execution->module));
    for (entry = 0u; ok && entry < execution->count; ++entry) {
        const yvex_program_entry *e = &execution->entries[entry];
        ok = execution_text(&text, "entry @%s inputs=%zu {\n", e->symbol, e->input_count);
        for (step = 0u; ok && step < e->step_count; ++step) {
            const yvex_program_step *s = &e->steps[step];
            const yvex_ir_operation *op = yvex_ir_operation_at(execution->module, s->semantic_operation);
            ok = execution_text(&text, "  ^%zu ", step) &&
                 execution_list(&text, "%", s->results, s->result_count) &&
                 execution_text(&text, " = %s.v%u(", op->definition->name, op->definition->version) &&
                 execution_list(&text, "%", s->operands, s->operand_count) &&
                 execution_text(&text, ") after[") &&
                 execution_list(&text, "^", s->dependencies, s->dependency_count) &&
                 execution_text(&text, "]\n");
        }
        for (value = 0u; ok && value < e->value_count; ++value) {
            const yvex_program_value *v = &e->values[value];
            ok = execution_text(&text, "  %%%zu last=", value);
            if (ok) ok = v->last_use == YVEX_IR_NONE ? execution_text(&text, "unused\n") :
                          execution_text(&text, "^%u\n", v->last_use);
        }
        if (ok) ok = execution_text(&text, "}\n");
    }
    if (ok) ok = yvex_core_bytes_append(out, text.data, text.count);
    free(text.data);
    return ok ? YVEX_OK : execution_refuse(err, YVEX_ERR_NOMEM, "execution inspection encoding failed");
}

int yvex_program_execution_compile(yvex_program_execution **out,
                                    const yvex_ir_module *module, yvex_error *err)
{
    yvex_program_execution *execution;
    yvex_core_bytes text = {.maximum = 64u * 1024u * 1024u};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    size_t index;
    int rc;
    if (out) *out = NULL;
    if (!out || !yvex_ir_identity(module))
        return execution_refuse(err, YVEX_ERR_INVALID_ARG, "execution lowering requires a sealed semantic program");
    execution = calloc(1u, sizeof(*execution));
    if (!execution) return execution_refuse(err, YVEX_ERR_NOMEM, "execution ownership allocation failed");
    execution->module = yvex_ir_module_retain(module, err);
    execution->count = yvex_ir_function_count(module);
    execution->entries = calloc(execution->count, sizeof(*execution->entries));
    rc = execution->module && execution->entries ? YVEX_OK :
         execution_refuse(err, YVEX_ERR_NOMEM, "execution program ownership failed");
    for (index = 0u; rc == YVEX_OK && index < execution->count; ++index)
        rc = execution_entry_compile(&execution->entries[index], module, (yvex_ir_id)index, err);
    if (rc == YVEX_OK) {
        qsort(execution->entries, execution->count, sizeof(*execution->entries), execution_entry_order);
        rc = yvex_program_execution_print(execution, &text, err);
    }
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.program.execution.v1") ||
            !yvex_sha256_update(&hash, text.data, text.count) || !yvex_sha256_final(&hash, digest))
            rc = execution_refuse(err, YVEX_ERR_STATE, "execution identity sealing failed");
        else yvex_sha256_hex(digest, execution->identity);
    }
    free(text.data);
    if (rc == YVEX_OK) *out = execution;
    else yvex_program_execution_close(&execution);
    return rc;
}

void yvex_program_execution_close(yvex_program_execution **out)
{
    yvex_program_execution *execution = out ? *out : NULL;
    size_t entry, step;
    if (!execution) return;
    *out = NULL;
    for (entry = 0u; execution->entries && entry < execution->count; ++entry) {
        const yvex_program_entry *e = &execution->entries[entry];
        for (step = 0u; step < e->step_count; ++step) free((void *)e->steps[step].operands);
        free((void *)e->steps);
        free((void *)e->values);
    }
    free(execution->entries);
    yvex_ir_module_close(&execution->module);
    free(execution);
}

const yvex_ir_module *yvex_program_execution_module(const yvex_program_execution *execution)
{
    return execution ? execution->module : NULL;
}

const char *yvex_program_execution_identity(const yvex_program_execution *execution)
{
    return execution ? execution->identity : NULL;
}

size_t yvex_program_execution_entry_count(const yvex_program_execution *execution)
{
    return execution ? execution->count : 0u;
}

const yvex_program_entry *yvex_program_execution_entry_at(const yvex_program_execution *execution, size_t index)
{
    return execution && index < execution->count ? &execution->entries[index] : NULL;
}
