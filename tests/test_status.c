#include <yvex/status.h>

#include "test.h"

int main(void)
{
    YVEX_TEST_STREQ(yvex_status_name(YVEX_OK), "YVEX_OK");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR), "YVEX_ERR");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_NOMEM), "YVEX_ERR_NOMEM");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_IO), "YVEX_ERR_IO");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_FORMAT), "YVEX_ERR_FORMAT");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_UNSUPPORTED), "YVEX_ERR_UNSUPPORTED");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_BACKEND), "YVEX_ERR_BACKEND");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_BOUNDS), "YVEX_ERR_BOUNDS");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_STATE), "YVEX_ERR_STATE");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_CANCELLED), "YVEX_ERR_CANCELLED");
    YVEX_TEST_STREQ(yvex_status_name(YVEX_ERR_INVALID_ARG), "YVEX_ERR_INVALID_ARG");
    YVEX_TEST_STREQ(yvex_status_name((yvex_status)1234), "YVEX_STATUS_UNKNOWN");

    YVEX_TEST_ASSERT(yvex_status_is_ok(YVEX_OK));
    YVEX_TEST_ASSERT(!yvex_status_is_ok(YVEX_ERR));
    YVEX_TEST_ASSERT(!yvex_status_is_error(YVEX_OK));
    YVEX_TEST_ASSERT(yvex_status_is_error(YVEX_ERR_INVALID_ARG));
    return 0;
}
