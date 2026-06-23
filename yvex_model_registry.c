/*
 * YVEX - Local model registry implementation
 */
#include "yvex_model_registry_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *yvex_model_registry_strdup(const char *s)
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

void yvex_model_registry_owned_entry_clear(yvex_model_registry_owned_entry *entry)
{
    if (!entry) return;
    free(entry->alias);
    free(entry->family);
    free(entry->model);
    free(entry->scope);
    free(entry->artifact_class);
    free(entry->qprofile);
    free(entry->calibration);
    free(entry->producer);
    free(entry->schema_version);
    free(entry->path);
    free(entry->sha256);
    free(entry->support_level);
    memset(entry, 0, sizeof(*entry));
}

void yvex_model_registry_entry_view(const yvex_model_registry_owned_entry *owned,
                                    yvex_model_registry_entry *view)
{
    memset(view, 0, sizeof(*view));
    if (!owned) return;
    view->alias = owned->alias;
    view->family = owned->family;
    view->model = owned->model;
    view->scope = owned->scope;
    view->artifact_class = owned->artifact_class;
    view->qprofile = owned->qprofile;
    view->calibration = owned->calibration;
    view->producer = owned->producer;
    view->schema_version = owned->schema_version;
    view->path = owned->path;
    view->sha256 = owned->sha256;
    view->support_level = owned->support_level;
    view->execution_ready = owned->execution_ready;
}

int yvex_model_registry_copy_entry(yvex_model_registry_owned_entry *dst,
                                   const yvex_model_registry_entry *src,
                                   yvex_error *err)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || !src->alias || !src->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry", "entry alias and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    dst->alias = yvex_model_registry_strdup(src->alias);
    dst->family = yvex_model_registry_strdup(src->family);
    dst->model = yvex_model_registry_strdup(src->model);
    dst->scope = yvex_model_registry_strdup(src->scope);
    dst->artifact_class = yvex_model_registry_strdup(src->artifact_class);
    dst->qprofile = yvex_model_registry_strdup(src->qprofile);
    dst->calibration = yvex_model_registry_strdup(src->calibration);
    dst->producer = yvex_model_registry_strdup(src->producer);
    dst->schema_version = yvex_model_registry_strdup(src->schema_version);
    dst->path = yvex_model_registry_strdup(src->path);
    dst->sha256 = yvex_model_registry_strdup(src->sha256);
    dst->support_level = yvex_model_registry_strdup(src->support_level);
    dst->execution_ready = src->execution_ready;
    if (!dst->alias || !dst->family || !dst->model || !dst->scope ||
        !dst->artifact_class || !dst->qprofile || !dst->calibration ||
        !dst->producer || !dst->schema_version || !dst->path ||
        !dst->sha256 || !dst->support_level) {
        yvex_model_registry_owned_entry_clear(dst);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry", "entry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

static int registry_reserve(yvex_model_registry *registry,
                            unsigned long long need,
                            yvex_error *err)
{
    yvex_model_registry_owned_entry *next;
    unsigned long long cap;

    if (need <= registry->cap) return YVEX_OK;
    cap = registry->cap ? registry->cap * 2u : 4u;
    while (cap < need) cap *= 2u;
    next = (yvex_model_registry_owned_entry *)realloc(registry->entries, (size_t)cap * sizeof(*next));
    if (!next) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry", "registry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    memset(next + registry->cap, 0, (size_t)(cap - registry->cap) * sizeof(*next));
    registry->entries = next;
    registry->cap = cap;
    return YVEX_OK;
}

static int is_ambiguous_token(const char *alias)
{
    return strcmp(alias, "latest") == 0 ||
           strcmp(alias, "final") == 0 ||
           strcmp(alias, "new") == 0 ||
           strcmp(alias, "test") == 0 ||
           strcmp(alias, "tmp") == 0 ||
           strcmp(alias, "debug") == 0 ||
           strstr(alias, "-latest") || strstr(alias, "latest-") ||
           strstr(alias, "-final") || strstr(alias, "final-") ||
           strstr(alias, "-new") || strstr(alias, "new-") ||
           strstr(alias, "-test") || strstr(alias, "test-") ||
           strstr(alias, "-tmp") || strstr(alias, "tmp-") ||
           strstr(alias, "-debug") || strstr(alias, "debug-");
}

int yvex_model_alias_validate(const char *alias, yvex_error *err)
{
    const char *p;
    int hyphens = 0;

    if (!alias || !alias[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (alias[0] == '-' || alias[strlen(alias) - 1u] == '-') {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not start or end with hyphen");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strstr(alias, "--")) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not contain empty segments");
        return YVEX_ERR_INVALID_ARG;
    }
    if (strchr(alias, '/') || strstr(alias, "..")) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must not be path-like");
        return YVEX_ERR_INVALID_ARG;
    }
    for (p = alias; *p; ++p) {
        if (*p == '-') {
            hyphens++;
            continue;
        }
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias uses invalid characters");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    if (hyphens < 3) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias must include family, model, scope, and artifact class");
        return YVEX_ERR_INVALID_ARG;
    }
    if (is_ambiguous_token(alias)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_alias", "alias contains ambiguous vocabulary");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_model_registry_default_path(char *out,
                                     unsigned long long out_size,
                                     yvex_error *err)
{
    const char *env = getenv("YVEX_MODELS_REGISTRY");
    int n;

    if (!out || out_size == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_path", "output buffer is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (env && env[0]) {
        n = snprintf(out, (size_t)out_size, "%s", env);
    } else {
        n = snprintf(out, (size_t)out_size, ".yvex/models.local.json");
    }
    if (n < 0 || (unsigned long long)n >= out_size) {
        out[0] = '\0';
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model_registry_path", "registry path buffer too small");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int yvex_model_registry_open(yvex_model_registry **out,
                             const yvex_model_registry_options *options,
                             yvex_error *err)
{
    yvex_model_registry *registry;
    char path[4096];
    const char *registry_path = NULL;
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    registry = (yvex_model_registry *)calloc(1u, sizeof(*registry));
    if (!registry) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_open", "registry allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (options && options->registry_path && options->registry_path[0]) {
        registry_path = options->registry_path;
    } else {
        rc = yvex_model_registry_default_path(path, sizeof(path), err);
        if (rc != YVEX_OK) {
            free(registry);
            return rc;
        }
        registry_path = path;
    }
    if (access(registry_path, F_OK) == 0) {
        rc = yvex_model_registry_parse_json_file(registry_path, registry, err);
        if (rc != YVEX_OK) {
            yvex_model_registry_close(registry);
            return rc;
        }
    } else if (!(options && options->create_if_missing)) {
        yvex_model_registry_close(registry);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_open", "registry does not exist: %s", registry_path);
        return YVEX_ERR_IO;
    }
    *out = registry;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_model_registry_close(yvex_model_registry *registry)
{
    unsigned long long i;

    if (!registry) return;
    free(registry->selected);
    for (i = 0; i < registry->count; ++i) {
        yvex_model_registry_owned_entry_clear(&registry->entries[i]);
    }
    free(registry->entries);
    free(registry);
}

unsigned long long yvex_model_registry_count(const yvex_model_registry *registry)
{
    return registry ? registry->count : 0u;
}

const yvex_model_registry_entry *yvex_model_registry_at(const yvex_model_registry *registry,
                                                        unsigned long long index)
{
    static yvex_model_registry_entry view;

    if (!registry || index >= registry->count) return NULL;
    yvex_model_registry_entry_view(&registry->entries[index], &view);
    return &view;
}

const yvex_model_registry_entry *yvex_model_registry_find(const yvex_model_registry *registry,
                                                          const char *alias)
{
    unsigned long long i;
    static yvex_model_registry_entry view;

    if (!registry || !alias) return NULL;
    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].alias, alias) == 0) {
            yvex_model_registry_entry_view(&registry->entries[i], &view);
            return &view;
        }
    }
    return NULL;
}

