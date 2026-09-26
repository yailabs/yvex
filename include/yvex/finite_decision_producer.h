/* Bounded local finite-decision producer; no tokenizer or model-family inputs. */
#ifndef INCLUDE_YVEX_FINITE_DECISION_PRODUCER_H_INCLUDED
#define INCLUDE_YVEX_FINITE_DECISION_PRODUCER_H_INCLUDED

#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_FINITE_PRODUCER_SCHEMA_V1 1u
#define YVEX_FINITE_PRODUCER_MAX_CANDIDATES 8u
#define YVEX_FINITE_PRODUCER_QUESTION_CAP 256u
#define YVEX_FINITE_PRODUCER_CONTEXT_CAP 512u
#define YVEX_FINITE_PRODUCER_CANDIDATE_TEXT_CAP 128u
#define YVEX_FINITE_PRODUCER_ID_CAP 128u

typedef enum { YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT = 1 } yvex_finite_producer_score_kind;

typedef struct {
    char id[YVEX_FINITE_PRODUCER_ID_CAP];
    char text[YVEX_FINITE_PRODUCER_CANDIDATE_TEXT_CAP];
} yvex_finite_producer_candidate;
typedef struct {
    unsigned int schema_version;
    char model_alias[128];
    unsigned long long expected_generation;
    char question[YVEX_FINITE_PRODUCER_QUESTION_CAP];
    char context[YVEX_FINITE_PRODUCER_CONTEXT_CAP];
    unsigned long long candidate_count;
    yvex_finite_producer_candidate candidates[YVEX_FINITE_PRODUCER_MAX_CANDIDATES];
} yvex_finite_producer_request;
typedef struct {
    char id[YVEX_FINITE_PRODUCER_ID_CAP];
    double raw_score;
    double relative_candidate_probability; /* Finite-population only; not calibrated. */
} yvex_finite_producer_candidate_result;
typedef struct {
    unsigned int schema_version;
    yvex_finite_producer_score_kind score_kind;
    unsigned long long engine_generation, token_count, candidate_count;
    unsigned long long model_forward_count, sampling_invocation_count, generated_token_count;
    unsigned long long resident_backbone_count, elapsed_nanoseconds;
    unsigned long long source_mapped_bytes, parameter_execution_bytes;
    unsigned long long workspace_host_bytes, workspace_device_bytes;
    char source_identity[65], logical_model_identity[65];
    char binding_identity[65], tokenizer_identity[65], physical_program_identity[65];
    char input_policy_identity[65];
    char input_identity[65], candidate_population_identity[65], result_identity[65];
    int calibrated;
    yvex_finite_producer_candidate_result candidates[YVEX_FINITE_PRODUCER_MAX_CANDIDATES];
} yvex_finite_producer_result;

/* Opens the existing private local host transport for one generation-bound call. */
int yvex_finite_producer_execute_local(const char *socket_path,
    const yvex_finite_producer_request *request,
    yvex_finite_producer_result *result, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
