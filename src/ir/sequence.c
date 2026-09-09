/* Typed stateful operation semantics, independent of layer plans and physical storage.
 * BF16 values denote rounded logical publications, not F32 device buffer layouts.
 * Recurrence accumulates in F32; attention uses the admitted BF16/F32/RNE contract. */
#include "src/ir/private.h"

#include <string.h>

static const yvex_ir_type *sequence_operand(const yvex_ir_module *m,
                                           const yvex_ir_operation *op, uint32_t index)
{
    return &m->types[m->values[op->operands[index]].type];
}

static int sequence_static(const yvex_ir_type *type, yvex_ir_type_kind kind,
                            yvex_ir_scalar scalar, const uint64_t *shape,
                            uint32_t rank, const char *domain)
{
    uint32_t index;
    if (type->kind != kind || type->scalar != scalar || type->rank != rank ||
        (domain && strcmp(type->domain, domain))) return 0;
    for (index = 0u; index < rank; ++index)
        if (type->shape[index].symbol != YVEX_IR_NONE || type->shape[index].extent != shape[index]) return 0;
    return 1;
}

static int sequence_rows(const yvex_ir_type *type, yvex_ir_extent rows, uint64_t width)
{
    return type->kind == YVEX_IR_TENSOR && type->scalar == YVEX_IR_BF16 && type->rank == 2u &&
           yvex_ir_extent_equal(type->shape[0], rows) && type->shape[1].symbol == YVEX_IR_NONE &&
           type->shape[1].extent == width;
}

static int sequence_state_result(const yvex_ir_module *m, const yvex_ir_operation *op,
                                  uint32_t input, uint32_t output)
{
    return m->values[op->operands[input]].type == m->values[op->results[output]].type;
}

static int sequence_delta_verify(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *qkv = sequence_operand(m, op, 0u);
    uint64_t kh = yvex_ir_attribute_get(m, id, "key_heads")->value.integer;
    uint64_t vh = yvex_ir_attribute_get(m, id, "value_heads")->value.integer;
    uint64_t kd = yvex_ir_attribute_get(m, id, "key_dimension")->value.integer;
    uint64_t vd = yvex_ir_attribute_get(m, id, "value_dimension")->value.integer;
    uint64_t kernel = yvex_ir_attribute_get(m, id, "convolution_kernel")->value.integer;
    unsigned long long kw, vw, combined;
    uint64_t weight[3], convolution[2], recurrent[3];
    double qk_epsilon = yvex_ir_attribute_get(m, id, "qk_epsilon")->value.real;
    double norm_epsilon = yvex_ir_attribute_get(m, id, "epsilon")->value.real;
    double scale = yvex_ir_attribute_get(m, id, "query_scale")->value.real;
    uint32_t index;
    if (!kh || !vh || !kd || !vd || kernel < 2u || vh % kh ||
        qk_epsilon <= 0.0 || norm_epsilon <= 0.0 || scale <= 0.0 ||
        !yvex_core_u64_mul(kh, kd, &kw) || !yvex_core_u64_mul(vh, vd, &vw) ||
        !yvex_core_u64_add(kw, kw, &combined) || !yvex_core_u64_add(combined, vw, &combined) ||
        !sequence_rows(qkv, qkv->shape[0], combined)) goto invalid;
    if (!sequence_rows(sequence_operand(m, op, 1u), qkv->shape[0], vw) ||
        !sequence_rows(sequence_operand(m, op, 2u), qkv->shape[0], vh) ||
        !sequence_rows(sequence_operand(m, op, 3u), qkv->shape[0], vh)) goto invalid;
    weight[0] = combined;
    weight[1] = 1u;
    weight[2] = kernel;
    if (!sequence_static(sequence_operand(m, op, 4u), YVEX_IR_TENSOR, YVEX_IR_BF16,
                          weight, 3u, NULL)) goto invalid;
    for (index = 5u; index < 8u; ++index) {
        weight[0] = index == 7u ? vd : vh;
        if (!sequence_static(sequence_operand(m, op, index), YVEX_IR_TENSOR, YVEX_IR_BF16,
                              weight, 1u, NULL)) goto invalid;
    }
    convolution[0] = combined;
    convolution[1] = kernel - 1u;
    recurrent[0] = vh;
    recurrent[1] = kd;
    recurrent[2] = vd;
    if (!sequence_static(sequence_operand(m, op, 8u), YVEX_IR_STATE, YVEX_IR_F32,
                          convolution, 2u, "convolution.causal") ||
        !sequence_static(sequence_operand(m, op, 9u), YVEX_IR_STATE, YVEX_IR_F32,
                          recurrent, 3u, "recurrent.gated_delta") ||
        !sequence_rows(&m->types[m->values[op->results[0]].type], qkv->shape[0], vw) ||
        !sequence_state_result(m, op, 8u, 1u) || !sequence_state_result(m, op, 9u, 2u)) goto invalid;
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "gated-delta operands, state versions or geometry disagree");
}

