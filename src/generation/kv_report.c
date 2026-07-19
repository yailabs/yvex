/* Owner: src/generation
 * Owns: KV report request evaluation, model-family fact scanning, source-only and unsupported-family report facts,
 *   minimal diagnostic ownership report facts, phase facts, blocker facts, and next-row facts.
 * Does not own: adapter dispatch, input grammar, text rendering, stdout/stderr output, attention execution, decode,
 *   logits, sampling, generation, eval, benchmark, or release decisions.
 * Invariants: report builders populate typed facts only and use the lowest true stage.
 * Boundary: KV reports are report-only unless explicitly describing a minimal session-owned diagnostic allocation.
 * Purpose: Construct typed KV capability and ownership reports from canonical model facts.
 * Inputs: A report request, immutable model/source facts, and caller-owned report storage.
 * Effects: Allocates only temporary requirement coverage and releases it before return.
 * Failure: Missing identity, coverage, or allocation returns a typed lowest-stage refusal. */
#include <yvex/internal/generation.h>

#include <yvex/artifact.h>

#include <ctype.h>
#include <stddef.h>
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

typedef struct {
    const char *name;
    yvex_tensor_role role;
    size_t scan_offset;
    size_t report_offset;
    const char *missing_blocker;
} kv_family_role;

#define KV_ROLE(name_, role_, scan_, report_, blocker_)                                    \
    {                                                                                      \
        name_, role_, offsetof(yvex_kv_role_scan, scan_),                                  \
            offsetof(yvex_kv_report, report_), blocker_                                    \
    }

static const kv_family_role kv_family_roles[] = {
    KV_ROLE("token embedding", YVEX_TENSOR_ROLE_TOKEN_EMBEDDING,
            has_token_embedding, role_token_embedding_status, NULL),
    KV_ROLE("attention norm", YVEX_TENSOR_ROLE_ATTENTION_NORM,
            has_attention_norm, role_attention_norm_status, NULL),
    KV_ROLE("q projection tensor", YVEX_TENSOR_ROLE_ATTENTION_Q, has_q,
            role_q_projection_status, "missing-attention-q"),
    KV_ROLE("k projection tensor", YVEX_TENSOR_ROLE_ATTENTION_K, has_k,
            role_k_projection_status, "missing-attention-k"),
    KV_ROLE("v projection tensor", YVEX_TENSOR_ROLE_ATTENTION_V, has_v,
            role_v_projection_status, "missing-attention-v"),
    KV_ROLE("o projection", YVEX_TENSOR_ROLE_ATTENTION_OUT, has_o,
            role_o_projection_status, NULL),
    KV_ROLE("output head", YVEX_TENSOR_ROLE_OUTPUT_HEAD, has_output_head,
            role_output_head_status, NULL),
};

#undef KV_ROLE

static const char *const kv_family_names[] = {"deepseek", "glm", "qwen", "llama"};

static const char *const kv_context_keys[] = {
    "llama.context_length",
    "deepseek.context_length",
    "qwen.context_length",
    "glm.context_length",
    "general.context_length",
};

static const char *const kv_phase_names[] = {
    "preflight",       "resolve-model", "resolve-family", "load-family-runtime",
    "load-attention-class", "kv-profile",    "kv-layout",      "kv-shape",
    "kv-indexing",     "kv-capacity",   "kv-residency",   "kv-context",
    "kv-readiness",    "blocker-report", "complete",        "failed",
    "cleanup",
};

/* Purpose: Implement the canonical kv streq mechanism owned by the generation boundary. */
static int kv_streq(const char *a, const char *b)
{
    return a && b && strcmp(a, b) == 0;
}

/* Purpose: Implement the canonical kv contains ci mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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

/* Purpose: Implement the canonical kv known family mechanism owned by the generation boundary. */
static int kv_known_family(const char *family)
{
    size_t index;

    if (kv_streq(family, "auto")) {
        return 1;
    }
    for (index = 0; index < sizeof(kv_family_names) / sizeof(kv_family_names[0]); ++index) {
        if (kv_streq(family, kv_family_names[index])) {
            return 1;
        }
    }
    return 0;
}

