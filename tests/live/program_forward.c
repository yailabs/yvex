/* Exact artifact-backed token programs: fresh-session replay and publication
 * lifetime. This is internal preservation evidence, not upstream conformance. */
#include <yvex/internal/backend.h>
#include <yvex/internal/compiler.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/sequence_state.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_runtime_execution_session *session;
    yvex_runtime_decoder_execution_context *runner;
    float *values;
    unsigned long long width;
    yvex_runtime_execution_profile profile;
    int cancel;
} forward_run;

static int forward_cancel(void *opaque)
{
    return ((forward_run *)opaque)->cancel;
}

static int forward_refuse(yvex_error *err, const char *reason)
{
    yvex_error_set(err, YVEX_ERR_STATE, "test.program-forward", reason);
    return YVEX_ERR_STATE;
}

static int forward_close(forward_run *run, yvex_error *err)
{
    int rc = yvex_runtime_decoder_execution_context_close(&run->runner, err);
    if (rc == YVEX_OK) rc = yvex_runtime_session_close(&run->session, err);
    if (rc == YVEX_OK) { free(run->values); run->values = NULL; }
    return rc;
}

static int forward_profile(forward_run *run, yvex_error *err)
{
    const yvex_runtime_session_view *view = yvex_runtime_session_view_get(run->session);
    yvex_runtime_session_summary session;
    yvex_backend_cuda_attention_graph_summary cuda = {0};
    yvex_execution_workload_profile workload = {
        .schema_version = YVEX_EXECUTION_WORKLOAD_PROFILE_SCHEMA_V1,
        .kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY,
        .minimum_session_context = 3u, .requested_session_context = 3u, .concurrent_sequences = 1u,
        .logical_batch_tokens = 1u, .prefill_chunk_tokens = 1u, .attention_microbatch_rows = 1u,
        .moe_row_tile = 1u, .output_head_rows = 1u,
        .system_reserve_bytes = YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE, .latency_priority = 1};
    yvex_core_text_copy(workload.name, sizeof(workload.name), "physical-program-replay");
    int rc = yvex_runtime_session_summary_copy(run->session, &session, err);
    if (rc == YVEX_OK) rc = yvex_execution_workload_profile_seal(&workload, err);
    if (rc == YVEX_OK) rc = yvex_backend_cuda_attention_graph_summary_get(view->backend, &cuda, err);
    if (rc != YVEX_OK) return rc;
    yvex_runtime_execution_profile_request request = {
        .schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1,
        .engine_generation = session.engine_generation,
        .engine_specialization_identity = session.engine_specialization_identity,
        .kernel_bundle_identity = cuda.cuda_build_identity, .workload_profile_identity = workload.identity,
        .generation_mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY,
        .evidence = YVEX_EXECUTION_EVIDENCE_PRODUCTION,
        .execution_class = cuda.kernel_bundle_native ? YVEX_EXECUTION_CLASS_DEVICE_NATIVE :
            YVEX_EXECUTION_CLASS_PORTABLE_REFERENCE,
        .sampling_resolution = YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED,
        .moe_resolution = YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED,
        .attention_resolution = YVEX_EXECUTION_RESOLUTION_COMPATIBLE_DEGRADED};
    return yvex_runtime_execution_profile_seal(&request, &run->profile, err);
}

