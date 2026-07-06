/*
 * yvex_cli_out.h - CLI stdout/stderr text writer primitives.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   operator text output primitives used by CLI command and render files.
 *
 * Does not own:
 *   domain facts, argv parsing, report building, runtime behavior, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   callers pass an explicit stream; formatting stays centralized in writer
 *   files; writer helpers do not create capability claims.
 *
 * Boundary:
 *   output serialization is not feature support or release readiness.
 */
#ifndef YVEX_CLI_OUT_H
#define YVEX_CLI_OUT_H

#include <stdarg.h>
#include <stdio.h>

int yvex_cli_out_writef(FILE *fp, const char *fmt, ...);
int yvex_cli_out_vwritef(FILE *fp, const char *fmt, va_list ap);
int yvex_cli_out_puts(FILE *fp, const char *text);
int yvex_cli_out_fputs(const char *text, FILE *fp);
int yvex_cli_out_char(FILE *fp, int ch);

void yvex_cli_out_line(FILE *fp, const char *text);
void yvex_cli_out_blank(FILE *fp);
void yvex_cli_out_kv_str(FILE *fp, const char *key, const char *value);
void yvex_cli_out_kv_u64(FILE *fp, const char *key, unsigned long long value);
void yvex_cli_out_kv_u32(FILE *fp, const char *key, unsigned int value);
void yvex_cli_out_kv_bool(FILE *fp, const char *key, int value);
void yvex_cli_out_kv_double(FILE *fp, const char *key, double value);
void yvex_cli_out_optional_u64(FILE *fp,
                               const char *key,
                               int seen,
                               unsigned long long value);
void yvex_cli_out_token_list(FILE *fp,
                             const char *key,
                             const unsigned int *tokens,
                             unsigned long long count);

#endif
