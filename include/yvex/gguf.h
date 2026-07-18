/*
 * Owner: abi.gguf (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - GGUF parser
 *
 * File: include/yvex/gguf.h
 * Layer: public format API
 *
 * Purpose:
 *   Defines the file-backed GGUF v3 structural reader, immutable parsed view,
 *   typed refusal result, resource budgets, and metadata/tensor accessors.
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
 *   - qtype storage geometry
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
    YVEX_GGUF_VALUE_FLOAT64 = 12,
    YVEX_GGUF_VALUE_INVALID = 13
} yvex_gguf_value_type;

typedef enum {
    YVEX_GGUF_PARSE_SECTION_NONE = 0,
    YVEX_GGUF_PARSE_SECTION_FILE = 1,
    YVEX_GGUF_PARSE_SECTION_CONTAINER = 2,
    YVEX_GGUF_PARSE_SECTION_METADATA = 3,
    YVEX_GGUF_PARSE_SECTION_TENSOR_INFO = 4,
    YVEX_GGUF_PARSE_SECTION_QTYPE = 5,
    YVEX_GGUF_PARSE_SECTION_RANGE = 6,
    YVEX_GGUF_PARSE_SECTION_RESOURCE = 7
} yvex_gguf_parse_section;

typedef enum {
    YVEX_GGUF_PARSE_OK = 0,
    YVEX_GGUF_PARSE_INVALID_ARGUMENT = 1,
    YVEX_GGUF_PARSE_FILE_UNREADABLE = 2,
    YVEX_GGUF_PARSE_SHORT_READ = 3,
    YVEX_GGUF_PARSE_INVALID_MAGIC = 4,
    YVEX_GGUF_PARSE_UNSUPPORTED_VERSION = 5,
    YVEX_GGUF_PARSE_INVALID_COUNT = 6,
    YVEX_GGUF_PARSE_RESOURCE_LIMIT = 7,
    YVEX_GGUF_PARSE_MALFORMED_KEY = 8,
    YVEX_GGUF_PARSE_DUPLICATE_METADATA_KEY = 9,
    YVEX_GGUF_PARSE_UNSUPPORTED_METADATA_TYPE = 10,
    YVEX_GGUF_PARSE_MALFORMED_VALUE = 11,
    YVEX_GGUF_PARSE_MALFORMED_STRING = 12,
    YVEX_GGUF_PARSE_MALFORMED_ARRAY = 13,
    YVEX_GGUF_PARSE_INVALID_ALIGNMENT = 14,
    YVEX_GGUF_PARSE_MALFORMED_TENSOR_NAME = 15,
    YVEX_GGUF_PARSE_DUPLICATE_TENSOR_NAME = 16,
    YVEX_GGUF_PARSE_INVALID_RANK = 17,
    YVEX_GGUF_PARSE_INVALID_DIMENSION = 18,
    YVEX_GGUF_PARSE_REFUSED_QTYPE = 19,
    YVEX_GGUF_PARSE_OFFSET_OVERFLOW = 20,
    YVEX_GGUF_PARSE_INCOMPLETE_DIRECTORY = 21,
    YVEX_GGUF_PARSE_ALLOCATION_FAILURE = 22,
    YVEX_GGUF_PARSE_EMPTY_METADATA_KEY = 23,
    YVEX_GGUF_PARSE_EMPTY_TENSOR_NAME = 24,
    YVEX_GGUF_PARSE_ELEMENT_COUNT_OVERFLOW = 25,
    YVEX_GGUF_PARSE_ROW_COUNT_OVERFLOW = 26,
    YVEX_GGUF_PARSE_ROW_BYTE_OVERFLOW = 27,
    YVEX_GGUF_PARSE_TOTAL_BYTE_OVERFLOW = 28
} yvex_gguf_parse_code;

typedef struct {
    yvex_gguf_parse_code code;
    yvex_gguf_parse_section section;
    unsigned long long byte_offset;
    unsigned long long record_index;
    const char *reason;
} yvex_gguf_parse_result;

typedef struct {
    unsigned long long max_metadata_entries;
    unsigned long long max_tensor_entries;
    unsigned long long max_array_entries;
    unsigned long long max_total_array_entries;
    unsigned long long max_string_bytes;
    unsigned long long max_total_string_bytes;
    unsigned long long max_owned_bytes;
    unsigned long long max_structural_bytes;
    unsigned int max_array_depth;
} yvex_gguf_reader_options;

typedef struct {
    unsigned long long file_size;
    unsigned long long structural_bytes_read;
    unsigned long long payload_bytes_read;
    unsigned long long read_calls;
    unsigned long long owned_bytes;
    unsigned long long total_string_bytes;
    unsigned long long total_array_entries;
    unsigned long long directory_end_offset;
} yvex_gguf_reader_stats;

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
    unsigned long long name_len;
    unsigned int rank;
    unsigned long long dims[YVEX_GGUF_MAX_DIMS];
    unsigned int ggml_type;
    const char *ggml_type_name;
    unsigned long long relative_offset;
    unsigned long long absolute_offset;
    unsigned long long storage_bytes;
    unsigned long long absolute_end_offset;
    int range_addressable;
} yvex_gguf_tensor_info;

int yvex_gguf_probe_file(const yvex_artifact *artifact, yvex_gguf_probe *out, yvex_error *err);
int yvex_gguf_read_header(const yvex_artifact *artifact, yvex_gguf_header *out, yvex_error *err);

void yvex_gguf_reader_options_default(yvex_gguf_reader_options *options);
/*
 * Opens one immutable structural view. The view owns decoded metadata, names,
 * tensors, and indexes; it does not borrow artifact storage after return.
 * Every accessor result remains valid until yvex_gguf_close.
 */
