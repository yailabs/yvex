/* Owner: gguf.quant plan (TRACK.QUANT).
 * Owns: candidate accounting, fixed v0.1 selection, terminal decisions, descriptor bijection, canonical profile
 *   identity, indexes, and cleanup.
 * Does not own: IR semantics, source IO, GGUF naming/mapping identity, numeric conversion, calibration collection,
 *   writing, runtime, or rendering.
 * Invariants: 1,360 canonical terminals and descriptors biject after complete typed-field validation; construction
 *   reads zero payload bytes.
 * Boundary: this chooses physical encodings but produces no encoded payload.
 * Purpose: seal deterministic physical decisions over immutable transform plans.
 * Inputs: admitted IR/binding, lowering descriptors, profile, and resource budget.
 * Effects: allocates indexed decision plans with canonical semantic identities.
 * Failure: typed refusal releases partial ownership and reads no payload bytes. */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/core.h>
#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/quant_numeric.h>

typedef struct {
    unsigned long long hash;
    unsigned long long ordinal_plus_one;
} quant_plan_slot;

typedef struct {
    yvex_transform_operation_kind operation;
    yvex_deepseek_gguf_transform lowering;
} operation_lowering;

static const operation_lowering operation_lowerings[] = {
    {YVEX_TRANSFORM_OP_IDENTITY, YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT},
    {YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR, YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0},
    {YVEX_TRANSFORM_OP_CHECKED_CAST, YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32},
    {YVEX_TRANSFORM_OP_EXPERT_AGGREGATE, YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4},
};

static const unsigned int exact_qtypes[YVEX_TRANSFORM_DTYPE_REAL + 1u] = {
    [YVEX_TRANSFORM_DTYPE_F32] = YVEX_GGUF_QTYPE_F32,
    [YVEX_TRANSFORM_DTYPE_F16] = YVEX_GGUF_QTYPE_F16,
    [YVEX_TRANSFORM_DTYPE_BF16] = YVEX_GGUF_QTYPE_BF16,
    [YVEX_TRANSFORM_DTYPE_I32] = YVEX_GGUF_QTYPE_I32,
    [YVEX_TRANSFORM_DTYPE_REAL] = YVEX_GGUF_QTYPE_F32,
};

static const yvex_quant_failure_code
    storage_failures[YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH + 1u] = {
        [YVEX_GGUF_QTYPE_STORAGE_INVALID_ARGUMENT] = YVEX_QUANT_FAILURE_INVALID_ARGUMENT,
        [YVEX_GGUF_QTYPE_STORAGE_UNKNOWN_ID] = YVEX_QUANT_FAILURE_UNKNOWN_QTYPE,
        [YVEX_GGUF_QTYPE_STORAGE_REMOVED_ID] = YVEX_QUANT_FAILURE_REMOVED_QTYPE,
        [YVEX_GGUF_QTYPE_STORAGE_RESERVED_ID] = YVEX_QUANT_FAILURE_UNKNOWN_QTYPE,
        [YVEX_GGUF_QTYPE_STORAGE_OUTSIDE_BASELINE] =
            YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE,
        [YVEX_GGUF_QTYPE_STORAGE_GEOMETRY_UNAVAILABLE] = YVEX_QUANT_FAILURE_UNKNOWN_QTYPE,
        [YVEX_GGUF_QTYPE_STORAGE_INVALID_RANK] = YVEX_QUANT_FAILURE_INVALID_RANK,
        [YVEX_GGUF_QTYPE_STORAGE_INVALID_DIMENSION] = YVEX_QUANT_FAILURE_INVALID_DIMENSION,
        [YVEX_GGUF_QTYPE_STORAGE_ROW_BLOCK_MISMATCH] = YVEX_QUANT_FAILURE_ROW_DIVISIBILITY,
        [YVEX_GGUF_QTYPE_STORAGE_ELEMENT_COUNT_OVERFLOW] = YVEX_QUANT_FAILURE_ELEMENT_OVERFLOW,
};

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

/* Purpose: provide the default zero-initialized quant-plan allocator.
 * Inputs: requested allocation size and ignored allocator context.
 * Effects: allocates caller-owned heap storage.
 * Failure: returns null when the system allocator cannot satisfy the request.
 * Boundary: default seam only; custom allocators remain plan-owned policy. */
static void *quant_plan_default_allocate(size_t size, void *context) {
    (void)context;
    return calloc(1u, size);
}

/* Purpose: release storage created by the default quant-plan allocator.
 * Inputs: optional heap allocation and ignored allocator context.
 * Effects: returns storage to the system allocator.
 * Failure: releasing null is accepted by the underlying allocator.
 * Boundary: never releases borrowed IR, binding, or lowering owners. */
static void quant_plan_default_release(void *allocation, void *context) {
    (void)context;
    free(allocation);
}

/* Purpose: publish one structured quantization-plan refusal.
 * Inputs: typed code, terminal/source facts, qtype, operation, and status.
 * Effects: resets the failure record and updates the shared error object.
 * Failure: returns the supplied status without exposing a sealed plan.
 * Boundary: centralizes diagnostics while callers retain decision policy. */
