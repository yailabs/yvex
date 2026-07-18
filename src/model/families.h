/*
 * src/model/families.h - canonical DeepSeek-V4 family contract.
 *
 * Owner: src/model/families.
 * Owns: the immutable architecture, exact tensor coverage, artifact-neutral
 * transformation recipe, GGUF lowering projection, payload binding, and
 * typed attention composition contract for the admitted DeepSeek-V4 profile.
 * Does not own: source IO, numeric byte execution, artifact publication,
 * backend kernels, persistent KV, transformer composition, or generation.
 * Invariants: one family header exposes one identity-preserving registration
 * boundary; family facts never become an independent generic registry.
 * Boundary: a complete family recipe is not runtime-generation admission.
 */
#ifndef YVEX_MODEL_FAMILY_DEEPSEEK_V4_H
#define YVEX_MODEL_FAMILY_DEEPSEEK_V4_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/native_weights.h>

struct yvex_source_verification;

#define YVEX_DEEPSEEK_V4_IR_NO_LAYER (~0ull)

typedef enum {
    YVEX_DEEPSEEK_V4_IR_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_NOT_VERIFIED,
    YVEX_DEEPSEEK_V4_IR_FAILURE_IDENTITY_MISMATCH,
    YVEX_DEEPSEEK_V4_IR_FAILURE_SOURCE_FACT_MISSING,
    YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_LENGTH,
    YVEX_DEEPSEEK_V4_IR_FAILURE_SCHEDULE_PATTERN,
    YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_COMPRESSION,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_DIMENSION,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_GROUP_GEOMETRY,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_POSITION,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_MHC,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_ROUTING,
    YVEX_DEEPSEEK_V4_IR_FAILURE_INVALID_EXPERT_TOPK,
    YVEX_DEEPSEEK_V4_IR_FAILURE_TOKENIZER_OUTPUT_MISMATCH,
    YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_SOURCE_CONSTRAINT,
    YVEX_DEEPSEEK_V4_IR_FAILURE_UNSUPPORTED_RUNTIME_NUMERIC,
    YVEX_DEEPSEEK_V4_IR_FAILURE_NUMERIC_VALUE,
    YVEX_DEEPSEEK_V4_IR_FAILURE_ARITHMETIC_OVERFLOW,
    YVEX_DEEPSEEK_V4_IR_FAILURE_ALLOCATION
} yvex_deepseek_v4_ir_failure_code;

typedef enum {
    YVEX_DEEPSEEK_V4_IR_COMPONENT_NONE = 0,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_IDENTITY,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_MODEL,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_ATTENTION,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_POSITION,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_MHC,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_MOE,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_OUTPUT,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_TOKENIZER,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_AUXILIARY,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_SOURCE_CONSTRAINT,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_RUNTIME_NUMERIC,
    YVEX_DEEPSEEK_V4_IR_COMPONENT_ALLOCATION
} yvex_deepseek_v4_ir_component;

typedef enum {
    YVEX_DEEPSEEK_V4_ATTENTION_SWA = 0,
    YVEX_DEEPSEEK_V4_ATTENTION_CSA,
    YVEX_DEEPSEEK_V4_ATTENTION_HCA
} yvex_deepseek_v4_attention_class;

typedef enum {
    YVEX_DEEPSEEK_V4_KV_SWA = 0,
    YVEX_DEEPSEEK_V4_KV_CSA,
    YVEX_DEEPSEEK_V4_KV_HCA
} yvex_deepseek_v4_kv_class;

typedef enum {
    YVEX_DEEPSEEK_V4_ROUTER_HASH_TOKEN_ID = 0,
    YVEX_DEEPSEEK_V4_ROUTER_LEARNED_HIDDEN_STATE
} yvex_deepseek_v4_router_class;

typedef enum {
    YVEX_DEEPSEEK_V4_MHC_STANDALONE_PRE = 0,
    YVEX_DEEPSEEK_V4_MHC_FUSED_PRIOR_POST_PRE
} yvex_deepseek_v4_mhc_entry;

typedef enum {
    YVEX_DEEPSEEK_V4_SCORING_SQRT_SOFTPLUS = 0
} yvex_deepseek_v4_scoring_policy;

typedef enum {
    YVEX_DEEPSEEK_V4_TOPK_NOAUX_TC = 0
} yvex_deepseek_v4_topk_policy;

typedef enum {
    YVEX_DEEPSEEK_V4_ACTIVATION_SILU = 0
} yvex_deepseek_v4_activation;

typedef enum {
    YVEX_DEEPSEEK_V4_SOURCE_WEIGHT_BF16 = 0
} yvex_deepseek_v4_source_weight_dtype;

