/* Import a source-declared dense text component into the common compiler.
 * No payload access, backend dispatch, session state or runtime layer loop. */
#include <yvex/internal/text_program.h>
#include <yvex/qtype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_ir_module *module;
    const yvex_component_text_recipe *recipe;
    yvex_ir_id block, rows, hidden, query, kv, intermediate, tables;
    yvex_ir_id cosine, sine;
    yvex_error *err;
    int rc;
} text_builder;

static yvex_ir_id text_type(text_builder *b, unsigned long long rows, unsigned long long width, int activation)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = rows || activation ? 2u : 1u};
    yvex_ir_id id = YVEX_IR_NONE;
    t.shape[0] = (yvex_ir_extent){activation ? b->rows : YVEX_IR_NONE, activation ? 0u : rows ? rows : width};
    if (t.rank == 2u) t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, width};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id text_op(text_builder *b, const char *name, const yvex_ir_id *inputs, size_t count,
    yvex_ir_id type, const yvex_ir_attribute *attributes, size_t attribute_count)
{
    yvex_ir_id id, result = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .operands = inputs, .operand_count = count,
        .result_types = &type, .result_count = 1u, .attributes = attributes, .attribute_count = attribute_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &id, b->err);
    if (b->rc == YVEX_OK) result = yvex_ir_operation_at(b->module, id)->results[0];
    return result;
}

static yvex_ir_id text_parameter(text_builder *b, unsigned long long ordinal, yvex_ir_id type)
{
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "weight_%llu", ordinal);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->recipe->semantic_identity);
    return text_op(b, "core.parameter", NULL, 0u, type, attrs, 2u);
}

static yvex_ir_id text_norm(text_builder *b, yvex_ir_id input, yvex_ir_id weight, yvex_ir_id type)
{
    yvex_ir_id operands[] = {input, weight};
    yvex_ir_attribute epsilon = {.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
        .value.real = b->recipe->normalization_epsilon};
    return text_op(b, "nn.group_rms_norm", operands, 2u, type, &epsilon, 1u);
}

static yvex_ir_id text_linear(text_builder *b, yvex_ir_id input, yvex_ir_id weight,
    yvex_ir_id residual, yvex_ir_id type)
{
    yvex_ir_id operands[] = {input, weight, residual};
    return text_op(b, residual == YVEX_IR_NONE ? "nn.linear" : "nn.linear_residual",
        operands, residual == YVEX_IR_NONE ? 2u : 3u, type, NULL, 0u);
}

static yvex_ir_id text_rotate(text_builder *b, yvex_ir_id input, yvex_ir_id type)
{
    yvex_ir_id operands[] = {input, b->cosine, b->sine};
    yvex_ir_attribute head = {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = b->recipe->head_dimension};
    return text_op(b, "tensor.rotary_half", operands, 3u, type, &head, 1u);
}

static yvex_ir_id text_inject(text_builder *b, yvex_ir_id input, yvex_ir_id source, yvex_ir_id mask, int add)
{
    yvex_ir_id operands[] = {input, source, mask};
    yvex_ir_attribute attr = {.name = "add", .kind = YVEX_IR_ATTR_BOOL, .value.integer = (unsigned int)add};
    return text_op(b, "tensor.masked_rows", operands, 3u, b->hidden, &attr, 1u);
}

