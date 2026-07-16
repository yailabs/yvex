/*
 * yvex_quant_plan.c - DeepSeek physical-profile planning and sealing owner.
 *
 * Owner: TRACK.QUANT.
 * Owns: candidate accounting, fixed v0.1 selection, terminal decisions,
 *   descriptor bijection, canonical profile identity, indexes, and cleanup.
 * Does not own: IR semantics, source IO, GGUF naming/mapping identity, numeric
 *   conversion, calibration collection, writing, runtime, or rendering.
 * Invariants: 1,360 canonical terminals and descriptors biject after complete
 *   typed-field validation; construction reads zero payload bytes.
 * Boundary: this chooses physical encodings but produces no encoded payload.
 */
#include "yvex_quant_plan.h"

#include "yvex_sha256.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned long long hash;
    unsigned long long ordinal_plus_one;
} quant_plan_slot;

struct yvex_quant_plan {
    yvex_quant_plan_summary summary;
    const yvex_transform_ir *ir;
    const yvex_transform_binding *binding;
    const yvex_deepseek_gguf_map *map;
    yvex_quant_decision *decisions;
    quant_plan_slot *index;
    yvex_quant_allocate_fn allocate;
    yvex_quant_release_fn release;
    void *allocator_context;
};

static void *quant_plan_default_allocate(size_t size, void *context)
{
    (void)context;
    return calloc(1u, size);
}

static void quant_plan_default_release(void *allocation, void *context)
{
    (void)context;
    free(allocation);
}

static int quant_plan_fail(yvex_quant_failure *failure,
                           yvex_quant_failure_code code,
                           unsigned long long terminal,
                           unsigned long long source,
                           unsigned long long expected,
                           unsigned long long actual,
                           unsigned int qtype,
                           yvex_transform_operation_kind operation,
                           yvex_error *err,
                           int status,
                           const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->terminal_ordinal = terminal;
        failure->source_index = source;
        failure->row_index = ULLONG_MAX;
        failure->block_index = ULLONG_MAX;
        failure->expected = expected;
        failure->actual = actual;
        failure->qtype = qtype;
        failure->operation = operation;
    }
    yvex_error_set(err, (yvex_status)status, "quant.plan", message);
    return status;
}

static unsigned long long quant_hash_u64(unsigned long long hash,
                                         unsigned long long value)
{
    unsigned int index;
    for (index = 0u; index < 8u; ++index) {
        hash ^= (value >> (index * 8u)) & 0xffu;
        hash *= 1099511628211ull;
    }
    return hash;
}

static unsigned long long quant_key_hash(
    const yvex_transform_logical_key *key)
{
    unsigned long long hash = 1469598103934665603ull;
    hash = quant_hash_u64(hash, key->scope);
    hash = quant_hash_u64(hash, key->subsystem);
    hash = quant_hash_u64(hash, key->role);
    hash = quant_hash_u64(hash, key->layer_index);
    hash = quant_hash_u64(hash, key->auxiliary_index);
    return quant_hash_u64(hash, key->group_index);
}

static int quant_key_equal(const yvex_transform_logical_key *left,
                           const yvex_transform_logical_key *right)
{
    return left->scope == right->scope &&
           left->subsystem == right->subsystem &&
           left->role == right->role &&
           left->layer_index == right->layer_index &&
           left->auxiliary_index == right->auxiliary_index &&
           left->group_index == right->group_index;
}

static unsigned long long quant_index_capacity(unsigned long long count)
{
    unsigned long long capacity = 1u;
    if (count > ULLONG_MAX / 2u) return 0u;
    while (capacity < count * 2u) {
        if (capacity > ULLONG_MAX / 2u) return 0u;
        capacity *= 2u;
    }
    return capacity;
}

/* Adds one ownership size without allowing diagnostic arithmetic to wrap. */
static int quant_size_add(size_t left, size_t right, size_t *out)
{
    if (!out) return 0;
    if (left > SIZE_MAX - right) {
        *out = SIZE_MAX;
        return 0;
    }
    *out = left + right;
    return 1;
}

static int quant_sha_u64(yvex_sha256 *hash, unsigned long long value)
{
    unsigned char bytes[8];
    unsigned int index;
    for (index = 0u; index < 8u; ++index)
        bytes[index] = (unsigned char)((value >> (index * 8u)) & 0xffu);
    return yvex_sha256_update(hash, bytes, sizeof(bytes));
}

static int quant_sha_text(yvex_sha256 *hash, const char *text)
{
    size_t length = text ? strlen(text) : 0u;
    return quant_sha_u64(hash, length) &&
           yvex_sha256_update(hash, text, length);
}

/* Computes immutable logical geometry with overflow refusal and no mutation. */
static int quant_transform_element_count(
    const yvex_transform_shape *shape,
    unsigned long long *out)
{
    unsigned long long count = 1u;
    unsigned int dimension;

    if (out) *out = 0u;
    if (!shape || !out || !shape->rank ||
        shape->rank > YVEX_TRANSFORM_IR_MAX_RANK) return 0;
    for (dimension = 0u; dimension < shape->rank; ++dimension) {
        if (!shape->dims[dimension] ||
            count > ULLONG_MAX / shape->dims[dimension]) return 0;
        count *= shape->dims[dimension];
    }
    *out = count;
    return 1;
}

static yvex_deepseek_tensor_scope quant_map_scope(yvex_transform_scope scope)
{
    if (scope == YVEX_TRANSFORM_SCOPE_GLOBAL)
        return YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL;
    if (scope == YVEX_TRANSFORM_SCOPE_MAIN_LAYER)
        return YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER;
    return YVEX_DEEPSEEK_TENSOR_SCOPE_MTP;
}

static yvex_deepseek_gguf_transform quant_map_operation(
    yvex_transform_operation_kind operation)
{
    switch (operation) {
    case YVEX_TRANSFORM_OP_IDENTITY:
        return YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT;
    case YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR:
        return YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0;
    case YVEX_TRANSFORM_OP_CHECKED_CAST:
        return YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32;
    case YVEX_TRANSFORM_OP_EXPERT_AGGREGATE:
        return YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4;
    default:
        return (yvex_deepseek_gguf_transform)-1;
    }
}

static unsigned int quant_exact_qtype(yvex_transform_dtype dtype)
{
    switch (dtype) {
    case YVEX_TRANSFORM_DTYPE_F32: return YVEX_GGUF_QTYPE_F32;
    case YVEX_TRANSFORM_DTYPE_F16: return YVEX_GGUF_QTYPE_F16;
    case YVEX_TRANSFORM_DTYPE_BF16: return YVEX_GGUF_QTYPE_BF16;
    case YVEX_TRANSFORM_DTYPE_I32: return YVEX_GGUF_QTYPE_I32;
    case YVEX_TRANSFORM_DTYPE_REAL: return YVEX_GGUF_QTYPE_F32;
    default: return UINT_MAX;
    }
}

