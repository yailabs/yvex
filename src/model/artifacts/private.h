/*
 * private.h - private model artifact registry storage.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   private registry storage structs shared by model artifact registry and
 *   explicit registry file writer modules.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, runtime support,
 *   artifact emission, generation, eval, benchmark, or release decisions.
 *
 * Invariants:
 *   private storage mirrors public registry entry views without exposing mutable
 *   ownership outside the model artifact registry modules.
 *
 * Boundary:
 *   model artifact registry storage is not artifact emission, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#ifndef YVEX_MODEL_ARTIFACT_PRIVATE_H
#define YVEX_MODEL_ARTIFACT_PRIVATE_H

#include <yvex/model_registry.h>

typedef struct {
    char *alias;
    char *family;
    char *model;
    char *scope;
    char *artifact_class;
    char *qprofile;
    char *calibration;
    char *producer;
    char *schema_version;
    char *path;
    char *sha256;
    unsigned long long file_size;
    char *format;
    char *architecture;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    char *primary_tensor_name;
    char *primary_tensor_role;
    char *primary_tensor_dtype;
    unsigned int primary_tensor_rank;
    char *primary_tensor_dims;
    unsigned long long primary_tensor_bytes;
    char *support_level;
    int selected_embedding_ready;
    unsigned long long selected_embedding_hidden_size;
    unsigned long long selected_embedding_vocab_size;
    unsigned long long selected_embedding_output_count;
    unsigned long long selected_embedding_slice_bytes;
    int execution_ready;
} yvex_model_registry_owned_entry;

struct yvex_model_registry {
    char *selected;
    yvex_model_registry_owned_entry *entries;
    unsigned long long count;
    unsigned long long cap;
};

void yvex_model_artifact_registry_entry_view(const yvex_model_registry_owned_entry *owned,
                                             yvex_model_registry_entry *view);
void yvex_model_artifact_registry_owned_entry_clear(yvex_model_registry_owned_entry *entry);
int yvex_model_artifact_registry_copy_entry(yvex_model_registry_owned_entry *dst,
                                            const yvex_model_registry_entry *src,
                                            yvex_error *err);

#endif
