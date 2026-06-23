/*
 * YVEX - Imatrix manifest validation
 *
 * File: src/tools/imatrix_validate.c
 * Layer: tool-plane implementation
 */
#include "imatrix_internal.h"

#include <unistd.h>

int yvex_imatrix_manifest_validate(const yvex_imatrix_manifest *manifest,
                                   yvex_error *err)
{
    yvex_imatrix_summary *summary;

    if (!manifest) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_validate", "manifest is required");
        return YVEX_ERR_INVALID_ARG;
    }
    summary = (yvex_imatrix_summary *)&manifest->summary;
    yvex_imatrix_manifest_refresh_summary(manifest, summary);

    if (manifest->quant_policy_path && manifest->quant_policy_path[0]) {
        yvex_quant_policy *policy = NULL;
        yvex_quant_policy_summary policy_summary;
        yvex_error policy_err;
        int rc;

        yvex_error_clear(&policy_err);
        rc = yvex_quant_policy_open(&policy, manifest->quant_policy_path, &policy_err);
        if (rc == YVEX_OK &&
            yvex_quant_policy_get_summary(policy, &policy_summary, &policy_err) == YVEX_OK) {
            summary->requires_imatrix_rule_count = policy_summary.requires_imatrix_count;
            if (policy_summary.requires_imatrix_count > 0) {
                if (summary->file_exists &&
                    (manifest->format == YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT ||
                     manifest->format == YVEX_IMATRIX_FORMAT_LLAMA_CPP_DAT ||
                     manifest->format == YVEX_IMATRIX_FORMAT_JSON_MANIFEST ||
                     manifest->format == YVEX_IMATRIX_FORMAT_OTHER)) {
                    summary->covered_rule_count = policy_summary.requires_imatrix_count;
                } else {
                    summary->uncovered_rule_count = policy_summary.requires_imatrix_count;
                    summary->issue_count++;
                }
            }
        } else {
            summary->issue_count++;
        }
        yvex_quant_policy_close(policy);
    }

    if (summary->status == YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT ||
        summary->status == YVEX_IMATRIX_STATUS_INVALID) {
        return YVEX_OK;
    }
    if (summary->issue_count == 0) {
        summary->status = YVEX_IMATRIX_STATUS_PRESENT;
    } else if (!summary->file_exists) {
        summary->status = YVEX_IMATRIX_STATUS_MISSING;
    } else {
        summary->status = YVEX_IMATRIX_STATUS_DECLARED;
    }
    return YVEX_OK;
}
