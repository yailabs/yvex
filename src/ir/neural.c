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

/* Conversion changes logical precision, never geometry or element order. */
static int neural_cast(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    if (!neural_float_tensor(x) || !neural_float_tensor(y) || x->rank != y->rank)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "precision conversion requires floating tensor values");
    for (uint32_t axis = 0u; axis < x->rank; ++axis)
        if (!yvex_ir_extent_equal(x->shape[axis], y->shape[axis]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "precision conversion cannot change shape");
    return YVEX_OK;
}

/* Row selection and partition assembly preserve values. Partition indices
 * must form a disjoint, complete cover; that dynamic fact is checked at invocation. */
static int neural_indexed_rows(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *y = neural_output(m, op, 0u);
    int partition = !strcmp(op->definition->name, "tensor.partition_rows");
    unsigned long long total = 0u;
    if (!neural_float_tensor(y) || y->rank != 2u || op->operand_count % 2u ||
        y->shape[1].symbol != YVEX_IR_NONE || (partition && y->shape[0].symbol != YVEX_IR_NONE)) goto invalid;
    for (uint32_t i = 0u; i < op->operand_count; i += 2u) {
        const yvex_ir_type *x = neural_input(m, op, i), *indices = neural_input(m, op, i + 1u);
        if (!neural_float_tensor(x) || x->scalar != y->scalar || x->rank != 2u ||
            !yvex_ir_extent_equal(x->shape[1], y->shape[1]) || indices->kind != YVEX_IR_TENSOR ||
            indices->scalar != YVEX_IR_INDEX || indices->rank != 1u ||
            !yvex_ir_extent_equal(indices->shape[0], partition ? x->shape[0] : y->shape[0])) goto invalid;
        if (partition && (x->shape[0].symbol != YVEX_IR_NONE ||
            !yvex_core_u64_add(total, x->shape[0].extent, &total))) goto invalid;
    }
    if (partition && total != y->shape[0].extent) goto invalid;
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "indexed rows require explicit compatible populations and index streams");
}

/* A bounded Cartesian coordinate becomes an explicit computational value.
 * No floating conversion or implicit truncation is permitted. */
static int neural_index_linearize(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *major = neural_input(m, op, 0u), *minor = neural_input(m, op, 1u);
    const yvex_ir_attribute *a = yvex_ir_attribute_get(m, id, "major_extent");
    const yvex_ir_attribute *b = yvex_ir_attribute_get(m, id, "minor_extent");
    unsigned long long bound;
    if (major->kind != YVEX_IR_TENSOR || major->scalar != YVEX_IR_INDEX || major->rank != 1u ||
        !yvex_ir_type_equal(major, minor) || !yvex_ir_type_equal(major, neural_output(m, op, 0u)) ||
        !a || !b || !a->value.integer || !b->value.integer ||
        !yvex_core_u64_mul(a->value.integer, b->value.integer, &bound))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "index linearization requires equal index streams and nonempty nonoverflowing coordinate domains");
    return YVEX_OK;
}

static int neural_sinusoidal(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    const yvex_ir_attribute *period = yvex_ir_attribute_get(m, id, "maximum_period");
    if (!neural_float_tensor(x) || x->scalar != YVEX_IR_F32 || x->rank != 2u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[1].extent != 1u ||
        !neural_float_tensor(y) || y->scalar != YVEX_IR_F32 || y->rank != 2u ||
        !yvex_ir_extent_equal(x->shape[0], y->shape[0]) || y->shape[1].symbol != YVEX_IR_NONE ||
        !y->shape[1].extent || y->shape[1].extent % 2u || !period || period->value.real <= 1.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "sinusoidal embedding requires scalar rows, paired F32 channels and a positive frequency period");
    return YVEX_OK;
}

/* Axis-major frequency product; the two halves repeat the same angles.
 * Position and frequency multiplication/trigonometry are F32, publication BF16. */
