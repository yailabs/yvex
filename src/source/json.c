/*
 * json.c - bounded source-metadata JSON reader.
 *
 * Owner: src/source.
 * Owns: bounded metadata-file reads and allocation-free structural JSON parsing.
 * Does not own: source verification policy, model facts, inventory, rendering, or writes.
 * Invariants: file size and JSON recursion are capped; no tensor payload is read.
 * Boundary: parsed metadata remains unverified until consumed by a domain owner.
 */
#include "json.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define YVEX_SOURCE_JSON_DEPTH_CAP 64u

void yvex_source_json_init(yvex_source_json *json,
                           const char *data,
                           size_t length)
{
    if (!json) return;
    json->cursor = data;
    json->end = data ? data + length : data;
    json->depth = 0u;
}

/* Allocates and reads one metadata file under an explicit byte cap. */
char *yvex_source_read_bounded_file(const char *path,
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

void yvex_source_json_space(yvex_source_json *json)
{
    while (json && json->cursor < json->end &&
           isspace((unsigned char)*json->cursor)) {
        json->cursor++;
    }
}

static int source_json_hex(char value)
{
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

/* Consumes one string and optionally copies its decoded bounded value. */
int yvex_source_json_string(yvex_source_json *json, char *out, size_t cap)
{
    size_t length = 0u;

    yvex_source_json_space(json);
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
            } else if (!strchr("\"\\/bfnrt", escaped)) {
                return 0;
            } else {
                ch = escaped;
            }
        }
        if (out && length + 1u < cap) out[length] = (char)ch;
        length++;
    }
    return 0;
}

/* Skips one recursively bounded array without materializing its values. */
int yvex_source_json_skip_array(yvex_source_json *json)
{
    yvex_source_json_space(json);
    if (!json || json->cursor >= json->end || *json->cursor++ != '[' ||
        json->depth >= YVEX_SOURCE_JSON_DEPTH_CAP) return 0;
    json->depth++;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!yvex_source_json_skip_value(json)) return 0;
        yvex_source_json_space(json);
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
int yvex_source_json_skip_object(yvex_source_json *json)
{
    char key[YVEX_SOURCE_JSON_KEY_CAP];

    yvex_source_json_space(json);
    if (!json || json->cursor >= json->end || *json->cursor++ != '{' ||
        json->depth >= YVEX_SOURCE_JSON_DEPTH_CAP) return 0;
    json->depth++;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == '}') {
        json->cursor++;
        json->depth--;
        return 1;
    }
    for (;;) {
        if (!yvex_source_json_string(json, key, sizeof(key))) return 0;
        yvex_source_json_space(json);
        if (json->cursor >= json->end || *json->cursor++ != ':') return 0;
        if (!yvex_source_json_skip_value(json)) return 0;
        yvex_source_json_space(json);
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
static int source_json_skip_number(yvex_source_json *json)
{
    const char *start;

    yvex_source_json_space(json);
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

static int source_json_literal(yvex_source_json *json, const char *literal)
{
    size_t length = strlen(literal);

    yvex_source_json_space(json);
    if (!json || (size_t)(json->end - json->cursor) < length ||
        memcmp(json->cursor, literal, length) != 0) return 0;
    json->cursor += length;
    return 1;
}

/* Dispatches one complete JSON value to its bounded structural parser. */
int yvex_source_json_skip_value(yvex_source_json *json)
{
    yvex_source_json_space(json);
    if (!json || json->cursor >= json->end) return 0;
    if (*json->cursor == '{') return yvex_source_json_skip_object(json);
    if (*json->cursor == '[') return yvex_source_json_skip_array(json);
    if (*json->cursor == '"') return yvex_source_json_string(json, NULL, 0u);
    if (*json->cursor == 't') return source_json_literal(json, "true");
    if (*json->cursor == 'f') return source_json_literal(json, "false");
    if (*json->cursor == 'n') return source_json_literal(json, "null");
    return source_json_skip_number(json);
}

int yvex_source_json_complete(yvex_source_json *json)
{
    yvex_source_json_space(json);
    return json && json->cursor == json->end && json->depth == 0u;
}

/* Parses an unsigned decimal integer with checked overflow and no coercion. */
int yvex_source_json_u64(yvex_source_json *json, unsigned long long *out)
{
    unsigned long long value = 0u;

    yvex_source_json_space(json);
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
int yvex_source_json_bool(yvex_source_json *json, int *out)
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
int yvex_source_json_number_text(yvex_source_json *json, char *out, size_t cap)
{
    const char *start;
    size_t length;

    if (!json || !out || cap == 0u) return 0;
    yvex_source_json_space(json);
    start = json->cursor;
    if (!source_json_skip_number(json)) return 0;
    length = (size_t)(json->cursor - start);
    if (length == 0u || length >= cap) return 0;
    memcpy(out, start, length);
    out[length] = '\0';
    return 1;
}

/* Parses a bounded array of unsigned integers without allocation. */
int yvex_source_json_u64_array(yvex_source_json *json,
                               unsigned long long *values,
                               size_t cap,
                               unsigned long long *count)
{
    unsigned long long used = 0u;

    if (!json || !values || !count || cap == 0u) return 0;
    yvex_source_json_space(json);
    if (json->cursor >= json->end || *json->cursor++ != '[') return 0;
    yvex_source_json_space(json);
    if (json->cursor < json->end && *json->cursor == ']') return 0;
    for (;;) {
        if (used >= (unsigned long long)cap ||
            !yvex_source_json_u64(json, &values[used])) return 0;
        used++;
        yvex_source_json_space(json);
        if (json->cursor >= json->end) return 0;
        if (*json->cursor == ']') {
            json->cursor++;
            *count = used;
            return 1;
        }
        if (*json->cursor++ != ',') return 0;
    }
}
