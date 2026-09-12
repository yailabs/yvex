/* Admitted numerical-operation ABI: operands, geometry, precision and backend
 * implementation contracts. No source schema, model plan or runner lifetime.
 * Historical operation type names are retained; record layouts are unchanged. */
#ifndef INCLUDE_YVEX_INTERNAL_NEURAL_OPERATIONS_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_NEURAL_OPERATIONS_H_INCLUDED
#include <yvex/internal/backend.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_gated_delta_plan yvex_gated_delta_plan;
typedef struct yvex_gated_delta_device_request yvex_gated_delta_device_request;
typedef struct yvex_gated_delta_device_result yvex_gated_delta_device_result;

#define YVEX_TRANSFORMER_LINEAR_PHYSICAL_SCHEMA_V3 3u
#define YVEX_TRANSFORMER_LINEAR_DOMAIN_CAP 96u
typedef enum {
    YVEX_TRANSFORMER_LINEAR_OPERATION_UNKNOWN = 0,
    YVEX_TRANSFORMER_LINEAR_OPERATION_JOINT_VIDEO_OUTPUT,
    YVEX_TRANSFORMER_LINEAR_OPERATION_JOINT_AUDIO_OUTPUT,
    YVEX_TRANSFORMER_LINEAR_OPERATION_MODULATION,
    YVEX_TRANSFORMER_LINEAR_OPERATION_QKV,
    YVEX_TRANSFORMER_LINEAR_OPERATION_ATTENTION_OUTPUT,
    YVEX_TRANSFORMER_LINEAR_OPERATION_GATE_UP,
    YVEX_TRANSFORMER_LINEAR_OPERATION_DOWN,
    YVEX_TRANSFORMER_LINEAR_OPERATION_PROJECTION
} yvex_transformer_linear_operation;
typedef enum {
    YVEX_TRANSFORMER_LINEAR_IMPLEMENTATION_UNKNOWN = 0,
    YVEX_TRANSFORMER_LINEAR_IMPLEMENTATION_DEVICE_F32_BIAS
} yvex_transformer_linear_implementation;
typedef enum {
    YVEX_TRANSFORMER_LINEAR_NUMERIC_UNKNOWN = 0,
    YVEX_TRANSFORMER_LINEAR_NUMERIC_SOURCE_EXACT,
    YVEX_TRANSFORMER_LINEAR_NUMERIC_BF16_F32_ACCUMULATION
} yvex_transformer_linear_numeric_contract;
typedef struct yvex_transformer_linear_requirement {
    yvex_transformer_linear_operation operation;
    yvex_transformer_linear_numeric_contract publication_contract;
    yvex_dtype source_dtype;
    yvex_dtype input_dtype, accumulation_dtype, output_dtype, publication_dtype;
    unsigned long long input_width, output_width;
    int bias;
} yvex_transformer_linear_requirement;
int yvex_transformer_linear_requirement_validate(
    const yvex_transformer_linear_requirement *, yvex_error *);
typedef struct yvex_transformer_linear_physical_plan {
    unsigned int schema_version;
    char semantic_domain[YVEX_TRANSFORMER_LINEAR_DOMAIN_CAP];
    yvex_transformer_linear_operation operation;
    yvex_transformer_linear_numeric_contract numeric_contract;
    yvex_dtype source_dtype;
    yvex_transformer_linear_implementation implementation;
    yvex_backend_kind backend;
    unsigned long long input_width, output_width, workspace_bytes;
    int bias, deterministic, exact;
    char operation_identity[YVEX_SHA256_HEX_BYTES];
    char physical_identity[YVEX_SHA256_HEX_BYTES];
} yvex_transformer_linear_physical_plan;
int yvex_transformer_linear_physical_seal(
    yvex_transformer_linear_physical_plan *plan, yvex_error *err);
int yvex_transformer_linear_physical_validate(
    const yvex_transformer_linear_physical_plan *plan, yvex_error *err);
