/*
 * Owner: abi.profile (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Runtime profile files
 *
 * File: include/yvex/profile.h
 * Layer: public runtime observability API
 *
 * Purpose:
 *   Defines JSON writers for observability layer metrics and profile summaries. These files
 *   summarize implemented runtime-shell work only and do not contain decode
 *   throughput, generated-token, or inference benchmark claims.
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_profile
 */
#ifndef YVEX_PROFILE_H
#define YVEX_PROFILE_H

#include <yvex/metrics.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *run_id;
    const char *command;
    const char *model_name;
    const char *backend_name;
    const char *status;
    int execution_ready;
    yvex_metric_counters counters;
} yvex_profile_summary;

int yvex_profile_write_json(const char *path,
                            const yvex_profile_summary *summary,
                            const yvex_metrics *metrics,
                            yvex_error *err);
int yvex_metrics_write_json(const char *path,
                            const yvex_metrics *metrics,
                            yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_PROFILE_H */
