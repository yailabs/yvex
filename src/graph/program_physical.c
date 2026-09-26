/* Legalize computational entries into typed physical work and reusable slots. */
#include <yvex/internal/program_physical.h>
#include <yvex/internal/execution.h>
#include <yvex/qtype.h>

#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PHYSICAL_LIMIT 1048576u
#define PHYSICAL_BYTES (256u * 1024u * 1024u)

struct yvex_program_physical {
    atomic_uint references;
    yvex_program_physical_summary summary;
    yvex_program_physical_value *values;
    yvex_program_physical_step *steps;
    yvex_ir_id *results;
    yvex_gated_delta_plan **delta;
    yvex_selective_ssd_geometry **ssd;
    yvex_sequence_state_binding *sequence;
    size_t sequence_count;
};

typedef struct {
    const char *semantic, *implementation;
    unsigned int effects, parameter_operands; /* Bit i declares encoded parameter operand i. */
} physical_rule;

static const physical_rule physical_rules[] = {
    {"core.observe", "observe.f32-storage.v1", YVEX_IR_PUBLISH | YVEX_IR_ORDERED, 0u},
    {"tensor.indexed_rows", "indexed_rows.bf16.v1", 0u, 0u},
    {"tensor.indexed_rows", "indexed_rows.f32.v1", 0u, 0u},
    {"tensor.partition_rows", "partition_rows.bf16.v1", 0u, 0u},
    {"tensor.partition_rows", "partition_rows.f32.v1", 0u, 0u},
    {"tensor.axis_rotary", "axis_rotary.f32math.bf16.v1", 0u, 0u},
    {"nn.sinusoidal_embedding", "sinusoidal_embedding.f32.v1", 0u, 0u},
    {"tensor.cast", "cast.rne.bf16.v1", 0u, 0u},
    {"tensor.cast", "cast.exact.f32.v1", 0u, 0u},
    {"nn.silu", "silu.bf16.v1", 0u, 0u},
    {"nn.silu", "silu.f32.v1", 0u, 0u},
    {"tensor.index_linearize", "index_linearize.host.u32.v1", 0u, 0u},
    {"nn.indexed_modulate", "indexed_modulate.bf16.v1", 0u, 0u},
    {"nn.indexed_gated_residual", "indexed_gated_residual.bf16.v1", 0u, 0u},
    {"tensor.mean", "mean.f32.v1", 0u, 0u},
    {"tensor.clamp", "clamp.f32.v1", 0u, 0u},
    {"nn.clamped_swiglu", "clamped_swiglu.f64math.bf16.v1", 0u, 0u},
    {"signal.conv1d", "conv1d.f32.v1", 0u, 0x06u},
    {"signal.conv2d_slice", "conv2d_slice.f32.v1", 0u, 0x06u},
    {"nn.spatial_group_norm_silu", "spatial_group_norm_silu.f32.v1", 0u, 0x06u},
    {"signal.normalized_conv1d", "normalized_conv1d.f32.v1", 0u, 0x0eu},
    {"signal.normalized_conv1d_unbiased", "normalized_conv1d_unbiased.f32.v1", 0u, 0x06u},
    {"signal.alias_snake", "alias_snake.f32.v1", 0u, 0x1eu},
    {"core.parameter", "parameter.encoded.v1", 0u, 0u},
    {"tensor.reshape", "reshape.f32-storage.v1", 0u, 0u},
    {"tensor.slice_rows", "slice_rows.f32.v1", 0u, 0u},
    {"tensor.copy", "parameter_copy.f32.v1", 0u, 0x01u},
    {"tensor.zeros", "zeros.f32.v1", 0u, 0u},
    {"tensor.concat_rows", "concat_rows.f32.v1", 0u, 0u},
    {"tensor.channel_bias", "channel_bias.f32.v1", 0u, 0x02u},
    {"tensor.scaled_residual", "scaled_residual.f32.v1", 0u, 0x04u},
    {"tensor.split_interleaved_three", "split_interleaved_three.bf16.v1", 0u, 0u},
    {"tensor.split_interleaved_three", "split_interleaved_three.f32.v1", 0u, 0u},
    {"nn.swiglu_split", "swiglu_split.bf16.v1", 0u, 0u},
    {"nn.swiglu_split", "swiglu_split.f32.v1", 0u, 0u},
    {"nn.rms_normalize", "rms_normalize.f32.v1", 0u, 0u},
    {"nn.embedding", "embedding.bf16.v1", 0u, 0x02u},
    {"nn.embedding", "embedding.encoded.f32.v1", 0u, 0x02u},
    {"nn.linear", "linear.bf16.f32acc.v1", 0u, 0x02u},
    {"nn.linear", "linear.encoded.f32.v1", 0u, 0x02u},
    {"nn.linear", "linear.row_dot.f32.v1", 0u, 0x02u},
    {"nn.linear", "linear.row_dot.q8.v1", 0u, 0x02u},
    {"nn.linear_residual", "linear_residual.bf16.f32add.v1", 0u, 0x02u},
    {"nn.linear_bias", "linear_bias.bf16.f32acc.v1", 0u, 0x06u},
    {"nn.linear_bias", "linear_bias.f32.v1", 0u, 0x06u},
    {"nn.linear_bias", "linear_bias.cuda.sm121.5376x96.f32.v1", 0u, 0x06u},
    {"nn.linear_bias", "linear_bias.cuda.sm121.5376x32.f32.v1", 0u, 0x06u},
    {"nn.layer_norm", "layer_norm.f32acc.bf16.v1", 0u, 0x06u},
    {"nn.layer_norm", "layer_norm.f32.v1", 0u, 0x06u},
    {"nn.layer_norm_unbiased", "layer_norm_unbiased.f32.v1", 0u, 0x02u},
    {"nn.gelu", "gelu.erf.bf16.v1", 0u, 0u},
    {"nn.gelu_tanh", "gelu.tanh.bf16.v1", 0u, 0u},
    {"tensor.split_three", "split_three.bf16.v1", 0u, 0u},
    {"tensor.split_three", "split_three.f32.v1", 0u, 0u},
    {"tensor.split_two", "split_two.f32.v1", 0u, 0u},
    {"tensor.multiply", "multiply.f32.v1", 0u, 0u},
    {"nn.gelu", "gelu.erf.f32.v1", 0u, 0u},
    {"tensor.rotary_half_f32", "rotary_half.f32acc.bf16.v1", 0u, 0u},
    {"tensor.rotary_half_f32", "rotary_half.f32.v1", 0u, 0u},
    {"tensor.grid_bilinear", "grid_bilinear.f64weights.bf16.v1", 0u, 0x01u},
    {"tensor.grid_rotary", "grid_rotary.f32.v1", 0u, 0u},
    {"mhc.head_norm", "mhc.head_norm.bf16.v1", 0u, 0x1eu},
    {"mhc.residual_pre", "mhc.residual_pre.bf16.v1", 0u, 0x0cu},
    {"tensor.stream_mean", "stream_mean.f32.f64acc.v1", 0u, 0u},
    {"mhc.residual_post", "mhc.residual_post.f64acc.bf16.v1", 0u, 0u},
    {"nn.rms_norm", "rms_norm.bf16.v1", 0u, 0x02u},
    {"nn.rms_norm", "rms_norm.f32.v1", 0u, 0x02u},
    {"nn.weighted_rms", "weighted_rms.f64scale.bf16.v1", 0u, 0x02u},
    {"nn.group_rms_norm", "group_rms_norm.bf16.vector4.v1", 0u, 0x02u},
    {"tensor.rotary_half", "rotary_half.bf16.products.v1", 0u, 0u},
    {"attention.full", "attention.full.f32acc.bf16.v1", 0u, 0u},
    {"attention.full", "attention.full.f32.v1", 0u, 0u},
    {"tensor.rotary_tables", "rotary_tables.f64.bf16.v1", 0u, 0u},
    {"tensor.masked_rows", "masked_rows.host.bf16.v1", 0u, 0u},
    {"nn.silu_product", "silu_product.bf16.v1", 0u, 0u},
    {"tensor.add", "add.bf16.v1", 0u, 0u},
    {"tensor.add", "add.f32.v1", 0u, 0u},
    {"sequence.selective_ssd", "selective_ssd.cpu.f32state.v1",
     YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, 0x7eu},
    {"sequence.gated_delta", "gated_delta.bf16.f32state.v1", YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, 0xf0u},
    {"attention.gated_causal", "gated_causal.bf16.v1", YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, 0x18u}};

static int physical_refuse(yvex_error *err, yvex_status status, const char *reason)
{
    yvex_error_set(err, status, "compiler.program.physical", reason);
    return status;
}

