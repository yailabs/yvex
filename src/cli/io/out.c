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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

/* Purpose: force buffered operator bytes to their selected stream.
 * Inputs: explicit stream, or stdout when absent.
 * Effects: flushes only the selected CLI-owned stream.
 * Failure: returns typed I/O refusal for flush or sticky stream failure.
 * Boundary: transport completion does not alter rendered domain facts. */
int yvex_cli_out_flush(FILE *fp)
{
    FILE *stream = fp ? fp : stdout;

    return fflush(stream) == 0 && !ferror(stream) ? YVEX_OK : YVEX_ERR_IO;
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

/* Purpose: Compute out kv str for its CLI invariant (`yvex_cli_out_kv_str`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_cli_out_kv_str(FILE *fp, const char *key, const char *value)
{
    (void)yvex_cli_out_writef(fp, "%s: %s\n", key ? key : "", value ? value : "");
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

/* Purpose: render one typed command failure through the operator error stream.
 * Inputs: immutable typed error and caller-selected nonzero exit code.
 * Effects: writes one diagnostic line to stderr.
 * Failure: preserves the requested exit code even if stderr is unavailable.
 * Boundary: renders domain failure without reclassifying it. */
int print_yvex_error(const yvex_error *err, int exit_code)
{
    yvex_cli_out_writef(stderr, "yvex: %s: %s\n", yvex_error_where(err),
                        yvex_error_message(err));
    return exit_code;
}

/* Purpose: map a domain status onto the stable process-exit contract.
 * Inputs: one domain status code.
 * Effects: none.
 * Failure: unknown failures map to the generic nonzero exit code.
 * Boundary: process status mapping does not alter domain classification. */
int exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

/* Purpose: parse one complete unsigned integer without accepting signs or suffixes.
 * Inputs: borrowed text and caller-owned result storage.
 * Effects: publishes the value only after complete validation.
 * Failure: returns false and leaves result ownership with the caller.
 * Boundary: lexical parsing does not apply command-specific limits. */
int parse_ull_allow_zero(const char *text, unsigned long long *out)
{
    char *end = NULL;
    unsigned long long value;

    if (!text || !out || text[0] == '\0' || text[0] == '-') return 0;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0') return 0;
    *out = value;
    return 1;
}

/* Purpose: parse one strictly positive unsigned integer.
 * Inputs: borrowed text and caller-owned result storage.
 * Effects: publishes a validated nonzero value.
 * Failure: returns false for zero or malformed input.
 * Boundary: delegates lexical parsing but owns the positive-value invariant. */
int parse_positive_ull(const char *text, unsigned long long *out)
{
    return parse_ull_allow_zero(text, out) && *out != 0;
}

/* Purpose: parse one bounded unsigned C integer without accepting suffixes.
 * Inputs: borrowed text and caller-owned result storage.
 * Effects: publishes only a value representable by unsigned int.
 * Failure: returns false for malformed, signed, or overflowing input.
 * Boundary: performs representation validation only. */
int parse_uint_allow_zero(const char *text, unsigned int *out)
{
    unsigned long long value;

    if (!out || !parse_ull_allow_zero(text, &value) || value > UINT32_MAX) return 0;
    *out = (unsigned int)value;
    return 1;
}

/* Purpose: render arbitrary token bytes without allowing control bytes onto the terminal.
 * Inputs: borrowed byte span and exact length.
 * Effects: writes a quoted escaped representation to stdout.
 * Failure: stream failure remains visible through stdout state.
 * Boundary: escaping changes presentation only, never token content. */
void print_quoted_bytes(const char *data, unsigned long long len)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "\"");
    for (i = 0; i < len; ++i) {
        unsigned char ch = (unsigned char)data[i];
        if (ch == '"' || ch == '\\') {
            yvex_cli_out_writef(stdout, "\\%c", (int)ch);
        } else if (ch == '\n') {
            yvex_cli_out_writef(stdout, "\\n");
        } else if (ch == '\r') {
            yvex_cli_out_writef(stdout, "\\r");
        } else if (ch == '\t') {
            yvex_cli_out_writef(stdout, "\\t");
        } else if (ch < 32 || ch > 126) {
            yvex_cli_out_writef(stdout, "\\x%02x", (unsigned int)ch);
        } else {
            yvex_cli_out_writef(stdout, "%c", (int)ch);
        }
    }
    yvex_cli_out_writef(stdout, "\"");
}

/* Purpose: resolve and open one artifact through the canonical registry boundary.
 * Inputs: path or alias plus caller-owned handle and typed error storage.
 * Effects: publishes one read-only artifact handle after resolution succeeds.
 * Failure: propagates resolver/open refusal and releases temporary model references.
 * Boundary: does not inspect GGUF semantics or infer model support. */
