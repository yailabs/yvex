/*
 * YVEX - Session internals
 *
 * File: yvex_session_internal.h
 * Layer: session implementation
 *
 * Purpose:
 *   Shares private engine/session layer engine/session/KV/logits structures across runtime
 *   implementation files. Public consumers see only opaque handles.
 *
 * Owns:
 *   - concrete yvex_engine storage
 *   - concrete yvex_session storage
 *   - concrete KV/logits skeleton storage
 *
 * Does not own:
 *   - public API declarations
 *   - backend implementation
 *   - generation/runtime execution
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_engine
 *   - build/tests/test_session
 */
#ifndef YVEX_SESSION_INTERNAL_H
#define YVEX_SESSION_INTERNAL_H

#include <stddef.h>

#include <yvex/yvex.h>

#define YVEX_RUNTIME_REASON_CAP 256u

struct yvex_engine {
    char *model_path;
    yvex_engine_status status;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_graph *graph;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

struct yvex_kv_cache {
    yvex_kv_summary summary;
};

struct yvex_logits {
    yvex_logits_summary summary;
};

struct yvex_session {
    const yvex_engine *engine;
    yvex_backend *backend;
    yvex_session_state state;
    unsigned long long context_length;
    unsigned long long max_tokens;
    unsigned long long position;
    unsigned long long accepted_tokens;
    unsigned long long rejected_tokens;
    int graph_partial;
    int backend_available;
    yvex_kv_cache *kv;
    yvex_logits *logits;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

char *yvex_runtime_strdup(const char *text);
void yvex_runtime_set_graph_reason(char *out, size_t cap, const yvex_graph *graph);
void yvex_runtime_set_text_reason(char *out, size_t cap, const char *text);

#endif /* YVEX_SESSION_INTERNAL_H */
