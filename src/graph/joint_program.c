/* Lower source-declared conditioned attention/FFN blocks into ordinary SSA.
 * No kernel, device or session semantics enter this compiler owner. */
#include <yvex/internal/joint_program.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const yvex_joint_program_recipe *recipe;
    const yvex_transformer_joint_recipe *architecture;
    yvex_joint_program *program;
    yvex_ir_module *module;
    yvex_ir_id block, hidden, attention, timestep, cosine, sine, indices;
    yvex_ir_id *results, *result_types;
    unsigned long long layer;
    unsigned long long rows, parameter_base, parameter_stride;
    int refiner;
    size_t result_count;
    yvex_error *err;
    int rc;
} joint_builder;

static yvex_ir_id joint_type(joint_builder *b, unsigned long long rows,
    unsigned long long width, yvex_ir_scalar scalar)
{
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = scalar, .rank = rows ? 2u : 1u,
        .shape = {{YVEX_IR_NONE, rows ? rows : width}, {YVEX_IR_NONE, rows ? width : 0u}}};
    yvex_ir_id id = YVEX_IR_NONE;
    if (!rows) memset(t.shape + 1u, 0, sizeof(t.shape[1]));
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &id, b->err);
    return id;
}

static yvex_ir_id joint_op(joint_builder *b, const char *name, const yvex_ir_id *args,
    size_t count, yvex_ir_id type, const yvex_ir_attribute *attrs, size_t attr_count)
{
    yvex_ir_id op;
    yvex_ir_operation_request r = {.operation = name, .operands = args, .operand_count = count,
        .result_types = &type, .result_count = 1u, .attributes = attrs, .attribute_count = attr_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_operation_at(b->module, op)->results[0] : YVEX_IR_NONE;
}

static yvex_ir_id joint_weight(joint_builder *b, unsigned long long ordinal,
    unsigned long long rows, unsigned long long width, yvex_ir_scalar scalar)
{
    yvex_ir_attribute a[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
        {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
    snprintf(a[0].value.text, sizeof(a[0].value.text), "weight_%llu", ordinal);
    yvex_core_text_copy(a[1].value.text, sizeof(a[1].value.text), b->recipe->source_identity);
    return joint_op(b, "core.parameter", NULL, 0u, joint_type(b, rows, width, scalar), a, 2u);
}

static yvex_ir_id joint_parameter(joint_builder *b, unsigned int slot,
    unsigned long long rows, unsigned long long width)
{
    return joint_weight(b, b->parameter_base + b->layer * b->parameter_stride + slot,
        rows, width, YVEX_IR_BF16);
}

static void joint_trace(joint_builder *b, yvex_transformer_joint_scope scope,
    unsigned long long block, yvex_transformer_joint_stage stage, yvex_ir_id value)
{
    const yvex_joint_program_recipe *r = b->recipe;
    int selected = stage == YVEX_TRANSFORMER_JOINT_STAGE_COUNT ? r->observe_blocks :
        r->observe_stages && r->observed_scope == scope && r->observed_block == block &&
        (r->observed_stage == YVEX_TRANSFORMER_JOINT_STAGE_COUNT || r->observed_stage == stage);
    if (!selected || b->rc != YVEX_OK) return;
    yvex_ir_attribute tag = {.name = "tag", .kind = YVEX_IR_ATTR_U64,
        .value.integer = ((unsigned long long)scope << 56u) | (block << 8u) | (unsigned int)stage};
    yvex_ir_operation_request request = {.operation = "core.observe", .operands = &value, .operand_count = 1u,
        .attributes = &tag, .attribute_count = 1u};
    yvex_ir_id op;
    b->rc = yvex_ir_operation_add(b->module, b->block, &request, &op, b->err);
}

static unsigned int joint_effects(const joint_builder *b)
{
    return b->recipe->observe_blocks || b->recipe->observe_stages ? YVEX_IR_PUBLISH | YVEX_IR_ORDERED : 0u;
}

static void joint_observe(joint_builder *b, yvex_transformer_joint_stage stage, yvex_ir_id value)
{
    joint_trace(b, b->refiner ? YVEX_TRANSFORMER_JOINT_SCOPE_REFINER : YVEX_TRANSFORMER_JOINT_SCOPE_OMNI,
        b->layer + 1u, stage, value);
    if (b->refiner || b->recipe->inspected_block != b->layer + 1u || b->rc != YVEX_OK) return;
    for (size_t i = 0u; i < b->program->stage_result_count; ++i)
        if (b->program->stages[i] == stage) b->results[1u + b->program->block_result_count + i] = value;
}

static yvex_ir_id joint_project(joint_builder *b, yvex_ir_id x, unsigned int slot,
    unsigned long long rows, unsigned long long in, unsigned long long out,
    yvex_transformer_joint_stage raw_stage)
{
    yvex_ir_id args[] = {x, joint_parameter(b, slot, out, in)};
    yvex_ir_id raw = joint_op(b, "nn.linear", args, 2u, joint_type(b, rows, out, YVEX_IR_F32), NULL, 0u);
    joint_observe(b, raw_stage, raw);
    return joint_op(b, "tensor.cast", &raw, 1u, joint_type(b, rows, out, YVEX_IR_BF16), NULL, 0u);
}

static yvex_ir_id joint_norm(joint_builder *b, yvex_ir_id x, unsigned int slot,
    unsigned long long width, yvex_ir_id type)
{
    yvex_ir_id args[] = {x, joint_parameter(b, slot, 0u, width)};
    yvex_ir_attribute epsilon = {.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
        .value.real = b->recipe->normalization_epsilon};
    return joint_op(b, "nn.group_rms_norm", args, 2u, type, &epsilon, 1u);
}

static yvex_ir_id joint_modulation(joint_builder *b)
{
    const yvex_transformer_joint_recipe *r = b->architecture;
    unsigned long long width = r->hidden_width * r->modulation_parameters * r->modality_count;
    yvex_ir_id activated = joint_op(b, "nn.silu", &b->timestep, 1u,
        joint_type(b, b->recipe->timesteps, r->timestep_width, YVEX_IR_F32), NULL, 0u);
    activated = joint_op(b, "tensor.cast", &activated, 1u,
        joint_type(b, b->recipe->timesteps, r->timestep_width, YVEX_IR_BF16), NULL, 0u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_CONDITION_ACTIVATION, activated);
    yvex_ir_id args[] = {activated, joint_parameter(b, 8u, width, r->timestep_width),
        joint_parameter(b, 9u, 0u, width)};
    yvex_ir_id table = joint_op(b, "nn.linear_bias", args, 3u,
        joint_type(b, b->recipe->timesteps, width, YVEX_IR_BF16), NULL, 0u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_MODULATION, table);
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 3u,
        .shape = {{YVEX_IR_NONE, b->recipe->timesteps * r->modality_count},
            {YVEX_IR_NONE, r->modulation_parameters}, {YVEX_IR_NONE, r->hidden_width}}};
    yvex_ir_id type;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &type, b->err);
    return b->rc == YVEX_OK ? joint_op(b, "tensor.reshape", &table, 1u, type, NULL, 0u) : YVEX_IR_NONE;
}

static yvex_ir_id joint_condition(joint_builder *b, yvex_ir_id x, yvex_ir_id table,
    yvex_ir_id update, unsigned int first)
{
    yvex_ir_id args[] = {x, table, b->indices, update};
    yvex_ir_attribute attrs[] = {{.name = "shift", .kind = YVEX_IR_ATTR_U64, .value.integer = first},
        {.name = "scale", .kind = YVEX_IR_ATTR_U64, .value.integer = first + 1u}};
    if (update != YVEX_IR_NONE) memcpy(attrs[0].name, "gate", sizeof("gate"));
    return joint_op(b, update == YVEX_IR_NONE ? "nn.indexed_modulate" : "nn.indexed_gated_residual",
        args, update == YVEX_IR_NONE ? 3u : 4u, b->hidden, attrs, update == YVEX_IR_NONE ? 2u : 1u);
}

static yvex_ir_id joint_attention(joint_builder *b, yvex_ir_id x)
{
    const yvex_transformer_joint_recipe *r = b->architecture;
    unsigned long long rows = b->rows;
    x = joint_project(b, x, 1u, rows, r->hidden_width, 3u * r->attention_width,
        YVEX_TRANSFORMER_JOINT_STAGE_QKV_F32);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_QKV_BF16, x);
    yvex_ir_attribute head = {.name = "head_dimension", .kind = YVEX_IR_ATTR_U64,
        .value.integer = r->head_dimension};
    yvex_ir_id types[] = {b->attention, b->attention, b->attention}, values[3], op;
    yvex_ir_operation_request split = {.operation = "tensor.split_interleaved_three", .operands = &x,
        .operand_count = 1u, .result_types = types, .result_count = 3u, .attributes = &head, .attribute_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &split, &op, b->err);
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    memcpy(values, yvex_ir_operation_at(b->module, op)->results, sizeof(values));
    for (size_t i = 0u; i < 3u; ++i)
        joint_observe(b, (yvex_transformer_joint_stage)(YVEX_TRANSFORMER_JOINT_STAGE_QUERY + i), values[i]);
    for (size_t i = 0u; i < 2u; ++i) {
        values[i] = joint_norm(b, values[i], (unsigned int)(2u + i), r->head_dimension, b->attention);
        joint_observe(b, (yvex_transformer_joint_stage)(YVEX_TRANSFORMER_JOINT_STAGE_QUERY_NORM + i), values[i]);
        if (!b->refiner) {
            yvex_ir_id args[] = {values[i], b->cosine, b->sine};
            values[i] = joint_op(b, "tensor.rotary_half", args, 3u, b->attention, &head, 1u);
            joint_observe(b, (yvex_transformer_joint_stage)(YVEX_TRANSFORMER_JOINT_STAGE_QUERY_ROTARY + i), values[i]);
        }
    }
    yvex_ir_attribute attrs[] = {head, {.name = "causal", .kind = YVEX_IR_ATTR_BOOL}};
    x = joint_op(b, "attention.full", values, 3u,
        joint_type(b, rows, r->attention_width, YVEX_IR_F32), attrs, 2u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_F32, x);
    x = joint_op(b, "tensor.cast", &x, 1u, b->attention, NULL, 0u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_BF16, x);
    x = joint_project(b, x, 4u, rows, r->attention_width, r->hidden_width,
        YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_PROJECTION_F32);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_PROJECTION_BF16, x);
    return x;
}