static int quant_plan_fail(yvex_quant_failure *failure, yvex_quant_failure_code code,
                           unsigned long long terminal, unsigned long long source,
                           unsigned long long expected, unsigned long long actual,
                           unsigned int qtype, yvex_transform_operation_kind operation,
                           yvex_error *err, int status, const char *message) {
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

/* Purpose: fold one explicit-width value into the logical-key hash.
 * Inputs: current FNV-style hash and unsigned 64-bit value.
 * Effects: none outside the returned hash.
 * Failure: deterministic arithmetic wraps by the defined unsigned rules.
 * Boundary: lookup hash only; canonical identity uses SHA-256 encoding. */
static unsigned long long quant_hash_u64(unsigned long long hash, unsigned long long value) {
    unsigned int index;
    for (index = 0u; index < 8u; ++index) {
        hash ^= (value >> (index * 8u)) & 0xffu;
        hash *= 1099511628211ull;
    }
    return hash;
}

/* Purpose: hash every typed logical-key field for immutable indexing.
 * Inputs: valid artifact-neutral logical tensor key.
 * Effects: none.
 * Failure: admitted keys always yield a deterministic non-authoritative hash.
 * Boundary: excludes GGUF names and physical encoding decisions. */
static unsigned long long quant_key_hash(const yvex_transform_logical_key *key) {
    unsigned long long hash = 1469598103934665603ull;
    hash = quant_hash_u64(hash, key->scope);
    hash = quant_hash_u64(hash, key->subsystem);
    hash = quant_hash_u64(hash, key->role);
    hash = quant_hash_u64(hash, key->layer_index);
    hash = quant_hash_u64(hash, key->auxiliary_index);
    return quant_hash_u64(hash, key->group_index);
}

/* Purpose: compare artifact-neutral logical tensor keys exactly.
 * Inputs: two admitted logical keys.
 * Effects: none.
 * Failure: returns false on any semantic-field mismatch.
 * Boundary: equality ignores allocation and emitted-name details. */
static int quant_key_equal(const yvex_transform_logical_key *left,
                           const yvex_transform_logical_key *right) {
    return left->scope == right->scope && left->subsystem == right->subsystem &&
           left->role == right->role && left->layer_index == right->layer_index &&
           left->auxiliary_index == right->auxiliary_index &&
           left->group_index == right->group_index;
}

/* Purpose: derive a power-of-two open-addressing capacity at bounded load.
 * Inputs: required terminal count.
 * Effects: none.
 * Failure: returns zero when doubling would overflow.
 * Boundary: capacity affects lookup storage, never plan identity. */
static unsigned long long quant_index_capacity(unsigned long long count) {
    unsigned long long capacity = 1u;
    if (count > ULLONG_MAX / 2u)
        return 0u;
    while (capacity < count * 2u) {
        if (capacity > ULLONG_MAX / 2u)
            return 0u;
        capacity *= 2u;
    }
    return capacity;
}

/* Purpose: add one ownership size without diagnostic arithmetic wrap.
 * Inputs: size operands and caller-owned result.
 * Effects: writes the sum or a saturated failure sentinel.
 * Failure: returns false for null output or overflow.
 * Boundary: tracks memory ownership only, not encoded model bytes. */
static int quant_size_add(size_t left, size_t right, size_t *out) {
    if (!out)
        return 0;
    if (left > SIZE_MAX - right) {
        *out = SIZE_MAX;
        return 0;
    }
    *out = left + right;
    return 1;
}

/* Purpose: compute logical element count with rank and overflow refusal.
 * Inputs: immutable transform shape and caller-owned count.
 * Effects: writes exact element count on success.
 * Failure: returns false for invalid rank/dimensions or product overflow.
 * Boundary: interprets logical shape only, not qtype row geometry. */
static int quant_transform_element_count(const yvex_transform_shape *shape,
                                         unsigned long long *out) {
    unsigned long long count = 1u;
    unsigned int dimension;

    if (out)
        *out = 0u;
    if (!shape || !out || !shape->rank || shape->rank > YVEX_TRANSFORM_IR_MAX_RANK)
        return 0;
    for (dimension = 0u; dimension < shape->rank; ++dimension) {
        if (!shape->dims[dimension] || count > ULLONG_MAX / shape->dims[dimension])
            return 0;
        count *= shape->dims[dimension];
    }
    *out = count;
    return 1;
}

/* Purpose: project transform scope into the typed DeepSeek lowering scope.
 * Inputs: admitted transform scope.
 * Effects: none.
 * Failure: non-global/non-main values map to the admitted MTP scope.
 * Boundary: typed projection only; no lexical scope parsing occurs. */
static yvex_tensor_scope quant_map_scope(yvex_transform_scope scope) {
    if (scope == YVEX_TRANSFORM_SCOPE_GLOBAL)
        return YVEX_TENSOR_SCOPE_GLOBAL;
    if (scope == YVEX_TRANSFORM_SCOPE_MAIN_LAYER)
        return YVEX_TENSOR_SCOPE_MAIN_LAYER;
    return YVEX_TENSOR_SCOPE_MTP;
}

/* Purpose: project supported transform operations into lowering semantics.
 * Inputs: typed artifact-neutral operation kind.
 * Effects: none.
 * Failure: unsupported kinds map to the explicit invalid sentinel.
 * Boundary: maps semantics without selecting qtypes or executing bytes. */
static yvex_deepseek_gguf_transform quant_map_operation(yvex_transform_operation_kind operation) {
    size_t index;
    for (index = 0u; index < sizeof(operation_lowerings) / sizeof(operation_lowerings[0]); ++index)
        if (operation_lowerings[index].operation == operation)
            return operation_lowerings[index].lowering;
    return (yvex_deepseek_gguf_transform)-1;
}

/* Purpose: select exact scalar GGUF storage for one logical dtype.
 * Inputs: typed transform value dtype.
 * Effects: none.
 * Failure: unsupported dtypes return the unsigned invalid sentinel.
 * Boundary: exact storage selection excludes lossy profile policy. */
static unsigned int quant_exact_qtype(yvex_transform_dtype dtype) {
    return ((dtype >= YVEX_TRANSFORM_DTYPE_F32 && dtype <= YVEX_TRANSFORM_DTYPE_I32) ||
            dtype == YVEX_TRANSFORM_DTYPE_REAL)
               ? exact_qtypes[dtype]
               : UINT_MAX;
}

/* Purpose: select candidate encoding from typed operation and precision facts.
 * Inputs: candidate profile plus immutable terminal and producer node.
 * Effects: none.
 * Failure: unsupported logical dtypes yield the invalid qtype sentinel.
 * Boundary: never parses source or emitted tensor names. */
static unsigned int quant_candidate_qtype(yvex_quant_profile_kind profile,
                                          const yvex_transform_value *terminal,
                                          const yvex_transform_node *node) {
    if (node->kind == YVEX_TRANSFORM_OP_CHECKED_CAST)
        return YVEX_GGUF_QTYPE_I32;
    if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE)
        return profile == YVEX_QUANT_PROFILE_RELEASE_Q8_Q2 ? YVEX_GGUF_QTYPE_Q2_K
                                                           : YVEX_GGUF_QTYPE_MXFP4;
    if (terminal->precision.flags & YVEX_TRANSFORM_PRECISION_QUANTIZABLE_WEIGHT) {
        if (profile == YVEX_QUANT_PROFILE_RELEASE_Q8_Q2)
            return YVEX_GGUF_QTYPE_Q8_0;
        if (node->kind == YVEX_TRANSFORM_OP_DECODE_SCALE_PAIR)
            return YVEX_GGUF_QTYPE_F32;
    }
    return quant_exact_qtype(terminal->dtype);
}

/* Purpose: prove one IR terminal bijects with one lowering descriptor and inputs.
 * Inputs: sealed IR, terminal/node, lowering map/descriptor, and ordinal.
 * Effects: may publish the first typed mismatch failure.
 * Failure: refuses roles, axes, shapes, operations, or contributions that diverge.
 * Boundary: compares canonical facts and performs no source or payload I/O. */
static int
quant_descriptor_matches(const yvex_transform_ir *ir, const yvex_transform_value *terminal,
                         const yvex_transform_node *node, const yvex_deepseek_gguf_map *map,
                         const yvex_deepseek_gguf_descriptor *descriptor,
                         unsigned long long ordinal, yvex_quant_failure *failure, yvex_error *err) {
    unsigned int dimension;
    unsigned long long input;

    if (!descriptor || descriptor->role != terminal->logical_key.role ||
        descriptor->scope != quant_map_scope(terminal->logical_key.scope) ||
        descriptor->layer_index != terminal->logical_key.layer_index ||
        descriptor->predictor_index != terminal->logical_key.auxiliary_index ||
        descriptor->transform != quant_map_operation(node->kind) ||
        descriptor->logical_rank != terminal->shape.rank ||
        descriptor->contribution_count != node->input_count) {
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal, ULLONG_MAX,
                               node->input_count, descriptor ? descriptor->contribution_count : 0u,
                               UINT_MAX, node->kind, err, YVEX_ERR_FORMAT,
                               "terminal and GGUF lowering descriptor do not biject");
    }
    for (dimension = 0u; dimension < terminal->shape.rank; ++dimension) {
        unsigned int terminal_axis = terminal->shape.rank - dimension - 1u;
        unsigned int source_axis = terminal_axis;

        if (node->kind == YVEX_TRANSFORM_OP_EXPERT_AGGREGATE) {
            source_axis = terminal_axis == node->axis  ? YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS
                          : terminal_axis > node->axis ? terminal_axis - 1u
                                                       : terminal_axis;
        }
        if (descriptor->logical_dims[dimension] != terminal->shape.dims[terminal_axis] ||
            descriptor->source_axis_for_logical[dimension] != source_axis) {
            return quant_plan_fail(failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal,
                                   ULLONG_MAX, terminal->shape.dims[terminal_axis],
                                   descriptor->logical_dims[dimension], UINT_MAX, node->kind, err,
                                   YVEX_ERR_FORMAT, "terminal and lowering physical axes diverge");
        }
    }
    for (input = 0u; input < node->input_count; ++input) {
        const yvex_transform_value *value = yvex_transform_ir_node_input_at(ir, node, input);
        const yvex_transform_source_value *source =
            value ? yvex_transform_ir_source_at(ir, value->source_index) : NULL;
        const yvex_deepseek_gguf_contribution *contribution =
            yvex_model_register_deepseek_v4()->lowering.contribution_at(
                map, descriptor->contribution_offset + input);
        if (!value || !source || !contribution || contribution->descriptor_index != ordinal ||
            strcmp(contribution->source_name, source->source_name) != 0 ||
            contribution->source_dtype != source->source_dtype ||
            contribution->expert_index != source->expert_index) {
            return quant_plan_fail(failure, YVEX_QUANT_FAILURE_UNMATCHED_LOWERING, ordinal,
                                   value ? value->source_index : ULLONG_MAX, input,
                                   contribution ? contribution->descriptor_index : ULLONG_MAX,
                                   UINT_MAX, node->kind, err, YVEX_ERR_FORMAT,
                                   "lowering contribution does not match the exact IR input");
        }
    }
    return YVEX_OK;
}

