/*
 * YVEX - Native weight report helpers
 *
 * File: yvex_native_weight_report.c
 * Layer: tool-plane implementation
 */
#include "yvex_native_weights_internal.h"

#include <stdio.h>

int yvex_native_weight_report_json(const char *source,
                                   const yvex_native_weight_table *table,
                                   yvex_error *err)
{
    yvex_native_weight_summary summary;

    if (yvex_native_weight_table_summary(table, &summary, err) != YVEX_OK) {
        return yvex_error_code(err);
    }
    printf("{\n");
    printf("  \"schema\": \"yvex.native_weights.v1\",\n");
    printf("  \"source\": \"%s\",\n", source ? source : "");
    printf("  \"summary\": {\n");
    printf("    \"shard_count\": %llu,\n", summary.shard_count);
    printf("    \"tensor_count\": %llu,\n", summary.tensor_count);
    printf("    \"total_tensor_bytes\": %llu,\n", summary.total_tensor_bytes);
    printf("    \"unknown_dtype_count\": %llu,\n", summary.unknown_dtype_count);
    printf("    \"malformed_shard_count\": %llu\n", summary.malformed_shard_count);
    printf("  }\n");
    printf("}\n");
    return YVEX_OK;
}
