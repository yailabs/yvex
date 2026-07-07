/*
 * yvex_generation_private.h - private diagnostic generation dependencies.
 *
 * Owner:
 *   src/generation
 *
 * Owns:
 *   generation-cell private declarations needed to preserve the existing
 *   diagnostic generate preflight path.
 *
 * Does not own:
 *   CLI parser/render declarations, operator command surfaces, graph
 *   implementation, artifact identity implementation, eval, benchmark, or
 *   release decisions.
 *
 * Invariants:
 *   only narrow non-rendering declarations needed by generation diagnostics
 *   are exposed here; CLI/operator headers remain out of src/generation.
 *
 * Boundary:
 *   private preflight declarations are not runtime generation support.
 */
#ifndef YVEX_GENERATION_PRIVATE_H
#define YVEX_GENERATION_PRIVATE_H

#include "yvex_generation_report.h"
#include "yvex_generation_trace.h"

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

int enforce_registered_identity_cli(const yvex_model_ref *ref, const char *surface);
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
