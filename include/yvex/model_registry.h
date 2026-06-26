/*
 * YVEX - Local model registry API
 *
 * File: include/yvex/model_registry.h
 * Layer: public tool/support API
 *
 * Purpose:
 *   Declares the machine-local model registry used by model selection work. The
 *   registry maps short aliases to external GGUF artifact paths and metadata.
 *   It does not execute, materialize, infer, or change one-shot command model
 *   resolution yet.
 */
#ifndef YVEX_MODEL_REGISTRY_H
#define YVEX_MODEL_REGISTRY_H

#include <yvex/error.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_model_registry yvex_model_registry;

typedef struct {
    const char *alias;
    const char *family;
    const char *model;
    const char *scope;
    const char *artifact_class;
    const char *qprofile;
    const char *calibration;
    const char *producer;
    const char *schema_version;
    const char *path;
    const char *sha256;
    unsigned long long file_size;
    const char *format;
    const char *architecture;
    unsigned long long tensor_count;
    unsigned long long known_tensor_bytes;
    const char *primary_tensor_name;
    const char *primary_tensor_dtype;
    const char *primary_tensor_dims;
    unsigned long long primary_tensor_bytes;
    const char *support_level;
    int execution_ready;
} yvex_model_registry_entry;

typedef struct {
    const char *registry_path;
    int create_if_missing;
} yvex_model_registry_options;

int yvex_model_registry_open(yvex_model_registry **out,
                             const yvex_model_registry_options *options,
                             yvex_error *err);

void yvex_model_registry_close(yvex_model_registry *registry);

unsigned long long yvex_model_registry_count(const yvex_model_registry *registry);

const yvex_model_registry_entry *yvex_model_registry_at(const yvex_model_registry *registry,
                                                        unsigned long long index);

const yvex_model_registry_entry *yvex_model_registry_find(const yvex_model_registry *registry,
                                                          const char *alias);

const yvex_model_registry_entry *yvex_model_registry_selected(const yvex_model_registry *registry);

int yvex_model_registry_add(yvex_model_registry *registry,
                            const yvex_model_registry_entry *entry,
                            yvex_error *err);

int yvex_model_registry_remove(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err);

int yvex_model_registry_select(yvex_model_registry *registry,
                               const char *alias,
                               yvex_error *err);

int yvex_model_registry_save(const yvex_model_registry *registry,
                             const char *path,
                             yvex_error *err);

int yvex_model_alias_validate(const char *alias,
                              yvex_error *err);

int yvex_model_registry_entry_derive_from_path(yvex_model_registry_entry *entry,
                                               const char *path,
                                               yvex_error *err);

int yvex_model_registry_scan_root(const char *root,
                                  yvex_model_registry_entry **entries_out,
                                  unsigned long long *count_out,
                                  yvex_error *err);

void yvex_model_registry_scan_free(yvex_model_registry_entry *entries,
                                   unsigned long long count);

int yvex_model_registry_default_path(char *out,
                                     unsigned long long out_size,
                                     yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_MODEL_REGISTRY_H */
