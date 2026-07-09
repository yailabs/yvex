/*
 * yvex_model_artifacts_surface_common.c - shared CLI surface helpers.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   small CLI-only helper implementations shared by model-artifacts command
 *   family surfaces.
 *
 * Does not own:
 *   command-family dispatch bodies, provider execution, domain algorithms,
 *   renderer contracts, libyvex sources, artifact emission, runtime
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   this file remains under 500 lines and contains no command-family owner.
 *
 * Boundary:
 *   shared CLI helpers do not imply runtime support or artifact emission.
 */
#include "yvex_model_artifacts_surface_common.h"

char *model_artifacts_cli_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

int path_exists(const char *path)
{
    return path && path[0] && access(path, F_OK) == 0;
}

int is_path_like_reference(const char *input)
{
    size_t len;

    if (!input || !input[0]) return 0;
    if (strchr(input, '/') || strchr(input, '\\')) return 1;
    len = strlen(input);
    return len >= 5u && strcmp(input + len - 5u, ".gguf") == 0;
}

int set_path_ref(yvex_model_ref *out, const char *input, yvex_error *err)
{
    memset(out, 0, sizeof(*out));
    out->input = model_artifacts_cli_strdup(input);
    out->path = model_artifacts_cli_strdup(input);
    out->alias = model_artifacts_cli_strdup("");
    out->family = model_artifacts_cli_strdup("");
    out->sha256 = model_artifacts_cli_strdup("");
    out->support_level = model_artifacts_cli_strdup("");
    out->format = model_artifacts_cli_strdup("");
    out->architecture = model_artifacts_cli_strdup("");
    out->primary_tensor_name = model_artifacts_cli_strdup("");
    out->primary_tensor_role = model_artifacts_cli_strdup("");
    out->primary_tensor_dtype = model_artifacts_cli_strdup("");
    out->primary_tensor_dims = model_artifacts_cli_strdup("");
    out->status = YVEX_MODEL_REF_STATUS_RESOLVED;
    out->kind = YVEX_MODEL_REF_PATH;
    out->execution_ready = 0;
    if (!out->input || !out->path || !out->alias || !out->family ||
        !out->sha256 || !out->support_level || !out->format ||
        !out->architecture || !out->primary_tensor_name ||
        !out->primary_tensor_role || !out->primary_tensor_dtype ||
        !out->primary_tensor_dims) {
        yvex_model_ref_clear(out);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_ref", "path reference allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

void write_escaped(FILE *fp, const char *s)
{
    if (!s) s = "";
    yvex_cli_out_char(fp, '"');
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            yvex_cli_out_char(fp, '\\');
            yvex_cli_out_char(fp, (int)ch);
        } else if (ch == '\n') {
            yvex_cli_out_fputs("\\n", fp);
        } else if (ch == '\r') {
            yvex_cli_out_fputs("\\r", fp);
        } else if (ch == '\t') {
            yvex_cli_out_fputs("\\t", fp);
        } else {
            yvex_cli_out_char(fp, (int)ch);
        }
    }
    yvex_cli_out_char(fp, '"');
}

void write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma)
{
    yvex_cli_out_fputs(indent, fp);
    yvex_cli_out_writef(fp, "\"%s\": ", key);
    write_escaped(fp, value);
    yvex_cli_out_writef(fp, "%s\n", comma ? "," : "");
}

void write_u64_field(FILE *fp,
                     const char *indent,
                     const char *key,
                     unsigned long long value,
                     int comma)
{
    yvex_cli_out_fputs(indent, fp);
    yvex_cli_out_writef(fp, "\"%s\": %llu%s\n", key, value, comma ? "," : "");
}

void write_bool_field(FILE *fp, const char *indent, const char *key, int value, int comma)
{
    yvex_cli_out_fputs(indent, fp);
    yvex_cli_out_writef(fp, "\"%s\": %s%s\n", key, value ? "true" : "false", comma ? "," : "");
}

