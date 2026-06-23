/*
 * YVEX - run artifact tests
 *
 * File: tests/test_run_artifacts.c
 * Layer: test
 *
 * Purpose:
 *   Proves observability layer run artifact preparation creates deterministic metrics, trace,
 *   profile, and command paths under explicit test directories.
 */
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "yvex_metrics_internal.h"
#include "test.h"

static int exists_file(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int main(void)
{
    yvex_run_artifacts artifacts;
    yvex_error err;
    char *argv[] = { "yvex", "run", "--prompt", "hello world" };
    int rc;

    mkdir("build/tests/tmp", 0777);
    rc = yvex_run_artifacts_prepare(&artifacts, 1, "build/tests/tmp/run-artifacts",
                                    NULL, NULL, NULL, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "prepare save-run artifacts");
    YVEX_TEST_ASSERT(artifacts.has_run_dir, "has run dir");
    YVEX_TEST_ASSERT(artifacts.has_metrics, "has metrics path");
    YVEX_TEST_ASSERT(artifacts.has_trace, "has trace path");
    YVEX_TEST_ASSERT(artifacts.has_profile, "has profile path");
    YVEX_TEST_ASSERT(strstr(artifacts.metrics_path, "metrics.json") != NULL, "metrics filename");

    rc = yvex_run_artifacts_write_command(&artifacts, 4, argv, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write command");
    YVEX_TEST_ASSERT(exists_file(artifacts.command_path), "command file exists");

    rc = yvex_run_artifacts_prepare(&artifacts, 0, NULL,
                                    "build/tests/tmp/explicit-metrics.json",
                                    "build/tests/tmp/explicit-trace.jsonl",
                                    "build/tests/tmp/explicit-profile.json",
                                    &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "prepare explicit artifacts");
    YVEX_TEST_ASSERT(!artifacts.has_run_dir, "no run dir for explicit paths");
    YVEX_TEST_ASSERT_STREQ(artifacts.metrics_path, "build/tests/tmp/explicit-metrics.json",
                           "explicit metrics");
    return 0;
}
