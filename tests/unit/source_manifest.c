/*
 * Verifies open-weight intake source provenance scanning and manifest writing over a tiny fake
 * source tree. No external model files are required or committed.
 */
#include "tests/test.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/source.h>
#include <yvex/internal/source_payload.h>

static int make_dir(const char *path)
{
    if (mkdir(path, 0777) != 0 && errno != EEXIST) {
        return 0;
    }
    return 1;
}

static int write_text(const char *path, const char *text)
{
    FILE *fp = fopen(path, "wb");

    if (!fp) {
        return 0;
    }
    fputs(text, fp);
    return fclose(fp) == 0;
}

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (!fp || cap == 0) {
        if (fp) {
            fclose(fp);
        }
        return 0;
    }
    n = fread(buf, 1, cap - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    return 1;
}

static int contains_text(const char *haystack, const char *needle)
{
    return strstr(haystack, needle) != NULL;
}

static int source_report_selection(void)
{
    yvex_source_report_request request = {0};
    yvex_error err;
    request.family = "deepseek";
    request.release = "v0.1.0";
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(&request, &err) == YVEX_OK,
                     "canonical report family is admitted");
    YVEX_TEST_ASSERT_STREQ(request.target, YVEX_SOURCE_RELEASE_TARGET_ID,
                           "DeepSeek default stays source-authored");
    memset(&request, 0, sizeof(request));
    request.family = "qwen";
    request.release = "v0.1.0";
    request.source = "/synthetic/source/qwen3-14b";
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(&request, &err) == YVEX_OK,
                     "family path selection is shared by product consumers");
    YVEX_TEST_ASSERT_STREQ(request.target, "qwen3-14b", "derived family target");
    YVEX_TEST_ASSERT(request.target == request.resolved_target, "derived target borrows caller storage");
    request.target = "qwen3-8b";
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(&request, &err) == YVEX_OK,
                     "explicit target remains admitted");
    YVEX_TEST_ASSERT_STREQ(request.target, "qwen3-8b", "explicit target is never replaced by basename");
    request.strict = 1;
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(&request, &err) == YVEX_ERR_INVALID_ARG,
                     "strict verification does not invent Qwen qualification");
    request.strict = 0;
    request.release = "v0.2.0";
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(&request, &err) == YVEX_ERR_INVALID_ARG,
                     "unadmitted report release refuses");
    YVEX_TEST_ASSERT(yvex_source_report_request_prepare(NULL, &err) == YVEX_ERR_INVALID_ARG,
                     "null selection refuses before dereference");
    return 0;
}

int yvex_test_source_manifest(void)
{
    const char *root = "build/tests/source_manifest_fixture";
    const char *manifest = "build/tests/source_manifest_fixture/manifest.json";
    yvex_source_manifest_summary summary;
    yvex_source_manifest_options options;
    yvex_error err;
    char json[8192];
    int rc;

    YVEX_TEST_ASSERT(source_report_selection() == 0, "shared report selection controls");

    YVEX_TEST_ASSERT(system("rm -rf build/tests/source_manifest_fixture") == 0,
                     "clear source manifest fixture");
    YVEX_TEST_ASSERT(make_dir("build"), "make build");
    YVEX_TEST_ASSERT(make_dir("build/tests"), "make build/tests");
    YVEX_TEST_ASSERT(make_dir(root), "make source manifest fixture");
    YVEX_TEST_ASSERT(write_text("build/tests/source_manifest_fixture/config.json", "{}\n"), "write config");
    YVEX_TEST_ASSERT(write_text("build/tests/source_manifest_fixture/tokenizer.json", "{}\n"), "write tokenizer");
    YVEX_TEST_ASSERT(write_text("build/tests/source_manifest_fixture/README.md", "fixture\n"), "write readme");
    YVEX_TEST_ASSERT(write_text("build/tests/source_manifest_fixture/model-00001-of-00002.safetensors", "abc"), "write safetensors 1");
    YVEX_TEST_ASSERT(write_text("build/tests/source_manifest_fixture/model-00002-of-00002.safetensors", "defg"), "write safetensors 2");

    yvex_error_clear(&err);
    memset(&summary, 0, sizeof(summary));
    rc = yvex_source_manifest_scan_local(root, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "scan local path succeeds");
    YVEX_TEST_ASSERT(summary.file_count == 5, "scan counts files");
    YVEX_TEST_ASSERT(summary.safetensors_count == 2, "scan counts safetensors");
    YVEX_TEST_ASSERT(summary.total_size_bytes > 0, "scan totals size");
    YVEX_TEST_ASSERT(summary.has_config, "scan detects config");
    YVEX_TEST_ASSERT(summary.has_tokenizer, "scan detects tokenizer");
    YVEX_TEST_ASSERT(summary.has_safetensors, "scan detects safetensors");

    memset(&options, 0, sizeof(options));
    options.repo = "test-org/test-model";
    options.revision = "test-rev";
    options.license = "test-license";
    options.model_card = "https://example.invalid/test-model";
    options.local_path = root;
    options.node_name = "test-node";
    options.status = YVEX_SOURCE_STATUS_IN_PROGRESS;
    options.include_files = 1;

    memset(&summary, 0, sizeof(summary));
    rc = yvex_source_manifest_write_json(manifest, &options, &summary, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write manifest JSON");
    YVEX_TEST_ASSERT(read_file(manifest, json, sizeof(json)), "manifest file exists");
    YVEX_TEST_ASSERT(contains_text(json, "\"schema\": \"yvex.source_manifest.v1\""), "manifest has schema");
    YVEX_TEST_ASSERT(contains_text(json, "\"repo\": \"test-org/test-model\""), "manifest has repo");
    YVEX_TEST_ASSERT(contains_text(json, "\"path\": \"build/tests/source_manifest_fixture\""), "manifest has local path");
    YVEX_TEST_ASSERT(contains_text(json, "\"status\": \"in-progress\""), "manifest has status");
    YVEX_TEST_ASSERT(contains_text(json, "\"path\": \"config.json\""), "manifest lists relative config path");
    YVEX_TEST_ASSERT(contains_text(json, "\"path\": \"model-00001-of-00002.safetensors\""), "manifest lists relative safetensors path");
    YVEX_TEST_ASSERT(contains_text(json, "\"sha256\": null"), "manifest uses null sha256");

    rc = yvex_source_manifest_scan_local("build/tests/source_manifest_fixture/missing", &summary, &err);
    YVEX_TEST_ASSERT(rc != YVEX_OK, "missing local path returns error");

    rc = yvex_source_manifest_write_json(NULL, &options, NULL, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "invalid args return error");

    YVEX_TEST_ASSERT_STREQ(yvex_source_status_name(YVEX_SOURCE_STATUS_COMPLETE), "complete", "source status name");
    return 0;
}
