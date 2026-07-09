/*
 * yvex_model_artifact_gate.h - model artifact gate implementation boundary.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   model gate and materialization gate public API backing.
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

#endif
