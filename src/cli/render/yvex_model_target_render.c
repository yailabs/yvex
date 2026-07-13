/*
 * yvex_model_target_render.c - model-target typed report rendering.
 *
 * Owner:
 *   src/cli/render
 *
 * Owns:
 *   normal/table/audit/help rendering for typed model-target reports.
 *
 * Does not own:
 *   CLI argument parsing, command dispatch, target catalogs, report
 *   construction, sidecar writing, runtime execution, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   this typed renderer writes only through src/cli/io helpers and renders
 *   typed report rows supplied by model-target report builders.
 *
 * Boundary:
 *   model-target rendering serializes existing report-only facts and does not
 *   prove quantization, artifact emission, runtime execution, generation,
 *   evaluation, benchmark, throughput, or release readiness.
 */
#include "yvex_model_target_render.h"

#include "yvex_cli_out.h"
#include "../../model/target/yvex_deepseek_gguf_map.h"
#include "../../model/target/yvex_deepseek_tensor_coverage.h"

static int model_target_render_rows(FILE *fp,
                                    const yvex_model_target_text_value *rows,
                                    unsigned long count)
{
    unsigned long i;
    int rc = 0;

    for (i = 0; i < count; ++i) {
        rc = yvex_cli_out_writef(fp, "%s\n", rows[i].value);
        if (rc < 0) {
            return rc;
        }
    }
    return rc;
}

static int model_target_render_table_rows(FILE *fp,
                                          const yvex_model_target_report *report)
{
    unsigned long r;
    int rc = 0;

    for (r = 0; r < report->table_row_count; ++r) {
        const yvex_model_target_table_row *row = &report->table_rows[r];
        unsigned int c;

        for (c = 0; c < row->column_count; ++c) {
            rc = yvex_cli_out_writef(fp, "%s%s",
                                     c == 0 ? "" : "  ",
                                     row->columns[c]);
            if (rc < 0) {
                return rc;
            }
        }
        rc = yvex_cli_out_writef(fp, "\n");
        if (rc < 0) {
            return rc;
        }
    }
    return rc;
}

