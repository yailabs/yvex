/*
 * YVEX - slash command tests
 *
 * File: tests/test_slash_commands.c
 * Layer: test
 *
 * Purpose:
 *   Proves the diagnostic runtime slash-command parser recognizes required chat commands and
 *   leaves normal text alone.
 *
 * Covers:
 *   - yvex_slash_parse
 *   - yvex_slash_command_name
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_slash_commands
 *
 * Expected:
 *   - exits 0 on success
 */
#include "yvex_chat_slash_internal.h"
#include "test.h"

int main(void)
{
    YVEX_TEST_ASSERT(yvex_slash_parse("/help") == YVEX_SLASH_HELP, "help recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/status") == YVEX_SLASH_STATUS, "status recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/model") == YVEX_SLASH_MODEL, "model recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/backend") == YVEX_SLASH_BACKEND, "backend recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/tokens") == YVEX_SLASH_TOKENS, "tokens recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/reset") == YVEX_SLASH_RESET, "reset recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/quit") == YVEX_SLASH_QUIT, "quit recognized");
    YVEX_TEST_ASSERT(yvex_slash_parse("/bad") == YVEX_SLASH_UNKNOWN, "unknown handled");
    YVEX_TEST_ASSERT(yvex_slash_parse("hello world") == YVEX_SLASH_NOT_COMMAND, "normal text");
    YVEX_TEST_ASSERT_STREQ(yvex_slash_command_name(YVEX_SLASH_STATUS), "status", "status name");
    return 0;
}
