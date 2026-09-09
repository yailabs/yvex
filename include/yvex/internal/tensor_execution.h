/* Prepared execution of compiler-owned tensor work; no family or layer topology. */
#ifndef INCLUDE_YVEX_INTERNAL_TENSOR_EXECUTION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_TENSOR_EXECUTION_H_INCLUDED

#include <yvex/internal/backend.h>
#include <yvex/internal/component.h>
#include <yvex/internal/program.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_tensor_execution yvex_tensor_execution;
typedef struct {
    const yvex_device_tensor *tensor;
    const yvex_component_encoded_weight *parameter;
} yvex_tensor_execution_argument;
typedef struct {
    unsigned long long host_bytes, device_bytes, workspace_required_bytes, preparation_count;
} yvex_tensor_execution_resources;
typedef struct {
    unsigned long long operations, linear_operations;
    yvex_backend_operation_facts backend;
} yvex_tensor_execution_result;

/* The immutable plan and backend outlive this serialized execution owner.
 * Prepare specializes only an admitted row population, never program meaning.
 * Execute borrows arguments and caller-owned outputs for one synchronous call;
 * partial output after failure is not a published result. The enclosing runtime
 * transaction/publication owner controls visibility and cancellation. Backend
 * scratch remains backend-owned: workspace_required_bytes is its high-water
 * requirement, not memory allocated by this owner. Failed cleanup retains out
 * for checked close retry. */
int yvex_tensor_execution_open(yvex_tensor_execution **, const yvex_program_tensor_plan *, yvex_backend *,
                               unsigned long long row_capacity, unsigned long long maximum_host_bytes,
                               unsigned long long maximum_device_bytes, yvex_error *);
int yvex_tensor_execution_prepare(yvex_tensor_execution *, unsigned long long rows, yvex_error *);
int yvex_tensor_execution_run(yvex_tensor_execution *, unsigned long long rows,
                              const yvex_tensor_execution_argument *, size_t argument_count,
                              yvex_device_tensor *const *outputs, size_t output_count,
                              yvex_tensor_execution_result *, yvex_error *);
const yvex_tensor_execution_resources *yvex_tensor_execution_resources_get(const yvex_tensor_execution *);
int yvex_tensor_execution_close(yvex_tensor_execution **, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