typedef struct yvex_transformer_linear_executable yvex_transformer_linear_executable;
#define YVEX_TRANSFORMER_LINEAR_EXECUTABLE_SCHEMA_V1 1u
typedef struct {
    const char *semantic_domain;
    const yvex_transformer_linear_requirement *requirement;
    unsigned long long input_rows;
} yvex_transformer_linear_compile_request;
typedef struct {
    unsigned int schema_version;
    unsigned long long input_rows, workspace_bytes, input_pack_bytes;
    unsigned long long plan_host_bytes, prepared_weight_bytes;
    unsigned long long preparation_nanoseconds, algorithm_selection_count, use_count;
    char identity[YVEX_SHA256_HEX_BYTES];
    int accelerated_matrix, exact;
} yvex_transformer_linear_executable_summary;
struct yvex_component_encoded_weight;
typedef struct {
    yvex_transformer_linear_executable *executable;
    const struct yvex_component_encoded_weight *weight;
    const yvex_device_tensor *input;
    yvex_device_tensor *output;
} yvex_transformer_linear_execution_request;
typedef struct yvex_component_encoded_weight yvex_transformer_encoded_weight;
typedef enum {
    YVEX_TRANSFORMER_DENSE_NORM1 = 0,
    YVEX_TRANSFORMER_DENSE_QKV_WEIGHT,
    YVEX_TRANSFORMER_DENSE_QKV_BIAS,
    YVEX_TRANSFORMER_DENSE_ATTENTION_WEIGHT,
    YVEX_TRANSFORMER_DENSE_ATTENTION_BIAS,
    YVEX_TRANSFORMER_DENSE_SCALE1,
    YVEX_TRANSFORMER_DENSE_NORM2,
    YVEX_TRANSFORMER_DENSE_FF1_WEIGHT,
    YVEX_TRANSFORMER_DENSE_FF1_BIAS,
    YVEX_TRANSFORMER_DENSE_FF2_WEIGHT,
    YVEX_TRANSFORMER_DENSE_FF2_BIAS,
    YVEX_TRANSFORMER_DENSE_SCALE2
} yvex_transformer_dense_decoder_weight_slot;
typedef struct yvex_transformer_dense_decoder_request {
    const yvex_transformer_encoded_weight *block_weights;
    const yvex_transformer_encoded_weight *final_norm_weight;
    const yvex_transformer_encoded_weight *final_norm_bias;
    const yvex_transformer_encoded_weight *output_weight;
    const yvex_transformer_encoded_weight *output_bias;
    const float *hidden, *cosines, *sines;
    unsigned long long rows, output_rows, width, heads, head_dim, rotary_dim;
    unsigned long long ffn_width, block_count, output_width, output_capacity;
    float epsilon;
    float *output;
    int (*cancel_requested)(void *context);
    void *cancel_context;
} yvex_transformer_dense_decoder_request;
typedef struct yvex_transformer_dense_decoder_result {
    unsigned long long rows, output_rows, block_count, output_values;
    unsigned long long kernel_launches, h2d_bytes, d2h_bytes, device_bytes;
    int complete;
} yvex_transformer_dense_decoder_result;
/* Full attention is one semantic operation here; physical tiling, command submission, and
   workspace layout remain backend-owned. The numeric contract prevents an optimized backend
   from silently changing the admitted accumulation or output precision. */
