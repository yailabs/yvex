/*
 * gguf/quant.c - Quantization manifests and calibration metadata.
 *
 * This file owns quantization policy, quant job, imatrix manifest handling,
 * and the small built-in quantizer support matrix.
 */

#include <yvex/artifact.h>
#include <yvex/dtype.h>
#include <yvex/gguf.h>
#include <yvex/imatrix.h>
#include <yvex/quant_job.h>
#include <yvex/quant_policy.h>
#include <yvex/tensor.h>

#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int yvex_quant_q8_0_available(void);


/* Quant policy */

struct yvex_quant_policy {
    char *name;
    char *architecture;
    char *source_kind;
    char *template_path;
    yvex_quant_policy_rule *rules;
    unsigned long long rule_count;
    unsigned long long rule_cap;
    yvex_quant_policy_summary summary;
};

int yvex_quant_policy_add_rule(yvex_quant_policy *policy,
                               yvex_quant_selector_kind selector_kind,
                               const char *selector,
                               yvex_tensor_role role,
                               yvex_quant_qtype qtype,
                               int requires_imatrix,
                               yvex_error *err);

int yvex_quant_policy_parse_json(yvex_quant_policy **out,
                                 const char *path,
                                 yvex_error *err);

int yvex_quant_policy_validate(yvex_quant_policy *policy,
                               const char *template_path,
                               yvex_error *err);

int yvex_quant_policy_write_json_file(const char *out_path,
                                      const yvex_quant_policy *policy,
                                      yvex_error *err);

void yvex_quant_policy_print_summary(const yvex_quant_policy *policy,
                                     const char *mode,
                                     const char *path);

char *yvex_quant_policy_strdup(const char *s);
yvex_quant_qtype yvex_quant_qtype_from_name(const char *name);
yvex_quant_selector_kind yvex_quant_selector_kind_from_name(const char *name);
yvex_tensor_role yvex_quant_role_from_name(const char *name);
yvex_dtype yvex_quant_qtype_to_dtype(yvex_quant_qtype qtype);
int yvex_quant_qtype_storage_supported(yvex_quant_qtype qtype);
int yvex_quant_qtype_compute_supported(yvex_quant_qtype qtype);


/* Quant jobs */

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


/* Imatrix manifests */

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



typedef struct {
    const char *p;
    const char *end;
    const char *path;
    yvex_error *err;
} im_json;

static void ij_skip_ws(im_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}

static int ij_fail(im_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "imatrix_json", "%s in %s", msg, j->path);
    return YVEX_ERR_FORMAT;
}

static int ij_expect(im_json *j, char ch)
{
    ij_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) return ij_fail(j, "unexpected JSON token");
    j->p++;
    return YVEX_OK;
}

static char *ij_string(im_json *j)
{
    char *out;
    size_t cap;
    size_t n = 0;

    ij_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        ij_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    cap = (size_t)(j->end - j->p) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "imatrix_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                ij_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') out[n++] = ch;
            else if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else {
                free(out);
                ij_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    ij_fail(j, "unterminated string");
    return NULL;
}

static int ij_skip_value(im_json *j);

static int ij_skip_literal(im_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) return ij_fail(j, "unexpected literal");
    j->p += n;
    return YVEX_OK;
}

static int ij_skip_object(im_json *j)
{
    int rc = ij_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = ij_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = ij_skip_value(j);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed object");
    }
    return ij_fail(j, "unterminated object");
}

static int ij_skip_array(im_json *j)
{
    int rc = ij_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = ij_skip_value(j);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed array");
    }
    return ij_fail(j, "unterminated array");
}

static int ij_skip_value(im_json *j)
{
    char *s;
    ij_skip_ws(j);
    if (j->p >= j->end) return ij_fail(j, "expected value");
    if (*j->p == '{') return ij_skip_object(j);
    if (*j->p == '[') return ij_skip_array(j);
    if (*j->p == '"') {
        s = ij_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (*j->p == 't') return ij_skip_literal(j, "true");
    if (*j->p == 'f') return ij_skip_literal(j, "false");
    if (*j->p == 'n') return ij_skip_literal(j, "null");
    while (j->p < j->end && (isdigit((unsigned char)*j->p) || *j->p == '-')) j->p++;
    return YVEX_OK;
}

static int ij_read_file(const char *path, char **out, unsigned long long *len, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot open manifest: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot size manifest: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot read manifest: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    *len = (unsigned long long)size;
    return YVEX_OK;
}

static int ij_parse_named_object(im_json *j, yvex_imatrix_manifest *manifest, const char *object_name)
{
    int rc = ij_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        key = ij_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            return rc;
        }
        if (strcmp(object_name, "imatrix") == 0 && strcmp(key, "path") == 0) {
            free(manifest->imatrix_path);
            manifest->imatrix_path = ij_string(j);
            if (!manifest->imatrix_path) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "dataset") == 0) {
            free(manifest->calibration_dataset);
            manifest->calibration_dataset = ij_string(j);
            if (!manifest->calibration_dataset) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "command") == 0) {
            free(manifest->calibration_command);
            manifest->calibration_command = ij_string(j);
            if (!manifest->calibration_command) rc = yvex_error_code(j->err);
        } else if (strcmp(object_name, "calibration") == 0 && strcmp(key, "producer") == 0) {
            free(manifest->producer);
            manifest->producer = ij_string(j);
            if (!manifest->producer) rc = yvex_error_code(j->err);
        } else {
            rc = ij_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    return ij_fail(j, "unterminated nested object");
}

static int ij_parse_coverage_row(im_json *j, yvex_imatrix_manifest *manifest)
{
    char *kind = NULL;
    char *selector = NULL;
    char *purpose = NULL;
    int rc = ij_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            break;
        }
        key = ij_string(j);
        if (!key) {
            rc = yvex_error_code(j->err);
            goto done;
        }
        rc = ij_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto done;
        }
        if (strcmp(key, "kind") == 0) kind = ij_string(j);
        else if (strcmp(key, "selector") == 0) selector = ij_string(j);
        else if (strcmp(key, "purpose") == 0) purpose = ij_string(j);
        else rc = ij_skip_value(j);
        free(key);
        if (rc != YVEX_OK) goto done;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    rc = yvex_imatrix_manifest_add_coverage(manifest,
                                            yvex_imatrix_coverage_kind_from_name(kind),
                                            selector ? selector : "",
                                            purpose ? purpose : "",
                                            j->err);
done:
    free(kind);
    free(selector);
    free(purpose);
    return rc;
}

static int ij_parse_coverage(im_json *j, yvex_imatrix_manifest *manifest)
{
    int rc = ij_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    ij_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = ij_parse_coverage_row(j, manifest);
        if (rc != YVEX_OK) return rc;
        ij_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return ij_fail(j, "malformed coverage array");
    }
    return ij_fail(j, "unterminated coverage array");
}

