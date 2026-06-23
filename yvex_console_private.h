/*
 * yvex_console_private.h - Diagnostic console boundary.
 *
 * This header is private to the CLI and chat runtime.
 */
#ifndef YVEX_CONSOLE_PRIVATE_H
#define YVEX_CONSOLE_PRIVATE_H

#include <stdio.h>

#include <yvex/yvex.h>

typedef struct {
    yvex_engine *engine;
    yvex_backend *backend;
    yvex_session *session;
    yvex_metrics *metrics;
    yvex_trace *trace;
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
    char run_id[YVEX_RUN_ID_CAP];
    char run_dir[YVEX_PATH_CAP];
    char metrics_out[YVEX_PATH_CAP];
    char trace_out[YVEX_PATH_CAP];
    char profile_out[YVEX_PATH_CAP];
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
void yvex_chat_runtime_set_observers(yvex_chat_runtime *runtime,
                                     yvex_metrics *metrics,
                                     yvex_trace *trace);
int yvex_chat_runtime_print_status(FILE *fp,
                                   const yvex_chat_runtime *runtime,
                                   yvex_error *err);
int yvex_run_command_plain(FILE *fp, const yvex_chat_accept_result *result);
int yvex_run_command_json(FILE *fp, const yvex_chat_accept_result *result);
int yvex_status_line_print(FILE *fp,
                           const char *phase,
                           unsigned long long tokens,
                           unsigned long long position);

typedef enum {
    YVEX_SLASH_NOT_COMMAND = 0,
    YVEX_SLASH_HELP,
    YVEX_SLASH_STATUS,
    YVEX_SLASH_MODEL,
    YVEX_SLASH_BACKEND,
    YVEX_SLASH_TOKENS,
    YVEX_SLASH_RESET,
    YVEX_SLASH_QUIT,
    YVEX_SLASH_UNKNOWN
} yvex_slash_command;

yvex_slash_command yvex_slash_parse(const char *line);
const char *yvex_slash_command_name(yvex_slash_command command);

#endif /* YVEX_CONSOLE_PRIVATE_H */