/* Purpose: derive exact qtype storage geometry for a physical decision.
 * Inputs: mutable decision, explicit dimensions/rank, and failure outputs.
 * Effects: fills row, element, shape, and encoded-byte fields on success.
 * Failure: maps canonical geometry refusal into typed quant recovery classes.
 * Boundary: consumes qtype geometry authority and never duplicates its tables. */
static int quant_decision_geometry_dims(yvex_quant_decision *decision,
                                        const unsigned long long *dims, unsigned int rank,
                                        yvex_quant_failure *failure, yvex_error *err) {
    yvex_gguf_qtype_storage_result storage;
    yvex_gguf_qtype_storage_status status;
    yvex_quant_failure_code code;

    status = yvex_gguf_qtype_tensor_storage(decision->qtype, dims, rank, &storage);
    if (status != YVEX_GGUF_QTYPE_STORAGE_OK) {
        code = status <= YVEX_GGUF_QTYPE_STORAGE_EXPECTED_ACTUAL_MISMATCH &&
                       storage_failures[status] != 0
                   ? storage_failures[status]
                   : YVEX_QUANT_FAILURE_BYTE_OVERFLOW;
        return quant_plan_fail(failure, code, decision->terminal_ordinal, ULLONG_MAX, 0u, status,
                               decision->qtype, decision->operation, err, YVEX_ERR_BOUNDS,
                               "selected qtype cannot represent the lowering tensor geometry");
    }
    decision->rank = rank;
    memcpy(decision->dims, dims, sizeof(decision->dims));
    decision->row_axis = 0u;
    decision->row_width = storage.row_width;
    decision->row_count = storage.row_count;
    decision->element_count = storage.element_count;
    decision->encoded_bytes = storage.total_bytes;
    return YVEX_OK;
}

/* Purpose: map numeric-registry refusal into the plan recovery taxonomy.
 * Inputs: optional immutable qtype capability row.
 * Effects: none.
 * Failure: absent/unknown capability maps to unknown-qtype refusal.
 * Boundary: projects canonical capability truth without redefining support. */
static yvex_quant_failure_code
quant_capability_failure(const yvex_quant_numeric_capability *capability) {
    if (!capability || !capability->identity_known)
        return YVEX_QUANT_FAILURE_UNKNOWN_QTYPE;
    if (capability->refusal == YVEX_QUANT_REFUSAL_REMOVED_IDENTITY)
        return YVEX_QUANT_FAILURE_REMOVED_QTYPE;
    if (capability->refusal == YVEX_QUANT_REFUSAL_OUTSIDE_PINNED_BASELINE)
        return YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE;
    if (capability->encoder_available && !capability->reference_decoder_available)
        return YVEX_QUANT_FAILURE_DECODER_UNAVAILABLE;
    return YVEX_QUANT_FAILURE_ENCODER_UNAVAILABLE;
}

/* Purpose: derive decision geometry from one canonical lowering descriptor.
 * Inputs: mutable decision, descriptor physical shape, and failure outputs.
 * Effects: fills exact qtype row and byte accounting.
 * Failure: propagates canonical geometry refusal.
 * Boundary: thin typed adapter; dimension logic remains in the shared helper. */
static int quant_decision_geometry(yvex_quant_decision *decision,
                                   const yvex_deepseek_gguf_descriptor *descriptor,
                                   yvex_quant_failure *failure, yvex_error *err) {
    return quant_decision_geometry_dims(decision, descriptor->logical_dims,
                                        descriptor->logical_rank, failure, err);
}

/* Purpose: derive deterministic identity for one complete physical decision.
 * Inputs: populated decision with logical key, operation, shape, and constraints.
 * Effects: writes SHA-256 identity into the owned decision.
 * Failure: returns false when canonical encoding or hash finalization fails.
 * Boundary: excludes pointers, names, allocation order, and runtime counters. */
static int quant_decision_identity(yvex_quant_decision *decision) {
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned int dimension;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.quant.decision.v1") ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.scope) ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.subsystem) ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.role) ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.layer_index) ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.auxiliary_index) ||
        !yvex_sha256_update_u64(&hash, decision->logical_key.group_index) ||
        !yvex_sha256_update_u64(&hash, decision->terminal_value_id) ||
        !yvex_sha256_update_u64(&hash, decision->operation) || !yvex_sha256_update_u64(&hash, decision->qtype) ||
        !yvex_sha256_update_u64(&hash, decision->rank))
        return 0;
    for (dimension = 0u; dimension < decision->rank; ++dimension)
        if (!yvex_sha256_update_u64(&hash, decision->dims[dimension]))
            return 0;
    if (!yvex_sha256_update_u64(&hash, decision->row_axis) ||
        !yvex_sha256_update_u64(&hash, decision->element_count) ||
        !yvex_sha256_update_u64(&hash, decision->encoded_bytes) ||
        !yvex_sha256_update_u64(&hash, decision->approximation) ||
        !yvex_sha256_update_u64(&hash, decision->calibration) ||
        !yvex_sha256_update_u64(&hash, decision->numeric_contract_version) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, decision->decision_identity);
    return 1;
}

/* Purpose: account one decision into its candidate profile summary.
 * Inputs: mutable candidate counters and immutable sealed decision.
 * Effects: advances bytes, qtype counts, compute, and calibration facts.
 * Failure: returns false for invalid qtype or any counter overflow.
 * Boundary: accounting does not select the final candidate. */
