/* Own the process-lifetime catalog of source-qualified model targets. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/source_catalog.h>
#include <yvex/internal/core.h>
#include <yvex/internal/io.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>

static int target_path_format(char *out,
                              size_t cap,
                              yvex_error *err,
                              const char *format,
                              ...)
{
    va_list args;
    int length;

    if (!out || cap == 0u || !format) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths",
                       "invalid path format argument");
        return YVEX_ERR_INVALID_ARG;
    }
    va_start(args, format);
    length = vsnprintf(out, cap, format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= cap) {
        out[cap - 1u] = '\0';
        yvex_error_setf(err, YVEX_ERR_BOUNDS, "operator_paths",
                        "path exceeds capacity %lu", (unsigned long)cap);
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

static int acquisition_path_join(char *out, size_t capacity, const char *directory,
    const char *leaf, yvex_error *err, const char *where)
{
    (void)where;
    return target_path_format(out, capacity, err, "%s/%s", directory, leaf);
}

void yvex_source_mapping_status(const char *tensor_map_path,
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
        yvex_core_file_read_text_prefix(tensor_map_path, buf, sizeof(buf))) {
        if (yvex_json_probe_string_field(buf,
                                         "required_role_coverage_status",
                                         coverage,
                                         sizeof(coverage))) {
            if (strcmp(coverage, "required-groups-present") != 0 &&
                tensor_map_incomplete) {
                *tensor_map_incomplete = 1;
            }
        } else {
            {
                const char *value = yvex_json_probe_field_value(buf, "unmapped_unknown_count");
                unmapped = value ? strtoll(value, NULL, 10) : -1;
            }
            if (unmapped > 0 && tensor_map_incomplete) {
                *tensor_map_incomplete = 1;
            }
        }
    }
    if (output_head_map_path && output_head_map_path[0] &&
        yvex_core_file_read_text_prefix(output_head_map_path, buf, sizeof(buf)) &&
        yvex_json_probe_string_field(buf, "output_head_status", status, sizeof(status)) &&
        strcmp(status, "present") != 0 &&
        output_head_map_missing) {
        *output_head_map_missing = 1;
    }
}

static int acquisition_family_valid(const char *family)
{
    const unsigned char *cursor = (const unsigned char *)family;
    if (!family || !family[0] || strlen(family) >= 32u || !islower(*cursor)) return 0;
    while (*cursor) {
        if (!(islower(*cursor) || isdigit(*cursor) || *cursor == '-' ||
              *cursor == '_' || *cursor == '.')) return 0;
        cursor++;
    }
    return strcmp(family, ".") && strcmp(family, "..");
}
const char *const *yvex_source_acquisition_default_patterns(int exclude, size_t *count)
{
    static const char *const includes[] = {"*.safetensors", "*.json", "*.txt", "*.model", "*.jinja", "*.md"};
    static const char *const excludes[] = {"*.bin", "*.pt",   "*.onnx", "*.msgpack", "*.tflite",
                                           "*.h5",  "*.ckpt", "*.tar",  "*.zip"};
    if (count) *count = exclude ? sizeof(excludes) / sizeof(excludes[0]) : sizeof(includes) / sizeof(includes[0]);
    return exclude ? excludes : includes;
}

int yvex_source_acquisition_provenance_paths(const char *target, const char *family,
                                  const yvex_operator_paths *operator_paths,
                                  yvex_source_acquisition_provenance *out, yvex_error *err) {
    char reports_family_dir[YVEX_PATH_CAP];
    char registry_family_dir[YVEX_PATH_CAP];
    char file_name[320];
    int rc;

    if (!target || !target[0] || strlen(target) >= 256u ||
        strchr(target, '/') || strchr(target, '\\') || !strcmp(target, ".") || !strcmp(target, "..") ||
        !acquisition_family_valid(family) || !operator_paths || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source.acquisition.provenance",
                       "bounded target, family, paths and output are required");
        return 0;
    }
    rc = acquisition_path_join(reports_family_dir, sizeof(reports_family_dir), operator_paths->reports_root,
                    family, err, "models_download_identity");
    if (rc == YVEX_OK) {
        rc = acquisition_path_join(registry_family_dir, sizeof(registry_family_dir),
                        operator_paths->registry_root, family, err, "models_download_identity");
    }
    if (rc != YVEX_OK)
        return 0;

    snprintf(file_name, sizeof(file_name), "%s.download.json", target);
    rc = acquisition_path_join(out->registry_path, sizeof(out->registry_path), registry_family_dir, file_name,
                    err, "models_download_identity");
    snprintf(file_name, sizeof(file_name), "%s.download-report.json", target);
    if (rc == YVEX_OK) {
        rc = acquisition_path_join(out->download_report_path, sizeof(out->download_report_path),
                        reports_family_dir, file_name, err, "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.source-manifest.json", target);
    if (rc == YVEX_OK) {
        rc = acquisition_path_join(out->manifest_path, sizeof(out->manifest_path), reports_family_dir,
                        file_name, err, "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.native-inventory.json", target);
    if (rc == YVEX_OK) {
        rc = acquisition_path_join(out->native_inventory_path, sizeof(out->native_inventory_path),
                        reports_family_dir, file_name, err, "models_download_identity");
    }
    snprintf(file_name, sizeof(file_name), "%s.acquisition.operation.json", target);
    if (rc == YVEX_OK) rc = acquisition_path_join(out->operation_path, sizeof(out->operation_path),
        reports_family_dir, file_name, err, "models_download_identity");
    snprintf(file_name, sizeof(file_name), "%s.acquisition.provider-event.json", target);
    if (rc == YVEX_OK) rc = acquisition_path_join(out->provider_event_path, sizeof(out->provider_event_path),
        reports_family_dir, file_name, err, "models_download_identity");
    if (rc == YVEX_OK) rc = target_path_format(out->supervisor_log_path, sizeof(out->supervisor_log_path),
        err, "%s/evidence/build/acquisition/%s.acquisition.supervisor.log", operator_paths->models_root, target);
    return rc == YVEX_OK;
}

static int download_record_patterns(const char *text, const char *key,
                                      char patterns[YVEX_SOURCE_ACQUISITION_PATTERN_CAP][1024],
                                      unsigned int *count)
{
    const char *value = yvex_json_probe_field_value(text, key);
    yvex_json json;
    yvex_json_iter array;
    yvex_json_item item;
    *count = 0u;
    if (!value) return 1; /* Older records used the default selection. */
    yvex_json_init(&json, value, strlen(value));
    if (!yvex_json_iter_begin(&json, &array, YVEX_JSON_COLLECTION_ARRAY)) return 0;
    while ((item = yvex_json_array_value(&array)) == YVEX_JSON_ITEM_READY) {
        if (*count >= YVEX_SOURCE_ACQUISITION_PATTERN_CAP ||
            !yvex_json_string(&json, patterns[*count], sizeof(patterns[0]))) return 0;
        (*count)++;
    }
    return item == YVEX_JSON_ITEM_END;
}