int yvex_program_physical_value_layout(const yvex_program_physical *p, size_t value,
    unsigned long long rows, yvex_program_value_layout *out, yvex_error *err)
{
    yvex_program_value_layout layout = {.elements = 1u};
    if (out) memset(out, 0, sizeof(*out));
    if (!p || !out || value >= p->summary.value_count)
        return physical_refuse(err, YVEX_ERR_INVALID_ARG, "admitted program and value required");
    if (rows < p->summary.minimum_rows || rows > p->summary.maximum_rows ||
        !p->summary.row_multiple || rows % p->summary.row_multiple)
        return physical_refuse(err, YVEX_ERR_BOUNDS, "population exceeds the compiled entrypoint envelope");
    const yvex_program_physical_value *v = &p->values[value];
    const yvex_ir_type *t = &v->type;
    if (v->parameter || t->kind != YVEX_IR_TENSOR || !t->rank || t->rank > YVEX_IR_RANK_CAP ||
        (t->scalar != YVEX_IR_F32 && t->scalar != YVEX_IR_BF16 &&
         !(t->scalar == YVEX_IR_INDEX && t->rank == 1u)))
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "value has no admitted activation carrier");
    layout.storage_scalar = t->scalar == YVEX_IR_INDEX ? YVEX_IR_INDEX : YVEX_IR_F32;
    layout.rank = t->rank;
    layout.dynamic_rows = t->shape[0].symbol != YVEX_IR_NONE;
    for (unsigned int axis = 0u; axis < t->rank; ++axis) {
        if (t->shape[axis].symbol != YVEX_IR_NONE && (axis || t->shape[axis].symbol != 0u))
            return physical_refuse(err, YVEX_ERR_FORMAT, "value contains an unlowered shape symbol");
        layout.dims[axis] = t->shape[axis].symbol == YVEX_IR_NONE ? t->shape[axis].extent : rows;
        if (!layout.dims[axis] || !yvex_core_u64_mul(layout.elements, layout.dims[axis], &layout.elements))
            return physical_refuse(err, YVEX_ERR_BOUNDS, "physical value extent overflowed");
    }
    if (!yvex_core_u64_mul(layout.elements, 4u, &layout.bytes))
        return physical_refuse(err, YVEX_ERR_BOUNDS, "physical value storage overflowed");
    *out = layout;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_program_physical_token_interface(const yvex_program_physical *p,
    yvex_program_token_interface *out, yvex_error *err)
{
    yvex_program_token_interface view = {0};
    const yvex_ir_type *tokens, *position, *hidden;
    unsigned char *returned = NULL;
    size_t i, embeddings = 0u;
    if (out) memset(out, 0, sizeof(*out));
    if (!p || !out || p->summary.input_count < 2u ||
        p->summary.result_count != p->summary.input_count - 1u) goto incompatible;
    tokens = &p->values[0].type;
    position = &p->values[1].type;
    hidden = &p->values[p->results[0]].type;
    if (tokens->kind != YVEX_IR_TENSOR || tokens->scalar != YVEX_IR_INDEX || tokens->rank != 1u ||
        position->kind != YVEX_IR_SCALAR || position->scalar != YVEX_IR_INDEX || position->rank ||
        hidden->kind != YVEX_IR_TENSOR ||
        (hidden->scalar != YVEX_IR_BF16 && hidden->scalar != YVEX_IR_F32) || hidden->rank != 2u ||
        hidden->shape[0].symbol != tokens->shape[0].symbol ||
        hidden->shape[0].extent != tokens->shape[0].extent ||
        hidden->shape[1].symbol != YVEX_IR_NONE) goto incompatible;
    view.hidden_width = hidden->shape[1].extent;
    view.state_inputs = p->summary.input_count - 2u;
    returned = calloc(p->summary.input_count, 1u);
    if (!returned) return physical_refuse(err, YVEX_ERR_NOMEM, "token interface state verification allocation failed");
    for (i = 1u; i < p->summary.result_count; ++i) {
        const yvex_program_physical_value *result = &p->values[p->results[i]];
        if (result->type.kind != YVEX_IR_STATE || result->definition == YVEX_IR_NONE ||
            result->state_root < 2u || result->state_root >= p->summary.input_count ||
            returned[result->state_root]) goto incompatible;
        returned[result->state_root] = 1u;
    }
    for (i = 2u; i < p->summary.input_count; ++i) {
        if (p->values[i].type.kind != YVEX_IR_STATE || !returned[i]) goto incompatible;
    }
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_physical_step *s = &p->steps[i];
        view.recurrent_operations += !strcmp(s->implementation, "gated_delta.bf16.f32state.v1");
        view.recurrent_operations += !strcmp(s->implementation, "selective_ssd.cpu.f32state.v1");
        view.attention_operations += !strcmp(s->implementation, "gated_causal.bf16.v1");
        if (!strcmp(s->implementation, "embedding.bf16.v1") ||
            !strcmp(s->implementation, "embedding.encoded.f32.v1")) {
            const yvex_program_physical_value *weight = &p->values[s->operands[1]];
            if (s->operands[0] != 0u || !weight->parameter) goto incompatible;
            if (!embeddings || weight->type.shape[0].extent < view.vocabulary_size)
                view.vocabulary_size = weight->type.shape[0].extent;
            embeddings++;
        }
    }
    if (!embeddings || !view.vocabulary_size || !view.hidden_width) goto incompatible;
    free(returned);
    *out = view;
    return YVEX_OK;
incompatible:
    free(returned);
    return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
        "program does not implement the admitted token-forward interface");
}

static int physical_seal(yvex_program_physical *, yvex_error *);

static const physical_rule *physical_rule_find(const char *name, int lowered)
{
    size_t i;
    for (i = 0u; name && i < sizeof(physical_rules) / sizeof(physical_rules[0]); ++i)
        if (!strcmp(name, lowered ? physical_rules[i].implementation : physical_rules[i].semantic))
            return &physical_rules[i];
    return NULL;
}

/* Implementation matching is a compiler decision. A semantic F32 result must
 * never silently use the BF16-publication implementation (or vice versa). */
static const physical_rule *physical_rule_select(const yvex_program_physical *p,
    const yvex_ir_operation *op, const yvex_ir_id *results, size_t result_count)
{
    const char *semantic = op->definition->name;
    if (!strcmp(semantic, "nn.linear"))
        for (unsigned int i = 0u; i < op->attribute_count; ++i)
            if (!strcmp(op->attributes[i].name, "reduction") &&
                op->attributes[i].value.integer == YVEX_IR_REDUCTION_ROW_DOT)
                return physical_rule_find("linear.row_dot.f32.v1", 1);
    if (!strcmp(semantic, "nn.linear") && result_count == 1u &&
        p->values[results[0]].type.scalar == YVEX_IR_F32)
        return physical_rule_find("linear.encoded.f32.v1", 1);
    if (result_count && p->values[results[0]].type.scalar == YVEX_IR_F32) {
        for (size_t i = 0u; i < sizeof(physical_rules) / sizeof(*physical_rules); ++i) {
            const physical_rule *r = physical_rules + i;
            size_t n = strlen(r->implementation);
            if (!strcmp(r->semantic, semantic) && n >= 7u && !strcmp(r->implementation + n - 7u, ".f32.v1"))
                return r;
        }
    }
    return physical_rule_find(semantic, 0);
}

