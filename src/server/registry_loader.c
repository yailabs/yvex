/* Admit registered deployment profiles into persistent engine generations.
 * Parsing, terminal output and signal handling belong to the product shell. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/server_loader.h>
#include <yvex/internal/core.h>
#include <yvex/internal/deployment_compatibility.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/runtime_capacity.h>
#include <yvex/internal/server_media.h>
#include <yvex/registry.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char name[256];
    char family[64];
    char profile_kind[32];
    char installation[PATH_MAX];
    char artifact[PATH_MAX];
    char binding[PATH_MAX];
    char target[128];
    char backend[8];
    char engine_kind[16];
    char execution_strategy[32];
    unsigned long long context_capacity;
    yvex_model_capability_summary capabilities;
} registry_profile;

typedef struct {
    const char *artifact_root;
    const char *output_root;
    const char *artifact_reopen_cache_root;
    char default_output_root[PATH_MAX];
    char default_cache_root[PATH_MAX];
} registry_media_configuration;

typedef struct yvex_server_registry_loader {
    pthread_mutex_t registry_mutex;
    yvex_server_trace_level trace_level;
} yvex_server_registry_loader;

static int profile_copy(registry_profile *profile,
                        const yvex_model_registry_entry *entry)
{
    yvex_model_capability_profile capability_profile;
    const char *profile_kind;
    const char *installation;
    const char *artifact;
    const char *binding;
    if (!profile || !entry || !entry->alias || !entry->family ||
        !entry->runtime_target || !entry->runtime_backend ||
        !entry->runtime_engine_kind || !entry->runtime_execution_strategy ||
        strlen(entry->alias) >= sizeof(profile->name) ||
        strlen(entry->family) >= sizeof(profile->family) ||
        strlen(entry->runtime_target) >= sizeof(profile->target) ||
        strlen(entry->runtime_backend) >= sizeof(profile->backend) ||
        strlen(entry->runtime_engine_kind) >= sizeof(profile->engine_kind) ||
        strlen(entry->runtime_execution_strategy) >=
            sizeof(profile->execution_strategy))
        return 0;
    profile_kind = entry->runtime_profile && entry->runtime_profile[0]
                       ? entry->runtime_profile : "single-artifact";
    installation = entry->runtime_installation ? entry->runtime_installation : "";
    artifact = entry->path ? entry->path : "";
    binding = entry->runtime_binding ? entry->runtime_binding : "";
    if (strlen(profile_kind) >= sizeof(profile->profile_kind) ||
        strlen(installation) >= sizeof(profile->installation) ||
        strlen(artifact) >= sizeof(profile->artifact) ||
        strlen(binding) >= sizeof(profile->binding))
        return 0;
    if (!(snprintf(profile->name, sizeof(profile->name), "%s", entry->alias) > 0 &&
           snprintf(profile->family, sizeof(profile->family), "%s", entry->family) >= 0 &&
           snprintf(profile->profile_kind, sizeof(profile->profile_kind), "%s",
                    profile_kind) > 0 &&
           snprintf(profile->installation, sizeof(profile->installation), "%s",
                    installation) >= 0 &&
           snprintf(profile->artifact, sizeof(profile->artifact), "%s", artifact) >= 0 &&
           snprintf(profile->binding, sizeof(profile->binding), "%s", binding) >= 0 &&
           snprintf(profile->target, sizeof(profile->target), "%s",
                    entry->runtime_target) >= 0 &&
           snprintf(profile->backend, sizeof(profile->backend), "%s",
                    entry->runtime_backend) >= 0 &&
           snprintf(profile->engine_kind, sizeof(profile->engine_kind), "%s",
                    entry->runtime_engine_kind) >= 0 &&
           snprintf(profile->execution_strategy,
                    sizeof(profile->execution_strategy), "%s",
                    entry->runtime_execution_strategy) >= 0))
        return 0;
    if (!strcmp(profile->engine_kind, "media"))
        capability_profile =
            YVEX_MODEL_CAPABILITY_PROFILE_CONDITIONED_AUDIOVISUAL_GENERATION;
    else if (!strcmp(profile->engine_kind, "text"))
        capability_profile = YVEX_MODEL_CAPABILITY_PROFILE_TEXT_GENERATION;
    else if (!strcmp(profile->engine_kind, "finite-decision"))
        capability_profile = YVEX_MODEL_CAPABILITY_PROFILE_FINITE_DECISION;
    else
        return 0;
    return yvex_model_capability_profile_describe(
               capability_profile, &profile->capabilities, NULL) == YVEX_OK;
}

static int profile_resolve(const char *name, registry_profile *profile,
                           yvex_error *err)
{
    yvex_model_registry_options options;
    yvex_model_registry *registry = NULL;
    const yvex_model_registry_entry *entry;
    yvex_deployment_compatibility compatibility = {0};
    int rc;
    memset(&options, 0, sizeof(options));
    memset(profile, 0, sizeof(*profile));
    rc = yvex_model_registry_open(&registry, &options, err);
    if (rc != YVEX_OK) return rc;
    entry = yvex_model_registry_find(registry, name);
    if (!entry) {
        yvex_error_setf(err, YVEX_ERR_STATE, "server.model-loader",
                        "profile is not registered: %s", name);
        yvex_model_registry_close(registry);
        return YVEX_ERR_STATE;
    }
    rc = yvex_deployment_compatibility_evaluate(entry, &compatibility, err);
    if (rc == YVEX_OK && !compatibility.current) {
        yvex_error_setf(err, YVEX_ERR_STATE, "server.model-loader",
                        "deployment is not current (%s): %s",
                        yvex_deployment_compatibility_status_name(
                            compatibility.status),
                        compatibility.reason);
        rc = YVEX_ERR_STATE;
    }
    if (rc != YVEX_OK) {
        yvex_model_registry_close(registry);
        return rc;
    }
    if (!profile_copy(profile, entry)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "server.model-loader",
                       "registered profile exceeds command limits");
        yvex_model_registry_close(registry);
        return YVEX_ERR_BOUNDS;
    }
    profile->context_capacity = entry->runtime_context;
    yvex_model_registry_close(registry);
    yvex_error_clear(err);
    return YVEX_OK;
}

static void engine_profile_defaults(yvex_server_engine_options *options,
                                    const registry_profile *profile)
{
    int media_requested = !strcmp(profile->engine_kind, "media");
    int finite_requested = !strcmp(profile->engine_kind, "finite-decision");

    memset(options, 0, sizeof(*options));
    options->schema_version = YVEX_SERVER_ENGINE_SCHEMA_CURRENT;
    options->alias = profile->name;
    options->artifact_path = profile->artifact;
    options->runtime_binding_path = profile->binding;
    options->target_id = profile->target;
    options->backend = media_requested || !strcmp(profile->backend, "cuda")
                           ? YVEX_BACKEND_KIND_CUDA : YVEX_BACKEND_KIND_CPU;
    options->engine_kind = media_requested ? YVEX_SERVER_ENGINE_MEDIA
                          : finite_requested ? YVEX_SERVER_ENGINE_FINITE_DECISION
                                             : YVEX_SERVER_ENGINE_TEXT;
    options->execution_strategy =
        media_requested || finite_requested ? YVEX_SERVER_EXECUTION_NOT_APPLICABLE
                        : (!strcmp(profile->execution_strategy, "speculative")
                               ? YVEX_SERVER_EXECUTION_SPECULATIVE
                               : YVEX_SERVER_EXECUTION_TARGET_ONLY);
    options->context_capacity = media_requested ? 0u : profile->context_capacity;
    options->prefill_chunk_tokens = 0u;
    options->maximum_new_tokens = 0u;
    options->maximum_output_bytes = 1048576u;
    options->maximum_sessions = 8u;
    options->concurrent_sequences = 1u;
    options->trace_level = YVEX_SERVER_TRACE_STAGES;
    options->capabilities = profile->capabilities;
}

static int finite_workspace_budget(yvex_server_engine_options *options,
                                   yvex_error *err)
{
    unsigned long long total, available, reserve;
    int process_limited;
    if (!yvex_runtime_private_memory_capacity(&total, &available, &process_limited)) {
        yvex_error_set(err, YVEX_ERR_STATE, "server.model-loader",
                       "live finite workspace capacity is unavailable");
        return YVEX_ERR_STATE;
    }
    reserve = yvex_runtime_private_system_reserve(total);
    if (available <= reserve) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "server.model-loader",
                       "finite workspace has no capacity after system reserve");
        return YVEX_ERR_NOMEM;
    }
    /* Physical-stage allocation remains bounded; this observation neither
     * reserves memory nor promises throughput. No device allocation is admitted. */
    options->maximum_host_bytes = available - reserve;
    options->maximum_device_bytes = 0ull;
    return YVEX_OK;
}

