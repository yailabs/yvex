/* Owner: cli.input.private (cli.input).
 * Owns: typed CLI argument records and parsers.
 * Does not own: domain policy, rendering, or command execution.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: CLI input adaptation.
 * Purpose: provide the canonical CLI input adaptation contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef SRC_CLI_INPUT_PRIVATE_H_INCLUDED
#define SRC_CLI_INPUT_PRIVATE_H_INCLUDED

#include "src/cli/io/private.h"

#include <stdio.h>
#include <yvex/artifact.h>
#include <yvex/core.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/generation.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/model_artifact.h>
#include <yvex/internal/model_target.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/source_payload.h>
#include <yvex/model.h>
#include <yvex/registry.h>
#include <yvex/tokenizer.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared CLI command, parser, and diagnostic rendering contract. */
int cli_backend_name_valid(const char *name);
void print_quoted_bytes(const char *data, unsigned long long len);
int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err);
void print_tensor_dims(const unsigned long long *dims, unsigned int rank);
void print_native_dims(const unsigned long long *dims, unsigned int rank);
void print_token_ids(const yvex_tokens *tokens);
int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len);
int parse_dims_csv(const char *text, unsigned int expected_rank, unsigned long long dims[4]);
int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface);
void print_token_input_summary(const yvex_token_input *input, const char *status,
                               const char *bounds_status, unsigned long long selected_index,
                               unsigned int selected_token, int has_selected);
int yvex_accounts_command(int arg_count, char **args);
void yvex_accounts_help(FILE *fp);
int yvex_attention_command(int arg_count, char **args);
void yvex_attention_help(FILE *fp);
int yvex_backend_command(int arg_count, char **args);
void yvex_backend_help(FILE *fp);
int yvex_chat_command(int arg_count, char **args);
void yvex_chat_help(FILE *fp);
int yvex_context_command(int arg_count, char **args);
void yvex_context_help(FILE *fp);
int yvex_convert_command(int arg_count, char **args);
void yvex_convert_help(FILE *fp);
int yvex_cuda_info_command(int arg_count, char **args);
void yvex_cuda_info_help(FILE *fp);
int yvex_decode_command(int arg_count, char **args);
void yvex_decode_help(FILE *fp);
int yvex_detokenize_command(int arg_count, char **args);
void yvex_detokenize_help(FILE *fp);
int yvex_engine_command(int arg_count, char **args);
void yvex_engine_help(FILE *fp);
int yvex_graph_command(int arg_count, char **args);
void yvex_graph_help(FILE *fp);
int yvex_generate_command(int arg_count, char **args);
void yvex_generate_help(FILE *fp);
int yvex_fullmodel_command(int arg_count, char **args);
void yvex_fullmodel_help(FILE *fp);
int yvex_gguf_template_command(int arg_count, char **args);
void yvex_gguf_template_help(FILE *fp);
int yvex_gguf_emit_command(int arg_count, char **args);
void yvex_gguf_emit_help(FILE *fp);
int yvex_imatrix_command(int arg_count, char **args);
void yvex_imatrix_help(FILE *fp);
int yvex_runtime_info_command(int arg_count, char **args);
void yvex_runtime_info_help(FILE *fp);
int yvex_inspect_command(int arg_count, char **args);
void yvex_inspect_help(FILE *fp);
int yvex_input_command(int arg_count, char **args);
void yvex_input_help(FILE *fp);
int yvex_integrity_command(int arg_count, char **args);
void yvex_integrity_help(FILE *fp);
int yvex_kv_command(int arg_count, char **args);
void yvex_kv_help(FILE *fp);
int yvex_logits_command(int arg_count, char **args);
void yvex_logits_help(FILE *fp);
int yvex_materialize_command(int arg_count, char **args);
void yvex_materialize_help(FILE *fp);
int yvex_materialize_gate_command(int arg_count, char **args);
void yvex_materialize_gate_help(FILE *fp);
int yvex_metadata_command(int arg_count, char **args);
void yvex_metadata_help(FILE *fp);
int yvex_model_gate_command(int arg_count, char **args);
void yvex_model_gate_help(FILE *fp);
int yvex_model_target_command(int arg_count, char **args);
void yvex_model_target_help(FILE *fp);
int yvex_moe_command(int arg_count, char **args);
void yvex_moe_help(FILE *fp);
int yvex_models_command(int arg_count, char **args);
void yvex_models_help(FILE *fp);
int yvex_native_weights_command(int arg_count, char **args);
void yvex_native_weights_help(FILE *fp);
int yvex_paths_command(int arg_count, char **args);
void yvex_paths_help(FILE *fp);
int yvex_plan_command(int arg_count, char **args);
void yvex_plan_help(FILE *fp);
int yvex_prefill_command(int arg_count, char **args);
void yvex_prefill_help(FILE *fp);
int yvex_prompt_command(int arg_count, char **args);
void yvex_prompt_help(FILE *fp);
int yvex_quant_job_command(int arg_count, char **args);
void yvex_quant_job_help(FILE *fp);
int yvex_quant_policy_command(int arg_count, char **args);
void yvex_quant_policy_help(FILE *fp);
int yvex_qtype_support_command(int arg_count, char **args);
void yvex_qtype_support_help(FILE *fp);
int yvex_run_command(int arg_count, char **args);
void yvex_run_help(FILE *fp);
int yvex_sample_command(int arg_count, char **args);
void yvex_sample_help(FILE *fp);
int yvex_session_command(int arg_count, char **args);
void yvex_session_help(FILE *fp);
int yvex_source_manifest_command(int arg_count, char **args);
void yvex_source_manifest_help(FILE *fp);
int yvex_source_manifest_report_command(int arg_count, char **args);
int yvex_tensor_collection_command(int arg_count, char **args);
void yvex_tensor_collection_help(FILE *fp);
int yvex_tensor_map_command(int arg_count, char **args);
void yvex_tensor_map_help(FILE *fp);
int yvex_tokenize_command(int arg_count, char **args);
void yvex_tokenize_help(FILE *fp);
int yvex_tokenizer_command(int arg_count, char **args);
void yvex_tokenizer_help(FILE *fp);
int yvex_tensors_command(int arg_count, char **args);
void yvex_tensors_help(FILE *fp);

