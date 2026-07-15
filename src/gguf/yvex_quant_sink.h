/*
 * yvex_quant_sink.h - private transactional quantized-output sink ABI.
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

#include "yvex_quant_plan.h"

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

#endif
