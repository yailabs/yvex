/*
 * yvex_cli.c - Command table and operator CLI.
 *
 * This file owns parsing and dispatch for repository-local operator commands.
 * Runtime work is delegated to the library modules behind each command.
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

static int command_backend(int argc, char **argv);
static int command_chat(int argc, char **argv);
static int command_commands(int argc, char **argv);
static int command_convert(int argc, char **argv);
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
static int command_input(int argc, char **argv);
static int command_integrity(int argc, char **argv);
static int command_integrity_report(int argc, char **argv);
static int command_kv(int argc, char **argv);
static int command_materialize(int argc, char **argv);
static int command_materialize_gate(int argc, char **argv);
static int command_metadata(int argc, char **argv);
static int command_model_gate(int argc, char **argv);
static int command_models(int argc, char **argv);
static int command_native_weights(int argc, char **argv);
static int command_paths(int argc, char **argv);
static int command_plan(int argc, char **argv);
static int command_prefill(int argc, char **argv);
static int command_prompt(int argc, char **argv);
static int command_quant_job(int argc, char **argv);
static int command_quant_policy(int argc, char **argv);
static int command_qtype_support(int argc, char **argv);
static int command_run(int argc, char **argv);
static int command_session(int argc, char **argv);
static int command_source_manifest(int argc, char **argv);
static int command_tensor_map(int argc, char **argv);
static int command_tokenize(int argc, char **argv);
static int command_tokenizer(int argc, char **argv);
static int command_tensors(int argc, char **argv);
static int command_version(int argc, char **argv);
static int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface);
static int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot,
                                      const char *path,
                                      yvex_error *err);
static void model_ref_registry_entry_view(const yvex_model_ref *ref,
                                          yvex_model_registry_entry *entry);
static void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report);

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
        "Start the diagnostic console.",
        "yvex chat [--model FILE_OR_ALIAS] --backend cpu|cuda [--ctx N] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session state and accepts user text without generating output. If --model is omitted, chat uses the current model selected with yvex models use ALIAS.",
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
        "convert",
        "Plan or emit selected open-weight GGUF conversions.",
        "yvex convert plan --arch ARCH --native-source DIR --out-plan FILE | yvex convert emit --arch ARCH --native-source DIR --tensor NAME --target-qtype QTYPE --out FILE [--overwrite]",
        "Builds conversion plans and emits selected tensor GGUF artifacts from official safetensors sources. Output files should use the canonical GGUF artifact naming grammar. It does not infer, execute a full model, or claim generation support.",
        command_convert,
    },
    {
        "cuda-info",
        "Probe CUDA devices.",
        "yvex cuda-info",
        "Reports CUDA driver/device facts when CUDA is available. It does not claim CUDA model execution, matmul, transformer attention, or inference.",
        command_cuda_info,
    },
    {
        "detokenize",
        "Decode token IDs with an implemented tokenizer.",
        "yvex detokenize <path> --ids IDS",
        "Opens a GGUF tokenizer descriptor and decodes comma-separated token IDs. Only the fixture tokenizer path is implemented.",
        command_detokenize,
    },
    {
        "engine",
        "Open an engine descriptor.",
        "yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]",
        "Opens the descriptor/tokenizer/graph stack and, when --backend is provided, attaches selected materialized weights as engine-owned residency state. It does not execute prefill, decode, or generation.",
        command_engine,
    },
    {
        "graph",
        "Build graph diagnostics or execute narrow graph segments.",
        "yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N] | yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N | yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal] | yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N",
        "Opens a GGUF descriptor and prints graph planning diagnostics. Execution flags attach selected weights and run the deterministic fixture embed node, real selected F16 embedding segment, selected embedding-plus-RMSNorm segment, standalone RoPE position op, standalone F32 attention primitive, or standalone F32 matmul/projection primitive; none of these paths is inference or text generation.",
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
        "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]",
        "Writes one controlled F32 or F16 tensor and controlled tokenizer metadata, then validates the emitted GGUF through YVEX parse and CPU materialization. It does not convert DeepSeek, quantize, infer, or emit a generic model.",
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
        "yvex inspect FILE_OR_ALIAS",
        "Opens a file, parses the GGUF directory, builds a YVEX tensor table and descriptor, and prints a descriptor-only summary. Inspect does not materialize weights or execute a graph.",
        command_inspect,
    },
    {
        "input",
        "Parse and validate runtime token input.",
        "yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT",
        "Parses explicit token sequences or tokenizer-backed prompt text into validated token input. This is not prefill, decode, logits, sampling, or generation.",
        command_input,
    },
    {
        "integrity",
        "Check or report local artifact integrity.",
        "yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N] | yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]",
        "Validates GGUF structural bounds, tensor byte ranges, local file identity digest, checked dtype/shape accounting, optional selected embedding readiness, and operator report summaries. It is not a supply-chain security audit.",
        command_integrity,
    },
    {
        "kv",
        "Create a minimal session-owned KV store.",
        "yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]",
        "Allocates a minimal F32 session-owned KV store, optionally appends deterministic demo positions, reads one position, and reports lifecycle/bounds facts. It does not run attention, decode, logits, sampling, generation, or prefill.",
        command_kv,
    },
    {
        "materialize",
        "Materialize selected weights into backend tensors.",
        "yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]",
        "Resolves a model path or registered alias, copies GGUF tensor bytes into backend-owned tensors, and reports residency. This does not execute prefill, decode, sampling, generation, or inference.",
        command_materialize,
    },
    {
        "materialize-gate",
        "Run a repeatable materialization hardening gate.",
        "yvex materialize-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --scope selected-tensor --expect-tensor NAME --expect-rank N --expect-dims model layer,D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--repeat N] [--check-cleanup] [--report-out FILE]",
        "Validates file identity, tensor specs, repeated CPU/CUDA materialization, cleanup, and failure classes. materialization gate uses DeepSeek as the only live target and does not claim execution or inference.",
        command_materialize_gate,
    },
    {
        "metadata",
        "Print parsed GGUF metadata entries.",
        "yvex metadata FILE_OR_ALIAS",
        "Resolves a model path or registered alias, opens a GGUF file, and prints parsed metadata key/value summaries. Arrays are summarized.",
        command_metadata,
    },
    {
        "model-gate",
        "Validate a produced GGUF artifact materialization gate.",
        "yvex model-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --expect-tensor NAME --expect-rank N --expect-dims model layer,D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--report-out FILE]",
        "Checks file identity, expected tensor specs, and requested CPU/CUDA materialization. It classifies selected-tensor materialization only and does not claim full-model support, graph execution, prefill, decode, generation, or inference.",
        command_model_gate,
    },
    {
        "models",
        "Manage the local model alias registry.",
        "yvex models scan --root DIR [--registry FILE] | yvex models add --path FILE [--alias ALIAS] [--support-level LEVEL] [--registry FILE] | yvex models list [--registry FILE] | yvex models use ALIAS [--registry FILE] | yvex models current [--registry FILE] | yvex models verify ALIAS [--registry FILE] | yvex models inspect ALIAS [--registry FILE] | yvex models remove ALIAS [--registry FILE]",
        "Discovers, registers, verifies, lists, selects, inspects, and removes local model artifacts by alias. Aliases use canonical artifact names such as deepseek4-v4-flash-selected-embed; simple labels such as controlled are rejected.",
        command_models,
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
        "prefill",
        "Create an inspectable prefill state summary.",
        "yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]",
        "Consumes validated explicit token input through the implemented selected embedding-plus-RMSNorm graph segment and records a segment-summary prefill foundation. With --attach-kv it binds processed positions to a minimal session-owned KV store. It does not run attention, decode, logits, sampling, or generation.",
        command_prefill,
    },
    {
        "prompt",
        "Render a bounded prompt from explicit messages.",
        "yvex prompt <path> [--system TEXT] --user TEXT [--assistant TEXT] [--tokens]",
        "Renders the YVEX default prompt format. Arbitrary Jinja chat templates are not executed in tokenizer layer.",
        command_prompt,
    },
    {
        "quant-job",
        "Create, inspect, or validate an external quantization job manifest.",
        "yvex quant-job create --name NAME --arch ARCH --tool TOOL --tool-path FILE --native-source DIR --template FILE --out-gguf FILE --log FILE --status STATUS --command TEXT --out FILE | yvex quant-job inspect|validate --manifest FILE",
        "Records a quantization/conversion job manifest. It does not run arbitrary external tools, make external tools part of the YVEX production path, infer, or claim model execution.",
        command_quant_job,
    },
    {
        "quant-policy",
        "Inspect, validate, or derive a quantization policy manifest.",
        "yvex quant-policy inspect|validate --policy FILE [--template FILE] | yvex quant-policy derive --template FILE --arch NAME --out FILE",
        "Handles declarative qtype policy manifests. It does not quantize tensors, emit GGUF, materialize weights, calibrate imatrix data, or infer.",
        command_quant_policy,
    },
    {
        "qtype-support",
        "Print conversion qtype support matrix.",
        "yvex qtype-support",
        "Reports policy/storage/emit/quantize/compute support separately. Compute support is not implied by conversion support.",
        command_qtype_support,
    },
    {
        "run",
        "Accept one prompt through the diagnostic runtime path.",
        "yvex run --model FILE --backend cpu|cuda --prompt TEXT [--system TEXT] [--output plain|json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session state, tokenizes the prompt, accepts tokens, and reports accepted-only diagnostics.",
        command_run,
    },
    {
        "session",
        "Create an engine/session layer diagnostic session.",
        "yvex session FILE_OR_ALIAS [--backend cpu|cuda] [--ctx N] [--text TEXT] [--accept-tokens]",
        "Creates a lifecycle-only session over an engine and backend. It observes engine-attached weights but does not run prefill, decode, or generation.",
        command_session,
    },
    {
        "source-manifest",
        "Create an open-weight source provenance manifest.",
        "yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE [--license TEXT] [--model-card URL] [--node NAME] [--dry-run-log FILE] [--download-log FILE] [--pid-file FILE] [--download-command TEXT]",
        "Scans a local official-weight source directory and writes provenance JSON. It does not download, parse safetensors, quantize, emit GGUF, materialize, or infer.",
        command_source_manifest,
    },
    {
        "tokenize",
        "Encode text with an implemented tokenizer.",
        "yvex tokenize <path> --text TEXT",
        "Opens a GGUF tokenizer descriptor and tokenizes text. Only the fixture tokenizer path is implemented.",
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
        "yvex tensors FILE_OR_ALIAS",
        "Resolves a model path or registered alias and prints YVEX tensor table rows with role, dtype, known storage bytes, and offsets.",
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

static void print_integrity_report(const yvex_artifact_integrity_report *report,
                                   const char *model_label)
{
    unsigned int i;

    printf("artifact_integrity: check\n");
    printf("model: %s\n", model_label ? model_label : report->path);
    printf("format: %s\n", report->format[0] ? report->format : "unknown");
    if (report->version) {
        printf("version: %u\n", report->version);
    }
    printf("file_size: %llu\n", report->file_size);
    if (report->architecture[0]) {
        printf("architecture: %s\n", report->architecture);
    }
    printf("tensor_count: %llu\n", report->tensor_count);
    printf("known_tensor_bytes: %llu\n", report->known_tensor_bytes);
    printf("tensor_ranges_checked: %llu\n", report->tensor_ranges_checked);
    printf("tensor_ranges_valid: %llu\n", report->tensor_ranges_valid);
    printf("tensor_ranges_invalid: %llu\n", report->tensor_ranges_invalid);
    printf("tensor_shapes_checked: %llu\n", report->tensor_shapes_checked);
    printf("tensor_shapes_valid: %llu\n", report->tensor_shapes_valid);
    printf("tensor_shapes_invalid: %llu\n", report->tensor_shapes_invalid);
    printf("tensor_dtypes_checked: %llu\n", report->tensor_dtypes_checked);
    printf("tensor_dtypes_valid: %llu\n", report->tensor_dtypes_valid);
    printf("tensor_dtypes_invalid: %llu\n", report->tensor_dtypes_invalid);
    printf("tensor_byte_counts_checked: %llu\n", report->tensor_byte_counts_checked);
    printf("tensor_byte_counts_invalid: %llu\n", report->tensor_byte_counts_invalid);
    if (report->selected_embedding_shape[0]) {
        printf("selected_embedding_shape: %s\n", report->selected_embedding_shape);
        printf("selected_embedding_hidden_size: %llu\n", report->selected_embedding_hidden_size);
        printf("selected_embedding_vocab_size: %llu\n", report->selected_embedding_vocab_size);
        printf("selected_embedding_output_count: %llu\n", report->selected_embedding_output_count);
        printf("selected_embedding_output_bytes: %llu\n", report->selected_embedding_output_bytes);
        printf("selected_embedding_slice_bytes: %llu\n", report->selected_embedding_slice_bytes);
    }
    printf("identity_checked: %s\n", report->identity_checked ? "true" : "false");
    printf("sha256: %s\n", report->sha256[0] ? report->sha256 : "unavailable");
    printf("registered_sha256: %s\n", report->registered_sha256[0] ? report->registered_sha256 : "absent");
    if (report->expected_sha256[0]) {
        printf("expected_sha256: %s\n", report->expected_sha256);
        printf("actual_sha256: %s\n", report->sha256[0] ? report->sha256 : "unavailable");
    }
    printf("digest_status: %s\n", report->digest_status[0] ? report->digest_status : "unknown");
    printf("integrity_status: %s\n", report->passed ? "pass" : "fail");
    printf("integrity_errors: %u\n", report->error_count);
    printf("integrity_warnings: %u\n", report->warning_count);

    for (i = 0; i < report->issue_count; ++i) {
        const yvex_integrity_issue *issue = yvex_artifact_integrity_issue_at(report, i);
        const char *prefix;

        if (!issue) {
            continue;
        }
        prefix = issue->severity == YVEX_INTEGRITY_SEVERITY_WARNING ? "warning" : "error";
        printf("%s_%u_code: %s\n", prefix, i, issue->code);
        if (issue->tensor[0]) {
            printf("%s_%u_tensor: %s\n", prefix, i, issue->tensor);
        }
        if (issue->has_range) {
            printf("%s_%u_relative_offset: %llu\n", prefix, i, issue->relative_offset);
            printf("%s_%u_absolute_offset: %llu\n", prefix, i, issue->absolute_offset);
            printf("%s_%u_tensor_bytes: %llu\n", prefix, i, issue->tensor_bytes);
            printf("%s_%u_file_size: %llu\n", prefix, i, issue->file_size);
        }
        printf("%s_%u_reason: %s\n", prefix, i, issue->reason);
    }

    printf("status: %s\n", report->passed ? "artifact-integrity-pass"
                                          : "artifact-integrity-fail");
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
    yvex_model_ref ref;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));

    rc = yvex_model_ref_resolve(&ref, path, NULL, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    options.path = ref.path;
    options.readonly = 1;
    options.map = 1;

    rc = yvex_artifact_open(artifact, &options, err);
    yvex_model_ref_clear(&ref);
    return rc;
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
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    int rc;

    memset(ctx, 0, sizeof(*ctx));
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
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
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(ctx->artifact,
                                              ctx->gguf,
                                              ctx->table,
                                              &integrity_options,
                                              &integrity_report,
                                              err);
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
    print_backend_capability(backend, YVEX_BACKEND_CAP_OP_ROPE);
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
            /* Accepted for tokenizer layer CLI shape; fixture tokenization has no implicit BOS/EOS. */
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

static int parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

static int parse_uint_allow_zero(const char *text, unsigned int *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out) {
        return 0;
    }
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 0xffffffffull) {
        return 0;
    }
    *out = (unsigned int)value;
    return 1;
}

static void print_token_input_tokens(const yvex_token_input *input)
{
    unsigned long long i;

    for (i = 0; input && i < input->token_count; ++i) {
        printf("token_%llu: %u\n", i, input->tokens[i]);
    }
}

static void fill_kv_demo_values(float *values,
                                unsigned long long value_count,
                                unsigned long long position)
{
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        values[i] = (float)((position * 1000ull) + i);
    }
}

static unsigned long long checksum_kv_values(const float *values,
                                             unsigned long long value_count)
{
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; values && i < value_count; ++i) {
        unsigned long long v = (unsigned long long)values[i];
        checksum ^= v + (i << 8u);
        checksum *= 1099511628211ull;
    }
    return checksum;
}

static void print_token_input_summary(const yvex_token_input *input,
                                      const char *status,
                                      const char *bounds_status,
                                      unsigned long long selected_index,
                                      unsigned int selected_token,
                                      int has_selected)
{
    printf("token_input_status: %s\n", status ? status : "fail");
    printf("token_input_kind: %s\n",
           input ? yvex_token_input_kind_name(input->kind) : "unknown");
    printf("token_count: %llu\n", input ? input->token_count : 0ull);
    if (input) {
        printf("selected_token_index: %llu\n", selected_index);
    }
    if (has_selected) {
        printf("selected_token_id: %u\n", selected_token);
    } else if (input) {
        printf("selected_token_id: unavailable\n");
    }
    printf("token_bounds_status: %s\n", bounds_status ? bounds_status : "not-checked");
}

static int cli_token_input_vocab_from_model(const char *path,
                                            unsigned long long *out_vocab_size,
                                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *tensor;
    yvex_tokenizer *tokenizer = NULL;
    int rc;

    if (!path || !out_vocab_size) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cli_token_input_vocab_from_model",
                       "path and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out_vocab_size = 0ull;
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (tensor && tensor->rank == 2 && tensor->dims[1] > 0ull) {
        *out_vocab_size = tensor->dims[1];
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }

    rc = yvex_tokenizer_from_gguf(&tokenizer, ctx.gguf, ctx.model, err);
    if (rc == YVEX_OK && yvex_tokenizer_vocab_size(tokenizer) > 0ull) {
        *out_vocab_size = yvex_tokenizer_vocab_size(tokenizer);
        yvex_tokenizer_close(tokenizer);
        close_model_context(&ctx);
        yvex_error_clear(err);
        return YVEX_OK;
    }
    yvex_tokenizer_close(tokenizer);
    close_model_context(&ctx);
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cli_token_input_vocab_from_model",
                   "tokenizer-metadata-missing");
    return YVEX_ERR_UNSUPPORTED;
}

static int command_input(int argc, char **argv)
{
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_token_input input;
    yvex_tokens tokens;
    yvex_error err;
    const char *subcommand;
    const char *model_arg = NULL;
    const char *tokens_text = NULL;
    const char *prompt_text = NULL;
    unsigned long long vocab_size = 0ull;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&input, 0, sizeof(input));
    memset(&tokens, 0, sizeof(tokens));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("input"));
        return argc >= 3 ? 0 : 2;
    }

    subcommand = argv[2];
    if (strcmp(subcommand, "tokens") != 0 && strcmp(subcommand, "prompt") != 0) {
        fprintf(stderr, "yvex: input requires tokens or prompt\n");
        fprintf(stderr, "usage: yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0 && strcmp(subcommand, "tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--text") == 0 && strcmp(subcommand, "prompt") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --text requires TEXT\n");
                return 2;
            }
            prompt_text = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown input option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help input' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: input requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (strcmp(subcommand, "tokens") == 0 && !tokens_text) {
        fprintf(stderr, "yvex: input tokens requires --tokens IDS\n");
        return 2;
    }
    if (strcmp(subcommand, "prompt") == 0 && !prompt_text) {
        fprintf(stderr, "yvex: input prompt requires --text TEXT\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&ref, "input");
    if (rc != YVEX_OK) {
        printf("token_input: %s\n", subcommand);
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("identity_status: fail\n");
        print_token_input_summary(NULL, "fail", "not-checked", 0ull, 0u, 0);
        printf("prefill_ready: false\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    if (strcmp(subcommand, "tokens") == 0) {
        rc = yvex_token_input_parse_explicit(tokens_text, &input, &err);
        if (rc == YVEX_OK) {
            rc = cli_token_input_vocab_from_model(ref.path, &vocab_size, &err);
        }
        if (rc == YVEX_OK) {
            rc = yvex_token_input_validate_bounds(&input, vocab_size, &err);
        }

        printf("token_input: tokens\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("identity_status: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered");
        print_token_input_summary(&input,
                                  rc == YVEX_OK ? "pass" : "fail",
                                  rc == YVEX_OK ? "pass" :
                                  input.token_bounds_checked ? "fail" : "not-checked",
                                  0ull,
                                  input.token_count > 0ull ? input.tokens[0] : 0u,
                                  input.token_count > 0ull);
        printf("vocab_size: %llu\n", vocab_size);
        print_token_input_tokens(&input);
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");
        yvex_model_ref_clear(&ref);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        return 0;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: not-checked\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        yvex_model_ref_clear(&ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_tokenizer_from_gguf(&ctx.tokenizer, ctx.gguf, ctx.model, &err);
    if (rc != YVEX_OK) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: missing\n");
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }
    if (yvex_tokenizer_support_of(ctx.tokenizer) != YVEX_TOKENIZER_SUPPORT_FIXTURE_ENCODE_DECODE) {
        printf("token_input: prompt\n");
        printf("model: %s\n", model_arg);
        printf("resolved_path: %s\n", ref.path ? ref.path : "");
        printf("model_input_kind: %s\n",
               ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
        printf("token_input_status: fail\n");
        printf("token_input_kind: prompt-text\n");
        printf("tokenizer_status: unsupported\n");
        printf("tokenizer_support: %s\n",
               yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
        printf("reason: tokenizer-metadata-missing\n");
        printf("token_bounds_status: not-checked\n");
        printf("prefill_ready: false\n");
        printf("logits_ready: false\n");
        printf("generation: unsupported\n");
        printf("status: token-input-fail\n");
        close_tokenizer_context(&ctx);
        yvex_model_ref_clear(&ref);
        yvex_error_set(&err, YVEX_ERR_UNSUPPORTED, "yvex_input_prompt",
                       "tokenizer-metadata-missing");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_UNSUPPORTED));
    }

    rc = yvex_tokenize_text(ctx.tokenizer, prompt_text, &tokens, &err);
    if (rc == YVEX_OK) {
        rc = yvex_token_input_from_ids(YVEX_TOKEN_INPUT_PROMPT_TEXT,
                                       tokens.ids,
                                       tokens.len,
                                       &input,
                                       &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&input,
                                              yvex_tokenizer_vocab_size(ctx.tokenizer),
                                              &err);
    }

    printf("token_input: prompt\n");
    printf("model: %s\n", model_arg);
    printf("resolved_path: %s\n", ref.path ? ref.path : "");
    printf("model_input_kind: %s\n",
           ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path");
    printf("tokenizer_status: present\n");
    printf("tokenizer_support: %s\n",
           yvex_tokenizer_support_name(yvex_tokenizer_support_of(ctx.tokenizer)));
    print_token_input_summary(&input,
                              rc == YVEX_OK ? "pass" : "fail",
                              rc == YVEX_OK ? "pass" :
                              input.token_bounds_checked ? "fail" : "not-checked",
                              0ull,
                              input.token_count > 0ull ? input.tokens[0] : 0u,
                              input.token_count > 0ull);
    printf("vocab_size: %llu\n", yvex_tokenizer_vocab_size(ctx.tokenizer));
    print_token_input_tokens(&input);
    printf("prefill_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", rc == YVEX_OK ? "token-input-pass" : "token-input-fail");

    yvex_tokens_free(&tokens);
    close_tokenizer_context(&ctx);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    return 0;
}

static int command_integrity(int argc, char **argv)
{
    yvex_artifact_integrity_options options;
    yvex_artifact_integrity_report report;
    yvex_model_ref ref;
    yvex_error err;
    const char *model_arg = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&report, 0, sizeof(report));
    memset(&ref, 0, sizeof(ref));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("integrity"));
        return 0;
    }
    if (argc >= 3 && strcmp(argv[2], "report") == 0) {
        return command_integrity_report(argc, argv);
    }
    if (argc < 3 || strcmp(argv[2], "check") != 0) {
        fprintf(stderr, "yvex: integrity requires check or report\n");
        fprintf(stderr, "usage: yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N] | yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--expect-sha256") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --expect-sha256 requires HASH\n");
                return 2;
            }
            options.expect_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--require-token-embedding") == 0) {
            options.require_token_embedding = 1;
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &options.token_id)) {
                fprintf(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            options.require_token_embedding = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown integrity option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help integrity' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: integrity check requires --model FILE_OR_ALIAS\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (ref.kind == YVEX_MODEL_REF_ALIAS && ref.sha256 && ref.sha256[0]) {
        options.registered_sha256 = ref.sha256;
    }

    rc = yvex_artifact_integrity_check_path(ref.path, &options, &report, &err);
    print_integrity_report(&report, ref.path);
    yvex_model_ref_clear(&ref);
    if (rc != YVEX_OK) {
        return exit_for_status(rc);
    }
    return 0;
}

static int command_kv(int argc, char **argv)
{
    yvex_kv_shape shape;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    float *append_values = NULL;
    float *read_values = NULL;
    unsigned long long value_count = 0ull;
    unsigned long long append_target = 0ull;
    unsigned long long appended_position = 0ull;
    unsigned long long read_position = 0ull;
    unsigned long long read_checksum = 0ull;
    int append_demo = 0;
    int read_requested = 0;
    int cleanup_attempted = 0;
    const char *cleanup_status = "not-needed";
    int i;
    int rc;

    memset(&shape, 0, sizeof(shape));
    memset(&summary, 0, sizeof(summary));
    yvex_error_clear(&err);

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("kv"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.layer_count)) {
                fprintf(stderr, "yvex: --layers requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--heads") == 0 || strcmp(argv[i], "--lanes") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.kv_head_count)) {
                fprintf(stderr, "yvex: --heads requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.head_dim)) {
                fprintf(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &shape.capacity)) {
                fprintf(stderr, "yvex: --capacity requires a positive integer\n");
                return 2;
            }
            i += 1;
        } else if (strcmp(argv[i], "--append-demo") == 0) {
            append_demo = 1;
        } else if (strcmp(argv[i], "--read-position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &read_position)) {
                fprintf(stderr, "yvex: --read-position requires a non-negative integer\n");
                return 2;
            }
            read_requested = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown kv option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help kv' for usage.\n");
            return 2;
        }
    }

    if (shape.layer_count == 0ull || shape.kv_head_count == 0ull ||
        shape.head_dim == 0ull || shape.capacity == 0ull) {
        fprintf(stderr, "usage: yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]\n");
        return 2;
    }

    rc = yvex_kv_cache_create_shape(&kv, &shape, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }

    value_count = yvex_kv_cache_position_value_count(kv);
    if (append_demo) {
        append_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!append_values) {
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV append demo buffer\n");
            return 4;
        }
        append_target = shape.capacity > 1ull ? 2ull : 1ull;
        for (i = 0; (unsigned long long)i < append_target; ++i) {
            fill_kv_demo_values(append_values, value_count, (unsigned long long)i);
            rc = yvex_kv_cache_append_position_f32(kv,
                                                   append_values,
                                                   value_count,
                                                   &appended_position,
                                                   &err);
            if (rc != YVEX_OK) {
                free(append_values);
                yvex_kv_cache_close(kv);
                return print_yvex_error(&err, exit_for_status(rc));
            }
        }
    }

    if (read_requested) {
        read_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!read_values) {
            free(append_values);
            yvex_kv_cache_close(kv);
            fprintf(stderr, "yvex: failed to allocate KV read buffer\n");
            return 4;
        }
        rc = yvex_kv_cache_read_position_f32(kv, read_position, read_values, value_count, &err);
        if (rc != YVEX_OK) {
            free(read_values);
            free(append_values);
            yvex_kv_cache_close(kv);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        read_checksum = checksum_kv_values(read_values, value_count);
    }

    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    if (rc != YVEX_OK) {
        free(read_values);
        free(append_values);
        yvex_kv_cache_close(kv);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    cleanup_attempted = 1;
    yvex_kv_cache_close(kv);
    kv = NULL;
    cleanup_status = "pass";

    printf("kv: ownership\n");
    printf("kv_created: true\n");
    printf("session_owned: %s\n", summary.session_owned ? "true" : "false");
    printf("layers: %llu\n", summary.layer_count);
    printf("heads: %llu\n", summary.kv_head_count);
    printf("head_dim: %llu\n", summary.head_dim);
    printf("capacity: %llu\n", summary.context_length);
    printf("dtype: %s\n", summary.dtype ? summary.dtype : "F32");
    printf("values_per_position: %llu\n", summary.values_per_position);
    printf("bytes_per_position: %llu\n", summary.bytes_per_position);
    printf("planned_bytes: %llu\n", summary.bytes);
    printf("allocated_bytes: %llu\n", summary.allocated_bytes);
    printf("append_count: %llu\n", summary.append_count);
    printf("read_count: %llu\n", summary.read_count);
    printf("written_positions: %llu\n", summary.written_positions);
    printf("last_appended_position: %llu\n", appended_position);
    if (read_requested) {
        unsigned long long sample_count = value_count < 8ull ? value_count : 8ull;
        unsigned long long j;
        printf("read_position: %llu\n", read_position);
        printf("read_value_count: %llu\n", value_count);
        printf("read_checksum: %llu\n", read_checksum);
        printf("read_sample_values:");
        for (j = 0; j < sample_count; ++j) {
            printf("%s%.9g", j == 0 ? " " : ",", (double)read_values[j]);
        }
        printf("\n");
    } else {
        printf("read_position: not-requested\n");
        printf("read_value_count: 0\n");
        printf("read_checksum: 0\n");
    }
    printf("overflow_status: %s\n",
           summary.overflow_status ? summary.overflow_status : "not-overflowed");
    printf("cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", cleanup_status);
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: kv-owned\n");

    free(read_values);
    free(append_values);
    return 0;
}

