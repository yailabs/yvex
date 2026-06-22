/*
 * YVEX - Logits buffer skeleton
 *
 * File: include/yvex/logits.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the H0 logits buffer skeleton. The object reports whether a logits
 *   buffer can be sized from the current descriptor; H0 does not compute logits
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
 *   - H0 tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_logits
 */
#ifndef YVEX_LOGITS_H
#define YVEX_LOGITS_H

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

int yvex_logits_create(yvex_logits **out,
                       const yvex_model_descriptor *model,
                       yvex_error *err);
void yvex_logits_close(yvex_logits *logits);

yvex_logits_status yvex_logits_status_of(const yvex_logits *logits);
const char *yvex_logits_status_name(yvex_logits_status status);

int yvex_logits_get_summary(const yvex_logits *logits,
                            yvex_logits_summary *out,
                            yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_LOGITS_H */