static yvex_ir_id joint_block(joint_builder *b, yvex_ir_id x)
{
    const yvex_transformer_joint_recipe *r = b->architecture;
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_INPUT_HIDDEN, x);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_INPUT_TIME, b->timestep);
    yvex_ir_id table = joint_modulation(b);
    yvex_ir_id normalized = joint_norm(b, x, 0u, r->hidden_width, b->hidden);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_NORM1, normalized);
    normalized = joint_condition(b, normalized, table, YVEX_IR_NONE, 0u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_MODULATED1, normalized);
    yvex_ir_id update = joint_attention(b, normalized);
    x = joint_condition(b, x, table, update, 2u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_RESIDUAL, x);
    normalized = joint_norm(b, x, 5u, r->hidden_width, b->hidden);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_NORM2, normalized);
    normalized = joint_condition(b, normalized, table, YVEX_IR_NONE, 3u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_MODULATED2, normalized);
    update = joint_project(b, normalized, 6u, b->rows, r->hidden_width, 2u * r->ffn_width,
        YVEX_TRANSFORMER_JOINT_STAGE_FC1_F32);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_FC1_BF16, update);
    yvex_ir_attribute gate = {.name = "gate_first", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u};
    update = joint_op(b, "nn.swiglu_split", &update, 1u,
        joint_type(b, b->rows, r->ffn_width, YVEX_IR_BF16), &gate, 1u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_SWIGLU, update);
    update = joint_project(b, update, 7u, b->rows, r->ffn_width, r->hidden_width,
        YVEX_TRANSFORMER_JOINT_STAGE_FC2_F32);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_FC2_BF16, update);
    x = joint_condition(b, x, table, update, 5u);
    joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_OUTPUT, x);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, b->layer + 1u, YVEX_TRANSFORMER_JOINT_STAGE_COUNT, x);
    return x;
}

