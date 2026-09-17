/* Compile dense neural component computation, not generation orchestration. */
#include <yvex/internal/dense_program.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const yvex_dense_program_recipe *recipe;
    yvex_ir_module *module;
    yvex_ir_id block, hidden, cosine, sine;
    yvex_error *err;
    int rc;
} dense_builder;

static yvex_ir_id dense_type(dense_builder *b, unsigned long long rows, unsigned long long width)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = rows ? 2u : 1u,
        .shape = {{YVEX_IR_NONE, rows ? rows : width}, {YVEX_IR_NONE, rows ? width : 0u}}};
    yvex_ir_id id = YVEX_IR_NONE;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id dense_op(dense_builder *b, const char *name, const yvex_ir_id *args, size_t count,
    yvex_ir_id type, const yvex_ir_attribute *attrs, size_t attr_count)
{
    yvex_ir_id id = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .operands = args, .operand_count = count,
        .result_types = &type, .result_count = 1u, .attributes = attrs, .attribute_count = attr_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &id, b->err);
    return b->rc == YVEX_OK ? yvex_ir_operation_at(b->module, id)->results[0] : YVEX_IR_NONE;
}

static yvex_ir_id dense_parameter(dense_builder *b, unsigned long long ordinal,
    unsigned long long rows, unsigned long long width)
{
    yvex_ir_attribute a[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(a[0].value.text, sizeof(a[0].value.text), "weight_%llu", ordinal);
    yvex_core_text_copy(a[1].value.text, sizeof(a[1].value.text), b->recipe->semantic_identity);
    return dense_op(b, "core.parameter", NULL, 0u, dense_type(b, rows, width), a, 2u);
}

static yvex_ir_id dense_project(dense_builder *b, yvex_ir_id x, unsigned long long ordinal,
    unsigned long long rows, unsigned long long in_width, unsigned long long out_width)
{
    yvex_ir_id type = dense_type(b, rows, out_width);
    yvex_ir_id args[] = {x, dense_parameter(b, ordinal, out_width, in_width),
        dense_parameter(b, ordinal + 1u, 0u, out_width)};
    return dense_op(b, "nn.linear_bias", args, 3u, type, NULL, 0u);
}

static yvex_ir_id dense_norm(dense_builder *b, yvex_ir_id x, unsigned long long ordinal, int centered)
{
    yvex_ir_attribute a[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = b->recipe->epsilon},
        {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64}};
    yvex_ir_id args[3] = {x, dense_parameter(b, ordinal, 0u, b->recipe->width), YVEX_IR_NONE};
    if (centered) args[2] = dense_parameter(b, ordinal + 1u, 0u, b->recipe->width);
    return dense_op(b, centered ? "nn.layer_norm" : "nn.rms_norm", args,
        centered ? 3u : 2u, b->hidden, a, 2u);
}

static yvex_ir_id dense_attention(dense_builder *b, yvex_ir_id x, unsigned long long base)
{
    const yvex_dense_program_recipe *r = b->recipe;
    x = dense_project(b, x, base + 1u, r->rows, r->width, r->width * 3u);
    yvex_ir_attribute head = {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = r->head_dimension};
    yvex_ir_id types[] = {b->hidden, b->hidden, b->hidden}, op, values[3];
    yvex_ir_operation_request split = {.operation = "tensor.split_interleaved_three", .operands = &x,
        .operand_count = 1u, .result_types = types, .result_count = 3u, .attributes = &head, .attribute_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &split, &op, b->err);
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    memcpy(values, yvex_ir_operation_at(b->module, op)->results, sizeof(values));
    yvex_ir_attribute norm[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = r->epsilon},
        {.name = "group_width", .kind = YVEX_IR_ATTR_U64, .value.integer = r->head_dimension}};
    for (size_t i = 0u; i < 2u; ++i) {
        values[i] = dense_op(b, "nn.rms_normalize", values + i, 1u, b->hidden, norm, 2u);
        yvex_ir_id args[] = {values[i], b->cosine, b->sine};
        values[i] = dense_op(b, "tensor.rotary_half_f32", args, 3u, b->hidden, &head, 1u);
    }
    yvex_ir_attribute attention[] = {head, {.name = "causal", .kind = YVEX_IR_ATTR_BOOL}};
    x = dense_op(b, "attention.full", values, 3u, b->hidden, attention, 2u);
    return dense_project(b, x, base + 3u, r->rows, r->width, r->width);
}

