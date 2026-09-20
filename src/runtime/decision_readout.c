/* Execute finite-candidate likelihood readout without sampling or generation. */
#include "src/runtime/private.h"

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/internal/decision_readout.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/logits.h>

typedef struct {
    yvex_runtime_execution_session *session;
    yvex_runtime_decoder_execution_context *decoder;
    yvex_runtime_logits_context *logits;
} readout_run;

struct yvex_decision_readout_context {
    yvex_model_engine *model;
    const yvex_model_engine_view *model_view;
    yvex_decision_readout_options options;
    yvex_model_engine_summary model_summary;
    yvex_execution_workload_profile workload;
    yvex_runtime_execution_profile profile;
    yvex_decision_readout_context_summary summary;
    readout_run source;
    pthread_mutex_t mutex;
    int mutex_ready, busy, prefix_prepared, invalidated;
};

struct yvex_decision_readout_prefix {
    yvex_runtime_session_prefix *runtime;
    float *logits;
    unsigned long long vocabulary_size;
    unsigned long long preparation_nanoseconds;
    yvex_runtime_session_prefix_summary runtime_summary;
    yvex_decision_readout_prefix_summary summary;
};

static int readout_refuse(yvex_error *err, yvex_status status,
                          const char *where, const char *reason)
{
    yvex_error_set(err, status, where, reason);
    return status;
}

