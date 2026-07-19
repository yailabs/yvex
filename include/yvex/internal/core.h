/* Owner: core.internal (core).
 * Owns: checked scalar/string helpers, bounded JSON, canonical SHA-256 fields, deterministic index hashing, and
 *   shard indexing.
 * Does not own: domain policy, rendering, or subsystem lifecycle.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: cross-subsystem core algorithms and operator projections.
 * Purpose: provide the canonical cross-subsystem core algorithms and operator projections contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_CORE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_CORE_H_INCLUDED

#include <stddef.h>
#include <stdint.h>
#include <yvex/core.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Json contract. */
#define YVEX_JSON_KEY_CAP 1024u
typedef struct {
    const char *cursor;
    const char *end;
    unsigned int depth;
} yvex_json;
void yvex_json_init(yvex_json *json, const char *data, size_t length);
char *yvex_read_bounded_file(const char *path,
                             size_t cap,
                             size_t *length,
                             yvex_error *err);
void yvex_json_space(yvex_json *json);
int yvex_json_string(yvex_json *json, char *out, size_t cap);
char *yvex_json_string_dup(yvex_json *json, size_t cap);
int yvex_json_skip_value(yvex_json *json);
int yvex_json_complete(yvex_json *json);
int yvex_json_u64(yvex_json *json, unsigned long long *out);
int yvex_json_bool(yvex_json *json, int *out);
int yvex_json_number_text(yvex_json *json, char *out, size_t cap);
int yvex_json_u64_array(yvex_json *json,
                        unsigned long long *values,
                        size_t cap,
                        unsigned long long *count);

/* Sha256 contract. */
#define YVEX_SHA256_DIGEST_BYTES 32u
#define YVEX_SHA256_HEX_BYTES 65u
typedef struct {
    uint32_t state[8];
    uint64_t length;
    unsigned char block[64];
    size_t used;
    int finalized;
} yvex_sha256;
void yvex_sha256_init(yvex_sha256 *context);
int yvex_sha256_update(yvex_sha256 *context, const void *data, size_t length);
int yvex_sha256_final(yvex_sha256 *context,
                      unsigned char digest[YVEX_SHA256_DIGEST_BYTES]);
void yvex_sha256_hex(const unsigned char digest[YVEX_SHA256_DIGEST_BYTES],
                     char output[YVEX_SHA256_HEX_BYTES]);
int yvex_sha256_hex_valid(const char *text);
int yvex_sha256_update_u64(yvex_sha256 *context, unsigned long long value);
int yvex_sha256_update_text(yvex_sha256 *context, const char *text);

/* Process-local deterministic helpers shared by diagnostic owners. */
int yvex_core_test_flag(const char *name);
char *yvex_core_strdup(const char *text);
unsigned long long yvex_core_hash_mix_u64(unsigned long long hash,
                                          unsigned long long value);
unsigned long long yvex_core_hash_mix_bytes(unsigned long long hash,
                                            const void *data,
                                            size_t length);
unsigned long long yvex_core_index_hash(const char *text);
int yvex_core_u64_add(unsigned long long left,
                      unsigned long long right,
                      unsigned long long *out);
int yvex_core_u64_mul(unsigned long long left,
                      unsigned long long right,
                      unsigned long long *out);
int yvex_core_mkdir_parent(const char *path,
                           const char *where,
                           yvex_error *err);

/* Shard Index contract. */
typedef enum {
    YVEX_SHARD_INDEX_OK = 0,
    YVEX_SHARD_INDEX_INVALID,
    YVEX_SHARD_INDEX_BUDGET,
    YVEX_SHARD_INDEX_DUPLICATE_ID,
    YVEX_SHARD_INDEX_DUPLICATE_KEY,
    YVEX_SHARD_INDEX_NONCANONICAL_ORDER
} yvex_shard_index_result;
typedef struct {
    unsigned long long canonical_id;
    const char *canonical_key;
} yvex_shard_index_entry;
typedef struct {
    const yvex_shard_index_entry *entries;
    unsigned long long count;
} yvex_shard_index;
yvex_shard_index_result yvex_shard_index_init(
    yvex_shard_index *index,
    const yvex_shard_index_entry *entries,
    unsigned long long count,
    unsigned long long maximum_entries);
const yvex_shard_index_entry *yvex_shard_index_at(
    const yvex_shard_index *index,
    unsigned long long canonical_id);
const yvex_shard_index_entry *yvex_shard_index_find(
    const yvex_shard_index *index,
    const char *canonical_key,
    unsigned long long *comparisons);
void yvex_shard_index_reset(yvex_shard_index *index);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_CORE_H_INCLUDED */
