/* Pure mHC ingress through the real three-result physical executor. */
#ifndef TESTS_SUPPORT_MHC_INGRESS_PROGRAM_H
#define TESTS_SUPPORT_MHC_INGRESS_PROGRAM_H
#include "tests/test.h"
#include <yvex/internal/program_stage.h>
#include <yvex/internal/quant_numeric.h>
#include <yvex/internal/moe.h>
#include <math.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int test_ingress_compile(yvex_program_physical **out, unsigned int bad, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 16384u, .multiple = 1u};
    yvex_ir_id dim, types[7], fn, block = YVEX_IR_NONE, op, operands[4];
    yvex_program_parameter_binding bindings[2];
    yvex_ir_attribute attrs[] = {
        {.name = "epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-6},
        {.name = "mhc_epsilon", .kind = YVEX_IR_ATTR_F64, .value.real = 1e-6},
        {.name = "post_multiplier", .kind = YVEX_IR_ATTR_F64, .value.real = 2.0},
        {.name = "sinkhorn_iterations", .kind = YVEX_IR_ATTR_U64, .value.integer = 3u}};
    int rc = yvex_ir_module_open(&m, "ingress_oracle", identity, dialects, 2u, err);
    if (out) *out = NULL;
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (unsigned int i = 0u; rc == YVEX_OK && i < 7u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR,
            .scalar = i == 0u || i == 4u ? YVEX_IR_BF16 : YVEX_IR_F32,
            .rank = i == 0u || i == 6u ? 3u : i == 2u || i == 3u ? 1u : 2u};
        t.shape[0] = i == 2u || i == 3u ? (yvex_ir_extent){YVEX_IR_NONE, i == 2u ? 3u : 8u} :
            (yvex_ir_extent){dim, 0u};
        if (t.rank > 1u) t.shape[1] = (yvex_ir_extent){YVEX_IR_NONE, i == 1u ? 8u : i == 4u ? 3u : 2u};
        if (t.rank == 3u) t.shape[2] = (yvex_ir_extent){YVEX_IR_NONE, i == 0u ? 3u : 2u};
        if (bad == i + 1u) {
            if (i == 0u || i == 4u) t.scalar = YVEX_IR_F32;
            else t.shape[t.rank - 1u].extent++;
        }
        if (bad == 8u && i == 1u) t.shape[0] = (yvex_ir_extent){YVEX_IR_NONE, 3u};
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "ingress", types, 2u, types + 4u, 3u, 0u, &fn, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, fn)->body;
        operands[0] = yvex_ir_block_at(m, block)->arguments[0];
        operands[1] = yvex_ir_block_at(m, block)->arguments[1];
    }
    for (unsigned int i = 0u; rc == YVEX_OK && i < 2u; ++i) {
        yvex_ir_attribute a[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        (void)snprintf(a[0].value.text, sizeof(a[0].value.text), "weight_%u", i);
        memcpy(a[1].value.text, identity, 65u);
        yvex_ir_operation_request p = {.operation = "core.parameter", .result_types = types + i + 2u,
            .result_count = 1u, .attributes = a, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &p, &op, err);
        if (rc == YVEX_OK) {
            operands[i + 2u] = yvex_ir_operation_at(m, op)->results[0];
            bindings[i] = (yvex_program_parameter_binding){operands[i + 2u], 32u + i, YVEX_GGUF_QTYPE_F32};
        }
    }
    if (rc == YVEX_OK) {
        if (bad >= 9u && bad <= 11u) attrs[bad - 9u].value.real = 0.0;
        if (bad == 12u) attrs[3].value.integer = 0u;
        yvex_ir_operation_request r = {.operation = "mhc.residual_pre", .operands = operands, .operand_count = 4u,
            .result_types = types + 4u, .result_count = 3u, .attributes = attrs, .attribute_count = 4u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) {
        yvex_ir_operation_request r = {.operation = "core.return", .operands = yvex_ir_operation_at(m, op)->results,
            .operand_count = 3u};
        rc = yvex_ir_operation_add(m, block, &r, &op, err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "ingress", bindings, 2u, identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_ingress_cancel(void *context) { (void)context; return 1; }

static int test_weighted_rms_compile(yvex_program_physical **out, unsigned int bad, yvex_error *err)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    yvex_ir_dialect dialects[] = {*yvex_ir_core_dialect(), *yvex_ir_neural_dialect()};
    yvex_ir_module *m = NULL;
    yvex_program_execution *execution = NULL;
    yvex_ir_id types[3], fn, block, op, args[2], dim;
    yvex_ir_dimension rows = {.name = "rows", .minimum = 1u, .maximum = 3u, .multiple = 1u};
    yvex_program_parameter_binding binding = {0};
    int rc = yvex_ir_module_open(&m, "weighted_rms_oracle", identity, dialects, 2u, err);
    if (rc == YVEX_OK) rc = yvex_ir_dimension_add(m, &rows, &dim, err);
    for (unsigned int i = 0u; rc == YVEX_OK && i < 3u; ++i) {
        yvex_ir_type t = {.kind = YVEX_IR_TENSOR, .scalar = i == 1u ? YVEX_IR_F32 : YVEX_IR_BF16,
            .rank = i == 1u ? 1u : 2u, .shape = {{YVEX_IR_NONE, i == 1u ? 4096u : 3u},
                {YVEX_IR_NONE, 4096u}}};
        if (i == 1u) t.shape[1] = (yvex_ir_extent){0};
        else t.shape[0] = (yvex_ir_extent){dim, 0u};
        if (bad == 1u && i == 0u) t.scalar = YVEX_IR_F32;
        if (bad == 2u && i == 2u) t.scalar = YVEX_IR_F32;
        if (bad == 3u && i == 1u) t.shape[0].extent--;
        rc = yvex_ir_type_intern(m, &t, &types[i], err);
    }
    if (rc == YVEX_OK) rc = yvex_ir_function_add(m, "norm", types, 1u, types + 2u, 1u, 0u, &fn, err);
    if (rc == YVEX_OK) {
        block = yvex_ir_function_at(m, fn)->body;
        args[0] = yvex_ir_block_at(m, block)->arguments[0];
        yvex_ir_attribute a[] = {{.name = "parameter", .kind = YVEX_IR_ATTR_SYMBOL, .value.text = "weight"},
            {.name = "source", .kind = YVEX_IR_ATTR_TEXT}};
        memcpy(a[1].value.text, identity, 65u);
        yvex_ir_operation_request p = {.operation = "core.parameter", .result_types = types + 1u,
            .result_count = 1u, .attributes = a, .attribute_count = 2u};
        rc = yvex_ir_operation_add(m, block, &p, &op, err);
        if (rc == YVEX_OK) {
            args[1] = yvex_ir_operation_at(m, op)->results[0];
            binding = (yvex_program_parameter_binding){args[1], 71u, YVEX_GGUF_QTYPE_F32};
            yvex_ir_attribute epsilon = {.name = "epsilon", .kind = YVEX_IR_ATTR_F64,
                .value.real = bad == 4u ? 0.0 : 1e-50};
            yvex_ir_operation_request n = {.operation = "nn.weighted_rms", .operands = args, .operand_count = 2u,
                .result_types = types + 2u, .result_count = 1u, .attributes = &epsilon, .attribute_count = 1u};
            rc = yvex_ir_operation_add(m, block, &n, &op, err);
        }
        if (rc == YVEX_OK) {
            yvex_ir_operation_request r = {.operation = "core.return",
                .operands = yvex_ir_operation_at(m, op)->results, .operand_count = 1u};
            rc = yvex_ir_operation_add(m, block, &r, &op, err);
        }
    }
    if (rc == YVEX_OK) rc = yvex_ir_seal(m, err);
    if (rc == YVEX_OK) rc = yvex_program_execution_compile(&execution, m, err);
    if (rc == YVEX_OK) rc = yvex_program_physical_compile(out, execution, "norm", &binding, 1u, identity, err);
    yvex_program_execution_close(&execution); yvex_ir_module_close(&m);
    return rc;
}

static int test_weighted_rms(yvex_backend *backend)
{
    enum { WIDTH = 4096, ROWS = 3, COUNT = WIDTH * ROWS };
    float *data = calloc(COUNT * 3u + WIDTH, sizeof(float));
    YVEX_TEST_ASSERT(data, "weighted RMS test storage");
    float *x = data, *expected = x + COUNT, *actual = expected + COUNT, *weight = actual + COUNT;
    const int cuda = yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CUDA;
    for (unsigned int i = 0u; i < WIDTH; ++i) {
        weight[i] = (float)(1u + i % 5u) * 0.25f;
        x[i] = (float)((int)(i % 11u) - 5) * 0.0625f;
        x[WIDTH + i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(i % 2u ? 1e20f : -1e20f));
        x[2u * WIDTH + i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(i % 2u ? 1e-25f : -1e-25f));
    }
    /* Test-owned retained reduction contract, not upstream conformance:
     * CUDA uses 256 F64 lanes/tree; CPU retains source-order F64 reduction.
     * Both scale using F64 epsilon. */
    for (unsigned int row = 0u; row < ROWS; ++row) {
        double sum = 0.0;
        if (cuda) {
            double lanes[256] = {0};
            for (unsigned int lane = 0u; lane < 256u; ++lane)
                for (unsigned int i = lane; i < WIDTH; i += 256u)
                    lanes[lane] += (double)x[row * WIDTH + i] * x[row * WIDTH + i];
            for (unsigned int offset = 128u; offset; offset >>= 1u)
                for (unsigned int lane = 0u; lane < offset; ++lane) lanes[lane] += lanes[lane + offset];
            sum = lanes[0];
        }
        if (!cuda) {
            sum = 0.0;
            for (unsigned int i = 0u; i < WIDTH; ++i) sum += (double)x[row * WIDTH + i] * x[row * WIDTH + i];
        }
        double inverse = 1.0 / sqrt(sum / WIDTH + 1e-50);
        for (unsigned int i = 0u; i < WIDTH; ++i)
            expected[row * WIDTH + i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
                (float)((double)x[row * WIDTH + i] * inverse * weight[i])));
    }
    yvex_error err = {0};
    yvex_program_physical *program = NULL, *refused = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend_operation_facts facts;
    yvex_program_kernel_parameter parameter = {.tensor_id = 71u,
        .weight = {(const unsigned char *)weight, WIDTH * sizeof(float), 1u, WIDTH,
            WIDTH * sizeof(float), YVEX_GGUF_QTYPE_F32}};
    YVEX_TEST_ASSERT(test_weighted_rms_compile(&program, 0u, &err) == YVEX_OK, "weighted RMS compiles");
    for (unsigned int bad = 1u; bad <= 4u; ++bad)
        YVEX_TEST_ASSERT(test_weighted_rms_compile(&refused, bad, &err) == YVEX_ERR_FORMAT && !refused,
            "weighted RMS rejects wrong precision, channels and epsilon before execution");
    int rc = yvex_program_stage_open(&stage, program, &parameter, 1u, backend, ROWS, 1, 0u, 0u, &err);
    if (rc == YVEX_OK) rc = yvex_program_stage_host(stage, ROWS,
        (const float *[]){x}, 1u, (float *[]){actual}, 1u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "weighted RMS: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK,
        "weighted RMS executes ordinary, overflowing-square and tiny-epsilon rows");
    double maximum = 0.0;
    for (unsigned int i = 0u; i < COUNT; ++i) {
        double error = fabs((double)actual[i] - expected[i]);
        if (error > maximum) maximum = error;
        YVEX_TEST_ASSERT(isfinite(actual[i]) && error == 0.0, "weighted RMS preserves exact admitted reduction");
    }
    actual[0] = 123.0f; x[0] = NAN;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, ROWS, (const float *[]){x}, 1u,
        (float *[]){actual}, 1u, NULL, NULL, &facts, &err) == YVEX_ERR_FORMAT && actual[0] == 123.0f,
        "nonfinite weighted RMS input does not publish results");
    printf("weighted RMS %s: values=%u width=%u epsilon=1e-50 max_abs=%.12g tolerance=0 "
        "square_overflow=avoided invalid_input=unpublished compiler_negatives=4\n",
        cuda ? "CUDA" : "CPU", COUNT, WIDTH, maximum);
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "weighted RMS cleanup");
    yvex_program_physical_close(&program); free(data);
    return 0;
}

