/*
 * YVEX - Chat REPL helpers
 *
 * File: src/chat/repl.c
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Provides small output helpers for the diagnostic runtime chat shell. Line reading remains
 *   in the CLI command so stdin/stdout behavior stays explicit.
 *
 * Implements:
 *   - yvex_chat_runtime_print_status
 *
 * Invariants:
 *   - REPL output is deterministic in piped mode
 *   - no generated assistant text is emitted
 *
 * Commands:
 *   - make test-cli
 *   - tests/test_cli_chat.sh
 */
#include "chat_internal.h"

int yvex_chat_runtime_print_status(FILE *fp,
                                   const yvex_chat_runtime *runtime,
                                   yvex_error *err)
{
    yvex_session_summary summary;
    int rc;

    if (!fp || !runtime) {
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_chat_runtime_get_summary(runtime, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    fprintf(fp, "session_state: %s\n", yvex_session_state_name(summary.state));
    fprintf(fp, "position: %llu\n", summary.position);
    fprintf(fp, "accepted_tokens: %llu\n", summary.accepted_tokens);
    fprintf(fp, "execution_ready: false\n");
    fprintf(fp, "generation: unsupported\n");
    return YVEX_OK;
}
