/* Independent decoded F64 oracle; nonfinite operands must fail before nonlinear clamping. */
#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/quant_numeric.h>
#include "src/backend/cuda/private.h"
#include "tests/test.h"

enum { DOT_WIDTH = 128, DOT_ROWS = 3, DOT_TOKENS = 2 };
typedef struct {
    unsigned char weights[DOT_ROWS * DOT_WIDTH * sizeof(float)];
    float input[DOT_TOKENS * DOT_WIDTH], route;
    unsigned long long selected;
    float before_canary, output[DOT_ROWS * DOT_TOKENS], after_canary;
    int status;
} dot_storage;

static int dot_fixture(dot_storage *s, unsigned int qtype, unsigned int scenario,
                       double expected[DOT_ROWS * DOT_TOKENS], unsigned long long *row_bytes)
{
    const yvex_gguf_qtype_geometry *g = yvex_gguf_qtype_geometry_find(qtype);
    float values[DOT_WIDTH], decoded[DOT_WIDTH];
    yvex_quant_failure failure;
    yvex_error err;
    *row_bytes = DOT_WIDTH / g->block_size * g->bytes_per_block;
    for (unsigned int i = 0u; i < DOT_WIDTH; ++i) values[i] = 1.0f;
    for (unsigned int row = 0u; row < DOT_ROWS; ++row)
        for (unsigned int i = 0u; i < DOT_WIDTH; i += g->block_size) {
            size_t written = 0u;
            unsigned char *p = s->weights + row * *row_bytes + i / g->block_size * g->bytes_per_block;
            YVEX_TEST_ASSERT(yvex_quant_encode_block(qtype, values + i, g->block_size,
                p, g->bytes_per_block, &written, &failure, &err) == YVEX_OK &&
                written == g->bytes_per_block, "encode finite dot fixture");
        }
    for (unsigned int token = 0u; token < DOT_TOKENS; ++token)
        for (unsigned int i = 0u; i < DOT_WIDTH; ++i)
            s->input[token * DOT_WIDTH + i] = scenario == 3u ?
                (i == 0u || i == 32u ? FLT_MAX : i == 64u || i == 96u ? -FLT_MAX : 0.0f)
                : (float)((int)((i * 7u + token) % 31u) - 15) / 16.0f;
    for (unsigned int row = 0u; row < DOT_ROWS; ++row) {
        for (unsigned int i = 0u; i < DOT_WIDTH; i += g->block_size)
            YVEX_TEST_ASSERT(yvex_quant_decode_block(qtype,
                s->weights + row * *row_bytes + i / g->block_size * g->bytes_per_block,
                g->bytes_per_block, decoded + i, g->block_size, &failure, &err) == YVEX_OK,
                "independent CPU decode for F64 dot oracle");
        for (unsigned int token = 0u; token < DOT_TOKENS; ++token) {
            double sum = 0.0;
            for (unsigned int i = 0u; i < DOT_WIDTH; ++i)
                sum += (double)decoded[i] * s->input[token * DOT_WIDTH + i];
            expected[token * DOT_ROWS + row] = sum;
        }
    }
    if (scenario == 1u) s->input[37] = NAN;
    if (scenario == 2u) s->input[37] = INFINITY;
    if (scenario == 4u) {
        if (qtype == YVEX_GGUF_QTYPE_F32) { uint32_t bits = 0x7fc00000u; memcpy(s->weights + 37u * 4u, &bits, 4u); }
        else if (qtype == YVEX_GGUF_QTYPE_MXFP4) s->weights[17] = 255u;
        else if (qtype == YVEX_GGUF_QTYPE_Q8_0) { uint16_t bits = 0x7e00u; memcpy(s->weights + 34u, &bits, 2u); }
        else { uint16_t bits = qtype == YVEX_GGUF_QTYPE_F16 ? 0x7e00u : 0x7fc0u; memcpy(s->weights + 37u * 2u, &bits, 2u); }
    }
    if (scenario == 5u) s->status = 1;
    s->route = 1.0f;
    s->before_canary = s->after_canary = 12345.0f;
    for (unsigned int i = 0u; i < DOT_ROWS * DOT_TOKENS; ++i) s->output[i] = 12345.0f;
    return 0;
}

