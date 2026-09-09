/* IR binary v1: explicit fields, bounded import through constructors + verifier. */
#include "src/ir/private.h"

#include <stdlib.h>
#include <string.h>

#define IR_BINARY_LIMIT (64u * 1024u * 1024u)

typedef struct {
    const unsigned char *data;
    size_t size, offset;
    yvex_ir_id *values;
    size_t value_count;
    yvex_ir_module *module;
} ir_reader;

static int ir_put(yvex_core_bytes *bytes, uint64_t value)
{
    unsigned char encoded[8];
    unsigned int index;
    for (index = 0u; index < 8u; ++index) encoded[index] = (unsigned char)(value >> (8u * index));
    return yvex_core_bytes_append(bytes, encoded, sizeof(encoded));
}

static int ir_put_text(yvex_core_bytes *bytes, const char *text)
{
    size_t length = strlen(text);
    return ir_put(bytes, length) && yvex_core_bytes_append(bytes, text, length);
}

static int ir_get(ir_reader *r, uint64_t *value)
{
    unsigned int index;
    if (r->offset > r->size || r->size - r->offset < 8u) return 0;
    *value = 0u;
    for (index = 0u; index < 8u; ++index) *value |= (uint64_t)r->data[r->offset + index] << (8u * index);
    r->offset += 8u;
    return 1;
}

static int ir_get_count(ir_reader *r, uint32_t *value, uint32_t maximum)
{
    uint64_t word;
    if (!ir_get(r, &word) || word > maximum) return 0;
    *value = (uint32_t)word;
    return 1;
}

static int ir_get_text(ir_reader *r, char *text, size_t capacity)
{
    uint64_t length;
    if (!ir_get(r, &length) || length >= capacity || length > r->size - r->offset ||
        memchr(r->data + r->offset, 0, (size_t)length)) return 0;
    memcpy(text, r->data + r->offset, (size_t)length);
    text[length] = '\0';
    r->offset += (size_t)length;
    return 1;
}

static int ir_type_write(yvex_core_bytes *bytes, const yvex_ir_type *type)
{
    uint32_t index;
    if (!ir_put(bytes, type->kind) || !ir_put(bytes, type->scalar) ||
        !ir_put(bytes, type->rank) || !ir_put_text(bytes, type->domain) ||
        !ir_put(bytes, type->member_count)) return 0;
    for (index = 0u; index < type->rank; ++index)
        if (!ir_put(bytes, type->shape[index].symbol) || !ir_put(bytes, type->shape[index].extent)) return 0;
    for (index = 0u; index < type->member_count; ++index)
        if (!ir_put(bytes, type->members[index])) return 0;
    return 1;
}

static int ir_type_read(ir_reader *r, yvex_ir_type *type)
{
    uint32_t kind, scalar, index;
    memset(type, 0, sizeof(*type));
    if (!ir_get_count(r, &kind, YVEX_IR_PROGRAM) || !ir_get_count(r, &scalar, YVEX_IR_F64) ||
        !ir_get_count(r, &type->rank, YVEX_IR_RANK_CAP) ||
        !ir_get_text(r, type->domain, sizeof(type->domain)) ||
        !ir_get_count(r, &type->member_count, YVEX_IR_AGGREGATE_CAP)) return 0;
    type->kind = (yvex_ir_type_kind)kind;
    type->scalar = (yvex_ir_scalar)scalar;
    for (index = 0u; index < type->rank; ++index)
        if (!ir_get_count(r, &type->shape[index].symbol, UINT32_MAX) ||
            !ir_get(r, &type->shape[index].extent)) return 0;
    for (index = 0u; index < type->member_count; ++index)
        if (!ir_get_count(r, &type->members[index], IR_OBJECT_LIMIT)) return 0;
    return 1;
}

static int ir_attribute_write(yvex_core_bytes *bytes, const yvex_ir_attribute *a)
{
    uint64_t value;
    if (!ir_put_text(bytes, a->name) || !ir_put(bytes, a->kind)) return 0;
    if (a->kind == YVEX_IR_ATTR_TEXT || a->kind == YVEX_IR_ATTR_SYMBOL)
        return ir_put_text(bytes, a->value.text);
    if (a->kind == YVEX_IR_ATTR_F64) memcpy(&value, &a->value.real, sizeof(value));
    else value = a->kind == YVEX_IR_ATTR_TYPE ? a->value.type : a->value.integer;
    return ir_put(bytes, value);
}

