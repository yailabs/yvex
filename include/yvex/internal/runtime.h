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
#include <yvex/metrics.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <yvex/runtime.h>
#include <yvex/tokenizer.h>
#include <yvex/internal/artifact.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Immutable admitted model-to-runtime descriptor projection. */
#define YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP 65u
#define YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP 43u
typedef struct {
    const char *status;
    const char *artifact_status;
    const char *reason;
    const char *next_row;
} yvex_runtime_descriptor_fact;
typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_STATUS_REFUSED = 0,
    YVEX_RUNTIME_DESCRIPTOR_STATUS_READY
} yvex_runtime_descriptor_status;
typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_NONE = 0,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ADMISSION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_DUPLICATE_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_QTYPE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION
} yvex_runtime_descriptor_failure_code;
typedef struct yvex_runtime_descriptor_failure {
    yvex_runtime_descriptor_failure_code code;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_runtime_descriptor_failure;
typedef struct {
    unsigned long long tensor_id;
    unsigned long long descriptor_index;
    const yvex_materialized_tensor_binding *binding;
    yvex_tensor_role role;
    yvex_tensor_collection collection;
    yvex_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned int qtype;
    yvex_materialization_placement placement;
    yvex_materialization_access_mode access_mode;
} yvex_runtime_tensor_binding;
typedef struct {
    yvex_runtime_descriptor_status status;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char materialization_plan_identity[YVEX_MATERIALIZATION_IDENTITY_CAP];
    char logical_model_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_descriptor_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_numeric_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    char runtime_hadamard_revision[128];
    unsigned int runtime_numeric_schema_version;
    unsigned long long runtime_activation_policy_count;
    unsigned long long runtime_sparse_topk_policy_count;
    unsigned long long tensor_count;
    unsigned long long payload_bytes;
    unsigned long long qtype_tensor_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long qtype_bytes[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long role_counts[YVEX_TENSOR_ROLE_COUNT];
    unsigned long long global_bindings;
    unsigned long long main_layer_bindings;
    unsigned long long mtp_bindings;
    unsigned long long routed_expert_bindings;
    unsigned long long expert_subview_count;
    unsigned long long missing_required_bindings;
    unsigned long long duplicate_bindings;
    unsigned long long unexpected_bindings;
    unsigned long long layer_count;
    unsigned long long mtp_layer_count;
    unsigned long long routed_experts;
    unsigned long long experts_per_token;
    unsigned long long vocabulary_size;
    int tokenizer_metadata_available;
    int graph_execution_ready;
    int generation_ready;
} yvex_runtime_descriptor_summary;
typedef struct yvex_runtime_descriptor yvex_runtime_descriptor;
const char *yvex_runtime_descriptor_status_name(yvex_runtime_descriptor_status status);
const char *yvex_runtime_descriptor_failure_name(
    yvex_runtime_descriptor_failure_code code);
int yvex_runtime_descriptor_build(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_session *session,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err);
void yvex_runtime_descriptor_close(yvex_runtime_descriptor *descriptor);
const yvex_runtime_descriptor_summary *yvex_runtime_descriptor_summary_get(
    const yvex_runtime_descriptor *descriptor);
const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_name(
    const yvex_runtime_descriptor *descriptor,
    const char *name);
const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    yvex_tensor_scope scope,
    unsigned long long layer_index,
    unsigned long long predictor_index);

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

/* Bounded runtime graph admission shared by graph and generation. */
typedef struct {
    const char *guard_status;
    const char *phase;
    const char *graph_kind;
    const char *integrity_status;
    const char *identity_status;
    const char *metadata_status;
    const char *shape_status;
    const char *range_status;
    const char *slice_range_status;
    const char *backend_status;
    const char *backend_op_status;
    int dispatch_attempted;
    int reference_read_attempted;
    int output_allocation_attempted;
    int cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned;
    unsigned long long output_bytes_allocated;
    unsigned long long reference_bytes_planned;
} yvex_cli_graph_guard_report;
typedef struct {
    const char *backend_name;
    unsigned long long layers;
    unsigned long long seq_len;
    unsigned long long position;
    unsigned long long hidden_dim;
    unsigned long long head_dim;
    unsigned long long ffn_dim;
    const float *initial_position_values;
    unsigned long long initial_position_value_count;
} yvex_cli_layer_fixture_options;
typedef struct {
    int executed;
    const char *status;
    const char *graph_integrity_guard;
    const char *graph_execution_phase;
    const char *backend_status;
    const char *backend_op_status;
    unsigned long long layers;
    unsigned long long total_op_count;
    unsigned long long output_bytes;
    unsigned long long scratch_bytes;
    unsigned long long final_output_checksum;
    unsigned long long final_reference_checksum;
    double final_max_abs_diff;
    unsigned long long output_value_count;
    float output_values[YVEX_SEGMENT_GRAPH_MAX_OUTPUT_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
} yvex_cli_layer_fixture_result;
typedef yvex_cli_layer_fixture_options yvex_graph_layer_fixture_options;
typedef yvex_cli_layer_fixture_result yvex_graph_layer_fixture_result;
int yvex_graph_preflight(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int token_id,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);
int yvex_graph_execute_layer_fixture(const yvex_graph_layer_fixture_options *options,
                                     yvex_graph_layer_fixture_result *out,
                                     yvex_error *err);

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
                                      yvex_fixture_graph_result *out,
                                      yvex_error *err);
int yvex_run_artifacts_prepare(yvex_run_artifacts *out,
                               int save_run,
                               const char *run_dir,
                               const char *metrics_out,
                               const char *trace_out,
                               const char *profile_out,
                               yvex_error *err);
int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts,
                                     int arg_count,
                                     char **args,
                                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_RUNTIME_H_INCLUDED */