int yvex_imatrix_manifest_parse_json(yvex_imatrix_manifest **out,
                                     const char *path,
                                     yvex_error *err)
{
    yvex_imatrix_manifest *manifest;
    im_json j;
    char *buf = NULL;
    unsigned long long len = 0;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = ij_read_file(path, &buf, &len, err);
    if (rc != YVEX_OK) return rc;
    manifest = (yvex_imatrix_manifest *)calloc(1, sizeof(*manifest));
    if (!manifest) {
        free(buf);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest allocation failed");
        return YVEX_ERR_NOMEM;
    }
    j.p = buf;
    j.end = buf + len;
    j.path = path;
    j.err = err;
    rc = ij_expect(&j, '{');
    if (rc != YVEX_OK) goto fail;
    while (j.p < j.end) {
        char *key;
        ij_skip_ws(&j);
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            break;
        }
        key = ij_string(&j);
        if (!key) {
            rc = yvex_error_code(err);
            goto fail;
        }
        rc = ij_expect(&j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto fail;
        }
        if (strcmp(key, "schema") == 0) {
            char *schema = ij_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.imatrix_manifest.v1") != 0) {
                free(schema);
                free(key);
                rc = ij_fail(&j, "unsupported imatrix schema");
                goto fail;
            }
            free(schema);
        } else if (strcmp(key, "name") == 0) {
            free(manifest->name);
            manifest->name = ij_string(&j);
            if (!manifest->name) rc = yvex_error_code(err);
        } else if (strcmp(key, "architecture") == 0) {
            free(manifest->architecture);
            manifest->architecture = ij_string(&j);
            if (!manifest->architecture) rc = yvex_error_code(err);
        } else if (strcmp(key, "status") == 0) {
            char *status = ij_string(&j);
            if (!status) rc = yvex_error_code(err);
            else {
                manifest->declared_status = yvex_imatrix_status_from_name(status);
                free(status);
            }
        } else if (strcmp(key, "format") == 0) {
            char *format = ij_string(&j);
            if (!format) rc = yvex_error_code(err);
            else {
                manifest->format = yvex_imatrix_format_from_name(format);
                free(format);
            }
        } else if (strcmp(key, "source_manifest") == 0) {
            free(manifest->source_manifest_path);
            manifest->source_manifest_path = ij_string(&j);
            if (!manifest->source_manifest_path) rc = yvex_error_code(err);
        } else if (strcmp(key, "quant_policy") == 0) {
            free(manifest->quant_policy_path);
            manifest->quant_policy_path = ij_string(&j);
            if (!manifest->quant_policy_path) rc = yvex_error_code(err);
        } else if (strcmp(key, "imatrix") == 0) {
            rc = ij_parse_named_object(&j, manifest, "imatrix");
        } else if (strcmp(key, "calibration") == 0) {
            rc = ij_parse_named_object(&j, manifest, "calibration");
        } else if (strcmp(key, "coverage") == 0) {
            rc = ij_parse_coverage(&j, manifest);
        } else {
            rc = ij_skip_value(&j);
        }
        free(key);
        if (rc != YVEX_OK) goto fail;
        ij_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') j.p++;
    }
    if (!manifest->name) manifest->name = yvex_imatrix_strdup("unnamed-imatrix");
    if (!manifest->architecture) manifest->architecture = yvex_imatrix_strdup("unknown");
    if (!manifest->source_manifest_path) manifest->source_manifest_path = yvex_imatrix_strdup("");
    if (!manifest->quant_policy_path) manifest->quant_policy_path = yvex_imatrix_strdup("");
    if (!manifest->imatrix_path) manifest->imatrix_path = yvex_imatrix_strdup("");
    if (!manifest->calibration_dataset) manifest->calibration_dataset = yvex_imatrix_strdup("");
    if (!manifest->calibration_command) manifest->calibration_command = yvex_imatrix_strdup("");
    if (!manifest->producer) manifest->producer = yvex_imatrix_strdup("");
    if (!manifest->name || !manifest->architecture || !manifest->source_manifest_path ||
        !manifest->quant_policy_path || !manifest->imatrix_path ||
        !manifest->calibration_dataset || !manifest->calibration_command || !manifest->producer) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest string allocation failed");
        goto fail;
    }
    yvex_imatrix_manifest_refresh_summary(manifest, &manifest->summary);
    *out = manifest;
    free(buf);
    return YVEX_OK;
fail:
    yvex_imatrix_manifest_close(manifest);
    free(buf);
    return rc;
}

static void ij_write_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') fputs("\\n", fp);
        else if (ch == '\r') fputs("\\r", fp);
        else if (ch == '\t') fputs("\\t", fp);
        else if (ch < 32) fprintf(fp, "\\u%04x", (unsigned int)ch);
        else fputc((int)ch, fp);
    }
    fputc('"', fp);
}

int yvex_imatrix_manifest_write_json_file(const char *out_path,
                                          const yvex_imatrix_manifest *manifest,
                                          yvex_error *err)
{
    FILE *fp;
    unsigned long long i;

    if (!out_path || !manifest) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json", "out_path and manifest are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot open output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.imatrix_manifest.v1\",\n");
    fprintf(fp, "  \"name\": "); ij_write_string(fp, manifest->name); fprintf(fp, ",\n");
    fprintf(fp, "  \"architecture\": "); ij_write_string(fp, manifest->architecture); fprintf(fp, ",\n");
    fprintf(fp, "  \"status\": "); ij_write_string(fp, yvex_imatrix_status_name(manifest->declared_status)); fprintf(fp, ",\n");
    fprintf(fp, "  \"format\": "); ij_write_string(fp, yvex_imatrix_format_name(manifest->format)); fprintf(fp, ",\n");
    fprintf(fp, "  \"source_manifest\": "); ij_write_string(fp, manifest->source_manifest_path); fprintf(fp, ",\n");
    fprintf(fp, "  \"quant_policy\": "); ij_write_string(fp, manifest->quant_policy_path); fprintf(fp, ",\n");
    fprintf(fp, "  \"imatrix\": {\n");
    fprintf(fp, "    \"path\": "); ij_write_string(fp, manifest->imatrix_path); fprintf(fp, ",\n");
    fprintf(fp, "    \"external\": true\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"calibration\": {\n");
    fprintf(fp, "    \"dataset\": "); ij_write_string(fp, manifest->calibration_dataset); fprintf(fp, ",\n");
    fprintf(fp, "    \"command\": "); ij_write_string(fp, manifest->calibration_command); fprintf(fp, ",\n");
    fprintf(fp, "    \"producer\": "); ij_write_string(fp, manifest->producer); fprintf(fp, "\n");
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"coverage\": [\n");
    for (i = 0; i < manifest->coverage_count; ++i) {
        const yvex_imatrix_coverage *row = &manifest->coverage[i];
        fprintf(fp, "    {\"kind\": ");
        ij_write_string(fp, yvex_imatrix_coverage_kind_name(row->kind));
        fprintf(fp, ", \"selector\": ");
        ij_write_string(fp, row->selector);
        fprintf(fp, ", \"purpose\": ");
        ij_write_string(fp, row->purpose);
        fprintf(fp, "}%s\n", i + 1u == manifest->coverage_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "failed closing output manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}





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



static yvex_quant_job_doc qj_last_summary_doc;

static void qj_store_summary_doc(yvex_quant_job_doc *doc)
{
    yvex_quant_job_doc_clear(&qj_last_summary_doc);
    qj_last_summary_doc = *doc;
    memset(doc, 0, sizeof(*doc));
}

char *yvex_quant_job_strdup(const char *s)
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

void yvex_quant_job_doc_clear(yvex_quant_job_doc *doc)
{
    if (!doc) return;
    free(doc->name);
    free(doc->architecture);
    free(doc->tool_path);
    free(doc->source_manifest_path);
    free(doc->native_source_dir);
    free(doc->template_path);
    free(doc->quant_policy_path);
    free(doc->imatrix_manifest_path);
    free(doc->imatrix_path);
    free(doc->out_gguf_path);
    free(doc->log_path);
    free(doc->command);
    memset(doc, 0, sizeof(*doc));
}

const char *yvex_quant_job_status_name(yvex_quant_job_status status)
{
    switch (status) {
    case YVEX_QUANT_JOB_STATUS_UNKNOWN: return "unknown";
    case YVEX_QUANT_JOB_STATUS_DECLARED: return "declared";
    case YVEX_QUANT_JOB_STATUS_READY: return "ready";
    case YVEX_QUANT_JOB_STATUS_RUNNING: return "running";
    case YVEX_QUANT_JOB_STATUS_SUCCEEDED: return "succeeded";
    case YVEX_QUANT_JOB_STATUS_FAILED: return "failed";
    case YVEX_QUANT_JOB_STATUS_SKIPPED: return "skipped";
    }
    return "unknown";
}

yvex_quant_job_status yvex_quant_job_status_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_JOB_STATUS_UNKNOWN;
    if (strcmp(name, "declared") == 0) return YVEX_QUANT_JOB_STATUS_DECLARED;
    if (strcmp(name, "ready") == 0) return YVEX_QUANT_JOB_STATUS_READY;
    if (strcmp(name, "running") == 0) return YVEX_QUANT_JOB_STATUS_RUNNING;
    if (strcmp(name, "succeeded") == 0) return YVEX_QUANT_JOB_STATUS_SUCCEEDED;
    if (strcmp(name, "failed") == 0) return YVEX_QUANT_JOB_STATUS_FAILED;
    if (strcmp(name, "skipped") == 0) return YVEX_QUANT_JOB_STATUS_SKIPPED;
    return YVEX_QUANT_JOB_STATUS_UNKNOWN;
}

const char *yvex_quant_job_tool_name(yvex_quant_job_tool tool)
{
    switch (tool) {
    case YVEX_QUANT_JOB_TOOL_UNKNOWN: return "unknown";
    case YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL: return "yvex-internal";
    case YVEX_QUANT_JOB_TOOL_EXTERNAL: return "external";
    }
    return "unknown";
}

yvex_quant_job_tool yvex_quant_job_tool_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_JOB_TOOL_UNKNOWN;
    if (strcmp(name, "unknown") == 0) return YVEX_QUANT_JOB_TOOL_UNKNOWN;
    if (strcmp(name, "yvex-internal") == 0) return YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL;
    if (strcmp(name, "external") == 0) return YVEX_QUANT_JOB_TOOL_EXTERNAL;
    return YVEX_QUANT_JOB_TOOL_UNKNOWN;
}