static int test_ingress_population(yvex_program_physical *program, yvex_backend *backend,
    const yvex_program_kernel_parameter *parameters)
{
    if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU) return 0;
    yvex_program_kernels *kernels = NULL;
    yvex_error err = {0};
    unsigned long long host, base, one, three, refused;
    YVEX_TEST_ASSERT(yvex_program_physical_summary_get(program)->maximum_rows == 16384u &&
        yvex_program_kernels_open(&kernels, program, parameters, 2u, backend, 0u, 256u, &err) == YVEX_OK,
        "large semantic horizon binds under a small actual preparation budget");
    yvex_program_kernels_resources(kernels, &host, &base);
    YVEX_TEST_ASSERT(base == 44u, "cold binding retains only the two decoded parameters");
    YVEX_TEST_ASSERT(yvex_program_kernels_prepare(kernels, 1u, 0u, 256u, &err) == YVEX_OK,
        "single population prepares only its scratch");
    yvex_program_kernels_resources(kernels, &host, &one);
    YVEX_TEST_ASSERT(one == base + 24u, "one row requires six F32 scratch values");
    YVEX_TEST_ASSERT(yvex_program_kernels_prepare(kernels, 3u, 0u, one + 71u, &err) == YVEX_ERR_BOUNDS,
        "scratch growth accounts replacement overlap and refuses a one-byte-short budget");
    yvex_program_kernels_resources(kernels, &host, &refused);
    YVEX_TEST_ASSERT(refused == one &&
        yvex_program_kernels_prepare(kernels, 1u, 0u, 256u, &err) == YVEX_OK,
        "failed growth preserves the existing prepared population and accounting");
    YVEX_TEST_ASSERT(yvex_program_kernels_prepare(kernels, 3u, 0u, 256u, &err) == YVEX_OK,
        "larger invocation prepares within its explicit peak budget");
    yvex_program_kernels_resources(kernels, &host, &three);
    YVEX_TEST_ASSERT(three == base + 72u &&
        yvex_program_kernels_prepare(kernels, 1u, 0u, 256u, &err) == YVEX_OK,
        "three-row scratch replaces rather than accumulates the old buffer");
    yvex_program_kernels_resources(kernels, &host, &refused);
    YVEX_TEST_ASSERT(refused == three && yvex_program_kernels_close(&kernels, &err) == YVEX_OK,
        "smaller invocations reuse scratch and checked close releases it");
    printf("mHC preparation CUDA: horizon=16384 cold=%llu one_row=%llu three_rows=%llu bytes "
        "growth_peak=%llu one_byte_short=refused prior_population=retained\n", base, one, three, one + 72u);
    return 0;
}

