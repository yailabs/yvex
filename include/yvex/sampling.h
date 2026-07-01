/*
 * YVEX - Sampling boundary
 *
 * File: include/yvex/sampling.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines one bounded greedy sampler over the implemented diagnostic logits
 *   buffer. This API does not perform stochastic sampling, append tokens,
 *   generate, or claim real model vocabulary sampling.
 */
#ifndef YVEX_SAMPLING_H
#define YVEX_SAMPLING_H

#include <yvex/logits.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_SAMPLING_STRATEGY_GREEDY = 0
} yvex_sampling_strategy;

typedef struct {
    const yvex_logits_buffer_options *logits_options;
    yvex_sampling_strategy strategy;
} yvex_sampling_options;

typedef struct {
    int sample_created;
    int sample_executed;
    const char *sampler_kind;
    const char *sampling_phase;
    const char *sampling_strategy;
    const char *sampling_source;
    const char *backend_name;
    int logits_invoked;
    int logits_buffer_created;
    const char *logits_buffer_kind;
    const char *logits_phase;
    unsigned long long logits_count;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    unsigned long long candidates_considered;
    const char *tie_break;
    unsigned long long selected_logit_index;
    unsigned int selected_token_id;
    double selected_logit;
    unsigned long long sample_checksum;
    int cleanup_attempted;
    const char *cleanup_status;
    int bounded_sampling_ready;
    int real_vocab_sampling;
    int real_model_sampling;
    int sampling_ready;
    int generation_ready;
    const char *generation_status;
} yvex_sampling_summary;

const char *yvex_sampling_strategy_name(yvex_sampling_strategy strategy);

int yvex_engine_sample_token(yvex_engine *engine,
                             const yvex_sampling_options *options,
                             yvex_sampling_summary *out,
                             yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_SAMPLING_H */
