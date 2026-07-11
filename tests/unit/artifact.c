/*
 * YVEX - Artifact tests
 *
 * File: tests/test_artifact.c
 * Layer: test
 *
 * Purpose:
 *   Proves artifact layer artifact opening and range checking against tiny checked-in
 *   fixtures. No model downloads or real model files are required.
 *
 * Covers:
 *   - yvex_artifact_open
 *   - yvex_artifact_close
 *   - yvex_artifact_path
 *   - yvex_artifact_size
 *   - yvex_artifact_data
 *   - yvex_range_check
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_artifact
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <limits.h>
#include <string.h>

#include <yvex/artifact.h>

#include "test.h"

int yvex_test_artifact(void)
{
    const char *fixture = "tests/fixtures/gguf/valid-minimal.gguf";
    yvex_artifact_options options;
    yvex_artifact *artifact;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.path = fixture;
    options.readonly = 1;
    options.map = 1;

    artifact = NULL;
    rc = yvex_artifact_open(&artifact, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open valid artifact");
    YVEX_TEST_ASSERT(artifact != NULL, "artifact non-null");
    YVEX_TEST_ASSERT_STREQ(yvex_artifact_path(artifact), fixture, "artifact path");
    YVEX_TEST_ASSERT(yvex_artifact_size(artifact) == 32, "artifact size");
    YVEX_TEST_ASSERT(yvex_artifact_is_mapped(artifact) == 1, "artifact mapping explicit");
    YVEX_TEST_ASSERT(yvex_artifact_data(artifact) != NULL, "artifact data");
    yvex_artifact_close(artifact);

    artifact = NULL;
    options.map = 0;
    rc = yvex_artifact_open(&artifact, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open file-backed artifact");
    YVEX_TEST_ASSERT(yvex_artifact_is_mapped(artifact) == 0, "file-backed artifact not mapped");
    YVEX_TEST_ASSERT(yvex_artifact_data(artifact) == NULL, "unmapped artifact has no payload pointer");
    {
        unsigned char magic[4];
        rc = yvex_artifact_read_at(artifact, 0ull, magic, sizeof(magic), &err);
        YVEX_TEST_ASSERT(rc == YVEX_OK, "positioned artifact read");
        YVEX_TEST_ASSERT(memcmp(magic, "GGUF", sizeof(magic)) == 0, "positioned bytes");
    }
    yvex_artifact_close(artifact);

    artifact = NULL;
    options.readonly = 0;
    rc = yvex_artifact_open(&artifact, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "writable artifact mode refused");
    YVEX_TEST_ASSERT(artifact == NULL, "writable refusal leaves artifact null");
    options.readonly = 1;

    artifact = NULL;
    options.path = "tests/fixtures/gguf/missing.gguf";
    rc = yvex_artifact_open(&artifact, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_IO, "missing file returns IO");
    YVEX_TEST_ASSERT(artifact == NULL, "missing artifact null");

    options.path = NULL;
    rc = yvex_artifact_open(&artifact, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "null path invalid arg");

    rc = yvex_artifact_open(NULL, &options, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_INVALID_ARG, "null out invalid arg");

    rc = yvex_range_check(24, 0, 24, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "full range ok");
    rc = yvex_range_check(24, 24, 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "zero range at EOF ok");
    rc = yvex_range_check(24, 25, 0, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "offset out of bounds");
    rc = yvex_range_check(24, 23, 2, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "range out of bounds");
    rc = yvex_range_check(ULLONG_MAX, ULLONG_MAX, 1, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "overflow-like range rejected");

    yvex_artifact_close(NULL);
    return 0;
}