static int quant_summary_add(yvex_quant_candidate_summary *summary,
                             const yvex_quant_decision *decision) {
    unsigned long long *class_bytes;

    if (!summary || !decision || decision->qtype > YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID ||
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
    if (ULLONG_MAX - *class_bytes < decision->encoded_bytes)
        return 0;
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

/* Purpose: build one candidate decision from typed IR and numeric capability.
 * Inputs: profile, binding, terminal/node, lowering descriptor, and ordinal.
 * Effects: fills decision geometry, constraints, compute facts, and identity.
 * Failure: typed refusal covers precision, codec, binding, geometry, or identity.
 * Boundary: plans physical bytes but performs no conversion or payload read. */
static int quant_build_candidate_decision(yvex_quant_profile_kind profile,
                                          const yvex_transform_binding *binding,
                                          const yvex_transform_value *terminal,
                                          const yvex_transform_node *node,
                                          const yvex_deepseek_gguf_descriptor *descriptor,
                                          unsigned long long ordinal, yvex_quant_decision *decision,
                                          yvex_quant_failure *failure, yvex_error *err) {
    const yvex_quant_numeric_capability *capability;
    yvex_transform_physical_decision binding_decision;
    yvex_transform_failure transform_failure;
    int rc;
    unsigned int qtype = quant_candidate_qtype(profile, terminal, node);

    if (qtype == UINT_MAX)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
                               ULLONG_MAX, 1u, 0u, qtype, node->kind, err, YVEX_ERR_UNSUPPORTED,
                               "terminal dtype has no admitted physical scalar representation");
    capability = yvex_quant_numeric_capability_at(qtype);
    if (!capability || !capability->encoder_available || !capability->reference_decoder_available) {
        return quant_plan_fail(failure, quant_capability_failure(capability), ordinal, ULLONG_MAX,
                               1u, 0u, qtype, node->kind, err, YVEX_ERR_UNSUPPORTED,
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
    decision->physical_class = qtype == YVEX_GGUF_QTYPE_F32 || qtype == YVEX_GGUF_QTYPE_F16 ||
                                       qtype == YVEX_GGUF_QTYPE_BF16 || qtype == YVEX_GGUF_QTYPE_I32
                                   ? YVEX_QUANT_PHYSICAL_EXACT_SCALAR
                                   : YVEX_QUANT_PHYSICAL_BLOCK_QUANTIZED;
    decision->approximation = qtype == YVEX_GGUF_QTYPE_Q8_0 || qtype == YVEX_GGUF_QTYPE_Q2_K ||
                              qtype == YVEX_GGUF_QTYPE_MXFP4;
    if (decision->approximation && !terminal->precision.approximation_allowed)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_APPROXIMATION_FORBIDDEN, ordinal,
                               ULLONG_MAX, 0u, 1u, qtype, node->kind, err, YVEX_ERR_FORMAT,
                               "profile selected approximation for an exact terminal");
    memset(&binding_decision, 0, sizeof(binding_decision));
    binding_decision.physical_class = capability->physical_class_mask;
    binding_decision.encoding_id = qtype;
    binding_decision.approximation_selected = decision->approximation;
    rc = yvex_transform_binding_decision_validate(binding, ordinal, &binding_decision,
                                                  &transform_failure, err);
    if (rc != YVEX_OK)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
                               ULLONG_MAX, terminal->precision.allowed_physical_classes,
                               capability->physical_class_mask, qtype, node->kind, err,
                               YVEX_ERR_FORMAT,
                               "profile decision violates the terminal precision constraint");
    decision->calibration = capability->calibration;
    decision->reference_decoder_required = 1;
    decision->cpu_compute_available = capability->dedicated_cpu_compute_available;
    decision->cuda_compute_available = capability->dedicated_cuda_compute_available;
    decision->numeric_contract_version = capability->numeric_contract_version;
    rc = quant_decision_geometry(decision, descriptor, failure, err);
    if (rc != YVEX_OK)
        return rc;
    if (!quant_decision_identity(decision))
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal, ULLONG_MAX, 1u,
                               0u, qtype, node->kind, err, YVEX_ERR_BOUNDS,
                               "decision identity encoding failed");
    return YVEX_OK;
}

/* Purpose: derive canonical identity for one complete physical quant plan.
 * Inputs: populated plan summary and canonical-ordinal decision identities.
 * Effects: writes the profile identity into the owned summary.
 * Failure: returns false when any explicit-width hash update fails.
 * Boundary: binds semantics and execution requirements, not temporary resources. */
static int quant_plan_identity(yvex_quant_plan *plan) {
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long ordinal;

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.quant.plan.v1") ||
        !yvex_sha256_update_u64(&hash, plan->summary.schema_version) ||
        !yvex_sha256_update_text(&hash, plan->summary.profile_name) ||
        !yvex_sha256_update_text(&hash, plan->summary.transform_identity) ||
        !yvex_sha256_update_u64(&hash, plan->summary.source_snapshot_identity) ||
        !yvex_sha256_update_text(&hash, plan->summary.required_payload_identity) ||
        !yvex_sha256_update_u64(&hash, plan->summary.mapping_identity) ||
        !yvex_sha256_update_text(&hash, plan->summary.calibration_identity) ||
        !yvex_sha256_update_text(&hash, plan->summary.backend_compute_contract) ||
        !yvex_sha256_update_u64(&hash, plan->summary.decision_count))
        return 0;
    for (ordinal = 0u; ordinal < plan->summary.decision_count; ++ordinal)
        if (!yvex_sha256_update_text(&hash, plan->decisions[ordinal].decision_identity))
            return 0;
    if (!yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, plan->summary.profile_identity);
    return 1;
}

typedef struct {
    const yvex_transform_ir *ir;
    const yvex_transform_binding *binding;
    const yvex_deepseek_gguf_map *map;
    const yvex_transform_ir_summary *ir_summary;
    const yvex_transform_binding_summary *binding_summary;
    yvex_quant_plan_options options;
    yvex_quant_plan *plan;
    yvex_quant_failure *failure;
    yvex_error *err;
    const char *profile_name;
    unsigned long long mapping_identity;
    int explicit_plan;
} quant_build_context;

/* Purpose: prove the immutable binding names exactly the supplied IR snapshot.
 * Inputs: build context containing admitted IR and binding summaries.
 * Effects: may publish one typed identity mismatch.
 * Failure: refuses transform, source snapshot, or payload identity divergence.
 * Boundary: validates identity binding before allocation and payload execution. */
static int quant_binding_identity_validate(quant_build_context *context) {
    if (strcmp(context->ir_summary->transform_identity,
               context->binding_summary->transform_identity) != 0)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_TRANSFORM_IDENTITY, ULLONG_MAX,
                               ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, context->err,
                               YVEX_ERR_FORMAT, "binding and IR identities diverge");
    if (context->ir_summary->source_snapshot_identity !=
        context->binding_summary->source_snapshot_identity)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_SOURCE_IDENTITY, ULLONG_MAX,
                               ULLONG_MAX, context->ir_summary->source_snapshot_identity,
                               context->binding_summary->source_snapshot_identity, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_FORMAT,
                               "binding and IR source identities diverge");
    if (strcmp(context->ir_summary->required_payload_identity,
               context->binding_summary->required_payload_identity) != 0)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_PAYLOAD_IDENTITY, ULLONG_MAX,
                               ULLONG_MAX, 1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, context->err,
                               YVEX_ERR_FORMAT, "binding and IR payload identities diverge");
    return YVEX_OK;
}

/* Purpose: apply allocator defaults and create bounded plan arrays/indexes.
 * Inputs: admitted build context and optional resource/allocator options.
 * Effects: allocates initialized plan, decisions, and open-addressing index.
 * Failure: typed refusal covers geometry, budget, or allocation failure.
 * Boundary: borrows IR/binding/lowering and owns only plan storage. */
