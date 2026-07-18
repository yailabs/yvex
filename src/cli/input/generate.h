/*
 * generate.h - generate command input parser types.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   typed CLI parse result for the generate command.
 *
 * Does not own:
 *   generation execution, report rendering, command dispatch, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   parsed arguments form a complete generation request before entering the
 *   generation domain.
 *
 * Boundary:
 *   parsing generate arguments is not generation support.
 */
#ifndef YVEX_GENERATE_ARGS_H
#define YVEX_GENERATE_ARGS_H

#include "src/generation/report.h"

typedef enum {
    YVEX_GENERATE_RENDER_NORMAL = 0,
    YVEX_GENERATE_RENDER_AUDIT
} yvex_generate_render_mode;

typedef struct {
    yvex_generation_request request;
    yvex_generate_render_mode render_mode;
    int help_requested;
} yvex_generate_args;

int yvex_generate_args_parse(int argc,
                             char **argv,
                             yvex_generate_args *out,
                             yvex_error *err);

#endif