/* Purpose: Implement the canonical kv family from text mechanism owned by the generation boundary. */
static const char *kv_family_from_text(const char *text)
{
    size_t index;

    for (index = 0; index < sizeof(kv_family_names) / sizeof(kv_family_names[0]); ++index) {
        if (kv_contains_ci(text, kv_family_names[index])) {
            return kv_family_names[index];
        }
    }
    return "unknown";
}

/* Purpose: Implement the canonical kv source only target mechanism owned by the generation boundary. */
static int kv_source_only_target(const char *model)
{
    return kv_contains_ci(model, "glm-5.2-official-safetensors");
}

/* Purpose: Implement the canonical kv role status mechanism owned by the generation boundary. */
static const char *kv_role_status(int present)
{
    return present ? "present" : "missing";
}

/* Purpose: Compute the bounded kv attention dependency status primitive under the declared dtype and shape contract. */
static const char *kv_attention_dependency_status(const yvex_kv_role_scan *scan)
{
    if (!scan || !scan->has_q || !scan->has_k || !scan->has_v) {
        return "blocked-missing-qkv";
    }
    return "blocked-runtime-integration";
}

/* Purpose: Implement the canonical kv class status for scan mechanism owned by the generation boundary. */
static const char *kv_class_status_for_scan(const yvex_kv_role_scan *scan)
{
    if (scan && scan->has_q && scan->has_k && scan->has_v && scan->has_o) {
        return "complete";
    }
    return "partial";
}

/* Purpose: Copy kv report copy between compatible admitted ranges without changing semantic identity. */
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

/* Purpose: Implement the canonical kv target class for model mechanism owned by the generation boundary. */
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

/* Purpose: Implement the canonical kv target id for model mechanism owned by the generation boundary. */
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

/* Purpose: Release the resources owned by kv model context close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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

/* Purpose: Construct the admitted kv model context open state only after its identities and resources are valid.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static int kv_model_context_open(const char *path,
                                 yvex_kv_model_context *ctx,
                                 yvex_error *err)
{
    yvex_artifact_integrity_options integrity_options;
    yvex_artifact_integrity_report integrity_report;
    yvex_artifact_options artifact_options;
    int rc;

    if (!path || !ctx) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "kv-report", "model path is required");
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

/* Purpose: Implement the canonical kv detect family mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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

/* Purpose: Implement the canonical kv scan roles mechanism owned by the generation boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
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
        for (size_t role = 0; role < sizeof(kv_family_roles) / sizeof(kv_family_roles[0]);
             ++role) {
            if (tensor->role == kv_family_roles[role].role) {
                int *present = (int *)((unsigned char *)scan +
                                       kv_family_roles[role].scan_offset);

                *present = 1;
                break;
            }
        }
    }
}

/* Purpose: Project immutable kv report context length facts into the typed reporting surface.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static unsigned long long kv_report_context_length(const yvex_kv_model_context *ctx)
{
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
    for (i = 0u; i < sizeof(kv_context_keys) / sizeof(kv_context_keys[0]); ++i) {
        const yvex_gguf_value *value =
            yvex_gguf_metadata_find(ctx->gguf, kv_context_keys[i]);
        if (value && yvex_gguf_value_as_u64(value, &value_u64) == YVEX_OK) {
            return value_u64;
        }
    }
    return 0ull;
}

/* Purpose: Project immutable kv report add phase facts into the typed reporting surface. */
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