/* Serializes the immutable logical emission plan without deriving map policy. */
static int model_target_render_deepseek_map(
    FILE *fp,
    yvex_model_target_render_mode mode,
    const yvex_model_target_report *report)
{
    const yvex_deepseek_gguf_map_summary *summary =
        yvex_deepseek_gguf_map_summary_get(report->deepseek_gguf_map);
    int rc = 0;

    if (!summary) return 0;
    if (mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        return yvex_cli_out_writef(
            fp,
            "{\"status\":\"%s\",\"target_id\":\"%s\",\"source_contributions\":%llu,\"descriptors\":%llu,\"trunk_descriptors\":%llu,\"mtp_descriptors\":%llu,\"pinned_standard_names\":%llu,\"extension_names\":%llu,\"metadata\":%llu,\"header_scans\":%llu,\"payload_bytes_read\":%llu,\"mapping_identity\":\"%016llx\",\"artifact\":\"not-produced\",\"runtime\":\"unsupported\",\"generation\":\"unsupported\",\"next\":\"%s\"}\n",
            report->status, report->target_id,
            summary->source_contribution_count, summary->descriptor_count,
            summary->trunk_descriptor_count, summary->mtp_descriptor_count,
            summary->pinned_standard_count, summary->extension_count,
            summary->metadata_count, summary->header_scan_count,
            summary->payload_bytes_read, summary->mapping_identity,
            report->next_row);
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        rc |= yvex_cli_out_writef(
            fp, "TARGET  STATUS  SOURCES  GGUF  TRUNK  MTP  METADATA  PAYLOAD  NEXT\n");
        rc |= yvex_cli_out_writef(
            fp, "%s  %s  %llu  %llu  %llu  %llu  %llu  %llu  %s\n",
            report->target_id, report->status,
            summary->source_contribution_count, summary->descriptor_count,
            summary->trunk_descriptor_count, summary->mtp_descriptor_count,
            summary->metadata_count, summary->payload_bytes_read,
            report->next_row);
        return rc < 0 ? rc : 0;
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        unsigned int collection;
        rc |= yvex_cli_out_writef(fp, "mapping_status: %s\n", report->status);
        rc |= yvex_cli_out_writef(fp, "target_id: %s\n", report->target_id);
        rc |= yvex_cli_out_writef(fp, "source_contribution_count: %llu\n",
                                  summary->source_contribution_count);
        rc |= yvex_cli_out_writef(fp, "descriptor_count: %llu\n",
                                  summary->descriptor_count);
        rc |= yvex_cli_out_writef(fp, "trunk_descriptor_count: %llu\n",
                                  summary->trunk_descriptor_count);
        rc |= yvex_cli_out_writef(fp, "mtp_descriptor_count: %llu\n",
                                  summary->mtp_descriptor_count);
        rc |= yvex_cli_out_writef(fp, "pinned_standard_name_count: %llu\n",
                                  summary->pinned_standard_count);
        rc |= yvex_cli_out_writef(fp, "semantic_standard_name_count: %llu\n",
                                  summary->semantic_standard_count);
        rc |= yvex_cli_out_writef(fp, "extension_name_count: %llu\n",
                                  summary->extension_count);
        for (collection = 0u;
             collection < YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT;
             ++collection) {
            rc |= yvex_cli_out_writef(
                fp, "collection_%u_count: %llu\n", collection,
                summary->collection_counts[collection]);
        }
        rc |= yvex_cli_out_writef(fp, "metadata_count: %llu\n",
                                  summary->metadata_count);
        rc |= yvex_cli_out_writef(fp, "header_scan_count: %llu\n",
                                  summary->header_scan_count);
        rc |= yvex_cli_out_writef(fp, "payload_bytes_read: %llu\n",
                                  summary->payload_bytes_read);
        rc |= yvex_cli_out_writef(fp, "source_identity: %016llx\n",
                                  summary->source_identity);
        rc |= yvex_cli_out_writef(fp, "coverage_identity: %016llx\n",
                                  summary->coverage_identity);
        rc |= yvex_cli_out_writef(fp, "mapping_identity: %016llx\n",
                                  summary->mapping_identity);
        rc |= yvex_cli_out_writef(fp, "artifact_status: not-produced\n");
        rc |= yvex_cli_out_writef(fp, "runtime_status: %s\n",
                                  report->runtime_status);
        rc |= yvex_cli_out_writef(fp, "generation_status: %s\n",
                                  report->generation_status);
        rc |= yvex_cli_out_writef(fp, "next: %s\n", report->next_row);
        rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
        return rc < 0 ? rc : 0;
    }
    rc |= yvex_cli_out_writef(fp, "deepseek-gguf-map: %s [%s]\n",
                              report->target_id, report->status);
    rc |= yvex_cli_out_writef(
        fp, "plan: sources=%llu gguf=%llu trunk=%llu mtp=%llu metadata=%llu\n",
        summary->source_contribution_count, summary->descriptor_count,
        summary->trunk_descriptor_count, summary->mtp_descriptor_count,
        summary->metadata_count);
    rc |= yvex_cli_out_writef(
        fp, "evidence: header-scans=%llu payload-bytes=%llu identity=%016llx\n",
        summary->header_scan_count, summary->payload_bytes_read,
        summary->mapping_identity);
    rc |= yvex_cli_out_writef(fp, "next: %s\n", report->next_row);
    rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
    return rc < 0 ? rc : 0;
}

