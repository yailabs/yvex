/* Interpret pure Mamba2 source topology and exact BF16 tensor roles before lowering. */
#include <yvex/internal/families/mamba2.h>

#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const yvex_ir_type *mamba_program_input(
    const yvex_ir_module *m, const yvex_ir_operation *op, uint32_t index)
{
    return yvex_ir_type_at(m, yvex_ir_value_at(m, op->operands[index])->type);
}

static int mamba_program_shape(const yvex_ir_type *type, yvex_ir_type_kind kind,
    uint32_t rank, const unsigned long long *shape, const char *domain)
{
    uint32_t index;
    if (!type || type->kind != kind || type->scalar != YVEX_IR_F32 || type->rank != rank ||
        (domain && strcmp(type->domain, domain))) return 0;
    for (index = 0u; index < rank; ++index)
        if (type->shape[index].symbol != YVEX_IR_NONE || type->shape[index].extent != shape[index]) return 0;
    return 1;
}

static int mamba_program_mixer_verify(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = yvex_ir_operation_at(m, id);
    const yvex_ir_type *input = mamba_program_input(m, op, 0u);
    const yvex_ir_attribute *heads = yvex_ir_attribute_get(m, id, "heads");
    const yvex_ir_attribute *head = yvex_ir_attribute_get(m, id, "head_dimension");
    const yvex_ir_attribute *state = yvex_ir_attribute_get(m, id, "state_dimension");
    const yvex_ir_attribute *groups = yvex_ir_attribute_get(m, id, "groups");
    const yvex_ir_attribute *kernel = yvex_ir_attribute_get(m, id, "convolution_kernel");
    yvex_selective_ssd_requirement requirement = {.schema_version = YVEX_SELECTIVE_SSD_SCHEMA_V1};
    yvex_selective_ssd_geometry geometry;
    unsigned long long shapes[10][3], hidden;
    const uint32_t ranks[] = {2u, 3u, 1u, 1u, 1u, 1u, 1u, 2u, 2u, 3u};
    uint32_t index;
    int rc;
    if (!input || input->kind != YVEX_IR_TENSOR || input->scalar != YVEX_IR_F32 || input->rank != 2u ||
        input->shape[1].symbol != YVEX_IR_NONE || !heads || !head || !state || !groups || !kernel) goto invalid;
    requirement.heads = heads->value.integer;
    requirement.head_dimension = head->value.integer;
    requirement.state_dimension = state->value.integer;
    requirement.groups = groups->value.integer;
    requirement.convolution_kernel = kernel->value.integer;
    requirement.normalization_groups = yvex_ir_attribute_get(m, id, "normalization_groups")->value.integer;
    requirement.normalization_epsilon = yvex_ir_attribute_get(m, id, "epsilon")->value.real;
    requirement.time_step_minimum = yvex_ir_attribute_get(m, id, "time_step_minimum")->value.real;
    requirement.time_step_maximum = yvex_ir_attribute_get(m, id, "time_step_maximum")->value.real;
    requirement.time_step_unbounded = (int)yvex_ir_attribute_get(m, id, "time_step_unbounded")->value.integer;
    requirement.norm_before_gate = (int)yvex_ir_attribute_get(m, id, "norm_before_gate")->value.integer;
    rc = yvex_selective_ssd_geometry_seal(&geometry, &requirement, err);
    if (rc != YVEX_OK) return rc;
    hidden = input->shape[1].extent;
    {
        const unsigned long long expected[10][3] = {
            {geometry.projection_width, hidden, 0u}, {geometry.convolution_width, 1u, requirement.convolution_kernel},
            {geometry.convolution_width, 0u, 0u}, {requirement.heads, 0u, 0u},
            {requirement.heads, 0u, 0u}, {requirement.heads, 0u, 0u}, {geometry.width, 0u, 0u},
            {hidden, geometry.width, 0u}, {geometry.convolution_width, requirement.convolution_kernel, 0u},
            {requirement.heads, requirement.head_dimension, requirement.state_dimension}};
        memcpy(shapes, expected, sizeof(shapes));
    }
    for (index = 0u; index < 10u; ++index)
        if (!mamba_program_shape(mamba_program_input(m, op, index + 1u),
                                 index < 8u ? YVEX_IR_TENSOR : YVEX_IR_STATE, ranks[index], shapes[index],
                                 index < 8u ? NULL : index == 8u ? "convolution.causal" : "ssm.selective"))
            goto invalid;
    if (yvex_ir_value_at(m, op->operands[0])->type != yvex_ir_value_at(m, op->results[0])->type ||
        yvex_ir_value_at(m, op->operands[9])->type != yvex_ir_value_at(m, op->results[1])->type ||
        yvex_ir_value_at(m, op->operands[10])->type != yvex_ir_value_at(m, op->results[2])->type) goto invalid;
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.program", "selective-SSD program operand/state geometry mismatch");
    return YVEX_ERR_FORMAT;
}

static int mamba_obligation_verify(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_attribute *name = yvex_ir_attribute_get(m, id, "authority");
    if (!name || (!name->value.text[0])) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.program", "source obligation must identify its authority");
        return YVEX_ERR_FORMAT;
    }
    return YVEX_OK;
}

