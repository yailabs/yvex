/* Bind computational constants to sealed transformation and physical decisions. */
#include <yvex/internal/program.h>

#include <yvex/internal/compilation.h>
#include <yvex/internal/core.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>

#include <stdlib.h>
#include <string.h>

struct yvex_program_parameters {
    yvex_ir_module *module;
    yvex_program_parameter *parameters;
    size_t count;
    char identity[YVEX_SHA256_HEX_BYTES];
};

typedef struct {
    const yvex_transform_source_value *source;
    const yvex_transform_value *terminal;
    const yvex_physical_execution_decision *physical;
} program_realization;

typedef struct {
    const char *symbol;
    const char *decision;
} program_identity_entry;

static int program_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.program.parameters", reason);
    return status;
}

/* This is a proof of semantic identity, not a general provenance walk: a
 * transpose, cast, aggregation or other transform cannot silently implement a
 * constant whose program still expects the original source type and values. */
static const yvex_transform_value *program_identity_source(const yvex_transform_ir *ir,
    const yvex_transform_value *value, unsigned long long limit)
{
    unsigned long long step;
    for (step = 0u; value && step <= limit; ++step) {
        const yvex_transform_node *node;
        if (value->kind == YVEX_TRANSFORM_VALUE_SOURCE) return value;
        node = yvex_transform_ir_node_at(ir, value->producer_node_id);
        if (!node || node->kind != YVEX_TRANSFORM_OP_IDENTITY || node->input_count != 1u ||
            node->numeric != YVEX_TRANSFORM_NUMERIC_EXACT) return NULL;
        value = yvex_transform_ir_node_input_at(ir, node, 0u);
    }
    return NULL;
}

static int program_realization_order(const void *left, const void *right)
{
    const program_realization *a = left, *b = right;
    return strcmp(a->source->source_name, b->source->source_name);
}

static int program_identity_order(const void *left, const void *right)
{
    const program_identity_entry *a = left, *b = right;
    return strcmp(a->symbol, b->symbol);
}

static int program_realizations(const yvex_transform_ir *transform,
    const yvex_physical_execution_ir *physical, program_realization **out,
    size_t *count, yvex_error *err)
{
    const yvex_transform_ir_summary *ts = yvex_transform_ir_summary_get(transform);
    const yvex_physical_execution_summary *ps = yvex_physical_execution_ir_summary(physical);
    const yvex_physical_execution_decision **decisions;
    program_realization *map;
    unsigned long long index;
    size_t used = 0u;
    if (ts->terminal_count > 1048576u || ps->decision_count != ts->terminal_count)
        return program_refuse(err, YVEX_ERR_BOUNDS, "physical/transform parameter populations disagree");
    map = calloc((size_t)ts->terminal_count, sizeof(*map));
    decisions = calloc((size_t)ts->terminal_count, sizeof(*decisions));
    if (!map || !decisions) {
        free(map);
        free(decisions);
        return program_refuse(err, YVEX_ERR_NOMEM, "parameter realization index allocation failed");
    }
    for (index = 0u; index < ps->decision_count; ++index) {
        const yvex_physical_execution_decision *decision = yvex_physical_execution_ir_decision_at(physical, index);
        if (!decision || decision->terminal_tensor_id >= ts->terminal_count ||
            decisions[decision->terminal_tensor_id]) {
            free(map);
            free(decisions);
            return program_refuse(err, YVEX_ERR_FORMAT, "duplicate or out-of-range physical parameter identity");
        }
        decisions[decision->terminal_tensor_id] = decision;
    }
    for (index = 0u; index < ts->terminal_count; ++index) {
        const yvex_transform_value *terminal = yvex_transform_ir_terminal_at(transform, index);
        const yvex_transform_value *source = program_identity_source(transform, terminal, ts->node_count);
        if (!source) continue;
        map[used].source = yvex_transform_ir_source_at(transform, source->source_index);
        map[used].terminal = terminal;
        map[used].physical = decisions[index];
        if (!map[used].source || !terminal || terminal->canonical_ordinal != index) {
            free(map);
            free(decisions);
            return program_refuse(err, YVEX_ERR_FORMAT, "transformation identity source is unresolved");
        }
        used++;
    }
    free(decisions);
    qsort(map, used, sizeof(*map), program_realization_order);
    *out = map;
    *count = used;
    return YVEX_OK;
}

