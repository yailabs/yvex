/*
 * yvex_source_manifest.c - source manifest summary API.
 *
 * Owner: src/source.
 * Owns: public source manifest scan entrypoints.
 * Does not own: JSON byte writing, CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: scans local metadata only and never loads tensor payload bytes.
 * Boundary: manifest scanning is not source verification, artifact emission, or release readiness.
 */
#include "yvex_source_manifest.h"
#include "yvex_source_private.h"

/*
 * yvex_source_manifest_scan_local()
 *
 * Purpose:
 *   summarize a local source directory into source-manifest footprint facts.
 *
 * Inputs:
 *   local_path is borrowed; out receives by-value summary fields.
 *
 * Effects:
 *   scans local directory entries and sizes through manifest file-list helpers;
 *   it does not hash files, read tensor payloads, or contact remotes.
 *
 * Failure:
 *   returns invalid-arg, IO, allocation, or scan errors from the manifest
 *   helpers.
 *
 * Boundary:
 *   local footprint scanning is not source verification, artifact emission,
 *   runtime support, generation, eval, benchmark, or release readiness.
 */
int yvex_source_manifest_scan_local(const char *local_path,
                                    yvex_source_manifest_summary *out,
                                    yvex_error *err)
{
    yvex_source_manifest_file_list files;
    int rc;

    if (!local_path || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "source_manifest_scan", "local_path and out are required");
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