int yvex_model_registry_mkdir_parent(const char *path, yvex_error *err)
{
    char buf[4096];
    char *slash;
    char *p;

    if (!path || strlen(path) >= sizeof(buf)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "invalid registry path");
        return YVEX_ERR_INVALID_ARG;
    }
    strcpy(buf, path);
    slash = strrchr(buf, '/');
    if (!slash) return YVEX_OK;
    *slash = '\0';
    if (!buf[0]) return YVEX_OK;
    for (p = buf + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(buf, 0775) != 0 && errno != EEXIST) {
                yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json",
                                "cannot create directory: %s", buf);
                return YVEX_ERR_IO;
            }
            *p = '/';
        }
    }
    if (mkdir(buf, 0775) != 0 && errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json",
                        "cannot create directory: %s", buf);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int parse_models_output_mode(const char *value, yvex_models_output_mode *mode)
{
    if (!value || !mode) return 0;
    if (strcmp(value, "normal") == 0) *mode = YVEX_MODELS_OUTPUT_NORMAL;
    else if (strcmp(value, "table") == 0) *mode = YVEX_MODELS_OUTPUT_TABLE;
    else if (strcmp(value, "audit") == 0) *mode = YVEX_MODELS_OUTPUT_AUDIT;
    else return 0;
    return 1;
}

void print_model_registry_entry_cli(const yvex_model_registry_entry *entry, int selected)
{
    if (!entry) return;
    yvex_cli_out_writef(stdout, "%c    %-46s %-10s %-22s %5llu %13llu  %s\n",
                        selected ? '*' : '-',
                        entry->alias ? entry->alias : "",
                        entry->family ? entry->family : "",
                        entry->artifact_class ? entry->artifact_class : "",
                        entry->tensor_count,
                        entry->known_tensor_bytes,
                        entry->selected_embedding_ready ? "yes" : "no");
}

void print_model_registry_entry_normal(const yvex_model_registry_entry *entry, int selected)
{
    if (!entry) return;
    yvex_cli_out_writef(stdout, "model: %s\n", entry->alias ? entry->alias : "");
    yvex_cli_out_writef(stdout, "family: %s\n", entry->family ? entry->family : "");
    yvex_cli_out_writef(stdout, "path: %s\n", entry->path ? entry->path : "");
    yvex_cli_out_writef(stdout, "selected: %s\n", selected ? "true" : "false");
}

void print_model_registry_entry_audit(const yvex_model_registry_entry *entry,
                                      int selected)
{
    if (!entry) return;
    yvex_cli_out_writef(stdout, "model: %s\n", entry->alias ? entry->alias : "");
    yvex_cli_out_writef(stdout, "path: %s\n", entry->path ? entry->path : "");
    yvex_cli_out_writef(stdout, "selected: %s\n", selected ? "true" : "false");
    yvex_cli_out_writef(stdout, "family: %s\n", entry->family ? entry->family : "");
    yvex_cli_out_writef(stdout, "model_name: %s\n", entry->model ? entry->model : "");
    yvex_cli_out_writef(stdout, "scope: %s\n", entry->scope ? entry->scope : "");
    yvex_cli_out_writef(stdout, "artifact_class: %s\n",
                        entry->artifact_class ? entry->artifact_class : "");
    yvex_cli_out_writef(stdout, "qprofile: %s\n", entry->qprofile ? entry->qprofile : "");
    yvex_cli_out_writef(stdout, "calibration: %s\n",
                        entry->calibration ? entry->calibration : "");
    yvex_cli_out_writef(stdout, "registered_file_size: %llu\n", entry->file_size);
    yvex_cli_out_writef(stdout, "registered_sha256: %s\n",
                        entry->sha256 && entry->sha256[0] ? entry->sha256 : "absent");
    yvex_cli_out_writef(stdout, "registered_format: %s\n",
                        entry->format ? entry->format : "");
    yvex_cli_out_writef(stdout, "registered_architecture: %s\n",
                        entry->architecture ? entry->architecture : "");
    yvex_cli_out_writef(stdout, "registered_tensor_count: %llu\n", entry->tensor_count);
    yvex_cli_out_writef(stdout, "registered_known_tensor_bytes: %llu\n",
                        entry->known_tensor_bytes);
    yvex_cli_out_writef(stdout, "registered_selected_embedding_ready: %s\n",
                        entry->selected_embedding_ready ? "true" : "false");
}

void print_model_registry_scan_entry_cli(const yvex_model_registry_entry *entry)
{
    if (!entry) return;
    yvex_cli_out_writef(stdout, "candidate: %s\n", entry->alias ? entry->alias : "");
    yvex_cli_out_writef(stdout, "path: %s\n", entry->path ? entry->path : "");
}