static yvex_transform_dtype program_dtype(yvex_ir_scalar scalar)
{
    switch (scalar) {
    case YVEX_IR_F32: return YVEX_TRANSFORM_DTYPE_F32;
    case YVEX_IR_F16: return YVEX_TRANSFORM_DTYPE_F16;
    case YVEX_IR_BF16: return YVEX_TRANSFORM_DTYPE_BF16;
    case YVEX_IR_I32: return YVEX_TRANSFORM_DTYPE_I32;
    case YVEX_IR_I64: return YVEX_TRANSFORM_DTYPE_I64;
    default: return YVEX_TRANSFORM_DTYPE_UNKNOWN;
    }
}

static int program_parameter_type(const yvex_ir_type *type, const program_realization *binding)
{
    const yvex_transform_value *terminal = binding->terminal;
    const yvex_transform_source_value *source = binding->source;
    unsigned long long elements = 1u, physical_elements;
    unsigned long long shape[2];
    yvex_gguf_qtype_storage_result storage;
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_at(binding->physical->canonical_qtype);
    uint32_t index;
    if (!type || type->kind != YVEX_IR_TENSOR || !type->rank ||
        type->rank != source->shape.rank || type->rank != terminal->shape.rank ||
        program_dtype(type->scalar) != source->value_dtype || source->value_dtype != terminal->dtype ||
        terminal->logical_key.role != binding->physical->role ||
        (unsigned int)terminal->logical_key.scope != (unsigned int)binding->physical->scope ||
        terminal->logical_key.layer_index != binding->physical->layer_index ||
        terminal->logical_key.auxiliary_index != binding->physical->predictor_index ||
        !capability || !capability->storage_admitted ||
        !(terminal->precision.allowed_physical_classes & capability->physical_class_mask) ||
        (capability->physical_class_mask == YVEX_TRANSFORM_PHYSICAL_QUANTIZED &&
         !terminal->precision.approximation_allowed))
        return 0;
    for (index = 0u; index < type->rank; ++index) {
        if (type->shape[index].symbol != YVEX_IR_NONE ||
            type->shape[index].extent != source->shape.dims[index] ||
            source->shape.dims[index] != terminal->shape.dims[index] ||
            !yvex_core_u64_mul(elements, type->shape[index].extent, &elements)) return 0;
    }
    /* Canonical row layout is a physical projection of the last logical axis.
     * A different layout requires an admitted lowering, not equal byte count. */
    shape[0] = binding->physical->canonical_row_width;
    shape[1] = binding->physical->canonical_row_count;
    return yvex_gguf_qtype_validate_tensor_storage(binding->physical->canonical_qtype,
               shape, 2u, binding->physical->encoded_bytes, &storage) == YVEX_GGUF_QTYPE_STORAGE_OK &&
           binding->physical->layout == YVEX_EXECUTION_LAYOUT_CANONICAL_ROW &&
           binding->physical->canonical_row_width == type->shape[type->rank - 1u].extent &&
           yvex_core_u64_mul(binding->physical->canonical_row_width,
                              binding->physical->canonical_row_count, &physical_elements) &&
           physical_elements == elements;
}

static int program_parameters_resolve(yvex_program_parameters *result,
    const program_realization *map, size_t count, program_identity_entry *identities, yvex_error *err)
{
    const yvex_ir_module *module = result->module;
    yvex_ir_id id;
    for (id = 0u; id < yvex_ir_operation_count(module); ++id) {
        const yvex_ir_operation *op = yvex_ir_operation_at(module, id);
        const yvex_ir_attribute *symbol, *source;
        size_t low = 0u, high = count;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        symbol = yvex_ir_attribute_get(module, id, "parameter");
        source = yvex_ir_attribute_get(module, id, "source");
        if (strcmp(source->value.text, yvex_ir_source_identity(module)))
            return program_refuse(err, YVEX_ERR_UNSUPPORTED, "parameter source has no admitted transformation input");
        while (low < high) {
            size_t middle = low + (high - low) / 2u;
            if (strcmp(map[middle].source->source_name, symbol->value.text) < 0) low = middle + 1u;
            else high = middle;
        }
        if (low == count || strcmp(map[low].source->source_name, symbol->value.text))
            return program_refuse(err, YVEX_ERR_UNSUPPORTED, "parameter has no identity-preserving realization");
        if (low + 1u < count && !strcmp(map[low + 1u].source->source_name, symbol->value.text))
            return program_refuse(err, YVEX_ERR_FORMAT, "parameter realization is ambiguous");
        if (!program_parameter_type(yvex_ir_type_at(module,
                yvex_ir_value_at(module, op->results[0])->type), &map[low]))
            return program_refuse(err, YVEX_ERR_FORMAT, "parameter semantic type or physical geometry disagrees");
        result->parameters[result->count] = (yvex_program_parameter){
            op->results[0], map[low].terminal->canonical_ordinal};
        identities[result->count++] = (program_identity_entry){
            symbol->value.text, map[low].physical->decision_identity};
    }
    return YVEX_OK;
}

