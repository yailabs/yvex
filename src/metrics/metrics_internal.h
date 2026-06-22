/*
 * YVEX - Metrics internals
 *
 * File: src/metrics/metrics_internal.h
 * Layer: runtime observability implementation
 *
 * Purpose:
 *   Shares small JSON, time, and run-artifact helpers between J0 metrics
 *   modules, CLI runtime code, and focused tests.
 */
#ifndef YVEX_METRICS_INTERNAL_H
#define YVEX_METRICS_INTERNAL_H

#include <stdio.h>

#include <yvex/yvex.h>

typedef struct {
    char run_id[YVEX_RUN_ID_CAP];
    char run_dir[YVEX_PATH_CAP];
    char command_path[YVEX_PATH_CAP];
    char metrics_path[YVEX_PATH_CAP];
    char trace_path[YVEX_PATH_CAP];
    char profile_path[YVEX_PATH_CAP];
    int has_run_dir;
    int has_metrics;
    int has_trace;
    int has_profile;
} yvex_run_artifacts;

unsigned long long yvex_time_monotonic_ns(void);

int yvex_json_write_string(FILE *fp, const char *text);

int yvex_run_artifacts_prepare(yvex_run_artifacts *out,
                               int save_run,
                               const char *run_dir,
                               const char *metrics_out,
                               const char *trace_out,
                               const char *profile_out,
                               yvex_error *err);
int yvex_run_artifacts_write_command(const yvex_run_artifacts *artifacts,
                                     int argc,
                                     char **argv,
                                     yvex_error *err);

#endif /* YVEX_METRICS_INTERNAL_H */
