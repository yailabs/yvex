/*
 * YVEX - Status codes
 *
 * File: include/yvex/status.h
 * Layer: public core API
 *
 * Purpose:
 *   Defines the stable status-code vocabulary used by the core library, CLI,
 *   and tests. These values are intentionally small and explicit.
 *
 * Owns:
 *   - yvex_status
 *   - yvex_status_name
 *   - yvex_status_is_ok
 *   - yvex_status_is_error
 *
 * Does not own:
 *   - formatted error messages
 *   - logging sinks
 *   - runtime failure recovery
 *
 * Used by:
 *   - error.h
 *   - CLI command dispatch
 *   - core tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_status
 */
#ifndef YVEX_STATUS_H
#define YVEX_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_OK = 0,
    YVEX_ERR = -1,
    YVEX_ERR_NOMEM = -2,
    YVEX_ERR_IO = -3,
    YVEX_ERR_FORMAT = -4,
    YVEX_ERR_UNSUPPORTED = -5,
    YVEX_ERR_BACKEND = -6,
    YVEX_ERR_BOUNDS = -7,
    YVEX_ERR_STATE = -8,
    YVEX_ERR_CANCELLED = -9,
    YVEX_ERR_INVALID_ARG = -10
} yvex_status;

const char *yvex_status_name(yvex_status status);
int yvex_status_is_ok(yvex_status status);
int yvex_status_is_error(yvex_status status);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_STATUS_H */
