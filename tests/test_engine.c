/*
 * YVEX - engine tests
 *
 * File: tests/test_engine.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the engine/session layer engine opens the full descriptor/tokenizer/graph stack
 *   and reports an inspectable partial runtime descriptor.
 *
 * Covers:
 *   - yvex_engine_open_path
 *   - yvex_engine_get_summary
 *   - engine-owned model/tensor/tokenizer accessors
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_engine
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static int test_engine_opens_fixture(void)
{
    yvex_engine *engine = NULL;
    yvex_engine_summary summary;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "engine opens fixture");
    YVEX_TEST_ASSERT(engine != NULL, "engine allocated");
    YVEX_TEST_ASSERT(yvex_engine_model(engine) != NULL, "engine owns model");
    YVEX_TEST_ASSERT(yvex_engine_tensors(engine) != NULL, "engine owns tensors");
    YVEX_TEST_ASSERT(yvex_engine_tokenizer(engine) != NULL, "engine owns tokenizer");
    YVEX_TEST_ASSERT(yvex_engine_graph(engine) != NULL, "engine owns graph");
    YVEX_TEST_ASSERT(yvex_engine_status_of(engine) == YVEX_ENGINE_STATUS_PARTIAL,
                     "engine status partial");

    rc = yvex_engine_get_summary(engine, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "engine summary");
    YVEX_TEST_ASSERT_STREQ(summary.architecture, "llama", "engine architecture");
    YVEX_TEST_ASSERT_STREQ(summary.model_name, "yvex-tokenizer-test", "engine model name");
    YVEX_TEST_ASSERT(summary.tensor_count == 1, "engine tensor count");
    YVEX_TEST_ASSERT(summary.known_tensor_bytes == 128, "engine known bytes");
    YVEX_TEST_ASSERT(summary.unsupported_tensor_accounting == 0, "engine unsupported accounting");
    YVEX_TEST_ASSERT_STREQ(summary.tokenizer_model, "yvex-fixture-simple", "engine tokenizer kind");
    YVEX_TEST_ASSERT_STREQ(summary.tokenizer_support, "fixture-encode-decode", "engine tokenizer support");
    YVEX_TEST_ASSERT_STREQ(summary.graph_status, "partial", "engine graph partial");
    YVEX_TEST_ASSERT(strstr(yvex_engine_diagnostic_reason(engine), "output_norm") != NULL,
                     "engine reason mentions output_norm");

    yvex_engine_close(engine);
    return 0;
}

static int test_engine_errors(void)
{
    yvex_engine *engine = NULL;
    yvex_error err;
    int rc;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/no-such.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_IO, "missing path returns IO");
    YVEX_TEST_ASSERT(engine == NULL, "missing path leaves engine null");

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/bad-magic.gguf", &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "bad gguf rejected");
    YVEX_TEST_ASSERT(engine == NULL, "bad gguf leaves engine null");
    return 0;
}

int main(void)
{
    if (test_engine_opens_fixture() != 0) return 1;
    if (test_engine_errors() != 0) return 1;
    return 0;
}
