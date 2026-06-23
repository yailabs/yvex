/*
 * YVEX - Local model registry internals
 *
 * File: src/tools/model_registry_internal.h
 * Layer: tool-plane implementation
 */
#ifndef YVEX_MODEL_REGISTRY_INTERNAL_H
#define YVEX_MODEL_REGISTRY_INTERNAL_H

#include <stddef.h>

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
    char *support_level;
    int execution_ready;
} yvex_model_registry_owned_entry;

struct yvex_model_registry {
    char *selected;
    yvex_model_registry_owned_entry *entries;
    unsigned long long count;
    unsigned long long cap;
};

char *yvex_model_registry_strdup(const char *s);
void yvex_model_registry_owned_entry_clear(yvex_model_registry_owned_entry *entry);
void yvex_model_registry_entry_view(const yvex_model_registry_owned_entry *owned,
                                    yvex_model_registry_entry *view);
int yvex_model_registry_copy_entry(yvex_model_registry_owned_entry *dst,
                                   const yvex_model_registry_entry *src,
                                   yvex_error *err);

int yvex_model_registry_parse_json_file(const char *path,
                                        yvex_model_registry *registry,
                                        yvex_error *err);

int yvex_model_registry_write_json_file(const yvex_model_registry *registry,
                                        const char *path,
                                        yvex_error *err);

int yvex_model_registry_mkdir_parent(const char *path,
                                     yvex_error *err);

void yvex_model_registry_print_entry(const yvex_model_registry_entry *entry,
                                     int selected);

void yvex_model_registry_print_scan_entry(const yvex_model_registry_entry *entry);

#endif /* YVEX_MODEL_REGISTRY_INTERNAL_H */
