/*
 * tensors.c - tensor-collection command-family CLI surface.
 * Owner: src/cli/model_artifacts
 * Owns: existing tensor-collection command-family parsing and output behavior.
 * Does not own: runtime implementation, graph execution, backend algorithms, artifact emission, eval, benchmark, or release claims.
 * Invariants: CLI-only and excluded from libyvex.a; preserves existing command behavior.
 * Boundary: tensor-collection reports are report-only diagnostics.
 */
#include "tensors.h"

typedef struct {
    const char *model;
    const char *backend;
    const char *family;
    const char *collection;
    const char *registry_path;
    int include_router;
    int include_experts;
    int include_shared;
    int include_dispatch;
    int include_storage;
    int include_residency;
    int include_blockers;
    yvex_models_output_mode output_mode;
} yvex_cli_tensor_collection_options;

typedef struct {
    const char *tensor_collection;
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
    const char *collection_stage;
    const char *model_is_moe;
    const char *moe_class_status;
    const char *router_collection_status;
    const char *router_role_status;
    const char *router_tensor_name;
    const char *router_tensor_present;
    const char *router_tensor_status;
    const char *router_dtype;
    const char *router_qtype;
    const char *router_shape;
    const char *router_source;
    const char *router_logits_status;
    const char *top_k_policy_status;
    unsigned long long top_k;
    const char *expert_collection_status;
    const char *expert_role_status;
    const char *expert_tensor_roles;
    const char *expert_gate_role_status;
    const char *expert_up_role_status;
    const char *expert_down_role_status;
    const char *expert_tensor_name_pattern;
    const char *expert_tensor_count_status;
    unsigned long long expert_tensor_count;
    const char *expert_count_status;
    unsigned long long expert_count;
    const char *active_expert_count_status;
    unsigned long long active_expert_count;
    const char *shared_expert_collection_status;
    const char *shared_expert_status;
    const char *shared_expert_roles;
    const char *shared_expert_count_status;
    unsigned long long shared_expert_count;
    const char *dispatch_metadata_status;
    const char *dispatch_metadata_roles;
    const char *expert_indexing_policy_status;
    const char *expert_storage_pressure_status;
    const char *expert_storage_pressure;
    const char *expert_residency_pressure_status;
    const char *expert_residency_pressure;
    const char *present_roles;
    const char *missing_roles;
    const char *unknown_roles;
    const char *not_applicable_roles;
    const char *blocked_rows;
    const char *next_required_rows;
    const char *runtime_claim;
    const char *generation;
    const char *benchmark_status;
    yvex_models_output_mode output_mode;
    int include_router;
    int include_experts;
    int include_shared;
    int include_dispatch;
    int include_storage;
    int include_residency;
    int include_blockers;
} yvex_tensor_collection_report;

static int tensor_collection_parse_value_option(const char *flag,
                                                int arg_count,
                                                char **args,
                                                int *index,
                                                const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: tensor-collection %s requires a value\n", flag);
        return 2;
    }
    *value = args[++(*index)];
    if (fullmodel_string_is_empty(*value)) {
        yvex_cli_out_writef(stderr, "yvex: tensor-collection %s value is empty\n", flag);
        return 2;
    }
    return 0;
}