int open_artifact_for_gguf(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;
    yvex_model_ref ref;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    rc = yvex_model_ref_resolve(&ref, path, NULL, err);
    if (rc != YVEX_OK) return rc;
    options.path = ref.path;
    options.readonly = 1;
    rc = yvex_artifact_open(artifact, &options, err);
    yvex_model_ref_clear(&ref);
    return rc;
}

/* Purpose: render one tensor shape in its declared native order.
 * Inputs: borrowed dimensions and rank.
 * Effects: writes bracketed dimensions to stdout.
 * Failure: stream failure remains visible through stdout state.
 * Boundary: does not transpose or validate tensor geometry. */
void print_tensor_dims(const unsigned long long *dims, unsigned int rank)
{
    unsigned int i;

    yvex_cli_out_writef(stdout, "[");
    for (i = 0; i < rank; ++i) {
        if (i > 0) yvex_cli_out_writef(stdout, ",");
        yvex_cli_out_writef(stdout, "%llu", dims[i]);
    }
    yvex_cli_out_writef(stdout, "]");
}

/* Purpose: render one native tensor shape without introducing a second ordering policy.
 * Inputs: borrowed dimensions and rank.
 * Effects: delegates exact rendering to the canonical tensor-dimension helper.
 * Failure: stream failure remains visible through stdout state.
 * Boundary: aliases presentation, not shape semantics. */
void print_native_dims(const unsigned long long *dims, unsigned int rank)
{
    print_tensor_dims(dims, rank);
}

/* Purpose: render one token sequence through the CLI-owned stream.
 * Inputs: borrowed immutable token sequence.
 * Effects: writes token identifiers to stdout.
 * Failure: stream failure remains visible through stdout state.
 * Boundary: does not decode, sample, or mutate tokens. */
void print_token_ids(const yvex_tokens *tokens)
{
    unsigned long long i;

    yvex_cli_out_writef(stdout, "ids:");
    for (i = 0; i < tokens->len; ++i) yvex_cli_out_writef(stdout, " %u", tokens->ids[i]);
    yvex_cli_out_writef(stdout, "\n");
}

/* Purpose: parse a non-empty comma-separated token-id sequence with bounded growth.
 * Inputs: borrowed text and caller-owned result pointers.
 * Effects: allocates and publishes an exact token-id array on success.
 * Failure: rejects malformed/overflowing input and frees partial storage.
 * Boundary: owns lexical conversion and allocation, not token validity policy. */
int parse_id_list(const char *text, unsigned int **out_ids, unsigned long long *out_len)
{
    unsigned int *ids = NULL;
    unsigned long long len = 0;
    unsigned long long capacity = 0;
    const char *cursor = text;

    if (!text || !out_ids || !out_len) return 0;
    *out_ids = NULL;
    *out_len = 0;
    while (*cursor) {
        char *end = NULL;
        unsigned long value = strtoul(cursor, &end, 10);
        unsigned int *next;

        if (end == cursor || value > UINT32_MAX) goto fail;
        if (len == capacity) {
            unsigned long long next_capacity = capacity == 0 ? 8 : capacity * 2u;
            if (next_capacity > (unsigned long long)(SIZE_MAX / sizeof(*ids))) goto fail;
            next = realloc(ids, (size_t)next_capacity * sizeof(*ids));
            if (!next) goto fail;
            ids = next;
            capacity = next_capacity;
        }
        ids[len++] = (unsigned int)value;
        if (*end == ',') {
            cursor = end + 1;
        } else if (*end == '\0') {
            cursor = end;
        } else {
            goto fail;
        }
    }
    if (len == 0) goto fail;
    *out_ids = ids;
    *out_len = len;
    return 1;

fail:
    free(ids);
    return 0;
}

/* Purpose: parse an exact-rank comma-separated tensor shape.
 * Inputs: borrowed text, required rank, and fixed caller-owned dimension storage.
 * Effects: publishes positive dimensions only after complete validation.
 * Failure: rejects invalid rank, malformed fields, zero dimensions, and overflow.
 * Boundary: parses shape syntax without applying tensor-role policy. */
int parse_dims_csv(const char *text, unsigned int rank, unsigned long long dims[4])
{
    const char *cursor = text;
    char *end = NULL;
    unsigned int i;

    if (!text || !dims || rank == 0 || rank > 4u) return 0;
    memset(dims, 0, 4u * sizeof(*dims));
    for (i = 0; i < rank; ++i) {
        errno = 0;
        dims[i] = strtoull(cursor, &end, 10);
        if (errno != 0 || end == cursor || dims[i] == 0) return 0;
        if (i + 1u < rank) {
            if (*end != ',') return 0;
            cursor = end + 1;
        } else if (*end != '\0') {
            return 0;
        }
    }
    return 1;
}
