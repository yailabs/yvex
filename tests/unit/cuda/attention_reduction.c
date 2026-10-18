/* Independent dense softmax oracle for native reduction, including register/streaming tails. */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include "src/backend/cuda/private.h"
#include "src/backend/cuda/attention_ops.h"
#include "src/backend/cuda/component_ops.h"
#include "tests/test.h"

enum { REDUCTION_WIDTH = 1280, REDUCTION_ROWS = 512, REDUCTION_HEADS = 3,
       REDUCTION_TOKENS = 3, REDUCTION_VALUES = REDUCTION_WIDTH * REDUCTION_HEADS * REDUCTION_TOKENS };

typedef struct {
    yvex_cuda_work *work;
    yvex_backend_attention_weight *weight;
    CUdeviceptr weights, input, output, status;
    unsigned rows, inputs;
} projection_graph_fixture;

static int projection_job_admission_check(void)
{
    float input[32] = {0};
    const yvex_cuda_attention_operations *ops = yvex_cuda_attention_operations_get();
    yvex_backend_attention_job job = {.schema = YVEX_BACKEND_ATTENTION_JOB_SCHEMA,
        .input = input, .input_stride = 32, .token_count = 1, .hidden_width = 32,
        .q_rank = 32, .query_heads = 1, .head_dimension = 32, .kv_width = 32,
        .max_device_bytes = 4096, .native_execution = 1,
        .phase = YVEX_BACKEND_ATTENTION_PHASE_DECODE,
        .operation_scope = YVEX_BACKEND_ATTENTION_SCOPE_CORE,
        .attention_class = YVEX_BACKEND_ATTENTION_SWA,
        .compute_contract = YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1};
    yvex_backend_attention_output output = {0};
    yvex_backend_attention_failure failure = {0};
    yvex_error err = {0};
    for (unsigned cls = YVEX_MATRIX_PROJECTION_BF16_F32_ACC_V1;
         cls <= YVEX_MATRIX_PROJECTION_PREPARED_BF16_F32_ACC_V1; ++cls) {
        for (unsigned negative = 0; negative < 5; ++negative) {
            yvex_backend_attention_job trial = job;
            trial.matrix_projection = negative == 4 ? cls + 2u : cls;
            trial.native_execution = negative != 1;
            trial.evidence_level = negative == 2 ? 3u : 0u;
            if (negative == 3) {
                trial.candidate_block_visible = 1;
                trial.phase = YVEX_BACKEND_ATTENTION_PHASE_SPECULATIVE_DRAFT;
                trial.token_count = 2;
            }
            int rc = ops->validate_job(&trial, &output, &failure, &err);
            YVEX_TEST_ASSERT(rc == (negative ? YVEX_ERR_UNSUPPORTED : YVEX_OK),
                "both matrix classes enter native target jobs; reference, draft and unknown classes refuse");
        }
    }
    puts("Matrix job admission: 2 native target classes accepted, 8 reference/draft/unknown refusals");
    return 0;
}

/* Full submission control, not just a projection: a library-backed phase must
 * still initialize inputs/history/status when a graph configuration is present. */
static int prepared_phase_submission_check(yvex_backend *backend)
{
    enum { WIDTH = 32, ROWS = 256, ELEMENTS = WIDTH * ROWS };
    float *input = calloc(ELEMENTS, sizeof(float)), *actual = calloc(ELEMENTS, sizeof(float));
    float *expected = calloc(ELEMENTS, sizeof(float));
    float *raw = calloc(ELEMENTS, sizeof(float));
    unsigned short identity[WIDTH * WIDTH] = {0};
    float norm[WIDTH], sink = 0.0f;
    yvex_error err = {0};
    yvex_backend_attention_failure failure = {0};
    yvex_backend_attention_job job = {.schema = YVEX_BACKEND_ATTENTION_JOB_SCHEMA,
        .phase = YVEX_BACKEND_ATTENTION_PHASE_PREFILL, .attention_class = YVEX_BACKEND_ATTENTION_SWA,
        .compute_contract = YVEX_BACKEND_ATTENTION_COMPUTE_BF16_F32_RNE_V1,
        .operation_scope = YVEX_BACKEND_ATTENTION_SCOPE_CORE, .native_execution = 1, .evidence_level = 1,
        .matrix_projection = YVEX_MATRIX_PROJECTION_PREPARED_BF16_F32_ACC_V1,
        .token_count = ROWS, .input_stride = WIDTH, .hidden_width = WIDTH, .q_rank = WIDTH,
        .query_heads = 1, .head_dimension = WIDTH, .kv_width = WIDTH,
        .sliding_window = 4, .output_groups = 1, .output_group_input_width = WIDTH, .output_rank = WIDTH,
        .rms_epsilon = 1e-6, .position = {10000, 1, 0, 32, 1, 2},
        .local_stride = WIDTH, .input = input,
        .max_host_bytes = 4ull * 1024ull * 1024ull, .max_device_bytes = 32ull * 1024ull * 1024ull};
    YVEX_TEST_ASSERT(input && actual && expected && raw, "bounded full-phase submission fixture");
    YVEX_TEST_ASSERT(yvex_backend_host_workspace_prepare_owned(backend, job.max_host_bytes, &err) == YVEX_OK,
        "full phase owns bounded host staging");
    for (unsigned i = 0; i < WIDTH; ++i) { identity[i * WIDTH + i] = 0x3f80; norm[i] = 1.0f; }
    yvex_device_tensor *resident = NULL, *workspace = NULL;
    unsigned char *weights = NULL;
    yvex_backend_tensor_desc d = {.name = "prepared-phase-weights", .dtype = YVEX_DTYPE_I8,
        .rank = 1, .dims = {sizeof(identity) + sizeof(norm) + sizeof(sink)},
        .bytes = sizeof(identity) + sizeof(norm) + sizeof(sink)};
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &d, &resident, &weights, &err) == YVEX_OK,
        "full phase owns fixed weight backing");
    memcpy(weights, identity, sizeof(identity));
    memcpy(weights + sizeof(identity), norm, sizeof(norm));
    memcpy(weights + sizeof(identity) + sizeof(norm), &sink, sizeof(sink));
    YVEX_TEST_ASSERT(yvex_backend_resident_attach(backend, weights, d.bytes, resident, 1, &err) == YVEX_OK,
        "full phase binds admitted weight residency");
    d.dims[0] = d.bytes = job.max_device_bytes;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &workspace, &err) == YVEX_OK &&
        yvex_backend_workspace_attach(backend, workspace, 1, &err) == YVEX_OK, "full phase owns device arena");
    for (unsigned i = 0; i <= YVEX_BACKEND_ATTENTION_WEIGHT_OUT_B; ++i) {
        int scalar = i == YVEX_BACKEND_ATTENTION_WEIGHT_SINKS;
        int scale = i == YVEX_BACKEND_ATTENTION_WEIGHT_Q_A_NORM || i == YVEX_BACKEND_ATTENTION_WEIGHT_KV_NORM;
        job.weights[i] = (yvex_backend_attention_weight){
            .encoded = weights + (scalar ? sizeof(identity) + sizeof(norm) : scale ? sizeof(identity) : 0),
            .encoded_bytes = scalar ? sizeof(sink) : scale ? sizeof(norm) : sizeof(identity),
            .row_width = scalar ? 1u : WIDTH, .row_count = scalar || scale ? 1u : WIDTH,
            .row_bytes = scalar ? sizeof(sink) : scale ? sizeof(norm) : WIDTH * sizeof(unsigned short),
            .qtype = scalar || scale ? YVEX_GGUF_QTYPE_F32 : YVEX_GGUF_QTYPE_BF16, .present = 1};
    }
    for (unsigned sample = 0; sample < 5; ++sample) {
        for (unsigned i = 0; i < ELEMENTS; ++i) input[i] = (float)((int)((i + sample * 7u) % 31u) - 15) / 16.0f;
        if (sample == 3u) input[0] = NAN;
        for (unsigned arm = 0; arm < 2; ++arm) {
            yvex_backend_cuda_attention_mode mode = arm ? YVEX_BACKEND_CUDA_ATTENTION_FULL : YVEX_BACKEND_CUDA_ATTENTION_EAGER;
            YVEX_TEST_ASSERT(yvex_backend_cuda_attention_configure(backend, job.phase, mode,
                "prepared-phase-submission", "prefill-256", 3, 0, 0, &err) == YVEX_OK,
                "configure explicit eager or graph-selected prepared phase");
            for (unsigned i = 0; i < ELEMENTS; ++i) actual[i] = raw[i] = -12345.0f;
            yvex_backend_attention_output output = {.output = {actual, ELEMENTS}, .raw_kv = {raw, ELEMENTS}};
            int rc = yvex_backend_attention_execute(backend, &job, &output, &failure, &err);
            if (sample == 3u) {
                YVEX_TEST_ASSERT(rc == YVEX_ERR_FORMAT, "nonfinite phase refuses before publication");
                for (unsigned i = 0; i < ELEMENTS; ++i)
                    YVEX_TEST_ASSERT(actual[i] == -12345.0f && raw[i] == -12345.0f,
                        "failed phase preserves every caller result and state span");
                continue;
            }
            if (rc != YVEX_OK) fprintf(stderr, "prepared phase arm=%u sample=%u rc=%d stage=%s actual=%llu reason=%s\n",
                arm, sample, rc, failure.stage ? failure.stage : "", failure.actual, err.message);
            YVEX_TEST_ASSERT(rc == YVEX_OK && output.tokens_executed == ROWS,
                "prepared full phase initializes all inputs, history and status before execution");
            for (unsigned i = 0; i < ELEMENTS; ++i)
                YVEX_TEST_ASSERT(isfinite(actual[i]) && actual[i] != -12345.0f, "complete finite phase output");
            if (!arm) memcpy(expected, actual, ELEMENTS * sizeof(float));
            else YVEX_TEST_ASSERT(!memcmp(expected, actual, ELEMENTS * sizeof(float)),
                "graph-selected fallback and explicit eager preserve identical phase results");
        }
    }
    unsigned long long released = 0;
    YVEX_TEST_ASSERT(yvex_backend_cuda_attention_graph_registry_apply(backend,
        YVEX_BACKEND_CUDA_GRAPH_REGISTRY_RELEASE, &released, &err) == YVEX_OK, "retire phase submission graphs");
    YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "retire phase weight backing");
    yvex_backend_workspace_detach(backend);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &workspace, &err) == YVEX_OK &&
        yvex_backend_host_workspace_detach(backend, &err) == YVEX_OK, "retire phase execution arenas");
    free(input); free(actual); free(expected); free(raw);
    puts("Prepared phase submission: 4 changed-input eager/full pairs, 8192 exact values each; 2 unpublished refusals and recovery");
    return 0;
}

static int projection_graph_enqueue(void *context, int enqueue, yvex_error *err)
{
    projection_graph_fixture *f = context;
    f->work->prepare_only = !enqueue;
    int rc = yvex_cuda_bf16_matrix(f->work, f->weight, f->weights, 1, f->rows,
        f->inputs, f->input, f->output, 0, f->status, err);
    f->work->prepare_only = 0;
    return rc;
}

