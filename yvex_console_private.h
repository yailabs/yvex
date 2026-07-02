/*
 * yvex_console_private.h - Private operator console and command support.
 *
 * This header exposes shared operator helpers, domain-owned command entrypoint
 * declarations, and diagnostic console state. It is not a command catalog and
 * must not own command help text.
 */
#ifndef YVEX_CONSOLE_PRIVATE_H
#define YVEX_CONSOLE_PRIVATE_H

#include <stdio.h>

#include <yvex/yvex.h>
#include "yvex_run_private.h"

typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
} yvex_cli_tokenizer_context;

typedef struct {
    yvex_model_registry_entry entry;
    char format[16];
    char architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    char support_level[64];
} yvex_cli_metadata_snapshot;

typedef struct {
    const char *registry_path;
    const char *path;
    const char *alias;
    const char *family;
    const char *model;
    const char *scope;
    const char *artifact_class;
    const char *qprofile;
    const char *calibration;
    const char *sha256;
    const char *support_level;
} yvex_cli_models_add_options;

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

int print_yvex_error(const yvex_error *err, int exit_code);
int exit_for_status(int status);
void print_quoted_bytes(const char *data, unsigned long long len);
int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err);
void close_tokenizer_context(yvex_cli_tokenizer_context *ctx);
void close_model_context(yvex_cli_tokenizer_context *ctx);
int open_model_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err);
int open_tokenizer_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err);
void print_tensor_dims(const unsigned long long *dims, unsigned int rank);
void print_native_dims(const unsigned long long *dims, unsigned int rank);
void print_token_ids(const yvex_tokens *tokens);
int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len);
int parse_positive_ull(const char *text, unsigned long long *out);
int parse_ull_allow_zero(const char *text, unsigned long long *out);
int parse_uint_allow_zero(const char *text, unsigned int *out);
int parse_dims_csv(const char *text, unsigned int expected_rank, unsigned long long dims[4]);
int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot, const char *path, yvex_error *err);
void model_ref_registry_entry_view(const yvex_model_ref *ref, yvex_model_registry_entry *entry);
void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report);
int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface);
void print_graph_guard_report(const yvex_cli_graph_guard_report *report);
int cli_token_input_vocab_from_model(const char *path, unsigned long long *vocab_size, yvex_error *err);
void print_token_input_summary(const yvex_token_input *input,
                               const char *status,
                               const char *bounds_status,
                               unsigned long long selected_index,
                               unsigned int selected_token,
                               int has_selected);
int models_registry_open(yvex_model_registry **registry,
                         const char *registry_path,
                         int create_if_missing,
                         yvex_error *err);
int preflight_graph_guard(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int token_id,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);
int yvex_cli_graph_execute_layer_fixture(const yvex_cli_layer_fixture_options *options,
                                         yvex_cli_layer_fixture_result *out,
                                         yvex_error *err);

int yvex_attention_command(int argc, char **argv);
void yvex_attention_help(FILE *fp);
int yvex_backend_command(int argc, char **argv);
void yvex_backend_help(FILE *fp);
int yvex_chat_command(int argc, char **argv);
void yvex_chat_help(FILE *fp);
int yvex_convert_command(int argc, char **argv);
void yvex_convert_help(FILE *fp);
int yvex_cuda_info_command(int argc, char **argv);
void yvex_cuda_info_help(FILE *fp);
int yvex_decode_command(int argc, char **argv);
void yvex_decode_help(FILE *fp);
int yvex_detokenize_command(int argc, char **argv);
void yvex_detokenize_help(FILE *fp);
int yvex_engine_command(int argc, char **argv);
void yvex_engine_help(FILE *fp);
int yvex_graph_command(int argc, char **argv);
void yvex_graph_help(FILE *fp);
int yvex_generate_command(int argc, char **argv);
void yvex_generate_help(FILE *fp);
int yvex_fullmodel_command(int argc, char **argv);
void yvex_fullmodel_help(FILE *fp);
int yvex_gguf_template_command(int argc, char **argv);
void yvex_gguf_template_help(FILE *fp);
int yvex_gguf_emit_command(int argc, char **argv);
void yvex_gguf_emit_help(FILE *fp);
int yvex_imatrix_command(int argc, char **argv);
void yvex_imatrix_help(FILE *fp);
int yvex_runtime_info_command(int argc, char **argv);
void yvex_runtime_info_help(FILE *fp);
int yvex_inspect_command(int argc, char **argv);
void yvex_inspect_help(FILE *fp);
int yvex_input_command(int argc, char **argv);
void yvex_input_help(FILE *fp);
int yvex_integrity_command(int argc, char **argv);
void yvex_integrity_help(FILE *fp);
int yvex_kv_command(int argc, char **argv);
void yvex_kv_help(FILE *fp);
int yvex_logits_command(int argc, char **argv);
void yvex_logits_help(FILE *fp);
int yvex_materialize_command(int argc, char **argv);
void yvex_materialize_help(FILE *fp);
int yvex_materialize_gate_command(int argc, char **argv);
void yvex_materialize_gate_help(FILE *fp);
int yvex_metadata_command(int argc, char **argv);
void yvex_metadata_help(FILE *fp);
int yvex_model_gate_command(int argc, char **argv);
void yvex_model_gate_help(FILE *fp);
int yvex_model_target_command(int argc, char **argv);
void yvex_model_target_help(FILE *fp);
int yvex_models_command(int argc, char **argv);
void yvex_models_help(FILE *fp);
int yvex_native_weights_command(int argc, char **argv);
void yvex_native_weights_help(FILE *fp);
int yvex_paths_command(int argc, char **argv);
void yvex_paths_help(FILE *fp);
int yvex_plan_command(int argc, char **argv);
void yvex_plan_help(FILE *fp);
int yvex_prefill_command(int argc, char **argv);
void yvex_prefill_help(FILE *fp);
int yvex_prompt_command(int argc, char **argv);
void yvex_prompt_help(FILE *fp);
int yvex_quant_job_command(int argc, char **argv);
void yvex_quant_job_help(FILE *fp);
int yvex_quant_policy_command(int argc, char **argv);
void yvex_quant_policy_help(FILE *fp);
int yvex_qtype_support_command(int argc, char **argv);
void yvex_qtype_support_help(FILE *fp);
int yvex_run_command(int argc, char **argv);
void yvex_run_help(FILE *fp);
int yvex_sample_command(int argc, char **argv);
void yvex_sample_help(FILE *fp);
int yvex_session_command(int argc, char **argv);
void yvex_session_help(FILE *fp);
int yvex_source_manifest_command(int argc, char **argv);
void yvex_source_manifest_help(FILE *fp);
int yvex_tensor_map_command(int argc, char **argv);
void yvex_tensor_map_help(FILE *fp);
int yvex_tokenize_command(int argc, char **argv);
void yvex_tokenize_help(FILE *fp);
int yvex_tokenizer_command(int argc, char **argv);
void yvex_tokenizer_help(FILE *fp);
int yvex_tensors_command(int argc, char **argv);
void yvex_tensors_help(FILE *fp);

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
