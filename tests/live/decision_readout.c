/* Exact artifact-backed finite-candidate scoring with no sampler or generation. */
#include <yvex/internal/decision_readout.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/logits.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/runtime_capacity.h>
#include <yvex/internal/graph_state.h>
#include <yvex/tokenizer.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One harness and numerical contract for recurrent CPU and hybrid CUDA. */
static yvex_backend_kind live_backend = YVEX_BACKEND_KIND_CPU;
static unsigned long long live_vocabulary = 32768ull;

typedef struct {
    unsigned long long calls, cancel_on_call;
} live_cancel;

typedef struct {
    yvex_decision_readout_context *context;
    yvex_decision_readout_prefix *prefix;
    yvex_decision_readout_context_summary context_summary;
    yvex_decision_readout_prefix_summary prefix_summary;
} live_readout;

static long double live_reference_log_probability(
    const float *logits, unsigned long long count, unsigned int token)
{
    long double maximum = (long double)logits[0], total = 0.0L;
    unsigned long long index;
    for (index = 1ull; index < count; ++index)
        if ((long double)logits[index] > maximum)
            maximum = (long double)logits[index];
    for (index = 0ull; index < count; ++index)
        total += expl((long double)logits[index] - maximum);
    return (long double)logits[token] - maximum - logl(total);
}

static int live_oracle_input_identity(
    unsigned int token, unsigned long long position,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(
            &hash, "yvex.test.decision-readout.oracle-input.v1") ||
        !yvex_sha256_update_u64(&hash, token) ||
        !yvex_sha256_update_u64(&hash, position) ||
        !yvex_sha256_final(&hash, digest)) return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int live_oracle_profile(
    yvex_model_engine *model, yvex_runtime_execution_session *session,
    yvex_execution_workload_profile *workload,
    yvex_runtime_execution_profile *profile, yvex_error *err)
{
    yvex_runtime_execution_profile_derivation derivation = {0};
    int rc;
    yvex_runtime_capacity_options options = {
        .backend = live_backend,
        .mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY,
        .workload_kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY,
        .context_capacity = 8ull, .prefill_chunk_tokens = 1ull,
        .evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION,
        .sampling_requirement = YVEX_EXECUTION_SAMPLING_NOT_INVOKED};
    yvex_runtime_capacity capacity = {0};
    yvex_graph_attention_capacity_plan *attention = NULL;
    yvex_model_engine_failure failure = {0};
    const yvex_runtime_session_view *view = yvex_runtime_session_view_get(session);
    rc = yvex_runtime_capacity_derive(model, session, &options, &capacity, &attention, err);
    yvex_graph_attention_capacity_plan_close(&attention);
    if (rc == YVEX_OK && view && view->attention_state_provider)
        rc = yvex_runtime_session_configure_persistent_pages(
            session, &capacity.capacity_plan, &failure, err);
    if (rc != YVEX_OK) return rc;
    *workload = capacity.workload_profile;
    derivation.schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1;
    derivation.model = model;
    derivation.session = session;
    derivation.workload = workload;
    derivation.backend = live_backend;
    derivation.generation_mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY;
    derivation.evidence = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    derivation.sampling_requirement = YVEX_EXECUTION_SAMPLING_NOT_INVOKED;
    rc = yvex_runtime_execution_profile_derive(
        &derivation, profile, err);
    if (rc == YVEX_OK && live_backend == YVEX_BACKEND_KIND_CPU &&
        (profile->execution_class != YVEX_EXECUTION_CLASS_PORTABLE_REFERENCE ||
         profile->attention_resolution != YVEX_EXECUTION_RESOLUTION_EXACT ||
         profile->moe_resolution != YVEX_EXECUTION_RESOLUTION_EXACT ||
         profile->sampling_resolution != YVEX_EXECUTION_RESOLUTION_EXACT)) {
        yvex_error_set(err, YVEX_ERR_STATE, "decision-readout.live",
                       "Mamba CPU execution-profile posture changed");
        return YVEX_ERR_STATE;
    }
    if (rc == YVEX_OK && live_backend == YVEX_BACKEND_KIND_CUDA &&
        (profile->execution_class != YVEX_EXECUTION_CLASS_DEVICE_NATIVE ||
         profile->attention_resolution != YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED ||
         profile->moe_resolution != YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED ||
         profile->sampling_resolution != YVEX_EXECUTION_RESOLUTION_EXACT))
        return YVEX_ERR_STATE;
    return rc;
}

