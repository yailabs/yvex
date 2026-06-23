/*
 * YVEX - GGUF template/native inventory comparison
 *
 * File: yvex_gguf_template_compare.c
 * Layer: tool-plane implementation
 */
#include "yvex_gguf_template_internal.h"

#include <stdio.h>

static int gt_same_shape(const yvex_tensor_info *tensor, const yvex_native_weight_info *native)
{
    unsigned int i;

    if (tensor->rank != native->rank) {
        return 0;
    }
    for (i = 0; i < tensor->rank; ++i) {
        if (tensor->dims[i] != native->dims[i]) {
            return 0;
        }
    }
    return 1;
}

int yvex_gguf_template_compare_native(yvex_gguf_template *tmpl,
                                      const yvex_gguf_template_options *options,
                                      yvex_error *err)
{
    yvex_native_weight_options native_options;
    yvex_native_weight_summary native_summary;
    unsigned long long i;
    int rc;

    if (!options->native_source_dir) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "gguf_template_compare", "native_source_dir is required");
        return YVEX_ERR_INVALID_ARG;
    }
    native_options.source_dir = options->native_source_dir;
    native_options.recursive = 1;
    native_options.include_metadata = 0;
    rc = yvex_native_weight_table_open(&tmpl->native, &native_options, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    rc = yvex_native_weight_table_summary(tmpl->native, &native_summary, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    tmpl->native_tensor_count = native_summary.tensor_count;

    for (i = 0; i < yvex_tensor_table_count(tmpl->tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tmpl->tensors, i);
        const yvex_native_weight_info *native;

        if (!tensor) continue;
        native = yvex_native_weight_table_find(tmpl->native, tensor->name);
        if (!native) {
            tmpl->missing_in_native++;
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR,
                                              tensor->name, "template tensor missing in native inventory; open-weight intake mapping may be required", err);
            if (rc != YVEX_OK) return rc;
            continue;
        }
        tmpl->matched_exact++;
        if (!gt_same_shape(tensor, native)) {
            char message[192];
            tmpl->shape_mismatch++;
            snprintf(message, sizeof(message), "native/template shape mismatch for exact-name tensor");
            rc = yvex_gguf_template_add_issue(tmpl, YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH,
                                              tensor->name, message, err);
            if (rc != YVEX_OK) return rc;
        }
    }

    if ((tmpl->missing_in_native > 0 || tmpl->shape_mismatch > 0) &&
        tmpl->summary.status == YVEX_GGUF_TEMPLATE_STATUS_VALID) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_PARTIAL;
    }
    if (options->require_all_template_tensors_in_native &&
        (tmpl->missing_in_native > 0 || tmpl->shape_mismatch > 0)) {
        tmpl->summary.status = YVEX_GGUF_TEMPLATE_STATUS_INVALID;
    }
    tmpl->summary.native_tensor_count = tmpl->native_tensor_count;
    tmpl->summary.matched_exact = tmpl->matched_exact;
    tmpl->summary.missing_in_native = tmpl->missing_in_native;
    tmpl->summary.shape_mismatch = tmpl->shape_mismatch;
    tmpl->summary.issue_count = tmpl->issue_count;
    return YVEX_OK;
}
