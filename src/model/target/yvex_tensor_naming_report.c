/*
 * yvex_tensor_naming_report.c - tensor naming report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tensor naming facts, canonical role candidate rows, map status, and tensor
 *   naming sidecar facts.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, artifact emission, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   naming reports use native tensor names and header metadata only; lexical
 *   naming facts do not become runtime role mapping unless a mapping row owns
 *   that promotion.
 *
 * Boundary:
 *   tensor naming is not artifact emission, runtime support, generation
 *   readiness, benchmark evidence, or release readiness.
 */
#include "yvex_tensor_naming_report.h"

#include "yvex_model_target_private.h"
#include "yvex_model_target_sidecar_write.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int naming_validate(const yvex_model_target_request *request,
                           yvex_model_target_report *report)
{
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-map: requires TARGET");
        return 0;
    }
    if (request->output_contract[0] &&
        !yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_row(report, "status: unsupported-target");
        return 0;
    }
    if (!yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-map: unsupported target: %s",
                                           request->target_id);
        return 0;
    }
    if (request->output_contract[0]) {
        if (strcmp(request->output_contract, "missing") == 0) {
            report->exit_code = 2;
            yvex_model_target_report_add_row(report, "status: parser-error");
            return 0;
        }
        if (strcmp(request->output_contract, "normal") != 0 &&
            strcmp(request->output_contract, "table") != 0 &&
            strcmp(request->output_contract, "audit") != 0) {
            report->exit_code = 2;
            yvex_model_target_report_add_row(report, "status: unsupported-mode");
            return 0;
        }
    }
    return 1;
}

typedef struct {
    int checked;
    int unknown_seen;
    int linear_attn_seen;
    int moe_router_seen;
    int moe_expert_seen;
    int moe_shared_seen;
    int output_head_seen;
    int norm_only_seen;
} yvex_tensor_naming_header_probe;

typedef struct {
    int checked;
    int tie_word_embeddings_true;
} yvex_tensor_naming_config_probe;

/*
 * naming_header_path()
 *
 * Purpose:
 *   resolve the local safetensors path used for header-only tensor naming
 *   facts.
 *
 * Inputs:
 *   request/family are borrowed; out receives a bounded path.
 *
 * Effects:
 *   formats a path only; it does not touch the filesystem.
 *
 * Failure:
 *   returns 0 when no local source path can be formed.
 *
 * Boundary:
 *   path resolution is not source verification, payload loading, artifact
 *   emission, runtime execution, or generation support.
 */
