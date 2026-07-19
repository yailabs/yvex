/* Owner: src/cli/render.
 * Owns: normal/table/audit rendering for yvex_source_report.
 * Does not own: source report building, argv parsing, local file scanning, runtime, generation, eval, or benchmark.
 * Invariants: renders typed report facts and uses CLI IO writers only.
 * Boundary: rendering source facts is not source verification, artifact emission, or runtime readiness.
 * Purpose: provide normal/table/audit rendering for yvex_source_report.
 * Inputs: typed domain facts, requested output mode, and caller-owned render state.
 * Effects: formats admitted facts through CLI I/O without changing domain state.
 * Failure: formatting or I/O refusal cannot alter capability facts. */
#include "src/cli/render/private.h"

#include "src/cli/io/private.h"

#include <yvex/internal/source.h>

#include <string.h>

static const yvex_render_field_spec u64_fields_0[] = {
    {"config_hidden_size", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, hidden_size), NULL},
    {"config_num_hidden_layers", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, num_hidden_layers), NULL},
    {"config_num_attention_heads", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        num_attention_heads), NULL},
    {"config_num_key_value_heads", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        num_key_value_heads), NULL},
    {"config_head_dim", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, head_dim), NULL},
    {"config_qk_rope_head_dim", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, qk_rope_head_dim), NULL},
    {"config_max_position_embeddings", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        max_position_embeddings), NULL},
    {"config_moe_intermediate_size", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        moe_intermediate_size), NULL},
    {"config_n_routed_experts", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, n_routed_experts), NULL},
    {"config_n_shared_experts", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, n_shared_experts), NULL},
    {"config_num_experts_per_tok", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        num_experts_per_tok), NULL},
    {"config_num_hash_layers", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, num_hash_layers), NULL},
    {"config_q_lora_rank", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, q_lora_rank), NULL},
    {"config_o_lora_rank", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, o_lora_rank), NULL},
    {"config_o_groups", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, o_groups), NULL},
    {"config_index_head_dim", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, index_head_dim), NULL},
    {"config_index_n_heads", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, index_n_heads), NULL},
    {"config_index_topk", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, index_topk), NULL},
};

static const yvex_render_field_spec u64_fields_1[] = {
    {"config_hc_mult", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, hc_mult), NULL},
    {"config_hc_sinkhorn_iters", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, hc_sinkhorn_iters), NULL},
    {"config_compress_rope_theta", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        compress_rope_theta), NULL},
    {"config_compress_ratio_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        compress_ratio_count), NULL},
};

static const yvex_render_field_spec u64_fields_2[] = {
    {"config_vocab_size", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, vocab_size), NULL},
    {"config_bos_token_id", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, bos_token_id), NULL},
    {"config_eos_token_id", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, eos_token_id), NULL},
};

static const yvex_render_field_spec u64_fields_3[] = {
    {"config_sliding_window", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, sliding_window), NULL},
    {"config_num_nextn_predict_layers", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        num_nextn_predict_layers), NULL},
};

static const yvex_render_field_spec u64_fields_4[] = {
    {"generation_config_bos_token_id", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        generation_bos_token_id), NULL},
    {"generation_config_eos_token_id", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        generation_eos_token_id), NULL},
};

