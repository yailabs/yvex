/* Project one live session's typed resources and committed computational state. */
#include "src/runtime/private.h"

#include <string.h>

#include <yvex/internal/core.h>

static int summary_refuse(yvex_error *err, yvex_status status,
                          const char *reason)
{
    yvex_error_set(err, status, "runtime.session.summary", reason);
    return status;
}

static int committed_state_refuse(yvex_error *err, yvex_status status,
                                  const char *reason)
{
    yvex_error_set(err, status, "runtime.session.committed-state", reason);
    return status;
}

static int committed_attention_observe(
    const yvex_attention_state_provider *provider, int provider_ready,
    int *present, unsigned long long *generation,
    unsigned long long *committed_sequence_length,
    unsigned long long *layer_count,
    char layout_identity[YVEX_SHA256_HEX_CAP],
    char capacity_identity[YVEX_SHA256_HEX_CAP],
    char content_identity[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    yvex_graph_attention_state_summary state = {0};
    int pristine;

    *present = 0;
    if (!provider_ready) return YVEX_OK;
    if (!provider || !provider->context || !provider->summary ||
        provider->summary(provider->context, &state, err) != YVEX_OK)
        return committed_state_refuse(
            err, YVEX_ERR_STATE,
            "attention state summary is unavailable");
    if (state.schema_version != YVEX_GRAPH_ATTENTION_STATE_SCHEMA_V4 ||
        !state.sealed || !state.persistent || state.cancelled ||
        state.invalidated || !state.position_consistent ||
        !state.layer_count ||
        !yvex_sha256_hex_valid(state.state_layout_identity))
        return committed_state_refuse(
            err, YVEX_ERR_STATE,
            "attention state authority is invalid or incomplete");
    pristine = !state.invalidated && !state.cancelled &&
               !state.transaction_active && !state.candidate_active &&
               !state.abort_required && !state.prepared_layer_count &&
               !state.staged_layer_count &&
               !state.committed_sequence_length && !state.next_position;
    if (pristine) return YVEX_OK;
    if (state.transaction_active ||
        state.candidate_active || state.abort_required ||
        !state.position_consistent || state.staged_layer_count ||
        state.staged_generation || state.staged_next_position ||
        state.staged_state_content_identity[0] ||
        state.staged_batch_complete || state.prefix_selected ||
        state.extension_ready ||
        state.prepared_layer_count != state.layer_count ||
        state.committed_sequence_length != state.next_position ||
        state.committed_sequence_length > state.capacity ||
        !state.generation ||
        !yvex_sha256_hex_valid(state.state_layout_identity) ||
        !yvex_sha256_hex_valid(state.state_content_identity) ||
        (state.paged &&
         (!state.paging_configured ||
          !yvex_sha256_hex_valid(state.capacity_plan_identity))))
        return committed_state_refuse(
            err, YVEX_ERR_STATE,
            "attention state is not one complete committed observation");
    *present = 1;
    *generation = state.generation;
    *committed_sequence_length = state.committed_sequence_length;
    *layer_count = state.layer_count;
    yvex_runtime_identity_copy(layout_identity, state.state_layout_identity);
    if (state.capacity_plan_identity[0])
        yvex_runtime_identity_copy(capacity_identity,
                                   state.capacity_plan_identity);
    yvex_runtime_identity_copy(content_identity, state.state_content_identity);
    return YVEX_OK;
}

static int committed_recurrent_observe(
    const yvex_sequence_state *state,
    yvex_runtime_session_committed_state_summary *out, yvex_error *err)
{
    yvex_sequence_state_summary recurrent = {0};

    if (!state) return YVEX_OK;
    if (yvex_sequence_state_summary_copy(state, &recurrent, err) != YVEX_OK ||
        recurrent.schema_version != YVEX_SEQUENCE_STATE_SCHEMA_V1 ||
        !recurrent.binding_count || recurrent.invalidated ||
        recurrent.transaction_active || recurrent.prepared ||
        recurrent.candidate_tokens || recurrent.staged_layers ||
        !recurrent.fork_supported ||
        recurrent.storage_backend != out->backend ||
        !yvex_sha256_hex_valid(recurrent.plan_identity) ||
        yvex_sequence_state_committed_identity(
            state, out->recurrent_content_identity, err) != YVEX_OK ||
        !yvex_sha256_hex_valid(out->recurrent_content_identity))
        return committed_state_refuse(
            err, YVEX_ERR_STATE,
            "recurrent state is not one complete committed observation");
    out->recurrent_present = 1;
    out->recurrent_generation = recurrent.generation;
    out->recurrent_committed_position = recurrent.committed_position;
    out->recurrent_binding_count = recurrent.binding_count;
    yvex_runtime_identity_copy(out->recurrent_plan_identity,
                               recurrent.plan_identity);
    return YVEX_OK;
}

static int committed_state_identity(
    yvex_runtime_session_committed_state_summary *summary)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];

    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(
            &hash, "yvex.runtime.session-committed-state.v1") ||
        !yvex_sha256_update_u64(&hash, summary->schema_version) ||
        !yvex_sha256_update_u64(&hash, summary->backend) ||
        !yvex_sha256_update_u64(&hash, summary->engine_generation) ||
        !yvex_sha256_update_u64(&hash, summary->session_ordinal) ||
        !yvex_sha256_update_u64(&hash, summary->active_domain_count) ||
        !yvex_sha256_update_text(&hash, summary->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, summary->runtime_binding_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->engine_specialization_identity) ||
        !yvex_sha256_update_text(&hash, summary->session_lineage_identity) ||
        !yvex_sha256_update_text(&hash, "target-attention") ||
        !yvex_sha256_update_u64(
            &hash, (unsigned long long)summary->target_attention_present) ||
        !yvex_sha256_update_u64(
            &hash, summary->target_attention_generation) ||
        !yvex_sha256_update_u64(
            &hash, summary->target_attention_committed_sequence_length) ||
        !yvex_sha256_update_u64(
            &hash, summary->target_attention_layer_count) ||
        !yvex_sha256_update_text(
            &hash, summary->target_attention_layout_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->target_attention_capacity_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->target_attention_content_identity) ||
        !yvex_sha256_update_text(&hash, "draft-attention") ||
        !yvex_sha256_update_u64(
            &hash, (unsigned long long)summary->draft_attention_present) ||
        !yvex_sha256_update_u64(
            &hash, summary->draft_attention_generation) ||
        !yvex_sha256_update_u64(
            &hash, summary->draft_attention_committed_sequence_length) ||
        !yvex_sha256_update_u64(
            &hash, summary->draft_attention_layer_count) ||
        !yvex_sha256_update_text(
            &hash, summary->draft_attention_layout_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->draft_attention_capacity_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->draft_attention_content_identity) ||
        !yvex_sha256_update_text(&hash, "recurrent") ||
        !yvex_sha256_update_u64(
            &hash, (unsigned long long)summary->recurrent_present) ||
        !yvex_sha256_update_u64(&hash, summary->recurrent_generation) ||
        !yvex_sha256_update_u64(
            &hash, summary->recurrent_committed_position) ||
        !yvex_sha256_update_u64(&hash, summary->recurrent_binding_count) ||
        !yvex_sha256_update_text(&hash, summary->recurrent_plan_identity) ||
        !yvex_sha256_update_text(&hash, summary->recurrent_content_identity) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, summary->identity);
    return 1;
}