static int acquisition_provenance_parse(const char *buf, const char *target, const char *family,
                                           yvex_source_acquisition_provenance *out) {
    char parsed_target[128];
    char parsed_family[32];
    char parsed_repo[256];
    char parsed_provider[32];
    char parsed_revision[128];
    char parsed_source[YVEX_PATH_CAP];
    yvex_json json;

    yvex_json_init(&json, buf, strlen(buf));
    if (!yvex_json_skip_value(&json) || !yvex_json_complete(&json)) return 0;

    memset(parsed_target, 0, sizeof(parsed_target));
    memset(parsed_family, 0, sizeof(parsed_family));
    memset(parsed_repo, 0, sizeof(parsed_repo));
    memset(parsed_provider, 0, sizeof(parsed_provider));
    memset(parsed_revision, 0, sizeof(parsed_revision));
    memset(parsed_source, 0, sizeof(parsed_source));

    yvex_json_probe_string_field(buf, "target_id", parsed_target, sizeof(parsed_target));
    if (parsed_target[0] && strcmp(parsed_target, target) != 0)
        return 0;
    yvex_json_probe_string_field(buf, "family", parsed_family, sizeof(parsed_family));
    if (parsed_family[0] && strcmp(parsed_family, family) != 0)
        return 0;
    yvex_json_probe_string_field(buf, "repo_id", parsed_repo, sizeof(parsed_repo));
    if (!parsed_repo[0]) {
        yvex_json_probe_string_field(buf, "repo", parsed_repo, sizeof(parsed_repo));
    }
    if (!parsed_repo[0]) return 0;
    yvex_json_probe_string_field(buf, "provider", parsed_provider, sizeof(parsed_provider));
    yvex_json_probe_string_field(buf, "revision", parsed_revision, sizeof(parsed_revision));
    yvex_json_probe_string_field(buf, "local_source_dir", parsed_source, sizeof(parsed_source));
    if (!parsed_source[0]) {
        yvex_json_probe_string_field(buf, "path", parsed_source, sizeof(parsed_source));
    }

    (void)yvex_json_probe_string_field(buf, "source_payload_digest",
                                       out->source_payload_digest, sizeof(out->source_payload_digest));
    if (!download_record_patterns(buf, "include_patterns", out->includes, &out->include_count) ||
        !download_record_patterns(buf, "exclude_patterns", out->excludes, &out->exclude_count)) return 0;
    snprintf(out->target_id, sizeof(out->target_id), "%s",
             parsed_target[0] ? parsed_target : target);
    snprintf(out->family, sizeof(out->family), "%s", parsed_family[0] ? parsed_family : family);
    snprintf(out->repo_id, sizeof(out->repo_id), "%s", parsed_repo[0] ? parsed_repo : "unknown");
    snprintf(out->provider, sizeof(out->provider), "%s",
             parsed_provider[0] ? parsed_provider : "huggingface");
    snprintf(out->revision, sizeof(out->revision), "%s",
             parsed_revision[0] ? parsed_revision : "main");
    snprintf(out->local_name, sizeof(out->local_name), "%s", target);
    if (parsed_source[0]) {
        snprintf(out->local_source_dir, sizeof(out->local_source_dir), "%s", parsed_source);
    }
    out->found = 1;
    return 1;
}

