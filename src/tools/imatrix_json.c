/*
 * YVEX - Imatrix manifest JSON parser/writer
 *
 * File: src/tools/imatrix_json.c
 * Layer: tool-plane implementation
 */
#include "imatrix_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *p;
    const char *end;
    const char *path;
    yvex_error *err;
} im_json;

static void ij_skip_ws(im_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}

static int ij_fail(im_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "imatrix_json", "%s in %s", msg, j->path);
    return YVEX_ERR_FORMAT;
}

static int ij_expect(im_json *j, char ch)
{
    ij_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) return ij_fail(j, "unexpected JSON token");
    j->p++;
    return YVEX_OK;
}

static char *ij_string(im_json *j)
{
    char *out;
    size_t cap;
    size_t n = 0;

    ij_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        ij_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    cap = (size_t)(j->end - j->p) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "imatrix_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                ij_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') out[n++] = ch;
            else if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else {
                free(out);
                ij_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    ij_fail(j, "unterminated string");
    return NULL;
}

static int ij_skip_value(im_json *j);

static int ij_skip_literal(im_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) return ij_fail(j, "unexpected literal");
    j->p += n;
    return YVEX_OK;
}

static int ij_skip_object(im_json *j)
{
    int rc = ij_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = ij_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = ij_skip_value(j);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed object");
    }
    return ij_fail(j, "unterminated object");
}

static int ij_skip_array(im_json *j)
{
    int rc = ij_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = ij_skip_value(j);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed array");
    }
    return ij_fail(j, "unterminated array");
}

static int ij_skip_value(im_json *j)
{
    char *s;
    ij_skip_ws(j);
    if (j->p >= j->end) return ij_fail(j, "expected value");
    if (*j->p == '{') return ij_skip_object(j);
    if (*j->p == '[') return ij_skip_array(j);
    if (*j->p == '"') {
        s = ij_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (*j->p == 't') return ij_skip_literal(j, "true");
    if (*j->p == 'f') return ij_skip_literal(j, "false");
    if (*j->p == 'n') return ij_skip_literal(j, "null");
    while (j->p < j->end && (isdigit((unsigned char)*j->p) || *j->p == '-')) j->p++;
    return YVEX_OK;
}

static int ij_read_file(const char *path, char **out, unsigned long long *len, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot open manifest: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot size manifest: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot read manifest: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    *len = (unsigned long long)size;
    return YVEX_OK;
}

static int ij_parse_named_object(im_json *j, yvex_imatrix_manifest *manifest, const char *object_name)
{
    int rc = ij_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        key = ij_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            return rc;
        }
        if (strcmp(object_name, "imatrix") == 0 && strcmp(key, "path") == 0) {
            free(manifest->imatrix_path);
            manifest->imatrix_path = ij_string(j);
            if (!manifest->imatrix_path) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "dataset") == 0) {
            free(manifest->calibration_dataset);
            manifest->calibration_dataset = ij_string(j);
            if (!manifest->calibration_dataset) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "command") == 0) {
            free(manifest->calibration_command);
            manifest->calibration_command = ij_string(j);
            if (!manifest->calibration_command) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "producer") == 0) {
            free(manifest->producer);
            manifest->producer = ij_string(j);
            if (!manifest->producer) rc = yvex_error_code(j->err);
        } else {
            rc = ij_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    return ij_fail(j, "unterminated nested object");
}

static int ij_parse_coverage_row(im_json *j, yvex_imatrix_manifest *manifest)
{
    char *kind = NULL;
    char *selector = NULL;
    char *purpose = NULL;
    int rc = ij_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            break;
        }
        key = ij_string(j);
        if (!key) {
            rc = yvex_error_code(j->err);
            goto done;
        }
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto done;
        }
        if (strcmp(key, "kind") == 0) kind = ij_string(j);
        else if (strcmp(key, "selector") == 0) selector = ij_string(j);
        else if (strcmp(key, "purpose") == 0) purpose = ij_string(j);
        else rc = ij_skip_value(j);
        free(key);
        if (rc != YVEX_OK) goto done;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    rc = yvex_imatrix_manifest_add_coverage(manifest,
                                            yvex_imatrix_coverage_kind_from_name(kind),
                                            selector ? selector : "",
                                            purpose ? purpose : "",
                                            j->err);
