/* Typed stateful operation semantics, independent of layer plans and physical storage.
 * BF16 values denote rounded logical publications, not F32 device buffer layouts.
 * Recurrence accumulates in F32; attention uses the admitted BF16/F32/RNE contract. */
#include "src/ir/private.h"

#include <math.h>
#include <stdint.h>
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

static int sequence_ssd_verify(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *projection = sequence_operand(m, op, 0u);
    const yvex_ir_attribute *heads = yvex_ir_attribute_get(m, id, "heads");
    const yvex_ir_attribute *head = yvex_ir_attribute_get(m, id, "head_dimension");
    const yvex_ir_attribute *state = yvex_ir_attribute_get(m, id, "state_dimension");
    const yvex_ir_attribute *groups = yvex_ir_attribute_get(m, id, "groups");
    const yvex_ir_attribute *kernel = yvex_ir_attribute_get(m, id, "convolution_kernel");
    const yvex_ir_attribute *normalization_groups = yvex_ir_attribute_get(m, id, "normalization_groups");
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    const yvex_ir_attribute *minimum = yvex_ir_attribute_get(m, id, "time_step_minimum");
    const yvex_ir_attribute *maximum = yvex_ir_attribute_get(m, id, "time_step_maximum");
    const yvex_ir_attribute *unbounded = yvex_ir_attribute_get(m, id, "time_step_unbounded");
    const yvex_ir_attribute *norm_before_gate = yvex_ir_attribute_get(m, id, "norm_before_gate");
    unsigned long long width, grouped, twice, convolution_width, projection_width;
    unsigned long long convolution_values, recurrent_values, state_values, bytes;
    uint64_t convolution_weight[3], convolution_state[2], recurrent[3], vector[1];
    const yvex_ir_type *output;
    if (!projection || projection->kind != YVEX_IR_TENSOR || projection->scalar != YVEX_IR_F32 ||
        projection->rank != 2u || !heads || !head || !state || !groups || !kernel ||
        !normalization_groups || !epsilon || !minimum || !maximum || !unbounded || !norm_before_gate)
        goto invalid;
    if (!heads->value.integer || !head->value.integer || !state->value.integer || !groups->value.integer ||
        heads->value.integer % groups->value.integer || !kernel->value.integer ||
        !normalization_groups->value.integer || heads->value.integer % normalization_groups->value.integer ||
        !isfinite((float)epsilon->value.real) || (float)epsilon->value.real <= 0.0f ||
        !isfinite((float)minimum->value.real) || minimum->value.real < 0.0 ||
        !isfinite((float)maximum->value.real) ||
        (unbounded->value.integer ? maximum->value.real != 0.0 : maximum->value.real < minimum->value.real) ||
        !yvex_core_u64_mul(heads->value.integer, head->value.integer, &width) ||
        !yvex_core_u64_mul(groups->value.integer, state->value.integer, &grouped) ||
        !yvex_core_u64_mul(grouped, 2u, &twice) ||
        !yvex_core_u64_add(width, twice, &convolution_width) ||
        !yvex_core_u64_add(width, convolution_width, &projection_width) ||
        !yvex_core_u64_add(projection_width, heads->value.integer, &projection_width) ||
        !yvex_core_u64_mul(convolution_width, kernel->value.integer, &convolution_values) ||
        !yvex_core_u64_mul(width, state->value.integer, &recurrent_values) ||
        !yvex_core_u64_add(convolution_values, recurrent_values, &state_values) ||
        !yvex_core_u64_mul(state_values, sizeof(float), &bytes) || bytes > SIZE_MAX)
        goto invalid;
    if (projection->shape[1].symbol != YVEX_IR_NONE ||
        projection->shape[1].extent != projection_width) goto invalid;
    convolution_weight[0] = convolution_width;
    convolution_weight[1] = 1u;
    convolution_weight[2] = kernel->value.integer;
    if (!sequence_static(sequence_operand(m, op, 1u), YVEX_IR_TENSOR, YVEX_IR_F32,
                         convolution_weight, 3u, NULL)) goto invalid;
    vector[0] = convolution_width;
    if (!sequence_static(sequence_operand(m, op, 2u), YVEX_IR_TENSOR, YVEX_IR_F32,
                         vector, 1u, NULL)) goto invalid;
    vector[0] = heads->value.integer;
    for (uint32_t index = 3u; index < 6u; ++index)
        if (!sequence_static(sequence_operand(m, op, index), YVEX_IR_TENSOR, YVEX_IR_F32,
                             vector, 1u, NULL)) goto invalid;
    vector[0] = width;
    if (!sequence_static(sequence_operand(m, op, 6u), YVEX_IR_TENSOR, YVEX_IR_F32,
                         vector, 1u, NULL)) goto invalid;
    convolution_state[0] = convolution_width;
    convolution_state[1] = kernel->value.integer;
    recurrent[0] = heads->value.integer;
    recurrent[1] = head->value.integer;
    recurrent[2] = state->value.integer;
    if (!sequence_static(sequence_operand(m, op, 7u), YVEX_IR_STATE, YVEX_IR_F32,
                         convolution_state, 2u, "convolution.causal") ||
        !sequence_static(sequence_operand(m, op, 8u), YVEX_IR_STATE, YVEX_IR_F32,
                         recurrent, 3u, "ssm.selective")) goto invalid;
    output = &m->types[m->values[op->results[0]].type];
    if (output->kind != YVEX_IR_TENSOR || output->scalar != YVEX_IR_F32 || output->rank != 2u ||
        !yvex_ir_extent_equal(output->shape[0], projection->shape[0]) ||
        output->shape[1].symbol != YVEX_IR_NONE || output->shape[1].extent != width ||
        !sequence_state_result(m, op, 7u, 1u) || !sequence_state_result(m, op, 8u, 2u)) goto invalid;
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "selective SSD operands, state versions or source-derived geometry disagree");
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
    static const yvex_ir_attribute_rule ssd[] = {
        {"heads", YVEX_IR_ATTR_U64, 1}, {"head_dimension", YVEX_IR_ATTR_U64, 1},
        {"state_dimension", YVEX_IR_ATTR_U64, 1}, {"groups", YVEX_IR_ATTR_U64, 1},
        {"convolution_kernel", YVEX_IR_ATTR_U64, 1}, {"normalization_groups", YVEX_IR_ATTR_U64, 1},
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"time_step_minimum", YVEX_IR_ATTR_F64, 1},
        {"time_step_maximum", YVEX_IR_ATTR_F64, 1}, {"time_step_unbounded", YVEX_IR_ATTR_BOOL, 1},
        {"norm_before_gate", YVEX_IR_ATTR_BOOL, 1}};
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
        {"sequence.selective_ssd", 1u, 9u, 9u, 3u, 3u, 0u,
         YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         ssd, sizeof(ssd) / sizeof(ssd[0]), 0, sequence_ssd_verify},
        {"sequence.gated_delta", 1u, 10u, 10u, 3u, 3u, 0u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         delta, sizeof(delta) / sizeof(delta[0]), 0, sequence_delta_verify},
        {"attention.gated_causal", 1u, 7u, 7u, 2u, 2u, 0u, YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE,
         attention, sizeof(attention) / sizeof(attention[0]), 0, sequence_attention_verify}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
