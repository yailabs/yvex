/*
 * Owner: abi.fs (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Runtime filesystem
 *
 * File: include/yvex/fs.h
 * Layer: public runtime API
 *
 * Purpose:
 *   Defines the implemented runtime filesystem path-resolution and run-directory skeleton
 *   surface. This module constructs config/cache/state/data paths and run
 *   directory paths without parsing config files or writing run artifacts.
 *
 * Owns:
 *   - YVEX_PATH_CAP
 *   - YVEX_RUN_ID_CAP
 *   - yvex_paths
 *   - yvex_run_dir
 *   - filesystem path and run-directory helper functions
 *
 * Does not own:
 *   - config file parsing
 *   - artifact loading or mmap
 *   - model execution
 *   - terminal UI types
 *
 * Used by:
 *   - libyvex runtime filesystem layer
 *   - yvex CLI
 *   - tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_fs
 */
#ifndef YVEX_FS_H
#define YVEX_FS_H

#include <stdio.h>

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YVEX_PATH_CAP 4096
#define YVEX_RUN_ID_CAP 64

typedef struct {
    char config_dir[YVEX_PATH_CAP];
    char cache_dir[YVEX_PATH_CAP];
    char state_dir[YVEX_PATH_CAP];
    char data_dir[YVEX_PATH_CAP];
    char project_dir[YVEX_PATH_CAP];
} yvex_paths;

typedef struct {
    char models_root_source[32];
    char models_root[YVEX_PATH_CAP];
    char hf_root[YVEX_PATH_CAP];
    char gguf_root[YVEX_PATH_CAP];
    char reports_root[YVEX_PATH_CAP];
    char reference_root[YVEX_PATH_CAP];
    char registry_root[YVEX_PATH_CAP];
    char config_path[YVEX_PATH_CAP];
} yvex_operator_paths;

typedef struct {
    char run_id[YVEX_RUN_ID_CAP];
    char root[YVEX_PATH_CAP];
    char command_path[YVEX_PATH_CAP];
    char stdout_path[YVEX_PATH_CAP];
    char stderr_path[YVEX_PATH_CAP];
    char metrics_path[YVEX_PATH_CAP];
    char trace_path[YVEX_PATH_CAP];
    char receipt_path[YVEX_PATH_CAP];
} yvex_run_dir;

int yvex_paths_default(yvex_paths *out, yvex_error *err);
int yvex_paths_project(yvex_paths *out, const char *project_root, yvex_error *err);
int yvex_operator_paths_resolve(const yvex_paths *paths,
                                const char *explicit_models_root,
                                yvex_operator_paths *out,
                                yvex_error *err);
int yvex_operator_paths_configure(const yvex_paths *paths,
                                  const char *models_root,
                                  int create_dirs,
                                  yvex_operator_paths *out,
                                  yvex_error *err);
int yvex_operator_paths_reset(const yvex_paths *paths,
                              int *out_removed,
                              yvex_operator_paths *out,
                              yvex_error *err);
int yvex_operator_paths_create(const yvex_operator_paths *operator_paths, yvex_error *err);
int yvex_operator_paths_resolve_target(const yvex_operator_paths *operator_paths,
                                       const char *family,
                                       const char *kind,
                                       char *out,
                                       size_t cap,
                                       int *out_exists,
                                       yvex_error *err);

int yvex_run_id_make(char *out, unsigned long cap, yvex_error *err);
int yvex_run_dir_prepare(yvex_run_dir *out, const yvex_paths *paths, const char *run_id, yvex_error *err);
int yvex_run_dir_create(const yvex_run_dir *run, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_FS_H */
