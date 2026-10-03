/* Model artifact discovery owns family/catalog metadata observations; renderers do not reconstruct
 * source sidecars or infer runtime admission from a filename. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/artifact_catalog.h>
#include <yvex/internal/source.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define ARTIFACT_CATALOG_CAP 256u
typedef struct { const char *family; } catalog_selection;
struct yvex_artifact_catalog {
    yvex_artifact_catalog_row rows[ARTIFACT_CATALOG_CAP];
    unsigned int count;
    int overflow;
};
static int catalog_path_exists(const char *path)
{
    return path && path[0] && access(path, F_OK) == 0;
}
static int catalog_path_join(char *out, size_t capacity, const char *directory,
    const char *leaf, yvex_error *err, const char *where)
{
    int length = snprintf(out, capacity, "%s/%s", directory, leaf);
    if (length < 0 || (size_t)length >= capacity) {
        if (capacity) out[0] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "catalog path exceeds native bounds");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}
static int artifacts_family_allowed(const catalog_selection *options,
                                    const char *family)
{
    return !options || !options->family || strcmp(options->family, family) == 0;
}

static int artifacts_rows_find(yvex_artifact_catalog *rows,
                               const char *target,
                               const char *family)
{
    unsigned int i;

    if (!rows || !target || !family) return -1;
    for (i = 0; i < rows->count; ++i) {
        if (strcmp(rows->rows[i].target_id, target) == 0 &&
            strcmp(rows->rows[i].family, family) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static yvex_artifact_catalog_row *artifacts_rows_append(
    yvex_artifact_catalog *rows,
    const char *target,
    const char *family)
{
    yvex_artifact_catalog_row *row;

    if (!rows || !target || !family || !target[0] || !family[0]) return NULL;
    if (artifacts_rows_find(rows, target, family) >= 0) {
        return &rows->rows[artifacts_rows_find(rows, target, family)];
    }
    if (rows->count >= ARTIFACT_CATALOG_CAP) { rows->overflow = 1; return NULL; }
    row = &rows->rows[rows->count++];
    memset(row, 0, sizeof(*row));
    snprintf(row->target_id, sizeof(row->target_id), "%s", target);
    snprintf(row->family, sizeof(row->family), "%s", family);
    snprintf(row->artifact_status, sizeof(row->artifact_status), "missing");
    snprintf(row->source_status, sizeof(row->source_status), "not-applicable");
    snprintf(row->prepare_status, sizeof(row->prepare_status), "unknown");
    snprintf(row->top_blocker, sizeof(row->top_blocker), "none");
    snprintf(row->detail, sizeof(row->detail), "artifact discovery only");
    return row;
}

static void artifacts_relative_path(const yvex_operator_paths *operator_paths,
                                    const char *path,
                                    char *out,
                                    size_t out_cap)
{
    size_t root_len;

    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (!path || !path[0]) {
        snprintf(out, out_cap, "-");
        return;
    }
    if (operator_paths && operator_paths->models_root[0]) {
        root_len = strlen(operator_paths->models_root);
        if (strncmp(path, operator_paths->models_root, root_len) == 0 &&
            path[root_len] == '/') {
            snprintf(out, out_cap, "%s", path + root_len + 1u);
            return;
        }
    }
    snprintf(out, out_cap, "%s", path);
}

static void artifacts_strip_suffix(char *text, const char *suffix)
{
    if (!yvex_source_ends_with(text, suffix))
        return;
    text[strlen(text) - strlen(suffix)] = '\0';
}

static void artifacts_target_from_gguf_name(const char *file_name,
                                            char *out,
                                            size_t out_cap)
{
    if (!out || out_cap == 0u) return;
    out[0] = '\0';
    if (!file_name || !file_name[0]) return;
    yvex_core_text_copy(out, out_cap, file_name);
    artifacts_strip_suffix(out, ".gguf");
    artifacts_strip_suffix(out, "-F16-noimatrix-yvex-v1");
}

static const char *artifacts_class_from_name(const char *file_name)
{
    if (file_name && strstr(file_name, "controlled")) return "yvex-controlled-gguf";
    if (file_name && strstr(file_name, "selected")) return "yvex-selected-gguf";
    return "unknown-gguf";
}

static int artifacts_stat_file(const char *path, unsigned long long *size_out)
{
    struct stat st;

    if (size_out) *size_out = 0ull;
    if (!path || stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return 0;
    if (size_out) *size_out = (unsigned long long)st.st_size;
    return 1;
}

static void artifacts_classify_dynamic_row(const yvex_operator_paths *operator_paths,
                                           yvex_artifact_catalog_row *row)
{
    int n;

    if (!operator_paths || !row) return;
    n = snprintf(row->expected_path, sizeof(row->expected_path),
                 "%s/%s/%s.gguf",
                 operator_paths->reference_root,
                 row->family,
                 row->target_id);
    if (n < 0 || (size_t)n >= sizeof(row->expected_path)) row->expected_path[0] = '\0';
    if (row->expected_path[0] &&
        artifacts_stat_file(row->expected_path, &row->size_bytes)) {
        snprintf(row->path, sizeof(row->path), "%s", row->expected_path);
        snprintf(row->artifact_status, sizeof(row->artifact_status), "present");
    } else {
        snprintf(row->artifact_status, sizeof(row->artifact_status), "missing");
        row->size_bytes = 0ull;
    }
    artifacts_relative_path(operator_paths,
                            row->path[0] ? row->path : row->expected_path,
                            row->display_path,
                            sizeof(row->display_path));
    snprintf(row->artifact_class, sizeof(row->artifact_class), "planned-full-gguf");
    row->source_present = row->source_status[0] &&
                          strcmp(row->source_status, "present") == 0;
    row->tensor_map_present = row->tensor_map_path[0] && catalog_path_exists(row->tensor_map_path);
    row->output_head_map_present =
        row->output_head_map_path[0] && catalog_path_exists(row->output_head_map_path);
    row->tokenizer_map_present =
        row->tokenizer_map_path[0] && catalog_path_exists(row->tokenizer_map_path);
    yvex_source_mapping_status(row->tensor_map_path,
                                     row->output_head_map_path,
                                     &row->tensor_map_incomplete,
                                     &row->output_head_missing);
    snprintf(row->tensor_map_status, sizeof(row->tensor_map_status), "%s",
        !row->tensor_map_present ? "missing" : row->tensor_map_incomplete
            ? "incomplete-report-only" : "present-report-only");
    snprintf(row->output_head_map_status, sizeof(row->output_head_map_status), "%s",
        !row->output_head_map_present ? "missing" : row->output_head_missing
            ? "missing-in-report" : "present-report-only");
    snprintf(row->tokenizer_map_status, sizeof(row->tokenizer_map_status), "%s",
        row->tokenizer_map_present ? "present-report-only" : "missing");
    /* Discovery reports all missing prerequisites; even complete sidecar
     * observations do not establish a full emitter or artifact admission. */
    row->prepare_blocker_count = 1u + (unsigned int)!row->source_present +
        (unsigned int)(!row->tensor_map_present || row->tensor_map_incomplete) +
        (unsigned int)(!row->output_head_map_present || row->output_head_missing) +
        (unsigned int)!row->tokenizer_map_present +
        (unsigned int)(strcmp(row->artifact_status, "present") != 0);
    snprintf(row->prepare_status, sizeof(row->prepare_status), "blocked");
    if (!row->source_present) {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "missing-source");
        snprintf(row->detail, sizeof(row->detail), "downloaded source path is missing");
    } else if (!row->output_head_map_present || row->output_head_missing) {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "missing-output-head-map");
        snprintf(row->detail, sizeof(row->detail),
            "output-head/tokenizer mapping missing; full GGUF emission not performed");
    } else if (!row->tensor_map_present || row->tensor_map_incomplete) {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "incomplete-tensor-map");
        snprintf(row->detail, sizeof(row->detail), "tensor map incomplete; full GGUF emission not performed");
    } else if (!row->tokenizer_map_present) {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "missing-tokenizer-map");
        snprintf(row->detail, sizeof(row->detail),
            "tokenizer metadata mapping missing; full GGUF emission not performed");
    } else if (strcmp(row->artifact_status, "missing") == 0) {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "%s",
                 strcmp(row->family, "deepseek") == 0
                     ? "complete-artifact-admission-required"
                     : "family-quantization-plan-unimplemented");
        snprintf(row->detail, sizeof(row->detail), "%s",
                 strcmp(row->family, "deepseek") == 0
                     ? "artifact discovery did not bind canonical complete-artifact admission"
                     : "this engineering family has no complete quantization plan");
    } else {
        snprintf(row->top_blocker, sizeof(row->top_blocker), "missing-artifact-identity");
        snprintf(row->detail, sizeof(row->detail),
            "artifact exists but identity/admission is not checked by this discovery command");
    }
}

