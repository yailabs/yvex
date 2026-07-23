/* Owner: gguf.imatrix
 * Owns: immutable calibration-manifest documents, coverage, validation, and JSON IO.
 * Does not own: quantization policy, quantization jobs, numeric codecs, or artifacts.
 * Invariants: parsed and constructed manifests own all strings and publish only complete state.
 * Boundary: calibration evidence informs policy; it does not execute or admit quantization.
 * Purpose: own the imatrix document lifecycle as one independently testable resource boundary.
 * Inputs: typed manifest options or bounded JSON bytes admitted by the shared core parser.
 * Effects: allocates manifest state and performs explicit manifest file IO.
 * Failure: typed errors publish no partial manifest and cleanup releases every owned allocation. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yvex/internal/core.h>
#include <yvex/internal/gguf.h>
#include <yvex/internal/io.h>
#include <yvex/quant.h>

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

typedef struct {
    const char *key;
    size_t manifest_offset;
    size_t option_offset;
    const char *fallback;
} imatrix_text_field;

static const imatrix_text_field imatrix_text_fields[] = {
    {"name", offsetof(yvex_imatrix_manifest, name), offsetof(yvex_imatrix_manifest_options, name),
     "unnamed-imatrix"},
    {"architecture", offsetof(yvex_imatrix_manifest, architecture),
     offsetof(yvex_imatrix_manifest_options, architecture), "unknown"},
    {"source_manifest", offsetof(yvex_imatrix_manifest, source_manifest_path),
     offsetof(yvex_imatrix_manifest_options, source_manifest_path), ""},
    {"quant_policy", offsetof(yvex_imatrix_manifest, quant_policy_path),
     offsetof(yvex_imatrix_manifest_options, quant_policy_path), ""},
    {"path", offsetof(yvex_imatrix_manifest, imatrix_path),
     offsetof(yvex_imatrix_manifest_options, imatrix_path), ""},
    {"dataset", offsetof(yvex_imatrix_manifest, calibration_dataset),
     offsetof(yvex_imatrix_manifest_options, calibration_dataset), ""},
    {"command", offsetof(yvex_imatrix_manifest, calibration_command),
     offsetof(yvex_imatrix_manifest_options, calibration_command), ""},
    {"producer", offsetof(yvex_imatrix_manifest, producer),
     offsetof(yvex_imatrix_manifest_options, producer), ""},
};

typedef enum {
    IMATRIX_JSON_TEXT,
    IMATRIX_JSON_STATUS,
    IMATRIX_JSON_FORMAT
} imatrix_json_value_kind;

typedef struct {
    const char *prefix;
    const char *suffix;
    imatrix_json_value_kind kind;
    size_t offset;
} imatrix_json_field;

static const imatrix_json_field imatrix_json_fields[] = {
    {"  \"name\": ", ",\n", IMATRIX_JSON_TEXT, offsetof(yvex_imatrix_manifest, name)},
    {"  \"architecture\": ", ",\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, architecture)},
    {"  \"status\": ", ",\n", IMATRIX_JSON_STATUS, 0u},
    {"  \"format\": ", ",\n", IMATRIX_JSON_FORMAT, 0u},
    {"  \"source_manifest\": ", ",\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, source_manifest_path)},
    {"  \"quant_policy\": ", ",\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, quant_policy_path)},
    {"  \"imatrix\": {\n    \"path\": ", ",\n    \"external\": true\n  },\n",
     IMATRIX_JSON_TEXT, offsetof(yvex_imatrix_manifest, imatrix_path)},
    {"  \"calibration\": {\n    \"dataset\": ", ",\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, calibration_dataset)},
    {"    \"command\": ", ",\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, calibration_command)},
    {"    \"producer\": ", "\n  },\n", IMATRIX_JSON_TEXT,
     offsetof(yvex_imatrix_manifest, producer)},
};

/* Purpose: resolve one manifest-owned string slot by declarative field ordinal. */
static char **manifest_text(yvex_imatrix_manifest *manifest, size_t index) {
    return (char **)((unsigned char *)manifest + imatrix_text_fields[index].manifest_offset);
}