static int parse_tensor_collection_options(int arg_count,
                                           char **args,
                                           yvex_cli_tensor_collection_options *options)
{
    int i;

    if (!options) return 2;
    memset(options, 0, sizeof(*options));
    options->backend = "cpu";
    options->family = "auto";
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_tensor_collection_help(stdout);
        return 1;
    }
    if (arg_count < 3 || strcmp(args[2], "report") != 0) {
        yvex_cli_out_writef(stderr, "yvex: tensor-collection requires report\n");
        yvex_cli_out_writef(stderr, "usage: " "yvex tensor-collection report --model FILE_OR_ALIAS --collection moe [--family auto|deepseek|glm|qwen] [--backend cpu|cuda]\n");
        return 2;
    }

    for (i = 3; i < arg_count; ++i) {
        const char *value = NULL;
        if (strcmp(args[i], "--model") == 0) {
            int rc = tensor_collection_parse_value_option("--model", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->model = value;
        } else if (strcmp(args[i], "--backend") == 0) {
            int rc = tensor_collection_parse_value_option("--backend", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->backend = value;
        } else if (strcmp(args[i], "--family") == 0) {
            int rc = tensor_collection_parse_value_option("--family", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->family = value;
        } else if (strcmp(args[i], "--collection") == 0) {
            int rc = tensor_collection_parse_value_option("--collection", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->collection = value;
        } else if (strcmp(args[i], "--registry") == 0) {
            int rc = tensor_collection_parse_value_option("--registry", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            options->registry_path = value;
        } else if (strcmp(args[i], "--" "include-router") == 0) {
            options->include_router = 1;
        } else if (strcmp(args[i], "--" "include-experts") == 0) {
            options->include_experts = 1;
        } else if (strcmp(args[i], "--" "include-shared") == 0) {
            options->include_shared = 1;
        } else if (strcmp(args[i], "--" "include-dispatch") == 0) {
            options->include_dispatch = 1;
        } else if (strcmp(args[i], "--" "include-storage") == 0) {
            options->include_storage = 1;
        } else if (strcmp(args[i], "--" "include-residency") == 0) {
            options->include_residency = 1;
        } else if (strcmp(args[i], "--" "include-blockers") == 0) {
            options->include_blockers = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            int rc = tensor_collection_parse_value_option("--" "output", arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: tensor-collection unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--help") == 0 || strcmp(args[i], "-h") == 0) {
            yvex_model_artifacts_surface_tensor_collection_help(stdout);
            return 1;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown tensor-collection option: %s\n", args[i]);
            return 2;
        }
    }

    if (!options->model || !options->model[0]) {
        yvex_cli_out_writef(stderr, "yvex: tensor-collection report requires --model FILE_OR_ALIAS\n");
        return 2;
    }
    return 0;
}

static const char *tensor_collection_requested_family(const yvex_cli_tensor_collection_options *options)
{
    return options && options->family && options->family[0] ? options->family : "auto";
}

static const char *tensor_collection_requested_collection(const yvex_cli_tensor_collection_options *options)
{
    return options && options->collection && options->collection[0] ? options->collection : "unknown";
}

static void tensor_collection_init_report(yvex_tensor_collection_report *report,
                                          const yvex_cli_tensor_collection_options *options)
{
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->tensor_collection = tensor_collection_requested_collection(options);
    report->status = "internal-error";
    report->target_id = "unknown";
    report->target_class = "unknown";
    report->model = options && options->model ? options->model : "";
    report->model_resolved_path = "unknown";
    report->family = tensor_collection_requested_family(options);
    report->family_detected = "unknown";
    report->family_requested = tensor_collection_requested_family(options);
    report->backend = options && options->backend ? options->backend : "cpu";
    report->source_class = "unknown";
    report->artifact_class = "unknown";
    report->implementation_stage = "report-only";
    report->collection_stage = "report-only";
    report->model_is_moe = "unknown";
    report->moe_class_status = "unknown";
    report->router_collection_status = "unknown";
    report->router_role_status = "unknown";
    report->router_tensor_name = "none";
    report->router_tensor_present = "false";
    report->router_tensor_status = "unknown";
    report->router_dtype = "unknown";
    report->router_qtype = "unknown";
    report->router_shape = "unknown";
    report->router_source = "unknown";
    report->router_logits_status = "unsupported";
    report->top_k_policy_status = "unknown";
    report->expert_collection_status = "unknown";
    report->expert_role_status = "unknown";
    report->expert_tensor_roles = "unknown";
    report->expert_gate_role_status = "unknown";
    report->expert_up_role_status = "unknown";
    report->expert_down_role_status = "unknown";
    report->expert_tensor_name_pattern = "unknown";
    report->expert_tensor_count_status = "unknown";
    report->expert_count_status = "unknown";
    report->active_expert_count_status = "unknown";
    report->shared_expert_collection_status = "unknown";
    report->shared_expert_status = "unknown";
    report->shared_expert_roles = "unknown";
    report->shared_expert_count_status = "unknown";
    report->dispatch_metadata_status = "unknown";
    report->dispatch_metadata_roles = "unknown";
    report->expert_indexing_policy_status = "unknown";
    report->expert_storage_pressure_status = "unknown";
    report->expert_storage_pressure = "unknown";
    report->expert_residency_pressure_status = "unknown";
    report->expert_residency_pressure = "unknown";
    report->present_roles = "none";
    report->missing_roles = "unknown";
    report->unknown_roles = "unknown";
    report->not_applicable_roles = "none";
    report->blocked_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    report->next_required_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    report->runtime_claim = "unsupported";
    report->generation = "unsupported-full-model";
    report->benchmark_status = "not-measured";
    report->output_mode = options ? options->output_mode : YVEX_MODELS_OUTPUT_AUDIT;
    report->include_router = options ? options->include_router : 0;
    report->include_experts = options ? options->include_experts : 0;
    report->include_shared = options ? options->include_shared : 0;
    report->include_dispatch = options ? options->include_dispatch : 0;
    report->include_storage = options ? options->include_storage : 0;
    report->include_residency = options ? options->include_residency : 0;
    report->include_blockers = options ? options->include_blockers : 0;
}

static void tensor_collection_print_report(const yvex_tensor_collection_report *report)
{
    if (report && report->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "report: tensor-collection\n");
        yvex_cli_out_writef(stdout, "model: %s\n", report->model ? report->model : "");
        yvex_cli_out_writef(stdout, "family: %s collection=%s\n",
               report->family ? report->family : "unknown",
               report->tensor_collection ? report->tensor_collection : "unknown");
        yvex_cli_out_writef(stdout, "status: %s\n", report->status ? report->status : "report-only");
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", report->blocked_rows ? report->blocked_rows : "tensor roles incomplete");
        yvex_cli_out_writef(stdout, "next: %s\n", report->next_required_rows ? report->next_required_rows : "V010.TENSOR.*");
        yvex_cli_out_writef(stdout, "boundary: report-only, no runtime execution\n");
        return;
    }

    yvex_cli_out_writef(stdout, "tensor_collection: %s\n", report && report->tensor_collection ? report->tensor_collection : "unknown");
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
    yvex_cli_out_writef(stdout, "collection_stage: %s\n", report && report->collection_stage ? report->collection_stage : "report-only");
    yvex_cli_out_writef(stdout, "model_is_moe: %s\n", report && report->model_is_moe ? report->model_is_moe : "unknown");
    yvex_cli_out_writef(stdout, "moe_class_status: %s\n", report && report->moe_class_status ? report->moe_class_status : "unknown");
    yvex_cli_out_writef(stdout, "router_collection_status: %s\n", report && report->router_collection_status ? report->router_collection_status : "unknown");
    yvex_cli_out_writef(stdout, "router_role_status: %s\n", report && report->router_role_status ? report->router_role_status : "unknown");
    yvex_cli_out_writef(stdout, "router_tensor_name: %s\n", report && report->router_tensor_name ? report->router_tensor_name : "none");
    yvex_cli_out_writef(stdout, "router_tensor_present: %s\n", report && report->router_tensor_present ? report->router_tensor_present : "false");
    yvex_cli_out_writef(stdout, "router_tensor_status: %s\n", report && report->router_tensor_status ? report->router_tensor_status : "unknown");
    yvex_cli_out_writef(stdout, "router_dtype: %s\n", report && report->router_dtype ? report->router_dtype : "unknown");
    yvex_cli_out_writef(stdout, "router_qtype: %s\n", report && report->router_qtype ? report->router_qtype : "unknown");
    yvex_cli_out_writef(stdout, "router_shape: %s\n", report && report->router_shape ? report->router_shape : "unknown");
    yvex_cli_out_writef(stdout, "router_source: %s\n", report && report->router_source ? report->router_source : "unknown");
    yvex_cli_out_writef(stdout, "router_logits_status: %s\n", report && report->router_logits_status ? report->router_logits_status : "unsupported");
    yvex_cli_out_writef(stdout, "top_k_policy_status: %s\n", report && report->top_k_policy_status ? report->top_k_policy_status : "unknown");
    yvex_cli_out_writef(stdout, "top_k: %llu\n", report ? report->top_k : 0ull);
    yvex_cli_out_writef(stdout, "expert_collection_status: %s\n", report && report->expert_collection_status ? report->expert_collection_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_role_status: %s\n", report && report->expert_role_status ? report->expert_role_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_roles: %s\n", report && report->expert_tensor_roles ? report->expert_tensor_roles : "unknown");
    yvex_cli_out_writef(stdout, "expert_gate_role_status: %s\n", report && report->expert_gate_role_status ? report->expert_gate_role_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_up_role_status: %s\n", report && report->expert_up_role_status ? report->expert_up_role_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_down_role_status: %s\n", report && report->expert_down_role_status ? report->expert_down_role_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_name_pattern: %s\n", report && report->expert_tensor_name_pattern ? report->expert_tensor_name_pattern : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_count_status: %s\n", report && report->expert_tensor_count_status ? report->expert_tensor_count_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_tensor_count: %llu\n", report ? report->expert_tensor_count : 0ull);
    yvex_cli_out_writef(stdout, "expert_count_status: %s\n", report && report->expert_count_status ? report->expert_count_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_count: %llu\n", report ? report->expert_count : 0ull);
    yvex_cli_out_writef(stdout, "active_expert_count_status: %s\n", report && report->active_expert_count_status ? report->active_expert_count_status : "unknown");
    yvex_cli_out_writef(stdout, "active_expert_count: %llu\n", report ? report->active_expert_count : 0ull);
    yvex_cli_out_writef(stdout, "shared_expert_collection_status: %s\n", report && report->shared_expert_collection_status ? report->shared_expert_collection_status : "unknown");
    yvex_cli_out_writef(stdout, "shared_expert_status: %s\n", report && report->shared_expert_status ? report->shared_expert_status : "unknown");
    yvex_cli_out_writef(stdout, "shared_expert_roles: %s\n", report && report->shared_expert_roles ? report->shared_expert_roles : "unknown");
    yvex_cli_out_writef(stdout, "shared_expert_count_status: %s\n", report && report->shared_expert_count_status ? report->shared_expert_count_status : "unknown");
    yvex_cli_out_writef(stdout, "shared_expert_count: %llu\n", report ? report->shared_expert_count : 0ull);
    yvex_cli_out_writef(stdout, "dispatch_metadata_status: %s\n", report && report->dispatch_metadata_status ? report->dispatch_metadata_status : "unknown");
    yvex_cli_out_writef(stdout, "dispatch_metadata_roles: %s\n", report && report->dispatch_metadata_roles ? report->dispatch_metadata_roles : "unknown");
    yvex_cli_out_writef(stdout, "expert_indexing_policy_status: %s\n", report && report->expert_indexing_policy_status ? report->expert_indexing_policy_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_storage_pressure_status: %s\n", report && report->expert_storage_pressure_status ? report->expert_storage_pressure_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_storage_pressure: %s\n", report && report->expert_storage_pressure ? report->expert_storage_pressure : "unknown");
    yvex_cli_out_writef(stdout, "expert_residency_pressure_status: %s\n", report && report->expert_residency_pressure_status ? report->expert_residency_pressure_status : "unknown");
    yvex_cli_out_writef(stdout, "expert_residency_pressure: %s\n", report && report->expert_residency_pressure ? report->expert_residency_pressure : "unknown");
    yvex_cli_out_writef(stdout, "present_roles: %s\n", report && report->present_roles ? report->present_roles : "none");
    yvex_cli_out_writef(stdout, "missing_roles: %s\n", report && report->missing_roles ? report->missing_roles : "unknown");
    yvex_cli_out_writef(stdout, "unknown_roles: %s\n", report && report->unknown_roles ? report->unknown_roles : "unknown");
    yvex_cli_out_writef(stdout, "not_applicable_roles: %s\n", report && report->not_applicable_roles ? report->not_applicable_roles : "none");
    yvex_cli_out_writef(stdout, "blocked_rows: %s\n", report && report->blocked_rows ? report->blocked_rows : "unknown");
    yvex_cli_out_writef(stdout, "next_required_rows: %s\n", report && report->next_required_rows ? report->next_required_rows : "unknown");
    yvex_cli_out_writef(stdout, "runtime_claim: %s\n", report && report->runtime_claim ? report->runtime_claim : "unsupported");
    yvex_cli_out_writef(stdout, "generation: %s\n", report && report->generation ? report->generation : "unsupported-full-model");
    yvex_cli_out_writef(stdout, "benchmark_status: %s\n", report && report->benchmark_status ? report->benchmark_status : "not-measured");
    yvex_cli_out_writef(stdout, "generation_ready: false\n");
    yvex_cli_out_writef(stdout, "report_options.include_router: %s\n", report && report->include_router ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_experts: %s\n", report && report->include_experts ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_shared: %s\n", report && report->include_shared ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_dispatch: %s\n", report && report->include_dispatch ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_storage: %s\n", report && report->include_storage ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_residency: %s\n", report && report->include_residency ? "true" : "false");
    yvex_cli_out_writef(stdout, "report_options.include_blockers: %s\n", report && report->include_blockers ? "true" : "false");
    yvex_cli_out_writef(stdout, "cleanup_attempted: false\n");
    yvex_cli_out_writef(stdout, "cleanup_status: not-needed\n");
}

static int tensor_collection_print_invalid_argument_report(const yvex_cli_tensor_collection_options *options,
                                                           const char *status,
                                                           const char *reason)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.status = status && status[0] ? status : "invalid-argument";
    report.target_id = options && options->model ? options->model : "unknown";
    report.model_resolved_path = "not-resolved";
    report.collection_stage = "report-only";
    report.router_logits_status = "unsupported";
    report.blocked_rows = reason && reason[0] ? reason : "invalid tensor collection report argument";
    report.next_required_rows = "valid-collection-moe";
    tensor_collection_print_report(&report);
    return 2;
}

static int tensor_collection_print_source_only_report(const yvex_cli_tensor_collection_options *options,
                                                      const char *target)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
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
    report.router_collection_status = "unsupported";
    report.router_role_status = "unsupported";
    report.router_tensor_status = "unsupported";
    report.router_source = "source-only-not-inspected";
    report.expert_collection_status = "unsupported";
    report.expert_role_status = "unsupported";
    report.expert_tensor_roles = "source-only-not-inspected";
    report.expert_gate_role_status = "unsupported";
    report.expert_up_role_status = "unsupported";
    report.expert_down_role_status = "unsupported";
    report.expert_tensor_count_status = "unknown";
    report.shared_expert_collection_status = "unknown";
    report.shared_expert_status = "unknown";
    report.shared_expert_count_status = "unknown";
    report.dispatch_metadata_status = "unknown";
    report.expert_storage_pressure_status = "unknown";
    report.expert_storage_pressure = "source-only-unmeasured";
    report.expert_residency_pressure_status = "unknown";
    report.expert_residency_pressure = "requires-storage-stream-plan";
    report.present_roles = "none";
    report.missing_roles = "source-only-not-inspected";
    report.unknown_roles = "moe_router,moe_expert_gate,moe_expert_up,moe_expert_down,moe_shared_expert,moe_dispatch_metadata,expert_indexing_policy";
    report.blocked_rows = "OWI.HUGE.0,V010.MAP.4,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14";
    report.next_required_rows = "OWI.HUGE.0,V010.MAP.4,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.STORAGE.18,V010.RESIDENCY.14,GLM-YVEX-produced-GGUF";
    tensor_collection_print_report(&report);
    return 5;
}

static int tensor_collection_print_missing_model_report(const yvex_cli_tensor_collection_options *options,
                                                        const char *reason)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "missing-model";
    report.target_id = options && options->model ? options->model : "missing";
    report.model_resolved_path = "missing";
    report.router_collection_status = "unknown";
    report.router_role_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_collection_status = "unknown";
    report.expert_role_status = "unknown";
    report.blocked_rows = reason && reason[0] ? reason : "model or alias could not be resolved";
    report.next_required_rows = "V010.TARGET.9,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17";
    tensor_collection_print_report(&report);
    return 5;
}

static int tensor_collection_print_unsupported_family_report(const yvex_cli_tensor_collection_options *options,
                                                            yvex_model_ref *ref,
                                                            const char *target_id,
                                                            const char *target_class,
                                                            const char *detected)
{
    yvex_tensor_collection_report report;

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
    report.status = "unsupported-family";
    report.target_id = target_id ? target_id : "path";
    report.target_class = target_class ? target_class : "candidate-GGUF-path";
    report.model_resolved_path = ref && ref->path ? ref->path : "unknown";
    report.family = tensor_collection_requested_family(options);
    report.family_detected = detected ? detected : "unknown";
    report.source_class = "GGUF artifact";
    report.artifact_class = "GGUF artifact";
    report.model_is_moe = "unknown";
    report.moe_class_status = "unsupported-family";
    report.router_collection_status = "unknown";
    report.router_role_status = "unknown";
    report.router_tensor_status = "unknown";
    report.expert_collection_status = "unknown";
    report.expert_role_status = "unknown";
    report.blocked_rows = "requested family is not supported by MoE tensor collection report";
    report.next_required_rows = "V010.CLASS.0,V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,family-specific-tensor-adapter";
    tensor_collection_print_report(&report);
    return 5;
}

static void tensor_collection_append_role(char *out, size_t out_cap, const char *role)
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

static const char *tensor_collection_role_status(const yvex_tensor_info *tensor)
{
    return tensor ? "present" : "missing";
}

static int tensor_collection_print_model_report(const yvex_cli_tensor_collection_options *options,
                                                yvex_model_ref *ref,
                                                yvex_model_context *ctx,
                                                const char *target_id,
                                                const char *target_class,
                                                yvex_arch arch)
{
    yvex_tensor_collection_report report;
    yvex_cli_fullmodel_options family_probe;
    const yvex_tensor_info *router;
    const yvex_tensor_info *expert_gate;
    const yvex_tensor_info *expert_up;
    const yvex_tensor_info *expert_down;
    const char *requested;
    const char *detected;
    char router_shape[128];
    char present_roles[192];
    char missing_roles[256];
    char expert_roles[192];
    unsigned int present_expert_roles = 0u;

    memset(&family_probe, 0, sizeof(family_probe));
    family_probe.model = options ? options->model : NULL;
    family_probe.family = options ? options->family : NULL;
    requested = tensor_collection_requested_family(options);
    detected = fullmodel_detect_family(&family_probe, arch, target_id);
    if (!fullmodel_family_request_matches(requested, detected) || strcmp(detected, "deepseek") != 0) {
        return tensor_collection_print_unsupported_family_report(options, ref, target_id, target_class, detected);
    }

    router = fullmodel_descriptor_find_tensor(ctx, "moe_router");
    expert_gate = fullmodel_descriptor_find_tensor(ctx, "moe_expert_gate");
    expert_up = fullmodel_descriptor_find_tensor(ctx, "moe_expert_up");
    expert_down = fullmodel_descriptor_find_tensor(ctx, "moe_expert_down");

    router_shape[0] = '\0';
    if (router) dims_to_text(router->dims, router->rank, router_shape, sizeof(router_shape));
    else snprintf(router_shape, sizeof(router_shape), "unknown");

    present_roles[0] = '\0';
    missing_roles[0] = '\0';
    expert_roles[0] = '\0';
    if (router) tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_router");
    else tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_router");
    if (expert_gate) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_gate");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_gate");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_gate");
    }
    if (expert_up) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_up");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_up");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_up");
    }
    if (expert_down) {
        present_expert_roles++;
        tensor_collection_append_role(present_roles, sizeof(present_roles), "moe_expert_down");
        tensor_collection_append_role(expert_roles, sizeof(expert_roles), "moe_expert_down");
    } else {
        tensor_collection_append_role(missing_roles, sizeof(missing_roles), "moe_expert_down");
    }
    if (!present_roles[0]) snprintf(present_roles, sizeof(present_roles), "none");
    if (!missing_roles[0]) snprintf(missing_roles, sizeof(missing_roles), "none");
    if (!expert_roles[0]) snprintf(expert_roles, sizeof(expert_roles), "missing");

    tensor_collection_init_report(&report, options);
    report.tensor_collection = "moe";
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
    report.router_collection_status = router ? "present" : "missing";
    report.router_role_status = tensor_collection_role_status(router);
    report.router_tensor_name = router && router->name ? router->name : "none";
    report.router_tensor_present = router ? "true" : "false";
    report.router_tensor_status = tensor_collection_role_status(router);
    report.router_dtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_qtype = router ? yvex_dtype_name(router->dtype) : "unknown";
    report.router_shape = router_shape;
    report.router_source = router ? "artifact-directory" : "missing";
    report.router_logits_status = "unsupported";
    report.top_k_policy_status = "unknown";
    report.top_k = 0ull;
    report.expert_collection_status = present_expert_roles == 3u ? "present" :
                                      (present_expert_roles > 0u ? "partial" : "missing");
    report.expert_role_status = present_expert_roles == 3u ? "present" :
                                (present_expert_roles > 0u ? "partial" : "missing");
    report.expert_tensor_roles = expert_roles;
    report.expert_gate_role_status = tensor_collection_role_status(expert_gate);
    report.expert_up_role_status = tensor_collection_role_status(expert_up);
    report.expert_down_role_status = tensor_collection_role_status(expert_down);
    report.expert_tensor_name_pattern = present_expert_roles > 0u ? "expert.*.(gate|up|down)" : "unknown";
    report.expert_tensor_count_status = present_expert_roles > 0u ? "known" : "missing";
    report.expert_tensor_count = present_expert_roles;
    report.expert_count_status = "unknown";
    report.active_expert_count_status = "unknown";
    report.shared_expert_collection_status = "unknown";
    report.shared_expert_status = "unknown";
    report.shared_expert_roles = "unknown";
    report.shared_expert_count_status = "unknown";
    report.dispatch_metadata_status = "unknown";
    report.dispatch_metadata_roles = "unknown";
    report.expert_indexing_policy_status = "unknown";
    report.expert_storage_pressure_status = "unknown";
    report.expert_storage_pressure = strcmp(report.target_class, "selected-runtime-slice") == 0
                                         ? "selected-slice-missing"
                                         : "expert-dominant-expected";
    report.expert_residency_pressure_status = "unknown";
    report.expert_residency_pressure = strcmp(report.target_class, "selected-runtime-slice") == 0
                                           ? "selected-slice-missing"
                                           : "requires-expert-residency-plan";
    report.present_roles = present_roles;
    report.missing_roles = missing_roles;
    report.unknown_roles = "moe_shared_expert,moe_dispatch_metadata,expert_indexing_policy";
    report.not_applicable_roles = "none";
    report.blocked_rows = "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.MOE.5,V010.MOE.6,V010.MOE.7,V010.PREFILL.7,V010.DECODE.6";
    report.next_required_rows = router || present_expert_roles > 0u
                                    ? "V010.STORAGE.18,V010.RESIDENCY.14,V010.MOE.5,V010.MOE.6,V010.MOE.7"
                                    : "V010.TENSOR.14,V010.TENSOR.15,V010.TENSOR.16,V010.TENSOR.17,V010.TARGET.9,V010.MAP.2,V010.FULLMODEL.6";
    tensor_collection_print_report(&report);
    return 0;
}

