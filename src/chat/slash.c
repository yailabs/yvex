/*
 * YVEX - Slash commands
 *
 * File: src/chat/slash.c
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Classifies I0 chat slash commands. Unknown slash commands remain handled
 *   runtime input and never terminate the process by themselves.
 *
 * Implements:
 *   - yvex_slash_parse
 *   - yvex_slash_command_name
 *
 * Invariants:
 *   - ordinary text is not a slash command
 *   - parser has no heap allocation
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_slash_commands
 */
#include "slash_internal.h"

#include <string.h>

static int slash_eq(const char *line, const char *command)
{
    size_t len = strlen(command);

    return strncmp(line, command, len) == 0 &&
           (line[len] == '\0' || line[len] == '\n' || line[len] == '\r' ||
            line[len] == ' ' || line[len] == '\t');
}

yvex_slash_command yvex_slash_parse(const char *line)
{
    if (!line || line[0] != '/') {
        return YVEX_SLASH_NOT_COMMAND;
    }
    if (slash_eq(line, "/help")) return YVEX_SLASH_HELP;
    if (slash_eq(line, "/status")) return YVEX_SLASH_STATUS;
    if (slash_eq(line, "/model")) return YVEX_SLASH_MODEL;
    if (slash_eq(line, "/backend")) return YVEX_SLASH_BACKEND;
    if (slash_eq(line, "/tokens")) return YVEX_SLASH_TOKENS;
    if (slash_eq(line, "/reset")) return YVEX_SLASH_RESET;
    if (slash_eq(line, "/quit")) return YVEX_SLASH_QUIT;
    return YVEX_SLASH_UNKNOWN;
}

const char *yvex_slash_command_name(yvex_slash_command command)
{
    switch (command) {
    case YVEX_SLASH_NOT_COMMAND: return "not-command";
    case YVEX_SLASH_HELP: return "help";
    case YVEX_SLASH_STATUS: return "status";
    case YVEX_SLASH_MODEL: return "model";
    case YVEX_SLASH_BACKEND: return "backend";
    case YVEX_SLASH_TOKENS: return "tokens";
    case YVEX_SLASH_RESET: return "reset";
    case YVEX_SLASH_QUIT: return "quit";
    case YVEX_SLASH_UNKNOWN: return "unknown";
    }
    return "unknown";
}
