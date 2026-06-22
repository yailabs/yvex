/*
 * YVEX - memory plan tests
 *
 * File: tests/test_memory_plan.c
 * Layer: test
 *
 * Purpose:
 *   Proves that F0 memory plans are estimate-only summaries derived from
 *   descriptor facts and graph shapes, without allocating backend memory.
 *
 * Covers:
 *   - yvex_memory_plan_from_graph
 *   - yvex_memory_plan_get_summary
 *   - yvex_memory_plan_dump
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_memory_plan
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
    yvex_graph *graph;
} memory_fixture;

static int open_fixture(memory_fixture *fixture)
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
    if (rc == YVEX_OK) {
        rc = yvex_graph_build_for_model(&fixture->graph, fixture->model, fixture->table, NULL, &err);
    }
    if (rc != YVEX_OK) {
        fprintf(stderr, "memory fixture failed: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        yvex_graph_close(fixture->graph);
        yvex_model_descriptor_close(fixture->model);
        yvex_tensor_table_close(fixture->table);
        yvex_gguf_close(fixture->gguf);
        yvex_artifact_close(fixture->artifact);
        return 1;
    }
    return 0;
}

static void close_fixture(memory_fixture *fixture)
{
    yvex_graph_close(fixture->graph);
    yvex_model_descriptor_close(fixture->model);
    yvex_tensor_table_close(fixture->table);
    yvex_gguf_close(fixture->gguf);
    yvex_artifact_close(fixture->artifact);
}

static int test_memory_plan_summary(void)
{
    memory_fixture fixture;
    yvex_memory_plan *plan = NULL;
    yvex_memory_plan_summary summary;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open memory fixture");
    rc = yvex_memory_plan_from_graph(&plan, fixture.graph, fixture.table, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory plan builds");
    YVEX_TEST_ASSERT(yvex_memory_plan_status_of(plan) == YVEX_MEMORY_PLAN_PARTIAL,
                     "memory plan partial");

    rc = yvex_memory_plan_get_summary(plan, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory summary succeeds");
    YVEX_TEST_ASSERT(summary.model_tensor_bytes_known == 128, "known model bytes");
    YVEX_TEST_ASSERT(summary.model_tensor_bytes_unknown_count == 0, "unknown byte count");
    YVEX_TEST_ASSERT(summary.activation_peak_bytes == 16, "activation peak bytes");
    YVEX_TEST_ASSERT(summary.kv_cache_bytes == 0, "kv deferred");
    YVEX_TEST_ASSERT(summary.scratch_peak_bytes == 0, "scratch deferred");
    YVEX_TEST_ASSERT(summary.total_known_bytes == 144, "total known bytes");

    yvex_memory_plan_close(plan);
    close_fixture(&fixture);
    return 0;
}

static int test_memory_plan_dump(void)
{
    memory_fixture fixture;
    yvex_memory_plan *plan = NULL;
    yvex_error err;
    FILE *fp;
    int rc;

    YVEX_TEST_ASSERT(open_fixture(&fixture) == 0, "open memory fixture for dump");
    rc = yvex_memory_plan_from_graph(&plan, fixture.graph, fixture.table, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory plan builds for dump");

    fp = fopen("build/tests/test_memory_plan_dump.out", "wb");
    YVEX_TEST_ASSERT(fp != NULL, "open memory dump file");
    rc = yvex_memory_plan_dump(plan, fp, &err);
    fclose(fp);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "memory dump succeeds");

    yvex_memory_plan_close(plan);
    close_fixture(&fixture);
    return 0;
}

int main(void)
{
    if (test_memory_plan_summary() != 0) return 1;
    if (test_memory_plan_dump() != 0) return 1;
    return 0;
}
