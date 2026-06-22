/*
 * YVEX - Chat runtime internals
 *
 * File: src/chat/chat_internal.h
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Defines the private I0 runtime shell object used by yvex run/chat. The
 *   shell opens engine/backend/session objects, accepts prompt tokens, and
 *   reports diagnostics without executing decode or generation.
 *
 * Owns:
 *   - yvex_chat_runtime
 *   - I0 run/chat result summaries
 *   - status-line helper declarations
 *
 * Does not own:
 *   - public API declarations
 *   - prefill/decode execution
 *   - sampler or generated text
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_chat_runtime
 */
#ifndef YVEX_CHAT_INTERNAL_H
#define YVEX_CHAT_INTERNAL_H

#include <stdio.h>

#include <yvex/yvex.h>

typedef struct {
    yvex_engine *engine;
    yvex_backend *backend;
    yvex_session *session;
    char *model_path;
    char *backend_name;
    unsigned long long accepted_turns;
} yvex_chat_runtime;

typedef struct {
    char model_name[128];
    char backend_name[32];
    char session_state[32];
    unsigned long long prompt_tokens;
    unsigned long long accepted_tokens;
    unsigned long long position;
    int execution_ready;
    char generation[32];
    char reason[160];
} yvex_chat_accept_result;

int yvex_chat_runtime_open(yvex_chat_runtime *runtime,
                           const char *model_path,
                           const char *backend_name,
                           unsigned long long context_length,
                           yvex_error *err);
void yvex_chat_runtime_close(yvex_chat_runtime *runtime);

int yvex_chat_runtime_accept_user_text(yvex_chat_runtime *runtime,
                                       const char *system_text,
                                       const char *user_text,
                                       yvex_chat_accept_result *out,
                                       yvex_error *err);
int yvex_chat_runtime_reset(yvex_chat_runtime *runtime, yvex_error *err);
int yvex_chat_runtime_get_summary(const yvex_chat_runtime *runtime,
                                  yvex_session_summary *out,
                                  yvex_error *err);
int yvex_chat_runtime_print_status(FILE *fp,
                                   const yvex_chat_runtime *runtime,
                                   yvex_error *err);

int yvex_run_command_plain(FILE *fp, const yvex_chat_accept_result *result);
int yvex_run_command_json(FILE *fp, const yvex_chat_accept_result *result);
int yvex_status_line_print(FILE *fp,
                           const char *phase,
                           unsigned long long tokens,
                           unsigned long long position);

#endif /* YVEX_CHAT_INTERNAL_H */
