/*
 * YVEX - Runtime filesystem tests
 *
 * File: tests/test_fs.c
 * Layer: test
 *
 * Purpose:
 *   Proves runtime filesystem runtime filesystem path resolution, project-local paths, run ID
 *   creation, run-directory preparation, and directory creation behavior.
 *
 * Covers:
 *   - yvex_paths_default
 *   - yvex_paths_project
 *   - yvex_run_id_make
 *   - yvex_run_dir_prepare
 *   - yvex_run_dir_create
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_fs
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <yvex/fs.h>

#include "test.h"

static int ensure_dir(const char *path)
{
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
        return 0;
    }
    fprintf(stderr, "FAIL: mkdir %s: %s\n", path, strerror(errno));
    return 1;
}

static int ensure_test_root(void)
{
    return ensure_dir("build") || ensure_dir("build/tests") || ensure_dir("build/tests/tmp");
}

static char *make_temp_dir(char *template_path)
{
    if (ensure_test_root() != 0) {
        return NULL;
    }
    return mkdtemp(template_path);
}

static void clear_yvex_env(void)
{
    unsetenv("YVEX_CONFIG_DIR");
    unsetenv("YVEX_CACHE_DIR");
    unsetenv("YVEX_STATE_DIR");
    unsetenv("YVEX_DATA_DIR");
    unsetenv("YVEX_RUN_DIR");
}

static int path_is_dir(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return S_ISDIR(st.st_mode) ? 1 : 0;
}

static int test_default_paths(void)
{
    char home_template[] = "build/tests/tmp/homeXXXXXX";
    char expected[YVEX_PATH_CAP * 2];
    char *home;
    yvex_paths paths;
    yvex_error err;
    int rc;

    clear_yvex_env();
    home = make_temp_dir(home_template);
    YVEX_TEST_ASSERT(home != NULL, "mkdtemp home");
    setenv("HOME", home, 1);

    rc = yvex_paths_default(&paths, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "default paths ok");

    snprintf(expected, sizeof(expected), "%s/.config/yvex", home);
    YVEX_TEST_ASSERT_STREQ(paths.config_dir, expected, "default config");
    snprintf(expected, sizeof(expected), "%s/.cache/yvex", home);
    YVEX_TEST_ASSERT_STREQ(paths.cache_dir, expected, "default cache");
    snprintf(expected, sizeof(expected), "%s/.local/state/yvex", home);
    YVEX_TEST_ASSERT_STREQ(paths.state_dir, expected, "default state");
    snprintf(expected, sizeof(expected), "%s/.local/share/yvex", home);
    YVEX_TEST_ASSERT_STREQ(paths.data_dir, expected, "default data");
    YVEX_TEST_ASSERT_STREQ(paths.project_dir, "", "default project empty");
    return 0;
}

static int test_env_overrides(void)
{
    char home_template[] = "build/tests/tmp/envhomeXXXXXX";
    char *home;
    yvex_paths paths;
    yvex_error err;
    int rc;

    clear_yvex_env();
    home = make_temp_dir(home_template);
    YVEX_TEST_ASSERT(home != NULL, "mkdtemp env home");
    setenv("HOME", home, 1);
    setenv("YVEX_CONFIG_DIR", "/tmp/yvex-config-test", 1);
    setenv("YVEX_CACHE_DIR", "/tmp/yvex-cache-test", 1);
    setenv("YVEX_STATE_DIR", "/tmp/yvex-state-test", 1);
    setenv("YVEX_DATA_DIR", "/tmp/yvex-data-test", 1);

    rc = yvex_paths_default(&paths, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "env override paths ok");
    YVEX_TEST_ASSERT_STREQ(paths.config_dir, "/tmp/yvex-config-test", "env config");
    YVEX_TEST_ASSERT_STREQ(paths.cache_dir, "/tmp/yvex-cache-test", "env cache");
    YVEX_TEST_ASSERT_STREQ(paths.state_dir, "/tmp/yvex-state-test", "env state");
    YVEX_TEST_ASSERT_STREQ(paths.data_dir, "/tmp/yvex-data-test", "env data");
    return 0;
}

static int test_missing_home_and_bounds(void)
{
    char long_home[YVEX_PATH_CAP + 256];
    yvex_paths paths;
    yvex_error err;
    int rc;
    size_t i;

    clear_yvex_env();
    unsetenv("HOME");
    rc = yvex_paths_default(&paths, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_STATE, "missing HOME returns state");
    YVEX_TEST_ASSERT_STREQ(yvex_error_where(&err), "yvex_paths_default", "missing HOME where");

    for (i = 0; i < sizeof(long_home) - 1; ++i) {
        long_home[i] = 'x';
    }
    long_home[sizeof(long_home) - 1] = '\0';
    setenv("HOME", long_home, 1);
    rc = yvex_paths_default(&paths, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "long HOME returns bounds");
    return 0;
}

static int test_project_paths(void)
{
    yvex_paths paths;
    yvex_error err;
    int rc;

    clear_yvex_env();
    rc = yvex_paths_project(&paths, ".", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "project paths ok");
    YVEX_TEST_ASSERT_STREQ(paths.project_dir, "./.yvex", "project dir");
    YVEX_TEST_ASSERT_STREQ(paths.config_dir, "./.yvex", "project config dir");
    YVEX_TEST_ASSERT_STREQ(paths.cache_dir, "./.yvex/cache", "project cache dir");
    YVEX_TEST_ASSERT_STREQ(paths.state_dir, "./.yvex/state", "project state dir");
    YVEX_TEST_ASSERT_STREQ(paths.data_dir, "./.yvex/data", "project data dir");
    return 0;
}

static int test_run_id_and_prepare(void)
{
    char project_template[] = "build/tests/tmp/projectXXXXXX";
    char expected[YVEX_PATH_CAP * 2];
    char run_id[YVEX_RUN_ID_CAP];
    char *project;
    yvex_paths paths;
    yvex_run_dir run;
    yvex_error err;
    int rc;

    clear_yvex_env();
    project = make_temp_dir(project_template);
    YVEX_TEST_ASSERT(project != NULL, "mkdtemp project");

    rc = yvex_run_id_make(run_id, sizeof(run_id), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run id ok");
    YVEX_TEST_ASSERT(strncmp(run_id, "run_", 4) == 0, "run id prefix");

    rc = yvex_paths_project(&paths, project, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "project paths for run");
    rc = yvex_run_dir_prepare(&run, &paths, "run_fixed", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run dir prepare");

    snprintf(expected, sizeof(expected), "%s/.yvex/runs/run_fixed", project);
    YVEX_TEST_ASSERT_STREQ(run.root, expected, "run root");
    snprintf(expected, sizeof(expected), "%s/command.txt", run.root);
    YVEX_TEST_ASSERT_STREQ(run.command_path, expected, "command path");
    snprintf(expected, sizeof(expected), "%s/stdout.log", run.root);
    YVEX_TEST_ASSERT_STREQ(run.stdout_path, expected, "stdout path");
    snprintf(expected, sizeof(expected), "%s/stderr.log", run.root);
    YVEX_TEST_ASSERT_STREQ(run.stderr_path, expected, "stderr path");
    snprintf(expected, sizeof(expected), "%s/metrics.json", run.root);
    YVEX_TEST_ASSERT_STREQ(run.metrics_path, expected, "metrics path");
    snprintf(expected, sizeof(expected), "%s/trace.jsonl", run.root);
    YVEX_TEST_ASSERT_STREQ(run.trace_path, expected, "trace path");
    snprintf(expected, sizeof(expected), "%s/receipt.json", run.root);
    YVEX_TEST_ASSERT_STREQ(run.receipt_path, expected, "receipt path");
    return 0;
}

static int test_run_dir_create(void)
{
    char project_template[] = "build/tests/tmp/createXXXXXX";
    char *project;
    yvex_paths paths;
    yvex_run_dir run;
    yvex_run_dir collision;
    yvex_error err;
    FILE *fp;
    int rc;

    clear_yvex_env();
    project = make_temp_dir(project_template);
    YVEX_TEST_ASSERT(project != NULL, "mkdtemp create project");

    rc = yvex_paths_project(&paths, project, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "project paths create");
    rc = yvex_run_dir_prepare(&run, &paths, "run_create", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run prepare create");
    rc = yvex_run_dir_create(&run, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run create");
    YVEX_TEST_ASSERT(path_is_dir(run.root), "run root is directory");
    rc = yvex_run_dir_create(&run, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "run create tolerates existing directory");

    rc = yvex_run_dir_prepare(&collision, &paths, "run_collision", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "collision prepare");
    rc = yvex_run_dir_create(&run, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "ensure parent exists");
    fp = fopen(collision.root, "w");
    YVEX_TEST_ASSERT(fp != NULL, "create collision file");
    fclose(fp);
    rc = yvex_run_dir_create(&collision, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_IO, "file collision rejected");
    YVEX_TEST_ASSERT(yvex_error_is_set(&err), "file collision sets error");
    return 0;
}

int yvex_test_fs(void)
{
    if (test_default_paths() != 0) {
        return 1;
    }
    if (test_env_overrides() != 0) {
        return 1;
    }
    if (test_missing_home_and_bounds() != 0) {
        return 1;
    }
    if (test_project_paths() != 0) {
        return 1;
    }
    if (test_run_id_and_prepare() != 0) {
        return 1;
    }
    if (test_run_dir_create() != 0) {
        return 1;
    }
    return 0;
}