const char *yvex_artifact_catalog_next(const yvex_artifact_catalog_row *row)
{
    if (!row || strcmp(row->prepare_status, "blocked") != 0) return "none";
    if (strcmp(row->top_blocker, "missing-tokenizer-map") == 0) {
        return "V010.MAP.7";
    }
    if (strcmp(row->top_blocker,
               "complete-artifact-admission-required") == 0) {
        return "V010.ARTIFACT.MATERIALIZE.0";
    }
    if (strcmp(row->top_blocker,
               "family-quantization-plan-unimplemented") == 0) {
        return "not-scheduled";
    }
    return "V010.MAP.8";
}

static void artifacts_add_dynamic_target(
    const yvex_operator_paths *operator_paths,
    yvex_artifact_catalog *rows,
    const char *target,
    const char *family,
    const yvex_source_acquisition_provenance *resolved)
{
    yvex_artifact_catalog_row *row;
    char family_dir[YVEX_PATH_CAP];

    row = artifacts_rows_append(rows, target, family);
    if (!row || !operator_paths) return;
    row->dynamic_source = 1;
    snprintf(row->artifact_class, sizeof(row->artifact_class), "planned-full-gguf");
    if (resolved) {
        snprintf(row->registry_path, sizeof(row->registry_path), "%s", resolved->registry_path);
        snprintf(row->download_report_path, sizeof(row->download_report_path), "%s", resolved->download_report_path);
        snprintf(row->source_manifest_path, sizeof(row->source_manifest_path), "%s", resolved->manifest_path);
        snprintf(row->native_inventory_path, sizeof(row->native_inventory_path), "%s", resolved->native_inventory_path);
        if (resolved->local_source_dir[0] && catalog_path_exists(resolved->local_source_dir)) {
            snprintf(row->source_status, sizeof(row->source_status), "present");
        } else {
            snprintf(row->source_status, sizeof(row->source_status), "missing");
        }
    }
    if (!row->source_status[0] || strcmp(row->source_status, "not-applicable") == 0) {
        snprintf(row->source_status, sizeof(row->source_status), "missing");
    }
    if (catalog_path_join(family_dir, sizeof(family_dir), operator_paths->reports_root,
                   family, NULL, "models_artifacts") == YVEX_OK) {
        char file_name[256];
        snprintf(file_name, sizeof(file_name), "%s.tensor-map.json", target);
        (void)catalog_path_join(row->tensor_map_path, sizeof(row->tensor_map_path),
                         family_dir, file_name, NULL, "models_artifacts");
        snprintf(file_name, sizeof(file_name), "%s.output-head-map.json", target);
        (void)catalog_path_join(row->output_head_map_path, sizeof(row->output_head_map_path),
                         family_dir, file_name, NULL, "models_artifacts");
        snprintf(file_name, sizeof(file_name), "%s.tokenizer-map.json", target);
        (void)catalog_path_join(row->tokenizer_map_path, sizeof(row->tokenizer_map_path),
                         family_dir, file_name, NULL, "models_artifacts");
    }
    artifacts_classify_dynamic_row(operator_paths, row);
}

