/*
 * YVEX - CLI bootstrap
 *
 * File: cli/yvex_cli.c
 * Layer: CLI
 *
 * Purpose:
 *   Implements the command table and CLI proof surface for implemented core,
 *   filesystem, artifact, GGUF directory, tensor table, and descriptor
 *   behavior. The CLI reports what exists and does not expose future runtime
 *   commands before their modules exist.
 *
 * Implements:
 *   - yvex
 *   - yvex backend cpu
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
 *   - yvex graph <path>
 *   - yvex prompt <path> --user TEXT
 *   - yvex plan <path>
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
static int command_commands(int argc, char **argv);
static int command_detokenize(int argc, char **argv);
static int command_graph(int argc, char **argv);
static int command_help(int argc, char **argv);
static int command_info(int argc, char **argv);
static int command_inspect(int argc, char **argv);
static int command_metadata(int argc, char **argv);
static int command_paths(int argc, char **argv);
static int command_plan(int argc, char **argv);
static int command_prompt(int argc, char **argv);
static int command_tokenize(int argc, char **argv);
static int command_tokenizer(int argc, char **argv);
static int command_tensors(int argc, char **argv);
static int command_version(int argc, char **argv);

static const yvex_cli_command yvex_commands[] = {
    {
        "backend",
        "Inspect an implemented backend.",
        "yvex backend cpu|cuda",
        "Reports backend status and capabilities. G0 implements CPU tensor allocation/read/write/embed; CUDA remains unsupported until L0.",
        command_backend,
    },
    {
        "commands",
        "List implemented CLI commands.",
        "yvex commands",
        "Prints the command names implemented by this binary.",
        command_commands,
    },
    {
        "detokenize",
        "Decode token IDs with an implemented tokenizer.",
        "yvex detokenize <path> --ids IDS",
        "Opens a GGUF tokenizer descriptor and decodes comma-separated token IDs. E0 executes only the yvex-fixture-simple tokenizer.",
        command_detokenize,
    },
    {
        "graph",
        "Build and dump the F0 graph planning substrate.",
        "yvex graph <path> [--seq N] [--ctx N]",
        "Opens a GGUF descriptor, builds a deterministic graph planning artifact, and prints values, planned ops, and missing-role diagnostics. It does not execute the graph.",
        command_graph,
    },
    {
        "help",
        "Show help for the CLI or a command.",
        "yvex help [command]",
        "Prints top-level help or detailed help for one implemented command.",
        command_help,
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
        "metadata",
        "Print parsed GGUF metadata entries.",
        "yvex metadata <path>",
        "Opens a GGUF file and prints parsed metadata key/value summaries. Arrays are summarized; tokenizers and model loading are not implemented.",
        command_metadata,
    },
    {
        "paths",
        "Show resolved runtime filesystem paths.",
        "yvex paths [--project DIR] [--run] [--create]",
        "Prints resolved config/cache/state/data paths. With --run it prints a prepared run directory. With --create it creates that run directory.",
        command_paths,
    },
    {
        "plan",
        "Build and dump an estimate-only execution plan.",
        "yvex plan <path> [--backend cpu|cuda] [--seq N] [--ctx N]",
        "Builds a graph and memory estimate. G0 reports CPU backend availability and CUDA unsupported status; execution remains disabled.",
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

static int command_backend(int argc, char **argv)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_memory_stats stats;
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
        if (options.kind == YVEX_BACKEND_KIND_CUDA) {
            printf("reason: CUDA backend is planned for L0\n");
        } else {
            printf("reason: %s\n", yvex_error_message(&err));
        }
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
    printf("memory:\n");
    printf("  allocated_bytes: %llu\n", stats.allocated_bytes);
    printf("  allocation_count: %llu\n", stats.allocation_count);
    printf("  peak_allocated_bytes: %llu\n", stats.peak_allocated_bytes);
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

static int command_info(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("name: YVEX\n");
    printf("version: %s\n", yvex_version_string());
    printf("language: C\n");
    printf("interface: CLI-only\n");
    printf("status: G0 CPU reference backend ABI\n");
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
    printf("backend_cuda: not implemented\n");
    printf("inference: not implemented\n");
    printf("cuda: not implemented\n");
    printf("server: not implemented\n");
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
