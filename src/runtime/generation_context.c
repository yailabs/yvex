/*
 * A generation context admits one model/session pair and owns the finite composition of lower
 * transformer, logits, sampling, tokenizer, and speculative resources. Construction publishes
 * nothing until every lower plan and lifecycle primitive is ready; close drains an admitted turn
 * before releasing those resources in reverse dependency order.
 */
#include "src/runtime/private.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <build_commit.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/core.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/moe.h>
#include <yvex/internal/runtime_state_store.h>

static int generation_context_refuse(yvex_error *err, yvex_status status,
                                     const char *reason)
{
    yvex_error_set(err, status, "runtime.generation", reason);
    return status;
}

int yvex_runtime_private_generation_cancelled(
    const yvex_runtime_generation_context *context, yvex_error *err)
{
    if (context->options.cancel_requested &&
        context->options.cancel_requested(context->options.cancel_context))
        return generation_context_refuse(
            err, YVEX_ERR_CANCELLED, "generation was cancelled");
    return YVEX_OK;
}

int yvex_runtime_private_generation_enter(yvex_runtime_generation_context *context, yvex_error *err)
{
    unsigned int expected = 0u;
    if (context && atomic_compare_exchange_strong_explicit(&context->lifecycle, &expected,
                       YVEX_GENERATION_LIFECYCLE_ACTIVE, memory_order_acq_rel, memory_order_acquire))
        return YVEX_OK;
    if (context) (void)atomic_fetch_add_explicit(&context->admission_failures, 1ull, memory_order_relaxed);
    return generation_context_refuse(err, YVEX_ERR_STATE,
        expected & YVEX_GENERATION_LIFECYCLE_CLOSING ? "generation context is closing"
                                                    : "generation context is already in use");
}
/* Release exclusive admission and wake the close owner after accounting; context stays owned. */
void yvex_runtime_private_generation_leave(yvex_runtime_generation_context *context, int rc, int executed)
{
    if (!context) return;
    if (executed) context->execution_count++;
    if (executed && rc != YVEX_OK) {
        context->failure_count++;
        if (rc == YVEX_ERR_CANCELLED) context->cancellation_count++;
    }
    /* Serialize the predicate change with close's check-and-wait. Testing
     * CLOSING before taking this lock loses the wakeup when close races leave. */
    if (context->drain_mutex_ready &&
        pthread_mutex_lock(&context->drain_mutex) == 0) {
        (void)atomic_fetch_and_explicit(
            &context->lifecycle, ~YVEX_GENERATION_LIFECYCLE_ACTIVE, memory_order_release);
        if (context->drain_condition_ready) (void)pthread_cond_broadcast(&context->drain_condition);
        (void)pthread_mutex_unlock(&context->drain_mutex);
        return;
    }
    (void)atomic_fetch_and_explicit(&context->lifecycle, ~YVEX_GENERATION_LIFECYCLE_ACTIVE, memory_order_release);
}

static int generation_options_valid(const yvex_runtime_generation_options *options)
{
    return options &&
           options->schema_version == YVEX_RUNTIME_GENERATION_SCHEMA_V6 &&
           (options->backend == YVEX_BACKEND_KIND_CPU ||
            options->backend == YVEX_BACKEND_KIND_CUDA) &&
           options->mode <= YVEX_GENERATION_MODE_SPECULATIVE &&
           options->workload_kind <= YVEX_EXECUTION_WORKLOAD_FULL_MODEL_RESEARCH &&
           options->context_capacity && options->prefill_chunk_tokens &&
           options->maximum_new_tokens && options->maximum_output_bytes &&
           options->trace_policy <= YVEX_RUNTIME_TRACE_FULL &&
           options->evidence_profile <= YVEX_EXECUTION_EVIDENCE_FORENSIC &&
           options->concurrent_sequences < 64ull &&
           options->runnable_sequences <= 64ull &&
           (!options->runnable_sequences ||
            options->runnable_sequences >= options->concurrent_sequences) &&
           (options->compatible_operation_batching == 0 ||
            options->compatible_operation_batching == 1) &&
           (!options->compatible_operation_batching ||
            options->concurrent_sequences > 1ull);
}

static int generation_device_stochastic(
    const yvex_runtime_generation_context *context,
    const yvex_backend *backend)
{
    return context && context->options.backend == YVEX_BACKEND_KIND_CUDA &&
           context->options.evidence_profile == YVEX_EXECUTION_EVIDENCE_PRODUCTION &&
           context->options.sampling_policy.strategy ==
               YVEX_SAMPLING_STRATEGY_STOCHASTIC &&
           yvex_backend_sampling_operations_get(backend) != NULL;
}

static int generation_device_selection(
    const yvex_runtime_generation_context *context,
    const yvex_backend *backend)
{
    return context && context->options.backend == YVEX_BACKEND_KIND_CUDA &&
           context->options.evidence_profile == YVEX_EXECUTION_EVIDENCE_PRODUCTION &&
           (context->options.sampling_policy.strategy ==
                YVEX_SAMPLING_STRATEGY_GREEDY ||
            generation_device_stochastic(context, backend));
}

