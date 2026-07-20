/* Owner: runtime.internal (runtime).
 * Owns: runtime descriptor contracts plus engine-owned artifact, backend, graph, tokenizer, and session state.
 * Does not own: artifact admission, graph policy, or generation claims.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: non-installed runtime descriptor and lifecycle ABI.
 * Purpose: provide the canonical cross-owner runtime descriptor and lifecycle contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_RUNTIME_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_RUNTIME_H_INCLUDED

#include <stdio.h>
#include <yvex/artifact.h>
#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/gguf.h>
#include <yvex/graph.h>
#include <yvex/internal/artifact.h>
#include <yvex/internal/graph.h>
#include <yvex/metrics.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <yvex/runtime.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private contract. */
/*
 * private.h - Private engine/session runtime state.
 *
 * This header is shared only by runtime-owned source files that need the
 * concrete engine shape. It is not a public API and must not become a generic
 * dumping ground for command state. */
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
    yvex_backend *weight_backend;
    yvex_weight_table *weights;
    yvex_materialize_summary weight_summary;
    char reason[YVEX_RUNTIME_REASON_CAP];
};
typedef struct {
    char run_id[YVEX_RUN_ID_CAP];
    char run_dir[YVEX_PATH_CAP];
    char command_path[YVEX_PATH_CAP];
    char metrics_path[YVEX_PATH_CAP];
    char trace_path[YVEX_PATH_CAP];
    char profile_path[YVEX_PATH_CAP];
    int has_run_dir;
    int has_metrics;
    int has_trace;
    int has_profile;
} yvex_run_artifacts;

/* Accepted-only chat runtime lifecycle and its CLI-independent result. */
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
int yvex_chat_runtime_open(yvex_chat_runtime *runtime, const char *model_path,
                           const char *backend_name, unsigned long long context_length,
                           yvex_error *err);
void yvex_chat_runtime_close(yvex_chat_runtime *runtime);
int yvex_chat_runtime_accept_user_text(yvex_chat_runtime *runtime, const char *system_text,
                                       const char *user_text, yvex_chat_accept_result *out,
                                       yvex_error *err);
int yvex_chat_runtime_reset(yvex_chat_runtime *runtime, yvex_error *err);
int yvex_chat_runtime_get_summary(const yvex_chat_runtime *runtime, yvex_session_summary *out,
                                  yvex_error *err);
void yvex_chat_runtime_set_observers(yvex_chat_runtime *runtime, yvex_metrics *metrics,
                                     yvex_trace *trace);
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

/* Bounded diagnostic graph evidence; never part of the installed runtime ABI. */
#define YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES 16u
typedef struct {
    unsigned int token_id;
} yvex_fixture_graph_options;
typedef struct {
    int executed;
    const char *graph_integrity_guard, *graph_execution_phase, *graph_kind;
    const char *shape_status, *range_status, *slice_range_status;
    const char *backend_status, *backend_op_status;
    int dispatch_attempted, reference_read_attempted, output_allocation_attempted;
    int cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned, output_bytes_allocated;
    unsigned long long reference_bytes_planned;
    const char *backend_name, *op_name, *weight_name;
    unsigned int token_id;
    unsigned long long node_count, output_count, output_bytes, output_checksum;
    unsigned long long output_value_count;
    float output_values[YVEX_FIXTURE_GRAPH_MAX_OUTPUT_VALUES];
    int execution_ready, graph_execution_ready;
} yvex_fixture_graph_result;
int yvex_engine_execute_fixture_graph(yvex_engine *engine,
                                      const yvex_fixture_graph_options *options,
                                      yvex_fixture_graph_result *out, yvex_error *err);
int yvex_run_artifacts_prepare(yvex_run_artifacts *out, int save_run, const char *run_dir,
                               const char *metrics_out, const char *trace_out,
                               const char *profile_out, yvex_error *err);
int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts, int arg_count,
                                     char **args, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_RUNTIME_H_INCLUDED */
