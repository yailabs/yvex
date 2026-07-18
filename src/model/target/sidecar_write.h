/*
 * sidecar_write.h - model-target sidecar writer boundary.
 *
 * Owner: src/model/target
 * Owns: explicit local sidecar writer declarations.
 * Does not own: CLI operator streams, rendering, runtime execution, generation, benchmark, or release readiness.
 * Invariants: sidecar writers write explicit file paths only.
 * Boundary: sidecar writing does not create runtime or generation capability.
 */
#ifndef YVEX_MODEL_TARGET_SIDECAR_WRITE_H
#define YVEX_MODEL_TARGET_SIDECAR_WRITE_H

int yvex_model_target_sidecar_writer_available(void);

int yvex_model_target_write_tensor_map_sidecar(const char *path,
                                               const char *target_id,
                                               const char *family,
                                               const char *status,
                                               const char *coverage);

int yvex_model_target_write_output_head_sidecar(const char *path,
                                                const char *target_id,
                                                const char *family,
                                                const char *status);

int yvex_model_target_write_tokenizer_sidecar(const char *path,
                                              const char *target_id,
                                              const char *family,
                                              const char *status);

#endif
