/*
 * prepare.h - models prepare/check CLI surface.
 * Owner: src/cli/model_artifacts
 * Owns: models prepare/check surface declarations and shared CLI probe helper.
 * Does not own: registry storage, model gate algorithms, materialization
 * algorithms, runtime generation, or artifact emission.
 * Invariants: CLI-only and excluded from libyvex.a.
 * Boundary: prepare/check output does not promote generation readiness.
 */
#ifndef YVEX_MODELS_PREPARE_SURFACE_H
#define YVEX_MODELS_PREPARE_SURFACE_H

#include "download.h"

int yvex_models_prepare_surface_command(int arg_count, char **args);
int yvex_models_check_surface_command(int arg_count, char **args);
void prepare_probe_map_sidecar_status(const char *tensor_map_path,
                                      const char *output_head_map_path,
                                      int *tensor_map_incomplete,
                                      int *output_head_map_missing);

#endif