static int neural_axis_rotary(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *frequency = neural_input(m, op, 1u);
    const yvex_ir_type *table = neural_output(m, op, 0u);
    unsigned long long width;
    if (!neural_float_tensor(x) || x->scalar != YVEX_IR_F32 || x->rank != 2u ||
        x->shape[1].symbol != YVEX_IR_NONE || !neural_float_tensor(frequency) ||
        frequency->scalar != YVEX_IR_F32 || frequency->rank != 1u ||
        frequency->shape[0].symbol != YVEX_IR_NONE ||
        !yvex_core_u64_mul(x->shape[1].extent, frequency->shape[0].extent, &width) ||
        !yvex_core_u64_mul(width, 2u, &width) || !neural_float_tensor(table) ||
        table->scalar != YVEX_IR_BF16 || table->rank != 2u ||
        !yvex_ir_extent_equal(x->shape[0], table->shape[0]) || table->shape[1].symbol != YVEX_IR_NONE ||
        table->shape[1].extent != width || !yvex_ir_type_equal(table, neural_output(m, op, 1u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "axis rotary tables require explicit F32 positions/frequencies and paired BF16 result geometry");
    return YVEX_OK;
}

/* Canonical product of static factors and symbolic dimensions. A dimension
 * with a single admitted value is constant; unrelated variable symbols are
 * never equated merely because their bounds happen to match. */
static int neural_volume(const yvex_ir_module *m, const yvex_ir_type *t,
    uint64_t *factor, yvex_ir_id symbols[YVEX_IR_RANK_CAP], uint32_t *count)
{
    *factor = 1u;
    *count = 0u;
    for (uint32_t axis = 0u; axis < t->rank; ++axis) {
        yvex_ir_extent e = t->shape[axis];
        if (e.symbol != YVEX_IR_NONE) {
            const yvex_ir_dimension *d = yvex_ir_dimension_at(m, e.symbol);
            if (!d) return 0;
            if (d->minimum != d->maximum) {
                uint32_t slot = (*count)++;
                while (slot && symbols[slot - 1u] > e.symbol) {
                    symbols[slot] = symbols[slot - 1u];
                    slot--;
                }
                symbols[slot] = e.symbol;
                continue;
            }
            e.extent = d->minimum;
        }
        if (!e.extent || e.extent > UINT64_MAX / *factor) return 0;
        *factor *= e.extent;
    }
    return 1;
}

/* A contiguous reshape changes geometry, not element order or precision. */
static int neural_reshape(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    yvex_ir_id xs[YVEX_IR_RANK_CAP], ys[YVEX_IR_RANK_CAP];
    uint64_t xf, yf;
    uint32_t xc, yc;
    if (!neural_float_tensor(x) || !neural_float_tensor(y) || x->scalar != y->scalar ||
        !neural_volume(m, x, &xf, xs, &xc) || !neural_volume(m, y, &yf, ys, &yc) ||
        xf != yf || xc != yc || memcmp(xs, ys, xc * sizeof(*xs)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "reshape requires equal proven element populations and unchanged precision");
    return YVEX_OK;
}

/* Logical weights are [output, input]; layout/qtype belongs to physical lowering. */
static int neural_linear(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *input = neural_input(m, op, 0u), *weight = neural_input(m, op, 1u);
    const yvex_ir_type *output = neural_output(m, op, 0u);
    uint32_t index;
    const yvex_ir_attribute *reduction = yvex_ir_attribute_get(m, id, "reduction");
    if (reduction && (reduction->value.integer != YVEX_IR_REDUCTION_ROW_DOT ||
        input->scalar != YVEX_IR_F32 || weight->scalar != YVEX_IR_F32 || output->scalar != YVEX_IR_F32))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "row-dot reduction requires F32 input, parameter and result");
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

/* The residual is added before result publication. This is not interchangeable
 * with a rounded low-precision linear result followed by tensor.add. */
static int neural_linear_residual(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    int rc = neural_linear(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_ir_type_equal(neural_input(m, op, 2u), neural_output(m, op, 0u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "linear residual requires exact result type and population");
    return YVEX_OK;
}

static int neural_linear_bias(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *bias = neural_input(m, op, 2u), *y = neural_output(m, op, 0u);
    int rc = neural_linear(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (!neural_float_tensor(bias) || bias->rank != 1u || bias->scalar != y->scalar ||
        !yvex_ir_extent_equal(bias->shape[0], y->shape[y->rank - 1u]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "linear bias requires exact output channel geometry and precision");
    return YVEX_OK;
}

static int neural_split_three(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    if (!neural_float_tensor(x) || !neural_float_tensor(y) || x->scalar != y->scalar ||
        x->rank != 2u || y->rank != 2u || !yvex_ir_extent_equal(x->shape[0], y->shape[0]) ||
        x->shape[1].symbol != YVEX_IR_NONE || y->shape[1].symbol != YVEX_IR_NONE ||
        x->shape[1].extent % 3u || x->shape[1].extent / 3u != y->shape[1].extent ||
        !yvex_ir_type_equal(y, neural_output(m, op, 1u)) ||
        !yvex_ir_type_equal(y, neural_output(m, op, 2u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "split-three requires equal contiguous channel partitions per row");
    return YVEX_OK;
}

static int neural_channel_bias(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *bias = neural_input(m, op, op->operand_count - 1u);
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (x->rank != 2u || !neural_float_tensor(bias) || bias->rank != 1u || bias->scalar != x->scalar ||
        !yvex_ir_extent_equal(x->shape[1], bias->shape[0]) ||
        (op->operand_count == 3u && !yvex_ir_type_equal(x, neural_input(m, op, 1u))))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "channel operation requires explicit equal-precision vector broadcast");
    return YVEX_OK;
}

static int neural_interleaved_three(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_attribute *head = yvex_ir_attribute_get(m, id, "head_dimension");
    int rc = neural_split_three(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (!head || !head->value.integer || neural_output(m, op, 0u)->shape[1].extent % head->value.integer)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "interleaved partition requires complete channel groups");
    return YVEX_OK;
}

static int neural_swiglu_split(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    if (!neural_float_tensor(x) || !neural_float_tensor(y) || x->scalar != y->scalar ||
        x->rank != 2u || y->rank != 2u || !yvex_ir_extent_equal(x->shape[0], y->shape[0]) ||
        x->shape[1].symbol != YVEX_IR_NONE || y->shape[1].symbol != YVEX_IR_NONE ||
        x->shape[1].extent % 2u || x->shape[1].extent / 2u != y->shape[1].extent)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "SwiGLU split requires two equal channel partitions");
    return YVEX_OK;
}

/* Weighted RMS with F64 epsilon, inverse and scaling, followed by F32 then
 * BF16 publication. Reduction order remains an admitted backend numerical
 * implementation, distinct from the ordinary F32-epsilon normalization. */
static int neural_weighted_rms(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *w = neural_input(m, op, 1u);
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    if (!neural_float_tensor(x) || x->scalar != YVEX_IR_BF16 || x->rank != 2u ||
        x->shape[1].symbol != YVEX_IR_NONE || !neural_float_tensor(w) ||
        w->scalar != YVEX_IR_F32 || w->rank != 1u ||
        !yvex_ir_extent_equal(x->shape[1], w->shape[0]) ||
        !yvex_ir_type_equal(x, neural_output(m, op, 0u)) || !epsilon || epsilon->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "weighted RMS requires BF16 rows/results, F32 channel weights and positive F64 epsilon");
    return YVEX_OK;
}

static int neural_rms_normalize(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u);
    const yvex_ir_attribute *group = yvex_ir_attribute_get(m, id, "group_width");
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (x->rank != 2u || x->shape[1].symbol != YVEX_IR_NONE || !group || !group->value.integer ||
        x->shape[1].extent % group->value.integer || !epsilon || epsilon->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "unweighted RMS requires complete groups and positive epsilon");
    return YVEX_OK;
}

static int neural_slice_rows(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *y = neural_output(m, op, 0u);
    const yvex_ir_attribute *start = yvex_ir_attribute_get(m, id, "start");
    if (!neural_float_tensor(x) || !neural_float_tensor(y) || x->scalar != y->scalar ||
        x->rank != 2u || y->rank != 2u || !yvex_ir_extent_equal(x->shape[1], y->shape[1]) ||
        x->shape[0].symbol != YVEX_IR_NONE || y->shape[0].symbol != YVEX_IR_NONE || !start ||
        start->value.integer > x->shape[0].extent || y->shape[0].extent > x->shape[0].extent - start->value.integer)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "row slice must lie within an exact admitted population");
    return YVEX_OK;
}

static int neural_rows(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *y = neural_output(m, op, 0u);
    unsigned long long rows = 0u;
    if (!neural_float_tensor(y) || y->rank != 2u || y->shape[0].symbol != YVEX_IR_NONE ||
        y->shape[1].symbol != YVEX_IR_NONE)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "row construction requires exact floating matrix geometry");
    for (size_t i = 0u; i < op->operand_count; ++i) {
        const yvex_ir_type *x = neural_input(m, op, (uint32_t)i);
        if (!neural_float_tensor(x) || x->scalar != y->scalar || x->rank != 2u ||
            !yvex_ir_extent_equal(x->shape[1], y->shape[1]) || x->shape[0].symbol != YVEX_IR_NONE ||
            !yvex_core_u64_add(rows, x->shape[0].extent, &rows))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "concatenated rows require common width and precision");
    }
    if (op->operand_count && rows != y->shape[0].extent)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "concatenation result must contain every input row exactly once");
    return YVEX_OK;
}

static int neural_grid(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *y = neural_output(m, op, 0u);
    const yvex_ir_attribute *height = yvex_ir_attribute_get(m, id, "grid_height");
    const yvex_ir_attribute *width = yvex_ir_attribute_get(m, id, "grid_width");
    const yvex_ir_attribute *block = yvex_ir_attribute_get(m, id, "block_size");
    unsigned long long rows;
    if (!height || !width || !block || !height->value.integer || !width->value.integer ||
        !block->value.integer || height->value.integer % block->value.integer ||
        width->value.integer % block->value.integer ||
        !yvex_core_u64_mul(height->value.integer, width->value.integer, &rows) ||
        !neural_float_tensor(y) || y->rank != 2u || y->shape[0].symbol != YVEX_IR_NONE ||
        y->shape[0].extent != rows || y->shape[1].symbol != YVEX_IR_NONE)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "grid operation requires exact blocked spatial geometry");
    if (!strcmp(op->definition->name, "tensor.grid_bilinear")) {
        const yvex_ir_type *table = neural_input(m, op, 0u);
        const yvex_ir_attribute *side = yvex_ir_attribute_get(m, id, "source_side");
        unsigned long long count;
        if (!side || !side->value.integer || !yvex_core_u64_mul(side->value.integer, side->value.integer, &count) ||
            !neural_float_tensor(table) || table->scalar != y->scalar || table->rank != 2u ||
            table->shape[0].symbol != YVEX_IR_NONE || table->shape[0].extent != count ||
            !yvex_ir_extent_equal(table->shape[1], y->shape[1]))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "bilinear grid source must have exact square/channel geometry");
    } else {
        const yvex_ir_attribute *theta = yvex_ir_attribute_get(m, id, "theta");
        if (y->scalar != YVEX_IR_F32 || y->shape[1].extent % 4u || !theta || theta->value.real <= 0.0 ||
            !yvex_ir_type_equal(y, neural_output(m, op, 1u)))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
                "spatial rotary requires equal F32 tables and quarter-head geometry");
    }
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
        (output->scalar != weight->scalar &&
         !(weight->scalar == YVEX_IR_BF16 && output->scalar == YVEX_IR_F32)) ||
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
        weight->rank != 1u ||
        (weight->scalar != input->scalar &&
         !(input->scalar == YVEX_IR_F32 && weight->scalar == YVEX_IR_BF16)) ||
        !yvex_ir_extent_equal(input->shape[input->rank - 1u], weight->shape[0]))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
                              "normalization requires positive epsilon and exact channel weights");
    if (op->operand_count == 3u && !yvex_ir_type_equal(weight, neural_input(m, op, 2u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "normalization bias has incompatible geometry");
    return YVEX_OK;
}

/* Packed channels form independent contiguous normalization groups. The
 * weight geometry defines a group; neither layer ordinals nor head kinds do. */
static int neural_group_norm(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *input = neural_input(m, op, 0u), *weight = neural_input(m, op, 1u);
    const yvex_ir_attribute *epsilon = yvex_ir_attribute_get(m, id, "epsilon");
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (input->rank != 2u || input->shape[1].symbol != YVEX_IR_NONE ||
        !neural_float_tensor(weight) || weight->scalar != input->scalar || weight->rank != 1u ||
        weight->shape[0].symbol != YVEX_IR_NONE || !weight->shape[0].extent ||
        input->shape[1].extent % weight->shape[0].extent || !epsilon || epsilon->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "group RMS normalization requires complete static channel groups and positive epsilon");
    return YVEX_OK;
}

/* Rotate-half publishes each product and then the sum in the operand type.
 * Tables are explicit computational inputs, not a hidden positional policy. */
static int neural_rotary_half(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *table = neural_input(m, op, 1u);
    const yvex_ir_attribute *head = yvex_ir_attribute_get(m, id, "head_dimension");
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (x->rank != 2u || x->shape[1].symbol != YVEX_IR_NONE ||
        !head || !head->value.integer || head->value.integer % 2u ||
        x->shape[1].extent % head->value.integer || !neural_float_tensor(table) ||
        table->scalar != (!strcmp(op->definition->name, "tensor.rotary_half_f32") ? YVEX_IR_F32 : x->scalar) ||
        table->rank != 2u ||
        !yvex_ir_extent_equal(table->shape[0], x->shape[0]) ||
        table->shape[1].symbol != YVEX_IR_NONE || !table->shape[1].extent ||
        table->shape[1].extent % 2u || table->shape[1].extent > head->value.integer ||
        !yvex_ir_type_equal(table, neural_input(m, op, 2u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "rotate-half requires complete heads and equal per-position cosine/sine tables");
    return YVEX_OK;
}

/* A whole admitted sequence, optionally causal. Retained-prefix attention is
 * a different stateful operation; this contract introduces no hidden cache. */
static int neural_attention(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *q = neural_input(m, op, 0u), *k = neural_input(m, op, 1u);
    const yvex_ir_attribute *head = yvex_ir_attribute_get(m, id, "head_dimension");
    const yvex_ir_type *result = neural_output(m, op, 0u);
    yvex_ir_type geometry = *result;
    geometry.scalar = q->scalar;
    if (!neural_float_tensor(q) || !yvex_ir_type_equal(q, &geometry) ||
        (result->scalar != q->scalar && !(q->scalar == YVEX_IR_BF16 && result->scalar == YVEX_IR_F32)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "attention result preserves geometry and admitted accumulation precision");
    if (q->rank != 2u || q->shape[1].symbol != YVEX_IR_NONE ||
        !neural_float_tensor(k) || k->scalar != q->scalar || k->rank != 2u ||
        k->shape[1].symbol != YVEX_IR_NONE || !k->shape[1].extent ||
        !yvex_ir_extent_equal(q->shape[0], k->shape[0]) ||
        !head || !head->value.integer || q->shape[1].extent % head->value.integer ||
        k->shape[1].extent % head->value.integer || q->shape[1].extent % k->shape[1].extent ||
        !yvex_ir_type_equal(k, neural_input(m, op, 2u)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "full attention requires equal sequence populations and integral grouped-query geometry");
    return YVEX_OK;
}

static int neural_rotary_tables(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *positions = neural_input(m, op, 0u), *table = neural_output(m, op, 0u);
    const yvex_ir_attribute *theta = yvex_ir_attribute_get(m, id, "theta");
    const yvex_ir_attribute *y = yvex_ir_attribute_get(m, id, "section_y");
    const yvex_ir_attribute *z = yvex_ir_attribute_get(m, id, "section_z");
    if (positions->kind != YVEX_IR_TENSOR || positions->scalar != YVEX_IR_INDEX || positions->rank != 1u ||
        !yvex_ir_type_equal(positions, neural_input(m, op, 1u)) ||
        !yvex_ir_type_equal(positions, neural_input(m, op, 2u)) ||
        !neural_float_tensor(table) || table->rank != 2u ||
        !yvex_ir_extent_equal(table->shape[0], positions->shape[0]) ||
        table->shape[1].symbol != YVEX_IR_NONE || !table->shape[1].extent || table->shape[1].extent % 2u ||
        !yvex_ir_type_equal(table, neural_output(m, op, 1u)) ||
        !theta || theta->value.real <= 0.0 || !y || !z ||
        y->value.integer > table->shape[1].extent / 2u ||
        z->value.integer > table->shape[1].extent / 2u - y->value.integer)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "rotary tables require three position streams, bounded interleaving sections and equal even-width results");
    return YVEX_OK;
}

static int neural_masked_rows(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *mask = neural_input(m, op, 2u);
    int rc = neural_binary(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (x->rank != 2u || !neural_float_tensor(mask) || mask->scalar != x->scalar || mask->rank != 2u ||
        !yvex_ir_extent_equal(x->shape[0], mask->shape[0]) ||
        mask->shape[1].symbol != YVEX_IR_NONE || mask->shape[1].extent != 1u)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "masked rows require one explicit Boolean-valued mask per row");
    return YVEX_OK;
}

/* Hyperconnection ingress exposes collapse, gates and the Sinkhorn matrix
 * as separate values; residual input remains immutable. */
static int neural_mhc_pre(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *mix = neural_input(m, op, 1u);
    const yvex_ir_type *scale = neural_input(m, op, 2u), *base = neural_input(m, op, 3u);
    const yvex_ir_type *y = neural_output(m, op, 0u), *post = neural_output(m, op, 1u);
    const yvex_ir_type *matrix = neural_output(m, op, 2u);
    unsigned long long mixing;
    const char *positive[] = {"epsilon", "mhc_epsilon", "post_multiplier"};
    for (size_t i = 0u; i < 3u; ++i) {
        const yvex_ir_attribute *a = yvex_ir_attribute_get(m, id, positive[i]);
        if (!a || a->value.real <= 0.0) goto invalid;
    }
    const yvex_ir_attribute *iterations = yvex_ir_attribute_get(m, id, "sinkhorn_iterations");
    if (!iterations || !iterations->value.integer || !neural_float_tensor(x) ||
        x->scalar != YVEX_IR_BF16 || x->rank != 3u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[2].symbol != YVEX_IR_NONE ||
        !yvex_core_u64_add(x->shape[1].extent, 2u, &mixing) ||
        !yvex_core_u64_mul(mixing, x->shape[1].extent, &mixing)) goto invalid;
    const yvex_ir_type *f32[] = {mix, scale, base, post, matrix};
    for (size_t i = 0u; i < 5u; ++i)
        if (!neural_float_tensor(f32[i]) || f32[i]->scalar != YVEX_IR_F32) goto invalid;
    if (mix->rank != 2u || !yvex_ir_extent_equal(mix->shape[0], x->shape[0]) ||
        mix->shape[1].symbol != YVEX_IR_NONE || mix->shape[1].extent != mixing ||
        scale->rank != 1u || scale->shape[0].symbol != YVEX_IR_NONE || scale->shape[0].extent != 3u ||
        base->rank != 1u || !yvex_ir_extent_equal(base->shape[0], mix->shape[1]) ||
        !neural_float_tensor(y) || y->scalar != YVEX_IR_BF16 || y->rank != 2u ||
        !yvex_ir_extent_equal(y->shape[0], x->shape[0]) || !yvex_ir_extent_equal(y->shape[1], x->shape[2]) ||
        post->rank != 2u || !yvex_ir_extent_equal(post->shape[0], x->shape[0]) ||
        !yvex_ir_extent_equal(post->shape[1], x->shape[1]) || matrix->rank != 3u ||
        !yvex_ir_extent_equal(matrix->shape[0], x->shape[0]) ||
        !yvex_ir_extent_equal(matrix->shape[1], x->shape[1]) ||
        !yvex_ir_extent_equal(matrix->shape[2], x->shape[1])) goto invalid;
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "mHC ingress requires typed residual/mix dependencies, three results and positive numerical attributes");
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

/* Channel-first signal programs carry their actual sample population. A
 * transposed convolution is not a reshaped dense projection: its output
 * geometry, padding and parameter contraction are semantic facts. */
static int neural_mean(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    for (uint32_t i = 1u; i < op->operand_count; ++i)
        if (!yvex_ir_type_equal(neural_input(m, op, 0u), neural_input(m, op, i)))
            return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "ordered mean requires equal branch shapes and types");
    return YVEX_OK;
}

static int neural_clamp(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    int rc = neural_unary(m, id, err);
    if (rc != YVEX_OK) return rc;
    double lower = yvex_ir_attribute_get(m, id, "lower")->value.real;
    double upper = yvex_ir_attribute_get(m, id, "upper")->value.real;
    if (lower > upper || lower < -3.4028234663852886e38 || upper > 3.4028234663852886e38)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "clamp requires ordered finite F32 bounds");
    return YVEX_OK;
}

/* A clamped gate product has one rounding boundary after the stable F64
 * sigmoid/product. Separate F32 clamp/silu/multiply operations are not an
 * equivalent decomposition at BF16 rounding ties. */
static int neural_clamped_swiglu(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *gate = neural_input(m, op, 0u), *up = neural_input(m, op, 1u);
    int rc = neural_cast(m, id, err);
    if (rc != YVEX_OK) return rc;
    if (gate->scalar != YVEX_IR_F32 || !yvex_ir_type_equal(gate, up) ||
        neural_output(m, op, 0u)->scalar != YVEX_IR_BF16 ||
        yvex_ir_attribute_get(m, id, "limit")->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "clamped SwiGLU requires equal F32 operands, positive limit and BF16 result");
    return YVEX_OK;
}

static int neural_convolution(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *w = neural_input(m, op, 1u);
    const yvex_ir_type *y = neural_output(m, op, 0u);
    uint64_t stride = yvex_ir_attribute_get(m, id, "stride")->value.integer;
    uint64_t dilation = yvex_ir_attribute_get(m, id, "dilation")->value.integer;
    uint64_t padding = yvex_ir_attribute_get(m, id, "padding")->value.integer;
    uint64_t output_padding = yvex_ir_attribute_get(m, id, "output_padding")->value.integer;
    int transposed = (int)yvex_ir_attribute_get(m, id, "transposed")->value.integer;
    int normalized = strcmp(op->definition->name, "signal.conv1d") != 0;
    int biased = strcmp(op->definition->name, "signal.normalized_conv1d_unbiased") != 0;
    if (!neural_float_tensor(x) || !neural_float_tensor(w) || !neural_float_tensor(y) ||
        x->scalar != YVEX_IR_F32 || w->scalar != x->scalar || y->scalar != x->scalar ||
        x->rank != 3u || w->rank != 3u || y->rank != 3u ||
        !yvex_ir_extent_equal(x->shape[0], y->shape[0]) || !stride || !dilation ||
        (transposed ? output_padding >= stride : output_padding != 0u)) goto invalid;
    for (size_t axis = 0u; axis < 3u; ++axis)
        if (w->shape[axis].symbol != YVEX_IR_NONE || !w->shape[axis].extent ||
            (axis && (x->shape[axis].symbol != YVEX_IR_NONE || y->shape[axis].symbol != YVEX_IR_NONE)))
            goto invalid;
    if (!yvex_ir_extent_equal(w->shape[transposed ? 0u : 1u], x->shape[1]) ||
        !yvex_ir_extent_equal(w->shape[transposed ? 1u : 0u], y->shape[1])) goto invalid;
    uint64_t kernel, doubled, length, expected;
    if (w->shape[2].extent - 1u > (UINT64_MAX - 1u) / dilation || padding > UINT64_MAX / 2u)
        goto invalid;
    kernel = (w->shape[2].extent - 1u) * dilation + 1u;
    doubled = padding * 2u;
    length = x->shape[2].extent;
    if (!length) goto invalid;
    if (transposed) {
        if (length - 1u > UINT64_MAX / stride) goto invalid;
        expected = (length - 1u) * stride;
        if (kernel > UINT64_MAX - expected) goto invalid;
        expected += kernel;
        if (output_padding > UINT64_MAX - expected) goto invalid;
        expected += output_padding;
        if (expected <= doubled) goto invalid;
        expected -= doubled;
    } else {
        if (doubled > UINT64_MAX - length || length + doubled < kernel) goto invalid;
        expected = (length + doubled - kernel) / stride + 1u;
    }
    if (y->shape[2].extent != expected) goto invalid;
    for (uint32_t i = 2u; i < op->operand_count; ++i) {
        const yvex_ir_type *v = neural_input(m, op, i);
        yvex_ir_extent channels = normalized && i == 2u ? w->shape[0] : y->shape[1];
        if (!neural_float_tensor(v) || v->scalar != x->scalar || v->rank != 1u ||
            !yvex_ir_extent_equal(v->shape[0], channels)) goto invalid;
    }
    if (op->operand_count != 2u + (unsigned int)normalized + (unsigned int)biased) goto invalid;
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "Conv1D requires explicit F32 channel/sample contraction, normalization axes and exact output geometry");
}

/* Spatial convolution consumes an explicit plane of an O/I/T/H/W parameter.
 * The plane is semantic input selection, not a runtime family convention. */
static int neural_spatial_convolution(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *w = neural_input(m, op, 1u);
    const yvex_ir_type *bias = neural_input(m, op, 2u), *y = neural_output(m, op, 0u);
    const char *strides[] = {"stride_height", "stride_width"};
    const char *before[] = {"padding_top", "padding_left"};
    const char *after[] = {"padding_bottom", "padding_right"};
    if (!neural_float_tensor(x) || x->rank != 4u || !neural_float_tensor(y) || y->rank != 4u ||
        y->scalar != x->scalar || !neural_float_tensor(w) || w->rank != 5u || w->scalar != x->scalar ||
        !neural_float_tensor(bias) || bias->rank != 1u || bias->scalar != x->scalar ||
        !yvex_ir_extent_equal(x->shape[0], y->shape[0]) ||
        !yvex_ir_extent_equal(x->shape[1], w->shape[1]) ||
        !yvex_ir_extent_equal(y->shape[1], w->shape[0]) ||
        !yvex_ir_extent_equal(bias->shape[0], w->shape[0])) goto invalid;
    for (unsigned int axis = 0u; axis < 5u; ++axis)
        if (w->shape[axis].symbol != YVEX_IR_NONE || !w->shape[axis].extent) goto invalid;
    if (yvex_ir_attribute_get(m, id, "kernel_plane")->value.integer >= w->shape[2].extent) goto invalid;
    for (unsigned int axis = 0u; axis < 2u; ++axis) {
        unsigned long long stride = yvex_ir_attribute_get(m, id, strides[axis])->value.integer;
        unsigned long long lo = yvex_ir_attribute_get(m, id, before[axis])->value.integer;
        unsigned long long hi = yvex_ir_attribute_get(m, id, after[axis])->value.integer, padded;
        if (!stride || x->shape[axis + 2u].symbol != YVEX_IR_NONE ||
            y->shape[axis + 2u].symbol != YVEX_IR_NONE ||
            !yvex_core_u64_add(x->shape[axis + 2u].extent, lo, &padded) ||
            !yvex_core_u64_add(padded, hi, &padded) || padded < w->shape[axis + 3u].extent ||
            y->shape[axis + 2u].extent != (padded - w->shape[axis + 3u].extent) / stride + 1u)
            goto invalid;
    }
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "spatial convolution requires explicit kernel plane, channel contraction and exact padded output geometry");
}