static yvex_ir_id text_block(text_builder *b, yvex_ir_id input, unsigned long long layer)
{
    const yvex_component_text_recipe *r = b->recipe;
    unsigned long long q = r->query_heads * r->head_dimension, kv = r->kv_heads * r->head_dimension;
    const unsigned long long rows[] = {0u, q, kv, kv, r->hidden_width, 0u, 0u, 0u,
        r->ffn_width, r->ffn_width, r->hidden_width};
    const unsigned long long widths[] = {r->hidden_width, r->hidden_width, r->hidden_width,
        r->hidden_width, q, r->head_dimension, r->head_dimension, r->hidden_width,
        r->hidden_width, r->hidden_width, r->ffn_width};
    yvex_ir_id w[11], normalized, query, key, value, attention, residual, gate, up, operands[3];
    yvex_ir_attribute attrs[] = {{.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = r->head_dimension}, {.name = "causal", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u}};
    for (size_t i = 0u; i < 11u; ++i)
        w[i] = text_parameter(b, 1u + layer * 11u + i, text_type(b, rows[i], widths[i], 0));
    normalized = text_norm(b, input, w[0], b->hidden);
    query = text_linear(b, normalized, w[1], YVEX_IR_NONE, b->query);
    key = text_linear(b, normalized, w[2], YVEX_IR_NONE, b->kv);
    value = text_linear(b, normalized, w[3], YVEX_IR_NONE, b->kv);
    query = text_norm(b, query, w[5], b->query);
    key = text_norm(b, key, w[6], b->kv);
    operands[0] = text_rotate(b, query, b->query);
    operands[1] = text_rotate(b, key, b->kv);
    operands[2] = value;
    attention = text_op(b, "attention.full", operands, 3u, b->query, attrs, 2u);
    residual = text_linear(b, attention, w[4], input, b->hidden);
    normalized = text_norm(b, residual, w[7], b->hidden);
    gate = text_linear(b, normalized, w[8], YVEX_IR_NONE, b->intermediate);
    up = text_linear(b, normalized, w[9], YVEX_IR_NONE, b->intermediate);
    operands[0] = gate; operands[1] = up;
    gate = text_op(b, "nn.silu_product", operands, 2u, b->intermediate, NULL, 0u);
    return text_linear(b, gate, w[10], residual, b->hidden);
}

static int text_module(text_builder *b, unsigned long long layers, unsigned long long maximum_rows,
    const unsigned long long sections[3], unsigned int injections)
{
    yvex_ir_dimension rows = {.name = "tokens", .minimum = 1u, .maximum = maximum_rows, .multiple = 1u};
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_id index, signature[70], function, op, args[70], result, operands[2];
    size_t input_count = injections ? 6u + injections : 4u;
    yvex_ir_type tokens = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_INDEX, .rank = 1u};
    const yvex_component_text_recipe *r = b->recipe;
    b->rc = yvex_ir_module_open(&b->module, "text_component", r->semantic_identity, dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &rows, &b->rows, b->err);
    tokens.shape[0] = (yvex_ir_extent){b->rows, 0u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &tokens, &index, b->err);
    b->hidden = text_type(b, 0u, r->hidden_width, 1);
    b->query = text_type(b, 0u, r->query_heads * r->head_dimension, 1);
    b->kv = text_type(b, 0u, r->kv_heads * r->head_dimension, 1);
    b->intermediate = text_type(b, 0u, r->ffn_width, 1);
    b->tables = text_type(b, 0u, r->head_dimension, 1);
    for (size_t i = 0u; i < 4u; ++i) signature[i] = index;
    if (injections) {
        signature[4] = text_type(b, 0u, 1u, 1);
        for (size_t i = 5u; i < input_count; ++i) signature[i] = b->hidden;
    }
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "encode", signature, input_count,
        &b->hidden, 1u, 0u, &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, input_count * sizeof(*args));
    yvex_ir_id tables[] = {b->tables, b->tables};
    yvex_ir_attribute attrs[] = {{.name = "theta", .kind = YVEX_IR_ATTR_F64, .value.real = (double)r->rope_theta},
        {.name = "section_y", .kind = YVEX_IR_ATTR_U64, .value.integer = sections ? sections[1] : 0u},
        {.name = "section_z", .kind = YVEX_IR_ATTR_U64, .value.integer = sections ? sections[2] : 0u}};
    yvex_ir_operation_request table = {.operation = "tensor.rotary_tables", .operands = args + 1u,
        .operand_count = 3u, .result_types = tables, .result_count = 2u, .attributes = attrs, .attribute_count = 3u};
    b->rc = yvex_ir_operation_add(b->module, b->block, &table, &op, b->err);
    if (b->rc == YVEX_OK) {
        b->cosine = yvex_ir_operation_at(b->module, op)->results[0];
        b->sine = yvex_ir_operation_at(b->module, op)->results[1];
    }
    operands[0] = args[0];
    operands[1] = text_parameter(b, 0u, text_type(b, r->vocabulary_size, r->hidden_width, 0));
    result = text_op(b, "nn.embedding", operands, 2u, b->hidden, NULL, 0u);
    if (injections) result = text_inject(b, result, args[5], args[4], 0);
    for (unsigned long long i = 0u; b->rc == YVEX_OK && i < layers; ++i) {
        result = text_block(b, result, i);
        if (i < injections) result = text_inject(b, result, args[6u + i], args[4], 1);
    }
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = &result, .operand_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &ret, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

