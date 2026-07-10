/*
 * yvex_models_prepare_surface.c - models prepare/check CLI surface.
 *
 * Owner: src/cli/render
 * Owns: models prepare/check command-family surface behavior.
 * Does not own: registry storage, model gate algorithms, materialization algorithms, runtime generation, or artifact emission.
 * Invariants: preserves existing prepare/check command syntax and output behavior; CLI-only.
 * Boundary: prepare/check reports do not promote generation or release readiness.
 */
#include "yvex_models_prepare_surface.h"
#include "yvex_model_target_catalog.h"

typedef struct {
    const char *target;
    const char *source;
    const char *out;
    const char *out_dir;
    const char *models_root;
    const char *registry_path;
    yvex_models_output_mode output_mode;
    int overwrite;
    int dry_run;
    int register_alias;
    int use_alias;
} yvex_cli_models_prepare_options;

/* Selected artifact prepare preset. */

static int parse_models_prepare_options(int arg_count,
                                        char **args,
                                        yvex_cli_models_prepare_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->register_alias = 1;
    options->use_alias = 1;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models prepare requires TARGET\n");
        return 2;
    }
    options->target = args[3];
    if (!cli_arg_value_valid(options->target)) {
        yvex_cli_out_writef(stderr, "yvex: models prepare target is empty or invalid\n");
        return 2;
    }
    for (i = 4; i < arg_count; ++i) {
        if (strcmp(args[i], "--overwrite") == 0) {
            options->overwrite = 1;
        } else if (strcmp(args[i], "--dry-run") == 0) {
            options->dry_run = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--no-register") == 0) {
            options->register_alias = 0;
            options->use_alias = 0;
        } else if (strcmp(args[i], "--no-use") == 0) {
            options->use_alias = 0;
        } else if (strcmp(args[i], "--source") == 0 ||
                   strcmp(args[i], "--out") == 0 ||
                   strcmp(args[i], "--out-dir") == 0 ||
                   strcmp(args[i], "--models-root") == 0 ||
                   strcmp(args[i], "--registry") == 0 ||
                   strcmp(args[i], "--" "output") == 0) {
            const char *flag = args[i];
            const char *value = NULL;
            int rc = parse_models_value_option("models prepare", flag,
                                               arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--source") == 0) options->source = value;
            else if (strcmp(flag, "--out") == 0) options->out = value;
            else if (strcmp(flag, "--out-dir") == 0) options->out_dir = value;
            else if (strcmp(flag, "--models-root") == 0) options->models_root = value;
            else if (strcmp(flag, "--registry") == 0) options->registry_path = value;
            else if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: models prepare unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--" "json") == 0) {
            yvex_cli_out_writef(stderr, "yvex: models prepare JSON output is unsupported; use --" "output normal|table|audit\n");
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown models prepare option: %s\n", args[i]);
            return 2;
        }
    }
    if (options->out && options->out_dir) {
        yvex_cli_out_writef(stderr, "yvex: models prepare --out and --out-dir are mutually exclusive\n");
        return 2;
    }
    if (!options->register_alias) {
        options->use_alias = 0;
    }
    return 0;
}

static void print_prepare_common(const yvex_cli_models_prepare_options *options,
                                 const yvex_operator_paths *operator_paths,
                                 const char *source_path,
                                 const char *artifact_path,
                                 const char *manifest_path,
                                 const char *plan_path,
                                 const char *registry_path)
{
    yvex_cli_out_writef(stdout, "models: prepare\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", options->target);
    yvex_cli_out_writef(stdout, "models_root_source: %s\n", operator_paths->models_root_source);
    yvex_cli_out_writef(stdout, "models_root: %s\n", operator_paths->models_root);
    yvex_cli_out_writef(stdout, "source_path: %s\n", source_path);
    yvex_cli_out_writef(stdout, "artifact_path: %s\n", artifact_path);
    yvex_cli_out_writef(stdout, "source_manifest_path: %s\n", manifest_path);
    yvex_cli_out_writef(stdout, "conversion_plan_path: %s\n", plan_path);
    yvex_cli_out_writef(stdout, "registry_path: %s\n", registry_path && registry_path[0] ? registry_path : ".yvex/models.local.json");
    yvex_cli_out_writef(stdout, "alias: deepseek4-v4-flash-selected-embed\n");
    yvex_cli_out_writef(stdout, "overwrite: %s\n", options->overwrite ? "true" : "false");
    yvex_cli_out_writef(stdout, "dry_run: %s\n", options->dry_run ? "true" : "false");
    yvex_cli_out_writef(stdout, "register: %s\n", options->register_alias ? "true" : "false");
    yvex_cli_out_writef(stdout, "use_alias: %s\n", options->use_alias ? "true" : "false");
}

static void print_prepare_dry_run_stages(int register_alias, int use_alias)
{
    static const char *planned[] = {
        "resolve-paths",
        "source-manifest",
        "native-weights",
        "tensor-map",
        "convert-plan",
        "convert-emit",
        "inspect",
        "tensors",
        "metadata"
    };
    unsigned long i;

    for (i = 0; i < sizeof(planned) / sizeof(planned[0]); ++i) {
        model_stage_print(planned[i], "planned");
    }
    model_stage_print("registry-remove-existing", register_alias ? "planned" : "skipped");
    model_stage_print("registry-add", register_alias ? "planned" : "skipped");
    model_stage_print("registry-use", use_alias ? "planned" : "skipped");
    model_stage_print("registry-verify", register_alias ? "planned" : "skipped");
}

static const char *prepare_unsupported_reason(const char *target)
{
    if (strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
        return "segment prepare is planned, not implemented by this preset";
    }
    if (strcmp(target, "glm-5.2-official-safetensors") == 0) {
        return "YVEX-produced GGUF emission for this target is planned, not implemented";
    }
    return NULL;
}

