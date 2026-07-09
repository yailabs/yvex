/*
 * yvex_gguf_private.h - GGUF target-owner private boundary records.
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

typedef enum {
    YVEX_GGUF_BOUNDARY_REPORT_ONLY = 0,
    YVEX_GGUF_BOUNDARY_UNSUPPORTED = 1,
    YVEX_GGUF_BOUNDARY_REFUSED = 2
} yvex_gguf_boundary_status;

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
    unsigned int block_size;
    unsigned int bytes_per_block;
    yvex_gguf_boundary_status status;
} yvex_gguf_qtype_geometry;

typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_gguf_report_fact;

const char *yvex_gguf_boundary_status_name(yvex_gguf_boundary_status status);

const yvex_gguf_boundary_fact *yvex_gguf_container_boundary(void);
int yvex_gguf_container_version_supported(unsigned int version, const char **reason);

const char *yvex_gguf_metadata_type_name(unsigned int type);
int yvex_gguf_metadata_type_supported(unsigned int type, const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_tensor_info_boundary(void);
int yvex_gguf_tensor_info_rank_supported(unsigned int rank, const char **reason);

const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_at(size_t index);
size_t yvex_gguf_qtype_geometry_count(void);
const yvex_gguf_qtype_geometry *yvex_gguf_qtype_geometry_find(unsigned int qtype);

int yvex_gguf_range_map_validate(unsigned long long offset,
                                 unsigned long long size,
                                 unsigned long long file_size,
                                 unsigned long long alignment,
                                 const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_reader_boundary(void);
int yvex_gguf_reader_parse_refusal(int parse_rc, const char **reason);

int yvex_gguf_writer_supported(const char **reason);
int yvex_gguf_roundtrip_supported(const char **reason);

int yvex_gguf_name_map_role_supported(const char *role, const char **reason);
int yvex_gguf_layout_map_supported(const char **reason);

const yvex_gguf_boundary_fact *yvex_gguf_descriptor_boundary(void);
int yvex_gguf_descriptor_accepts_abi(int container_ok,
                                     int metadata_ok,
                                     int tensor_info_ok,
                                     const char **reason);

void yvex_gguf_report_fact_init(yvex_gguf_report_fact *report,
                                const char *kind,
                                const char *status,
                                const char *reason,
                                const char *next_row);

#endif