typedef enum {
    YVEX_DEEPSEEK_V4_SOURCE_EXPERT_FP4 = 0
} yvex_deepseek_v4_source_expert_dtype;

typedef enum {
    YVEX_DEEPSEEK_V4_SOURCE_QUANT_FP8_E4M3_UE8M0_DYNAMIC = 0
} yvex_deepseek_v4_source_quantization;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_ATTENTION_KV_NON_ROPE,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_NON_ROTATED,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_COMPRESSOR_ROTATED,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_INDEXER_QUERY_ROTATED
} yvex_deepseek_v4_runtime_activation_stage;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT,
    YVEX_DEEPSEEK_V4_RUNTIME_ACTIVATION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT
} yvex_deepseek_v4_runtime_activation_quantization;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_AXIS_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_AXIS_FINAL_DIMENSION
} yvex_deepseek_v4_runtime_axis;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_SCALE_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_SCALE_UE8M0
} yvex_deepseek_v4_runtime_scale_format;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_TRANSFORM_DAO_FHT_V1_1_0_POST2
} yvex_deepseek_v4_runtime_transform;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_TAIL_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_TAIL_EXACT_OR_SHORT_FINAL_BLOCK
} yvex_deepseek_v4_runtime_tail_policy;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_NONFINITE_REFUSE = 0
} yvex_deepseek_v4_runtime_nonfinite_policy;

typedef enum {
    YVEX_DEEPSEEK_V4_RUNTIME_TOPK_NONE = 0,
    YVEX_DEEPSEEK_V4_RUNTIME_TOPK_YVEX_SCORE_DESC_ORDINAL_ASC_V1
} yvex_deepseek_v4_runtime_sparse_topk_policy_id;

typedef struct {
    int required;
    yvex_deepseek_v4_runtime_activation_stage stage;
    yvex_deepseek_v4_runtime_activation_quantization quantization;
    yvex_deepseek_v4_runtime_axis block_axis;
    unsigned long long block_width;
    yvex_deepseek_v4_runtime_scale_format scale_format;
    yvex_native_dtype scale_dtype;
    yvex_deepseek_v4_runtime_transform pre_transform;
    yvex_deepseek_v4_runtime_tail_policy tail_policy;
    yvex_deepseek_v4_runtime_nonfinite_policy nonfinite_policy;
    int fake_quant_inplace;
    int zero_pad_hadamard_to_power_of_two;
} yvex_deepseek_v4_runtime_activation_policy;

typedef struct {
    int required;
    unsigned int version;
    yvex_deepseek_v4_runtime_sparse_topk_policy_id policy;
    unsigned long long k;
    int reject_nonfinite;
    int score_descending;
    int equal_score_ordinal_ascending;
    int plus_zero_equals_minus_zero;
    int duplicate_ordinal_refused;
    int output_ranked_order;
} yvex_deepseek_v4_runtime_sparse_topk_policy;

typedef struct {
    yvex_deepseek_v4_ir_failure_code code;
    yvex_deepseek_v4_ir_component component;
    const char *field;
    unsigned long long layer_index;
    unsigned long long expected;
    unsigned long long actual;
} yvex_deepseek_v4_ir_failure;

typedef struct {
    unsigned long long rope_dimension;
    unsigned long long theta;
    unsigned long long scaling_factor;
    unsigned long long original_context;
    unsigned long long beta_fast;
    unsigned long long beta_slow;
    unsigned long long maximum_context;
    int partial_rope;
    int inverse_output_rotation;
} yvex_deepseek_v4_position_spec;

typedef struct {
    yvex_deepseek_v4_kv_class class_id;
    unsigned long long compression_ratio;
    unsigned long long sliding_window;
    int requires_state_cache;
    int requires_uncompressed_tail;
    int requires_compressed_core;
    int requires_indexer_cache;
} yvex_deepseek_v4_kv_spec;

typedef struct {
    unsigned long long residual_streams;
    unsigned long long stream_width;
    unsigned long long expanded_width;
    unsigned long long mixing_rows;
    unsigned long long mixing_columns;
    unsigned long long base_width;
    unsigned long long scale_width;
    unsigned long long sinkhorn_iterations;
    double epsilon;
    double residual_post_multiplier;
    yvex_deepseek_v4_mhc_entry entry;
    int attention_pre_and_post;
    int ffn_pre_and_deferred_post;
} yvex_deepseek_v4_mhc_spec;

typedef struct {
    int required;
    unsigned long long width;
} yvex_deepseek_v4_norm_spec;