static int bf16_projection_check(yvex_backend *backend, unsigned inputs, int publication, int negative)
{
    enum { WIDTH = 4096, ROWS = 17, CAPACITY = YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH };
    unsigned short weights[(ROWS + 1) * WIDTH];
    YVEX_TEST_ASSERT(inputs && inputs <= CAPACITY, "bounded matrix fixture");
    float *input = malloc((size_t)inputs * WIDTH * sizeof(float));
    float *observed = malloc(((size_t)inputs * ROWS + 2u) * sizeof(float));
    YVEX_TEST_ASSERT(input && observed, "bounded matrix fixtures do not consume the thread stack");
    yvex_error err = {0};
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {.backend = backend, .state = state, .raw_only = 1,
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, .matrix_projection = 1};
    CUdeviceptr w = 0, x = 0, y = 0, status = 0;
    unsigned seed = 293u;
    for (unsigned i = 0; i < (ROWS + 1) * WIDTH; ++i) {
        seed = seed * 1664525u + 1013904223u;
        weights[i] = (unsigned short)((seed & 0x807fu) | ((122u + (seed >> 16u) % 7u) << 7u));
    }
    for (unsigned i = 0; i < inputs * WIDTH; ++i) {
        seed = seed * 1664525u + 1013904223u;
        unsigned bits = ((seed & 0x807fu) | ((122u + (seed >> 16u) % 7u) << 7u)) << 16u;
        memcpy(input + i, &bits, sizeof(bits));
    }
    if (negative == 1) input[0] = 1.1f;
    if (negative == 2) input[0] = NAN;
    if (negative == 3) weights[WIDTH] = 1u;
    if (negative == 5) input[0] = 0x1p-57f;
    if (negative == 6) weights[WIDTH] = 183u << 7u;
    if (negative == 7) input[0] = INFINITY;
    if (negative == 8) weights[WIDTH] = 255u << 7u;
    for (unsigned i = 0; i < inputs * ROWS + 2u; ++i) observed[i] = -12345.0f;
    YVEX_TEST_ASSERT(yvex_cuda_work_allocate(&work, &w, sizeof(weights), weights, 0, "matrix.weight", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &x, inputs * WIDTH * sizeof(float), input, 0, "matrix.input", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &y, (inputs * ROWS + 2u) * sizeof(float), observed, 0, "matrix.output", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &status, sizeof(int), NULL, 1, "matrix.status", NULL, &err) == YVEX_OK,
        "matrix experiment owns bounded source/output/status memory");
    yvex_backend_attention_weight weight = {.present = 1, .qtype = YVEX_GGUF_QTYPE_BF16,
        .row_width = WIDTH, .row_count = ROWS + 1u, .row_bytes = WIDTH * sizeof(unsigned short)};
    int code = 0;
    if (negative == 4) {
        code = 7;
        YVEX_TEST_ASSERT(state->driver.cuMemcpyHtoD_v2(status, &code, sizeof(code)) == 0, "prior failure witness");
    }
    YVEX_TEST_ASSERT(yvex_cuda_bf16_matrix(&work, &weight, w, 1, ROWS, inputs, x, y,
        publication, status, &err) == YVEX_OK &&
        state->driver.cuCtxSynchronize() == 0 &&
        state->driver.cuMemcpyDtoH_v2(&code, status, sizeof(code)) == 0 &&
        state->driver.cuMemcpyDtoH_v2(observed, y, (inputs * ROWS + 2u) * sizeof(float)) == 0,
        "selected matrix completes before observation");
    if ((inputs == 3u || inputs == 33u) && !publication && !negative) {
        CUdeviceptr changed = 0;
        for (unsigned i = 0; i < inputs * WIDTH; ++i) input[i] = -input[i];
        YVEX_TEST_ASSERT(yvex_cuda_work_allocate(&work, &changed, inputs * WIDTH * sizeof(float),
            input, 0, "matrix.changed", NULL, &err) == YVEX_OK, "own replay's different input address");
        projection_graph_fixture fixture = {&work, &weight, w, x, y, status, ROWS, inputs};
        yvex_backend_cuda_graph_info info;
        for (unsigned sample = 0; sample < 2u; ++sample) {
            fixture.input = sample ? changed : x;
            YVEX_TEST_ASSERT(yvex_cuda_graph_execute(backend, "test.bf16-matrix.replay", NULL,
                projection_graph_enqueue, &fixture, 0u, &info, &err) == YVEX_OK && info.capture_count == 1ull,
                "capture then rebind the selected physical matrix without recapture");
        }
        YVEX_TEST_ASSERT(state->driver.cuMemcpyDtoH_v2(&code, status, sizeof(code)) == 0 &&
            state->driver.cuMemcpyDtoH_v2(observed, y, (inputs * ROWS + 2u) * sizeof(float)) == 0,
            "observe rebound operands after graph completion");
        YVEX_TEST_ASSERT(yvex_cuda_graphs_close_all(backend, &err) == YVEX_OK,
            "release graph before its matrix operand lifetime ends");
    }
    YVEX_TEST_ASSERT(observed[inputs * ROWS] == -12345.0f && observed[inputs * ROWS + 1u] == -12345.0f,
        "partial matrix tiles preserve output canaries");
    if (negative) {
        YVEX_TEST_ASSERT(code != 0, "unsupported operand precision and prior failures refuse publication");
        if (negative == 4) for (unsigned i = 0; i < inputs * ROWS; ++i)
            YVEX_TEST_ASSERT(observed[i] == -12345.0f, "prior failure performs no stores");
    } else {
        YVEX_TEST_ASSERT(!code, "finite source-BF16 operands execute");
        const double gamma = (WIDTH * 0x1p-24) / (1.0 - WIDTH * 0x1p-24);
        for (unsigned c = 0; c < inputs; ++c) for (unsigned r = 0; r < ROWS; ++r) {
            double reference = 0.0, norm = 0.0;
            for (unsigned k = 0; k < WIDTH; ++k) {
                unsigned bits = (unsigned)weights[(r + 1u) * WIDTH + k] << 16u;
                float value; memcpy(&value, &bits, sizeof(value));
                double product = (double)value * (double)input[c * WIDTH + k];
                reference += product; norm += fabs(product);
            }
            double bound = gamma * norm + WIDTH * 0x1p-149;
            if (publication) bound += (fabs(reference) + bound) / 256.0 + 0x1p-133;
            YVEX_TEST_ASSERT(isfinite(observed[c * ROWS + r]) &&
                fabs((double)observed[c * ROWS + r] - reference) <= bound,
                "independently decoded F64 reference bounds explicit F32 contraction and publication");
        }
    }
    work.matrix_projection = 0;
    YVEX_TEST_ASSERT(yvex_cuda_bf16_matrix(&work, &weight, w, 0, ROWS, inputs, x, y,
        publication, status, &err) == YVEX_ERR_UNSUPPORTED,
        "legacy numerical class cannot silently select BF16/F32 matrix");
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK, "matrix resources released");
    free(observed); free(input);
    return 0;
}
static int prepared_projection_check(yvex_backend *backend, unsigned qtype,
    unsigned width, unsigned rows, int publication, int negative, unsigned INPUTS)
{
    YVEX_TEST_ASSERT(INPUTS >= 256u && INPUTS <= YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH,
        "prepared experiment preserves the actual admitted input population");
    const unsigned WIDTH = width, ROWS = rows;
    const size_t row_bytes = qtype == YVEX_GGUF_QTYPE_BF16 ? WIDTH * 2u : WIDTH / 32u * 17u;
    const size_t weight_bytes = (ROWS + 1ull) * row_bytes;
    const size_t input_bytes = INPUTS * (size_t)WIDTH * sizeof(float);
    const size_t output_bytes = (INPUTS * (size_t)ROWS + 2u) * sizeof(float);
    unsigned char *weights = malloc((ROWS + 1ull) * WIDTH * 2u);
    float *input = malloc(input_bytes), *observed = malloc(output_bytes);
    YVEX_TEST_ASSERT(weights && input && observed, "bounded fixture storage is allocated off the stack");
    yvex_error err = {0};
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_cuda_work work = {.backend = backend, .state = state, .raw_only = 1,
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, .matrix_projection = 2};
    CUdeviceptr w = 0, x = 0, y = 0, status = 0;
    unsigned seed = 829u;
    for (unsigned i = 0; i < (ROWS + 1u) * WIDTH; ++i) {
        seed = seed * 1664525u + 1013904223u;
        unsigned short value = (unsigned short)((seed & 0x807fu) | (125u << 7u));
        memcpy(weights + i * 2u, &value, 2u);
    }
    if (qtype == YVEX_GGUF_QTYPE_MXFP4)
        for (unsigned i = 0; i < (ROWS + 1u) * WIDTH / 32u; ++i) {
            weights[i * 17u] = 123u + i % 4u;
            for (unsigned j = 1; j < 17u; ++j) weights[i * 17u + j] = (unsigned char)(i * 13u + j * 27u);
        }
    for (unsigned i = 0; i < INPUTS * WIDTH; ++i) input[i] = (float)((int)(i * 31u % 251u) - 125) / 128.0f;
    if (negative == 1) input[0] = 1.1f;
    if (negative == 2) input[WIDTH] = NAN;
    if (negative == 3) input[0] = 0x1p-57f;
    if (negative == 4) input[0] = INFINITY;
    for (unsigned i = 0; i < INPUTS * ROWS + 2u; ++i) observed[i] = -12345.0f;
    YVEX_TEST_ASSERT(yvex_cuda_work_allocate(&work, &w, weight_bytes, weights, 0, "prepared.weight", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &x, input_bytes, input, 0, "prepared.input", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &y, output_bytes, observed, 0, "prepared.output", NULL, &err) == YVEX_OK &&
        yvex_cuda_work_allocate(&work, &status, 4u, NULL, 1, "prepared.status", NULL, &err) == YVEX_OK,
        "bounded preparation fixture owns all inputs and guarded output");
    yvex_backend_attention_weight weight = {.present = 1, .qtype = qtype,
        .row_width = WIDTH, .row_count = ROWS + 1u, .row_bytes = row_bytes};
    YVEX_TEST_ASSERT(yvex_cuda_prepared_matrix(&work, &weight, w, 1, ROWS, INPUTS, x, y,
        publication, status, &err) == YVEX_OK && state->driver.cuCtxSynchronize() == 0,
        "multiple replacement tiles consume the real input once");
    int code = 0;
    YVEX_TEST_ASSERT(state->driver.cuMemcpyDtoH_v2(&code, status, sizeof(code)) == 0 &&
        state->driver.cuMemcpyDtoH_v2(observed, y, output_bytes) == 0,
        "completion precedes numerical observation");
    YVEX_TEST_ASSERT(observed[INPUTS * ROWS] == -12345.0f && observed[INPUTS * ROWS + 1u] == -12345.0f &&
        work.decoded_capacity == yvex_cuda_decoded_workspace(INPUTS),
        "partial tiles preserve canaries and stay in the canonical decoded budget");
    YVEX_TEST_ASSERT(work.tensor_core_launches > 0ull &&
        work.tensor_core_launches <= (ROWS + 511ull) / 512ull,
        "arena-derived preparation never adds contractions relative to the fixed-512 control");
    if (WIDTH == 4096u && ROWS == 2203u && INPUTS == 256u)
        YVEX_TEST_ASSERT(work.tensor_core_launches == 2ull,
            "bounded 1728-row replacement and exact 475-row tail use two contractions, not five");
    if (negative) YVEX_TEST_ASSERT(code != 0, "invalid precision/nonfinite/exponent refuses the transaction");
    else {
        YVEX_TEST_ASSERT(code == 0, "valid prepared operands complete");
        const double gamma = (WIDTH * 0x1p-24) / (1.0 - WIDTH * 0x1p-24);
        const int levels[] = {0, 1, 2, 3, 4, 6, 8, 12};
        for (unsigned z = 0; z < 1024u; ++z) {
            unsigned c = z % INPUTS, r = z < ROWS ? z : z * 167u % ROWS;
            double reference = 0.0, norm = 0.0;
            const unsigned char *row = weights + (r + 1u) * row_bytes;
            for (unsigned k = 0; k < WIDTH; ++k) {
                float value;
                if (qtype == YVEX_GGUF_QTYPE_BF16) {
                    unsigned short encoded; memcpy(&encoded, row + k * 2u, 2u);
                    unsigned bits = (unsigned)encoded << 16u; memcpy(&value, &bits, 4u);
                } else {
                    const unsigned char *block = row + k / 32u * 17u;
                    unsigned code4 = (block[1u + k % 16u] >> (k % 32u / 16u * 4u)) & 15u;
                    value = ldexpf((float)((code4 & 8u) ? -levels[code4 & 7u] : levels[code4 & 7u]), (int)block[0] - 128);
                }
                double product = (double)value * input[c * WIDTH + k];
                reference += product; norm += fabs(product);
            }
            double bound = gamma * norm + WIDTH * 0x1p-149;
            if (publication) bound += (fabs(reference) + bound) / 256.0 + 0x1p-133;
            YVEX_TEST_ASSERT(isfinite(observed[c * ROWS + r]) &&
                fabs(observed[c * ROWS + r] - reference) <= bound,
                "independent codec/F64 contraction bounds replacement tiles and publication");
        }
    }
    work.prepare_only = 1;
    YVEX_TEST_ASSERT(yvex_cuda_prepared_matrix(&work, &weight, w, 1, ROWS, INPUTS, x, y,
        publication, status, &err) == YVEX_ERR_UNSUPPORTED, "opaque library operation cannot use owned-node replay");
    work.prepare_only = 0; work.matrix_projection = 1;
    YVEX_TEST_ASSERT(yvex_cuda_prepared_matrix(&work, &weight, w, 1, ROWS, INPUTS, x, y,
        publication, status, &err) == YVEX_ERR_UNSUPPORTED, "older numerical class cannot inherit prepared execution");
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK, "release every prepared resource");
    free(weights); free(input); free(observed);
    return 0;
}
typedef struct {
    float query[REDUCTION_VALUES];
    float local[REDUCTION_ROWS * REDUCTION_WIDTH];
    float compressed[REDUCTION_ROWS * REDUCTION_WIDTH];
    float sinks[REDUCTION_HEADS];
    unsigned long long positions[REDUCTION_ROWS], compressed_positions[REDUCTION_ROWS];
    unsigned long long selected[REDUCTION_TOKENS * REDUCTION_ROWS], counts[REDUCTION_TOKENS];
    float before_canary, output[REDUCTION_VALUES], after_canary;
    int status;
} reduction_storage;