/* Purpose: project one declarative JSON field onto its immutable manifest value. */
static const char *manifest_json_value(const yvex_imatrix_manifest *manifest,
                                       const imatrix_json_field *field) {
    if (field->kind == IMATRIX_JSON_STATUS)
        return yvex_imatrix_status_name(manifest->declared_status);
    if (field->kind == IMATRIX_JSON_FORMAT)
        return yvex_imatrix_format_name(manifest->format);
    return *(const char *const *)((const unsigned char *)manifest + field->offset);
}

/* Purpose: resolve one immutable manifest-option string by declarative field ordinal. */
static const char *manifest_option_text(const yvex_imatrix_manifest_options *options,
                                        size_t index) {
    return *(const char *const *)((const unsigned char *)options +
                                 imatrix_text_fields[index].option_offset);
}

/* Purpose: locate one imatrix text field in an exact schema-owned interval. */
static char **manifest_text_find(yvex_imatrix_manifest *manifest, const char *key, size_t begin,
                                 size_t end) {
    size_t index;
    for (index = begin; index < end; ++index)
        if (strcmp(imatrix_text_fields[index].key, key) == 0)
            return manifest_text(manifest, index);
    return NULL;
}

static int manifest_add_coverage(yvex_imatrix_manifest *manifest, yvex_imatrix_coverage_kind kind,
                                 const char *selector, const char *purpose, yvex_error *err);

static int manifest_parse_json(yvex_imatrix_manifest **out, const char *path, yvex_error *err);

static int manifest_write_json_file(const char *out_path, const yvex_imatrix_manifest *manifest,
                                    yvex_error *err);

static void manifest_refresh_summary(const yvex_imatrix_manifest *manifest,
                                     yvex_imatrix_summary *summary);

typedef struct {
    int value;
    const char *name;
} quant_name;

/* Purpose: map one admitted enum value through a compact immutable name table. */
static const char *quant_name_of(const quant_name *rows, size_t count, int value,
                                 const char *fallback) {
    size_t i;
    for (i = 0u; i < count; ++i) {
        if (rows[i].value == value)
            return rows[i].name;
    }
    return fallback;
}

/* Purpose: resolve one exact document spelling without accepting partial matches. */
static int quant_value_of(const quant_name *rows, size_t count, const char *name, int fallback) {
    size_t i;
    if (!name)
        return fallback;
    for (i = 0u; i < count; ++i) {
        if (strcmp(rows[i].name, name) == 0)
            return rows[i].value;
    }
    return fallback;
}

#define QUANT_NAMES_COUNT(rows) (sizeof(rows) / sizeof((rows)[0]))

static const quant_name imatrix_status_names[] = {
    {YVEX_IMATRIX_STATUS_UNKNOWN, "unknown"},
    {YVEX_IMATRIX_STATUS_DECLARED, "declared"},
    {YVEX_IMATRIX_STATUS_PRESENT, "present"},
    {YVEX_IMATRIX_STATUS_MISSING, "missing"},
    {YVEX_IMATRIX_STATUS_INVALID, "invalid"},
    {YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT, "unsupported_format"},
};

static const quant_name imatrix_format_names[] = {
    {YVEX_IMATRIX_FORMAT_UNKNOWN, "unknown"},
    {YVEX_IMATRIX_FORMAT_LLAMA_CPP_DAT, "llama_cpp_dat"},
    {YVEX_IMATRIX_FORMAT_ROUTED_MOE_DAT, "routed_moe_dat"},
    {YVEX_IMATRIX_FORMAT_JSON_MANIFEST, "json_manifest"},
    {YVEX_IMATRIX_FORMAT_OTHER, "other"},
};

static const quant_name imatrix_coverage_names[] = {
    {YVEX_IMATRIX_COVERAGE_UNKNOWN, "unknown"},
    {YVEX_IMATRIX_COVERAGE_GLOBAL, "global"},
    {YVEX_IMATRIX_COVERAGE_TENSOR_PATTERN, "tensor_pattern"},
    {YVEX_IMATRIX_COVERAGE_TENSOR_ROLE, "tensor_role"},
    {YVEX_IMATRIX_COVERAGE_LAYER_RANGE, "layer_range"},
    {YVEX_IMATRIX_COVERAGE_EXPERT_GROUP, "expert_group"},
    {YVEX_IMATRIX_COVERAGE_ROUTED_MOE, "routed_moe"},
};

