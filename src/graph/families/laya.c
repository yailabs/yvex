/* Compile Laya's bounded bidirectional computation into the common physical SSA.
 * The model family owns names and source semantics, not execution or state. */
#include <yvex/internal/families/laya.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/execution.h>
#include <yvex/internal/program.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LAYA_PARAMETER_CAP 256u

typedef struct {
    char name[YVEX_IR_NAME_CAP];
    yvex_ir_id semantic_value;
} laya_parameter;

struct yvex_laya_program {
    yvex_program_physical *physical;
    laya_parameter parameters[LAYA_PARAMETER_CAP];
    size_t parameter_count;
};

typedef struct {
    const yvex_laya_program_recipe *recipe;
    yvex_laya_program *result;
    yvex_ir_module *module;
    yvex_ir_id block, row_symbol, hidden_type, rotary_type;
    yvex_ir_id full_cos, full_sin, local_cos, local_sin;
    yvex_error *err;
    int rc;
} laya_builder;

static yvex_ir_id laya_tensor(laya_builder *b, unsigned long long rows,
    unsigned long long width, int index)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR,
        .scalar = index ? YVEX_IR_INDEX : YVEX_IR_F32,
        .rank = width ? 2u : 1u};
    t.shape[0] = rows ? (yvex_ir_extent){YVEX_IR_NONE, rows} :
        (yvex_ir_extent){b->row_symbol, 0u};
    if (width) t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, width};
    yvex_ir_id id = YVEX_IR_NONE;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id laya_emit(laya_builder *b, const char *operation,
    const yvex_ir_id *operands, size_t operand_count, yvex_ir_id result_type,
    const yvex_ir_attribute *attributes, size_t attribute_count)
{
    yvex_ir_operation_request request = {.operation = operation, .operands = operands,
        .operand_count = operand_count, .result_types = &result_type, .result_count = 1u,
        .attributes = attributes, .attribute_count = attribute_count};
    yvex_ir_id op = YVEX_IR_NONE;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block,
        &request, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_operation_at(b->module, op)->results[0] : YVEX_IR_NONE;
}

static yvex_ir_id laya_parameter_emit(laya_builder *b, const char *name,
    unsigned long long rows, unsigned long long width)
{
    size_t ordinal = b->result->parameter_count;
    if (b->rc != YVEX_OK || !name || !*name || strlen(name) >= YVEX_IR_NAME_CAP ||
        ordinal >= LAYA_PARAMETER_CAP) {
        b->rc = YVEX_ERR_BOUNDS;
        yvex_error_set(b->err, b->rc, "compiler.laya.parameter",
            "bounded unique source parameter directory required");
        return YVEX_IR_NONE;
    }
    for (size_t i = 0u; i < ordinal; ++i)
        if (!strcmp(name, b->result->parameters[i].name)) {
            b->rc = YVEX_ERR_FORMAT;
            yvex_error_set(b->err, b->rc, "compiler.laya.parameter",
                "duplicate source parameter role");
            return YVEX_IR_NONE;
        }
    yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), name);
    yvex_core_text_copy(attrs[1].value.text, sizeof(attrs[1].value.text), b->recipe->source_identity);
    yvex_ir_id type = laya_tensor(b, rows, width, 0);
    yvex_ir_id value = laya_emit(b, "core.parameter", NULL, 0u, type, attrs, 2u);
    if (b->rc == YVEX_OK) {
        yvex_core_text_copy(b->result->parameters[ordinal].name,
            sizeof(b->result->parameters[ordinal].name), name);
        b->result->parameters[ordinal].semantic_value = value;
        b->result->parameter_count++;
    }
    return value;
}