int yvex_program_parameters_compile(yvex_program_parameters **out,
    const yvex_ir_module *module, const yvex_transform_ir *transform,
    const yvex_physical_execution_ir *physical, yvex_error *err)
{
    const yvex_transform_ir_summary *ts = yvex_transform_ir_summary_get(transform);
    const yvex_physical_execution_summary *ps = yvex_physical_execution_ir_summary(physical);
    yvex_program_parameters *result = NULL;
    program_realization *map = NULL;
    program_identity_entry *identities = NULL;
    size_t count = 0u, index, operations = yvex_ir_operation_count(module);
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    int rc;
    if (out) *out = NULL;
    if (!out || !yvex_ir_identity(module) || !ts || !ts->complete || !ts->terminal_count ||
        !ps || strcmp(yvex_ir_source_identity(module), ts->required_payload_identity))
        return program_refuse(err, YVEX_ERR_FORMAT, "sealed program and exact source/physical lowering are required");
    rc = program_realizations(transform, physical, &map, &count, err);
    if (rc != YVEX_OK) return rc;
    result = calloc(1u, sizeof(*result));
    if (result) {
        result->module = yvex_ir_module_retain(module, err);
        result->parameters = calloc(operations ? operations : 1u, sizeof(*result->parameters));
        identities = calloc(operations ? operations : 1u, sizeof(*identities));
    }
    if (!result || !result->module || !result->parameters || !identities)
        rc = program_refuse(err, YVEX_ERR_NOMEM, "physical parameter projection allocation failed");
    if (rc == YVEX_OK) rc = program_parameters_resolve(result, map, count, identities, err);
    if (rc == YVEX_OK) {
        qsort(identities, result->count, sizeof(*identities), program_identity_order);
        yvex_sha256_init(&hash);
        if (!yvex_sha256_update_text(&hash, "yvex.program.parameters.v1") ||
            !yvex_sha256_update_text(&hash, yvex_ir_identity(module)) ||
            !yvex_sha256_update_text(&hash, ts->transform_identity) ||
            !yvex_sha256_update_text(&hash, ps->identity) ||
            !yvex_sha256_update_u64(&hash, result->count)) rc = YVEX_ERR_STATE;
        for (index = 0u; rc == YVEX_OK && index < result->count; ++index)
            if (!yvex_sha256_update_text(&hash, identities[index].symbol) ||
                !yvex_sha256_update_text(&hash, identities[index].decision)) rc = YVEX_ERR_STATE;
        if (rc == YVEX_OK && !yvex_sha256_final(&hash, digest)) rc = YVEX_ERR_STATE;
        if (rc == YVEX_OK) yvex_sha256_hex(digest, result->identity);
        else rc = program_refuse(err, YVEX_ERR_STATE, "parameter projection identity failed");
    }
    free(map);
    free(identities);
    if (rc == YVEX_OK) { *out = result; yvex_error_clear(err); }
    else yvex_program_parameters_close(&result);
    return rc;
}

void yvex_program_parameters_close(yvex_program_parameters **owner)
{
    yvex_program_parameters *program = owner ? *owner : NULL;
    if (!program) return;
    yvex_ir_module_close(&program->module);
    free(program->parameters);
    free(program);
    *owner = NULL;
}

const yvex_ir_module *yvex_program_parameters_module(const yvex_program_parameters *program)
{
    return program ? program->module : NULL;
}

const char *yvex_program_parameters_identity(const yvex_program_parameters *program)
{
    return program ? program->identity : NULL;
}

size_t yvex_program_parameters_count(const yvex_program_parameters *program)
{
    return program ? program->count : 0u;
}

const yvex_program_parameter *yvex_program_parameter_at(const yvex_program_parameters *program, size_t index)
{
    return program && index < program->count ? &program->parameters[index] : NULL;
}