/* Selects one candidate encoding from typed IR precision, never lexical names. */
static unsigned int quant_candidate_qtype(
    yvex_quant_profile_kind profile,
    const yvex_transform_value *terminal,
    const yvex_transform_node *node)
{
    if (node->kind == YVEX_TRANSFORM_OP_CHECKED_CAST)
        return YVEX_GGUF_QTYPE_I32;
    if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE)
        return profile == YVEX_QUANT_PROFILE_RELEASE_Q8_Q2
            ? YVEX_GGUF_QTYPE_Q2_K : YVEX_GGUF_QTYPE_MXFP4;
    if (terminal->precision.flags &
            YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT) {
        if (profile == YVEX_QUANT_PROFILE_RELEASE_Q8_Q2)
            return YVEX_GGUF_QTYPE_Q8_0;
        if (node->kind == YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR)
            return YVEX_GGUF_QTYPE_F32;
    }
    return quant_exact_qtype(terminal->dtype);
}

static int quant_descriptor_matches(
    const yvex_transform_ir *ir,
    const yvex_transform_value *terminal,
    const yvex_transform_node *node,
    const yvex_deepseek_gguf_map *map,
    const yvex_deepseek_gguf_descriptor *descriptor,
    unsigned long long ordinal,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    unsigned int dimension;
    unsigned long long input;

    if (!descriptor || descriptor->role != terminal->logical_key.role ||
        descriptor->scope != quant_map_scope(terminal->logical_key.scope) ||
        descriptor->layer_index != terminal->logical_key.layer_index ||
        descriptor->predictor_index != terminal->logical_key.auxiliary_index ||
        descriptor->transform != quant_map_operation(node->kind) ||
        descriptor->logical_rank != terminal->shape.rank ||
        descriptor->contribution_count != node->input_count) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal,
            ULLONG_MAX, node->input_count,
            descriptor ? descriptor->contribution_count : 0u, UINT_MAX,
            node->kind, err, YVEX_ERR_FORMAT,
            "terminal and GGUF lowering descriptor do not biject");
    }
    for (dimension = 0u; dimension < terminal->shape.rank; ++dimension) {
        unsigned int terminal_axis = terminal->shape.rank - dimension - 1u;
        unsigned int source_axis = terminal_axis;

        if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE) {
            source_axis = terminal_axis == node->axis
                ? YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS
                : terminal_axis > node->axis
                    ? terminal_axis - 1u : terminal_axis;
        }
        if (descriptor->logical_dims[dimension] !=
                terminal->shape.dims[terminal_axis] ||
            descriptor->source_axis_for_logical[dimension] != source_axis) {
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal,
                ULLONG_MAX, terminal->shape.dims[terminal_axis],
                descriptor->logical_dims[dimension], UINT_MAX, node->kind,
                err, YVEX_ERR_FORMAT,
                "terminal and lowering physical axes diverge");
        }
    }
    for (input = 0u; input < node->input_count; ++input) {
        const yvex_transform_value *value =
            yvex_transform_ir_node_input_at(ir, node, input);
        const yvex_transform_source_value *source = value
            ? yvex_transform_ir_source_at(ir, value->source_index) : NULL;
        const yvex_deepseek_gguf_contribution *contribution =
            yvex_deepseek_gguf_map_contribution_at(
                map, descriptor->contribution_offset + input);
        if (!value || !source || !contribution ||
            contribution->descriptor_index != ordinal ||
            strcmp(contribution->source_name, source->source_name) != 0 ||
            contribution->source_dtype != source->source_dtype ||
            contribution->expert_index != source->expert_index) {
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal,
                value ? value->source_index : ULLONG_MAX, input,
                contribution ? contribution->descriptor_index : ULLONG_MAX,
                UINT_MAX, node->kind, err, YVEX_ERR_FORMAT,
                "lowering contribution does not match the exact IR input");
        }
    }
    return YVEX_OK;
}

static int quant_decision_geometry_dims(
    yvex_quant_decision *decision,
    const unsigned long long *dims,
    unsigned int rank,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    yvex_gguf_qtype_storage_result storage;
    yvex_gguf_qtype_storage_status status;
    yvex_quant_failure_code code;

    status = yvex_gguf_qtype_tensor_storage(
        decision->qtype, dims, rank,
        &storage);
    if (status != YVEX_GGUF_QTYPE_STORAGE_OK) {
        switch (status) {
        case YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID:
        case YVEX_GGUF_QTYPE_STORAGE_RESERVED_ID:
        case YVEX_GGUF_QTYPE_STORAGE_GEOMETRY_UNAVAILABLE:
            code = YVEX_QUANT_FAILURE_UNKNOWN_QTYPE; break;
        case YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID:
            code = YVEX_QUANT_FAILURE_REMOVED_QTYPE; break;
        case YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE:
            code = YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE; break;
        case YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK:
            code = YVEX_QUANT_FAILURE_INVALID_RANK; break;
        case YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION:
            code = YVEX_QUANT_FAILURE_INVALID_DIMENSION; break;
        case YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH:
            code = YVEX_QUANT_FAILURE_ROW_DIVISIBILITY; break;
        case YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW:
            code = YVEX_QUANT_FAILURE_ELEMENT_OVERFLOW; break;
        case YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT:
            code = YVEX_QUANT_FAILURE_INVALID_ARGUMENT; break;
        case YVEX_GGUF_QTYPE_STORAGE_ROW_BYTE_OVERFLOW:
        case YVEX_GGUF_QTYPE_STORAGE_ROW_COUNT_OVERFLOW:
        case YVEX_GGUF_QTYPE_STORAGE_TOTAL_BYTE_OVERFLOW:
        case YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH:
        default:
            code = YVEX_QUANT_FAILURE_BYTE_OVERFLOW; break;
        }
        return quant_plan_fail(
            failure, code,
            decision->terminal_ordinal, ULLONG_MAX, 0u, status,
            decision->qtype, decision->operation, err, YVEX_ERR_BOUNDS,
            "selected qtype cannot represent the lowering tensor geometry");
    }
    decision->rank = rank;
    memcpy(decision->dims, dims,
           sizeof(decision->dims));
    decision->row_axis = 0u;
    decision->row_width = storage.row_width;
    decision->row_count = storage.row_count;
    decision->element_count = storage.element_count;
    decision->encoded_bytes = storage.total_bytes;
    return YVEX_OK;
}