typedef struct {
    unsigned long long width, heads, tokens, initial, stride, topk, window, ratio, start;
    unsigned int attention_class, scenario;
    int block_visible;
} reduction_case;

static float reduction_bf16(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    bits += 0x7fffu + ((bits >> 16u) & 1u);
    bits &= 0xffff0000u;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/* Full materialized softmax in scalar F64, not the device's online recurrence
 * or its lane reduction. Input/source rows are selected independently here. */
static double reduction_oracle(const reduction_storage *s, const reduction_case *c,
                               unsigned long long token, unsigned long long head,
                               double expected[REDUCTION_WIDTH])
{
    const float *rows[REDUCTION_ROWS * 2];
    double scores[REDUCTION_ROWS * 2], maximum = s->sinks[head], denominator;
    unsigned long long count = 0ull, position = c->start + token;
    unsigned long long first = position + 1ull > c->window ? position + 1ull - c->window : 0ull;
    for (unsigned long long i = 0ull; i < c->initial + c->tokens; ++i) {
        if (!c->block_visible && (s->positions[i] < first || s->positions[i] > position)) continue;
        rows[count++] = s->local + i * REDUCTION_WIDTH;
    }
    if (c->attention_class) {
        unsigned long long n = c->attention_class == 1u ? s->counts[token]
            : position / c->ratio + ((position + 1ull) % c->ratio == 0ull);
        for (unsigned long long j = 0ull; j < n; ++j) {
            unsigned long long i = c->attention_class == 1u ? s->selected[token * REDUCTION_ROWS + j] : j;
            if (s->compressed_positions[i] + c->ratio - 1ull > position) continue;
            rows[count++] = s->compressed + i * REDUCTION_WIDTH;
        }
    }
    for (unsigned long long row = 0ull; row < count; ++row) {
        double dot = 0.0;
        for (unsigned long long i = 0ull; i < c->width; ++i)
            dot += (double)s->query[(token * c->heads + head) * c->width + i] * rows[row][i];
        scores[row] = dot / sqrt((double)c->width);
        if (scores[row] > maximum) maximum = scores[row];
    }
    denominator = exp((double)s->sinks[head] - maximum);
    memset(expected, 0, REDUCTION_WIDTH * sizeof(*expected));
    for (unsigned long long row = 0ull; row < count; ++row) {
        double weight = exp(scores[row] - maximum);
        denominator += weight;
        for (unsigned long long i = 0ull; i < c->width; ++i) expected[i] += weight * rows[row][i];
    }
    for (unsigned long long i = 0ull; i < c->width; ++i) expected[i] /= denominator;
    return denominator;
}

static void reduction_inputs(reduction_storage *s, reduction_case *c)
{
    int analytic = c->scenario == 0u;
    c->heads = REDUCTION_HEADS; c->tokens = REDUCTION_TOKENS;
    c->initial = analytic ? 4ull : 128ull;
    c->stride = REDUCTION_WIDTH; c->topk = REDUCTION_ROWS;
    c->window = 128ull; c->start = 8192ull;
    c->ratio = c->attention_class == 1u ? 4ull : c->attention_class == 2u ? 128ull : 0ull;
    c->block_visible = c->scenario != 2u;
    s->before_canary = s->after_canary = 12345.0f;
    for (unsigned long long i = 0ull; i < REDUCTION_VALUES; ++i) {
        s->query[i] = analytic ? 0.0f : (float)((int)((i * 19ull + 3ull) % 127ull) - 63) / 128.0f;
        s->output[i] = 12345.0f;
    }
    for (unsigned long long row = 0ull; row < REDUCTION_ROWS; ++row) {
        s->positions[row] = c->start - c->initial + row;
        s->compressed_positions[row] = row * c->ratio;
        for (unsigned long long i = 0ull; i < REDUCTION_WIDTH; ++i) {
            float value = (float)((int)(i % 15ull) - 7) / 8.0f;
            s->local[row * REDUCTION_WIDTH + i] = analytic ? value
                : (float)((int)((row * 17ull + i * 7ull) % 251ull) - 125) / 64.0f;
            s->compressed[row * REDUCTION_WIDTH + i] = analytic ? value
                : (float)((int)((row * 31ull + i * 5ull) % 241ull) - 120) / 64.0f;
        }
    }
    for (unsigned long long token = 0ull; token < c->tokens; ++token) {
        s->counts[token] = analytic ? 8ull : REDUCTION_ROWS;
        for (unsigned long long i = 0ull; i < REDUCTION_ROWS; ++i)
            s->selected[token * REDUCTION_ROWS + i] = s->counts[token] > i ? s->counts[token] - 1ull - i : i;
    }
    for (unsigned long long head = 0ull; head < c->heads; ++head)
        s->sinks[head] = analytic ? 0.0f : (float)((int)head - 1) / 4.0f;
    if (c->scenario == 3u) s->query[0] = NAN;
    if (c->scenario == 4u) s->status = 1;
    if (c->scenario == 5u) c->stride = c->width - 1ull;
    if (c->scenario == 6u) c->ratio = 7ull;
    if (c->scenario == 7u) s->sinks[0] = NAN;
}

static int reduction_check(yvex_backend *backend, unsigned long long width,
                           unsigned int attention_class, unsigned int scenario, int exact)
{
    reduction_storage *host = calloc(1u, sizeof(*host)), *observed = calloc(1u, sizeof(*observed));
    reduction_case c = {.width = width, .attention_class = attention_class, .scenario = scenario};
    yvex_backend_tensor_desc descriptor = {0};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err;
    CUdeviceptr base, query, local, positions, compressed, compressed_positions, selected, counts, sinks, output, status;
    double maximum_error = 0.0, worst_ratio = 0.0;
    int rc, device_wide = 0;
    YVEX_TEST_ASSERT(host && observed, "allocate native reduction oracle");
    reduction_inputs(host, &c);
    descriptor.name = "attention-reduction"; descriptor.dtype = YVEX_DTYPE_I8;
    descriptor.rank = 1u; descriptor.dims[0] = sizeof(*host); descriptor.bytes = sizeof(*host);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &descriptor, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK, "upload reduction fixture");
    base = yvex_cuda_activation_pointer(backend, arena);
    query = base + offsetof(reduction_storage, query); local = base + offsetof(reduction_storage, local);
    positions = base + offsetof(reduction_storage, positions); compressed = base + offsetof(reduction_storage, compressed);
    compressed_positions = base + offsetof(reduction_storage, compressed_positions);
    selected = base + offsetof(reduction_storage, selected); counts = base + offsetof(reduction_storage, counts);
    sinks = base + offsetof(reduction_storage, sinks); output = base + offsetof(reduction_storage, output);
    status = base + offsetof(reduction_storage, status);
    void *params[] = {&query, &local, &positions, &c.initial, &c.stride, &compressed, &compressed_positions,
        &c.stride, &selected, &counts, &c.topk, &sinks, &c.heads, &c.width, &c.window, &c.ratio,
        &c.attention_class, &c.start, &c.tokens, &c.block_visible, &output, &status};
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        exact == 3 ? state->attention_reduce_native_candidates_function :
        exact == 1 ? state->attention_reduce_function : exact == 2 ?
            (width == 512ull ? state->attention_reduce_native_warp_512_function :
             state->attention_reduce_native_warp_function) : state->attention_reduce_native_function,
        (unsigned int)(exact == 2 ? (c.heads * c.tokens + 7ull) / 8ull : c.heads * c.tokens),
        256u, exact == 1 ? 256u * sizeof(double) : 0u,
        params, "cuda.test.attention-reduction", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, "cuda.test.attention-reduction", &err);
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(
        backend, arena, observed, sizeof(*observed), &err) == YVEX_OK, "read native reduction");
    if (scenario >= 3u) {
        YVEX_TEST_ASSERT(observed->status != 0, "nonfinite/prior error/invalid geometry refuses publication");
    } else {
        YVEX_TEST_ASSERT(observed->status == 0, "valid reduction succeeds");
        for (unsigned long long token = 0ull; token < c.tokens; ++token)
            for (unsigned long long head = 0ull; head < c.heads; ++head) {
                double expected[REDUCTION_WIDTH];
                YVEX_TEST_ASSERT(isfinite(reduction_oracle(host, &c, token, head, expected)), "finite softmax oracle");
                for (unsigned long long i = 0ull; i < width; ++i) {
                    float value = observed->output[(token * c.heads + head) * width + i];
                    double difference = fabs((double)value - expected[i]);
                    /* Half of a BF16 relative spacing, plus bounded F32 arithmetic error.
                     * Analytic equal scores additionally require exact BF16 identity. */
                    double tolerance = fabs(expected[i]) / 256.0 + 2e-5;
                    if (difference > maximum_error) maximum_error = difference;
                    if (difference / tolerance > worst_ratio) worst_ratio = difference / tolerance;
                    YVEX_TEST_ASSERT(isfinite(value) && difference <= tolerance, "native output agrees with independent dense F64 softmax");
                    if (scenario == 0u) YVEX_TEST_ASSERT(value == reduction_bf16((float)expected[i]), "analytic BF16 output is exact");
                }
            }
    }
    YVEX_TEST_ASSERT(!memcmp(host, observed, offsetof(reduction_storage, before_canary)),
        "reduction does not modify query, sinks, state, positions or selected candidates");
    YVEX_TEST_ASSERT(observed->before_canary == 12345.0f && observed->after_canary == 12345.0f,
        "output allocation canaries retained");
    for (unsigned long long i = c.heads * c.tokens * width; i < REDUCTION_VALUES; ++i)
        YVEX_TEST_ASSERT(observed->output[i] == 12345.0f, "output stays within admitted shape");
    printf("attention reduction: mode=%s class=%u width=%llu scenario=%u values=%llu status=%d max_abs=%.12g "
           "worst_error_over_tolerance=%.9g analytic_exact=%s\n",
           exact == 3 ? "native-candidates" : exact == 1 ? "forensic" : exact == 2 ? "native-warp" : "native",
           attention_class, width, scenario,
           c.heads * c.tokens * width, observed->status, maximum_error, worst_ratio, scenario ? "n/a" : "true");
    if (exact >= 2) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK &&
            yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                state->attention_reduce_native_function, (unsigned int)(c.heads * c.tokens),
                256u, 0u, params, "cuda.test.attention-reference", &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                &device_wide, "cuda.test.attention-reference", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, host, sizeof(*host), &err) == YVEX_OK,
            "original block reduction remains an independently executed exact-tree reference");
        YVEX_TEST_ASSERT(host->status == observed->status &&
            (scenario >= 3u || !memcmp(host->output, observed->output, sizeof(host->output))),
            "candidate/warp permutation preserves original reduction bits and refusal status");
    }
    if (exact >= 2 && width == 512ull && scenario == 0u) {
        c.width = 511ull;
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, host, sizeof(*host), &err) == YVEX_OK &&
            yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                exact == 3 ? state->attention_reduce_native_candidates_function :
                    state->attention_reduce_native_warp_512_function,
                (unsigned)(exact == 3 ? c.heads * c.tokens : (c.heads * c.tokens + 7ull) / 8ull),
                256u, 0u, params,
                "cuda.test.attention-width-refusal", &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                &device_wide, "cuda.test.attention-width-refusal", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, observed, sizeof(*observed), &err) == YVEX_OK &&
            observed->status == 2 && !memcmp(host->output, observed->output, sizeof(host->output)),
            "fixed geometry rejects a mismatched width without publishing output");
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release reduction fixture");
    free(host); free(observed);
    return 0;
}