static int quant_build_allocate(quant_build_context *context,
                                const yvex_quant_plan_options *options) {
    size_t decision_bytes;
    size_t index_bytes;
    size_t owned_bytes = SIZE_MAX;
    unsigned long long count = context->ir_summary->terminal_count;
    memset(&context->options, 0, sizeof(context->options));
    context->options.allocate = quant_plan_default_allocate;
    context->options.release = quant_plan_default_release;
    context->options.maximum_owned_bytes = 16u * 1024u * 1024u;
    if (options)
        context->options = *options;
    if (!context->options.allocate)
        context->options.allocate = quant_plan_default_allocate;
    if (!context->options.release)
        context->options.release = quant_plan_default_release;
    if (!context->options.maximum_owned_bytes)
        context->options.maximum_owned_bytes = 16u * 1024u * 1024u;
    context->plan = (yvex_quant_plan *)context->options.allocate(sizeof(*context->plan),
                                                                 context->options.context);
    if (!context->plan)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX,
                               ULLONG_MAX, sizeof(*context->plan), 0u, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_NOMEM,
                               context->explicit_plan
                                   ? "explicit quantization plan allocation failed"
                                   : "quantization plan allocation failed");
    memset(context->plan, 0, sizeof(*context->plan));
    context->plan->allocate = context->options.allocate;
    context->plan->release = context->options.release;
    context->plan->allocator_context = context->options.context;
    context->plan->ir = context->ir;
    context->plan->binding = context->binding;
    context->plan->map = context->map;
    context->plan->summary.schema_version = YVEX_QUANT_PROFILE_SCHEMA_VERSION;
    context->plan->summary.state = YVEX_QUANT_PLAN_BUILDING;
    context->plan->summary.terminal_count = count;
    context->plan->summary.source_value_count = context->ir_summary->source_value_count;
    context->plan->summary.source_snapshot_identity = context->ir_summary->source_snapshot_identity;
    context->plan->summary.mapping_identity = context->mapping_identity;
    (void)snprintf(context->plan->summary.profile_name, sizeof(context->plan->summary.profile_name),
                   "%s", context->profile_name);
    (void)snprintf(context->plan->summary.transform_identity,
                   sizeof(context->plan->summary.transform_identity), "%s",
                   context->ir_summary->transform_identity);
    (void)snprintf(context->plan->summary.required_payload_identity,
                   sizeof(context->plan->summary.required_payload_identity), "%s",
                   context->ir_summary->required_payload_identity);
    (void)snprintf(context->plan->summary.backend_compute_contract,
                   sizeof(context->plan->summary.backend_compute_contract), "%s",
                   "cpu-cuda-encoded-row-dot-v1");
    (void)snprintf(context->plan->summary.calibration_identity,
                   sizeof(context->plan->summary.calibration_identity), "%s",
                   "no-calibration-required");
    context->plan->summary.index_capacity = quant_index_capacity(count);
    if (!context->plan->summary.index_capacity ||
        count > SIZE_MAX / sizeof(*context->plan->decisions) ||
        context->plan->summary.index_capacity > SIZE_MAX / sizeof(*context->plan->index))
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ULLONG_MAX, ULLONG_MAX, SIZE_MAX,
            count, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_BOUNDS,
            context->explicit_plan ? "explicit plan allocation geometry overflowed"
                                   : "quantization plan allocation geometry overflowed");
    decision_bytes = (size_t)count * sizeof(*context->plan->decisions);
    index_bytes = (size_t)context->plan->summary.index_capacity * sizeof(*context->plan->index);
    if (!quant_size_add(sizeof(*context->plan), decision_bytes, &owned_bytes) ||
        !quant_size_add(owned_bytes, index_bytes, &owned_bytes) ||
        owned_bytes > context->options.maximum_owned_bytes)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ULLONG_MAX,
                               ULLONG_MAX, context->options.maximum_owned_bytes, owned_bytes,
                               UINT_MAX, YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_BOUNDS,
                               context->explicit_plan
                                   ? "explicit plan ownership budget exceeded"
                                   : "quantization plan ownership budget exceeded");
    context->plan->decisions = (yvex_quant_decision *)context->plan->allocate(
        decision_bytes, context->plan->allocator_context);
    context->plan->index =
        (quant_plan_slot *)context->plan->allocate(index_bytes, context->plan->allocator_context);
    if (!context->plan->decisions || !context->plan->index)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_ALLOCATION, ULLONG_MAX,
                               ULLONG_MAX, owned_bytes - sizeof(*context->plan), 0u, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_NOMEM,
                               context->explicit_plan
                                   ? "explicit decision/index allocation failed"
                                   : "quantization decision/index allocation failed");
    memset(context->plan->decisions, 0, decision_bytes);
    memset(context->plan->index, 0, index_bytes);
    context->plan->summary.owned_bytes = owned_bytes;
    context->plan->summary.peak_builder_bytes = owned_bytes;
    return YVEX_OK;
}

/* Purpose: insert one immutable logical decision into expected-O(1) index.
 * Inputs: build context, completed decision, ordinal, and diagnostics.
 * Effects: installs one slot and advances decision/qtype/role counters.
 * Failure: refuses duplicate logical keys or exhausted probe capacity.
 * Boundary: indexing order cannot change canonical terminal ordering. */
static int quant_index_add(quant_build_context *context, yvex_quant_decision *decision,
                           unsigned long long ordinal, const char *duplicate_message,
                           const char *exhausted_message) {
    unsigned long long hash = quant_key_hash(&decision->logical_key);
    unsigned long long slot = hash & (context->plan->summary.index_capacity - 1u);
    unsigned long long probe;

    for (probe = 0u; probe < context->plan->summary.index_capacity; ++probe) {
        if (!context->plan->index[slot].ordinal_plus_one)
            break;
        if (context->plan->index[slot].hash == hash &&
            quant_key_equal(
                &context->plan->decisions[context->plan->index[slot].ordinal_plus_one - 1u]
                     .logical_key,
                &decision->logical_key))
            return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_DUPLICATE_DECISION, ordinal,
                                   ULLONG_MAX, 1u, 2u, decision->qtype, decision->operation,
                                   context->err, YVEX_ERR_FORMAT, duplicate_message);
        slot = (slot + 1u) & (context->plan->summary.index_capacity - 1u);
    }
    if (probe == context->plan->summary.index_capacity)
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_RESOURCE_BUDGET, ordinal, ULLONG_MAX,
            context->plan->summary.index_capacity, context->plan->summary.index_capacity,
            decision->qtype, decision->operation, context->err, YVEX_ERR_BOUNDS, exhausted_message);
    context->plan->index[slot].hash = hash;
    context->plan->index[slot].ordinal_plus_one = ordinal + 1u;
    context->plan->summary.decision_count++;
    context->plan->summary.qtype_tensor_counts[decision->qtype]++;
    if ((unsigned int)decision->role < YVEX_TENSOR_ROLE_COUNT)
        context->plan->summary.role_tensor_counts[decision->role]++;
    return YVEX_OK;
}

/* Purpose: validate and materialize one caller-selected physical decision.
 * Inputs: build context, explicit decision specification, ordinal, and summary.
 * Effects: fills one decision, accounts candidate bytes, and indexes its key.
 * Failure: typed refusal covers binding, codec, shape, precision, or identity.
 * Boundary: accepts physical choices without mutating Transformation IR. */