static int generation_scheduler_maximum_width(
    const yvex_runtime_generation_context *context,
    unsigned long long *width, yvex_error *err)
{
    int rc;
    if (width) *width = 0ull;
    if (!context || !width)
        return generation_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "compatible execution width owner is unavailable");
    rc = yvex_model_engine_scheduler_maximum_width_copy(
        context->model, width, err);
    if (rc != YVEX_OK) return rc;
    if (*width > context->options.concurrent_sequences)
        *width = context->options.concurrent_sequences;
    if (*width < 2ull || *width >= 64ull)
        return generation_context_refuse(
            err, YVEX_ERR_UNSUPPORTED,
            "engine specialization does not admit compatible scheduling");
    yvex_error_clear(err);
    return YVEX_OK;
}


static yvex_runtime_capacity_options generation_capacity_options(
    const yvex_runtime_generation_options *options)
{
    yvex_runtime_capacity_options result = {
        .backend = options->backend,
        .mode = options->mode == YVEX_GENERATION_MODE_SPECULATIVE
                    ? YVEX_EXECUTION_GENERATION_SPECULATIVE
                    : YVEX_EXECUTION_GENERATION_TARGET_ONLY,
        .workload_kind = options->workload_kind,
        .evidence_profile = options->evidence_profile,
        .sampling_requirement =
            options->sampling_policy.strategy == YVEX_SAMPLING_STRATEGY_STOCHASTIC
                ? YVEX_EXECUTION_SAMPLING_STOCHASTIC : YVEX_EXECUTION_SAMPLING_GREEDY,
        .context_capacity = options->context_capacity,
        .prefill_chunk_tokens = options->prefill_chunk_tokens,
        .concurrent_sequences = options->concurrent_sequences,
        .maximum_host_bytes = options->maximum_host_bytes,
        .maximum_device_bytes = options->maximum_device_bytes,
        .compatible_operation_batching = options->compatible_operation_batching,
    };
    return result;
}

static int generation_capacity_build(
    yvex_runtime_generation_context *context,
    yvex_graph_attention_capacity_plan **workspace_capacity, yvex_error *err)
{
    yvex_runtime_capacity_options options = generation_capacity_options(&context->options);
    return yvex_runtime_capacity_derive(context->model, context->session,
        &options, &context->capacity, workspace_capacity, err);
}

int yvex_runtime_private_generation_capacity_preflight(
    const yvex_runtime_binding *binding, yvex_backend *backend,
    const yvex_runtime_generation_options *options,
    unsigned long long *required_bytes, unsigned long long *available_bytes,
    yvex_error *err)
{
    yvex_runtime_capacity_options capacity;
    if (!generation_options_valid(options))
        return generation_context_refuse(err, YVEX_ERR_INVALID_ARG,
            "complete startup generation options are required");
    capacity = generation_capacity_options(options);
    return yvex_runtime_capacity_preflight(binding, backend, &capacity,
        required_bytes, available_bytes, err);
}

static int generation_stops_open(yvex_runtime_generation_context *context,
                                 unsigned long long vocabulary_size,
                                 yvex_error *err)
{
    unsigned long long index;
    if (!context->options.additional_stop_token_count) return YVEX_OK;
    if (!context->options.additional_stop_token_ids ||
        context->options.additional_stop_token_count > SIZE_MAX / sizeof(unsigned int))
        return generation_context_refuse(
            err, YVEX_ERR_BOUNDS,
            "additional stop-token extent is invalid");
    context->additional_stops = yvex_core_calloc(
        (size_t)context->options.additional_stop_token_count,
        sizeof(*context->additional_stops));
    if (!context->additional_stops)
        return generation_context_refuse(
            err, YVEX_ERR_NOMEM,
            "additional stop-token allocation failed");
    for (index = 0ull; index < context->options.additional_stop_token_count; ++index) {
        unsigned int token = context->options.additional_stop_token_ids[index];
        unsigned long long scan;
        if (token >= vocabulary_size)
            return generation_context_refuse(
                err, YVEX_ERR_BOUNDS,
                "additional stop token is outside vocabulary");
        for (scan = 0ull; scan < index; ++scan)
            if (context->additional_stops[scan] == token)
                return generation_context_refuse(
                    err, YVEX_ERR_FORMAT,
                    "additional stop tokens contain a duplicate");
        context->additional_stops[index] = token;
    }
    context->options.additional_stop_token_ids = context->additional_stops;
    return YVEX_OK;
}

static int generation_execution_profile_build(
    yvex_runtime_generation_context *context, yvex_error *err)
{
    yvex_runtime_execution_profile_derivation derivation = {0};
    derivation.schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1;
    derivation.model = context->model;
    derivation.session = context->session;
    derivation.workload = &context->capacity.workload_profile;
    derivation.backend = context->options.backend;
    derivation.generation_mode =
        context->options.mode == YVEX_GENERATION_MODE_SPECULATIVE
            ? YVEX_EXECUTION_GENERATION_SPECULATIVE
            : YVEX_EXECUTION_GENERATION_TARGET_ONLY;
    derivation.evidence = context->options.evidence_profile;
    derivation.sampling_requirement =
        context->options.sampling_policy.strategy == YVEX_SAMPLING_STRATEGY_GREEDY
            ? YVEX_EXECUTION_SAMPLING_GREEDY
            : YVEX_EXECUTION_SAMPLING_STOCHASTIC;
    return yvex_runtime_execution_profile_derive(
        &derivation, &context->execution_profile, err);
}

