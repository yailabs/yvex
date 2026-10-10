/* Goal-driven physical compilation. Planning never loads an engine or proves
 * quality. Schema checks precede all field reads; unknown costs remain unknown. */
#ifndef YVEX_OPTIMIZATION_H
#define YVEX_OPTIMIZATION_H

#include <yvex/backend.h>
#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_OPTIMIZATION_SCHEMA_V1 1u
#define YVEX_OPTIMIZATION_MAX_CANDIDATES 32u

typedef enum {
    YVEX_OPTIMIZATION_BALANCED = 0,
    YVEX_OPTIMIZATION_THROUGHPUT,
    YVEX_OPTIMIZATION_MEMORY,
    YVEX_OPTIMIZATION_QUALITY
} yvex_optimization_goal;

typedef enum {
    YVEX_OPTIMIZATION_NEEDS_QUALIFICATION = 0,
    YVEX_OPTIMIZATION_RESOURCE_INFEASIBLE,
    YVEX_OPTIMIZATION_NUMERICAL_INCOMPATIBLE,
    YVEX_OPTIMIZATION_IMPLEMENTATION_UNSUPPORTED,
    YVEX_OPTIMIZATION_COMPILATION_REFUSED
} yvex_optimization_state;

enum {
    YVEX_OPTIMIZATION_MISSING_WORKSPACE = 1u << 0,
    YVEX_OPTIMIZATION_MISSING_STATE = 1u << 1,
    YVEX_OPTIMIZATION_MISSING_MODEL_EXECUTION = 1u << 2,
    YVEX_OPTIMIZATION_MISSING_QUALITY = 1u << 3,
    YVEX_OPTIMIZATION_MISSING_PERFORMANCE = 1u << 4,
    YVEX_OPTIMIZATION_MISSING_LIFECYCLE = 1u << 5
};

typedef struct {
    unsigned int schema_version;
    const char *target_id, *source_path, *models_root, *source_manifest_path;
    const char *imatrix_path;
    /* Optional expert restriction. Both modes use the same compiler. */
    const char *policy_path;
    yvex_optimization_goal goal;
    yvex_backend_kind backend;
    unsigned int compute_major, compute_minor, device_count;
    unsigned int maximum_candidates;
    unsigned long long context_tokens, prefill_tokens, concurrent_sequences;
    unsigned long long system_memory_bytes, available_memory_bytes;
    unsigned long long memory_limit_bytes, system_reserve_bytes;
    int allow_approximation, require_routed_matrix;
} yvex_optimization_request;

typedef struct {
    unsigned int schema_version;
    yvex_optimization_state state;
    unsigned int missing_evidence;
    char recipe[128], physical_variant_identity[65], transform_identity[65];
    char source_identity[65], policy_identity[65], candidate_identity[65];
    unsigned long long artifact_bytes, encoded_bytes, maximum_tensor_bytes;
    /* Conservative initial lower bound; excludes explicitly missing state/workspace.
     * It is not an observed physical working set or a successful admission. */
    unsigned long long reserve_bytes, initial_required_bytes, memory_budget_bytes;
    unsigned long long approximate_tensors, routed_tensors, matrix_incompatible_tensors;
    int operand_codecs_available;
    /* 0: no static catalog exposed; 1: matches exposed constraint; 2: incompatible;
     * 3: outside catalog, generic complete-production proof required.
     * Neither a match nor a proof-capable path authenticates an unproduced artifact. */
    unsigned int artifact_catalog_compatibility;
    int failure_status;
    char reason[256];
} yvex_optimization_candidate;

typedef struct yvex_optimization_search yvex_optimization_search;
/* Builds immutable candidate plans sequentially using registered exact-target
 * recipes. No payload transformation/emission, backend opening or service mutation.
 * out remains NULL on malformed requests/integrity failures. */
int yvex_optimization_search_open(yvex_optimization_search **out,
                                 const yvex_optimization_request *request,
                                 yvex_error *err);
