/*
 * yvex_model_commands.c - Model registry command adapters.
 *
 * This file owns argv/output adapters for local model alias commands. Registry
 * storage, identity, metadata drift, and artifact facts remain owned by
 * yvex_model_artifacts.c.
 */

#include "yvex_command_private.h"

int models_registry_open(yvex_model_registry **registry,
                                const char *registry_path,
                                int create_if_missing,
                                yvex_error *err)
{
    yvex_model_registry_options options;

    memset(&options, 0, sizeof(options));
    options.registry_path = registry_path;
    options.create_if_missing = create_if_missing;
    return yvex_model_registry_open(registry, &options, err);
}

static void print_model_registry_entry_cli(const yvex_model_registry_entry *entry,
                                           int selected)
{
    if (!entry) return;
    printf("%c %s\n", selected ? '*' : '-', entry->alias ? entry->alias : "");
    printf("  family: %s\n", entry->family ? entry->family : "");
    printf("  model: %s\n", entry->model ? entry->model : "");
    printf("  scope: %s\n", entry->scope ? entry->scope : "");
    printf("  artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("  qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("  calibration: %s\n", entry->calibration ? entry->calibration : "");
    printf("  producer: %s\n", entry->producer ? entry->producer : "");
    printf("  schema_version: %s\n", entry->schema_version ? entry->schema_version : "");
    printf("  support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("  execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    printf("  path: %s\n", entry->path ? entry->path : "");
    printf("  registered_file_size: %llu\n", entry->file_size);
    printf("  registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("  registered_format: %s\n", entry->format ? entry->format : "");
    printf("  registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("  registered_tensor_count: %llu\n", entry->tensor_count);
    printf("  registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("  registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("  registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("  registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("  registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("  registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("  registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("  registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
}

static void print_model_registry_scan_entry_cli(const yvex_model_registry_entry *entry)
{
    if (!entry) return;
    printf("candidate: %s\n", entry->alias ? entry->alias : "");
    printf("path: %s\n", entry->path ? entry->path : "");
    printf("family: %s\n", entry->family ? entry->family : "");
    printf("model: %s\n", entry->model ? entry->model : "");
    printf("scope: %s\n", entry->scope ? entry->scope : "");
    printf("artifact_class: %s\n", entry->artifact_class ? entry->artifact_class : "");
    printf("qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    printf("calibration: %s\n", entry->calibration ? entry->calibration : "");
}

static void dims_to_text(const unsigned long long *dims,
                         unsigned int rank,
                         char *out,
                         size_t out_cap)
{
    unsigned int i;
    size_t used = 0u;

    if (!out || out_cap == 0u) {
        return;
    }
    out[0] = '\0';
    if (used + 1u < out_cap) {
        out[used++] = '[';
        out[used] = '\0';
    }
    for (i = 0; i < rank && used < out_cap; ++i) {
        int n = snprintf(out + used,
                         used < out_cap ? out_cap - used : 0u,
                         "%s%llu",
                         i == 0 ? "" : ",",
                         dims[i]);
        if (n < 0 || (size_t)n >= (used < out_cap ? out_cap - used : 0u)) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
    if (used + 1u < out_cap) {
        out[used++] = ']';
        out[used] = '\0';
    }
}

static const char *current_support_from_metadata(const yvex_model_registry_entry *entry)
{
    if (entry && entry->primary_tensor_name && entry->primary_tensor_name[0]) {
        return "selected-tensor-materialized";
    }
    if (entry && entry->format && entry->format[0]) {
        return "descriptor-only";
    }
    return "";
}

void model_ref_registry_entry_view(const yvex_model_ref *ref,
                                          yvex_model_registry_entry *entry)
{
    memset(entry, 0, sizeof(*entry));
    if (!ref) return;
    entry->alias = ref->alias;
    entry->path = ref->path;
    entry->sha256 = ref->sha256;
    entry->file_size = ref->registered_file_size;
    entry->format = ref->format;
    entry->architecture = ref->architecture;
    entry->tensor_count = ref->tensor_count;
    entry->known_tensor_bytes = ref->known_tensor_bytes;
    entry->primary_tensor_name = ref->primary_tensor_name;
    entry->primary_tensor_role = ref->primary_tensor_role;
    entry->primary_tensor_dtype = ref->primary_tensor_dtype;
    entry->primary_tensor_rank = ref->primary_tensor_rank;
    entry->primary_tensor_dims = ref->primary_tensor_dims;
    entry->primary_tensor_bytes = ref->primary_tensor_bytes;
    entry->support_level = ref->support_level;
    entry->selected_embedding_ready = ref->selected_embedding_ready;
    entry->selected_embedding_hidden_size = ref->selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = ref->selected_embedding_vocab_size;
    entry->selected_embedding_output_count = ref->selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = ref->selected_embedding_slice_bytes;
    entry->execution_ready = ref->execution_ready;
}

void print_metadata_drift_cli(const yvex_model_metadata_drift_report *report)
{
    unsigned int i;

    if (!report) return;
    printf("metadata_status: %s\n", report->metadata_status[0] ? report->metadata_status : "unknown");
    printf("readiness_status: %s\n", report->readiness_status[0] ? report->readiness_status : "unknown");
    for (i = 0; i < report->issue_count; ++i) {
        printf("metadata_issue_%u_code: %s\n", i, report->issues[i].code);
        printf("metadata_issue_%u_registered: %s\n", i, report->issues[i].registered_value);
        printf("metadata_issue_%u_current: %s\n", i, report->issues[i].current_value);
    }
}

int populate_registry_metadata(yvex_cli_metadata_snapshot *snapshot,
                                      const char *path,
                                      yvex_error *err)
{
    yvex_cli_tokenizer_context ctx;
    const yvex_tensor_info *primary = NULL;
    const yvex_tensor_info *embedding = NULL;
    yvex_selected_embedding_shape selected_shape;
    unsigned long long known_bytes = 0ull;
    unsigned long long i;
    int rc;

    if (!snapshot || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_metadata",
                       "metadata snapshot and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    memset(&ctx, 0, sizeof(ctx));
    rc = open_model_context(path, &ctx, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(snapshot->format, sizeof(snapshot->format), "gguf");
    snprintf(snapshot->architecture, sizeof(snapshot->architecture), "%s",
             yvex_arch_name(yvex_model_arch(ctx.model)));

    for (i = 0; i < yvex_tensor_table_count(ctx.table); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(ctx.table, i);
        if (!tensor) {
            continue;
        }
        known_bytes += tensor->storage_bytes;
        if (!primary && strcmp(tensor->name, "token_embd.weight") == 0) {
            primary = tensor;
            embedding = tensor;
        }
    }
    if (!primary && yvex_tensor_table_count(ctx.table) > 0ull) {
        primary = yvex_tensor_table_at(ctx.table, 0);
    }

    if (primary) {
        snprintf(snapshot->primary_tensor_name, sizeof(snapshot->primary_tensor_name),
                 "%s", primary->name ? primary->name : "");
        snprintf(snapshot->primary_tensor_role, sizeof(snapshot->primary_tensor_role),
                 "%s", yvex_tensor_role_name(primary->role));
        snprintf(snapshot->primary_tensor_dtype, sizeof(snapshot->primary_tensor_dtype),
                 "%s", yvex_dtype_name(primary->dtype));
        dims_to_text(primary->dims, primary->rank, snapshot->primary_tensor_dims,
                     sizeof(snapshot->primary_tensor_dims));
        snapshot->entry.primary_tensor_rank = primary->rank;
        snapshot->entry.primary_tensor_bytes = primary->storage_bytes;
    }

    if (embedding) {
        yvex_error shape_err;
        yvex_error_clear(&shape_err);
        memset(&selected_shape, 0, sizeof(selected_shape));
        if (yvex_selected_embedding_shape_validate(embedding, 0u, &selected_shape,
                                                   &shape_err) == YVEX_OK) {
            snapshot->entry.selected_embedding_ready = 1;
            snapshot->entry.selected_embedding_hidden_size = selected_shape.hidden_size;
            snapshot->entry.selected_embedding_vocab_size = selected_shape.vocab_size;
            snapshot->entry.selected_embedding_output_count = selected_shape.output_count;
            snapshot->entry.selected_embedding_slice_bytes = selected_shape.slice_bytes;
        } else {
            yvex_error_clear(&shape_err);
        }
    }

    snapshot->entry.path = path;
    snapshot->entry.format = snapshot->format;
    snapshot->entry.architecture = snapshot->architecture;
    snapshot->entry.tensor_count = yvex_tensor_table_count(ctx.table);
    snapshot->entry.known_tensor_bytes = known_bytes;
    snapshot->entry.primary_tensor_name = snapshot->primary_tensor_name;
    snapshot->entry.primary_tensor_role = snapshot->primary_tensor_role;
    snapshot->entry.primary_tensor_dtype = snapshot->primary_tensor_dtype;
    snapshot->entry.primary_tensor_dims = snapshot->primary_tensor_dims;
    snprintf(snapshot->support_level, sizeof(snapshot->support_level), "%s",
             current_support_from_metadata(&snapshot->entry));
    snapshot->entry.support_level = snapshot->support_level;

    close_model_context(&ctx);
    return YVEX_OK;
}

static int populate_registry_identity(yvex_model_registry_entry *entry,
                                      char sha256[YVEX_SHA256_HEX_CAP],
                                      char format[16],
                                      char architecture[64],
                                      char primary_name[128],
                                      char primary_role[64],
                                      char primary_dtype[32],
                                      char primary_dims[128],
                                      yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot snapshot;
    int rc;

    if (!entry || !entry->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_add_identity",
                       "registry entry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    rc = yvex_artifact_identity_read(entry->path, &identity, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = populate_registry_metadata(&snapshot, entry->path, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    snprintf(sha256, YVEX_SHA256_HEX_CAP, "%s", identity.sha256);
    snprintf(format, 16u, "%s", snapshot.format);
    snprintf(architecture, 64u, "%s", snapshot.architecture);
    snprintf(primary_name, 128u, "%s", snapshot.primary_tensor_name);
    snprintf(primary_role, 64u, "%s", snapshot.primary_tensor_role);
    snprintf(primary_dtype, 32u, "%s", snapshot.primary_tensor_dtype);
    snprintf(primary_dims, 128u, "%s", snapshot.primary_tensor_dims);

    entry->sha256 = sha256;
    entry->file_size = identity.file_size;
    entry->format = format;
    entry->architecture = architecture;
    entry->tensor_count = snapshot.entry.tensor_count;
    entry->known_tensor_bytes = snapshot.entry.known_tensor_bytes;
    entry->primary_tensor_name = primary_name;
    entry->primary_tensor_role = primary_role;
    entry->primary_tensor_dtype = primary_dtype;
    entry->primary_tensor_rank = snapshot.entry.primary_tensor_rank;
    entry->primary_tensor_dims = primary_dims;
    entry->primary_tensor_bytes = snapshot.entry.primary_tensor_bytes;
    entry->selected_embedding_ready = snapshot.entry.selected_embedding_ready;
    entry->selected_embedding_hidden_size = snapshot.entry.selected_embedding_hidden_size;
    entry->selected_embedding_vocab_size = snapshot.entry.selected_embedding_vocab_size;
    entry->selected_embedding_output_count = snapshot.entry.selected_embedding_output_count;
    entry->selected_embedding_slice_bytes = snapshot.entry.selected_embedding_slice_bytes;

    return YVEX_OK;
}

static int parse_models_registry_option(int argc, char **argv, int start, const char **registry_path)
{
    int i;

    for (i = start; i < argc; ++i) {
        if (strcmp(argv[i], "--registry") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "yvex: models --registry requires a file\n");
                return 2;
            }
            *registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for future machine-readable output; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int parse_models_add_options(int argc, char **argv,
                                    yvex_cli_models_add_options *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 3; i < argc; ++i) {
        if (i + 1 >= argc) {
            fprintf(stderr, "yvex: models add option requires a value: %s\n", argv[i]);
            return 2;
        }
        if (strcmp(argv[i], "--registry") == 0) options->registry_path = argv[++i];
        else if (strcmp(argv[i], "--path") == 0) options->path = argv[++i];
        else if (strcmp(argv[i], "--alias") == 0) options->alias = argv[++i];
        else if (strcmp(argv[i], "--family") == 0) options->family = argv[++i];
        else if (strcmp(argv[i], "--model") == 0) options->model = argv[++i];
        else if (strcmp(argv[i], "--scope") == 0) options->scope = argv[++i];
        else if (strcmp(argv[i], "--class") == 0) options->artifact_class = argv[++i];
        else if (strcmp(argv[i], "--qprofile") == 0) options->qprofile = argv[++i];
        else if (strcmp(argv[i], "--calibration") == 0) options->calibration = argv[++i];
        else if (strcmp(argv[i], "--sha256") == 0) options->sha256 = argv[++i];
        else if (strcmp(argv[i], "--support-level") == 0) options->support_level = argv[++i];
        else {
            fprintf(stderr, "yvex: unknown models add option: %s\n", argv[i]);
            return 2;
        }
    }
    return 0;
}

static int command_models_scan(int argc, char **argv)
{
    yvex_model_registry_entry *entries = NULL;
    yvex_error err;
    const char *root = NULL;
    const char *registry_path = NULL;
    unsigned long long count = 0;
    unsigned long long i;
    int rc;

    yvex_error_clear(&err);
    for (i = 3; (int)i < argc; ++i) {
        if (strcmp(argv[i], "--root") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --root requires a directory\n");
                return 2;
            }
            root = argv[++i];
        } else if (strcmp(argv[i], "--registry") == 0) {
            if ((int)i + 1 >= argc) {
                fprintf(stderr, "yvex: models scan --registry requires a file\n");
                return 2;
            }
            registry_path = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            /* Reserved for model selection work compatibility; text output remains canonical. */
        } else {
            fprintf(stderr, "yvex: unknown models scan option: %s\n", argv[i]);
            return 2;
        }
    }
    (void)registry_path;
    if (!root) {
        fprintf(stderr, "yvex: models scan requires --root DIR\n");
        return 2;
    }
    rc = yvex_model_registry_scan_root(root, &entries, &count, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    printf("models: scan\n");
    printf("root: %s\n", root);
    for (i = 0; i < count; ++i) {
        if (i > 0) printf("\n");
        print_model_registry_scan_entry_cli(&entries[i]);
    }
    printf("candidates: %llu\n", count);
    printf("status: models-scan\n");
    yvex_model_registry_scan_free(entries, count);
    return 0;
}

static int command_models_add(int argc, char **argv)
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
    rc = parse_models_add_options(argc, argv, &cli_options);
    if (rc != 0) return rc;
    if (!cli_options.path) {
        fprintf(stderr, "yvex: models add requires --path FILE\n");
        return 2;
    }
    if (yvex_model_registry_entry_derive_from_path(&derived, cli_options.path, &err) == YVEX_OK) {
        have_derived = 1;
    } else {
        yvex_error_clear(&err);
    }
    if (!cli_options.alias && !have_derived) {
        fprintf(stderr, "yvex: models add requires --alias when filename is not canonical\n");
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
    printf("models: add\n");
    printf("alias: %s\n", entry.alias);
    printf("path: %s\n", entry.path);
    printf("registered_file_size: %llu\n", entry.file_size);
    printf("registered_sha256: %s\n", entry.sha256);
    printf("registered_format: %s\n", entry.format);
    printf("registered_architecture: %s\n", entry.architecture);
    printf("registered_tensor_count: %llu\n", entry.tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry.known_tensor_bytes);
    printf("registered_primary_tensor: %s\n", entry.primary_tensor_name);
    printf("registered_primary_role: %s\n", entry.primary_tensor_role);
    printf("registered_primary_dtype: %s\n", entry.primary_tensor_dtype);
    printf("registered_primary_rank: %u\n", entry.primary_tensor_rank);
    printf("registered_primary_dims: %s\n", entry.primary_tensor_dims);
    printf("registered_primary_bytes: %llu\n", entry.primary_tensor_bytes);
    printf("registered_selected_embedding_ready: %s\n",
           entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry.selected_embedding_hidden_size);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry.selected_embedding_vocab_size);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry.selected_embedding_output_count);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry.selected_embedding_slice_bytes);
    printf("identity_status: recorded\n");
    printf("status: models-added\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_list(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    char selected_alias[256];
    unsigned long long i;
    unsigned long long count;
    int rc;

    yvex_error_clear(&err);
    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    selected_alias[0] = '\0';
    if (selected && selected->alias) {
        snprintf(selected_alias, sizeof(selected_alias), "%s", selected->alias);
    }
    count = yvex_model_registry_count(registry);
    printf("models: list\n");
    for (i = 0; i < count; ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int is_selected = selected_alias[0] && entry && strcmp(selected_alias, entry->alias) == 0;
        print_model_registry_entry_cli(entry, is_selected);
    }
    printf("count: %llu\n", count);
    printf("status: models-list\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_use(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models use requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_select(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: use\n");
    printf("selected: %s\n", alias);
    printf("status: models-selected\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_current(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const yvex_model_registry_entry *selected;
    int rc;

    rc = parse_models_registry_option(argc, argv, 3, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    selected = yvex_model_registry_selected(registry);
    printf("models: current\n");
    if (selected) {
        printf("selected: %s\n", selected->alias);
        printf("path: %s\n", selected->path);
        printf("registered_file_size: %llu\n", selected->file_size);
        printf("registered_sha256: %s\n", selected->sha256 && selected->sha256[0] ? selected->sha256 : "absent");
        printf("registered_format: %s\n", selected->format ? selected->format : "");
        printf("registered_architecture: %s\n", selected->architecture ? selected->architecture : "");
        printf("registered_tensor_count: %llu\n", selected->tensor_count);
        printf("registered_known_tensor_bytes: %llu\n", selected->known_tensor_bytes);
        printf("registered_primary_tensor: %s\n", selected->primary_tensor_name ? selected->primary_tensor_name : "");
        printf("registered_primary_role: %s\n", selected->primary_tensor_role ? selected->primary_tensor_role : "");
        printf("registered_primary_dtype: %s\n", selected->primary_tensor_dtype ? selected->primary_tensor_dtype : "");
        printf("registered_primary_rank: %u\n", selected->primary_tensor_rank);
        printf("registered_primary_dims: %s\n", selected->primary_tensor_dims ? selected->primary_tensor_dims : "");
        printf("metadata_status: %s\n",
               selected->primary_tensor_name && selected->primary_tensor_name[0] ? "recorded" : "missing");
        printf("execution_ready: %s\n", selected->execution_ready ? "true" : "false");
        printf("status: models-current\n");
    } else {
        printf("selected: none\n");
        printf("status: models-none\n");
    }
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_verify(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot current_metadata;
    yvex_model_metadata_drift_report metadata_report;
    yvex_error err;
    const char *registry_path = NULL;
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

    if (argc < 4) {
        fprintf(stderr, "yvex: models verify requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
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

    printf("models: verify\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("current_sha256: %s\n", identity.sha256[0] ? identity.sha256 : "unavailable");
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("current_file_size: %llu\n", identity.file_size);
    printf("digest_status: %s\n", digest_status);
    printf("identity_status: %s\n", identity_status);
    printf("registered_support_level: %s\n", entry->support_level ? entry->support_level : "");
    printf("current_support_level: %s\n",
           metadata_checked ? current_metadata.entry.support_level : "not-checked");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("current_architecture: %s\n",
           metadata_checked ? current_metadata.entry.architecture : "not-checked");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("current_tensor_count: %llu\n",
           metadata_checked ? current_metadata.entry.tensor_count : 0ull);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("current_known_tensor_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.known_tensor_bytes : 0ull);
    printf("registered_primary_tensor: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("current_primary_tensor: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_name : "not-checked");
    printf("registered_primary_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("current_primary_role: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_role : "not-checked");
    printf("registered_primary_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("current_primary_dtype: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dtype : "not-checked");
    printf("registered_primary_rank: %u\n", entry->primary_tensor_rank);
    printf("current_primary_rank: %u\n",
           metadata_checked ? current_metadata.entry.primary_tensor_rank : 0u);
    printf("registered_primary_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("current_primary_dims: %s\n",
           metadata_checked ? current_metadata.entry.primary_tensor_dims : "not-checked");
    printf("registered_primary_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("current_primary_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.primary_tensor_bytes : 0ull);
    printf("registered_selected_embedding_ready: %s\n",
           entry->selected_embedding_ready ? "true" : "false");
    printf("current_selected_embedding_ready: %s\n",
           metadata_checked && current_metadata.entry.selected_embedding_ready ? "true" : "false");
    printf("registered_selected_embedding_hidden_size: %llu\n",
           entry->selected_embedding_hidden_size);
    printf("current_selected_embedding_hidden_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_hidden_size : 0ull);
    printf("registered_selected_embedding_vocab_size: %llu\n",
           entry->selected_embedding_vocab_size);
    printf("current_selected_embedding_vocab_size: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_vocab_size : 0ull);
    printf("registered_selected_embedding_output_count: %llu\n",
           entry->selected_embedding_output_count);
    printf("current_selected_embedding_output_count: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_output_count : 0ull);
    printf("registered_selected_embedding_slice_bytes: %llu\n",
           entry->selected_embedding_slice_bytes);
    printf("current_selected_embedding_slice_bytes: %llu\n",
           metadata_checked ? current_metadata.entry.selected_embedding_slice_bytes : 0ull);
    if (metadata_checked) {
        print_metadata_drift_cli(&metadata_report);
    } else {
        printf("metadata_status: %s\n", metadata_status);
        printf("readiness_status: %s\n", readiness_status);
    }
    printf("reason: %s\n", reason);
    printf("status: %s\n", status);
    yvex_model_registry_close(registry);
    return pass ? 0 : exit_for_status(YVEX_ERR_STATE);
}

static int command_models_remove(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_error err;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models remove requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_remove(registry, alias, &err);
    if (rc == YVEX_OK) rc = yvex_model_registry_save(registry, registry_path, &err);
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return print_yvex_error(&err, exit_for_status(rc));
    }
    printf("models: remove\n");
    printf("removed: %s\n", alias);
    printf("status: models-removed\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models_inspect(int argc, char **argv)
{
    yvex_model_registry *registry = NULL;
    yvex_cli_tokenizer_context ctx;
    yvex_error err;
    const yvex_model_registry_entry *entry;
    const yvex_gguf_header *header;
    const char *registry_path = NULL;
    const char *alias;
    int rc;

    if (argc < 4) {
        fprintf(stderr, "yvex: models inspect requires ALIAS\n");
        return 2;
    }
    alias = argv[3];
    rc = parse_models_registry_option(argc, argv, 4, &registry_path);
    if (rc != 0) return rc;
    yvex_error_clear(&err);
    rc = models_registry_open(&registry, registry_path, 1, &err);
    if (rc != YVEX_OK) return print_yvex_error(&err, exit_for_status(rc));
    entry = yvex_model_registry_find(registry, alias);
    if (!entry) {
        yvex_model_registry_close(registry);
        fprintf(stderr, "yvex: model alias not found: %s\n", alias);
        return 2;
    }
    printf("models: inspect\n");
    printf("alias: %s\n", entry->alias);
    printf("path: %s\n", entry->path);
    printf("family: %s\n", entry->family);
    printf("model: %s\n", entry->model);
    printf("scope: %s\n", entry->scope);
    printf("artifact_class: %s\n", entry->artifact_class);
    printf("qprofile: %s\n", entry->qprofile);
    printf("calibration: %s\n", entry->calibration);
    printf("support_level: %s\n", entry->support_level);
    printf("registered_file_size: %llu\n", entry->file_size);
    printf("registered_sha256: %s\n", entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    printf("registered_format: %s\n", entry->format ? entry->format : "");
    printf("registered_architecture: %s\n", entry->architecture ? entry->architecture : "");
    printf("registered_tensor_count: %llu\n", entry->tensor_count);
    printf("registered_known_tensor_bytes: %llu\n", entry->known_tensor_bytes);
    printf("primary_tensor_name: %s\n", entry->primary_tensor_name ? entry->primary_tensor_name : "");
    printf("primary_tensor_role: %s\n", entry->primary_tensor_role ? entry->primary_tensor_role : "");
    printf("primary_tensor_dtype: %s\n", entry->primary_tensor_dtype ? entry->primary_tensor_dtype : "");
    printf("primary_tensor_rank: %u\n", entry->primary_tensor_rank);
    printf("primary_tensor_dims: %s\n", entry->primary_tensor_dims ? entry->primary_tensor_dims : "");
    printf("primary_tensor_bytes: %llu\n", entry->primary_tensor_bytes);
    printf("selected_embedding_ready: %s\n", entry->selected_embedding_ready ? "true" : "false");
    printf("selected_embedding_hidden_size: %llu\n", entry->selected_embedding_hidden_size);
    printf("selected_embedding_vocab_size: %llu\n", entry->selected_embedding_vocab_size);
    printf("selected_embedding_output_count: %llu\n", entry->selected_embedding_output_count);
    printf("selected_embedding_slice_bytes: %llu\n", entry->selected_embedding_slice_bytes);
    printf("execution_ready: %s\n", entry->execution_ready ? "true" : "false");
    rc = open_model_context(entry->path, &ctx, &err);
    if (rc == YVEX_OK) {
        header = yvex_gguf_header_view(ctx.gguf);
        printf("gguf:\n");
        printf("  version: %u\n", header->version);
        printf("  tensor_count: %llu\n", header->tensor_count);
        close_model_context(&ctx);
    } else {
        printf("gguf:\n");
        printf("  status: unavailable\n");
        printf("  reason: %s\n", yvex_error_message(&err));
        yvex_error_clear(&err);
    }
    printf("status: models-inspect\n");
    yvex_model_registry_close(registry);
    return 0;
}

static int command_models(int argc, char **argv)
{
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        yvex_cli_print_command_help(stdout, yvex_cli_find_command("models"));
        return 0;
    }
    if (argc < 3) {
        fprintf(stderr, "yvex: models requires scan, add, list, use, current, verify, inspect, or remove\n");
        return 2;
    }
    if (strcmp(argv[2], "scan") == 0) return command_models_scan(argc, argv);
    if (strcmp(argv[2], "add") == 0) return command_models_add(argc, argv);
    if (strcmp(argv[2], "list") == 0) return command_models_list(argc, argv);
    if (strcmp(argv[2], "use") == 0) return command_models_use(argc, argv);
    if (strcmp(argv[2], "current") == 0) return command_models_current(argc, argv);
    if (strcmp(argv[2], "verify") == 0) return command_models_verify(argc, argv);
    if (strcmp(argv[2], "inspect") == 0) return command_models_inspect(argc, argv);
    if (strcmp(argv[2], "remove") == 0) return command_models_remove(argc, argv);
    fprintf(stderr, "yvex: unknown models subcommand: %s\n", argv[2]);
    return 2;
}

int yvex_cli_command_models(int argc, char **argv)
{
    return command_models(argc, argv);
}
