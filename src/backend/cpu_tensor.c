/*
 * YVEX - CPU backend tensors
 *
 * File: src/backend/cpu_tensor.c
 * Layer: backend implementation
 *
 * Purpose:
 *   Implements CPU reference tensor allocation, free, read, write, and copy.
 *   CPU tensors are zero-initialized heap buffers owned by the allocating
 *   backend.
 *
 * Implements:
 *   - yvex_cpu_tensor_alloc
 *   - yvex_cpu_tensor_free
 *   - yvex_cpu_tensor_write
 *   - yvex_cpu_tensor_read
 *   - yvex_cpu_tensor_copy
 *
 * Invariants:
 *   - full-buffer read/write only in G0
 *   - allocation stats update on success only
 *   - tensor ownership is checked before mutation
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_cpu
 */
#include "backend_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int yvex_backend_desc_is_valid(const yvex_backend_tensor_desc *desc, yvex_error *err)
{
    unsigned int i;

    if (!desc) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "descriptor is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->rank == 0 || desc->rank > YVEX_TENSOR_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "rank is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->bytes == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "bytes must be non-zero");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_tensor_alloc", "bytes exceed host allocation size");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < desc->rank; ++i) {
        if (desc->dims[i] == 0) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc",
                           "dimensions must be non-zero");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor)
{
    return backend && tensor && tensor->owner == backend && tensor->owner_id != 0;
}

int yvex_backend_tensor_same_shape(const yvex_device_tensor *a,
                                   const yvex_device_tensor *b)
{
    unsigned int i;

    if (!a || !b || a->dtype != b->dtype || a->rank != b->rank || a->bytes != b->bytes) {
        return 0;
    }
    for (i = 0; i < a->rank; ++i) {
        if (a->dims[i] != b->dims[i]) {
            return 0;
        }
    }
    return 1;
}

static int memory_can_add(const yvex_backend *backend,
                          unsigned long long bytes,
                          yvex_error *err)
{
    unsigned long long next;

    if (backend->stats.allocated_bytes > ULLONG_MAX - bytes) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "allocated byte counter overflow");
        return YVEX_ERR_NOMEM;
    }
    next = backend->stats.allocated_bytes + bytes;
    if (backend->stats.memory_limit_bytes != 0 && next > backend->stats.memory_limit_bytes) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc",
                        "allocation exceeds CPU backend memory limit %llu",
                        backend->stats.memory_limit_bytes);
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err)
{
    yvex_device_tensor *tensor;
    unsigned int i;
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.tensor_alloc", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = memory_can_add(backend, desc->bytes, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor object");
        return YVEX_ERR_NOMEM;
    }
    tensor->data = (unsigned char *)calloc(1, (size_t)desc->bytes);
    if (!tensor->data) {
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor data");
        return YVEX_ERR_NOMEM;
    }
    tensor->name = yvex_backend_strdup(desc->name);
    if (!tensor->name) {
        free(tensor->data);
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to copy tensor name");
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

    backend->stats.allocated_bytes += desc->bytes;
    backend->stats.allocation_count += 1;
    if (backend->stats.allocated_bytes > backend->stats.peak_allocated_bytes) {
        backend->stats.peak_allocated_bytes = backend->stats.allocated_bytes;
    }

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_cpu_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor)
{
    if (!backend || !tensor || !yvex_backend_tensor_owner_is(backend, tensor)) {
        return;
    }
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
    free(tensor->data);
    free(tensor->name);
    free(tensor);
}

static int check_tensor_rw(const char *where,
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

int yvex_cpu_tensor_write(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          const void *src,
                          unsigned long long len,
                          yvex_error *err)
{
    int rc = check_tensor_rw("yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(tensor->data, src, (size_t)len);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_tensor_read(yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         void *dst,
                         unsigned long long len,
                         yvex_error *err)
{
    int rc = check_tensor_rw("yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(dst, tensor->data, (size_t)len);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_tensor_copy(yvex_backend *backend,
                         yvex_device_tensor *dst,
                         const yvex_device_tensor *src,
                         yvex_error *err)
{
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
    memcpy(dst->data, src->data, (size_t)src->bytes);
    dst->is_written = src->is_written;
    yvex_error_clear(err);
    return YVEX_OK;
}
