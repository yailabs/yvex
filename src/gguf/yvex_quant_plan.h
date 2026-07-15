/*
 * yvex_quant_plan.h - private immutable physical quantization plan ABI.
 *
 * Owner: TRACK.QUANT.
 * Owns: versioned profiles, terminal physical decisions, plan identity,
 *   deterministic indexes, sealing, lookup, and release.
 * Does not own: Transformation IR semantics, GGUF naming/layout identity,
 *   payload IO, numeric execution, CUDA launches, writing, or rendering.
 * Invariants: one sealed decision bijects each IR terminal with one lowering
 *   descriptor; the IR, map, and binding outlive the plan.
 * Boundary: a selected physical plan is not encoded payload or an artifact.
 */
#ifndef YVEX_QUANT_PLAN_H
#define YVEX_QUANT_PLAN_H

#include "yvex_quant_numeric.h"
#include "../model/compilation/yvex_transform_binding.h"
#include "../model/target/yvex_deepseek_gguf_map.h"

#define YVEX_QUANT_PROFILE_SCHEMA_VERSION 1u
#define YVEX_QUANT_PLAN_IDENTITY_CAP 65u
#define YVEX_QUANT_RELEASE_PROFILE_NAME \
    "deepseek-v4-flash-q8_0-q2_k-v1"
#define YVEX_QUANT_REFERENCE_PROFILE_NAME \
    "deepseek-v4-flash-source-faithful-v1"

typedef enum {
    YVEX_QUANT_PLAN_BUILDING = 0,
    YVEX_QUANT_PLAN_SEALED,
    YVEX_QUANT_PLAN_RELEASED
} yvex_quant_plan_state;

typedef enum {
    YVEX_QUANT_PROFILE_SOURCE_FAITHFUL = 0,
    YVEX_QUANT_PROFILE_RELEASE_Q8_Q2
} yvex_quant_profile_kind;

typedef enum {
    YVEX_QUANT_PHYSICAL_EXACT_SCALAR = 0,
    YVEX_QUANT_PHYSICAL_BLOCK_QUANTIZED
} yvex_quant_physical_class;

typedef struct {
    yvex_transform_logical_key logical_key;
    unsigned long long terminal_ordinal;
    unsigned long long terminal_value_id;
    unsigned long long node_id;
    yvex_tensor_role role;
    yvex_transform_scope scope;
    yvex_transform_operation_kind operation;
    yvex_quant_physical_class physical_class;
    unsigned int qtype;
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_QTYPE_MAX_DIMS];
    unsigned int row_axis;
    unsigned long long row_width;
    unsigned long long row_count;
    unsigned long long element_count;
    unsigned long long encoded_bytes;
    int approximation;
    yvex_quant_calibration_requirement calibration;
    int reference_decoder_required;
    int cpu_compute_available;
    int cuda_compute_available;
    unsigned int numeric_contract_version;
    char decision_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
} yvex_quant_decision;

typedef struct {
    yvex_quant_profile_kind kind;
    const char *name;
    unsigned long long terminal_count;
    unsigned long long encoded_bytes;
    unsigned long long exact_scalar_bytes;
    unsigned long long q8_0_bytes;
    unsigned long long q2_k_bytes;
    unsigned long long mxfp4_bytes;
    unsigned long long qtype_tensor_counts[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    int calibration_required;
    int numerically_admissible;
    int compute_admissible;
} yvex_quant_candidate_summary;

typedef struct {
    unsigned int schema_version;
    yvex_quant_plan_state state;
    char profile_name[64];
    char profile_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    char transform_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    unsigned long long source_snapshot_identity;
    char required_payload_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    unsigned long long mapping_identity;
    char backend_compute_contract[64];
    char calibration_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    unsigned long long terminal_count;
    unsigned long long decision_count;
    unsigned long long source_value_count;
    unsigned long long encoded_bytes;
    unsigned long long exact_scalar_bytes;
    unsigned long long q8_0_bytes;
    unsigned long long q2_k_bytes;
    unsigned long long mxfp4_bytes;
    unsigned long long qtype_tensor_counts[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    unsigned long long role_tensor_counts[YVEX_TENSOR_ROLE_COUNT];
    unsigned long long index_capacity;
    unsigned long long lookup_count;
    size_t owned_bytes;
    size_t peak_builder_bytes;
    unsigned long long payload_bytes_read;
    int calibration_required;
    int complete;
    yvex_quant_candidate_summary candidates[2];
} yvex_quant_plan_summary;

typedef void *(*yvex_quant_allocate_fn)(size_t size, void *context);
typedef void (*yvex_quant_release_fn)(void *allocation, void *context);

typedef struct {
    yvex_quant_allocate_fn allocate;
    yvex_quant_release_fn release;
    void *context;
    size_t maximum_owned_bytes;
} yvex_quant_plan_options;

typedef struct {
    unsigned int qtype;
    int approximation;
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_QTYPE_MAX_DIMS];
    unsigned int row_axis;
} yvex_quant_explicit_decision;

typedef struct yvex_quant_plan yvex_quant_plan;

int yvex_deepseek_quant_plan_build(
    yvex_quant_plan **out,
    const yvex_transform_ir *ir,
    const yvex_transform_binding *binding,
    const yvex_deepseek_gguf_map *map,
    const yvex_quant_plan_options *options,
    yvex_quant_failure *failure,
    yvex_error *err);
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
    yvex_error *err);
void yvex_quant_plan_release(yvex_quant_plan **plan);

const yvex_quant_plan_summary *yvex_quant_plan_summary_get(
    const yvex_quant_plan *plan);
const yvex_quant_decision *yvex_quant_plan_decision_at(
    const yvex_quant_plan *plan, unsigned long long ordinal);
const yvex_quant_decision *yvex_quant_plan_find(
    const yvex_quant_plan *plan, const yvex_transform_logical_key *key);
const yvex_transform_ir *yvex_quant_plan_transform_ir(
    const yvex_quant_plan *plan);
const yvex_transform_binding *yvex_quant_plan_binding(
    const yvex_quant_plan *plan);
const yvex_deepseek_gguf_map *yvex_quant_plan_lowering(
    const yvex_quant_plan *plan);

#endif
