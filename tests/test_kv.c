/*
 * YVEX - KV skeleton tests
 *
 * File: tests/test_kv.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the engine/session layer KV cache object reports unavailable state without
 *   allocating backend memory when descriptor attention metadata is absent.
 *
 * Covers:
 *   - yvex_kv_cache_create
 *   - yvex_kv_cache_get_summary
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_kv
 *
 * Expected:
 *   - exits 0 on success
 */
#include <yvex/yvex.h>

#include "test.h"

static int test_kv_unavailable_for_fixture(void)
{
    yvex_engine *engine = NULL;
    yvex_kv_cache *kv = NULL;
    yvex_kv_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open engine");
    rc = yvex_kv_cache_create(&kv, yvex_engine_model(engine), 4096, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create kv");
    YVEX_TEST_ASSERT(yvex_kv_status_of(kv) == YVEX_KV_STATUS_UNAVAILABLE, "kv unavailable");
    rc = yvex_kv_cache_get_summary(kv, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "kv summary");
    YVEX_TEST_ASSERT(summary.context_length == 4096, "kv context");
    YVEX_TEST_ASSERT(summary.bytes == 0, "kv bytes zero");

    yvex_kv_cache_close(kv);
    yvex_engine_close(engine);
    return 0;
}

static int test_kv_invalid_context(void)
{
    yvex_engine *engine = NULL;
    yvex_kv_cache *kv = NULL;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open engine invalid context");
    rc = yvex_kv_cache_create(&kv, yvex_engine_model(engine), 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "zero context rejected");
    YVEX_TEST_ASSERT(kv == NULL, "failed kv null");
    yvex_engine_close(engine);
    return 0;
}

int main(void)
{
    if (test_kv_unavailable_for_fixture() != 0) return 1;
    if (test_kv_invalid_context() != 0) return 1;
    return 0;
}
