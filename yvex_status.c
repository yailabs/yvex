/*
 * YVEX - Status code helpers
 *
 * File: yvex_status.c
 * Layer: core implementation
 *
 * Purpose:
 *   Implements deterministic status name and predicate helpers for the core
 *   public status vocabulary.
 *
 * Implements:
 *   - yvex_status_name
 *   - yvex_status_is_ok
 *   - yvex_status_is_error
 *
 * Invariants:
 *   - every known core status has a stable string name
 *   - unknown status values return YVEX_STATUS_UNKNOWN
 *   - helpers allocate no memory and do not log
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_status
 */
#include <yvex/status.h>

const char *yvex_status_name(yvex_status status)
{
    switch (status) {
    case YVEX_OK:
        return "YVEX_OK";
    case YVEX_ERR:
        return "YVEX_ERR";
    case YVEX_ERR_NOMEM:
        return "YVEX_ERR_NOMEM";
    case YVEX_ERR_IO:
        return "YVEX_ERR_IO";
    case YVEX_ERR_FORMAT:
        return "YVEX_ERR_FORMAT";
    case YVEX_ERR_UNSUPPORTED:
        return "YVEX_ERR_UNSUPPORTED";
    case YVEX_ERR_BACKEND:
        return "YVEX_ERR_BACKEND";
    case YVEX_ERR_BOUNDS:
        return "YVEX_ERR_BOUNDS";
    case YVEX_ERR_STATE:
        return "YVEX_ERR_STATE";
    case YVEX_ERR_CANCELLED:
        return "YVEX_ERR_CANCELLED";
    case YVEX_ERR_INVALID_ARG:
        return "YVEX_ERR_INVALID_ARG";
    default:
        return "YVEX_STATUS_UNKNOWN";
    }
}

int yvex_status_is_ok(yvex_status status)
{
    return status == YVEX_OK;
}

int yvex_status_is_error(yvex_status status)
{
    return status != YVEX_OK;
}
