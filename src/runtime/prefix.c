/* Share exact committed attention or recurrent state across isolated sessions. */
#include "src/runtime/private.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/internal/runtime_prefix.h>

struct yvex_runtime_session_prefix {
    unsigned int schema_version;
    yvex_attention_state_prefix *target, *draft;
    yvex_sequence_state *sequence;
    yvex_attention_state_recipe *target_recipes, *draft_recipes;
    unsigned long long target_recipe_count, draft_recipe_count;
    yvex_execution_capacity_plan target_capacity, draft_capacity;
    char runtime_model_identity[YVEX_SHA256_HEX_CAP];
    yvex_runtime_session_prefix_summary summary;
};

static int prefix_refuse(yvex_model_engine_failure *failure,
                         yvex_status status, const char *reason,
                         yvex_error *err)
{
    yvex_runtime_private_failure_record(
        failure, YVEX_MODEL_ENGINE_FAILURE_GRAPH, "session-prefix", 1ull,
        0ull, reason);
    yvex_error_set(err, status, "runtime.session-prefix", reason);
    return status;
}

static unsigned long long prefix_sequence_bytes(
    const yvex_sequence_state_summary *summary)
{
    if (summary->host_authoritative) return summary->host_state_bytes;
    if (summary->device_authoritative) return summary->device_state_bytes;
    return 0ull;
}

static int prefix_identity(
    const yvex_runtime_session_prefix *prefix,
    yvex_runtime_session_prefix_summary *summary)
{
    yvex_attention_state_prefix_summary target = {0}, draft = {0};
    yvex_sequence_state_summary sequence = {0};
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_error err;
    unsigned long long shared = 0ull, mapped = 0ull, references = 0ull;
    unsigned long long committed = 0ull, layer, scopes = 0ull;
    unsigned long long sequence_bytes = 0ull;
    char sequence_identity[YVEX_SHA256_HEX_CAP] = {0};

    if (!prefix || prefix->schema_version !=
                       YVEX_RUNTIME_SESSION_PREFIX_SCHEMA_CURRENT ||
        (!prefix->target && !prefix->sequence) ||
        !yvex_sha256_hex_valid(prefix->runtime_model_identity))
        return 0;
    if (prefix->target) {
        if (yvex_attention_state_prefix_summary_copy(prefix->target, &target,
                                                     &err) != YVEX_OK ||
            prefix->target_recipe_count != target.layer_count ||
            (target.layer_count && !prefix->target_recipes) ||
            strcmp(target.capacity_plan_identity,
                   prefix->target_capacity.identity) != 0)
            return 0;
        shared = target.shared_bytes;
        mapped = target.mapped_bytes;
        references = target.reference_count;
        committed = target.committed_sequence_length;
        scopes++;
    }
    if (prefix->draft) {
        if (yvex_attention_state_prefix_summary_copy(prefix->draft, &draft,
                                                     &err) != YVEX_OK ||
            !prefix->target || committed != draft.committed_sequence_length ||
            prefix->draft_recipe_count != draft.layer_count ||
            (draft.layer_count && !prefix->draft_recipes) ||
            strcmp(draft.capacity_plan_identity,
                   prefix->draft_capacity.identity) != 0 ||
            !yvex_core_u64_add(shared, draft.shared_bytes, &shared) ||
            !yvex_core_u64_add(mapped, draft.mapped_bytes, &mapped))
            return 0;
        if (draft.reference_count < references)
            references = draft.reference_count;
        scopes++;
    }
    if (prefix->sequence) {
        if (yvex_sequence_state_summary_copy(
                prefix->sequence, &sequence, &err) != YVEX_OK ||
            yvex_sequence_state_committed_identity(
                prefix->sequence, sequence_identity, &err) != YVEX_OK ||
            (scopes && committed != sequence.committed_position) ||
            !(sequence_bytes = prefix_sequence_bytes(&sequence)) ||
            !yvex_core_u64_add(shared, sequence_bytes, &shared))
            return 0;
        committed = sequence.committed_position;
        scopes++;
        if (!references) references = 1ull;
    }
    memset(summary, 0, sizeof(*summary));
    summary->schema_version = YVEX_RUNTIME_SESSION_PREFIX_SCHEMA_CURRENT;
    summary->scope_count = scopes;
    summary->committed_sequence_length = committed;
    summary->shared_bytes = shared;
    summary->mapped_bytes = mapped;
    summary->reference_count = references;
    yvex_runtime_identity_copy(summary->runtime_model_identity,
                               prefix->runtime_model_identity);
    yvex_runtime_identity_copy(summary->target_prefix_identity,
                               target.prefix_identity);
    if (prefix->draft)
        yvex_runtime_identity_copy(summary->draft_prefix_identity,
                                   draft.prefix_identity);
    if (prefix->sequence) {
        summary->sequence_state_bytes = sequence_bytes;
        summary->sequence_state_binding_count = sequence.binding_count;
        summary->sequence_state_generation = sequence.generation;
        yvex_runtime_identity_copy(summary->sequence_plan_identity,
                                   sequence.plan_identity);
        yvex_runtime_identity_copy(summary->sequence_prefix_identity,
                                   sequence_identity);
    }
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash,
                                 "yvex.runtime.session-prefix.v2") ||
        !yvex_sha256_update_u64(&hash, summary->schema_version) ||
        !yvex_sha256_update_u64(&hash, summary->scope_count) ||
        !yvex_sha256_update_u64(
            &hash, summary->committed_sequence_length) ||
        !yvex_sha256_update_u64(&hash, summary->shared_bytes) ||
        !yvex_sha256_update_u64(&hash, summary->mapped_bytes) ||
        !yvex_sha256_update_u64(&hash, summary->sequence_state_bytes) ||
        !yvex_sha256_update_u64(
            &hash, summary->sequence_state_binding_count) ||
        !yvex_sha256_update_u64(
            &hash, summary->sequence_state_generation) ||
        !yvex_sha256_update_text(&hash, summary->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, summary->target_prefix_identity) ||
        !yvex_sha256_update_text(&hash, summary->draft_prefix_identity) ||
        !yvex_sha256_update_text(&hash, summary->sequence_plan_identity) ||
        !yvex_sha256_update_text(&hash, summary->sequence_prefix_identity))
        return 0;
    for (layer = 0ull; layer < prefix->target_recipe_count; ++layer)
        if (!yvex_sha256_hex_valid(prefix->target_recipes[layer].identity) ||
            !yvex_sha256_update_text(
                &hash, prefix->target_recipes[layer].identity))
            return 0;
    for (layer = 0ull; layer < prefix->draft_recipe_count; ++layer)
        if (!yvex_sha256_hex_valid(prefix->draft_recipes[layer].identity) ||
            !yvex_sha256_update_text(
                &hash, prefix->draft_recipes[layer].identity))
            return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, summary->prefix_identity);
    return 1;
}