static int test_ingress_nonzero(yvex_program_physical *program, yvex_backend *backend)
{
    const float scale[] = {0.5f, 0.75f, 1.25f};
    const float base[] = {-0.25f, 0.5f, 0.75f, -0.5f, 0.125f, -0.25f, 0.375f, -0.5f};
    const float x[] = {1, 2, 3, -2, 4, 1};
    const float mixes[] = {-3, -2.75f, -2.5f, -2.25f, -2, -1.75f, -1.5f, -1.25f};
    /* Independent scalar evaluation: inverse RMS=0.4140393001163312,
     * pre coefficients=.2950350042822035/.4826819220627415. F32
     * matrix storage is applied at each of three Sinkhorn iterations. These
     * are component equation vectors, NOT upstream/whole-model vectors. */
    const float expected[] = {-0.671875f, 2.515625f, 1.3671875f,
        0.9868389368057251f, 0.4634162187576294f,
        0.4378267526626587f, 0.5621793866157532f, 0.5621722340583801f, 0.43781962990760803f};
    float actual[9] = {0};
    yvex_program_kernel_parameter parameters[] = {
        {.tensor_id = 32u, .weight = {(const unsigned char *)scale, sizeof(scale), 1u, 3u, sizeof(scale), YVEX_GGUF_QTYPE_F32}},
        {.tensor_id = 33u, .weight = {(const unsigned char *)base, sizeof(base), 1u, 8u, sizeof(base), YVEX_GGUF_QTYPE_F32}}};
    yvex_program_stage *stage = NULL;
    yvex_backend_operation_facts facts;
    yvex_error err = {0};
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, program, parameters, 2u, backend, 1u, 1, 0u, 0u, &err) == YVEX_OK,
        "nonzero coefficient program binds a distinct immutable parameter set");
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, (const float *[]){x, mixes}, 2u,
        (float *[]){actual, actual + 3u, actual + 5u}, 3u, NULL, NULL, &facts, &err) == YVEX_OK,
        "nonuniform gates and mixing matrix execute");
    double maximum = 0.0, relative = 0.0;
    unsigned int worst = 0u;
    for (unsigned int i = 0u; i < 9u; ++i) {
        double error = fabs((double)actual[i] - expected[i]);
        if (error > maximum) { maximum = error; worst = i; }
        if (error / fabs(expected[i]) > relative) relative = error / fabs(expected[i]);
        YVEX_TEST_ASSERT(isfinite(actual[i]) && error <= (i < 3u ? 0.0 : 1e-6),
            "nonuniform ingress matches independent fixed component vectors");
    }
    printf("mHC ingress nonuniform %s: values=9 expected[0]=%.9g observed[0]=%.9g "
        "max_abs=%.12g max_rel=%.12g worst=%u collapse_tolerance=0 gate_matrix_tolerance=1e-6\n",
        yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA",
        (double)expected[0], (double)actual[0], maximum, relative, worst);
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "nonuniform stage releases");
    return 0;
}