static int physical_numeric_verify(const yvex_program_physical *p,
    const yvex_program_physical_step *s, yvex_error *err)
{
    unsigned int i;
    const physical_rule *rule = physical_rule_find(s->implementation, 1);
    int q8 = !strcmp(s->implementation, "linear.row_dot.q8.v1");
    int row_dot = q8 || !strcmp(s->implementation, "linear.row_dot.f32.v1");
    int encoded = row_dot || !strcmp(s->implementation, "linear.encoded.f32.v1") ||
        !strcmp(s->implementation, "embedding.encoded.f32.v1");
    int residual = !strcmp(s->implementation, "linear_residual.bf16.f32add.v1");
    size_t length = strlen(s->implementation);
    int f32 = !encoded && length >= 7u && !strcmp(s->implementation + length - 7u, ".f32.v1") &&
        strcmp(s->implementation, "grid_rotary.f32.v1");
    if (!rule) return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "physical operation implementation is unknown");
    /* Encoded parameters have no activation slot. An implementation must
     * explicitly consume that representation; type compatibility alone does
     * not make a constant into a materialized runtime tensor (or vice versa). */
    for (i = 0u; i < s->operand_count; ++i)
        if (!!p->values[s->operands[i]].parameter != !!(rule->parameter_operands & (1u << i)))
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "operand storage class requires another physical implementation");
    if (!strcmp(s->implementation, "parameter.encoded.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "selective_ssd.cpu.f32state.v1")) {
        for (i = 1u; i < 7u; ++i) {
            unsigned int qtype = p->values[s->operands[i]].qtype;
            if (qtype != YVEX_GGUF_QTYPE_F32 && qtype != YVEX_GGUF_QTYPE_BF16)
                return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                    "selective SSD parameters require losslessly decoded F32/BF16 source values");
        }
        return YVEX_OK; /* Semantic verification owns every geometry and state relation. */
    }
    if (!strcmp(s->implementation, "index_linearize.host.u32.v1")) {
        const yvex_ir_attribute *a = yvex_program_physical_attribute(s, "major_extent");
        const yvex_ir_attribute *b = yvex_program_physical_attribute(s, "minor_extent");
        unsigned long long domain;
        if (!a || !b || !yvex_core_u64_mul(a->value.integer, b->value.integer, &domain) ||
            !domain || domain - 1u > UINT_MAX)
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "index domain has no exact U32 representation");
        return YVEX_OK;
    }
    if (row_dot) {
        const yvex_ir_attribute *reduction = yvex_program_physical_attribute(s, "reduction");
        if (!reduction || reduction->kind != YVEX_IR_ATTR_U64 ||
            reduction->value.integer != YVEX_IR_REDUCTION_ROW_DOT ||
            p->values[s->operands[0]].type.scalar != YVEX_IR_F32 ||
            p->values[s->operands[1]].type.scalar != YVEX_IR_F32)
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "row-dot implementation requires F32 values and admitted encoded parameters");
        const yvex_program_physical_value *w = &p->values[s->operands[1]];
        if (q8 && (w->type.rank != 2u || w->type.shape[1].symbol != YVEX_IR_NONE ||
            w->type.shape[1].extent % 256u || (w->qtype != YVEX_GGUF_QTYPE_IQ2_XXS &&
            w->qtype != YVEX_GGUF_QTYPE_Q2_K && w->qtype != YVEX_GGUF_QTYPE_Q8_0 &&
            w->qtype != YVEX_GGUF_QTYPE_MXFP4)))
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "Q8 row-dot target requires an admitted 256-wide encoded activation geometry");
    }
    if (!row_dot && !strcmp(rule->semantic, "nn.linear") && yvex_program_physical_attribute(s, "reduction"))
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "linear implementation does not satisfy its reduction obligation");
    if (!strncmp(s->implementation, "linear_bias.cuda.sm121.", 22u)) {
        const yvex_ir_type *w = &p->values[s->operands[1]].type;
        unsigned long long out = !strcmp(s->implementation, "linear_bias.cuda.sm121.5376x96.f32.v1") ? 96u : 32u;
        if (w->rank != 2u || w->shape[0].extent != out || w->shape[1].extent != 5376u)
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "target affine implementation geometry is not admitted");
    }
    if (!strcmp(s->implementation, "cast.rne.bf16.v1") || !strcmp(s->implementation, "cast.exact.f32.v1")) {
        yvex_ir_scalar input = p->values[s->operands[0]].type.scalar;
        yvex_ir_scalar output = p->values[s->results[0]].type.scalar;
        return (input == YVEX_IR_F32 || input == YVEX_IR_BF16) &&
            output == (f32 ? YVEX_IR_F32 : YVEX_IR_BF16) ? YVEX_OK :
            physical_refuse(err, YVEX_ERR_UNSUPPORTED, "conversion requires admitted F32/BF16 storage and precision");
    }
    if (!strcmp(s->implementation, "reshape.f32-storage.v1") ||
        !strcmp(s->implementation, "observe.f32-storage.v1")) {
        yvex_ir_scalar scalar = p->values[s->operands[0]].type.scalar;
        return scalar == YVEX_IR_F32 || scalar == YVEX_IR_BF16 ? YVEX_OK :
            physical_refuse(err, YVEX_ERR_UNSUPPORTED, "tensor view has no admitted scalar storage");
    }
    if (!strcmp(s->implementation, "stream_mean.f32.f64acc.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "clamped_swiglu.f64math.bf16.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "axis_rotary.f32math.bf16.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "grid_rotary.f32.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "rotary_half.f32acc.bf16.v1")) {
        return p->values[s->operands[0]].type.scalar == YVEX_IR_BF16 ? YVEX_OK :
            physical_refuse(err, YVEX_ERR_UNSUPPORTED, "F32-table rotary currently publishes BF16");
    }
    if (!strcmp(s->implementation, "mhc.residual_post.f64acc.bf16.v1") ||
        !strcmp(s->implementation, "mhc.residual_pre.bf16.v1")) return YVEX_OK;
    if (!strcmp(s->implementation, "mhc.head_norm.bf16.v1"))
        return YVEX_OK; /* The semantic verifier checks every scalar and shape. */
    if (!strcmp(s->implementation, "weighted_rms.f64scale.bf16.v1")) {
        unsigned int qtype = p->values[s->operands[1]].qtype;
        return qtype == YVEX_GGUF_QTYPE_F32 || qtype == YVEX_GGUF_QTYPE_BF16 ? YVEX_OK :
            physical_refuse(err, YVEX_ERR_UNSUPPORTED, "weighted RMS requires losslessly decoded F32/BF16 weights");
    }
    if (!strcmp(s->implementation, "swiglu_split.bf16.v1") &&
        !yvex_program_physical_attribute(s, "gate_first")->value.integer)
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "BF16 packed SwiGLU implementation requires gate-first layout");
    if (!strcmp(s->implementation, "group_rms_norm.bf16.vector4.v1") &&
        p->values[s->operands[1]].type.shape[0].extent % 4u)
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "vector-four normalization requires complete groups of four channels");
    if (!strcmp(s->implementation, "embedding.encoded.f32.v1") &&
        (s->operand_count != 2u || s->result_count != 1u ||
         p->values[s->operands[0]].type.rank != 1u ||
         p->values[s->operands[1]].type.rank != 2u ||
         p->values[s->results[0]].type.rank != 2u))
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "encoded embedding requires indices, an immutable matrix and matrix output");
    if (((encoded && strcmp(s->implementation, "embedding.encoded.f32.v1")) || residual) &&
        (s->operand_count != (residual ? 3u : 2u) || s->result_count != 1u ||
        p->values[s->operands[0]].type.rank != 2u ||
        p->values[s->results[0]].type.rank != 2u))
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
            "encoded linear requires a matrix input and an immutable parameter");
    for (i = 0u; i < s->operand_count; ++i) {
        const yvex_program_physical_value *v = &p->values[s->operands[i]];
        if (v->type.kind != YVEX_IR_TENSOR || v->type.scalar == YVEX_IR_INDEX) continue;
        if (!strcmp(s->implementation, "attention.full.f32.v1") &&
            (v->type.scalar == YVEX_IR_BF16 || v->type.scalar == YVEX_IR_F32)) continue;
        int decoded_norm = !strcmp(s->implementation, "rms_norm.f32.v1") &&
            (v->qtype == YVEX_GGUF_QTYPE_F32 || v->qtype == YVEX_GGUF_QTYPE_BF16);
        if ((!encoded && !decoded_norm &&
             v->type.scalar != (f32 ? YVEX_IR_F32 : YVEX_IR_BF16)) ||
            (v->parameter && !encoded && !decoded_norm &&
             v->qtype != (f32 ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16)))
            {
                yvex_error_setf(err, YVEX_ERR_UNSUPPORTED,
                    "compiler.program.physical",
                    "operand precision requires another physical implementation: %s",
                    s->implementation);
                return YVEX_ERR_UNSUPPORTED;
            }
    }
    for (i = 0u; i < s->result_count; ++i) {
        const yvex_program_physical_value *v = &p->values[s->results[i]];
        if (v->type.kind == YVEX_IR_TENSOR && v->type.scalar != (encoded || f32 ? YVEX_IR_F32 : YVEX_IR_BF16))
            return physical_refuse(err, YVEX_ERR_FORMAT,
                "physical implementation does not preserve result precision");
    }
    return YVEX_OK;
}

static int physical_allocate(yvex_program_physical **out, const yvex_program_physical_summary *s,
                             yvex_error *err)
{
    yvex_program_physical *p;
    if (!s->input_count || s->input_count > s->value_count || !s->step_count || !s->result_count ||
        s->value_count > PHYSICAL_LIMIT || s->step_count > PHYSICAL_LIMIT ||
        s->result_count > s->value_count || s->storage_count > s->value_count)
        return physical_refuse(err, YVEX_ERR_BOUNDS, "physical program population exceeds its envelope");
    p = calloc(1u, sizeof(*p));
    if (!p) return physical_refuse(err, YVEX_ERR_NOMEM, "physical program owner allocation failed");
    atomic_init(&p->references, 1u);
    p->summary = *s;
    p->values = calloc(s->value_count, sizeof(*p->values));
    p->steps = calloc(s->step_count, sizeof(*p->steps));
    p->results = calloc(s->result_count, sizeof(*p->results));
    if (!p->values || !p->steps || !p->results) {
        yvex_program_physical_close(&p);
        return physical_refuse(err, YVEX_ERR_NOMEM, "physical program records allocation failed");
    }
    *out = p;
    return YVEX_OK;
}

static int physical_type_equal(const yvex_ir_type *a, const yvex_ir_type *b)
{
    unsigned int i;
    if (a->kind != b->kind || a->scalar != b->scalar || a->rank != b->rank ||
        strcmp(a->domain, b->domain)) return 0;
    for (i = 0u; i < a->rank; ++i)
        if (a->shape[i].symbol != b->shape[i].symbol || a->shape[i].extent != b->shape[i].extent) return 0;
    return 1;
}

static int physical_state_roots_verify(const yvex_program_physical *p, yvex_error *err)
{
    size_t i, j, k;
    for (i = 0u; i < p->summary.value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        if ((v->type.kind != YVEX_IR_STATE && v->state_root != YVEX_IR_NONE) ||
            (v->type.kind == YVEX_IR_STATE && i < p->summary.input_count && v->state_root != i))
            return physical_refuse(err, YVEX_ERR_FORMAT, "physical state input lifetime is inconsistent");
    }
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_physical_step *step = &p->steps[i];
        for (j = 0u; j < step->result_count; ++j) {
            const yvex_program_physical_value *v = &p->values[step->results[j]];
            size_t matches = 0u;
            if (v->type.kind != YVEX_IR_STATE) continue;
            for (k = 0u; k < step->operand_count; ++k) {
                const yvex_program_physical_value *input = &p->values[step->operands[k]];
                if (!physical_type_equal(&input->type, &v->type)) continue;
                if (input->state_root != v->state_root)
                    return physical_refuse(err, YVEX_ERR_FORMAT, "physical state successor changes its lifetime");
                matches++;
            }
            if (matches != 1u || v->state_root >= p->summary.input_count)
                return physical_refuse(err, YVEX_ERR_FORMAT, "physical state successor lacks a unique input lifetime");
        }
    }
    return YVEX_OK;
}