static int text_lower(yvex_program_physical **out, const yvex_ir_module *module, yvex_error *err)
{
    yvex_program_execution *execution = NULL;
    size_t count = yvex_ir_operation_count(module), used = 0u;
    yvex_program_parameter_binding *bindings = calloc(count, sizeof(*bindings));
    if (!bindings) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "compiler.text-program", "parameter linkage allocation failed");
        return YVEX_ERR_NOMEM;
    }
    int rc = YVEX_OK;
    for (size_t i = 0u; i < count; ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(module, i);
        unsigned long long ordinal;
        char trailing;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const yvex_ir_attribute *symbol = yvex_ir_attribute_get(module, i, "parameter");
        if (!symbol || sscanf(symbol->value.text, "weight_%llu%c", &ordinal, &trailing) != 1) {
            yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.text-program", "parameter import identity lost");
            rc = YVEX_ERR_FORMAT;
            break;
        }
        bindings[used++] = (yvex_program_parameter_binding){op->results[0], ordinal, YVEX_GGUF_QTYPE_BF16};
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, module, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "encode", bindings, used,
        yvex_ir_identity(module), err);
    yvex_program_execution_close(&execution);
    free(bindings);
    return rc;
}

int yvex_text_program_compile(yvex_program_physical **out, const yvex_component_text_recipe *r,
    unsigned long long layers, unsigned long long maximum_rows, const unsigned long long sections[3],
    unsigned int injections, yvex_error *err)
{
    text_builder b = {.recipe = r, .err = err};
    yvex_ir_module *canonical = NULL;
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    unsigned long long width, section_total;
    if (out) *out = NULL;
    if (!out || !r || r->schema_version != YVEX_COMPONENT_TEXT_RECIPE_SCHEMA_V1 ||
        !yvex_sha256_hex_valid(r->semantic_identity) || !r->layer_capacity || layers > r->layer_capacity ||
        layers > 4096u || injections > 64u || (injections && !sections) ||
        !maximum_rows || maximum_rows > UINT_MAX || !r->hidden_width || !r->ffn_width ||
        !r->query_heads || !r->kv_heads || r->query_heads % r->kv_heads ||
        !r->head_dimension || r->head_dimension % 4u || !r->vocabulary_size || !r->rope_theta ||
        !isfinite(r->normalization_epsilon) || r->normalization_epsilon <= 0.0f ||
        !yvex_core_u64_mul(r->query_heads, r->head_dimension, &width) ||
        !yvex_core_u64_mul(r->kv_heads, r->head_dimension, &width) ||
        (sections && (!yvex_core_u64_add(sections[0], sections[1], &section_total) ||
            !yvex_core_u64_add(section_total, sections[2], &section_total) ||
            section_total != r->head_dimension / 2u))) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.text-program",
            "bounded admitted source text geometry required");
        return YVEX_ERR_FORMAT;
    }
    int rc = text_module(&b, layers, maximum_rows, sections, injections);
    if (rc == YVEX_OK) rc = yvex_ir_pass_pipeline(b.module, passes, 2u, &canonical, NULL, err);
    if (rc == YVEX_OK) rc = text_lower(out, canonical, err);
    yvex_ir_module_close(&canonical);
    yvex_ir_module_close(&b.module);
    return rc;
}

int yvex_text_program_condition(const yvex_component_text_recipe *recipe,
    const yvex_media_conditioning_request *request, yvex_runtime_av_conditioning_result *result,
    const unsigned long long sections[3], unsigned int injections,
    int (*execute)(const yvex_media_conditioning_request *, yvex_runtime_av_conditioning_result *, yvex_error *),
    yvex_error *err)
{
    yvex_program_physical *program = NULL;
    if (!request || request->schema_version != YVEX_MEDIA_CONDITIONING_SCHEMA_V3 || !result || !execute) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "compiler.text-entry", "typed component request required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_media_conditioning_request compiled = *request;
    int rc = yvex_text_program_compile(&program, recipe, request->layer_count, request->maximum_prompt_tokens,
        request->condition_count ? sections : NULL, request->condition_count ? injections : 0u, err);
    compiled.text_program = program;
    if (rc == YVEX_OK) rc = execute(&compiled, result, err);
    yvex_program_physical_close(&program);
    return rc;
}