static int readout_identity_text(
    const char *domain, char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!domain || !output || !yvex_sha256_update_text(&hash, domain) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int readout_token_identity(
    const char *domain, const unsigned int *tokens, unsigned long long count,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!domain || !tokens || !count || !output ||
        !yvex_sha256_update_text(&hash, domain) ||
        !yvex_sha256_update_u64(&hash, count)) return 0;
    for (index = 0ull; index < count; ++index)
        if (!yvex_sha256_update_u64(&hash, tokens[index])) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int readout_input_identity(
    const unsigned int *tokens, unsigned long long token_start,
    unsigned long long count, char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!tokens || !count || !output ||
        !yvex_sha256_update_text(
            &hash, "yvex.decision-readout.teacher-input.v1") ||
        !yvex_sha256_update_u64(&hash, token_start) ||
        !yvex_sha256_update_u64(&hash, count)) return 0;
    for (index = 0ull; index < count; ++index)
        if (!yvex_sha256_update_u64(&hash, tokens[index])) return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int readout_update_double(yvex_sha256 *hash, double value)
{
    uint64_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return yvex_sha256_update_u64(hash, (unsigned long long)bits);
}

int yvex_decision_readout_log_probability(
    const float *logits, unsigned long long vocabulary_size,
    unsigned int token_id, double *out, yvex_error *err)
{
    double maximum, total = 0.0;
    unsigned long long index;
    if (out) *out = 0.0;
    if (!logits || !out || !vocabulary_size || token_id >= vocabulary_size)
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.score",
            "finite vocabulary logits and one in-range token are required");
    maximum = (double)logits[0];
    if (!isfinite(maximum))
        return readout_refuse(
            err, YVEX_ERR_FORMAT, "runtime.decision-readout.score",
            "candidate logits contain a non-finite value");
    for (index = 1ull; index < vocabulary_size; ++index) {
        double value = (double)logits[index];
        if (!isfinite(value))
            return readout_refuse(
                err, YVEX_ERR_FORMAT, "runtime.decision-readout.score",
                "candidate logits contain a non-finite value");
        if (value > maximum) maximum = value;
    }
    for (index = 0ull; index < vocabulary_size; ++index)
        total += exp((double)logits[index] - maximum);
    *out = (double)logits[token_id] - maximum - log(total);
    if (!isfinite(*out))
        return readout_refuse(
            err, YVEX_ERR_FORMAT, "runtime.decision-readout.score",
            "candidate token log-probability is non-finite");
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_decision_readout_relative_distribution(
    yvex_decision_readout_candidate_result *candidates,
    unsigned long long candidate_count, yvex_error *err)
{
    double maximum, total = 0.0;
    unsigned long long index;
    if (!candidates || !candidate_count)
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.distribution",
            "one finite candidate score population is required");
    maximum = candidates[0].candidate_log_likelihood;
    if (!isfinite(maximum))
        return readout_refuse(
            err, YVEX_ERR_FORMAT, "runtime.decision-readout.distribution",
            "candidate score population is non-finite");
    for (index = 1ull; index < candidate_count; ++index) {
        double score = candidates[index].candidate_log_likelihood;
        if (!isfinite(score))
            return readout_refuse(
                err, YVEX_ERR_FORMAT, "runtime.decision-readout.distribution",
                "candidate score population is non-finite");
        if (score > maximum) maximum = score;
    }
    for (index = 0ull; index < candidate_count; ++index)
        total += exp(candidates[index].candidate_log_likelihood - maximum);
    if (!isfinite(total) || total <= 0.0)
        return readout_refuse(
            err, YVEX_ERR_FORMAT, "runtime.decision-readout.distribution",
            "candidate relative normalization failed");
    for (index = 0ull; index < candidate_count; ++index)
        candidates[index].relative_candidate_probability =
            exp(candidates[index].candidate_log_likelihood - maximum) / total;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int readout_profile_open(yvex_decision_readout_context *context,
                                yvex_error *err)
{
    yvex_runtime_session_summary session = {0};
    yvex_runtime_execution_profile_request request = {0};
    yvex_execution_workload_profile *workload = &context->workload;
    int rc = yvex_runtime_session_summary_copy(
        context->source.session, &session, err);
    if (rc != YVEX_OK) return rc;
    workload->schema_version = YVEX_EXECUTION_WORKLOAD_PROFILE_SCHEMA_V1;
    workload->kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY;
    workload->minimum_session_context = 1ull;
    workload->requested_session_context = context->options.context_capacity;
    workload->concurrent_sequences = 1ull;
    workload->logical_batch_tokens = 1ull;
    workload->prefill_chunk_tokens = context->options.maximum_prefix_tokens;
    workload->attention_microbatch_rows = 1ull;
    workload->moe_row_tile = 1ull;
    workload->output_head_rows = 1ull;
    workload->system_reserve_bytes = YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE;
    workload->latency_priority = 1;
    yvex_core_text_copy(workload->name, sizeof(workload->name),
                        "decision-readout-zero-decode-v0");
    rc = yvex_execution_workload_profile_seal(workload, err);
    if (rc != YVEX_OK) return rc;
    request.schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1;
    request.engine_generation = session.engine_generation;
    request.engine_specialization_identity =
        session.engine_specialization_identity;
    request.kernel_bundle_identity = session.engine_specialization_identity;
    request.workload_profile_identity = workload->identity;
    request.generation_mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY;
    request.evidence = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    request.execution_class = context->options.backend == YVEX_BACKEND_KIND_CPU
                                  ? YVEX_EXECUTION_CLASS_PORTABLE_REFERENCE
                                  : YVEX_EXECUTION_CLASS_DEVICE_NATIVE;
    request.attention_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    request.moe_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    request.sampling_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    return yvex_runtime_execution_profile_seal(
        &request, &context->profile, err);
}

static int readout_run_contexts_open(
    yvex_decision_readout_context *context, readout_run *run,
    unsigned long long token_capacity, yvex_error *err)
{
    yvex_runtime_decoder_execution_options decoder = {0};
    yvex_runtime_logits_options logits = {0};
    decoder.context_capacity = context->options.context_capacity;
    decoder.token_capacity = token_capacity;
    decoder.maximum_host_bytes = context->options.maximum_host_bytes;
    decoder.maximum_device_bytes = context->options.maximum_device_bytes;
    decoder.cancel_requested = context->options.cancel_requested;
    decoder.cancel_context = context->options.cancel_context;
    decoder.execution_profile = &context->profile;
    logits.maximum_rows = 1ull;
    logits.maximum_host_bytes = context->options.maximum_host_bytes;
    logits.maximum_device_bytes = context->options.maximum_device_bytes;
    logits.evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    logits.execution_profile = &context->profile;
    if (yvex_runtime_decoder_execution_context_open(
            &run->decoder, context->model, run->session, &decoder, err) !=
        YVEX_OK) return yvex_error_code(err);
    return yvex_runtime_logits_context_open_program(
        &run->logits, context->model, run->session, &logits, err);
}

static int readout_run_open(
    yvex_decision_readout_context *context,
    const yvex_runtime_session_prefix *prefix,
    readout_run *run, unsigned long long token_capacity,
    yvex_runtime_session_prefix_summary *attached, yvex_error *err)
{
    yvex_runtime_session_open_request request = {0};
    yvex_model_engine_failure failure = {0};
    int rc;
    memset(run, 0, sizeof(*run));
    request.backend = context->options.backend;
    request.maximum_host_bytes = context->options.maximum_host_bytes;
    request.maximum_device_bytes = context->options.maximum_device_bytes;
    rc = yvex_runtime_session_open(
        &run->session, context->model, &request, &failure, err);
    if (rc == YVEX_OK && prefix)
        rc = yvex_runtime_session_prefix_attach(
            run->session, prefix, attached, &failure, err);
    if (rc == YVEX_OK)
        rc = readout_run_contexts_open(
            context, run, token_capacity, err);
    return rc;
}

static int readout_run_close(readout_run *run, yvex_error *err)
{
    yvex_error cleanup;
    int rc = yvex_runtime_logits_context_close(&run->logits, err);
    int current = yvex_runtime_decoder_execution_context_close(
        &run->decoder, &cleanup);
    if (rc == YVEX_OK && current != YVEX_OK) {
        rc = current;
        if (err) *err = cleanup;
    }
    current = yvex_runtime_session_close(&run->session, &cleanup);
    if (rc == YVEX_OK && current != YVEX_OK) {
        rc = current;
        if (err) *err = cleanup;
    }
    return rc;
}

static int readout_context_identity(
    yvex_decision_readout_context_summary *summary)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!summary ||
        !yvex_sha256_update_text(&hash, "yvex.decision-readout.context.v1") ||
        !yvex_sha256_update_u64(&hash, summary->schema_version) ||
        !yvex_sha256_update_u64(&hash, summary->backend) ||
        !yvex_sha256_update_u64(&hash, summary->score_policy) ||
        !yvex_sha256_update_u64(&hash, summary->engine_generation) ||
        !yvex_sha256_update_u64(&hash, summary->vocabulary_size) ||
        !yvex_sha256_update_u64(&hash, summary->context_capacity) ||
        !yvex_sha256_update_u64(&hash, summary->maximum_candidate_count) ||
        !yvex_sha256_update_u64(&hash, summary->maximum_candidate_tokens) ||
        !yvex_sha256_update_text(&hash, summary->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, summary->artifact_identity) ||
        !yvex_sha256_update_text(&hash, summary->runtime_binding_identity) ||
        !yvex_sha256_update_text(&hash, summary->tokenizer_identity) ||
        !yvex_sha256_update_text(&hash, summary->forward_program_identity) ||
        !yvex_sha256_update_text(&hash, summary->output_program_identity) ||
        !yvex_sha256_update_text(
            &hash, summary->readout_implementation_identity) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, summary->context_identity);
    return 1;
}

