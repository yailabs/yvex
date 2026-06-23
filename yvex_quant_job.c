/*
 * YVEX - External quantization job manifest implementation
 *
 * File: yvex_quant_job.c
 * Layer: tool-plane implementation
 */
#include "yvex_quant_job_internal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static yvex_quant_job_doc qj_last_summary_doc;

static void qj_store_summary_doc(yvex_quant_job_doc *doc)
{
    yvex_quant_job_doc_clear(&qj_last_summary_doc);
    qj_last_summary_doc = *doc;
    memset(doc, 0, sizeof(*doc));
}

char *yvex_quant_job_strdup(const char *s)
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

void yvex_quant_job_doc_clear(yvex_quant_job_doc *doc)
{
    if (!doc) return;
    free(doc->name);
    free(doc->architecture);
    free(doc->tool_path);
    free(doc->source_manifest_path);
    free(doc->native_source_dir);
    free(doc->template_path);
    free(doc->quant_policy_path);
    free(doc->imatrix_manifest_path);
    free(doc->imatrix_path);
    free(doc->out_gguf_path);
    free(doc->log_path);
    free(doc->command);
    memset(doc, 0, sizeof(*doc));
}

const char *yvex_quant_job_status_name(yvex_quant_job_status status)
{
    switch (status) {
    case YVEX_QUANT_JOB_STATUS_UNKNOWN: return "unknown";
    case YVEX_QUANT_JOB_STATUS_DECLARED: return "declared";
    case YVEX_QUANT_JOB_STATUS_READY: return "ready";
    case YVEX_QUANT_JOB_STATUS_RUNNING: return "running";
    case YVEX_QUANT_JOB_STATUS_SUCCEEDED: return "succeeded";
    case YVEX_QUANT_JOB_STATUS_FAILED: return "failed";
    case YVEX_QUANT_JOB_STATUS_SKIPPED: return "skipped";
    }
    return "unknown";
}

yvex_quant_job_status yvex_quant_job_status_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_JOB_STATUS_UNKNOWN;
    if (strcmp(name, "declared") == 0) return YVEX_QUANT_JOB_STATUS_DECLARED;
    if (strcmp(name, "ready") == 0) return YVEX_QUANT_JOB_STATUS_READY;
    if (strcmp(name, "running") == 0) return YVEX_QUANT_JOB_STATUS_RUNNING;
    if (strcmp(name, "succeeded") == 0) return YVEX_QUANT_JOB_STATUS_SUCCEEDED;
    if (strcmp(name, "failed") == 0) return YVEX_QUANT_JOB_STATUS_FAILED;
    if (strcmp(name, "skipped") == 0) return YVEX_QUANT_JOB_STATUS_SKIPPED;
    return YVEX_QUANT_JOB_STATUS_UNKNOWN;
}

const char *yvex_quant_job_tool_name(yvex_quant_job_tool tool)
{
    switch (tool) {
    case YVEX_QUANT_JOB_TOOL_UNKNOWN: return "unknown";
    case YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL: return "yvex-internal";
    case YVEX_QUANT_JOB_TOOL_EXTERNAL: return "external";
    }
    return "unknown";
}

yvex_quant_job_tool yvex_quant_job_tool_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_JOB_TOOL_UNKNOWN;
    if (strcmp(name, "unknown") == 0) return YVEX_QUANT_JOB_TOOL_UNKNOWN;
    if (strcmp(name, "yvex-internal") == 0) return YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL;
    if (strcmp(name, "external") == 0) return YVEX_QUANT_JOB_TOOL_EXTERNAL;
    return YVEX_QUANT_JOB_TOOL_UNKNOWN;
}

