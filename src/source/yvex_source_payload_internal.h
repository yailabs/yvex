/*
 * yvex_source_payload_internal.h - private payload owner implementation state.
 *
 * Owner: src/source.
 * Owns: payload session/cache structs and per-session testable IO seams.
 * Does not own: public API, mapping policy, payload transforms, or rendering.
 * Invariants: hooks are session-local; cached descriptors are pinned before IO.
 * Boundary: implementation state is never retained by payload consumers.
 */
#ifndef YVEX_SOURCE_PAYLOAD_INTERNAL_H
#define YVEX_SOURCE_PAYLOAD_INTERNAL_H

#include "yvex_source_payload.h"
#include "yvex_shard_index.h"

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef struct {
    int (*openat_fn)(int, const char *, int);
    int (*fstat_fn)(int, struct stat *);
    int (*fstatat_fn)(int, const char *, struct stat *, int);
    ssize_t (*pread_fn)(int, void *, size_t, off_t);
    int (*close_fn)(int);
    void *(*malloc_fn)(size_t);
    void *(*calloc_fn)(size_t, size_t);
    void (*free_fn)(void *);
} yvex_source_payload_ops;

typedef struct {
    int fd;
    unsigned long long shard_index;
    unsigned int pins;
    unsigned long long use_tick;
} yvex_source_payload_handle;

typedef struct {
    unsigned char *bytes;
    size_t capacity;
    int in_use;
} yvex_source_payload_buffer;

typedef struct {
    dev_t device;
    ino_t inode;
    off_t size;
    struct timespec mtime;
    struct timespec ctime;
} yvex_source_payload_file_identity;

typedef struct {
    yvex_source_payload_shard public_fact;
    char *name;
    char expected_digest[YVEX_SOURCE_PAYLOAD_DIGEST_CAP];
    char observed_digest[YVEX_SOURCE_PAYLOAD_DIGEST_CAP];
    char digest_algorithm[24];
    char digest_authority[40];
    yvex_source_payload_file_identity admitted_identity;
} yvex_source_payload_owned_shard;

struct yvex_source_payload_session {
    pthread_mutex_t mutex;
    int mutex_initialized;
    int root_fd;
    yvex_source_payload_state state;
    int cancelled;
    yvex_source_payload_budget budget;
    yvex_source_payload_ops ops;
    yvex_source_tensor_snapshot *snapshot;
    yvex_source_verification verification;
    char target_id[128];
    char family_key[64];
    char repository_id[256];
    char *manifest_path;
    yvex_source_payload_owned_shard *shards;
    yvex_shard_index_entry *shard_index_entries;
    yvex_shard_index shard_index;
    yvex_source_payload_range *ranges;
    yvex_source_payload_handle *handles;
    yvex_source_payload_buffer *buffers;
    unsigned long long shard_count;
    unsigned long long tensor_count;
    unsigned long long logical_tensor_bytes;
    unsigned long long use_tick;
    unsigned long long active_plans;
    size_t inflight_host_bytes;
    yvex_source_payload_session_facts facts;
};

struct yvex_source_payload_plan {
    yvex_source_payload_session *session;
    yvex_source_payload_range *ranges;
    yvex_source_payload_chunk *chunks;
    yvex_source_payload_plan_summary summary;
    int registered;
};

void yvex_source_payload_default_ops(yvex_source_payload_ops *ops);
int yvex_source_payload_session_open_with_ops(
    yvex_source_payload_session **out,
    const yvex_source_payload_open_options *options,
    const yvex_source_payload_ops *ops,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_handle_acquire(
    yvex_source_payload_session *session,
    unsigned long long shard_index,
    int *fd,
    yvex_source_payload_failure *failure,
    yvex_error *err);
void yvex_source_payload_handle_release(
    yvex_source_payload_session *session,
    unsigned long long shard_index);
int yvex_source_payload_buffer_acquire(
    yvex_source_payload_session *session,
    size_t bytes,
    unsigned char **buffer,
    unsigned int *slot,
    yvex_source_payload_failure *failure,
    yvex_error *err);
void yvex_source_payload_buffer_release(
    yvex_source_payload_session *session,
    unsigned int slot);
int yvex_source_payload_exact_read(
    yvex_source_payload_session *session,
    unsigned long long shard_index,
    int fd,
    unsigned long long offset,
    unsigned char *buffer,
    size_t length,
    yvex_source_payload_stream_result *result,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_identity_compute(
    yvex_source_payload_session *session,
    yvex_source_payload_failure *failure,
    yvex_error *err);
int yvex_source_payload_manifest_publish(
    const yvex_source_payload_session *session,
    yvex_error *err);
void yvex_source_payload_fail(
    yvex_source_payload_failure *failure,
    yvex_source_payload_failure_code code,
    unsigned long long shard_index,
    unsigned long long tensor_index,
    unsigned long long requested,
    unsigned long long delivered,
    int system_error,
    yvex_error *err,
    int status,
    const char *where,
    const char *message);

#endif