static int readout_context_validate_options(
    const yvex_decision_readout_options *options)
{
    return options && options->schema_version == YVEX_DECISION_READOUT_SCHEMA_V1 &&
           (options->backend == YVEX_BACKEND_KIND_CPU ||
            options->backend == YVEX_BACKEND_KIND_CUDA) &&
           options->score_policy ==
               YVEX_DECISION_READOUT_SCORE_LOG_LIKELIHOOD &&
           options->context_capacity && options->maximum_prefix_tokens &&
           options->maximum_prefix_tokens < options->context_capacity &&
           options->maximum_candidate_count &&
           options->maximum_candidate_tokens &&
           options->maximum_candidate_tokens < options->context_capacity &&
           options->maximum_prefix_state_bytes &&
           options->expected_engine_generation &&
           yvex_sha256_hex_valid(options->expected_runtime_model_identity) &&
           yvex_sha256_hex_valid(options->expected_runtime_binding_identity) &&
           yvex_sha256_hex_valid(options->expected_tokenizer_identity);
}

int yvex_decision_readout_context_open(
    yvex_decision_readout_context **out, yvex_model_engine *model,
    const yvex_decision_readout_options *options, yvex_error *err)
{
    yvex_decision_readout_context *context = NULL;
    yvex_runtime_session_open_request session_request = {0};
    yvex_model_engine_failure failure = {0};
    const yvex_tokenizer_plan_summary *tokenizer;
    const yvex_program_physical_summary *forward, *output;
    const yvex_program_token_interface *interface;
    int rc;
    if (out) *out = NULL;
    if (!out || !model || !readout_context_validate_options(options))
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.open",
            "exact model identities and bounded readout options are required");
    context = calloc(1u, sizeof(*context));
    if (!context)
        return readout_refuse(
            err, YVEX_ERR_NOMEM, "runtime.decision-readout.open",
            "readout context allocation failed");
    context->model = model;
    context->model_view = yvex_model_engine_view_get(model);
    context->options = *options;
    rc = yvex_model_engine_summary_copy(model, &context->model_summary, err);
    tokenizer = context->model_view
                    ? yvex_tokenizer_plan_summary_get(
                          context->model_view->tokenizer)
                    : NULL;
    forward = context->model_view
                  ? yvex_program_physical_summary_get(
                        yvex_compiled_model_plan_forward(
                            context->model_view->compiled_plan))
                  : NULL;
    output = context->model_view
                 ? yvex_program_physical_summary_get(
                       yvex_compiled_model_plan_output(
                           context->model_view->compiled_plan))
                 : NULL;
    if (rc == YVEX_OK &&
        (!context->model_view || !context->model_view->binding || !tokenizer ||
         !forward || !output ||
         context->model_summary.engine_generation !=
             options->expected_engine_generation ||
         strcmp(context->model_summary.runtime_model_identity,
                options->expected_runtime_model_identity) != 0 ||
         strcmp(context->model_summary.runtime_binding_identity,
                options->expected_runtime_binding_identity) != 0 ||
         strcmp(tokenizer->tokenizer_plan_identity,
                options->expected_tokenizer_identity) != 0))
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.open",
            "model, binding, tokenizer, or engine identity is stale");
    session_request.backend = options->backend;
    session_request.maximum_host_bytes = options->maximum_host_bytes;
    session_request.maximum_device_bytes = options->maximum_device_bytes;
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_open(
            &context->source.session, model, &session_request, &failure, err);
    if (rc == YVEX_OK) rc = readout_profile_open(context, err);
    if (rc == YVEX_OK)
        rc = readout_run_contexts_open(
            context, &context->source, options->maximum_prefix_tokens, err);
    interface = rc == YVEX_OK
                    ? yvex_runtime_decoder_execution_interface(
                          context->source.decoder)
                    : NULL;
    if (rc == YVEX_OK &&
        (!interface || !interface->vocabulary_size ||
         interface->vocabulary_size != tokenizer->vocabulary_size))
        rc = readout_refuse(
            err, YVEX_ERR_FORMAT, "runtime.decision-readout.open",
            "compiled forward and tokenizer vocabulary disagree");
    if (rc == YVEX_OK && pthread_mutex_init(&context->mutex, NULL) != 0)
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.open",
            "readout synchronization initialization failed");
    if (rc == YVEX_OK) context->mutex_ready = 1;
    if (rc == YVEX_OK) {
        yvex_decision_readout_context_summary *summary = &context->summary;
        summary->schema_version = YVEX_DECISION_READOUT_SCHEMA_V1;
        summary->backend = options->backend;
        summary->score_policy = options->score_policy;
        summary->engine_generation = context->model_summary.engine_generation;
        summary->vocabulary_size = interface->vocabulary_size;
        summary->context_capacity = options->context_capacity;
        summary->maximum_candidate_count = options->maximum_candidate_count;
        summary->maximum_candidate_tokens = options->maximum_candidate_tokens;
        yvex_runtime_identity_copy(summary->runtime_model_identity,
                                   context->model_summary.runtime_model_identity);
        yvex_runtime_identity_copy(summary->artifact_identity,
                                   context->model_summary.artifact_identity);
        yvex_runtime_identity_copy(summary->runtime_binding_identity,
                                   context->model_summary.runtime_binding_identity);
        yvex_runtime_identity_copy(summary->tokenizer_identity,
                                   tokenizer->tokenizer_plan_identity);
        yvex_runtime_identity_copy(summary->forward_program_identity,
                                   forward->identity);
        yvex_runtime_identity_copy(summary->output_program_identity,
                                   output->identity);
        if (!readout_identity_text(
                "yvex.decision-readout.zero-decode.v1",
                summary->readout_implementation_identity) ||
            !readout_context_identity(summary))
            rc = readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.open",
                "readout implementation identity could not seal");
        else
            summary->available = 1;
    }
    if (rc != YVEX_OK) {
        yvex_error primary = err ? *err : (yvex_error){0};
        (void)yvex_decision_readout_context_close(&context, NULL);
        if (err) *err = primary;
        return rc;
    }
    *out = context;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_decision_readout_context_summary_copy(
    const yvex_decision_readout_context *context,
    yvex_decision_readout_context_summary *summary, yvex_error *err)
{
    if (!context || !summary || !context->summary.available ||
        !yvex_sha256_hex_valid(context->summary.context_identity))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.summary",
            "one available readout context is required");
    *summary = context->summary;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int readout_decoder_logits(
    yvex_decision_readout_context *context, readout_run *run,
    const unsigned int *tokens, unsigned long long token_start,
    unsigned long long token_count, unsigned long long row_ordinal,
    float *logits, yvex_runtime_decoder_execution_result *execution,
    yvex_runtime_logits_row_result *row, yvex_error *err)
{
    yvex_runtime_decoder_execution_request request = {0};
    yvex_runtime_logits_source source = {0};
    char input_identity[YVEX_SHA256_HEX_CAP];
    if (!readout_input_identity(
            tokens, token_start, token_count, input_identity))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.forward",
            "teacher-forced input identity could not seal");
    request.token_ids = tokens;
    request.token_start = token_start;
    request.token_count = token_count;
    request.input_identity = input_identity;
    if (yvex_runtime_decoder_execution_execute(
            run->decoder, &request, execution, err) != YVEX_OK)
        return yvex_error_code(err);
    if (yvex_runtime_logits_source_from_decoder(
            run->logits, &source, execution,
            token_start ? YVEX_LOGITS_SOURCE_DECODE
                        : YVEX_LOGITS_SOURCE_PREFILL,
            row_ordinal, err) != YVEX_OK)
        return yvex_error_code(err);
    return yvex_runtime_logits_project(
        run->logits, &source, context->options.backend, logits,
        context->summary.vocabulary_size, row, err);
}

