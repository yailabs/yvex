/*
 * yvex_model_artifacts_surface.h - model-artifacts surface declarations.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   declarations for the historical command-family surface entrypoints that
 *   are implemented in family-owned CLI-only files.
 *
 * Does not own:
 *   domain registry storage, model reference ownership, model gate algorithms,
 *   renderer contracts, stdout/stderr writer implementation, artifact
 *   emission, runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   exported functions preserve existing CLI syntax and behavior only.
 *
 * Boundary:
 *   these declarations preserve router compatibility; implementation
 *   ownership is per command family and never libyvex.a.
 */
#ifndef YVEX_MODEL_ARTIFACTS_SURFACE_H
#define YVEX_MODEL_ARTIFACTS_SURFACE_H

#include <stdio.h>

int yvex_model_artifacts_surface_models_command(int arg_count, char **args);
void yvex_model_artifacts_surface_models_help(FILE *fp);

int yvex_model_artifacts_surface_fullmodel_command(int arg_count, char **args);
void yvex_model_artifacts_surface_fullmodel_help(FILE *fp);

int yvex_model_artifacts_surface_attention_command(int arg_count, char **args);
void yvex_model_artifacts_surface_attention_help(FILE *fp);

int yvex_model_artifacts_surface_context_command(int arg_count, char **args);
void yvex_model_artifacts_surface_context_help(FILE *fp);

int yvex_model_artifacts_surface_moe_command(int arg_count, char **args);
void yvex_model_artifacts_surface_moe_help(FILE *fp);

int yvex_model_artifacts_surface_tensor_collection_command(int arg_count, char **args);
void yvex_model_artifacts_surface_tensor_collection_help(FILE *fp);

#endif
