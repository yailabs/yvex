/*
 * YVEX - graph tests
 *
 * File: tests/test_graph.c
 * Layer: test
 *
 * Purpose:
 *   Proves that F0 builds a deterministic partial graph from the current
 *   descriptor fixture and reports missing required tensor roles.
 *
 * Covers:
 *   - yvex_graph_build_for_model
 *   - graph value/op accessors
 *   - missing-role diagnostics
 *   - graph dump
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_graph
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <stdio.h>
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
} graph_fixture;

static int open_fixture(graph_fixture *fixture)
{
    yvex_artifact_options options;
    yvex_error err;
    int rc;

    memset(fixture, 0, sizeof(*fixture));
    memset(&options, 0, sizeof(options));
    options.path = "tests/fixtures/gguf/valid-tokenizer-simple.gguf";
    options.readonly = 1;

    rc = yvex_artifact_open(&fixture->artifact, &options, &err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&fixture->gguf, fixture->artifact, &err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&fixture->table, fixture->gguf, &err);
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&fixture->model, fixture->gguf, fixture->table, &err);
    }
    if (rc != YVEX_OK) {
        fprintf(stderr, "fixture open failed: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        yvex_model_descriptor_close(fixture->model);
        yvex_tensor_table_close(fixture->table);
        yvex_gguf_close(fixture->gguf);
        yvex_artifact_close(fixture->artifact);
        return 1;
    }
    return 0;
}

static void close_fixture(graph_fixture *fixture)
{
    yvex_model_descriptor_close(fixture->model);
    yvex_tensor_table_close(fixture->table);
    yvex_gguf_close(fixture->gguf);
    yvex_artifact_close(fixture->artifact);
    memset(fixture, 0, sizeof(*fixture));
}

static int file_contains(const char *path, const char *needle)
{
    FILE *fp = fopen(path, "rb");
    char buf[8192];
    size_t n;
    int found = 0;

    if (!fp) {
        return 0;
    }
    n = fread(buf, 1, sizeof(buf) - 1u, fp);
    buf[n] = '\0';
    fclose(fp);
    found = strstr(buf, needle) != NULL;
    return found;
}

static int test_graph_from_fixture(void)
{
    graph_fixture fixture;
    yvex_graph *graph = NULL;
    yvex_graph_build_options options;
    const yvex_graph_value_info *value;
    const yvex_graph_op_info *op;
    const yvex_graph_missing_required *missing;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.sequence_length = 1;
    options.include_prefill_path = 1;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open graph fixture");
    rc = yvex_graph_build_for_model(&graph, fixture.model, fixture.table, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "graph builds");

    YVEX_TEST_ASSERT(yvex_graph_status_of(graph) == YVEX_GRAPH_STATUS_PARTIAL, "graph is partial");
    YVEX_TEST_ASSERT(yvex_graph_value_count(graph) == 3, "graph value count");
    YVEX_TEST_ASSERT(yvex_graph_op_count(graph) == 1, "graph op count");
    YVEX_TEST_ASSERT(yvex_graph_missing_required_count(graph) == 2, "missing required count");

    value = yvex_graph_value_at(graph, 0);
    YVEX_TEST_ASSERT(value != NULL, "value 0 exists");
    YVEX_TEST_ASSERT(value->kind == YVEX_VALUE_TOKEN_IDS, "value 0 token ids");
    YVEX_TEST_ASSERT(value->rank == 1 && value->dims[0] == 1, "token ids shape");

    value = yvex_graph_value_at(graph, 1);
    YVEX_TEST_ASSERT(value != NULL, "value 1 exists");
    YVEX_TEST_ASSERT(value->kind == YVEX_VALUE_WEIGHT, "value 1 weight");
    YVEX_TEST_ASSERT_STREQ(value->name, "token_embd.weight", "weight value name");
    YVEX_TEST_ASSERT(value->rank == 2 && value->dims[0] == 4 && value->dims[1] == 8, "weight dims");

    value = yvex_graph_value_at(graph, 2);
    YVEX_TEST_ASSERT(value != NULL, "value 2 exists");
    YVEX_TEST_ASSERT(value->kind == YVEX_VALUE_ACTIVATION, "value 2 activation");
    YVEX_TEST_ASSERT(value->rank == 2 && value->dims[0] == 1 && value->dims[1] == 4,
                     "hidden activation shape");

    op = yvex_graph_op_at(graph, 0);
    YVEX_TEST_ASSERT(op != NULL, "op 0 exists");
    YVEX_TEST_ASSERT(op->kind == YVEX_OP_EMBED, "embed op");
    YVEX_TEST_ASSERT(op->status == YVEX_OP_STATUS_PLANNED, "embed planned");

    missing = yvex_graph_missing_required_at(graph, 0);
    YVEX_TEST_ASSERT(missing != NULL, "missing 0 exists");
    YVEX_TEST_ASSERT(missing->role == YVEX_TENSOR_ROLE_OUTPUT_NORM, "missing output norm");
    missing = yvex_graph_missing_required_at(graph, 1);
    YVEX_TEST_ASSERT(missing != NULL, "missing 1 exists");
    YVEX_TEST_ASSERT(missing->role == YVEX_TENSOR_ROLE_OUTPUT_HEAD, "missing output head");

    yvex_graph_close(graph);
    close_fixture(&fixture);
    return 0;
}

static int test_graph_dump(void)
{
    graph_fixture fixture;
    yvex_graph *graph = NULL;
    yvex_error err;
    FILE *fp;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open graph fixture for dump");
    rc = yvex_graph_build_for_model(&graph, fixture.model, fixture.table, NULL, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "graph builds for dump");

    fp = fopen("build/tests/test_graph_dump.out", "wb");
    YVEX_TEST_ASSERT(fp != NULL, "open graph dump output");
    rc = yvex_graph_dump(graph, fp, &err);
    fclose(fp);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "graph dump succeeds");
    YVEX_TEST_ASSERT(file_contains("build/tests/test_graph_dump.out", "graph status: partial"),
                     "dump status line");
    YVEX_TEST_ASSERT(file_contains("build/tests/test_graph_dump.out", "op 0 embed status=planned"),
                     "dump embed op");
    YVEX_TEST_ASSERT(file_contains("build/tests/test_graph_dump.out", "missing output_head"),
                     "dump missing output head");

    yvex_graph_close(graph);
    close_fixture(&fixture);
    return 0;
}

int main(void)
{
    if (test_graph_from_fixture() != 0) return 1;
    if (test_graph_dump() != 0) return 1;
    return 0;
}
