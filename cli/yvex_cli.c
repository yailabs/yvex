/*
 * YVEX - CLI bootstrap
 *
 * File: cli/yvex_cli.c
 * Layer: CLI
 *
 * Purpose:
 *   Implements the command table and CLI proof surface for implemented core
 *   filesystem, artifact, and GGUF directory behavior. The CLI reports what
 *   exists and does not expose future runtime commands before their modules
 *   exist.
 *
 * Implements:
 *   - yvex
 *   - yvex info
 *   - yvex commands
 *   - yvex help [command]
 *   - yvex version
 *   - yvex paths
 *   - yvex inspect <path>
 *   - yvex metadata <path>
 *   - yvex tensors <path>
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
#include <stdio.h>
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

static int command_commands(int argc, char **argv);
static int command_help(int argc, char **argv);
static int command_info(int argc, char **argv);
static int command_inspect(int argc, char **argv);
static int command_metadata(int argc, char **argv);
static int command_paths(int argc, char **argv);
static int command_tensors(int argc, char **argv);
static int command_version(int argc, char **argv);

static const yvex_cli_command yvex_commands[] = {
    {
        "commands",
        "List implemented CLI commands.",
        "yvex commands",
        "Prints the command names implemented by this binary.",
        command_commands,
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
        "Prints the implemented core/filesystem/artifact/GGUF directory status.",
        command_info,
    },
    {
        "inspect",
        "Inspect a GGUF artifact directory.",
        "yvex inspect <path>",
        "Opens a file, parses the GGUF header, metadata table, and tensor directory, and prints a directory-only summary. Tokenizers, model descriptors, and model loading are not implemented.",
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
        "tensors",
        "Print parsed GGUF tensor directory records.",
        "yvex tensors <path>",
        "Opens a GGUF file and prints raw tensor directory records. Dtype/qtype registry, tensor table, backend support, and model loading are not implemented.",
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
    printf("status: C1 GGUF metadata/tensor directory parser\n");
    printf("library: libyvex.a\n");
    printf("filesystem: implemented\n");
    printf("artifact: open/read implemented\n");
    printf("inference: not implemented\n");
    printf("gguf: metadata/tensor directory parsing implemented\n");
    printf("cuda: not implemented\n");
    printf("server: not implemented\n");
    return 0;
}

static int command_inspect(int argc, char **argv)
{
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
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
        header = yvex_gguf_header_view(gguf);
        printf("format: gguf\n");
        printf("version: %u\n", header->version);
        printf("metadata_count: %llu\n", header->metadata_count);
        printf("tensor_count: %llu\n", header->tensor_count);
        printf("tensor_data_offset: %llu\n", yvex_gguf_tensor_data_offset(gguf));
        printf("alignment: %u\n", yvex_gguf_alignment(gguf));
        printf("status: directory-only\n");
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return 0;
    }

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

static int command_tensors(int argc, char **argv)
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
    if (rc != YVEX_OK) {
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

    for (i = 0; i < yvex_gguf_tensor_count(gguf); ++i) {
        unsigned int d;
        const yvex_gguf_tensor_info *tensor = yvex_gguf_tensor_at(gguf, i);
        printf("%llu %s rank=%u dims=[", i, tensor->name, tensor->rank);
        for (d = 0; d < tensor->rank; ++d) {
            if (d > 0) {
                printf(",");
            }
            printf("%llu", tensor->dims[d]);
        }
        printf("] type=%s offset=%llu absolute=%llu\n",
               tensor->ggml_type_name,
               tensor->relative_offset,
               tensor->absolute_offset);
    }

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