static yvex_ir_id laya_named_parameter(laya_builder *b, const char *prefix,
    const char *suffix, unsigned long long rows, unsigned long long width)
{
    char name[YVEX_IR_NAME_CAP];
    size_t first = strlen(prefix), second = strlen(suffix);
    if (first >= sizeof(name) || second >= sizeof(name) - first - 1u) {
        b->rc = YVEX_ERR_BOUNDS;
        yvex_error_set(b->err, b->rc, "compiler.laya.parameter", "source name exceeds IR symbol capacity");
        return YVEX_IR_NONE;
    }
    memcpy(name, prefix, first);
    name[first] = '.';
    memcpy(name + first + 1u, suffix, second + 1u);
    return laya_parameter_emit(b, name, rows, width);
}

static int laya_name(laya_builder *b, char out[YVEX_IR_NAME_CAP],
    const char *prefix, const char *suffix)
{
    out[0] = '\0';
    size_t first = strlen(prefix), second = strlen(suffix);
    if (first >= YVEX_IR_NAME_CAP || second >= YVEX_IR_NAME_CAP - first - 1u) {
        b->rc = YVEX_ERR_BOUNDS;
        yvex_error_set(b->err, b->rc, "compiler.laya.name", "source name exceeds IR symbol capacity");
        return b->rc;
    }
    memcpy(out, prefix, first);
    out[first] = '.';
    memcpy(out + first + 1u, suffix, second + 1u);
    return YVEX_OK;
}

static yvex_ir_id laya_norm(laya_builder *b, yvex_ir_id value,
    const char *prefix, int biased)
{
    unsigned long long width = b->recipe->hidden_width;
    yvex_ir_id args[] = {value, laya_named_parameter(b, prefix, "weight", width, 0u),
        YVEX_IR_NONE};
    if (biased) args[2] = laya_named_parameter(b, prefix, "bias", width, 0u);
    yvex_ir_attribute attrs[] = {{.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
        .value.real = b->recipe->epsilon}, {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64}};
    return laya_emit(b, biased ? "nn.layer_norm" : "nn.layer_norm_unbiased",
        args, biased ? 3u : 2u, laya_tensor(b, 0u, width, 0), attrs, 2u);
}

static yvex_ir_id laya_linear(laya_builder *b, yvex_ir_id value,
    const char *prefix, unsigned long long input_width,
    unsigned long long output_width, int biased)
{
    yvex_ir_id args[] = {value,
        laya_named_parameter(b, prefix, "weight", output_width, input_width), YVEX_IR_NONE};
    if (biased) args[2] = laya_named_parameter(b, prefix, "bias", output_width, 0u);
    return laya_emit(b, biased ? "nn.linear_bias" : "nn.linear", args,
        biased ? 3u : 2u, laya_tensor(b, 0u, output_width, 0), NULL, 0u);
}

static int laya_split(laya_builder *b, yvex_ir_id value, unsigned int count,
    unsigned long long width, yvex_ir_id out[3])
{
    yvex_ir_id types[3], op = YVEX_IR_NONE;
    for (unsigned int i = 0u; i < count; ++i) types[i] = laya_tensor(b, 0u, width, 0);
    yvex_ir_operation_request request = {.operation = count == 3u ? "tensor.split_three" : "tensor.split_two",
        .operands = &value, .operand_count = 1u, .result_types = types, .result_count = count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &request, &op, b->err);
    if (b->rc == YVEX_OK) memcpy(out, yvex_ir_operation_at(b->module, op)->results,
        count * sizeof(*out));
    return b->rc;
}

static yvex_ir_id laya_add(laya_builder *b, yvex_ir_id left, yvex_ir_id right)
{
    yvex_ir_id args[] = {left, right};
    return laya_emit(b, "tensor.add", args, 2u, b->hidden_type, NULL, 0u);
}

