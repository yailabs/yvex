/*
 * YVEX - CLI bootstrap
 *
 * File: cli/yvex_cli.c
 * Layer: CLI
 *
 * Purpose:
 *   Implements the command table and CLI proof surface for implemented core,
 *   filesystem, artifact, GGUF directory, tensor table, and descriptor
 *   behavior, and H0 engine/session diagnostics. The CLI reports what exists
 *   and does not expose future generation commands before their modules exist.
 *
 * Implements:
 *   - yvex
 *   - yvex backend cpu
 *   - yvex cuda-info
 *   - yvex info
 *   - yvex commands
 *   - yvex help [command]
 *   - yvex version
 *   - yvex paths
 *   - yvex inspect <path>
 *   - yvex metadata <path>
 *   - yvex tensors <path>
 *   - yvex tokenizer <path>
 *   - yvex tokenize <path> --text TEXT
 *   - yvex detokenize <path> --ids IDS
 *   - yvex engine <path>
 *   - yvex graph <path>
 *   - yvex prompt <path> --user TEXT
 *   - yvex plan <path>
 *   - yvex run --model <path> --backend cpu --prompt TEXT
 *   - yvex chat --model <path> --backend cpu
 *   - yvex session <path> --backend cpu
 *   - yvex --help
 *   - yvex --version
 *
 * Invariants:
 *   - unknown commands exit 2
 *   - implemented commands are declared in one command table
 *   - future runtime commands are not listed as implemented
 *
 * Commands:
 *   - make test-cli
 *   - make smoke
 *   - build/bin/yvex info
 */
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/yvex.h>

#include "../src/chat/chat_internal.h"
#include "../src/chat/slash_internal.h"
#include "../src/metrics/metrics_internal.h"

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

static int command_backend(int argc, char **argv);
static int command_chat(int argc, char **argv);
static int command_commands(int argc, char **argv);
static int command_cuda_info(int argc, char **argv);
static int command_detokenize(int argc, char **argv);
static int command_engine(int argc, char **argv);
static int command_graph(int argc, char **argv);
static int command_gguf_emit(int argc, char **argv);
static int command_gguf_template(int argc, char **argv);
static int command_help(int argc, char **argv);
static int command_imatrix(int argc, char **argv);
static int command_info(int argc, char **argv);
static int command_inspect(int argc, char **argv);
static int command_materialize(int argc, char **argv);
static int command_metadata(int argc, char **argv);
static int command_native_weights(int argc, char **argv);
static int command_paths(int argc, char **argv);
static int command_plan(int argc, char **argv);
static int command_prompt(int argc, char **argv);
static int command_quant_policy(int argc, char **argv);
static int command_run(int argc, char **argv);
static int command_session(int argc, char **argv);
static int command_source_manifest(int argc, char **argv);
static int command_tensor_map(int argc, char **argv);
static int command_tokenize(int argc, char **argv);
static int command_tokenizer(int argc, char **argv);
static int command_tensors(int argc, char **argv);
static int command_version(int argc, char **argv);

static const yvex_cli_command yvex_commands[] = {
    {
        "backend",
        "Inspect an implemented backend.",
        "yvex backend cpu|cuda",
        "Reports backend status and capabilities. CPU is the reference path; CUDA is available when the local driver/device probe succeeds.",
        command_backend,
    },
    {
        "chat",
        "Start the I0 diagnostic chat shell.",
        "yvex chat --model FILE --backend cpu|cuda [--ctx N] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session and accepts user prompt tokens in a REPL. Optional J0 artifacts record implemented runtime phases only.",
        command_chat,
    },
    {
        "commands",
        "List implemented CLI commands.",
        "yvex commands",
        "Prints the command names implemented by this binary.",
        command_commands,
    },
    {
        "cuda-info",
        "Probe CUDA devices.",
        "yvex cuda-info",
        "Reports CUDA driver/device facts when CUDA is available. It does not claim CUDA model execution, matmul, attention, or inference.",
        command_cuda_info,
    },
    {
        "detokenize",
        "Decode token IDs with an implemented tokenizer.",
        "yvex detokenize <path> --ids IDS",
        "Opens a GGUF tokenizer descriptor and decodes comma-separated token IDs. E0 executes only the yvex-fixture-simple tokenizer.",
        command_detokenize,
    },
    {
        "engine",
        "Open an H0 engine descriptor.",
        "yvex engine <path>",
        "Opens the descriptor/tokenizer/graph stack and reports engine diagnostics. It does not execute prefill, decode, run, or chat.",
        command_engine,
    },
    {
        "graph",
        "Build and dump the F0 graph planning substrate.",
        "yvex graph <path> [--seq N] [--ctx N]",
        "Opens a GGUF descriptor, builds a deterministic graph planning artifact, and prints values, planned ops, and missing-role diagnostics. It does not execute the graph.",
        command_graph,
    },
    {
        "gguf-template",
        "Inspect or validate a GGUF conversion template.",
        "yvex gguf-template inspect|validate --template FILE | yvex gguf-template compare --template FILE --native-source DIR",
        "Validates GGUF metadata, tokenizer metadata, tensor directory, tensor roles, and optional exact-name native inventory comparison. It does not emit GGUF, quantize, materialize, or infer.",
        command_gguf_template,
    },
    {
        "gguf-emit",
        "Emit a controlled YVEX-owned GGUF.",
        "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--overwrite]",
        "Writes one controlled F32 tensor and controlled tokenizer metadata, then validates the emitted GGUF through YVEX parse and CPU materialization. It does not convert DeepSeek, quantize, infer, or emit a generic model.",
        command_gguf_emit,
    },
    {
        "help",
        "Show help for the CLI or a command.",
        "yvex help [command]",
        "Prints top-level help or detailed help for one implemented command.",
        command_help,
    },
    {
        "imatrix",
        "Create, inspect, or validate an imatrix manifest.",
        "yvex imatrix create --name NAME --arch NAME --imatrix FILE --format FORMAT --status STATUS --out FILE | yvex imatrix inspect|validate --manifest FILE",
        "Handles calibration/imatrix provenance manifests. It does not generate imatrix data, run calibration, quantize, emit GGUF, materialize, or infer.",
        command_imatrix,
    },
    {
        "info",
        "Show current YVEX build and implementation status.",
        "yvex info",
        "Prints the implemented core/filesystem/artifact/GGUF/tensor descriptor status.",
        command_info,
    },
    {
        "inspect",
        "Inspect a GGUF artifact descriptor.",
        "yvex inspect <path>",
        "Opens a file, parses the GGUF directory, builds a YVEX tensor table and descriptor, and prints a descriptor-only summary. Tokenizers, backends, and model execution are not implemented.",
        command_inspect,
    },
    {
        "materialize",
        "Materialize fixture weights into backend tensors.",
        "yvex materialize --model FILE --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]",
        "Copies GGUF tensor bytes into backend-owned tensors and reports residency. This does not execute prefill, decode, sampling, generation, or inference.",
        command_materialize,
    },
    {
        "metadata",
        "Print parsed GGUF metadata entries.",
        "yvex metadata <path>",
        "Opens a GGUF file and prints parsed metadata key/value summaries. Arrays are summarized; tokenizers and model loading are not implemented.",
        command_metadata,
    },
    {
        "native-weights",
        "Inventory safetensors native weights.",
        "yvex native-weights --source DIR [--limit N] [--tensor NAME] [--json]",
        "Reads safetensors headers under a local source directory and reports tensor metadata. It does not read tensor payloads, convert, quantize, emit GGUF, materialize, or infer.",
        command_native_weights,
    },
    {
        "paths",
        "Show resolved runtime filesystem paths.",
        "yvex paths [--project DIR] [--run] [--create]",
        "Prints resolved config/cache/state/data paths. With --run it prints a prepared run directory. With --create it creates that run directory.",
        command_paths,
    },
    {
        "tensor-map",
        "Map native tensors to GGUF/template roles.",
        "yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]",
        "Maps native safetensors tensor names to canonical YVEX roles and proposed GGUF/template tensor names. It reads metadata only and does not convert, quantize, emit GGUF, materialize, or infer.",
        command_tensor_map,
    },
    {
        "plan",
        "Build and dump an estimate-only execution plan.",
        "yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx N]",
        "Builds a graph and memory estimate. CPU and CUDA backend capability labels are probed, but execution remains disabled.",
        command_plan,
    },
    {
        "prompt",
        "Render a bounded prompt from explicit messages.",
        "yvex prompt <path> [--system TEXT] --user TEXT [--assistant TEXT] [--tokens]",
        "Renders the YVEX default prompt format. Arbitrary Jinja chat templates are not executed in E0.",
        command_prompt,
    },
    {
        "quant-policy",
        "Inspect, validate, or derive a quantization policy manifest.",
        "yvex quant-policy inspect|validate --policy FILE [--template FILE] | yvex quant-policy derive --template FILE --arch NAME --out FILE",
        "Handles declarative qtype policy manifests. It does not quantize tensors, emit GGUF, materialize weights, calibrate imatrix data, or infer.",
        command_quant_policy,
    },
    {
        "run",
        "Accept one prompt through the I0 runtime shell.",
        "yvex run --model FILE --backend cpu|cuda --prompt TEXT [--system TEXT] [--output plain|json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session, tokenizes the prompt, accepts tokens, and reports accepted-only runtime diagnostics. Optional J0 artifacts record implemented runtime phases only.",
        command_run,
    },
    {
        "session",
        "Create an H0 diagnostic session.",
        "yvex session <path> [--backend cpu|cuda] [--ctx N] [--text TEXT] [--accept-tokens]",
        "Creates a lifecycle-only session over an engine and backend. It may accept tokens for diagnostics, but it does not run prefill, decode, or generation.",
        command_session,
    },
    {
        "source-manifest",
        "Create an OWI source provenance manifest.",
        "yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE [--license TEXT] [--model-card URL] [--node NAME] [--dry-run-log FILE] [--download-log FILE] [--pid-file FILE] [--download-command TEXT]",
        "Scans a local official-weight source directory and writes provenance JSON. It does not download, parse safetensors, quantize, emit GGUF, materialize, or infer.",
        command_source_manifest,
    },
    {
        "tokenize",
        "Encode text with an implemented tokenizer.",
        "yvex tokenize <path> --text TEXT",
        "Opens a GGUF tokenizer descriptor and tokenizes text. E0 executes only the yvex-fixture-simple tokenizer.",
        command_tokenize,
    },
    {
        "tokenizer",
        "Inspect GGUF tokenizer metadata.",
        "yvex tokenizer <path>",
        "Prints tokenizer kind, support level, vocabulary size, special token IDs, and chat template presence.",
        command_tokenizer,
    },
    {
        "tensors",
        "Print YVEX tensor table rows.",
        "yvex tensors <path>",
        "Opens a GGUF file and prints YVEX tensor table rows with role, dtype, and known storage bytes. Backend support and model execution are not implemented.",
        command_tensors,
    },
    {
        "version",
        "Print the YVEX version.",
        "yvex version",
        "Prints the same version string as yvex --version.",
        command_version,
    },
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

    fprintf(fp, "usage: yvex <command> [options]\n");
    fprintf(fp, "\n");
    fprintf(fp, "Implemented commands:\n");
    for (i = 0; i < yvex_command_count; ++i) {
        fprintf(fp, "  %-10s %s\n", yvex_commands[i].name, yvex_commands[i].summary);
    }
    fprintf(fp, "\n");
    fprintf(fp, "Options:\n");
    fprintf(fp, "  -h, --help       Show top-level help.\n");
    fprintf(fp, "  --version        Print the YVEX version.\n");
    fprintf(fp, "\n");
    fprintf(fp, "Planned runtime areas are documented in docs/, but are not implemented by this binary.\n");
}