int yvex_source_acquisition_provenance_read(const char *path, const char *target, const char *family,
                                             yvex_source_acquisition_provenance *out)
{
    yvex_source_acquisition_provenance *next;
    yvex_error err;
    char *record;
    size_t bytes = 0u;
    int found;
    if (!path || !path[0] || !target || !family || !out) return 0;
    yvex_error_clear(&err);
    record = yvex_read_bounded_file(path, 256u * 1024u, &bytes, &err);
    if (!record || !bytes) { free(record); return 0; }
    next = malloc(sizeof(*next));
    if (!next) { free(record); return 0; }
    *next = *out; /* Preserve caller-formed paths, publish facts only after full validation. */
    next->include_count = next->exclude_count = 0u;
    found = acquisition_provenance_parse(record, target, family, next);
    if (found) *out = *next;
    free(next);
    free(record);
    return found;
}

static int acquisition_selected_identity(const char *target, const char *family,
                                             const yvex_operator_paths *paths,
                                             yvex_source_acquisition_provenance *out,
                                             yvex_error *err)
{
    char directory[YVEX_PATH_CAP];
    DIR *stream;
    struct dirent *entry;
    int found = 0;
    if (acquisition_path_join(directory, sizeof(directory), paths->registry_root, family, err,
                   "source.acquisition.resolve") != YVEX_OK) return 0;
    stream = opendir(directory);
    if (!stream) return 0;
    while ((entry = readdir(stream)) != NULL) {
        const char *suffix = ".download.json";
        size_t length = strlen(entry->d_name), ending = strlen(suffix);
        char stem[256];
        yvex_source_acquisition_provenance candidate = {0};
        if (length <= ending || strcmp(entry->d_name + length - ending, suffix) ||
            length - ending >= sizeof(stem)) continue;
        memcpy(stem, entry->d_name, length - ending);
        stem[length - ending] = '\0';
        if (!yvex_source_acquisition_provenance_paths(stem, family, paths, &candidate, err) ||
            !yvex_source_acquisition_provenance_read(candidate.registry_path, target, family, &candidate)) continue;
        if (found && strcmp(out->local_source_dir, candidate.local_source_dir)) {
            closedir(stream);
            memset(out, 0, sizeof(*out));
            yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.resolve",
                           "multiple acquisitions match; inspect exact source records with model show");
            return -1;
        }
        *out = candidate;
        found = 1;
    }
    closedir(stream);
    return found;
}

