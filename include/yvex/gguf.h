/*
 * YVEX - GGUF parser
 *
 * File: include/yvex/gguf.h
 * Layer: public format API
 *
 * Purpose:
 *   Defines the implemented GGUF probe, header, metadata, and raw tensor
 *   directory API. This surface parses GGUF directory structure only; model
 *   descriptors, tensor tables, tokenizers, and execution remain future work.
 *
 * Owns:
 *   - YVEX_GGUF_MAGIC
 *   - yvex_gguf_header
 *   - yvex_gguf_probe
 *   - yvex_gguf
 *   - yvex_gguf_value
 *   - yvex_gguf_tensor_info
 *   - yvex_gguf_probe_file
 *   - yvex_gguf_read_header
 *   - yvex_gguf_open
 *   - yvex_gguf_close
 *   - metadata and tensor directory accessors
 *
 * Does not own:
 *   - qtype support
 *   - YVEX tensor table
 *   - model descriptor
 *   - model execution
 *
 * Used by:
 *   - yvex inspect
 *   - yvex metadata
 *   - yvex tensors
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_gguf
 */
#ifndef YVEX_GGUF_H
#define YVEX_GGUF_H

#include <yvex/artifact.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_GGUF_MAGIC 0x46554747u
#define YVEX_GGUF_MAX_DIMS 4u

typedef struct {
    unsigned int version;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
} yvex_gguf_header;

typedef struct {
    int is_gguf;
    yvex_gguf_header header;
} yvex_gguf_probe;

typedef struct yvex_gguf yvex_gguf;
typedef struct yvex_gguf_value yvex_gguf_value;

typedef enum {
    YVEX_GGUF_VALUE_UINT8 = 0,
    YVEX_GGUF_VALUE_INT8 = 1,
    YVEX_GGUF_VALUE_UINT16 = 2,
    YVEX_GGUF_VALUE_INT16 = 3,
    YVEX_GGUF_VALUE_UINT32 = 4,
    YVEX_GGUF_VALUE_INT32 = 5,
    YVEX_GGUF_VALUE_FLOAT32 = 6,
    YVEX_GGUF_VALUE_BOOL = 7,
    YVEX_GGUF_VALUE_STRING = 8,
    YVEX_GGUF_VALUE_ARRAY = 9,
    YVEX_GGUF_VALUE_UINT64 = 10,
    YVEX_GGUF_VALUE_INT64 = 11,
    YVEX_GGUF_VALUE_FLOAT64 = 12
} yvex_gguf_value_type;

typedef struct {
    const unsigned char *data;
    unsigned long long len;
} yvex_gguf_bytes;

typedef struct {
    yvex_gguf_value_type element_type;
    unsigned long long count;
} yvex_gguf_array_info;

typedef struct {
    const char *name;
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_MAX_DIMS];
    unsigned int ggml_type;
    const char *ggml_type_name;
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
} yvex_gguf_tensor_info;

int yvex_gguf_probe_file(const yvex_artifact *artifact, yvex_gguf_probe *out, yvex_error *err);
int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err);

int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err);
void yvex_gguf_close(yvex_gguf *gguf);

const yvex_gguf_header *yvex_gguf_header_view(const yvex_gguf *gguf);
const char *yvex_gguf_value_type_name(yvex_gguf_value_type type);

unsigned long long yvex_gguf_metadata_count(const yvex_gguf *gguf);
const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key);

yvex_gguf_value_type yvex_gguf_value_type_of(const yvex_gguf_value *value);
int yvex_gguf_value_as_u64(const yvex_gguf_value *value, unsigned long long *out);
int yvex_gguf_value_as_i64(const yvex_gguf_value *value, long long *out);
int yvex_gguf_value_as_f64(const yvex_gguf_value *value, double *out);
int yvex_gguf_value_as_bool(const yvex_gguf_value *value, int *out);
int yvex_gguf_value_as_string(const yvex_gguf_value *value, const char **data, unsigned long long *len);
int yvex_gguf_value_array_info(const yvex_gguf_value *value, yvex_gguf_array_info *out);

unsigned long long yvex_gguf_tensor_count(const yvex_gguf *gguf);
const yvex_gguf_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name);
unsigned long long yvex_gguf_tensor_data_offset(const yvex_gguf *gguf);
unsigned int yvex_gguf_alignment(const yvex_gguf *gguf);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_H */
