/*
 * yvex_models_surface.c - models namespace CLI surface.
 *
 * Owner: src/cli/model_artifacts
 * Owns: models namespace routing and registry-backed models commands.
 * Does not own: download lifecycle, prepare/check gates, artifact reports, runtime generation, or release claims.
 * Invariants: preserves existing models command syntax; CLI-only and excluded from libyvex.a.
 * Boundary: registry command output is operator CLI surface, not domain ownership.
 */
#include "yvex_models_surface.h"
#include "yvex_models_artifacts_surface.h"
#include "yvex_models_download_surface.h"
#include "yvex_models_prepare_surface.h"

static int parse_models_registry_option(int arg_count, char **args, int start, const char **registry_path)
{
    int i;

    for (i = start; i < arg_count; ++i) {
        if (strcmp(args[i], "--registry") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = args[++i];
        } else if (strcmp(args[i], "--" "json") == 0) {
            /* Reserved for future machine-readable output; text output remains canonical. */
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown models option: %s\n", args[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_models_registry_output_options(int arg_count,
                                                char **args,
                                                int start,
                                                const char **registry_path,
                                                yvex_models_output_mode *output_mode)
{
    int i;

    if (output_mode) {
        *output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    }
    for (i = start; i < arg_count; ++i) {
        if (strcmp(args[i], "--registry") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = args[++i];
        } else if (strcmp(args[i], "--" "audit") == 0) {
            if (output_mode) *output_mode = YVEX_MODELS_OUTPUT_AUDIT;
        } else if (strcmp(args[i], "--" "output") == 0) {
            if (i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: models --" "output requires normal|table|audit\n");
                return 2;
            }
            if (!output_mode || !parse_models_output_mode(args[++i], output_mode)) {
                yvex_cli_out_writef(stderr, "yvex: models unsupported output mode: %s\n", args[i]);
                return 2;
            }
        } else if (strcmp(args[i], "--" "json") == 0) {
            yvex_cli_out_writef(stderr, "yvex: models JSON output is unsupported; use --" "output normal|table|audit\n");
            return 2;
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown models option: %s\n", args[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_models_add_options(int arg_count, char **args,
                                    yvex_cli_models_add_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 3; i < arg_count; ++i) {
        if (i + 1 >= arg_count) {
            yvex_cli_out_writef(stderr, "yvex: models add option requires a value: %s\n", args[i]);
            return 2;
        }
        if (strcmp(args[i], "--registry") == 0) options->registry_path = args[++i];
        else if (strcmp(args[i], "--path") == 0) options->path = args[++i];
        else if (strcmp(args[i], "--alias") == 0) options->alias = args[++i];
        else if (strcmp(args[i], "--family") == 0) options->family = args[++i];
        else if (strcmp(args[i], "--model") == 0) options->model = args[++i];
        else if (strcmp(args[i], "--scope") == 0) options->scope = args[++i];
        else if (strcmp(args[i], "--class") == 0) options->artifact_class = args[++i];
        else if (strcmp(args[i], "--qprofile") == 0) options->qprofile = args[++i];
        else if (strcmp(args[i], "--calibration") == 0) options->calibration = args[++i];
        else if (strcmp(args[i], "--sha256") == 0) options->sha256 = args[++i];
        else if (strcmp(args[i], "--support-level") == 0) options->support_level = args[++i];
        else {
            yvex_cli_out_writef(stderr, "yvex: unknown models add option: %s\n", args[i]);
            return 2;
        }
    }
    return 0;
}

/* Registry-backed models subcommands. */

static int command_models_scan(int arg_count, char **args)
{
    yvex_model_registry_entry *entries = NULL;
    yvex_error err;
    const char *root = NULL;
    const char *registry_path = NULL;
    unsigned long long count = 0;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    for (i = 3; (int)i < arg_count; ++i) {
        if (strcmp(args[i], "--root") == 0) {
            if ((int)i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: models scan --root requires a directory\n");
                return 2;
            }
            root = args[++i];
        } else if (strcmp(args[i], "--registry") == 0) {
            if ((int)i + 1 >= arg_count) {
                yvex_cli_out_writef(stderr, "yvex: models scan --registry requires a file\n");
                return 2;
            }
            registry_path = args[++i];
        } else if (strcmp(args[i], "--" "json") == 0) {
            /* Reserved for model selection work compatibility; text output remains canonical. */
        } else {
            yvex_cli_out_writef(stderr, "yvex: unknown models scan option: %s\n", args[i]);
            return 2;
        }
    }
    (void)registry_path;
    if (!root) {
        yvex_cli_out_writef(stderr, "yvex: models scan requires --root DIR\n");
        return 2;
    }
    rc = yvex_model_registry_scan_root(root, &entries, &count, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    yvex_cli_out_writef(stdout, "models: scan\n");
    yvex_cli_out_writef(stdout, "root: %s\n", root);
    for (i = 0; i < count; ++i) {
        if (i > 0) yvex_cli_out_writef(stdout, "\n");
        print_model_registry_scan_entry_cli(&entries[i]);
    }
    yvex_cli_out_writef(stdout, "candidates: %llu\n", count);
    yvex_cli_out_writef(stdout, "status: models-scan\n");
    yvex_model_registry_scan_free(entries, count);
    return 0;
}

static int command_models_add(int arg_count, char **args)
{
    yvex_cli_models_add_options cli_options;
    yvex_model_registry *registry = NULL;
    yvex_model_registry_entry derived;
    yvex_model_registry_entry entry;
    yvex_error err;
    char registered_sha256[YVEX_SHA256_HEX_CAP];
    char registered_format[16];
    char registered_architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    int have_derived = 0;
    int rc;

    yvex_error_clear(&err);
    memset(&derived, 0, sizeof(derived));
    memset(&entry, 0, sizeof(entry));
    memset(registered_sha256, 0, sizeof(registered_sha256));
    memset(registered_format, 0, sizeof(registered_format));
    memset(registered_architecture, 0, sizeof(registered_architecture));
    memset(primary_tensor_name, 0, sizeof(primary_tensor_name));
    memset(primary_tensor_role, 0, sizeof(primary_tensor_role));
    memset(primary_tensor_dtype, 0, sizeof(primary_tensor_dtype));
    memset(primary_tensor_dims, 0, sizeof(primary_tensor_dims));
    rc = parse_models_add_options(arg_count, args, &cli_options);
    if (rc != 0) return rc;
    if (!cli_options.path) {
        yvex_cli_out_writef(stderr, "yvex: models add requires --path FILE\n");
        return 2;
    }
    if (yvex_model_registry_entry_derive_from_path(&derived, cli_options.path, &err) == YVEX_OK) {
        have_derived = 1;
    } else {
        yvex_error_clear(&err);
    }
    if (!cli_options.alias && !have_derived) {
        yvex_cli_out_writef(stderr, "yvex: models add requires --alias when filename is not canonical\n");
        return 2;
    }
    entry.alias = cli_options.alias ? cli_options.alias : derived.alias;
    entry.family = cli_options.family ? cli_options.family : (have_derived ? derived.family : "");
    entry.model = cli_options.model ? cli_options.model : (have_derived ? derived.model : "");
    entry.scope = cli_options.scope ? cli_options.scope : (have_derived ? derived.scope : "");
    entry.artifact_class = cli_options.artifact_class ? cli_options.artifact_class : (have_derived ? derived.artifact_class : "");
    entry.qprofile = cli_options.qprofile ? cli_options.qprofile : (have_derived ? derived.qprofile : "");
    entry.calibration = cli_options.calibration ? cli_options.calibration : (have_derived ? derived.calibration : "");
    entry.producer = have_derived ? derived.producer : "yvex";
    entry.schema_version = have_derived ? derived.schema_version : "v1";
    entry.path = cli_options.path;
    entry.sha256 = "";
    entry.file_size = 0ull;
    entry.format = "";
    entry.architecture = "";
    entry.tensor_count = 0ull;
    entry.known_tensor_bytes = 0ull;
    entry.primary_tensor_name = "";
    entry.primary_tensor_role = "";
    entry.primary_tensor_dtype = "";
    entry.primary_tensor_rank = 0u;
    entry.primary_tensor_dims = "";
    entry.primary_tensor_bytes = 0ull;
    entry.support_level = cli_options.support_level ? cli_options.support_level : "";
    entry.selected_embedding_ready = 0;
    entry.selected_embedding_hidden_size = 0ull;
    entry.selected_embedding_vocab_size = 0ull;
    entry.selected_embedding_output_count = 0ull;
    entry.selected_embedding_slice_bytes = 0ull;
    entry.execution_ready = 0;

    rc = populate_registry_identity(&entry,
                                    registered_sha256,
                                    registered_format,
                                    registered_architecture,
                                    primary_tensor_name,
                                    primary_tensor_role,
                                    primary_tensor_dtype,
                                    primary_tensor_dims,
                                    &err);
    if (rc != YVEX_OK) {
        return print_yvex_error(&err, exit_for_status(rc));
    }
    if (cli_options.sha256 && cli_options.sha256[0] &&
        strcmp(cli_options.sha256, registered_sha256) != 0) {
        yvex_error_setf(&err, YVEX_ERR_STATE, "models_add_identity",
                        "sha256 mismatch: expected %s got %s",
                        cli_options.sha256, registered_sha256);
        return print_yvex_error(&err, exit_for_status(YVEX_ERR_STATE));
    }

    rc = models_registry_open(&registry, cli_options.registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_add(registry, &entry, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, cli_options.registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    yvex_cli_out_writef(stdout, "models: add\n");
    yvex_cli_out_writef(stdout, "alias: %s\n", entry.alias);
    yvex_cli_out_writef(stdout, "path: %s\n", entry.path);
    yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", entry.file_size);
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n", entry.sha256);
    yvex_cli_out_writef(stdout, "registered_format: %s\n", entry.format);
    yvex_cli_out_writef(stdout, "registered_architecture: %s\n", entry.architecture);
    yvex_cli_out_writef(stdout, "registered_tensor_count: %llu\n", entry.tensor_count);
    yvex_cli_out_writef(stdout, "registered_known_tensor_bytes: %llu\n", entry.known_tensor_bytes);
    yvex_cli_out_writef(stdout, "registered_primary_tensor: %s\n", entry.primary_tensor_name);
    yvex_cli_out_writef(stdout, "registered_primary_role: %s\n", entry.primary_tensor_role);
    yvex_cli_out_writef(stdout, "registered_primary_dtype: %s\n", entry.primary_tensor_dtype);
    yvex_cli_out_writef(stdout, "registered_primary_rank: %u\n", entry.primary_tensor_rank);
    yvex_cli_out_writef(stdout, "registered_primary_dims: %s\n", entry.primary_tensor_dims);
    yvex_cli_out_writef(stdout, "registered_primary_bytes: %llu\n", entry.primary_tensor_bytes);
    yvex_cli_out_writef(stdout, "registered_selected_embedding_ready: %s\n",
           entry.selected_embedding_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "registered_selected_embedding_hidden_size: %llu\n",
           entry.selected_embedding_hidden_size);
    yvex_cli_out_writef(stdout, "registered_selected_embedding_vocab_size: %llu\n",
           entry.selected_embedding_vocab_size);
    yvex_cli_out_writef(stdout, "registered_selected_embedding_output_count: %llu\n",
           entry.selected_embedding_output_count);
    yvex_cli_out_writef(stdout, "registered_selected_embedding_slice_bytes: %llu\n",
           entry.selected_embedding_slice_bytes);
    yvex_cli_out_writef(stdout, "identity_status: recorded\n");
    yvex_cli_out_writef(stdout, "status: models-added\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_list(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const yvex_model_registry_entry *selected;
    char selected_alias[256];
    unsigned long long i;
    unsigned long long count;
    int rc;

    yvex_error_clear(&err);
    rc = parse_models_registry_output_options(arg_count, args, 3, &registry_path, &output_mode);
    if (rc != 0) return rc;
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    selected_alias[0] = '\0';
    if (selected && selected->alias) {
        snprintf(selected_alias, sizeof(selected_alias), "%s", selected->alias);
    }
    count = yvex_model_registry_count(registry);
    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "MODELS  count=%llu\n\n", count);
        yvex_cli_out_writef(stdout, "%-3s  %-44s  %-10s  %-16s  %7s  %12s  %s\n",
               "SEL",
               "ALIAS",
               "FAMILY",
               "CLASS",
               "TENSORS",
               "SIZE",
               "READY");
        for (i = 0; i < count; ++i) {
            const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
            int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
            print_model_registry_entry_cli(entry, is_selected);
        }
        yvex_cli_out_writef(stdout, "status: models-list\n");
        yvex_model_registry_close(registry);
        return 0;
    }
    yvex_cli_out_writef(stdout, "models: list\n");
    for (i = 0; i < count; ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
        print_model_registry_entry_audit(entry, is_selected);
    }
    yvex_cli_out_writef(stdout, "count: %llu\n", count);
    yvex_cli_out_writef(stdout, "status: models-list\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_use(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models use requires ALIAS\n");
        return 2;
    }
    alias = args[3];
    rc = parse_models_registry_option(arg_count, args, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_select(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    yvex_cli_out_writef(stdout, "models: use\n");
    yvex_cli_out_writef(stdout, "selected: %s\n", alias);
    yvex_cli_out_writef(stdout, "status: models-selected\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_current(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const yvex_model_registry_entry *selected;
    int rc;

    rc = parse_models_registry_output_options(arg_count, args, 3, &registry_path, &output_mode);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    yvex_cli_out_writef(stdout, "models: current\n");
    if (selected) {
        if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            yvex_cli_out_writef(stdout, "selected: %s\n", selected->alias);
            yvex_cli_out_writef(stdout, "path: %s\n", selected->path);
            yvex_cli_out_writef(stdout, "execution_ready: %s\n", selected->execution_ready ? "true" : "false");
            yvex_cli_out_writef(stdout, "status: models-current\n");
            yvex_cli_out_writef(stdout, "hint: use --" "audit for registered digest and tensor fields\n");
            yvex_model_registry_close(registry);
            return 0;
        }
        yvex_cli_out_writef(stdout, "selected: %s\n", selected->alias);
        yvex_cli_out_writef(stdout, "path: %s\n", selected->path);
        yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", selected->file_size);
        yvex_cli_out_writef(stdout, "registered_sha256: %s\n", selected->sha256 && selected->sha256[0] ? selected->sha256 : "absent");
        yvex_cli_out_writef(stdout, "registered_format: %s\n", selected->format ? selected->format : "");
        yvex_cli_out_writef(stdout, "registered_architecture: %s\n", selected->architecture ? selected->architecture : "");
        yvex_cli_out_writef(stdout, "registered_tensor_count: %llu\n", selected->tensor_count);
        yvex_cli_out_writef(stdout, "registered_known_tensor_bytes: %llu\n", selected->known_tensor_bytes);
        yvex_cli_out_writef(stdout, "registered_primary_tensor: %s\n", selected->primary_tensor_name ? selected->primary_tensor_name : "");
        yvex_cli_out_writef(stdout, "registered_primary_role: %s\n", selected->primary_tensor_role ? selected->primary_tensor_role : "");
        yvex_cli_out_writef(stdout, "registered_primary_dtype: %s\n", selected->primary_tensor_dtype ? selected->primary_tensor_dtype : "");
        yvex_cli_out_writef(stdout, "registered_primary_rank: %u\n", selected->primary_tensor_rank);
        yvex_cli_out_writef(stdout, "registered_primary_dims: %s\n", selected->primary_tensor_dims ? selected->primary_tensor_dims : "");
        yvex_cli_out_writef(stdout, "metadata_status: %s\n",
               selected->primary_tensor_name && selected->primary_tensor_name[0] ? "recorded" : "missing");
        yvex_cli_out_writef(stdout, "execution_ready: %s\n", selected->execution_ready ? "true" : "false");
        yvex_cli_out_writef(stdout, "status: models-current\n");
    } else {
        yvex_cli_out_writef(stdout, "selected: none\n");
        yvex_cli_out_writef(stdout, "status: models-none\n");
        if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
            yvex_cli_out_writef(stdout, "hint: use 'yvex models use ALIAS' to select a model\n");
        }
    }
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_verify(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *registry_path = NULL;
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const yvex_model_registry_entry *entry;
    const char *alias;
    const char *identity_status = "unknown";
    const char *digest_status = "unknown";
    const char *metadata_status = "not-checked";
    const char *readiness_status = "not-checked";
    const char *status = "models-identity-fail";
    const char *reason = "";
    int pass = 0;
    int metadata_checked = 0;
    int rc;

    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models verify requires ALIAS\n");
        return 2;
    }
    alias = args[3];
    rc = parse_models_registry_output_options(arg_count, args, 4, &registry_path, &output_mode);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        yvex_cli_out_writef(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }

    memset(&identity, 0, sizeof(identity));
    rc = yvex_artifact_identity_read(entry->path, &identity, &err);
    if (rc != YVEX_OK) {
        identity_status = "fail";
        digest_status = "fail";
        reason = yvex_error_message(&err);
    } else if (!entry->sha256 || !entry->sha256[0] ||
               !yvex_sha256_hex_is_valid(entry->sha256)) {
        identity_status = "missing";
        digest_status = "missing";
        reason = "registered alias lacks digest identity; re-add model";
    } else if (strcmp(entry->sha256, identity.sha256) != 0 ||
               (entry->file_size != 0ull && entry->file_size != identity.file_size)) {
        identity_status = "fail";
        digest_status = "fail";
        reason = "digest mismatch for registered alias";
    } else {
        identity_status = "pass";
        digest_status = "pass";
        reason = "current file identity matches registered alias";
        pass = 1;
        memset(&current_metadata, 0, sizeof(current_metadata));
        memset(&metadata_report, 0, sizeof(metadata_report));
        rc = populate_registry_metadata(&current_metadata, entry->path, &err);
        if (rc != YVEX_OK) {
            metadata_status = "fail";
            readiness_status = "fail";
            reason = "current artifact metadata could not be parsed";
            pass = 0;
            status = "models-metadata-drift";
        } else {
            rc = yvex_model_registry_compare_metadata(entry,
                                                      &current_metadata.entry,
                                                      &metadata_report,
                                                      &err);
            metadata_checked = 1;
            if (rc != YVEX_OK) {
                metadata_status = "fail";
                readiness_status = "fail";
                reason = yvex_error_message(&err);
                pass = 0;
                status = "models-metadata-drift";
            } else {
                metadata_status = metadata_report.metadata_status;
                readiness_status = metadata_report.readiness_status;
                if (strcmp(metadata_status, "pass") == 0 &&
                    strcmp(readiness_status, "pass") == 0) {
                    status = "models-identity-pass";
                } else if (strcmp(metadata_status, "missing") == 0 ||
                           strcmp(readiness_status, "missing") == 0) {
                    reason = "registered alias lacks metadata summary; re-add model";
                    pass = 0;
                    status = "models-metadata-missing";
                } else {
                    reason = "registered alias metadata does not match current artifact facts";
                    pass = 0;
                    status = "models-metadata-drift";
                }
            }
        }
    }
    if (strcmp(identity_status, "missing") == 0) {
        status = "models-identity-missing";
    } else if (strcmp(identity_status, "fail") == 0) {
        status = "models-identity-fail";
    }

    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        yvex_cli_out_writef(stdout, "verify: %s alias=%s\n", pass ? "pass" : "fail", entry->alias);
        yvex_cli_out_writef(stdout, "identity: %s digest: %s metadata: %s\n",
               identity_status, digest_status, metadata_status);
        yvex_cli_out_writef(stdout, "artifact: %s tensors=%llu size=%llu\n",
               entry->format ? entry->format : "unknown",
               metadata_checked ? current_metadata.entry.tensor_count : entry->tensor_count,
               identity.file_size);
        yvex_cli_out_writef(stdout, "top_blocker: %s\n", pass ? "none" : reason);
        yvex_cli_out_writef(stdout, "boundary: identity verified, runtime generation unsupported\n");
        yvex_cli_out_writef(stdout, "status: %s\n", status);
    } else {
        yvex_cli_out_writef(stdout, "models: verify\n");
        yvex_cli_out_writef(stdout, "alias: %s\n", entry->alias);
        yvex_cli_out_writef(stdout, "path: %s\n", entry->path);
        yvex_cli_out_writef(stdout, "registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
        yvex_cli_out_writef(stdout, "current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
        yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", entry->file_size);
        yvex_cli_out_writef(stdout, "current_file_size: %llu\n", identity.file_size);
        yvex_cli_out_writef(stdout, "digest_status: %s\n", digest_status);
        yvex_cli_out_writef(stdout, "identity_status: %s\n", identity_status);
        yvex_cli_out_writef(stdout, "registered_support_level: %s\n", entry->support_level ? entry->support_level : "");
        yvex_cli_out_writef(stdout, "current_support_level: %s\n",
               metadata_checked ? current_metadata.entry.support_level : "not-checked");
        yvex_cli_out_writef(stdout, "registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
        yvex_cli_out_writef(stdout, "current_architecture: %s\n",
               metadata_checked ? current_metadata.entry.architecture : "not-checked");
        yvex_cli_out_writef(stdout, "registered_tensor_count: %llu\n", entry->tensor_count);
        yvex_cli_out_writef(stdout, "current_tensor_count: %llu\n",
               metadata_checked ? current_metadata.entry.tensor_count : 0ull);
        yvex_cli_out_writef(stdout, "registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
        yvex_cli_out_writef(stdout, "current_known_tensor_bytes: %llu\n",
               metadata_checked ? current_metadata.entry.known_tensor_bytes : 0ull);
        yvex_cli_out_writef(stdout, "registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
        yvex_cli_out_writef(stdout, "current_primary_tensor: %s\n",
               metadata_checked ? current_metadata.entry.primary_tensor_name : "not-checked");
        yvex_cli_out_writef(stdout, "registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
        yvex_cli_out_writef(stdout, "current_primary_role: %s\n",
               metadata_checked ? current_metadata.entry.primary_tensor_role : "not-checked");
        yvex_cli_out_writef(stdout, "registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
        yvex_cli_out_writef(stdout, "current_primary_dtype: %s\n",
               metadata_checked ? current_metadata.entry.primary_tensor_dtype : "not-checked");
        yvex_cli_out_writef(stdout, "registered_primary_rank: %u\n", entry->primary_tensor_rank);
        yvex_cli_out_writef(stdout, "current_primary_rank: %u\n",
               metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
        yvex_cli_out_writef(stdout, "registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
        yvex_cli_out_writef(stdout, "current_primary_dims: %s\n",
               metadata_checked ? current_metadata.entry.primary_tensor_dims : "not-checked");
        yvex_cli_out_writef(stdout, "registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
        yvex_cli_out_writef(stdout, "current_primary_bytes: %llu\n",
               metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
        yvex_cli_out_writef(stdout, "registered_selected_embedding_ready: %s\n",
               entry->selected_embedding_ready ? "true" : "false");
        yvex_cli_out_writef(stdout, "current_selected_embedding_ready: %s\n",
               metadata_checked && current_metadata.entry.selected_embedding_ready ? "true" : "false");
        yvex_cli_out_writef(stdout, "registered_selected_embedding_hidden_size: %llu\n",
               entry->selected_embedding_hidden_size);
        yvex_cli_out_writef(stdout, "current_selected_embedding_hidden_size: %llu\n",
               metadata_checked ? current_metadata.entry.selected_embedding_hidden_size : 0ull);
        yvex_cli_out_writef(stdout, "registered_selected_embedding_vocab_size: %llu\n",
               entry->selected_embedding_vocab_size);
        yvex_cli_out_writef(stdout, "current_selected_embedding_vocab_size: %llu\n",
               metadata_checked ? current_metadata.entry.selected_embedding_vocab_size : 0ull);
        yvex_cli_out_writef(stdout, "registered_selected_embedding_output_count: %llu\n",
               entry->selected_embedding_output_count);
        yvex_cli_out_writef(stdout, "current_selected_embedding_output_count: %llu\n",
               metadata_checked ? current_metadata.entry.selected_embedding_output_count : 0ull);
        yvex_cli_out_writef(stdout, "registered_selected_embedding_slice_bytes: %llu\n",
               entry->selected_embedding_slice_bytes);
        yvex_cli_out_writef(stdout, "current_selected_embedding_slice_bytes: %llu\n",
               metadata_checked ? current_metadata.entry.selected_embedding_slice_bytes : 0ull);
        if (metadata_checked) {
            print_metadata_drift_cli(&metadata_report);
        } else {
            yvex_cli_out_writef(stdout, "metadata_status: %s\n", metadata_status);
            yvex_cli_out_writef(stdout, "readiness_status: %s\n", readiness_status);
        }
        yvex_cli_out_writef(stdout, "reason: %s\n", reason);
        yvex_cli_out_writef(stdout, "status: %s\n", status);
    }
    yvex_model_registry_close(registry);
    return pass ? 0 : exit_for_status(YVEX_ERR_STATE);
}

static int command_models_remove(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models remove requires ALIAS\n");
        return 2;
    }
    alias = args[3];
    rc = parse_models_registry_option(arg_count, args, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_remove(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    yvex_cli_out_writef(stdout, "models: remove\n");
    yvex_cli_out_writef(stdout, "removed: %s\n", alias);
    yvex_cli_out_writef(stdout, "status: models-removed\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_inspect(int arg_count, char **args)
{
    yvex_model_registry *registry = NULL;
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const yvex_model_registry_entry *entry;
    const yvex_gguf_header *header;
    const char *registry_path = NULL;
    yvex_models_output_mode output_mode = YVEX_MODELS_OUTPUT_NORMAL;
    const char *alias;
    int rc;

    if (arg_count < 4) {
        yvex_cli_out_writef(stderr, "yvex: models inspect requires ALIAS\n");
        return 2;
    }
    alias = args[3];
    rc = parse_models_registry_output_options(arg_count, args, 4, &registry_path, &output_mode);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        yvex_cli_out_writef(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }
    if (output_mode != YVEX_MODELS_OUTPUT_AUDIT) {
        int selected_slice =
            strcmp(entry->alias, "deepseek4-v4-flash-selected-embed") == 0 ||
            strcmp(entry->alias, "deepseek4-v4-flash-selected-embed-rmsnorm") == 0;
        const char *display_family = selected_slice ? "deepseek" : (entry->family ? entry->family : "");
        const char *display_class = selected_slice ? "selected-slice" : (entry->artifact_class ? entry->artifact_class : "");
        yvex_cli_out_writef(stdout, "model: %s\n", entry->alias);
        yvex_cli_out_writef(stdout, "family: %s class=%s tensors=%llu size=%llu\n",
               display_family,
               display_class,
               entry->tensor_count,
               entry->file_size);
        yvex_cli_out_writef(stdout, "primary: %s %s %s\n",
               entry->primary_tensor_name ? entry->primary_tensor_name : "",
               entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "",
               entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
        yvex_cli_out_writef(stdout, "state: %s execution_ready=%s\n",
               entry->support_level ? entry->support_level : "",
               entry->execution_ready ? "true" : "false");
        yvex_cli_out_writef(stdout, "boundary: selected-slice only, full-runtime generation unsupported\n");
        yvex_cli_out_writef(stdout, "status: models-inspect\n");
        yvex_model_registry_close(registry);
        return 0;
    }
    yvex_cli_out_writef(stdout, "models: inspect\n");
    yvex_cli_out_writef(stdout, "alias: %s\n", entry->alias);
    yvex_cli_out_writef(stdout, "path: %s\n", entry->path);
    yvex_cli_out_writef(stdout, "family: %s\n", entry->family);
    yvex_cli_out_writef(stdout, "model: %s\n", entry->model);
    yvex_cli_out_writef(stdout, "scope: %s\n", entry->scope);
    yvex_cli_out_writef(stdout, "artifact_class: %s\n", entry->artifact_class);
    yvex_cli_out_writef(stdout, "qprofile: %s\n", entry->qprofile);
    yvex_cli_out_writef(stdout, "calibration: %s\n", entry->calibration);
    yvex_cli_out_writef(stdout, "support_level: %s\n", entry->support_level);
    yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", entry->file_size);
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    yvex_cli_out_writef(stdout, "registered_format: %s\n", entry->format ? entry->format : "");
    yvex_cli_out_writef(stdout, "registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    yvex_cli_out_writef(stdout, "registered_tensor_count: %llu\n", entry->tensor_count);
    yvex_cli_out_writef(stdout, "registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    yvex_cli_out_writef(stdout, "primary_tensor_name: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    yvex_cli_out_writef(stdout, "primary_tensor_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    yvex_cli_out_writef(stdout, "primary_tensor_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    yvex_cli_out_writef(stdout, "primary_tensor_rank: %u\n", entry->primary_tensor_rank);
    yvex_cli_out_writef(stdout, "primary_tensor_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    yvex_cli_out_writef(stdout, "primary_tensor_bytes: %llu\n", entry->primary_tensor_bytes);
    yvex_cli_out_writef(stdout, "selected_embedding_ready: %s\n", entry->selected_embedding_ready ? "true" : "false");
    yvex_cli_out_writef(stdout, "selected_embedding_hidden_size: %llu\n", entry->selected_embedding_hidden_size);
    yvex_cli_out_writef(stdout, "selected_embedding_vocab_size: %llu\n", entry->selected_embedding_vocab_size);
    yvex_cli_out_writef(stdout, "selected_embedding_output_count: %llu\n", entry->selected_embedding_output_count);
    yvex_cli_out_writef(stdout, "selected_embedding_slice_bytes: %llu\n", entry->selected_embedding_slice_bytes);
    yvex_cli_out_writef(stdout, "execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    rc = open_model_context(entry->path, &ctx, &err);
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(ctx.gguf);
        yvex_cli_out_writef(stdout, "gguf:\n");
        yvex_cli_out_writef(stdout, "  version: %u\n", header->version);
        yvex_cli_out_writef(stdout, "  tensor_count: %llu\n", header->tensor_count);
        close_model_context(&ctx);
    } else {
        yvex_cli_out_writef(stdout, "gguf:\n");
        yvex_cli_out_writef(stdout, "  status: unavailable\n");
        yvex_cli_out_writef(stdout, "  reason: %s\n", yvex_error_message(&err));
        yvex_error_clear(&err);
    }
    yvex_cli_out_writef(stdout, "status: models-inspect\n");
    yvex_model_registry_close(registry);
    return 0;
}

/* Full model inventory and placement reporting. */

typedef int (*yvex_models_subcommand_fn)(int arg_count, char **args);

typedef struct {
    const char *name;
    yvex_models_subcommand_fn run;
} yvex_models_subcommand;

static const yvex_models_subcommand model_subcommands[] = {
    { "scan", command_models_scan },
    { "add", command_models_add },
    { "download", yvex_models_download_surface_command },
    { "artifacts", yvex_models_artifacts_surface_command },
    { "prepare", yvex_models_prepare_surface_command },
    { "check", yvex_models_check_surface_command },
    { "list", command_models_list },
    { "use", command_models_use },
    { "current", command_models_current },
    { "verify", command_models_verify },
    { "inspect", command_models_inspect },
    { "remove", command_models_remove }
};

static int command_models(int arg_count, char **args)
{
    unsigned long i;

    if (arg_count >= 3 && (strcmp(args[2], "--help") == 0 || strcmp(args[2], "-h") == 0)) {
        yvex_model_artifacts_surface_models_help(stdout);
        return 0;
    }
    if (arg_count < 3) {
        yvex_cli_out_writef(stderr, "yvex: models requires scan, add, download, artifacts, prepare, check, list, use, current, verify, inspect, or remove\n");
        return 2;
    }
    for (i = 0; i < sizeof(model_subcommands) / sizeof(model_subcommands[0]); ++i) {
        if (strcmp(args[2], model_subcommands[i].name) == 0) {
            return model_subcommands[i].run(arg_count, args);
        }
    }
    yvex_cli_out_writef(stderr, "yvex: unknown models subcommand: %s\n", args[2]);
    return 2;
}

int yvex_model_artifacts_surface_models_command(int arg_count, char **args)
{
    return command_models(arg_count, args);
}

void yvex_model_artifacts_surface_models_help(FILE *fp)
{
    yvex_cli_out_writef(fp, "usage: " "yvex models scan --root DIR [--registry FILE]\n");
    yvex_cli_out_writef(fp, "       yvex models add --path FILE [--alias ALIAS] [--support-level LEVEL] [--registry FILE]\n");
    yvex_cli_out_writef(fp, "       yvex models download TARGET [--models-root DIR] [--auth auto|required|never] [--dry-run] [--progress auto|live|plain|log|off] [--tick-seconds N] [--no-progress] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models download status TARGET [--models-root DIR] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models download stop TARGET [--models-root DIR] [--force] [--timeout-seconds N] [--match-provider-process] [--dry-run] [--" "audit]\n");
    yvex_cli_out_writef(fp, "       yvex models download resume TARGET [--models-root DIR] [--auth auto|required|never] [--progress auto|live|plain|log|off] [--tick-seconds N] [--clear-stale-locks] [--" "audit]\n");
    yvex_cli_out_writef(fp, "       yvex models download cleanup TARGET [--models-root DIR] [--stale-locks] [--logs] [--receipts] [--failed-partials] [--all-provider-cache] [--dry-run] [--yes] [--" "audit]\n");
    yvex_cli_out_writef(fp, "       yvex models download --repo OWNER/NAME --family deepseek|glm|qwen|gemma [--name LOCAL_NAME] [--models-root DIR] [--auth auto|required|never] [--progress auto|live|plain|log|off]\n");
    yvex_cli_out_writef(fp, "       yvex models download --provider github --repo OWNER/NAME [--release TAG] --asset GLOB [--models-root DIR] [--auth auto|required|never] [--progress auto|live|plain|log|off]\n");
    yvex_cli_out_writef(fp, "       yvex models artifacts list [--models-root DIR] [--family deepseek|glm|qwen|gemma] [--" "output normal|table|audit|json]\n");
    yvex_cli_out_writef(fp, "       yvex models artifacts status TARGET [--models-root DIR] [--" "audit | --" "output normal|table|audit|json]\n");
    yvex_cli_out_writef(fp, "       yvex models prepare TARGET [--overwrite] [--source DIR] [--out FILE | --out-dir DIR] [--models-root DIR] [--registry FILE] [--dry-run] [--no-register] [--no-use] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models check TARGET [--backend cpu|cuda] [--level quick|runtime|full] [--models-root DIR] [--registry FILE] [--report-dir DIR] [--no-materialize] [--no-graph] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models list|current [--registry FILE] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models verify|inspect ALIAS [--registry FILE] [--" "audit | --" "output normal|table|audit]\n");
    yvex_cli_out_writef(fp, "       yvex models use|remove ALIAS [--registry FILE]\n");
    yvex_cli_out_writef(fp, "\nExamples:\n");
    yvex_cli_out_writef(fp, "  yvex models check deepseek4-v4-flash-selected-embed\n");
    yvex_cli_out_writef(fp, "  yvex models download gemma-4-12b-it --models-root ~/lab/models --dry-run --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download status gemma-4-12b-it --models-root ~/lab/models --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download stop gemma-4-12b-it --models-root ~/lab/models --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download resume gemma-4-12b-it --models-root ~/lab/models --auth required --progress live --tick-seconds 2 --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download cleanup gemma-4-12b-it --models-root ~/lab/models --stale-locks --dry-run --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download gemma-4-12b-it --models-root ~/lab/models --auth required --progress live --tick-seconds 2 --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download qwen3-8b --models-root ~/lab/models --auth auto --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download status qwen3-32b --models-root ~/lab/models\n");
    yvex_cli_out_writef(fp, "  yvex models artifacts list --models-root ~/lab/models --" "output table\n");
    yvex_cli_out_writef(fp, "  yvex models artifacts status qwen3-6-35b-a3b --models-root ~/lab/models --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models download --provider github --repo OWNER/REPO --release TAG --asset \"*.gguf\" --models-root ~/lab/models --auth auto --" "audit\n");
    yvex_cli_out_writef(fp, "  yvex models check deepseek4-v4-flash-selected-embed --backend cpu --level runtime\n");
    yvex_cli_out_writef(fp, "  yvex models check deepseek4-v4-flash-selected-embed --backend cuda --level runtime --no-graph\n");
    yvex_cli_out_writef(fp, "  yvex models check deepseek4-v4-flash-selected-embed --level full --report-dir build/reports\n");
    yvex_cli_out_writef(fp, "\nModels manages the local alias registry, source tensor download sidecars, GGUF artifact discovery, selected artifact preparation, selected artifact checks, digest identity, and metadata drift facts for registered artifacts. Download uses the local accounts/provider preflight for Hugging Face and GitHub provider CLIs, writes source intake reports only, and does not register runtime artifacts. Artifacts list/status reads operator paths, GGUF filenames, and source sidecars only; it does not hash files, load tensor payloads, emit GGUF, materialize, execute runtime paths, generate, evaluate, or benchmark. Prepare currently supports deepseek4-v4-flash-selected-embed only and does not materialize, run graph execution, decode, logits, sampling, generation, evaluation, or benchmarks.\n");
    yvex_cli_out_writef(fp, "Default report output is compact. Use --" "audit for full diagnostic fields.\n");
    yvex_cli_out_writef(fp, "Check composes implemented artifact, identity, integrity, selected materialization, engine/session, plan, selected graph, and selected gates only; it does not create artifacts, run source conversion, run prefill, decode, produce logits, sample, generate, evaluate, or benchmark.\n");
}