static int generation_plan_build(yvex_runtime_generation_context *context,
                                 yvex_error *err)
{
    yvex_model_engine_summary model;
    const yvex_transformer_plan_summary *transformer;
    const yvex_program_token_interface *decoder;
    const yvex_runtime_logits_plan_summary *logits;
    const yvex_tokenizer_plan_summary *tokenizer;
    const char *producer_identity;
    unsigned long long producer_vocabulary;
    yvex_execution_plan_kind producer_kind;
    yvex_runtime_generation_plan_summary plan = {0};
    if (yvex_model_engine_summary_copy(context->model, &model, err) != YVEX_OK)
        return yvex_error_code(err);
    transformer = yvex_transformer_plan_summary_get(
        yvex_runtime_transformer_context_plan(context->transformer));
    decoder = yvex_runtime_decoder_execution_interface(context->decoder_execution);
    logits = yvex_runtime_logits_plan_summary_get(context->logits);
    tokenizer = yvex_tokenizer_plan_summary_get(context->tokenizer);
    if ((transformer == NULL) == (decoder == NULL) || !logits || !tokenizer ||
        !tokenizer->sealed || !tokenizer->runtime_bound)
        return generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation lower-owner plans are unavailable");
    producer_kind = transformer ? YVEX_EXECUTION_PLAN_TRANSFORMER
                                : YVEX_EXECUTION_PLAN_DECODER;
    producer_identity = transformer ? transformer->transformer_plan_identity
                                    : logits->decoder_plan_identity;
    producer_vocabulary = transformer ? transformer->vocabulary_size
                                      : decoder->vocabulary_size;
    if (
        tokenizer->vocabulary_size > producer_vocabulary ||
        producer_vocabulary != logits->vocabulary_size ||
        logits->producer_kind != producer_kind ||
        strcmp(producer_identity,
               producer_kind == YVEX_EXECUTION_PLAN_TRANSFORMER
                   ? logits->transformer_plan_identity
                   : logits->decoder_plan_identity) != 0)
        return generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation lower-owner plans are incompatible");
    plan.schema_version = YVEX_RUNTIME_GENERATION_PLAN_SCHEMA_CURRENT;
    plan.producer_kind = producer_kind;
    plan.backend = context->options.backend;
    plan.mode = context->options.mode;
    plan.context_capacity = context->options.context_capacity;
    plan.prefill_chunk_tokens = context->options.prefill_chunk_tokens;
    plan.maximum_new_tokens = context->options.maximum_new_tokens;
    plan.maximum_output_bytes = context->options.maximum_output_bytes;
    plan.trace_policy = (unsigned int)context->options.trace_policy;
    plan.evidence_profile = context->execution_profile.evidence;
    plan.execution_class = context->execution_profile.execution_class;
    yvex_runtime_identity_copy(plan.runtime_model_identity,
                               model.runtime_model_identity);
    yvex_runtime_identity_copy(plan.runtime_binding_identity,
                               context->model_view->binding->identity);
    yvex_runtime_identity_copy(plan.runtime_descriptor_identity,
                               model.runtime_descriptor_identity);
    yvex_runtime_identity_copy(plan.tokenizer_plan_identity,
                               tokenizer->tokenizer_plan_identity);
    yvex_runtime_identity_copy(plan.prompt_policy_identity,
                               tokenizer->prompt_policy_identity);
    yvex_runtime_identity_copy(plan.producer_plan_identity,
                               producer_identity);
    yvex_runtime_identity_copy(plan.logits_plan_identity,
                               logits->output_head_plan_identity);
    yvex_runtime_identity_copy(plan.sampling_policy_identity,
                               context->options.sampling_policy.policy_identity);
    yvex_runtime_identity_copy(plan.kernel_bundle_identity,
                               context->execution_profile.kernel_bundle_identity);
    yvex_runtime_identity_copy(plan.execution_profile_identity,
                               context->execution_profile.identity);
    yvex_runtime_identity_copy(plan.workload_profile_identity, context->capacity.workload_profile.identity);
    yvex_core_text_copy(plan.hardware_profile, sizeof(plan.hardware_profile),
                        context->capacity.hardware_profile.name);
    if (context->speculation) {
        const yvex_speculation_family_policy *policy =
            yvex_runtime_speculation_policy_get(context->speculation);
        if (!policy || !yvex_sha256_hex_valid(policy->policy_identity))
            return generation_context_refuse(
                err, YVEX_ERR_STATE,
                "DSpark policy identity is unavailable");
        yvex_runtime_identity_copy(plan.speculation_policy_identity,
                                   policy->policy_identity);
    }
    if (!yvex_runtime_generation_stop_identity(
            tokenizer, context->additional_stops,
            context->options.additional_stop_token_count,
            plan.stop_policy_identity) ||
        !yvex_runtime_generation_plan_identity(
            &plan, plan.generation_plan_identity))
        return generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation plan identity derivation failed");
    context->plan = plan;
    return YVEX_OK;
}

