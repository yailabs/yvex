/*
 * ir_validate.c - canonical DAG validation and sealing.
 *
 * Owner:
 *   src/model/compilation
 *
 * Owns:
 *   insertion-order-independent canonicalization, producer/consumer checks,
 *   operation shape/dtype validation, iterative cycle detection, topological
 *   depth, exact source-use accounting, immutable indexes, and publication.
 *
 * Does not own:
 *   family tensor discovery, canonical identity encoding, source IO, GGUF
 *   projection, numerical execution, quantization, or rendering.
 *
 * Invariants:
 *   validation is iterative and bounded; each non-source value has one
 *   producer; every required source use and terminal is exact before publish.
 *
 * Boundary:
 *   graph admission proves plan consistency, not transformation execution.
 */
#include "private.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned long long old_index;
    unsigned long long key;
    unsigned int class_id;
    const char *name;
} transform_order;

typedef struct {
    unsigned long long value;
    unsigned long long old_index;
} transform_u64_order;

static int transform_order_compare(const void *left_ptr, const void *right_ptr)
{
    const transform_order *left = (const transform_order *)left_ptr;
    const transform_order *right = (const transform_order *)right_ptr;

    if (left->class_id < right->class_id) return -1;
    if (left->class_id > right->class_id) return 1;
    if (left->key < right->key) return -1;
    if (left->key > right->key) return 1;
    if (left->name || right->name) {
        int compared = strcmp(left->name ? left->name : "",
                              right->name ? right->name : "");
        if (compared) return compared;
    }
    return left->old_index < right->old_index
        ? -1 : (left->old_index > right->old_index ? 1 : 0);
}

static int transform_u64_order_compare(const void *left_ptr,
                                       const void *right_ptr)
{
    const transform_u64_order *left =
        (const transform_u64_order *)left_ptr;
    const transform_u64_order *right =
        (const transform_u64_order *)right_ptr;

    if (left->value < right->value) return -1;
    if (left->value > right->value) return 1;
    return left->old_index < right->old_index
        ? -1 : (left->old_index > right->old_index ? 1 : 0);
}

static int transform_shape_equal(const yvex_transform_shape *left,
                                 const yvex_transform_shape *right)
{
    unsigned int index;

    if (!left || !right || left->rank != right->rank) return 0;
    for (index = 0u; index < left->rank; ++index)
        if (left->dims[index] != right->dims[index]) return 0;
    return 1;
}

/* Inserts one terminal key while resolving hash collisions by typed equality. */
static int transform_terminal_index_insert(yvex_transform_ir *ir,
                                           unsigned long long value_id)
{
    const yvex_transform_value *candidate = &ir->values[value_id];
    unsigned long long hash =
        yvex_transform_hash_logical_key(&candidate->logical_key);
    unsigned long long slot = hash & (ir->terminal_index_capacity - 1u);
    unsigned long long probe;

    for (probe = 0u; probe < ir->terminal_index_capacity; ++probe) {
        yvex_transform_index_slot *entry = &ir->terminal_index[slot];
        if (entry->value_plus_one == 0u) {
            entry->hash = hash;
            entry->value_plus_one = value_id + 1u;
            return 1;
        }
        if (entry->hash == hash &&
            yvex_transform_logical_key_equal(
                &ir->values[entry->value_plus_one - 1u].logical_key,
                &candidate->logical_key)) return 0;
        slot = (slot + 1u) & (ir->terminal_index_capacity - 1u);
    }
    return 0;
}

static const yvex_transform_source_value *transform_value_source(
    const yvex_transform_ir *ir, const yvex_transform_value *value)
{
    if (!ir || !value || value->kind != YVEX_TRANSFORM_VALUE_SOURCE ||
        value->source_index >= ir->summary.source_value_count) return NULL;
    return &ir->sources[value->source_index];
}

static int transform_operation_fail(yvex_transform_failure *failure,
                                    yvex_transform_failure_code code,
                                    const yvex_transform_node *node,
                                    unsigned long long input,
                                    unsigned long long expected,
                                    unsigned long long actual,
                                    yvex_error *err)
{
    return yvex_transform_fail(
        failure, code, node ? node->output_value_id : YVEX_TRANSFORM_IR_NO_ID,
        node ? node->id : YVEX_TRANSFORM_IR_NO_ID,
        YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID, input,
        expected, actual, node ? node->axis : 0u, err,
        "transform_operation_validate");
}

static int transform_validate_precision(const yvex_transform_value *output,
                                        const yvex_transform_node *node,
                                        yvex_transform_failure *failure,
                                        yvex_error *err)
{
    const unsigned int known_flags =
        YVEX_TRANSFORM_PRECISION_EXACT |
        YVEX_TRANSFORM_PRECISION_LOSSLESS |
        YVEX_TRANSFORM_PRECISION_RANGE_PROOF |
        YVEX_TRANSFORM_PRECISION_SCALE_PAIRED |
        YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT |
        YVEX_TRANSFORM_PRECISION_INTEGER_ONLY |
        YVEX_TRANSFORM_PRECISION_REFERENCE_COMPUTE;
    const unsigned int known_physical =
        YVEX_TRANSFORM_PHYSICAL_F32 |
        YVEX_TRANSFORM_PHYSICAL_F16 |
        YVEX_TRANSFORM_PHYSICAL_BF16 |
        YVEX_TRANSFORM_PHYSICAL_I32 |
        YVEX_TRANSFORM_PHYSICAL_QUANTIZED;
    unsigned int required = 0u;

    if (!output->precision.flags ||
        (output->precision.flags & ~known_flags) != 0u ||
        !output->precision.allowed_physical_classes ||
        (output->precision.allowed_physical_classes & ~known_physical) != 0u) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node,
            YVEX_TRANSFORM_IR_NO_ID, known_physical,
            output->precision.allowed_physical_classes, err);
    }
    switch (node->kind) {
    case YVEX_TRANSFORM_OP_IDENTITY:
        required = YVEX_TRANSFORM_PRECISION_EXACT;
        break;
    case YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR:
    case YVEX_TRANSFORM_OP_EXPERT_AGGREGATE:
        required = YVEX_TRANSFORM_PRECISION_SCALE_PAIRED |
                   YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT |
                   YVEX_TRANSFORM_PRECISION_REFERENCE_COMPUTE;
        break;
    case YVEX_TRANSFORM_OP_CHECKED_CAST:
        required = YVEX_TRANSFORM_PRECISION_LOSSLESS |
                   YVEX_TRANSFORM_PRECISION_RANGE_PROOF |
                   YVEX_TRANSFORM_PRECISION_INTEGER_ONLY;
        break;
    default:
        required = YVEX_TRANSFORM_PRECISION_EXACT;
        break;
    }
    if ((output->precision.flags & required) != required ||
        (required & YVEX_TRANSFORM_PRECISION_RANGE_PROOF &&
         !output->precision.range_proof_required) ||
        (required & YVEX_TRANSFORM_PRECISION_REFERENCE_COMPUTE &&
         !output->precision.reference_compute_required)) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node,
            YVEX_TRANSFORM_IR_NO_ID, required, output->precision.flags, err);
    }
    if (node->numeric == YVEX_TRANSFORM_NUMERIC_APPROXIMATION_ALLOWED &&
        !output->precision.approximation_allowed) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node,
            YVEX_TRANSFORM_IR_NO_ID, 1u, 0u, err);
    }
    return YVEX_OK;
}

