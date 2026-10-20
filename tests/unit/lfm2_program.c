/* Whole scaled source program against an independent pinned upstream fixture.
 * This exercises the native VM/state owners, not checkpoint model quality. */
#include "tests/test.h"
#include "src/graph/private.h"
#include <yvex/internal/families/lfm2.h>
#include <yvex/internal/program_sequence.h>
#include <yvex/internal/program_stage.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/internal/quant_numeric.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define LFM_CHECK(x) YVEX_TEST_ASSERT(x, #x)
#define LFM_WEIGHTS 80u
#define LFM_ROWS 16u
#define LFM_WIDTH 64u
static const char lfm_reference_source[] =
    "0496e07771dcac3d552ab01bb8f0f95af01871c7fc74f59c3da4487de7fd4484";

typedef struct {
    char name[256];
    unsigned char *data;
    size_t count;
    unsigned int scalar;
} lfm_reference_weight;

typedef struct {
    yvex_lfm2_architecture a;
    lfm_reference_weight source[LFM_WEIGHTS];
    size_t source_count, weight_count;
    yvex_program_kernel_parameter weights[LFM_WEIGHTS];
    yvex_program_parameter_binding parameters[LFM_WEIGHTS];
    yvex_ir_module *module;
    yvex_program_execution *execution;
    yvex_program_physical *physical, *output_program;
    yvex_backend *backend;
    yvex_program_kernels *kernels;
    yvex_program_sequence *sequence;
    yvex_program_device *device;
    yvex_program_stage *output;
    yvex_sequence_state *state;
    yvex_runtime_session_view session;
    yvex_model_engine_view model;
    yvex_attention_layer_plan layer;
    yvex_attention_plan attention;
    yvex_attention_state_provider attention_state;
    yvex_graph_attention_capacity_plan *capacity;
    yvex_runtime_state_residency *residency;
    yvex_device_tensor *hidden;
    unsigned char *encoded_arena;
    yvex_device_tensor *encoded_device;
    yvex_backend_kind backend_kind;
    yvex_program_device_argument args[5];
    yvex_program_device_kernel dispatch[64];
    unsigned int tokens[LFM_ROWS];
    float expected_hidden[LFM_ROWS * LFM_WIDTH], expected_logits[LFM_ROWS * LFM_WIDTH];
    size_t rows;
    int cancel, cancel_after_attention, prepared;
} lfm_reference;

static int lfm_reference_read(lfm_reference *f, yvex_error *err)
{
    const char *path = getenv("YVEX_LFM2_PROGRAM_ORACLE");
    FILE *file = path ? fopen(path, "r") : NULL;
    char config[4096], trailing;
    LFM_CHECK(file && fgets(config, sizeof(config), file));
    LFM_CHECK(yvex_model_register_lfm2()->config_parse(config, strlen(config), &f->a, err) == YVEX_OK);
    LFM_CHECK(f->a.layers == 3u && f->a.hidden <= LFM_WIDTH && f->a.vocabulary <= LFM_WIDTH &&
        f->a.convolution_layers == 2u && f->a.attention_layers == 1u &&
        f->a.mixer[1] == YVEX_LFM2_MIXER_ATTENTION);
    LFM_CHECK(fscanf(file, "%zu", &f->source_count) == 1 && f->source_count <= LFM_WEIGHTS);
    for (size_t i = 0u; i < f->source_count; ++i) {
        lfm_reference_weight *w = &f->source[i];
        LFM_CHECK(fscanf(file, "%255s %u %zu", w->name, &w->scalar, &w->count) == 3 &&
            (w->scalar == 16u || w->scalar == 32u) && w->count && w->count <= 1048576u);
        w->data = calloc(w->count, w->scalar / 8u);
        LFM_CHECK(w->data);
        for (size_t j = 0u; j < w->count; ++j) {
            float x;
            LFM_CHECK(fscanf(file, "%f", &x) == 1 && isfinite(x));
            if (w->scalar == 32u) memcpy(w->data + j * 4u, &x, 4u);
            else {
                unsigned short b = yvex_quant_bf16_encode(x);
                LFM_CHECK(yvex_quant_bf16_decode(b) == x);
                w->data[j * 2u] = (unsigned char)b;
                w->data[j * 2u + 1u] = (unsigned char)(b >> 8u);
            }
        }
    }
    LFM_CHECK(fscanf(file, "%zu", &f->rows) == 1 && f->rows > 3u && f->rows <= LFM_ROWS);
    for (size_t i = 0u; i < f->rows; ++i)
        LFM_CHECK(fscanf(file, "%u", &f->tokens[i]) == 1 && f->tokens[i] < f->a.vocabulary);
    for (size_t i = 0u; i < f->rows * f->a.hidden; ++i)
        LFM_CHECK(fscanf(file, "%f", &f->expected_hidden[i]) == 1 && isfinite(f->expected_hidden[i]));
    for (size_t i = 0u; i < f->rows * f->a.vocabulary; ++i)
        LFM_CHECK(fscanf(file, "%f", &f->expected_logits[i]) == 1 && isfinite(f->expected_logits[i]));
    LFM_CHECK(fscanf(file, " %c", &trailing) == EOF && fclose(file) == 0);
    return 0;
}

