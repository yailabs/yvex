/*
 * YVEX - Runtime metrics collector
 *
 * File: yvex_metrics.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Implements observability layer phase timing and accepted-token counters for runtime shell
 *   paths that exist today. This module does not compute inference throughput.
 */
#include <yvex/metrics.h>

#include "yvex_metrics_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define YVEX_METRIC_PHASE_COUNT ((unsigned long)YVEX_METRIC_PHASE_TOTAL + 1ul)

typedef struct {
    unsigned long long count;
    unsigned long long total_ns;
    unsigned long long last_ns;
    unsigned long long min_ns;
    unsigned long long max_ns;
    unsigned long long active_token;
    unsigned long long active_start_ns;
} yvex_metric_slot;

struct yvex_metrics {
    yvex_metric_counters counters;
    yvex_metric_slot phases[YVEX_METRIC_PHASE_COUNT];
    unsigned long long next_token;
};

static int phase_valid(yvex_metric_phase phase)
{
    return phase >= YVEX_METRIC_PHASE_ENGINE_OPEN && phase <= YVEX_METRIC_PHASE_TOTAL;
}

static int add_ull(unsigned long long *dst, unsigned long long n, yvex_error *err, const char *where)
{
    if (!dst) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "counter pointer is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (ULLONG_MAX - *dst < n) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "counter overflow");
        return YVEX_ERR_BOUNDS;
    }
    *dst += n;
    yvex_error_clear(err);
    return YVEX_OK;
}

const char *yvex_metric_phase_name(yvex_metric_phase phase)
{
    switch (phase) {
    case YVEX_METRIC_PHASE_ENGINE_OPEN: return "engine_open";
    case YVEX_METRIC_PHASE_ARTIFACT_OPEN: return "artifact_open";
    case YVEX_METRIC_PHASE_GGUF_PARSE: return "gguf_parse";
    case YVEX_METRIC_PHASE_TENSOR_TABLE: return "tensor_table";
    case YVEX_METRIC_PHASE_MODEL_DESCRIPTOR: return "model_descriptor";
    case YVEX_METRIC_PHASE_TOKENIZER: return "tokenizer";
    case YVEX_METRIC_PHASE_GRAPH_BUILD: return "graph_build";
    case YVEX_METRIC_PHASE_PLAN_BUILD: return "plan_build";
    case YVEX_METRIC_PHASE_BACKEND_OPEN: return "backend_open";
    case YVEX_METRIC_PHASE_SESSION_CREATE: return "session_create";
    case YVEX_METRIC_PHASE_PROMPT_RENDER: return "prompt_render";
    case YVEX_METRIC_PHASE_TOKENIZE: return "tokenize";
    case YVEX_METRIC_PHASE_ACCEPT_TOKENS: return "accept_tokens";
    case YVEX_METRIC_PHASE_CHAT_TURN: return "chat_turn";
    case YVEX_METRIC_PHASE_TOTAL: return "total";
    default: return "unknown";
    }
}

