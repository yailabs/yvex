/* Project neural patch/attention/merger computation into verified SSA.
 * Image decoding and request publication are not model operations. */
#include <yvex/internal/vision_program.h>
#include <yvex/qtype.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_ir_module *module;
    const yvex_vision_recipe *recipe;
    yvex_vision_program *program;
    yvex_ir_id block, population, hidden, ffn, merged, merged_hidden, cosine, sine;
    yvex_ir_id *results, *result_types;
    size_t result_count, observed;
    unsigned long long rows, merge_rows, patch_width, height, width;
    int inspect, rc;
    yvex_error *err;
} vision_builder;

static yvex_ir_id vision_type(vision_builder *b, unsigned long long rows,
    unsigned long long width, yvex_ir_scalar scalar)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = scalar, .rank = rows ? 2u : 1u,
        .shape = {{YVEX_IR_NONE, rows ? rows : width}, {YVEX_IR_NONE, rows ? width : 0u}}};
    yvex_ir_id id = YVEX_IR_NONE;
    if (!rows) memset(t.shape + 1u, 0, sizeof(t.shape[1]));
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id vision_op(vision_builder *b, const char *name, const yvex_ir_id *inputs,
    size_t count, yvex_ir_id type, const yvex_ir_attribute *attributes, size_t attribute_count)
{
    yvex_ir_id id, result = YVEX_IR_NONE;
    yvex_ir_operation_request r = {.operation = name, .operands = inputs, .operand_count = count,
        .result_types = &type, .result_count = 1u, .attributes = attributes, .attribute_count = attribute_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &id, b->err);
    if (b->rc == YVEX_OK) result = yvex_ir_operation_at(b->module, id)->results[0];
    return result;
}

static yvex_ir_id vision_parameter(vision_builder *b, unsigned long long ordinal,
    unsigned long long rows, unsigned long long width)
{
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(attrs[0].value.text, sizeof(attrs[0].value.text), "weight_%llu", ordinal);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->recipe->semantic_identity);
    return vision_op(b, "core.parameter", NULL, 0u, vision_type(b, rows, width, YVEX_IR_BF16), attrs, 2u);
}

static void vision_observation(vision_builder *b, yvex_ir_id value)
{
    if (b->inspect && b->rc == YVEX_OK) b->results[4u + b->observed++] = value;
}

static yvex_ir_id vision_linear(vision_builder *b, yvex_ir_id input, unsigned long long ordinal,
    unsigned long long in, unsigned long long out, yvex_ir_id type)
{
    yvex_ir_id operands[] = {input, vision_parameter(b, ordinal, out, in),
        vision_parameter(b, ordinal + 1u, 0u, out)};
    return vision_op(b, "nn.linear_bias", operands, 3u, type, NULL, 0u);
}

static yvex_ir_id vision_norm(vision_builder *b, yvex_ir_id input, unsigned long long ordinal,
    unsigned long long width, yvex_ir_id type)
{
    yvex_ir_attribute attrs[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
        .value.real = b->recipe->normalization_epsilon},
        {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64, .value.real = 0.0}};
    yvex_ir_id operands[] = {input, vision_parameter(b, ordinal, 0u, width),
        vision_parameter(b, ordinal + 1u, 0u, width)};
    return vision_op(b, "nn.layer_norm", operands, 3u, type, attrs, 2u);
}

static yvex_ir_id vision_merge(vision_builder *b, yvex_ir_id input, unsigned long long index, int post)
{
    const yvex_vision_recipe *r = b->recipe;
    unsigned long long width = r->hidden_width * r->merge * r->merge;
    unsigned long long base = 3u + r->layer_count * 12u + index * 6u;
    if (!post) input = vision_norm(b, input, base, r->hidden_width, b->hidden);
    input = vision_op(b, "tensor.reshape", &input, 1u, b->merged_hidden, NULL, 0u);
    if (post) input = vision_norm(b, input, base, width, b->merged_hidden);
    input = vision_linear(b, input, base + 2u, width, width, b->merged_hidden);
    input = vision_op(b, "nn.gelu", &input, 1u, b->merged_hidden, NULL, 0u);
    return vision_linear(b, input, base + 4u, width, r->output_width, b->merged);
}

