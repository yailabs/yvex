/*
 * YVEX - Version implementation
 *
 * File: src/core/version.c
 * Layer: core implementation
 *
 * Purpose:
 *   Implements the fixed core version surface. These helpers return static
 *   compile-time values and do not read files, environment, or runtime state.
 *
 * Implements:
 *   - yvex_version_string
 *   - yvex_version_major
 *   - yvex_version_minor
 *   - yvex_version_patch
 *
 * Invariants:
 *   - helpers allocate no memory
 *   - helpers cannot fail
 *   - helpers are safe before runtime initialization
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_version
 */
#include <yvex/version.h>

const char *yvex_version_string(void)
{
    return "0.1.0";
}

int yvex_version_major(void)
{
    return YVEX_VERSION_MAJOR;
}

int yvex_version_minor(void)
{
    return YVEX_VERSION_MINOR;
}

int yvex_version_patch(void)
{
    return YVEX_VERSION_PATCH;
}
