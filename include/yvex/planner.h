/*
 * YVEX - Planner
 *
 * File: include/yvex/planner.h
 * Layer: public planning API
 *
 * Purpose:
 *   Defines the F0 high-level plan object that owns a graph and memory plan.
 *   Backend names are labels only in F0 and do not activate execution.
 *
 * Owns:
 *   - yvex_plan
 *   - plan options
 *   - plan dump surface
 *
 * Does not own:
 *   - backend ABI
 *   - kernel dispatch
 *   - session execution
 *
 * Used by:
 *   - CLI plan command
 *   - planner tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_planner
 */
#ifndef YVEX_PLANNER_H
#define YVEX_PLANNER_H

#include <stdio.h>

#include <yvex/memory_plan.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_plan yvex_plan;

typedef struct {
    unsigned long long sequence_length;
    unsigned long long context_length;
    const char *backend_name;
} yvex_plan_options;

int yvex_plan_create(yvex_plan **out,
                     const yvex_model_descriptor *model,
                     const yvex_tensor_table *tensors,
                     const yvex_plan_options *options,
                     yvex_error *err);

void yvex_plan_close(yvex_plan *plan);

const yvex_graph *yvex_plan_graph(const yvex_plan *plan);
const yvex_memory_plan *yvex_plan_memory(const yvex_plan *plan);

int yvex_plan_dump(const yvex_plan *plan, FILE *fp, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_PLANNER_H */
