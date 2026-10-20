/* Operation/state vertical, not model inference or checkpoint quality. */
#include "tests/test.h"
#include <yvex/internal/program_sequence.h>
#include <yvex/internal/sequence_state.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/qtype.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) YVEX_TEST_ASSERT(x, #x)

static const char conv_identity[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

/* Independent rounding, deliberately not the production bit implementation.
 * Inputs here are finite normal dyadic values; not an exceptional-value oracle. */
static float conv_reference_round(double x)
{
    int exponent;
    if (!x) return (float)x;
    (void)frexp(fabs(x), &exponent);
    double unit = ldexp(1.0, exponent - 8);
    return (float)(nearbyint(x / unit) * unit);
}

static void conv_reference(const float *projection, const float *weight,
                           unsigned int rows, float *out, float *state)
{
    /* Direct causal indexing, no mutable recurrence and no production helper. */
    for (unsigned int t = 0u; t < rows; ++t) for (unsigned int c = 0u; c < 4u; ++c) {
        double sum = 0.0;
        for (unsigned int k = 0u; k < 3u; ++k) {
            int position = (int)t + (int)k - 2;
            if (position >= 0) {
                const float *p = projection + (unsigned int)position * 12u;
                sum += (double)conv_reference_round((double)p[c] * p[8u + c]) * weight[c * 3u + k];
            }
        }
        out[t * 4u + c] = conv_reference_round((double)projection[t * 12u + 4u + c] *
                                               conv_reference_round(sum));
    }
    for (unsigned int c = 0u; c < 4u; ++c) for (unsigned int k = 0u; k < 3u; ++k) {
        int position = (int)rows + (int)k - 3;
        const float *p = position < 0 ? NULL : projection + (unsigned int)position * 12u;
        state[c * 3u + k] = p ? conv_reference_round((double)p[c] * p[8u + c]) : 0.0f;
    }
}

static int conv_compile(yvex_program_physical **out, unsigned int invalid, yvex_error *err)
{
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_sequence_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *e = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 9u, .multiple = 1u};
    yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = YVEX_IR_BF16, .rank = 2u};
    yvex_ir_id dimension, types[4], fn, block = YVEX_IR_NONE, op, args[3], results[2], inputs[2];
    yvex_program_parameter_binding binding = {0};
    int rc = yvex_ir_module_open(&m, "double_gated_conv", conv_identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dimension, err);
    if (rc == YVEX_OK) {
        t.shape[0] = (yvex_ir_extent){dimension, 0u};
        t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, invalid == 1u ? 11u : 12u};
        rc = yvex_ir_type_intern(m, &t, &types[0], err);
    }
    if (rc == YVEX_OK) {
        t.shape[1].extent = 4u;
        rc = yvex_ir_type_intern(m, &t, &types[2], err);
    }
    if (rc == YVEX_OK) {
        t.rank = 3u; t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 4u};
        t.shape[1].extent = 1u; t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        rc = yvex_ir_type_intern(m, &t, &types[1], err);
    }
    if (rc == YVEX_OK) {
        t.kind = YVEX_IR_STATE; t.scalar = YVEX_IR_F32; t.rank = 2u;
        t.shape[1].extent = 3u; t.shape[2] = (yvex_ir_extent){0};
        snprintf(t.domain, sizeof(t.domain), "%s", invalid == 2u ? "ssm.selective" : "convolution.causal");
        rc = yvex_ir_type_intern(m, &t, &types[3], err);
    }
    if (rc == YVEX_OK) {
        inputs[0] = types[0]; inputs[1] = types[3];
        results[0] = types[2]; results[1] = types[3];
        rc = yvex_ir_function_add(m, "forward", inputs, 2u, results, 2u,
            invalid == 3u ? YVEX_IR_READ_STATE : YVEX_IR_READ_STATE | YVEX_IR_WRITE_STATE, &fn, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_attribute attrs[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "conv.weight"},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        memcpy(attrs[1].value.text, conv_identity, sizeof(conv_identity));
        yvex_ir_operation_request r = {.operation = "core.parameter", .result_types = &types[1],
            .result_count = 1u, .attributes = attrs, .attribute_count = 2u};
        block = yvex_ir_function_at(m, fn)->body;
        args[0] = yvex_ir_block_at(m, block)->arguments[0];
        args[2] = yvex_ir_block_at(m, block)->arguments[1];
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_attribute attrs[] = {{.name = "width", .kind = YVEX_IR_ATTR_U64, .value.integer = 4u},
            {.name = "convolution_kernel", .kind = YVEX_IR_ATTR_U64, .value.integer = invalid == 4u ? 1u : 3u}};
        args[1] = yvex_ir_operation_at(m, op)->results[0];
        binding = (yvex_program_parameter_binding){args[1], 0u,
            invalid == 5u ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16};
        yvex_ir_operation_request r = {.operation = "sequence.double_gated_conv", .operands = args,
            .operand_count = 3u, .result_types = results, .result_count = 2u, .attributes = attrs, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        memcpy(results, yvex_ir_operation_at(m, op)->results, sizeof(results));
        if (invalid == 6u) results[1] = args[2];
        yvex_ir_operation_request r = {.operation = "core.return", .operands = results, .operand_count = 2u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&e, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, e, "forward", &binding, 1u, conv_identity, err);
    yvex_program_execution_close(&e);
    yvex_ir_module_close(&m);
    return rc;
}

typedef struct {
    yvex_program_sequence *sequence;
    yvex_program_kernels *kernels;
    int cancel;
} conv_dispatch;

static int conv_cancel(void *opaque) { return ((conv_dispatch *)opaque)->cancel; }
static int conv_invoke(void *opaque, const yvex_program_device_invocation *r,
                        yvex_backend_operation_facts *facts, yvex_error *err)
{
    conv_dispatch *c = opaque;
    yvex_program_sequence_request options = {conv_identity, conv_cancel, c};
    return yvex_program_sequence_invoke(c->sequence, r, &options, facts, err);
}

static int conv_run(yvex_program_device *vm, yvex_sequence_state *state,
    yvex_device_tensor *input, yvex_device_tensor *output, conv_dispatch *c,
    unsigned int start, unsigned int rows, yvex_error *err)
{
    yvex_device_tensor in, out;
    yvex_program_device_argument args[2] = {{0}, {.state_handle = 1u}};
    yvex_program_device_result result;
    yvex_runtime_transaction_participant participant;
    int rc;
    if (!yvex_backend_tensor_f32_subview(input, start * 12u, rows * 12u, &in) ||
        !yvex_backend_tensor_f32_subview(output, start * 4u, rows * 4u, &out)) return YVEX_ERR_BOUNDS;
    in.rank = out.rank = 2u;
    in.dims[0] = out.dims[0] = rows; in.dims[1] = 12u; out.dims[1] = 4u;
    args[0].tensor = &in;
    yvex_device_tensor *outputs[] = {&out};
    rc = yvex_sequence_state_begin(state, start, rows, err);
    if (rc == YVEX_OK) rc = yvex_program_device_run(vm, rows, args, 2u, outputs, 1u,
        conv_cancel, c, &result, err);
    if (yvex_sequence_state_participant(state, &participant, err) != YVEX_OK) return YVEX_ERR_STATE;
    return yvex_runtime_transaction_resolve(&participant, 1u, rc, err);
}

static int conv_program(void)
{
    yvex_program_physical *p = NULL, *copy = NULL;
    yvex_program_device *vm = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CPU};
    yvex_device_tensor *input = NULL, *output = NULL;
    yvex_sequence_state *state = NULL, *unsupported = NULL, *forked = NULL;
    yvex_sequence_state_plan plan;
    yvex_sequence_state_geometry cost;
    yvex_sequence_state_view committed;
    yvex_runtime_session_view session = {0};
    yvex_model_engine_view model = {0};
    yvex_core_bytes bytes = {.maximum = 65536u};
    yvex_backend_tensor_desc d = {.name = "conv-input", .dtype = YVEX_DTYPE_F32,
        .rank = 2u, .dims = {9u, 12u}, .bytes = 108u * sizeof(float)};
    conv_dispatch context = {0};
    const yvex_program_device_kernel implementation = {"double_gated_conv.bf16.v2", conv_invoke};
    float projection[108], weight[12], expected[36], expected_state[12], actual[36];
    unsigned char encoded[24];
    char before[65], after[65];
    yvex_error err = {0};
    for (unsigned int i = 0u; i < 108u; ++i) projection[i] = (float)((int)(i % 17u) - 8) / 8.0f;
    for (unsigned int i = 0u; i < 12u; ++i) {
        weight[i] = (float)((int)(i % 7u) - 3) / 8.0f;
        unsigned short b = yvex_quant_bf16_encode(weight[i]);
        encoded[2u * i] = (unsigned char)b; encoded[2u * i + 1u] = (unsigned char)(b >> 8u);
    }
    conv_reference(projection, weight, 9u, expected, expected_state);
    yvex_program_kernel_parameter parameter = {.tensor_id = 0u,
        .weight = {.encoded = encoded, .encoded_bytes = sizeof(encoded), .row_count = 4u,
            .row_width = 3u, .row_bytes = 6u, .qtype = YVEX_GGUF_QTYPE_BF16}};
    for (unsigned int invalid = 1u; invalid <= 6u; ++invalid)
        CHECK(conv_compile(&p, invalid, &err) != YVEX_OK && !p);
    CHECK(conv_compile(&p, 0u, &err) == YVEX_OK);
    CHECK(yvex_program_physical_encode(p, &bytes, &err) == YVEX_OK);
    CHECK(yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK);
    CHECK(!strcmp(yvex_program_physical_summary_get(p)->identity, yvex_program_physical_summary_get(copy)->identity));
    CHECK(yvex_program_physical_sequence_state(copy, &plan) && plan.binding_count == 1u);
    CHECK(plan.bindings[0].schema_version == YVEX_SEQUENCE_STATE_CONV_SCHEMA_V2 &&
          plan.bindings[0].convolution_state_values == 12u && !plan.bindings[0].recurrent_state_values);
    {
        yvex_sequence_state_binding stale = plan.bindings[0];
        stale.schema_version = YVEX_SEQUENCE_STATE_SCHEMA_V1;
        CHECK(yvex_sequence_state_binding_validate(&stale, &err) == YVEX_ERR_FORMAT);
    }
    CHECK(yvex_sequence_state_plan_measure(&plan, &cost, &err) == YVEX_OK &&
          cost.committed_bytes == 48u && cost.candidate_bytes == 48u && !cost.recurrent_values);
    /* Metadata admission is not device attachment or numerical qualification. */
    CHECK(yvex_sequence_state_open_for_backend(&unsupported, &plan, YVEX_BACKEND_KIND_CUDA, &err) == YVEX_OK);
    CHECK(yvex_sequence_state_begin(unsupported, 0u, 1u, &err) == YVEX_ERR_INVALID_ARG);
    yvex_sequence_state_close(&unsupported);
    CHECK(yvex_sequence_state_open(&state, &plan, &err) == YVEX_OK);
    CHECK(yvex_backend_open(&backend, &options, &err) == YVEX_OK);
    CHECK(yvex_program_kernels_open(&context.kernels, copy, &parameter, 1u, backend, 0u, 0u, &err) == YVEX_OK);
    session.backend = backend; session.sequence_state = state;
    CHECK(yvex_program_sequence_open(&context.sequence, copy, &model, &session, context.kernels,
                                    9u, 0u, 0u, &err) == YVEX_OK);
    CHECK(yvex_program_device_open(&vm, copy, backend, 9u, 0u, 0u, &implementation, 1u, &context, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_alloc(backend, &d, &input, &err) == YVEX_OK);
    d.name = "conv-output"; d.dims[1] = 4u; d.bytes = sizeof(actual);
    CHECK(yvex_backend_tensor_alloc(backend, &d, &output, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_write(backend, input, projection, sizeof(projection), &err) == YVEX_OK);
    CHECK(conv_run(vm, state, input, output, &context, 0u, 9u, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_read(backend, output, actual, sizeof(actual), &err) == YVEX_OK);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    CHECK(yvex_sequence_state_committed(state, 1u, &committed, &err) == YVEX_OK &&
          !memcmp(committed.convolution, expected_state, sizeof(expected_state)));
    CHECK(yvex_sequence_state_fork(&forked, state, &err) == YVEX_OK);
    CHECK(yvex_sequence_state_reset(state, &err) == YVEX_OK);
    CHECK(conv_run(vm, state, input, output, &context, 0u, 2u, &err) == YVEX_OK);
    CHECK(yvex_sequence_state_committed_identity(state, before, &err) == YVEX_OK);
    context.cancel = 1;
    CHECK(conv_run(vm, state, input, output, &context, 2u, 4u, &err) == YVEX_ERR_CANCELLED);
    CHECK(yvex_sequence_state_committed_identity(state, after, &err) == YVEX_OK && !strcmp(before, after));
    context.cancel = 0;
    projection[2u * 12u + 3u] = NAN;
    CHECK(yvex_backend_tensor_write(backend, input, projection, sizeof(projection), &err) == YVEX_OK);
    CHECK(conv_run(vm, state, input, output, &context, 2u, 4u, &err) == YVEX_ERR_FORMAT);
    CHECK(yvex_sequence_state_committed_identity(state, after, &err) == YVEX_OK && !strcmp(before, after));
    projection[2u * 12u + 3u] = (float)((int)(27u % 17u) - 8) / 8.0f;
    CHECK(yvex_backend_tensor_write(backend, input, projection, sizeof(projection), &err) == YVEX_OK);
    CHECK(conv_run(vm, state, input, output, &context, 2u, 4u, &err) == YVEX_OK);
    for (unsigned int row = 6u; row < 9u; ++row)
        CHECK(conv_run(vm, state, input, output, &context, row, 1u, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_read(backend, output, actual, sizeof(actual), &err) == YVEX_OK);
    CHECK(!memcmp(actual, expected, sizeof(actual)));
    CHECK(yvex_sequence_state_committed(state, 1u, &committed, &err) == YVEX_OK &&
          !memcmp(committed.convolution, expected_state, sizeof(expected_state)));
    CHECK(yvex_sequence_state_committed(forked, 1u, &committed, &err) == YVEX_OK &&
          !memcmp(committed.convolution, expected_state, sizeof(expected_state)));
    CHECK(yvex_program_device_close(&vm, &err) == YVEX_OK);
    CHECK(yvex_program_sequence_close(&context.sequence, &err) == YVEX_OK);
    CHECK(yvex_program_kernels_close(&context.kernels, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_release(backend, &input, &err) == YVEX_OK);
    CHECK(yvex_backend_tensor_release(backend, &output, &err) == YVEX_OK);
    CHECK(yvex_backend_close_checked(&backend, &err) == YVEX_OK);
    yvex_sequence_state_close(&state); yvex_sequence_state_close(&forked);
    yvex_program_physical_close(&p); yvex_program_physical_close(&copy);
    free(bytes.data);
    return 0;
}

int yvex_test_short_conv(void)
{
    yvex_short_conv_geometry g, bad;
    yvex_error err;
    float p[12] = {0}, weight[12] = {0}, next[12], output[4];
    yvex_short_conv_cpu_result result;
    yvex_short_conv_cpu_request r = {.rows = 1u, .projection = p, .projection_capacity = 12u,
        .weight = weight, .weight_capacity = 12u, .next_state = next, .next_state_capacity = 12u,
        .output = output, .output_capacity = 4u};
    CHECK(yvex_short_conv_geometry_seal(&g, 4u, 3u, &err) == YVEX_OK);
    bad = g; bad.state_values++;
    CHECK(yvex_short_conv_execute_cpu(&bad, &r, &result, &err) == YVEX_ERR_FORMAT);
    r.output_capacity = 3u;
    CHECK(yvex_short_conv_execute_cpu(&g, &r, &result, &err) == YVEX_ERR_BOUNDS);
    r.output_capacity = 4u; r.next_state = p;
    CHECK(yvex_short_conv_execute_cpu(&g, &r, &result, &err) == YVEX_ERR_BOUNDS);
    r.next_state = next; p[0] = 0.1f;
    CHECK(yvex_short_conv_execute_cpu(&g, &r, &result, &err) == YVEX_ERR_FORMAT && !result.complete);
    p[0] = 0.0f; weight[0] = INFINITY;
    CHECK(yvex_short_conv_execute_cpu(&g, &r, &result, &err) == YVEX_ERR_FORMAT);
    weight[0] = 0.0f;
    CHECK(yvex_short_conv_execute_cpu(&g, &r, &result, &err) == YVEX_OK && result.complete);
    CHECK(conv_program() == 0);
    printf("short-conv: typed import; exact dyadic F64 oracle; chunk/single; commit/abort/fork; CPU-only admission\n");
    return 0;
}

int yvex_test_short_conv_reference(void)
{
    const char *path = getenv("YVEX_LFM2_CONV_ORACLE");
    FILE *f = path ? fopen(path, "r") : NULL;
    unsigned int rows, width, kernel;
    char trailing;
    yvex_short_conv_geometry g;
    yvex_short_conv_cpu_result result;
    yvex_error err;
    size_t projection_count, state_count, output_count, input_count;
    float *fixture, *actual, *next, *single, *previous;
    CHECK(f && fscanf(f, "%u%u%u", &rows, &width, &kernel) == 3);
    CHECK(rows == 9u && width == 2048u && kernel == 3u);
    projection_count = (size_t)rows * width * 3u;
    state_count = (size_t)width * kernel; output_count = (size_t)rows * width;
    input_count = projection_count + state_count * 2u + output_count;
    fixture = malloc(input_count * sizeof(float)); actual = malloc(output_count * sizeof(float));
    next = malloc(state_count * sizeof(float)); previous = calloc(state_count, sizeof(float));
    single = malloc(output_count * sizeof(float));
    CHECK(fixture && actual && next && previous && single);
    for (size_t i = 0u; i < input_count; ++i) CHECK(fscanf(f, "%f", &fixture[i]) == 1 && isfinite(fixture[i]));
    CHECK(fscanf(f, " %c", &trailing) == EOF);
    fclose(f);
    CHECK(yvex_short_conv_geometry_seal(&g, width, kernel, &err) == YVEX_OK);
    yvex_short_conv_cpu_request request = {.rows = rows, .projection = fixture,
        .projection_capacity = projection_count, .weight = fixture + projection_count,
        .weight_capacity = state_count, .next_state = next, .next_state_capacity = state_count,
        .output = actual, .output_capacity = output_count};
    CHECK(yvex_short_conv_execute_cpu(&g, &request, &result, &err) == YVEX_OK);
    size_t differences = 0u;
    float maximum = 0.0f;
    for (size_t i = 0u; i < output_count; ++i) {
        float reference = fixture[projection_count + state_count + i];
        differences += actual[i] != reference;
        maximum = fmaxf(maximum, fabsf(actual[i] - reference));
    }
    printf("LFM2 real convolution reference: values=%zu differing=%zu max_abs=%.9g state_values=%zu\n",
           output_count, differences, maximum, state_count);
    CHECK(!differences && !memcmp(next, fixture + input_count - state_count, state_count * sizeof(float)));
    for (unsigned int row = 0u; row < rows; ++row) {
        request.rows = 1u; request.projection = fixture + row * width * 3u;
        request.projection_capacity = (size_t)width * 3u;
        request.state = previous; request.state_capacity = state_count;
        request.output = single + row * width; request.output_capacity = width;
        CHECK(yvex_short_conv_execute_cpu(&g, &request, &result, &err) == YVEX_OK);
        memcpy(previous, next, state_count * sizeof(float));
    }
    CHECK(!memcmp(single, actual, output_count * sizeof(float)) &&
          !memcmp(next, fixture + input_count - state_count, state_count * sizeof(float)));
    free(single); free(previous); free(next); free(actual); free(fixture);
    return 0;
}