static void physical_state_clear(yvex_program_physical *p)
{
    for (size_t i = 0u; p->delta && i < p->summary.step_count; ++i) free(p->delta[i]);
    for (size_t i = 0u; p->ssd && i < p->summary.step_count; ++i) free(p->ssd[i]);
    free(p->sequence);
    free(p->delta);
    free(p->ssd);
    p->sequence = NULL;
    p->delta = NULL;
    p->ssd = NULL;
    p->sequence_count = 0u;
}

static int physical_state_lower(yvex_program_physical *p, yvex_error *err)
{
    size_t i;
    /* Target specialization re-verifies an imported program. Replace its
     * derived provider views, never overwrite their owning allocations. */
    physical_state_clear(p);
    p->delta = calloc(p->summary.step_count, sizeof(*p->delta));
    p->ssd = calloc(p->summary.step_count, sizeof(*p->ssd));
    p->sequence = calloc(p->summary.input_count, sizeof(*p->sequence));
    if (!p->delta || !p->ssd || !p->sequence)
        return physical_refuse(err, YVEX_ERR_NOMEM, "state implementation projection allocation failed");
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_physical_step *s = &p->steps[i];
        yvex_gated_delta_requirement r = {.schema_version = YVEX_SEQUENCE_MIXER_GATED_DELTA_SCHEMA_V2,
            .projected_dtype = YVEX_DTYPE_F32, .convolution_state_dtype = YVEX_DTYPE_F32,
            .recurrent_state_dtype = YVEX_DTYPE_F32, .accumulation_dtype = YVEX_DTYPE_F32,
            .output_dtype = YVEX_DTYPE_F32, .numeric_contract = YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
            .output_normalization_weight_convention = YVEX_NORMALIZATION_WEIGHT_DIRECT, .deterministic = 1};
        yvex_gated_delta_plan *d;
        yvex_ir_id root;
        int rc;
        if (!strcmp(s->implementation, "selective_ssd.cpu.f32state.v1")) {
            yvex_selective_ssd_requirement requirement = {.schema_version = YVEX_SELECTIVE_SSD_SCHEMA_V1};
            yvex_selective_ssd_geometry *geometry = p->ssd[i] = calloc(1u, sizeof(*geometry));
            if (!geometry) return physical_refuse(err, YVEX_ERR_NOMEM, "SSD implementation allocation failed");
            requirement.heads = yvex_program_physical_attribute(s, "heads")->value.integer;
            requirement.head_dimension = yvex_program_physical_attribute(s, "head_dimension")->value.integer;
            requirement.state_dimension = yvex_program_physical_attribute(s, "state_dimension")->value.integer;
            requirement.groups = yvex_program_physical_attribute(s, "groups")->value.integer;
            requirement.convolution_kernel =
                yvex_program_physical_attribute(s, "convolution_kernel")->value.integer;
            requirement.normalization_groups =
                yvex_program_physical_attribute(s, "normalization_groups")->value.integer;
            requirement.normalization_epsilon = yvex_program_physical_attribute(s, "epsilon")->value.real;
            requirement.time_step_minimum =
                yvex_program_physical_attribute(s, "time_step_minimum")->value.real;
            requirement.time_step_maximum =
                yvex_program_physical_attribute(s, "time_step_maximum")->value.real;
            requirement.time_step_unbounded =
                (int)yvex_program_physical_attribute(s, "time_step_unbounded")->value.integer;
            requirement.norm_before_gate =
                (int)yvex_program_physical_attribute(s, "norm_before_gate")->value.integer;
            root = p->values[s->operands[7]].state_root;
            rc = yvex_selective_ssd_geometry_seal(geometry, &requirement, err);
            if (rc == YVEX_OK) rc = yvex_sequence_state_binding_seal(
                &p->sequence[p->sequence_count], root, geometry->convolution_state_values,
                geometry->recurrent_state_values, geometry->identity, err);
            if (rc != YVEX_OK) return rc;
            p->sequence_count++;
            continue;
        }
        if (strcmp(s->implementation, "gated_delta.bf16.f32state.v1")) continue;
        r.query_heads = r.key_heads = yvex_program_physical_attribute(s, "key_heads")->value.integer;
        r.value_heads = yvex_program_physical_attribute(s, "value_heads")->value.integer;
        r.key_head_dimension = yvex_program_physical_attribute(s, "key_dimension")->value.integer;
        r.value_head_dimension = yvex_program_physical_attribute(s, "value_dimension")->value.integer;
        r.convolution_kernel = yvex_program_physical_attribute(s, "convolution_kernel")->value.integer;
        r.qk_normalization_epsilon = yvex_program_physical_attribute(s, "qk_epsilon")->value.real;
        r.output_normalization_epsilon = yvex_program_physical_attribute(s, "epsilon")->value.real;
        r.query_scale = yvex_program_physical_attribute(s, "query_scale")->value.real;
        d = p->delta[i] = calloc(1u, sizeof(*d));
        if (!d) return physical_refuse(err, YVEX_ERR_NOMEM, "state implementation allocation failed");
        root = p->values[s->operands[8]].state_root;
        rc = yvex_gated_delta_plan_seal(d, &r, err);
        if (rc == YVEX_OK) rc = yvex_sequence_state_binding_seal(&p->sequence[p->sequence_count], root,
            d->convolution_state_values, d->recurrent_state_values, d->identity, err);
        if (rc != YVEX_OK) return rc;
        p->sequence_count++;
    }
    return YVEX_OK;
}

static int physical_state_transactions_verify(const yvex_program_physical *p, yvex_error *err)
{
    unsigned char *written = calloc(p->summary.input_count, 1u);
    size_t i, j;
    int rc = YVEX_OK;
    if (!written) return physical_refuse(err, YVEX_ERR_NOMEM, "state legalization allocation failed");
    /* Current providers stage one successor per root per invocation. Multiple
     * SSA transitions remain valid semantic IR, but require a different physical
     * implementation that can read its preceding candidate, not committed state. */
    for (i = 0u; rc == YVEX_OK && i < p->summary.step_count; ++i)
        for (j = 0u; rc == YVEX_OK && j < p->steps[i].result_count; ++j) {
            const yvex_program_physical_value *v = &p->values[p->steps[i].results[j]];
            if (v->type.kind != YVEX_IR_STATE) continue;
            if (written[v->state_root]) rc = physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                "multiple transitions of one state lifetime require another physical legalization");
            written[v->state_root] = 1u;
        }
    free(written);
    return rc;
}

static int physical_is_result(const yvex_program_physical *p, size_t value)
{
    size_t i;
    for (i = 0u; i < p->summary.result_count; ++i)
        if (p->results[i] == value) return 1;
    return 0;
}

/* Storage is an explicit compiler decision, not an inference performed during
 * invocation. Values alive at the same instruction never share writable slots.
 * Outputs stay caller-owned; state versions retain their original state root. */
static int physical_storage_plan(yvex_program_physical *p, int verify, yvex_error *err)
{
    yvex_ir_id *owners;
    size_t i, count = 0u;
    owners = malloc(p->summary.value_count * sizeof(*owners));
    if (!owners) return physical_refuse(err, YVEX_ERR_NOMEM, "storage planning allocation failed");
    for (i = 0u; i < p->summary.value_count; ++i) {
        yvex_program_physical_value *v = &p->values[i];
        yvex_ir_id slot = YVEX_IR_NONE;
        if (i >= p->summary.input_count && !v->parameter && v->type.kind == YVEX_IR_TENSOR &&
            !physical_is_result(p, i)) {
            size_t j;
            for (j = 0u; j < count; ++j) {
                const yvex_program_physical_value *prior = &p->values[owners[j]];
                if (prior->last_use < v->definition && physical_type_equal(&prior->type, &v->type)) break;
            }
            if (j == count) count++;
            slot = (yvex_ir_id)j;
            owners[j] = (yvex_ir_id)i;
        }
        if (verify && v->storage != slot) {
            free(owners);
            return physical_refuse(err, YVEX_ERR_FORMAT, "physical storage aliases live computational values");
        }
        if (!verify) v->storage = slot;
    }
    free(owners);
    if (verify && p->summary.storage_count != count)
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical storage population is inconsistent");
    if (!verify) p->summary.storage_count = count;
    return YVEX_OK;
}

