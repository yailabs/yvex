/*
 * YVEX - chat runtime tests
 *
 * File: tests/test_chat_runtime.c
 * Layer: test
 *
 * Purpose:
 *   Proves the I0 runtime shell can open engine/backend/session objects,
 *   accept fixture prompt text into the session, reset state, and handle CUDA
 *   ready/unavailable paths without claiming generation.
 *
 * Covers:
 *   - yvex_chat_runtime_open
 *   - yvex_chat_runtime_accept_user_text
 *   - yvex_chat_runtime_reset
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_chat_runtime
 *
 * Expected:
 *   - exits 0 on success
 */
#include <string.h>

#include "../src/chat/chat_internal.h"
#include "test.h"

static int test_runtime_accept_and_reset(void)
{
    yvex_chat_runtime runtime;
    yvex_chat_accept_result result;
    yvex_session_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_chat_runtime_open(&runtime, "tests/fixtures/gguf/valid-tokenizer-simple.gguf",
                                "cpu", 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open chat runtime");

    rc = yvex_chat_runtime_accept_user_text(&runtime, NULL, "hello world", &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "accept user text");
    YVEX_TEST_ASSERT_STREQ(result.model_name, "yvex-tokenizer-test", "result model");
    YVEX_TEST_ASSERT_STREQ(result.backend_name, "cpu", "result backend");
    YVEX_TEST_ASSERT_STREQ(result.session_state, "partial", "result session state");
    YVEX_TEST_ASSERT(result.prompt_tokens > 0, "prompt tokens recorded");
    YVEX_TEST_ASSERT(result.accepted_tokens == result.prompt_tokens, "accepted tokens match prompt");
    YVEX_TEST_ASSERT(result.position == result.prompt_tokens, "position matches prompt");
    YVEX_TEST_ASSERT_STREQ(result.generation, "unsupported", "generation unsupported");
    YVEX_TEST_ASSERT(strstr(result.reason, "I0") != NULL, "reason mentions I0");

    rc = yvex_chat_runtime_reset(&runtime, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "runtime reset");
    rc = yvex_chat_runtime_get_summary(&runtime, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "runtime summary after reset");
    YVEX_TEST_ASSERT(summary.position == 0, "position reset");
    YVEX_TEST_ASSERT(summary.accepted_tokens == 0, "accepted reset");

    yvex_chat_runtime_close(&runtime);
    return 0;
}

static int test_runtime_errors(void)
{
    yvex_chat_runtime runtime;
    yvex_error err;
    int rc;

    rc = yvex_chat_runtime_open(&runtime, "tests/fixtures/gguf/valid-tokenizer-simple.gguf",
                                "cuda", 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK || rc == YVEX_ERR_UNSUPPORTED,
                     "cuda runtime opens or reports unsupported");
    if (rc == YVEX_OK) {
        yvex_session_summary summary;

        rc = yvex_chat_runtime_get_summary(&runtime, &summary, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda runtime summary");
        YVEX_TEST_ASSERT(summary.execution_ready == 0, "cuda runtime execution false");
        yvex_chat_runtime_close(&runtime);
    }

    rc = yvex_chat_runtime_open(&runtime, NULL, "cpu", 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "missing model path fails");
    return 0;
}

int main(void)
{
    if (test_runtime_accept_and_reset() != 0) return 1;
    if (test_runtime_errors() != 0) return 1;
    return 0;
}