static const quant_name imatrix_issue_names[] = {
    {YVEX_IMATRIX_ISSUE_NONE, "none"},
    {YVEX_IMATRIX_ISSUE_FILE_MISSING, "file_missing"},
    {YVEX_IMATRIX_ISSUE_FORMAT_UNSUPPORTED, "format_unsupported"},
    {YVEX_IMATRIX_ISSUE_POLICY_REQUIRES_IMATRIX, "policy_requires_imatrix"},
    {YVEX_IMATRIX_ISSUE_POLICY_RULE_UNCOVERED, "policy_rule_uncovered"},
    {YVEX_IMATRIX_ISSUE_SOURCE_MISMATCH, "source_mismatch"},
    {YVEX_IMATRIX_ISSUE_FORMAT, "format"},
    {YVEX_IMATRIX_ISSUE_IO, "io"},
};

/* Purpose: render one calibration-manifest status through the canonical table.
 * Inputs: status enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to unknown.
 * Boundary: rendering cannot alter manifest admission. */
const char *yvex_imatrix_status_name(yvex_imatrix_status status) {
    return quant_name_of(imatrix_status_names, QUANT_NAMES_COUNT(imatrix_status_names), status,
                         "unknown");
}

/* Purpose: parse one exact calibration-manifest status spelling.
 * Inputs: nullable status text.
 * Effects: returns a value without allocation.
 * Failure: null or unknown text maps to the unknown status.
 * Boundary: enum parsing does not validate a manifest. */
yvex_imatrix_status yvex_imatrix_status_from_name(const char *name) {
    return (yvex_imatrix_status)quant_value_of(imatrix_status_names,
                                               QUANT_NAMES_COUNT(imatrix_status_names), name,
                                               YVEX_IMATRIX_STATUS_UNKNOWN);
}

/* Purpose: render one calibration format through the canonical table.
 * Inputs: format enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to unknown.
 * Boundary: naming does not admit calibration bytes. */
const char *yvex_imatrix_format_name(yvex_imatrix_format format) {
    return quant_name_of(imatrix_format_names, QUANT_NAMES_COUNT(imatrix_format_names), format,
                         "unknown");
}

/* Purpose: parse one exact calibration-format spelling.
 * Inputs: nullable format text.
 * Effects: returns a value without allocation.
 * Failure: null or unknown text maps to the unknown format.
 * Boundary: parsing a name does not inspect a calibration file. */
yvex_imatrix_format yvex_imatrix_format_from_name(const char *name) {
    return (yvex_imatrix_format)quant_value_of(imatrix_format_names,
                                               QUANT_NAMES_COUNT(imatrix_format_names), name,
                                               YVEX_IMATRIX_FORMAT_UNKNOWN);
}

/* Purpose: render one calibration coverage kind through the canonical table.
 * Inputs: coverage-kind enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to unknown.
 * Boundary: rendering does not establish tensor coverage. */
const char *yvex_imatrix_coverage_kind_name(yvex_imatrix_coverage_kind kind) {
    return quant_name_of(imatrix_coverage_names, QUANT_NAMES_COUNT(imatrix_coverage_names), kind,
                         "unknown");
}

/* Purpose: parse one exact calibration coverage-kind spelling.
 * Inputs: nullable coverage text.
 * Effects: returns a value without allocation.
 * Failure: null or unknown text maps to unknown coverage.
 * Boundary: parsing does not resolve a tensor selector. */
yvex_imatrix_coverage_kind yvex_imatrix_coverage_kind_from_name(const char *name) {
    return (yvex_imatrix_coverage_kind)quant_value_of(imatrix_coverage_names,
                                                      QUANT_NAMES_COUNT(imatrix_coverage_names),
                                                      name, YVEX_IMATRIX_COVERAGE_UNKNOWN);
}

/* Purpose: render one typed calibration issue for reports and callers.
 * Inputs: issue enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to format refusal.
 * Boundary: issue rendering does not classify new failures. */
const char *yvex_imatrix_issue_kind_name(yvex_imatrix_issue_kind issue) {
    return quant_name_of(imatrix_issue_names, QUANT_NAMES_COUNT(imatrix_issue_names), issue,
                         "format");
}

