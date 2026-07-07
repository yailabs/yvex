/*
 * yvex_sampling_private.h - private sampling-cell declarations.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   narrow declarations needed by the sampling report builder and sampler
 *   implementation.
 *
 * Does not own:
 *   command dispatch, text rendering, stdout/stderr output, graph
 *   implementation, model registry implementation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   this header exposes no operator-output writer and no command surface.
 *
 * Boundary:
 *   private diagnostic declarations are not runtime generation support.
 */
#ifndef YVEX_SAMPLING_PRIVATE_H
#define YVEX_SAMPLING_PRIVATE_H

#include "yvex_sampling_report.h"

typedef struct {
    const char *guard_status;
    const char *phase;
    const char *graph_kind;
    const char *integrity_status;
    const char *identity_status;
    const char *metadata_status;
    const char *shape_status;
    const char *range_status;
    const char *slice_range_status;
    const char *backend_status;
    const char *backend_op_status;
    int dispatch_attempted;
    int reference_read_attempted;
    int output_allocation_attempted;
    int cleanup_attempted;
    const char *cleanup_status;
    unsigned long long output_bytes_planned;
    unsigned long long output_bytes_allocated;
    unsigned long long reference_bytes_planned;
} yvex_cli_graph_guard_report;

void sample_summary_defaults(yvex_sampling_summary *out,
                             const yvex_sampling_options *options);

int cli_token_input_vocab_from_model(const char *path,
                                     unsigned long long *vocab_size,
                                     yvex_error *err);
int preflight_graph_guard(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int token_id,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);

#endif
