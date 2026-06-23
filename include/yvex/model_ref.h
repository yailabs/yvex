/*
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
    const char *support_level;
    int execution_ready;
} yvex_model_ref;

int yvex_model_ref_resolve(yvex_model_ref *out,
                           const char *input,
                           const yvex_model_ref_options *options,
                           yvex_error *err);

void yvex_model_ref_clear(yvex_model_ref *ref);

const char *yvex_model_ref_kind_name(yvex_model_ref_kind kind);
const char *yvex_model_ref_status_name(yvex_model_ref_status status);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MODEL_REF_H */