static int physical_values_lower(yvex_program_physical *p, const yvex_ir_module *m,
    const yvex_program_entry *entry, const yvex_program_parameter_binding *bindings,
    size_t binding_count, yvex_error *err)
{
    yvex_ir_id population = YVEX_IR_NONE;
    size_t i;
    for (i = 0u; i < entry->value_count; ++i) {
        yvex_program_physical_value *v = &p->values[i];
        const yvex_ir_value *logical = yvex_ir_value_at(m, entry->values[i].semantic_value);
        const yvex_ir_type *type = logical ? yvex_ir_type_at(m, logical->type) : NULL;
        const yvex_ir_operation *producer = logical ? yvex_ir_operation_at(m, logical->definition) : NULL;
        unsigned int axis;
        if (!type || type->member_count || (type->kind != YVEX_IR_TENSOR &&
            type->kind != YVEX_IR_STATE && type->kind != YVEX_IR_SCALAR) ||
            (type->kind == YVEX_IR_TENSOR && type->scalar != YVEX_IR_BF16 && type->scalar != YVEX_IR_F32 &&
             !(type->scalar == YVEX_IR_INDEX && type->rank == 1u)) ||
            (type->kind == YVEX_IR_SCALAR && type->scalar != YVEX_IR_INDEX))
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "value type has no physical execution representation");
        v->type = *type;
        v->definition = entry->values[i].definition;
        v->last_use = entry->values[i].last_use;
        v->storage = v->state_root = YVEX_IR_NONE;
        v->tensor_id = ULLONG_MAX;
        if (type->kind == YVEX_IR_STATE && i < entry->input_count) v->state_root = (yvex_ir_id)i;
        for (axis = 0u; axis < type->rank; ++axis) {
            const yvex_ir_extent *extent = &type->shape[axis];
            if (extent->symbol != YVEX_IR_NONE) {
                const yvex_ir_dimension *d = yvex_ir_dimension_at(m, extent->symbol);
                if (axis || !d || type->kind != YVEX_IR_TENSOR ||
                    (population != YVEX_IR_NONE && population != extent->symbol))
                    return physical_refuse(err, YVEX_ERR_UNSUPPORTED,
                        "physical execution requires one shared row population");
                population = extent->symbol;
                p->summary.minimum_rows = d->minimum;
                p->summary.maximum_rows = d->maximum;
                p->summary.row_multiple = d->multiple;
                v->type.shape[axis].symbol = 0u;
            }
        }
        if (producer && !strcmp(producer->definition->name, "core.parameter")) {
            size_t j, matches = 0u;
            v->parameter = 1;
            for (j = 0u; j < binding_count; ++j)
                if (bindings[j].semantic_value == entry->values[i].semantic_value) {
                    v->tensor_id = bindings[j].tensor_id;
                    v->qtype = bindings[j].qtype;
                    matches++;
                }
            if (matches != 1u || v->tensor_id == ULLONG_MAX)
                return physical_refuse(err, YVEX_ERR_FORMAT, "constant requires one exact physical parameter binding");
        }
    }
    /* A fixed-shape component is one invocation, not a fictitious dynamic
     * batch. Each instruction still derives its actual population from its
     * verified tensor geometry. No runtime dimension may resize this entry. */
    if (population == YVEX_IR_NONE)
        p->summary.minimum_rows = p->summary.maximum_rows = p->summary.row_multiple = 1u;
    return YVEX_OK;
}

static int physical_steps_lower(yvex_program_physical *p, const yvex_ir_module *m,
    const yvex_program_entry *entry, yvex_error *err)
{
    size_t i, j;
    for (i = 0u; i < p->summary.step_count; ++i) {
        const yvex_program_step *source = &entry->steps[i];
        const yvex_ir_operation *op = yvex_ir_operation_at(m, source->semantic_operation);
        const physical_rule *rule = op ? physical_rule_select(p, op,
            source->results, source->result_count) : NULL;
        yvex_program_physical_step *step = &p->steps[i];
        int parameter = rule && !strcmp(rule->semantic, "core.parameter");
        if (!rule || op->definition->version != 1u ||
            source->operand_count > YVEX_PROGRAM_OPERAND_CAP || source->result_count > YVEX_PROGRAM_RESULT_CAP ||
            (!parameter && op->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP) ||
            yvex_ir_operation_effects(m, source->semantic_operation) != rule->effects)
            return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "operation lacks an admitted physical implementation");
        step->implementation = rule->implementation;
        step->effects = rule->effects;
        step->operand_count = (unsigned int)source->operand_count;
        step->result_count = (unsigned int)source->result_count;
        step->attribute_count = parameter ? 0u : op->attribute_count;
        memcpy(step->operands, source->operands, source->operand_count * sizeof(*step->operands));
        memcpy(step->results, source->results, source->result_count * sizeof(*step->results));
        if (step->attribute_count)
            memcpy(step->attributes, op->attributes, step->attribute_count * sizeof(*step->attributes));
        for (j = 0u; j < source->result_count; ++j) {
            yvex_program_physical_value *result = &p->values[source->results[j]];
            size_t k, matches = 0u;
            if (result->type.kind != YVEX_IR_STATE) continue;
            for (k = 0u; k < source->operand_count; ++k) {
                const yvex_program_physical_value *input = &p->values[source->operands[k]];
                if (physical_type_equal(&input->type, &result->type)) {
                    result->state_root = input->state_root;
                    matches++;
                }
            }
            if (matches != 1u || result->state_root == YVEX_IR_NONE)
                return physical_refuse(err, YVEX_ERR_FORMAT, "state result requires an unambiguous input lifetime");
        }
    }
    return YVEX_OK;
}

/* Reuse the operation/type/state verifiers at the physical trust boundary.
 * This transient verification object is discarded before execution; it never
 * legalizes work, imports a family, or becomes a second retained program. */
static int physical_verify(yvex_program_physical *p, yvex_error *err)
{
    const yvex_program_physical_summary *s = &p->summary;
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect(), *yvex_ir_sequence_dialect()};
    yvex_ir_dimension rows = {.name = "rows", .minimum = s->minimum_rows,
        .maximum = s->maximum_rows, .multiple = s->row_multiple};
    yvex_ir_module *m = NULL;
    yvex_ir_id dimension, function, block = YVEX_IR_NONE, opid;
    yvex_ir_id *types = NULL, *values = NULL, *results = NULL;
    yvex_ir_id *last = NULL;
    size_t i, j, defined = s->input_count;
    unsigned int effects = 0u;
    int rc;
    if (!s->entry[0] || !yvex_sha256_hex_valid(s->semantic_identity) ||
        !yvex_sha256_hex_valid(s->execution_identity) || !yvex_sha256_hex_valid(s->parameter_identity) ||
        s->maximum_rows > INT_MAX)
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical program identity/envelope is invalid");
    rc = yvex_ir_module_open(&m, "physical_verifier", s->semantic_identity, dialects, 3u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dimension, err);
    types = malloc(s->value_count * sizeof(*types));
    values = malloc(s->value_count * sizeof(*values));
    last = malloc(s->value_count * sizeof(*last));
    results = malloc(s->result_count * sizeof(*results));
    if (rc == YVEX_OK && (!types || !values || !last || !results))
        rc = physical_refuse(err, YVEX_ERR_NOMEM, "physical verification storage allocation failed");
    for (i = 0u; rc == YVEX_OK && i < s->value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        last[i] = YVEX_IR_NONE;
        if (v->type.rank > YVEX_IR_RANK_CAP || v->type.member_count ||
            (v->type.kind != YVEX_IR_TENSOR && v->type.kind != YVEX_IR_STATE && v->type.kind != YVEX_IR_SCALAR) ||
            (v->type.kind == YVEX_IR_TENSOR && v->type.scalar != YVEX_IR_BF16 && v->type.scalar != YVEX_IR_F32 &&
             !(v->type.scalar == YVEX_IR_INDEX && v->type.rank == 1u)) ||
            (v->type.kind == YVEX_IR_SCALAR && v->type.scalar != YVEX_IR_INDEX) ||
            (v->parameter != 0 && v->parameter != 1) ||
            (v->parameter && (!v->type.rank || v->type.kind != YVEX_IR_TENSOR ||
                              !yvex_gguf_qtype_geometry_find(v->qtype) || v->tensor_id == ULLONG_MAX)) ||
            (!v->parameter && (v->tensor_id != ULLONG_MAX || v->qtype)) ||
            (i < s->input_count && (v->parameter || v->definition != YVEX_IR_NONE))) {
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value storage/definition is invalid");
            break;
        }
        for (j = 0u; j < v->type.rank; ++j)
            if (v->type.shape[j].symbol != YVEX_IR_NONE &&
                (j || v->type.shape[j].symbol != 0u || v->parameter || v->type.kind != YVEX_IR_TENSOR))
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value has an unlowered shape symbol");
        if (rc == YVEX_OK) rc = yvex_ir_type_intern(m, &v->type, &types[i], err);
        if (rc == YVEX_OK && !v->parameter && v->type.kind == YVEX_IR_TENSOR) {
            yvex_program_value_layout layout;
            rc = yvex_program_physical_value_layout(p, i, s->maximum_rows, &layout, err);
        }
    }
    for (i = 0u; rc == YVEX_OK && i < s->result_count; ++i) {
        if (p->results[i] >= s->value_count || p->values[p->results[i]].parameter ||
            (p->values[p->results[i]].type.kind == YVEX_IR_TENSOR &&
             p->values[p->results[i]].type.scalar == YVEX_IR_INDEX))
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical entry result is invalid");
        else results[i] = types[p->results[i]];
        for (j = 0u; rc == YVEX_OK && j < i; ++j)
            if (p->results[j] == p->results[i])
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "duplicate external result requires explicit copy lowering");
    }
    for (i = 0u; i < s->step_count; ++i) effects |= p->steps[i].effects;
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, s->entry, types, s->input_count,
        results, s->result_count, effects, &function, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, function)->body;
        memcpy(values, yvex_ir_block_at(m, block)->arguments, s->input_count * sizeof(*values));
    }
    for (i = 0u; rc == YVEX_OK && i < s->step_count; ++i) {
        const yvex_program_physical_step *step = &p->steps[i];
        const physical_rule *rule = physical_rule_find(step->implementation, 1);
        yvex_ir_id args[YVEX_PROGRAM_OPERAND_CAP], output[YVEX_PROGRAM_RESULT_CAP];
        yvex_ir_attribute attrs[2] = {{.name = "source", .kind = YVEX_IR_ATTR_TEXT},
                                    {.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL}};
        yvex_ir_operation_request r = {.operands = args, .result_types = output,
            .operand_count = step->operand_count, .result_count = step->result_count,
            .attributes = step->attributes, .attribute_count = step->attribute_count};
        if (!rule || step->effects != rule->effects || step->operand_count > YVEX_PROGRAM_OPERAND_CAP ||
            step->result_count > YVEX_PROGRAM_RESULT_CAP || step->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical operation/effects are not admitted");
        if (rc != YVEX_OK) break;
        r.operation = rule->semantic;
        for (j = 0u; rc == YVEX_OK && j < step->operand_count; ++j) {
            if (step->operands[j] >= defined)
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical operand has no preceding definition");
            else { args[j] = values[step->operands[j]]; last[step->operands[j]] = (yvex_ir_id)i; }
        }
        for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j) {
            if (defined >= s->value_count || step->results[j] != defined || p->values[defined].definition != i)
                rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical result definition is not unique");
            else output[j] = types[defined++];
        }
        if (rc == YVEX_OK && !strcmp(rule->semantic, "core.parameter")) {
            if (step->result_count != 1u || step->attribute_count ||
                !p->values[step->results[0]].parameter) rc = YVEX_ERR_FORMAT;
            if (rc == YVEX_OK) {
                yvex_core_text_copy(attrs[0].value.text, sizeof(attrs[0].value.text), s->parameter_identity);
                (void)snprintf(attrs[1].value.text, sizeof(attrs[1].value.text), "tensor_%llu",
                               p->values[step->results[0]].tensor_id);
                r.attributes = attrs;
                r.attribute_count = 2u;
            }
        } else for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j)
            if (p->values[step->results[j]].parameter) rc = YVEX_ERR_FORMAT;
        if (rc == YVEX_OK) rc = yvex_ir_operation_add(m, block, &r, &opid, err);
        if (rc == YVEX_OK) rc = physical_numeric_verify(p, step, err);
        for (j = 0u; rc == YVEX_OK && j < step->result_count; ++j)
            values[step->results[j]] = yvex_ir_operation_at(m, opid)->results[j];
    }
    if (rc == YVEX_OK && defined != s->value_count)
        rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical program contains undefined values");
    for (i = 0u; rc == YVEX_OK && i < s->result_count; ++i) {
        results[i] = values[p->results[i]];
        last[p->results[i]] = (yvex_ir_id)s->step_count;
    }
    for (i = 0u; rc == YVEX_OK && i < s->value_count; ++i)
        if (last[i] != p->values[i].last_use)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "physical value lifetime differs from its uses");
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results,
            .operand_count = (uint32_t)s->result_count};
        rc = yvex_ir_operation_add(m, block, &r, &opid, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = physical_state_roots_verify(p, err);
    if (rc == YVEX_OK) rc = physical_state_transactions_verify(p, err);
    if (rc == YVEX_OK) rc = physical_storage_plan(p, 1, err);
    if (rc == YVEX_OK) rc = physical_state_lower(p, err);
    free(results);
    free(last);
    free(values);
    free(types);
    yvex_ir_module_close(&m);
    return rc;
}