static int dot_case(yvex_backend *backend, unsigned int qtype, unsigned int scenario, unsigned int mode)
{
    dot_storage before = {0}, after;
    double expected[DOT_ROWS * DOT_TOKENS], maximum_error = 0.0, limit = 10.0;
    unsigned long long bytes, width = DOT_WIDTH, rows = DOT_ROWS, tokens = DOT_TOKENS;
    unsigned long long zero = 0ull, one = 1ull, expert_bytes;
    int no = 0, device_wide = 0, rc, failure = scenario == 1u || scenario == 2u || scenario >= 4u;
    yvex_backend_tensor_desc d = {.name = "dot-finiteness", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    yvex_error err;
    CUdeviceptr base, weight, input, output, status, selected, route, absent = 0ull;
    if (dot_fixture(&before, qtype, scenario, expected, &bytes)) return 1;
    expert_bytes = bytes * rows;
    if (mode == 3u) {
        memcpy(before.weights + expert_bytes, before.weights, (size_t)expert_bytes);
        if (scenario >= 6u) {
            uint16_t bits = scenario == 6u ? 0x7fc0u : 0x7f80u;
            memcpy(before.weights + expert_bytes + 37u * 2u, &bits, sizeof(bits));
        }
    }
    d.bytes = d.dims[0] = sizeof(before);
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &d, &arena, &err) == YVEX_OK &&
        yvex_backend_tensor_write(backend, arena, &before, sizeof(before), &err) == YVEX_OK,
        "upload finite/invalid dot inputs");
    base = yvex_cuda_activation_pointer(backend, arena);
    weight = base + offsetof(dot_storage, weights); input = base + offsetof(dot_storage, input);
    output = base + offsetof(dot_storage, output); status = base + offsetof(dot_storage, status);
    selected = base + offsetof(dot_storage, selected); route = base + offsetof(dot_storage, route);
    CUdeviceptr second_weight = weight + expert_bytes;
    CUdeviceptr second_output = output + DOT_ROWS * sizeof(float);
    void *ordinary[] = {&weight, &bytes, &width, &zero, &rows, &tokens, &qtype,
        &input, &width, &no, &no, &no, &absent, &output, &rows, &no, &status};
    void *grouped[] = {&weight, &bytes, &width, &one, &rows, &one, &tokens, &qtype,
        &input, &width, &output, &rows, &no, &status};
    void *expert[] = {&weight, &bytes, &expert_bytes, &qtype, &weight, &bytes, &expert_bytes, &qtype,
        &selected, &route, &one, &one, &input, &width, &no, &rows, &limit, &output, &status};
    void *pair[] = {&weight, &bytes, &second_weight, &bytes, &width, &rows, &input, &output, &second_output, &status};
    CUfunction function = mode == 0u ? state->qtype_matvec_function :
        mode == 1u ? state->qtype_grouped_rows_function : mode == 2u ? state->moe_grouped_up_function :
        state->attention_bf16_pair_function;
    rc = yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED, function, 1u, 256u, 0u,
        mode == 0u ? ordinary : mode == 1u ? grouped : mode == 2u ? expert : pair, "cuda.test.dot-finiteness", &err);
    if (rc == YVEX_OK) rc = yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
        &device_wide, "cuda.test.dot-finiteness", &err);
    if (rc != YVEX_OK) fprintf(stderr, "dot launch: %s\n", yvex_error_message(&err));
    YVEX_TEST_ASSERT(rc == YVEX_OK && yvex_backend_tensor_read(backend, arena, &after, sizeof(after), &err) == YVEX_OK,
        "read dot completion/status");
    YVEX_TEST_ASSERT((after.status != 0) == failure, "nonfinite input refuses before clamp; finite overflow remains recoverable");
    if (!failure) for (unsigned int i = 0u; i < (mode == 2u ? DOT_ROWS : DOT_ROWS * DOT_TOKENS); ++i) {
        double value = expected[mode == 3u ? i % DOT_ROWS : i];
        if (mode == 2u) {
            double g = fmin(value, limit), u = fmax(-limit, fmin(value, limit));
            double silu = g >= 0.0 ? g / (1.0 + exp(-g)) : g * exp(g) / (1.0 + exp(g));
            float result = (float)(silu * u);
            value = yvex_quant_bf16_decode(yvex_quant_bf16_encode(result));
        }
        double difference = fabs((double)after.output[i] - value);
        if (difference > maximum_error) maximum_error = difference;
        YVEX_TEST_ASSERT(isfinite(after.output[i]) && difference == 0.0, "exact finite dot/recovery/BF16 clamp oracle");
    }
    YVEX_TEST_ASSERT(!memcmp(before.weights, after.weights, sizeof(before.weights)) &&
        !memcmp(before.input, after.input, sizeof(before.input)) && after.before_canary == 12345.0f &&
        after.after_canary == 12345.0f, "inputs and bounded output canaries are preserved");
    if (scenario == 5u) YVEX_TEST_ASSERT(!memcmp(before.output, after.output, sizeof(before.output)),
        "prior refusal writes no dot result");
    printf("dot finiteness: qtype=%u mode=%u scenario=%u status=%d max_abs=%.12g tolerance=0 result=pass\n",
        qtype, mode, scenario, after.status, maximum_error);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release dot fixture");
    return 0;
}

