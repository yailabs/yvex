/*
 * yvex_gguf_reader.c - GGUF reader policy, budgets, and typed failures.
 *
 * Owner:
 *   src/gguf reader ABI
 *
 * Owns:
 *   default structural-read budgets, stable parse sections/codes, error
 *   projection, and operational reader boundary facts.
 *
 * Does not own:
 *   byte decoding, metadata/tensor storage, reports, writer emission, global
 *   layout admission, payload reads, materialization, or runtime support.
 *
 * Invariants:
 *   parse classification is carried by typed facts and never reconstructed
 *   from human-readable error strings.
 *
 * Boundary:
 *   structural reader acceptance is not complete artifact integrity, writer
 *   roundtrip, materialization, or runtime support.
 */
#include "yvex_gguf_private.h"

#include <limits.h>
#include <stdio.h>

static const yvex_gguf_boundary_fact reader_boundary = {
    "src/gguf/yvex_gguf_reader.c",
    "scalable native GGUF structural reader",
    YVEX_GGUF_BOUNDARY_OPERATIONAL,
    "file-backed GGUF v3 directory parsing with typed refusal and zero payload reads",
    YVEX_GGUF_ABI_NEXT_ROW
};

/* Contract: returns the operational structural-reader boundary without IO. */
const yvex_gguf_boundary_fact *yvex_gguf_reader_boundary(void)
{
    return &reader_boundary;
}

/* Contract: initializes target-capable bounded defaults without allocation. */
void yvex_gguf_reader_options_default(yvex_gguf_reader_options *options)
{
    if (!options) return;
    options->max_metadata_entries = 1048576ull;
    options->max_tensor_entries = 1048576ull;
    options->max_array_entries = 16777216ull;
    options->max_total_array_entries = 33554432ull;
    options->max_string_bytes = 67108864ull;
    options->max_total_string_bytes = 1073741824ull;
    options->max_owned_bytes = 2147483648ull;
    options->max_structural_bytes = 2147483648ull;
    options->max_array_depth = 16u;
}

/* Contract: resets one parse result to a stable successful empty state. */
void yvex_gguf_parse_result_reset(yvex_gguf_parse_result *result)
{
    if (!result) return;
    result->code = YVEX_GGUF_PARSE_OK;
    result->section = YVEX_GGUF_PARSE_SECTION_NONE;
    result->byte_offset = 0ull;
    result->record_index = ULLONG_MAX;
    result->reason = "GGUF structural reader accepted input";
}

/* Contract: maps a typed parser code to the existing public error vocabulary. */
static int parse_code_error(yvex_gguf_parse_code code)
{
    switch (code) {
    case YVEX_GGUF_PARSE_OK:
        return YVEX_OK;
    case YVEX_GGUF_PARSE_INVALID_ARGUMENT:
        return YVEX_ERR_INVALID_ARG;
    case YVEX_GGUF_PARSE_FILE_UNREADABLE:
        return YVEX_ERR_IO;
    case YVEX_GGUF_PARSE_SHORT_READ:
        return YVEX_ERR_BOUNDS;
    case YVEX_GGUF_PARSE_UNSUPPORTED_VERSION:
    case YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE:
    case YVEX_GGUF_PARSE_REFUSED_QTYPE:
        return YVEX_ERR_UNSUPPORTED;
    case YVEX_GGUF_PARSE_RESOURCE_LIMIT:
    case YVEX_GGUF_PARSE_OFFSET_OVERFLOW:
    case YVEX_GGUF_PARSE_INCOMPLETE_DIRECTORY:
    case YVEX_GGUF_PARSE_ELEMENT_COUNT_OVERFLOW:
    case YVEX_GGUF_PARSE_ROW_COUNT_OVERFLOW:
    case YVEX_GGUF_PARSE_ROW_BYTE_OVERFLOW:
    case YVEX_GGUF_PARSE_TOTAL_BYTE_OVERFLOW:
        return YVEX_ERR_BOUNDS;
    case YVEX_GGUF_PARSE_ALLOCATION_FAILURE:
        return YVEX_ERR_NOMEM;
    default:
        return YVEX_ERR_FORMAT;
    }
}