static int neural_spatial_norm(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *w = neural_input(m, op, 1u);
    unsigned long long groups = yvex_ir_attribute_get(m, id, "groups")->value.integer;
    if (neural_unary(m, id, err) != YVEX_OK || x->rank != 4u ||
        !neural_float_tensor(w) || w->rank != 1u || w->scalar != x->scalar ||
        w->shape[0].symbol != YVEX_IR_NONE || !yvex_ir_extent_equal(x->shape[1], w->shape[0]) ||
        !yvex_ir_type_equal(w, neural_input(m, op, 2u)) || !groups || w->shape[0].extent % groups ||
        x->shape[2].symbol != YVEX_IR_NONE || x->shape[3].symbol != YVEX_IR_NONE ||
        yvex_ir_attribute_get(m, id, "epsilon")->value.real <= 0.0)
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "spatial GroupNorm/SiLU requires per-sample channel groups, affine parameters and positive epsilon");
    return YVEX_OK;
}

static int neural_alias_snake(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u);
    if (neural_unary(m, id, err) != YVEX_OK || x->scalar != YVEX_IR_F32 || x->rank != 3u ||
        x->shape[1].symbol != YVEX_IR_NONE || x->shape[2].symbol != YVEX_IR_NONE)
        goto invalid;
    for (uint32_t i = 1u; i < 5u; ++i) {
        const yvex_ir_type *v = neural_input(m, op, i);
        uint64_t width = i < 3u ? x->shape[1].extent : 12u;
        if (!neural_float_tensor(v) || v->scalar != YVEX_IR_F32 || v->rank != 1u ||
            v->shape[0].symbol != YVEX_IR_NONE || v->shape[0].extent != width) goto invalid;
    }
    return YVEX_OK;
