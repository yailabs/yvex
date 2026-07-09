/*
 * yvex_moe_surface.c - moe command-family CLI surface.
 * Owner: src/cli/model_artifacts
 * Owns: existing moe command-family parsing and output behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing command behavior.
 * Boundary: moe reports are report-only diagnostics.
 */
#include "yvex_moe_surface.h"

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *registry_path;
    int include_tensors;
    int include_residency;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_moe_options;

typedef struct {
    const char *status;
    const char *target_id;
    const char *target_class;
    const char *model;
    const char *model_resolved_path;
    const char *family;
    const char *family_detected;
    const char *family_requested;
    const char *backend;
    const char *source_class;
    const char *artifact_class;
    const char *implementation_stage;
    const char *model_is_moe;
    const char *moe_class_status;
    const char *expert_count_status;
    unsigned long long expert_count;
    const char *active_expert_count_status;
    unsigned long long active_expert_count;
    const char *router_status;
    const char *router_tensor_status;
    const char *router_tensor_name;
    const char *router_dtype;
    const char *router_logits_status;
    const char *top_k_policy_status;
    unsigned long long top_k;
    const char *shared_expert_status;
    unsigned long long shared_expert_count;
    const char *expert_tensor_collection_status;
    const char *expert_tensor_roles;
    unsigned long long expert_tensor_count;
    const char *expert_qtype_summary;
    const char *expert_storage_pressure;
    const char *expert_residency_pressure;
    const char *expert_dispatch_status;
    const char *expert_activation_status;
    const char *expert_accumulation_status;
    const char *prefill_integration_status;
    const char *decode_integration_status;
    const char *graph_integration_status;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    const char *blockers;
    const char *next_required_rows;
    yvex_models_output_mode output_mode;
    int include_tensors;
    int include_residency;
    int include_blockers;
} yvex_moe_class_report;