static int ir_attribute_read(ir_reader *r, yvex_ir_attribute *a)
{
    uint32_t kind;
    uint64_t value;
    memset(a, 0, sizeof(*a));
    if (!ir_get_text(r, a->name, sizeof(a->name)) ||
        !ir_get_count(r, &kind, YVEX_IR_ATTR_SYMBOL)) return 0;
    a->kind = (yvex_ir_attribute_kind)kind;
    if (a->kind == YVEX_IR_ATTR_TEXT || a->kind == YVEX_IR_ATTR_SYMBOL)
        return ir_get_text(r, a->value.text, sizeof(a->value.text));
    if (!ir_get(r, &value)) return 0;
    if (a->kind == YVEX_IR_ATTR_F64) memcpy(&a->value.real, &value, sizeof(value));
    else if (a->kind == YVEX_IR_ATTR_TYPE) {
        if (value >= r->module->type_count) return 0;
        a->value.type = (yvex_ir_id)value;
    } else a->value.integer = value;
    return 1;
}

static int ir_block_write(const yvex_ir_module *m, yvex_ir_id block_id, yvex_core_bytes *bytes)
{
    const yvex_ir_block *block = &m->blocks[block_id];
    yvex_ir_id id;
    uint32_t index, count = 0u;
    if (!ir_put(bytes, block->argument_count)) return 0;
    for (index = 0u; index < block->argument_count; ++index)
        if (!ir_put(bytes, block->arguments[index])) return 0;
    for (id = block->first_operation; id != YVEX_IR_NONE; id = m->operations[id].next) count++;
    if (!ir_put(bytes, count)) return 0;
    for (id = block->first_operation; id != YVEX_IR_NONE; id = m->operations[id].next) {
        const yvex_ir_operation *op = &m->operations[id];
        if (!ir_put_text(bytes, op->definition->name) || !ir_put(bytes, op->definition->version) ||
            !ir_put(bytes, op->operand_count) || !ir_put(bytes, op->result_count) ||
            !ir_put(bytes, op->attribute_count) || !ir_put(bytes, op->region_count)) return 0;
        for (index = 0u; index < op->operand_count; ++index)
            if (!ir_put(bytes, op->operands[index])) return 0;
        for (index = 0u; index < op->result_count; ++index)
            if (!ir_put(bytes, op->results[index]) || !ir_put(bytes, m->values[op->results[index]].type)) return 0;
        for (index = 0u; index < op->attribute_count; ++index)
            if (!ir_attribute_write(bytes, &op->attributes[index])) return 0;
        for (index = 0u; index < op->region_count; ++index) {
            const yvex_ir_block *region = &m->blocks[op->regions[index]];
            uint32_t argument;
            if (!ir_put(bytes, region->argument_count)) return 0;
            for (argument = 0u; argument < region->argument_count; ++argument)
                if (!ir_put(bytes, m->values[region->arguments[argument]].type)) return 0;
            if (!ir_block_write(m, op->regions[index], bytes)) return 0;
        }
    }
    return 1;
}

static int ir_value_bind(ir_reader *r, uint32_t old, yvex_ir_id value)
{
    if (old >= r->value_count || r->values[old] != YVEX_IR_NONE) return 0;
    r->values[old] = value;
    return 1;
}

static yvex_ir_id *ir_ids_read(ir_reader *r, uint32_t count, int operands)
{
    yvex_ir_id *ids;
    uint32_t index, maximum = operands ? (uint32_t)r->value_count : (uint32_t)r->module->type_count;
    if (count > (r->size - r->offset) / 8u) return NULL;
    ids = malloc((count ? count : 1u) * sizeof(*ids));
    if (!ids) return NULL;
    for (index = 0u; index < count; ++index) {
        uint32_t value;
        if (!ir_get_count(r, &value, maximum) || value >= maximum ||
            (operands && r->values[value] == YVEX_IR_NONE)) {
            free(ids);
            return NULL;
        }
        ids[index] = operands ? r->values[value] : value;
    }
    return ids;
}

static int ir_block_read(ir_reader *, yvex_ir_id, unsigned int, yvex_error *);

