/*
 * Owner: runtime.private (runtime).
 * Owns: the private-interface boundary consumed by server,generation,cli.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 */
#ifndef YVEX_RUNTIME_PRIVATE_H
#define YVEX_RUNTIME_PRIVATE_H

/*
 * private.h - Private engine/session runtime state.
 *
 * This header is shared only by runtime-owned source files that need the
 * concrete engine shape. It is not a public API and must not become a generic
 * dumping ground for command state.
 */

#include <stdio.h>

#include <yvex/api.h>

#define YVEX_RUNTIME_REASON_CAP 256u

struct yvex_engine {
    char *model_path;
    yvex_engine_status status;
    yvex_artifact *artifact;
    yvex_gguf *gguf;
    yvex_tensor_table *tensors;
    yvex_model_descriptor *model;
    yvex_tokenizer *tokenizer;
    yvex_graph *graph;
    yvex_backend *weight_backend;
    yvex_weight_table *weights;
    yvex_materialize_summary weight_summary;
    char reason[YVEX_RUNTIME_REASON_CAP];
};

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
                                     int arg_count,
                                     char **args,
                                     yvex_error *err);

#endif
