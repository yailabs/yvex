/*
 * YVEX - Materialized weight internals
 */
#ifndef YVEX_WEIGHTS_INTERNAL_H
#define YVEX_WEIGHTS_INTERNAL_H

#include <yvex/weights.h>

struct yvex_materialized_weight {
    char *name;
    yvex_dtype dtype;
    yvex_tensor_role role;
    unsigned long long bytes;
    yvex_weight_residency residency;
    yvex_device_tensor *device_tensor;
};

struct yvex_weight_table {
    yvex_backend *backend;
    char *backend_name;
    yvex_materialized_weight *items;
    unsigned long long count;
    yvex_materialize_summary summary;
};

char *yvex_weight_strdup(const char *text);
void yvex_materialized_weight_clear(yvex_weight_table *table,
                                    yvex_materialized_weight *weight);

#endif /* YVEX_WEIGHTS_INTERNAL_H */
