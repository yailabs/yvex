/* Tensor-input/result physical program with explicit host/device transport.
 * This owner prepares immutable parameters and staging; it never builds IR. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_STAGE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_STAGE_H_INCLUDED
#include <yvex/internal/program_kernels.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_program_stage yvex_program_stage;
/* Borrows the sealed program/backend/encoded parameters until checked close.
 * Calls are serialized by the enclosing execution context. A failed open may
 * retain a non-runnable cleanup owner. Host results publish together only after
 * execution, transfers and the final cancellation check have succeeded. */
int yvex_program_stage_open(yvex_program_stage **, const yvex_program_physical *,
    const yvex_program_kernel_parameter *, size_t, yvex_backend *, unsigned long long capacity,
    int host_io, unsigned long long host_limit, unsigned long long device_limit, yvex_error *);
int yvex_program_stage_device(yvex_program_stage *, unsigned long long rows,
    const yvex_device_tensor *const *, size_t, yvex_device_tensor *const *, size_t,
    int (*cancel)(void *), void *, yvex_backend_operation_facts *, yvex_error *);
int yvex_program_stage_host(yvex_program_stage *, unsigned long long rows,
    const float *const *, size_t, float *const *, size_t, int (*cancel)(void *), void *,
    yvex_backend_operation_facts *, yvex_error *);
void yvex_program_stage_resources(const yvex_program_stage *, unsigned long long *host, unsigned long long *device);
int yvex_program_stage_close(yvex_program_stage **, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