/* Purpose: append one owned coverage row to a mutable manifest under construction.
 * Inputs: manifest, known kind, optional selector and purpose, and typed error sink.
 * Effects: may grow the coverage array and takes owned copies of both strings.
 * Failure: invalid input or allocation failure leaves the row count unchanged.
 * Boundary: appending a declaration does not prove actual calibration coverage. */
static int manifest_add_coverage(yvex_imatrix_manifest *manifest, yvex_imatrix_coverage_kind kind,
                                 const char *selector, const char *purpose, yvex_error *err) {
    yvex_imatrix_coverage *next;
    yvex_imatrix_coverage *row;

    if (!manifest || kind == YVEX_IMATRIX_COVERAGE_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_coverage",
                       "manifest and known coverage kind are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (manifest->coverage_count == manifest->coverage_cap) {
        unsigned long long cap = manifest->coverage_cap == 0 ? 2u : manifest->coverage_cap * 2u;
        next = (yvex_imatrix_coverage *)realloc(manifest->coverage,
                                                (size_t)cap * sizeof(manifest->coverage[0]));
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
    row->selector = yvex_core_strdup(selector);
    row->purpose = yvex_core_strdup(purpose);
    if (!row->selector || !row->purpose) {
        free(row->selector);
        free(row->purpose);
        memset(row, 0, sizeof(*row));
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_coverage",
                       "coverage string allocation failed");
        return YVEX_ERR_NOMEM;
    }
    manifest->coverage_count++;
    return YVEX_OK;
}

/* Purpose: construct one owned calibration manifest from typed options.
 * Inputs: output slot, required options, and typed error sink.
 * Effects: allocates the manifest, strings, default coverage, and summary state.
 * Failure: invalid input or any allocation failure leaves the output null.
 * Boundary: construction records calibration intent but does not read calibration data. */
int yvex_imatrix_manifest_create(yvex_imatrix_manifest **out,
                                 const yvex_imatrix_manifest_options *options, yvex_error *err) {
    yvex_imatrix_manifest *manifest;
    size_t index;
    int rc;

    if (!out || !options || !options->name || !options->architecture || !options->imatrix_path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_create",
                       "out, name, architecture, and imatrix path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    manifest = (yvex_imatrix_manifest *)calloc(1, sizeof(*manifest));
    if (!manifest) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_create", "manifest allocation failed");
        return YVEX_ERR_NOMEM;
    }
    for (index = 0u; index < sizeof(imatrix_text_fields) / sizeof(imatrix_text_fields[0]); ++index)
        *manifest_text(manifest, index) = yvex_core_strdup(manifest_option_text(options, index));
    manifest->format = options->format;
    manifest->declared_status = options->status;
    for (index = 0u; index < sizeof(imatrix_text_fields) / sizeof(imatrix_text_fields[0]); ++index)
        if (!*manifest_text(manifest, index)) {
            yvex_imatrix_manifest_close(manifest);
            yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_create",
                           "manifest string allocation failed");
            return YVEX_ERR_NOMEM;
        }
    rc = manifest_add_coverage(manifest, YVEX_IMATRIX_COVERAGE_ROUTED_MOE, "blk.*.ffn.experts.*",
                               "expert-aware quantization weighting", err);
    if (rc != YVEX_OK) {
        yvex_imatrix_manifest_close(manifest);
        return rc;
    }
    manifest_refresh_summary(manifest, &manifest->summary);
    *out = manifest;
    return YVEX_OK;
}

/* Purpose: release an owned calibration manifest and every nested allocation.
 * Inputs: nullable manifest owner.
 * Effects: frees strings, coverage rows, and the manifest object.
 * Failure: cannot report failure; null is a no-op.
 * Boundary: callers must not retain borrowed summary strings after close. */
void yvex_imatrix_manifest_close(yvex_imatrix_manifest *manifest) {
    unsigned long long i;
    size_t index;

    if (!manifest)
        return;
    for (index = 0u; index < sizeof(imatrix_text_fields) / sizeof(imatrix_text_fields[0]); ++index)
        free(*manifest_text(manifest, index));
    for (i = 0; i < manifest->coverage_count; ++i) {
        free(manifest->coverage[i].selector);
        free(manifest->coverage[i].purpose);
    }
    free(manifest->coverage);
    free(manifest);
}