typedef struct {
    int required;
    unsigned long long function_rows;
    unsigned long long function_columns;
    unsigned long long base_width;
    unsigned long long scale_width;
} yvex_deepseek_v4_mhc_head_spec;

typedef struct {
    unsigned long long q_a_rows;
    unsigned long long q_a_columns;
    unsigned long long q_b_rows;
    unsigned long long q_b_columns;
    unsigned long long kv_rows;
    unsigned long long kv_columns;
    unsigned long long o_a_rows;
    unsigned long long o_a_columns;
    unsigned long long o_b_rows;
    unsigned long long o_b_columns;
    unsigned long long compressor_ape_rows;
    unsigned long long compressor_ape_columns;
    unsigned long long compressor_norm_width;
    unsigned long long compressor_projection_rows;
    unsigned long long compressor_projection_columns;
    unsigned long long indexer_ape_rows;
    unsigned long long indexer_ape_columns;
    unsigned long long indexer_norm_width;
    unsigned long long indexer_projection_rows;
    unsigned long long indexer_projection_columns;
    unsigned long long indexer_query_rows;
    unsigned long long indexer_query_columns;
    unsigned long long indexer_weight_rows;
    unsigned long long indexer_weight_columns;
} yvex_deepseek_v4_attention_tensor_spec;

typedef struct {
    yvex_deepseek_v4_router_class router_class;
    yvex_deepseek_v4_scoring_policy scoring;
    yvex_deepseek_v4_topk_policy topk_policy;
    yvex_deepseek_v4_activation activation;
    unsigned long long routed_experts;
    unsigned long long shared_experts;
    unsigned long long experts_per_token;
    unsigned long long expert_intermediate_size;
    unsigned long long shared_intermediate_size;
    unsigned long long hash_table_rows;
    unsigned long long hash_table_columns;
    unsigned long long correction_bias_width;
    double routed_scaling_factor;
    double activation_limit;
    int requires_token_ids;
    int requires_hidden_state;
    int requires_correction_bias;
    int normalize_topk_probabilities;
} yvex_deepseek_v4_moe_spec;

typedef struct {
    unsigned long long layer_index;
    yvex_deepseek_v4_attention_class attention_class;
    unsigned long long compression_ratio;
    unsigned long long query_heads;
    unsigned long long kv_heads;
    unsigned long long head_dimension;
    unsigned long long rope_head_dimension;
    unsigned long long non_rope_head_dimension;
    unsigned long long query_lora_rank;
    unsigned long long output_lora_rank;
    unsigned long long output_groups;
    unsigned long long output_heads_per_group;
    unsigned long long output_group_input_width;
    unsigned long long indexer_heads;
    unsigned long long indexer_head_dimension;
    unsigned long long indexer_topk;
    unsigned long long attention_sink_count;
    double attention_dropout;
    int causal;
    int attention_bias;
    int query_norm_required;
    int kv_norm_required;
    int compressor_required;
    int indexer_required;
    yvex_deepseek_v4_position_spec position;
    yvex_deepseek_v4_kv_spec kv;
    yvex_deepseek_v4_mhc_spec mhc;
    yvex_deepseek_v4_moe_spec moe;
    yvex_deepseek_v4_runtime_activation_policy attention_kv_activation;
    yvex_deepseek_v4_runtime_activation_policy compressor_activation;
    yvex_deepseek_v4_runtime_activation_policy compressor_rotated_activation;
    yvex_deepseek_v4_runtime_activation_policy indexer_query_activation;
    yvex_deepseek_v4_runtime_sparse_topk_policy sparse_topk;
    yvex_deepseek_v4_norm_spec attention_input_norm;
    yvex_deepseek_v4_norm_spec post_attention_ffn_norm;
    yvex_deepseek_v4_attention_tensor_spec tensors;
    double rms_norm_epsilon;
} yvex_deepseek_v4_layer_spec;

typedef struct {
    yvex_deepseek_v4_layer_spec layer;
    unsigned long long predictor_index;
    unsigned long long previous_hidden_width;
    unsigned long long embedding_projection_input;
    unsigned long long embedding_projection_output;
    unsigned long long hidden_projection_input;
    unsigned long long hidden_projection_output;
    int requires_token_embedding;
    int requires_previous_hidden_state;
    int requires_embedding_norm;
    int requires_hidden_norm;
    int requires_separate_mhc_head;
    yvex_deepseek_v4_mhc_head_spec mhc_head;
    int shares_output_head;
    int shares_final_norm;
} yvex_deepseek_v4_auxiliary_spec;

typedef struct {
    int required;
    unsigned long long vocabulary_size;
    unsigned long long hidden_size;
} yvex_deepseek_v4_embedding_spec;

