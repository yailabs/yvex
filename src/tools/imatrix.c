/*
 * YVEX - Imatrix manifest implementation
 *
 * File: src/tools/imatrix.c
 * Layer: tool-plane implementation
 */
#include "imatrix_internal.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *yvex_imatrix_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

const char *yvex_imatrix_status_name(yvex_imatrix_status status)
{
    switch (status) {
    case YVEX_IMATRIX_STATUS_UNKNOWN: return "unknown";
    case YVEX_IMATRIX_STATUS_DECLARED: return "declared";
    case YVEX_IMATRIX_STATUS_PRESENT: return "present";
    case YVEX_IMATRIX_STATUS_MISSING: return "missing";
    case YVEX_IMATRIX_STATUS_INVALID: return "invalid";
    case YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT: return "unsupported_format";
    }
    return "unknown";
}

yvex_imatrix_status yvex_imatrix_status_from_name(const char *name)
{
    if (!name) return YVEX_IMATRIX_STATUS_UNKNOWN;
    if (strcmp(name, "declared") == 0) return YVEX_IMATRIX_STATUS_DECLARED;
    if (strcmp(name, "present") == 0) return YVEX_IMATRIX_STATUS_PRESENT;
    if (strcmp(name, "missing") == 0) return YVEX_IMATRIX_STATUS_MISSING;
    if (strcmp(name, "invalid") == 0) return YVEX_IMATRIX_STATUS_INVALID;
    if (strcmp(name, "unsupported_format") == 0) return YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT;
    return YVEX_IMATRIX_STATUS_UNKNOWN;
}

const char *yvex_imatrix_format_name(yvex_imatrix_format format)
{
    switch (format) {
    case YVEX_IMATRIX_FORMAT_UNKNOWN: return "unknown";
    case YVEX_IMATRIX_FORMAT_LLAMA_CPP_DAT: return "llama_cpp_dat";
    case YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT: return "routed_moe_dat";
    case YVEX_IMATRIX_FORMAT_JSON_MANIFEST: return "json_manifest";
    case YVEX_IMATRIX_FORMAT_OTHER: return "other";
    }
    return "unknown";
}

yvex_imatrix_format yvex_imatrix_format_from_name(const char *name)
{
    if (!name) return YVEX_IMATRIX_FORMAT_UNKNOWN;
    if (strcmp(name, "llama_cpp_dat") == 0) return YVEX_IMATRIX_FORMAT_LLAMA_CPP_DAT;
    if (strcmp(name, "routed_moe_dat") == 0) return YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT;
    if (strcmp(name, "json_manifest") == 0) return YVEX_IMATRIX_FORMAT_JSON_MANIFEST;
    if (strcmp(name, "other") == 0) return YVEX_IMATRIX_FORMAT_OTHER;
    return YVEX_IMATRIX_FORMAT_UNKNOWN;
}

const char *yvex_imatrix_coverage_kind_name(yvex_imatrix_coverage_kind kind)
{
    switch (kind) {
    case YVEX_IMATRIX_COVERAGE_UNKNOWN: return "unknown";
    case YVEX_IMATRIX_COVERAGE_GLOBAL: return "global";
    case YVEX_IMATRIX_COVERAGE_TENSOR_PATTERN: return "tensor_pattern";
    case YVEX_IMATRIX_COVERAGE_TENSOR_ROLE: return "tensor_role";
    case YVEX_IMATRIX_COVERAGE_LAYER_RANGE: return "layer_range";
    case YVEX_IMATRIX_COVERAGE_EXPERT_GROUP: return "expert_group";
    case YVEX_IMATRIX_COVERAGE_ROUTED_MOE: return "routed_moe";
    }
    return "unknown";
}

