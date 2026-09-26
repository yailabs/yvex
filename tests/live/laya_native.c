/* Exact source, compiled binding, native typed result and empty-host qualification. */
#define _POSIX_C_SOURCE 200809L
#include <yvex/internal/families/laya.h>
#include <yvex/internal/families/laya_source.h>
#include <yvex/internal/tensor_binding.h>
#include <yvex/internal/tensor_source.h>
#include <yvex/internal/finite_input.h>
#include <yvex/finite_decision.h>
#include <yvex/finite_decision_producer.h>
#include <yvex/server_finite_decision.h>
#include <yvex/internal/finite_producer_wire.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>

extern char **environ;

#define LAYA_EXPECTED_SOURCE "4fa56de72383a9d3efa9cfa78955733c81b9fc8067a587ca4beb82c78107a24e"
#define LAYA_EXPECTED_BINDING "5d1d4598c32c8317efbacfa3097cc4e630485cb9722e1f25e3fc3fe802fa1c41"
#define LAYA_EXPECTED_TOKENIZER "6c8aaa9a542084f2457eab775d4eeb51f92a70c0fd9de28d5edb0ddec3c08d30"
#define LAYA_EXPECTED_PROGRAM "cc2bbc47dbe9e053299e948a8bb4a5b4b234f6c37a3f2ad08a118dfe190625b1"

static void producer_fixture(yvex_finite_producer_request *producer,
    unsigned long long generation)
{
    memset(producer, 0, sizeof(*producer));
    producer->schema_version = YVEX_FINITE_PRODUCER_SCHEMA_V1;
    producer->expected_generation = generation;
    producer->candidate_count = 3u;
    strcpy(producer->model_alias, "laya-typed");
    strcpy(producer->question, "Select the best option.");
    strcpy(producer->context, "A short state.");
    strcpy(producer->candidates[0].id, "continue");
    strcpy(producer->candidates[0].text, "continue");
    strcpy(producer->candidates[1].id, "stop");
    strcpy(producer->candidates[1].text, "stop");
    strcpy(producer->candidates[2].id, "escalate");
    strcpy(producer->candidates[2].text, "escalate");
}

static void *host_serve(void *opaque)
{
    yvex_error err = {0};
    return (void *)(intptr_t)yvex_server_serve(opaque, &err);
}

