/*
 * YVEX - Quant job internals
 *
 * File: src/tools/quant_job_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_QUANT_JOB_INTERNAL_H
#define YVEX_QUANT_JOB_INTERNAL_H

#include <yvex/quant_job.h>

typedef struct {
    char *name;
    char *architecture;
    char *tool_path;
    char *source_manifest_path;
    char *native_source_dir;
    char *template_path;
    char *quant_policy_path;
    char *imatrix_manifest_path;
    char *imatrix_path;
    char *out_gguf_path;
    char *log_path;
    char *command;
    yvex_quant_job_tool tool;
    yvex_quant_job_status status;
} yvex_quant_job_doc;

char *yvex_quant_job_strdup(const char *s);
void yvex_quant_job_doc_clear(yvex_quant_job_doc *doc);

int yvex_quant_job_parse_json_file(const char *path,
                                   yvex_quant_job_doc *doc,
                                   yvex_error *err);

int yvex_quant_job_write_json_file(const char *out_path,
                                   const yvex_quant_job_options *options,
                                   yvex_error *err);

void yvex_quant_job_summarize(const yvex_quant_job_doc *doc,
                              yvex_quant_job_summary *summary);

#endif /* YVEX_QUANT_JOB_INTERNAL_H */