static int readout_decoder_advance(
    readout_run *run, const unsigned int *tokens,
    unsigned long long token_start, unsigned long long token_count,
    yvex_runtime_decoder_execution_result *execution, yvex_error *err)
{
    yvex_runtime_decoder_execution_request request = {0};
    char input_identity[YVEX_SHA256_HEX_CAP];
    if (!readout_input_identity(
            tokens, token_start, token_count, input_identity))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.forward",
            "teacher-forced input identity could not seal");
    request.token_ids = tokens;
    request.token_start = token_start;
    request.token_count = token_count;
    request.input_identity = input_identity;
    return yvex_runtime_decoder_execution_execute(
        run->decoder, &request, execution, err);
}

static int readout_logits_identity(
    const float *logits, unsigned long long count,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long bytes;
    yvex_sha256_init(&hash);
    if (!logits || !count || !output ||
        !yvex_core_u64_mul(count, sizeof(*logits), &bytes) ||
        bytes > SIZE_MAX ||
        !yvex_sha256_update_text(
            &hash, "yvex.decision-readout.shared-logits.v1") ||
        !yvex_sha256_update_u64(&hash, count) ||
        !yvex_sha256_update(&hash, logits, (size_t)bytes) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int readout_prefix_identity(
    const yvex_decision_readout_context_summary *context,
    yvex_decision_readout_prefix_summary *summary)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!context || !summary ||
        !yvex_sha256_update_text(&hash, "yvex.decision-readout.prefix.v1") ||
        !yvex_sha256_update_u64(&hash, summary->schema_version) ||
        !yvex_sha256_update_u64(&hash, summary->engine_generation) ||
        !yvex_sha256_update_u64(&hash, summary->prefix_token_count) ||
        !yvex_sha256_update_u64(
            &hash, summary->committed_sequence_length) ||
        !yvex_sha256_update_text(&hash, context->context_identity) ||
        !yvex_sha256_update_text(&hash, summary->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, summary->runtime_binding_identity) ||
        !yvex_sha256_update_text(&hash, summary->tokenizer_identity) ||
        !yvex_sha256_update_text(&hash, summary->context_identity) ||
        !yvex_sha256_update_text(&hash, summary->prefix_token_identity) ||
        !yvex_sha256_update_text(&hash, summary->runtime_prefix_identity) ||
        !yvex_sha256_update_text(&hash, summary->shared_logits_identity) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, summary->prefix_identity);
    return 1;
}

static int readout_lock(yvex_decision_readout_context *context,
                        const char *operation, yvex_error *err)
{
    if (!context || !context->mutex_ready ||
        pthread_mutex_lock(&context->mutex) != 0)
        return readout_refuse(
            err, YVEX_ERR_STATE, operation,
            "readout synchronization is unavailable");
    if (context->busy || context->invalidated) {
        (void)pthread_mutex_unlock(&context->mutex);
        return readout_refuse(
            err, YVEX_ERR_STATE, operation,
            "readout context is busy or invalidated");
    }
    context->busy = 1;
    return YVEX_OK;
}

static void readout_unlock(yvex_decision_readout_context *context)
{
    context->busy = 0;
    (void)pthread_mutex_unlock(&context->mutex);
}

static int readout_source_state_identity(
    const yvex_decision_readout_context *context,
    char output[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    const yvex_runtime_session_view *view =
        yvex_runtime_session_view_get(context->source.session);
    if (output) output[0] = '\0';
    if (!view || !output)
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.state",
            "readout source session state is unavailable");
    if (view->sequence_state)
        return yvex_sequence_state_committed_identity(
            view->sequence_state, output, err);
    return readout_identity_text(
               "yvex.decision-readout.attention-state.v1", output)
               ? YVEX_OK
               : readout_refuse(
                     err, YVEX_ERR_STATE, "runtime.decision-readout.state",
                     "readout source state identity could not seal");
}

