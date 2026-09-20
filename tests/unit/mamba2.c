/* Source-contract fixtures are not model acquisition or hosted-generation evidence. */
#include "tests/test.h"
#include <yvex/internal/families/mamba2.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/core.h>
#include <yvex/internal/decoder_execution.h>
#include <yvex/internal/deployment.h>
#include <yvex/internal/family_catalog.h>
#include <yvex/internal/compilation.h>
#include <yvex/internal/program.h>
#include <yvex/internal/program_device.h>
#include <yvex/internal/program_kernels.h>
#include <yvex/internal/program_physical.h>
#include <yvex/internal/program_sequence.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/internal/logits.h>
#include <yvex/internal/graph.h>
#include <yvex/internal/runtime.h>
#include <yvex/internal/source_catalog.h>
#include <yvex/qtype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAMBA_TEST_PARAMETER_CAP 32u
#define MAMBA_TEST_STATE_CAP 128u

typedef struct {
    yvex_backend *backend;
    yvex_program_kernels *kernels, *head_kernels;
    yvex_program_sequence *sequence;
    yvex_program_device *device, *head_device;
    yvex_sequence_state *state;
    yvex_device_tensor *output, *logits;
    yvex_program_kernel_parameter parameters[MAMBA_TEST_PARAMETER_CAP];
    unsigned char *encoded[MAMBA_TEST_PARAMETER_CAP];
    size_t parameter_count, input_count;
    yvex_program_device_argument args[6];
    yvex_runtime_session_view session;
    yvex_model_engine_view model;
    const char *input_identity;
    unsigned int cancel, cancel_after_ssd, ssd_count;
} mamba_execution_fixture;

static int mamba_cancel(void *context)
{
    return ((mamba_execution_fixture *)context)->cancel;
}

static int mamba_invoke(void *context, const yvex_program_device_invocation *request,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    mamba_execution_fixture *f = context;
    if (!strcmp(request->step->implementation, "selective_ssd.cpu.f32state.v1")) {
        yvex_program_sequence_request options = {f->input_identity, mamba_cancel, f};
        int rc = yvex_program_sequence_invoke(f->sequence, request, &options, facts, err);
        f->ssd_count += rc == YVEX_OK;
        if (rc == YVEX_OK && f->cancel_after_ssd == f->ssd_count) f->cancel = 1u;
        return rc;
    }
    return yvex_program_kernels_invoke(
        !strcmp(yvex_program_physical_summary_get(request->program)->entry, "output") ?
        f->head_kernels : f->kernels, request, facts, err);
}

static int mamba_write(const char *root, const char *name, const char *text)
{
    char path[768], metadata[768], oid[41];
    yvex_error err;
    FILE *fp;
    snprintf(path, sizeof(path), "%s/%s", root, name);
    fp = fopen(path, "wb");
    if (!fp) return 0;
    if (fputs(text, fp) < 0 || fclose(fp)) return 0;
    if (yvex_source_git_blob_oid_file(path, oid, &err) != YVEX_OK) return 0;
    snprintf(metadata, sizeof(metadata), "%s/.cache/huggingface/download/%s.metadata", root, name);
    fp = fopen(metadata, "wb");
    if (!fp) return 0;
    if (fprintf(fp, "%s\n%s\n0\n", YVEX_MAMBA2_REVISION, oid) < 0 || fclose(fp)) return 0;
    return 1;
}

static int mamba_fixture_config(const char *root, const char *model_type, unsigned int groups,
    int norm_before_gate, const char *extra)
{
    char config[2048];
    snprintf(config, sizeof(config),
        "{\"architectures\":[\"Mamba2ForCausalLM\"],\"model_type\":\"%s\","
        "\"hidden_size\":4,\"num_hidden_layers\":2,\"vocab_size\":4,\"expand\":2,"
        "\"num_heads\":4,\"head_dim\":2,\"state_size\":2,\"n_groups\":%u,"
        "\"conv_kernel\":3,\"chunk_size\":2,\"bos_token_id\":0,\"eos_token_id\":0,"
        "\"pad_token_id\":0,\"layer_norm_epsilon\":0.00001,\"rms_norm\":true,"
        "\"residual_in_fp32\":true,\"tie_word_embeddings\":false,\"use_bias\":false,"
        "\"use_conv_bias\":true,\"norm_before_gate\":%s,\"hidden_act\":\"silu\","
        "\"torch_dtype\":\"bfloat16\",\"time_step_limit\":[0.0,Infinity]%s}", model_type, groups,
        norm_before_gate ? "true" : "false", extra ? extra : "");
    return mamba_write(root, "config.json", config);
}

static int mamba_inventory(const yvex_mamba2_architecture *a, yvex_native_weight_table **out)
{
    const char *const global[] = {"backbone.embeddings.weight", "backbone.norm_f.weight", "lm_head.weight"};
    const char *const suffix[] = {"norm.weight", "mixer.in_proj.weight", "mixer.conv1d.weight",
        "mixer.conv1d.bias", "mixer.A_log", "mixer.D", "mixer.dt_bias", "mixer.norm.weight",
        "mixer.out_proj.weight"};
    const unsigned long long shapes[12][3] = {
        {4,4,0}, {4,0,0}, {4,4,0}, {4,0,0}, {28,4,0}, {16,1,3},
        {16,0,0}, {4,0,0}, {4,0,0}, {4,0,0}, {8,0,0}, {4,8,0}};
    unsigned long long i, layer, offset = 0;
    yvex_error err;
    *out = calloc(1, sizeof(**out));
    if (!*out) return 0;
    for (layer = 0; layer <= a->layer_count; ++layer)
        for (i = layer == a->layer_count ? 0u : 3u;
             i < (layer == a->layer_count ? 3u : 12u); ++i) {
            char name[128];
            unsigned int rank = shapes[i][2] ? 3u : shapes[i][1] ? 2u : 1u;
            unsigned long long bytes = 2u, j;
            if (i < 3u) snprintf(name, sizeof(name), "%s", global[i]);
            else snprintf(name, sizeof(name), "backbone.layers.%llu.%s", layer, suffix[i - 3u]);
            for (j = 0; j < rank; ++j) bytes *= shapes[i][j];
            if (yvex_native_weight_table_add(*out, name, "fixture.safetensors", "BF16",
                    rank, shapes[i], offset, offset + bytes, &err) != YVEX_OK) return 0;
            offset += bytes;
        }
    return yvex_native_weight_table_finalize(*out, &err) == YVEX_OK;
}

static float mamba_parameter_value(const char *name, const yvex_ir_type *type, size_t index)
{
    unsigned long long width = type->shape[type->rank - 1u].extent;
    if (strstr(name, "norm.weight")) return 1.0f;
    if (strstr(name, ".A_log")) return -1.0f;
    if (strstr(name, ".D")) return 0.5f;
    if (strstr(name, ".dt_bias")) return -1.5f;
    if (strstr(name, "conv1d.bias")) return 0.0f;
    if (strstr(name, "conv1d.weight")) return index % width == width - 1u ? 0.5f : 0.125f;
    if (strstr(name, "embeddings") || strstr(name, "lm_head"))
        return (float)((int)(index % 7u) - 3) / 8.0f;
    return index % width == (index / width) % width ? 0.25f : 0.03125f;
}

static int mamba_parameters_open(mamba_execution_fixture *f, const yvex_ir_module *program,
    const yvex_program_parameter_binding *bindings, const char *const *names, size_t count)
{
    size_t i;
    if (!f || count > MAMBA_TEST_PARAMETER_CAP) return 0;
    for (i = 0u; i < count; ++i) {
        const yvex_ir_value *value = yvex_ir_value_at(program, bindings[i].semantic_value);
        const yvex_ir_type *type = value ? yvex_ir_type_at(program, value->type) : NULL;
        unsigned long long elements = 1u, width, index;
        if (!type || type->kind != YVEX_IR_TENSOR || !type->rank || bindings[i].qtype != YVEX_GGUF_QTYPE_BF16)
            return 0;
        for (unsigned int axis = 0u; axis < type->rank; ++axis) elements *= type->shape[axis].extent;
        if (!elements || elements > SIZE_MAX / 2u) return 0;
        width = type->shape[type->rank - 1u].extent;
        f->encoded[i] = malloc((size_t)elements * 2u);
        if (!f->encoded[i]) return 0;
        for (index = 0u; index < elements; ++index) {
            unsigned short bits = yvex_quant_bf16_encode(mamba_parameter_value(names[i], type, (size_t)index));
            f->encoded[i][2u * index] = (unsigned char)bits;
            f->encoded[i][2u * index + 1u] = (unsigned char)(bits >> 8u);
        }
        f->parameters[i] = (yvex_program_kernel_parameter){.tensor_id = bindings[i].tensor_id,
            .weight = {.encoded = f->encoded[i], .encoded_bytes = elements * 2u,
                .row_count = elements / width, .row_width = width, .row_bytes = width * 2u,
                .qtype = YVEX_GGUF_QTYPE_BF16}};
    }
    f->parameter_count = count;
    return 1;
}