typedef struct {
    int required;
    int tied_to_embedding;
    unsigned long long input_width;
    unsigned long long vocabulary_size;
} yvex_deepseek_v4_output_spec;

typedef struct {
    char tokenizer_class[64];
    char model_type[32];
    unsigned long long vocabulary_size;
    unsigned long long base_vocab_entries;
    unsigned long long added_token_entries;
    unsigned long long maximum_token_id;
    unsigned long long maximum_context;
    unsigned long long bos_token_id;
    unsigned long long eos_token_id;
    int bos_required;
    int eos_required;
} yvex_deepseek_v4_tokenizer_spec;

typedef struct {
    yvex_deepseek_v4_source_weight_dtype weight_dtype;
    yvex_deepseek_v4_source_expert_dtype expert_dtype;
    yvex_deepseek_v4_source_quantization quantization;
    unsigned long long quant_block_rows;
    unsigned long long quant_block_columns;
    unsigned long long fp4_packing_factor;
    unsigned long long fp4_scale_group_width;
    yvex_native_dtype fp4_physical_dtype;
    yvex_native_dtype scale_dtype;
} yvex_deepseek_v4_source_constraint;

typedef struct {
    char target_id[128];
    char family[32];
    char architecture[128];
    char repository[256];
    char revision[128];
    char verification_stage[64];
    char paper_revision[32];
    char sglang_revision[64];
    char vllm_revision[64];
    char hadamard_revision[128];
    unsigned int runtime_numeric_schema_version;
    unsigned long long runtime_activation_policy_count;
    unsigned long long runtime_sparse_topk_policy_count;
    unsigned long long hidden_size;
    unsigned long long vocabulary_size;
    unsigned long long maximum_context;
    unsigned long long main_layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long swa_layer_count;
    unsigned long long csa_layer_count;
    unsigned long long hca_layer_count;
    unsigned long long hash_router_layer_count;
    unsigned long long learned_router_layer_count;
    unsigned long long source_header_scan_count;
    unsigned long long source_header_tensor_count;
    unsigned long long source_payload_bytes_read;
    yvex_deepseek_v4_embedding_spec embedding;
    yvex_deepseek_v4_output_spec output;
    yvex_deepseek_v4_tokenizer_spec tokenizer;
    yvex_deepseek_v4_source_constraint source_constraint;
    yvex_deepseek_v4_mhc_spec final_mhc;
    yvex_deepseek_v4_mhc_head_spec final_mhc_head;
    double final_norm_epsilon;
    int use_cache;
    int final_mhc_post_required;
    int final_mhc_head_required;
    int final_norm_after_mhc_head;
} yvex_deepseek_v4_model_spec;

typedef void *(*yvex_deepseek_v4_ir_allocate_fn)(size_t size, void *context);
typedef void (*yvex_deepseek_v4_ir_release_fn)(void *allocation,
                                               void *context);

typedef struct {
    yvex_deepseek_v4_ir_allocate_fn allocate;
    yvex_deepseek_v4_ir_release_fn release;
    void *context;
} yvex_deepseek_v4_ir_allocator;

typedef struct yvex_deepseek_v4_ir yvex_deepseek_v4_ir;

#include "src/source/inventory.h"

typedef enum {
    YVEX_DEEPSEEK_TENSOR_COLLECTION_GLOBAL = 0,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ATTENTION,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_COMPRESSOR,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_INDEXER,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_NORM,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_MHC,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTER,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_ROUTED_EXPERT,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_SHARED_EXPERT,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_AUXILIARY,
    YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT
} yvex_deepseek_tensor_collection;

typedef enum {
    YVEX_DEEPSEEK_TENSOR_SCOPE_GLOBAL = 0,
    YVEX_DEEPSEEK_TENSOR_SCOPE_MAIN_LAYER,
    YVEX_DEEPSEEK_TENSOR_SCOPE_MTP
} yvex_deepseek_tensor_scope;

typedef enum {
    YVEX_DEEPSEEK_COVERAGE_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SOURCE_IDENTITY,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_AUTHORITY,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVENTORY_DRIFT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ARCHITECTURE_INCOMPLETE,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_MISSING_REQUIREMENT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_AMBIGUOUS_MATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_UNEXPECTED_SOURCE,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_INVALID_INDEX,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_RANK_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SHAPE_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_DTYPE_MISMATCH,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_SCALE_COMPANION,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ARITHMETIC_OVERFLOW,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_RESOURCE_LIMIT,
    YVEX_DEEPSEEK_COVERAGE_FAILURE_ALLOCATION
} yvex_deepseek_tensor_coverage_failure_code;