const yvex_model_registry_entry *yvex_model_registry_selected(const yvex_model_registry *registry)
{
    if (!registry || !registry->selected || !registry->selected[0]) return NULL;
    return yvex_model_registry_find(registry, registry->selected);
}

int yvex_model_registry_add(yvex_model_registry *registry,
                            const yvex_model_registry_entry *entry,
                            yvex_error *err)
{
    yvex_model_registry_owned_entry copy;
    int rc;

    if (!registry || !entry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_add", "registry and entry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_model_alias_validate(entry->alias, err);
    if (rc != YVEX_OK) return rc;
    if (yvex_model_registry_find(registry, entry->alias)) {
        yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_add", "duplicate alias: %s", entry->alias);
        return YVEX_ERR_STATE;
    }
    if (access(entry->path, F_OK) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_add", "model path does not exist: %s", entry->path);
        return YVEX_ERR_IO;
    }
    rc = registry_reserve(registry, registry->count + 1u, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_model_registry_copy_entry(&copy, entry, err);
    if (rc != YVEX_OK) return rc;
    registry->entries[registry->count++] = copy;
    return YVEX_OK;
}

int yvex_model_registry_remove(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err)
{
    unsigned long long i;

    if (!registry || !alias) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_remove", "registry and alias are required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (i = 0; i < registry->count; ++i) {
        if (strcmp(registry->entries[i].alias, alias) == 0) {
            yvex_model_registry_owned_entry_clear(&registry->entries[i]);
            if (i + 1u < registry->count) {
                memmove(&registry->entries[i], &registry->entries[i + 1u],
                        (size_t)(registry->count - i - 1u) * sizeof(registry->entries[0]));
            }
            registry->count--;
            memset(&registry->entries[registry->count], 0, sizeof(registry->entries[0]));
            if (registry->selected && strcmp(registry->selected, alias) == 0) {
                free(registry->selected);
                registry->selected = NULL;
            }
            return YVEX_OK;
        }
    }
    yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_remove", "alias not found: %s", alias);
    return YVEX_ERR_STATE;
}

int yvex_model_registry_select(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err)
{
    char *copy;

    if (!registry || !alias) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_select", "registry and alias are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_model_registry_find(registry, alias)) {
        yvex_error_setf(err, YVEX_ERR_STATE, "model_registry_select", "alias not found: %s", alias);
        return YVEX_ERR_STATE;
    }
    copy = yvex_model_registry_strdup(alias);
    if (!copy) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_select", "selected alias allocation failed");
        return YVEX_ERR_NOMEM;
    }
    free(registry->selected);
    registry->selected = copy;
    return YVEX_OK;
}

