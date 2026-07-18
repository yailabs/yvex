/*
 * Owner: abi.model_ref (abi).
 * Owns: the public-abi boundary consumed by repository.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=public match config/source_owners.tsv.
 * Boundary: public-abi; moving this contract requires an ownership-manifest change.
 *
 * YVEX - Model reference resolver API
 *
 * File: include/yvex/model_ref.h
 * Layer: public tool/support API
 *
 * Purpose:
 *   Resolves a CLI model reference into a concrete GGUF path. A reference may
 *   be an existing filesystem path or a local model registry alias.
 */
#ifndef YVEX_MODEL_REF_H
#define YVEX_MODEL_REF_H

#include <yvex/error.h>
#include <yvex/model_registry.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YVEX_MODEL_REF_UNKNOWN = 0,
    YVEX_MODEL_REF_PATH,
    YVEX_MODEL_REF_ALIAS
} yvex_model_ref_kind;

typedef enum {
    YVEX_MODEL_REF_STATUS_UNKNOWN = 0,
    YVEX_MODEL_REF_STATUS_RESOLVED,
    YVEX_MODEL_REF_STATUS_NOT_FOUND,
    YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING,
    YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE,
    YVEX_MODEL_REF_STATUS_INVALID
} yvex_model_ref_status;

typedef struct {
    const char *registry_path;
    int allow_registry;
} yvex_model_ref_options;

typedef struct {
    yvex_model_ref_status status;
    yvex_model_ref_kind kind;
    const char *input;
    const char *path;
    const char *alias;
    const char *family;
    const char *sha256;
    unsigned long long registered_file_size;
    const char *support_level;
    const char *format;
    const char *architecture;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    const char *primary_tensor_name;
    const char *primary_tensor_role;
    const char *primary_tensor_dtype;
    unsigned int primary_tensor_rank;
    const char *primary_tensor_dims;
    unsigned long long primary_tensor_bytes;
    int selected_embedding_ready;
    unsigned long long selected_embedding_hidden_size;
    unsigned long long selected_embedding_vocab_size;
    unsigned long long selected_embedding_output_count;
    unsigned long long selected_embedding_slice_bytes;
    int execution_ready;
} yvex_model_ref;

typedef struct {
    yvex_model_registry_entry entry;
    char format[16];
    char architecture[64];
    char primary_tensor_name[128];
    char primary_tensor_role[64];
    char primary_tensor_dtype[32];
    char primary_tensor_dims[128];
    char support_level[64];
} yvex_model_metadata_snapshot;

typedef struct {
    int required;
    int passed;
    char digest_status[16];
    char identity_status[16];
    char metadata_status[32];
    char readiness_status[32];
    char reason[256];
    char current_sha256[65];
    unsigned long long registered_file_size;
    unsigned long long current_file_size;
    yvex_model_metadata_drift_report metadata_drift;
} yvex_model_ref_identity_result;

int yvex_model_ref_resolve(yvex_model_ref *out,
                           const char *input,
                           const yvex_model_ref_options *options,
                           yvex_error *err);

void yvex_model_ref_clear(yvex_model_ref *ref);

const char *yvex_model_ref_kind_name(yvex_model_ref_kind kind);
const char *yvex_model_ref_status_name(yvex_model_ref_status status);

int yvex_model_metadata_snapshot_read(yvex_model_metadata_snapshot *out,
                                      const char *path_or_alias,
                                      yvex_error *err);
void yvex_model_ref_registry_entry_view(const yvex_model_ref *ref,
                                        yvex_model_registry_entry *out);
int yvex_model_ref_identity_validate(const yvex_model_ref *ref,
                                     yvex_model_ref_identity_result *out,
                                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MODEL_REF_H */
