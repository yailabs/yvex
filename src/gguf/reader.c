/* Owner: src/gguf reader ABI
 * Owns: default structural-read budgets, stable parse sections/codes, error projection, and operational reader
 *   boundary facts.
 * Does not own: byte decoding, metadata/tensor storage, reports, writer emission, global layout admission, payload
 *   reads, materialization, or runtime support.
 * Invariants: parse classification is carried by typed facts and never reconstructed from human-readable error
 *   strings.
 * Boundary: structural reader acceptance is not complete artifact integrity, writer roundtrip, materialization, or
 *   runtime support.
 * Purpose: define bounded reader policy and typed projection of structural parse failures.
 * Inputs: caller-owned options, parse results, error records, and structural reports.
 * Effects: initializes or updates only the supplied policy and diagnostic records.
 * Failure: invalid, unsupported, bounded, allocation, and format cases retain distinct codes. */
#include <limits.h>
#include <stdio.h>
#include <yvex/internal/gguf.h>

static const char *const parse_code_names[] = {
    "ok", "invalid-argument", "file-unreadable", "short-read", "invalid-magic",
    "unsupported-version", "invalid-count", "resource-limit", "malformed-metadata-key",
    "duplicate-metadata-key", "unsupported-metadata-type", "malformed-metadata-value",
    "malformed-string", "malformed-array", "invalid-alignment", "malformed-tensor-name",
    "duplicate-tensor-name", "invalid-rank", "invalid-dimension", "refused-qtype",
    "offset-overflow", "incomplete-directory", "allocation-failure", "empty-metadata-key",
    "empty-tensor-name", "element-count-overflow", "row-count-overflow", "row-byte-overflow",
    "total-byte-overflow",
};

static const char *const parse_section_names[] = {
    "none", "file", "container", "metadata", "tensor-info", "qtype", "range", "resource",
};

/* Purpose: initialize target-capable structural reader budgets without allocation.
 * Inputs: an optional writable options record.
 * Effects: replaces every budget field when the record is present.
 * Failure: none; a null record is a no-op.
 * Boundary: defaults constrain structural reads and never read tensor payload. */
void yvex_gguf_reader_options_default(yvex_gguf_reader_options *options) {
    if (!options)
        return;
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

/* Purpose: reset one parse result to the canonical successful empty state.
 * Inputs: an optional writable result record.
 * Effects: clears location facts and installs the stable acceptance reason.
 * Failure: none; a null result is a no-op.
 * Boundary: reset state is not evidence that a file was parsed. */
void yvex_gguf_parse_result_reset(yvex_gguf_parse_result *result) {
    if (!result)
        return;
    result->code = YVEX_GGUF_PARSE_OK;
    result->section = YVEX_GGUF_PARSE_SECTION_NONE;
    result->byte_offset = 0ull;
    result->record_index = ULLONG_MAX;
    result->reason = "GGUF structural reader accepted input";
}

/* Purpose: map one typed parser code to the stable public status vocabulary.
 * Inputs: parser failure code.
 * Effects: none.
 * Failure: unknown parse codes map to format refusal.
 * Boundary: mapping preserves parser detail in the separate parse result. */
static int parse_code_error(yvex_gguf_parse_code code) {
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

/* Purpose: publish one typed structural-reader refusal and its precise location.
 * Inputs: optional result/error records, parse code, section, offsets, and diagnostic text.
 * Effects: replaces supplied diagnostics and returns the mapped public status.
 * Failure: this function represents the supplied failure and performs no allocation.
 * Boundary: refusal publication owns neither artifact cleanup nor capability classification. */
int yvex_gguf_reader_fail(yvex_gguf_parse_result *result, yvex_gguf_parse_code code,
                          yvex_gguf_parse_section section, unsigned long long byte_offset,
                          unsigned long long record_index, yvex_error *err, const char *where,
                          const char *reason) {
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

/* Purpose: project one parse code as a stable diagnostic name.
 * Inputs: parser code value.
 * Effects: none.
 * Failure: out-of-range values yield unknown-parse-code.
 * Boundary: text never becomes the source of failure classification. */
const char *yvex_gguf_parse_code_name(yvex_gguf_parse_code code) {
    return code >= YVEX_GGUF_PARSE_OK &&
                   (size_t)code < sizeof(parse_code_names) / sizeof(parse_code_names[0])
               ? parse_code_names[code]
               : "unknown-parse-code";
}

/* Purpose: project one structural parser section as a stable diagnostic name.
 * Inputs: parser section value.
 * Effects: none.
 * Failure: out-of-range values yield unknown-section.
 * Boundary: section text does not infer parser state. */
const char *yvex_gguf_parse_section_name(yvex_gguf_parse_section section) {
    return section >= YVEX_GGUF_PARSE_SECTION_NONE &&
                   (size_t)section < sizeof(parse_section_names) / sizeof(parse_section_names[0])
               ? parse_section_names[section]
               : "unknown-section";
}

/* Purpose: copy bounded diagnostic text with deterministic null handling. */
static void copy_error_text(char *dst, size_t cap, const char *text) {
    if (!dst || cap == 0u)
        return;
    (void)snprintf(dst, cap, "%s", text ? text : "");
}

/* Purpose: project one typed parse failure into the affected ABI report section.
 * Inputs: public parse status, optional typed result/error, and writable report.
 * Effects: updates report status, location, reason, and the single affected section.
 * Failure: missing reports are ignored; no failure is inferred from diagnostic text.
 * Boundary: report projection cannot promote structural acceptance or artifact support. */
void yvex_gguf_reader_classify_error(int parse_rc, const yvex_gguf_parse_result *result,
                                     const yvex_error *err, yvex_gguf_abi_report *report) {
    yvex_gguf_abi_section_status status;
    const char *reason;

    if (!report)
        return;
    report->parser_status = parse_rc;
    if (result)
        report->parse_result = *result;
    reason = result && result->reason ? result->reason : yvex_error_message(err);
    copy_error_text(report->failure_where, sizeof(report->failure_where),
                    result ? yvex_gguf_parse_section_name(result->section) : yvex_error_where(err));
    copy_error_text(report->failure_reason, sizeof(report->failure_reason), reason);

    status = YVEX_GGUF_ABI_SECTION_MALFORMED;
    if (parse_rc == YVEX_ERR_IO)
        status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
    else if (parse_rc == YVEX_ERR_UNSUPPORTED)
        status = YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
    else if (parse_rc == YVEX_ERR_BOUNDS || parse_rc == YVEX_ERR_NOMEM)
        status = YVEX_GGUF_ABI_SECTION_REFUSED;
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
