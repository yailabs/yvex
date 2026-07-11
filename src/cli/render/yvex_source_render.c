/*
 * yvex_source_render.c - typed source pressure report renderer.
 *
 * Owner: src/cli/render.
 * Owns: normal/table/audit rendering for yvex_source_report.
 * Does not own: source report building, argv parsing, local file scanning, runtime, generation, eval, or benchmark.
 * Invariants: renders typed report facts and uses CLI IO writers only.
 * Boundary: rendering source facts is not source verification, artifact emission, or runtime readiness.
 */
#include "yvex_source_render.h"

#include "yvex_cli_out.h"
#include "yvex_cli_json.h"

#include <string.h>

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

static void yvex_source_render_tensor_rows(
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


static const char *source_render_model_name(const yvex_source_report_request *options,
                                          const yvex_source_report *report)
{
    (void)options;
    return report && report->semantics.model_name
               ? report->semantics.model_name : "unknown";
}

static const char *source_render_present_missing(int present)
{
    return present ? "present" : "missing";
}

static const char *source_render_tokenizer_status(const yvex_source_report *report)
{
    return report->semantics.tokenizer_status;
}

static const char *source_render_safetensors_status(const yvex_source_report *report)
{
    return report->semantics.safetensors_status;
}

static const char *source_render_manifest_status(const yvex_source_report *report)
{
    return report->semantics.manifest_status;
}

static const char *source_render_native_inventory_report_status(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_report_status;
}

static const char *source_render_tensor_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_map_report_status;
}

static const char *source_render_tensor_role_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_role_map_report_status;
}

static const char *source_render_output_head_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.output_head_map_report_status;
}

static const char *source_render_tokenizer_map_report_status(
    const yvex_source_report *report)
{
    return report->semantics.tokenizer_map_report_status;
}

static const char *source_render_native_inventory_status(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_status;
}

static const char *source_render_native_inventory_source(
    const yvex_source_report *report)
{
    return report->semantics.native_inventory_source;
}

static const char *source_render_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.tensor_metadata_status;
}

static const char *source_render_tensor_metadata_source(
    const yvex_source_report *report)
{
    return report->semantics.tensor_metadata_source;
}

static const char *source_render_native_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.native_tensor_metadata_status;
}

static const char *source_render_native_tensor_payload_status(
    const yvex_source_report *report)
{
    return report->semantics.native_tensor_payload_status;
}

static const char *source_render_sidecar_status(const yvex_source_report *report)
{
    return report->semantics.sidecar_status;
}

static const char *source_render_tensor_payload_status(const yvex_source_report *report)
{
    return report->semantics.tensor_payload_status;
}

static const char *source_render_target_artifact_status(const yvex_source_family_profile *profile)
{
    return profile && profile->yvex_produced_artifact_status
               ? profile->yvex_produced_artifact_status : "planned";
}

static const char *source_render_footprint_class(const yvex_source_report *report)
{
    return report->semantics.footprint_class;
}

static const char *source_render_footprint_status(const yvex_source_report *report)
{
    return report->semantics.footprint_status;
}

static const char *source_render_provenance_origin_normal(
    const yvex_source_report *report)
{
    return report->semantics.provenance_origin_normal;
}

static const char *source_render_provenance_origin_audit(
    const yvex_source_report_request *options,
    const yvex_source_report *report)
{
    (void)options;
    return report->semantics.provenance_origin_audit;
}

static const char *source_render_provenance_status(
    const yvex_source_report *report)
{
    return report->semantics.provenance_status;
}

static const char *source_render_identity_status(
    const yvex_source_report *report)
{
    return report->semantics.identity_status;
}

static const char *source_render_authority(const yvex_source_report *report)
{
    return report->semantics.authority;
}

static const char *source_render_authority_status(const yvex_source_report *report)
{
    return report->semantics.authority_status;
}

static const char *source_render_manifest_provenance_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_provenance_status;
}

static const char *source_render_manifest_authority(
    const yvex_source_report *report)
{
    return report->semantics.manifest_authority;
}

static const char *source_render_manifest_schema_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_schema_status;
}

static const char *source_render_manifest_family_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_family_status;
}

static const char *source_render_manifest_target_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_target_status;
}

static const char *source_render_manifest_artifact_class_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_artifact_class_status;
}

static const char *source_render_manifest_footprint_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_footprint_status;
}