static yvex_ir_id joint_stage_type(joint_builder *b, yvex_transformer_joint_stage stage)
{
    const yvex_transformer_joint_recipe *r = b->architecture;
    unsigned long long rows = b->recipe->rows, width = r->hidden_width;
    yvex_ir_scalar scalar = YVEX_IR_BF16;
    if (stage == YVEX_TRANSFORMER_JOINT_STAGE_MODULATION) {
        rows = b->recipe->timesteps; width *= r->modality_count * r->modulation_parameters;
    } else if (stage == YVEX_TRANSFORMER_JOINT_STAGE_INPUT_TIME ||
        stage == YVEX_TRANSFORMER_JOINT_STAGE_CONDITION_ACTIVATION) {
        rows = b->recipe->timesteps; width = r->timestep_width;
        if (stage == YVEX_TRANSFORMER_JOINT_STAGE_INPUT_TIME) scalar = YVEX_IR_F32;
    } else if (stage >= YVEX_TRANSFORMER_JOINT_STAGE_QKV_F32 && stage <= YVEX_TRANSFORMER_JOINT_STAGE_QKV_BF16)
        width = r->attention_width * 3u;
    else if (stage >= YVEX_TRANSFORMER_JOINT_STAGE_QUERY && stage <= YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_BF16)
        width = r->attention_width;
    else if (stage == YVEX_TRANSFORMER_JOINT_STAGE_FC1_F32 || stage == YVEX_TRANSFORMER_JOINT_STAGE_FC1_BF16)
        width = r->ffn_width * 2u;
    else if (stage == YVEX_TRANSFORMER_JOINT_STAGE_SWIGLU) width = r->ffn_width;
    if (stage == YVEX_TRANSFORMER_JOINT_STAGE_QKV_F32 || stage == YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_F32 ||
        stage == YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_PROJECTION_F32 ||
        stage == YVEX_TRANSFORMER_JOINT_STAGE_FC1_F32 ||
        stage == YVEX_TRANSFORMER_JOINT_STAGE_FC2_F32) scalar = YVEX_IR_F32;
    return joint_type(b, rows, width, scalar);
}

static yvex_ir_id joint_affine(joint_builder *b, yvex_ir_id x, unsigned long long ordinal,
    unsigned long long rows, unsigned long long in, unsigned long long out, yvex_ir_scalar scalar)
{
    x = joint_op(b, "tensor.cast", &x, 1u, joint_type(b, rows, in, scalar), NULL, 0u);
    if (ordinal == 29u)
        joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, b->recipe->blocks + 1u,
            YVEX_TRANSFORMER_JOINT_STAGE_FINAL_TIME_ACTIVATION, x);
    yvex_ir_id args[] = {x, joint_weight(b, ordinal, out, in, scalar),
        joint_weight(b, ordinal + 1u, 0u, out, scalar)};
    return joint_op(b, "nn.linear_bias", args, 3u, joint_type(b, rows, out, scalar), NULL, 0u);
}

static yvex_ir_id joint_refine(joint_builder *b, yvex_ir_id x)
{
    const yvex_transformer_joint_recipe *a = b->architecture;
    b->refiner = 1; b->rows = b->recipe->text_rows;
    b->parameter_base = 10u; b->parameter_stride = 8u;
    b->hidden = joint_type(b, b->rows, a->hidden_width, YVEX_IR_BF16);
    b->attention = joint_type(b, b->rows, a->attention_width, YVEX_IR_BF16);
    x = joint_affine(b, x, 4u, b->rows, a->condition_input_width, a->hidden_width, YVEX_IR_BF16);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_REFINER, 0u, YVEX_TRANSFORMER_JOINT_STAGE_INPUT_HIDDEN, x);
    for (b->layer = 0u; b->rc == YVEX_OK && b->layer < a->refiner_block_count; ++b->layer) {
        yvex_ir_id normalized = joint_norm(b, x, 0u, a->hidden_width, b->hidden);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_NORM1, normalized);
        yvex_ir_id residual[] = {x, joint_attention(b, normalized)};
        x = joint_op(b, "tensor.add", residual, 2u, b->hidden, NULL, 0u);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_ATTENTION_RESIDUAL, x);
        normalized = joint_norm(b, x, 5u, a->hidden_width, b->hidden);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_NORM2, normalized);
        yvex_ir_id update = joint_project(b, normalized, 6u, b->rows, a->hidden_width, a->ffn_width * 2u,
            YVEX_TRANSFORMER_JOINT_STAGE_FC1_F32);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_FC1_BF16, update);
        yvex_ir_attribute gate = {.name = "gate_first", .kind = YVEX_IR_ATTR_BOOL, .value.integer = 1u};
        update = joint_op(b, "nn.swiglu_split", &update, 1u,
            joint_type(b, b->rows, a->ffn_width, YVEX_IR_BF16), &gate, 1u);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_SWIGLU, update);
        update = joint_project(b, update, 7u, b->rows, a->ffn_width, a->hidden_width,
            YVEX_TRANSFORMER_JOINT_STAGE_FC2_F32);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_FC2_BF16, update);
        residual[0] = x; residual[1] = update;
        x = joint_op(b, "tensor.add", residual, 2u, b->hidden, NULL, 0u);
        joint_observe(b, YVEX_TRANSFORMER_JOINT_STAGE_OUTPUT, x);
    }
    b->parameter_base = 26u; b->layer = 0u;
    x = joint_norm(b, x, 0u, a->hidden_width, b->hidden);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_REFINER, a->refiner_block_count + 1u,
        YVEX_TRANSFORMER_JOINT_STAGE_OUTPUT, x);
    b->refiner = 0; b->rows = b->recipe->rows;
    b->parameter_base = 35u; b->parameter_stride = 10u;
    b->hidden = joint_type(b, b->rows, a->hidden_width, YVEX_IR_BF16);
    b->attention = joint_type(b, b->rows, a->attention_width, YVEX_IR_BF16);
    return x;
}

