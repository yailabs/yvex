/* Owner: src/gguf report projection
 * Owns: report construction from one operational reader result and explicit unsupported
 *   writer/roundtrip/materialization states.
 * Does not own: GGUF parsing, failure classification, CLI rendering, file writing, global layout policy, runtime
 *   generation, eval, benchmark, or release claims.
 * Invariants: report construction never turns a rejected parse into successful artifact validity; accepted sections
 *   project the same immutable parsed view and canonical layout result.
 * Boundary: a successful structural report is not complete artifact integrity, materialization, or runtime
 *   capability.
 * Purpose: project canonical reader, layout, and descriptor facts into one typed ABI report.
 * Inputs: an artifact path plus caller-owned report and error records.
 * Effects: opens and closes a read-only artifact snapshot while replacing the report.
 * Failure: parser or projection refusal remains typed and leaves no retained artifact handle. */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <yvex/internal/gguf.h>

/* Purpose: copy bounded report text while normalizing null input to an empty string. */
static void copy_text(char *dst, size_t cap, const char *text) {
    if (!dst || cap == 0u)
        return;
    (void)snprintf(dst, cap, "%s", text ? text : "");
}

/* Purpose: initialize a fail-closed ABI report before any file admission attempt.
 * Inputs: writable report and borrowed path.
 * Effects: resets every section to not-evaluated and retains only the borrowed path pointer.
 * Failure: none; a null report is a no-op.
 * Boundary: initialization is not structural acceptance. */
static void report_init(yvex_gguf_abi_report *report, const char *path) {
    if (!report)
        return;
    memset(report, 0, sizeof(*report));
    report->path = path;
    report->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    yvex_gguf_container_abi_init(&report->container);
    yvex_gguf_metadata_abi_init(&report->metadata);
    yvex_gguf_tensor_info_abi_init(&report->tensor_info);
    yvex_gguf_qtype_abi_init(&report->qtype);
    report->layout.code = YVEX_GGUF_LAYOUT_INVALID_ARGUMENT;
    yvex_gguf_range_fact_init(&report->range);
    report->descriptor.status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    report->descriptor.reason = "GGUF structural descriptor not evaluated";
    report->parser_status = YVEX_OK;
    yvex_gguf_parse_result_reset(&report->parse_result);
    report->next_row = YVEX_GGUF_ABI_NEXT_ROW;
}

/* Purpose: publish one typed report-projection failure with bounded diagnostic text.
 * Inputs: writable report, section status, location, reason, and optional error record.
 * Effects: marks the report failed and replaces supplied error state.
 * Failure: always returns format refusal.
 * Boundary: projection refusal does not alter the underlying parsed artifact snapshot. */
static int projection_failure(yvex_gguf_abi_report *report, yvex_gguf_abi_section_status status,
                              const char *where, const char *reason, yvex_error *err) {
    report->status = status;
    report->parser_status = YVEX_ERR_FORMAT;
    copy_text(report->failure_where, sizeof(report->failure_where), where);
    copy_text(report->failure_reason, sizeof(report->failure_reason), reason);
    yvex_error_set(err, YVEX_ERR_FORMAT, where, reason);
    return YVEX_ERR_FORMAT;
}

/* Purpose: build one complete typed structural report from a read-only GGUF snapshot.
 * Inputs: artifact path and writable report/error records.
 * Effects: opens the artifact, parses once, validates global layout, then closes all handles.
 * Failure: I/O, parse, qtype, layout, or descriptor refusal leaves a classified report.
 * Boundary: a successful report proves structure only, not complete-model or runtime support. */
