/*
 * yvex_deepseek_v4_ir.h - canonical DeepSeek-V4-Flash architecture IR.
 *
 * Owner:
 *   src/model/architecture
 *
 * Owns:
 *   immutable typed model, layer, attention, position, KV, mHC, MoE, output,
 *   tokenizer, source-constraint, and auxiliary topology projected from one
 *   successful exact-source verification result.
 *
 * Does not own:
 *   source parsing, tensor-name discovery, role coverage, GGUF mapping,
 *   payload IO, qtype selection, materialization, graph execution, runtime,
 *   tokenizer execution, or generation.
 *
 * Invariants:
 *   construction consumes verified facts without reopening source files; each
 *   main and auxiliary layer has one explicit descriptor; published objects
 *   are immutable and own their post-source lifetime.
 *
 * Boundary:
 *   an execution-complete architecture specification defines required model
 *   semantics but is not an artifact or runtime capability.
 */
#ifndef YVEX_DEEPSEEK_V4_IR_H
#define YVEX_DEEPSEEK_V4_IR_H

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

int yvex_deepseek_v4_ir_build(
    yvex_deepseek_v4_ir **out,
    const struct yvex_source_verification *verification,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err);

int yvex_deepseek_v4_ir_build_with_allocator(
    yvex_deepseek_v4_ir **out,
    const struct yvex_source_verification *verification,
    const yvex_deepseek_v4_ir_allocator *allocator,
    yvex_deepseek_v4_ir_failure *failure,
    yvex_error *err);

void yvex_deepseek_v4_ir_close(yvex_deepseek_v4_ir *ir);

const yvex_deepseek_v4_model_spec *yvex_deepseek_v4_ir_model(
    const yvex_deepseek_v4_ir *ir);

unsigned long long yvex_deepseek_v4_ir_layer_count(
    const yvex_deepseek_v4_ir *ir);

const yvex_deepseek_v4_layer_spec *yvex_deepseek_v4_ir_layer_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index);

unsigned long long yvex_deepseek_v4_ir_auxiliary_count(
    const yvex_deepseek_v4_ir *ir);

const yvex_deepseek_v4_auxiliary_spec *yvex_deepseek_v4_ir_auxiliary_at(
    const yvex_deepseek_v4_ir *ir,
    unsigned long long index);

const char *yvex_deepseek_v4_ir_failure_name(
    yvex_deepseek_v4_ir_failure_code code);
const char *yvex_deepseek_v4_ir_component_name(
    yvex_deepseek_v4_ir_component component);
const char *yvex_deepseek_v4_attention_name(
    yvex_deepseek_v4_attention_class class_id);
const char *yvex_deepseek_v4_kv_name(yvex_deepseek_v4_kv_class class_id);
const char *yvex_deepseek_v4_router_name(
    yvex_deepseek_v4_router_class class_id);
const char *yvex_deepseek_v4_source_weight_dtype_name(
    yvex_deepseek_v4_source_weight_dtype dtype);
const char *yvex_deepseek_v4_source_expert_dtype_name(
    yvex_deepseek_v4_source_expert_dtype dtype);
const char *yvex_deepseek_v4_source_quantization_name(
    yvex_deepseek_v4_source_quantization quantization);
const char *yvex_deepseek_v4_runtime_activation_stage_name(
    yvex_deepseek_v4_runtime_activation_stage stage);
const char *yvex_deepseek_v4_runtime_activation_quantization_name(
    yvex_deepseek_v4_runtime_activation_quantization quantization);
const char *yvex_deepseek_v4_runtime_transform_name(
    yvex_deepseek_v4_runtime_transform transform);
const char *yvex_deepseek_v4_runtime_sparse_topk_policy_name(
    yvex_deepseek_v4_runtime_sparse_topk_policy_id policy);

#endif
