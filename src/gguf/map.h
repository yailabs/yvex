/*
 * map.h - private logical GGUF name and shape mapping contract.
 *
 * Owner: src/gguf.
 * Owns: canonical emitted names, name provenance, and logical-shape admission.
 * Does not own: family source semantics, payload transforms, physical offsets,
 *   writer emission, materialization, or runtime support.
 * Invariants: standard names match the pinned DeepSeek mapping reference;
 *   extension names are versioned and collision-free.
 * Boundary: an admitted logical name and shape are writer inputs, not bytes.
 */
#ifndef YVEX_GGUF_MAP_H
#define YVEX_GGUF_MAP_H

#include <stddef.h>

#include <yvex/tensor.h>

#define YVEX_GGUF_MAPPING_REFERENCE_COMMIT \
    "e920c523e3b8a0163fe498af5bf90df35ff51d25"
#define YVEX_GGUF_MTP_EXTENSION_VERSION 1u
#define YVEX_GGUF_NO_FORCED_QTYPE (~0u)

typedef enum {
    YVEX_GGUF_NAME_PINNED_STANDARD = 0,
    YVEX_GGUF_NAME_SEMANTIC_STANDARD,
    YVEX_GGUF_NAME_YVEX_EXTENSION
} yvex_gguf_name_provenance;

int yvex_gguf_name_map_resolve(yvex_tensor_role role,
                               int mtp_extension,
                               unsigned long long layer_index,
                               unsigned long long predictor_index,
                               char *out,
                               size_t out_cap,
                               yvex_gguf_name_provenance *provenance,
                               const char **reason);

int yvex_gguf_layout_map_shape_supported(yvex_tensor_role role,
                                         unsigned int qtype,
                                         unsigned int rank,
                                         const unsigned long long *dims,
                                         const char **reason);

const char *yvex_gguf_name_provenance_name(
    yvex_gguf_name_provenance provenance);

#endif
