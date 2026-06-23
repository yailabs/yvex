/*
 * YVEX - Local model registry JSON parser/writer
 */
#include "yvex_model_registry_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int read_file(const char *path, char **out, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot open registry: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot size registry: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_json", "read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot read registry: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    return YVEX_OK;
}

static char *extract_string_in(const char *start, const char *end, const char *key)
{
    char needle[128];
    const char *p;
    const char *colon;
    const char *s;
    char *out;
    size_t n = 0;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(start, needle);
    if (!p || (end && p >= end)) return yvex_model_registry_strdup("");
    colon = strchr(p, ':');
    if (!colon || (end && colon >= end)) return NULL;
    s = colon + 1;
    while ((!end || s < end) && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
    if (end && s >= end) return NULL;
    if (*s != '"') return NULL;
    s++;
    out = (char *)malloc((size_t)(end ? end - s : (long)strlen(s)) + 1u);
    if (!out) return NULL;
    while (*s && (!end || s < end)) {
        char ch = *s++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\' && *s && (!end || s < end)) {
            ch = *s++;
            if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else out[n++] = ch;
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    return NULL;
}

static int extract_bool_in(const char *start, const char *end, const char *key)
{
    char needle[128];
    const char *p;
    const char *colon;
    const char *s;

    snprintf(needle, sizeof(needle), "\"%s\"", key);
    p = strstr(start, needle);
    if (!p || (end && p >= end)) return 0;
    colon = strchr(p, ':');
    if (!colon || (end && colon >= end)) return 0;
    s = colon + 1;
    while ((!end || s < end) && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')) s++;
    return strncmp(s, "true", 4) == 0 ? 1 : 0;
}

static void free_entry_view_strings(yvex_model_registry_entry *view)
{
    if (!view) return;
    free((char *)view->alias);
    free((char *)view->family);
    free((char *)view->model);
    free((char *)view->scope);
    free((char *)view->artifact_class);
    free((char *)view->qprofile);
    free((char *)view->calibration);
    free((char *)view->producer);
    free((char *)view->schema_version);
    free((char *)view->path);
    free((char *)view->sha256);
    free((char *)view->support_level);
    memset(view, 0, sizeof(*view));
}

static const char *find_matching_object_end(const char *start)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    const char *p;

    for (p = start; *p; ++p) {
        if (in_string) {
            if (escape) escape = 0;
            else if (*p == '\\') escape = 1;
            else if (*p == '"') in_string = 0;
            continue;
        }
        if (*p == '"') in_string = 1;
        else if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) return p + 1;
        }
    }
    return NULL;
}

int yvex_model_registry_parse_json_file(const char *path,
                                        yvex_model_registry *registry,
                                        yvex_error *err)
{
    char *json = NULL;
    const char *models;
    const char *p;
    int rc;

    if (!path || !registry) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "path and registry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = read_file(path, &json, err);
    if (rc != YVEX_OK) return rc;
    if (!strstr(json, "\"schema\"") || !strstr(json, "yvex.models.local.v1")) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "registry schema missing or unsupported");
        return YVEX_ERR_FORMAT;
    }
    registry->selected = extract_string_in(json, NULL, "selected");
    if (!registry->selected) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "malformed selected field");
        return YVEX_ERR_FORMAT;
    }
    models = strstr(json, "\"models\"");
    if (!models) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "models array missing");
        return YVEX_ERR_FORMAT;
    }
    p = strchr(models, '[');
    if (!p) {
        free(json);
        yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "models array malformed");
        return YVEX_ERR_FORMAT;
    }
    p++;
    while (*p) {
        const char *obj_start;
        const char *obj_end;
        yvex_model_registry_entry view;
        yvex_model_registry_owned_entry owned;
        memset(&view, 0, sizeof(view));
        while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
        if (*p == ']') break;
        if (*p != '{') {
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry malformed");
            return YVEX_ERR_FORMAT;
        }
        obj_start = p;
        obj_end = find_matching_object_end(obj_start);
        if (!obj_end) {
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry unterminated");
            return YVEX_ERR_FORMAT;
        }
        view.alias = extract_string_in(obj_start, obj_end, "alias");
        view.family = extract_string_in(obj_start, obj_end, "family");
        view.model = extract_string_in(obj_start, obj_end, "model");
        view.scope = extract_string_in(obj_start, obj_end, "scope");
        view.artifact_class = extract_string_in(obj_start, obj_end, "artifact_class");
        view.qprofile = extract_string_in(obj_start, obj_end, "qprofile");
        view.calibration = extract_string_in(obj_start, obj_end, "calibration");
        view.producer = extract_string_in(obj_start, obj_end, "producer");
        view.schema_version = extract_string_in(obj_start, obj_end, "schema_version");
        view.path = extract_string_in(obj_start, obj_end, "path");
        view.sha256 = extract_string_in(obj_start, obj_end, "sha256");
        view.support_level = extract_string_in(obj_start, obj_end, "support_level");
        view.execution_ready = extract_bool_in(obj_start, obj_end, "execution_ready");
        if (!view.alias || !view.family || !view.model || !view.scope ||
            !view.artifact_class || !view.qprofile || !view.calibration ||
            !view.producer || !view.schema_version || !view.path ||
            !view.sha256 || !view.support_level) {
            free_entry_view_strings(&view);
            free(json);
            yvex_error_set(err, YVEX_ERR_FORMAT, "model_registry_json", "model entry has malformed string field");
            return YVEX_ERR_FORMAT;
        }
        rc = yvex_model_alias_validate(view.alias, err);
        if (rc != YVEX_OK) {
            free_entry_view_strings(&view);
            free(json);
            return rc;
        }
        rc = yvex_model_registry_copy_entry(&owned, &view, err);
        free_entry_view_strings(&view);
        if (rc != YVEX_OK) {
            free(json);
            return rc;
        }
        if (registry->count == registry->cap) {
            yvex_model_registry_owned_entry *next;
            unsigned long long cap = registry->cap ? registry->cap * 2u : 4u;
            next = (yvex_model_registry_owned_entry *)realloc(registry->entries, (size_t)cap * sizeof(*next));
            if (!next) {
                yvex_model_registry_owned_entry_clear(&owned);
                free(json);
                yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_json", "registry allocation failed");
                return YVEX_ERR_NOMEM;
            }
            memset(next + registry->cap, 0, (size_t)(cap - registry->cap) * sizeof(*next));
            registry->entries = next;
            registry->cap = cap;
        }
        registry->entries[registry->count++] = owned;
        p = obj_end;
    }
    free(json);
    return YVEX_OK;
}