static int moe_parse_value_option(const char *flag,
                                  int arg_count,
                                  char **args,
                                  int *index,
                                  const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: moe %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: moe %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_moe_options(int arg_count,
                             char **args,
                             yvex_cli_moe_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_moe_help(stdout);
        return 1;
    }
    if (arg_count < 3 || strcmp(args[2], "report") != 0) {
        yvex_cli_out_writef(stderr, "yvex: moe requires report\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex moe report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--" "include-tensors] [--" "include-residency] [--" "include-blockers]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--model") == 0) {
            int rc = moe_parse_value_option("--model", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(args[i], "--backend") == 0) {
            int rc = moe_parse_value_option("--backend", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(value, "cpu") != 0 && strcmp(value, "cuda") != 0) {
                yvex_cli_out_writef(stderr, "yvex: moe --backend must be cpu or cuda\n");
                return 2;
            }
            options->backend = value;
        } else if (strcmp(args[i], "--family") == 0) {
            int rc = moe_parse_value_option("--family", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(args[i], "--registry") == 0) {
            int rc = moe_parse_value_option("--registry", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(args[i], "--" "include-tensors") == 0) {
            options->include_tensors = 1;
        } else if (strcmp(args[i], "--" "include-residency") == 0) {
            options->include_residency = 1;
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            int rc = moe_parse_value_option("--" "output", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: moe unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_model_artifacts_surface_moe_help(stdout);
            return 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown moe option: %s\n", args[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        yvex_cli_out_writef(stderr, "yvex: moe report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *moe_requested_family(const yvex_cli_moe_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static void moe_init_report(yvex_moe_class_report *report,
                            const yvex_cli_moe_options *options)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = "internal-error";
    report->target_id = "unknown";
    report->target_class = "unknown";
    report->model = options && options->model ? options->model : "";
    report->model_resolved_path = "unknown";
    report->family = moe_requested_family(options);
    report->family_detected = "unknown";
    report->family_requested = moe_requested_family(options);
    report->backend = options && options->backend ? options->backend : "cpu";
    report->source_class = "unknown";
    report->artifact_class = "unknown";
    report->implementation_stage = "report-only";
    report->model_is_moe = "unknown";
    report->moe_class_status = "unknown";
    report->expert_count_status = "unknown";
    report->active_expert_count_status = "unknown";
    report->router_status = "unknown";
    report->router_tensor_status = "unknown";
    report->router_tensor_name = "none";
    report->router_dtype = "unknown";
    report->router_logits_status = "unsupported";
    report->top_k_policy_status = "unknown";
    report->shared_expert_status = "unknown";
    report->expert_tensor_collection_status = "unknown";
    report->expert_tensor_roles = "unknown";
    report->expert_qtype_summary = "unknown";
    report->expert_storage_pressure = "unknown";
    report->expert_residency_pressure = "unknown";
    report->expert_dispatch_status = "unsupported";
    report->expert_activation_status = "unsupported";
    report->expert_accumulation_status = "unsupported";
    report->prefill_integration_status = "unsupported";
    report->decode_integration_status = "unsupported";
    report->graph_integration_status = "unsupported";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported-full-model";
    report->benchmark_status = "not-measured";
    report->blockers = "MoE model-class report failed before blocker classification";
    report->next_required_rows = "V010.CLASS.3";
    report->output_mode = options ? options->output_mode : YVEX_MODELS_OUTPUT_AUDIT;
    report->include_tensors = options ? options->include_tensors : 0;
    report->include_residency = options ? options->include_residency : 0;
    report->include_blockers = options ? options->include_blockers : 0;
}

static void moe_print_report(const yvex_moe_class_report *report)
{
    if (report && report->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "report: moe\n");
        yvex_cli_out_writef(stdout, "model: %s\n", report->model ? report->model : "");
        yvex_cli_out_writef(stdout, "family: %s\n", report->family ? report->family : "unknown");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status ? report->status : "report-only");
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", report->blockers ? report->blockers : "router/expert runtime unsupported");
        yvex_cli_out_writef(stdout, "next: %s\n", report->next_required_rows ? report->next_required_rows : "V010.MOE.*");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return;
    }

    yvex_cli_out_writef(stdout, "moe: report\n");
    yvex_cli_out_writef(stdout, "status: %s\n", report && report->status ? report->status : "internal-error");
    yvex_cli_out_writef(stdout, "target_id: %s\n", report && report->target_id ? report->target_id : "unknown");
    yvex_cli_out_writef(stdout, "target_class: %s\n", report && report->target_class ? report->target_class : "unknown");
    yvex_cli_out_writef(stdout, "model: %s\n", report && report->model ? report->model : "");
    yvex_cli_out_writef(stdout, "model_resolved_path: %s\n", report && report->model_resolved_path ? report->model_resolved_path : "unknown");
    yvex_cli_out_writef(stdout, "family: %s\n", report && report->family ? report->family : "unknown");
    yvex_cli_out_writef(stdout, "family_detected: %s\n", report && report->family_detected ? report->family_detected : "unknown");
    yvex_cli_out_writef(stdout, "family_requested: %s\n", report && report->family_requested ? report->family_requested : "auto");
    yvex_cli_out_writef(stdout, "backend: %s\n", report && report->backend ? report->backend : "cpu");
    yvex_cli_out_writef(stdout, "source_class: %s\n", report && report->source_class ? report->source_class : "unknown");
    yvex_cli_out_writef(stdout, "artifact_class: %s\n", report && report->artifact_class ? report->artifact_class : "unknown");
    yvex_cli_out_writef(stdout, "implementation_stage: %s\n", report && report->implementation_stage ? report->implementation_stage : "report-only");
    yvex_cli_out_writef(stdout, "model_is_moe: %s\n", report && report->model_is_moe ? report->model_is_moe : "unknown");
    yvex_cli_out_writef(stdout, "moe_class_status: %s\n", report && report->moe_class_status ? report->moe_class_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_count_status: %s\n", report && report->expert_count_status ? report->expert_count_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_count: %llu\n", report ? report->expert_count : 0ull);
    yvex_cli_out_writef(stdout, "active_expert_count_status: %s\n", report && report->active_expert_count_status ? report->active_expert_count_status : "unknown");
    yvex_cli_out_writef(stdout, "active_expert_count: %llu\n", report ? report->active_expert_count : 0ull);
    yvex_cli_out_writef(stdout, "router_status: %s\n", report && report->router_status ? report->router_status : "unknown");
    yvex_cli_out_writef(stdout, "router_tensor_status: %s\n", report && report->router_tensor_status ? report->router_tensor_status : "unknown");
    yvex_cli_out_writef(stdout, "router_tensor_name: %s\n", report && report->router_tensor_name ? report->router_tensor_name : "none");
    yvex_cli_out_writef(stdout, "router_dtype: %s\n", report && report->router_dtype ? report->router_dtype : "unknown");
    yvex_cli_out_writef(stdout, "router_logits_status: %s\n", report && report->router_logits_status ? report->router_logits_status : "unsupported");
    yvex_cli_out_writef(stdout, "top_k_policy_status: %s\n", report && report->top_k_policy_status ? report->top_k_policy_status : "unknown");
    yvex_cli_out_writef(stdout, "top_k: %llu\n", report ? report->top_k : 0ull);
    yvex_cli_out_writef(stdout, "shared_expert_status: %s\n", report && report->shared_expert_status ? report->shared_expert_status : "unknown");
    yvex_cli_out_writef(stdout, "shared_expert_count: %llu\n", report ? report->shared_expert_count : 0ull);
    yvex_cli_out_writef(stdout, "expert_tensor_collection_status: %s\n", report && report->expert_tensor_collection_status ? report->expert_tensor_collection_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_roles: %s\n", report && report->expert_tensor_roles ? report->expert_tensor_roles : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_count: %llu\n", report ? report->expert_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "expert_qtype_summary: %s\n", report && report->expert_qtype_summary ? report->expert_qtype_summary : "unknown");
    yvex_cli_out_writef(stdout, "expert_storage_pressure: %s\n", report && report->expert_storage_pressure ? report->expert_storage_pressure : "unknown");
    yvex_cli_out_writef(stdout, "expert_residency_pressure: %s\n", report && report->expert_residency_pressure ? report->expert_residency_pressure : "unknown");
    yvex_cli_out_writef(stdout, "expert_dispatch_status: %s\n", report && report->expert_dispatch_status ? report->expert_dispatch_status : "unsupported");
    yvex_cli_out_writef(stdout, "expert_activation_status: %s\n", report && report->expert_activation_status ? report->expert_activation_status : "unsupported");
    yvex_cli_out_writef(stdout, "expert_accumulation_status: %s\n", report && report->expert_accumulation_status ? report->expert_accumulation_status : "unsupported");
    yvex_cli_out_writef(stdout, "prefill_integration_status: %s\n", report && report->prefill_integration_status ? report->prefill_integration_status : "unsupported");
    yvex_cli_out_writef(stdout, "decode_integration_status: %s\n", report && report->decode_integration_status ? report->decode_integration_status : "unsupported");
    yvex_cli_out_writef(stdout, "graph_integration_status: %s\n", report && report->graph_integration_status ? report->graph_integration_status : "unsupported");
    yvex_cli_out_writef(stdout, "runtime_claim: %s\n", report && report->runtime_claim ? report->runtime_claim : "unsupported");
    yvex_cli_out_writef(stdout, "generation: %s\n", report && report->generation ? report->generation : "unsupported-full-model");
    yvex_cli_out_writef(stdout, "benchmark_status: %s\n", report && report->benchmark_status ? report->benchmark_status : "not-measured");
    yvex_cli_out_writef(stdout, "blockers: %s\n", report && report->blockers ? report->blockers : "unknown");
    yvex_cli_out_writef(stdout, "next_required_rows: %s\n", report && report->next_required_rows ? report->next_required_rows : "V010.CLASS.3");
    yvex_cli_out_writef(stdout, "report_options.include_tensors: %s\n", report && report->include_tensors ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_residency: %s\n", report && report->include_residency ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_blockers: %s\n", report && report->include_blockers ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
}

static int moe_print_source_only_report(const yvex_cli_moe_options *options,
                                        const char *target)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "unsupported-source-only";
    report.target_id = target ? target : "glm-5.2-official-safetensors";
    report.target_class = "huge-source-pressure";
    report.model_resolved_path = "source-only-target";
    report.family = "glm";
    report.family_detected = "glm";
    report.source_class = "official safetensors";
    report.artifact_class = "future YVEX-produced GGUF";
    report.model_is_moe = "true";
    report.moe_class_status = "source-only";
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.router_status = "unsupported";
    report.router_tensor_status = "unsupported";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.shared_expert_status = "unknown";
    report.expert_tensor_collection_status = "unsupported";
    report.expert_tensor_roles = "source-only-not-inspected";
    report.expert_qtype_summary = "unknown";
    report.expert_storage_pressure = "source-only-huge-pressure";
    report.expert_residency_pressure = "source-only-huge-pressure";
    report.blockers = "source-only target has no YVEX-produced GGUF tensor inventory; GLM runtime unsupported";
    report.next_required_rows = "OWI.HUGE.0,MODEL.CLASS.3,TENSOR.COLLECTION.2,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,GLM-YVEX-produced-GGUF";
    moe_print_report(&report);
    return 5;
}

static int moe_print_missing_model_report(const yvex_cli_moe_options *options,
                                          const char *reason)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "missing-model";
    report.target_id = options && options->model ? options->model : "missing";
    report.target_class = "unknown";
    report.model_resolved_path = "missing";
    report.family_detected = "unknown";
    report.source_class = "unknown";
    report.artifact_class = "unknown";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unknown";
    report.router_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_tensor_collection_status = "unknown";
    report.expert_tensor_roles = "unknown";
    report.blockers = reason && reason[0] ? reason : "model or alias could not be resolved";
    report.next_required_rows = "V010.TARGET.9,V010.CLASS.3";
    moe_print_report(&report);
    return 5;
}

static int moe_print_unsupported_family_report(const yvex_cli_moe_options *options,
                                               yvex_model_ref *ref,
                                               const char *target_id,
                                               const char *target_class,
                                               const char *detected)
{
    yvex_moe_class_report report;

    moe_init_report(&report, options);
    report.status = "unsupported-family";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = moe_requested_family(options);
    report.family_detected = detected ? detected : "unknown";
    report.source_class = "GGUF artifact";
    report.artifact_class = "GGUF artifact";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unsupported-family";
    report.router_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_tensor_collection_status = "unknown";
    report.expert_tensor_roles = "unknown";
    report.blockers = "requested family is not supported by MoE model-class report";
    report.next_required_rows = "V010.CLASS.0,V010.CLASS.3,family-specific-MoE-adapter";
    moe_print_report(&report);
    return 5;
}

static void moe_append_role(char *out, size_t out_cap, const char *role)
{
    size_t used;

    if (!out || out_cap == 0u || !role || !role[0]) return;
    used = strlen(out);
    if (used > 0u) {
        if (used + 1u >= out_cap) return;
        out[used++] = ',';
        out[used] = '\0';
    }
    snprintf(out + used, used < out_cap ? out_cap - used : 0u, "%s", role);
}

static int moe_print_model_report(const yvex_cli_moe_options *options,
                                  yvex_model_ref *ref,
                                  yvex_cli_tokenizer_context *ctx,
                                  const char *target_id,
                                  const char *target_class,
                                  yvex_arch arch,
                                  const yvex_fullmodel_collections *collections)
{
    yvex_moe_class_report report;
    yvex_cli_fullmodel_options family_probe;
    const yvex_tensor_info *router;
    const yvex_tensor_info *expert_gate;
    const yvex_tensor_info *expert_up;
    const yvex_tensor_info *expert_down;
    const char *requested;
    const char *detected;
    char expert_roles[160];
    char expert_qtypes[192];
    unsigned int present_roles = 0u;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : NULL;
    requested = moe_requested_family(options);
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) || strcmp(detected, "deepseek") != 0) {
        return moe_print_unsupported_family_report(options, ref, target_id, target_class, detected);
    }

    router = fullmodel_descriptor_find_tensor(ctx, "moe_router");
    expert_gate = fullmodel_descriptor_find_tensor(ctx, "moe_expert_gate");
    expert_up = fullmodel_descriptor_find_tensor(ctx, "moe_expert_up");
    expert_down = fullmodel_descriptor_find_tensor(ctx, "moe_expert_down");

    expert_roles[0] = '\0';
    expert_qtypes[0] = '\0';
    if (expert_gate) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-gate");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-gate:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_gate->dtype));
    }
    if (expert_up) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-up");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-up:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_up->dtype));
    }
    if (expert_down) {
        present_roles++;
        moe_append_role(expert_roles, sizeof(expert_roles), "moe-expert-down");
        snprintf(expert_qtypes + strlen(expert_qtypes),
                 strlen(expert_qtypes) < sizeof(expert_qtypes) ? sizeof(expert_qtypes) - strlen(expert_qtypes) : 0u,
                 "%smoe-expert-down:%s",
                 expert_qtypes[0] ? "," : "",
                 yvex_dtype_name(expert_down->dtype));
    }
    if (!expert_roles[0]) snprintf(expert_roles, sizeof(expert_roles), "missing");
    if (!expert_qtypes[0]) snprintf(expert_qtypes, sizeof(expert_qtypes), "unknown");

    moe_init_report(&report, options);
    report.status = "ok-partial";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = "deepseek";
    report.family_detected = detected;
    report.family_requested = requested;
    report.source_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                              ? "YVEX-produced selected GGUF"
                              : "GGUF artifact";
    report.artifact_class = strcmp(report.target_class, "selected-runtime-slice") == 0
                                ? "YVEX-produced selected GGUF"
                                : "GGUF artifact";
    report.model_is_moe = "true";
    report.moe_class_status = "partial";
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.router_status = router ? "known" : "missing";
    report.router_tensor_status = router ? "known" : "missing";
    report.router_tensor_name = router && router->name ? router->name : "none";
    report.router_dtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.shared_expert_status = "unknown";
    report.expert_tensor_collection_status = present_roles == 3u ? "known" :
                                             (present_roles > 0u ? "partial" : "missing");
    report.expert_tensor_roles = expert_roles;
    report.expert_tensor_count = present_roles;
    report.expert_qtype_summary = expert_qtypes;
    report.expert_storage_pressure = present_roles > 0u ? "planned" : "missing";
    report.expert_residency_pressure = present_roles > 0u ? "planned" : "missing";
    report.blockers = router
                          ? "router tensor is classified but router logits, top-k routing, expert activation, dispatch, accumulation, graph integration, prefill integration, and decode integration are unsupported"
                          : "router tensor missing; expert tensor collection missing; router logits unsupported; top-k routing unsupported; expert activation unsupported; expert dispatch unsupported; expert accumulation unsupported";
    report.next_required_rows = router || present_roles > 0u
                                    ? "V010.MOE.4,V010.MOE.5,V010.MOE.6,V010.MOE.7,V010.STORAGE.18,V010.RESIDENCY.14"
                                    : "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,V010.MOE.4";
    if (collections && collections->has_moe_expert && present_roles == 0u) {
        report.expert_tensor_collection_status = "partial";
        report.expert_tensor_count = collections->moe;
        report.expert_tensor_roles = "expert-pattern-detected";
        report.expert_storage_pressure = "planned";
        report.expert_residency_pressure = "planned";
    }
    moe_print_report(&report);
    return 0;
}