int yvex_program_physical_compile(yvex_program_physical **out, const yvex_program_execution *execution,
    const char *symbol, const yvex_program_parameter_binding *bindings, size_t binding_count,
    const char *parameter_identity, yvex_error *err)
{
    const yvex_ir_module *m = yvex_program_execution_module(execution);
    const yvex_program_entry *entry = NULL;
    yvex_program_physical *p = NULL;
    yvex_program_physical_summary s = {0};
    size_t i;
    int rc;
    if (out) *out = NULL;
    if (!out || !m || !symbol || !yvex_sha256_hex_valid(parameter_identity) || (binding_count && !bindings))
        return physical_refuse(err, YVEX_ERR_INVALID_ARG, "verified execution and exact parameter bindings required");
    for (i = 0u; i < yvex_program_execution_entry_count(execution); ++i) {
        const yvex_program_entry *e = yvex_program_execution_entry_at(execution, i);
        if (!strcmp(e->symbol, symbol)) entry = e;
    }
    if (!entry || entry->step_count < 2u)
        return physical_refuse(err, YVEX_ERR_UNSUPPORTED, "physical entry has no admitted computation");
    yvex_core_text_copy(s.entry, sizeof(s.entry), symbol);
    yvex_core_text_copy(s.semantic_identity, sizeof(s.semantic_identity), yvex_ir_identity(m));
    yvex_core_text_copy(s.execution_identity, sizeof(s.execution_identity), yvex_program_execution_identity(execution));
    yvex_core_text_copy(s.parameter_identity, sizeof(s.parameter_identity), parameter_identity);
    s.input_count = entry->input_count;
    s.value_count = entry->value_count;
    s.step_count = entry->step_count - 1u;
    s.result_count = entry->result_count;
    rc = physical_allocate(&p, &s, err);
    if (rc == YVEX_OK) {
        memcpy(p->results, entry->results, s.result_count * sizeof(*p->results));
        rc = physical_values_lower(p, m, entry, bindings, binding_count, err);
    }
    if (rc == YVEX_OK) rc = physical_steps_lower(p, m, entry, err);
    if (rc == YVEX_OK) rc = physical_storage_plan(p, 0, err);
    if (rc == YVEX_OK) rc = physical_verify(p, err);
    if (rc == YVEX_OK) rc = physical_seal(p, err);
    if (rc == YVEX_OK) *out = p;
    else yvex_program_physical_close(&p);
    return rc;
}

yvex_program_physical *yvex_program_physical_retain(const yvex_program_physical *sealed, yvex_error *err)
{
    yvex_program_physical *p = (yvex_program_physical *)sealed;
    unsigned int count;
    if (!p || !yvex_sha256_hex_valid(p->summary.identity)) {
        physical_refuse(err, YVEX_ERR_STATE, "only sealed physical programs may cross owner lifetimes");
        return NULL;
    }
    count = atomic_load_explicit(&p->references, memory_order_relaxed);
    do {
        if (!count || count == UINT_MAX) {
            physical_refuse(err, YVEX_ERR_BOUNDS, "physical program reference population overflow");
            return NULL;
        }
    } while (!atomic_compare_exchange_weak_explicit(&p->references, &count, count + 1u,
        memory_order_relaxed, memory_order_relaxed));
    return p;
}

int yvex_program_physical_target_compile(yvex_program_physical **out, const yvex_program_physical *source,
    const yvex_program_target_choice *choices, size_t count, yvex_error *err)
{
    yvex_program_physical *p = NULL;
    yvex_core_bytes wire = {.maximum = PHYSICAL_BYTES};
    if (out) *out = NULL;
    if (!out || !source || !choices || !count || count > source->summary.step_count)
        return physical_refuse(err, YVEX_ERR_INVALID_ARG, "target lowering requires bounded explicit choices");
    int rc = yvex_program_physical_encode(source, &wire, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_decode(&p, wire.data, wire.count, err);
    free(wire.data);
    for (size_t i = 0u; rc == YVEX_OK && i < count; ++i) {
        const physical_rule *selected = physical_rule_find(choices[i].implementation, 1);
        if (choices[i].step >= p->summary.step_count || !selected) {
            rc = physical_refuse(err, YVEX_ERR_UNSUPPORTED, "target choice does not name admitted work");
            break;
        }
        for (size_t j = 0u; j < i; ++j) if (choices[j].step == choices[i].step)
            rc = physical_refuse(err, YVEX_ERR_FORMAT, "one operation has conflicting target choices");
        yvex_program_physical_step *step = p->steps + choices[i].step;
        const physical_rule *prior = physical_rule_find(step->implementation, 1);
        if (!prior || strcmp(prior->semantic, selected->semantic) || prior->effects != selected->effects ||
            prior->parameter_operands != selected->parameter_operands)
            rc = physical_refuse(err, YVEX_ERR_FORMAT,
                "target specialization cannot change computation or storage classes");
        if (rc == YVEX_OK) step->implementation = selected->implementation;
    }
    if (rc == YVEX_OK) rc = physical_verify(p, err);
    if (rc == YVEX_OK) rc = physical_seal(p, err);
    if (rc == YVEX_OK) *out = p;
    else yvex_program_physical_close(&p);
    return rc;
}

void yvex_program_physical_close(yvex_program_physical **owner)
{
    yvex_program_physical *p = owner ? *owner : NULL;
    if (!p) return;
    *owner = NULL;
    if (atomic_fetch_sub_explicit(&p->references, 1u, memory_order_acq_rel) != 1u) return;
    physical_state_clear(p);
    free(p->values);
    free(p->steps);
    free(p->results);
    free(p);
}

const yvex_program_physical_summary *yvex_program_physical_summary_get(const yvex_program_physical *p)
{ return p ? &p->summary : NULL; }
const yvex_program_physical_value *yvex_program_physical_value_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.value_count ? &p->values[i] : NULL; }
const yvex_program_physical_step *yvex_program_physical_step_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.step_count ? &p->steps[i] : NULL; }
unsigned long long yvex_program_physical_step_population(const yvex_program_physical *p, size_t i,
    unsigned long long rows)
{
    const yvex_program_physical_step *s = yvex_program_physical_step_at(p, i);
    if (!s) return 0u;
    for (unsigned int j = 0u; j < s->result_count; ++j) {
        const yvex_ir_type *t = &p->values[s->results[j]].type;
        if (t->kind == YVEX_IR_TENSOR && t->rank)
            return t->shape[0].symbol == YVEX_IR_NONE ? t->shape[0].extent : rows;
    }
    return rows;
}
yvex_ir_id yvex_program_physical_result_at(const yvex_program_physical *p, size_t i)
{ return p && i < p->summary.result_count ? p->results[i] : YVEX_IR_NONE; }
const yvex_ir_attribute *yvex_program_physical_attribute(const yvex_program_physical_step *s, const char *name)
{
    unsigned int i;
    for (i = 0u; s && name && i < s->attribute_count; ++i)
        if (!strcmp(s->attributes[i].name, name)) return &s->attributes[i];
    return NULL;
}

