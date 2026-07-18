/*
 * Owner: abi.log (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Log names
 *
 * File: include/yvex/log.h
 * Layer: public core API
 *
 * Purpose:
 *   Defines the narrow core logging level/domain vocabulary and string helpers.
 *   This is a naming surface only; no logging sink is implemented here.
 *
 * Owns:
 *   - yvex_log_level
 *   - yvex_log_domain
 *   - yvex_log_level_name
 *   - yvex_log_domain_name
 *
 * Does not own:
 *   - log sinks
 *   - trace events
 *   - runtime configuration
 *   - future module-specific log domains
 *
 * Used by:
 *   - libyvex core
 *   - yvex CLI
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_log
 */
#ifndef YVEX_LOG_H
#define YVEX_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_LOG_ERROR,
    YVEX_LOG_WARN,
    YVEX_LOG_INFO,
    YVEX_LOG_DEBUG,
    YVEX_LOG_TRACE
} yvex_log_level;

typedef enum {
    YVEX_LOG_CORE,
    YVEX_LOG_CLI
} yvex_log_domain;

const char *yvex_log_level_name(yvex_log_level level);
const char *yvex_log_domain_name(yvex_log_domain domain);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_LOG_H */
