/*
 * YVEX - Artifact integrity baseline
 *
 * File: include/yvex/artifact_integrity.h
 * Layer: public artifact API
 *
 * Purpose:
 *   Defines the baseline GGUF artifact integrity validator. The validator
 *   checks structural bounds, tensor byte ranges, dtype/shape accounting, and
 *   selected embedding readiness before runtime paths trust tensor payloads.
 *
 * Does not own:
 *   - supply-chain security
 *   - digest enforcement
 *   - registry drift detection
 *   - malware detection
 *   - sandboxing
 */
#ifndef YVEX_ARTIFACT_INTEGRITY_H
#define YVEX_ARTIFACT_INTEGRITY_H

#include <yvex/artifact.h>
#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_INTEGRITY_CODE_CAP 64u
#define YVEX_INTEGRITY_TENSOR_CAP 128u
#define YVEX_INTEGRITY_REASON_CAP 256u
#define YVEX_INTEGRITY_FORMAT_CAP 16u
#define YVEX_INTEGRITY_ARCH_CAP 64u
#define YVEX_INTEGRITY_MAX_ISSUES 32u

typedef enum {
    YVEX_INTEGRITY_SEVERITY_ERROR = 0,
    YVEX_INTEGRITY_SEVERITY_WARNING = 1
} yvex_integrity_severity;

typedef struct {
    yvex_integrity_severity severity;
    char code[YVEX_INTEGRITY_CODE_CAP];
    char tensor[YVEX_INTEGRITY_TENSOR_CAP];
    char reason[YVEX_INTEGRITY_REASON_CAP];
} yvex_integrity_issue;

typedef struct {
    int require_token_embedding;
    unsigned int token_id;
} yvex_artifact_integrity_options;

typedef struct {
    int checked;
    int passed;
    char path[YVEX_ARTIFACT_PATH_CAP];
    char format[YVEX_INTEGRITY_FORMAT_CAP];
    unsigned long long file_size;
    unsigned int version;
    char architecture[YVEX_INTEGRITY_ARCH_CAP];
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    unsigned int error_count;
    unsigned int warning_count;
    unsigned int issue_count;
    yvex_integrity_issue issues[YVEX_INTEGRITY_MAX_ISSUES];
} yvex_artifact_integrity_report;

int yvex_artifact_integrity_check_path(const char *path,
                                       const yvex_artifact_integrity_options *options,
                                       yvex_artifact_integrity_report *out,
                                       yvex_error *err);

int yvex_artifact_integrity_validate(const yvex_artifact *artifact,
                                     const yvex_gguf *gguf,
                                     const yvex_tensor_table *tensors,
                                     const yvex_artifact_integrity_options *options,
                                     yvex_artifact_integrity_report *out,
                                     yvex_error *err);

const yvex_integrity_issue *yvex_artifact_integrity_issue_at(
    const yvex_artifact_integrity_report *report,
    unsigned int index);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_ARTIFACT_INTEGRITY_H */