done:
    free(kind);
    free(selector);
    free(purpose);
    return rc;
}

static int ij_parse_coverage(im_json *j, yvex_imatrix_manifest *manifest)
{
    int rc = ij_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = ij_parse_coverage_row(j, manifest);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed coverage array");
    }
    return ij_fail(j, "unterminated coverage array");
}

int yvex_imatrix_manifest_parse_json(yvex_imatrix_manifest **out,
                                     const char *path,
                                     yvex_error *err)
{
    yvex_imatrix_manifest *manifest;
    im_json j;
    char *buf = NULL;
    unsigned long long len = 0;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = ij_read_file(path, &buf, &len, err);
    if (rc != YVEX_OK) return rc;
    manifest = (yvex_imatrix_manifest *)calloc(1, sizeof(*manifest));
    if (!manifest) {
        free(buf);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest allocation failed");
        return YVEX_ERR_NOMEM;
    }
    j.p = buf;
    j.end = buf + len;
    j.path = path;
    j.err = err;
    rc = ij_expect(&j, '{');
    if (rc != YVEX_OK) goto fail;
    while (j.p < j.end) {
        char *key;
        ij_skip_ws(&j);
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            break;
        }
        key = ij_string(&j);
        if (!key) {
            rc = yvex_error_code(err);
            goto fail;
        }
        rc = ij_expect(&j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto fail;
        }
        if (strcmp(key, "schema") == 0) {
            char *schema = ij_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.imatrix_manifest.v1") != 0) {
                free(schema);
                free(key);
                rc = ij_fail(&j, "unsupported imatrix schema");
                goto fail;
            }
            free(schema);
        } else if (strcmp(key, "name") == 0) {
            free(manifest->name);
            manifest->name = ij_string(&j);
            if (!manifest->name) rc = yvex_error_code(err);
        } else if (strcmp(key, "architecture") == 0) {
            free(manifest->architecture);
            manifest->architecture = ij_string(&j);
            if (!manifest->architecture) rc = yvex_error_code(err);
        } else if (strcmp(key, "status") == 0) {
            char *status = ij_string(&j);
            if (!status) rc = yvex_error_code(err);
            else {
                manifest->declared_status = yvex_imatrix_status_from_name(status);
                free(status);
            }
        } else if (strcmp(key, "format") == 0) {
            char *format = ij_string(&j);
            if (!format) rc = yvex_error_code(err);
            else {
                manifest->format = yvex_imatrix_format_from_name(format);
                free(format);
            }
        } else if (strcmp(key, "source_manifest") == 0) {
            free(manifest->source_manifest_path);
            manifest->source_manifest_path = ij_string(&j);
            if (!manifest->source_manifest_path) rc = yvex_error_code(err);
        } else if (strcmp(key, "quant_policy") == 0) {
            free(manifest->quant_policy_path);
            manifest->quant_policy_path = ij_string(&j);
            if (!manifest->quant_policy_path) rc = yvex_error_code(err);
        } else if (strcmp(key, "imatrix") == 0) {
            rc = ij_parse_named_object(&j, manifest, "imatrix");
        } else if (strcmp(key, "calibration") == 0) {
            rc = ij_parse_named_object(&j, manifest, "calibration");
        } else if (strcmp(key, "coverage") == 0) {
            rc = ij_parse_coverage(&j, manifest);
        } else {
            rc = ij_skip_value(&j);
        }
        free(key);
        if (rc != YVEX_OK) goto fail;
        ij_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') j.p++;
    }
    if (!manifest->name) manifest->name = yvex_imatrix_strdup("unnamed-imatrix");
    if (!manifest->architecture) manifest->architecture = yvex_imatrix_strdup("unknown");
    if (!manifest->source_manifest_path) manifest->source_manifest_path = yvex_imatrix_strdup("");
    if (!manifest->quant_policy_path) manifest->quant_policy_path = yvex_imatrix_strdup("");
    if (!manifest->imatrix_path) manifest->imatrix_path = yvex_imatrix_strdup("");
    if (!manifest->calibration_dataset) manifest->calibration_dataset = yvex_imatrix_strdup("");
    if (!manifest->calibration_command) manifest->calibration_command = yvex_imatrix_strdup("");
    if (!manifest->producer) manifest->producer = yvex_imatrix_strdup("");
    if (!manifest->name || !manifest->architecture || !manifest->source_manifest_path ||
        !manifest->quant_policy_path || !manifest->imatrix_path ||
        !manifest->calibration_dataset || !manifest->calibration_command || !manifest->producer) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest string allocation failed");
        goto fail;
    }
    yvex_imatrix_manifest_refresh_summary(manifest, &manifest->summary);
    *out = manifest;
    free(buf);
    return YVEX_OK;
