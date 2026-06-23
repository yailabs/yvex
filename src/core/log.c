/*
 * YVEX - Log name helpers
 *
 * File: src/core/log.c
 * Layer: core implementation
 *
 * Purpose:
 *   Implements string helpers for the narrow core logging vocabulary. This is
 *   not a logging sink and does not emit logs.
 *
 * Implements:
 *   - yvex_log_level_name
 *   - yvex_log_domain_name
 *
 * Invariants:
 *   - unknown levels and domains return "unknown"
 *   - helpers allocate no memory
 *   - helpers require no runtime initialization
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_log
 */
#include <yvex/log.h>

const char *yvex_log_level_name(yvex_log_level level)
{
    switch (level) {
    case YVEX_LOG_ERROR:
        return "error";
    case YVEX_LOG_WARN:
        return "warn";
    case YVEX_LOG_INFO:
        return "info";
    case YVEX_LOG_DEBUG:
        return "debug";
    case YVEX_LOG_TRACE:
        return "trace";
    default:
        return "unknown";
    }
}

const char *yvex_log_domain_name(yvex_log_domain domain)
{
    switch (domain) {
    case YVEX_LOG_CORE:
        return "core";
    case YVEX_LOG_CLI:
        return "cli";
    default:
        return "unknown";
    }
}
