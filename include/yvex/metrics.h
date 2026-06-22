/*
 * YVEX - Runtime metrics
 *
 * File: include/yvex/metrics.h
 * Layer: public runtime observability API
 *
 * Purpose:
 *   Defines the J0 metrics collector for implemented runtime shell phases and
 *   accepted-token counters. Metrics describe what exists today; they do not
 *   report decode, sampling, inference, or backend kernel throughput.
 *
 * Owns:
 *   - yvex_metrics
 *   - phase timing summaries
 *   - accepted-token/runtime counters
 *
 * Does not own:
 *   - trace files
 *   - generated-token benchmarks
 *   - server/provider observability
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_metrics
 */
#ifndef YVEX_METRICS_H
#define YVEX_METRICS_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_metrics yvex_metrics;

typedef enum {
    YVEX_METRIC_PHASE_ENGINE_OPEN = 0,
    YVEX_METRIC_PHASE_ARTIFACT_OPEN,
    YVEX_METRIC_PHASE_GGUF_PARSE,
    YVEX_METRIC_PHASE_TENSOR_TABLE,
    YVEX_METRIC_PHASE_MODEL_DESCRIPTOR,
    YVEX_METRIC_PHASE_TOKENIZER,
    YVEX_METRIC_PHASE_GRAPH_BUILD,
    YVEX_METRIC_PHASE_PLAN_BUILD,
    YVEX_METRIC_PHASE_BACKEND_OPEN,
    YVEX_METRIC_PHASE_SESSION_CREATE,
    YVEX_METRIC_PHASE_PROMPT_RENDER,
    YVEX_METRIC_PHASE_TOKENIZE,
    YVEX_METRIC_PHASE_ACCEPT_TOKENS,
    YVEX_METRIC_PHASE_CHAT_TURN,
    YVEX_METRIC_PHASE_TOTAL
} yvex_metric_phase;

typedef struct {
    unsigned long long prompt_tokens;
    unsigned long long accepted_tokens;
    unsigned long long rejected_tokens;
    unsigned long long chat_turns;
    unsigned long long bytes_read;
    unsigned long long known_tensor_bytes;
    unsigned long long unsupported_tensor_accounting;
} yvex_metric_counters;

typedef struct {
    yvex_metric_phase phase;
    const char *name;
    unsigned long long count;
    unsigned long long total_ns;
    unsigned long long last_ns;
    unsigned long long min_ns;
    unsigned long long max_ns;
} yvex_metric_phase_summary;

int yvex_metrics_create(yvex_metrics **out, yvex_error *err);
void yvex_metrics_close(yvex_metrics *metrics);
void yvex_metrics_reset(yvex_metrics *metrics);

int yvex_metrics_phase_begin(yvex_metrics *metrics,
                             yvex_metric_phase phase,
                             unsigned long long *token,
                             yvex_error *err);
int yvex_metrics_phase_end(yvex_metrics *metrics,
                           yvex_metric_phase phase,
                           unsigned long long token,
                           yvex_error *err);

int yvex_metrics_add_prompt_tokens(yvex_metrics *metrics,
                                   unsigned long long n,
                                   yvex_error *err);
int yvex_metrics_add_accepted_tokens(yvex_metrics *metrics,
                                     unsigned long long n,
                                     yvex_error *err);
int yvex_metrics_add_rejected_tokens(yvex_metrics *metrics,
                                     unsigned long long n,
                                     yvex_error *err);
int yvex_metrics_add_chat_turn(yvex_metrics *metrics,
                               yvex_error *err);
int yvex_metrics_set_model_bytes(yvex_metrics *metrics,
                                 unsigned long long known_tensor_bytes,
                                 unsigned long long unsupported_tensor_accounting,
                                 yvex_error *err);

int yvex_metrics_get_counters(const yvex_metrics *metrics,
                              yvex_metric_counters *out,
                              yvex_error *err);
int yvex_metrics_get_phase(const yvex_metrics *metrics,
                           yvex_metric_phase phase,
                           yvex_metric_phase_summary *out,
                           yvex_error *err);

const char *yvex_metric_phase_name(yvex_metric_phase phase);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_METRICS_H */
