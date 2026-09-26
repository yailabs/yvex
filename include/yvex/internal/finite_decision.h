/* Host inspection of the exact resident computational engine. */
#ifndef INCLUDE_YVEX_INTERNAL_FINITE_DECISION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_FINITE_DECISION_H_INCLUDED

#include <yvex/finite_decision.h>
#include <yvex/internal/tensor_engine.h>

#ifdef __cplusplus
extern "C" {
#endif

int yvex_finite_decision_engine_summary_copy(const yvex_finite_decision_engine *,
    yvex_tensor_engine_summary *, yvex_error *);

#ifdef __cplusplus
}
#endif
#endif