static void artifacts_scan_gguf_family(const yvex_operator_paths *operator_paths,
                                       yvex_artifact_catalog *rows,
                                       const char *family, const char *root)
{
    DIR *dir;
    struct dirent *ent;
    char family_dir[YVEX_PATH_CAP];
    yvex_error err;

    yvex_error_clear(&err);
    if (!operator_paths || !rows || !family) return;
    if (catalog_path_join(family_dir, sizeof(family_dir), root,
                   family, &err, "models_artifacts") != YVEX_OK) {
        return;
    }
    dir = opendir(family_dir);
    if (!dir) return;
    while ((ent = readdir(dir)) != NULL) {
        char path[YVEX_PATH_CAP];
        char target[128];
        yvex_artifact_catalog_row *row;

        if (ent->d_name[0] == '.') continue;
        if (!yvex_source_ends_with(ent->d_name, ".gguf")) continue;
        if (catalog_path_join(path, sizeof(path), family_dir, ent->d_name, &err,
                       "models_artifacts") != YVEX_OK) {
            continue;
        }
        if (!artifacts_stat_file(path, NULL)) continue;
        artifacts_target_from_gguf_name(ent->d_name, target, sizeof(target));
        row = artifacts_rows_append(rows, target, family);
        if (!row) continue;
        snprintf(row->artifact_class, sizeof(row->artifact_class), "%s",
                 artifacts_class_from_name(ent->d_name));
        snprintf(row->artifact_status, sizeof(row->artifact_status), "present");
        snprintf(row->source_status, sizeof(row->source_status), "not-applicable");
        snprintf(row->prepare_status, sizeof(row->prepare_status), "ready");
        snprintf(row->top_blocker, sizeof(row->top_blocker), "none");
        snprintf(row->detail, sizeof(row->detail), "GGUF artifact present");
        snprintf(row->path, sizeof(row->path), "%s", path);
        snprintf(row->expected_path, sizeof(row->expected_path), "%s", path);
        artifacts_stat_file(path, &row->size_bytes);
        artifacts_relative_path(operator_paths, path, row->display_path,
                                sizeof(row->display_path));
    }
    closedir(dir);
}

