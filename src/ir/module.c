/* Bounded ownership and construction of typed computational programs. */
#include "src/ir/private.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

int yvex_ir_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.ir", reason);
    return status;
}

int yvex_ir_name_valid(const char *name, int qualified)
{
    size_t index;
    int dot = 0;
    if (!name || !name[0]) return 0;
    for (index = 0u; index < YVEX_IR_NAME_CAP && name[index]; ++index) {
        unsigned char c = (unsigned char)name[index];
        if (c == '.') {
            if (!index || name[index - 1u] == '.') return 0;
            dot = 1;
        } else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                     c == '_' || (index && c >= '0' && c <= '9'))) return 0;
    }
    return index < YVEX_IR_NAME_CAP && name[index - 1u] != '.' &&
           (!qualified || dot);
}

static int ir_reserve(void **storage, size_t *capacity, size_t count, size_t item)
{
    size_t next;
    void *grown;
    if (count > IR_OBJECT_LIMIT || !item) return 0;
    if (count <= *capacity) return 1;
    next = *capacity ? *capacity : 16u;
    while (next < count) next *= 2u;
    if (next > SIZE_MAX / item) return 0;
    grown = realloc(*storage, next * item);
    if (!grown) return 0;
    *storage = grown;
    *capacity = next;
    return 1;
}

static void *ir_copy(const void *source, size_t count, size_t item)
{
    void *copy;
    if (!source || !count || count > IR_OBJECT_LIMIT || count > SIZE_MAX / item)
        return NULL;
    copy = malloc(count * item);
    if (copy) memcpy(copy, source, count * item);
    return copy;
}

const yvex_ir_operation_definition *yvex_ir_definition_find(
    const yvex_ir_module *module, const char *name)
{
    size_t dialect, operation;
    if (!module || !yvex_ir_name_valid(name, 1)) return NULL;
    for (dialect = 0u; dialect < module->dialect_count; ++dialect)
        for (operation = 0u; operation < module->dialects[dialect].count; ++operation) {
            const yvex_ir_operation_definition *definition =
                &module->dialects[dialect].operations[operation];
            if (strcmp(definition->name, name) == 0) return definition;
        }
    return NULL;
}

static int ir_builtin_definition_valid(const yvex_ir_operation_definition *op)
{
    const yvex_ir_dialect *builtins[] = {
        yvex_ir_core_dialect(), yvex_ir_neural_dialect(), yvex_ir_sequence_dialect()};
    size_t dialect, index;
    if (strncmp(op->name, "core.", 5u) && strncmp(op->name, "state.", 6u) &&
        strncmp(op->name, "tensor.", 7u) && strncmp(op->name, "nn.", 3u) &&
        strncmp(op->name, "attention.", 10u) && strncmp(op->name, "sequence.", 9u)) return 1;
    for (dialect = 0u; dialect < sizeof(builtins) / sizeof(builtins[0]); ++dialect)
        for (index = 0u; index < builtins[dialect]->count; ++index)
            if (op == &builtins[dialect]->operations[index]) return 1;
    return 0;
}

static int ir_dialects_valid(const yvex_ir_dialect *dialects, size_t count)
{
    size_t i, j, k, l;
    if (!dialects || !count || count > 256u) return 0;
    for (i = 0u; i < count; ++i) {
        if (!dialects[i].operations || !dialects[i].count ||
            dialects[i].count > 4096u) return 0;
        for (j = 0u; j < dialects[i].count; ++j) {
            const yvex_ir_operation_definition *op = &dialects[i].operations[j];
            if (!yvex_ir_name_valid(op->name, 1) || !op->version || !op->verify ||
                op->minimum_operands > op->maximum_operands ||
                op->minimum_results > op->maximum_results ||
                op->maximum_operands > IR_OBJECT_LIMIT ||
                op->maximum_results > IR_OBJECT_LIMIT ||
                op->regions > 2u || (op->effects & ~IR_EFFECT_MASK) ||
                op->attribute_count > 256u ||
                (op->attribute_count && !op->attributes) ||
                (op->terminator != 0 && op->terminator != 1)) return 0;
            /* Standard names have one semantic owner. Extensions use their
             * own namespace, not a namesake with different types/effects. */
            if (!ir_builtin_definition_valid(op)) return 0;
            for (k = 0u; k < op->attribute_count; ++k) {
                const yvex_ir_attribute_rule *rule = &op->attributes[k];
                if (!yvex_ir_name_valid(rule->name, 0) ||
                    rule->kind < YVEX_IR_ATTR_U64 || rule->kind > YVEX_IR_ATTR_SYMBOL ||
                    (rule->required != 0 && rule->required != 1)) return 0;
                for (l = 0u; l < k; ++l)
                    if (strcmp(rule->name, op->attributes[l].name) == 0) return 0;
            }
            for (k = 0u; k <= i; ++k)
                for (l = 0u; l < (k == i ? j : dialects[k].count); ++l)
                    if (strcmp(op->name, dialects[k].operations[l].name) == 0) return 0;
        }
    }
    return 1;
}