static int lfm_reference_compile(lfm_reference *f, yvex_error *err)
{
    int rc = yvex_model_register_lfm2()->program_build(&f->module, &f->a, lfm_reference_source, err);
    if (rc != YVEX_OK) return rc;
    for (size_t i = 0u; i < yvex_ir_operation_count(f->module); ++i) {
        const yvex_ir_operation *op = yvex_ir_operation_at(f->module, (yvex_ir_id)i);
        if (strcmp(op->definition->name, "core.parameter")) continue;
        const char *name = yvex_ir_attribute_get(f->module, (yvex_ir_id)i, "parameter")->value.text;
        const yvex_ir_type *t = yvex_ir_type_at(f->module, yvex_ir_value_at(f->module, op->results[0])->type);
        lfm_reference_weight *w = NULL;
        size_t count = 1u, n = f->weight_count;
        for (size_t j = 0u; j < f->source_count; ++j)
            if (!strcmp(name, f->source[j].name)) { if (w) return YVEX_ERR_FORMAT; w = &f->source[j]; }
        for (size_t j = 0u; j < t->rank; ++j) count *= t->shape[j].extent;
        if (!w || count != w->count || n >= LFM_WEIGHTS ||
            w->scalar != (t->scalar == YVEX_IR_F32 ? 32u : 16u)) return YVEX_ERR_FORMAT;
        unsigned long long width = t->shape[t->rank - 1u].extent;
        unsigned int qtype = w->scalar == 32u ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16;
        f->parameters[n] = (yvex_program_parameter_binding){op->results[0], n, qtype};
        f->weights[n] = (yvex_program_kernel_parameter){.tensor_id = n, .weight = {
            .encoded = w->data, .encoded_bytes = count * (w->scalar / 8u),
            .row_count = count / width, .row_width = width,
            .row_bytes = width * (w->scalar / 8u), .qtype = qtype}};
        f->weight_count++;
    }
    rc = yvex_program_execution_compile(&f->execution, f->module, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&f->physical, f->execution, "forward",
        f->parameters, f->weight_count, lfm_reference_source, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(&f->output_program, f->execution, "output",
        f->parameters, f->weight_count, lfm_reference_source, err);
    return rc;
}

static int lfm_reference_cancel(void *context) { return ((lfm_reference *)context)->cancel; }
static int lfm_reference_invoke(void *context, const yvex_program_device_invocation *r,
    yvex_backend_operation_facts *facts, yvex_error *err)
{
    lfm_reference *f = context;
    const char *implementation = r->step->implementation;
    int attention = !strcmp(implementation, "causal.eager_bf16.v1");
    int rc;
    if (attention || !strcmp(implementation, "double_gated_conv.cpu.bf16.v1") ||
        !strcmp(implementation, "double_gated_conv.bf16.v2")) {
        yvex_program_sequence_request options = {lfm_reference_source, lfm_reference_cancel, f};
        rc = yvex_program_sequence_invoke(f->sequence, r, &options, facts, err);
        if (rc == YVEX_OK && attention && f->cancel_after_attention) f->cancel = 1;
    } else rc = yvex_program_kernels_invoke(f->kernels, r, facts, err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid program step %zu %s: %s\n",
        r->step_index, implementation, yvex_error_message(err));
    return rc;
}

static int lfm_reference_attention(lfm_reference *f, yvex_error *err)
{
    yvex_graph_attention_capacity_request request = {
        .scope = YVEX_ATTENTION_PROBE_SCOPE_FULL, .token_count = 32u, .execution_count = 1u};
    yvex_attention_failure failure = {0};
    f->layer = (yvex_attention_layer_plan){.ordinal = 0u, .layer_index = 1u,
        .predictor_index = YVEX_ATTENTION_NO_TENSOR_INDEX, .tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER,
        .attention_class = YVEX_ATTENTION_CLASS_SWA, .compute_contract = YVEX_ATTENTION_COMPUTE_BF16_EAGER_V1,
        .sliding_window = f->a.context, .query_heads = f->a.heads, .kv_heads = f->a.kv_heads,
        .head_dimension = f->a.head_width, .rope_head_dimension = f->a.head_width,
        .hidden_dimension = f->a.hidden, .position = {.theta = (unsigned long long)f->a.rope_theta,
            .scaling_factor = 1u}};
    f->attention.layers = &f->layer; f->attention.layer_count = 1u;
    f->attention.summary = (yvex_attention_summary){.status = YVEX_ATTENTION_STATUS_EXECUTION_READY,
        .tensor_scope = YVEX_TENSOR_SCOPE_MAIN_LAYER, .layer_count = 1u, .swa_layer_count = 1u,
        .history_contract_ready = 1, .state_delta_contract_ready = 1};
    yvex_core_text_copy(f->attention.summary.attention_plan_identity,
        sizeof(f->attention.summary.attention_plan_identity), lfm_reference_source);
    f->model.attention = &f->attention;
    int rc = yvex_graph_attention_capacity_plan_build(&f->capacity, &f->attention, &request, err);
    if (rc == YVEX_OK) rc = yvex_attention_state_provider_open_persistent(&f->attention, 0u,
        &f->attention_state, &failure, err);
    const yvex_graph_attention_capacity_layer *capacity = yvex_graph_attention_capacity_plan_layer(f->capacity, 0u);
    if (rc == YVEX_OK && !capacity) return YVEX_ERR_STATE;
    if (rc == YVEX_OK) rc = f->attention_state.prepare(f->attention_state.context, 0u,
        &capacity->recipe, NULL, &failure, err);
    if (rc == YVEX_OK) rc = yvex_runtime_state_residency_prepare(&f->residency, f->backend,
        f->capacity, &f->attention_state, 0u, 0u, 0u, 0u, err);
    f->session.attention_state_provider = &f->attention_state;
    f->session.state_residency = f->residency;
    return rc;
}

static int lfm_reference_resident(lfm_reference *f, yvex_error *err)
{
    if (f->backend_kind == YVEX_BACKEND_KIND_CPU) return YVEX_OK;
    unsigned long long bytes = 0u, cursor = 0u;
    for (size_t i = 0u; i < f->weight_count; ++i)
        bytes += f->weights[i].weight.encoded_bytes;
    if (!bytes || bytes > 16u * 1024u * 1024u) return YVEX_ERR_BOUNDS;
    yvex_backend_tensor_desc d = {.name = "liquid-reference-parameters", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {bytes}, .bytes = bytes};
    int rc = yvex_backend_resident_alloc(f->backend, &d, &f->encoded_device, &f->encoded_arena, err);
    if (rc != YVEX_OK) return rc;
    for (size_t i = 0u; i < f->weight_count; ++i) {
        yvex_component_encoded_weight *w = &f->weights[i].weight;
        memcpy(f->encoded_arena + cursor, w->encoded, (size_t)w->encoded_bytes);
        w->encoded = f->encoded_arena + cursor; cursor += w->encoded_bytes;
    }
    f->encoded_device->is_written = 1;
    rc = yvex_backend_resident_attach(f->backend, f->encoded_arena, bytes,
        f->encoded_device, 1u, err);
    return rc;
}

static int lfm_reference_open(lfm_reference *f, yvex_error *err)
{
    yvex_sequence_state_plan state;
    size_t count = 0u;
    int rc = lfm_reference_compile(f, err);
    yvex_backend_options options = {.kind = f->backend_kind};
    if (rc == YVEX_OK) rc = yvex_backend_open(&f->backend, &options, err);
    if (rc == YVEX_OK) rc = lfm_reference_resident(f, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_open(&f->kernels, f->physical, f->weights,
        f->weight_count, f->backend, 0u, 0u, err);
    if (rc == YVEX_OK && !yvex_program_physical_sequence_state(f->physical, &state)) return YVEX_ERR_FORMAT;
    if (rc == YVEX_OK) rc = yvex_sequence_state_open_for_backend(&f->state, &state, f->backend_kind, err);
    if (rc == YVEX_OK && f->backend_kind != YVEX_BACKEND_KIND_CPU)
        rc = yvex_sequence_state_attach_device(f->state, f->backend, err);
    f->session.backend = f->backend; f->session.sequence_state = f->state;
    if (rc == YVEX_OK) rc = lfm_reference_attention(f, err);
    if (rc == YVEX_OK) rc = yvex_program_sequence_open(&f->sequence, f->physical, &f->model,
        &f->session, f->kernels, f->rows, 0u, 0u, err);
    if (rc != YVEX_OK) return rc;
    for (size_t i = 0u; i < yvex_program_physical_summary_get(f->physical)->step_count; ++i) {
        const char *name = yvex_program_physical_step_at(f->physical, i)->implementation;
        size_t j;
        for (j = 0u; j < count; ++j) if (!strcmp(f->dispatch[j].implementation, name)) break;
        if (j < count) continue;
        if (count == 64u) return YVEX_ERR_BOUNDS;
        f->dispatch[count++] = (yvex_program_device_kernel){name, lfm_reference_invoke};
    }
    rc = yvex_program_device_open(&f->device, f->physical, f->backend, f->rows, 0u, 0u,
        f->dispatch, count, f, err);
    yvex_backend_tensor_desc d = {.name = "liquid-reference-hidden", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {f->rows, f->a.hidden}, .bytes = f->rows * f->a.hidden * sizeof(float)};
    if (rc == YVEX_OK) rc = yvex_backend_tensor_alloc(f->backend, &d, &f->hidden, err);
    if (rc == YVEX_OK) rc = yvex_program_kernels_prepare(f->kernels, f->rows, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_program_stage_open(&f->output, f->output_program, f->weights, f->weight_count,
        f->backend, f->rows, 1, 0u, 0u, err);
    for (size_t i = 2u; i < 5u; ++i) f->args[i].state_handle = i;
    return rc;
}

static int lfm_reference_prepare(void *context, yvex_error *err)
{
    lfm_reference *f = context;
    int rc = yvex_runtime_state_residency_prepare_commit(f->residency, err);
    if (rc == YVEX_OK) rc = f->attention_state.prepare_commit(f->attention_state.context, NULL, err);
    f->prepared = rc == YVEX_OK;
    return rc;
}
static void lfm_reference_publish(void *context)
{
    lfm_reference *f = context;
    yvex_runtime_state_residency_publish_commit(f->residency);
    f->attention_state.publish_commit(f->attention_state.context); f->prepared = 0;
}
static int lfm_reference_abort(void *context, yvex_error *err)
{
    lfm_reference *f = context;
    if (f->prepared) f->attention_state.cancel_commit(f->attention_state.context);
    f->prepared = 0; yvex_runtime_state_residency_abort(f->residency);
    return f->attention_state.abort(f->attention_state.context, NULL, err);
}
static int lfm_reference_run(lfm_reference *f, size_t start, size_t rows,
    float *hidden, float *logits, yvex_error *err)
{
    yvex_device_tensor view, *outputs[] = {&view};
    yvex_program_device_result result;
    yvex_runtime_transaction_participant participants[2];
    if (!yvex_backend_tensor_f32_subview(f->hidden, 0u, rows * f->a.hidden, &view)) return YVEX_ERR_BOUNDS;
    view.rank = 2u; view.dims[0] = rows; view.dims[1] = f->a.hidden;
    f->args[0].indices = f->tokens + start; f->args[1].index = start;
    int rc = yvex_program_kernels_prepare(f->kernels, rows, 0u, 0u, err);
    if (rc == YVEX_OK) rc = yvex_sequence_state_begin(f->state, start, rows, err);
    if (rc != YVEX_OK) return rc;
    rc = yvex_program_device_run(f->device, rows, f->args, 5u, outputs, 1u,
        lfm_reference_cancel, f, &result, err);
    if (yvex_sequence_state_participant(f->state, participants, NULL) != YVEX_OK) return YVEX_ERR_STATE;
    participants[1] = (yvex_runtime_transaction_participant){f, lfm_reference_prepare,
        lfm_reference_publish, lfm_reference_abort};
    rc = yvex_runtime_transaction_resolve(participants, 2u, rc, err);
    if (rc == YVEX_OK) rc = yvex_backend_tensor_read(f->backend, &view, hidden,
        rows * f->a.hidden * sizeof(float), err);
    const float *inputs[] = {hidden}; float *results[] = {logits};
    yvex_backend_operation_facts facts;
    if (rc == YVEX_OK) rc = yvex_program_stage_host(f->output, rows, inputs, 1u, results, 1u,
        lfm_reference_cancel, f, &facts, err);
    return rc;
}

static int lfm_reference_reset(lfm_reference *f, yvex_error *err)
{
    int rc = yvex_sequence_state_reset(f->state, err);
    if (rc == YVEX_OK) rc = f->attention_state.reset(f->attention_state.context, NULL, err);
    if (rc == YVEX_OK) rc = yvex_runtime_state_residency_reset(f->residency, err);
    return rc;
}

static int lfm_reference_close(lfm_reference *f, yvex_error *err)
{
    LFM_CHECK(yvex_program_device_close(&f->device, err) == YVEX_OK);
    LFM_CHECK(yvex_program_stage_close(&f->output, err) == YVEX_OK);
    LFM_CHECK(yvex_program_sequence_close(&f->sequence, err) == YVEX_OK);
    LFM_CHECK(yvex_program_kernels_close(&f->kernels, err) == YVEX_OK);
    LFM_CHECK(yvex_sequence_state_close_checked(&f->state, err) == YVEX_OK);
    LFM_CHECK(yvex_runtime_state_residency_close(&f->residency, err) == YVEX_OK);
    LFM_CHECK(f->attention_state.release(&f->attention_state.context, err) == YVEX_OK);
    LFM_CHECK(yvex_backend_tensor_release(f->backend, &f->hidden, err) == YVEX_OK);
    if (f->encoded_device) {
        LFM_CHECK(yvex_backend_resident_detach(f->backend, err) == YVEX_OK);
        LFM_CHECK(yvex_backend_tensor_release(f->backend, &f->encoded_device, err) == YVEX_OK);
    }
    yvex_backend_memory_stats memory;
    LFM_CHECK(yvex_backend_get_memory_stats(f->backend, &memory, err) == YVEX_OK && !memory.allocated_bytes);
    LFM_CHECK(yvex_backend_close_checked(&f->backend, err) == YVEX_OK);
    yvex_graph_attention_capacity_plan_close(&f->capacity);
    yvex_program_physical_close(&f->physical); yvex_program_physical_close(&f->output_program);
    yvex_program_execution_close(&f->execution); yvex_ir_module_close(&f->module);
    for (size_t i = 0u; i < f->source_count; ++i) free(f->source[i].data);
    return 0;
}

static int lfm_program_reference(yvex_backend_kind backend)
{
    lfm_reference f = {.backend_kind = backend};
    yvex_error err = {0};
    float hidden[LFM_ROWS * LFM_WIDTH], logits[LFM_ROWS * LFM_WIDTH];
    float single_hidden[LFM_ROWS * LFM_WIDTH], single_logits[LFM_ROWS * LFM_WIDTH];
    LFM_CHECK(lfm_reference_read(&f, &err) == 0);
    int rc = lfm_reference_open(&f, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid program prepare: %s\n", yvex_error_message(&err));
    LFM_CHECK(rc == YVEX_OK);
    rc = lfm_reference_run(&f, 0u, f.rows, hidden, logits, &err);
    if (rc != YVEX_OK) fprintf(stderr, "Liquid whole forward: %s\n", yvex_error_message(&err));
    LFM_CHECK(rc == YVEX_OK);
    size_t different_hidden = 0u, different_logits = 0u;
    float hidden_error = 0.0f, logits_error = 0.0f;
    for (size_t i = 0u; i < f.rows * f.a.hidden; ++i) {
        different_hidden += hidden[i] != f.expected_hidden[i];
        hidden_error = fmaxf(hidden_error, fabsf(hidden[i] - f.expected_hidden[i]));
    }
    for (size_t i = 0u; i < f.rows * f.a.vocabulary; ++i) {
        different_logits += logits[i] != f.expected_logits[i];
        logits_error = fmaxf(logits_error, fabsf(logits[i] - f.expected_logits[i]));
    }
    printf("Liquid whole scaled program (backend=%d) vs pinned upstream: hidden=%zu/%llu max_abs=%g; "
        "logits=%zu/%llu max_abs=%g; required tolerance=0\n", backend, different_hidden, f.rows * f.a.hidden,
        hidden_error, different_logits, f.rows * f.a.vocabulary, logits_error);
    LFM_CHECK(lfm_reference_reset(&f, &err) == YVEX_OK);
    for (size_t i = 0u; i < f.rows; ++i) {
        LFM_CHECK(lfm_reference_run(&f, i, 1u, single_hidden + i * f.a.hidden,
            single_logits + i * f.a.vocabulary, &err) == YVEX_OK);
        if (i >= 2u && i + 1u < f.rows) {
            size_t best = 0u;
            for (size_t j = 1u; j < f.a.vocabulary; ++j)
                if (single_logits[i * f.a.vocabulary + j] > single_logits[i * f.a.vocabulary + best]) best = j;
            LFM_CHECK(best == f.tokens[i + 1u]);
        }
    }
    LFM_CHECK(!memcmp(hidden, single_hidden, f.rows * f.a.hidden * sizeof(float)) &&
        !memcmp(logits, single_logits, f.rows * f.a.vocabulary * sizeof(float)));
    LFM_CHECK(lfm_reference_reset(&f, &err) == YVEX_OK);
    f.cancel_after_attention = 1;
    LFM_CHECK(lfm_reference_run(&f, 0u, 1u, single_hidden, single_logits, &err) == YVEX_ERR_CANCELLED);
    yvex_sequence_state_summary state;
    yvex_graph_attention_state_summary kv;
    LFM_CHECK(yvex_sequence_state_summary_copy(f.state, &state, NULL) == YVEX_OK &&
        !state.committed_position && !state.transaction_active);
    LFM_CHECK(f.attention_state.summary(f.attention_state.context, &kv, NULL) == YVEX_OK &&
        !kv.committed_sequence_length && !kv.transaction_active);
    f.cancel = f.cancel_after_attention = 0;
    LFM_CHECK(lfm_reference_run(&f, 0u, f.rows, single_hidden, single_logits, &err) == YVEX_OK &&
        !memcmp(hidden, single_hidden, f.rows * f.a.hidden * sizeof(float)));
    LFM_CHECK(lfm_reference_close(&f, &err) == 0);
    LFM_CHECK(!different_hidden && !different_logits);
    return 0;
}

int yvex_test_lfm2_program_reference(void) { return lfm_program_reference(YVEX_BACKEND_KIND_CPU); }
int yvex_test_lfm2_program_cuda_reference(void) { return lfm_program_reference(YVEX_BACKEND_KIND_CUDA); }
