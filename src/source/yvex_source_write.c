/*
 * yvex_source_write.c - source manifest JSON file writer.
 *
 * Owner: src/source.
 * Owns: explicit source sidecar file writing.
 * Does not own: CLI/operator output, report rendering, runtime, generation, eval, or benchmark.
 * Invariants: writes only caller-selected local files and never chooses standard streams.
 * Boundary: writing source sidecars is not source verification, artifact emission, or release readiness.
 */
#include "yvex_source_write.h"
#include "yvex_source_private.h"
#include "yvex_json_writer.h"

#include <stdio.h>

int yvex_source_manifest_write_json(const char *out_path,
                                    const yvex_source_manifest_options *options,
                                    yvex_source_manifest_summary *summary_out,
                                    yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    if (!out_path || !options || !options->repo || !options->revision ||
        !options->local_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_write", "out_path, repo, revision, and local_path are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(options->local_path, options->include_files, &files, err);
    if (rc == YVEX_OK) {
        rc = yvex_source_manifest_write_json_file(out_path, options, &files, err);
    }
    if (rc == YVEX_OK && summary_out) {
        *summary_out = files.summary;
    }
    yvex_source_manifest_file_list_free(&files);
    return rc;
}

static void yvex_sm_json_field(FILE *fp, const char *name, const char *value, int comma)
{
    yvex_file_json_write_field(fp, "    ", name, value, comma);
}

/*
 * yvex_source_manifest_write_json_file()
 *
 * Purpose:
 *   write source manifest summary and file-list facts to JSON.
 *
 * Inputs:
 *   output path, manifest options, and file list are borrowed.
 *
 * Effects:
 *   opens a local JSON file, writes escaped metadata and footprint fields, and
 *   closes the file; it does not create artifacts or hash tensor payloads.
 *
 * Failure:
 *   returns invalid-arg or IO errors for missing inputs, open/write/close
 *   failures, and path issues.
 *
 * Boundary:
 *   writing a source manifest is not source verification, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
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
        yvex_file_json_write_string(fp, files->items[i].path);
        fprintf(fp, ",\n");
        fprintf(fp, "      \"size_bytes\": %llu,\n", files->items[i].size_bytes);
        fprintf(fp, "      \"kind\": ");
        yvex_file_json_write_string(fp, files->items[i].kind);
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
