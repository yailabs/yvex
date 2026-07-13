/*
 * yvex_source_private.h - private source cell shared structs.
 *
 * Owner: src/source.
 * Owns: source-cell private manifest and native inventory structures.
 * Does not own: CLI parsing, operator rendering, runtime, generation, eval, or benchmark.
 * Invariants: private structs stay inside the source cell and preserve header-only source boundaries.
 * Boundary: private source facts are not source verification or runtime readiness.
 */
#ifndef YVEX_SOURCE_PRIVATE_H
#define YVEX_SOURCE_PRIVATE_H

#include <stddef.h>
#include <yvex/error.h>
#include <yvex/native_weights.h>
#include <yvex/source_manifest.h>

typedef struct {
    char *path;
    unsigned long long size_bytes;
    const char *kind;
} yvex_source_manifest_file;

typedef struct {
    yvex_source_manifest_file *items;
    size_t count;
    size_t cap;
    yvex_source_manifest_summary summary;
} yvex_source_manifest_file_list;

struct yvex_native_weight_table {
    yvex_native_weight_info *items;
    unsigned long long count;
    unsigned long long cap;
    yvex_native_weight_summary summary;
    unsigned long long header_read_count;
    unsigned long long header_error_count;
    unsigned long long header_bytes;
    unsigned long long *name_slots;
    size_t name_slot_count;
    unsigned long long lookup_count;
    unsigned long long collision_count;
    unsigned long long maximum_probe;
    int finalized;
};

void yvex_source_manifest_file_list_init(yvex_source_manifest_file_list *list);
void yvex_source_manifest_file_list_free(yvex_source_manifest_file_list *list);

int yvex_source_manifest_scan_files(const char *local_path,
                                    int include_files,
                                    yvex_source_manifest_file_list *out,
                                    yvex_error *err);

int yvex_source_manifest_write_json_file(const char *out_path,
                                         const yvex_source_manifest_options *options,
                                         const yvex_source_manifest_file_list *files,
                                         yvex_error *err);

int yvex_native_weight_table_add(yvex_native_weight_table *table,
                                 const char *name,
                                 const char *shard_path,
                                 const char *dtype_name,
                                 unsigned int rank,
                                 const unsigned long long *dims,
                                 unsigned long long data_start,
                                 unsigned long long data_end,
                                 yvex_error *err);

int yvex_native_weight_table_finalize(yvex_native_weight_table *table,
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

#endif
