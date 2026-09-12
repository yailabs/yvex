/* Transfer admitted temporary device results to independent caller storage.
 * Enqueue is not transaction commit. The producer owns status/completion and
 * must retain each source until the copies have been ordered on its stream. */
#ifndef SRC_BACKEND_CUDA_DEVICE_RESULTS_H_INCLUDED
#define SRC_BACKEND_CUDA_DEVICE_RESULTS_H_INCLUDED
#include "src/backend/cuda/private.h"
#ifdef __cplusplus
extern "C" {
#endif
/* A null sources vector performs admission only. Sources, when supplied, are
 * producer-owned device addresses; they are never published as borrowed views.
 * Targets are disjoint F32 storage views, disjoint from input and
 * workspace. No storage, stream, completion or publication ownership transfers. */
int yvex_cuda_device_results(yvex_backend *, const yvex_device_tensor *,
    yvex_device_tensor *const *, const unsigned long long *, const CUdeviceptr *,
    size_t, unsigned long long *, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
