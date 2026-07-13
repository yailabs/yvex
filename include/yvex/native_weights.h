/*
 * YVEX - Native open-weight inventory API
 *
 * File: include/yvex/native_weights.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the open-weight intake metadata-only native weight inventory surface for
 *   Hugging Face safetensors source trees.
 *
 * Does not own:
 *   - tensor payload loading
 *   - dequantization
 *   - quantization
 *   - GGUF emission
 *   - materialization
 *   - inference
 */
#ifndef YVEX_NATIVE_WEIGHTS_H
#define YVEX_NATIVE_WEIGHTS_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_NATIVE_WEIGHT_MAX_DIMS 8

typedef struct yvex_native_weight_table yvex_native_weight_table;

typedef enum {
    YVEX_NATIVE_DTYPE_UNKNOWN = 0,
    YVEX_NATIVE_DTYPE_F64,
    YVEX_NATIVE_DTYPE_F32,
    YVEX_NATIVE_DTYPE_F16,
    YVEX_NATIVE_DTYPE_BF16,
    YVEX_NATIVE_DTYPE_I64,
    YVEX_NATIVE_DTYPE_I32,
    YVEX_NATIVE_DTYPE_I16,
    YVEX_NATIVE_DTYPE_I8,
    YVEX_NATIVE_DTYPE_U8,
    YVEX_NATIVE_DTYPE_BOOL,
    YVEX_NATIVE_DTYPE_F8_E4M3,
    YVEX_NATIVE_DTYPE_F8_E5M2,
    YVEX_NATIVE_DTYPE_F8_E8M0,
    YVEX_NATIVE_DTYPE_FP4,
    YVEX_NATIVE_DTYPE_OTHER
} yvex_native_dtype;

typedef struct {
    const char *name;
    const char *shard_path;
    yvex_native_dtype dtype;
    const char *dtype_name;
    unsigned int rank;
    unsigned long long dims[YVEX_NATIVE_WEIGHT_MAX_DIMS];
    unsigned long long data_start;
    unsigned long long data_end;
    unsigned long long data_bytes;
} yvex_native_weight_info;

typedef struct {
    unsigned long long shard_count;
    unsigned long long tensor_count;
    unsigned long long total_tensor_bytes;
    unsigned long long unknown_dtype_count;
    unsigned long long malformed_shard_count;
} yvex_native_weight_summary;

typedef struct {
    const char *source_dir;
    int recursive;
    int include_metadata;
} yvex_native_weight_options;

int yvex_native_weight_table_open(yvex_native_weight_table **out,
                                  const yvex_native_weight_options *options,
                                  yvex_error *err);

void yvex_native_weight_table_close(yvex_native_weight_table *table);

unsigned long long yvex_native_weight_table_count(const yvex_native_weight_table *table);

const yvex_native_weight_info *yvex_native_weight_table_at(const yvex_native_weight_table *table,
                                                           unsigned long long index);

const yvex_native_weight_info *yvex_native_weight_table_find(const yvex_native_weight_table *table,
                                                             const char *name);

int yvex_native_weight_table_summary(const yvex_native_weight_table *table,
                                     yvex_native_weight_summary *out,
                                     yvex_error *err);

const char *yvex_native_dtype_name(yvex_native_dtype dtype);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_NATIVE_WEIGHTS_H */