static float certificate_weight(unsigned int row, unsigned long long column)
{
    float maximum = yvex_quant_bf16_decode(0x7f7fu);
    if (row == 1u) return column == 0ull ? 1.0f : column == 1ull ? 0x1p-24f : 0.0f;
    if (row == 2u) return column == 0ull ? maximum : column == 1ull ? -maximum : column == 2ull ? 1.0f : 0.0f;
    if (row == 3u) return column % 2ull ? -0.0f : 0.0f;
    if (row == 4u) return column == 0ull ? 0x1p-133f : 0.0f;
    if (row == 5u) return column < 2ull ? maximum : column < 4ull ? -maximum : 0.0f;
    uint32_t bits = (uint32_t)((column + 1ull) * 2654435761ull + row * 2246822519ull);
    bits = (bits & 0x807fffffu) | ((110u + (bits >> 24u) % 30u) << 23u);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int certificate_case(yvex_backend *backend, unsigned int qtype,
                            unsigned long long width, int prefix_control)
{
    enum { ROWS = 19 };
    unsigned long long bytes = width * (qtype == YVEX_GGUF_QTYPE_F32 ? 4ull : 2ull);
    size_t input_offset = (size_t)((bytes * ROWS + 7ull) & ~7ull);
    size_t output_offset = input_offset + (size_t)width * sizeof(float) + sizeof(float);
    size_t status_offset = output_offset + (ROWS + 1u) * sizeof(float);
    size_t total = status_offset + sizeof(int);
    unsigned char *before = calloc(1u, total), *after = malloc(total);
    float expected[ROWS], *input;
    unsigned long long rows = ROWS, tokens = 1ull, zero = 0ull;
    yvex_backend_tensor_desc desc = {.name = "ordered-dot-certificate", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_error err;
    unsigned int grid, block;
    int block_row, no = 0, device_wide = 0;
    YVEX_TEST_ASSERT(before && after, "allocate independent certificate oracle");
    input = (float *)(before + input_offset);
    for (unsigned long long i = 0ull; i < width; ++i)
        input[i] = prefix_control || i < 4ull ? 1.0f : (float)((int)(i % 251ull) - 125) / 127.0f;
    for (unsigned int row = 0u; row < ROWS; ++row) {
        double ordered = 0.0;
        for (unsigned long long i = 0ull; i < width; ++i) {
            float value = certificate_weight(row, i);
            if (prefix_control && row >= 6u) {
                /* Cancellation makes the global norm pessimistic. A tiny
                 * term defeats the exact-lattice shortcut; the last three
                 * terms approach a F32 midpoint from either side. */
                value = i < width - 4ull ? (i % 2ull ? -1.0f : 1.0f) :
                    i == width - 4ull ? 0x1p-100f : i == width - 3ull ? 1.0f :
                    i == width - 2ull ? 0x1p-24f : (row & 1u ? -0x1p-29f : 0x1p-29f);
            }
            unsigned char *encoded = before + row * bytes;
            if (qtype == YVEX_GGUF_QTYPE_F32) memcpy(encoded + i * 4ull, &value, sizeof(value));
            else {
                uint16_t bits = yvex_quant_bf16_encode(value);
                memcpy(encoded + i * 2ull, &bits, sizeof(bits));
                value = yvex_quant_bf16_decode(bits);
            }
            ordered = fma((double)value, (double)input[i], ordered);
        }
        expected[row] = (float)ordered;
    }
    float canary = 12345.0f;
    memcpy(before + output_offset - sizeof(float), &canary, sizeof(canary));
    memcpy(before + output_offset + ROWS * sizeof(float), &canary, sizeof(canary));
    desc.bytes = desc.dims[0] = total;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_cuda_qtype_matvec_geometry(rows, width, tokens, qtype, 1, 1,
            &grid, &block, &block_row), "admit partial certificate row population");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena), absent = 0ull;
    CUdeviceptr device_input = base + input_offset, output = base + output_offset;
    CUdeviceptr status = base + status_offset;
    for (int publication = 0; publication < 4; ++publication)
    for (int forensic = 0; forensic <= 1; ++forensic) {
        int bf16 = publication & 1;
        CUdeviceptr additive = publication & 2 ? device_input : absent;
        float published[ROWS];
        for (unsigned int row = 0u; row < ROWS; ++row) {
            float value = additive ? expected[row] + input[row] : expected[row];
            published[row] = bf16 ? yvex_quant_bf16_decode(yvex_quant_bf16_encode(value)) : value;
        }
        void *params[] = {&base, &bytes, &width, &zero, &rows, &tokens, &qtype,
            &device_input, &width, &no, &block_row, &forensic, &additive, &output,
            &rows, &bf16, &status};
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, before, total, &err) == YVEX_OK &&
            yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                yvex_cuda_state(backend)->qtype_matvec_function, grid, block, 0u,
                params, "cuda.test.dot-certificate", &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                &device_wide, "cuda.test.dot-certificate", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, after, total, &err) == YVEX_OK,
            "execute certified and literal ordered realizations");
        for (unsigned int row = 0u; row < ROWS; ++row) {
            const float *actual = (const float *)(after + output_offset);
            if (memcmp(&published[row], &actual[row], sizeof(float)))
                fprintf(stderr, "certificate mismatch qtype=%u width=%llu forensic=%d publication=%d row=%u expected=%a actual=%a status=%d\n",
                    qtype, width, forensic, publication, row, (double)published[row], (double)actual[row],
                    *(int *)(after + status_offset));
        }
        YVEX_TEST_ASSERT(*(int *)(after + status_offset) == 0 &&
            memcmp(published, after + output_offset, sizeof(published)) == 0,
            "bitwise scalar F64 oracle: ties, cancellation, norm overflow, subnormal and random rows");
        YVEX_TEST_ASSERT(!memcmp(before, after, output_offset) &&
            !memcmp(before + output_offset + sizeof(expected), after + output_offset + sizeof(expected),
                total - output_offset - sizeof(expected)), "certificate preserves inputs and both output canaries");
    }
    printf("ordered dot certificate: qtype=%u width=%llu rows=%u realizations=2 publications=4 bit_differences=0\n",
        qtype, width, ROWS);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "release certificate oracle ownership");
    free(before);
    free(after);
    return 0;
}