/*
 * Contract: records one stable parser refusal, mirrors it into yvex_error, and
 * returns the mapped public status without allocation or cleanup ownership.
 */
int yvex_gguf_reader_fail(yvex_gguf_parse_result *result,
                          yvex_gguf_parse_code code,
                          yvex_gguf_parse_section section,
                          unsigned long long byte_offset,
                          unsigned long long record_index,
                          yvex_error *err,
                          const char *where,
                          const char *reason)
{
    int rc = parse_code_error(code);
    if (result) {
        result->code = code;
        result->section = section;
        result->byte_offset = byte_offset;
        result->record_index = record_index;
        result->reason = reason ? reason : "GGUF structural reader refused input";
    }
    yvex_error_set(err, rc, where ? where : "yvex_gguf_open_ex",
                   reason ? reason : "GGUF structural reader refused input");
    return rc;
}

/* Contract: returns a stable parser-code name without allocation. */
const char *yvex_gguf_parse_code_name(yvex_gguf_parse_code code)
{
    switch (code) {
    case YVEX_GGUF_PARSE_OK: return "ok";
    case YVEX_GGUF_PARSE_INVALID_ARGUMENT: return "invalid-argument";
    case YVEX_GGUF_PARSE_FILE_UNREADABLE: return "file-unreadable";
    case YVEX_GGUF_PARSE_SHORT_READ: return "short-read";
    case YVEX_GGUF_PARSE_INVALID_MAGIC: return "invalid-magic";
    case YVEX_GGUF_PARSE_UNSUPPORTED_VERSION: return "unsupported-version";
    case YVEX_GGUF_PARSE_INVALID_COUNT: return "invalid-count";
    case YVEX_GGUF_PARSE_RESOURCE_LIMIT: return "resource-limit";
    case YVEX_GGUF_PARSE_MALFORMED_KEY: return "malformed-metadata-key";
    case YVEX_GGUF_PARSE_DUPLICATE_METADATA_KEY: return "duplicate-metadata-key";
    case YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE: return "unsupported-metadata-type";
    case YVEX_GGUF_PARSE_MALFORMED_VALUE: return "malformed-metadata-value";
    case YVEX_GGUF_PARSE_MALFORMED_STRING: return "malformed-string";
    case YVEX_GGUF_PARSE_MALFORMED_ARRAY: return "malformed-array";
    case YVEX_GGUF_PARSE_INVALID_ALIGNMENT: return "invalid-alignment";
    case YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME: return "malformed-tensor-name";
    case YVEX_GGUF_PARSE_DUPLICATE_TENSOR_NAME: return "duplicate-tensor-name";
    case YVEX_GGUF_PARSE_INVALID_RANK: return "invalid-rank";
    case YVEX_GGUF_PARSE_INVALID_DIMENSION: return "invalid-dimension";
    case YVEX_GGUF_PARSE_REFUSED_QTYPE: return "refused-qtype";
    case YVEX_GGUF_PARSE_OFFSET_OVERFLOW: return "offset-overflow";
    case YVEX_GGUF_PARSE_INCOMPLETE_DIRECTORY: return "incomplete-directory";
    case YVEX_GGUF_PARSE_ALLOCATION_FAILURE: return "allocation-failure";
    case YVEX_GGUF_PARSE_EMPTY_METADATA_KEY: return "empty-metadata-key";
    case YVEX_GGUF_PARSE_EMPTY_TENSOR_NAME: return "empty-tensor-name";
    case YVEX_GGUF_PARSE_ELEMENT_COUNT_OVERFLOW: return "element-count-overflow";
    case YVEX_GGUF_PARSE_ROW_COUNT_OVERFLOW: return "row-count-overflow";
    case YVEX_GGUF_PARSE_ROW_BYTE_OVERFLOW: return "row-byte-overflow";
    case YVEX_GGUF_PARSE_TOTAL_BYTE_OVERFLOW: return "total-byte-overflow";
    }
    return "unknown-parse-code";
}

