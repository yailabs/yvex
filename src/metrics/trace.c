/*
 * YVEX - Runtime trace JSONL writer
 *
 * File: src/metrics/trace.c
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Writes observability layer trace events as one JSON object per line for implemented
 *   runtime shell paths.
 */
#include <yvex/trace.h>

#include "metrics_internal.h"

#include <stdlib.h>
#include <string.h>

struct yvex_trace {
    FILE *fp;
    char run_id[YVEX_RUN_ID_CAP];
    int enabled;
    unsigned long long seq;
};

const char *yvex_trace_event_kind_name(yvex_trace_event_kind kind)
{
    switch (kind) {
    case YVEX_TRACE_EVENT_RUN_START: return "run_start";
    case YVEX_TRACE_EVENT_RUN_END: return "run_end";
    case YVEX_TRACE_EVENT_PHASE_START: return "phase_start";
    case YVEX_TRACE_EVENT_PHASE_END: return "phase_end";
    case YVEX_TRACE_EVENT_ENGINE: return "engine";
    case YVEX_TRACE_EVENT_BACKEND: return "backend";
    case YVEX_TRACE_EVENT_SESSION: return "session";
    case YVEX_TRACE_EVENT_PROMPT: return "prompt";
    case YVEX_TRACE_EVENT_TOKENIZE: return "tokenize";
    case YVEX_TRACE_EVENT_ACCEPT_TOKENS: return "accept_tokens";
    case YVEX_TRACE_EVENT_CHAT_TURN: return "chat_turn";
    case YVEX_TRACE_EVENT_ERROR: return "error";
    default: return "unknown";
    }
}

int yvex_trace_open(yvex_trace **out,
                    const yvex_trace_options *options,
                    yvex_error *err)
{
    yvex_trace *trace;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_trace_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }

    trace = (yvex_trace *)calloc(1u, sizeof(*trace));
    if (!trace) {
        *out = NULL;
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_trace_open", "failed to allocate trace");
        return YVEX_ERR_NOMEM;
    }

    if (!options || !options->enabled) {
        *out = trace;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!options->path || options->path[0] == '\0') {
        free(trace);
        *out = NULL;
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_trace_open",
                       "enabled trace requires a path");
        return YVEX_ERR_INVALID_ARG;
    }

    trace->fp = fopen(options->path, "w");
    if (!trace->fp) {
        free(trace);
        *out = NULL;
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_trace_open",
                        "cannot open trace file %s", options->path);
        return YVEX_ERR_IO;
    }
    snprintf(trace->run_id, sizeof(trace->run_id), "%s", options->run_id ? options->run_id : "");
    trace->enabled = 1;

    *out = trace;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_trace_close(yvex_trace *trace)
{
    if (!trace) {
        return;
    }
    if (trace->fp) {
        fflush(trace->fp);
        fclose(trace->fp);
    }
    free(trace);
}

int yvex_trace_emit(yvex_trace *trace,
                    yvex_trace_event_kind kind,
                    const char *name,
                    const char *status,
                    const char *message,
                    yvex_error *err)
{
    if (!trace || !trace->enabled) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!trace->fp) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_trace_emit", "trace file is not open");
        return YVEX_ERR_STATE;
    }

    trace->seq += 1u;
    fprintf(trace->fp, "{");
    fprintf(trace->fp, "\"schema\": \"yvex.trace.v1\", ");
    fprintf(trace->fp, "\"run_id\": ");
    yvex_json_write_string(trace->fp, trace->run_id);
    fprintf(trace->fp, ", \"seq\": %llu, ", trace->seq);
    fprintf(trace->fp, "\"event\": ");
    yvex_json_write_string(trace->fp, yvex_trace_event_kind_name(kind));
    fprintf(trace->fp, ", \"name\": ");
    yvex_json_write_string(trace->fp, name ? name : "");
    fprintf(trace->fp, ", \"status\": ");
    yvex_json_write_string(trace->fp, status ? status : "");
    fprintf(trace->fp, ", \"message\": ");
    yvex_json_write_string(trace->fp, message ? message : "");
    fprintf(trace->fp, ", \"ts_ns\": %llu", yvex_time_monotonic_ns());
    fprintf(trace->fp, "}\n");
    fflush(trace->fp);

    yvex_error_clear(err);
    return YVEX_OK;
}