static int prefix_recipes_capture(
    yvex_attention_state_provider *provider, unsigned long long layer_count,
    yvex_attention_state_recipe **recipes, yvex_error *err)
{
    unsigned long long layer;
    if (recipes) *recipes = NULL;
    if (!provider || !provider->recipe || !recipes || !layer_count ||
        layer_count > SIZE_MAX / sizeof(**recipes))
        return prefix_refuse(NULL, YVEX_ERR_STATE,
                             "source prefix recipes are unavailable", err);
    *recipes = calloc((size_t)layer_count, sizeof(**recipes));
    if (!*recipes)
        return prefix_refuse(NULL, YVEX_ERR_NOMEM,
                             "source prefix recipe allocation failed", err);
    for (layer = 0ull; layer < layer_count; ++layer) {
        const yvex_attention_state_recipe *recipe =
            provider->recipe(provider->context, layer);
        if (!recipe || !yvex_sha256_hex_valid(recipe->identity)) {
            free(*recipes);
            *recipes = NULL;
            return prefix_refuse(NULL, YVEX_ERR_STATE,
                                 "source prefix recipe is invalid", err);
        }
        (*recipes)[layer] = *recipe;
    }
    return YVEX_OK;
}

static int prefix_provider_capture(
    yvex_attention_state_provider *provider, unsigned long long maximum_bytes,
    yvex_execution_capacity_plan *capacity,
    yvex_attention_state_prefix **prefix,
    yvex_attention_state_recipe **recipes,
    unsigned long long *recipe_count,
    yvex_attention_state_prefix_summary *summary, yvex_error *err)
{
    const yvex_execution_capacity_plan *source;
    yvex_attention_failure attention_failure = {0};
    int rc;

    if (!provider || !provider->context || !provider->capacity ||
        !provider->prefix_capture || !capacity || !prefix || !recipes ||
        !recipe_count || !summary ||
        !(source = provider->capacity(provider->context)) ||
        yvex_execution_capacity_plan_validate(source, err) != YVEX_OK)
        return prefix_refuse(NULL, YVEX_ERR_STATE,
                             "paged provider capacity is unavailable", err);
    *capacity = *source;
    rc = provider->prefix_capture(provider->context, maximum_bytes, prefix,
                                  &attention_failure, err);
    if (rc == YVEX_OK)
        rc = yvex_attention_state_prefix_summary_copy(*prefix, summary, err);
    if (rc == YVEX_OK)
        rc = prefix_recipes_capture(provider, summary->layer_count, recipes,
                                    err);
    if (rc == YVEX_OK) *recipe_count = summary->layer_count;
    if (rc == YVEX_OK &&
        strcmp(summary->capacity_plan_identity, capacity->identity) != 0)
        rc = prefix_refuse(NULL, YVEX_ERR_FORMAT,
                           "captured prefix capacity identity changed", err);
    return rc;
}