/* Serializes an already admitted one-to-one tensor coverage result. */
static int model_target_render_deepseek_coverage(
    FILE *fp,
    yvex_model_target_render_mode mode,
    const yvex_model_target_report *report)
{
    const yvex_deepseek_tensor_coverage_summary *summary =
        yvex_deepseek_tensor_coverage_summary_get(
            report->deepseek_tensor_coverage);
    int rc = 0;

    if (!summary) return 0;
    if (mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        return yvex_cli_out_writef(
            fp,
            "{\"status\":\"%s\",\"target_id\":\"%s\",\"source_tensors\":%llu,\"required_tensors\":%llu,\"matched_tensors\":%llu,\"missing\":%llu,\"ambiguous\":%llu,\"unexpected\":%llu,\"header_scans\":%llu,\"payload_bytes_read\":%llu,\"coverage_identity\":\"%016llx\",\"mapping\":\"blocked\",\"runtime\":\"unsupported\",\"generation\":\"unsupported\",\"next\":\"%s\"}\n",
            report->status, report->target_id, summary->source_tensor_count,
            summary->required_tensor_count, summary->matched_tensor_count,
            summary->missing_count, summary->ambiguous_count,
            summary->unexpected_count,
            summary->header_scan_count, summary->payload_bytes_read,
            summary->coverage_identity, report->next_row);
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        rc |= yvex_cli_out_writef(
            fp, "TARGET  STATUS  SOURCE  REQUIRED  MATCHED  MISSING  UNEXPECTED  NEXT\n");
        rc |= yvex_cli_out_writef(
            fp, "%s  %s  %llu  %llu  %llu  %llu  %llu  %s\n",
            report->target_id, report->status, summary->source_tensor_count,
            summary->required_tensor_count, summary->matched_tensor_count,
            summary->missing_count, summary->unexpected_count,
            report->next_row);
        return rc < 0 ? rc : 0;
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        unsigned int collection;
        rc |= yvex_cli_out_writef(fp, "tensor_coverage_status: %s\n",
                                  report->status);
        rc |= yvex_cli_out_writef(fp, "target_id: %s\n", report->target_id);
        rc |= yvex_cli_out_writef(fp, "source_tensor_count: %llu\n",
                                  summary->source_tensor_count);
        rc |= yvex_cli_out_writef(fp, "required_tensor_count: %llu\n",
                                  summary->required_tensor_count);
        rc |= yvex_cli_out_writef(fp, "matched_tensor_count: %llu\n",
                                  summary->matched_tensor_count);
        rc |= yvex_cli_out_writef(fp, "missing_tensor_count: %llu\n",
                                  summary->missing_count);
        rc |= yvex_cli_out_writef(fp, "ambiguous_tensor_count: %llu\n",
                                  summary->ambiguous_count);
        rc |= yvex_cli_out_writef(fp, "unexpected_tensor_count: %llu\n",
                                  summary->unexpected_count);
        for (collection = 0u;
             collection < YVEX_DEEPSEEK_TENSOR_COLLECTION_COUNT;
             ++collection) {
            rc |= yvex_cli_out_writef(
                fp, "collection_%s_count: %llu\n",
                yvex_deepseek_tensor_collection_name(
                    (yvex_deepseek_tensor_collection)collection),
                summary->collection_counts[collection]);
        }
        rc |= yvex_cli_out_writef(fp, "header_scan_count: %llu\n",
                                  summary->header_scan_count);
        rc |= yvex_cli_out_writef(fp, "payload_bytes_read: %llu\n",
                                  summary->payload_bytes_read);
        rc |= yvex_cli_out_writef(fp, "source_lookup_count: %llu\n",
                                  summary->source_lookup_count);
        rc |= yvex_cli_out_writef(fp, "source_collision_count: %llu\n",
                                  summary->source_collision_count);
        rc |= yvex_cli_out_writef(fp, "source_maximum_probe: %llu\n",
                                  summary->source_maximum_probe);
        rc |= yvex_cli_out_writef(fp, "source_identity: %016llx\n",
                                  summary->source_identity);
        rc |= yvex_cli_out_writef(fp, "coverage_identity: %016llx\n",
                                  summary->coverage_identity);
        rc |= yvex_cli_out_writef(fp, "mapping: blocked\n");
        rc |= yvex_cli_out_writef(fp, "runtime_execution: unsupported\n");
        rc |= yvex_cli_out_writef(fp, "generation: unsupported\n");
        rc |= yvex_cli_out_writef(fp, "next_required_row: %s\n",
                                  report->next_row);
        rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
        return rc < 0 ? rc : 0;
    }
    rc |= yvex_cli_out_writef(fp, "tensor-coverage: deepseek-v4-flash\n");
    rc |= yvex_cli_out_writef(fp, "target: %s\n", report->target_id);
    rc |= yvex_cli_out_writef(fp, "status: %s\n", report->status);
    rc |= yvex_cli_out_writef(
        fp, "coverage: source=%llu required=%llu matched=%llu missing=%llu unexpected=%llu\n",
        summary->source_tensor_count, summary->required_tensor_count,
        summary->matched_tensor_count, summary->missing_count,
        summary->unexpected_count);
    rc |= yvex_cli_out_writef(fp, "identity: %016llx\n",
                              summary->coverage_identity);
    rc |= yvex_cli_out_writef(fp, "next: %s\n", report->next_row);
    rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
    return rc < 0 ? rc : 0;
}

