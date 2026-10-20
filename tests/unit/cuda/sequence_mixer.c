/* CUDA gated-delta execution is compared with the portable exact F32 semantic owner. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/api.h>
#include <yvex/internal/sequence_mixer.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/internal/transformer.h>
#include <yvex/internal/quant_numeric.h>

#include "tests/test.h"

typedef struct {
    float *qkv, *output_gate, *beta, *decay;
    float *convolution_weight, *decay_log, *time_bias, *normalization_weight;
    float *committed_convolution, *committed_recurrent;
    float *candidate_convolution, *candidate_recurrent, *output;
    float *observed_convolution, *observed_recurrent, *observed_output;
} gated_delta_host;

typedef struct {
    yvex_device_tensor *qkv, *output_gate, *beta, *decay;
    yvex_device_tensor *convolution_weight, *decay_log, *time_bias;
    yvex_device_tensor *normalization_weight;
    yvex_device_tensor *committed_convolution, *committed_recurrent;
    yvex_device_tensor *candidate_convolution, *candidate_recurrent, *output;
} gated_delta_device;

static int gated_delta_allocate(float **values, unsigned long long count)
{
    if (!values || !count || count > SIZE_MAX / sizeof(float)) return 0;
    *values = (float *)calloc((size_t)count, sizeof(float));
    return *values != NULL;
}

static int gated_delta_host_open(const yvex_gated_delta_plan *plan,
                                 unsigned long long tokens, int committed,
                                 gated_delta_host *host)
{
    unsigned long long qkv_values = tokens * plan->qkv_width;
    unsigned long long output_values = tokens * plan->value_width;
    unsigned long long head_values = tokens * plan->requirement.value_heads;
    unsigned long long weight_values =
        plan->qkv_width * plan->requirement.convolution_kernel;
    memset(host, 0, sizeof(*host));
#define ALLOCATE(field, count) \
    do { if (!gated_delta_allocate(&host->field, (count))) return 0; } while (0)
    ALLOCATE(qkv, qkv_values);
    ALLOCATE(output_gate, output_values);
    ALLOCATE(beta, head_values);
    ALLOCATE(decay, head_values);
    ALLOCATE(convolution_weight, weight_values);
    ALLOCATE(decay_log, plan->requirement.value_heads);
    ALLOCATE(time_bias, plan->requirement.value_heads);
    ALLOCATE(normalization_weight, plan->requirement.value_head_dimension);
    if (committed) {
        ALLOCATE(committed_convolution, plan->convolution_state_values);
        ALLOCATE(committed_recurrent, plan->recurrent_state_values);
    }
    ALLOCATE(candidate_convolution, plan->convolution_state_values);
    ALLOCATE(candidate_recurrent, plan->recurrent_state_values);
    ALLOCATE(output, output_values);
    ALLOCATE(observed_convolution, plan->convolution_state_values);
    ALLOCATE(observed_recurrent, plan->recurrent_state_values);
    ALLOCATE(observed_output, output_values);
#undef ALLOCATE
    return 1;
}

static void gated_delta_host_close(gated_delta_host *host)
{
    if (!host) return;
    free(host->qkv); free(host->output_gate); free(host->beta); free(host->decay);
    free(host->convolution_weight); free(host->decay_log); free(host->time_bias);
    free(host->normalization_weight); free(host->committed_convolution);
    free(host->committed_recurrent); free(host->candidate_convolution);
    free(host->candidate_recurrent); free(host->output);
    free(host->observed_convolution); free(host->observed_recurrent);
    free(host->observed_output);
    memset(host, 0, sizeof(*host));
}

static void gated_delta_seed(const yvex_gated_delta_plan *plan,
                             unsigned long long tokens, gated_delta_host *host)
{
    unsigned long long index, channel, tap;
    for (index = 0ull; index < tokens * plan->qkv_width; ++index)
        host->qkv[index] = (float)((int)((index * 13ull + 5ull) % 31ull) - 15) * 0.0125f;
    for (index = 0ull; index < tokens * plan->value_width; ++index)
        host->output_gate[index] =
            (float)((int)((index * 7ull + 3ull) % 23ull) - 11) * 0.025f;
    for (index = 0ull; index < tokens * plan->requirement.value_heads; ++index) {
        host->beta[index] =
            (float)((int)((index * 5ull + 1ull) % 17ull) - 8) * 0.04f;
        host->decay[index] =
            (float)((int)((index * 11ull + 2ull) % 19ull) - 9) * 0.03f;
    }
    for (channel = 0ull; channel < plan->qkv_width; ++channel)
        for (tap = 0ull; tap < plan->requirement.convolution_kernel; ++tap)
            host->convolution_weight[
                channel * plan->requirement.convolution_kernel + tap] =
                tap + 1ull == plan->requirement.convolution_kernel
                    ? 0.72f : 0.035f * (float)(tap + 1ull);
    for (index = 0ull; index < plan->requirement.value_heads; ++index) {
        host->decay_log[index] = -2.1f + 0.025f * (float)(index % 7ull);
        host->time_bias[index] = -0.8f + 0.04f * (float)(index % 5ull);
    }
    for (index = 0ull; index < plan->requirement.value_head_dimension; ++index)
        host->normalization_weight[index] = 0.9f + 0.002f * (float)(index % 17ull);
    if (host->committed_convolution)
        for (index = 0ull; index < plan->convolution_state_values; ++index)
            host->committed_convolution[index] =
                (float)((int)(index % 13ull) - 6) * 0.003f;
    if (host->committed_recurrent)
        for (index = 0ull; index < plan->recurrent_state_values; ++index)
            host->committed_recurrent[index] =
                (float)((int)(index % 17ull) - 8) * 0.0005f;
}

static int gated_delta_tensor_open(yvex_backend *backend, const char *name,
                                   unsigned long long count, const float *source,
                                   yvex_device_tensor **tensor, yvex_error *err)
{
    yvex_backend_tensor_desc descriptor = {0};
    int rc;
    descriptor.name = name;
    descriptor.dtype = YVEX_DTYPE_F32;
    descriptor.rank = 1u;
    descriptor.dims[0] = count;
    descriptor.bytes = count * sizeof(float);
    rc = yvex_backend_tensor_alloc(backend, &descriptor, tensor, err);
    if (rc == YVEX_OK && source)
        rc = yvex_backend_tensor_write(
            backend, *tensor, source, descriptor.bytes, err);
    return rc;
}

static int gated_delta_device_open(
    yvex_backend *backend, const yvex_gated_delta_plan *plan,
    unsigned long long tokens, const gated_delta_host *host,
    gated_delta_device *device, yvex_error *err)
{
    unsigned long long output_values = tokens * plan->value_width;
    unsigned long long head_values = tokens * plan->requirement.value_heads;
    int rc = YVEX_OK;
    memset(device, 0, sizeof(*device));
#define OPEN(field, count, source) \
    do { \
        if (rc == YVEX_OK) rc = gated_delta_tensor_open( \
            backend, "gated-delta-" #field, (count), (source), &device->field, err); \
    } while (0)
    OPEN(qkv, tokens * plan->qkv_width, host->qkv);
    OPEN(output_gate, output_values, host->output_gate);
    OPEN(beta, head_values, host->beta);
    OPEN(decay, head_values, host->decay);
    OPEN(convolution_weight,
         plan->qkv_width * plan->requirement.convolution_kernel,
         host->convolution_weight);
    OPEN(decay_log, plan->requirement.value_heads, host->decay_log);
    OPEN(time_bias, plan->requirement.value_heads, host->time_bias);
    OPEN(normalization_weight, plan->requirement.value_head_dimension,
         host->normalization_weight);
    if (host->committed_convolution) {
        OPEN(committed_convolution, plan->convolution_state_values,
             host->committed_convolution);
        OPEN(committed_recurrent, plan->recurrent_state_values,
             host->committed_recurrent);
    }
    OPEN(candidate_convolution, plan->convolution_state_values, NULL);
    OPEN(candidate_recurrent, plan->recurrent_state_values, NULL);
    OPEN(output, output_values, NULL);
#undef OPEN
    return rc;
}

static int gated_delta_device_close(yvex_backend *backend,
                                    gated_delta_device *device, yvex_error *err)
{
    yvex_device_tensor **tensors[] = {
        &device->qkv, &device->output_gate, &device->beta, &device->decay,
        &device->convolution_weight, &device->decay_log, &device->time_bias,
        &device->normalization_weight, &device->committed_convolution,
        &device->committed_recurrent, &device->candidate_convolution,
        &device->candidate_recurrent, &device->output};
    size_t index;
    int rc = YVEX_OK;
    for (index = sizeof(tensors) / sizeof(tensors[0]); index > 0u; --index) {
        int release_rc = *tensors[index - 1u]
            ? yvex_backend_tensor_release(backend, tensors[index - 1u], err)
            : YVEX_OK;
        if (rc == YVEX_OK && release_rc != YVEX_OK) rc = release_rc;
    }
    return rc;
}

static double gated_delta_compare(const float *expected, const float *actual,
                                  unsigned long long count, double tolerance)
{
    unsigned long long index;
    double maximum = 0.0;
    for (index = 0ull; index < count; ++index) {
        double difference = fabs((double)expected[index] - (double)actual[index]);
        double bound = tolerance * (1.0 + fabs((double)expected[index]));
        if (!isfinite(actual[index]) || difference > bound) return -1.0;
        if (difference > maximum) maximum = difference;
    }
    return maximum;
}

static int gated_delta_cancelled(void *context)
{
    return context != NULL;
}

static int gated_delta_device_state_lifecycle(
    yvex_backend *owner, const yvex_gated_delta_requirement *requirement)
{
    yvex_backend *backend = NULL;
    yvex_gated_delta_plan mixer;
    yvex_sequence_state_binding binding;
    yvex_sequence_state_plan plan;
    yvex_sequence_state *state = NULL, *forked = NULL;
    yvex_sequence_state_summary summary;
    yvex_sequence_device_state_view committed;
    yvex_sequence_device_state_output candidate;
    yvex_runtime_transaction_participant participant;
    float *convolution = NULL, *recurrent = NULL, *observed = NULL;
    unsigned long long convolution_values, recurrent_values, index;
    char source_identity[YVEX_SHA256_HEX_CAP] = {0};
    char fork_identity[YVEX_SHA256_HEX_CAP] = {0};
    yvex_error err;

    YVEX_TEST_ASSERT(yvex_backend_open_shared_cuda(
        &backend, owner, 0ull, &err) == YVEX_OK,
        "source state uses a session-local executor");
    if (yvex_gated_delta_plan_seal(&mixer, requirement, &err) != YVEX_OK)
        return 1;
    YVEX_TEST_ASSERT(yvex_sequence_state_binding_seal(
        &binding, 3ull, mixer.convolution_state_values,
        mixer.recurrent_state_values, mixer.identity, &err) == YVEX_OK,
        "seal device state geometry");
    plan = (yvex_sequence_state_plan){
        .schema_version = YVEX_SEQUENCE_STATE_SCHEMA_V1,
        .bindings = &binding,
        .binding_count = 1ull};
    YVEX_TEST_ASSERT(
        yvex_sequence_state_open_for_backend(
            &state, &plan, YVEX_BACKEND_KIND_CUDA, &err) == YVEX_OK &&
            yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            !summary.host_state_bytes && !summary.device_state_bytes &&
            !summary.device_attached && !summary.fork_supported,
        "CUDA sequence state opens without a giant host mirror");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_attach_device(state, backend, &err) == YVEX_OK &&
            yvex_sequence_state_summary_copy(state, &summary, &err) == YVEX_OK &&
            summary.device_attached && summary.device_authoritative &&
            summary.device_state_bytes ==
                2ull * (summary.convolution_state_bytes +
                        summary.recurrent_state_bytes) &&
            summary.fork_supported,
        "CUDA sequence state owns two exact backend banks");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_begin(state, 0ull, 1ull, &err) == YVEX_OK &&
            yvex_sequence_state_device_layer(
                state, 3ull, &committed, &candidate, &err) == YVEX_OK &&
            !committed.convolution && !committed.recurrent &&
            yvex_sequence_state_stage(state, 3ull, &err) == YVEX_ERR_STATE,
        "unwritten CUDA candidate state cannot stage");
    convolution_values =
        yvex_device_tensor_bytes(candidate.convolution) / sizeof(float);
    recurrent_values =
        yvex_device_tensor_bytes(candidate.recurrent) / sizeof(float);
    convolution = calloc((size_t)convolution_values, sizeof(*convolution));
    recurrent = calloc((size_t)recurrent_values, sizeof(*recurrent));
    observed = calloc(
        (size_t)(convolution_values > recurrent_values
                     ? convolution_values
                     : recurrent_values),
        sizeof(*observed));
    YVEX_TEST_ASSERT(convolution && recurrent && observed,
                     "allocate bounded CUDA state fixture");
    for (index = 0ull; index < convolution_values; ++index)
        convolution[index] = 1.25f;
    for (index = 0ull; index < recurrent_values; ++index)
        recurrent[index] = -2.5f;
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_write(
            backend, candidate.convolution, convolution,
            convolution_values * sizeof(float), &err) == YVEX_OK &&
            yvex_backend_tensor_write(
                backend, candidate.recurrent, recurrent,
                recurrent_values * sizeof(float), &err) == YVEX_OK &&
            yvex_sequence_state_stage(state, 3ull, &err) == YVEX_OK &&
            yvex_sequence_state_participant(state, &participant, &err) == YVEX_OK &&
            yvex_runtime_transaction_resolve(
                &participant, 1u, YVEX_OK, &err) == YVEX_OK,
        "CUDA candidate recurrence publishes through the common transaction");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_fork(&forked, state, &err) == YVEX_OK &&
            yvex_sequence_state_committed_identity(
                state, source_identity, &err) == YVEX_OK &&
            yvex_sequence_state_committed_identity(
                forked, fork_identity, &err) == YVEX_OK &&
            strcmp(source_identity, fork_identity) == 0 &&
            yvex_sequence_state_begin(forked, 1ull, 1ull, &err) == YVEX_OK &&
            yvex_sequence_state_device_layer(
                forked, 3ull, &committed, &candidate, &err) == YVEX_OK &&
            committed.convolution && committed.recurrent &&
            yvex_backend_tensor_read(
                committed.convolution->owner, committed.convolution, observed,
                convolution_values * sizeof(float), &err) == YVEX_OK &&
            observed[0] == 1.25f &&
            yvex_sequence_state_participant(
                forked, &participant, &err) == YVEX_OK &&
            yvex_runtime_transaction_resolve(
                &participant, 1u, YVEX_ERR_CANCELLED, &err) ==
                YVEX_ERR_CANCELLED,
        "device-authored recurrent fork preserves exact committed state");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_begin(state, 1ull, 1ull, &err) == YVEX_OK &&
            yvex_sequence_state_device_layer(
                state, 3ull, &committed, &candidate, &err) == YVEX_OK &&
            committed.convolution && committed.recurrent &&
            yvex_backend_tensor_read(
                backend, committed.convolution, observed,
                convolution_values * sizeof(float), &err) == YVEX_OK &&
            observed[0] == 1.25f &&
            yvex_sequence_state_participant(state, &participant, &err) == YVEX_OK &&
            yvex_runtime_transaction_resolve(
                &participant, 1u, YVEX_ERR_CANCELLED, &err) ==
                YVEX_ERR_CANCELLED,
        "next CUDA turn reads only committed recurrence and aborts its candidate");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_reset(state, &err) == YVEX_OK &&
            yvex_sequence_state_begin(state, 0ull, 1ull, &err) == YVEX_OK &&
            yvex_sequence_state_device_layer(
                state, 3ull, &committed, &candidate, &err) == YVEX_OK &&
            yvex_backend_tensor_read(
                backend, candidate.convolution, observed,
                convolution_values * sizeof(float), &err) == YVEX_OK &&
            observed[0] == 0.0f &&
            yvex_sequence_state_participant(state, &participant, &err) == YVEX_OK &&
            yvex_runtime_transaction_resolve(
                &participant, 1u, YVEX_ERR_CANCELLED, &err) ==
                YVEX_ERR_CANCELLED,
        "reset clears both device banks and restores position zero");
    YVEX_TEST_ASSERT(
        yvex_sequence_state_close_checked(&state, &err) == YVEX_OK && !state &&
            yvex_backend_close_checked(&backend, &err) == YVEX_OK && !backend &&
            yvex_sequence_state_committed_identity(
                forked, fork_identity, &err) == YVEX_OK &&
            strcmp(source_identity, fork_identity) == 0,
        "immutable CUDA fork survives source session executor close");
    YVEX_TEST_ASSERT(setenv("YVEX_TEST_CUDA_CLEANUP_FAILURE", "tensor-alloc", 1) == 0,
        "inject snapshot storage release failure");
    int close_rc = yvex_sequence_state_close_checked(&forked, &err);
    YVEX_TEST_ASSERT(unsetenv("YVEX_TEST_CUDA_CLEANUP_FAILURE") == 0,
        "remove snapshot storage release failure");
    YVEX_TEST_ASSERT(close_rc == YVEX_ERR_BACKEND && forked &&
        yvex_sequence_state_committed_identity(forked, fork_identity, &err) != YVEX_OK &&
        yvex_sequence_state_close_checked(&forked, &err) == YVEX_OK && !forked,
        "failed snapshot release retains an invalidated owner and closes on retry");
    printf("CUDA recurrent fork lifetime: source_executor=closed snapshot_identity=unchanged "
        "cleanup_failure=retained retry=complete\n");
    free(observed);
    free(recurrent);
    free(convolution);
    return 0;
}

static int gated_delta_parity_case(
    yvex_backend *backend, const yvex_gated_delta_requirement *requirement,
    unsigned long long tokens, int committed, unsigned long long expected_chunks,
    double tolerance, double *maximum_difference)
{
    const yvex_backend_transformer_operations *operations =
        yvex_backend_transformer_operations_get(backend);
    yvex_gated_delta_plan plan;
    gated_delta_host host;
    gated_delta_device device;
    yvex_gated_delta_cpu_request cpu_request = {0};
    yvex_gated_delta_cpu_result cpu_result;
    yvex_gated_delta_device_request request = {0};
    yvex_gated_delta_device_result result;
    yvex_backend_operation_facts facts;
    yvex_error err;
    unsigned long long workspace = 0ull;
    double difference;
    int rc;
    YVEX_TEST_ASSERT(
        operations && operations->gated_delta_workspace_required &&
            operations->gated_delta_execute,
        "CUDA exposes admitted gated-delta transformer operations");
    YVEX_TEST_ASSERT(
        yvex_gated_delta_plan_seal(&plan, requirement, &err) == YVEX_OK &&
            gated_delta_host_open(&plan, tokens, committed, &host),
        "seal CUDA gated-delta plan and allocate host fixture");
    gated_delta_seed(&plan, tokens, &host);
    cpu_request.token_count = tokens;
    cpu_request.projected_qkv = host.qkv;
    cpu_request.projected_qkv_capacity = tokens * plan.qkv_width;
    cpu_request.projected_output_gate = host.output_gate;
    cpu_request.projected_output_gate_capacity = tokens * plan.value_width;
    cpu_request.projected_beta = host.beta;
    cpu_request.projected_beta_capacity = tokens * requirement->value_heads;
    cpu_request.projected_decay = host.decay;
    cpu_request.projected_decay_capacity = tokens * requirement->value_heads;
    cpu_request.convolution_weight = host.convolution_weight;
    cpu_request.convolution_weight_capacity =
        plan.qkv_width * requirement->convolution_kernel;
    cpu_request.decay_log = host.decay_log;
    cpu_request.decay_log_capacity = requirement->value_heads;
    cpu_request.time_bias = host.time_bias;
    cpu_request.time_bias_capacity = requirement->value_heads;
    cpu_request.normalization_weight = host.normalization_weight;
    cpu_request.normalization_weight_capacity = requirement->value_head_dimension;
    cpu_request.state.convolution = host.committed_convolution;
    cpu_request.state.recurrent = host.committed_recurrent;
    cpu_request.next_state.convolution = host.candidate_convolution;
    cpu_request.next_state.convolution_capacity = plan.convolution_state_values;
    cpu_request.next_state.recurrent = host.candidate_recurrent;
    cpu_request.next_state.recurrent_capacity = plan.recurrent_state_values;
    cpu_request.output = host.output;
    cpu_request.output_capacity = tokens * plan.value_width;
    YVEX_TEST_ASSERT(
        yvex_gated_delta_execute_cpu(
            &plan, &cpu_request, &cpu_result, &err) == YVEX_OK &&
            cpu_result.complete,
        "portable gated-delta semantic authority executes fixture");
    YVEX_TEST_ASSERT(
        gated_delta_device_open(
            backend, &plan, tokens, &host, &device, &err) == YVEX_OK,
        "allocate complete CUDA gated-delta fixture");
    request.token_count = tokens;
    request.projected_qkv = device.qkv;
    request.projected_output_gate = device.output_gate;
    request.projected_beta = device.beta;
    request.projected_decay = device.decay;
    request.convolution_weight = device.convolution_weight;
    request.decay_log = device.decay_log;
    request.time_bias = device.time_bias;
    request.normalization_weight = device.normalization_weight;
    request.convolution_state = device.committed_convolution;
    request.recurrent_state = device.committed_recurrent;
    request.next_convolution_state = device.candidate_convolution;
    request.next_recurrent_state = device.candidate_recurrent;
    request.output = device.output;
    YVEX_TEST_ASSERT(
        operations->gated_delta_workspace_required(
            &plan, tokens, &workspace, &err) == YVEX_OK && workspace > sizeof(int),
        "derive bounded chunked CUDA gated-delta workspace");
    rc = operations->gated_delta_execute(
        backend, &plan, &request, &result, &facts, &err);
    if (rc != YVEX_OK || !result.complete || result.token_count != tokens ||
        result.execution_chunks != expected_chunks ||
        facts.kernel_launches != expected_chunks * 2ull ||
        facts.download_count != expected_chunks ||
        facts.queue_synchronizations + facts.device_synchronizations != expected_chunks ||
        facts.temporary_bytes > workspace ||
        facts.temporary_bytes <
            (tokens < 64ull ? tokens : 64ull) * plan.qkv_width *
                sizeof(float) + sizeof(int) ||
        facts.state_bytes !=
            (plan.convolution_state_bytes + plan.recurrent_state_bytes) *
                (committed ? 2ull : 1ull))
        fprintf(stderr,
                "gated-delta facts rc=%d where=%s error=%s complete=%d "
                "tokens=%llu chunks=%llu kernels=%llu downloads=%llu sync=%llu "
                "temporary=%llu/%llu state=%llu/%llu\n",
                rc, yvex_error_where(&err), yvex_error_message(&err), result.complete,
                result.token_count, result.execution_chunks, facts.kernel_launches,
                facts.download_count,
                facts.queue_synchronizations + facts.device_synchronizations,
                facts.temporary_bytes, workspace, facts.state_bytes,
                (plan.convolution_state_bytes + plan.recurrent_state_bytes) *
                    (committed ? 2ull : 1ull));
    YVEX_TEST_ASSERT(
        rc == YVEX_OK && result.complete && result.token_count == tokens &&
            result.execution_chunks == expected_chunks &&
            facts.kernel_launches == expected_chunks * 2ull &&
            facts.download_count == expected_chunks &&
            facts.queue_synchronizations + facts.device_synchronizations ==
                expected_chunks &&
            facts.temporary_bytes <= workspace &&
            facts.temporary_bytes >=
                (tokens < 64ull ? tokens : 64ull) * plan.qkv_width *
                    sizeof(float) + sizeof(int) &&
            facts.state_bytes ==
                (plan.convolution_state_bytes + plan.recurrent_state_bytes) *
                    (committed ? 2ull : 1ull),
        "CUDA gated-delta publishes exact chunk, state, and operation facts");
    YVEX_TEST_ASSERT(
        yvex_backend_tensor_read(
            backend, device.candidate_convolution, host.observed_convolution,
            plan.convolution_state_bytes, &err) == YVEX_OK &&
            yvex_backend_tensor_read(
                backend, device.candidate_recurrent, host.observed_recurrent,
                plan.recurrent_state_bytes, &err) == YVEX_OK &&
            yvex_backend_tensor_read(
                backend, device.output, host.observed_output,
                tokens * plan.value_width * sizeof(float), &err) == YVEX_OK,
        "read CUDA gated-delta candidate state and output");
    difference = gated_delta_compare(
        host.candidate_convolution, host.observed_convolution,
        plan.convolution_state_values, tolerance);
    YVEX_TEST_ASSERT(difference >= 0.0,
                     "CUDA causal convolution state matches portable authority");
    if (difference > *maximum_difference) *maximum_difference = difference;
    difference = gated_delta_compare(
        host.candidate_recurrent, host.observed_recurrent,
        plan.recurrent_state_values, tolerance);
    YVEX_TEST_ASSERT(difference >= 0.0,
                     "CUDA recurrent matrix state matches portable authority");
    if (difference > *maximum_difference) *maximum_difference = difference;
    difference = gated_delta_compare(
        host.output, host.observed_output, tokens * plan.value_width, tolerance);
    YVEX_TEST_ASSERT(difference >= 0.0,
                     "CUDA gated-delta output matches portable authority");
    if (difference > *maximum_difference) *maximum_difference = difference;
    if (committed) {
        memset(host.observed_convolution, 0, (size_t)plan.convolution_state_bytes);
        memset(host.observed_recurrent, 0, (size_t)plan.recurrent_state_bytes);
        request.cancel_requested = gated_delta_cancelled;
        request.cancel_context = &request;
        YVEX_TEST_ASSERT(
            operations->gated_delta_execute(
                backend, &plan, &request, &result, &facts, &err) ==
                YVEX_ERR_CANCELLED && result.cancelled &&
                !yvex_device_tensor_is_written(device.candidate_convolution) &&
                !yvex_device_tensor_is_written(device.candidate_recurrent) &&
                yvex_backend_tensor_read(
                    backend, device.committed_convolution,
                    host.observed_convolution, plan.convolution_state_bytes,
                    &err) == YVEX_OK &&
                yvex_backend_tensor_read(
                    backend, device.committed_recurrent,
                    host.observed_recurrent, plan.recurrent_state_bytes,
                    &err) == YVEX_OK &&
                memcmp(host.committed_convolution, host.observed_convolution,
                       (size_t)plan.convolution_state_bytes) == 0 &&
                memcmp(host.committed_recurrent, host.observed_recurrent,
                       (size_t)plan.recurrent_state_bytes) == 0,
            "pre-launch cancellation leaves committed mixed sequence state unchanged");
        request.cancel_requested = NULL;
        request.cancel_context = NULL;
        host.qkv[0] = NAN;
        YVEX_TEST_ASSERT(
            yvex_backend_tensor_write(
                backend, device.qkv, host.qkv,
                tokens * plan.qkv_width * sizeof(float), &err) == YVEX_OK &&
                operations->gated_delta_execute(
                    backend, &plan, &request, &result, &facts, &err) ==
                    YVEX_ERR_FORMAT && !result.complete &&
                !yvex_device_tensor_is_written(device.candidate_convolution) &&
                !yvex_device_tensor_is_written(device.candidate_recurrent) &&
                yvex_backend_tensor_read(
                    backend, device.committed_convolution,
                    host.observed_convolution, plan.convolution_state_bytes,
                    &err) == YVEX_OK &&
                yvex_backend_tensor_read(
                    backend, device.committed_recurrent,
                    host.observed_recurrent, plan.recurrent_state_bytes,
                    &err) == YVEX_OK &&
                memcmp(host.committed_convolution, host.observed_convolution,
                       (size_t)plan.convolution_state_bytes) == 0 &&
                memcmp(host.committed_recurrent, host.observed_recurrent,
                       (size_t)plan.recurrent_state_bytes) == 0,
            "non-finite CUDA recurrence refuses unpublished candidate state");
    }
    YVEX_TEST_ASSERT(
        gated_delta_device_close(backend, &device, &err) == YVEX_OK,
        "release CUDA gated-delta fixture tensors");
    gated_delta_host_close(&host);
    return 0;
}

static int short_conv_cancel(void *opaque)
{
    unsigned int *calls = opaque;
    return ++*calls == 3u; /* Admission, first chunk, then cancellation. */
}