int yvex_metrics_create(yvex_metrics **out, yvex_error *err)
{
    yvex_metrics *metrics;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_create", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }

    metrics = (yvex_metrics *)calloc(1u, sizeof(*metrics));
    if (!metrics) {
        *out = NULL;
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_metrics_create", "failed to allocate metrics");
        return YVEX_ERR_NOMEM;
    }

    *out = metrics;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_metrics_close(yvex_metrics *metrics)
{
    free(metrics);
}

void yvex_metrics_reset(yvex_metrics *metrics)
{
    if (metrics) {
        memset(metrics, 0, sizeof(*metrics));
    }
}

int yvex_metrics_phase_begin(yvex_metrics *metrics,
                             yvex_metric_phase phase,
                             unsigned long long *token,
                             yvex_error *err)
{
    yvex_metric_slot *slot;

    if (!metrics || !token || !phase_valid(phase)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_phase_begin",
                       "metrics, token, and valid phase are required");
        return YVEX_ERR_INVALID_ARG;
    }

    slot = &metrics->phases[(unsigned long)phase];
    if (slot->active_token != 0) {
        yvex_error_setf(err, YVEX_ERR_STATE, "yvex_metrics_phase_begin",
                        "phase %s already active", yvex_metric_phase_name(phase));
        return YVEX_ERR_STATE;
    }
    if (metrics->next_token == ULLONG_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_metrics_phase_begin", "phase token overflow");
        return YVEX_ERR_BOUNDS;
    }

    metrics->next_token += 1u;
    slot->active_token = metrics->next_token;
    slot->active_start_ns = yvex_time_monotonic_ns();
    *token = slot->active_token;

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_metrics_phase_end(yvex_metrics *metrics,
                           yvex_metric_phase phase,
                           unsigned long long token,
                           yvex_error *err)
{
    yvex_metric_slot *slot;
    unsigned long long now;
    unsigned long long elapsed;

    if (!metrics || !phase_valid(phase) || token == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_phase_end",
                       "metrics, valid phase, and token are required");
        return YVEX_ERR_INVALID_ARG;
    }

    slot = &metrics->phases[(unsigned long)phase];
    if (slot->active_token != token) {
        yvex_error_setf(err, YVEX_ERR_STATE, "yvex_metrics_phase_end",
                        "phase %s ended without matching begin token",
                        yvex_metric_phase_name(phase));
        return YVEX_ERR_STATE;
    }

    now = yvex_time_monotonic_ns();
    elapsed = now >= slot->active_start_ns ? now - slot->active_start_ns : 0;
    if (ULLONG_MAX - slot->total_ns < elapsed) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_metrics_phase_end", "phase timing overflow");
        return YVEX_ERR_BOUNDS;
    }

    slot->count += 1u;
    slot->last_ns = elapsed;
    slot->total_ns += elapsed;
    if (slot->min_ns == 0 || elapsed < slot->min_ns) {
        slot->min_ns = elapsed;
    }
    if (elapsed > slot->max_ns) {
        slot->max_ns = elapsed;
    }
    slot->active_token = 0;
    slot->active_start_ns = 0;

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_metrics_add_prompt_tokens(yvex_metrics *metrics,
                                   unsigned long long n,
                                   yvex_error *err)
{
    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_add_prompt_tokens", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return add_ull(&metrics->counters.prompt_tokens, n, err, "yvex_metrics_add_prompt_tokens");
}

int yvex_metrics_add_accepted_tokens(yvex_metrics *metrics,
                                     unsigned long long n,
                                     yvex_error *err)
{
    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_add_accepted_tokens", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return add_ull(&metrics->counters.accepted_tokens, n, err, "yvex_metrics_add_accepted_tokens");
}

int yvex_metrics_add_rejected_tokens(yvex_metrics *metrics,
                                     unsigned long long n,
                                     yvex_error *err)
{
    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_add_rejected_tokens", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return add_ull(&metrics->counters.rejected_tokens, n, err, "yvex_metrics_add_rejected_tokens");
}

int yvex_metrics_add_chat_turn(yvex_metrics *metrics,
                               yvex_error *err)
{
    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_add_chat_turn", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return add_ull(&metrics->counters.chat_turns, 1u, err, "yvex_metrics_add_chat_turn");
}

int yvex_metrics_set_model_bytes(yvex_metrics *metrics,
                                 unsigned long long known_tensor_bytes,
                                 unsigned long long unsupported_tensor_accounting,
                                 yvex_error *err)
{
    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_set_model_bytes", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    metrics->counters.known_tensor_bytes = known_tensor_bytes;
    metrics->counters.unsupported_tensor_accounting = unsupported_tensor_accounting;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_metrics_get_counters(const yvex_metrics *metrics,
                              yvex_metric_counters *out,
                              yvex_error *err)
{
    if (!metrics || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_get_counters",
                       "metrics and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = metrics->counters;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_metrics_get_phase(const yvex_metrics *metrics,
                           yvex_metric_phase phase,
                           yvex_metric_phase_summary *out,
                           yvex_error *err)
{
    const yvex_metric_slot *slot;

    if (!metrics || !out || !phase_valid(phase)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_get_phase",
                       "metrics, out, and valid phase are required");
        return YVEX_ERR_INVALID_ARG;
    }

    slot = &metrics->phases[(unsigned long)phase];
    out->phase = phase;
    out->name = yvex_metric_phase_name(phase);
    out->count = slot->count;
    out->total_ns = slot->total_ns;
    out->last_ns = slot->last_ns;
    out->min_ns = slot->min_ns;
    out->max_ns = slot->max_ns;
    yvex_error_clear(err);
    return YVEX_OK;
}
