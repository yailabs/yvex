/*
 * YVEX - Source manifest internals
 *
 * File: yvex_source_manifest_internal.h
 * Layer: tool-plane implementation
 *
 * Purpose:
 *   Shares the small internal file-list representation used by the open-weight intake
 *   source scanner and JSON writer. This is not part of the public API.
 */
#ifndef YVEX_SOURCE_MANIFEST_INTERNAL_H
#define YVEX_SOURCE_MANIFEST_INTERNAL_H

#include <stddef.h>

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

#endif /* YVEX_SOURCE_MANIFEST_INTERNAL_H */
