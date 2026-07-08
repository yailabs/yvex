/*
 * yvex_model_target_args.h - model-target CLI input contract.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   parsed model-target CLI arguments.
 *
 * Does not own:
 *   report construction, target catalogs, sidecar IO, rendering, stdout/stderr
 *   writing, runtime execution, generation, eval, benchmark, or release
 *   decisions.
 *
 * Invariants:
 *   parser output is a typed request over copied scalar option values.
 *
 * Boundary:
 *   input parsing does not inspect model/source artifacts or create capability.
 */
#ifndef YVEX_MODEL_TARGET_ARGS_H
#define YVEX_MODEL_TARGET_ARGS_H

#include <yvex/error.h>

#include <yvex_model_target_report.h>

typedef struct {
    yvex_model_target_request request;
    int help_requested;
    int parse_failed;
    char error_message[256];
} yvex_model_target_args;

int yvex_model_target_args_parse(int argc,
                                 char **argv,
                                 yvex_model_target_args *out,
                                 yvex_error *err);

#endif
