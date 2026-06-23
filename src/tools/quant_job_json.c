/*
 * YVEX - Quant job JSON parser/writer
 *
 * File: src/tools/quant_job_json.c
 * Layer: tool-plane implementation
 */
#include "quant_job_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int qj_read_file(const char *path, char **out, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot open manifest: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot size manifest: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_job_json", "read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot read manifest: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    return YVEX_OK;
}

static char *qj_extract_string(const char *json, const char *key)
{
    const char *p;
    const char *colon;
    const char *s;
    char *needle;
    char *out;
    size_t key_len = strlen(key);
    size_t n = 0;

    needle = (char *)malloc(key_len + 4u);
    if (!needle) return NULL;
    sprintf(needle, "\"%s\"", key);
    p = strstr(json, needle);
    free(needle);
    if (!p) return yvex_quant_job_strdup("");
    colon = strchr(p, ':');
    if (!colon) return NULL;
    s = colon + 1;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (*s != '"') return NULL;
    s++;
    out = (char *)malloc(strlen(s) + 1u);
    if (!out) return NULL;
    while (*s) {
        char ch = *s++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\' && *s) {
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

int yvex_quant_job_parse_json_file(const char *path,
                                   yvex_quant_job_doc *doc,
                                   yvex_error *err)
{
    char *json = NULL;
    char *status = NULL;
    char *tool = NULL;
    int rc;

    if (!path || !doc) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_json", "path and doc are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(doc, 0, sizeof(*doc));
    rc = qj_read_file(path, &json, err);
    if (rc != YVEX_OK) return rc;
    doc->name = qj_extract_string(json, "name");
    doc->architecture = qj_extract_string(json, "architecture");
    doc->tool_path = qj_extract_string(json, "path");
    doc->source_manifest_path = qj_extract_string(json, "source_manifest");
    doc->native_source_dir = qj_extract_string(json, "native_source");
    doc->template_path = qj_extract_string(json, "template");
    doc->quant_policy_path = qj_extract_string(json, "quant_policy");
    doc->imatrix_manifest_path = qj_extract_string(json, "imatrix_manifest");
    doc->imatrix_path = qj_extract_string(json, "imatrix");
    doc->out_gguf_path = qj_extract_string(json, "gguf");
    doc->log_path = qj_extract_string(json, "log");
    doc->command = qj_extract_string(json, "command");
    status = qj_extract_string(json, "status");
    tool = qj_extract_string(json, "kind");
    if (!doc->name || !doc->architecture || !doc->tool_path ||
        !doc->source_manifest_path || !doc->native_source_dir ||
        !doc->template_path || !doc->quant_policy_path ||
        !doc->imatrix_manifest_path || !doc->imatrix_path ||
        !doc->out_gguf_path || !doc->log_path || !doc->command ||
        !status || !tool) {
        free(json);
        free(status);
        free(tool);
        yvex_quant_job_doc_clear(doc);
        yvex_error_set(err, YVEX_ERR_FORMAT, "quant_job_json", "malformed quant job manifest");
        return YVEX_ERR_FORMAT;
    }
    doc->status = yvex_quant_job_status_from_name(status);
    doc->tool = yvex_quant_job_tool_from_name(tool);
    free(json);
    free(status);
    free(tool);
    return YVEX_OK;
}

static void qj_write_escaped(FILE *fp, const char *s)
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

static void qj_write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": ", key);
    qj_write_escaped(fp, value);
    fprintf(fp, "%s\n", comma ? "," : "");
}

int yvex_quant_job_write_json_file(const char *out_path,
                                   const yvex_quant_job_options *options,
                                   yvex_error *err)
{
    FILE *fp;

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot write manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    qj_write_field(fp, "  ", "schema", "yvex.quant_job.v1", 1);
    qj_write_field(fp, "  ", "name", options->name, 1);
    qj_write_field(fp, "  ", "architecture", options->architecture, 1);
    qj_write_field(fp, "  ", "status", yvex_quant_job_status_name(options->status), 1);
    fprintf(fp, "  \"tool\": {\n");
    qj_write_field(fp, "    ", "kind", yvex_quant_job_tool_name(options->tool), 1);
    qj_write_field(fp, "    ", "path", options->tool_path, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"inputs\": {\n");
    qj_write_field(fp, "    ", "source_manifest", options->source_manifest_path, 1);
    qj_write_field(fp, "    ", "native_source", options->native_source_dir, 1);
    qj_write_field(fp, "    ", "template", options->template_path, 1);
    qj_write_field(fp, "    ", "quant_policy", options->quant_policy_path, 1);
    qj_write_field(fp, "    ", "imatrix_manifest", options->imatrix_manifest_path, 1);
    qj_write_field(fp, "    ", "imatrix", options->imatrix_path, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"outputs\": {\n");
    qj_write_field(fp, "    ", "gguf", options->out_gguf_path, 1);
    qj_write_field(fp, "    ", "log", options->log_path, 0);
    fprintf(fp, "  },\n");
    qj_write_field(fp, "  ", "command", options->command, 0);
    fprintf(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot close manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
