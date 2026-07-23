/* Owner: gguf.quant_job
 * Owns: quantization-job documents, validation summaries, and deterministic JSON IO.
 * Does not own: quantization policy, calibration manifests, numeric execution, or GGUF writing.
 * Invariants: summaries borrow from one complete owned document and malformed JSON fails closed.
 * Boundary: a quantization job records orchestration facts; it does not prove numeric execution.
 * Purpose: own the job-document lifecycle without embedding a second JSON grammar.
 * Inputs: typed job options or bounded JSON bytes admitted by the shared core parser.
 * Effects: owns summary strings and performs explicit job-manifest file IO.
 * Failure: typed errors retain no partial document and do not promote job status. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <yvex/internal/core.h>
#include <yvex/internal/io.h>
#include <yvex/quant.h>

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

typedef struct {
    const char *key;
    size_t document_offset;
    size_t option_offset;
} job_text_field;

static const job_text_field job_text_fields[] = {
    {"name", offsetof(yvex_quant_job_doc, name), offsetof(yvex_quant_job_options, name)},
    {"architecture", offsetof(yvex_quant_job_doc, architecture),
     offsetof(yvex_quant_job_options, architecture)},
    {"tool_path", offsetof(yvex_quant_job_doc, tool_path),
     offsetof(yvex_quant_job_options, tool_path)},
    {"source_manifest", offsetof(yvex_quant_job_doc, source_manifest_path),
     offsetof(yvex_quant_job_options, source_manifest_path)},
    {"native_source", offsetof(yvex_quant_job_doc, native_source_dir),
     offsetof(yvex_quant_job_options, native_source_dir)},
    {"template", offsetof(yvex_quant_job_doc, template_path),
     offsetof(yvex_quant_job_options, template_path)},
    {"quant_policy", offsetof(yvex_quant_job_doc, quant_policy_path),
     offsetof(yvex_quant_job_options, quant_policy_path)},
    {"imatrix_manifest", offsetof(yvex_quant_job_doc, imatrix_manifest_path),
     offsetof(yvex_quant_job_options, imatrix_manifest_path)},
    {"imatrix", offsetof(yvex_quant_job_doc, imatrix_path),
     offsetof(yvex_quant_job_options, imatrix_path)},
    {"gguf", offsetof(yvex_quant_job_doc, out_gguf_path),
     offsetof(yvex_quant_job_options, out_gguf_path)},
    {"log", offsetof(yvex_quant_job_doc, log_path), offsetof(yvex_quant_job_options, log_path)},
    {"command", offsetof(yvex_quant_job_doc, command),
     offsetof(yvex_quant_job_options, command)},
};

/* Purpose: resolve one job document string slot by its declarative field ordinal. */
static char **job_document_text(yvex_quant_job_doc *doc, size_t index) {
    return (char **)((unsigned char *)doc + job_text_fields[index].document_offset);
}

/* Purpose: resolve one immutable options string by its declarative field ordinal. */
static const char *job_option_text(const yvex_quant_job_options *options, size_t index) {
    return *(const char *const *)((const unsigned char *)options + job_text_fields[index].option_offset);
}

/* Purpose: locate one declared job field in an exact bounded index interval. */
static char **job_field_find(yvex_quant_job_doc *doc, const char *key, size_t begin, size_t end) {
    size_t index;
    for (index = begin; index < end; ++index)
        if (strcmp(job_text_fields[index].key, key) == 0)
            return job_document_text(doc, index);
    return NULL;
}

static yvex_quant_job_doc last_summary_doc;

typedef struct {
    int value;
    const char *name;
} job_name;