static const char *source_render_manifest_native_inventory_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_native_inventory_status;
}

static const char *source_render_manifest_tensor_metadata_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_tensor_metadata_status;
}

static const char *source_render_manifest_consistency_status(
    const yvex_source_report *report)
{
    return report->semantics.manifest_consistency_status;
}

static const char *source_render_presence_verification_status(int present)
{
    return present ? "present-unverified" : "not-present";
}


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
                           "verification: %s  config=%s  tokenizer=%s  generation_config=%s  index=%s  headers=%llu/%llu\n",
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
    yvex_source_render_tensor_rows(fp, options, report);
    return report->exit_code;
}

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
    yvex_source_render_tensor_rows(fp, options, report);
    return report->exit_code;
}

int yvex_source_render_audit(FILE *fp, const yvex_source_report *report)
{
    const yvex_source_report_request *options = report ? &report->request : NULL;
    unsigned long i;

    yvex_cli_out_writef(fp, "source-report: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "status: %s\n", report->status);
    yvex_cli_out_writef(fp, "release: %s\n", options->release);
    yvex_cli_out_writef(fp, "family: %s\n", options->profile->display_family);
    yvex_cli_out_writef(fp, "family_key: %s\n", options->profile->family_key);
    yvex_cli_out_writef(fp, "model: %s\n", source_render_model_name(options, report));
    yvex_cli_out_writef(fp, "target_id: %s\n", options->target);
    if (strcmp(options->profile->family_key, "deepseek") == 0) {
        const yvex_source_verification *verification = &report->verification;

        yvex_cli_out_writef(fp, "canonical_repository: %s\n",
                           yvex_deepseek_v4_upstream_repo_id);
        yvex_cli_out_writef(fp, "source_kind: %s\n",
                           verification->source_kind[0]
                               ? verification->source_kind
                               : "missing");
        yvex_cli_out_writef(fp, "resolved_source_path: %s\n",
                           verification->resolved_source_path[0]
                               ? verification->resolved_source_path
                               : report->source_path);
        yvex_cli_out_writef(fp, "source_verification_status: %s\n",
                           report->semantics.verification_status);
        yvex_cli_out_writef(fp, "repository_verified: %s\n",
                           verification->repository_verified ? "true" : "false");
        yvex_cli_out_writef(fp, "revision_verified: %s\n",
                           verification->revision_verified ? "true" : "false");
        yvex_cli_out_writef(fp, "config_identity_status: %s\n",
                           report->semantics.config_identity_status);
        yvex_cli_out_writef(fp, "config_model_type: %s\n",
                           verification->model_type[0]
                               ? verification->model_type
                               : "missing");
        yvex_cli_out_writef(fp, "config_architecture: %s\n",
                           verification->architecture[0]
                               ? verification->architecture
                               : "missing");
        yvex_cli_out_writef(fp, "config_hidden_size: %llu\n",
                           verification->hidden_size);
        yvex_cli_out_writef(fp, "config_num_hidden_layers: %llu\n",
                           verification->num_hidden_layers);
        yvex_cli_out_writef(fp, "config_num_attention_heads: %llu\n",
                           verification->num_attention_heads);
        yvex_cli_out_writef(fp, "config_num_key_value_heads: %llu\n",
                           verification->num_key_value_heads);
        yvex_cli_out_writef(fp, "config_head_dim: %llu\n",
                           verification->head_dim);
        yvex_cli_out_writef(fp, "config_qk_rope_head_dim: %llu\n",
                           verification->qk_rope_head_dim);
        yvex_cli_out_writef(fp, "config_max_position_embeddings: %llu\n",
                           verification->max_position_embeddings);
        yvex_cli_out_writef(fp, "config_moe_intermediate_size: %llu\n",
                           verification->moe_intermediate_size);
        yvex_cli_out_writef(fp, "config_n_routed_experts: %llu\n",
                           verification->n_routed_experts);
        yvex_cli_out_writef(fp, "config_n_shared_experts: %llu\n",
                           verification->n_shared_experts);
        yvex_cli_out_writef(fp, "config_num_experts_per_tok: %llu\n",
                           verification->num_experts_per_tok);
        yvex_cli_out_writef(fp, "config_num_hash_layers: %llu\n",
                           verification->num_hash_layers);
        yvex_cli_out_writef(fp, "config_q_lora_rank: %llu\n",
                           verification->q_lora_rank);
        yvex_cli_out_writef(fp, "config_o_lora_rank: %llu\n",
                           verification->o_lora_rank);
        yvex_cli_out_writef(fp, "config_o_groups: %llu\n",
                           verification->o_groups);
        yvex_cli_out_writef(fp, "config_index_head_dim: %llu\n",
                           verification->index_head_dim);
        yvex_cli_out_writef(fp, "config_index_n_heads: %llu\n",
                           verification->index_n_heads);
        yvex_cli_out_writef(fp, "config_index_topk: %llu\n",
                           verification->index_topk);
        yvex_cli_out_writef(fp, "config_hc_eps: %s\n",
                           verification->hc_eps[0]
                               ? verification->hc_eps
                               : "missing");
        yvex_cli_out_writef(fp, "config_hc_mult: %llu\n",
                           verification->hc_mult);
        yvex_cli_out_writef(fp, "config_hc_sinkhorn_iters: %llu\n",
                           verification->hc_sinkhorn_iters);
        yvex_cli_out_writef(fp, "config_compress_rope_theta: %llu\n",
                           verification->compress_rope_theta);
        yvex_cli_out_writef(fp, "config_compress_ratio_count: %llu\n",
                           verification->compress_ratio_count);
        yvex_cli_out_writef(fp, "config_compress_ratios: [");
        for (i = 0; i < verification->compress_ratio_count; ++i) {
            yvex_cli_out_writef(fp, "%s%llu", i ? "," : "",
                               verification->compress_ratios[i]);
        }
        yvex_cli_out_writef(fp, "]\n");
        yvex_cli_out_writef(fp, "config_vocab_size: %llu\n",
                           verification->vocab_size);
        yvex_cli_out_writef(fp, "config_bos_token_id: %llu\n",
                           verification->bos_token_id);
        yvex_cli_out_writef(fp, "config_eos_token_id: %llu\n",
                           verification->eos_token_id);
        yvex_cli_out_writef(fp, "config_tie_word_embeddings: %s\n",
                           verification->tie_word_embeddings ? "true" : "false");
        yvex_cli_out_writef(fp, "config_attention_bias: %s\n",
                           verification->attention_bias ? "true" : "false");
        yvex_cli_out_writef(fp, "config_attention_dropout: %s\n",
                           verification->attention_dropout[0]
                               ? verification->attention_dropout
                               : "missing");
        yvex_cli_out_writef(fp, "config_sliding_window: %llu\n",
                           verification->sliding_window);
        yvex_cli_out_writef(fp, "config_num_nextn_predict_layers: %llu\n",
                           verification->num_nextn_predict_layers);
        yvex_cli_out_writef(fp, "config_rms_norm_eps: %s\n",
                           verification->rms_norm_eps[0]
                               ? verification->rms_norm_eps
                               : "missing");
        yvex_cli_out_writef(fp, "config_rope_theta: %llu\n",
                           verification->rope_theta);
        yvex_cli_out_writef(fp, "config_hidden_act: %s\n",
                           verification->hidden_act[0]
                               ? verification->hidden_act
                               : "missing");
        yvex_cli_out_writef(fp, "config_torch_dtype: %s\n",
                           verification->torch_dtype[0]
                               ? verification->torch_dtype
                               : "missing");
        yvex_cli_out_writef(fp, "config_expert_dtype: %s\n",
                           verification->expert_dtype[0]
                               ? verification->expert_dtype
                               : "missing");
        yvex_cli_out_writef(fp, "config_routed_scaling_factor: %s\n",
                           verification->routed_scaling_factor[0]
                               ? verification->routed_scaling_factor
                               : "missing");
        yvex_cli_out_writef(fp, "config_scoring_func: %s\n",
                           verification->scoring_func[0]
                               ? verification->scoring_func
                               : "missing");
        yvex_cli_out_writef(fp, "config_topk_method: %s\n",
                           verification->topk_method[0]
                               ? verification->topk_method
                               : "missing");
        yvex_cli_out_writef(fp, "config_norm_topk_prob: %s\n",
                           verification->norm_topk_prob ? "true" : "false");
        yvex_cli_out_writef(fp, "config_swiglu_limit: %s\n",
                           verification->swiglu_limit[0]
                               ? verification->swiglu_limit
                               : "missing");
        yvex_cli_out_writef(fp, "config_use_cache: %s\n",
                           verification->use_cache ? "true" : "false");
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
        yvex_cli_out_writef(fp, "tokenizer_status: %s\n",
                           report->semantics.tokenizer_verification_status);
        yvex_cli_out_writef(fp, "tokenizer_class: %s\n",
                           verification->tokenizer_class[0]
                               ? verification->tokenizer_class
                               : "missing");
        yvex_cli_out_writef(fp, "tokenizer_model_type: %s\n",
                           verification->tokenizer_model_type[0]
                               ? verification->tokenizer_model_type
                               : "missing");
        yvex_cli_out_writef(fp, "tokenizer_model_max_length: %llu\n",
                           verification->tokenizer_model_max_length);
        yvex_cli_out_writef(fp, "generation_config_status: %s\n",
                           report->semantics.generation_config_status);
        yvex_cli_out_writef(fp, "generation_config_from_model: %s\n",
                           verification->generation_from_model_config
                               ? "true"
                               : "false");
        yvex_cli_out_writef(fp, "generation_config_bos_token_id: %llu\n",
                           verification->generation_bos_token_id);
        yvex_cli_out_writef(fp, "generation_config_eos_token_id: %llu\n",
                           verification->generation_eos_token_id);
        yvex_cli_out_writef(fp, "generation_config_do_sample: %s\n",
                           verification->generation_do_sample
                               ? "true"
                               : "false");
        yvex_cli_out_writef(fp, "generation_config_temperature: %s\n",
                           verification->generation_temperature[0]
                               ? verification->generation_temperature
                               : "missing");
        yvex_cli_out_writef(fp, "generation_config_top_p: %s\n",
                           verification->generation_top_p[0]
                               ? verification->generation_top_p
                               : "missing");
        yvex_cli_out_writef(fp, "generation_config_transformers_version: %s\n",
                           verification->generation_transformers_version[0]
                               ? verification->generation_transformers_version
                               : "missing");
        yvex_cli_out_writef(fp, "shard_index_status: %s\n",
                           report->semantics.shard_index_status);
        yvex_cli_out_writef(fp, "indexed_tensor_count: %llu\n",
                           verification->indexed_tensor_count);
        yvex_cli_out_writef(fp, "referenced_shard_count: %llu\n",
                           verification->referenced_shard_count);
        yvex_cli_out_writef(fp, "header_index_match: %s\n",
                           verification->shard_index_headers_match
                               ? "true"
                               : "false");
        yvex_cli_out_writef(fp, "inventory_authority: %s\n",
                           report->semantics.inventory_authority);
        yvex_cli_out_writef(fp, "upstream_index_oid: %s\n",
                           verification->upstream_index_oid[0]
                               ? verification->upstream_index_oid
                               : "not-applicable");
        yvex_cli_out_writef(fp, "local_index_oid: %s\n",
                           verification->local_index_oid[0]
                               ? verification->local_index_oid
                               : "not-applicable");
        yvex_cli_out_writef(fp, "upstream_index_identity_verified: %s\n",
                           verification->upstream_index_identity_verified
                               ? "true" : "false");
        yvex_cli_out_writef(fp, "header_scan_count: %llu\n",
                           verification->header_scan_count);
        yvex_cli_out_writef(fp, "manifest_schema: %s\n",
                           verification->manifest_schema[0]
                               ? verification->manifest_schema : "missing");
        yvex_cli_out_writef(fp, "manifest_verification_stage: %s\n",
                           verification->verification_stage[0]
                               ? verification->verification_stage : "missing");
        yvex_cli_out_writef(fp, "manifest_verified: %s\n",
                           verification->manifest_verified ? "true" : "false");
        yvex_cli_out_writef(fp, "manifest_published: %s\n",
                           verification->manifest_published ? "true" : "false");
        yvex_cli_out_writef(fp, "manifest_reopened: %s\n",
                           verification->manifest_reopened ? "true" : "false");
        yvex_cli_out_writef(fp, "source_manifest_status: %s\n",
                           verification->manifest_status[0]
                               ? verification->manifest_status
                               : "missing");
        yvex_cli_out_writef(fp, "source_manifest_path: %s\n",
                           verification->manifest_path[0]
                               ? verification->manifest_path
                               : "missing");
        yvex_cli_out_writef(fp, "source_manifest_revision: %s\n",
                           verification->manifest_revision[0]
                               ? verification->manifest_revision
                               : "missing");
        yvex_cli_out_writef(fp, "source_revision: %s\n",
                           verification->revision[0]
                               ? verification->revision
                               : "missing");
        yvex_cli_out_writef(fp, "source_file_count: %llu\n",
                           verification->source_file_count);
        yvex_cli_out_writef(fp, "source_total_size_bytes: %llu\n",
                           verification->source_total_bytes);
        yvex_cli_out_writef(fp, "source_shard_count: %llu\n",
                           verification->shard_count);
        yvex_cli_out_writef(fp, "source_shard_bytes: %llu\n",
                           verification->shard_bytes);
        yvex_cli_out_writef(fp, "header_shard_count: %llu\n",
                           verification->header_shard_count);
        yvex_cli_out_writef(fp, "header_tensor_count: %llu\n",
                           verification->header_tensor_count);
        yvex_cli_out_writef(fp, "header_bytes: %llu\n",
                           verification->header_bytes);
        yvex_cli_out_writef(fp, "declared_tensor_bytes: %llu\n",
                           verification->declared_tensor_bytes);
        yvex_cli_out_writef(fp, "header_max_tensor_rank: %llu\n",
                           verification->max_tensor_rank);
        yvex_cli_out_writef(fp, "header_dtype_f16_count: %llu\n",
                           verification->dtype_f16_count);
        yvex_cli_out_writef(fp, "header_dtype_bf16_count: %llu\n",
                           verification->dtype_bf16_count);
        yvex_cli_out_writef(fp, "header_dtype_f32_count: %llu\n",
                           verification->dtype_f32_count);
        yvex_cli_out_writef(fp, "header_dtype_i64_count: %llu\n",
                           verification->dtype_i64_count);
        yvex_cli_out_writef(fp, "header_dtype_i8_count: %llu\n",
                           verification->dtype_i8_count);
        yvex_cli_out_writef(fp, "header_dtype_fp4_count: %llu\n",
                           verification->dtype_fp4_count);
        yvex_cli_out_writef(fp, "header_dtype_f8_count: %llu\n",
                           verification->dtype_f8_count);
        yvex_cli_out_writef(fp, "header_dtype_f8_e8m0_count: %llu\n",
                           verification->dtype_f8_e8m0_count);
        yvex_cli_out_writef(fp, "header_dtype_other_count: %llu\n",
                           verification->dtype_other_count);
        yvex_cli_out_writef(fp, "source_payload_loaded: false\n");
        yvex_cli_out_writef(fp, "release_target_selected: true\n");
        yvex_cli_out_writef(fp, "release_qtype: unselected\n");
        yvex_cli_out_writef(fp, "artifact_status: not-produced\n");
        yvex_cli_out_writef(fp, "runtime_claim: unsupported\n");
        yvex_cli_out_writef(fp, "generation: unsupported-full-model\n");
        yvex_cli_out_writef(fp, "benchmark_status: not-measured\n");
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
                           "boundary: exact source metadata/header verification only; no payload/artifact/runtime/generation/benchmark\n");
        return report->exit_code;
    }
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
    yvex_cli_out_writef(fp, "source_file_count: %llu\n", report->source_file_count);
    yvex_cli_out_writef(fp, "source_regular_file_count: %llu\n", report->source_regular_file_count);
    yvex_cli_out_writef(fp, "source_safetensors_count: %llu\n", report->safetensors_count);
    yvex_cli_out_writef(fp, "source_bin_count: %llu\n", report->bin_count);
    yvex_cli_out_writef(fp, "source_dat_count: %llu\n", report->dat_count);
    yvex_cli_out_writef(fp, "source_json_count: %llu\n", report->json_count);
    yvex_cli_out_writef(fp, "source_tokenizer_file_count: %llu\n", report->tokenizer_file_count);
    yvex_cli_out_writef(fp, "source_config_file_count: %llu\n", report->config_file_count);
    yvex_cli_out_writef(fp, "source_total_size_bytes: %llu\n", report->total_size_bytes);
    yvex_cli_out_writef(fp, "source_safetensors_size_bytes: %llu\n", report->safetensors_size_bytes);
    yvex_cli_out_writef(fp, "source_sidecar_size_bytes: %llu\n", report->sidecar_size_bytes);
    yvex_cli_out_writef(fp, "source_other_size_bytes: %llu\n", report->other_size_bytes);
    yvex_cli_out_writef(fp, "source_footprint_class: %s\n", source_render_footprint_class(report));
    yvex_cli_out_writef(fp, "source_footprint_status: %s\n", source_render_footprint_status(report));
    yvex_cli_out_writef(fp, "source_count_scope: top-level-regular-files\n");
    yvex_cli_out_writef(fp, "source_payload_loaded: false\n");
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
    yvex_cli_out_writef(fp, "source_manifest_payload_loaded: false\n");
    yvex_cli_out_writef(fp, "source_manifest_remote_checked: false\n");
    yvex_cli_out_writef(fp, "source_manifest_hash_computed: false\n");
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
    yvex_cli_out_writef(fp, "source_tag: unknown\n");
    yvex_cli_out_writef(fp, "source_tag_status: unknown\n");
    yvex_cli_out_writef(fp, "source_license_status: %s\n",
           source_render_presence_verification_status(report->license_exists));
    yvex_cli_out_writef(fp, "source_readme_status: %s\n",
           source_render_presence_verification_status(report->readme_exists));
    yvex_cli_out_writef(fp, "source_identity_status: %s\n",
           source_render_identity_status(report));
    yvex_cli_out_writef(fp, "source_digest_status: not-computed\n");
    yvex_cli_out_writef(fp, "source_hash_status: not-computed\n");
    if (strcmp(options->profile->family_key, "deepseek") != 0) {
        yvex_cli_out_writef(fp, "source_verification_status: not-verified\n");
    }
    yvex_cli_out_writef(fp, "source_remote_checked: false\n");
    yvex_cli_out_writef(fp, "native_inventory_status: %s\n",
           source_render_native_inventory_status(report));
    yvex_cli_out_writef(fp, "native_inventory_scope: top-level-safetensors-headers\n");
    yvex_cli_out_writef(fp, "native_inventory_source: %s\n",
           source_render_native_inventory_source(report));
    yvex_cli_out_writef(fp, "native_safetensors_count: %llu\n", report->native_safetensors_count);
    yvex_cli_out_writef(fp, "native_safetensors_opened: %llu\n", report->native_safetensors_opened);
    yvex_cli_out_writef(fp, "native_safetensors_header_read_count: %llu\n",
           report->native_safetensors_header_read_count);
    yvex_cli_out_writef(fp, "native_safetensors_header_error_count: %llu\n",
           report->native_safetensors_header_error_count);
    yvex_cli_out_writef(fp, "native_safetensors_header_bytes: %llu\n",
           report->native_safetensors_header_bytes);
    yvex_cli_out_writef(fp, "native_safetensors_payload_loaded: false\n");
    yvex_cli_out_writef(fp, "native_safetensors_payload_bytes_read: 0\n");
    yvex_cli_out_writef(fp, "native_tensor_count: %llu\n", report->native_tensor_count);
    yvex_cli_out_writef(fp, "native_tensor_metadata_status: %s\n",
           source_render_native_tensor_metadata_status(report));
    yvex_cli_out_writef(fp, "native_tensor_payload_status: %s\n",
           source_render_native_tensor_payload_status(report));
    yvex_cli_out_writef(fp, "native_declared_data_bytes: %llu\n",
           report->native_declared_data_bytes);
    yvex_cli_out_writef(fp, "native_declared_tensor_bytes: %llu\n",
           report->native_declared_tensor_bytes);
    yvex_cli_out_writef(fp, "native_max_rank: %llu\n", report->native_max_rank);
    yvex_cli_out_writef(fp, "native_max_tensor_elements: %llu\n",
           report->native_max_tensor_elements);
    yvex_cli_out_writef(fp, "native_largest_tensor_name: %s\n",
           report->native_largest_tensor_name[0]
               ? report->native_largest_tensor_name
               : "none");
    yvex_cli_out_writef(fp, "native_largest_tensor_bytes: %llu\n",
           report->native_largest_tensor_bytes);
    yvex_cli_out_writef(fp, "native_dtype_f16_count: %llu\n", report->native_dtype_f16_count);
    yvex_cli_out_writef(fp, "native_dtype_bf16_count: %llu\n", report->native_dtype_bf16_count);
    yvex_cli_out_writef(fp, "native_dtype_f32_count: %llu\n", report->native_dtype_f32_count);
    yvex_cli_out_writef(fp, "native_dtype_i8_count: %llu\n", report->native_dtype_i8_count);
    yvex_cli_out_writef(fp, "native_dtype_i16_count: %llu\n", report->native_dtype_i16_count);
    yvex_cli_out_writef(fp, "native_dtype_i32_count: %llu\n", report->native_dtype_i32_count);
    yvex_cli_out_writef(fp, "native_dtype_i64_count: %llu\n", report->native_dtype_i64_count);
    yvex_cli_out_writef(fp, "native_dtype_u8_count: %llu\n", report->native_dtype_u8_count);
    yvex_cli_out_writef(fp, "native_dtype_other_count: %llu\n", report->native_dtype_other_count);
    yvex_cli_out_writef(fp, "native_invalid_file_count: %llu\n", report->native_invalid_file_count);
    yvex_cli_out_writef(fp, "native_inventory_error_count: %llu\n",
           report->native_inventory_error_count);
    yvex_cli_out_writef(fp, "native_inventory_report_status: %s\n",
           source_render_native_inventory_report_status(report));
    yvex_cli_out_writef(fp, "native_inventory_path: %s\n",
           report->native_inventory_path[0] ? report->native_inventory_path : "unknown");
    yvex_cli_out_writef(fp, "source_tensor_metadata_status: %s\n",
           source_render_tensor_metadata_status(report));
    yvex_cli_out_writef(fp, "source_tensor_metadata_scope: safetensors-header\n");
    yvex_cli_out_writef(fp, "source_tensor_metadata_source: %s\n",
           source_render_tensor_metadata_source(report));
    yvex_cli_out_writef(fp, "source_tensor_metadata_payload_loaded: false\n");
    yvex_cli_out_writef(fp, "source_tensor_metadata_payload_bytes_read: 0\n");
    yvex_cli_out_writef(fp, "source_tensor_count: %llu\n", report->source_tensor_count);
    yvex_cli_out_writef(fp, "source_tensor_name_count: %llu\n", report->source_tensor_name_count);
    yvex_cli_out_writef(fp, "source_tensor_file_count: %llu\n", report->source_tensor_file_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_count: %llu\n", report->source_tensor_dtype_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_count: %llu\n", report->source_tensor_rank_count);
    yvex_cli_out_writef(fp, "source_tensor_shape_count: %llu\n", report->source_tensor_shape_count);
    yvex_cli_out_writef(fp, "source_tensor_declared_data_bytes: %llu\n",
           report->source_tensor_declared_data_bytes);
    yvex_cli_out_writef(fp, "source_tensor_declared_tensor_bytes: %llu\n",
           report->source_tensor_declared_tensor_bytes);
    yvex_cli_out_writef(fp, "source_tensor_total_elements: %llu\n",
           report->source_tensor_total_elements);
    yvex_cli_out_writef(fp, "source_tensor_max_rank: %llu\n", report->source_tensor_max_rank);
    yvex_cli_out_writef(fp, "source_tensor_max_elements: %llu\n", report->source_tensor_max_elements);
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
    yvex_cli_out_writef(fp, "source_tensor_largest_elements: %llu\n",
           report->source_tensor_largest_elements);
    yvex_cli_out_writef(fp, "source_tensor_largest_declared_bytes: %llu\n",
           report->source_tensor_largest_declared_bytes);
    yvex_cli_out_writef(fp, "source_tensor_dtype_f16_count: %llu\n",
           report->source_tensor_dtype_f16_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_bf16_count: %llu\n",
           report->source_tensor_dtype_bf16_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_f32_count: %llu\n",
           report->source_tensor_dtype_f32_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_i8_count: %llu\n",
           report->source_tensor_dtype_i8_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_i16_count: %llu\n",
           report->source_tensor_dtype_i16_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_i32_count: %llu\n",
           report->source_tensor_dtype_i32_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_i64_count: %llu\n",
           report->source_tensor_dtype_i64_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_u8_count: %llu\n",
           report->source_tensor_dtype_u8_count);
    yvex_cli_out_writef(fp, "source_tensor_dtype_other_count: %llu\n",
           report->source_tensor_dtype_other_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_0_count: %llu\n",
           report->source_tensor_rank_0_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_1_count: %llu\n",
           report->source_tensor_rank_1_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_2_count: %llu\n",
           report->source_tensor_rank_2_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_3_count: %llu\n",
           report->source_tensor_rank_3_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_4_count: %llu\n",
           report->source_tensor_rank_4_count);
    yvex_cli_out_writef(fp, "source_tensor_rank_other_count: %llu\n",
           report->source_tensor_rank_other_count);
    yvex_cli_out_writef(fp, "source_tensor_name_pattern_status: lexical-only\n");
    yvex_cli_out_writef(fp, "source_tensor_name_embed_count: %llu\n",
           report->source_tensor_name_embed_count);
    yvex_cli_out_writef(fp, "source_tensor_name_attn_count: %llu\n",
           report->source_tensor_name_attn_count);
    yvex_cli_out_writef(fp, "source_tensor_name_mlp_count: %llu\n",
           report->source_tensor_name_mlp_count);
    yvex_cli_out_writef(fp, "source_tensor_name_norm_count: %llu\n",
           report->source_tensor_name_norm_count);
    yvex_cli_out_writef(fp, "source_tensor_name_lm_head_count: %llu\n",
           report->source_tensor_name_lm_head_count);
    yvex_cli_out_writef(fp, "source_tensor_name_other_count: %llu\n",
           report->source_tensor_name_other_count);
    yvex_cli_out_writef(fp, "source_tensor_metadata_error_count: %llu\n",
           report->source_tensor_metadata_error_count);
    yvex_cli_out_writef(fp, "source_tensor_sample_count: %llu\n",
           report->source_tensor_sample_count);
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
    yvex_cli_out_writef(fp, "artifact_status: missing\n");
    yvex_cli_out_writef(fp, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(fp, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(fp, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(fp, "release_ready: false\n");
    yvex_cli_out_writef(fp, "blocker_count: %lu\n", report->blocker_count);
    for (i = 0; i < report->blocker_count; ++i) {
        yvex_cli_out_writef(fp, "blocker_%lu: %s\n", i, report->blockers[i]);
    }
    yvex_cli_out_writef(fp, "next_required_rows: %s\n", report->next_row);
    yvex_cli_out_writef(fp, "boundary: source report only; no artifact/runtime/generation/benchmark\n");
    return report->exit_code;
}

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

static void yvex_source_render_usage(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex source-manifest report --family deepseek|qwen|gemma --release v0.1.0 [options]\n");
}

void yvex_source_render_help(FILE *fp)
{
    yvex_source_render_usage(fp);
    yvex_cli_out_writef(fp, "\nOptions:\n");
    yvex_cli_out_writef(fp, "  --family deepseek|qwen|gemma\n");
    yvex_cli_out_writef(fp, "  --release v0.1.0\n");
    yvex_cli_out_writef(fp, "  --models-root DIR\n");
    yvex_cli_out_writef(fp, "  --source DIR\n");
    yvex_cli_out_writef(fp, "  --target deepseek4-v4-flash|qwen3-8b|qwen-small|qwen-medium|gemma-4-12b-it\n");
    yvex_cli_out_writef(fp, "  --" "include-files --" "include-config --" "include-blockers --" "include-next\n");
    yvex_cli_out_writef(fp, "  --" "include-tensors [--tensor-limit N]\n");
    yvex_cli_out_writef(fp, "  --strict (return non-zero unless exact source verification passes)\n");
    yvex_cli_out_writef(fp, "  --" "audit | --json | --" "output normal|table|audit|json\n\n");
    yvex_cli_out_writef(fp, "Report fields include source artifact class, target artifact class, source footprint, and source provenance evidence.\n");
    yvex_cli_out_writef(fp, "Source footprint uses checked byte accounting without loading tensor payloads.\n");
    yvex_cli_out_writef(fp, "DeepSeek strict verification checks structured repository, revision, model/tokenizer/generation config, index, shard, dtype, and header facts. Qwen and Gemma retain their bounded report behavior.\n");
    yvex_cli_out_writef(fp, "Native safetensors inventory reads safetensors headers only and never loads tensor payload bytes.\n");
    yvex_cli_out_writef(fp, "Source tensor metadata inventory is derived from safetensors headers only and does not map tensors to runtime roles.\n");
    yvex_cli_out_writef(fp, "Strict DeepSeek verification may atomically promote and reopen the canonical YVEX manifest after all metadata/header checks pass; it never writes inside the official source tree.\n");
    yvex_cli_out_writef(fp, "Verification does not hash weight payloads or load tensor payload bytes.\n");
    yvex_cli_out_writef(fp, "The source pressure report inspects source-path readiness only. It does not download weights, emit artifacts, materialize tensors, execute runtime paths, generate, evaluate, benchmark, or mark a release ready.\n");
}


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
