/*
 * YVEX - Source manifest JSON writer
 *
 * File: src/tools/source_manifest_json.c
 * Layer: tool-plane implementation
 *
 * Purpose:
 *   Emits the open-weight intake source manifest JSON contract. The writer records
 *   provenance and local file summaries only; it does not hash, parse, or copy
 *   model bytes.
 */
#include "source_manifest_internal.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static void yvex_sm_json_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        switch (ch) {
        case '"':
            fputs("\\\"", fp);
            break;
        case '\\':
            fputs("\\\\", fp);
            break;
        case '\b':
            fputs("\\b", fp);
            break;
        case '\f':
            fputs("\\f", fp);
            break;
        case '\n':
            fputs("\\n", fp);
            break;
        case '\r':
            fputs("\\r", fp);
            break;
        case '\t':
            fputs("\\t", fp);
            break;
        default:
            if (ch < 32) {
                fprintf(fp, "\\u%04x", (unsigned int)ch);
            } else {
                fputc((int)ch, fp);
            }
            break;
        }
    }
    fputc('"', fp);
}

static void yvex_sm_json_field(FILE *fp, const char *name, const char *value, int comma)
{
    fprintf(fp, "    ");
    yvex_sm_json_string(fp, name);
    fputs(": ", fp);
    yvex_sm_json_string(fp, value);
    fputs(comma ? ",\n" : "\n", fp);
}

int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err)
{
    FILE *fp;
    size_t i;

    if (!out_path || !options || !files) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_json", "out_path, options, and files are required");
        return YVEX_ERR_INVALID_ARG;
    }

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "cannot open output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }

    fprintf(fp, "{\n");
    yvex_sm_json_field(fp, "schema", "yvex.source_manifest.v1", 1);
    yvex_sm_json_field(fp, "status", yvex_source_status_name(options->status), 1);
    fprintf(fp, "  \"source\": {\n");
    yvex_sm_json_field(fp, "kind", "huggingface", 1);
    yvex_sm_json_field(fp, "repo", options->repo, 1);
    yvex_sm_json_field(fp, "revision", options->revision, 1);
    yvex_sm_json_field(fp, "license", options->license, 1);
    yvex_sm_json_field(fp, "model_card", options->model_card, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"local\": {\n");
    yvex_sm_json_field(fp, "path", options->local_path, 1);
    yvex_sm_json_field(fp, "node_role", "provider", 1);
    yvex_sm_json_field(fp, "node_name", options->node_name, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"download\": {\n");
    yvex_sm_json_field(fp, "command", options->download_command, 1);
    yvex_sm_json_field(fp, "dry_run_log", options->dry_run_log, 1);
    yvex_sm_json_field(fp, "download_log", options->download_log, 1);
    yvex_sm_json_field(fp, "pid_file", options->pid_file, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"files\": [\n");
    for (i = 0; i < files->count; ++i) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"path\": ");
        yvex_sm_json_string(fp, files->items[i].path);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"size_bytes\": %llu,\n", files->items[i].size_bytes);
        fprintf(fp, "      \"kind\": ");
        yvex_sm_json_string(fp, files->items[i].kind);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"sha256\": null\n");
        fprintf(fp, "    }%s\n", i + 1u == files->count ? "" : ",");
    }
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"summary\": {\n");
    fprintf(fp, "    \"file_count\": %llu,\n", files->summary.file_count);
    fprintf(fp, "    \"safetensors_count\": %llu,\n", files->summary.safetensors_count);
    fprintf(fp, "    \"total_size_bytes\": %llu,\n", files->summary.total_size_bytes);
    fprintf(fp, "    \"has_config\": %s,\n", files->summary.has_config ? "true" : "false");
    fprintf(fp, "    \"has_tokenizer\": %s,\n", files->summary.has_tokenizer ? "true" : "false");
    fprintf(fp, "    \"has_safetensors\": %s\n", files->summary.has_safetensors ? "true" : "false");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "source_manifest_json", "failed closing output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