void yvex_quant_job_summarize(const yvex_quant_job_doc *doc,
                              yvex_quant_job_summary *summary)
{
    if (!doc || !summary) return;
    memset(summary, 0, sizeof(*summary));
    summary->status = doc->status;
    summary->tool = doc->tool;
    summary->name = doc->name;
    summary->architecture = doc->architecture;
    summary->tool_path = doc->tool_path;
    summary->native_source_dir = doc->native_source_dir;
    summary->template_path = doc->template_path;
    summary->out_gguf_path = doc->out_gguf_path;
    summary->log_path = doc->log_path;
    summary->tool_exists = doc->tool_path && doc->tool_path[0] && access(doc->tool_path, X_OK) == 0;
    summary->source_exists = doc->native_source_dir && doc->native_source_dir[0] && access(doc->native_source_dir, F_OK) == 0;
    summary->template_exists = doc->template_path && doc->template_path[0] && access(doc->template_path, F_OK) == 0;
    summary->imatrix_exists = doc->imatrix_path && doc->imatrix_path[0] && access(doc->imatrix_path, F_OK) == 0;
    summary->output_exists = doc->out_gguf_path && doc->out_gguf_path[0] && access(doc->out_gguf_path, F_OK) == 0;
}

static int qj_options_to_doc(const yvex_quant_job_options *options,
                             yvex_quant_job_doc *doc,
                             yvex_error *err)
{
    memset(doc, 0, sizeof(*doc));
    if (!options || !options->name || !options->architecture ||
        !options->tool_path || !options->native_source_dir ||
        !options->template_path || !options->out_gguf_path ||
        !options->log_path || !options->command) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "name, architecture, tool path, native source, template, output GGUF, log, and command are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "known status is required");
        return YVEX_ERR_INVALID_ARG;
    }
    doc->name = yvex_quant_job_strdup(options->name);
    doc->architecture = yvex_quant_job_strdup(options->architecture);
    doc->tool_path = yvex_quant_job_strdup(options->tool_path);
    doc->source_manifest_path = yvex_quant_job_strdup(options->source_manifest_path);
    doc->native_source_dir = yvex_quant_job_strdup(options->native_source_dir);
    doc->template_path = yvex_quant_job_strdup(options->template_path);
    doc->quant_policy_path = yvex_quant_job_strdup(options->quant_policy_path);
    doc->imatrix_manifest_path = yvex_quant_job_strdup(options->imatrix_manifest_path);
    doc->imatrix_path = yvex_quant_job_strdup(options->imatrix_path);
    doc->out_gguf_path = yvex_quant_job_strdup(options->out_gguf_path);
    doc->log_path = yvex_quant_job_strdup(options->log_path);
    doc->command = yvex_quant_job_strdup(options->command);
    doc->tool = options->tool;
    doc->status = options->status;
    if (!doc->name || !doc->architecture || !doc->tool_path ||
        !doc->source_manifest_path || !doc->native_source_dir ||
        !doc->template_path || !doc->quant_policy_path ||
        !doc->imatrix_manifest_path || !doc->imatrix_path ||
        !doc->out_gguf_path || !doc->log_path || !doc->command) {
        yvex_quant_job_doc_clear(doc);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_job", "manifest string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_quant_job_write_json(const char *out_path,
                              const yvex_quant_job_options *options,
                              yvex_quant_job_summary *summary_out,
                              yvex_error *err)
{
    yvex_quant_job_doc doc;
    int rc;

    if (!out_path || !out_path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = qj_options_to_doc(options, &doc, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_quant_job_write_json_file(out_path, options, err);
    if (rc == YVEX_OK) {
        qj_store_summary_doc(&doc);
        if (summary_out) yvex_quant_job_summarize(&qj_last_summary_doc, summary_out);
    } else {
        yvex_quant_job_doc_clear(&doc);
    }
    return rc;
}

int yvex_quant_job_validate(const char *manifest_path,
                            yvex_quant_job_summary *summary_out,
                            yvex_error *err)
{
    yvex_quant_job_doc doc;
    yvex_quant_job_summary summary;
    int rc;

    if (!manifest_path || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_validate", "manifest path and summary are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&doc, 0, sizeof(doc));
    rc = yvex_quant_job_parse_json_file(manifest_path, &doc, err);
    if (rc != YVEX_OK) return rc;
    yvex_quant_job_summarize(&doc, &summary);
    *summary_out = summary;
    if (!summary.name || !summary.name[0] || !summary.architecture || !summary.architecture[0] ||
        doc.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        yvex_quant_job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_FORMAT, "quant_job_validate", "manifest is missing required identity fields");
        return YVEX_ERR_FORMAT;
    }
    if (doc.status == YVEX_QUANT_JOB_STATUS_SUCCEEDED && !summary.output_exists) {
        yvex_quant_job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_STATE, "quant_job_validate", "succeeded quant job requires output GGUF to exist");
        return YVEX_ERR_STATE;
    }
    qj_store_summary_doc(&doc);
    yvex_quant_job_summarize(&qj_last_summary_doc, summary_out);
    return YVEX_OK;
}



static int qj_read_file(const char *path, char **out, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot open manifest: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot size manifest: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_job_json", "read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot read manifest: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    return YVEX_OK;
}

static char *qj_extract_string(const char *json, const char *key)
{
    const char *p;
    const char *colon;
    const char *s;
    char *needle;
    char *out;
    size_t key_len = strlen(key);
    size_t n = 0;

    needle = (char *)malloc(key_len + 4u);
    if (!needle) return NULL;
    sprintf(needle, "\"%s\"", key);
    p = strstr(json, needle);
    free(needle);
    if (!p) return yvex_quant_job_strdup("");
    colon = strchr(p, ':');
    if (!colon) return NULL;
    s = colon + 1;
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t') s++;
    if (*s != '"') return NULL;
    s++;
    out = (char *)malloc(strlen(s) + 1u);
    if (!out) return NULL;
    while (*s) {
        char ch = *s++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\' && *s) {
            ch = *s++;
            if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else out[n++] = ch;
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    return NULL;
}

int yvex_quant_job_parse_json_file(const char *path,
                                   yvex_quant_job_doc *doc,
                                   yvex_error *err)
{
    char *json = NULL;
    char *status = NULL;
    char *tool = NULL;
    int rc;

    if (!path || !doc) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_json", "path and doc are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(doc, 0, sizeof(*doc));
    rc = qj_read_file(path, &json, err);
    if (rc != YVEX_OK) return rc;
    doc->name = qj_extract_string(json, "name");
    doc->architecture = qj_extract_string(json, "architecture");
    doc->tool_path = qj_extract_string(json, "path");
    doc->source_manifest_path = qj_extract_string(json, "source_manifest");
    doc->native_source_dir = qj_extract_string(json, "native_source");
    doc->template_path = qj_extract_string(json, "template");
    doc->quant_policy_path = qj_extract_string(json, "quant_policy");
    doc->imatrix_manifest_path = qj_extract_string(json, "imatrix_manifest");
    doc->imatrix_path = qj_extract_string(json, "imatrix");
    doc->out_gguf_path = qj_extract_string(json, "gguf");
    doc->log_path = qj_extract_string(json, "log");
    doc->command = qj_extract_string(json, "command");
    status = qj_extract_string(json, "status");
    tool = qj_extract_string(json, "kind");
    if (!doc->name || !doc->architecture || !doc->tool_path ||
        !doc->source_manifest_path || !doc->native_source_dir ||
        !doc->template_path || !doc->quant_policy_path ||
        !doc->imatrix_manifest_path || !doc->imatrix_path ||
        !doc->out_gguf_path || !doc->log_path || !doc->command ||
        !status || !tool) {
        free(json);
        free(status);
        free(tool);
        yvex_quant_job_doc_clear(doc);
        yvex_error_set(err, YVEX_ERR_FORMAT, "quant_job_json", "malformed quant job manifest");
        return YVEX_ERR_FORMAT;
    }
    doc->status = yvex_quant_job_status_from_name(status);
    doc->tool = yvex_quant_job_tool_from_name(tool);
    free(json);
    free(status);
    free(tool);
    return YVEX_OK;
}

static void qj_write_escaped(FILE *fp, const char *s)
{
    if (!s) s = "";
    fputc('"', fp);
    while (*s) {
        unsigned char ch = (unsigned char)*s++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

static void qj_write_field(FILE *fp, const char *indent, const char *key, const char *value, int comma)
{
    fputs(indent, fp);
    fprintf(fp, "\"%s\": ", key);
    qj_write_escaped(fp, value);
    fprintf(fp, "%s\n", comma ? "," : "");
}

int yvex_quant_job_write_json_file(const char *out_path,
                                   const yvex_quant_job_options *options,
                                   yvex_error *err)
{
    FILE *fp;

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot write manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    qj_write_field(fp, "  ", "schema", "yvex.quant_job.v1", 1);
    qj_write_field(fp, "  ", "name", options->name, 1);
    qj_write_field(fp, "  ", "architecture", options->architecture, 1);
    qj_write_field(fp, "  ", "status", yvex_quant_job_status_name(options->status), 1);
    fprintf(fp, "  \"tool\": {\n");
    qj_write_field(fp, "    ", "kind", yvex_quant_job_tool_name(options->tool), 1);
    qj_write_field(fp, "    ", "path", options->tool_path, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"inputs\": {\n");
    qj_write_field(fp, "    ", "source_manifest", options->source_manifest_path, 1);
    qj_write_field(fp, "    ", "native_source", options->native_source_dir, 1);
    qj_write_field(fp, "    ", "template", options->template_path, 1);
    qj_write_field(fp, "    ", "quant_policy", options->quant_policy_path, 1);
    qj_write_field(fp, "    ", "imatrix_manifest", options->imatrix_manifest_path, 1);
    qj_write_field(fp, "    ", "imatrix", options->imatrix_path, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"outputs\": {\n");
    qj_write_field(fp, "    ", "gguf", options->out_gguf_path, 1);
    qj_write_field(fp, "    ", "log", options->log_path, 0);
    fprintf(fp, "  },\n");
    qj_write_field(fp, "  ", "command", options->command, 0);
    fprintf(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot close manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}





char *yvex_quant_policy_strdup(const char *s)
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

const char *yvex_quant_qtype_name(yvex_quant_qtype qtype)
{
    switch (qtype) {
    case YVEX_QUANT_QTYPE_UNKNOWN: return "UNKNOWN";
    case YVEX_QUANT_QTYPE_F32: return "F32";
    case YVEX_QUANT_QTYPE_F16: return "F16";
    case YVEX_QUANT_QTYPE_BF16: return "BF16";
    case YVEX_QUANT_QTYPE_Q8_0: return "Q8_0";
    case YVEX_QUANT_QTYPE_Q4_0: return "Q4_0";
    case YVEX_QUANT_QTYPE_Q4_K: return "Q4_K";
    case YVEX_QUANT_QTYPE_Q5_K: return "Q5_K";
    case YVEX_QUANT_QTYPE_Q6_K: return "Q6_K";
    case YVEX_QUANT_QTYPE_Q2_K: return "Q2_K";
    case YVEX_QUANT_QTYPE_IQ2_XXS: return "IQ2_XXS";
    case YVEX_QUANT_QTYPE_IQ2_XS: return "IQ2_XS";
    case YVEX_QUANT_QTYPE_IQ3_XXS: return "IQ3_XXS";
    case YVEX_QUANT_QTYPE_IQ4_NL: return "IQ4_NL";
    case YVEX_QUANT_QTYPE_OTHER: return "OTHER";
    }
    return "UNKNOWN";
}

yvex_quant_qtype yvex_quant_qtype_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_QTYPE_UNKNOWN;
    if (strcmp(name, "F32") == 0) return YVEX_QUANT_QTYPE_F32;
    if (strcmp(name, "F16") == 0) return YVEX_QUANT_QTYPE_F16;
    if (strcmp(name, "BF16") == 0) return YVEX_QUANT_QTYPE_BF16;
    if (strcmp(name, "Q8_0") == 0) return YVEX_QUANT_QTYPE_Q8_0;
    if (strcmp(name, "Q4_0") == 0) return YVEX_QUANT_QTYPE_Q4_0;
    if (strcmp(name, "Q4_K") == 0) return YVEX_QUANT_QTYPE_Q4_K;
    if (strcmp(name, "Q5_K") == 0) return YVEX_QUANT_QTYPE_Q5_K;
    if (strcmp(name, "Q6_K") == 0) return YVEX_QUANT_QTYPE_Q6_K;
    if (strcmp(name, "Q2_K") == 0) return YVEX_QUANT_QTYPE_Q2_K;
    if (strcmp(name, "IQ2_XXS") == 0) return YVEX_QUANT_QTYPE_IQ2_XXS;
    if (strcmp(name, "IQ2_XS") == 0) return YVEX_QUANT_QTYPE_IQ2_XS;
    if (strcmp(name, "IQ3_XXS") == 0) return YVEX_QUANT_QTYPE_IQ3_XXS;
    if (strcmp(name, "IQ4_NL") == 0) return YVEX_QUANT_QTYPE_IQ4_NL;
    if (strcmp(name, "OTHER") == 0) return YVEX_QUANT_QTYPE_OTHER;
    return YVEX_QUANT_QTYPE_UNKNOWN;
}

const char *yvex_quant_selector_kind_name(yvex_quant_selector_kind kind)
{
    switch (kind) {
    case YVEX_QUANT_SELECTOR_UNKNOWN: return "unknown";
    case YVEX_QUANT_SELECTOR_ROLE: return "role";
    case YVEX_QUANT_SELECTOR_TENSOR_NAME: return "tensor_name";
    case YVEX_QUANT_SELECTOR_TENSOR_PATTERN: return "pattern";
    case YVEX_QUANT_SELECTOR_LAYER_RANGE: return "layer_range";
    case YVEX_QUANT_SELECTOR_EXPERT_GROUP: return "expert_group";
    case YVEX_QUANT_SELECTOR_DEFAULT: return "default";
    }
    return "unknown";
}

yvex_quant_selector_kind yvex_quant_selector_kind_from_name(const char *name)
{
    if (!name) return YVEX_QUANT_SELECTOR_UNKNOWN;
    if (strcmp(name, "role") == 0) return YVEX_QUANT_SELECTOR_ROLE;
    if (strcmp(name, "tensor_name") == 0) return YVEX_QUANT_SELECTOR_TENSOR_NAME;
    if (strcmp(name, "name") == 0) return YVEX_QUANT_SELECTOR_TENSOR_NAME;
    if (strcmp(name, "pattern") == 0) return YVEX_QUANT_SELECTOR_TENSOR_PATTERN;
    if (strcmp(name, "tensor_pattern") == 0) return YVEX_QUANT_SELECTOR_TENSOR_PATTERN;
    if (strcmp(name, "layer_range") == 0) return YVEX_QUANT_SELECTOR_LAYER_RANGE;
    if (strcmp(name, "expert_group") == 0) return YVEX_QUANT_SELECTOR_EXPERT_GROUP;
    if (strcmp(name, "default") == 0) return YVEX_QUANT_SELECTOR_DEFAULT;
    return YVEX_QUANT_SELECTOR_UNKNOWN;
}

const char *yvex_quant_policy_status_name(yvex_quant_policy_status status)
{
    switch (status) {
    case YVEX_QUANT_POLICY_STATUS_UNKNOWN: return "quant-policy-unknown";
    case YVEX_QUANT_POLICY_STATUS_VALID: return "quant-policy-valid";
    case YVEX_QUANT_POLICY_STATUS_PARTIAL: return "quant-policy-partial";
    case YVEX_QUANT_POLICY_STATUS_INVALID: return "quant-policy-invalid";
    }
    return "quant-policy-unknown";
}

const char *yvex_quant_policy_issue_kind_name(yvex_quant_policy_issue_kind issue)
{
    switch (issue) {
    case YVEX_QUANT_POLICY_ISSUE_NONE: return "none";
    case YVEX_QUANT_POLICY_ISSUE_UNKNOWN_QTYPE: return "unknown_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_STORAGE_QTYPE: return "unsupported_storage_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNSUPPORTED_COMPUTE_QTYPE: return "unsupported_compute_qtype";
    case YVEX_QUANT_POLICY_ISSUE_UNKNOWN_ROLE: return "unknown_role";
    case YVEX_QUANT_POLICY_ISSUE_UNMATCHED_SELECTOR: return "unmatched_selector";
    case YVEX_QUANT_POLICY_ISSUE_TEMPLATE_QTYPE_MISMATCH: return "template_qtype_mismatch";
    case YVEX_QUANT_POLICY_ISSUE_REQUIRES_IMATRIX: return "requires_imatrix";
    case YVEX_QUANT_POLICY_ISSUE_FORMAT: return "format";
    }
    return "format";
}

yvex_dtype yvex_quant_qtype_to_dtype(yvex_quant_qtype qtype)
{
    switch (qtype) {
    case YVEX_QUANT_QTYPE_F32: return YVEX_DTYPE_F32;
    case YVEX_QUANT_QTYPE_F16: return YVEX_DTYPE_F16;
    case YVEX_QUANT_QTYPE_BF16: return YVEX_DTYPE_BF16;
    case YVEX_QUANT_QTYPE_Q8_0: return YVEX_DTYPE_Q8_0;
    case YVEX_QUANT_QTYPE_Q4_0: return YVEX_DTYPE_Q4_0;
    case YVEX_QUANT_QTYPE_Q4_K: return YVEX_DTYPE_Q4_K;
    case YVEX_QUANT_QTYPE_Q5_K: return YVEX_DTYPE_Q5_K;
    case YVEX_QUANT_QTYPE_Q6_K: return YVEX_DTYPE_Q6_K;
    case YVEX_QUANT_QTYPE_Q2_K: return YVEX_DTYPE_Q2_K;
    case YVEX_QUANT_QTYPE_IQ2_XXS: return YVEX_DTYPE_IQ2_XXS;
    case YVEX_QUANT_QTYPE_IQ2_XS: return YVEX_DTYPE_IQ2_XS;
    case YVEX_QUANT_QTYPE_IQ3_XXS: return YVEX_DTYPE_IQ3_XXS;
    case YVEX_QUANT_QTYPE_IQ4_NL: return YVEX_DTYPE_IQ4_NL;
    case YVEX_QUANT_QTYPE_UNKNOWN:
    case YVEX_QUANT_QTYPE_OTHER:
        return YVEX_DTYPE_UNKNOWN;
    }
    return YVEX_DTYPE_UNKNOWN;
}

int yvex_quant_qtype_storage_supported(yvex_quant_qtype qtype)
{
    const yvex_dtype_info *info = yvex_dtype_get_info(yvex_quant_qtype_to_dtype(qtype));
    return info && info->is_supported_for_storage_accounting;
}

int yvex_quant_qtype_compute_supported(yvex_quant_qtype qtype)
{
    return qtype == YVEX_QUANT_QTYPE_F32;
}

yvex_tensor_role yvex_quant_role_from_name(const char *name)
{
    unsigned int i;

    if (!name) return YVEX_TENSOR_ROLE_UNKNOWN;
    for (i = 0; i <= (unsigned int)YVEX_TENSOR_ROLE_MOE_EXPERT_DOWN; ++i) {
        yvex_tensor_role role = (yvex_tensor_role)i;
        if (strcmp(name, yvex_tensor_role_name(role)) == 0) return role;
    }
    return YVEX_TENSOR_ROLE_UNKNOWN;
}

static void qp_refresh_summary(yvex_quant_policy *policy)
{
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status = policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->requires_imatrix) policy->summary.requires_imatrix_count++;
        if (rule->storage_supported) policy->summary.storage_supported_count++;
        if (rule->compute_supported) policy->summary.compute_supported_count++;
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        } else if (!rule->storage_supported || !rule->compute_supported || rule->requires_imatrix) {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
    }
}

int yvex_quant_policy_add_rule(yvex_quant_policy *policy,
                               yvex_quant_selector_kind selector_kind,
                               const char *selector,
                               yvex_tensor_role role,
                               yvex_quant_qtype qtype,
                               int requires_imatrix,
                               yvex_error *err)
{
    yvex_quant_policy_rule *next;
    yvex_quant_policy_rule *rule;

    if (!policy || !selector) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_add", "policy and selector are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (policy->rule_count == policy->rule_cap) {
        unsigned long long cap = policy->rule_cap == 0 ? 8u : policy->rule_cap * 2u;
        next = (yvex_quant_policy_rule *)realloc(policy->rules, (size_t)cap * sizeof(policy->rules[0]));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "rule allocation failed");
            return YVEX_ERR_NOMEM;
        }
        policy->rules = next;
        policy->rule_cap = cap;
    }
    rule = &policy->rules[policy->rule_count];
    memset(rule, 0, sizeof(*rule));
    rule->selector_kind = selector_kind;
    rule->selector = yvex_quant_policy_strdup(selector);
    if (!rule->selector) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_add", "selector allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rule->role = role;
    rule->qtype = qtype;
    rule->requires_imatrix = requires_imatrix ? 1 : 0;
    rule->storage_supported = yvex_quant_qtype_storage_supported(qtype);
    rule->compute_supported = yvex_quant_qtype_compute_supported(qtype);
    policy->rule_count++;
    qp_refresh_summary(policy);
    return YVEX_OK;
}

