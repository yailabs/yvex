/*
 * YVEX - Memory plan
 *
 * File: include/yvex/memory_plan.h
 * Layer: public planning API
 *
 * Purpose:
 *   Defines an estimate-only memory plan built from F0 graph and tensor-table
 *   facts. The plan does not allocate memory or query a backend.
 *
 * Owns:
 *   - yvex_memory_plan
 *   - memory plan status
 *   - memory plan summary and dump
 *
 * Does not own:
 *   - backend memory allocation
 *   - KV cache implementation
 *   - execution sessions
 *
 * Used by:
 *   - planner
 *   - CLI plan command
 *   - memory plan tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_memory_plan
 */
#ifndef YVEX_MEMORY_PLAN_H
#define YVEX_MEMORY_PLAN_H

#include <stdio.h>

#include <yvex/graph.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_memory_plan yvex_memory_plan;

typedef enum {
    YVEX_MEMORY_PLAN_EMPTY = 0,
    YVEX_MEMORY_PLAN_ESTIMATED,
    YVEX_MEMORY_PLAN_PARTIAL,
    YVEX_MEMORY_PLAN_UNSUPPORTED
} yvex_memory_plan_status;

typedef struct {
    unsigned long long model_tensor_bytes_known;
    unsigned long long model_tensor_bytes_unknown_count;
    unsigned long long activation_peak_bytes;
    unsigned long long kv_cache_bytes;
    unsigned long long scratch_peak_bytes;
    unsigned long long total_known_bytes;
} yvex_memory_plan_summary;

int yvex_memory_plan_from_graph(yvex_memory_plan **out,
                                const yvex_graph *graph,
                                const yvex_tensor_table *tensors,
                                yvex_error *err);

void yvex_memory_plan_close(yvex_memory_plan *plan);

yvex_memory_plan_status yvex_memory_plan_status_of(const yvex_memory_plan *plan);
const char *yvex_memory_plan_status_name(yvex_memory_plan_status status);

int yvex_memory_plan_get_summary(const yvex_memory_plan *plan,
                                 yvex_memory_plan_summary *out,
                                 yvex_error *err);

int yvex_memory_plan_dump(const yvex_memory_plan *plan,
                          FILE *fp,
                          yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MEMORY_PLAN_H */
