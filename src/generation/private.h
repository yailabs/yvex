/*
 * private.h - private diagnostic generation dependencies.
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

#include "report.h"
#include "kv_report.h"
#include "sampling_report.h"
#include "trace.h"

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

int yvex_graph_preflight(const yvex_model_ref *model_ref,
                          const char *backend_name,
                          int execute_fixture,
                          int execute_segment,
                          unsigned int token_id,
                          yvex_cli_graph_guard_report *report,
                          yvex_error *err);

void yvex_kv_fill_demo_values(float *values,
                         unsigned long long value_count,
                         unsigned long long position);
unsigned long long yvex_kv_checksum_values(const float *values,
                                      unsigned long long value_count);

void yvex_sampling_summary_defaults(yvex_sampling_summary *out,
                             const yvex_sampling_options *options);

#endif
