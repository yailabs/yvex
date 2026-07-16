/*
 * YVEX - Version surface
 *
 * File: include/yvex/version.h
 * Layer: public core API
 *
 * Purpose:
 *   Exposes core version constants, no-allocation version helper functions,
 *   and the immutable Operator protocol identity report used by trusted local
 *   clients.
 *
 * Owns:
 *   - YVEX_VERSION_MAJOR
 *   - YVEX_VERSION_MINOR
 *   - YVEX_VERSION_PATCH
 *   - yvex_version_string
 *   - yvex_version_major
 *   - yvex_version_minor
 *   - yvex_version_patch
 *   - yvex_operator_contract_report
 *   - yvex_operator_contract_report_build
 *
 * Does not own:
 *   - build metadata discovery
 *   - runtime configuration
 *   - CLI parsing or JSON rendering
 *   - runtime/backend capability discovery
 *   - filesystem or environment reads
 *
 * Used by:
 *   - libyvex core
 *   - yvex CLI
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_version
 */
#ifndef YVEX_VERSION_H
#define YVEX_VERSION_H

#include <yvex/status.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_VERSION_MAJOR 0
#define YVEX_VERSION_MINOR 1
#define YVEX_VERSION_PATCH 0

#define YVEX_OPERATOR_CONTRACT_SCHEMA_VERSION "1"
#define YVEX_OPERATOR_PROTOCOL_VERSION "1"

typedef struct {
    const char *schema_version;
    const char *protocol_version;
    const char *yvex_version;
    const char *product;
} yvex_operator_contract_report;

const char *yvex_version_string(void);
int yvex_version_major(void);
int yvex_version_minor(void);
int yvex_version_patch(void);
yvex_status yvex_operator_contract_report_build(
    yvex_operator_contract_report *report);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_VERSION_H */