/* Independent ordered host transition: compare complete prefix states as well
 * as compressed values. This is a component oracle, not upstream conformance. */
static void rolling_oracle(float *kv, float *score, const float *input, const float *gates,
    const float *ape, float *output, unsigned long long ratio, unsigned long long head,
    unsigned long long rows, unsigned long long cursor, int overlap, int checkpoints)
{
    unsigned long long factor = overlap ? 2ull : 1ull, width = head * factor;
    unsigned long long extent = width * ratio * factor, emitted = 0ull;
    for (unsigned long long t = 0ull; t < rows; ++t) {
        unsigned long long c = (cursor + t) % ratio;
        unsigned long long before = checkpoints ? t * extent : 0ull;
        unsigned long long after = checkpoints ? (t + 1ull) * extent : 0ull;
        if (checkpoints) {
            memcpy(kv + after, kv + before, (size_t)extent * sizeof(float));
            memcpy(score + after, score + before, (size_t)extent * sizeof(float));
        }
        for (unsigned long long lane = 0ull; lane < width; ++lane) {
            unsigned long long i = after + (overlap ? ratio + c : c) * width + lane;
            kv[i] = input[t * width + lane];
            score[i] = gates[t * width + lane] + ape[c * width + lane];
        }
        if (c + 1ull != ratio) continue;
        for (unsigned long long lane = 0ull; lane < head; ++lane) {
            double maximum = -INFINITY, denominator = 0.0, sum = 0.0;
            for (unsigned long long slot = 0ull; slot < ratio; ++slot)
                for (unsigned long long side = 0ull; side < factor; ++side) {
                    unsigned long long i = after + (slot + side * ratio) * width + lane + side * head;
                    if (score[i] > maximum) maximum = score[i];
                }
            for (unsigned long long slot = 0ull; slot < ratio; ++slot)
                for (unsigned long long side = 0ull; side < factor; ++side) {
                    unsigned long long i = after + (slot + side * ratio) * width + lane + side * head;
                    double weight = exp((double)score[i] - maximum);
                    volatile double product = weight * (double)kv[i];
                    denominator += weight;
                    sum += product;
                }
            output[emitted * head + lane] = reduction_bf16((float)(sum / denominator));
        }
        ++emitted;
        if (overlap) {
            memmove(kv + after, kv + after + ratio * width, (size_t)(ratio * width) * sizeof(float));
            memmove(score + after, score + after + ratio * width, (size_t)(ratio * width) * sizeof(float));
        }
    }
}

static int rolling_rows_check(yvex_backend *backend, unsigned long long ratio,
    unsigned long long head, unsigned long long rows, int checkpoints, unsigned int negative)
{
    unsigned long long cursor = ratio - 1ull, factor = ratio == 4ull ? 2ull : 1ull;
    unsigned long long width = head * factor, extent = width * ratio * factor;
    unsigned long long state_count = extent * (checkpoints ? rows + 1ull : 1ull);
    unsigned long long offsets[] = {0ull, state_count, 2ull * state_count,
        2ull * state_count + rows * width, 2ull * state_count + 2ull * rows * width,
        2ull * state_count + 2ull * rows * width + ratio * width};
    unsigned long long outputs = ((cursor + rows) / ratio + 1ull) * head;
    unsigned long long status_offset = offsets[5] + outputs, count = status_offset + 1ull;
    unsigned long long ape_bytes = width * sizeof(float), submitted_rows = negative == 2u ? 0ull : rows;
    float *host = calloc((size_t)count, sizeof(float)), *actual = calloc((size_t)count, sizeof(float));
    yvex_device_tensor *arena = NULL;
    yvex_backend_tensor_desc desc = {.name = "rolling_rows", .dtype = YVEX_DTYPE_F32,
        .rank = 1u, .dims = {count}, .bytes = count * sizeof(float)};
    yvex_error err;
    CUdeviceptr pointers[7], base;
    unsigned int qtype = YVEX_GGUF_QTYPE_F32;
    int overlap = ratio == 4ull, device_wide = 0, status = negative == 3u ? 17 : 0;
    YVEX_TEST_ASSERT(host && actual, "rolling component host allocation");
    for (unsigned long long i = 0ull; i < extent; ++i) {
        host[i] = (float)((int)(i * 131ull % 251ull) - 125) / 128.0f;
        host[offsets[1] + i] = (float)((int)(i * 17ull % 103ull) - 51) / 16.0f;
    }
    for (unsigned long long i = 0ull; i < rows * width; ++i) {
        host[offsets[2] + i] = (float)((int)(i * 79ull % 251ull) - 125) / 64.0f;
        host[offsets[3] + i] = (float)((int)(i * 41ull % 127ull) - 63) / 32.0f;
    }
    for (unsigned long long i = 0ull; i < ratio * width; ++i)
        host[offsets[4] + i] = (float)((int)(i * 37ull % 97ull) - 48) / 64.0f;
    for (unsigned long long i = 0ull; i < outputs; ++i) host[offsets[5] + i] = 12345.0f;
    if (negative == 1u) host[offsets[2]] = NAN;
    memcpy(host + status_offset, &status, sizeof(status));
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, host, (size_t)desc.bytes, &err) == YVEX_OK,
        "rolling component device allocation");
    base = yvex_cuda_tensor_ptr(arena);
    for (unsigned int i = 0u; i < 6u; ++i) pointers[i] = base + offsets[i] * sizeof(float);
    pointers[6] = base + status_offset * sizeof(float);
    void *params[] = {&pointers[0], &pointers[1], &pointers[2], &pointers[3], &pointers[4],
        &ape_bytes, &qtype, &pointers[5], &ratio, &head, &submitted_rows, &cursor,
        &overlap, &checkpoints, &pointers[6]};
    YVEX_TEST_ASSERT(yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        yvex_cuda_state(backend)->attention_rolling_rows_function, (unsigned int)((head + 255ull) / 256ull),
        256u, 0u, params, "cuda.test.rolling_rows", &err) == YVEX_OK &&
        yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.rolling_rows", &err) == YVEX_OK &&
        yvex_backend_tensor_read(backend, arena, actual, (size_t)desc.bytes, &err) == YVEX_OK,
        "rolling phase executes and completes");
    memcpy(&status, actual + status_offset, sizeof(status));
    if (!negative) {
        YVEX_TEST_ASSERT(status == 0, "rolling phase numerical success");
        rolling_oracle(host, host + offsets[1], host + offsets[2], host + offsets[3],
            host + offsets[4], host + offsets[5], ratio, head, rows, cursor, overlap, checkpoints);
        YVEX_TEST_ASSERT(!memcmp(host, actual, (size_t)status_offset * sizeof(float)),
            "rolling emissions, all prefix states, immutable inputs and canaries match host oracle");
    } else {
        YVEX_TEST_ASSERT(status == (negative == 1u ? 1 : negative == 2u ? 2 : 17),
            "nonfinite/malformed/prior-error rolling execution stays fail-closed");
        if (negative > 1u) YVEX_TEST_ASSERT(!memcmp(host, actual, (size_t)status_offset * sizeof(float)),
            "invalid geometry and prior failure do not mutate state/output");
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
                     "rolling component storage retires");
    free(host); free(actual);
    return 0;
}