int yvex_source_acquisition_provenance_resolve(const char *target,
                                             const yvex_operator_paths *operator_paths,
                                             yvex_source_acquisition_provenance *out,
                                             yvex_error *err)
{
    DIR *directory;
    struct dirent *entry;
    int found = 0;
    if (!out || !target || !target[0] || !operator_paths) return 0;
    memset(out, 0, sizeof(*out));
    directory = opendir(operator_paths->registry_root);
    if (!directory) return 0;
    while ((entry = readdir(directory)) != NULL) {
        char path[YVEX_PATH_CAP];
        struct stat status;
        yvex_source_acquisition_provenance candidate = {0};
        int selected;
        if (entry->d_name[0] == '.' || !acquisition_family_valid(entry->d_name) ||
            !strcmp(entry->d_name, "runtime") || !strcmp(entry->d_name, "provenance") ||
            acquisition_path_join(path, sizeof(path), operator_paths->registry_root, entry->d_name, err,
                       "source.acquisition.resolve") != YVEX_OK ||
            lstat(path, &status) != 0 || !S_ISDIR(status.st_mode)) continue;
        selected = acquisition_selected_identity(target, entry->d_name, operator_paths, &candidate, err);
        if (selected < 0 || (selected && found && strcmp(out->local_source_dir, candidate.local_source_dir))) {
            found = -1;
            memset(out, 0, sizeof(*out));
            yvex_error_set(err, YVEX_ERR_STATE, "source.acquisition.resolve", "ambiguous acquisition target");
            break;
        }
        if (selected) { *out = candidate; out->found = 1; found = 1; }
    }
    closedir(directory);
    return found;
}



int yvex_operator_paths_resolve_target(const yvex_operator_paths *operator_paths,
                                       const char *family,
                                       const char *kind,
                                       char *out,
                                       size_t cap,
                                       int *out_exists,
                                       yvex_error *err)
{
    struct stat status;
    int rc;

    if (!operator_paths || !family || !kind || !out || !out_exists) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "operator_paths",
                       "family, kind and outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(family, "deepseek") != 0 && strcmp(family, "glm") != 0 &&
        strcmp(family, "qwen") != 0 && strcmp(family, "gemma") != 0 &&
        strcmp(family, "minimax-h3") != 0 && strcmp(family, "mamba2") != 0) {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator_paths",
                        "unknown family: %s", family);
        return YVEX_ERR_INVALID_ARG;
    }
    if (strcmp(kind, "source") == 0) {
        const char *target = !strcmp(family, "deepseek") ? YVEX_SOURCE_RELEASE_TARGET_ID :
                             !strcmp(family, "qwen") ? YVEX_SOURCE_QWEN3_8_27B_TARGET_ID :
                             !strcmp(family, "minimax-h3") ? YVEX_SOURCE_MINIMAX_H3_TARGET_ID :
                             !strcmp(family, "mamba2") ? "mamba-codestral-7b-v0.1" : NULL;
        if (!target) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "operator_paths",
                           "no qualified default source; select a model catalog record or local path");
            return YVEX_ERR_UNSUPPORTED;
        }
        if (!yvex_source_target_path(out, cap, operator_paths->models_root,
                                     yvex_source_target_identity_find(target))) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, "operator_paths",
                           "source path exceeds output capacity");
            return YVEX_ERR_BOUNDS;
        }
        rc = YVEX_OK;
    } else if (strcmp(kind, "gguf") == 0) {
        rc = target_path_format(out, cap, err, "%s/%s",
                                operator_paths->gguf_root, family);
    } else if (strcmp(kind, "reports") == 0) {
        rc = target_path_format(out, cap, err, "%s/%s",
                                operator_paths->reports_root, family);
    } else if (strcmp(kind, "reference") == 0) {
        rc = target_path_format(out, cap, err, "%s/%s",
                                operator_paths->reference_root, family);
    } else if (strcmp(kind, "registry") == 0) {
        rc = target_path_format(out, cap, err, "%s", operator_paths->registry_root);
    } else {
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "operator_paths",
                        "unknown kind: %s", kind);
        return YVEX_ERR_INVALID_ARG;
    }
    if (rc != YVEX_OK) return rc;
    *out_exists = stat(out, &status) == 0 ? 1 : 0;
    return YVEX_OK;
}


/* The pinned source relationship joins a base checkpoint and its proposal
 * module at the logical layer only. Payloads, targets and deployments differ. */
static const yvex_source_logical_model release_logical_model = {
    .identity = "family:deepseek4/model:v4-flash",
    .family = "deepseek4", .model = "v4-flash",
    .display_name = "DeepSeek-V4-Flash",
    .family_aliases = {"deepseek4", "deepseek"},
    .model_aliases = {"v4-flash", "v4-flash-dspark"},
    .related_repository = "deepseek-ai/DeepSeek-V4-Flash",
    .related_revision = "60d8d70770c6776ff598c94bb586a859a38244f1"
};

