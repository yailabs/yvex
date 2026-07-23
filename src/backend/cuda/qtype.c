/* Owner: src/backend/cuda
 * Owns: CUDA projection of canonical qtype support/refusal state and the bounded encoded-row-dot Driver API launch,
 *   transfer, rollback, and cleanup path.
 * Does not own: GGUF qtype byte geometry, quantization, full transformer graph execution, runtime generation, eval,
 *   benchmark, or release claims.
 * Invariants: qtype compute support must be present in TRACK.QUANT and proven by the dedicated generated-PTX
 *   row-dot variant before this owner reports it.
 * Boundary: CUDA qtype facts do not make CUDA runtime generation available.
 * Purpose: Project canonical qtype capability and execute bounded encoded row-dot proofs on CUDA.
 * Inputs: Canonical qtype facts, encoded host blocks, F32 vectors, and caller-owned output.
 * Effects: Uses temporary CUDA allocations and releases them before returning.
 * Failure: Unsupported qtypes and Driver failures return typed refusal with deterministic cleanup. */
#include "src/backend/cuda/private.h"

#include <yvex/internal/quant_numeric.h>

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Purpose: Implement the canonical quant fail mechanism owned by the CUDA backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
static int cuda_quant_fail(yvex_quant_failure *failure,
                           yvex_quant_failure_code code,
                           unsigned int qtype,
                           unsigned long long expected,
                           unsigned long long actual,
                           yvex_error *err,
                           int status,
                           const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->terminal_ordinal = ULLONG_MAX;
        failure->source_index = ULLONG_MAX;
        failure->row_index = 0u;
        failure->block_index = ULLONG_MAX;
        failure->expected = expected;
        failure->actual = actual;
        failure->qtype = qtype;
        failure->operation = YVEX_TRANSFORM_OP_COUNT;
    }
    yvex_error_set(err, (yvex_status)status, "cuda.quant.row_dot", message);
    return status;
}

/*
 * Executes one encoded row dot directly on CUDA. Host inputs are borrowed,
 * device temporaries are always released, and no decoded tensor is retained. */
/* Purpose: Implement the canonical quant row dot mechanism owned by the CUDA backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed CUDA refusal and publishes no partial success state.
 * Boundary: CUDA execution; does not infer model topology, profile policy, or runtime support. */
