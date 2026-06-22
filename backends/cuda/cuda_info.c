/*
 * YVEX - CUDA driver probing
 *
 * File: backends/cuda/cuda_info.c
 * Layer: CUDA backend implementation
 *
 * Purpose:
 *   Loads the CUDA Driver API dynamically and exposes helper routines used by
 *   the L0 backend and CLI probing paths.
 */
#include "cuda_internal.h"

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

static int load_symbol(void *library, void **slot, const char *name, yvex_error *err)
{
    *slot = dlsym(library, name);
    if (!*slot) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "cuda.driver_load",
                        "CUDA driver symbol unavailable: %s", name);
        return YVEX_ERR_UNSUPPORTED;
    }
    return YVEX_OK;
}

#define YVEX_LOAD_REQUIRED(driver, field) \
    do { \
        if (load_symbol((driver)->library, (void **)&((driver)->field), #field, err) != YVEX_OK) { \
            return YVEX_ERR_UNSUPPORTED; \
        } \
    } while (0)

int yvex_cuda_driver_load(yvex_cuda_driver *driver, yvex_error *err)
{
    if (!driver) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.driver_load", "driver is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(driver, 0, sizeof(*driver));
    driver->library = dlopen("libcuda.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!driver->library) {
        driver->library = dlopen("libcuda.so", RTLD_NOW | RTLD_LOCAL);
    }
    if (!driver->library) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cuda.driver_load",
                       "CUDA driver library is not available");
        return YVEX_ERR_UNSUPPORTED;
    }

    YVEX_LOAD_REQUIRED(driver, cuInit);
    YVEX_LOAD_REQUIRED(driver, cuDriverGetVersion);
    YVEX_LOAD_REQUIRED(driver, cuDeviceGetCount);
    YVEX_LOAD_REQUIRED(driver, cuDeviceGet);
    YVEX_LOAD_REQUIRED(driver, cuDeviceGetName);
    YVEX_LOAD_REQUIRED(driver, cuDeviceComputeCapability);
    YVEX_LOAD_REQUIRED(driver, cuDeviceTotalMem_v2);
    YVEX_LOAD_REQUIRED(driver, cuDeviceGetAttribute);
    YVEX_LOAD_REQUIRED(driver, cuCtxCreate_v2);
    YVEX_LOAD_REQUIRED(driver, cuCtxDestroy_v2);
    YVEX_LOAD_REQUIRED(driver, cuCtxSetCurrent);
    YVEX_LOAD_REQUIRED(driver, cuCtxSynchronize);
    YVEX_LOAD_REQUIRED(driver, cuMemGetInfo_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemAlloc_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemFree_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemsetD8_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemcpyHtoD_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemcpyDtoH_v2);
    YVEX_LOAD_REQUIRED(driver, cuMemcpyDtoD_v2);
    YVEX_LOAD_REQUIRED(driver, cuModuleLoadData);
    YVEX_LOAD_REQUIRED(driver, cuModuleGetFunction);
    YVEX_LOAD_REQUIRED(driver, cuModuleUnload);
    YVEX_LOAD_REQUIRED(driver, cuLaunchKernel);
    YVEX_LOAD_REQUIRED(driver, cuGetErrorName);
    YVEX_LOAD_REQUIRED(driver, cuGetErrorString);

    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_cuda_driver_unload(yvex_cuda_driver *driver)
{
    if (!driver) {
        return;
    }
    if (driver->library) {
        dlclose(driver->library);
    }
    memset(driver, 0, sizeof(*driver));
}

yvex_cuda_backend_state *yvex_cuda_state(const yvex_backend *backend)
{
    return backend ? (yvex_cuda_backend_state *)backend->impl : NULL;
}

int yvex_cuda_set_current(const yvex_backend *backend, const char *where, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, where ? where : "cuda.set_current",
                       "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    return yvex_cuda_status(&state->driver, state->driver.cuCtxSetCurrent(state->context),
                            where ? where : "cuda.set_current", err);
}

int yvex_cuda_refresh_memory_info(yvex_backend *backend, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    size_t free_bytes = 0;
    size_t total_bytes = 0;
    int rc;

    if (!backend || !state) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.memory_info",
                       "CUDA backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_cuda_set_current(backend, "cuda.memory_info", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemGetInfo_v2(&free_bytes, &total_bytes),
                          "cuda.memory_info", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    backend->device_info.free_memory_bytes = (unsigned long long)free_bytes;
    backend->device_info.total_memory_bytes = (unsigned long long)total_bytes;
    yvex_error_clear(err);
    return YVEX_OK;
}

CUdeviceptr yvex_cuda_tensor_ptr(const yvex_device_tensor *tensor)
{
    return (CUdeviceptr)(size_t)tensor->data;
}