static const yvex_source_target_identity source_target_identities[] = {
    {
        .target_id = YVEX_SOURCE_RELEASE_TARGET_ID,
        .family_key = YVEX_SOURCE_RELEASE_FAMILY_KEY,
        .family_display = YVEX_SOURCE_RELEASE_FAMILY_DISPLAY,
        .model_name = YVEX_SOURCE_RELEASE_NAME,
        .upstream_repo_id = YVEX_SOURCE_RELEASE_REPOSITORY,
        .source_dir_leaf = YVEX_SOURCE_RELEASE_SOURCE_LEAF,
        .upstream_revision = YVEX_SOURCE_RELEASE_REVISION,
        .upstream_index_path = YVEX_SOURCE_RELEASE_INDEX_PATH,
        .upstream_index_oid = YVEX_SOURCE_RELEASE_INDEX_OID,
        .upstream_index_size = YVEX_SOURCE_RELEASE_INDEX_SIZE,
        .upstream_inventory_authority = YVEX_SOURCE_RELEASE_INVENTORY_AUTHORITY,
        .config_model_type = YVEX_SOURCE_RELEASE_CONFIG_TYPE,
        .config_architecture = YVEX_SOURCE_RELEASE_CONFIG_ARCHITECTURE,
        .config_validation = YVEX_SOURCE_CONFIG_VALIDATION_DEEPSEEK_V4,
        .required_sidecars = YVEX_SOURCE_SIDECARS_DEEPSEEK_V4,
        .logical_model = &release_logical_model,
    },
    {
        .target_id = YVEX_SOURCE_MINIMAX_H3_TARGET_ID,
        .family_key = YVEX_SOURCE_MINIMAX_H3_FAMILY_KEY,
        .family_display = YVEX_SOURCE_MINIMAX_H3_FAMILY_DISPLAY,
        .model_name = YVEX_SOURCE_MINIMAX_H3_NAME,
        .upstream_repo_id = YVEX_SOURCE_MINIMAX_H3_REPOSITORY,
        .source_dir_leaf = YVEX_SOURCE_MINIMAX_H3_SOURCE_LEAF,
        .upstream_revision = YVEX_SOURCE_MINIMAX_H3_REVISION,
        .upstream_inventory_authority = "component-source-manifest",
        .config_validation = YVEX_SOURCE_CONFIG_VALIDATION_FAMILY_SEMANTIC,
    },
    {
        .target_id = YVEX_SOURCE_QWEN3_8_27B_TARGET_ID,
        .family_key = YVEX_SOURCE_QWEN3_8_27B_FAMILY_KEY,
        .family_display = YVEX_SOURCE_QWEN3_8_27B_FAMILY_DISPLAY,
        .model_name = YVEX_SOURCE_QWEN3_8_27B_NAME,
        .upstream_repo_id = YVEX_SOURCE_QWEN3_8_27B_REPOSITORY,
        .source_dir_leaf = YVEX_SOURCE_QWEN3_8_27B_SOURCE_LEAF,
        .upstream_revision = YVEX_SOURCE_QWEN3_8_27B_REVISION,
        .upstream_index_path = YVEX_SOURCE_QWEN3_8_27B_INDEX_PATH,
        .upstream_index_oid = YVEX_SOURCE_QWEN3_8_27B_INDEX_OID,
        .upstream_index_size = YVEX_SOURCE_QWEN3_8_27B_INDEX_SIZE,
        .upstream_inventory_authority = "upstream-index",
        .config_model_type = YVEX_SOURCE_QWEN3_8_27B_CONFIG_TYPE,
        .config_architecture = YVEX_SOURCE_QWEN3_8_27B_CONFIG_ARCHITECTURE,
        .config_validation = YVEX_SOURCE_CONFIG_VALIDATION_FAMILY_SEMANTIC,
        .required_sidecars = YVEX_SOURCE_SIDECARS_TEXT,
    },
    {
        .target_id = "mamba-codestral-7b-v0.1",
        .family_key = "mamba2",
        .family_display = "Mamba2",
        .model_name = "Mamba-Codestral-7B-v0.1",
        .upstream_repo_id = "mistralai/Mamba-Codestral-7B-v0.1",
        .source_dir_leaf = "mamba-codestral-7b-v0.1",
        .upstream_revision = "4f086c08c1e0f07bdc50ca25125dbbf7475d21da",
        .upstream_index_path = "model.safetensors.index.json",
        .upstream_index_oid = "102c8ea69509aa0d5cba284b16517f8d64c6df14",
        .upstream_index_size = 45172u,
        .upstream_inventory_authority = "upstream-index",
        .config_model_type = "mamba2",
        .config_architecture = "Mamba2ForCausalLM",
        .config_validation = YVEX_SOURCE_CONFIG_VALIDATION_FAMILY_SEMANTIC,
        .required_sidecars = YVEX_SOURCE_SIDECARS_TEXT,
    },
    {
        .target_id = YVEX_SOURCE_QWEN3_5_08B_TARGET_ID,
        .family_key = "qwen",
        .family_display = "Qwen3.5",
        .model_name = "Qwen3.5-0.8B",
        .upstream_repo_id = YVEX_SOURCE_QWEN3_5_08B_REPOSITORY,
        .source_dir_leaf = YVEX_SOURCE_QWEN3_5_08B_TARGET_ID,
        .upstream_revision = YVEX_SOURCE_QWEN3_5_08B_REVISION,
        .upstream_index_path = "model.safetensors.index.json",
        .upstream_index_oid = "f691cefdb79d73270895ebd6d9594ddcecfc1838",
        .upstream_index_size = 50900ull,
        .upstream_inventory_authority = "upstream-index",
        .config_model_type = "qwen3_5",
        .config_architecture = "Qwen3_5ForConditionalGeneration",
        .config_validation = YVEX_SOURCE_CONFIG_VALIDATION_FAMILY_SEMANTIC,
        /* This exact source has no generation_config.json. Generation and
         * conversation interpretation remain family-owned, not fabricated files. */
        .required_sidecars = YVEX_SOURCE_SIDECAR_CONFIG |
            YVEX_SOURCE_SIDECAR_TOKENIZER | YVEX_SOURCE_SIDECAR_TOKENIZER_CONFIG,
    },
};