int yvex_model_registry_save(const yvex_model_registry *registry,
                             const char *path,
                             yvex_error *err)
{
    char default_path[4096];

    if (!registry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_save", "registry is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!path || !path[0]) {
        int rc = yvex_model_registry_default_path(default_path, sizeof(default_path), err);
        if (rc != YVEX_OK) return rc;
        path = default_path;
    }
    return yvex_model_registry_write_json_file(registry, path, err);
}

static int split_canonical_stem(const char *stem,
                                char *family, size_t family_cap,
                                char *model, size_t model_cap,
                                char *scope, size_t scope_cap,
                                char *artifact_class, size_t class_cap,
                                char *qprofile, size_t qprofile_cap,
                                char *calibration, size_t calibration_cap,
                                char *producer, size_t producer_cap,
                                char *schema, size_t schema_cap,
                                char *alias, size_t alias_cap)
{
    char buf[1024];
    char *parts[64];
    int count = 0;
    char *tok;
    int tail;
    int i;
    size_t pos = 0;

    if (strlen(stem) >= sizeof(buf)) return 0;
    strcpy(buf, stem);
    tok = strtok(buf, "-");
    while (tok && count < 64) {
        parts[count++] = tok;
        tok = strtok(NULL, "-");
    }
    if (count < 8) return 0;
    tail = count - 4;
    snprintf(qprofile, qprofile_cap, "%s", parts[tail]);
    snprintf(calibration, calibration_cap, "%s", parts[tail + 1]);
    snprintf(producer, producer_cap, "%s", parts[tail + 2]);
    snprintf(schema, schema_cap, "%s", parts[tail + 3]);
    snprintf(family, family_cap, "%s", parts[0]);
    snprintf(scope, scope_cap, "%s", parts[tail - 2]);
    snprintf(artifact_class, class_cap, "%s", parts[tail - 1]);
    model[0] = '\0';
    for (i = 1; i < tail - 2; ++i) {
        int n = snprintf(model + pos, model_cap > pos ? model_cap - pos : 0,
                         "%s%s", pos ? "-" : "", parts[i]);
        if (n < 0 || (size_t)n >= (model_cap > pos ? model_cap - pos : 0)) return 0;
        pos += (size_t)n;
    }
    snprintf(alias, alias_cap, "%s-%s-%s-%s", family, model, scope, artifact_class);
    return strcmp(producer, "yvex") == 0 && strcmp(schema, "v1") == 0 &&
           family[0] && model[0] && scope[0] && artifact_class[0] &&
           qprofile[0] && calibration[0];
}

int yvex_model_registry_entry_derive_from_path(yvex_model_registry_entry *entry,
                                               const char *path,
                                               yvex_error *err)
{
    static char alias[256];
    static char family[128];
    static char model[128];
    static char scope[64];
    static char artifact_class[128];
    static char qprofile[64];
    static char calibration[128];
    static char producer[64];
    static char schema[64];
    static char path_copy[4096];
    char stem[1024];
    const char *base;
    size_t len;

    if (!entry || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_derive", "entry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    base = strrchr(path, '/');
    base = base ? base + 1 : path;
    len = strlen(base);
    if (len <= 5u || strcmp(base + len - 5u, ".gguf") != 0 || len >= sizeof(stem)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_derive", "filename is not a canonical GGUF artifact name");
        return YVEX_ERR_FORMAT;
    }
    memcpy(stem, base, len - 5u);
    stem[len - 5u] = '\0';
    if (!split_canonical_stem(stem, family, sizeof(family), model, sizeof(model),
                              scope, sizeof(scope), artifact_class, sizeof(artifact_class),
                              qprofile, sizeof(qprofile), calibration, sizeof(calibration),
                              producer, sizeof(producer), schema, sizeof(schema),
                              alias, sizeof(alias))) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_derive", "filename does not match YVEX artifact naming grammar");
        return YVEX_ERR_FORMAT;
    }
    if (yvex_model_alias_validate(alias, err) != YVEX_OK) return yvex_error_code(err);
    snprintf(path_copy, sizeof(path_copy), "%s", path);
    memset(entry, 0, sizeof(*entry));
    entry->alias = alias;
    entry->family = family;
    entry->model = model;
    entry->scope = scope;
    entry->artifact_class = artifact_class;
    entry->qprofile = qprofile;
    entry->calibration = calibration;
    entry->producer = producer;
    entry->schema_version = schema;
    entry->path = path_copy;
    entry->sha256 = "";
    entry->support_level = "";
    entry->execution_ready = 0;
    return YVEX_OK;
}