/* Maps canonical numeric-registry refusal to the plan's recovery class. */
static yvex_quant_failure_code quant_capability_failure(
    const yvex_quant_numeric_capability *capability)
{
    if (!capability || !capability->identity_known)
        return YVEX_QUANT_FAILURE_UNKNOWN_QTYPE;
    if (capability->refusal == YVEX_QUANT_REFUSAL_REMOVED_IDENTITY)
        return YVEX_QUANT_FAILURE_REMOVED_QTYPE;
    if (capability->refusal == YVEX_QUANT_REFUSAL_OUTSIDE_PINNED_BASELINE)
        return YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE;
    if (capability->encoder_available &&
        !capability->reference_decoder_available)
        return YVEX_QUANT_FAILURE_DECODER_UNAVAILABLE;
    return YVEX_QUANT_FAILURE_ENCODER_UNAVAILABLE;
}

static int quant_decision_geometry(
    yvex_quant_decision *decision,
    const yvex_deepseek_gguf_descriptor *descriptor,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    return quant_decision_geometry_dims(
        decision, descriptor->logical_dims, descriptor->logical_rank,
        failure, err);
}

static int quant_decision_identity(yvex_quant_decision *decision)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned int dimension;

    yvex_sha256_init(&hash);
    if (!quant_sha_text(&hash, "yvex.quant.decision.v1") ||
        !quant_sha_u64(&hash, decision->logical_key.scope) ||
        !quant_sha_u64(&hash, decision->logical_key.subsystem) ||
        !quant_sha_u64(&hash, decision->logical_key.role) ||
        !quant_sha_u64(&hash, decision->logical_key.layer_index) ||
        !quant_sha_u64(&hash, decision->logical_key.auxiliary_index) ||
        !quant_sha_u64(&hash, decision->logical_key.group_index) ||
        !quant_sha_u64(&hash, decision->terminal_value_id) ||
        !quant_sha_u64(&hash, decision->operation) ||
        !quant_sha_u64(&hash, decision->qtype) ||
        !quant_sha_u64(&hash, decision->rank)) return 0;
    for (dimension = 0u; dimension < decision->rank; ++dimension)
        if (!quant_sha_u64(&hash, decision->dims[dimension])) return 0;
    if (!quant_sha_u64(&hash, decision->row_axis) ||
        !quant_sha_u64(&hash, decision->element_count) ||
        !quant_sha_u64(&hash, decision->encoded_bytes) ||
        !quant_sha_u64(&hash, decision->approximation) ||
        !quant_sha_u64(&hash, decision->calibration) ||
        !quant_sha_u64(&hash, decision->numeric_contract_version) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, decision->decision_identity);
    return 1;
}

static int quant_summary_add(yvex_quant_candidate_summary *summary,
                             const yvex_quant_decision *decision)
{
    unsigned long long *class_bytes;

    if (!summary || !decision ||
        decision->qtype > YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID ||
        ULLONG_MAX - summary->encoded_bytes < decision->encoded_bytes ||
        summary->terminal_count == ULLONG_MAX ||
        summary->qtype_tensor_counts[decision->qtype] == ULLONG_MAX)
        return 0;
    if (decision->qtype == YVEX_GGUF_QTYPE_Q8_0)
        class_bytes = &summary->q8_0_bytes;
    else if (decision->qtype == YVEX_GGUF_QTYPE_Q2_K)
        class_bytes = &summary->q2_k_bytes;
    else if (decision->qtype == YVEX_GGUF_QTYPE_MXFP4)
        class_bytes = &summary->mxfp4_bytes;
    else
        class_bytes = &summary->exact_scalar_bytes;
    if (ULLONG_MAX - *class_bytes < decision->encoded_bytes) return 0;
    summary->encoded_bytes += decision->encoded_bytes;
    summary->terminal_count++;
    summary->qtype_tensor_counts[decision->qtype]++;
    *class_bytes += decision->encoded_bytes;
    if (decision->calibration == YVEX_QUANT_CALIBRATION_REQUIRED)
        summary->calibration_required = 1;
    if (!decision->cpu_compute_available || !decision->cuda_compute_available)
        summary->compute_admissible = 0;
    return 1;
}

static int quant_build_candidate_decision(
    yvex_quant_profile_kind profile,
    const yvex_transform_binding *binding,
    const yvex_transform_value *terminal,
    const yvex_transform_node *node,
    const yvex_deepseek_gguf_descriptor *descriptor,
    unsigned long long ordinal,
    yvex_quant_decision *decision,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    const yvex_quant_numeric_capability *capability;
    yvex_transform_physical_decision binding_decision;
    yvex_transform_failure transform_failure;
    int rc;
    unsigned int qtype = quant_candidate_qtype(profile, terminal, node);

    if (qtype == UINT_MAX)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
            ULLONG_MAX, 1u, 0u, qtype, node->kind, err,
            YVEX_ERR_UNSUPPORTED,
            "terminal dtype has no admitted physical scalar representation");
    capability = yvex_quant_numeric_capability_at(qtype);
    if (!capability || !capability->encoder_available ||
        !capability->reference_decoder_available) {
        return quant_plan_fail(
            failure, quant_capability_failure(capability), ordinal,
            ULLONG_MAX, 1u, 0u, qtype, node->kind, err,
            YVEX_ERR_UNSUPPORTED,
            "selected profile requires an unavailable codec");
    }
    memset(decision, 0, sizeof(*decision));
    decision->logical_key = terminal->logical_key;
    decision->terminal_ordinal = ordinal;
    decision->terminal_value_id = terminal->id;
    decision->node_id = node->id;
    decision->role = terminal->logical_key.role;
    decision->scope = terminal->logical_key.scope;
    decision->operation = node->kind;
    decision->qtype = qtype;
    decision->physical_class = qtype == YVEX_GGUF_QTYPE_F32 ||
            qtype == YVEX_GGUF_QTYPE_F16 ||
            qtype == YVEX_GGUF_QTYPE_BF16 ||
            qtype == YVEX_GGUF_QTYPE_I32
        ? YVEX_QUANT_PHYSICAL_EXACT_SCALAR
        : YVEX_QUANT_PHYSICAL_BLOCK_QUANTIZED;
    decision->approximation = qtype == YVEX_GGUF_QTYPE_Q8_0 ||
                              qtype == YVEX_GGUF_QTYPE_Q2_K ||
                              qtype == YVEX_GGUF_QTYPE_MXFP4;
    if (decision->approximation && !terminal->precision.approximation_allowed)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_APPROXIMATION_FORBIDDEN, ordinal,
            ULLONG_MAX, 0u, 1u, qtype, node->kind, err, YVEX_ERR_FORMAT,
            "profile selected approximation for an exact terminal");
    memset(&binding_decision, 0, sizeof(binding_decision));
    binding_decision.physical_class = capability->physical_class_mask;
    binding_decision.encoding_id = qtype;
    binding_decision.approximation_selected = decision->approximation;
    rc = yvex_transform_binding_decision_validate(
        binding, ordinal, &binding_decision, &transform_failure, err);
    if (rc != YVEX_OK)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
            ULLONG_MAX, terminal->precision.allowed_physical_classes,
            capability->physical_class_mask, qtype, node->kind, err,
            YVEX_ERR_FORMAT,
            "profile decision violates the terminal precision constraint");
    decision->calibration = capability->calibration;
    decision->reference_decoder_required = 1;
    decision->cpu_compute_available =
        capability->dedicated_cpu_compute_available;
    decision->cuda_compute_available =
        capability->dedicated_cuda_compute_available;
    decision->numeric_contract_version = capability->numeric_contract_version;
    rc = quant_decision_geometry(decision, descriptor, failure, err);
    if (rc != YVEX_OK) return rc;
    if (!quant_decision_identity(decision))
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal, ULLONG_MAX,
            1u, 0u, qtype, node->kind, err, YVEX_ERR_BOUNDS,
            "decision identity encoding failed");
    return YVEX_OK;
}

