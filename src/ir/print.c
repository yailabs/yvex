/* Canonical diagnostic projection; no object memory or platform formatting. */
#include "src/ir/private.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const yvex_ir_module *module;
    yvex_core_bytes *bytes;
    yvex_ir_id *values, next_value;
} ir_printer;

static int ir_text(ir_printer *p, const char *format, ...)
{
    char text[512];
    va_list args;
    int count;
    va_start(args, format);
    count = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    return count >= 0 && (size_t)count < sizeof(text) &&
           yvex_core_bytes_append(p->bytes, text, (size_t)count);
}

static int ir_quote(ir_printer *p, const char *text)
{
    const unsigned char *c = (const unsigned char *)text;
    if (!ir_text(p, "\"")) return 0;
    for (; *c; ++c) {
        if (*c < 32u || *c >= 127u || *c == '\\' || *c == '"') {
            if (!ir_text(p, "\\x%02x", (unsigned int)*c)) return 0;
        } else if (!yvex_core_bytes_append(p->bytes, c, 1u)) return 0;
    }
    return ir_text(p, "\"");
}

static int ir_type_print(ir_printer *p, yvex_ir_id id, unsigned int depth)
{
    static const char *const scalars[] = {
        "invalid", "bool", "index", "i32", "i64", "f16", "bf16", "f32", "f64"};
    const yvex_ir_type *type = yvex_ir_type_at(p->module, id);
    uint32_t index;
    if (!type || depth > 32u) return 0;
    if (type->kind == YVEX_IR_SCALAR) return ir_text(p, "%s", scalars[type->scalar]);
    if (type->kind == YVEX_IR_TUPLE || type->kind == YVEX_IR_PROGRAM) {
        if (!ir_text(p, "%s<", type->kind == YVEX_IR_TUPLE ? "tuple" : "program")) return 0;
        for (index = 0u; index < type->member_count; ++index)
            if ((index && !ir_text(p, ", ")) ||
                !ir_type_print(p, type->members[index], depth + 1u)) return 0;
        return ir_text(p, ">");
    }
    if (!ir_text(p, "%s<", type->kind == YVEX_IR_STATE ? "state" : "tensor")) return 0;
    if (type->kind == YVEX_IR_STATE && !ir_text(p, "%s; ", type->domain)) return 0;
    for (index = 0u; index < type->rank; ++index) {
        const yvex_ir_extent *extent = &type->shape[index];
        if (extent->symbol == YVEX_IR_NONE) {
            if (!ir_text(p, "%" PRIu64 "x", extent->extent)) return 0;
        } else if (!ir_text(p, "${%s}x", yvex_ir_dimension_at(p->module, extent->symbol)->name)) return 0;
    }
    return ir_text(p, "%s>", scalars[type->scalar]);
}

static int ir_attribute_order(const void *left, const void *right)
{
    const yvex_ir_attribute *const *a = left, *const *b = right;
    return strcmp((*a)->name, (*b)->name);
}

static int ir_attributes_print(ir_printer *p, const yvex_ir_operation *op)
{
    const yvex_ir_attribute *ordered[256];
    uint32_t index;
    if (!op->attribute_count) return 1;
    for (index = 0u; index < op->attribute_count; ++index) ordered[index] = &op->attributes[index];
    qsort(ordered, op->attribute_count, sizeof(*ordered), ir_attribute_order);
    if (!ir_text(p, " {")) return 0;
    for (index = 0u; index < op->attribute_count; ++index) {
        const yvex_ir_attribute *a = ordered[index];
        uint64_t bits;
        if ((index && !ir_text(p, ", ")) || !ir_text(p, "%s = ", a->name)) return 0;
        switch (a->kind) {
        case YVEX_IR_ATTR_U64:
            if (!ir_text(p, "u64(%" PRIu64 ")", a->value.integer)) return 0;
            break;
        case YVEX_IR_ATTR_F64:
            memcpy(&bits, &a->value.real, sizeof(bits));
            if (!ir_text(p, "f64bits(0x%016" PRIx64 ")", bits)) return 0;
            break;
        case YVEX_IR_ATTR_BOOL:
            if (!ir_text(p, "%s", a->value.integer ? "true" : "false")) return 0;
            break;
        case YVEX_IR_ATTR_SYMBOL:
            if (!ir_text(p, "@%s", a->value.text)) return 0;
            break;
        case YVEX_IR_ATTR_TEXT:
            if (!ir_quote(p, a->value.text)) return 0;
            break;
        case YVEX_IR_ATTR_TYPE:
            if (!ir_type_print(p, a->value.type, 0u)) return 0;
            break;
        default: return 0;
        }
    }
    return ir_text(p, "}");
}

static int ir_arguments_print(ir_printer *p, const yvex_ir_block *block)
{
    uint32_t index;
    for (index = 0u; index < block->argument_count; ++index) {
        yvex_ir_id value = block->arguments[index];
        p->values[value] = p->next_value++;
        if ((index && !ir_text(p, ", ")) || !ir_text(p, "%%%u: ", p->values[value]) ||
            !ir_type_print(p, p->module->values[value].type, 0u)) return 0;
    }
    return 1;
}

