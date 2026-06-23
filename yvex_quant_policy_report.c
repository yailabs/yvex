/*
 * YVEX - Quant policy reporting
 *
 * File: yvex_quant_policy_report.c
 * Layer: tool-plane implementation
 */
#include "yvex_quant_policy_internal.h"

#include <stdio.h>

void yvex_quant_policy_print_summary(const yvex_quant_policy *policy,
                                     const char *mode,
                                     const char *path)
{
    yvex_quant_policy_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (!policy || yvex_quant_policy_get_summary(policy, &summary, &err) != YVEX_OK) {
        return;
    }
    printf("quant policy: %s\n", mode);
    if (path) printf("policy: %s\n", path);
    printf("name: %s\n", summary.name ? summary.name : "");
    printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
    printf("rules: %llu\n", summary.rule_count);
    printf("issues: %llu\n", summary.issue_count);
    printf("requires_imatrix: %llu\n", summary.requires_imatrix_count);
    printf("storage_supported: %llu\n", summary.storage_supported_count);
    printf("compute_supported: %llu\n", summary.compute_supported_count);
    printf("status: %s\n", yvex_quant_policy_status_name(summary.status));
}
