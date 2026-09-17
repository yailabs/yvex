/* Own bounded CUDA work allocations and admitted arenas with stable tensor views. */
#include "src/backend/cuda/private.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Initialize one stable work range without breaking active stream capture. */
int yvex_cuda_work_initialize(yvex_cuda_work *work, CUdeviceptr target,
                              size_t bytes, const void *source, int zero,
                              const char *stage, yvex_error *err)
{
    CUstream stream;
    if (!source && !zero) return YVEX_OK;
    if (!work || !work->state || !target || !bytes) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA range initialization is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    if (work->prepare_only) return YVEX_OK;
    stream = yvex_cuda_launch_stream(work->backend);
    if (stream) {
        if ((source && !work->state->driver.cuMemcpyHtoDAsync_v2) ||
            (zero && !work->state->driver.cuMemsetD8Async)) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, stage,
                           "captured CUDA range initialization is unavailable");
            return YVEX_ERR_UNSUPPORTED;
        }
        return yvex_cuda_status(
            &work->state->driver,
            source ? work->state->driver.cuMemcpyHtoDAsync_v2(
                         target, source, bytes, stream)
                   : work->state->driver.cuMemsetD8Async(target, 0u, bytes, stream),
            stage, err);
    }
    return yvex_cuda_status(
        &work->state->driver,
        source ? work->state->driver.cuMemcpyHtoD_v2(target, source, bytes)
               : work->state->driver.cuMemsetD8_v2(target, 0u, bytes),
        stage, err);
}

/* Own one bounded allocation transaction across reusable and temporary device storage. */
int yvex_cuda_work_allocate(yvex_cuda_work *work,
                            CUdeviceptr *out,
                            size_t bytes,
                            const void *source,
                            int zero,
                            const char *stage,
                            yvex_cuda_work_failure *failure,
                            yvex_error *err)
{
    unsigned long long address = 0ull;
    unsigned long long next;
    int acquired;
    int rc;
    if (failure) *failure = YVEX_CUDA_WORK_FAILURE_NONE;
    if (!work || !work->backend || !work->state || !out || !bytes ||
        work->count >= YVEX_CUDA_WORK_MAX_RANGES) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, stage,
                       "CUDA work allocation request is invalid");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = backend_dispatch_admit(work->backend, stage, err);
    if (rc != YVEX_OK) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
        return rc;
    }
    if (work->count == 0u) {
        rc = yvex_cuda_deferred_release_drain(work->backend, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
            return rc;
        }
    }
    if (work->current_bytes > ULLONG_MAX - (unsigned long long)bytes ||
        (work->budget && work->current_bytes + (unsigned long long)bytes > work->budget)) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
        yvex_error_set(err, YVEX_ERR_NOMEM, stage, "CUDA work device-byte budget exceeded");
        return YVEX_ERR_NOMEM;
    }
    next = work->current_bytes + (unsigned long long)bytes;
    acquired = work->raw_only ? YVEX_BACKEND_RESIDENT_MISS
                              : yvex_backend_workspace_acquire(
                                    work->backend, bytes, 256ull, &address);
    if (acquired == YVEX_BACKEND_RESIDENT_HIT) {
        *out = (CUdeviceptr)address;
        work->workspace_owned[work->count] = 1u;
    } else if (!work->raw_only && work->backend->workspace_device_tensor) {
        if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
        yvex_error_setf(
            err, acquired == YVEX_BACKEND_RESIDENT_INVALID
                     ? YVEX_ERR_BOUNDS : YVEX_ERR_NOMEM,
            stage,
            "CUDA reusable workspace needs %zu bytes at cursor %llu of %llu",
            bytes, work->backend->workspace_cursor, work->backend->workspace_bytes);
        return acquired == YVEX_BACKEND_RESIDENT_INVALID ? YVEX_ERR_BOUNDS
                                                         : YVEX_ERR_NOMEM;
    } else {
        rc = yvex_backend_memory_can_add(work->backend, bytes, "CUDA", stage, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_BUDGET;
            return rc;
        }
        rc = yvex_cuda_status(&work->state->driver,
                              work->state->driver.cuMemAlloc_v2(out, bytes), stage, err);
        if (rc != YVEX_OK) {
            if (failure) *failure = YVEX_CUDA_WORK_FAILURE_ALLOCATION;
            return rc;
        }
        backend_memory_acquire(work->backend, bytes);
    }
    work->pointers[work->count] = *out;
    work->sizes[work->count++] = bytes;
    work->current_bytes = next;
    if (next > work->peak_bytes) work->peak_bytes = next;
    rc = yvex_cuda_work_initialize(work, *out, bytes, source, zero, stage, err);
    if (rc != YVEX_OK && failure) *failure = YVEX_CUDA_WORK_FAILURE_COPY;
    return rc;
}

/* Release an allocation transaction in reverse order while preserving failed ownership. */
int yvex_cuda_work_cleanup(yvex_cuda_work *work, yvex_error *err)
{
    yvex_error cleanup;
    int result = YVEX_OK;
    if (!work) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    while (work->count) {
        unsigned int index = work->count - 1u;
        int rc = work->workspace_owned[index]
                     ? YVEX_OK
                     : yvex_cuda_temporary_free(
                           work->backend, work->variant, &work->pointers[index],
                           work->sizes[index], 1, "cuda.work.cleanup", &cleanup);
        if (!work->workspace_owned[index] && work->pointers[index] != 0u) {
            if (result == YVEX_OK) {
                result = rc;
                if (err) *err = cleanup;
            }
            break;
        }
        work->current_bytes = work->current_bytes >= work->sizes[index]
                                  ? work->current_bytes - work->sizes[index]
                                  : 0ull;
        work->pointers[index] = 0u;
        work->workspace_owned[index] = 0u;
        work->sizes[index] = 0ull;
        work->count = index;
        if (rc != YVEX_OK && result == YVEX_OK) {
            result = rc;
            if (err) *err = cleanup;
        }
    }
    if (result == YVEX_OK) yvex_error_clear(err);
    return result;
}
