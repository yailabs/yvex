/*
 * YVEX - Open-weight source manifest API
 *
 * File: include/yvex/source_manifest.h
 * Layer: public tool-plane API
 *
 * Purpose:
 *   Declares the open-weight intake source provenance contract for official open-weight
 *   intake. The API records source identity, provider-node paths, download
 *   proof paths, and local file presence without parsing tensor payloads.
 *
 * Owns:
 *   - source status vocabulary
 *   - source manifest write options
 *   - local source tree summary
 *   - JSON source manifest writer
 *   - read-only local source scanner
 *
 * Does not own:
 *   - model download execution
 *   - safetensors metadata parsing
 *   - checksumming large model files
 *   - quantization
 *   - GGUF emission
 *   - materialization
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_source_manifest
 */
#ifndef YVEX_SOURCE_MANIFEST_H
#define YVEX_SOURCE_MANIFEST_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_SOURCE_STATUS_UNKNOWN = 0,
    YVEX_SOURCE_STATUS_IN_PROGRESS,
    YVEX_SOURCE_STATUS_INCOMPLETE,
    YVEX_SOURCE_STATUS_COMPLETE,
    YVEX_SOURCE_STATUS_FAILED
} yvex_source_status;

typedef struct {
    const char *repo;
    const char *revision;
    const char *license;
    const char *model_card;
    const char *local_path;
    const char *node_name;
    const char *dry_run_log;
    const char *download_log;
    const char *pid_file;
    const char *download_command;
    yvex_source_status status;
    int include_files;
} yvex_source_manifest_options;

typedef struct {
    unsigned long long file_count;
    unsigned long long safetensors_count;
    unsigned long long total_size_bytes;
    int has_config;
    int has_tokenizer;
    int has_safetensors;
} yvex_source_manifest_summary;

const char *yvex_source_status_name(yvex_source_status status);

int yvex_source_manifest_write_json(const char *out_path,
                                    const yvex_source_manifest_options *options,
                                    yvex_source_manifest_summary *summary_out,
                                    yvex_error *err);

int yvex_source_manifest_scan_local(const char *local_path,
                                    yvex_source_manifest_summary *out,
                                    yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_SOURCE_MANIFEST_H */
