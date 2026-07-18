/*
 * kv.h - KV command input parser types.
 *
 * Owner:
 *   src/cli/input
 *
 * Owns:
 *   typed parse result for the KV command surface.
 *
 * Does not own:
 *   KV cache allocation, report construction, command dispatch, rendering,
 *   stdout/stderr output, attention execution, decode, logits, sampling,
 *   generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   parsed values form a typed KV report request before entering the domain.
 *
 * Boundary:
 *   parsing KV command input is not KV execution or model support.
 */
#ifndef YVEX_KV_ARGS_H
#define YVEX_KV_ARGS_H

#include "src/generation/kv_report.h"

typedef struct {
    yvex_kv_report_request request;
    int help_requested;
} yvex_kv_args;

int yvex_kv_args_parse(int argc,
                       char **argv,
                       yvex_kv_args *out,
                       yvex_error *err);

#endif