/* Purpose: Publish kv report set phases only within its admitted destination range.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void kv_report_set_phases(yvex_kv_report *report,
                                 const char *default_status,
                                 const char *failed_phase)
{
    unsigned long i;
    unsigned long failed_index = sizeof(kv_phase_names) / sizeof(kv_phase_names[0]);

    if (!report) {
        return;
    }
    report->phase_count = 0u;
    if (failed_phase) {
        for (i = 0u; i < sizeof(kv_phase_names) / sizeof(kv_phase_names[0]); ++i) {
            if (strcmp(kv_phase_names[i], failed_phase) == 0) {
                failed_index = i;
                break;
            }
        }
    }
    for (i = 0u; i < sizeof(kv_phase_names) / sizeof(kv_phase_names[0]); ++i) {
        const char *phase_status = default_status;
        if (!failed_phase && strcmp(kv_phase_names[i], "failed") == 0) {
            phase_status = "unknown";
        } else if (failed_phase) {
            if (i < failed_index) {
                phase_status = "pass";
            } else if (i == failed_index || strcmp(kv_phase_names[i], "failed") == 0) {
                phase_status = "failed";
            } else if (strcmp(kv_phase_names[i], "cleanup") == 0) {
                phase_status = "pass";
            } else {
                phase_status = "blocked";
            }
        }
        kv_report_add_phase(report, kv_phase_names[i], phase_status);
    }
}

/* Purpose: Project immutable kv report add blocker facts into the typed reporting surface. */
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

/* Declarative refusal state keeps the KV capability boundary in one immutable fact table. */
static const yvex_kv_report kv_report_default = {
    .kind = YVEX_KV_REQUEST_REPORT,
    .report_name = "kv",
    .status = "kv-report",
    .model = "",
    .model_resolved_path = "not-resolved",
    .target_id = "candidate-GGUF-path",
    .target_class = "unknown",
    .backend = "cpu",
    .family = "auto",
    .family_detected = "unknown",
    .family_requested = "auto",
    .family_runtime_status = "unknown",
    .attention_class_status = "unknown",
    .kv_class_status = "report-only",
    .kv_stage = "report-only",
    .kv_support_status = "report-only",
    .runtime_claim = "unsupported",
    .generation = "unsupported-full-model",
    .benchmark_status = "not-measured",
    .diagnostic_kv_available = 1,
    .diagnostic_kv_boundary = "segment-summary/minimal diagnostic KV",
    .kv_required = 1,
    .kv_source = "attention-qkv-requirements",
    .kv_layout = "planned",
    .kv_layout_status = "planned",
    .kv_dtype = "planned",
    .kv_dtype_status = "planned",
    .kv_layers = "unknown",
    .kv_layers_status = "planned",
    .kv_heads = "unknown",
    .kv_heads_status = "planned",
    .kv_head_dim = "unknown",
    .kv_head_dim_status = "planned",
    .kv_positions = "context-dependent",
    .kv_capacity = "planned",
    .kv_capacity_status = "planned",
    .kv_indexing = "layer-head-position-token-order",
    .context_length_source = "planned",
    .attention_dependency_status = "blocked-missing-qkv",
    .attention_q_required = 1,
    .attention_k_required = 1,
    .attention_v_required = 1,
    .attention_q_status = "missing",
    .attention_k_status = "missing",
    .attention_v_status = "missing",
    .attention_o_status = "missing",
    .tensor_inventory_status = "not-performed",
    .role_token_embedding_status = "missing",
    .role_attention_norm_status = "missing",
    .role_q_projection_status = "missing",
    .role_k_projection_status = "missing",
    .role_v_projection_status = "missing",
    .role_o_projection_status = "missing",
    .role_output_head_status = "missing",
    .cleanup_attempted = 1,
    .cleanup_status = "pass",
    .next_required_rows =
        "ATTENTION.CLASS.0 complete,CONTEXT.CLASS.0,RUNTIME.KV.1,"
        "RUNTIME.KV.2,RUNTIME.KV.3,real-transformer-prefill,real-decode,"
        "real-output-head-logits,GEN.DEEPSEEK.0",
    .top_blocker = "real attention-backed KV unsupported",
    .source_artifact_class = "",
    .target_artifact_class = "",
    .reason = "",
    .kv_layer_indexing = "planned",
    .kv_head_indexing = "planned",
    .kv_position_indexing = "planned",
    .kv_token_order_policy = "prompt-order-then-append-order",
    .kv_residency_class = "planned",
    .kv_residency_status = "planned",
    .kv_cpu_bytes_estimate = "unknown",
    .kv_cuda_bytes_estimate = "unknown",
    .kv_host_staged_bytes_estimate = "unknown",
    .kv_ssd_staged_status = "planned",
    .kv_ssd_streamed_status = "planned",
    .kv_paged_status = "planned",
    .kv_chunked_status = "planned",
    .kv_quantized_status = "planned",
    .kv_managed_memory_status = "planned",
    .context_required = "true",
    .requested_context = "not-requested",
    .context_capacity_status = "planned",
    .context_overflow_policy = "planned-refusal-before-mutation",
    .attention_runtime_ready = "false",
    .full_transformer_attention_ready = "false",
    .prefill_kv_write_required = "true",
    .prefill_kv_write_ready = "false",
    .decode_kv_read_required = "true",
    .decode_kv_read_ready = "false",
    .qkv_role_coverage = "missing"
};

