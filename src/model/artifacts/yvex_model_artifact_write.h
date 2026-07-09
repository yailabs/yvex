/*
 * yvex_model_artifact_write.h - explicit model artifact registry file writer.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   explicit local registry JSON file serialization.
 *
 * Does not own:
 *   operator streams, CLI rendering, command parsing, registry mutation, artifact
 *   emission, runtime generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   writer functions write only explicit local files supplied by callers.
 *
 * Boundary:
 *   registry file serialization is not artifact emission or model support.
 */
#ifndef YVEX_MODEL_ARTIFACT_WRITE_H
#define YVEX_MODEL_ARTIFACT_WRITE_H

#include <yvex/model_registry.h>

int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                        const char *path,
                                        yvex_error *err);

#endif
