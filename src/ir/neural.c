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

const yvex_ir_dialect *yvex_ir_neural_dialect(void)
{
    static const yvex_ir_attribute_rule norm[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"weight_offset", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_operation_definition operations[] = {
        {"tensor.add", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"tensor.multiply", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"nn.linear", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_linear},
        {"nn.embedding", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_embedding},
        {"nn.rms_norm", 1u, 2u, 2u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"nn.layer_norm", 1u, 3u, 3u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"nn.silu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        {"nn.gelu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        /* SiLU rounds to the operand type before multiplication; the product
         * rounds again. Fusion must preserve both semantic rounding points. */
        {"nn.silu_product", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
