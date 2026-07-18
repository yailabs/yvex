/*
 * tensor_collection.c - tensor collection report builder.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   tensor collection counts, layer coverage facts, and report construction.
 *
 * Does not own:
 *   CLI parsing, rendering, tensor payload loading, tensor role mapping,
 *   artifact emission, runtime execution, generation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   collection reports classify header-visible tensor name groups only and do
 *   not promote lexical groups into runtime role support.
 *
 * Boundary:
 *   tensor collection inventory is not artifact emission, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "tensor_collection.h"

#include "private.h"
#include "src/model/families.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <yvex/native_weights.h>

typedef struct {
    char source_path[512];
    int source_present;
    unsigned long long tensors;
    unsigned long long embed;
    unsigned long long attn;
    unsigned long long mlp;
    unsigned long long norm;
    unsigned long long head;
    unsigned long long moe;
    unsigned long long layers;
} tensor_collection_scan;

static int collection_deepseek_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    yvex_source_verification verification;
    yvex_deepseek_tensor_coverage_failure failure;
    char models_root[512];
    char source_path[512];
    int rc;

    if (!yvex_model_target_release_source_paths(
            request, models_root, sizeof(models_root), source_path,
            sizeof(source_path))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "tensor_collection",
                       "DeepSeek source path exceeds report bounds");
        return YVEX_ERR_BOUNDS;
    }
    rc = yvex_model_register_deepseek_v4()->coverage.open_verified_source(
        &report->deepseek_tensor_coverage, &verification, source_path,
        models_root, &failure, err);
    if (rc != YVEX_OK) {
        report->status = "tensor-coverage-blocked";
        report->exit_code = 5;
        yvex_model_target_report_add_error(
            report,
            "model-target tensor-collection: DeepSeek coverage refused: %s tensor=%s",
            yvex_model_register_deepseek_v4()->coverage.failure_name(failure.code),
            failure.tensor_name[0] ? failure.tensor_name : "none");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    report->status = "exact-source-tensor-covered";
    (void)snprintf(report->target_id, sizeof(report->target_id), "%s",
                   request->target_id);
    (void)snprintf(report->family, sizeof(report->family), "%s", "deepseek");
    (void)snprintf(report->stage, sizeof(report->stage), "%s",
                   "header-only");
    (void)snprintf(report->tensor_map_status,
                   sizeof(report->tensor_map_status), "%s", "blocked");
    (void)snprintf(report->runtime_status, sizeof(report->runtime_status),
                   "%s", "unsupported");
    (void)snprintf(report->generation_status,
                   sizeof(report->generation_status), "%s", "unsupported");
    (void)snprintf(report->next_row, sizeof(report->next_row), "%s",
                   "V010.SOURCE.PAYLOAD.STREAM.0");
    (void)snprintf(report->boundary, sizeof(report->boundary), "%s",
                   "exact source coverage consumed by the canonical map; payload, artifact, runtime, and generation remain blocked");
    report->exit_code = 0;
    return YVEX_OK;
}

static int collection_validate(const yvex_model_target_request *request,
                               yvex_model_target_report *report)
{
    if (!request->target_id[0]) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-collection: requires TARGET");
        return 0;
    }
    if (!yvex_model_target_supported_source_target(request->target_id)) {
        report->exit_code = 2;
        yvex_model_target_report_add_error(report, "model-target tensor-collection: unsupported target: %s",
                                           request->target_id);
        return 0;
    }
    return 1;
}

static void collection_family_facts(const char *family,
                                    const char **top_blocker,
                                    const char **source_blocker)
{
    if (strcmp(family, "gemma") == 0) {
        *top_blocker = "missing-gemma-tensor-role-map";
        *source_blocker = "missing-gemma-source-path";
    } else {
        *top_blocker = "missing-qwen-tensor-role-map";
        *source_blocker = "missing-qwen-source-path";
    }
}

static int collection_is_dir(const char *path)
{
    struct stat st;

    return path && path[0] && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void collection_resolve_source(const yvex_model_target_request *request,
                                      const char *family,
                                      char *out,
                                      size_t cap)
{
    if (!out || cap == 0u) return;
    out[0] = '\0';
    if (request->source_path[0]) {
        (void)snprintf(out, cap, "%s", request->source_path);
        return;
    }
    if (request->models_root[0]) {
        (void)snprintf(out, cap, "%s/hf/%s/%s", request->models_root,
                       family, request->target_id);
    }
}

static int collection_name_has(const char *name, const char *needle)
{
    return name && needle && strstr(name, needle) != NULL;
}

static void collection_count_name(tensor_collection_scan *scan,
                                  const char *name)
{
    int is_attn = collection_name_has(name, "self_attn") ||
                  collection_name_has(name, "attention");
    int is_mlp = collection_name_has(name, ".mlp.") ||
                 collection_name_has(name, "feed_forward");

    if (collection_name_has(name, "embed_tokens")) {
        scan->embed = 1;
    }
    if (is_attn &&
        (collection_name_has(name, "q_proj") ||
         collection_name_has(name, "k_proj") ||
         collection_name_has(name, "v_proj") ||
         collection_name_has(name, "o_proj"))) {
        scan->attn++;
    }
    if (is_mlp &&
        (collection_name_has(name, "gate_proj") ||
         collection_name_has(name, "up_proj") ||
         collection_name_has(name, "down_proj") ||
         collection_name_has(name, "mlp.gate.weight") ||
         collection_name_has(name, "experts.gate_up_proj") ||
         collection_name_has(name, "shared_expert.down_proj"))) {
        scan->mlp++;
    }
    if (collection_name_has(name, "norm") ||
        collection_name_has(name, "layernorm")) {
        scan->norm++;
    }
    if (collection_name_has(name, "lm_head") ||
        collection_name_has(name, "output_head")) {
        scan->head++;
    }
    if (collection_name_has(name, "router") ||
        collection_name_has(name, "expert")) {
        scan->moe++;
    }
}

static void collection_scan_source(const yvex_model_target_request *request,
                                   const char *family,
                                   tensor_collection_scan *scan)
{
    yvex_native_weight_options options;
    yvex_native_weight_table *table = NULL;
    yvex_error err;
    unsigned long long i;

    memset(scan, 0, sizeof(*scan));
    collection_resolve_source(request, family, scan->source_path,
                              sizeof(scan->source_path));
    scan->source_present = collection_is_dir(scan->source_path);
    if (!scan->source_present) {
        return;
    }

    memset(&options, 0, sizeof(options));
    options.source_dir = scan->source_path;
    options.recursive = 1;
    yvex_error_clear(&err);
    if (yvex_native_weight_table_open(&table, &options, &err) != YVEX_OK) {
        return;
    }
    scan->tensors = yvex_native_weight_table_count(table);
    for (i = 0; i < scan->tensors; ++i) {
        const yvex_native_weight_info *info =
            yvex_native_weight_table_at(table, i);
        if (info) {
            collection_count_name(scan, info->name);
        }
    }
    yvex_native_weight_table_close(table);
    scan->layers = scan->attn >= 4 || scan->mlp >= 3 ? 1ull : 0ull;
}

static void collection_audit_common(yvex_model_target_report *report,
                                    const yvex_model_target_request *request,
                                    const char *family,
                                    const tensor_collection_scan *scan,
                                    const char *status)
{
    yvex_model_target_report_add_row(report, "tensor_collection_status: %s", status);
    yvex_model_target_report_add_row(report, "tensor_collection_family: %s", family);
    yvex_model_target_report_add_row(report, "tensor_collection_target_id: %s", request->target_id);
    yvex_model_target_report_add_row(report, "tensor_collection_stage: header-collection-inventory");
    yvex_model_target_report_add_row(report, "tensor_collection_evidence_basis: header-metadata-only");
    yvex_model_target_report_add_row(report, "tensor_collection_source_status: %s",
                                     scan->source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_collection_manifest_status: not-checked");
    yvex_model_target_report_add_row(report, "tensor_collection_config_status: %s",
                                     scan->source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_collection_tokenizer_status: %s",
                                     scan->source_present ? "present" : "missing");
    yvex_model_target_report_add_row(report, "tensor_collection_tensor_count: %llu", scan->tensors);
    yvex_model_target_report_add_row(report, "tensor_collection_layer_count_observed: %llu", scan->layers);
    yvex_model_target_report_add_row(report, "tensor_collection_embedding_status: candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_embedding_tensor_count: %llu", scan->embed);
    yvex_model_target_report_add_row(report, "tensor_collection_attention_status: candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_attention_q_count: %llu", scan->attn >= 1 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_attention_k_count: %llu", scan->attn >= 2 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_attention_v_count: %llu", scan->attn >= 3 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_attention_o_count: %llu", scan->attn >= 4 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_attention_complete_qkvo_layer_count: %llu", scan->attn >= 4 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_mlp_status: candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_mlp_gate_count: %llu", scan->mlp >= 1 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_mlp_up_count: %llu", scan->mlp >= 2 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_mlp_down_count: %llu", scan->mlp >= 3 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_mlp_complete_gud_layer_count: %llu", scan->mlp >= 3 ? 1ull : 0ull);
    yvex_model_target_report_add_row(report, "tensor_collection_norm_status: candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_norm_tensor_count: %llu", scan->norm);
    yvex_model_target_report_add_row(report, "tensor_collection_output_head_status: candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_output_head_tensor_count: %llu", scan->head);
    yvex_model_target_report_add_row(report, "tensor_collection_moe_status: %s",
                                     scan->moe == 0 ? "not-observed" : "candidate");
    yvex_model_target_report_add_row(report, "tensor_collection_moe_router_count: %llu", scan->moe);
    yvex_model_target_report_add_row(report, "tensor_collection_moe_expert_count: %llu", scan->moe);
    yvex_model_target_report_add_row(report, "tensor_collection_tokenizer_collection_status: sidecar-observed");
    yvex_model_target_report_add_row(report, "tensor_collection_kv_runtime_state_status: runtime-state-required-not-implemented");
    yvex_model_target_report_add_row(report, "tensor_collection_validation_status: lexical-and-header-only");
    yvex_model_target_report_add_row(report, "tensor_collection_role_mapping_status: not-implemented");
    yvex_model_target_report_add_row(report, "tensor_collection_runtime_descriptor_status: not-implemented");
    yvex_model_target_report_add_row(report, "tensor_collection_graph_consumer_status: not-implemented");
    yvex_model_target_report_common_tail(report);
    yvex_model_target_report_add_row(report, "next_required_rows: V010.MAP.8");
}

int yvex_tensor_collection_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err)
{
    const char *family;
    const char *top_blocker;
    const char *source_blocker;
    tensor_collection_scan scan;
    const char *status;

    if (!request || !report ||
        request->kind != YVEX_MODEL_TARGET_COMMAND_TENSOR_COLLECTION) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "tensor_collection",
                       "tensor collection report requires tensor-collection command kind");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!collection_validate(request, report)) {
        return YVEX_OK;
    }
    if (yvex_model_target_is_release_target(request->target_id)) {
        return collection_deepseek_build(request, report, err);
    }
    family = yvex_model_target_family_key(request->target_id);
    collection_family_facts(family, &top_blocker, &source_blocker);
    collection_scan_source(request, family, &scan);
    status = scan.source_present ? "collection-profiled" : "source-missing";
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_TABLE) {
        yvex_model_target_report_add_row(report, "TENSOR COLLECTION INVENTORY");
        yvex_model_target_report_add_row(report, "FAMILY  TARGET  STATUS  EMBED  ATTN_QKVO  MLP_GUD  NORM  HEAD  MOE  LAYERS  NEXT");
        yvex_model_target_report_add_row(report, "%s  %s  %s  %llu  %llu  %llu  %llu  %llu  %llu  %llu  V010.MAP.8",
                                         family, request->target_id, status,
                                         scan.embed, scan.attn >= 4 ? 1ull : 0ull,
                                         scan.mlp >= 3 ? 1ull : 0ull,
                                         scan.norm, scan.head, scan.moe,
                                         scan.layers);
        return YVEX_OK;
    }
    if (request->mode == YVEX_MODEL_TARGET_OUTPUT_AUDIT) {
        collection_audit_common(report, request, family, &scan, status);
        return YVEX_OK;
    }
    yvex_model_target_report_add_row(report, "tensor-collection: %s", family);
    yvex_model_target_report_add_row(report, "target: %s", request->target_id);
    yvex_model_target_report_add_row(report, "status: %s", status);
    yvex_model_target_report_add_row(report, "stage: header-collection-inventory");
    yvex_model_target_report_add_row(report, "evidence: header-metadata-only");
    yvex_model_target_report_add_row(report, "collections: embedding=%llu attention_qkvo=%llu mlp_gud=%llu norm=%llu head=%llu moe=%llu",
                                     scan.embed, scan.attn >= 4 ? 1ull : 0ull,
                                     scan.mlp >= 3 ? 1ull : 0ull, scan.norm,
                                     scan.head, scan.moe);
    yvex_model_target_report_add_row(report, "layers_observed: %llu", scan.layers);
    yvex_model_target_report_add_row(report, "top_blocker: %s",
                                     scan.source_present ? top_blocker : source_blocker);
    yvex_model_target_report_add_row(report, "next: V010.MAP.8");
    yvex_model_target_report_add_row(report, "boundary: tensor collection inventory only; no role mapping/runtime/generation");
    return YVEX_OK;
}
