/*
 * YVEX - Materialization reporting
 */
#include "yvex_weights_internal.h"

#include <string.h>

int yvex_weight_table_get_summary(const yvex_weight_table *weights,
                                  yvex_materialize_summary *out,
                                  yvex_error *err)
{
    yvex_backend_memory_stats stats;

    if (!weights || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_weight_table_get_summary",
                       "weights and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    memcpy(out, &weights->summary, sizeof(*out));
    if (weights->backend &&
        yvex_backend_get_memory_stats(weights->backend, &stats, err) == YVEX_OK) {
        out->backend_allocated_bytes = stats.allocated_bytes;
    }
    return YVEX_OK;
}