static int ir_block_print(ir_printer *p, yvex_ir_id block_id, unsigned int depth)
{
    const yvex_ir_block *block = &p->module->blocks[block_id];
    yvex_ir_id id;
    if (depth > 64u) return 0;
    for (id = block->first_operation; id != YVEX_IR_NONE; id = p->module->operations[id].next) {
        const yvex_ir_operation *op = &p->module->operations[id];
        uint32_t index;
        if (!ir_text(p, "%*s", (int)(depth * 2u), "")) return 0;
        for (index = 0u; index < op->result_count; ++index) {
            yvex_ir_id value = op->results[index];
            p->values[value] = p->next_value++;
            if ((index && !ir_text(p, ", ")) || !ir_text(p, "%%%u", p->values[value])) return 0;
        }
        if ((op->result_count && !ir_text(p, " = ")) ||
            !ir_text(p, "%s.v%u(", op->definition->name, op->definition->version)) return 0;
        for (index = 0u; index < op->operand_count; ++index)
            if ((index && !ir_text(p, ", ")) ||
                !ir_text(p, "%%%u", p->values[op->operands[index]])) return 0;
        if (!ir_text(p, ")") || !ir_attributes_print(p, op) || !ir_text(p, " -> (")) return 0;
        for (index = 0u; index < op->result_count; ++index)
            if ((index && !ir_text(p, ", ")) ||
                !ir_type_print(p, p->module->values[op->results[index]].type, 0u)) return 0;
        if (!ir_text(p, ") effects=0x%x\n", yvex_ir_operation_effects(p->module, id))) return 0;
        for (index = 0u; index < op->region_count; ++index) {
            if (!ir_text(p, "%*sregion %u(", (int)(depth * 2u), "", index) ||
                !ir_arguments_print(p, &p->module->blocks[op->regions[index]]) ||
                !ir_text(p, ") {\n") || !ir_block_print(p, op->regions[index], depth + 1u) ||
                !ir_text(p, "%*s}\n", (int)(depth * 2u), "")) return 0;
        }
    }
    return 1;
}

static int ir_dimension_order(const void *left, const void *right)
{
    const yvex_ir_dimension *const *a = left, *const *b = right;
    return strcmp((*a)->name, (*b)->name);
}

static int ir_function_order(const void *left, const void *right)
{
    const yvex_ir_function *const *a = left, *const *b = right;
    return strcmp((*a)->symbol, (*b)->symbol);
}

static int ir_module_print(ir_printer *p)
{
    const yvex_ir_module *m = p->module;
    const yvex_ir_dimension **dimensions = NULL;
    const yvex_ir_function **functions = NULL;
    size_t index;
    int ok = 0;
    dimensions = malloc((m->dimension_count ? m->dimension_count : 1u) * sizeof(*dimensions));
    functions = malloc(m->function_count * sizeof(*functions));
    if (!dimensions || !functions) goto done;
    for (index = 0u; index < m->dimension_count; ++index) dimensions[index] = &m->dimensions[index];
    for (index = 0u; index < m->function_count; ++index) functions[index] = &m->functions[index];
    qsort(dimensions, m->dimension_count, sizeof(*dimensions), ir_dimension_order);
    qsort(functions, m->function_count, sizeof(*functions), ir_function_order);
    if (!ir_text(p, "yvex.ir.text.v1\nmodule @%s source=%s {\n", m->name, m->source_identity)) goto done;
    for (index = 0u; index < m->dimension_count; ++index) {
        const yvex_ir_dimension *d = dimensions[index];
        if (!ir_text(p, "  dim $%s [%" PRIu64 ", %" PRIu64 "] multiple=%" PRIu64 "\n",
                     d->name, d->minimum, d->maximum, d->multiple)) goto done;
    }
    for (index = 0u; index < m->function_count; ++index) {
        const yvex_ir_function *f = functions[index];
        uint32_t result;
        p->next_value = 0u;
        if (!ir_text(p, "  func @%s(", f->symbol) ||
            !ir_arguments_print(p, &m->blocks[f->body]) || !ir_text(p, ") -> (")) goto done;
        for (result = 0u; result < f->result_count; ++result)
            if ((result && !ir_text(p, ", ")) || !ir_type_print(p, f->results[result], 0u)) goto done;
        if (!ir_text(p, ") effects=0x%x {\n", f->effects) ||
            !ir_block_print(p, f->body, 2u) || !ir_text(p, "  }\n")) goto done;
    }
    ok = ir_text(p, "}\n");
done:
    free(functions);
    free(dimensions);
    return ok;
}

int yvex_ir_print(const yvex_ir_module *m, yvex_core_bytes *bytes, yvex_error *err)
{
    ir_printer printer = {.module = m, .bytes = bytes};
    size_t start;
    int rc = yvex_ir_verify(m, err);
    if (rc != YVEX_OK) return rc;
    if (!bytes) return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "IR output is required");
    start = bytes->count;
    printer.values = calloc(m->value_count ? m->value_count : 1u, sizeof(*printer.values));
    if (!printer.values || !ir_module_print(&printer)) {
        bytes->count = start;
        rc = yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR projection exceeds output/allocation budget");
    }
    free(printer.values);
    return rc;
}

int yvex_ir_seal(yvex_ir_module *m, yvex_error *err)
{
    yvex_core_bytes bytes = {.maximum = 64u * 1024u * 1024u};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc;
    if (!m) return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "IR module is required");
    if (m->sealed) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    rc = yvex_ir_print(m, &bytes, err);
    if (rc == YVEX_OK) {
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.ir.semantic.v1") ||
            !yvex_sha256_update(&hash, bytes.data, bytes.count) ||
            !yvex_sha256_final(&hash, digest))
            rc = yvex_ir_refuse(err, YVEX_ERR_FORMAT, "IR identity projection failed");
        else {
            yvex_sha256_hex(digest, m->identity);
            m->sealed = 1;
        }
    }
    free(bytes.data);
    return rc;
}
