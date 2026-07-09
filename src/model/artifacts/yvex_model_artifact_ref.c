/*
 * yvex_model_artifact_ref.c - model artifact references.
 *
 * Owner:
 *   src/model/artifacts
 *
 * Owns:
 *   alias/path model reference resolution and model ref copy/free helpers.
 *
 * Does not own:
 *   CLI parsing, command dispatch, rendering, stdout/stderr, registry storage,
 *   explicit file writing, artifact emission, runtime generation, eval,
 *   benchmark, or release decisions.
 *
 * Invariants:
 *   references preserve public model_ref API behavior and do not imply support.
 *
 * Boundary:
 *   resolving a model reference is not artifact verification, runtime support,
 *   generation readiness, benchmark evidence, or release readiness.
 */
#include "yvex_model_artifact_ref.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <yvex/model_registry.h>
#include <yvex/yvex.h>

static char *yvex_model_ref_strdup(const char *s)
{
    size_t len;
    char *out;

    if (!s) s = "";
    len = strlen(s);
    out = (char *)malloc(len + 1u);
    if (!out) return NULL;
    memcpy(out, s, len + 1u);
    return out;
}

void yvex_model_ref_clear(yvex_model_ref *ref)
{
    if (!ref) return;
    free((char *)ref->input);
    free((char *)ref->path);
    free((char *)ref->alias);
    free((char *)ref->family);
    free((char *)ref->sha256);
    free((char *)ref->support_level);
    free((char *)ref->format);
    free((char *)ref->architecture);
    free((char *)ref->primary_tensor_name);
    free((char *)ref->primary_tensor_role);
    free((char *)ref->primary_tensor_dtype);
    free((char *)ref->primary_tensor_dims);
    memset(ref, 0, sizeof(*ref));
}