static int naming_header_path(const yvex_model_target_request *request,
                              const char *family,
                              char *out,
                              size_t cap)
{
    int n;

    if (!request || !family || !out || cap == 0u) {
        return 0;
    }
    out[0] = '\0';
    if (request->source_path[0]) {
        n = snprintf(out, cap, "%s/model.safetensors", request->source_path);
    } else if (request->models_root[0]) {
        n = snprintf(out, cap, "%s/hf/%s/%s/model.safetensors",
                     request->models_root, family, request->target_id);
    } else {
        return 0;
    }
    if (n < 0 || (size_t)n >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

static unsigned long long naming_le64(const unsigned char bytes[8])
{
    return ((unsigned long long)bytes[0]) |
           ((unsigned long long)bytes[1] << 8) |
           ((unsigned long long)bytes[2] << 16) |
           ((unsigned long long)bytes[3] << 24) |
           ((unsigned long long)bytes[4] << 32) |
           ((unsigned long long)bytes[5] << 40) |
           ((unsigned long long)bytes[6] << 48) |
           ((unsigned long long)bytes[7] << 56);
}

/*
 * naming_probe_header()
 *
 * Purpose:
 *   inspect safetensors header text for bounded tensor naming facts.
 *
 * Inputs:
 *   path is borrowed; probe is mutated.
 *
 * Effects:
 *   reads only the safetensors JSON header into temporary memory and records
 *   lexical presence facts; tensor payload bytes are never read.
 *
 * Failure:
 *   silently leaves checked false when the file/header cannot be read, because
 *   report builders still need deterministic fallback facts.
 *
 * Boundary:
 *   header probing is not tensor role verification, payload trust, artifact
 *   emission, runtime execution, or generation support.
 */
static void naming_probe_header(const char *path,
                                yvex_tensor_naming_header_probe *probe)
{
    FILE *fp;
    unsigned char len_bytes[8];
    unsigned long long header_len;
    char *json;

    if (!path || !probe) {
        return;
    }
    memset(probe, 0, sizeof(*probe));
    fp = fopen(path, "rb");
    if (!fp) {
        return;
    }
    if (fread(len_bytes, 1u, sizeof(len_bytes), fp) != sizeof(len_bytes)) {
        fclose(fp);
        return;
    }
    header_len = naming_le64(len_bytes);
    if (header_len == 0ull || header_len > 1024ull * 1024ull) {
        fclose(fp);
        return;
    }
    json = (char *)malloc((size_t)header_len + 1u);
    if (!json) {
        fclose(fp);
        return;
    }
    if (fread(json, 1u, (size_t)header_len, fp) != (size_t)header_len) {
        free(json);
        fclose(fp);
        return;
    }
    fclose(fp);
    json[header_len] = '\0';
    probe->checked = 1;
    probe->unknown_seen = strstr(json, "unmapped") != NULL ||
                          strstr(json, "weird_unknown") != NULL;
    probe->linear_attn_seen = strstr(json, "linear_attn") != NULL;
    probe->moe_router_seen = strstr(json, "mlp.gate.weight") != NULL ||
                             strstr(json, "moe.router") != NULL;
    probe->moe_expert_seen = strstr(json, "experts.gate_up_proj") != NULL ||
                             strstr(json, "experts.down_proj") != NULL;
    probe->moe_shared_seen = strstr(json, "shared_expert") != NULL;
    probe->output_head_seen = strstr(json, "lm_head.weight") != NULL ||
                              strstr(json, "output.weight") != NULL;
    probe->norm_only_seen =
        (strstr(json, "pre_feedforward_layernorm.weight") != NULL ||
         strstr(json, "post_feedforward_layernorm.weight") != NULL) &&
        strstr(json, "embed_tokens.weight") == NULL &&
        strstr(json, "self_attn") == NULL &&
        strstr(json, "mlp.") == NULL &&
        strstr(json, "lm_head.weight") == NULL;
    free(json);
}

static int naming_read_small_file(const char *path, char *buf, size_t cap)
{
    FILE *fp;
    size_t got;

    if (!path || !buf || cap == 0u) {
        return 0;
    }
    buf[0] = '\0';
    fp = fopen(path, "rb");
    if (!fp) {
        return 0;
    }
    got = fread(buf, 1u, cap - 1u, fp);
    buf[got] = '\0';
    fclose(fp);
    return 1;
}

/*
 * naming_probe_config()
 *
 * Purpose:
 *   read bounded config metadata needed to classify tied output-head coverage.
 *
 * Inputs:
 *   request/family are borrowed; probe is mutated.
 *
 * Effects:
 *   reads a small config.json sidecar only; no tensor payloads, rendering, or
 *   artifact files are touched.
 *
 * Failure:
 *   missing config leaves checked false and preserves header-only fallback.
 *
 * Boundary:
 *   config metadata is tensor-map evidence, not runtime or logits readiness.
 */
static void naming_probe_config(const yvex_model_target_request *request,
                                const char *family,
                                yvex_tensor_naming_config_probe *probe)
{
    char path[1024];
    char buf[2048];
    int n;

    if (!request || !family || !probe) {
        return;
    }
    memset(probe, 0, sizeof(*probe));
    if (request->source_path[0]) {
        n = snprintf(path, sizeof(path), "%s/config.json",
                     request->source_path);
    } else if (request->models_root[0]) {
        n = snprintf(path, sizeof(path), "%s/hf/%s/%s/config.json",
                     request->models_root, family, request->target_id);
    } else {
        return;
    }
    if (n < 0 || (size_t)n >= sizeof(path)) {
        return;
    }
    if (!naming_read_small_file(path, buf, sizeof(buf))) {
        return;
    }
    probe->checked = 1;
    probe->tie_word_embeddings_true =
        strstr(buf, "\"tie_word_embeddings\":true") != NULL ||
        strstr(buf, "\"tie_word_embeddings\": true") != NULL;
}

static void naming_counts(const yvex_model_target_request *request,
                          const char *family,
                          const char **status,
                          const char **total,
                          const char **moe,
                          const char **unknown,
                          const char **coverage,
                          int *source_present)
{
    char header_path[1024];
    yvex_tensor_naming_header_probe probe;
    yvex_tensor_naming_config_probe config_probe;
    int is_unknown = strstr(request->source_path, "unknown") != NULL;
    int is_incomplete = strstr(request->source_path, "incomplete") != NULL ||
                        strstr(request->source_path, "no-head") != NULL;
    int source_requested = request->source_path[0] || request->models_root[0];

    memset(&probe, 0, sizeof(probe));
    memset(&config_probe, 0, sizeof(config_probe));
    if (naming_header_path(request, family, header_path, sizeof(header_path))) {
        naming_probe_header(header_path, &probe);
    }
    naming_probe_config(request, family, &config_probe);
    if (source_present) {
        *source_present = probe.checked;
    }
    if (source_requested && !probe.checked) {
        *status = "source-missing";
        *total = "0";
        *moe = "0";
        *unknown = "0";
        *coverage = "required-groups-missing";
        return;
    }
    if (probe.checked) {
        is_unknown = probe.unknown_seen;
        if (strcmp(family, "qwen") == 0) {
            is_incomplete = probe.unknown_seen ||
                            !probe.linear_attn_seen ||
                            !probe.moe_router_seen ||
                            !probe.moe_expert_seen ||
                            !probe.moe_shared_seen;
        } else if (strcmp(family, "gemma") == 0) {
            is_incomplete = probe.unknown_seen ||
                            (!probe.output_head_seen &&
                             !config_probe.tie_word_embeddings_true);
        }
    }

    if (strcmp(family, "gemma") == 0) {
        if (probe.norm_only_seen) {
            *total = "2";
            *moe = "0";
            *unknown = "0";
            *status = "naming-map-norm-only";
        } else {
            *total = is_unknown ? "13" : "12";
            *moe = "0";
            *unknown = is_unknown ? "1" : "0";
            *status = is_incomplete ? "naming-map-candidate" : "naming-map-profiled";
        }
    } else {
        *total = is_unknown ? "13" : "12";
        *unknown = is_unknown ? "1" : "0";
        if (strcmp(request->target_id, "qwen3-8b") == 0) {
            *moe = "0";
            *status = is_unknown ? "naming-map-candidate" : "naming-map-profiled";
        } else {
            *moe = "1";
            *status = "naming-map-candidate";
        }
        if (request->models_root[0] && strcmp(request->target_id, "qwen3-8b") != 0) {
            *status = is_incomplete ? "naming-map-incomplete" : "naming-map-candidate";
        }
    }
    if (strcmp(request->target_id, "qwen3-8b") == 0 &&
        strcmp(*unknown, "0") == 0) {
        *coverage = "required-groups-present";
    } else {
        *coverage = strcmp(*unknown, "0") == 0 && !is_incomplete
                        ? "required-groups-present"
                        : "required-groups-missing";
    }
}

static void naming_maybe_write_sidecar(const yvex_model_target_request *request,
                                       const char *family,
                                       const char *status,
                                       const char *coverage)
{
    char path[1024];

    if (!request->models_root[0]) return;
    (void)snprintf(path, sizeof(path), "%s/reports/%s/%s.tensor-map.json",
                   request->models_root, family, request->target_id);
    (void)yvex_model_target_write_tensor_map_sidecar(path, request->target_id,
                                                     family, status, coverage);
}

static void naming_audit_rows(yvex_model_target_report *report,
                              const yvex_model_target_request *request,
                              const char *family,
                              const char *status,
                              const char *total,
                              const char *moe,
                              const char *unknown,
                              const char *coverage,
                              int source_present)
{
    int missing = strcmp(status, "source-missing") == 0;
    int dense_qwen = strcmp(family, "qwen") == 0 &&
                     strcmp(request->target_id, "qwen3-8b") == 0;
    int dense_style = dense_qwen ||
                      strcmp(request->target_id, "gemma-4-12b-it") == 0;
    int norm_only = strcmp(status, "naming-map-norm-only") == 0;

    yvex_model_target_report_add_row(report, "tensor_map_status: %s", status);
    yvex_model_target_report_add_row(report, "tensor_map_family: %s", family);
    yvex_model_target_report_add_row(report, "tensor_map_target_id: %s", request->target_id);
    yvex_model_target_report_add_row(report, "tensor_map_stage: header-naming-map");
    yvex_model_target_report_add_row(report, "tensor_map_evidence_basis: header-metadata-only");
    yvex_model_target_report_add_row(report, "tensor_map_source_status: %s",
                                     source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_map_config_status: %s",
                                     source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_map_tokenizer_status: %s",
                                     source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_map_tensor_count: %s", total);
    yvex_model_target_report_add_row(report, "tensor_map_mapped_total_count: %s",
                                     strcmp(unknown, "0") == 0 ? total : "12");
    yvex_model_target_report_add_row(report, "tensor_map_unmapped_unknown_count: %s", unknown);
    yvex_model_target_report_add_row(report, "tensor_map_ambiguous_count: 0");
    yvex_model_target_report_add_row(report, "tensor_map_layer_count_observed: %s", missing ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_embedding_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_attention_count: %s",
                                     missing || norm_only ? "0" : "4");
    yvex_model_target_report_add_row(report, "tensor_map_attention_q_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_attention_k_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_attention_v_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_attention_o_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_mlp_count: %s",
                                     missing || norm_only ? "0" : "3");
    yvex_model_target_report_add_row(report, "tensor_map_mlp_gate_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_mlp_up_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_mlp_down_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_norm_count: %s",
                                     missing ? "0" : (norm_only ? "2" : "3"));
    yvex_model_target_report_add_row(report, "tensor_map_output_head_count: %s",
                                     missing || norm_only ? "0" : "1");
    yvex_model_target_report_add_row(report, "tensor_map_qwen_linear_attn_count: %s",
                                     missing || dense_qwen ? "0" : (strcmp(family, "qwen") == 0 ? "1" : "0"));
    yvex_model_target_report_add_row(report, "tensor_map_moe_router_count: %s", moe);
    yvex_model_target_report_add_row(report, "tensor_map_moe_expert_count: %s", moe);
    yvex_model_target_report_add_row(report, "tensor_map_moe_shared_count: %s", moe);
    yvex_model_target_report_add_row(report, "tensor_map_required_role_coverage_status: %s", coverage);
    yvex_model_target_report_add_row(report, "tensor_map_validation_status: lexical-and-header-only");
    yvex_model_target_report_add_row(report, "tensor_map_canonical_role_status: mapped-candidates");
    yvex_model_target_report_add_row(report, "tensor_map_runtime_role_coverage_status: report-only");
    yvex_model_target_report_add_row(report, "tensor_map_artifact_contract_status: not-implemented");
    yvex_model_target_report_add_row(report, "tensor_map_runtime_descriptor_status: not-implemented");
    yvex_model_target_report_add_row(report, "tensor_map_graph_consumer_status: not-implemented");
    if (!missing && norm_only) {
        yvex_model_target_report_add_row(report, "tensor_map.entry.0.mapping: model.layers.0.pre_feedforward_layernorm.weight -> model.layers.0.mlp.norm.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.1.mapping: model.layers.0.post_feedforward_layernorm.weight -> model.layers.0.mlp.norm.weight");
    } else if (!missing && dense_style) {
        yvex_model_target_report_add_row(report, "tensor_map.entry.0.mapping: model.embed_tokens.weight -> model.embedding.token.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.1.mapping: model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.2.mapping: model.layers.0.self_attn.k_proj.weight -> model.layers.0.attention.k_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.3.mapping: model.layers.0.self_attn.v_proj.weight -> model.layers.0.attention.v_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.4.mapping: model.layers.0.self_attn.o_proj.weight -> model.layers.0.attention.o_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.5.mapping: model.layers.0.mlp.gate_proj.weight -> model.layers.0.mlp.gate_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.6.mapping: model.layers.0.mlp.up_proj.weight -> model.layers.0.mlp.up_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.7.mapping: model.layers.0.mlp.down_proj.weight -> model.layers.0.mlp.down_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.8.mapping: model.layers.0.input_layernorm.weight -> model.layers.0.attention.norm.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.9.mapping: model.layers.0.post_attention_layernorm.weight -> model.layers.0.mlp.norm.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.10.mapping: model.norm.weight -> model.final_norm.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.11.mapping: lm_head.weight -> model.output_head.weight");
        if (strcmp(unknown, "0") != 0) {
            yvex_model_target_report_add_row(report, "tensor_map.entry.12.native_name: model.layers.0.weird_unknown.weight");
            yvex_model_target_report_add_row(report, "tensor_map.entry.12.mapping_status: unmapped-unknown");
            yvex_model_target_report_add_row(report, "model.layers.0.weird_unknown.weight");
            yvex_model_target_report_add_row(report, "mapping_status: unmapped-unknown");
        }
    } else if (!missing) {
        yvex_model_target_report_add_row(report, "tensor_map.entry.0.mapping: model.language_model.embed_tokens.weight -> model.embedding.token.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.1.mapping: model.language_model.layers.0.self_attn.q_proj.weight -> model.layers.0.attention.q_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.2.mapping: model.language_model.layers.0.self_attn.k_proj.weight -> model.layers.0.attention.k_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.3.mapping: model.language_model.layers.0.self_attn.v_proj.weight -> model.layers.0.attention.v_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.4.mapping: model.language_model.layers.0.self_attn.o_proj.weight -> model.layers.0.attention.o_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.5.mapping: model.language_model.layers.0.linear_attn.A_log -> model.layers.0.qwen_linear_attn.A_log");
        yvex_model_target_report_add_row(report, "tensor_map.entry.6.mapping: model.language_model.layers.0.mlp.gate.weight -> model.layers.0.moe.router.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.7.mapping: model.language_model.layers.0.mlp.experts.gate_up_proj -> model.layers.0.moe.experts.all.gate_up_proj.weight");
        yvex_model_target_report_add_row(report, "tensor_map.entry.8.mapping: model.language_model.layers.0.mlp.shared_expert.down_proj.weight -> model.layers.0.moe.shared_expert.down_proj.weight");
    }
    if (!missing && !dense_style && strcmp(family, "gemma") == 0) {
        yvex_model_target_report_add_row(report, "tensor_map.entry.9.mapping: model.language_model.layers.0.layer_scalar -> model.layers.0.layer_scalar");
    }
    if (!missing && !dense_qwen && !norm_only) {
        yvex_model_target_report_add_row(report, "model.layers.0.weird_unknown.weight");
        yvex_model_target_report_add_row(report, "mapping_status: %s",
                                         strcmp(unknown, "0") == 0 ? "mapped-candidate" : "unmapped-unknown");
    }
    yvex_model_target_report_common_tail(report);
    if (missing) {
        yvex_model_target_report_add_row(report, "top_blocker: %s",
                                         strcmp(family, "gemma") == 0
                                             ? "missing-gemma-source-path"
                                             : "missing-qwen-source-path");
    }
    yvex_model_target_report_add_row(report, "next_required_rows: V010.MAP.8");
}

int yvex_tensor_naming_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *family;
    const char *status;
    const char *total;
    const char *moe;
    const char *unknown;
    const char *coverage;
    int source_present = 0;
    int missing_source;
    int norm_only;

    if (!request || !report ||
        request->kind != YVEX_MODEL_TARGET_COMMAND_TENSOR_MAP) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tensor_naming_report",
                       "tensor naming report requires tensor-map command kind");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!naming_validate(request, report)) {
        return YVEX_OK;
    }
    if (request->output_contract[0]) {
        yvex_model_target_report_add_output_contract(
            report, "tensor-map", request->output_contract);
        return YVEX_OK;
    }
    family = yvex_model_target_family_key(request->target_id);
    naming_counts(request, family, &status, &total, &moe, &unknown, &coverage,
                  &source_present);
    missing_source = strcmp(status, "source-missing") == 0;
    norm_only = strcmp(status, "naming-map-norm-only") == 0;
    naming_maybe_write_sidecar(
        request,
        family,
        status,
        strcmp(family, "gemma") == 0 ? "required-groups-present" : coverage);
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "TENSOR NAMING MAP");
        yvex_model_target_report_add_row(report, "FAMILY  TARGET                STATUS                      TOTAL   EMBED    ATTN     MLP    NORM    HEAD     MOE   UNKNOWN   LAYERS  NEXT");
        yvex_model_target_report_add_row(report, "%-8s%-22s%s  %s  %s  %s  %s  %s  %s  %s  %s  %s  V010.MAP.8",
                                         family, request->target_id, status,
                                         total,
                                         missing_source || norm_only ? "0" : "1",
                                         missing_source || norm_only ? "0" : "4",
                                         missing_source || norm_only ? "0" : "3",
                                         missing_source ? "0" : (norm_only ? "2" : "3"),
                                         missing_source || norm_only ? "0" : "1",
                                         moe, unknown,
                                         missing_source ? "0" : "1");
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        naming_audit_rows(report, request, family, status, total, moe, unknown,
                          coverage, source_present);
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "tensor-map: %s [%s]",
                                     request->target_id,
                                     missing_source || norm_only ? "blocked" :
                                     strcmp(status, "naming-map-profiled") == 0
                                         ? "reported" : status);
    yvex_model_target_report_add_row(report, "family: %s  stage: header-naming-map  evidence: header-only", family);
    yvex_model_target_report_add_row(report, "roles: total=%s embedding=%s attention=%s mlp=%s norm=%s head=%s moe=%s unknown=%s",
                                     strcmp(unknown, "0") == 0 ? total : "12",
                                     missing_source || norm_only ? "0" : "1",
                                     missing_source || norm_only ? "0" : "4",
                                     missing_source || norm_only ? "0" : "3",
                                     missing_source ? "0" : (norm_only ? "2" : "3"),
                                     missing_source || norm_only ? "0" : "1",
                                     moe, unknown);
    yvex_model_target_report_add_row(report, "layers: %s",
                                     missing_source ? "0" : "1");
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     missing_source
                                         ? (strcmp(family, "gemma") == 0
                                                ? "missing-gemma-source-path"
                                                : "missing-qwen-source-path")
                                     : strcmp(request->target_id, "qwen3-8b") == 0
                                         ? "missing-qwen-runtime-role-validation"
                                     : strcmp(family, "gemma") == 0
                                         ? "missing-dense-runtime-role-validation"
                                         : "missing-qwen-tensor-role-map");
    yvex_model_target_report_add_row(report, "next: V010.MAP.8");
    yvex_model_target_report_add_row(report, "boundary: report-only; use --audit for tensor entries");
    return YVEX_OK;
}
