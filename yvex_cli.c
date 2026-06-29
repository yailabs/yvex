/*
 * yvex_cli.c - Operator CLI entrypoint.
 *
 * This file owns the top-level command grammar, help/catalog rendering,
 * dispatch, and small shared argv/output helpers. Command behavior lives in
 * the domain modules that own the underlying runtime or artifact boundary.
 */

#include "yvex_command_private.h"

/* Shared command helpers. */

int print_yvex_error(const yvex_error *err, int exit_code)
{
    fprintf(stderr, "yvex: %s: %s\n", yvex_error_where(err), yvex_error_message(err));
    return exit_code;
}

int exit_for_status(int status)
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

void print_quoted_bytes(const char *data, unsigned long long len)
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

int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
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

void close_tokenizer_context(yvex_cli_tokenizer_context *ctx)
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

void close_model_context(yvex_cli_tokenizer_context *ctx)
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

int open_model_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
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

int open_tokenizer_context(const char *path, yvex_cli_tokenizer_context *ctx, yvex_error *err)
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

void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
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

void print_native_dims(const unsigned long long *dims, unsigned int rank)
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

void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;

    printf("ids:");
    for (i = 0; i < tokens->len; ++i) {
        printf(" %u", tokens->ids[i]);
    }
    printf("\n");
}

int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
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

int parse_positive_ull(const char *text, unsigned long long *out)
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

int parse_ull_allow_zero(const char *text, unsigned long long *out)
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

int parse_uint_allow_zero(const char *text, unsigned int *out)
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

