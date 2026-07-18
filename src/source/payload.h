/*
 * payload.h - internal verified source payload streaming ABI.
 *
 * Owner: src/source.
 * Owns: typed payload trust, indexed ranges, bounded plans, sessions, and sinks.
 * Does not own: source parsing, tensor mapping, conversion, quantization, or output.
 * Invariants: sessions derive from exact verification plus its retained snapshot.
 * Boundary: exact source bytes are quantizer input, not an emitted artifact.
 */
#ifndef YVEX_SOURCE_PAYLOAD_H
#define YVEX_SOURCE_PAYLOAD_H

#include "inventory.h"
#include "verify.h"

#include <stddef.h>

#define YVEX_SOURCE_PAYLOAD_IDENTITY_CAP 65u
#define YVEX_SOURCE_PAYLOAD_DIGEST_CAP 65u
#define YVEX_SOURCE_PAYLOAD_DEFAULT_CHUNK_BYTES (8u * 1024u * 1024u)
#define YVEX_SOURCE_PAYLOAD_MIN_CHUNK_BYTES 4096u
#define YVEX_SOURCE_PAYLOAD_MAX_CHUNK_BYTES (64u * 1024u * 1024u)

typedef enum {
    YVEX_SOURCE_PAYLOAD_TRUST_NONE = 0,
    YVEX_SOURCE_PAYLOAD_TRUST_LOCAL_SNAPSHOT_SEALED,
    YVEX_SOURCE_PAYLOAD_TRUST_UPSTREAM_VERIFIED
} yvex_source_payload_trust_class;

typedef enum {
    YVEX_SOURCE_PAYLOAD_STATE_CONSTRUCTING = 0,
    YVEX_SOURCE_PAYLOAD_STATE_UNTRUSTED,
    YVEX_SOURCE_PAYLOAD_STATE_VERIFYING,
    YVEX_SOURCE_PAYLOAD_STATE_READY,
    YVEX_SOURCE_PAYLOAD_STATE_POISONED,
    YVEX_SOURCE_PAYLOAD_STATE_CLOSED
} yvex_source_payload_state;

typedef enum {
    YVEX_SOURCE_PAYLOAD_FAILURE_NONE = 0,
    YVEX_SOURCE_PAYLOAD_FAILURE_INVALID_ARGUMENT,
    YVEX_SOURCE_PAYLOAD_FAILURE_INVALID_STATE,
    YVEX_SOURCE_PAYLOAD_FAILURE_METADATA_NOT_VERIFIED,
    YVEX_SOURCE_PAYLOAD_FAILURE_PAYLOAD_NOT_TRUSTED,
    YVEX_SOURCE_PAYLOAD_FAILURE_MANIFEST_VERSION_UNSUPPORTED,
    YVEX_SOURCE_PAYLOAD_FAILURE_SOURCE_IDENTITY_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_PAYLOAD_IDENTITY_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_MAPPING_IDENTITY_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_DUPLICATE_SHARD,
    YVEX_SOURCE_PAYLOAD_FAILURE_DUPLICATE_TENSOR,
    YVEX_SOURCE_PAYLOAD_FAILURE_SHARD_NOT_INDEXED,
    YVEX_SOURCE_PAYLOAD_FAILURE_TENSOR_NOT_INDEXED,
    YVEX_SOURCE_PAYLOAD_FAILURE_PATH_ESCAPE,
    YVEX_SOURCE_PAYLOAD_FAILURE_SYMLINK_REFUSED,
    YVEX_SOURCE_PAYLOAD_FAILURE_SHARD_OPEN,
    YVEX_SOURCE_PAYLOAD_FAILURE_NON_REGULAR_SHARD,
    YVEX_SOURCE_PAYLOAD_FAILURE_STAT,
    YVEX_SOURCE_PAYLOAD_FAILURE_SHARD_SIZE_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_SHARD_DRIFT,
    YVEX_SOURCE_PAYLOAD_FAILURE_EXPECTED_DIGEST_UNAVAILABLE,
    YVEX_SOURCE_PAYLOAD_FAILURE_DIGEST_ALGORITHM_UNSUPPORTED,
    YVEX_SOURCE_PAYLOAD_FAILURE_DIGEST_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_RANGE_OVERFLOW,
    YVEX_SOURCE_PAYLOAD_FAILURE_RANGE_OUTSIDE_DATA_REGION,
    YVEX_SOURCE_PAYLOAD_FAILURE_RANGE_OUTSIDE_FILE,
    YVEX_SOURCE_PAYLOAD_FAILURE_TENSOR_LENGTH_MISMATCH,
    YVEX_SOURCE_PAYLOAD_FAILURE_INVALID_CHUNK,
    YVEX_SOURCE_PAYLOAD_FAILURE_RESOURCE_BUDGET,
    YVEX_SOURCE_PAYLOAD_FAILURE_ALLOCATION,
    YVEX_SOURCE_PAYLOAD_FAILURE_HANDLE_CACHE_EXHAUSTED,
    YVEX_SOURCE_PAYLOAD_FAILURE_SHORT_READ,
    YVEX_SOURCE_PAYLOAD_FAILURE_IO,
    YVEX_SOURCE_PAYLOAD_FAILURE_CONSUMER,
    YVEX_SOURCE_PAYLOAD_FAILURE_CANCELLED,
    YVEX_SOURCE_PAYLOAD_FAILURE_CLOSE_BUSY,
    YVEX_SOURCE_PAYLOAD_FAILURE_CLEANUP
} yvex_source_payload_failure_code;

