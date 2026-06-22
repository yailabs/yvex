/*
 * YVEX - CUDA tensor tests
 *
 * File: tests/test_cuda_tensor.c
 * Layer: test
 *
 * Purpose:
 *   Proves CUDA tensor allocation, zero-read, write/read, copy, and memory
 *   accounting when CUDA is available. Returns 77 when CUDA is unavailable.
 */
#include <stdio.h>
#include <string.h>

#include <yvex/yvex.h>

#include "test.h"

static void make_desc(yvex_backend_tensor_desc *desc,
                      const char *name,
                      unsigned long long d0,
                      unsigned long long d1)
{
    memset(desc, 0, sizeof(*desc));
    desc->name = name;
    desc->dtype = YVEX_DTYPE_F32;
    desc->rank = 2;
    desc->dims[0] = d0;
    desc->dims[1] = d1;
    desc->bytes = d0 * d1 * (unsigned long long)sizeof(float);
}

int main(void)
{
    yvex_backend *backend = NULL;
    yvex_device_tensor *a = NULL;
    yvex_device_tensor *b = NULL;
    yvex_backend_options options;
    yvex_backend_tensor_desc desc;
    yvex_backend_memory_stats stats;
    yvex_error err;
    float data[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    float out[4] = {9.0f, 9.0f, 9.0f, 9.0f};
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open cuda backend");

    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "initial stats");
    YVEX_TEST_ASSERT(stats.allocated_bytes == 0, "initial allocated bytes");

    make_desc(&desc, "cuda_a", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda tensor a");
    rc = yvex_backend_tensor_read(backend, a, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read zero initialized");
    YVEX_TEST_ASSERT(out[0] == 0.0f && out[3] == 0.0f, "zero initialized values");

    rc = yvex_backend_tensor_write(backend, a, data, sizeof(data), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "write cuda tensor");
    memset(out, 0, sizeof(out));
    rc = yvex_backend_tensor_read(backend, a, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read cuda tensor");
    YVEX_TEST_ASSERT(memcmp(data, out, sizeof(data)) == 0, "read equals written");

    make_desc(&desc, "cuda_b", 2, 2);
    rc = yvex_backend_tensor_alloc(backend, &desc, &b, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "allocate cuda tensor b");
    rc = yvex_backend_tensor_copy(backend, b, a, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "copy cuda tensor");
    memset(out, 0, sizeof(out));
    rc = yvex_backend_tensor_read(backend, b, out, sizeof(out), &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "read copied cuda tensor");
    YVEX_TEST_ASSERT(memcmp(data, out, sizeof(data)) == 0, "copy equals source");

    yvex_backend_tensor_free(backend, b);
    yvex_backend_tensor_free(backend, a);
    rc = yvex_backend_get_memory_stats(backend, &stats, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "final stats");
    YVEX_TEST_ASSERT(stats.allocated_bytes == 0, "allocated bytes return to zero");
    YVEX_TEST_ASSERT(stats.allocation_count == 0, "allocation count returns to zero");
    yvex_backend_close(backend);
    return 0;
}