const yvex_gated_delta_plan *yvex_program_physical_delta_at(const yvex_program_physical *p, size_t i)
{
    return p && p->delta && i < p->summary.step_count ? p->delta[i] : NULL;
}

const yvex_selective_ssd_geometry *yvex_program_physical_ssd_at(
    const yvex_program_physical *p, size_t i)
{
    return p && p->ssd && i < p->summary.step_count ? p->ssd[i] : NULL;
}

int yvex_program_physical_sequence_state(const yvex_program_physical *p, yvex_sequence_state_plan *out)
{
    if (!p || !out) return 0;
    *out = (yvex_sequence_state_plan){YVEX_SEQUENCE_STATE_SCHEMA_V1, p->sequence, p->sequence_count};
    return 1;
}

static int physical_put(yvex_core_bytes *b, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int i;
    for (i = 0u; i < 8u; ++i) bytes[i] = (unsigned char)(value >> (8u * i));
    return yvex_core_bytes_append(b, bytes, sizeof(bytes));
}

static int physical_text(yvex_core_bytes *b, const char *text)
{
    size_t n = strlen(text);
    return physical_put(b, n) && yvex_core_bytes_append(b, text, n);
}

static int physical_value_write(yvex_core_bytes *b, const yvex_program_physical_value *v)
{
    unsigned int i;
    int ok = physical_put(b, v->type.kind) && physical_put(b, v->type.scalar) &&
        physical_put(b, v->type.rank) && physical_text(b, v->type.domain) &&
        physical_put(b, v->definition) && physical_put(b, v->last_use) &&
        physical_put(b, v->storage) && physical_put(b, v->state_root) &&
        physical_put(b, v->tensor_id) && physical_put(b, v->qtype) && physical_put(b, (unsigned int)v->parameter);
    for (i = 0u; ok && i < v->type.rank; ++i)
        ok = physical_put(b, v->type.shape[i].symbol) && physical_put(b, v->type.shape[i].extent);
    return ok;
}

static int physical_step_write(yvex_core_bytes *b, const yvex_program_physical_step *s)
{
    unsigned int i;
    int ok = physical_text(b, s->implementation) && physical_put(b, s->effects) &&
        physical_put(b, s->operand_count) && physical_put(b, s->result_count) && physical_put(b, s->attribute_count);
    for (i = 0u; ok && i < s->operand_count; ++i) ok = physical_put(b, s->operands[i]);
    for (i = 0u; ok && i < s->result_count; ++i) ok = physical_put(b, s->results[i]);
    for (i = 0u; ok && i < s->attribute_count; ++i) {
        const yvex_ir_attribute *a = &s->attributes[i];
        uint64_t bits = a->value.integer;
        if (a->kind == YVEX_IR_ATTR_F64) memcpy(&bits, &a->value.real, sizeof(bits));
        else if (a->kind != YVEX_IR_ATTR_U64 && a->kind != YVEX_IR_ATTR_BOOL) return 0;
        ok = physical_text(b, a->name) && physical_put(b, a->kind) && physical_put(b, bits);
    }
    return ok;
}

static int physical_records(const yvex_program_physical *p, yvex_core_bytes *b)
{
    const yvex_program_physical_summary *s = &p->summary;
    size_t i;
    int ok = physical_text(b, "yvex.program.physical.v1") && physical_text(b, s->entry) &&
        physical_text(b, s->semantic_identity) && physical_text(b, s->execution_identity) &&
        physical_text(b, s->parameter_identity) && physical_put(b, s->minimum_rows) &&
        physical_put(b, s->maximum_rows) && physical_put(b, s->row_multiple) &&
        physical_put(b, s->input_count) && physical_put(b, s->value_count) && physical_put(b, s->step_count) &&
        physical_put(b, s->result_count) && physical_put(b, s->storage_count);
    for (i = 0u; ok && i < s->value_count; ++i) ok = physical_value_write(b, &p->values[i]);
    for (i = 0u; ok && i < s->step_count; ++i) ok = physical_step_write(b, &p->steps[i]);
    for (i = 0u; ok && i < s->result_count; ++i) ok = physical_put(b, p->results[i]);
    return ok;
}

static int physical_digest(const yvex_core_bytes *b, char out[YVEX_SHA256_HEX_BYTES])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update(&hash, b->data, b->count) || !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, out);
    return 1;
}

int yvex_program_physical_parameter_view(const yvex_program_physical_value *v,
    unsigned int rank, const unsigned long long *dims, unsigned int qtype,
    unsigned long long encoded_bytes, unsigned long long *rows, unsigned long long *width, yvex_error *err)
{
    const yvex_gguf_qtype_geometry *g = yvex_gguf_qtype_geometry_find(qtype);
    unsigned long long source_count = 1u, count = 1u, bytes;
    unsigned int source_axis = 0u, program_axis = 0u;
    if (rows) *rows = 0u;
    if (width) *width = 0u;
    if (!v || !v->parameter || !rows || !width || !dims || !rank || rank > YVEX_TENSOR_MAX_DIMS ||
        !v->type.rank || v->type.rank > YVEX_IR_RANK_CAP || v->type.kind != YVEX_IR_TENSOR || qtype != v->qtype ||
        !g || !g->block_size || !g->bytes_per_block) goto incompatible;
    for (unsigned int i = 0u; i < rank; ++i)
        if (!dims[i] || !yvex_core_u64_mul(source_count, dims[i], &source_count)) goto incompatible;
    for (unsigned int i = 0u; i < v->type.rank; ++i)
        if (v->type.shape[i].symbol != YVEX_IR_NONE || !v->type.shape[i].extent ||
            !yvex_core_u64_mul(count, v->type.shape[i].extent, &count)) goto incompatible;
    if (source_count != count) goto incompatible;
    while (source_axis < rank || program_axis < v->type.rank) {
        if (source_axis < rank && dims[source_axis] == 1u) { source_axis++; continue; }
        if (program_axis < v->type.rank && v->type.shape[program_axis].extent == 1u) { program_axis++; continue; }
        if (source_axis == rank || program_axis == v->type.rank ||
            dims[source_axis++] != v->type.shape[program_axis++].extent) goto incompatible;
    }
    unsigned long long last = v->type.shape[v->type.rank - 1u].extent;
    if (dims[rank - 1u] % g->block_size || last % g->block_size ||
        (g->block_size != 1u && dims[rank - 1u] != last) ||
        !yvex_core_u64_mul(count / g->block_size, g->bytes_per_block, &bytes) || bytes != encoded_bytes)
        goto incompatible;
    *rows = count / last;
    *width = last;
    return YVEX_OK;
incompatible:
    return physical_refuse(err, YVEX_ERR_FORMAT,
        "package parameter is not an exact singleton-axis view of the program");
}

int yvex_program_physical_source_parameter_view(const yvex_program_physical_value *v,
    unsigned int rank, const unsigned long long *dims, unsigned int qtype,
    unsigned long long encoded_bytes, unsigned long long *rows, unsigned long long *width, yvex_error *err)
{
    if (!v || v->type.rank <= YVEX_TENSOR_MAX_DIMS)
        return yvex_program_physical_parameter_view(v, rank, dims, qtype, encoded_bytes, rows, width, err);
    if (rows) *rows = 0u;
    if (width) *width = 0u;
    const yvex_gguf_qtype_geometry *g = yvex_gguf_qtype_geometry_find(qtype);
    if (!rows || !width || !dims || rank != YVEX_TENSOR_MAX_DIMS || v->type.rank > YVEX_IR_RANK_CAP ||
        !g || g->block_size != 1u)
        return physical_refuse(err, YVEX_ERR_FORMAT, "source-order tail view requires an exact scalar package");
    yvex_program_physical_value folded = *v;
    unsigned long long tail = 1u;
    for (unsigned int i = YVEX_TENSOR_MAX_DIMS - 1u; i < v->type.rank; ++i) {
        if (v->type.shape[i].symbol != YVEX_IR_NONE || !v->type.shape[i].extent ||
            !yvex_core_u64_mul(tail, v->type.shape[i].extent, &tail))
            return physical_refuse(err, YVEX_ERR_FORMAT, "source-order tail view has invalid logical geometry");
    }
    folded.type.rank = YVEX_TENSOR_MAX_DIMS;
    folded.type.shape[YVEX_TENSOR_MAX_DIMS - 1u].extent = tail;
    unsigned long long folded_rows, folded_width;
    int rc = yvex_program_physical_parameter_view(&folded, rank, dims, qtype, encoded_bytes,
        &folded_rows, &folded_width, err);
    if (rc == YVEX_OK) {
        *width = v->type.shape[v->type.rank - 1u].extent;
        *rows = encoded_bytes / g->bytes_per_block / *width;
    }
    return rc;
}