static yvex_ir_id laya_attention(laya_builder *b, yvex_ir_id value,
    const char *prefix, int rotary, int full)
{
    unsigned long long width = b->recipe->hidden_width;
    unsigned long long head = width / b->recipe->attention_heads;
    char qkv_name[YVEX_IR_NAME_CAP], output_name[YVEX_IR_NAME_CAP];
    if (laya_name(b, qkv_name, prefix,
            rotary ? "attn.Wqkv" : "self_attn") != YVEX_OK ||
        laya_name(b, output_name, prefix,
            rotary ? "attn.Wo" : "self_attn.out_proj") != YVEX_OK) {
        return YVEX_IR_NONE;
    }
    yvex_ir_id projected;
    if (rotary) projected = laya_linear(b, value, qkv_name, width, width * 3u, 0);
    else {
        yvex_ir_id args[] = {value,
            laya_named_parameter(b, qkv_name, "in_proj_weight", width * 3u, width),
            laya_named_parameter(b, qkv_name, "in_proj_bias", width * 3u, 0u)};
        projected = laya_emit(b, "nn.linear_bias", args, 3u,
            laya_tensor(b, 0u, width * 3u, 0), NULL, 0u);
    }
    yvex_ir_id parts[3] = {YVEX_IR_NONE};
    laya_split(b, projected, 3u, width, parts);
    yvex_ir_attribute head_attr = {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = head};
    if (rotary) {
        yvex_ir_id cosine = full ? b->full_cos : b->local_cos;
        yvex_ir_id sine = full ? b->full_sin : b->local_sin;
        for (unsigned int i = 0u; i < 2u; ++i) {
            yvex_ir_id args[] = {parts[i], cosine, sine};
            parts[i] = laya_emit(b, "tensor.rotary_half_f32", args, 3u,
                b->hidden_type, &head_attr, 1u);
        }
    }
    yvex_ir_attribute attrs[] = {head_attr,
        {.name = "causal", .kind = YVEX_IR_ATTR_BOOL}};
    yvex_ir_id attended = laya_emit(b, "attention.full", parts, 3u,
        b->hidden_type, attrs, 2u);
    return laya_linear(b, attended, output_name, width, width, !rotary);
}

static yvex_ir_id laya_encoder_layer(laya_builder *b, yvex_ir_id value, unsigned long long layer)
{
    char prefix[YVEX_IR_NAME_CAP], name[YVEX_IR_NAME_CAP];
    int length = snprintf(prefix, sizeof(prefix), "encoder.layers.%llu", layer);
    if (length < 0 || (size_t)length >= sizeof(prefix)) { b->rc = YVEX_ERR_BOUNDS; return YVEX_IR_NONE; }
    yvex_ir_id normalized = value;
    if (layer) {
        laya_name(b, name, prefix, "attn_norm");
        normalized = laya_norm(b, value, name, 0);
    }
    yvex_ir_id update = laya_attention(b, normalized, prefix, 1, layer % 3u == 0u);
    value = laya_add(b, value, update);
    laya_name(b, name, prefix, "mlp_norm");
    normalized = laya_norm(b, value, name, 0);
    laya_name(b, name, prefix, "mlp.Wi");
    update = laya_linear(b, normalized, name, b->recipe->hidden_width,
        b->recipe->intermediate_width * 2u, 0);
    yvex_ir_id parts[3] = {YVEX_IR_NONE};
    laya_split(b, update, 2u, b->recipe->intermediate_width, parts);
    parts[0] = laya_emit(b, "nn.gelu", parts, 1u,
        laya_tensor(b, 0u, b->recipe->intermediate_width, 0), NULL, 0u);
    yvex_ir_id args[] = {parts[0], parts[1]};
    update = laya_emit(b, "tensor.multiply", args, 2u,
        laya_tensor(b, 0u, b->recipe->intermediate_width, 0), NULL, 0u);
    laya_name(b, name, prefix, "mlp.Wo");
    update = laya_linear(b, update, name, b->recipe->intermediate_width,
        b->recipe->hidden_width, 0);
    return laya_add(b, value, update);
}