invalid:
    return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
        "alias-free SnakeBeta requires F32 channel-first signals, channel log parameters and two 12-tap filters");
}

/* Row-selected conditioning is explicit dataflow, not a hidden modality
 * lookup. Every multiply/add rounds to the logical scalar type. */
static int neural_indexed_conditioning(const yvex_ir_module *m, yvex_ir_id id, yvex_error *err)
{
    const yvex_ir_operation *op = &m->operations[id];
    const yvex_ir_type *x = neural_input(m, op, 0u), *table = neural_input(m, op, 1u);
    const yvex_ir_type *indices = neural_input(m, op, 2u);
    int gated = op->operand_count == 4u;
    if (!neural_float_tensor(x) || x->rank != 2u || x->shape[1].symbol != YVEX_IR_NONE ||
        !yvex_ir_type_equal(x, neural_output(m, op, 0u)) ||
        !neural_float_tensor(table) || table->rank != 3u || table->scalar != x->scalar ||
        table->shape[0].symbol != YVEX_IR_NONE || table->shape[1].symbol != YVEX_IR_NONE ||
        !table->shape[0].extent || !table->shape[1].extent ||
        !yvex_ir_extent_equal(x->shape[1], table->shape[2]) ||
        indices->kind != YVEX_IR_TENSOR || indices->scalar != YVEX_IR_INDEX || indices->rank != 1u ||
        !yvex_ir_extent_equal(indices->shape[0], x->shape[0]) ||
        (gated && !yvex_ir_type_equal(x, neural_input(m, op, 3u))))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT,
            "indexed conditioning requires exact table, row and value geometry");
    const yvex_ir_attribute *first = yvex_ir_attribute_get(m, id, gated ? "gate" : "shift");
    const yvex_ir_attribute *scale = gated ? NULL : yvex_ir_attribute_get(m, id, "scale");
    if (!first || first->value.integer >= table->shape[1].extent ||
        (!gated && (!scale || scale->value.integer >= table->shape[1].extent)))
        return yvex_ir_refuse(err, YVEX_ERR_FORMAT, "conditioning slot lies outside its declared table");
    return YVEX_OK;
}