int parse_dims_csv(const char *text,
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

/* Top-level command catalog and help surface. */

static const yvex_cli_command yvex_commands[] = {
    {
        "backend",
        "Inspect an implemented backend.",
        "yvex backend cpu|cuda",
        "Reports backend status and capabilities. CPU is the reference path; CUDA is available when the local driver/device probe succeeds.",
        yvex_cli_command_backend,
    },
    {
        "chat",
        "Start the diagnostic console.",
        "yvex chat [--model FILE_OR_ALIAS] --backend cpu|cuda [--ctx N] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session state and accepts user text without generating output. If --model is omitted, chat uses the current model selected with yvex models use ALIAS.",
        yvex_cli_command_chat,
    },
    {
        "commands",
        "List implemented CLI commands.",
        "yvex commands",
        "Prints the command names implemented by this binary.",
        yvex_cli_command_commands,
    },
    {
        "convert",
        "Plan or emit selected open-weight GGUF conversions.",
        "yvex convert plan --arch ARCH --native-source DIR --out-plan FILE | yvex convert emit --arch ARCH --native-source DIR --tensor NAME --target-qtype QTYPE --out FILE [--overwrite]",
        "Builds conversion plans and emits selected tensor GGUF artifacts from official safetensors sources. Output files should use the canonical GGUF artifact naming grammar. It does not infer, execute a full model, or claim generation support.",
        yvex_cli_command_convert,
    },
    {
        "cuda-info",
        "Probe CUDA devices.",
        "yvex cuda-info",
        "Reports CUDA driver/device facts when CUDA is available. It does not claim CUDA model execution, matmul, transformer attention, or inference.",
        yvex_cli_command_cuda_info,
    },
    {
        "detokenize",
        "Decode token IDs with an implemented tokenizer.",
        "yvex detokenize <path> --ids IDS",
        "Opens a GGUF tokenizer descriptor and decodes comma-separated token IDs. Only the fixture tokenizer path is implemented.",
        yvex_cli_command_detokenize,
    },
    {
        "engine",
        "Open an engine descriptor.",
        "yvex engine [--model] FILE_OR_ALIAS [--backend cpu|cuda]",
        "Opens the descriptor/tokenizer/graph stack and, when --backend is provided, attaches selected materialized weights as engine-owned residency state. It does not execute prefill, decode, or generation.",
        yvex_cli_command_engine,
    },
    {
        "graph",
        "Build graph diagnostics or execute narrow graph segments.",
        "yvex graph [--model] FILE_OR_ALIAS [--seq N] [--ctx N] [--backend cpu|cuda] [--execute-fixture] [--execute-partial] [--execute-segment --segment embedding-rmsnorm] [--partial-token N] [--tokens IDS --token-index N] | yvex graph --backend cpu|cuda --execute-op --op rope --position N --head-dim N | yvex graph --backend cpu|cuda --execute-op --op attention --seq-len N --position N --head-dim N [--causal] | yvex graph --backend cpu|cuda --execute-op --op matmul --m M --k K --n N | yvex graph --backend cpu|cuda --execute-op --op mlp --hidden-dim N --ffn-dim N --activation silu --gated [--experts N --expert-id N]",
        "Opens a GGUF descriptor and prints graph planning diagnostics. Execution flags attach selected weights and run the deterministic fixture embed node, real selected F16 embedding segment, selected embedding-plus-RMSNorm segment, standalone RoPE position op, standalone F32 attention primitive, standalone F32 matmul/projection primitive, or standalone F32 MLP/feed-forward primitive; none of these paths is inference or text generation.",
        yvex_cli_command_graph,
    },
    {
        "gguf-template",
        "Inspect or validate a GGUF conversion template.",
        "yvex gguf-template inspect|validate --template FILE | yvex gguf-template compare --template FILE --native-source DIR",
        "Validates GGUF metadata, tokenizer metadata, tensor directory, tensor roles, and optional exact-name native inventory comparison. It does not emit GGUF, quantize, materialize, or infer.",
        yvex_cli_command_gguf_template,
    },
    {
        "gguf-emit",
        "Emit a controlled YVEX-owned GGUF.",
        "yvex gguf-emit controlled --out FILE [--template FILE] [--model-name NAME] [--arch ARCH] [--target-qtype F32|F16] [--overwrite]",
        "Writes one controlled F32 or F16 tensor and controlled tokenizer metadata, then validates the emitted GGUF through YVEX parse and CPU materialization. It does not convert DeepSeek, quantize, infer, or emit a generic model.",
        yvex_cli_command_gguf_emit,
    },
    {
        "help",
        "Show help for the CLI or a command.",
        "yvex help [command]",
        "Prints top-level help or detailed help for one implemented command.",
        yvex_cli_command_help,
    },
    {
        "imatrix",
        "Create, inspect, or validate an imatrix manifest.",
        "yvex imatrix create --name NAME --arch NAME --imatrix FILE --format FORMAT --status STATUS --out FILE | yvex imatrix inspect|validate --manifest FILE",
        "Handles calibration/imatrix provenance manifests. It does not generate imatrix data, run calibration, quantize, emit GGUF, materialize, or infer.",
        yvex_cli_command_imatrix,
    },
    {
        "info",
        "Show current YVEX build and implementation status.",
        "yvex info",
        "Prints the implemented core/filesystem/artifact/GGUF/tensor descriptor status.",
        yvex_cli_command_info,
    },
    {
        "inspect",
        "Inspect a GGUF artifact descriptor.",
        "yvex inspect FILE_OR_ALIAS",
        "Opens a file, parses the GGUF directory, builds a YVEX tensor table and descriptor, and prints a descriptor-only summary. Inspect does not materialize weights or execute a graph.",
        yvex_cli_command_inspect,
    },
    {
        "input",
        "Parse and validate runtime token input.",
        "yvex input tokens --model FILE_OR_ALIAS --tokens IDS | yvex input prompt --model FILE_OR_ALIAS --text TEXT",
        "Parses explicit token sequences or tokenizer-backed prompt text into validated token input. This is not prefill, decode, logits, sampling, or generation.",
        yvex_cli_command_input,
    },
    {
        "integrity",
        "Check or report local artifact integrity.",
        "yvex integrity check --model FILE_OR_ALIAS [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N] | yvex integrity report --model FILE_OR_ALIAS [--backend cpu|cuda] [--expect-sha256 HASH] [--require-token-embedding] [--partial-token N]",
        "Validates GGUF structural bounds, tensor byte ranges, local file identity digest, checked dtype/shape accounting, optional selected embedding readiness, and operator report summaries. It is not a supply-chain security audit.",
        yvex_cli_command_integrity,
    },
    {
        "kv",
        "Create a minimal session-owned KV store.",
        "yvex kv --layers N --heads N --head-dim N --capacity N [--append-demo] [--read-position N]",
        "Allocates a minimal F32 session-owned KV store, optionally appends deterministic demo positions, reads one position, and reports lifecycle/bounds facts. It does not run attention, decode, logits, sampling, generation, or prefill.",
        yvex_cli_command_kv,
    },
    {
        "materialize",
        "Materialize selected weights into backend tensors.",
        "yvex materialize --model FILE_OR_ALIAS --backend cpu|cuda [--require-all] [--allow-unsupported-dtype]",
        "Resolves a model path or registered alias, copies GGUF tensor bytes into backend-owned tensors, and reports residency. This does not execute prefill, decode, sampling, generation, or inference.",
        yvex_cli_command_materialize,
    },
    {
        "materialize-gate",
        "Run a repeatable materialization hardening gate.",
        "yvex materialize-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --scope selected-tensor --expect-tensor NAME --expect-rank N --expect-dims model layer,D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--repeat N] [--check-cleanup] [--report-out FILE]",
        "Validates file identity, tensor specs, repeated CPU/CUDA materialization, cleanup, and failure classes. materialization gate uses DeepSeek as the only live target and does not claim execution or inference.",
        yvex_cli_command_materialize_gate,
    },
    {
        "metadata",
        "Print parsed GGUF metadata entries.",
        "yvex metadata FILE_OR_ALIAS",
        "Resolves a model path or registered alias, opens a GGUF file, and prints parsed metadata key/value summaries. Arrays are summarized.",
        yvex_cli_command_metadata,
    },
    {
        "model-gate",
        "Validate a produced GGUF artifact materialization gate.",
        "yvex model-gate check --model FILE_OR_ALIAS --label LABEL --family FAMILY --expect-tensor NAME --expect-rank N --expect-dims model layer,D1[,D2,D3] --expect-dtype DTYPE --expect-bytes BYTES [--sha256 HASH] [--backend cpu] [--backend cuda] [--require-cpu] [--require-cuda] [--report-out FILE]",
        "Checks file identity, expected tensor specs, and requested CPU/CUDA materialization. It classifies selected-tensor materialization only and does not claim full-model support, graph execution, prefill, decode, generation, or inference.",
        yvex_cli_command_model_gate,
    },
    {
        "models",
        "Manage the local model alias registry.",
        "yvex models scan --root DIR [--registry FILE] | yvex models add --path FILE [--alias ALIAS] [--support-level LEVEL] [--registry FILE] | yvex models list [--registry FILE] | yvex models use ALIAS [--registry FILE] | yvex models current [--registry FILE] | yvex models verify ALIAS [--registry FILE] | yvex models inspect ALIAS [--registry FILE] | yvex models remove ALIAS [--registry FILE]",
        "Discovers, registers, verifies, lists, selects, inspects, and removes local model artifacts by alias. Aliases use canonical artifact names such as deepseek4-v4-flash-selected-embed; simple labels such as controlled are rejected.",
        yvex_cli_command_models,
    },
    {
        "native-weights",
        "Inventory safetensors native weights.",
        "yvex native-weights --source DIR [--limit N] [--tensor NAME] [--json]",
        "Reads safetensors headers under a local source directory and reports tensor metadata. It does not read tensor payloads, convert, quantize, emit GGUF, materialize, or infer.",
        yvex_cli_command_native_weights,
    },
    {
        "paths",
        "Show resolved runtime filesystem paths.",
        "yvex paths [--project DIR] [--run] [--create]",
        "Prints resolved config/cache/state/data paths. With --run it prints a prepared run directory. With --create it creates that run directory.",
        yvex_cli_command_paths,
    },
    {
        "tensor-map",
        "Map native tensors to GGUF/template roles.",
        "yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]",
        "Maps native safetensors tensor names to canonical YVEX roles and proposed GGUF/template tensor names. It reads metadata only and does not convert, quantize, emit GGUF, materialize, or infer.",
        yvex_cli_command_tensor_map,
    },
    {
        "plan",
        "Build and dump an estimate-only execution plan.",
        "yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx N]",
        "Builds a graph and memory estimate. CPU and CUDA backend capability labels are probed, but execution remains disabled.",
        yvex_cli_command_plan,
    },
    {
        "prefill",
        "Create an inspectable prefill state summary.",
        "yvex prefill --model FILE_OR_ALIAS --backend cpu|cuda --segment embedding-rmsnorm --tokens IDS [--attach-kv --kv-layers N --kv-heads N --kv-head-dim N --kv-capacity N]",
        "Consumes validated explicit token input through the implemented selected embedding-plus-RMSNorm graph segment and records a segment-summary prefill foundation. With --attach-kv it binds processed positions to a minimal session-owned KV store. It does not run attention, decode, logits, sampling, or generation.",
        yvex_cli_command_prefill,
    },
    {
        "prompt",
        "Render a bounded prompt from explicit messages.",
        "yvex prompt <path> [--system TEXT] --user TEXT [--assistant TEXT] [--tokens]",
        "Renders the YVEX default prompt format. Arbitrary Jinja chat templates are not executed in tokenizer layer.",
        yvex_cli_command_prompt,
    },
    {
        "quant-job",
        "Create, inspect, or validate an external quantization job manifest.",
        "yvex quant-job create --name NAME --arch ARCH --tool TOOL --tool-path FILE --native-source DIR --template FILE --out-gguf FILE --log FILE --status STATUS --command TEXT --out FILE | yvex quant-job inspect|validate --manifest FILE",
        "Records a quantization/conversion job manifest. It does not run arbitrary external tools, make external tools part of the YVEX production path, infer, or claim model execution.",
        yvex_cli_command_quant_job,
    },
    {
        "quant-policy",
        "Inspect, validate, or derive a quantization policy manifest.",
        "yvex quant-policy inspect|validate --policy FILE [--template FILE] | yvex quant-policy derive --template FILE --arch NAME --out FILE",
        "Handles declarative qtype policy manifests. It does not quantize tensors, emit GGUF, materialize weights, calibrate imatrix data, or infer.",
        yvex_cli_command_quant_policy,
    },
    {
        "qtype-support",
        "Print conversion qtype support matrix.",
        "yvex qtype-support",
        "Reports policy/storage/emit/quantize/compute support separately. Compute support is not implied by conversion support.",
        yvex_cli_command_qtype_support,
    },
    {
        "run",
        "Accept one prompt through the diagnostic runtime path.",
        "yvex run --model FILE --backend cpu|cuda --prompt TEXT [--system TEXT] [--output plain|json] [--metrics-out FILE] [--trace-out FILE] [--profile-out FILE] [--save-run] [--run-dir DIR]",
        "Opens engine/backend/session state, tokenizes the prompt, accepts tokens, and reports accepted-only diagnostics.",
        yvex_cli_command_run,
    },
    {
        "session",
        "Create an engine/session layer diagnostic session.",
        "yvex session FILE_OR_ALIAS [--backend cpu|cuda] [--ctx N] [--text TEXT] [--accept-tokens]",
        "Creates a lifecycle-only session over an engine and backend. It observes engine-attached weights but does not run prefill, decode, or generation.",
        yvex_cli_command_session,
    },
    {
        "source-manifest",
        "Create an open-weight source provenance manifest.",
        "yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE [--license TEXT] [--model-card URL] [--node NAME] [--dry-run-log FILE] [--download-log FILE] [--pid-file FILE] [--download-command TEXT]",
        "Scans a local official-weight source directory and writes provenance JSON. It does not download, parse safetensors, quantize, emit GGUF, materialize, or infer.",
        yvex_cli_command_source_manifest,
    },
    {
        "tokenize",
        "Encode text with an implemented tokenizer.",
        "yvex tokenize <path> --text TEXT",
        "Opens a GGUF tokenizer descriptor and tokenizes text. Only the fixture tokenizer path is implemented.",
        yvex_cli_command_tokenize,
    },
    {
        "tokenizer",
        "Inspect GGUF tokenizer metadata.",
        "yvex tokenizer <path>",
        "Prints tokenizer kind, support level, vocabulary size, special token IDs, and chat template presence.",
        yvex_cli_command_tokenizer,
    },
    {
        "tensors",
        "Print YVEX tensor table rows.",
        "yvex tensors FILE_OR_ALIAS",
        "Resolves a model path or registered alias and prints YVEX tensor table rows with role, dtype, known storage bytes, and offsets.",
        yvex_cli_command_tensors,
    },
    {
        "version",
        "Print the YVEX version.",
        "yvex version",
        "Prints the same version string as yvex --version.",
        yvex_cli_command_version,
    },
};

static const unsigned long yvex_command_count = sizeof(yvex_commands) / sizeof(yvex_commands[0]);

const yvex_cli_command *yvex_cli_find_command(const char *name)
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

void yvex_cli_print_top_level_help(FILE *fp)
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

void yvex_cli_print_command_help(FILE *fp, const yvex_cli_command *command)
{
    fprintf(fp, "usage: %s\n", command->usage);
    fprintf(fp, "\n");
    fprintf(fp, "%s\n", command->description);
}

int yvex_cli_command_commands(int argc, char **argv)
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

int yvex_cli_command_help(int argc, char **argv)
{
    const yvex_cli_command *command;

    if (argc <= 2) {
        yvex_cli_print_top_level_help(stdout);
        return 0;
    }

    command = yvex_cli_find_command(argv[2]);
    if (!command) {
        fprintf(stderr, "yvex: unknown help topic: %s\n", argv[2]);
        fprintf(stderr, "Try 'yvex commands' for implemented commands.\n");
        return 2;
    }

    yvex_cli_print_command_help(stdout, command);
    return 0;
}

int yvex_cli_command_version(int argc, char **argv)
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
        yvex_cli_print_top_level_help(stdout);
        return 0;
    }

    name = argv[1];

    if (strcmp(name, "--help") == 0 || strcmp(name, "-h") == 0) {
        yvex_cli_print_top_level_help(stdout);
        return 0;
    }

    if (strcmp(name, "--version") == 0) {
        return yvex_cli_command_version(argc, argv);
    }

    command = yvex_cli_find_command(name);
    if (command) {
        return command->handler(argc, argv);
    }

    fprintf(stderr, "yvex: unknown command: %s\n", name);
    fprintf(stderr, "Try 'yvex help' for usage.\n");
    return 2;
}