static int mamba_state_snapshot(const mamba_execution_fixture *f, const yvex_sequence_state_plan *plan,
    float values[MAMBA_TEST_STATE_CAP], size_t *count, yvex_error *err)
{
    size_t cursor = 0u;
    for (size_t i = 0u; i < plan->binding_count; ++i) {
        yvex_sequence_state_view view = {0};
        const yvex_sequence_state_binding *binding = plan->bindings + i;
        if (cursor + binding->convolution_state_values + binding->recurrent_state_values > MAMBA_TEST_STATE_CAP ||
            yvex_sequence_state_committed(f->state, binding->layer_index, &view, err) != YVEX_OK) return 0;
        memcpy(values + cursor, view.convolution, binding->convolution_state_values * sizeof(float));
        cursor += (size_t)binding->convolution_state_values;
        memcpy(values + cursor, view.recurrent, binding->recurrent_state_values * sizeof(float));
        cursor += (size_t)binding->recurrent_state_values;
    }
    *count = cursor;
    return 1;
}

static int mamba_forward_run(mamba_execution_fixture *f, const unsigned int *tokens,
    unsigned long long start, unsigned long long rows, float *output,
    yvex_program_device_result *result, yvex_error *err)
{
    yvex_runtime_transaction_participant participant;
    yvex_device_tensor view, *outputs[] = {&view};
    int rc;
    if (!yvex_backend_tensor_f32_subview(f->output, 0u, rows * 4u, &view)) return YVEX_ERR_BOUNDS;
    view.rank = 2u; view.dims[0] = rows; view.dims[1] = 4u;
    f->args[0].indices = tokens; f->args[1].index = start;
    f->ssd_count = 0u;
    rc = yvex_sequence_state_begin(f->state, start, rows, err);
    if (rc == YVEX_OK) rc = yvex_program_device_run(f->device, rows, f->args, f->input_count,
        outputs, 1u, mamba_cancel, f, result, err);
    if (yvex_sequence_state_participant(f->state, &participant, NULL) != YVEX_OK) return YVEX_ERR_STATE;
    rc = yvex_runtime_transaction_resolve(&participant, 1u, rc, err);
    f->output->is_written = view.is_written;
    if (rc == YVEX_OK && output)
        rc = yvex_backend_tensor_read(f->backend, &view, output, rows * 4u * sizeof(float), err);
    return rc;
}