static int live_oracle_token(
    yvex_runtime_decoder_execution_context *decoder,
    yvex_runtime_logits_context *logits, unsigned int token,
    unsigned long long position, float *values,
    unsigned long long value_count, yvex_error *err)
{
    yvex_runtime_decoder_execution_request request = {0};
    yvex_runtime_decoder_execution_result execution = {0};
    yvex_runtime_logits_source source = {0};
    yvex_runtime_logits_row_result row = {0};
    char identity[YVEX_SHA256_HEX_CAP];
    int rc;
    if (!live_oracle_input_identity(token, position, identity))
        return YVEX_ERR_STATE;
    request.token_ids = &token;
    request.token_start = position;
    request.token_count = 1ull;
    request.input_identity = identity;
    rc = yvex_runtime_decoder_execution_execute(
        decoder, &request, &execution, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_source_from_decoder(
            logits, &source, &execution,
            position ? YVEX_LOGITS_SOURCE_DECODE
                     : YVEX_LOGITS_SOURCE_PREFILL,
            0ull, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_project(
            logits, &source, live_backend, values, value_count,
            &row, err);
    return rc;
}

static int live_independent_oracle(
    yvex_model_engine *model, const unsigned int *prefix_tokens,
    const yvex_decision_readout_candidate *candidate,
    long double *expected, yvex_error *err)
{
    yvex_runtime_session_open_request session_request = {0};
    yvex_runtime_decoder_execution_options decoder_options = {0};
    yvex_runtime_logits_options logits_options = {0};
    yvex_model_engine_failure failure = {0};
    yvex_runtime_execution_session *session = NULL;
    yvex_runtime_decoder_execution_context *decoder = NULL;
    yvex_runtime_logits_context *logits = NULL;
    yvex_execution_workload_profile workload = {0};
    yvex_runtime_execution_profile profile = {0};
    float *values = NULL;
    unsigned long long token;
    int rc;
    if (expected) *expected = 0.0L;
    if (!model || !prefix_tokens || !candidate || !candidate->token_count ||
        !expected) return YVEX_ERR_INVALID_ARG;
    session_request.backend = live_backend;
    rc = yvex_runtime_session_open(
        &session, model, &session_request, &failure, err);
    if (rc == YVEX_OK)
        rc = live_oracle_profile(
            model, session, &workload, &profile, err);
    decoder_options.context_capacity = 8ull;
    decoder_options.token_capacity = 1ull;
    decoder_options.execution_profile = &profile;
    logits_options.maximum_rows = 1ull;
    logits_options.evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    logits_options.execution_profile = &profile;
    if (rc == YVEX_OK)
        rc = yvex_runtime_decoder_execution_context_open(
            &decoder, model, session, &decoder_options, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_context_open_program(
            &logits, model, session, &logits_options, err);
    if (rc == YVEX_OK) {
        values = malloc((size_t)live_vocabulary * sizeof(*values));
        if (!values) {
            yvex_error_set(
                err, YVEX_ERR_NOMEM, "test.decision-readout.oracle",
                "independent logits buffer allocation failed");
            rc = YVEX_ERR_NOMEM;
        }
    }
    if (rc == YVEX_OK)
        rc = live_oracle_token(
            decoder, logits, prefix_tokens[0], 0ull, values, live_vocabulary, err);
    if (rc == YVEX_OK) {
        yvex_runtime_session_committed_state_summary state = {0};
        rc = yvex_runtime_session_committed_state_summary_copy(session, &state, err);
        if (rc == YVEX_OK &&
            (!state.recurrent_present || state.draft_attention_present ||
             state.target_attention_present != (live_backend == YVEX_BACKEND_KIND_CUDA)))
            rc = YVEX_ERR_STATE;
        if (rc == YVEX_OK)
            printf("replay_state domains=%llu attention=%d recurrent=%d kernel=%s "
                   "attention_resolution=%u moe_resolution=%u identity=%s\n",
                   state.active_domain_count, state.target_attention_present,
                   state.recurrent_present, profile.kernel_bundle_identity,
                   (unsigned int)profile.attention_resolution,
                   (unsigned int)profile.moe_resolution, state.identity);
    }
    for (token = 0ull; rc == YVEX_OK && token < candidate->token_count;
         ++token) {
        *expected += live_reference_log_probability(
            values, live_vocabulary, candidate->token_ids[token]);
        if (token + 1ull < candidate->token_count)
            rc = live_oracle_token(
                decoder, logits, candidate->token_ids[token], token + 1ull,
                values, live_vocabulary, err);
    }
    free(values);
    (void)yvex_runtime_logits_context_close(&logits, NULL);
    (void)yvex_runtime_decoder_execution_context_close(&decoder, NULL);
    (void)yvex_runtime_session_close(&session, NULL);
    return rc;
}

static int live_cancel_requested(void *opaque)
{
    live_cancel *cancel = opaque;
    cancel->calls++;
    return cancel->calls >= cancel->cancel_on_call;
}

static int live_fail(const char *stage, const yvex_error *err)
{
    fprintf(stderr, "decision readout stage=%s where=%s reason=%s\n", stage,
            err ? yvex_error_where(err) : "test.decision-readout",
            err ? yvex_error_message(err) : "qualification invariant failed");
    return 0;
}

static yvex_decision_readout_options live_options(
    const yvex_model_engine_summary *model,
    const yvex_tokenizer_plan_summary *tokenizer, live_cancel *cancel)
{
    yvex_decision_readout_options options = {0};
    options.schema_version = YVEX_DECISION_READOUT_SCHEMA_V1;
    options.backend = live_backend;
    options.score_policy = YVEX_DECISION_READOUT_SCORE_LOG_LIKELIHOOD;
    options.context_capacity = 8ull;
    options.maximum_prefix_tokens = 2ull;
    options.maximum_candidate_count = 8ull;
    options.maximum_candidate_tokens = 6ull;
    options.maximum_prefix_state_bytes = 1ull << 30u;
    options.maximum_host_bytes = 96ull << 30u;
    options.expected_engine_generation = model->engine_generation;
    options.expected_runtime_model_identity = model->runtime_model_identity;
    options.expected_runtime_binding_identity =
        model->runtime_binding_identity;
    options.expected_tokenizer_identity = tokenizer->tokenizer_plan_identity;
    options.cancel_requested = live_cancel_requested;
    options.cancel_context = cancel;
    return options;
}

static int live_readout_open(
    live_readout *run, yvex_model_engine *model,
    const yvex_model_engine_summary *model_summary,
    const yvex_tokenizer_plan_summary *tokenizer, live_cancel *cancel,
    const unsigned int *prefix_tokens, unsigned long long prefix_count,
    yvex_error *err)
{
    yvex_decision_readout_options options =
        live_options(model_summary, tokenizer, cancel);
    int rc = yvex_decision_readout_context_open(
        &run->context, model, &options, err);
    if (rc == YVEX_OK)
        rc = yvex_decision_readout_context_summary_copy(
            run->context, &run->context_summary, err);
    if (rc == YVEX_OK)
        rc = yvex_decision_readout_prefix_prepare(
            run->context, prefix_tokens, prefix_count, &run->prefix,
            &run->prefix_summary, err);
    return rc;
}

static int live_readout_close(live_readout *run, yvex_error *err)
{
    int rc;
    yvex_decision_readout_prefix_close(&run->prefix);
    rc = yvex_decision_readout_context_close(&run->context, err);
    memset(run, 0, sizeof(*run));
    return rc;
}

static const yvex_decision_readout_candidate_result *live_candidate_find(
    const yvex_decision_readout_result *result, const char *candidate_id)
{
    unsigned long long index;
    for (index = 0ull; index < result->candidate_count; ++index)
        if (strcmp(result->candidates[index].candidate_id, candidate_id) == 0)
            return &result->candidates[index];
    return NULL;
}

static int live_results_equal(
    const yvex_decision_readout_result *left,
    const yvex_decision_readout_result *right, double tolerance)
{
    unsigned long long index;
    if (!left->completed || !right->completed ||
        left->candidate_count != right->candidate_count)
        return 0;
    for (index = 0ull; index < left->candidate_count; ++index) {
        const yvex_decision_readout_candidate_result *candidate =
            &left->candidates[index];
        const yvex_decision_readout_candidate_result *peer =
            live_candidate_find(right, candidate->candidate_id);
        if (!peer || peer->token_count != candidate->token_count ||
            fabs(peer->candidate_log_likelihood -
                 candidate->candidate_log_likelihood) > tolerance ||
            fabs(peer->mean_token_log_probability -
                 candidate->mean_token_log_probability) > tolerance ||
            fabs(peer->relative_candidate_probability -
                 candidate->relative_candidate_probability) > tolerance)
            return 0;
    }
    return 1;
}

static double live_results_max_abs(
    const yvex_decision_readout_result *left,
    const yvex_decision_readout_result *right)
{
    double maximum = 0.0;
    unsigned long long index;
    for (index = 0ull; index < left->candidate_count; ++index) {
        const yvex_decision_readout_candidate_result *candidate =
            &left->candidates[index];
        const yvex_decision_readout_candidate_result *peer =
            live_candidate_find(right, candidate->candidate_id);
        double difference = peer
                                ? fabs(peer->candidate_log_likelihood -
                                       candidate->candidate_log_likelihood)
                                : INFINITY;
        if (difference > maximum) maximum = difference;
    }
    return maximum;
}

static int live_replay_candidates(
    yvex_model_engine *model, const yvex_model_engine_summary *model_summary,
    const yvex_tokenizer_plan_summary *tokenizer, live_cancel *cancel,
    const unsigned int *prefix_tokens,
    const yvex_decision_readout_candidate *candidates,
    unsigned long long candidate_count,
    const yvex_decision_readout_result *shared, double *maximum_difference,
    yvex_error *err)
{
    unsigned long long index;
    (void)model_summary;
    (void)tokenizer;
    (void)cancel;
    if (maximum_difference) *maximum_difference = 0.0;
    for (index = 0ull; index < candidate_count; ++index) {
        long double replay = 0.0L;
        const yvex_decision_readout_candidate_result *expected =
            live_candidate_find(shared, candidates[index].candidate_id);
        int rc = live_independent_oracle(
            model, prefix_tokens, &candidates[index], &replay, err);
        if (rc == YVEX_OK &&
            (!expected || fabsl(replay -
                  (long double)expected->candidate_log_likelihood) > 1e-12L)) {
            yvex_error_set(err, YVEX_ERR_STATE,
                           "test.decision-readout.replay",
                           "shared-prefix and full-prefix replay scores differ");
            rc = YVEX_ERR_STATE;
        }
        if (rc == YVEX_OK && maximum_difference) {
            double difference = (double)fabsl(replay -
                (long double)expected->candidate_log_likelihood);
            if (difference > *maximum_difference)
                *maximum_difference = difference;
        }
        if (rc != YVEX_OK) return rc;
    }
    return YVEX_OK;
}

static int live_negative_controls(
    live_readout *run, const yvex_decision_readout_candidate *candidates,
    unsigned int vocabulary_size, yvex_error *err)
{
    yvex_decision_readout_candidate invalid[2];
    yvex_decision_readout_result result = {0};
    unsigned int overflow_tokens[7] = {3u, 4u, 5u, 6u, 7u, 8u, 9u};
    unsigned int invalid_token = vocabulary_size;
    char stale[YVEX_SHA256_HEX_CAP];
    int ok = 1;

    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            NULL, 0ull, &result, err) != YVEX_ERR_INVALID_ARG)
        ok = 0;
    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            candidates, 9ull, &result, err) != YVEX_ERR_INVALID_ARG)
        ok = 0;
    invalid[0] = candidates[0];
    invalid[1] = candidates[1];
    invalid[1].candidate_id = invalid[0].candidate_id;
    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            invalid, 2ull, &result, err) != YVEX_ERR_FORMAT)
        ok = 0;
    invalid[0] = (yvex_decision_readout_candidate){
        .candidate_id = "empty", .token_ids = overflow_tokens};
    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            invalid, 1ull, &result, err) != YVEX_ERR_BOUNDS)
        ok = 0;
    invalid[0] = (yvex_decision_readout_candidate){
        .candidate_id = "invalid-token", .token_ids = &invalid_token,
        .token_count = 1ull};
    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            invalid, 1ull, &result, err) != YVEX_ERR_BOUNDS)
        ok = 0;
    invalid[0] = (yvex_decision_readout_candidate){
        .candidate_id = "overflow", .token_ids = overflow_tokens,
        .token_count = 7ull};
    if (yvex_decision_readout_execute(
            run->context, run->prefix, run->prefix_summary.prefix_identity,
            invalid, 1ull, &result, err) != YVEX_ERR_BOUNDS)
        ok = 0;
    memcpy(stale, run->prefix_summary.prefix_identity, sizeof(stale));
    stale[0] = stale[0] == '0' ? '1' : '0';
    if (yvex_decision_readout_execute(
            run->context, run->prefix, stale, candidates, 1ull,
            &result, err) != YVEX_ERR_STATE)
        ok = 0;
    yvex_decision_readout_result_release(&result);
    return ok ? YVEX_OK : YVEX_ERR_STATE;
}

