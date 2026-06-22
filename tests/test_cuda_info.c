/*
 * YVEX - CUDA info tests
 *
 * File: tests/test_cuda_info.c
 * Layer: test
 *
 * Purpose:
 *   Proves that the L0 CUDA device probe opens a real CUDA backend when the
 *   local driver/device is available. Returns 77 when CUDA is unavailable.
 */
#include <stdio.h>

#include <yvex/yvex.h>

#include "test.h"

int main(void)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_backend_device_info info;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open cuda backend");
    rc = yvex_backend_get_device_info(backend, &info, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK, "get cuda device info");
    YVEX_TEST_ASSERT(info.kind == YVEX_BACKEND_KIND_CUDA, "device info kind");
    YVEX_TEST_ASSERT(info.name && info.name[0] != '\0', "device name non-empty");
    YVEX_TEST_ASSERT(info.compute_capability_major >= 1, "compute capability major");
    YVEX_TEST_ASSERT(info.global_memory_bytes > 0, "global memory nonzero");
    YVEX_TEST_ASSERT(info.total_memory_bytes > 0, "total memory nonzero");
    yvex_backend_close(backend);
    return 0;
}