static yvex_ir_id joint_time(joint_builder *b, yvex_ir_id time)
{
    const yvex_joint_program_recipe *r = b->recipe;
    const yvex_transformer_joint_recipe *a = b->architecture;
    yvex_ir_attribute period = {.name = "maximum_period", .kind = YVEX_IR_ATTR_F64,
        .value.real = r->maximum_period};
    time = joint_op(b, "nn.sinusoidal_embedding", &time, 1u,
        joint_type(b, r->timesteps, r->time_embedding_width, YVEX_IR_F32), &period, 1u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, 1u, YVEX_TRANSFORMER_JOINT_STAGE_TIME_INPUT, time);
    yvex_ir_id weight = joint_weight(b, 6u, a->hidden_width, r->time_embedding_width, YVEX_IR_F32);
    yvex_ir_id args[] = {time, weight};
    time = joint_op(b, "nn.linear", args, 2u,
        joint_type(b, r->timesteps, a->hidden_width, YVEX_IR_F32), NULL, 0u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, 1u, YVEX_TRANSFORMER_JOINT_STAGE_TIME_PROJECTION_IN, time);
    args[0] = time; args[1] = joint_weight(b, 7u, 0u, a->hidden_width, YVEX_IR_F32);
    time = joint_op(b, "tensor.channel_bias", args, 2u,
        joint_type(b, r->timesteps, a->hidden_width, YVEX_IR_F32), NULL, 0u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, 1u, YVEX_TRANSFORMER_JOINT_STAGE_TIME_BIAS_IN, time);
    time = joint_op(b, "nn.silu", &time, 1u,
        joint_type(b, r->timesteps, a->hidden_width, YVEX_IR_F32), NULL, 0u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, 1u, YVEX_TRANSFORMER_JOINT_STAGE_TIME_ACTIVATION, time);
    args[0] = time; args[1] = joint_weight(b, 8u, a->timestep_width, a->hidden_width, YVEX_IR_F32);
    yvex_ir_id type = joint_type(b, r->timesteps, a->timestep_width, YVEX_IR_F32);
    time = joint_op(b, "nn.linear", args, 2u, type, NULL, 0u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, 1u, YVEX_TRANSFORMER_JOINT_STAGE_TIME_PROJECTION_OUT, time);
    args[0] = time; args[1] = joint_weight(b, 9u, 0u, a->timestep_width, YVEX_IR_F32);
    return joint_op(b, "tensor.channel_bias", args, 2u, type, NULL, 0u);
}

static yvex_ir_id joint_final(joint_builder *b, yvex_ir_id x, yvex_ir_id indices)
{
    const yvex_transformer_joint_recipe *a = b->architecture;
    unsigned long long timesteps = b->recipe->timesteps;
    b->parameter_base = 28u; b->layer = 0u;
    x = joint_norm(b, x, 0u, a->hidden_width, b->hidden);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, b->recipe->blocks + 1u,
        YVEX_TRANSFORMER_JOINT_STAGE_FINAL_NORM, x);
    yvex_ir_id time = joint_op(b, "nn.silu", &b->timestep, 1u,
        joint_type(b, timesteps, a->timestep_width, YVEX_IR_F32), NULL, 0u);
    time = joint_affine(b, time, 29u, timesteps, a->timestep_width, a->hidden_width * 2u, YVEX_IR_BF16);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, b->recipe->blocks + 1u,
        YVEX_TRANSFORMER_JOINT_STAGE_FINAL_ADALN, time);
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 3u,
        .shape = {{YVEX_IR_NONE, timesteps}, {YVEX_IR_NONE, 2u}, {YVEX_IR_NONE, a->hidden_width}}};
    yvex_ir_id type;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_type_intern(b->module, &t, &type, b->err);
    if (b->rc != YVEX_OK) return YVEX_IR_NONE;
    time = joint_op(b, "tensor.reshape", &time, 1u, type, NULL, 0u);
    b->indices = indices;
    x = joint_condition(b, x, time, YVEX_IR_NONE, 0u);
    joint_trace(b, YVEX_TRANSFORMER_JOINT_SCOPE_OMNI, b->recipe->blocks + 1u,
        YVEX_TRANSFORMER_JOINT_STAGE_FINAL_HIDDEN, x);
    return x;
}

static int joint_return(joint_builder *b, const yvex_ir_id *values, size_t count)
{
    yvex_ir_id op;
    yvex_ir_operation_request r = {.operation = "core.return", .operands = values, .operand_count = count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &op, b->err);
    return b->rc;
}

