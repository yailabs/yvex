/* Logical neural semantics. Physical tensor encoding is deliberately absent. */
#include "src/ir/private.h"

#include <string.h>

static const yvex_ir_type *neural_input(const yvex_ir_module *m,
                                       const yvex_ir_operation *op, uint32_t index)
{
    return &m->types[m->values[op->operands[index]].type];
}

static const yvex_ir_type *neural_output(const yvex_ir_module *m,
                                        const yvex_ir_operation *op, uint32_t index)
{
    return &m->types[m->values[op->results[index]].type];
}

static int neural_float_tensor(const yvex_ir_type *type)
{
    return type->kind == YVEX_IR_TENSOR && type->rank && type->scalar >= YVEX_IR_F16;
}

static int neural_unary(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *input = neural_input(m, op, 0u);
    if (!neural_float_tensor(input) || !yvex_ir_type_equal(input, neural_output(m, op, 0u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "neural unary operation preserves floating tensor type/shape");
    return YVEX_OK;
}

static int neural_binary(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_ir_type_equal(neural_input(m, op, 0u), neural_input(m, op, 1u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
                              "elementwise operands require equal logical shape; no implicit broadcast");
    return YVEX_OK;
}

/* Logical weights are [output, input]; layout/qtype belongs to physical lowering. */
static int neural_linear(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *input = neural_input(m, op, 0u), *weight = neural_input(m, op, 1u);
    const yvex_ir_type *output = neural_output(m, op, 0u);
    uint32_t index;
    if (!neural_float_tensor(input) || !neural_float_tensor(weight) ||
        !neural_float_tensor(output) || weight->rank != 2u ||
        output->rank != input->rank ||
        (output->scalar != input->scalar &&
         !(input->scalar == YVEX_IR_BF16 && output->scalar == YVEX_IR_F32)) ||
        (weight->scalar != input->scalar &&
         !(input->scalar == YVEX_IR_F32 && weight->scalar == YVEX_IR_BF16)) ||
        !yvex_ir_extent_equal(input->shape[input->rank - 1u], weight->shape[1]) ||
        !yvex_ir_extent_equal(output->shape[output->rank - 1u], weight->shape[0]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "linear contraction/result geometry is incompatible");
    for (index = 0u; index + 1u < input->rank; ++index)
        if (!yvex_ir_extent_equal(input->shape[index], output->shape[index]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "linear changes a non-contracted dimension");
    return YVEX_OK;
}

static int neural_embedding(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *tokens = neural_input(m, op, 0u), *weight = neural_input(m, op, 1u);
    const yvex_ir_type *output = neural_output(m, op, 0u);
    uint32_t index;
    if (tokens->kind != YVEX_IR_TENSOR || tokens->scalar != YVEX_IR_INDEX ||
        !neural_float_tensor(weight) || weight->rank != 2u ||
        !neural_float_tensor(output) || output->rank != tokens->rank + 1u ||
        output->scalar != weight->scalar ||
        !yvex_ir_extent_equal(output->shape[tokens->rank], weight->shape[1]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "embedding requires index tensor and [vocabulary, width] weights");
    for (index = 0u; index < tokens->rank; ++index)
        if (!yvex_ir_extent_equal(tokens->shape[index], output->shape[index]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "embedding changes input population");
    return YVEX_OK;
}

static int neural_norm(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *input = neural_input(m, op, 0u), *weight = neural_input(m, op, 1u);
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (!epsilon || epsilon->value.real <= 0.0 || !neural_float_tensor(weight) ||
        weight->rank != 1u || weight->scalar != input->scalar ||
        !yvex_ir_extent_equal(input->shape[input->rank - 1u], weight->shape[0]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
                              "normalization requires positive epsilon and exact channel weights");
    if (op->operand_count == 3u && !yvex_ir_type_equal(weight, neural_input(m, op, 2u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "normalization bias has incompatible geometry");
    return YVEX_OK;
}

/* Sigmoid hyperconnection collapse followed by RMS normalization. Both BF16
 * rounding points are semantic, and the pre-normalized value is an explicit
 * result (draft consumers must not borrow a hidden intermediate). */
static int neural_mhc_head(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *fn = neural_input(m, op, 1u);
    const yvex_ir_type *base = neural_input(m, op, 2u), *scale = neural_input(m, op, 3u);
    const yvex_ir_type *norm = neural_input(m, op, 4u), *y = neural_output(m, op, 0u);
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    const yvex_ir_attribute *mhc = yvex_ir_attribute_get(m, id, "mhc_epsilon");
    unsigned long long expanded;
    unsigned int i;
    if (!neural_float_tensor(x) || x->scalar != YVEX_IR_F32 || x->rank != 3u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[2].symbol != YVEX_IR_NONE ||
        !yvex_core_u64_mul(x->shape[1].extent, x->shape[2].extent, &expanded) ||
        !expanded || !epsilon || epsilon->value.real <= 0.0 || !mhc || mhc->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "mHC head requires explicit stream/channel geometry and epsilons");
    for (i = 1u; i < 5u; ++i)
        if (!neural_float_tensor(neural_input(m, op, i)) || neural_input(m, op, i)->scalar != YVEX_IR_F32)
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "mHC head parameters have F32 logical semantics");
    if (fn->rank != 2u || !yvex_ir_extent_equal(fn->shape[0], x->shape[1]) ||
        fn->shape[1].symbol != YVEX_IR_NONE || fn->shape[1].extent != expanded ||
        base->rank != 1u || !yvex_ir_extent_equal(base->shape[0], x->shape[1]) ||
        scale->rank != 1u || scale->shape[0].symbol != YVEX_IR_NONE || scale->shape[0].extent != 1u ||
        norm->rank != 1u || !yvex_ir_extent_equal(norm->shape[0], x->shape[2]) ||
        !neural_float_tensor(y) || y->rank != 2u || y->scalar != YVEX_IR_BF16 ||
        !yvex_ir_extent_equal(y->shape[0], x->shape[0]) || !yvex_ir_extent_equal(y->shape[1], x->shape[2]) ||
        !yvex_ir_type_equal(y, neural_output(m, op, 1u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "mHC head parameter/result shapes disagree with stream geometry");
    return YVEX_OK;
}

/* The reduction accumulates streams in order in F64, divides in F64, then
 * publishes F32. It does not introduce a BF16 rounding point. */
static int neural_stream_mean(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    if (!neural_float_tensor(x) || x->scalar != YVEX_IR_F32 || x->rank != 3u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[2].symbol != YVEX_IR_NONE ||
        !neural_float_tensor(y) || y->scalar != YVEX_IR_F32 || y->rank != 2u ||
        !yvex_ir_extent_equal(x->shape[0], y->shape[0]) ||
        !yvex_ir_extent_equal(x->shape[2], y->shape[1]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "stream mean requires exact row/stream/channel geometry");
    return YVEX_OK;
}

/* Gates and source-to-target mixing remain explicit operands. State lifetime
 * is not an effect of this pure residual computation. */
static int neural_mhc_post(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *core = neural_input(m, op, 1u);
    const yvex_ir_type *post = neural_input(m, op, 2u), *mix = neural_input(m, op, 3u);
    const yvex_ir_type *y = neural_output(m, op, 0u);
    const yvex_ir_type *inputs[] = {x, core, post, mix};
    for (size_t i = 0u; i < 4u; ++i)
        if (!neural_float_tensor(inputs[i]) || inputs[i]->scalar != YVEX_IR_F32 ||
            !yvex_ir_extent_equal(inputs[i]->shape[0], x->shape[0]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "mHC post requires F32 operands with the same row population");
    if (x->rank != 3u || core->rank != 2u || post->rank != 2u || mix->rank != 3u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[2].symbol != YVEX_IR_NONE ||
        !yvex_ir_extent_equal(core->shape[1], x->shape[2]) ||
        !yvex_ir_extent_equal(post->shape[1], x->shape[1]) ||
        !yvex_ir_extent_equal(mix->shape[1], x->shape[1]) ||
        !yvex_ir_extent_equal(mix->shape[2], x->shape[1]) ||
        y->kind != YVEX_IR_TENSOR || y->scalar != YVEX_IR_BF16 || y->rank != 3u)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "mHC post stream/channel geometry or output precision is incompatible");
    for (size_t i = 0u; i < 3u; ++i)
        if (!yvex_ir_extent_equal(x->shape[i], y->shape[i]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "mHC post preserves residual geometry");
    return YVEX_OK;
}

const yvex_ir_dialect *yvex_ir_neural_dialect(void)
{
    static const yvex_ir_attribute_rule norm[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"weight_offset", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule mhc[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"mhc_epsilon", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_operation_definition operations[] = {
        {"tensor.add", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"tensor.multiply", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"nn.linear", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_linear},
        {"nn.embedding", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_embedding},
        {"nn.rms_norm", 1u, 2u, 2u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"nn.layer_norm", 1u, 3u, 3u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"mhc.head_norm", 1u, 5u, 5u, 2u, 2u, 0u, 0u, mhc, 2u, 0, neural_mhc_head},
        {"tensor.stream_mean", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_stream_mean},
        {"mhc.residual_post", 1u, 4u, 4u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_mhc_post},
        {"nn.silu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        {"nn.gelu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        /* SiLU rounds to the operand type before multiplication; the product
         * rounds again. Fusion must preserve both semantic rounding points. */
        {"nn.silu_product", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
