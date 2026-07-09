/*
 * yvex_model_artifact_ref.h - model artifact reference implementation boundary.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   model alias/path reference resolution backing.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, artifact emission,
 *   runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   reference APIs preserve include/yvex/model_ref.h signatures.
 *
 * Boundary:
 *   reference resolution is not model support or runtime readiness.
 */
#ifndef YVEX_MODEL_ARTIFACT_REF_H
#define YVEX_MODEL_ARTIFACT_REF_H

#include <yvex/model_ref.h>

#endif