/* Validates one closed operation kind against already-canonical value facts. */
static int transform_validate_operation(const yvex_transform_ir *ir,
                                        const yvex_transform_node *node,
                                        yvex_transform_failure *failure,
                                        yvex_error *err)
{
    const yvex_transform_value *output;
    const yvex_transform_value *first;
    unsigned long long first_count;
    unsigned long long output_count;
    unsigned long long input;
    unsigned int dimension;
    yvex_transform_numeric_semantics required_numeric;
    yvex_transform_ordering_semantics required_ordering;
    int rc;

    if (!ir || !node || node->kind >= YVEX_TRANSFORM_OP_COUNT ||
        node->output_value_id >= ir->summary.value_count ||
        node->input_offset > ir->summary.edge_count ||
        node->input_count > ir->summary.edge_count - node->input_offset) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_UNSUPPORTED_OPERATION, node,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_OP_COUNT,
            node ? node->kind : YVEX_TRANSFORM_OP_COUNT, err);
    }
    output = &ir->values[node->output_value_id];
    first = yvex_transform_ir_node_input_at(ir, node, 0u);
    if (!first) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_UNRESOLVED_EDGE, node, 0u,
            1u, 0u, err);
    }
    required_numeric = YVEX_TRANSFORM_NUMERIC_EXACT;
    required_ordering = YVEX_TRANSFORM_ORDER_INPUT;
    if (node->kind == YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR ||
        node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE)
        required_numeric = YVEX_TRANSFORM_NUMERIC_LOSSLESS;
    else if (node->kind == YVEX_TRANSFORM_OP_CHECKED_CAST)
        required_numeric = YVEX_TRANSFORM_NUMERIC_RANGE_PROOF;
    if (node->kind == YVEX_TRANSFORM_OP_TRANSPOSE ||
        node->kind == YVEX_TRANSFORM_OP_CONCATENATE ||
        node->kind == YVEX_TRANSFORM_OP_STACK)
        required_ordering = YVEX_TRANSFORM_ORDER_AXIS;
    else if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE)
        required_ordering = YVEX_TRANSFORM_ORDER_EXPERT_INDEX;
    if (node->numeric != required_numeric ||
        node->ordering != required_ordering ||
        !node->payload_execution_required) {
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_UNSUPPORTED_OPERATION, node,
            YVEX_TRANSFORM_IR_NO_ID,
            ((unsigned long long)required_numeric << 32u) |
                (unsigned long long)required_ordering,
            ((unsigned long long)node->numeric << 32u) |
                (unsigned long long)node->ordering,
            err);
    }
    rc = transform_validate_precision(output, node, failure, err);
    if (rc != YVEX_OK) return rc;
    switch (node->kind) {
    case YVEX_TRANSFORM_OP_IDENTITY:
        if (node->input_count != 1u)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_ARITY, node,
                YVEX_TRANSFORM_IR_NO_ID, 1u, node->input_count, err);
        if (first->dtype != output->dtype ||
            !transform_shape_equal(&first->shape, &output->shape))
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node, 0u,
                first->dtype, output->dtype, err);
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR: {
        const yvex_transform_value *scale;
        if (node->input_count != 2u)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_ARITY, node,
                YVEX_TRANSFORM_IR_NO_ID, 2u, node->input_count, err);
        scale = yvex_transform_ir_node_input_at(ir, node, 1u);
        if (!scale || first->dtype != YVEX_TRANSFORM_DTYPE_FP8_E4M3 ||
            scale->dtype != YVEX_TRANSFORM_DTYPE_E8M0_SCALE ||
            output->dtype != YVEX_TRANSFORM_DTYPE_REAL ||
            !transform_shape_equal(&first->shape, &output->shape) ||
            first->shape.rank != scale->shape.rank ||
            node->scale_block_rows == 0u ||
            node->scale_block_columns == 0u)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node, 1u,
                YVEX_TRANSFORM_DTYPE_E8M0_SCALE,
                scale ? scale->dtype : YVEX_TRANSFORM_DTYPE_UNKNOWN, err);
        for (dimension = 0u; dimension < first->shape.rank; ++dimension) {
            unsigned long long block = dimension == 0u
                ? node->scale_block_rows : node->scale_block_columns;
            if (first->shape.dims[dimension] % block != 0u ||
                scale->shape.dims[dimension] !=
                    first->shape.dims[dimension] / block) {
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node, 1u,
                    first->shape.dims[dimension] / block,
                    scale->shape.dims[dimension], err);
            }
        }
        return YVEX_OK;
    }

    case YVEX_TRANSFORM_OP_CHECKED_CAST:
        if (node->input_count != 1u)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_ARITY, node,
                YVEX_TRANSFORM_IR_NO_ID, 1u, node->input_count, err);
        if (first->dtype != YVEX_TRANSFORM_DTYPE_I64 ||
            output->dtype != YVEX_TRANSFORM_DTYPE_I32 ||
            !transform_shape_equal(&first->shape, &output->shape) ||
            node->numeric != YVEX_TRANSFORM_NUMERIC_RANGE_PROOF)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node, 0u,
                YVEX_TRANSFORM_DTYPE_I64, first->dtype, err);
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_RESHAPE:
        if (node->input_count != 1u || first->dtype != output->dtype)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_ARITY, node,
                YVEX_TRANSFORM_IR_NO_ID, 1u, node->input_count, err);
        rc = yvex_transform_shape_element_count(&first->shape, &first_count,
                                                failure, err);
        if (rc != YVEX_OK) return rc;
        rc = yvex_transform_shape_element_count(&output->shape, &output_count,
                                                failure, err);
        if (rc != YVEX_OK) return rc;
        if (first_count != output_count)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_ELEMENT_COUNT_MISMATCH, node,
                0u, first_count, output_count, err);
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_TRANSPOSE: {
        unsigned char seen[YVEX_TRANSFORM_IR_MAX_RANK] = {0};
        if (node->input_count != 1u || first->dtype != output->dtype ||
            first->shape.rank != output->shape.rank ||
            node->permutation_rank != first->shape.rank)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_RANK, node, 0u,
                first->shape.rank, node->permutation_rank, err);
        for (dimension = 0u; dimension < node->permutation_rank; ++dimension) {
            unsigned int axis = node->permutation[dimension];
            if (axis >= first->shape.rank || seen[axis])
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_PERMUTATION, node,
                    0u, first->shape.rank, axis, err);
            seen[axis] = 1u;
            if (output->shape.dims[dimension] != first->shape.dims[axis])
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node, 0u,
                    first->shape.dims[axis], output->shape.dims[dimension],
                    err);
        }
        return YVEX_OK;
    }

    case YVEX_TRANSFORM_OP_CONCATENATE:
        if (node->input_count < 2u || node->axis >= first->shape.rank ||
            output->shape.rank != first->shape.rank ||
            output->dtype != first->dtype)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_AXIS, node,
                YVEX_TRANSFORM_IR_NO_ID, first->shape.rank, node->axis, err);
        output_count = 0u;
        for (input = 0u; input < node->input_count; ++input) {
            const yvex_transform_value *value =
                yvex_transform_ir_node_input_at(ir, node, input);
            if (!value || value->dtype != first->dtype ||
                value->shape.rank != first->shape.rank)
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node,
                    input, first->dtype,
                    value ? value->dtype : YVEX_TRANSFORM_DTYPE_UNKNOWN, err);
            for (dimension = 0u; dimension < first->shape.rank; ++dimension) {
                if (dimension != node->axis &&
                    value->shape.dims[dimension] != first->shape.dims[dimension])
                    return transform_operation_fail(
                        failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                        input, first->shape.dims[dimension],
                        value->shape.dims[dimension], err);
            }
            if (value->shape.dims[node->axis] > ULLONG_MAX - output_count)
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_DIMENSION_OVERFLOW, node,
                    input, ULLONG_MAX, value->shape.dims[node->axis], err);
            output_count += value->shape.dims[node->axis];
        }
        if (output->shape.dims[node->axis] != output_count)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                YVEX_TRANSFORM_IR_NO_ID, output_count,
                output->shape.dims[node->axis], err);
        for (dimension = 0u; dimension < output->shape.rank; ++dimension)
            if (dimension != node->axis &&
                output->shape.dims[dimension] != first->shape.dims[dimension])
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                    YVEX_TRANSFORM_IR_NO_ID, first->shape.dims[dimension],
                    output->shape.dims[dimension], err);
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_STACK:
        if (node->axis > first->shape.rank ||
            output->shape.rank != first->shape.rank + 1u ||
            output->dtype != first->dtype)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_AXIS, node,
                YVEX_TRANSFORM_IR_NO_ID, first->shape.rank, node->axis, err);
        for (input = 0u; input < node->input_count; ++input) {
            const yvex_transform_value *value =
                yvex_transform_ir_node_input_at(ir, node, input);
            if (!value || value->dtype != first->dtype ||
                !transform_shape_equal(&value->shape, &first->shape))
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                    input, first->shape.rank, value ? value->shape.rank : 0u,
                    err);
        }
        if (output->shape.dims[node->axis] != node->input_count)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                YVEX_TRANSFORM_IR_NO_ID, node->input_count,
                output->shape.dims[node->axis], err);
        for (dimension = 0u; dimension < output->shape.rank; ++dimension) {
            unsigned int source_dimension;
            if (dimension == node->axis) continue;
            source_dimension = dimension < node->axis ? dimension
                                                       : dimension - 1u;
            if (output->shape.dims[dimension] !=
                first->shape.dims[source_dimension])
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                    YVEX_TRANSFORM_IR_NO_ID,
                    first->shape.dims[source_dimension],
                    output->shape.dims[dimension], err);
        }
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_AGGREGATE:
        if (node->input_count < 2u || output->dtype != first->dtype ||
            !transform_shape_equal(&output->shape, &first->shape))
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_AGGREGATION, node,
                YVEX_TRANSFORM_IR_NO_ID, 2u, node->input_count, err);
        for (input = 1u; input < node->input_count; ++input) {
            const yvex_transform_value *value =
                yvex_transform_ir_node_input_at(ir, node, input);
            if (!value || value->dtype != first->dtype ||
                !transform_shape_equal(&value->shape, &first->shape))
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                    input, first->shape.rank, value ? value->shape.rank : 0u,
                    err);
        }
        return YVEX_OK;

    case YVEX_TRANSFORM_OP_EXPERT_AGGREGATE: {
        unsigned long long required_inputs;
        unsigned long long logical_width;
        if (node->expert_count == 0u || node->expert_count > ULLONG_MAX / 2u)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_AGGREGATION, node,
                YVEX_TRANSFORM_IR_NO_ID, 1u, node->expert_count, err);
        required_inputs = node->expert_count * 2u;
        if (node->input_count != required_inputs || node->axis != 0u ||
            node->packing_factor == 0u || node->scale_group_width == 0u ||
            first->shape.rank != 2u || output->shape.rank != 3u ||
            output->dtype != YVEX_TRANSFORM_DTYPE_REAL ||
            first->shape.dims[1] > ULLONG_MAX / node->packing_factor)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_AGGREGATION, node,
                YVEX_TRANSFORM_IR_NO_ID, required_inputs, node->input_count,
                err);
        logical_width = first->shape.dims[1] * node->packing_factor;
        if (logical_width % node->scale_group_width != 0u ||
            output->shape.dims[0] != node->expert_count ||
            output->shape.dims[1] != first->shape.dims[0] ||
            output->shape.dims[2] != logical_width)
            return transform_operation_fail(
                failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                YVEX_TRANSFORM_IR_NO_ID, logical_width,
                output->shape.dims[2], err);
        for (input = 0u; input < node->expert_count; ++input) {
            const yvex_transform_value *weight =
                yvex_transform_ir_node_input_at(ir, node, input * 2u);
            const yvex_transform_value *scale =
                yvex_transform_ir_node_input_at(ir, node, input * 2u + 1u);
            const yvex_transform_source_value *weight_source =
                transform_value_source(ir, weight);
            const yvex_transform_source_value *scale_source =
                transform_value_source(ir, scale);
            if (!weight || !scale || !weight_source || !scale_source ||
                weight->dtype != YVEX_TRANSFORM_DTYPE_PACKED_FP4 ||
                scale->dtype != YVEX_TRANSFORM_DTYPE_E8M0_SCALE ||
                weight_source->source_dtype != YVEX_NATIVE_DTYPE_I8 ||
                scale_source->source_dtype != YVEX_NATIVE_DTYPE_F8_E8M0)
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_DTYPE, node,
                    input, YVEX_TRANSFORM_DTYPE_PACKED_FP4,
                    weight ? weight->dtype : YVEX_TRANSFORM_DTYPE_UNKNOWN, err);
            if (weight_source->expert_index != input ||
                scale_source->expert_index != input)
                return transform_operation_fail(
                    failure,
                    weight_source->expert_index < input ||
                            scale_source->expert_index < input
                        ? YVEX_TRANSFORM_FAILURE_DUPLICATE_EXPERT
                        : YVEX_TRANSFORM_FAILURE_MISSING_EXPERT,
                    node, input, input, weight_source->expert_index, err);
            if (!transform_shape_equal(&weight->shape, &first->shape) ||
                scale->shape.rank != 2u ||
                scale->shape.dims[0] != first->shape.dims[0] ||
                scale->shape.dims[1] !=
                    logical_width / node->scale_group_width)
                return transform_operation_fail(
                    failure, YVEX_TRANSFORM_FAILURE_INVALID_SHAPE, node,
                    input, logical_width / node->scale_group_width,
                    scale->shape.dims[1], err);
        }
        return YVEX_OK;
    }

    default:
        return transform_operation_fail(
            failure, YVEX_TRANSFORM_FAILURE_UNSUPPORTED_OPERATION, node,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_OP_COUNT, node->kind, err);
    }
}