enum { UNIT_ROWS = 513, UNIT_WIDTH = 1280 };
typedef struct {
    float before, values[UNIT_ROWS * UNIT_WIDTH], after;
    int status;
} unit_norm_fixture;

/* Retain the original CUDA tree as a byte control, and independently bound
 * the normalized real value by the BF16 rounding cell. No altered tolerance
 * or arithmetic class is admitted by this test. */
static int unit_norm_check(yvex_backend *backend, unsigned long long rows,
    unsigned long long width, unsigned int scenario)
{
    unit_norm_fixture *seed = calloc(1u, sizeof(*seed)), *actual = calloc(1u, sizeof(*actual));
    unit_norm_fixture *control = calloc(1u, sizeof(*control));
    yvex_backend_tensor_desc desc = {.name = "exact-unit-norm", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(*seed)}, .bytes = sizeof(*seed)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    yvex_backend_attention_failure failure = {0};
    yvex_error err = {0};
    double epsilon = 1e-6;
    int device_wide = 0;
    YVEX_TEST_ASSERT(seed && actual && control, "allocate unit-norm independent controls");
    seed->before = seed->after = 12345.0f;
    seed->status = scenario == 4u ? 17 : 0;
    for (unsigned long long i = 0ull; i < UNIT_ROWS * UNIT_WIDTH; ++i) {
        float value = (float)((int)(i * 17ull % 251ull) - 125) / 128.0f;
        if (scenario == 1u) value = ldexpf(value, (int)(i % 201ull) - 100);
        if (scenario == 2u) value = i & 1ull ? -0.0f : 0.0f;
        seed->values[i] = i < rows * width ? value : 12345.0f;
    }
    if (scenario == 3u) seed->values[(rows - 1ull) * width] = NAN;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate bounded unit-norm device control");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr values = base + offsetof(unit_norm_fixture, values);
    CUdeviceptr status = base + offsetof(unit_norm_fixture, status);
    for (unsigned int arm = 0u; arm < 2u; ++arm) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, seed, sizeof(*seed), &err) == YVEX_OK,
            "reset unit-norm input and prior status");
        if (arm) {
            YVEX_TEST_ASSERT(yvex_cuda_attention_operations_get()->unit_norm(&work, values, rows,
                width, epsilon, status, "cuda.test.unit-norm", &failure, &err) == YVEX_OK,
                "canonical unit-norm selects only admitted real vector geometry");
        } else {
            void *params[] = {&values, &rows, &width, &epsilon, &status};
            YVEX_TEST_ASSERT(yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                work.state->attention_unit_norm_function, (unsigned int)rows, 256u,
                256u * sizeof(double), params, "cuda.test.unit-norm.control", &err) == YVEX_OK,
                "execute unchanged 256-leaf CUDA control");
        }
        YVEX_TEST_ASSERT(yvex_cuda_launch_synchronize(backend, work.variant, &device_wide,
            "cuda.test.unit-norm", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, arm ? actual : control, sizeof(*actual), &err) == YVEX_OK,
            "read exact unit-norm realization");
    }
    YVEX_TEST_ASSERT(actual->status == (scenario == 3u ? 1 : seed->status),
        "nonfinite and prior status remain refused");
    YVEX_TEST_ASSERT(actual->before == seed->before && actual->after == seed->after &&
        !memcmp(actual->values + rows * width, seed->values + rows * width,
            (UNIT_ROWS * UNIT_WIDTH - rows * width) * sizeof(float)), "unit-norm tail and canaries remain intact");
    if (scenario != 3u) YVEX_TEST_ASSERT(!memcmp(actual, control, sizeof(*actual)),
        "cohort and fallback preserve every publication bit including signed zero and exponent spread");
    if (scenario == 4u) YVEX_TEST_ASSERT(!memcmp(actual, seed, sizeof(*actual)),
        "prior refusal never mutates vector storage");
    if (scenario < 3u) for (unsigned long long row = 0ull; row < rows; ++row) {
        double total = 0.0;
        for (unsigned long long i = 0ull; i < width; ++i) {
            double v = seed->values[row * width + i];
            total += v * v;
        }
        double denominator = sqrt(total / (double)width + epsilon);
        for (unsigned long long i = 0ull; i < width; ++i) {
            double expected = (double)seed->values[row * width + i] / denominator;
            double observed = actual->values[row * width + i];
            YVEX_TEST_ASSERT(isfinite(observed) && fabs(expected - observed) <=
                ldexp(fabs(expected), -8) + 0x1p-134 + 0x1p-149,
                "independent scalar F64 normalization lies within the BF16 rounding cell bound");
        }
    }
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK,
        "unit-norm work releases bounded scratch");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "retire unit-norm storage");
    free(seed); free(actual); free(control);
    return 0;
}

enum { EMISSION_ROWS = 128, EMISSION_WIDTH = 512 };
typedef struct {
    float before, values[EMISSION_ROWS * EMISSION_WIDTH], after, weights[EMISSION_WIDTH];
    int status;
} emission_fixture;

/* Independent emitted vectors may share a launch, not an arithmetic reduction.
 * Compare batch/serial publication bytes and a scalar F64 normalization/RoPE
 * oracle. This does not qualify full-model reference behavior. */
static int emission_rows_check(yvex_backend *backend, unsigned long long rows,
    unsigned long long ratio, int index, unsigned int negative)
{
    const unsigned long long width = EMISSION_WIDTH, rope_width = 128ull;
    unsigned long long activation_width = index ? width : width - rope_width;
    unsigned long long start = 256ull, step = negative == 1u ? 0ull : ratio;
    if (negative == 2u) start = ~0ull;
    emission_fixture *seed = calloc(1u, sizeof(*seed)), *actual = calloc(1u, sizeof(*actual));
    float *previous = malloc(sizeof(seed->values));
    yvex_backend_tensor_desc desc = {.name = "compressed-emission-rows", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(*seed)}, .bytes = sizeof(*seed)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    const yvex_cuda_attention_operations *ops = yvex_cuda_attention_operations_get();
    yvex_backend_attention_weight weight = {.row_width = width, .row_count = 1ull,
        .row_bytes = width * sizeof(float), .qtype = YVEX_GGUF_QTYPE_F32, .present = 1};
    yvex_backend_attention_position position = {.theta = 10000ull, .scaling_factor = 1ull,
        .rope_dimensions = rope_width};
    yvex_backend_attention_activation activation = {.required = 1, .block_width = 32ull,
        .quantization = index ? 1u : 2u, .hadamard = index};
    yvex_backend_attention_failure failure = {0};
    yvex_error err = {0};
    int device_wide = 0;
    const double epsilon = 1e-6;
    YVEX_TEST_ASSERT(seed && actual && previous, "allocate emission oracle");
    seed->before = seed->after = 12345.0f;
    seed->status = negative == 3u ? 17 : 0;
    for (unsigned long long i = 0ull; i < width; ++i)
        seed->weights[i] = 1.0f + (float)(i % 7ull) / 16.0f;
    for (unsigned long long i = 0ull; i < EMISSION_ROWS * width; ++i)
        seed->values[i] = i < rows * width ? (float)((int)(i * 17ull % 251ull) - 125) / 128.0f : 12345.0f;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate emission device storage");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr values = base + offsetof(emission_fixture, values);
    CUdeviceptr weights = base + offsetof(emission_fixture, weights);
    CUdeviceptr status = base + offsetof(emission_fixture, status);
    for (unsigned int serial = 0u; serial < 2u; ++serial) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, seed, sizeof(*seed), &err) == YVEX_OK,
            "initialize independent emission realization");
        if (negative == 1u || negative == 2u) {
            YVEX_TEST_ASSERT(ops->rope(&work, values, 1ull, rows, width, start, step, &position,
                0, status, "cuda.test.emissions.invalid", &failure, &err) == YVEX_ERR_BOUNDS &&
                yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK &&
                !memcmp(seed, actual, sizeof(*seed)), "zero/overflow position step refuses before mutation");
            break;
        }
        for (unsigned long long row = 0ull; row < (serial ? rows : 1ull); ++row)
            YVEX_TEST_ASSERT(ops->weighted_norm(&work, values + row * width * sizeof(float), width,
                serial ? 1ull : rows, &weight, weights, epsilon, status, "cuda.test.emissions.norm",
                &failure, &err) == YVEX_OK, "normalize independent emission population");
        YVEX_TEST_ASSERT(yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.emissions.norm", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK,
            "complete normalization oracle boundary");
        if (!negative) for (unsigned long long row = 0ull; row < rows; ++row) {
            double sum = 0.0;
            for (unsigned long long i = 0ull; i < width; ++i) {
                double value = seed->values[row * width + i];
                sum += value * value;
            }
            double inverse = 1.0 / sqrt(sum / (double)width + epsilon);
            for (unsigned long long i = 0ull; i < width; ++i)
                YVEX_TEST_ASSERT(actual->values[row * width + i] == reduction_bf16((float)(
                    (double)seed->values[row * width + i] * inverse * seed->weights[i])),
                    "emission normalization matches independent scalar F64/BF16 oracle exactly");
        }
        float normalized[EMISSION_WIDTH];
        for (unsigned long long row = 0ull; row < (serial ? rows : 1ull); ++row)
            YVEX_TEST_ASSERT(ops->rope(&work, values + row * width * sizeof(float), 1ull,
                serial ? 1ull : rows, width, start + row * ratio, step, &position, 0, status,
                "cuda.test.emissions.rope", &failure, &err) == YVEX_OK,
                "rotate the exact source-authored emission positions");
        if (!negative) memcpy(normalized, actual->values, sizeof(normalized));
        YVEX_TEST_ASSERT(yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.emissions.rope", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK,
            "complete positional oracle boundary");
        if (!negative) for (unsigned long long pair = 0ull; pair < rope_width / 2ull; ++pair) {
            double angle = (double)start * pow(10000.0, -2.0 * (double)pair / (double)rope_width);
            unsigned long long i = width - rope_width + pair * 2ull;
            double left = (double)normalized[i] * cos(angle) - (double)normalized[i + 1ull] * sin(angle);
            double right = (double)normalized[i] * sin(angle) + (double)normalized[i + 1ull] * cos(angle);
            YVEX_TEST_ASSERT(fabs((double)actual->values[i] - left) <= fabs(left) / 256.0 + 2e-5 &&
                fabs((double)actual->values[i + 1ull] - right) <= fabs(right) / 256.0 + 2e-5,
                "emission RoPE agrees with independent trigonometric F64 oracle within BF16 publication tolerance");
        }
        for (unsigned long long row = 0ull; row < (serial ? rows : 1ull); ++row)
            YVEX_TEST_ASSERT(ops->activation(&work, values + row * width * sizeof(float),
                serial ? 1ull : rows, activation_width, width, &activation, status,
                "cuda.test.emissions.activation", &failure, &err) == YVEX_OK,
                "publish the admitted activation class with physical vector stride");
        YVEX_TEST_ASSERT(yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.emissions.activation", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK,
            "complete emitted publication");
        YVEX_TEST_ASSERT(actual->status == seed->status && actual->before == seed->before &&
            actual->after == seed->after && !memcmp(actual->weights, seed->weights, sizeof(seed->weights)),
            "emission status, canaries and immutable weights retained");
        if (negative == 3u) YVEX_TEST_ASSERT(!memcmp(seed, actual, sizeof(*seed)),
            "prior status prevents every emitted-vector transformation");
        if (serial) YVEX_TEST_ASSERT(!memcmp(previous, actual->values, sizeof(actual->values)),
            "batch/serial normalization, positional stride, Hadamard/quantization and unused tail are byte-identical");
        else memcpy(previous, actual->values, sizeof(actual->values));
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "retire emitted-vector storage");
    free(previous); free(actual); free(seed);
    return 0;
}