/* Formats the compact architecture view from the immutable typed IR. */
static int model_target_render_deepseek_normal(
    FILE *fp,
    const yvex_model_target_report *report,
    const yvex_deepseek_v4_ir *ir,
    const yvex_deepseek_v4_model_spec *model)
{
    const yvex_deepseek_v4_layer_spec *first =
        yvex_deepseek_v4_ir_layer_at(ir, 0u);
    int rc = 0;

    rc |= yvex_cli_out_writef(fp, "model-class: deepseek-v4-flash\n");
    rc |= yvex_cli_out_writef(fp, "target: %s\n", model->target_id);
    rc |= yvex_cli_out_writef(fp, "status: %s\n", report->status);
    rc |= yvex_cli_out_writef(
        fp, "topology: layers=%llu mtp=%llu hidden=%llu vocab=%llu context=%llu\n",
        model->main_layer_count, model->auxiliary_layer_count,
        model->hidden_size, model->vocabulary_size, model->maximum_context);
    rc |= yvex_cli_out_writef(
        fp, "attention: swa=%llu csa=%llu hca=%llu heads=%llu kv_heads=%llu head_dim=%llu rope_dim=%llu\n",
        model->swa_layer_count, model->csa_layer_count,
        model->hca_layer_count, first->query_heads, first->kv_heads,
        first->head_dimension, first->rope_head_dimension);
    rc |= yvex_cli_out_writef(
        fp, "routing: hash=%llu learned=%llu experts=%llu topk=%llu shared=%llu\n",
        model->hash_router_layer_count, model->learned_router_layer_count,
        first->moe.routed_experts, first->moe.experts_per_token,
        first->moe.shared_experts);
    rc |= yvex_cli_out_writef(
        fp, "mhc: streams=%llu expanded=%llu mixing_rows=%llu sinkhorn=%llu\n",
        model->final_mhc.residual_streams, model->final_mhc.expanded_width,
        model->final_mhc.mixing_rows, model->final_mhc.sinkhorn_iterations);
    rc |= yvex_cli_out_writef(fp, "next: %s\n", report->next_row);
    rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
    return rc < 0 ? rc : 0;
}

/* Formats one table row without deriving architecture state in the renderer. */
static int model_target_render_deepseek_table(
    FILE *fp,
    const yvex_model_target_report *report,
    const yvex_deepseek_v4_model_spec *model)
{
    int rc = 0;

    rc |= yvex_cli_out_writef(
        fp, "TARGET  STATUS  LAYERS  MTP  SWA  CSA  HCA  HASH  LEARNED  NEXT\n");
    rc |= yvex_cli_out_writef(
        fp, "%s  %s  %llu  %llu  %llu  %llu  %llu  %llu  %llu  %s\n",
        model->target_id, report->status, model->main_layer_count,
        model->auxiliary_layer_count, model->swa_layer_count,
        model->csa_layer_count, model->hca_layer_count,
        model->hash_router_layer_count, model->learned_router_layer_count,
        report->next_row);
    return rc < 0 ? rc : 0;
}