static int parse_dims_csv(const char *text,
                          unsigned int rank,
                          unsigned long long dims[4])
{
    const char *p = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u) return 0;
    for (i = 0; i < 4u; ++i) dims[i] = 0;
    for (i = 0; i < rank; ++i) {
        unsigned long long value;
        errno = 0;
        value = strtoull(p, &end, 10);
        if (errno != 0 || end == p || value == 0) return 0;
        dims[i] = value;
        if (i + 1u < rank) {
            if (*end != ',') return 0;
            p = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
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
    yvex_model_ref model_ref;
    yvex_engine_options options;
    yvex_engine_summary summary;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&options, 0, sizeof(options));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("engine"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires a file or alias\n");
                return 2;
            }
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu or cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "yvex: unknown engine option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help engine' for usage.\n");
            return 2;
        } else {
            if (model_arg) {
                fprintf(stderr, "yvex: engine accepts only one model reference\n");
                return 2;
            }
            model_arg = argv[i];
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: engine requires FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]\n");
        return 2;
    }
    if (backend_name && strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (backend_name) {
        rc = enforce_registered_identity_cli(&model_ref, "engine");
        if (rc != YVEX_OK) {
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }
    }

    options.model_path = model_ref.path;
    options.load_tokenizer = backend_name ? 0 : 1;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    options.attach_weights = backend_name != NULL;
    options.backend_name = backend_name;
    options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && backend_name && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: engine-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, &err);
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
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
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("weight_backend_allocated_bytes: %llu\n", summary.weight_backend_allocated_bytes);
    if (summary.weights_attached) {
        const yvex_tensor_table *table = yvex_engine_tensors(engine);
        unsigned long long count = yvex_tensor_table_count(table);
        unsigned long long j;

        for (j = 0; j < count; ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(table, j);
            unsigned int d;

            if (!tensor) {
                continue;
            }
            printf("attached_weight_%llu: %s role=%s rank=%u dims=[",
                   j,
                   tensor->name ? tensor->name : "",
                   yvex_tensor_role_name(tensor->role),
                   tensor->rank);
            for (d = 0; d < tensor->rank; ++d) {
                if (d > 0) {
                    printf(",");
                }
                printf("%llu", tensor->dims[d]);
            }
            printf("] dtype=%s bytes=%llu\n",
                   yvex_dtype_name(tensor->dtype),
                   tensor->storage_bytes);
        }
    }
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_engine_diagnostic_reason(engine));
    printf("status: %s\n", summary.weights_attached ? "engine-weights-attached" : "engine-descriptor");

    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
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
            printf("reason: architecture adapter mapping requires open-weight intake\n");
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
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
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
        } else if (strcmp(argv[i], "--target-qtype") == 0) {
            options.target_qtype = argv[++i];
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
        fprintf(stderr, "usage: yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]\n");
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

static int command_qtype_support(int argc, char **argv)
{
    unsigned long long i;

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("qtype-support"));
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "yvex: qtype-support takes no arguments\n");
        return 2;
    }
    printf("qtype support:\n");
    for (i = 0; i < yvex_qtype_support_count(); ++i) {
        const yvex_qtype_support_info *row = yvex_qtype_support_at(i);
        printf("  %s policy=%s storage=%s emit=%s quantize=%s compute=%s notes=%s\n",
               row->qtype,
               row->policy_supported ? "yes" : "no",
               row->storage_supported ? "yes" : "no",
               row->emit_supported ? "yes" : "no",
               strcmp(row->qtype, "F32") == 0 ? "n/a" : (row->quantize_supported ? "yes" : "no"),
               row->compute_supported ? "partial" : "no",
               row->notes ? row->notes : "");
    }
    printf("status: qtype-support\n");
    return 0;
}

static int command_convert(int argc, char **argv)
{
    yvex_conversion_options options;
    yvex_conversion_summary summary;
    yvex_error err;
    const char *out_plan = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("convert"));
        return 0;
    }
    if (argc < 3 || (strcmp(argv[2], "plan") != 0 && strcmp(argv[2], "emit") != 0)) {
        fprintf(stderr, "yvex: convert requires plan or emit\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--overwrite") == 0) {
            options.overwrite = 1;
            continue;
        }
        if (strcmp(argv[i], "--allow-unsupported-qtype") == 0) {
            options.allow_unsupported_qtype = 1;
            continue;
        }
        if (strcmp(argv[i], "--require-all") == 0) {
            options.require_all = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: convert option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--arch") == 0) options.architecture = argv[++i];
        else if (strcmp(argv[i], "--source-manifest") == 0) options.source_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--native-source") == 0) options.native_source_dir = argv[++i];
        else if (strcmp(argv[i], "--template") == 0) options.template_path = argv[++i];
        else if (strcmp(argv[i], "--quant-policy") == 0) options.quant_policy_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix-manifest") == 0) options.imatrix_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--out") == 0) options.out_path = argv[++i];
        else if (strcmp(argv[i], "--out-plan") == 0) out_plan = argv[++i];
        else if (strcmp(argv[i], "--tensor") == 0) options.tensor_name = argv[++i];
        else if (strcmp(argv[i], "--target-qtype") == 0) options.target_qtype = argv[++i];
        else if (strcmp(argv[i], "--limit") == 0) {
            char *end = NULL;
            options.limit_tensors = strtoull(argv[++i], &end, 10);
            if (!end || *end != '\0') {
                fprintf(stderr, "yvex: invalid convert limit\n");
                return 2;
            }
        } else {
            fprintf(stderr, "yvex: unknown convert option: %s\n", argv[i]);
            return 2;
        }
    }

    if (strcmp(argv[2], "plan") == 0) {
        if (!options.architecture || !options.native_source_dir || !out_plan) {
            fprintf(stderr, "yvex: convert plan requires --arch --native-source --out-plan\n");
            return 2;
        }
        rc = yvex_conversion_plan_write_json(&options, out_plan, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        printf("conversion plan: written\n");
        printf("architecture: %s\n", options.architecture);
        printf("native_tensors: %llu\n", summary.native_tensor_count);
        printf("planned_tensors: %llu\n", summary.planned_tensor_count);
        printf("unmapped_tensors: %llu\n", summary.unmapped_tensor_count);
        printf("unsupported_qtypes: %llu\n", summary.unsupported_qtype_count);
        printf("out: %s\n", out_plan);
        printf("status: conversion-plan-written\n");
        return 0;
    }

    if (!options.architecture || !options.native_source_dir || !options.tensor_name ||
        !options.target_qtype || !options.out_path) {
        fprintf(stderr, "yvex: convert emit requires --arch --native-source --tensor --target-qtype --out\n");
        return 2;
    }
    rc = yvex_conversion_emit_gguf(&options, &summary, &err);
    if (rc != YVEX_OK) {
        printf("conversion emit: gguf\n");
        printf("architecture: %s\n", options.architecture);
        printf("source_tensor: %s\n", options.tensor_name);
        printf("target_qtype: %s\n", options.target_qtype);
        printf("status: conversion-failed\n");
        fprintf(stderr, "reason: %s\n", yvex_error_message(&err));
        return exit_for_status(rc);
    }
    printf("conversion emit: gguf\n");
    printf("architecture: %s\n", options.architecture);
    printf("source_tensor: %s\n", options.tensor_name);
    printf("target_qtype: %s\n", options.target_qtype);
    printf("out: %s\n", options.out_path);
    printf("bytes_read: %llu\n", summary.bytes_read);
    printf("bytes_written: %llu\n", summary.bytes_written);
    printf("roundtrip_validated: %s\n", summary.roundtrip_validated ? "yes" : "no");
    printf("execution_ready: false\n");
    printf("status: conversion-gguf-written\n");
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
    printf("status: selected tensor materialization, engine weight attachment, fixture graph execution, real selected graph segments, standalone RoPE, attention, and matmul ops, explicit token input boundary, prefill state foundation, minimal KV binding, and minimal KV ownership\n");
    printf("library: libyvex.a\n");
    printf("filesystem: implemented\n");
    printf("artifact: open/read implemented\n");
    printf("gguf: metadata/tensor directory parsing implemented\n");
    printf("model: descriptor-only implemented\n");
    printf("tokenizer: fixture encode/decode implemented\n");
    printf("token_input: explicit token boundary implemented\n");
    printf("prefill_state: segment-summary foundation and minimal KV binding implemented\n");
    printf("prompt: default renderer implemented\n");
    printf("graph: partial planning, deterministic fixture execution, selected embedding partial execution, selected embedding RMSNorm segment execution, standalone RoPE position op, standalone F32 attention primitive, and standalone F32 matmul/projection primitive implemented\n");
    printf("planner: estimate-only implemented\n");
    printf("backend: CPU reference implemented\n");
    printf("backend_cuda: tensor movement plus F32/F16 embed, RMSNorm, RoPE position op, F32 attention primitive, and F32 matmul/projection primitive implemented when CUDA is available\n");
    printf("weights: selected tensor materialization implemented\n");
    printf("engine: descriptor open and selected-weight attachment implemented\n");
    printf("session: lifecycle diagnostics, engine attachment observer, and KV ownership implemented\n");
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
    printf("conversion: open-weight selected tensor bridge implemented\n");
    printf("model_ref: alias-or-path resolver implemented\n");
    printf("model_registry: local model alias registry implemented\n");
    printf("quant_job: external quantization job manifest implemented\n");
    printf("qtype_support: conversion support matrix implemented\n");
    printf("weight_mapping: tensor adapter contract implemented\n");
    printf("quant_policy: manifest validator implemented\n");
    printf("imatrix: calibration artifact manifest implemented\n");
    printf("server_binary: yvexd shell implemented\n");
    printf("server_endpoints: health/metrics/models status implemented\n");
    printf("server_generation: not implemented\n");
    printf("kv: minimal session-owned append/read boundary implemented\n");
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
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("inspect"));
            return 0;
        }
        fprintf(stderr, "yvex: inspect requires exactly one FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex inspect FILE_OR_ALIAS\n");
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
        rc = yvex_artifact_integrity_validate(artifact,
                                              gguf,
                                              tensors,
                                              &integrity_options,
                                              &integrity_report,
                                              &err);
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
        fprintf(stderr, "yvex: metadata requires exactly one FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex metadata FILE_OR_ALIAS\n");
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