int yvex_quant_policy_open(yvex_quant_policy **out, const char *path, yvex_error *err)
{
    int rc = yvex_quant_policy_parse_json(out, path, err);
    if (rc == YVEX_OK) {
        rc = yvex_quant_policy_validate(*out, NULL, err);
    }
    return rc;
}

void yvex_quant_policy_close(yvex_quant_policy *policy)
{
    unsigned long long i;

    if (!policy) return;
    free(policy->name);
    free(policy->architecture);
    free(policy->source_kind);
    free(policy->template_path);
    for (i = 0; i < policy->rule_count; ++i) {
        free((char *)policy->rules[i].selector);
    }
    free(policy->rules);
    free(policy);
}

int yvex_quant_policy_write_json(const char *out_path,
                                 const yvex_quant_policy *policy,
                                 yvex_error *err)
{
    return yvex_quant_policy_write_json_file(out_path, policy, err);
}

int yvex_quant_policy_get_summary(const yvex_quant_policy *policy,
                                  yvex_quant_policy_summary *out,
                                  yvex_error *err)
{
    if (!policy || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_summary", "policy and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = policy->summary;
    return YVEX_OK;
}

unsigned long long yvex_quant_policy_rule_count(const yvex_quant_policy *policy)
{
    return policy ? policy->rule_count : 0;
}

const yvex_quant_policy_rule *yvex_quant_policy_rule_at(const yvex_quant_policy *policy,
                                                        unsigned long long index)
{
    if (!policy || index >= policy->rule_count) return NULL;
    return &policy->rules[index];
}



static yvex_quant_qtype yvex_quant_policy_template_qtype_from_dtype(yvex_dtype dtype)
{
    switch (dtype) {
    case YVEX_DTYPE_F32: return YVEX_QUANT_QTYPE_F32;
    case YVEX_DTYPE_F16: return YVEX_QUANT_QTYPE_F16;
    case YVEX_DTYPE_BF16: return YVEX_QUANT_QTYPE_BF16;
    case YVEX_DTYPE_Q8_0: return YVEX_QUANT_QTYPE_Q8_0;
    case YVEX_DTYPE_Q4_0: return YVEX_QUANT_QTYPE_Q4_0;
    case YVEX_DTYPE_Q4_K: return YVEX_QUANT_QTYPE_Q4_K;
    case YVEX_DTYPE_Q5_K: return YVEX_QUANT_QTYPE_Q5_K;
    case YVEX_DTYPE_Q6_K: return YVEX_QUANT_QTYPE_Q6_K;
    case YVEX_DTYPE_Q2_K: return YVEX_QUANT_QTYPE_Q2_K;
    case YVEX_DTYPE_IQ2_XXS: return YVEX_QUANT_QTYPE_IQ2_XXS;
    case YVEX_DTYPE_IQ2_XS: return YVEX_QUANT_QTYPE_IQ2_XS;
    case YVEX_DTYPE_IQ3_XXS: return YVEX_QUANT_QTYPE_IQ3_XXS;
    case YVEX_DTYPE_IQ4_NL: return YVEX_QUANT_QTYPE_IQ4_NL;
    default: return YVEX_QUANT_QTYPE_OTHER;
    }
}

static int qp_has_role_qtype(const yvex_quant_policy *policy,
                             yvex_tensor_role role,
                             yvex_quant_qtype qtype)
{
    unsigned long long i;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE &&
            rule->role == role && rule->qtype == qtype) {
            return 1;
        }
    }
    return 0;
}

