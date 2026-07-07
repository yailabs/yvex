/*
 * yvex_cli_out.c - CLI stdout/stderr text writer implementation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   approved direct text output calls for operator normal/table/audit text.
 *
 * Does not own:
 *   domain facts, command parsing, report building, JSON policy, runtime
 *   behavior, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   direct stdio output stays in this file; callers provide target streams;
 *   wrappers preserve stdio return behavior where legacy code checks it.
 *
 * Boundary:
 *   writer calls serialize existing facts only and do not create capability.
 */
#include "yvex_cli_out.h"

int yvex_cli_out_vwritef(FILE *fp, const char *fmt, va_list ap)
{
    return vfprintf(fp ? fp : stdout, fmt ? fmt : "", ap);
}

int yvex_cli_out_writef(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = yvex_cli_out_vwritef(fp, fmt, ap);
    va_end(ap);
    return rc;
}

int yvex_cli_out_puts(FILE *fp, const char *text)
{
    return fputs(text ? text : "", fp ? fp : stdout);
}

int yvex_cli_out_fputs(const char *text, FILE *fp)
{
    return yvex_cli_out_puts(fp, text);
}

int yvex_cli_out_char(FILE *fp, int ch)
{
    return fputc(ch, fp ? fp : stdout);
}

FILE *yvex_cli_out_stdout(void)
{
    return stdout;
}

FILE *yvex_cli_out_stderr(void)
{
    return stderr;
}

void yvex_cli_out_line(FILE *fp, const char *text)
{
    (void)yvex_cli_out_puts(fp, text);
    (void)yvex_cli_out_char(fp, '\n');
}

void yvex_cli_out_blank(FILE *fp)
{
    (void)yvex_cli_out_char(fp, '\n');
}

void yvex_cli_out_kv_str(FILE *fp, const char *key, const char *value)
{
    (void)yvex_cli_out_writef(fp, "%s: %s\n", key ? key : "", value ? value : "");
}

void yvex_cli_out_kv_u64(FILE *fp, const char *key, unsigned long long value)
{
    (void)yvex_cli_out_writef(fp, "%s: %llu\n", key ? key : "", value);
}

void yvex_cli_out_kv_u32(FILE *fp, const char *key, unsigned int value)
{
    (void)yvex_cli_out_writef(fp, "%s: %u\n", key ? key : "", value);
}

void yvex_cli_out_kv_bool(FILE *fp, const char *key, int value)
{
    yvex_cli_out_kv_str(fp, key, value ? "true" : "false");
}

void yvex_cli_out_kv_double(FILE *fp, const char *key, double value)
{
    (void)yvex_cli_out_writef(fp, "%s: %.17g\n", key ? key : "", value);
}

void yvex_cli_out_optional_u64(FILE *fp,
                               const char *key,
                               int seen,
                               unsigned long long value)
{
    if (seen) {
        yvex_cli_out_kv_u64(fp, key, value);
    } else {
        yvex_cli_out_kv_str(fp, key, "unset");
    }
}

void yvex_cli_out_token_list(FILE *fp,
                             const char *key,
                             const unsigned int *tokens,
                             unsigned long long count)
{
    unsigned long long i;

    (void)yvex_cli_out_writef(fp, "%s:", key ? key : "");
    for (i = 0; i < count; ++i) {
        (void)yvex_cli_out_writef(fp, "%s%u", i == 0 ? " " : ",", tokens ? tokens[i] : 0u);
    }
    (void)yvex_cli_out_char(fp, '\n');
}
