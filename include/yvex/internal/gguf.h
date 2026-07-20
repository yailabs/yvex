/* Owner: gguf.internal (gguf).
 * Owns: GGUF private facts, mapping, tokenizer projection, and roundtrip validation.
 * Does not own: writer publication, quant policy, or runtime support.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: container-owned internal parsing and equivalence.
 * Purpose: provide the canonical container-owned internal parsing and equivalence contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_GGUF_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_GGUF_H_INCLUDED

#include <stddef.h>
#include <yvex/artifact.h>
#include <yvex/core.h>
#include <yvex/gguf.h>
#include <yvex/model.h>
#include <yvex/qtype.h>
#include <yvex/internal/gguf_writer.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/source.h>

#ifdef __cplusplus
extern "C" {
#endif

int yvex_gguf_qtype_reference_dequantization_supported(unsigned int qtype);

/* Private contract. */
#define YVEX_GGUF_ABI_NEXT_ROW "V010.CUDA.FAILCLOSED.0"
#define YVEX_GGUF_QTYPE_ABI_NEXT_ROW "V010.GGUF.ARTIFACT.ABI.1"
typedef enum {
    YVEX_GGUF_BOUNDARY_OPERATIONAL = 0,
    YVEX_GGUF_BOUNDARY_REPORT_ONLY = 1,
    YVEX_GGUF_BOUNDARY_UNSUPPORTED = 2,
    YVEX_GGUF_BOUNDARY_REFUSED = 3
} yvex_gguf_boundary_status;
typedef enum {
    YVEX_GGUF_ABI_SECTION_NOT_EVALUATED = 0,
    YVEX_GGUF_ABI_SECTION_OK = 1,
    YVEX_GGUF_ABI_SECTION_REPORT_ONLY = 2,
    YVEX_GGUF_ABI_SECTION_REFUSED = 3,
    YVEX_GGUF_ABI_SECTION_UNSUPPORTED = 4,
    YVEX_GGUF_ABI_SECTION_MALFORMED = 5,
    YVEX_GGUF_ABI_SECTION_NOT_PRESENT = 6
} yvex_gguf_abi_section_status;
typedef struct {
    const char *owner;
    const char *stage;
    yvex_gguf_boundary_status status;
    const char *reason;
    const char *next_row;
} yvex_gguf_boundary_fact;
typedef struct {
    unsigned int qtype;
    const char *name;
    const char *identity_status;
    const char *storage_class;
    unsigned int block_size;
    unsigned int bytes_per_block;
    unsigned int scalar_width;
    const char *storage_status;
    const char *reference_dequantization;
    unsigned long long expected_storage_bytes;
    const char *reason;
    const char *next_row;
} yvex_gguf_qtype_report_row;
typedef struct {
    yvex_gguf_abi_section_status status;
    unsigned int magic;
    unsigned int version;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    const char *reason;
} yvex_gguf_container_abi;
typedef struct {
    yvex_gguf_abi_section_status status;
    unsigned long long entry_count;
    unsigned long long string_value_count;
    unsigned long long array_value_count;
    const char *reason;
} yvex_gguf_metadata_abi;
typedef struct {
    yvex_gguf_abi_section_status status;
    unsigned long long tensor_count;
    unsigned int max_rank;
    unsigned long long rank_one_tensor_count;
    unsigned long long named_tensor_count;
    unsigned long long qtype_known_tensor_count;
    unsigned long long qtype_refused_tensor_count;
    const char *reason;
} yvex_gguf_tensor_info_abi;
typedef struct {
    yvex_gguf_abi_section_status status;
    unsigned long long checked_tensor_count;
    unsigned long long known_tensor_count;
    unsigned long long refused_tensor_count;
    unsigned long long total_storage_bytes;
    unsigned int first_refused_qtype;
    const char *reason;
    const char *next_row;
} yvex_gguf_qtype_abi;
typedef struct {
    yvex_gguf_abi_section_status status;
    unsigned long long checked_tensor_count;
    unsigned long long tensor_data_offset;
    unsigned long long file_size;
    unsigned long long total_expected_storage_bytes;
    unsigned long long first_expected_storage_bytes;
    unsigned long long first_actual_available_bytes;
    unsigned long long qtype_checked_tensor_count;
    unsigned int alignment;
    const char *reason;
} yvex_gguf_range_fact;
typedef struct {
    yvex_gguf_abi_section_status status;
    const char *reason;
} yvex_gguf_descriptor_abi;
typedef struct {
    const char *path;
    yvex_gguf_abi_section_status status;
    yvex_gguf_container_abi container;
    yvex_gguf_metadata_abi metadata;
    yvex_gguf_tensor_info_abi tensor_info;
    yvex_gguf_qtype_abi qtype;
    yvex_gguf_layout_result layout;
    yvex_gguf_range_fact range;
    yvex_gguf_descriptor_abi descriptor;
    int parser_status;
    yvex_gguf_parse_result parse_result;
    yvex_gguf_reader_stats reader_stats;
    char failure_where[YVEX_ERROR_WHERE_CAP];
    char failure_reason[YVEX_ERROR_MESSAGE_CAP];
    const char *next_row;
} yvex_gguf_abi_report;
typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_gguf_report_fact;
void yvex_gguf_container_abi_init(yvex_gguf_container_abi *abi);
void yvex_gguf_container_abi_from_header(const yvex_gguf_header *header,
                                         yvex_gguf_container_abi *abi);