static void print_command_help(FILE *fp, const yvex_cli_command *command)
{
    fprintf(fp, "usage: %s\n", command->usage);
    fprintf(fp, "\n");
    fprintf(fp, "%s\n", command->description);
}

static int print_yvex_error(const yvex_error *err, int exit_code)
{
    fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

static int exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

static void print_quoted_bytes(const char *data, unsigned long long len)
{
    unsigned long long i;

    putchar('"');
    for (i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        if (ch == '"' || ch == '\\') {
            putchar('\\');
            putchar((int)ch);
        } else if (ch == '\n') {
            printf("\\n");
        } else if (ch == '\r') {
            printf("\\r");
        } else if (ch == '\t') {
            printf("\\t");
        } else if (ch < 32 || ch > 126) {
            printf("\\x%02x", (unsigned int)ch);
        } else {
            putchar((int)ch);
        }
    }
    putchar('"');
}

static void print_metadata_value(const yvex_gguf_value *value)
{
    unsigned long long u64;
    long long i64;
    double f64;
    int bool_value;
    const char *string_data;
    unsigned long long string_len;
    yvex_gguf_array_info array;

    switch (yvex_gguf_value_type_of(value)) {
    case YVEX_GGUF_VALUE_UINT8:
    case YVEX_GGUF_VALUE_UINT16:
    case YVEX_GGUF_VALUE_UINT32:
    case YVEX_GGUF_VALUE_UINT64:
        if (yvex_gguf_value_as_u64(value, &u64) == YVEX_OK) {
            printf("%llu", u64);
        }
        break;
    case YVEX_GGUF_VALUE_INT8:
    case YVEX_GGUF_VALUE_INT16:
    case YVEX_GGUF_VALUE_INT32:
    case YVEX_GGUF_VALUE_INT64:
        if (yvex_gguf_value_as_i64(value, &i64) == YVEX_OK) {
            printf("%lld", i64);
        }
        break;
    case YVEX_GGUF_VALUE_FLOAT32:
    case YVEX_GGUF_VALUE_FLOAT64:
        if (yvex_gguf_value_as_f64(value, &f64) == YVEX_OK) {
            printf("%g", f64);
        }
        break;
    case YVEX_GGUF_VALUE_BOOL:
        if (yvex_gguf_value_as_bool(value, &bool_value) == YVEX_OK) {
            printf("%s", bool_value ? "true" : "false");
        }
        break;
    case YVEX_GGUF_VALUE_STRING:
        if (yvex_gguf_value_as_string(value, &string_data, &string_len) == YVEX_OK) {
            print_quoted_bytes(string_data, string_len);
        }
        break;
    case YVEX_GGUF_VALUE_ARRAY:
        if (yvex_gguf_value_array_info(value, &array) == YVEX_OK) {
            printf("array<%s>[%llu]", yvex_gguf_value_type_name(array.element_type), array.count);
        }
        break;
    }
}

static int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;
    options.map = 1;

    return yvex_artifact_open(artifact, &options, err);
}

static void close_tokenizer_context(yvex_cli_tokenizer_context *ctx)
{
    if (!ctx) {
        return;
    }
    yvex_tokenizer_close(ctx->tokenizer);
    ctx->tokenizer = NULL;
    yvex_model_descriptor_close(ctx->model);
    yvex_tensor_table_close(ctx->table);
    yvex_gguf_close(ctx->gguf);
    yvex_artifact_close(ctx->artifact);
    memset(ctx, 0, sizeof(*ctx));
}

static void close_model_context(yvex_cli_tokenizer_context *ctx)
{
    if (!ctx) {
        return;
    }
    yvex_model_descriptor_close(ctx->model);
    yvex_tensor_table_close(ctx->table);
    yvex_gguf_close(ctx->gguf);
    yvex_artifact_close(ctx->artifact);
    memset(ctx, 0, sizeof(*ctx));
}

static int open_model_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
{
    int rc;

    memset(ctx, 0, sizeof(*ctx));
    rc = open_artifact_for_gguf(path, &ctx->artifact, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&ctx->gguf, ctx->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&ctx->table, ctx->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&ctx->model, ctx->gguf, ctx->table, err);
    }
    if (rc != YVEX_OK) {
        close_model_context(ctx);
    }
    return rc;
}

static int open_tokenizer_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
{
    int rc = open_model_context(path, ctx, err);

    if (rc == YVEX_OK) {
        rc = yvex_tokenizer_from_gguf(&ctx->tokenizer, ctx->gguf, ctx->model, err);
    }
    if (rc != YVEX_OK) {
        close_tokenizer_context(ctx);
    }
    return rc;
}

static void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int d;

    printf("[");
    for (d = 0; d < rank; ++d) {
        if (d > 0) {
            printf(",");
        }
        printf("%llu", dims[d]);
    }
    printf("]");
}

static void print_native_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    printf("[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) {
            printf(",");
        }
        printf("%llu", dims[i]);
    }
    printf("]");
}

static void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;

    printf("ids:");
    for (i = 0; i < tokens->len; ++i) {
        printf(" %u", tokens->ids[i]);
    }
    printf("\n");
}

static int print_special_id_line(const char *name, int (*fn)(const yvex_tokenizer *, unsigned int *), const yvex_tokenizer *tokenizer)
{
    unsigned int id;
    int rc = fn(tokenizer, &id);

    if (rc == YVEX_OK) {
        printf("%s: %u\n", name, id);
    } else {
        printf("%s: absent\n", name);
    }
    return YVEX_OK;
}

static void print_backend_capability(const yvex_backend *backend, yvex_backend_capability capability)
{
    printf("  %s: %s\n",
           yvex_backend_capability_name(capability),
           yvex_backend_supports(backend, capability) ? "yes" : "no");
}

static const char *yes_no(int value)
{
    return value ? "yes" : "no";
}

static int parse_source_status(const char *text, yvex_source_status *out)
{
    if (!text || !out) {
        return 0;
    }
    if (strcmp(text, "unknown") == 0) {
        *out = YVEX_SOURCE_STATUS_UNKNOWN;
        return 1;
    }
    if (strcmp(text, "in-progress") == 0) {
        *out = YVEX_SOURCE_STATUS_IN_PROGRESS;
        return 1;
    }
    if (strcmp(text, "incomplete") == 0) {
        *out = YVEX_SOURCE_STATUS_INCOMPLETE;
        return 1;
    }
    if (strcmp(text, "complete") == 0) {
        *out = YVEX_SOURCE_STATUS_COMPLETE;
        return 1;
    }
    if (strcmp(text, "failed") == 0) {
        *out = YVEX_SOURCE_STATUS_FAILED;
        return 1;
    }
    return 0;
}

static int command_backend(int argc, char **argv)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("backend"));
            return 0;
        }
        fprintf(stderr, "yvex: backend requires cpu or cuda\n");
        fprintf(stderr, "usage: yvex backend cpu|cuda\n");
        return 2;
    }

    if (strcmp(argv[2], "cpu") == 0) {
        options.kind = YVEX_BACKEND_KIND_CPU;
    } else if (strcmp(argv[2], "cuda") == 0) {
        options.kind = YVEX_BACKEND_KIND_CUDA;
    } else {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", argv[2]);
        fprintf(stderr, "Try 'yvex help backend' for usage.\n");
        return 2;
    }

    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("backend: %s\n", argv[2]);
        printf("status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: backend-unsupported\n");
        return 5;
    }
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(backend)));
    printf("status: %s\n", yvex_backend_status_name(yvex_backend_status_of(backend)));
    if (yvex_backend_get_device_info(backend, &device_info, &err) == YVEX_OK &&
        yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CUDA) {
        printf("device: %d\n", device_info.device_index);
        printf("name: %s\n", device_info.name ? device_info.name : "");
        printf("compute_capability: %d.%d\n",
               device_info.compute_capability_major,
               device_info.compute_capability_minor);
        printf("memory:\n");
        printf("  free_bytes: %llu\n", device_info.free_memory_bytes);
        printf("  total_bytes: %llu\n", device_info.total_memory_bytes);
        printf("  allocated_bytes: %llu\n", stats.allocated_bytes);
        printf("  allocation_count: %llu\n", stats.allocation_count);
        printf("  peak_allocated_bytes: %llu\n", stats.peak_allocated_bytes);
    } else {
    printf("memory:\n");
    printf("  allocated_bytes: %llu\n", stats.allocated_bytes);
    printf("  allocation_count: %llu\n", stats.allocation_count);
    printf("  peak_allocated_bytes: %llu\n", stats.peak_allocated_bytes);
    }
    printf("capabilities:\n");
    print_backend_capability(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC);
    print_backend_capability(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_EMBED);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_MATMUL);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_RMS_NORM);
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_ATTENTION);
    printf("status: backend-ready\n");

    yvex_backend_close(backend);
    return 0;
}

