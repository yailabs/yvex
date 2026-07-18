/*
 * quant_sink.h - private transactional quantized-output sink ABI.
 *
 * Owner: TRACK.QUANT.
 * Owns: terminal begin/chunk/commit/abort protocol and digest/discard sink.
 * Does not own: transformation execution, GGUF layout/writing, files, or CLI.
 * Invariants: chunks are borrowed only during callbacks; offsets are monotonic;
 *   callbacks for distinct terminals may run concurrently but one terminal is
 *   serialized in offset order; sink context must be concurrency-safe unless
 *   the executor has one worker; the sealed plan and callback context outlive
 *   execution/sink release; aggregate identity exists only after commit.
 * Boundary: committed digest-only output is not an artifact identity.
 */
#ifndef YVEX_QUANT_SINK_H
#define YVEX_QUANT_SINK_H

#include "quant_plan.h"

#include <pthread.h>
#include <stdatomic.h>

typedef struct {
    int (*begin_terminal)(void *context,
                          const yvex_quant_decision *decision);
    int (*deliver_chunk)(void *context,
                         const yvex_quant_decision *decision,
                         unsigned long long output_offset,
                         const unsigned char *bytes,
                         size_t byte_count);
    int (*commit_terminal)(void *context,
                           const yvex_quant_decision *decision,
                           unsigned long long delivered_bytes);
    void (*abort_terminal)(void *context,
                           const yvex_quant_decision *decision,
                           const yvex_quant_failure *failure,
                           unsigned long long delivered_bytes);
    void *context;
} yvex_quant_output_sink;

typedef struct {
    unsigned long long terminal_count;
    unsigned long long committed_terminals;
    unsigned long long aborted_terminals;
    unsigned long long output_chunks;
    unsigned long long encoded_bytes;
    unsigned long long qtype_bytes[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    char execution_identity[YVEX_QUANT_PLAN_IDENTITY_CAP];
    int complete;
} yvex_quant_digest_summary;

typedef struct {
    unsigned long long terminal_ordinal;
    unsigned int qtype;
    unsigned long long delivered_bytes;
    unsigned long long chunks;
    char sha256[YVEX_QUANT_PLAN_IDENTITY_CAP];
    int committed;
} yvex_quant_terminal_digest;

typedef struct yvex_quant_digest_sink yvex_quant_digest_sink;

typedef struct {
    yvex_quant_allocate_fn allocate;
    yvex_quant_release_fn release;
    void *context;
} yvex_quant_sink_allocator;

int yvex_quant_digest_sink_create(
    yvex_quant_digest_sink **out,
    const yvex_quant_plan *plan,
    const char *payload_identity,
    yvex_quant_failure *failure,
    yvex_error *err);
int yvex_quant_digest_sink_create_with_allocator(
    yvex_quant_digest_sink **out,
    const yvex_quant_plan *plan,
    const char *payload_identity,
    const yvex_quant_sink_allocator *allocator,
    yvex_quant_failure *failure,
    yvex_error *err);
void yvex_quant_digest_sink_release(yvex_quant_digest_sink **sink);
void yvex_quant_digest_sink_adapter(yvex_quant_digest_sink *sink,
                                    yvex_quant_output_sink *out);
int yvex_quant_digest_sink_finalize(
    yvex_quant_digest_sink *sink,
    yvex_quant_digest_summary *out,
    yvex_quant_failure *failure,
    yvex_error *err);
int yvex_quant_digest_summary_validate(
    const yvex_quant_digest_summary *summary,
    const char *expected_execution_identity,
    yvex_quant_failure *failure,
    yvex_error *err);
int yvex_quant_digest_sink_terminal_at(
    yvex_quant_digest_sink *sink,
    unsigned long long ordinal,
    yvex_quant_terminal_digest *out,
    yvex_quant_failure *failure,
    yvex_error *err);
size_t yvex_quant_digest_sink_owned_bytes(
    const yvex_quant_digest_sink *sink);

typedef struct {
    atomic_int requested;
} yvex_quant_cancellation;

typedef void *(*yvex_quant_executor_allocate_fn)(size_t size, void *context);
typedef void (*yvex_quant_executor_release_fn)(void *allocation, void *context);

typedef struct {
    unsigned int worker_count;
    size_t source_chunk_bytes;
    size_t output_chunk_bytes;
    size_t maximum_owned_bytes;
    yvex_quant_cancellation *cancellation;
    yvex_quant_executor_allocate_fn allocate;
    yvex_quant_executor_release_fn release;
    int (*thread_create)(pthread_t *thread,
                         void *(*entry)(void *),
                         void *argument,
                         void *context);
    void *context;
} yvex_quant_executor_options;

typedef struct {
    unsigned long long terminal_decisions;
    unsigned long long terminals_attempted;
    unsigned long long terminals_executed;
    unsigned long long committed_terminals;
    unsigned long long aborted_terminals;
    unsigned long long source_values_consumed;
    unsigned long long source_ranges_read;
    unsigned long long payload_bytes_read;
    unsigned long long source_chunks;
    unsigned long long output_chunks;
    unsigned long long encoded_output_bytes;
    unsigned long long reference_decode_elements;
    unsigned long long qtype_tensor_counts[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    unsigned long long qtype_output_bytes[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    unsigned long long role_tensor_counts[YVEX_TENSOR_ROLE_COUNT];
    yvex_quant_metrics qtype_metrics[
        YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID + 1u];
    yvex_quant_metrics role_metrics[YVEX_TENSOR_ROLE_COUNT];
    unsigned long long numeric_bound_violations;
    unsigned long long short_reads;
    unsigned long long payload_drifts;
    unsigned long long sink_failures;
    unsigned long long cancellations;
    unsigned long long worker_failures;
    size_t configured_memory_budget;
    size_t peak_owned_bytes;
    unsigned int configured_workers;
    unsigned int workers_started;
    int complete;
} yvex_quant_execution_summary;

void yvex_quant_cancellation_init(yvex_quant_cancellation *cancellation);
void yvex_quant_cancellation_request(yvex_quant_cancellation *cancellation);
int yvex_quant_cancellation_requested(
    const yvex_quant_cancellation *cancellation);
void yvex_quant_executor_options_default(yvex_quant_executor_options *options);

int yvex_quant_execute(
    const yvex_quant_plan *plan,
    const yvex_quant_output_sink *sink,
    const yvex_quant_executor_options *options,
    yvex_quant_execution_summary *summary,
    yvex_quant_failure *failure,
    yvex_error *err);

#endif