static yvex_ir_id dense_block(dense_builder *b, yvex_ir_id x, unsigned long long base)
{
    const yvex_dense_program_recipe *r = b->recipe;
    yvex_ir_id update = dense_attention(b, dense_norm(b, x, base, 0), base);
    yvex_ir_id args[] = {x, update, dense_parameter(b, base + 5u, 0u, r->width)};
    x = dense_op(b, "tensor.scaled_residual", args, 3u, b->hidden, NULL, 0u);
    update = dense_norm(b, x, base + 6u, 0);
    update = dense_project(b, update, base + 7u, r->rows, r->width, r->ffn_width * 2u);
    yvex_ir_attribute gate = {.name = "gate_first", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u};
    update = dense_op(b, "nn.swiglu_split", &update, 1u, dense_type(b, r->rows, r->ffn_width), &gate, 1u);
    update = dense_project(b, update, base + 9u, r->rows, r->ffn_width, r->width);
    args[0] = x; args[1] = update; args[2] = dense_parameter(b, base + 11u, 0u, r->width);
    return dense_op(b, "tensor.scaled_residual", args, 3u, b->hidden, NULL, 0u);
}

static int dense_module(dense_builder *b)
{
    const yvex_dense_program_recipe *r = b->recipe;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_id function, row_symbol = YVEX_IR_NONE, input_types[3], output_type, op;
    yvex_ir_dimension rows = {.name = "rows", .minimum = r->rows, .maximum = r->rows, .multiple = 1u};
    b->rc = yvex_ir_module_open(&b->module, "dense_component", r->semantic_identity, dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &rows, &row_symbol, b->err);
    b->hidden = dense_type(b, r->rows, r->width);
    output_type = dense_type(b, r->output_rows, r->output_width);
    for (size_t i = 0u; i < 3u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
            .shape = {{row_symbol, 0u}, {YVEX_IR_NONE, i ? r->rotary_dimension : r->width}}};
        if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, input_types + i, b->err);
    }
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "forward", input_types, 3u,
        &output_type, 1u, 0u, &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    yvex_ir_id args[3];
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, sizeof(args));
    yvex_ir_id x = dense_op(b, "tensor.reshape", args, 1u, b->hidden, NULL, 0u);
    yvex_ir_id table = dense_type(b, r->rows, r->rotary_dimension);
    b->cosine = dense_op(b, "tensor.reshape", args + 1u, 1u, table, NULL, 0u);
    b->sine = dense_op(b, "tensor.reshape", args + 2u, 1u, table, NULL, 0u);
    for (unsigned long long i = 0u; b->rc == YVEX_OK && i < r->block_count; ++i) x = dense_block(b, x, i * 12u);
    x = dense_norm(b, x, r->block_count * 12u, 1);
    yvex_ir_attribute start = {.name = "start", .kind = YVEX_IR_ATTR_U64};
    x = dense_op(b, "tensor.slice_rows", &x, 1u, dense_type(b, r->output_rows, r->width), &start, 1u);
    x = dense_project(b, x, r->block_count * 12u + 2u, r->output_rows, r->width, r->output_width);
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = &x, .operand_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &ret, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

static int dense_lower(yvex_program_physical **out, dense_builder *b, size_t count, yvex_error *err)
{
    yvex_ir_module *canonical = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_parameter_binding *bindings = NULL;
    int rc = b->rc;
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    if (rc == YVEX_OK) rc = yvex_ir_pass_pipeline(b->module, passes, 2u, &canonical, NULL, err);
    if (rc == YVEX_OK && !(bindings = calloc((size_t)count, sizeof(*bindings)))) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "compiler.dense", "parameter lowering directory allocation failed");
        rc = YVEX_ERR_NOMEM;
    }
    size_t used = 0u;
    for (size_t i = 0u; rc == YVEX_OK && i < yvex_ir_operation_count(canonical); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(canonical, i);
        unsigned long long ordinal;
        char trailing;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const yvex_ir_attribute *symbol = yvex_ir_attribute_get(canonical, i, "parameter");
        if (used >= count || !symbol || sscanf(symbol->value.text, "weight_%llu%c", &ordinal, &trailing) != 1 ||
            ordinal >= count) { rc = YVEX_ERR_FORMAT; break; }
        bindings[used++] = (yvex_program_parameter_binding){op->results[0], ordinal, YVEX_GGUF_QTYPE_F32};
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, canonical, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "forward", bindings, used,
        yvex_ir_identity(canonical), err);
    free(bindings);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&canonical);
    yvex_ir_module_close(&b->module);
    return rc;
}

