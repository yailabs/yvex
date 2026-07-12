/*
 * yvex_backend_args.h - typed backend command input.
 *
 * Owner: src/cli/input.
 * Owns: borrowed backend and cuda-info parse results.
 * Does not own: backend opens, capability policy, rendering, or execution.
 * Invariants: parsing has no backend or IO side effects.
 * Boundary: selecting a backend report is not backend support.
 */
#ifndef YVEX_BACKEND_ARGS_H
#define YVEX_BACKEND_ARGS_H

#include "yvex_backend_report.h"

typedef struct {
    yvex_backend_report_request request;
    int help;
} yvex_backend_args;

int yvex_backend_args_parse(int argc, char **argv,
                            yvex_backend_args *out, yvex_error *err);
int yvex_cuda_info_args_parse(int argc, char **argv,
                              yvex_backend_args *out, yvex_error *err);

#endif