static int live_context_refusals(
    yvex_model_engine *model, const yvex_model_engine_summary *summary,
    const yvex_tokenizer_plan_summary *tokenizer, live_cancel *cancel,
    yvex_error *err)
{
    yvex_decision_readout_context *context = NULL;
    yvex_decision_readout_options options =
        live_options(summary, tokenizer, cancel);
    char binding[YVEX_SHA256_HEX_CAP];
    int ok = 1;
    options.expected_engine_generation++;
    if (yvex_decision_readout_context_open(
            &context, model, &options, err) != YVEX_ERR_STATE || context)
        ok = 0;
    options = live_options(summary, tokenizer, cancel);
    memcpy(binding, summary->runtime_binding_identity, sizeof(binding));
    binding[0] = binding[0] == '0' ? '1' : '0';
    options.expected_runtime_binding_identity = binding;
    if (yvex_decision_readout_context_open(
            &context, model, &options, err) != YVEX_ERR_STATE || context)
        ok = 0;
    options = live_options(summary, tokenizer, cancel);
    options.score_policy = (yvex_decision_readout_score_policy)99;
    if (yvex_decision_readout_context_open(
            &context, model, &options, err) != YVEX_ERR_INVALID_ARG || context)
        ok = 0;
    return ok ? YVEX_OK : YVEX_ERR_STATE;
}