static const yvex_ir_dialect *mamba_program_dialect(void)
{
    static const yvex_ir_attribute_rule mixer[] = {
        {"heads", YVEX_IR_ATTR_U64, 1}, {"head_dimension", YVEX_IR_ATTR_U64, 1},
        {"state_dimension", YVEX_IR_ATTR_U64, 1}, {"groups", YVEX_IR_ATTR_U64, 1},
        {"convolution_kernel", YVEX_IR_ATTR_U64, 1}, {"normalization_groups", YVEX_IR_ATTR_U64, 1},
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"time_step_minimum", YVEX_IR_ATTR_F64, 1},
        {"time_step_maximum", YVEX_IR_ATTR_F64, 1}, {"time_step_unbounded", YVEX_IR_ATTR_BOOL, 1},
        {"norm_before_gate", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule obligation[] = {
        {"authority", YVEX_IR_ATTR_SYMBOL, 1}, {"conflict", YVEX_IR_ATTR_BOOL, 1}};
    /* This operation names the Mamba2 mixer computation, including source-weight
     * roles, selective SSD, causal convolution and gated normalization. It does
     * not own allocation, session lifetime, dispatch or numerical qualification. */
    static const yvex_ir_operation_definition operations[] = {
        {"mamba2.mixer", 1u, 11u, 11u, 3u, 3u, 0u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         mixer, 11u, 0, mamba_program_mixer_verify},
        {"mamba2.source_obligation", 1u, 0u, 0u, 0u, 0u, 0u, YVEX_IR_ORDERED,
         obligation, 2u, 0, mamba_obligation_verify}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}

typedef struct {
    yvex_ir_module *module;
    const yvex_mamba2_architecture *architecture;
    const char *source_identity;
    yvex_ir_id block, tensor, parameters[12], convolution, recurrent;
    int rc;
    yvex_error *error;
} mamba_program_builder;

static yvex_ir_id mamba_program_emit(mamba_program_builder *b, const char *operation,
    const yvex_ir_id *operands, size_t operand_count, const yvex_ir_id *types, size_t result_count,
    const yvex_ir_attribute *attributes, size_t attribute_count)
{
    yvex_ir_id op = YVEX_IR_NONE;
    yvex_ir_operation_request request = {.operation = operation, .operands = operands,
        .operand_count = operand_count, .result_types = types, .result_count = result_count,
        .attributes = attributes, .attribute_count = attribute_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &request, &op, b->error);
    return op;
}

static yvex_ir_id mamba_program_value(mamba_program_builder *b, yvex_ir_id op, uint32_t result)
{
    const yvex_ir_operation *value = yvex_ir_operation_at(b->module, op);
    return value && result < value->result_count ? value->results[result] : YVEX_IR_NONE;
}

static yvex_ir_id mamba_program_parameter(mamba_program_builder *b, const char *name, yvex_ir_id type)
{
    yvex_ir_attribute attributes[2] = {
        {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}, {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    yvex_ir_id op;
    yvex_core_text_copy(attributes[0].value.text, sizeof(attributes[0].value.text), name);
    yvex_core_text_copy(attributes[1].value.text, sizeof(attributes[1].value.text),
                        b->source_identity);
    op = mamba_program_emit(b, "core.parameter", NULL, 0u, &type, 1u, attributes, 2u);
    return mamba_program_value(b, op, 0u);
}

static yvex_ir_id mamba_program_norm(mamba_program_builder *b, yvex_ir_id input, const char *weight)
{
    yvex_ir_attribute attributes[2] = {
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64}, {.name = "weight_offset", .kind = YVEX_IR_ATTR_F64}};
    yvex_ir_id operands[2], op;
    attributes[0].value.real = b->architecture->normalization_epsilon;
    operands[0] = input;
    operands[1] = mamba_program_parameter(b, weight, b->parameters[1]);
    op = mamba_program_emit(b, "nn.rms_norm", operands, 2u, &b->tensor, 1u, attributes, 2u);
    return mamba_program_value(b, op, 0u);
}

static int mamba_program_types(mamba_program_builder *b, yvex_ir_id *tokens)
{
    const yvex_mamba2_architecture *a = b->architecture;
    const yvex_selective_ssd_requirement *r = &a->mixer.requirement;
    const unsigned long long shapes[12][3] = {
        {a->vocabulary_size, a->hidden_size, 0u}, {a->hidden_size, 0u, 0u},
        {a->mixer.projection_width, a->hidden_size, 0u}, {a->mixer.convolution_width, 1u, r->convolution_kernel},
        {a->mixer.convolution_width, 0u, 0u}, {r->heads, 0u, 0u}, {r->heads, 0u, 0u}, {r->heads, 0u, 0u},
        {a->mixer.width, 0u, 0u}, {a->hidden_size, a->mixer.width, 0u},
        {a->mixer.convolution_width, r->convolution_kernel, 0u}, {r->heads, r->head_dimension, r->state_dimension}};
    yvex_ir_dimension sequence = {.name = "sequence", .minimum = 1u, .maximum = 1048576u, .multiple = 1u};
    yvex_ir_type type = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_INDEX, .rank = 1u};
    yvex_ir_id symbol;
    uint32_t index, dim;
    int rc = yvex_ir_dimension_add(b->module, &sequence, &symbol, b->error);
    type.shape[0] = (yvex_ir_extent){symbol, 0u};
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(b->module, &type, tokens, b->error);
    type.scalar = YVEX_IR_F32;
    type.rank = 2u;
    type.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, a->hidden_size};
    if (rc == YVEX_OK) rc = yvex_ir_type_intern(b->module, &type, &b->tensor, b->error);
    for (index = 0u; rc == YVEX_OK && index < 12u; ++index) {
        type = (yvex_ir_type){.kind = index < 10u ? YVEX_IR_TENSOR : YVEX_IR_STATE,
            .scalar = YVEX_IR_F32, .rank = shapes[index][2] ? 3u : shapes[index][1] ? 2u : 1u};
        for (dim = 0u; dim < type.rank; ++dim) type.shape[dim] = (yvex_ir_extent){YVEX_IR_NONE, shapes[index][dim]};
        if (index >= 10u)
            yvex_core_text_copy(type.domain, sizeof(type.domain),
                                index == 10u ? "convolution.causal" : "ssm.selective");
        rc = yvex_ir_type_intern(b->module, &type, &b->parameters[index], b->error);
    }
    b->convolution = b->parameters[10];
    b->recurrent = b->parameters[11];
    return rc;
}

static yvex_ir_id mamba_program_layer(mamba_program_builder *b, unsigned long long layer,
    yvex_ir_id input, yvex_ir_id *convolution, yvex_ir_id *recurrent)
{
    static const char *const suffix[] = {"in_proj.weight", "conv1d.weight", "conv1d.bias",
        "A_log", "D", "dt_bias", "norm.weight", "out_proj.weight"};
    const yvex_selective_ssd_requirement *r = &b->architecture->mixer.requirement;
    yvex_ir_attribute attributes[] = {
        {.name = "heads", .kind = YVEX_IR_ATTR_U64, .value.integer = r->heads},
        {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = r->head_dimension},
        {.name = "state_dimension", .kind = YVEX_IR_ATTR_U64, .value.integer = r->state_dimension},
        {.name = "groups", .kind = YVEX_IR_ATTR_U64, .value.integer = r->groups},
        {.name = "convolution_kernel", .kind = YVEX_IR_ATTR_U64, .value.integer = r->convolution_kernel},
        {.name = "normalization_groups", .kind = YVEX_IR_ATTR_U64, .value.integer = r->normalization_groups},
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = r->normalization_epsilon},
        {.name = "time_step_minimum", .kind = YVEX_IR_ATTR_F64, .value.real = r->time_step_minimum},
        {.name = "time_step_maximum", .kind = YVEX_IR_ATTR_F64, .value.real = r->time_step_maximum},
        {.name = "time_step_unbounded", .kind = YVEX_IR_ATTR_BOOL, .value.integer = (uint64_t)r->time_step_unbounded},
        {.name = "norm_before_gate", .kind = YVEX_IR_ATTR_BOOL, .value.integer = (uint64_t)r->norm_before_gate}};
    yvex_ir_id operands[11], types[] = {b->tensor, b->convolution, b->recurrent}, op, residual[2];
    char name[96];
    uint32_t index;
    snprintf(name, sizeof(name), "backbone.layers.%llu.norm.weight", layer);
    operands[0] = mamba_program_norm(b, input, name);
    for (index = 0u; index < 8u; ++index) {
        snprintf(name, sizeof(name), "backbone.layers.%llu.mixer.%s", layer, suffix[index]);
        operands[index + 1u] = mamba_program_parameter(b, name, b->parameters[index + 2u]);
    }
    operands[9] = *convolution;
    operands[10] = *recurrent;
    op = mamba_program_emit(b, "mamba2.mixer", operands, 11u, types, 3u,
                            attributes, sizeof(attributes) / sizeof(attributes[0]));
    *convolution = mamba_program_value(b, op, 1u);
    *recurrent = mamba_program_value(b, op, 2u);
    residual[0] = input;
    residual[1] = mamba_program_value(b, op, 0u);
    return mamba_program_value(b, mamba_program_emit(b, "tensor.add", residual, 2u,
                                                     &b->tensor, 1u, NULL, 0u), 0u);
}

