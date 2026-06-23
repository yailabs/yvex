/*
 * YVEX - logits skeleton tests
 *
 * File: tests/test_logits.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the engine/session layer logits object reports unavailable state when the
 *   descriptor lacks a reliable output head/runtime logits binding.
 *
 * Covers:
 *   - yvex_logits_create
 *   - yvex_logits_get_summary
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_logits
 *
 * Expected:
 *   - exits 0 on success
 */
#include <yvex/yvex.h>

#include "test.h"

int yvex_test_logits(void)
{
    yvex_engine *engine = NULL;
    yvex_logits *logits = NULL;
    yvex_logits_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open engine");
    rc = yvex_logits_create(&logits, yvex_engine_model(engine), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create logits");
    YVEX_TEST_ASSERT(yvex_logits_status_of(logits) == YVEX_LOGITS_STATUS_UNAVAILABLE,
                     "logits unavailable");
    rc = yvex_logits_get_summary(logits, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "logits summary");
    YVEX_TEST_ASSERT(summary.vocab_size == 0, "logits vocab unavailable");
    YVEX_TEST_ASSERT(summary.bytes == 0, "logits bytes zero");

    yvex_logits_close(logits);
    yvex_engine_close(engine);
    return 0;
}
