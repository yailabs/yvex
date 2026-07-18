/*
 * kv_report.c - typed KV report construction.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   KV report request evaluation, model-family fact scanning, source-only and
 *   unsupported-family report facts, minimal diagnostic ownership report facts,
 *   phase facts, blocker facts, and next-row facts.
 *
 * Does not own:
 *   adapter dispatch, input grammar, text rendering, stdout/stderr output,
 *   attention execution, decode, logits, sampling, generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   report builders populate typed facts only and use the lowest true stage.
 *
 * Boundary:
 *   KV reports are report-only unless explicitly describing a minimal
 *   session-owned diagnostic allocation.
 */
#include "kv_report.h"

#include "private.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *table;
    yvex_model_descriptor *model;
} yvex_kv_model_context;

typedef struct {
    unsigned long long tensor_count;
    unsigned long long total_tensor_bytes;
    int has_token_embedding;
    int has_attention_norm;
    int has_q;
    int has_k;
    int has_v;
    int has_o;
    int has_output_head;
} yvex_kv_role_scan;

static int kv_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

static int kv_contains_ci(const char *text, const char *needle)
{
    size_t needle_len;
    size_t i;
    size_t j;

    if (!text || !needle) {
        return 0;
    }
    needle_len = strlen(needle);
    if (needle_len == 0u) {
        return 1;
    }
    for (i = 0u; text[i] != '\0'; ++i) {
        for (j = 0u; j < needle_len; ++j) {
            unsigned char a = (unsigned char)text[i + j];
            unsigned char b = (unsigned char)needle[j];
            if (a == '\0' || (char)tolower(a) != (char)tolower(b)) {
                break;
            }
        }
        if (j == needle_len) {
            return 1;
        }
    }
    return 0;
}

static int kv_known_family(const char *family)
{
    return kv_streq(family, "auto") ||
           kv_streq(family, "deepseek") ||
           kv_streq(family, "glm") ||
           kv_streq(family, "qwen") ||
           kv_streq(family, "llama");
}

static const char *kv_family_from_text(const char *text)
{
    if (kv_contains_ci(text, "deepseek")) {
        return "deepseek";
    }
    if (kv_contains_ci(text, "glm")) {
        return "glm";
    }
    if (kv_contains_ci(text, "qwen")) {
        return "qwen";
    }
    if (kv_contains_ci(text, "llama")) {
        return "llama";
    }
    return "unknown";
}

static int kv_source_only_target(const char *model)
{
    return kv_contains_ci(model, "glm-5.2-official-safetensors");
}

static const char *kv_role_status(int present)
{
    return present ? "present" : "missing";
}

static const char *kv_attention_dependency_status(const yvex_kv_role_scan *scan)
{
    if (!scan || !scan->has_q || !scan->has_k || !scan->has_v) {
        return "blocked-missing-qkv";
    }
    return "blocked-runtime-integration";
}

static const char *kv_class_status_for_scan(const yvex_kv_role_scan *scan)
{
    if (scan && scan->has_q && scan->has_k && scan->has_v && scan->has_o) {
        return "complete";
    }
    return "partial";
}

static void kv_report_copy(char *dst,
                           size_t dst_size,
                           const char **field,
                           const char *value)
{
    if (!dst || dst_size == 0u || !field) {
        return;
    }
    (void)snprintf(dst, dst_size, "%s", value ? value : "");
    *field = dst;
}

static int kv_exit_for_status(int status)
{
    switch (status) {
    case YVEX_ERR_INVALID_ARG:
        return 2;
    case YVEX_ERR_IO:
        return 3;
    case YVEX_ERR_FORMAT:
    case YVEX_ERR_BOUNDS:
        return 4;
    case YVEX_ERR_UNSUPPORTED:
        return 5;
    default:
        return 1;
    }
}