int yvex_program_physical_parameters_validate(const yvex_program_physical *p,
    const yvex_physical_execution_ir *parameters, yvex_error *err)
{
    const yvex_physical_execution_summary *summary = yvex_physical_execution_ir_summary(parameters);
    size_t i;
    if (!p || !summary || strcmp(p->summary.parameter_identity, summary->identity))
        return physical_refuse(err, YVEX_ERR_FORMAT, "program requires its authenticated physical parameters");
    for (i = 0u; i < p->summary.value_count; ++i) {
        const yvex_program_physical_value *v = &p->values[i];
        const yvex_physical_execution_decision *found = NULL;
        const yvex_gguf_qtype_geometry *geometry = yvex_gguf_qtype_geometry_find(v->qtype);
        unsigned long long j, elements = 1u, bytes;
        unsigned int axis;
        size_t matches = 0u;
        if (!v->parameter) continue;
        for (j = 0u; j < summary->decision_count; ++j) {
            const yvex_physical_execution_decision *d = yvex_physical_execution_ir_decision_at(parameters, j);
            if (d->terminal_tensor_id == v->tensor_id) { found = d; matches++; }
        }
        for (axis = 0u; axis < v->type.rank; ++axis)
            if (!yvex_core_u64_mul(elements, v->type.shape[axis].extent, &elements))
                return physical_refuse(err, YVEX_ERR_BOUNDS, "program parameter geometry overflows");
        if (!v->type.rank || matches != 1u || !found || found->canonical_qtype != v->qtype ||
            found->canonical_row_width != v->type.shape[v->type.rank - 1u].extent ||
            !found->canonical_row_width || found->canonical_row_count != elements / found->canonical_row_width ||
            !geometry || !geometry->block_size || !geometry->bytes_per_block ||
            found->canonical_row_width % geometry->block_size ||
            !yvex_core_u64_mul(elements / geometry->block_size, geometry->bytes_per_block, &bytes) ||
            found->encoded_bytes != bytes)
            return physical_refuse(err, YVEX_ERR_FORMAT,
                "program parameter shape/storage differs from physical binding");
    }
    return YVEX_OK;
}

static int physical_seal(yvex_program_physical *p, yvex_error *err)
{
    yvex_core_bytes b = {.maximum = PHYSICAL_BYTES};
    int ok = physical_records(p, &b) && physical_digest(&b, p->summary.identity);
    free(b.data);
    return ok ? YVEX_OK : physical_refuse(err, YVEX_ERR_NOMEM, "physical program identity encoding failed");
}

int yvex_program_physical_encode(const yvex_program_physical *p, yvex_core_bytes *out, yvex_error *err)
{
    yvex_core_bytes b = {.maximum = PHYSICAL_BYTES};
    char identity[YVEX_SHA256_HEX_BYTES];
    int ok = p && out && physical_records(p, &b) && physical_digest(&b, identity) &&
        !strcmp(identity, p->summary.identity) && physical_text(&b, identity) &&
        yvex_core_bytes_append(out, b.data, b.count);
    free(b.data);
    return ok ? YVEX_OK : physical_refuse(err, YVEX_ERR_FORMAT, "physical program encoding/identity is invalid");
}

typedef struct { const unsigned char *data; size_t count, offset; } physical_cursor;

static int physical_get(physical_cursor *c, unsigned long long *value)
{
    unsigned int i;
    if (c->offset > c->count || c->count - c->offset < 8u) return 0;
    *value = 0ull;
    for (i = 0u; i < 8u; ++i) *value |= (unsigned long long)c->data[c->offset++] << (8u * i);
    return 1;
}

static int physical_get_id(physical_cursor *c, unsigned int *out)
{
    unsigned long long value;
    if (!physical_get(c, &value) || value > UINT32_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}

static int physical_get_text(physical_cursor *c, char *out, size_t capacity)
{
    unsigned long long n;
    if (!physical_get(c, &n) || n >= capacity || n > c->count - c->offset ||
        memchr(c->data + c->offset, 0, (size_t)n)) return 0;
    memcpy(out, c->data + c->offset, (size_t)n);
    out[n] = '\0';
    c->offset += (size_t)n;
    return 1;
}

static int physical_value_read(physical_cursor *c, yvex_program_physical_value *v)
{
    unsigned int kind, scalar, parameter, axis;
    if (!physical_get_id(c, &kind) || !physical_get_id(c, &scalar) || !physical_get_id(c, &v->type.rank) ||
        v->type.rank > YVEX_IR_RANK_CAP || !physical_get_text(c, v->type.domain, sizeof(v->type.domain)) ||
        !physical_get_id(c, &v->definition) || !physical_get_id(c, &v->last_use) ||
        !physical_get_id(c, &v->storage) || !physical_get_id(c, &v->state_root) ||
        !physical_get(c, &v->tensor_id) || !physical_get_id(c, &v->qtype) ||
        !physical_get_id(c, &parameter) || parameter > 1u) return 0;
    v->type.kind = (yvex_ir_type_kind)kind;
    v->type.scalar = (yvex_ir_scalar)scalar;
    v->parameter = (int)parameter;
    for (axis = 0u; axis < v->type.rank; ++axis) {
        unsigned long long extent;
        if (!physical_get_id(c, &v->type.shape[axis].symbol) || !physical_get(c, &extent)) return 0;
        v->type.shape[axis].extent = extent;
    }
    return 1;
}

static int physical_step_read(physical_cursor *c, yvex_program_physical_step *s)
{
    char name[YVEX_IR_NAME_CAP];
    const physical_rule *rule;
    unsigned int i;
    if (!physical_get_text(c, name, sizeof(name)) || !(rule = physical_rule_find(name, 1)) ||
        !physical_get_id(c, &s->effects) || !physical_get_id(c, &s->operand_count) ||
        s->operand_count > YVEX_PROGRAM_OPERAND_CAP || !physical_get_id(c, &s->result_count) ||
        s->result_count > YVEX_PROGRAM_RESULT_CAP || !physical_get_id(c, &s->attribute_count) ||
        s->attribute_count > YVEX_PROGRAM_ATTRIBUTE_CAP) return 0;
    s->implementation = rule->implementation;
    for (i = 0u; i < s->operand_count; ++i) if (!physical_get_id(c, &s->operands[i])) return 0;
    for (i = 0u; i < s->result_count; ++i) if (!physical_get_id(c, &s->results[i])) return 0;
    for (i = 0u; i < s->attribute_count; ++i) {
        yvex_ir_attribute *a = &s->attributes[i];
        unsigned int kind;
        unsigned long long bits;
        if (!physical_get_text(c, a->name, sizeof(a->name)) || !physical_get_id(c, &kind) ||
            !physical_get(c, &bits)) return 0;
        a->kind = (yvex_ir_attribute_kind)kind;
        if (a->kind == YVEX_IR_ATTR_F64) memcpy(&a->value.real, &bits, sizeof(bits));
        else if (a->kind == YVEX_IR_ATTR_U64 || a->kind == YVEX_IR_ATTR_BOOL) a->value.integer = bits;
        else return 0;
    }
    return 1;
}

static int physical_summary_read(physical_cursor *c, yvex_program_physical_summary *s)
{
    unsigned long long populations[5];
    char domain[64];
    size_t i;
    if (!physical_get_text(c, domain, sizeof(domain)) || strcmp(domain, "yvex.program.physical.v1") ||
        !physical_get_text(c, s->entry, sizeof(s->entry)) ||
        !physical_get_text(c, s->semantic_identity, sizeof(s->semantic_identity)) ||
        !physical_get_text(c, s->execution_identity, sizeof(s->execution_identity)) ||
        !physical_get_text(c, s->parameter_identity, sizeof(s->parameter_identity)) ||
        !physical_get(c, &s->minimum_rows) || !physical_get(c, &s->maximum_rows) ||
        !physical_get(c, &s->row_multiple)) return 0;
    for (i = 0u; i < 5u; ++i)
        if (!physical_get(c, &populations[i]) || populations[i] > PHYSICAL_LIMIT) return 0;
    s->input_count = (size_t)populations[0];
    s->value_count = (size_t)populations[1];
    s->step_count = (size_t)populations[2];
    s->result_count = (size_t)populations[3];
    s->storage_count = (size_t)populations[4];
    /* Bound allocation amplification even before semantic verification. */
    return s->value_count <= c->count / 88u && s->step_count <= c->count / 40u;
}

int yvex_program_physical_decode(yvex_program_physical **out, const unsigned char *data,
                                  size_t count, yvex_error *err)
{
    physical_cursor c = {data, count, 0u};
    yvex_program_physical_summary s = {0};
    yvex_program_physical *p = NULL;
    char identity[YVEX_SHA256_HEX_BYTES];
    size_t i;
    int rc;
    if (out) *out = NULL;
    if (!out || !data || !count || count > PHYSICAL_BYTES || !physical_summary_read(&c, &s))
        return physical_refuse(err, YVEX_ERR_FORMAT, "physical program header is malformed");
    rc = physical_allocate(&p, &s, err);
    for (i = 0u; rc == YVEX_OK && i < s.value_count; ++i)
        if (!physical_value_read(&c, &p->values[i])) rc = YVEX_ERR_FORMAT;
    for (i = 0u; rc == YVEX_OK && i < s.step_count; ++i)
        if (!physical_step_read(&c, &p->steps[i])) rc = YVEX_ERR_FORMAT;
    for (i = 0u; rc == YVEX_OK && i < s.result_count; ++i)
        if (!physical_get_id(&c, &p->results[i])) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK && (!physical_get_text(&c, identity, sizeof(identity)) || c.offset != c.count))
        rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = physical_verify(p, err);
    if (rc == YVEX_OK) rc = physical_seal(p, err);
    if (rc == YVEX_OK && strcmp(identity, p->summary.identity)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) *out = p;
    else {
        yvex_program_physical_close(&p);
        if (!yvex_error_is_set(err)) physical_refuse(err, (yvex_status)rc, "physical program records are malformed");
    }
    return rc;
}