int yvex_mamba2_program_build(yvex_ir_module **out, const yvex_mamba2_architecture *a,
                               const char *source_identity, yvex_error *err)
{
    yvex_ir_dialect dialects[3] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *mamba_program_dialect()};
    mamba_program_builder builder = {.architecture = a, .source_identity = source_identity, .error = err};
    yvex_ir_id *arguments = NULL, *results = NULL, *outputs = NULL, tokens, function, hidden, op;
    size_t count, layer;
    yvex_ir_type output_type;
    if (out) *out = NULL;
    if (!out || !a || !a->architecture_complete || !a->hidden_size || !a->vocabulary_size ||
        !a->layer_count || a->layer_count > YVEX_MAMBA2_LAYER_CAP ||
        !yvex_sha256_hex_valid(a->architecture_identity) ||
        !yvex_sha256_hex_valid(source_identity) ||
        yvex_selective_ssd_geometry_validate(&a->mixer, err) != YVEX_OK) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "mamba2.program", "source-authoritative geometry is required");
        return YVEX_ERR_FORMAT;
    }
    count = 1u + 2u * (size_t)a->layer_count;
    arguments = calloc(count, sizeof(*arguments));
    results = calloc(count, sizeof(*results));
    outputs = calloc(count, sizeof(*outputs));
    builder.rc = arguments && results && outputs ? YVEX_OK : YVEX_ERR_NOMEM;
    if (builder.rc != YVEX_OK)
        yvex_error_set(err, YVEX_ERR_NOMEM, "mamba2.program", "program signature allocation failed");
    if (builder.rc == YVEX_OK)
        builder.rc = yvex_ir_module_open(&builder.module, "mamba2", source_identity, dialects, 3u, err);
    if (builder.rc == YVEX_OK) builder.rc = mamba_program_types(&builder, &tokens);
    if (builder.rc != YVEX_OK) goto done;
    arguments[0] = tokens;
    output_type = *yvex_ir_type_at(builder.module, builder.tensor);
    output_type.shape[1].extent = a->vocabulary_size;
    builder.rc = yvex_ir_type_intern(builder.module, &output_type, &results[0], err);
    for (layer = 0u; layer < a->layer_count; ++layer) {
        arguments[1u + 2u * layer] = results[1u + 2u * layer] = builder.convolution;
        arguments[2u + 2u * layer] = results[2u + 2u * layer] = builder.recurrent;
    }
    if (builder.rc == YVEX_OK)
        builder.rc = yvex_ir_function_add(builder.module, "forward", arguments, count, results, count,
                                           YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE | YVEX_IR_ORDERED, &function, err);
    if (builder.rc != YVEX_OK) goto done;
    builder.block = yvex_ir_function_at(builder.module, function)->body;
    memcpy(outputs, yvex_ir_block_at(builder.module, builder.block)->arguments, count * sizeof(*outputs));
    {
        yvex_ir_attribute obligation[] = {
            {.name = "authority", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "source.normalization"},
            {.name = "conflict", .kind = YVEX_IR_ATTR_BOOL,
             .value.integer = (uint64_t)a->normalization_policy_conflict}};
        mamba_program_emit(&builder, "mamba2.source_obligation", NULL, 0u, NULL, 0u, obligation, 2u);
        yvex_core_text_copy(obligation[0].value.text, sizeof(obligation[0].value.text), "source.tokenizer");
        obligation[1].value.integer = (uint64_t)a->token_policy_conflict;
        mamba_program_emit(&builder, "mamba2.source_obligation", NULL, 0u, NULL, 0u, obligation, 2u);
    }
    {
        yvex_ir_id inputs[] = {outputs[0], mamba_program_parameter(&builder,
                                     "backbone.embeddings.weight", builder.parameters[0])};
        op = mamba_program_emit(&builder, "nn.embedding", inputs, 2u, &builder.tensor, 1u, NULL, 0u);
        hidden = mamba_program_value(&builder, op, 0u);
    }
    for (layer = 0u; builder.rc == YVEX_OK && layer < a->layer_count; ++layer)
        hidden = mamba_program_layer(&builder, layer, hidden, &outputs[1u + 2u * layer], &outputs[2u + 2u * layer]);
    hidden = mamba_program_norm(&builder, hidden, "backbone.norm_f.weight");
    {
        yvex_ir_id inputs[] = {hidden, mamba_program_parameter(&builder, "lm_head.weight", builder.parameters[0])};
        op = mamba_program_emit(&builder, "nn.linear", inputs, 2u, &results[0], 1u, NULL, 0u);
        outputs[0] = mamba_program_value(&builder, op, 0u);
    }
    mamba_program_emit(&builder, "core.return", outputs, count, NULL, 0u, NULL, 0u);
    if (builder.rc == YVEX_OK) builder.rc = yvex_ir_seal(builder.module, err);
    if (builder.rc == YVEX_OK) {
        yvex_ir_pass passes[] = {*yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
        builder.rc = yvex_ir_pass_pipeline(builder.module, passes, 2u, out, NULL, err);
    }
done:
    yvex_ir_module_close(&builder.module);
    free(outputs);
    free(results);
    free(arguments);
    return builder.rc;
}

