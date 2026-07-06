/*
 * cuda/cuda_tensor.c - CUDA backend tensor storage.
 *
 * This file owns CUDA tensor allocation, host/device transfer, and device copy
 * through the CUDA Driver API.
 */

#include "cuda_internal.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>


static int cuda_memory_can_add(const yvex_backend *backend,
                               unsigned long long bytes,
                               yvex_error *err)
{
    unsigned long long next;

    if (backend->stats.allocated_bytes > ULLONG_MAX - bytes) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                       "allocated byte counter overflow");
        return YVEX_ERR_NOMEM;
    }
    next = backend->stats.allocated_bytes + bytes;
    if (backend->stats.memory_limit_bytes != 0 && next > backend->stats.memory_limit_bytes) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                        "allocation exceeds CUDA backend memory limit %llu",
                        backend->stats.memory_limit_bytes);
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_cuda_tensor_alloc(yvex_backend *backend,
                           const yvex_backend_tensor_desc *desc,
                           yvex_device_tensor **out,
                           yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_device_tensor *tensor = NULL;
    CUdeviceptr ptr = 0;
    unsigned int i;
    int rc;

    if (!backend || !state || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.tensor_alloc",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = cuda_memory_can_add(backend, desc->bytes, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_set_current(backend, "cuda.tensor_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemAlloc_v2(&ptr, (size_t)desc->bytes),
                          "cuda.tensor_alloc", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemsetD8_v2(ptr, 0, (size_t)desc->bytes),
                          "cuda.tensor_alloc", err);
    if (rc != YVEX_OK) {
        (void)state->driver.cuMemFree_v2(ptr);
        return rc;
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        (void)state->driver.cuMemFree_v2(ptr);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                       "failed to allocate CUDA tensor object");
        return YVEX_ERR_NOMEM;
    }
    tensor->name = yvex_backend_strdup(desc->name);
    if (!tensor->name) {
        free(tensor);
        (void)state->driver.cuMemFree_v2(ptr);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                       "failed to copy CUDA tensor name");
        return YVEX_ERR_NOMEM;
    }

    tensor->owner = backend;
    tensor->owner_id = backend->tensor_id_next++;
    tensor->dtype = desc->dtype;
    tensor->rank = desc->rank;
    for (i = 0; i < desc->rank; ++i) {
        tensor->dims[i] = desc->dims[i];
    }
    tensor->bytes = desc->bytes;
    tensor->data = (unsigned char *)(uintptr_t)ptr;

    backend->stats.allocated_bytes += desc->bytes;
    backend->stats.allocation_count += 1;
    if (backend->stats.allocated_bytes > backend->stats.peak_allocated_bytes) {
        backend->stats.peak_allocated_bytes = backend->stats.allocated_bytes;
    }
    (void)yvex_cuda_refresh_memory_info(backend, err);

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_cuda_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);

    if (!backend || !state || !tensor || !yvex_backend_tensor_owner_is(backend, tensor)) {
        return;
    }
    (void)yvex_cuda_set_current(backend, "cuda.tensor_free", NULL);
    (void)state->driver.cuMemFree_v2(yvex_cuda_tensor_ptr(tensor));
    if (backend->stats.allocated_bytes >= tensor->bytes) {
        backend->stats.allocated_bytes -= tensor->bytes;
    } else {
        backend->stats.allocated_bytes = 0;
    }
    if (backend->stats.allocation_count > 0) {
        backend->stats.allocation_count -= 1;
    }
    tensor->owner = NULL;
    tensor->owner_id = 0;
    free(tensor->name);
    free(tensor);
}

static int cuda_check_rw(const char *where,
                         const yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         unsigned long long len,
                         yvex_error *err)
{
    if (!yvex_backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (len != tensor->bytes) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, where,
                        "length %llu does not match tensor bytes %llu", len, tensor->bytes);
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int yvex_cuda_tensor_write(yvex_backend *backend,
                           yvex_device_tensor *tensor,
                           const void *src,
                           unsigned long long len,
                           yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc = cuda_check_rw("yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_set_current(backend, "yvex_backend_tensor_write", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemcpyHtoD_v2(yvex_cuda_tensor_ptr(tensor),
                                                         src, (size_t)len),
                          "yvex_backend_tensor_write", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    tensor->is_written = 1;
    return yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                            "yvex_backend_tensor_write", err);
}

int yvex_cuda_tensor_read(yvex_backend *backend,
                          const yvex_device_tensor *tensor,
                          void *dst,
                          unsigned long long len,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc = cuda_check_rw("yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_set_current(backend, "yvex_backend_tensor_read", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemcpyDtoH_v2(dst, yvex_cuda_tensor_ptr(tensor),
                                                         (size_t)len),
                          "yvex_backend_tensor_read", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    return yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                            "yvex_backend_tensor_read", err);
}

int yvex_cuda_tensor_copy(yvex_backend *backend,
                          yvex_device_tensor *dst,
                          const yvex_device_tensor *src,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    if (!yvex_backend_tensor_owner_is(backend, dst) ||
        !yvex_backend_tensor_owner_is(backend, src)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_tensor_copy",
                       "source and destination must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_same_shape(dst, src)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_tensor_copy",
                       "source and destination tensor descriptors differ");
        return YVEX_ERR_BOUNDS;
    }
    rc = yvex_cuda_set_current(backend, "yvex_backend_tensor_copy", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuMemcpyDtoD_v2(yvex_cuda_tensor_ptr(dst),
                                                         yvex_cuda_tensor_ptr(src),
                                                         (size_t)src->bytes),
                          "yvex_backend_tensor_copy", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    dst->is_written = src->is_written;
    return yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(),
                            "yvex_backend_tensor_copy", err);
}