static int test_ingress_composition(yvex_backend *backend, unsigned int norm_qtype)
{
    const char *identity = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    float function[48], scale[] = {0.5f, 0.75f, 1.25f};
    float base[] = {-0.25f, 0.5f, 0.75f, -0.5f, 0.125f, -0.25f, 0.375f, -0.5f};
    float norm[] = {0.75f, 1.5f, 0.5f}, x[] = {1, 2, 3, -2, 4, 1};
    unsigned short encoded_norm[3];
    float expected[11] = {0}, actual[11] = {0}, router[] = {1.0f, 0.5f, -0.25f, -0.5f, 1.0f, 0.25f};
    yvex_moe_layer_plan layer = {.hidden_width = 3u, .residual_streams = 2u, .expanded_width = 6u,
        .mhc_mixing_rows = 8u, .mhc_sinkhorn_iterations = 3u, .rms_epsilon = 1e-6,
        .mhc_epsilon = 1e-6, .mhc_post_multiplier = 2.0, .routed_experts = 2u};
    yvex_device_tensor *resident = NULL;
    const unsigned int slots[] = {YVEX_MOE_WEIGHT_MHC_FUNCTION, YVEX_MOE_WEIGHT_MHC_SCALE,
        YVEX_MOE_WEIGHT_MHC_BASE, YVEX_MOE_WEIGHT_FFN_NORM, YVEX_MOE_WEIGHT_ROUTER};
    const unsigned long long widths[] = {6u, 3u, 8u, 3u, 3u}, counts[] = {8u, 1u, 1u, 1u, 2u};
    const float *data[] = {function, scale, base, norm, router};
    yvex_program_kernel_parameter parameters[5] = {0};
    yvex_program_physical *program = NULL, *copy = NULL, *refused = NULL;
    yvex_program_stage *stage = NULL;
    yvex_backend_operation_facts facts;
    yvex_core_bytes encoded = {.maximum = 65536u};
    yvex_error err = {0};
    for (size_t i = 0u; i < 48u; ++i) function[i] = (float)((int)(i % 11u) - 5) * 0.0625f;
    for (size_t i = 0u; i < 5u; ++i) {
        layer.tensor_ids[slots[i]] = 32u + i; layer.qtypes[slots[i]] = YVEX_GGUF_QTYPE_F32;
        unsigned long long bytes = widths[i] * counts[i] * sizeof(float);
        parameters[i] = (yvex_program_kernel_parameter){.tensor_id = 32u + i,
            .weight = {(const unsigned char *)data[i], bytes, counts[i], widths[i],
                widths[i] * sizeof(float), YVEX_GGUF_QTYPE_F32}};
    }
    if (norm_qtype == YVEX_GGUF_QTYPE_BF16) {
        for (size_t i = 0u; i < 3u; ++i) encoded_norm[i] = yvex_quant_bf16_encode(norm[i]);
        layer.qtypes[YVEX_MOE_WEIGHT_FFN_NORM] = norm_qtype;
        parameters[3].weight.encoded = (const unsigned char *)encoded_norm;
        parameters[3].weight.encoded_bytes = parameters[3].weight.row_bytes = sizeof(encoded_norm);
        parameters[3].weight.qtype = norm_qtype;
    }
    /* Test-owned replay of the retired scalar composition. The numerical
     * operation has separate equation vectors; this is migration preservation. */
    float mixes[8];
    for (size_t i = 0u; i < 8u; ++i) {
        yvex_quant_failure failure = {0};
        YVEX_TEST_ASSERT(yvex_quant_cpu_dot(YVEX_GGUF_QTYPE_F32, (const unsigned char *)(function + i * 6u),
            6u * sizeof(float), x, 6u, mixes + i, &failure, &err) == YVEX_OK, "scalar projection control");
    }
    yvex_mhc_pre_request pre = {.geometry = {2u, 3u, 3u, 1e-6, 1e-6, 2.0}, .residual = x,
        .linear_mixes = mixes, .scale = scale, .base = base, .rows = 1u, .residual_stride = 6u, .mix_stride = 8u,
        .collapsed = expected, .post = expected + 3u, .combination = expected + 5u,
        .collapsed_stride = 3u, .post_stride = 2u, .combination_stride = 4u};
    YVEX_TEST_ASSERT(yvex_mhc_pre_f32(&pre, &err) == YVEX_OK, "scalar ingress control");
    double sum = 0.0;
    for (size_t i = 0u; i < 3u; ++i) sum += (double)expected[i] * expected[i];
    double inverse = 1.0 / sqrt(sum / 3.0 + 1e-6);
    for (size_t i = 0u; i < 3u; ++i)
        expected[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode((float)((double)expected[i] * inverse * norm[i])));
    for (size_t i = 0u; i < 2u; ++i) {
        double dot = 0.0;
        for (size_t j = 0u; j < 3u; ++j) dot += (double)router[i * 3u + j] * expected[j];
        expected[9u + i] = (float)dot;
    }
    int rc = yvex_moe_ingress_program_import(&program, &layer, identity, identity, 3u, &err);
    if (rc != YVEX_OK) fprintf(stderr, "ingress import: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_program_physical_summary_get(program)->input_count == 1u &&
        yvex_program_physical_summary_get(program)->result_count == 4u,
        "cold importer composes ingress and router projection with four explicit results");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(program, &encoded, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, encoded.data, encoded.count, &err) == YVEX_OK &&
        !strcmp(yvex_program_physical_summary_get(copy)->identity, yvex_program_physical_summary_get(program)->identity),
        "complete ingress survives physical container import with exact identity");
    layer.expanded_width++;
    YVEX_TEST_ASSERT(yvex_moe_ingress_program_import(&refused, &layer, identity, identity, 3u, &err) ==
        YVEX_ERR_FORMAT && !refused, "contradictory source geometry refuses before lowering");
    layer.expanded_width--;
    layer.qtypes[YVEX_MOE_WEIGHT_MHC_FUNCTION] = UINT_MAX;
    YVEX_TEST_ASSERT(yvex_moe_ingress_program_import(&refused, &layer, identity, identity, 3u, &err) !=
        YVEX_OK && !refused, "row-dot obligation cannot admit an unknown parameter numerical representation");
    layer.qtypes[YVEX_MOE_WEIGHT_MHC_FUNCTION] = YVEX_GGUF_QTYPE_F32;
    if (yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CUDA) {
        unsigned char *mapped = NULL;
        yvex_backend_tensor_desc d = {.name = "ingress-function", .dtype = YVEX_DTYPE_I8,
            .rank = 1u, .dims = {sizeof(function) + sizeof(router)}, .bytes = sizeof(function) + sizeof(router)};
        YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &mapped, &err) == YVEX_OK &&
            yvex_backend_resident_attach(backend, mapped, d.bytes, resident, 1u, &err) == YVEX_OK,
            "physical affine parameter has exact device residency");
        memcpy(mapped, function, sizeof(function));
        memcpy(mapped + sizeof(function), router, sizeof(router));
        parameters[0].weight.encoded = mapped;
        parameters[4].weight.encoded = mapped + sizeof(function);
    }
    YVEX_TEST_ASSERT(yvex_program_stage_open(&stage, copy, parameters, 5u, backend, 1u, 1, 0u, 0u, &err) == YVEX_OK,
        "whole ingress prepares immutable parameters");
    rc = yvex_program_stage_host(stage, 1u, (const float *[]){x}, 1u,
        (float *[]){actual, actual + 3u, actual + 5u, actual + 9u}, 4u, NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "ingress composition: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK, "complete compiled ingress executes");
    double maximum = 0.0;
    for (size_t i = 0u; i < 11u; ++i) {
        double error = fabs((double)actual[i] - expected[i]);
        if (error > maximum) maximum = error;
        YVEX_TEST_ASSERT(isfinite(actual[i]) && error <= (i < 3u ||
            yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU ? 0.0 : 1e-6),
            "compiled projection, ingress and normalization preserve the scalar control");
    }
    printf("MoE ingress composition %s norm_qtype=%u: values=11 expected[0]=%.9g observed[0]=%.9g max_abs=%.12g "
        "normalized_tolerance=0 gate_matrix_tolerance=%g oracle=internal-preservation\n",
        yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", norm_qtype, (double)expected[0],
        (double)actual[0], maximum, yvex_backend_kind_of(backend) == YVEX_BACKEND_KIND_CPU ? 0.0 : 1e-6);
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK, "whole ingress releases resources");
    if (resident) YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "ingress parameter residency releases");
    yvex_program_physical_close(&copy); yvex_program_physical_close(&program); free(encoded.data);
    return 0;
}