/* Purpose: serialize one admitted manifest through the canonical JSON writer.
 * Inputs: destination path, immutable manifest, and typed error sink.
 * Effects: writes or replaces the requested document according to writer semantics.
 * Failure: typed argument or I/O errors never report successful publication.
 * Boundary: serialization does not create calibration evidence. */
int yvex_imatrix_manifest_write_json(const char *out_path, const yvex_imatrix_manifest *manifest,
                                     yvex_error *err) {
    return manifest_write_json_file(out_path, manifest, err);
}

/* Purpose: parse and validate one bounded calibration-manifest document.
 * Inputs: output slot, source path, and typed error sink.
 * Effects: allocates an owned manifest only after successful parsing.
 * Failure: malformed, unavailable, or invalid documents leave no successful output.
 * Boundary: opening the manifest does not read the referenced imatrix payload. */
int yvex_imatrix_manifest_open(yvex_imatrix_manifest **out, const char *path, yvex_error *err) {
    int rc = manifest_parse_json(out, path, err);
    if (rc == YVEX_OK) {
        rc = yvex_imatrix_manifest_validate(*out, err);
    }
    return rc;
}

/* Purpose: project the current immutable manifest summary into caller storage.
 * Inputs: manifest, output summary, and typed error sink.
 * Effects: copies a summary whose strings remain borrowed from the manifest.
 * Failure: null inputs return a typed invalid-argument result.
 * Boundary: a summary is evidence projection, not calibration admission. */
int yvex_imatrix_manifest_get_summary(const yvex_imatrix_manifest *manifest,
                                      yvex_imatrix_summary *out, yvex_error *err) {
    if (!manifest || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_summary",
                       "manifest and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = manifest->summary;
    return YVEX_OK;
}

/* Purpose: derive manifest status and issue counters from owned document facts.
 * Inputs: immutable manifest and caller-owned summary.
 * Effects: refreshes borrowed views and probes the declared imatrix path for existence.
 * Failure: null input is ignored; absence is recorded as data rather than raised.
 * Boundary: existence probing does not validate imatrix format or numeric contents. */
static void manifest_refresh_summary(const yvex_imatrix_manifest *manifest,
                                     yvex_imatrix_summary *summary) {
    if (!manifest || !summary)
        return;
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
        summary->status =
            summary->file_exists ? YVEX_IMATRIX_STATUS_PRESENT : YVEX_IMATRIX_STATUS_MISSING;
    }
    if (!summary->file_exists) {
        summary->issue_count++;
        if (summary->status == YVEX_IMATRIX_STATUS_PRESENT)
            summary->status = YVEX_IMATRIX_STATUS_MISSING;
    }
    if (manifest->format == YVEX_IMATRIX_FORMAT_UNKNOWN) {
        summary->issue_count++;
        summary->status = YVEX_IMATRIX_STATUS_UNSUPPORTED_FORMAT;
    }
}

/* Purpose: parse one known nested imatrix or calibration object.
 * Inputs: bounded cursor, manifest under construction, and admitted object name.
 * Effects: replaces owned manifest strings and skips unknown fields structurally.
 * Failure: malformed JSON or allocation failure leaves typed error context.
 * Boundary: parsing records paths and provenance but does not open referenced payloads. */
static int ij_parse_named_object(yvex_gguf_json *j, yvex_imatrix_manifest *manifest,
                                 const char *object_name) {
    size_t begin = strcmp(object_name, "imatrix") == 0 ? 4u : 5u;
    size_t end = begin == 4u ? 5u : 8u;
    int rc = yvex_gguf_json_expect(j, '{');
    if (rc != YVEX_OK)
        return rc;
    while (j->cursor.cursor < j->cursor.end) {
        char *key = NULL;
        int complete = 0;

        rc = yvex_gguf_json_member(j, &key, &complete);
        if (rc != YVEX_OK) return rc;
        if (complete) return YVEX_OK;
        {
            char **target = manifest_text_find(manifest, key, begin, end);
            if (target) {
                char *value = yvex_gguf_json_string(j);
                if (!value)
                    rc = yvex_error_code(j->err);
                else {
                    free(*target);
                    *target = value;
                }
            } else {
                rc = yvex_gguf_json_skip(j);
            }
        }
        free(key);
        if (rc != YVEX_OK)
            return rc;
        yvex_gguf_json_optional_comma(j);
    }
    return yvex_gguf_json_fail(j, "unterminated nested object");
}