static int quant_explicit_decision_build(quant_build_context *context,
                                         const yvex_quant_explicit_decision *spec,
                                         unsigned long long ordinal,
                                         yvex_quant_candidate_summary *candidate) {
    const yvex_transform_value *terminal = yvex_transform_ir_terminal_at(context->ir, ordinal);
    const yvex_transform_node *node =
        terminal ? yvex_transform_ir_node_at(context->ir, terminal->producer_node_id) : NULL;
    const yvex_quant_numeric_capability *capability = yvex_quant_numeric_capability_at(spec->qtype);
    yvex_quant_decision *decision = &context->plan->decisions[ordinal];
    yvex_transform_physical_decision binding_decision;
    yvex_transform_failure transform_failure;
    unsigned long long logical_elements;
    int logical_geometry_ok;
    int rc;

    if (!terminal || !node || terminal->canonical_ordinal != ordinal ||
        yvex_transform_binding_terminal_at(context->binding, ordinal) != terminal ||
        yvex_transform_binding_terminal_operation(context->binding, ordinal) != node)
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ordinal, ULLONG_MAX, ordinal,
            terminal ? terminal->canonical_ordinal : ULLONG_MAX, spec->qtype,
            node ? node->kind : YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_FORMAT,
            "binding does not expose the canonical terminal operation");
    if (!capability || !capability->identity_known)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_UNKNOWN_QTYPE, ordinal,
                               ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, context->err,
                               YVEX_ERR_UNSUPPORTED, "explicit qtype identity is unknown");
    if (!capability->encoder_available || !capability->reference_decoder_available)
        return quant_plan_fail(context->failure, quant_capability_failure(capability), ordinal,
                               ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, context->err,
                               YVEX_ERR_UNSUPPORTED, "explicit plan selected an unavailable codec");
    if (node->kind >= YVEX_TRANSFORM_OP_COUNT ||
        !(capability->transform_kind_mask & (1u << node->kind)))
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_UNSUPPORTED_OPERATION, ordinal,
                               ULLONG_MAX, capability->transform_kind_mask, node->kind, spec->qtype,
                               node->kind, context->err, YVEX_ERR_UNSUPPORTED,
                               "explicit qtype does not admit the terminal operation");
    if (spec->row_axis != 0u)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_INVALID_ROW_AXIS, ordinal,
                               ULLONG_MAX, 0u, spec->row_axis, spec->qtype, node->kind,
                               context->err, YVEX_ERR_UNSUPPORTED,
                               "current physical geometry requires qtype rows on axis zero");
    memset(decision, 0, sizeof(*decision));
    decision->logical_key = terminal->logical_key;
    decision->terminal_ordinal = ordinal;
    decision->terminal_value_id = terminal->id;
    decision->node_id = node->id;
    decision->role = terminal->logical_key.role;
    decision->scope = terminal->logical_key.scope;
    decision->operation = node->kind;
    decision->qtype = spec->qtype;
    decision->physical_class = capability->physical_class_mask == YVEX_TRANSFORM_PHYSICAL_QUANTIZED
                                   ? YVEX_QUANT_PHYSICAL_BLOCK_QUANTIZED
                                   : YVEX_QUANT_PHYSICAL_EXACT_SCALAR;
    decision->approximation = spec->approximation;
    if (decision->approximation && !terminal->precision.approximation_allowed)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_APPROXIMATION_FORBIDDEN,
                               ordinal, ULLONG_MAX, 0u, 1u, spec->qtype, node->kind, context->err,
                               YVEX_ERR_FORMAT,
                               "explicit plan selected approximation for an exact terminal");
    memset(&binding_decision, 0, sizeof(binding_decision));
    binding_decision.physical_class = capability->physical_class_mask;
    binding_decision.encoding_id = spec->qtype;
    binding_decision.approximation_selected = spec->approximation;
    rc = yvex_transform_binding_decision_validate(context->binding, ordinal, &binding_decision,
                                                  &transform_failure, context->err);
    if (rc != YVEX_OK)
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_PRECISION_CONSTRAINT, ordinal,
                               ULLONG_MAX, terminal->precision.allowed_physical_classes,
                               capability->physical_class_mask, spec->qtype, node->kind,
                               context->err, YVEX_ERR_FORMAT,
                               "explicit decision violates the terminal precision constraint");
    decision->calibration = capability->calibration;
    decision->reference_decoder_required = 1;
    decision->cpu_compute_available = capability->dedicated_cpu_compute_available;
    decision->cuda_compute_available = capability->dedicated_cuda_compute_available;
    decision->numeric_contract_version = capability->numeric_contract_version;
    rc = quant_decision_geometry_dims(decision, spec->dims, spec->rank, context->failure,
                                      context->err);
    logical_geometry_ok = quant_transform_element_count(&terminal->shape, &logical_elements);
    if (rc == YVEX_OK && (!logical_geometry_ok || logical_elements != decision->element_count))
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_INVALID_DIMENSION, ordinal,
                               ULLONG_MAX, logical_geometry_ok ? logical_elements : ULLONG_MAX,
                               decision->element_count, spec->qtype, node->kind, context->err,
                               YVEX_ERR_BOUNDS,
                               "physical decision element count differs from its logical terminal");
    if (rc != YVEX_OK)
        return rc;
    if (!quant_decision_identity(decision))
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                               ULLONG_MAX, 1u, 0u, spec->qtype, node->kind, context->err,
                               YVEX_ERR_BOUNDS, "explicit decision identity failed");
    if (!quant_summary_add(candidate, decision))
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal,
                               ULLONG_MAX, ULLONG_MAX, candidate->encoded_bytes, spec->qtype,
                               node->kind, context->err, YVEX_ERR_BOUNDS,
                               "explicit plan byte accounting overflowed");
    return quant_index_add(context, decision, ordinal,
                           "duplicate logical terminal decision refused",
                           "explicit decision index exhausted");
}

/* Purpose: copy selected candidate accounting into the canonical plan summary.
 * Inputs: mutable plan summary and immutable candidate totals.
 * Effects: updates selected bytes and calibration fields.
 * Failure: admitted summaries make this field projection infallible.
 * Boundary: copies accounting only and does not select the candidate. */
static void quant_summary_select(yvex_quant_plan_summary *summary,
                                 const yvex_quant_candidate_summary *candidate) {
    summary->encoded_bytes = candidate->encoded_bytes;
    summary->exact_scalar_bytes = candidate->exact_scalar_bytes;
    summary->q8_0_bytes = candidate->q8_0_bytes;
    summary->q2_k_bytes = candidate->q2_k_bytes;
    summary->mxfp4_bytes = candidate->mxfp4_bytes;
    summary->calibration_required = candidate->calibration_required;
}

/* Purpose: build and seal a caller-described physical plan over a complete binding.
 * Inputs: IR/binding identities, lowering identity, exact decisions, and budget.
 * Effects: returns an independently owned, indexed, immutable quant plan.
 * Failure: releases partial ownership and publishes a typed plan refusal.
 * Boundary: performs no I/O and cannot replace or mutate transform semantics. */
