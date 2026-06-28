#ifndef YVEX_CLI_PRIVATE_H
#define YVEX_CLI_PRIVATE_H

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