int yvex_ir_module_open(yvex_ir_module **out, const char *name,
                        const char *source_identity,
                        const yvex_ir_dialect *dialects, size_t dialect_count,
                        yvex_error *err)
{
    yvex_ir_module *module;
    if (out) *out = NULL;
    if (!out || !yvex_ir_name_valid(name, 0) ||
        !yvex_sha256_hex_valid(source_identity) ||
        !ir_dialects_valid(dialects, dialect_count))
        return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG,
                              "module requires source identity and unique static operation contracts");
    module = calloc(1u, sizeof(*module));
    if (module) atomic_init(&module->references, 1u);
    if (module) module->dialects = ir_copy(dialects, dialect_count, sizeof(*dialects));
    if (!module || !module->dialects) {
        yvex_ir_module_close(&module);
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR module allocation failed");
    }
    module->dialect_count = dialect_count;
    yvex_core_text_copy(module->name, sizeof(module->name), name);
    yvex_core_text_copy(module->source_identity, sizeof(module->source_identity), source_identity);
    *out = module;
    yvex_error_clear(err);
    return YVEX_OK;
}

static void ir_operation_clear(yvex_ir_operation *operation)
{
    free((void *)operation->operands);
    free((void *)operation->results);
    free((void *)operation->regions);
    free((void *)operation->attributes);
    memset(operation, 0, sizeof(*operation));
}

void yvex_ir_module_close(yvex_ir_module **owner)
{
    yvex_ir_module *module = owner ? *owner : NULL;
    size_t index;
    if (!module) return;
    *owner = NULL;
    if (atomic_fetch_sub_explicit(&module->references, 1u, memory_order_acq_rel) != 1u) return;
    for (index = 0u; index < module->operation_count; ++index)
        ir_operation_clear(&module->operations[index]);
    for (index = 0u; index < module->block_count; ++index)
        free((void *)module->blocks[index].arguments);
    for (index = 0u; index < module->function_count; ++index)
        free((void *)module->functions[index].results);
    free(module->functions);
    free(module->blocks);
    free(module->operations);
    free(module->values);
    free(module->types);
    free(module->dimensions);
    free(module->dialects);
    free(module);
}

yvex_ir_module *yvex_ir_module_retain(const yvex_ir_module *sealed, yvex_error *err)
{
    yvex_ir_module *module = (yvex_ir_module *)sealed;
    unsigned int count;
    if (!module || !module->sealed) {
        (void)yvex_ir_refuse(err, YVEX_ERR_STATE, "only immutable sealed programs may cross owner lifetimes");
        return NULL;
    }
    count = atomic_load_explicit(&module->references, memory_order_relaxed);
    do {
        if (!count || count == UINT_MAX) {
            (void)yvex_ir_refuse(err, YVEX_ERR_BOUNDS, "program reference population overflow");
            return NULL;
        }
    } while (!atomic_compare_exchange_weak_explicit(&module->references, &count, count + 1u,
                                                     memory_order_relaxed, memory_order_relaxed));
    yvex_error_clear(err);
    return module;
}

