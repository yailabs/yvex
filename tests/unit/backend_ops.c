/*
 * YVEX - backend op tests
 *
 * File: tests/test_backend_ops.c
 * Layer: test
 *
 * Purpose:
 *   Proves that backend layer CPU backend capabilities are explicit and that the minimal
 *   F32 embedding reference op works over backend tensors.
 *
 * Covers:
 *   - yvex_backend_supports
 *   - yvex_backend_capability_name
 *   - yvex_backend_op_embed
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_ops
 *
 * Expected:
 *   - exits 0 on success
 *   - prints concise failure to stderr
 */
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static void make_desc(yvex_backend_tensor_desc *desc,
                      const char *name,
                      yvex_dtype dtype,
                      unsigned int rank,
                      unsigned long long d0,
                      unsigned long long d1,
                      unsigned long long bytes)
{
    memset(desc, 0, sizeof(*desc));
    desc->name = name;
    desc->dtype = dtype;
    desc->rank = rank;
    desc->dims[0] = d0;
    desc->dims[1] = d1;
    desc->bytes = bytes;
}

static void write_u16le(unsigned char *p, unsigned int value)
{
    p[0] = (unsigned char)(value & 0xffu);
    p[1] = (unsigned char)((value >> 8) & 0xffu);
}

static int test_capabilities(void)
{
    yvex_backend *backend = NULL;
    yvex_error err;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");
    YVEX_TEST_ASSERT_STREQ(yvex_backend_capability_name(YVEX_BACKEND_CAP_TENSOR_ALLOC),
                           "tensor_alloc", "capability name");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_ALLOC),
                     "supports tensor alloc");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE),
                     "supports tensor read/write");
    YVEX_TEST_ASSERT(yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_EMBED),
                     "supports embed");
    YVEX_TEST_ASSERT(!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_MATMUL),
                     "matmul unsupported");
    YVEX_TEST_ASSERT(!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_RMS_NORM),
                     "rms norm unsupported");
    YVEX_TEST_ASSERT(!yvex_backend_supports(backend, YVEX_BACKEND_CAP_OP_ATTENTION),
                     "attention unsupported");
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float embedding_data[12] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        10.0f, 11.0f, 12.0f, 13.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    float out_data[8];
    float expected[8] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    unsigned int token_ids[2] = {0, 2};
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate embedding");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate output");

    rc = yvex_backend_op_embed(backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "embed succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0, "embed output matches expected rows");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_f16_success(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    unsigned char embedding_data[24];
    float out_data[8];
    float expected[8] = {
        0.0f, 1.0f, 2.0f, 3.0f,
        20.0f, 21.0f, 22.0f, 23.0f,
    };
    unsigned int token_ids[2] = {0, 2};
    unsigned int half_values[12] = {
        0x0000u, 0x3c00u, 0x4000u, 0x4200u,
        0x4900u, 0x4980u, 0x4a00u, 0x4a80u,
        0x4d00u, 0x4d40u, 0x4d80u, 0x4dc0u,
    };
    unsigned int i;
    int rc;

    for (i = 0; i < 12u; ++i) {
        write_u16le(embedding_data + (i * 2u), half_values[i]);
    }

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu f16");

    make_desc(&desc, "embedding", YVEX_DTYPE_F16, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate f16 embedding");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write f16 embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate f16 output");

    rc = yvex_backend_op_embed(backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "f16 embed succeeds");
    rc = yvex_backend_tensor_read(backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read f16 embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0,
                     "f16 embed output converts expected rows");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

static int test_embed_failures(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *embedding = NULL;
    yvex_device_tensor *out = NULL;
    yvex_backend_tensor_desc desc;
    yvex_error err;
    float embedding_data[12] = {0};
    unsigned int good_ids[1] = {0};
    unsigned int bad_ids[1] = {9};
    int rc;

    YVEX_TEST_ASSERT(yvex_backend_open_cpu(&backend, &err) == YVEX_OK, "open cpu failures");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate embedding failures");
    rc = yvex_backend_tensor_write(backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write embedding failures");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 1, 4, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate output failures");

    rc = yvex_backend_op_embed(backend, embedding, bad_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad token id fails");

    yvex_backend_tensor_free(backend, out);
    make_desc(&desc, "bad_out", YVEX_DTYPE_F32, 2, 4, 1, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bad output");
    rc = yvex_backend_op_embed(backend, embedding, good_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad output shape fails");
    yvex_backend_tensor_free(backend, out);
    out = NULL;

    yvex_backend_tensor_free(backend, embedding);
    make_desc(&desc, "bad_embedding", YVEX_DTYPE_BF16, 2, 4, 3, 24);
    rc = yvex_backend_tensor_alloc(backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bf16 embedding");
    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 1, 4, 4u * sizeof(float));
    rc = yvex_backend_tensor_alloc(backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate bf16 output");
    rc = yvex_backend_op_embed(backend, embedding, good_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_UNSUPPORTED, "unsupported embedding dtype rejected");

    yvex_backend_tensor_free(backend, out);
    yvex_backend_tensor_free(backend, embedding);
    yvex_backend_close(backend);
    return 0;
}

int yvex_test_backend_ops(void)
{
    if (test_capabilities() != 0) return 1;
    if (test_embed_success() != 0) return 1;
    if (test_embed_f16_success() != 0) return 1;
    if (test_embed_failures() != 0) return 1;
    return 0;
}