static void transform_heap_push(unsigned long long *heap,
                                unsigned long long *count,
                                unsigned long long value)
{
    unsigned long long cursor = (*count)++;

    while (cursor) {
        unsigned long long parent = (cursor - 1u) / 2u;
        if (heap[parent] <= value) break;
        heap[cursor] = heap[parent];
        cursor = parent;
    }
    heap[cursor] = value;
}

static unsigned long long transform_heap_pop(unsigned long long *heap,
                                             unsigned long long *count)
{
    unsigned long long result = heap[0];
    unsigned long long value = heap[--(*count)];
    unsigned long long cursor = 0u;

    while (cursor * 2u + 1u < *count) {
        unsigned long long child = cursor * 2u + 1u;
        if (child + 1u < *count && heap[child + 1u] < heap[child]) child++;
        if (heap[child] >= value) break;
        heap[cursor] = heap[child];
        cursor = child;
    }
    if (*count) heap[cursor] = value;
    return result;
}

static void transform_release_temporary(yvex_transform_allocator *allocator,
                                        void **allocation)
{
    if (allocation && *allocation) {
        allocator->release(*allocation, allocator->context);
        *allocation = NULL;
    }
}

/* Adds one projected allocation to a checked size budget without allocating. */
static int transform_project_bytes(size_t *total,
                                   unsigned long long count,
                                   size_t element_size)
{
    size_t bytes;

    if (!total || element_size == 0u ||
        count > (unsigned long long)(SIZE_MAX / element_size)) return 0;
    bytes = (size_t)count * element_size;
    if (bytes > SIZE_MAX - *total) return 0;
    *total += bytes;
    return 1;
}