void dims_to_text(const unsigned long long *dims,
                  unsigned int rank,
                  char *out,
                  size_t out_cap)
{
    size_t used = 0u;
    unsigned int i;

    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    for (i = 0; i < rank && used < out_cap; ++i) {
        int n = snprintf(out + used,
                         out_cap - used,
                         "%s%llu",
                         i == 0u ? "" : "x",
                         dims ? dims[i] : 0ull);
        if (n < 0) return;
        if ((size_t)n >= out_cap - used) {
            out[out_cap - 1u] = '\0';
            return;
        }
        used += (size_t)n;
    }
}

int populate_registry_identity(yvex_model_registry_entry *entry,
                               char *sha256,
                               char *format,
                               char *architecture,
                               char *primary_name,
                               char *primary_role,
                               char *primary_dtype,
                               char *primary_dims,
                               yvex_error *err)
{
    yvex_artifact_file_identity identity;
    yvex_cli_metadata_snapshot snapshot;
    yvex_error_clear(err);
    memset(&identity, 0, sizeof(identity));
    memset(&snapshot, 0, sizeof(snapshot));
    if (yvex_artifact_identity_read(entry->path, &identity, err) != YVEX_OK) return yvex_error_code(err);
    if (populate_registry_metadata(&snapshot, entry->path, err) != YVEX_OK) return yvex_error_code(err);
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

void model_stage_print(const char *stage, const char *status)
{
    yvex_cli_out_writef(stdout, "stage: %s %s\n", stage ? stage : "", status ? status : "");
}

void model_print_runtime_generation(const char *runtime_execution)
{
    yvex_cli_out_writef(stdout, "runtime_execution: %s\n",
                        runtime_execution ? runtime_execution : "not-performed");
    yvex_cli_out_writef(stdout, "generation: unsupported\n");
}

int cli_arg_value_valid(const char *value)
{
    return value && value[0] && !strchr(value, '\n') && !strchr(value, '\r');
}

int parse_models_value_option(const char *command,
                              const char *flag,
                              int arg_count,
                              char **args,
                              int *index,
                              const char **value)
{
    if (*index + 1 >= arg_count) {
        yvex_cli_out_writef(stderr, "yvex: %s %s requires a value\n", command, flag);
        return 2;
    }
    *value = args[++(*index)];
    if (!cli_arg_value_valid(*value)) {
        yvex_cli_out_writef(stderr, "yvex: %s %s value is empty or invalid\n", command, flag);
        return 2;
    }
    return 0;
}

int model_backend_kind_from_name(const char *backend_name, yvex_backend_kind *kind)
{
    if (!kind) return 0;
    if (!backend_name || strcmp(backend_name, "cpu") == 0) {
        *kind = YVEX_BACKEND_KIND_CPU;
        return 1;
    }
    if (strcmp(backend_name, "cuda") == 0) {
        *kind = YVEX_BACKEND_KIND_CUDA;
        return 1;
    }
    return 0;
}

int expand_operator_path(const char *input,
                         char *out,
                         size_t out_cap,
                         yvex_error *err,
                         const char *where)
{
    const char *home;
    int n;

    if (!input || !out || out_cap == 0u) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path value is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!cli_arg_value_valid(input)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "path value is empty or contains a newline");
        return YVEX_ERR_INVALID_ARG;
    }
    if (input[0] == '~' && input[1] == '/') {
        home = getenv("HOME");
        if (!home || !home[0]) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "HOME is required to expand ~/ paths");
            return YVEX_ERR_INVALID_ARG;
        }
        n = snprintf(out, out_cap, "%s/%s", home, input + 2);
    } else {
        n = snprintf(out, out_cap, "%s", input);
    }
    if (n < 0 || (size_t)n >= out_cap) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "path is too long");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int path_join2(char *out,
               size_t out_cap,
               const char *dir,
               const char *file,
               yvex_error *err,
               const char *where)
{
    int n = snprintf(out, out_cap, "%s/%s", dir, file);
    if (n < 0 || (size_t)n >= out_cap) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "resolved path is too long");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int path_parent_dir(const char *path, char *out, size_t out_cap)
{
    const char *slash;
    size_t len;

    if (!path || !out || out_cap == 0u) return 0;
    slash = strrchr(path, '/');
    if (!slash) {
        snprintf(out, out_cap, ".");
        return 1;
    }
    len = (size_t)(slash - path);
    if (len == 0u) len = 1u;
    if (len >= out_cap) return 0;
    memcpy(out, path, len);
    out[len] = '\0';
    return 1;
}
