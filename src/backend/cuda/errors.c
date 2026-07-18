/*
 * Owner: backend.cuda.errors (backend.cuda).
 * Owns: the reusable-algorithm boundary consumed by backend,graph.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=backend and visibility=private match config/source_owners.tsv.
 * Boundary: reusable-algorithm; moving this contract requires an ownership-manifest change.
 *
 * cuda/cuda_errors.c - CUDA driver error mapping.
 *
 * This file maps CUDA Driver API status codes into YVEX errors without
 * exposing CUDA headers outside the CUDA boundary.
 */

#include "driver.h"
#include <stdio.h>


int yvex_cuda_status(const yvex_cuda_driver *driver,
                     CUresult code,
                     const char *where,
                     yvex_error *err)
{
    const char *name = NULL;
    const char *message = NULL;
    yvex_status status = YVEX_ERR_BACKEND;

    if (code == YVEX_CUDA_SUCCESS) {
        yvex_error_clear(err);
        return YVEX_OK;
    }

    if (driver && driver->cuGetErrorName) {
        (void)driver->cuGetErrorName(code, &name);
    }
    if (driver && driver->cuGetErrorString) {
        (void)driver->cuGetErrorString(code, &message);
    }
    if (!name) {
        name = "CUDA_ERROR_UNKNOWN";
    }
    if (!message) {
        message = "no CUDA error string available";
    }

    switch (code) {
    case YVEX_CUDA_ERROR_OUT_OF_MEMORY:
        status = YVEX_ERR_NOMEM;
        break;
    case YVEX_CUDA_ERROR_NO_DEVICE:
    case YVEX_CUDA_ERROR_NOT_SUPPORTED:
        status = YVEX_ERR_UNSUPPORTED;
        break;
    case YVEX_CUDA_ERROR_INVALID_VALUE:
        status = YVEX_ERR_INVALID_ARG;
        break;
    default:
        status = YVEX_ERR_BACKEND;
        break;
    }

    yvex_error_setf(err, status, where ? where : "cuda",
                    "CUDA error %d (%s): %s", code, name, message);
    return status;
}