static int joint_call(joint_builder *b, const char *symbol, const yvex_ir_id *inputs, size_t count,
    const yvex_ir_id *types, yvex_ir_id *values, size_t results)
{
    yvex_ir_id op;
    yvex_ir_attribute callee = {.name = "callee", .kind = YVEX_IR_ATTR_SYMBOL};
    yvex_core_text_copy(callee.value.text, sizeof(callee.value.text), symbol);
    yvex_ir_operation_request r = {.operation = "core.call", .operands = inputs, .operand_count = count,
        .result_types = types, .result_count = results, .attributes = &callee, .attribute_count = 1u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &r, &op, b->err);
    if (b->rc == YVEX_OK) memcpy(values, yvex_ir_operation_at(b->module, op)->results, results * sizeof(*values));
    return b->rc;
}

static int joint_preparation(joint_builder *b, const yvex_ir_id *inputs, const yvex_ir_id outputs[3])
{
    const yvex_joint_program_recipe *r = b->recipe;
    const yvex_transformer_joint_recipe *a = b->architecture;
    yvex_ir_id function, op;
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "prepare", inputs, 2u, outputs, 3u,
        joint_effects(b), &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    yvex_ir_id args[2], results[3];
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, sizeof(args));
    results[0] = joint_refine(b, args[0]);
    yvex_ir_id positions = joint_op(b, "tensor.reshape", args + 1u, 1u,
        joint_type(b, r->rows, r->position_axes, YVEX_IR_F32), NULL, 0u);
    unsigned long long frequencies = a->rotary_width / (2u * r->position_axes);
    yvex_ir_id frequency_type = joint_type(b, 0u, frequencies, YVEX_IR_F32);
    yvex_ir_id freq = joint_weight(b, 27u, 0u, frequencies, YVEX_IR_F32);
    freq = joint_op(b, "tensor.copy", &freq, 1u, frequency_type, NULL, 0u);
    yvex_ir_id rotary[] = {positions, freq};
    yvex_ir_operation_request tables = {.operation = "tensor.axis_rotary", .operands = rotary,
        .operand_count = 2u, .result_types = outputs + 1u, .result_count = 2u};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &tables, &op, b->err);
    if (b->rc == YVEX_OK) memcpy(results + 1u, yvex_ir_operation_at(b->module, op)->results, 2u * sizeof(*results));
    return joint_return(b, results, 3u);
}

static int joint_step(joint_builder *b, const yvex_ir_id inputs[11], const yvex_ir_id *outputs, size_t result_count)
{
    const yvex_joint_program_recipe *r = b->recipe;
    const yvex_transformer_joint_recipe *a = b->architecture;
    yvex_ir_id function, args[11], results[3];
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "step", inputs, 11u, outputs, result_count,
        joint_effects(b), &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, sizeof(args));
    b->rows = r->rows; b->parameter_base = 35u; b->parameter_stride = 10u;
    b->hidden = joint_type(b, r->rows, a->hidden_width, YVEX_IR_BF16);
    b->attention = joint_type(b, r->rows, a->attention_width, YVEX_IR_BF16);
    yvex_ir_id video = joint_affine(b, args[0], 2u, r->video_rows,
        a->video_input_width, a->hidden_width, YVEX_IR_F32);
    video = joint_op(b, "tensor.cast", &video, 1u,
        joint_type(b, r->video_rows, a->hidden_width, YVEX_IR_BF16), NULL, 0u);
    yvex_ir_id audio = joint_affine(b, args[1], 0u, r->audio_rows,
        a->audio_input_width, a->hidden_width, YVEX_IR_F32);
    audio = joint_op(b, "tensor.cast", &audio, 1u,
        joint_type(b, r->audio_rows, a->hidden_width, YVEX_IR_BF16), NULL, 0u);
    yvex_ir_id partition[] = {video, args[6], audio, args[7], args[2], args[8]};
    yvex_ir_id x = joint_op(b, "tensor.partition_rows", partition, 6u, b->hidden, NULL, 0u);
    b->cosine = args[4]; b->sine = args[5];
    yvex_ir_id coordinates[] = {args[10], args[9]};
    yvex_ir_attribute domains[] = {
        {.name = "major_extent", .kind = YVEX_IR_ATTR_U64, .value.integer = r->timesteps},
        {.name = "minor_extent", .kind = YVEX_IR_ATTR_U64, .value.integer = a->modality_count}};
    b->indices = joint_op(b, "tensor.index_linearize", coordinates, 2u,
        joint_type(b, 0u, r->rows, YVEX_IR_INDEX), domains, 2u);
    b->timestep = joint_time(b, args[3]);
    for (b->layer = 0u; b->rc == YVEX_OK && b->layer < r->blocks; ++b->layer) x = joint_block(b, x);
    x = joint_final(b, x, args[10]);
    results[2] = x;
    video = joint_affine(b, x, 31u, r->rows, a->hidden_width, a->video_input_width, YVEX_IR_F32);
    audio = joint_affine(b, x, 33u, r->rows, a->hidden_width, a->audio_input_width, YVEX_IR_F32);
    yvex_ir_id selection[] = {video, args[6]};
    results[0] = joint_op(b, "tensor.indexed_rows", selection, 2u, outputs[0], NULL, 0u);
    selection[0] = audio; selection[1] = args[7];
    results[1] = joint_op(b, "tensor.indexed_rows", selection, 2u, outputs[1], NULL, 0u);
    return joint_return(b, results, result_count);
}