typedef enum { MAMBA_U64, MAMBA_BOOL, MAMBA_DOUBLE, MAMBA_TEXT } mamba_field_kind;
typedef struct {
    const char *name;
    mamba_field_kind kind;
    size_t offset;
} mamba_field;

typedef struct {
    unsigned long long hidden_size, layers, vocabulary, expansion, heads, head_dimension;
    unsigned long long state_dimension, groups, kernel, chunk;
    unsigned long long bos, eos, pad;
    double epsilon;
    int rms_norm, residual_f32, tied, bias, conv_bias, norm_before_gate;
    char model_type[64], activation[64], dtype[64];
} mamba_config;

static int mamba_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "mamba2.source", reason);
    return status;
}

static int mamba_double(yvex_json *json, double *value)
{
    char text[64], *end;
    if (!yvex_json_number_text(json, text, sizeof(text))) return 0;
    errno = 0;
    *value = strtod(text, &end);
    return !errno && !*end && isfinite(*value);
}

static int mamba_field_read(yvex_json *json, const mamba_field *field, void *object)
{
    unsigned char *destination = (unsigned char *)object + field->offset;
    switch (field->kind) {
    case MAMBA_U64: return yvex_json_u64(json, (unsigned long long *)(void *)destination);
    case MAMBA_BOOL: return yvex_json_bool(json, (int *)(void *)destination);
    case MAMBA_DOUBLE: return mamba_double(json, (double *)(void *)destination);
    case MAMBA_TEXT: return yvex_json_string(json, (char *)destination, 64u);
    }
    return 0;
}

static int mamba_bound(yvex_json *json, yvex_selective_ssd_requirement *r)
{
    yvex_json_iter array;
    if (!yvex_json_iter_begin(json, &array, YVEX_JSON_COLLECTION_ARRAY) ||
        yvex_json_array_value(&array) != YVEX_JSON_ITEM_READY ||
        !mamba_double(json, &r->time_step_minimum) ||
        yvex_json_array_value(&array) != YVEX_JSON_ITEM_READY) return 0;
    yvex_json_space(json);
    if ((size_t)(json->end - json->cursor) >= 8u && !memcmp(json->cursor, "Infinity", 8u)) {
        json->cursor += 8u;
        r->time_step_unbounded = 1;
    } else if (!mamba_double(json, &r->time_step_maximum)) return 0;
    return yvex_json_array_value(&array) == YVEX_JSON_ITEM_END && !array.trailing_separator;
}

static int mamba_architecture_array(yvex_json *json)
{
    char name[64];
    yvex_json_iter array;
    return yvex_json_iter_begin(json, &array, YVEX_JSON_COLLECTION_ARRAY) &&
        yvex_json_array_value(&array) == YVEX_JSON_ITEM_READY &&
        yvex_json_string(json, name, sizeof(name)) && !strcmp(name, "Mamba2ForCausalLM") &&
        yvex_json_array_value(&array) == YVEX_JSON_ITEM_END && !array.trailing_separator;
}

