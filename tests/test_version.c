#include <yvex/version.h>

#include "test.h"

int main(void)
{
    YVEX_TEST_STREQ(yvex_version_string(), "0.1.0");
    YVEX_TEST_ASSERT(yvex_version_major() == 0);
    YVEX_TEST_ASSERT(yvex_version_minor() == 1);
    YVEX_TEST_ASSERT(yvex_version_patch() == 0);
    return 0;
}
