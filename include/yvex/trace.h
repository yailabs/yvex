/*
 * YVEX - Runtime trace
 *
 * File: include/yvex/trace.h
 * Layer: public runtime observability API
 *
 * Purpose:
 *   Defines the observability layer JSONL trace writer for implemented runtime shell events.
 *   Trace events report runtime lifecycle and accepted-token phases only; they
 *   do not claim inference, decode, sampling, or CUDA execution.
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_trace
 */
#ifndef YVEX_TRACE_H
#define YVEX_TRACE_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#endif /* YVEX_TRACE_H */