static yvex_ir_id laya_head_layer(laya_builder *b, yvex_ir_id value, unsigned long long layer)
{
    char prefix[YVEX_IR_NAME_CAP], name[YVEX_IR_NAME_CAP];
    snprintf(prefix, sizeof(prefix), "head.layers.%llu", layer);
    laya_name(b, name, prefix, "norm1");
    yvex_ir_id normalized = laya_norm(b, value, name, 1);
    yvex_ir_id update = laya_attention(b, normalized, prefix, 0, 1);
    value = laya_add(b, value, update);
    laya_name(b, name, prefix, "norm2");
    normalized = laya_norm(b, value, name, 1);
    laya_name(b, name, prefix, "linear1");
    update = laya_linear(b, normalized, name, b->recipe->hidden_width,
        b->recipe->hidden_width * 4u, 1);
    yvex_ir_attribute clamp[] = {{.name = "lower", .kind = YVEX_IR_ATTR_F64},
        {.name = "upper", .kind = YVEX_IR_ATTR_F64, .value.real = 3.4028234663852886e38}};
    update = laya_emit(b, "tensor.clamp", &update, 1u,
        laya_tensor(b, 0u, b->recipe->hidden_width * 4u, 0), clamp, 2u);
    laya_name(b, name, prefix, "linear2");
    update = laya_linear(b, update, name, b->recipe->hidden_width * 4u,
        b->recipe->hidden_width, 1);
    return laya_add(b, value, update);
}

static int laya_module(laya_builder *b)
{
    const yvex_laya_program_recipe *r = b->recipe;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_dimension rows = {.name = "tokens", .minimum = 1u,
        .maximum = r->maximum_tokens, .multiple = 1u};
    yvex_ir_id inputs[6], output, function, op;
    b->rc = yvex_ir_module_open(&b->module, "laya_typed_decision", r->source_identity,
        dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &rows, &b->row_symbol, b->err);
    inputs[0] = laya_tensor(b, 0u, 0u, 1);
    inputs[1] = laya_tensor(b, 0u, 0u, 1);
    b->hidden_type = laya_tensor(b, 0u, r->hidden_width, 0);
    b->rotary_type = laya_tensor(b, 0u, r->hidden_width / r->attention_heads, 0);
    for (unsigned int i = 2u; i < 6u; ++i) inputs[i] = b->rotary_type;
    output = laya_tensor(b, 0u, 1u, 0);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "forward", inputs, 6u,
        &output, 1u, 0u, &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    const yvex_ir_id *args = yvex_ir_block_at(b->module, b->block)->arguments;
    b->full_cos = args[2]; b->full_sin = args[3];
    b->local_cos = args[4]; b->local_sin = args[5];
    yvex_ir_id token_weight = laya_named_parameter(b, "encoder.embeddings.tok_embeddings",
        "weight", r->vocabulary_size, r->hidden_width);
    yvex_ir_id embed_args[] = {args[0], token_weight};
    yvex_ir_id value = laya_emit(b, "nn.embedding", embed_args, 2u,
        b->hidden_type, NULL, 0u);
    value = laya_norm(b, value, "encoder.embeddings.norm", 0);
    for (unsigned long long i = 0u; b->rc == YVEX_OK && i < r->layer_count; ++i)
        value = laya_encoder_layer(b, value, i);
    value = laya_norm(b, value, "encoder.final_norm", 0);
    yvex_ir_id type_weight = laya_named_parameter(b, "type_emb", "weight", 3u, r->hidden_width);
    yvex_ir_id type_args[] = {args[1], type_weight};
    yvex_ir_id type_values = laya_emit(b, "nn.embedding", type_args, 2u,
        b->hidden_type, NULL, 0u);
    value = laya_add(b, value, type_values);
    for (unsigned long long i = 0u; b->rc == YVEX_OK && i < r->head_layer_count; ++i)
        value = laya_head_layer(b, value, i);
    value = laya_norm(b, value, "scorer.0", 1);
    value = laya_linear(b, value, "scorer.1", r->hidden_width, r->hidden_width, 1);
    value = laya_emit(b, "nn.gelu", &value, 1u, b->hidden_type, NULL, 0u);
    value = laya_linear(b, value, "scorer.3", r->hidden_width, 1u, 1);
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = &value,
        .operand_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &ret, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

int yvex_laya_program_compile(yvex_laya_program **out,
    const yvex_laya_program_recipe *recipe, yvex_error *err)
{
    if (out) *out = NULL;
    if (!out || !recipe || !yvex_sha256_hex_valid(recipe->source_identity) ||
        !recipe->maximum_tokens || recipe->maximum_tokens > 64u ||
        !recipe->layer_count || recipe->layer_count > 28u ||
        !recipe->head_layer_count || recipe->head_layer_count > 2u ||
        !recipe->vocabulary_size || !recipe->hidden_width ||
        !recipe->attention_heads || recipe->hidden_width % recipe->attention_heads ||
        (recipe->hidden_width / recipe->attention_heads) % 2u ||
        !recipe->intermediate_width || !isfinite(recipe->epsilon) ||
        recipe->epsilon <= 0.0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.laya",
            "exact bounded unpadded encoder and finite decision geometry required");
        return YVEX_ERR_FORMAT;
    }
    yvex_laya_program *program = calloc(1u, sizeof(*program));
    if (!program) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "compiler.laya", "program directory allocation failed");
        return YVEX_ERR_NOMEM;
    }
    laya_builder builder = {.recipe = recipe, .result = program, .err = err};
    int rc = laya_module(&builder);
    yvex_program_execution *execution = NULL;
    yvex_program_parameter_binding *bindings = NULL;
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, builder.module, err);
    if (rc == YVEX_OK) {
        bindings = calloc(program->parameter_count, sizeof(*bindings));
        if (!bindings) {
            rc = YVEX_ERR_NOMEM;
            yvex_error_set(err, rc, "compiler.laya", "parameter bindings unavailable");
        }
    }
    for (size_t i = 0u; rc == YVEX_OK && i < program->parameter_count; ++i)
        bindings[i] = (yvex_program_parameter_binding){.semantic_value = program->parameters[i].semantic_value,
            .tensor_id = i, .qtype = YVEX_GGUF_QTYPE_F32};
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&program->physical, execution,
        "forward", bindings, program->parameter_count, recipe->source_identity, err);
    free(bindings);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&builder.module);
    if (rc != YVEX_OK) { yvex_laya_program_close(&program); return rc; }
    *out = program;
    return YVEX_OK;
}