#define YVEX_DEEPSEEK_TENSOR_NO_INDEX (~0ull)

typedef struct {
    yvex_deepseek_tensor_coverage_failure_code code;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    char tensor_name[256];
    unsigned long long layer_index;
    unsigned long long expert_index;
    unsigned long long dimension_index;
    unsigned long long expected;
    unsigned long long actual;
} yvex_deepseek_tensor_coverage_failure;

typedef struct {
    const yvex_native_weight_info *source;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long expert_index;
} yvex_deepseek_tensor_coverage_row;

typedef struct {
    unsigned long long source_tensor_count;
    unsigned long long required_tensor_count;
    unsigned long long matched_tensor_count;
    unsigned long long missing_count;
    unsigned long long ambiguous_count;
    unsigned long long unexpected_count;
    unsigned long long collection_counts[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT];
    unsigned long long main_layer_count;
    unsigned long long auxiliary_layer_count;
    unsigned long long routed_expert_count;
    unsigned long long shared_expert_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long source_lookup_count;
    unsigned long long source_collision_count;
    unsigned long long source_maximum_probe;
    unsigned long long source_identity;
    unsigned long long coverage_identity;
    int complete;
} yvex_deepseek_tensor_coverage_summary;

typedef void *(*yvex_deepseek_tensor_coverage_allocate_fn)(size_t size,
                                                            void *context);
typedef void (*yvex_deepseek_tensor_coverage_release_fn)(void *allocation,
                                                         void *context);

typedef struct {
    yvex_deepseek_tensor_coverage_allocate_fn allocate;
    yvex_deepseek_tensor_coverage_release_fn release;
    void *context;
    unsigned long long maximum_tensors;
} yvex_deepseek_tensor_coverage_options;

typedef struct yvex_deepseek_tensor_coverage yvex_deepseek_tensor_coverage;

#include "ir.h"

#include "src/source/verify.h"

#define YVEX_DEEPSEEK_TRANSFORM_SOURCE_COUNT 69187ull
#define YVEX_DEEPSEEK_TRANSFORM_TERMINAL_COUNT 1360ull
#define YVEX_DEEPSEEK_TRANSFORM_MAIN_TERMINAL_COUNT 1328ull
#define YVEX_DEEPSEEK_TRANSFORM_AUX_TERMINAL_COUNT 32ull

#include <yvex/tensor.h>

#include "src/model/compilation/ir.h"
#include "src/gguf/map.h"

#define YVEX_DEEPSEEK_GGUF_NO_INDEX (~0ull)
#define YVEX_DEEPSEEK_GGUF_AGGREGATED_AXIS (~0u)
#define YVEX_DEEPSEEK_GGUF_DESCRIPTOR_COUNT 1360ull
#define YVEX_DEEPSEEK_GGUF_TRUNK_DESCRIPTOR_COUNT 1328ull
#define YVEX_DEEPSEEK_GGUF_MTP_DESCRIPTOR_COUNT 32ull
#define YVEX_DEEPSEEK_GGUF_SOURCE_COUNT 69187ull
#define YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY 0x1aecbbe25b04de0dull

typedef enum {
    YVEX_DEEPSEEK_GGUF_TRANSFORM_DIRECT = 0,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_FP8_E4M3_E8M0,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_EXPERT_MXFP4,
    YVEX_DEEPSEEK_GGUF_TRANSFORM_I64_TO_I32
} yvex_deepseek_gguf_transform;

typedef enum {
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_PRIMARY = 0,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_SCALE,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_WEIGHT,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_EXPERT_SCALE,
    YVEX_DEEPSEEK_GGUF_CONTRIBUTION_ROUTING_TABLE
} yvex_deepseek_gguf_contribution_kind;

typedef enum {
    YVEX_DEEPSEEK_GGUF_METADATA_STRING = 0,
    YVEX_DEEPSEEK_GGUF_METADATA_U64,
    YVEX_DEEPSEEK_GGUF_METADATA_F64,
    YVEX_DEEPSEEK_GGUF_METADATA_BOOL,
    YVEX_DEEPSEEK_GGUF_METADATA_U64_ARRAY,
    YVEX_DEEPSEEK_GGUF_METADATA_F64_ARRAY
} yvex_deepseek_gguf_metadata_type;

typedef enum {
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARCHITECTURE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_COVERAGE_ROW,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MISSING_SOURCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_SOURCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_SOURCE_DTYPE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_EXPERT_SEQUENCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_NAME,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_DUPLICATE_NAME,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LAYOUT,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_METADATA,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ACCOUNTING,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ARITHMETIC_OVERFLOW,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_ALLOCATION,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_TRANSFORM_IR,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_LOWERING_DIVERGENCE,
    YVEX_DEEPSEEK_GGUF_MAP_FAILURE_MAPPING_IDENTITY
} yvex_deepseek_gguf_map_failure_code;