const char *yvex_ir_source_identity(const yvex_ir_module *module)
{
    return module && module->sealed ? module->source_identity : NULL;
}

int yvex_ir_dimension_add(yvex_ir_module *module, const yvex_ir_dimension *dimension,
                          yvex_ir_id *out, yvex_error *err)
{
    size_t index;
    if (out) *out = YVEX_IR_NONE;
    if (!module || module->sealed || !dimension || !out ||
        module->storage_bytes > IR_STORAGE_LIMIT - sizeof(*dimension) ||
        !yvex_ir_name_valid(dimension->name, 0) || !dimension->minimum ||
        dimension->minimum > dimension->maximum || !dimension->multiple ||
        dimension->minimum % dimension->multiple ||
        dimension->maximum % dimension->multiple)
        return yvex_ir_refuse(err, YVEX_ERR_INVALID_ARG, "invalid admitted dimension bounds");
    for (index = 0u; index < module->dimension_count; ++index)
        if (strcmp(module->dimensions[index].name, dimension->name) == 0)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "dimension symbol is already defined");
    if (!ir_reserve((void **)&module->dimensions, &module->dimension_capacity,
                    module->dimension_count + 1u, sizeof(*module->dimensions)))
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR dimension allocation failed");
    *out = (yvex_ir_id)module->dimension_count;
    module->dimensions[module->dimension_count++] = *dimension;
    module->storage_bytes += sizeof(*dimension);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_ir_extent_equal(yvex_ir_extent left, yvex_ir_extent right)
{
    return left.symbol == right.symbol && left.extent == right.extent;
}

int yvex_ir_type_equal(const yvex_ir_type *left, const yvex_ir_type *right)
{
    uint32_t index;
    if (!left || !right || left->rank > YVEX_IR_RANK_CAP ||
        left->member_count > YVEX_IR_AGGREGATE_CAP ||
        strnlen(left->domain, sizeof(left->domain)) == sizeof(left->domain) ||
        strnlen(right->domain, sizeof(right->domain)) == sizeof(right->domain) ||
        left->kind != right->kind || left->scalar != right->scalar ||
        left->rank != right->rank || left->member_count != right->member_count ||
        strcmp(left->domain, right->domain) != 0) return 0;
    for (index = 0u; index < left->rank; ++index)
        if (!yvex_ir_extent_equal(left->shape[index], right->shape[index])) return 0;
    for (index = 0u; index < left->member_count; ++index)
        if (left->members[index] != right->members[index]) return 0;
    return 1;
}

int yvex_ir_type_valid(const yvex_ir_module *module, const yvex_ir_type *type)
{
    uint32_t index;
    uint64_t elements = 1u;
    if (!module || !type || type->kind < YVEX_IR_SCALAR || type->kind > YVEX_IR_PROGRAM ||
        type->rank > YVEX_IR_RANK_CAP || type->member_count > YVEX_IR_AGGREGATE_CAP ||
        strnlen(type->domain, sizeof(type->domain)) == sizeof(type->domain)) return 0;
    if (type->kind == YVEX_IR_TUPLE || type->kind == YVEX_IR_PROGRAM) {
        if (type->scalar || type->rank || type->domain[0]) return 0;
        for (index = 0u; index < type->member_count; ++index) {
            if (type->members[index] >= module->type_count) return 0;
            if (type->kind == YVEX_IR_PROGRAM &&
                module->types[type->members[index]].kind != YVEX_IR_TUPLE) return 0;
        }
        return type->kind != YVEX_IR_PROGRAM || type->member_count == 2u;
    }
    if (type->scalar < YVEX_IR_BOOL || type->scalar > YVEX_IR_F64 || type->member_count ||
        (type->kind == YVEX_IR_SCALAR && type->rank) ||
        (type->kind == YVEX_IR_STATE ? !yvex_ir_name_valid(type->domain, 1) : type->domain[0]))
        return 0;
    for (index = 0u; index < type->rank; ++index) {
        const yvex_ir_extent *extent = &type->shape[index];
        uint64_t maximum;
        if (extent->symbol == YVEX_IR_NONE) {
            if (!extent->extent) return 0;
            maximum = extent->extent;
        } else {
            if (extent->symbol >= module->dimension_count || extent->extent) return 0;
            maximum = module->dimensions[extent->symbol].maximum;
        }
        if (elements > UINT64_MAX / maximum) return 0;
        elements *= maximum;
    }
    return 1;
}

