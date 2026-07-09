/*
 * yvex_gguf_reader.c - GGUF reader boundary over parser facts.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   reader status facts and parse-refusal propagation for existing parser
 *   results.
 *
 * Does not own:
 *   writer emission, roundtrip, artifact emission, materialization, runtime
 *   descriptor projection, or generation.
 *
 * Invariants:
 *   reader state mirrors parser outcomes and does not invent capability.
 *
 * Boundary:
 *   reader acceptance is not writer roundtrip, materialization, or runtime
 *   support.
 */
#include "yvex_gguf_private.h"

#include <stdio.h>
#include <string.h>

static const yvex_gguf_boundary_fact reader_boundary = {
    "src/gguf/yvex_gguf_reader.c",
    "GGUF reader",
    YVEX_GGUF_BOUNDARY_REPORT_ONLY,
    "reader facts wrap existing parser outcomes",
    "V010.GGUF.ARTIFACT.ABI.0"
};

/* Contract: exposes reader boundary facts without opening files. */
const yvex_gguf_boundary_fact *yvex_gguf_reader_boundary(void)
{
    return &reader_boundary;
}

/* Contract: maps parser return codes to reader refusal state. */
int yvex_gguf_reader_parse_refusal(int parse_rc, const char **reason)
{
    if (parse_rc == 0) {
        if (reason) *reason = "GGUF parser accepted directory facts";
        return 0;
    }
    if (reason) *reason = "GGUF parser refused input";
    return 1;
}

static void copy_error_text(char *dst, size_t cap, const char *text)
{
    if (!dst || cap == 0u) return;
    if (!text) text = "";
    (void)snprintf(dst, cap, "%s", text);
}

static yvex_gguf_abi_section_status malformed_or_refused(int parse_rc)
{
    if (parse_rc == YVEX_ERR_UNSUPPORTED) return YVEX_GGUF_ABI_SECTION_UNSUPPORTED;
    if (parse_rc == YVEX_ERR_BOUNDS) return YVEX_GGUF_ABI_SECTION_REFUSED;
    if (parse_rc == YVEX_ERR_IO) return YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
    return YVEX_GGUF_ABI_SECTION_MALFORMED;
}

/* Contract: classifies existing parser errors into typed ABI section facts. */
void yvex_gguf_reader_classify_error(int parse_rc,
                                     const yvex_error *err,
                                     yvex_gguf_abi_report *report)
{
    const char *where;
    const char *message;
    yvex_gguf_abi_section_status status;

    if (!report) return;
    report->parser_status = parse_rc;

    where = yvex_error_where(err);
    message = yvex_error_message(err);
    copy_error_text(report->failure_where, sizeof(report->failure_where), where);
    copy_error_text(report->failure_reason, sizeof(report->failure_reason), message);

    status = malformed_or_refused(parse_rc);
    report->status = status;

    if (parse_rc == YVEX_ERR_IO) {
        report->container.status = YVEX_GGUF_ABI_SECTION_NOT_PRESENT;
        report->container.reason = "GGUF file is missing or unreadable";
        return;
    }

    if ((where && strstr(where, "header")) ||
        (message && (strstr(message, "magic") || strstr(message, "version") || strstr(message, "header")))) {
        report->container.status = status;
        report->container.reason = message && message[0] ? message : "GGUF container header refused";
        return;
    }

    if (where && strstr(where, "metadata")) {
        report->metadata.status = status;
        report->metadata.reason = message && message[0] ? message : "GGUF metadata ABI refused";
        return;
    }

    if (where && (strstr(where, "tensor.offset") || strstr(where, "tensor_dir"))) {
        report->range.status = status;
        report->range.reason = message && message[0] ? message : "GGUF range map refused";
        return;
    }

    if (where && strstr(where, "tensor")) {
        report->tensor_info.status = status;
        report->tensor_info.reason = message && message[0] ? message : "GGUF tensor_info ABI refused";
        return;
    }

    report->container.status = status;
    report->container.reason = message && message[0] ? message : "GGUF reader refused input";
}