/* Check the encoded Q8_0 class against CPU block decoding plus a literal F64
 * oracle, including rounding ties, cancellation, signed zero and subnormals.
 * Nine independent inputs and nineteen rows cover both partial warp tiles. */
static int q8_certificate_case(yvex_backend *backend, unsigned long long width)
{
    enum { ROWS = 19, TOKENS = 9, OUTPUTS = ROWS * TOKENS };
    unsigned qtype = YVEX_GGUF_QTYPE_Q8_0, grid, block;
    unsigned long long bytes = width / 32ull * 34ull, rows = ROWS, tokens = TOKENS, zero = 0ull;
    size_t input_offset = (size_t)((bytes * ROWS + 7ull) & ~7ull);
    size_t output_offset = input_offset + (size_t)width * TOKENS * sizeof(float) + sizeof(float);
    size_t status_offset = output_offset + (OUTPUTS + 1u) * sizeof(float), total = status_offset + sizeof(int);
    unsigned char *before = calloc(1u, total), *after = malloc(total);
    float expected[OUTPUTS], published[OUTPUTS], decoded[32], canary = 12345.0f;
    yvex_backend_tensor_desc desc = {.name = "q8-ordered-certificate", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_quant_failure failure;
    yvex_error err;
    int block_row, no = 0, device_wide = 0;
    YVEX_TEST_ASSERT(before && after && width >= 32ull && width % 32ull == 0ull,
        "allocate bounded Q8 certificate oracle");
    float *input = (float *)(before + input_offset);
    for (unsigned token = 0u; token < TOKENS; ++token)
        for (unsigned long long i = 0ull; i < width; ++i)
            input[token * width + i] = token == 0u ? 1.0f : token == 1u ?
                (i == 0ull ? 1.0f : i == 1ull ? 0x1p-24f : 0.0f) : token == 2u ?
                (i == 0ull ? 0x1p-125f : 0.0f) :
                (float)((int)((i * 37ull + token * 13u) % 251ull) - 125) / 127.0f;
    for (unsigned row = 0u; row < ROWS; ++row) {
        double sums[TOKENS] = {0};
        for (unsigned long long first = 0ull; first < width; first += 32ull) {
            unsigned char *encoded = before + row * bytes + first / 32ull * 34ull;
            uint16_t scale = row == 3u ? 0x0001u : row == 4u ? 0x8000u :
                (uint16_t)(0x3400u + ((first / 32ull + row) % 9ull) * 0x400u);
            if (row == 1u || row == 2u) scale = 0x3c00u;
            encoded[0] = (unsigned char)scale;
            encoded[1] = (unsigned char)(scale >> 8u);
            for (unsigned i = 0u; i < 32u; ++i) {
                int value = row == 1u ? (first + i < 2ull ? 1 : 0) : row == 2u ?
                    (i & 1u ? -1 : 1) : row == 3u ? 1 :
                    (int)((first * 17ull + i * 23u + row * 29u) % 256ull) - 128;
                encoded[2u + i] = (unsigned char)value;
            }
            YVEX_TEST_ASSERT(yvex_quant_decode_block(qtype, encoded, 34u, decoded, 32u,
                &failure, &err) == YVEX_OK, "independent canonical Q8 block decode");
            for (unsigned token = 0u; token < TOKENS; ++token)
                for (unsigned i = 0u; i < 32u; ++i)
                    sums[token] = fma((double)decoded[i], (double)input[token * width + first + i], sums[token]);
        }
        for (unsigned token = 0u; token < TOKENS; ++token)
            expected[token * ROWS + row] = (float)sums[token];
    }
    memcpy(before + output_offset - sizeof(float), &canary, sizeof(canary));
    memcpy(before + output_offset + OUTPUTS * sizeof(float), &canary, sizeof(canary));
    desc.bytes = desc.dims[0] = total;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK &&
        yvex_cuda_qtype_matvec_geometry(rows, width, tokens, qtype, 1, 1,
            &grid, &block, &block_row) && !block_row, "admit decoded Q8 partial tiles");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena), absent = 0ull;
    CUdeviceptr device_input = base + input_offset, output = base + output_offset, status = base + status_offset;
    for (int publication = 0; publication < 4; ++publication)
    for (int forensic = 0; forensic <= 1; ++forensic) {
        int bf16 = publication & 1;
        CUdeviceptr additive = publication & 2 ? device_input : absent;
        for (unsigned i = 0u; i < OUTPUTS; ++i) {
            float value = additive ? expected[i] + input[i] : expected[i];
            published[i] = bf16 ? yvex_quant_bf16_decode(yvex_quant_bf16_encode(value)) : value;
        }
        void *params[] = {&base, &bytes, &width, &zero, &rows, &tokens, &qtype,
            &device_input, &width, &no, &block_row, &forensic, &additive, &output,
            &rows, &bf16, &status};
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, before, total, &err) == YVEX_OK &&
            yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                yvex_cuda_state(backend)->qtype_matvec_function, grid, block, 0u, params,
                "cuda.test.q8-certificate", &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                &device_wide, "cuda.test.q8-certificate", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, after, total, &err) == YVEX_OK,
            "execute certified and literal Q8 decoded realizations");
        YVEX_TEST_ASSERT(*(int *)(after + status_offset) == 0 &&
            !memcmp(published, after + output_offset, sizeof(published)),
            "Q8 F32/BF16/additive publication matches literal F64 bitwise");
        YVEX_TEST_ASSERT(!memcmp(before, after, output_offset) &&
            !memcmp(before + output_offset + sizeof(expected), after + output_offset + sizeof(expected),
                total - output_offset - sizeof(expected)), "Q8 certificate preserves inputs and output canaries");
    }
    printf("ordered Q8 certificate: width=%llu rows=%u inputs=%u realizations=2 publications=4 bit_differences=0\n",
        width, ROWS, TOKENS);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release Q8 certificate");
    free(before);
    free(after);
    return 0;
}

