/*
 * YVEX - Status-line helper
 *
 * File: yvex_chat_status_line.c
 * Layer: CLI runtime implementation
 *
 * Purpose:
 *   Provides the diagnostic runtime plain status-line skeleton. It writes one deterministic
 *   line and does not implement a terminal UI or persistent renderer.
 *
 * Implements:
 *   - yvex_status_line_print
 *
 * Invariants:
 *   - caller chooses stderr/stdout target
 *   - no terminal dependency is introduced
 *
 * Commands:
 *   - make test-core
 *   - make test-cli
 */
#include "yvex_chat_internal.h"

int yvex_status_line_print(FILE *fp,
                           const char *phase,
                           unsigned long long tokens,
                           unsigned long long position)
{
    if (!fp || !phase) {
        return YVEX_ERR_INVALID_ARG;
    }
    fprintf(fp, "[%s] tokens=%llu position=%llu\n", phase, tokens, position);
    return YVEX_OK;
}
