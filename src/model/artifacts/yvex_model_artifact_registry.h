/*
 * yvex_model_artifact_registry.h - model artifact registry implementation boundary.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   private declarations for registry storage and public registry API backing.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, file writing,
 *   artifact emission, runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   registry APIs preserve include/yvex/model_registry.h signatures.
 *
 * Boundary:
 *   model registry ownership is not artifact emission or runtime support.
 */
#ifndef YVEX_MODEL_ARTIFACT_REGISTRY_H
#define YVEX_MODEL_ARTIFACT_REGISTRY_H

#include <yvex/model_registry.h>

#endif
