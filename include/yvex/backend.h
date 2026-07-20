/* Owner: public backend ABI.
 * Owns: backend admission, device tensors, capabilities, and primitive execution.
 * Does not own: model topology, graph policy, or runtime orchestration.
 * Invariants: declarations are format-stable, externally consumable, and independently includable.
 * Boundary: typed CPU/CUDA execution contracts.
 * Purpose: Expose typed CPU/CUDA execution contracts.
 * Inputs: Typed caller-owned values and immutable borrowed views as declared below.
 * Effects: Only functions with explicit lifecycle or I/O contracts mutate external state.
 * Failure: Typed status and error outputs remain authoritative; declarations add no capability. */
#ifndef YVEX_BACKEND_H
#define YVEX_BACKEND_H

#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Backend execution. */
typedef struct yvex_backend yvex_backend;
typedef struct yvex_device_tensor yvex_device_tensor;

typedef enum {
    YVEX_BACKEND_KIND_CPU = 0,
    YVEX_BACKEND_KIND_CUDA,
    YVEX_BACKEND_KIND_METAL,
    YVEX_BACKEND_KIND_ROCM
} yvex_backend_kind;

typedef enum {
    YVEX_BACKEND_STATUS_READY = 0,
    YVEX_BACKEND_STATUS_CONTEXT_READY,
    YVEX_BACKEND_STATUS_UNSUPPORTED,
    YVEX_BACKEND_STATUS_FAILED
} yvex_backend_status;

typedef struct {
    yvex_backend_kind kind;
    const char *device;
    unsigned long long memory_limit_bytes;
} yvex_backend_options;

typedef struct {
    unsigned long long allocated_bytes;
    unsigned long long allocation_count;
    unsigned long long peak_allocated_bytes;
    unsigned long long memory_limit_bytes;
} yvex_backend_memory_stats;

typedef struct {
    const char *name;
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long bytes;
} yvex_backend_tensor_desc;

typedef struct {
    yvex_backend_kind kind;
    const char *name;
    int device_index;
    int compute_capability_major;
    int compute_capability_minor;
    unsigned long long global_memory_bytes;
    unsigned long long free_memory_bytes;
    unsigned long long total_memory_bytes;
    int unified_addressing;
    int managed_memory;
} yvex_backend_device_info;

typedef enum {
    YVEX_BACKEND_CAP_TENSOR_ALLOC = 0,
    YVEX_BACKEND_CAP_TENSOR_READ_WRITE,
    YVEX_BACKEND_CAP_OP_EMBED,
    YVEX_BACKEND_CAP_OP_MATMUL,
    YVEX_BACKEND_CAP_OP_MLP,
    YVEX_BACKEND_CAP_OP_RMS_NORM,
    YVEX_BACKEND_CAP_OP_ROPE,
    YVEX_BACKEND_CAP_OP_ATTENTION
} yvex_backend_capability;

typedef enum {
    YVEX_BACKEND_VARIANT_TENSOR_ALLOC = 0,
    YVEX_BACKEND_VARIANT_TENSOR_ZERO,
    YVEX_BACKEND_VARIANT_TENSOR_WRITE,
    YVEX_BACKEND_VARIANT_TENSOR_READ,
    YVEX_BACKEND_VARIANT_TENSOR_COPY,
    YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32,
    YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32,
    YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32,
    YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16,
    YVEX_BACKEND_VARIANT_ROPE_F32,
    YVEX_BACKEND_VARIANT_MATMUL_F32,
    YVEX_BACKEND_VARIANT_MLP_DENSE_F32,
    YVEX_BACKEND_VARIANT_MLP_ROUTED_F32,
    YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32,
    YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32,
    YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT,
    YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
    YVEX_BACKEND_VARIANT_COUNT
} yvex_backend_operation_variant;

typedef enum {
    YVEX_BACKEND_CAPABILITY_UNSUPPORTED = 0,
    YVEX_BACKEND_CAPABILITY_SUPPORTED,
    YVEX_BACKEND_CAPABILITY_FAILED
} yvex_backend_capability_state;

