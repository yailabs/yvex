/* Typed finite-candidate likelihood readout over one admitted model prefix. */
#ifndef INCLUDE_YVEX_INTERNAL_DECISION_READOUT_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_DECISION_READOUT_H_INCLUDED

#include <yvex/internal/runtime.h>
#include <yvex/internal/runtime_prefix.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_DECISION_READOUT_SCHEMA_V1 1u
#define YVEX_DECISION_READOUT_CANDIDATE_ID_CAP 128u

typedef enum {
    YVEX_DECISION_READOUT_SCORE_LOG_LIKELIHOOD = 0
} yvex_decision_readout_score_policy;

typedef struct {
    const char *candidate_id;
    const unsigned int *token_ids;
    unsigned long long token_count;
} yvex_decision_readout_candidate;

typedef struct {
    unsigned int schema_version;
    yvex_backend_kind backend;
    yvex_decision_readout_score_policy score_policy;
    unsigned long long context_capacity, maximum_prefix_tokens;
    unsigned long long maximum_candidate_count, maximum_candidate_tokens;
    unsigned long long maximum_prefix_state_bytes;
    unsigned long long maximum_host_bytes, maximum_device_bytes;
    unsigned long long expected_engine_generation;
    const char *expected_runtime_model_identity;
    const char *expected_runtime_binding_identity;
    const char *expected_tokenizer_identity;
    int (*cancel_requested)(void *context);
    void *cancel_context;
} yvex_decision_readout_options;

typedef struct {
    unsigned int schema_version;
    int available;
    yvex_backend_kind backend;
    yvex_decision_readout_score_policy score_policy;
    unsigned long long engine_generation, vocabulary_size, context_capacity;
    unsigned long long maximum_candidate_count, maximum_candidate_tokens;
    char runtime_model_identity[YVEX_SHA256_HEX_CAP];
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char runtime_binding_identity[YVEX_SHA256_HEX_CAP];
    char tokenizer_identity[YVEX_SHA256_HEX_CAP];
    char forward_program_identity[YVEX_SHA256_HEX_CAP];
    char output_program_identity[YVEX_SHA256_HEX_CAP];
    char readout_implementation_identity[YVEX_SHA256_HEX_CAP];
    char context_identity[YVEX_SHA256_HEX_CAP];
} yvex_decision_readout_context_summary;

typedef struct {
    unsigned int schema_version;
    unsigned long long engine_generation, prefix_token_count;
    unsigned long long committed_sequence_length;
    unsigned long long shared_state_bytes, mapped_state_bytes;
    unsigned long long logits_bytes, prefix_forward_count;
    char runtime_model_identity[YVEX_SHA256_HEX_CAP];
    char runtime_binding_identity[YVEX_SHA256_HEX_CAP];
    char tokenizer_identity[YVEX_SHA256_HEX_CAP];
    char context_identity[YVEX_SHA256_HEX_CAP];
    char prefix_token_identity[YVEX_SHA256_HEX_CAP];
    char runtime_prefix_identity[YVEX_SHA256_HEX_CAP];
    char shared_logits_identity[YVEX_SHA256_HEX_CAP];
    char prefix_identity[YVEX_SHA256_HEX_CAP];
} yvex_decision_readout_prefix_summary;

typedef struct {
    char candidate_id[YVEX_DECISION_READOUT_CANDIDATE_ID_CAP];
    unsigned long long token_count, teacher_forced_tokens;
    double candidate_log_likelihood;
    double mean_token_log_probability;
    double relative_candidate_probability;
    char token_identity[YVEX_SHA256_HEX_CAP];
    char candidate_result_identity[YVEX_SHA256_HEX_CAP];
} yvex_decision_readout_candidate_result;

typedef struct {
    unsigned int schema_version;
    int completed, relative_distribution_available, calibrated;
    unsigned long long engine_generation, candidate_count;
    unsigned long long prefix_token_count, candidate_token_count;
    unsigned long long prefix_forward_count, teacher_forced_forward_count;
    unsigned long long logits_row_count, sampling_invocation_count;
    unsigned long long generated_token_count, resident_backbone_count;
    unsigned long long mapped_model_bytes, prepared_model_bytes;
    unsigned long long resident_host_model_bytes;
    unsigned long long resident_device_model_bytes;
    unsigned long long shared_prefix_state_bytes;
    /* Maximum branch-owned physical allocation observed after attach and each
     * committed token; not a process/device peak or an incremental prefix cost. */
    unsigned long long peak_candidate_state_bytes, peak_workspace_bytes;
    unsigned long long logits_buffer_bytes;
    unsigned long long prefix_nanoseconds, candidate_nanoseconds;
    unsigned long long total_nanoseconds;
    char runtime_model_identity[YVEX_SHA256_HEX_CAP];
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char runtime_binding_identity[YVEX_SHA256_HEX_CAP];
    char tokenizer_identity[YVEX_SHA256_HEX_CAP];
    char readout_implementation_identity[YVEX_SHA256_HEX_CAP];
    char prefix_identity[YVEX_SHA256_HEX_CAP];
    char shared_state_identity_before[YVEX_SHA256_HEX_CAP];
    char shared_state_identity_after[YVEX_SHA256_HEX_CAP];
    char candidate_population_identity[YVEX_SHA256_HEX_CAP];
    char score_policy_identity[YVEX_SHA256_HEX_CAP];
    char result_identity[YVEX_SHA256_HEX_CAP];
    yvex_decision_readout_candidate_result *candidates;
} yvex_decision_readout_result;

typedef struct yvex_decision_readout_context yvex_decision_readout_context;
typedef struct yvex_decision_readout_prefix yvex_decision_readout_prefix;

int yvex_decision_readout_log_probability(
    const float *logits, unsigned long long vocabulary_size,
    unsigned int token_id, double *out, yvex_error *err);
int yvex_decision_readout_relative_distribution(
    yvex_decision_readout_candidate_result *candidates,
    unsigned long long candidate_count, yvex_error *err);
int yvex_decision_readout_context_open(
    yvex_decision_readout_context **out, yvex_model_engine *model,
    const yvex_decision_readout_options *options, yvex_error *err);
/* On failed cleanup, open may return a non-NULL invalidated owner. Close must
 * be retried; execution cannot reuse retained failed candidate resources. */
int yvex_decision_readout_context_summary_copy(
    const yvex_decision_readout_context *context,
    yvex_decision_readout_context_summary *summary, yvex_error *err);
int yvex_decision_readout_prefix_prepare(
    yvex_decision_readout_context *context,
    const unsigned int *prefix_tokens, unsigned long long prefix_token_count,
    yvex_decision_readout_prefix **out,
    yvex_decision_readout_prefix_summary *summary, yvex_error *err);
int yvex_decision_readout_execute(
    yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    const char *expected_prefix_identity,
    const yvex_decision_readout_candidate *candidates,
    unsigned long long candidate_count,
    yvex_decision_readout_result *result, yvex_error *err);
void yvex_decision_readout_result_release(
    yvex_decision_readout_result *result);
void yvex_decision_readout_prefix_close(
    yvex_decision_readout_prefix **prefix);
int yvex_decision_readout_context_close(
    yvex_decision_readout_context **context, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
