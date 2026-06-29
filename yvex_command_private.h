#ifndef YVEX_COMMAND_PRIVATE_H
#define YVEX_COMMAND_PRIVATE_H

/*
 * yvex_command_private.h - Private operator command adapter surface.
 *
 * This header is shared by yvex_cli.c and the domain-owned command adapters.
 * It is not a public API and must not become a second runtime ownership map.
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/yvex.h>
#include "yvex_console_private.h"
#include "yvex_run_private.h"

typedef int (*yvex_cli_handler)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *summary;
    const char *usage;
    const char *description;
    yvex_cli_handler handler;
} yvex_cli_command;

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

const yvex_cli_command *yvex_cli_find_command(const char *name);
void yvex_cli_print_command_help(FILE *fp, const yvex_cli_command *command);

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
int models_registry_open(yvex_model_registry **registry, const char *registry_path, int create_if_missing, yvex_error *err);
int preflight_graph_guard(const yvex_model_ref *model_ref, const char *backend_name, int execute_fixture, int execute_segment, unsigned int token_id, yvex_cli_graph_guard_report *report, yvex_error *err);
void yvex_cli_print_top_level_help(FILE *fp);
int yvex_cli_command_commands(int argc, char **argv);
int yvex_cli_command_help(int argc, char **argv);
int yvex_cli_command_version(int argc, char **argv);

int yvex_cli_command_backend(int argc, char **argv);
int yvex_cli_command_chat(int argc, char **argv);
int yvex_cli_command_convert(int argc, char **argv);
int yvex_cli_command_cuda_info(int argc, char **argv);
int yvex_cli_command_detokenize(int argc, char **argv);
int yvex_cli_command_engine(int argc, char **argv);
int yvex_cli_command_graph(int argc, char **argv);
int yvex_cli_command_gguf_template(int argc, char **argv);
int yvex_cli_command_gguf_emit(int argc, char **argv);
int yvex_cli_command_imatrix(int argc, char **argv);
int yvex_cli_command_info(int argc, char **argv);
int yvex_cli_command_inspect(int argc, char **argv);
int yvex_cli_command_input(int argc, char **argv);
int yvex_cli_command_integrity(int argc, char **argv);
int yvex_cli_command_kv(int argc, char **argv);
int yvex_cli_command_materialize(int argc, char **argv);
int yvex_cli_command_materialize_gate(int argc, char **argv);
int yvex_cli_command_metadata(int argc, char **argv);
int yvex_cli_command_model_gate(int argc, char **argv);
int yvex_cli_command_models(int argc, char **argv);
int yvex_cli_command_native_weights(int argc, char **argv);
int yvex_cli_command_paths(int argc, char **argv);
int yvex_cli_command_tensor_map(int argc, char **argv);
int yvex_cli_command_plan(int argc, char **argv);
int yvex_cli_command_prefill(int argc, char **argv);
int yvex_cli_command_prompt(int argc, char **argv);
int yvex_cli_command_quant_job(int argc, char **argv);
int yvex_cli_command_quant_policy(int argc, char **argv);
int yvex_cli_command_qtype_support(int argc, char **argv);
int yvex_cli_command_run(int argc, char **argv);
int yvex_cli_command_session(int argc, char **argv);
int yvex_cli_command_source_manifest(int argc, char **argv);
int yvex_cli_command_tokenize(int argc, char **argv);
int yvex_cli_command_tokenizer(int argc, char **argv);
int yvex_cli_command_tensors(int argc, char **argv);

#endif
