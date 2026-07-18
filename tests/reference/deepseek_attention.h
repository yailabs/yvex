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

#include "src/graph/private.h"

#define YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP 192u

typedef struct {
    unsigned long long compared_values;
    unsigned long long discrete_mismatches;
    double maximum_absolute_error;
    double maximum_relative_error;
    double squared_error_sum;
    const char *first_failed_stage;
} yvex_test_attention_reference_metrics;

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

#endif
