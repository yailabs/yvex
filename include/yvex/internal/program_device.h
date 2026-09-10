/* Serialized invocation of verified physical SSA work over borrowed inputs and
 * caller-owned outputs. State handles name provider lifetimes, not tensor bytes. */
#ifndef INCLUDE_YVEX_INTERNAL_PROGRAM_DEVICE_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_PROGRAM_DEVICE_H_INCLUDED

#include <yvex/internal/backend.h>
#include <yvex/internal/program_physical.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_program_device yvex_program_device;
typedef struct {
    const yvex_device_tensor *tensor;
    const unsigned int *indices;
    unsigned long long index, state_handle;
} yvex_program_device_argument;

typedef struct {
    const yvex_program_physical *program;
    const yvex_program_physical_step *step;
    const yvex_program_device_argument *arguments;
    yvex_device_tensor *values;
    unsigned long long rows;
    size_t step_index;
} yvex_program_device_invocation;

/* A static implementation table, not a dynamic plugin protocol. Each name is
 * a compiler-admitted physical operation contract. Bindings/operation resources
 * are prepared by the enclosing execution owner before invocation. It also owns
 * state transactions and result publication; this owner never commits state. */
typedef struct {
    const char *implementation;
    int (*invoke)(void *, const yvex_program_device_invocation *, yvex_backend_operation_facts *, yvex_error *);
} yvex_program_device_kernel;

typedef struct {
    unsigned long long operations, host_bytes, device_bytes;
    yvex_backend_operation_facts backend;
} yvex_program_device_result;

int yvex_program_device_open(yvex_program_device **, const yvex_program_physical *, yvex_backend *,
    unsigned long long capacity, unsigned long long host_limit, unsigned long long device_limit,
    const yvex_program_device_kernel *, size_t kernel_count, void *kernel_context, yvex_error *);
int yvex_program_device_run(yvex_program_device *, unsigned long long rows,
    const yvex_program_device_argument *, size_t argument_count,
    yvex_device_tensor *const *tensor_outputs, size_t tensor_output_count,
    int (*cancel_requested)(void *), void *cancel_context, yvex_program_device_result *, yvex_error *);
void yvex_program_device_resources(const yvex_program_device *, unsigned long long *host, unsigned long long *device);
int yvex_program_device_close(yvex_program_device **, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
