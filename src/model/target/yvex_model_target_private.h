/*
 * yvex_model_target_private.h - private model-target helpers.
 *
 * Owner:
 *   src/model/target
 *
 * Owns:
 *   small private declarations shared by model-target report modules.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr writing, runtime
 *   execution, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   shared declarations expose typed report facts only; specialized modules
 *   own their own report construction.
 *
 * Boundary:
 *   private helpers support report-only facts and do not create runtime,
 *   generation, benchmark, or release capability.
 */
#ifndef YVEX_MODEL_TARGET_PRIVATE_H
#define YVEX_MODEL_TARGET_PRIVATE_H

#include "yvex_model_target_catalog.h"

const char *yvex_model_target_family_key(const char *target_id);
const char *yvex_model_target_family_display(const char *target_id);
int yvex_model_target_supported_source_target(const char *target_id);
void yvex_model_target_report_common_tail(yvex_model_target_report *report);
void yvex_model_target_report_add_output_contract(yvex_model_target_report *report,
                                                  const char *report_name,
                                                  const char *mode);

#endif
