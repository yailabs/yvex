/*
 * yvex_cli_error.c - CLI parser error writer implementation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   approved stderr text for parser and errno errors.
 *
 * Does not own:
 *   command behavior, domain validation, runtime behavior, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   direct stderr output stays in this file; helpers preserve compact parser
 *   wording and do not decide command semantics.
 *
 * Boundary:
 *   error text serialization is not feature support.
 */
#include "yvex_cli_error.h"

#include "yvex_cli_out.h"

#include <errno.h>
#include <string.h>

void yvex_cli_error_usage(FILE *fp, const char *command, const char *message)
{
    (void)yvex_cli_out_writef(fp ? fp : stderr, "%s: %s\n",
                              command ? command : "yvex",
                              message ? message : "invalid usage");
}

void yvex_cli_error_unknown_option(FILE *fp, const char *command, const char *option)
{
    (void)yvex_cli_out_writef(fp ? fp : stderr, "%s: unknown option: %s\n",
                              command ? command : "yvex",
                              option ? option : "");
}

void yvex_cli_error_missing_value(FILE *fp, const char *option, const char *expected)
{
    (void)yvex_cli_out_writef(fp ? fp : stderr, "%s requires %s\n",
                              option ? option : "option",
                              expected ? expected : "a value");
}

void yvex_cli_error_unsupported_value(FILE *fp, const char *option, const char *value)
{
    (void)yvex_cli_out_writef(fp ? fp : stderr, "%s unsupported value: %s\n",
                              option ? option : "option",
                              value ? value : "");
}

void yvex_cli_error_errno(FILE *fp, const char *message)
{
    (void)yvex_cli_out_writef(fp ? fp : stderr, "%s: %s\n",
                              message ? message : "error",
                              strerror(errno));
}
