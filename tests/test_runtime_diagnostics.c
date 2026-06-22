/*
 * YVEX - runtime diagnostics tests
 *
 * File: tests/test_runtime_diagnostics.c
 * Layer: test
 *
 * Purpose:
 *   Proves that H0 engine/session diagnostics expose the partial graph reason
 *   and keep execution readiness false.
 *
 * Covers:
 *   - yvex_engine_diagnostic_reason
 *   - yvex_session_diagnostic_reason
 *   - backend unsupported status through public backend API
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_runtime_diagnostics
 *
 * Expected:
 *   - exits 0 on success
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

int main(void)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_backend *cuda = NULL;
    yvex_backend_options cuda_options;
    yvex_session *session = NULL;
    yvex_session_options options;
    yvex_session_summary summary;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&cuda_options, 0, sizeof(cuda_options));
    options.allow_partial_graph = 1;

    rc = yvex_engine_open_path(&engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open diagnostic engine");
    YVEX_TEST_ASSERT(strstr(yvex_engine_diagnostic_reason(engine), "graph partial") != NULL,
                     "engine reason graph partial");
    YVEX_TEST_ASSERT(strstr(yvex_engine_diagnostic_reason(engine), "output_head") != NULL,
                     "engine reason output_head");

    rc = yvex_backend_open_cpu(&backend, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open diagnostic backend");
    rc = yvex_session_create(&session, engine, backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create diagnostic session");
    rc = yvex_session_get_summary(session, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "diagnostic session summary");
    YVEX_TEST_ASSERT(summary.execution_ready == 0, "execution readiness false");
    YVEX_TEST_ASSERT_STREQ(summary.backend_status, "ready", "backend ready visible");
    YVEX_TEST_ASSERT(strstr(yvex_session_diagnostic_reason(session), "output_norm") != NULL,
                     "session reason output_norm");

    cuda_options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&cuda, &cuda_options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "cuda unsupported visible");
    YVEX_TEST_ASSERT(cuda == NULL, "cuda backend not allocated");

    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    return 0;
}
