/* Common admitted execution capacity, independent of an output runner. */
#ifndef INCLUDE_YVEX_INTERNAL_RUNTIME_CAPACITY_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_RUNTIME_CAPACITY_H_INCLUDED

#include <yvex/internal/runtime.h>
#include <yvex/internal/backend.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    yvex_backend_kind backend;
    yvex_execution_generation_mode mode;
    yvex_execution_workload_profile_kind workload_kind;
    yvex_execution_evidence_profile evidence_profile;
    yvex_execution_sampling_requirement sampling_requirement;
    unsigned long long context_capacity, prefill_chunk_tokens;
    unsigned long long concurrent_sequences;
    unsigned long long maximum_host_bytes, maximum_device_bytes;
    int compatible_operation_batching;
} yvex_runtime_capacity_options;

typedef struct {
    yvex_execution_hardware_profile hardware_profile;
    yvex_backend_bandwidth_evidence bandwidth_evidence;
    yvex_execution_workload_profile workload_profile;
    yvex_execution_capacity_plan capacity_plan;
    unsigned long long system_capacity_bytes, system_reserve_bytes;
    unsigned long long sampling_workspace_bytes, physical_rows;
} yvex_runtime_capacity;

int yvex_runtime_capacity_derive(
    yvex_model_engine *model, yvex_runtime_execution_session *session,
    const yvex_runtime_capacity_options *options, yvex_runtime_capacity *out,
    yvex_graph_attention_capacity_plan **attention_capacity, yvex_error *err);

int yvex_runtime_capacity_preflight(
    const yvex_runtime_binding *binding, yvex_backend *backend,
    const yvex_runtime_capacity_options *options,
    unsigned long long *required, unsigned long long *available, yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