int yvex_runtime_session_prefix_capture(
    yvex_runtime_execution_session *source,
    unsigned long long maximum_shared_bytes,
    yvex_runtime_session_prefix **out,
    yvex_runtime_session_prefix_summary *summary,
    yvex_model_engine_failure *failure, yvex_error *err)
{
    yvex_runtime_session_prefix *prefix = NULL;
    yvex_attention_state_prefix_summary target = {0}, draft = {0};
    yvex_model_engine_summary model = {0};
    unsigned long long remaining;
    yvex_sequence_state_summary sequence = {0};
    int rc = YVEX_OK, draft_pristine = 0;

    if (out) *out = NULL;
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!source || !out || !summary || !maximum_shared_bytes ||
        !source->lifecycle_mutex_ready ||
        pthread_mutex_lock(&source->lifecycle_mutex) != 0)
        return prefix_refuse(failure, YVEX_ERR_INVALID_ARG,
                             "idle source session and byte budget are required",
                             err);
    if (!source->summary.open || source->summary.busy || source->closing ||
        source->summary.invalidated ||
        (!source->attention_state_provider_ready && !source->sequence_state)) {
        rc = prefix_refuse(failure, YVEX_ERR_STATE,
                           "source session cannot publish a prefix", err);
        goto done;
    }
    prefix = calloc(1u, sizeof(*prefix));
    if (!prefix) {
        rc = prefix_refuse(failure, YVEX_ERR_NOMEM,
                           "session prefix allocation failed", err);
        goto done;
    }
    prefix->schema_version = YVEX_RUNTIME_SESSION_PREFIX_SCHEMA_CURRENT;
    rc = yvex_model_engine_summary_copy(source->engine, &model, err);
    if (rc != YVEX_OK || !yvex_sha256_hex_valid(model.runtime_model_identity)) {
        rc = prefix_refuse(failure, YVEX_ERR_STATE,
                           "source runtime model identity is unavailable", err);
        goto done;
    }
    yvex_runtime_identity_copy(prefix->runtime_model_identity,
                               model.runtime_model_identity);
    remaining = maximum_shared_bytes;
    if (source->attention_state_provider_ready) {
        rc = prefix_provider_capture(
            &source->attention_state_provider, remaining,
            &prefix->target_capacity, &prefix->target,
            &prefix->target_recipes, &prefix->target_recipe_count,
            &target, err);
        if (rc != YVEX_OK) goto done;
        if (target.shared_bytes > remaining) {
            rc = prefix_refuse(failure, YVEX_ERR_BOUNDS,
                               "target prefix exceeded the shared byte budget",
                               err);
            goto done;
        }
        remaining -= target.shared_bytes;
    }
    if (source->draft_attention_state_provider_ready) {
        rc = yvex_runtime_private_attention_state_pristine(
            &source->draft_attention_state_provider, &draft_pristine, err);
        if (rc != YVEX_OK) goto done;
        if (!draft_pristine) {
            if (!remaining) {
                rc = prefix_refuse(
                    failure, YVEX_ERR_BOUNDS,
                    "target prefix exhausted the shared byte budget", err);
                goto done;
            }
            rc = prefix_provider_capture(
                &source->draft_attention_state_provider, remaining,
                &prefix->draft_capacity, &prefix->draft,
                &prefix->draft_recipes, &prefix->draft_recipe_count,
                &draft, err);
            if (rc != YVEX_OK) goto done;
            if (!prefix->target || target.committed_sequence_length !=
                                       draft.committed_sequence_length) {
                rc = prefix_refuse(failure, YVEX_ERR_STATE,
                                   "target and draft prefixes diverged", err);
                goto done;
            }
            if (draft.shared_bytes > remaining) {
                rc = prefix_refuse(
                    failure, YVEX_ERR_BOUNDS,
                    "draft prefix exceeded the shared byte budget", err);
                goto done;
            }
            remaining -= draft.shared_bytes;
        }
    }
    if (source->sequence_state) {
        unsigned long long sequence_bytes;
        rc = yvex_sequence_state_summary_copy(
            source->sequence_state, &sequence, err);
        if (rc != YVEX_OK) {
            rc = prefix_refuse(
                failure, (yvex_status)rc,
                "recurrent prefix summary is unavailable", err);
            goto done;
        }
        sequence_bytes = prefix_sequence_bytes(&sequence);
        if (!sequence.fork_supported || !sequence_bytes) {
            rc = prefix_refuse(
                failure, YVEX_ERR_UNSUPPORTED,
                "recurrent prefix storage is not forkable", err);
            goto done;
        }
        if (sequence_bytes > remaining) {
            rc = prefix_refuse(
                failure, YVEX_ERR_BOUNDS,
                "recurrent prefix exceeded the shared byte budget", err);
            goto done;
        }
        rc = yvex_sequence_state_fork(
            &prefix->sequence, source->sequence_state, err);
        if (rc != YVEX_OK) goto done;
    }
    if (!prefix_identity(prefix, &prefix->summary) ||
        prefix->summary.shared_bytes > maximum_shared_bytes) {
        rc = prefix_refuse(failure, YVEX_ERR_FORMAT,
                           "session prefix identity could not seal", err);
        goto done;
    }
    *summary = prefix->summary;
    *out = prefix;
    prefix = NULL;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
