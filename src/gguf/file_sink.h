/*
 * file_sink.h - transactional file-backed quant output sink.
 *
 * Owner: TRACK.ARTIFACT.
 * Owns: destination preflight, unique temporary files, preallocation,
 *   positioned terminal writes, digest fan-out, flush, atomic publication,
 *   progress facts, and failure cleanup.
 * Does not own: numeric execution, writer planning, reader validation,
 *   official-reader proof, artifact admission, materialization, or rendering.
 * Invariants: publication requires exact commit of every planned terminal;
 *   pre-existing destinations are never replaced and owned temps are removed.
 * Boundary: a finalized temporary file is not published or admitted.
 */
#ifndef YVEX_GGUF_FILE_SINK_H
#define YVEX_GGUF_FILE_SINK_H

#include "writer.h"
#include "quant_sink.h"

typedef enum {
    YVEX_GGUF_FILE_OK = 0,
    YVEX_GGUF_FILE_INVALID_ARGUMENT,
    YVEX_GGUF_FILE_UNSAFE_DESTINATION,
    YVEX_GGUF_FILE_DESTINATION_EXISTS,
    YVEX_GGUF_FILE_DIRECTORY_OPEN,
    YVEX_GGUF_FILE_TEMP_CREATE,
    YVEX_GGUF_FILE_INSUFFICIENT_SPACE,
    YVEX_GGUF_FILE_PREALLOCATE,
    YVEX_GGUF_FILE_WRITE,
    YVEX_GGUF_FILE_TERMINAL_PROTOCOL,
    YVEX_GGUF_FILE_TERMINAL_ABORT,
    YVEX_GGUF_FILE_INCOMPLETE,
    YVEX_GGUF_FILE_EXECUTION_IDENTITY,
    YVEX_GGUF_FILE_FLUSH,
    YVEX_GGUF_FILE_SNAPSHOT_DRIFT,
    YVEX_GGUF_FILE_VALIDATION_REQUIRED,
    YVEX_GGUF_FILE_PUBLICATION,
    YVEX_GGUF_FILE_DIRECTORY_FLUSH,
    YVEX_GGUF_FILE_CLEANUP,
    YVEX_GGUF_FILE_ALLOCATION,
    YVEX_GGUF_FILE_CANCELLED
} yvex_gguf_file_code;

typedef struct {
    yvex_gguf_file_code code;
    int system_error;
    unsigned long long terminal_ordinal;
    unsigned long long expected;
    unsigned long long actual;
    unsigned long long file_offset;
} yvex_gguf_file_failure;

typedef struct {
    const char *destination_path;
    unsigned long long safety_margin_bytes;
    unsigned long long injected_write_failure_call;
    unsigned long long injected_write_eintr_call;
    size_t injected_write_max_bytes;
    int inject_temp_create_failure;
    int inject_preallocate_failure;
    int inject_fsync_failure;
    int inject_publish_failure;
    int inject_directory_fsync_failure;
} yvex_gguf_file_sink_options;

typedef struct {
    unsigned long long planned_file_bytes;
    unsigned long long available_bytes;
    unsigned long long required_safe_bytes;
    unsigned long long prefix_bytes_written;
    unsigned long long encoded_bytes_written;
    unsigned long long physical_write_bytes;
    unsigned long long source_ranges_committed;
    unsigned long long source_bytes_committed;
    unsigned long long write_calls;
    unsigned long long committed_terminals;
    unsigned long long aborted_terminals;
    unsigned long long output_chunks;
    unsigned long long peak_owned_bytes;
    unsigned long long file_device;
    unsigned long long file_inode;
    unsigned long long file_size;
    long long validated_mtime_seconds;
    long long validated_mtime_nanoseconds;
    long long validated_ctime_seconds;
    long long validated_ctime_nanoseconds;
    long long published_mtime_seconds;
    long long published_mtime_nanoseconds;
    long long published_ctime_seconds;
    long long published_ctime_nanoseconds;
    char execution_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    char temporary_path[YVEX_ARTIFACT_PATH_CAP];
    char published_path[YVEX_ARTIFACT_PATH_CAP];
    int preallocated;
    int finalized;
    int published;
} yvex_gguf_file_sink_summary;

typedef struct yvex_gguf_file_sink yvex_gguf_file_sink;
typedef struct yvex_gguf_roundtrip_summary yvex_gguf_roundtrip_summary;

void yvex_gguf_file_sink_options_default(
    yvex_gguf_file_sink_options *options);
int yvex_gguf_file_sink_create(
    yvex_gguf_file_sink **out,
    const yvex_gguf_writer_plan *writer_plan,
    const yvex_quant_plan *quant_plan,
    const yvex_gguf_file_sink_options *options,
    yvex_gguf_file_failure *failure,
    yvex_error *err);
void yvex_gguf_file_sink_adapter(
    yvex_gguf_file_sink *sink,
    yvex_quant_output_sink *out);
int yvex_gguf_file_sink_finalize(
    yvex_gguf_file_sink *sink,
    yvex_gguf_file_sink_summary *out,
    yvex_gguf_file_failure *failure,
    yvex_error *err);
int yvex_gguf_file_sink_publish(
    yvex_gguf_file_sink *sink,
    const yvex_gguf_roundtrip_summary *roundtrip,
    yvex_gguf_file_sink_summary *out,
    yvex_gguf_file_failure *failure,
    yvex_error *err);
int yvex_gguf_file_sink_withdraw(
    yvex_gguf_file_sink *sink,
    yvex_gguf_file_failure *failure,
    yvex_error *err);
yvex_quant_digest_sink *yvex_gguf_file_sink_digest(
    yvex_gguf_file_sink *sink);
const char *yvex_gguf_file_sink_temporary_path(
    const yvex_gguf_file_sink *sink);
int yvex_gguf_file_sink_summary_get(
    yvex_gguf_file_sink *sink,
    yvex_gguf_file_sink_summary *out);
void yvex_gguf_file_sink_release(yvex_gguf_file_sink **sink);
const char *yvex_gguf_file_code_name(yvex_gguf_file_code code);

#endif