yvex_imatrix_coverage_kind yvex_imatrix_coverage_kind_from_name(const char *name)
{
    if (!name) return YVEX_IMATRIX_COVERAGE_UNKNOWN;
    if (strcmp(name, "global") == 0) return YVEX_IMATRIX_COVERAGE_GLOBAL;
    if (strcmp(name, "tensor_pattern") == 0) return YVEX_IMATRIX_COVERAGE_TENSOR_PATTERN;
    if (strcmp(name, "tensor_role") == 0) return YVEX_IMATRIX_COVERAGE_TENSOR_ROLE;
    if (strcmp(name, "layer_range") == 0) return YVEX_IMATRIX_COVERAGE_LAYER_RANGE;
    if (strcmp(name, "expert_group") == 0) return YVEX_IMATRIX_COVERAGE_EXPERT_GROUP;
    if (strcmp(name, "routed_moe") == 0) return YVEX_IMATRIX_COVERAGE_ROUTED_MOE;
    return YVEX_IMATRIX_COVERAGE_UNKNOWN;
}

const char *yvex_imatrix_issue_kind_name(yvex_imatrix_issue_kind issue)
{
    switch (issue) {
    case YVEX_IMATRIX_ISSUE_NONE: return "none";
    case YVEX_IMATRIX_ISSUE_FILE_MISSING: return "file_missing";
    case YVEX_IMATRIX_ISSUE_FORMAT_UNSUPPORTED: return "format_unsupported";
    case YVEX_IMATRIX_ISSUE_POLICY_REQUIRES_IMATRIX: return "policy_requires_imatrix";
    case YVEX_IMATRIX_ISSUE_POLICY_RULE_UNCOVERED: return "policy_rule_uncovered";
    case YVEX_IMATRIX_ISSUE_SOURCE_MISMATCH: return "source_mismatch";
    case YVEX_IMATRIX_ISSUE_FORMAT: return "format";
    case YVEX_IMATRIX_ISSUE_IO: return "io";
    }
    return "format";
}