static int prepared_case(yvex_backend *backend, unsigned width, unsigned tokens, int scenario)
{
    enum { ROWS = 33 };
    size_t input_offset = ROWS * width * 2u;
    size_t output_offset = input_offset + tokens * width * 4u + 4u;
    size_t status_offset = output_offset + ROWS * tokens * 4u + 4u;
    size_t bytes = status_offset + 4u;
    unsigned char *before = calloc(1u, bytes), *after = malloc(bytes);
    yvex_backend_tensor_desc desc = {.name = "prepared-dot-oracle", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_backend_attention_weight weight = {.present = 1, .qtype = YVEX_GGUF_QTYPE_BF16,
        .row_width = width, .row_bytes = width * 2ull, .row_count = ROWS};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    yvex_error err;
    float canary = 12345.0f;
    int device_wide = 0;
    YVEX_TEST_ASSERT(before && after, "allocate prepared-dot oracle");
    float *input = (float *)(before + input_offset);
    for (unsigned row = 0u; row < ROWS; ++row)
        for (unsigned k = 0u; k < width; ++k) {
            uint16_t value = yvex_quant_bf16_encode(certificate_weight(row, k));
            memcpy(before + (row * width + k) * 2u, &value, 2u);
        }
    for (unsigned t = 0u; t < tokens; ++t)
        for (unsigned k = 0u; k < width; ++k) {
            float value = k < 4u ? 1.0f : (float)((int)((k * 7u + t) % 31u) - 15) / 16.0f;
            if (t % 4u == 1u) value = k % 3u ? 0.0f : -0.0f;
            if (t % 4u == 2u) value = ldexpf(value, -130);
            if (t % 4u == 3u) value = ldexpf(value, (int)(k % 150u) - 75);
            input[t * width + k] = value;
        }
    if (scenario == 1) input[37] = NAN;
    if (scenario == 2) *(int *)(before + status_offset) = 1;
    memcpy(before + output_offset - 4u, &canary, 4u);
    memcpy(before + output_offset + ROWS * tokens * 4u, &canary, 4u);
    desc.bytes = desc.dims[0] = bytes;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate prepared-dot device oracle");
    CUdeviceptr encoded = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr device_input = encoded + input_offset, output = encoded + output_offset;
    CUdeviceptr status = encoded + status_offset;
    for (int bf16 = 0; bf16 < 2; ++bf16) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, before, bytes, &err) == YVEX_OK &&
            yvex_cuda_decoded_rows(&work, &weight, encoded, 0ull, ROWS, tokens,
                device_input, output, bf16, status, &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, work.variant, &device_wide,
                "cuda.test.prepared", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, after, bytes, &err) == YVEX_OK,
            "execute prepared-dot projection and completion");
        int actual_status = *(int *)(after + status_offset);
        YVEX_TEST_ASSERT((actual_status != 0) == (scenario != 0), "prepared-dot finite status");
        if (!scenario) for (unsigned t = 0u; t < tokens; ++t)
            for (unsigned row = 0u; row < ROWS; ++row) {
                double reference = 0.0;
                for (unsigned k = 0u; k < width; ++k) {
                    uint16_t bits;
                    memcpy(&bits, before + (row * width + k) * 2u, 2u);
                    reference = fma((double)yvex_quant_bf16_decode(bits),
                        (double)input[t * width + k], reference);
                }
                float expected = (float)reference;
                if (bf16) expected = yvex_quant_bf16_decode(yvex_quant_bf16_encode(expected));
                YVEX_TEST_ASSERT(!memcmp(&expected, after + output_offset +
                    (t * ROWS + row) * 4u, 4u), "prepared-dot exact ordered host F64 oracle");
            }
        YVEX_TEST_ASSERT(!memcmp(before, after, output_offset) &&
            !memcmp(before + status_offset - 4u, after + status_offset - 4u, 4u),
            "prepared-dot inputs and output canaries unchanged");
        if (scenario == 2) YVEX_TEST_ASSERT(!memcmp(before + output_offset,
            after + output_offset, ROWS * tokens * 4u), "prior status prohibits result writes");
    }
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "prepared-dot temporary ownership released");
    printf("prepared dot oracle: width=%u inputs=%u rows=%u scenario=%d publications=2 bit_differences=0\n",
        width, tokens, ROWS, scenario);
    free(before);
    free(after);
    return 0;
}

