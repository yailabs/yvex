/*
 * yvex_attention_surface.c - attention command-family CLI surface.
 * Owner: src/cli/render
 * Owns: existing attention command-family parsing and output behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing command behavior.
 * Boundary: attention reports are report-only diagnostics.
 */
#include "yvex_attention_render.h"

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    int include_kv;
    int include_context;
    int include_graph;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_attention_options;

static int attention_parse_value_option(const char *flag,
                                        int arg_count,
                                        char **args,
                                        int *index,
                                        const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: attention %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: attention %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_attention_options(int arg_count,
                                   char **args,
                                   yvex_cli_attention_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_attention_render_help(stdout);
        return 1;
    }
    if (arg_count < 3 || strcmp(args[2], "report") != 0) {
        yvex_cli_out_writef(stderr, "yvex: attention requires report\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex attention report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--" "include-kv] [--" "include-context] [--" "include-graph] [--" "include-blockers]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--model") == 0) {
            int rc = attention_parse_value_option("--model", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(args[i], "--backend") == 0) {
            int rc = attention_parse_value_option("--backend", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                yvex_cli_out_writef(stderr, "yvex: attention --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(args[i], "--family") == 0) {
            int rc = attention_parse_value_option("--family", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(args[i], "--registry") == 0) {
            int rc = attention_parse_value_option("--registry", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(args[i], "--" "include-kv") == 0) {
            options->include_kv = 1;
        } else if (strcmp(args[i], "--" "include-context") == 0) {
            options->include_context = 1;
        } else if (strcmp(args[i], "--" "include-graph") == 0) {
            options->include_graph = 1;
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            int rc = attention_parse_value_option("--" "output", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: attention unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_attention_render_help(stdout);
            return 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown attention option: %s\n", args[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        yvex_cli_out_writef(stderr, "yvex: attention report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *attention_requested_family(const yvex_cli_attention_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void attention_print_phase(unsigned int index,
                                  const char *name,
                                  const char *status)
{
    yvex_cli_out_writef(stdout, "attention_phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "attention_phase.%u.status: %s\n", index, status ? status : "planned");
}

static void attention_print_phases(const char *attention_status,
                                   const char *head_layout_status,
                                   const char *qkv_status,
                                   const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "attention-profile",
        "head-layout",
        "qkv-role-check",
        "position-rules",
        "mask-rules",
        "kv-requirements",
        "context-requirements",
        "graph-requirements",
        "backend-requirements",
        "blocker-report",
        "complete",
        "failed",
        "cleanup"
    };
    unsigned int i;
    int failed_seen = 0;

    for (i = 0; i < sizeof(phases) / sizeof(phases[0]); ++i) {
        const char *status = "pass";
        if (failure_phase && strcmp(failure_phase, phases[i]) == 0) {
            status = "failed";
            failed_seen = 1;
        } else if (failed_seen) {
            status = "blocked";
        } else if (strcmp(phases[i], "attention-profile") == 0) {
            status = attention_status ? attention_status : "partial";
        } else if (strcmp(phases[i], "head-layout") == 0) {
            status = head_layout_status ? head_layout_status : "unknown";
        } else if (strcmp(phases[i], "qkv-role-check") == 0) {
            status = qkv_status ? qkv_status : "partial";
        } else if (strcmp(phases[i], "position-rules") == 0 ||
                   strcmp(phases[i], "mask-rules") == 0 ||
                   strcmp(phases[i], "context-requirements") == 0) {
            status = "planned";
        } else if (strcmp(phases[i], "kv-requirements") == 0 ||
                   strcmp(phases[i], "graph-requirements") == 0 ||
                   strcmp(phases[i], "backend-requirements") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "unsupported";
        }
        attention_print_phase(i, phases[i], status);
    }
}

static const char *attention_role_status(yvex_cli_tokenizer_context *ctx,
                                         const yvex_fullmodel_collections *collections,
                                         const char *role)
{
    return fullmodel_role_status_from_tensor(ctx, collections, role);
}

static void attention_print_projection_role(yvex_cli_tokenizer_context *ctx,
                                            const yvex_fullmodel_collections *collections,
                                            const char *role,
                                            const char *prefix)
{
    const yvex_tensor_info *tensor = fullmodel_descriptor_find_tensor(ctx, role);
    char dims[128];
    const char *status = attention_role_status(ctx, collections, role);

    dims[0] = '\0';
    if (tensor) dims_to_text(tensor->dims, tensor->rank, dims, sizeof(dims));
    yvex_cli_out_writef(stdout, "%s_required: true\n", prefix);
    yvex_cli_out_writef(stdout, "%s_status: %s\n", prefix, status);
    yvex_cli_out_writef(stdout, "%s_tensor: %s\n", prefix, tensor && tensor->name ? tensor->name : "none");
    yvex_cli_out_writef(stdout, "%s_shape: %s\n", prefix, tensor ? dims : "unknown");
    yvex_cli_out_writef(stdout, "%s_dtype: %s\n", prefix, tensor ? yvex_dtype_name(tensor->dtype) : "unknown");
    yvex_cli_out_writef(stdout, "%s_qtype: %s\n", prefix, tensor ? yvex_dtype_name(tensor->dtype) : "unknown");
    yvex_cli_out_writef(stdout, "%s_bytes: %llu\n", prefix, tensor ? tensor->storage_bytes : 0ull);
    yvex_cli_out_writef(stdout, "%s_runtime_consumer: %s\n", prefix,
           tensor ? "planned-full-transformer-attention" : "blocked-missing-role");
}

static void attention_print_unsupported_common(const char *model,
                                               const char *resolved_path,
                                               const char *target_id,
                                               const char *target_class,
                                               const char *backend,
                                               const char *family,
                                               const char *detected,
                                               const char *requested,
                                               const char *status,
                                               const char *reason,
                                               const char *phase)
{
    yvex_cli_out_writef(stdout, "attention: report\n");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "attention-report-unsupported");
    yvex_cli_out_writef(stdout, "model: %s\n", model ? model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "unknown");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend ? backend : "cpu");
    yvex_cli_out_writef(stdout, "family: %s\n", family ? family : "unknown");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected ? detected : "unknown");
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested ? requested : "auto");
    yvex_cli_out_writef(stdout, "family_runtime_status: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_class_status: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_stage: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "attention_family: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_type: unknown\n");
    yvex_cli_out_writef(stdout, "attention_support_status: report-only\n");
    yvex_cli_out_writef(stdout, "full_transformer_attention: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_backend_ready: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "q_projection_required: true\n");
    yvex_cli_out_writef(stdout, "k_projection_required: true\n");
    yvex_cli_out_writef(stdout, "v_projection_required: true\n");
    yvex_cli_out_writef(stdout, "o_projection_required: true\n");
    yvex_cli_out_writef(stdout, "q_projection_status: unknown\n");
    yvex_cli_out_writef(stdout, "k_projection_status: unknown\n");
    yvex_cli_out_writef(stdout, "v_projection_status: unknown\n");
    yvex_cli_out_writef(stdout, "o_projection_status: unknown\n");
    yvex_cli_out_writef(stdout, "attention_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "hidden_size: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_status: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_source: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_blockers: attention class unavailable\n");
    yvex_cli_out_writef(stdout, "position_policy: planned\n");
    yvex_cli_out_writef(stdout, "rope_required: true\n");
    yvex_cli_out_writef(stdout, "rope_status: planned\n");
    yvex_cli_out_writef(stdout, "rope_base: unknown\n");
    yvex_cli_out_writef(stdout, "rope_scaling: unknown\n");
    yvex_cli_out_writef(stdout, "rope_dimension: unknown\n");
    yvex_cli_out_writef(stdout, "rope_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "mask_policy: planned\n");
    yvex_cli_out_writef(stdout, "causal_mask_required: true\n");
    yvex_cli_out_writef(stdout, "mask_status: planned\n");
    yvex_cli_out_writef(stdout, "mask_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-full-transformer-kv\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_kv_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_boundary: diagnostic-kv-exists-real-attention-kv-unsupported\n");
    yvex_cli_out_writef(stdout, "context_policy: planned\n");
    yvex_cli_out_writef(stdout, "max_context: unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_status: planned\n");
    yvex_cli_out_writef(stdout, "context_blockers: attention class unavailable\n");
    yvex_cli_out_writef(stdout, "graph_rope_primitive: implemented\n");
    yvex_cli_out_writef(stdout, "graph_attention_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph_qkv_projection: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_o_projection: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_full_transformer_attention: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_backend_status: report-only\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: full-transformer-attention,real-attention-backed-kv,real-transformer-prefill\n");
    yvex_cli_out_writef(stdout, "backend_attention_status: implemented-fixture-full-transformer-unsupported\n");
    yvex_cli_out_writef(stdout, "backend_rope_status: implemented-primitive\n");
    yvex_cli_out_writef(stdout, "backend_softmax_status: implemented-inside-attention-fixture\n");
    yvex_cli_out_writef(stdout, "backend_matmul_status: implemented-primitive\n");
    yvex_cli_out_writef(stdout, "backend_kv_status: unsupported-real-attention-kv\n");
    yvex_cli_out_writef(stdout, "attention_blockers: %s\n", reason ? reason : "attention class unavailable");
    yvex_cli_out_writef(stdout, "next_required_rows: ATTENTION.CLASS.0,KV.CACHE.0,CONTEXT.CLASS.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    attention_print_phases("unsupported", "unknown", "unknown", phase);
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "unsupported attention class report");
}

static int attention_print_source_only_report(const yvex_cli_attention_options *options,
                                              const char *target)
{
    attention_print_unsupported_common(options && options->model ? options->model : target,
                                       "source-only-target",
                                       target,
                                       "official-source-huge-model",
                                       options && options->backend ? options->backend : "cpu",
                                       "glm",
                                       "glm",
                                       attention_requested_family(options),
                                       "attention-report-unsupported",
                                       "source-only target has no YVEX-produced GGUF tensor inventory; GLM attention class mapping planned",
                                       "resolve-model");
    yvex_cli_out_writef(stdout, "tensor_inventory_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "source_artifact_class: official safetensors\n");
    yvex_cli_out_writef(stdout, "target_artifact_class: future YVEX-produced GGUF\n");
    return 5;
}

static int attention_print_report(const yvex_cli_attention_options *options,
                                  yvex_model_ref *ref,
                                  yvex_cli_tokenizer_context *ctx,
                                  const char *target_id,
                                  const char *target_class,
                                  unsigned long long artifact_bytes,
                                  yvex_arch arch,
                                  unsigned long long tensor_count,
                                  unsigned long long total_tensor_bytes,
                                  const yvex_fullmodel_collections *collections,
                                  int selected_target)
{
    yvex_fullmodel_backend_fit fit;
    const char *backend = options && options->backend ? options->backend : "cpu";
    const char *requested = attention_requested_family(options);
    yvex_cli_fullmodel_options family_probe;
    const char *detected;
    int request_matches;
    int supported_family;
    int has_q;
    int has_k;
    int has_v;
    int has_o;
    int has_all_qkvo;
    const char *attention_class_status;
    const char *qkv_status;
    const char *graph_projection_status;
    const char *blockers;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : "auto";
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    request_matches = fullmodel_family_request_matches(requested, detected);
    supported_family = request_matches && strcmp(detected, "deepseek") == 0;
    if (!supported_family) {
        const char *reason = !request_matches
                                 ? "requested family does not match detected family"
                                 : "attention report supports DeepSeek-family artifacts first";
        attention_print_unsupported_common(options && options->model ? options->model : "",
                                           ref && ref->path ? ref->path : "",
                                           target_id ? target_id : "path",
                                           target_class ? target_class : "candidate-GGUF-path",
                                           backend,
                                           detected ? detected : "unknown",
                                           detected ? detected : "unknown",
                                           requested,
                                           "attention-report-unsupported",
                                           reason,
                                           "resolve-family");
        return 5;
    }

    has_q = collections && collections->has_attention_q;
    has_k = collections && collections->has_attention_k;
    has_v = collections && collections->has_attention_v;
    has_o = collections && collections->has_attention_out;
    has_all_qkvo = has_q && has_k && has_v && has_o;
    attention_class_status = selected_target || !has_all_qkvo ? "partial" : "complete";
    qkv_status = has_all_qkvo ? "pass" : "partial";
    graph_projection_status = has_all_qkvo ? "planned" : "missing-tensor";
    blockers = has_all_qkvo
                   ? "real QKV projection over model tensors unsupported; attention-backed KV write unsupported; full transformer attention integration unsupported; real transformer prefill unsupported"
                   : "q projection tensor missing; k projection tensor missing; v projection tensor missing; o projection tensor missing; attention head layout incomplete; real QKV projection unsupported; real attention-backed KV writes unsupported; full transformer attention unsupported; real transformer prefill unsupported";

    fullmodel_probe_backend_fit(backend, total_tensor_bytes, &fit);

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "report: attention\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "family: deepseek backend=%s\n", backend);
        yvex_cli_out_writef(stdout, "status: %s\n", attention_class_status);
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", has_all_qkvo ? "runtime attention integration missing" : "missing Q/K/V/O projection tensors");
        yvex_cli_out_writef(stdout, "next: V010.ATTN.9\n");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return 0;
    }

    yvex_cli_out_writef(stdout, "attention: report\n");
    yvex_cli_out_writef(stdout, "status: attention-report\n");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", ref && ref->path ? ref->path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "path");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "candidate-GGUF-path");
    yvex_cli_out_writef(stdout, "backend: %s\n", backend);
    yvex_cli_out_writef(stdout, "family: deepseek\n");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected);
    yvex_cli_out_writef(stdout, "family_requested: %s\n", requested);
    yvex_cli_out_writef(stdout, "family_runtime_status: %s\n", attention_class_status);
    yvex_cli_out_writef(stdout, "attention_class_status: %s\n", attention_class_status);
    yvex_cli_out_writef(stdout, "attention_stage: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
    yvex_cli_out_writef(stdout, "artifact_identity_status: %s\n", fullmodel_identity_status(ref, artifact_bytes));
    yvex_cli_out_writef(stdout, "tensor_inventory_status: pass\n");
    yvex_cli_out_writef(stdout, "tensor_count: %llu\n", tensor_count);
    yvex_cli_out_writef(stdout, "total_tensor_bytes: %llu\n", total_tensor_bytes);
    yvex_cli_out_writef(stdout, "attention_family: model-family-specific\n");
    yvex_cli_out_writef(stdout, "attention_type: unknown\n");
    yvex_cli_out_writef(stdout, "attention_support_status: report-only\n");
    yvex_cli_out_writef(stdout, "full_transformer_attention: unsupported\n");
    yvex_cli_out_writef(stdout, "attention_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_backend_ready: false\n");
    yvex_cli_out_writef(stdout, "full_model_execution: unsupported\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_metadata_status: incomplete\n");
    yvex_cli_out_writef(stdout, "attention_blocker: attention metadata incomplete\n");
    yvex_cli_out_writef(stdout, "attention_norm_role: %s\n", attention_role_status(ctx, collections, "attention_norm"));
    attention_print_projection_role(ctx, collections, "q_projection", "q_projection");
    attention_print_projection_role(ctx, collections, "k_projection", "k_projection");
    attention_print_projection_role(ctx, collections, "v_projection", "v_projection");
    attention_print_projection_role(ctx, collections, "o_projection", "o_projection");
    yvex_cli_out_writef(stdout, "attention_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "hidden_size: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_status: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_source: unknown\n");
    yvex_cli_out_writef(stdout, "head_layout_blockers: attention metadata incomplete; head count, KV head count, and head dimension unavailable\n");
    yvex_cli_out_writef(stdout, "position_policy: rope-or-family-specific-planned\n");
    yvex_cli_out_writef(stdout, "rope_required: true\n");
    yvex_cli_out_writef(stdout, "rope_status: planned\n");
    yvex_cli_out_writef(stdout, "rope_base: unknown\n");
    yvex_cli_out_writef(stdout, "rope_scaling: unknown\n");
    yvex_cli_out_writef(stdout, "rope_dimension: unknown\n");
    yvex_cli_out_writef(stdout, "graph_rope_primitive: implemented\n");
    yvex_cli_out_writef(stdout, "rope_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "mask_policy: causal-or-family-specific-planned\n");
    yvex_cli_out_writef(stdout, "causal_mask_required: true\n");
    yvex_cli_out_writef(stdout, "mask_status: planned\n");
    yvex_cli_out_writef(stdout, "mask_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_required: true\n");
    yvex_cli_out_writef(stdout, "kv_layout: planned\n");
    yvex_cli_out_writef(stdout, "kv_dtype: planned\n");
    yvex_cli_out_writef(stdout, "kv_layers: unknown\n");
    yvex_cli_out_writef(stdout, "kv_heads: unknown\n");
    yvex_cli_out_writef(stdout, "kv_head_dim: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported-full-transformer-kv\n");
    yvex_cli_out_writef(stdout, "kv_write_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_read_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_kv_runtime_ready: false\n");
    yvex_cli_out_writef(stdout, "kv_boundary: diagnostic-kv-exists-real-attention-kv-unsupported\n");
    yvex_cli_out_writef(stdout, "context_policy: planned\n");
    yvex_cli_out_writef(stdout, "max_context: metadata-or-unknown\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "context_status: planned\n");
    yvex_cli_out_writef(stdout, "context_blockers: context class report pending; attention head layout incomplete\n");
    yvex_cli_out_writef(stdout, "graph_attention_primitive: implemented-fixture\n");
    yvex_cli_out_writef(stdout, "graph_matmul_primitive: implemented\n");
    yvex_cli_out_writef(stdout, "graph_qkv_projection: %s\n", graph_projection_status);
    yvex_cli_out_writef(stdout, "graph_o_projection: %s\n", has_o ? "planned" : "missing-tensor");
    yvex_cli_out_writef(stdout, "graph_model_qkv_projection: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_attention_kv_write: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_layer_integrated_attention: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_full_transformer_attention: unsupported\n");
    yvex_cli_out_writef(stdout, "graph_backend_status: report-only\n");
    yvex_cli_out_writef(stdout, "unsupported_graph_ops: full-transformer-attention,real-qkv-projection,attention-backed-kv-write,layer-integrated-attention,real-transformer-prefill\n");
    yvex_cli_out_writef(stdout, "backend_attention_status: implemented-fixture-full-transformer-unsupported\n");
    yvex_cli_out_writef(stdout, "backend_rope_status: implemented-primitive\n");
    yvex_cli_out_writef(stdout, "backend_softmax_status: implemented-inside-attention-fixture\n");
    yvex_cli_out_writef(stdout, "backend_matmul_status: implemented-primitive\n");
    yvex_cli_out_writef(stdout, "backend_kv_status: unsupported-real-attention-kv\n");
    yvex_cli_out_writef(stdout, "backend_available: %s\n", fit.available ? "true" : "false");
    yvex_cli_out_writef(stdout, "backend_required_bytes: %llu\n", fit.required_bytes);
    yvex_cli_out_writef(stdout, "backend_fit_status: %s\n", fit.fit_status);
    yvex_cli_out_writef(stdout, "backend_allocation_attempted: false\n");
    yvex_cli_out_writef(stdout, "attention_blockers: %s\n", blockers);
    yvex_cli_out_writef(stdout, "prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "logits_ready: false\n");
    yvex_cli_out_writef(stdout, "sampling_ready: false\n");
    yvex_cli_out_writef(stdout, "runtime_execution_ready: false\n");
    yvex_cli_out_writef(stdout, "next_required_rows: KV.CACHE.0,CONTEXT.CLASS.0,real-transformer-prefill,real-attention-backed-KV,real-decode,GEN.DEEPSEEK.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
    attention_print_phases(attention_class_status, "unknown", qkv_status, NULL);
    return 0;
}

int yvex_attention_render_command(int arg_count, char **args)
{
    yvex_cli_attention_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long total_tensor_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_attention_options(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return attention_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        attention_print_unsupported_common(options.model,
                                           ref.path,
                                           ref.alias && ref.alias[0] ? ref.alias : "unknown",
                                           "unresolved-artifact",
                                           options.backend,
                                           "unknown",
                                           "unknown",
                                           attention_requested_family(&options),
                                           "attention-report-fail",
                                           "artifact path does not exist",
                                           "resolve-model");
        yvex_model_ref_clear(&ref);
        return exit_for_status(YVEX_ERR_IO);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        attention_print_unsupported_common(options.model,
                                           ref.path,
                                           ref.alias && ref.alias[0] ? ref.alias : "path",
                                           "GGUF-artifact",
                                           options.backend,
                                           "unknown",
                                           "unknown",
                                           attention_requested_family(&options),
                                           "attention-report-fail",
                                           yvex_error_message(&err),
                                           "load-family-runtime");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        total_tensor_bytes += tensor->storage_bytes;
        fullmodel_classify_tensor(tensor, &collections);
    }
    if (yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.tokens") ||
        yvex_gguf_metadata_find(ctx.gguf, "tokenizer.ggml.model")) {
        collections.tokenizer = 1ull;
        collections.has_tokenizer_metadata = 1;
    }

    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = attention_print_report(&options,
                                &ref,
                                &ctx,
                                target_id,
                                target_class,
                                artifact_bytes,
                                arch,
                                tensor_count,
                                total_tensor_bytes,
                                &collections,
                                selected_target);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_attention_render_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex attention report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--" "audit | --" "output normal|table|audit] [--" "include-kv] [--" "include-context] [--" "include-graph] [--" "include-blockers]\n");
    yvex_cli_out_writef(fp, "\nExamples:\n");
    yvex_cli_out_writef(fp, "  yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu\n");
    yvex_cli_out_writef(fp, "  yvex attention report --model deepseek4-v4-flash-selected-embed-rmsnorm --family auto --backend cpu --" "include-kv --" "include-graph\n");
    yvex_cli_out_writef(fp, "\nattention report:\n");
    yvex_cli_out_writef(fp, "  classifies attention requirements, head layout, Q/K/V/O roles, RoPE/position rules, mask rules, KV requirements, context blockers, graph requirements, backend requirements, and runtime blockers.\n");
    yvex_cli_out_writef(fp, "  Default output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "  report-only boundary: it does not run full attention, does not run transformer prefill, does not project Q/K/V from model tensors, does not write real attention-backed KV, does not generate, and does not benchmark.\n");
    yvex_cli_out_writef(fp, "  standalone RoPE and attention primitives may be implemented, but those primitive proofs are not full transformer attention and are not model inference support.\n");
    yvex_cli_out_writef(fp, "Boundary: no full transformer attention execution, no real QKV projection, no real attention-backed KV writes, no full model execution, no DeepSeek generation, no provider generation, no eval, no benchmark, no throughput.\n");
}
