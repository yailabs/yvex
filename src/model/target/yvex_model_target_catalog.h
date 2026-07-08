/*
 * yvex_model_target_catalog.h - model-target catalog API.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   read-only model-target catalog declarations.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, runtime execution, generation,
 *   eval, benchmark, or release decisions.
 *
 * Invariants:
 *   catalog records are report-only target pressure facts.
 *
 * Boundary:
 *   catalog visibility does not imply model support or readiness.
 */
#ifndef YVEX_MODEL_TARGET_CATALOG_H
#define YVEX_MODEL_TARGET_CATALOG_H

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

const yvex_model_target_record *yvex_model_target_find(const char *target_id);
const yvex_model_target_class_record *yvex_model_target_class_find(const char *class_id);
unsigned long yvex_model_target_count(void);
const yvex_model_target_record *yvex_model_target_at(unsigned long index);
unsigned long yvex_model_target_class_count(void);
const yvex_model_target_class_record *yvex_model_target_class_at(unsigned long index);

#endif