#define MAMBA_FIELD(name_, kind_, member_) {name_, kind_, offsetof(mamba_config, member_)}
static const mamba_field mamba_config_fields[] = {
    MAMBA_FIELD("hidden_size", MAMBA_U64, hidden_size),
    MAMBA_FIELD("num_hidden_layers", MAMBA_U64, layers),
    MAMBA_FIELD("vocab_size", MAMBA_U64, vocabulary),
    MAMBA_FIELD("expand", MAMBA_U64, expansion),
    MAMBA_FIELD("num_heads", MAMBA_U64, heads),
    MAMBA_FIELD("head_dim", MAMBA_U64, head_dimension),
    MAMBA_FIELD("state_size", MAMBA_U64, state_dimension),
    MAMBA_FIELD("n_groups", MAMBA_U64, groups),
    MAMBA_FIELD("conv_kernel", MAMBA_U64, kernel),
    MAMBA_FIELD("chunk_size", MAMBA_U64, chunk),
    MAMBA_FIELD("bos_token_id", MAMBA_U64, bos),
    MAMBA_FIELD("eos_token_id", MAMBA_U64, eos),
    MAMBA_FIELD("pad_token_id", MAMBA_U64, pad),
    MAMBA_FIELD("layer_norm_epsilon", MAMBA_DOUBLE, epsilon),
    MAMBA_FIELD("rms_norm", MAMBA_BOOL, rms_norm),
    MAMBA_FIELD("residual_in_fp32", MAMBA_BOOL, residual_f32),
    MAMBA_FIELD("tie_word_embeddings", MAMBA_BOOL, tied),
    MAMBA_FIELD("use_bias", MAMBA_BOOL, bias),
    MAMBA_FIELD("use_conv_bias", MAMBA_BOOL, conv_bias),
    MAMBA_FIELD("norm_before_gate", MAMBA_BOOL, norm_before_gate),
    MAMBA_FIELD("model_type", MAMBA_TEXT, model_type),
    MAMBA_FIELD("hidden_act", MAMBA_TEXT, activation),
    MAMBA_FIELD("torch_dtype", MAMBA_TEXT, dtype)
};
#undef MAMBA_FIELD