typedef struct {
    size_t offset;
    const char *value;
} kv_string_patch;

#define KV_STRING(field, text) {offsetof(yvex_kv_report, field), text}

static const kv_string_patch kv_unsupported_strings[] = {
    KV_STRING(status, "kv-report-unsupported"),
    KV_STRING(family_runtime_status, "unsupported"),
    KV_STRING(attention_class_status, "unsupported"),
    KV_STRING(kv_class_status, "unsupported"),
    KV_STRING(kv_source, "unsupported-family"),
    KV_STRING(kv_layout, "unsupported"),
    KV_STRING(kv_layout_status, "unsupported"),
    KV_STRING(kv_dtype, "unsupported"),
    KV_STRING(kv_dtype_status, "unsupported"),
    KV_STRING(kv_layers_status, "unsupported"),
    KV_STRING(kv_heads_status, "unsupported"),
    KV_STRING(kv_head_dim_status, "unsupported"),
    KV_STRING(kv_positions, "unknown"),
    KV_STRING(kv_capacity, "unsupported"),
    KV_STRING(kv_capacity_status, "unsupported"),
    KV_STRING(kv_indexing, "unsupported"),
    KV_STRING(context_length_source, "unsupported-family"),
    KV_STRING(attention_dependency_status, "unsupported-family"),
    KV_STRING(prefill_kv_write_required, "unknown"),
    KV_STRING(decode_kv_read_required, "unknown"),
};

static const kv_string_patch kv_source_only_strings[] = {
    KV_STRING(status, "kv-report-unsupported"),
    KV_STRING(model_resolved_path, "not-resolved-source-only-target"),
    KV_STRING(target_id, "glm-5.2-official-safetensors"),
    KV_STRING(target_class, "official-source-huge-model"),
    KV_STRING(family_detected, "glm"),
    KV_STRING(family_runtime_status, "unsupported"),
    KV_STRING(attention_class_status, "unsupported"),
    KV_STRING(kv_class_status, "unsupported"),
    KV_STRING(tensor_inventory_status, "not-performed-source-only-target"),
    KV_STRING(source_artifact_class, "official safetensors"),
    KV_STRING(target_artifact_class, "future YVEX-produced GGUF"),
    KV_STRING(kv_source, "planned-family-mapping"),
    KV_STRING(kv_positions, "planned"),
    KV_STRING(kv_indexing, "planned"),
    KV_STRING(kv_token_order_policy, "planned"),
    KV_STRING(context_length_source, "planned-source-manifest"),
    KV_STRING(attention_dependency_status, "unsupported-source-only"),
};

#undef KV_STRING

/* Purpose: apply one immutable refusal projection without duplicating report policy. */
static void kv_report_apply_strings(yvex_kv_report *report,
                                    const kv_string_patch *patches,
                                    size_t count)
{
    size_t index;

    for (index = 0; report && patches && index < count; ++index) {
        const char **field = (const char **)((unsigned char *)report + patches[index].offset);

        *field = patches[index].value;
    }
}

/* Purpose: Project immutable kv report defaults facts into the typed reporting surface. */
static void kv_report_defaults(const yvex_kv_report_request *request, yvex_kv_report *report)
{
    *report = kv_report_default;
    if (!request) return;
    report->kind = request->kind;
    report->model = request->model ? request->model : "";
    report->backend = request->backend ? request->backend : "cpu";
    report->family = request->family ? request->family : "auto";
    report->family_requested = report->family;
    report->include_attention = request->include_attention;
    report->include_context = request->include_context;
    report->include_residency = request->include_residency;
    report->include_blockers = request->include_blockers;
}