static int live_cleanup_retry(live_readout *run,
    const yvex_decision_readout_candidate *candidate, yvex_error *err)
{
    yvex_decision_readout_result result = {0};
    int rc, ok;
    if (setenv("YVEX_TEST_RUNTIME_SESSION_CLEANUP_FAILURE", "1", 1) != 0)
        return YVEX_ERR_IO;
    rc = yvex_decision_readout_execute(run->context, run->prefix,
        run->prefix_summary.prefix_identity, candidate, 1ull, &result, err);
    ok = rc == YVEX_ERR_STATE && !result.completed && !result.candidates &&
         yvex_decision_readout_execute(run->context, run->prefix,
            run->prefix_summary.prefix_identity, candidate, 1ull, &result, err) ==
                YVEX_ERR_STATE &&
         yvex_decision_readout_context_close(&run->context, err) == YVEX_ERR_STATE &&
         run->context != NULL;
    (void)unsetenv("YVEX_TEST_RUNTIME_SESSION_CLEANUP_FAILURE");
    rc = yvex_decision_readout_context_close(&run->context, err);
    yvex_decision_readout_result_release(&result);
    if (!ok || rc != YVEX_OK || run->context) {
        yvex_error_set(err, YVEX_ERR_STATE, "test.decision-readout.cleanup",
            "failed candidate cleanup lost ownership or published a result");
        return YVEX_ERR_STATE;
    }
    printf("cleanup retained=true publication=false reuse=refused retry=closed\n");
    return YVEX_OK;
}