/* Projected query contains interleaved query/gate per head. Q/K undergo
 * one-plus RMSNorm, half-split partial RoPE, causal GQA, then sigmoid gating.
 * Projections and output linear remain independent program operations. */
static int sequence_attention_verify(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *query = sequence_operand(m, op, 0u);
    const yvex_ir_type *position = sequence_operand(m, op, 5u);
    const yvex_ir_type *state = sequence_operand(m, op, 6u);
    uint64_t heads = yvex_ir_attribute_get(m, id, "query_heads")->value.integer;
    uint64_t kv_heads = yvex_ir_attribute_get(m, id, "kv_heads")->value.integer;
    uint64_t head = yvex_ir_attribute_get(m, id, "head_dimension")->value.integer;
    uint64_t rotary = yvex_ir_attribute_get(m, id, "rotary_dimension")->value.integer;
    uint64_t context = yvex_ir_attribute_get(m, id, "maximum_context")->value.integer;
    uint64_t theta = yvex_ir_attribute_get(m, id, "theta")->value.integer;
    uint64_t shape[] = {2u, context, kv_heads, head};
    unsigned long long q_width, kv_width, combined;
    double epsilon = yvex_ir_attribute_get(m, id, "qk_epsilon")->value.real;
    if (!heads || !kv_heads || heads % kv_heads || !head || !context || theta <= 1u ||
        !rotary || rotary > head || rotary % 2u || epsilon <= 0.0 ||
        !yvex_core_u64_mul(heads, head, &q_width) || !yvex_core_u64_mul(kv_heads, head, &kv_width) ||
        !yvex_core_u64_mul(q_width, 2u, &combined) ||
        !sequence_rows(query, query->shape[0], combined) ||
        !sequence_rows(sequence_operand(m, op, 1u), query->shape[0], kv_width) ||
        !sequence_rows(sequence_operand(m, op, 2u), query->shape[0], kv_width) ||
        !sequence_static(sequence_operand(m, op, 3u), YVEX_IR_TENSOR, YVEX_IR_BF16, &head, 1u, NULL) ||
        !sequence_static(sequence_operand(m, op, 4u), YVEX_IR_TENSOR, YVEX_IR_BF16, &head, 1u, NULL) ||
        position->kind != YVEX_IR_SCALAR || position->scalar != YVEX_IR_INDEX ||
        !sequence_static(state, YVEX_IR_STATE, YVEX_IR_BF16, shape, 4u, "attention.causal_kv") ||
        !sequence_rows(&m->types[m->values[op->results[0]].type], query->shape[0], q_width) ||
        !sequence_state_result(m, op, 6u, 1u))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "gated causal attention operands or state geometry disagree");
    return YVEX_OK;
}

const yvex_ir_dialect *yvex_ir_sequence_dialect(void)
{
    static const yvex_ir_attribute_rule delta[] = {
        {"key_heads", YVEX_IR_ATTR_U64, 1}, {"value_heads", YVEX_IR_ATTR_U64, 1},
        {"key_dimension", YVEX_IR_ATTR_U64, 1}, {"value_dimension", YVEX_IR_ATTR_U64, 1},
        {"convolution_kernel", YVEX_IR_ATTR_U64, 1}, {"qk_epsilon", YVEX_IR_ATTR_F64, 1},
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"query_scale", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule attention[] = {
        {"query_heads", YVEX_IR_ATTR_U64, 1}, {"kv_heads", YVEX_IR_ATTR_U64, 1},
        {"head_dimension", YVEX_IR_ATTR_U64, 1}, {"rotary_dimension", YVEX_IR_ATTR_U64, 1},
        {"maximum_context", YVEX_IR_ATTR_U64, 1}, {"theta", YVEX_IR_ATTR_U64, 1},
        {"qk_epsilon", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_operation_definition operations[] = {
        {"sequence.gated_delta", 1u, 10u, 10u, 3u, 3u, 0u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         delta, sizeof(delta) / sizeof(delta[0]), 0, sequence_delta_verify},
        {"attention.gated_causal", 1u, 7u, 7u, 2u, 2u, 0u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         attention, sizeof(attention) / sizeof(attention[0]), 0, sequence_attention_verify}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