static int set_path_ref(yvex_model_ref *out, const char *input, yvex_error *err)
{
    memset(out, 0, sizeof(*out));
    out->input = yvex_model_ref_strdup(input);
    out->path = yvex_model_ref_strdup(input);
    out->alias = yvex_model_ref_strdup("");
    out->family = yvex_model_ref_strdup("");
    out->sha256 = yvex_model_ref_strdup("");
    out->support_level = yvex_model_ref_strdup("");
    out->format = yvex_model_ref_strdup("");
    out->architecture = yvex_model_ref_strdup("");
    out->primary_tensor_name = yvex_model_ref_strdup("");
    out->primary_tensor_role = yvex_model_ref_strdup("");
    out->primary_tensor_dtype = yvex_model_ref_strdup("");
    out->primary_tensor_dims = yvex_model_ref_strdup("");
    out->status = YVEX_MODEL_REF_STATUS_RESOLVED;
    out->kind = YVEX_MODEL_REF_PATH;
    out->execution_ready = 0;
    if (!out->input || !out->path || !out->alias || !out->family ||
        !out->sha256 || !out->support_level || !out->format ||
        !out->architecture || !out->primary_tensor_name ||
        !out->primary_tensor_role || !out->primary_tensor_dtype ||
        !out->primary_tensor_dims) {
        yvex_model_ref_clear(out);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_ref", "path reference allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int is_path_like_reference(const char *input)
{
    size_t len;

    if (!input || !input[0]) return 0;
    if (strchr(input, '/') || strchr(input, '\\')) return 1;
    len = strlen(input);
    if (len >= 5u && strcmp(input + len - 5u, ".gguf") == 0) return 1;
    return 0;
}

static int yvex_model_ref_copy_from_entry(yvex_model_ref *out,
                                   const char *input,
                                   const yvex_model_registry_entry *entry,
                                   yvex_error *err)
{
    memset(out, 0, sizeof(*out));
    if (!input || !entry || !entry->alias || !entry->path) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", "input and registry entry are required");
        return YVEX_ERR_INVALID_ARG;
    }
    out->input = yvex_model_ref_strdup(input);
    out->path = yvex_model_ref_strdup(entry->path);
    out->alias = yvex_model_ref_strdup(entry->alias);
    out->family = yvex_model_ref_strdup(entry->family);
    out->sha256 = yvex_model_ref_strdup(entry->sha256);
    out->registered_file_size = entry->file_size;
    out->support_level = yvex_model_ref_strdup(entry->support_level);
    out->format = yvex_model_ref_strdup(entry->format);
    out->architecture = yvex_model_ref_strdup(entry->architecture);
    out->tensor_count = entry->tensor_count;
    out->known_tensor_bytes = entry->known_tensor_bytes;
    out->primary_tensor_name = yvex_model_ref_strdup(entry->primary_tensor_name);
    out->primary_tensor_role = yvex_model_ref_strdup(entry->primary_tensor_role);
    out->primary_tensor_dtype = yvex_model_ref_strdup(entry->primary_tensor_dtype);
    out->primary_tensor_rank = entry->primary_tensor_rank;
    out->primary_tensor_dims = yvex_model_ref_strdup(entry->primary_tensor_dims);
    out->primary_tensor_bytes = entry->primary_tensor_bytes;
    out->selected_embedding_ready = entry->selected_embedding_ready;
    out->selected_embedding_hidden_size = entry->selected_embedding_hidden_size;
    out->selected_embedding_vocab_size = entry->selected_embedding_vocab_size;
    out->selected_embedding_output_count = entry->selected_embedding_output_count;
    out->selected_embedding_slice_bytes = entry->selected_embedding_slice_bytes;
    out->status = YVEX_MODEL_REF_STATUS_RESOLVED;
    out->kind = YVEX_MODEL_REF_ALIAS;
    out->execution_ready = entry->execution_ready;
    if (!out->input || !out->path || !out->alias || !out->family ||
        !out->sha256 || !out->support_level || !out->format ||
        !out->architecture || !out->primary_tensor_name ||
        !out->primary_tensor_role || !out->primary_tensor_dtype ||
        !out->primary_tensor_dims) {
        yvex_model_ref_clear(out);
        yvex_error_set(err, YVEX_ERR_NOMEM, "model_ref", "alias reference allocation failed");
        return YVEX_ERR_NOMEM;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static void append_available_aliases(char *buf,
                                     size_t cap,
                                     const yvex_model_registry *registry)
{
    unsigned long long i;
    size_t used;

    if (!buf || cap == 0 || !registry) return;
    used = strlen(buf);
    for (i = 0; i < yvex_model_registry_count(registry); ++i) {
        const yvex_model_registry_entry *entry = yvex_model_registry_at(registry, i);
        int n;
        if (!entry || !entry->alias || !entry->alias[0]) continue;
        if (used + 4u >= cap) break;
        n = snprintf(buf + used, cap - used, "\n  %s", entry->alias);
        if (n < 0 || (size_t)n >= cap - used) {
            buf[cap - 1u] = '\0';
            break;
        }
        used += (size_t)n;
    }
}

int yvex_model_ref_resolve(yvex_model_ref *out,
                           const char *input,
                           const yvex_model_ref_options *options,
                           yvex_error *err)
{
    yvex_model_registry *registry = NULL;
    yvex_model_registry_options registry_options;
    const yvex_model_registry_entry *entry;
    char message[1024];
    int rc;

    if (!out || !input || !input[0]) {
        if (out) {
            memset(out, 0, sizeof(*out));
            out->status = YVEX_MODEL_REF_STATUS_INVALID;
        }
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", "model reference is required");
        return YVEX_ERR_INVALID_ARG;
    }

    memset(out, 0, sizeof(*out));
    if (access(input, F_OK) == 0) {
        return set_path_ref(out, input, err);
    }

    if (is_path_like_reference(input)) {
        return set_path_ref(out, input, err);
    }

    if (options && !options->allow_registry) {
        out->status = YVEX_MODEL_REF_STATUS_NOT_FOUND;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "model_ref",
                        "model path does not exist: %s", input);
        return YVEX_ERR_INVALID_ARG;
    }

    memset(&registry_options, 0, sizeof(registry_options));
    registry_options.registry_path = options ? options->registry_path : NULL;
    registry_options.create_if_missing = 0;
    rc = yvex_model_registry_open(&registry, &registry_options, err);
    if (rc != YVEX_OK) {
        const char *env_registry = getenv("YVEX_MODELS_REGISTRY");

        out->status = YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        if (env_registry && env_registry[0]) {
            yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                            "model registry unavailable for reference: %s; YVEX_MODELS_REGISTRY=%s; hint: register the alias in that registry, unset YVEX_MODELS_REGISTRY, or pass an existing path",
                            input, env_registry);
        } else {
            yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                            "model registry unavailable for reference: %s; hint: run './yvex models list' or pass an existing path",
                            input);
        }
        return YVEX_ERR_IO;
    }

    entry = yvex_model_registry_find(registry, input);
    if (!entry) {
        snprintf(message, sizeof(message),
                 "model reference not found: %s; hint: run './yvex models list'; available models:",
                 input);
        append_available_aliases(message, sizeof(message), registry);
        out->status = YVEX_MODEL_REF_STATUS_NOT_FOUND;
        out->kind = YVEX_MODEL_REF_UNKNOWN;
        yvex_model_registry_close(registry);
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "model_ref", message);
        return YVEX_ERR_INVALID_ARG;
    }

    if (access(entry->path, F_OK) != 0) {
        out->status = YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING;
        out->kind = YVEX_MODEL_REF_ALIAS;
        out->alias = yvex_model_ref_strdup(entry->alias);
        out->path = yvex_model_ref_strdup(entry->path);
        yvex_error_setf(err, YVEX_ERR_IO, "model_ref",
                        "model alias exists but path is missing: alias=%s path=%s; hint: update or remove the registry entry with './yvex models remove %s'",
                        entry->alias, entry->path, entry->alias);
        yvex_model_registry_close(registry);
        return YVEX_ERR_IO;
    }

    rc = yvex_model_ref_copy_from_entry(out, input, entry, err);
    yvex_model_registry_close(registry);
    return rc;
}


const char *yvex_model_ref_kind_name(yvex_model_ref_kind kind)
{
    switch (kind) {
    case YVEX_MODEL_REF_PATH:
        return "path";
    case YVEX_MODEL_REF_ALIAS:
        return "alias";
    case YVEX_MODEL_REF_UNKNOWN:
    default:
        return "unknown";
    }
}

const char *yvex_model_ref_status_name(yvex_model_ref_status status)
{
    switch (status) {
    case YVEX_MODEL_REF_STATUS_RESOLVED:
        return "resolved";
    case YVEX_MODEL_REF_STATUS_NOT_FOUND:
        return "not-found";
    case YVEX_MODEL_REF_STATUS_ALIAS_PATH_MISSING:
        return "alias-path-missing";
    case YVEX_MODEL_REF_STATUS_REGISTRY_UNAVAILABLE:
        return "registry-unavailable";
    case YVEX_MODEL_REF_STATUS_INVALID:
        return "invalid";
    case YVEX_MODEL_REF_STATUS_UNKNOWN:
    default:
        return "unknown";
    }
}