static int generation_execution_owners_open(
    yvex_runtime_generation_context *context,
    const yvex_runtime_generation_options *options,
    const yvex_runtime_logits_plan_summary **logits_plan,
    unsigned long long *workspace_bytes, yvex_error *err)
{
    yvex_runtime_transformer_options transformer = {0};
    yvex_runtime_decoder_execution_options decoder = {0};
    yvex_runtime_logits_options logits = {0};
    yvex_runtime_sampling_options sampling = {0};
    yvex_runtime_speculation_options speculation = {0};
    const yvex_runtime_session_view *session_view =
        yvex_runtime_session_view_get(context->session);
    unsigned long long compatible_width = 0ull, draft_workspace = 0ull;
    unsigned long long workspace_token_capacity;
    const int decoder_producer =
        context->model_view && yvex_compiled_model_plan_forward(context->model_view->compiled_plan) != NULL;
    int device_selection = session_view &&
        generation_device_selection(context, session_view->backend);
    int rc;

    context->device_selection = device_selection;
    *workspace_bytes = 0ull;
    if (decoder_producer &&
        (options->mode != YVEX_GENERATION_MODE_TARGET_ONLY ||
         options->compatible_operation_batching))
        return generation_context_refuse(
            err, YVEX_ERR_UNSUPPORTED,
            options->compatible_operation_batching
                ? "heterogeneous decoder compatible-operation batching is not admitted"
                : "heterogeneous decoder execution has no admitted draft producer");
    if (options->compatible_operation_batching) {
        rc = generation_scheduler_maximum_width(context, &compatible_width, err);
        if (rc != YVEX_OK) return rc;
    } else {
        compatible_width = 1ull;
    }
    rc = yvex_runtime_private_model_scheduler_acquire(
        context->model, options->concurrent_sequences,
        options->runnable_sequences, compatible_width, err);
    if (rc != YVEX_OK) return rc;
    context->scheduler_acquired = 1;

    workspace_token_capacity = options->prefill_chunk_tokens;
    if (options->mode == YVEX_GENERATION_MODE_SPECULATIVE &&
        workspace_token_capacity < YVEX_SPECULATION_MAX_BLOCK + 2ull)
        workspace_token_capacity = YVEX_SPECULATION_MAX_BLOCK + 2ull;
    if (options->compatible_operation_batching &&
        workspace_token_capacity < options->concurrent_sequences)
        workspace_token_capacity = options->concurrent_sequences;
    transformer.maximum_host_bytes = options->maximum_host_bytes;
    transformer.maximum_device_bytes = options->maximum_device_bytes;
    transformer.context_capacity = options->context_capacity;
    transformer.workspace_token_capacity = workspace_token_capacity;
    transformer.minimum_device_workspace_bytes = context->capacity.sampling_workspace_bytes;
    transformer.engine_scheduling = options->compatible_operation_batching;
    transformer.scheduler_maximum_width = options->compatible_operation_batching
                                              ? compatible_width : 0ull;
    transformer.cancel_requested = options->cancel_requested;
    transformer.cancel_context = options->cancel_context;
    transformer.evidence_level =
        runtime_attention_evidence(options->evidence_profile);
    transformer.device_hidden_output =
        device_selection && options->mode == YVEX_GENERATION_MODE_TARGET_ONLY;
    transformer.execution_profile = &context->execution_profile;
    if (decoder_producer) {
        decoder.context_capacity = options->context_capacity;
        decoder.token_capacity = workspace_token_capacity;
        decoder.maximum_host_bytes = options->maximum_host_bytes;
        decoder.maximum_device_bytes = options->maximum_device_bytes;
        decoder.cancel_requested = options->cancel_requested;
        decoder.cancel_context = options->cancel_context;
        decoder.execution_profile = &context->execution_profile;
        rc = yvex_runtime_decoder_execution_context_open(
            &context->decoder_execution, context->model, context->session,
            &decoder, err);
    } else {
        rc = yvex_runtime_transformer_context_open(
            &context->transformer, context->model, context->session,
            &transformer, workspace_bytes, err);
    }
    if (rc != YVEX_OK && !yvex_error_is_set(err))
        return generation_context_refuse(
            err, rc,
            decoder_producer
                ? "compiled token-forward execution owner refused without a diagnostic"
                : "Transformer execution owner refused without a diagnostic");
    logits.maximum_rows = options->mode == YVEX_GENERATION_MODE_SPECULATIVE
                              ? YVEX_SPECULATION_MAX_BLOCK + 1ull
                              : options->compatible_operation_batching
                                    ? compatible_width : 1ull;
    logits.maximum_host_bytes = options->maximum_host_bytes;
    logits.maximum_device_bytes = options->maximum_device_bytes;
    logits.evidence_profile = options->evidence_profile;
    logits.device_selection = device_selection;
    logits.execution_profile = &context->execution_profile;
    logits.cancel_requested = options->cancel_requested;
    logits.cancel_context = options->cancel_context;
    if (rc == YVEX_OK && decoder_producer)
        rc = yvex_runtime_logits_context_open_program(
            &context->logits, context->model, context->session,
            &logits, err);
    else if (rc == YVEX_OK)
        rc = yvex_runtime_logits_context_open(
            &context->logits, context->model, context->session,
            yvex_runtime_transformer_context_plan(context->transformer),
            &logits, err);
    if (rc != YVEX_OK && !yvex_error_is_set(err))
        return generation_context_refuse(
            err, rc, "compiled output execution owner refused without a diagnostic");
    *logits_plan = rc == YVEX_OK
                       ? yvex_runtime_logits_plan_summary_get(context->logits)
                       : NULL;
    if (rc == YVEX_OK && !*logits_plan)
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE, "runtime logits plan is unavailable");
    if (rc != YVEX_OK) return rc;
    sampling.maximum_vocabulary_size =
        yvex_tokenizer_vocab_size(context->tokenizer);
    sampling.selection_vocabulary_size =
        yvex_tokenizer_vocab_size(context->tokenizer);
    sampling.maximum_rows = options->mode == YVEX_GENERATION_MODE_SPECULATIVE
                                ? YVEX_SPECULATION_MAX_BLOCK + 1ull : 1ull;
    sampling.maximum_host_bytes = options->maximum_host_bytes;
    sampling.device_selection = device_selection;
    sampling.cancel_requested = options->cancel_requested;
    sampling.cancel_context = options->cancel_context;
    rc = yvex_runtime_sampling_context_open(
        &context->sampling, *logits_plan, &context->options.sampling_policy,
        &sampling, err);
    if (rc != YVEX_OK && !yvex_error_is_set(err))
        return generation_context_refuse(
            err, rc, "sampling execution owner refused without a diagnostic");
    if (rc != YVEX_OK || options->mode != YVEX_GENERATION_MODE_SPECULATIVE)
        return rc;
    speculation.backend = options->backend;
    speculation.context_capacity = options->context_capacity;
    speculation.prefill_chunk_tokens = options->prefill_chunk_tokens;
    speculation.maximum_host_bytes = options->maximum_host_bytes;
    speculation.maximum_device_bytes = options->maximum_device_bytes;
    speculation.engine_scheduling = options->compatible_operation_batching;
    speculation.scheduler_maximum_width = options->compatible_operation_batching
                                              ? compatible_width : 0ull;
    speculation.cancel_requested = options->cancel_requested;
    speculation.cancel_context = options->cancel_context;
    speculation.execution_profile = &context->execution_profile;
    rc = yvex_runtime_speculation_context_open(
        &context->speculation, context->model, context->session,
        context->transformer, context->logits, context->sampling,
        &context->options.sampling_policy, &speculation, &draft_workspace, err);
    if (rc == YVEX_OK && draft_workspace > *workspace_bytes)
        *workspace_bytes = draft_workspace;
    return rc;
}