int yvex_ir_type_intern(yvex_ir_module *module, const yvex_ir_type *type,
                        yvex_ir_id *out, yvex_error *err)
{
    size_t index;
    if (out) *out = YVEX_IR_NONE;
    if (!module || module->sealed || !out || !yvex_ir_type_valid(module, type) ||
        module->storage_bytes > IR_STORAGE_LIMIT - sizeof(*type))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "invalid logical type or shape");
    for (index = 0u; index < module->type_count; ++index)
        if (yvex_ir_type_equal(type, &module->types[index])) {
            *out = (yvex_ir_id)index;
            yvex_error_clear(err);
            return YVEX_OK;
        }
    if (!ir_reserve((void **)&module->types, &module->type_capacity,
                    module->type_count + 1u, sizeof(*module->types)))
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR type allocation failed");
    *out = (yvex_ir_id)module->type_count;
    module->types[module->type_count++] = *type;
    module->storage_bytes += sizeof(*type);
    yvex_error_clear(err);
    return YVEX_OK;
}

static int ir_aggregate_value_valid(const yvex_ir_module *module, yvex_ir_id id, unsigned int depth)
{
    const yvex_ir_type *type = &module->types[id];
    uint32_t index;
    if (depth > 64u) return 0;
    if (type->kind != YVEX_IR_TUPLE) return type->kind != YVEX_IR_STATE;
    for (index = 0u; index < type->member_count; ++index)
        if (!ir_aggregate_value_valid(module, type->members[index], depth + 1u)) return 0;
    return 1;
}

static int ir_types_present(const yvex_ir_module *module,
                            const yvex_ir_id *types, size_t count)
{
    size_t index;
    if (count > IR_OBJECT_LIMIT || (count && !types)) return 0;
    for (index = 0u; index < count; ++index)
        if (types[index] >= module->type_count ||
            (module->types[types[index]].kind == YVEX_IR_TUPLE &&
             !ir_aggregate_value_valid(module, types[index], 0u))) return 0;
    return 1;
}

static int ir_block_add(yvex_ir_module *module, yvex_ir_id function,
                        yvex_ir_id parent, const yvex_ir_id *types,
                        size_t count, yvex_ir_id *out, yvex_error *err)
{
    yvex_ir_id *arguments = NULL;
    size_t index;
    size_t bytes = sizeof(yvex_ir_block) + count * (sizeof(yvex_ir_value) + sizeof(yvex_ir_id));
    if (bytes > IR_STORAGE_LIMIT - module->storage_bytes)
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR block exceeds storage budget");
    if (count) arguments = malloc(count * sizeof(*arguments));
    if ((count && !arguments) ||
        !ir_reserve((void **)&module->blocks, &module->block_capacity,
                     module->block_count + 1u, sizeof(*module->blocks)) ||
        !ir_reserve((void **)&module->values, &module->value_capacity,
                     module->value_count + count, sizeof(*module->values))) {
        free(arguments);
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR block allocation failed");
    }
    *out = (yvex_ir_id)module->block_count;
    module->blocks[module->block_count++] = (yvex_ir_block){
        .function = function, .parent_operation = parent,
        .first_operation = YVEX_IR_NONE, .last_operation = YVEX_IR_NONE,
        .arguments = arguments, .argument_count = (uint32_t)count};
    module->storage_bytes += bytes;
    for (index = 0u; index < count; ++index) {
        arguments[index] = (yvex_ir_id)module->value_count;
        module->values[module->value_count++] = (yvex_ir_value){
            .type = types[index], .block = *out, .definition = YVEX_IR_NONE,
            .ordinal = (uint32_t)index};
    }
    return YVEX_OK;
}

