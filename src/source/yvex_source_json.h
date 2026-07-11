/*
 * yvex_source_json.h - bounded source-metadata JSON reader.
 *
 * Owner: src/source.
 * Owns: allocation-bounded metadata reads and a small structural JSON cursor.
 * Does not own: source policy, manifests, model facts, shard inventory, or IO writes.
 * Invariants: recursion and file sizes are bounded; malformed input fails closed.
 * Boundary: syntactic JSON validity is not source identity verification.
 */
#ifndef YVEX_SOURCE_JSON_H
#define YVEX_SOURCE_JSON_H

#include <stddef.h>

#include <yvex/error.h>

#define YVEX_SOURCE_JSON_KEY_CAP 1024u

typedef struct {
    const char *cursor;
    const char *end;
    unsigned int depth;
} yvex_source_json;

void yvex_source_json_init(yvex_source_json *json,
                           const char *data,
                           size_t length);
char *yvex_source_read_bounded_file(const char *path,
                                    size_t cap,
                                    size_t *length,
                                    yvex_error *err);
void yvex_source_json_space(yvex_source_json *json);
int yvex_source_json_string(yvex_source_json *json, char *out, size_t cap);
int yvex_source_json_skip_array(yvex_source_json *json);
int yvex_source_json_skip_object(yvex_source_json *json);
int yvex_source_json_skip_value(yvex_source_json *json);
int yvex_source_json_complete(yvex_source_json *json);
int yvex_source_json_u64(yvex_source_json *json, unsigned long long *out);
int yvex_source_json_bool(yvex_source_json *json, int *out);
int yvex_source_json_number_text(yvex_source_json *json, char *out, size_t cap);
int yvex_source_json_u64_array(yvex_source_json *json,
                               unsigned long long *values,
                               size_t cap,
                               unsigned long long *count);

#endif