int yvex_runtime_generation_context_summary_copy(
    const yvex_runtime_generation_context *context,
    yvex_runtime_generation_context_summary *summary, yvex_error *err)
{
    yvex_runtime_generation_context *mutable =
        (yvex_runtime_generation_context *)context;
    yvex_runtime_sampling_context_summary sampling;
    yvex_token_sequence_summary sequence;
    unsigned int lifecycle;
    int rc;
    if (!context || !summary)
        return generation_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "generation context and snapshot output are required");
    rc = yvex_runtime_private_generation_enter(mutable, err);
    if (rc != YVEX_OK) return rc;
    memset(summary, 0, sizeof(*summary));
    rc = yvex_runtime_sampling_context_snapshot(context->sampling, &sampling, err);
    if (rc == YVEX_OK)
        rc = yvex_token_sequence_summary_get(context->sequence, &sequence, err);
    lifecycle = atomic_load_explicit(&context->lifecycle, memory_order_acquire);
    if (rc == YVEX_OK) {
        summary->schema_version = YVEX_RUNTIME_GENERATION_SCHEMA_V3;
        summary->open = !(lifecycle & YVEX_GENERATION_LIFECYCLE_CLOSING);
        summary->busy = 0;
        summary->closing =
            (lifecycle & YVEX_GENERATION_LIFECYCLE_CLOSING) != 0u;
        summary->execution_count = context->execution_count;
        summary->failure_count = context->failure_count +
            atomic_load_explicit(&context->admission_failures,
                                 memory_order_relaxed);
        summary->cancellation_count = context->cancellation_count;
        summary->token_capacity = context->options.maximum_new_tokens;
        summary->text_capacity = context->options.maximum_output_bytes;
        summary->workspace_bytes = context->workspace_bytes +
                                   sampling.workspace_bytes;
        summary->concurrent_sequences = context->capacity.capacity_plan.concurrent_sequences;
        summary->capacity_required_bytes = context->capacity.capacity_plan.required_bytes;
        summary->capacity_unreserved_bytes = context->capacity.capacity_plan.unreserved_bytes;
        summary->compatible_operation_batching =
            context->options.compatible_operation_batching;
        yvex_runtime_identity_copy(summary->generation_plan_identity,
                                   context->plan.generation_plan_identity);
        yvex_runtime_identity_copy(summary->capacity_plan_identity,
                                   context->capacity.capacity_plan.identity);
        yvex_runtime_identity_copy(summary->token_sequence_identity,
                                   sequence.state_identity);
        yvex_runtime_identity_copy(summary->rng_state_identity,
                                   sampling.rng_state_identity);
    }
    yvex_runtime_private_generation_leave(mutable, rc, 0);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_runtime_generation_context_open(
    yvex_runtime_generation_context **out, yvex_model_engine *model,
    yvex_runtime_execution_session *session,
    const yvex_runtime_generation_options *options, yvex_error *err)
{
    yvex_runtime_generation_context *context = NULL;
    yvex_runtime_decode_options decode_options = {0};
    yvex_model_engine_failure state_failure = {0};
    yvex_model_engine_failure workspace_failure = {0};
    yvex_graph_attention_capacity_plan *workspace_capacity = NULL;
    yvex_tokenizer_decode_options decoder_options = {0};
    const yvex_runtime_logits_plan_summary *logits_plan = NULL;
    unsigned long long hidden_bytes, logits_bytes, execution_workspace = 0ull;
    unsigned long long physical_rows = 0ull;
    int rc = YVEX_OK;
    if (out) *out = NULL;
    if (!out || !model || !session || !generation_options_valid(options))
        return generation_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "complete bounded generation options are required");
    context = yvex_core_calloc(1u, sizeof(*context));
    if (!context)
        return generation_context_refuse(
            err, YVEX_ERR_NOMEM,
            "generation context allocation failed");
    context->model = model;
    context->session = session;
    context->model_view = yvex_model_engine_view_get(model);
    context->tokenizer = context->model_view ? context->model_view->tokenizer : NULL;
    context->options = *options;
    if (!context->options.concurrent_sequences)
        context->options.concurrent_sequences = 1ull;
    if (!context->options.runnable_sequences)
        context->options.runnable_sequences = context->options.concurrent_sequences;
    if (context->options.prefill_chunk_tokens > context->options.context_capacity)
        context->options.prefill_chunk_tokens = context->options.context_capacity;
    atomic_init(&context->lifecycle, 0u);
    atomic_init(&context->admission_failures, 0ull);
    if (!context->model_view || !context->tokenizer ||
        !yvex_runtime_session_view_get(session) ||
        yvex_runtime_session_view_get(session)->engine != model) {
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation model, session, and tokenizer are not paired");
        goto failure;
    }
    rc = generation_stops_open(
        context, yvex_tokenizer_vocab_size(context->tokenizer), err);
    if (rc != YVEX_OK) goto failure;
    rc = yvex_runtime_sampling_policy_seal(
        &context->options.sampling_policy,
        yvex_tokenizer_vocab_size(context->tokenizer), err);
    if (rc != YVEX_OK) goto failure;
    rc = generation_capacity_build(context, &workspace_capacity, err);
    if (rc != YVEX_OK) goto failure;
    rc = generation_execution_profile_build(context, err);
    if (rc != YVEX_OK) goto failure;
    if (context->capacity.capacity_plan.schema_version &&
        yvex_runtime_session_view_get(session)->attention_state_provider) {
        rc = yvex_runtime_session_configure_persistent_pages(
            session, &context->capacity.capacity_plan, &state_failure, err);
        if (rc != YVEX_OK) goto failure;
    } else if (context->capacity.capacity_plan.schema_version &&
               context->model_view->attention) {
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE,
            "attention execution has no admitted persistent-state provider");
        goto failure;
    }
    rc = generation_execution_owners_open(
        context, &context->options, &logits_plan, &execution_workspace, err);
    if (rc == YVEX_OK)
        physical_rows = context->capacity.physical_rows;
    if (rc != YVEX_OK) goto failure;
    if (context->options.backend == YVEX_BACKEND_KIND_CUDA)
        rc = yvex_runtime_session_prepare_attention_workspace(
            context->session,
            context->options.compatible_operation_batching ||
                    context->execution_profile.attention_resolution !=
                YVEX_EXECUTION_RESOLUTION_EXACT
                ? YVEX_RUNTIME_MODE_EAGER : YVEX_RUNTIME_MODE_FULL,
            YVEX_RUNTIME_SCOPE_ATTENTION_ENVELOPE, YVEX_ATTENTION_EVIDENCE_NONE,
            workspace_capacity, physical_rows, execution_workspace,
            &workspace_failure, err);
    yvex_graph_attention_capacity_plan_close(&workspace_capacity);
    if (rc != YVEX_OK) goto failure;
    rc = generation_plan_build(context, err);
    if (rc != YVEX_OK) goto failure;
    decode_options.maximum_steps = options->maximum_new_tokens;
    decode_options.cancel_requested = options->cancel_requested;
    decode_options.cancel_context = options->cancel_context;
    if (context->transformer)
        rc = yvex_runtime_decode_context_open(
            &context->decode, context->transformer, session, &decode_options,
            err);
    if (rc != YVEX_OK) goto failure;
    decoder_options.skip_special_tokens = 1;
    decoder_options.require_complete_utf8 = 1;
    decoder_options.cancelled = options->cancel_requested;
    decoder_options.cancel_context = options->cancel_context;
    rc = yvex_tokenizer_decoder_open(
        &context->decoder, context->tokenizer, &decoder_options, err);
    if (rc != YVEX_OK) goto failure;
    rc = yvex_token_sequence_open(
        &context->sequence, options->maximum_new_tokens, err);
    if (rc != YVEX_OK) goto failure;
    context->hidden_count = logits_plan->hidden_width;
    context->logits_count = logits_plan->vocabulary_size;
    if ((context->speculation &&
         !yvex_core_u64_mul(context->hidden_count,
                            YVEX_SPECULATION_MAX_BLOCK + 2ull,
                            &context->hidden_count)) ||
        !yvex_core_u64_mul(context->hidden_count, sizeof(float), &hidden_bytes) ||
        !yvex_core_u64_mul(context->logits_count, sizeof(float), &logits_bytes)) {
        rc = generation_context_refuse(err, YVEX_ERR_NOMEM,
            "generation-local workspace geometry overflowed");
        goto failure;
    }
    context->workspace_bytes = hidden_bytes;
    if ((!context->device_selection &&
         !yvex_core_u64_add(context->workspace_bytes, logits_bytes,
                            &context->workspace_bytes)) ||
        context->workspace_bytes > SIZE_MAX ||
        (options->maximum_host_bytes &&
         context->workspace_bytes > options->maximum_host_bytes)) {
        rc = generation_context_refuse(err, YVEX_ERR_NOMEM,
            "generation-local workspace exceeds its budget");
        goto failure;
    }
    context->hidden = yvex_core_calloc((size_t)context->hidden_count, sizeof(float));
    if (!context->device_selection)
        context->logits_row = yvex_core_calloc((size_t)context->logits_count, sizeof(float));
    if (!context->hidden ||
        (!context->device_selection && !context->logits_row)) {
        rc = generation_context_refuse(err, YVEX_ERR_NOMEM,
            "generation-local workspace allocation failed");
        goto failure;
    }
    if (pthread_mutex_init(&context->drain_mutex, NULL) != 0) {
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation lifecycle mutex initialization failed");
        goto failure;
    }
    context->drain_mutex_ready = 1;
    if (pthread_cond_init(&context->drain_condition, NULL) != 0) {
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation lifecycle condition initialization failed");
        goto failure;
    }
    context->drain_condition_ready = 1;
    context->continuation_allowed = 1;
    *out = context;
    yvex_error_clear(err);
    return YVEX_OK;