static int prepared_mxfp4_case(yvex_backend *backend, unsigned width, unsigned tokens,
                               int scenario, unsigned groups)
{
    enum { ROWS = 17 };
    unsigned total = groups * ROWS;
    size_t row_bytes = width / 32u * 17u;
    size_t input_offset = (total * row_bytes + 3u) & ~(size_t)3u;
    size_t output_offset = input_offset + tokens * groups * width * 4u + 4u;
    size_t status_offset = output_offset + total * tokens * 4u + 4u;
    size_t bytes = status_offset + 4u;
    unsigned char *before = calloc(1u, bytes), *after = malloc(bytes);
    yvex_backend_tensor_desc desc = {.name = "prepared-mxfp4-oracle", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_backend_attention_weight weight = {.present = 1, .qtype = YVEX_GGUF_QTYPE_MXFP4,
        .row_width = width, .row_bytes = row_bytes, .row_count = total};
    yvex_device_tensor *arena = NULL;
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend),
        .variant = YVEX_BACKEND_VARIANT_ATTENTION_ENCODED};
    yvex_error err;
    float canary = 12345.0f;
    int device_wide = 0;
    const unsigned levels[] = {0u, 1u, 2u, 3u, 4u, 6u, 8u, 12u};
    YVEX_TEST_ASSERT(before && after, "allocate prepared-MXFP4 oracle");
    float *input = (float *)(before + input_offset);
    for (unsigned row = 0u; row < total; ++row)
        for (unsigned block = 0u; block < width / 32u; ++block) {
            unsigned char *p = before + row * row_bytes + block * 17u;
            p[0] = block % 8u < 2u ? block % 8u : 112u + (row + block) % 16u;
            for (unsigned i = 1u; i < 17u; ++i) p[i] = (unsigned char)(i * 53u + row * 7u + block);
        }
    for (unsigned t = 0u; t < tokens * groups; ++t)
        for (unsigned k = 0u; k < width; ++k) {
            float value = (float)((int)((k * 7u + t) % 31u) - 15) / 16.0f;
            if (t % 4u == 1u) value = k % 3u ? 0.0f : -0.0f;
            if (t % 4u == 2u) value = ldexpf(value, -130);
            if (t % 4u == 3u) value = ldexpf(value, (int)(k % 64u) - 32);
            input[t * width + k] = value;
        }
    if (scenario == 1) input[37] = NAN;
    if (scenario == 2) *(int *)(before + status_offset) = 1;
    if (scenario == 3) before[0] = 255u;
    memcpy(before + output_offset - 4u, &canary, 4u);
    memcpy(before + status_offset - 4u, &canary, 4u);
    desc.bytes = desc.dims[0] = bytes;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate prepared-MXFP4 device oracle");
    CUdeviceptr encoded = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr device_input = encoded + input_offset, output = encoded + output_offset;
    CUdeviceptr status = encoded + status_offset;
    for (int bf16 = 0; bf16 < 2; ++bf16) {
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, before, bytes, &err) == YVEX_OK &&
            yvex_cuda_decoded_mxfp4(&work, &weight, encoded, groups, ROWS, tokens,
                device_input, output, bf16, status, &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, work.variant, &device_wide,
                "cuda.test.prepared-mxfp4", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, after, bytes, &err) == YVEX_OK,
            "execute prepared-MXFP4 projection and completion");
        YVEX_TEST_ASSERT((*(int *)(after + status_offset) != 0) == (scenario != 0),
            "prepared-MXFP4 finite status");
        if (scenario != 2) {
            int eligibility[128];
            CUdeviceptr flags = work.decoded_workspace +
                (unsigned long long)tokens * groups * width * 25ull / 8ull;
            YVEX_TEST_ASSERT(work.state->driver.cuMemcpyDtoH_v2(eligibility, flags,
                tokens * groups * sizeof(int)) == YVEX_CUDA_SUCCESS,
                "read activation-owned eligibility after checked completion");
            for (unsigned t = 0u; t < tokens * groups; ++t)
                YVEX_TEST_ASSERT(eligibility[t] ==
                    (t % 4u != 3u && !(scenario == 1 && t == 0u)),
                    "unsupported spans and nonfinite inputs do not inherit stale eligibility");
        }
        if (!scenario) for (unsigned t = 0u; t < tokens; ++t)
            for (unsigned row = 0u; row < total; ++row) {
                double reference = 0.0;
                for (unsigned k = 0u; k < width; ++k) {
                    const unsigned char *p = before + row * row_bytes + k / 32u * 17u;
                    unsigned code = k % 32u < 16u ? p[1u + k % 16u] & 15u : p[1u + k % 16u] >> 4u;
                    float scale = ldexpf(1.0f, p[0] ? (int)p[0] - 127 : -127);
                    float value = (float)levels[code & 7u] * scale * 0.5f;
                    if (code & 8u) value = -value;
                    reference = fma((double)value,
                        (double)input[(t * groups + row / ROWS) * width + k], reference);
                }
                float expected = (float)reference;
                if (bf16) expected = yvex_quant_bf16_decode(yvex_quant_bf16_encode(expected));
                YVEX_TEST_ASSERT(!memcmp(&expected, after + output_offset +
                    (t * total + row) * 4u, 4u), "prepared-MXFP4 exact ordered host F64 oracle");
            }
        YVEX_TEST_ASSERT(!memcmp(before, after, output_offset) &&
            !memcmp(before + status_offset - 4u, after + status_offset - 4u, 4u),
            "prepared-MXFP4 inputs and canaries unchanged");
        if (scenario == 2) YVEX_TEST_ASSERT(!memcmp(before + output_offset,
            after + output_offset, total * tokens * 4u), "prior status prohibits MXFP4 writes");
    }
    weight.row_count--;
    YVEX_TEST_ASSERT(yvex_cuda_decoded_mxfp4(&work, &weight, encoded, groups, ROWS, tokens,
        device_input, output, 0, status, &err) == YVEX_ERR_INVALID_ARG,
        "prepared-MXFP4 malformed group extent refused");
    YVEX_TEST_ASSERT(yvex_cuda_work_cleanup(&work, &err) == YVEX_OK &&
        yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK,
        "prepared-MXFP4 temporary ownership released");
    printf("prepared MXFP4 oracle: width=%u inputs=%u groups=%u rows=17 scenario=%d bit_differences=0\n",
        width, tokens, groups, scenario);
    free(before);
    free(after);
    return 0;
}

