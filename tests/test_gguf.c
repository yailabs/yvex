/*
 * YVEX - GGUF header tests
 *
 * File: tests/test_gguf.c
 * Layer: test
 *
 * Purpose:
 *   Proves C0 GGUF magic probing and fixed header parsing on tiny fixtures.
 *   Metadata, tensor directory, tokenizer, and model execution are not tested
 *   because they are not implemented in C0.
 *
 * Covers:
 *   - yvex_gguf_probe_file
 *   - yvex_gguf_read_header
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_gguf
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <string.h>

#include <yvex/artifact.h>
#include <yvex/gguf.h>

#include "test.h"

static int open_fixture(const char *path, yvex_artifact **out)
{
    yvex_artifact_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;

    rc = yvex_artifact_open(out, &options, &err);
    if (rc != YVEX_OK) {
        fprintf(stderr, "open fixture failed: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    return 0;
}

static int test_valid_minimal(void)
{
    yvex_artifact *artifact;
    yvex_gguf_header header;
    yvex_gguf_probe probe;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture("tests/fixtures/gguf/valid-minimal.gguf", &artifact) == 0, "open valid fixture");

    rc = yvex_gguf_read_header(artifact, &header, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "valid header parses");
    YVEX_TEST_ASSERT(header.version == 3, "valid version");
    YVEX_TEST_ASSERT(header.tensor_count == 0, "valid tensor count");
    YVEX_TEST_ASSERT(header.metadata_count == 0, "valid metadata count");

    rc = yvex_gguf_probe_file(artifact, &probe, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "valid probe ok");
    YVEX_TEST_ASSERT(probe.is_gguf == 1, "valid probe is gguf");
    YVEX_TEST_ASSERT(probe.header.version == 3, "probe version");

    yvex_artifact_close(artifact);
    return 0;
}

static int test_bad_magic(void)
{
    yvex_artifact *artifact;
    yvex_gguf_header header;
    yvex_gguf_probe probe;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture("tests/fixtures/gguf/bad-magic.gguf", &artifact) == 0, "open bad magic fixture");

    rc = yvex_gguf_probe_file(artifact, &probe, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "bad magic probe ok");
    YVEX_TEST_ASSERT(probe.is_gguf == 0, "bad magic probe unknown");

    rc = yvex_gguf_read_header(artifact, &header, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT, "bad magic read format error");

    yvex_artifact_close(artifact);
    return 0;
}

static int test_short_header(void)
{
    yvex_artifact *artifact;
    yvex_gguf_header header;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture("tests/fixtures/gguf/short-header.gguf", &artifact) == 0, "open short fixture");
    rc = yvex_gguf_read_header(artifact, &header, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT, "short header format error");
    yvex_artifact_close(artifact);
    return 0;
}

static int test_unsupported_version(void)
{
    yvex_artifact *artifact;
    yvex_gguf_header header;
    yvex_error err;
    int rc;

    YVEX_TEST_ASSERT(open_fixture("tests/fixtures/gguf/unsupported-version.gguf", &artifact) == 0, "open unsupported fixture");
    rc = yvex_gguf_read_header(artifact, &header, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "unsupported version");
    yvex_artifact_close(artifact);
    return 0;
}

int main(void)
{
    if (test_valid_minimal() != 0) {
        return 1;
    }
    if (test_bad_magic() != 0) {
        return 1;
    }
    if (test_short_header() != 0) {
        return 1;
    }
    if (test_unsupported_version() != 0) {
        return 1;
    }
    return 0;
}
