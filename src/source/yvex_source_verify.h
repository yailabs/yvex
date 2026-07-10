/*
 * yvex_source_verify.h - exact source verification contract.
 *
 * Owner:
 *   src/source
 *
 * Owns:
 *   typed, header-bounded verification facts for one canonical source target.
 *
 * Does not own:
 *   target selection, tensor payload reads, architecture IR, role mapping,
 *   quantization, GGUF emission, materialization, runtime, or generation.
 *
 * Invariants:
 *   verification parses structured sidecars and safetensors headers only;
 *   absent facts remain absent and every refusal has a stable reason.
 *
 * Boundary:
 *   verified source identity is not model or runtime support.
 */
#ifndef YVEX_SOURCE_VERIFY_H
#define YVEX_SOURCE_VERIFY_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/fs.h>

#include "yvex_model_target_catalog.h"

#define YVEX_SOURCE_VERIFY_BLOCKER_CAP 24u
#define YVEX_SOURCE_VERIFY_COMPRESS_RATIO_CAP 128u

typedef struct {
    const yvex_model_target_identity *identity;
    const char *source_path;
    const char *models_root;
    const char *manifest_path;
} yvex_source_verify_options;

typedef struct {
    int verified;
    char resolved_source_path[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char source_kind[32];
    char repository_id[256];
    char manifest_revision[128];
    char revision[128];
    char manifest_status[32];
    int path_verified;
    int repository_verified;
    int revision_verified;
    int config_valid;
    int tokenizer_json_valid;
    int tokenizer_config_valid;
    int generation_config_valid;
    int shard_index_present;
    int shard_index_valid;
    int shard_index_headers_match;
    int footprint_overflow;
    char model_type[64];
    char architecture[128];
    char torch_dtype[32];
    char expert_dtype[32];
    char hidden_act[32];
    char scoring_func[32];
    char topk_method[32];
    char tokenizer_model_type[32];
    char rope_scaling_type[32];
    char quant_method[32];
    char quant_format[32];
    char tokenizer_class[64];
    char generation_transformers_version[32];
    char generation_temperature[32];
    char generation_top_p[32];
    unsigned long long hidden_size;
    unsigned long long num_hidden_layers;
    unsigned long long num_attention_heads;
    unsigned long long num_key_value_heads;
    unsigned long long head_dim;
    unsigned long long qk_rope_head_dim;
    unsigned long long max_position_embeddings;
    unsigned long long moe_intermediate_size;
    unsigned long long n_routed_experts;
    unsigned long long n_shared_experts;
    unsigned long long num_experts_per_tok;
    unsigned long long num_hash_layers;
    unsigned long long q_lora_rank;
    unsigned long long o_lora_rank;
    unsigned long long vocab_size;
    unsigned long long sliding_window;
    unsigned long long bos_token_id;
    unsigned long long eos_token_id;
    unsigned long long compress_ratios[YVEX_SOURCE_VERIFY_COMPRESS_RATIO_CAP];
    unsigned long long compress_ratio_count;
    unsigned long long compress_rope_theta;
    char attention_dropout[32];
    char hc_eps[32];
    unsigned long long hc_mult;
    unsigned long long hc_sinkhorn_iters;
    unsigned long long index_head_dim;
    unsigned long long index_n_heads;
    unsigned long long index_topk;
    unsigned long long num_nextn_predict_layers;
    unsigned long long o_groups;
    char rms_norm_eps[32];
    unsigned long long rope_theta;
    char routed_scaling_factor[32];
    char swiglu_limit[32];
    int attention_bias;
    int norm_topk_prob;
    int use_cache;
    unsigned long long rope_scaling_factor;
    unsigned long long rope_original_context;
    unsigned long long quant_block_rows;
    unsigned long long quant_block_columns;
    unsigned long long tokenizer_model_max_length;
    unsigned long long generation_bos_token_id;
    unsigned long long generation_eos_token_id;
    int tie_word_embeddings;
    int generation_from_model_config;
    int generation_do_sample;
    unsigned long long source_file_count;
    unsigned long long source_total_bytes;
    unsigned long long shard_count;
    unsigned long long shard_bytes;
    unsigned long long indexed_tensor_count;
    unsigned long long referenced_shard_count;
    unsigned long long header_shard_count;
    unsigned long long header_tensor_count;
    unsigned long long header_bytes;
    unsigned long long declared_tensor_bytes;
    unsigned long long max_tensor_rank;
    unsigned long long dtype_f16_count;
    unsigned long long dtype_bf16_count;
    unsigned long long dtype_f32_count;
    unsigned long long dtype_i64_count;
    unsigned long long dtype_i8_count;
    unsigned long long dtype_fp4_count;
    unsigned long long dtype_f8_count;
    unsigned long long dtype_f8_e8m0_count;
    unsigned long long dtype_other_count;
    const char *blockers[YVEX_SOURCE_VERIFY_BLOCKER_CAP];
    unsigned int blocker_count;
} yvex_source_verification;

int yvex_source_checked_add_u64(unsigned long long *total,
                                unsigned long long value);

int yvex_source_verify(const yvex_source_verify_options *options,
                       yvex_source_verification *out,
                       yvex_error *err);

const char *yvex_source_verification_status(
    const yvex_source_verification *verification);

#endif
