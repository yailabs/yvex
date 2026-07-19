/* Owner: artifact.internal (artifact).
 * Owns: artifact identities, materialization plans, and complete-artifact admission.
 * Does not own: GGUF encoding, graph execution, or runtime support.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: artifact-owned internal lifecycle and admission.
 * Purpose: provide the canonical artifact-owned internal lifecycle and admission contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_ARTIFACT_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_ARTIFACT_H_INCLUDED

#include <stddef.h>
#include <yvex/artifact.h>
#include <yvex/gguf.h>
#include <yvex/model.h>
#include <yvex/qtype.h>
#include <yvex/internal/core.h>
#include <yvex/internal/gguf.h>
#include <yvex/internal/gguf_writer.h>

#ifdef __cplusplus
extern "C" {
#endif

int yvex_artifact_snapshot_equal(const yvex_artifact_snapshot *left,
                                 const yvex_artifact_snapshot *right);

/* Identity contract. */
typedef struct {
    yvex_sha256 hash;
    unsigned long long bytes;
    int active;
} yvex_artifact_identity_stream;
void yvex_artifact_identity_stream_init(
    yvex_artifact_identity_stream *stream);
int yvex_artifact_identity_stream_update(
    yvex_artifact_identity_stream *stream,
    const unsigned char *bytes,
    size_t byte_count,
    yvex_error *err);
int yvex_artifact_identity_stream_final(
    yvex_artifact_identity_stream *stream,
    unsigned long long expected_bytes,
    char out_hex[YVEX_SHA256_HEX_CAP],
    yvex_error *err);

/* Roundtrip Gate contract. */
#define YVEX_GGUF_OFFICIAL_READER_REVISION \
    "af97976c7810cdabb1863172f31c432dab767de7"
#define YVEX_SELECTED_DEEPSEEK_ARTIFACT_FILENAME \
    "deepseek-v4-flash-q8_0-q2_k-v1.gguf"
#define YVEX_SELECTED_DEEPSEEK_ARTIFACT_IDENTITY \
    "01b2bed4f070d0a3fdb02e546764b3a49cb69886eebe17b4877d20294725682c"
#define YVEX_SELECTED_DEEPSEEK_PROFILE_NAME \
    "deepseek-v4-flash-q8_0-q2_k-v1"
#define YVEX_SELECTED_DEEPSEEK_PROFILE_IDENTITY \
    "04be09e124fd997ae3b785d0d3018f9d571cb6b96df5488d0ab21de3345bce25"
#define YVEX_SELECTED_DEEPSEEK_EXECUTION_IDENTITY \
    "b81f3c5d670737bf20c938e635a1bffdbb0d60f885f994225a02225bb7ba51db"
#define YVEX_SELECTED_DEEPSEEK_PAYLOAD_IDENTITY \
    "e22b3678d131d334f154a93214bdddfafc172c9869f4c52db28fea198eaa9165"
#define YVEX_SELECTED_DEEPSEEK_TRANSFORM_IDENTITY \
    "1c5ceab43fa9f9bf437aacc3b4b3c246ff26446ab0d7abd22ea642ce726017f5"
#define YVEX_SELECTED_DEEPSEEK_WRITER_PLAN_IDENTITY \
    "4b47814e06c43b3426efcaab72b836596c42358a7c59ea5619ddd70c0eefe9fd"