int yvex_ir_function_add(yvex_ir_module *module, const char *symbol,
                         const yvex_ir_id *arguments, size_t argument_count,
                         const yvex_ir_id *results, size_t result_count,
                         uint32_t effects, yvex_ir_id *out, yvex_error *err)
{
    yvex_ir_function function = {0};
    int rc;
    if (out) *out = YVEX_IR_NONE;
    if (!module || module->sealed || !out || !yvex_ir_name_valid(symbol, 0) ||
        (effects & ~IR_EFFECT_MASK) ||
        !ir_types_present(module, arguments, argument_count) ||
        !ir_types_present(module, results, result_count) ||
        yvex_ir_function_find(module, symbol, NULL))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "invalid or duplicate function signature");
    if (sizeof(function) + result_count * sizeof(*results) + sizeof(yvex_ir_block) +
        argument_count * (sizeof(yvex_ir_value) + sizeof(yvex_ir_id)) >
        IR_STORAGE_LIMIT - module->storage_bytes)
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR function exceeds storage budget");
    if (result_count) function.results = ir_copy(results, result_count, sizeof(*results));
    if ((result_count && !function.results) ||
        !ir_reserve((void **)&module->functions, &module->function_capacity,
                    module->function_count + 1u, sizeof(*module->functions))) {
        free((void *)function.results);
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR function allocation failed");
    }
    rc = ir_block_add(module, (yvex_ir_id)module->function_count, YVEX_IR_NONE,
                      arguments, argument_count, &function.body, err);
    if (rc != YVEX_OK) {
        free((void *)function.results);
        return rc;
    }
    yvex_core_text_copy(function.symbol, sizeof(function.symbol), symbol);
    function.result_count = (uint32_t)result_count;
    function.effects = effects;
    *out = (yvex_ir_id)module->function_count;
    module->functions[module->function_count++] = function;
    module->storage_bytes += sizeof(function) + result_count * sizeof(*results);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_ir_operation_add(yvex_ir_module *module, yvex_ir_id block,
                          const yvex_ir_operation_request *request,
                          yvex_ir_id *out, yvex_error *err)
{
    yvex_ir_operation op = {.block = block, .next = YVEX_IR_NONE};
    yvex_ir_id *results;
    size_t index, bytes;
    if (out) *out = YVEX_IR_NONE;
    if (!module || module->sealed || !out || !request || block >= module->block_count ||
        !(op.definition = yvex_ir_definition_find(module, request->operation)) ||
        request->operand_count < op.definition->minimum_operands ||
        request->operand_count > op.definition->maximum_operands ||
        request->result_count < op.definition->minimum_results ||
        request->result_count > op.definition->maximum_results ||
        request->attribute_count > op.definition->attribute_count ||
        (request->operand_count && !request->operands) ||
        (request->attribute_count && !request->attributes) ||
        !ir_types_present(module, request->result_types, request->result_count))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operation is unknown or has invalid arity/types");
    bytes = sizeof(op) + request->operand_count * sizeof(*op.operands) +
            request->result_count * (sizeof(*results) + sizeof(yvex_ir_value)) +
            request->attribute_count * sizeof(*op.attributes);
    if (bytes > IR_STORAGE_LIMIT - module->storage_bytes)
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR operation exceeds storage budget");
    if (module->blocks[block].last_operation != YVEX_IR_NONE &&
        module->operations[module->blocks[block].last_operation].definition->terminator)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operation cannot follow a block terminator");
    for (index = 0u; index < request->operand_count; ++index)
        if (request->operands[index] >= module->value_count)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operand has no definition");
    op.operands = ir_copy(request->operands, request->operand_count, sizeof(*op.operands));
    op.attributes = ir_copy(request->attributes, request->attribute_count, sizeof(*op.attributes));
    results = request->result_count ? malloc(request->result_count * sizeof(*results)) : NULL;
    op.results = results;
    if ((request->operand_count && !op.operands) ||
        (request->attribute_count && !op.attributes) || (request->result_count && !results) ||
        !ir_reserve((void **)&module->operations, &module->operation_capacity,
                    module->operation_count + 1u, sizeof(*module->operations)) ||
        !ir_reserve((void **)&module->values, &module->value_capacity,
                    module->value_count + request->result_count, sizeof(*module->values))) {
        ir_operation_clear(&op);
        return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR operation allocation failed");
    }
    *out = (yvex_ir_id)module->operation_count;
    op.operand_count = (uint32_t)request->operand_count;
    op.result_count = (uint32_t)request->result_count;
    op.attribute_count = (uint32_t)request->attribute_count;
    for (index = 0u; index < request->result_count; ++index) {
        results[index] = (yvex_ir_id)module->value_count;
        module->values[module->value_count++] = (yvex_ir_value){
            .type = request->result_types[index], .block = block,
            .definition = *out, .ordinal = (uint32_t)index};
    }
    module->operations[module->operation_count++] = op;
    module->storage_bytes += bytes;
    if (module->blocks[block].last_operation == YVEX_IR_NONE)
        module->blocks[block].first_operation = *out;
    else module->operations[module->blocks[block].last_operation].next = *out;
    module->blocks[block].last_operation = *out;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_ir_region_add(yvex_ir_module *module, yvex_ir_id operation,
                       const yvex_ir_id *arguments, size_t argument_count,
                       yvex_ir_id *out, yvex_error *err)
{
    yvex_ir_operation *op;
    yvex_ir_id *regions;
    int rc;
    if (out) *out = YVEX_IR_NONE;
    if (!module || module->sealed || !out || operation >= module->operation_count ||
        !ir_types_present(module, arguments, argument_count))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "invalid region arguments or owner");
    op = &module->operations[operation];
    if (op->region_count >= op->definition->regions)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "operation region population exceeded");
    regions = realloc((void *)op->regions, (op->region_count + 1u) * sizeof(*regions));
    if (!regions) return yvex_ir_refuse(err, YVEX_ERR_NOMEM, "IR region allocation failed");
    op->regions = regions;
    rc = ir_block_add(module, module->blocks[op->block].function, operation,
                      arguments, argument_count, out, err);
    if (rc != YVEX_OK) return rc;
    regions[op->region_count++] = *out;
    yvex_error_clear(err);
    return YVEX_OK;
}

