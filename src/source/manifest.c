/* Owner: source manifest summary.
 * Owns: stable status naming and local manifest summary scanning.
 * Does not own: manifest serialization, source admission, artifacts, or runtime.
 * Invariants: summary scans remain metadata-only and never load tensor payloads.
 * Boundary: manifest summaries do not create source trust.
 * Purpose: expose status names and the local manifest summary boundary.
 * Inputs: typed status, source options, and caller-owned summary storage.
 * Effects: delegates one metadata-only scan without payload reads.
 * Failure: invalid options or scan failure leaves an incomplete summary. */
#include <yvex/internal/source_payload.h>
#include <yvex/source.h>

/* Purpose: project a typed source manifest summary value to its stable diagnostic name.
 * Inputs: typed source manifest summary arguments; borrowed inputs outlive the call.
 * Effects: mutates only explicit caller-owned source manifest summary state.
 * Failure: invalid, bounds, allocation, or I/O failure publishes no partial result.
 * Boundary: manifest summaries do not create source trust. */
const char *yvex_source_status_name(yvex_source_status status) {
    switch (status) {
    case YVEX_SOURCE_STATUS_UNKNOWN:
        return "unknown";
    case YVEX_SOURCE_STATUS_IN_PROGRESS:
        return "in-progress";
    case YVEX_SOURCE_STATUS_INCOMPLETE:
        return "incomplete";
    case YVEX_SOURCE_STATUS_COMPLETE:
        return "complete";
    case YVEX_SOURCE_STATUS_FAILED:
        return "failed";
    }
    return "unknown";
}

/* Purpose: summarize a local source directory into source-manifest footprint facts.
 * Inputs: typed source manifest summary arguments; borrowed inputs outlive the call.
 * Effects: reads bounded evidence and updates only caller-owned source manifest summary state.
 * Failure: invalid, short, inconsistent, or I/O input yields typed refusal.
 * Boundary: manifest summaries do not create source trust. */
int yvex_source_manifest_scan_local(const char *local_path,
                                    yvex_source_manifest_summary *out,
                                    yvex_error *err) {
    yvex_source_manifest_file_list files;
    int rc;

    if (!local_path || !out) {
        yvex_error_set(
            err, YVEX_ERR_INVALID_ARG, "source_manifest_scan", "local_path and out are required");
        return YVEX_ERR_INVALID_ARG;
    }

    yvex_source_manifest_file_list_init(&files);
    rc = yvex_source_manifest_scan_files(local_path, 0, &files, err);
    if (rc == YVEX_OK) {
        *out = files.summary;
    }
    yvex_source_manifest_file_list_free(&files);
    return rc;
}
