/* Owner: src/cli/io
 * Owns: approved direct JSON text output for CLI plumbing surfaces.
 * Does not own: command schemas, domain report semantics, runtime behavior, generation, eval, benchmark, or release
 *   decisions.
 * Invariants: helpers serialize only caller-provided fields and do not claim uniform JSON.
 * Boundary: JSON writer primitives are not command-level JSON support by themselves.
 * Purpose: provide approved direct JSON text output for CLI plumbing surfaces.
 * Inputs: typed scalar, table, or JSON fields and the selected operator stream.
 * Effects: writes only explicitly requested operator output bytes.
 * Failure: propagates stream or encoding failure without changing domain state. */
#include "src/cli/io/private.h"

/* Purpose: Compute json string for its CLI invariant (`json_string`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void json_string(FILE *fp, const char *text)
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

/* Purpose: Compute json begin for its CLI invariant (`yvex_cli_json_begin`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_json_begin(FILE *fp)
{
    yvex_cli_out_line(fp, "{");
}

/* Purpose: Compute json end for its CLI invariant (`yvex_cli_json_end`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_json_end(FILE *fp)
{
    yvex_cli_out_line(fp, "}");
}

/* Purpose: Compute json field str for its CLI invariant (`yvex_cli_json_field_str`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_json_field_str(FILE *fp, const char *key, const char *value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    json_string(fp, key);
    (void)yvex_cli_out_puts(fp, ": ");
    json_string(fp, value);
    (void)yvex_cli_out_writef(fp, "%s\n", comma ? "," : "");
}

/* Purpose: Compute json field u64 for its CLI invariant (`yvex_cli_json_field_u64`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_json_field_u64(FILE *fp, const char *key, unsigned long long value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    json_string(fp, key);
    (void)yvex_cli_out_writef(fp, ": %llu%s\n", value, comma ? "," : "");
}

/* Purpose: Compute json field bool for its CLI invariant (`yvex_cli_json_field_bool`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_json_field_bool(FILE *fp, const char *key, int value, int comma)
{
    (void)yvex_cli_out_puts(fp, "  ");
    json_string(fp, key);
    (void)yvex_cli_out_writef(fp, ": %s%s\n", value ? "true" : "false", comma ? "," : "");
}
