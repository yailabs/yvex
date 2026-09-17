/* Tensor-input/result physical program with explicit host/device transport.
 * This owner prepares immutable parameters and staging; it never builds IR. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_STAGE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_STAGE_H_INCLUDED
#include <yvex/internal/program_kernels.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_program_stage yvex_program_stage;
typedef struct {
    const float *values;
    const unsigned int *indices;
} yvex_program_host_input;
/* Diagnostic values are borrowed only during this callback. A refusal stops
 * execution before final publication. Tags belong to the compiled inspection
 * program; the runtime neither interprets them nor infers model stages. */
typedef struct {
    int (*publish)(void *, unsigned long long tag, const yvex_ir_type *,
        unsigned long long rows, const float *, unsigned long long count, yvex_error *);
    void *context;
} yvex_program_observer;
int yvex_program_stage_observe(yvex_program_stage *, const yvex_program_observer *, yvex_error *);
/* Retains the immutable program; borrows backend/encoded parameters until checked close.
 * Calls are serialized by the enclosing execution context. A failed open may
 * retain a non-runnable cleanup owner. Host results publish together only after
 * execution, transfers and the final cancellation check have succeeded. */
int yvex_program_stage_open(yvex_program_stage **, const yvex_program_physical *,
    const yvex_program_kernel_parameter *, size_t, yvex_backend *, unsigned long long capacity,
    int host_io, unsigned long long host_limit, unsigned long long device_limit, yvex_error *);
/* Cold preparation of an exact population; does not execute or publish values. */
int yvex_program_stage_prepare(yvex_program_stage *, unsigned long long rows, yvex_error *);
int yvex_program_stage_device(yvex_program_stage *, unsigned long long rows,
    const yvex_device_tensor *const *, size_t, yvex_device_tensor *const *, size_t,
    int (*cancel)(void *), void *, yvex_backend_operation_facts *, yvex_error *);
int yvex_program_stage_host(yvex_program_stage *, unsigned long long rows,
    const float *const *, size_t, float *const *, size_t, int (*cancel)(void *), void *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_program_stage_host_inputs(yvex_program_stage *, unsigned long long rows,
    const yvex_program_host_input *, size_t, float *const *, size_t, int (*cancel)(void *), void *,
    yvex_backend_operation_facts *, yvex_error *);
void yvex_program_stage_resources(const yvex_program_stage *, unsigned long long *host, unsigned long long *device);
int yvex_program_stage_close(yvex_program_stage **, yvex_error *);

/* Immutable prepared tensor results linked to exact step input slots. No
 * topology, semantic role, family or checkpoint policy is inferred here.
 * The enclosing resource owner decides identity, retention and invalidation. */
typedef struct yvex_program_prepared yvex_program_prepared;
int yvex_program_prepared_open(yvex_program_prepared **,
    const yvex_program_physical *preparation, const yvex_program_physical *step,
    const yvex_program_kernel_parameter *, size_t, yvex_backend *,
    unsigned long long preparation_rows, unsigned long long step_rows,
    const yvex_program_host_input *, const size_t *result_to_input, size_t,
    unsigned long long host_limit, unsigned long long device_limit,
    const yvex_program_observer *, yvex_backend_operation_facts *preparation_facts, yvex_error *);
/* Linked inputs must be empty in the caller directory. Substituting a value
 * would bypass the enclosing resource's prepared identity and is refused. */
int yvex_program_prepared_run(yvex_program_prepared *, const yvex_program_host_input *, size_t,
    float *const *, size_t, int (*cancel)(void *), void *, const yvex_program_observer *,
    yvex_backend_operation_facts *, yvex_error *);
void yvex_program_prepared_resources(const yvex_program_prepared *, unsigned long long *host,
    unsigned long long *device, unsigned long long *retained);
int yvex_program_prepared_close(yvex_program_prepared **, yvex_error *);
#ifdef __cplusplus
}
#endif
#endif