static int client_process(const char *program, const char *mode,
    const char *socket_path, unsigned long long generation)
{
    char number[32];
    pid_t child;
    int status;
    snprintf(number, sizeof(number), "%llu", generation);
    char *const args[] = {(char *)program, (char *)mode, (char *)socket_path, number, NULL};
    if (posix_spawn(&child, program, NULL, NULL, args, environ) != 0 ||
        waitpid(child, &status, 0) != child)
        return 0;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int client_mode(const char *mode, const char *socket_path, unsigned long long generation)
{
    yvex_finite_producer_request request;
    yvex_finite_producer_result result = {0};
    yvex_error err = {0};
    producer_fixture(&request, generation);
    if (!strcmp(mode, "--client-invalid"))
        strcpy(request.candidates[1].id, request.candidates[0].id);
    if (!strcmp(mode, "--client-overlong")) {
        memset(request.question, 'x', sizeof(request.question) - 1u);
        request.question[sizeof(request.question) - 1u] = 0;
    }
    if (!strcmp(mode, "--client-disconnect")) {
        unsigned char payload[4096];
        size_t payload_bytes = 0u;
        yvex_client *client = NULL;
        if (yvex_finite_producer_request_encode(&request, payload,
                sizeof(payload), &payload_bytes, &err) != YVEX_OK ||
            yvex_client_connect(&client, socket_path, &err) != YVEX_OK) return 1;
        yvex_client_request outbound = {.schema_version = YVEX_LOCAL_PROTOCOL_VERSION,
            .operation = YVEX_CLIENT_OP_FINITE_DECISION, .request_number = 5u,
            .engine_generation = generation, .prompt = payload, .prompt_bytes = payload_bytes};
        strcpy(outbound.model_alias, request.model_alias);
        int sent = yvex_client_send(client, &outbound, &err) == YVEX_OK;
        if (sent) { struct timespec pause = {.tv_nsec = 200000000L}; nanosleep(&pause, NULL); }
        yvex_client_close(&client);
        return sent ? 0 : 1;
    }
    int rc = yvex_finite_producer_execute_local(socket_path, &request, &result, &err);
    if (!strcmp(mode, "--client-stale"))
        return rc == YVEX_ERR_STATE && !result.schema_version ? 0 : 1;
    if (!strcmp(mode, "--client-invalid"))
        return rc == YVEX_ERR_FORMAT && !result.schema_version ? 0 : 1;
    if (!strcmp(mode, "--client-overlong"))
        return rc == YVEX_ERR_BOUNDS && !result.schema_version ? 0 : 1;
    const double expected[] = {0.752040267, 0.347543657, -0.223986357};
    if (rc != YVEX_OK || result.candidate_count != 3u || result.token_count != 29u ||
        result.engine_generation != generation || result.sampling_invocation_count ||
        result.generated_token_count || result.resident_backbone_count != 1u ||
        result.calibrated || result.score_kind != YVEX_FINITE_PRODUCER_SCORE_MODEL_LOGIT ||
        strcmp(result.source_identity, LAYA_EXPECTED_SOURCE) ||
        strcmp(result.binding_identity, LAYA_EXPECTED_BINDING) ||
        strcmp(result.tokenizer_identity, LAYA_EXPECTED_TOKENIZER) ||
        strcmp(result.physical_program_identity, LAYA_EXPECTED_PROGRAM) ||
        !yvex_sha256_hex_valid(result.input_policy_identity)) {
        fprintf(stderr, "producer client: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
        return 1;
    }
    double max_abs = 0.0;
    for (size_t i = 0u; i < 3u; ++i) {
        double delta = fabs(result.candidates[i].raw_score - expected[i]);
        if (delta > 1e-4 || strcmp(result.candidates[i].id, request.candidates[i].id)) return 1;
        if (delta > max_abs) max_abs = delta;
    }
    printf("producer_process=separate generation=%llu input_tokens=%llu raw=%.9g,%.9g,%.9g "
        "upstream_max_abs=%.9g tolerance=0.0001 sampling=%llu generated=%llu backbones=%llu\n",
        result.engine_generation, result.token_count, result.candidates[0].raw_score,
        result.candidates[1].raw_score, result.candidates[2].raw_score,
        max_abs, result.sampling_invocation_count, result.generated_token_count,
        result.resident_backbone_count);
    return 0;
}

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

static int diagnostic(const char *checkpoint_directory, const char *program)
{
    yvex_error err = {0};
    yvex_laya_typed_source_summary admitted = {0};
    yvex_tensor_source *source = NULL;
    yvex_tensor_binding *package = NULL;
    yvex_laya_program *compiled = NULL;
    yvex_finite_decision_engine *engine = NULL;
    yvex_finite_input *input_policy = NULL;
    yvex_server *server = NULL;
    pthread_t serving;
    int serving_started = 0;
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
    yvex_finite_input_admission input_engine = {
        .input_format = published.input_format, .marker_token_id = published.marker_token_id,
        .token_domain_size = published.token_domain_size,
        .type_domain_size = published.type_domain_size};
    memcpy(input_engine.tokenizer_identity, published.tokenizer_identity,
        sizeof(input_engine.tokenizer_identity));
    memcpy(input_engine.binding_identity, published.identity,
        sizeof(input_engine.binding_identity));
    memcpy(input_engine.source_identity, published.source_identity,
        sizeof(input_engine.source_identity));
    yvex_finite_input_admission foreign_source = input_engine;
    memset(foreign_source.source_identity, '0', 64u);
    foreign_source.source_identity[64] = 0;
    if (yvex_finite_input_open(&input_policy, admitted.weight_path, &foreign_source, &err) !=
        YVEX_ERR_UNSUPPORTED || input_policy) goto done;
    yvex_finite_producer_request producer;
    producer_fixture(&producer, 17u);
    yvex_finite_input_compiled prepared = {0};
    if (yvex_finite_input_open(&input_policy, admitted.weight_path, &input_engine, &err) != YVEX_OK ||
        yvex_finite_input_build(input_policy, &producer, &prepared, &err) != YVEX_OK ||
        prepared.token_count != count || memcmp(prepared.tokens, tokens, sizeof(tokens)) ||
        prepared.candidates[0].marker_position != 10u ||
        prepared.candidates[1].marker_position != 14u ||
        prepared.candidates[2].marker_position != 18u) goto done;
    printf("producer_input_tokens=%llu upstream_input_equal=1 marker_positions=10,14,18\n",
        prepared.token_count);
    yvex_finite_input_close(&input_policy);
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
    if (pthread_create(&serving, NULL, host_serve, server) != 0) goto done;
    serving_started = 1;
    if (!client_process(program, "--client", socket_path, loaded.generation) ||
        !client_process(program, "--client-invalid", socket_path, loaded.generation) ||
        !client_process(program, "--client-overlong", socket_path, loaded.generation) ||
        !client_process(program, "--client-disconnect", socket_path, loaded.generation)) goto done;
    for (unsigned int attempt = 0u; attempt < 100u; ++attempt) {
        yvex_server_engine_summary observed[1] = {{0}};
        unsigned long long snapshot_count = 0u;
        if (yvex_server_engine_snapshot(server, observed, 1u, &snapshot_count, &err) != YVEX_OK ||
            snapshot_count != 1u) goto done;
        if (!observed[0].active_work) break;
        struct timespec pause = {.tv_nsec = 100000000L};
        nanosleep(&pause, NULL);
        if (attempt == 99u) goto done;
    }
    decision.candidates = candidates;
    decision.expected_generation = loaded.generation;
    memset(&result, 0, sizeof(result));
    if (yvex_server_finite_decision_execute(server, resident.alias, &decision,
            &result, &err) != YVEX_OK ||
        fabs(result.candidates[0].raw_logit - (double)expected[0]) > tolerance ||
        result.sampling_invocation_count || result.generated_token_count ||
        result.resident_backbone_count != 1u) goto done;
    printf("producer_refusals=duplicate,overlength,stale disconnect_followup_forward=pass\n");
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
    if (!client_process(program, "--client-stale", socket_path, loaded.generation)) goto done;
    if (yvex_server_finite_decision_execute(server, resident.alias, &decision,
            &result, &err) == YVEX_OK || result.schema_version) goto done;
    yvex_error_clear(&err);
    if (yvex_server_engine_unload(server, resident.alias, replacement.generation,
            &unloaded, &err) != YVEX_OK) goto done;
    printf("host_empty_start=1 host_generation=%llu replacement_generation=%llu "
        "host_unloaded=1 stale_refusal=1\n", loaded.generation, replacement.generation);
    if (yvex_server_stop(server, &err) != YVEX_OK ||
        yvex_server_finish(server, &err) != YVEX_OK) goto done;
    if (serving_started) { pthread_join(serving, NULL); serving_started = 0; }
    rc = 0;
done:
    if (serving_started) {
        yvex_error stop_error = {0};
        (void)yvex_server_stop(server, &stop_error);
        pthread_join(serving, NULL);
    }
    yvex_finite_input_close(&input_policy);
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
    if (argc == 4 && !strncmp(argv[1], "--client", 8u))
        return client_mode(argv[1], argv[2], strtoull(argv[3], NULL, 10));
    if (argc != 2) { fprintf(stderr, "usage: laya_native CHECKPOINT_DIRECTORY\n"); return 2; }
    return diagnostic(argv[1], argv[0]);
}