/* Purpose: Project immutable kv report apply scan facts into the typed reporting surface.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void kv_report_apply_scan(yvex_kv_report *report,
                                 const yvex_kv_role_scan *scan,
                                 unsigned long long max_context)
{
    size_t role;
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
    report->blocker_count = 0u;
    for (role = 0; role < sizeof(kv_family_roles) / sizeof(kv_family_roles[0]); ++role) {
        const int *present = (const int *)((const unsigned char *)scan +
                                          kv_family_roles[role].scan_offset);
        const char **status = (const char **)((unsigned char *)report +
                                             kv_family_roles[role].report_offset);

        *status = kv_role_status(*present);
        if (!*present && kv_family_roles[role].missing_blocker) {
            kv_report_add_blocker(report, kv_family_roles[role].name, "missing",
                                  kv_family_roles[role].missing_blocker);
        }
    }
    report->qkv_role_coverage = has_qkv ? "present" : "missing";
    kv_report_add_blocker(report, "real attention-backed KV writes",
                          "unsupported", "missing-real-kv-path");
    kv_report_add_blocker(report, "decode KV consumer",
                          "unsupported", "missing-decode-consumer");
    kv_report_add_blocker(report, "KV capacity estimator",
                          "planned", "capacity-estimator-pending");
}

/* Purpose: Publish kv report set unsupported family only within its admitted destination range.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void kv_report_set_unsupported_family(yvex_kv_report *report,
                                             const yvex_kv_report_request *request,
                                             const char *detected,
                                             const char *reason)
{
    kv_report_defaults(request, report);
    kv_report_apply_strings(report, kv_unsupported_strings,
                            sizeof(kv_unsupported_strings) / sizeof(kv_unsupported_strings[0]));
    report->family = request && request->family ? request->family : "unknown";
    report->family_detected = detected ? detected : "unknown";
    report->kv_required = 0;
    report->attention_q_required = 0;
    report->attention_k_required = 0;
    report->attention_v_required = 0;
    report->top_blocker = reason ? reason : "unsupported family";
    kv_report_copy(report->reason_storage, sizeof(report->reason_storage),
                   &report->reason, report->top_blocker);
    report->exit_code = 5;
    report->blocker_count = 0u;
    kv_report_add_blocker(report, report->top_blocker, "blocked",
                          "unsupported-family");
    kv_report_set_phases(report, "unsupported", "resolve-family");
}

/* Purpose: Publish kv report set source only only within its admitted destination range.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed generation refusal and publishes no partial success state.
 * Boundary: Generation protocol; does not bypass graph admission, persistent state, or runtime readiness. */
static void kv_report_set_source_only(yvex_kv_report *report,
                                      const yvex_kv_report_request *request)
{
    const char *family = request && kv_streq(request->family, "auto")
                             ? "glm"
                             : request ? request->family : "glm";

    kv_report_defaults(request, report);
    kv_report_apply_strings(report, kv_source_only_strings,
                            sizeof(kv_source_only_strings) / sizeof(kv_source_only_strings[0]));
    report->family = family ? family : "glm";
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

/* Purpose: build a typed KV class report from model, family, backend, and registry request facts.
 * Inputs: request and report are caller-owned; err is optional diagnostic storage.
 * Effects: may open and close a local model artifact context to inspect metadata and tensor
 * roles; writes no operator output and allocates no persistent state.
 * Failure: returns precise YVEX status and leaves report populated when a reportable boundary exists.
 * Boundary: this is report construction only, not runtime KV execution. */
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
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "kv-report", "request and report are required");
    }
    kv_report_defaults(request, report);
    if (!request->model) {
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "kv-report", "yvex: kv report requires model");
    }
    if (!request->backend ||
        (!kv_streq(request->backend, "cpu") &&
         !kv_streq(request->backend, "cuda"))) {
        return yvex_generation_refuse(
            err, YVEX_ERR_INVALID_ARG, "kv-report",
            "yvex: kv report backend must be cpu or cuda");
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
        report->exit_code = yvex_generation_exit_code(rc, YVEX_GENERATION_EXIT_KV);
        kv_report_set_phases(report, "failed", "resolve-model");
        goto done;
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
        report->exit_code = yvex_generation_exit_code(rc, YVEX_GENERATION_EXIT_KV);
        kv_report_set_phases(report, "failed", "kv-profile");
        goto done;
    }

    detected_family = kv_detect_family(&ref, &ctx, request->model);
    family = kv_streq(request->family, "auto") ? detected_family : request->family;
    if (!kv_streq(request->family, "auto") &&
        !kv_streq(request->family, detected_family)) {
        kv_report_set_unsupported_family(
            report,
            request,
            detected_family,
            "requested family does not match resolved model family");
        rc = YVEX_OK;
        goto done;
    }
    if (!kv_streq(family, "deepseek")) {
        kv_report_set_unsupported_family(
            report,
            request,
            detected_family,
            "KV class report is currently implemented for DeepSeek-family GGUF artifacts only");
        rc = YVEX_OK;
        goto done;
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

done:
    kv_model_context_close(&ctx);
    yvex_model_ref_clear(&ref);
    if (rc == YVEX_OK) {
        yvex_error_clear(err);
    }
    return rc;
}

