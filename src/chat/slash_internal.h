/*
 * YVEX - Slash command internals
 *
 * File: src/chat/slash_internal.h
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Defines the private slash-command parser used by the I0 chat shell.
 *   Slash commands inspect/reset the runtime shell; ordinary text is not
 *   treated as a command.
 *
 * Owns:
 *   - yvex_slash_command
 *   - slash command parser/classifier
 *
 * Does not own:
 *   - REPL line reading
 *   - session/backend execution
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_slash_commands
 */
#ifndef YVEX_SLASH_INTERNAL_H
#define YVEX_SLASH_INTERNAL_H

typedef enum {
    YVEX_SLASH_NOT_COMMAND = 0,
    YVEX_SLASH_HELP,
    YVEX_SLASH_STATUS,
    YVEX_SLASH_MODEL,
    YVEX_SLASH_BACKEND,
    YVEX_SLASH_TOKENS,
    YVEX_SLASH_RESET,
    YVEX_SLASH_QUIT,
    YVEX_SLASH_UNKNOWN
} yvex_slash_command;

yvex_slash_command yvex_slash_parse(const char *line);
const char *yvex_slash_command_name(yvex_slash_command command);

#endif /* YVEX_SLASH_INTERNAL_H */
