/*
 * YVEX - GGUF template report helpers
 *
 * File: src/tools/gguf_template_report.c
 * Layer: tool-plane implementation
 */
#include "gguf_template_internal.h"

#include <stdio.h>

void yvex_gguf_template_print_summary(const yvex_gguf_template *tmpl,
                                      const char *mode,
                                      const char *template_path)
{
    yvex_gguf_template_summary summary;
    yvex_error err;

    yvex_error_clear(&err);
    if (yvex_gguf_template_get_summary(tmpl, &summary, &err) != YVEX_OK) {
        return;
    }
    printf("gguf template: %s\n", mode);
    printf("template: %s\n", template_path ? template_path : "");
    printf("architecture: %s\n", summary.architecture ? summary.architecture : "");
    printf("model_name: %s\n", summary.model_name ? summary.model_name : "");
    printf("metadata_count: %llu\n", summary.metadata_count);
    printf("tensor_count: %llu\n", summary.tensor_count);
    printf("has_tokenizer: %s\n", summary.has_tokenizer ? "yes" : "no");
    printf("known_roles: %llu\n", summary.known_role_count);
    printf("unknown_roles: %llu\n", summary.unknown_role_count);
    printf("status: %s\n", yvex_gguf_template_status_name(summary.status));
}