static const job_name job_status_names[] = {
    {YVEX_QUANT_JOB_STATUS_UNKNOWN, "unknown"},
    {YVEX_QUANT_JOB_STATUS_DECLARED, "declared"},
    {YVEX_QUANT_JOB_STATUS_READY, "ready"},
    {YVEX_QUANT_JOB_STATUS_RUNNING, "running"},
    {YVEX_QUANT_JOB_STATUS_SUCCEEDED, "succeeded"},
    {YVEX_QUANT_JOB_STATUS_FAILED, "failed"},
    {YVEX_QUANT_JOB_STATUS_SKIPPED, "skipped"},
};
static const job_name job_tool_names[] = {
    {YVEX_QUANT_JOB_TOOL_UNKNOWN, "unknown"},
    {YVEX_QUANT_JOB_TOOL_YVEX_INTERNAL, "yvex-internal"},
    {YVEX_QUANT_JOB_TOOL_EXTERNAL, "external"},
};

/* Purpose: map one exact job enum through a bounded immutable table. */
static const char *job_name_of(const job_name *rows, size_t count, int value) {
    size_t index;
    for (index = 0u; index < count; ++index)
        if (rows[index].value == value)
            return rows[index].name;
    return "unknown";
}

/* Purpose: parse one exact job spelling without accepting prefixes or aliases. */
static int job_value_of(const job_name *rows, size_t count, const char *name) {
    size_t index;
    if (name)
        for (index = 0u; index < count; ++index)
            if (strcmp(rows[index].name, name) == 0)
                return rows[index].value;
    return 0;
}

static void job_doc_clear(yvex_quant_job_doc *doc);
static int job_parse_json_file(const char *path, yvex_quant_job_doc *doc, yvex_error *err);
static int job_write_json_file(const char *out_path, const yvex_quant_job_options *options,
                               yvex_error *err);
static void job_summarize(const yvex_quant_job_doc *doc, yvex_quant_job_summary *summary);

/* Purpose: emit one contiguous range of declarative job-option string fields.
 * Inputs: open stream, immutable options, indentation, admitted interval, and trailing-comma fact.
 * Effects: appends fields in schema order with exact comma placement.
 * Failure: stream failures are observed by the enclosing document writer.
 * Boundary: formatting projects typed options and owns no job policy. */
static void job_write_fields(FILE *file, const yvex_quant_job_options *options,
                             const char *indent, size_t begin, size_t end, int trailing_comma) {
    size_t index;
    for (index = begin; index < end; ++index)
        yvex_file_json_write_field(file, indent, job_text_fields[index].key,
                                   job_option_text(options, index),
                                   index + 1u < end || trailing_comma);
}

/* Purpose: transfer one complete job document into process-lifetime summary storage. */
static void store_summary_doc(yvex_quant_job_doc *doc) {
    job_doc_clear(&last_summary_doc);
    last_summary_doc = *doc;
    memset(doc, 0, sizeof(*doc));
}

/* Purpose: release every owned string in one mutable job document.
 * Inputs: nullable document owner.
 * Effects: frees nested allocations and zeroes the document.
 * Failure: cannot report failure; null is a no-op.
 * Boundary: callers must not retain summary views into the cleared document. */
static void job_doc_clear(yvex_quant_job_doc *doc) {
    size_t index;
    if (!doc)
        return;
    for (index = 0u; index < sizeof(job_text_fields) / sizeof(job_text_fields[0]); ++index)
        free(*job_document_text(doc, index));
    memset(doc, 0, sizeof(*doc));
}

/* Purpose: render one quantization-job lifecycle status.
 * Inputs: status enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to unknown.
 * Boundary: rendering cannot advance the job lifecycle. */
const char *yvex_quant_job_status_name(yvex_quant_job_status status) {
    return job_name_of(job_status_names, sizeof(job_status_names) / sizeof(job_status_names[0]),
                       status);
}

/* Purpose: parse one exact quantization-job status spelling.
 * Inputs: nullable status text.
 * Effects: returns a value without allocation.
 * Failure: null or unknown text maps to unknown.
 * Boundary: parsing does not validate a job document. */
yvex_quant_job_status yvex_quant_job_status_from_name(const char *name) {
    return (yvex_quant_job_status)job_value_of(
        job_status_names, sizeof(job_status_names) / sizeof(job_status_names[0]), name);
}

