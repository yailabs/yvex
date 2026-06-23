/*
 * YVEX - External quantization job manifest API
 *
 * File: include/yvex/quant_job.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the OWI.9 external quantization job manifest surface. Quant jobs
 *   record provenance for external quantization commands.
 *   They do not run arbitrary shell commands, implement quantizers, infer, or
 *   claim runtime execution support.
 */
#ifndef YVEX_QUANT_JOB_H
#define YVEX_QUANT_JOB_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_QUANT_JOB_STATUS_UNKNOWN = 0,
    YVEX_QUANT_JOB_STATUS_DECLARED,
    YVEX_QUANT_JOB_STATUS_READY,
    YVEX_QUANT_JOB_STATUS_RUNNING,
    YVEX_QUANT_JOB_STATUS_SUCCEEDED,
    YVEX_QUANT_JOB_STATUS_FAILED,
    YVEX_QUANT_JOB_STATUS_SKIPPED
} yvex_quant_job_status;

typedef enum {
    YVEX_QUANT_JOB_TOOL_UNKNOWN = 0,
    YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL,
    YVEX_QUANT_JOB_TOOL_EXTERNAL
} yvex_quant_job_tool;

typedef struct {
    const char *name;
    const char *architecture;
    const char *tool_path;
    const char *source_manifest_path;
    const char *native_source_dir;
    const char *template_path;
    const char *quant_policy_path;
    const char *imatrix_manifest_path;
    const char *imatrix_path;
    const char *out_gguf_path;
    const char *log_path;
    const char *command;
    yvex_quant_job_tool tool;
    yvex_quant_job_status status;
} yvex_quant_job_options;

typedef struct {
    yvex_quant_job_status status;
    yvex_quant_job_tool tool;
    const char *name;
    const char *architecture;
    const char *tool_path;
    const char *native_source_dir;
    const char *template_path;
    const char *out_gguf_path;
    const char *log_path;
    int tool_exists;
    int source_exists;
    int template_exists;
    int imatrix_exists;
    int output_exists;
} yvex_quant_job_summary;

int yvex_quant_job_write_json(const char *out_path,
                              const yvex_quant_job_options *options,
                              yvex_quant_job_summary *summary_out,
                              yvex_error *err);

int yvex_quant_job_validate(const char *manifest_path,
                            yvex_quant_job_summary *summary_out,
                            yvex_error *err);

const char *yvex_quant_job_status_name(yvex_quant_job_status status);
const char *yvex_quant_job_tool_name(yvex_quant_job_tool tool);

yvex_quant_job_status yvex_quant_job_status_from_name(const char *name);
yvex_quant_job_tool yvex_quant_job_tool_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_QUANT_JOB_H */
