/*
 * Owner: abi.version (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Version surface
 *
 * File: include/yvex/version.h
 * Layer: public core API
 *
 * Purpose:
 *   Exposes core version constants and no-allocation version helper functions
 *   used by the core library, CLI, and tests.
 *
 * Owns:
 *   - YVEX_VERSION_MAJOR
 *   - YVEX_VERSION_MINOR
 *   - YVEX_VERSION_PATCH
 *   - yvex_version_string
 *   - yvex_version_major
 *   - yvex_version_minor
 *   - yvex_version_patch
 *
 * Does not own:
 *   - build metadata discovery
 *   - runtime configuration
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

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_VERSION_MAJOR 0
#define YVEX_VERSION_MINOR 1
#define YVEX_VERSION_PATCH 0

const char *yvex_version_string(void);
int yvex_version_major(void);
int yvex_version_minor(void);
int yvex_version_patch(void);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_VERSION_H */
