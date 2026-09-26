/* Exact token-domain finite-decision computation, independent of generation. */
#ifndef INCLUDE_YVEX_FINITE_DECISION_H_INCLUDED
#define INCLUDE_YVEX_FINITE_DECISION_H_INCLUDED

#include <yvex/backend.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_FINITE_DECISION_SCHEMA_V1 1u
#define YVEX_FINITE_DECISION_MAX_CANDIDATES 32u
#define YVEX_FINITE_DECISION_ID_CAP 128u
#define YVEX_FINITE_DECISION_IDENTITY_CAP 65u

typedef struct yvex_finite_decision_engine yvex_finite_decision_engine;
typedef struct {
    unsigned int schema_version;
    const char *source_path, *binding_path;
    yvex_backend_kind backend;
    unsigned long long generation, maximum_tokens;
    unsigned long long maximum_host_bytes, maximum_device_bytes;
} yvex_finite_decision_engine_options;
typedef struct {
    const char *candidate_id;
    unsigned long long marker_position;
} yvex_finite_decision_candidate;
typedef struct {
    unsigned int schema_version;
    unsigned long long expected_generation;
    const char *expected_binding_identity, *expected_tokenizer_identity;
    const unsigned int *token_ids;
    unsigned long long token_count;
    unsigned int input_type_id;
    const yvex_finite_decision_candidate *candidates;
    unsigned long long candidate_count;
    int (*cancel_requested)(void *);
    void *cancel_context;
} yvex_finite_decision_request;
typedef struct {
    char candidate_id[YVEX_FINITE_DECISION_ID_CAP];
    unsigned long long marker_position;
    double raw_logit, relative_candidate_probability;
} yvex_finite_decision_candidate_result;
typedef struct {
    unsigned int schema_version;
    unsigned long long engine_generation, token_count, candidate_count;
    unsigned long long model_forward_count, sampling_invocation_count, generated_token_count;
    unsigned long long resident_backbone_count, elapsed_nanoseconds;
    unsigned long long source_mapped_bytes, parameter_execution_bytes;
    unsigned long long workspace_host_bytes, workspace_device_bytes;
    char source_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char logical_model_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char binding_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char tokenizer_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char physical_program_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char input_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char candidate_population_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    char result_identity[YVEX_FINITE_DECISION_IDENTITY_CAP];
    int calibrated;
    yvex_finite_decision_candidate_result candidates[YVEX_FINITE_DECISION_MAX_CANDIDATES];
} yvex_finite_decision_result;

/* The caller supplies exact tokenizer-domain tokens and marker positions. This
 * API does not tokenize text, infer candidates, sample, or interpret scores. */
/* A failed open may return a non-runnable cleanup owner; close it explicitly. */
int yvex_finite_decision_engine_open(yvex_finite_decision_engine **,
    const yvex_finite_decision_engine_options *, yvex_error *);
int yvex_finite_decision_execute(yvex_finite_decision_engine *,
    const yvex_finite_decision_request *, yvex_finite_decision_result *, yvex_error *);
int yvex_finite_decision_engine_close(yvex_finite_decision_engine **, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
