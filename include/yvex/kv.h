/*
 * YVEX - KV cache runtime boundary
 *
 * File: include/yvex/kv.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the minimal session-owned KV store. The object owns a bounded F32
 *   key/value buffer, shape accounting, append/read lifecycle, and summary
 *   reports. It does not run attention, decode, logits, sampling, or generation.
 *
 * Owns:
 *   - yvex_kv_cache
 *   - KV shape and summary/status vocabulary
 *   - minimal append/read/clear lifecycle
 *
 * Does not own:
 *   - backend allocation
 *   - attention execution
 *   - decode runtime
 *   - logits or generation
 *
 * Used by:
 *   - yvex_session
 *   - engine/session layer tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_kv
 */
#ifndef YVEX_KV_H
#define YVEX_KV_H

#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_kv_cache yvex_kv_cache;

typedef enum {
    YVEX_KV_STATUS_EMPTY = 0,
    YVEX_KV_STATUS_UNAVAILABLE,
    YVEX_KV_STATUS_PLANNED,
    YVEX_KV_STATUS_ALLOCATED
} yvex_kv_status;

typedef struct {
    unsigned long long layer_count;
    unsigned long long kv_head_count;
    unsigned long long head_dim;
    unsigned long long capacity;
} yvex_kv_shape;

typedef struct {
    yvex_kv_status status;
    const char *owner;
    const char *dtype;
    unsigned long long context_length;
    unsigned long long layer_count;
    unsigned long long kv_head_count;
    unsigned long long head_dim;
    unsigned long long values_per_position;
    unsigned long long bytes_per_position;
    unsigned long long bytes;
    unsigned long long allocated_bytes;
    unsigned long long written_positions;
    unsigned long long append_count;
    unsigned long long read_count;
    unsigned long long last_read_position;
    const char *overflow_status;
    const char *cleanup_status;
    int session_owned;
    int decode_ready;
    int logits_ready;
    int generation_ready;
} yvex_kv_summary;

int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err);
int yvex_kv_cache_create_shape(yvex_kv_cache **out,
                               const yvex_kv_shape *shape,
                               yvex_error *err);
void yvex_kv_cache_close(yvex_kv_cache *kv);

yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv);
const char *yvex_kv_status_name(yvex_kv_status status);

unsigned long long yvex_kv_cache_position_value_count(const yvex_kv_cache *kv);
int yvex_kv_cache_append_position_f32(yvex_kv_cache *kv,
                                      const float *values,
                                      unsigned long long value_count,
                                      unsigned long long *out_position,
                                      yvex_error *err);
int yvex_kv_cache_read_position_f32(yvex_kv_cache *kv,
                                    unsigned long long position,
                                    float *out_values,
                                    unsigned long long value_count,
                                    yvex_error *err);
int yvex_kv_cache_clear(yvex_kv_cache *kv,
                        yvex_error *err);
int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_KV_H */