int yvex_decision_readout_prefix_prepare(
    yvex_decision_readout_context *context,
    const unsigned int *prefix_tokens, unsigned long long prefix_token_count,
    yvex_decision_readout_prefix **out,
    yvex_decision_readout_prefix_summary *summary, yvex_error *err)
{
    yvex_decision_readout_prefix *prefix = NULL;
    yvex_runtime_decoder_execution_result execution = {0};
    yvex_runtime_logits_row_result row = {0};
    yvex_model_engine_failure failure = {0};
    unsigned long long index, logits_bytes, started;
    int rc;
    if (out) *out = NULL;
    if (summary) memset(summary, 0, sizeof(*summary));
    if (!context || !prefix_tokens || !prefix_token_count || !out || !summary ||
        prefix_token_count > context->options.maximum_prefix_tokens)
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.prefix",
            "one bounded explicit prefix token sequence is required");
    for (index = 0ull; index < prefix_token_count; ++index)
        if (prefix_tokens[index] >= context->summary.vocabulary_size)
            return readout_refuse(
                err, YVEX_ERR_BOUNDS, "runtime.decision-readout.prefix",
                "prefix token exceeds the admitted vocabulary");
    rc = readout_lock(context, "runtime.decision-readout.prefix", err);
    if (rc != YVEX_OK) return rc;
    if (context->prefix_prepared) {
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.prefix",
            "one readout context admits one immutable shared prefix");
        goto done;
    }
    if (!yvex_core_u64_mul(context->summary.vocabulary_size, sizeof(float),
                           &logits_bytes) || logits_bytes > SIZE_MAX) {
        rc = readout_refuse(
            err, YVEX_ERR_BOUNDS, "runtime.decision-readout.prefix",
            "shared vocabulary row exceeds this host");
        goto done;
    }
    prefix = calloc(1u, sizeof(*prefix));
    if (prefix) prefix->logits = malloc((size_t)logits_bytes);
    if (!prefix || !prefix->logits) {
        rc = readout_refuse(
            err, YVEX_ERR_NOMEM, "runtime.decision-readout.prefix",
            "shared prefix allocation failed");
        goto done;
    }
    prefix->vocabulary_size = context->summary.vocabulary_size;
    started = yvex_core_monotonic_ns();
    rc = readout_decoder_logits(
        context, &context->source, prefix_tokens, 0ull, prefix_token_count,
        prefix_token_count - 1ull, prefix->logits, &execution, &row, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_prefix_capture(
            context->source.session, context->options.maximum_prefix_state_bytes,
            &prefix->runtime, &prefix->runtime_summary, &failure, err);
    if (rc == YVEX_OK) {
        yvex_decision_readout_prefix_summary *facts = &prefix->summary;
        facts->schema_version = YVEX_DECISION_READOUT_SCHEMA_V1;
        facts->engine_generation = context->summary.engine_generation;
        facts->prefix_token_count = prefix_token_count;
        facts->committed_sequence_length =
            prefix->runtime_summary.committed_sequence_length;
        facts->shared_state_bytes = prefix->runtime_summary.shared_bytes;
        facts->mapped_state_bytes = prefix->runtime_summary.mapped_bytes;
        facts->logits_bytes = logits_bytes;
        facts->prefix_forward_count = 1ull;
        yvex_runtime_identity_copy(
            facts->runtime_model_identity,
            context->summary.runtime_model_identity);
        yvex_runtime_identity_copy(
            facts->runtime_binding_identity,
            context->summary.runtime_binding_identity);
        yvex_runtime_identity_copy(
            facts->tokenizer_identity, context->summary.tokenizer_identity);
        yvex_runtime_identity_copy(
            facts->context_identity, context->summary.context_identity);
        yvex_runtime_identity_copy(
            facts->runtime_prefix_identity,
            prefix->runtime_summary.prefix_identity);
        if (!readout_token_identity(
                "yvex.decision-readout.prefix-tokens.v1", prefix_tokens,
                prefix_token_count, facts->prefix_token_identity) ||
            !readout_logits_identity(
                prefix->logits, prefix->vocabulary_size,
                facts->shared_logits_identity) ||
            !readout_prefix_identity(&context->summary, facts))
            rc = readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.prefix",
                "shared prefix identities could not seal");
    }
    if (rc == YVEX_OK &&
        prefix->summary.committed_sequence_length != prefix_token_count)
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.prefix",
            "shared prefix state did not commit every prefix token");
    if (rc == YVEX_OK) {
        prefix->preparation_nanoseconds =
            yvex_core_monotonic_ns() - started;
        *summary = prefix->summary;
        *out = prefix;
        prefix = NULL;
        context->prefix_prepared = 1;
        yvex_error_clear(err);
    } else {
        context->invalidated = 1;
    }
done:
    yvex_decision_readout_prefix_close(&prefix);
    readout_unlock(context);
    return rc;
}

static int readout_candidates_validate(
    const yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    const yvex_decision_readout_candidate *candidates,
    unsigned long long candidate_count, unsigned long long *token_count,
    char population_identity[YVEX_SHA256_HEX_CAP], yvex_error *err)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long candidate, prior, token, total = 0ull;
    if (token_count) *token_count = 0ull;
    if (!context || !prefix || !candidates || !candidate_count ||
        !token_count || !population_identity ||
        candidate_count > context->options.maximum_candidate_count)
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.execute",
            "one bounded finite candidate population is required");
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(
            &hash, "yvex.decision-readout.candidate-population.v1") ||
        !yvex_sha256_update_u64(&hash, candidate_count))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "candidate population identity could not begin");
    for (candidate = 0ull; candidate < candidate_count; ++candidate) {
        const yvex_decision_readout_candidate *current =
            &candidates[candidate];
        size_t id_length = current->candidate_id
                               ? strnlen(current->candidate_id,
                                         YVEX_DECISION_READOUT_CANDIDATE_ID_CAP)
                               : 0u;
        if (!id_length ||
            id_length >= YVEX_DECISION_READOUT_CANDIDATE_ID_CAP ||
            !current->token_ids || !current->token_count ||
            current->token_count >
                context->options.maximum_candidate_tokens ||
            prefix->summary.prefix_token_count >
                context->options.context_capacity - current->token_count)
            return readout_refuse(
                err, YVEX_ERR_BOUNDS, "runtime.decision-readout.execute",
                "candidate identity, tokens, or context extent is invalid");
        for (prior = 0ull; prior < candidate; ++prior)
            if (strcmp(current->candidate_id,
                       candidates[prior].candidate_id) == 0)
                return readout_refuse(
                    err, YVEX_ERR_FORMAT, "runtime.decision-readout.execute",
                    "candidate identities must be unique");
        if (!yvex_core_u64_add(total, current->token_count, &total) ||
            !yvex_sha256_update_u64(&hash, candidate) ||
            !yvex_sha256_update_text(&hash, current->candidate_id) ||
            !yvex_sha256_update_u64(&hash, current->token_count))
            return readout_refuse(
                err, YVEX_ERR_BOUNDS, "runtime.decision-readout.execute",
                "candidate population extent overflowed");
        for (token = 0ull; token < current->token_count; ++token) {
            if (current->token_ids[token] >=
                context->summary.vocabulary_size)
                return readout_refuse(
                    err, YVEX_ERR_BOUNDS, "runtime.decision-readout.execute",
                    "candidate token exceeds the admitted vocabulary");
            if (!yvex_sha256_update_u64(&hash, current->token_ids[token]))
                return readout_refuse(
                    err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
                    "candidate population identity could not seal");
        }
    }
    if (!yvex_sha256_final(&hash, digest))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "candidate population identity could not seal");
    yvex_sha256_hex(digest, population_identity);
    *token_count = total;
    return YVEX_OK;
}