typedef struct {
    yvex_cuda_work *work;
    CUdeviceptr values, status;
    unsigned long long heads, tokens, width, start, step;
    yvex_backend_attention_position *position;
    yvex_backend_attention_failure *failure;
    int inverse;
} rope_graph_fixture;

static int rope_graph_enqueue(void *context, int enqueue, yvex_error *err)
{
    rope_graph_fixture *f = context;
    f->work->prepare_only = !enqueue;
    int rc = yvex_cuda_attention_operations_get()->rope(f->work, f->values,
        f->heads, f->tokens, f->width, f->start, f->step, f->position,
        f->inverse, f->status, "cuda.test.rope.graph", f->failure, err);
    f->work->prepare_only = 0;
    return rc;
}

/* Source-geometry angle reuse must equal separate one-vector launches exactly.
 * The scalar oracle is independent of the CUDA frequency implementation and
 * uses the existing F64-to-BF16 publication tolerance, including YaRN ramps. */
static int rope_cohort_check(yvex_backend *backend, unsigned long long tokens,
    unsigned long long heads, unsigned long long dims, unsigned long long scaling,
    int inverse, unsigned int negative)
{
    const unsigned long long width = 512ull, step = 4ull, start = 8191ull;
    const size_t count = (size_t)(tokens * heads * width), value_bytes = count * sizeof(float);
    const size_t bytes = value_bytes + 2u * sizeof(float) + sizeof(int);
    unsigned char *seed = calloc(1u, bytes), *actual = malloc(bytes), *previous = malloc(bytes);
    yvex_backend_tensor_desc desc = {.name = "rope-cohort", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {bytes}, .bytes = bytes};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    const yvex_cuda_attention_operations *ops = yvex_cuda_attention_operations_get();
    yvex_backend_attention_position position = {.theta = negative == 3u ? 1ull : 10000ull,
        .scaling_factor = scaling, .original_context = scaling == 1ull ? 0ull : 8192ull,
        .beta_fast = 32ull, .beta_slow = 1ull, .rope_dimensions = dims};
    yvex_backend_attention_failure failure = {0};
    yvex_error err = {0};
    YVEX_TEST_ASSERT(seed && actual && previous, "allocate independent RoPE controls");
    float *input = (float *)seed + 1;
    ((float *)seed)[0] = input[count] = 12345.0f;
    for (size_t i = 0u; i < count; ++i) input[i] = (float)((int)(i * 19u % 251u) - 125) / 128.0f;
    if (negative == 1u) *(int *)(seed + bytes - sizeof(int)) = 17;
    if (negative == 2u) input[width - dims] = NAN;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate bounded RoPE fixture");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr values = base + sizeof(float), status = base + bytes - sizeof(int);
    unsigned iterations = negative ? 1u :
        (tokens == 4ull && (heads == 9ull || heads == 65ull) ? 4u : 2u);
    for (unsigned int serial = 0u; serial < iterations; ++serial) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, seed, bytes, &err) == YVEX_OK,
            "initialize ordered RoPE fixture");
        if (!serial) {
            int rc = ops->rope(&work, values, heads, tokens, width, start, step,
                &position, inverse, status, "cuda.test.rope.cohort", &failure, &err);
            YVEX_TEST_ASSERT(rc == (negative == 4u ? YVEX_ERR_BOUNDS : YVEX_OK),
                "execute real heads or refuse odd rotary dimensions before launch");
        } else if (serial >= 2u) {
            rope_graph_fixture graph = {.work = &work, .values = values, .status = status,
                .heads = heads, .tokens = tokens, .width = width, .start = start, .step = step,
                .position = &position, .failure = &failure, .inverse = inverse};
            char key[128];
            snprintf(key, sizeof(key), "test.rope.cohort.%llu.%llu.%llu.%llu.%d",
                dims, heads, tokens, scaling, inverse);
            yvex_backend_cuda_graph_info info = {0};
            int rc = yvex_cuda_graph_execute(backend, key, NULL,
                rope_graph_enqueue, &graph, 0u, &info, &err);
            if (rc != YVEX_OK || !info.capture_count)
                fprintf(stderr, "RoPE graph rc=%d state=%u captures=%llu kernels=%llu reason=%s\n",
                    rc, (unsigned)info.state, info.capture_count, info.inventory.kernel_node_count, err.message);
            YVEX_TEST_ASSERT(rc == YVEX_OK && info.capture_count && info.replay_count,
                "RoPE capture/rebind/replay consumes exact current head and rotary geometry");
            if (serial == 3u && dims == 64ull && heads == 9ull && scaling == 16ull && !inverse) {
                YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, seed, bytes, &err) == YVEX_OK,
                    "initialize geometry mismatch witness");
                graph.heads = 8ull;
                YVEX_TEST_ASSERT(yvex_cuda_graph_execute(backend, key, NULL,
                    rope_graph_enqueue, &graph, 0u, &info, &err) == YVEX_ERR_STATE,
                    "a changed head grid cannot reuse a foreign compatibility identity");
                YVEX_TEST_ASSERT(yvex_backend_tensor_read(backend, arena, actual, bytes, &err) == YVEX_OK &&
                    !memcmp(actual, seed, bytes), "incompatible graph refuses before operand mutation");
                graph.heads = heads;
                YVEX_TEST_ASSERT(yvex_cuda_graph_execute(backend, key, NULL,
                    rope_graph_enqueue, &graph, 0u, &info, &err) == YVEX_OK,
                    "original geometry recovers after refused replay");
            }
        } else for (unsigned long long token = 0ull; token < tokens; ++token)
            for (unsigned long long head = 0ull; head < heads; ++head)
                YVEX_TEST_ASSERT(ops->rope(&work, values + (token * heads + head) * width * sizeof(float),
                    1ull, 1ull, width, start + token * step, step, &position, inverse, status,
                    "cuda.test.rope.serial", &failure, &err) == YVEX_OK,
                    "execute unchanged one-vector trigonometric control");
        int device_wide = 0;
        YVEX_TEST_ASSERT(yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.rope", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, actual, bytes, &err) == YVEX_OK,
            "complete RoPE publication");
        float *out = (float *)actual + 1;
        int observed = *(int *)(actual + bytes - sizeof(int));
        YVEX_TEST_ASSERT(((float *)actual)[0] == 12345.0f && out[count] == 12345.0f,
            "RoPE write extent preserves both canaries");
        YVEX_TEST_ASSERT(observed == (negative == 1u ? 17 : negative == 2u ? 1 : negative == 3u ? 2 : 0),
            "RoPE retains prior status and refuses nonfinite/invalid parameters");
        if (negative == 1u || negative == 3u || negative == 4u)
            YVEX_TEST_ASSERT(!memcmp(input, out, value_bytes), "refused RoPE does not mutate values");
        if (serial) YVEX_TEST_ASSERT(!memcmp(previous, actual, bytes),
            "shared and separately recomputed angles publish exactly identical bits");
        else memcpy(previous, actual, bytes);
        if (!negative && !serial) for (unsigned long long token = 0ull; token < tokens; ++token)
            for (unsigned long long pair = 0ull; pair < dims / 2ull; ++pair) {
                double frequency = pow(10000.0, -2.0 * (double)pair / (double)dims);
                if (scaling != 1ull) {
                    double pi = acos(-1.0);
                    double low = floor((double)dims * log(8192.0 / (32.0 * 2.0 * pi)) / (2.0 * log(10000.0)));
                    double high = ceil((double)dims * log(8192.0 / (2.0 * pi)) / (2.0 * log(10000.0)));
                    low = fmax(low, 0.0); high = fmin(high, (double)dims - 1.0);
                    if (low == high) high += 0.001;
                    double ramp = fmin(1.0, fmax(0.0, ((double)pair - low) / (high - low)));
                    frequency *= (1.0 - ramp) + ramp / (double)scaling;
                }
                double angle = (double)(start + token * step) * frequency;
                double c = cos(angle), s = (inverse ? -1.0 : 1.0) * sin(angle);
                for (unsigned long long head = 0ull; head < heads; ++head) {
                    size_t i = (size_t)((token * heads + head) * width + width - dims + 2ull * pair);
                    double left = input[i] * c - input[i + 1u] * s;
                    double right = input[i] * s + input[i + 1u] * c;
                    YVEX_TEST_ASSERT(fabs((double)out[i] - left) <= fabs(left) / 256.0 + 2e-5 &&
                        fabs((double)out[i + 1u] - right) <= fabs(right) / 256.0 + 2e-5,
                        "forward/inverse scaled RoPE agrees with independent F64/BF16 oracle");
                }
            }
        for (unsigned long long vector = 0ull; vector < tokens * heads; ++vector)
            YVEX_TEST_ASSERT(!memcmp(input + vector * width, out + vector * width,
                (size_t)(width - dims) * sizeof(float)), "nonrotary coordinates stay unchanged");
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "retire RoPE test state");
    free(seed); free(actual); free(previous);
    return 0;
}

typedef struct {
    float query[17 * 16 * 512], local[160 * 512], compressed[160 * 512], sinks[16];
    unsigned long long positions[160], compressed_positions[160], selected[17 * 160], counts[17];
    float before, output[17 * 16 * 512], after;
    int status;
} block_reduction_fixture;

typedef struct {
    yvex_cuda_work work;
    yvex_cuda_attention_reduction_phase phase;
    yvex_backend_attention_failure failure;
} block_graph_fixture;

static int block_graph_enqueue(void *context, int enqueue, yvex_error *err)
{
    block_graph_fixture *fixture = context;
    int rc = yvex_cuda_work_cleanup(&fixture->work, err);
    if (rc != YVEX_OK) return rc;
    backend_workspace_reset(fixture->work.backend);
    fixture->work.prepare_only = !enqueue;
    rc = yvex_cuda_attention_operations_get()->reduction_phase(&fixture->work,
        &fixture->phase, "cuda.test.online64.graph", &fixture->failure, err);
    fixture->work.prepare_only = 0;
    return rc;
}