typedef struct {
    yvex_deepseek_gguf_map_failure_code code;
    yvex_tensor_role role;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned long long expert_index;
    unsigned long long expected;
    unsigned long long actual;
    char source_name[256];
    char emitted_name[192];
} yvex_deepseek_gguf_map_failure;

typedef struct {
    char source_name[256];
    yvex_native_dtype source_dtype;
    unsigned int source_rank;
    unsigned long long source_dims[2];
    yvex_deepseek_gguf_contribution_kind kind;
    unsigned long long source_row_index;
    unsigned long long descriptor_index;
    unsigned long long expert_index;
} yvex_deepseek_gguf_contribution;

typedef struct {
    yvex_tensor_role role;
    yvex_deepseek_tensor_collection collection;
    yvex_deepseek_tensor_scope scope;
    unsigned long long layer_index;
    unsigned long long predictor_index;
    unsigned long long expert_count;
    char emitted_name[192];
    yvex_deepseek_gguf_transform transform;
    yvex_gguf_name_provenance name_provenance;
    unsigned int forced_qtype;
    unsigned int logical_rank;
    unsigned long long logical_dims[YVEX_TENSOR_MAX_DIMS];
    unsigned int source_axis_for_logical[YVEX_TENSOR_MAX_DIMS];
    unsigned long long contribution_offset;
    unsigned long long contribution_count;
    unsigned long long identity;
} yvex_deepseek_gguf_descriptor;

typedef struct {
    char key[128];
    yvex_deepseek_gguf_metadata_type type;
    char string_value[192];
    unsigned long long u64_value;
    double f64_value;
    int bool_value;
    unsigned long long array_values[64];
    double f64_array_values[64];
    unsigned int array_count;
} yvex_deepseek_gguf_metadata;

typedef struct {
    unsigned long long source_contribution_count;
    unsigned long long descriptor_count;
    unsigned long long trunk_descriptor_count;
    unsigned long long mtp_descriptor_count;
    unsigned long long pinned_standard_count;
    unsigned long long semantic_standard_count;
    unsigned long long extension_count;
    unsigned long long collection_counts[YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT];
    unsigned long long metadata_count;
    unsigned long long header_scan_count;
    unsigned long long payload_bytes_read;
    unsigned long long source_identity;
    unsigned long long coverage_identity;
    unsigned long long mapping_identity;
    int complete;
} yvex_deepseek_gguf_map_summary;

typedef void *(*yvex_deepseek_gguf_map_allocate_fn)(size_t, void *);
typedef void (*yvex_deepseek_gguf_map_release_fn)(void *, void *);

typedef struct {
    yvex_deepseek_gguf_map_allocate_fn allocate;
    yvex_deepseek_gguf_map_release_fn release;
    void *context;
} yvex_deepseek_gguf_map_allocator;

typedef struct yvex_deepseek_gguf_map yvex_deepseek_gguf_map;

#include "src/model/compilation/binding.h"
#include "src/source/payload.h"

#define YVEX_DEEPSEEK_PAYLOAD_MAPPING_IDENTITY \
    YVEX_DEEPSEEK_GGUF_MAPPING_IDENTITY

typedef enum {
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_NONE = 0,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_INVALID_ARGUMENT,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_SOURCE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_ARCHITECTURE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_COVERAGE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_TRANSFORM_IR,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_MAPPING_IDENTITY,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_CONTRIBUTION,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_RANGE,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_BINDING,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_PLAN,
    YVEX_DEEPSEEK_PAYLOAD_FAILURE_ALLOCATION
} yvex_deepseek_payload_failure_code;

typedef struct {
    yvex_deepseek_payload_failure_code code;
    unsigned long long descriptor_index;
    unsigned long long contribution_index;
    yvex_source_payload_failure payload_failure;
} yvex_deepseek_payload_failure;

typedef struct {
    unsigned long long mapping_identity;
    char transform_identity[YVEX_TRANSFORM_IR_IDENTITY_CAP];
    unsigned long long source_snapshot_identity;
    unsigned long long descriptor_count;
    unsigned long long descriptors_covered;
    unsigned long long contribution_count;
    unsigned long long contributions_resolved;
    unsigned long long direct_contributions;
    unsigned long long fp8_weight_contributions;
    unsigned long long e8m0_scale_contributions;
    unsigned long long expert_contributions;
    unsigned long long i64_router_contributions;
    unsigned long long global_contributions;
    unsigned long long norm_contributions;
    unsigned long long shared_expert_contributions;
    unsigned long long output_head_contributions;
    unsigned long long mtp_contributions;
    unsigned long long routed_expert_logical_bytes;
    unsigned long long output_head_logical_bytes;
    unsigned long long range_lookup_count;
    int complete;
} yvex_deepseek_payload_handoff_summary;

