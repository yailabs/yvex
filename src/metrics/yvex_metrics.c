/*
 * yvex_metrics.c - Metrics, traces, profiles, and run artifacts.
 *
 * This file owns runtime measurements and local run artifact helpers used by
 * diagnostic CLI commands.
 */

#include <yvex/metrics.h>
#include <yvex/profile.h>
#include <yvex/trace.h>

#include "yvex_run_private.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int yvex_json_write_string(FILE *fp, const char *text)
{
    const unsigned char *p;

    if (!fp) {
        return YVEX_ERR_INVALID_ARG;
    }

    p = (const unsigned char *)(text ? text : "");
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
    return YVEX_OK;
}




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




static int open_output(FILE **out, const char *path, yvex_error *err, const char *where)
{
    if (!out || !path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }

    *out = fopen(path, "w");
    if (!*out) {
        yvex_error_setf(err, YVEX_ERR_IO, where, "cannot open output file %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static void write_counters(FILE *fp, const yvex_metric_counters *counters)
{
    fprintf(fp, "  \"counters\": {\n");
    fprintf(fp, "    \"prompt_tokens\": %llu,\n", counters->prompt_tokens);
    fprintf(fp, "    \"accepted_tokens\": %llu,\n", counters->accepted_tokens);
    fprintf(fp, "    \"rejected_tokens\": %llu,\n", counters->rejected_tokens);
    fprintf(fp, "    \"chat_turns\": %llu,\n", counters->chat_turns);
    fprintf(fp, "    \"bytes_read\": %llu,\n", counters->bytes_read);
    fprintf(fp, "    \"known_tensor_bytes\": %llu,\n", counters->known_tensor_bytes);
    fprintf(fp, "    \"unsupported_tensor_accounting\": %llu\n", counters->unsupported_tensor_accounting);
    fprintf(fp, "  }");
}

int yvex_metrics_write_json(const char *path,
                            const yvex_metrics *metrics,
                            yvex_error *err)
{
    FILE *fp;
    yvex_metric_counters counters;
    yvex_metric_phase_summary phase;
    int rc;
    unsigned long i;
    int first_phase = 1;

    if (!metrics) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_metrics_write_json", "metrics is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = open_output(&fp, path, err, "yvex_metrics_write_json");
    if (rc != YVEX_OK) {
        return rc;
    }

    rc = yvex_metrics_get_counters(metrics, &counters, err);
    if (rc != YVEX_OK) {
        fclose(fp);
        return rc;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.metrics.v1\",\n");
    fprintf(fp, "  \"status\": \"accepted-only\",\n");
    write_counters(fp, &counters);
    fprintf(fp, ",\n");
    fprintf(fp, "  \"phases\": [\n");
    for (i = 0; i <= (unsigned long)YVEX_METRIC_PHASE_TOTAL; ++i) {
        rc = yvex_metrics_get_phase(metrics, (yvex_metric_phase)i, &phase, err);
        if (rc != YVEX_OK) {
            fclose(fp);
            return rc;
        }
        if (phase.count == 0) {
            continue;
        }
        if (!first_phase) {
            fprintf(fp, ",\n");
        }
        first_phase = 0;
        fprintf(fp, "    {\"name\": ");
        yvex_json_write_string(fp, phase.name);
        fprintf(fp, ", \"count\": %llu, \"total_ns\": %llu, \"last_ns\": %llu, \"min_ns\": %llu, \"max_ns\": %llu}",
                phase.count, phase.total_ns, phase.last_ns, phase.min_ns, phase.max_ns);
    }
    fprintf(fp, "\n  ]\n");
    fprintf(fp, "}\n");

    fclose(fp);
    yvex_error_clear(err);
    return YVEX_OK;
}

#define _POSIX_C_SOURCE 200809L



static int path_format(char *dst, size_t cap, yvex_error *err, const char *where, const char *fmt, ...)
{
    va_list ap;
    int n;

    if (!dst || cap == 0 || !fmt) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "invalid path format argument");
        return YVEX_ERR_INVALID_ARG;
    }
    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap) {
        dst[cap - 1] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int mkdir_one(const char *path, yvex_error *err)
{
    struct stat st;

    if (mkdir(path, 0777) == 0) {
        return YVEX_OK;
    }
    if (errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_prepare",
                        "mkdir failed for %s: %s", path, strerror(errno));
        return YVEX_ERR_IO;
    }
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_prepare",
                        "path exists but is not a directory: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static int mkdir_p(const char *path, yvex_error *err)
{
    char tmp[YVEX_PATH_CAP];
    char *p;
    size_t len;
    int rc;

    if (!path || path[0] == '\0') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_artifacts_prepare",
                       "directory path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = path_format(tmp, sizeof(tmp), err, "yvex_run_artifacts_prepare", "%s", path);
    if (rc != YVEX_OK) {
        return rc;
    }
    len = strlen(tmp);
    while (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
        --len;
    }
    for (p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            rc = mkdir_one(tmp, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            *p = '/';
        }
    }
    return mkdir_one(tmp, err);
}

static int copy_optional_path(char *dst, const char *src, yvex_error *err, const char *where)
{
    if (!src || src[0] == '\0') {
        dst[0] = '\0';
        return YVEX_OK;
    }
    return path_format(dst, YVEX_PATH_CAP, err, where, "%s", src);
}

int yvex_run_artifacts_prepare(yvex_run_artifacts *out,
                               int save_run,
                               const char *run_dir,
                               const char *metrics_out,
                               const char *trace_out,
                               const char *profile_out,
                               yvex_error *err)
{
    yvex_paths paths;
    yvex_run_dir prepared;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_run_artifacts_prepare", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));

    rc = yvex_run_id_make(out->run_id, sizeof(out->run_id), err);
    if (rc != YVEX_OK) {
        return rc;
    }

    if (save_run) {
        if (run_dir && run_dir[0] != '\0') {
            rc = path_format(out->run_dir, sizeof(out->run_dir), err,
                             "yvex_run_artifacts_prepare", "%s", run_dir);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = mkdir_p(out->run_dir, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = path_format(out->command_path, sizeof(out->command_path), err,
                             "yvex_run_artifacts_prepare", "%s/command.txt", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->metrics_path, sizeof(out->metrics_path), err,
                             "yvex_run_artifacts_prepare", "%s/metrics.json", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->trace_path, sizeof(out->trace_path), err,
                             "yvex_run_artifacts_prepare", "%s/trace.jsonl", out->run_dir);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->profile_path, sizeof(out->profile_path), err,
                             "yvex_run_artifacts_prepare", "%s/profile.json", out->run_dir);
            if (rc != YVEX_OK) return rc;
        } else {
            rc = yvex_paths_default(&paths, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = yvex_run_dir_prepare(&prepared, &paths, out->run_id, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = yvex_run_dir_create(&prepared, err);
            if (rc != YVEX_OK) {
                return rc;
            }
            rc = path_format(out->run_dir, sizeof(out->run_dir), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.root);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->command_path, sizeof(out->command_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.command_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->metrics_path, sizeof(out->metrics_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.metrics_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->trace_path, sizeof(out->trace_path), err,
                             "yvex_run_artifacts_prepare", "%s", prepared.trace_path);
            if (rc != YVEX_OK) return rc;
            rc = path_format(out->profile_path, sizeof(out->profile_path), err,
                             "yvex_run_artifacts_prepare", "%s/profile.json", prepared.root);
            if (rc != YVEX_OK) return rc;
        }
        out->has_run_dir = 1;
        out->has_metrics = 1;
        out->has_trace = 1;
        out->has_profile = 1;
    }

    if (metrics_out && metrics_out[0] != '\0') {
        rc = copy_optional_path(out->metrics_path, metrics_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_metrics = 1;
    }
    if (trace_out && trace_out[0] != '\0') {
        rc = copy_optional_path(out->trace_path, trace_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_trace = 1;
    }
    if (profile_out && profile_out[0] != '\0') {
        rc = copy_optional_path(out->profile_path, profile_out, err, "yvex_run_artifacts_prepare");
        if (rc != YVEX_OK) return rc;
        out->has_profile = 1;
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts,
                                     int arg_count,
                                     char **args,
                                     yvex_error *err)
{
    FILE *fp;
    int i;

    if (!artifacts || !artifacts->has_run_dir || artifacts->command_path[0] == '\0') {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    fp = fopen(artifacts->command_path, "w");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "yvex_run_artifacts_write_command",
                        "cannot open command file %s", artifacts->command_path);
        return YVEX_ERR_IO;
    }
    for (i = 0; i < arg_count; ++i) {
        if (i > 0) {
            fputc(' ', fp);
        }
        fputs(args[i] ? args[i] : "", fp);
    }
    fputc('\n', fp);
    fclose(fp);

    yvex_error_clear(err);
    return YVEX_OK;
}

#define _POSIX_C_SOURCE 200809L



unsigned long long yvex_time_monotonic_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }

    return ((unsigned long long)ts.tv_sec * 1000000000ull) + (unsigned long long)ts.tv_nsec;
}




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