/* Purpose: build a typed report for the minimal session-owned diagnostic KV allocation path.
 * Inputs: request carries shape and optional append/read demo settings; report is
 * caller-owned output; err is optional diagnostic storage.
 * Effects: allocates a bounded temporary KV cache, may append/read synthetic values,
 * captures summary facts, and always closes temporary KV state before returning.
 * Failure: returns precise YVEX status for invalid shape, allocation failure, bounds refusal, or read failure.
 * Boundary: the allocation is minimal diagnostic KV only, not attention-backed KV. */
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
        return yvex_generation_refuse(err, YVEX_ERR_INVALID_ARG,
                                      "kv-ownership", "request and report are required");
    }
    kv_report_defaults(request, report);
    report->kind = YVEX_KV_REQUEST_OWNERSHIP;
    report->status = "kv-owned";
    report->generation = "unsupported";
    report->exit_code = 0;

    rc = yvex_kv_cache_create_shape(&kv, &request->shape, err);
    if (rc != YVEX_OK) {
        report->status = "kv-fail";
        report->exit_code = yvex_generation_exit_code(rc, YVEX_GENERATION_EXIT_KV);
        return rc;
    }

    value_count = yvex_kv_cache_position_value_count(kv);
    if (request->append_demo) {
        append_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!append_values) {
            rc = yvex_generation_refuse(
                err, YVEX_ERR_NOMEM, "kv-ownership",
                "failed to allocate KV append demo buffer");
            goto fail;
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
                goto fail;
            }
        }
    }

    if (request->read_requested) {
        read_values = (float *)calloc((size_t)value_count, sizeof(float));
        if (!read_values) {
            rc = yvex_generation_refuse(err, YVEX_ERR_NOMEM,
                                        "kv-ownership",
                                        "failed to allocate KV read buffer");
            goto fail;
        }
        rc = yvex_kv_cache_read_position_f32(kv,
                                             request->read_position,
                                             read_values,
                                             value_count,
                                             err);
        if (rc != YVEX_OK) {
            goto fail;
        }
        report->read_checksum = yvex_kv_checksum_values(read_values, value_count);
    }

    rc = yvex_kv_cache_get_summary(kv, &report->ownership_summary, err);
    if (rc != YVEX_OK) {
        goto fail;
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
        memcpy(report->read_sample_values, read_values,
               (size_t)sample_count * sizeof(read_values[0]));
    }

    report->cleanup_attempted = 1;
    report->cleanup_status = "pass";
    yvex_error_clear(err);
    goto done;

fail:
    report->status = "kv-fail";
    report->exit_code = yvex_generation_exit_code(
        rc, rc == YVEX_ERR_NOMEM ? YVEX_GENERATION_EXIT_KV_OWNERSHIP
                                 : YVEX_GENERATION_EXIT_KV);

done:
    free(read_values);
    free(append_values);
    yvex_kv_cache_close(kv);
    return rc;
}