static int prepared_workspace_refusal(yvex_backend *backend)
{
    yvex_cuda_work work = {.backend = backend, .state = yvex_cuda_state(backend)};
    yvex_backend_attention_weight weight = {.present = 1,
        .qtype = YVEX_GGUF_QTYPE_BF16, .row_width = 4096ull,
        .row_count = 1024ull, .row_bytes = 8192ull};
    yvex_error err;
    /* Opaque addresses must not be touched: this input passes shape checks,
     * but its 13 MiB decoded weights exceed the 4 MiB small-row envelope. */
    YVEX_TEST_ASSERT(yvex_cuda_decoded_rows(&work, &weight, 1ull, 0ull,
        weight.row_count, 4ull, 1ull, 1ull, 0, 1ull, &err) == YVEX_ERR_BOUNDS &&
        work.count == 0u && work.launches == 0ull && work.decoded_capacity == 0ull,
        "oversized decoded layout refuses before allocating or launching");
    return 0;
}

/* Exhaust the complete finite F16 population through the production gather,
 * not a host/device helper agreement. Exceptional rows must publish nothing. */
static int f16_codec_case(yvex_backend *backend)
{
    enum { VALUES = 65536, FINITE = 63488, NONFINITE = 2048 };
    size_t ids_offset = VALUES * 2u, output_offset = ids_offset + FINITE * sizeof(unsigned) + sizeof(float);
    size_t status_offset = output_offset + (FINITE + 1u) * sizeof(float), total = status_offset + sizeof(int);
    unsigned char *before = calloc(1u, total), *after = malloc(total);
    float *expected = malloc(FINITE * sizeof(float)), canary = 12345.0f;
    yvex_backend_tensor_desc desc = {.name = "f16-complete-codec", .dtype = YVEX_DTYPE_I8, .rank = 1u};
    yvex_device_tensor *arena = NULL;
    yvex_error err;
    unsigned qtype = YVEX_GGUF_QTYPE_F16;
    unsigned long long row_bytes = 2ull, width = 1ull, rows = VALUES, selected;
    int device_wide = 0;
    YVEX_TEST_ASSERT(before && after && expected, "allocate complete F16 conversion oracle");
    unsigned *ids = (unsigned *)(before + ids_offset);
    for (unsigned i = 0u; i < VALUES; ++i) {
        before[i * 2u] = (unsigned char)i;
        before[i * 2u + 1u] = (unsigned char)(i >> 8u);
    }
    memcpy(before + output_offset - sizeof(float), &canary, sizeof(canary));
    memcpy(before + output_offset + FINITE * sizeof(float), &canary, sizeof(canary));
    desc.bytes = desc.dims[0] = total;
    YVEX_TEST_ASSERT(yvex_backend_tensor_alloc(backend, &desc, &arena, &err) == YVEX_OK,
        "allocate exhaustive F16 device fixture");
    CUdeviceptr base = yvex_cuda_activation_pointer(backend, arena);
    CUdeviceptr indices = base + ids_offset, output = base + output_offset, status = base + status_offset;
    for (unsigned scenario = 0u; scenario < 3u; ++scenario) {
        unsigned n = 0u;
        for (unsigned i = 0u; i < VALUES; ++i)
            if (((i & 0x7c00u) != 0x7c00u) == (scenario != 1u)) {
                ids[n] = i;
                if (scenario != 1u) expected[n] = yvex_quant_f16_decode((unsigned short)i);
                ++n;
            }
        YVEX_TEST_ASSERT(n == (scenario == 1u ? NONFINITE : FINITE), "exact finite/exceptional F16 population");
        selected = n;
        for (unsigned i = 0u; i < FINITE; ++i)
            memcpy(before + output_offset + i * sizeof(float), &canary, sizeof(canary));
        *(int *)(before + status_offset) = scenario == 2u;
        void *params[] = {&base, &row_bytes, &width, &rows, &indices, &selected, &qtype, &output, &status};
        YVEX_TEST_ASSERT(yvex_backend_tensor_write(backend, arena, before, total, &err) == YVEX_OK &&
            yvex_cuda_launch(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                yvex_cuda_state(backend)->qtype_gather_function, (n + 255u) / 256u, 256u, 0u,
                params, "cuda.test.f16-codec", &err) == YVEX_OK &&
            yvex_cuda_launch_synchronize(backend, YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                &device_wide, "cuda.test.f16-codec", &err) == YVEX_OK &&
            yvex_backend_tensor_read(backend, arena, after, total, &err) == YVEX_OK,
            "execute complete F16 conversion/refusal control");
        YVEX_TEST_ASSERT(*(int *)(after + status_offset) == (scenario != 0u) &&
            !memcmp(before, after, output_offset) &&
            !memcmp(before + output_offset + FINITE * sizeof(float),
                after + output_offset + FINITE * sizeof(float), sizeof(float)),
            "F16 status, immutable inputs and output canaries are preserved");
        YVEX_TEST_ASSERT(!memcmp(scenario ? before + output_offset : (unsigned char *)expected,
            after + output_offset, FINITE * sizeof(float)),
            "all finite F16 bits match CPU conversion; exceptional/prior refusal publishes nothing");
    }
    printf("F16 conversion: finite=%u exceptional=%u bit_differences=0 refused_output_writes=0\n", FINITE, NONFINITE);
    YVEX_TEST_ASSERT(yvex_backend_tensor_release(backend, &arena, &err) == YVEX_OK, "release complete F16 fixture");
    free(expected);
    free(before);
    free(after);
    return 0;
}

