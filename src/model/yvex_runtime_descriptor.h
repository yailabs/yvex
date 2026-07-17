/*
 * yvex_runtime_descriptor.h - canonical runtime descriptor projection.
 *
 * Owner:
 *   src/model
 *
 * Owns:
 *   immutable runtime model descriptor projection from complete-artifact
 *   admission, committed materialization, canonical qtype facts, and optional
 *   DeepSeek GGUF map facts.
 *
 * Does not own:
 *   backend tensor allocation, graph binding, graph execution, attention, KV,
 *   prefill, decode, logits, sampling, generation, eval, benchmark, or release
 *   claims.
 *
 * Invariants:
 *   descriptors bind one committed materialization session and never include
 *   ephemeral addresses in their deterministic identity.
 *
 * Boundary:
 *   runtime descriptor readiness is the graph input contract, not graph
 *   execution.
 */
#ifndef YVEX_RUNTIME_DESCRIPTOR_H
#define YVEX_RUNTIME_DESCRIPTOR_H

#include "src/artifact/yvex_artifact_materialize.h"

#include <yvex/gguf_qtype.h>
#include <yvex/tensor.h>

#define YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP 65u
#define YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP 43u

typedef struct {
    const char *status;
    const char *artifact_status;
    const char *reason;
    const char *next_row;
} yvex_runtime_descriptor_fact;

typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_STATUS_REFUSED = 0,
    YVEX_RUNTIME_DESCRIPTOR_STATUS_READY
} yvex_runtime_descriptor_status;

typedef enum {
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_NONE = 0,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_INVALID_ARGUMENT,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ADMISSION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MATERIALIZATION,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_DUPLICATE_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_MISSING_BINDING,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ARCHITECTURE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_QTYPE,
    YVEX_RUNTIME_DESCRIPTOR_FAILURE_ALLOCATION
} yvex_runtime_descriptor_failure_code;

typedef struct {
    yvex_runtime_descriptor_failure_code code;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_runtime_descriptor_failure;

typedef struct {
    unsigned long long tensor_id;
    unsigned long long descriptor_index;
    const yvex_materialized_tensor_binding *binding;
    yvex_tensor_role role;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned int qtype;
    yvex_materialization_placement placement;
    yvex_materialization_access_mode access_mode;
} yvex_runtime_tensor_binding;

typedef struct {
    yvex_runtime_descriptor_status status;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char materialization_plan_identity[YVEX_MATERIALIZATION_IDENTITY_CAP];
    char runtime_descriptor_identity[YVEX_RUNTIME_DESCRIPTOR_IDENTITY_CAP];
    unsigned long long tensor_count;
    unsigned long long payload_bytes;
    unsigned long long qtype_tensor_counts[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long qtype_bytes[YVEX_RUNTIME_DESCRIPTOR_QTYPE_CAP];
    unsigned long long role_counts[YVEX_TENSOR_ROLE_COUNT];
    unsigned long long global_bindings;
    unsigned long long main_layer_bindings;
    unsigned long long mtp_bindings;
    unsigned long long routed_expert_bindings;
    unsigned long long expert_subview_count;
    unsigned long long missing_required_bindings;
    unsigned long long duplicate_bindings;
    unsigned long long unexpected_bindings;
    unsigned long long layer_count;
    unsigned long long mtp_layer_count;
    unsigned long long routed_experts;
    unsigned long long experts_per_token;
    unsigned long long vocabulary_size;
    int tokenizer_metadata_available;
    int graph_execution_ready;
    int generation_ready;
} yvex_runtime_descriptor_summary;

typedef struct yvex_runtime_descriptor yvex_runtime_descriptor;

void yvex_runtime_descriptor_refuse(yvex_runtime_descriptor_fact *fact);
int yvex_runtime_descriptor_projection_supported(int artifact_descriptor_ok,
                                                 const char **reason);
const char *yvex_runtime_descriptor_status_name(yvex_runtime_descriptor_status status);
const char *yvex_runtime_descriptor_failure_name(
    yvex_runtime_descriptor_failure_code code);

int yvex_runtime_descriptor_build(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_session *session,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err);
int yvex_runtime_descriptor_build_deepseek(
    yvex_runtime_descriptor **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_materialization_session *session,
    const yvex_deepseek_gguf_map *deepseek_map,
    yvex_runtime_descriptor_failure *failure,
    yvex_error *err);
void yvex_runtime_descriptor_close(yvex_runtime_descriptor *descriptor);
const yvex_runtime_descriptor_summary *yvex_runtime_descriptor_summary_get(
    const yvex_runtime_descriptor *descriptor);
unsigned long long yvex_runtime_descriptor_tensor_count(
    const yvex_runtime_descriptor *descriptor);
const yvex_runtime_tensor_binding *yvex_runtime_descriptor_tensor_at(
    const yvex_runtime_descriptor *descriptor,
    unsigned long long index);
const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_name(
    const yvex_runtime_descriptor *descriptor,
    const char *name);
const yvex_runtime_tensor_binding *yvex_runtime_descriptor_find_role(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    yvex_deepseek_tensor_scope scope,
    unsigned long long layer_index,
    unsigned long long predictor_index);

#endif
