/*
 * YVEX - CUDA op tests
 *
 * File: tests/test_cuda_ops.c
 * Layer: test
 *
 * Purpose:
 *   Proves that L0 CUDA supports the same minimal F32 embedding op as the G0
 *   CPU reference backend. Returns 77 when CUDA is unavailable.
 */
#include <stdio.h>
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

static int open_cuda_or_skip(yvex_backend **out)
{
    yvex_backend_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(out, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open cuda backend");
    return 0;
}

int main(void)
{
    yvex_backend *cuda_backend = NULL;
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
    unsigned int bad_ids[1] = {9};
    int rc;

    rc = open_cuda_or_skip(&cuda_backend);
    if (rc == 77) return 77;

    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_TENSOR_ALLOC),
                     "cuda tensor alloc supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_TENSOR_READ_WRITE),
                     "cuda tensor rw supported");
    YVEX_TEST_ASSERT(yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_EMBED),
                     "cuda embed supported");
    YVEX_TEST_ASSERT(!yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_MATMUL),
                     "cuda matmul unsupported");
    YVEX_TEST_ASSERT(!yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_RMS_NORM),
                     "cuda rmsnorm unsupported");
    YVEX_TEST_ASSERT(!yvex_backend_supports(cuda_backend, YVEX_BACKEND_CAP_OP_ATTENTION),
                     "cuda attention unsupported");

    make_desc(&desc, "embedding", YVEX_DTYPE_F32, 2, 4, 3, sizeof(embedding_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &embedding, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda embedding");
    rc = yvex_backend_tensor_write(cuda_backend, embedding, embedding_data, sizeof(embedding_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda embedding");

    make_desc(&desc, "out", YVEX_DTYPE_F32, 2, 2, 4, sizeof(out_data));
    rc = yvex_backend_tensor_alloc(cuda_backend, &desc, &out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda output");
    rc = yvex_backend_op_embed(cuda_backend, embedding, token_ids, 2, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "cuda embed succeeds");
    rc = yvex_backend_tensor_read(cuda_backend, out, out_data, sizeof(out_data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda embed output");
    YVEX_TEST_ASSERT(memcmp(out_data, expected, sizeof(expected)) == 0,
                     "cuda embed output matches expected rows");

    rc = yvex_backend_op_embed(cuda_backend, embedding, bad_ids, 1, out, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_BOUNDS, "bad token id fails");

    yvex_backend_tensor_free(cuda_backend, out);
    yvex_backend_tensor_free(cuda_backend, embedding);
    yvex_backend_close(cuda_backend);
    return 0;
}
