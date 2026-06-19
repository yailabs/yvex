#include <yvex/log.h>

#include "test.h"

int main(void)
{
    YVEX_TEST_STREQ(yvex_log_level_name(YVEX_LOG_ERROR), "error");
    YVEX_TEST_STREQ(yvex_log_level_name(YVEX_LOG_WARN), "warn");
    YVEX_TEST_STREQ(yvex_log_level_name(YVEX_LOG_INFO), "info");
    YVEX_TEST_STREQ(yvex_log_level_name(YVEX_LOG_DEBUG), "debug");
    YVEX_TEST_STREQ(yvex_log_level_name(YVEX_LOG_TRACE), "trace");
    YVEX_TEST_STREQ(yvex_log_level_name((yvex_log_level)1234), "unknown");

    YVEX_TEST_STREQ(yvex_log_domain_name(YVEX_LOG_CORE), "core");
    YVEX_TEST_STREQ(yvex_log_domain_name(YVEX_LOG_CLI), "cli");
    YVEX_TEST_STREQ(yvex_log_domain_name((yvex_log_domain)1234), "unknown");
    return 0;
}