int yvex_quant_plan_build_explicit(yvex_quant_plan **out, const yvex_transform_ir *ir,
                                   const yvex_transform_binding *binding, const char *profile_name,
                                   unsigned long long lowering_identity,
                                   const yvex_quant_explicit_decision *decisions,
                                   unsigned long long decision_count,
                                   const yvex_quant_plan_options *options,
                                   yvex_quant_failure *failure, yvex_error *err) {
    quant_build_context context;
    yvex_quant_candidate_summary candidate;
    unsigned long long ordinal;
    int rc;

    memset(&context, 0, sizeof(context));
    context.ir = ir;
    context.binding = binding;
    context.ir_summary = yvex_transform_ir_summary_get(ir);
    context.binding_summary = yvex_transform_binding_summary_get(binding);
    context.profile_name = profile_name;
    context.mapping_identity = lowering_identity;
    context.explicit_plan = 1;
    context.failure = failure;
    context.err = err;
    if (out)
        *out = NULL;
    if (!out || !ir || !binding || !profile_name || !profile_name[0] ||
        strlen(profile_name) >= sizeof(((yvex_quant_plan_summary *)0)->profile_name) ||
        !lowering_identity || !decisions || !context.ir_summary || !context.binding_summary ||
        yvex_transform_binding_ir(binding) != ir || !context.ir_summary->complete ||
        !context.binding_summary->complete) {
        return quant_plan_fail(
            failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, ULLONG_MAX, ULLONG_MAX, 1u, 0u, UINT_MAX,
            YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_INVALID_ARG,
            "complete IR/binding, profile, lowering identity, and decisions are required");
    }
    if (decision_count != context.ir_summary->terminal_count) {
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ULLONG_MAX, ULLONG_MAX,
                               context.ir_summary->terminal_count, decision_count, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
                               "explicit plan must decide every canonical terminal exactly once");
    }
    rc = quant_binding_identity_validate(&context);
    if (rc != YVEX_OK)
        return rc;

    rc = quant_build_allocate(&context, options);
    if (rc != YVEX_OK) {
        yvex_quant_plan_release(&context.plan);
        return rc;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.kind = YVEX_QUANT_PROFILE_RELEASE_Q8_Q2;
    candidate.name = context.plan->summary.profile_name;
    candidate.compute_admissible = 1;

    for (ordinal = 0u; ordinal < decision_count; ++ordinal) {
        rc = quant_explicit_decision_build(&context, &decisions[ordinal], ordinal, &candidate);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&context.plan);
            return rc;
        }
    }
    candidate.numerically_admissible = 1;
    context.plan->summary.candidates[0] = candidate;
    quant_summary_select(&context.plan->summary, &candidate);
    if (context.plan->summary.decision_count != decision_count ||
        context.plan->summary.calibration_required || !candidate.compute_admissible ||
        !quant_plan_identity(context.plan)) {
        yvex_quant_plan_release(&context.plan);
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_INCOMPLETE, ULLONG_MAX, ULLONG_MAX,
                               decision_count, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err,
                               YVEX_ERR_FORMAT, "explicit quantization plan did not seal");
    }
    context.plan->summary.state = YVEX_QUANT_PLAN_SEALED;
    context.plan->summary.complete = 1;
    *out = context.plan;
    if (failure)
        memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: account both DeepSeek candidates and index one selected terminal.
 * Inputs: target-scale context, selected profile, and canonical ordinal.
 * Effects: builds reference/release decisions and stores the selected one.
 * Failure: propagates bijection, codec, geometry, accounting, or index refusal.
 * Boundary: candidate comparison remains deterministic and payload-free. */
static int quant_deepseek_decision_build(quant_build_context *context,
                                         yvex_quant_profile_kind profile,
                                         unsigned long long ordinal) {
    const yvex_transform_value *terminal = yvex_transform_ir_terminal_at(context->ir, ordinal);
    const yvex_transform_node *node =
        terminal ? yvex_transform_ir_node_at(context->ir, terminal->producer_node_id) : NULL;
    const yvex_deepseek_gguf_descriptor *descriptor =
        yvex_model_register_deepseek_v4()->lowering.at(context->map, ordinal);
    yvex_quant_decision reference_decision;
    yvex_quant_decision release_decision;
    yvex_quant_decision *decision = &context->plan->decisions[ordinal];
    int rc;

    if (!terminal || !node || terminal->canonical_ordinal != ordinal ||
        yvex_transform_binding_terminal_at(context->binding, ordinal) != terminal ||
        yvex_transform_binding_terminal_operation(context->binding, ordinal) != node)
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ordinal, ULLONG_MAX, ordinal,
            terminal ? terminal->canonical_ordinal : ULLONG_MAX, UINT_MAX,
            node ? node->kind : YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_FORMAT,
            "binding does not expose the canonical terminal operation");
    rc = quant_descriptor_matches(context->ir, terminal, node, context->map, descriptor, ordinal,
                                  context->failure, context->err);
    if (rc != YVEX_OK)
        return rc;
    rc = quant_build_candidate_decision(YVEX_QUANT_PROFILE_SOURCE_FAITHFUL, context->binding,
                                        terminal, node, descriptor, ordinal, &reference_decision,
                                        context->failure, context->err);
    if (rc != YVEX_OK)
        return rc;
    if (!quant_summary_add(&context->plan->summary.candidates[0], &reference_decision))
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal, ULLONG_MAX, ULLONG_MAX,
            context->plan->summary.candidates[0].encoded_bytes, UINT_MAX, node->kind, context->err,
            YVEX_ERR_BOUNDS, "reference candidate byte accounting overflowed");
    rc = quant_build_candidate_decision(YVEX_QUANT_PROFILE_RELEASE_Q8_Q2, context->binding,
                                        terminal, node, descriptor, ordinal, &release_decision,
                                        context->failure, context->err);
    if (rc != YVEX_OK)
        return rc;
    if (!quant_summary_add(&context->plan->summary.candidates[1], &release_decision))
        return quant_plan_fail(
            context->failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, ordinal, ULLONG_MAX, ULLONG_MAX,
            context->plan->summary.candidates[1].encoded_bytes, UINT_MAX, node->kind, context->err,
            YVEX_ERR_BOUNDS, "release candidate byte accounting overflowed");
    *decision =
        profile == YVEX_QUANT_PROFILE_SOURCE_FAITHFUL ? reference_decision : release_decision;
    return quant_index_add(context, decision, ordinal,
                           "duplicate logical terminal decision refused",
                           "quantization decision index is exhausted");
}

/* Purpose: seal selected target-scale accounting and canonical profile identity.
 * Inputs: fully populated DeepSeek context and selected profile kind.
 * Effects: marks candidate evidence, copies totals, hashes identity, and seals state.
 * Failure: refuses incomplete decisions, calibration, compute, or identity failure.
 * Boundary: sealing freezes the plan and records zero payload bytes read. */
static int quant_deepseek_plan_seal(quant_build_context *context, yvex_quant_profile_kind profile) {
    yvex_quant_candidate_summary *selected = &context->plan->summary.candidates[profile];

    context->plan->summary.candidates[0].numerically_admissible = 1;
    context->plan->summary.candidates[1].numerically_admissible = 1;
    quant_summary_select(&context->plan->summary, selected);
    if (context->plan->summary.decision_count != context->ir_summary->terminal_count ||
        context->plan->summary.calibration_required || !selected->compute_admissible ||
        !quant_plan_identity(context->plan))
        return quant_plan_fail(context->failure, YVEX_QUANT_FAILURE_INCOMPLETE, ULLONG_MAX,
                               ULLONG_MAX, context->ir_summary->terminal_count, 0u, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, context->err, YVEX_ERR_FORMAT,
                               "quantization profile did not seal completely");
    context->plan->summary.state = YVEX_QUANT_PLAN_SEALED;
    context->plan->summary.complete = 1;
    context->plan->summary.payload_bytes_read = 0u;
    return YVEX_OK;
}

/* Purpose: build both DeepSeek candidates and seal the caller-selected profile.
 * Inputs: complete IR/binding/lowering, admitted profile, and resource options.
 * Effects: returns independently owned canonical decisions and candidate totals.
 * Failure: releases all construction state and returns typed mismatch/refusal.
 * Boundary: performs zero source I/O and never mutates payload or upstream plans. */
