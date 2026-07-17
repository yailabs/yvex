/*
 * yvex_artifact_materialize.h - canonical artifact materialization boundary.
 *
 * Owner:
 *   src/artifact
 *
 * Owns:
 *   complete-artifact admission handoff, immutable tensor materialization
 *   plans, file-backed/staged runtime-addressable bindings, bounded payload
 *   access walks, lifecycle accounting, expert subview facts, and cleanup
 *   state for materialization sessions.
 *
 * Does not own:
 *   graph execution, attention arithmetic, KV, prefill, decode, logits,
 *   sampling, generation, eval, benchmark, release claims, or qtype policy.
 *
 * Invariants:
 *   materialization consumes complete-artifact admission and an immutable
 *   artifact snapshot; payload access is bounded and never allocates a buffer
 *   as large as a target-scale tensor.
 *
 * Boundary:
 *   materialization makes tensors runtime-addressable through owned access
 *   paths. It is not backend execution or generation.
 */
#ifndef YVEX_ARTIFACT_MATERIALIZE_H
#define YVEX_ARTIFACT_MATERIALIZE_H

#include "yvex_artifact_roundtrip_gate.h"
#include "src/model/target/yvex_deepseek_gguf_map.h"

#include <stddef.h>

#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/gguf_qtype.h>
#include <yvex/tensor.h>

#define YVEX_MATERIALIZATION_IDENTITY_CAP 65u
#define YVEX_MATERIALIZATION_NAME_CAP 192u
#define YVEX_MATERIALIZATION_QTYPE_CAP 43u
#define YVEX_MATERIALIZATION_NO_INDEX (~0ull)