static void artifacts_scan_dynamic_sidecar_dir(
    const yvex_operator_paths *operator_paths,
    yvex_artifact_catalog *rows,
    const char *family,
    const char *dir_path,
    const char *suffix)
{
    DIR *dir;
    struct dirent *ent;
    yvex_error err;

    if (!operator_paths || !rows || !family || !dir_path || !suffix) return;
    dir = opendir(dir_path);
    if (!dir) return;
    yvex_error_clear(&err);
    while ((ent = readdir(dir)) != NULL) {
        char target[128];
        char path[YVEX_PATH_CAP];
        yvex_source_acquisition_provenance resolved;
        size_t name_len;

        if (ent->d_name[0] == '.') continue;
        if (!yvex_source_ends_with(ent->d_name, suffix)) continue;
        name_len = strlen(ent->d_name);
        if (name_len >= sizeof(target)) continue;
        memcpy(target, ent->d_name, name_len + 1u);
        artifacts_strip_suffix(target, suffix);
        if (!yvex_source_acquisition_provenance_paths(target, family, operator_paths,
                                           &resolved, &err)) {
            yvex_error_clear(&err);
            continue;
        }
        if (catalog_path_join(path, sizeof(path), dir_path, ent->d_name, &err,
                       "models_artifacts") != YVEX_OK) {
            yvex_error_clear(&err);
            continue;
        }
        if (yvex_source_acquisition_provenance_read(path, target, family, &resolved) ||
            yvex_source_acquisition_provenance_resolve(target, operator_paths,
                                                     &resolved, &err) > 0) {
            artifacts_add_dynamic_target(operator_paths, rows,
                                         resolved.target_id[0] ? resolved.target_id : target,
                                         resolved.family[0] ? resolved.family : family,
                                         &resolved);
        }
        yvex_error_clear(&err);
    }
    closedir(dir);
}