static int joint_complete_module(joint_builder *b)
{
    const yvex_joint_program_recipe *r = b->recipe;
    const yvex_transformer_joint_recipe *a = b->architecture;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_dimension population = {.name = "rows", .minimum = r->rows, .maximum = r->rows, .multiple = 1u};
    yvex_ir_id dim, function, inputs[10], results[3];
    b->rc = yvex_ir_module_open(&b->module, "joint_component", r->source_identity, dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &population, &dim, b->err);
    inputs[0] = joint_type(b, r->video_rows, a->video_input_width, YVEX_IR_F32);
    inputs[1] = joint_type(b, r->audio_rows, a->audio_input_width, YVEX_IR_F32);
    inputs[2] = joint_type(b, r->text_rows, a->condition_input_width, YVEX_IR_F32);
    inputs[3] = joint_type(b, r->timesteps, 1u, YVEX_IR_F32);
    yvex_ir_id positions_type = joint_type(b, r->rows, r->position_axes, YVEX_IR_F32);
    if (b->rc == YVEX_OK) {
        yvex_ir_type symbolic = *yvex_ir_type_at(b->module, positions_type);
        symbolic.shape[0] = (yvex_ir_extent){dim, 0u};
        b->rc = yvex_ir_type_intern(b->module, &symbolic, inputs + 4u, b->err);
    }
    inputs[5] = joint_type(b, 0u, r->video_rows, YVEX_IR_INDEX);
    inputs[6] = joint_type(b, 0u, r->audio_rows, YVEX_IR_INDEX);
    inputs[7] = joint_type(b, 0u, r->text_rows, YVEX_IR_INDEX);
    inputs[8] = inputs[9] = joint_type(b, 0u, r->rows, YVEX_IR_INDEX);
    yvex_ir_id outputs[] = {joint_type(b, r->video_rows, a->video_input_width, YVEX_IR_F32),
        joint_type(b, r->audio_rows, a->audio_input_width, YVEX_IR_F32),
        joint_type(b, r->rows, a->hidden_width, YVEX_IR_BF16)};
    size_t result_count = r->final_hidden_result ? 3u : 2u;
    yvex_ir_id prepared_types[] = {joint_type(b, r->text_rows, a->hidden_width, YVEX_IR_BF16),
        joint_type(b, r->rows, a->rotary_width, YVEX_IR_BF16),
        joint_type(b, r->rows, a->rotary_width, YVEX_IR_BF16)};
    yvex_ir_id preparation_inputs[] = {inputs[2], inputs[4]};
    if (b->rc == YVEX_OK) b->rc = joint_preparation(b, preparation_inputs, prepared_types);
    yvex_ir_id step_inputs[] = {inputs[0], inputs[1], prepared_types[0], inputs[3],
        prepared_types[1], prepared_types[2], inputs[5], inputs[6], inputs[7], inputs[8], inputs[9]};
    if (b->rc == YVEX_OK) b->rc = joint_step(b, step_inputs, outputs, result_count);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "forward", inputs, 10u, outputs, result_count,
        joint_effects(b), &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    yvex_ir_id args[10];
    memcpy(args, yvex_ir_block_at(b->module, b->block)->arguments, sizeof(args));
    yvex_ir_id prepared[3], preparation_args[] = {args[2], args[4]};
    joint_call(b, "prepare", preparation_args, 2u, prepared_types, prepared, 3u);
    if (b->rc != YVEX_OK) return b->rc;
    yvex_ir_id step_args[] = {args[0], args[1], prepared[0], args[3], prepared[1], prepared[2],
        args[5], args[6], args[7], args[8], args[9]};
    joint_call(b, "step", step_args, 11u, outputs, results, result_count);
    joint_return(b, results, result_count);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

static int joint_module(joint_builder *b)
{
    const yvex_joint_program_recipe *r = b->recipe;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_dimension rows = {.name = "rows", .minimum = r->rows, .maximum = r->rows, .multiple = 1u};
    yvex_ir_id dimension, inputs[5], function, op;
    b->rc = yvex_ir_module_open(&b->module, "joint_conditioned", r->source_identity, dialects, 2u, b->err);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_dimension_add(b->module, &rows, &dimension, b->err);
    b->hidden = joint_type(b, r->rows, b->architecture->hidden_width, YVEX_IR_BF16);
    b->attention = joint_type(b, r->rows, b->architecture->attention_width, YVEX_IR_BF16);
    if (b->rc == YVEX_OK) {
        yvex_ir_type input = *yvex_ir_type_at(b->module, b->hidden);
        input.shape[0] = (yvex_ir_extent){dimension, 0u};
        b->rc = yvex_ir_type_intern(b->module, &input, inputs, b->err);
    }
    inputs[1] = joint_type(b, r->timesteps, b->architecture->timestep_width, YVEX_IR_F32);
    inputs[2] = joint_type(b, 0u, r->rows, YVEX_IR_INDEX);
    inputs[3] = inputs[4] = joint_type(b, r->rows, b->architecture->rotary_width, YVEX_IR_BF16);
    b->result_count = 1u + b->program->block_result_count + b->program->stage_result_count;
    b->results = calloc(b->result_count, sizeof(*b->results));
    b->result_types = calloc(b->result_count, sizeof(*b->result_types));
    if (!b->results || !b->result_types) return YVEX_ERR_NOMEM;
    for (size_t i = 0u; i <= b->program->block_result_count; ++i) b->result_types[i] = b->hidden;
    for (size_t i = 0u; i < b->program->stage_result_count; ++i)
        b->result_types[1u + b->program->block_result_count + i] = joint_stage_type(b, b->program->stages[i]);
    if (b->rc == YVEX_OK) b->rc = yvex_ir_function_add(b->module, "forward", inputs, 5u,
        b->result_types, b->result_count, joint_effects(b), &function, b->err);
    if (b->rc != YVEX_OK) return b->rc;
    b->block = yvex_ir_function_at(b->module, function)->body;
    const yvex_ir_id *args = yvex_ir_block_at(b->module, b->block)->arguments;
    yvex_ir_id x = args[0];
    b->timestep = args[1]; b->indices = args[2]; b->cosine = args[3]; b->sine = args[4];
    x = joint_op(b, "tensor.reshape", &x, 1u, b->hidden, NULL, 0u);
    for (b->layer = 0u; b->rc == YVEX_OK && b->layer < r->blocks; ++b->layer) {
        x = joint_block(b, x);
        if (r->block_results) b->results[1u + b->layer] = x;
    }
    b->results[0] = x;
    /* External result slots cannot alias even when two observations name the
     * same SSA value. Materialize a separately owned, precision-preserving view. */
    for (size_t i = 1u; i < b->result_count; ++i)
        for (size_t j = 0u; j < i; ++j)
            if (b->results[j] == b->results[i]) {
                b->results[i] = joint_op(b, "tensor.reshape", b->results + i, 1u, b->result_types[i], NULL, 0u);
                break;
            }
    yvex_ir_operation_request ret = {.operation = "core.return", .operands = b->results,
        .operand_count = b->result_count};
    if (b->rc == YVEX_OK) b->rc = yvex_ir_operation_add(b->module, b->block, &ret, &op, b->err);
    return b->rc == YVEX_OK ? yvex_ir_seal(b->module, b->err) : b->rc;
}

/* Import the currently admitted output specializations at the physical trust
 * boundary. They never become semantic attributes or backend topology. */
static int joint_target(joint_builder *b, yvex_program_physical **physical)
{
    const yvex_transformer_linear_physical_plan *targets[] = {
        b->recipe->video_output_target, b->recipe->audio_output_target};
    if (!targets[0] && !targets[1]) return YVEX_OK;
    yvex_program_target_choice choices[2];
    const yvex_program_physical_summary *s = yvex_program_physical_summary_get(*physical);
    for (size_t i = 0u; i < 2u; ++i) {
        const yvex_transformer_linear_physical_plan *t = targets[i];
        if (!t || yvex_transformer_linear_physical_validate(t, b->err) != YVEX_OK ||
            t->backend != YVEX_BACKEND_KIND_CUDA || t->workspace_bytes != 1048576u ||
            t->input_width != b->architecture->hidden_width ||
            t->output_width != (i ? b->architecture->audio_input_width : b->architecture->video_input_width) ||
            t->operation != (i ? YVEX_TRANSFORMER_LINEAR_OPERATION_JOINT_AUDIO_OUTPUT :
                YVEX_TRANSFORMER_LINEAR_OPERATION_JOINT_VIDEO_OUTPUT)) {
            yvex_error_set(b->err, YVEX_ERR_UNSUPPORTED, "compiler.joint.target",
                "output target differs from the admitted physical implementation");
            return YVEX_ERR_UNSUPPORTED;
        }
        choices[i] = (yvex_program_target_choice){.step = SIZE_MAX,
            .implementation = i ? "linear_bias.cuda.sm121.5376x32.f32.v1" : "linear_bias.cuda.sm121.5376x96.f32.v1"};
        for (size_t j = 0u; j < s->step_count; ++j) {
            const yvex_program_physical_step *step = yvex_program_physical_step_at(*physical, j);
            if (!strcmp(step->implementation, "linear_bias.f32.v1") &&
                yvex_program_physical_value_at(*physical, step->operands[1])->tensor_id == 31u + i * 2u)
                choices[i].step = j;
        }
    }
    yvex_program_physical *target = NULL;
    int rc = yvex_program_physical_target_compile(&target, *physical, choices, 2u, b->err);
    if (rc == YVEX_OK) {
        yvex_program_physical_close(physical);
        *physical = target;
    }
    return rc;
}

static int joint_lower(joint_builder *b)
{
    yvex_ir_module *canonical = NULL;
    yvex_program_execution *execution = NULL;
    size_t count = (size_t)b->recipe->blocks * 10u + (b->recipe->video_rows ? 35u : 0u), used = 0u;
    yvex_ir_pass passes[] = {*yvex_ir_inline_pass(), *yvex_ir_canonical_pass(), *yvex_ir_dead_code_pass()};
    int rc = yvex_ir_pass_pipeline(b->module, passes, 3u, &canonical, NULL, b->err);
    size_t capacity = rc == YVEX_OK ? yvex_ir_operation_count(canonical) : 0u;
    yvex_program_parameter_binding *bindings = calloc(capacity ? capacity : 1u, sizeof(*bindings));
    if (rc == YVEX_OK && !bindings) rc = YVEX_ERR_NOMEM;
    for (size_t i = 0u; rc == YVEX_OK && i < yvex_ir_operation_count(canonical); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(canonical, i);
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const yvex_ir_attribute *a = yvex_ir_attribute_get(canonical, i, "parameter");
        unsigned long long ordinal;
        char trailing;
        if (!a || used >= capacity || sscanf(a->value.text, "weight_%llu%c", &ordinal, &trailing) != 1 ||
            ordinal >= count) {
            rc = YVEX_ERR_FORMAT; break;
        }
        const yvex_ir_type *type = yvex_ir_type_at(canonical, yvex_ir_value_at(canonical, op->results[0])->type);
        bindings[used++] = (yvex_program_parameter_binding){op->results[0], ordinal,
            type->scalar == YVEX_IR_F32 ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16};
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, canonical, b->err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&b->program->physical, execution, "forward",
        bindings, used, yvex_ir_identity(canonical), b->err);
    if (rc == YVEX_OK) rc = joint_target(b, &b->program->physical);
    if (rc == YVEX_OK && b->recipe->video_rows)
        rc = yvex_program_physical_compile(&b->program->preparation, execution, "prepare",
            bindings, used, yvex_ir_identity(canonical), b->err);
    if (rc == YVEX_OK && b->recipe->video_rows)
        rc = yvex_program_physical_compile(&b->program->step, execution, "step",
            bindings, used, yvex_ir_identity(canonical), b->err);
    if (rc == YVEX_OK && b->program->step) rc = joint_target(b, &b->program->step);
    free(bindings); yvex_program_execution_close(&execution); yvex_ir_module_close(&canonical);
    return rc;
}

int yvex_joint_program_compile(yvex_joint_program **out, const yvex_joint_program_recipe *r, yvex_error *err)
{
    const yvex_transformer_joint_recipe *a = r ? r->architecture : NULL;
    unsigned long long product, width;
    if (out) *out = NULL;
    if (!out || !a || a->schema_version != YVEX_TRANSFORMER_JOINT_SCHEMA_V5 ||
        !yvex_sha256_hex_valid(r->source_identity) || !r->rows || !r->timesteps || !r->blocks ||
        r->rows > a->maximum_packed_rows || r->timesteps > a->maximum_timesteps || r->blocks > a->block_count ||
        r->inspected_block > r->blocks || (r->block_results != 0 && r->block_results != 1) ||
        (r->final_hidden_result != 0 && r->final_hidden_result != 1) || (!r->video_rows && r->final_hidden_result) ||
        (!r->video_rows && (r->video_output_target || r->audio_output_target)) ||
        (r->observe_blocks != 0 && r->observe_blocks != 1) ||
        (r->observe_stages != 0 && r->observe_stages != 1) ||
        (r->observe_stages && ((unsigned int)r->observed_scope >= YVEX_TRANSFORMER_JOINT_SCOPE_COUNT ||
            (unsigned int)r->observed_stage > YVEX_TRANSFORMER_JOINT_STAGE_COUNT ||
            r->observed_block > (r->observed_scope == YVEX_TRANSFORMER_JOINT_SCOPE_REFINER ?
                a->refiner_block_count : r->blocks) + 1u)) ||
        !a->hidden_width || !a->timestep_width || !a->ffn_width || !a->modality_count ||
        a->modulation_parameters != 6u ||
        !a->attention_heads || !a->head_dimension || a->hidden_width % 4u || a->head_dimension % 4u ||
        !yvex_core_u64_mul(a->attention_heads, a->head_dimension, &width) || width != a->attention_width ||
        !a->rotary_width || a->rotary_width > a->head_dimension || a->rotary_width % 2u ||
        a->qkv_layout != YVEX_TRANSFORMER_QKV_LAYOUT_PER_HEAD_THREE ||
        a->swiglu_layout != YVEX_TRANSFORMER_SWIGLU_LAYOUT_GATE_THEN_UP ||
        !isfinite(r->normalization_epsilon) || r->normalization_epsilon <= 0.0 ||
        !yvex_core_u64_mul(a->hidden_width, a->modality_count, &product) ||
        !yvex_core_u64_mul(product, a->modulation_parameters, &product) ||
        !yvex_core_u64_mul(product, r->timesteps, &product) ||
        a->ffn_width > UINT64_MAX / 2u || width > UINT64_MAX / 3u || r->blocks > 65536u) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.joint",
            "source conditioning geometry/layout exceeds admitted program scope");
        return YVEX_ERR_FORMAT;
    }
    if (r->video_rows && (!r->audio_rows || !r->text_rows || !r->position_axes ||
        r->position_axes > a->rotary_width / 2u || a->rotary_width % (2u * r->position_axes) ||
        !r->time_embedding_width || r->time_embedding_width % 2u ||
        !isfinite(r->maximum_period) || r->maximum_period <= 1.0 ||
        !yvex_core_u64_add(r->video_rows, r->audio_rows, &product) ||
        !yvex_core_u64_add(product, r->text_rows, &product) || product != r->rows ||
        !a->video_input_width || !a->audio_input_width || !a->condition_input_width ||
        a->refiner_block_count != 2u || r->inspected_block || r->block_results)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "compiler.joint", "complete component source signature is incompatible");
        return YVEX_ERR_FORMAT;
    }
    yvex_joint_program *p = calloc(1u, sizeof(*p));
    if (!p) return YVEX_ERR_NOMEM;
    p->block_result_count = r->block_results ? (size_t)r->blocks : 0u;
    if (r->inspected_block) {
        for (unsigned int stage = 0u; stage <= YVEX_TRANSFORMER_JOINT_STAGE_INPUT_TIME; ++stage)
            p->stages[p->stage_result_count++] = (yvex_transformer_joint_stage)stage;
        p->stages[p->stage_result_count++] = YVEX_TRANSFORMER_JOINT_STAGE_CONDITION_ACTIVATION;
    }
    joint_builder b = {.recipe = r, .architecture = a, .program = p, .err = err,
        .rows = r->rows, .parameter_stride = 10u};
    int rc = r->video_rows ? joint_complete_module(&b) : joint_module(&b);
    if (rc == YVEX_OK) rc = joint_lower(&b);
    free(b.results); free(b.result_types); yvex_ir_module_close(&b.module);
    if (rc == YVEX_OK) *out = p;
    else yvex_joint_program_close(&p);
    return rc;
}

void yvex_joint_program_close(yvex_joint_program **pointer)
{
    yvex_joint_program *p = pointer ? *pointer : NULL;
    if (!p) return;
    yvex_program_physical_close(&p->preparation); yvex_program_physical_close(&p->step);
    yvex_program_physical_close(&p->physical); free(p); *pointer = NULL;
}
