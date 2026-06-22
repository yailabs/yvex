/*
 * YVEX - Graph planning
 *
 * File: include/yvex/graph.h
 * Layer: public graph API
 *
 * Purpose:
 *   Defines the opaque graph object and graph inspection APIs for F0. Graphs
 *   are deterministic planning artifacts; they do not execute computation.
 *
 * Owns:
 *   - yvex_graph
 *   - graph build options
 *   - graph status and diagnostics
 *   - graph dump surface
 *
 * Does not own:
 *   - backend execution
 *   - device allocation
 *   - sessions
 *   - inference
 *
 * Used by:
 *   - planner
 *   - memory plan
 *   - CLI graph/plan commands
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_graph
 */
#ifndef YVEX_GRAPH_H
#define YVEX_GRAPH_H

#include <stdio.h>

#include <yvex/model.h>
#include <yvex/op.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_graph yvex_graph;

typedef enum {
    YVEX_GRAPH_STATUS_EMPTY = 0,
    YVEX_GRAPH_STATUS_BUILT,
    YVEX_GRAPH_STATUS_PARTIAL,
    YVEX_GRAPH_STATUS_UNSUPPORTED,
    YVEX_GRAPH_STATUS_INVALID
} yvex_graph_status;

typedef struct {
    unsigned long long sequence_length;
    unsigned long long context_length;
    int include_decode_step;
    int include_prefill_path;
} yvex_graph_build_options;

typedef struct {
    yvex_tensor_role role;
    const char *role_name;
    const char *reason;
} yvex_graph_missing_required;

int yvex_graph_build_for_model(yvex_graph **out,
                                const yvex_model_descriptor *model,
                                const yvex_tensor_table *tensors,
                                const yvex_graph_build_options *options,
                                yvex_error *err);

void yvex_graph_close(yvex_graph *graph);

yvex_graph_status yvex_graph_status_of(const yvex_graph *graph);
const char *yvex_graph_status_name(yvex_graph_status status);

unsigned long long yvex_graph_value_count(const yvex_graph *graph);
unsigned long long yvex_graph_op_count(const yvex_graph *graph);
unsigned long long yvex_graph_missing_required_count(const yvex_graph *graph);

const yvex_graph_value_info *yvex_graph_value_at(const yvex_graph *graph,
                                                 unsigned long long index);
const yvex_graph_op_info *yvex_graph_op_at(const yvex_graph *graph,
                                           unsigned long long index);
const yvex_graph_missing_required *yvex_graph_missing_required_at(const yvex_graph *graph,
                                                                  unsigned long long index);

int yvex_graph_dump(const yvex_graph *graph, FILE *fp, yvex_error *err);

int yvex_shape_product(const unsigned long long *dims,
                       unsigned int rank,
                       unsigned long long *out,
                       yvex_error *err);
int yvex_shape_equal(const unsigned long long *a,
                     unsigned int a_rank,
                     const unsigned long long *b,
                     unsigned int b_rank);
int yvex_shape_copy(unsigned long long *dst,
                    unsigned int dst_cap,
                    const unsigned long long *src,
                    unsigned int src_rank,
                    yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GRAPH_H */