typedef enum {
    YVEX_BACKEND_CAPABILITY_REASON_NONE = 0,
    YVEX_BACKEND_CAPABILITY_REASON_DRIVER_UNAVAILABLE,
    YVEX_BACKEND_CAPABILITY_REASON_DEVICE_UNAVAILABLE,
    YVEX_BACKEND_CAPABILITY_REASON_CONTEXT_UNAVAILABLE,
    YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT,
    YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_REJECTED,
    YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING,
    YVEX_BACKEND_CAPABILITY_REASON_VARIANT_UNSUPPORTED,
    YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED,
    YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED,
    YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED
} yvex_backend_capability_reason;

typedef struct {
    yvex_backend_kind backend_kind;
    yvex_backend_operation_variant variant;
    yvex_backend_capability_state state;
    yvex_backend_capability_reason reason;
    yvex_dtype input_dtype;
    yvex_dtype weight_dtype;
    yvex_dtype output_dtype;
    int context_available;
    int kernel_bundle_available;
    int function_available;
} yvex_backend_capability_result;

typedef struct {
    const char *backend;
    const char *qtype;
    const char *compute_status;
    const char *reason;
} yvex_backend_qtype_fact;

typedef struct {
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long expert_count;
    unsigned long long expert_id;
    int routed_expert_mode;
    int gated;
    const char *activation;
} yvex_mlp_options;

/*
 * Encoded attention execution contract.
 *
 * This is the backend-facing, family-neutral transport for an admitted
 * attention recipe.  Model and graph owners select the weights, schedule,
 * and activation policy; a backend consumes those immutable facts without
 * reconstructing model topology.  All pointers are borrowed for one call. */
typedef enum {
    YVEX_BACKEND_ATTENTION_WEIGHT_Q_A = 0,
    YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM,
    YVEX_BACKEND_ATTENTION_WEIGHT_Q_B,
    YVEX_BACKEND_ATTENTION_WEIGHT_KV,
    YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM,
    YVEX_BACKEND_ATTENTION_WEIGHT_SINKS,
    YVEX_BACKEND_ATTENTION_WEIGHT_OUT_A,
    YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B,
    YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_KV,
    YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_GATE,
    YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_APE,
    YVEX_BACKEND_ATTENTION_WEIGHT_MAIN_NORM,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_KV,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_GATE,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_APE,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_NORM,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_Q,
    YVEX_BACKEND_ATTENTION_WEIGHT_INDEX_PROJECTION,
    YVEX_BACKEND_ATTENTION_WEIGHT_COUNT
} yvex_backend_attention_weight_slot;

typedef struct {
    const unsigned char *encoded;
    size_t encoded_bytes;
    unsigned long long row_bytes;
    unsigned long long row_width;
    unsigned long long row_count;
    unsigned int qtype;
    int present;
} yvex_backend_attention_weight;

typedef struct {
    unsigned long long theta;
    unsigned long long scaling_factor;
    unsigned long long original_context;
    unsigned long long beta_fast;
    unsigned long long beta_slow;
    unsigned long long rope_dimensions;
} yvex_backend_attention_position;

typedef struct {
    int required;
    unsigned long long block_width;
    unsigned int quantization;
    int hadamard;
} yvex_backend_attention_activation;

typedef enum {
    YVEX_BACKEND_ATTENTION_SWA = 0,
    YVEX_BACKEND_ATTENTION_CSA,
    YVEX_BACKEND_ATTENTION_HCA
} yvex_backend_attention_class;

typedef enum {
    YVEX_BACKEND_ATTENTION_COMPUTE_UNKNOWN = 0,
    YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1
} yvex_backend_attention_compute;

typedef int (*yvex_backend_cancelled_fn)(void *context);

typedef struct {
    yvex_backend_cancelled_fn requested;
    void *context;
} yvex_backend_cancellation;

typedef struct {
    float *data;
    unsigned long long capacity;
} yvex_backend_float_span;

typedef struct {
    unsigned long long *data;
    unsigned long long capacity;
} yvex_backend_u64_span;

typedef struct {
    int present;
    unsigned long long next_token_position;
    unsigned long long ratio;
    unsigned long long head_dimension;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long cursor;
    unsigned long long previous_fill;
    unsigned long long current_fill;
    const float *kv_state;
    unsigned long long kv_state_capacity;
    const float *score_state;
    unsigned long long score_state_capacity;
    int overlap;
} yvex_backend_attention_rolling;