int yvex_gguf_open_ex(yvex_gguf **out,
                      const yvex_artifact *artifact,
                      const yvex_gguf_reader_options *options,
                      yvex_gguf_parse_result *result,
                      yvex_error *err);
int yvex_gguf_open(yvex_gguf **out, const yvex_artifact *artifact, yvex_error *err);
/* Safe for null and for every successfully returned parsed view. */
void yvex_gguf_close(yvex_gguf *gguf);

const char *yvex_gguf_parse_code_name(yvex_gguf_parse_code code);
const char *yvex_gguf_parse_section_name(yvex_gguf_parse_section section);

const yvex_gguf_header *yvex_gguf_header_view(const yvex_gguf *gguf);
const char *yvex_gguf_value_type_name(yvex_gguf_value_type type);

unsigned long long yvex_gguf_metadata_count(const yvex_gguf *gguf);
const char *yvex_gguf_metadata_key(const yvex_gguf *gguf, unsigned long long index);
unsigned long long yvex_gguf_metadata_key_len(const yvex_gguf *gguf,
                                               unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_value(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_value *yvex_gguf_metadata_find(const yvex_gguf *gguf, const char *key);

yvex_gguf_value_type yvex_gguf_value_type_of(const yvex_gguf_value *value);
int yvex_gguf_value_as_u64(const yvex_gguf_value *value, unsigned long long *out);
int yvex_gguf_value_as_i64(const yvex_gguf_value *value, long long *out);
int yvex_gguf_value_as_f64(const yvex_gguf_value *value, double *out);
int yvex_gguf_value_as_bool(const yvex_gguf_value *value, int *out);
int yvex_gguf_value_as_string(const yvex_gguf_value *value, const char **data, unsigned long long *len);
int yvex_gguf_value_array_info(const yvex_gguf_value *value, yvex_gguf_array_info *out);
const yvex_gguf_value *yvex_gguf_value_array_at(const yvex_gguf_value *value, unsigned long long index);

unsigned long long yvex_gguf_tensor_count(const yvex_gguf *gguf);
const yvex_gguf_tensor_info *yvex_gguf_tensor_at(const yvex_gguf *gguf, unsigned long long index);
const yvex_gguf_tensor_info *yvex_gguf_tensor_find(const yvex_gguf *gguf, const char *name);
unsigned long long yvex_gguf_tensor_data_offset(const yvex_gguf *gguf);
unsigned int yvex_gguf_alignment(const yvex_gguf *gguf);
unsigned long long yvex_gguf_file_size(const yvex_gguf *gguf);
const yvex_gguf_reader_stats *yvex_gguf_reader_stats_view(const yvex_gguf *gguf);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_GGUF_H */
