/*
 * YVEX - GGUF template contract API
 *
 * File: include/yvex/gguf_template.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the open-weight intake GGUF template validator surface. A template is a
 *   structural contract for conversion/emission, not a source of truth,
 *   executable model, or generated YVEX artifact.
 */
#ifndef YVEX_GGUF_TEMPLATE_H
#define YVEX_GGUF_TEMPLATE_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_gguf_template yvex_gguf_template;

typedef enum {
    YVEX_GGUF_TEMPLATE_STATUS_UNKNOWN = 0,
    YVEX_GGUF_TEMPLATE_STATUS_VALID,
    YVEX_GGUF_TEMPLATE_STATUS_PARTIAL,
    YVEX_GGUF_TEMPLATE_STATUS_INVALID
} yvex_gguf_template_status;

typedef enum {
    YVEX_GGUF_TEMPLATE_ISSUE_NONE = 0,
    YVEX_GGUF_TEMPLATE_ISSUE_MISSING_ARCHITECTURE,
    YVEX_GGUF_TEMPLATE_ISSUE_MISSING_MODEL_NAME,
    YVEX_GGUF_TEMPLATE_ISSUE_MISSING_TOKENIZER,
    YVEX_GGUF_TEMPLATE_ISSUE_EMPTY_TENSOR_DIRECTORY,
    YVEX_GGUF_TEMPLATE_ISSUE_UNKNOWN_TENSOR_ROLE,
    YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_MISSING_TENSOR,
    YVEX_GGUF_TEMPLATE_ISSUE_NATIVE_SHAPE_MISMATCH,
    YVEX_GGUF_TEMPLATE_ISSUE_UNSUPPORTED_DTYPE,
    YVEX_GGUF_TEMPLATE_ISSUE_FORMAT
} yvex_gguf_template_issue_kind;

typedef struct {
    yvex_gguf_template_issue_kind kind;
    const char *tensor_name;
    const char *message;
} yvex_gguf_template_issue;

typedef struct {
    yvex_gguf_template_status status;
    const char *architecture;
    const char *model_name;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long known_role_count;
    unsigned long long unknown_role_count;
    unsigned long long tokenizer_metadata_count;
    unsigned long long issue_count;
    unsigned long long native_tensor_count;
    unsigned long long matched_exact;
    unsigned long long missing_in_native;
    unsigned long long shape_mismatch;
    int has_architecture;
    int has_tokenizer;
    int has_tensor_directory;
} yvex_gguf_template_summary;

typedef struct {
    const char *template_path;
    const char *native_source_dir;
    int compare_native;
    int require_tokenizer;
    int require_all_template_tensors_in_native;
} yvex_gguf_template_options;

int yvex_gguf_template_open(yvex_gguf_template **out,
                            const yvex_gguf_template_options *options,
                            yvex_error *err);

void yvex_gguf_template_close(yvex_gguf_template *tmpl);

int yvex_gguf_template_get_summary(const yvex_gguf_template *tmpl,
                                   yvex_gguf_template_summary *out,
                                   yvex_error *err);

unsigned long long yvex_gguf_template_issue_count(const yvex_gguf_template *tmpl);

const yvex_gguf_template_issue *yvex_gguf_template_issue_at(const yvex_gguf_template *tmpl,
                                                            unsigned long long index);

const char *yvex_gguf_template_status_name(yvex_gguf_template_status status);
const char *yvex_gguf_template_issue_kind_name(yvex_gguf_template_issue_kind kind);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_TEMPLATE_H */