static int attention_summary_add(
    const yvex_attention_state_provider *provider,
    yvex_runtime_session_summary *summary, yvex_error *err)
{
    yvex_graph_attention_state_summary state = {0};
    unsigned long long allocated, resident, virtual_bytes, page_table;
    int rc;
    if (!provider || !provider->context) return YVEX_OK;
    rc = provider->summary(provider->context, &state, err);
    if (rc != YVEX_OK) return rc;
    if (!yvex_core_u64_add(summary->attention_state_allocated_bytes,
                           state.allocated_bytes, &allocated) ||
        !yvex_core_u64_add(summary->attention_state_resident_bytes,
                           state.resident_bytes, &resident) ||
        !yvex_core_u64_add(summary->attention_state_virtual_bytes,
                           state.virtual_bytes, &virtual_bytes) ||
        !yvex_core_u64_add(summary->attention_state_page_table_bytes,
                           state.page_table_bytes, &page_table))
        return summary_refuse(err, YVEX_ERR_BOUNDS,
                              "attention state byte totals overflowed");
    summary->attention_state_allocated_bytes = allocated;
    summary->attention_state_resident_bytes = resident;
    summary->attention_state_virtual_bytes = virtual_bytes;
    summary->attention_state_page_table_bytes = page_table;
    return YVEX_OK;
}