typedef struct {
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_artifact_materialize_fact;

typedef enum {
    YVEX_MATERIALIZATION_STATUS_REFUSED = 0,
    YVEX_MATERIALIZATION_STATUS_PLANNED,
    YVEX_MATERIALIZATION_STATUS_COMMITTED,
    YVEX_MATERIALIZATION_STATUS_ABORTED
} yvex_materialization_status;

typedef enum {
    YVEX_MATERIALIZATION_PLACEMENT_FILE_BACKED = 0,
    YVEX_MATERIALIZATION_PLACEMENT_STAGED_CACHE,
    YVEX_MATERIALIZATION_PLACEMENT_BACKEND_RESIDENT_CANDIDATE
} yvex_materialization_placement;

typedef enum {
    YVEX_MATERIALIZATION_ACCESS_FILE_RANGE = 0,
    YVEX_MATERIALIZATION_ACCESS_STAGED_SUBVIEW,
    YVEX_MATERIALIZATION_ACCESS_BACKEND_CANDIDATE_FILE_RANGE
} yvex_materialization_access_mode;

typedef enum {
    YVEX_MATERIALIZATION_FAILURE_NONE = 0,
    YVEX_MATERIALIZATION_FAILURE_INVALID_ARGUMENT,
    YVEX_MATERIALIZATION_FAILURE_ADMISSION,
    YVEX_MATERIALIZATION_FAILURE_SNAPSHOT_DRIFT,
    YVEX_MATERIALIZATION_FAILURE_LAYOUT,
    YVEX_MATERIALIZATION_FAILURE_TENSOR_COUNT,
    YVEX_MATERIALIZATION_FAILURE_TENSOR_RECORD,
    YVEX_MATERIALIZATION_FAILURE_DUPLICATE_TENSOR,
    YVEX_MATERIALIZATION_FAILURE_QTYPE,
    YVEX_MATERIALIZATION_FAILURE_RANGE,
    YVEX_MATERIALIZATION_FAILURE_BUDGET,
    YVEX_MATERIALIZATION_FAILURE_ALLOCATION,
    YVEX_MATERIALIZATION_FAILURE_READ,
    YVEX_MATERIALIZATION_FAILURE_CANCELLED,
    YVEX_MATERIALIZATION_FAILURE_LIFECYCLE,
    YVEX_MATERIALIZATION_FAILURE_EXPERT_SUBVIEW
} yvex_materialization_failure_code;

typedef struct {
    yvex_materialization_failure_code code;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    unsigned long long offset;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_materialization_failure;

typedef struct {
    unsigned long long max_chunk_bytes;
    unsigned long long cache_budget_bytes;
    unsigned long long backend_resident_budget_bytes;
    unsigned long long future_graph_scratch_reserve_bytes;
    unsigned long long future_kv_reserve_bytes;
    int require_complete_admission;
    int require_deepseek_map;
    int cancel_after_first_chunk;
} yvex_materialization_options;

typedef struct {
    unsigned long long tensor_id;
    unsigned long long descriptor_index;
    char name[YVEX_MATERIALIZATION_NAME_CAP];
    yvex_tensor_role role;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned long long expert_count;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned int qtype;
    yvex_gguf_qtype_storage_class storage_class;
    unsigned long long row_width;
    unsigned long long row_count;
    unsigned long long block_size;
    unsigned long long bytes_per_block;
    unsigned long long encoded_bytes;
    unsigned long long absolute_offset;
    unsigned long long absolute_end_offset;
    unsigned int alignment;
    yvex_materialization_placement placement;
    yvex_materialization_access_mode access_mode;
    int backend_compatible;
} yvex_materialized_tensor_binding;

typedef struct {
    unsigned long long expert_index;
    unsigned long long expert_count;
    unsigned long long absolute_offset;
    unsigned long long encoded_bytes;
    unsigned long long block_size;
    int block_aligned;
} yvex_materialized_expert_subview;

typedef struct {
    yvex_materialization_status status;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char plan_identity[YVEX_MATERIALIZATION_IDENTITY_CAP];
    unsigned long long tensor_count;
    unsigned long long payload_bytes;
    unsigned long long file_bytes;
    unsigned long long qtype_tensor_counts[YVEX_MATERIALIZATION_QTYPE_CAP];
    unsigned long long qtype_bytes[YVEX_MATERIALIZATION_QTYPE_CAP];
    unsigned long long file_backed_tensors;
    unsigned long long file_backed_bytes;
    unsigned long long staged_cache_tensors;
    unsigned long long staged_cache_bytes;
    unsigned long long backend_candidate_tensors;
    unsigned long long backend_candidate_bytes;
    unsigned long long mapped_virtual_bytes;
    unsigned long long file_backed_bytes_owned;
    unsigned long long process_resident_bytes;
    unsigned long long pageable_host_bytes;
    unsigned long long pinned_host_bytes;
    unsigned long long backend_allocated_bytes;
    unsigned long long staging_bytes;
    unsigned long long cache_bytes;
    unsigned long long graph_scratch_reserved_bytes;
    unsigned long long kv_reserved_bytes;
    unsigned long long peak_executor_owned_bytes;
    unsigned long long access_calls;
    unsigned long long payload_bytes_accessed;
    unsigned long long full_walks;
    unsigned long long snapshot_drift_count;
    unsigned long long committed_bindings;
    unsigned long long aborted_bindings;
    unsigned long long expert_subview_count;
    int committed;
    int cleanup_complete;
    int execution_ready;
} yvex_materialization_summary;

typedef struct yvex_materialization_plan yvex_materialization_plan;
typedef struct yvex_materialization_session yvex_materialization_session;

typedef void (*yvex_materialization_progress_fn)(
    void *context,
    const yvex_materialization_summary *summary,
    const yvex_materialized_tensor_binding *binding);

void yvex_artifact_materialize_refuse(yvex_artifact_materialize_fact *fact);
int yvex_artifact_materialize_supported(const char **reason);
void yvex_materialization_options_default(yvex_materialization_options *options);
const char *yvex_materialization_status_name(yvex_materialization_status status);
const char *yvex_materialization_placement_name(yvex_materialization_placement placement);
const char *yvex_materialization_access_mode_name(yvex_materialization_access_mode mode);
const char *yvex_materialization_failure_name(yvex_materialization_failure_code code);

int yvex_materialization_plan_build(
    yvex_materialization_plan **out,
    const yvex_complete_artifact_admission *admission,
    const yvex_artifact *artifact,
    const yvex_gguf *gguf,
    const yvex_tensor_table *tensors,
    const yvex_deepseek_gguf_map *deepseek_map,
    const yvex_materialization_options *options,
    yvex_materialization_failure *failure,
    yvex_error *err);
void yvex_materialization_plan_close(yvex_materialization_plan *plan);
const yvex_materialization_summary *yvex_materialization_plan_summary(
    const yvex_materialization_plan *plan);
unsigned long long yvex_materialization_plan_tensor_count(
    const yvex_materialization_plan *plan);
const yvex_materialized_tensor_binding *yvex_materialization_plan_tensor_at(
    const yvex_materialization_plan *plan,
    unsigned long long index);
const yvex_materialized_tensor_binding *yvex_materialization_plan_find_name(
    const yvex_materialization_plan *plan,
    const char *name);

int yvex_materialization_session_open(
    yvex_materialization_session **out,
    const yvex_materialization_plan *plan,
    const yvex_artifact *artifact,
    const yvex_materialization_options *options,
    yvex_materialization_failure *failure,
    yvex_error *err);
int yvex_materialization_session_commit(
    yvex_materialization_session *session,
    yvex_materialization_failure *failure,
    yvex_error *err);
int yvex_materialization_session_abort(
    yvex_materialization_session *session,
    yvex_materialization_failure *failure,
    yvex_error *err);
void yvex_materialization_session_close(yvex_materialization_session *session);
const yvex_materialization_summary *yvex_materialization_session_summary(
    const yvex_materialization_session *session);
const yvex_materialized_tensor_binding *yvex_materialization_session_tensor_at(
    const yvex_materialization_session *session,
    unsigned long long index);
const yvex_materialized_tensor_binding *yvex_materialization_session_find_name(
    const yvex_materialization_session *session,
    const char *name);
int yvex_materialization_session_read(
    yvex_materialization_session *session,
    const yvex_materialized_tensor_binding *binding,
    unsigned long long binding_offset,
    void *dst,
    size_t len,
    yvex_materialization_failure *failure,
    yvex_error *err);
int yvex_materialization_session_walk_payload(
    yvex_materialization_session *session,
    yvex_materialization_progress_fn progress,
    void *progress_context,
    yvex_materialization_failure *failure,
    yvex_error *err);
int yvex_materialization_session_expert_subview(
    const yvex_materialization_session *session,
    const yvex_materialized_tensor_binding *binding,
    unsigned long long expert_index,
    yvex_materialized_expert_subview *out,
    yvex_materialization_failure *failure,
    yvex_error *err);

#endif
