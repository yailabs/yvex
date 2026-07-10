/*
 * yvex_model_target_catalog.h - model-target catalog API.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   read-only model-target catalog declarations and the canonical v0.1.0
 *   release-target identity.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   the release-target ID, upstream repository, family, source directory,
 *   config identity, and local source mapping have one definition.
 *
 * Boundary:
 *   catalog visibility does not imply model support or readiness.
 */
#ifndef YVEX_MODEL_TARGET_CATALOG_H
#define YVEX_MODEL_TARGET_CATALOG_H

#include "yvex_model_target_report.h"

typedef struct {
    const char *class_id;
    const char *capability_claim;
    const char *runtime_execution;
    const char *generation;
    const char *description;
} yvex_model_target_class_record;

typedef struct {
    const char *target_id;
    const char *family;
    const char *model;
    const char *target_class;
    const char *source_artifact_class;
    const char *target_artifact_class;
    const char *pressure_purpose;
    const char *tensor_set;
    const char *local_path_class;
    const char *source_footprint_class;
    const char *runtime_boundary;
    const char *runtime_execution;
    const char *generation;
    const char *external_reference;
} yvex_model_target_record;

typedef struct {
    const char *target_id;
    const char *family_key;
    const char *family_display;
    const char *model_name;
    const char *upstream_repo_id;
    const char *source_dir_leaf;
    const char *config_model_type;
    const char *config_architecture;
} yvex_model_target_identity;

extern const char yvex_deepseek_v4_target_id[];
extern const char yvex_deepseek_v4_family_key[];
extern const char yvex_deepseek_v4_family_display[];
extern const char yvex_deepseek_v4_model_name[];
extern const char yvex_deepseek_v4_upstream_repo_id[];
extern const char yvex_deepseek_v4_source_dir_leaf[];
extern const char yvex_deepseek_v4_config_model_type[];
extern const char yvex_deepseek_v4_config_architecture[];

const yvex_model_target_identity *yvex_model_target_release_identity(void);
int yvex_model_target_is_release_target(const char *target_id);
int yvex_model_target_source_path(char *out,
                                  size_t cap,
                                  const char *models_root,
                                  const yvex_model_target_identity *identity);

const yvex_model_target_record *yvex_model_target_find(const char *target_id);
const yvex_model_target_class_record *yvex_model_target_class_find(const char *class_id);
unsigned long yvex_model_target_count(void);
const yvex_model_target_record *yvex_model_target_at(unsigned long index);
unsigned long yvex_model_target_class_count(void);
const yvex_model_target_class_record *yvex_model_target_class_at(unsigned long index);

int yvex_model_target_catalog_report_build(
    const yvex_model_target_request *request,
    yvex_model_target_report *report,
    yvex_error *err);

int yvex_model_target_catalog_help_report_build(
    yvex_model_target_report *report,
    yvex_error *err);

#endif
