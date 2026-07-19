/* Owner: src/cli/io
 * Owns: approved direct text output calls for operator normal/table/audit text.
 * Does not own: domain facts, command parsing, report building, JSON policy, runtime behavior, generation, eval,
 *   benchmark, or release decisions.
 * Invariants: direct stdio output stays in this file; callers provide target streams; wrappers preserve stdio
 *   return behavior where legacy code checks it.
 * Boundary: writer calls serialize existing facts only and do not create capability.
 * Purpose: provide approved direct text output calls for operator normal/table/audit text.
 * Inputs: typed scalar, table, or JSON fields and the selected operator stream.
 * Effects: writes only explicitly requested operator output bytes.
 * Failure: propagates stream or encoding failure without changing domain state. */
#include "src/cli/io/private.h"

/* Purpose: Transfer bounded out vwritef data (`yvex_cli_out_vwritef`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cli_out_vwritef(FILE *fp, const char *fmt, va_list ap)
{
    return vfprintf(fp ? fp : stdout, fmt ? fmt : "", ap);
}

/* Purpose: Transfer bounded out writef data (`yvex_cli_out_writef`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cli_out_writef(FILE *fp, const char *fmt, ...)
{
    va_list ap;
    int rc;

    va_start(ap, fmt);
    rc = yvex_cli_out_vwritef(fp, fmt, ap);
    va_end(ap);
    return rc;
}

/* Purpose: Compute out puts for its CLI invariant (`yvex_cli_out_puts`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cli_out_puts(FILE *fp, const char *text)
{
    return fputs(text ? text : "", fp ? fp : stdout);
}

/* Purpose: Compute out fputs for its CLI invariant (`yvex_cli_out_fputs`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cli_out_fputs(const char *text, FILE *fp)
{
    return yvex_cli_out_puts(fp, text);
}

/* Purpose: Compute out char for its CLI invariant (`yvex_cli_out_char`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_cli_out_char(FILE *fp, int ch)
{
    return fputc(ch, fp ? fp : stdout);
}

/* Purpose: Compute out stdout for its CLI invariant (`yvex_cli_out_stdout`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
FILE *yvex_cli_out_stdout(void)
{
    return stdout;
}

/* Purpose: Compute out stderr for its CLI invariant (`yvex_cli_out_stderr`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
FILE *yvex_cli_out_stderr(void)
{
    return stderr;
}

/* Purpose: Compute out line for its CLI invariant (`yvex_cli_out_line`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_line(FILE *fp, const char *text)
{
    (void)yvex_cli_out_puts(fp, text);
    (void)yvex_cli_out_char(fp, '\n');
}

/* Emit an immutable ordered block of complete text lines. */
/* Purpose: Compute out lines for its CLI invariant (`yvex_cli_out_lines`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_lines(FILE *fp,
                        const char *const *lines,
                        size_t line_count)
{
    size_t i;

    if (!lines) {
        return;
    }
    for (i = 0; i < line_count; ++i) {
        yvex_cli_out_line(fp, lines[i]);
    }
}

/* Purpose: Compute out blank for its CLI invariant (`yvex_cli_out_blank`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_blank(FILE *fp)
{
    (void)yvex_cli_out_char(fp, '\n');
}

/* Purpose: Compute out kv str for its CLI invariant (`yvex_cli_out_kv_str`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_str(FILE *fp, const char *key, const char *value)
{
    (void)yvex_cli_out_writef(fp, "%s: %s\n", key ? key : "", value ? value : "");
}

/* Purpose: Compute out kv u64 for its CLI invariant (`yvex_cli_out_kv_u64`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_u64(FILE *fp, const char *key, unsigned long long value)
{
    (void)yvex_cli_out_writef(fp, "%s: %llu\n", key ? key : "", value);
}

/* Purpose: Compute out kv u32 for its CLI invariant (`yvex_cli_out_kv_u32`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_u32(FILE *fp, const char *key, unsigned int value)
{
    (void)yvex_cli_out_writef(fp, "%s: %u\n", key ? key : "", value);
}

/* Purpose: Compute out kv bool for its CLI invariant (`yvex_cli_out_kv_bool`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_bool(FILE *fp, const char *key, int value)
{
    yvex_cli_out_kv_str(fp, key, value ? "true" : "false");
}

/* Purpose: Compute out kv double for its CLI invariant (`yvex_cli_out_kv_double`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_double(FILE *fp, const char *key, double value)
{
    (void)yvex_cli_out_writef(fp, "%s: %.17g\n", key ? key : "", value);
}

/* Purpose: Compute out optional u64 for its CLI invariant (`yvex_cli_out_optional_u64`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
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

/* Purpose: Compute out token list for its CLI invariant (`yvex_cli_out_token_list`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_token_list(FILE *fp,
                             const char *key,
                             const unsigned int *tokens,
                             unsigned long long count)
{
    unsigned long long i;

    (void)yvex_cli_out_writef(fp, "%s:", key ? key : "");
    for (i = 0; i < count; ++i) {
        (void)yvex_cli_out_writef(fp, "%s%u", i == 0 ? " " : ",",
                                  tokens ? tokens[i] : 0u);
    }
    (void)yvex_cli_out_char(fp, '\n');
}

/* Purpose: Serialize an immutable typed fact projection in declared order.
 * Inputs: Borrowed object and field schema; offsets address the declared object type.
 * Effects: Writes only to the caller-selected operator stream.
 * Failure: Returns -1 on the first stream failure; invalid projections are refused.
 * Boundary: Field projection formats facts but never derives capability state. */