int yvex_model_artifacts_surface_moe_command(int arg_count, char **args)
{
    yvex_cli_moe_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_fullmodel_collections collections;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    unsigned long long tensor_count;
    unsigned long long i;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&collections, 0, sizeof(collections));

    rc = parse_moe_options(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return moe_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&err);
        return moe_print_missing_model_report(&options, "model or alias could not be resolved");
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = moe_print_missing_model_report(&options, "resolved artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        const char *reason = yvex_error_message(&err);
        rc = moe_print_missing_model_report(&options, reason && reason[0] ? reason : "GGUF metadata or tensor directory parse failed");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    tensor_count = yvex_tensor_table_count(ctx.table);
    for (i = 0; i < tensor_count; ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) continue;
        fullmodel_classify_tensor(tensor, &collections);
    }
    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = moe_print_model_report(&options,
                                &ref,
                                &ctx,
                                target_id,
                                target_class,
                                arch,
                                &collections);
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_model_artifacts_surface_moe_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex moe report --model FILE_OR_ALIAS [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--" "audit | --" "output normal|table|audit] [--" "include-tensors] [--" "include-residency] [--" "include-blockers]\n");
    yvex_cli_out_writef(fp, "\nExamples:\n");
    yvex_cli_out_writef(fp, "  yvex moe report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --" "include-tensors --" "include-blockers\n");
    yvex_cli_out_writef(fp, "  yvex moe report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cuda --" "include-residency\n");
    yvex_cli_out_writef(fp, "  yvex moe report --model glm-5.2-official-safetensors --family glm --backend cpu --" "include-blockers\n");
    yvex_cli_out_writef(fp, "\nmoe report:\n");
    yvex_cli_out_writef(fp, "  classifies the model as MoE/source-only/unsupported-family and reports router facts, expert tensor-role facts, shared-expert facts, storage and residency pressure, blockers, and next rows.\n");
    yvex_cli_out_writef(fp, "  Default output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "  report-only boundary: it does not execute router logits, does not perform top-k routing, does not activate experts, does not dispatch experts, does not accumulate expert outputs, does not integrate MoE into graph/prefill/decode, does not generate, and does not benchmark.\n");
    yvex_cli_out_writef(fp, "  selected-runtime-slice targets may return ok-partial when the family is MoE but router or expert tensors are not present in the selected artifact.\n");
    yvex_cli_out_writef(fp, "  source-only targets are reported without opening huge source shards or downloading artifacts.\n");
    yvex_cli_out_writef(fp, "Boundary: no MoE runtime execution, no full transformer block, no full transformer prefill, no attention-backed KV writes, no decode, no logits, no vocabulary sampling, no generation, no serving, no eval, no benchmark, no throughput.\n");
}