static int quant_plan_identity(yvex_quant_plan *plan)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long ordinal;

    yvex_sha256_init(&hash);
    if (!quant_sha_text(&hash, "yvex.quant.plan.v1") ||
        !quant_sha_u64(&hash, plan->summary.schema_version) ||
        !quant_sha_text(&hash, plan->summary.profile_name) ||
        !quant_sha_text(&hash, plan->summary.transform_identity) ||
        !quant_sha_u64(&hash, plan->summary.source_snapshot_identity) ||
        !quant_sha_text(&hash, plan->summary.required_payload_identity) ||
        !quant_sha_u64(&hash, plan->summary.mapping_identity) ||
        !quant_sha_text(&hash, plan->summary.calibration_identity) ||
        !quant_sha_text(&hash, plan->summary.backend_compute_contract) ||
        !quant_sha_u64(&hash, plan->summary.decision_count)) return 0;
    for (ordinal = 0u; ordinal < plan->summary.decision_count; ++ordinal)
        if (!quant_sha_text(&hash,
                            plan->decisions[ordinal].decision_identity))
            return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, plan->summary.profile_identity);
    return 1;
}

/*
 * Builds and seals a caller-described physical plan over any complete binding.
 * The decision array is canonical-terminal ordered; it supplies physical facts
 * only and cannot replace or mutate Transformation IR semantics. No IO occurs.
 */
