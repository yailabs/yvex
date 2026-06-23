/*
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
int yvex_paths_print(const yvex_paths *paths, FILE *fp, yvex_error *err);

int yvex_run_id_make(char *out, unsigned long cap, yvex_error *err);
int yvex_run_dir_prepare(yvex_run_dir *out, const yvex_paths *paths, const char *run_id, yvex_error *err);
int yvex_run_dir_create(const yvex_run_dir *run, yvex_error *err);
int yvex_run_dir_print(const yvex_run_dir *run, FILE *fp, yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_FS_H */