static int media_configuration_defaults(
    const registry_profile *profile, registry_media_configuration *configuration,
    yvex_error *err)
{
    yvex_paths paths;
    char admission_path[YVEX_PATH_CAP];
    int admission_written, written, rc;
    if (!configuration->artifact_root)
        configuration->artifact_root = profile->installation;
    rc = yvex_paths_default(&paths, err);
    if (rc == YVEX_OK) {
        yvex_core_text_copy(configuration->default_cache_root,
                            sizeof(configuration->default_cache_root), paths.cache_dir);
        configuration->artifact_reopen_cache_root = configuration->default_cache_root;
    }
    if (rc != YVEX_OK || configuration->output_root) return rc;
    written = rc == YVEX_OK
                  ? snprintf(configuration->default_output_root,
                             sizeof(configuration->default_output_root), "%s/media",
                             paths.data_dir)
                  : -1;
    if (rc != YVEX_OK) return rc;
    admission_written = written >= 0 &&
                                (size_t)written < sizeof(configuration->default_output_root)
                            ? snprintf(admission_path, sizeof(admission_path),
                                       "%s/.publication",
                                       configuration->default_output_root)
                            : -1;
    if (written < 0 || (size_t)written >= sizeof(configuration->default_output_root) ||
        admission_written < 0 || (size_t)admission_written >= sizeof(admission_path)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "server.media-output",
                       "default media output path exceeds capacity");
        return YVEX_ERR_BOUNDS;
    }
    rc = yvex_core_mkdir_parent(admission_path, "server.media-output", err);
    if (rc == YVEX_OK)
        configuration->output_root = configuration->default_output_root;
    return rc;
}