static int mamba_config_parse(const char *data, size_t length, mamba_config *config,
                              yvex_selective_ssd_requirement *requirement)
{
    yvex_json json;
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP];
    unsigned long long seen = 0;
    int architecture = 0, bound = 0;
    size_t index, count = sizeof(mamba_config_fields) / sizeof(mamba_config_fields[0]);

    yvex_json_init(&json, data, length);
    if (!yvex_json_iter_begin(&json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        for (index = 0; index < count; ++index)
            if (!strcmp(key, mamba_config_fields[index].name)) break;
        if (index < count) {
            if ((seen & (1ull << index)) ||
                !mamba_field_read(&json, &mamba_config_fields[index], config)) return 0;
            seen |= 1ull << index;
        } else if (!strcmp(key, "architectures")) {
            if (architecture++ || !mamba_architecture_array(&json)) return 0;
        } else if (!strcmp(key, "time_step_limit")) {
            if (bound++ || !mamba_bound(&json, requirement)) return 0;
        } else if (!strcmp(key, "attn_layer_idx") || !strcmp(key, "attention_layers") ||
                   !strcmp(key, "layer_types")) {
            /* A topology-bearing hybrid declaration is not pure Mamba2. */
            return 0;
        } else if (!yvex_json_skip_value(&json)) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator &&
        yvex_json_complete(&json) && seen == (1ull << count) - 1u && architecture == 1 && bound == 1;
}

static int mamba_params_parse(const char *data, size_t length, const mamba_config *config)
{
    const char *const names[] = {"dim", "n_layers", "vocab_size", "n_groups"};
    const unsigned long long expected[] = {
        config->hidden_size, config->layers, config->vocabulary, config->groups};
    const char *const bool_names[] = {"rms_norm", "residual_in_fp32", "tie_embeddings"};
    const int bool_expected[] = {config->rms_norm, config->residual_f32, config->tied};
    yvex_json json;
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP], type[32];
    unsigned int seen = 0, index;

    yvex_json_init(&json, data, length);
    if (!yvex_json_iter_begin(&json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        for (index = 0; index < 4u; ++index) if (!strcmp(key, names[index])) break;
        if (index < 4u) {
            unsigned long long value;
            if ((seen & (1u << index)) || !yvex_json_u64(&json, &value) ||
                value != expected[index]) return 0;
            seen |= 1u << index;
            continue;
        }
        for (index = 0; index < 3u; ++index) if (!strcmp(key, bool_names[index])) break;
        if (index < 3u) {
            int value;
            if ((seen & (1u << (index + 4u))) || !yvex_json_bool(&json, &value) ||
                value != bool_expected[index]) return 0;
            seen |= 1u << (index + 4u);
        } else if (!strcmp(key, "model_type")) {
            if ((seen & 128u) || !yvex_json_string(&json, type, sizeof(type)) ||
                strcmp(type, "mamba")) return 0;
            seen |= 128u;
        } else if (!yvex_json_skip_value(&json)) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator &&
        yvex_json_complete(&json) && seen == 255u;
}

static int mamba_generation_parse(const char *data, size_t length, yvex_mamba2_architecture *a)
{
    const char *const names[] = {"bos_token_id", "eos_token_id", "pad_token_id"};
    unsigned long long *const values[] = {&a->generation_bos, &a->generation_eos, &a->generation_pad};
    yvex_json json;
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP];
    unsigned int seen = 0, index;
    yvex_json_init(&json, data, length);
    if (!yvex_json_iter_begin(&json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        for (index = 0; index < 3u; ++index) if (!strcmp(key, names[index])) break;
        if (index < 3u) {
            if ((seen & (1u << index)) || !yvex_json_u64(&json, values[index]) ||
                *values[index] >= a->vocabulary_size) return 0;
            seen |= 1u << index;
        } else if (!yvex_json_skip_value(&json)) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator &&
        yvex_json_complete(&json) && seen == 7u;
}

static int mamba_vocab(yvex_json *json, yvex_mamba2_architecture *a)
{
    yvex_json_iter object;
    yvex_json_item item;
    char token[YVEX_JSON_KEY_CAP];
    unsigned char *ids = calloc((size_t)a->vocabulary_size, 1u);
    unsigned long long count = 0, id;
    int seen = 0, valid = 0;
    if (!ids || !yvex_json_iter_begin(json, &object, YVEX_JSON_COLLECTION_OBJECT)) goto cleanup;
    while ((item = yvex_json_object_member(&object, token, sizeof(token))) == YVEX_JSON_ITEM_READY) {
        if (!yvex_json_u64(json, &id) || id >= a->vocabulary_size || ids[id]) goto cleanup;
        ids[id] = 1u;
        count++;
        if (!strcmp(token, "<s>")) { a->tokenizer_bos = id; seen |= 1; }
        if (!strcmp(token, "</s>")) { a->tokenizer_eos = id; seen |= 2; }
        if (!strcmp(token, "<unk>")) { a->tokenizer_unk = id; seen |= 4; }
    }
    valid = item == YVEX_JSON_ITEM_END && !object.trailing_separator &&
        count == a->vocabulary_size && seen == 7;
cleanup:
    free(ids);
    return valid;
}

static int mamba_tokenizer_model(yvex_json *json, yvex_mamba2_architecture *a)
{
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP], type[32];
    int seen = 0;
    if (!yvex_json_iter_begin(json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        if (!strcmp(key, "vocab")) {
            if ((seen & 1) || !mamba_vocab(json, a)) return 0;
            seen |= 1;
        } else if (!strcmp(key, "type")) {
            if ((seen & 2) || !yvex_json_string(json, type, sizeof(type)) || strcmp(type, "BPE")) return 0;
            seen |= 2;
        } else if (!yvex_json_skip_value(json)) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator && seen == 3;
}

static int mamba_tokenizer_parse(const char *data, size_t length, yvex_mamba2_architecture *a)
{
    yvex_json json;
    yvex_json_iter object;
    yvex_json_item item;
    char key[YVEX_JSON_KEY_CAP];
    int seen = 0;
    yvex_json_init(&json, data, length);
    if (!yvex_json_iter_begin(&json, &object, YVEX_JSON_COLLECTION_OBJECT)) return 0;
    while ((item = yvex_json_object_member(&object, key, sizeof(key))) == YVEX_JSON_ITEM_READY) {
        if (!strcmp(key, "model")) {
            if (seen++ || !mamba_tokenizer_model(&json, a)) return 0;
        } else if (!yvex_json_skip_value(&json)) return 0;
    }
    return item == YVEX_JSON_ITEM_END && !object.trailing_separator &&
        yvex_json_complete(&json) && seen == 1;
}

static int mamba_identity(yvex_mamba2_architecture *a)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    const unsigned long long values[] = {
        a->hidden_size, a->layer_count, a->vocabulary_size, a->expansion, a->chunk_size,
        a->config_bos, a->config_eos, a->config_pad, a->generation_bos, a->generation_eos,
        a->generation_pad, a->tokenizer_bos, a->tokenizer_eos, a->tokenizer_unk,
        (unsigned long long)a->residual_in_f32, (unsigned long long)a->declared_norm_before_gate};
    size_t i;
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.mamba2.source-topology.v1") ||
        !yvex_sha256_update_text(&hash, a->source_revision) ||
        !yvex_sha256_update_text(&hash, a->mixer.identity)) return 0;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
        if (!yvex_sha256_update_u64(&hash, values[i])) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, a->architecture_identity);
    return 1;
}

static int mamba_open(const yvex_source_verification *verification,
                      yvex_mamba2_architecture *a, yvex_error *err)
{
    const char *const files[] = {"config.json", "params.json", "generation_config.json", "tokenizer.json"};
    yvex_source_metadata_blob blobs[4] = {0};
    const char *data[4] = {0};
    size_t lengths[4] = {0};
    mamba_config config = {0};
    yvex_selective_ssd_requirement r = {.schema_version = YVEX_SELECTIVE_SSD_SCHEMA_V1};
    unsigned long long width;
    unsigned int i;
    int rc = YVEX_ERR_FORMAT;
    if (a) memset(a, 0, sizeof(*a));
    if (!a || !verification || !verification->verified || !verification->config_valid)
        return mamba_refuse(err, YVEX_ERR_STATE, "verified source is required before Mamba2 interpretation");
    if (strcmp(verification->repository_id, YVEX_MAMBA2_REPOSITORY) ||
        strcmp(verification->revision, YVEX_MAMBA2_REVISION))
        return mamba_refuse(err, YVEX_ERR_FORMAT, "source is not the acquired immutable reference revision");
    for (i = 0; i < 4u; ++i) {
        if (yvex_source_provenance_metadata_read(verification, files[i],
                i == 3u ? 8u * 1024u * 1024u : 65536u, &blobs[i], err) != YVEX_OK)
            goto cleanup;
        data[i] = (const char *)blobs[i].bytes;
        lengths[i] = blobs[i].byte_count;
    }
    if (!mamba_config_parse(data[0], lengths[0], &config, &r) ||
        strcmp(config.model_type, "mamba2") || strcmp(config.activation, "silu") ||
        strcmp(config.dtype, "bfloat16") || !config.rms_norm || !config.residual_f32 ||
        config.tied || config.bias || !config.conv_bias || !config.hidden_size ||
        !config.layers || config.layers > YVEX_MAMBA2_LAYER_CAP || !config.vocabulary ||
        config.vocabulary > 1048576u || !config.chunk || config.epsilon <= 0.0 ||
        config.bos >= config.vocabulary || config.eos >= config.vocabulary ||
        config.pad >= config.vocabulary ||
        !yvex_core_u64_mul(config.hidden_size, config.expansion, &width) ||
        !mamba_params_parse(data[1], lengths[1], &config)) goto cleanup;
    a->hidden_size = config.hidden_size;
    a->layer_count = config.layers;
    a->vocabulary_size = config.vocabulary;
    a->expansion = config.expansion;
    a->chunk_size = config.chunk;
    a->normalization_epsilon = config.epsilon;
    a->residual_in_f32 = config.residual_f32;
    a->declared_norm_before_gate = config.norm_before_gate;
    a->config_bos = config.bos; a->config_eos = config.eos; a->config_pad = config.pad;
    r.heads = config.heads; r.head_dimension = config.head_dimension;
    r.state_dimension = config.state_dimension; r.groups = config.groups;
    r.convolution_kernel = config.kernel; r.normalization_epsilon = config.epsilon;
    /* Descriptive candidate: Mistral's params.json recipe uses Mamba2's grouped gated
     * normalization defaults. Preserve conflicting HF declarations explicitly; this
     * source inspection does not resolve them into an executable numeric contract. */
    r.normalization_groups = config.groups;
    r.norm_before_gate = 0;
    if (yvex_selective_ssd_geometry_seal(&a->mixer, &r, err) != YVEX_OK ||
        a->mixer.width != width || !mamba_generation_parse(data[2], lengths[2], a) ||
        !mamba_tokenizer_parse(data[3], lengths[3], a)) goto cleanup;
    a->token_policy_conflict = a->config_bos != a->tokenizer_bos ||
        a->generation_bos != a->tokenizer_bos || a->config_eos != a->tokenizer_eos ||
        a->generation_eos != a->tokenizer_eos;
    a->normalization_policy_conflict = config.norm_before_gate != r.norm_before_gate;
    yvex_core_text_copy(a->source_revision, sizeof(a->source_revision), verification->revision);
    if (!mamba_identity(a)) goto cleanup;
    a->architecture_complete = 1;
    rc = YVEX_OK;
cleanup:
    for (i = 0; i < 4u; ++i) yvex_source_metadata_blob_release(&blobs[i]);
    if (rc != YVEX_OK) {
        memset(a, 0, sizeof(*a));
        return mamba_refuse(err, rc, "Mamba2 configuration, tokenizer or cross-source geometry is invalid");
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static const char *const mamba_role_names[YVEX_MAMBA2_ROLE_COUNT] = {
    "unknown", "token_embedding", "final_norm", "lm_head", "block_norm",
    "mixer_input_projection", "causal_convolution_weight", "causal_convolution_bias",
    "ssm_decay_log", "ssm_skip", "ssm_time_bias", "gated_mixer_norm", "mixer_output_projection"
};

static const char *mamba_role_name(yvex_mamba2_role role)
{
    return role > YVEX_MAMBA2_ROLE_UNKNOWN && role < YVEX_MAMBA2_ROLE_COUNT
        ? mamba_role_names[role] : "unknown";
}

static yvex_mamba2_role mamba_role(const char *name, unsigned long long *layer)
{
    const char *const global[] = {"backbone.embeddings.weight", "backbone.norm_f.weight", "lm_head.weight"};
    const char *const suffix[] = {"norm.weight", "mixer.in_proj.weight", "mixer.conv1d.weight",
        "mixer.conv1d.bias", "mixer.A_log", "mixer.D", "mixer.dt_bias", "mixer.norm.weight",
        "mixer.out_proj.weight"};
    char *end;
    const char *number;
    unsigned int i;
    *layer = ~0ull;
    for (i = 0; i < 3u; ++i) if (!strcmp(name, global[i])) return (yvex_mamba2_role)(i + 1u);
    if (strncmp(name, "backbone.layers.", 16u)) return YVEX_MAMBA2_ROLE_UNKNOWN;
    number = name + 16u;
    if (*number < '0' || *number > '9' || (*number == '0' && number[1] != '.'))
        return YVEX_MAMBA2_ROLE_UNKNOWN;
    errno = 0;
    *layer = strtoull(number, &end, 10);
    if (errno || *end != '.') return YVEX_MAMBA2_ROLE_UNKNOWN;
    for (i = 0; i < 9u; ++i)
        if (!strcmp(end + 1u, suffix[i])) return (yvex_mamba2_role)(i + 4u);
    return YVEX_MAMBA2_ROLE_UNKNOWN;
}

static int mamba_tensor_classify(const yvex_mamba2_architecture *a,
    const yvex_native_weight_info *tensor, yvex_mamba2_tensor_binding *binding, yvex_error *err)
{
    unsigned long long dimensions[3] = {0}, elements = 1u, bytes;
    unsigned int rank = 1u, index;
    if (!a || !tensor || !tensor->name || !binding || !a->architecture_complete ||
        !yvex_sha256_hex_valid(a->architecture_identity))
        return mamba_refuse(err, YVEX_ERR_INVALID_ARG, "sealed architecture and tensor are required");
    binding->role = mamba_role(tensor->name, &binding->layer_index);
    if (binding->role == YVEX_MAMBA2_ROLE_UNKNOWN ||
        (binding->layer_index != ~0ull && binding->layer_index >= a->layer_count) ||
        tensor->dtype != YVEX_NATIVE_DTYPE_BF16)
        return mamba_refuse(err, YVEX_ERR_FORMAT, "unexpected tensor role, layer or dtype");
    switch (binding->role) {
    case YVEX_MAMBA2_ROLE_EMBEDDING:
    case YVEX_MAMBA2_ROLE_LM_HEAD:
        dimensions[0] = a->vocabulary_size; dimensions[1] = a->hidden_size; rank = 2u; break;
    case YVEX_MAMBA2_ROLE_FINAL_NORM:
    case YVEX_MAMBA2_ROLE_BLOCK_NORM: dimensions[0] = a->hidden_size; break;
    case YVEX_MAMBA2_ROLE_INPUT_PROJECTION:
        dimensions[0] = a->mixer.projection_width; dimensions[1] = a->hidden_size; rank = 2u; break;
    case YVEX_MAMBA2_ROLE_CONVOLUTION_WEIGHT:
        dimensions[0] = a->mixer.convolution_width; dimensions[1] = 1u;
        dimensions[2] = a->mixer.requirement.convolution_kernel; rank = 3u; break;
    case YVEX_MAMBA2_ROLE_CONVOLUTION_BIAS: dimensions[0] = a->mixer.convolution_width; break;
    case YVEX_MAMBA2_ROLE_DECAY_LOG:
    case YVEX_MAMBA2_ROLE_SKIP:
    case YVEX_MAMBA2_ROLE_TIME_BIAS: dimensions[0] = a->mixer.requirement.heads; break;
    case YVEX_MAMBA2_ROLE_GATED_NORM: dimensions[0] = a->mixer.width; break;
    case YVEX_MAMBA2_ROLE_OUTPUT_PROJECTION:
        dimensions[0] = a->hidden_size; dimensions[1] = a->mixer.width; rank = 2u; break;
    default: return mamba_refuse(err, YVEX_ERR_FORMAT, "unmapped execution tensor");
    }
    if (tensor->rank != rank) return mamba_refuse(err, YVEX_ERR_FORMAT, "wrong Mamba2 tensor rank");
    for (index = 0; index < rank; ++index)
        if (tensor->dims[index] != dimensions[index] ||
            !yvex_core_u64_mul(elements, dimensions[index], &elements))
            return mamba_refuse(err, YVEX_ERR_FORMAT, "wrong Mamba2 tensor shape");
    if (!yvex_core_u64_mul(elements, 2u, &bytes) || tensor->data_bytes != bytes ||
        tensor->data_end < tensor->data_start || tensor->data_end - tensor->data_start != bytes)
        return mamba_refuse(err, YVEX_ERR_FORMAT, "wrong Mamba2 tensor byte range");
    return YVEX_OK;
}

static int mamba_audit(const yvex_mamba2_architecture *a, const yvex_native_weight_table *table,
    const yvex_source_tensor_snapshot *snapshot, yvex_mamba2_inventory *out, yvex_error *err)
{
    unsigned char seen[YVEX_MAMBA2_LAYER_CAP + 1u][YVEX_MAMBA2_ROLE_COUNT] = {{0}};
    yvex_source_tensor_snapshot_facts facts;
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long count, i, layer;
    unsigned int role;
    if (out) memset(out, 0, sizeof(*out));
    if (!a || !out || (!table && !snapshot) || !a->architecture_complete ||
        !a->layer_count || a->layer_count > YVEX_MAMBA2_LAYER_CAP)
        return mamba_refuse(err, YVEX_ERR_INVALID_ARG, "Mamba2 architecture and inventory are required");
    if (snapshot && yvex_source_tensor_snapshot_facts_get(snapshot, &facts, err) != YVEX_OK)
        return yvex_error_code(err);
    count = snapshot ? facts.tensor_count : yvex_native_weight_table_count(table);
    out->required_tensors = 3u + 9u * a->layer_count;
    if (count != out->required_tensors)
        return mamba_refuse(err, YVEX_ERR_FORMAT, "Mamba2 tensor population is incomplete or unexpected");
    yvex_sha256_init(&hash);
    yvex_sha256_update_text(&hash, "yvex.mamba2.role-coverage.v1");
    yvex_sha256_update_text(&hash, a->architecture_identity);
    for (i = 0; i < count; ++i) {
        const yvex_native_weight_info *tensor = snapshot ? yvex_source_tensor_snapshot_at(snapshot, i)
            : yvex_native_weight_table_at(table, i);
        yvex_mamba2_tensor_binding binding;
        if (mamba_tensor_classify(a, tensor, &binding, err) != YVEX_OK) return yvex_error_code(err);
        layer = binding.layer_index == ~0ull ? a->layer_count : binding.layer_index;
        if (seen[layer][binding.role]++) return mamba_refuse(err, YVEX_ERR_FORMAT, "duplicate Mamba2 role");
        out->role_counts[binding.role]++;
        if (!yvex_core_u64_add(out->tensor_bytes, tensor->data_bytes, &out->tensor_bytes))
            return mamba_refuse(err, YVEX_ERR_BOUNDS, "Mamba2 tensor bytes overflow");
    }
    /* Canonical role order makes the audit independent of shard/header enumeration order. */
    for (layer = 0; layer <= a->layer_count; ++layer)
        for (role = 1u; role < YVEX_MAMBA2_ROLE_COUNT; ++role) {
            int expected = layer == a->layer_count ? role <= YVEX_MAMBA2_ROLE_LM_HEAD
                : role >= YVEX_MAMBA2_ROLE_BLOCK_NORM;
            if (seen[layer][role] != expected)
                return mamba_refuse(err, YVEX_ERR_FORMAT, "missing required Mamba2 role");
            yvex_sha256_update_u64(&hash, seen[layer][role]);
        }
    yvex_sha256_final(&hash, digest);
    yvex_sha256_hex(digest, out->role_identity);
    out->tensors = count;
    out->complete = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int mamba_table_audit(const yvex_mamba2_architecture *a, const yvex_native_weight_table *table,
    yvex_mamba2_inventory *out, yvex_error *err)
{
    return mamba_audit(a, table, NULL, out, err);
}

static int mamba_snapshot_audit(const yvex_mamba2_architecture *a, const yvex_source_tensor_snapshot *snapshot,
    yvex_mamba2_inventory *out, yvex_error *err)
{
    return mamba_audit(a, NULL, snapshot, out, err);
}

const yvex_mamba2_api *yvex_model_register_mamba2(void)
{
    static const yvex_mamba2_api api = {
        .schema_version = 1u, .open = mamba_open, .tensor_classify = mamba_tensor_classify,
        .tensor_audit = mamba_table_audit, .snapshot_audit = mamba_snapshot_audit,
        .role_name = mamba_role_name};
    return &api;
}
