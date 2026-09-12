/* One fail-closed storage admission for multi-result CUDA producers. */
#include "src/backend/cuda/device_results.h"
#include <stdint.h>

static int results_disjoint(const yvex_device_tensor *left, const yvex_device_tensor *right)
{
    uintptr_t a = left ? (uintptr_t)left->data : 0u, b = right ? (uintptr_t)right->data : 0u;
    return !right || (a && b && left->bytes <= UINTPTR_MAX - a && right->bytes <= UINTPTR_MAX - b &&
        (a + left->bytes <= b || b + right->bytes <= a));
}

int yvex_cuda_device_results(yvex_backend *backend, const yvex_device_tensor *input,
    yvex_device_tensor *const *targets, const unsigned long long *elements,
    const CUdeviceptr *sources, size_t count, unsigned long long *copied, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    unsigned long long total = 0u, bytes;
    if (!state || !backend_tensor_owner_is(backend, input) || !targets || !elements ||
        !count || (sources && !copied)) goto invalid;
    for (size_t i = 0u; i < count; ++i) {
        if (!backend_tensor_owner_is(backend, targets[i]) || targets[i]->dtype != YVEX_DTYPE_F32 || !elements[i] ||
            !backend_tensor_f32_elements(targets[i], elements[i]) ||
            !results_disjoint(targets[i], input) ||
            !results_disjoint(targets[i], backend->workspace_device_tensor) ||
            !yvex_core_u64_mul(elements[i], sizeof(float), &bytes) || bytes > SIZE_MAX ||
            !yvex_core_u64_add(total, bytes, &total) ||
            (sources && (!sources[i] || sources[i] > ULLONG_MAX - bytes))) goto invalid;
        for (size_t j = 0u; j < i; ++j)
            if (!results_disjoint(targets[i], targets[j])) goto invalid;
    }
    if (!sources) { yvex_error_clear(err); return YVEX_OK; }
    if (*copied > ULLONG_MAX - total) goto invalid;
    for (size_t i = 0u; i < count; ++i) {
        CUstream stream = yvex_cuda_launch_stream(backend);
        CUdeviceptr destination = (CUdeviceptr)targets[i]->data;
        bytes = elements[i] * sizeof(float);
        CUresult status = stream && state->driver.cuMemcpyDtoDAsync_v2
            ? state->driver.cuMemcpyDtoDAsync_v2(destination, sources[i], (size_t)bytes, stream)
            : !stream ? state->driver.cuMemcpyDtoD_v2(destination, sources[i], (size_t)bytes) : (CUresult)1;
        int rc = yvex_cuda_status(&state->driver, status, "cuda.results-copy", err);
        if (rc != YVEX_OK) return rc;
        *copied += bytes;
    }
    yvex_error_clear(err);
    return YVEX_OK;
invalid:
    yvex_error_set(err, YVEX_ERR_FORMAT, "cuda.results-copy",
        "result storage is incomplete, aliased, undersized or outside representable bounds");
    return YVEX_ERR_FORMAT;
}