int yvex_cli_out_fields(FILE *fp,
                        const void *object,
                        const yvex_cli_field_spec *fields,
                        size_t field_count)
{
    const unsigned char *base = object;
    size_t i;

    if (!object || (!fields && field_count != 0u)) {
        return -1;
    }
    for (i = 0; i < field_count; ++i) {
        const yvex_cli_field_spec *field = &fields[i];
        const void *value = base + field->offset;
        const char *text;
        int rc;

        switch (field->kind) {
        case YVEX_CLI_FIELD_TEXT:
            text = *(const char *const *)value;
            rc = yvex_cli_out_writef(fp, "%s: %s\n", field->key,
                                     text && text[0]
                                         ? text
                                         : (field->fallback ? field->fallback : "unknown"));
            break;
        case YVEX_CLI_FIELD_TEXT_ARRAY:
            text = value;
            rc = yvex_cli_out_writef(fp, "%s: %s\n", field->key,
                                     text[0]
                                         ? text
                                         : (field->fallback ? field->fallback : "unknown"));
            break;
        case YVEX_CLI_FIELD_U64:
            rc = yvex_cli_out_writef(fp, "%s: %llu\n", field->key,
                                     *(const unsigned long long *)value);
            break;
        case YVEX_CLI_FIELD_U32:
            rc = yvex_cli_out_writef(fp, "%s: %u\n", field->key,
                                     *(const unsigned int *)value);
            break;
        case YVEX_CLI_FIELD_I32:
            rc = yvex_cli_out_writef(fp, "%s: %d\n", field->key, *(const int *)value);
            break;
        case YVEX_CLI_FIELD_BOOL:
            rc = yvex_cli_out_writef(fp, "%s: %s\n", field->key,
                                     *(const int *)value ? "true" : "false");
            break;
        case YVEX_CLI_FIELD_DOUBLE:
            rc = yvex_cli_out_writef(fp, "%s: %.17g\n", field->key,
                                     *(const double *)value);
            break;
        case YVEX_CLI_FIELD_FLOAT9:
            rc = yvex_cli_out_writef(fp, "%s: %.9g\n", field->key,
                                     *(const double *)value);
            break;
        case YVEX_CLI_FIELD_HEX64:
            rc = yvex_cli_out_writef(fp, "%s: %016llx\n", field->key,
                                     *(const unsigned long long *)value);
            break;
        default:
            return -1;
        }
        if (rc < 0) {
            return -1;
        }
    }
    return 0;
}
