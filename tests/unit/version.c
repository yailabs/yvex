/* Exercises the core version API returns stable compile-time values without runtime setup. */
#include <yvex/core.h>

#include "tests/test.h"
#include <stdio.h>

int yvex_test_version(void)
{
    char expected[64];
    (void)snprintf(expected, sizeof(expected), "%d.%d.%d",
                   YVEX_VERSION_MAJOR, YVEX_VERSION_MINOR, YVEX_VERSION_PATCH);
    YVEX_TEST_ASSERT_STREQ(yvex_version_string(), expected, "canonical version projection");
    YVEX_TEST_ASSERT(yvex_version_major() == YVEX_VERSION_MAJOR, "version major");
    YVEX_TEST_ASSERT(yvex_version_minor() == YVEX_VERSION_MINOR, "version minor");
    YVEX_TEST_ASSERT(yvex_version_patch() == YVEX_VERSION_PATCH, "version patch");
    return 0;
}