/* Purpose: parse and append one calibration coverage declaration.
 * Inputs: bounded cursor and manifest under construction.
 * Effects: allocates temporary fields and one owned manifest coverage row.
 * Failure: malformed rows or allocation failure append no partial row.
 * Boundary: declared coverage remains distinct from measured calibration evidence. */
static int ij_parse_coverage_row(yvex_gguf_json *j, void *context) {
    yvex_imatrix_manifest *manifest = context;
    char *kind = NULL;
    char *selector = NULL;
    char *purpose = NULL;
    int rc = yvex_gguf_json_expect(j, '{');

    if (rc != YVEX_OK)
        return rc;
    while (j->cursor.cursor < j->cursor.end) {
        char *key = NULL;
        int complete = 0;

        rc = yvex_gguf_json_member(j, &key, &complete);
        if (rc != YVEX_OK) goto done;
        if (complete) break;
        if (strcmp(key, "kind") == 0)
            kind = yvex_gguf_json_string(j);
        else if (strcmp(key, "selector") == 0)
            selector = yvex_gguf_json_string(j);
        else if (strcmp(key, "purpose") == 0)
            purpose = yvex_gguf_json_string(j);
        else
            rc = yvex_gguf_json_skip(j);
        free(key);
        if (rc != YVEX_OK)
            goto done;
        yvex_gguf_json_optional_comma(j);
    }
    rc = manifest_add_coverage(manifest, yvex_imatrix_coverage_kind_from_name(kind),
                               selector ? selector : "", purpose ? purpose : "", j->err);
done:
    free(kind);
    free(selector);
    free(purpose);
    return rc;
}

/* Purpose: parse one complete bounded JSON document into an owned manifest.
 * Inputs: output slot, source path, and typed error sink.
 * Effects: reads metadata bytes, allocates manifest state, and refreshes its summary.
 * Failure: schema, grammar, I/O, or allocation failure releases all partial state.
 * Boundary: document parsing performs zero reads from the referenced imatrix payload. */
