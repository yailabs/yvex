/*
 * Owner: abi.error (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Error objects
 *
 * File: include/yvex/error.h
 * Layer: public core API
 *
 * Purpose:
 *   Defines caller-owned fixed-size error objects and helper functions for
 *   setting, clearing, and inspecting precise YVEX failures.
 *
 * Owns:
 *   - YVEX_ERROR_WHERE_CAP
 *   - YVEX_ERROR_MESSAGE_CAP
 *   - yvex_error
 *   - yvex_error_clear
 *   - yvex_error_set
 *   - yvex_error_setf
 *   - yvex_error_is_set
 *   - yvex_error_code
 *   - yvex_error_where
 *   - yvex_error_message
 *
 * Does not own:
 *   - logging
 *   - process exit behavior
 *   - heap allocation
 *
 * Used by:
 *   - libyvex core
 *   - yvex CLI
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_error
 */
#ifndef YVEX_ERROR_H
#define YVEX_ERROR_H

#include <yvex/status.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_ERROR_WHERE_CAP 96
#define YVEX_ERROR_MESSAGE_CAP 256

typedef struct {
    yvex_status code;
    char where[YVEX_ERROR_WHERE_CAP];
    char message[YVEX_ERROR_MESSAGE_CAP];
} yvex_error;

void yvex_error_clear(yvex_error *err);
void yvex_error_set(yvex_error *err, yvex_status code, const char *where, const char *message);
void yvex_error_setf(yvex_error *err, yvex_status code, const char *where, const char *fmt, ...);
int yvex_error_is_set(const yvex_error *err);
yvex_status yvex_error_code(const yvex_error *err);
const char *yvex_error_where(const yvex_error *err);
const char *yvex_error_message(const yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ERROR_H */