static int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *identity_status = "pass";
    const char *digest_status = "pass";
    const char *reason = "current file identity matches registered alias";
    int pass = 1;
    int rc;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) {
        return YVEX_OK;
    }

    yvex_error_clear(&err);
    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(ref->path, &identity, &err);
    if (rc != YVEX_OK) {
        identity_status = "fail";
        digest_status = "fail";
        reason = yvex_error_message(&err);
        pass = 0;
    } else if (!ref->sha256 || !ref->sha256[0] || !yvex_sha256_hex_is_valid(ref->sha256)) {
        identity_status = "missing";
        digest_status = "missing";
        reason = "registered alias lacks digest identity; re-add model";
        pass = 0;
        rc = YVEX_ERR_STATE;
    } else if (strcmp(ref->sha256, identity.sha256) != 0 ||
               (ref->registered_file_size != 0ull &&
                ref->registered_file_size != identity.file_size)) {
        identity_status = "fail";
        digest_status = "fail";
        reason = "digest mismatch for registered alias";
        pass = 0;
        rc = YVEX_ERR_STATE;
    } else {
        rc = YVEX_OK;
    }

    if (pass) {
        memset(&current_metadata, 0, sizeof(current_metadata));
        memset(&registered_metadata, 0, sizeof(registered_metadata));
        memset(&metadata_report, 0, sizeof(metadata_report));
        rc = populate_registry_metadata(&current_metadata, ref->path, &err);
        if (rc != YVEX_OK) {
            printf("artifact_identity: check\n");
            printf("surface: %s\n", surface ? surface : "unknown");
            printf("alias: %s\n", ref->alias ? ref->alias : "");
            printf("path: %s\n", ref->path ? ref->path : "");
            printf("registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
            printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
            printf("registered_file_size: %llu\n", ref->registered_file_size);
            printf("current_file_size: %llu\n", identity.file_size);
            printf("digest_status: %s\n", digest_status);
            printf("identity_status: %s\n", identity_status);
            printf("metadata_status: fail\n");
            printf("readiness_status: fail\n");
            printf("metadata_issue_0_code: current-metadata-unavailable\n");
            printf("metadata_issue_0_registered: available\n");
            printf("metadata_issue_0_current: %s\n", yvex_error_message(&err));
            printf("reason: current artifact metadata could not be parsed\n");
            printf("status: models-metadata-drift\n");
            return exit_for_status(YVEX_ERR_STATE);
        }
        model_ref_registry_entry_view(ref, &registered_metadata);
        rc = yvex_model_registry_compare_metadata(&registered_metadata,
                                                  &current_metadata.entry,
                                                  &metadata_report,
                                                  &err);
        if (rc != YVEX_OK ||
            strcmp(metadata_report.metadata_status, "pass") != 0 ||
            strcmp(metadata_report.readiness_status, "pass") != 0) {
            const char *status = strcmp(metadata_report.metadata_status, "missing") == 0
                                     ? "models-metadata-missing"
                                     : "models-metadata-drift";
            printf("artifact_identity: check\n");
            printf("surface: %s\n", surface ? surface : "unknown");
            printf("alias: %s\n", ref->alias ? ref->alias : "");
            printf("path: %s\n", ref->path ? ref->path : "");
            printf("registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
            printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
            printf("registered_file_size: %llu\n", ref->registered_file_size);
            printf("current_file_size: %llu\n", identity.file_size);
            printf("digest_status: %s\n", digest_status);
            printf("identity_status: %s\n", identity_status);
            print_metadata_drift_cli(&metadata_report);
            printf("reason: registered alias metadata does not match current artifact facts\n");
            printf("status: %s\n", status);
            return exit_for_status(YVEX_ERR_STATE);
        }
    }

    if (!pass) {
        printf("artifact_identity: check\n");
        printf("surface: %s\n", surface ? surface : "unknown");
        printf("alias: %s\n", ref->alias ? ref->alias : "");
        printf("path: %s\n", ref->path ? ref->path : "");
        printf("registered_sha256: %s\n", ref->sha256 && ref->sha256[0] ? ref->sha256 : "absent");
        printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
        printf("registered_file_size: %llu\n", ref->registered_file_size);
        printf("current_file_size: %llu\n", identity.file_size);
        printf("digest_status: %s\n", digest_status);
        printf("identity_status: %s\n", identity_status);
        printf("reason: %s\n", reason);
        printf("status: %s\n", strcmp(identity_status, "missing") == 0
               ? "models-identity-missing"
               : "models-identity-fail");
    }
    return rc;
}

static void print_materialization_gate_fields(const char *gate,
                                              const char *phase,
                                              const char *integrity_status,
                                              const char *identity_status,
                                              const char *metadata_status,
                                              const char *shape_status,
                                              const char *range_status,
                                              const char *backend_status,
                                              int allocation_attempted,
                                              int transfer_attempted,
                                              int cleanup_attempted,
                                              const char *cleanup_status,
                                              unsigned long long bytes_planned,
                                              unsigned long long bytes_allocated,
                                              unsigned long long bytes_transferred)
{
    printf("materialization_gate: %s\n", gate ? gate : "fail");
    printf("materialization_phase: %s\n", phase ? phase : "preflight");
    printf("integrity_status: %s\n", integrity_status ? integrity_status : "unchecked");
    printf("identity_status: %s\n", identity_status ? identity_status : "unregistered");
    printf("metadata_status: %s\n", metadata_status ? metadata_status : "unregistered");
    printf("shape_status: %s\n", shape_status ? shape_status : "unchecked");
    printf("range_status: %s\n", range_status ? range_status : "unchecked");
    printf("backend_status: %s\n", backend_status ? backend_status : "not-opened");
    printf("allocation_attempted: %s\n", allocation_attempted ? "true" : "false");
    printf("transfer_attempted: %s\n", transfer_attempted ? "true" : "false");
    printf("cleanup_attempted: %s\n", cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", cleanup_status ? cleanup_status : "not-needed");
    printf("bytes_planned: %llu\n", bytes_planned);
    printf("bytes_allocated: %llu\n", bytes_allocated);
    printf("bytes_transferred: %llu\n", bytes_transferred);
}

static int command_materialize(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *model_path = NULL;
    const char *backend_name = NULL;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&ctx, 0, sizeof(ctx));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc == 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("materialize"));
        return 0;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
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
        fprintf(stderr, "yvex: materialize requires --model FILE_OR_ALIAS and --backend cpu|cuda\n");
        fprintf(stderr, "usage: yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]\n");
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

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "materialize");
    if (rc != YVEX_OK) {
        print_materialization_gate_fields("fail", "preflight",
                                          "not-checked", "fail", "fail",
                                          "not-checked", "not-checked", "not-opened",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        printf("status: materialization-integrity-fail\n");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = open_model_context(model_ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        print_materialization_gate_fields("fail", "preflight",
                                          "fail",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "unchecked", "unchecked", "not-opened",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        printf("status: materialization-integrity-fail\n");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        printf("materialization status: unsupported\n");
        printf("backend: %s\n", backend_name);
        print_materialization_gate_fields("fail", "preflight", "pass",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "pass", "pass", "unavailable",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: weights-unsupported\n");
        close_model_context(&ctx);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        close_model_context(&ctx);
        yvex_model_ref_clear(&model_ref);
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
        print_materialization_gate_fields("fail", "preflight", "pass",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                          "fail", "fail", "ready",
                                          0, 0, 0, "not-needed", 0, 0, 0);
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: weights-unsupported\n");
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_TRANSFER")) {
            print_materialization_gate_fields("fail", "transfer", "pass",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "pass", "pass", "ready",
                                              1, 1, 1, "pass", 0, 0, 0);
            printf("status: materialization-failed-cleaned\n");
        } else if (getenv("YVEX_TEST_FAIL_MATERIALIZE_AFTER_ALLOC")) {
            print_materialization_gate_fields("fail", "allocation", "pass",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "pass", "pass", "ready",
                                              1, 0, 1, "pass", 0, 0, 0);
            printf("status: materialization-failed-cleaned\n");
        } else {
            print_materialization_gate_fields("fail", "preflight", "fail",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                              "fail", "fail", "ready",
                                              0, 0, 0, "not-needed", 0, 0, 0);
            printf("status: materialization-integrity-fail\n");
        }
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_weight_table_get_summary(weights, &summary, &err);
    if (rc != YVEX_OK) {
        yvex_weight_table_close(weights);
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    printf("materialization status: %s\n", yvex_weight_status_name(summary.status));
    printf("model: %s\n", yvex_model_name(ctx.model)[0] ? yvex_model_name(ctx.model) : "unknown");
    printf("backend: %s\n", backend_name);
    print_materialization_gate_fields(summary.materialization_gate,
                                      summary.materialization_phase,
                                      "pass",
                                      model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered",
                                      summary.shape_status,
                                      summary.range_status,
                                      summary.backend_status,
                                      summary.allocation_attempted,
                                      summary.transfer_attempted,
                                      summary.cleanup_attempted,
                                      summary.cleanup_status,
                                      summary.bytes_planned,
                                      summary.bytes_allocated,
                                      summary.bytes_transferred);
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
    yvex_model_ref_clear(&model_ref);
    return 0;
}

static yvex_materialize_scope parse_materialize_scope_name(const char *name)
{
    if (!name) return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
    if (strcmp(name, "selected-tensor") == 0) return YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR;
    if (strcmp(name, "partial-model") == 0) return YVEX_MATERIALIZE_SCOPE_PARTIAL_MODEL;
    if (strcmp(name, "full-model") == 0) return YVEX_MATERIALIZE_SCOPE_FULL_MODEL;
    return YVEX_MATERIALIZE_SCOPE_UNKNOWN;
}

static void print_materialize_gate_report(FILE *fp,
                                          const yvex_materialize_gate_options *options,
                                          const yvex_materialize_gate_summary *summary,
                                          const char *reason)
{
    fprintf(fp, "materialize gate: check\n");
    fprintf(fp, "label: %s\n", summary->label ? summary->label : "");
    fprintf(fp, "family: %s\n", summary->family ? summary->family : "");
    fprintf(fp, "scope: %s\n", yvex_materialize_scope_name(summary->scope));
    fprintf(fp, "model: %s\n", summary->model_path ? summary->model_path : "");
    fprintf(fp, "expected_sha256: %s\n", summary->expected_sha256 ? summary->expected_sha256 : "");
    fprintf(fp, "actual_sha256: %s\n", summary->actual_sha256);
    fprintf(fp, "digest_status: %s\n", summary->digest_status ? summary->digest_status : "unrequested");
    fprintf(fp, "identity_status: %s\n", summary->identity_status ? summary->identity_status : "unrequested");
    fprintf(fp, "metadata_status: %s\n", summary->metadata_status ? summary->metadata_status : "unregistered");
    fprintf(fp, "materialization_gate: %s\n", summary->materialization_gate ? summary->materialization_gate : "fail");
    fprintf(fp, "materialization_phase: %s\n", summary->materialization_phase ? summary->materialization_phase : "preflight");
    fprintf(fp, "integrity_status: %s\n", summary->integrity_status ? summary->integrity_status : "unchecked");
    fprintf(fp, "shape_status: %s\n", summary->shape_status ? summary->shape_status : "unchecked");
    fprintf(fp, "range_status: %s\n", summary->range_status ? summary->range_status : "unchecked");
    fprintf(fp, "backend_status: %s\n", summary->backend_status ? summary->backend_status : "not-opened");
    fprintf(fp, "allocation_attempted: %s\n", summary->allocation_attempted ? "true" : "false");
    fprintf(fp, "transfer_attempted: %s\n", summary->transfer_attempted ? "true" : "false");
    fprintf(fp, "cleanup_attempted: %s\n", summary->cleanup_attempted ? "true" : "false");
    fprintf(fp, "cleanup_status: %s\n", summary->cleanup_status ? summary->cleanup_status : "not-needed");
    fprintf(fp, "bytes_planned: %llu\n", summary->bytes_planned);
    fprintf(fp, "bytes_allocated: %llu\n", summary->bytes_allocated);
    fprintf(fp, "bytes_transferred: %llu\n", summary->bytes_transferred);
    fprintf(fp, "file_bytes: %llu\n", summary->file_bytes);
    fprintf(fp, "tensor_count: %llu\n", summary->tensor_count);
    fprintf(fp, "expected_tensor_matches: %llu\n", summary->expected_tensor_matches);
    fprintf(fp, "expected_tensor_mismatches: %llu\n", summary->expected_tensor_mismatches);
    fprintf(fp, "bytes_materialized_cpu: %llu\n", summary->bytes_materialized_cpu);
    fprintf(fp, "bytes_materialized_cuda: %llu\n", summary->bytes_materialized_cuda);
    fprintf(fp, "cpu: %s\n", yvex_materialize_backend_status_name(summary->cpu_status));
    fprintf(fp, "cuda: %s\n", yvex_materialize_backend_status_name(summary->cuda_status));
    fprintf(fp, "repeat_count: %u\n", summary->repeat_count);
    fprintf(fp, "cleanup_verified: %s\n", summary->cleanup_verified ? "yes" : "no");
    fprintf(fp, "execution_ready: false\n");
    if (options && options->expected_tensor_count == 1 && options->expected_tensors) {
        const yvex_materialize_expected_tensor *t = &options->expected_tensors[0];
        fprintf(fp, "expected_tensor: %s\n", t->name ? t->name : "");
        fprintf(fp, "expected_rank: %u\n", t->rank);
        fprintf(fp, "expected_dims:");
        if (t->rank > 0) fprintf(fp, " %llu", t->dims[0]);
        if (t->rank > 1) fprintf(fp, ",%llu", t->dims[1]);
        if (t->rank > 2) fprintf(fp, ",%llu", t->dims[2]);
        if (t->rank > 3) fprintf(fp, ",%llu", t->dims[3]);
        fprintf(fp, "\n");
        fprintf(fp, "expected_dtype: %s\n", t->dtype ? t->dtype : "");
        fprintf(fp, "expected_bytes: %llu\n", t->bytes);
    }
    fprintf(fp, "failure_class: %s\n",
            yvex_materialize_failure_class_name(summary->failure_class));
    fprintf(fp, "reason: %s\n", reason && reason[0] ? reason : "materialization hardening gate");
    fprintf(fp, "status: %s\n", yvex_materialize_gate_status_name(summary->status));
}

static int command_materialize_gate(int argc, char **argv)
{
    yvex_materialize_gate_options options;
    yvex_materialize_expected_tensor expected;
    yvex_materialize_gate_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *report_out = NULL;
    unsigned long long value;
    int have_tensor = 0;
    int have_rank = 0;
    int have_dims = 0;
    int have_dtype = 0;
    int have_bytes = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&expected, 0, sizeof(expected));
    memset(&summary, 0, sizeof(summary));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("materialize-gate"));
        return 0;
    }
    if (argc < 3 || strcmp(argv[2], "check") != 0) {
        fprintf(stderr, "yvex: materialize-gate requires subcommand check\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--require-cpu") == 0) {
            options.require_cpu = 1;
            options.check_cpu = 1;
            continue;
        }
        if (strcmp(argv[i], "--require-cuda") == 0) {
            options.require_cuda = 1;
            options.check_cuda = 1;
            continue;
        }
        if (strcmp(argv[i], "--check-cleanup") == 0) {
            options.check_cleanup = 1;
            continue;
        }
        if (strcmp(argv[i], "--json") == 0) {
            options.json = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: materialize-gate option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--model") == 0) {
            options.model_path = argv[++i];
        } else if (strcmp(argv[i], "--label") == 0) {
            options.label = argv[++i];
        } else if (strcmp(argv[i], "--family") == 0) {
            options.family = argv[++i];
        } else if (strcmp(argv[i], "--sha256") == 0) {
            options.sha256 = argv[++i];
        } else if (strcmp(argv[i], "--scope") == 0) {
            options.scope = parse_materialize_scope_name(argv[++i]);
            if (options.scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
                fprintf(stderr, "yvex: invalid materialize-gate scope\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--expect-tensor") == 0) {
            expected.name = argv[++i];
            have_tensor = 1;
        } else if (strcmp(argv[i], "--expect-rank") == 0) {
            if (!parse_positive_ull(argv[++i], &value) || value > 4ull) {
                fprintf(stderr, "yvex: invalid --expect-rank\n");
                return 2;
            }
            expected.rank = (unsigned int)value;
            have_rank = 1;
        } else if (strcmp(argv[i], "--expect-dims") == 0) {
            const char *dims_text = argv[++i];
            if (!have_rank || !parse_dims_csv(dims_text, expected.rank, expected.dims)) {
                fprintf(stderr, "yvex: invalid --expect-dims; pass --expect-rank before --expect-dims\n");
                return 2;
            }
            have_dims = 1;
        } else if (strcmp(argv[i], "--expect-dtype") == 0) {
            expected.dtype = argv[++i];
            have_dtype = 1;
        } else if (strcmp(argv[i], "--expect-bytes") == 0) {
            if (!parse_positive_ull(argv[++i], &expected.bytes)) {
                fprintf(stderr, "yvex: invalid --expect-bytes\n");
                return 2;
            }
            have_bytes = 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            const char *backend = argv[++i];
            if (strcmp(backend, "cpu") == 0) {
                options.check_cpu = 1;
            } else if (strcmp(backend, "cuda") == 0) {
                options.check_cuda = 1;
            } else {
                fprintf(stderr, "yvex: materialize-gate backend must be cpu or cuda\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--repeat") == 0) {
            if (!parse_positive_ull(argv[++i], &value) || value > 1000ull) {
                fprintf(stderr, "yvex: invalid --repeat\n");
                return 2;
            }
            options.repeat_count = (unsigned int)value;
        } else if (strcmp(argv[i], "--report-out") == 0) {
            report_out = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown materialize-gate option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help materialize-gate' for usage.\n");
            return 2;
        }
    }

    if (!options.model_path || !options.label || !options.family ||
        options.scope == YVEX_MATERIALIZE_SCOPE_UNKNOWN) {
        fprintf(stderr, "yvex: materialize-gate check requires --model --label --family --scope\n");
        return 2;
    }
    if (have_tensor || have_rank || have_dims || have_dtype || have_bytes) {
        if (!have_tensor || !have_rank || !have_dims || !have_dtype || !have_bytes) {
            fprintf(stderr, "yvex: materialize-gate expected tensor spec must be complete\n");
            return 2;
        }
        options.expected_tensors = &expected;
        options.expected_tensor_count = 1;
    }
    if (options.repeat_count == 0) options.repeat_count = 1;

    rc = yvex_model_ref_resolve(&model_ref, options.model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "materialize-gate");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }
    options.model_path = model_ref.path;
    if ((!options.sha256 || !options.sha256[0]) &&
        model_ref.kind == YVEX_MODEL_REF_ALIAS &&
        model_ref.sha256 && model_ref.sha256[0]) {
        options.sha256 = model_ref.sha256;
    }
    options.metadata_status = model_ref.kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";

    rc = yvex_materialize_gate_check(&options, &summary, &err);
    print_materialize_gate_report(stdout, &options, &summary,
                                  rc == YVEX_OK ? NULL : yvex_error_message(&err));
    if (report_out) {
        FILE *fp = fopen(report_out, "wb");
        if (!fp) {
            fprintf(stderr, "yvex: cannot write report: %s\n", report_out);
            yvex_model_ref_clear(&model_ref);
            return 1;
        }
        print_materialize_gate_report(fp, &options, &summary,
                                      rc == YVEX_OK ? NULL : yvex_error_message(&err));
        fclose(fp);
    }
    yvex_model_ref_clear(&model_ref);
    return rc == YVEX_OK ? 0 : exit_for_status(rc);
}

static void print_model_gate_report(FILE *fp,
                                    const yvex_model_gate_options *options,
                                    const yvex_model_gate_summary *summary,
                                    const char *reason)
{
    fprintf(fp, "model gate: check\n");
    fprintf(fp, "label: %s\n", summary->model_label ? summary->model_label : "");
    fprintf(fp, "family: %s\n", summary->family ? summary->family : "");
    fprintf(fp, "model: %s\n", summary->model_path ? summary->model_path : "");
    fprintf(fp, "expected_sha256: %s\n", summary->expected_sha256 ? summary->expected_sha256 : "");
    fprintf(fp, "actual_sha256: %s\n", summary->actual_sha256);
    fprintf(fp, "digest_status: %s\n", summary->digest_status ? summary->digest_status : "unrequested");
    fprintf(fp, "identity_status: %s\n", summary->identity_status ? summary->identity_status : "unrequested");
    fprintf(fp, "file_bytes: %llu\n", summary->file_bytes);
    fprintf(fp, "tensor_count: %llu\n", summary->tensor_count);
    fprintf(fp, "expected_tensor_matches: %llu\n", summary->expected_tensor_matches);
    fprintf(fp, "expected_tensor_mismatches: %llu\n", summary->expected_tensor_mismatches);
    fprintf(fp, "cpu: %s\n", yvex_model_gate_backend_status_name(summary->cpu_status));
    fprintf(fp, "cuda: %s\n", yvex_model_gate_backend_status_name(summary->cuda_status));
    fprintf(fp, "support_level: %s\n", yvex_model_support_level_name(summary->support_level));
    fprintf(fp, "execution_ready: false\n");
    if (options && options->expected_tensor_count == 1 && options->expected_tensors) {
        const yvex_model_gate_expected_tensor *t = &options->expected_tensors[0];
        fprintf(fp, "expected_tensor: %s\n", t->name ? t->name : "");
        fprintf(fp, "expected_rank: %u\n", t->rank);
        fprintf(fp, "expected_dims:");
        if (t->rank > 0) fprintf(fp, " %llu", t->dims[0]);
        if (t->rank > 1) fprintf(fp, ",%llu", t->dims[1]);
        if (t->rank > 2) fprintf(fp, ",%llu", t->dims[2]);
        if (t->rank > 3) fprintf(fp, ",%llu", t->dims[3]);
        fprintf(fp, "\n");
        fprintf(fp, "expected_dtype: %s\n", t->dtype ? t->dtype : "");
        fprintf(fp, "expected_bytes: %llu\n", t->bytes);
    }
    fprintf(fp, "reason: %s\n", reason && reason[0] ? reason : "selected tensor materialization gate");
    fprintf(fp, "status: %s\n", yvex_model_gate_status_name(summary->status));
}

static int command_model_gate(int argc, char **argv)
{
    yvex_model_gate_options options;
    yvex_model_gate_expected_tensor expected;
    yvex_model_gate_summary summary;
    yvex_model_ref model_ref;
    yvex_error err;
    const char *report_out = NULL;
    unsigned long long value;
    int have_tensor = 0;
    int have_rank = 0;
    int have_dims = 0;
    int have_dtype = 0;
    int have_bytes = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&expected, 0, sizeof(expected));
    memset(&summary, 0, sizeof(summary));
    memset(&model_ref, 0, sizeof(model_ref));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("model-gate"));
        return 0;
    }
    if (argc < 3 || strcmp(argv[2], "check") != 0) {
        fprintf(stderr, "yvex: model-gate requires subcommand check\n");
        return 2;
    }

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--require-cpu") == 0) {
            options.require_cpu = 1;
            options.check_cpu = 1;
            continue;
        }
        if (strcmp(argv[i], "--require-cuda") == 0) {
            options.require_cuda = 1;
            options.check_cuda = 1;
            continue;
        }
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: model-gate option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--model") == 0) {
            options.model_path = argv[++i];
        } else if (strcmp(argv[i], "--label") == 0) {
            options.model_label = argv[++i];
        } else if (strcmp(argv[i], "--family") == 0) {
            options.family = argv[++i];
        } else if (strcmp(argv[i], "--sha256") == 0) {
            options.artifact_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--expect-tensor") == 0) {
            expected.name = argv[++i];
            have_tensor = 1;
        } else if (strcmp(argv[i], "--expect-rank") == 0) {
            if (!parse_positive_ull(argv[++i], &value) || value > 4ull) {
                fprintf(stderr, "yvex: invalid --expect-rank\n");
                return 2;
            }
            expected.rank = (unsigned int)value;
            have_rank = 1;
        } else if (strcmp(argv[i], "--expect-dims") == 0) {
            const char *dims_text = argv[++i];
            if (!have_rank || !parse_dims_csv(dims_text, expected.rank, expected.dims)) {
                fprintf(stderr, "yvex: invalid --expect-dims; pass --expect-rank before --expect-dims\n");
                return 2;
            }
            have_dims = 1;
        } else if (strcmp(argv[i], "--expect-dtype") == 0) {
            expected.dtype = argv[++i];
            have_dtype = 1;
        } else if (strcmp(argv[i], "--expect-bytes") == 0) {
            if (!parse_positive_ull(argv[++i], &expected.bytes)) {
                fprintf(stderr, "yvex: invalid --expect-bytes\n");
                return 2;
            }
            have_bytes = 1;
        } else if (strcmp(argv[i], "--backend") == 0) {
            const char *backend = argv[++i];
            if (strcmp(backend, "cpu") == 0) {
                options.check_cpu = 1;
            } else if (strcmp(backend, "cuda") == 0) {
                options.check_cuda = 1;
            } else {
                fprintf(stderr, "yvex: model-gate backend must be cpu or cuda\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--report-out") == 0) {
            report_out = argv[++i];
        } else {
            fprintf(stderr, "yvex: unknown model-gate option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help model-gate' for usage.\n");
            return 2;
        }
    }

    if (!options.model_path || !options.model_label || !options.family ||
        !have_tensor || !have_rank || !have_dims || !have_dtype || !have_bytes) {
        fprintf(stderr, "yvex: model-gate check requires --model --label --family and one complete --expect-* tensor spec\n");
        return 2;
    }

    options.expected_tensors = &expected;
    options.expected_tensor_count = 1;
    rc = yvex_model_ref_resolve(&model_ref, options.model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "model-gate");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }
    options.model_path = model_ref.path;
    if ((!options.artifact_sha256 || !options.artifact_sha256[0]) &&
        model_ref.kind == YVEX_MODEL_REF_ALIAS &&
        model_ref.sha256 && model_ref.sha256[0]) {
        options.artifact_sha256 = model_ref.sha256;
    }

    rc = yvex_model_gate_check(&options, &summary, &err);
    print_model_gate_report(stdout, &options, &summary,
                            rc == YVEX_OK ? NULL : yvex_error_message(&err));
    if (report_out) {
        FILE *fp = fopen(report_out, "wb");
        if (!fp) {
            fprintf(stderr, "yvex: cannot write report: %s\n", report_out);
            yvex_model_ref_clear(&model_ref);
            return 1;
        }
        print_model_gate_report(fp, &options, &summary,
                                rc == YVEX_OK ? NULL : yvex_error_message(&err));
        fclose(fp);
    }
    yvex_model_ref_clear(&model_ref);
    return rc == YVEX_OK ? 0 : exit_for_status(rc);
}

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

static int models_registry_open(yvex_model_registry **registry,
                                const char *registry_path,
                                int create_if_missing,
                                yvex_error *err)
{
    yvex_model_registry_options options;

    memset(&options, 0, sizeof(options));
    options.registry_path = registry_path;
    options.create_if_missing = create_if_missing;
    return yvex_model_registry_open(registry, &options, err);
}