void yvex_gguf_metadata_abi_init(yvex_gguf_metadata_abi *abi);
int yvex_gguf_metadata_abi_from_gguf(const yvex_gguf *gguf,
                                     yvex_gguf_metadata_abi *abi,
                                     const char **reason);
void yvex_gguf_tensor_info_abi_init(yvex_gguf_tensor_info_abi *abi);
int yvex_gguf_tensor_info_abi_from_gguf(const yvex_gguf *gguf,
                                        yvex_gguf_tensor_info_abi *abi,
                                        const char **reason);
void yvex_gguf_qtype_abi_init(yvex_gguf_qtype_abi *abi);
int yvex_gguf_qtype_abi_from_gguf(const yvex_gguf *gguf,
                                  yvex_gguf_qtype_abi *abi,
                                  const char **reason);
void yvex_gguf_qtype_report_row_from_geometry(const yvex_gguf_qtype_geometry *geometry,
                                              const unsigned long long *dims,
                                              unsigned int rank,
                                              yvex_gguf_qtype_report_row *row);
void yvex_gguf_range_fact_init(yvex_gguf_range_fact *fact);
int yvex_gguf_range_fact_from_layout(const yvex_gguf_layout_result *layout,
                                     yvex_gguf_range_fact *fact,
                                     const char **reason);
void yvex_gguf_parse_result_reset(yvex_gguf_parse_result *result);
int yvex_gguf_reader_fail(yvex_gguf_parse_result *result,
                          yvex_gguf_parse_code code,
                          yvex_gguf_parse_section section,
                          unsigned long long byte_offset,
                          unsigned long long record_index,
                          yvex_error *err,
                          const char *where,
                          const char *reason);
void yvex_gguf_reader_classify_error(int parse_rc,
                                     const yvex_gguf_parse_result *result,
                                     const yvex_error *err,
                                     yvex_gguf_abi_report *report);
int yvex_gguf_writer_supported(const char **reason);
int yvex_gguf_roundtrip_supported(const char **reason);
void yvex_gguf_descriptor_abi_from_sections(const yvex_gguf_container_abi *container,
                                            const yvex_gguf_metadata_abi *metadata,
                                            const yvex_gguf_tensor_info_abi *tensor_info,
                                            const yvex_gguf_qtype_abi *qtype,
                                            const yvex_gguf_range_fact *range,
                                            yvex_gguf_descriptor_abi *descriptor);
int yvex_gguf_artifact_abi_report_build(const char *path,
                                        yvex_gguf_abi_report *report,
                                        yvex_error *err);