failure:
    yvex_graph_attention_capacity_plan_close(&workspace_capacity);
    if (context) {
        yvex_error cleanup;
        yvex_error_clear(&cleanup);
        yvex_token_sequence_close(&context->sequence);
        yvex_tokenizer_decoder_close(&context->decoder);
        (void)yvex_runtime_decode_context_close(&context->decode, &cleanup);
        (void)yvex_runtime_speculation_context_close(
            &context->speculation, &cleanup);
        (void)yvex_runtime_sampling_context_close(&context->sampling, &cleanup);
        (void)yvex_runtime_logits_context_close(&context->logits, &cleanup);
        (void)yvex_runtime_decoder_execution_context_close(
            &context->decoder_execution, &cleanup);
        (void)yvex_runtime_transformer_context_close(
            &context->transformer, &cleanup);
        (void)yvex_runtime_private_model_scheduler_finish(
            context->model, &context->scheduler_acquired, &cleanup);
        if (context->drain_condition_ready)
            (void)pthread_cond_destroy(&context->drain_condition);
        if (context->drain_mutex_ready)
            (void)pthread_mutex_destroy(&context->drain_mutex);
        yvex_core_free(context->logits_row);
        yvex_core_free(context->hidden);
        yvex_core_free(context->additional_stops);
        yvex_core_free(context);
    }
    return rc;
}