static int block_graph_replay_check(yvex_backend *backend, int wide)
{
    block_reduction_fixture *seed = calloc(1u, sizeof(*seed)), *actual = calloc(1u, sizeof(*actual));
    yvex_device_tensor *arena = NULL, *workspace = NULL, *resident = NULL;
    unsigned char *resident_data = NULL;
    yvex_backend_tensor_desc desc = {.name = "online64-graph", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(*seed)}, .bytes = sizeof(*seed)};
    yvex_backend_attention_job job = {.phase = YVEX_BACKEND_ATTENTION_PHASE_DECODE,
        .token_count = 1, .query_heads = 16, .head_dimension = 512, .sliding_window = 128,
        .local_stride = 512, .compressed_stride = 512, .indexer_topk = 160,
        .compute_contract = YVEX_BACKEND_ATTENTION_COMPUTE_BF16_ONLINE64_F32_MMA_V1};
    block_graph_fixture fixture = {.work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, .activation_q8 = 1}};
    yvex_error err = {0};
    job.token_count = wide ? 17ull : 1ull;
    job.phase = wide ? YVEX_BACKEND_ATTENTION_PHASE_PREFILL : YVEX_BACKEND_ATTENTION_PHASE_DECODE;
    YVEX_TEST_ASSERT(seed && actual, "allocate online64 replay oracle");
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate online64 replay operands");
    desc.bytes = desc.dims[0] = 32ull;
    YVEX_TEST_ASSERT(yvex_backend_resident_alloc(backend, &desc, &resident, &resident_data, &err) == YVEX_OK &&
        yvex_backend_resident_attach(backend, resident_data, desc.bytes, resident, 1ull, &err) == YVEX_OK,
        "attach bounded replay residency");
    desc.bytes = desc.dims[0] = 2ull * 1024ull * 1024ull;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &workspace, &err) == YVEX_OK &&
        yvex_backend_workspace_attach(backend, workspace, 1ull, &err) == YVEX_OK &&
        yvex_backend_cuda_attention_configure(backend, job.phase, YVEX_BACKEND_CUDA_ATTENTION_FULL,
            "online64-replay", wide ? "prefill-17" : "decode-1", 127ull, 160ull, 159ull, &err) == YVEX_OK,
        "admit bounded replay workspace and history");
    yvex_backend_workspace_summary admitted;
    YVEX_TEST_ASSERT(yvex_backend_workspace_summary_get(backend, &admitted) &&
        admitted.attached && admitted.capacity == desc.bytes && admitted.generation == 1ull &&
        !yvex_backend_workspace_summary_get(NULL, &admitted) &&
        !yvex_backend_workspace_summary_get(backend, NULL),
        "graph observes the attached device budget, not a standalone primitive constant");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
#define BLOCK_ADDRESS(member) (base + offsetof(block_reduction_fixture, member))
    fixture.phase = (yvex_cuda_attention_reduction_phase){&job, BLOCK_ADDRESS(query), BLOCK_ADDRESS(local),
        BLOCK_ADDRESS(positions), BLOCK_ADDRESS(compressed), BLOCK_ADDRESS(compressed_positions),
        BLOCK_ADDRESS(selected), BLOCK_ADDRESS(counts), BLOCK_ADDRESS(sinks), BLOCK_ADDRESS(output),
        BLOCK_ADDRESS(status), 0ull, 0ull, 160ull};
#undef BLOCK_ADDRESS
    const unsigned long long starts[3][6] = {{61, 62, 63, 64, 65, 63},
        {29, 30, 31, 32, 33, 31}, {8190, 8191, 8318, 8319, 8320, 8318}};
    const unsigned long long wide_starts[] = {433, 434, 435, 436, 437, 435};
    for (unsigned int cls = wide ? 1u : 0u; cls < (wide ? 2u : 3u); ++cls) {
        char keys[6][160];
        job.attention_class = (yvex_backend_attention_class)cls;
        job.compression_ratio = cls == 1u ? 4ull : cls == 2u ? 128ull : 0ull;
        for (unsigned int sample = 0; sample < 6; ++sample) {
            fixture.phase.output = base + (sample & 1u
                ? offsetof(block_reduction_fixture, query) : offsetof(block_reduction_fixture, output));
            job.token_position = wide ? wide_starts[sample] : starts[cls][sample];
            job.local_count = job.token_position < 127ull ? job.token_position : 127ull;
            fixture.phase.initial_local_count = job.local_count;
            fixture.phase.compressed_count = cls ? (job.token_position + job.token_count) / job.compression_ratio : 0ull;
            memset(seed, 0, sizeof(*seed));
            seed->before = seed->after = 12345.0f;
            for (unsigned int i = 0; i < 160; ++i) {
                seed->positions[i] = job.token_position - job.local_count + i;
                seed->compressed_positions[i] = i * job.compression_ratio;
                for (unsigned long long t = 0; t < job.token_count; ++t) seed->selected[t * 160ull + i] = i;
                for (unsigned int d = 0; d < 512; ++d)
                    seed->local[i * 512 + d] = seed->compressed[i * 512 + d] = (float)((int)(d % 9) - 4) / 8.0f;
            }
            for (unsigned long long t = 0; t < job.token_count; ++t)
                seed->counts[t] = fixture.phase.compressed_count;
            YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, seed, sizeof(*seed), &err) == YVEX_OK,
                "update visible history before graph replay");
            backend_workspace_reset(backend);
            YVEX_TEST_ASSERT(yvex_cuda_attention_graph_key(backend, &job,
                YVEX_CUDA_ATTENTION_STAGE_REDUCE, YVEX_CUDA_ATTENTION_STAGE_REDUCE + 1u,
                keys[sample], &err) == YVEX_OK, "derive real reduction topology key");
            yvex_backend_cuda_graph_info info;
            int rc = yvex_cuda_graph_execute(backend, keys[sample], NULL,
                block_graph_enqueue, &fixture, 0u, &info, &err);
            if (rc != YVEX_OK) fprintf(stderr, "online64 graph: %s\n", yvex_error_message(&err));
            YVEX_TEST_ASSERT(rc == YVEX_OK && info.capture_count == 1ull &&
                yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK,
                "capture or replay prepared reduction across causal and preparation bucket boundaries");
            const float *published = sample & 1u ? actual->query : actual->output;
            for (unsigned long long t = 0; t < job.token_count; ++t)
            for (unsigned int h = 0; h < 16; ++h) for (unsigned int d = 0; d < 512; ++d) {
                unsigned long long count = job.local_count + 1ull +
                    (cls ? (job.token_position + t + 1ull) / job.compression_ratio : 0ull);
                float expected = reduction_bf16((float)((double)seed->local[d] * count / (count + 1ull)));
                YVEX_TEST_ASSERT(actual->status == 0 && published[(t * 16ull + h) * 512ull + d] == expected,
                    "replayed graph preserves independently counted visible rows and sink");
            }
            YVEX_TEST_ASSERT(actual->before == seed->before && actual->after == seed->after,
                "graph replay preserves publication bounds");
        }
        YVEX_TEST_ASSERT(!strcmp(keys[0], keys[1]) && !strcmp(keys[2], keys[5]) &&
            strcmp(keys[0], keys[4]), "same pitch reuses a graph; changed pitch separates topology");
    }
    unsigned long long released = 0ull;
    YVEX_TEST_ASSERT(yvex_backend_cuda_attention_graph_registry_apply(backend,
        YVEX_BACKEND_CUDA_GRAPH_REGISTRY_RELEASE, &released, &err) == YVEX_OK && released == (wide ? 2ull : 6ull) &&
        yvex_cuda_work_cleanup(&fixture.work, &err) == YVEX_OK, "retire all six geometry graphs");
    YVEX_TEST_ASSERT(yvex_backend_resident_detach(backend, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &resident, &err) == YVEX_OK, "retire replay residency");
    yvex_backend_workspace_detach(backend);
    YVEX_TEST_ASSERT(yvex_backend_workspace_summary_get(backend, &admitted) &&
        !admitted.attached && !admitted.capacity && !admitted.generation,
        "retired device workspace cannot authorize another operation");
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &workspace, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release replay workspace and operands");
    free(actual); free(seed);
    return 0;
}

static int block_reduction_check(yvex_backend *backend, unsigned int cls,
                                unsigned int negative, int wide, int inplace)
{
    block_reduction_fixture *seed = calloc(1u, sizeof(*seed)), *actual = calloc(1u, sizeof(*actual));
    yvex_backend_tensor_desc desc = {.name = "online64-reduction", .dtype = YVEX_DTYPE_I8,
        .rank = 1u, .dims = {sizeof(*seed)}, .bytes = sizeof(*seed)};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, .activation_q8 = 1};
    yvex_backend_attention_job job = {.token_count = 7, .query_heads = 16, .head_dimension = 512,
        .sliding_window = 16, .local_stride = 512, .compressed_stride = 512,
        .attention_class = (yvex_backend_attention_class)cls, .native_execution = 1,
        .compute_contract = YVEX_BACKEND_ATTENTION_COMPUTE_BF16_ONLINE64_F32_MMA_V1};
    job.token_count = wide ? 17ull : 7ull;
    job.sliding_window = wide ? 128ull : 16ull;
    job.token_position = wide ? (cls == 2u ? 16383ull : 511ull) : (cls == 2u ? 511ull : 63ull);
    job.compression_ratio = cls == 1u ? 4ull : cls == 2u ? 128ull : 0ull;
    unsigned long long compressed = cls ? (job.token_position + job.token_count) / job.compression_ratio : 0ull;
    yvex_error err = {0}; yvex_backend_attention_failure failure = {0};
    int device_wide = 0;
    YVEX_TEST_ASSERT(seed && actual, "allocate block reduction analytic oracle");
    seed->before = seed->after = 12345.0f;
    for (unsigned int i = 0; i < 17u * 16u * 512u; ++i) seed->output[i] = 12345.0f;
    for (unsigned int r = 0; r < 160; ++r) {
        seed->positions[r] = job.token_position - (job.sliding_window - 1ull) + r;
        seed->compressed_positions[r] = r * job.compression_ratio;
        for (unsigned int d = 0; d < 512; ++d)
            seed->local[r * 512 + d] = seed->compressed[r * 512 + d] = (float)((int)(d % 9) - 4) / 8.0f;
    }
    for (unsigned int t = 0; t < job.token_count; ++t) {
        seed->counts[t] = compressed;
        for (unsigned long long i = 0; i < compressed; ++i) seed->selected[t * 160 + i] = compressed - i - 1ull;
    }
    if (negative == 1u) seed->query[0] = NAN;
    if (negative == 2u) seed->local[0] = INFINITY;
    if (negative == 3u) seed->sinks[0] = NAN;
    if (negative == 4u) seed->status = 17;
    if (negative == 5u) seed->positions[0] = ~0ull;
    if (negative == 6u) seed->selected[0] = compressed;
    if (negative == 7u) work.budget = 1ull;
    if (negative == 8u) {
        seed->compressed[0] = NAN;
        for (unsigned int t = 0; t < job.token_count; ++t) {
            seed->counts[t] = compressed - 1ull;
            for (unsigned long long i = 0; i + 1ull < compressed; ++i)
                seed->selected[t * 160 + i] = compressed - i - 1ull;
        }
    }
    if (negative == 9u) {
        /* Selected but causally invisible until after this whole phase. */
        seed->compressed[compressed * 512ull] = NAN;
        for (unsigned int t = 0; t < job.token_count; ++t) {
            seed->selected[t * 160 + compressed] = compressed;
            seed->counts[t] = compressed + 1ull;
        }
    }
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, seed, sizeof(*seed), &err) == YVEX_OK, "upload online64 fixture");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
#define BLOCK_ADDRESS(member) (base + offsetof(block_reduction_fixture, member))
    yvex_cuda_attention_reduction_phase phase = {&job, BLOCK_ADDRESS(query), BLOCK_ADDRESS(local),
        BLOCK_ADDRESS(positions), BLOCK_ADDRESS(compressed), BLOCK_ADDRESS(compressed_positions),
        BLOCK_ADDRESS(selected), BLOCK_ADDRESS(counts), BLOCK_ADDRESS(sinks), BLOCK_ADDRESS(output),
        BLOCK_ADDRESS(status), job.sliding_window - 1ull, compressed, 160ull};