typedef struct yvex_deepseek_payload_handoff yvex_deepseek_payload_handoff;

typedef struct {
    const char *source_path;
    const char *models_root;
    const char *manifest_path;
    yvex_source_payload_budget budget;
    size_t chunk_bytes;
    size_t page_bytes;
} yvex_deepseek_payload_handoff_options;

/*
 * One immutable registration is the complete family-visible ABI.  Consumers
 * select typed operations from this table; implementation helpers remain
 * private to the family translation unit.
 */
typedef struct {
    int (*build)(yvex_deepseek_v4_ir **out,
                 const struct yvex_source_verification *verification,
                 yvex_deepseek_v4_ir_failure *failure,
                 yvex_error *err);
    int (*build_with_allocator)(
        yvex_deepseek_v4_ir **out,
        const struct yvex_source_verification *verification,
        const yvex_deepseek_v4_ir_allocator *allocator,
        yvex_deepseek_v4_ir_failure *failure,
        yvex_error *err);
    void (*close)(yvex_deepseek_v4_ir *ir);
    const yvex_deepseek_v4_model_spec *(*model)(
        const yvex_deepseek_v4_ir *ir);
    unsigned long long (*layer_count)(const yvex_deepseek_v4_ir *ir);
    const yvex_deepseek_v4_layer_spec *(*layer_at)(
        const yvex_deepseek_v4_ir *ir, unsigned long long index);
    unsigned long long (*auxiliary_count)(const yvex_deepseek_v4_ir *ir);
    const yvex_deepseek_v4_auxiliary_spec *(*auxiliary_at)(
        const yvex_deepseek_v4_ir *ir, unsigned long long index);
    const char *(*failure_name)(yvex_deepseek_v4_ir_failure_code code);
    const char *(*component_name)(yvex_deepseek_v4_ir_component component);
    const char *(*attention_name)(yvex_deepseek_v4_attention_class class_id);
    const char *(*kv_name)(yvex_deepseek_v4_kv_class class_id);
    const char *(*router_name)(yvex_deepseek_v4_router_class class_id);
    const char *(*source_weight_dtype_name)(
        yvex_deepseek_v4_source_weight_dtype dtype);
    const char *(*source_expert_dtype_name)(
        yvex_deepseek_v4_source_expert_dtype dtype);
    const char *(*source_quantization_name)(
        yvex_deepseek_v4_source_quantization quantization);
    const char *(*activation_stage_name)(
        yvex_deepseek_v4_runtime_activation_stage stage);
    const char *(*activation_quantization_name)(
        yvex_deepseek_v4_runtime_activation_quantization quantization);
    const char *(*runtime_transform_name)(
        yvex_deepseek_v4_runtime_transform transform);
    const char *(*sparse_topk_policy_name)(
        yvex_deepseek_v4_runtime_sparse_topk_policy_id policy);
} yvex_model_family_ir_api;

typedef struct {
    int (*build)(yvex_deepseek_tensor_coverage **out,
                 const yvex_source_verification *verification,
                 const yvex_deepseek_v4_ir *ir,
                 yvex_source_tensor_snapshot *snapshot,
                 const yvex_deepseek_tensor_coverage_options *options,
                 yvex_deepseek_tensor_coverage_failure *failure,
                 yvex_error *err);
    int (*open_verified_source)(
        yvex_deepseek_tensor_coverage **out,
        yvex_source_verification *verification,
        const char *source_path,
        const char *models_root,
        yvex_deepseek_tensor_coverage_failure *failure,
        yvex_error *err);
    void (*close)(yvex_deepseek_tensor_coverage *coverage);
    const yvex_deepseek_tensor_coverage_summary *(*summary)(
        const yvex_deepseek_tensor_coverage *coverage);
    const yvex_deepseek_tensor_coverage_row *(*at)(
        const yvex_deepseek_tensor_coverage *coverage,
        unsigned long long index);
    const yvex_deepseek_tensor_coverage_row *(*find)(
        const yvex_deepseek_tensor_coverage *coverage,
        const char *source_name);
    int (*find_index)(const yvex_deepseek_tensor_coverage *coverage,
                      const char *source_name,
                      unsigned long long *row_index);
    int (*find_source_index)(const yvex_deepseek_tensor_coverage *coverage,
                             const char *source_name,
                             unsigned long long *source_index);
    const char *(*collection_name)(yvex_deepseek_tensor_collection collection);
    const char *(*failure_name)(
        yvex_deepseek_tensor_coverage_failure_code code);
} yvex_model_family_coverage_api;