#define YVEX_SELECTED_DEEPSEEK_FILE_BYTES 102408545440ull
#define YVEX_SELECTED_DEEPSEEK_PAYLOAD_BYTES 102396843592ull
#define YVEX_SELECTED_DEEPSEEK_TENSOR_COUNT 1360ull
#define YVEX_SELECTED_DEEPSEEK_METADATA_COUNT 68ull
#define YVEX_SELECTED_DEEPSEEK_SOURCE_IDENTITY 0x818f3e5c5eaf9ffcull
#define YVEX_SELECTED_DEEPSEEK_MAPPING_IDENTITY 0x1aecbbe25b04de0dull
typedef enum {
    YVEX_ARTIFACT_CLASS_REFUSED = 0,
    YVEX_ARTIFACT_CLASS_TENSOR_PROOF,
    YVEX_ARTIFACT_CLASS_EXTERNAL_UNADMITTED,
    YVEX_ARTIFACT_CLASS_COMPLETE_YVEX
} yvex_artifact_class;
typedef enum {
    YVEX_ARTIFACT_ADMISSION_OK = 0,
    YVEX_ARTIFACT_ADMISSION_INVALID_ARGUMENT,
    YVEX_ARTIFACT_ADMISSION_WRITER_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_EMISSION_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_NATIVE_ROUNDTRIP,
    YVEX_ARTIFACT_ADMISSION_OFFICIAL_READER,
    YVEX_ARTIFACT_ADMISSION_IDENTITY_MISMATCH,
    YVEX_ARTIFACT_ADMISSION_TOKENIZER_INCOMPLETE,
    YVEX_ARTIFACT_ADMISSION_TENSOR_COVERAGE,
    YVEX_ARTIFACT_ADMISSION_FILE_OPEN,
    YVEX_ARTIFACT_ADMISSION_FILE_DRIFT
} yvex_artifact_admission_code;
typedef struct {
    char revision[41];
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long file_bytes;
    unsigned long long file_device;
    unsigned long long file_inode;
    long long file_mtime_seconds;
    long long file_mtime_nanoseconds;
    long long file_ctime_seconds;
    long long file_ctime_nanoseconds;
    int accepted;
} yvex_artifact_official_reader_fact;
typedef struct yvex_artifact_admission_failure {
    yvex_artifact_admission_code code;
    unsigned long long expected;
    unsigned long long actual;
    char field[96];
} yvex_artifact_admission_failure;
typedef struct {
    const char *artifact_path;
    const yvex_gguf_writer_plan *writer_plan;
    const yvex_gguf_file_sink_summary *emission;
    const yvex_gguf_roundtrip_summary *native_roundtrip;
    const yvex_artifact_official_reader_fact *official_reader;
} yvex_artifact_admission_request;
typedef struct yvex_complete_artifact_admission {
    yvex_artifact_class artifact_class;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long payload_bytes;
    unsigned long long file_bytes;
    unsigned long long source_snapshot_identity;
    unsigned long long mapping_identity;
    yvex_artifact_snapshot file_snapshot;
    char artifact_path[YVEX_ARTIFACT_PATH_CAP];
    char payload_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char transform_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char profile_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char profile_name[64];
    char quant_execution_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char writer_plan_identity[YVEX_GGUF_WRITER_IDENTITY_CAP];
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char official_reader_revision[41];
    int tokenizer_complete;
    int native_reader_accepted;
    int official_reader_accepted;
    int payload_integrity_accepted;
    int materialization_input_ready;
    int runtime_supported;
    int complete;
} yvex_complete_artifact_admission;
typedef enum {
    YVEX_ARTIFACT_DESCRIPTOR_REFUSED = 0,
    YVEX_ARTIFACT_DESCRIPTOR_REPORT_ONLY = 1,
    YVEX_ARTIFACT_DESCRIPTOR_COMPLETE_ADMITTED = 2
} yvex_artifact_descriptor_status;
typedef struct {
    yvex_artifact_descriptor_status status;
    const char *format;
    const char *reason;
    const char *next_row;
    const char *artifact_identity;
    unsigned long long tensor_count;
    int materialization_input_ready;
    int runtime_supported;
} yvex_artifact_descriptor_fact;
int yvex_complete_artifact_admit(
    const yvex_artifact_admission_request *request,
    yvex_complete_artifact_admission *out,
    yvex_artifact_admission_failure *failure,
    yvex_error *err);
const char *yvex_artifact_class_name(yvex_artifact_class artifact_class);
const char *yvex_artifact_admission_code_name(
    yvex_artifact_admission_code code);
int yvex_artifact_descriptor_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_artifact_descriptor_fact *fact);

/* Materialize contract. */
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
typedef struct yvex_materialization_failure {
    yvex_materialization_failure_code code;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    unsigned long long offset;
    char tensor_name[YVEX_MATERIALIZATION_NAME_CAP];
    const char *reason;
} yvex_materialization_failure;
typedef struct yvex_materialization_options {
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
    yvex_tensor_collection collection;
    yvex_tensor_scope scope;
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
void yvex_materialization_options_default(yvex_materialization_options *options);
const char *yvex_materialization_status_name(yvex_materialization_status status);
const char *yvex_materialization_failure_name(yvex_materialization_failure_code code);
void yvex_materialization_plan_close(yvex_materialization_plan *plan);
const yvex_materialization_summary *yvex_materialization_plan_summary(
    const yvex_materialization_plan *plan);
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
void yvex_materialization_session_close(yvex_materialization_session *session);
const yvex_materialization_summary *yvex_materialization_session_summary(
    const yvex_materialization_session *session);
const yvex_materialized_tensor_binding *yvex_materialization_session_tensor_at(
    const yvex_materialization_session *session,
    unsigned long long index);
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

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_ARTIFACT_H_INCLUDED */