static int short_conv_device_case(yvex_backend *backend)
{
    enum { WIDTH = 4, TAPS = 3, ROWS = 65, OUTPUT = WIDTH * ROWS, INPUT = OUTPUT * 3 };
    float projection[INPUT], weight[WIDTH * TAPS], expected[OUTPUT], expected_state[WIDTH * TAPS];
    float observed[OUTPUT], observed_state[WIDTH * TAPS], cpu_state[WIDTH * TAPS], cpu_output[OUTPUT];
    yvex_device_tensor *input = NULL, *w = NULL, *output = NULL, *status = NULL;
    yvex_short_conv_geometry g;
    yvex_short_conv_device_request request = {0};
    yvex_short_conv_device_result result;
    yvex_backend_operation_facts facts;
    yvex_sequence_state_binding binding;
    yvex_sequence_state_plan plan = {.schema_version = YVEX_SEQUENCE_STATE_SCHEMA_V1,
        .bindings = &binding, .binding_count = 1u};
    yvex_sequence_state *owner = NULL;
    yvex_sequence_device_state_view committed;
    yvex_sequence_device_state_output candidate;
    yvex_runtime_transaction_participant participant;
    const yvex_backend_transformer_operations *ops = yvex_backend_transformer_operations_get(backend);
    yvex_error err;
    char before[65], after[65];
#define CONV_CHECK(condition) YVEX_TEST_ASSERT((condition), "CUDA double-gated convolution: " #condition)
    for (unsigned int i = 0u; i < INPUT; ++i) projection[i] = (float)((int)(i % 17u) - 8) / 8.0f;
    for (unsigned int i = 0u; i < WIDTH * TAPS; ++i) weight[i] = (float)((int)(i % 7u) - 3) / 8.0f;
    /* Independent direct time-indexed oracle. Dyadic products/sums are exactly
     * representable, with explicit source BF16 publications, no native recurrence. */
    for (int row = 0; row < ROWS; ++row) for (int c = 0; c < WIDTH; ++c) {
        double sum = 0.0;
        for (int tap = 0; tap < TAPS; ++tap) {
            int t = row + tap - TAPS + 1;
            if (t >= 0) {
                float product = projection[t * WIDTH * 3 + c] * projection[t * WIDTH * 3 + 2 * WIDTH + c];
                sum += (double)yvex_quant_bf16_decode(yvex_quant_bf16_encode(product)) * weight[c * TAPS + tap];
            }
        }
        float convolved = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)sum));
        expected[row * WIDTH + c] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
            convolved * projection[row * WIDTH * 3 + WIDTH + c]));
    }
    for (int c = 0; c < WIDTH; ++c) for (int k = 0; k < TAPS; ++k) {
        int t = ROWS + k - TAPS;
        expected_state[c * TAPS + k] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
            projection[t * WIDTH * 3 + c] * projection[t * WIDTH * 3 + 2 * WIDTH + c]));
    }
    CONV_CHECK(ops && ops->short_conv_execute);
    CONV_CHECK(yvex_short_conv_geometry_seal(&g, WIDTH, TAPS, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_binding_seal(&binding, 7u, g.state_values, 0u, g.identity, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_open_for_backend(&owner, &plan, YVEX_BACKEND_KIND_CUDA, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_attach_device(owner, backend, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_participant(owner, &participant, &err) == YVEX_OK);
    CONV_CHECK(gated_delta_tensor_open(backend, "short-projection", INPUT, projection, &input, &err) == YVEX_OK);
    CONV_CHECK(gated_delta_tensor_open(backend, "short-weight", WIDTH * TAPS, weight, &w, &err) == YVEX_OK);
    CONV_CHECK(gated_delta_tensor_open(backend, "short-output", OUTPUT, NULL, &output, &err) == YVEX_OK);
    CONV_CHECK(gated_delta_tensor_open(backend, "short-status", 1u, NULL, &status, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_begin(owner, 0u, ROWS, &err) == YVEX_OK);
    CONV_CHECK(yvex_sequence_state_device_layer(owner, 7u, &committed, &candidate, &err) == YVEX_OK);
    CONV_CHECK(!committed.convolution && !committed.recurrent && !candidate.recurrent);
    request = (yvex_short_conv_device_request){.rows = ROWS, .projection = input, .weight = w,
        .next_state = candidate.convolution, .output = output, .status = status};
    CONV_CHECK(yvex_sequence_state_stage(owner, 7u, &err) == YVEX_ERR_STATE);
    CONV_CHECK(ops->short_conv_execute(backend, &g, &request, &result, &facts, &err) == YVEX_OK);
    CONV_CHECK(result.complete && result.rows_completed == ROWS && result.execution_chunks == 2u);
    CONV_CHECK(facts.temporary_bytes == 4u && facts.kernel_launches == 2u && facts.state_bytes == 48u);
    CONV_CHECK(yvex_backend_tensor_read(backend, output, observed, sizeof(observed), &err) == YVEX_OK);
    CONV_CHECK(yvex_backend_tensor_read(backend, candidate.convolution, observed_state, sizeof(observed_state), &err) == YVEX_OK);
    CONV_CHECK(!memcmp(expected, observed, sizeof(expected)) && !memcmp(expected_state, observed_state, sizeof(expected_state)));
    CONV_CHECK(yvex_sequence_state_stage(owner, 7u, &err) == YVEX_OK);
    CONV_CHECK(participant.prepare(participant.context, &err) == YVEX_OK);
    participant.publish(participant.context);
    CONV_CHECK(yvex_sequence_state_committed_identity(owner, before, &err) == YVEX_OK);
    for (unsigned int bad = 0u; bad < 6u; ++bad) {
        unsigned int calls = 0u;
        CONV_CHECK(yvex_sequence_state_begin(owner, ROWS, ROWS, &err) == YVEX_OK);
        CONV_CHECK(yvex_sequence_state_device_layer(owner, 7u, &committed, &candidate, &err) == YVEX_OK);
        request.state = committed.convolution; request.next_state = candidate.convolution;
        request.status = bad == 3u ? candidate.convolution : status;
        request.cancel_requested = bad == 4u ? short_conv_cancel : NULL; request.cancel_context = &calls;
        projection[0] = bad == 0u ? NAN : bad == 1u ? 0.1f : -1.0f;
        weight[0] = bad == 2u ? INFINITY : -0.375f;
        CONV_CHECK(yvex_backend_tensor_write(backend, input, projection, sizeof(projection), &err) == YVEX_OK);
        CONV_CHECK(yvex_backend_tensor_write(backend, w, weight, sizeof(weight), &err) == YVEX_OK);
        int rc = ops->short_conv_execute(backend, &g, &request, &result, &facts, &err);
        CONV_CHECK(rc == (bad == 5u ? YVEX_OK : bad == 4u ? YVEX_ERR_CANCELLED : YVEX_ERR_FORMAT));
        if (bad < 5u) {
            CONV_CHECK(!result.complete && !candidate.convolution->is_written);
            CONV_CHECK(yvex_sequence_state_stage(owner, 7u, &err) == YVEX_ERR_STATE);
        } else {
            yvex_short_conv_cpu_request cpu = {.rows = ROWS, .projection = projection, .weight = weight,
                .state = expected_state, .projection_capacity = INPUT, .weight_capacity = WIDTH * TAPS,
                .state_capacity = WIDTH * TAPS, .next_state = cpu_state, .next_state_capacity = WIDTH * TAPS,
                .output = cpu_output, .output_capacity = OUTPUT};
            yvex_short_conv_cpu_result cpu_result;
            CONV_CHECK(yvex_short_conv_execute_cpu(&g, &cpu, &cpu_result, &err) == YVEX_OK);
            CONV_CHECK(yvex_backend_tensor_read(backend, output, observed, sizeof(observed), &err) == YVEX_OK);
            CONV_CHECK(yvex_backend_tensor_read(backend, candidate.convolution, observed_state, sizeof(observed_state), &err) == YVEX_OK);
            CONV_CHECK(!memcmp(cpu_output, observed, sizeof(observed)) && !memcmp(cpu_state, observed_state, sizeof(cpu_state)));
        }
        CONV_CHECK(participant.abort(participant.context, &err) == YVEX_OK);
        CONV_CHECK(yvex_sequence_state_committed_identity(owner, after, &err) == YVEX_OK && !strcmp(before, after));
    }
    CONV_CHECK(yvex_sequence_state_close_checked(&owner, &err) == YVEX_OK);
    CONV_CHECK(yvex_backend_tensor_release(backend, &input, &err) == YVEX_OK);
    CONV_CHECK(yvex_backend_tensor_release(backend, &w, &err) == YVEX_OK);
    CONV_CHECK(yvex_backend_tensor_release(backend, &output, &err) == YVEX_OK);
    CONV_CHECK(yvex_backend_tensor_release(backend, &status, &err) == YVEX_OK);
#undef CONV_CHECK
    return 0;
}

int yvex_cuda_test_sequence_mixer(void)
{
    const yvex_gated_delta_requirement small = {
        .schema_version = YVEX_SEQUENCE_MIXER_GATED_DELTA_SCHEMA_V2,
        .output_normalization_weight_convention =
            YVEX_NORMALIZATION_WEIGHT_ONE_PLUS,
        .query_heads = 2ull, .key_heads = 2ull, .value_heads = 4ull,
        .key_head_dimension = 4ull, .value_head_dimension = 3ull,
        .convolution_kernel = 3ull,
        .projected_dtype = YVEX_DTYPE_F32,
        .convolution_state_dtype = YVEX_DTYPE_F32,
        .recurrent_state_dtype = YVEX_DTYPE_F32,
        .accumulation_dtype = YVEX_DTYPE_F32,
        .output_dtype = YVEX_DTYPE_F32,
        .numeric_contract = YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
        .qk_normalization_epsilon = 1e-6,
        .output_normalization_epsilon = 1e-6,
        .query_scale = 0.5,
        .deterministic = 1};
    const yvex_gated_delta_requirement wide = {
        .schema_version = YVEX_SEQUENCE_MIXER_GATED_DELTA_SCHEMA_V2,
        .output_normalization_weight_convention =
            YVEX_NORMALIZATION_WEIGHT_DIRECT,
        .query_heads = 16ull, .key_heads = 16ull, .value_heads = 48ull,
        .key_head_dimension = 128ull, .value_head_dimension = 128ull,
        .convolution_kernel = 4ull,
        .projected_dtype = YVEX_DTYPE_F32,
        .convolution_state_dtype = YVEX_DTYPE_F32,
        .recurrent_state_dtype = YVEX_DTYPE_F32,
        .accumulation_dtype = YVEX_DTYPE_F32,
        .output_dtype = YVEX_DTYPE_F32,
        .numeric_contract = YVEX_SEQUENCE_MIXER_NUMERIC_F32_RECURRENCE,
        .qk_normalization_epsilon = 1e-6,
        .output_normalization_epsilon = 1e-6,
        .query_scale = 0.08838834764831845,
        .deterministic = 1};
    yvex_backend *backend = NULL;
    yvex_backend_options options = {0};
    yvex_error err;
    double maximum_difference = 0.0;
    int rc;
    options.kind = YVEX_BACKEND_KIND_CUDA;
    rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) {
        fprintf(stderr, "SKIP: CUDA unavailable: %s\n", yvex_error_message(&err));
        return 77;
    }
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open CUDA gated-delta backend");
    YVEX_TEST_ASSERT(short_conv_device_case(backend) == 0, "double-gated BF16 convolution and state lifecycle");
    YVEX_TEST_ASSERT(
        gated_delta_parity_case(
            backend, &small, 65ull, 0, 2ull, 6e-5, &maximum_difference) == 0,
        "chunked causal-convolution and recurrence prefill match CPU authority");
    YVEX_TEST_ASSERT(
        gated_delta_parity_case(
            backend, &small, 1ull, 1, 1ull, 6e-5, &maximum_difference) == 0,
        "cached single-token recurrence consumes immutable committed state");
    YVEX_TEST_ASSERT(
        gated_delta_parity_case(
            backend, &wide, 1ull, 0, 1ull, 8e-5, &maximum_difference) == 0,
        "16/16/48 by 128 production geometry matches portable authority");
    YVEX_TEST_ASSERT(
        gated_delta_device_state_lifecycle(backend, &small) == 0,
        "runtime-owned CUDA recurrence retains exact transactional state");
    fprintf(stderr, "CUDA gated-delta maximum absolute difference: %.9g\n",
            maximum_difference);
    YVEX_TEST_ASSERT(
        yvex_backend_close_checked(&backend, &err) == YVEX_OK && !backend,
        "close CUDA gated-delta backend cleanly");
    return 0;
}