const yvex_ir_dimension *yvex_ir_dimension_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->dimension_count ? &m->dimensions[id] : NULL;
}

const yvex_ir_type *yvex_ir_type_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->type_count ? &m->types[id] : NULL;
}

const yvex_ir_value *yvex_ir_value_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->value_count ? &m->values[id] : NULL;
}

const yvex_ir_operation *yvex_ir_operation_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->operation_count ? &m->operations[id] : NULL;
}

const yvex_ir_block *yvex_ir_block_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->block_count ? &m->blocks[id] : NULL;
}

const yvex_ir_function *yvex_ir_function_at(const yvex_ir_module *m, yvex_ir_id id)
{
    return m && id < m->function_count ? &m->functions[id] : NULL;
}

const yvex_ir_attribute *yvex_ir_attribute_get(
    const yvex_ir_module *m, yvex_ir_id operation, const char *name)
{
    const yvex_ir_operation *op = yvex_ir_operation_at(m, operation);
    uint32_t index;
    if (!op || !name) return NULL;
    for (index = 0u; index < op->attribute_count; ++index)
        if (strcmp(op->attributes[index].name, name) == 0) return &op->attributes[index];
    return NULL;
}

size_t yvex_ir_function_count(const yvex_ir_module *m)
{
    return m ? m->function_count : 0u;
}

size_t yvex_ir_operation_count(const yvex_ir_module *m)
{
    return m ? m->operation_count : 0u;
}

int yvex_ir_function_find(const yvex_ir_module *m, const char *name, yvex_ir_id *out)
{
    size_t index;
    if (out) *out = YVEX_IR_NONE;
    if (!m || !name) return 0;
    for (index = 0u; index < m->function_count; ++index)
        if (strcmp(m->functions[index].symbol, name) == 0) {
            if (out) *out = (yvex_ir_id)index;
            return 1;
        }
    return 0;
}

const char *yvex_ir_identity(const yvex_ir_module *m)
{
    return m && m->sealed ? m->identity : NULL;
}