/* Formats complete audit evidence while preserving the IR as semantic owner. */
static int model_target_render_deepseek_audit(
    FILE *fp,
    const yvex_model_target_report *report,
    const yvex_deepseek_v4_ir *ir,
    const yvex_deepseek_v4_model_spec *model)
{
    unsigned long long i;
    int rc = 0;

    rc |= yvex_cli_out_writef(fp, "architecture_ir_status: %s\n",
                              report->status);
    rc |= yvex_cli_out_writef(fp, "target_id: %s\n", model->target_id);
    rc |= yvex_cli_out_writef(fp, "family: %s\n", model->family);
    rc |= yvex_cli_out_writef(fp, "architecture: %s\n",
                              model->architecture);
    rc |= yvex_cli_out_writef(fp, "repository: %s\n", model->repository);
    rc |= yvex_cli_out_writef(fp, "revision: %s\n", model->revision);
    rc |= yvex_cli_out_writef(fp, "verification_stage: %s\n",
                              model->verification_stage);
    rc |= yvex_cli_out_writef(fp, "paper_revision: %s\n",
                              model->paper_revision);
    rc |= yvex_cli_out_writef(fp, "sglang_revision: %s\n",
                              model->sglang_revision);
    rc |= yvex_cli_out_writef(fp, "vllm_revision: %s\n",
                              model->vllm_revision);
    rc |= yvex_cli_out_writef(fp, "hidden_size: %llu\n", model->hidden_size);
    rc |= yvex_cli_out_writef(fp, "vocabulary_size: %llu\n",
                              model->vocabulary_size);
    rc |= yvex_cli_out_writef(fp, "maximum_context: %llu\n",
                              model->maximum_context);
    rc |= yvex_cli_out_writef(fp, "main_layer_count: %llu\n",
                              model->main_layer_count);
    rc |= yvex_cli_out_writef(fp, "auxiliary_layer_count: %llu\n",
                              model->auxiliary_layer_count);
    rc |= yvex_cli_out_writef(fp, "swa_layer_count: %llu\n",
                              model->swa_layer_count);
    rc |= yvex_cli_out_writef(fp, "csa_layer_count: %llu\n",
                              model->csa_layer_count);
    rc |= yvex_cli_out_writef(fp, "hca_layer_count: %llu\n",
                              model->hca_layer_count);
    rc |= yvex_cli_out_writef(fp, "hash_router_layer_count: %llu\n",
                              model->hash_router_layer_count);
    rc |= yvex_cli_out_writef(fp, "learned_router_layer_count: %llu\n",
                              model->learned_router_layer_count);
    rc |= yvex_cli_out_writef(fp, "mhc_residual_streams: %llu\n",
                              model->final_mhc.residual_streams);
    rc |= yvex_cli_out_writef(fp, "mhc_expanded_width: %llu\n",
                              model->final_mhc.expanded_width);
    rc |= yvex_cli_out_writef(fp, "mhc_mixing_rows: %llu\n",
                              model->final_mhc.mixing_rows);
    rc |= yvex_cli_out_writef(fp, "mhc_mixing_columns: %llu\n",
                              model->final_mhc.mixing_columns);
    rc |= yvex_cli_out_writef(fp, "mhc_sinkhorn_iterations: %llu\n",
                              model->final_mhc.sinkhorn_iterations);
    rc |= yvex_cli_out_writef(fp, "final_mhc_post_required: %s\n",
                              model->final_mhc_post_required ? "true" : "false");
    rc |= yvex_cli_out_writef(fp, "final_mhc_head_required: %s\n",
                              model->final_mhc_head_required ? "true" : "false");
    rc |= yvex_cli_out_writef(fp, "final_norm_after_mhc_head: %s\n",
                              model->final_norm_after_mhc_head ? "true" : "false");
    rc |= yvex_cli_out_writef(fp, "tokenizer_class: %s\n",
                              model->tokenizer.tokenizer_class);
    rc |= yvex_cli_out_writef(fp, "tokenizer_model_type: %s\n",
                              model->tokenizer.model_type);
    rc |= yvex_cli_out_writef(fp, "tokenizer_vocabulary_size: %llu\n",
                              model->tokenizer.vocabulary_size);
    rc |= yvex_cli_out_writef(fp, "tokenizer_base_vocab_entries: %llu\n",
                              model->tokenizer.base_vocab_entries);
    rc |= yvex_cli_out_writef(fp, "tokenizer_added_token_entries: %llu\n",
                              model->tokenizer.added_token_entries);
    rc |= yvex_cli_out_writef(fp, "bos_token_id: %llu\n",
                              model->tokenizer.bos_token_id);
    rc |= yvex_cli_out_writef(fp, "eos_token_id: %llu\n",
                              model->tokenizer.eos_token_id);
    rc |= yvex_cli_out_writef(fp, "output_head_required: %s\n",
                              model->output.required ? "true" : "false");
    rc |= yvex_cli_out_writef(fp, "output_head_tied: %s\n",
                              model->output.tied_to_embedding ? "true" : "false");
    rc |= yvex_cli_out_writef(
        fp, "source_weight_dtype: %s\n",
        yvex_deepseek_v4_source_weight_dtype_name(
            model->source_constraint.weight_dtype));
    rc |= yvex_cli_out_writef(
        fp, "source_expert_dtype: %s\n",
        yvex_deepseek_v4_source_expert_dtype_name(
            model->source_constraint.expert_dtype));
    rc |= yvex_cli_out_writef(
        fp, "source_quantization: %s block=%llux%llu\n",
        yvex_deepseek_v4_source_quantization_name(
            model->source_constraint.quantization),
        model->source_constraint.quant_block_rows,
        model->source_constraint.quant_block_columns);
    rc |= yvex_cli_out_writef(fp, "source_header_scan_count: %llu\n",
                              model->source_header_scan_count);
    rc |= yvex_cli_out_writef(fp, "source_header_tensor_count: %llu\n",
                              model->source_header_tensor_count);
    rc |= yvex_cli_out_writef(fp, "source_payload_bytes_read: %llu\n",
                              model->source_payload_bytes_read);
    for (i = 0u; i < yvex_deepseek_v4_ir_layer_count(ir); ++i) {
        const yvex_deepseek_v4_layer_spec *layer =
            yvex_deepseek_v4_ir_layer_at(ir, i);
        rc |= yvex_cli_out_writef(
            fp,
            "layer_%llu: attention=%s ratio=%llu kv=%s router=%s mhc_entry=%s\n",
            layer->layer_index,
            yvex_deepseek_v4_attention_name(layer->attention_class),
            layer->compression_ratio,
            yvex_deepseek_v4_kv_name(layer->kv.class_id),
            yvex_deepseek_v4_router_name(layer->moe.router_class),
            layer->mhc.entry == YVEX_DEEPSEEK_V4_MHC_STANDALONE_PRE
                ? "standalone-pre"
                : "fused-prior-post-pre");
    }
    for (i = 0u; i < yvex_deepseek_v4_ir_auxiliary_count(ir); ++i) {
        const yvex_deepseek_v4_auxiliary_spec *aux =
            yvex_deepseek_v4_ir_auxiliary_at(ir, i);
        rc |= yvex_cli_out_writef(
            fp,
            "mtp_%llu: layer=%llu attention=%s ratio=%llu router=%s previous_hidden_width=%llu shared_head=%s\n",
            aux->predictor_index, aux->layer.layer_index,
            yvex_deepseek_v4_attention_name(aux->layer.attention_class),
            aux->layer.compression_ratio,
            yvex_deepseek_v4_router_name(aux->layer.moe.router_class),
            aux->previous_hidden_width,
            aux->shares_output_head ? "true" : "false");
    }
    rc |= yvex_cli_out_writef(fp, "runtime_execution: unsupported\n");
    rc |= yvex_cli_out_writef(fp, "generation: unsupported\n");
    rc |= yvex_cli_out_writef(fp, "next_required_row: %s\n",
                              report->next_row);
    rc |= yvex_cli_out_writef(fp, "boundary: %s\n", report->boundary);
    return rc < 0 ? rc : 0;
}

