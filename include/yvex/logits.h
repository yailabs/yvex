/*
 * Owner: abi.logits (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Logits buffer skeleton
 *
 * File: include/yvex/logits.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the engine/session layer logits buffer skeleton. The object reports whether a logits
 *   buffer can be sized from the current descriptor; engine/session layer does not compute logits
 *   or expose a sampler.
 *
 * Owns:
 *   - yvex_logits
 *   - logits status and summary vocabulary
 *
 * Does not own:
 *   - output-head execution
 *   - sampler behavior
 *   - token generation
 *
 * Used by:
 *   - yvex_session
 *   - engine/session layer tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_logits
 */
#ifndef YVEX_LOGITS_H
#define YVEX_LOGITS_H

#include <yvex/decode.h>
#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_logits yvex_logits;

typedef enum {
    YVEX_LOGITS_STATUS_EMPTY = 0,
    YVEX_LOGITS_STATUS_UNAVAILABLE,
    YVEX_LOGITS_STATUS_ALLOCATED
} yvex_logits_status;

typedef struct {
    yvex_logits_status status;
    unsigned long long vocab_size;
    unsigned long long bytes;
} yvex_logits_summary;

#define YVEX_LOGITS_MAX_SAMPLE_VALUES 16u

typedef struct {
    const yvex_decode_step_options *decode_options;
    unsigned long long logits_count;
} yvex_logits_buffer_options;

typedef struct {
    int logits_buffer_created;
    const char *logits_buffer_kind;
    const char *logits_phase;
    const char *logits_source;
    const char *backend_name;
    int decode_invoked;
    int decode_state_created;
    int decode_step_executed;
    const char *decode_step_kind;
    const char *decode_phase;
    unsigned long long decode_position;
    unsigned long long decode_state_checksum;
    unsigned long long logits_count;
    unsigned long long logits_bytes;
    unsigned long long logits_checksum;
    double logits_min;
    double logits_max;
    double logits_sum;
    unsigned long long logits_sample_count;
    float logits_sample_values[YVEX_LOGITS_MAX_SAMPLE_VALUES];
    int cleanup_attempted;
    const char *cleanup_status;
    int real_model_logits;
    int real_model_output_head;
    int bounded_logits_ready;
    int logits_ready;
    int sampling_ready;
    int generation_ready;
    const char *generation_status;
} yvex_logits_buffer_summary;

int yvex_logits_count_valid(unsigned long long count);
void yvex_logits_buffer_summary_init(yvex_logits_buffer_summary *out,
                                     const yvex_logits_buffer_options *options);

int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err);
void yvex_logits_close(yvex_logits *logits);

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits);
const char *yvex_logits_status_name(yvex_logits_status status);

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err);

int yvex_engine_create_logits_buffer(yvex_engine *engine,
                                     const yvex_logits_buffer_options *options,
                                     yvex_logits_buffer_summary *out,
                                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_LOGITS_H */