/* Canonicalizes, validates, indexes, identities, and publishes one complete IR. */
int yvex_transform_ir_validate_and_seal(
    yvex_transform_builder *builder,
    yvex_transform_ir **out,
    yvex_transform_failure *failure,
    yvex_error *err)
{
    yvex_transform_allocator *allocator = &builder->allocator;
    yvex_transform_ir *ir = NULL;
    transform_order *source_order = NULL;
    transform_order *value_order = NULL;
    transform_order *node_order = NULL;
    transform_u64_order *tensor_order = NULL;
    unsigned long long *old_source_to_new = NULL;
    unsigned long long *old_value_to_new = NULL;
    unsigned long long *producer = NULL;
    unsigned long long *indegree = NULL;
    unsigned long long *adjacency_counts = NULL;
    unsigned long long *adjacency_offsets = NULL;
    unsigned long long *adjacency_cursor = NULL;
    unsigned long long *adjacency = NULL;
    unsigned long long *heap = NULL;
    unsigned long long source;
    unsigned long long value;
    unsigned long long node;
    unsigned long long edge;
    unsigned long long topological_count = 0u;
    unsigned long long heap_count = 0u;
    unsigned long long source_index_capacity;
    unsigned long long terminal_index_capacity;
    size_t projected_bytes;
    size_t projected_sealed_bytes;
    size_t projected_temporary_bytes;
    size_t temporary_bytes = 0u;
    size_t sealed_bytes = sizeof(*ir);
    int rc = YVEX_OK;

#define TRANSFORM_TEMP_ALLOC(target, count, type)                              \
    do {                                                                       \
        size_t transform_temp_size;                                            \
        if ((count) > (unsigned long long)(SIZE_MAX / sizeof(type))) {         \
            rc = yvex_transform_fail(                                          \
                failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,               \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, SIZE_MAX, (count), 0u, err,           \
                "transform_ir_seal");                                        \
            goto cleanup;                                                      \
        }                                                                      \
        transform_temp_size = (size_t)(count) * sizeof(type);                  \
        (target) = (type *)yvex_transform_allocate_zero(                       \
            allocator, transform_temp_size ? transform_temp_size : 1u);        \
        if (!(target)) {                                                       \
            rc = yvex_transform_fail(                                          \
                failure, YVEX_TRANSFORM_FAILURE_ALLOCATION,                    \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, transform_temp_size, 0u, 0u, err,     \
                "transform_ir_seal");                                        \
            goto cleanup;                                                      \
        }                                                                      \
        temporary_bytes += transform_temp_size;                                \
    } while (0)
#define TRANSFORM_IR_ALLOC(target, count, type)                                \
    do {                                                                       \
        size_t transform_ir_size;                                              \
        if ((count) > (unsigned long long)(SIZE_MAX / sizeof(type))) {         \
            rc = yvex_transform_fail(                                          \
                failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,               \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, SIZE_MAX, (count), 0u, err,           \
                "transform_ir_seal");                                        \
            goto cleanup;                                                      \
        }                                                                      \
        transform_ir_size = (size_t)(count) * sizeof(type);                    \
        (target) = (type *)yvex_transform_allocate_zero(                       \
            allocator, transform_ir_size ? transform_ir_size : 1u);            \
        if (!(target)) {                                                       \
            rc = yvex_transform_fail(                                          \
                failure, YVEX_TRANSFORM_FAILURE_ALLOCATION,                    \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,              \
                YVEX_TRANSFORM_IR_NO_ID, transform_ir_size, 0u, 0u, err,       \
                "transform_ir_seal");                                        \
            goto cleanup;                                                      \
        }                                                                      \
        sealed_bytes += transform_ir_size;                                     \
    } while (0)

    if (builder->source_count != builder->header.expected_source_count ||
        builder->terminal_count != builder->header.expected_terminal_count ||
        builder->value_count < builder->source_count + builder->terminal_count ||
        builder->node_count == 0u || builder->edge_count == 0u) {
        rc = yvex_transform_fail(
            failure,
            builder->source_count != builder->header.expected_source_count
                ? YVEX_TRANSFORM_FAILURE_UNEXPECTED_SOURCE
                : YVEX_TRANSFORM_FAILURE_MISSING_TERMINAL,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, builder->header.expected_source_count,
            builder->source_count, 0u, err, "transform_ir_seal");
        goto cleanup;
    }
    source_index_capacity = yvex_transform_index_capacity(
        builder->source_count);
    terminal_index_capacity = yvex_transform_index_capacity(
        builder->terminal_count);
    projected_sealed_bytes = sizeof(*ir);
    projected_temporary_bytes = 0u;
    if (!source_index_capacity || !terminal_index_capacity ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->source_count,
                                 sizeof(yvex_transform_source_value)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->value_count,
                                 sizeof(yvex_transform_value)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->node_count,
                                 sizeof(yvex_transform_node)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->edge_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->node_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 builder->terminal_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 source_index_capacity,
                                 sizeof(yvex_transform_index_slot)) ||
        !transform_project_bytes(&projected_sealed_bytes,
                                 terminal_index_capacity,
                                 sizeof(yvex_transform_index_slot)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->source_count,
                                 sizeof(transform_order)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->source_count,
                                 sizeof(transform_u64_order)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->value_count,
                                 sizeof(transform_order)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count,
                                 sizeof(transform_order)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->source_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->value_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->value_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count,
                                 sizeof(unsigned long long)) ||
        builder->node_count == ULLONG_MAX ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count + 1u,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->edge_count,
                                 sizeof(unsigned long long)) ||
        !transform_project_bytes(&projected_temporary_bytes,
                                 builder->node_count,
                                 sizeof(unsigned long long))) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, SIZE_MAX, builder->value_count,
            0u, err, "transform_ir_seal");
        goto cleanup;
    }
    projected_bytes = builder->owned_bytes;
    if (projected_sealed_bytes > SIZE_MAX - projected_bytes) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, SIZE_MAX, projected_sealed_bytes,
            0u, err, "transform_ir_seal");
        goto cleanup;
    }
    projected_bytes += projected_sealed_bytes;
    if (projected_temporary_bytes > SIZE_MAX - projected_bytes ||
        projected_bytes + projected_temporary_bytes >
            builder->budget.maximum_owned_bytes) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, builder->budget.maximum_owned_bytes,
            projected_temporary_bytes > SIZE_MAX - projected_bytes
                ? SIZE_MAX : projected_bytes + projected_temporary_bytes,
            0u, err, "transform_ir_seal");
        goto cleanup;
    }
    ir = (yvex_transform_ir *)yvex_transform_allocate_zero(allocator,
                                                            sizeof(*ir));
    if (!ir) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_ALLOCATION,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, sizeof(*ir), 0u, 0u, err,
            "transform_ir_seal");
        goto cleanup;
    }
    ir->allocator = *allocator;
    TRANSFORM_IR_ALLOC(ir->sources, builder->source_count,
                       yvex_transform_source_value);
    TRANSFORM_IR_ALLOC(ir->values, builder->value_count, yvex_transform_value);
    TRANSFORM_IR_ALLOC(ir->nodes, builder->node_count, yvex_transform_node);
    TRANSFORM_IR_ALLOC(ir->edges, builder->edge_count, unsigned long long);
    TRANSFORM_IR_ALLOC(ir->topological_order, builder->node_count,
                       unsigned long long);
    TRANSFORM_IR_ALLOC(ir->terminal_values, builder->terminal_count,
                       unsigned long long);

    TRANSFORM_TEMP_ALLOC(source_order, builder->source_count, transform_order);
    TRANSFORM_TEMP_ALLOC(tensor_order, builder->source_count,
                         transform_u64_order);
    TRANSFORM_TEMP_ALLOC(value_order, builder->value_count, transform_order);
    TRANSFORM_TEMP_ALLOC(node_order, builder->node_count, transform_order);
    TRANSFORM_TEMP_ALLOC(old_source_to_new, builder->source_count,
                         unsigned long long);
    TRANSFORM_TEMP_ALLOC(old_value_to_new, builder->value_count,
                         unsigned long long);
    TRANSFORM_TEMP_ALLOC(producer, builder->value_count, unsigned long long);
    TRANSFORM_TEMP_ALLOC(indegree, builder->node_count, unsigned long long);
    TRANSFORM_TEMP_ALLOC(adjacency_counts, builder->node_count,
                         unsigned long long);
    TRANSFORM_TEMP_ALLOC(adjacency_offsets, builder->node_count + 1u,
                         unsigned long long);
    TRANSFORM_TEMP_ALLOC(adjacency_cursor, builder->node_count,
                         unsigned long long);
    TRANSFORM_TEMP_ALLOC(adjacency, builder->edge_count, unsigned long long);
    TRANSFORM_TEMP_ALLOC(heap, builder->node_count, unsigned long long);
    for (value = 0u; value < builder->value_count; ++value)
        producer[value] = YVEX_TRANSFORM_IR_NO_ID;

    for (source = 0u; source < builder->source_count; ++source) {
        source_order[source].old_index = source;
        source_order[source].key = 0u;
        source_order[source].class_id = 0u;
        source_order[source].name = builder->sources[source].source_name;
        tensor_order[source].value = builder->sources[source].source_tensor_index;
        tensor_order[source].old_index = source;
    }
    qsort(source_order, (size_t)builder->source_count,
          sizeof(source_order[0]), transform_order_compare);
    qsort(tensor_order, (size_t)builder->source_count,
          sizeof(tensor_order[0]), transform_u64_order_compare);
    for (source = 0u; source < builder->source_count; ++source) {
        unsigned long long old = source_order[source].old_index;
        if (source &&
            strcmp(source_order[source - 1u].name,
                   source_order[source].name) == 0) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_SOURCE,
                builder->sources[old].value_id, YVEX_TRANSFORM_IR_NO_ID,
                builder->sources[old].source_tensor_index,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                1u, 2u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        if (source && tensor_order[source - 1u].value ==
                          tensor_order[source].value) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_SOURCE,
                builder->sources[tensor_order[source].old_index].value_id,
                YVEX_TRANSFORM_IR_NO_ID, tensor_order[source].value,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                1u, 2u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        ir->sources[source] = builder->sources[old];
        old_source_to_new[old] = source;
    }

    for (value = 0u; value < builder->value_count; ++value) {
        const yvex_transform_value *item = &builder->values[value];
        value_order[value].old_index = value;
        value_order[value].class_id = (unsigned int)item->kind;
        value_order[value].name = NULL;
        if (item->kind == YVEX_TRANSFORM_VALUE_SOURCE) {
            value_order[value].key = old_source_to_new[item->source_index];
        } else if (item->kind == YVEX_TRANSFORM_VALUE_INTERMEDIATE) {
            value_order[value].key = item->semantic_id;
        } else {
            value_order[value].key = item->canonical_ordinal;
        }
    }
    qsort(value_order, (size_t)builder->value_count,
          sizeof(value_order[0]), transform_order_compare);
    for (value = 0u; value < builder->value_count; ++value) {
        unsigned long long old = value_order[value].old_index;
        yvex_transform_value *copy = &ir->values[value];
        *copy = builder->values[old];
        copy->id = value;
        copy->producer_node_id = YVEX_TRANSFORM_IR_NO_ID;
        copy->consumer_count = 0u;
        copy->depth = 0u;
        if (copy->kind == YVEX_TRANSFORM_VALUE_SOURCE) {
            copy->source_index = old_source_to_new[copy->source_index];
            ir->sources[copy->source_index].value_id = value;
        }
        old_value_to_new[old] = value;
    }
    for (value = 1u; value < builder->value_count; ++value) {
        const yvex_transform_value *left = &ir->values[value - 1u];
        const yvex_transform_value *right = &ir->values[value];
        if (left->kind == YVEX_TRANSFORM_VALUE_INTERMEDIATE &&
            right->kind == left->kind && left->semantic_id == right->semantic_id) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_VALUE,
                right->id, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, 1u, 2u, 0u, err,
                "transform_ir_seal");
            goto cleanup;
        }
        if (left->kind == YVEX_TRANSFORM_VALUE_TERMINAL &&
            right->kind == left->kind &&
            (left->canonical_ordinal == right->canonical_ordinal ||
             yvex_transform_logical_key_equal(&left->logical_key,
                                              &right->logical_key))) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_TERMINAL,
                right->id, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, right->canonical_ordinal,
                YVEX_TRANSFORM_IR_NO_ID, 1u, 2u, 0u, err,
                "transform_ir_seal");
            goto cleanup;
        }
    }

    for (node = 0u; node < builder->node_count; ++node) {
        const yvex_transform_node *item = &builder->nodes[node].node;
        if (item->output_value_id >= builder->value_count) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_UNRESOLVED_EDGE,
                item->output_value_id, node, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                builder->value_count, item->output_value_id, 0u, err,
                "transform_ir_seal");
            goto cleanup;
        }
        node_order[node].old_index = node;
        node_order[node].key = old_value_to_new[item->output_value_id];
        node_order[node].class_id = 0u;
        node_order[node].name = NULL;
    }
    qsort(node_order, (size_t)builder->node_count, sizeof(node_order[0]),
          transform_order_compare);
    edge = 0u;
    for (node = 0u; node < builder->node_count; ++node) {
        unsigned long long old = node_order[node].old_index;
        const yvex_transform_node *item = &builder->nodes[old].node;
        yvex_transform_node *copy = &ir->nodes[node];
        unsigned long long local;

        *copy = *item;
        copy->id = node;
        copy->output_value_id = old_value_to_new[item->output_value_id];
        copy->input_offset = edge;
        if (producer[copy->output_value_id] != YVEX_TRANSFORM_IR_NO_ID) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_MULTIPLE_PRODUCERS,
                copy->output_value_id, node, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                1u, 2u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        if (ir->values[copy->output_value_id].kind ==
            YVEX_TRANSFORM_VALUE_SOURCE) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_MULTIPLE_PRODUCERS,
                copy->output_value_id, node,
                ir->values[copy->output_value_id].source_index,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                0u, 1u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        producer[copy->output_value_id] = node;
        ir->values[copy->output_value_id].producer_node_id = node;
        for (local = 0u; local < item->input_count; ++local) {
            unsigned long long old_input =
                builder->edges[item->input_offset + local];
            unsigned long long new_input;
            if (old_input >= builder->value_count) {
                rc = yvex_transform_fail(
                    failure, YVEX_TRANSFORM_FAILURE_UNRESOLVED_EDGE,
                    copy->output_value_id, node, YVEX_TRANSFORM_IR_NO_ID,
                    YVEX_TRANSFORM_IR_NO_ID, local,
                    builder->value_count, old_input, 0u, err,
                    "transform_ir_seal");
                goto cleanup;
            }
            new_input = old_value_to_new[old_input];
            ir->edges[edge++] = new_input;
            ir->values[new_input].consumer_count++;
        }
    }
    if (edge != builder->edge_count) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_SEAL,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, builder->edge_count, edge, 0u, err,
            "transform_ir_seal");
        goto cleanup;
    }
    for (value = 0u; value < builder->value_count; ++value) {
        yvex_transform_value *item = &ir->values[value];
        unsigned long long element_count;

        rc = yvex_transform_shape_element_count(
            &item->shape, &element_count, failure, err);
        if (rc != YVEX_OK) {
            if (failure) failure->value_id = value;
            goto cleanup;
        }
        if (item->kind != YVEX_TRANSFORM_VALUE_SOURCE &&
            item->producer_node_id == YVEX_TRANSFORM_IR_NO_ID) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_MISSING_PRODUCER,
                value, YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                item->canonical_ordinal, YVEX_TRANSFORM_IR_NO_ID,
                1u, 0u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        if (item->kind == YVEX_TRANSFORM_VALUE_SOURCE) {
            const yvex_transform_source_value *record =
                &ir->sources[item->source_index];
            if (item->consumer_count != record->required_uses) {
                rc = yvex_transform_fail(
                    failure, YVEX_TRANSFORM_FAILURE_UNCONSUMED_SOURCE,
                    value, YVEX_TRANSFORM_IR_NO_ID, item->source_index,
                    YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                    record->required_uses, item->consumer_count, 0u, err,
                    "transform_ir_seal");
                goto cleanup;
            }
        }
    }

    ir->summary.source_value_count = builder->source_count;
    ir->summary.value_count = builder->value_count;
    ir->summary.node_count = builder->node_count;
    ir->summary.edge_count = builder->edge_count;
    ir->summary.terminal_count = builder->terminal_count;
    ir->summary.intermediate_value_count =
        builder->value_count - builder->source_count - builder->terminal_count;
    for (node = 0u; node < builder->node_count; ++node) {
        rc = transform_validate_operation(ir, &ir->nodes[node], failure, err);
        if (rc != YVEX_OK) goto cleanup;
        ir->summary.operation_counts[ir->nodes[node].kind]++;
        if (ir->nodes[node].input_count > ir->summary.maximum_fan_in)
            ir->summary.maximum_fan_in = ir->nodes[node].input_count;
    }

    for (node = 0u; node < builder->node_count; ++node) {
        const yvex_transform_node *consumer = &ir->nodes[node];
        unsigned long long local;
        for (local = 0u; local < consumer->input_count; ++local) {
            unsigned long long input_value =
                ir->edges[consumer->input_offset + local];
            unsigned long long source_node = producer[input_value];
            if (source_node != YVEX_TRANSFORM_IR_NO_ID) {
                indegree[node]++;
                adjacency_counts[source_node]++;
            }
        }
    }
    for (node = 0u; node < builder->node_count; ++node) {
        if (adjacency_counts[node] > ULLONG_MAX - adjacency_offsets[node]) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DIMENSION_OVERFLOW,
                YVEX_TRANSFORM_IR_NO_ID, node, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                ULLONG_MAX, adjacency_counts[node], 0u, err,
                "transform_ir_seal");
            goto cleanup;
        }
        adjacency_offsets[node + 1u] =
            adjacency_offsets[node] + adjacency_counts[node];
        adjacency_cursor[node] = adjacency_offsets[node];
        if (indegree[node] == 0u) transform_heap_push(heap, &heap_count, node);
    }
    for (node = 0u; node < builder->node_count; ++node) {
        const yvex_transform_node *consumer = &ir->nodes[node];
        unsigned long long local;
        for (local = 0u; local < consumer->input_count; ++local) {
            unsigned long long source_node =
                producer[ir->edges[consumer->input_offset + local]];
            if (source_node != YVEX_TRANSFORM_IR_NO_ID)
                adjacency[adjacency_cursor[source_node]++] = node;
        }
    }
    while (heap_count) {
        unsigned long long current = transform_heap_pop(heap, &heap_count);
        yvex_transform_node *current_node = &ir->nodes[current];
        yvex_transform_value *output =
            &ir->values[current_node->output_value_id];
        unsigned long long local;
        unsigned long long depth = 1u;

        for (local = 0u; local < current_node->input_count; ++local) {
            const yvex_transform_value *input =
                &ir->values[ir->edges[current_node->input_offset + local]];
            if (input->depth >= depth) depth = input->depth + 1u;
        }
        output->depth = depth;
        if (depth > ir->summary.maximum_depth)
            ir->summary.maximum_depth = depth;
        ir->topological_order[topological_count++] = current;
        for (local = adjacency_offsets[current];
             local < adjacency_offsets[current + 1u]; ++local) {
            unsigned long long consumer = adjacency[local];
            if (--indegree[consumer] == 0u)
                transform_heap_push(heap, &heap_count, consumer);
        }
    }
    if (topological_count != builder->node_count) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_CYCLE,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, builder->node_count,
            topological_count, 0u, err, "transform_ir_seal");
        goto cleanup;
    }

    ir->source_index_capacity = source_index_capacity;
    ir->terminal_index_capacity = terminal_index_capacity;
    if (!ir->source_index_capacity || !ir->terminal_index_capacity) {
        rc = yvex_transform_fail(
            failure, YVEX_TRANSFORM_FAILURE_RESOURCE_BUDGET,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
            YVEX_TRANSFORM_IR_NO_ID, ULLONG_MAX, builder->value_count,
            0u, err, "transform_ir_seal");
        goto cleanup;
    }
    TRANSFORM_IR_ALLOC(ir->source_index, ir->source_index_capacity,
                       yvex_transform_index_slot);
    TRANSFORM_IR_ALLOC(ir->terminal_index, ir->terminal_index_capacity,
                       yvex_transform_index_slot);
    for (source = 0u; source < builder->source_count; ++source) {
        if (!yvex_transform_index_insert(
                ir->source_index, ir->source_index_capacity,
                yvex_transform_hash_string(ir->sources[source].source_name),
                source)) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_SOURCE,
                ir->sources[source].value_id, YVEX_TRANSFORM_IR_NO_ID,
                source, YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                1u, 2u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
    }
    for (value = 0u; value < builder->value_count; ++value) {
        const yvex_transform_value *item = &ir->values[value];
        if (item->kind != YVEX_TRANSFORM_VALUE_TERMINAL) continue;
        if (item->canonical_ordinal >= builder->terminal_count ||
            ir->terminal_values[item->canonical_ordinal] != 0u) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_TERMINAL,
                value, item->producer_node_id, YVEX_TRANSFORM_IR_NO_ID,
                item->canonical_ordinal, YVEX_TRANSFORM_IR_NO_ID,
                builder->terminal_count, item->canonical_ordinal, 0u, err,
                "transform_ir_seal");
            goto cleanup;
        }
        ir->terminal_values[item->canonical_ordinal] = value + 1u;
        if (!transform_terminal_index_insert(ir, value)) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_DUPLICATE_TERMINAL,
                value, item->producer_node_id, YVEX_TRANSFORM_IR_NO_ID,
                item->canonical_ordinal, YVEX_TRANSFORM_IR_NO_ID,
                1u, 2u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
    }
    for (value = 0u; value < builder->terminal_count; ++value) {
        if (ir->terminal_values[value] == 0u) {
            rc = yvex_transform_fail(
                failure, YVEX_TRANSFORM_FAILURE_MISSING_TERMINAL,
                YVEX_TRANSFORM_IR_NO_ID, YVEX_TRANSFORM_IR_NO_ID,
                YVEX_TRANSFORM_IR_NO_ID, value, YVEX_TRANSFORM_IR_NO_ID,
                1u, 0u, 0u, err, "transform_ir_seal");
            goto cleanup;
        }
        ir->terminal_values[value]--;
    }

    ir->summary.schema_version = builder->header.schema_version;
    ir->summary.state = YVEX_TRANSFORM_IR_STATE_SEALED;
    (void)snprintf(ir->summary.logical_model_identity,
                   sizeof(ir->summary.logical_model_identity), "%s",
                   builder->logical_model_identity);
    ir->summary.source_snapshot_identity =
        builder->header.source_snapshot_identity;
    ir->summary.coverage_identity = builder->header.coverage_identity;
    (void)snprintf(ir->summary.required_payload_identity,
                   sizeof(ir->summary.required_payload_identity), "%s",
                   builder->required_payload_identity);
    (void)snprintf(ir->summary.payload_trust_class,
                   sizeof(ir->summary.payload_trust_class), "%s",
                   builder->payload_trust_class);
    ir->summary.index_capacity = ir->source_index_capacity +
                                 ir->terminal_index_capacity;
    ir->summary.validation_steps = builder->value_count + builder->node_count +
                                   builder->edge_count + topological_count;
    ir->summary.builder_peak_bytes = builder->peak_bytes;
    ir->summary.sealed_ir_bytes = sealed_bytes;
    ir->summary.temporary_validation_bytes = temporary_bytes;
    ir->summary.total_owned_bytes = sealed_bytes;
    ir->summary.header_scan_count = builder->header.header_scan_count;
    ir->summary.payload_bytes_read = 0u;
    rc = yvex_transform_ir_compute_identity(ir, failure, err);
    if (rc != YVEX_OK) goto cleanup;
    ir->summary.complete = 1;
    *out = ir;
    ir = NULL;
    yvex_error_clear(err);

cleanup:
    transform_release_temporary(allocator, (void **)&heap);
    transform_release_temporary(allocator, (void **)&adjacency);
    transform_release_temporary(allocator, (void **)&adjacency_cursor);
    transform_release_temporary(allocator, (void **)&adjacency_offsets);
    transform_release_temporary(allocator, (void **)&adjacency_counts);
    transform_release_temporary(allocator, (void **)&indegree);
    transform_release_temporary(allocator, (void **)&producer);
    transform_release_temporary(allocator, (void **)&old_value_to_new);
    transform_release_temporary(allocator, (void **)&old_source_to_new);
    transform_release_temporary(allocator, (void **)&node_order);
    transform_release_temporary(allocator, (void **)&value_order);
    transform_release_temporary(allocator, (void **)&tensor_order);
    transform_release_temporary(allocator, (void **)&source_order);
    yvex_transform_ir_release(&ir);
#undef TRANSFORM_IR_ALLOC
#undef TRANSFORM_TEMP_ALLOC
    return rc;
}