const yvex_source_logical_model *yvex_source_logical_model_for_registry(
    const char *family, const char *model)
{
    const yvex_source_logical_model *selected = NULL;
    size_t index, family_index, model_index;
    if (!family || !model || !family[0] || !model[0]) return NULL;
    for (index = 0u; index < sizeof(source_target_identities) /
                                  sizeof(source_target_identities[0]); ++index) {
        const yvex_source_logical_model *relation = source_target_identities[index].logical_model;
        if (!relation) continue;
        for (family_index = 0u; family_index < 2u; ++family_index)
            for (model_index = 0u; model_index < 2u; ++model_index)
                if (relation->family_aliases[family_index] && relation->model_aliases[model_index] &&
                    !strcmp(family, relation->family_aliases[family_index]) &&
                    !strcmp(model, relation->model_aliases[model_index])) {
                    if (selected && selected != relation) return NULL;
                    selected = relation;
                }
    }
    return selected;
}

const yvex_source_logical_model *yvex_source_logical_model_for_revision(
    const char *provider, const char *repository, const char *revision)
{
    const yvex_source_logical_model *selected = NULL;
    size_t index;
    if (!provider || (strcmp(provider, "hf") && strcmp(provider, "huggingface")) ||
        !repository || !revision || !repository[0] || !revision[0]) return NULL;
    for (index = 0u; index < sizeof(source_target_identities) /
                                  sizeof(source_target_identities[0]); ++index) {
        const yvex_source_target_identity *target = &source_target_identities[index];
        const yvex_source_logical_model *relation = target->logical_model;
        if (!relation) continue;
        if ((!strcmp(repository, target->upstream_repo_id) && !strcmp(revision, target->upstream_revision)) ||
            (relation->related_repository && relation->related_revision &&
             !strcmp(repository, relation->related_repository) && !strcmp(revision, relation->related_revision))) {
            if (selected && selected != relation) return NULL;
            selected = relation;
        }
    }
    return selected;
}