static int media_prepare(const registry_profile *profile,
                         const registry_media_configuration *configuration,
                         yvex_runtime_media_host_profile *host,
                         yvex_server_media_options *media,
                         yvex_server_engine_options *options, yvex_error *err)
{
    const yvex_component_variant_adapter *adapter;
    yvex_media_target_profile target = {0};
    int rc;

    if (!configuration->artifact_root || !configuration->artifact_root[0] ||
        !configuration->output_root || !configuration->output_root[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "server.media-profile",
                       "media mode requires a composite installation and publication root");
        return YVEX_ERR_INVALID_ARG;
    }
    adapter = yvex_graph_component_variant_find(profile->target);
    if (!adapter || !adapter->media_target_profile || !adapter->media_execution) {
        yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "server.media-profile",
                        "target has no conversational media adapter: %s", profile->target);
        return YVEX_ERR_UNSUPPORTED;
    }
    rc = adapter->media_target_profile(&target, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_media_host_profile_build(
            host, &target, adapter->media_execution, configuration->artifact_root,
            configuration->output_root, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_media_execution_preset_build(
            host, &media->execution_preset, err);
    if (rc != YVEX_OK) return rc;
    if (options->backend != host->request_template.component_backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "server.media-profile",
                       "media mode requires the admitted CUDA backend");
        return YVEX_ERR_INVALID_ARG;
    }
    options->artifact_path = NULL;
    options->runtime_binding_path = NULL;
    options->target_id = host->target;
    media->schema_version = YVEX_SERVER_MEDIA_SCHEMA_V2;
    media->output_root = host->output_root;
    media->artifact_reopen_cache_root = configuration->artifact_reopen_cache_root;
    media->request_template = host->request_template;
    media->profiles = host->profiles;
    media->profile_count = host->profile_count;
    media->frames_per_chunk = host->frames_per_chunk;
    media->frame_remainder = host->frame_remainder;
    media->minimum_frames = host->minimum_frames;
    media->maximum_frames = host->maximum_frames;
    media->minimum_inference_steps = host->minimum_inference_steps;
    media->maximum_inference_steps = host->maximum_inference_steps;
    media->released_sigma_grid_points = host->released_sigma_grid_points;
    media->default_seed = host->default_seed;
    media->canvas_multiple = host->canvas_multiple;
    media->canvas_short_edge = host->canvas_short_edge;
    media->minimum_canvas_pixels = host->minimum_canvas_pixels;
    media->maximum_canvas_pixels = host->maximum_canvas_pixels;
    media->released_width = host->released_width;
    media->released_height = host->released_height;
    media->minimum_duration_milliseconds = host->minimum_duration_milliseconds;
    media->maximum_duration_milliseconds = host->maximum_duration_milliseconds;
    media->minimum_aspect_numerator = host->minimum_aspect_numerator;
    media->minimum_aspect_denominator = host->minimum_aspect_denominator;
    media->maximum_aspect_numerator = host->maximum_aspect_numerator;
    media->maximum_aspect_denominator = host->maximum_aspect_denominator;
    return YVEX_OK;
}