void yvex_optimization_search_close(yvex_optimization_search **search);
unsigned int yvex_optimization_search_count(const yvex_optimization_search *search);
const yvex_optimization_candidate *yvex_optimization_search_at(
    const yvex_optimization_search *search, unsigned int index);
/* Borrow the selected, sealed policy for the existing quant plan/emit workflow.
 * Valid until search_close. No selection may promote unmeasured qualification. */
struct yvex_quant_policy;
int yvex_optimization_search_policy(const yvex_optimization_search *search,
                                   const char *candidate_identity,
                                   const struct yvex_quant_policy **out,
                                   yvex_error *err);
const char *yvex_optimization_state_name(yvex_optimization_state state);
/* Deterministic experiment priority, NOT a measured performance or quality
 * ranking. Static-refused rows follow eligible rows; ties retain recipe order.
 * Search-at exposes this order. Unknown quality/rates are never imputed. */
const char *yvex_optimization_priority_basis(yvex_optimization_goal goal);
/* Compare two compiler assessments for experiment ordering (-1, 0, +1).
 * Refuses stale records. It does not authenticate caller-supplied evidence. */
int yvex_optimization_candidate_priority(yvex_optimization_goal goal,
    const yvex_optimization_candidate *left,
    const yvex_optimization_candidate *right, int *order, yvex_error *err);
/* Re-evaluates numeric/resource constraints, never upgrades evidence. The caller
 * must supply a canonical compiler-produced candidate. Useful for conservative
 * resource scenario comparisons; does not claim runtime admission. */
int yvex_optimization_candidate_assess(const yvex_optimization_request *request,
                                      yvex_optimization_candidate *candidate,
                                      yvex_error *err);

/* Evaluation supplies authenticated, comparable projections from the existing
 * qualification authority. These records are not a second evidence store and
 * this pure calculation does not verify receipts or confer qualification.
 * comparison_identity binds checkpoint/workload/hardware/measurement policy;
 * quality_reference_identity binds the independently qualified loss metric;
 * NULL is legal only with MISSING_QUALITY and cannot earn selection.
 * Lower quality_loss is better, in that exact metric's units. */
typedef struct {
    unsigned int schema_version, samples, missing_evidence;
    const char *candidate_identity, *comparison_identity;
    const char *quality_reference_identity, *evidence_identity;
    double prefill_tokens_per_second, decode_tokens_per_second;
    double ttft_seconds, preparation_seconds, quality_loss;
    unsigned long long peak_working_bytes;
} yvex_optimization_observation;

typedef struct {
    unsigned int schema_version, minimum_samples;
    double minimum_prefill, minimum_decode, maximum_ttft, maximum_quality_loss;
    unsigned long long maximum_working_bytes;
} yvex_optimization_constraints;

typedef enum {
    YVEX_OPTIMIZATION_FRONTIER = 0,
    YVEX_OPTIMIZATION_DOMINATED,
    YVEX_OPTIMIZATION_OUTSIDE_CONSTRAINTS,
    YVEX_OPTIMIZATION_EVIDENCE_INCOMPLETE
} yvex_optimization_selection_state;

typedef struct {
    unsigned int schema_version;
    yvex_optimization_selection_state state;
    /* Index in the supplied population, or count when no dominator exists. */
    unsigned int dominator;
} yvex_optimization_selection;

/* Bounded measured Pareto frontier, not a global optimum or quality oracle.
 * Unknown evidence and undersampled rows cannot dominate eligible rows.
 * Incompatible keys, duplicate candidates and nonfinite facts refuse atomically.
 * A zero minimum rate/maximum TTFT disables that one operational threshold;
 * the quality floor, memory bound and minimum sample count are mandatory.
 * Equivalent rows remain on the frontier: no hidden scalar tradeoff score. */
int yvex_optimization_select(const yvex_optimization_constraints *constraints,
    const yvex_optimization_observation *observations, unsigned int count,
    yvex_optimization_selection *out, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif /* YVEX_OPTIMIZATION_H */