static const yvex_source_acquisition_target source_acquisition_targets[] = {
    {"gemma-4-e2b", "gemma", "hf", "google/gemma-4-E2B", "gemma-4-e2b", "main"},
    {"gemma-4-e2b-it", "gemma", "hf", "google/gemma-4-E2B-it", "gemma-4-e2b-it", "main"},
    {"gemma-4-e4b", "gemma", "hf", "google/gemma-4-E4B", "gemma-4-e4b", "main"},
    {"gemma-4-e4b-it", "gemma", "hf", "google/gemma-4-E4B-it", "gemma-4-e4b-it", "main"},
    {"gemma-4-12b", "gemma", "hf", "google/gemma-4-12B", "gemma-4-12b", "main"},
    {"gemma-4-12b-it", "gemma", "hf", "google/gemma-4-12B-it", "gemma-4-12b-it", "main"},
    {"gemma-4-26b-a4b", "gemma", "hf", "google/gemma-4-26B-A4B", "gemma-4-26b-a4b", "main"},
    {"gemma-4-26b-a4b-it", "gemma", "hf", "google/gemma-4-26B-A4B-it", "gemma-4-26b-a4b-it", "main"},
    {"gemma-4-31b", "gemma", "hf", "google/gemma-4-31B", "gemma-4-31b", "main"},
    {"gemma-4-31b-it", "gemma", "hf", "google/gemma-4-31B-it", "gemma-4-31b-it", "main"},
    {"qwen3-8b", "qwen", "hf", "Qwen/Qwen3-8B", "qwen3-8b", "main"},
    {"qwen3-32b", "qwen", "hf", "Qwen/Qwen3-32B", "qwen3-32b", "main"},
};

const yvex_source_target_identity *yvex_source_release_identity(void)
{
    return &source_target_identities[0];
}

const yvex_source_target_identity *yvex_source_target_identity_find(
    const char *target_id)
{
    unsigned long long index;

    if (!target_id) return NULL;
    for (index = 0ull;
         index < sizeof(source_target_identities) / sizeof(source_target_identities[0]);
         ++index)
        if (strcmp(source_target_identities[index].target_id, target_id) == 0)
            return &source_target_identities[index];
    return NULL;
}

const yvex_source_target_identity *yvex_source_target_identity_find_repository(
    const char *repository)
{
    unsigned long long index;

    if (!repository) return NULL;
    for (index = 0ull;
         index < sizeof(source_target_identities) / sizeof(source_target_identities[0]);
         ++index)
        if (strcmp(source_target_identities[index].upstream_repo_id, repository) == 0)
            return &source_target_identities[index];
    return NULL;
}

const yvex_source_acquisition_target *yvex_source_acquisition_target_find(
    const char *target_id)
{
    unsigned long long index;

    if (!target_id) return NULL;
    for (index = 0ull;
         index < sizeof(source_acquisition_targets) / sizeof(source_acquisition_targets[0]);
         ++index)
        if (strcmp(source_acquisition_targets[index].target_id, target_id) == 0)
            return &source_acquisition_targets[index];
    return NULL;
}

int yvex_source_is_release_target(const char *target_id)
{
    return target_id &&
           strcmp(target_id, source_target_identities[0].target_id) == 0;
}

int yvex_source_target_path(char *out, size_t cap, const char *models_root,
                            const yvex_source_target_identity *identity)
{
    return identity && yvex_source_provider_path(
        out, cap, models_root, identity->upstream_repo_id,
        identity->upstream_revision);
}

int yvex_source_provider_path(char *out, size_t cap, const char *models_root,
                              const char *repository, const char *revision)
{
    size_t index, slashes = 0u, segment = 0u, length;
    int written;

    if (!out || !cap) return 0;
    out[0] = '\0';
    if (!models_root || !models_root[0] || !repository || !revision) return 0;
    length = strlen(revision);
    if (length != 40u && length != 64u) return 0;
    for (index = 0u; index < length; ++index)
        if (!isxdigit((unsigned char)revision[index])) return 0;
    for (index = 0u; repository[index]; ++index) {
        unsigned char value = (unsigned char)repository[index];
        if (value == '/') {
            if (!segment || ++slashes > 1u) return 0;
            segment = 0u;
        } else {
            if ((!segment && value == '.') ||
                (!isalnum(value) && value != '-' && value != '_' && value != '.'))
                return 0;
            segment++;
        }
    }
    if (slashes != 1u || !segment) return 0;
    written = snprintf(out, cap, "%s/source/hf/%s/%s", models_root,
                        repository, revision);
    if (written < 0 || (size_t)written >= cap) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}