static int mamba_execution_open(mamba_execution_fixture *f, const yvex_ir_module *program,
    const yvex_program_physical *forward, const yvex_program_physical *head,
    const yvex_program_parameter_binding *bindings, const char *const *names, size_t count,
    const char *identity, yvex_sequence_state_plan *state, yvex_error *err)
{
    static const yvex_program_device_kernel implementations[] = {
        {"embedding.encoded.f32.v1", mamba_invoke}, {"rms_norm.f32.v1", mamba_invoke},
        {"linear.encoded.f32.v1", mamba_invoke}, {"selective_ssd.cpu.f32state.v1", mamba_invoke},
        {"add.f32.v1", mamba_invoke}};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CPU};
    yvex_backend_tensor_desc hidden = {.name = "mamba-hidden", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {3u, 4u}, .bytes = 12u * sizeof(float)};
    yvex_backend_tensor_desc logits = {.name = "mamba-logits", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {3u, 4u}, .bytes = 12u * sizeof(float)};
    int rc = mamba_parameters_open(f, program, bindings, names, count) ? YVEX_OK : YVEX_ERR_NOMEM;
    f->input_identity = identity;
    if (rc == YVEX_OK) rc = yvex_backend_open(&f->backend, &options, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_open(&f->kernels, forward, f->parameters,
        f->parameter_count, f->backend, 0u, 0u, err);
    if (rc == YVEX_OK && !yvex_program_physical_sequence_state(forward, state)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = yvex_sequence_state_open_for_backend(&f->state, state, YVEX_BACKEND_KIND_CPU, err);
    f->session.backend = f->backend; f->session.sequence_state = f->state;
    if (rc == YVEX_OK) rc = yvex_program_sequence_open(&f->sequence, forward, &f->model, &f->session,
        f->kernels, 3u, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_program_device_open(&f->device, forward, f->backend, 3u, 0u, 0u,
        implementations, sizeof(implementations) / sizeof(*implementations), f, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(f->kernels, 1u, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(f->kernels, 3u, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_open(&f->head_kernels, head, f->parameters,
        f->parameter_count, f->backend, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_program_device_open(&f->head_device, head, f->backend, 3u, 0u, 0u,
        implementations, sizeof(implementations) / sizeof(*implementations), f, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(f->backend, &hidden, &f->output, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(f->backend, &logits, &f->logits, err);
    f->input_count = yvex_program_physical_summary_get(forward)->input_count;
    for (size_t i = 2u; i < f->input_count && i < 6u; ++i) f->args[i].state_handle = i;
    return rc;
}

static int mamba_execution_close(mamba_execution_fixture *f, yvex_error *err)
{
    int rc = yvex_program_device_close(&f->head_device, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_close(&f->head_kernels, err);
    if (rc == YVEX_OK) rc = yvex_program_device_close(&f->device, err);
    if (rc == YVEX_OK) rc = yvex_program_sequence_close(&f->sequence, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_close(&f->kernels, err);
    if (rc == YVEX_OK) rc = yvex_sequence_state_close_checked(&f->state, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_release(f->backend, &f->output, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_release(f->backend, &f->logits, err);
    if (rc == YVEX_OK) rc = yvex_backend_close_checked(&f->backend, err);
    for (size_t i = 0u; i < f->parameter_count; ++i) free(f->encoded[i]);
    return rc;
}

static int mamba_execution_check(const yvex_ir_module *program, const yvex_program_physical *forward,
    const yvex_program_physical *head, const yvex_program_parameter_binding *bindings,
    const char *const *names, size_t count, const char *identity)
{
    mamba_execution_fixture f = {0};
    yvex_sequence_state_plan state = {0};
    yvex_sequence_state_summary summary = {0};
    yvex_program_device_result result = {0}, head_result = {0};
    yvex_program_device_argument head_argument = {0};
    yvex_device_tensor head_input, head_output, *head_outputs[] = {&head_output};
    yvex_error err = {0};
    const unsigned int tokens[] = {1u, 2u, 3u};
    float chunk[12], singles[12], logits[12];
    float chunk_state[MAMBA_TEST_STATE_CAP], single_state[MAMBA_TEST_STATE_CAP];
    size_t state_count = 0u, single_count = 0u, nonzero = 0u;
    float output_max = 0.0f, state_max = 0.0f;
    int rc = mamba_execution_open(&f, program, forward, head, bindings, names, count, identity, &state, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Mamba CPU execution open: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && f.input_count == 6u && state.binding_count == 2u,
        "pure SSM physical program binds CPU kernels and common two-layer state");
    rc = mamba_forward_run(&f, tokens, 0u, 3u, chunk, &result, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Mamba CPU forward: %s\n", yvex_error_message(&err));
    if (rc == YVEX_OK && yvex_sequence_state_summary_copy(f.state, &summary, &err) == YVEX_OK &&
        !(result.operations > 10u && f.output->is_written && summary.committed_position == 3u &&
          summary.generation == 1u))
        fprintf(stderr, "Mamba CPU forward facts: operations=%llu written=%d position=%llu generation=%llu\n",
            result.operations, f.output->is_written, summary.committed_position, summary.generation);
    YVEX_TEST_ASSERT(rc == YVEX_OK && result.operations > 10u && f.output->is_written &&
        yvex_sequence_state_summary_copy(f.state, &summary, &err) == YVEX_OK &&
        summary.committed_position == 3u && summary.generation == 1u,
        "complete two-layer forward commits all state through physical SSA");
    YVEX_TEST_ASSERT(mamba_state_snapshot(&f, &state, chunk_state, &state_count, &err) &&
        state_count == MAMBA_TEST_STATE_CAP, "committed state exposes exact convolution and recurrence geometry");
    head_input = *f.output; head_input.rank = 2u; head_input.dims[0] = 3u; head_input.dims[1] = 4u;
    head_output = *f.logits; head_output.rank = 2u; head_output.dims[0] = 3u; head_output.dims[1] = 4u;
    head_argument.tensor = &head_input;
    rc = yvex_program_device_run(f.head_device, 3u, &head_argument, 1u, head_outputs, 1u,
        NULL, NULL, &head_result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && head_result.operations == 1u &&
        yvex_backend_tensor_read(f.backend, &head_output, logits, sizeof(logits), &err) == YVEX_OK,
        "source-bound LM head consumes compiled hidden output as a separate common entrypoint");
    for (size_t i = 0u; i < 12u; ++i)
        YVEX_TEST_ASSERT(isfinite(chunk[i]) && isfinite(logits[i]), "hidden and logits remain finite");
    YVEX_TEST_ASSERT(yvex_sequence_state_reset(f.state, &err) == YVEX_OK,
        "common state reset precedes retained single-token continuation");
    for (size_t i = 0u; i < 3u; ++i)
        YVEX_TEST_ASSERT(mamba_forward_run(&f, tokens + i, i, 1u, singles + 4u * i, &result, &err) == YVEX_OK,
            "single-token continuation advances without prefix replay");
    for (size_t i = 0u; i < 12u; ++i) {
        float difference = fabsf(chunk[i] - singles[i]);
        if (difference > output_max) output_max = difference;
        YVEX_TEST_ASSERT(difference == 0.0f, "chunk and retained single-token hidden output agree exactly");
    }
    YVEX_TEST_ASSERT(mamba_state_snapshot(&f, &state, single_state, &single_count, &err) &&
        single_count == state_count, "single-token continuation preserves exact state geometry");
    for (size_t i = 0u; i < state_count; ++i) {
        float difference = fabsf(chunk_state[i] - single_state[i]);
        if (difference > state_max) state_max = difference;
        nonzero += chunk_state[i] != 0.0f;
        YVEX_TEST_ASSERT(difference == 0.0f, "chunk and retained single-token state agree exactly");
    }
    YVEX_TEST_ASSERT(nonzero > 0u, "state preservation evidence cannot pass from all-zero state");
    f.cancel_after_ssd = 1u;
    rc = mamba_forward_run(&f, tokens, 3u, 1u, NULL, &result, &err);
    YVEX_TEST_ASSERT(rc == YVEX_ERR_CANCELLED && result.operations > 0u && !f.output->is_written &&
        yvex_sequence_state_summary_copy(f.state, &summary, NULL) == YVEX_OK &&
        summary.committed_position == 3u && !summary.transaction_active,
        "cancellation after candidate SSD work aborts state and publishes no hidden result");
    YVEX_TEST_ASSERT(mamba_state_snapshot(&f, &state, single_state, &single_count, &err) &&
        !memcmp(single_state, chunk_state, state_count * sizeof(float)),
        "cancelled candidate preserves every committed state byte");
    f.cancel_after_ssd = f.cancel = 0u;
    YVEX_TEST_ASSERT(mamba_forward_run(&f, tokens, 3u, 1u, singles, &result, &err) == YVEX_OK,
        "retry after abort executes the same admitted program");
    printf("Mamba2 physical CPU program: layers=2 operations=%llu state_values=%zu nonzero=%zu; "
           "chunk(3) vs single(1+1+1): hidden_max_abs=%g state_max_abs=%g tolerance=0; "
           "LM-head logits=12 finite; cancel-after-SSD -> abort/unpublished; retry -> commit\n",
           result.operations, state_count, nonzero, (double)output_max, (double)state_max);
    YVEX_TEST_ASSERT(mamba_execution_close(&f, &err) == YVEX_OK,
        "compiled program, common state and CPU backend close without retained resources");
    return 0;
}

static int mamba_contract(const char *root)
{
    const yvex_mamba2_api *api = yvex_model_register_mamba2();
    yvex_source_verification verification = {0};
    yvex_mamba2_architecture a;
    yvex_mamba2_inventory inventory;
    yvex_mamba2_tensor_binding binding;
    yvex_native_weight_table *table = NULL;
    yvex_native_weight_info bad;
    yvex_error err;
    verification.verified = verification.config_valid = 1;
    snprintf(verification.resolved_source_path, sizeof(verification.resolved_source_path), "%s", root);
    snprintf(verification.repository_id, sizeof(verification.repository_id), "%s", YVEX_MAMBA2_REPOSITORY);
    snprintf(verification.revision, sizeof(verification.revision), "%s", YVEX_MAMBA2_REVISION);
    YVEX_TEST_ASSERT(api->open(&verification, &a, &err) == YVEX_OK && a.architecture_complete &&
        a.layer_count == 2u && a.mixer.width == 8u && a.mixer.convolution_state_values == 48u &&
        a.mixer.recurrent_state_values == 16u && a.token_policy_conflict &&
        a.normalization_policy_conflict && a.tokenizer_bos == 1u && a.tokenizer_eos == 2u &&
        a.effective_bos == 1u && a.effective_eos == 2u && a.effective_pad == ULLONG_MAX &&
        a.token_policy_resolved && a.normalization_policy_resolved &&
        a.token_authority == YVEX_MAMBA2_TOKEN_AUTHORITY_MISTRAL_TOKENIZER_V1 &&
        a.normalization_authority == YVEX_MAMBA2_NORMALIZATION_AUTHORITY_MISTRAL_RECIPE_V1 &&
        a.tokenizer_assets_match && !a.tokenizer_has_pad && !a.chat_template_present &&
        a.mixer.requirement.normalization_groups == 2u && !a.mixer.requirement.norm_before_gate,
        "pinned tokenizer and Mistral recipe resolve execution authority while retaining raw conflicts");
    YVEX_TEST_ASSERT(mamba_inventory(&a, &table) &&
        api->tensor_audit(&a, table, &inventory, &err) == YVEX_OK &&
        inventory.complete && inventory.tensors == 21u,
        "bidirectional role coverage is exact");
    {
        yvex_ir_module *program = NULL, *again = NULL;
        yvex_program_execution *execution = NULL;
        yvex_program_physical *physical = NULL, *head = NULL;
        yvex_program_parameter_binding bindings[32];
        const char *parameter_names[32];
        size_t binding_count = 0u;
        yvex_program_token_interface interface = {0};
        yvex_sequence_state_plan state = {0};
        yvex_core_bytes dump = {.maximum = 65536u};
        const yvex_ir_function *forward, *output;
        /* Synthetic role fixture identity, not a published provider snapshot. */
        YVEX_TEST_ASSERT(yvex_mamba2_program_build(&program, &a, inventory.role_identity, &err) == YVEX_OK &&
                         yvex_mamba2_program_build(&again, &a, inventory.role_identity, &err) == YVEX_OK,
                         "pure SSM source projects into verified typed program");
        forward = yvex_ir_function_at(program, 0u);
        output = yvex_ir_function_at(program, 1u);
        YVEX_TEST_ASSERT(forward && forward->result_count == 5u &&
                         output && output->result_count == 1u &&
                         strcmp(yvex_ir_identity(program), yvex_ir_identity(again)) == 0,
                         "forward publishes hidden plus four state versions; output head is a separate entrypoint");
        YVEX_TEST_ASSERT(yvex_program_execution_compile(&execution, program, &err) == YVEX_OK &&
            yvex_program_execution_entry_count(execution) == 2u &&
            yvex_program_execution_entry_at(execution, 0u)->result_count == 5u,
            "pure SSM compiles forward and output dependency forms without attention");
        for (size_t i = 0u; i < yvex_ir_operation_count(program); ++i) {
            const yvex_ir_operation *parameter = yvex_ir_operation_at(program, (yvex_ir_id)i);
            if (strcmp(parameter->definition->name, "core.parameter")) continue;
            parameter_names[binding_count] =
                yvex_ir_attribute_get(program, (yvex_ir_id)i, "parameter")->value.text;
            bindings[binding_count] = (yvex_program_parameter_binding){
                parameter->results[0], binding_count, YVEX_GGUF_QTYPE_BF16};
            binding_count++;
        }
        int physical_rc = binding_count < 32u ?
            yvex_program_physical_compile(&physical, execution, "forward", bindings, binding_count,
                                          inventory.role_identity, &err) : YVEX_ERR_BOUNDS;
        if (physical_rc != YVEX_OK) fprintf(stderr, "Mamba physical forward: %s\n", yvex_error_message(&err));
        YVEX_TEST_ASSERT(physical_rc == YVEX_OK &&
            yvex_program_physical_token_interface(physical, &interface, &err) == YVEX_OK &&
            interface.vocabulary_size == 4u && interface.hidden_width == 4u &&
            interface.state_inputs == 4u && interface.recurrent_operations == 2u &&
            interface.attention_operations == 0u &&
            yvex_program_physical_sequence_state(physical, &state) && state.binding_count == 2u,
            "pure-SSM forward legalizes into physical SSA and common recurrent state without KV");
        YVEX_TEST_ASSERT(yvex_program_physical_compile(&head, execution, "output", bindings, binding_count,
                                                       inventory.role_identity, &err) == YVEX_OK &&
            yvex_program_physical_summary_get(head)->result_count == 1u,
            "source-bound LM head legalizes as its own common program");
        YVEX_TEST_ASSERT(mamba_execution_check(program, physical, head, bindings,
            parameter_names, binding_count, inventory.role_identity) == 0,
            "pure SSM forward and output execute through common physical/runtime owners");
        yvex_program_physical_close(&head);
        yvex_program_physical_close(&physical);
        yvex_program_execution_close(&execution);
        YVEX_TEST_ASSERT(yvex_ir_print(program, &dump, &err) == YVEX_OK &&
                         yvex_core_bytes_append(&dump, "", 1u) &&
                         strstr((char *)dump.data, "sequence.selective_ssd.v1") &&
                         strstr((char *)dump.data, "state<ssm.selective; 4x2x2xf32>") &&
                         strstr((char *)dump.data, "state<convolution.causal; 16x3xf32>") &&
                         strstr((char *)dump.data, "func @output") &&
                         !strstr((char *)dump.data, "mamba2.mixer") &&
                         !strstr((char *)dump.data, "source_obligation") &&
                         !strstr((char *)dump.data, "attention") && !strstr((char *)dump.data, "rope"),
                         "canonical sequence IR contains no family runtime or invented Transformer semantics");
        free(dump.data);
        yvex_ir_module_close(&again);
        YVEX_TEST_ASSERT(yvex_mamba2_program_build(&again, &a, a.architecture_identity, &err) == YVEX_OK &&
                         strcmp(yvex_ir_identity(program), yvex_ir_identity(again)) != 0,
                         "changing source identity changes program identity even with identical geometry");
        yvex_ir_module_close(&again);
        YVEX_TEST_ASSERT(yvex_mamba2_program_build(&again, &a, "", &err) != YVEX_OK && !again,
                         "source identity cannot be inferred from architecture geometry");
        yvex_ir_module_close(&program);
    }
    bad = *yvex_native_weight_table_at(table, 0);
    bad.dims[0]++;
    YVEX_TEST_ASSERT(api->tensor_classify(&a, &bad, &binding, &err) != YVEX_OK,
        "wrong tensor shape rejected");
    bad = *yvex_native_weight_table_at(table, 0);
    bad.dtype = YVEX_NATIVE_DTYPE_F32;
    YVEX_TEST_ASSERT(api->tensor_classify(&a, &bad, &binding, &err) != YVEX_OK,
        "wrong source dtype rejected");
    bad = *yvex_native_weight_table_at(table, 0);
    bad.name = "backbone.layers.0.self_attn.q_proj.weight";
    YVEX_TEST_ASSERT(api->tensor_classify(&a, &bad, &binding, &err) != YVEX_OK,
        "attention tensors cannot be relabeled as SSM");
    table->count--;
    YVEX_TEST_ASSERT(api->tensor_audit(&a, table, &inventory, &err) != YVEX_OK && !inventory.complete,
        "missing required tensor prevents complete coverage");
    table->count++;
    yvex_native_weight_table_close(table);
    YVEX_TEST_ASSERT(yvex_graph_execution_find(0, 0, YVEX_MAMBA2_TARGET),
        "family catalog publishes the compiler-owned pure-SSM execution boundary");
    verification.revision[0] = '0';
    YVEX_TEST_ASSERT(api->open(&verification, &a, &err) != YVEX_OK, "revision drift rejected");
    snprintf(verification.revision, sizeof(verification.revision), "%s", YVEX_MAMBA2_REVISION);
    YVEX_TEST_ASSERT(mamba_write(root, "tokenizer.model.v3", "different-tokenizer-fixture") &&
        api->open(&verification, &a, &err) != YVEX_OK &&
        mamba_write(root, "tokenizer.model.v3", "identical-tokenizer-fixture"),
        "divergent tokenizer assets fail closed");
    YVEX_TEST_ASSERT(mamba_write(root, "tokenizer_config.json", "{\"add_bos_token\":true,"
            "\"add_eos_token\":false,\"bos_token\":\"<s>\",\"eos_token\":\"</s>\","
            "\"unk_token\":\"<unk>\",\"pad_token\":\"<s>\",\"chat_template\":null,"
            "\"tokenizer_class\":\"LlamaTokenizer\"}") &&
        api->open(&verification, &a, &err) != YVEX_OK &&
        mamba_write(root, "tokenizer_config.json", "{\"add_bos_token\":true,\"add_eos_token\":false,"
            "\"bos_token\":\"<s>\",\"eos_token\":\"</s>\",\"unk_token\":\"<unk>\","
            "\"pad_token\":null,\"chat_template\":null,\"tokenizer_class\":\"LlamaTokenizer\"}"),
        "impossible padding alias is not promoted into tokenizer truth");
    YVEX_TEST_ASSERT(mamba_write(root, "generation_config.json",
            "{\"bos_token_id\":0,\"eos_token_id\":3,\"pad_token_id\":1}") &&
        api->open(&verification, &a, &err) != YVEX_OK &&
        mamba_write(root, "generation_config.json",
            "{\"bos_token_id\":0,\"eos_token_id\":2,\"pad_token_id\":1}"),
        "generation stop policy that disagrees with tokenizer truth is rejected");
    YVEX_TEST_ASSERT(mamba_write(root, "tokenizer_config.json", "{\"add_bos_token\":true,"
            "\"add_eos_token\":false,\"bos_token\":\"<s>\",\"eos_token\":\"</s>\","
            "\"unk_token\":\"<unk>\",\"pad_token\":null,\"chat_template\":\"invented\","
            "\"tokenizer_class\":\"LlamaTokenizer\"}") &&
        api->open(&verification, &a, &err) != YVEX_OK &&
        mamba_write(root, "tokenizer_config.json", "{\"add_bos_token\":true,\"add_eos_token\":false,"
            "\"bos_token\":\"<s>\",\"eos_token\":\"</s>\",\"unk_token\":\"<unk>\","
            "\"pad_token\":null,\"tokenizer_class\":\"LlamaTokenizer\"}") &&
        api->open(&verification, &a, &err) == YVEX_OK && !a.chat_template_present,
        "absent chat template is authoritative absence; a fabricated template is rejected");
    YVEX_TEST_ASSERT(mamba_fixture_config(root, "mamba2", 2u, 0, "") &&
        api->open(&verification, &a, &err) != YVEX_OK,
        "unpinned normalization declaration cannot silently select another whole-model policy");
    YVEX_TEST_ASSERT(mamba_fixture_config(root, "mamba", 2u, 1, "") &&
        api->open(&verification, &a, &err) != YVEX_OK, "Mamba1 declaration rejected");
    YVEX_TEST_ASSERT(mamba_fixture_config(root, "mamba2", 3u, 1, "") &&
        api->open(&verification, &a, &err) != YVEX_OK, "head/group and params disagreement rejected");
    YVEX_TEST_ASSERT(mamba_fixture_config(root, "mamba2", 2u, 1, ",\"attn_layer_idx\":[0]") &&
        api->open(&verification, &a, &err) != YVEX_OK,
        "hybrid attention declaration cannot be admitted as pure Mamba2");
    return 0;
}

static int mamba_acquired_program(const yvex_mamba2_architecture *architecture,
    const yvex_mamba2_inventory *inventory, const char *source_identity, yvex_error *err)
{
    yvex_ir_module *program = NULL;
    yvex_program_execution *execution = NULL;
    yvex_program_physical *forward = NULL, *head = NULL;
    yvex_program_parameter_binding *bindings = NULL;
    yvex_program_token_interface interface = {0};
    yvex_sequence_state_plan state = {0};
    yvex_sequence_state_geometry resources = {0};
    const yvex_program_physical_summary *forward_summary, *head_summary;
    size_t index, count = 0u;
    int rc = yvex_mamba2_program_build(&program, architecture, source_identity, err);
    if (rc == YVEX_OK) {
        for (index = 0u; index < yvex_ir_operation_count(program); ++index) {
            const yvex_ir_operation *operation = yvex_ir_operation_at(program, (yvex_ir_id)index);
            count += operation && !strcmp(operation->definition->name, "core.parameter");
        }
        bindings = calloc(count, sizeof(*bindings));
        if (!bindings) rc = YVEX_ERR_NOMEM;
    }
    count = 0u;
    for (index = 0u; rc == YVEX_OK && index < yvex_ir_operation_count(program); ++index) {
        const yvex_ir_operation *operation = yvex_ir_operation_at(program, (yvex_ir_id)index);
        if (!operation || strcmp(operation->definition->name, "core.parameter")) continue;
        bindings[count] = (yvex_program_parameter_binding){operation->results[0], count, YVEX_GGUF_QTYPE_BF16};
        count++;
    }
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, program, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&forward, execution, "forward", bindings, count,
                                                           inventory->role_identity, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&head, execution, "output", bindings, count,
                                                           inventory->role_identity, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_token_interface(forward, &interface, err);
    if (rc == YVEX_OK && !yvex_program_physical_sequence_state(forward, &state)) rc = YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = yvex_sequence_state_plan_measure(&state, &resources, err);
    forward_summary = rc == YVEX_OK ? yvex_program_physical_summary_get(forward) : NULL;
    head_summary = rc == YVEX_OK ? yvex_program_physical_summary_get(head) : NULL;
    YVEX_TEST_ASSERT(rc == YVEX_OK && count == inventory->tensors && count == 579u &&
        interface.vocabulary_size == 32768u && interface.hidden_width == 4096u &&
        interface.state_inputs == 128u && interface.recurrent_operations == 64u &&
        interface.attention_operations == 0u && state.binding_count == 64u &&
        resources.convolution_values == 2621440u && resources.recurrent_values == 67108864u &&
        forward_summary && head_summary && head_summary->result_count == 1u,
        "exact acquired roles lower through pure-SSM execution/physical IR with common state geometry");
    printf("acquired-mamba2-program: parameters=%zu forward_steps=%zu output_steps=%zu "
           "state_bindings=%llu attention=0 KV=0 RoPE=0 dense_FFN=0 "
           "committed_state_bytes=%llu candidate_state_bytes=%llu hosted=false\n",
           count, forward_summary->step_count, head_summary->step_count, state.binding_count,
           resources.committed_bytes, resources.candidate_bytes);
    yvex_program_physical_close(&head);
    yvex_program_physical_close(&forward);
    yvex_program_execution_close(&execution);
    yvex_ir_module_close(&program);
    free(bindings);
    return 0;
}

typedef struct {
    int requested;
} mamba_exact_cancellation;

static int mamba_exact_cancel(void *context)
{
    return ((mamba_exact_cancellation *)context)->requested;
}

static int mamba_exact_input_identity(
    unsigned int token, unsigned long long position,
    char output[YVEX_SHA256_HEX_CAP])
{
    yvex_sha256 hash;
    unsigned char digest[YVEX_SHA256_DIGEST_BYTES];
    yvex_sha256_init(&hash);
    if (!yvex_sha256_update_text(&hash, "yvex.test.mamba2.exact-token.v1") ||
        !yvex_sha256_update_u64(&hash, token) ||
        !yvex_sha256_update_u64(&hash, position) ||
        !yvex_sha256_final(&hash, digest))
        return 0;
    yvex_sha256_hex(digest, output);
    return 1;
}

static int mamba_exact_profile(
    const yvex_runtime_session_summary *session,
    yvex_execution_workload_profile *workload,
    yvex_runtime_execution_profile *profile, yvex_error *err)
{
    yvex_runtime_execution_profile_request request = {0};
    workload->schema_version = YVEX_EXECUTION_WORKLOAD_PROFILE_SCHEMA_V1;
    workload->kind = YVEX_EXECUTION_WORKLOAD_INTERACTIVE_LATENCY;
    workload->minimum_session_context = 4u;
    workload->requested_session_context = 4u;
    workload->concurrent_sequences = 1u;
    workload->logical_batch_tokens = 1u;
    workload->prefill_chunk_tokens = 1u;
    workload->attention_microbatch_rows = 1u;
    workload->moe_row_tile = 1u;
    workload->output_head_rows = 1u;
    workload->system_reserve_bytes = YVEX_EXECUTION_MINIMUM_SYSTEM_RESERVE;
    workload->latency_priority = 1;
    strcpy(workload->name, "mamba2-exact-qualification");
    if (yvex_execution_workload_profile_seal(workload, err) != YVEX_OK)
        return yvex_error_code(err);
    request.schema_version = YVEX_RUNTIME_EXECUTION_PROFILE_SCHEMA_V1;
    request.engine_generation = session->engine_generation;
    request.engine_specialization_identity =
        session->engine_specialization_identity;
    request.kernel_bundle_identity = session->engine_specialization_identity;
    request.workload_profile_identity = workload->identity;
    request.generation_mode = YVEX_EXECUTION_GENERATION_TARGET_ONLY;
    request.evidence = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
    request.execution_class = YVEX_EXECUTION_CLASS_PORTABLE_REFERENCE;
    request.attention_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    request.moe_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    request.sampling_resolution = YVEX_EXECUTION_RESOLUTION_EXACT;
    return yvex_runtime_execution_profile_seal(&request, profile, err);
}

static int mamba_exact_token(
    yvex_runtime_decoder_execution_context *decoder,
    yvex_runtime_logits_context *logits, unsigned int token,
    unsigned long long position,
    yvex_runtime_decoder_execution_result *execution,
    yvex_runtime_logits_row_result *row, float *values,
    unsigned long long value_count, yvex_error *err)
{
    yvex_runtime_decoder_execution_request request = {0};
    yvex_runtime_logits_source source;
    char identity[YVEX_SHA256_HEX_CAP];
    int rc;
    if (!mamba_exact_input_identity(token, position, identity))
        return YVEX_ERR_STATE;
    request.token_ids = &token;
    request.token_start = position;
    request.token_count = 1u;
    request.input_identity = identity;
    rc = yvex_runtime_decoder_execution_execute(
        decoder, &request, execution, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_source_from_decoder(
            logits, &source, execution,
            position ? YVEX_LOGITS_SOURCE_DECODE : YVEX_LOGITS_SOURCE_PREFILL,
            0u, err);
    if (rc == YVEX_OK)
        rc = yvex_runtime_logits_project(
            logits, &source, YVEX_BACKEND_KIND_CPU, values, value_count,
            row, err);
    return rc;
}

static int mamba_exact_tokenizer(const yvex_tokenizer *tokenizer,
                                 yvex_error *err)
{
    static const unsigned char text[] = "Hello world";
    const unsigned int expected[] = {1u, 23325u, 2294u};
    yvex_tokenizer_encode_options encode_options = {
        .add_bos = 1, .maximum_tokens = 8u};
    yvex_tokenizer_decode_options decode_options = {
        .skip_special_tokens = 1, .require_complete_utf8 = 1};
    yvex_tokenizer_encode_result encoded = {0};
    yvex_tokenizer_decode_result decoded = {0};
    yvex_tokenizer_decoder *decoder = NULL;
    yvex_tokenizer_fragment fragment = {0};
    yvex_tokenizer_token_classification eos = {0};
    const char *chat = NULL;
    unsigned long long chat_bytes = 0u;
    unsigned int id;
    int rc = yvex_tokenizer_encode(
        tokenizer, text, sizeof(text) - 1u, &encode_options, &encoded, err);
    if (rc == YVEX_OK &&
        (encoded.tokens.len != 3u ||
         memcmp(encoded.tokens.ids, expected, sizeof(expected)) != 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "test.mamba2.tokenizer",
                       "pinned tokenizer encode vector drifted");
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decode(
            tokenizer, encoded.tokens.ids, encoded.tokens.len,
            &decode_options, &decoded, err);
    if (rc == YVEX_OK &&
        (decoded.byte_count != sizeof(text) - 1u ||
         memcmp(decoded.bytes, text, sizeof(text) - 1u) != 0)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "test.mamba2.tokenizer",
                       "pinned tokenizer decode vector drifted");
        rc = YVEX_ERR_FORMAT;
    }
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decoder_open(
            &decoder, tokenizer, &decode_options, err);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decoder_push(decoder, expected[0], &fragment, err);
    if (rc == YVEX_OK && fragment.byte_count) rc = YVEX_ERR_FORMAT;
    yvex_tokenizer_fragment_clear(&fragment);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decoder_push(decoder, expected[1], &fragment, err);
    if (rc == YVEX_OK &&
        (fragment.byte_count != 5u || memcmp(fragment.bytes, "Hello", 5u)))
        rc = YVEX_ERR_FORMAT;
    yvex_tokenizer_fragment_clear(&fragment);
    if (rc == YVEX_OK)
        rc = yvex_tokenizer_decoder_push(decoder, expected[2], &fragment, err);
    if (rc == YVEX_OK &&
        (fragment.byte_count != 6u || memcmp(fragment.bytes, " world", 6u)))
        rc = YVEX_ERR_FORMAT;
    yvex_tokenizer_fragment_clear(&fragment);
    if (rc == YVEX_OK &&
        (yvex_tokenizer_bos_id(tokenizer, &id) != YVEX_OK || id != 1u ||
         yvex_tokenizer_eos_id(tokenizer, &id) != YVEX_OK || id != 2u ||
         yvex_tokenizer_unk_id(tokenizer, &id) != YVEX_OK || id != 0u ||
         yvex_tokenizer_pad_id(tokenizer, &id) != YVEX_ERR_UNSUPPORTED ||
         yvex_tokenizer_chat_template(tokenizer, &chat, &chat_bytes) !=
             YVEX_ERR_UNSUPPORTED || chat || chat_bytes ||
         yvex_tokenizer_token_classify(tokenizer, 2u, &eos, err) != YVEX_OK ||
         !eos.eos || !eos.stop)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "test.mamba2.tokenizer",
                       "pinned special-token/output policy drifted");
        rc = YVEX_ERR_FORMAT;
    }
    yvex_tokenizer_decoder_close(&decoder);
    yvex_tokenizer_decode_result_clear(&decoded);
    yvex_tokenizer_encode_result_clear(&encoded);
    return rc;
}

static int mamba_exact_result_valid(
    const yvex_program_physical_summary *forward,
    const yvex_program_physical_summary *output,
    const yvex_runtime_decoder_execution_result *first,
    const yvex_runtime_decoder_execution_result *repeated,
    const yvex_runtime_decoder_execution_result *retry,
    const yvex_runtime_logits_row_result *first_row,
    const yvex_runtime_logits_row_result *repeated_row,
    const yvex_runtime_logits_row_result *retry_row,
    const yvex_sequence_state_summary *initial,
    const yvex_sequence_state_summary *after_first,
    const yvex_sequence_state_summary *after_reset,
    const yvex_sequence_state_summary *after_retry,
    const yvex_runtime_session_summary *isolated,
    int cancelled, int stale)
{
    return forward && output && forward->step_count == 900u &&
        output->step_count == 2u && first->completed && repeated->completed &&
        retry->completed && first->layers_executed == 64u &&
        !first->attention_layers && first->recurrent_layers == 64u &&
        first->position_after == 1u && repeated->layers_executed == 64u &&
        repeated->position_after == 1u && retry->layers_executed == 64u &&
        retry->position_after == 2u && first_row->completed &&
        first_row->vocabulary_size == 32768u &&
        first_row->finite_count == 32768u && repeated_row->completed &&
        repeated_row->finite_count == 32768u && retry_row->completed &&
        retry_row->finite_count == 32768u &&
        strcmp(first_row->raw_logits_digest,
               repeated_row->raw_logits_digest) == 0 &&
        initial->binding_count == 64u && !initial->committed_position &&
        after_first->committed_position == 1u &&
        after_reset->committed_position == 0u &&
        after_retry->committed_position == 2u &&
        cancelled == YVEX_ERR_CANCELLED && stale == YVEX_ERR_STATE &&
        isolated->sequence_state_binding_count == 64u &&
        isolated->sequence_state_generation == initial->generation;
}

typedef struct {
    yvex_runtime_execution_session *session;
    yvex_runtime_decoder_execution_context *decoder;
    yvex_runtime_logits_context *logits;
    const yvex_runtime_session_view *session_view;
    yvex_model_engine_failure *failure;
    mamba_exact_cancellation cancellation;
    yvex_sequence_state_summary initial;
    yvex_sequence_state_summary after_first;
    yvex_sequence_state_summary after_reset;
    yvex_sequence_state_summary after_retry;
    yvex_runtime_decoder_execution_result first;
    yvex_runtime_decoder_execution_result repeated;
    yvex_runtime_decoder_execution_result retry;
    yvex_runtime_logits_row_result first_row;
    yvex_runtime_logits_row_result repeated_row;
    yvex_runtime_logits_row_result retry_row;
    float *first_logits;
    float *repeated_logits;
    float *retry_logits;
    const char *stage;
    int cancelled;
    int stale;
} mamba_exact_run;

static int mamba_exact_run_execute(mamba_exact_run *run, yvex_error *err)
{
    int rc;
    run->stage = "initial-state";
    rc = yvex_sequence_state_summary_copy(
        run->session_view->sequence_state, &run->initial, err);
    run->first_logits = calloc(32768u, sizeof(*run->first_logits));
    run->repeated_logits = calloc(32768u, sizeof(*run->repeated_logits));
    run->retry_logits = calloc(32768u, sizeof(*run->retry_logits));
    if (rc == YVEX_OK &&
        (!run->first_logits || !run->repeated_logits || !run->retry_logits)) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "test.mamba2.exact-runtime",
                       "cannot allocate exact logits evidence buffers");
        rc = YVEX_ERR_NOMEM;
    }
    if (rc == YVEX_OK) {
        run->stage = "first-token";
        rc = mamba_exact_token(
            run->decoder, run->logits, 1u, 0u, &run->first,
            &run->first_row, run->first_logits, 32768u, err);
    }
    if (rc == YVEX_OK) {
        run->stage = "first-state";
        rc = yvex_sequence_state_summary_copy(
            run->session_view->sequence_state, &run->after_first, err);
    }
    if (rc == YVEX_OK) {
        run->stage = "state-reset";
        rc = yvex_runtime_session_reset_persistent_state(
            run->session, run->failure, err);
    }
    if (rc == YVEX_OK) {
        run->stage = "reset-state";
        rc = yvex_sequence_state_summary_copy(
            run->session_view->sequence_state, &run->after_reset, err);
    }
    if (rc == YVEX_OK) {
        run->stage = "repeated-token";
        rc = mamba_exact_token(
            run->decoder, run->logits, 1u, 0u, &run->repeated,
            &run->repeated_row, run->repeated_logits, 32768u, err);
    }
    if (rc == YVEX_OK) {
        unsigned int token = 3u;
        char identity[YVEX_SHA256_HEX_CAP];
        yvex_runtime_decoder_execution_request request = {0};
        yvex_runtime_decoder_execution_result result = {0};
        run->stage = "cancelled-token";
        run->cancellation.requested = 1;
        mamba_exact_input_identity(token, 1u, identity);
        request.token_ids = &token;
        request.token_start = 1u;
        request.token_count = 1u;
        request.input_identity = identity;
        run->cancelled = yvex_runtime_decoder_execution_execute(
            run->decoder, &request, &result, err);
        run->cancellation.requested = 0;
        if (run->cancelled == YVEX_ERR_CANCELLED)
            yvex_error_clear(err);
        else
            rc = run->cancelled;
    }
    if (rc == YVEX_OK) {
        run->stage = "retry-token";
        rc = mamba_exact_token(
            run->decoder, run->logits, 3u, 1u, &run->retry,
            &run->retry_row, run->retry_logits, 32768u, err);
    }
    if (rc == YVEX_OK) {
        run->stage = "retry-state";
        rc = yvex_sequence_state_summary_copy(
            run->session_view->sequence_state, &run->after_retry, err);
    }
    if (rc == YVEX_OK) {
        unsigned int token = 4u;
        char identity[YVEX_SHA256_HEX_CAP];
        yvex_runtime_decoder_execution_request request = {0};
        yvex_runtime_decoder_execution_result result = {0};
        run->stage = "stale-position";
        mamba_exact_input_identity(token, 0u, identity);
        request.token_ids = &token;
        request.token_count = 1u;
        request.input_identity = identity;
        run->stale = yvex_runtime_decoder_execution_execute(
            run->decoder, &request, &result, err);
        if (run->stale == YVEX_ERR_STATE)
            yvex_error_clear(err);
        else
            rc = run->stale;
    }
    return rc;
}

static int mamba_acquired_runtime(const char *artifact, const char *binding)
{
    yvex_model_engine_open_request model_request = {0};
    yvex_runtime_session_open_request session_request = {0};
    yvex_runtime_decoder_execution_options decoder_options = {0};
    yvex_runtime_logits_options logits_options = {0};
    yvex_model_engine *model = NULL;
    yvex_runtime_execution_session *session = NULL, *isolated = NULL;
    yvex_runtime_decoder_execution_context *decoder = NULL;
    yvex_runtime_logits_context *logits = NULL;
    yvex_model_engine_failure failure = {0};
    yvex_model_engine_summary model_summary = {0};
    yvex_runtime_session_summary session_summary = {0}, isolated_summary = {0};
    yvex_execution_workload_profile workload = {0};
    yvex_runtime_execution_profile profile = {0};
    mamba_exact_run run = {.cancelled = YVEX_ERR, .stale = YVEX_ERR};
    const yvex_runtime_session_view *session_view;
    const yvex_model_engine_view *model_view;
    const yvex_program_physical_summary *forward, *output;
    unsigned long long started = yvex_core_monotonic_ns(), completed;
    yvex_error err;
    const char *stage = "model-open";
    int rc = YVEX_OK, passed = 0;

    model_request.artifact_path = artifact;
    model_request.runtime_binding_path = binding;
    model_request.target_id = YVEX_MAMBA2_TARGET;
    model_request.residency_backend = YVEX_BACKEND_KIND_CPU;
    session_request.backend = YVEX_BACKEND_KIND_CPU;
    yvex_error_clear(&err);
    rc = yvex_model_engine_open(
        &model, &model_request, &failure, &err);
    if (rc != YVEX_OK) goto close;
    {
        stage = "model-summary";
        rc = yvex_model_engine_summary_copy(model, &model_summary, &err);
        if (rc == YVEX_OK) {
            stage = "session-open";
            rc = yvex_runtime_session_open(
                &session, model, &session_request, &failure, &err);
        }
        if (rc == YVEX_OK) {
            stage = "session-summary";
            rc = yvex_runtime_session_summary_copy(
                session, &session_summary, &err);
        }
        if (rc == YVEX_OK) {
            stage = "execution-profile";
            rc = mamba_exact_profile(
                &session_summary, &workload, &profile, &err);
        }
        decoder_options.context_capacity = 4u;
        decoder_options.token_capacity = 1u;
        decoder_options.execution_profile = &profile;
        decoder_options.cancel_requested = mamba_exact_cancel;
        decoder_options.cancel_context = &run.cancellation;
        logits_options.maximum_rows = 1u;
        logits_options.evidence_profile = YVEX_EXECUTION_EVIDENCE_PRODUCTION;
        logits_options.execution_profile = &profile;
        if (rc == YVEX_OK) {
            stage = "decoder-open";
            rc = yvex_runtime_decoder_execution_context_open(
                &decoder, model, session, &decoder_options, &err);
        }
        if (rc == YVEX_OK) {
            stage = "logits-open";
            rc = yvex_runtime_logits_context_open_program(
                &logits, model, session, &logits_options, &err);
        }
        session_view = yvex_runtime_session_view_get(session);
        model_view = yvex_model_engine_view_get(model);
        forward = model_view
                      ? yvex_program_physical_summary_get(
                            yvex_compiled_model_plan_forward(
                                model_view->compiled_plan))
                      : NULL;
        output = model_view
                     ? yvex_program_physical_summary_get(
                           yvex_compiled_model_plan_output(
                               model_view->compiled_plan))
                     : NULL;
        if (rc == YVEX_OK) {
            stage = "exact-tokenizer";
            rc = mamba_exact_tokenizer(model_view->tokenizer, &err);
        }
        if (rc == YVEX_OK) {
            run.session = session;
            run.decoder = decoder;
            run.logits = logits;
            run.session_view = session_view;
            run.failure = &failure;
            rc = mamba_exact_run_execute(&run, &err);
            stage = run.stage;
        }
        if (rc == YVEX_OK) {
            stage = "isolated-session-open";
            rc = yvex_runtime_session_open(
                &isolated, model, &session_request, &failure, &err);
        }
        if (rc == YVEX_OK) {
            stage = "isolated-session-summary";
            rc = yvex_runtime_session_summary_copy(
                isolated, &isolated_summary, &err);
        }
        if (rc == YVEX_OK) {
            stage = "final-session-summary";
            rc = yvex_runtime_session_summary_copy(
                session, &session_summary, &err);
        }
        passed = rc == YVEX_OK && mamba_exact_result_valid(
            forward, output, &run.first, &run.repeated, &run.retry,
            &run.first_row, &run.repeated_row, &run.retry_row, &run.initial,
            &run.after_first, &run.after_reset, &run.after_retry,
            &isolated_summary, run.cancelled, run.stale);
        if (!passed && rc == YVEX_OK) {
            yvex_error_set(&err, YVEX_ERR_STATE, "test.mamba2.exact-runtime",
                           "exact all-layer/output/session evidence was incomplete");
            rc = YVEX_ERR_STATE;
        }
        if (rc != YVEX_OK)
            fprintf(stderr, "exact Mamba2 runtime stage=%s: %s: %s\n",
                    stage, yvex_error_where(&err), yvex_error_message(&err));
    }
    completed = yvex_core_monotonic_ns();
    if (passed)
        printf("acquired-mamba2-runtime: forward_steps=900 output_steps=2 "
               "layers=64 attention=0 KV=0 RoPE=0 dense_FFN=0 "
               "vocabulary=32768 finite=32768 positions=0->1/reset->0->1->2 "
               "cancel=%d stale=%d logits=%s mapped_bytes=%llu "
               "resident_host_bytes=%llu committed_state_bytes=%llu "
               "candidate_state_bytes=%llu elapsed=%.3fs\n",
               run.cancelled, run.stale, run.first_row.raw_logits_digest,
               model_summary.mapped_package_bytes,
               model_summary.resident_host_bytes,
               session_summary.sequence_committed_state_bytes,
               session_summary.sequence_candidate_state_bytes,
               (double)(completed - started) / 1000000000.0);

close:
    if (!passed && rc != YVEX_OK)
        fprintf(stderr, "exact Mamba2 runtime stage=%s status=%d where=%s reason=%s\n",
                stage, rc, yvex_error_where(&err), yvex_error_message(&err));
    free(run.retry_logits);
    free(run.repeated_logits);
    free(run.first_logits);
    (void)yvex_runtime_logits_context_close(&logits, NULL);
    (void)yvex_runtime_decoder_execution_context_close(&decoder, NULL);
    (void)yvex_runtime_session_close(&isolated, NULL);
    (void)yvex_runtime_session_close(&session, NULL);
    yvex_model_engine_close(&model);
    return passed ? 0 : 1;
}

static int mamba_acquired_contract(void)
{
    const char *source = getenv("YVEX_MAMBA2_SOURCE");
    const yvex_mamba2_api *api = yvex_model_register_mamba2();
    yvex_source_verify_options options = {0};
    yvex_source_verification verification;
    yvex_source_tensor_snapshot *snapshot = NULL;
    yvex_mamba2_architecture architecture;
    yvex_mamba2_inventory inventory;
    yvex_family_source_products products = {0};
    yvex_compilation_runtime_binding_request compilation = {0};
    yvex_error err;
    int rc;
    if (!source || !source[0]) return 0;
    options.identity = yvex_source_target_identity_find(YVEX_MAMBA2_TARGET);
    options.source_path = source;
    options.models_root = getenv("YVEX_MAMBA2_MODELS_ROOT");
    options.manifest_path = getenv("YVEX_MAMBA2_MANIFEST");
    YVEX_TEST_ASSERT(options.models_root && options.manifest_path,
                     "real source proof requires its existing root and verified manifest");
    rc = yvex_source_verify_with_snapshot(&options, &verification, &snapshot, &err);
    if (rc == YVEX_OK && !verification.verified) {
        fprintf(stderr, "real Mamba2 verification: path=%d repo=%d revision=%d config=%d manifest=%d blockers=%u\n",
            verification.path_verified, verification.repository_verified, verification.revision_verified,
            verification.config_valid, verification.manifest_verified, verification.blocker_count);
        for (unsigned int i = 0u; i < verification.blocker_count; ++i)
            fprintf(stderr, "real Mamba2 blocker: %s\n", verification.blockers[i]);
    }
    if (rc == YVEX_OK) rc = api->open(&verification, &architecture, &err);
    if (rc == YVEX_OK) rc = api->snapshot_audit(&architecture, snapshot, &inventory, &err);
    yvex_source_tensor_snapshot_release(snapshot);
    if (rc != YVEX_OK) fprintf(stderr, "real Mamba2 source contract: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && inventory.complete, "real acquired source contract");
    compilation.source_path = source;
    compilation.models_root = options.models_root;
    compilation.source_manifest_path = options.manifest_path;
    rc = yvex_family_source_compile(YVEX_MAMBA2_TARGET, &compilation, &products, &err);
    const yvex_semantic_model_ir_summary *semantic =
        rc == YVEX_OK ? yvex_semantic_model_ir_summary_get(products.semantic_model) : NULL;
    YVEX_TEST_ASSERT(rc == YVEX_OK && semantic && semantic->schema_version == YVEX_SEMANTIC_MODEL_IR_SCHEMA_V2 &&
        semantic->family_adapter_id == YVEX_MAMBA2_ADAPTER_ID && semantic->decoder_layer_count == 0u &&
        semantic->execution_descriptor.schema_version ==
            YVEX_MODEL_EXECUTION_DESCRIPTOR_SCHEMA_V2 &&
        semantic->execution_descriptor.layer_count == 64u &&
        semantic->execution_descriptor.sequence_mixer_layers == 64u &&
        !semantic->execution_descriptor.attention_heads &&
        !semantic->execution_descriptor.kv_heads &&
        !semantic->execution_descriptor.head_width &&
        !semantic->execution_descriptor.dense_ffn_width &&
        yvex_semantic_model_ir_program(products.semantic_model) &&
        yvex_sha256_hex_valid(products.derivation_identity),
        "canonical family source gate publishes program-owned pure-SSM execution semantics");
    yvex_family_source_products_release(&products);
    YVEX_TEST_ASSERT(mamba_acquired_program(
                         &architecture, &inventory, verification.manifest_payload_identity, &err) == 0,
                     "real acquired model compiles through common program and physical lowering");
    printf("acquired-mamba2: revision=%s architecture=%s roles=%s tensors=%llu bytes=%llu "
           "conv_values_per_layer=%llu recurrent_values_per_layer=%llu layers=%llu "
           "token_conflict=%d token_authority=%d normalization_conflict=%d normalization_authority=%d "
           "compiler_executable=true hosted=false\n",
           architecture.source_revision, architecture.architecture_identity, inventory.role_identity,
           inventory.tensors, inventory.tensor_bytes, architecture.mixer.convolution_state_values,
           architecture.mixer.recurrent_state_values, architecture.layer_count,
           architecture.token_policy_conflict, architecture.token_authority,
           architecture.normalization_policy_conflict, architecture.normalization_authority);
    {
        const char *artifact = getenv("YVEX_MAMBA2_ARTIFACT");
        const char *durable_binding = getenv("YVEX_MAMBA2_BINDING");
        const char *plan = getenv("YVEX_MAMBA2_PHYSICAL_PLAN");
        const yvex_graph_execution_binding *execution =
            yvex_graph_execution_find(0u, 0u, YVEX_MAMBA2_TARGET);
        yvex_compilation_runtime_binding_request request = {0};
        yvex_runtime_binding_summary summary = {0};
        yvex_runtime_binding_failure failure = {0};
        yvex_runtime_binding *binding = NULL;
        char directory[] = "build/tests/mamba2-binding.XXXXXX";
        char path[YVEX_PATH_CAP] = {0};
        int published = 0;

        if (artifact && artifact[0] && plan && plan[0]) {
            YVEX_TEST_ASSERT(execution && execution->compiler && mkdtemp(directory),
                             "exact binding test owner created");
            request.source_path = source;
            request.models_root = options.models_root;
            request.source_manifest_path = options.manifest_path;
            request.artifact_path = artifact;
            request.directory = directory;
            request.quant_preset_name = "mamba-codestral-source-faithful-v1";
            request.physical_variant_plan_path = plan;
            request.family_adapter_id = YVEX_MAMBA2_ADAPTER_ID;
            request.family_adapter_version = YVEX_MAMBA2_ADAPTER_VERSION;
            request.source_stream_count = 1u;
            rc = yvex_runtime_binding_compile_publish(
                execution->compiler, &request, path, &published, &err);
            if (rc != YVEX_OK)
                fprintf(stderr, "exact Mamba2 binding: %s: %s\n",
                        err.where, yvex_error_message(&err));
            YVEX_TEST_ASSERT(
                rc == YVEX_OK && published &&
                yvex_runtime_binding_open(&binding, path, &summary, NULL,
                                          &failure, &err) == YVEX_OK &&
                summary.schema_version == YVEX_RUNTIME_BINDING_SCHEMA_CURRENT &&
                summary.tensor_count == 579u && !summary.layer_count &&
                summary.recurrent_layer_count == 64u &&
                !summary.attention_plan_identity[0] &&
                summary.semantic_maximum_context == 1048576u,
                "exact artifact reopens through attention-optional executable binding");
            printf("acquired-mamba2-binding: schema=%u tensors=%llu attention=%llu "
                   "recurrent=%llu artifact=%s binding=%s\n",
                   summary.schema_version, summary.tensor_count, summary.layer_count,
                   summary.recurrent_layer_count, summary.artifact_identity,
                   summary.identity);
            yvex_runtime_binding_close(binding);
            (void)unlink(path);
            (void)rmdir(directory);
        }
        if (artifact && artifact[0] && durable_binding && durable_binding[0])
            YVEX_TEST_ASSERT(
                mamba_acquired_runtime(artifact, durable_binding) == 0,
                "exact artifact executes all layers/output through the common runtime");
    }
    return 0;
}

int yvex_test_mamba2_source(void)
{
    char root[] = "build/tests/mamba2-source.XXXXXX", path[768];
    const char *const dirs[] = {".cache", ".cache/huggingface", ".cache/huggingface/download"};
    const char *const files[] = {"config.json", "params.json", "generation_config.json", "tokenizer.json",
        "tokenizer_config.json", "special_tokens_map.json", "tokenizer.model", "tokenizer.model.v3"};
    size_t i;
    int rc;
    YVEX_TEST_ASSERT(mkdtemp(root) != NULL, "create bounded family fixture root");
    for (i = 0; i < 3u; ++i) {
        snprintf(path, sizeof(path), "%s/%s", root, dirs[i]);
        YVEX_TEST_ASSERT(mkdir(path, 0700) == 0, "create fixture provider metadata directory");
    }
    YVEX_TEST_ASSERT(mamba_fixture_config(root, "mamba2", 2u, 1, "") &&
        mamba_write(root, "params.json", "{\"dim\":4,\"n_layers\":2,\"vocab_size\":4,\"n_groups\":2,"
            "\"rms_norm\":true,\"residual_in_fp32\":true,\"tie_embeddings\":false,\"model_type\":\"mamba\"}") &&
        mamba_write(root, "generation_config.json", "{\"bos_token_id\":0,\"eos_token_id\":2,\"pad_token_id\":1}") &&
        mamba_write(root, "tokenizer.json", "{\"model\":{\"type\":\"BPE\","
            "\"vocab\":{\"<unk>\":0,\"<s>\":1,\"</s>\":2,\"a\":3}}}") &&
        mamba_write(root, "tokenizer_config.json", "{\"add_bos_token\":true,\"add_eos_token\":false,"
            "\"bos_token\":\"<s>\",\"eos_token\":\"</s>\",\"unk_token\":\"<unk>\","
            "\"pad_token\":null,\"chat_template\":null,\"tokenizer_class\":\"LlamaTokenizer\"}") &&
        mamba_write(root, "special_tokens_map.json", "{\"bos_token\":{\"content\":\"<s>\"},"
            "\"eos_token\":{\"content\":\"</s>\"},\"unk_token\":{\"content\":\"<unk>\"}}") &&
        mamba_write(root, "tokenizer.model", "identical-tokenizer-fixture") &&
        mamba_write(root, "tokenizer.model.v3", "identical-tokenizer-fixture"), "write identity-bound fixtures");
    rc = mamba_contract(root);
    for (i = 0; i < 8u; ++i) {
        snprintf(path, sizeof(path), "%s/%s", root, files[i]);
        (void)unlink(path);
        snprintf(path, sizeof(path), "%s/.cache/huggingface/download/%s.metadata", root, files[i]);
        (void)unlink(path);
    }
    for (i = 3u; i > 0; --i) {
        snprintf(path, sizeof(path), "%s/%s", root, dirs[i - 1u]);
        (void)rmdir(path);
    }
    (void)rmdir(root);
    return rc ? rc : mamba_acquired_contract();
}
