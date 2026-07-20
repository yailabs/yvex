/*
 * deepseek_attention_reference.h - independent DeepSeek attention test oracle.
 *
 * Owner:
 *   tests/reference
 *
 * Owns:
 *   declarations for scalar algorithms used only to judge the production
 *   DeepSeek attention executor.
 *
 * Does not own:
 *   production graph behavior, runtime admission, payload ownership, CUDA
 *   capability, persistent KV, generation, or project claims.
 *
 * Invariants:
 *   the reference implementation never calls production attention numeric
 *   primitives.
 *
 * Boundary:
 *   independent test evidence is not a runtime implementation.
 */
#ifndef YVEX_TEST_DEEPSEEK_ATTENTION_REFERENCE_H
#define YVEX_TEST_DEEPSEEK_ATTENTION_REFERENCE_H

#include <yvex/internal/families/deepseek_v4.h>
#include <yvex/internal/graph.h>

#define YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP 192u
#define YVEX_TEST_ATTENTION_EVIDENCE_SCHEMA_V2 2u

typedef enum {
    YVEX_TEST_ATTENTION_STAGE_INPUT = 0,
    YVEX_TEST_ATTENTION_STAGE_Q_LOW,
    YVEX_TEST_ATTENTION_STAGE_QUERY,
    YVEX_TEST_ATTENTION_STAGE_RAW_KV,
    YVEX_TEST_ATTENTION_STAGE_COMPRESSED_KV,
    YVEX_TEST_ATTENTION_STAGE_INDEXER_KV,
    YVEX_TEST_ATTENTION_STAGE_INDEX_QUERY,
    YVEX_TEST_ATTENTION_STAGE_INDEX_WEIGHTS,
    YVEX_TEST_ATTENTION_STAGE_ATTENTION,
    YVEX_TEST_ATTENTION_STAGE_OUTPUT,
    YVEX_TEST_ATTENTION_STAGE_MAIN_STATE,
    YVEX_TEST_ATTENTION_STAGE_INDEXER_STATE,
    YVEX_TEST_ATTENTION_STAGE_COUNT
} yvex_test_attention_reference_stage;

typedef struct {
    unsigned int schema_version;
    const char *path_name;
    double absolute[YVEX_TEST_ATTENTION_STAGE_COUNT];
    double relative[YVEX_TEST_ATTENTION_STAGE_COUNT];
    unsigned long long qtype_binding_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    int compressed_positions_exact;
    int indexer_positions_exact;
    int topk_positions_exact;
} yvex_test_attention_reference_contract;

typedef struct {
    unsigned int schema_version;
    char oracle_trace_identity[YVEX_SHA256_HEX_CAP];
    char oracle_output_identity[YVEX_SHA256_HEX_CAP];
    char fixture_identity[YVEX_SHA256_HEX_CAP];
    char history_identity[YVEX_SHA256_HEX_CAP];
    char comparison_contract_identity[YVEX_SHA256_HEX_CAP];
    char evidence_identity[YVEX_SHA256_HEX_CAP];
    unsigned long long exact_topk_positions;
} yvex_test_attention_reference_evidence;

typedef struct {
    unsigned long long compared_values;
    unsigned long long discrete_mismatches;
    unsigned long long first_failed_index;
    double maximum_absolute_error;
    double maximum_relative_error;
    double squared_error_sum;
    const char *first_failed_stage;
} yvex_test_attention_reference_metrics;

const char *yvex_test_attention_status_name(yvex_attention_status status);
const char *yvex_test_attention_failure_name(yvex_attention_failure_code code);

int yvex_test_attention_reference_hadamard(const float *input,
                                           unsigned long long length,
                                           float scale,
                                           int reject_nonfinite,
                                           float *output);
int yvex_test_attention_reference_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_execution_trace *trace,
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP]);
int yvex_test_attention_reference_compare(
    const yvex_attention_execution_trace *production,
    const yvex_attention_execution_trace *reference,
    double absolute_tolerance,
    double relative_tolerance,
    yvex_test_attention_reference_metrics *metrics);
int yvex_test_attention_reference_compare_contract(
    const yvex_attention_execution_trace *production,
    const yvex_attention_execution_trace *reference,
    const yvex_test_attention_reference_contract *contract,
    yvex_test_attention_reference_metrics *metrics);
int yvex_test_attention_reference_evidence_build(
    const yvex_attention_execution_trace *reference,
    const yvex_attention_history_view *history,
    const char *attention_plan_identity,
    const yvex_test_attention_reference_contract *contract,
    yvex_test_attention_reference_evidence *evidence);
int yvex_test_attention_reference_topk(
    const float *scores, const unsigned long long *positions,
    unsigned long long count, unsigned long long k,
    unsigned long long *selected, unsigned long long *selected_count);

#endif