const yvex_program_physical *yvex_laya_program_physical(const yvex_laya_program *program)
{
    return program ? program->physical : NULL;
}

size_t yvex_laya_program_parameter_count(const yvex_laya_program *program)
{
    return program ? program->parameter_count : 0u;
}

int yvex_laya_program_parameter_name(void *context, unsigned long long tensor_id,
    char name[256], yvex_error *err)
{
    const yvex_laya_program *program = context;
    if (!program || !name || tensor_id >= program->parameter_count) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "compiler.laya.parameter", "unknown compiled parameter identity");
        return YVEX_ERR_BOUNDS;
    }
    yvex_core_text_copy(name, 256u, program->parameters[tensor_id].name);
    return YVEX_OK;
}

void yvex_laya_program_close(yvex_laya_program **program)
{
    if (!program || !*program) return;
    yvex_program_physical_close(&(*program)->physical);
    free(*program);
    *program = NULL;
}

/* The graph recipe is present before source/artifact admission. Catalog
 * projection deliberately remains unavailable until that authority exists. */
static const yvex_family_source_adapter *laya_unadmitted_source(void)
{
    return NULL;
}

extern const yvex_family_descriptor yvex_graph_family_descriptor_laya;
const yvex_family_descriptor yvex_graph_family_descriptor_laya = {
    .schema_version = YVEX_FAMILY_DESCRIPTOR_SCHEMA_V1,
    .target_id = "laya-typed-decisions",
    .family = "laya",
    .tokenizer_architecture = "modernbert",
    .tokenizer_pre = "default",
    .source = laya_unadmitted_source,
};
