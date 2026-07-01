/*
 * yvex_cli.c - Operator CLI entrypoint.
 *
 * This file owns only top-level command lookup, short command listing,
 * top-level help, and argv dispatch. Command behavior and detailed help are
 * owned by the domain modules that implement each runtime or artifact boundary.
 */

#include <stdio.h>
#include <string.h>

#include "yvex_console_private.h"
#include "yvex/version.h"

typedef int (*yvex_cli_handler_fn)(int argc, char **argv);
typedef void (*yvex_cli_help_fn)(FILE *fp);

typedef struct {
    const char *name;
    const char *summary;
    yvex_cli_handler_fn handler;
    yvex_cli_help_fn help;
} yvex_cli_command;

static int command_commands(int argc, char **argv);
static int command_help(int argc, char **argv);
static int command_version(int argc, char **argv);
static void command_commands_help(FILE *fp);
static void command_help_help(FILE *fp);
static void command_version_help(FILE *fp);

static const yvex_cli_command yvex_commands[] = {
    { "backend", "Inspect backend availability.", yvex_backend_command, yvex_backend_help },
    { "chat", "Open the diagnostic console.", yvex_chat_command, yvex_chat_help },
    { "commands", "List implemented CLI commands.", command_commands, command_commands_help },
    { "convert", "Plan or emit selected GGUF conversions.", yvex_convert_command, yvex_convert_help },
    { "cuda-info", "Probe CUDA devices.", yvex_cuda_info_command, yvex_cuda_info_help },
    { "decode", "Create one bounded diagnostic decode-state step.", yvex_decode_command, yvex_decode_help },
    { "detokenize", "Decode token IDs.", yvex_detokenize_command, yvex_detokenize_help },
    { "engine", "Open an engine descriptor.", yvex_engine_command, yvex_engine_help },
    { "graph", "Build graph diagnostics or run narrow graph proofs.", yvex_graph_command, yvex_graph_help },
    { "generate", "bounded diagnostic generation loop.", yvex_generate_command, yvex_generate_help },
    { "gguf-template", "Inspect or validate a GGUF conversion template.", yvex_gguf_template_command, yvex_gguf_template_help },
    { "gguf-emit", "Emit a controlled YVEX-owned GGUF.", yvex_gguf_emit_command, yvex_gguf_emit_help },
    { "help", "Show CLI or command help.", command_help, command_help_help },
    { "imatrix", "Create, inspect, or validate an imatrix manifest.", yvex_imatrix_command, yvex_imatrix_help },
    { "info", "Show YVEX build and boundary status.", yvex_runtime_info_command, yvex_runtime_info_help },
    { "inspect", "Inspect an artifact descriptor.", yvex_inspect_command, yvex_inspect_help },
    { "input", "Parse and validate token input.", yvex_input_command, yvex_input_help },
    { "integrity", "Check or report local artifact integrity.", yvex_integrity_command, yvex_integrity_help },
    { "kv", "Create a minimal session-owned KV store.", yvex_kv_command, yvex_kv_help },
    { "logits", "Create a bounded diagnostic logits buffer.", yvex_logits_command, yvex_logits_help },
    { "materialize", "Materialize selected weights.", yvex_materialize_command, yvex_materialize_help },
    { "materialize-gate", "Run a materialization hardening gate.", yvex_materialize_gate_command, yvex_materialize_gate_help },
    { "metadata", "Print parsed metadata entries.", yvex_metadata_command, yvex_metadata_help },
    { "model-gate", "Validate selected artifact materialization facts.", yvex_model_gate_command, yvex_model_gate_help },
    { "model-target", "Inspect model pressure targets.", yvex_model_target_command, yvex_model_target_help },
    { "models", "Manage the local model alias registry.", yvex_models_command, yvex_models_help },
    { "native-weights", "Inventory safetensors native weights.", yvex_native_weights_command, yvex_native_weights_help },
    { "paths", "Show runtime filesystem paths.", yvex_paths_command, yvex_paths_help },
    { "plan", "Build an estimate-only execution plan.", yvex_plan_command, yvex_plan_help },
    { "prefill", "Create an inspectable prefill state summary.", yvex_prefill_command, yvex_prefill_help },
    { "prompt", "Render a bounded prompt.", yvex_prompt_command, yvex_prompt_help },
    { "quant-job", "Create, inspect, or validate a quantization job manifest.", yvex_quant_job_command, yvex_quant_job_help },
    { "quant-policy", "Inspect, validate, or derive a quantization policy.", yvex_quant_policy_command, yvex_quant_policy_help },
    { "qtype-support", "Print qtype support policy.", yvex_qtype_support_command, yvex_qtype_support_help },
    { "run", "Accept one prompt through diagnostics.", yvex_run_command, yvex_run_help },
    { "sample", "Select one bounded diagnostic token.", yvex_sample_command, yvex_sample_help },
    { "session", "Create an engine/session diagnostic session.", yvex_session_command, yvex_session_help },
    { "source-manifest", "Create a source provenance manifest.", yvex_source_manifest_command, yvex_source_manifest_help },
    { "tensor-map", "Map native tensors to YVEX roles.", yvex_tensor_map_command, yvex_tensor_map_help },
    { "tokenize", "Encode text with an implemented tokenizer.", yvex_tokenize_command, yvex_tokenize_help },
    { "tokenizer", "Inspect tokenizer metadata.", yvex_tokenizer_command, yvex_tokenizer_help },
    { "tensors", "Print tensor table rows.", yvex_tensors_command, yvex_tensors_help },
    { "version", "Print the YVEX version.", command_version, command_version_help },
};