typedef struct {
    yvex_source_payload_failure_code code;
    unsigned long long shard_index;
    unsigned long long tensor_index;
    unsigned long long requested_bytes;
    unsigned long long delivered_bytes;
    int system_error;
} yvex_source_payload_failure;

typedef struct {
    unsigned long long maximum_shards;
    unsigned long long maximum_tensors;
    unsigned long long maximum_plan_chunks;
    unsigned int maximum_open_handles;
    unsigned int maximum_streams;
    size_t chunk_bytes;
    size_t page_bytes;
    size_t maximum_inflight_host_bytes;
    int allow_local_snapshot_seal;
} yvex_source_payload_budget;

typedef struct {
    unsigned long long canonical_id;
    const char *canonical_name;
    unsigned long long file_bytes;
    unsigned long long data_region_offset;
    unsigned long long payload_bytes;
    const char *digest_algorithm;
    const char *digest_authority;
    const char *expected_digest;
    const char *observed_digest;
    yvex_source_payload_trust_class trust_class;
} yvex_source_payload_shard;

typedef struct {
    unsigned long long source_tensor_index;
    const char *source_tensor_name;
    unsigned long long shard_index;
    unsigned long long data_region_offset;
    unsigned long long relative_begin;
    unsigned long long relative_end;
    unsigned long long absolute_begin;
    unsigned long long absolute_end;
    unsigned long long byte_length;
    yvex_native_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS];
    unsigned long long source_snapshot_identity;
    const char *payload_identity;
    yvex_source_payload_trust_class trust_class;
} yvex_source_payload_range;

typedef struct {
    unsigned long long ordinal;
    unsigned long long range_ordinal;
    unsigned long long source_tensor_index;
    const char *source_tensor_name;
    unsigned long long shard_index;
    unsigned long long logical_offset;
    unsigned long long absolute_offset;
    size_t byte_length;
    unsigned long long first_page;
    unsigned long long last_page;
} yvex_source_payload_chunk;

typedef struct yvex_source_payload_session yvex_source_payload_session;
typedef struct yvex_source_payload_plan yvex_source_payload_plan;

typedef struct {
    const yvex_source_verify_options *verification_options;
    const yvex_source_verification *verification;
    yvex_source_tensor_snapshot *snapshot;
    yvex_source_payload_budget budget;
    const char *manifest_path;
} yvex_source_payload_open_options;

typedef struct {
    unsigned long long range_count;
    unsigned long long chunk_count;
    unsigned long long logical_bytes;
    unsigned long long page_count;
    size_t chunk_bytes;
    size_t page_bytes;
} yvex_source_payload_plan_summary;

typedef struct {
    unsigned long long requested_logical_bytes;
    unsigned long long delivered_logical_bytes;
    unsigned long long physical_bytes_read;
    unsigned long long trust_bytes_read;
    unsigned long long chunks_attempted;
    unsigned long long chunks_completed;
    int complete;
    int committed;
    int aborted;
} yvex_source_payload_stream_result;

typedef struct {
    int (*begin)(void *context, const yvex_source_payload_plan_summary *summary);
    int (*chunk)(void *context,
                 const yvex_source_payload_chunk *chunk,
                 const unsigned char *bytes);
    int (*commit)(void *context,
                  const yvex_source_payload_stream_result *result);
    void (*abort)(void *context,
                  const yvex_source_payload_failure *failure,
                  const yvex_source_payload_stream_result *result);
    void *context;
} yvex_source_payload_sink;