static int artifacts_collect(const catalog_selection *options,
                             const yvex_operator_paths *operator_paths,
                             yvex_artifact_catalog *rows,
                             yvex_error *err)
{
    static const char *families[] = { "deepseek", "qwen", "gemma", "glm" };
    unsigned long i;

    if (!options || !operator_paths || !rows) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "models_artifacts",
                       "options, operator paths, and rows are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(rows, 0, sizeof(*rows));
    for (i = 0; i < sizeof(families) / sizeof(families[0]); ++i) {
        char registry_family_dir[YVEX_PATH_CAP];
        char reports_family_dir[YVEX_PATH_CAP];
        const char *family = families[i];

        if (!artifacts_family_allowed(options, family)) continue;
        artifacts_scan_gguf_family(operator_paths, rows, family, operator_paths->gguf_root);
        artifacts_scan_gguf_family(operator_paths, rows, family, operator_paths->reference_root);
        if (catalog_path_join(registry_family_dir, sizeof(registry_family_dir),
                       operator_paths->registry_root, family, err,
                       "models_artifacts") == YVEX_OK) {
            artifacts_scan_dynamic_sidecar_dir(operator_paths, rows, family,
                                               registry_family_dir,
                                               ".download.json");
        }
        yvex_error_clear(err);
        if (catalog_path_join(reports_family_dir, sizeof(reports_family_dir),
                       operator_paths->reports_root, family, err,
                       "models_artifacts") == YVEX_OK) {
            artifacts_scan_dynamic_sidecar_dir(operator_paths, rows, family,
                                               reports_family_dir,
                                               ".download-report.json");
            artifacts_scan_dynamic_sidecar_dir(operator_paths, rows, family,
                                               reports_family_dir,
                                               ".source-manifest.json");
        }
        yvex_error_clear(err);
    }
    return YVEX_OK;
}


int yvex_artifact_catalog_open(yvex_artifact_catalog **out, const yvex_operator_paths *paths,
    const char *family, yvex_error *err)
{
    catalog_selection selection = {family};
    yvex_artifact_catalog *catalog;
    int rc;
    if (out) *out = NULL;
    if (!out || !paths || (family && strcmp(family, "deepseek") && strcmp(family, "qwen") &&
        strcmp(family, "gemma") && strcmp(family, "glm"))) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "artifact.catalog", "paths and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    catalog = calloc(1u, sizeof(*catalog));
    if (!catalog) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "artifact.catalog", "catalog allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rc = artifacts_collect(&selection, paths, catalog, err);
    if (rc == YVEX_OK && catalog->overflow) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "artifact.catalog", "catalog exceeds its bounded row population");
        rc = YVEX_ERR_BOUNDS;
    }
    if (rc != YVEX_OK) { free(catalog); return rc; }
    *out = catalog;
    return YVEX_OK;
}
void yvex_artifact_catalog_close(yvex_artifact_catalog *catalog) { free(catalog); }
unsigned int yvex_artifact_catalog_count(const yvex_artifact_catalog *catalog)
{
    return catalog ? catalog->count : 0u;
}
const yvex_artifact_catalog_row *yvex_artifact_catalog_at(
    const yvex_artifact_catalog *catalog, unsigned int index)
{
    return catalog && index < catalog->count ? &catalog->rows[index] : NULL;
}