#undef BLOCK_ADDRESS
    if (inplace) phase.output = phase.query;
    if (negative == 9u) phase.compressed_count++;
    int rc = yvex_cuda_attention_operations_get()->reduction_phase(&work, &phase,
        "cuda.test.online64", &failure, &err);
    YVEX_TEST_ASSERT((negative == 7u ? rc != YVEX_OK : rc == YVEX_OK) &&
        yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
            &device_wide, "cuda.test.online64", &err) == YVEX_OK &&
        yvex_backend_tensor_read(backend, arena, actual, sizeof(*actual), &err) == YVEX_OK,
        "complete admitted reduction or bounded scratch refusal");
    YVEX_TEST_ASSERT(actual->before == seed->before && actual->after == seed->after,
        "online64 output extent preserves canaries");
    if (negative && negative < 8u) {
        YVEX_TEST_ASSERT((negative == 7u || actual->status != 0) &&
            (negative != 4u || actual->status == 17), "online64 retains first failure");
        if (negative == 4u || negative == 7u)
            YVEX_TEST_ASSERT(inplace ? !memcmp(seed->query, actual->query, sizeof(seed->query)) :
                !memcmp(seed->output, actual->output, sizeof(seed->output)),
                "prior failure and resource refusal publish nothing");
    } else for (unsigned int t = 0; t < job.token_count; ++t) {
        unsigned long long n = job.sliding_window + (cls ? (job.token_position + t + 1ull) / job.compression_ratio : 0ull);
        if (negative == 8u) n--;
        for (unsigned int h = 0; h < 16; ++h) for (unsigned int d = 0; d < 512; ++d) {
            float expected = reduction_bf16((float)((double)seed->local[d] * n / (n + 1ull)));
            const float *published = inplace ? actual->query : actual->output;
            YVEX_TEST_ASSERT(actual->status == 0 && published[(t * 16u + h) * 512u + d] == expected,
                "uniform scores follow independently counted causal rows and sink");
        }
    }
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release online64 scratch and fixture");
    free(actual); free(seed);
    return 0;
}

int yvex_cuda_test_attention_reduction(void)
{
    const unsigned long long widths[] = {1ull, 127ull, 128ull, 255ull, 256ull, 257ull,
        511ull, 512ull, 768ull, 1023ull, 1024ull, 1025ull, 1280ull};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open reduction CUDA backend");
    const unsigned long long norm_rows[] = {1ull, 7ull, 8ull, 512ull, 513ull};
    for (size_t r = 0u; r < sizeof(norm_rows) / sizeof(norm_rows[0]); ++r)
        for (size_t w = 0u; w < sizeof(widths) / sizeof(widths[0]); ++w)
            for (unsigned int scenario = 0u; scenario < 5u; ++scenario)
                if (unit_norm_check(backend, norm_rows[r], widths[w], scenario)) return 1;
    puts("Unit normalization: 325 exact-tree, independent BF16 bound, vector/column tail and refusal controls");
    if (projection_job_admission_check()) return 1;
    if (prepared_phase_submission_check(backend)) return 1;
    const unsigned matrix_inputs[] = {1u, 3u, 31u, 32u, 33u, 65u, 1025u, 2048u};
    for (unsigned i = 0u; i < sizeof(matrix_inputs) / sizeof(matrix_inputs[0]); ++i)
        for (int publication = 0; publication <= 1; ++publication)
            if (bf16_projection_check(backend, matrix_inputs[i], publication, 0)) return 1;
    for (int negative = 1; negative <= 8; ++negative)
        if (bf16_projection_check(backend, 1u, 0, negative) ||
            bf16_projection_check(backend, 31u, 0, negative) ||
            bf16_projection_check(backend, 33u, 0, negative)) return 1;
    puts("BF16/F32 projection: 16 numerical/tail controls at K4096 through N2048, 24 refusal controls; narrow/wide replay and independent F64 bound; explicit class only");
    for (unsigned q = 0; q < 2u; ++q) {
        unsigned qtype = q ? YVEX_GGUF_QTYPE_MXFP4 : YVEX_GGUF_QTYPE_BF16;
        for (int publication = 0; publication <= 1; ++publication)
            if (prepared_projection_check(backend, qtype, 128u, 519u, publication, 0, 256u) ||
                prepared_projection_check(backend, qtype, 4096u, 2203u, publication, 0, 256u) ||
                prepared_projection_check(backend, qtype, 4096u, 2203u, publication, 0, 2048u)) return 1;
        for (int negative = 1; negative <= 4; ++negative)
            if (prepared_projection_check(backend, qtype, 128u, 519u, 0, negative, 256u)) return 1;
    }
    puts("Prepared matrices: 12 numerical/tile/publication controls through N2048, 8 operand refusals, replay/class refusal and cleanup; independent codec/F64 bound");
    if (block_graph_replay_check(backend, 0) || block_graph_replay_check(backend, 1)) return 1;
    for (unsigned int cls = 0; cls < 3; ++cls)
        if (block_reduction_check(backend, cls, 0u, 0, 0) ||
            block_reduction_check(backend, cls, 0u, 1, 0) ||
            block_reduction_check(backend, cls, 0u, 0, 1) ||
            block_reduction_check(backend, cls, 0u, 1, 1)) return 1;
    for (unsigned int negative = 1; negative <= 9; ++negative)
        if (block_reduction_check(backend, 1u, negative, 0, 0) ||
            block_reduction_check(backend, 1u, negative, 1, 0) ||
            block_reduction_check(backend, 1u, negative, 0, 1) ||
            block_reduction_check(backend, 1u, negative, 1, 1)) return 1;
    puts("Online64: 24 analytic/refusal controls and 24 capture/rebind/replays; staged and fused geometry");
    const unsigned long long rope_tokens[] = {1ull, 4ull, 16ull};
    const unsigned long long rope_heads[] = {1ull, 3ull, 4ull, 7ull, 8ull, 9ull, 16ull, 64ull, 65ull};
    for (size_t t = 0u; t < sizeof(rope_tokens) / sizeof(rope_tokens[0]); ++t)
    for (unsigned long long dims = 64ull; dims <= 128ull; dims *= 2ull)
        for (size_t h = 0u; h < sizeof(rope_heads) / sizeof(rope_heads[0]); ++h)
            for (unsigned long long scaling = 1ull; scaling <= 16ull; scaling *= 16ull)
                for (int inverse = 0; inverse <= 1; ++inverse)
                    if (rope_cohort_check(backend, rope_tokens[t], rope_heads[h], dims,
                        scaling, inverse, 0u)) return 1;
    for (unsigned long long dims = 64ull; dims <= 128ull; dims *= 2ull)
    for (unsigned int negative = 1u; negative <= 3u; ++negative)
        if (rope_cohort_check(backend, 4ull, 9ull, dims, 16ull, 0, negative)) return 1;
    if (rope_cohort_check(backend, 4ull, 9ull, 1ull, 16ull, 0, 4u)) return 1;
    puts("RoPE cohorts: 216 bitwise batch/serial controls, 32 capture/rebind/replays, 7 error controls; 64/128 rotary geometry, head tails, independent scaled/inverse F64 oracle; unchanged BF16 tolerance");
    const unsigned long long emission_rows[] = {1ull, 2ull, 7ull, 31ull, 32ull, 128ull};
    const unsigned long long emission_ratios[] = {1ull, 4ull, 128ull};
    for (size_t i = 0u; i < sizeof(emission_rows) / sizeof(emission_rows[0]); ++i)
        for (size_t r = 0u; r < sizeof(emission_ratios) / sizeof(emission_ratios[0]); ++r)
            for (int index = 0; index <= 1; ++index)
                if (emission_rows_check(backend, emission_rows[i], emission_ratios[r], index, 0u)) return 1;
    for (unsigned int negative = 1u; negative <= 3u; ++negative)
        if (emission_rows_check(backend, 7ull, 4ull, 0, negative)) return 1;
    puts("compressed emissions: 36 batch/serial controls, 3 refusal controls; exact normalization/BF16 oracle; "
         "RoPE F64 tolerance=abs(reference)/256+2e-5; main/index activation bytes identical");
    for (unsigned long long ratio = 4ull; ratio <= 128ull; ratio *= 32ull)
        for (unsigned long long head = 128ull; head <= 512ull; head *= 4ull)
            for (int checkpoints = 0; checkpoints <= 1; ++checkpoints)
                for (unsigned long long rows = 6ull; rows <= 128ull; rows += 122ull)
                    if (rolling_rows_check(backend, ratio, head, rows, checkpoints, 0u)) return 1;
    for (unsigned int negative = 1u; negative <= 3u; ++negative)
        if (rolling_rows_check(backend, 4ull, 128ull, 6ull, 1, negative)) return 1;
    for (size_t i = 0u; i < sizeof(widths) / sizeof(widths[0]); ++i)
        for (unsigned int attention_class = 0u; attention_class < 3u; ++attention_class)
            for (unsigned int scenario = 0u; scenario < 3u; ++scenario) {
                if (reduction_check(backend, widths[i], attention_class, scenario, 0)) return 1;
                if (widths[i] <= 512ull && reduction_check(backend, widths[i], attention_class, scenario, 2)) return 1;
                if (widths[i] == 512ull && reduction_check(backend, widths[i], attention_class, scenario, 3)) return 1;
            }
    for (unsigned int scenario = 3u; scenario <= 7u; ++scenario)
        if (reduction_check(backend, 512ull, 1u, scenario, 0)) return 1;
    for (unsigned int scenario = 3u; scenario <= 7u; ++scenario)
        if (reduction_check(backend, 512ull, 1u, scenario, 2)) return 1;
    for (unsigned int scenario = 3u; scenario <= 7u; ++scenario)
        if (reduction_check(backend, 512ull, 1u, scenario, 3)) return 1;
    for (unsigned int attention_class = 0u; attention_class < 3u; ++attention_class)
        for (unsigned int scenario = 0u; scenario < 3u; ++scenario)
            if (reduction_check(backend, 512ull, attention_class, scenario, 1)) return 1;
    for (unsigned int scenario = 3u; scenario <= 7u; ++scenario)
        if (reduction_check(backend, 512ull, 1u, scenario, 1)) return 1;
    yvex_backend_close(backend);
    return 0;
}
