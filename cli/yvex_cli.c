/*
 * YVEX - CLI bootstrap
 *
 * File: cli/yvex_cli.c
 * Layer: CLI
 *
 * Purpose:
 *   Implements the A0.1 command table and CLI proof surface for implemented
 *   core behavior. The CLI reports what exists and does not expose future
 *   runtime commands before their modules exist.
 *
 * Implements:
 *   - yvex
 *   - yvex info
 *   - yvex commands
 *   - yvex help [command]
 *   - yvex version
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
        "Prints the implemented A0.1 core/CLI skeleton status.",
        command_info,
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
    printf("status: A0.1 core/CLI skeleton\n");
    printf("library: libyvex.a\n");
    printf("inference: not implemented\n");
    printf("gguf: not implemented\n");
    printf("cuda: not implemented\n");
    printf("server: not implemented\n");
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