static yvex_ir_id vision_block(vision_builder *b, yvex_ir_id input, unsigned long long layer)
{
    const yvex_vision_recipe *r = b->recipe;
    unsigned long long base = 3u + layer * 12u;
    yvex_ir_id qkv_type = vision_type(b, b->rows, r->hidden_width * 3u, YVEX_IR_BF16);
    yvex_ir_id normalized = vision_norm(b, input, base, r->hidden_width, b->hidden);
    if (!layer) vision_observation(b, normalized);
    yvex_ir_id qkv = vision_linear(b, normalized, base + 2u, r->hidden_width, r->hidden_width * 3u, qkv_type);
    if (!layer) vision_observation(b, qkv);
    yvex_ir_id types[] = {b->hidden, b->hidden, b->hidden}, values[3], operation;
    yvex_ir_operation_request split = {.operation = "tensor.split_three", .operands = &qkv,
        .operand_count = 1u, .result_types = types, .result_count = 3u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &split, &operation, b->err);
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    memcpy(values, yvex_ir_operation_at(b->module, operation)->results, sizeof(values));
    yvex_ir_attribute attrs[] = {{.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = r->head_dimension}, {.name = "causal", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 0u}};
    for (size_t i = 0u; i < 2u; ++i) {
        yvex_ir_id args[] = {values[i], b->cosine, b->sine};
        values[i] = vision_op(b, "tensor.rotary_half_f32", args, 3u, b->hidden, attrs, 1u);
        if (!layer) vision_observation(b, values[i]);
    }
    yvex_ir_id update = vision_op(b, "attention.full", values, 3u, b->hidden, attrs, 2u);
    if (!layer) vision_observation(b, update);
    update = vision_linear(b, update, base + 4u, r->hidden_width, r->hidden_width, b->hidden);
    if (!layer) vision_observation(b, update);
    yvex_ir_id add[] = {input, update};
    input = vision_op(b, "tensor.add", add, 2u, b->hidden, NULL, 0u);
    normalized = vision_norm(b, input, base + 6u, r->hidden_width, b->hidden);
    if (!layer) vision_observation(b, normalized);
    update = vision_linear(b, normalized, base + 8u, r->hidden_width, r->ffn_width, b->ffn);
    if (!layer) vision_observation(b, update);
    update = vision_op(b, "nn.gelu_tanh", &update, 1u, b->ffn, NULL, 0u);
    if (!layer) vision_observation(b, update);
    update = vision_linear(b, update, base + 10u, r->ffn_width, r->hidden_width, b->hidden);
    if (!layer) vision_observation(b, update);
    add[0] = input; add[1] = update;
    return vision_op(b, "tensor.add", add, 2u, b->hidden, NULL, 0u);
}

static int vision_signature(vision_builder *b)
{
    const yvex_vision_recipe *r = b->recipe;
    const unsigned int stages[] = {YVEX_VISION_OBSERVE_PATCH, YVEX_VISION_OBSERVE_POSITION,
        YVEX_VISION_OBSERVE_NORM1, YVEX_VISION_OBSERVE_QKV, YVEX_VISION_OBSERVE_QUERY,
        YVEX_VISION_OBSERVE_KEY, YVEX_VISION_OBSERVE_ATTENTION, YVEX_VISION_OBSERVE_ATTENTION_PROJECTION,
        YVEX_VISION_OBSERVE_NORM2, YVEX_VISION_OBSERVE_FF1, YVEX_VISION_OBSERVE_GELU, YVEX_VISION_OBSERVE_FF2};
    b->result_count = 4u + (b->inspect ? 12u + r->layer_count : 0u);
    b->results = calloc(b->result_count, sizeof(*b->results));
    b->result_types = calloc(b->result_count, sizeof(*b->result_types));
    if (!b->results || !b->result_types) return YVEX_ERR_NOMEM;
    for (size_t i = 0u; i < 4u; ++i) b->result_types[i] = b->merged;
    if (!b->inspect) return b->rc;
    b->program->observation_count = b->result_count - 4u;
    b->program->observations = calloc(b->program->observation_count, sizeof(*b->program->observations));
    if (!b->program->observations) return YVEX_ERR_NOMEM;
    for (size_t i = 0u; i < b->program->observation_count; ++i) {
        unsigned long long width = i == 3u ? r->hidden_width * 3u :
            i == 9u || i == 10u ? r->ffn_width : r->hidden_width;
        b->result_types[4u + i] = vision_type(b, b->rows, width, YVEX_IR_BF16);
        b->program->observations[i] = (yvex_vision_program_observation){
            i < 12u ? stages[i] : YVEX_VISION_OBSERVE_BLOCK, i < 12u ? 0u : i - 12u, b->rows, width};
    }
    return b->rc;
}

