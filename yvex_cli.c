/*
 * yvex_cli.c - Operator CLI entrypoint.
 *
 * Command metadata lives in yvex_cli_commands.c and command implementations
 * live in private root-first CLI modules. This file owns only argv dispatch.
 */

#include "yvex_cli_private.h"

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