/* Map contract. */
#define YVEX_GGUF_MAPPING_REFERENCE_COMMIT \
    "e920c523e3b8a0163fe498af5bf90df35ff51d25"
#define YVEX_GGUF_MTP_EXTENSION_VERSION 1u
#define YVEX_GGUF_NO_FORCED_QTYPE (~0u)
int yvex_gguf_name_map_resolve(yvex_tensor_role role,
                               int mtp_extension,
                               unsigned long long layer_index,
                               unsigned long long predictor_index,
                               char *out,
                               size_t out_cap,
                               yvex_gguf_name_provenance *provenance,
                               const char **reason);
int yvex_gguf_layout_map_shape_supported(yvex_tensor_role role,
                                         unsigned int qtype,
                                         unsigned int rank,
                                         const unsigned long long *dims,
                                         const char **reason);

/* Tokenizer Metadata contract. */
#define YVEX_GGUF_TOKENIZER_SHA256_CAP 65u
typedef enum {
    YVEX_GGUF_TOKENIZER_OK = 0,
    YVEX_GGUF_TOKENIZER_INVALID_ARGUMENT,
    YVEX_GGUF_TOKENIZER_SOURCE_IDENTITY,
    YVEX_GGUF_TOKENIZER_RESOURCE_LIMIT,
    YVEX_GGUF_TOKENIZER_ALLOCATION,
    YVEX_GGUF_TOKENIZER_MALFORMED_JSON,
    YVEX_GGUF_TOKENIZER_UNSUPPORTED_JSON,
    YVEX_GGUF_TOKENIZER_DUPLICATE_TOKEN_ID,
    YVEX_GGUF_TOKENIZER_MISSING_TOKEN_ID,
    YVEX_GGUF_TOKENIZER_TOKEN_MISMATCH,
    YVEX_GGUF_TOKENIZER_CARDINALITY,
    YVEX_GGUF_TOKENIZER_SPECIAL_TOKEN
} yvex_gguf_tokenizer_code;
typedef struct {
    yvex_gguf_tokenizer_code code;
    unsigned long long record_index;
    unsigned long long expected;
    unsigned long long actual;
    char field[64];
} yvex_gguf_tokenizer_failure;
typedef struct {
    unsigned long long tokenizer_json_bytes;
    unsigned long long tokenizer_config_bytes;
    unsigned long long token_count;
    unsigned long long merge_count;
    unsigned long long added_token_count;
    unsigned long long owned_bytes;
    unsigned long long decoded_string_bytes;
    unsigned int bos_token_id;
    unsigned int eos_token_id;
    unsigned int pad_token_id;
    int add_bos_token;
    int add_eos_token;
    int chat_template_present;
    char pre_tokenizer[32];
    char tokenizer_json_sha256[YVEX_GGUF_TOKENIZER_SHA256_CAP];
    char tokenizer_config_sha256[YVEX_GGUF_TOKENIZER_SHA256_CAP];
    char tokenizer_json_git_oid[41];
    char tokenizer_config_git_oid[41];
    int complete;
} yvex_gguf_tokenizer_summary;
typedef struct yvex_gguf_tokenizer_metadata yvex_gguf_tokenizer_metadata;
int yvex_gguf_tokenizer_metadata_load(
    yvex_gguf_tokenizer_metadata **out,
    const yvex_source_verification *verification,
    unsigned long long expected_vocab_size,
    const char *pre_tokenizer,
    size_t maximum_owned_bytes,
    yvex_gguf_tokenizer_failure *failure,
    yvex_error *err);
void yvex_gguf_tokenizer_metadata_release(
    yvex_gguf_tokenizer_metadata **metadata);
const yvex_gguf_tokenizer_summary *yvex_gguf_tokenizer_summary_get(
    const yvex_gguf_tokenizer_metadata *metadata);
int yvex_gguf_tokenizer_token_at(
    const yvex_gguf_tokenizer_metadata *metadata,
    unsigned long long index,
    const unsigned char **bytes,
    size_t *byte_count,
    int *token_type);
int yvex_gguf_tokenizer_merge_at(
    const yvex_gguf_tokenizer_metadata *metadata,
    unsigned long long index,
    const unsigned char **bytes,
    size_t *byte_count);
