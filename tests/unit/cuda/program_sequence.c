/* A complete imported recurrent computation executes through physical SSA work.
 * Chunk/single preservation is not upstream model conformance. */
#include "tests/test.h"
#include "tests/support/program_sequence.h"
#include <yvex/internal/program_sequence.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/internal/quant_numeric.h>
#include "src/graph/private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    yvex_backend *backend;
    yvex_ir_module *module;
    yvex_program_physical *program;
    yvex_program_device *device;
    yvex_program_kernels *kernels;
    yvex_program_sequence *sequence;
    yvex_sequence_state *state;
    yvex_device_tensor *parameters, *output;
    yvex_program_kernel_parameter weights[64];
    size_t weight_count;
    yvex_program_device_argument args[7];
    yvex_runtime_session_view session;
    yvex_model_engine_view model;
    yvex_attention_layer_plan attention_layer;
    yvex_attention_plan attention;
    yvex_attention_state_provider attention_state;
    yvex_graph_attention_capacity_plan *capacity;
    yvex_runtime_state_residency *residency;
    size_t input_count;
    int hybrid, cancel, cancel_after_transition, attention_prepared;
} sequence_fixture;

static int sequence_cancel(void *context)
{
    return ((sequence_fixture *)context)->cancel;
}

static int sequence_invoke(void *context, const yvex_program_device_invocation *r,
                             yvex_backend_operation_facts *facts, yvex_error *err)
{
    sequence_fixture *f = context;
    int attention = !strcmp(r->step->implementation, "gated_causal.bf16.v1");
    if (attention || !strcmp(r->step->implementation, "gated_delta.bf16.f32state.v1")) {
        yvex_program_sequence_request options = {yvex_ir_source_identity(f->module), sequence_cancel, f};
        int rc = yvex_program_sequence_invoke(f->sequence, r, &options, facts, err);
        if (rc == YVEX_OK && f->cancel_after_transition == (attention ? 2 : 1)) f->cancel = 1;
        return rc;
    }
    {
        int rc = yvex_program_kernels_invoke(f->kernels, r, facts, err);
        if (rc != YVEX_OK) fprintf(stderr, "step=%zu implementation=%s rows=%llu input_width=%llu output_width=%llu: %s\n",
            r->step_index, r->step->implementation, r->rows, r->values[r->step->operands[0]].dims[1],
            r->values[r->step->results[0]].dims[1], yvex_error_message(err));
        return rc;
    }
}

static float sequence_weight_value(const char *name, const yvex_ir_type *type, size_t index)
{
    unsigned long long width = type->shape[type->rank - 1u].extent;
    if (strstr(name, "embed_tokens")) return (float)((int)(index % 11u) - 5) / 16.0f;
    if (strstr(name, "conv1d")) return index % 3u == 2u ? 1.0f : 0.125f;
    if (type->rank == 1u)
        return strstr(name, "linear_attn.norm") || strstr(name, "q_norm") || strstr(name, "k_norm") ? 1.0f : 0.0f;
    return index % width == (index / width) % 32u ? 0.125f : 0.0f;
}

