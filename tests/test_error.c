#include <yvex/error.h>

#include "test.h"

int main(void)
{
    yvex_error err;
    char long_message[400];
    unsigned long i;

    yvex_error_clear(&err);
    YVEX_TEST_ASSERT(err.code == YVEX_OK);
    YVEX_TEST_STREQ(err.where, "");
    YVEX_TEST_STREQ(err.message, "");
    YVEX_TEST_ASSERT(!yvex_error_is_set(&err));

    yvex_error_set(&err, YVEX_ERR_INVALID_ARG, "test_error", "bad argument");
    YVEX_TEST_ASSERT(err.code == YVEX_ERR_INVALID_ARG);
    YVEX_TEST_STREQ(err.where, "test_error");
    YVEX_TEST_STREQ(err.message, "bad argument");
    YVEX_TEST_ASSERT(yvex_error_is_set(&err));

    yvex_error_set(&err, YVEX_ERR, NULL, NULL);
    YVEX_TEST_STREQ(err.where, "unknown");
    YVEX_TEST_STREQ(err.message, "");

    yvex_error_setf(&err, YVEX_ERR_BOUNDS, "bounds", "index %d", 7);
    YVEX_TEST_ASSERT(err.code == YVEX_ERR_BOUNDS);
    YVEX_TEST_STREQ(err.where, "bounds");
    YVEX_TEST_STREQ(err.message, "index 7");

    for (i = 0; i < sizeof(long_message) - 1; ++i) {
        long_message[i] = 'x';
    }
    long_message[sizeof(long_message) - 1] = '\0';

    yvex_error_set(&err, YVEX_ERR, long_message, long_message);
    YVEX_TEST_ASSERT(err.where[sizeof(err.where) - 1] == '\0');
    YVEX_TEST_ASSERT(err.message[sizeof(err.message) - 1] == '\0');
    YVEX_TEST_ASSERT(strlen(err.where) == sizeof(err.where) - 1);
    YVEX_TEST_ASSERT(strlen(err.message) == sizeof(err.message) - 1);

    yvex_error_clear(NULL);
    yvex_error_set(NULL, YVEX_ERR, "ignored", "ignored");
    yvex_error_setf(NULL, YVEX_ERR, "ignored", "ignored");
    YVEX_TEST_ASSERT(!yvex_error_is_set(NULL));
    return 0;
}