int yvex_dense_program_compile(yvex_program_physical **out, const yvex_dense_program_recipe *r, yvex_error *err)
{
    dense_builder b = {.recipe = r, .err = err};
    unsigned long long product, count;
    if (out) *out = NULL;
    if (!out || !r || !yvex_sha256_hex_valid(r->semantic_identity) || !r->rows ||
        !r->output_rows || r->output_rows > r->rows || !r->width || !r->heads || !r->head_dimension ||
        !yvex_core_u64_mul(r->heads, r->head_dimension, &product) || product != r->width ||
        !r->rotary_dimension || (r->rotary_dimension & 1u) || r->rotary_dimension > r->head_dimension ||
        !r->ffn_width || !r->block_count || !r->output_width || !isfinite(r->epsilon) || r->epsilon <= 0.0 ||
        !yvex_core_u64_mul(r->width, 3u, &product) || !yvex_core_u64_mul(r->ffn_width, 2u, &product) ||
        !yvex_core_u64_mul(r->block_count, 12u, &count) || !yvex_core_u64_add(count, 4u, &count) ||
        count > SIZE_MAX / sizeof(yvex_program_parameter_binding)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.dense", "source component geometry is invalid");
        return YVEX_ERR_FORMAT;
    }
    b.rc = dense_module(&b);
    return dense_lower(out, &b, (size_t)count, err);
}

int yvex_dense_prefix_compile(yvex_program_physical **out, const yvex_dense_prefix_recipe *r, yvex_error *err)
{
    unsigned long long joined, total;
    if (out) *out = NULL;
    if (!out || !r || !yvex_sha256_hex_valid(r->semantic_identity) || !r->rows || !r->input_width ||
        !r->intermediate_width || !r->output_width || !r->learned_rows || !r->padding_rows ||
        !yvex_core_u64_add(r->rows, r->learned_rows, &joined) ||
        !yvex_core_u64_add(joined, r->padding_rows, &total)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.dense.prefix", "source prefix geometry is invalid");
        return YVEX_ERR_FORMAT;
    }
    yvex_dense_program_recipe source = {.semantic_identity = r->semantic_identity};
    dense_builder b = {.recipe = &source, .err = err};
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_dimension rows = {.name = "rows", .minimum = r->rows, .maximum = r->rows, .multiple = 1u};
    yvex_ir_id symbol, input, output, function, op;
    b.rc = yvex_ir_module_open(&b.module, "dense_prefix", r->semantic_identity, dialects, 2u, err);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_dimension_add(b.module, &rows, &symbol, err);
    if (b.rc == YVEX_OK) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_F32, .rank = 2u,
            .shape = {{symbol, 0u}, {YVEX_IR_NONE, r->input_width}}};
        b.rc = yvex_ir_type_intern(b.module, &t, &input, err);
    }
    output = dense_type(&b, total, r->output_width);
    if (b.rc == YVEX_OK) b.rc = yvex_ir_function_add(b.module, "forward", &input, 1u, &output,
        1u, 0u, &function, err);
    if (b.rc == YVEX_OK) {
        b.block = yvex_ir_function_at(b.module, function)->body;
        yvex_ir_id x = yvex_ir_block_at(b.module, b.block)->arguments[0];
        x = dense_op(&b, "tensor.reshape", &x, 1u, dense_type(&b, r->rows, r->input_width), NULL, 0u);
        x = dense_project(&b, x, 0u, r->rows, r->input_width, r->intermediate_width);
        x = dense_project(&b, x, 2u, r->rows, r->intermediate_width, r->output_width);
        yvex_ir_id registers = dense_parameter(&b, 4u, r->learned_rows, r->output_width);
        registers = dense_op(&b, "tensor.copy", &registers, 1u,
            dense_type(&b, r->learned_rows, r->output_width), NULL, 0u);
        yvex_ir_id args[] = {x, registers};
        x = dense_op(&b, "tensor.concat_rows", args, 2u, dense_type(&b, joined, r->output_width), NULL, 0u);
        args[0] = x;
        args[1] = dense_op(&b, "tensor.zeros", NULL, 0u, dense_type(&b, r->padding_rows, r->output_width), NULL, 0u);
        x = dense_op(&b, "tensor.concat_rows", args, 2u, output, NULL, 0u);
        yvex_ir_operation_request ret = {.operation = "core.return", .operands = &x, .operand_count = 1u};
        if (b.rc == YVEX_OK) b.rc = yvex_ir_operation_add(b.module, b.block, &ret, &op, err);
    }
    if (b.rc == YVEX_OK) b.rc = yvex_ir_seal(b.module, err);
    return dense_lower(out, &b, 5u, err);
}