static void write_escaped(FILE *fp, const char *s)
{
    if (!s) s = "";
    fputc('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static void write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": ", key);
    write_escaped(fp, value);
    fprintf(fp, "%s\n", comma ? "," : "");
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
                yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot create directory: %s", buf);
                return YVEX_ERR_IO;
            }
            *p = '/';
        }
    }
    if (mkdir(buf, 0775) != 0 && errno != EEXIST) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot create directory: %s", buf);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                        const char *path,
                                        yvex_error *err)
{
    char tmp[4096];
    FILE *fp;
    unsigned long long i;
    int n;
    int rc;

    if (!registry || !path || !path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_json", "registry and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_model_registry_mkdir_parent(path, err);
    if (rc != YVEX_OK) return rc;
    n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(tmp)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "model_registry_json", "temporary path too long");
        return YVEX_ERR_BOUNDS;
    }
    fp = fopen(tmp, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot write registry: %s", tmp);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    write_field(fp, "  ", "schema", "yvex.models.local.v1", 1);
    write_field(fp, "  ", "selected", registry->selected ? registry->selected : "", 1);
    fprintf(fp, "  \"models\": [\n");
    for (i = 0; i < registry->count; ++i) {
        const yvex_model_registry_owned_entry *e = &registry->entries[i];
        fprintf(fp, "    {\n");
        write_field(fp, "      ", "alias", e->alias, 1);
        write_field(fp, "      ", "family", e->family, 1);
        write_field(fp, "      ", "model", e->model, 1);
        write_field(fp, "      ", "scope", e->scope, 1);
        write_field(fp, "      ", "artifact_class", e->artifact_class, 1);
        write_field(fp, "      ", "qprofile", e->qprofile, 1);
        write_field(fp, "      ", "calibration", e->calibration, 1);
        write_field(fp, "      ", "producer", e->producer, 1);
        write_field(fp, "      ", "schema_version", e->schema_version, 1);
        write_field(fp, "      ", "path", e->path, 1);
        write_field(fp, "      ", "sha256", e->sha256, 1);
        write_field(fp, "      ", "support_level", e->support_level, 1);
        fprintf(fp, "      \"execution_ready\": %s\n", e->execution_ready ? "true" : "false");
        fprintf(fp, "    }%s\n", (i + 1u < registry->count) ? "," : "");
    }
    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    if (fflush(fp) != 0 || fclose(fp) != 0) {
        remove(tmp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot close registry: %s", tmp);
        return YVEX_ERR_IO;
    }
    if (rename(tmp, path) != 0) {
        remove(tmp);
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_json", "cannot replace registry: %s", path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
