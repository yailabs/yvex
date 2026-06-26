/*
 * YVEX - session tests
 *
 * File: tests/test_session.c
 * Layer: test
 *
 * Purpose:
 *   Proves that engine/session layer sessions bind an engine to the CPU backend, expose partial
 *   lifecycle state, accept bounded tokens, and fail prefill/decode honestly.
 *
 * Covers:
 *   - yvex_session_create
 *   - yvex_session_accept_tokens
 *   - yvex_session_prefill
 *   - yvex_session_decode_next
 *   - cancel/reset transitions
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_session
 *
 * Expected:
 *   - exits 0 on success
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

typedef struct {
    yvex_engine *engine;
    yvex_backend *backend;
} session_fixture;

static int open_fixture(session_fixture *fixture)
{
    yvex_error err;
    int rc;

    memset(fixture, 0, sizeof(*fixture));
    rc = yvex_engine_open_path(&fixture->engine, "tests/fixtures/gguf/valid-tokenizer-simple.gguf", &err);
    if (rc == YVEX_OK) {
        rc = yvex_backend_open_cpu(&fixture->backend, &err);
    }
    if (rc != YVEX_OK) {
        yvex_backend_close(fixture->backend);
        yvex_engine_close(fixture->engine);
        fprintf(stderr, "session fixture failed: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    return 0;
}

static void close_fixture(session_fixture *fixture)
{
    yvex_backend_close(fixture->backend);
    yvex_engine_close(fixture->engine);
}

static int test_session_lifecycle(void)
{
    session_fixture fixture;
    yvex_session *session = NULL;
    yvex_session_options options;
    yvex_session_summary summary;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.allow_partial_graph = 1;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open session fixture");
    rc = yvex_session_create(&session, fixture.engine, fixture.backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create session");
    YVEX_TEST_ASSERT(yvex_session_state_of(session) == YVEX_SESSION_STATE_PARTIAL,
                     "partial graph creates partial session");
    YVEX_TEST_ASSERT(yvex_session_context_length(session) == 4096, "context from descriptor");
    YVEX_TEST_ASSERT(yvex_session_position(session) == 0, "initial position");

    rc = yvex_session_get_summary(session, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "session summary");
    YVEX_TEST_ASSERT(summary.execution_ready == 0, "execution false");
    YVEX_TEST_ASSERT(summary.graph_partial == 1, "graph partial in summary");
    YVEX_TEST_ASSERT_STREQ(summary.kv_status, "unavailable", "kv unavailable");
    YVEX_TEST_ASSERT_STREQ(summary.logits_status, "unavailable", "logits unavailable");

    rc = yvex_session_cancel(session, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cancel session");
    YVEX_TEST_ASSERT(yvex_session_state_of(session) == YVEX_SESSION_STATE_CANCELLED, "cancelled state");
    rc = yvex_session_reset(session, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "reset session");
    YVEX_TEST_ASSERT(yvex_session_state_of(session) == YVEX_SESSION_STATE_PARTIAL, "reset to partial");

    yvex_session_close(session);
    close_fixture(&fixture);
    return 0;
}

static int test_session_tokens_and_unsupported_runtime(void)
{
    session_fixture fixture;
    yvex_session *session = NULL;
    yvex_session_options options;
    yvex_tokens tokens;
    unsigned int token;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&tokens, 0, sizeof(tokens));
    options.allow_partial_graph = 1;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open token fixture");
    rc = yvex_session_create(&session, fixture.engine, fixture.backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create token session");

    rc = yvex_tokenize_text(yvex_engine_tokenizer(fixture.engine), "hello world", &tokens, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "tokenize hello world");
    YVEX_TEST_ASSERT(tokens.len == 3, "token count");
    rc = yvex_session_accept_tokens(session, &tokens, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "accept tokens");
    YVEX_TEST_ASSERT(yvex_session_position(session) == 3, "position advanced");

    rc = yvex_session_prefill(session, &tokens, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "prefill unsupported");
    YVEX_TEST_ASSERT(yvex_session_position(session) == 3, "prefill does not advance");
    rc = yvex_session_decode_next(session, &token, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "decode unsupported");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    close_fixture(&fixture);
    return 0;
}

static int test_session_context_overflow(void)
{
    session_fixture fixture;
    yvex_session *session = NULL;
    yvex_session_options options;
    yvex_tokens tokens;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&tokens, 0, sizeof(tokens));
    options.context_length = 2;
    options.allow_partial_graph = 1;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open overflow fixture");
    rc = yvex_session_create(&session, fixture.engine, fixture.backend, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create overflow session");
    rc = yvex_tokenize_text(yvex_engine_tokenizer(fixture.engine), "hello world", &tokens, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "tokenize overflow text");
    rc = yvex_session_accept_tokens(session, &tokens, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "context overflow rejected");
    YVEX_TEST_ASSERT(yvex_session_position(session) == 0, "overflow does not advance");

    yvex_tokens_free(&tokens);
    yvex_session_close(session);
    close_fixture(&fixture);
    return 0;
}

static int test_session_observes_engine_weights(void)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_engine_options engine_options;
    yvex_session_options session_options;
    yvex_session_summary summary;
    yvex_error err;
    int rc;

    memset(&engine_options, 0, sizeof(engine_options));
    memset(&session_options, 0, sizeof(session_options));
    engine_options.model_path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
    engine_options.load_tokenizer = 1;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = "cpu";
    engine_options.require_all_weights = 1;
    session_options.allow_partial_graph = 1;

    rc = yvex_engine_open(&engine, &engine_options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open attached engine");
    rc = yvex_backend_open_cpu(&backend, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open session backend");
    rc = yvex_session_create(&session, engine, backend, &session_options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "create session over attached engine");
    rc = yvex_session_get_summary(session, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "attached session summary");
    YVEX_TEST_ASSERT(summary.weights_attached == 1, "session sees attached weights");
    YVEX_TEST_ASSERT_STREQ(summary.weights_backend, "cpu", "session sees weight backend");
    YVEX_TEST_ASSERT(summary.weight_tensor_count == 1, "session sees weight count");
    YVEX_TEST_ASSERT(summary.weight_total_bytes == 128, "session sees weight bytes");
    YVEX_TEST_ASSERT(summary.execution_ready == 0, "session execution false");
    YVEX_TEST_ASSERT(summary.graph_execution_ready == 0, "session graph execution false");

    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    return 0;
}

int yvex_test_session(void)
{
    if (test_session_lifecycle() != 0) return 1;
    if (test_session_tokens_and_unsupported_runtime() != 0) return 1;
    if (test_session_context_overflow() != 0) return 1;
    if (test_session_observes_engine_weights() != 0) return 1;
    return 0;
}