static int print_prepare_unsupported(const char *target)
{
    const char *reason = prepare_unsupported_reason(target);

    yvex_cli_out_writef(stdout, "models: prepare\n");
    yvex_cli_out_writef(stdout, "target_id: %s\n", target);
    model_stage_print("target", "unsupported");
    model_print_runtime_generation("not-performed");
    if (!reason) {
        yvex_cli_out_writef(stdout, "reason: unknown model prepare target\n");
        yvex_cli_out_writef(stdout, "status: model-prepare-unknown-target\n");
        return 2;
    }
    yvex_cli_out_writef(stdout, "reason: %s\n", reason);
    yvex_cli_out_writef(stdout, "status: model-prepare-unsupported\n");
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

void prepare_probe_map_sidecar_status(const char *tensor_map_path,
                                             const char *output_head_map_path,
                                             int *tensor_map_incomplete,
                                             int *output_head_map_missing)
{
    char buf[16384];
    char status[64];
    char coverage[64];
    long long unmapped;

    if (tensor_map_incomplete) *tensor_map_incomplete = 0;
    if (output_head_map_missing) *output_head_map_missing = 0;
    if (tensor_map_path && tensor_map_path[0] &&
        model_download_read_small_file(tensor_map_path, buf, sizeof(buf))) {
        if (model_download_json_string_field(buf,
                                             "required_role_coverage_status",
                                             coverage,
                                             sizeof(coverage))) {
            if (strcmp(coverage, "required-groups-present") != 0 &&
                tensor_map_incomplete) {
                *tensor_map_incomplete = 1;
            }
        } else {
            unmapped = model_download_json_i64_field(buf,
                                                     "unmapped_unknown_count");
            if (unmapped > 0 && tensor_map_incomplete) {
                *tensor_map_incomplete = 1;
            }
        }
    }
    if (output_head_map_path && output_head_map_path[0] &&
        model_download_read_small_file(output_head_map_path, buf, sizeof(buf)) &&
        model_download_json_string_field(buf, "output_head_status", status,
                                         sizeof(status)) &&
        strcmp(status, "present") != 0 &&
        output_head_map_missing) {
        *output_head_map_missing = 1;
    }
}

typedef struct {
    char target_id[128];
    char family[32];
    char provider[32];
    char repo_id[256];
    char revision[128];
    char models_root[YVEX_PATH_CAP];
    char source_path[YVEX_PATH_CAP];
    char source_manifest_path[YVEX_PATH_CAP];
    char native_inventory_path[YVEX_PATH_CAP];
    char tensor_map_path[YVEX_PATH_CAP];
    char output_head_map_path[YVEX_PATH_CAP];
    char tokenizer_map_path[YVEX_PATH_CAP];
    char expected_artifact_path[YVEX_PATH_CAP];
    char download_registry_path[YVEX_PATH_CAP];
    char download_report_path[YVEX_PATH_CAP];
    const char *source_status;
    const char *model_class_status;
    const char *tensor_map_status;
    const char *output_head_map_status;
    const char *tokenizer_map_status;
    const char *artifact_status;
    const char *artifact_plan_status;
    const char *artifact_emission_status;
    const char *artifact_identity_status;
    unsigned int blocker_count;
    const char *top_blocker;
    const char *reason;
    const char *next;
    const char *final_status;
    int downloaded_target_resolved;
} yvex_models_prepare_source_report;

static void prepare_source_report_build(
    const yvex_cli_models_prepare_options *options,
    const yvex_operator_paths *operator_paths,
    const yvex_model_download_resolved_target *target,
    yvex_models_prepare_source_report *report)
{
    int source_present;
    int expected_artifact_present = 0;
    int tensor_map_present = 0;
    int output_head_map_present = 0;
    int tokenizer_map_present = 0;
    int tensor_map_incomplete = 0;
    int output_head_map_missing = 0;

    memset(report, 0, sizeof(*report));
    snprintf(report->target_id, sizeof(report->target_id), "%s",
             target && target->target_id[0]
                 ? target->target_id
                 : options && options->target ? options->target : "unknown");
    snprintf(report->family, sizeof(report->family), "%s",
             target && target->family[0] ? target->family : "unknown");
    snprintf(report->provider, sizeof(report->provider), "%s",
             target && target->provider[0] ? target->provider : "huggingface");
    snprintf(report->repo_id, sizeof(report->repo_id), "%s",
             target && target->repo_id[0] ? target->repo_id : "unknown");
    snprintf(report->revision, sizeof(report->revision), "%s",
             target && target->revision[0] ? target->revision : "main");
    snprintf(report->models_root, sizeof(report->models_root), "%s",
             operator_paths ? operator_paths->models_root : "unknown");
    snprintf(report->source_path, sizeof(report->source_path), "%s",
             target && target->local_source_dir[0] ? target->local_source_dir : "unknown");
    snprintf(report->source_manifest_path, sizeof(report->source_manifest_path), "%s",
             target && target->manifest_path[0] ? target->manifest_path : "unknown");
    snprintf(report->native_inventory_path, sizeof(report->native_inventory_path), "%s",
             target && target->native_inventory_path[0] ? target->native_inventory_path : "unknown");
    snprintf(report->download_registry_path, sizeof(report->download_registry_path), "%s",
             target && target->registry_path[0] ? target->registry_path : "unknown");
    snprintf(report->download_report_path, sizeof(report->download_report_path), "%s",
             target && target->download_report_path[0] ? target->download_report_path : "unknown");

    source_present = target && target->local_source_dir[0] &&
                     path_exists(target->local_source_dir);
    if (operator_paths && target && target->family[0] && target->target_id[0]) {
        int n;
        n = snprintf(report->expected_artifact_path,
                     sizeof(report->expected_artifact_path),
                     "%s/%s/%s.gguf",
                     operator_paths->gguf_root,
                     target->family,
                     target->target_id);
        if (n < 0 || (size_t)n >= sizeof(report->expected_artifact_path)) {
            report->expected_artifact_path[0] = '\0';
        }
        expected_artifact_present = report->expected_artifact_path[0] &&
                                    path_exists(report->expected_artifact_path);
        n = snprintf(report->tensor_map_path,
                     sizeof(report->tensor_map_path),
                     "%s/%s/%s.tensor-map.json",
                     operator_paths->reports_root,
                     target->family,
                     target->target_id);
        if (n < 0 || (size_t)n >= sizeof(report->tensor_map_path)) {
            report->tensor_map_path[0] = '\0';
        }
        n = snprintf(report->output_head_map_path,
                     sizeof(report->output_head_map_path),
                     "%s/%s/%s.output-head-map.json",
                     operator_paths->reports_root,
                     target->family,
                     target->target_id);
        if (n < 0 || (size_t)n >= sizeof(report->output_head_map_path)) {
            report->output_head_map_path[0] = '\0';
        }
        n = snprintf(report->tokenizer_map_path,
                     sizeof(report->tokenizer_map_path),
                     "%s/%s/%s.tokenizer-map.json",
                     operator_paths->reports_root,
                     target->family,
                     target->target_id);
        if (n < 0 || (size_t)n >= sizeof(report->tokenizer_map_path)) {
            report->tokenizer_map_path[0] = '\0';
        }
        tensor_map_present = report->tensor_map_path[0] &&
                             path_exists(report->tensor_map_path);
        output_head_map_present = report->output_head_map_path[0] &&
                                  path_exists(report->output_head_map_path);
        tokenizer_map_present = report->tokenizer_map_path[0] &&
                                path_exists(report->tokenizer_map_path);
        prepare_probe_map_sidecar_status(report->tensor_map_path,
                                         report->output_head_map_path,
                                         &tensor_map_incomplete,
                                         &output_head_map_missing);
    } else {
        snprintf(report->expected_artifact_path,
                 sizeof(report->expected_artifact_path), "unknown");
        snprintf(report->tensor_map_path, sizeof(report->tensor_map_path), "unknown");
        snprintf(report->output_head_map_path,
                 sizeof(report->output_head_map_path), "unknown");
        snprintf(report->tokenizer_map_path,
                 sizeof(report->tokenizer_map_path), "unknown");
    }

    if (!source_present) report->blocker_count++;
    if (!tensor_map_present || tensor_map_incomplete) report->blocker_count++;
    if (!output_head_map_present || output_head_map_missing) report->blocker_count++;
    if (!tokenizer_map_present) report->blocker_count++;
    if (!expected_artifact_present) report->blocker_count++;
    report->blocker_count++; /* full artifact emitter/identity is not implemented for dynamic source targets. */

    report->source_status = source_present ? "present" : "missing";
    report->model_class_status = source_present ? "present" : "missing";
    report->tensor_map_status =
        tensor_map_present
            ? (tensor_map_incomplete ? "incomplete-report-only" : "present-report-only")
            : "missing";
    report->output_head_map_status =
        output_head_map_present
            ? (output_head_map_missing ? "missing-in-report" : "present-report-only")
            : "missing";
    report->tokenizer_map_status =
        tokenizer_map_present ? "present-report-only" : "missing";
    report->artifact_status = expected_artifact_present ? "present" : "missing";
    report->artifact_plan_status = "planned-full-gguf";
    report->artifact_emission_status = "not-performed";
    report->artifact_identity_status =
        expected_artifact_present ? "not-checked" : "missing";
    if (!tensor_map_present || tensor_map_incomplete ||
        !output_head_map_present || output_head_map_missing) {
        report->next = "V010.MAP.8";
    } else if (!tokenizer_map_present) {
        report->next = "V010.MAP.7";
    } else {
        report->next = "V010.QUANT.2";
    }
    report->final_status = "model-prepare-unsupported";
    report->downloaded_target_resolved = 1;

    if (!source_present) {
        report->top_blocker = "missing-source";
    } else if (!output_head_map_present || output_head_map_missing) {
        report->top_blocker = "missing-output-head-map";
    } else if (!tensor_map_present || tensor_map_incomplete) {
        report->top_blocker = "incomplete-tensor-map";
    } else if (!tokenizer_map_present) {
        report->top_blocker = "missing-tokenizer-map";
    } else {
        report->top_blocker = "qtype-compute-refusal-matrix-missing";
    }

    report->reason =
        !tensor_map_present
            ? "missing tensor map / model class / artifact path"
            : tensor_map_incomplete
                  ? (!tokenizer_map_present
                         ? "incomplete tensor map / tokenizer metadata mapping / artifact path missing"
                         : "incomplete tensor map / artifact path missing")
                  : !output_head_map_present
                        ? (!tokenizer_map_present
                               ? "missing output head map / tokenizer metadata mapping / artifact path missing"
                               : "missing output head map / artifact path missing")
                        : output_head_map_missing
                              ? (!tokenizer_map_present
                                     ? "output head mapping missing / tokenizer metadata mapping / artifact path missing"
                                     : "output head mapping missing / artifact path missing")
                              : !tokenizer_map_present
                                    ? "tokenizer metadata mapping / artifact path missing"
                                    : "qtype compute/refusal matrix missing";
}

static void prepare_source_report_render_porcelain(
    const yvex_models_prepare_source_report *report)
{
    yvex_render_out out;

    yvex_render_out_init(&out, stdout, YVEX_RENDER_MODE_PORCELAIN);
    yvex_render_report_title(&out, "models prepare", report->target_id, "blocked");
    yvex_render_fields3(&out,
                        "family", report->family,
                        "source", report->source_status,
                        "artifact", report->artifact_status);
    yvex_render_fields2(&out,
                        "plan", "full-gguf planned",
                        "emission", report->artifact_emission_status);
    yvex_render_top_blocker(&out, report->top_blocker);
    yvex_render_next(&out, report->next);
    yvex_render_kv(&out,
                   "boundary",
                   "prepare dry-run only; no artifact emission/runtime/generation");
}

static void prepare_source_report_render_audit(
    const yvex_models_prepare_source_report *report)
{
    yvex_render_out out;

    yvex_render_out_init(&out, stdout, YVEX_RENDER_MODE_AUDIT);
    yvex_render_section(&out, "models: prepare");
    yvex_render_kv(&out, "target_id", report->target_id);
    yvex_render_kv(&out, "family", report->family);
    yvex_render_kv(&out, "provider", report->provider);
    yvex_render_kv(&out, "repo_id", report->repo_id);
    yvex_render_kv(&out, "revision", report->revision);
    yvex_render_kv(&out, "models_root", report->models_root);
    yvex_render_kv(&out, "source_path", report->source_path);
    yvex_render_kv(&out, "source_status", report->source_status);
    yvex_render_kv(&out, "source_manifest_path", report->source_manifest_path);
    yvex_render_kv(&out, "native_inventory_path", report->native_inventory_path);
    yvex_render_kv(&out, "model_class_status", report->model_class_status);
    yvex_render_kv(&out, "tensor_map_path", report->tensor_map_path);
    yvex_render_kv(&out, "tensor_map_status", report->tensor_map_status);
    yvex_render_kv(&out, "output_head_map_path", report->output_head_map_path);
    yvex_render_kv(&out, "output_head_map_status", report->output_head_map_status);
    yvex_render_kv(&out, "tokenizer_map_path", report->tokenizer_map_path);
    yvex_render_kv(&out, "tokenizer_map_status", report->tokenizer_map_status);
    yvex_render_kv(&out, "artifact_status", report->artifact_status);
    yvex_render_kv(&out, "expected_artifact_path", report->expected_artifact_path);
    yvex_render_kv(&out, "artifact_plan_status", report->artifact_plan_status);
    yvex_render_kv(&out, "artifact_emission_status", report->artifact_emission_status);
    yvex_render_kv(&out, "artifact_identity_status", report->artifact_identity_status);
    yvex_render_kv_u(&out, "prepare_blocker_count", report->blocker_count);
    yvex_render_top_blocker(&out, report->top_blocker);
    yvex_render_kv(&out, "download_registry_path", report->download_registry_path);
    yvex_render_kv(&out, "download_report_path", report->download_report_path);
    yvex_render_kv(&out, "downloaded_target_resolved",
                   report->downloaded_target_resolved ? "true" : "false");
    model_stage_print("target", "unsupported");
    model_print_runtime_generation("not-performed");
    yvex_render_kv(&out, "reason", report->reason);
    yvex_render_next(&out, report->next);
    yvex_render_status(&out, report->final_status);
}

static int print_prepare_downloaded_source_unsupported(
    const yvex_cli_models_prepare_options *options,
    const yvex_operator_paths *operator_paths,
    const yvex_model_download_resolved_target *target)
{
    yvex_models_prepare_source_report report;

    prepare_source_report_build(options, operator_paths, target, &report);
    if (!options || options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        prepare_source_report_render_porcelain(&report);
        return exit_for_status(YVEX_ERR_UNSUPPORTED);
    }
    prepare_source_report_render_audit(&report);
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int prepare_registry_verify(const yvex_model_registry_entry *entry,
                                   yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    int rc;

    if (!entry) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "prepared alias was not found after registration");
        return YVEX_ERR_STATE;
    }
    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(entry->path, &identity, err);
    if (rc != YVEX_OK) return rc;
    if (!entry->sha256 || strcmp(entry->sha256, identity.sha256) != 0 ||
        entry->file_size != identity.file_size) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "registered identity does not match emitted artifact");
        return YVEX_ERR_STATE;
    }
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));
    rc = populate_registry_metadata(&current_metadata, entry->path, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_model_registry_compare_metadata(entry, &current_metadata.entry, &metadata_report, err);
    if (rc != YVEX_OK) return rc;
    if (strcmp(metadata_report.metadata_status, "pass") != 0 ||
        strcmp(metadata_report.readiness_status, "pass") != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "model_prepare_registry", "registered metadata drifted immediately after prepare");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

int yvex_models_prepare_render_command(int arg_count, char **args)
{
    static const char *target_alias = "deepseek4-v4-flash-selected-embed";
    static const char *artifact_name = "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf";
    yvex_cli_models_prepare_options options;
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    yvex_source_manifest_options manifest_options;
    yvex_source_manifest_summary manifest_summary;
    yvex_native_weight_options native_options;
    yvex_native_weight_table *native_table = NULL;
    yvex_native_weight_summary native_summary;
    yvex_conversion_options conversion_options;
    yvex_conversion_summary conversion_summary;
    yvex_cli_metadata_snapshot metadata_snapshot;
    yvex_model_registry *registry = NULL;
    yvex_model_registry_entry derived;
    yvex_model_registry_entry entry;
    yvex_error err;
    char source_path[YVEX_PATH_CAP];
    char out_dir[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    char manifest_path[YVEX_PATH_CAP];
    char plan_path[YVEX_PATH_CAP];
    char registry_path_buf[YVEX_PATH_CAP];
    char registered_sha256[YVEX_SHA256_HEX_CAP];
    char registered_format[16];
    char registered_architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    const char *registry_path = NULL;
    int source_exists;
    int artifact_exists;
    int rc;

    rc = parse_models_prepare_options(arg_count, args, &options);
    if (rc != 0) return rc;

    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    yvex_error_clear(&err);
    rc = yvex_operator_paths_resolve(&paths, options.models_root, &operator_paths, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (strcmp(options.target, target_alias) != 0) {
        yvex_model_download_resolved_target downloaded;

        yvex_error_clear(&err);
        if (model_download_resolve_downloaded_target(options.target,
                                                     &operator_paths,
                                                     &downloaded,
                                                     &err)) {
            return print_prepare_downloaded_source_unsupported(&options,
                                                               &operator_paths,
                                                               &downloaded);
        }
        return print_prepare_unsupported(options.target);
    }

    rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "source",
                                            source_path, sizeof(source_path),
                                            &source_exists, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    if (options.source) {
        rc = expand_operator_path(options.source, source_path, sizeof(source_path), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        source_exists = path_exists(source_path);
    }

    if (options.out) {
        rc = expand_operator_path(options.out, artifact_path, sizeof(artifact_path), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (!path_parent_dir(artifact_path, out_dir, sizeof(out_dir))) {
            yvex_error_set(&err, YVEX_ERR_BOUNDS, "models_prepare", "output parent path is too long");
            return print_yvex_error(&err, exit_for_status(YVEX_ERR_BOUNDS));
        }
    } else {
        if (options.out_dir) {
            rc = expand_operator_path(options.out_dir, out_dir, sizeof(out_dir), &err, "models_prepare");
            if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        } else {
            rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "gguf",
                                                    out_dir, sizeof(out_dir),
                                                    &artifact_exists, &err);
            if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        }
        rc = path_join2(artifact_path, sizeof(artifact_path), out_dir, artifact_name, &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    }
    artifact_exists = path_exists(artifact_path);

    rc = path_join2(manifest_path, sizeof(manifest_path), out_dir, "deepseek-source-manifest.json", &err, "models_prepare");
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    rc = path_join2(plan_path, sizeof(plan_path), out_dir, "deepseek-selected-plan.json", &err, "models_prepare");
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    if (options.registry_path) {
        rc = expand_operator_path(options.registry_path, registry_path_buf, sizeof(registry_path_buf), &err, "models_prepare");
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        registry_path = registry_path_buf;
    } else {
        rc = yvex_model_registry_default_path(registry_path_buf, sizeof(registry_path_buf), &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        registry_path = registry_path_buf;
    }

    print_prepare_common(&options, &operator_paths, source_path, artifact_path,
                         manifest_path, plan_path, registry_path);

    if (options.dry_run) {
        print_prepare_dry_run_stages(options.register_alias, options.use_alias);
        model_print_runtime_generation("not-performed");
        yvex_cli_out_writef(stdout, "status: model-prepare-dry-run\n");
        return 0;
    }

    if (!source_exists) {
        model_stage_print("source-path", "fail");
        model_print_runtime_generation("not-performed");
        yvex_cli_out_writef(stdout, "reason: source path does not exist\n");
        yvex_cli_out_writef(stdout, "status: model-prepare-fail\n");
        return exit_for_status(YVEX_ERR_IO);
    }
    if (artifact_exists && !options.overwrite) {
        model_stage_print("convert-emit", "refused");
        model_print_runtime_generation("not-performed");
        yvex_cli_out_writef(stdout, "reason: artifact exists; pass --overwrite to replace it\n");
        yvex_cli_out_writef(stdout, "status: model-prepare-refused\n");
        return exit_for_status(YVEX_ERR_STATE);
    }

    rc = yvex_model_registry_mkdir_parent(artifact_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(manifest_path, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_mkdir_parent(plan_path, &err);
    if (rc == YVEX_OK && options.register_alias) rc = yvex_model_registry_mkdir_parent(registry_path, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));

    memset(&manifest_options, 0, sizeof(manifest_options));
    memset(&manifest_summary, 0, sizeof(manifest_summary));
    manifest_options.repo = yvex_deepseek_v4_upstream_repo_id;
    manifest_options.revision = "main";
    manifest_options.local_path = source_path;
    manifest_options.status = YVEX_SOURCE_STATUS_IN_PROGRESS;
    rc = yvex_source_manifest_write_json(manifest_path, &manifest_options, &manifest_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("source-manifest", "pass");

    memset(&native_options, 0, sizeof(native_options));
    memset(&native_summary, 0, sizeof(native_summary));
    native_options.source_dir = source_path;
    native_options.recursive = 1;
    rc = yvex_native_weight_table_open(&native_table, &native_options, &err);
    if (rc == YVEX_OK) rc = yvex_native_weight_table_summary(native_table, &native_summary, &err);
    if (rc == YVEX_OK && !yvex_native_weight_table_find(native_table, "embed.weight")) {
        yvex_error_set(&err, YVEX_ERR_STATE, "models_prepare", "required native tensor is missing: embed.weight");
        rc = YVEX_ERR_STATE;
    }
    if (rc != YVEX_OK) {
        yvex_native_weight_table_close(native_table);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    model_stage_print("native-weights", "pass");
    yvex_cli_out_writef(stdout, "native_tensor_count: %llu\n", native_summary.tensor_count);
    model_stage_print("tensor-map", "pass");
    yvex_native_weight_table_close(native_table);
    native_table = NULL;

    memset(&conversion_options, 0, sizeof(conversion_options));
    memset(&conversion_summary, 0, sizeof(conversion_summary));
    conversion_options.architecture = "deepseek4";
    conversion_options.source_manifest_path = manifest_path;
    conversion_options.native_source_dir = source_path;
    conversion_options.tensor_name = "embed.weight";
    conversion_options.target_qtype = "F16";
    conversion_options.out_path = artifact_path;
    conversion_options.overwrite = options.overwrite;
    rc = yvex_conversion_plan_write_json(&conversion_options, plan_path, &conversion_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("convert-plan", "pass");

    memset(&conversion_summary, 0, sizeof(conversion_summary));
    rc = yvex_conversion_emit_gguf(&conversion_options, &conversion_summary, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("convert-emit", "pass");
    yvex_cli_out_writef(stdout, "bytes_written: %llu\n", conversion_summary.bytes_written);

    memset(&metadata_snapshot, 0, sizeof(metadata_snapshot));
    rc = populate_registry_metadata(&metadata_snapshot, artifact_path, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    model_stage_print("inspect", "pass");
    model_stage_print("tensors", "pass");
    model_stage_print("metadata", "pass");
    yvex_cli_out_writef(stdout, "artifact_architecture: %s\n", metadata_snapshot.architecture);
    yvex_cli_out_writef(stdout, "artifact_tensor_count: %llu\n", metadata_snapshot.entry.tensor_count);

    if (options.register_alias) {
        memset(&derived, 0, sizeof(derived));
        memset(&entry, 0, sizeof(entry));
        memset(registered_sha256, 0, sizeof(registered_sha256));
        memset(registered_format, 0, sizeof(registered_format));
        memset(registered_architecture, 0, sizeof(registered_architecture));
        memset(primary_tensor_name, 0, sizeof(primary_tensor_name));
        memset(primary_tensor_role, 0, sizeof(primary_tensor_role));
        memset(primary_tensor_dtype, 0, sizeof(primary_tensor_dtype));
        memset(primary_tensor_dims, 0, sizeof(primary_tensor_dims));

        rc = yvex_model_registry_entry_derive_from_path(&derived, artifact_path, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        entry = derived;
        entry.support_level = "selected-tensor-materialized";
        rc = populate_registry_identity(&entry,
                                        registered_sha256,
                                        registered_format,
                                        registered_architecture,
                                        primary_tensor_name,
                                        primary_tensor_role,
                                        primary_tensor_dtype,
                                        primary_tensor_dims,
                                        &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        rc = models_registry_open(&registry, registry_path, 1, &err);
        if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
        if (yvex_model_registry_find(registry, target_alias)) {
            rc = yvex_model_registry_remove(registry, target_alias, &err);
            if (rc != YVEX_OK) {
                yvex_model_registry_close(registry);
                return print_yvex_error(&err, exit_for_status(rc));
            }
            model_stage_print("registry-remove-existing", "pass");
        } else {
            model_stage_print("registry-remove-existing", "not-found");
        }
        rc = yvex_model_registry_add(registry, &entry, &err);
        if (rc == YVEX_OK) model_stage_print("registry-add", "pass");
        if (rc == YVEX_OK && options.use_alias) {
            rc = yvex_model_registry_select(registry, target_alias, &err);
            if (rc == YVEX_OK) model_stage_print("registry-use", "pass");
        } else if (rc == YVEX_OK) {
            model_stage_print("registry-use", "skipped");
        }
        if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
        if (rc == YVEX_OK) {
            const yvex_model_registry_entry *registered = yvex_model_registry_find(registry, target_alias);
            rc = prepare_registry_verify(registered, &err);
            if (rc == YVEX_OK) model_stage_print("registry-verify", "pass");
        }
        if (rc != YVEX_OK) {
            yvex_model_registry_close(registry);
            return print_yvex_error(&err, exit_for_status(rc));
        }
        yvex_model_registry_close(registry);
        registry = NULL;
    } else {
        model_stage_print("registry-remove-existing", "skipped");
        model_stage_print("registry-add", "skipped");
        model_stage_print("registry-use", "skipped");
        model_stage_print("registry-verify", "skipped");
    }

    model_print_runtime_generation("not-performed");
    yvex_cli_out_writef(stdout, "status: model-prepare\n");
    return 0;
}

/* Native source tensor download lane. */

#define YVEX_MODEL_DOWNLOAD_PATTERN_CAP 32u

typedef enum {
    YVEX_CLI_MODEL_CHECK_QUICK = 0,
    YVEX_CLI_MODEL_CHECK_RUNTIME,
    YVEX_CLI_MODEL_CHECK_FULL
} yvex_cli_model_check_level;

/* Selected artifact check preset. */

typedef struct {
    const char *target;
    const char *backend_name;
    const char *level_name;
    const char *models_root;
    const char *registry_path;
    const char *report_dir;
    int no_materialize;
    int no_graph;
    yvex_cli_model_check_level level;
    yvex_models_output_mode output_mode;
} yvex_cli_models_check_options;

typedef struct {
    char target_id[256];
    char model_input_kind[32];
    char backend_name[16];
    char level_name[16];
    char artifact_path[YVEX_PATH_CAP];
    char registry_path[YVEX_PATH_CAP];
    char report_path[YVEX_PATH_CAP];
    char error[256];
    char graph_skip_reason[128];
    const char *stage_resolve_target;
    const char *stage_resolve_artifact;
    const char *stage_inspect;
    const char *stage_tensors;
    const char *stage_metadata;
    const char *stage_registry_identity;
    const char *stage_integrity_check;
    const char *stage_integrity_report;
    const char *stage_materialize;
    const char *stage_engine;
    const char *stage_session;
    const char *stage_plan;
    const char *stage_graph_partial;
    const char *stage_model_gate;
    const char *stage_materialize_gate;
    const char *runtime_execution;
    const char *final_status;
} yvex_cli_model_check_report;

static void model_check_report_init(yvex_cli_model_check_report *report,
                                    const yvex_cli_models_check_options *options)
{
    memset(report, 0, sizeof(*report));
    snprintf(report->target_id, sizeof(report->target_id), "%s",
             options->target ? options->target : "");
    snprintf(report->backend_name, sizeof(report->backend_name), "%s",
             options->backend_name ? options->backend_name : "cpu");
    snprintf(report->level_name, sizeof(report->level_name), "%s",
             options->level_name ? options->level_name : "quick");
    snprintf(report->model_input_kind, sizeof(report->model_input_kind), "unknown");
    report->stage_resolve_target = "not-run";
    report->stage_resolve_artifact = "not-run";
    report->stage_inspect = "not-run";
    report->stage_tensors = "not-run";
    report->stage_metadata = "not-run";
    report->stage_registry_identity = "skipped";
    report->stage_integrity_check = "not-run";
    report->stage_integrity_report = "skipped";
    report->stage_materialize = "skipped";
    report->stage_engine = "skipped";
    report->stage_session = "skipped";
    report->stage_plan = "skipped";
    report->stage_graph_partial = "skipped";
    report->stage_model_gate = "skipped";
    report->stage_materialize_gate = "skipped";
    report->runtime_execution = "not-performed";
    report->final_status = "model-check-fail";
}

static void model_check_report_print(FILE *fp,
                                     const yvex_cli_model_check_report *report)
{
    yvex_cli_out_writef(fp, "status: model-check\n");
    yvex_cli_out_writef(fp, "target_id: %s\n", report->target_id);
    yvex_cli_out_writef(fp, "model_input_kind: %s\n", report->model_input_kind);
    yvex_cli_out_writef(fp, "backend: %s\n", report->backend_name);
    yvex_cli_out_writef(fp, "level: %s\n", report->level_name);
    yvex_cli_out_writef(fp, "artifact_path: %s\n", report->artifact_path);
    yvex_cli_out_writef(fp, "registry_path: %s\n", report->registry_path);
    if (report->report_path[0]) {
        yvex_cli_out_writef(fp, "report_path: %s\n", report->report_path);
    }
    yvex_cli_out_writef(fp, "stage: resolve-target %s\n", report->stage_resolve_target);
    yvex_cli_out_writef(fp, "stage: resolve-artifact %s\n", report->stage_resolve_artifact);
    yvex_cli_out_writef(fp, "stage: inspect %s\n", report->stage_inspect);
    yvex_cli_out_writef(fp, "stage: tensors %s\n", report->stage_tensors);
    yvex_cli_out_writef(fp, "stage: metadata %s\n", report->stage_metadata);
    yvex_cli_out_writef(fp, "stage: registry-identity %s\n", report->stage_registry_identity);
    yvex_cli_out_writef(fp, "stage: integrity-check %s\n", report->stage_integrity_check);
    yvex_cli_out_writef(fp, "stage: integrity-report %s\n", report->stage_integrity_report);
    yvex_cli_out_writef(fp, "stage: materialize %s\n", report->stage_materialize);
    yvex_cli_out_writef(fp, "stage: engine %s\n", report->stage_engine);
    yvex_cli_out_writef(fp, "stage: session %s\n", report->stage_session);
    yvex_cli_out_writef(fp, "stage: plan %s\n", report->stage_plan);
    yvex_cli_out_writef(fp, "stage: graph-partial %s\n", report->stage_graph_partial);
    if (report->graph_skip_reason[0]) {
        yvex_cli_out_writef(fp, "graph_partial_reason: %s\n", report->graph_skip_reason);
    }
    yvex_cli_out_writef(fp, "stage: model-gate %s\n", report->stage_model_gate);
    yvex_cli_out_writef(fp, "stage: materialize-gate %s\n", report->stage_materialize_gate);
    if (report->error[0]) {
        yvex_cli_out_writef(fp, "error: %s\n", report->error);
    }
    yvex_cli_out_writef(fp, "runtime_execution: %s\n", report->runtime_execution);
    yvex_cli_out_writef(fp, "execution_ready: false\n");
    yvex_cli_out_writef(fp, "graph_execution_ready: false\n");
    yvex_cli_out_writef(fp, "prefill_ready: false\n");
    yvex_cli_out_writef(fp, "logits_ready: false\n");
    yvex_cli_out_writef(fp, "generation: unsupported\n");
    yvex_cli_out_writef(fp, "status: %s\n", report->final_status);
}

static int model_check_write_report(const yvex_cli_models_check_options *options,
                                    yvex_cli_model_check_report *report,
                                    yvex_error *err)
{
    FILE *fp;
    char report_dir[YVEX_PATH_CAP];
    char report_name[128];
    int rc;
    int n;

    if (!options->report_dir) {
        return YVEX_OK;
    }
    rc = expand_operator_path(options->report_dir, report_dir, sizeof(report_dir),
                              err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    n = snprintf(report_name, sizeof(report_name),
                 "model-check-deepseek4-v4-flash-selected-embed-%s-%s.txt",
                 report->backend_name, report->level_name);
    if (n < 0 || (size_t)n >= sizeof(report_name)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "models_check", "report filename is too long");
        return YVEX_ERR_BOUNDS;
    }
    rc = path_join2(report->report_path, sizeof(report->report_path),
                    report_dir, report_name, err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_model_registry_mkdir_parent(report->report_path, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    fp = fopen(report->report_path, "w");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "failed to open report: %s", report->report_path);
        return YVEX_ERR_IO;
    }
    model_check_report_print(fp, report);
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "failed to close report: %s", report->report_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

static void model_check_report_print_normal(FILE *fp,
                                            const yvex_cli_model_check_report *report)
{
    if (!fp || !report) return;
    yvex_cli_out_writef(fp, "model-check: %s target=%s level=%s\n",
            strcmp(report->final_status ? report->final_status : "", "model-check-pass") == 0
                ? "pass"
                : "fail",
            report->target_id,
            report->level_name);
    yvex_cli_out_writef(fp, "backend: %s\n", report->backend_name);
    yvex_cli_out_writef(fp, "checked: inspect, metadata, registry, integrity%s%s\n",
            strcmp(report->stage_materialize ? report->stage_materialize : "", "pass") == 0
                ? ", materialize"
                : "",
            strcmp(report->stage_graph_partial ? report->stage_graph_partial : "", "pass") == 0
                ? ", selected graph"
                : "");
    yvex_cli_out_writef(fp, "runtime: %s\n", report->runtime_execution ? report->runtime_execution : "not-performed");
    if (report->error[0]) {
        yvex_cli_out_writef(fp, "top_blocker: %s\n", report->error);
    } else {
        yvex_cli_out_writef(fp, "top_blocker: none\n");
    }
    yvex_cli_out_writef(fp, "boundary: selected-slice check only, generation unsupported\n");
    yvex_cli_out_writef(fp, "status: %s\n", report->final_status ? report->final_status : "model-check-fail");
}

static int parse_models_check_options(int arg_count,
                                      char **args,
                                      yvex_cli_models_check_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    options->backend_name = "cpu";
    options->level_name = "quick";
    options->level = YVEX_CLI_MODEL_CHECK_QUICK;
    options->output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models check requires TARGET\n");
        return 2;
    }
    options->target = args[3];
    if (!cli_arg_value_valid(options->target)) {
        yvex_cli_out_writef(stderr, "yvex: models check target is empty or invalid\n");
        return 2;
    }
    for (i = 4; i < arg_count; ++i) {
        if (strcmp(args[i], "--no-materialize") == 0) {
            options->no_materialize = 1;
        } else if (strcmp(args[i], "--no-graph") == 0) {
            options->no_graph = 1;
        } else if (strcmp(args[i], "--" "audit") == 0) {
            options->output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            const char *value = NULL;
            int rc = parse_models_value_option("models check", "--" "output",
                                               arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (!parse_models_output_mode(value, &options->output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: models check unsupported output mode: %s\n", value);
                return 2;
            }
        } else if (strcmp(args[i], "--backend") == 0 ||
                   strcmp(args[i], "--level") == 0 ||
                   strcmp(args[i], "--models-root") == 0 ||
                   strcmp(args[i], "--registry") == 0 ||
                   strcmp(args[i], "--report-dir") == 0) {
            const char *flag = args[i];
            const char *value = NULL;
            int rc = parse_models_value_option("models check", flag,
                                               arg_count, args, &i, &value);
            if (rc != 0) return rc;
            if (strcmp(flag, "--backend") == 0) {
                options->backend_name = value;
            } else if (strcmp(flag, "--level") == 0) {
                options->level_name = value;
            } else if (strcmp(flag, "--models-root") == 0) {
                options->models_root = value;
            } else if (strcmp(flag, "--registry") == 0) {
                options->registry_path = value;
            } else {
                options->report_dir = value;
            }
        } else if (args[i][0] == '-') {
            yvex_cli_out_writef(stderr, "yvex: unknown models check option: %s\n", args[i]);
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "yvex: models check accepts only one TARGET\n");
            return 2;
        }
    }
    if (strcmp(options->backend_name, "cpu") != 0 &&
        strcmp(options->backend_name, "cuda") != 0) {
        yvex_cli_out_writef(stderr, "yvex: unknown backend kind: %s\n", options->backend_name);
        return 2;
    }
    if (strcmp(options->level_name, "quick") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_QUICK;
    } else if (strcmp(options->level_name, "runtime") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_RUNTIME;
    } else if (strcmp(options->level_name, "full") == 0) {
        options->level = YVEX_CLI_MODEL_CHECK_FULL;
    } else {
        yvex_cli_out_writef(stderr, "yvex: unknown models check level: %s\n", options->level_name);
        return 2;
    }
    return 0;
}

static int print_model_check_unsupported(const yvex_cli_models_check_options *options)
{
    const char *target = options && options->target ? options->target : "";
    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "model-check: unsupported target=%s level=%s\n",
               target,
               options->level_name ? options->level_name : "quick");
        yvex_cli_out_writef(stdout, "backend: %s\n", options->backend_name ? options->backend_name : "cpu");
        yvex_cli_out_writef(stdout, "top_blocker: %s\n",
               strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0
                   ? "segment check is planned"
                   : "source-only target cannot be checked as a YVEX-produced runtime artifact yet");
        yvex_cli_out_writef(stdout, "boundary: report-only refusal, generation unsupported\n");
        yvex_cli_out_writef(stdout, "status: model-check-unsupported\n");
    } else {
        yvex_cli_out_writef(stdout, "status: model-check-unsupported\n");
        yvex_cli_out_writef(stdout, "target_id: %s\n", target);
        if (strcmp(target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0) {
            yvex_cli_out_writef(stdout, "reason: segment check is planned, not implemented by this preset\n");
        } else {
            yvex_cli_out_writef(stdout, "reason: source-only target cannot be checked as a YVEX-produced runtime artifact yet\n");
        }
        model_print_runtime_generation("unsupported");
    }
    return exit_for_status(YVEX_ERR_UNSUPPORTED);
}

static int model_check_resolve_registry_path(const yvex_cli_models_check_options *options,
                                             char *registry_path,
                                             size_t registry_path_cap,
                                             yvex_error *err)
{
    if (options->registry_path) {
        return expand_operator_path(options->registry_path, registry_path,
                                    registry_path_cap, err, "models_check");
    }
    return yvex_model_registry_default_path(registry_path,
                                            (unsigned long long)registry_path_cap,
                                            err);
}

static int model_check_resolve_canonical_path(
    const yvex_cli_models_check_options *options,
    yvex_model_ref *ref,
    yvex_cli_model_check_report *report,
    yvex_error *err)
{
    static const char *artifact_name =
        "deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf";
    yvex_paths paths;
    yvex_operator_paths operator_paths;
    char gguf_dir[YVEX_PATH_CAP];
    char artifact_path[YVEX_PATH_CAP];
    int exists = 0;
    int rc;

    memset(&paths, 0, sizeof(paths));
    memset(&operator_paths, 0, sizeof(operator_paths));
    rc = yvex_operator_paths_resolve(&paths, options->models_root, &operator_paths, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_operator_paths_resolve_target(&operator_paths, "deepseek", "gguf",
                                            gguf_dir, sizeof(gguf_dir), &exists, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = path_join2(artifact_path, sizeof(artifact_path), gguf_dir, artifact_name,
                    err, "models_check");
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!path_exists(artifact_path)) {
        yvex_error_setf(err, YVEX_ERR_IO, "models_check",
                        "canonical selected artifact does not exist: %s",
                        artifact_path);
        return YVEX_ERR_IO;
    }
    rc = set_path_ref(ref, artifact_path, err);
    if (rc == YVEX_OK) {
        snprintf(report->model_input_kind, sizeof(report->model_input_kind), "target");
    }
    return rc;
}

static int model_check_resolve_ref(const yvex_cli_models_check_options *options,
                                   const char *registry_path,
                                   yvex_model_ref *ref,
                                   yvex_cli_model_check_report *report,
                                   yvex_error *err)
{
    yvex_model_ref_options ref_options;
    int rc;

    if (options->models_root &&
        strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0 &&
        !is_path_like_reference(options->target)) {
        return model_check_resolve_canonical_path(options, ref, report, err);
    }

    memset(&ref_options, 0, sizeof(ref_options));
    ref_options.registry_path = registry_path;
    ref_options.allow_registry = 1;
    rc = yvex_model_ref_resolve(ref, options->target, &ref_options, err);
    if (rc == YVEX_OK) {
        if (is_path_like_reference(options->target)) {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "path");
        } else if (strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0) {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "target");
        } else {
            snprintf(report->model_input_kind, sizeof(report->model_input_kind), "alias");
        }
        return YVEX_OK;
    }
    if (strcmp(options->target, "deepseek4-v4-flash-selected-embed") == 0) {
        yvex_error_clear(err);
        yvex_model_ref_clear(ref);
        return model_check_resolve_canonical_path(options, ref, report, err);
    }
    return rc;
}

static int model_check_verify_registry_identity(const yvex_model_ref *ref,
                                                yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_registry_entry registered_metadata;
    yvex_model_metadata_drift_report metadata_report;
    int rc;

    if (!ref || ref->kind != YVEX_MODEL_REF_ALIAS) {
        return YVEX_OK;
    }
    memset(&identity, 0, sizeof(identity));
    memset(&current_metadata, 0, sizeof(current_metadata));
    memset(&registered_metadata, 0, sizeof(registered_metadata));
    memset(&metadata_report, 0, sizeof(metadata_report));

    rc = yvex_artifact_identity_read(ref->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!ref->sha256 || !ref->sha256[0] ||
        strcmp(ref->sha256, identity.sha256) != 0 ||
        (ref->registered_file_size != 0ull &&
         ref->registered_file_size != identity.file_size)) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "registry identity drift");
        return YVEX_ERR_STATE;
    }
    rc = populate_registry_metadata(&current_metadata, ref->path, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    model_ref_registry_entry_view(ref, &registered_metadata);
    rc = yvex_model_registry_compare_metadata(&registered_metadata,
                                              &current_metadata.entry,
                                              &metadata_report,
                                              err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (strcmp(metadata_report.metadata_status, "pass") != 0 ||
        strcmp(metadata_report.readiness_status, "pass") != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "registry identity drift");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_integrity(const yvex_model_ref *ref,
                                 int require_embedding,
                                 yvex_artifact_integrity_report *report,
                                 yvex_error *err)
{
    yvex_artifact_integrity_options integrity_options;

    memset(&integrity_options, 0, sizeof(integrity_options));
    integrity_options.require_token_embedding = require_embedding;
    integrity_options.token_id = 0u;
    if (ref->kind == YVEX_MODEL_REF_ALIAS && ref->sha256 && ref->sha256[0]) {
        integrity_options.registered_sha256 = ref->sha256;
    }
    return yvex_artifact_integrity_check_path(ref->path, &integrity_options,
                                             report, err);
}

static int model_check_backend_probe(const char *backend_name, yvex_error *err)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    int rc;

    memset(&options, 0, sizeof(options));
    (void)model_backend_kind_from_name(backend_name, &options.kind);
    rc = yvex_backend_open(&backend, &options, err);
    if (rc == YVEX_OK) {
        yvex_backend_close(backend);
    }
    return rc;
}

static int model_check_materialize(const char *path,
                                   const char *backend_name,
                                   yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_backend *backend = NULL;
    yvex_weight_table *weights = NULL;
    yvex_backend_options backend_options;
    yvex_materialize_options materialize_options;
    yvex_materialize_summary summary;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&materialize_options, 0, sizeof(materialize_options));
    memset(&summary, 0, sizeof(summary));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)model_backend_kind_from_name(backend_name, &backend_options.kind);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc != YVEX_OK) {
        close_model_context(&ctx);
        return rc;
    }
    materialize_options.backend_name = backend_name;
    materialize_options.require_all_tensors = 1;
    rc = yvex_weight_table_materialize(&weights,
                                       ctx.artifact,
                                       ctx.gguf,
                                       ctx.table,
                                       backend,
                                       &materialize_options,
                                       err);
    if (rc == YVEX_OK) {
        rc = yvex_weight_table_get_summary(weights, &summary, err);
    }
    if (rc == YVEX_OK && summary.status != YVEX_WEIGHT_STATUS_MATERIALIZED) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "materialization did not reach weights-materialized");
        rc = YVEX_ERR_STATE;
    }
    yvex_weight_table_close(weights);
    yvex_backend_close(backend);
    close_model_context(&ctx);
    return rc;
}

static int model_check_engine(const char *path,
                              const char *backend_name,
                              yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_engine_options options;
    yvex_engine_summary summary;
    int rc;

    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    options.model_path = path;
    options.load_tokenizer = 0;
    options.build_descriptor = 1;
    options.build_default_graph = 1;
    options.attach_weights = 1;
    options.backend_name = backend_name;
    options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &options, err);
    if (rc == YVEX_OK) {
        rc = yvex_engine_get_summary(engine, &summary, err);
    }
    if (rc == YVEX_OK && !summary.weights_attached) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "engine did not attach selected weights");
        rc = YVEX_ERR_STATE;
    }
    yvex_engine_close(engine);
    return rc;
}

static int model_check_session(const char *path,
                               const char *backend_name,
                               yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_backend *backend = NULL;
    yvex_session *session = NULL;
    yvex_engine_options engine_options;
    yvex_backend_options backend_options;
    yvex_session_options session_options;
    yvex_session_summary summary;
    int rc;

    memset(&engine_options, 0, sizeof(engine_options));
    memset(&backend_options, 0, sizeof(backend_options));
    memset(&session_options, 0, sizeof(session_options));
    memset(&summary, 0, sizeof(summary));
    engine_options.model_path = path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)model_backend_kind_from_name(backend_name, &backend_options.kind);
    rc = yvex_backend_open(&backend, &backend_options, err);
    if (rc == YVEX_OK) {
        session_options.allow_partial_graph = 1;
        rc = yvex_session_create(&session, engine, backend, &session_options, err);
    }
    if (rc == YVEX_OK) {
        rc = yvex_session_get_summary(session, &summary, err);
    }
    if (rc == YVEX_OK && !summary.weights_attached) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "session did not observe attached weights");
        rc = YVEX_ERR_STATE;
    }
    yvex_session_close(session);
    yvex_backend_close(backend);
    yvex_engine_close(engine);
    return rc;
}

static int model_check_plan(const char *path,
                            const char *backend_name,
                            yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    yvex_plan *plan = NULL;
    yvex_plan_options options;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&options, 0, sizeof(options));
    options.sequence_length = 1ull;
    options.context_length = 16ull;
    options.backend_name = backend_name;
    rc = open_model_context(path, &ctx, err);
    if (rc == YVEX_OK) {
        rc = yvex_plan_create(&plan, ctx.model, ctx.table, &options, err);
    }
    yvex_plan_close(plan);
    close_model_context(&ctx);
    return rc;
}

static int model_check_graph_partial(const char *path,
                                     const char *backend_name,
                                     yvex_error *err)
{
    yvex_engine *engine = NULL;
    yvex_engine_options engine_options;
    yvex_partial_graph_options partial_options;
    yvex_partial_graph_result partial_result;
    int rc;

    memset(&engine_options, 0, sizeof(engine_options));
    memset(&partial_options, 0, sizeof(partial_options));
    memset(&partial_result, 0, sizeof(partial_result));
    engine_options.model_path = path;
    engine_options.load_tokenizer = 0;
    engine_options.build_descriptor = 1;
    engine_options.build_default_graph = 1;
    engine_options.attach_weights = 1;
    engine_options.backend_name = backend_name;
    engine_options.require_all_weights = 1;
    partial_options.token_id = 0u;
    rc = yvex_engine_open(&engine, &engine_options, err);
    if (rc == YVEX_OK) {
        rc = yvex_engine_execute_partial_graph(engine, &partial_options,
                                              &partial_result, err);
    }
    if (rc == YVEX_OK && !partial_result.executed) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "selected partial graph did not execute");
        rc = YVEX_ERR_STATE;
    }
    yvex_engine_close(engine);
    return rc;
}