static int ir_operation_read(ir_reader *r, yvex_ir_id block, unsigned int depth, yvex_error *err)
{
    char name[YVEX_IR_NAME_CAP];
    uint32_t version, input_count, result_count, attribute_count, region_count, index;
    yvex_ir_id *inputs = NULL, *types = NULL, *old = NULL, created;
    yvex_ir_attribute attributes[256];
    yvex_ir_operation_request request = {0};
    const yvex_ir_operation_definition *definition;
    int ok = 0;
    if (!ir_get_text(r, name, sizeof(name)) || !ir_get_count(r, &version, UINT32_MAX) ||
        !(definition = yvex_ir_definition_find(r->module, name)) || definition->version != version ||
        !ir_get_count(r, &input_count, definition->maximum_operands) ||
        !ir_get_count(r, &result_count, definition->maximum_results) ||
        !ir_get_count(r, &attribute_count, (uint32_t)definition->attribute_count) ||
        !ir_get_count(r, &region_count, definition->regions) || region_count != definition->regions ||
        !(inputs = ir_ids_read(r, input_count, 1))) goto done;
    if (result_count > (r->size - r->offset) / 16u) goto done;
    types = malloc((result_count ? result_count : 1u) * sizeof(*types));
    old = malloc((result_count ? result_count : 1u) * sizeof(*old));
    if (!types || !old) goto done;
    for (index = 0u; index < result_count; ++index)
        if (!ir_get_count(r, &old[index], (uint32_t)r->value_count) ||
            !ir_get_count(r, &types[index], (uint32_t)r->module->type_count)) goto done;
    for (index = 0u; index < attribute_count; ++index)
        if (!ir_attribute_read(r, &attributes[index])) goto done;
    request = (yvex_ir_operation_request){.operation = name, .operands = inputs,
        .operand_count = input_count, .result_types = types, .result_count = result_count,
        .attributes = attributes, .attribute_count = attribute_count};
    if (yvex_ir_operation_add(r->module, block, &request, &created, err) != YVEX_OK) goto done;
    for (index = 0u; index < result_count; ++index)
        if (!ir_value_bind(r, old[index], r->module->operations[created].results[index])) goto done;
    for (index = 0u; index < region_count; ++index) {
        uint32_t argument_count;
        yvex_ir_id *arguments, body;
        int rc;
        if (!ir_get_count(r, &argument_count, IR_OBJECT_LIMIT)) goto done;
        arguments = ir_ids_read(r, argument_count, 0);
        if (!arguments) goto done;
        rc = yvex_ir_region_add(r->module, created, arguments, argument_count, &body, err);
        free(arguments);
        if (rc != YVEX_OK || !ir_block_read(r, body, depth + 1u, err)) goto done;
    }
    ok = 1;
done:
    free(old);
    free(types);
    free(inputs);
    return ok;
}

static int ir_block_read(ir_reader *r, yvex_ir_id block, unsigned int depth, yvex_error *err)
{
    uint32_t count, index;
    if (depth > 64u || !ir_get_count(r, &count, IR_OBJECT_LIMIT) ||
        count != r->module->blocks[block].argument_count) return 0;
    for (index = 0u; index < count; ++index) {
        uint32_t old;
        if (!ir_get_count(r, &old, (uint32_t)r->value_count) ||
            !ir_value_bind(r, old, r->module->blocks[block].arguments[index])) return 0;
    }
    if (!ir_get_count(r, &count, IR_OBJECT_LIMIT) || count > (r->size - r->offset) / 48u) return 0;
    for (index = 0u; index < count; ++index)
        if (!ir_operation_read(r, block, depth, err)) return 0;
    return 1;
}

static int ir_functions_read(ir_reader *r, yvex_error *err)
{
    uint32_t count, index;
    if (!ir_get_count(r, &count, IR_OBJECT_LIMIT) || count > (r->size - r->offset) / 32u) return 0;
    for (index = 0u; index < count; ++index) {
        char symbol[YVEX_IR_NAME_CAP];
        uint32_t effects, input_count, result_count;
        yvex_ir_id *arguments = NULL, *results = NULL, function;
        int ok = 0;
        if (ir_get_text(r, symbol, sizeof(symbol)) && ir_get_count(r, &effects, IR_EFFECT_MASK) &&
            ir_get_count(r, &input_count, IR_OBJECT_LIMIT) &&
            ir_get_count(r, &result_count, IR_OBJECT_LIMIT) &&
            (arguments = ir_ids_read(r, input_count, 0)) &&
            (results = ir_ids_read(r, result_count, 0)))
            ok = yvex_ir_function_add(r->module, symbol, arguments, input_count, results,
                                       result_count, effects, &function, err) == YVEX_OK;
        free(results);
        free(arguments);
        if (!ok) return 0;
    }
    for (index = 0u; index < count; ++index)
        if (!ir_block_read(r, r->module->functions[index].body, 0u, err)) return 0;
    return 1;
}