static const yvex_render_field_spec u64_fields_5[] = {
    {"indexed_tensor_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, indexed_tensor_count), NULL},
    {"referenced_shard_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, referenced_shard_count), NULL},
};

static const yvex_render_field_spec u64_fields_6[] = {
    {"source_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, source_file_count), NULL},
    {"source_total_size_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, source_total_bytes), NULL},
    {"source_shard_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, shard_count), NULL},
    {"source_shard_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, shard_bytes), NULL},
    {"header_shard_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, header_shard_count), NULL},
    {"header_tensor_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, header_tensor_count), NULL},
    {"header_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, header_bytes), NULL},
    {"declared_tensor_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, declared_tensor_bytes), NULL},
    {"header_max_tensor_rank", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, max_tensor_rank), NULL},
    {"header_dtype_f16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_f16_count), NULL},
    {"header_dtype_bf16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_bf16_count), NULL},
    {"header_dtype_f32_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_f32_count), NULL},
    {"header_dtype_i64_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_i64_count), NULL},
    {"header_dtype_i8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_i8_count), NULL},
    {"header_dtype_fp4_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_fp4_count), NULL},
    {"header_dtype_f8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_f8_count), NULL},
    {"header_dtype_f8_e8m0_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification,
        dtype_f8_e8m0_count), NULL},
    {"header_dtype_other_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_verification, dtype_other_count), NULL},
};

static const yvex_render_field_spec u64_fields_7[] = {
    {"source_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_file_count), NULL},
    {"source_regular_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_regular_file_count), NULL},
    {"source_safetensors_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, safetensors_count), NULL},
    {"source_bin_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, bin_count), NULL},
    {"source_dat_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, dat_count), NULL},
    {"source_json_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, json_count), NULL},
    {"source_tokenizer_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, tokenizer_file_count), NULL},
    {"source_config_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, config_file_count), NULL},
    {"source_total_size_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, total_size_bytes), NULL},
    {"source_safetensors_size_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        safetensors_size_bytes), NULL},
    {"source_sidecar_size_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, sidecar_size_bytes), NULL},
    {"source_other_size_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, other_size_bytes), NULL},
};

static const yvex_render_field_spec u64_fields_8[] = {
    {"native_safetensors_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_safetensors_count), NULL},
    {"native_safetensors_opened", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_safetensors_opened), NULL},
    {"native_safetensors_header_read_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_safetensors_header_read_count), NULL},
    {"native_safetensors_header_error_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_safetensors_header_error_count), NULL},
    {"native_safetensors_header_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_safetensors_header_bytes), NULL},
};

static const yvex_render_field_spec u64_fields_9[] = {
    {"native_declared_data_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_declared_data_bytes), NULL},
    {"native_declared_tensor_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_declared_tensor_bytes), NULL},
    {"native_max_rank", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_max_rank), NULL},
    {"native_max_tensor_elements", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_max_tensor_elements), NULL},
};

static const yvex_render_field_spec u64_fields_10[] = {
    {"native_largest_tensor_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_largest_tensor_bytes), NULL},
    {"native_dtype_f16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_f16_count), NULL},
    {"native_dtype_bf16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_bf16_count), NULL},
    {"native_dtype_f32_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_f32_count), NULL},
    {"native_dtype_i8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_i8_count), NULL},
    {"native_dtype_i16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_i16_count), NULL},
    {"native_dtype_i32_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_i32_count), NULL},
    {"native_dtype_i64_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_i64_count), NULL},
    {"native_dtype_u8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_u8_count), NULL},
    {"native_dtype_other_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_dtype_other_count), NULL},
    {"native_invalid_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, native_invalid_file_count), NULL},
    {"native_inventory_error_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        native_inventory_error_count), NULL},
};

static const yvex_render_field_spec u64_fields_11[] = {
    {"source_tensor_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_count), NULL},
    {"source_tensor_name_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_name_count), NULL},
    {"source_tensor_file_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_file_count), NULL},
    {"source_tensor_dtype_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_dtype_count), NULL},
    {"source_tensor_rank_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_rank_count), NULL},
    {"source_tensor_shape_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_shape_count), NULL},
    {"source_tensor_declared_data_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_declared_data_bytes), NULL},
    {"source_tensor_declared_tensor_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_declared_tensor_bytes), NULL},
    {"source_tensor_total_elements", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_total_elements), NULL},
    {"source_tensor_max_rank", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report, source_tensor_max_rank), NULL},
    {"source_tensor_max_elements", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_max_elements), NULL},
};

static const yvex_render_field_spec u64_fields_12[] = {
    {"source_tensor_largest_elements", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_largest_elements), NULL},
    {"source_tensor_largest_declared_bytes", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_largest_declared_bytes), NULL},
    {"source_tensor_dtype_f16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_f16_count), NULL},
    {"source_tensor_dtype_bf16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_bf16_count), NULL},
    {"source_tensor_dtype_f32_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_f32_count), NULL},
    {"source_tensor_dtype_i8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_i8_count), NULL},
    {"source_tensor_dtype_i16_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_i16_count), NULL},
    {"source_tensor_dtype_i32_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_i32_count), NULL},
    {"source_tensor_dtype_i64_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_i64_count), NULL},
    {"source_tensor_dtype_u8_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_u8_count), NULL},
    {"source_tensor_dtype_other_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_dtype_other_count), NULL},
    {"source_tensor_rank_0_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_0_count), NULL},
    {"source_tensor_rank_1_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_1_count), NULL},
    {"source_tensor_rank_2_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_2_count), NULL},
    {"source_tensor_rank_3_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_3_count), NULL},
    {"source_tensor_rank_4_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_4_count), NULL},
    {"source_tensor_rank_other_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_rank_other_count), NULL},
};

static const yvex_render_field_spec u64_fields_13[] = {
    {"source_tensor_name_embed_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_embed_count), NULL},
    {"source_tensor_name_attn_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_attn_count), NULL},
    {"source_tensor_name_mlp_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_mlp_count), NULL},
    {"source_tensor_name_norm_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_norm_count), NULL},
    {"source_tensor_name_lm_head_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_lm_head_count), NULL},
    {"source_tensor_name_other_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_name_other_count), NULL},
    {"source_tensor_metadata_error_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_metadata_error_count), NULL},
    {"source_tensor_sample_count", YVEX_RENDER_FIELD_U64, offsetof(yvex_source_report,
        source_tensor_sample_count), NULL},
};

#define VERIFY_FIELD(key_, kind_, member_, fallback_) \
    {key_, kind_, offsetof(yvex_source_verification, member_), fallback_}

static const yvex_render_field_spec config_source_fields[] = {
    VERIFY_FIELD("source_kind", YVEX_RENDER_FIELD_TEXT_ARRAY, source_kind, "missing")
};

static const yvex_render_field_spec config_verify_fields[] = {
    VERIFY_FIELD("repository_verified", YVEX_RENDER_FIELD_BOOL, repository_verified, NULL),
    VERIFY_FIELD("revision_verified", YVEX_RENDER_FIELD_BOOL, revision_verified, NULL)
};

static const yvex_render_field_spec config_type_fields[] = {
    VERIFY_FIELD("config_model_type", YVEX_RENDER_FIELD_TEXT_ARRAY, model_type, "missing"),
    VERIFY_FIELD("config_architecture", YVEX_RENDER_FIELD_TEXT_ARRAY, architecture, "missing")
};

static const yvex_render_field_spec config_hc_fields[] = {
    VERIFY_FIELD("config_hc_eps", YVEX_RENDER_FIELD_TEXT_ARRAY, hc_eps, "missing")
};

static const yvex_render_field_spec config_behavior_fields[] = {
    VERIFY_FIELD("config_tie_word_embeddings", YVEX_RENDER_FIELD_BOOL, tie_word_embeddings, NULL),
    VERIFY_FIELD("config_attention_bias", YVEX_RENDER_FIELD_BOOL, attention_bias, NULL),
    VERIFY_FIELD("config_attention_dropout", YVEX_RENDER_FIELD_TEXT_ARRAY, attention_dropout, "missing")
};

static const yvex_render_field_spec config_numeric_fields[] = {
    VERIFY_FIELD("config_rms_norm_eps", YVEX_RENDER_FIELD_TEXT_ARRAY, rms_norm_eps, "missing"),
    VERIFY_FIELD("config_rope_theta", YVEX_RENDER_FIELD_U64, rope_theta, NULL),
    VERIFY_FIELD("config_hidden_act", YVEX_RENDER_FIELD_TEXT_ARRAY, hidden_act, "missing"),
    VERIFY_FIELD("config_torch_dtype", YVEX_RENDER_FIELD_TEXT_ARRAY, torch_dtype, "missing"),
    VERIFY_FIELD("config_expert_dtype", YVEX_RENDER_FIELD_TEXT_ARRAY, expert_dtype, "missing"),
    VERIFY_FIELD("config_routed_scaling_factor", YVEX_RENDER_FIELD_TEXT_ARRAY,
                 routed_scaling_factor, "missing"),
    VERIFY_FIELD("config_scoring_func", YVEX_RENDER_FIELD_TEXT_ARRAY, scoring_func, "missing"),
    VERIFY_FIELD("config_topk_method", YVEX_RENDER_FIELD_TEXT_ARRAY, topk_method, "missing"),
    VERIFY_FIELD("config_norm_topk_prob", YVEX_RENDER_FIELD_BOOL, norm_topk_prob, NULL),
    VERIFY_FIELD("config_swiglu_limit", YVEX_RENDER_FIELD_TEXT_ARRAY, swiglu_limit, "missing"),
    VERIFY_FIELD("config_use_cache", YVEX_RENDER_FIELD_BOOL, use_cache, NULL)
};

static const yvex_render_field_spec tokenizer_identity_fields[] = {
    VERIFY_FIELD("tokenizer_class", YVEX_RENDER_FIELD_TEXT_ARRAY, tokenizer_class, "missing"),
    VERIFY_FIELD("tokenizer_model_type", YVEX_RENDER_FIELD_TEXT_ARRAY, tokenizer_model_type, "missing"),
    VERIFY_FIELD("tokenizer_model_max_length", YVEX_RENDER_FIELD_U64, tokenizer_model_max_length, NULL)
};

static const yvex_render_field_spec generation_origin_fields[] = {
    VERIFY_FIELD("generation_config_from_model", YVEX_RENDER_FIELD_BOOL,
                 generation_from_model_config, NULL)
};

static const yvex_render_field_spec generation_policy_fields[] = {
    VERIFY_FIELD("generation_config_do_sample", YVEX_RENDER_FIELD_BOOL, generation_do_sample, NULL),
    VERIFY_FIELD("generation_config_temperature", YVEX_RENDER_FIELD_TEXT_ARRAY,
                 generation_temperature, "missing"),
    VERIFY_FIELD("generation_config_top_p", YVEX_RENDER_FIELD_TEXT_ARRAY, generation_top_p, "missing"),
    VERIFY_FIELD("generation_config_transformers_version", YVEX_RENDER_FIELD_TEXT_ARRAY,
                 generation_transformers_version, "missing")
};

static const yvex_render_field_spec inventory_identity_fields[] = {
    VERIFY_FIELD("header_index_match", YVEX_RENDER_FIELD_BOOL, shard_index_headers_match, NULL),
    VERIFY_FIELD("upstream_index_oid", YVEX_RENDER_FIELD_TEXT_ARRAY, upstream_index_oid, "not-applicable"),
    VERIFY_FIELD("local_index_oid", YVEX_RENDER_FIELD_TEXT_ARRAY, local_index_oid, "not-applicable"),
    VERIFY_FIELD("upstream_index_identity_verified", YVEX_RENDER_FIELD_BOOL,
                 upstream_index_identity_verified, NULL),
    VERIFY_FIELD("header_scan_count", YVEX_RENDER_FIELD_U64, header_scan_count, NULL)
};

static const yvex_render_field_spec manifest_identity_fields[] = {
    VERIFY_FIELD("manifest_schema", YVEX_RENDER_FIELD_TEXT_ARRAY, manifest_schema, "missing"),
    VERIFY_FIELD("manifest_verification_stage", YVEX_RENDER_FIELD_TEXT_ARRAY,
                 verification_stage, "missing"),
    VERIFY_FIELD("manifest_verified", YVEX_RENDER_FIELD_BOOL, manifest_verified, NULL),
    VERIFY_FIELD("manifest_published", YVEX_RENDER_FIELD_BOOL, manifest_published, NULL),
    VERIFY_FIELD("manifest_reopened", YVEX_RENDER_FIELD_BOOL, manifest_reopened, NULL),
    VERIFY_FIELD("source_manifest_status", YVEX_RENDER_FIELD_TEXT_ARRAY, manifest_status, "missing"),
    VERIFY_FIELD("source_manifest_path", YVEX_RENDER_FIELD_TEXT_ARRAY, manifest_path, "missing"),
    VERIFY_FIELD("source_manifest_revision", YVEX_RENDER_FIELD_TEXT_ARRAY, manifest_revision, "missing"),
    VERIFY_FIELD("source_revision", YVEX_RENDER_FIELD_TEXT_ARRAY, revision, "missing")
};

#undef VERIFY_FIELD

static const char *const literal_pair_0[] = { "source_tensor_metadata_payload_loaded: false",
    "source_tensor_metadata_payload_bytes_read: 0"};

static const char *const literal_pair_1[] = { "native_safetensors_payload_loaded: false",
    "native_safetensors_payload_bytes_read: 0"};

static const char *const literal_pair_2[] = { "source_digest_status: not-computed",
    "source_hash_status: not-computed"};

static const char *const literal_pair_3[] = { "source_tag: unknown",
    "source_tag_status: unknown"};

static const char *const literal_pair_4[] = { "source_count_scope: top-level-regular-files",
    "source_payload_loaded: false"};

static const char *const literal_lines_0[] = { "source_payload_loaded: false",
    "release_target_selected: true",
    "release_qtype: unselected",
    "artifact_status: not-produced",
    "runtime_claim: unsupported",
    "generation: unsupported-full-model",
    "benchmark_status: not-measured"};

static const char *const literal_lines_1[] = { "source_manifest_payload_loaded: false",
    "source_manifest_remote_checked: false",
    "source_manifest_hash_computed: false"};

static const char *const literal_lines_2[] = { "artifact_status: missing",
    "runtime_claim: unsupported",
    "generation: unsupported-full-model",
    "benchmark_status: not-measured",
    "release_ready: false"};

static const char *const literal_lines_3[] = { "\nOptions:",
    "  --family deepseek|qwen|gemma",
    "  --release v0.1.0",
    "  --models-root DIR",
    "  --source DIR",
    "  --target deepseek4-v4-flash|qwen3-8b|qwen-small|qwen-medium|gemma-4-12b-it"};

static const char *const literal_lines_4[] = {
    "Report fields include source artifact class, target artifact class, source footprint, and source "
        "provenance evidence.",
    "Source footprint uses checked byte accounting without loading tensor payloads.",
    "DeepSeek strict verification checks structured repository, revision, model/tokenizer/generation "
        "config, index, shard, dtype, and header facts. Qwen and Gemma retain their bounded report behavior.",
    "Native safetensors inventory reads safetensors headers only and never loads tensor payload bytes.",
    "Source tensor metadata inventory is derived from safetensors headers only and does not map tensors to "
        "runtime roles.",
    "Strict DeepSeek verification may atomically promote and reopen the canonical YVEX manifest after all "
        "metadata/header checks pass; it never writes inside the official source tree.",
    "Verification does not hash weight payloads or load tensor payload bytes.",
    "The source pressure report inspects source-path readiness only. It does not download weights, emit "
        "artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a "
        "release ready."
};

/* Purpose: Render source render tensor print limit from typed facts (`source_render_tensor_print_limit`). */
static unsigned long long source_render_tensor_print_limit(
    const yvex_source_report_request *options,
    const yvex_source_report *report)
{
    unsigned long long limit = options && options->tensor_limit
                                   ? options->tensor_limit
                                   : 20ull;

    if (!report) {
        return 0;
    }
    if (limit > YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        limit = YVEX_SOURCE_TENSOR_SAMPLE_CAP;
    }
    if (limit > report->source_tensor_sample_count) {
        limit = report->source_tensor_sample_count;
    }
    return limit;
}

/* Purpose: Render render tensor rows from typed facts (`render_tensor_rows`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void render_tensor_rows(
    FILE *fp,
    const yvex_source_report_request *options,
    const yvex_source_report *report)
{
    unsigned long long limit;
    unsigned long long display_limit;
    unsigned long long i;

    if (!options || !report || !options->include_tensors) {
        return;
    }
    limit = source_render_tensor_print_limit(options, report);
    display_limit = options->tensor_limit ? options->tensor_limit : 20ull;
    if (display_limit > YVEX_SOURCE_TENSOR_SAMPLE_CAP) {
        display_limit = YVEX_SOURCE_TENSOR_SAMPLE_CAP;
    }
    yvex_cli_out_writef(fp, "\nTENSORS  limit=%llu\n\n", display_limit);
    yvex_cli_out_writef(fp, "%-32s  %-32s  %-6s  %4s  %-18s  %8s  %8s\n",
           "NAME", "FILE", "DTYPE", "RANK", "SHAPE", "ELEMENTS", "BYTES");
    for (i = 0; i < limit; ++i) {
        const yvex_source_tensor_sample *sample = &report->source_tensor_samples[i];
        yvex_cli_out_writef(fp, "%-32s  %-32s  %-6s  %4llu  %-18s  %8llu  %8llu\n",
               sample->name,
               sample->file,
               sample->dtype,
               sample->rank,
               sample->shape,
               sample->elements,
               sample->declared_bytes);
    }
}

/* Purpose: Render source render model name from typed facts (`source_render_model_name`). */
static const char *source_render_model_name(const yvex_source_report_request *options,
                                          const yvex_source_report *report)
{
    (void)options;
    return report && report->semantics.model_name
               ? report->semantics.model_name : "unknown";
}

/* Purpose: Render source render present missing from typed facts (`source_render_present_missing`). */
static const char *source_render_present_missing(int present)
{
    return present ? "present" : "missing";
}

/* Purpose: Render source render tokenizer status from typed facts (`source_render_tokenizer_status`). */
static const char *source_render_tokenizer_status(const yvex_source_report *report)
{
    return report->semantics.tokenizer_status;
}

/* Purpose: Render source render safetensors status from typed facts (`source_render_safetensors_status`). */
static const char *source_render_safetensors_status(const yvex_source_report *report)
{
    return report->semantics.safetensors_status;
}

/* Purpose: Render source render manifest status from typed facts (`source_render_manifest_status`). */
static const char *source_render_manifest_status(const yvex_source_report *report)
{
    return report->semantics.manifest_status;
}

/* Purpose: Render source render native inventory report status from typed facts
 *   (`source_render_native_inventory_report_status`). */
static const char *source_render_native_inventory_report_status(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_report_status;
}

/* Purpose: Render source render tensor map report status from typed facts
 *   (`source_render_tensor_map_report_status`). */
static const char *source_render_tensor_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_map_report_status;
}

/* Purpose: Render source render tensor role map report status from typed facts
 *   (`source_render_tensor_role_map_report_status`). */
static const char *source_render_tensor_role_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_role_map_report_status;
}

/* Purpose: Render source render output head map report status from typed facts
 *   (`source_render_output_head_map_report_status`). */
static const char *source_render_output_head_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.output_head_map_report_status;
}

/* Purpose: Render source render tokenizer map report status from typed facts
 *   (`source_render_tokenizer_map_report_status`). */
static const char *source_render_tokenizer_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tokenizer_map_report_status;
}

/* Purpose: Render source render native inventory status from typed facts (`source_render_native_inventory_status`). */
static const char *source_render_native_inventory_status(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_status;
}

/* Purpose: Render source render native inventory source from typed facts (`source_render_native_inventory_source`). */
static const char *source_render_native_inventory_source(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_source;
}

/* Purpose: Render source render tensor metadata status from typed facts (`source_render_tensor_metadata_status`). */
static const char *source_render_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_metadata_status;
}

/* Purpose: Render source render tensor metadata source from typed facts (`source_render_tensor_metadata_source`). */
static const char *source_render_tensor_metadata_source(
    const yvex_source_report *report)
{
    return report->semantics.tensor_metadata_source;
}

/* Purpose: Render source render native tensor metadata status from typed facts
 *   (`source_render_native_tensor_metadata_status`). */
static const char *source_render_native_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.native_tensor_metadata_status;
}

/* Purpose: Render source render native tensor payload status from typed facts
 * (`source_render_native_tensor_payload_status`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static const char *source_render_native_tensor_payload_status(
    const yvex_source_report *report)
{
    return report->semantics.native_tensor_payload_status;
}

/* Purpose: Render source render sidecar status from typed facts (`source_render_sidecar_status`). */
static const char *source_render_sidecar_status(const yvex_source_report *report)
{
    return report->semantics.sidecar_status;
}

/* Purpose: Render source render tensor payload status from typed facts (`source_render_tensor_payload_status`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static const char *source_render_tensor_payload_status(const yvex_source_report *report)
{
    return report->semantics.tensor_payload_status;
}

/* Purpose: Render source render target artifact status from typed facts (`source_render_target_artifact_status`). */
static const char *source_render_target_artifact_status(const yvex_source_family_profile *profile)
{
    return profile && profile->yvex_produced_artifact_status
               ? profile->yvex_produced_artifact_status : "planned";
}

/* Purpose: Render source render footprint class from typed facts (`source_render_footprint_class`). */
static const char *source_render_footprint_class(const yvex_source_report *report)
{
    return report->semantics.footprint_class;
}

/* Purpose: Render source render footprint status from typed facts (`source_render_footprint_status`). */
static const char *source_render_footprint_status(const yvex_source_report *report)
{
    return report->semantics.footprint_status;
}

/* Purpose: Render source render provenance origin normal from typed facts
 *   (`source_render_provenance_origin_normal`). */
static const char *source_render_provenance_origin_normal(
    const yvex_source_report *report)
{
    return report->semantics.provenance_origin_normal;
}

/* Purpose: Render source render provenance origin audit from typed facts (`source_render_provenance_origin_audit`). */
static const char *source_render_provenance_origin_audit(
    const yvex_source_report_request *options,
    const yvex_source_report *report)
{
    (void)options;
    return report->semantics.provenance_origin_audit;
}

/* Purpose: Render source render provenance status from typed facts (`source_render_provenance_status`). */
static const char *source_render_provenance_status(
    const yvex_source_report *report)
{
    return report->semantics.provenance_status;
}

/* Purpose: Render source render identity status from typed facts (`source_render_identity_status`). */
static const char *source_render_identity_status(
    const yvex_source_report *report)
{
    return report->semantics.identity_status;
}

/* Purpose: Render source render authority from typed facts (`source_render_authority`). */
static const char *source_render_authority(const yvex_source_report *report)
{
    return report->semantics.authority;
}

/* Purpose: Render source render authority status from typed facts (`source_render_authority_status`). */
static const char *source_render_authority_status(const yvex_source_report *report)
{
    return report->semantics.authority_status;
}

/* Purpose: Render source render manifest provenance status from typed facts
 *   (`source_render_manifest_provenance_status`). */
static const char *source_render_manifest_provenance_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_provenance_status;
}

/* Purpose: Render source render manifest authority from typed facts (`source_render_manifest_authority`). */
static const char *source_render_manifest_authority(
    const yvex_source_report *report)
{
    return report->semantics.manifest_authority;
}

/* Purpose: Render source render manifest schema status from typed facts (`source_render_manifest_schema_status`). */
static const char *source_render_manifest_schema_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_schema_status;
}

/* Purpose: Render source render manifest family status from typed facts (`source_render_manifest_family_status`). */
static const char *source_render_manifest_family_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_family_status;
}

/* Purpose: Render source render manifest target status from typed facts (`source_render_manifest_target_status`). */
static const char *source_render_manifest_target_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_target_status;
}

/* Purpose: Render source render manifest artifact class status from typed facts
 *   (`source_render_manifest_artifact_class_status`). */
static const char *source_render_manifest_artifact_class_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_artifact_class_status;
}

/* Purpose: Render source render manifest footprint status from typed facts
 *   (`source_render_manifest_footprint_status`). */
static const char *source_render_manifest_footprint_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_footprint_status;
}

/* Purpose: Render source render manifest native inventory status from typed facts
 *   (`source_render_manifest_native_inventory_status`). */
static const char *source_render_manifest_native_inventory_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_native_inventory_status;
}

/* Purpose: Render source render manifest tensor metadata status from typed facts
 *   (`source_render_manifest_tensor_metadata_status`). */
static const char *source_render_manifest_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_tensor_metadata_status;
}

/* Purpose: Render source render manifest consistency status from typed facts
 *   (`source_render_manifest_consistency_status`). */
static const char *source_render_manifest_consistency_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_consistency_status;
}

/* Purpose: Render source render presence verification status from typed facts
 *   (`source_render_presence_verification_status`). */
static const char *source_render_presence_verification_status(int present)
{
    return present ? "present-unverified" : "not-present";
}

/* Purpose: Render source render normal from typed facts (`yvex_source_render_normal`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_source_render_normal(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = report ? &report->request : NULL;
    int deepseek = options &&
                   strcmp(options->profile->family_key, "deepseek") == 0;
    yvex_cli_out_writef(fp, "report: %s\n", options->profile->report_name);
    yvex_cli_out_writef(fp, "status: %s\n", report->status);
    yvex_cli_out_writef(fp, "family: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "target: %s\n", options->target);
    yvex_cli_out_writef(fp, "source: %s  status=%s\n",
           options->profile->source_artifact_class,
           report->source_state);
    yvex_cli_out_writef(fp, "artifact: %s  status=%s\n",
           options->profile->target_artifact_class,
           source_render_target_artifact_status(options->profile));
    yvex_cli_out_writef(fp, "files: %llu  safetensors: %llu  bytes: %llu  footprint: %s\n",
           report->source_file_count,
           report->safetensors_count,
           report->total_size_bytes,
           source_render_footprint_class(report));
    yvex_cli_out_writef(fp, "provenance: %s status=%s revision=%s\n",
           source_render_provenance_origin_normal(report),
           deepseek ? report->semantics.verification_status
                    : source_render_provenance_status(report),
           report->identity_revision[0] ? report->identity_revision : "unknown");
    yvex_cli_out_writef(fp, "native: %s  files=%llu  tensors=%llu  header_bytes=%llu\n",
           source_render_native_inventory_status(report),
           report->native_safetensors_count,
           report->native_tensor_count,
           report->native_safetensors_header_bytes);
    yvex_cli_out_writef(fp, "metadata: %s  tensors=%llu  dtypes=%llu  max_rank=%llu\n",
           source_render_tensor_metadata_status(report),
           report->source_tensor_count,
           report->source_tensor_dtype_count,
           report->source_tensor_max_rank);
    yvex_cli_out_writef(fp, "manifest: %s  consistency=%s\n",
           source_render_manifest_status(report),
           source_render_manifest_consistency_status(report));
    if (deepseek) {
        yvex_cli_out_writef(fp, "repository: %s  status=%s\n",
                           report->identity_repo_id[0]
                               ? report->identity_repo_id
                               : "unknown",
                           report->semantics.repository_status);
        yvex_cli_out_writef(fp,
                           "verification: %s  config=%s  tokenizer=%s  generation_config=%s  index=%s  "
                               "headers=%llu/%llu\n",
                           report->semantics.verification_status,
                           report->semantics.config_identity_status,
                           report->semantics.tokenizer_verification_status,
                           report->semantics.generation_config_status,
                           report->semantics.shard_index_status,
                           report->verification.header_shard_count,
                           report->verification.shard_count);
        yvex_cli_out_writef(
            fp,
            "inventory: %s  upstream_identity=%s  tensors=%llu  source_bytes=%llu  header_scans=%llu\n",
            report->semantics.inventory_authority,
            report->semantics.upstream_index_identity_status,
            report->verification.header_tensor_count,
            report->verification.source_total_bytes,
            report->verification.header_scan_count);
    }
    yvex_cli_out_writef(fp, "top_blocker: %s\n", report->top_blocker);
    yvex_cli_out_writef(fp, "next: %s\n", report->next_row);
    yvex_cli_out_writef(fp, "boundary: source report only; no artifact/runtime/generation/benchmark\n");
    render_tensor_rows(fp, options, report);
    return report->exit_code;
}

/* Purpose: Render source render table from typed facts (`yvex_source_render_table`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_source_render_table(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = report ? &report->request : NULL;
    int deepseek = options &&
                   strcmp(options->profile->family_key, "deepseek") == 0;

    if (deepseek) {
        yvex_cli_out_writef(fp, "SOURCE VERIFY  release=%s\n\n", options->release);
        yvex_cli_out_writef(fp,
                           "%-8s  %-24s  %-8s  %-14s  %7s  %6s  %13s  %-40s  %s\n",
                           "FAMILY", "TARGET", "VERIFY", "INVENTORY",
                           "TENSORS", "SHARDS", "SOURCE_BYTES", "REVISION",
                           "NEXT");
        yvex_cli_out_writef(fp,
                           "%-8s  %-24s  %-8s  %-14s  %7llu  %6llu  %13llu  %-40s  %s\n",
                           options->profile->family_key, options->target,
                           report->semantics.verification_status,
                           report->semantics.inventory_authority,
                           report->verification.header_tensor_count,
                           report->verification.shard_count,
                           report->verification.source_total_bytes,
                           report->identity_revision[0]
                               ? report->identity_revision
                               : "missing",
                           report->next_row);
        yvex_cli_out_writef(fp, "top_blocker: %s\n", report->top_blocker);
        return report->exit_code;
    }
    yvex_cli_out_writef(fp, "SOURCE PRESSURE  release=%s\n\n", options->release);
    yvex_cli_out_writef(fp, "%-6s  %-24s  %-7s  %7s  %-8s  %-11s  %s\n",
           "FAMILY", "TARGET", "SOURCE", "TENSORS", "MANIFEST",
           "CONSISTENCY", "NEXT");
    yvex_cli_out_writef(fp, "%-6s  %-24s  %-7s  %7llu  %-8s  %-11s  %s\n",
           options->profile->family_key,
           options->target,
           report->source_state,
           report->source_tensor_count,
           source_render_manifest_status(report),
           source_render_manifest_consistency_status(report),
           report->next_row);
    render_tensor_rows(fp, options, report);
    return report->exit_code;
}

/* Render immutable DeepSeek configuration and source-format facts. */
/* Purpose: Render source render deepseek config from typed facts (`source_render_deepseek_config`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void source_render_deepseek_config(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_verification *verification = &report->verification;
    unsigned long i;

    yvex_cli_out_writef(fp, "canonical_repository: %s\n",
                       yvex_source_release_identity()->upstream_repo_id);
    render_object_fields(fp, verification, config_source_fields,
                         sizeof(config_source_fields) / sizeof(config_source_fields[0]));
    yvex_cli_out_writef(fp, "resolved_source_path: %s\n",
                       verification->resolved_source_path[0]
                           ? verification->resolved_source_path
                           : report->source_path);
    yvex_cli_out_writef(fp, "source_verification_status: %s\n",
                       report->semantics.verification_status);
    render_object_fields(fp, verification, config_verify_fields,
                         sizeof(config_verify_fields) / sizeof(config_verify_fields[0]));
    yvex_cli_out_writef(fp, "config_identity_status: %s\n",
                       report->semantics.config_identity_status);
    render_object_fields(fp, verification, config_type_fields,
                         sizeof(config_type_fields) / sizeof(config_type_fields[0]));
    render_object_fields(fp, verification, u64_fields_0, sizeof(u64_fields_0) / sizeof(u64_fields_0[0]));
    render_object_fields(fp, verification, config_hc_fields,
                         sizeof(config_hc_fields) / sizeof(config_hc_fields[0]));
    render_object_fields(fp, verification, u64_fields_1, sizeof(u64_fields_1) / sizeof(u64_fields_1[0]));
    yvex_cli_out_writef(fp, "config_compress_ratios: [");
    for (i = 0; i < verification->compress_ratio_count; ++i) {
        yvex_cli_out_writef(fp, "%s%llu", i ? "," : "",
                           verification->compress_ratios[i]);
    }
    yvex_cli_out_writef(fp, "]\n");
    render_object_fields(fp, verification, u64_fields_2, sizeof(u64_fields_2) / sizeof(u64_fields_2[0]));
    render_object_fields(fp, verification, config_behavior_fields,
                         sizeof(config_behavior_fields) / sizeof(config_behavior_fields[0]));
    render_object_fields(fp, verification, u64_fields_3, sizeof(u64_fields_3) / sizeof(u64_fields_3[0]));
    render_object_fields(fp, verification, config_numeric_fields,
                         sizeof(config_numeric_fields) / sizeof(config_numeric_fields[0]));
    yvex_cli_out_writef(fp, "rope_scaling: %s factor=%llu original=%llu\n",
                       verification->rope_scaling_type[0]
                           ? verification->rope_scaling_type
                           : "missing",
                       verification->rope_scaling_factor,
                       verification->rope_original_context);
    yvex_cli_out_writef(fp, "source_quantization: %s/%s block=%llux%llu\n",
                       verification->quant_method[0]
                           ? verification->quant_method
                           : "missing",
                       verification->quant_format[0]
                           ? verification->quant_format
                           : "missing",
                       verification->quant_block_rows,
                       verification->quant_block_columns);

}

/* Render retained DeepSeek tokenizer, manifest, and inventory admission facts. */
/* Purpose: Render source render deepseek inventory from typed facts (`source_render_deepseek_inventory`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void source_render_deepseek_inventory(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_verification *verification = &report->verification;
    unsigned long i;

    yvex_cli_out_writef(fp, "tokenizer_status: %s\n",
                       report->semantics.tokenizer_verification_status);
    render_object_fields(fp, verification, tokenizer_identity_fields,
                         sizeof(tokenizer_identity_fields) / sizeof(tokenizer_identity_fields[0]));
    yvex_cli_out_writef(fp, "generation_config_status: %s\n",
                       report->semantics.generation_config_status);
    render_object_fields(fp, verification, generation_origin_fields,
                         sizeof(generation_origin_fields) / sizeof(generation_origin_fields[0]));
    render_object_fields(fp, verification, u64_fields_4, sizeof(u64_fields_4) / sizeof(u64_fields_4[0]));
    render_object_fields(fp, verification, generation_policy_fields,
                         sizeof(generation_policy_fields) / sizeof(generation_policy_fields[0]));
    yvex_cli_out_writef(fp, "shard_index_status: %s\n",
                       report->semantics.shard_index_status);
    render_object_fields(fp, verification, u64_fields_5, sizeof(u64_fields_5) / sizeof(u64_fields_5[0]));
    render_object_fields(fp, verification, inventory_identity_fields, 1u);
    yvex_cli_out_writef(fp, "inventory_authority: %s\n",
                       report->semantics.inventory_authority);
    render_object_fields(fp, verification, inventory_identity_fields + 1,
                         sizeof(inventory_identity_fields) /
                             sizeof(inventory_identity_fields[0]) - 1u);
    render_object_fields(fp, verification, manifest_identity_fields,
                         sizeof(manifest_identity_fields) / sizeof(manifest_identity_fields[0]));
    render_object_fields(fp, verification, u64_fields_6, sizeof(u64_fields_6) / sizeof(u64_fields_6[0]));
    yvex_cli_out_lines(fp, literal_lines_0, sizeof(literal_lines_0) / sizeof(literal_lines_0[0]));
    yvex_cli_out_writef(fp, "source_verification_blocker_count: %u\n",
                       verification->blocker_count);
    yvex_cli_out_writef(fp, "blocker_count: %lu\n",
                       report->blocker_count);
    for (i = 0; i < report->blocker_count; ++i) {
        yvex_cli_out_writef(fp, "blocker_%lu: %s\n", i,
                           report->blockers[i]);
    }
    yvex_cli_out_writef(fp, "next_required_rows: %s\n",
                       report->next_row);
    yvex_cli_out_writef(fp,
                       "boundary: exact source metadata/header verification only; no payload/artifact/"
                           "runtime/generation/benchmark\n");

}

/* Render generic source provenance, filesystem, and manifest facts. */
/* Purpose: Render source render generic source from typed facts (`source_render_generic_source`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void source_render_generic_source(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = &report->request;

    yvex_cli_out_writef(fp, "target_class: %s\n", options->profile->target_class);
    yvex_cli_out_writef(fp, "source_target_status: %s\n", options->profile->source_target_status);
    yvex_cli_out_writef(fp, "source_family_profile_status: %s\n",
       options->profile->source_family_profile_status);
    yvex_cli_out_writef(fp, "source_artifact_class: %s\n", options->profile->source_artifact_class);
    yvex_cli_out_writef(fp, "source_artifact_status: %s\n", report->source_state);
    yvex_cli_out_writef(fp, "source_artifact_format: %s\n", options->profile->source_artifact_format);
    yvex_cli_out_writef(fp, "source_artifact_origin: %s\n", options->profile->source_artifact_origin);
    yvex_cli_out_writef(fp, "source_artifact_authority: %s\n",
       options->profile->source_artifact_authority);
    yvex_cli_out_writef(fp, "source_sidecar_status: %s\n", source_render_sidecar_status(report));
    yvex_cli_out_writef(fp, "source_tensor_container: %s\n", options->profile->source_tensor_container);
    yvex_cli_out_writef(fp, "source_tensor_payload_status: %s\n",
       source_render_tensor_payload_status(report));
    yvex_cli_out_writef(fp, "target_artifact_class: %s\n", options->profile->target_artifact_class);
    yvex_cli_out_writef(fp, "target_artifact_status: %s\n",
       source_render_target_artifact_status(options->profile));
    yvex_cli_out_writef(fp, "target_artifact_origin: %s\n", options->profile->target_artifact_origin);
    yvex_cli_out_writef(fp, "target_artifact_required: %s\n",
       options->profile->target_artifact_required);
    yvex_cli_out_writef(fp, "external_reference_status: %s\n",
       options->profile->external_reference_status);
    yvex_cli_out_writef(fp, "yvex_produced_artifact_status: %s\n",
       options->profile->yvex_produced_artifact_status);
    yvex_cli_out_writef(fp, "pressure_purpose: %s\n", options->profile->pressure_purpose);
    yvex_cli_out_writef(fp, "runtime_shape: %s\n", options->profile->runtime_shape);
    yvex_cli_out_writef(fp, "hardware_lane: %s\n", options->profile->hardware_lane);
    yvex_cli_out_writef(fp, "backend_lane: %s\n", options->profile->backend_lane);
    yvex_cli_out_writef(fp, "source_class: %s\n", options->profile->source_class);
    yvex_cli_out_writef(fp, "source_provenance_status: %s\n", source_render_provenance_status(report));
    yvex_cli_out_writef(fp, "source_origin: %s\n", source_render_provenance_origin_audit(options, report));
    yvex_cli_out_writef(fp, "source_authority: %s\n", source_render_authority(report));
    yvex_cli_out_writef(fp, "source_authority_status: %s\n", source_render_authority_status(report));
    yvex_cli_out_writef(fp, "source_path: %s\n", report->source_path);
    yvex_cli_out_writef(fp, "source_path_source: %s\n", report->source_path_source);
    yvex_cli_out_writef(fp, "source_path_status: %s\n", report->source_state);
    yvex_cli_out_writef(fp, "source_exists: %s\n", report->source_exists ? "true" : "false");
    yvex_cli_out_writef(fp, "download_registry_path: %s\n",
       report->download_registry_path[0] ? report->download_registry_path : "unknown");
    yvex_cli_out_writef(fp, "download_registry_status: %s\n",
       report->download_registry_exists ? "present" : "missing");
    yvex_cli_out_writef(fp, "download_report_path: %s\n",
       report->download_report_path[0] ? report->download_report_path : "unknown");
    yvex_cli_out_writef(fp, "download_report_status: %s\n",
       report->download_report_exists ? "present" : "missing");
    yvex_cli_out_writef(fp, "download_repo_id: %s\n",
       report->identity_repo_id[0] ? report->identity_repo_id : "unknown");
    yvex_cli_out_writef(fp, "download_revision: %s\n",
       report->identity_revision[0] ? report->identity_revision : "unknown");
    render_object_fields(fp, report, u64_fields_7, sizeof(u64_fields_7) / sizeof(u64_fields_7[0]));
    yvex_cli_out_writef(fp, "source_footprint_class: %s\n", source_render_footprint_class(report));
    yvex_cli_out_writef(fp, "source_footprint_status: %s\n", source_render_footprint_status(report));
    yvex_cli_out_lines(fp, literal_pair_4, sizeof(literal_pair_4) / sizeof(literal_pair_4[0]));
    yvex_cli_out_writef(fp, "largest_source_file_bytes: %llu\n", report->largest_source_file_bytes);
    yvex_cli_out_writef(fp, "largest_source_file_name: %s\n",
       report->largest_source_file_name[0]
           ? report->largest_source_file_name
           : "none");
    yvex_cli_out_writef(fp, "config_status: %s\n", source_render_present_missing(report->config_exists));
    yvex_cli_out_writef(fp, "tokenizer_status: %s\n", source_render_tokenizer_status(report));
    yvex_cli_out_writef(fp, "generation_config_status: %s\n",
       source_render_present_missing(report->generation_config_exists));
    yvex_cli_out_writef(fp, "safetensors_status: %s\n", source_render_safetensors_status(report));
    yvex_cli_out_writef(fp, "safetensors_count: %llu\n", report->safetensors_count);
    yvex_cli_out_writef(fp, "source_manifest_expected: true\n");
    yvex_cli_out_writef(fp, "source_manifest_status: %s\n", source_render_manifest_status(report));
    yvex_cli_out_writef(fp, "source_manifest_path: %s\n",
       report->manifest_path[0] ? report->manifest_path : "unknown");
    yvex_cli_out_writef(fp, "source_manifest_schema_status: %s\n",
       source_render_manifest_schema_status(report));
    yvex_cli_out_writef(fp, "source_manifest_schema_version: %s\n",
       report->manifest_schema_version[0]
           ? report->manifest_schema_version
           : "unknown");
    yvex_cli_out_writef(fp, "source_manifest_family: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "source_manifest_family_status: %s\n",
       source_render_manifest_family_status(report));
    yvex_cli_out_writef(fp, "source_manifest_target_id: %s\n", options->target);
    yvex_cli_out_writef(fp, "source_manifest_target_status: %s\n",
       source_render_manifest_target_status(report));
    yvex_cli_out_writef(fp, "source_manifest_source_path_status: %s\n", report->source_state);
    yvex_cli_out_writef(fp, "source_manifest_artifact_class_status: %s\n",
       source_render_manifest_artifact_class_status(report));
    yvex_cli_out_writef(fp, "source_manifest_footprint_status: %s\n",
       source_render_manifest_footprint_status(report));
    yvex_cli_out_writef(fp, "source_manifest_authority: %s\n",
       source_render_manifest_authority(report));
    yvex_cli_out_writef(fp, "source_manifest_provenance_status: %s\n",
       source_render_manifest_provenance_status(report));
    yvex_cli_out_writef(fp, "source_manifest_native_inventory_status: %s\n",
       source_render_manifest_native_inventory_status(report));
    yvex_cli_out_writef(fp, "source_manifest_tensor_metadata_status: %s\n",
       source_render_manifest_tensor_metadata_status(report));
    yvex_cli_out_writef(fp, "source_manifest_consistency_status: %s\n",
       source_render_manifest_consistency_status(report));
    yvex_cli_out_writef(fp, "source_manifest_hardening_status: %s\n",
                   report->semantics.manifest_hardening_status);
    yvex_cli_out_writef(fp, "source_manifest_creation_performed: %s\n",
                   report->semantics.manifest_creation_performed
                       ? "true" : "false");
    yvex_cli_out_lines(fp, literal_lines_1, sizeof(literal_lines_1) / sizeof(literal_lines_1[0]));
    yvex_cli_out_writef(fp, "source_revision: %s\n",
                   report->identity_revision[0]
                       ? report->identity_revision
                       : "unknown");
    yvex_cli_out_writef(fp, "source_revision_status: %s\n",
                   report->semantics.revision_status);
    yvex_cli_out_writef(fp, "source_commit: %s\n",
                   report->identity_revision[0]
                       ? report->identity_revision
                       : "unknown");
    yvex_cli_out_writef(fp, "source_commit_status: %s\n",
                   report->semantics.revision_status);
    yvex_cli_out_lines(fp, literal_pair_3, sizeof(literal_pair_3) / sizeof(literal_pair_3[0]));
    yvex_cli_out_writef(fp, "source_license_status: %s\n",
       source_render_presence_verification_status(report->license_exists));
    yvex_cli_out_writef(fp, "source_readme_status: %s\n",
       source_render_presence_verification_status(report->readme_exists));
    yvex_cli_out_writef(fp, "source_identity_status: %s\n",
       source_render_identity_status(report));
    yvex_cli_out_lines(fp, literal_pair_2, sizeof(literal_pair_2) / sizeof(literal_pair_2[0]));

}

/* Render generic retained native and tensor inventory accounting. */
/* Purpose: Render source render generic inventory from typed facts (`source_render_generic_inventory`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void source_render_generic_inventory(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = &report->request;
    unsigned long i;

    if (strcmp(options->profile->family_key, "deepseek") != 0) {
    yvex_cli_out_writef(fp, "source_verification_status: not-verified\n");
    }
    yvex_cli_out_writef(fp, "source_remote_checked: false\n");
    yvex_cli_out_writef(fp, "native_inventory_status: %s\n",
       source_render_native_inventory_status(report));
    yvex_cli_out_writef(fp, "native_inventory_scope: top-level-safetensors-headers\n");
    yvex_cli_out_writef(fp, "native_inventory_source: %s\n",
       source_render_native_inventory_source(report));
    render_object_fields(fp, report, u64_fields_8, sizeof(u64_fields_8) / sizeof(u64_fields_8[0]));
    yvex_cli_out_lines(fp, literal_pair_1, sizeof(literal_pair_1) / sizeof(literal_pair_1[0]));
    yvex_cli_out_writef(fp, "native_tensor_count: %llu\n", report->native_tensor_count);
    yvex_cli_out_writef(fp, "native_tensor_metadata_status: %s\n",
       source_render_native_tensor_metadata_status(report));
    yvex_cli_out_writef(fp, "native_tensor_payload_status: %s\n",
       source_render_native_tensor_payload_status(report));
    render_object_fields(fp, report, u64_fields_9, sizeof(u64_fields_9) / sizeof(u64_fields_9[0]));
    yvex_cli_out_writef(fp, "native_largest_tensor_name: %s\n",
       report->native_largest_tensor_name[0]
           ? report->native_largest_tensor_name
           : "none");
    render_object_fields(fp, report, u64_fields_10, sizeof(u64_fields_10) / sizeof(u64_fields_10[0]));
    yvex_cli_out_writef(fp, "native_inventory_report_status: %s\n",
       source_render_native_inventory_report_status(report));
    yvex_cli_out_writef(fp, "native_inventory_path: %s\n",
       report->native_inventory_path[0] ? report->native_inventory_path : "unknown");
    yvex_cli_out_writef(fp, "source_tensor_metadata_status: %s\n",
       source_render_tensor_metadata_status(report));
    yvex_cli_out_writef(fp, "source_tensor_metadata_scope: safetensors-header\n");
    yvex_cli_out_writef(fp, "source_tensor_metadata_source: %s\n",
       source_render_tensor_metadata_source(report));
    yvex_cli_out_lines(fp, literal_pair_0, sizeof(literal_pair_0) / sizeof(literal_pair_0[0]));
    render_object_fields(fp, report, u64_fields_11, sizeof(u64_fields_11) / sizeof(u64_fields_11[0]));
    yvex_cli_out_writef(fp, "source_tensor_largest_name: %s\n",
       report->source_tensor_largest_name[0]
           ? report->source_tensor_largest_name
           : "none");
    yvex_cli_out_writef(fp, "source_tensor_largest_file: %s\n",
       report->source_tensor_largest_file[0]
           ? report->source_tensor_largest_file
           : "none");
    yvex_cli_out_writef(fp, "source_tensor_largest_dtype: %s\n",
       report->source_tensor_largest_dtype[0]
           ? report->source_tensor_largest_dtype
           : "none");
    yvex_cli_out_writef(fp, "source_tensor_largest_rank: %llu\n",
       report->source_tensor_largest_rank);
    yvex_cli_out_writef(fp, "source_tensor_largest_shape: %s\n",
       report->source_tensor_largest_shape[0]
           ? report->source_tensor_largest_shape
           : "[]");
    render_object_fields(fp, report, u64_fields_12, sizeof(u64_fields_12) / sizeof(u64_fields_12[0]));
    yvex_cli_out_writef(fp, "source_tensor_name_pattern_status: lexical-only\n");
    render_object_fields(fp, report, u64_fields_13, sizeof(u64_fields_13) / sizeof(u64_fields_13[0]));
    for (i = 0; i < report->source_tensor_sample_count; ++i) {
    const yvex_source_tensor_sample *sample = &report->source_tensor_samples[i];
    yvex_cli_out_writef(fp, "source_tensor_%lu_name: %s\n", i, sample->name);
    yvex_cli_out_writef(fp, "source_tensor_%lu_file: %s\n", i, sample->file);
    yvex_cli_out_writef(fp, "source_tensor_%lu_dtype: %s\n", i, sample->dtype);
    yvex_cli_out_writef(fp, "source_tensor_%lu_rank: %llu\n", i, sample->rank);
    yvex_cli_out_writef(fp, "source_tensor_%lu_shape: %s\n", i, sample->shape);
    yvex_cli_out_writef(fp, "source_tensor_%lu_elements: %llu\n", i, sample->elements);
    yvex_cli_out_writef(fp, "source_tensor_%lu_declared_bytes: %llu\n",
           i,
           sample->declared_bytes);
    }

}

/* Render final generic capability refusals and blockers. */
/* Purpose: Render source render generic boundary from typed facts (`source_render_generic_boundary`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
static void source_render_generic_boundary(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = &report->request;
    unsigned long i;

    yvex_cli_out_writef(fp, "model_class_profile_status: %s\n",
       strcmp(options->profile->family_key, "qwen") == 0 ||
       strcmp(options->profile->family_key, "gemma") == 0
           ? "command-visible"
           : "missing");
    yvex_cli_out_writef(fp, "tensor_map_path: %s\n",
       report->tensor_map_path[0] ? report->tensor_map_path : "unknown");
    yvex_cli_out_writef(fp, "tensor_map_status: %s\n",
       source_render_tensor_map_report_status(report));
    yvex_cli_out_writef(fp, "tensor_role_map_status: %s\n",
       source_render_tensor_role_map_report_status(report));
    yvex_cli_out_writef(fp, "output_head_map_path: %s\n",
       report->output_head_map_path[0] ? report->output_head_map_path : "unknown");
    yvex_cli_out_writef(fp, "output_head_map_status: %s\n",
       source_render_output_head_map_report_status(report));
    yvex_cli_out_writef(fp, "tokenizer_map_path: %s\n",
       report->tokenizer_map_path[0] ? report->tokenizer_map_path : "unknown");
    yvex_cli_out_writef(fp, "tokenizer_map_status: %s\n",
       source_render_tokenizer_map_report_status(report));
    yvex_cli_out_lines(fp, literal_lines_2, sizeof(literal_lines_2) / sizeof(literal_lines_2[0]));
    yvex_cli_out_writef(fp, "blocker_count: %lu\n", report->blocker_count);
    for (i = 0; i < report->blocker_count; ++i) {
    yvex_cli_out_writef(fp, "blocker_%lu: %s\n", i, report->blockers[i]);
    }
    yvex_cli_out_writef(fp, "next_required_rows: %s\n", report->next_row);
    yvex_cli_out_writef(fp, "boundary: source report only; no artifact/runtime/generation/benchmark\n");

}

/* Purpose: Render source render audit from typed facts (`yvex_source_render_audit`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_source_render_audit(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = report ? &report->request : NULL;

    yvex_cli_out_writef(fp, "source-report: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "status: %s\n", report->status);
    yvex_cli_out_writef(fp, "release: %s\n", options->release);
    yvex_cli_out_writef(fp, "family: %s\n", options->profile->display_family);
    yvex_cli_out_writef(fp, "family_key: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "model: %s\n", source_render_model_name(options, report));
    yvex_cli_out_writef(fp, "target_id: %s\n", options->target);
    if (strcmp(options->profile->family_key, "deepseek") == 0) {
        source_render_deepseek_config(fp, report);
        source_render_deepseek_inventory(fp, report);
        return report->exit_code;
    }
    source_render_generic_source(fp, report);
    source_render_generic_inventory(fp, report);
    source_render_generic_boundary(fp, report);
    return report->exit_code;
}

/* Purpose: Render source render json from typed facts (`yvex_source_render_json`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_source_render_json(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = report ? &report->request : NULL;
    const char *verification_status;
    char blocker_key[32];
    unsigned long i;

    if (!report || !options || !options->profile) return 3;
    verification_status = report->semantics.verification_status;
    yvex_cli_json_begin(fp);
    yvex_cli_json_field_str(fp, "status", report->status, 1);
    yvex_cli_json_field_str(fp, "family", options->profile->family_key, 1);
    yvex_cli_json_field_str(fp, "target_id", options->target, 1);
    yvex_cli_json_field_str(fp, "source_kind",
                            report->verification.source_kind, 1);
    yvex_cli_json_field_str(fp, "repository", report->identity_repo_id, 1);
    yvex_cli_json_field_str(fp, "revision", report->identity_revision, 1);
    yvex_cli_json_field_str(fp, "source_path", report->source_path, 1);
    yvex_cli_json_field_str(fp, "verification", verification_status, 1);
    yvex_cli_json_field_str(fp, "inventory_authority",
                            report->verification.inventory_authority, 1);
    yvex_cli_json_field_str(fp, "upstream_index_oid",
                            report->verification.upstream_index_oid, 1);
    yvex_cli_json_field_bool(fp, "upstream_index_identity_verified",
                             report->verification.upstream_index_identity_verified,
                             1);
    yvex_cli_json_field_u64(fp, "header_scan_count",
                            report->verification.header_scan_count, 1);
    yvex_cli_json_field_str(fp, "manifest_schema",
                            report->verification.manifest_schema, 1);
    yvex_cli_json_field_str(fp, "manifest_state",
                            report->verification.manifest_status, 1);
    yvex_cli_json_field_str(fp, "manifest_verification_stage",
                            report->verification.verification_stage, 1);
    yvex_cli_json_field_bool(fp, "manifest_verified",
                             report->verification.manifest_verified, 1);
    yvex_cli_json_field_str(fp, "top_blocker", report->top_blocker, 1);
    yvex_cli_json_field_u64(fp, "source_verification_blocker_count",
                            report->verification.blocker_count, 1);
    yvex_cli_json_field_u64(fp, "blocker_count", report->blocker_count, 1);
    for (i = 0; i < report->blocker_count; ++i) {
        (void)snprintf(blocker_key, sizeof(blocker_key), "blocker_%lu", i);
        yvex_cli_json_field_str(fp, blocker_key, report->blockers[i], 1);
    }
    yvex_cli_json_field_str(fp, "next", report->next_row, 1);
    yvex_cli_json_field_u64(fp, "source_file_count", report->source_file_count, 1);
    yvex_cli_json_field_u64(fp, "source_total_bytes", report->total_size_bytes, 1);
    yvex_cli_json_field_u64(fp, "shard_count", report->safetensors_count, 1);
    yvex_cli_json_field_u64(fp, "header_shard_count",
                            report->native_safetensors_header_read_count, 1);
    yvex_cli_json_field_u64(fp, "header_tensor_count",
                            report->native_tensor_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_bf16_count",
                            report->verification.dtype_bf16_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_f32_count",
                            report->verification.dtype_f32_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_i64_count",
                            report->verification.dtype_i64_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_i8_count",
                            report->verification.dtype_i8_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_f8_e4m3_count",
                            report->verification.dtype_f8_count, 1);
    yvex_cli_json_field_u64(fp, "header_dtype_f8_e8m0_count",
                            report->verification.dtype_f8_e8m0_count, 1);
    yvex_cli_json_field_bool(fp, "config_valid",
                             report->verification.config_valid, 1);
    yvex_cli_json_field_bool(fp, "tokenizer_valid",
                             report->semantics.tokenizer_verified,
                             1);
    yvex_cli_json_field_bool(fp, "generation_config_valid",
                             report->verification.generation_config_valid, 1);
    yvex_cli_json_field_bool(fp, "shard_index_valid",
                             report->verification.shard_index_valid, 1);
    yvex_cli_json_field_bool(fp, "tensor_payload_loaded", 0, 1);
    yvex_cli_json_field_str(fp, "artifact_status",
                            source_render_target_artifact_status(options->profile), 1);
    yvex_cli_json_field_str(fp, "runtime", "unsupported", 1);
    yvex_cli_json_field_str(fp, "generation", "unsupported", 1);
    yvex_cli_json_field_str(fp, "benchmark", "not-measured", 0);
    yvex_cli_json_end(fp);
    return report->exit_code;
}

/* Purpose: Render render usage from typed facts (`render_usage`). */
static void render_usage(FILE *fp)
{
    yvex_cli_out_writef(fp,
        "usage: yvex source-manifest report --family deepseek|qwen|gemma --release v0.1.0 [options]\n");
}

/* Purpose: Render source render help from typed facts (`yvex_source_render_help`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
void yvex_source_render_help(FILE *fp)
{
    render_usage(fp);
    yvex_cli_out_lines(fp, literal_lines_3, sizeof(literal_lines_3) / sizeof(literal_lines_3[0]));
    yvex_cli_out_writef(fp, "  --include-files --include-config --include-blockers --include-next\n");
    yvex_cli_out_writef(fp, "  --include-tensors [--tensor-limit N]\n");
    yvex_cli_out_writef(fp, "  --strict (return non-zero unless exact source verification passes)\n");
    yvex_cli_out_writef(fp, "  --audit | --json | --output normal|table|audit|json\n\n");
    yvex_cli_out_lines(fp, literal_lines_4, sizeof(literal_lines_4) / sizeof(literal_lines_4[0]));
}

/* Purpose: Render source render from typed facts (`yvex_source_render`).
 * Inputs: Borrowed typed facts.
 * Effects: Writes through CLI I/O only.
 * Failure: Typed refusal; outputs remain defined.
 * Boundary: No capability policy. */
int yvex_source_render(FILE *fp,
                       yvex_source_render_mode mode,
                       const yvex_source_report *report)
{
    if (!report) {
        yvex_cli_out_writef(fp, "source render: missing report\n");
        return 3;
    }
    if (mode == YVEX_SOURCE_RENDER_TABLE) {
        return yvex_source_render_table(fp, report);
    }
    if (mode == YVEX_SOURCE_RENDER_AUDIT) {
        return yvex_source_render_audit(fp, report);
    }
    if (mode == YVEX_SOURCE_RENDER_JSON) {
        return yvex_source_render_json(fp, report);
    }
    return yvex_source_render_normal(fp, report);
}
