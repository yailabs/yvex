/*
 * yvex_model_artifact_gate.h - model artifact gate implementation boundary.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   model gate and materialization gate public API backing, plus the internal
 *   complete-artifact admission projection.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, artifact emission,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   gate APIs preserve include/yvex/model_gate.h and materialize_gate.h
 *   signatures.
 *
 * Boundary:
 *   gate facts are evidence only and do not prove runtime generation.
 */
#ifndef YVEX_MODEL_ARTIFACT_GATE_H
#define YVEX_MODEL_ARTIFACT_GATE_H

#include <yvex/model_gate.h>
#include <yvex/materialize_gate.h>

#include "src/artifact/yvex_artifact_roundtrip_gate.h"

typedef struct {
    yvex_model_gate_status status;
    yvex_model_support_level support_level;
    const char *artifact_identity;
    const char *artifact_path;
    const char *profile_name;
    unsigned long long tensor_count;
    unsigned long long file_bytes;
    int complete_artifact_admitted;
    int materialization_input_ready;
    int execution_ready;
} yvex_model_complete_artifact_gate_fact;

int yvex_model_artifact_gate_from_admission(
    const yvex_complete_artifact_admission *admission,
    yvex_model_complete_artifact_gate_fact *fact,
    yvex_error *err);

#endif