static int forward_execute(forward_run *run, yvex_model_engine *model,
    yvex_backend_kind backend, unsigned int replay, yvex_error *err)
{
    const unsigned int tokens[] = {1u, 2u};
    char input_identity[YVEX_SHA256_HEX_CAP];
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_runtime_session_open_request session = {.backend = backend};
    yvex_runtime_decoder_execution_options options = {.context_capacity = 3u, .token_capacity = 1u,
        .cancel_requested = forward_cancel, .cancel_context = run, .execution_profile = &run->profile};
    yvex_model_engine_failure failure = {0};
    yvex_runtime_decoder_execution_result result = {0};
    yvex_execution_device_view previous = {0};
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.test.program-forward.tokens.v1") ||
        !yvex_sha256_update_u64(&hash, tokens[0]) || !yvex_sha256_update_u64(&hash, tokens[1]) ||
        !yvex_sha256_final(&hash, digest)) return forward_refuse(err, "input identity failed");
    yvex_sha256_hex(digest, input_identity);
    int rc = yvex_runtime_session_open(&run->session, model, &session, &failure, err);
    if (rc == YVEX_OK) rc = forward_profile(run, err);
    if (rc == YVEX_OK) rc = yvex_runtime_decoder_execution_context_open(
        &run->runner, model, run->session, &options, err);
    const yvex_program_token_interface *interface = yvex_runtime_decoder_execution_interface(run->runner);
    if (rc != YVEX_OK) return rc;
    if (!interface || interface->vocabulary_size <= tokens[1] || !interface->hidden_width ||
        interface->hidden_width > SIZE_MAX / (2u * sizeof(float)))
        return forward_refuse(err, "bounded token-forward signature required");
    run->width = interface->hidden_width;
    run->values = malloc((size_t)run->width * 2u * sizeof(float));
    if (!run->values) return forward_refuse(err, "result allocation failed");
    for (unsigned int i = 0u; i < 2u; ++i) {
        yvex_runtime_decoder_execution_request request = {.token_ids = tokens + i,
            .token_start = i, .token_count = 1u, .input_identity = input_identity};
        unsigned long long started = yvex_core_monotonic_ns();
        rc = yvex_runtime_decoder_execution_execute(run->runner, &request, &result, err);
        if (rc != YVEX_OK) return rc;
        if (!result.completed || result.position_after != i + 1u ||
            yvex_execution_device_view_validate(&result.device_hidden, err) != YVEX_OK)
            return forward_refuse(err, "forward result or committed position is invalid");
        yvex_device_tensor view;
        if (!yvex_backend_tensor_f32_subview(result.device_hidden.tensor,
                result.device_hidden.element_offset, run->width, &view))
            return forward_refuse(err, "hidden publication geometry is invalid");
        rc = yvex_backend_tensor_read(result.device_hidden.backend, &view,
            run->values + i * run->width, run->width * sizeof(float), err);
        if (rc != YVEX_OK) return rc;
        for (unsigned long long j = 0u; j < run->width; ++j)
            if (!isfinite(run->values[i * run->width + j]))
                return forward_refuse(err, "non-finite hidden result");
        if (i) {
            yvex_error stale = {0};
            if (yvex_execution_device_view_validate(&previous, &stale) == YVEX_OK)
                return forward_refuse(err, "superseded hidden borrow remained valid");
        }
        previous = result.device_hidden;
        printf("program_forward replay=%u token=%u position=%llu width=%llu layers=%llu "
            "attention=%llu recurrent=%llu elapsed=%.6f first=%.9g last=%.9g state=%s\n",
            replay, tokens[i], result.position_after, run->width, result.layers_executed,
            result.attention_layers, result.recurrent_layers,
            (double)(yvex_core_monotonic_ns() - started) / 1e9,
            (double)run->values[i * run->width], (double)run->values[(i + 1u) * run->width - 1u],
            result.persistent_state_identity);
        fflush(stdout);
    }
    const yvex_runtime_session_view *session_view = yvex_runtime_session_view_get(run->session);
    yvex_sequence_state_summary before, after;
    rc = yvex_sequence_state_summary_copy(session_view->sequence_state, &before, err);
    if (rc != YVEX_OK) return rc;
    run->cancel = 1;
    yvex_runtime_decoder_execution_request cancelled = {.token_ids = tokens, .token_start = 2u,
        .token_count = 1u, .input_identity = input_identity};
    memset(&result, 0, sizeof(result));
    rc = yvex_runtime_decoder_execution_execute(run->runner, &cancelled, &result, err);
    if (rc != YVEX_ERR_CANCELLED || result.completed)
        return forward_refuse(err, "cancellation did not refuse without publication");
    yvex_error_clear(err);
    rc = yvex_sequence_state_summary_copy(session_view->sequence_state, &after, err);
    if (rc != YVEX_OK) return rc;
    if (after.committed_position != before.committed_position || after.transaction_active || after.invalidated)
        return forward_refuse(err, "cancellation changed committed position or retained a transaction");
    printf("program_forward replay=%u cancellation=refused committed_position=%llu stale_borrow=refused\n",
        replay, after.committed_position);
    return YVEX_OK;
}

