/*
 * yvex_sampling_report.h - typed sampling report API.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   sampling report request and result shapes for the diagnostic sampler.
 *
 * Does not own:
 *   CLI input grammar, command dispatch, rendering, stdout/stderr output,
 *   stochastic sampling, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   reports carry facts only and use the lowest true support boundary.
 *
 * Boundary:
 *   sampling reports do not imply real vocabulary sampling or generation.
 */
#ifndef YVEX_SAMPLING_REPORT_H
#define YVEX_SAMPLING_REPORT_H

#include <yvex/yvex.h>

typedef enum {
    YVEX_SAMPLING_REPORT_NORMAL = 0,
    YVEX_SAMPLING_REPORT_AUDIT
} yvex_sampling_report_mode;

typedef struct {
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;
    const char *tokens_text;

    yvex_sampling_strategy strategy;
    unsigned long long logits_count;

    unsigned long long position_start;
    unsigned long long context_length;
    int context_length_seen;

    int attach_kv;
    yvex_kv_shape kv_shape;

    int layer_count_seen;
    unsigned long long layer_count;
    unsigned long long layer_hidden_dim;
    unsigned long long layer_head_dim;
    unsigned long long layer_ffn_dim;

    int chunk_size_seen;
    unsigned long long chunk_size;
} yvex_sampling_report_request;

typedef struct {
    const char *status;
    const char *model_arg;
    const char *backend_name;
    const char *segment_name;

    const char *token_input_status;
    unsigned long long input_token_count;

    yvex_sampling_summary summary;

    int graph_guard_rendered;
    const char *graph_guard_status;
    const char *graph_guard_phase;

    int cleanup_attempted;
    const char *cleanup_status;

    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;

    int real_vocab_sampling;
    int real_model_sampling;
    int sampling_ready;
    int generation_ready;

    int exit_code;
} yvex_sampling_report;

int yvex_sampling_report_build(const yvex_sampling_report_request *request,
                               yvex_sampling_report *report,
                               yvex_error *err);

#endif
