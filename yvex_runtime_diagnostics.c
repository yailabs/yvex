/*
 * YVEX - Runtime diagnostics
 *
 * File: yvex_runtime_diagnostics.c
 * Layer: session implementation
 *
 * Purpose:
 *   Builds compact diagnostic reason strings for engine/session layer engine/session summaries.
 *   Diagnostics describe why a runtime object is inspectable but not
 *   execution-ready.
 *
 * Implements:
 *   - yvex_runtime_strdup
 *   - yvex_runtime_set_graph_reason
 *   - yvex_runtime_set_text_reason
 *
 * Invariants:
 *   - diagnostics do not print from library code
 *   - execution readiness remains false in engine/session layer
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_runtime_diagnostics
 */
#include "yvex_session_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *yvex_runtime_strdup(const char *text)
{
    char *copy;
    size_t len;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text)
{
    if (!out || cap == 0) {
        return;
    }
    snprintf(out, cap, "%s", text ? text : "");
}

void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph)
{
    unsigned long long missing_count;
    int has_output_norm = 0;
    int has_output_head = 0;
    int has_token_embedding = 0;
    unsigned long long i;

    if (!out || cap == 0) {
        return;
    }
    out[0] = '\0';

    if (!graph) {
        snprintf(out, cap, "graph not built");
        return;
    }

    missing_count = yvex_graph_missing_required_count(graph);
    for (i = 0; i < missing_count; ++i) {
        const yvex_graph_missing_required *missing = yvex_graph_missing_required_at(graph, i);
        if (!missing) {
            continue;
        }
        if (missing->role == YVEX_TENSOR_ROLE_TOKEN_EMBEDDING) {
            has_token_embedding = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_NORM) {
            has_output_norm = 1;
        } else if (missing->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD) {
            has_output_head = 1;
        }
    }

    if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_PARTIAL) {
        if (has_output_norm && has_output_head) {
            snprintf(out, cap, "graph partial; missing output_norm, output_head");
        } else if (has_output_norm) {
            snprintf(out, cap, "graph partial; missing output_norm");
        } else if (has_output_head) {
            snprintf(out, cap, "graph partial; missing output_head");
        } else if (missing_count > 0) {
            snprintf(out, cap, "graph partial; missing required tensor roles");
        } else {
            snprintf(out, cap, "graph partial");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_UNSUPPORTED) {
        if (has_token_embedding) {
            snprintf(out, cap, "graph unsupported; missing token_embedding");
        } else {
            snprintf(out, cap, "graph unsupported");
        }
    } else if (yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_BUILT) {
        snprintf(out, cap, "decode runtime not implemented in engine/session layer");
    } else {
        snprintf(out, cap, "graph status: %s", yvex_graph_status_name(yvex_graph_status_of(graph)));
    }
}