int yvex_quant_policy_create_from_template(yvex_quant_policy **out,
                                           const char *template_path,
                                           const char *architecture,
                                           yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    yvex_quant_policy *policy = NULL;
    unsigned long long i;
    int rc;

    if (!out || !template_path || !architecture) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_derive", "out, template_path, and architecture are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    policy->name = yvex_quant_policy_strdup("template-derived-policy");
    policy->architecture = yvex_quant_policy_strdup(architecture);
    policy->source_kind = yvex_quant_policy_strdup("template-derived");
    policy->template_path = yvex_quant_policy_strdup(template_path);
    if (!policy->name || !policy->architecture || !policy->source_kind || !policy->template_path) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_derive", "policy string allocation failed");
        goto done;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) goto done;

    for (i = 0; i < yvex_tensor_table_count(tensors); ++i) {
        const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, i);
        yvex_quant_qtype qtype;

        if (!tensor) continue;
        qtype = yvex_quant_policy_template_qtype_from_dtype(tensor->dtype);
        if (tensor->role != YVEX_TENSOR_ROLE_UNKNOWN) {
            if (qp_has_role_qtype(policy, tensor->role, qtype)) continue;
            rc = yvex_quant_policy_add_rule(policy, YVEX_QUANT_SELECTOR_ROLE,
                                            yvex_tensor_role_name(tensor->role),
                                            tensor->role, qtype, 0, err);
        } else {
            rc = yvex_quant_policy_add_rule(policy, YVEX_QUANT_SELECTOR_TENSOR_NAME,
                                            tensor->name,
                                            YVEX_TENSOR_ROLE_UNKNOWN, qtype, 0, err);
        }
        if (rc != YVEX_OK) goto done;
    }
    rc = yvex_quant_policy_validate(policy, NULL, err);
    if (rc != YVEX_OK) goto done;
    *out = policy;
    policy = NULL;
    rc = YVEX_OK;