int yvex_quant_plan_build_explicit(
    yvex_quant_plan **out,
    const yvex_transform_ir *ir,
    const yvex_transform_binding *binding,
    const char *profile_name,
    unsigned long long lowering_identity,
    const yvex_quant_explicit_decision *decisions,
    unsigned long long decision_count,
    const yvex_quant_plan_options *options,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    const yvex_transform_ir_summary *ir_summary =
        yvex_transform_ir_summary_get(ir);
    const yvex_transform_binding_summary *binding_summary =
        yvex_transform_binding_summary_get(binding);
    yvex_quant_plan_options local_options;
    yvex_quant_plan *plan;
    yvex_quant_candidate_summary candidate;
    unsigned long long ordinal;
    size_t decision_bytes;
    size_t index_bytes;
    size_t owned_bytes = SIZE_MAX;

    if (out) *out = NULL;
    if (!out || !ir || !binding || !profile_name || !profile_name[0] ||
        strlen(profile_name) >= sizeof(((yvex_quant_plan_summary *)0)->profile_name) ||
        !lowering_identity || !decisions || !ir_summary || !binding_summary ||
        yvex_transform_binding_ir(binding) != ir || !ir_summary->complete ||
        !binding_summary->complete) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_INVALID_ARG,
            "complete IR/binding, profile, lowering identity, and decisions are required");
    }
    if (decision_count != ir_summary->terminal_count) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ULLONG_MAX,
            ULLONG_MAX, ir_summary->terminal_count, decision_count, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
            "explicit plan must decide every canonical terminal exactly once");
    }
    if (strcmp(ir_summary->transform_identity,
               binding_summary->transform_identity) != 0) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_TRANSFORM_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_FORMAT, "binding and IR identities diverge");
    }
    if (ir_summary->source_snapshot_identity !=
            binding_summary->source_snapshot_identity) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_SOURCE_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, ir_summary->source_snapshot_identity,
            binding_summary->source_snapshot_identity, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
            "binding and IR source identities diverge");
    }
    if (strcmp(ir_summary->required_payload_identity,
               binding_summary->required_payload_identity) != 0) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_PAYLOAD_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_FORMAT, "binding and IR payload identities diverge");
    }

    memset(&local_options, 0, sizeof(local_options));
    local_options.allocate = quant_plan_default_allocate;
    local_options.release = quant_plan_default_release;
    local_options.maximum_owned_bytes = 16u * 1024u * 1024u;
    if (options) local_options = *options;
    if (!local_options.allocate)
        local_options.allocate = quant_plan_default_allocate;
    if (!local_options.release)
        local_options.release = quant_plan_default_release;
    if (!local_options.maximum_owned_bytes)
        local_options.maximum_owned_bytes = 16u * 1024u * 1024u;

    plan = (yvex_quant_plan *)local_options.allocate(
        sizeof(*plan), local_options.context);
    if (!plan) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX, ULLONG_MAX,
            sizeof(*plan), 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_NOMEM, "explicit quantization plan allocation failed");
    }
    memset(plan, 0, sizeof(*plan));
    plan->allocate = local_options.allocate;
    plan->release = local_options.release;
    plan->allocator_context = local_options.context;
    plan->ir = ir;
    plan->binding = binding;
    plan->summary.schema_version = YVEX_QUANT_PROFILE_SCHEMA_VERSION;
    plan->summary.state = YVEX_QUANT_PLAN_BUILDING;
    plan->summary.terminal_count = ir_summary->terminal_count;
    plan->summary.source_value_count = ir_summary->source_value_count;
    plan->summary.source_snapshot_identity = ir_summary->source_snapshot_identity;
    plan->summary.mapping_identity = lowering_identity;
    (void)snprintf(plan->summary.profile_name,
                   sizeof(plan->summary.profile_name), "%s", profile_name);
    (void)snprintf(plan->summary.transform_identity,
                   sizeof(plan->summary.transform_identity), "%s",
                   ir_summary->transform_identity);
    (void)snprintf(plan->summary.required_payload_identity,
                   sizeof(plan->summary.required_payload_identity), "%s",
                   ir_summary->required_payload_identity);
    (void)snprintf(plan->summary.backend_compute_contract,
                   sizeof(plan->summary.backend_compute_contract), "%s",
                   "cpu-cuda-encoded-row-dot-v1");
    (void)snprintf(plan->summary.calibration_identity,
                   sizeof(plan->summary.calibration_identity), "%s",
                   "no-calibration-required");
    plan->summary.index_capacity = quant_index_capacity(decision_count);
    if (!plan->summary.index_capacity ||
        decision_count > SIZE_MAX / sizeof(*plan->decisions) ||
        plan->summary.index_capacity > SIZE_MAX / sizeof(*plan->index)) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ULLONG_MAX,
            ULLONG_MAX, SIZE_MAX, decision_count, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_BOUNDS,
            "explicit plan allocation geometry overflowed");
    }
    decision_bytes = (size_t)decision_count * sizeof(*plan->decisions);
    index_bytes = (size_t)plan->summary.index_capacity * sizeof(*plan->index);
    if (!quant_size_add(sizeof(*plan), decision_bytes, &owned_bytes) ||
        !quant_size_add(owned_bytes, index_bytes, &owned_bytes) ||
        owned_bytes > local_options.maximum_owned_bytes) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ULLONG_MAX,
            ULLONG_MAX, local_options.maximum_owned_bytes,
            owned_bytes, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_BOUNDS,
            "explicit plan ownership budget exceeded");
    }
    plan->decisions = (yvex_quant_decision *)plan->allocate(
        decision_bytes, plan->allocator_context);
    plan->index = (quant_plan_slot *)plan->allocate(
        index_bytes, plan->allocator_context);
    if (!plan->decisions || !plan->index) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX, ULLONG_MAX,
            owned_bytes - sizeof(*plan), 0u, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_NOMEM,
            "explicit decision/index allocation failed");
    }
    memset(plan->decisions, 0, decision_bytes);
    memset(plan->index, 0, index_bytes);
    plan->summary.owned_bytes = owned_bytes;
    plan->summary.peak_builder_bytes = plan->summary.owned_bytes;
    memset(&candidate, 0, sizeof(candidate));
    candidate.kind = YVEX_QUANT_PROFILE_RELEASE_Q8_Q2;
    candidate.name = plan->summary.profile_name;
    candidate.compute_admissible = 1;

    for (ordinal = 0u; ordinal < decision_count; ++ordinal) {
        const yvex_transform_value *terminal =
            yvex_transform_ir_terminal_at(ir, ordinal);
        const yvex_transform_node *node = terminal
            ? yvex_transform_ir_node_at(ir, terminal->producer_node_id) : NULL;
        const yvex_quant_explicit_decision *spec = &decisions[ordinal];
        const yvex_quant_numeric_capability *capability =
            yvex_quant_numeric_capability_at(spec->qtype);
        yvex_quant_decision *decision = &plan->decisions[ordinal];
        yvex_transform_physical_decision binding_decision;
        yvex_transform_failure transform_failure;
        unsigned long long logical_elements;
        unsigned long long hash;
        unsigned long long slot;
        unsigned long long probe;
        int logical_geometry_ok;
        int rc;

        if (!terminal || !node || terminal->canonical_ordinal != ordinal ||
            yvex_transform_binding_terminal_at(binding, ordinal) != terminal ||
            yvex_transform_binding_terminal_operation(binding, ordinal) != node) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ordinal,
                ULLONG_MAX, ordinal,
                terminal ? terminal->canonical_ordinal : ULLONG_MAX,
                spec->qtype, node ? node->kind : YVEX_TRANSFORM_OP_COUNT,
                err, YVEX_ERR_FORMAT,
                "binding does not expose the canonical terminal operation");
        }
        if (!capability || !capability->identity_known) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_UNKNOWN_QTYPE, ordinal,
                ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, err,
                YVEX_ERR_UNSUPPORTED, "explicit qtype identity is unknown");
        }
        if (!capability->encoder_available ||
            !capability->reference_decoder_available) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, quant_capability_failure(capability), ordinal,
                ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, err,
                YVEX_ERR_UNSUPPORTED,
                "explicit plan selected an unavailable codec");
        }
        if (node->kind >= YVEX_TRANSFORM_OP_COUNT ||
            !(capability->transform_kind_mask & (1u << node->kind))) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_UNSUPPORTED_OPERATION, ordinal,
                ULLONG_MAX, capability->transform_kind_mask, node->kind,
                spec->qtype, node->kind, err, YVEX_ERR_UNSUPPORTED,
                "explicit qtype does not admit the terminal operation");
        }
        if (spec->row_axis != 0u) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_INVALID_ROW_AXIS, ordinal,
                ULLONG_MAX, 0u, spec->row_axis, spec->qtype, node->kind,
                err, YVEX_ERR_UNSUPPORTED,
                "current physical geometry requires qtype rows on axis zero");
        }
        memset(decision, 0, sizeof(*decision));
        decision->logical_key = terminal->logical_key;
        decision->terminal_ordinal = ordinal;
        decision->terminal_value_id = terminal->id;
        decision->node_id = node->id;
        decision->role = terminal->logical_key.role;
        decision->scope = terminal->logical_key.scope;
        decision->operation = node->kind;
        decision->qtype = spec->qtype;
        decision->physical_class =
            capability->physical_class_mask == YVEX_TRANSFORM_PHYSICAL_QUANTIZED
                ? YVEX_QUANT_PHYSICAL_BLOCK_QUANTIZED
                : YVEX_QUANT_PHYSICAL_EXACT_SCALAR;
        decision->approximation = spec->approximation;
        if (decision->approximation &&
            !terminal->precision.approximation_allowed) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_APPROXIMATION_FORBIDDEN,
                ordinal, ULLONG_MAX, 0u, 1u, spec->qtype, node->kind,
                err, YVEX_ERR_FORMAT,
                "explicit plan selected approximation for an exact terminal");
        }
        memset(&binding_decision, 0, sizeof(binding_decision));
        binding_decision.physical_class = capability->physical_class_mask;
        binding_decision.encoding_id = spec->qtype;
        binding_decision.approximation_selected = spec->approximation;
        rc = yvex_transform_binding_decision_validate(
            binding, ordinal, &binding_decision, &transform_failure, err);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
                ULLONG_MAX, terminal->precision.allowed_physical_classes,
                capability->physical_class_mask, spec->qtype, node->kind,
                err, YVEX_ERR_FORMAT,
                "explicit decision violates the terminal precision constraint");
        }
        decision->calibration = capability->calibration;
        decision->reference_decoder_required = 1;
        decision->cpu_compute_available =
            capability->dedicated_cpu_compute_available;
        decision->cuda_compute_available =
            capability->dedicated_cuda_compute_available;
        decision->numeric_contract_version =
            capability->numeric_contract_version;
        rc = quant_decision_geometry_dims(
            decision, spec->dims, spec->rank, failure, err);
        logical_geometry_ok = quant_transform_element_count(
            &terminal->shape, &logical_elements);
        if (rc == YVEX_OK &&
            (!logical_geometry_ok ||
             logical_elements != decision->element_count)) {
            rc = quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_INVALID_DIMENSION, ordinal,
                ULLONG_MAX, logical_geometry_ok
                    ? logical_elements : ULLONG_MAX,
                decision->element_count, spec->qtype, node->kind, err,
                YVEX_ERR_BOUNDS,
                "physical decision element count differs from its logical terminal");
            yvex_quant_plan_release(&plan);
            return rc;
        }
        if (rc != YVEX_OK || !quant_decision_identity(decision)) {
            yvex_quant_plan_release(&plan);
            return rc != YVEX_OK ? rc : quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, err,
                YVEX_ERR_BOUNDS, "explicit decision identity failed");
        }
        if (!quant_summary_add(&candidate, decision)) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                ULLONG_MAX, ULLONG_MAX, candidate.encoded_bytes,
                spec->qtype, node->kind, err, YVEX_ERR_BOUNDS,
                "explicit plan byte accounting overflowed");
        }
        hash = quant_key_hash(&decision->logical_key);
        slot = hash & (plan->summary.index_capacity - 1u);
        for (probe = 0u; probe < plan->summary.index_capacity; ++probe) {
            if (!plan->index[slot].ordinal_plus_one) break;
            if (plan->index[slot].hash == hash &&
                quant_key_equal(
                    &plan->decisions[
                        plan->index[slot].ordinal_plus_one - 1u].logical_key,
                    &decision->logical_key)) {
                yvex_quant_plan_release(&plan);
                return quant_plan_fail(
                    failure, YVEX_QUANT_FAILURE_DUPLICATE_DECISION,
                    ordinal, ULLONG_MAX, 1u, 2u, spec->qtype, node->kind,
                    err, YVEX_ERR_FORMAT,
                    "duplicate logical terminal decision refused");
            }
            slot = (slot + 1u) & (plan->summary.index_capacity - 1u);
        }
        if (probe == plan->summary.index_capacity) {
            rc = quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ordinal,
                ULLONG_MAX, plan->summary.index_capacity,
                plan->summary.index_capacity, spec->qtype, node->kind,
                err, YVEX_ERR_BOUNDS, "explicit decision index exhausted");
            yvex_quant_plan_release(&plan);
            return rc;
        }
        plan->index[slot].hash = hash;
        plan->index[slot].ordinal_plus_one = ordinal + 1u;
        plan->summary.decision_count++;
        plan->summary.qtype_tensor_counts[decision->qtype]++;
        if ((unsigned int)decision->role < YVEX_TENSOR_ROLE_COUNT)
            plan->summary.role_tensor_counts[decision->role]++;
    }

    candidate.numerically_admissible = 1;
    plan->summary.candidates[0] = candidate;
    plan->summary.encoded_bytes = candidate.encoded_bytes;
    plan->summary.exact_scalar_bytes = candidate.exact_scalar_bytes;
    plan->summary.q8_0_bytes = candidate.q8_0_bytes;
    plan->summary.q2_k_bytes = candidate.q2_k_bytes;
    plan->summary.mxfp4_bytes = candidate.mxfp4_bytes;
    plan->summary.calibration_required = candidate.calibration_required;
    if (plan->summary.decision_count != decision_count ||
        plan->summary.calibration_required || !candidate.compute_admissible ||
        !quant_plan_identity(plan)) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_INCOMPLETE, ULLONG_MAX, ULLONG_MAX,
            decision_count, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_FORMAT, "explicit quantization plan did not seal");
    }
    plan->summary.state = YVEX_QUANT_PLAN_SEALED;
    plan->summary.complete = 1;
    *out = plan;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * Accounts both admitted candidates and seals the caller-selected canonical
 * profile. The selected decisions are independently owned; no source IO or
 * payload mutation occurs, and every construction failure releases the plan.
 */
