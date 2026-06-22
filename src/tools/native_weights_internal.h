/*
 * YVEX - Native weight internals
 *
 * File: src/tools/native_weights_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_NATIVE_WEIGHTS_INTERNAL_H
#define YVEX_NATIVE_WEIGHTS_INTERNAL_H

#include <stddef.h>

#include <yvex/native_weights.h>

struct yvex_native_weight_table {
    yvex_native_weight_info *items;
    unsigned long long count;
    unsigned long long cap;
    yvex_native_weight_summary summary;
};

int yvex_native_weight_table_add(yvex_native_weight_table *table,
                                 const char *name,
                                 const char *shard_path,
                                 const char *dtype_name,
                                 unsigned int rank,
                                 const unsigned long long *dims,
                                 unsigned long long data_start,
                                 unsigned long long data_end,
                                 yvex_error *err);

int yvex_safetensors_read_header_file(const char *abs_path,
                                      const char *shard_path,
                                      yvex_native_weight_table *table,
                                      yvex_error *err);

int yvex_safetensors_parse_header(const char *json,
                                  unsigned long long payload_bytes,
                                  const char *shard_path,
                                  yvex_native_weight_table *table,
                                  yvex_error *err);

int yvex_native_weight_report_json(const char *source,
                                   const yvex_native_weight_table *table,
                                   yvex_error *err);

#endif /* YVEX_NATIVE_WEIGHTS_INTERNAL_H */
