/*
 * YVEX - KV cache skeleton
 *
 * File: include/yvex/kv.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the engine/session layer KV cache planning skeleton. The object reports whether KV
 *   cache sizing is available from the current descriptor; it does not allocate
 *   backend tensors or store attention state in engine/session layer.
 *
 * Owns:
 *   - yvex_kv_cache
 *   - KV summary/status vocabulary
 *
 * Does not own:
 *   - backend allocation
 *   - attention execution
 *   - decode runtime
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
    yvex_kv_status status;
    unsigned long long context_length;
    unsigned long long layer_count;
    unsigned long long kv_head_count;
    unsigned long long head_dim;
    unsigned long long bytes;
} yvex_kv_summary;

int yvex_kv_cache_create(yvex_kv_cache **out,
                         const yvex_model_descriptor *model,
                         unsigned long long context_length,
                         yvex_error *err);
void yvex_kv_cache_close(yvex_kv_cache *kv);

yvex_kv_status yvex_kv_status_of(const yvex_kv_cache *kv);
const char *yvex_kv_status_name(yvex_kv_status status);

int yvex_kv_cache_get_summary(const yvex_kv_cache *kv,
                              yvex_kv_summary *out,
                              yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_KV_H */
