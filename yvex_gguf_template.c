/*
 * YVEX - GGUF template contract implementation
 *
 * File: yvex_gguf_template.c
 * Layer: tool-plane implementation
 */
#include "yvex_gguf_template_internal.h"

#include <stdlib.h>
#include <string.h>

static char *gt_strdup(const char *s)
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

static int gt_open_artifact(const char *path, yvex_artifact **artifact, yvex_error *err)
{
    yvex_artifact_options options;

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;
    options.map = 1;
    return yvex_artifact_open(artifact, &options, err);
}

const char *yvex_gguf_template_status_name(yvex_gguf_template_status status)
{
    switch (status) {
    case YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN: return "template-unknown";
    case YVEX_GGUF_TEMPLATE_STATUS_VALID: return "template-valid";
    case YVEX_GGUF_TEMPLATE_STATUS_PARTIAL: return "template-partial";
    case YVEX_GGUF_TEMPLATE_STATUS_INVALID: return "template-invalid";
    }
    return "template-unknown";
}

const char *yvex_gguf_template_issue_kind_name(yvex_gguf_template_issue_kind kind)
{
    switch (kind) {
    case YVEX_GGUF_TEMPLATE_ISSUE_NONE: return "none";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE: return "missing_architecture";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME: return "missing_model_name";
    case YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER: return "missing_tokenizer";
    case YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY: return "empty_tensor_directory";
    case YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE: return "unknown_tensor_role";
    case YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR: return "native_missing_tensor";
    case YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH: return "native_shape_mismatch";
    case YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE: return "unsupported_dtype";
    case YVEX_GGUF_TEMPLATE_ISSUE_FORMAT: return "format";
    }
    return "format";
}

int yvex_gguf_template_add_issue(yvex_gguf_template *tmpl,
                                 yvex_gguf_template_issue_kind kind,
                                 const char *tensor_name,
                                 const char *message,
                                 yvex_error *err)
{
    yvex_gguf_template_issue *next;
    yvex_gguf_template_issue *issue;

    if (!tmpl) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_issue", "template is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tmpl->issue_count == tmpl->issue_cap) {
        unsigned long long cap = tmpl->issue_cap == 0 ? 8u : tmpl->issue_cap * 2u;
        next = (yvex_gguf_template_issue *)realloc(tmpl->issues, (size_t)cap * sizeof(tmpl->issues[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_issue", "issue allocation failed");
            return YVEX_ERR_NOMEM;
        }
        tmpl->issues = next;
        tmpl->issue_cap = cap;
    }
    issue = &tmpl->issues[tmpl->issue_count];
    memset(issue, 0, sizeof(*issue));
    issue->kind = kind;
    issue->tensor_name = gt_strdup(tensor_name);
    issue->message = gt_strdup(message);
    if (!issue->tensor_name || !issue->message) {
        free((char *)issue->tensor_name);
        free((char *)issue->message);
        memset(issue, 0, sizeof(*issue));
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_issue", "issue string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    tmpl->issue_count++;
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}

int yvex_gguf_template_open(yvex_gguf_template **out,
                            const yvex_gguf_template_options *options,
                            yvex_error *err)
{
    yvex_gguf_template *tmpl;
    int rc;

    if (!out || !options || !options->template_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_open", "out and template_path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    tmpl = (yvex_gguf_template *)calloc(1, sizeof(*tmpl));
    if (!tmpl) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "gguf_template_open", "template allocation failed");
        return YVEX_ERR_NOMEM;
    }
    tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN;

    rc = gt_open_artifact(options->template_path, &tmpl->artifact, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&tmpl->gguf, tmpl->artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tmpl->tensors, tmpl->gguf, err);
    if (rc == YVEX_OK) rc = yvex_model_descriptor_from_gguf(&tmpl->model, tmpl->gguf, tmpl->tensors, err);
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    rc = yvex_gguf_template_validate(tmpl, options, err);
    if (rc == YVEX_OK && options->compare_native) {
        rc = yvex_gguf_template_compare_native(tmpl, options, err);
    }
    if (rc != YVEX_OK) {
        yvex_gguf_template_close(tmpl);
        return rc;
    }
    *out = tmpl;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_gguf_template_close(yvex_gguf_template *tmpl)
{
    unsigned long long i;

    if (!tmpl) return;
    for (i = 0; i < tmpl->issue_count; ++i) {
        free((char *)tmpl->issues[i].tensor_name);
        free((char *)tmpl->issues[i].message);
    }
    free(tmpl->issues);
    free(tmpl->architecture);
    free(tmpl->model_name);
    yvex_native_weight_table_close(tmpl->native);
    yvex_tokenizer_close(tmpl->tokenizer);
    yvex_model_descriptor_close(tmpl->model);
    yvex_tensor_table_close(tmpl->tensors);
    yvex_gguf_close(tmpl->gguf);
    yvex_artifact_close(tmpl->artifact);
    free(tmpl);
}

int yvex_gguf_template_get_summary(const yvex_gguf_template *tmpl,
                                   yvex_gguf_template_summary *out,
                                   yvex_error *err)
{
    if (!tmpl || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_summary", "template and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = tmpl->summary;
    return YVEX_OK;
}

unsigned long long yvex_gguf_template_issue_count(const yvex_gguf_template *tmpl)
{
    return tmpl ? tmpl->issue_count : 0;
}

const yvex_gguf_template_issue *yvex_gguf_template_issue_at(const yvex_gguf_template *tmpl,
                                                            unsigned long long index)
{
    if (!tmpl || index >= tmpl->issue_count) return NULL;
    return &tmpl->issues[index];
}