const yvex_ir_dialect *yvex_ir_neural_dialect(void)
{
    static const yvex_ir_attribute_rule linear[] = {{"reduction", YVEX_IR_ATTR_U64, 0}};
    static const yvex_ir_attribute_rule norm[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"weight_offset", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule mhc[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"mhc_epsilon", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule mhc_pre[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"mhc_epsilon", YVEX_IR_ATTR_F64, 1},
        {"post_multiplier", YVEX_IR_ATTR_F64, 1}, {"sinkhorn_iterations", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule group_norm[] = {{"epsilon", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule rms_unit[] = {
        {"epsilon", YVEX_IR_ATTR_F64, 1}, {"group_width", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule gate[] = {{"gate_first", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule modulation[] = {
        {"shift", YVEX_IR_ATTR_U64, 1}, {"scale", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule indexed_gate[] = {{"gate", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule index_domain[] = {
        {"major_extent", YVEX_IR_ATTR_U64, 1}, {"minor_extent", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule slice[] = {{"start", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule rotary[] = {{"head_dimension", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule sinusoidal[] = {{"maximum_period", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule attention[] = {
        {"head_dimension", YVEX_IR_ATTR_U64, 1}, {"causal", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule tables[] = {{"theta", YVEX_IR_ATTR_F64, 1},
        {"section_y", YVEX_IR_ATTR_U64, 1}, {"section_z", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule masked[] = {{"add", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule clamp[] = {
        {"lower", YVEX_IR_ATTR_F64, 1}, {"upper", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule activation_limit[] = {{"limit", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule convolution[] = {
        {"stride", YVEX_IR_ATTR_U64, 1}, {"dilation", YVEX_IR_ATTR_U64, 1},
        {"padding", YVEX_IR_ATTR_U64, 1}, {"output_padding", YVEX_IR_ATTR_U64, 1},
        {"transposed", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule spatial[] = {
        {"stride_height", YVEX_IR_ATTR_U64, 1}, {"stride_width", YVEX_IR_ATTR_U64, 1},
        {"padding_top", YVEX_IR_ATTR_U64, 1}, {"padding_bottom", YVEX_IR_ATTR_U64, 1},
        {"padding_left", YVEX_IR_ATTR_U64, 1}, {"padding_right", YVEX_IR_ATTR_U64, 1},
        {"kernel_plane", YVEX_IR_ATTR_U64, 1}, {"reflect", YVEX_IR_ATTR_BOOL, 1}};
    static const yvex_ir_attribute_rule spatial_norm[] = {
        {"groups", YVEX_IR_ATTR_U64, 1}, {"epsilon", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_attribute_rule grid[] = {{"grid_height", YVEX_IR_ATTR_U64, 1},
        {"grid_width", YVEX_IR_ATTR_U64, 1}, {"block_size", YVEX_IR_ATTR_U64, 1},
        {"source_side", YVEX_IR_ATTR_U64, 1}};
    static const yvex_ir_attribute_rule grid_rotary[] = {{"grid_height", YVEX_IR_ATTR_U64, 1},
        {"grid_width", YVEX_IR_ATTR_U64, 1}, {"block_size", YVEX_IR_ATTR_U64, 1},
        {"theta", YVEX_IR_ATTR_F64, 1}};
    static const yvex_ir_operation_definition operations[] = {
        {"tensor.index_linearize", 1u, 2u, 2u, 1u, 1u, 0u, 0u, index_domain, 2u, 0, neural_index_linearize},
        {"tensor.cast", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_cast},
        {"nn.indexed_modulate", 1u, 3u, 3u, 1u, 1u, 0u, 0u, modulation, 2u, 0, neural_indexed_conditioning},
        {"nn.indexed_gated_residual", 1u, 4u, 4u, 1u, 1u, 0u, 0u, indexed_gate, 1u, 0, neural_indexed_conditioning},
        {"tensor.mean", 1u, 1u, 16u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_mean},
        {"tensor.clamp", 1u, 1u, 1u, 1u, 1u, 0u, 0u, clamp, 2u, 0, neural_clamp},
        {"nn.clamped_swiglu", 1u, 2u, 2u, 1u, 1u, 0u, 0u, activation_limit, 1u, 0, neural_clamped_swiglu},
        {"signal.conv1d", 1u, 3u, 3u, 1u, 1u, 0u, 0u, convolution, 5u, 0, neural_convolution},
        {"signal.conv2d_slice", 1u, 3u, 3u, 1u, 1u, 0u, 0u, spatial, 8u, 0, neural_spatial_convolution},
        {"nn.spatial_group_norm_silu", 1u, 3u, 3u, 1u, 1u, 0u, 0u, spatial_norm, 2u, 0, neural_spatial_norm},
        {"signal.normalized_conv1d", 1u, 4u, 4u, 1u, 1u, 0u, 0u, convolution, 5u, 0, neural_convolution},
        {"signal.normalized_conv1d_unbiased", 1u, 3u, 3u, 1u, 1u, 0u, 0u, convolution, 5u, 0, neural_convolution},
        {"signal.alias_snake", 1u, 5u, 5u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_alias_snake},
        {"tensor.reshape", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_reshape},
        {"tensor.slice_rows", 1u, 1u, 1u, 1u, 1u, 0u, 0u, slice, 1u, 0, neural_slice_rows},
        {"tensor.copy", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        {"tensor.zeros", 1u, 0u, 0u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_rows},
        {"tensor.concat_rows", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_rows},
        {"tensor.channel_bias", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_channel_bias},
        {"tensor.scaled_residual", 1u, 3u, 3u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_channel_bias},
        {"tensor.split_interleaved_three", 1u, 1u, 1u, 3u, 3u, 0u, 0u, rotary, 1u, 0, neural_interleaved_three},
        {"nn.swiglu_split", 1u, 1u, 1u, 1u, 1u, 0u, 0u, gate, 1u, 0, neural_swiglu_split},
        {"nn.rms_normalize", 1u, 1u, 1u, 1u, 1u, 0u, 0u, rms_unit, 2u, 0, neural_rms_normalize},
        {"tensor.add", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"tensor.multiply", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary},
        {"tensor.indexed_rows", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_indexed_rows},
        {"tensor.partition_rows", 1u, 2u, 16u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_indexed_rows},
        {"tensor.axis_rotary", 1u, 2u, 2u, 2u, 2u, 0u, 0u, NULL, 0u, 0, neural_axis_rotary},
        {"nn.sinusoidal_embedding", 1u, 1u, 1u, 1u, 1u, 0u, 0u, sinusoidal, 1u, 0, neural_sinusoidal},
        {"nn.linear", 1u, 2u, 2u, 1u, 1u, 0u, 0u, linear, 1u, 0, neural_linear},
        {"nn.linear_residual", 1u, 3u, 3u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_linear_residual},
        /* Bias is added to the accumulated projection before result rounding. */
        {"nn.linear_bias", 1u, 3u, 3u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_linear_bias},
        {"tensor.split_three", 1u, 1u, 1u, 3u, 3u, 0u, 0u, NULL, 0u, 0, neural_split_three},
        {"tensor.grid_bilinear", 1u, 1u, 1u, 1u, 1u, 0u, 0u, grid, 4u, 0, neural_grid},
        {"tensor.grid_rotary", 1u, 0u, 0u, 2u, 2u, 0u, 0u, grid_rotary, 4u, 0, neural_grid},
        {"nn.embedding", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_embedding},
        {"nn.rms_norm", 1u, 2u, 2u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"nn.weighted_rms", 1u, 2u, 2u, 1u, 1u, 0u, 0u, group_norm, 1u, 0, neural_weighted_rms},
        {"nn.group_rms_norm", 1u, 2u, 2u, 1u, 1u, 0u, 0u, group_norm, 1u, 0, neural_group_norm},
        {"tensor.rotary_half", 1u, 3u, 3u, 1u, 1u, 0u, 0u, rotary, 1u, 0, neural_rotary_half},
        /* F32 tables/products; only the final result rounds to the value type. */
        {"tensor.rotary_half_f32", 1u, 3u, 3u, 1u, 1u, 0u, 0u, rotary, 1u, 0, neural_rotary_half},
        {"attention.full", 1u, 3u, 3u, 1u, 1u, 0u, 0u, attention, 2u, 0, neural_attention},
        {"tensor.rotary_tables", 1u, 3u, 3u, 2u, 2u, 0u, 0u, tables, 3u, 0, neural_rotary_tables},
        {"tensor.masked_rows", 1u, 3u, 3u, 1u, 1u, 0u, 0u, masked, 1u, 0, neural_masked_rows},
        {"nn.layer_norm", 1u, 3u, 3u, 1u, 1u, 0u, 0u, norm, 2u, 0, neural_norm},
        {"mhc.head_norm", 1u, 5u, 5u, 2u, 2u, 0u, 0u, mhc, 2u, 0, neural_mhc_head},
        {"mhc.residual_pre", 1u, 4u, 4u, 3u, 3u, 0u, 0u, mhc_pre, 4u, 0, neural_mhc_pre},
        {"tensor.stream_mean", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_stream_mean},
        {"mhc.residual_post", 1u, 4u, 4u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_mhc_post},
        {"nn.silu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        {"nn.gelu", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        {"nn.gelu_tanh", 1u, 1u, 1u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_unary},
        /* SiLU rounds to the operand type before multiplication; the product
         * rounds again. Fusion must preserve both semantic rounding points. */
        {"nn.silu_product", 1u, 2u, 2u, 1u, 1u, 0u, 0u, NULL, 0u, 0, neural_binary}};
    static const yvex_ir_dialect dialect = {operations, sizeof(operations) / sizeof(operations[0])};
    return &dialect;
}