static const unsigned long yvex_command_count = sizeof(yvex_commands) / sizeof(yvex_commands[0]);

static const yvex_cli_command *find_command(const char *name)
{
    unsigned long i;

    if (!name) {
        return NULL;
    }
    for (i = 0; i < yvex_command_count; ++i) {
        if (strcmp(yvex_commands[i].name, name) == 0) {
            return &yvex_commands[i];
        }
    }
    return NULL;
}

static void print_top_level_help(FILE *fp)
{
    unsigned long i;

    fprintf(fp, "usage: yvex <command> [options]\n\n");
    fprintf(fp, "Implemented commands:\n");
    for (i = 0; i < yvex_command_count; ++i) {
        fprintf(fp, "  %-18s %s\n", yvex_commands[i].name, yvex_commands[i].summary);
    }
    fprintf(fp, "\nOptions:\n");
    fprintf(fp, "  -h, --help       Show top-level help.\n");
    fprintf(fp, "  --version        Print the YVEX version.\n");
    fprintf(fp, "\nDetailed command help is owned by each runtime or artifact module.\n");
}

static int command_commands(int argc, char **argv)
{
    unsigned long i;

    (void)argc;
    (void)argv;
    printf("Implemented commands:\n");
    for (i = 0; i < yvex_command_count; ++i) {
        printf("  %-18s %s\n", yvex_commands[i].name, yvex_commands[i].summary);
    }
    return 0;
}

static int command_help(int argc, char **argv)
{
    const yvex_cli_command *command;

    if (argc <= 2) {
        print_top_level_help(stdout);
        return 0;
    }
    command = find_command(argv[2]);
    if (!command) {
        fprintf(stderr, "yvex: unknown help topic: %s\n", argv[2]);
        fprintf(stderr, "Try 'yvex commands' for implemented commands.\n");
        return 2;
    }
    command->help(stdout);
    return 0;
}

static int command_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("yvex %s\n", yvex_version_string());
    return 0;
}

static void command_commands_help(FILE *fp)
{
    fprintf(fp, "usage: yvex commands\n\nPrints implemented command names and short operator summaries.\n");
}

static void command_help_help(FILE *fp)
{
    fprintf(fp, "usage: yvex help [command]\n\nPrints top-level help or detailed help for one implemented command.\n");
}

static void command_version_help(FILE *fp)
{
    fprintf(fp, "usage: yvex version\n\nPrints the same version string as yvex --version.\n");
}

int main(int argc, char **argv)
{
    const yvex_cli_command *command;

    if (argc == 1) {
        print_top_level_help(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_top_level_help(stdout);
        return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
        return command_version(argc, argv);
    }
    command = find_command(argv[1]);
    if (command) {
        return command->handler(argc, argv);
    }
    fprintf(stderr, "yvex: unknown command: %s\n", argv[1]);
    fprintf(stderr, "Try 'yvex help' for usage.\n");
    return 2;
}