typedef struct {
    yvex_source_payload_state state;
    yvex_source_payload_trust_class trust_class;
    unsigned long long source_snapshot_identity;
    char admitted_payload_identity[YVEX_SOURCE_PAYLOAD_IDENTITY_CAP];
    char payload_identity[YVEX_SOURCE_PAYLOAD_IDENTITY_CAP];
    unsigned long long shard_count;
    unsigned long long trusted_shard_count;
    unsigned long long tensor_count;
    unsigned long long logical_tensor_bytes;
    unsigned long long handle_cache_hits;
    unsigned long long handle_cache_misses;
    unsigned long long handle_opens;
    unsigned long long handle_reopens;
    unsigned long long handle_evictions;
    unsigned long long short_reads;
    unsigned long long digest_mismatches;
    unsigned long long identity_drifts;
    unsigned long long cancellations;
    unsigned long long failed_streams;
    unsigned long long active_streams;
    unsigned long long active_plans;
    unsigned long long peak_active_streams;
    unsigned long long peak_inflight_host_bytes;
    unsigned long long buffer_allocations;
    unsigned long long buffer_reuses;
    unsigned long long buffer_releases;
    unsigned int open_handles;
    unsigned int peak_open_handles;
    unsigned long long range_lookup_count;
    unsigned long long shard_lookup_count;
    unsigned long long header_scan_count;
} yvex_source_payload_session_facts;

typedef enum {
    YVEX_SOURCE_PAYLOAD_PROBE_COLD_ADVISORY = 0,
    YVEX_SOURCE_PAYLOAD_PROBE_WARM,
    YVEX_SOURCE_PAYLOAD_PROBE_REPEATED,
    YVEX_SOURCE_PAYLOAD_PROBE_STAGED
} yvex_source_payload_probe_mode;

typedef struct {
    yvex_source_payload_probe_mode mode;
    unsigned int repetitions;
    int page_cache_advice_requested;
    int page_cache_advice_accepted;
    unsigned long long elapsed_nanoseconds;
    unsigned long long logical_bytes;
    unsigned long long physical_bytes;
    unsigned long long chunks;
    unsigned long long handle_hits;
    unsigned long long handle_misses;
    unsigned long long buffer_reuses;
} yvex_source_payload_probe_result;

void yvex_source_payload_budget_default(yvex_source_payload_budget *budget);
const char *yvex_source_payload_trust_class_name(
    yvex_source_payload_trust_class trust_class);
const char *yvex_source_payload_failure_name(
    yvex_source_payload_failure_code code);

int yvex_source_payload_session_open(
    yvex_source_payload_session **out,
    const yvex_source_payload_open_options *options,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_verify(
    yvex_source_payload_session *session,
    const yvex_source_payload_plan *delivery_plan,
    const yvex_source_payload_sink *sink,
    yvex_source_payload_stream_result *result,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_stream(
    yvex_source_payload_session *session,
    const yvex_source_payload_plan *plan,
    const yvex_source_payload_sink *sink,
    yvex_source_payload_stream_result *result,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_cancel(
    yvex_source_payload_session *session,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_reset_cancel(
    yvex_source_payload_session *session,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_close(
    yvex_source_payload_session *session,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_release(
    yvex_source_payload_session **session,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_session_facts_get(
    const yvex_source_payload_session *session,
    yvex_source_payload_session_facts *out,
    yvex_error *err);

const yvex_source_payload_shard *yvex_source_payload_shard_at(
    yvex_source_payload_session *session,
    unsigned long long index);
const yvex_source_payload_shard *yvex_source_payload_shard_find(
    yvex_source_payload_session *session,
    const char *canonical_name);
const yvex_source_payload_range *yvex_source_payload_range_at(
    yvex_source_payload_session *session,
    unsigned long long tensor_index);
const yvex_source_payload_range *yvex_source_payload_range_find(
    yvex_source_payload_session *session,
    const char *tensor_name);

int yvex_source_payload_plan_build(
    yvex_source_payload_plan **out,
    yvex_source_payload_session *session,
    const unsigned long long *tensor_indices,
    unsigned long long tensor_count,
    size_t chunk_bytes,
    size_t page_bytes,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_plan_build_all(
    yvex_source_payload_plan **out,
    yvex_source_payload_session *session,
    size_t chunk_bytes,
    size_t page_bytes,
    yvex_source_payload_failure *failure,
    yvex_error *err);
const yvex_source_payload_plan_summary *yvex_source_payload_plan_summary_get(
    const yvex_source_payload_plan *plan);
const yvex_source_payload_range *yvex_source_payload_plan_range_at(
    const yvex_source_payload_plan *plan,
    unsigned long long ordinal);
const yvex_source_payload_chunk *yvex_source_payload_plan_chunk_at(
    const yvex_source_payload_plan *plan,
    unsigned long long ordinal);
void yvex_source_payload_plan_close(yvex_source_payload_plan *plan);

int yvex_source_payload_probe(
    yvex_source_payload_session *session,
    const yvex_source_payload_plan *plan,
    yvex_source_payload_probe_mode mode,
    unsigned int repetitions,
    yvex_source_payload_probe_result *out,
    yvex_source_payload_failure *failure,
    yvex_error *err);

#endif
