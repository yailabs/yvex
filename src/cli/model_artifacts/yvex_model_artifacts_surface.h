/*
 * yvex_model_artifacts_surface.h - transitional model-artifacts CLI surfaces.
 *
 * Owner:
 *   src/cli/model_artifacts
 *
 * Owns:
 *   declarations for historical models/fullmodel/attention/context/moe and
 *   tensor-collection CLI surfaces while they are split away from the public
 *   command adapter.
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
 *   transitional CLI surfaces are not domain ownership and do not enter
 *   libyvex.a.
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