/* Formats the machine-readable summary from already-decided typed fields. */
static int model_target_render_deepseek_json(
    FILE *fp,
    const yvex_model_target_report *report,
    const yvex_deepseek_v4_model_spec *model)
{
    return yvex_cli_out_writef(
        fp,
        "{\"status\":\"%s\",\"target_id\":\"%s\",\"repository\":\"%s\",\"revision\":\"%s\",\"layers\":%llu,\"mtp_layers\":%llu,\"hidden_size\":%llu,\"vocabulary_size\":%llu,\"context\":%llu,\"attention\":{\"swa\":%llu,\"csa\":%llu,\"hca\":%llu},\"routing\":{\"hash\":%llu,\"learned\":%llu},\"mhc_streams\":%llu,\"payload_bytes_read\":%llu,\"runtime\":\"unsupported\",\"generation\":\"unsupported\",\"next\":\"%s\"}\n",
        report->status, model->target_id, model->repository, model->revision,
        model->main_layer_count, model->auxiliary_layer_count,
        model->hidden_size, model->vocabulary_size, model->maximum_context,
        model->swa_layer_count, model->csa_layer_count,
        model->hca_layer_count, model->hash_router_layer_count,
        model->learned_router_layer_count,
        model->final_mhc.residual_streams, model->source_payload_bytes_read,
        report->next_row);
}

