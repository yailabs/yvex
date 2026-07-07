/*
 * yvex_sampling_args.h - typed sampling command input.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   parsed sampling command input shape.
 *
 * Does not own:
 *   sampling report construction, command dispatch, rendering, engine open,
 *   graph guard preflight, stdout/stderr output, generation, eval, benchmark,
 *   or release decisions.
 *
 * Invariants:
 *   parsed input is typed and contains no opened runtime state.
 *
 * Boundary:
 *   parsing sampling input is not runtime sampling support.
 */
#ifndef YVEX_SAMPLING_ARGS_H
#define YVEX_SAMPLING_ARGS_H

#include "yvex_sampling_report.h"

typedef struct {
    yvex_sampling_report_request request;
    yvex_sampling_report_mode render_mode;
    int help_requested;
    int help_exit_code;
} yvex_sampling_args;

int yvex_sampling_args_parse(int argc,
                             char **argv,
                             yvex_sampling_args *out,
                             yvex_error *err);

#endif