int yvex_gguf_tokenizer_raw_json(
    const yvex_gguf_tokenizer_metadata *metadata,
    const unsigned char **bytes,
    size_t *byte_count);
int yvex_gguf_tokenizer_raw_config(
    const yvex_gguf_tokenizer_metadata *metadata,
    const unsigned char **bytes,
    size_t *byte_count);

/* Roundtrip contract. */
typedef enum {
    YVEX_GGUF_ROUNDTRIP_OK = 0,
    YVEX_GGUF_ROUNDTRIP_INVALID_ARGUMENT,
    YVEX_GGUF_ROUNDTRIP_ARTIFACT_OPEN,
    YVEX_GGUF_ROUNDTRIP_READER_REFUSAL,
    YVEX_GGUF_ROUNDTRIP_LAYOUT_REFUSAL,
    YVEX_GGUF_ROUNDTRIP_HEADER_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_METADATA_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_TENSOR_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_PREFIX_DIVERGENCE,
    YVEX_GGUF_ROUNDTRIP_PAYLOAD_DIGEST,
    YVEX_GGUF_ROUNDTRIP_ARTIFACT_DIGEST,
    YVEX_GGUF_ROUNDTRIP_SHORT_READ,
    YVEX_GGUF_ROUNDTRIP_NONZERO_PADDING,
    YVEX_GGUF_ROUNDTRIP_TOKENIZER_INCOMPLETE,
    YVEX_GGUF_ROUNDTRIP_FILE_DRIFT,
    YVEX_GGUF_ROUNDTRIP_ALLOCATION
} yvex_gguf_roundtrip_code;
typedef struct {
    yvex_gguf_roundtrip_code code;
    unsigned long long metadata_index;
    unsigned long long tensor_index;
    unsigned long long expected;
    unsigned long long actual;
    unsigned long long file_offset;
    char name[YVEX_GGUF_WRITER_NAME_CAP];
} yvex_gguf_roundtrip_failure;
typedef struct yvex_gguf_roundtrip_summary {
    unsigned long long file_bytes;
    unsigned long long bytes_hashed;
    unsigned long long prefix_bytes_verified;
    unsigned long long payload_bytes_verified;
    unsigned long long padding_bytes_verified;
    unsigned long long metadata_count;
    unsigned long long tensor_count;
    unsigned long long terminals_verified;
    unsigned long long read_calls;
    unsigned long long peak_owned_bytes;
    unsigned long long file_device;
    unsigned long long file_inode;
    long long file_mtime_seconds;
    long long file_mtime_nanoseconds;
    long long file_ctime_seconds;
    long long file_ctime_nanoseconds;
    char artifact_identity[YVEX_SHA256_HEX_CAP];
    char payload_byte_identity[YVEX_SHA256_HEX_CAP];
    int tokenizer_complete;
    int reader_accepted;
    int layout_accepted;
    int payload_accepted;
    int snapshot_stable;
    int complete;
} yvex_gguf_roundtrip_summary;
typedef void (*yvex_gguf_roundtrip_progress_fn)(
    void *context,
    const yvex_gguf_roundtrip_summary *summary,
    unsigned long long planned_file_bytes);
typedef struct {
    size_t verification_chunk_bytes;
    yvex_gguf_roundtrip_progress_fn progress;
    void *progress_context;
} yvex_gguf_roundtrip_options;
void yvex_gguf_roundtrip_options_default(
    yvex_gguf_roundtrip_options *options);
int yvex_gguf_roundtrip_validate(
    const char *path,
    const yvex_gguf_writer_plan *writer_plan,
    yvex_quant_digest_sink *digest_sink,
    const yvex_gguf_roundtrip_options *options,
    yvex_gguf_roundtrip_summary *out,
    yvex_gguf_roundtrip_failure *failure,
    yvex_error *err);
const char *yvex_gguf_roundtrip_code_name(yvex_gguf_roundtrip_code code);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_GGUF_H_INCLUDED */
