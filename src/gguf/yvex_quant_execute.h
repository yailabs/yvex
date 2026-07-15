/*
 * yvex_quant_execute.h - private bounded Transformation IR byte executor ABI.
 *
 * Owner: TRACK.QUANT.
 * Owns: trusted binding admission, four DeepSeek operation dispatches,
 *   bounded worker/buffer budgets, cancellation, metrics, and cleanup facts.
 * Does not own: source file IO, transform semantics, profile selection, GGUF
 *   writing, artifact publication, runtime materialization, or rendering.
 * Invariants: every source byte arrives through yvex_source_payload_session;
 *   terminal output commits only after exact encoded completion.
 * Boundary: encoded sink delivery is not an emitted artifact or runtime proof.
 */
#ifndef YVEX_QUANT_EXECUTE_H
#define YVEX_QUANT_EXECUTE_H

#include "yvex_quant_sink.h"

#include <pthread.h>
#include <stdatomic.h>

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