static int readout_cancelled(
    const yvex_decision_readout_context *context, yvex_error *err)
{
    if (context->options.cancel_requested &&
        context->options.cancel_requested(context->options.cancel_context))
        return readout_refuse(
            err, YVEX_ERR_CANCELLED, "runtime.decision-readout.execute",
            "finite-candidate readout was cancelled");
    return YVEX_OK;
}

static int readout_candidate_result_identity(
    yvex_decision_readout_candidate_result *result)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!result ||
        !yvex_sha256_update_text(
            &hash, "yvex.decision-readout.candidate-result.v1") ||
        !yvex_sha256_update_text(&hash, result->candidate_id) ||
        !yvex_sha256_update_u64(&hash, result->token_count) ||
        !yvex_sha256_update_text(&hash, result->token_identity) ||
        !readout_update_double(&hash, result->candidate_log_likelihood) ||
        !readout_update_double(&hash, result->mean_token_log_probability) ||
        !readout_update_double(
            &hash, result->relative_candidate_probability) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, result->candidate_result_identity);
    return 1;
}

static int readout_candidate_execute(
    yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    const yvex_decision_readout_candidate *candidate,
    yvex_decision_readout_candidate_result *result,
    unsigned long long *state_bytes, unsigned long long *workspace_bytes,
    unsigned long long *logits_rows, yvex_error *err)
{
    yvex_runtime_session_prefix_summary attached = {0};
    yvex_runtime_decoder_execution_result execution = {0};
    yvex_runtime_logits_row_result row = {0};
    yvex_runtime_session_summary session = {0};
    readout_run run = {0};
    float *next_logits = NULL;
    const float *current_logits = prefix->logits;
    unsigned long long token, logits_bytes;
    int rc;
    if (!yvex_core_u64_mul(prefix->vocabulary_size, sizeof(float),
                           &logits_bytes) || logits_bytes > SIZE_MAX)
        return readout_refuse(
            err, YVEX_ERR_BOUNDS, "runtime.decision-readout.candidate",
            "candidate logits row exceeds this host");
    if (candidate->token_count > 1ull) {
        next_logits = malloc((size_t)logits_bytes);
        if (!next_logits)
            return readout_refuse(
                err, YVEX_ERR_NOMEM, "runtime.decision-readout.candidate",
                "candidate logits row allocation failed");
    }
    rc = readout_run_open(
        context, prefix->runtime, &run, candidate->token_count,
        &attached, err);
    if (rc == YVEX_OK &&
        strcmp(attached.prefix_identity,
               prefix->summary.runtime_prefix_identity) != 0)
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.candidate",
            "candidate branch attached a different shared prefix");
    memset(result, 0, sizeof(*result));
    if (rc == YVEX_OK)
        yvex_core_text_copy(result->candidate_id,
                            sizeof(result->candidate_id),
                            candidate->candidate_id);
    for (token = 0ull; rc == YVEX_OK && token < candidate->token_count;
         ++token) {
        double log_probability = 0.0;
        rc = readout_cancelled(context, err);
        if (rc == YVEX_OK)
            rc = yvex_decision_readout_log_probability(
                current_logits, prefix->vocabulary_size,
                candidate->token_ids[token], &log_probability, err);
        if (rc == YVEX_OK) {
            result->candidate_log_likelihood += log_probability;
            if (token + 1ull < candidate->token_count)
                rc = readout_decoder_logits(
                    context, &run, &candidate->token_ids[token],
                    prefix->summary.prefix_token_count + token, 1ull, 0ull,
                    next_logits, &execution, &row, err);
            else
                rc = readout_decoder_advance(
                    &run, &candidate->token_ids[token],
                    prefix->summary.prefix_token_count + token, 1ull,
                    &execution, err);
        }
        if (rc == YVEX_OK && token + 1ull < candidate->token_count) {
            current_logits = next_logits;
            (*logits_rows)++;
        }
    }
    if (rc == YVEX_OK)
        rc = yvex_runtime_session_summary_copy(run.session, &session, err);
    if (rc == YVEX_OK) {
        result->token_count = candidate->token_count;
        result->teacher_forced_tokens = candidate->token_count;
        result->mean_token_log_probability =
            result->candidate_log_likelihood / (double)candidate->token_count;
        if (!isfinite(result->candidate_log_likelihood) ||
            !isfinite(result->mean_token_log_probability) ||
            !readout_token_identity(
                "yvex.decision-readout.candidate-tokens.v1",
                candidate->token_ids, candidate->token_count,
                result->token_identity))
            rc = readout_refuse(
                err, YVEX_ERR_FORMAT, "runtime.decision-readout.candidate",
                "candidate score or token identity is invalid");
        if (session.sequence_host_state_bytes > *state_bytes)
            *state_bytes = session.sequence_host_state_bytes;
        if (session.workspace_peak_bytes > *workspace_bytes)
            *workspace_bytes = session.workspace_peak_bytes;
        if (session.host_workspace_peak_bytes > *workspace_bytes)
            *workspace_bytes = session.host_workspace_peak_bytes;
    }
    {
        yvex_error primary = err ? *err : (yvex_error){0};
        yvex_error cleanup = {0};
        int close_rc = readout_run_close(&run, &cleanup);
        free(next_logits);
        if (rc == YVEX_OK && close_rc != YVEX_OK) {
            rc = close_rc;
            if (err) *err = cleanup;
        } else if (rc != YVEX_OK && err) {
            *err = primary;
        }
    }
    return rc;
}