int yvex_server_registry_model_load(void *opaque, yvex_server *server,
                                 const char *alias,
                                 unsigned long long requested_context_capacity,
                                 yvex_error *err)
{
    yvex_server_registry_loader *context = opaque;
    if (!context || !server || !alias || !alias[0]) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "server.model-loader",
                       "loader, server and profile alias are required");
        return YVEX_ERR_INVALID_ARG;
    }
    registry_profile profile;
    registry_media_configuration media_configuration;
    yvex_runtime_media_host_profile media_host = {0};
    yvex_server_media_options media = {0};
    yvex_server_engine_options selected;
    yvex_server_engine_summary summary;
    int media_requested = 0, rc;
    if (pthread_mutex_lock(&context->registry_mutex) != 0) {
        yvex_error_set(err, YVEX_ERR_STATE, "server.model-loader",
                       "model registry lock failed");
        return YVEX_ERR_STATE;
    }
    rc = profile_resolve(alias, &profile, err);
    (void)pthread_mutex_unlock(&context->registry_mutex);
    if (rc != YVEX_OK) return rc;
    media_requested = !strcmp(profile.engine_kind, "media");
    engine_profile_defaults(&selected, &profile);
    if (requested_context_capacity) {
        if (media_requested) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "server.model-loader",
                           "token context override is unavailable for media engines");
            return YVEX_ERR_UNSUPPORTED;
        }
        selected.context_capacity = requested_context_capacity;
    }
    selected.maximum_new_tokens = selected.engine_kind == YVEX_SERVER_ENGINE_TEXT
                                      ? selected.context_capacity : 0ull;
    selected.trace_level = context->trace_level;
    memset(&media_configuration, 0, sizeof(media_configuration));
    if (media_requested) {
        rc = media_configuration_defaults(
            &profile, &media_configuration, err);
        if (rc == YVEX_OK)
            rc = media_prepare(&profile, &media_configuration, &media_host,
                               &media, &selected, err);
    } else if (selected.engine_kind == YVEX_SERVER_ENGINE_FINITE_DECISION) {
        rc = finite_workspace_budget(&selected, err);
    } else {
        rc = YVEX_OK;
    }
    if (rc != YVEX_OK) return rc;
    return media_requested
               ? yvex_server_media_engine_load(
                     server, &selected, &media, &summary, err)
               : yvex_server_engine_load(server, &selected, &summary, err);
}


int yvex_server_registry_loader_create(yvex_server_registry_loader **out,
                                       yvex_server_trace_level trace_level,
                                       yvex_error *err)
{
    yvex_server_registry_loader *loader;
    if (!out || *out || trace_level < YVEX_SERVER_TRACE_SUMMARY ||
        trace_level > YVEX_SERVER_TRACE_FULL) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "server.model-loader",
                       "empty output and admitted trace level are required");
        return YVEX_ERR_INVALID_ARG;
    }
    loader = calloc(1u, sizeof(*loader));
    if (!loader) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "server.model-loader",
                       "registry coordinator allocation failed");
        return YVEX_ERR_NOMEM;
    }
    if (pthread_mutex_init(&loader->registry_mutex, NULL) != 0) {
        free(loader);
        yvex_error_set(err, YVEX_ERR_STATE, "server.model-loader",
                       "registry coordinator creation failed");
        return YVEX_ERR_STATE;
    }
    loader->trace_level = trace_level;
    *out = loader;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_server_registry_loader_close(yvex_server_registry_loader **loader)
{
    if (!loader || !*loader) return;
    (void)pthread_mutex_destroy(&(*loader)->registry_mutex);
    free(*loader);
    *loader = NULL;
}