static const char *kv_target_class_for_model(const char *model,
                                             const yvex_model_ref *ref)
{
    const char *text = model ? model : "";

    if (kv_source_only_target(text)) {
        return "official-source-huge-model";
    }
    if (kv_contains_ci(text, "selected-embed") ||
        (ref && ref->alias && kv_contains_ci(ref->alias, "selected-embed"))) {
        return "selected-runtime-slice";
    }
    return "candidate-GGUF-path";
}

static const char *kv_target_id_for_model(const yvex_kv_report_request *request,
                                          const yvex_model_ref *ref)
{
    if (ref && ref->alias && ref->alias[0]) {
        return ref->alias;
    }
    if (request && request->model && kv_source_only_target(request->model)) {
        return "glm-5.2-official-safetensors";
    }
    if (request && request->model &&
        kv_contains_ci(request->model, "selected-embed-rmsnorm")) {
        return "deepseek4-v4-flash-selected-embed-rmsnorm";
    }
    if (request && request->model &&
        kv_contains_ci(request->model, "selected-embed")) {
        return "deepseek4-v4-flash-selected-embed";
    }
    return "candidate-GGUF-path";
}

static void kv_model_context_close(yvex_kv_model_context *ctx)
{
    if (!ctx) {
        return;
    }
    yvex_model_descriptor_close(ctx->model);
    yvex_tensor_table_close(ctx->table);
    yvex_gguf_close(ctx->gguf);
    yvex_artifact_close(ctx->artifact);
    memset(ctx, 0, sizeof(*ctx));
}

