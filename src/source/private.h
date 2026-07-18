/*
 * private.h - private source cell shared structs.
 *
 * Owner: src/source.
 * Owns: source-cell private manifest and native inventory structures.
 * Does not own: CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: private structs stay inside the source cell and preserve header-only source boundaries.
 * Boundary: private source facts are not source verification or runtime readiness.
 */
#ifndef YVEX_SOURCE_PRIVATE_H
#define YVEX_SOURCE_PRIVATE_H

#include "payload.h"
#include "verify.h"
#include "src/core/shard_index.h"

#include <pthread.h>
#include <stddef.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <yvex/error.h>
#include <yvex/native_weights.h>
#include <yvex/source_manifest.h>

typedef struct {
    char *path;
    unsigned long long size_bytes;
    const char *kind;
} yvex_source_manifest_file;

typedef struct {
    yvex_source_manifest_file *items;
    size_t count;
    size_t cap;
    yvex_source_manifest_summary summary;
} yvex_source_manifest_file_list;

struct yvex_native_weight_table {
    yvex_native_weight_info *items;
    unsigned long long count;
    unsigned long long cap;
    yvex_native_weight_summary summary;
    unsigned long long header_read_count;
    unsigned long long header_error_count;
    unsigned long long header_bytes;
    unsigned long long *name_slots;
    size_t name_slot_count;
    unsigned long long lookup_count;
    unsigned long long collision_count;
    unsigned long long maximum_probe;
    int finalized;
};

void yvex_source_manifest_file_list_init(yvex_source_manifest_file_list *list);
void yvex_source_manifest_file_list_free(yvex_source_manifest_file_list *list);

int yvex_source_manifest_scan_files(const char *local_path,
                                    int include_files,
                                    yvex_source_manifest_file_list *out,
                                    yvex_error *err);

int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err);

int yvex_native_weight_table_add(yvex_native_weight_table *table,
                                 const char *name,
                                 const char *shard_path,
                                 const char *dtype_name,
                                 unsigned int rank,
                                 const unsigned long long *dims,
                                 unsigned long long data_start,
                                 unsigned long long data_end,
                                 yvex_error *err);

int yvex_native_weight_table_finalize(yvex_native_weight_table *table,
                                      yvex_error *err);

int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err);

typedef struct {
    unsigned long long file_bytes;
    unsigned long long header_json_bytes;
    unsigned long long data_region_offset;
    unsigned long long payload_bytes;
} yvex_safetensors_file_facts;

int yvex_safetensors_read_header_file_with_facts(
    const char *abs_path,
    const char *shard_path,
    yvex_native_weight_table *table,
    yvex_safetensors_file_facts *facts,
    yvex_error *err);

int yvex_safetensors_parse_header(const char *json,
                                  unsigned long long payload_bytes,
                                  const char *shard_path,
                                  yvex_native_weight_table *table,
                                  yvex_error *err);

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

void yvex_source_verification_add_blocker(yvex_source_verification *out,
                                          const char *reason);
int yvex_source_verification_has_blocker(
    const yvex_source_verification *out,
    const char *reason);
void yvex_source_verification_remove_blocker(yvex_source_verification *out,
                                             const char *reason);
int yvex_source_path_join(char *out,
                          size_t cap,
                          const char *left,
                          const char *right);
int yvex_source_regular_file(const char *path, unsigned long long *size);
int yvex_source_revision_is_commit(const char *text);

#endif