int main(int argc, char **argv)
{
    if (argc != 4 || !argv[3][0]) {
        fprintf(stderr, "usage: %s ARTIFACT RUNTIME_BINDING TARGET\n", argv[0]);
        return 2;
    }
    yvex_backend_kind backend = YVEX_BACKEND_KIND_CUDA;
    yvex_model_engine_open_request request = {.artifact_path = argv[1], .runtime_binding_path = argv[2],
        .residency_backend = backend, .target_id = argv[3]};
    yvex_model_engine *model = NULL;
    yvex_model_engine_failure failure = {0};
    yvex_model_engine_summary summary;
    yvex_error err = {0}, cleanup = {0};
    forward_run runs[2] = {0};
    int rc = yvex_model_engine_open(&model, &request, &failure, &err);
    if (rc == YVEX_OK) rc = yvex_model_engine_summary_copy(model, &summary, &err);
    if (rc == YVEX_OK) {
        const yvex_model_engine_view *view = yvex_model_engine_view_get(model);
        const yvex_program_physical *program = yvex_compiled_model_plan_forward(view->compiled_plan);
        const yvex_program_physical_summary *physical = yvex_program_physical_summary_get(program);
        if (!physical) rc = forward_refuse(&err, "binding contains no physical token program");
        else printf("program_forward backend=%s artifact=%s binding=%s model=%s generation=%llu "
            "semantic=%s execution=%s physical=%s steps=%zu slots=%zu\n", "cuda", summary.artifact_identity,
            summary.runtime_binding_identity, summary.runtime_model_identity, summary.engine_generation,
            physical->semantic_identity, physical->execution_identity, physical->identity,
            physical->step_count, physical->storage_count);
        fflush(stdout);
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 2u; ++i) {
        rc = forward_execute(runs + i, model, backend, i, &err);
        if (runs[i].runner) {
            int closed = yvex_runtime_decoder_execution_context_close(&runs[i].runner, &cleanup);
            if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; err = cleanup; }
        }
        if (!runs[i].runner && runs[i].session) {
            int closed = yvex_runtime_session_close(&runs[i].session, &cleanup);
            if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; err = cleanup; }
        }
    }
    if (rc == YVEX_OK) {
        double maximum = 0.0;
        if (runs[0].width != runs[1].width) rc = forward_refuse(&err, "replay signature changed");
        for (unsigned long long i = 0u; rc == YVEX_OK && i < 2u * runs[0].width; ++i) {
            double difference = fabs((double)runs[0].values[i] - runs[1].values[i]);
            if (difference > maximum) maximum = difference;
        }
        printf("program_forward fresh_session_replay values=%llu max_abs=%.12g tolerance=0 "
            "oracle=same-implementation-determinism upstream=NOT_EXECUTED\n", 2u * runs[0].width, maximum);
        if (maximum != 0.0) rc = forward_refuse(&err, "fresh-session deterministic replay differs");
    }
    for (size_t i = 0u; i < 2u; ++i) {
        int closed = forward_close(runs + i, &cleanup);
        if (rc == YVEX_OK && closed != YVEX_OK) { rc = closed; err = cleanup; }
    }
    yvex_model_engine_close(&model);
    if (rc != YVEX_OK) fprintf(stderr, "program_forward status=%d where=%s reason=%s\n",
        rc, yvex_error_where(&err), yvex_error_message(&err));
    else puts("program_forward cleanup=complete result=PASS");
    return rc == YVEX_OK ? 0 : 1;
}