int main(int argc, char **argv)
{
    const unsigned int prefix_tokens[] = {1u};
    const unsigned int option_a[] = {3u};
    const unsigned int option_b[] = {4u};
    const unsigned int option_ab[] = {3u, 4u};
    const yvex_decision_readout_candidate candidates[] = {
        {.candidate_id = "option-a", .token_ids = option_a, .token_count = 1ull},
        {.candidate_id = "option-b", .token_ids = option_b, .token_count = 1ull},
        {.candidate_id = "option-ab", .token_ids = option_ab, .token_count = 2ull},
        {.candidate_id = "option-a-alias", .token_ids = option_a, .token_count = 1ull}};
    const yvex_decision_readout_candidate permuted[] = {
        candidates[2], candidates[0], candidates[3], candidates[1]};
    yvex_model_engine_open_request model_request = {0};
    yvex_model_engine_failure failure = {0};
    yvex_model_engine_summary model_summary = {0};
    yvex_model_engine *model = NULL;
    const yvex_model_engine_view *view;
    const yvex_tokenizer_plan_summary *tokenizer;
    yvex_decision_readout_result canonical = {0}, reordered = {0};
    yvex_decision_readout_result cancelled = {0}, retry = {0};
    live_readout run = {0};
    live_cancel cancel = {.cancel_on_call = ~0ull};
    long double oracle_expected = 0.0L, oracle_max_abs = 0.0L;
    double oracle_observed = 0.0;
    double order_max_abs = 0.0, replay_max_abs = 0.0;
    unsigned long long cancellation_calls = 0ull;
    int cancellation_status = YVEX_OK;
    yvex_error err;
    int rc = YVEX_OK, passed = 0;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s ARTIFACT BINDING TARGET [cuda]\n", argv[0]);
        return 2;
    }
    if (argc == 5) {
        if (strcmp(argv[4], "cuda")) return 2;
        live_backend = YVEX_BACKEND_KIND_CUDA;
        live_vocabulary = 248320ull;
    }
    model_request.artifact_path = argv[1];
    model_request.runtime_binding_path = argv[2];
    model_request.target_id = argv[3];
    model_request.residency_backend = live_backend;
    rc = yvex_model_engine_open(&model, &model_request, &failure, &err);
    if (rc == YVEX_OK)
        rc = yvex_model_engine_summary_copy(model, &model_summary, &err);
    view = rc == YVEX_OK ? yvex_model_engine_view_get(model) : NULL;
    tokenizer = view ? yvex_tokenizer_plan_summary_get(view->tokenizer) : NULL;
    if (rc == YVEX_OK && live_backend == YVEX_BACKEND_KIND_CPU &&
        (!tokenizer || tokenizer->vocabulary_size != 32768ull ||
         !tokenizer->bos_present || tokenizer->bos_token_id != 1u ||
         !tokenizer->eos_present || tokenizer->eos_token_id != 2u ||
         tokenizer->pad_present)) {
        yvex_error_set(&err, YVEX_ERR_FORMAT, "test.decision-readout.tokenizer",
                       "exact Mamba2 tokenizer policy is incompatible");
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK && live_backend == YVEX_BACKEND_KIND_CUDA &&
        (!tokenizer || tokenizer->vocabulary_size != 248077ull ||
         strcmp(model_summary.artifact_identity,
            "1fce07008eaa78e04eedd1a031144f48eb6af617f2b5c508811ba91dca7e00f1"))) {
        yvex_error_set(&err, YVEX_ERR_FORMAT, "test.decision-readout.tokenizer",
                       "exact Qwen artifact/tokenizer domain is incompatible");
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK)
        rc = live_context_refusals(
            model, &model_summary, tokenizer, &cancel, &err);
    if (rc == YVEX_OK)
        rc = live_readout_open(
            &run, model, &model_summary, tokenizer, &cancel,
            prefix_tokens, 1ull, &err);
    if (rc == YVEX_OK)
        rc = live_negative_controls(
            &run, candidates, (unsigned int)tokenizer->vocabulary_size, &err);
    if (rc == YVEX_OK) {
        cancel.calls = 0ull;
        rc = yvex_decision_readout_execute(
            run.context, run.prefix, run.prefix_summary.prefix_identity,
            candidates, 4ull, &canonical, &err);
    }
    if (rc == YVEX_OK) {
        const yvex_decision_readout_candidate_result *observed =
            live_candidate_find(&canonical, "option-ab");
        rc = live_independent_oracle(
            model, prefix_tokens, &candidates[2], &oracle_expected, &err);
        if (rc == YVEX_OK && observed) {
            oracle_observed = observed->candidate_log_likelihood;
            oracle_max_abs =
                fabsl((long double)oracle_observed - oracle_expected);
        }
        if (rc == YVEX_OK &&
            (!observed || oracle_max_abs > 1e-12L)) {
            yvex_error_set(
                &err, YVEX_ERR_STATE, "test.decision-readout.oracle",
                "real candidate likelihood differs from independent logits oracle");
            rc = YVEX_ERR_STATE;
        }
    }
    if (rc == YVEX_OK) {
        cancel.calls = 0ull;
        rc = yvex_decision_readout_execute(
            run.context, run.prefix, run.prefix_summary.prefix_identity,
            permuted, 4ull, &reordered, &err);
    }
    if (rc == YVEX_OK &&
        (!live_results_equal(&canonical, &reordered, 1e-12) ||
         canonical.candidates[0].candidate_log_likelihood !=
             canonical.candidates[3].candidate_log_likelihood)) {
        yvex_error_set(&err, YVEX_ERR_STATE, "test.decision-readout.order",
                       "candidate order or opaque aliases changed scores");
        rc = YVEX_ERR_STATE;
    }
    if (rc == YVEX_OK)
        order_max_abs = live_results_max_abs(&canonical, &reordered);
    if (rc == YVEX_OK) {
        cancel.calls = 0ull;
        rc = live_replay_candidates(
            model, &model_summary, tokenizer, &cancel, prefix_tokens,
            candidates, 4ull, &canonical, &replay_max_abs, &err);
    }
    if (rc == YVEX_OK) {
        cancel.calls = 0ull;
        cancel.cancel_on_call = 3ull;
        cancellation_status = yvex_decision_readout_execute(
            run.context, run.prefix, run.prefix_summary.prefix_identity,
            candidates, 4ull, &cancelled, &err);
        cancellation_calls = cancel.calls;
        rc = cancellation_status;
        if (cancellation_status == YVEX_ERR_CANCELLED && !cancelled.completed &&
            !cancelled.candidates && cancel.calls >= 3ull) {
            yvex_error_clear(&err);
            rc = YVEX_OK;
        }
    }
    if (rc == YVEX_OK) {
        cancel.calls = 0ull;
        cancel.cancel_on_call = ~0ull;
        rc = yvex_decision_readout_execute(
            run.context, run.prefix, run.prefix_summary.prefix_identity,
            candidates, 4ull, &retry, &err);
    }
    if (rc == YVEX_OK) rc = live_cleanup_retry(&run, &candidates[0], &err);
    passed = rc == YVEX_OK && live_results_equal(&canonical, &retry, 1e-12) &&
             canonical.completed && canonical.relative_distribution_available &&
             !canonical.calibrated && canonical.sampling_invocation_count == 0ull &&
             canonical.generated_token_count == 0ull &&
             canonical.resident_backbone_count == 1ull &&
             canonical.prefix_forward_count == 1ull &&
             canonical.teacher_forced_forward_count == 5ull &&
             canonical.peak_candidate_state_bytes > 0ull &&
             canonical.logits_row_count == 2ull &&
             strcmp(canonical.shared_state_identity_before,
                    canonical.shared_state_identity_after) == 0 &&
             yvex_sha256_hex_valid(canonical.result_identity);
    if (!passed && rc == YVEX_OK) rc = YVEX_ERR_STATE;
    if (passed) {
        unsigned long long index;
        printf("decision-readout model=%s artifact=%s binding=%s engine=%llu "
               "tokenizer=%s readout=%s prefix=%s prefix_tokens=%llu "
               "candidates=%llu candidate_tokens=%llu prefix_forwards=%llu "
               "teacher_forced=%llu logits_rows=%llu sampling=%llu generated=%llu "
               "backbones=%llu prefix_ns=%llu candidate_ns=%llu total_ns=%llu "
               "mapped_model_bytes=%llu prepared_model_bytes=%llu "
               "resident_host_model_bytes=%llu resident_device_model_bytes=%llu "
               "shared_state_bytes=%llu branch_state_bytes=%llu "
               "workspace_bytes=%llu result=%s calibrated=false\n",
               canonical.runtime_model_identity, canonical.artifact_identity,
               canonical.runtime_binding_identity, canonical.engine_generation,
               canonical.tokenizer_identity,
               canonical.readout_implementation_identity,
               canonical.prefix_identity, canonical.prefix_token_count,
               canonical.candidate_count, canonical.candidate_token_count,
               canonical.prefix_forward_count,
               canonical.teacher_forced_forward_count,
               canonical.logits_row_count,
               canonical.sampling_invocation_count,
               canonical.generated_token_count,
               canonical.resident_backbone_count,
               canonical.prefix_nanoseconds, canonical.candidate_nanoseconds,
               canonical.total_nanoseconds, canonical.mapped_model_bytes,
               canonical.prepared_model_bytes,
               canonical.resident_host_model_bytes,
               canonical.resident_device_model_bytes,
               canonical.shared_prefix_state_bytes,
               canonical.peak_candidate_state_bytes,
               canonical.peak_workspace_bytes, canonical.result_identity);
        printf("decision-readout-oracle candidate=option-ab expected=%.21Lg "
               "observed=%.17g max_abs=%.21Lg tolerance=1e-12 "
               "authority=independent-long-double-logsumexp\n",
               oracle_expected, oracle_observed, oracle_max_abs);
        printf("decision-readout-controls order_max_abs=%.17g "
               "replay_max_abs=%.17g tolerance=1e-12 cancellation_status=%d "
               "cancellation_calls=%llu partial_published=false retry_equal=true "
               "shared_before=%s shared_after=%s\n",
               order_max_abs, replay_max_abs, cancellation_status,
               cancellation_calls, canonical.shared_state_identity_before,
               canonical.shared_state_identity_after);
        for (index = 0ull; index < canonical.candidate_count; ++index)
            printf("candidate id=%s tokens=%llu token_identity=%s log_likelihood=%.17g "
                   "mean_token_log_probability=%.17g relative_distribution=%.17g\n",
                   canonical.candidates[index].candidate_id,
                   canonical.candidates[index].token_count,
                   canonical.candidates[index].token_identity,
                   canonical.candidates[index].candidate_log_likelihood,
                   canonical.candidates[index].mean_token_log_probability,
                   canonical.candidates[index].relative_candidate_probability);
    } else {
        (void)live_fail("qualification", &err);
    }
    yvex_decision_readout_result_release(&retry);
    yvex_decision_readout_result_release(&cancelled);
    yvex_decision_readout_result_release(&reordered);
    yvex_decision_readout_result_release(&canonical);
    (void)live_readout_close(&run, NULL);
    yvex_model_engine_close(&model);
    return passed ? 0 : 1;
}