static int vision_module(vision_builder *b)
{
    const yvex_vision_recipe *r = b->recipe;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_dimension rows = {.name = "patches", .minimum = b->rows, .maximum = b->rows, .multiple = 1u};
    yvex_ir_id function, input_type, input, operation, table_types[2];
    b->rc = yvex_ir_module_open(&b->module, "vision_component", r->semantic_identity, dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &rows, &b->population, b->err);
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u,
        .shape = {{b->population, 0u}, {YVEX_IR_NONE, b->patch_width}}};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &input_type, b->err);
    b->hidden = vision_type(b, b->rows, r->hidden_width, YVEX_IR_BF16);
    b->ffn = vision_type(b, b->rows, r->ffn_width, YVEX_IR_BF16);
    b->merged_hidden = vision_type(b, b->merge_rows, r->hidden_width * r->merge * r->merge, YVEX_IR_BF16);
    b->merged = vision_type(b, b->merge_rows, r->output_width, YVEX_IR_BF16);
    if (b->rc == YVEX_OK) b->rc = vision_signature(b);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, b->inspect ? "inspect" : "encode",
        &input_type, 1u, b->result_types, b->result_count, 0u, &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    input = yvex_ir_block_at(b->module, b->block)->arguments[0];
    input = vision_op(b, "tensor.reshape", &input, 1u,
        vision_type(b, b->rows, b->patch_width, YVEX_IR_BF16), NULL, 0u);
    input = vision_linear(b, input, 0u, b->patch_width, r->hidden_width, b->hidden);
    vision_observation(b, input);
    yvex_ir_attribute attrs[] = {{.name = "grid_height", .kind = YVEX_IR_ATTR_U64, .value.integer = b->height},
        {.name = "grid_width", .kind = YVEX_IR_ATTR_U64, .value.integer = b->width},
        {.name = "block_size", .kind = YVEX_IR_ATTR_U64, .value.integer = r->merge},
        {.name = "source_side", .kind = YVEX_IR_ATTR_U64, .value.integer = r->position_grid_side}};
    yvex_ir_id position = vision_parameter(b, 2u, r->position_grid_side * r->position_grid_side, r->hidden_width);
    position = vision_op(b, "tensor.grid_bilinear", &position, 1u, b->hidden, attrs, 4u);
    yvex_ir_id add[] = {input, position};
    input = vision_op(b, "tensor.add", add, 2u, b->hidden, NULL, 0u);
    vision_observation(b, input);
    attrs[3] = (yvex_ir_attribute){.name = "theta", .kind = YVEX_IR_ATTR_F64, .value.real = (double)r->rope_theta};
    table_types[0] = table_types[1] = vision_type(b, b->rows, r->head_dimension, YVEX_IR_F32);
    yvex_ir_operation_request table = {.operation = "tensor.grid_rotary", .result_types = table_types,
        .result_count = 2u, .attributes = attrs, .attribute_count = 4u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &table, &operation, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->cosine = yvex_ir_operation_at(b->module, operation)->results[0];
    b->sine = yvex_ir_operation_at(b->module, operation)->results[1];
    size_t deep = 0u;
    for (unsigned long long layer = 0u; b->rc == YVEX_OK && layer < r->layer_count; ++layer) {
        input = vision_block(b, input, layer);
        vision_observation(b, input);
        if (deep < r->deepstack_layer_count && r->deepstack_layers[deep] == layer) {
            b->results[1u + deep] = vision_merge(b, input, 1u + deep, 1);
            deep++;
        }
    }
    b->results[0] = vision_merge(b, input, 0u, 0);
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = b->results,
        .operand_count = b->result_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &ret, &operation, b->err);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