int yvex_deepseek_quant_plan_build_profile(
    yvex_quant_plan **out,
    const yvex_transform_ir *ir,
    const yvex_transform_binding *binding,
    const yvex_deepseek_gguf_map *map,
    yvex_quant_profile_kind profile,
    const yvex_quant_plan_options *options,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    const yvex_transform_ir_summary *ir_summary =
        yvex_transform_ir_summary_get(ir);
    const yvex_transform_binding_summary *binding_summary =
        yvex_transform_binding_summary_get(binding);
    const yvex_deepseek_gguf_map_summary *map_summary =
        yvex_deepseek_gguf_map_summary_get(map);
    yvex_quant_plan_options local_options;
    yvex_quant_plan *plan;
    unsigned long long ordinal;
    size_t decision_bytes;
    size_t index_bytes;
    size_t owned_bytes = SIZE_MAX;

    if (out) *out = NULL;
    if (!out || !ir || !binding || !map ||
        (profile != YVEX_QUANT_PROFILE_SOURCE_FAITHFUL &&
         profile != YVEX_QUANT_PROFILE_RELEASE_Q8_Q2) ||
        yvex_transform_binding_ir(binding) != ir || !ir_summary ||
        !binding_summary || !map_summary || !ir_summary->complete ||
        !binding_summary->complete || !map_summary->complete) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_INVALID_ARG,
            "sealed IR, complete binding, lowering, and output are required");
    }
    if (strcmp(ir_summary->transform_identity,
               binding_summary->transform_identity) != 0)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_TRANSFORM_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_FORMAT, "binding and IR identities diverge");
    if (ir_summary->source_snapshot_identity !=
            binding_summary->source_snapshot_identity)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_SOURCE_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, ir_summary->source_snapshot_identity,
            binding_summary->source_snapshot_identity, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
            "binding and IR source identities diverge");
    if (strcmp(ir_summary->required_payload_identity,
               binding_summary->required_payload_identity) != 0)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_PAYLOAD_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_FORMAT, "binding and IR payload identities diverge");
    if (map_summary->mapping_identity != YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_MAPPING_IDENTITY, ULLONG_MAX,
            ULLONG_MAX, YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY,
            map_summary->mapping_identity, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
            "GGUF mapping identity is not the pinned lowering");
    if (ir_summary->terminal_count != YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        ir_summary->source_value_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        map_summary->descriptor_count != ir_summary->terminal_count) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ULLONG_MAX,
            ULLONG_MAX, YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
            ir_summary->terminal_count, UINT_MAX, YVEX_TRANSFORM_OP_COUNT,
            err, YVEX_ERR_FORMAT,
            "target-scale terminal or source accounting is incomplete");
    }
    memset(&local_options, 0, sizeof(local_options));
    local_options.allocate = quant_plan_default_allocate;
    local_options.release = quant_plan_default_release;
    local_options.maximum_owned_bytes = 16u * 1024u * 1024u;
    if (options) local_options = *options;
    if (!local_options.allocate) local_options.allocate = quant_plan_default_allocate;
    if (!local_options.release) local_options.release = quant_plan_default_release;
    if (!local_options.maximum_owned_bytes)
        local_options.maximum_owned_bytes = 16u * 1024u * 1024u;
    plan = (yvex_quant_plan *)local_options.allocate(
        sizeof(*plan), local_options.context);
    if (!plan)
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX, ULLONG_MAX,
            sizeof(*plan), 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
            YVEX_ERR_NOMEM, "quantization plan allocation failed");
    memset(plan, 0, sizeof(*plan));
    plan->allocate = local_options.allocate;
    plan->release = local_options.release;
    plan->allocator_context = local_options.context;
    plan->ir = ir;
    plan->binding = binding;
    plan->map = map;
    plan->summary.schema_version = YVEX_QUANT_PROFILE_SCHEMA_VERSION;
    plan->summary.state = YVEX_QUANT_PLAN_BUILDING;
    plan->summary.terminal_count = ir_summary->terminal_count;
    plan->summary.source_value_count = ir_summary->source_value_count;
    plan->summary.source_snapshot_identity = ir_summary->source_snapshot_identity;
    plan->summary.mapping_identity = map_summary->mapping_identity;
    (void)snprintf(plan->summary.profile_name,
                   sizeof(plan->summary.profile_name), "%s",
                   profile == YVEX_QUANT_PROFILE_SOURCE_FAITHFUL
                       ? YVEX_QUANT_REFERENCE_PROFILE_NAME
                       : YVEX_QUANT_RELEASE_PROFILE_NAME);
    (void)snprintf(plan->summary.transform_identity,
                   sizeof(plan->summary.transform_identity), "%s",
                   ir_summary->transform_identity);
    (void)snprintf(plan->summary.required_payload_identity,
                   sizeof(plan->summary.required_payload_identity), "%s",
                   ir_summary->required_payload_identity);
    (void)snprintf(plan->summary.backend_compute_contract,
                   sizeof(plan->summary.backend_compute_contract), "%s",
                   "cpu-cuda-encoded-row-dot-v1");
    (void)snprintf(plan->summary.calibration_identity,
                   sizeof(plan->summary.calibration_identity), "%s",
                   "no-calibration-required");
    plan->summary.index_capacity = quant_index_capacity(
        ir_summary->terminal_count);
    if (!plan->summary.index_capacity ||
        ir_summary->terminal_count > SIZE_MAX / sizeof(*plan->decisions) ||
        plan->summary.index_capacity > SIZE_MAX / sizeof(*plan->index)) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ULLONG_MAX,
            ULLONG_MAX, SIZE_MAX, ir_summary->terminal_count, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_BOUNDS,
            "quantization plan allocation geometry overflowed");
    }
    decision_bytes = (size_t)ir_summary->terminal_count *
                     sizeof(*plan->decisions);
    index_bytes = (size_t)plan->summary.index_capacity * sizeof(*plan->index);
    if (!quant_size_add(sizeof(*plan), decision_bytes, &owned_bytes) ||
        !quant_size_add(owned_bytes, index_bytes, &owned_bytes) ||
        owned_bytes > local_options.maximum_owned_bytes) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ULLONG_MAX,
            ULLONG_MAX, local_options.maximum_owned_bytes,
            owned_bytes, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_BOUNDS,
            "quantization plan ownership budget exceeded");
    }
    plan->decisions = (yvex_quant_decision *)plan->allocate(
        decision_bytes, plan->allocator_context);
    plan->index = (quant_plan_slot *)plan->allocate(
        index_bytes, plan->allocator_context);
    if (!plan->decisions || !plan->index) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX, ULLONG_MAX,
            owned_bytes - sizeof(*plan), 0u, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_NOMEM,
            "quantization decision/index allocation failed");
    }
    memset(plan->decisions, 0, decision_bytes);
    memset(plan->index, 0, index_bytes);
    plan->summary.owned_bytes = owned_bytes;
    plan->summary.peak_builder_bytes = plan->summary.owned_bytes;
    plan->summary.candidates[0].kind = YVEX_QUANT_PROFILE_SOURCE_FAITHFUL;
    plan->summary.candidates[0].name = YVEX_QUANT_REFERENCE_PROFILE_NAME;
    plan->summary.candidates[0].compute_admissible = 1;
    plan->summary.candidates[1].kind = YVEX_QUANT_PROFILE_RELEASE_Q8_Q2;
    plan->summary.candidates[1].name = YVEX_QUANT_RELEASE_PROFILE_NAME;
    plan->summary.candidates[1].compute_admissible = 1;
    for (ordinal = 0u; ordinal < ir_summary->terminal_count; ++ordinal) {
        const yvex_transform_value *terminal =
            yvex_transform_ir_terminal_at(ir, ordinal);
        const yvex_transform_node *node = terminal
            ? yvex_transform_ir_node_at(ir, terminal->producer_node_id) : NULL;
        const yvex_deepseek_gguf_descriptor *descriptor =
            yvex_deepseek_gguf_map_at(map, ordinal);
        yvex_quant_decision reference_decision;
        yvex_quant_decision release_decision;
        yvex_quant_decision *decision = &plan->decisions[ordinal];
        unsigned long long hash;
        unsigned long long slot;
        unsigned long long probe;
        int rc;

        if (!terminal || !node || terminal->canonical_ordinal != ordinal ||
            yvex_transform_binding_terminal_at(binding, ordinal) != terminal ||
            yvex_transform_binding_terminal_operation(binding, ordinal) != node) {
            yvex_quant_plan_release(&plan);
            return quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ordinal,
                ULLONG_MAX, ordinal,
                terminal ? terminal->canonical_ordinal : ULLONG_MAX,
                UINT_MAX, node ? node->kind : YVEX_TRANSFORM_OP_COUNT, err,
                YVEX_ERR_FORMAT,
                "binding does not expose the canonical terminal operation");
        }
        rc = quant_descriptor_matches(ir, terminal, node, map, descriptor,
                                      ordinal, failure, err);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&plan);
            return rc;
        }
        rc = quant_build_candidate_decision(
            YVEX_QUANT_PROFILE_SOURCE_FAITHFUL, binding, terminal, node,
            descriptor, ordinal, &reference_decision, failure, err);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&plan);
            return rc;
        }
        if (!quant_summary_add(
                &plan->summary.candidates[0], &reference_decision)) {
            rc = quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                ULLONG_MAX, ULLONG_MAX,
                plan->summary.candidates[0].encoded_bytes, UINT_MAX,
                node->kind, err, YVEX_ERR_BOUNDS,
                "reference candidate byte accounting overflowed");
            yvex_quant_plan_release(&plan);
            return rc;
        }
        rc = quant_build_candidate_decision(
            YVEX_QUANT_PROFILE_RELEASE_Q8_Q2, binding, terminal, node,
            descriptor, ordinal, &release_decision, failure, err);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&plan);
            return rc;
        }
        if (!quant_summary_add(&plan->summary.candidates[1],
                               &release_decision)) {
            rc = quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                ULLONG_MAX, ULLONG_MAX,
                plan->summary.candidates[1].encoded_bytes, UINT_MAX,
                node->kind, err, YVEX_ERR_BOUNDS,
                "release candidate byte accounting overflowed");
            yvex_quant_plan_release(&plan);
            return rc;
        }
        *decision = profile == YVEX_QUANT_PROFILE_SOURCE_FAITHFUL
            ? reference_decision : release_decision;
        hash = quant_key_hash(&decision->logical_key);
        slot = hash & (plan->summary.index_capacity - 1u);
        for (probe = 0u; probe < plan->summary.index_capacity; ++probe) {
            if (!plan->index[slot].ordinal_plus_one) break;
            if (plan->index[slot].hash == hash &&
                quant_key_equal(
                    &plan->decisions[
                        plan->index[slot].ordinal_plus_one - 1u].logical_key,
                    &decision->logical_key)) {
                rc = quant_plan_fail(
                    failure, YVEX_QUANT_FAILURE_DUPLICATE_DECISION,
                    ordinal, ULLONG_MAX, 1u, 2u, decision->qtype,
                    node->kind, err, YVEX_ERR_FORMAT,
                    "duplicate logical terminal decision refused");
                yvex_quant_plan_release(&plan);
                return rc;
            }
            slot = (slot + 1u) & (plan->summary.index_capacity - 1u);
        }
        if (probe == plan->summary.index_capacity) {
            rc = quant_plan_fail(
                failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ordinal,
                ULLONG_MAX, plan->summary.index_capacity,
                plan->summary.index_capacity, decision->qtype, node->kind,
                err, YVEX_ERR_BOUNDS,
                "quantization decision index is exhausted");
            yvex_quant_plan_release(&plan);
            return rc;
        }
        plan->index[slot].hash = hash;
        plan->index[slot].ordinal_plus_one = ordinal + 1u;
        plan->summary.decision_count++;
        plan->summary.qtype_tensor_counts[decision->qtype]++;
        plan->summary.role_tensor_counts[decision->role]++;
    }
    plan->summary.candidates[0].numerically_admissible = 1;
    plan->summary.candidates[1].numerically_admissible = 1;
    plan->summary.encoded_bytes = plan->summary.candidates[profile].encoded_bytes;
    plan->summary.exact_scalar_bytes =
        plan->summary.candidates[profile].exact_scalar_bytes;
    plan->summary.q8_0_bytes = plan->summary.candidates[profile].q8_0_bytes;
    plan->summary.q2_k_bytes = plan->summary.candidates[profile].q2_k_bytes;
    plan->summary.mxfp4_bytes = plan->summary.candidates[profile].mxfp4_bytes;
    plan->summary.calibration_required =
        plan->summary.candidates[profile].calibration_required;
    if (plan->summary.decision_count != ir_summary->terminal_count ||
        plan->summary.calibration_required ||
        !plan->summary.candidates[profile].compute_admissible ||
        !quant_plan_identity(plan)) {
        yvex_quant_plan_release(&plan);
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_INCOMPLETE, ULLONG_MAX, ULLONG_MAX,
            ir_summary->terminal_count, 0u, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
            "quantization profile did not seal completely");
    }
    plan->summary.state = YVEX_QUANT_PLAN_SEALED;
    plan->summary.complete = 1;
    plan->summary.payload_bytes_read = 0u;
    *out = plan;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Seals the fixed selected v0.1 release profile for existing consumers. */