static int test_ingress_execute(yvex_backend_kind kind)
{
    /* Zero affine logits give an analytic oracle: gates=1, collapse=(.5+eps)
     * times the ordered stream sum, and every Sinkhorn entry is identical.
     * Nonzero per-row mixes exercise the dependency while zero scale makes
     * the expected gate independent of norm implementation/reduction order. */
    const float scale[3] = {0}, base[8] = {0};
    float input[18] = {1, 2, 3, -2, 4, 1, 0.25f, -1, 2, 3, 0.5f, -4, 4, -2, 0.5f, 2, 4, -1};
    float mixes[24], expected[9], collapsed[9] = {0}, gates[6] = {0}, matrix[12] = {0};
    float single[9] = {0}, original[18];
    double maximum = 0.0;
    float entry = (float)(0.5 + 1e-6);
    for (unsigned int i = 0u; i < 5u; ++i) entry = (float)((double)entry / (2.0 * entry + 1e-6));
    memcpy(original, input, sizeof(input));
    for (unsigned int i = 0u; i < 24u; ++i) mixes[i] = (float)((int)i - 12) * 0.25f;
    for (unsigned int row = 0u; row < 3u; ++row)
        for (unsigned int lane = 0u; lane < 3u; ++lane) {
            float sum = (float)((0.5 + 1e-6) * input[row * 6u + lane]);
            sum += (float)((0.5 + 1e-6) * input[row * 6u + 3u + lane]);
            expected[row * 3u + lane] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(sum));
        }
    yvex_program_physical *program = NULL, *copy = NULL, *bad = NULL;
    yvex_program_stage *stage = NULL, *refused = NULL;
    yvex_backend *backend = NULL;
    yvex_backend_options options = {.kind = kind};
    yvex_backend_memory_stats before, after;
    yvex_backend_operation_facts facts;
    yvex_core_bytes bytes = {.maximum = 65536u}, again = {.maximum = 65536u};
    yvex_program_kernel_parameter parameters[2] = {
        {.tensor_id = 32u, .weight = {(const unsigned char *)scale, sizeof(scale), 1u, 3u, sizeof(scale), YVEX_GGUF_QTYPE_F32}},
        {.tensor_id = 33u, .weight = {(const unsigned char *)base, sizeof(base), 1u, 8u, sizeof(base), YVEX_GGUF_QTYPE_F32}}};
    yvex_error err = {0};
    YVEX_TEST_ASSERT(test_ingress_compile(&program, 0u, &err) == YVEX_OK, "three-result ingress compiles");
    for (unsigned int i = 1u; i <= 12u; ++i)
        YVEX_TEST_ASSERT(test_ingress_compile(&bad, i, &err) != YVEX_OK && !bad,
            "ingress rejects malformed operand/result precision, geometry, rows, epsilons and iteration count");
    YVEX_TEST_ASSERT(yvex_program_physical_encode(program, &bytes, &err) == YVEX_OK &&
        yvex_program_physical_decode(&copy, bytes.data, bytes.count, &err) == YVEX_OK &&
        yvex_program_physical_encode(copy, &again, &err) == YVEX_OK && bytes.count == again.count &&
        !memcmp(bytes.data, again.data, bytes.count), "ingress physical identity roundtrips exactly");
    YVEX_TEST_ASSERT(yvex_backend_open(&backend, &options, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &before, &err) == YVEX_OK &&
        yvex_program_stage_open(&stage, copy, parameters, 2u, backend, 3u, 1, 0u, 0u, &err) == YVEX_OK,
        "parameter-bound ingress stage opens");
    float *outputs[] = {collapsed, gates, matrix};
    int rc = yvex_program_stage_host(stage, 3u, (const float *[]){input, mixes}, 2u, outputs, 3u,
        NULL, NULL, &facts, &err);
    if (rc != YVEX_OK) fprintf(stderr, "ingress execution: %s: %s\n", yvex_error_where(&err), yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && !memcmp(collapsed, expected, sizeof(expected)) &&
        !memcmp(input, original, sizeof(input)), "collapse equals analytic BF16 oracle; pure input is unchanged");
    for (unsigned int i = 0u; i < 6u; ++i)
        YVEX_TEST_ASSERT(gates[i] == 1.0f, "post gate is exactly multiplier times sigmoid zero");
    for (unsigned int i = 0u; i < 12u; ++i) {
        double error = fabs((double)matrix[i] - entry);
        if (error > maximum) maximum = error;
        YVEX_TEST_ASSERT(error <= 1e-7, "matrix equals analytic uniform Sinkhorn oracle");
    }
    for (unsigned int row = 0u; row < 3u; ++row) {
        outputs[0] = single + row * 3u;
        YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 1u, (const float *[]){input + row * 6u, mixes + row * 8u},
            2u, outputs, 3u, NULL, NULL, &facts, &err) == YVEX_OK, "single row executes");
    }
    YVEX_TEST_ASSERT(!memcmp(single, expected, sizeof(single)), "chunk and single collapse are exactly equivalent");
    outputs[0] = collapsed;
    collapsed[0] = gates[0] = matrix[0] = 123.0f;
    outputs[1] = collapsed + 1u;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){input, mixes}, 2u, outputs, 3u,
        NULL, NULL, &facts, &err) == YVEX_ERR_FORMAT && collapsed[0] == 123.0f &&
        gates[0] == 123.0f && matrix[0] == 123.0f, "overlapping result spans refuse before publication");
    outputs[1] = gates;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){input, mixes}, 2u, outputs, 3u,
        test_ingress_cancel, NULL, &facts, &err) == YVEX_ERR_CANCELLED &&
        collapsed[0] == 123.0f && gates[0] == 123.0f && matrix[0] == 123.0f,
        "cancellation publishes none of three results");
    input[0] = NAN;
    YVEX_TEST_ASSERT(yvex_program_stage_host(stage, 3u, (const float *[]){input, mixes}, 2u, outputs, 3u,
        NULL, NULL, &facts, &err) == YVEX_ERR_FORMAT && collapsed[0] == 123.0f &&
        gates[0] == 123.0f && matrix[0] == 123.0f, "numeric refusal publishes none of three results");
    input[0] = original[0];
    YVEX_TEST_ASSERT(yvex_program_stage_open(&refused, copy, parameters, 2u, backend, 3u, 1, 1u, 1u, &err) !=
        YVEX_OK && !refused, "insufficient preparation budget refuses without a live stage");
    if (test_weighted_rms(backend) != 0 || test_ingress_population(copy, backend, parameters) != 0 ||
        test_ingress_nonzero(copy, backend) != 0) return 1;
    if (test_ingress_composition(backend, YVEX_GGUF_QTYPE_F32) != 0 ||
        test_ingress_composition(backend, YVEX_GGUF_QTYPE_BF16) != 0) return 1;
    YVEX_TEST_ASSERT(yvex_program_stage_close(&stage, &err) == YVEX_OK &&
        yvex_backend_get_memory_stats(backend, &after, &err) == YVEX_OK &&
        before.allocated_bytes == after.allocated_bytes && yvex_backend_close_checked(&backend, &err) == YVEX_OK,
        "ingress cleanup restores initial allocation count");
    printf("mHC ingress %s: collapse=9 exact gates=6 exact matrix=12 max_abs=%.12g tolerance=1e-7; "
        "compiler_negatives=12 chunk=single input=unchanged cancellation=unpublished allocation_delta=0\n",
        kind == YVEX_BACKEND_KIND_CPU ? "CPU" : "CUDA", maximum);
    yvex_program_physical_close(&program); yvex_program_physical_close(&copy);
    free(bytes.data); free(again.data);
    return 0;
}
#endif
