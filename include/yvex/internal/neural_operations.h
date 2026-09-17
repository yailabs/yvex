/* Admitted numerical-operation ABI: operands, geometry, precision and backend
 * implementation contracts. No source schema, model plan or runner lifetime.
 * Internal operation tables evolve with all compiled consumers. */
#ifndef INCLUDE_YVEX_INTERNAL_NEURAL_OPERATIONS_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_NEURAL_OPERATIONS_H_INCLUDED
#include <yvex/internal/backend.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct yvex_gated_delta_plan yvex_gated_delta_plan;
typedef struct yvex_gated_delta_device_request yvex_gated_delta_device_request;
typedef struct yvex_gated_delta_device_result yvex_gated_delta_device_result;
struct yvex_convolution_2d_geometry;
/* Pure bounded gate-product numerical contract shared by compiled execution
 * and selected-expert compatibility. The caller owns publication on success. */
int yvex_clamped_swiglu_bf16(const float *, const float *, unsigned long long,
    double limit, float route_weight, float *, yvex_error *);
/* Pure hyperconnection ingress. Geometry describes residual streams, never
 * attention heads, sequence state, a model family, or physical allocation.
 * The collapsed result rounds to BF16/RNE represented in F32; gates and the
 * source-to-target mixing matrix remain F32. Scratch is caller-owned. */
typedef struct {
    unsigned long long streams, width, sinkhorn_iterations;
    double rms_epsilon, epsilon, post_multiplier;
} yvex_mhc_geometry;
typedef struct {
    yvex_mhc_geometry geometry;
    const float *residual, *linear_mixes, *scale, *base;
    unsigned long long rows, residual_stride, mix_stride;
    float *collapsed, *post, *combination;
    unsigned long long collapsed_stride, post_stride, combination_stride;
} yvex_mhc_pre_request;
int yvex_mhc_pre_f32(const yvex_mhc_pre_request *, yvex_error *);
typedef struct {
    yvex_mhc_geometry geometry;
    unsigned long long rows;
    /* residual, linear mixes, scale, base -> collapse, post, mixing matrix */
    const yvex_device_tensor *inputs[4];
    yvex_device_tensor *outputs[3], *workspace;
} yvex_mhc_device_request;
int yvex_mhc_pre_admit(yvex_backend *, const yvex_mhc_device_request *,
    yvex_backend_operation_facts *, yvex_error *);
typedef struct yvex_component_encoded_weight {
    const unsigned char *encoded;
    unsigned long long encoded_bytes, row_count, row_width, row_bytes;
    unsigned int qtype;
} yvex_component_encoded_weight;
typedef struct {
    unsigned long long batch, input_channels, output_channels, input_length;
    unsigned long long kernel_size, stride, dilation, padding, output_padding;
    int transposed;
} yvex_convolution_1d_geometry;
int yvex_convolution_1d_output_length(const yvex_convolution_1d_geometry *,
    unsigned long long *, yvex_error *);
int yvex_convolution_1d_f32(const yvex_convolution_1d_geometry *, const float *,
    unsigned long long, const float *, unsigned long long, const float *, unsigned long long,
    const float *, unsigned long long, float *, unsigned long long, yvex_error *);
int yvex_signal_alias_snake_f32(const float *, unsigned long long, unsigned long long,
    unsigned long long, const float *, const float *, const float[12], const float[12],
    float *, float *, unsigned long long, yvex_error *);

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
/* Channel-first signal operations. Geometry and output population are compiled;
 * workspace is caller-owned physical scratch, not a semantic state value. */
