/*
 * cuda/cuda_backend.c - CUDA backend object lifecycle.
 *
 * This file attaches CUDA to the backend ABI. Device kernels remain in
 * cuda_kernels.cu.
 */

#include "cuda_internal.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>


static int parse_device_index(const char *text, int *out, yvex_error *err)
{
    long value = 0;
    const char *p;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.parse_device", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = 0;
    if (!text || text[0] == '\0') {
        return YVEX_OK;
    }
    for (p = text; *p; ++p) {
        if (!isdigit((unsigned char)*p)) {
            yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "cuda.parse_device",
                            "CUDA device must be numeric: %s", text);
            return YVEX_ERR_INVALID_ARG;
        }
        value = (value * 10) + (*p - '0');
        if (value > 65535) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.parse_device",
                           "CUDA device index is too large");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    *out = (int)value;
    return YVEX_OK;
}

static int cuda_close(yvex_backend *backend, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_driver *driver;

    if (!backend || !state) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    driver = &state->driver;
    if (state->module_loaded && driver->cuModuleUnload) {
        (void)driver->cuModuleUnload(state->module);
    }
    if (state->context && driver->cuCtxDestroy_v2) {
        (void)driver->cuCtxDestroy_v2(state->context);
    }
    yvex_cuda_driver_unload(driver);
    free(state);
    backend->impl = NULL;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cuda_memory_stats(const yvex_backend *backend,
                             yvex_backend_memory_stats *out,
                             yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.memory_stats",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = backend->stats;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cuda_device_info(const yvex_backend *backend,
                            yvex_backend_device_info *out,
                            yvex_error *err)
{
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.device_info",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_cuda_refresh_memory_info((yvex_backend *)backend, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    *out = backend->device_info;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cuda_sync(yvex_backend *backend, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    if (!backend || !state) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.sync", "backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_cuda_set_current(backend, "cuda.sync", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    return yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(), "cuda.sync", err);
}

static int cuda_supports(const yvex_backend *backend, yvex_backend_capability capability)
{
    if (!backend) {
        return 0;
    }
    switch (capability) {
    case YVEX_BACKEND_CAP_TENSOR_ALLOC:
    case YVEX_BACKEND_CAP_TENSOR_READ_WRITE:
    case YVEX_BACKEND_CAP_OP_EMBED:
    case YVEX_BACKEND_CAP_OP_MATMUL:
    case YVEX_BACKEND_CAP_OP_RMS_NORM:
    case YVEX_BACKEND_CAP_OP_ROPE:
    case YVEX_BACKEND_CAP_OP_ATTENTION:
        return 1;
    }
    return 0;
}

static const yvex_backend_vtable cuda_vtable = {
    cuda_close,
    cuda_memory_stats,
    cuda_device_info,
    yvex_cuda_tensor_alloc,
    yvex_cuda_tensor_free,
    yvex_cuda_tensor_write,
    yvex_cuda_tensor_read,
    yvex_cuda_tensor_copy,
    cuda_sync,
    cuda_supports,
    yvex_cuda_op_embed,
    yvex_cuda_op_rms_norm,
    yvex_cuda_op_rope,
    yvex_cuda_op_matmul,
    yvex_cuda_op_attention,
};

int yvex_backend_open_cuda_impl(yvex_backend **out,
                                const char *device,
                                unsigned long long memory_limit_bytes,
                                yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_cuda_backend_state *state = NULL;
    int device_index = 0;
    int device_count = 0;
    int unified = 0;
    int managed = 0;
    size_t global_bytes = 0;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open_cuda_impl",
                       "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = parse_device_index(device, &device_index, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    backend = (yvex_backend *)calloc(1, sizeof(*backend));
    state = (yvex_cuda_backend_state *)calloc(1, sizeof(*state));
    if (!backend || !state) {
        free(state);
        free(backend);
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_backend_open_cuda_impl",
                       "failed to allocate CUDA backend");
        return YVEX_ERR_NOMEM;
    }

    rc = yvex_cuda_driver_load(&state->driver, err);
    if (rc != YVEX_OK) {
        free(state);
        free(backend);
        return rc;
    }
    rc = yvex_cuda_status(&state->driver, state->driver.cuInit(0),
                          "yvex_backend_open_cuda_impl", err);
    if (rc != YVEX_OK) {
        yvex_cuda_driver_unload(&state->driver);
        free(state);
        free(backend);
        return rc;
    }
    rc = yvex_cuda_status(&state->driver, state->driver.cuDeviceGetCount(&device_count),
                          "yvex_backend_open_cuda_impl", err);
    if (rc != YVEX_OK || device_count <= 0) {
        if (rc == YVEX_OK) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_open_cuda_impl",
                           "CUDA runtime is available but no CUDA device was found");
            rc = YVEX_ERR_UNSUPPORTED;
        }
        yvex_cuda_driver_unload(&state->driver);
        free(state);
        free(backend);
        return rc;
    }
    if (device_index < 0 || device_index >= device_count) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open_cuda_impl",
                        "CUDA device %d is out of range; device_count=%d",
                        device_index, device_count);
        yvex_cuda_driver_unload(&state->driver);
        free(state);
        free(backend);
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_cuda_status(&state->driver, state->driver.cuDeviceGet(&state->device, device_index),
                          "yvex_backend_open_cuda_impl", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_status(&state->driver,
                              state->driver.cuCtxCreate_v2(&state->context, 0, state->device),
                              "yvex_backend_open_cuda_impl", err);
    }
    if (rc != YVEX_OK) {
        yvex_cuda_driver_unload(&state->driver);
        free(state);
        free(backend);
        return rc;
    }

    state->device_index = device_index;
    (void)state->driver.cuDriverGetVersion(&state->driver_version);
    (void)state->driver.cuDeviceGetName(backend->device_name_storage,
                                        (int)sizeof(backend->device_name_storage),
                                        state->device);
    (void)state->driver.cuDeviceComputeCapability(&backend->device_info.compute_capability_major,
                                                  &backend->device_info.compute_capability_minor,
                                                  state->device);
    (void)state->driver.cuDeviceTotalMem_v2(&global_bytes, state->device);
    (void)state->driver.cuDeviceGetAttribute(&unified,
                                             YVEX_CUDA_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING,
                                             state->device);
    (void)state->driver.cuDeviceGetAttribute(&managed,
                                             YVEX_CUDA_DEVICE_ATTRIBUTE_MANAGED_MEMORY,
                                             state->device);

    backend->kind = YVEX_BACKEND_KIND_CUDA;
    backend->status = YVEX_BACKEND_STATUS_READY;
    backend->vtable = &cuda_vtable;
    backend->impl = state;
    backend->stats.memory_limit_bytes = memory_limit_bytes;
    backend->tensor_id_next = 1;
    backend->device_info.kind = YVEX_BACKEND_KIND_CUDA;
    backend->device_info.name = backend->device_name_storage;
    backend->device_info.device_index = device_index;
    backend->device_info.global_memory_bytes = (unsigned long long)global_bytes;
    backend->device_info.unified_addressing = unified != 0;
    backend->device_info.managed_memory = managed != 0;
    (void)yvex_cuda_refresh_memory_info(backend, err);

    *out = backend;
    yvex_error_clear(err);
    return YVEX_OK;
}