fail:
    yvex_imatrix_manifest_close(manifest);
    free(buf);
    return rc;
}

static void ij_write_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') fputs("\\n", fp);
        else if (ch == '\r') fputs("\\r", fp);
        else if (ch == '\t') fputs("\\t", fp);
        else if (ch < 32) fprintf(fp, "\\u%04x", (unsigned int)ch);
        else fputc((int)ch, fp);
    }
    fputc('"', fp);
}

int yvex_imatrix_manifest_write_json_file(const char *out_path,
                                          const yvex_imatrix_manifest *manifest,
                                          yvex_error *err)
{
    FILE *fp;
    unsigned long long i;

    if (!out_path || !manifest) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json", "out_path and manifest are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot open output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.imatrix_manifest.v1\",\n");
    fprintf(fp, "  \"name\": "); ij_write_string(fp, manifest->name); fprintf(fp, ",\n");
    fprintf(fp, "  \"architecture\": "); ij_write_string(fp, manifest->architecture); fprintf(fp, ",\n");
    fprintf(fp, "  \"status\": "); ij_write_string(fp, yvex_imatrix_status_name(manifest->declared_status)); fprintf(fp, ",\n");
    fprintf(fp, "  \"format\": "); ij_write_string(fp, yvex_imatrix_format_name(manifest->format)); fprintf(fp, ",\n");
    fprintf(fp, "  \"source_manifest\": "); ij_write_string(fp, manifest->source_manifest_path); fprintf(fp, ",\n");
    fprintf(fp, "  \"quant_policy\": "); ij_write_string(fp, manifest->quant_policy_path); fprintf(fp, ",\n");
    fprintf(fp, "  \"imatrix\": {\n");
    fprintf(fp, "    \"path\": "); ij_write_string(fp, manifest->imatrix_path); fprintf(fp, ",\n");
    fprintf(fp, "    \"external\": true\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"calibration\": {\n");
    fprintf(fp, "    \"dataset\": "); ij_write_string(fp, manifest->calibration_dataset); fprintf(fp, ",\n");
    fprintf(fp, "    \"command\": "); ij_write_string(fp, manifest->calibration_command); fprintf(fp, ",\n");
    fprintf(fp, "    \"producer\": "); ij_write_string(fp, manifest->producer); fprintf(fp, "\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"coverage\": [\n");
    for (i = 0; i < manifest->coverage_count; ++i) {
        const yvex_imatrix_coverage *row = &manifest->coverage[i];
        fprintf(fp, "    {\"kind\": ");
        ij_write_string(fp, yvex_imatrix_coverage_kind_name(row->kind));
        fprintf(fp, ", \"selector\": ");
        ij_write_string(fp, row->selector);
        fprintf(fp, ", \"purpose\": ");
        ij_write_string(fp, row->purpose);
        fprintf(fp, "}%s\n", i + 1u == manifest->coverage_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "failed closing output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