static int model_check_is_real_selected_embedding(
    const yvex_cli_metadata_snapshot *metadata)
{
    return metadata &&
           strcmp(metadata->entry.primary_tensor_name, "token_embd.weight") == 0 &&
           strcmp(metadata->entry.primary_tensor_dtype, "F16") == 0 &&
           metadata->entry.primary_tensor_rank == 2u &&
           strcmp(metadata->entry.primary_tensor_dims, "[4096,129280]") == 0 &&
           metadata->entry.primary_tensor_bytes == 1059061760ull;
}

static int model_check_run_model_gate(const yvex_model_ref *ref,
                                      const char *backend_name,
                                      yvex_error *err)
{
    yvex_model_gate_expected_tensor expected;
    yvex_model_gate_options options;
    yvex_model_gate_summary summary;
    int rc;

    memset(&expected, 0, sizeof(expected));
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    expected.name = "token_embd.weight";
    expected.dtype = "F16";
    expected.rank = 2u;
    expected.dims[0] = 4096ull;
    expected.dims[1] = 129280ull;
    expected.bytes = 1059061760ull;
    options.model_path = ref->path;
    options.model_label = "deepseek-v4-flash-selected-embedding";
    options.family = "deepseek4";
    options.artifact_sha256 = ref->kind == YVEX_MODEL_REF_ALIAS ? ref->sha256 : NULL;
    options.expected_tensors = &expected;
    options.expected_tensor_count = 1ull;
    options.check_cpu = strcmp(backend_name, "cpu") == 0;
    options.check_cuda = strcmp(backend_name, "cuda") == 0;
    options.require_cpu = options.check_cpu;
    options.require_cuda = options.check_cuda;
    rc = yvex_model_gate_check(&options, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (summary.status != YVEX_MODEL_GATE_PASS) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "model gate did not pass");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_run_materialize_gate(const yvex_model_ref *ref,
                                            const char *backend_name,
                                            yvex_error *err)
{
    yvex_materialize_expected_tensor expected;
    yvex_materialize_gate_options options;
    yvex_materialize_gate_summary summary;
    int rc;

    memset(&expected, 0, sizeof(expected));
    memset(&options, 0, sizeof(options));
    memset(&summary, 0, sizeof(summary));
    expected.name = "token_embd.weight";
    expected.dtype = "F16";
    expected.rank = 2u;
    expected.dims[0] = 4096ull;
    expected.dims[1] = 129280ull;
    expected.bytes = 1059061760ull;
    options.model_path = ref->path;
    options.label = "deepseek-v4-flash-selected-embedding";
    options.family = "deepseek4";
    options.sha256 = ref->kind == YVEX_MODEL_REF_ALIAS ? ref->sha256 : NULL;
    options.metadata_status = "pass";
    options.scope = YVEX_MATERIALIZE_SCOPE_SELECTED_TENSOR;
    options.expected_tensors = &expected;
    options.expected_tensor_count = 1ull;
    options.check_cpu = strcmp(backend_name, "cpu") == 0;
    options.check_cuda = strcmp(backend_name, "cuda") == 0;
    options.require_cpu = options.check_cpu;
    options.require_cuda = options.check_cuda;
    options.repeat_count = 1u;
    options.check_cleanup = 1;
    rc = yvex_materialize_gate_check(&options, &summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (summary.status != YVEX_MATERIALIZE_GATE_PASS) {
        yvex_error_set(err, YVEX_ERR_STATE, "models_check",
                       "materialize gate did not pass");
        return YVEX_ERR_STATE;
    }
    return YVEX_OK;
}

static int model_check_finish(yvex_cli_models_check_options *options,
                              yvex_cli_model_check_report *report,
                              int exit_code,
                              yvex_error *err)
{
    int rc;

    rc = model_check_write_report(options, report, err);
    if (rc != YVEX_OK && exit_code == 0) {
        snprintf(report->error, sizeof(report->error), "%s", yvex_error_message(err));
        report->final_status = "model-check-fail";
        exit_code = exit_for_status(rc);
    }
    if (options && options->output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        model_check_report_print_normal(stdout, report);
    } else {
        model_check_report_print(stdout, report);
    }
    return exit_code;
}

int yvex_models_check_render_command(int arg_count, char **args)
{
    yvex_cli_models_check_options options;
    yvex_cli_model_check_report report;
    yvex_model_ref ref;
    yvex_cli_tokenizer_context ctx;
    yvex_cli_metadata_snapshot metadata_snapshot;
    yvex_artifact_integrity_report integrity_report;
    yvex_error err;
    char registry_path[YVEX_PATH_CAP];
    int rc;
    int exit_code = 0;

    yvex_error_clear(&err);
    memset(&ref, 0, sizeof(ref));
    memset(&ctx, 0, sizeof(ctx));
    memset(&metadata_snapshot, 0, sizeof(metadata_snapshot));
    memset(&integrity_report, 0, sizeof(integrity_report));
    memset(registry_path, 0, sizeof(registry_path));

    rc = parse_models_check_options(arg_count, args, &options);
    if (rc != 0) {
        return rc;
    }
    if (strcmp(options.target, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0 ||
        strcmp(options.target, "glm-5.2-official-safetensors") == 0) {
        return print_model_check_unsupported(&options);
    }
    if (strcmp(options.target, "deepseek4-v4-flash-selected-embed") != 0 &&
        !is_path_like_reference(options.target)) {
        yvex_model_ref_options ref_options;
        memset(&ref_options, 0, sizeof(ref_options));
        ref_options.allow_registry = 1;
        ref_options.registry_path = options.registry_path;
        rc = yvex_model_ref_resolve(&ref, options.target, &ref_options, &err);
        if (rc != YVEX_OK) {
            return print_yvex_error(&err, exit_for_status(rc));
        }
        yvex_model_ref_clear(&ref);
    }

    model_check_report_init(&report, &options);
    rc = model_check_resolve_registry_path(&options, registry_path,
                                           sizeof(registry_path), &err);
    if (rc != YVEX_OK) {
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    snprintf(report.registry_path, sizeof(report.registry_path), "%s", registry_path);

    rc = model_check_resolve_ref(&options, registry_path, &ref, &report, &err);
    if (rc != YVEX_OK) {
        report.stage_resolve_target = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_resolve_target = "pass";
    snprintf(report.artifact_path, sizeof(report.artifact_path), "%s", ref.path);
    report.stage_resolve_artifact = path_exists(ref.path) ? "pass" : "fail";
    if (strcmp(report.stage_resolve_artifact, "pass") != 0) {
        snprintf(report.error, sizeof(report.error), "artifact path does not exist");
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  exit_for_status(YVEX_ERR_IO), &err);
    }

    rc = open_model_context(ref.path, &ctx, &err);
    if (rc != YVEX_OK) {
        report.stage_inspect = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_inspect = "pass";
    report.stage_tensors = "pass";

    rc = populate_registry_metadata(&metadata_snapshot, ref.path, &err);
    if (rc != YVEX_OK) {
        report.stage_metadata = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_metadata = "pass";

    if (ref.kind == YVEX_MODEL_REF_ALIAS) {
        rc = model_check_verify_registry_identity(&ref, &err);
        if (rc != YVEX_OK) {
            report.stage_registry_identity = "fail";
            snprintf(report.error, sizeof(report.error), "registry identity drift");
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report,
                                      exit_for_status(YVEX_ERR_STATE), &err);
        }
        report.stage_registry_identity = "pass";
    } else {
        report.stage_registry_identity = "unregistered";
    }

    rc = model_check_integrity(&ref, 0, &integrity_report, &err);
    if (rc != YVEX_OK || !integrity_report.passed) {
        report.stage_integrity_check = "fail";
        snprintf(report.error, sizeof(report.error), "%s",
                 rc != YVEX_OK ? yvex_error_message(&err) : "artifact integrity failed");
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  exit_for_status(rc != YVEX_OK ? rc : YVEX_ERR_FORMAT),
                                  &err);
    }
    report.stage_integrity_check = "pass";

    if (options.level == YVEX_CLI_MODEL_CHECK_QUICK) {
        if (options.no_graph) {
            snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                     "quick level does not run graph");
        }
        report.final_status = "model-check-pass";
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, 0, &err);
    }

    rc = model_check_backend_probe(options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_integrity_report = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_integrity_report = "pass";

    if (options.no_materialize) {
        report.stage_materialize = "skipped";
        report.stage_engine = "skipped";
        report.stage_session = "skipped";
        report.stage_plan = "skipped";
        report.stage_graph_partial = "skipped";
        snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                 "disabled by --no-materialize");
        report.stage_model_gate = "skipped";
        report.stage_materialize_gate = "skipped";
        report.runtime_execution = "not-performed";
        report.final_status = "model-check-pass";
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, 0, &err);
    }

    rc = model_check_materialize(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_materialize = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_materialize = "pass";

    rc = model_check_engine(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_engine = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_engine = "pass";

    rc = model_check_session(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_session = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report,
                                  rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                  &err);
    }
    report.stage_session = "pass";

    rc = model_check_plan(ref.path, options.backend_name, &err);
    if (rc != YVEX_OK) {
        report.stage_plan = "fail";
        snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
        close_model_context(&ctx);
        yvex_model_ref_clear(&ref);
        return model_check_finish(&options, &report, exit_for_status(rc), &err);
    }
    report.stage_plan = "pass";

    if (options.no_graph) {
        report.stage_graph_partial = "skipped";
        snprintf(report.graph_skip_reason, sizeof(report.graph_skip_reason),
                 "disabled by --no-graph");
    } else {
        rc = model_check_graph_partial(ref.path, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_graph_partial = rc == YVEX_ERR_UNSUPPORTED ? "unsupported" : "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report,
                                      rc == YVEX_ERR_UNSUPPORTED ? 5 : exit_for_status(rc),
                                      &err);
        }
        report.stage_graph_partial = "pass";
    }
    report.runtime_execution = "selected-boundary-only";

    if (options.level == YVEX_CLI_MODEL_CHECK_FULL &&
        model_check_is_real_selected_embedding(&metadata_snapshot) &&
        !options.no_materialize) {
        rc = model_check_run_model_gate(&ref, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_model_gate = "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report, exit_for_status(rc), &err);
        }
        report.stage_model_gate = "pass";
        rc = model_check_run_materialize_gate(&ref, options.backend_name, &err);
        if (rc != YVEX_OK) {
            report.stage_materialize_gate = "fail";
            snprintf(report.error, sizeof(report.error), "%s", yvex_error_message(&err));
            close_model_context(&ctx);
            yvex_model_ref_clear(&ref);
            return model_check_finish(&options, &report, exit_for_status(rc), &err);
        }
        report.stage_materialize_gate = "pass";
    } else if (options.level == YVEX_CLI_MODEL_CHECK_FULL) {
        report.stage_model_gate = "skipped";
        report.stage_materialize_gate = "skipped";
    }

    report.final_status = "model-check-pass";
    close_model_context(&ctx);
    yvex_model_ref_clear(&ref);
    return model_check_finish(&options, &report, exit_code, &err);
}