/* Purpose: render one quantization tool class.
 * Inputs: tool enum.
 * Effects: returns borrowed immutable text.
 * Failure: unknown values map to unknown.
 * Boundary: rendering does not admit an executable. */
const char *yvex_quant_job_tool_name(yvex_quant_job_tool tool) {
    return job_name_of(job_tool_names, sizeof(job_tool_names) / sizeof(job_tool_names[0]), tool);
}

/* Purpose: parse one exact quantization-tool spelling.
 * Inputs: nullable tool text.
 * Effects: returns a value without allocation.
 * Failure: null or unknown text maps to unknown.
 * Boundary: parsing does not inspect a tool path. */
yvex_quant_job_tool yvex_quant_job_tool_from_name(const char *name) {
    return (yvex_quant_job_tool)job_value_of(
        job_tool_names, sizeof(job_tool_names) / sizeof(job_tool_names[0]), name);
}

/* Purpose: project owned job fields and current path-existence facts into a borrowed summary.
 * Inputs: immutable job document and caller-owned summary.
 * Effects: replaces summary fields and probes declared paths for existence.
 * Failure: null input is ignored; missing paths remain typed summary facts.
 * Boundary: existence probes do not inspect source or output contents. */
static void job_summarize(const yvex_quant_job_doc *doc, yvex_quant_job_summary *summary) {
    if (!doc || !summary)
        return;
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
    summary->source_exists = doc->native_source_dir && doc->native_source_dir[0] &&
                             access(doc->native_source_dir, F_OK) == 0;
    summary->template_exists =
        doc->template_path && doc->template_path[0] && access(doc->template_path, F_OK) == 0;
    summary->imatrix_exists =
        doc->imatrix_path && doc->imatrix_path[0] && access(doc->imatrix_path, F_OK) == 0;
    summary->output_exists =
        doc->out_gguf_path && doc->out_gguf_path[0] && access(doc->out_gguf_path, F_OK) == 0;
}

/* Purpose: copy typed job options into one owned mutable document.
 * Inputs: immutable options, destination document, and typed error sink.
 * Effects: allocates owned strings and initializes tool and status fields.
 * Failure: invalid input or allocation failure clears all partial state.
 * Boundary: option capture neither executes a tool nor validates referenced files. */
static int options_to_doc(const yvex_quant_job_options *options, yvex_quant_job_doc *doc,
                          yvex_error *err) {
    size_t index;
    memset(doc, 0, sizeof(*doc));
    if (!options || !options->name || !options->architecture || !options->tool_path ||
        !options->native_source_dir || !options->template_path || !options->out_gguf_path ||
        !options->log_path || !options->command) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job",
                       "name, architecture, tool path, native source, template, "
                       "output GGUF, log, and command are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (options->status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "known status is required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (index = 0u; index < sizeof(job_text_fields) / sizeof(job_text_fields[0]); ++index)
        *job_document_text(doc, index) = yvex_core_strdup(job_option_text(options, index));
    doc->tool = options->tool;
    doc->status = options->status;
    for (index = 0u; index < sizeof(job_text_fields) / sizeof(job_text_fields[0]); ++index)
        if (!*job_document_text(doc, index)) {
            job_doc_clear(doc);
            yvex_error_set(err, YVEX_ERR_NOMEM, "quant_job", "manifest string allocation failed");
            return YVEX_ERR_NOMEM;
        }
    return YVEX_OK;
}

/* Purpose: publish one deterministic quantization-job JSON document.
 * Inputs: destination path, typed job options, optional summary, and error sink.
 * Effects: writes the file and retains one owned process-lifetime summary document.
 * Failure: invalid input or I/O failure releases temporary state and reports no success.
 * Boundary: job serialization records orchestration; it does not execute quantization. */