static int manifest_parse_json(yvex_imatrix_manifest **out, const char *path, yvex_error *err) {
    yvex_imatrix_manifest *manifest;
    yvex_gguf_json j;
    int rc;

    if (!out || !path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json", "out and path are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    rc = yvex_gguf_json_open(&j, path, "imatrix_json", err);
    if (rc != YVEX_OK)
        return rc;
    manifest = (yvex_imatrix_manifest *)calloc(1, sizeof(*manifest));
    if (!manifest) {
        yvex_gguf_json_close(&j);
        yvex_error_set(err, YVEX_ERR_NOMEM, "imatrix_json", "manifest allocation failed");
        return YVEX_ERR_NOMEM;
    }
    rc = yvex_gguf_json_expect(&j, '{');
    if (rc != YVEX_OK)
        goto fail;
    while (j.cursor.cursor < j.cursor.end) {
        char *key = NULL;
        int complete = 0;

        rc = yvex_gguf_json_member(&j, &key, &complete);
        if (rc != YVEX_OK) goto fail;
        if (complete) break;
        if (strcmp(key, "schema") == 0) {
            char *schema = yvex_gguf_json_string(&j);
            if (!schema) {
                free(key);
                rc = yvex_error_code(err);
                goto fail;
            }
            if (strcmp(schema, "yvex.imatrix_manifest.v1") != 0) {
                free(schema);
                free(key);
                rc = yvex_gguf_json_fail(&j, "unsupported imatrix schema");
                goto fail;
            }
            free(schema);
        } else if (manifest_text_find(manifest, key, 0u, 4u)) {
            char **target = manifest_text_find(manifest, key, 0u, 4u);
            char *value = yvex_gguf_json_string(&j);
            if (!value)
                rc = yvex_error_code(err);
            else {
                free(*target);
                *target = value;
            }
        } else if (strcmp(key, "status") == 0) {
            char *status = yvex_gguf_json_string(&j);
            if (!status)
                rc = yvex_error_code(err);
            else {
                manifest->declared_status = yvex_imatrix_status_from_name(status);
                free(status);
            }
        } else if (strcmp(key, "format") == 0) {
            char *format = yvex_gguf_json_string(&j);
            if (!format)
                rc = yvex_error_code(err);
            else {
                manifest->format = yvex_imatrix_format_from_name(format);
                free(format);
            }
        } else if (strcmp(key, "imatrix") == 0) {
            rc = ij_parse_named_object(&j, manifest, "imatrix");
        } else if (strcmp(key, "calibration") == 0) {
            rc = ij_parse_named_object(&j, manifest, "calibration");
        } else if (strcmp(key, "coverage") == 0) {
            rc = yvex_gguf_json_array(&j, ij_parse_coverage_row, manifest,
                                      "malformed coverage array",
                                      "unterminated coverage array");
        } else {
            rc = yvex_gguf_json_skip(&j);
        }
        free(key);
        if (rc != YVEX_OK)
            goto fail;
        yvex_gguf_json_optional_comma(&j);
    }
    {
        size_t index;
        for (index = 0u; index < sizeof(imatrix_text_fields) / sizeof(imatrix_text_fields[0]);
             ++index) {
            char **field = manifest_text(manifest, index);
            if (!*field)
                *field = yvex_core_strdup(imatrix_text_fields[index].fallback);
            if (!*field) {
                rc = YVEX_ERR_NOMEM;
                yvex_error_set(err, rc, "imatrix_json", "manifest string allocation failed");
                goto fail;
            }
        }
    }
    manifest_refresh_summary(manifest, &manifest->summary);
    *out = manifest;
    yvex_gguf_json_close(&j);
    return YVEX_OK;
fail:
    yvex_imatrix_manifest_close(manifest);
    yvex_gguf_json_close(&j);
    return rc;
}

/* Purpose: serialize one complete calibration manifest deterministically.
 * Inputs: destination path, immutable manifest, and typed error sink.
 * Effects: creates or replaces the requested JSON file and closes its stream.
 * Failure: invalid arguments, open failure, or close failure return typed errors.
 * Boundary: the writer publishes a manifest document, never calibration tensor data. */
static int manifest_write_json_file(const char *out_path, const yvex_imatrix_manifest *manifest,
                                    yvex_error *err) {
    FILE *fp;
    unsigned long long i;
    size_t field;

    if (!out_path || !manifest) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_json",
                       "out_path and manifest are required");
        return YVEX_ERR_INVALID_ARG;
    }
    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "cannot open output manifest: %s",
                        out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema\": \"yvex.imatrix_manifest.v1\",\n");
    for (field = 0u; field < sizeof(imatrix_json_fields) / sizeof(imatrix_json_fields[0]); ++field) {
        fputs(imatrix_json_fields[field].prefix, fp);
        yvex_file_json_write_string(fp, manifest_json_value(manifest, &imatrix_json_fields[field]));
        fputs(imatrix_json_fields[field].suffix, fp);
    }
    fprintf(fp, "  \"coverage\": [\n");
    for (i = 0; i < manifest->coverage_count; ++i) {
        const yvex_imatrix_coverage *row = &manifest->coverage[i];
        fprintf(fp, "    {\"kind\": ");
        yvex_file_json_write_string(fp, yvex_imatrix_coverage_kind_name(row->kind));
        fprintf(fp, ", \"selector\": ");
        yvex_file_json_write_string(fp, row->selector);
        fprintf(fp, ", \"purpose\": ");
        yvex_file_json_write_string(fp, row->purpose);
        fprintf(fp, "}%s\n", i + 1u == manifest->coverage_count ? "" : ",");
    }
    fprintf(fp, "  ]\n}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "imatrix_json", "failed closing output manifest: %s",
                        out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}

/* Purpose: validate calibration availability and policy coverage for one manifest.
 * Inputs: immutable manifest and typed error sink.
 * Effects: refreshes its owned summary and may read a referenced policy document.
 * Failure: invalid input returns typed refusal; unavailable evidence remains summary state.
 * Boundary: validation does not decode or numerically consume imatrix data. */
int yvex_imatrix_manifest_validate(const yvex_imatrix_manifest *manifest, yvex_error *err) {
    yvex_imatrix_summary *summary;

    if (!manifest) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "imatrix_validate", "manifest is required");
        return YVEX_ERR_INVALID_ARG;
    }
    summary = (yvex_imatrix_summary *)&manifest->summary;
    manifest_refresh_summary(manifest, summary);

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