done:
    yvex_quant_policy_close(policy);
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}



typedef struct {
    const char *p;
    const char *end;
    const char *path;
    yvex_error *err;
} qp_json;

static void qj_skip_ws(qp_json *j)
{
    while (j->p < j->end && isspace((unsigned char)*j->p)) j->p++;
}

static int qj_fail(qp_json *j, const char *msg)
{
    yvex_error_setf(j->err, YVEX_ERR_FORMAT, "quant_policy_json", "%s in %s", msg, j->path);
    return YVEX_ERR_FORMAT;
}

static int qj_expect(qp_json *j, char ch)
{
    qj_skip_ws(j);
    if (j->p >= j->end || *j->p != ch) return qj_fail(j, "unexpected JSON token");
    j->p++;
    return YVEX_OK;
}

static char *qj_string(qp_json *j)
{
    char *out;
    size_t cap;
    size_t n = 0;

    qj_skip_ws(j);
    if (j->p >= j->end || *j->p != '"') {
        qj_fail(j, "expected JSON string");
        return NULL;
    }
    j->p++;
    cap = (size_t)(j->end - j->p) + 1u;
    out = (char *)malloc(cap);
    if (!out) {
        yvex_error_set(j->err, YVEX_ERR_NOMEM, "quant_policy_json", "string allocation failed");
        return NULL;
    }
    while (j->p < j->end) {
        char ch = *j->p++;
        if (ch == '"') {
            out[n] = '\0';
            return out;
        }
        if (ch == '\\') {
            if (j->p >= j->end) {
                free(out);
                qj_fail(j, "unterminated escape");
                return NULL;
            }
            ch = *j->p++;
            if (ch == '"' || ch == '\\' || ch == '/') out[n++] = ch;
            else if (ch == 'n') out[n++] = '\n';
            else if (ch == 'r') out[n++] = '\r';
            else if (ch == 't') out[n++] = '\t';
            else {
                free(out);
                qj_fail(j, "unsupported string escape");
                return NULL;
            }
        } else {
            out[n++] = ch;
        }
    }
    free(out);
    qj_fail(j, "unterminated string");
    return NULL;
}

static int yvex_quant_policy_json_skip_value(qp_json *j);

static int qj_skip_literal(qp_json *j, const char *lit)
{
    size_t n = strlen(lit);
    if ((size_t)(j->end - j->p) < n || strncmp(j->p, lit, n) != 0) {
        return qj_fail(j, "unexpected literal");
    }
    j->p += n;
    return YVEX_OK;
}

static int qj_skip_object(qp_json *j)
{
    int rc = qj_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == '}') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        char *key = qj_string(j);
        if (!key) return yvex_error_code(j->err);
        free(key);
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) return rc;
        rc = yvex_quant_policy_json_skip_value(j);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed object");
    }
    return qj_fail(j, "unterminated object");
}

static int qj_skip_array(qp_json *j)
{
    int rc = qj_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = yvex_quant_policy_json_skip_value(j);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed array");
    }
    return qj_fail(j, "unterminated array");
}

static int yvex_quant_policy_json_skip_value(qp_json *j)
{
    char *s;
    qj_skip_ws(j);
    if (j->p >= j->end) return qj_fail(j, "expected value");
    if (*j->p == '{') return qj_skip_object(j);
    if (*j->p == '[') return qj_skip_array(j);
    if (*j->p == '"') {
        s = qj_string(j);
        if (!s) return yvex_error_code(j->err);
        free(s);
        return YVEX_OK;
    }
    if (*j->p == 't') return qj_skip_literal(j, "true");
    if (*j->p == 'f') return qj_skip_literal(j, "false");
    if (*j->p == 'n') return qj_skip_literal(j, "null");
    while (j->p < j->end && (isdigit((unsigned char)*j->p) || *j->p == '-')) j->p++;
    return YVEX_OK;
}

