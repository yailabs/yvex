/*
 * YVEX - trace tests
 *
 * File: tests/test_trace.c
 * Layer: test
 *
 * Purpose:
 *   Proves the J0 trace writer emits JSONL event records with schema, event,
 *   and incrementing sequence fields.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <yvex/yvex.h>

#include "test.h"

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "r");
    size_t n;

    if (!fp) {
        return 0;
    }
    n = fread(buf, 1u, cap - 1u, fp);
    buf[n] = '\0';
    fclose(fp);
    return 1;
}

int main(void)
{
    yvex_trace *trace = NULL;
    yvex_trace_options options;
    yvex_error err;
    char buf[4096];
    int rc;

    mkdir("build/tests/tmp", 0777);

    memset(&options, 0, sizeof(options));
    rc = yvex_trace_open(&trace, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "disabled trace open");
    rc = yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "disabled trace emit");
    yvex_trace_close(trace);

    memset(&options, 0, sizeof(options));
    options.enabled = 1;
    options.path = "build/tests/tmp/trace.jsonl";
    options.run_id = "run_test";
    rc = yvex_trace_open(&trace, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "trace open");
    rc = yvex_trace_emit(trace, YVEX_TRACE_EVENT_RUN_START, "run", "started", "hello \"json\"", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run_start emit");
    rc = yvex_trace_emit(trace, YVEX_TRACE_EVENT_PHASE_END, "tokenize", "ok", "", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "phase_end emit");
    yvex_trace_close(trace);

    YVEX_TEST_ASSERT(read_file("build/tests/tmp/trace.jsonl", buf, sizeof(buf)), "read trace");
    YVEX_TEST_ASSERT(strstr(buf, "\"schema\": \"yvex.trace.v1\"") != NULL, "schema present");
    YVEX_TEST_ASSERT(strstr(buf, "\"event\": \"run_start\"") != NULL, "event present");
    YVEX_TEST_ASSERT(strstr(buf, "\"seq\": 1") != NULL, "seq 1");
    YVEX_TEST_ASSERT(strstr(buf, "\"seq\": 2") != NULL, "seq 2");
    YVEX_TEST_ASSERT(strstr(buf, "hello \\\"json\\\"") != NULL, "escaped string");
    return 0;
}