done:
    (void)pthread_mutex_unlock(&source->lifecycle_mutex);
    yvex_runtime_session_prefix_close(&prefix);
    if (prefix) *out = prefix; /* Retain failed rollback ownership for retry. */
    return rc;
}

static int prefix_provider_attach(
    yvex_attention_state_provider *provider,
    const yvex_execution_capacity_plan *capacity,
    const yvex_attention_state_prefix *prefix,
    const yvex_attention_state_recipe *recipes,
    unsigned long long recipe_count, yvex_error *err)
{
    yvex_attention_failure attention_failure = {0};
    yvex_attention_state_prefix_summary prefix_summary = {0};
    yvex_graph_attention_state_summary state_summary = {0};
    unsigned long long layer;
    int rc;

    if (!provider || !provider->context || !provider->configure_pages ||
        !provider->prepare || !provider->summary ||
        !provider->prefix_attach || !capacity ||
        !prefix || !recipes || !recipe_count ||
        yvex_attention_state_prefix_summary_copy(
            prefix, &prefix_summary, err) != YVEX_OK ||
        recipe_count != prefix_summary.layer_count)
        return prefix_refuse(NULL, YVEX_ERR_STATE,
                             "destination prefix provider is unavailable", err);
    rc = provider->configure_pages(provider->context, capacity,
                                   &attention_failure, err);
    for (layer = 0ull; rc == YVEX_OK && layer < recipe_count; ++layer)
        rc = provider->prepare(provider->context, layer, &recipes[layer],
                               NULL, &attention_failure, err);
    if (rc == YVEX_OK)
        rc = provider->summary(provider->context, &state_summary, err);
    if (rc == YVEX_OK &&
        state_summary.prepared_layer_count != prefix_summary.layer_count)
        rc = prefix_refuse(NULL, YVEX_ERR_FORMAT,
                           "destination prefix layer coverage is incompatible",
                           err);
    if (rc == YVEX_OK &&
        state_summary.committed_sequence_length != recipes[0].initial_position)
        rc = prefix_refuse(NULL, YVEX_ERR_FORMAT,
                           "destination prefix preparation is not pristine",
                           err);
    if (rc == YVEX_OK &&
        strcmp(state_summary.state_layout_identity,
               prefix_summary.state_layout_identity) != 0)
        rc = prefix_refuse(NULL, YVEX_ERR_FORMAT,
                           "destination prefix state layout is incompatible",
                           err);
    if (rc == YVEX_OK &&
        strcmp(state_summary.capacity_plan_identity,
               prefix_summary.capacity_plan_identity) != 0)
        rc = prefix_refuse(NULL, YVEX_ERR_FORMAT,
                           "destination prefix capacity is incompatible", err);
    if (rc == YVEX_OK)
        rc = provider->prefix_attach(provider->context, prefix,
                                     &attention_failure, err);
    return rc;
}

