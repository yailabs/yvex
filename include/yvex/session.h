/*
 * YVEX - Session runtime skeleton
 *
 * File: include/yvex/session.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the engine/session layer session object and state machine. Sessions bind an engine
 *   to a backend and expose lifecycle/token-acceptance diagnostics; they do
 *   not execute prefill, decode, sampling, or generation in engine/session layer.
 *
 * Owns:
 *   - yvex_session
 *   - session state vocabulary
 *   - token acceptance counters
 *   - session summaries and diagnostics
 *
 * Does not own:
 *   - engine/backend object lifetime
 *   - graph executor loop
 *   - sampler or generated output
 *   - server/provider behavior
 *
 * Used by:
 *   - yvex session
 *   - engine/session layer runtime tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_session
 */
#ifndef YVEX_SESSION_H
#define YVEX_SESSION_H

#include <yvex/backend.h>
#include <yvex/engine.h>
#include <yvex/kv.h>
#include <yvex/logits.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_session yvex_session;

typedef enum {
    YVEX_SESSION_STATE_CREATED = 0,
    YVEX_SESSION_STATE_READY,
    YVEX_SESSION_STATE_PREFILLING,
    YVEX_SESSION_STATE_DECODING,
    YVEX_SESSION_STATE_PARTIAL,
    YVEX_SESSION_STATE_CANCELLED,
    YVEX_SESSION_STATE_FAILED,
    YVEX_SESSION_STATE_CLOSED
} yvex_session_state;

typedef struct {
    unsigned long long context_length;
    unsigned long long max_tokens;
    int allow_partial_graph;
} yvex_session_options;

typedef struct {
    yvex_session_state state;
    const char *engine_status;
    const char *backend_kind;
    const char *backend_status;
    unsigned long long context_length;
    unsigned long long position;
    unsigned long long accepted_tokens;
    unsigned long long rejected_tokens;
    const char *kv_status;
    unsigned long long kv_bytes;
    const char *logits_status;
    unsigned long long logits_capacity;
    int graph_partial;
    int weights_attached;
    const char *weights_backend;
    unsigned long long weight_tensor_count;
    unsigned long long weight_total_bytes;
    int backend_available;
    int execution_ready;
    int graph_execution_ready;
} yvex_session_summary;

int yvex_session_create(yvex_session **out,
                        const yvex_engine *engine,
                        yvex_backend *backend,
                        const yvex_session_options *options,
                        yvex_error *err);
void yvex_session_close(yvex_session *session);

yvex_session_state yvex_session_state_of(const yvex_session *session);
const char *yvex_session_state_name(yvex_session_state state);

unsigned long long yvex_session_position(const yvex_session *session);
unsigned long long yvex_session_context_length(const yvex_session *session);

int yvex_session_get_summary(const yvex_session *session,
                             yvex_session_summary *out,
                             yvex_error *err);
const char *yvex_session_diagnostic_reason(const yvex_session *session);

int yvex_session_accept_tokens(yvex_session *session,
                               const yvex_tokens *tokens,
                               yvex_error *err);
int yvex_session_prefill(yvex_session *session,
                         const yvex_tokens *tokens,
                         yvex_error *err);
int yvex_session_decode_next(yvex_session *session,
                             unsigned int *out_token,
                             yvex_error *err);
int yvex_session_cancel(yvex_session *session,
                        yvex_error *err);
int yvex_session_reset(yvex_session *session,
                       yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_SESSION_H */