static int kv_model_context_open(const char *path,
                                 yvex_kv_model_context *ctx,
                                 yvex_error *err)
{
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_artifact_options artifact_options;
    int rc;

    if (!path || !ctx) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv-report",
                       "model path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(ctx, 0, sizeof(*ctx));
    memset(&integrity_options, 0, sizeof(integrity_options));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(&artifact_options, 0, sizeof(artifact_options));

    artifact_options.path = path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&ctx->artifact, &artifact_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_gguf_open(&ctx->gguf, ctx->artifact, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_tensor_table_from_gguf(&ctx->table, ctx->gguf, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_model_descriptor_from_gguf(&ctx->model,
                                             ctx->gguf,
                                             ctx->table,
                                             err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_artifact_integrity_validate(ctx->artifact,
                                              ctx->gguf,
                                              ctx->table,
                                              &integrity_options,
                                              &integrity_report,
                                              err);
    }
    if (rc != YVEX_OK) {
        kv_model_context_close(ctx);
    }
    return rc;
}

static const char *kv_detect_family(const yvex_model_ref *ref,
                                    const yvex_kv_model_context *ctx,
                                    const char *input)
{
    const char *arch = NULL;

    if (ctx && ctx->model) {
        arch = yvex_arch_name(yvex_model_arch(ctx->model));
        if (arch && strcmp(arch, "unknown") != 0) {
            return arch;
        }
    }
    if (ref) {
        if (ref->family && ref->family[0]) {
            return ref->family;
        }
        if (ref->architecture && ref->architecture[0]) {
            return ref->architecture;
        }
        if (ref->alias) {
            arch = kv_family_from_text(ref->alias);
            if (strcmp(arch, "unknown") != 0) {
                return arch;
            }
        }
        if (ref->path) {
            arch = kv_family_from_text(ref->path);
            if (strcmp(arch, "unknown") != 0) {
                return arch;
            }
        }
    }
    return kv_family_from_text(input);
}

static void kv_scan_roles(const yvex_tensor_table *table, yvex_kv_role_scan *scan)
{
    unsigned long long i;

    memset(scan, 0, sizeof(*scan));
    if (!table) {
        return;
    }
    scan->tensor_count = yvex_tensor_table_count(table);
    for (i = 0ull; i < scan->tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(table, i);
        if (!tensor) {
            continue;
        }
        scan->total_tensor_bytes += tensor->storage_bytes;
        switch (tensor->role) {
        case YVEX_TENSOR_ROLE_TOKEN_EMBEDDING:
            scan->has_token_embedding = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_NORM:
            scan->has_attention_norm = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_Q:
            scan->has_q = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_K:
            scan->has_k = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_V:
            scan->has_v = 1;
            break;
        case YVEX_TENSOR_ROLE_ATTENTION_OUT:
            scan->has_o = 1;
            break;
        case YVEX_TENSOR_ROLE_OUTPUT_HEAD:
            scan->has_output_head = 1;
            break;
        default:
            break;
        }
    }
}

static unsigned long long kv_report_context_length(const yvex_kv_model_context *ctx)
{
    static const char *keys[] = {
        "llama.context_length",
        "deepseek.context_length",
        "qwen.context_length",
        "glm.context_length",
        "general.context_length"
    };
    unsigned long i;
    unsigned long long value_u64 = 0ull;

    if (!ctx) {
        return 0ull;
    }
    if (ctx->model) {
        value_u64 = yvex_model_context_length(ctx->model);
        if (value_u64 > 0ull) {
            return value_u64;
        }
    }
    for (i = 0u; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *value = yvex_gguf_metadata_find(ctx->gguf, keys[i]);
        if (value && yvex_gguf_value_as_u64(value, &value_u64) == YVEX_OK) {
            return value_u64;
        }
    }
    return 0ull;
}

static void kv_report_add_phase(yvex_kv_report *report,
                                const char *name,
                                const char *status)
{
    if (!report || report->phase_count >= 24u) {
        return;
    }
    report->phases[report->phase_count].name = name;
    report->phases[report->phase_count].status = status;
    report->phase_count += 1u;
}

static void kv_report_set_phases(yvex_kv_report *report,
                                 const char *default_status,
                                 const char *failed_phase)
{
    static const char *names[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "load-attention-class",
        "kv-profile",
        "kv-layout",
        "kv-shape",
        "kv-indexing",
        "kv-capacity",
        "kv-residency",
        "kv-context",
        "kv-readiness",
        "blocker-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned long i;
    unsigned long failed_index = sizeof(names) / sizeof(names[0]);

    if (!report) {
        return;
    }
    report->phase_count = 0u;
    if (failed_phase) {
        for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
            if (strcmp(names[i], failed_phase) == 0) {
                failed_index = i;
                break;
            }
        }
    }
    for (i = 0u; i < sizeof(names) / sizeof(names[0]); ++i) {
        const char *phase_status = default_status;
        if (!failed_phase && strcmp(names[i], "failed") == 0) {
            phase_status = "unknown";
        } else if (failed_phase) {
            if (i < failed_index) {
                phase_status = "pass";
            } else if (i == failed_index || strcmp(names[i], "failed") == 0) {
                phase_status = "failed";
            } else if (strcmp(names[i], "cleanup") == 0) {
                phase_status = "pass";
            } else {
                phase_status = "blocked";
            }
        }
        kv_report_add_phase(report, names[i], phase_status);
    }
}

static void kv_report_add_blocker(yvex_kv_report *report,
                                  const char *name,
                                  const char *status,
                                  const char *blocker_class)
{
    if (!report || report->blocker_count >= 32u) {
        return;
    }
    report->blockers[report->blocker_count].name = name;
    report->blockers[report->blocker_count].status = status;
    report->blockers[report->blocker_count].blocker_class = blocker_class;
    report->blocker_count += 1u;
}

static void kv_report_defaults(const yvex_kv_report_request *request,
                               yvex_kv_report *report)
{
    memset(report, 0, sizeof(*report));
    report->kind = request ? request->kind : YVEX_KV_REQUEST_REPORT;
    report->report_name = "kv";
    report->status = "kv-report";
    report->model = request && request->model ? request->model : "";
    report->model_resolved_path = "not-resolved";
    report->target_id = "candidate-GGUF-path";
    report->target_class = "unknown";
    report->backend = request && request->backend ? request->backend : "cpu";
    report->family = request && request->family ? request->family : "auto";
    report->family_detected = "unknown";
    report->family_requested = request && request->family ? request->family : "auto";
    report->family_runtime_status = "unknown";
    report->attention_class_status = "unknown";
    report->kv_class_status = "report-only";
    report->kv_stage = "report-only";
    report->kv_support_status = "report-only";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported-full-model";
    report->benchmark_status = "not-measured";
    report->diagnostic_kv_available = 1;
    report->diagnostic_kv_boundary = "segment-summary/minimal diagnostic KV";
    report->real_attention_kv = 0;
    report->real_attention_kv_write_ready = 0;
    report->real_attention_kv_read_ready = 0;
    report->decode_kv_consumer_ready = 0;
    report->kv_required = 1;
    report->kv_source = "attention-qkv-requirements";
    report->kv_layout = "planned";
    report->kv_layout_status = "planned";
    report->kv_dtype = "planned";
    report->kv_dtype_status = "planned";
    report->kv_layers = "unknown";
    report->kv_layers_status = "planned";
    report->kv_heads = "unknown";
    report->kv_heads_status = "planned";
    report->kv_head_dim = "unknown";
    report->kv_head_dim_status = "planned";
    report->kv_positions = "context-dependent";
    report->kv_capacity = "planned";
    report->kv_capacity_status = "planned";
    report->kv_indexing = "layer-head-position-token-order";
    report->context_length_source = "planned";
    report->attention_dependency_status = "blocked-missing-qkv";
    report->attention_q_required = 1;
    report->attention_k_required = 1;
    report->attention_v_required = 1;
    report->attention_q_status = "missing";
    report->attention_k_status = "missing";
    report->attention_v_status = "missing";
    report->attention_o_status = "missing";
    report->tensor_inventory_status = "not-performed";
    report->role_token_embedding_status = "missing";
    report->role_attention_norm_status = "missing";
    report->role_q_projection_status = "missing";
    report->role_k_projection_status = "missing";
    report->role_v_projection_status = "missing";
    report->role_o_projection_status = "missing";
    report->role_output_head_status = "missing";
    report->cleanup_attempted = 1;
    report->cleanup_status = "pass";
    report->next_required_rows =
        "ATTENTION.CLASS.0 complete,CONTEXT.CLASS.0,RUNTIME.KV.1,"
        "RUNTIME.KV.2,RUNTIME.KV.3,real-transformer-prefill,real-decode,"
        "real-output-head-logits,GEN.DEEPSEEK.0";
    report->exit_code = 0;
    report->include_attention = request ? request->include_attention : 0;
    report->include_context = request ? request->include_context : 0;
    report->include_residency = request ? request->include_residency : 0;
    report->include_blockers = request ? request->include_blockers : 0;
    report->top_blocker = "real attention-backed KV unsupported";
    report->source_artifact_class = "";
    report->target_artifact_class = "";
    report->reason = "";
    report->kv_layer_indexing = "planned";
    report->kv_head_indexing = "planned";
    report->kv_position_indexing = "planned";
    report->kv_token_order_policy = "prompt-order-then-append-order";
    report->kv_residency_class = "planned";
    report->kv_residency_status = "planned";
    report->kv_cpu_bytes_estimate = "unknown";
    report->kv_cuda_bytes_estimate = "unknown";
    report->kv_host_staged_bytes_estimate = "unknown";
    report->kv_ssd_staged_status = "planned";
    report->kv_ssd_streamed_status = "planned";
    report->kv_paged_status = "planned";
    report->kv_chunked_status = "planned";
    report->kv_quantized_status = "planned";
    report->kv_managed_memory_status = "planned";
    report->context_required = "true";
    report->requested_context = "not-requested";
    report->context_capacity_status = "planned";
    report->context_overflow_policy = "planned-refusal-before-mutation";
    report->attention_runtime_ready = "false";
    report->full_transformer_attention_ready = "false";
    report->prefill_kv_write_required = "true";
    report->prefill_kv_write_ready = "false";
    report->decode_kv_read_required = "true";
    report->decode_kv_read_ready = "false";
    report->qkv_role_coverage = "missing";
}

static void kv_report_apply_scan(yvex_kv_report *report,
                                 const yvex_kv_role_scan *scan,
                                 unsigned long long max_context)
{
    int has_qkv;

    if (!report || !scan) {
        return;
    }
    has_qkv = scan->has_q && scan->has_k && scan->has_v;
    report->kv_class_status = kv_class_status_for_scan(scan);
    report->family_runtime_status = report->kv_class_status;
    report->attention_class_status = report->kv_class_status;
    report->context_length_source = max_context > 0ull ? "model-metadata" : "planned";
    report->max_context = max_context;
    report->max_context_seen = max_context > 0ull;
    report->attention_dependency_status = kv_attention_dependency_status(scan);
    report->attention_q_status = kv_role_status(scan->has_q);
    report->attention_k_status = kv_role_status(scan->has_k);
    report->attention_v_status = kv_role_status(scan->has_v);
    report->attention_o_status = kv_role_status(scan->has_o);
    report->tensor_inventory_status = "loaded-gguf-directory";
    report->tensor_count = scan->tensor_count;
    report->tensor_bytes = scan->total_tensor_bytes;
    report->role_token_embedding_status = kv_role_status(scan->has_token_embedding);
    report->role_attention_norm_status = kv_role_status(scan->has_attention_norm);
    report->role_q_projection_status = kv_role_status(scan->has_q);
    report->role_k_projection_status = kv_role_status(scan->has_k);
    report->role_v_projection_status = kv_role_status(scan->has_v);
    report->role_o_projection_status = kv_role_status(scan->has_o);
    report->role_output_head_status = kv_role_status(scan->has_output_head);
    report->qkv_role_coverage = has_qkv ? "present" : "missing";
    report->blocker_count = 0u;
    if (!scan->has_q) {
        kv_report_add_blocker(report, "q projection tensor", "missing",
                              "missing-attention-q");
    }
    if (!scan->has_k) {
        kv_report_add_blocker(report, "k projection tensor", "missing",
                              "missing-attention-k");
    }
    if (!scan->has_v) {
        kv_report_add_blocker(report, "v projection tensor", "missing",
                              "missing-attention-v");
    }
    kv_report_add_blocker(report, "real attention-backed KV writes",
                          "unsupported", "missing-real-kv-path");
    kv_report_add_blocker(report, "decode KV consumer",
                          "unsupported", "missing-decode-consumer");
    kv_report_add_blocker(report, "KV capacity estimator",
                          "planned", "capacity-estimator-pending");
}

static void kv_report_set_unsupported_family(yvex_kv_report *report,
                                             const yvex_kv_report_request *request,
                                             const char *detected,
                                             const char *reason)
{
    kv_report_defaults(request, report);
    report->status = "kv-report-unsupported";
    report->family = request && request->family ? request->family : "unknown";
    report->family_detected = detected ? detected : "unknown";
    report->family_runtime_status = "unsupported";
    report->attention_class_status = "unsupported";
    report->kv_class_status = "unsupported";
    report->kv_required = 0;
    report->kv_source = "unsupported-family";
    report->kv_layout = "unsupported";
    report->kv_layout_status = "unsupported";
    report->kv_dtype = "unsupported";
    report->kv_dtype_status = "unsupported";
    report->kv_layers_status = "unsupported";
    report->kv_heads_status = "unsupported";
    report->kv_head_dim_status = "unsupported";
    report->kv_positions = "unknown";
    report->kv_capacity = "unsupported";
    report->kv_capacity_status = "unsupported";
    report->kv_indexing = "unsupported";
    report->context_length_source = "unsupported-family";
    report->attention_dependency_status = "unsupported-family";
    report->attention_q_required = 0;
    report->attention_k_required = 0;
    report->attention_v_required = 0;
    report->prefill_kv_write_required = "unknown";
    report->decode_kv_read_required = "unknown";
    report->top_blocker = reason ? reason : "unsupported family";
    kv_report_copy(report->reason_storage, sizeof(report->reason_storage),
                   &report->reason, report->top_blocker);
    report->exit_code = 5;
    report->blocker_count = 0u;
    kv_report_add_blocker(report, report->top_blocker, "blocked",
                          "unsupported-family");
    kv_report_set_phases(report, "unsupported", "resolve-family");
}

static void kv_report_set_source_only(yvex_kv_report *report,
                                      const yvex_kv_report_request *request)
{
    const char *family = request && kv_streq(request->family, "auto")
                             ? "glm"
                             : request ? request->family : "glm";

    kv_report_defaults(request, report);
    report->status = "kv-report-unsupported";
    report->model_resolved_path = "not-resolved-source-only-target";
    report->target_id = "glm-5.2-official-safetensors";
    report->target_class = "official-source-huge-model";
    report->family = family ? family : "glm";
    report->family_detected = "glm";
    report->family_runtime_status = "unsupported";
    report->attention_class_status = "unsupported";
    report->kv_class_status = "unsupported";
    report->tensor_inventory_status = "not-performed-source-only-target";
    report->source_artifact_class = "official safetensors";
    report->target_artifact_class = "future YVEX-produced GGUF";
    report->kv_source = "planned-family-mapping";
    report->kv_positions = "planned";
    report->kv_indexing = "planned";
    report->kv_token_order_policy = "planned";
    report->context_length_source = "planned-source-manifest";
    report->attention_dependency_status = "unsupported-source-only";
    report->top_blocker =
        "source-only target has no YVEX-produced GGUF tensor inventory";
    report->exit_code = 5;
    report->blocker_count = 0u;
    kv_report_add_blocker(report, report->top_blocker, "blocked",
                          "source-payload-not-loaded");
    kv_report_add_blocker(report, "GLM KV mapping", "planned",
                          "unsupported-generation-family");
    kv_report_add_blocker(report, "real attention-backed KV writes",
                          "unsupported", "missing-real-kv-path");
    kv_report_set_phases(report, "planned", "load-family-runtime");
}

/*
 * yvex_kv_report_build()
 *
 * Purpose:
 *   build a typed KV class report from model, family, backend, and registry
 *   request facts.
 *
 * Inputs:
 *   request and report are caller-owned; err is optional diagnostic storage.
 *
 * Effects:
 *   may open and close a local model artifact context to inspect metadata and
 *   tensor roles; writes no operator output and allocates no persistent state.
 *
 * Failure:
 *   returns precise YVEX status and leaves report populated when a reportable
 *   boundary exists.
 *
 * Boundary:
 *   this is report construction only, not runtime KV execution.
 */
int yvex_kv_report_build(const yvex_kv_report_request *request,
                         yvex_kv_report *report,
                         yvex_error *err)
{
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_kv_model_context ctx;
    yvex_kv_role_scan scan;
    const char *detected_family = "unknown";
    const char *family = "unknown";
    unsigned long long max_context = 0ull;
    int rc;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv-report",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    kv_report_defaults(request, report);
    if (!request->model) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv-report",
                       "yvex: kv report requires model");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!request->backend ||
        (!kv_streq(request->backend, "cpu") &&
         !kv_streq(request->backend, "cuda"))) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv-report",
                       "yvex: kv report backend must be cpu or cuda");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!kv_known_family(request->family)) {
        kv_report_set_unsupported_family(
            report,
            request,
            "unknown",
            "unknown family requested; KV class report is not generic model support");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (kv_source_only_target(request->model)) {
        kv_report_set_source_only(report, request);
        yvex_error_clear(err);
        return YVEX_OK;
    }

    memset(&ref_options, 0, sizeof(ref_options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&scan, 0, sizeof(scan));
    ref_options.allow_registry = 1;
    ref_options.registry_path = request->registry_path;
    rc = yvex_model_ref_resolve(&ref, request->model, &ref_options, err);
    if (rc != YVEX_OK) {
        report->status = "fail";
        report->family = kv_streq(request->family, "auto") ?
            "unknown" : request->family;
        report->top_blocker = yvex_error_message(err);
        report->exit_code = kv_exit_for_status(rc);
        kv_report_set_phases(report, "failed", "resolve-model");
        yvex_model_ref_clear(&ref);
        return rc;
    }
    report->model_resolved_path = ref.path ? ref.path : "not-resolved";
    kv_report_copy(report->model_resolved_path_storage,
                   sizeof(report->model_resolved_path_storage),
                   &report->model_resolved_path,
                   report->model_resolved_path);
    report->target_id = kv_target_id_for_model(request, &ref);
    kv_report_copy(report->target_id_storage, sizeof(report->target_id_storage),
                   &report->target_id, report->target_id);
    report->target_class = kv_target_class_for_model(request->model, &ref);

    rc = kv_model_context_open(ref.path, &ctx, err);
    if (rc != YVEX_OK) {
        report->status = "fail";
        report->family = kv_streq(request->family, "auto") ?
            "unknown" : request->family;
        report->top_blocker = yvex_error_message(err);
        kv_report_copy(report->reason_storage, sizeof(report->reason_storage),
                       &report->reason, report->top_blocker);
        report->tensor_inventory_status = "failed";
        report->exit_code = kv_exit_for_status(rc);
        kv_report_set_phases(report, "failed", "kv-profile");
        yvex_model_ref_clear(&ref);
        return rc;
    }

    detected_family = kv_detect_family(&ref, &ctx, request->model);
    family = kv_streq(request->family, "auto") ? detected_family : request->family;
    if (!kv_streq(request->family, "auto") &&
        !kv_streq(request->family, detected_family)) {
        kv_model_context_close(&ctx);
        yvex_model_ref_clear(&ref);
        kv_report_set_unsupported_family(
            report,
            request,
            detected_family,
            "requested family does not match resolved model family");
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!kv_streq(family, "deepseek")) {
        kv_model_context_close(&ctx);
        yvex_model_ref_clear(&ref);
        kv_report_set_unsupported_family(
            report,
            request,
            detected_family,
            "KV class report is currently implemented for DeepSeek-family GGUF artifacts only");
        yvex_error_clear(err);
        return YVEX_OK;
    }

    kv_scan_roles(ctx.table, &scan);
    max_context = kv_report_context_length(&ctx);
    report->status = "kv-report";
    report->family = family;
    report->family_detected = detected_family;
    kv_report_copy(report->family_storage, sizeof(report->family_storage),
                   &report->family, report->family);
    kv_report_copy(report->family_detected_storage,
                   sizeof(report->family_detected_storage),
                   &report->family_detected,
                   report->family_detected);
    kv_report_apply_scan(report, &scan, max_context);
    kv_report_set_phases(report, "pass", NULL);

    kv_model_context_close(&ctx);
    yvex_model_ref_clear(&ref);
    yvex_error_clear(err);
    return YVEX_OK;
}

/*
 * yvex_kv_ownership_report_build()
 *
 * Purpose:
 *   build a typed report for the minimal session-owned diagnostic KV
 *   allocation path.
 *
 * Inputs:
 *   request carries shape and optional append/read demo settings; report is
 *   caller-owned output; err is optional diagnostic storage.
 *
 * Effects:
 *   allocates a bounded temporary KV cache, may append/read synthetic values,
 *   captures summary facts, and always closes temporary KV state before
 *   returning.
 *
 * Failure:
 *   returns precise YVEX status for invalid shape, allocation failure, bounds
 *   refusal, or read failure.
 *
 * Boundary:
 *   the allocation is minimal diagnostic KV only, not attention-backed KV.
 */
int yvex_kv_ownership_report_build(const yvex_kv_report_request *request,
                                   yvex_kv_report *report,
                                   yvex_error *err)
{
    yvex_kv_cache *kv = NULL;
    float *append_values = NULL;
    float *read_values = NULL;
    unsigned long long value_count = 0ull;
    unsigned long long append_target = 0ull;
    unsigned long long appended_position = 0ull;
    unsigned long long sample_count = 0ull;
    unsigned long long i;
    int rc;

    if (!request || !report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "kv-ownership",
                       "request and report are required");
        return YVEX_ERR_INVALID_ARG;
    }
    kv_report_defaults(request, report);
    report->kind = YVEX_KV_REQUEST_OWNERSHIP;
    report->status = "kv-owned";
    report->generation = "unsupported";
    report->exit_code = 0;

    rc = yvex_kv_cache_create_shape(&kv, &request->shape, err);
    if (rc != YVEX_OK) {
        report->status = "kv-fail";
        report->exit_code = kv_exit_for_status(rc);
        return rc;
    }

    value_count = yvex_kv_cache_position_value_count(kv);
    if (request->append_demo) {
        append_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!append_values) {
            yvex_kv_cache_close(kv);
            yvex_error_set(err, YVEX_ERR_NOMEM, "kv-ownership",
                           "failed to allocate KV append demo buffer");
            report->status = "kv-fail";
            report->exit_code = 4;
            return YVEX_ERR_NOMEM;
        }
        append_target = request->shape.capacity > 1ull ? 2ull : 1ull;
        for (i = 0ull; i < append_target; ++i) {
            yvex_kv_fill_demo_values(append_values, value_count, i);
            rc = yvex_kv_cache_append_position_f32(kv,
                                                   append_values,
                                                   value_count,
                                                   &appended_position,
                                                   err);
            if (rc != YVEX_OK) {
                free(append_values);
                yvex_kv_cache_close(kv);
                report->status = "kv-fail";
                report->exit_code = kv_exit_for_status(rc);
                return rc;
            }
        }
    }

    if (request->read_requested) {
        read_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!read_values) {
            free(append_values);
            yvex_kv_cache_close(kv);
            yvex_error_set(err, YVEX_ERR_NOMEM, "kv-ownership",
                           "failed to allocate KV read buffer");
            report->status = "kv-fail";
            report->exit_code = 4;
            return YVEX_ERR_NOMEM;
        }
        rc = yvex_kv_cache_read_position_f32(kv,
                                             request->read_position,
                                             read_values,
                                             value_count,
                                             err);
        if (rc != YVEX_OK) {
            free(read_values);
            free(append_values);
            yvex_kv_cache_close(kv);
            report->status = "kv-fail";
            report->exit_code = kv_exit_for_status(rc);
            return rc;
        }
        report->read_checksum = yvex_kv_checksum_values(read_values, value_count);
    }

    rc = yvex_kv_cache_get_summary(kv, &report->ownership_summary, err);
    if (rc != YVEX_OK) {
        free(read_values);
        free(append_values);
        yvex_kv_cache_close(kv);
        report->status = "kv-fail";
        report->exit_code = kv_exit_for_status(rc);
        return rc;
    }

    report->kv_created = 1;
    report->session_owned = report->ownership_summary.session_owned;
    report->last_appended_position = appended_position;
    report->read_requested = request->read_requested;
    report->read_position = request->read_position;
    if (request->read_requested) {
        sample_count = value_count < 8ull ? value_count : 8ull;
        report->read_value_count = value_count;
        report->read_sample_count = sample_count;
        for (i = 0ull; i < sample_count; ++i) {
            report->read_sample_values[i] = read_values[i];
        }
    }

    yvex_kv_cache_close(kv);
    report->cleanup_attempted = 1;
    report->cleanup_status = "pass";

    free(read_values);
    free(append_values);
    yvex_error_clear(err);
    return YVEX_OK;
}
