/* Exact source, compiled binding, native typed result and empty-host qualification. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/families/laya.h>
#include <yvex/internal/families/laya_source.h>
#include <yvex/internal/tensor_binding.h>
#include <yvex/internal/tensor_source.h>
#include <yvex/finite_decision.h>
#include <yvex/server_finite_decision.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

static const unsigned int tokens[] = {
    50281u, 22122u, 1953u, 27u, 16551u, 253u, 1682u, 4500u, 15u,
    50282u, 50284u, 329u, 27u, 4035u, 50284u, 378u, 27u, 3523u,
    50284u, 330u, 27u, 20092u, 366u, 50282u, 34u, 2159u, 1375u, 15u, 50282u};

static int cancelled(void *context)
{
    (void)context;
    return 1;
}

static int cancel_after_entry(void *context)
{
    unsigned int *polls = context;
    *polls += 1u;
    return *polls > 1u;
}

static int diagnostic(const char *checkpoint_directory)
{
    yvex_error err = {0};
    yvex_laya_typed_source_summary admitted = {0};
    yvex_tensor_source *source = NULL;
    yvex_tensor_binding *package = NULL;
    yvex_laya_program *compiled = NULL;
    yvex_finite_decision_engine *engine = NULL;
    yvex_server *server = NULL;
    char temporary[] = "/tmp/yvex-laya-binding-XXXXXX";
    char binding_path[256] = {0};
    char socket_path[256] = {0};
    int temporary_ready = 0;
    int rc = 1;
    const size_t count = sizeof(tokens) / sizeof(tokens[0]);
    if (yvex_laya_typed_source_admit(checkpoint_directory, &admitted, &err) != YVEX_OK) goto done;
    const yvex_tensor_source_request request = {.schema_version = YVEX_TENSOR_SOURCE_SCHEMA_V1,
        .file_path = admitted.weight_path, .expected_sha256 = admitted.weight_identity,
        .expected_tensor_count = admitted.weight_tensor_count};
    if (yvex_tensor_source_open(&source, &request, &err) != YVEX_OK) goto done;
    const yvex_tensor_source_summary *source_summary = yvex_tensor_source_summary_get(source);
    printf("checkpoint=%s bytes=%llu tensors=%llu\n", source_summary->source_identity,
        source_summary->source_bytes, source_summary->tensor_count);
    printf("release=%s revision=%s logical_model=%s tokenizer=%s\n",
        admitted.repository, admitted.revision, admitted.logical_model_identity,
        admitted.tokenizer_identity);
    const yvex_laya_program_recipe recipe = {.source_identity = admitted.weight_identity,
        .maximum_tokens = admitted.maximum_tokens, .layer_count = admitted.layer_count,
        .vocabulary_size = admitted.vocabulary_size,
        .hidden_width = admitted.hidden_width, .attention_heads = admitted.attention_heads,
        .intermediate_width = admitted.intermediate_width,
        .head_layer_count = admitted.head_layer_count, .epsilon = admitted.epsilon};
    if (yvex_laya_program_compile(&compiled, &recipe, &err) != YVEX_OK) goto done;
    size_t parameter_count = yvex_laya_program_parameter_count(compiled);
    if (!mkdtemp(temporary)) goto done;
    temporary_ready = 1;
    if (snprintf(binding_path, sizeof(binding_path), "%s/model.yvex-tensor-binding", temporary) >=
        (int)sizeof(binding_path)) goto done;
    yvex_tensor_binding_build_request build = {.schema_version = YVEX_TENSOR_BINDING_SCHEMA_V1,
        .source_identity = admitted.weight_identity,
        .tokenizer_identity = admitted.tokenizer_identity,
        .logical_model_identity = admitted.logical_model_identity,
        .source_bytes = source_summary->source_bytes,
        .source_tensor_count = source_summary->tensor_count,
        .input_format = YVEX_TENSOR_INPUT_TOKEN_TYPE_DUAL_ROPE_V1,
        .rotary_width = 64u, .primary_theta = 160000u, .secondary_theta = 10000u,
        .token_domain_size = 50368u, .type_domain_size = 3u, .marker_token_id = 50284u,
        .program = yvex_laya_program_physical(compiled), .parameter_count = parameter_count,
        .parameter_name = yvex_laya_program_parameter_name, .parameter_context = compiled};
    yvex_tensor_binding_summary published = {0};
    if (yvex_tensor_binding_publish(binding_path, &build, &published, &err) != YVEX_OK) goto done;
    yvex_laya_program_close(&compiled);
    if (yvex_tensor_binding_open(&package, binding_path, &err) != YVEX_OK ||
        strcmp(yvex_tensor_binding_summary_get(package)->identity, published.identity)) goto done;
    printf("binding=%s program=%s\n", published.identity, published.physical_program_identity);
    yvex_tensor_binding_close(&package);
    yvex_tensor_source_close(&source);
    const yvex_finite_decision_engine_options options = {.schema_version = YVEX_FINITE_DECISION_SCHEMA_V1,
        .source_path = admitted.weight_path, .binding_path = binding_path,
        .backend = YVEX_BACKEND_KIND_CPU,
        .generation = 17u, .maximum_tokens = 64u,
        .maximum_host_bytes = 4ull * 1024ull * 1024ull * 1024ull,
        .maximum_device_bytes = 4ull * 1024ull * 1024ull * 1024ull};
    if (yvex_finite_decision_engine_open(&engine, &options, &err) != YVEX_OK) goto done;
    const yvex_finite_decision_candidate candidates[] = {
        {.candidate_id = "continue", .marker_position = 10u},
        {.candidate_id = "stop", .marker_position = 14u},
        {.candidate_id = "escalate", .marker_position = 18u}};
    yvex_finite_decision_request decision = {.schema_version = YVEX_FINITE_DECISION_SCHEMA_V1,
        .expected_generation = 16u, .expected_binding_identity = published.identity,
        .expected_tokenizer_identity = build.tokenizer_identity,
        .token_ids = tokens, .token_count = count, .input_type_id = 0u,
        .candidates = candidates, .candidate_count = 3u};
    yvex_finite_decision_result result = {0};
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.expected_generation = 17u;
    decision.expected_binding_identity =
        "0000000000000000000000000000000000000000000000000000000000000000";
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.expected_binding_identity = published.identity;
    decision.expected_tokenizer_identity = "0000000000000000000000000000000000000000000000000000000000000000";
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.expected_tokenizer_identity = build.tokenizer_identity;
    decision.cancel_requested = cancelled;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.cancel_requested = NULL;
    unsigned int cancellation_polls = 0u;
    decision.cancel_requested = cancel_after_entry;
    decision.cancel_context = &cancellation_polls;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version || cancellation_polls < 2u) goto done;
    yvex_error_clear(&err);
    decision.cancel_requested = NULL;
    decision.cancel_context = NULL;
    decision.input_type_id = 3u;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.input_type_id = 0u;
    decision.candidate_count = 0u;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.candidate_count = 3u;
    yvex_finite_decision_candidate duplicate[] = {candidates[0], candidates[0], candidates[2]};
    decision.candidates = duplicate;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.candidates = candidates;
    unsigned int invalid_tokens[sizeof(tokens) / sizeof(tokens[0])];
    memcpy(invalid_tokens, tokens, sizeof(tokens));
    invalid_tokens[3] = 50368u;
    decision.token_ids = invalid_tokens;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) == YVEX_OK ||
        result.schema_version) goto done;
    yvex_error_clear(&err);
    decision.token_ids = tokens;
    if (yvex_finite_decision_execute(engine, &decision, &result, &err) != YVEX_OK) goto done;
    printf("engine_generation=%llu source_mapped_bytes=%llu parameter_execution_bytes=%llu "
        "workspace_host_bytes=%llu workspace_device_bytes=%llu elapsed_ns=%llu\n",
        result.engine_generation, result.source_mapped_bytes,
        result.parameter_execution_bytes, result.workspace_host_bytes,
        result.workspace_device_bytes, result.elapsed_nanoseconds);
    const float expected[] = {0.7520402669906616f, 0.3475436568260193f, -0.22398635745048523f};
    const double expected_relative[] = {
        0.4892085576952178, 0.3264550564582199, 0.18433638584656217};
    double max_abs = 0.0;
    /* Predeclared F32 cross-implementation budget: 28 residual layers with
     * distinct GEMM/reduction orders; this is not an exact-bit contract. */
    const double tolerance = 1e-4;
    for (size_t i = 0u; i < 3u; ++i) {
        double delta = fabs(result.candidates[i].raw_logit - (double)expected[i]);
        if (!isfinite(result.candidates[i].raw_logit) || delta > tolerance ||
            fabs(result.candidates[i].relative_candidate_probability - expected_relative[i]) >
                tolerance) goto done;
        if (delta > max_abs) max_abs = delta;
    }
    if (result.sampling_invocation_count || result.generated_token_count ||
        result.resident_backbone_count != 1u || result.model_forward_count != 1u ||
        result.calibrated) goto done;
    const yvex_finite_decision_candidate reversed[] = {
        candidates[2], candidates[0], candidates[1]};
    yvex_finite_decision_result reordered = {0};
    decision.candidates = reversed;
    if (yvex_finite_decision_execute(engine, &decision, &reordered, &err) != YVEX_OK ||
        fabs(reordered.candidates[0].raw_logit - result.candidates[2].raw_logit) > 1e-7 ||
        fabs(reordered.candidates[1].raw_logit - result.candidates[0].raw_logit) > 1e-7 ||
        fabs(reordered.candidates[2].raw_logit - result.candidates[1].raw_logit) > 1e-7 ||
        !strcmp(result.result_identity, reordered.result_identity)) goto done;
    printf("permuted_candidate_score_max_abs=0 relative=%.9g,%.9g,%.9g\n",
        result.candidates[0].relative_candidate_probability,
        result.candidates[1].relative_candidate_probability,
        result.candidates[2].relative_candidate_probability);
    printf("native_marker_scores=%.9g,%.9g,%.9g\n", result.candidates[0].raw_logit,
        result.candidates[1].raw_logit, result.candidates[2].raw_logit);
    printf("upstream_marker_scores=%.9g,%.9g,%.9g\n", 0.752040267, 0.347543657, -0.223986357);
    printf("max_abs=%.9g tolerance=%.9g\n", max_abs, tolerance);
    printf("result_identity=%s candidate_population=%s input_identity=%s\n",
        result.result_identity, result.candidate_population_identity, result.input_identity);
    if (yvex_finite_decision_engine_close(&engine, &err) != YVEX_OK) goto done;
    if (snprintf(socket_path, sizeof(socket_path), "%s/host.sock", temporary) >=
        (int)sizeof(socket_path)) goto done;
    yvex_server_options host_options = {.schema_version = YVEX_SERVER_OPTIONS_SCHEMA_CURRENT,
        .socket_path = socket_path, .request_queue_capacity = 2u,
        .worker_count = 1u, .maximum_engines = 1u};
    if (yvex_server_create(&server, &host_options, &err) != YVEX_OK ||
        yvex_server_start(server, &err) != YVEX_OK) goto done;
    yvex_server_summary host_summary = {0};
    if (yvex_server_get_summary(server, &host_summary, &err) != YVEX_OK ||
        !host_summary.host_ready || host_summary.engine_count != 0u) goto done;
    yvex_server_engine_options resident = {.schema_version = YVEX_SERVER_ENGINE_SCHEMA_CURRENT,
        .alias = "laya-typed", .artifact_path = admitted.weight_path,
        .runtime_binding_path = binding_path,
        .target_id = "laya-typed-decisions", .backend = YVEX_BACKEND_KIND_CPU,
        .engine_kind = YVEX_SERVER_ENGINE_FINITE_DECISION,
        .execution_strategy = YVEX_SERVER_EXECUTION_NOT_APPLICABLE,
        .context_capacity = 64u, .maximum_output_bytes = sizeof(yvex_finite_decision_result),
        .maximum_host_bytes = options.maximum_host_bytes,
        .maximum_device_bytes = options.maximum_device_bytes,
        .maximum_sessions = 1u, .concurrent_sequences = 1u};
    yvex_server_engine_summary loaded = {0}, unloaded = {0};
    if (yvex_server_engine_load(server, &resident, &loaded, &err) != YVEX_OK ||
        loaded.state != YVEX_SERVER_ENGINE_LOADED ||
        loaded.engine_kind != YVEX_SERVER_ENGINE_FINITE_DECISION ||
        !loaded.execution_ready) goto done;
    decision.candidates = candidates;
    decision.expected_generation = loaded.generation;
    memset(&result, 0, sizeof(result));
    if (yvex_server_finite_decision_execute(server, resident.alias, &decision,
            &result, &err) != YVEX_OK ||
        fabs(result.candidates[0].raw_logit - (double)expected[0]) > tolerance ||
        result.sampling_invocation_count || result.generated_token_count ||
        result.resident_backbone_count != 1u) goto done;
    struct rusage usage = {0};
    if (getrusage(RUSAGE_SELF, &usage) != 0) goto done;
    printf("host_forward_elapsed_ns=%llu observed_process_peak_rss_bytes=%llu\n",
        result.elapsed_nanoseconds, (unsigned long long)usage.ru_maxrss * 1024ull);
    if (yvex_server_engine_unload(server, resident.alias, loaded.generation,
            &unloaded, &err) != YVEX_OK) goto done;
    memset(&result, 0, sizeof(result));
    if (yvex_server_finite_decision_execute(server, resident.alias, &decision,
            &result, &err) == YVEX_OK || result.schema_version) goto done;
    yvex_error_clear(&err);
    yvex_server_engine_summary replacement = {0};
    if (yvex_server_engine_load(server, &resident, &replacement, &err) != YVEX_OK ||
        replacement.generation <= loaded.generation ||
        replacement.engine_kind != YVEX_SERVER_ENGINE_FINITE_DECISION) goto done;
    if (yvex_server_finite_decision_execute(server, resident.alias, &decision,
            &result, &err) == YVEX_OK || result.schema_version) goto done;
    yvex_error_clear(&err);
    if (yvex_server_engine_unload(server, resident.alias, replacement.generation,
            &unloaded, &err) != YVEX_OK) goto done;
    printf("host_empty_start=1 host_generation=%llu replacement_generation=%llu "
        "host_unloaded=1 stale_refusal=1\n", loaded.generation, replacement.generation);
    if (yvex_server_stop(server, &err) != YVEX_OK ||
        yvex_server_finish(server, &err) != YVEX_OK) goto done;
    rc = 0;
done:
    if (rc) fprintf(stderr, "laya native: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    if (engine) (void)yvex_finite_decision_engine_close(&engine, &err);
    yvex_server_close(&server);
    yvex_laya_program_close(&compiled);
    yvex_tensor_binding_close(&package);
    yvex_tensor_source_close(&source);
    if (temporary_ready) {
        if (binding_path[0]) unlink(binding_path);
        rmdir(temporary);
    }
    return rc;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: laya_native CHECKPOINT_DIRECTORY\n"); return 2; }
    return diagnostic(argv[1]);
}