void yvex_quant_job_summarize(const yvex_quant_job_doc *doc,
                              yvex_quant_job_summary *summary)
{
    if (!doc || !summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->status = doc->status;
    summary->tool = doc->tool;
    summary->name = doc->name;
    summary->architecture = doc->architecture;
    summary->tool_path = doc->tool_path;
    summary->native_source_dir = doc->native_source_dir;
    summary->template_path = doc->template_path;
    summary->out_gguf_path = doc->out_gguf_path;
    summary->log_path = doc->log_path;
    summary->tool_exists = doc->tool_path && doc->tool_path[0] && access(doc->tool_path, X_OK) == 0;
    summary->source_exists = doc->native_source_dir && doc->native_source_dir[0] && access(doc->native_source_dir, F_OK) == 0;
    summary->template_exists = doc->template_path && doc->template_path[0] && access(doc->template_path, F_OK) == 0;
    summary->imatrix_exists = doc->imatrix_path && doc->imatrix_path[0] && access(doc->imatrix_path, F_OK) == 0;
    summary->output_exists = doc->out_gguf_path && doc->out_gguf_path[0] && access(doc->out_gguf_path, F_OK) == 0;
}

static int qj_options_to_doc(const yvex_quant_job_options *options,
                             yvex_quant_job_doc *doc,
                             yvex_error *err)
{
    memset(doc, 0, sizeof(*doc));
    if (!options || !options->name || !options->architecture ||
        !options->tool_path || !options->native_source_dir ||
        !options->template_path || !options->out_gguf_path ||
        !options->log_path || !options->command) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "name, architecture, tool path, native source, template, output GGUF, log, and command are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "known status is required");
        return YVEX_ERR_INVALID_ARG;
    }
    doc->name = yvex_quant_job_strdup(options->name);
    doc->architecture = yvex_quant_job_strdup(options->architecture);
    doc->tool_path = yvex_quant_job_strdup(options->tool_path);
    doc->source_manifest_path = yvex_quant_job_strdup(options->source_manifest_path);
    doc->native_source_dir = yvex_quant_job_strdup(options->native_source_dir);
    doc->template_path = yvex_quant_job_strdup(options->template_path);
    doc->quant_policy_path = yvex_quant_job_strdup(options->quant_policy_path);
    doc->imatrix_manifest_path = yvex_quant_job_strdup(options->imatrix_manifest_path);
    doc->imatrix_path = yvex_quant_job_strdup(options->imatrix_path);
    doc->out_gguf_path = yvex_quant_job_strdup(options->out_gguf_path);
    doc->log_path = yvex_quant_job_strdup(options->log_path);
    doc->command = yvex_quant_job_strdup(options->command);
    doc->tool = options->tool;
    doc->status = options->status;
    if (!doc->name || !doc->architecture || !doc->tool_path ||
        !doc->source_manifest_path || !doc->native_source_dir ||
        !doc->template_path || !doc->quant_policy_path ||
        !doc->imatrix_manifest_path || !doc->imatrix_path ||
        !doc->out_gguf_path || !doc->log_path || !doc->command) {
        yvex_quant_job_doc_clear(doc);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_job", "manifest string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_quant_job_write_json(const char *out_path,
                              const yvex_quant_job_options *options,
                              yvex_quant_job_summary *summary_out,
                              yvex_error *err)
{
    yvex_quant_job_doc doc;
    int rc;

    if (!out_path || !out_path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = qj_options_to_doc(options, &doc, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_quant_job_write_json_file(out_path, options, err);
    if (rc == YVEX_OK) {
        qj_store_summary_doc(&doc);
        if (summary_out) yvex_quant_job_summarize(&qj_last_summary_doc, summary_out);
    } else {
        yvex_quant_job_doc_clear(&doc);
    }
    return rc;
}

int yvex_quant_job_validate(const char *manifest_path,
                            yvex_quant_job_summary *summary_out,
                            yvex_error *err)
{
    yvex_quant_job_doc doc;
    yvex_quant_job_summary summary;
    int rc;

    if (!manifest_path || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_validate", "manifest path and summary are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&doc, 0, sizeof(doc));
    rc = yvex_quant_job_parse_json_file(manifest_path, &doc, err);
    if (rc != YVEX_OK) return rc;
    yvex_quant_job_summarize(&doc, &summary);
    *summary_out = summary;
    if (!summary.name || !summary.name[0] || !summary.architecture || !summary.architecture[0] ||
        doc.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        yvex_quant_job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_FORMAT, "quant_job_validate", "manifest is missing required identity fields");
        return YVEX_ERR_FORMAT;
    }
    if (doc.status == YVEX_QUANT_JOB_STATUS_SUCCEEDED && !summary.output_exists) {
        yvex_quant_job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_STATE, "quant_job_validate", "succeeded quant job requires output GGUF to exist");
        return YVEX_ERR_STATE;
    }
    qj_store_summary_doc(&doc);
    yvex_quant_job_summarize(&qj_last_summary_doc, summary_out);
    return YVEX_OK;
}