int yvex_quant_plan_build_deepseek_profile(yvex_quant_plan **out, const yvex_transform_ir *ir,
                                           const yvex_transform_binding *binding,
                                           const yvex_deepseek_gguf_map *map,
                                           yvex_quant_profile_kind profile,
                                           const yvex_quant_plan_options *options,
                                           yvex_quant_failure *failure, yvex_error *err) {
    quant_build_context context;
    const yvex_deepseek_gguf_map_summary *map_summary =
        yvex_model_register_deepseek_v4()->lowering.summary(map);
    unsigned long long ordinal;
    int rc;

    memset(&context, 0, sizeof(context));
    context.ir = ir;
    context.binding = binding;
    context.map = map;
    context.ir_summary = yvex_transform_ir_summary_get(ir);
    context.binding_summary = yvex_transform_binding_summary_get(binding);
    context.profile_name = profile == YVEX_QUANT_PROFILE_SOURCE_FAITHFUL
                               ? YVEX_QUANT_REFERENCE_PROFILE_NAME
                               : YVEX_QUANT_RELEASE_PROFILE_NAME;
    context.mapping_identity = map_summary ? map_summary->mapping_identity : 0u;
    context.failure = failure;
    context.err = err;
    if (out)
        *out = NULL;
    if (!out || !ir || !binding || !map ||
        (profile != YVEX_QUANT_PROFILE_SOURCE_FAITHFUL &&
         profile != YVEX_QUANT_PROFILE_RELEASE_Q8_Q2) ||
        yvex_transform_binding_ir(binding) != ir || !context.ir_summary ||
        !context.binding_summary || !map_summary || !context.ir_summary->complete ||
        !context.binding_summary->complete || !map_summary->complete)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, ULLONG_MAX, ULLONG_MAX,
                               1u, 0u, UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_INVALID_ARG,
                               "sealed IR, complete binding, lowering, and output are required");
    rc = quant_binding_identity_validate(&context);
    if (rc != YVEX_OK)
        return rc;
    if (map_summary->mapping_identity != YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_MAPPING_IDENTITY, ULLONG_MAX, ULLONG_MAX,
                               YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY, map_summary->mapping_identity,
                               UINT_MAX, YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
                               "GGUF mapping identity is not the pinned lowering");
    if (context.ir_summary->terminal_count != YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT ||
        context.ir_summary->source_value_count != YVEX_DEEPSEEK_GGUF_SOURCE_COUNT ||
        map_summary->descriptor_count != context.ir_summary->terminal_count)
        return quant_plan_fail(failure, YVEX_QUANT_FAILURE_MISSING_DECISION, ULLONG_MAX, ULLONG_MAX,
                               YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT,
                               context.ir_summary->terminal_count, UINT_MAX,
                               YVEX_TRANSFORM_OP_COUNT, err, YVEX_ERR_FORMAT,
                               "target-scale terminal or source accounting is incomplete");
    rc = quant_build_allocate(&context, options);
    if (rc != YVEX_OK) {
        yvex_quant_plan_release(&context.plan);
        return rc;
    }
    context.plan->summary.candidates[0].kind = YVEX_QUANT_PROFILE_SOURCE_FAITHFUL;
    context.plan->summary.candidates[0].name = YVEX_QUANT_REFERENCE_PROFILE_NAME;
    context.plan->summary.candidates[0].compute_admissible = 1;
    context.plan->summary.candidates[1].kind = YVEX_QUANT_PROFILE_RELEASE_Q8_Q2;
    context.plan->summary.candidates[1].name = YVEX_QUANT_RELEASE_PROFILE_NAME;
    context.plan->summary.candidates[1].compute_admissible = 1;
    for (ordinal = 0u; ordinal < context.ir_summary->terminal_count; ++ordinal) {
        rc = quant_deepseek_decision_build(&context, profile, ordinal);
        if (rc != YVEX_OK) {
            yvex_quant_plan_release(&context.plan);
            return rc;
        }
    }
    rc = quant_deepseek_plan_seal(&context, profile);
    if (rc != YVEX_OK) {
        yvex_quant_plan_release(&context.plan);
        return rc;
    }
    *out = context.plan;
    if (failure)
        memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}
/* Purpose: release all independently owned quant-plan storage idempotently.
 * Inputs: address of an optional quant-plan handle.
 * Effects: nulls the handle and releases index, decisions, and plan via its allocator.
 * Failure: absent handles are accepted; cleanup exposes no recoverable error.
 * Boundary: borrowed IR, binding, and lowering owners remain untouched. */
void yvex_quant_plan_release(yvex_quant_plan **plan_address) {
    yvex_quant_plan *plan;
    yvex_quant_release_fn release;
    void *context;

    if (!plan_address || !*plan_address)
        return;
    plan = *plan_address;
    *plan_address = NULL;
    release = plan->release ? plan->release : quant_plan_default_release;
    context = plan->allocator_context;
    if (plan->index)
        release(plan->index, context);
    if (plan->decisions)
        release(plan->decisions, context);
    plan->summary.state = YVEX_QUANT_PLAN_RELEASED;
    release(plan, context);
}

/* Purpose: borrow the current immutable quant-plan summary.
 * Inputs: optional quant plan in any lifecycle state.
 * Effects: none.
 * Failure: returns null only when the plan handle is absent.
 * Boundary: caller must inspect typed lifecycle/complete fields before consumption. */
const yvex_quant_plan_summary *yvex_quant_plan_summary_get(const yvex_quant_plan *plan) {
    return plan ? &plan->summary : NULL;
}

/* Purpose: retrieve one sealed physical decision by canonical ordinal.
 * Inputs: immutable plan and requested terminal ordinal.
 * Effects: none.
 * Failure: returns null for unsealed plans or out-of-range ordinals.
 * Boundary: returned decision view remains plan-owned. */
const yvex_quant_decision *yvex_quant_plan_decision_at(const yvex_quant_plan *plan,
                                                       unsigned long long ordinal) {
    return plan && plan->summary.state == YVEX_QUANT_PLAN_SEALED &&
                   ordinal < plan->summary.decision_count
               ? &plan->decisions[ordinal]
               : NULL;
}

/* Purpose: find a sealed decision by artifact-neutral logical key.
 * Inputs: immutable plan and exact typed logical key.
 * Effects: performs lookup without mutating counters or plan state.
 * Failure: returns null for invalid state, absent key, or exhausted probe chain.
 * Boundary: expected-O(1) index lookup never parses GGUF tensor names. */
const yvex_quant_decision *yvex_quant_plan_find(const yvex_quant_plan *plan,
                                                const yvex_transform_logical_key *key) {
    unsigned long long hash;
    unsigned long long slot;
    unsigned long long probe;

    if (!plan || plan->summary.state != YVEX_QUANT_PLAN_SEALED || !key)
        return NULL;
    hash = quant_key_hash(key);
    slot = hash & (plan->summary.index_capacity - 1u);
    for (probe = 0u; probe < plan->summary.index_capacity && plan->index[slot].ordinal_plus_one;
         ++probe) {
        if (plan->index[slot].hash == hash) {
            const yvex_quant_decision *decision =
                &plan->decisions[plan->index[slot].ordinal_plus_one - 1u];
            if (quant_key_equal(&decision->logical_key, key))
                return decision;
        }
        slot = (slot + 1u) & (plan->summary.index_capacity - 1u);
    }
    return NULL;
}

/* Purpose: expose the transform IR borrowed by a quant plan.
 * Inputs: optional plan.
 * Effects: none.
 * Failure: returns null for an absent plan.
 * Boundary: lifetime and ownership remain with the upstream IR owner. */
const yvex_transform_ir *yvex_quant_plan_transform_ir(const yvex_quant_plan *plan) {
    return plan ? plan->ir : NULL;
}

/* Purpose: expose the immutable transform binding borrowed by a quant plan.
 * Inputs: optional plan.
 * Effects: none.
 * Failure: returns null for an absent plan.
 * Boundary: quant-plan release never releases or mutates the binding. */
const yvex_transform_binding *yvex_quant_plan_binding(const yvex_quant_plan *plan) {
    return plan ? plan->binding : NULL;
}

/* Purpose: expose the format lowering borrowed by a quant plan.
 * Inputs: optional plan.
 * Effects: none.
 * Failure: returns null when explicit plans have no lowering or plan is absent.
 * Boundary: lowering stays independent from quant decision ownership. */
const yvex_deepseek_gguf_map *yvex_quant_plan_lowering(const yvex_quant_plan *plan) {
    return plan ? plan->map : NULL;
}