static int sequence_parameters(sequence_fixture *f, yvex_error *err)
{
    unsigned long long total = 0u, cursor = 0u;
    size_t i;
    unsigned char *mapped = NULL;
    yvex_backend_tensor_desc d = {.name = "sequence-program-parameters", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    int rc;
    for (i = 0u; i < yvex_ir_operation_count(f->module); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(f->module, (yvex_ir_id)i);
        const yvex_ir_type *t;
        unsigned long long n = 1u;
        unsigned int axis;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        t = yvex_ir_type_at(f->module, yvex_ir_value_at(f->module, op->results[0])->type);
        for (axis = 0u; axis < t->rank; ++axis) n *= t->shape[axis].extent;
        /* Parameter subranges obey the artifact fixture's physical alignment;
         * tiny one-head scale tensors must not misalign subsequent matrices. */
        total = (total + 255u) & ~255ull;
        total += n * 2u;
    }
    d.dims[0] = d.bytes = total;
    rc = yvex_backend_resident_alloc(f->backend, &d, &f->parameters, &mapped, err);
    if (rc != YVEX_OK) return rc;
    for (i = 0u; i < yvex_ir_operation_count(f->module); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(f->module, (yvex_ir_id)i);
        const yvex_ir_type *t;
        const char *name;
        unsigned long long n = 1u, index, width;
        unsigned int axis;
        if (strcmp(op->definition->name, "core.parameter")) continue;
        cursor = (cursor + 255u) & ~255ull;
        t = yvex_ir_type_at(f->module, yvex_ir_value_at(f->module, op->results[0])->type);
        name = yvex_ir_attribute_get(f->module, (yvex_ir_id)i, "parameter")->value.text;
        for (axis = 0u; axis < t->rank; ++axis) n *= t->shape[axis].extent;
        width = t->shape[t->rank - 1u].extent;
        for (index = 0u; index < n; ++index) {
            unsigned short bits = yvex_quant_bf16_encode(sequence_weight_value(name, t, (size_t)index));
            mapped[cursor + 2u * index] = (unsigned char)bits;
            mapped[cursor + 2u * index + 1u] = (unsigned char)(bits >> 8u);
        }
        if (f->weight_count >= 64u) return YVEX_ERR_BOUNDS;
        f->weights[f->weight_count] = (yvex_program_kernel_parameter){f->weight_count,
            {.encoded = mapped + cursor, .encoded_bytes = n * 2u, .row_count = n / width,
             .row_width = width, .row_bytes = width * 2u, .qtype = YVEX_GGUF_QTYPE_BF16}};
        f->weight_count++;
        cursor += n * 2u;
    }
    return yvex_backend_resident_attach(f->backend, mapped, total, f->parameters, 1u, err);
}

static int sequence_attention_open(sequence_fixture *f, yvex_error *err)
{
    yvex_graph_attention_capacity_request request = {.scope = YVEX_ATTENTION_PROBE_SCOPE_FULL,
        .token_count = 8u, .execution_count = 1u};
    yvex_attention_failure failure = {0};
    const yvex_graph_attention_capacity_layer *capacity;
    int rc;
    if (!f->hybrid) return YVEX_OK;
    /* A bounded provider fixture, not a source/package admission claim. Geometry
     * deliberately matches the one attention op imported in the third block. */
    f->attention_layer = (yvex_attention_layer_plan){.ordinal = 0u, .layer_index = 2u,
        .predictor_index = YVEX_ATTENTION_NO_TENSOR_INDEX, .tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER,
        .attention_class = YVEX_ATTENTION_CLASS_SWA, .compute_contract = YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
        .sliding_window = 8u, .query_heads = 1u, .kv_heads = 1u, .head_dimension = 32u,
        .rope_head_dimension = 16u, .hidden_dimension = 32u,
        .position = {.theta = 10000u, .scaling_factor = 1u}};
    f->attention.layers = &f->attention_layer;
    f->attention.layer_count = 1u;
    f->attention.summary = (yvex_attention_summary){.status = YVEX_ATTENTION_STATUS_EXECUTION_READY,
        .tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER, .layer_count = 1u, .swa_layer_count = 1u,
        .history_contract_ready = 1, .state_delta_contract_ready = 1};
    yvex_core_text_copy(f->attention.summary.attention_plan_identity,
        sizeof(f->attention.summary.attention_plan_identity), yvex_program_physical_summary_get(f->program)->identity);
    f->model.attention = &f->attention;
    rc = yvex_graph_attention_capacity_plan_build(&f->capacity, &f->attention, &request, err);
    if (rc == YVEX_OK) rc = yvex_attention_state_provider_open_persistent(&f->attention, 0u,
        &f->attention_state, &failure, err);
    capacity = yvex_graph_attention_capacity_plan_layer(f->capacity, 0u);
    if (rc == YVEX_OK && !capacity) rc = YVEX_ERR_STATE;
    if (rc == YVEX_OK) rc = f->attention_state.prepare(f->attention_state.context, 0u,
        &capacity->recipe, NULL, &failure, err);
    if (rc == YVEX_OK) rc = yvex_runtime_state_residency_prepare(&f->residency, f->backend,
        f->capacity, &f->attention_state, 0u, 0u, 0u, 0u, err);
    f->session.attention_state_provider = &f->attention_state;
    f->session.state_residency = f->residency;
    return rc;
}

static int sequence_open(sequence_fixture *f)
{
    static const yvex_program_device_kernel kernels[] = {
        {"embedding.bf16.v1", sequence_invoke}, {"linear.bf16.f32acc.v1", sequence_invoke},
        {"rms_norm.bf16.v1", sequence_invoke}, {"silu_product.bf16.v1", sequence_invoke},
        {"add.bf16.v1", sequence_invoke}, {"gated_delta.bf16.f32state.v1", sequence_invoke},
        {"gated_causal.bf16.v1", sequence_invoke}};
    yvex_sequence_state_plan state;
    yvex_backend_tensor_desc d = {.name = "sequence-program-output", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {3u, 32u}, .bytes = 96u * sizeof(float)};
    yvex_error err = {0};
    size_t i;
    int rc = f->hybrid ? test_sequence_program_kind(&f->module, &f->program, 1, &err) :
        test_sequence_program(&f->module, &f->program, &err);
    if (rc == YVEX_OK) rc = sequence_parameters(f, &err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_open(&f->kernels, f->program, f->weights, f->weight_count,
        f->backend, 0u, 0u, &err);
    if (rc == YVEX_OK && !yvex_program_physical_sequence_state(f->program, &state)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = yvex_sequence_state_open_for_backend(&f->state, &state, YVEX_BACKEND_KIND_CUDA, &err);
    if (rc == YVEX_OK) rc = yvex_sequence_state_attach_device(f->state, f->backend, &err);
    f->session.backend = f->backend;
    f->session.sequence_state = f->state;
    if (rc == YVEX_OK) rc = sequence_attention_open(f, &err);
    if (rc == YVEX_OK) rc = yvex_program_sequence_open(&f->sequence, f->program, &f->model, &f->session,
        f->kernels, 3u, 0u, 0u, &err);
    if (rc == YVEX_OK) rc = yvex_program_device_open(&f->device, f->program, f->backend, 3u, 0u, 0u,
        kernels, sizeof(kernels) / sizeof(kernels[0]), f, &err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(f->backend, &d, &f->output, &err);
    if (rc == YVEX_OK) {
        unsigned long long before_host, before_device, after_host, after_device;
        yvex_program_kernels_resources(f->kernels, &before_host, &before_device);
        YVEX_TEST_ASSERT(yvex_program_kernels_prepare(f->kernels, 1u, 1u, 1u, &err) == YVEX_ERR_BOUNDS,
            "current owner budget refuses preparation before numerical dispatch");
        yvex_program_kernels_resources(f->kernels, &after_host, &after_device);
        YVEX_TEST_ASSERT(before_host == after_host && before_device == after_device,
            "capacity refusal preserves prepared resource accounting");
        yvex_error_clear(&err);
        rc = yvex_program_kernels_prepare(f->kernels, 1u, 0u, 0u, &err);
    }
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(f->kernels, 3u, 0u, 0u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "sequence program prepare: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "whole imported recurrent program binds physical parameters and state providers");
    f->input_count = yvex_program_physical_summary_get(f->program)->input_count;
    YVEX_TEST_ASSERT(f->input_count == (f->hybrid ? 7u : 6u) && state.binding_count == 2u,
        "state roots, not decoder-layer records, define provider allocation");
    for (i = 2u; i < f->input_count; ++i) f->args[i].state_handle = i;
    return 0;
}

static int sequence_attention_prepare(void *context, yvex_error *err)
{
    sequence_fixture *f = context;
    int rc = yvex_runtime_state_residency_prepare_commit(f->residency, err);
    if (rc == YVEX_OK) rc = f->attention_state.prepare_commit(f->attention_state.context, NULL, err);
    f->attention_prepared = rc == YVEX_OK;
    return rc;
}

static void sequence_attention_publish(void *context)
{
    sequence_fixture *f = context;
    yvex_runtime_state_residency_publish_commit(f->residency);
    f->attention_state.publish_commit(f->attention_state.context);
    f->attention_prepared = 0;
}

static int sequence_attention_abort(void *context, yvex_error *err)
{
    sequence_fixture *f = context;
    if (f->attention_prepared) f->attention_state.cancel_commit(f->attention_state.context);
    f->attention_prepared = 0;
    yvex_runtime_state_residency_abort(f->residency);
    return f->attention_state.abort(f->attention_state.context, NULL, err);
}

static int sequence_run(sequence_fixture *f, const unsigned int *tokens, unsigned long long start,
    unsigned long long rows, float *output, yvex_program_device_result *result, yvex_error *err)
{
    yvex_runtime_transaction_participant participants[2];
    yvex_device_tensor view, *outputs[] = {&view};
    int rc;
    if (!yvex_backend_tensor_f32_subview(f->output, 0u, rows * 32u, &view)) return YVEX_ERR_BOUNDS;
    view.rank = 2u; view.dims[0] = rows; view.dims[1] = 32u;
    f->args[0].indices = tokens; f->args[1].index = start;
    rc = yvex_sequence_state_begin(f->state, start, rows, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_program_device_run(f->device, rows, f->args, f->input_count, outputs, 1u, sequence_cancel, f, result, err);
    if (yvex_sequence_state_participant(f->state, &participants[0], NULL) != YVEX_OK) return YVEX_ERR_STATE;
    participants[1] = (yvex_runtime_transaction_participant){f, sequence_attention_prepare,
        sequence_attention_publish, sequence_attention_abort};
    rc = yvex_runtime_transaction_resolve(participants, f->hybrid ? 2u : 1u, rc, err);
    if (rc == YVEX_OK && output) rc = yvex_backend_tensor_read(f->backend, &view, output, rows * 32u * sizeof(float), err);
    return rc;
}

static int sequence_reset(sequence_fixture *f, yvex_error *err)
{
    int rc = yvex_sequence_state_reset(f->state, err);
    if (rc == YVEX_OK && f->hybrid) rc = f->attention_state.reset(f->attention_state.context, NULL, err);
    if (rc == YVEX_OK && f->hybrid) rc = yvex_runtime_state_residency_reset(f->residency, err);
    return rc;
}

static int sequence_snapshot(sequence_fixture *f, float values[2624], size_t *count, yvex_error *err)
{
    yvex_runtime_transaction_participant participant;
    size_t i, cursor = 0u;
    int rc = yvex_sequence_state_begin(f->state, 3u, 1u, err);
    if (rc != YVEX_OK) return rc;
    /* Read committed device views through the provider contract; the diagnostic
     * transaction is aborted without staging or publishing any candidate. */
    for (i = 2u; rc == YVEX_OK && i < 6u; i += 2u) {
        yvex_sequence_device_state_view committed = {0};
        yvex_sequence_device_state_output candidate = {0};
        rc = yvex_sequence_state_device_layer(f->state, i, &committed, &candidate, err);
        const yvex_device_tensor *tensors[] = {committed.convolution, committed.recurrent};
        for (size_t j = 0u; rc == YVEX_OK && j < 2u; ++j) {
            const yvex_device_tensor *t = tensors[j];
            if (!t || t->bytes % sizeof(float) || t->bytes / sizeof(float) > 2624u - cursor) rc = YVEX_ERR_BOUNDS;
            if (rc == YVEX_OK) {
                rc = yvex_backend_tensor_read(f->backend, t, values + cursor, t->bytes, err);
                cursor += (size_t)(t->bytes / sizeof(float));
            }
        }
    }
    if (yvex_sequence_state_participant(f->state, &participant, NULL) != YVEX_OK) return YVEX_ERR_STATE;
    if (participant.abort(participant.context, NULL) != YVEX_OK) return YVEX_ERR_STATE;
    if (rc == YVEX_OK && f->hybrid) {
        const yvex_attention_history_view *kv = f->attention_state.view(f->attention_state.context,
            0u, YVEX_ATTENTION_STATE_VIEW_COMMITTED);
        if (!kv || !kv->local_kv || kv->local_tail_count != 3u || kv->local_kv_stride < 64u || cursor != 2432u)
            return YVEX_ERR_STATE;
        for (i = 0u; i < 3u; ++i) {
            if (!kv->local_positions || kv->local_positions[i] != i) return YVEX_ERR_STATE;
            memcpy(values + cursor, kv->local_kv + i * kv->local_kv_stride, 64u * sizeof(float));
            cursor += 64u;
        }
    }
    *count = cursor;
    return rc;
}

static int sequence_checks(sequence_fixture *f)
{
    const unsigned int tokens[] = {1u, 2u, 3u};
    yvex_program_device_result result;
    yvex_sequence_state_summary state;
    yvex_error err = {0};
    float chunk[96], singles[96], maximum = 0.0f;
    float chunk_state[2624], single_state[2624], state_maximum = 0.0f;
    size_t state_count = 0u, observed_count = 0u, nonzero = 0u;
    unsigned long long expected_operations = 0u;
    size_t i;
    for (i = 0u; i < yvex_program_physical_summary_get(f->program)->step_count; ++i)
        if (strcmp(yvex_program_physical_step_at(f->program, i)->implementation, "parameter.encoded.v1")) expected_operations++;
    int rc = sequence_run(f, tokens, 0u, 3u, chunk, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "sequence program invocation: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations == expected_operations && expected_operations > 20u,
        "whole forward executes every admitted numerical operation, not only its FFN");
    YVEX_TEST_ASSERT(yvex_sequence_state_summary_copy(f->state, &state, &err) == YVEX_OK &&
        state.committed_position == 3u && state.generation == 1u, "all compiled state successors commit once");
    YVEX_TEST_ASSERT(sequence_snapshot(f, chunk_state, &state_count, &err) == YVEX_OK &&
        state_count == (f->hybrid ? 2624u : 2432u), "snapshot exact committed convolution/recurrent/KV values");
    YVEX_TEST_ASSERT(sequence_reset(f, &err) == YVEX_OK, "independent single-token replay starts from reset");
    for (i = 0u; i < 3u; ++i)
        YVEX_TEST_ASSERT(sequence_run(f, &tokens[i], i, 1u, &singles[i * 32u], &result, &err) == YVEX_OK,
            "single-token program invocation advances persistent provider state");
    for (i = 0u; i < 96u; ++i) {
        float difference = fabsf(chunk[i] - singles[i]);
        if (difference > maximum) maximum = difference;
        YVEX_TEST_ASSERT(isfinite(chunk[i]) && isfinite(singles[i]) && difference == 0.0f,
            "chunk and single invocation preserve identical BF16 results");
    }
    YVEX_TEST_ASSERT(sequence_snapshot(f, single_state, &observed_count, &err) == YVEX_OK &&
        observed_count == state_count, "single-token replay exposes the same state geometry");
    for (i = 0u; i < state_count; ++i) {
        float difference = fabsf(chunk_state[i] - single_state[i]);
        if (difference > state_maximum) state_maximum = difference;
        nonzero += chunk_state[i] != 0.0f;
        YVEX_TEST_ASSERT(isfinite(chunk_state[i]) && isfinite(single_state[i]) && difference == 0.0f,
            "chunk/single preservation includes every committed state value, not only hidden output");
    }
    YVEX_TEST_ASSERT(nonzero > 0u, "state comparison cannot pass from an all-zero computation");
    f->cancel_after_transition = f->hybrid ? 2 : 1;
    rc = sequence_run(f, tokens, 3u, 1u, NULL, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_CANCELLED && result.operations > 0u &&
        yvex_sequence_state_summary_copy(f->state, &state, NULL) == YVEX_OK &&
        state.committed_position == 3u && !state.transaction_active,
        "abort after one state transition leaves both committed state owners unchanged");
    if (f->hybrid) {
        yvex_graph_attention_state_summary kv_summary;
        YVEX_TEST_ASSERT(f->attention_state.summary(f->attention_state.context, &kv_summary, &err) == YVEX_OK &&
            kv_summary.committed_sequence_length == 3u && !kv_summary.transaction_active,
            "cancellation after attention aborts KV and both earlier recurrent transitions together");
    }
    YVEX_TEST_ASSERT(!f->output->is_written && sequence_snapshot(f, single_state, &observed_count, &err) == YVEX_OK &&
        observed_count == state_count && memcmp(single_state, chunk_state, state_count * sizeof(float)) == 0,
        "cancelled candidates publish no output and leave every committed state byte unchanged");
    f->cancel_after_transition = f->cancel = 0;
    YVEX_TEST_ASSERT(sequence_run(f, tokens, 3u, 1u, singles, &result, &err) == YVEX_OK,
        "retry after cancelled candidate uses the same admitted program");
    printf("CUDA imported %s program: operations=%llu states=%zu, nonzero BF16 parameters; "
        "chunk(3) vs single(1+1+1): values=96 max_abs=%g tolerance=0; state_values=%zu nonzero=%zu max_abs=%g tolerance=0; committed_position=3; "
        "cancel after transition -> abort, retry -> commit; cross-path preservation, not upstream conformance\n",
        f->hybrid ? "hybrid" : "recurrent", result.operations, f->input_count - 2u, (double)maximum,
        state_count, nonzero, (double)state_maximum);
    return 0;
}

static int sequence_fixture_check(int hybrid)
{
    sequence_fixture f = {.hybrid = hybrid};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend_memory_stats before, after;
    yvex_error err = {0};
    int rc = yvex_backend_open(&f.backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_get_memory_stats(f.backend, &before, &err) == YVEX_OK &&
        sequence_open(&f) == 0 && sequence_checks(&f) == 0, "real CUDA program/state execution");
    YVEX_TEST_ASSERT(yvex_program_device_close(&f.device, &err) == YVEX_OK &&
        yvex_program_sequence_close(&f.sequence, &err) == YVEX_OK &&
        yvex_program_kernels_close(&f.kernels, &err) == YVEX_OK &&
        yvex_sequence_state_close_checked(&f.state, &err) == YVEX_OK &&
        yvex_runtime_state_residency_close(&f.residency, &err) == YVEX_OK &&
        (!f.hybrid || f.attention_state.release(&f.attention_state.context, &err) == YVEX_OK) &&
        yvex_backend_resident_detach(f.backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(f.backend, &f.parameters, &err) == YVEX_OK &&
        yvex_backend_tensor_release(f.backend, &f.output, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(f.backend, &after, &err) == YVEX_OK &&
        after.allocated_bytes == before.allocated_bytes, "every program and candidate allocation returns to baseline");
    yvex_program_physical_close(&f.program);
    yvex_graph_attention_capacity_plan_close(&f.capacity);
    yvex_ir_module_close(&f.module);
    YVEX_TEST_ASSERT(yvex_backend_close_checked(&f.backend, &err) == YVEX_OK, "backend closes last");
    return 0;
}

int yvex_cuda_test_program_sequence(void)
{
    int rc = sequence_fixture_check(0);
    return rc ? rc : sequence_fixture_check(1);
}
