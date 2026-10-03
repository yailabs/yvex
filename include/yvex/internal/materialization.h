/* Materialization outcomes outlive retirement without changing the public weight ABI. */
#ifndef INCLUDE_YVEX_INTERNAL_MATERIALIZATION_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_MATERIALIZATION_H_INCLUDED

#include <yvex/materialization.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Synchronous projection of the same operation. On failure, *out is NULL and report
 * survives resource retirement. Strings are static except backend_name, borrowed
 * from options (or the static backend kind name). Cleanup PASS requires observed
 * backend accounting to return to its pre-operation value. */
int yvex_weight_table_materialize_report(
    yvex_weight_table **out,
    const yvex_artifact *artifact,
    const yvex_gguf *gguf,
    const yvex_tensor_table *tensors,
    yvex_backend *backend,
    const yvex_materialize_options *options,
    yvex_materialize_summary *report,
    yvex_error *err);

#ifdef __cplusplus
}
#endif
#endif
