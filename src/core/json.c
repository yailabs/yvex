/* Owner: core.json
 * Owns: bounded file reads and structural JSON token decoding.
 * Does not own: document schemas, trust decisions, rendering, or JSON serialization.
 * Invariants: reads, recursion, copied keys, and allocated strings have explicit caps.
 * Boundary: consumers assign meaning only after this owner accepts the byte syntax.
 * Purpose: eliminate independent JSON cursors in source and quantization owners.
 * Inputs: immutable byte spans or regular-file paths plus caller-selected limits.
 * Effects: advances cursors and allocates only complete file or string values.
 * Failure: malformed or oversized input returns failure with no partial publication. */
#include <yvex/internal/core.h>

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define YVEX_JSON_DEPTH_CAP 64u

/* Purpose: initialize a cursor over one immutable byte span without allocation. */
/* Purpose: Construct the owned json init state (`yvex_json_init`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_json_init(yvex_json *json, const char *data, size_t length)
{
    if (!json) return;
    json->cursor = data;
    json->end = data ? data + length : data;
    json->depth = 0u;
}

/* Allocates and reads one metadata file under an explicit byte cap. */
/* Purpose: Transfer bounded read bounded file data (`yvex_read_bounded_file`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
char *yvex_read_bounded_file(const char *path,
                             size_t cap,
                             size_t *length,
                             yvex_error *err)
{
    struct stat st;
    FILE *fp;
    char *data;
    size_t wanted;
    int read_failed;

    if (length) *length = 0u;
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
        return NULL;
    }
    if ((unsigned long long)st.st_size > (unsigned long long)cap) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "source_metadata_read",
                        "metadata file exceeds bounded read limit: %s", path);
        return NULL;
    }
    wanted = (size_t)st.st_size;
    data = (char *)malloc(wanted + 1u);
    if (!data) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "source_metadata_read",
                       "metadata buffer allocation failed");
        return NULL;
    }
    fp = fopen(path, "rb");
    if (!fp) {
        free(data);
        yvex_error_setf(err, YVEX_ERR_IO, "source_metadata_read",
                        "cannot read metadata file: %s", path);
        return NULL;
    }
    read_failed = (wanted && fread(data, 1u, wanted, fp) != wanted) || ferror(fp);
    if (fclose(fp) != 0) read_failed = 1;
    if (read_failed) {
        free(data);
        yvex_error_setf(err, YVEX_ERR_IO, "source_metadata_read",
                        "cannot read metadata file: %s", path);
        return NULL;
    }
    data[wanted] = '\0';
    if (length) *length = wanted;
    return data;
}

/* Purpose: Compute json space for its core invariant (`yvex_json_space`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
void yvex_json_space(yvex_json *json)
{
    while (json && json->cursor < json->end &&
           isspace((unsigned char)*json->cursor)) {
        json->cursor++;
    }
}

/* Purpose: Compute source json hex for its core invariant (`source_json_hex`). */
static int source_json_hex(char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

/* Consumes one string and optionally copies its decoded bounded value. */
/* Purpose: Compute json string for its core invariant (`yvex_json_string`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_string(yvex_json *json, char *out, size_t cap)
{
    size_t length = 0u;

    yvex_json_space(json);
    if (!json || json->cursor >= json->end || *json->cursor++ != '"') return 0;
    while (json->cursor < json->end) {
        unsigned char ch = (unsigned char)*json->cursor++;
        if (ch == '"') {
            if (out && cap) out[length < cap ? length : cap - 1u] = '\0';
            return !out || length < cap;
        }
        if (ch < 0x20u) return 0;
        if (ch == '\\') {
            unsigned char escaped;
            if (json->cursor >= json->end) return 0;
            escaped = (unsigned char)*json->cursor++;
            if (escaped == 'u') {
                unsigned int i;
                for (i = 0; i < 4u; ++i) {
                    if (json->cursor >= json->end ||
                        !source_json_hex(*json->cursor++)) return 0;
                }
                ch = '?';
            } else {
                switch (escaped) {
                case '"': ch = '"'; break;
                case '\\': ch = '\\'; break;
                case '/': ch = '/'; break;
                case 'b': ch = '\b'; break;
                case 'f': ch = '\f'; break;
                case 'n': ch = '\n'; break;
                case 'r': ch = '\r'; break;
                case 't': ch = '\t'; break;
                default: return 0;
                }
            }
        }
        if (out && length + 1u < cap) out[length] = (char)ch;
        length++;
    }
    return 0;
}

/* Purpose: decode one JSON string into newly allocated, exactly sized storage.
 * Inputs: Borrowed typed facts.
 * Effects: Mutates only declared core state.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
char *yvex_json_string_dup(yvex_json *json, size_t cap)
{
    yvex_json original;
    yvex_json probe;
    const char *start;
    char *value;
    size_t encoded;

    if (!json || cap == 0u) return NULL;
    original = *json;
    probe = *json;
    yvex_json_space(&probe);
    start = probe.cursor;
    if (!yvex_json_string(&probe, NULL, 0u)) return NULL;
    encoded = (size_t)(probe.cursor - start);
    if (encoded < 2u || encoded - 1u > cap) return NULL;
    value = (char *)malloc(encoded);
    if (!value) return NULL;
    if (!yvex_json_string(json, value, encoded)) {
        *json = original;
        free(value);
        return NULL;
    }
    return value;
}

/* Skips one recursively bounded array without materializing its values. */
/* Purpose: Compute json skip array for its core invariant (`json_skip_array`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
static int json_skip_array(yvex_json *json)
{
    yvex_json_space(json);
    if (!json || json->cursor >= json->end || *json->cursor++ != '[' ||
        json->depth >= YVEX_JSON_DEPTH_CAP) return 0;
    json->depth++;
    yvex_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!yvex_json_skip_value(json)) return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            json->depth--;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Skips one recursively bounded object and rejects malformed member syntax. */
/* Purpose: Compute json skip object for its core invariant (`json_skip_object`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
static int json_skip_object(yvex_json *json)
{
    char key[YVEX_JSON_KEY_CAP];

    yvex_json_space(json);
    if (!json || json->cursor >= json->end || *json->cursor++ != '{' ||
        json->depth >= YVEX_JSON_DEPTH_CAP) return 0;
    json->depth++;
    yvex_json_space(json);
    if (json->cursor < json->end && *json->cursor == '}') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!yvex_json_string(json, key, sizeof(key))) return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (!yvex_json_skip_value(json)) return 0;
        yvex_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == '}') {
            json->cursor++;
            json->depth--;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}

/* Consumes one JSON number grammar without converting its value. */
/* Purpose: Compute source json skip number for its core invariant (`source_json_skip_number`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
static int source_json_skip_number(yvex_json *json)
{
    const char *start;

    yvex_json_space(json);
    if (!json) return 0;
    start = json->cursor;
    if (json->cursor < json->end && *json->cursor == '-') json->cursor++;
    if (json->cursor >= json->end) return 0;
    if (*json->cursor == '0') {
        json->cursor++;
    } else {
        if (!isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    if (json->cursor < json->end && *json->cursor == '.') {
        json->cursor++;
        if (json->cursor >= json->end ||
            !isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    if (json->cursor < json->end &&
        (*json->cursor == 'e' || *json->cursor == 'E')) {
        json->cursor++;
        if (json->cursor < json->end &&
            (*json->cursor == '+' || *json->cursor == '-')) json->cursor++;
        if (json->cursor >= json->end ||
            !isdigit((unsigned char)*json->cursor)) return 0;
        while (json->cursor < json->end &&
               isdigit((unsigned char)*json->cursor)) json->cursor++;
    }
    return json->cursor > start;
}

/* Purpose: Compute source json literal for its core invariant (`source_json_literal`). */
static int source_json_literal(yvex_json *json, const char *literal)
{
    size_t length = strlen(literal);

    yvex_json_space(json);
    if (!json || (size_t)(json->end - json->cursor) < length ||
        memcmp(json->cursor, literal, length) != 0) return 0;
    json->cursor += length;
    return 1;
}

/* Dispatches one complete JSON value to its bounded structural parser. */
/* Purpose: Compute json skip value for its core invariant (`yvex_json_skip_value`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_skip_value(yvex_json *json)
{
    yvex_json_space(json);
    if (!json || json->cursor >= json->end) return 0;
    if (*json->cursor == '{') return json_skip_object(json);
    if (*json->cursor == '[') return json_skip_array(json);
    if (*json->cursor == '"') return yvex_json_string(json, NULL, 0u);
    if (*json->cursor == 't') return source_json_literal(json, "true");
    if (*json->cursor == 'f') return source_json_literal(json, "false");
    if (*json->cursor == 'n') return source_json_literal(json, "null");
    return source_json_skip_number(json);
}

/* Purpose: Compute json complete for its core invariant (`yvex_json_complete`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_complete(yvex_json *json)
{
    yvex_json_space(json);
    return json && json->cursor == json->end && json->depth == 0u;
}

/* Parses an unsigned decimal integer with checked overflow and no coercion. */
/* Purpose: Compute json u64 for its core invariant (`yvex_json_u64`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_u64(yvex_json *json, unsigned long long *out)
{
    unsigned long long value = 0u;

    yvex_json_space(json);
    if (!json || !out || json->cursor >= json->end ||
        !isdigit((unsigned char)*json->cursor)) return 0;
    while (json->cursor < json->end &&
           isdigit((unsigned char)*json->cursor)) {
        unsigned int digit = (unsigned int)(*json->cursor++ - '0');
        if (value > (ULLONG_MAX - digit) / 10u) return 0;
        value = value * 10u + digit;
    }
    *out = value;
    return 1;
}

/* Parses an exact JSON boolean into caller-owned storage. */
/* Purpose: Compute json bool for its core invariant (`yvex_json_bool`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_bool(yvex_json *json, int *out)
{
    if (!out) return 0;
    if (source_json_literal(json, "true")) {
        *out = 1;
        return 1;
    }
    if (source_json_literal(json, "false")) {
        *out = 0;
        return 1;
    }
    return 0;
}

/* Preserves one syntactically valid JSON number as bounded source text. */
/* Purpose: Compute json number text for its core invariant (`yvex_json_number_text`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_number_text(yvex_json *json, char *out, size_t cap)
{
    const char *start;
    size_t length;

    if (!json || !out || cap == 0u) return 0;
    yvex_json_space(json);
    start = json->cursor;
    if (!source_json_skip_number(json)) return 0;
    length = (size_t)(json->cursor - start);
    if (length == 0u || length >= cap) return 0;
    memcpy(out, start, length);
    out[length] = '\0';
    return 1;
}

/* Parses a bounded array of unsigned integers without allocation. */
/* Purpose: Compute json u64 array for its core invariant (`yvex_json_u64_array`).
 * Inputs: Borrowed typed facts.
 * Effects: Mutates declared CLI state only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: Core mechanism only. */
int yvex_json_u64_array(yvex_json *json,
                               unsigned long long *values,
                               size_t cap,
                               unsigned long long *count)
{
    unsigned long long used = 0u;

    if (!json || !values || !count || cap == 0u) return 0;
    yvex_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    yvex_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (used >= (unsigned long long)cap ||
            !yvex_json_u64(json, &values[used])) return 0;
        used++;
        yvex_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            *count = used;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}