int yvex_cuda_test_dot_finiteness(void)
{
    const unsigned int qtypes[] = {YVEX_GGUF_QTYPE_F32, YVEX_GGUF_QTYPE_F16,
        YVEX_GGUF_QTYPE_BF16, YVEX_GGUF_QTYPE_MXFP4, YVEX_GGUF_QTYPE_Q8_0};
    yvex_backend_options options = {.kind = YVEX_BACKEND_KIND_CUDA};
    yvex_backend *backend = NULL;
    yvex_error err;
    int rc = yvex_backend_open(&backend, &options, &err);
    if (rc == YVEX_ERR_UNSUPPORTED) return 77;
    YVEX_TEST_ASSERT(rc == YVEX_OK, "open dot CUDA backend");
    if (f16_codec_case(backend)) return 1;
    if (prepared_workspace_refusal(backend)) return 1;
    for (unsigned int q = 0u; q < sizeof(qtypes) / sizeof(qtypes[0]); ++q)
        for (unsigned int mode = 0u; mode < 3u; ++mode)
            for (unsigned int scenario = 0u; scenario < 6u; ++scenario)
                if (dot_case(backend, qtypes[q], scenario, mode)) return 1;
    for (unsigned int scenario = 0u; scenario < 8u; ++scenario)
        if (dot_case(backend, YVEX_GGUF_QTYPE_BF16, scenario, 3u)) return 1;
    const unsigned long long widths[] = {33ull, 4096ull, 16384ull};
    for (unsigned int i = 0u; i < sizeof(widths) / sizeof(widths[0]); ++i)
        if (certificate_case(backend, YVEX_GGUF_QTYPE_F32, widths[i], 0) ||
            certificate_case(backend, YVEX_GGUF_QTYPE_BF16, widths[i], 0)) return 1;
    const unsigned long long prefix_widths[] = {4096ull, 16384ull, 32768ull, 32769ull};
    for (unsigned i = 0u; i < sizeof(prefix_widths) / sizeof(prefix_widths[0]); ++i)
        if (certificate_case(backend, YVEX_GGUF_QTYPE_F32, prefix_widths[i], 1)) return 1;
    if (q8_certificate_case(backend, 128ull) || q8_certificate_case(backend, 12288ull)) return 1;
    for (int scenario = 0; scenario < 3; ++scenario)
        if (prepared_case(backend, 96u, 65u, scenario)) return 1;
    if (prepared_case(backend, 4096u, 33u, 0)) return 1;
    for (int scenario = 0; scenario < 4; ++scenario)
        if (prepared_mxfp4_case(backend, 96u, 5u, scenario, 2u)) return 1;
    if (prepared_mxfp4_case(backend, 4096u, 1u, 0, 2u) ||
        prepared_mxfp4_case(backend, 8192u, 16u, 0, 2u) ||
        prepared_mxfp4_case(backend, 8192u, 16u, 0, 8u)) return 1;
    yvex_backend_close(backend);
    return 0;
}