static int command_cuda_info(int argc, char **argv)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_device_info info;
    yvex_error err;
    int rc;

    if (argc != 2 && !(argc == 3 && (strcmp(argv[2], "--help") == 0 ||
                                    strcmp(argv[2], "-h") == 0))) {
        fprintf(stderr, "usage: yvex cuda-info\n");
        return 2;
    }
    if (argc == 3) {
        print_command_help(stdout, find_command("cuda-info"));
        return 0;
    }

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("cuda: unavailable\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: cuda-unavailable\n");
        return 5;
    }
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_get_device_info(backend, &info, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("cuda: available\n");
    printf("device_count: >=1\n");
    printf("\n");
    printf("device %d:\n", info.device_index);
    printf("  name: %s\n", info.name ? info.name : "");
    printf("  compute_capability: %d.%d\n",
           info.compute_capability_major,
           info.compute_capability_minor);
    printf("  global_memory_bytes: %llu\n", info.global_memory_bytes);
    printf("  free_memory_bytes: %llu\n", info.free_memory_bytes);
    printf("  total_memory_bytes: %llu\n", info.total_memory_bytes);
    printf("  unified_addressing: %s\n", yes_no(info.unified_addressing));
    printf("  managed_memory: %s\n", yes_no(info.managed_memory));
    printf("\n");
    printf("status: cuda-info\n");
    yvex_backend_close(backend);
    return 0;
}

static int command_commands(int argc, char **argv)
{
    unsigned long i;
    (void)argc;
    (void)argv;

    printf("Implemented commands:\n");
    for (i = 0; i < yvex_command_count; ++i) {
        printf("  %s\n", yvex_commands[i].name);
    }
    return 0;
}

static int command_tokenizer(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const char *chat_template;
    unsigned long long chat_template_len;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("tokenizer"));
            return 0;
        }
        fprintf(stderr, "yvex: tokenizer requires exactly one path\n");
        fprintf(stderr, "usage: yvex tokenizer <path>\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("format: gguf\n");
    printf("architecture: %s\n", yvex_arch_name(yvex_model_arch(ctx.model)));
    printf("model_name: %s\n", yvex_model_name(ctx.model));
    printf("tokenizer_model: %s\n", yvex_tokenizer_kind_name(yvex_tokenizer_kind_of(ctx.tokenizer)));
    printf("support: %s\n", yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    printf("vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    (void)print_special_id_line("bos_token_id", yvex_tokenizer_bos_id, ctx.tokenizer);
    (void)print_special_id_line("eos_token_id", yvex_tokenizer_eos_id, ctx.tokenizer);
    (void)print_special_id_line("unk_token_id", yvex_tokenizer_unk_id, ctx.tokenizer);
    if (yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK) {
        (void)chat_template;
        printf("chat_template: present-unsupported\n");
    } else {
        printf("chat_template: absent\n");
    }
    printf("status: tokenizer-descriptor\n");

    close_tokenizer_context(&ctx);
    return 0;
}

static int command_tokenize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_tokens tokens;
    yvex_error err;
    const char *text = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("tokenize"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--text") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = argv[++i];
        } else if (strcmp(argv[i], "--pieces") == 0 || strcmp(argv[i], "--no-bos") == 0 || strcmp(argv[i], "--eos") == 0) {
            /* Accepted for E0 CLI shape; fixture tokenization has no implicit BOS/EOS. */
        } else {
            fprintf(stderr, "yvex: unknown tokenize option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help tokenize' for usage.\n");
            return 2;
        }
    }
    if (!text) {
        fprintf(stderr, "yvex: tokenize requires --text\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, text, &tokens, &err);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("tokens: %llu\n", tokens.len);
    print_token_ids(&tokens);
    printf("pieces:\n");
    for (i = 0; (unsigned long long)i < tokens.len; ++i) {
        const yvex_token_info *token = yvex_tokenizer_token_at(ctx.tokenizer, tokens.ids[i]);
        printf("  %u ", tokens.ids[i]);
        print_quoted_bytes(token ? token->text : "", token ? token->text_len : 0);
        printf("\n");
    }
    printf("status: tokenized\n");

    yvex_tokens_free(&tokens);
    close_tokenizer_context(&ctx);
    return 0;
}

static int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
{
    unsigned int *ids = NULL;
    unsigned long long len = 0;
    unsigned long long cap = 0;
    const char *p = text;

    *out_ids = NULL;
    *out_len = 0;

    while (*p) {
        char *end = NULL;
        unsigned long value;
        unsigned int *next;

        value = strtoul(p, &end, 10);
        if (end == p || value > 0xfffffffful) {
            free(ids);
            return 0;
        }
        if (len == cap) {
            unsigned long long next_cap = cap == 0 ? 8 : cap * 2u;
            if (next_cap > (unsigned long long)(SIZE_MAX / sizeof(*ids))) {
                free(ids);
                return 0;
            }
            next = (unsigned int *)realloc(ids, (size_t)next_cap * sizeof(*ids));
            if (!next) {
                free(ids);
                return 0;
            }
            ids = next;
            cap = next_cap;
        }
        ids[len++] = (unsigned int)value;
        if (*end == ',') {
            p = end + 1;
        } else if (*end == '\0') {
            p = end;
        } else {
            free(ids);
            return 0;
        }
    }

    *out_ids = ids;
    *out_len = len;
    return len > 0;
}

static int parse_positive_ull(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0) {
        return 0;
    }
    *out = value;
    return 1;
}

static int command_detokenize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const char *ids_text = NULL;
    unsigned int *ids = NULL;
    unsigned long long ids_len = 0;
    char out[4096];
    int i;
    int rc;

    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("detokenize"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--ids") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --ids requires a value\n");
                return 2;
            }
            ids_text = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown detokenize option: %s\n", argv[i]);
            return 2;
        }
    }
    if (!ids_text || !parse_id_list(ids_text, &ids, &ids_len)) {
        fprintf(stderr, "yvex: detokenize requires comma-separated --ids\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        free(ids);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_detokenize_ids(ctx.tokenizer, ids, ids_len, out, sizeof(out), &err);
    free(ids);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("text: ");
    print_quoted_bytes(out, (unsigned long long)strlen(out));
    printf("\n");
    printf("status: detokenized\n");

    close_tokenizer_context(&ctx);
    return 0;
}

static int command_engine(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_engine_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("engine"));
            return 0;
        }
        fprintf(stderr, "yvex: engine requires exactly one path\n");
        fprintf(stderr, "usage: yvex engine <path>\n");
        return 2;
    }

    rc = yvex_engine_open_path(&engine, argv[2], &err);
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, &err);
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine status: %s\n", yvex_engine_status_name(summary.status));
    printf("format: gguf\n");
    printf("architecture: %s\n", summary.architecture);
    printf("model_name: %s\n", summary.model_name);
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("known_tensor_bytes: %llu\n", summary.known_tensor_bytes);
    printf("unsupported_tensor_accounting: %llu\n", summary.unsupported_tensor_accounting);
    printf("tokenizer_model: %s\n", summary.tokenizer_model);
    printf("tokenizer_support: %s\n", summary.tokenizer_support);
    printf("graph_status: %s\n", summary.graph_status);
    printf("execution_ready: false\n");
    printf("reason: %s\n", yvex_engine_diagnostic_reason(engine));
    printf("status: engine-descriptor\n");

    yvex_engine_close(engine);
    return 0;
}

