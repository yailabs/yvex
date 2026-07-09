/*
 * yvex_gguf_report.c - typed GGUF report facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   typed GGUF report construction for ABI/report-only and unsupported
 *   writer/roundtrip/materialization states.
 *
 * Does not own:
 *   CLI rendering, operator byte output, file writing, runtime generation,
 *   eval, benchmark, or release claims.
 *
 * Invariants:
 *   reports carry facts only and do not serialize user-visible output.
 *
 * Boundary:
 *   GGUF reports do not imply writer, roundtrip, materialization, or runtime
 *   capability.
 */
#include "yvex_gguf_private.h"

#include <stdio.h>
#include <string.h>

static void copy_text(char *dst, size_t cap, const char *text)
{
    if (!dst || cap == 0u) return;
    if (!text) text = "";
    (void)snprintf(dst, cap, "%s", text);
}

static unsigned int read_magic_hint(const yvex_artifact *artifact)
{
    const unsigned char *data;

    if (!artifact || yvex_artifact_size(artifact) < 4ull) return 0u;
    data = yvex_artifact_data(artifact);
    if (!data) return 0u;
    return ((unsigned int)data[0]) |
           ((unsigned int)data[1] << 8) |
           ((unsigned int)data[2] << 16) |
           ((unsigned int)data[3] << 24);
}

/* Contract: initializes a GGUF report fact without allocation or rendering. */
void yvex_gguf_report_fact_init(yvex_gguf_report_fact *report,
                                const char *kind,
                                const char *status,
                                const char *reason,
                                const char *next_row)
{
    if (!report) return;
    report->kind = kind ? kind : "gguf";
    report->status = status ? status : "unsupported";
    report->reason = reason ? reason : "GGUF capability is future-owned";
    report->next_row = next_row ? next_row : "V010.GGUF.ARTIFACT.ABI.0";
}

/* Contract: initializes a typed GGUF ABI report with no rendering or IO. */
void yvex_gguf_abi_report_init(yvex_gguf_abi_report *report, const char *path)
{
    if (!report) return;
    report->path = path;
    report->status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    yvex_gguf_container_abi_init(&report->container);
    yvex_gguf_metadata_abi_init(&report->metadata);
    yvex_gguf_tensor_info_abi_init(&report->tensor_info);
    yvex_gguf_qtype_abi_init(&report->qtype);
    yvex_gguf_range_fact_init(&report->range);
    report->descriptor.status = YVEX_GGUF_ABI_SECTION_NOT_EVALUATED;
    report->descriptor.reason = "GGUF descriptor not evaluated";
    report->parser_status = YVEX_OK;
    report->failure_where[0] = '\0';
    report->failure_reason[0] = '\0';
    report->next_row = YVEX_GGUF_ABI_NEXT_ROW;
}

/* Contract: builds a typed report for GGUF container/metadata/tensor_info ABI. */
int yvex_gguf_artifact_abi_report_build(const char *path,
                                        yvex_gguf_abi_report *report,
                                        yvex_error *err)
{
    yvex_artifact_options options;
    yvex_artifact *artifact = NULL;
    yvex_gguf *gguf = NULL;
    yvex_gguf_header header;
    yvex_error local_err;
    yvex_error *parse_err;
    const char *reason = NULL;
    int rc;

    if (!report) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_gguf_artifact_abi_report_build", "report is required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_gguf_abi_report_init(report, path);
    if (!path || path[0] == '\0') {
        report->status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        report->container.status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        report->container.reason = "GGUF path is required";
        return YVEX_OK;
    }

    parse_err = err ? err : &local_err;
    yvex_error_clear(parse_err);
    memset(&header, 0, sizeof(header));

    memset(&options, 0, sizeof(options));
    options.path = path;
    options.readonly = 1;

    rc = yvex_artifact_open(&artifact, &options, parse_err);
    if (rc != YVEX_OK) {
        yvex_gguf_reader_classify_error(rc, parse_err, report);
        return YVEX_OK;
    }

    report->container.magic = read_magic_hint(artifact);
    rc = yvex_gguf_read_header(artifact, &header, parse_err);
    if (rc != YVEX_OK) {
        report->container.version = header.version;
        yvex_gguf_reader_classify_error(rc, parse_err, report);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    yvex_gguf_container_abi_from_header(&header, &report->container);
    rc = yvex_gguf_open(&gguf, artifact, parse_err);
    if (rc != YVEX_OK) {
        yvex_gguf_reader_classify_error(rc, parse_err, report);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    if (!yvex_gguf_metadata_abi_from_gguf(gguf, &report->metadata, &reason)) {
        report->status = report->metadata.status;
        copy_text(report->failure_reason, sizeof(report->failure_reason), reason);
        yvex_gguf_descriptor_abi_from_sections(&report->container,
                                               &report->metadata,
                                               &report->tensor_info,
                                               &report->qtype,
                                               &report->range,
                                               &report->descriptor);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    if (!yvex_gguf_tensor_info_abi_from_gguf(gguf, &report->tensor_info, &reason)) {
        report->status = report->tensor_info.status;
        copy_text(report->failure_reason, sizeof(report->failure_reason), reason);
        yvex_gguf_descriptor_abi_from_sections(&report->container,
                                               &report->metadata,
                                               &report->tensor_info,
                                               &report->qtype,
                                               &report->range,
                                               &report->descriptor);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    if (!yvex_gguf_qtype_abi_from_gguf(gguf, &report->qtype, &reason)) {
        report->status = report->qtype.status;
        copy_text(report->failure_reason, sizeof(report->failure_reason), reason);
        yvex_gguf_descriptor_abi_from_sections(&report->container,
                                               &report->metadata,
                                               &report->tensor_info,
                                               &report->qtype,
                                               &report->range,
                                               &report->descriptor);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    if (!yvex_gguf_range_fact_from_gguf(artifact, gguf, &report->range, &reason)) {
        report->status = report->range.status;
        copy_text(report->failure_reason, sizeof(report->failure_reason), reason);
        yvex_gguf_descriptor_abi_from_sections(&report->container,
                                               &report->metadata,
                                               &report->tensor_info,
                                               &report->qtype,
                                               &report->range,
                                               &report->descriptor);
        yvex_gguf_close(gguf);
        yvex_artifact_close(artifact);
        return YVEX_OK;
    }

    yvex_gguf_descriptor_abi_from_sections(&report->container,
                                           &report->metadata,
                                           &report->tensor_info,
                                           &report->qtype,
                                           &report->range,
                                           &report->descriptor);
    report->status = YVEX_GGUF_ABI_SECTION_REPORT_ONLY;
    report->parser_status = YVEX_OK;
    copy_text(report->failure_where, sizeof(report->failure_where), "");
    copy_text(report->failure_reason, sizeof(report->failure_reason), "GGUF artifact ABI accepted at report-only boundary");

    yvex_gguf_close(gguf);
    yvex_artifact_close(artifact);
    yvex_error_clear(parse_err);
    return YVEX_OK;
}