static int vision_lower(vision_builder *b)
{
    yvex_ir_module *canonical = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    size_t count = yvex_ir_operation_count(b->module), used = 0u;
    yvex_program_parameter_binding *bindings = calloc(count, sizeof(*bindings));
    if (!bindings) return YVEX_ERR_NOMEM;
    int rc = yvex_ir_pass_pipeline(b->module, passes, 2u, &canonical, NULL, b->err);
    for (size_t i = 0u; rc == YVEX_OK && i < yvex_ir_operation_count(canonical); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(canonical, i);
        unsigned long long ordinal;
        char trailing;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const yvex_ir_attribute *symbol = yvex_ir_attribute_get(canonical, i, "parameter");
        if (!symbol || sscanf(symbol->value.text, "weight_%llu%c", &ordinal, &trailing) != 1) {
            rc = YVEX_ERR_FORMAT;
            break;
        }
        bindings[used++] = (yvex_program_parameter_binding){op->results[0], ordinal, YVEX_GGUF_QTYPE_BF16};
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, canonical, b->err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&b->program->physical, execution,
        b->inspect ? "inspect" : "encode", bindings, used, yvex_ir_identity(canonical), b->err);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&canonical);
    free(bindings);
    return rc;
}

int yvex_vision_program_compile(yvex_vision_program **out, const yvex_vision_recipe *r,
    unsigned long long height, unsigned long long width, int inspect, yvex_error *err)
{
    vision_builder b = {.recipe = r, .height = height, .width = width, .inspect = inspect, .err = err};
    unsigned long long channels, patch, merge, hidden, side;
    if (out) *out = NULL;
    if (!out || !r || r->schema_version != YVEX_VISION_RECIPE_SCHEMA_V1 ||
        !yvex_sha256_hex_valid(r->semantic_identity) || !height || !width || !r->merge ||
        height % r->merge || width % r->merge || !r->patch_channels || !r->temporal_patch ||
        !r->patch_height || !r->patch_width || !r->hidden_width || !r->ffn_width ||
        !r->heads || !r->head_dimension || r->head_dimension % 4u || !r->output_width ||
        !r->layer_count || r->layer_count > 4096u || r->deepstack_layer_count != 3u ||
        r->deepstack_layers[0] >= r->deepstack_layers[1] || r->deepstack_layers[1] >= r->deepstack_layers[2] ||
        r->deepstack_layers[2] >= r->layer_count || !r->position_grid_side || !r->rope_theta ||
        !isfinite(r->normalization_epsilon) || r->normalization_epsilon <= 0.0f ||
        !yvex_core_u64_mul(height, width, &b.rows) || b.rows > INT_MAX ||
        !yvex_core_u64_mul(r->heads, r->head_dimension, &hidden) || hidden != r->hidden_width ||
        !yvex_core_u64_mul(r->merge, r->merge, &merge) ||
        !yvex_core_u64_mul(merge, hidden, &channels) || channels > INT_MAX ||
        !yvex_core_u64_mul(r->patch_channels, r->temporal_patch, &channels) ||
        !yvex_core_u64_mul(r->patch_height, r->patch_width, &patch) ||
        !yvex_core_u64_mul(channels, patch, &b.patch_width) || b.patch_width > INT_MAX ||
        !yvex_core_u64_mul(r->position_grid_side, r->position_grid_side, &side)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.vision-program",
            "bounded exact source vision geometry required");
        return YVEX_ERR_FORMAT;
    }
    b.merge_rows = b.rows / merge;
    b.program = calloc(1u, sizeof(*b.program));
    if (!b.program) return YVEX_ERR_NOMEM;
    int rc = vision_module(&b);
    if (rc == YVEX_OK) rc = vision_lower(&b);
    yvex_ir_module_close(&b.module);
    free(b.results); free(b.result_types);
    if (rc == YVEX_OK) *out = b.program;
    else yvex_vision_program_close(&b.program);
    return rc;
}

void yvex_vision_program_close(yvex_vision_program **out)
{
    yvex_vision_program *p = out ? *out : NULL;
    if (!p) return;
    yvex_program_physical_close(&p->physical);
    free(p->observations); free(p); *out = NULL;
}