/* Contract: returns a stable parser-section name without allocation. */
const char *yvex_gguf_parse_section_name(yvex_gguf_parse_section section)
{
    switch (section) {
    case YVEX_GGUF_PARSE_SECTION_NONE: return "none";
    case YVEX_GGUF_PARSE_SECTION_FILE: return "file";
    case YVEX_GGUF_PARSE_SECTION_CONTAINER: return "container";
    case YVEX_GGUF_PARSE_SECTION_METADATA: return "metadata";
    case YVEX_GGUF_PARSE_SECTION_TENSOR_INFO: return "tensor-info";
    case YVEX_GGUF_PARSE_SECTION_QTYPE: return "qtype";
    case YVEX_GGUF_PARSE_SECTION_RANGE: return "range";
    case YVEX_GGUF_PARSE_SECTION_RESOURCE: return "resource";
    }
    return "unknown-section";
}

/* Contract: reports whether an existing parser status is a refusal. */
int yvex_gguf_reader_parse_refusal(int parse_rc, const char **reason)
{
    if (parse_rc == YVEX_OK) {
        if (reason) *reason = "GGUF structural reader accepted input";
        return 0;
    }
    if (reason) *reason = "GGUF structural reader refused input";
    return 1;
}

static void copy_error_text(char *dst, size_t cap, const char *text)
{
    if (!dst || cap == 0u) return;
    (void)snprintf(dst, cap, "%s", text ? text : "");
}

/* Contract: maps a typed parse result into report section state without policy reconstruction. */
void yvex_gguf_reader_classify_error(int parse_rc,
                                     const yvex_gguf_parse_result *result,
                                     const yvex_error *err,
                                     yvex_gguf_abi_report *report)
{
    yvex_gguf_abi_section_status status;
    const char *reason;

    if (!report) return;
    report->parser_status = parse_rc;
    if (result) report->parse_result = *result;
    reason = result && result->reason ? result->reason : yvex_error_message(err);
    copy_error_text(report->failure_where, sizeof(report->failure_where),
                    result ? yvex_gguf_parse_section_name(result->section) : yvex_error_where(err));
    copy_error_text(report->failure_reason, sizeof(report->failure_reason), reason);

    status = YVEX_GGUF_ABI_SECTION_MALFORMED;
    if (parse_rc == YVEX_ERR_IO) status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
    else if (parse_rc == YVEX_ERR_UNSUPPORTED) status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
    else if (parse_rc == YVEX_ERR_BOUNDS || parse_rc == YVEX_ERR_NOMEM) status = YVEX_GGUF_ABI_SECTION_REFUSED;
    if (result && result->section == YVEX_GGUF_PARSE_SECTION_QTYPE) {
        status = YVEX_GGUF_ABI_SECTION_REFUSED;
    }
    report->status = status;

    if (!result || result->section == YVEX_GGUF_PARSE_SECTION_FILE ||
        result->section == YVEX_GGUF_PARSE_SECTION_CONTAINER ||
        result->section == YVEX_GGUF_PARSE_SECTION_RESOURCE) {
        report->container.status = status;
        report->container.reason = reason;
    } else if (result->section == YVEX_GGUF_PARSE_SECTION_METADATA) {
        report->metadata.status = status;
        report->metadata.reason = reason;
    } else if (result->section == YVEX_GGUF_PARSE_SECTION_TENSOR_INFO) {
        report->tensor_info.status = status;
        report->tensor_info.reason = reason;
    } else if (result->section == YVEX_GGUF_PARSE_SECTION_QTYPE) {
        report->qtype.status = status;
        report->qtype.reason = reason;
    } else {
        report->range.status = status;
        report->range.reason = reason;
    }
}