/* Backend contract. */
typedef struct {
    yvex_backend_report_request request;
    int help;
} yvex_backend_args;
int yvex_backend_args_parse(int argc, char **argv, yvex_backend_args *out, yvex_error *err);
int yvex_cuda_info_args_parse(int argc, char **argv, yvex_backend_args *out, yvex_error *err);

/* Generate contract. */
typedef struct {
    yvex_generation_request request;
    yvex_generate_render_mode render_mode;
    int help_requested;
} yvex_generate_args;
int yvex_generate_args_parse(int argc, char **argv, yvex_generate_args *out, yvex_error *err);

/* Graph contract. */
typedef struct {
    yvex_graph_report_request request;
    yvex_graph_report_mode render_mode;
    struct {
        const char *target;
        const char *artifact_path;
        const char *models_root;
        const char *backend;
        const char *probe;
        const char *scope;
        int execute;
        int compare_backends;
    } attention;
    int help_requested;
    int help_exit_code;
} yvex_graph_args;
int yvex_graph_args_parse(int argc, char **argv, yvex_graph_args *out, yvex_error *err);

/* Kv contract. */
typedef struct {
    yvex_kv_report_request request;
    int help_requested;
} yvex_kv_args;
int yvex_kv_args_parse(int argc, char **argv, yvex_kv_args *out, yvex_error *err);

/* Model Artifacts contract. */
typedef struct {
    yvex_model_artifact_report_request request;
    int help_requested;
} yvex_model_artifacts_args;
int yvex_model_artifacts_args_parse(int argc, char **argv, yvex_model_artifacts_args *out,
                                    yvex_error *err);

/* Model Target contract. */
typedef struct {
    yvex_model_target_request request;
    int help_requested;
    int parse_failed;
    char error_message[256];
} yvex_model_target_args;
int yvex_model_target_args_parse(int argc, char **argv, yvex_model_target_args *out,
                                 yvex_error *err);

/* Sampling contract. */
typedef struct {
    yvex_sampling_report_request request;
    yvex_sampling_report_mode render_mode;
    int help_requested;
    int help_exit_code;
} yvex_sampling_args;
int yvex_sampling_args_parse(int argc, char **argv, yvex_sampling_args *out, yvex_error *err);

/* Source contract. */
typedef struct {
    const char *family;
    const char *release;
    const char *models_root;
    const char *source;
    const char *target;
    int include_files;
    int include_config;
    int include_blockers;
    int include_next;
    int include_tensors;
    int strict;
    unsigned long long tensor_limit;
    yvex_source_render_mode render_mode;
    int help;
} yvex_source_args;
int yvex_source_args_parse(int argc, char **argv, yvex_source_args *out, yvex_error *err);
void yvex_source_report_request_from_parsed(yvex_source_report_request *request,
                                            const yvex_source_args *args);

#ifdef __cplusplus
}
#endif

#endif /* SRC_CLI_INPUT_PRIVATE_H_INCLUDED */
