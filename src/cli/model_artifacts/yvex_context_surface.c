/*
 * yvex_context_surface.c - context command-family CLI surface.
 * Owner: src/cli/model_artifacts
 * Owns: existing context command-family parsing and output behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing command behavior.
 * Boundary: context reports are report-only diagnostics.
 */
#include "yvex_context_surface.h"

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    const char *tokens_text;
    unsigned long long context_length;
    unsigned long long chunk_size;
    int context_length_seen;
    int chunk_size_seen;
    int include_attention;
    int include_kv;
    int include_prefill;
    int include_decode;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_context_options;

static int context_parse_value_option(const char *flag,
                                      int arg_count,
                                      char **args,
                                      int *index,
                                      const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: context %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: context %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_context_options(int arg_count,
                                 char **args,
                                 yvex_cli_context_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_context_help(stdout);
        return 1;
    }
    if (arg_count < 3 || strcmp(args[2], "report") != 0) {
        yvex_cli_out_writef(stderr, "yvex: context requires report\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex context report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [options]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--model") == 0) {
            int rc = context_parse_value_option("--model", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(args[i], "--backend") == 0) {
            int rc = context_parse_value_option("--backend", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                yvex_cli_out_writef(stderr, "yvex: context --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(args[i], "--family") == 0) {
            int rc = context_parse_value_option("--family", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(args[i], "--registry") == 0) {
            int rc = context_parse_value_option("--registry", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(args[i], "--tokens") == 0) {
            int rc = context_parse_value_option("--tokens", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->tokens_text = value;
        } else if (strcmp(args[i], "--context-length") == 0) {
            int rc = context_parse_value_option("--context-length", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &options->context_length)) {
                yvex_cli_out_writef(stderr, "yvex: context --context-length requires a positive integer\n");
                return 2;
            }
            options->context_length_seen = 1;
        } else if (strcmp(args[i], "--chunk-size") == 0) {
            int rc = context_parse_value_option("--chunk-size", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_positive_ull(value, &options->chunk_size)) {
                yvex_cli_out_writef(stderr, "yvex: context --chunk-size requires a positive integer\n");
                return 2;
            }
            options->chunk_size_seen = 1;
        } else if (strcmp(args[i], "--" "include-attention") == 0) {
            options->include_attention = 1;
        } else if (strcmp(args[i], "--" "include-kv") == 0) {
            options->include_kv = 1;
        } else if (strcmp(args[i], "--" "include-prefill") == 0) {
            options->include_prefill = 1;
        } else if (strcmp(args[i], "--" "include-decode") == 0) {
            options->include_decode = 1;
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            int rc = context_parse_value_option("--" "output", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: context unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_model_artifacts_surface_context_help(stdout);
            return 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown context option: %s\n", args[i]);
            return 2;
        }
    }
    if (!options->model || !options->model[0]) {
        yvex_cli_out_writef(stderr, "yvex: context report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *context_requested_family(const yvex_cli_context_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void context_print_phase(unsigned int index,
                                const char *name,
                                const char *status)
{
    yvex_cli_out_writef(stdout, "context_phase.%u.name: %s\n", index, name ? name : "");
    yvex_cli_out_writef(stdout, "context_phase.%u.status: %s\n", index, status ? status : "unknown");
}

static void context_print_phases(const char *family_status,
                                 const char *attention_status,
                                 const char *kv_status,
                                 const char *context_status,
                                 const char *failure_phase)
{
    static const char *const phases[] = {
        "preflight",
        "resolve-model",
        "resolve-family",
        "load-family-runtime",
        "load-attention-class",
        "load-kv-class",
        "context-profile",
        "model-context",
        "requested-context",
        "active-context",
        "token-input",
        "chunking-policy",
        "prefill-boundary",
        "decode-position-policy",
        "overflow-policy",
        "runtime-blockers",
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
        } else if (failed_seen && strcmp(phases[i], "cleanup") != 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "load-family-runtime") == 0) {
            status = family_status ? family_status : "unknown";
        } else if (strcmp(phases[i], "load-attention-class") == 0) {
            status = attention_status ? attention_status : "unknown";
        } else if (strcmp(phases[i], "load-kv-class") == 0) {
            status = kv_status ? kv_status : "unknown";
        } else if (strcmp(phases[i], "context-profile") == 0) {
            status = context_status ? context_status : "planned";
        } else if (strcmp(phases[i], "prefill-boundary") == 0 ||
                   strcmp(phases[i], "decode-position-policy") == 0 ||
                   strcmp(phases[i], "runtime-blockers") == 0) {
            status = "blocked";
        } else if (strcmp(phases[i], "failed") == 0 && !failure_phase) {
            status = "unsupported";
        }
        context_print_phase(i, phases[i], status);
    }
}

static unsigned long long context_metadata_max_context(yvex_cli_tokenizer_context *ctx,
                                                       const char **source)
{
    static const char *const keys[] = {
        "llama.context_length",
        "deepseek.context_length",
        "qwen.context_length",
        "glm.context_length",
        "general.context_length"
    };
    unsigned int i;
    unsigned long long value = 0ull;

    if (source) *source = "unknown";
    if (ctx && ctx->model) {
        value = yvex_model_context_length(ctx->model);
        if (value > 0ull) {
            if (source) *source = "model-descriptor";
            return value;
        }
    }
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        const yvex_gguf_value *meta = ctx && ctx->gguf ? yvex_gguf_metadata_find(ctx->gguf, keys[i]) : NULL;
        if (meta && yvex_gguf_value_as_u64(meta, &value) == YVEX_OK && value > 0ull) {
            if (source) *source = keys[i];
            return value;
        }
    }
    return 0ull;
}

static const char *context_status_from_collections(const yvex_fullmodel_collections *collections,
                                                   int selected_target)
{
    if (selected_target) return "partial";
    if (collections &&
        collections->has_token_embedding &&
        collections->has_attention_q &&
        collections->has_attention_k &&
        collections->has_attention_v &&
        collections->has_attention_out) {
        return "complete";
    }
    return "partial";
}

static void context_print_common_header(const yvex_cli_context_options *options,
                                        const char *status,
                                        const char *resolved_path,
                                        const char *target_id,
                                        const char *target_class,
                                        const char *family,
                                        const char *detected,
                                        const char *family_status,
                                        const char *attention_status,
                                        const char *kv_status)
{
    yvex_cli_out_writef(stdout, "context: report\n");
    yvex_cli_out_writef(stdout, "status: %s\n", status ? status : "context-report");
    yvex_cli_out_writef(stdout, "model: %s\n", options && options->model ? options->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", resolved_path ? resolved_path : "");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target_id ? target_id : "unknown");
    yvex_cli_out_writef(stdout, "target_class: %s\n", target_class ? target_class : "unknown");
    yvex_cli_out_writef(stdout, "backend: %s\n", options && options->backend ? options->backend : "cpu");
    yvex_cli_out_writef(stdout, "family: %s\n", family ? family : "unknown");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", detected ? detected : "unknown");
    yvex_cli_out_writef(stdout, "family_requested: %s\n", context_requested_family(options));
    yvex_cli_out_writef(stdout, "family_runtime_status: %s\n", family_status ? family_status : "unknown");
    yvex_cli_out_writef(stdout, "attention_class_status: %s\n", attention_status ? attention_status : "unknown");
    yvex_cli_out_writef(stdout, "kv_class_status: %s\n", kv_status ? kv_status : "unknown");
    yvex_cli_out_writef(stdout, "context_class_status: report-only\n");
    yvex_cli_out_writef(stdout, "context_stage: report-only\n");
    yvex_cli_out_writef(stdout, "runtime_claim: unsupported\n");
    yvex_cli_out_writef(stdout, "generation: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "benchmark_status: not-measured\n");
}

static void context_print_unsupported_source_only(const yvex_cli_context_options *options)
{
    const char *family = strcmp(context_requested_family(options), "auto") == 0
                             ? "glm"
                             : context_requested_family(options);
    context_print_common_header(options,
                                "context-report-unsupported",
                                "source-only-target",
                                "glm-5.2-official-safetensors",
                                "official-source-huge-model",
                                family,
                                "glm",
                                "unsupported",
                                "unsupported",
                                "unsupported");
    yvex_cli_out_writef(stdout, "model_max_context: unknown\n");
    yvex_cli_out_writef(stdout, "model_max_context_source: source-manifest-planned\n");
    yvex_cli_out_writef(stdout, "model_max_context_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "requested_context: %s", options && options->context_length_seen ? "" : "not-requested");
    if (options && options->context_length_seen) yvex_cli_out_writef(stdout, "%llu", options->context_length);
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "requested_context_source: %s\n", options && options->context_length_seen ? "operator-request" : "none");
    yvex_cli_out_writef(stdout, "requested_context_status: %s\n", options && options->context_length_seen ? "reported-only" : "not-requested");
    yvex_cli_out_writef(stdout, "active_context: unsupported\n");
    yvex_cli_out_writef(stdout, "active_context_source: source-only-target\n");
    yvex_cli_out_writef(stdout, "active_context_status: unsupported\n");
    yvex_cli_out_writef(stdout, "token_input_status: not-performed-source-only-target\n");
    yvex_cli_out_writef(stdout, "token_count: 0\n");
    yvex_cli_out_writef(stdout, "prompt_token_count: 0\n");
    yvex_cli_out_writef(stdout, "prefill_token_count: 0\n");
    yvex_cli_out_writef(stdout, "generated_token_count: 0\n");
    yvex_cli_out_writef(stdout, "token_range_start: none\n");
    yvex_cli_out_writef(stdout, "token_range_end: none\n");
    yvex_cli_out_writef(stdout, "chunking_required: false\n");
    yvex_cli_out_writef(stdout, "chunk_size: not-requested\n");
    yvex_cli_out_writef(stdout, "chunk_count: 0\n");
    yvex_cli_out_writef(stdout, "last_chunk_size: 0\n");
    yvex_cli_out_writef(stdout, "chunking_policy: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "chunking_status: unsupported\n");
    yvex_cli_out_writef(stdout, "prefill_boundary_status: unsupported\n");
    yvex_cli_out_writef(stdout, "prefill_position_start: unknown\n");
    yvex_cli_out_writef(stdout, "prefill_position_end: unknown\n");
    yvex_cli_out_writef(stdout, "prefill_context_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_position_policy: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "decode_start_position: unknown\n");
    yvex_cli_out_writef(stdout, "decode_position_status: unsupported\n");
    yvex_cli_out_writef(stdout, "decode_context_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "overflow_policy: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "context_overflow: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_check_status: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_stop_reason: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_mutates_state: false\n");
    yvex_cli_out_writef(stdout, "attention_dependency_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "attention_context_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_position_policy: planned\n");
    yvex_cli_out_writef(stdout, "rope_context_status: planned\n");
    yvex_cli_out_writef(stdout, "mask_context_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_dependency_status: unsupported-source-only\n");
    yvex_cli_out_writef(stdout, "kv_capacity_required: true\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_context_positions: unknown\n");
    yvex_cli_out_writef(stdout, "kv_context_ready: false\n");
    yvex_cli_out_writef(stdout, "real_attention_kv_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_context_status: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "bounded_generation_context_policy: diagnostic-only-not-run\n");
    yvex_cli_out_writef(stdout, "full_generation_context_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "context_blockers: source-only target has no YVEX-produced GGUF tensor inventory; GLM context mapping planned; real transformer prefill unsupported\n");
    yvex_cli_out_writef(stdout, "next_required_rows: MOE.CLASS.0,source-manifest-context-profile,YVEX-produced-GGUF,real-transformer-prefill,real-decode,GEN.DEEPSEEK.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    context_print_phases("unsupported", "unsupported", "unsupported", "unsupported", "context-profile");
}

static int context_print_unsupported_family(const yvex_cli_context_options *options,
                                            const char *resolved_path,
                                            const char *target_id,
                                            const char *target_class,
                                            const char *detected,
                                            const char *reason)
{
    context_print_common_header(options,
                                "context-report-unsupported",
                                resolved_path,
                                target_id,
                                target_class,
                                context_requested_family(options),
                                detected ? detected : "unknown",
                                "unsupported",
                                "unsupported",
                                "unsupported");
    yvex_cli_out_writef(stdout, "model_max_context: unknown\n");
    yvex_cli_out_writef(stdout, "model_max_context_source: unsupported-family\n");
    yvex_cli_out_writef(stdout, "model_max_context_status: unsupported\n");
    yvex_cli_out_writef(stdout, "requested_context: not-requested\n");
    yvex_cli_out_writef(stdout, "requested_context_source: none\n");
    yvex_cli_out_writef(stdout, "requested_context_status: not-requested\n");
    yvex_cli_out_writef(stdout, "active_context: unsupported\n");
    yvex_cli_out_writef(stdout, "active_context_source: unsupported-family\n");
    yvex_cli_out_writef(stdout, "active_context_status: unsupported\n");
    yvex_cli_out_writef(stdout, "token_input_status: not-performed\n");
    yvex_cli_out_writef(stdout, "token_count: 0\n");
    yvex_cli_out_writef(stdout, "prompt_token_count: 0\n");
    yvex_cli_out_writef(stdout, "prefill_token_count: 0\n");
    yvex_cli_out_writef(stdout, "generated_token_count: 0\n");
    yvex_cli_out_writef(stdout, "token_range_start: none\n");
    yvex_cli_out_writef(stdout, "token_range_end: none\n");
    yvex_cli_out_writef(stdout, "chunking_required: false\n");
    yvex_cli_out_writef(stdout, "chunk_size: not-requested\n");
    yvex_cli_out_writef(stdout, "chunk_count: 0\n");
    yvex_cli_out_writef(stdout, "last_chunk_size: 0\n");
    yvex_cli_out_writef(stdout, "chunking_policy: unsupported-family\n");
    yvex_cli_out_writef(stdout, "chunking_status: unsupported\n");
    yvex_cli_out_writef(stdout, "prefill_boundary_status: unsupported\n");
    yvex_cli_out_writef(stdout, "prefill_position_start: unknown\n");
    yvex_cli_out_writef(stdout, "prefill_position_end: unknown\n");
    yvex_cli_out_writef(stdout, "prefill_context_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_position_policy: unsupported-family\n");
    yvex_cli_out_writef(stdout, "decode_start_position: unknown\n");
    yvex_cli_out_writef(stdout, "decode_position_status: unsupported\n");
    yvex_cli_out_writef(stdout, "decode_context_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");
    yvex_cli_out_writef(stdout, "overflow_policy: unsupported-family\n");
    yvex_cli_out_writef(stdout, "context_overflow: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_check_status: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_stop_reason: unsupported\n");
    yvex_cli_out_writef(stdout, "overflow_mutates_state: false\n");
    yvex_cli_out_writef(stdout, "attention_dependency_status: unsupported-family\n");
    yvex_cli_out_writef(stdout, "attention_context_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_position_policy: unsupported\n");
    yvex_cli_out_writef(stdout, "rope_context_status: unsupported\n");
    yvex_cli_out_writef(stdout, "mask_context_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_dependency_status: unsupported-family\n");
    yvex_cli_out_writef(stdout, "kv_capacity_required: unknown\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: unsupported\n");
    yvex_cli_out_writef(stdout, "kv_context_positions: unknown\n");
    yvex_cli_out_writef(stdout, "kv_context_ready: false\n");
    yvex_cli_out_writef(stdout, "real_attention_kv_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_context_status: unsupported-full-model\n");
    yvex_cli_out_writef(stdout, "bounded_generation_context_policy: diagnostic-only-not-run\n");
    yvex_cli_out_writef(stdout, "full_generation_context_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "context_blockers: %s\n", reason ? reason : "unsupported context family");
    yvex_cli_out_writef(stdout, "next_required_rows: CONTEXT.CLASS.0,MOE.CLASS.0,real-transformer-prefill,real-decode,GEN.DEEPSEEK.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    context_print_phases("unsupported", "unsupported", "unsupported", "unsupported", "resolve-family");
    yvex_cli_out_writef(stdout, "reason: %s\n", reason ? reason : "unsupported context family");
    return 5;
}

static int context_print_report(const yvex_cli_context_options *options,
                                yvex_model_ref *ref,
                                yvex_cli_tokenizer_context *ctx,
                                const char *target_id,
                                const char *target_class,
                                yvex_arch arch,
                                const yvex_fullmodel_collections *collections,
                                int selected_target)
{
    yvex_cli_fullmodel_options family_probe;
    yvex_token_input input;
    yvex_error err;
    const char *requested = context_requested_family(options);
    const char *detected;
    const char *source = "unknown";
    const char *coverage_status;
    unsigned long long model_max_context;
    unsigned long long active_context = 0ull;
    unsigned long long token_count = 0ull;
    unsigned long long chunk_count = 0ull;
    unsigned long long last_chunk = 0ull;
    unsigned long long decode_start = 0ull;
    int overflow_known = 0;
    int overflow = 0;
    int has_qkv;
    int rc;

    memset(&family_probe, 0, sizeof(family_probe));
    memset(&input, 0, sizeof(input));
    yvex_error_clear(&err);

    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : "auto";
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) ||
        strcmp(detected, "deepseek") != 0) {
        return context_print_unsupported_family(options,
                                                ref && ref->path ? ref->path : "",
                                                target_id,
                                                target_class,
                                                detected,
                                                "context report supports DeepSeek-family GGUF artifacts first");
    }

    model_max_context = context_metadata_max_context(ctx, &source);
    coverage_status = context_status_from_collections(collections, selected_target);
    has_qkv = collections &&
              collections->has_attention_q &&
              collections->has_attention_k &&
              collections->has_attention_v;

    if (options && options->tokens_text) {
        rc = yvex_token_input_parse_explicit(options->tokens_text, &input, &err);
        if (rc != YVEX_OK) {
            context_print_common_header(options,
                                        "context-report-fail",
                                        ref && ref->path ? ref->path : "",
                                        target_id,
                                        target_class,
                                        "deepseek",
                                        detected,
                                        coverage_status,
                                        coverage_status,
                                        coverage_status);
            yvex_cli_out_writef(stdout, "token_input_status: failed\n");
            yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
            yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
            yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
            context_print_phases(coverage_status, coverage_status, coverage_status, "failed", "token-input");
            return exit_for_status(rc);
        }
        token_count = input.token_count;
    }

    if (options && options->context_length_seen) {
        active_context = options->context_length;
        overflow_known = 1;
    } else if (model_max_context > 0ull && !selected_target) {
        active_context = model_max_context;
        overflow_known = 1;
    } else {
        active_context = 0ull;
    }
    if (overflow_known && token_count > active_context) {
        overflow = 1;
    }
    if (options && options->chunk_size_seen && token_count > 0ull) {
        chunk_count = (token_count + options->chunk_size - 1ull) / options->chunk_size;
        last_chunk = token_count % options->chunk_size;
        if (last_chunk == 0ull) last_chunk = options->chunk_size;
    }
    decode_start = token_count;

    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "report: context\n");
        yvex_cli_out_writef(stdout, "model: %s\n", options->model ? options->model : "");
        yvex_cli_out_writef(stdout, "family: deepseek\n");
        yvex_cli_out_writef(stdout, "status: %s\n", coverage_status ? coverage_status : "report-only");
        yvex_cli_out_writef(stdout, "top_blocker: %s\n",
               selected_target
                   ? "selected-slice context metadata incomplete"
                   : "full transformer prefill unsupported");
        yvex_cli_out_writef(stdout, "next: V010.CONTEXT.*\n");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return 0;
    }

    context_print_common_header(options,
                                "context-report",
                                ref && ref->path ? ref->path : "",
                                target_id,
                                target_class,
                                "deepseek",
                                detected,
                                coverage_status,
                                coverage_status,
                                coverage_status);
    yvex_cli_out_writef(stdout, "report_options.include_attention: %s\n", options && options->include_attention ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_kv: %s\n", options && options->include_kv ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_prefill: %s\n", options && options->include_prefill ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_decode: %s\n", options && options->include_decode ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_blockers: %s\n", options && options->include_blockers ? "true" : "false");

    yvex_cli_out_writef(stdout, "model_max_context: ");
    if (model_max_context > 0ull) yvex_cli_out_writef(stdout, "%llu", model_max_context);
    else yvex_cli_out_writef(stdout, "unknown");
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "model_max_context_source: %s\n", model_max_context > 0ull ? source : "metadata-missing-or-selected-slice");
    yvex_cli_out_writef(stdout, "model_max_context_status: %s\n", model_max_context > 0ull ? "available" : "planned");
    yvex_cli_out_writef(stdout, "requested_context: ");
    if (options && options->context_length_seen) yvex_cli_out_writef(stdout, "%llu", options->context_length);
    else yvex_cli_out_writef(stdout, "not-requested");
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "requested_context_source: %s\n", options && options->context_length_seen ? "operator-request" : "none");
    yvex_cli_out_writef(stdout, "requested_context_status: %s\n", options && options->context_length_seen ? "reported-only" : "not-requested");
    yvex_cli_out_writef(stdout, "active_context: ");
    if (active_context > 0ull) yvex_cli_out_writef(stdout, "%llu", active_context);
    else yvex_cli_out_writef(stdout, "diagnostic");
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "active_context_source: %s\n",
           options && options->context_length_seen ? "operator-request" :
           (model_max_context > 0ull && !selected_target ? "model-metadata" : "diagnostic-runtime-boundary"));
    yvex_cli_out_writef(stdout, "active_context_status: %s\n", active_context > 0ull ? "reported-only" : "diagnostic");

    yvex_cli_out_writef(stdout, "token_input_status: %s\n", options && options->tokens_text ? "available" : "not-provided");
    yvex_cli_out_writef(stdout, "token_count: %llu\n", token_count);
    yvex_cli_out_writef(stdout, "prompt_token_count: %llu\n", token_count);
    yvex_cli_out_writef(stdout, "prefill_token_count: %llu\n", token_count);
    yvex_cli_out_writef(stdout, "generated_token_count: 0\n");
    yvex_cli_out_writef(stdout, "token_range_start: %s\n", token_count > 0ull ? "0" : "none");
    yvex_cli_out_writef(stdout, "token_range_end: ");
    if (token_count > 0ull) yvex_cli_out_writef(stdout, "%llu", token_count - 1ull);
    else yvex_cli_out_writef(stdout, "none");
    yvex_cli_out_writef(stdout, "\n");

    yvex_cli_out_writef(stdout, "chunking_required: %s\n", options && options->chunk_size_seen ? "true" : "false");
    yvex_cli_out_writef(stdout, "chunk_size: ");
    if (options && options->chunk_size_seen) yvex_cli_out_writef(stdout, "%llu", options->chunk_size);
    else yvex_cli_out_writef(stdout, "not-requested");
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "chunk_count: %llu\n", chunk_count);
    yvex_cli_out_writef(stdout, "last_chunk_size: %llu\n", last_chunk);
    yvex_cli_out_writef(stdout, "chunking_policy: %s\n", options && options->chunk_size_seen ? "explicit-token-chunks-report-only" : "not-requested");
    yvex_cli_out_writef(stdout, "chunking_status: %s\n", options && options->chunk_size_seen ? "report-only" : "planned");

    yvex_cli_out_writef(stdout, "prefill_boundary_status: report-only\n");
    yvex_cli_out_writef(stdout, "prefill_position_start: %s\n", token_count > 0ull ? "0" : "none");
    yvex_cli_out_writef(stdout, "prefill_position_end: %llu\n", token_count);
    yvex_cli_out_writef(stdout, "prefill_context_ready: false\n");
    yvex_cli_out_writef(stdout, "full_transformer_prefill_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_position_policy: prefill-end-token-count\n");
    yvex_cli_out_writef(stdout, "decode_start_position: %llu\n", decode_start);
    yvex_cli_out_writef(stdout, "decode_position_status: report-only\n");
    yvex_cli_out_writef(stdout, "decode_context_ready: false\n");
    yvex_cli_out_writef(stdout, "decode_ready: false\n");

    yvex_cli_out_writef(stdout, "overflow_policy: bounded-diagnostic-refuse-before-mutation\n");
    yvex_cli_out_writef(stdout, "context_overflow: %s\n", overflow_known ? (overflow ? "overflow" : "none") : "unknown");
    yvex_cli_out_writef(stdout, "overflow_check_status: %s\n", overflow_known ? "pass" : "unknown");
    yvex_cli_out_writef(stdout, "overflow_stop_reason: %s\n", overflow ? "context-limit" : "none");
    yvex_cli_out_writef(stdout, "overflow_mutates_state: false\n");

    yvex_cli_out_writef(stdout, "attention_dependency_status: %s\n", has_qkv ? "blocked-runtime-integration" : "blocked-missing-qkv");
    yvex_cli_out_writef(stdout, "attention_context_ready: false\n");
    yvex_cli_out_writef(stdout, "attention_position_policy: rope-or-family-specific-planned\n");
    yvex_cli_out_writef(stdout, "rope_context_status: planned\n");
    yvex_cli_out_writef(stdout, "mask_context_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_dependency_status: %s\n", has_qkv ? "blocked-runtime-integration" : "blocked-missing-qkv");
    yvex_cli_out_writef(stdout, "kv_capacity_required: true\n");
    yvex_cli_out_writef(stdout, "kv_capacity_status: planned\n");
    yvex_cli_out_writef(stdout, "kv_context_positions: %s", active_context > 0ull ? "" : "unknown");
    if (active_context > 0ull) yvex_cli_out_writef(stdout, "%llu", active_context);
    yvex_cli_out_writef(stdout, "\n");
    yvex_cli_out_writef(stdout, "kv_context_ready: false\n");
    yvex_cli_out_writef(stdout, "real_attention_kv_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_context_status: bounded-diagnostic-only\n");
    yvex_cli_out_writef(stdout, "bounded_generation_context_policy: context-limit-pre-append\n");
    yvex_cli_out_writef(stdout, "full_generation_context_ready: false\n");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "context_blockers: %s\n",
           selected_target
               ? "selected runtime slice has no full context metadata, no real attention KV, no full transformer prefill, no real decode, no full generation"
               : "full transformer prefill unsupported; real attention-backed KV unsupported; real decode unsupported; full generation unsupported");
    yvex_cli_out_writef(stdout, "next_required_rows: MOE.CLASS.0,RUNTIME.KV.1,real-transformer-prefill,real-decode,real-output-head-logits,GEN.DEEPSEEK.0\n");
    yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
    yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
    context_print_phases(coverage_status, coverage_status, coverage_status, "report-only", NULL);
    return 0;
}

int yvex_model_artifacts_surface_context_command(int arg_count, char **args)
{
    yvex_cli_context_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref_options, 0, sizeof(ref_options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_context_options(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        context_print_unsupported_source_only(&options);
        return 5;
    }

    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        context_print_common_header(&options,
                                    "context-report-fail",
                                    "",
                                    "unknown",
                                    "unknown",
                                    "unknown",
                                    "unknown",
                                    "failed",
                                    "failed",
                                    "failed");
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
        yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
        context_print_phases("failed", "failed", "failed", "failed", "resolve-model");
        yvex_error_clear(&err);
        return exit_for_status(rc);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        context_print_common_header(&options,
                                    "context-report-fail",
                                    ref.path,
                                    ref.alias && ref.alias[0] ? ref.alias : "path",
                                    "GGUF-artifact",
                                    "unknown",
                                    "unknown",
                                    "failed",
                                    "failed",
                                    "failed");
        yvex_cli_out_writef(stdout, "reason: %s\n", yvex_error_message(&err));
        yvex_cli_out_writef(stdout, "cleanup_attempted: true\n");
        yvex_cli_out_writef(stdout, "cleanup_status: pass\n");
        context_print_phases("failed", "failed", "failed", "failed", "context-profile");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return exit_for_status(rc);
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (tensor) fullmodel_classify_tensor(tensor, &collections);
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
    rc = context_print_report(&options,
                              &ref,
                              &ctx,
                              target_id,
                              target_class,
                              arch,
                              &collections,
                              selected_target);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_model_artifacts_surface_context_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex context report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--" "audit | --" "output normal|table|audit] [options]\n\n");
    yvex_cli_out_writef(fp, "Examples:\n");
    yvex_cli_out_writef(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3\n");
    yvex_cli_out_writef(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3 --chunk-size 2\n");
    yvex_cli_out_writef(fp, "  yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3 --context-length 2\n\n");
    yvex_cli_out_writef(fp, "Options: --registry FILE --context-length N --tokens IDS --chunk-size N --" "include-attention --" "include-kv --" "include-prefill --" "include-decode --" "include-blockers\n\n");
    yvex_cli_out_writef(fp, "context report:\n");
    yvex_cli_out_writef(fp, "  report-only boundary for model/requested/active context, token counts, chunking policy, overflow behavior, prefill boundary, decode position policy, attention dependency, KV dependency, and runtime blockers.\n");
    yvex_cli_out_writef(fp, "  Default output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "  It reports bounded diagnostic context behavior separately from full model context readiness.\n");
    yvex_cli_out_writef(fp, "  It does not run full transformer prefill, does not execute real decode, does not write real attention-backed KV, does not generate, does not evaluate, does not benchmark, and does not report throughput.\n");
    yvex_cli_out_writef(fp, "Boundary: no long-context runtime support, no context extension support, no full model execution, no DeepSeek generation, no provider generation, no streaming generation, no eval, no benchmark.\n");
}