static int readout_result_identity(yvex_decision_readout_result *result)
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    unsigned long long index;
    yvex_sha256_init(&hash);
    if (!result || !result->candidates ||
        !yvex_sha256_update_text(&hash, "yvex.decision-readout.result.v1") ||
        !yvex_sha256_update_u64(&hash, result->schema_version) ||
        !yvex_sha256_update_u64(&hash, result->engine_generation) ||
        !yvex_sha256_update_u64(&hash, result->candidate_count) ||
        !yvex_sha256_update_text(&hash, result->runtime_model_identity) ||
        !yvex_sha256_update_text(&hash, result->artifact_identity) ||
        !yvex_sha256_update_text(&hash, result->runtime_binding_identity) ||
        !yvex_sha256_update_text(&hash, result->tokenizer_identity) ||
        !yvex_sha256_update_text(
            &hash, result->readout_implementation_identity) ||
        !yvex_sha256_update_text(&hash, result->prefix_identity) ||
        !yvex_sha256_update_text(
            &hash, result->shared_state_identity_before) ||
        !yvex_sha256_update_text(
            &hash, result->shared_state_identity_after) ||
        !yvex_sha256_update_text(
            &hash, result->candidate_population_identity) ||
        !yvex_sha256_update_text(&hash, result->score_policy_identity) ||
        !yvex_sha256_update_u64(&hash, result->sampling_invocation_count) ||
        !yvex_sha256_update_u64(&hash, result->generated_token_count))
        return 0;
    for (index = 0ull; index < result->candidate_count; ++index)
        if (!yvex_sha256_update_text(
                &hash, result->candidates[index].candidate_result_identity))
            return 0;
    if (!yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, result->result_identity);
    return 1;
}

static int readout_execute_compatible(
    yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    const char *expected_prefix_identity, yvex_error *err)
{
    yvex_model_engine_summary current = {0};
    if (!prefix || !prefix->runtime || !expected_prefix_identity ||
        !yvex_sha256_hex_valid(expected_prefix_identity) ||
        strcmp(expected_prefix_identity, prefix->summary.prefix_identity) != 0 ||
        strcmp(prefix->summary.context_identity,
               context->summary.context_identity) != 0 ||
        strcmp(prefix->summary.runtime_model_identity,
               context->summary.runtime_model_identity) != 0 ||
        strcmp(prefix->summary.runtime_binding_identity,
               context->summary.runtime_binding_identity) != 0 ||
        prefix->summary.engine_generation !=
            context->summary.engine_generation)
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "shared prefix, model, binding, or engine identity is stale");
    if (yvex_model_engine_summary_copy(context->model, &current, err) !=
            YVEX_OK ||
        !current.sealed || !current.valid ||
        current.engine_generation != context->summary.engine_generation ||
        strcmp(current.runtime_model_identity,
               context->summary.runtime_model_identity) != 0 ||
        strcmp(current.runtime_binding_identity,
               context->summary.runtime_binding_identity) != 0)
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "readout engine generation is no longer compatible");
    return YVEX_OK;
}

static int readout_result_initialize(
    const yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    unsigned long long candidate_count, unsigned long long token_count,
    const char *population_identity, yvex_decision_readout_result *result,
    yvex_error *err)
{
    memset(result, 0, sizeof(*result));
    if (candidate_count > SIZE_MAX / sizeof(*result->candidates))
        return readout_refuse(
            err, YVEX_ERR_BOUNDS, "runtime.decision-readout.execute",
            "readout result extent overflowed");
    result->candidates = calloc((size_t)candidate_count,
                                sizeof(*result->candidates));
    if (!result->candidates)
        return readout_refuse(
            err, YVEX_ERR_NOMEM, "runtime.decision-readout.execute",
            "candidate result allocation failed");
    result->schema_version = YVEX_DECISION_READOUT_SCHEMA_V1;
    result->engine_generation = context->summary.engine_generation;
    result->candidate_count = candidate_count;
    result->prefix_token_count = prefix->summary.prefix_token_count;
    result->candidate_token_count = token_count;
    result->prefix_forward_count = prefix->summary.prefix_forward_count;
    result->teacher_forced_forward_count = token_count;
    result->logits_row_count = 1ull;
    result->resident_backbone_count = 1ull;
    result->mapped_model_bytes = context->model_summary.mapped_package_bytes;
    result->prepared_model_bytes = context->model_summary.prepared_bytes;
    result->resident_host_model_bytes =
        context->model_summary.resident_host_bytes;
    result->resident_device_model_bytes =
        context->model_summary.resident_device_bytes;
    result->shared_prefix_state_bytes = prefix->summary.shared_state_bytes;
    result->logits_buffer_bytes = prefix->summary.logits_bytes;
    result->prefix_nanoseconds = prefix->preparation_nanoseconds;
    yvex_runtime_identity_copy(
        result->runtime_model_identity,
        context->summary.runtime_model_identity);
    yvex_runtime_identity_copy(
        result->artifact_identity, context->summary.artifact_identity);
    yvex_runtime_identity_copy(
        result->runtime_binding_identity,
        context->summary.runtime_binding_identity);
    yvex_runtime_identity_copy(
        result->tokenizer_identity, context->summary.tokenizer_identity);
    yvex_runtime_identity_copy(
        result->readout_implementation_identity,
        context->summary.readout_implementation_identity);
    yvex_runtime_identity_copy(
        result->prefix_identity, prefix->summary.prefix_identity);
    yvex_runtime_identity_copy(
        result->candidate_population_identity, population_identity);
    if (!readout_identity_text(
            "yvex.decision-readout.raw-log-likelihood.v1",
            result->score_policy_identity))
        return readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "score-policy identity could not seal");
    return YVEX_OK;
}

