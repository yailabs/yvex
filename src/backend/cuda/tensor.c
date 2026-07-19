/* Owner: src/backend/cuda.
 * Owns: CUDA tensor allocation, zeroing, host/device transfer, device copy, output-written state, checked device
 *   release, and allocation accounting.
 * Does not own: kernel bundles, primitive kernels, graph/model semantics, CLI output, materialization policy, qtype
 *   compute, or runtime generation.
 * Invariants: writes become visible only after synchronization; failed release preserves tensor ownership and
 *   counters; unpublished allocations are cleaned.
 * Boundary: tensor movement is not CUDA graph or generation support.
 * Purpose: Own CUDA tensor allocation, transfer, copy, accounting, and release.
 * Inputs: A ready backend, typed tensor descriptors, checked byte ranges, and host buffers.
 * Effects: Mutates only owned device allocation state and admitted transfer destinations.
 * Failure: Allocation, transfer, synchronization, and cleanup failures preserve coherent accounting. */

#include "src/backend/cuda/private.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

/* Contract: releases an unpublished allocation and gives cleanup failure precedence. */
/* Purpose: Reserve budgeted storage for failed allocation cleanup with checked size accounting.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
static int cuda_failed_allocation_cleanup(yvex_backend *backend,
                                          CUdeviceptr ptr,
                                          int primary_rc,
                                          yvex_error *err)
{
    yvex_error cleanup_error;
    int cleanup_rc;

    yvex_error_clear(&cleanup_error);
    cleanup_rc = yvex_cuda_temporary_free(backend, YVEX_BACKEND_VARIANT_TENSOR_ALLOC,
                                          ptr, "cuda.tensor_alloc.cleanup",
                                          &cleanup_error);
    if (cleanup_rc != YVEX_OK) {
        if (err) {
            *err = cleanup_error;
        }
        return cleanup_rc;
    }
    return primary_rc;
}

/* Purpose: Reserve budgeted storage for tensor alloc with checked size accounting.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
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

    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_ALLOC,
                                      "cuda.tensor_alloc", err);
    if (rc == YVEX_OK) {
        rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_ZERO,
                                          "cuda.tensor_alloc", err);
    }
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_backend_memory_can_add(backend, desc->bytes, "CUDA", "cuda.tensor_alloc", err);
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
        return cuda_failed_allocation_cleanup(backend, ptr, rc, err);
    }
    rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_TENSOR_ZERO,
                               "cuda.tensor_alloc.zero_sync", err);
    if (rc != YVEX_OK) {
        return cuda_failed_allocation_cleanup(backend, ptr, rc, err);
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                       "failed to allocate CUDA tensor object");
        return cuda_failed_allocation_cleanup(backend, ptr, YVEX_ERR_NOMEM, err);
    }
    tensor->name = yvex_core_strdup(desc->name);
    if (!tensor->name) {
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cuda.tensor_alloc",
                       "failed to copy CUDA tensor name");
        return cuda_failed_allocation_cleanup(backend, ptr, YVEX_ERR_NOMEM, err);
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

    yvex_backend_memory_acquire(backend, desc->bytes);
    (void)yvex_cuda_refresh_memory_info(backend, err);

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Release the resources owned by tensor free without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_tensor_free(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    if (!backend || !state || !tensor || !yvex_backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, "cuda.tensor_free",
                       "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
    }
    rc = yvex_cuda_set_current(backend, "cuda.tensor_free", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_temporary_free(backend, YVEX_BACKEND_VARIANT_TENSOR_ALLOC,
                                  yvex_cuda_tensor_ptr(tensor),
                                  "cuda.tensor_free", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    yvex_backend_memory_release(backend, tensor->bytes);
    tensor->owner = NULL;
    tensor->owner_id = 0;
    free(tensor->name);
    free(tensor);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: Publish tensor write only within its admitted destination range.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_tensor_write(yvex_backend *backend,
                           yvex_device_tensor *tensor,
                           const void *src,
                           unsigned long long len,
                           yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_WRITE,
                                      "yvex_backend_tensor_write", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    tensor->is_written = 0;
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
    rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_TENSOR_WRITE,
                               "yvex_backend_tensor_write", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    tensor->is_written = 1;
    return YVEX_OK;
}

/* Purpose: Retrieve tensor read from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_tensor_read(yvex_backend *backend,
                          const yvex_device_tensor *tensor,
                          void *dst,
                          unsigned long long len,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc = yvex_backend_tensor_rw_validate(
        "yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_READ,
                                      "yvex_backend_tensor_read", err);
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
    return yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_TENSOR_READ,
                                 "yvex_backend_tensor_read", err);
}

/* Purpose: Copy tensor copy between compatible admitted ranges without changing semantic identity.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_tensor_copy(yvex_backend *backend,
                          yvex_device_tensor *dst,
                          const yvex_device_tensor *src,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    rc = yvex_backend_tensor_copy_validate(
        backend, dst, src, "yvex_backend_tensor_copy", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_cuda_require_capability(backend, YVEX_BACKEND_VARIANT_TENSOR_COPY,
                                      "yvex_backend_tensor_copy", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    dst->is_written = 0;
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
    rc = yvex_cuda_synchronize(backend, YVEX_BACKEND_VARIANT_TENSOR_COPY,
                               "yvex_backend_tensor_copy", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    dst->is_written = src->is_written;
    return YVEX_OK;
}