typedef struct {
    yvex_backend_attention_class attention_class;
    yvex_backend_attention_compute compute_contract;
    unsigned long long token_position;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long query_heads;
    unsigned long long head_dimension;
    unsigned long long kv_width;
    unsigned long long sliding_window;
    unsigned long long compression_ratio;
    unsigned long long output_groups;
    unsigned long long output_group_input_width;
    unsigned long long output_rank;
    unsigned long long indexer_heads;
    unsigned long long indexer_head_dimension;
    unsigned long long indexer_topk;
    double rms_epsilon;
    yvex_backend_attention_position position;
    yvex_backend_attention_activation attention_kv_activation;
    yvex_backend_attention_activation compressor_activation;
    yvex_backend_attention_activation compressor_rotated_activation;
    yvex_backend_attention_activation indexer_query_activation;
    yvex_backend_attention_weight weights[YVEX_BACKEND_ATTENTION_WEIGHT_COUNT];
    const float *input;
    const float *local_kv;
    const unsigned long long *local_positions;
    unsigned long long local_count;
    unsigned long long local_stride;
    const float *compressed_kv;
    const unsigned long long *compressed_positions;
    unsigned long long compressed_count;
    unsigned long long compressed_stride;
    const float *indexer_kv;
    const unsigned long long *indexer_positions;
    unsigned long long indexer_count;
    unsigned long long indexer_stride;
    yvex_backend_attention_rolling main_rolling;
    yvex_backend_attention_rolling indexer_rolling;
    const yvex_backend_cancellation *cancellation;
    unsigned long long max_host_bytes;
    unsigned long long max_device_bytes;
} yvex_backend_attention_job;

typedef struct {
    yvex_backend_float_span q_low;
    yvex_backend_float_span query;
    yvex_backend_float_span raw_kv;
    yvex_backend_float_span compressed_kv;
    yvex_backend_float_span indexer_kv;
    yvex_backend_float_span index_query;
    yvex_backend_float_span index_weights;
    yvex_backend_float_span attention_values;
    yvex_backend_float_span output;
    yvex_backend_u64_span compressed_positions;
    yvex_backend_u64_span indexer_positions;
    yvex_backend_u64_span topk_positions;
    yvex_backend_float_span main_kv_state;
    yvex_backend_float_span main_score_state;
    yvex_backend_float_span indexer_kv_state;
    yvex_backend_float_span indexer_score_state;
    unsigned long long compressed_count;
    unsigned long long indexer_count;
    unsigned long long topk_count;
    unsigned long long valid_candidate_count;
    unsigned long long host_bytes;
    unsigned long long peak_host_bytes;
    unsigned long long device_bytes;
    unsigned long long peak_device_bytes;
    unsigned long long kernel_launches;
} yvex_backend_attention_output;

typedef enum {
    YVEX_BACKEND_ATTENTION_FAILURE_NONE = 0,
    YVEX_BACKEND_ATTENTION_FAILURE_INVALID_ARGUMENT,
    YVEX_BACKEND_ATTENTION_FAILURE_CAPABILITY,
    YVEX_BACKEND_ATTENTION_FAILURE_BUDGET,
    YVEX_BACKEND_ATTENTION_FAILURE_ALLOCATION,
    YVEX_BACKEND_ATTENTION_FAILURE_COPY,
    YVEX_BACKEND_ATTENTION_FAILURE_LAUNCH,
    YVEX_BACKEND_ATTENTION_FAILURE_SYNCHRONIZE,
    YVEX_BACKEND_ATTENTION_FAILURE_NUMERIC,
    YVEX_BACKEND_ATTENTION_FAILURE_CANCELLED,
    YVEX_BACKEND_ATTENTION_FAILURE_CLEANUP
} yvex_backend_attention_failure_code;

typedef struct {
    yvex_backend_attention_failure_code code;
    const char *stage;
    unsigned long long expected;
    unsigned long long actual;
} yvex_backend_attention_failure;

int yvex_backend_open(yvex_backend **out,
                      const yvex_backend_options *options,
                      yvex_error *err);
int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err);
int yvex_backend_cuda_context_available(void);
int yvex_backend_cuda_available(void);
void yvex_backend_close(yvex_backend *backend);

yvex_backend_kind yvex_backend_kind_of(const yvex_backend *backend);
const char *yvex_backend_kind_name(yvex_backend_kind kind);
int yvex_backend_kind_parse(const char *name,
                            yvex_backend_kind *out,
                            yvex_error *err);
yvex_backend_status yvex_backend_status_of(const yvex_backend *backend);
const char *yvex_backend_status_name(yvex_backend_status status);