typedef struct {
    yvex_convolution_1d_geometry geometry;
    unsigned long long output_length;
    const yvex_transformer_encoded_weight *weight, *gain, *bias;
    const yvex_device_tensor *input;
    yvex_device_tensor *output, *workspace;
} yvex_convolution_1d_request;
typedef struct {
    unsigned long long batch, channels, length;
    const yvex_transformer_encoded_weight *alpha, *beta, *up_filter, *down_filter;
    const yvex_device_tensor *input;
    yvex_device_tensor *output, *workspace;
} yvex_alias_snake_request;
int yvex_convolution_1d_admit(yvex_backend *, const yvex_convolution_1d_request *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_alias_snake_admit(yvex_backend *, const yvex_alias_snake_request *,
    yvex_backend_operation_facts *, yvex_error *);
int yvex_neural_elementwise_admit(yvex_backend *, const yvex_device_tensor *const *, size_t,
    yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
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
    /* Optional implementation workspace, not a model value or sequence state.
     * Its owner retains it until this synchronous invocation completes. */
    yvex_device_tensor *workspace;
} yvex_transformer_attention_request;
struct yvex_backend_transformer_operations {
    int (*modulate_bf16)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        const unsigned int *, yvex_device_tensor *, unsigned long long, unsigned long long,
        unsigned long long, unsigned long long, unsigned int, unsigned int,
        yvex_backend_operation_facts *, yvex_error *);
    int (*gated_residual_bf16)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        const unsigned int *, const yvex_device_tensor *, yvex_device_tensor *, unsigned long long,
        unsigned long long, unsigned long long, unsigned long long, unsigned int,
        yvex_backend_operation_facts *, yvex_error *);
    /* Ordered F32 accumulation. Mean retains the admitted implementation's
     * final scaling and rounding contract. */
    int (*combine_f32)(yvex_backend *, const yvex_device_tensor *const *, size_t, int,
        yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*clamp_f32)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *, float, float,
        yvex_backend_operation_facts *, yvex_error *);
    /* Stable F64 SiLU(min(gate, limit)) * clamp(up, -limit, limit),
     * then F32 -> BF16 RNE publication. Inputs are never rounded or mutated. */
    int (*clamped_swiglu_bf16)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        yvex_device_tensor *, double, yvex_backend_operation_facts *, yvex_error *);
    int (*convolution_1d)(yvex_backend *, const yvex_convolution_1d_request *,
        yvex_backend_operation_facts *, yvex_error *);
    int (*convolution_2d)(yvex_backend *, const struct yvex_convolution_2d_geometry *,
        const yvex_device_tensor *, const yvex_component_encoded_weight *, const yvex_component_encoded_weight *,
        yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*spatial_norm_silu)(yvex_backend *, const yvex_device_tensor *,
        const yvex_component_encoded_weight *, const yvex_component_encoded_weight *,
        unsigned long long, float, yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*alias_snake)(yvex_backend *, const yvex_alias_snake_request *,
        yvex_backend_operation_facts *, yvex_error *);
    /* F32 affine and normalization contracts retain exact parameter precision.
     * Backend implementations retain their admitted accumulation order. */
    int (*linear_bias_f32)(yvex_backend *, const unsigned char *, unsigned long long,
        const yvex_device_tensor *, unsigned long long, unsigned long long,
        unsigned long long, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_backend_operation_facts *, yvex_error *);
    int (*sinusoidal_embedding)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        unsigned long long, unsigned long long, float, yvex_backend_operation_facts *, yvex_error *);
    int (*linear_bias_target)(yvex_backend *, const char *, const unsigned char *, unsigned long long,
        const unsigned char *, unsigned long long, unsigned long long, unsigned long long,
        unsigned long long, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_backend_operation_facts *, yvex_error *);
    int (*normalization_f32)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        const yvex_device_tensor *, yvex_device_tensor *, unsigned long long, unsigned long long,
        double, yvex_backend_operation_facts *, yvex_error *);
    /* F64 epsilon/inverse/scaling and F32-to-BF16 publication; retained
     * backend reduction order, with no implicit conversion of epsilon. */
    int (*weighted_rms_bf16)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        yvex_device_tensor *, unsigned long long, unsigned long long, double,
        yvex_backend_operation_facts *, yvex_error *);
    int (*channel_bias)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        yvex_device_tensor *, unsigned long long, unsigned long long, int,
        yvex_backend_operation_facts *, yvex_error *);
    int (*scaled_residual_f32)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        const yvex_device_tensor *, yvex_device_tensor *, unsigned long long, unsigned long long,
        yvex_backend_operation_facts *, yvex_error *);
    int (*split_interleaved_three)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_device_tensor *, yvex_device_tensor *, unsigned long long, unsigned long long,
        unsigned long long, yvex_backend_operation_facts *, yvex_error *);
    int (*swiglu_split_f32)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        unsigned long long, unsigned long long, int, yvex_backend_operation_facts *, yvex_error *);
    int (*swiglu_split_bf16)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        unsigned long long, unsigned long long, yvex_backend_operation_facts *, yvex_error *);
    int (*linear_bias_bf16)(yvex_backend *, const unsigned char *, unsigned long long,
        const unsigned char *, unsigned long long, unsigned long long, unsigned long long,
        unsigned long long, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_backend_operation_facts *, yvex_error *);
    int (*layer_norm_f32)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
        const yvex_device_tensor *, yvex_device_tensor *, unsigned long long, unsigned long long,
        float, yvex_backend_operation_facts *, yvex_error *);
    int (*gelu)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        unsigned long long, int, int, yvex_backend_operation_facts *, yvex_error *);
    int (*split_three)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
        yvex_device_tensor *, yvex_device_tensor *, unsigned long long, unsigned long long,
        yvex_backend_operation_facts *, yvex_error *);
    /* Packed group normalization preserves the vector-four reduction and
     * BF16 publication contract selected by physical lowering. */
    int (*group_rms_norm_bf16)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
                              yvex_device_tensor *, unsigned long long, unsigned long long, float,
                              yvex_backend_operation_facts *, yvex_error *);
    int (*residual_post)(yvex_backend *, const yvex_device_tensor *, const yvex_device_tensor *,
                         const yvex_device_tensor *, const yvex_device_tensor *,
                         unsigned long long, unsigned long long, unsigned long long,
                         yvex_device_tensor *, yvex_backend_operation_facts *, yvex_error *);
    int (*residual_pre)(yvex_backend *, const yvex_mhc_device_request *,
                        yvex_backend_operation_facts *, yvex_error *);
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
    int (*rotary_half_bf16)(yvex_backend *, yvex_device_tensor *,
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
    int (*silu)(yvex_backend *, const yvex_device_tensor *, yvex_device_tensor *,
                unsigned long long, int, yvex_backend_operation_facts *, yvex_error *);
    int (*add_bf16)(yvex_backend *, const yvex_device_tensor *,
                    const yvex_device_tensor *, yvex_device_tensor *,
                    unsigned long long, unsigned long long,
                    yvex_backend_operation_facts *, yvex_error *);
    int (*bf16_round)(yvex_backend *, yvex_device_tensor *, unsigned long long,
                      yvex_backend_operation_facts *, yvex_error *);
};

#ifdef __cplusplus
}
#endif
#endif
