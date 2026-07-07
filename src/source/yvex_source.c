/*
 * yvex_source.c - source status helpers.
 *
 * Owner: src/source.
 * Owns: small source-domain status vocabulary helpers.
 * Does not own: source report building, manifest writing, native inventory, CLI parsing, help, rendering, runtime, generation, eval, or benchmark.
 * Invariants: no CLI/operator output or parser code lives here.
 * Boundary: source status naming is not source verification or runtime readiness.
 */
#include <yvex/source_manifest.h>

const char *yvex_source_status_name(yvex_source_status status)
{
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
