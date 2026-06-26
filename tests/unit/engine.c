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
#include <sys/stat.h>

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

static int test_engine_attaches_weights(void)
{
    yvex_engine *engine = NULL;
    yvex_engine_options options;
    yvex_engine_summary summary;
    yvex_error err;
    int rc;
    int i;

    for (i = 0; i < 3; ++i) {
        memset(&options, 0, sizeof(options));
        options.model_path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
        options.load_tokenizer = 1;
        options.build_descriptor = 1;
        options.build_default_graph = 1;
        options.attach_weights = 1;
        options.backend_name = "cpu";
        options.require_all_weights = 1;

        rc = yvex_engine_open(&engine, &options, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "engine opens with attached weights");
        rc = yvex_engine_get_summary(engine, &summary, &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "attached engine summary");
        YVEX_TEST_ASSERT(summary.weights_attached == 1, "weights attached");
        YVEX_TEST_ASSERT_STREQ(summary.weights_backend, "cpu", "weights backend cpu");
        YVEX_TEST_ASSERT(summary.weight_tensor_count == 1, "attached tensor count");
        YVEX_TEST_ASSERT(summary.weight_total_bytes == 128, "attached weight bytes");
        YVEX_TEST_ASSERT(summary.weight_backend_allocated_bytes == 128,
                         "attached backend allocated bytes");
        YVEX_TEST_ASSERT(summary.graph_execution_ready == 0, "graph execution false");
        YVEX_TEST_ASSERT_STREQ(summary.graph_status, "partial", "graph remains partial");

        yvex_engine_close(engine);
        engine = NULL;
    }

    return 0;
}

static int emit_controlled_engine_fixture(const char *path)
{
    yvex_gguf_emit_options options;
    yvex_gguf_emit_summary summary;
    yvex_error err;
    int rc;

    (void)mkdir("build/tests", 0777);
    (void)mkdir("build/tests/engine", 0777);
    remove(path);
    memset(&options, 0, sizeof(options));
    yvex_error_clear(&err);
    options.out_path = path;
    options.model_name = "m4-controlled";
    options.architecture = "deepseek";
    options.overwrite = 1;

    rc = yvex_gguf_emit_controlled(&options, &summary, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "controlled emit failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    return 0;
}

static int open_attached_engine(const char *path,
                                const char *backend_name,
                                int build_graph,
                                int attach_weights,
                                yvex_engine **out)
{
    yvex_engine_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.model_path = path;
    options.build_descriptor = 1;
    options.build_default_graph = build_graph;
    options.attach_weights = attach_weights;
    options.backend_name = backend_name;
    options.require_all_weights = 1;

    rc = yvex_engine_open(out, &options, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "engine open failed: %s: %s\n",
                yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    return 0;
}

static int test_engine_executes_fixture_graph(void)
{
    const char *path = "build/tests/engine/m4-controlled.gguf";
    yvex_engine *engine = NULL;
    yvex_fixture_graph_options options;
    yvex_fixture_graph_result row0;
    yvex_fixture_graph_result row1;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(emit_controlled_engine_fixture(path) == 0, "emit M4 fixture");
    YVEX_TEST_ASSERT(open_attached_engine(path, "cpu", 1, 1, &engine) == 0,
                     "open attached M4 engine");

    memset(&options, 0, sizeof(options));
    rc = yvex_engine_execute_fixture_graph(engine, &options, &row0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "execute fixture row 0");
    YVEX_TEST_ASSERT(row0.executed == 1, "row 0 executed");
    YVEX_TEST_ASSERT_STREQ(row0.backend_name, "cpu", "row 0 backend");
    YVEX_TEST_ASSERT_STREQ(row0.op_name, "embed", "row 0 op");
    YVEX_TEST_ASSERT_STREQ(row0.weight_name, "token_embd.weight", "row 0 weight");
    YVEX_TEST_ASSERT(row0.node_count == 1, "row 0 node count");
    YVEX_TEST_ASSERT(row0.output_count == 4, "row 0 output count");
    YVEX_TEST_ASSERT(row0.output_bytes == 16, "row 0 output bytes");
    YVEX_TEST_ASSERT(row0.output_value_count == 4, "row 0 output values");
    YVEX_TEST_ASSERT(row0.output_values[0] == 0.0f &&
                     row0.output_values[1] == 4.0f &&
                     row0.output_values[2] == 8.0f &&
                     row0.output_values[3] == 12.0f,
                     "row 0 output constants");
    YVEX_TEST_ASSERT(row0.execution_ready == 0, "row 0 execution false");
    YVEX_TEST_ASSERT(row0.graph_execution_ready == 0, "row 0 graph execution false");

    memset(&options, 0, sizeof(options));
    options.token_id = 1;
    rc = yvex_engine_execute_fixture_graph(engine, &options, &row1, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "execute fixture row 1");
    YVEX_TEST_ASSERT(row1.output_values[0] == 16.0f &&
                     row1.output_values[1] == 20.0f &&
                     row1.output_values[2] == 24.0f &&
                     row1.output_values[3] == 28.0f,
                     "row 1 output constants");
    YVEX_TEST_ASSERT(row0.output_checksum != row1.output_checksum,
                     "different token rows produce different checksums");

    yvex_engine_close(engine);
    return 0;
}

static int test_fixture_graph_failures(void)
{
    const char *path = "build/tests/engine/m4-controlled.gguf";
    yvex_engine *engine = NULL;
    yvex_fixture_graph_options options;
    yvex_fixture_graph_result result;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(emit_controlled_engine_fixture(path) == 0, "emit M4 failure fixture");
    YVEX_TEST_ASSERT(open_attached_engine(path, "cpu", 1, 0, &engine) == 0,
                     "open engine without attachment");
    rc = yvex_engine_execute_fixture_graph(engine, NULL, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE, "fixture execution requires attached weights");
    yvex_engine_close(engine);
    engine = NULL;

    YVEX_TEST_ASSERT(open_attached_engine(path, "cpu", 0, 1, &engine) == 0,
                     "open attached engine without graph");
    rc = yvex_engine_execute_fixture_graph(engine, NULL, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE, "fixture execution requires built graph");
    yvex_engine_close(engine);
    engine = NULL;

    YVEX_TEST_ASSERT(open_attached_engine(path, "cpu", 1, 1, &engine) == 0,
                     "open attached engine for bad token");
    memset(&options, 0, sizeof(options));
    options.token_id = 99;
    rc = yvex_engine_execute_fixture_graph(engine, &options, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad fixture token fails");
    yvex_engine_close(engine);
    return 0;
}

int yvex_test_engine(void)
{
    if (test_engine_opens_fixture() != 0) return 1;
    if (test_engine_errors() != 0) return 1;
    if (test_engine_attaches_weights() != 0) return 1;
    if (test_engine_executes_fixture_graph() != 0) return 1;
    if (test_fixture_graph_failures() != 0) return 1;
    return 0;
}
