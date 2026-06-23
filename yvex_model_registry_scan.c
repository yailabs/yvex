/*
 * YVEX - Local model registry scan helpers
 */
#include "yvex_model_registry_internal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int append_scan_entry(yvex_model_registry_entry **entries,
                             unsigned long long *count,
                             unsigned long long *cap,
                             const yvex_model_registry_entry *entry,
                             yvex_error *err)
{
    yvex_model_registry_owned_entry owned;
    yvex_model_registry_entry view;
    yvex_model_registry_entry *next;
    int rc;

    if (*count == *cap) {
        unsigned long long new_cap = *cap ? *cap * 2u : 8u;
        next = (yvex_model_registry_entry *)realloc(*entries, (size_t)new_cap * sizeof(*next));
        if (!next) {
            yvex_error_set(err, YVEX_ERR_NOMEM, "model_registry_scan", "scan allocation failed");
            return YVEX_ERR_NOMEM;
        }
        memset(next + *cap, 0, (size_t)(new_cap - *cap) * sizeof(*next));
        *entries = next;
        *cap = new_cap;
    }
    rc = yvex_model_registry_copy_entry(&owned, entry, err);
    if (rc != YVEX_OK) return rc;
    yvex_model_registry_entry_view(&owned, &view);
    (*entries)[*count] = view;
    (*entries)[*count].alias = owned.alias;
    (*entries)[*count].family = owned.family;
    (*entries)[*count].model = owned.model;
    (*entries)[*count].scope = owned.scope;
    (*entries)[*count].artifact_class = owned.artifact_class;
    (*entries)[*count].qprofile = owned.qprofile;
    (*entries)[*count].calibration = owned.calibration;
    (*entries)[*count].producer = owned.producer;
    (*entries)[*count].schema_version = owned.schema_version;
    (*entries)[*count].path = owned.path;
    (*entries)[*count].sha256 = owned.sha256;
    (*entries)[*count].support_level = owned.support_level;
    (*entries)[*count].execution_ready = owned.execution_ready;
    (*count)++;
    return YVEX_OK;
}

static int scan_dir(const char *dir,
                    yvex_model_registry_entry **entries,
                    unsigned long long *count,
                    unsigned long long *cap,
                    yvex_error *err)
{
    DIR *dp;
    struct dirent *de;

    dp = opendir(dir);
    if (!dp) {
        yvex_error_setf(err, YVEX_ERR_IO, "model_registry_scan", "cannot open scan root: %s", dir);
        return YVEX_ERR_IO;
    }
    while ((de = readdir(dp)) != NULL) {
        char path[4096];
        struct stat st;
        size_t len;
        yvex_model_registry_entry entry;
        int n;

        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) continue;
        n = snprintf(path, sizeof(path), "%s/%s", dir, de->d_name);
        if (n < 0 || (size_t)n >= sizeof(path)) continue;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            int rc = scan_dir(path, entries, count, cap, err);
            if (rc != YVEX_OK) {
                closedir(dp);
                return rc;
            }
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        len = strlen(path);
        if (len <= 5u || strcmp(path + len - 5u, ".gguf") != 0) continue;
        if (yvex_model_registry_entry_derive_from_path(&entry, path, err) == YVEX_OK) {
            int rc = append_scan_entry(entries, count, cap, &entry, err);
            if (rc != YVEX_OK) {
                closedir(dp);
                return rc;
            }
        }
    }
    closedir(dp);
    return YVEX_OK;
}

int yvex_model_registry_scan_root(const char *root,
                                  yvex_model_registry_entry **entries_out,
                                  unsigned long long *count_out,
                                  yvex_error *err)
{
    yvex_model_registry_entry *entries = NULL;
    unsigned long long count = 0;
    unsigned long long cap = 0;
    int rc;

    if (!root || !entries_out || !count_out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_registry_scan", "root and outputs are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *entries_out = NULL;
    *count_out = 0;
    rc = scan_dir(root, &entries, &count, &cap, err);
    if (rc != YVEX_OK) {
        yvex_model_registry_scan_free(entries, count);
        return rc;
    }
    *entries_out = entries;
    *count_out = count;
    return YVEX_OK;
}

void yvex_model_registry_scan_free(yvex_model_registry_entry *entries,
                                   unsigned long long count)
{
    unsigned long long i;

    if (!entries) return;
    for (i = 0; i < count; ++i) {
        free((char *)entries[i].alias);
        free((char *)entries[i].family);
        free((char *)entries[i].model);
        free((char *)entries[i].scope);
        free((char *)entries[i].artifact_class);
        free((char *)entries[i].qprofile);
        free((char *)entries[i].calibration);
        free((char *)entries[i].producer);
        free((char *)entries[i].schema_version);
        free((char *)entries[i].path);
        free((char *)entries[i].sha256);
        free((char *)entries[i].support_level);
    }
    free(entries);
}