static int prefix_capacity_rebuild(
    const yvex_attention_plan *attention,
    const yvex_attention_state_recipe *recipes,
    unsigned long long recipe_count,
    yvex_graph_attention_capacity_plan **out, yvex_error *err)
{
    yvex_graph_attention_capacity_request request = {0};
    const yvex_graph_attention_capacity_summary *summary;
    unsigned long long layer;
    int rc;

    if (out) *out = NULL;
    if (!attention || !recipes || !recipe_count || !out ||
        recipes[0].final_position <= recipes[0].initial_position)
        return prefix_refuse(
            NULL, YVEX_ERR_FORMAT,
            "prefix attention capacity is incomplete", err);
    request.scope = YVEX_ATTENTION_PROBE_SCOPE_FULL;
    request.history_tokens = recipes[0].initial_position;
    request.start_position = recipes[0].initial_position;
    request.token_count = recipes[0].final_position -
                          recipes[0].initial_position;
    request.execution_count = 1ull;
    request.use_requested_position = 1;
    rc = yvex_graph_attention_capacity_plan_build(
        out, attention, &request, err);
    summary = rc == YVEX_OK
                  ? yvex_graph_attention_capacity_plan_summary(*out)
                  : NULL;
    if (rc == YVEX_OK &&
        (!summary || summary->layer_count != recipe_count ||
         summary->selected_layer_count != recipe_count))
        rc = prefix_refuse(
            NULL, YVEX_ERR_FORMAT,
            "prefix attention capacity coverage changed", err);
    for (layer = 0ull; rc == YVEX_OK && layer < recipe_count; ++layer) {
        const yvex_graph_attention_capacity_layer *capacity =
            yvex_graph_attention_capacity_plan_layer(*out, layer);
        if (!capacity || !capacity->selected ||
            strcmp(capacity->recipe.identity, recipes[layer].identity) != 0)
            rc = prefix_refuse(
                NULL, YVEX_ERR_FORMAT,
                "prefix attention capacity recipe changed", err);
    }
    if (rc != YVEX_OK)
        yvex_graph_attention_capacity_plan_close(out);
    return rc;
}