static int qj_bool(qp_json *j, int *out)
{
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == 't') {
        int rc = qj_skip_literal(j, "true");
        *out = 1;
        return rc;
    }
    if (j->p < j->end && *j->p == 'f') {
        int rc = qj_skip_literal(j, "false");
        *out = 0;
        return rc;
    }
    return qj_fail(j, "expected boolean");
}

static int qj_parse_source(qp_json *j, yvex_quant_policy *policy)
{
    int rc = qj_expect(j, '{');
    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            return YVEX_OK;
        }
        key = qj_string(j);
        if (!key) return yvex_error_code(j->err);
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            return rc;
        }
        if (strcmp(key, "kind") == 0) {
            free(policy->source_kind);
            policy->source_kind = qj_string(j);
        } else if (strcmp(key, "template_path") == 0) {
            free(policy->template_path);
            policy->template_path = qj_string(j);
        } else {
            rc = yvex_quant_policy_json_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    return qj_fail(j, "unterminated source object");
}

static int qj_parse_rule(qp_json *j, yvex_quant_policy *policy)
{
    char *selector_kind = NULL;
    char *selector = NULL;
    char *qtype = NULL;
    int requires_imatrix = 0;
    yvex_quant_selector_kind kind;
    yvex_quant_qtype qt;
    yvex_tensor_role role = YVEX_TENSOR_ROLE_UNKNOWN;
    int rc = qj_expect(j, '{');

    if (rc != YVEX_OK) return rc;
    while (j->p < j->end) {
        char *key;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == '}') {
            j->p++;
            break;
        }
        key = qj_string(j);
        if (!key) {
            rc = yvex_error_code(j->err);
            goto done;
        }
        rc = qj_expect(j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto done;
        }
        if (strcmp(key, "selector_kind") == 0) {
            free(selector_kind);
            selector_kind = qj_string(j);
            if (!selector_kind) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "selector") == 0) {
            free(selector);
            selector = qj_string(j);
            if (!selector) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "qtype") == 0) {
            free(qtype);
            qtype = qj_string(j);
            if (!qtype) rc = yvex_error_code(j->err);
        } else if (strcmp(key, "requires_imatrix") == 0) {
            rc = qj_bool(j, &requires_imatrix);
        } else {
            rc = yvex_quant_policy_json_skip_value(j);
        }
        free(key);
        if (rc != YVEX_OK) goto done;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') j->p++;
    }
    if (!selector_kind || !selector || !qtype) {
        rc = qj_fail(j, "policy rule missing selector_kind, selector, or qtype");
        goto done;
    }
    kind = yvex_quant_selector_kind_from_name(selector_kind);
    qt = yvex_quant_qtype_from_name(qtype);
    if (kind == YVEX_QUANT_SELECTOR_ROLE) role = yvex_quant_role_from_name(selector);
    rc = yvex_quant_policy_add_rule(policy, kind, selector, role, qt, requires_imatrix, j->err);

done:
    free(selector_kind);
    free(selector);
    free(qtype);
    return rc;
}

static int qj_parse_rules(qp_json *j, yvex_quant_policy *policy)
{
    int rc = qj_expect(j, '[');
    if (rc != YVEX_OK) return rc;
    qj_skip_ws(j);
    if (j->p < j->end && *j->p == ']') {
        j->p++;
        return YVEX_OK;
    }
    while (j->p < j->end) {
        rc = qj_parse_rule(j, policy);
        if (rc != YVEX_OK) return rc;
        qj_skip_ws(j);
        if (j->p < j->end && *j->p == ',') {
            j->p++;
            continue;
        }
        if (j->p < j->end && *j->p == ']') {
            j->p++;
            return YVEX_OK;
        }
        return qj_fail(j, "malformed rules array");
    }
    return qj_fail(j, "unterminated rules array");
}

static int yvex_quant_policy_json_read_file(const char *path, char **out, unsigned long long *len, yvex_error *err)
{
    FILE *fp;
    long size;
    char *buf;

    fp = fopen(path, "rb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot open policy: %s", path);
        return YVEX_ERR_IO;
    }
    if (fseek(fp, 0, SEEK_END) != 0 || (size = ftell(fp)) < 0 || fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot size policy: %s", path);
        return YVEX_ERR_IO;
    }
    buf = (char *)malloc((size_t)size + 1u);
    if (!buf) {
        fclose(fp);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy read allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (fread(buf, 1, (size_t)size, fp) != (size_t)size) {
        free(buf);
        fclose(fp);
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot read policy: %s", path);
        return YVEX_ERR_IO;
    }
    fclose(fp);
    buf[size] = '\0';
    *out = buf;
    *len = (unsigned long long)size;
    return YVEX_OK;
}

int yvex_quant_policy_parse_json(yvex_quant_policy **out, const char *path, yvex_error *err)
{
    yvex_quant_policy *policy;
    qp_json j;
    char *buf = NULL;
    unsigned long long len = 0;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = yvex_quant_policy_json_read_file(path, &buf, &len, err);
    if (rc != YVEX_OK) return rc;
    policy = (yvex_quant_policy *)calloc(1, sizeof(*policy));
    if (!policy) {
        free(buf);
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy allocation failed");
        return YVEX_ERR_NOMEM;
    }
    j.p = buf;
    j.end = buf + len;
    j.path = path;
    j.err = err;
    rc = qj_expect(&j, '{');
    if (rc != YVEX_OK) goto fail;
    while (j.p < j.end) {
        char *key;
        qj_skip_ws(&j);
        if (j.p < j.end && *j.p == '}') {
            j.p++;
            break;
        }
        key = qj_string(&j);
        if (!key) {
            rc = yvex_error_code(err);
            goto fail;
        }
        rc = qj_expect(&j, ':');
        if (rc != YVEX_OK) {
            free(key);
            goto fail;
        }
        if (strcmp(key, "schema") == 0) {
            char *schema = qj_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.quant_policy.v1") != 0) {
                free(schema);
                free(key);
                rc = qj_fail(&j, "unsupported quant policy schema");
                goto fail;
            }
            free(schema);
        } else if (strcmp(key, "name") == 0) {
            free(policy->name);
            policy->name = qj_string(&j);
            if (!policy->name) rc = yvex_error_code(err);
        } else if (strcmp(key, "architecture") == 0) {
            free(policy->architecture);
            policy->architecture = qj_string(&j);
            if (!policy->architecture) rc = yvex_error_code(err);
        } else if (strcmp(key, "source") == 0) {
            rc = qj_parse_source(&j, policy);
        } else if (strcmp(key, "rules") == 0) {
            rc = qj_parse_rules(&j, policy);
        } else {
            rc = yvex_quant_policy_json_skip_value(&j);
        }
        free(key);
        if (rc != YVEX_OK) goto fail;
        qj_skip_ws(&j);
        if (j.p < j.end && *j.p == ',') j.p++;
    }
    if (!policy->name) policy->name = yvex_quant_policy_strdup("unnamed-policy");
    if (!policy->architecture) policy->architecture = yvex_quant_policy_strdup("unknown");
    if (!policy->name || !policy->architecture) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_json", "policy string allocation failed");
        goto fail;
    }
    *out = policy;
    free(buf);
    return YVEX_OK;

fail:
    yvex_quant_policy_close(policy);
    free(buf);
    return rc;
}

static void qj_write_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    fputc('"', fp);
    while (*p) {
        unsigned char ch = *p++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', fp);
            fputc((int)ch, fp);
        } else if (ch == '\n') {
            fputs("\\n", fp);
        } else if (ch == '\r') {
            fputs("\\r", fp);
        } else if (ch == '\t') {
            fputs("\\t", fp);
        } else if (ch < 32) {
            fprintf(fp, "\\u%04x", (unsigned int)ch);
        } else {
            fputc((int)ch, fp);
        }
    }
    fputc('"', fp);
}