int yvex_imatrix_manifest_add_coverage(yvex_imatrix_manifest *manifest,
                                       yvex_imatrix_coverage_kind kind,
                                       const char *selector,
                                       const char *purpose,
                                       yvex_error *err)
{
    yvex_imatrix_coverage *next;
    yvex_imatrix_coverage *row;

    if (!manifest || kind == YVEX_IMATRIX_COVERAGE_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_coverage", "manifest and known coverage kind are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (manifest->coverage_count == manifest->coverage_cap) {
        unsigned long long cap = manifest->coverage_cap == 0 ? 2u : manifest->coverage_cap * 2u;
        next = (yvex_imatrix_coverage *)realloc(manifest->coverage, (size_t)cap * sizeof(manifest->coverage[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_coverage", "coverage allocation failed");
            return YVEX_ERR_NOMEM;
        }
        manifest->coverage = next;
        manifest->coverage_cap = cap;
    }
    row = &manifest->coverage[manifest->coverage_count];
    memset(row, 0, sizeof(*row));
    row->kind = kind;
    row->selector = yvex_imatrix_strdup(selector);
    row->purpose = yvex_imatrix_strdup(purpose);
    if (!row->selector || !row->purpose) {
        free(row->selector);
        free(row->purpose);
        memset(row, 0, sizeof(*row));
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_coverage", "coverage string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    manifest->coverage_count++;
    return YVEX_OK;
}

int yvex_imatrix_manifest_create(yvex_imatrix_manifest **out,
                                 const yvex_imatrix_manifest_options *options,
                                 yvex_error *err)
{
    yvex_imatrix_manifest *manifest;
    int rc;

    if (!out || !options || !options->name || !options->architecture || !options->imatrix_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_create", "out, name, architecture, and imatrix path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    manifest = (yvex_imatrix_manifest *)calloc(1, sizeof(*manifest));
    if (!manifest) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_create", "manifest allocation failed");
        return YVEX_ERR_NOMEM;
    }
    manifest->name = yvex_imatrix_strdup(options->name);
    manifest->architecture = yvex_imatrix_strdup(options->architecture);
    manifest->source_manifest_path = yvex_imatrix_strdup(options->source_manifest_path);
    manifest->quant_policy_path = yvex_imatrix_strdup(options->quant_policy_path);
    manifest->imatrix_path = yvex_imatrix_strdup(options->imatrix_path);
    manifest->calibration_dataset = yvex_imatrix_strdup(options->calibration_dataset);
    manifest->calibration_command = yvex_imatrix_strdup(options->calibration_command);
    manifest->producer = yvex_imatrix_strdup(options->producer);
    manifest->format = options->format;
    manifest->declared_status = options->status;
    if (!manifest->name || !manifest->architecture || !manifest->source_manifest_path ||
        !manifest->quant_policy_path || !manifest->imatrix_path ||
        !manifest->calibration_dataset || !manifest->calibration_command || !manifest->producer) {
        yvex_imatrix_manifest_close(manifest);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_create", "manifest string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rc = yvex_imatrix_manifest_add_coverage(manifest, YVEX_IMATRIX_COVERAGE_ROUTED_MOE,
                                            "blk.*.ffn.experts.*",
                                            "expert-aware quantization weighting", err);
    if (rc != YVEX_OK) {
        yvex_imatrix_manifest_close(manifest);
        return rc;
    }
    yvex_imatrix_manifest_refresh_summary(manifest, &manifest->summary);
    *out = manifest;
    return YVEX_OK;
}

void yvex_imatrix_manifest_close(yvex_imatrix_manifest *manifest)
{
    unsigned long long i;

    if (!manifest) return;
    free(manifest->name);
    free(manifest->architecture);
    free(manifest->source_manifest_path);
    free(manifest->quant_policy_path);
    free(manifest->imatrix_path);
    free(manifest->calibration_dataset);
    free(manifest->calibration_command);
    free(manifest->producer);
    for (i = 0; i < manifest->coverage_count; ++i) {
        free(manifest->coverage[i].selector);
        free(manifest->coverage[i].purpose);
    }
    free(manifest->coverage);
    free(manifest);
}

int yvex_imatrix_manifest_write_json(const char *out_path,
                                     const yvex_imatrix_manifest *manifest,
                                     yvex_error *err)
{
    return yvex_imatrix_manifest_write_json_file(out_path, manifest, err);
}

int yvex_imatrix_manifest_open(yvex_imatrix_manifest **out,
                               const char *path,
                               yvex_error *err)
{
    int rc = yvex_imatrix_manifest_parse_json(out, path, err);
    if (rc == YVEX_OK) {
        rc = yvex_imatrix_manifest_validate(*out, err);
    }
    return rc;
}

int yvex_imatrix_manifest_get_summary(const yvex_imatrix_manifest *manifest,
                                      yvex_imatrix_summary *out,
                                      yvex_error *err)
{
    if (!manifest || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_summary", "manifest and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = manifest->summary;
    return YVEX_OK;
}

void yvex_imatrix_manifest_refresh_summary(const yvex_imatrix_manifest *manifest,
                                           yvex_imatrix_summary *summary)
{
    if (!manifest || !summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->status = manifest->declared_status;
    summary->format = manifest->format;
    summary->name = manifest->name;
    summary->architecture = manifest->architecture;
    summary->imatrix_path = manifest->imatrix_path;
    summary->source_manifest_path = manifest->source_manifest_path;
    summary->quant_policy_path = manifest->quant_policy_path;
    summary->file_exists = manifest->imatrix_path && access(manifest->imatrix_path, F_OK) == 0;
    if (summary->status == YVEX_IMATRIX_STATUS_UNKNOWN) {
        summary->status = summary->file_exists ? YVEX_IMATRIX_STATUS_PRESENT : YVEX_IMATRIX_STATUS_MISSING;
    }
    if (!summary->file_exists) {
        summary->issue_count++;
        if (summary->status == YVEX_IMATRIX_STATUS_PRESENT) summary->status = YVEX_IMATRIX_STATUS_MISSING;
    }
    if (manifest->format == YVEX_IMATRIX_FORMAT_UNKNOWN) {
        summary->issue_count++;
        summary->status = YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT;
    }
}