int yvex_gguf_artifact_abi_report_build(const char *path, yvex_gguf_abi_report *report,
                                        yvex_error *err) {
    yvex_artifact_options artifact_options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_error local_err;
    yvex_error *active_err = err ? err : &local_err;
    yvex_gguf_parse_result parse_result;
    const yvex_gguf_header *header;
    const yvex_gguf_reader_stats *stats;
    const char *reason = NULL;
    int rc;

    if (!report) {
        yvex_error_set(active_err, YVEX_ERR_INVALID_ARG, "yvex_gguf_artifact_abi_report_build",
                       "report is required");
        return YVEX_ERR_INVALID_ARG;
    }
    report_init(report, path);
    if (!path || path[0] == '\0') {
        yvex_gguf_reader_fail(&report->parse_result, YVEX_GGUF_PARSE_INVALID_ARGUMENT,
                              YVEX_GGUF_PARSE_SECTION_FILE, 0ull, ULLONG_MAX, active_err,
                              "yvex_gguf_artifact_abi_report_build", "GGUF path is required");
        yvex_gguf_reader_classify_error(YVEX_ERR_INVALID_ARG, &report->parse_result, active_err,
                                        report);
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&artifact_options, 0, sizeof(artifact_options));
    artifact_options.path = path;
    artifact_options.readonly = 1;
    rc = yvex_artifact_open(&artifact, &artifact_options, active_err);
    if (rc != YVEX_OK) {
        yvex_gguf_parse_result_reset(&parse_result);
        yvex_gguf_reader_fail(&parse_result, YVEX_GGUF_PARSE_FILE_UNREADABLE,
                              YVEX_GGUF_PARSE_SECTION_FILE, 0ull, ULLONG_MAX, active_err,
                              "gguf.file", "GGUF file is missing or unreadable");
        yvex_gguf_reader_classify_error(rc, &parse_result, active_err, report);
        return rc;
    }

    rc = yvex_gguf_open_ex(&gguf, artifact, NULL, &parse_result, active_err);
    if (rc != YVEX_OK) {
        yvex_gguf_reader_classify_error(rc, &parse_result, active_err, report);
        yvex_artifact_close(artifact);
        return rc;
    }
    report->parse_result = parse_result;
    header = yvex_gguf_header_view(gguf);
    stats = yvex_gguf_reader_stats_view(gguf);
    if (stats)
        report->reader_stats = *stats;
    yvex_gguf_container_abi_from_header(header, &report->container);
    report->container.magic = YVEX_GGUF_MAGIC;

    if (!yvex_gguf_metadata_abi_from_gguf(gguf, &report->metadata, &reason)) {
        rc = projection_failure(report, report->metadata.status, "gguf.metadata", reason,
                                active_err);
        goto done;
    }
    if (!yvex_gguf_tensor_info_abi_from_gguf(gguf, &report->tensor_info, &reason)) {
        rc = projection_failure(report, report->tensor_info.status, "gguf.tensor-info", reason,
                                active_err);
        goto done;
    }
    if (!yvex_gguf_qtype_abi_from_gguf(gguf, &report->qtype, &reason)) {
        rc = projection_failure(report, report->qtype.status, "gguf.qtype", reason, active_err);
        goto done;
    }
    rc = yvex_gguf_layout_validate(artifact, gguf, &report->layout, active_err);
    if (rc != YVEX_OK) {
        report->range.status = YVEX_GGUF_ABI_SECTION_REFUSED;
        report->range.reason = report->layout.reason;
        rc = projection_failure(report, YVEX_GGUF_ABI_SECTION_REFUSED, "gguf.layout",
                                report->layout.reason, active_err);
        goto done;
    }
    if (!yvex_gguf_range_fact_from_layout(&report->layout, &report->range, &reason)) {
        rc = projection_failure(report, report->range.status, "gguf.range", reason, active_err);
        goto done;
    }

    yvex_gguf_descriptor_abi_from_sections(&report->container, &report->metadata,
                                           &report->tensor_info, &report->qtype, &report->range,
                                           &report->descriptor);
    if (report->descriptor.status != YVEX_GGUF_ABI_SECTION_OK) {
        rc = projection_failure(report, report->descriptor.status, "gguf.descriptor",
                                report->descriptor.reason, active_err);
        goto done;
    }
    report->status = YVEX_GGUF_ABI_SECTION_OK;
    report->parser_status = YVEX_OK;
    copy_text(report->failure_where, sizeof(report->failure_where), "");
    copy_text(report->failure_reason, sizeof(report->failure_reason),
              "GGUF structural reader and canonical global layout accepted input");
    yvex_error_clear(active_err);
    rc = YVEX_OK;

done:
    if (rc != YVEX_OK) {
        yvex_gguf_descriptor_abi_from_sections(&report->container, &report->metadata,
                                               &report->tensor_info, &report->qtype, &report->range,
                                               &report->descriptor);
    }
    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    return rc;
}