typedef enum {
    YVEX_TRANSFORMER_ATTENTION_LAYOUT_UNKNOWN = 0,
    YVEX_TRANSFORMER_ATTENTION_LAYOUT_TOKEN_HEAD_DIM
} yvex_transformer_attention_layout;
typedef enum {
    YVEX_TRANSFORMER_ATTENTION_MASK_UNKNOWN = 0,
    YVEX_TRANSFORMER_ATTENTION_MASK_FULL,
    YVEX_TRANSFORMER_ATTENTION_MASK_CAUSAL
} yvex_transformer_attention_mask;
typedef enum {
    YVEX_TRANSFORMER_ATTENTION_NUMERIC_UNKNOWN = 0,
    YVEX_TRANSFORMER_ATTENTION_NUMERIC_EXACT_F32
} yvex_transformer_attention_numeric_contract;
typedef struct yvex_transformer_attention_requirement {
    unsigned long long query_tokens, key_value_tokens, query_start;
    unsigned long long query_heads, key_value_heads, head_dimension;
    /* Zero selects the packed token/head/dimension row width. Non-zero strides
     * admit authenticated subviews such as [Q|gate] and [K|V] without copying
     * the retained prefix. Strides are measured in F32 elements per token. */
    unsigned long long query_token_stride, key_token_stride, value_token_stride;
    yvex_dtype query_dtype, key_dtype, value_dtype, output_dtype;
    yvex_transformer_attention_layout layout;
    yvex_transformer_attention_mask mask;
    yvex_transformer_attention_numeric_contract numeric_contract;
    int deterministic;
} yvex_transformer_attention_requirement;
typedef struct yvex_transformer_attention_request {
    yvex_transformer_attention_requirement requirement;
    const yvex_device_tensor *query, *key, *value;
    yvex_device_tensor *output;
} yvex_transformer_attention_request;
struct yvex_backend_transformer_operations {
    int (*residual_post)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
                         const yvex_device_tensor *, const yvex_device_tensor *,
                         unsigned long long, unsigned long long, unsigned long long,
                         yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*initial)(yvex_backend *, const yvex_device_tensor *, unsigned int,
                   unsigned long long, unsigned long long, unsigned long long,
                   yvex_device_tensor *, yvex_device_tensor *,
                   yvex_backend_operation_facts *, yvex_error *);
    int (*feature_mean)(yvex_backend *, const yvex_device_tensor *,
                        unsigned long long, unsigned long long, unsigned long long,
                        yvex_device_tensor *, yvex_device_tensor *, unsigned long long,
                        unsigned long long, unsigned long long, float *,
                        yvex_backend_operation_facts *, yvex_error *);
    int (*final)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
                 const yvex_device_tensor *, const yvex_device_tensor *,
                 const yvex_device_tensor *, unsigned long long, unsigned long long,
                 unsigned long long, double, double, yvex_device_tensor *,
                 yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*attention_workspace_required)(const yvex_transformer_attention_requirement *,
                                        unsigned long long *, yvex_error *);
    int (*attention_execute)(yvex_backend *, const yvex_transformer_attention_request *,
                             yvex_backend_operation_facts *, yvex_error *);
    int (*gated_delta_workspace_required)(const yvex_gated_delta_plan *,
                                          unsigned long long,
                                          unsigned long long *, yvex_error *);
    int (*gated_delta_execute)(yvex_backend *, const yvex_gated_delta_plan *,
                               const yvex_gated_delta_device_request *,
                               yvex_gated_delta_device_result *,
                               yvex_backend_operation_facts *, yvex_error *);
    int (*linear_workspace_required)(const yvex_transformer_linear_compile_request *,
                                     unsigned long long *, yvex_error *);
    int (*linear_compile)(yvex_backend *, const yvex_transformer_linear_compile_request *,
                          yvex_transformer_linear_executable **,
                          yvex_transformer_linear_executable_summary *, yvex_error *);
    int (*linear_execute)(yvex_backend *, const yvex_transformer_linear_execution_request *,
                          yvex_backend_operation_facts *, yvex_error *);
    int (*linear_summary)(const yvex_transformer_linear_executable *,
                          yvex_transformer_linear_executable_summary *, yvex_error *);
    int (*linear_release)(yvex_backend *, yvex_transformer_linear_executable **,
                          yvex_error *);
    int (*rotary_half_f32)(yvex_backend *, yvex_device_tensor *,
                           const yvex_device_tensor *, const yvex_device_tensor *,
                           unsigned long long, unsigned long long,
                           unsigned long long, unsigned long long,
                           yvex_backend_operation_facts *, yvex_error *);
    int (*split_interleaved_two_f32)(
        yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_device_tensor *, unsigned long long, unsigned long long,
        unsigned long long, yvex_backend_operation_facts *, yvex_error *);
    int (*silu_product_bf16)(yvex_backend *, const yvex_device_tensor *,
                             const yvex_device_tensor *, yvex_device_tensor *,
                             unsigned long long, yvex_backend_operation_facts *,
                             yvex_error *);
    int (*sigmoid_product_bf16)(yvex_backend *, const yvex_device_tensor *,
                                const yvex_device_tensor *, yvex_device_tensor *,
                                unsigned long long, yvex_backend_operation_facts *,
                                yvex_error *);
    int (*add_bf16)(yvex_backend *, const yvex_device_tensor *,
                    const yvex_device_tensor *, yvex_device_tensor *,
                    unsigned long long, unsigned long long,
                    yvex_backend_operation_facts *, yvex_error *);
    int (*bf16_round)(yvex_backend *, yvex_device_tensor *, unsigned long long,
                      yvex_backend_operation_facts *, yvex_error *);
    int (*dense_decoder_execute)(yvex_backend *,
                                 const yvex_transformer_dense_decoder_request *,
                                 yvex_transformer_dense_decoder_result *, yvex_error *);
};

#ifdef __cplusplus
}
#endif
#endif