int yvex_quant_policy_write_json_file(const char *out_path,
                                      const yvex_quant_policy *policy,
                                      yvex_error *err)
{
    FILE *fp;
    unsigned long long i;

    if (!out_path || !policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_json", "out_path and policy are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "cannot open output policy: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.quant_policy.v1\",\n");
    fprintf(fp, "  \"name\": ");
    qj_write_string(fp, policy->name);
    fprintf(fp, ",\n  \"architecture\": ");
    qj_write_string(fp, policy->architecture);
    fprintf(fp, ",\n");
    if (policy->source_kind || policy->template_path) {
        fprintf(fp, "  \"source\": {\n");
        fprintf(fp, "    \"kind\": ");
        qj_write_string(fp, policy->source_kind ? policy->source_kind : "template-derived");
        fprintf(fp, ",\n    \"template_path\": ");
        qj_write_string(fp, policy->template_path);
        fprintf(fp, "\n  },\n");
    }
    fprintf(fp, "  \"rules\": [\n");
    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        fprintf(fp, "    {\"selector_kind\": ");
        qj_write_string(fp, yvex_quant_selector_kind_name(rule->selector_kind));
        fprintf(fp, ", \"selector\": ");
        qj_write_string(fp, rule->selector);
        fprintf(fp, ", \"qtype\": ");
        qj_write_string(fp, yvex_quant_qtype_name(rule->qtype));
        fprintf(fp, ", \"requires_imatrix\": %s}%s\n",
                rule->requires_imatrix ? "true" : "false",
                i + 1u == policy->rule_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_policy_json", "failed closing output policy: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}



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



static yvex_quant_qtype yvex_quant_policy_validate_qtype_from_dtype(yvex_dtype dtype)
{
    switch (dtype) {
    case YVEX_DTYPE_F32: return YVEX_QUANT_QTYPE_F32;
    case YVEX_DTYPE_F16: return YVEX_QUANT_QTYPE_F16;
    case YVEX_DTYPE_BF16: return YVEX_QUANT_QTYPE_BF16;
    case YVEX_DTYPE_Q8_0: return YVEX_QUANT_QTYPE_Q8_0;
    case YVEX_DTYPE_Q4_0: return YVEX_QUANT_QTYPE_Q4_0;
    case YVEX_DTYPE_Q4_K: return YVEX_QUANT_QTYPE_Q4_K;
    case YVEX_DTYPE_Q5_K: return YVEX_QUANT_QTYPE_Q5_K;
    case YVEX_DTYPE_Q6_K: return YVEX_QUANT_QTYPE_Q6_K;
    case YVEX_DTYPE_Q2_K: return YVEX_QUANT_QTYPE_Q2_K;
    case YVEX_DTYPE_IQ2_XXS: return YVEX_QUANT_QTYPE_IQ2_XXS;
    case YVEX_DTYPE_IQ2_XS: return YVEX_QUANT_QTYPE_IQ2_XS;
    case YVEX_DTYPE_IQ3_XXS: return YVEX_QUANT_QTYPE_IQ3_XXS;
    case YVEX_DTYPE_IQ4_NL: return YVEX_QUANT_QTYPE_IQ4_NL;
    default: return YVEX_QUANT_QTYPE_OTHER;
    }
}

static void qp_set_summary(yvex_quant_policy *policy,
                           unsigned long long extra_issues,
                           int fatal)
{
    unsigned long long i;

    memset(&policy->summary, 0, sizeof(policy->summary));
    policy->summary.name = policy->name;
    policy->summary.architecture = policy->architecture;
    policy->summary.rule_count = policy->rule_count;
    policy->summary.status = policy->rule_count > 0 ? YVEX_QUANT_POLICY_STATUS_VALID : YVEX_QUANT_POLICY_STATUS_INVALID;
    policy->summary.issue_count = extra_issues;
    if (extra_issues > 0) policy->summary.status = fatal ? YVEX_QUANT_POLICY_STATUS_INVALID : YVEX_QUANT_POLICY_STATUS_PARTIAL;

    for (i = 0; i < policy->rule_count; ++i) {
        yvex_quant_policy_rule *rule = &policy->rules[i];
        rule->storage_supported = yvex_quant_qtype_storage_supported(rule->qtype);
        rule->compute_supported = yvex_quant_qtype_compute_supported(rule->qtype);
        if (rule->requires_imatrix) {
            policy->summary.requires_imatrix_count++;
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->storage_supported) policy->summary.storage_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->compute_supported) policy->summary.compute_supported_count++;
        else {
            policy->summary.issue_count++;
            if (policy->summary.status == YVEX_QUANT_POLICY_STATUS_VALID) {
                policy->summary.status = YVEX_QUANT_POLICY_STATUS_PARTIAL;
            }
        }
        if (rule->qtype == YVEX_QUANT_QTYPE_UNKNOWN ||
            rule->selector_kind == YVEX_QUANT_SELECTOR_UNKNOWN ||
            (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && rule->role == YVEX_TENSOR_ROLE_UNKNOWN)) {
            policy->summary.issue_count++;
            policy->summary.status = YVEX_QUANT_POLICY_STATUS_INVALID;
        }
    }
}

static int qp_match_pattern(const char *pattern, const char *name)
{
    const char *star;
    size_t prefix_len;
    size_t suffix_len;
    size_t name_len;

    if (!pattern || !name) return 0;
    if (strcmp(pattern, "*") == 0) return 1;
    star = strchr(pattern, '*');
    if (!star) return strcmp(pattern, name) == 0;
    prefix_len = (size_t)(star - pattern);
    suffix_len = strlen(star + 1);
    name_len = strlen(name);
    if (name_len < prefix_len + suffix_len) return 0;
    if (strncmp(pattern, name, prefix_len) != 0) return 0;
    if (suffix_len > 0 && strcmp(name + name_len - suffix_len, star + 1) != 0) return 0;
    return 1;
}

static int qp_validate_template(yvex_quant_policy *policy,
                                const char *template_path,
                                unsigned long long *issues,
                                yvex_error *err)
{
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_tensor_table *tensors = NULL;
    unsigned long long i;
    int rc;

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = template_path;
    artifact_options.readonly = 1;
    artifact_options.map = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, err);
    if (rc == YVEX_OK) rc = yvex_gguf_open(&gguf, artifact, err);
    if (rc == YVEX_OK) rc = yvex_tensor_table_from_gguf(&tensors, gguf, err);
    if (rc != YVEX_OK) goto done;

    for (i = 0; i < policy->rule_count; ++i) {
        const yvex_quant_policy_rule *rule = &policy->rules[i];
        unsigned long long j;
        int matched = 0;

        for (j = 0; j < yvex_tensor_table_count(tensors); ++j) {
            const yvex_tensor_info *tensor = yvex_tensor_table_at(tensors, j);
            int applies = 0;

            if (!tensor) continue;
            if (rule->selector_kind == YVEX_QUANT_SELECTOR_DEFAULT) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_ROLE && tensor->role == rule->role) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_NAME && strcmp(rule->selector, tensor->name) == 0) applies = 1;
            else if (rule->selector_kind == YVEX_QUANT_SELECTOR_TENSOR_PATTERN && qp_match_pattern(rule->selector, tensor->name)) applies = 1;
            if (!applies) continue;
            matched = 1;
            if (yvex_quant_policy_validate_qtype_from_dtype(tensor->dtype) != rule->qtype) {
                (*issues)++;
            }
        }
        if (!matched) (*issues)++;
    }

done:
    yvex_tensor_table_close(tensors);
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}

int yvex_quant_policy_validate(yvex_quant_policy *policy,
                               const char *template_path,
                               yvex_error *err)
{
    unsigned long long template_issues = 0;
    int rc = YVEX_OK;

    if (!policy) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_policy_validate", "policy is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!policy->name) policy->name = yvex_quant_policy_strdup("unnamed-policy");
    if (!policy->architecture) policy->architecture = yvex_quant_policy_strdup("unknown");
    if (!policy->name || !policy->architecture) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "quant_policy_validate", "policy string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (template_path) {
        rc = qp_validate_template(policy, template_path, &template_issues, err);
        if (rc != YVEX_OK) return rc;
    }
    qp_set_summary(policy, template_issues, policy->rule_count == 0);
    return YVEX_OK;
}


int yvex_quant_q8_0_available(void)
{
    return 0;
}