/* Selects presentation only; the model owner has already decided every fact. */
static int model_target_render_deepseek_ir(
    FILE *fp,
    yvex_model_target_render_mode mode,
    const yvex_model_target_report *report)
{
    const yvex_deepseek_v4_model_spec *model =
        yvex_deepseek_v4_ir_model(report->deepseek_architecture_ir);

    if (!model) return 0;
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        return model_target_render_deepseek_table(fp, report, model);
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        return model_target_render_deepseek_audit(
            fp, report, report->deepseek_architecture_ir, model);
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_JSON) {
        return model_target_render_deepseek_json(fp, report, model);
    }
    return model_target_render_deepseek_normal(
        fp, report, report->deepseek_architecture_ir, model);
}

/*
 * yvex_model_target_render()
 *
 * Purpose:
 *   render typed model-target report rows.
 *
 * Inputs:
 *   fp is an explicit output stream; mode selects normal/table/audit handling;
 *   report is borrowed.
 *
 * Effects:
 *   writes report rows through CLI IO helpers.
 *
 * Failure:
 *   returns CLI writer status.
 *
 * Boundary:
 *   rendering does not build reports, write sidecars, execute runtime paths, or
 *   create capability claims.
 */
int yvex_model_target_render(FILE *fp,
                             yvex_model_target_render_mode mode,
                             const yvex_model_target_report *report)
{
    int rc;

    if (!report) {
        return 0;
    }
    if (report->deepseek_gguf_map) {
        return model_target_render_deepseek_map(fp, mode, report);
    }
    if (report->deepseek_tensor_coverage) {
        return model_target_render_deepseek_coverage(fp, mode, report);
    }
    if (report->deepseek_architecture_ir) {
        return model_target_render_deepseek_ir(fp, mode, report);
    }
    if (mode == YVEX_MODEL_TARGET_OUTPUT_TABLE && report->table_row_count > 0) {
        rc = model_target_render_table_rows(fp, report);
        if (rc < 0) {
            return rc;
        }
    }
    return model_target_render_rows(fp, report->rows, report->row_count);
}

int yvex_model_target_render_errors(FILE *fp,
                                    const yvex_model_target_report *report)
{
    if (!report) {
        return 0;
    }
    return model_target_render_rows(fp, report->error_rows,
                                    report->error_row_count);
}

/*
 * yvex_model_target_render_help()
 *
 * Purpose:
 *   render model-target help as a typed report.
 *
 * Inputs:
 *   fp is an explicit output stream.
 *
 * Effects:
 *   builds the help report and writes it through CLI IO helpers.
 *
 * Failure:
 *   returns non-zero if help report construction fails.
 *
 * Boundary:
 *   help rendering describes report-only surfaces and does not create runtime
 *   or generation capability.
 */
int yvex_model_target_render_help(FILE *fp)
{
    yvex_model_target_report report;
    yvex_error err;
    int rc;

    yvex_error_clear(&err);
    rc = yvex_model_target_help_report_build(&report, &err);
    if (rc != YVEX_OK) {
        yvex_cli_out_writef(fp, "model-target help unavailable\n");
        return rc;
    }
    (void)yvex_model_target_render(fp, YVEX_MODEL_TARGET_OUTPUT_NORMAL, &report);
    yvex_model_target_report_close(&report);
    return 0;
}
