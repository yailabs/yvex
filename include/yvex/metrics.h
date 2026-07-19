/* Owner: public metrics ABI.
 * Owns: metrics counters, profiles, and trace events.
 * Does not own: capability admission, benchmark claims, or operator rendering.
 * Invariants: declarations are format-stable, externally consumable, and independently includable.
 * Boundary: measurement and trace data contracts.
 * Purpose: Expose measurement and trace data contracts.
 * Inputs: Typed caller-owned values and immutable borrowed views as declared below.
 * Effects: Only functions with explicit lifecycle or I/O contracts mutate external state.
 * Failure: Typed status and error outputs remain authoritative; declarations add no capability. */
#ifndef YVEX_METRICS_H
#define YVEX_METRICS_H

#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Metric counters. */
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

/* Profile serialization. */
typedef struct {
    const char *run_id;
    const char *command;
    const char *model_name;
    const char *backend_name;
    const char *status;
    int execution_ready;
    yvex_metric_counters counters;
} yvex_profile_summary;

int yvex_profile_write_json(const char *path,
                            const yvex_profile_summary *summary,
                            const yvex_metrics *metrics,
                            yvex_error *err);
int yvex_metrics_write_json(const char *path,
                            const yvex_metrics *metrics,
                            yvex_error *err);

/* Trace events. */
typedef struct yvex_trace yvex_trace;

typedef enum {
    YVEX_TRACE_EVENT_RUN_START = 0,
    YVEX_TRACE_EVENT_RUN_END,
    YVEX_TRACE_EVENT_PHASE_START,
    YVEX_TRACE_EVENT_PHASE_END,
    YVEX_TRACE_EVENT_ENGINE,
    YVEX_TRACE_EVENT_BACKEND,
    YVEX_TRACE_EVENT_SESSION,
    YVEX_TRACE_EVENT_PROMPT,
    YVEX_TRACE_EVENT_TOKENIZE,
    YVEX_TRACE_EVENT_ACCEPT_TOKENS,
    YVEX_TRACE_EVENT_CHAT_TURN,
    YVEX_TRACE_EVENT_ERROR
} yvex_trace_event_kind;

typedef struct {
    const char *path;
    const char *run_id;
    int enabled;
} yvex_trace_options;

int yvex_trace_open(yvex_trace **out,
                    const yvex_trace_options *options,
                    yvex_error *err);
void yvex_trace_close(yvex_trace *trace);

int yvex_trace_emit(yvex_trace *trace,
                    yvex_trace_event_kind kind,
                    const char *name,
                    const char *status,
                    const char *message,
                    yvex_error *err);

const char *yvex_trace_event_kind_name(yvex_trace_event_kind kind);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_METRICS_H */