int yvex_deepseek_quant_plan_build(
    yvex_quant_plan **out,
    const yvex_transform_ir *ir,
    const yvex_transform_binding *binding,
    const yvex_deepseek_gguf_map *map,
    const yvex_quant_plan_options *options,
    yvex_quant_failure *failure,
    yvex_error *err)
{
    return yvex_deepseek_quant_plan_build_profile(
        out, ir, binding, map, YVEX_QUANT_PROFILE_RELEASE_Q8_Q2,
        options, failure, err);
}

/* Releases all independently owned plan memory and nulls the caller handle. */
void yvex_quant_plan_release(yvex_quant_plan **plan_address)
{
    yvex_quant_plan *plan;
    yvex_quant_release_fn release;
    void *context;

    if (!plan_address || !*plan_address) return;
    plan = *plan_address;
    *plan_address = NULL;
    release = plan->release ? plan->release : quant_plan_default_release;
    context = plan->allocator_context;
    if (plan->index) release(plan->index, context);
    if (plan->decisions) release(plan->decisions, context);
    plan->summary.state = YVEX_QUANT_PLAN_RELEASED;
    release(plan, context);
}

const yvex_quant_plan_summary *yvex_quant_plan_summary_get(
    const yvex_quant_plan *plan)
{
    return plan ? &plan->summary : NULL;
}

