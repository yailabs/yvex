/*
 * kv_report.h - typed KV report request and facts.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   KV report request and report fact shapes.
 *
 * Does not own:
 *   input grammar, adapter dispatch, text rendering, stdout/stderr output,
 *   attention execution, decode, logits, sampling, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   reports carry diagnostic/report-only KV facts unless the ownership
 *   ownership requests explicitly report a minimal session-owned KV allocation.
 *
 * Boundary:
 *   KV reports are not real attention-backed KV, decode readiness, generation
 *   readiness, benchmark evidence, throughput, or release readiness.
 */
#ifndef YVEX_KV_REPORT_H
#define YVEX_KV_REPORT_H

#include <yvex/api.h>

typedef enum {
    YVEX_KV_REPORT_MODE_NORMAL = 0,
    YVEX_KV_REPORT_MODE_TABLE,
    YVEX_KV_REPORT_MODE_AUDIT
} yvex_kv_report_mode;

typedef enum {
    YVEX_KV_REQUEST_REPORT = 0,
    YVEX_KV_REQUEST_OWNERSHIP
} yvex_kv_request_kind;

typedef struct {
    yvex_kv_request_kind kind;
    const char *model;
    const char *family;
    const char *backend;
    const char *registry_path;
    int include_attention;
    int include_context;
    int include_residency;
    int include_blockers;
    yvex_kv_shape shape;
    int append_demo;
    int read_requested;
    unsigned long long read_position;
    yvex_kv_report_mode report_mode;
} yvex_kv_report_request;

typedef struct {
    const char *name;
    const char *status;
} yvex_kv_phase_fact;

typedef struct {
    const char *name;
    const char *status;
    const char *blocker_class;
} yvex_kv_blocker_fact;

typedef struct {
    yvex_kv_request_kind kind;
    const char *status;
    const char *report_name;
    const char *model;
    const char *model_resolved_path;
    const char *target_id;
    const char *target_class;
    const char *backend;
    const char *family;
    const char *family_detected;
    const char *family_requested;
    const char *family_runtime_status;
    const char *attention_class_status;
    const char *kv_class_status;
    const char *kv_stage;
    const char *kv_support_status;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    int diagnostic_kv_available;
    const char *diagnostic_kv_boundary;
    int real_attention_kv;
    int real_attention_kv_write_ready;
    int real_attention_kv_read_ready;
    int decode_kv_consumer_ready;
    int kv_required;
    const char *kv_source;
    const char *kv_layout;
    const char *kv_layout_status;
    const char *kv_dtype;
    const char *kv_dtype_status;
    const char *kv_layers;
    const char *kv_layers_status;
    const char *kv_heads;
    const char *kv_heads_status;
    const char *kv_head_dim;
    const char *kv_head_dim_status;
    const char *kv_positions;
    const char *kv_capacity;
    const char *kv_capacity_status;
    const char *kv_indexing;
    const char *context_length_source;
    unsigned long long max_context;
    int max_context_seen;
    const char *attention_dependency_status;
    int attention_q_required;
    int attention_k_required;
    int attention_v_required;
    const char *attention_q_status;
    const char *attention_k_status;
    const char *attention_v_status;
    const char *attention_o_status;
    const char *tensor_inventory_status;
    unsigned long long tensor_count;
    unsigned long long tensor_bytes;
    const char *role_token_embedding_status;
    const char *role_attention_norm_status;
    const char *role_q_projection_status;
    const char *role_k_projection_status;
    const char *role_v_projection_status;
    const char *role_o_projection_status;
    const char *role_output_head_status;
    yvex_kv_summary ownership_summary;
    int kv_created;
    int session_owned;
    unsigned long long last_appended_position;
    int read_requested;
    unsigned long long read_position;
    unsigned long long read_value_count;
    unsigned long long read_checksum;
    unsigned long long read_sample_count;
    float read_sample_values[8];
    int cleanup_attempted;
    const char *cleanup_status;
    yvex_kv_phase_fact phases[24];
    unsigned long phase_count;
    yvex_kv_blocker_fact blockers[32];
    unsigned long blocker_count;
    const char *next_required_rows;
    int exit_code;
    int include_attention;
    int include_context;
    int include_residency;
    int include_blockers;
    const char *top_blocker;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *reason;
    const char *kv_layer_indexing;
    const char *kv_head_indexing;
    const char *kv_position_indexing;
    const char *kv_token_order_policy;
    const char *kv_residency_class;
    const char *kv_residency_status;
    const char *kv_cpu_bytes_estimate;
    const char *kv_cuda_bytes_estimate;
    const char *kv_host_staged_bytes_estimate;
    const char *kv_ssd_staged_status;
    const char *kv_ssd_streamed_status;
    const char *kv_paged_status;
    const char *kv_chunked_status;
    const char *kv_quantized_status;
    const char *kv_managed_memory_status;
    const char *context_required;
    const char *requested_context;
    const char *context_capacity_status;
    const char *context_overflow_policy;
    const char *attention_runtime_ready;
    const char *full_transformer_attention_ready;
    const char *prefill_kv_write_required;
    const char *prefill_kv_write_ready;
    const char *decode_kv_read_required;
    const char *decode_kv_read_ready;
    const char *qkv_role_coverage;
    int backend_allocation_attempted;
    int full_kv_allocation_proof;
    int cuda_full_kv_allocation_proof;
    int paged_kv_implementation;
    int chunked_kv_runtime_implementation;
    int ssd_backed_kv;
    int quantized_kv_runtime;
    int full_transformer_prefill_ready;
    int decode_ready;
    int logits_ready;
    int sampling_ready;
    int runtime_execution_ready;
    int generation_ready;
    char model_resolved_path_storage[YVEX_PATH_CAP];
    char target_id_storage[128];
    char family_storage[64];
    char family_detected_storage[64];
    char blocker_storage[256];
    char reason_storage[256];
} yvex_kv_report;

int yvex_kv_report_build(const yvex_kv_report_request *request,
                         yvex_kv_report *report,
                         yvex_error *err);
int yvex_kv_ownership_report_build(const yvex_kv_report_request *request,
                                   yvex_kv_report *report,
                                   yvex_error *err);

#endif
