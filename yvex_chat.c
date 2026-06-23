#include "yvex_internal.h"

#include <stdlib.h>
#include <string.h>

static char *chat_strdup(const char *text)
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

static void copy_text(char *dst, unsigned long cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, (size_t)cap, "%s", src ? src : "");
}

static int open_backend(yvex_backend **out, const char *backend_name, yvex_error *err)
{
    yvex_backend_options options;

    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        return yvex_backend_open_cpu(out, err);
    }
    if (strcmp(backend_name, "cuda") == 0) {
        memset(&options, 0, sizeof(options));
        options.kind = YVEX_BACKEND_KIND_CUDA;
        return yvex_backend_open(out, &options, err);
    }

    memset(&options, 0, sizeof(options));
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_chat_backend",
                    "unknown backend: %s", backend_name);
    return YVEX_ERR_INVALID_ARG;
}

static int runtime_phase_begin(yvex_chat_runtime *runtime,
                               yvex_metric_phase phase,
                               unsigned long long *token,
                               yvex_error *err)
{
    if (!token) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "runtime_phase_begin", "token is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *token = 0;
    if (runtime && runtime->trace) {
        (void)yvex_trace_emit(runtime->trace, YVEX_TRACE_EVENT_PHASE_START,
                              yvex_metric_phase_name(phase), "started", "", err);
    }
    if (runtime && runtime->metrics) {
        return yvex_metrics_phase_begin(runtime->metrics, phase, token, err);
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int runtime_phase_end(yvex_chat_runtime *runtime,
                             yvex_metric_phase phase,
                             unsigned long long token,
                             int phase_status,
                             yvex_error *err)
{
    int rc = YVEX_OK;
    yvex_error trace_err;

    if (runtime && runtime->metrics && token != 0) {
        rc = yvex_metrics_phase_end(runtime->metrics, phase, token, err);
    }
    if (runtime && runtime->trace) {
        yvex_error_clear(&trace_err);
        (void)yvex_trace_emit(runtime->trace, YVEX_TRACE_EVENT_PHASE_END,
                              yvex_metric_phase_name(phase),
                              phase_status == YVEX_OK ? "ok" : "failed",
                              "", &trace_err);
    }
    return rc;
}

int yvex_chat_runtime_open(yvex_chat_runtime *runtime,
                           const char *model_path,
                           const char *backend_name,
                           unsigned long long context_length,
                           yvex_error *err)
{
    yvex_session_options session_options;
    int rc;

    if (!runtime || !model_path || !backend_name) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_chat_runtime_open",
                       "runtime, model_path, and backend_name are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->model_path = chat_strdup(model_path);
    runtime->backend_name = chat_strdup(backend_name);
    if (!runtime->model_path || !runtime->backend_name) {
        yvex_chat_runtime_close(runtime);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_chat_runtime_open",
                       "failed to copy runtime labels");
        return YVEX_ERR_NOMEM;
    }

    rc = yvex_engine_open_path(&runtime->engine, model_path, err);
    if (rc == YVEX_OK) {
        rc = open_backend(&runtime->backend, backend_name, err);
    }
    if (rc == YVEX_OK) {
        memset(&session_options, 0, sizeof(session_options));
        session_options.context_length = context_length;
        session_options.allow_partial_graph = 1;
        rc = yvex_session_create(&runtime->session, runtime->engine, runtime->backend,
                                 &session_options, err);
    }
    if (rc != YVEX_OK) {
        yvex_chat_runtime_close(runtime);
        return rc;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_chat_runtime_close(yvex_chat_runtime *runtime)
{
    if (!runtime) {
        return;
    }
    yvex_session_close(runtime->session);
    yvex_backend_close(runtime->backend);
    yvex_engine_close(runtime->engine);
    free(runtime->model_path);
    free(runtime->backend_name);
    memset(runtime, 0, sizeof(*runtime));
}

void yvex_chat_runtime_set_observers(yvex_chat_runtime *runtime,
                                     yvex_metrics *metrics,
                                     yvex_trace *trace)
{
    if (!runtime) {
        return;
    }
    runtime->metrics = metrics;
    runtime->trace = trace;
}

int yvex_chat_runtime_accept_user_text(yvex_chat_runtime *runtime,
                                       const char *system_text,
                                       const char *user_text,
                                       yvex_chat_accept_result *out,
                                       yvex_error *err)
{
    yvex_tokens tokens;
    yvex_session_summary summary;
    yvex_engine_summary engine_summary;
    unsigned long long phase_token = 0;
    int rc;
    int phase_rc;

    if (!runtime || !runtime->session || !user_text || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_chat_runtime_accept_user_text",
                       "runtime, user_text, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    memset(&tokens, 0, sizeof(tokens));

    (void)system_text;
    rc = runtime_phase_begin(runtime, YVEX_METRIC_PHASE_TOKENIZE, &phase_token, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_tokenize_text(yvex_engine_tokenizer(runtime->engine), user_text, &tokens, err);
    phase_rc = runtime_phase_end(runtime, YVEX_METRIC_PHASE_TOKENIZE, phase_token, rc, err);
    if (rc == YVEX_OK && phase_rc != YVEX_OK) {
        rc = phase_rc;
    }
    if (rc == YVEX_OK) {
        rc = runtime_phase_begin(runtime, YVEX_METRIC_PHASE_ACCEPT_TOKENS, &phase_token, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_session_accept_tokens(runtime->session, &tokens, err);
        phase_rc = runtime_phase_end(runtime, YVEX_METRIC_PHASE_ACCEPT_TOKENS, phase_token, rc, err);
        if (rc == YVEX_OK && phase_rc != YVEX_OK) {
            rc = phase_rc;
        }
    }
    if (rc == YVEX_OK) {
        rc = yvex_session_get_summary(runtime->session, &summary, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(runtime->engine, &engine_summary, err);
    }
    if (rc != YVEX_OK) {
        yvex_tokens_free(&tokens);
        return rc;
    }

    copy_text(out->model_name, sizeof(out->model_name), engine_summary.model_name);
    copy_text(out->backend_name, sizeof(out->backend_name), summary.backend_kind);
    copy_text(out->session_state, sizeof(out->session_state),
              yvex_session_state_name(summary.state));
    out->prompt_tokens = tokens.len;
    out->accepted_tokens = summary.accepted_tokens;
    out->position = summary.position;
    out->execution_ready = 0;
    copy_text(out->generation, sizeof(out->generation), "unsupported");
    copy_text(out->reason, sizeof(out->reason), "decode runtime is not implemented in diagnostic runtime");
    runtime->accepted_turns += 1;

    if (runtime->metrics) {
        rc = yvex_metrics_add_prompt_tokens(runtime->metrics, tokens.len, err);
        if (rc == YVEX_OK) {
            rc = yvex_metrics_add_accepted_tokens(runtime->metrics, tokens.len, err);
        }
        if (rc != YVEX_OK) {
            yvex_tokens_free(&tokens);
            return rc;
        }
    }
    if (runtime->trace) {
        (void)yvex_trace_emit(runtime->trace, YVEX_TRACE_EVENT_ACCEPT_TOKENS,
                              "accept_tokens", "ok", "prompt tokens accepted", err);
    }

    yvex_tokens_free(&tokens);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_chat_runtime_reset(yvex_chat_runtime *runtime, yvex_error *err)
{
    if (!runtime || !runtime->session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_chat_runtime_reset",
                       "runtime session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    runtime->accepted_turns = 0;
    return yvex_session_reset(runtime->session, err);
}

int yvex_chat_runtime_get_summary(const yvex_chat_runtime *runtime,
                                  yvex_session_summary *out,
                                  yvex_error *err)
{
    if (!runtime || !runtime->session) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_chat_runtime_get_summary",
                       "runtime session is required");
        return YVEX_ERR_INVALID_ARG;
    }
    return yvex_session_get_summary(runtime->session, out, err);
}

#include "yvex_internal.h"

int yvex_chat_runtime_print_status(FILE *fp,
                                   const yvex_chat_runtime *runtime,
                                   yvex_error *err)
{
    yvex_session_summary summary;
    int rc;

    if (!fp || !runtime) {
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_chat_runtime_get_summary(runtime, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    fprintf(fp, "session_state: %s\n", yvex_session_state_name(summary.state));
    fprintf(fp, "position: %llu\n", summary.position);
    fprintf(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
    fprintf(fp, "execution_ready: false\n");
    fprintf(fp, "generation: unsupported\n");
    return YVEX_OK;
}

#include "yvex_internal.h"

#include <string.h>

static void json_print_escaped(FILE *fp, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    fputc('"', fp);
    while (*p) {
        if (*p == '"' || *p == '\\') {
            fputc('\\', fp);
            fputc((int)*p, fp);
        } else if (*p == '\n') {
            fputs("\\n", fp);
        } else if (*p == '\r') {
            fputs("\\r", fp);
        } else if (*p == '\t') {
            fputs("\\t", fp);
        } else if (*p < 32u) {
            fprintf(fp, "\\u%04x", (unsigned int)*p);
        } else {
            fputc((int)*p, fp);
        }
        ++p;
    }
    fputc('"', fp);
}

int yvex_run_command_plain(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) {
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "run status: accepted-only\n");
    fprintf(fp, "model: %s\n", result->model_name);
    fprintf(fp, "backend: %s\n", result->backend_name);
    fprintf(fp, "session_state: %s\n", result->session_state);
    fprintf(fp, "prompt_tokens: %llu\n", result->prompt_tokens);
    fprintf(fp, "accepted_tokens: %llu\n", result->accepted_tokens);
    fprintf(fp, "position: %llu\n", result->position);
    fprintf(fp, "execution_ready: false\n");
    fprintf(fp, "generation: %s\n", result->generation);
    fprintf(fp, "reason: %s\n", result->reason);
    if (result->run_id[0] != '\0') {
        fprintf(fp, "run_id: %s\n", result->run_id);
    }
    if (result->run_dir[0] != '\0') {
        fprintf(fp, "run_dir: %s\n", result->run_dir);
    }
    if (result->metrics_out[0] != '\0') {
        fprintf(fp, "metrics_out: %s\n", result->metrics_out);
    }
    if (result->trace_out[0] != '\0') {
        fprintf(fp, "trace_out: %s\n", result->trace_out);
    }
    if (result->profile_out[0] != '\0') {
        fprintf(fp, "profile_out: %s\n", result->profile_out);
    }
    return YVEX_OK;
}

int yvex_run_command_json(FILE *fp, const yvex_chat_accept_result *result)
{
    if (!fp || !result) {
        return YVEX_ERR_INVALID_ARG;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.cli.result.v1\",\n");
    fprintf(fp, "  \"command\": \"run\",\n");
    fprintf(fp, "  \"status\": \"accepted-only\",\n");
    fprintf(fp, "  \"data\": {\n");
    fprintf(fp, "    \"model\": ");
    json_print_escaped(fp, result->model_name);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"backend\": ");
    json_print_escaped(fp, result->backend_name);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"session_state\": ");
    json_print_escaped(fp, result->session_state);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    fprintf(fp, "    \"accepted_tokens\": %llu,\n", result->accepted_tokens);
    fprintf(fp, "    \"position\": %llu,\n", result->position);
    fprintf(fp, "    \"execution_ready\": false,\n");
    fprintf(fp, "    \"generation\": ");
    json_print_escaped(fp, result->generation);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"reason\": ");
    json_print_escaped(fp, result->reason);
    fprintf(fp, ",\n");
    fprintf(fp, "    \"metrics\": {\n");
    fprintf(fp, "      \"prompt_tokens\": %llu,\n", result->prompt_tokens);
    fprintf(fp, "      \"accepted_tokens\": %llu\n", result->accepted_tokens);
    fprintf(fp, "    }");
    if (result->metrics_out[0] != '\0' || result->trace_out[0] != '\0' ||
        result->profile_out[0] != '\0' || result->run_dir[0] != '\0') {
        fprintf(fp, ",\n");
        fprintf(fp, "    \"artifacts\": {\n");
        fprintf(fp, "      \"run_id\": ");
        json_print_escaped(fp, result->run_id);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"run_dir\": ");
        json_print_escaped(fp, result->run_dir);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"metrics_out\": ");
        json_print_escaped(fp, result->metrics_out);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"trace_out\": ");
        json_print_escaped(fp, result->trace_out);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"profile_out\": ");
        json_print_escaped(fp, result->profile_out);
        fprintf(fp, "\n");
        fprintf(fp, "    }");
    }
    fprintf(fp, "\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"error\": null\n");
    fprintf(fp, "}\n");
    return YVEX_OK;
}

#include "yvex_internal.h"

#include <string.h>

static int slash_eq(const char *line, const char *command)
{
    size_t len = strlen(command);

    return strncmp(line, command, len) == 0 &&
           (line[len] == '\0' || line[len] == '\n' || line[len] == '\r' ||
            line[len] == ' ' || line[len] == '\t');
}

yvex_slash_command yvex_slash_parse(const char *line)
{
    if (!line || line[0] != '/') {
        return YVEX_SLASH_NOT_COMMAND;
    }
    if (slash_eq(line, "/help")) return YVEX_SLASH_HELP;
    if (slash_eq(line, "/status")) return YVEX_SLASH_STATUS;
    if (slash_eq(line, "/model")) return YVEX_SLASH_MODEL;
    if (slash_eq(line, "/backend")) return YVEX_SLASH_BACKEND;
    if (slash_eq(line, "/tokens")) return YVEX_SLASH_TOKENS;
    if (slash_eq(line, "/reset")) return YVEX_SLASH_RESET;
    if (slash_eq(line, "/quit")) return YVEX_SLASH_QUIT;
    return YVEX_SLASH_UNKNOWN;
}

const char *yvex_slash_command_name(yvex_slash_command command)
{
    switch (command) {
    case YVEX_SLASH_NOT_COMMAND: return "not-command";
    case YVEX_SLASH_HELP: return "help";
    case YVEX_SLASH_STATUS: return "status";
    case YVEX_SLASH_MODEL: return "model";
    case YVEX_SLASH_BACKEND: return "backend";
    case YVEX_SLASH_TOKENS: return "tokens";
    case YVEX_SLASH_RESET: return "reset";
    case YVEX_SLASH_QUIT: return "quit";
    case YVEX_SLASH_UNKNOWN: return "unknown";
    }
    return "unknown";
}

#include "yvex_internal.h"

int yvex_status_line_print(FILE *fp,
                           const char *phase,
                           unsigned long long tokens,
                           unsigned long long position)
{
    if (!fp || !phase) {
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "[%s] tokens=%llu position=%llu\n", phase, tokens, position);
    return YVEX_OK;
}