static int sequence_summary_bind(
    const yvex_sequence_state *state, yvex_runtime_session_summary *summary,
    yvex_error *err)
{
    yvex_sequence_state_summary sequence = {0};
    if (!state) return YVEX_OK;
    if (yvex_sequence_state_summary_copy(state, &sequence, err) != YVEX_OK)
        return yvex_error_code(err);
    summary->sequence_state_binding_count = sequence.binding_count;
    summary->sequence_state_generation = sequence.generation;
    summary->sequence_committed_state_bytes = sequence.committed_state_bytes;
    summary->sequence_candidate_state_bytes = sequence.candidate_state_bytes;
    summary->sequence_host_state_bytes = sequence.host_state_bytes;
    summary->sequence_device_state_bytes = sequence.device_state_bytes;
    summary->sequence_recurrent_state_bytes = sequence.recurrent_state_bytes;
    summary->sequence_convolution_state_bytes = sequence.convolution_state_bytes;
    return YVEX_OK;
}

int yvex_runtime_session_summary_copy(
    const yvex_runtime_execution_session *session,
    yvex_runtime_session_summary *out, yvex_error *err)
{
    yvex_runtime_execution_session *mutable_session =
        (yvex_runtime_execution_session *)session;
    int rc;
    if (!session || !out)
        return summary_refuse(err, YVEX_ERR_INVALID_ARG,
                              "runtime session and summary output are required");
    if (!session->lifecycle_mutex_ready ||
        pthread_mutex_lock(&mutable_session->lifecycle_mutex) != 0)
        return summary_refuse(err, YVEX_ERR_STATE,
                              "runtime session synchronization is unavailable");
    *out = session->summary;
    out->attention_state_allocated_bytes = 0ull;
    out->attention_state_resident_bytes = 0ull;
    out->attention_state_virtual_bytes = 0ull;
    out->attention_state_page_table_bytes = 0ull;
    rc = attention_summary_add(
        session->attention_state_provider_ready > 0
            ? &session->attention_state_provider : NULL,
        out, err);
    if (rc == YVEX_OK)
        rc = attention_summary_add(
            session->draft_attention_state_provider_ready > 0
                ? &session->draft_attention_state_provider : NULL,
            out, err);
    if (rc == YVEX_OK)
        rc = sequence_summary_bind(session->sequence_state, out, err);
    (void)pthread_mutex_unlock(&mutable_session->lifecycle_mutex);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_runtime_session_committed_state_summary_copy(
    const yvex_runtime_execution_session *session,
    yvex_runtime_session_committed_state_summary *out, yvex_error *err)
{
    yvex_runtime_execution_session *mutable_session =
        (yvex_runtime_execution_session *)session;
    const yvex_model_engine_summary *model;
    char session_lineage_identity[YVEX_SHA256_HEX_CAP];
    int rc;

    if (out) memset(out, 0, sizeof(*out));
    if (!session || !out)
        return committed_state_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "runtime session and committed-state output are required");
    if (!session->lifecycle_mutex_ready ||
        pthread_mutex_lock(&mutable_session->lifecycle_mutex) != 0)
        return committed_state_refuse(
            err, YVEX_ERR_STATE,
            "runtime session synchronization is unavailable");
    model = session->engine ? &session->engine->summary : NULL;
    if (!session->summary.open || session->summary.busy ||
        session->summary.cancelled || session->summary.invalidated ||
        session->closing || session->execution_owner_ready ||
        !session->engine_registered || !session->engine_reserved ||
        session->engine_release_pending || session->invalidation_pending ||
        session->host_workspace_cleanup_pending ||
        !model || !model->sealed || !model->valid ||
        session->view.engine != session->engine ||
        session->view.backend != session->backend || !session->backend ||
        yvex_backend_kind_of(session->backend) != session->summary.backend ||
        !session->summary.engine_generation ||
        session->summary.engine_generation != model->engine_generation ||
        !yvex_sha256_hex_valid(model->runtime_model_identity) ||
        !yvex_sha256_hex_valid(model->runtime_binding_identity) ||
        !session->specialization ||
        session->specialization !=
            session->engine->specializations[session->summary.backend] ||
        session->specialization->summary.schema_version !=
            YVEX_ENGINE_SPECIALIZATION_SCHEMA_V1 ||
        session->specialization->summary.backend != session->summary.backend ||
        !yvex_sha256_hex_valid(
            session->summary.engine_specialization_identity) ||
        strcmp(session->summary.engine_specialization_identity,
               session->specialization->summary.identity) != 0 ||
        !session->batch_source_ordinal ||
        !yvex_sha256_hex_valid(session->batch_source_identity) ||
        !yvex_runtime_private_session_lineage_identity(
            model->runtime_model_identity, session->batch_source_ordinal,
            session_lineage_identity) ||
        strcmp(session_lineage_identity,
               session->batch_source_identity) != 0) {
        rc = committed_state_refuse(
            err, YVEX_ERR_STATE,
            "open idle session lineage is unavailable or stale");
        goto done;
    }
    out->schema_version = YVEX_RUNTIME_SESSION_COMMITTED_STATE_SCHEMA_V1;
    out->backend = session->summary.backend;
    out->engine_generation = session->summary.engine_generation;
    out->session_ordinal = session->batch_source_ordinal;
    yvex_runtime_identity_copy(out->runtime_model_identity,
                               model->runtime_model_identity);
    yvex_runtime_identity_copy(out->runtime_binding_identity,
                               model->runtime_binding_identity);
    yvex_runtime_identity_copy(out->engine_specialization_identity,
                               session->summary.engine_specialization_identity);
    yvex_runtime_identity_copy(out->session_lineage_identity,
                               session->batch_source_identity);
    rc = committed_attention_observe(
        &session->attention_state_provider,
        session->attention_state_provider_ready,
        &out->target_attention_present,
        &out->target_attention_generation,
        &out->target_attention_committed_sequence_length,
        &out->target_attention_layer_count,
        out->target_attention_layout_identity,
        out->target_attention_capacity_identity,
        out->target_attention_content_identity, err);
    if (rc == YVEX_OK)
        rc = committed_attention_observe(
            &session->draft_attention_state_provider,
            session->draft_attention_state_provider_ready,
            &out->draft_attention_present,
            &out->draft_attention_generation,
            &out->draft_attention_committed_sequence_length,
            &out->draft_attention_layer_count,
            out->draft_attention_layout_identity,
            out->draft_attention_capacity_identity,
            out->draft_attention_content_identity, err);
    if (rc == YVEX_OK)
        rc = committed_recurrent_observe(session->sequence_state, out, err);
    out->active_domain_count =
        (unsigned long long)out->target_attention_present +
        (unsigned long long)out->draft_attention_present +
        (unsigned long long)out->recurrent_present;
    if (rc == YVEX_OK &&
        ((model->attention_layer_count && !out->target_attention_present) ||
         (model->draft_attention_layer_count &&
          !out->draft_attention_present) ||
         (session->engine->binding_summary.recurrent_layer_count &&
          !out->recurrent_present)))
        rc = committed_state_refuse(
            err, YVEX_ERR_STATE,
            "required committed session-state domain is unavailable");
    if (rc == YVEX_OK && out->target_attention_present &&
        out->recurrent_present &&
        out->target_attention_committed_sequence_length !=
            out->recurrent_committed_position)
        rc = committed_state_refuse(
            err, YVEX_ERR_STATE,
            "target attention and recurrent committed extents disagree");
    if (rc == YVEX_OK && !committed_state_identity(out))
        rc = committed_state_refuse(
            err, YVEX_ERR_STATE,
            "committed session-state identity could not seal");
done:
    (void)pthread_mutex_unlock(&mutable_session->lifecycle_mutex);
    if (rc == YVEX_OK) yvex_error_clear(err);
    else memset(out, 0, sizeof(*out));
    return rc;
}