int yvex_quant_job_write_json(const char *out_path, const yvex_quant_job_options *options,
                              yvex_quant_job_summary *summary_out, yvex_error *err) {
    yvex_quant_job_doc doc;
    int rc;

    if (!out_path || !out_path[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job", "output path is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = options_to_doc(options, &doc, err);
    if (rc != YVEX_OK)
        return rc;
    rc = job_write_json_file(out_path, options, err);
    if (rc == YVEX_OK) {
        store_summary_doc(&doc);
        if (summary_out)
            job_summarize(&last_summary_doc, summary_out);
    } else {
        job_doc_clear(&doc);
    }
    return rc;
}

/* Purpose: parse and validate a quantization-job document and referenced path facts.
 * Inputs: manifest path, caller-owned summary, and typed error sink.
 * Effects: reads bounded JSON and retains the accepted summary document.
 * Failure: malformed identity or inconsistent succeeded state leaves no accepted summary.
 * Boundary: validation does not run the declared command or inspect output payload. */
int yvex_quant_job_validate(const char *manifest_path, yvex_quant_job_summary *summary_out,
                            yvex_error *err) {
    yvex_quant_job_doc doc;
    yvex_quant_job_summary summary;
    int rc;

    if (!manifest_path || !summary_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_validate",
                       "manifest path and summary are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(&doc, 0, sizeof(doc));
    rc = job_parse_json_file(manifest_path, &doc, err);
    if (rc != YVEX_OK)
        return rc;
    job_summarize(&doc, &summary);
    *summary_out = summary;
    if (!summary.name || !summary.name[0] || !summary.architecture || !summary.architecture[0] ||
        doc.status == YVEX_QUANT_JOB_STATUS_UNKNOWN) {
        job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_FORMAT, "quant_job_validate",
                       "manifest is missing required identity fields");
        return YVEX_ERR_FORMAT;
    }
    if (doc.status == YVEX_QUANT_JOB_STATUS_SUCCEEDED && !summary.output_exists) {
        job_doc_clear(&doc);
        yvex_error_set(err, YVEX_ERR_STATE, "quant_job_validate",
                       "succeeded quant job requires output GGUF to exist");
        return YVEX_ERR_STATE;
    }
    store_summary_doc(&doc);
    job_summarize(&last_summary_doc, summary_out);
    return YVEX_OK;
}

typedef struct {
    yvex_json cursor;
    const char *path;
    yvex_error *err;
} job_json;

/* Purpose: refuse one malformed job document at the shared-parser boundary. */
static int json_fail(job_json *json, const char *message) {
    yvex_error_setf(json->err, YVEX_ERR_FORMAT, "quant_job_json", "%s in %s", message, json->path);
    return YVEX_ERR_FORMAT;
}

/* Purpose: replace one owned job string from a bounded JSON token.
 * Inputs: parser cursor and address of an owned string slot.
 * Effects: allocates the replacement and releases the previous string.
 * Failure: malformed or oversized text preserves the existing slot.
 * Boundary: this helper owns token transfer, not field semantics. */
static int json_replace_text(job_json *json, char **target) {
    char *value = yvex_json_string_dup(&json->cursor, 16u * 1024u * 1024u);

    if (!value)
        return json_fail(json, "expected bounded JSON string");
    free(*target);
    *target = value;
    return YVEX_OK;
}

/* Purpose: skip an unknown member without extending the job schema. */
static int json_skip(job_json *json) {
    return yvex_json_skip_value(&json->cursor) ? YVEX_OK : json_fail(json, "malformed JSON value");
}

/* Purpose: parse the nested tool identity and executable path.
 * Inputs: bounded cursor and job document under construction.
 * Effects: replaces owned tool-path text and records the typed tool class.
 * Failure: malformed members fail closed with parser context.
 * Boundary: parsing a path does not execute or admit the tool. */
static int parse_tool(job_json *json, yvex_quant_job_doc *doc) {
    char key[YVEX_JSON_KEY_CAP];
    yvex_json_iter iter;
    yvex_json_item item;

    if (!yvex_json_iter_begin(&json->cursor, &iter, YVEX_JSON_COLLECTION_OBJECT))
        return json_fail(json, "unexpected JSON token");
    while ((item = yvex_json_object_member(&iter, key, sizeof(key))) ==
           YVEX_JSON_ITEM_READY) {
        if (strcmp(key, "kind") == 0) {
            char *value = yvex_json_string_dup(&json->cursor, 256u);
            if (!value)
                return json_fail(json, "invalid quantization tool kind");
            doc->tool = yvex_quant_job_tool_from_name(value);
            free(value);
        } else if (strcmp(key, "path") == 0) {
            if (json_replace_text(json, &doc->tool_path) != YVEX_OK) {
                return YVEX_ERR_FORMAT;
            }
        } else if (json_skip(json) != YVEX_OK) {
            return YVEX_ERR_FORMAT;
        }
    }
    return item == YVEX_JSON_ITEM_END && !iter.trailing_separator
               ? YVEX_OK
               : json_fail(json, "malformed object member");
}

/* Purpose: parse one bounded range of declared quant-job path fields.
 * Inputs: bounded cursor, document, and half-open canonical field-table range.
 * Effects: replaces only recognized owned paths and skips unknown members.
 * Failure: malformed values stop the object without promoting partial state.
 * Boundary: parsing path declarations performs no source or artifact I/O. */
static int parse_paths(job_json *json, yvex_quant_job_doc *doc,
                       size_t begin, size_t end) {
    char key[YVEX_JSON_KEY_CAP];
    char **target;
    yvex_json_iter iter;
    yvex_json_item item;

    if (!yvex_json_iter_begin(&json->cursor, &iter, YVEX_JSON_COLLECTION_OBJECT))
        return json_fail(json, "unexpected JSON token");
    while ((item = yvex_json_object_member(&iter, key, sizeof(key))) ==
           YVEX_JSON_ITEM_READY) {
        target = job_field_find(doc, key, begin, end);
        if (target) {
            if (json_replace_text(json, target) != YVEX_OK)
                return YVEX_ERR_FORMAT;
        } else if (json_skip(json) != YVEX_OK) {
            return YVEX_ERR_FORMAT;
        }
    }
    return item == YVEX_JSON_ITEM_END && !iter.trailing_separator
               ? YVEX_OK
               : json_fail(json, "malformed object member");
}

/* Purpose: materialize owned empty strings for every omitted optional quant-job field.
 * Inputs: mutable parsed job document.
 * Effects: allocates only missing strings and leaves existing values untouched.
 * Failure: allocation refusal is returned for caller-owned document cleanup.
 * Boundary: filling defaults does not validate job paths or execute quantization. */
static int fill_missing_text(yvex_quant_job_doc *doc) {
    size_t index;

    for (index = 0u; index < sizeof(job_text_fields) / sizeof(job_text_fields[0]); ++index) {
        char **field = job_document_text(doc, index);
        if (!*field)
            *field = yvex_core_strdup("");
        if (!*field)
            return 0;
    }
    return 1;
}

/* Purpose: decode one complete quantization-job document using the core JSON cursor.
 * Inputs: source path, destination document, and typed error sink.
 * Effects: reads bounded metadata bytes and allocates all document fields.
 * Failure: I/O, grammar, or allocation failure clears every partial field.
 * Boundary: parsing reads no model payload and executes no external command. */
static int job_parse_json_file(const char *path, yvex_quant_job_doc *doc, yvex_error *err) {
    char key[YVEX_JSON_KEY_CAP];
    char *buffer = NULL;
    size_t length = 0u;
    job_json json;
    yvex_json_iter iter;
    yvex_json_item item;
    int rc = YVEX_OK;

    if (!path || !doc) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "quant_job_json", "path and doc are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(doc, 0, sizeof(*doc));
    buffer = yvex_read_bounded_file(path, 16u * 1024u * 1024u, &length, err);
    if (!buffer) {
        if (yvex_error_code(err) == YVEX_OK) {
            yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot read JSON document: %s",
                            path);
        }
        return yvex_error_code(err);
    }
    yvex_json_init(&json.cursor, buffer, length);
    json.path = path;
    json.err = err;
    if (!yvex_json_iter_begin(&json.cursor, &iter, YVEX_JSON_COLLECTION_OBJECT)) {
        json_fail(&json, "unexpected JSON token");
        rc = YVEX_ERR_FORMAT;
        goto fail;
    }
    while ((item = yvex_json_object_member(&iter, key, sizeof(key))) ==
           YVEX_JSON_ITEM_READY) {
        if (strcmp(key, "name") == 0)
            rc = json_replace_text(&json, &doc->name);
        else if (strcmp(key, "architecture") == 0) {
            rc = json_replace_text(&json, &doc->architecture);
        } else if (strcmp(key, "status") == 0) {
            char *value = yvex_json_string_dup(&json.cursor, 256u);
            if (!value)
                rc = json_fail(&json, "invalid quantization job status");
            else {
                doc->status = yvex_quant_job_status_from_name(value);
                free(value);
            }
        } else if (strcmp(key, "tool") == 0)
            rc = parse_tool(&json, doc);
        else if (strcmp(key, "inputs") == 0)
            rc = parse_paths(&json, doc, 3u, 9u);
        else if (strcmp(key, "outputs") == 0)
            rc = parse_paths(&json, doc, 9u, 11u);
        else if (strcmp(key, "command") == 0)
            rc = json_replace_text(&json, &doc->command);
        else
            rc = json_skip(&json);
        if (rc != YVEX_OK)
            goto fail;
    }
    if (item != YVEX_JSON_ITEM_END || iter.trailing_separator ||
        !yvex_json_complete(&json.cursor)) {
        rc = json_fail(&json, "incomplete quantization job document");
        goto fail;
    }
    if (!fill_missing_text(doc)) {
        rc = YVEX_ERR_NOMEM;
        yvex_error_set(err, rc, "quant_job_json", "job field allocation failed");
        goto fail;
    }
    free(buffer);
    return YVEX_OK;

fail:
    free(buffer);
    job_doc_clear(doc);
    return rc;
}

/* Purpose: serialize typed job options into the canonical JSON schema.
 * Inputs: destination path, immutable options, and typed error sink.
 * Effects: creates or replaces the requested document and closes its stream.
 * Failure: open or close errors return typed I/O failure.
 * Boundary: serialization does not run the job or publish a GGUF artifact. */
static int job_write_json_file(const char *out_path, const yvex_quant_job_options *options,
                               yvex_error *err) {
    FILE *fp;

    fp = fopen(out_path, "wb");
    if (!fp) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot write manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    fprintf(fp, "{\n");
    yvex_file_json_write_field(fp, "  ", "schema", "yvex.quant_job.v1", 1);
    job_write_fields(fp, options, "  ", 0u, 2u, 1);
    yvex_file_json_write_field(fp, "  ", "status", yvex_quant_job_status_name(options->status), 1);
    fprintf(fp, "  \"tool\": {\n");
    yvex_file_json_write_field(fp, "    ", "kind", yvex_quant_job_tool_name(options->tool), 1);
    yvex_file_json_write_field(fp, "    ", "path", options->tool_path, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"inputs\": {\n");
    job_write_fields(fp, options, "    ", 3u, 9u, 0);
    fprintf(fp, "  },\n");
    fprintf(fp, "  \"outputs\": {\n");
    job_write_fields(fp, options, "    ", 9u, 11u, 0);
    fprintf(fp, "  },\n");
    job_write_fields(fp, options, "  ", 11u, 12u, 0);
    fprintf(fp, "}\n");
    if (fclose(fp) != 0) {
        yvex_error_setf(err, YVEX_ERR_IO, "quant_job_json", "cannot close manifest: %s", out_path);
        return YVEX_ERR_IO;
    }
    return YVEX_OK;
}
