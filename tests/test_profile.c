/*
 * YVEX - profile JSON tests
 *
 * File: tests/test_profile.c
 * Layer: test
 *
 * Purpose:
 *   Proves J0 metrics/profile JSON writers persist accepted-token counters
 *   and omit fake decode/generation benchmark fields.
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
    yvex_metrics *metrics = NULL;
    yvex_profile_summary profile;
    yvex_error err;
    char buf[8192];
    int rc;

    mkdir("build/tests/tmp", 0777);

    rc = yvex_metrics_create(&metrics, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "metrics create");
    (void)yvex_metrics_add_prompt_tokens(metrics, 3, &err);
    (void)yvex_metrics_add_accepted_tokens(metrics, 3, &err);
    (void)yvex_metrics_set_model_bytes(metrics, 128, 0, &err);

    rc = yvex_metrics_write_json("build/tests/tmp/metrics.json", metrics, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "metrics json");
    YVEX_TEST_ASSERT(read_file("build/tests/tmp/metrics.json", buf, sizeof(buf)), "read metrics");
    YVEX_TEST_ASSERT(strstr(buf, "\"schema\": \"yvex.metrics.v1\"") != NULL, "metrics schema");
    YVEX_TEST_ASSERT(strstr(buf, "\"accepted_tokens\": 3") != NULL, "accepted counter");
    YVEX_TEST_ASSERT(strstr(buf, "decode_" "tps") == NULL, "no decode tps");
    YVEX_TEST_ASSERT(strstr(buf, "generated_" "tokens") == NULL, "no generated tokens");

    memset(&profile, 0, sizeof(profile));
    profile.run_id = "run_test";
    profile.command = "run";
    profile.model_name = "yvex-tokenizer-test";
    profile.backend_name = "cpu";
    profile.status = "accepted-only";
    profile.execution_ready = 0;
    rc = yvex_profile_write_json("build/tests/tmp/profile.json", &profile, metrics, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "profile json");
    YVEX_TEST_ASSERT(read_file("build/tests/tmp/profile.json", buf, sizeof(buf)), "read profile");
    YVEX_TEST_ASSERT(strstr(buf, "\"schema\": \"yvex.profile.v1\"") != NULL, "profile schema");
    YVEX_TEST_ASSERT(strstr(buf, "\"generation\": \"unsupported\"") != NULL, "generation unsupported");
    YVEX_TEST_ASSERT(strstr(buf, "tokens_per_" "second") == NULL, "no tps");

    yvex_metrics_close(metrics);
    return 0;
}