int yvex_cuda_quant_row_dot(yvex_backend *backend,
                            unsigned int qtype,
                            const unsigned char *encoded,
                            size_t encoded_bytes,
                            const float *vector,
                            unsigned long long elements,
                            float *out,
                            yvex_quant_failure *failure,
                            yvex_error *err)
{
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_at(qtype);
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work;
    yvex_gguf_qtype_storage_result storage;
    unsigned long long dims[1];
    CUdeviceptr device_encoded = 0u;
    CUdeviceptr device_vector = 0u;
    CUdeviceptr device_output = 0u;
    size_t vector_bytes;
    void *params[5];
    const char *copy_failure;
    yvex_error cleanup_error;
    yvex_error primary_error;
    int rc;
    int cleanup_rc;

    memset(&work, 0, sizeof(work));
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    if (!backend || !state || !encoded || !vector || !out || !elements ||
        ((uintptr_t)vector % _Alignof(float)) != 0u ||
        ((uintptr_t)out % _Alignof(float)) != 0u ||
        ((qtype == YVEX_GGUF_QTYPE_F32 ||
          qtype == YVEX_GGUF_QTYPE_I32) &&
         ((uintptr_t)encoded % 4u) != 0u) ||
        ((qtype == YVEX_GGUF_QTYPE_F16 ||
          qtype == YVEX_GGUF_QTYPE_BF16) &&
         ((uintptr_t)encoded % 2u) != 0u)) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, qtype, 1u, 0u,
            err, YVEX_ERR_INVALID_ARG,
            "CUDA encoded row, aligned vector/result, and backend are required");
    }
    rc = backend_dispatch_admit(backend, "cuda.quant.row_dot", err);
    if (rc != YVEX_OK) return rc;
    if (!capability || !capability->dedicated_cuda_compute_available) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_CUDA_COMPUTE_UNAVAILABLE, qtype,
            1u, 0u, err, YVEX_ERR_UNSUPPORTED,
            "qtype has no dedicated CUDA numeric contract");
    }
    dims[0] = elements;
    if (yvex_gguf_qtype_tensor_storage(qtype, dims, 1u, &storage) !=
            YVEX_GGUF_QTYPE_STORAGE_OK) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_ROW_DIVISIBILITY, qtype,
            yvex_gguf_qtype_geometry_find(qtype)->block_size, elements,
            err, YVEX_ERR_BOUNDS,
            "CUDA qtype row does not satisfy canonical block geometry");
    }
    if (storage.total_bytes != encoded_bytes ||
        elements > SIZE_MAX / sizeof(float)) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_BYTE_OVERFLOW, qtype,
            storage.total_bytes, encoded_bytes, err, YVEX_ERR_BOUNDS,
            "CUDA encoded row byte geometry is inconsistent");
    }
    vector_bytes = (size_t)elements * sizeof(float);
    rc = yvex_cuda_deferred_release_drain(backend, err);
    if (rc != YVEX_OK) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_CLEANUP, qtype, 0u, 0u, err, rc,
            "prior CUDA qtype temporary cleanup remains incomplete");
    }
    rc = yvex_cuda_require_capability(
        backend, YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT,
        "cuda.quant.row_dot.capability", err);
    if (rc != YVEX_OK) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_CUDA_COMPUTE_UNAVAILABLE, qtype,
            1u, 0u, err, rc,
            "CUDA qtype row-dot kernel is not admitted");
    }
    rc = yvex_cuda_set_current(backend, "cuda.quant.row_dot.context", err);
    if (rc != YVEX_OK) goto execution_failure;
    work.backend = backend;
    work.state = state;
    work.variant = YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT;
    work.raw_only = 1;
    rc = yvex_cuda_work_allocate(&work, &device_encoded, encoded_bytes, NULL, 0,
                                 "cuda.quant.row_dot.alloc_encoded", NULL, err);
    if (rc != YVEX_OK) goto execution_failure;
    rc = yvex_cuda_work_allocate(&work, &device_vector, vector_bytes, NULL, 0,
                                 "cuda.quant.row_dot.alloc_vector", NULL, err);
    if (rc != YVEX_OK) goto execution_failure;
    rc = yvex_cuda_work_allocate(&work, &device_output, sizeof(float), NULL, 1,
                                 "cuda.quant.row_dot.alloc_output", NULL, err);
    if (rc != YVEX_OK) goto execution_failure;

    copy_failure = getenv("YVEX_TEST_CUDA_QTYPE_COPY_FAILURE");
    if (copy_failure && strcmp(copy_failure, "input") == 0) {
        yvex_error_set(err, YVEX_ERR_BACKEND, "cuda.quant.row_dot.copy_input",
                       "injected CUDA qtype input copy failure");
        rc = YVEX_ERR_BACKEND;
        goto execution_failure;
    }
    rc = yvex_cuda_status(
        &state->driver,
        state->driver.cuMemcpyHtoD_v2(device_encoded, encoded, encoded_bytes),
        "cuda.quant.row_dot.copy_encoded", err);
    if (rc != YVEX_OK) goto execution_failure;
    rc = yvex_cuda_status(
        &state->driver,
        state->driver.cuMemcpyHtoD_v2(device_vector, vector, vector_bytes),
        "cuda.quant.row_dot.copy_vector", err);
    if (rc != YVEX_OK) goto execution_failure;
    params[0] = &device_encoded;
    params[1] = &device_vector;
    params[2] = &elements;
    params[3] = &qtype;
    params[4] = &device_output;
    rc = yvex_cuda_launch(
        backend, YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT,
        state->qtype_row_dot_function, 1u, 1u, 0u, params,
        "cuda.quant.row_dot.launch", err);
    if (rc != YVEX_OK) goto execution_failure;
    rc = yvex_cuda_synchronize(
        backend, YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT,
        "cuda.quant.row_dot.synchronize", err);
    if (rc != YVEX_OK) goto execution_failure;
    if (copy_failure && strcmp(copy_failure, "output") == 0) {
        yvex_error_set(err, YVEX_ERR_BACKEND, "cuda.quant.row_dot.copy_output",
                       "injected CUDA qtype output copy failure");
        rc = YVEX_ERR_BACKEND;
        goto execution_failure;
    }
    rc = yvex_cuda_status(
        &state->driver,
        state->driver.cuMemcpyDtoH_v2(out, device_output, sizeof(float)),
        "cuda.quant.row_dot.copy_output", err);
    if (rc != YVEX_OK) goto execution_failure;

    cleanup_rc = yvex_cuda_work_cleanup(&work, err);
    if (cleanup_rc != YVEX_OK) {
        return cuda_quant_fail(
            failure, YVEX_QUANT_FAILURE_CLEANUP, qtype, 3u, 0u, err,
            cleanup_rc, "CUDA qtype temporary cleanup failed");
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;

execution_failure:
    if (err)
        primary_error = *err;
    else
        yvex_error_clear(&primary_error);
    yvex_error_clear(&cleanup_error);
    cleanup_rc = yvex_cuda_work_cleanup(&work, &cleanup_error);
    (void)cleanup_rc;
    rc = cuda_quant_fail(
        failure, YVEX_QUANT_FAILURE_WORKER, qtype, 1u, 0u, err,
        rc == YVEX_OK ? YVEX_ERR_BACKEND : rc,
        "CUDA qtype encoded-row execution failed");
    if (err)
        *err = primary_error;
    return rc;
}