const yvex_runtime_generation_plan_summary *yvex_runtime_generation_plan_summary_get(
    const yvex_runtime_generation_context *context)
{
    return context ? &context->plan : NULL;
}

static int generation_checkpoint_identity(
    const yvex_runtime_generation_checkpoint *checkpoint,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!checkpoint ||
        !yvex_sha256_update_text(&hash,
                                 "yvex.runtime.generation.checkpoint.v1") ||
        !yvex_sha256_update_u64(&hash, checkpoint->schema_version) ||
        !yvex_sha256_update_text(&hash,
                                 checkpoint->generation_plan_identity) ||
        !yvex_sha256_update_text(&hash,
                                 checkpoint->sampling.checkpoint_identity) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int generation_checkpoint_plan_identity(
    const yvex_runtime_generation_plan_summary *plan,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_runtime_generation_plan_summary compatible;
    if (!plan || !output) return 0;
    compatible = *plan;
    memset(compatible.execution_profile_identity, 0,
           sizeof(compatible.execution_profile_identity));
    return yvex_runtime_generation_plan_identity(&compatible, output);
}

int yvex_runtime_generation_context_checkpoint(
    yvex_runtime_generation_context *context,
    yvex_runtime_generation_checkpoint *checkpoint, yvex_error *err)
{
    int rc;
    if (checkpoint) memset(checkpoint, 0, sizeof(*checkpoint));
    if (!context || !checkpoint)
        return generation_context_refuse(
            err, YVEX_ERR_INVALID_ARG,
            "generation checkpoint output is required");
    rc = yvex_runtime_private_generation_enter(context, err);
    if (rc != YVEX_OK) return rc;
    checkpoint->schema_version = YVEX_RUNTIME_GENERATION_CHECKPOINT_SCHEMA_V1;
    if (!generation_checkpoint_plan_identity(
            &context->plan, checkpoint->generation_plan_identity))
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation checkpoint plan compatibility identity failed");
    if (rc == YVEX_OK)
        rc = yvex_runtime_sampling_context_checkpoint(
            context->sampling, &checkpoint->sampling, err);
    if (rc == YVEX_OK &&
        !generation_checkpoint_identity(checkpoint,
                                        checkpoint->checkpoint_identity))
        rc = generation_context_refuse(
            err, YVEX_ERR_STATE, "generation checkpoint identity failed");
    if (rc != YVEX_OK) memset(checkpoint, 0, sizeof(*checkpoint));
    yvex_runtime_private_generation_leave(context, rc, 0);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_runtime_generation_context_restore(
    yvex_runtime_generation_context *context,
    const yvex_runtime_generation_checkpoint *checkpoint, yvex_error *err)
{
    char identity[YVEX_SHA256_HEX_CAP];
    char compatible_plan[YVEX_SHA256_HEX_CAP];
    int rc;
    if (!context || !checkpoint)
        return generation_context_refuse(err, YVEX_ERR_INVALID_ARG,
                                         "generation checkpoint is required");
    rc = yvex_runtime_private_generation_enter(context, err);
    if (rc != YVEX_OK) return rc;
    if (checkpoint->schema_version !=
            YVEX_RUNTIME_GENERATION_CHECKPOINT_SCHEMA_V1 ||
        !generation_checkpoint_plan_identity(
            &context->plan, compatible_plan) ||
        strcmp(checkpoint->generation_plan_identity,
               compatible_plan) != 0 ||
        !generation_checkpoint_identity(checkpoint, identity) ||
        strcmp(identity, checkpoint->checkpoint_identity) != 0)
        rc = generation_context_refuse(
            err, YVEX_ERR_FORMAT,
            "generation checkpoint is incompatible or corrupt");
    if (rc == YVEX_OK)
        rc = yvex_runtime_sampling_context_restore(
            context->sampling, &checkpoint->sampling, err);
    yvex_runtime_private_generation_leave(context, rc, 0);
    if (rc == YVEX_OK) yvex_error_clear(err);
    return rc;
}

int yvex_runtime_generation_context_close(
    yvex_runtime_generation_context **context, yvex_error *err)
{
    yvex_runtime_generation_context *owner;
    unsigned int observed, desired;
    int rc;
    if (!context || !*context) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    owner = *context;
    observed = atomic_load_explicit(&owner->lifecycle, memory_order_acquire);
    while (!(observed & YVEX_GENERATION_LIFECYCLE_CLOSING)) {
        desired = observed | YVEX_GENERATION_LIFECYCLE_CLOSING;
        if (atomic_compare_exchange_weak_explicit(
                &owner->lifecycle, &observed, desired,
                memory_order_acq_rel, memory_order_acquire))
            break;
    }
    if (owner->drain_mutex_ready) {
        if (pthread_mutex_lock(&owner->drain_mutex) != 0)
            return generation_context_refuse(
                err, YVEX_ERR_STATE,
                "generation close drain lock failed");
        while (atomic_load_explicit(&owner->lifecycle, memory_order_acquire) &
               YVEX_GENERATION_LIFECYCLE_ACTIVE) {
            if (!owner->drain_condition_ready ||
                pthread_cond_wait(&owner->drain_condition,
                                  &owner->drain_mutex) != 0) {
                (void)pthread_mutex_unlock(&owner->drain_mutex);
                return generation_context_refuse(
                    err, YVEX_ERR_STATE,
                    "generation close drain failed");
            }
        }
        (void)pthread_mutex_unlock(&owner->drain_mutex);
    }
    rc = yvex_runtime_speculation_context_close(&owner->speculation, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_sampling_context_close(&owner->sampling, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_context_close(&owner->logits, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_decode_context_close(&owner->decode, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_decoder_execution_context_close(
            &owner->decoder_execution, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_transformer_context_close(&owner->transformer, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_private_model_scheduler_finish(
            owner->model, &owner->scheduler_acquired, err);
    if (rc != YVEX_OK) return rc;
    yvex_tokenizer_decoder_close(&owner->decoder);
    yvex_token_sequence_close(&owner->sequence);
    if (owner->drain_condition_ready &&
        pthread_cond_destroy(&owner->drain_condition) != 0)
        return generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation close condition cleanup failed");
    owner->drain_condition_ready = 0;
    if (owner->drain_mutex_ready &&
        pthread_mutex_destroy(&owner->drain_mutex) != 0)
        return generation_context_refuse(
            err, YVEX_ERR_STATE,
            "generation close mutex cleanup failed");
    owner->drain_mutex_ready = 0;
    atomic_store_explicit(&owner->lifecycle,
                          YVEX_GENERATION_LIFECYCLE_CLOSED,
                          memory_order_release);
    yvex_core_free(owner->logits_row);
    yvex_core_free(owner->hidden);
    yvex_core_free(owner->additional_stops);
    memset(owner, 0, sizeof(*owner));
    yvex_core_free(owner);
    *context = NULL;
    yvex_error_clear(err);
    return YVEX_OK;
}