typedef struct {
    int (*architecture_identity)(
        const yvex_deepseek_v4_ir *architecture,
        char output[YVEX_TRANSFORM_IR_IDENTITY_CAP]);
    int (*build)(yvex_transform_ir **out,
                 const yvex_source_verification *verification,
                 const yvex_deepseek_v4_ir *architecture,
                 const yvex_deepseek_tensor_coverage *coverage,
                 const yvex_transform_builder_options *options,
                 yvex_transform_failure *failure,
                 yvex_error *err);
} yvex_model_family_transform_api;

typedef struct {
    int (*build)(yvex_deepseek_gguf_map **out,
                 const yvex_deepseek_v4_ir *ir,
                 const yvex_transform_ir *transform_ir,
                 yvex_deepseek_gguf_map_failure *failure,
                 yvex_error *err);
    int (*build_with_allocator)(
        yvex_deepseek_gguf_map **out,
        const yvex_deepseek_v4_ir *ir,
        const yvex_transform_ir *transform_ir,
        const yvex_deepseek_gguf_map_allocator *allocator,
        yvex_deepseek_gguf_map_failure *failure,
        yvex_error *err);
    void (*close)(yvex_deepseek_gguf_map *map);
    const yvex_deepseek_gguf_map_summary *(*summary)(
        const yvex_deepseek_gguf_map *map);
    const yvex_deepseek_gguf_descriptor *(*at)(
        const yvex_deepseek_gguf_map *map, unsigned long long index);
    const yvex_deepseek_gguf_contribution *(*contribution_at)(
        const yvex_deepseek_gguf_map *map, unsigned long long index);
    const yvex_deepseek_gguf_descriptor *(*find_source)(
        const yvex_deepseek_gguf_map *map, const char *source_name);
    const yvex_deepseek_gguf_descriptor *(*find_emitted)(
        const yvex_deepseek_gguf_map *map, const char *emitted_name);
    const yvex_deepseek_gguf_descriptor *(*find_role)(
        const yvex_deepseek_gguf_map *map,
        yvex_tensor_role role,
        yvex_deepseek_tensor_scope scope,
        unsigned long long layer_index,
        unsigned long long predictor_index);
    const yvex_deepseek_gguf_metadata *(*metadata_at)(
        const yvex_deepseek_gguf_map *map, unsigned long long index);
    const yvex_deepseek_gguf_metadata *(*metadata_find)(
        const yvex_deepseek_gguf_map *map, const char *key);
    const char *(*transform_name)(yvex_deepseek_gguf_transform transform);
    const char *(*failure_name)(yvex_deepseek_gguf_map_failure_code code);
} yvex_model_family_lowering_api;

typedef struct {
    int (*open)(yvex_deepseek_payload_handoff **out,
                const yvex_deepseek_payload_handoff_options *options,
                yvex_deepseek_payload_failure *failure,
                yvex_error *err);
    void (*close)(yvex_deepseek_payload_handoff *handoff);
    const yvex_deepseek_payload_handoff_summary *(*summary)(
        const yvex_deepseek_payload_handoff *handoff);
    const yvex_source_verification *(*verification)(
        const yvex_deepseek_payload_handoff *handoff);
    const yvex_deepseek_gguf_map *(*map)(
        const yvex_deepseek_payload_handoff *handoff);
    const yvex_transform_ir *(*transform_ir)(
        const yvex_deepseek_payload_handoff *handoff);
    const yvex_transform_binding *(*binding)(
        const yvex_deepseek_payload_handoff *handoff);
    yvex_source_payload_session *(*session)(
        yvex_deepseek_payload_handoff *handoff);
    const yvex_source_payload_plan *(*plan)(
        const yvex_deepseek_payload_handoff *handoff);
    const char *(*failure_name)(yvex_deepseek_payload_failure_code code);
} yvex_model_family_payload_api;

typedef struct {
    unsigned int schema_version;
    const char *family_key;
    yvex_model_family_ir_api ir;
    yvex_model_family_coverage_api coverage;
    yvex_model_family_transform_api transform;
    yvex_model_family_lowering_api lowering;
    yvex_model_family_payload_api payload;
} yvex_model_family_api;

/* Returns a process-lifetime immutable family registration and performs no
 * allocation, mutation, or IO. */
const yvex_model_family_api *yvex_model_register_deepseek_v4(void);

#endif