int yvex_decision_readout_execute(
    yvex_decision_readout_context *context,
    const yvex_decision_readout_prefix *prefix,
    const char *expected_prefix_identity,
    const yvex_decision_readout_candidate *candidates,
    unsigned long long candidate_count,
    yvex_decision_readout_result *result, yvex_error *err)
{
    yvex_decision_readout_result pending = {0};
    char population_identity[YVEX_SHA256_HEX_CAP] = {0};
    unsigned long long candidate, token_count = 0ull, started;
    int rc;
    if (result) memset(result, 0, sizeof(*result));
    if (!context || !result)
        return readout_refuse(
            err, YVEX_ERR_INVALID_ARG, "runtime.decision-readout.execute",
            "readout context and result storage are required");
    rc = readout_lock(context, "runtime.decision-readout.execute", err);
    if (rc != YVEX_OK) return rc;
    rc = readout_execute_compatible(
        context, prefix, expected_prefix_identity, err);
    if (rc == YVEX_OK)
        rc = readout_candidates_validate(
            context, prefix, candidates, candidate_count, &token_count,
            population_identity, err);
    if (rc == YVEX_OK)
        rc = readout_result_initialize(
            context, prefix, candidate_count, token_count,
            population_identity, &pending, err);
    if (rc == YVEX_OK)
        rc = readout_source_state_identity(
            context, pending.shared_state_identity_before, err);
    started = yvex_core_monotonic_ns();
    for (candidate = 0ull; rc == YVEX_OK && candidate < candidate_count;
         ++candidate) {
        rc = readout_cancelled(context, err);
        if (rc == YVEX_OK)
            rc = readout_candidate_execute(
                context, prefix, &candidates[candidate],
                &pending.candidates[candidate],
                &pending.peak_candidate_state_bytes,
                &pending.peak_workspace_bytes,
                &pending.logits_row_count, err);
    }
    pending.candidate_nanoseconds = yvex_core_monotonic_ns() - started;
    if (rc == YVEX_OK)
        rc = yvex_decision_readout_relative_distribution(
            pending.candidates, pending.candidate_count, err);
    for (candidate = 0ull; rc == YVEX_OK && candidate < candidate_count;
         ++candidate)
        if (!readout_candidate_result_identity(
                &pending.candidates[candidate]))
            rc = readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
                "candidate result identity could not seal");
    if (rc == YVEX_OK)
        rc = readout_source_state_identity(
            context, pending.shared_state_identity_after, err);
    if (rc == YVEX_OK &&
        strcmp(pending.shared_state_identity_before,
               pending.shared_state_identity_after) != 0)
        rc = readout_refuse(
            err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
            "candidate execution mutated the shared source state");
    if (rc == YVEX_OK) {
        pending.relative_distribution_available = 1;
        pending.calibrated = 0;
        pending.total_nanoseconds =
            pending.prefix_nanoseconds + pending.candidate_nanoseconds;
        if (!readout_result_identity(&pending))
            rc = readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.execute",
                "readout result identity could not seal");
    }
    if (rc == YVEX_OK) {
        pending.completed = 1;
        *result = pending;
        memset(&pending, 0, sizeof(pending));
        yvex_error_clear(err);
    }
    yvex_decision_readout_result_release(&pending);
    readout_unlock(context);
    return rc;
}

void yvex_decision_readout_result_release(
    yvex_decision_readout_result *result)
{
    if (!result) return;
    free(result->candidates);
    memset(result, 0, sizeof(*result));
}

void yvex_decision_readout_prefix_close(
    yvex_decision_readout_prefix **owner)
{
    yvex_decision_readout_prefix *prefix = owner ? *owner : NULL;
    if (!prefix) return;
    *owner = NULL;
    yvex_runtime_session_prefix_close(&prefix->runtime);
    free(prefix->logits);
    memset(prefix, 0, sizeof(*prefix));
    free(prefix);
}

int yvex_decision_readout_context_close(
    yvex_decision_readout_context **owner, yvex_error *err)
{
    yvex_decision_readout_context *context = owner ? *owner : NULL;
    int locked = 0;
    int rc;
    if (!owner || !context) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (context->mutex_ready) {
        if (pthread_mutex_lock(&context->mutex) != 0)
            return readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.close",
                "readout synchronization is unavailable");
        locked = 1;
        if (context->busy) {
            (void)pthread_mutex_unlock(&context->mutex);
            return readout_refuse(
                err, YVEX_ERR_STATE, "runtime.decision-readout.close",
                "idle readout context is required for close");
        }
    }
    rc = readout_run_close(&context->source, err);
    if (rc != YVEX_OK) {
        if (locked) (void)pthread_mutex_unlock(&context->mutex);
        return rc;
    }
    if (locked) {
        (void)pthread_mutex_unlock(&context->mutex);
        (void)pthread_mutex_destroy(&context->mutex);
    }
    *owner = NULL;
    memset(context, 0, sizeof(*context));
    free(context);
    yvex_error_clear(err);
    return YVEX_OK;
}
