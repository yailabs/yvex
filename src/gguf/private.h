/*
 * private.h - GGUF target-owner private boundary records.
 *
 * Owner:
 *   src/gguf
 *
 * Owns:
 *   internal status records shared by GGUF ABI, qtype geometry, writer,
 *   roundtrip, name-map, layout-map, descriptor, and report owner files.
 *
 * Does not own:
 *   CLI rendering, artifact emission, materialization, backend execution,
 *   graph execution, runtime generation, eval, benchmark, or release claims.
 *
 * Invariants:
 *   all future-owned GGUF behavior must fail closed with an explicit status,
 *   reason, and next row.
 *
 * Boundary:
 *   these records describe GGUF ownership boundaries only; they do not make
 *   writer, roundtrip, materialization, or generation capability available.
 */
#ifndef YVEX_GGUF_PRIVATE_H
#define YVEX_GGUF_PRIVATE_H

#include <stddef.h>

#include <yvex/error.h>
#include <yvex/gguf.h>
#include <yvex/gguf_layout.h>
#include <yvex/gguf_qtype.h>

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

const char *yvex_gguf_boundary_status_name(yvex_gguf_boundary_status status);
const char *yvex_gguf_abi_section_status_name(yvex_gguf_abi_section_status status);

const yvex_gguf_boundary_fact *yvex_gguf_container_boundary(void);
void yvex_gguf_container_abi_init(yvex_gguf_container_abi *abi);
int yvex_gguf_container_magic_supported(unsigned int magic, const char **reason);
int yvex_gguf_container_version_supported(unsigned int version, const char **reason);
int yvex_gguf_container_counts_supported(unsigned long long metadata_count,
                                         unsigned long long tensor_count,
                                         const char **reason);
void yvex_gguf_container_abi_from_header(const yvex_gguf_header *header,
                                         yvex_gguf_container_abi *abi);

const char *yvex_gguf_metadata_type_name(unsigned int type);
int yvex_gguf_metadata_type_supported(unsigned int type, const char **reason);
void yvex_gguf_metadata_abi_init(yvex_gguf_metadata_abi *abi);
int yvex_gguf_metadata_abi_from_gguf(const yvex_gguf *gguf,
                                     yvex_gguf_metadata_abi *abi,
                                     const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_tensor_info_boundary(void);
void yvex_gguf_tensor_info_abi_init(yvex_gguf_tensor_info_abi *abi);
int yvex_gguf_tensor_info_rank_supported(unsigned int rank, const char **reason);
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

int yvex_gguf_range_map_validate(unsigned long long offset,
                                 unsigned long long size,
                                 unsigned long long file_size,
                                 unsigned long long alignment,
                                 const char **reason);
void yvex_gguf_range_fact_init(yvex_gguf_range_fact *fact);
int yvex_gguf_range_fact_from_layout(const yvex_gguf_layout_result *layout,
                                     yvex_gguf_range_fact *fact,
                                     const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_reader_boundary(void);
int yvex_gguf_reader_parse_refusal(int parse_rc, const char **reason);
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

int yvex_gguf_name_map_role_supported(const char *role, const char **reason);
int yvex_gguf_layout_map_supported(const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_descriptor_boundary(void);
int yvex_gguf_descriptor_accepts_abi(int container_ok,
                                     int metadata_ok,
                                     int tensor_info_ok,
                                     const char **reason);
void yvex_gguf_descriptor_abi_from_sections(const yvex_gguf_container_abi *container,
                                            const yvex_gguf_metadata_abi *metadata,
                                            const yvex_gguf_tensor_info_abi *tensor_info,
                                            const yvex_gguf_qtype_abi *qtype,
                                            const yvex_gguf_range_fact *range,
                                            yvex_gguf_descriptor_abi *descriptor);

void yvex_gguf_report_fact_init(yvex_gguf_report_fact *report,
                                const char *kind,
                                const char *status,
                                const char *reason,
                                const char *next_row);
void yvex_gguf_abi_report_init(yvex_gguf_abi_report *report, const char *path);
int yvex_gguf_artifact_abi_report_build(const char *path,
                                        yvex_gguf_abi_report *report,
                                        yvex_error *err);

#endif
