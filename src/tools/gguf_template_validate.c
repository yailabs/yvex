/*
 * YVEX - GGUF template validation
 *
 * File: src/tools/gguf_template_validate.c
 * Layer: tool-plane implementation
 */
#include "gguf_template_internal.h"

#include <stdlib.h>
#include <string.h>

static char *gt_copy_string_value(const yvex_gguf *gguf, const char *key)
{
    const yvex_gguf_value *value = yvex_gguf_metadata_find(gguf, key);
    const char *data;
    unsigned long long len;
    char *out;

    if (!value || yvex_gguf_value_as_string(value, &data, &len) != YVEX_OK) {
        return NULL;
    }
    out = (char *)malloc((size_t)len + 1u);
    if (!out) return NULL;
    memcpy(out, data, (size_t)len);
    out[len] = '\0';
    return out;
}

static unsigned long long gt_tokenizer_metadata_count(const yvex_gguf *gguf)
{
    unsigned long long i;
    unsigned long long n = 0;

    for (i = 0; i < yvex_gguf_metadata_count(gguf); ++i) {
        const char *key = yvex_gguf_metadata_key(gguf, i);
        if (key && strncmp(key, "tokenizer.", 10) == 0) {
            n++;
        }
    }
    return n;
}

int yvex_gguf_template_validate(yvex_gguf_template *tmpl,
                                const yvex_gguf_template_options *options,
                                yvex_error *err)
{
    unsigned long long i;
    int fatal = 0;
    int partial = 0;
    int rc;

    tmpl->summary.metadata_count = yvex_gguf_metadata_count(tmpl->gguf);
    tmpl->summary.tensor_count = yvex_tensor_table_count(tmpl->tensors);
    tmpl->summary.has_tensor_directory = tmpl->summary.tensor_count > 0;
    tmpl->summary.tokenizer_metadata_count = gt_tokenizer_metadata_count(tmpl->gguf);
    tmpl->summary.has_tokenizer =
        yvex_gguf_metadata_find(tmpl->gguf, "tokenizer.ggml.model") != NULL &&
        yvex_gguf_metadata_find(tmpl->gguf, "tokenizer.ggml.tokens") != NULL;

    tmpl->architecture = gt_copy_string_value(tmpl->gguf, "general.architecture");
    tmpl->model_name = gt_copy_string_value(tmpl->gguf, "general.name");
    tmpl->summary.architecture = tmpl->architecture ? tmpl->architecture : "";
    tmpl->summary.model_name = tmpl->model_name ? tmpl->model_name : "";
    tmpl->summary.has_architecture = tmpl->architecture && tmpl->architecture[0] != '\0';

    if (!tmpl->summary.has_architecture) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE,
                                          "", "general.architecture missing", err);
        if (rc != YVEX_OK) return rc;
        fatal = 1;
    }
    if (!tmpl->model_name || tmpl->model_name[0] == '\0') {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME,
                                          "", "general.name missing", err);
        if (rc != YVEX_OK) return rc;
        partial = 1;
    }
    if (!tmpl->summary.has_tokenizer) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER,
                                          "", "tokenizer.ggml.model or tokenizer.ggml.tokens missing", err);
        if (rc != YVEX_OK) return rc;
        if (options->require_tokenizer) {
            fatal = 1;
        } else {
            partial = 1;
        }
    }
    if (!tmpl->summary.has_tensor_directory) {
        rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY,
                                          "", "tensor directory is empty", err);
        if (rc != YVEX_OK) return rc;
        fatal = 1;
    }

    for (i = 0; i < yvex_tensor_table_count(tmpl->tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tmpl->tensors, i);
        if (!tensor) continue;
        if (tensor->role == YVEX_TENSOR_ROLE_UNKNOWN) {
            tmpl->summary.unknown_role_count++;
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE,
                                              tensor->name, "tensor role is unknown before open-weight intake mapping", err);
            if (rc != YVEX_OK) return rc;
            partial = 1;
        } else {
            tmpl->summary.known_role_count++;
        }
        if (tensor->storage_bytes == 0) {
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE,
                                              tensor->name, "tensor dtype has unsupported storage accounting", err);
            if (rc != YVEX_OK) return rc;
            partial = 1;
        }
    }

    if (fatal) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_INVALID;
    } else if (partial || tmpl->issue_count > 0) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_PARTIAL;
    } else {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_VALID;
    }
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}
