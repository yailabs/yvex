/*
 * YVEX - Imatrix manifest internals
 *
 * File: yvex_imatrix_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_IMATRIX_INTERNAL_H
#define YVEX_IMATRIX_INTERNAL_H

#include <yvex/imatrix.h>
#include <yvex/quant_policy.h>

typedef struct {
    yvex_imatrix_coverage_kind kind;
    char *selector;
    char *purpose;
} yvex_imatrix_coverage;

struct yvex_imatrix_manifest {
    char *name;
    char *architecture;
    char *source_manifest_path;
    char *quant_policy_path;
    char *imatrix_path;
    char *calibration_dataset;
    char *calibration_command;
    char *producer;
    yvex_imatrix_format format;
    yvex_imatrix_status declared_status;
    yvex_imatrix_summary summary;
    yvex_imatrix_coverage *coverage;
    unsigned long long coverage_count;
    unsigned long long coverage_cap;
};

char *yvex_imatrix_strdup(const char *s);

int yvex_imatrix_manifest_add_coverage(yvex_imatrix_manifest *manifest,
                                       yvex_imatrix_coverage_kind kind,
                                       const char *selector,
                                       const char *purpose,
                                       yvex_error *err);

int yvex_imatrix_manifest_parse_json(yvex_imatrix_manifest **out,
                                     const char *path,
                                     yvex_error *err);

int yvex_imatrix_manifest_write_json_file(const char *out_path,
                                          const yvex_imatrix_manifest *manifest,
                                          yvex_error *err);

void yvex_imatrix_manifest_refresh_summary(const yvex_imatrix_manifest *manifest,
                                           yvex_imatrix_summary *summary);

yvex_imatrix_status yvex_imatrix_status_from_name(const char *name);
yvex_imatrix_format yvex_imatrix_format_from_name(const char *name);
yvex_imatrix_coverage_kind yvex_imatrix_coverage_kind_from_name(const char *name);

#endif /* YVEX_IMATRIX_INTERNAL_H */