static int cli_parse_gguf_template_options(int argc, char **argv, int start,
                                           const char **template_path,
                                           const char **native_source,
                                           int *require_all)
{
    int i = start;

    *template_path = NULL;
    *native_source = NULL;
    *require_all = 0;
    while (i < argc) {
        if (strcmp(argv[i], "--require-all-template-tensors-in-native") == 0) {
            *require_all = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: gguf-template option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--template") == 0) {
            *template_path = argv[i + 1];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            *native_source = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown gguf-template option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void cli_print_template_issues(const yvex_gguf_template *tmpl)
{
    unsigned long long i;
    unsigned long long count = yvex_gguf_template_issue_count(tmpl);

    for (i = 0; i < count; ++i) {
        const yvex_gguf_template_issue *issue = yvex_gguf_template_issue_at(tmpl, i);
        if (!issue) {
            continue;
        }
        if (issue->tensor_name && issue->tensor_name[0] != '\0') {
            printf("issue %llu %s tensor=\"%s\" message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->tensor_name,
                   issue->message ? issue->message : "");
        } else {
            printf("issue %llu %s message=\"%s\"\n",
                   i,
                   yvex_gguf_template_issue_kind_name(issue->kind),
                   issue->message ? issue->message : "");
        }
    }
}

static int command_gguf_template(int argc, char **argv)
{
    yvex_gguf_template_options options;
    yvex_gguf_template *tmpl = NULL;
    yvex_gguf_template_summary summary;
    yvex_error err;
    const char *template_path;
    const char *native_source;
    int require_all;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("gguf-template"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: gguf-template requires inspect, validate, or compare\n");
        fprintf(stderr, "usage: yvex gguf-template inspect|validate --template FILE\n");
        return 2;
    }
    if (strcmp(argv[2], "inspect") != 0 && strcmp(argv[2], "validate") != 0 &&
        strcmp(argv[2], "compare") != 0) {
        fprintf(stderr, "yvex: unknown gguf-template subcommand: %s\n", argv[2]);
        return 2;
    }
    rc = cli_parse_gguf_template_options(argc, argv, 3, &template_path, &native_source, &require_all);
    if (rc != 0) {
        return rc;
    }
    if (!template_path) {
        fprintf(stderr, "yvex: gguf-template requires --template FILE\n");
        return 2;
    }
    if (strcmp(argv[2], "compare") == 0 && !native_source) {
        fprintf(stderr, "yvex: gguf-template compare requires --native-source DIR\n");
        return 2;
    }

    memset(&options, 0, sizeof(options));
    options.template_path = template_path;
    options.native_source_dir = native_source;
    options.compare_native = strcmp(argv[2], "compare") == 0;
    options.require_all_template_tensors_in_native = require_all;

    rc = yvex_gguf_template_open(&tmpl, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_gguf_template_get_summary(tmpl, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (strcmp(argv[2], "compare") == 0) {
        printf("gguf template: compare\n");
        printf("template: %s\n", template_path);
        printf("native_source: %s\n", native_source);
        printf("template_tensors: %llu\n", summary.tensor_count);
        printf("native_tensors: %llu\n", summary.native_tensor_count);
        printf("matched_exact: %llu\n", summary.matched_exact);
        printf("missing_in_native: %llu\n", summary.missing_in_native);
        printf("shape_mismatch: %llu\n", summary.shape_mismatch);
        printf("status: template-compare-%s\n",
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_VALID ? "valid" :
               summary.status == YVEX_GGUF_TEMPLATE_STATUS_INVALID ? "invalid" : "partial");
        if (summary.missing_in_native > 0 || summary.shape_mismatch > 0) {
            printf("reason: architecture adapter mapping requires OWI.4\n");
        }
        cli_print_template_issues(tmpl);
    } else if (strcmp(argv[2], "validate") == 0) {
        printf("gguf template: validate\n");
        printf("template: %s\n", template_path);
        printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
        printf("issues: %llu\n", summary.issue_count);
        cli_print_template_issues(tmpl);
    } else {
        printf("gguf template: inspect\n");
        printf("template: %s\n", template_path);
        printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
        printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
        printf("metadata_count: %llu\n", summary.metadata_count);
        printf("tensor_count: %llu\n", summary.tensor_count);
        printf("has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
        printf("known_roles: %llu\n", summary.known_role_count);
        printf("unknown_roles: %llu\n", summary.unknown_role_count);
        printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
    }

    yvex_gguf_template_close(tmpl);
    return 0;
}

static int command_gguf_emit(int argc, char **argv)
{
    yvex_gguf_emit_options options;
    yvex_gguf_emit_summary summary;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("gguf-emit"));
        return 0;
    }
    if (argc < 3 || strcmp(argv[2], "controlled") != 0) {
        fprintf(stderr, "yvex: gguf-emit requires subcommand controlled\n");
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--overwrite]\n");
        return 2;
    }

    options.tensor_name = "embed.weight";
    options.target_name = "token_embd.weight";
    options.model_name = "yvex-owned-gguf-test";
    options.architecture = "llama";
    options.transpose_2d = 1;

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: gguf-emit option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--out") == 0) {
            options.out_path = argv[++i];
        } else if (strcmp(argv[i], "--template") == 0) {
            options.template_path = argv[++i];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            options.native_source_dir = argv[++i];
        } else if (strcmp(argv[i], "--tensor-name") == 0) {
            options.tensor_name = argv[++i];
        } else if (strcmp(argv[i], "--target-name") == 0) {
            options.target_name = argv[++i];
        } else if (strcmp(argv[i], "--model-name") == 0) {
            options.model_name = argv[++i];
        } else if (strcmp(argv[i], "--arch") == 0) {
            options.architecture = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown gguf-emit option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help gguf-emit' for usage.\n");
            return 2;
        }
    }

    if (!options.out_path) {
        fprintf(stderr, "yvex: gguf-emit controlled requires --out FILE\n");
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--overwrite]\n");
        return 2;
    }

    rc = yvex_gguf_emit_controlled(&options, &summary, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("gguf emit: controlled\n");
    printf("out: %s\n", summary.out_path ? summary.out_path : "");
    printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
    printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("tensor_payload_bytes: %llu\n", summary.tensor_payload_bytes);
    printf("alignment: %llu\n", summary.alignment);
    printf("roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    printf("status: %s\n", yvex_gguf_emit_status_name(summary.status));
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

    print_command_help(stdout, command);
    return 0;
}

static int parse_imatrix_create_options(int argc, char **argv,
                                        yvex_imatrix_manifest_options *options,
                                        const char **out_path)
{
    int i = 3;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--name") == 0) options->name = argv[i + 1];
        else if (strcmp(argv[i], "--arch") == 0) options->architecture = argv[i + 1];
        else if (strcmp(argv[i], "--source-manifest") == 0) options->source_manifest_path = argv[i + 1];
        else if (strcmp(argv[i], "--quant-policy") == 0) options->quant_policy_path = argv[i + 1];
        else if (strcmp(argv[i], "--imatrix") == 0) options->imatrix_path = argv[i + 1];
        else if (strcmp(argv[i], "--format") == 0) options->format = yvex_imatrix_format_from_name(argv[i + 1]);
        else if (strcmp(argv[i], "--status") == 0) options->status = yvex_imatrix_status_from_name(argv[i + 1]);
        else if (strcmp(argv[i], "--dataset") == 0) options->calibration_dataset = argv[i + 1];
        else if (strcmp(argv[i], "--command") == 0) options->calibration_command = argv[i + 1];
        else if (strcmp(argv[i], "--producer") == 0) options->producer = argv[i + 1];
        else if (strcmp(argv[i], "--out") == 0) *out_path = argv[i + 1];
        else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int parse_imatrix_manifest_option(int argc, char **argv, const char **manifest_path)
{
    int i = 3;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: imatrix option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--manifest") == 0) {
            *manifest_path = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown imatrix option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static void print_imatrix_summary(const char *mode,
                                  const char *manifest_path,
                                  const yvex_imatrix_summary *summary)
{
    printf("imatrix: %s\n", mode);
    if (manifest_path) printf("manifest: %s\n", manifest_path);
    printf("name: %s\n", summary->name ? summary->name : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("format: %s\n", yvex_imatrix_format_name(summary->format));
    printf("status: %s\n", yvex_imatrix_status_name(summary->status));
    printf("file_exists: %s\n", summary->file_exists ? "yes" : "no");
    printf("source_manifest: %s\n", summary->source_manifest_path ? summary->source_manifest_path : "");
    printf("quant_policy: %s\n", summary->quant_policy_path ? summary->quant_policy_path : "");
    printf("imatrix: %s\n", summary->imatrix_path ? summary->imatrix_path : "");
}

static int command_imatrix(int argc, char **argv)
{
    yvex_error err;
    yvex_imatrix_manifest *manifest = NULL;
    yvex_imatrix_summary summary;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("imatrix"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: imatrix requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(argv[2], "create") == 0) {
        yvex_imatrix_manifest_options options;
        const char *out_path = NULL;

        memset(&options, 0, sizeof(options));
        rc = parse_imatrix_create_options(argc, argv, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.imatrix_path || !out_path ||
            options.format == YVEX_IMATRIX_FORMAT_UNKNOWN ||
            options.status == YVEX_IMATRIX_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: imatrix create requires --name --arch --imatrix --format --status --out\n");
            return 2;
        }
        rc = yvex_imatrix_manifest_create(&manifest, &options, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_imatrix_manifest_validate(manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_write_json(out_path, manifest, &err);
        if (rc == YVEX_OK) rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("imatrix manifest: written\n");
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("format: %s\n", yvex_imatrix_format_name(summary.format));
        printf("status: %s\n", yvex_imatrix_status_name(summary.status));
        printf("file_exists: %s\n", summary.file_exists ? "yes" : "no");
        printf("out: %s\n", out_path);
        printf("status: imatrix-manifest-written\n");
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    if (strcmp(argv[2], "inspect") == 0 || strcmp(argv[2], "validate") == 0) {
        const char *manifest_path = NULL;

        rc = parse_imatrix_manifest_option(argc, argv, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: imatrix %s requires --manifest FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_imatrix_manifest_open(&manifest, manifest_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(argv[2], "validate") == 0) {
            rc = yvex_imatrix_manifest_validate(manifest, &err);
            if (rc != YVEX_OK) {
                yvex_imatrix_manifest_close(manifest);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_imatrix_manifest_get_summary(manifest, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_imatrix_manifest_close(manifest);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        print_imatrix_summary(argv[2], manifest_path, &summary);
        if (strcmp(argv[2], "validate") == 0) {
            printf("issues: %llu\n", summary.issue_count);
            printf("requires_imatrix_rules: %llu\n", summary.requires_imatrix_rule_count);
            printf("covered_rules: %llu\n", summary.covered_rule_count);
            printf("uncovered_rules: %llu\n", summary.uncovered_rule_count);
            printf("status: imatrix-%s\n",
                   summary.issue_count == 0 ? "valid" :
                   (summary.file_exists ? "partial" : "invalid"));
        } else {
            printf("status: imatrix-manifest\n");
        }
        yvex_imatrix_manifest_close(manifest);
        return 0;
    }

    fprintf(stderr, "yvex: unknown imatrix subcommand: %s\n", argv[2]);
    return 2;
}

static int command_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("name: YVEX\n");
    printf("version: %s\n", yvex_version_string());
    printf("language: C\n");
    printf("interface: CLI-only\n");
    printf("status: M0 fixture weight materialization\n");
    printf("library: libyvex.a\n");
    printf("filesystem: implemented\n");
    printf("artifact: open/read implemented\n");
    printf("gguf: metadata/tensor directory parsing implemented\n");
    printf("model: descriptor-only implemented\n");
    printf("tokenizer: fixture encode/decode implemented\n");
    printf("prompt: default renderer implemented\n");
    printf("graph: partial planning implemented\n");
    printf("planner: estimate-only implemented\n");
    printf("backend: CPU reference implemented\n");
    printf("backend_cuda: tensor movement and F32 embed implemented when CUDA is available\n");
    printf("weights: fixture materialization implemented\n");
    printf("engine: runtime object skeleton implemented\n");
    printf("session: lifecycle skeleton implemented\n");
    printf("run: accepted-only runtime shell implemented\n");
    printf("chat: accepted-only REPL shell implemented\n");
    printf("metrics: runtime collector implemented\n");
    printf("trace: JSONL writer implemented\n");
    printf("profile: JSON writer implemented\n");
    printf("run_artifacts: metrics/trace/profile files implemented\n");
    printf("source_manifest: provenance JSON writer implemented\n");
    printf("native_weights: safetensors header inventory implemented\n");
    printf("gguf_template: contract validator implemented\n");
    printf("gguf_emit: controlled GGUF writer implemented\n");
    printf("weight_mapping: tensor adapter contract implemented\n");
    printf("quant_policy: manifest validator implemented\n");
    printf("imatrix: calibration artifact manifest implemented\n");
    printf("server_binary: yvexd shell implemented\n");
    printf("server_endpoints: health/metrics/models status implemented\n");
    printf("server_generation: not implemented\n");
    printf("kv: unavailable skeleton implemented\n");
    printf("logits: unavailable skeleton implemented\n");
    printf("generation: unsupported\n");
    printf("inference: not implemented\n");
    printf("cuda: available when local driver/device probe succeeds\n");
    printf("server: yvexd status shell implemented\n");
    return 0;
}

static int command_inspect(int argc, char **argv)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_model_descriptor *model = NULL;
    yvex_gguf_probe probe;
    const yvex_gguf_header *header;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("inspect"));
            return 0;
        }
        fprintf(stderr, "yvex: inspect requires exactly one path\n");
        fprintf(stderr, "usage: yvex inspect <path>\n");
        return 2;
    }

    rc = open_artifact_for_gguf(argv[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_probe_file(artifact, &probe, &err);
    if (rc == YVEX_OK && !probe.is_gguf) {
        printf("format: unknown\n");
        printf("status: unsupported\n");
        yvex_artifact_close(artifact);
        return 5;
    }

    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_UNSUPPORTED) {
            printf("format: gguf\n");
            printf("status: unsupported\n");
        }
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&tensors, gguf, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&model, gguf, tensors, &err);
    }
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(gguf);
        printf("format: gguf\n");
        printf("version: %u\n", header->version);
        printf("metadata_count: %llu\n", header->metadata_count);
        printf("tensor_count: %llu\n", header->tensor_count);
        printf("tensor_data_offset: %llu\n", yvex_gguf_tensor_data_offset(gguf));
        printf("alignment: %u\n", yvex_gguf_alignment(gguf));
        printf("architecture: %s\n", yvex_arch_name(yvex_model_arch(model)));
        printf("model_name: %s\n", yvex_model_name(model));
        printf("known_tensor_bytes: %llu\n", yvex_model_total_storage_bytes(model));
        printf("unsupported_tensor_accounting: %llu\n",
               yvex_model_unsupported_tensor_accounting_count(model));
        printf("status: descriptor-only\n");
        yvex_model_descriptor_close(model);
        yvex_tensor_table_close(tensors);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return 0;
    }

    yvex_model_descriptor_close(model);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return print_yvex_error(&err, exit_for_status(rc));
}

static int command_metadata(int argc, char **argv)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    const yvex_gguf_header *header;
    yvex_error err;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("metadata"));
            return 0;
        }
        fprintf(stderr, "yvex: metadata requires exactly one path\n");
        fprintf(stderr, "usage: yvex metadata <path>\n");
        return 2;
    }

    rc = open_artifact_for_gguf(argv[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc != YVEX_OK) {
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    header = yvex_gguf_header_view(gguf);
    printf("format: gguf\n");
    printf("version: %u\n", header->version);
    printf("metadata_count: %llu\n", yvex_gguf_metadata_count(gguf));
    printf("\n");

    for (i = 0; i < yvex_gguf_metadata_count(gguf); ++i) {
        const char *key = yvex_gguf_metadata_key(gguf, i);
        const yvex_gguf_value *value = yvex_gguf_metadata_value(gguf, i);
        printf("%s = ", key ? key : "");
        print_metadata_value(value);
        printf("\n");
    }

    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

static int command_materialize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary summary;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ctx, 0, sizeof(ctx));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("materialize"));
        return 0;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a file\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--require-all") == 0) {
            materialize_options.require_all_tensors = 1;
        } else if (strcmp(argv[i], "--allow-unsupported-dtype") == 0) {
            materialize_options.allow_unsupported_dtype = 1;
        } else {
            fprintf(stderr, "yvex: unknown materialize option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help materialize' for usage.\n");
            return 2;
        }
    }

    if (!model_path || !backend_name) {
        fprintf(stderr, "yvex: materialize requires --model FILE and --backend cpu|cuda\n");
        fprintf(stderr, "usage: yvex materialize --model FILE --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]\n");
        return 2;
    }

    if (strcmp(backend_name, "cpu") == 0) {
        backend_options.kind = YVEX_BACKEND_KIND_CPU;
    } else if (strcmp(backend_name, "cuda") == 0) {
        backend_options.kind = YVEX_BACKEND_KIND_CUDA;
    } else {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = open_model_context(model_path, &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("materialization status: unsupported\n");
        printf("backend: %s\n", backend_name);
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: weights-unsupported\n");
        close_model_context(&ctx);
        return 5;
    }
    if (rc != YVEX_OK) {
        close_model_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    materialize_options.backend_name = backend_name;
    rc = yvex_weight_table_materialize(&weights,
                                       ctx.artifact,
                                       ctx.gguf,
                                       ctx.table,
                                       backend,
                                       &materialize_options,
                                       &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("materialization status: unsupported\n");
        printf("backend: %s\n", backend_name);
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: weights-unsupported\n");
        yvex_backend_close(backend);
        close_model_context(&ctx);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        close_model_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_weight_table_get_summary(weights, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        close_model_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("materialization status: %s\n", yvex_weight_status_name(summary.status));
    printf("model: %s\n", yvex_model_name(ctx.model)[0] ? yvex_model_name(ctx.model) : "unknown");
    printf("backend: %s\n", backend_name);
    printf("tensors_total: %llu\n", summary.tensors_total);
    printf("tensors_materialized: %llu\n", summary.tensors_materialized);
    printf("tensors_failed: %llu\n", summary.tensors_failed);
    printf("bytes_total: %llu\n", summary.bytes_total);
    printf("bytes_materialized: %llu\n", summary.bytes_materialized);
    printf("backend_allocated_bytes: %llu\n", summary.backend_allocated_bytes);
    printf("execution_ready: false\n");
    printf("reason: graph partial; materialized weights do not imply executable inference\n");
    printf("status: %s\n", summary.status == YVEX_WEIGHT_STATUS_MATERIALIZED
           ? "weights-materialized"
           : "weights-partial");

    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return 0;
}

static int command_native_weights(int argc, char **argv)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_summary summary;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.recursive = 1;

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("native-weights"));
        return 0;
    }

    i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: native-weights option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--source") == 0) {
            options.source_dir = argv[i + 1];
        } else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(argv[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid native-weights limit: %s\n", argv[i + 1]);
                return 2;
            }
        } else if (strcmp(argv[i], "--tensor") == 0) {
            tensor_name = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown native-weights option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.source_dir) {
        fprintf(stderr, "yvex: native-weights requires --source DIR\n");
        return 2;
    }

    rc = yvex_native_weight_table_open(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_native_weight_table_summary(table, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(table);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (json) {
        printf("{\n");
        printf("  \"schema\": \"yvex.native_weights.v1\",\n");
        printf("  \"source\": \"%s\",\n", options.source_dir);
        printf("  \"summary\": {\n");
        printf("    \"shard_count\": %llu,\n", summary.shard_count);
        printf("    \"tensor_count\": %llu,\n", summary.tensor_count);
        printf("    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
        printf("    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
        printf("    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
        printf("  }\n");
        printf("}\n");
        yvex_native_weight_table_close(table);
        return 0;
    }

    printf("native weights: safetensors\n");
    printf("source: %s\n", options.source_dir);
    printf("shards: %llu\n", summary.shard_count);
    printf("tensors: %llu\n", summary.tensor_count);
    printf("total_tensor_bytes: %llu\n", summary.total_tensor_bytes);
    printf("unknown_dtype_count: %llu\n", summary.unknown_dtype_count);
    printf("malformed_shard_count: %llu\n", summary.malformed_shard_count);
    printf("\n");

    if (tensor_name) {
        const yvex_native_weight_info *row = yvex_native_weight_table_find(table, tensor_name);
        if (!row) {
            yvex_native_weight_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        printf("0 %s shard=%s dtype=%s rank=%u shape=",
               row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
        print_native_dims(row->dims, row->rank);
        printf(" bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
    } else {
        unsigned long long count = yvex_native_weight_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;
        for (idx = 0; idx < n; ++idx) {
            const yvex_native_weight_info *row = yvex_native_weight_table_at(table, idx);
            printf("%llu %s shard=%s dtype=%s rank=%u shape=",
                   idx, row->name, row->shard_path, yvex_native_dtype_name(row->dtype), row->rank);
            print_native_dims(row->dims, row->rank);
            printf(" bytes=%llu offsets=[%llu,%llu]\n", row->data_bytes, row->data_start, row->data_end);
        }
    }
    printf("status: %s\n", summary.shard_count == 0 ? "native-weights-empty" : "native-weights");
    yvex_native_weight_table_close(table);
    return 0;
}

static int command_tensor_map(int argc, char **argv)
{
    yvex_weight_mapping_options options;
    yvex_weight_mapping_table *table = NULL;
    yvex_error err;
    const char *tensor_name = NULL;
    unsigned long long limit = 20;
    unsigned long long mapped = 0;
    unsigned long long unmapped = 0;
    unsigned long long shape_mismatch = 0;
    int json = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("tensor-map"));
        return 0;
    }

    i = 2;
    while (i < argc) {
        if (strcmp(argv[i], "--json") == 0) {
            json = 1;
            i++;
            continue;
        }
        if (strcmp(argv[i], "--require-all-native-mapped") == 0) {
            options.require_all_native_mapped = 1;
            i++;
            continue;
        }
        if (strcmp(argv[i], "--require-all-template-matched") == 0) {
            options.require_all_template_matched = 1;
            i++;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: tensor-map option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--arch") == 0) {
            options.architecture = argv[i + 1];
        } else if (strcmp(argv[i], "--native-source") == 0) {
            options.native_source_dir = argv[i + 1];
        } else if (strcmp(argv[i], "--template") == 0) {
            options.template_path = argv[i + 1];
            options.compare_template = 1;
        } else if (strcmp(argv[i], "--tensor") == 0) {
            tensor_name = argv[i + 1];
        } else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            limit = strtoull(argv[i + 1], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid tensor-map limit: %s\n", argv[i + 1]);
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown tensor-map option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }

    if (!options.architecture || !options.native_source_dir) {
        fprintf(stderr, "yvex: tensor-map requires --arch NAME and --native-source DIR\n");
        return 2;
    }

    rc = yvex_weight_mapping_table_build(&table, &options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    for (i = 0; (unsigned long long)i < yvex_weight_mapping_table_count(table); ++i) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, (unsigned long long)i);
        if (!row) continue;
        if (row->status == YVEX_WEIGHT_MAPPING_STATUS_MAPPED) mapped++;
        else if (row->status == YVEX_WEIGHT_MAPPING_STATUS_SHAPE_MISMATCH) shape_mismatch++;
        else unmapped++;
    }

    if (json) {
        printf("{\n");
        printf("  \"schema\": \"yvex.tensor_map.v1\",\n");
        printf("  \"architecture\": \"%s\",\n", options.architecture);
        printf("  \"native_source\": \"%s\",\n", options.native_source_dir);
        printf("  \"native_tensors\": %llu,\n", yvex_weight_mapping_table_count(table));
        printf("  \"mapped\": %llu,\n", mapped);
        printf("  \"unmapped\": %llu,\n", unmapped);
        printf("  \"shape_mismatch\": %llu\n", shape_mismatch);
        printf("}\n");
        yvex_weight_mapping_table_close(table);
        return 0;
    }

    printf("tensor map: %s\n", options.architecture);
    printf("native_source: %s\n", options.native_source_dir);
    if (options.template_path) {
        printf("template: %s\n", options.template_path);
    }
    printf("native_tensors: %llu\n", yvex_weight_mapping_table_count(table));
    printf("mapped: %llu\n", mapped);
    printf("unmapped: %llu\n", unmapped);
    printf("shape_mismatch: %llu\n", shape_mismatch);
    printf("\n");

    if (tensor_name) {
        const yvex_weight_mapping_info *row = yvex_weight_mapping_table_find_native(table, tensor_name);
        if (!row) {
            yvex_weight_mapping_table_close(table);
            fprintf(stderr, "yvex: native tensor not found: %s\n", tensor_name);
            return 4;
        }
        printf("0 native=%s role=%s target=%s status=%s native_shape=",
               row->native_name, yvex_tensor_role_name(row->role), row->target_name,
               yvex_weight_mapping_status_name(row->status));
        print_native_dims(row->native_dims, row->native_rank);
        printf(" target_shape=");
        if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
        else printf("unknown");
        printf(" transform=%s", row->requires_transpose ? "transpose" : "none");
        if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
            printf(" issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
        }
        printf("\n");
    } else {
        unsigned long long count = yvex_weight_mapping_table_count(table);
        unsigned long long n = limit < count ? limit : count;
        unsigned long long idx;

        for (idx = 0; idx < n; ++idx) {
            const yvex_weight_mapping_info *row = yvex_weight_mapping_table_at(table, idx);
            printf("%llu native=%s role=%s target=%s status=%s native_shape=",
                   idx, row->native_name, yvex_tensor_role_name(row->role), row->target_name,
                   yvex_weight_mapping_status_name(row->status));
            print_native_dims(row->native_dims, row->native_rank);
            printf(" target_shape=");
            if (row->target_rank > 0) print_native_dims(row->target_dims, row->target_rank);
            else printf("unknown");
            printf(" transform=%s", row->requires_transpose ? "transpose" : "none");
            if (row->issue != YVEX_WEIGHT_MAPPING_ISSUE_NONE) {
                printf(" issue=%s", yvex_weight_mapping_issue_kind_name(row->issue));
            }
            printf("\n");
        }
    }
    printf("status: tensor-map\n");
    yvex_weight_mapping_table_close(table);
    return 0;
}

static void print_quant_policy_rules(const yvex_quant_policy *policy)
{
    unsigned long long i;

    for (i = 0; i < yvex_quant_policy_rule_count(policy); ++i) {
        const yvex_quant_policy_rule *rule = yvex_quant_policy_rule_at(policy, i);
        if (!rule) continue;
        printf("%llu selector=%s:%s qtype=%s storage_supported=%s compute_supported=%s requires_imatrix=%s\n",
               i,
               yvex_quant_selector_kind_name(rule->selector_kind),
               rule->selector,
               yvex_quant_qtype_name(rule->qtype),
               rule->storage_supported ? "yes" : "no",
               rule->compute_supported ? "yes" : "no",
               rule->requires_imatrix ? "yes" : "no");
    }
}

static int parse_quant_policy_common(int argc, char **argv, int start,
                                     const char **policy_path,
                                     const char **template_path,
                                     const char **arch,
                                     const char **out_path)
{
    int i = start;

    while (i < argc) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-policy option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--policy") == 0) {
            *policy_path = argv[i + 1];
        } else if (strcmp(argv[i], "--template") == 0) {
            *template_path = argv[i + 1];
        } else if (strcmp(argv[i], "--arch") == 0) {
            *arch = argv[i + 1];
        } else if (strcmp(argv[i], "--out") == 0) {
            *out_path = argv[i + 1];
        } else {
            fprintf(stderr, "yvex: unknown quant-policy option: %s\n", argv[i]);
            return 2;
        }
        i += 2;
    }
    return 0;
}

static int command_quant_policy(int argc, char **argv)
{
    const char *policy_path = NULL;
    const char *template_path = NULL;
    const char *arch = NULL;
    const char *out_path = NULL;
    yvex_quant_policy *policy = NULL;
    yvex_quant_policy_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("quant-policy"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: quant-policy requires inspect, validate, or derive\n");
        return 2;
    }
    rc = parse_quant_policy_common(argc, argv, 3, &policy_path, &template_path, &arch, &out_path);
    if (rc != 0) return rc;

    if (strcmp(argv[2], "derive") == 0) {
        if (!template_path || !arch || !out_path) {
            fprintf(stderr, "yvex: quant-policy derive requires --template FILE --arch NAME --out FILE\n");
            return 2;
        }
        rc = yvex_quant_policy_create_from_template(&policy, template_path, arch, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = yvex_quant_policy_write_json(out_path, policy, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("quant policy: derived\n");
        printf("architecture: %s\n", summary.architecture);
        printf("template: %s\n", template_path);
        printf("rules: %llu\n", summary.rule_count);
        printf("requires_imatrix: %llu\n", summary.requires_imatrix_count);
        printf("out: %s\n", out_path);
        printf("status: quant-policy-written\n");
        yvex_quant_policy_close(policy);
        return 0;
    }

    if (strcmp(argv[2], "inspect") == 0 || strcmp(argv[2], "validate") == 0) {
        if (!policy_path) {
            fprintf(stderr, "yvex: quant-policy %s requires --policy FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_quant_policy_open(&policy, policy_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (strcmp(argv[2], "validate") == 0) {
            rc = yvex_quant_policy_validate(policy, template_path, &err);
            if (rc != YVEX_OK) {
                yvex_quant_policy_close(policy);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
        rc = yvex_quant_policy_get_summary(policy, &summary, &err);
        if (rc != YVEX_OK) {
            yvex_quant_policy_close(policy);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("quant policy: %s\n", argv[2]);
        printf("policy: %s\n", policy_path);
        if (template_path) printf("template: %s\n", template_path);
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("rules: %llu\n", summary.rule_count);
        printf("issues: %llu\n", summary.issue_count);
        printf("requires_imatrix: %llu\n", summary.requires_imatrix_count);
        printf("storage_supported: %llu\n", summary.storage_supported_count);
        printf("compute_supported: %llu\n", summary.compute_supported_count);
        printf("\n");
        if (strcmp(argv[2], "inspect") == 0) {
            print_quant_policy_rules(policy);
        }
        printf("status: %s\n", yvex_quant_policy_status_name(summary.status));
        yvex_quant_policy_close(policy);
        return 0;
    }

    fprintf(stderr, "yvex: unknown quant-policy subcommand: %s\n", argv[2]);
    return 2;
}

static int command_paths(int argc, char **argv)
{
    const char *project_root = NULL;
    int want_run = 0;
    int want_create = 0;
    int i;
    int rc;
    yvex_paths paths;
    yvex_run_dir run;
    yvex_error err;

    yvex_error_clear(&err);

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--project") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --project requires a path\n");
                return 2;
            }
            project_root = argv[++i];
        } else if (strcmp(argv[i], "--run") == 0) {
            want_run = 1;
        } else if (strcmp(argv[i], "--create") == 0) {
            want_create = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_command_help(stdout, find_command("paths"));
            return 0;
        } else {
            fprintf(stderr, "yvex: unknown paths option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help paths' for usage.\n");
            return 2;
        }
    }

    if (want_create && !want_run) {
        fprintf(stderr, "yvex: --create requires --run\n");
        return 2;
    }

    rc = project_root ? yvex_paths_project(&paths, project_root, &err) : yvex_paths_default(&paths, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (!want_run) {
        rc = yvex_paths_print(&paths, stdout, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
        return 0;
    }

    rc = yvex_run_dir_prepare(&run, &paths, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }

    if (want_create) {
        rc = yvex_run_dir_create(&run, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
        }
    }

    rc = yvex_run_dir_print(&run, stdout, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, rc == YVEX_ERR_INVALID_ARG ? 2 : 3);
    }
    return 0;
}

static int command_graph(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_graph *graph = NULL;
    yvex_graph_build_options options;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.include_prefill_path = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("graph"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                fprintf(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown graph option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help graph' for usage.\n");
            return 2;
        }
    }

    rc = open_model_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_graph_build_for_model(&graph, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_graph_dump(graph, stdout, &err);
    }

    yvex_graph_close(graph);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_plan(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    yvex_error err;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.backend_name = "cpu";

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("plan"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--seq") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.sequence_length)) {
                fprintf(stderr, "yvex: --seq requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            options.backend_name = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown plan option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help plan' for usage.\n");
            return 2;
        }
    }

    rc = open_model_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, &err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_dump(plan, stdout, &err);
    }

    yvex_plan_close(plan);
    close_model_context(&ctx);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_prompt(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_prompt_message messages[16];
    unsigned long long message_count = 0;
    yvex_prompt_options options;
    yvex_rendered_prompt rendered;
    yvex_tokens tokens;
    yvex_error err;
    const char *chat_template = NULL;
    unsigned long long chat_template_len = 0;
    int want_tokens = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&rendered, 0, sizeof(rendered));
    options.add_bos = 0;
    options.add_eos = 0;
    options.add_generation_prompt = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("prompt"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        yvex_prompt_role role;
        if (strcmp(argv[i], "--system") == 0) {
            role = YVEX_PROMPT_ROLE_SYSTEM;
        } else if (strcmp(argv[i], "--user") == 0) {
            role = YVEX_PROMPT_ROLE_USER;
        } else if (strcmp(argv[i], "--assistant") == 0) {
            role = YVEX_PROMPT_ROLE_ASSISTANT;
        } else if (strcmp(argv[i], "--no-generation-prompt") == 0) {
            options.add_generation_prompt = 0;
            continue;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            want_tokens = 1;
            continue;
        } else {
            fprintf(stderr, "yvex: unknown prompt option: %s\n", argv[i]);
            return 2;
        }

        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: prompt option %s requires text\n", argv[i]);
            return 2;
        }
        if (message_count >= sizeof(messages) / sizeof(messages[0])) {
            fprintf(stderr, "yvex: too many prompt messages\n");
            return 2;
        }
        messages[message_count].role = role;
        messages[message_count].content = argv[++i];
        message_count += 1;
    }

    if (message_count == 0) {
        fprintf(stderr, "yvex: prompt requires at least one message\n");
        return 2;
    }

    rc = open_tokenizer_context(argv[2], &ctx, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_prompt_render(&rendered, ctx.tokenizer, messages, message_count, &options, &err);
    if (rc != YVEX_OK) {
        close_tokenizer_context(&ctx);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("template: yvex-default\n");
    printf("chat_template_metadata: %s\n",
           yvex_tokenizer_chat_template(ctx.tokenizer, &chat_template, &chat_template_len) == YVEX_OK
               ? "present-unsupported"
               : "absent");
    printf("rendered_bytes: %llu\n", rendered.len);
    printf("rendered:\n%s", rendered.text);

    if (want_tokens) {
        rc = yvex_tokenize_text(ctx.tokenizer, rendered.text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_rendered_prompt_free(&rendered);
            close_tokenizer_context(&ctx);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        printf("tokens: %llu\n", tokens.len);
        print_token_ids(&tokens);
        yvex_tokens_free(&tokens);
    }
    printf("status: rendered\n");

    yvex_rendered_prompt_free(&rendered);
    close_tokenizer_context(&ctx);
    return 0;
}

static void trim_line(char *line)
{
    size_t len;

    if (!line) {
        return;
    }
    len = strlen(line);
    while (len > 0 && (line[len - 1u] == '\n' || line[len - 1u] == '\r')) {
        line[len - 1u] = '\0';
        len -= 1u;
    }
}

static void cli_copy_text(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0) {
        return;
    }
    snprintf(dst, cap, "%s", src ? src : "");
    dst[cap - 1u] = '\0';
}

static void cli_set_result_artifacts(yvex_chat_accept_result *result,
                                     const yvex_run_artifacts *artifacts)
{
    if (!result || !artifacts) {
        return;
    }
    cli_copy_text(result->run_id, sizeof(result->run_id), artifacts->run_id);
    cli_copy_text(result->run_dir, sizeof(result->run_dir), artifacts->run_dir);
    if (artifacts->has_metrics) {
        cli_copy_text(result->metrics_out, sizeof(result->metrics_out), artifacts->metrics_path);
    }
    if (artifacts->has_trace) {
        cli_copy_text(result->trace_out, sizeof(result->trace_out), artifacts->trace_path);
    }
    if (artifacts->has_profile) {
        cli_copy_text(result->profile_out, sizeof(result->profile_out), artifacts->profile_path);
    }
}

static int cli_write_observability_files(const char *command_name,
                                         const char *model_name,
                                         const char *backend_name,
                                         const char *status,
                                         const yvex_run_artifacts *artifacts,
                                         const yvex_metrics *metrics,
                                         int argc,
                                         char **argv,
                                         yvex_error *err)
{
    yvex_profile_summary profile;
    yvex_metric_counters counters;
    int rc;

    if (!artifacts || !metrics) {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_run_artifacts_write_command(artifacts, argc, argv, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (artifacts->has_metrics) {
        rc = yvex_metrics_write_json(artifacts->metrics_path, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }
    if (artifacts->has_profile) {
        rc = yvex_metrics_get_counters(metrics, &counters, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        memset(&profile, 0, sizeof(profile));
        profile.run_id = artifacts->run_id;
        profile.command = command_name;
        profile.model_name = model_name;
        profile.backend_name = backend_name;
        profile.status = status;
        profile.execution_ready = 0;
        profile.counters = counters;
        rc = yvex_profile_write_json(artifacts->profile_path, &profile, metrics, err);
        if (rc != YVEX_OK) {
            return rc;
        }
    }

    yvex_error_clear(err);
    return YVEX_OK;
}

static int command_run(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_chat_accept_result result;
    yvex_engine_summary engine_summary;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *prompt_text = NULL;
    const char *system_text = NULL;
    const char *output = "plain";
    const char *status_line = "off";
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    int save_run = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        print_command_help(stdout, find_command("run"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --prompt requires a value\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else if (strcmp(argv[i], "--system") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --system requires a value\n");
                return 2;
            }
            system_text = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --output requires plain or json\n");
                return 2;
            }
            output = argv[++i];
            if (strcmp(output, "plain") != 0 && strcmp(output, "json") != 0) {
                fprintf(stderr, "yvex: --output must be plain or json\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--status-line") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
            status_line = argv[++i];
            if (strcmp(status_line, "auto") != 0 && strcmp(status_line, "off") != 0 &&
                strcmp(status_line, "always") != 0) {
                fprintf(stderr, "yvex: --status-line requires auto, off, or always\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown run option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help run' for usage.\n");
            return 2;
        }
    }

    if (!model_path) {
        fprintf(stderr, "yvex: --model is required for yvex run in I0\n");
        return 2;
    }
    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex run in I0\n");
        return 2;
    }
    if (!prompt_text) {
        fprintf(stderr, "yvex: --prompt is required for yvex run in I0\n");
        return 2;
    }

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("run status: backend-unsupported\n");
        printf("backend: cuda\n");
        printf("execution_ready: false\n");
        printf("reason: %s\n", yvex_error_message(&err));
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    if (yvex_engine_get_summary(runtime.engine, &engine_summary, &err) == YVEX_OK) {
        (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                           engine_summary.unsupported_tensor_accounting, &err);
    }

    rc = yvex_chat_runtime_accept_user_text(&runtime, system_text, prompt_text, &result, &err);
    if (rc != YVEX_OK) {
        yvex_chat_runtime_close(&runtime);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "accepted-only",
                          "decode runtime is not implemented in I0", &err);
    cli_set_result_artifacts(&result, &artifacts);

    if (strcmp(status_line, "always") == 0) {
        (void)yvex_status_line_print(stderr, "accept", result.prompt_tokens, result.position);
    }

    if (strcmp(output, "json") == 0) {
        (void)yvex_run_command_json(stdout, &result);
    } else {
        (void)yvex_run_command_plain(stdout, &result);
    }

    yvex_trace_close(trace);
    rc = cli_write_observability_files("run", result.model_name, result.backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

static int command_session(int argc, char **argv)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_session_options session_options;
    yvex_session_summary summary;
    yvex_backend_options backend_options;
    yvex_tokens tokens;
    yvex_error err;
    const char *backend_name = "cpu";
    const char *text = NULL;
    int accept_tokens = 0;
    int tokenized = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&session_options, 0, sizeof(session_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&tokens, 0, sizeof(tokens));
    session_options.allow_partial_graph = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("session"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &session_options.context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--text") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires a value\n");
                return 2;
            }
            text = argv[++i];
        } else if (strcmp(argv[i], "--accept-tokens") == 0) {
            accept_tokens = 1;
        } else {
            fprintf(stderr, "yvex: unknown session option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help session' for usage.\n");
            return 2;
        }
    }

    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_engine_open_path(&engine, argv[2], &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        yvex_engine_close(engine);
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (text) {
        rc = yvex_tokenize_text(yvex_engine_tokenizer(engine), text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_session_close(session);
            yvex_backend_close(backend);
            yvex_engine_close(engine);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        tokenized = 1;
        if (accept_tokens) {
            rc = yvex_session_accept_tokens(session, &tokens, &err);
            if (rc != YVEX_OK) {
                yvex_tokens_free(&tokens);
                yvex_session_close(session);
                yvex_backend_close(backend);
                yvex_engine_close(engine);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    rc = yvex_session_get_summary(session, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_tokens_free(&tokens);
        yvex_session_close(session);
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("engine_status: %s\n", summary.engine_status);
    printf("backend: %s\n", summary.backend_kind);
    printf("backend_status: %s\n", summary.backend_status);
    printf("session_state: %s\n", yvex_session_state_name(summary.state));
    printf("context_length: %llu\n", summary.context_length);
    printf("position: %llu\n", summary.position);
    printf("accepted_tokens: %llu\n", summary.accepted_tokens);
    printf("kv_status: %s\n", summary.kv_status);
    printf("kv_bytes: %llu\n", summary.kv_bytes);
    printf("logits_status: %s\n", summary.logits_status);
    printf("execution_ready: false\n");
    printf("reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        printf("tokens: %llu\n", tokens.len);
    }
    printf("status: %s\n", accept_tokens ? "session-token-accepted" : "session-created");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    return 0;
}

static void print_chat_slash_help(FILE *fp)
{
    fprintf(fp, "commands:\n");
    fprintf(fp, "  /help\n");
    fprintf(fp, "  /status\n");
    fprintf(fp, "  /model\n");
    fprintf(fp, "  /backend\n");
    fprintf(fp, "  /tokens\n");
    fprintf(fp, "  /reset\n");
    fprintf(fp, "  /quit\n");
}

static void print_chat_model(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_engine_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_engine_get_summary(runtime->engine, &summary, &err) == YVEX_OK) {
        fprintf(fp, "model: %s\n", summary.model_name);
        fprintf(fp, "architecture: %s\n", summary.architecture);
        fprintf(fp, "tokenizer: %s\n", summary.tokenizer_model);
    }
}

static void print_chat_backend(FILE *fp, const yvex_chat_runtime *runtime)
{
    fprintf(fp, "backend: %s\n", yvex_backend_kind_name(yvex_backend_kind_of(runtime->backend)));
    fprintf(fp, "status: %s\n", yvex_backend_status_name(yvex_backend_status_of(runtime->backend)));
    fprintf(fp, "capabilities:\n");
    fprintf(fp, "  tensor_alloc: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_ALLOC) ? "yes" : "no");
    fprintf(fp, "  tensor_read_write: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE) ? "yes" : "no");
    fprintf(fp, "  op_embed: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_EMBED) ? "yes" : "no");
}

static void print_chat_tokens(FILE *fp, const yvex_chat_runtime *runtime)
{
    yvex_session_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_chat_runtime_get_summary(runtime, &summary, &err) == YVEX_OK) {
        fprintf(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
        fprintf(fp, "position: %llu\n", summary.position);
    }
}

static int handle_chat_slash(yvex_chat_runtime *runtime,
                             yvex_slash_command command,
                             const char *line,
                             int *done,
                             yvex_error *err)
{
    int rc;

    switch (command) {
    case YVEX_SLASH_HELP:
        print_chat_slash_help(stdout);
        return YVEX_OK;
    case YVEX_SLASH_STATUS:
        return yvex_chat_runtime_print_status(stdout, runtime, err);
    case YVEX_SLASH_MODEL:
        print_chat_model(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_BACKEND:
        print_chat_backend(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_TOKENS:
        print_chat_tokens(stdout, runtime);
        return YVEX_OK;
    case YVEX_SLASH_RESET:
        rc = yvex_chat_runtime_reset(runtime, err);
        if (rc != YVEX_OK) {
            return rc;
        }
        printf("session reset\n");
        printf("position: 0\n");
        return YVEX_OK;
    case YVEX_SLASH_QUIT:
        printf("bye\n");
        *done = 1;
        return YVEX_OK;
    case YVEX_SLASH_UNKNOWN:
        printf("unknown slash command: %s\n", line ? line : "");
        printf("type /help\n");
        return YVEX_OK;
    case YVEX_SLASH_NOT_COMMAND:
        break;
    }
    return YVEX_OK;
}

static int command_chat(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_engine_summary engine_summary;
    yvex_session_summary session_summary;
    yvex_metrics *metrics = NULL;
    yvex_trace *trace = NULL;
    yvex_trace_options trace_options;
    yvex_run_artifacts artifacts;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    const char *metrics_out = NULL;
    const char *trace_out = NULL;
    const char *profile_out = NULL;
    const char *run_dir = NULL;
    unsigned long long context_length = 0;
    unsigned long long phase_token = 0;
    unsigned long long total_token = 0;
    char line[4096];
    int save_run = 0;
    int done = 0;
    int i;
    int rc;
    int final_rc = 0;

    yvex_error_clear(&err);
    memset(&runtime, 0, sizeof(runtime));
    memset(&artifacts, 0, sizeof(artifacts));

    if (argc == 2 || (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0))) {
        print_command_help(stdout, find_command("chat"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a value\n");
                return 2;
            }
            model_path = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires a value\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--ctx") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &context_length)) {
                fprintf(stderr, "yvex: --ctx requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--metrics-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --metrics-out requires a value\n");
                return 2;
            }
            metrics_out = argv[++i];
        } else if (strcmp(argv[i], "--trace-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --trace-out requires a value\n");
                return 2;
            }
            trace_out = argv[++i];
        } else if (strcmp(argv[i], "--profile-out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --profile-out requires a value\n");
                return 2;
            }
            profile_out = argv[++i];
        } else if (strcmp(argv[i], "--save-run") == 0) {
            save_run = 1;
        } else if (strcmp(argv[i], "--run-dir") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --run-dir requires a value\n");
                return 2;
            }
            run_dir = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown chat option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help chat' for usage.\n");
            return 2;
        }
    }

    if (!model_path) {
        fprintf(stderr, "yvex: --model is required for yvex chat in I0\n");
        return 2;
    }
    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex chat in I0\n");
        return 2;
    }

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "chat", "started", "", &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_TOTAL, &total_token, &err);
    (void)yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, &phase_token, &err);
    rc = yvex_chat_runtime_open(&runtime, model_path, backend_name, context_length, &err);
    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_ENGINE_OPEN, phase_token, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: chat-backend-unsupported\n");
        (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "backend-unsupported",
                              yvex_error_message(&err), &err);
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_set_observers(&runtime, metrics, trace);
    (void)yvex_engine_get_summary(runtime.engine, &engine_summary, &err);
    (void)yvex_metrics_set_model_bytes(metrics, engine_summary.known_tensor_bytes,
                                       engine_summary.unsupported_tensor_accounting, &err);
    (void)yvex_chat_runtime_get_summary(&runtime, &session_summary, &err);
    printf("YVEX chat runtime\n");
    printf("model: %s\n", engine_summary.model_name);
    printf("backend: %s\n", backend_name);
    printf("session_state: %s\n", yvex_session_state_name(session_summary.state));
    printf("generation: unsupported in I0\n");
    printf("type /help for commands\n");

    while (!done) {
        printf("yvex> ");
        fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        trim_line(line);
        if (line[0] == '\0') {
            continue;
        }

        if (line[0] == '/') {
            rc = handle_chat_slash(&runtime, yvex_slash_parse(line), line, &done, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            continue;
        }

        {
            yvex_chat_accept_result result;
            rc = yvex_metrics_phase_begin(metrics, YVEX_METRIC_PHASE_CHAT_TURN, &phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            rc = yvex_chat_runtime_accept_user_text(&runtime, NULL, line, &result, &err);
            (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_CHAT_TURN, phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            (void)yvex_metrics_add_chat_turn(metrics, &err);
            (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_CHAT_TURN, "chat_turn", "accepted",
                                  "user prompt accepted", &err);
            printf("accepted tokens: %llu\n", result.prompt_tokens);
            printf("position: %llu\n", result.position);
            printf("assistant: [generation unsupported in I0]\n");
        }
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "chat", "accepted-only",
                          "chat runtime exited without generation", &err);
    yvex_trace_close(trace);
    rc = cli_write_observability_files("chat", engine_summary.model_name, backend_name,
                                       "accepted-only", &artifacts, metrics, argc, argv, &err);
    if (rc != YVEX_OK) {
        final_rc = print_yvex_error(&err, exit_for_status(rc));
    }

    yvex_chat_runtime_close(&runtime);
    yvex_metrics_close(metrics);
    if (final_rc != 0) {
        return final_rc;
    }
    return 0;
}

static int command_source_manifest(int argc, char **argv)
{
    yvex_source_manifest_options options;
    yvex_source_manifest_summary summary;
    yvex_error err;
    const char *out_path = NULL;
    int i;
    int rc;

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("source-manifest"));
        return 0;
    }

    if (argc < 3) {
        fprintf(stderr, "yvex: source-manifest requires a subcommand\n");
        fprintf(stderr, "usage: yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE\n");
        return 2;
    }

    if (strcmp(argv[2], "inspect") == 0) {
        fprintf(stderr, "yvex: source-manifest inspect is not implemented in OWI.1\n");
        return 5;
    }
    if (strcmp(argv[2], "create") != 0) {
        fprintf(stderr, "yvex: unknown source-manifest subcommand: %s\n", argv[2]);
        return 2;
    }

    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    yvex_error_clear(&err);
    options.status = YVEX_SOURCE_STATUS_UNKNOWN;
    options.include_files = 1;

    i = 3;
    while (i < argc) {
        const char *name = argv[i];
        const char *value;

        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: option requires a value: %s\n", name);
            return 2;
        }
        value = argv[i + 1];

        if (strcmp(name, "--hf-repo") == 0) {
            options.repo = value;
        } else if (strcmp(name, "--revision") == 0) {
            options.revision = value;
        } else if (strcmp(name, "--license") == 0) {
            options.license = value;
        } else if (strcmp(name, "--model-card") == 0) {
            options.model_card = value;
        } else if (strcmp(name, "--local-path") == 0) {
            options.local_path = value;
        } else if (strcmp(name, "--node") == 0) {
            options.node_name = value;
        } else if (strcmp(name, "--status") == 0) {
            if (!parse_source_status(value, &options.status)) {
                fprintf(stderr, "yvex: unknown source status: %s\n", value);
                return 2;
            }
        } else if (strcmp(name, "--dry-run-log") == 0) {
            options.dry_run_log = value;
        } else if (strcmp(name, "--download-log") == 0) {
            options.download_log = value;
        } else if (strcmp(name, "--pid-file") == 0) {
            options.pid_file = value;
        } else if (strcmp(name, "--download-command") == 0) {
            options.download_command = value;
        } else if (strcmp(name, "--out") == 0) {
            out_path = value;
        } else {
            fprintf(stderr, "yvex: unknown source-manifest option: %s\n", name);
            return 2;
        }
        i += 2;
    }

    if (!options.repo || !options.revision || !options.local_path || !out_path) {
        fprintf(stderr, "yvex: --hf-repo, --revision, --local-path, and --out are required\n");
        return 2;
    }

    rc = yvex_source_manifest_write_json(out_path, &options, &summary, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("source manifest: written\n");
    printf("repo: %s\n", options.repo);
    printf("revision: %s\n", options.revision);
    printf("local_path: %s\n", options.local_path);
    printf("status: %s\n", yvex_source_status_name(options.status));
    printf("files: %llu\n", summary.file_count);
    printf("safetensors: %llu\n", summary.safetensors_count);
    printf("total_size_bytes: %llu\n", summary.total_size_bytes);
    printf("out: %s\n", out_path);
    printf("status: source-manifest-written\n");
    return 0;
}

static int command_tensors(int argc, char **argv)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *table = NULL;
    const yvex_gguf_header *header;
    yvex_error err;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("tensors"));
            return 0;
        }
        fprintf(stderr, "yvex: tensors requires exactly one path\n");
        fprintf(stderr, "usage: yvex tensors <path>\n");
        return 2;
    }

    rc = open_artifact_for_gguf(argv[2], &artifact, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_gguf_open(&gguf, artifact, &err);
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&table, gguf, &err);
    }
    if (rc != YVEX_OK) {
        yvex_tensor_table_close(table);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    header = yvex_gguf_header_view(gguf);
    printf("format: gguf\n");
    printf("version: %u\n", header->version);
    printf("tensor_count: %llu\n", yvex_gguf_tensor_count(gguf));
    printf("tensor_data_offset: %llu\n", yvex_gguf_tensor_data_offset(gguf));
    printf("alignment: %u\n", yvex_gguf_alignment(gguf));
    printf("\n");

    for (i = 0; i < yvex_tensor_table_count(table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(table, i);
        printf("%llu %s role=%s rank=%u dims=",
               i,
               tensor->name,
               yvex_tensor_role_name(tensor->role),
               tensor->rank);
        print_tensor_dims(tensor->dims, tensor->rank);
        printf(" dtype=%s bytes=%llu offset=%llu absolute=%llu\n",
               yvex_dtype_name(tensor->dtype),
               tensor->storage_bytes,
               tensor->relative_offset,
               tensor->absolute_offset);
    }

    yvex_tensor_table_close(table);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return 0;
}

static int command_version(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("yvex %s\n", yvex_version_string());
    return 0;
}

int main(int argc, char **argv)
{
    const char *name;
    const yvex_cli_command *command;

    if (argc == 1) {
        print_top_level_help(stdout);
        return 0;
    }

    name = argv[1];

    if (strcmp(name, "--help") == 0 || strcmp(name, "-h") == 0) {
        print_top_level_help(stdout);
        return 0;
    }

    if (strcmp(name, "--version") == 0) {
        return command_version(argc, argv);
    }

    command = find_command(name);
    if (command) {
        return command->handler(argc, argv);
    }

    fprintf(stderr, "yvex: unknown command: %s\n", name);
    fprintf(stderr, "Try 'yvex help' for usage.\n");
    return 2;
}