const yvex_quant_decision *yvex_quant_plan_decision_at(
    const yvex_quant_plan *plan, unsigned long long ordinal)
{
    return plan && plan->summary.state == YVEX_QUANT_PLAN_SEALED &&
           ordinal < plan->summary.decision_count
        ? &plan->decisions[ordinal] : NULL;
}

/* Performs expected-O(1) immutable logical-key lookup without mutable counters. */
const yvex_quant_decision *yvex_quant_plan_find(
    const yvex_quant_plan *plan, const yvex_transform_logical_key *key)
{
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long probe;

    if (!plan || plan->summary.state != YVEX_QUANT_PLAN_SEALED || !key)
        return NULL;
    hash = quant_key_hash(key);
    slot = hash & (plan->summary.index_capacity - 1u);
    for (probe = 0u; probe < plan->summary.index_capacity &&
         plan->index[slot].ordinal_plus_one; ++probe) {
        if (plan->index[slot].hash == hash) {
            const yvex_quant_decision *decision =
                &plan->decisions[plan->index[slot].ordinal_plus_one - 1u];
            if (quant_key_equal(&decision->logical_key, key)) return decision;
        }
        slot = (slot + 1u) & (plan->summary.index_capacity - 1u);
    }
    return NULL;
}

const yvex_transform_ir *yvex_quant_plan_transform_ir(
    const yvex_quant_plan *plan)
{
    return plan ? plan->ir : NULL;
}

const yvex_transform_binding *yvex_quant_plan_binding(
    const yvex_quant_plan *plan)
{
    return plan ? plan->binding : NULL;
}

const yvex_deepseek_gguf_map *yvex_quant_plan_lowering(
    const yvex_quant_plan *plan)
{
    return plan ? plan->map : NULL;
}