static void print_model_registry_entry_cli(const yvex_model_registry_entry *entry,
                                           int selected)
{
    if (!entry) return;
    printf("%c %s\n", selected ? '*' : '-', entry->alias ? entry->alias : "");
    printf("  family: %s\n", entry->family ? entry->family : "");
    printf("  model: %s\n", entry->model ? entry->model : "");
    printf("  scope: %s\n", entry->scope ? entry->scope : "");
    printf("  artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("  qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("  calibration: %s\n", entry->calibration ? entry->calibration : "");
    printf("  producer: %s\n", entry->producer ? entry->producer : "");
    printf("  schema_version: %s\n", entry->schema_version ? entry->schema_version : "");
    printf("  support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("  execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    printf("  path: %s\n", entry->path ? entry->path : "");
    printf("  registered_file_size: %llu\n", entry->file_size);
    printf("  registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("  registered_format: %s\n", entry->format ? entry->format : "");
    printf("  registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("  registered_tensor_count: %llu\n", entry->tensor_count);
    printf("  registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("  registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("  registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("  registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("  registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("  registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("  registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("  registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
}

static void print_model_registry_scan_entry_cli(const yvex_model_registry_entry *entry)
{
    if (!entry) return;
    printf("candidate: %s\n", entry->alias ? entry->alias : "");
    printf("path: %s\n", entry->path ? entry->path : "");
    printf("family: %s\n", entry->family ? entry->family : "");
    printf("model: %s\n", entry->model ? entry->model : "");
    printf("scope: %s\n", entry->scope ? entry->scope : "");
    printf("artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("calibration: %s\n", entry->calibration ? entry->calibration : "");
}

static void dims_to_text(const unsigned long long *dims,
                         unsigned int rank,
                         char *out,
                         size_t out_cap)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (used + 1u < out_cap) {
        out[used++] = '[';
        out[used] = '\0';
    }
    for (i = 0; i < rank && used < out_cap; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%llu",
                         i == 0 ? "" : ",",
                         dims[i]);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used + 1u < out_cap) {
        out[used++] = ']';
        out[used] = '\0';
    }
}

static const char *current_support_from_metadata(const yvex_model_registry_entry *entry)
{
    if (entry && entry->primary_tensor_name && entry->primary_tensor_name[0]) {
        return "selected-tensor-materialized";
    }
    if (entry && entry->format && entry->format[0]) {
        return "descriptor-only";
    }
    return "";
}

static void model_ref_registry_entry_view(const yvex_model_ref *ref,
                                          yvex_model_registry_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    if (!ref) return;
    entry->alias = ref->alias;
    entry->path = ref->path;
    entry->sha256 = ref->sha256;
    entry->file_size = ref->registered_file_size;
    entry->format = ref->format;
    entry->architecture = ref->architecture;
    entry->tensor_count = ref->tensor_count;
    entry->known_tensor_bytes = ref->known_tensor_bytes;
    entry->primary_tensor_name = ref->primary_tensor_name;
    entry->primary_tensor_role = ref->primary_tensor_role;
    entry->primary_tensor_dtype = ref->primary_tensor_dtype;
    entry->primary_tensor_rank = ref->primary_tensor_rank;
    entry->primary_tensor_dims = ref->primary_tensor_dims;
    entry->primary_tensor_bytes = ref->primary_tensor_bytes;
    entry->support_level = ref->support_level;
    entry->selected_embedding_ready = ref->selected_embedding_ready;
    entry->selected_embedding_hidden_size = ref->selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = ref->selected_embedding_vocab_size;
    entry->selected_embedding_output_count = ref->selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = ref->selected_embedding_slice_bytes;
    entry->execution_ready = ref->execution_ready;
}

static void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report)
{
    unsigned int i;

    if (!report) return;
    printf("metadata_status: %s\n", report->metadata_status[0] ? report->metadata_status : "unknown");
    printf("readiness_status: %s\n", report->readiness_status[0] ? report->readiness_status : "unknown");
    for (i = 0; i < report->issue_count; ++i) {
        printf("metadata_issue_%u_code: %s\n", i, report->issues[i].code);
        printf("metadata_issue_%u_registered: %s\n", i, report->issues[i].registered_value);
        printf("metadata_issue_%u_current: %s\n", i, report->issues[i].current_value);
    }
}

static int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot,
                                      const char *path,
                                      yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *primary = NULL;
    const yvex_tensor_info *embedding = NULL;
    yvex_selected_embedding_shape selected_shape;
    unsigned long long known_bytes = 0ull;
    unsigned long long i;
    int rc;

    if (!snapshot || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_metadata",
                       "metadata snapshot and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(snapshot->format, sizeof(snapshot->format), "gguf");
    snprintf(snapshot->architecture, sizeof(snapshot->architecture), "%s",
             yvex_arch_name(yvex_model_arch(ctx.model)));

    for (i = 0; i < yvex_tensor_table_count(ctx.table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) {
            continue;
        }
        known_bytes += tensor->storage_bytes;
        if (!primary && strcmp(tensor->name, "token_embd.weight") == 0) {
            primary = tensor;
            embedding = tensor;
        }
    }
    if (!primary && yvex_tensor_table_count(ctx.table) > 0ull) {
        primary = yvex_tensor_table_at(ctx.table, 0);
    }

    if (primary) {
        snprintf(snapshot->primary_tensor_name, sizeof(snapshot->primary_tensor_name),
                 "%s", primary->name ? primary->name : "");
        snprintf(snapshot->primary_tensor_role, sizeof(snapshot->primary_tensor_role),
                 "%s", yvex_tensor_role_name(primary->role));
        snprintf(snapshot->primary_tensor_dtype, sizeof(snapshot->primary_tensor_dtype),
                 "%s", yvex_dtype_name(primary->dtype));
        dims_to_text(primary->dims, primary->rank, snapshot->primary_tensor_dims,
                     sizeof(snapshot->primary_tensor_dims));
        snapshot->entry.primary_tensor_rank = primary->rank;
        snapshot->entry.primary_tensor_bytes = primary->storage_bytes;
    }

    if (embedding) {
        yvex_error shape_err;
        yvex_error_clear(&shape_err);
        memset(&selected_shape, 0, sizeof(selected_shape));
        if (yvex_selected_embedding_shape_validate(embedding, 0u, &selected_shape,
                                                   &shape_err) == YVEX_OK) {
            snapshot->entry.selected_embedding_ready = 1;
            snapshot->entry.selected_embedding_hidden_size = selected_shape.hidden_size;
            snapshot->entry.selected_embedding_vocab_size = selected_shape.vocab_size;
            snapshot->entry.selected_embedding_output_count = selected_shape.output_count;
            snapshot->entry.selected_embedding_slice_bytes = selected_shape.slice_bytes;
        } else {
            yvex_error_clear(&shape_err);
        }
    }

    snapshot->entry.path = path;
    snapshot->entry.format = snapshot->format;
    snapshot->entry.architecture = snapshot->architecture;
    snapshot->entry.tensor_count = yvex_tensor_table_count(ctx.table);
    snapshot->entry.known_tensor_bytes = known_bytes;
    snapshot->entry.primary_tensor_name = snapshot->primary_tensor_name;
    snapshot->entry.primary_tensor_role = snapshot->primary_tensor_role;
    snapshot->entry.primary_tensor_dtype = snapshot->primary_tensor_dtype;
    snapshot->entry.primary_tensor_dims = snapshot->primary_tensor_dims;
    snprintf(snapshot->support_level, sizeof(snapshot->support_level), "%s",
             current_support_from_metadata(&snapshot->entry));
    snapshot->entry.support_level = snapshot->support_level;

    close_model_context(&ctx);
    return YVEX_OK;
}

static int populate_registry_identity(yvex_model_registry_entry *entry,
                                      char sha256[YVEX_SHA256_HEX_CAP],
                                      char format[16],
                                      char architecture[64],
                                      char primary_name[128],
                                      char primary_role[64],
                                      char primary_dtype[32],
                                      char primary_dims[128],
                                      yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot snapshot;
    int rc;

    if (!entry || !entry->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_add_identity",
                       "registry entry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_artifact_identity_read(entry->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = populate_registry_metadata(&snapshot, entry->path, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(sha256, YVEX_SHA256_HEX_CAP, "%s", identity.sha256);
    snprintf(format, 16u, "%s", snapshot.format);
    snprintf(architecture, 64u, "%s", snapshot.architecture);
    snprintf(primary_name, 128u, "%s", snapshot.primary_tensor_name);
    snprintf(primary_role, 64u, "%s", snapshot.primary_tensor_role);
    snprintf(primary_dtype, 32u, "%s", snapshot.primary_tensor_dtype);
    snprintf(primary_dims, 128u, "%s", snapshot.primary_tensor_dims);

    entry->sha256 = sha256;
    entry->file_size = identity.file_size;
    entry->format = format;
    entry->architecture = architecture;
    entry->tensor_count = snapshot.entry.tensor_count;
    entry->known_tensor_bytes = snapshot.entry.known_tensor_bytes;
    entry->primary_tensor_name = primary_name;
    entry->primary_tensor_role = primary_role;
    entry->primary_tensor_dtype = primary_dtype;
    entry->primary_tensor_rank = snapshot.entry.primary_tensor_rank;
    entry->primary_tensor_dims = primary_dims;
    entry->primary_tensor_bytes = snapshot.entry.primary_tensor_bytes;
    entry->selected_embedding_ready = snapshot.entry.selected_embedding_ready;
    entry->selected_embedding_hidden_size = snapshot.entry.selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = snapshot.entry.selected_embedding_vocab_size;
    entry->selected_embedding_output_count = snapshot.entry.selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = snapshot.entry.selected_embedding_slice_bytes;

    return YVEX_OK;
}

static int parse_models_registry_option(int argc, char **argv, int start, const char **registry_path)
{
    int i;

    for (i = start; i < argc; ++i) {
        if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for future machine-readable output; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_models_add_options(int argc, char **argv,
                                    yvex_cli_models_add_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: models add option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--registry") == 0) options->registry_path = argv[++i];
        else if (strcmp(argv[i], "--path") == 0) options->path = argv[++i];
        else if (strcmp(argv[i], "--alias") == 0) options->alias = argv[++i];
        else if (strcmp(argv[i], "--family") == 0) options->family = argv[++i];
        else if (strcmp(argv[i], "--model") == 0) options->model = argv[++i];
        else if (strcmp(argv[i], "--scope") == 0) options->scope = argv[++i];
        else if (strcmp(argv[i], "--class") == 0) options->artifact_class = argv[++i];
        else if (strcmp(argv[i], "--qprofile") == 0) options->qprofile = argv[++i];
        else if (strcmp(argv[i], "--calibration") == 0) options->calibration = argv[++i];
        else if (strcmp(argv[i], "--sha256") == 0) options->sha256 = argv[++i];
        else if (strcmp(argv[i], "--support-level") == 0) options->support_level = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown models add option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int command_models_scan(int argc, char **argv)
{
    yvex_model_registry_entry *entries = NULL;
    yvex_error err;
    const char *root = NULL;
    const char *registry_path = NULL;
    unsigned long long count = 0;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    for (i = 3; (int)i < argc; ++i) {
        if (strcmp(argv[i], "--root") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --root requires a directory\n");
                return 2;
            }
            root = argv[++i];
        } else if (strcmp(argv[i], "--registry") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --registry requires a file\n");
                return 2;
            }
            registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for model selection work compatibility; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models scan option: %s\n", argv[i]);
            return 2;
        }
    }
    (void)registry_path;
    if (!root) {
        fprintf(stderr, "yvex: models scan requires --root DIR\n");
        return 2;
    }
    rc = yvex_model_registry_scan_root(root, &entries, &count, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    printf("models: scan\n");
    printf("root: %s\n", root);
    for (i = 0; i < count; ++i) {
        if (i > 0) printf("\n");
        print_model_registry_scan_entry_cli(&entries[i]);
    }
    printf("candidates: %llu\n", count);
    printf("status: models-scan\n");
    yvex_model_registry_scan_free(entries, count);
    return 0;
}

static int command_models_add(int argc, char **argv)
{
    yvex_cli_models_add_options cli_options;
    yvex_model_registry *registry = NULL;
    yvex_model_registry_entry derived;
    yvex_model_registry_entry entry;
    yvex_error err;
    char registered_sha256[YVEX_SHA256_HEX_CAP];
    char registered_format[16];
    char registered_architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    int have_derived = 0;
    int rc;

    yvex_error_clear(&err);
    memset(&derived, 0, sizeof(derived));
    memset(&entry, 0, sizeof(entry));
    memset(registered_sha256, 0, sizeof(registered_sha256));
    memset(registered_format, 0, sizeof(registered_format));
    memset(registered_architecture, 0, sizeof(registered_architecture));
    memset(primary_tensor_name, 0, sizeof(primary_tensor_name));
    memset(primary_tensor_role, 0, sizeof(primary_tensor_role));
    memset(primary_tensor_dtype, 0, sizeof(primary_tensor_dtype));
    memset(primary_tensor_dims, 0, sizeof(primary_tensor_dims));
    rc = parse_models_add_options(argc, argv, &cli_options);
    if (rc != 0) return rc;
    if (!cli_options.path) {
        fprintf(stderr, "yvex: models add requires --path FILE\n");
        return 2;
    }
    if (yvex_model_registry_entry_derive_from_path(&derived, cli_options.path, &err) == YVEX_OK) {
        have_derived = 1;
    } else {
        yvex_error_clear(&err);
    }
    if (!cli_options.alias && !have_derived) {
        fprintf(stderr, "yvex: models add requires --alias when filename is not canonical\n");
        return 2;
    }
    entry.alias = cli_options.alias ? cli_options.alias : derived.alias;
    entry.family = cli_options.family ? cli_options.family : (have_derived ? derived.family : "");
    entry.model = cli_options.model ? cli_options.model : (have_derived ? derived.model : "");
    entry.scope = cli_options.scope ? cli_options.scope : (have_derived ? derived.scope : "");
    entry.artifact_class = cli_options.artifact_class ? cli_options.artifact_class : (have_derived ? derived.artifact_class : "");
    entry.qprofile = cli_options.qprofile ? cli_options.qprofile : (have_derived ? derived.qprofile : "");
    entry.calibration = cli_options.calibration ? cli_options.calibration : (have_derived ? derived.calibration : "");
    entry.producer = have_derived ? derived.producer : "yvex";
    entry.schema_version = have_derived ? derived.schema_version : "v1";
    entry.path = cli_options.path;
    entry.sha256 = "";
    entry.file_size = 0ull;
    entry.format = "";
    entry.architecture = "";
    entry.tensor_count = 0ull;
    entry.known_tensor_bytes = 0ull;
    entry.primary_tensor_name = "";
    entry.primary_tensor_role = "";
    entry.primary_tensor_dtype = "";
    entry.primary_tensor_rank = 0u;
    entry.primary_tensor_dims = "";
    entry.primary_tensor_bytes = 0ull;
    entry.support_level = cli_options.support_level ? cli_options.support_level : "";
    entry.selected_embedding_ready = 0;
    entry.selected_embedding_hidden_size = 0ull;
    entry.selected_embedding_vocab_size = 0ull;
    entry.selected_embedding_output_count = 0ull;
    entry.selected_embedding_slice_bytes = 0ull;
    entry.execution_ready = 0;

    rc = populate_registry_identity(&entry,
                                    registered_sha256,
                                    registered_format,
                                    registered_architecture,
                                    primary_tensor_name,
                                    primary_tensor_role,
                                    primary_tensor_dtype,
                                    primary_tensor_dims,
                                    &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (cli_options.sha256 && cli_options.sha256[0] &&
        strcmp(cli_options.sha256, registered_sha256) != 0) {
        yvex_error_setf(&err, YVEX_ERR_STATE, "models_add_identity",
                        "sha256 mismatch: expected %s got %s",
                        cli_options.sha256, registered_sha256);
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_STATE));
    }

    rc = models_registry_open(&registry, cli_options.registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_add(registry, &entry, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, cli_options.registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: add\n");
    printf("alias: %s\n", entry.alias);
    printf("path: %s\n", entry.path);
    printf("registered_file_size: %llu\n", entry.file_size);
    printf("registered_sha256: %s\n", entry.sha256);
    printf("registered_format: %s\n", entry.format);
    printf("registered_architecture: %s\n", entry.architecture);
    printf("registered_tensor_count: %llu\n", entry.tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry.known_tensor_bytes);
    printf("registered_primary_tensor: %s\n", entry.primary_tensor_name);
    printf("registered_primary_role: %s\n", entry.primary_tensor_role);
    printf("registered_primary_dtype: %s\n", entry.primary_tensor_dtype);
    printf("registered_primary_rank: %u\n", entry.primary_tensor_rank);
    printf("registered_primary_dims: %s\n", entry.primary_tensor_dims);
    printf("registered_primary_bytes: %llu\n", entry.primary_tensor_bytes);
    printf("registered_selected_embedding_ready: %s\n",
           entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry.selected_embedding_hidden_size);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry.selected_embedding_vocab_size);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry.selected_embedding_output_count);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry.selected_embedding_slice_bytes);
    printf("identity_status: recorded\n");
    printf("status: models-added\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_list(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    char selected_alias[256];
    unsigned long long i;
    unsigned long long count;
    int rc;

    yvex_error_clear(&err);
    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    selected_alias[0] = '\0';
    if (selected && selected->alias) {
        snprintf(selected_alias, sizeof(selected_alias), "%s", selected->alias);
    }
    count = yvex_model_registry_count(registry);
    printf("models: list\n");
    for (i = 0; i < count; ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
        print_model_registry_entry_cli(entry, is_selected);
    }
    printf("count: %llu\n", count);
    printf("status: models-list\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_use(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models use requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_select(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: use\n");
    printf("selected: %s\n", alias);
    printf("status: models-selected\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_current(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    int rc;

    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    printf("models: current\n");
    if (selected) {
        printf("selected: %s\n", selected->alias);
        printf("path: %s\n", selected->path);
        printf("registered_file_size: %llu\n", selected->file_size);
        printf("registered_sha256: %s\n", selected->sha256 && selected->sha256[0] ? selected->sha256 : "absent");
        printf("registered_format: %s\n", selected->format ? selected->format : "");
        printf("registered_architecture: %s\n", selected->architecture ? selected->architecture : "");
        printf("registered_tensor_count: %llu\n", selected->tensor_count);
        printf("registered_known_tensor_bytes: %llu\n", selected->known_tensor_bytes);
        printf("registered_primary_tensor: %s\n", selected->primary_tensor_name ? selected->primary_tensor_name : "");
        printf("registered_primary_role: %s\n", selected->primary_tensor_role ? selected->primary_tensor_role : "");
        printf("registered_primary_dtype: %s\n", selected->primary_tensor_dtype ? selected->primary_tensor_dtype : "");
        printf("registered_primary_rank: %u\n", selected->primary_tensor_rank);
        printf("registered_primary_dims: %s\n", selected->primary_tensor_dims ? selected->primary_tensor_dims : "");
        printf("metadata_status: %s\n",
               selected->primary_tensor_name && selected->primary_tensor_name[0] ? "recorded" : "missing");
        printf("execution_ready: %s\n", selected->execution_ready ? "true" : "false");
        printf("status: models-current\n");
    } else {
        printf("selected: none\n");
        printf("status: models-none\n");
    }
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_verify(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *entry;
    const char *alias;
    const char *identity_status = "unknown";
    const char *digest_status = "unknown";
    const char *metadata_status = "not-checked";
    const char *readiness_status = "not-checked";
    const char *status = "models-identity-fail";
    const char *reason = "";
    int pass = 0;
    int metadata_checked = 0;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models verify requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }

    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(entry->path, &identity, &err);
    if (rc != YVEX_OK) {
        identity_status = "fail";
        digest_status = "fail";
        reason = yvex_error_message(&err);
    } else if (!entry->sha256 || !entry->sha256[0] ||
               !yvex_sha256_hex_is_valid(entry->sha256)) {
        identity_status = "missing";
        digest_status = "missing";
        reason = "registered alias lacks digest identity; re-add model";
    } else if (strcmp(entry->sha256, identity.sha256) != 0 ||
               (entry->file_size != 0ull && entry->file_size != identity.file_size)) {
        identity_status = "fail";
        digest_status = "fail";
        reason = "digest mismatch for registered alias";
    } else {
        identity_status = "pass";
        digest_status = "pass";
        reason = "current file identity matches registered alias";
        pass = 1;
        memset(&current_metadata, 0, sizeof(current_metadata));
        memset(&metadata_report, 0, sizeof(metadata_report));
        rc = populate_registry_metadata(&current_metadata, entry->path, &err);
        if (rc != YVEX_OK) {
            metadata_status = "fail";
            readiness_status = "fail";
            reason = "current artifact metadata could not be parsed";
            pass = 0;
            status = "models-metadata-drift";
        } else {
            rc = yvex_model_registry_compare_metadata(entry,
                                                      &current_metadata.entry,
                                                      &metadata_report,
                                                      &err);
            metadata_checked = 1;
            if (rc != YVEX_OK) {
                metadata_status = "fail";
                readiness_status = "fail";
                reason = yvex_error_message(&err);
                pass = 0;
                status = "models-metadata-drift";
            } else {
                metadata_status = metadata_report.metadata_status;
                readiness_status = metadata_report.readiness_status;
                if (strcmp(metadata_status, "pass") == 0 &&
                    strcmp(readiness_status, "pass") == 0) {
                    status = "models-identity-pass";
                } else if (strcmp(metadata_status, "missing") == 0 ||
                           strcmp(readiness_status, "missing") == 0) {
                    reason = "registered alias lacks metadata summary; re-add model";
                    pass = 0;
                    status = "models-metadata-missing";
                } else {
                    reason = "registered alias metadata does not match current artifact facts";
                    pass = 0;
                    status = "models-metadata-drift";
                }
            }
        }
    }
    if (strcmp(identity_status, "missing") == 0) {
        status = "models-identity-missing";
    } else if (strcmp(identity_status, "fail") == 0) {
        status = "models-identity-fail";
    }

    printf("models: verify\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("current_file_size: %llu\n", identity.file_size);
    printf("digest_status: %s\n", digest_status);
    printf("identity_status: %s\n", identity_status);
    printf("registered_support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("current_support_level: %s\n",
           metadata_checked ? current_metadata.entry.support_level : "not-checked");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("current_architecture: %s\n",
           metadata_checked ? current_metadata.entry.architecture : "not-checked");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("current_tensor_count: %llu\n",
           metadata_checked ? current_metadata.entry.tensor_count : 0ull);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("current_known_tensor_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.known_tensor_bytes : 0ull);
    printf("registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("current_primary_tensor: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_name : "not-checked");
    printf("registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("current_primary_role: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_role : "not-checked");
    printf("registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("current_primary_dtype: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dtype : "not-checked");
    printf("registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("current_primary_rank: %u\n",
           metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
    printf("registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("current_primary_dims: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dims : "not-checked");
    printf("registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("current_primary_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
    printf("registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
    printf("current_selected_embedding_ready: %s\n",
           metadata_checked && current_metadata.entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry->selected_embedding_hidden_size);
    printf("current_selected_embedding_hidden_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_hidden_size : 0ull);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry->selected_embedding_vocab_size);
    printf("current_selected_embedding_vocab_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_vocab_size : 0ull);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry->selected_embedding_output_count);
    printf("current_selected_embedding_output_count: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_output_count : 0ull);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry->selected_embedding_slice_bytes);
    printf("current_selected_embedding_slice_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_slice_bytes : 0ull);
    if (metadata_checked) {
        print_metadata_drift_cli(&metadata_report);
    } else {
        printf("metadata_status: %s\n", metadata_status);
        printf("readiness_status: %s\n", readiness_status);
    }
    printf("reason: %s\n", reason);
    printf("status: %s\n", status);
    yvex_model_registry_close(registry);
    return pass ? 0 : exit_for_status(YVEX_ERR_STATE);
}

static int command_models_remove(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models remove requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_remove(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: remove\n");
    printf("removed: %s\n", alias);
    printf("status: models-removed\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_inspect(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const yvex_model_registry_entry *entry;
    const yvex_gguf_header *header;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models inspect requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }
    printf("models: inspect\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("family: %s\n", entry->family);
    printf("model: %s\n", entry->model);
    printf("scope: %s\n", entry->scope);
    printf("artifact_class: %s\n", entry->artifact_class);
    printf("qprofile: %s\n", entry->qprofile);
    printf("calibration: %s\n", entry->calibration);
    printf("support_level: %s\n", entry->support_level);
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("registered_format: %s\n", entry->format ? entry->format : "");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("primary_tensor_name: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("primary_tensor_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("primary_tensor_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("primary_tensor_rank: %u\n", entry->primary_tensor_rank);
    printf("primary_tensor_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("primary_tensor_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("selected_embedding_ready: %s\n", entry->selected_embedding_ready ? "true" : "false");
    printf("selected_embedding_hidden_size: %llu\n", entry->selected_embedding_hidden_size);
    printf("selected_embedding_vocab_size: %llu\n", entry->selected_embedding_vocab_size);
    printf("selected_embedding_output_count: %llu\n", entry->selected_embedding_output_count);
    printf("selected_embedding_slice_bytes: %llu\n", entry->selected_embedding_slice_bytes);
    printf("execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    rc = open_model_context(entry->path, &ctx, &err);
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(ctx.gguf);
        printf("gguf:\n");
        printf("  version: %u\n", header->version);
        printf("  tensor_count: %llu\n", header->tensor_count);
        close_model_context(&ctx);
    } else {
        printf("gguf:\n");
        printf("  status: unavailable\n");
        printf("  reason: %s\n", yvex_error_message(&err));
        yvex_error_clear(&err);
    }
    printf("status: models-inspect\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models(int argc, char **argv)
{
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("models"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: models requires scan, add, list, use, current, verify, inspect, or remove\n");
        return 2;
    }
    if (strcmp(argv[2], "scan") == 0) return command_models_scan(argc, argv);
    if (strcmp(argv[2], "add") == 0) return command_models_add(argc, argv);
    if (strcmp(argv[2], "list") == 0) return command_models_list(argc, argv);
    if (strcmp(argv[2], "use") == 0) return command_models_use(argc, argv);
    if (strcmp(argv[2], "current") == 0) return command_models_current(argc, argv);
    if (strcmp(argv[2], "verify") == 0) return command_models_verify(argc, argv);
    if (strcmp(argv[2], "inspect") == 0) return command_models_inspect(argc, argv);
    if (strcmp(argv[2], "remove") == 0) return command_models_remove(argc, argv);
    fprintf(stderr, "yvex: unknown models subcommand: %s\n", argv[2]);
    return 2;
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

static int parse_quant_job_create_options(int argc, char **argv,
                                          yvex_quant_job_options *options,
                                          const char **out_path)
{
    int i;

    memset(options, 0, sizeof(*options));
    *out_path = NULL;
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--name") == 0) options->name = argv[++i];
        else if (strcmp(argv[i], "--arch") == 0) options->architecture = argv[++i];
        else if (strcmp(argv[i], "--tool") == 0) options->tool = yvex_quant_job_tool_from_name(argv[++i]);
        else if (strcmp(argv[i], "--tool-path") == 0) options->tool_path = argv[++i];
        else if (strcmp(argv[i], "--source-manifest") == 0) options->source_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--native-source") == 0) options->native_source_dir = argv[++i];
        else if (strcmp(argv[i], "--template") == 0) options->template_path = argv[++i];
        else if (strcmp(argv[i], "--quant-policy") == 0) options->quant_policy_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix-manifest") == 0) options->imatrix_manifest_path = argv[++i];
        else if (strcmp(argv[i], "--imatrix") == 0) options->imatrix_path = argv[++i];
        else if (strcmp(argv[i], "--out-gguf") == 0) options->out_gguf_path = argv[++i];
        else if (strcmp(argv[i], "--log") == 0) options->log_path = argv[++i];
        else if (strcmp(argv[i], "--status") == 0) options->status = yvex_quant_job_status_from_name(argv[++i]);
        else if (strcmp(argv[i], "--command") == 0) options->command = argv[++i];
        else if (strcmp(argv[i], "--out") == 0) *out_path = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_quant_job_manifest_option(int argc, char **argv, const char **manifest_path)
{
    int i;

    *manifest_path = NULL;
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: quant-job option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--manifest") == 0) *manifest_path = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown quant-job option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static void print_quant_job_summary(const char *mode,
                                    const char *path,
                                    const yvex_quant_job_summary *summary)
{
    printf("quant job: %s\n", mode);
    if (path) printf("manifest: %s\n", path);
    printf("name: %s\n", summary->name ? summary->name : "");
    printf("architecture: %s\n", summary->architecture ? summary->architecture : "");
    printf("tool: %s\n", yvex_quant_job_tool_name(summary->tool));
    printf("tool_path: %s\n", summary->tool_path ? summary->tool_path : "");
    printf("native_source: %s\n", summary->native_source_dir ? summary->native_source_dir : "");
    printf("template: %s\n", summary->template_path ? summary->template_path : "");
    printf("out_gguf: %s\n", summary->out_gguf_path ? summary->out_gguf_path : "");
    printf("log: %s\n", summary->log_path ? summary->log_path : "");
    printf("tool_exists: %s\n", summary->tool_exists ? "yes" : "no");
    printf("source_exists: %s\n", summary->source_exists ? "yes" : "no");
    printf("template_exists: %s\n", summary->template_exists ? "yes" : "no");
    printf("imatrix_exists: %s\n", summary->imatrix_exists ? "yes" : "no");
    printf("output_exists: %s\n", summary->output_exists ? "yes" : "no");
    printf("status: %s\n", yvex_quant_job_status_name(summary->status));
}

static int command_quant_job(int argc, char **argv)
{
    yvex_quant_job_summary summary;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    memset(&summary, 0, sizeof(summary));

    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(stdout, find_command("quant-job"));
        return 0;
    }
    if (argc < 3 || (strcmp(argv[2], "create") != 0 &&
                     strcmp(argv[2], "inspect") != 0 &&
                     strcmp(argv[2], "validate") != 0)) {
        fprintf(stderr, "yvex: quant-job requires create, inspect, or validate\n");
        return 2;
    }

    if (strcmp(argv[2], "create") == 0) {
        yvex_quant_job_options options;
        const char *out_path = NULL;

        rc = parse_quant_job_create_options(argc, argv, &options, &out_path);
        if (rc != 0) return rc;
        if (!options.name || !options.architecture || !options.tool_path ||
            !options.native_source_dir || !options.template_path ||
            !options.out_gguf_path || !options.log_path || !options.command ||
            !out_path || options.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
            fprintf(stderr, "yvex: quant-job create requires --name --arch --tool --tool-path --native-source --template --out-gguf --log --status --command --out\n");
            return 2;
        }
        rc = yvex_quant_job_write_json(out_path, &options, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        printf("quant job: written\n");
        printf("name: %s\n", summary.name);
        printf("architecture: %s\n", summary.architecture);
        printf("tool: %s\n", yvex_quant_job_tool_name(summary.tool));
        printf("tool_exists: %s\n", summary.tool_exists ? "yes" : "no");
        printf("source_exists: %s\n", summary.source_exists ? "yes" : "no");
        printf("template_exists: %s\n", summary.template_exists ? "yes" : "no");
        printf("imatrix_exists: %s\n", summary.imatrix_exists ? "yes" : "no");
        printf("output_exists: %s\n", summary.output_exists ? "yes" : "no");
        printf("status: %s\n", yvex_quant_job_status_name(summary.status));
        printf("out: %s\n", out_path);
        printf("status: quant-job-written\n");
        return 0;
    }

    {
        const char *manifest_path = NULL;

        rc = parse_quant_job_manifest_option(argc, argv, &manifest_path);
        if (rc != 0) return rc;
        if (!manifest_path) {
            fprintf(stderr, "yvex: quant-job %s requires --manifest FILE\n", argv[2]);
            return 2;
        }
        rc = yvex_quant_job_validate(manifest_path, &summary, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        print_quant_job_summary(argv[2], manifest_path, &summary);
        printf("status: quant-job-%s\n", strcmp(argv[2], "validate") == 0 ? "valid" : "manifest");
        return 0;
    }
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

static int cli_test_env_enabled(const char *name)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void init_graph_guard_report(yvex_cli_graph_guard_report *report,
                                    const char *graph_kind,
                                    int needs_slice_range,
                                    const yvex_model_ref *model_ref)
{
    memset(report, 0, sizeof(*report));
    report->guard_status = "fail";
    report->phase = "preflight";
    report->graph_kind = graph_kind ? graph_kind : "unknown";
    report->integrity_status = "unchecked";
    report->identity_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->metadata_status = model_ref && model_ref->kind == YVEX_MODEL_REF_ALIAS ? "pass" : "unregistered";
    report->shape_status = "unchecked";
    report->range_status = "unchecked";
    report->slice_range_status = needs_slice_range ? "unchecked" : "not-needed";
    report->backend_status = "not-opened";
    report->backend_op_status = "unchecked";
    report->cleanup_status = "not-needed";
}

static const yvex_tensor_info *cli_find_first_rmsnorm_tensor(const yvex_tensor_table *tensors)
{
    static const char *preferred[] = {
        "blk.0.attn_norm.weight",
        "blk.0.attention_norm.weight",
        "blk.0.input_layernorm.weight",
        "model.layers.0.input_layernorm.weight",
    };
    unsigned int i;
    unsigned long long count;
    unsigned long long index;

    if (!tensors) {
        return NULL;
    }
    for (i = 0; i < sizeof(preferred) / sizeof(preferred[0]); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_find(tensors, preferred[i]);
        if (tensor) {
            return tensor;
        }
    }
    count = yvex_tensor_table_count(tensors);
    for (index = 0; index < count; ++index) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, index);
        if (tensor && tensor->role == YVEX_TENSOR_ROLE_ATTENTION_NORM) {
            return tensor;
        }
    }
    return NULL;
}

static int cli_has_rmsnorm_epsilon(const yvex_gguf *gguf)
{
    static const char *keys[] = {
        "llama.attention.layer_norm_rms_epsilon",
        "deepseek2.attention.layer_norm_rms_epsilon",
        "general.rms_norm_epsilon",
    };
    unsigned int i;

    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, keys[i]);
        double epsilon = 0.0;
        if (value && yvex_gguf_value_as_f64(value, &epsilon) == YVEX_OK && epsilon > 0.0) {
            return 1;
        }
    }
    return 0;
}

static void print_graph_guard_report(const yvex_cli_graph_guard_report *report)
{
    printf("graph_integrity_guard: %s\n", report->guard_status ? report->guard_status : "fail");
    printf("graph_execution_phase: %s\n", report->phase ? report->phase : "preflight");
    printf("graph_kind: %s\n", report->graph_kind ? report->graph_kind : "unknown");
    printf("integrity_status: %s\n", report->integrity_status ? report->integrity_status : "unchecked");
    printf("identity_status: %s\n", report->identity_status ? report->identity_status : "unregistered");
    printf("metadata_status: %s\n", report->metadata_status ? report->metadata_status : "unregistered");
    printf("shape_status: %s\n", report->shape_status ? report->shape_status : "unchecked");
    printf("range_status: %s\n", report->range_status ? report->range_status : "unchecked");
    printf("slice_range_status: %s\n", report->slice_range_status ? report->slice_range_status : "unchecked");
    printf("backend_status: %s\n", report->backend_status ? report->backend_status : "not-opened");
    printf("backend_op_status: %s\n", report->backend_op_status ? report->backend_op_status : "unchecked");
    printf("dispatch_attempted: %s\n", report->dispatch_attempted ? "true" : "false");
    printf("reference_read_attempted: %s\n", report->reference_read_attempted ? "true" : "false");
    printf("output_allocation_attempted: %s\n", report->output_allocation_attempted ? "true" : "false");
    printf("cleanup_attempted: %s\n", report->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n", report->cleanup_status ? report->cleanup_status : "not-needed");
    printf("output_bytes_planned: %llu\n", report->output_bytes_planned);
    printf("output_bytes_allocated: %llu\n", report->output_bytes_allocated);
    printf("reference_bytes_planned: %llu\n", report->reference_bytes_planned);
}

static unsigned long long cli_checksum_bytes(const void *data, unsigned long long len)
{
    const unsigned char *bytes = (const unsigned char *)data;
    unsigned long long checksum = 1469598103934665603ull;
    unsigned long long i;

    for (i = 0; i < len; ++i) {
        checksum ^= (unsigned long long)bytes[i];
        checksum *= 1099511628211ull;
    }
    return checksum;
}

static float cli_abs_float(float x)
{
    return x < 0.0f ? -x : x;
}

static double cli_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double cli_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static double cli_exp_double(double x)
{
    const double ln2 = 0.69314718055994530942;
    double term;
    double sum;
    int n = 0;
    unsigned int i;

    if (x < -60.0) {
        return 0.0;
    }
    if (x > 60.0) {
        x = 60.0;
    }
    while (x > 0.5) {
        x -= ln2;
        ++n;
    }
    while (x < -0.5) {
        x += ln2;
        --n;
    }
    term = 1.0;
    sum = 1.0;
    for (i = 1u; i <= 18u; ++i) {
        term *= x / (double)i;
        sum += term;
    }
    while (n > 0) {
        sum *= 2.0;
        --n;
    }
    while (n < 0) {
        sum *= 0.5;
        ++n;
    }
    return sum;
}

static double cli_nth_root_double(double x, unsigned long long n)
{
    double lo = 1.0;
    double hi = x > 1.0 ? x : 1.0;
    unsigned int iter;

    if (x <= 0.0 || n == 0) {
        return 0.0;
    }
    if (n == 1) {
        return x;
    }
    for (iter = 0; iter < 96u; ++iter) {
        double mid = 0.5 * (lo + hi);
        double acc = 1.0;
        unsigned long long i;

        for (i = 0; i < n; ++i) {
            acc *= mid;
            if (acc > x) {
                break;
            }
        }
        if (acc > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

static double cli_wrap_radians(double x)
{
    const double two_pi = 6.28318530717958647692;

    while (x > 3.14159265358979323846) {
        x -= two_pi;
    }
    while (x < -3.14159265358979323846) {
        x += two_pi;
    }
    return x;
}

static void cli_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = cli_wrap_radians(x);
    x2 = x * x;
    s = x * (1.0 -
             (x2 / 6.0) +
             ((x2 * x2) / 120.0) -
             ((x2 * x2 * x2) / 5040.0) +
             ((x2 * x2 * x2 * x2) / 362880.0));
    c = 1.0 -
        (x2 / 2.0) +
        ((x2 * x2) / 24.0) -
        ((x2 * x2 * x2) / 720.0) +
        ((x2 * x2 * x2 * x2) / 40320.0);
    if (cli_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (cli_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static void cli_rope_fill_input(float *values, unsigned long long head_dim)
{
    unsigned long long i;

    for (i = 0; i < head_dim; ++i) {
        values[i] = (float)((double)(i + 1ull) * 0.25);
    }
}

static void cli_rope_reference(const float *input,
                               unsigned long long head_dim,
                               unsigned long long position,
                               float rope_base,
                               float *out)
{
    unsigned long long pair_count = head_dim / 2ull;
    unsigned long long pair;
    double inverse_root = 1.0 / cli_nth_root_double((double)rope_base, pair_count);
    double frequency = 1.0;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input[even_index];
        double odd = (double)input[odd_index];
        double angle = (double)position * frequency;

        cli_sincos_double(angle, &sine, &cosine);
        out[even_index] = (float)((even * cosine) - (odd * sine));
        out[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }
}

static void cli_attention_fill_inputs(float *query,
                                      float *keys,
                                      float *values,
                                      unsigned long long seq_len,
                                      unsigned long long head_dim)
{
    unsigned long long i;
    unsigned long long d;

    for (d = 0; d < head_dim; ++d) {
        query[d] = (float)(0.02 + ((double)(d + 1ull) * 0.01));
    }
    for (i = 0; i < seq_len; ++i) {
        for (d = 0; d < head_dim; ++d) {
            keys[(i * head_dim) + d] =
                (float)(0.03 + ((double)(i + 1ull) * 0.04) + ((double)(d + 1ull) * 0.002));
            values[(i * head_dim) + d] =
                (float)(0.10 + ((double)(i + 1ull) * 0.08) + ((double)(d + 1ull) * 0.01));
        }
    }
}

static void cli_attention_reference(const float *query,
                                    const float *keys,
                                    const float *values,
                                    unsigned long long seq_len,
                                    unsigned long long position,
                                    unsigned long long head_dim,
                                    float scale,
                                    int causal,
                                    float *score_scratch,
                                    float *probability_scratch,
                                    float *out)
{
    unsigned long long visible_count = causal ? position + 1ull : seq_len;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_scratch[i] = 0.0f;
            probability_scratch[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)query[d] * (double)keys[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_scratch[i] = (float)score;
        if (i == 0ull || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = cli_exp_double((double)score_scratch[i] - max_score);
        probability_scratch[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        for (d = 0; d < head_dim; ++d) {
            out[d] = 0.0f;
        }
        return;
    }
    for (i = 0; i < visible_count; ++i) {
        probability_scratch[i] = (float)((double)probability_scratch[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)probability_scratch[i] * (double)values[(i * head_dim) + d];
        }
        out[d] = (float)value;
    }
}

static void cli_matmul_fill_inputs(float *input,
                                   float *weight,
                                   unsigned long long m,
                                   unsigned long long k,
                                   unsigned long long n)
{
    unsigned long long row;
    unsigned long long inner;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (inner = 0; inner < k; ++inner) {
            input[(row * k) + inner] =
                (float)(0.05 + ((double)(row + 1ull) * 0.02) +
                        ((double)(inner + 1ull) * 0.01));
        }
    }
    for (inner = 0; inner < k; ++inner) {
        for (col = 0; col < n; ++col) {
            weight[(inner * n) + col] =
                (float)(0.03 + ((double)(inner + 1ull) * 0.004) +
                        ((double)(col + 1ull) * 0.002));
        }
    }
}

static void cli_matmul_reference(const float *input,
                                 const float *weight,
                                 unsigned long long m,
                                 unsigned long long k,
                                 unsigned long long n,
                                 float *out)
{
    unsigned long long row;
    unsigned long long col;

    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input[(row * k) + inner] *
                       (double)weight[(inner * n) + col];
            }
            out[(row * n) + col] = (float)sum;
        }
    }
}

static float cli_max_abs_diff_f32(const float *a,
                                  const float *b,
                                  unsigned long long count)
{
    unsigned long long i;
    float max_diff = 0.0f;

    for (i = 0; i < count; ++i) {
        float diff = cli_abs_float(a[i] - b[i]);
        if (diff > max_diff) {
            max_diff = diff;
        }
    }
    return max_diff;
}

static void cli_print_float_values(const char *name,
                                   const float *values,
                                   unsigned long long count)
{
    unsigned long long i;

    printf("%s:", name);
    for (i = 0; i < count; ++i) {
        printf("%s%.9g", i == 0 ? " " : ",", (double)values[i]);
    }
    printf("\n");
}

static void print_rope_readiness_fields(void)
{
    printf("attention_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("execution_ready: false\n");
}

static void print_attention_readiness_fields(int primitive_executed)
{
    printf("attention_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    printf("qkv_projection_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_prefill_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("execution_ready: false\n");
}

static void print_matmul_readiness_fields(int primitive_executed)
{
    printf("matmul_primitive_executed: %s\n", primitive_executed ? "true" : "false");
    printf("qkv_projection_ready: false\n");
    printf("transformer_block_ready: false\n");
    printf("full_prefill_ready: false\n");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("execution_ready: false\n");
}

static int command_graph_execute_rope_op(const char *backend_name,
                                         unsigned long long position,
                                         unsigned long long head_dim)
{
    const float rope_base = 10000.0f;
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long bytes = 0ull;
    unsigned long long sample_count;
    float max_abs_diff = 0.0f;
    float input_output_max_abs_diff = 0.0f;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, "rope-position-op", 0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "not-needed";

    if (head_dim == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: rope-head-dim-zero\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if ((head_dim & 1ull) != 0ull) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: rope-head-dim-odd\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_ROPE_BYTE_OVERFLOW") ||
        head_dim > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }

    bytes = head_dim * (unsigned long long)sizeof(float);
    guard.shape_status = "pass";
    guard.range_status = "not-applicable";
    guard.output_bytes_planned = bytes;
    guard.reference_bytes_planned = bytes;

    input_values = (float *)malloc((size_t)bytes);
    output_values = (float *)malloc((size_t)bytes);
    reference_values = (float *)malloc((size_t)bytes);
    if (!input_values || !output_values || !reference_values) {
        free(reference_values);
        free(output_values);
        free(input_values);
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph rope", "failed to allocate host buffers");
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
    }
    cli_rope_fill_input(input_values, head_dim);
    cli_rope_reference(input_values, head_dim, position, rope_base, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: graph-op-fail\n");
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(rc);
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ROPE)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("reason: backend-op-rope-unsupported\n");
        printf("status: graph-op-fail\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    guard.backend_op_status = "supported";

    desc.name = "rope.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc == YVEX_OK) {
        desc.name = "rope.output";
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = input || out ? 1 : 0;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, bytes, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        printf("input_bytes: %llu\n", bytes);
        printf("output_bytes: %llu\n", bytes);
        printf("reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        printf("reason: injected-rope-after-alloc\n");
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return exit_for_status(YVEX_ERR_STATE);
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_rope(backend, input, position, rope_base, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ROPE_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        printf("input_bytes: %llu\n", bytes);
        printf("output_bytes: %llu\n", bytes);
        printf("reference_bytes: %llu\n", bytes);
        print_rope_readiness_fields();
        printf("reason: %s\n",
               rc == YVEX_OK ? "injected-rope-after-dispatch" : yvex_error_message(&err));
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return rc == YVEX_OK ? exit_for_status(YVEX_ERR_STATE) : exit_for_status(rc);
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, bytes, &err);
    if (rc != YVEX_OK) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        print_graph_guard_report(&guard);
        printf("op: rope\n");
        printf("backend: %s\n", backend_name);
        printf("position: %llu\n", position);
        printf("head_dim: %llu\n", head_dim);
        printf("dtype: f32\n");
        print_rope_readiness_fields();
        printf("status: graph-op-failed-cleaned\n");
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
        free(reference_values);
        free(output_values);
        free(input_values);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    input_output_max_abs_diff = cli_max_abs_diff_f32(output_values, input_values, head_dim);
    guard.guard_status = max_abs_diff <= 0.0005f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    printf("op: rope\n");
    printf("backend: %s\n", backend_name);
    printf("position: %llu\n", position);
    printf("head_dim: %llu\n", head_dim);
    printf("rope_base: %.9g\n", (double)rope_base);
    printf("dtype: f32\n");
    printf("input_bytes: %llu\n", bytes);
    printf("output_bytes: %llu\n", bytes);
    printf("scratch_bytes: 0\n");
    printf("reference_bytes: %llu\n", bytes);
    printf("input_checksum: %llu\n", cli_checksum_bytes(input_values, bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("input_output_max_abs_diff: %.9g\n", (double)input_output_max_abs_diff);
    printf("position_zero_identity: %s\n",
           position == 0ull && input_output_max_abs_diff <= 0.0000001f ? "true" : "false");
    printf("position_dependent_output: %s\n",
           position != 0ull && input_output_max_abs_diff > 0.0001f ? "true" : "false");
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("rope_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_rope_readiness_fields();
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, input);
    yvex_backend_close(backend);
    free(reference_values);
    free(output_values);
    free(input_values);
    return exit_code;
}

static void print_attention_operation_fields(const char *backend_name,
                                             unsigned long long seq_len,
                                             unsigned long long position,
                                             unsigned long long head_dim,
                                             float scale,
                                             int causal,
                                             unsigned long long query_bytes,
                                             unsigned long long key_bytes,
                                             unsigned long long value_bytes,
                                             unsigned long long input_bytes,
                                             unsigned long long score_scratch_bytes,
                                             unsigned long long probability_scratch_bytes,
                                             unsigned long long output_bytes,
                                             unsigned long long reference_bytes)
{
    printf("op: attention\n");
    printf("backend: %s\n", backend_name);
    printf("dtype: f32\n");
    printf("seq_len: %llu\n", seq_len);
    printf("position: %llu\n", position);
    printf("head_dim: %llu\n", head_dim);
    printf("scale: %.9g\n", (double)scale);
    printf("mask: %s\n", causal ? "causal" : "none");
    printf("query_bytes: %llu\n", query_bytes);
    printf("key_bytes: %llu\n", key_bytes);
    printf("value_bytes: %llu\n", value_bytes);
    printf("input_bytes: %llu\n", input_bytes);
    printf("score_scratch_bytes: %llu\n", score_scratch_bytes);
    printf("probability_scratch_bytes: %llu\n", probability_scratch_bytes);
    printf("scratch_bytes: %llu\n", score_scratch_bytes + probability_scratch_bytes);
    printf("output_bytes: %llu\n", output_bytes);
    printf("reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_attention_op(const char *backend_name,
                                              unsigned long long seq_len,
                                              unsigned long long position,
                                              unsigned long long head_dim,
                                              int causal)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *query = NULL;
    yvex_device_tensor *keys = NULL;
    yvex_device_tensor *values = NULL;
    yvex_device_tensor *score_scratch = NULL;
    yvex_device_tensor *probability_scratch = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *query_values = NULL;
    float *key_values = NULL;
    float *value_values = NULL;
    float *score_values = NULL;
    float *probability_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    float *reference_scores = NULL;
    float *reference_probabilities = NULL;
    unsigned long long kv_elements = 0ull;
    unsigned long long query_bytes = 0ull;
    unsigned long long key_bytes = 0ull;
    unsigned long long value_bytes = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long score_scratch_bytes = 0ull;
    unsigned long long probability_scratch_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long visible_keys = 0ull;
    unsigned long long masked_keys = 0ull;
    unsigned long long sample_count = 0ull;
    float scale = 0.0f;
    float max_abs_diff = 0.0f;
    float probability_max_abs_diff = 0.0f;
    float masked_probability_max = 0.0f;
    float position_zero_value_diff = 0.0f;
    const char *reason = NULL;
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, "attention-primitive", 0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "unchecked";

    if (head_dim == 0 || seq_len == 0) {
        guard.shape_status = "fail";
        reason = head_dim == 0 ? "attention-head-dim-zero" : "attention-seq-len-zero";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, 0.0f,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    scale = (float)(1.0 / cli_sqrt_double((double)head_dim));
    if (position >= seq_len) {
        guard.shape_status = "fail";
        guard.slice_range_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: position-out-of-range\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    if (cli_test_env_enabled("YVEX_TEST_ATTENTION_BYTE_OVERFLOW") ||
        seq_len > ULLONG_MAX / head_dim ||
        head_dim > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        seq_len > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    kv_elements = seq_len * head_dim;
    if (kv_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        kv_elements > ULLONG_MAX / (unsigned long long)sizeof(float)) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, 0, 0, 0, 0, 0, 0, 0, 0);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    query_bytes = head_dim * (unsigned long long)sizeof(float);
    key_bytes = kv_elements * (unsigned long long)sizeof(float);
    value_bytes = key_bytes;
    score_scratch_bytes = seq_len * (unsigned long long)sizeof(float);
    probability_scratch_bytes = score_scratch_bytes;
    output_bytes = query_bytes;
    reference_bytes = output_bytes;
    if (query_bytes > ULLONG_MAX - key_bytes ||
        query_bytes + key_bytes > ULLONG_MAX - value_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes, 0,
                                         score_scratch_bytes, probability_scratch_bytes,
                                         output_bytes, reference_bytes);
        print_attention_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = query_bytes + key_bytes + value_bytes;
    visible_keys = causal ? position + 1ull : seq_len;
    masked_keys = seq_len - visible_keys;
    guard.shape_status = "pass";
    guard.slice_range_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    query_values = (float *)malloc((size_t)query_bytes);
    key_values = (float *)malloc((size_t)key_bytes);
    value_values = (float *)malloc((size_t)value_bytes);
    score_values = (float *)malloc((size_t)score_scratch_bytes);
    probability_values = (float *)malloc((size_t)probability_scratch_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    reference_scores = (float *)malloc((size_t)score_scratch_bytes);
    reference_probabilities = (float *)malloc((size_t)probability_scratch_bytes);
    if (!query_values || !key_values || !value_values || !score_values ||
        !probability_values || !output_values || !reference_values ||
        !reference_scores || !reference_probabilities) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph attention",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_attention_fill_inputs(query_values, key_values, value_values, seq_len, head_dim);
    cli_attention_reference(query_values, key_values, value_values, seq_len, position,
                            head_dim, scale, causal, reference_scores,
                            reference_probabilities, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        printf("visible_keys: %llu\n", visible_keys);
        printf("masked_keys: %llu\n", masked_keys);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION)) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                         causal, query_bytes, key_bytes, value_bytes,
                                         input_bytes, score_scratch_bytes,
                                         probability_scratch_bytes, output_bytes,
                                         reference_bytes);
        print_attention_readiness_fields(0);
        printf("visible_keys: %llu\n", visible_keys);
        printf("masked_keys: %llu\n", masked_keys);
        printf("reason: backend-op-attention-unsupported\n");
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "attention.query";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 1;
    desc.dims[0] = head_dim;
    desc.bytes = query_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &query, &err);
    if (rc == YVEX_OK) {
        desc.name = "attention.keys";
        desc.rank = 2;
        desc.dims[0] = seq_len;
        desc.dims[1] = head_dim;
        desc.bytes = key_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &keys, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.values";
        desc.bytes = value_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &values, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.scores";
        desc.rank = 1;
        desc.dims[0] = seq_len;
        desc.dims[1] = 0;
        desc.bytes = score_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &score_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.probabilities";
        desc.bytes = probability_scratch_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &probability_scratch, &err);
    }
    if (rc == YVEX_OK) {
        desc.name = "attention.output";
        desc.dims[0] = head_dim;
        desc.bytes = output_bytes;
        rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = query || keys || values || score_scratch ||
                                  probability_scratch || out;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, query, query_values, query_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, keys, key_values, key_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, values, value_values, value_bytes, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }

    if (cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-attention-after-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_attention(backend, query, keys, values, seq_len, position,
                                   scale, causal, score_scratch,
                                   probability_scratch, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-attention-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, score_scratch, score_values, score_scratch_bytes, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_read(backend, probability_scratch, probability_values,
                                      probability_scratch_bytes, &err);
    }
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_ATTENTION_AFTER_REFERENCE")) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-attention-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, head_dim);
    probability_max_abs_diff = cli_max_abs_diff_f32(probability_values,
                                                    reference_probabilities,
                                                    seq_len);
    if (causal) {
        unsigned long long mask_index;
        for (mask_index = position + 1ull; mask_index < seq_len; ++mask_index) {
            float p = cli_abs_float(probability_values[mask_index]);
            if (p > masked_probability_max) {
                masked_probability_max = p;
            }
        }
    }
    position_zero_value_diff = cli_max_abs_diff_f32(output_values,
                                                    value_values,
                                                    head_dim);
    guard.guard_status = (max_abs_diff <= 0.001f && probability_max_abs_diff <= 0.001f)
                             ? "pass"
                             : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = head_dim < 8ull ? head_dim : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    printf("visible_keys: %llu\n", visible_keys);
    printf("masked_keys: %llu\n", masked_keys);
    printf("causal_prefix_keys: %llu\n", visible_keys);
    printf("causal_mask_future_prob_zero: %s\n",
           !causal || masked_probability_max <= 0.000001f ? "true" : "false");
    printf("masked_probability_max: %.9g\n", (double)masked_probability_max);
    printf("position_zero_single_key: %s\n",
           position == 0ull && position_zero_value_diff <= 0.000001f ? "true" : "false");
    printf("last_position_full_prefix: %s\n",
           causal && position + 1ull == seq_len ? "true" : "false");
    printf("query_checksum: %llu\n", cli_checksum_bytes(query_values, query_bytes));
    printf("key_checksum: %llu\n", cli_checksum_bytes(key_values, key_bytes));
    printf("value_checksum: %llu\n", cli_checksum_bytes(value_values, value_bytes));
    printf("score_checksum: %llu\n", cli_checksum_bytes(score_values, score_scratch_bytes));
    printf("probability_checksum: %llu\n", cli_checksum_bytes(probability_values, probability_scratch_bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("softmax_max_abs_diff: %.9g\n", (double)probability_max_abs_diff);
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("attention_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("query_sample_values", query_values, sample_count);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_attention_readiness_fields(exit_code == 0);
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_attention_operation_fields(backend_name, seq_len, position, head_dim, scale,
                                     causal, query_bytes, key_bytes, value_bytes,
                                     input_bytes, score_scratch_bytes,
                                     probability_scratch_bytes, output_bytes,
                                     reference_bytes);
    printf("visible_keys: %llu\n", visible_keys);
    printf("masked_keys: %llu\n", masked_keys);
    print_attention_readiness_fields(0);
    printf("reason: %s\n", reason ? reason : "attention-op-failed");
    printf("status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, probability_scratch);
        yvex_backend_tensor_free(backend, score_scratch);
        yvex_backend_tensor_free(backend, values);
        yvex_backend_tensor_free(backend, keys);
        yvex_backend_tensor_free(backend, query);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_probabilities);
    free(reference_scores);
    free(reference_values);
    free(output_values);
    free(probability_values);
    free(score_values);
    free(value_values);
    free(key_values);
    free(query_values);
    return exit_code;
}

static void print_matmul_operation_fields(const char *backend_name,
                                          unsigned long long m,
                                          unsigned long long k,
                                          unsigned long long n,
                                          unsigned long long input_bytes,
                                          unsigned long long weight_bytes,
                                          unsigned long long output_bytes,
                                          unsigned long long reference_bytes)
{
    printf("op: matmul\n");
    printf("backend: %s\n", backend_name);
    printf("dtype: f32\n");
    printf("m: %llu\n", m);
    printf("k: %llu\n", k);
    printf("n: %llu\n", n);
    printf("projection_shape: %s\n", m == 1ull ? "true" : "false");
    printf("non_projection_shape: %s\n", m == 1ull ? "false" : "true");
    printf("input_bytes: %llu\n", input_bytes);
    printf("weight_bytes: %llu\n", weight_bytes);
    printf("output_bytes: %llu\n", output_bytes);
    printf("scratch_bytes: 0\n");
    printf("reference_bytes: %llu\n", reference_bytes);
}

static int command_graph_execute_matmul_op(const char *backend_name,
                                           unsigned long long m,
                                           unsigned long long k,
                                           unsigned long long n)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *input = NULL;
    yvex_device_tensor *weight = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_options backend_options;
    yvex_backend_tensor_desc desc;
    yvex_cli_graph_guard_report guard;
    yvex_error err;
    float *input_values = NULL;
    float *weight_values = NULL;
    float *output_values = NULL;
    float *reference_values = NULL;
    unsigned long long input_elements = 0ull;
    unsigned long long weight_elements = 0ull;
    unsigned long long output_elements = 0ull;
    unsigned long long input_bytes = 0ull;
    unsigned long long weight_bytes = 0ull;
    unsigned long long output_bytes = 0ull;
    unsigned long long reference_bytes = 0ull;
    unsigned long long total_input_bytes = 0ull;
    unsigned long long sample_count = 0ull;
    float max_abs_diff = 0.0f;
    const char *reason = NULL;
    int rc = YVEX_OK;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&desc, 0, sizeof(desc));
    init_graph_guard_report(&guard, m == 1ull ? "matmul-projection" : "matmul-matrix",
                            0, NULL);
    guard.integrity_status = "not-applicable";
    guard.identity_status = "unregistered";
    guard.metadata_status = "unregistered";
    guard.shape_status = "unchecked";
    guard.range_status = "not-applicable";
    guard.slice_range_status = "not-needed";

    if (m == 0 || k == 0 || n == 0) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: matmul-zero-dimension\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_FORMAT);
    }
    if (cli_test_env_enabled("YVEX_TEST_MATMUL_BYTE_OVERFLOW") ||
        m > ULLONG_MAX / k ||
        k > ULLONG_MAX / n ||
        m > ULLONG_MAX / n) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_elements = m * k;
    weight_elements = k * n;
    output_elements = m * n;
    if (input_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        weight_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        output_elements > ULLONG_MAX / (unsigned long long)sizeof(float) ||
        input_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        weight_elements > (unsigned long long)(SIZE_MAX / sizeof(float)) ||
        output_elements > (unsigned long long)(SIZE_MAX / sizeof(float))) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n, 0, 0, 0, 0);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    input_bytes = input_elements * (unsigned long long)sizeof(float);
    weight_bytes = weight_elements * (unsigned long long)sizeof(float);
    output_bytes = output_elements * (unsigned long long)sizeof(float);
    reference_bytes = output_bytes;
    if (input_bytes > ULLONG_MAX - weight_bytes) {
        guard.shape_status = "fail";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: byte-count-overflow\n");
        printf("status: graph-op-fail\n");
        return exit_for_status(YVEX_ERR_BOUNDS);
    }
    total_input_bytes = input_bytes + weight_bytes;
    guard.shape_status = "pass";
    guard.output_bytes_planned = output_bytes;
    guard.reference_bytes_planned = reference_bytes;

    input_values = (float *)malloc((size_t)input_bytes);
    weight_values = (float *)malloc((size_t)weight_bytes);
    output_values = (float *)malloc((size_t)output_bytes);
    reference_values = (float *)malloc((size_t)reference_bytes);
    if (!input_values || !weight_values || !output_values || !reference_values) {
        yvex_error_set(&err, YVEX_ERR_NOMEM, "yvex graph matmul",
                       "failed to allocate host buffers");
        exit_code = print_yvex_error(&err, exit_for_status(YVEX_ERR_NOMEM));
        goto cleanup_host;
    }
    cli_matmul_fill_inputs(input_values, weight_values, m, k, n);
    cli_matmul_reference(input_values, weight_values, m, k, n, reference_values);
    guard.reference_read_attempted = 1;

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, &err);
    if (rc != YVEX_OK) {
        guard.backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        guard.backend_op_status = "unsupported";
        reason = yvex_error_message(&err);
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: %s\n", reason);
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(rc);
        goto cleanup_host;
    }
    guard.backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL) ||
        cli_test_env_enabled("YVEX_TEST_MATMUL_BACKEND_OP_UNSUPPORTED")) {
        guard.backend_op_status = "unsupported";
        print_graph_guard_report(&guard);
        print_matmul_operation_fields(backend_name, m, k, n,
                                      input_bytes, weight_bytes,
                                      output_bytes, reference_bytes);
        print_matmul_readiness_fields(0);
        printf("reason: backend-op-matmul-unsupported\n");
        printf("status: graph-op-fail\n");
        exit_code = exit_for_status(YVEX_ERR_UNSUPPORTED);
        goto cleanup_backend;
    }
    guard.backend_op_status = "supported";

    desc.name = "matmul.input";
    desc.dtype = YVEX_DTYPE_F32;
    desc.rank = 2;
    desc.dims[0] = m;
    desc.dims[1] = k;
    desc.bytes = input_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &input, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = input ? 1 : 0;
        guard.cleanup_status = guard.cleanup_attempted ? "pass" : "not-needed";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_INPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-input-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.weight";
    desc.dims[0] = k;
    desc.dims[1] = n;
    desc.bytes = weight_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &weight, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_WEIGHT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-weight-alloc";
        goto fail_cleaned;
    }

    desc.name = "matmul.output";
    desc.dims[0] = m;
    desc.dims[1] = n;
    desc.bytes = output_bytes;
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    guard.output_allocation_attempted = 1;
    guard.output_bytes_allocated = output_bytes;

    rc = yvex_backend_tensor_write(backend, input, input_values, input_bytes, &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_tensor_write(backend, weight, weight_values, weight_bytes, &err);
    }
    if (rc != YVEX_OK) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = yvex_error_message(&err);
        goto fail_cleaned;
    }
    if (cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_OUTPUT_ALLOC")) {
        guard.phase = "output";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = "injected-matmul-after-output-alloc";
        goto fail_cleaned;
    }

    guard.dispatch_attempted = 1;
    rc = yvex_backend_op_matmul(backend, input, weight, out, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_DISPATCH")) {
        guard.phase = "dispatch";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-matmul-after-dispatch" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    rc = yvex_backend_tensor_read(backend, out, output_values, output_bytes, &err);
    if (rc != YVEX_OK || cli_test_env_enabled("YVEX_TEST_FAIL_MATMUL_AFTER_REFERENCE")) {
        guard.phase = "reference";
        guard.cleanup_attempted = 1;
        guard.cleanup_status = "pass";
        reason = rc == YVEX_OK ? "injected-matmul-after-reference" : yvex_error_message(&err);
        goto fail_cleaned;
    }

    max_abs_diff = cli_max_abs_diff_f32(output_values, reference_values, output_elements);
    guard.guard_status = max_abs_diff <= 0.001f ? "pass" : "fail";
    guard.phase = "complete";
    guard.cleanup_status = "not-needed";
    sample_count = output_elements < 8ull ? output_elements : 8ull;
    exit_code = strcmp(guard.guard_status, "pass") == 0 ? 0 : exit_for_status(YVEX_ERR_STATE);

    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("weight_elements: %llu\n", weight_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    printf("input_checksum: %llu\n", cli_checksum_bytes(input_values, input_bytes));
    printf("weight_checksum: %llu\n", cli_checksum_bytes(weight_values, weight_bytes));
    printf("output_checksum: %llu\n", cli_checksum_bytes(output_values, output_bytes));
    printf("reference_checksum: %llu\n", cli_checksum_bytes(reference_values, reference_bytes));
    printf("max_abs_diff: %.9g\n", (double)max_abs_diff);
    printf("reference_attempted: true\n");
    if (strcmp(backend_name, "cuda") == 0) {
        printf("matmul_cuda_parity: %s\n", exit_code == 0 ? "pass" : "fail");
        printf("cuda_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    } else {
        printf("cpu_reference_max_abs_diff: %.9g\n", (double)max_abs_diff);
    }
    printf("output_sample_count: %llu\n", sample_count);
    cli_print_float_values("input_sample_values", input_values,
                           input_elements < 8ull ? input_elements : 8ull);
    cli_print_float_values("weight_sample_values", weight_values,
                           weight_elements < 8ull ? weight_elements : 8ull);
    cli_print_float_values("output_sample_values", output_values, sample_count);
    cli_print_float_values("reference_sample_values", reference_values, sample_count);
    print_matmul_readiness_fields(exit_code == 0);
    printf("status: %s\n", exit_code == 0 ? "graph-op-executed" : "graph-op-fail");
    goto cleanup_backend;

fail_cleaned:
    print_graph_guard_report(&guard);
    print_matmul_operation_fields(backend_name, m, k, n,
                                  input_bytes, weight_bytes,
                                  output_bytes, reference_bytes);
    printf("input_elements: %llu\n", input_elements);
    printf("weight_elements: %llu\n", weight_elements);
    printf("output_elements: %llu\n", output_elements);
    printf("input_total_bytes: %llu\n", total_input_bytes);
    print_matmul_readiness_fields(0);
    printf("reason: %s\n", reason ? reason : "matmul-op-failed");
    printf("status: graph-op-failed-cleaned\n");
    exit_code = exit_for_status(rc == YVEX_OK ? YVEX_ERR_STATE : rc);

cleanup_backend:
    if (backend) {
        yvex_backend_tensor_free(backend, out);
        yvex_backend_tensor_free(backend, weight);
        yvex_backend_tensor_free(backend, input);
        yvex_backend_close(backend);
    }

cleanup_host:
    free(reference_values);
    free(output_values);
    free(weight_values);
    free(input_values);
    return exit_code;
}

static int preflight_graph_guard(const yvex_model_ref *model_ref,
                                 const char *backend_name,
                                 int execute_fixture,
                                 int execute_segment,
                                 unsigned int token_id,
                                 yvex_cli_graph_guard_report *report,
                                 yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_artifact_integrity_report integrity_report;
    yvex_tensor_range tensor_range;
    yvex_tensor_slice_range slice_range;
    yvex_selected_embedding_shape embedding_shape;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    const yvex_tensor_info *tensor;
    const yvex_tensor_info *rmsnorm_tensor = NULL;
    unsigned long long hidden_size;
    unsigned long long output_bytes;
    unsigned long long planned_bytes;
    int rc;

    init_graph_guard_report(report,
                            execute_fixture ? "fixture-embedding" :
                            (execute_segment ? "selected-embedding-rmsnorm"
                                             : "selected-embedding-partial"),
                            !execute_fixture,
                            model_ref);
    memset(&ctx, 0, sizeof(ctx));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&tensor_range, 0, sizeof(tensor_range));
    memset(&slice_range, 0, sizeof(slice_range));
    memset(&embedding_shape, 0, sizeof(embedding_shape));
    memset(&backend_options, 0, sizeof(backend_options));

    rc = open_model_context(model_ref->path, &ctx, err);
    if (rc != YVEX_OK) {
        report->integrity_status = "fail";
        return rc;
    }
    rc = yvex_artifact_integrity_validate(ctx.artifact,
                                          ctx.gguf,
                                          ctx.table,
                                          NULL,
                                          &integrity_report,
                                          err);
    report->integrity_status = (rc == YVEX_OK && integrity_report.passed) ? "pass" : "fail";
    report->shape_status =
        integrity_report.tensor_shapes_invalid == 0 &&
        integrity_report.tensor_dtypes_invalid == 0 &&
        integrity_report.tensor_byte_counts_invalid == 0 ? "pass" : "fail";
    report->range_status = integrity_report.tensor_ranges_invalid == 0 ? "pass" : "fail";
    if (rc != YVEX_OK || !integrity_report.passed) {
        close_model_context(&ctx);
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_STATE, "graph_integrity_preflight",
                           "artifact integrity preflight failed");
        }
        return rc == YVEX_OK ? YVEX_ERR_STATE : rc;
    }

    tensor = yvex_tensor_table_find(ctx.table, "token_embd.weight");
    if (!tensor) {
        report->shape_status = "fail";
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "required tensor not found: token_embd.weight");
        return YVEX_ERR_UNSUPPORTED;
    }

    rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, tensor, &tensor_range, err);
    if (rc != YVEX_OK) {
        report->range_status = "fail";
        close_model_context(&ctx);
        return rc;
    }
    report->range_status = "pass";

    if (execute_fixture) {
        if (tensor->dtype != YVEX_DTYPE_F32) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "fixture graph embed execution requires F32 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        if (tensor->rank != 2 || tensor->dims[0] == 0 || tensor->dims[1] == 0) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                           "fixture graph token embedding must be rank 2 with non-zero dims");
            return YVEX_ERR_FORMAT;
        }
        if ((unsigned long long)token_id >= tensor->dims[1]) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                            "fixture token id %u exceeds embedding vocab size %llu",
                            token_id, tensor->dims[1]);
            return YVEX_ERR_BOUNDS;
        }
        hidden_size = tensor->dims[0];
        if (hidden_size > (unsigned long long)(~(size_t)0 / sizeof(float))) {
            report->shape_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                           "fixture graph output is too large");
            return YVEX_ERR_BOUNDS;
        }
        report->shape_status = "pass";
        report->slice_range_status = "not-needed";
        report->output_bytes_planned = hidden_size * (unsigned long long)sizeof(float);
    } else {
        if (tensor->dtype != YVEX_DTYPE_F16) {
            report->shape_status = "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                           "real partial embedding segment requires F16 token_embd.weight");
            return YVEX_ERR_UNSUPPORTED;
        }
        rc = yvex_selected_embedding_shape_validate(tensor, token_id, &embedding_shape, err);
        if (rc != YVEX_OK) {
            const char *msg = yvex_error_message(err);
            report->shape_status = msg && strstr(msg, "token-out-of-range") ? "pass" : "fail";
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            if (msg && strstr(msg, "token-out-of-range")) {
                yvex_error_setf(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                                "partial token out of range: %u >= %llu",
                                token_id, embedding_shape.vocab_size);
            }
            return rc;
        }
        rc = yvex_tensor_embedding_slice_range_validate(&tensor_range,
                                                        token_id,
                                                        &slice_range,
                                                        err);
        if (rc != YVEX_OK) {
            report->slice_range_status = "fail";
            close_model_context(&ctx);
            return rc;
        }
        report->shape_status = "pass";
        report->slice_range_status = "pass";
        report->output_bytes_planned = embedding_shape.output_bytes;
        report->reference_bytes_planned = slice_range.slice_bytes;

        if (execute_segment) {
            rmsnorm_tensor = cli_find_first_rmsnorm_tensor(ctx.table);
            if (!rmsnorm_tensor) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-tensor-missing");
                return YVEX_ERR_UNSUPPORTED;
            }
            if (rmsnorm_tensor->rank != 1 || rmsnorm_tensor->dims[0] != embedding_shape.hidden_size) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-shape-invalid");
                return YVEX_ERR_FORMAT;
            }
            if (rmsnorm_tensor->dtype != YVEX_DTYPE_F16 &&
                rmsnorm_tensor->dtype != YVEX_DTYPE_F32) {
                report->shape_status = "fail";
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                               "rmsnorm-dtype-invalid");
                return YVEX_ERR_UNSUPPORTED;
            }
            rc = yvex_tensor_range_validate(ctx.artifact, ctx.gguf, rmsnorm_tensor, &tensor_range, err);
            if (rc != YVEX_OK) {
                report->range_status = "fail";
                close_model_context(&ctx);
                return rc;
            }
            if (!cli_has_rmsnorm_epsilon(ctx.gguf)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_FORMAT, "graph_integrity_preflight",
                               "rmsnorm-epsilon-missing");
                return YVEX_ERR_FORMAT;
            }
            if (embedding_shape.output_bytes > ULLONG_MAX / 2ull ||
                embedding_shape.output_bytes > (unsigned long long)(~(size_t)0)) {
                close_model_context(&ctx);
                yvex_error_set(err, YVEX_ERR_BOUNDS, "graph_integrity_preflight",
                               "segment-memory-plan-overflow");
                return YVEX_ERR_BOUNDS;
            }
            output_bytes = embedding_shape.output_bytes;
            planned_bytes = output_bytes * 2ull;
            report->output_bytes_planned = planned_bytes;
            report->reference_bytes_planned = output_bytes;
        }
    }

    backend_options.kind = strcmp(backend_name, "cuda") == 0
                               ? YVEX_BACKEND_KIND_CUDA
                               : YVEX_BACKEND_KIND_CPU;
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        report->backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
        report->backend_op_status = "unsupported";
        close_model_context(&ctx);
        return rc;
    }
    report->backend_status = "ready";
    if (!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED) ||
        (execute_segment && !yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM)) ||
        cli_test_env_enabled("YVEX_TEST_GRAPH_BACKEND_OP_UNSUPPORTED")) {
        report->backend_op_status = "unsupported";
        yvex_backend_close(backend);
        close_model_context(&ctx);
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "graph_integrity_preflight",
                       "backend-op-unsupported");
        return YVEX_ERR_UNSUPPORTED;
    }
    report->backend_op_status = "supported";
    report->guard_status = "pass";
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return YVEX_OK;
}

static int command_integrity_report(int argc, char **argv)
{
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_cli_graph_guard_report graph_report;
    yvex_backend_options backend_options;
    yvex_backend *backend = NULL;
    yvex_model_ref ref;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = NULL;
    const char *identity_status = "unregistered";
    const char *metadata_status = "unregistered";
    const char *readiness_status = "not-checked";
    const char *support_level = "not-checked";
    const char *materialization_preflight = "not-checked";
    const char *materialization_gate = "not-checked";
    const char *materialization_backend = "not-checked";
    const char *backend_status = "not-checked";
    const char *graph_fixture_guard = "not-applicable";
    const char *graph_partial_guard = "not-checked";
    const char *graph_partial_backend = "not-checked";
    const char *graph_partial_dispatch_ready = "false";
    const char *graph_partial_reference_ready = "false";
    const char *report_status = "pass";
    const char *status = "integrity-report-pass";
    const char *model_input_kind;
    unsigned long long selected_hidden_size = 0ull;
    unsigned long long selected_vocab_size = 0ull;
    unsigned long long selected_output_count = 0ull;
    unsigned long long selected_output_bytes = 0ull;
    unsigned long long selected_slice_bytes = 0ull;
    int metadata_checked = 0;
    int selected_ready = 0;
    int hard_fail = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&registered_metadata, 0, sizeof(registered_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));
    memset(&graph_report, 0, sizeof(graph_report));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&ref, 0, sizeof(ref));

    for (i = 3; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--expect-sha256") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --expect-sha256 requires HASH\n");
                return 2;
            }
            integrity_options.expect_sha256 = argv[++i];
        } else if (strcmp(argv[i], "--require-token-embedding") == 0) {
            integrity_options.require_token_embedding = 1;
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1],
                                                        &integrity_options.token_id)) {
                fprintf(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            integrity_options.require_token_embedding = 1;
            i += 1;
        } else {
            fprintf(stderr, "yvex: unknown integrity report option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help integrity' for usage.\n");
            return 2;
        }
    }

    if (!model_arg) {
        fprintf(stderr, "yvex: integrity report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    if (backend_name &&
        strcmp(backend_name, "cpu") != 0 &&
        strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }

    rc = yvex_model_ref_resolve(&ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (ref.kind == YVEX_MODEL_REF_ALIAS && ref.sha256 && ref.sha256[0]) {
        integrity_options.registered_sha256 = ref.sha256;
    }

    rc = yvex_artifact_integrity_check_path(ref.path, &integrity_options,
                                            &integrity_report, &err);
    if (!integrity_report.passed) {
        hard_fail = 1;
    }

    model_input_kind = ref.kind == YVEX_MODEL_REF_ALIAS ? "alias" : "path";
    if (ref.kind == YVEX_MODEL_REF_ALIAS) {
        if (!ref.sha256 || !ref.sha256[0] || !yvex_sha256_hex_is_valid(ref.sha256)) {
            identity_status = "missing";
            hard_fail = 1;
        } else if (strcmp(integrity_report.digest_status, "pass") == 0 &&
                   (ref.registered_file_size == 0ull ||
                    ref.registered_file_size == integrity_report.file_size)) {
            identity_status = "pass";
        } else {
            identity_status = "fail";
            hard_fail = 1;
        }
    }

    if (integrity_report.passed) {
        yvex_error metadata_err;
        yvex_error_clear(&metadata_err);
        if (populate_registry_metadata(&current_metadata, ref.path, &metadata_err) == YVEX_OK) {
            metadata_checked = 1;
            support_level = current_metadata.entry.support_level &&
                            current_metadata.entry.support_level[0]
                                ? current_metadata.entry.support_level
                                : "not-checked";
            selected_ready = current_metadata.entry.selected_embedding_ready;
            selected_hidden_size = current_metadata.entry.selected_embedding_hidden_size;
            selected_vocab_size = current_metadata.entry.selected_embedding_vocab_size;
            selected_output_count = current_metadata.entry.selected_embedding_output_count;
            selected_slice_bytes = current_metadata.entry.selected_embedding_slice_bytes;
            selected_output_bytes = selected_output_count * (unsigned long long)sizeof(float);

            if (ref.kind == YVEX_MODEL_REF_ALIAS && strcmp(identity_status, "pass") == 0) {
                model_ref_registry_entry_view(&ref, &registered_metadata);
                if (yvex_model_registry_compare_metadata(&registered_metadata,
                                                         &current_metadata.entry,
                                                         &metadata_report,
                                                         &metadata_err) == YVEX_OK) {
                    metadata_status = metadata_report.metadata_status[0]
                                          ? metadata_report.metadata_status
                                          : "unknown";
                    readiness_status = metadata_report.readiness_status[0]
                                           ? metadata_report.readiness_status
                                           : "unknown";
                    support_level = ref.support_level && ref.support_level[0]
                                        ? ref.support_level
                                        : support_level;
                    if (strcmp(metadata_status, "pass") != 0 ||
                        strcmp(readiness_status, "pass") != 0) {
                        hard_fail = 1;
                    }
                } else {
                    metadata_status = "fail";
                    readiness_status = "fail";
                    hard_fail = 1;
                }
            } else if (ref.kind == YVEX_MODEL_REF_ALIAS) {
                metadata_status = "not-checked";
                readiness_status = "not-checked";
            } else {
                metadata_status = "unregistered";
                readiness_status = selected_ready ? "pass" : "not-checked";
            }
        } else {
            metadata_checked = 0;
            if (ref.kind == YVEX_MODEL_REF_ALIAS && strcmp(identity_status, "pass") == 0) {
                metadata_status = "fail";
                readiness_status = "fail";
                hard_fail = 1;
            }
            yvex_error_clear(&metadata_err);
        }
    }

    if (integrity_report.selected_embedding_shape[0]) {
        selected_ready = strcmp(integrity_report.selected_embedding_shape, "valid") == 0;
        selected_hidden_size = integrity_report.selected_embedding_hidden_size;
        selected_vocab_size = integrity_report.selected_embedding_vocab_size;
        selected_output_count = integrity_report.selected_embedding_output_count;
        selected_output_bytes = integrity_report.selected_embedding_output_bytes;
        selected_slice_bytes = integrity_report.selected_embedding_slice_bytes;
        readiness_status = selected_ready ? "pass" : "fail";
    } else if (integrity_options.require_token_embedding) {
        selected_ready = 0;
        readiness_status = "fail";
        hard_fail = 1;
    }

    if (backend_name) {
        materialization_backend = backend_name;
        graph_partial_backend = backend_name;
        if (!hard_fail) {
            backend_options.kind = strcmp(backend_name, "cuda") == 0
                                       ? YVEX_BACKEND_KIND_CUDA
                                       : YVEX_BACKEND_KIND_CPU;
            rc = yvex_backend_open(&backend, &backend_options, &err);
            if (rc == YVEX_OK) {
                backend_status = "ready";
                materialization_preflight = "pass";
                materialization_gate = "pass";
                yvex_backend_close(backend);
                backend = NULL;
            } else {
                backend_status = rc == YVEX_ERR_UNSUPPORTED ? "unavailable" : "fail";
                materialization_preflight = "fail";
                materialization_gate = "fail";
                hard_fail = 1;
                yvex_error_clear(&err);
            }
        } else {
            backend_status = "not-opened";
            materialization_preflight = "fail";
            materialization_gate = "fail";
        }

        if (!hard_fail && selected_ready) {
            yvex_error graph_err;
            yvex_error_clear(&graph_err);
            rc = preflight_graph_guard(&ref,
                                       backend_name,
                                       0,
                                       0,
                                       integrity_options.token_id,
                                       &graph_report,
                                       &graph_err);
            if (rc == YVEX_OK && strcmp(graph_report.guard_status, "pass") == 0) {
                graph_partial_guard = "pass";
                graph_partial_dispatch_ready = "true";
                graph_partial_reference_ready = "true";
            } else {
                graph_partial_guard = "fail";
                graph_partial_dispatch_ready = "false";
                graph_partial_reference_ready = "false";
                hard_fail = 1;
                yvex_error_clear(&graph_err);
            }
        } else if (backend_name && !selected_ready && integrity_options.require_token_embedding) {
            graph_partial_guard = "fail";
        } else if (backend_name && !selected_ready) {
            graph_partial_guard = "not-applicable";
        }
    }

    if (hard_fail) {
        report_status = "fail";
        status = "integrity-report-fail";
    }

    printf("artifact_integrity_report: summary\n");
    printf("model: %s\n", model_arg);
    printf("resolved_path: %s\n", ref.path ? ref.path : "");
    printf("model_input_kind: %s\n", model_input_kind);
    printf("format: %s\n", integrity_report.format[0] ? integrity_report.format : "unknown");
    if (integrity_report.version) {
        printf("version: %u\n", integrity_report.version);
    }
    printf("architecture: %s\n",
           integrity_report.architecture[0] ? integrity_report.architecture : "unknown");
    printf("identity_status: %s\n", identity_status);
    printf("digest_status: %s\n",
           integrity_report.digest_status[0] ? integrity_report.digest_status : "unknown");
    printf("sha256: %s\n", integrity_report.sha256[0] ? integrity_report.sha256 : "unavailable");
    printf("registered_sha256: %s\n",
           integrity_report.registered_sha256[0] ? integrity_report.registered_sha256 : "absent");
    if (integrity_report.expected_sha256[0]) {
        printf("expected_sha256: %s\n", integrity_report.expected_sha256);
        printf("actual_sha256: %s\n",
               integrity_report.sha256[0] ? integrity_report.sha256 : "unavailable");
    }
    printf("metadata_status: %s\n", metadata_status);
    printf("readiness_status: %s\n", readiness_status);
    printf("support_level: %s\n", support_level);
    printf("integrity_status: %s\n", integrity_report.passed ? "pass" : "fail");
    printf("integrity_errors: %u\n", integrity_report.error_count);
    printf("integrity_warnings: %u\n", integrity_report.warning_count);
    printf("tensor_count: %llu\n", integrity_report.tensor_count);
    printf("known_tensor_bytes: %llu\n", integrity_report.known_tensor_bytes);
    printf("tensor_ranges_checked: %llu\n", integrity_report.tensor_ranges_checked);
    printf("tensor_ranges_invalid: %llu\n", integrity_report.tensor_ranges_invalid);
    printf("tensor_shapes_checked: %llu\n", integrity_report.tensor_shapes_checked);
    printf("tensor_shapes_invalid: %llu\n", integrity_report.tensor_shapes_invalid);
    printf("tensor_dtypes_checked: %llu\n", integrity_report.tensor_dtypes_checked);
    printf("tensor_dtypes_invalid: %llu\n", integrity_report.tensor_dtypes_invalid);
    printf("tensor_byte_counts_checked: %llu\n", integrity_report.tensor_byte_counts_checked);
    printf("tensor_byte_counts_invalid: %llu\n", integrity_report.tensor_byte_counts_invalid);

    printf("primary_tensor: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_name
               ? current_metadata.entry.primary_tensor_name
               : "");
    printf("primary_tensor_role: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_role
               ? current_metadata.entry.primary_tensor_role
               : "");
    printf("primary_tensor_dtype: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_dtype
               ? current_metadata.entry.primary_tensor_dtype
               : "");
    printf("primary_tensor_rank: %u\n",
           metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
    printf("primary_tensor_dims: %s\n",
           metadata_checked && current_metadata.entry.primary_tensor_dims
               ? current_metadata.entry.primary_tensor_dims
               : "");
    printf("primary_tensor_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
    printf("selected_embedding_ready: %s\n", selected_ready ? "true" : "false");
    printf("selected_embedding_hidden_size: %llu\n", selected_hidden_size);
    printf("selected_embedding_vocab_size: %llu\n", selected_vocab_size);
    printf("selected_embedding_output_count: %llu\n", selected_output_count);
    printf("selected_embedding_output_bytes: %llu\n", selected_output_bytes);
    printf("selected_embedding_slice_bytes: %llu\n", selected_slice_bytes);
    printf("backend_status: %s\n", backend_status);
    printf("materialization_preflight: %s\n", materialization_preflight);
    printf("materialization_backend: %s\n", materialization_backend);
    printf("materialization_gate: %s\n", materialization_gate);
    printf("allocation_required_bytes: %llu\n", integrity_report.known_tensor_bytes);
    printf("graph_fixture_guard: %s\n", graph_fixture_guard);
    printf("graph_partial_guard: %s\n", graph_partial_guard);
    printf("graph_partial_backend: %s\n", graph_partial_backend);
    printf("graph_partial_token: %u\n", integrity_options.token_id);
    printf("graph_partial_dispatch_ready: %s\n", graph_partial_dispatch_ready);
    printf("graph_partial_reference_ready: %s\n", graph_partial_reference_ready);
    if (metadata_checked && ref.kind == YVEX_MODEL_REF_ALIAS) {
        for (i = 0; i < (int)metadata_report.issue_count; ++i) {
            printf("metadata_issue_%u_code: %s\n", (unsigned int)i,
                   metadata_report.issues[i].code);
            printf("metadata_issue_%u_registered: %s\n", (unsigned int)i,
                   metadata_report.issues[i].registered_value);
            printf("metadata_issue_%u_current: %s\n", (unsigned int)i,
                   metadata_report.issues[i].current_value);
        }
    }
    printf("execution_ready: false\n");
    printf("prefill_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation: unsupported\n");
    printf("report_status: %s\n", report_status);
    printf("status: %s\n", status);

    yvex_model_ref_clear(&ref);
    return hard_fail ? exit_for_status(YVEX_ERR_STATE) : 0;
}

static int command_graph(int argc, char **argv)
{
    yvex_cli_tokenizer_context ctx;
    yvex_graph *graph = NULL;
    yvex_graph_build_options options;
    yvex_engine_options engine_options;
    yvex_fixture_graph_options fixture_options;
    yvex_fixture_graph_result fixture_result;
    yvex_partial_graph_options partial_options;
    yvex_partial_graph_result partial_result;
    yvex_segment_graph_options segment_options;
    yvex_segment_graph_result segment_result;
    yvex_cli_graph_guard_report graph_guard;
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    const char *op_name = NULL;
    unsigned int token_index = 0u;
    unsigned int selected_token_id = 0u;
    unsigned long long token_vocab_size = 0ull;
    unsigned long long rope_position = 0ull;
    unsigned long long rope_head_dim = 0ull;
    unsigned long long attention_seq_len = 0ull;
    unsigned long long matmul_m = 0ull;
    unsigned long long matmul_k = 0ull;
    unsigned long long matmul_n = 0ull;
    int execute_fixture = 0;
    int execute_partial = 0;
    int execute_segment = 0;
    int execute_op = 0;
    int attention_causal = 0;
    int fixture_token_provided = 0;
    int partial_token_provided = 0;
    int token_input_provided = 0;
    int token_index_provided = 0;
    int rope_position_provided = 0;
    int rope_head_dim_provided = 0;
    int attention_seq_len_provided = 0;
    int matmul_m_provided = 0;
    int matmul_k_provided = 0;
    int matmul_n_provided = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&fixture_options, 0, sizeof(fixture_options));
    memset(&fixture_result, 0, sizeof(fixture_result));
    memset(&partial_options, 0, sizeof(partial_options));
    memset(&partial_result, 0, sizeof(partial_result));
    memset(&segment_options, 0, sizeof(segment_options));
    memset(&segment_result, 0, sizeof(segment_result));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    options.sequence_length = 1;
    options.include_prefill_path = 1;

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("graph"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--seq") == 0) {
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
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--execute-fixture") == 0) {
            execute_fixture = 1;
        } else if (strcmp(argv[i], "--fixture-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &fixture_options.token_id)) {
                fprintf(stderr, "yvex: --fixture-token requires a non-negative integer\n");
                return 2;
            }
            fixture_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--execute-partial") == 0) {
            execute_partial = 1;
        } else if (strcmp(argv[i], "--execute-segment") == 0) {
            execute_segment = 1;
        } else if (strcmp(argv[i], "--execute-op") == 0) {
            execute_op = 1;
        } else if (strcmp(argv[i], "--op") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --op requires rope, attention, or matmul\n");
                return 2;
            }
            op_name = argv[++i];
        } else if (strcmp(argv[i], "--m") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_m)) {
                fprintf(stderr, "yvex: --m requires a positive integer\n");
                return 2;
            }
            matmul_m_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--k") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_k)) {
                fprintf(stderr, "yvex: --k requires a positive integer\n");
                return 2;
            }
            matmul_k_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--n") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &matmul_n)) {
                fprintf(stderr, "yvex: --n requires a positive integer\n");
                return 2;
            }
            matmul_n_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--seq-len") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &attention_seq_len)) {
                fprintf(stderr, "yvex: --seq-len requires a positive integer\n");
                return 2;
            }
            attention_seq_len_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--position") == 0) {
            if (i + 1 >= argc || !parse_ull_allow_zero(argv[i + 1], &rope_position)) {
                fprintf(stderr, "yvex: --position requires a non-negative integer\n");
                return 2;
            }
            rope_position_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &rope_head_dim)) {
                fprintf(stderr, "yvex: --head-dim requires a positive integer\n");
                return 2;
            }
            rope_head_dim_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--causal") == 0) {
            attention_causal = 1;
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--partial-token") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &partial_options.token_id)) {
                fprintf(stderr, "yvex: --partial-token requires a non-negative integer\n");
                return 2;
            }
            segment_options.token_id = partial_options.token_id;
            partial_token_provided = 1;
            i += 1;
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires comma-separated token IDs\n");
                return 2;
            }
            tokens_text = argv[++i];
            token_input_provided = 1;
        } else if (strcmp(argv[i], "--token-index") == 0) {
            if (i + 1 >= argc || !parse_uint_allow_zero(argv[i + 1], &token_index)) {
                fprintf(stderr, "yvex: --token-index requires a non-negative integer\n");
                return 2;
            }
            token_index_provided = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            fprintf(stderr, "yvex: unknown graph option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help graph' for usage.\n");
            return 2;
        }
    }

    if (!model_arg && !execute_op) {
        fprintf(stderr, "yvex: graph requires FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N] | yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N | yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal] | yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N\n");
        return 2;
    }
    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if ((execute_fixture ? 1 : 0) + (execute_partial ? 1 : 0) +
        (execute_segment ? 1 : 0) + (execute_op ? 1 : 0) > 1) {
        fprintf(stderr, "yvex: --execute-fixture, --execute-partial, --execute-segment, and --execute-op are mutually exclusive\n");
        return 2;
    }
    if (execute_op) {
        if (model_arg) {
            fprintf(stderr, "yvex: --execute-op does not take a model artifact\n");
            return 2;
        }
        if (!op_name ||
            (strcmp(op_name, "rope") != 0 &&
             strcmp(op_name, "attention") != 0 &&
             strcmp(op_name, "matmul") != 0)) {
            fprintf(stderr, "yvex: --execute-op requires --op rope, --op attention, or --op matmul\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") == 0) {
            if (!matmul_m_provided || !matmul_k_provided || !matmul_n_provided) {
                fprintf(stderr, "yvex: --execute-op --op matmul requires --m M --k K --n N\n");
                return 2;
            }
            if (rope_position_provided || rope_head_dim_provided ||
                attention_seq_len_provided || attention_causal) {
                fprintf(stderr, "yvex: --position, --head-dim, --seq-len, and --causal require --op rope or --op attention\n");
                return 2;
            }
        } else if (!rope_position_provided) {
            fprintf(stderr, "yvex: --execute-op requires --position N\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 && !rope_head_dim_provided) {
            fprintf(stderr, "yvex: --execute-op requires --head-dim N\n");
            return 2;
        }
        if (strcmp(op_name, "attention") == 0 && !attention_seq_len_provided) {
            fprintf(stderr, "yvex: --execute-op --op attention requires --seq-len N\n");
            return 2;
        }
        if (strcmp(op_name, "rope") == 0 && (attention_seq_len_provided || attention_causal)) {
            fprintf(stderr, "yvex: --seq-len and --causal require --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "rope") != 0 && strcmp(op_name, "attention") != 0 &&
            (rope_position_provided || rope_head_dim_provided)) {
            fprintf(stderr, "yvex: --position and --head-dim require --op rope or --op attention\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") != 0 &&
            (matmul_m_provided || matmul_k_provided || matmul_n_provided)) {
            fprintf(stderr, "yvex: --m, --k, and --n require --op matmul\n");
            return 2;
        }
        if (fixture_token_provided || partial_token_provided || token_input_provided ||
            token_index_provided || segment_name) {
            fprintf(stderr, "yvex: --execute-op cannot be combined with model graph token or segment options\n");
            return 2;
        }
        if (strcmp(op_name, "matmul") == 0) {
            return command_graph_execute_matmul_op(backend_name, matmul_m, matmul_k, matmul_n);
        }
        if (strcmp(op_name, "attention") == 0) {
            return command_graph_execute_attention_op(backend_name, attention_seq_len,
                                                      rope_position, rope_head_dim,
                                                      attention_causal);
        }
        return command_graph_execute_rope_op(backend_name, rope_position, rope_head_dim);
    }
    if (op_name || rope_position_provided || rope_head_dim_provided ||
        attention_seq_len_provided || attention_causal ||
        matmul_m_provided || matmul_k_provided || matmul_n_provided) {
        fprintf(stderr, "yvex: --op, --position, --head-dim, --seq-len, --causal, --m, --k, and --n require --execute-op\n");
        return 2;
    }
    if (execute_segment) {
        if (!segment_name) {
            fprintf(stderr, "yvex: --execute-segment requires --segment embedding-rmsnorm\n");
            return 2;
        }
        if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
            fprintf(stderr, "yvex: unsupported segment: %s\n", segment_name);
            return 2;
        }
        segment_options.segment_name = segment_name;
    } else if (segment_name) {
        fprintf(stderr, "yvex: --segment requires --execute-segment\n");
        return 2;
    }
    if (token_index_provided && !token_input_provided) {
        fprintf(stderr, "yvex: --token-index requires --tokens\n");
        return 2;
    }
    if (token_input_provided && !(execute_fixture || execute_partial || execute_segment)) {
        fprintf(stderr, "yvex: --tokens is only supported with graph execution flags\n");
        return 2;
    }
    if (token_input_provided && (partial_token_provided || fixture_token_provided)) {
        fprintf(stderr, "yvex: --tokens cannot be combined with --partial-token or --fixture-token\n");
        return 2;
    }

    if (execute_fixture || execute_partial || execute_segment) {
        rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = enforce_registered_identity_cli(&model_ref,
                                             execute_fixture ? "graph-fixture" :
                                             (execute_segment ? "graph-segment" : "graph-partial"));
        if (rc != YVEX_OK) {
            init_graph_guard_report(&graph_guard,
                                    execute_fixture ? "fixture-embedding" :
                                    (execute_segment ? "selected-embedding-rmsnorm"
                                                     : "selected-embedding-partial"),
                                    !execute_fixture,
                                    &model_ref);
            graph_guard.identity_status = "fail";
            graph_guard.metadata_status = "fail";
            print_graph_guard_report(&graph_guard);
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return exit_for_status(rc);
        }

        if (token_input_provided) {
            rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
            if (rc == YVEX_OK) {
                rc = cli_token_input_vocab_from_model(model_ref.path, &token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_validate_bounds(&token_input, token_vocab_size, &err);
            }
            if (rc == YVEX_OK) {
                rc = yvex_token_input_select(&token_input,
                                             (unsigned long long)token_index,
                                             &selected_token_id,
                                             &err);
            }
            if (rc != YVEX_OK) {
                init_graph_guard_report(&graph_guard,
                                        execute_fixture ? "fixture-embedding" :
                                        (execute_segment ? "selected-embedding-rmsnorm"
                                                         : "selected-embedding-partial"),
                                        !execute_fixture,
                                        &model_ref);
                graph_guard.slice_range_status =
                    token_input.token_bounds_checked && token_input.token_bounds_valid
                        ? "pass"
                        : "fail";
                print_token_input_summary(&token_input,
                                          "fail",
                                          token_input.token_bounds_checked
                                              ? (token_input.token_bounds_valid ? "pass" : "fail")
                                              : "not-checked",
                                          (unsigned long long)token_index,
                                          selected_token_id,
                                          0);
                printf("vocab_size: %llu\n", token_vocab_size);
                print_graph_guard_report(&graph_guard);
                printf("status: graph-integrity-fail\n");
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }

            if (execute_fixture) {
                fixture_options.token_id = selected_token_id;
            } else if (execute_segment) {
                segment_options.token_id = selected_token_id;
                partial_options.token_id = selected_token_id;
            } else {
                partial_options.token_id = selected_token_id;
                segment_options.token_id = selected_token_id;
            }
        }

        rc = preflight_graph_guard(&model_ref,
                                   backend_name,
                                   execute_fixture,
                                   execute_segment,
                                   execute_fixture ? fixture_options.token_id :
                                   (execute_segment ? segment_options.token_id : partial_options.token_id),
                                   &graph_guard,
                                   &err);
        if (rc != YVEX_OK) {
            print_graph_guard_report(&graph_guard);
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (token_input_provided) {
            print_token_input_summary(&token_input,
                                      "pass",
                                      "pass",
                                      (unsigned long long)token_index,
                                      selected_token_id,
                                      1);
            printf("vocab_size: %llu\n", token_vocab_size);
        }

        engine_options.model_path = model_ref.path;
        engine_options.load_tokenizer = 0;
        engine_options.build_descriptor = 1;
        engine_options.build_default_graph = 1;
        engine_options.attach_weights = 1;
        engine_options.backend_name = backend_name;
        engine_options.require_all_weights = 1;

        rc = yvex_engine_open(&engine, &engine_options, &err);
        if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
            graph_guard.guard_status = "fail";
            graph_guard.phase = "preflight";
            graph_guard.backend_status = "unavailable";
            graph_guard.backend_op_status = "unsupported";
            print_graph_guard_report(&graph_guard);
            printf("%s_backend: cuda\n", execute_fixture ? "fixture" :
                   (execute_segment ? "segment" : "partial"));
            printf("%s_backend_status: unsupported\n", execute_fixture ? "fixture" :
                   (execute_segment ? "segment" : "partial"));
            printf("reason: %s\n", yvex_error_message(&err));
            printf("status: graph-integrity-fail\n");
            yvex_model_ref_clear(&model_ref);
            return 5;
        }
        if (rc == YVEX_OK && execute_fixture) {
            rc = yvex_engine_execute_fixture_graph(engine, &fixture_options, &fixture_result, &err);
        } else if (rc == YVEX_OK && execute_segment) {
            rc = yvex_engine_execute_segment_graph(engine, &segment_options, &segment_result, &err);
        } else if (rc == YVEX_OK) {
            rc = yvex_engine_execute_partial_graph(engine, &partial_options, &partial_result, &err);
        }
        if (rc != YVEX_OK) {
            if (execute_fixture) {
                graph_guard.phase = fixture_result.graph_execution_phase
                                        ? fixture_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = fixture_result.shape_status
                                               ? fixture_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = fixture_result.range_status
                                               ? fixture_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = fixture_result.slice_range_status
                                                     ? fixture_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = fixture_result.backend_status
                                                  ? fixture_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = fixture_result.backend_op_status
                                                     ? fixture_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = fixture_result.dispatch_attempted;
                graph_guard.reference_read_attempted = fixture_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = fixture_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = fixture_result.cleanup_attempted;
                graph_guard.cleanup_status = fixture_result.cleanup_status
                                                ? fixture_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = fixture_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = fixture_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = fixture_result.reference_bytes_planned;
            } else if (execute_segment) {
                graph_guard.phase = segment_result.graph_execution_phase
                                        ? segment_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = segment_result.shape_status
                                               ? segment_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = segment_result.range_status
                                               ? segment_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = segment_result.slice_range_status
                                                     ? segment_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = segment_result.backend_status
                                                  ? segment_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = segment_result.backend_op_status
                                                     ? segment_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = segment_result.dispatch_attempted;
                graph_guard.reference_read_attempted = segment_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = segment_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = segment_result.cleanup_attempted;
                graph_guard.cleanup_status = segment_result.cleanup_status
                                                ? segment_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = segment_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = segment_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = segment_result.reference_bytes_planned;
            } else {
                graph_guard.phase = partial_result.graph_execution_phase
                                        ? partial_result.graph_execution_phase
                                        : "dispatch";
                graph_guard.shape_status = partial_result.shape_status
                                               ? partial_result.shape_status
                                               : graph_guard.shape_status;
                graph_guard.range_status = partial_result.range_status
                                               ? partial_result.range_status
                                               : graph_guard.range_status;
                graph_guard.slice_range_status = partial_result.slice_range_status
                                                     ? partial_result.slice_range_status
                                                     : graph_guard.slice_range_status;
                graph_guard.backend_status = partial_result.backend_status
                                                  ? partial_result.backend_status
                                                  : graph_guard.backend_status;
                graph_guard.backend_op_status = partial_result.backend_op_status
                                                     ? partial_result.backend_op_status
                                                     : graph_guard.backend_op_status;
                graph_guard.dispatch_attempted = partial_result.dispatch_attempted;
                graph_guard.reference_read_attempted = partial_result.reference_read_attempted;
                graph_guard.output_allocation_attempted = partial_result.output_allocation_attempted;
                graph_guard.cleanup_attempted = partial_result.cleanup_attempted;
                graph_guard.cleanup_status = partial_result.cleanup_status
                                                ? partial_result.cleanup_status
                                                : graph_guard.cleanup_status;
                graph_guard.output_bytes_planned = partial_result.output_bytes_planned;
                graph_guard.output_bytes_allocated = partial_result.output_bytes_allocated;
                graph_guard.reference_bytes_planned = partial_result.reference_bytes_planned;
            }
            graph_guard.guard_status = "fail";
            print_graph_guard_report(&graph_guard);
            printf("status: %s\n",
                   graph_guard.cleanup_attempted ? "graph-failed-cleaned" : "graph-integrity-fail");
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
            return print_yvex_error(&err, exit_for_status(rc));
        }

        if (execute_fixture) {
            graph_guard.guard_status = fixture_result.graph_integrity_guard
                                           ? fixture_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = fixture_result.graph_execution_phase
                                    ? fixture_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = fixture_result.shape_status ? fixture_result.shape_status : "pass";
            graph_guard.range_status = fixture_result.range_status ? fixture_result.range_status : "pass";
            graph_guard.slice_range_status = fixture_result.slice_range_status
                                                 ? fixture_result.slice_range_status
                                                 : "not-needed";
            graph_guard.backend_status = fixture_result.backend_status ? fixture_result.backend_status : "ready";
            graph_guard.backend_op_status = fixture_result.backend_op_status
                                                ? fixture_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = fixture_result.dispatch_attempted;
            graph_guard.reference_read_attempted = fixture_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = fixture_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = fixture_result.cleanup_attempted;
            graph_guard.cleanup_status = fixture_result.cleanup_status
                                            ? fixture_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = fixture_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = fixture_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = fixture_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("fixture_graph_executed: true\n");
            printf("fixture_backend: %s\n", fixture_result.backend_name);
            printf("fixture_op: %s\n", fixture_result.op_name);
            printf("fixture_weight: %s\n", fixture_result.weight_name);
            printf("fixture_token_id: %u\n", fixture_result.token_id);
            printf("fixture_node_count: %llu\n", fixture_result.node_count);
            printf("fixture_output_count: %llu\n", fixture_result.output_count);
            printf("fixture_output_bytes: %llu\n", fixture_result.output_bytes);
            printf("fixture_output_checksum: %llu\n", fixture_result.output_checksum);
            printf("fixture_output_values:");
            for (i = 0; (unsigned long long)i < fixture_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)fixture_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("status: fixture-graph-executed\n");
        } else if (execute_segment) {
            graph_guard.guard_status = segment_result.graph_integrity_guard
                                           ? segment_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = segment_result.graph_execution_phase
                                    ? segment_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = segment_result.shape_status ? segment_result.shape_status : "pass";
            graph_guard.range_status = segment_result.range_status ? segment_result.range_status : "pass";
            graph_guard.slice_range_status = segment_result.slice_range_status
                                                 ? segment_result.slice_range_status
                                                 : "pass";
            graph_guard.backend_status = segment_result.backend_status ? segment_result.backend_status : "ready";
            graph_guard.backend_op_status = segment_result.backend_op_status
                                                ? segment_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = segment_result.dispatch_attempted;
            graph_guard.reference_read_attempted = segment_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = segment_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = segment_result.cleanup_attempted;
            graph_guard.cleanup_status = segment_result.cleanup_status
                                            ? segment_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = segment_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = segment_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = segment_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("segment_graph_executed: true\n");
            printf("segment_backend: %s\n", segment_result.backend_name);
            printf("segment_name: %s\n", segment_result.segment_name);
            printf("segment_ops: %llu\n", segment_result.segment_ops);
            printf("segment_op_0: embed\n");
            printf("segment_op_1: rms_norm\n");
            printf("partial_token: %u\n", segment_result.token_id);
            printf("token_tensor: %s\n", segment_result.token_tensor_name);
            printf("token_tensor_dtype: %s\n", segment_result.token_tensor_dtype);
            printf("rmsnorm_tensor: %s\n", segment_result.rmsnorm_tensor_name);
            printf("rmsnorm_tensor_dtype: %s\n", segment_result.rmsnorm_tensor_dtype);
            printf("hidden_size: %llu\n", segment_result.hidden_size);
            printf("vocab_size: %llu\n", segment_result.vocab_size);
            printf("rmsnorm_epsilon_key: %s\n", segment_result.rmsnorm_epsilon_key);
            printf("rmsnorm_epsilon: %.9g\n", segment_result.rmsnorm_epsilon);
            printf("segment_memory_plan: explicit\n");
            printf("segment_intermediate_count: %llu\n", segment_result.segment_intermediate_count);
            printf("segment_intermediate_bytes: %llu\n", segment_result.segment_intermediate_bytes);
            printf("segment_output_count: %llu\n", segment_result.segment_output_count);
            printf("segment_output_bytes: %llu\n", segment_result.segment_output_bytes);
            printf("segment_scratch_bytes: %llu\n", segment_result.segment_scratch_bytes);
            printf("segment_reference_bytes: %llu\n", segment_result.segment_reference_bytes);
            printf("segment_output_checksum: %llu\n", segment_result.output_checksum);
            printf("segment_reference_checksum: %llu\n", segment_result.reference_checksum);
            printf("segment_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            if (strcmp(segment_result.backend_name, "cuda") == 0) {
                printf("segment_cuda_parity: pass\n");
                printf("cuda_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            } else {
                printf("cpu_reference_max_abs_diff: %.9g\n", segment_result.max_abs_diff);
            }
            printf("segment_output_sample_count: %llu\n", segment_result.output_value_count);
            printf("segment_output_sample_values:");
            for (i = 0; (unsigned long long)i < segment_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)segment_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("prefill_ready: false\n");
            printf("logits_ready: false\n");
            printf("generation: unsupported\n");
            printf("status: real-segment-graph-executed\n");
        } else {
            graph_guard.guard_status = partial_result.graph_integrity_guard
                                           ? partial_result.graph_integrity_guard
                                           : "pass";
            graph_guard.phase = partial_result.graph_execution_phase
                                    ? partial_result.graph_execution_phase
                                    : "complete";
            graph_guard.shape_status = partial_result.shape_status ? partial_result.shape_status : "pass";
            graph_guard.range_status = partial_result.range_status ? partial_result.range_status : "pass";
            graph_guard.slice_range_status = partial_result.slice_range_status
                                                 ? partial_result.slice_range_status
                                                 : "pass";
            graph_guard.backend_status = partial_result.backend_status ? partial_result.backend_status : "ready";
            graph_guard.backend_op_status = partial_result.backend_op_status
                                                ? partial_result.backend_op_status
                                                : "supported";
            graph_guard.dispatch_attempted = partial_result.dispatch_attempted;
            graph_guard.reference_read_attempted = partial_result.reference_read_attempted;
            graph_guard.output_allocation_attempted = partial_result.output_allocation_attempted;
            graph_guard.cleanup_attempted = partial_result.cleanup_attempted;
            graph_guard.cleanup_status = partial_result.cleanup_status
                                            ? partial_result.cleanup_status
                                            : "not-needed";
            graph_guard.output_bytes_planned = partial_result.output_bytes_planned;
            graph_guard.output_bytes_allocated = partial_result.output_bytes_allocated;
            graph_guard.reference_bytes_planned = partial_result.reference_bytes_planned;
            print_graph_guard_report(&graph_guard);
            printf("real_partial_graph_executed: true\n");
            printf("partial_graph_kind: %s\n", partial_result.segment_name);
            printf("partial_backend: %s\n", partial_result.backend_name);
            printf("partial_weight: %s\n", partial_result.weight_name);
            printf("partial_weight_dtype: %s\n", partial_result.weight_dtype);
            printf("partial_token: %u\n", partial_result.token_id);
            printf("partial_node_count: %llu\n", partial_result.node_count);
            printf("partial_output_dtype: %s\n", partial_result.output_dtype);
            printf("partial_output_count: %llu\n", partial_result.output_count);
            printf("partial_output_bytes: %llu\n", partial_result.output_bytes);
            printf("partial_output_checksum: %llu\n", partial_result.output_checksum);
            printf("partial_reference_checksum: %llu\n", partial_result.reference_checksum);
            printf("partial_max_abs_diff: %.9g\n", partial_result.max_abs_diff);
            printf("partial_output_sample_count: %llu\n", partial_result.output_value_count);
            printf("partial_output_sample_values:");
            for (i = 0; (unsigned long long)i < partial_result.output_value_count; ++i) {
                printf("%s%.9g", i == 0 ? " " : ",", (double)partial_result.output_values[i]);
            }
            printf("\n");
            printf("execution_ready: false\n");
            printf("graph_execution_ready: false\n");
            printf("prefill_ready: false\n");
            printf("logits_ready: false\n");
            printf("generation: unsupported\n");
            printf("status: real-partial-graph-executed\n");
        }

        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return 0;
    }

    rc = open_model_context(model_arg, &ctx, &err);
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

static void print_prefill_state_summary(const yvex_prefill_state_summary *summary,
                                        const char *model_arg,
                                        const char *backend_name,
                                        const char *token_input_status,
                                        const char *status)
{
    unsigned long long i;

    printf("prefill: state\n");
    printf("prefill_state_created: %s\n",
           summary && summary->prefill_state_created ? "true" : "false");
    printf("prefill_state_kind: %s\n",
           summary && summary->prefill_state_kind ? summary->prefill_state_kind : "segment-summary");
    printf("sequence_execution_mode: %s\n",
           summary && summary->sequence_execution_mode
               ? summary->sequence_execution_mode
               : "independent-token-segments");
    printf("prefill_phase: %s\n",
           summary && summary->prefill_phase ? summary->prefill_phase : "preflight");
    printf("model: %s\n", model_arg ? model_arg : "");
    printf("backend: %s\n",
           summary && summary->backend_name && strcmp(summary->backend_name, "none") != 0
               ? summary->backend_name
               : (backend_name ? backend_name : "cpu"));
    printf("segment: %s\n",
           summary && summary->segment_name ? summary->segment_name : "embedding-rmsnorm");
    printf("token_input_status: %s\n", token_input_status ? token_input_status : "fail");
    printf("token_count: %llu\n", summary ? summary->token_count : 0ull);
    printf("tokens_processed: %llu\n", summary ? summary->tokens_processed : 0ull);
    printf("position_start: %llu\n", summary ? summary->position_start : 0ull);
    printf("position_end: %llu\n", summary ? summary->position_end : 0ull);
    printf("failed_token_index: %llu\n", summary ? summary->failed_token_index : 0ull);
    printf("segment_graph_executions: %llu\n", summary ? summary->segment_graph_executions : 0ull);
    printf("segment_output_count: %llu\n", summary ? summary->segment_output_count : 0ull);
    printf("segment_output_bytes: %llu\n", summary ? summary->segment_output_bytes : 0ull);
    printf("prefill_aggregate_checksum: %llu\n", summary ? summary->aggregate_checksum : 0ull);
    printf("prefill_final_token_checksum: %llu\n", summary ? summary->final_token_checksum : 0ull);
    printf("prefill_total_output_bytes: %llu\n", summary ? summary->total_output_bytes : 0ull);
    printf("prefill_scratch_bytes: %llu\n", summary ? summary->scratch_bytes : 0ull);
    printf("prefill_max_abs_diff: %.9g\n", summary ? summary->max_abs_diff : 0.0);
    printf("cleanup_attempted: %s\n",
           summary && summary->cleanup_attempted ? "true" : "false");
    printf("cleanup_status: %s\n",
           summary && summary->cleanup_status ? summary->cleanup_status : "not-needed");
    if (summary && summary->cuda_parity) {
        printf("prefill_cuda_parity: pass\n");
    }
    printf("kv_ready: %s\n", summary && summary->kv_ready ? "true" : "false");
    printf("session_kv_owned: %s\n",
           summary && summary->session_kv_owned ? "true" : "false");
    printf("kv_bound_to_prefill: %s\n",
           summary && summary->kv_bound_to_prefill ? "true" : "false");
    printf("kv_binding_kind: %s\n",
           summary && summary->kv_binding_kind ? summary->kv_binding_kind : "none");
    printf("kv_status: %s\n",
           summary && summary->kv_status ? summary->kv_status : "not-requested");
    printf("kv_owner: %s\n",
           summary && summary->kv_owner ? summary->kv_owner : "none");
    printf("kv_dtype: %s\n",
           summary && summary->kv_dtype ? summary->kv_dtype : "none");
    printf("kv_layers: %llu\n", summary ? summary->kv_layers : 0ull);
    printf("kv_heads: %llu\n", summary ? summary->kv_heads : 0ull);
    printf("kv_head_dim: %llu\n", summary ? summary->kv_head_dim : 0ull);
    printf("kv_capacity: %llu\n", summary ? summary->kv_capacity : 0ull);
    printf("kv_values_per_position: %llu\n", summary ? summary->kv_values_per_position : 0ull);
    printf("kv_bytes_per_position: %llu\n", summary ? summary->kv_bytes_per_position : 0ull);
    printf("kv_planned_bytes: %llu\n", summary ? summary->kv_planned_bytes : 0ull);
    printf("kv_allocated_bytes: %llu\n", summary ? summary->kv_allocated_bytes : 0ull);
    printf("kv_positions_written: %llu\n", summary ? summary->kv_positions_written : 0ull);
    printf("kv_append_count: %llu\n", summary ? summary->kv_append_count : 0ull);
    printf("kv_read_count: %llu\n", summary ? summary->kv_read_count : 0ull);
    printf("kv_read_position: %llu\n", summary ? summary->kv_read_position : 0ull);
    printf("kv_read_value_count: %llu\n", summary ? summary->kv_read_value_count : 0ull);
    printf("kv_read_checksum: %llu\n", summary ? summary->kv_read_checksum : 0ull);
    printf("kv_read_sample_values:");
    if (summary && summary->kv_read_sample_count > 0ull) {
        for (i = 0; i < summary->kv_read_sample_count; ++i) {
            printf("%s%.9g", i == 0 ? " " : ",", (double)summary->kv_read_sample_values[i]);
        }
    }
    printf("\n");
    printf("kv_overflow: %s\n",
           summary && summary->kv_overflow_status ? summary->kv_overflow_status : "not-checked");
    printf("kv_cleanup_status: %s\n",
           summary && summary->kv_cleanup_status ? summary->kv_cleanup_status : "not-needed");
    printf("full_transformer_prefill_ready: false\n");
    printf("decode_ready: false\n");
    printf("logits_ready: false\n");
    printf("generation_ready: false\n");
    printf("generation: unsupported\n");
    printf("status: %s\n", status ? status : "prefill-state-fail");
}

static void init_prefill_summary_cli_defaults(yvex_prefill_state_summary *summary,
                                              const char *segment_name,
                                              int attach_kv,
                                              const yvex_kv_shape *shape)
{
    if (!summary) {
        return;
    }
    memset(summary, 0, sizeof(*summary));
    summary->prefill_state_kind = "segment-summary";
    summary->sequence_execution_mode = "independent-token-segments";
    summary->prefill_phase = "preflight";
    summary->segment_name = segment_name ? segment_name : "embedding-rmsnorm";
    summary->cleanup_status = "not-needed";
    summary->generation_status = "unsupported";
    summary->kv_binding_kind = attach_kv ? "minimal-diagnostic" : "none";
    summary->kv_status = attach_kv ? "planned" : "not-requested";
    summary->kv_owner = "none";
    summary->kv_dtype = "none";
    summary->kv_overflow_status = "not-checked";
    summary->kv_cleanup_status = "not-needed";
    if (shape) {
        summary->kv_layers = shape->layer_count;
        summary->kv_heads = shape->kv_head_count;
        summary->kv_head_dim = shape->head_dim;
        summary->kv_capacity = shape->capacity;
    }
}

static int command_prefill(int argc, char **argv)
{
    yvex_model_ref model_ref;
    yvex_token_input token_input;
    yvex_engine_options engine_options;
    yvex_prefill_state_options prefill_options;
    yvex_prefill_state_summary prefill_summary;
    yvex_cli_graph_guard_report graph_guard;
    yvex_engine *engine = NULL;
    yvex_error err;
    const char *model_arg = NULL;
    const char *backend_name = "cpu";
    const char *segment_name = NULL;
    const char *tokens_text = NULL;
    yvex_kv_shape kv_shape;
    unsigned long long vocab_size = 0ull;
    int attach_kv = 0;
    int kv_shape_seen = 0;
    int i;
    int rc;

    yvex_error_clear(&err);
    memset(&model_ref, 0, sizeof(model_ref));
    memset(&token_input, 0, sizeof(token_input));
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&prefill_options, 0, sizeof(prefill_options));
    memset(&prefill_summary, 0, sizeof(prefill_summary));
    memset(&graph_guard, 0, sizeof(graph_guard));
    memset(&kv_shape, 0, sizeof(kv_shape));

    if (argc < 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        print_command_help(stdout, find_command("prefill"));
        return argc >= 3 ? 0 : 2;
    }

    for (i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--model") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --model requires FILE_OR_ALIAS\n");
                return 2;
            }
            model_arg = argv[++i];
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --backend requires cpu|cuda\n");
                return 2;
            }
            backend_name = argv[++i];
        } else if (strcmp(argv[i], "--segment") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --segment requires embedding-rmsnorm\n");
                return 2;
            }
            segment_name = argv[++i];
        } else if (strcmp(argv[i], "--tokens") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: --tokens requires IDS\n");
                return 2;
            }
            tokens_text = argv[++i];
        } else if (strcmp(argv[i], "--attach-kv") == 0) {
            attach_kv = 1;
        } else if (strcmp(argv[i], "--kv-layers") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.layer_count)) {
                fprintf(stderr, "yvex: --kv-layers requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-heads") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.kv_head_count)) {
                fprintf(stderr, "yvex: --kv-heads requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-head-dim") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.head_dim)) {
                fprintf(stderr, "yvex: --kv-head-dim requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (strcmp(argv[i], "--kv-capacity") == 0) {
            if (i + 1 >= argc || !parse_positive_ull(argv[i + 1], &kv_shape.capacity)) {
                fprintf(stderr, "yvex: --kv-capacity requires a positive integer\n");
                return 2;
            }
            kv_shape_seen = 1;
            i += 1;
        } else if (!model_arg) {
            model_arg = argv[i];
        } else {
            fprintf(stderr, "yvex: unknown prefill option: %s\n", argv[i]);
            fprintf(stderr, "Try 'yvex help prefill' for usage.\n");
            return 2;
        }
    }

    if (!model_arg || !tokens_text || !segment_name) {
        fprintf(stderr, "usage: yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]\n");
        return 2;
    }
    if (strcmp(backend_name, "cpu") != 0 && strcmp(backend_name, "cuda") != 0) {
        fprintf(stderr, "yvex: unknown backend kind: %s\n", backend_name);
        return 2;
    }
    if (strcmp(segment_name, "embedding-rmsnorm") != 0) {
        fprintf(stderr, "yvex: unsupported prefill segment: %s\n", segment_name);
        return 2;
    }
    if (kv_shape_seen && !attach_kv) {
        fprintf(stderr, "yvex: --kv-* options require --attach-kv\n");
        return 2;
    }
    if (attach_kv && (kv_shape.layer_count == 0ull ||
                      kv_shape.kv_head_count == 0ull ||
                      kv_shape.head_dim == 0ull ||
                      kv_shape.capacity == 0ull)) {
        fprintf(stderr, "yvex: --attach-kv requires --kv-layers, --kv-heads, --kv-head-dim, and --kv-capacity\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_arg, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "prefill");
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    rc = yvex_token_input_parse_explicit(tokens_text, &token_input, &err);
    if (rc == YVEX_OK) {
        rc = cli_token_input_vocab_from_model(model_ref.path, &vocab_size, &err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_token_input_validate_bounds(&token_input, vocab_size, &err);
    }
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "fail", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = preflight_graph_guard(&model_ref,
                               backend_name,
                               0,
                               1,
                               token_input.tokens[0],
                               &graph_guard,
                               &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        print_graph_guard_report(&graph_guard);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc != YVEX_OK) {
        init_prefill_summary_cli_defaults(&prefill_summary, segment_name, attach_kv, &kv_shape);
        prefill_summary.token_count = token_input.token_count;
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    prefill_options.token_input = &token_input;
    prefill_options.segment_name = segment_name;
    prefill_options.position_start = 0ull;
    prefill_options.attach_kv = attach_kv;
    prefill_options.kv_shape = kv_shape;
    rc = yvex_engine_create_prefill_state(engine, &prefill_options, &prefill_summary, &err);
    if (rc != YVEX_OK) {
        print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-fail");
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    print_prefill_state_summary(&prefill_summary, model_arg, backend_name, "pass", "prefill-state-created");
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
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
    yvex_model_ref model_ref;
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
    memset(&model_ref, 0, sizeof(model_ref));

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
        fprintf(stderr, "yvex: --model is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex run in diagnostic runtime\n");
        return 2;
    }
    if (!prompt_text) {
        fprintf(stderr, "yvex: --prompt is required for yvex run in diagnostic runtime\n");
        return 2;
    }

    rc = yvex_model_ref_resolve(&model_ref, model_path, NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
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
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
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
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_TOTAL, total_token, &err);
    (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_END, "run", "accepted-only",
                          "decode runtime is not implemented in diagnostic runtime", &err);
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
    yvex_model_ref_clear(&model_ref);
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
    yvex_model_ref model_ref;
    yvex_engine_options engine_options;
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
    memset(&engine_options, 0, sizeof(engine_options));
    memset(&session_options, 0, sizeof(session_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&tokens, 0, sizeof(tokens));
    memset(&model_ref, 0, sizeof(model_ref));
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

    rc = yvex_model_ref_resolve(&model_ref, argv[2], NULL, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = enforce_registered_identity_cli(&model_ref, "session");
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return exit_for_status(rc);
    }

    engine_options.model_path = model_ref.path;
    engine_options.load_tokenizer = text ? 1 : 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;

    rc = yvex_engine_open(&engine, &engine_options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED && strcmp(backend_name, "cuda") == 0) {
        printf("backend: cuda\n");
        printf("backend_status: unsupported\n");
        printf("reason: %s\n", yvex_error_message(&err));
        printf("status: session-backend-unsupported\n");
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
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
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    if (rc != YVEX_OK) {
        yvex_backend_close(backend);
        yvex_engine_close(engine);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }

    if (text) {
        rc = yvex_tokenize_text(yvex_engine_tokenizer(engine), text, &tokens, &err);
        if (rc != YVEX_OK) {
            yvex_session_close(session);
            yvex_backend_close(backend);
            yvex_engine_close(engine);
            yvex_model_ref_clear(&model_ref);
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
                yvex_model_ref_clear(&model_ref);
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
        yvex_model_ref_clear(&model_ref);
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
    printf("weights_attached: %s\n", summary.weights_attached ? "true" : "false");
    printf("weights_backend: %s\n", summary.weights_backend);
    printf("weight_tensor_count: %llu\n", summary.weight_tensor_count);
    printf("weight_total_bytes: %llu\n", summary.weight_total_bytes);
    printf("execution_ready: false\n");
    printf("graph_execution_ready: false\n");
    printf("reason: %s\n", yvex_session_diagnostic_reason(session));
    if (tokenized) {
        printf("tokens: %llu\n", tokens.len);
    }
    printf("status: %s\n", accept_tokens ? "session-token-accepted" : "session-created");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    yvex_model_ref_clear(&model_ref);
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
    fprintf(fp, "  op_rope: %s\n",
            yvex_backend_supports(runtime->backend, YVEX_BACKEND_CAP_OP_ROPE) ? "yes" : "no");
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

static int resolve_chat_model_ref(yvex_model_ref *out,
                                  const char *explicit_model,
                                  yvex_error *err)
{
    yvex_model_registry *registry = NULL;
    const yvex_model_registry_entry *selected;
    char alias[256];
    int n;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat", "model reference output is required");
        return YVEX_ERR_INVALID_ARG;
    }

    if (explicit_model && explicit_model[0]) {
        return yvex_model_ref_resolve(out, explicit_model, NULL, err);
    }

    rc = models_registry_open(&registry, NULL, 0, err);
    if (rc != YVEX_OK) {
        if (rc == YVEX_ERR_IO) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                           "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
            return YVEX_ERR_INVALID_ARG;
        }
        return rc;
    }

    selected = yvex_model_registry_selected(registry);
    if (!selected || !selected->alias || !selected->alias[0]) {
        yvex_model_registry_close(registry);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "chat",
                       "no model selected; pass --model FILE_OR_ALIAS or run './yvex models use ALIAS'");
        return YVEX_ERR_INVALID_ARG;
    }

    n = snprintf(alias, sizeof(alias), "%s", selected->alias);
    yvex_model_registry_close(registry);
    if (n < 0 || (size_t)n >= sizeof(alias)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "chat", "selected model alias is too long");
        return YVEX_ERR_BOUNDS;
    }

    return yvex_model_ref_resolve(out, alias, NULL, err);
}

static int command_chat(int argc, char **argv)
{
    yvex_chat_runtime runtime;
    yvex_engine_summary engine_summary;
    yvex_session_summary session_summary;
    yvex_model_ref model_ref;
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
    memset(&model_ref, 0, sizeof(model_ref));

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

    if (!backend_name) {
        fprintf(stderr, "yvex: --backend is required for yvex chat in diagnostic runtime\n");
        return 2;
    }

    rc = resolve_chat_model_ref(&model_ref, model_path, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_path = model_ref.path;

    rc = yvex_metrics_create(&metrics, &err);
    if (rc != YVEX_OK) {
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    rc = yvex_run_artifacts_prepare(&artifacts, save_run, run_dir, metrics_out, trace_out,
                                    profile_out, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    memset(&trace_options, 0, sizeof(trace_options));
    trace_options.path = artifacts.has_trace ? artifacts.trace_path : NULL;
    trace_options.run_id = artifacts.run_id;
    trace_options.enabled = artifacts.has_trace;
    rc = yvex_trace_open(&trace, &trace_options, &err);
    if (rc != YVEX_OK) {
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
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
        yvex_model_ref_clear(&model_ref);
        return 5;
    }
    if (rc != YVEX_OK) {
        yvex_trace_close(trace);
        yvex_metrics_close(metrics);
        yvex_model_ref_clear(&model_ref);
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
    printf("generation: unsupported in diagnostic runtime\n");
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
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
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
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            rc = yvex_chat_runtime_accept_user_text(&runtime, NULL, line, &result, &err);
            (void)yvex_metrics_phase_end(metrics, YVEX_METRIC_PHASE_CHAT_TURN, phase_token, &err);
            if (rc != YVEX_OK) {
                yvex_chat_runtime_close(&runtime);
                yvex_trace_close(trace);
                yvex_metrics_close(metrics);
                yvex_model_ref_clear(&model_ref);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            (void)yvex_metrics_add_chat_turn(metrics, &err);
            (void)yvex_trace_emit(trace, YVEX_TRACE_EVENT_CHAT_TURN, "chat_turn", "accepted",
                                  "user prompt accepted", &err);
            printf("accepted tokens: %llu\n", result.prompt_tokens);
            printf("position: %llu\n", result.position);
            printf("assistant: [generation unsupported in diagnostic runtime]\n");
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
    yvex_model_ref_clear(&model_ref);
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
        fprintf(stderr, "yvex: source-manifest inspect is not implemented in open-weight intake\n");
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
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));

    if (argc != 3 || strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0) {
        if (argc == 3) {
            print_command_help(stdout, find_command("tensors"));
            return 0;
        }
        fprintf(stderr, "yvex: tensors requires exactly one FILE_OR_ALIAS\n");
        fprintf(stderr, "usage: yvex tensors FILE_OR_ALIAS\n");
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
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(artifact,
                                              gguf,
                                              table,
                                              &integrity_options,
                                              &integrity_report,
                                              &err);
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
        yvex_tensor_range range;
        int range_rc;

        memset(&range, 0, sizeof(range));
        range_rc = yvex_tensor_range_validate(artifact, gguf, tensor, &range, &err);
        printf("%llu %s role=%s rank=%u dims=",
               i,
               tensor->name,
               yvex_tensor_role_name(tensor->role),
               tensor->rank);
        print_tensor_dims(tensor->dims, tensor->rank);
        printf(" dtype=%s bytes=%llu offset=%llu absolute=%llu",
               yvex_dtype_name(tensor->dtype),
               tensor->storage_bytes,
               tensor->relative_offset,
               tensor->absolute_offset);
        if (range_rc == YVEX_OK) {
            printf(" range=%llu..%llu range_status=valid alignment_status=%s\n",
                   range.tensor_absolute_offset,
                   range.tensor_end_offset,
                   range.aligned ? "valid" : "invalid");
        } else {
            printf(" range_status=invalid alignment_status=unknown\n");
            yvex_error_clear(&err);
        }
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
