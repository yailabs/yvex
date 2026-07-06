/*
 * yvex_cli_error.h - CLI parser error writer primitives.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   common parser error text helpers for CLI command adapters.
 *
 * Does not own:
 *   parser state, domain validation, runtime behavior, generation, eval, or
 *   benchmark evidence.
 *
 * Invariants:
 *   helpers write only to explicit streams and do not choose exit codes.
 *
 * Boundary:
 *   parser error serialization is not capability support.
 */
#ifndef YVEX_CLI_ERROR_H
#define YVEX_CLI_ERROR_H

#include <stdio.h>

void yvex_cli_error_usage(FILE *fp, const char *command, const char *message);
void yvex_cli_error_unknown_option(FILE *fp, const char *command, const char *option);
void yvex_cli_error_missing_value(FILE *fp, const char *option, const char *expected);
void yvex_cli_error_unsupported_value(FILE *fp, const char *option, const char *value);
void yvex_cli_error_errno(FILE *fp, const char *message);

#endif