int yvex_runtime_session_prefix_attach(
    yvex_runtime_execution_session *destination,
    const yvex_runtime_session_prefix *prefix,
    yvex_runtime_session_prefix_summary *summary,
    yvex_model_engine_failure *failure, yvex_error *err)
{
    yvex_model_engine_summary model = {0};
    const yvex_model_engine_view *model_view;
    yvex_graph_attention_capacity_plan *target_capacity = NULL;
    yvex_graph_attention_capacity_plan *draft_capacity = NULL;
    yvex_runtime_session_prefix_summary current = {0};
    yvex_sequence_state_summary destination_sequence = {0};
    int rc, draft_pristine = 0;

    if (summary) memset(summary, 0, sizeof(*summary));
    if (!destination || !prefix || !summary ||
        !destination->lifecycle_mutex_ready ||
        pthread_mutex_lock(&destination->lifecycle_mutex) != 0)
        return prefix_refuse(failure, YVEX_ERR_INVALID_ARG,
                             "empty destination session is required", err);
    rc = prefix_identity(prefix, &current) ? YVEX_OK : YVEX_ERR_FORMAT;
    if (rc == YVEX_OK)
        rc = yvex_model_engine_summary_copy(destination->engine, &model, err);
    model_view = rc == YVEX_OK
                     ? yvex_model_engine_view_get(destination->engine)
                     : NULL;
    if (rc == YVEX_OK && !prefix->draft &&
        destination->draft_attention_state_provider_ready)
        rc = yvex_runtime_private_attention_state_pristine(
            &destination->draft_attention_state_provider,
            &draft_pristine, err);
    if (rc == YVEX_OK && destination->sequence_state)
        rc = yvex_sequence_state_summary_copy(
            destination->sequence_state, &destination_sequence, err);
    if (rc != YVEX_OK || !destination->summary.open ||
        destination->summary.busy || destination->closing ||
        destination->summary.invalidated ||
        destination->state_residency ||
        destination->draft_state_residency ||
        (!!prefix->target != !!destination->attention_state_provider_ready) ||
        (!!prefix->sequence != !!destination->sequence_state) ||
        (prefix->sequence &&
         (destination_sequence.committed_position ||
          destination_sequence.generation ||
          strcmp(destination_sequence.plan_identity,
                 current.sequence_plan_identity) != 0)) ||
        (prefix->draft &&
         !destination->draft_attention_state_provider_ready) ||
        (!prefix->draft &&
         destination->draft_attention_state_provider_ready &&
         !draft_pristine) ||
        strcmp(current.runtime_model_identity,
               model.runtime_model_identity) != 0) {
        rc = prefix_refuse(failure, YVEX_ERR_STATE,
                           "prefix and destination session are incompatible",
                           err);
        goto done;
    }
    if (prefix->target)
        rc = prefix_capacity_rebuild(
            model_view ? model_view->attention : NULL,
            prefix->target_recipes, prefix->target_recipe_count,
            &target_capacity, err);
    if (rc == YVEX_OK && prefix->draft)
        rc = prefix_capacity_rebuild(
            model_view ? model_view->draft_attention : NULL,
            prefix->draft_recipes, prefix->draft_recipe_count,
            &draft_capacity, err);
    rc = prefix->target
             && rc == YVEX_OK
             ? prefix_provider_attach(
                   &destination->attention_state_provider,
                   &prefix->target_capacity, prefix->target,
                   prefix->target_recipes, prefix->target_recipe_count, err)
             : rc;
    if (rc == YVEX_OK && target_capacity)
        rc = yvex_runtime_private_session_prepare_persistent_scope_state_locked(
            destination, YVEX_TENSOR_SCOPE_GLOBAL, target_capacity,
            failure, err);
    if (rc == YVEX_OK && prefix->draft)
        rc = prefix_provider_attach(
            &destination->draft_attention_state_provider,
            &prefix->draft_capacity, prefix->draft,
            prefix->draft_recipes, prefix->draft_recipe_count, err);
    if (rc == YVEX_OK && draft_capacity)
        rc = yvex_runtime_private_session_prepare_persistent_scope_state_locked(
            destination, YVEX_TENSOR_SCOPE_DRAFT, draft_capacity,
            failure, err);
    if (rc == YVEX_OK && prefix->sequence)
        rc = yvex_sequence_state_restore(
            destination->sequence_state, prefix->sequence, err);
    if (rc != YVEX_OK) {
        yvex_error cleanup;
        destination->summary.invalidated = 1;
        (void)yvex_runtime_private_session_invalidate(destination, 1,
                                                      &cleanup);
        yvex_runtime_private_failure_record(
            failure, YVEX_MODEL_ENGINE_FAILURE_GRAPH, "session-prefix",
            1ull, 0ull, "prefix attachment failed atomically");
        goto done;
    }
    if (!prefix_identity(prefix, summary)) {
        rc = prefix_refuse(failure, YVEX_ERR_FORMAT,
                           "attached prefix identity changed", err);
        goto done;
    }
    if (destination->sequence_state) {
        rc = yvex_sequence_state_summary_copy(
            destination->sequence_state, &destination_sequence, err);
        if (rc != YVEX_OK) goto done;
        yvex_runtime_private_session_sequence_summary_bind(
            &destination->summary, &destination_sequence);
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
done:
    yvex_graph_attention_capacity_plan_close(&draft_capacity);
    yvex_graph_attention_capacity_plan_close(&target_capacity);
    (void)pthread_mutex_unlock(&destination->lifecycle_mutex);
    return rc;
}

void yvex_runtime_session_prefix_close(yvex_runtime_session_prefix **owner)
{
    yvex_runtime_session_prefix *prefix = owner ? *owner : NULL;
    if (!prefix) return;
    if (yvex_sequence_state_close_checked(&prefix->sequence, NULL) != YVEX_OK)
        return;
    *owner = NULL;
    yvex_attention_state_prefix_close(&prefix->draft);
    yvex_attention_state_prefix_close(&prefix->target);
    free(prefix->draft_recipes);
    free(prefix->target_recipes);
    memset(prefix, 0, sizeof(*prefix));
    free(prefix);
}