int yvex_model_artifacts_surface_tensor_collection_command(int arg_count, char **args)
{
    yvex_cli_tensor_collection_options options;
    yvex_model_ref_options ref_options;
    yvex_model_ref ref;
    yvex_model_context ctx;
    yvex_error err;
    const char *target_id;
    const char *target_class;
    yvex_arch arch;
    unsigned long long artifact_bytes = 0ull;
    int selected_target;
    int rc;

    yvex_error_clear(&err);
    memset(&options, 0, sizeof(options));
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));

    rc = parse_tensor_collection_options(arg_count, args, &options);
    if (rc == 1) return 0;
    if (rc != 0) return rc;

    if (strcmp(options.backend, "cpu") != 0 && strcmp(options.backend, "cuda") != 0) {
        return tensor_collection_print_invalid_argument_report(&options,
                                                              "unsupported-backend",
                                                              "backend must be cpu or cuda");
    }
    if (!options.collection || strcmp(options.collection, "moe") != 0) {
        return tensor_collection_print_invalid_argument_report(&options,
                                                              "invalid-argument",
                                                              "only --collection moe is implemented");
    }
    if (strcmp(options.model, "glm-5.2-official-safetensors") == 0) {
        return tensor_collection_print_source_only_report(&options, "glm-5.2-official-safetensors");
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.allow_registry = 1;
    ref_options.registry_path = options.registry_path;
    rc = yvex_model_ref_resolve(&ref, options.model, &ref_options, &err);
    if (rc != YVEX_OK) {
        yvex_error_clear(&err);
        return tensor_collection_print_missing_model_report(&options, "model or alias could not be resolved");
    }
    if (!fullmodel_file_size(ref.path, &artifact_bytes)) {
        rc = tensor_collection_print_missing_model_report(&options, "resolved artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return rc;
    }

    rc = yvex_model_context_open(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        const char *reason = yvex_error_message(&err);
        rc = tensor_collection_print_missing_model_report(&options, reason && reason[0] ? reason : "GGUF metadata or tensor directory parse failed");
        yvex_error_clear(&err);
        yvex_model_ref_clear(&ref);
        return rc;
    }

    arch = yvex_model_arch(ctx.model);
    target_id = ref.alias && ref.alias[0] ? ref.alias : "path";
    selected_target = fullmodel_is_selected_target(target_id) ||
                      fullmodel_is_selected_target(options.model);
    target_class = selected_target ? "selected-runtime-slice" : "candidate-GGUF-path";
    rc = tensor_collection_print_model_report(&options, &ref, &ctx, target_id, target_class, arch);
    yvex_model_context_close(&ctx);
    yvex_model_ref_clear(&ref);
    return rc;
}

void yvex_model_artifacts_surface_tensor_collection_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex tensor-collection report --model FILE_OR_ALIAS --collection moe [--family auto|deepseek|glm|qwen] [--backend cpu|cuda] [--registry FILE] [--" "audit | --" "output normal|table|audit] [--" "include-router] [--" "include-experts] [--" "include-shared] [--" "include-dispatch] [--" "include-storage] [--" "include-residency] [--" "include-blockers]\n");
    yvex_cli_out_writef(fp, "\nExamples:\n");
    yvex_cli_out_writef(fp, "  yvex tensor-collection report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --collection moe --backend cpu --" "include-router --" "include-experts --" "include-blockers\n");
    yvex_cli_out_writef(fp, "  yvex tensor-collection report --model glm-5.2-official-safetensors --family glm --collection moe --backend cpu --" "include-blockers\n");
    yvex_cli_out_writef(fp, "\ntensor-collection report:\n");
    yvex_cli_out_writef(fp, "  reports tensor collection requirements and coverage for the requested collection. The current implemented collection is moe.\n");
    yvex_cli_out_writef(fp, "  Default output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "  MoE collection reports classify router, expert gate/up/down, shared expert, dispatch metadata, indexing, storage pressure, residency pressure, blockers, and next rows.\n");
    yvex_cli_out_writef(fp, "  report-only boundary: it does not materialize tensors, does not execute router logits, does not select experts, does not dispatch experts, does not accumulate expert outputs, does not run MoE blocks, does not run prefill, does not run decode, does not produce logits, does not sample, does not generate, does not evaluate, and does not benchmark.\n");
    yvex_cli_out_writef(fp, "  selected-runtime-slice targets may return ok-partial when the family is MoE but router or expert tensors are not present in the selected artifact.\n");
    yvex_cli_out_writef(fp, "  source-only targets are reported without opening huge source shards or downloading artifacts.\n");
    yvex_cli_out_writef(fp, "Boundary: no MoE runtime execution, no router logits, no top-k routing, no expert activation, no expert dispatch, no expert accumulation, no full transformer block, no full transformer prefill, no decode, no logits, no vocabulary sampling, no generation, no eval, no benchmark, no throughput.\n");
}