static int ir_header_write(const yvex_ir_module *m, yvex_core_bytes *bytes)
{
    size_t index;
    if (!ir_put_text(bytes, "yvex.ir.binary.v1") || !ir_put_text(bytes, m->name) ||
        !ir_put_text(bytes, m->source_identity) || !ir_put_text(bytes, m->identity) ||
        !ir_put(bytes, m->value_count) || !ir_put(bytes, m->dimension_count)) return 0;
    for (index = 0u; index < m->dimension_count; ++index) {
        const yvex_ir_dimension *d = &m->dimensions[index];
        if (!ir_put_text(bytes, d->name) || !ir_put(bytes, d->minimum) ||
            !ir_put(bytes, d->maximum) || !ir_put(bytes, d->multiple)) return 0;
    }
    if (!ir_put(bytes, m->type_count)) return 0;
    for (index = 0u; index < m->type_count; ++index)
        if (!ir_type_write(bytes, &m->types[index])) return 0;
    if (!ir_put(bytes, m->function_count)) return 0;
    for (index = 0u; index < yvex_ir_function_count(m); ++index) {
        const yvex_ir_function *f = &m->functions[index];
        const yvex_ir_block *block = &m->blocks[f->body];
        uint32_t argument;
        if (!ir_put_text(bytes, f->symbol) || !ir_put(bytes, f->effects) ||
            !ir_put(bytes, block->argument_count) || !ir_put(bytes, f->result_count)) return 0;
        for (argument = 0u; argument < block->argument_count; ++argument)
            if (!ir_put(bytes, m->values[block->arguments[argument]].type)) return 0;
        for (argument = 0u; argument < f->result_count; ++argument)
            if (!ir_put(bytes, f->results[argument])) return 0;
    }
    return 1;
}

int yvex_ir_encode(const yvex_ir_module *m, yvex_core_bytes *bytes, yvex_error *err)
{
    yvex_core_bytes encoded = {.maximum = IR_BINARY_LIMIT};
    size_t index;
    int ok;
    if (!m || !m->sealed || !bytes)
        return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "binary projection requires sealed module");
    ok = ir_header_write(m, &encoded);
    for (index = 0u; ok && index < m->function_count; ++index)
        ok = ir_block_write(m, m->functions[index].body, &encoded);
    if (ok) ok = yvex_core_bytes_append(bytes, encoded.data, encoded.count);
    free(encoded.data);
    if (!ok) return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "binary projection exceeds allocation/output budget");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_ir_decode(yvex_ir_module **out, const unsigned char *data, size_t size,
                    const yvex_ir_dialect *dialects, size_t dialect_count, yvex_error *err)
{
    ir_reader reader = {.data = data, .size = size};
    char schema[32], name[YVEX_IR_NAME_CAP], source[65], identity[65];
    uint32_t count, index, values;
    int rc = YVEX_ERR_FORMAT;
    if (out) *out = NULL;
    if (!out || !data || !size || size > IR_BINARY_LIMIT ||
        !ir_get_text(&reader, schema, sizeof(schema)) || strcmp(schema, "yvex.ir.binary.v1") ||
        !ir_get_text(&reader, name, sizeof(name)) || !ir_get_text(&reader, source, sizeof(source)) ||
        !ir_get_text(&reader, identity, sizeof(identity)) || !yvex_sha256_hex_valid(identity) ||
        !ir_get_count(&reader, &values, IR_OBJECT_LIMIT) ||
        yvex_ir_module_open(&reader.module, name, source, dialects, dialect_count, err) != YVEX_OK) goto done;
    reader.value_count = values;
    reader.values = malloc((values ? values : 1u) * sizeof(*reader.values));
    if (!reader.values) { rc = YVEX_ERR_NOMEM; goto done; }
    for (index = 0u; index < values; ++index) reader.values[index] = YVEX_IR_NONE;
    if (!ir_get_count(&reader, &count, IR_OBJECT_LIMIT) || count > (reader.size - reader.offset) / 32u) goto done;
    for (index = 0u; index < count; ++index) {
        yvex_ir_dimension d = {0};
        yvex_ir_id id;
        if (!ir_get_text(&reader, d.name, sizeof(d.name)) || !ir_get(&reader, &d.minimum) ||
            !ir_get(&reader, &d.maximum) || !ir_get(&reader, &d.multiple) ||
            yvex_ir_dimension_add(reader.module, &d, &id, err) != YVEX_OK) goto done;
    }
    if (!ir_get_count(&reader, &count, IR_OBJECT_LIMIT) || count > (reader.size - reader.offset) / 40u) goto done;
    for (index = 0u; index < count; ++index) {
        yvex_ir_type type;
        yvex_ir_id id;
        if (!ir_type_read(&reader, &type) ||
            yvex_ir_type_intern(reader.module, &type, &id, err) != YVEX_OK || id != index) goto done;
    }
    if (!ir_functions_read(&reader, err) || reader.offset != size || reader.module->value_count != values) goto done;
    for (index = 0u; index < values; ++index) if (reader.values[index] == YVEX_IR_NONE) goto done;
    if (yvex_ir_seal(reader.module, err) != YVEX_OK || strcmp(reader.module->identity, identity)) goto done;
    rc = YVEX_OK;
    *out = reader.module;
    reader.module = NULL;
done:
    free(reader.values);
    yvex_ir_module_close(&reader.module);
    if (rc != YVEX_OK)
        return yvex_ir_refuse(err, (yvex_status)rc, "IR binary is malformed, incompatible or unauthenticated");
    yvex_error_clear(err);
    return rc;
}