int yvex_backend_get_memory_stats(const yvex_backend *backend,
                                  yvex_backend_memory_stats *out,
                                  yvex_error *err);
int yvex_backend_get_device_info(const yvex_backend *backend,
                                 yvex_backend_device_info *out,
                                 yvex_error *err);

int yvex_backend_tensor_alloc(yvex_backend *backend,
                              const yvex_backend_tensor_desc *desc,
                              yvex_device_tensor **out,
                              yvex_error *err);
void yvex_backend_tensor_free(yvex_backend *backend,
                              yvex_device_tensor *tensor);
int yvex_backend_tensor_release(yvex_backend *backend,
                                yvex_device_tensor **tensor,
                                yvex_error *err);

const char *yvex_device_tensor_name(const yvex_device_tensor *tensor);
yvex_dtype yvex_device_tensor_dtype(const yvex_device_tensor *tensor);
unsigned int yvex_device_tensor_rank(const yvex_device_tensor *tensor);
const unsigned long long *yvex_device_tensor_dims(const yvex_device_tensor *tensor);
unsigned long long yvex_device_tensor_bytes(const yvex_device_tensor *tensor);
int yvex_device_tensor_is_written(const yvex_device_tensor *tensor);

int yvex_backend_tensor_write(yvex_backend *backend,
                              yvex_device_tensor *tensor,
                              const void *src,
                              unsigned long long len,
                              yvex_error *err);
int yvex_backend_tensor_read(yvex_backend *backend,
                             const yvex_device_tensor *tensor,
                             void *dst,
                             unsigned long long len,
                             yvex_error *err);
int yvex_backend_tensor_copy(yvex_backend *backend,
                             yvex_device_tensor *dst,
                             const yvex_device_tensor *src,
                             yvex_error *err);
int yvex_backend_sync(yvex_backend *backend, yvex_error *err);

int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability);
const char *yvex_backend_capability_name(yvex_backend_capability capability);
int yvex_backend_query_capability(const yvex_backend *backend,
                                  yvex_backend_operation_variant variant,
                                  yvex_backend_capability_result *out,
                                  yvex_error *err);
const char *yvex_backend_operation_variant_name(yvex_backend_operation_variant variant);
const char *yvex_backend_capability_state_name(yvex_backend_capability_state state);
const char *yvex_backend_capability_reason_name(yvex_backend_capability_reason reason);
void yvex_backend_qtype_refuse(yvex_backend_qtype_fact *fact,
                               const char *backend,
                               const char *qtype);

int yvex_backend_op_embed(yvex_backend *backend,
                          const yvex_device_tensor *embedding,
                          const unsigned int *token_ids,
                          unsigned long long token_count,
                          yvex_device_tensor *out,
                          yvex_error *err);

int yvex_backend_op_rms_norm(yvex_backend *backend,
                             const yvex_device_tensor *input,
                             const yvex_device_tensor *weight,
                             float epsilon,
                             yvex_device_tensor *out,
                             yvex_error *err);

int yvex_backend_op_rope(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         unsigned long long position,
                         float rope_base,
                         yvex_device_tensor *out,
                         yvex_error *err);

int yvex_backend_op_matmul(yvex_backend *backend,
                           const yvex_device_tensor *input,
                           const yvex_device_tensor *weight,
                           yvex_device_tensor *out,
                           yvex_error *err);

int yvex_backend_op_mlp(yvex_backend *backend,
                        const yvex_device_tensor *input,
                        const yvex_device_tensor *gate_weight,
                        const yvex_device_tensor *up_weight,
                        const yvex_device_tensor *down_weight,
                        const yvex_mlp_options *options,
                        yvex_device_tensor *intermediate,
                        yvex_device_tensor *out,
                        yvex_error *err);

int yvex_backend_op_attention(yvex_backend *backend,
                              const yvex_device_tensor *query,
                              const yvex_device_tensor *keys,
                              const yvex_device_tensor *values,
                              unsigned long long seq_len,
                              unsigned long long position,
                              float scale,
                              int causal,
                              yvex_device_tensor *score_scratch,
                              yvex_device_tensor *probability_scratch,
                              yvex_device_tensor *out,
                              yvex_error *err);

int yvex_backend_attention_execute(
    yvex_backend *backend,
    const yvex_backend_attention_job *job,
    yvex_backend_attention_output *output,
    yvex_backend_attention_failure *failure,
    yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_BACKEND_H */
