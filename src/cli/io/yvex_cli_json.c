/*
 * yvex_cli_json.c - CLI JSON writer foundation implementation.
 *
 * Owner:
 *   src/cli/io
 *
 * Owns:
 *   approved direct JSON text output for CLI plumbing surfaces.
 *
 * Does not own:
 *   command schemas, domain report semantics, runtime behavior, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   helpers serialize only caller-provided fields and do not claim uniform JSON.
 *
 * Boundary:
 *   JSON writer primitives are not command-level JSON support by themselves.
 */
#include "yvex_cli_json.h"

#include "yvex_cli_out.h"

static void yvex_cli_json_string(FILE *fp, const char *text)
{
    const unsigned char *p = (const unsigned char *)(text ? text : "");

    (void)yvex_cli_out_char(fp, '"');
    while (*p) {
        if (*p == '"' || *p == '\\') {
            (void)yvex_cli_out_char(fp, '\\');
            (void)yvex_cli_out_char(fp, *p);
        } else if (*p == '\n') {
            (void)yvex_cli_out_puts(fp, "\\n");
        } else if (*p == '\r') {
            (void)yvex_cli_out_puts(fp, "\\r");
        } else if (*p == '\t') {
            (void)yvex_cli_out_puts(fp, "\\t");
        } else {
            (void)yvex_cli_out_char(fp, *p);
        }
        ++p;
    }
    (void)yvex_cli_out_char(fp, '"');
}

void yvex_cli_json_begin(FILE *fp)
{
    yvex_cli_out_line(fp, "{");
}

void yvex_cli_json_end(FILE *fp)
{
    yvex_cli_out_line(fp, "}");
}

void yvex_cli_json_field_str(FILE *fp, const char *key, const char *value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    yvex_cli_json_string(fp, key);
    (void)yvex_cli_out_puts(fp, ": ");
    yvex_cli_json_string(fp, value);
    (void)yvex_cli_out_writef(fp, "%s\n", comma ? "," : "");
}

void yvex_cli_json_field_u64(FILE *fp, const char *key, unsigned long long value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    yvex_cli_json_string(fp, key);
    (void)yvex_cli_out_writef(fp, ": %llu%s\n", value, comma ? "," : "");
}

void yvex_cli_json_field_bool(FILE *fp, const char *key, int value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    yvex_cli_json_string(fp, key);
    (void)yvex_cli_out_writef(fp, ": %s%s\n", value ? "true" : "false", comma ? "," : "");
}
