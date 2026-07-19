/*
 * deepseek_attention_reference.c - independent DeepSeek attention test oracle.
 *
 * Owner:
 *   tests/reference
 *
 * Owns:
 *   direct scalar equations used to judge production DeepSeek attention
 *   numeric behavior.
 *
 * Does not own:
 *   production graph behavior, runtime admission, source or artifact IO
 *   policy, CUDA capability, persistent KV, generation, or project claims.
 *
 * Invariants:
 *   this translation unit does not include the production attention-private
 *   header and does not call production attention numeric algorithms.
 *
 * Boundary:
 *   independent scalar comparison is test evidence, not runtime support.
 */
#include "tests/reference/deepseek_attention.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yvex/qtype.h>

#define REF_PI 3.14159265358979323846264338327950288

typedef struct {
    float score;
    unsigned long long position;
    unsigned long long index;
} ref_ranked_candidate;

static int ref_mul(unsigned long long a,
                   unsigned long long b,
                   unsigned long long *out)
{
    if (!out || (a && b > ULLONG_MAX / a)) return 0;
    *out = a * b;
    return 1;
}

static void *ref_alloc(unsigned long long count, size_t width)
{
    if (!count || !width || count > SIZE_MAX / width) return NULL;
    return calloc((size_t)count, width);
}

static void ref_reason(char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP],
                       const char *message)
{
    if (!reason) return;
    (void)snprintf(reason, YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP, "%s",
                   message ? message : "reference failure");
}

static unsigned short ref_u16(const unsigned char *bytes)
{
    return (unsigned short)((unsigned short)bytes[0] |
                            ((unsigned short)bytes[1] << 8));
}

static unsigned int ref_u32(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
           ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) |
           ((unsigned int)bytes[3] << 24);
}

/* Contract: independently decodes one IEEE binary16 bit pattern. */
static float ref_f16(unsigned short bits)
{
    unsigned int sign = (unsigned int)(bits >> 15);
    unsigned int exponent = ((unsigned int)bits >> 10) & 0x1fu;
    unsigned int fraction = (unsigned int)bits & 0x3ffu;
    double magnitude;

    if (exponent == 0u) {
        magnitude = fraction
            ? ldexp((double)fraction, -24)
            : 0.0;
    } else if (exponent == 0x1fu) {
        if (fraction) return NAN;
        return sign ? -INFINITY : INFINITY;
    } else {
        magnitude = ldexp(1.0 + (double)fraction / 1024.0,
                          (int)exponent - 15);
    }
    return (float)(sign ? -magnitude : magnitude);
}

static float ref_bf16(unsigned short bits)
{
    uint32_t value = (uint32_t)bits << 16;
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

static const yvex_runtime_tensor_binding *ref_binding(
    const yvex_runtime_descriptor *descriptor,
    yvex_tensor_role role,
    unsigned long long layer)
{
    return yvex_runtime_descriptor_find_role(
        descriptor, role, YVEX_TENSOR_SCOPE_MAIN_LAYER, layer,
        YVEX_DEEPSEEK_GGUF_NO_INDEX);
}

static int ref_row_bytes(const yvex_materialized_tensor_binding *binding,
                         unsigned long long *out)
{
    unsigned long long blocks;

    if (!binding || !out || !binding->row_width || !binding->block_size ||
        !binding->bytes_per_block ||
        binding->row_width % binding->block_size != 0ull)
        return 0;
    blocks = binding->row_width / binding->block_size;
    return ref_mul(blocks, binding->bytes_per_block, out) && *out > 0ull;
}

/* Contract: independently decodes one admitted attention block. */
static int ref_decode_block(unsigned int qtype,
                            const unsigned char *encoded,
                            unsigned long long bytes,
                            float *out,
                            unsigned long long elements)
{
    unsigned long long lane;

    if (!encoded || !out) return 0;
    if (qtype == YVEX_GGUF_QTYPE_F32 && bytes == 4ull && elements == 1ull) {
        uint32_t bits = ref_u32(encoded);
        memcpy(out, &bits, sizeof(*out));
        return isfinite(*out);
    }
    if (qtype == YVEX_GGUF_QTYPE_F16 && bytes == 2ull && elements == 1ull) {
        *out = ref_f16(ref_u16(encoded));
        return isfinite(*out);
    }
    if (qtype == YVEX_GGUF_QTYPE_BF16 && bytes == 2ull && elements == 1ull) {
        *out = ref_bf16(ref_u16(encoded));
        return isfinite(*out);
    }
    if (qtype == YVEX_GGUF_QTYPE_Q8_0 && bytes == 34ull &&
        elements == 32ull) {
        float scale = ref_f16(ref_u16(encoded));
        if (!isfinite(scale) || scale < 0.0f) return 0;
        for (lane = 0ull; lane < 32ull; ++lane) {
            int value = encoded[2ull + lane] <= 127u
                ? (int)encoded[2ull + lane]
                : (int)encoded[2ull + lane] - 256;
            out[lane] = scale * (float)value;
        }
        return 1;
    }
    return 0;
}

/* Contract: reads and independently decodes one complete materialized row. */
static int ref_decode_row(yvex_materialization_session *session,
                          const yvex_runtime_tensor_binding *runtime,
                          unsigned long long row,
                          float *out,
                          char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    const yvex_materialized_tensor_binding *binding;
    unsigned long long row_bytes;
    unsigned long long block;
    unsigned long long blocks;
    unsigned char *encoded;
    yvex_error error;

    if (!session || !runtime || !(binding = runtime->binding) || !out ||
        row >= binding->row_count || !ref_row_bytes(binding, &row_bytes) ||
        row_bytes > SIZE_MAX) {
        ref_reason(reason, "reference row geometry is invalid");
        return 0;
    }
    encoded = (unsigned char *)malloc((size_t)row_bytes);
    if (!encoded) {
        ref_reason(reason, "reference row allocation failed");
        return 0;
    }
    yvex_error_clear(&error);
    if (yvex_materialization_session_read(
            session, binding, row * row_bytes, encoded, (size_t)row_bytes,
            NULL, &error) != YVEX_OK) {
        free(encoded);
        ref_reason(reason, "reference materialized row read failed");
        return 0;
    }
    blocks = binding->row_width / binding->block_size;
    for (block = 0ull; block < blocks; ++block) {
        if (!ref_decode_block(
                binding->qtype,
                encoded + block * binding->bytes_per_block,
                binding->bytes_per_block,
                out + block * binding->block_size,
                binding->block_size)) {
            free(encoded);
            ref_reason(reason, "reference qtype decode failed");
            return 0;
        }
    }
    free(encoded);
    return 1;
}

/* Contract: independently evaluates a matrix row range for a token batch. */
static int ref_matrix_batch(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *runtime,
    unsigned long long row_start,
    unsigned long long rows,
    const float *input,
    unsigned long long token_count,
    unsigned long long input_stride,
    float *output,
    unsigned long long output_stride,
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    const yvex_materialized_tensor_binding *binding;
    float *decoded;
    unsigned long long row;

    if (!session || !runtime || !(binding = runtime->binding) || !input ||
        !output || !token_count || input_stride < binding->row_width ||
        output_stride < rows || row_start > binding->row_count ||
        rows > binding->row_count - row_start) {
        ref_reason(reason, "reference matrix geometry is invalid");
        return 0;
    }
    decoded = (float *)ref_alloc(binding->row_width, sizeof(*decoded));
    if (!decoded) {
        ref_reason(reason, "reference matrix scratch allocation failed");
        return 0;
    }
    for (row = 0ull; row < rows; ++row) {
        unsigned long long token;
        if (!ref_decode_row(session, runtime, row_start + row, decoded,
                            reason)) {
            free(decoded);
            return 0;
        }
        for (token = 0ull; token < token_count; ++token) {
            unsigned long long lane;
            double sum = 0.0;
            const float *vector = input + token * input_stride;
            for (lane = 0ull; lane < binding->row_width; ++lane)
                sum += (double)decoded[lane] * (double)vector[lane];
            output[token * output_stride + row] = (float)sum;
            if (!isfinite(output[token * output_stride + row])) {
                free(decoded);
                ref_reason(reason, "reference matrix result is non-finite");
                return 0;
            }
        }
    }
    free(decoded);
    return 1;
}

static int ref_rms(float *values,
                   unsigned long long count,
                   const float *weights,
                   double epsilon)
{
    unsigned long long lane;
    double squares = 0.0;
    double inverse;

    if (!values || !count || !(epsilon > 0.0) || !isfinite(epsilon))
        return 0;
    for (lane = 0ull; lane < count; ++lane) {
        if (!isfinite(values[lane]) ||
            (weights && !isfinite(weights[lane])))
            return 0;
        squares += (double)values[lane] * (double)values[lane];
    }
    inverse = 1.0 / sqrt(squares / (double)count + epsilon);
    if (!isfinite(inverse)) return 0;
    for (lane = 0ull; lane < count; ++lane)
        values[lane] = (float)((double)values[lane] * inverse *
                               (weights ? (double)weights[lane] : 1.0));
    return 1;
}

static double ref_yarn_frequency(
    const yvex_attention_position_policy *position,
    unsigned long long pair,
    unsigned long long rope_dimensions)
{
    double base;

    if (!position || rope_dimensions < 2ull || position->theta <= 1ull)
        return 0.0;
    base = 1.0 / pow((double)position->theta,
                     (double)(pair * 2ull) /
                         (double)rope_dimensions);
    if (position->original_context && position->scaling_factor) {
        double denominator = 2.0 * log((double)position->theta);
        double lower = floor(
            (double)rope_dimensions *
            log((double)position->original_context /
                ((double)position->beta_fast * 2.0 * REF_PI)) /
            denominator);
        double upper = ceil(
            (double)rope_dimensions *
            log((double)position->original_context /
                ((double)position->beta_slow * 2.0 * REF_PI)) /
            denominator);
        double ramp;
        double smooth;
        if (lower < 0.0) lower = 0.0;
        if (upper > (double)rope_dimensions - 1.0)
            upper = (double)rope_dimensions - 1.0;
        if (lower == upper) upper += 0.001;
        ramp = ((double)pair - lower) / (upper - lower);
        if (ramp < 0.0) ramp = 0.0;
        if (ramp > 1.0) ramp = 1.0;
        smooth = 1.0 - ramp;
        base = base / (double)position->scaling_factor * (1.0 - smooth) +
               base * smooth;
    }
    return base;
}

static int ref_rope(float *values,
                    unsigned long long width,
                    unsigned long long rope_dimensions,
                    unsigned long long position_index,
                    const yvex_attention_position_policy *position,
                    int inverse)
{
    unsigned long long lane;
    unsigned long long start;

    if (!values || rope_dimensions < 2ull || rope_dimensions > width)
        return 0;
    if (rope_dimensions & 1ull) rope_dimensions--;
    start = width - rope_dimensions;
    for (lane = 0ull; lane < rope_dimensions; lane += 2ull) {
        double angle = (double)position_index *
                       ref_yarn_frequency(position, lane / 2ull,
                                          rope_dimensions);
        double sine = sin(angle);
        double cosine = cos(angle);
        double first = values[start + lane];
        double second = values[start + lane + 1ull];
        if (inverse) sine = -sine;
        values[start + lane] = (float)(first * cosine - second * sine);
        values[start + lane + 1ull] =
            (float)(first * sine + second * cosine);
        if (!isfinite(values[start + lane]) ||
            !isfinite(values[start + lane + 1ull]))
            return 0;
    }
    return 1;
}

static float ref_power_two_ceil(float value)
{
    int exponent;
    float fraction;

    if (!(value > 0.0f) || !isfinite(value)) return 0.0f;
    fraction = frexpf(value, &exponent);
    if (fraction > 0.5f) exponent++;
    return ldexpf(1.0f, exponent - 1);
}

static unsigned char ref_ue8m0_encode(float scale)
{
    int exponent;
    float fraction;

    if (!(scale > 0.0f) || !isfinite(scale)) return 0u;
    fraction = frexpf(scale, &exponent);
    if (fraction > 0.5f) exponent++;
    exponent += 126;
    if (exponent < 0) return 0u;
    if (exponent > 255) return 255u;
    return (unsigned char)exponent;
}

static float ref_ue8m0_decode(unsigned char code)
{
    return code ? ldexpf(1.0f, (int)code - 127) : 0.0f;
}

static float ref_fp8_decode(unsigned char code)
{
    unsigned int negative = code & 0x80u;
    unsigned int exponent = (code >> 3u) & 0x0fu;
    unsigned int fraction = code & 0x07u;
    float magnitude;

    if ((code & 0x7fu) == 0u) return negative ? -0.0f : 0.0f;
    magnitude = exponent
        ? ldexpf(1.0f + (float)fraction / 8.0f, (int)exponent - 7)
        : ldexpf((float)fraction / 8.0f, -6);
    if (magnitude > 448.0f) magnitude = 448.0f;
    return negative ? -magnitude : magnitude;
}

static unsigned char ref_fp8_encode(float value)
{
    unsigned int code;
    unsigned char best = 0u;
    float best_error = INFINITY;

    if (value > 448.0f) value = 448.0f;
    if (value < -448.0f) value = -448.0f;
    for (code = 0u; code < 256u; ++code) {
        float error = fabsf(ref_fp8_decode((unsigned char)code) - value);
        if (error < best_error) {
            best_error = error;
            best = (unsigned char)code;
        }
    }
    return best;
}

static float ref_fp4_decode(unsigned char code)
{
    static const float values[8] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f
    };
    float value = values[code & 7u];
    return (code & 8u) ? -value : value;
}

static unsigned char ref_fp4_encode(float value)
{
    static const float values[8] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f
    };
    unsigned int code;
    unsigned int best = 0u;
    float magnitude = fabsf(value);
    float error = fabsf(magnitude - values[0]);

    for (code = 1u; code < 8u; ++code) {
        float next = fabsf(magnitude - values[code]);
        if (next < error) {
            best = code;
            error = next;
        }
    }
    if (best && signbit(value)) best |= 8u;
    return (unsigned char)best;
}

static int ref_fake_quant_block(float *values,
                                unsigned long long count,
                                int fp4)
{
    unsigned long long lane;
    float maximum = fp4 ? 0.0f : 1.0e-4f;
    float divisor = fp4 ? 6.0f : 448.0f;
    float scale;
    unsigned char scale_code;

    for (lane = 0ull; lane < count; ++lane) {
        float magnitude;
        if (!isfinite(values[lane])) return 0;
        magnitude = fabsf(values[lane]);
        if (magnitude > maximum) maximum = magnitude;
    }
    if (fp4 && maximum < 6.0f * ldexpf(1.0f, -126))
        maximum = 6.0f * ldexpf(1.0f, -126);
    scale_code = ref_ue8m0_encode(ref_power_two_ceil(maximum / divisor));
    scale = ref_ue8m0_decode(scale_code);
    if (!(scale > 0.0f) || !isfinite(scale)) return 0;
    for (lane = 0ull; lane < count; ++lane) {
        unsigned char code = fp4
            ? ref_fp4_encode(values[lane] / scale)
            : ref_fp8_encode(values[lane] / scale);
        values[lane] = (fp4 ? ref_fp4_decode(code) : ref_fp8_decode(code)) *
                       scale;
    }
    return 1;
}

static int ref_activation(
    const yvex_attention_activation_policy *policy,
    float *values,
    unsigned long long count)
{
    unsigned long long offset;
    float *rotated = NULL;

    if (!policy || !policy->required) return 1;
    if (!values || !count || !policy->block_width ||
        count % policy->block_width != 0ull)
        return 0;
    if (policy->pre_transform ==
        YVEX_ATTENTION_TRANSFORM_DAO_FHT_V1_1_0_POST2) {
        rotated = (float *)ref_alloc(count, sizeof(*rotated));
        if (!rotated ||
            !yvex_test_attention_reference_hadamard(
                values, count, 1.0f / sqrtf((float)count), 1, rotated)) {
            free(rotated);
            return 0;
        }
        memcpy(values, rotated, (size_t)count * sizeof(*values));
        free(rotated);
    } else if (policy->pre_transform !=
               YVEX_ATTENTION_TRANSFORM_NONE) {
        return 0;
    }
    for (offset = 0ull; offset < count; offset += policy->block_width) {
        int fp4 = policy->quantization ==
            YVEX_ATTENTION_QUANT_FP4_E2M1_UE8M0_FAKE_DEQUANT;
        if (!fp4 && policy->quantization !=
            YVEX_ATTENTION_QUANT_FP8_E4M3_UE8M0_FAKE_DEQUANT)
            return 0;
        if (!ref_fake_quant_block(values + offset, policy->block_width,
                                  fp4))
            return 0;
    }
    return 1;
}

/* Contract: computes one zero-padded Walsh matrix product directly. */
int yvex_test_attention_reference_hadamard(const float *input,
                                           unsigned long long length,
                                           float scale,
                                           int reject_nonfinite,
                                           float *output)
{
    unsigned long long padded = 1ull;
    unsigned long long row;

    if (!input || !output || !length || !isfinite(scale)) return 0;
    while (padded < length) {
        if (padded > ULLONG_MAX / 2ull) return 0;
        padded *= 2ull;
    }
    for (row = 0ull; row < length; ++row) {
        unsigned long long column;
        double sum = 0.0;
        for (column = 0ull; column < length; ++column) {
            unsigned long long parity = row & column;
            unsigned int odd = 0u;
            if (reject_nonfinite && !isfinite(input[column])) return 0;
            while (parity) {
                odd ^= (unsigned int)(parity & 1ull);
                parity >>= 1u;
            }
            sum += odd ? -(double)input[column] : (double)input[column];
        }
        output[row] = (float)(sum * (double)scale);
        if (!isfinite(output[row])) return 0;
    }
    return 1;
}

typedef struct {
    unsigned long long ratio;
    unsigned long long head_dimension;
    unsigned long long width;
    unsigned long long slots;
    unsigned long long next_position;
    unsigned long long cursor;
    unsigned long long previous_fill;
    unsigned long long current_fill;
    int overlap;
    float *kv;
    float *score;
} ref_rolling_state;

static void ref_rolling_release(ref_rolling_state *state)
{
    if (!state) return;
    free(state->kv);
    free(state->score);
    memset(state, 0, sizeof(*state));
}

/* Contract: creates an independently owned rolling state from one immutable
 * runtime view or the canonical empty state. */
static int ref_rolling_init(
    ref_rolling_state *state,
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    const yvex_attention_rolling_state_view *view,
    unsigned long long token_position)
{
    unsigned long long extent;
    unsigned long long slot;

    if (!state || !layer || !layer->compression_ratio) return 0;
    memset(state, 0, sizeof(*state));
    state->ratio = layer->compression_ratio;
    state->head_dimension =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? layer->indexer_head_dimension
            : layer->head_dimension;
    state->overlap =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER ||
        layer->attention_class == YVEX_ATTENTION_CLASS_CSA;
    state->width = state->head_dimension * (state->overlap ? 2ull : 1ull);
    state->slots = state->ratio * (state->overlap ? 2ull : 1ull);
    if (!state->head_dimension || !state->width || !state->slots ||
        !ref_mul(state->slots, state->width, &extent))
        return 0;
    state->kv = (float *)ref_alloc(extent, sizeof(*state->kv));
    state->score = (float *)ref_alloc(extent, sizeof(*state->score));
    if (!state->kv || !state->score) {
        ref_rolling_release(state);
        return 0;
    }
    for (slot = 0ull; slot < extent; ++slot) state->score[slot] = -INFINITY;
    state->next_position = token_position;
    state->cursor = token_position % state->ratio;
    if (view && view->present) {
        if (view->ratio != state->ratio ||
            view->head_dimension != state->head_dimension ||
            view->state_width != state->width ||
            view->state_slots != state->slots ||
            view->kv_state_stride < state->width ||
            view->score_state_stride < state->width ||
            view->next_token_position != token_position ||
            !view->kv_state || !view->score_state)
            goto fail;
        state->cursor = view->cursor;
        state->previous_fill = view->previous_fill;
        state->current_fill = view->current_fill;
        for (slot = 0ull; slot < state->slots; ++slot) {
            memcpy(state->kv + slot * state->width,
                   view->kv_state + slot * view->kv_state_stride,
                   (size_t)state->width * sizeof(*state->kv));
            memcpy(state->score + slot * state->width,
                   view->score_state + slot * view->score_state_stride,
                   (size_t)state->width * sizeof(*state->score));
        }
    }
    return 1;

fail:
    ref_rolling_release(state);
    return 0;
}

/* Contract: independently applies one rolling compressor transition and
 * emits only after a complete ratio-sized group. */
static int ref_rolling_step(ref_rolling_state *state,
                            const float *token_kv,
                            const float *token_score,
                            const float *ape,
                            float *emission,
                            int *emitted)
{
    unsigned long long slot;
    unsigned long long lane;

    if (emitted) *emitted = 0;
    if (!state || !token_kv || !token_score || !ape || !emission ||
        !emitted || state->cursor != state->next_position % state->ratio)
        return 0;
    slot = state->overlap ? state->ratio + state->cursor : state->cursor;
    for (lane = 0ull; lane < state->width; ++lane) {
        float value = token_kv[lane];
        float score = token_score[lane] + ape[lane];
        if (!isfinite(value) || !isfinite(score)) return 0;
        state->kv[slot * state->width + lane] = value;
        state->score[slot * state->width + lane] = score;
    }
    if (state->current_fill < state->cursor + 1ull)
        state->current_fill = state->cursor + 1ull;
    state->next_position++;
    state->cursor = (state->cursor + 1ull) % state->ratio;
    if (state->cursor != 0ull) return 1;

    for (lane = 0ull; lane < state->head_dimension; ++lane) {
        double maximum = -HUGE_VAL;
        double denominator = 0.0;
        double sum = 0.0;
        for (slot = 0ull; slot < state->ratio; ++slot) {
            double first = state->score[slot * state->width + lane];
            if (first > maximum) maximum = first;
            if (state->overlap) {
                double second = state->score[
                    (state->ratio + slot) * state->width +
                    lane + state->head_dimension];
                if (second > maximum) maximum = second;
            }
        }
        for (slot = 0ull; slot < state->ratio; ++slot) {
            double weight = exp(state->score[slot * state->width + lane] -
                                maximum);
            denominator += weight;
            sum += weight * state->kv[slot * state->width + lane];
            if (state->overlap) {
                unsigned long long offset =
                    (state->ratio + slot) * state->width +
                    lane + state->head_dimension;
                weight = exp(state->score[offset] - maximum);
                denominator += weight;
                sum += weight * state->kv[offset];
            }
        }
        if (!(denominator > 0.0) || !isfinite(denominator) || !isfinite(sum))
            return 0;
        emission[lane] = (float)(sum / denominator);
    }
    if (state->overlap) {
        for (slot = 0ull; slot < state->ratio; ++slot) {
            memcpy(state->kv + slot * state->width,
                   state->kv + (state->ratio + slot) * state->width,
                   (size_t)state->width * sizeof(*state->kv));
            memcpy(state->score + slot * state->width,
                   state->score + (state->ratio + slot) * state->width,
                   (size_t)state->width * sizeof(*state->score));
        }
        state->previous_fill = state->ratio;
    } else {
        state->previous_fill = 0ull;
    }
    state->current_fill = 0ull;
    *emitted = 1;
    return 1;
}

/* Contract: transfers one independently computed rolling state into an owned
 * public trace value without retaining the caller's history view. */
static int ref_rolling_export(
    ref_rolling_state *state,
    const yvex_attention_layer_plan *layer,
    yvex_attention_rolling_kind kind,
    const yvex_attention_rolling_state_view *before,
    yvex_attention_rolling_state_output *out)
{
    unsigned long long extent;

    if (!state || !layer || !out || !state->kv || !state->score ||
        !ref_mul(state->slots, state->width, &extent))
        return 0;
    memset(out, 0, sizeof(*out));
    out->present = 1;
    out->schema_version = YVEX_DEEPSEEK_ATTENTION_ROLLING_STATE_SCHEMA_V1;
    out->kind = kind;
    out->attention_class = layer->attention_class;
    out->layer_index = layer->layer_index;
    out->next_token_position = state->next_position;
    out->ratio = state->ratio;
    out->head_dimension = state->head_dimension;
    out->state_width = state->width;
    out->state_slots = state->slots;
    out->previous_fill = state->previous_fill;
    out->current_fill = state->current_fill;
    out->cursor = state->cursor;
    out->kv_state_stride = state->width;
    out->score_state_stride = state->width;
    out->kv_state_extent = extent;
    out->score_state_extent = extent;
    out->kv_state = state->kv;
    out->score_state = state->score;
    out->overlap = state->overlap;
    out->rotated = kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER;
    if (before)
        memcpy(out->attention_plan_identity,
               before->attention_plan_identity,
               sizeof(out->attention_plan_identity));
    state->kv = NULL;
    state->score = NULL;
    return 1;
}

static int ref_rank_compare(const void *left, const void *right)
{
    const ref_ranked_candidate *a = (const ref_ranked_candidate *)left;
    const ref_ranked_candidate *b = (const ref_ranked_candidate *)right;

    if (a->score > b->score) return -1;
    if (a->score < b->score) return 1;
    if (a->position < b->position) return -1;
    if (a->position > b->position) return 1;
    return 0;
}

static const float *ref_segmented_row(const float *history,
                                      unsigned long long history_count,
                                      unsigned long long history_stride,
                                      const float *current,
                                      unsigned long long current_count,
                                      unsigned long long current_stride,
                                      unsigned long long index)
{
    if (index < history_count)
        return history ? history + index * history_stride : NULL;
    index -= history_count;
    if (index >= current_count) return NULL;
    return current ? current + index * current_stride : NULL;
}

static unsigned long long ref_segmented_position(
    const unsigned long long *history,
    unsigned long long history_count,
    const unsigned long long *current,
    unsigned long long current_count,
    unsigned long long index)
{
    if (index < history_count) return history ? history[index] : ULLONG_MAX;
    index -= history_count;
    if (index >= current_count) return ULLONG_MAX;
    return current ? current[index] : ULLONG_MAX;
}

static int ref_decode_flat(yvex_materialization_session *session,
                           const yvex_runtime_tensor_binding *runtime,
                           float *out,
                           unsigned long long elements,
                           char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    unsigned long long total;
    unsigned long long row;

    if (!runtime || !runtime->binding ||
        !ref_mul(runtime->binding->row_width, runtime->binding->row_count,
                 &total) || total != elements)
        return 0;
    for (row = 0ull; row < runtime->binding->row_count; ++row) {
        if (!ref_decode_row(session, runtime, row,
                            out + row * runtime->binding->row_width,
                            reason))
            return 0;
    }
    return 1;
}

static unsigned long long ref_emission_count(unsigned long long start,
                                             unsigned long long count,
                                             unsigned long long ratio)
{
    unsigned long long token;
    unsigned long long emitted = 0ull;

    if (!ratio) return ULLONG_MAX;
    for (token = 0ull; token < count; ++token) {
        if (start > ULLONG_MAX - token - 1ull) return ULLONG_MAX;
        if ((start + token + 1ull) % ratio == 0ull) emitted++;
    }
    return emitted;
}

/* Contract: independently executes one complete learned rolling compressor
 * from raw hidden activations and materialized weights. */
static int ref_compressor_execute(
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_layer_plan *layer,
    const yvex_deepseek_v4_layer_spec *architecture,
    yvex_attention_rolling_kind kind,
    const yvex_attention_rolling_state_view *before,
    const float *hidden,
    unsigned long long token_count,
    unsigned long long hidden_stride,
    unsigned long long token_position,
    float **output,
    unsigned long long **positions,
    unsigned long long *output_count,
    yvex_attention_rolling_state_output *next_state,
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    yvex_tensor_role kv_role =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_KV
            : YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_KV;
    yvex_tensor_role gate_role =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_GATE
            : YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_GATE;
    yvex_tensor_role ape_role =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_APE
            : YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_APE;
    yvex_tensor_role norm_role =
        kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER
            ? YVEX_TENSOR_ROLE_INDEXER_COMPRESSOR_NORM
            : YVEX_TENSOR_ROLE_ATTENTION_COMPRESSOR_NORM;
    const yvex_runtime_tensor_binding *wkv =
        ref_binding(descriptor, kv_role, layer->layer_index);
    const yvex_runtime_tensor_binding *wgate =
        ref_binding(descriptor, gate_role, layer->layer_index);
    const yvex_runtime_tensor_binding *ape =
        ref_binding(descriptor, ape_role, layer->layer_index);
    const yvex_runtime_tensor_binding *norm =
        ref_binding(descriptor, norm_role, layer->layer_index);
    ref_rolling_state state;
    unsigned long long projected_count;
    unsigned long long emissions;
    unsigned long long token;
    unsigned long long emitted_count = 0ull;
    float *projected_kv = NULL;
    float *projected_score = NULL;
    float *ape_row = NULL;
    float *norm_weight = NULL;
    float *emitted_values = NULL;
    unsigned long long *emitted_positions = NULL;
    int ok = 0;

    if (output) *output = NULL;
    if (positions) *positions = NULL;
    if (output_count) *output_count = 0ull;
    if (next_state) memset(next_state, 0, sizeof(*next_state));
    memset(&state, 0, sizeof(state));
    if (!session || !descriptor || !layer || !architecture || !hidden ||
        !output || !positions || !output_count || !next_state || !wkv ||
        !wgate || !ape || !norm ||
        !ref_rolling_init(&state, layer, kind, before,
                                   token_position) ||
        !ref_mul(token_count, state.width, &projected_count)) {
        ref_reason(reason, "reference compressor admission failed");
        goto cleanup;
    }
    projected_kv = (float *)ref_alloc(projected_count,
                                      sizeof(*projected_kv));
    projected_score = (float *)ref_alloc(projected_count,
                                         sizeof(*projected_score));
    ape_row = (float *)ref_alloc(state.width, sizeof(*ape_row));
    norm_weight = (float *)ref_alloc(state.head_dimension,
                                     sizeof(*norm_weight));
    emissions = ref_emission_count(token_position, token_count, state.ratio);
    if (emissions == ULLONG_MAX ||
        (emissions &&
         (!(emitted_values = (float *)ref_alloc(
                emissions * state.head_dimension,
                sizeof(*emitted_values))) ||
          !(emitted_positions = (unsigned long long *)ref_alloc(
                emissions, sizeof(*emitted_positions))))) ||
        !projected_kv || !projected_score || !ape_row || !norm_weight) {
        ref_reason(reason, "reference compressor allocation failed");
        goto cleanup;
    }
    if (!ref_matrix_batch(session, wkv, 0ull, state.width, hidden,
                          token_count, hidden_stride, projected_kv,
                          state.width, reason) ||
        !ref_matrix_batch(session, wgate, 0ull, state.width, hidden,
                          token_count, hidden_stride, projected_score,
                          state.width, reason) ||
        !ref_decode_row(session, norm, 0ull, norm_weight, reason))
        goto cleanup;
    for (token = 0ull; token < token_count; ++token) {
        float scratch[1];
        float *destination = emissions
            ? emitted_values + emitted_count * state.head_dimension
            : scratch;
        int emitted = 0;
        unsigned long long position;

        if (!ref_decode_row(session, ape,
                            (token_position + token) % state.ratio,
                            ape_row, reason) ||
            !ref_rolling_step(&state,
                              projected_kv + token * state.width,
                              projected_score + token * state.width,
                              ape_row, destination, &emitted)) {
            ref_reason(reason, "reference compressor transition failed");
            goto cleanup;
        }
        if (!emitted) continue;
        if (emitted_count >= emissions ||
            !ref_rms(destination, state.head_dimension, norm_weight,
                     architecture->rms_norm_epsilon)) {
            ref_reason(reason, "reference compressor normalization failed");
            goto cleanup;
        }
        position = token_position + token + 1ull - state.ratio;
        if (!ref_rope(destination, state.head_dimension,
                      architecture->rope_head_dimension, position,
                      &architecture->position, 0)) {
            ref_reason(reason, "reference compressor RoPE failed");
            goto cleanup;
        }
        if (kind == YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER) {
            if (!ref_activation(&layer->compressor_rotated_activation,
                                destination, state.head_dimension)) {
                ref_reason(reason,
                           "reference indexer compressor activation failed");
                goto cleanup;
            }
        } else if (state.head_dimension > architecture->rope_head_dimension &&
                   !ref_activation(
                       &layer->compressor_activation, destination,
                       state.head_dimension - architecture->rope_head_dimension)) {
            ref_reason(reason, "reference compressor activation failed");
            goto cleanup;
        }
        emitted_positions[emitted_count++] = position;
    }
    if (emitted_count != emissions) {
        ref_reason(reason, "reference compressor emission count mismatched");
        goto cleanup;
    }
    if (!ref_rolling_export(&state, layer, kind, before, next_state)) {
        ref_reason(reason, "reference compressor state export failed");
        goto cleanup;
    }
    *output = emitted_values;
    *positions = emitted_positions;
    *output_count = emitted_count;
    emitted_values = NULL;
    emitted_positions = NULL;
    ok = 1;

cleanup:
    ref_rolling_release(&state);
    free(projected_kv);
    free(projected_score);
    free(ape_row);
    free(norm_weight);
    free(emitted_values);
    free(emitted_positions);
    return ok;
}

/* Contract: independently scores and ranks one CSA candidate set. */
static int ref_csa_select(
    const yvex_attention_layer_plan *layer,
    const yvex_attention_history_view *history,
    const float *current_indexer,
    unsigned long long current_count,
    unsigned long long current_stride,
    const unsigned long long *current_positions,
    const float *query,
    const float *weights,
    unsigned long long query_position,
    unsigned long long *selected,
    unsigned long long *selected_count)
{
    unsigned long long total = history->indexer_entry_count + current_count;
    ref_ranked_candidate *ranked;
    unsigned long long candidate;
    unsigned long long valid = 0ull;
    unsigned long long count;

    *selected_count = 0ull;
    if (!total) return 1;
    ranked = (ref_ranked_candidate *)ref_alloc(total, sizeof(*ranked));
    if (!ranked) return 0;
    for (candidate = 0ull; candidate < total; ++candidate) {
        const float *key = ref_segmented_row(
            history->indexer_kv, history->indexer_entry_count,
            history->indexer_kv_stride, current_indexer, current_count,
            current_stride, candidate);
        unsigned long long position = ref_segmented_position(
            history->indexer_positions, history->indexer_entry_count,
            current_positions, current_count, candidate);
        unsigned long long head;
        double score = 0.0;

        if (!key || position == ULLONG_MAX || position > query_position ||
            position > ULLONG_MAX - layer->compression_ratio + 1ull ||
            position + layer->compression_ratio - 1ull > query_position)
            continue;
        for (head = 0ull; head < layer->indexer_heads; ++head) {
            unsigned long long lane;
            double dot = 0.0;
            for (lane = 0ull; lane < layer->indexer_head_dimension; ++lane)
                dot += (double)query[
                           head * layer->indexer_head_dimension + lane] *
                       (double)key[lane];
            if (dot < 0.0) dot = 0.0;
            score += dot * (double)weights[head];
        }
        score /= sqrt((double)layer->indexer_head_dimension);
        score /= sqrt((double)layer->indexer_heads);
        if (!isfinite(score)) {
            free(ranked);
            return 0;
        }
        ranked[valid].score = (float)score;
        ranked[valid].position = position;
        ranked[valid].index = candidate;
        valid++;
    }
    qsort(ranked, (size_t)valid, sizeof(*ranked), ref_rank_compare);
    for (candidate = 1ull; candidate < valid; ++candidate) {
        if (ranked[candidate - 1ull].position == ranked[candidate].position) {
            free(ranked);
            return 0;
        }
    }
    count = valid < layer->sparse_topk.k ? valid : layer->sparse_topk.k;
    for (candidate = 0ull; candidate < count; ++candidate)
        selected[candidate] = ranked[candidate].index;
    *selected_count = count;
    free(ranked);
    return 1;
}

static int ref_candidate_score(const float *query,
                               const float *row,
                               unsigned long long width,
                               double scale,
                               double *score)
{
    unsigned long long lane;
    double sum = 0.0;

    for (lane = 0ull; lane < width; ++lane) {
        if (!isfinite(query[lane]) || !isfinite(row[lane])) return 0;
        sum += (double)query[lane] * (double)row[lane];
    }
    *score = sum * scale;
    return isfinite(*score);
}

static int ref_accumulate(const float *query,
                          const float *row,
                          unsigned long long width,
                          double scale,
                          double maximum,
                          double *denominator,
                          float *output)
{
    unsigned long long lane;
    double score;
    double weight;

    if (!ref_candidate_score(query, row, width, scale, &score)) return 0;
    weight = exp(score - maximum);
    if (!isfinite(weight)) return 0;
    *denominator += weight;
    for (lane = 0ull; lane < width; ++lane)
        output[lane] += (float)(weight * (double)row[lane]);
    return 1;
}

/* Contract: independently evaluates local plus HCA/CSA sparse attention and
 * records deterministic CSA selections. */
static int ref_sparse_reduce(
    const yvex_attention_layer_plan *layer,
    const yvex_deepseek_v4_layer_spec *architecture,
    const float *query,
    const yvex_attention_history_view *history,
    const float *current_kv,
    unsigned long long current_kv_stride,
    const float *current_compressed,
    unsigned long long current_compressed_count,
    const unsigned long long *current_compressed_positions,
    const float *current_indexer,
    unsigned long long current_indexer_count,
    const unsigned long long *current_indexer_positions,
    const float *index_query,
    const float *index_weights,
    const float *sinks,
    unsigned long long token_count,
    unsigned long long token_position,
    float *out,
    unsigned long long *topk_counts,
    unsigned long long *topk_positions,
    unsigned long long topk_stride)
{
    unsigned long long compressed_total =
        history->compressed_entry_count + current_compressed_count;
    unsigned long long *selected = NULL;
    unsigned long long token;
    double scale = 1.0 / sqrt((double)layer->head_dimension);

    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
        compressed_total) {
        selected = (unsigned long long *)ref_alloc(
            compressed_total < layer->sparse_topk.k
                ? compressed_total : layer->sparse_topk.k,
            sizeof(*selected));
        if (!selected) return 0;
    }
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long absolute = token_position + token;
        unsigned long long selected_count = 0ull;
        unsigned long long head;

        if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA &&
            compressed_total &&
            !ref_csa_select(
                layer, history, current_indexer, current_indexer_count,
                layer->indexer_head_dimension, current_indexer_positions,
                index_query + token * layer->indexer_heads *
                                  layer->indexer_head_dimension,
                index_weights + token * layer->indexer_heads, absolute,
                selected, &selected_count)) {
            free(selected);
            return 0;
        }
        if (topk_counts) {
            unsigned long long i;
            if (selected_count > topk_stride) {
                free(selected);
                return 0;
            }
            topk_counts[token] = selected_count;
            for (i = 0ull; i < selected_count; ++i)
                topk_positions[token * topk_stride + i] =
                    ref_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, selected[i]);
        }
        for (head = 0ull; head < layer->query_heads; ++head) {
            const float *head_query =
                query + token * layer->query_heads * layer->head_dimension +
                head * layer->head_dimension;
            float *destination =
                out + token * layer->query_heads * layer->head_dimension +
                head * layer->head_dimension;
            unsigned long long candidate;
            unsigned long long lane;
            double maximum = sinks[head];
            double denominator;

            if (!isfinite(maximum)) goto fail;
            for (candidate = 0ull; candidate < history->local_tail_count;
                 ++candidate) {
                unsigned long long position = history->local_positions[candidate];
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window : 0ull;
                double score;
                if (position < first || position > absolute) continue;
                if (!ref_candidate_score(
                        head_query,
                        history->local_kv + candidate * history->local_kv_stride,
                        layer->head_dimension, scale, &score))
                    goto fail;
                if (score > maximum) maximum = score;
            }
            for (candidate = 0ull; candidate <= token; ++candidate) {
                unsigned long long position = token_position + candidate;
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window : 0ull;
                double score;
                if (position < first) continue;
                if (!ref_candidate_score(
                        head_query, current_kv + candidate * current_kv_stride,
                        layer->head_dimension, scale, &score))
                    goto fail;
                if (score > maximum) maximum = score;
            }
            if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA) {
                for (candidate = 0ull; candidate < compressed_total;
                     ++candidate) {
                    unsigned long long position = ref_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, candidate);
                    const float *row;
                    double score;
                    if (position == ULLONG_MAX || position > absolute ||
                        position > ULLONG_MAX - layer->compression_ratio + 1ull ||
                        position + layer->compression_ratio - 1ull > absolute)
                        continue;
                    row = ref_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, layer->head_dimension,
                        candidate);
                    if (!ref_candidate_score(head_query, row,
                                             layer->head_dimension, scale,
                                             &score))
                        goto fail;
                    if (score > maximum) maximum = score;
                }
            } else if (layer->attention_class ==
                       YVEX_ATTENTION_CLASS_CSA) {
                for (candidate = 0ull; candidate < selected_count;
                     ++candidate) {
                    const float *row = ref_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, layer->head_dimension,
                        selected[candidate]);
                    double score;
                    if (!ref_candidate_score(head_query, row,
                                             layer->head_dimension, scale,
                                             &score))
                        goto fail;
                    if (score > maximum) maximum = score;
                }
            }
            denominator = exp((double)sinks[head] - maximum);
            memset(destination, 0,
                   (size_t)layer->head_dimension * sizeof(*destination));
            for (candidate = 0ull; candidate < history->local_tail_count;
                 ++candidate) {
                unsigned long long position = history->local_positions[candidate];
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window : 0ull;
                if (position < first || position > absolute) continue;
                if (!ref_accumulate(
                        head_query,
                        history->local_kv + candidate * history->local_kv_stride,
                        layer->head_dimension, scale, maximum, &denominator,
                        destination))
                    goto fail;
            }
            for (candidate = 0ull; candidate <= token; ++candidate) {
                unsigned long long position = token_position + candidate;
                unsigned long long first =
                    absolute + 1ull > layer->sliding_window
                        ? absolute + 1ull - layer->sliding_window : 0ull;
                if (position < first) continue;
                if (!ref_accumulate(
                        head_query, current_kv + candidate * current_kv_stride,
                        layer->head_dimension, scale, maximum, &denominator,
                        destination))
                    goto fail;
            }
            if (layer->attention_class == YVEX_ATTENTION_CLASS_HCA) {
                for (candidate = 0ull; candidate < compressed_total;
                     ++candidate) {
                    unsigned long long position = ref_segmented_position(
                        history->compressed_positions,
                        history->compressed_entry_count,
                        current_compressed_positions,
                        current_compressed_count, candidate);
                    const float *row;
                    if (position == ULLONG_MAX || position > absolute ||
                        position > ULLONG_MAX - layer->compression_ratio + 1ull ||
                        position + layer->compression_ratio - 1ull > absolute)
                        continue;
                    row = ref_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, layer->head_dimension,
                        candidate);
                    if (!ref_accumulate(head_query, row,
                                        layer->head_dimension, scale, maximum,
                                        &denominator, destination))
                        goto fail;
                }
            } else if (layer->attention_class ==
                       YVEX_ATTENTION_CLASS_CSA) {
                for (candidate = 0ull; candidate < selected_count;
                     ++candidate) {
                    const float *row = ref_segmented_row(
                        history->compressed_kv,
                        history->compressed_entry_count,
                        history->compressed_kv_stride, current_compressed,
                        current_compressed_count, layer->head_dimension,
                        selected[candidate]);
                    if (!ref_accumulate(head_query, row,
                                        layer->head_dimension, scale, maximum,
                                        &denominator, destination))
                        goto fail;
                }
            }
            if (!(denominator > 0.0) || !isfinite(denominator)) goto fail;
            for (lane = 0ull; lane < layer->head_dimension; ++lane)
                destination[lane] = (float)((double)destination[lane] /
                                            denominator);
            if (!ref_rope(destination, layer->head_dimension,
                          architecture->rope_head_dimension, absolute,
                          &architecture->position, 1))
                goto fail;
        }
    }
    free(selected);
    return 1;

fail:
    free(selected);
    return 0;
}

static int ref_output_projection(
    yvex_materialization_session *session,
    const yvex_runtime_tensor_binding *out_a,
    const yvex_runtime_tensor_binding *out_b,
    const yvex_deepseek_v4_layer_spec *architecture,
    const float *attention,
    unsigned long long token_count,
    unsigned long long attention_stride,
    unsigned long long hidden_width,
    float *output,
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    unsigned long long low_stride;
    unsigned long long low_count;
    unsigned long long group;
    float *low;

    if (!ref_mul(architecture->output_groups,
                 architecture->output_lora_rank, &low_stride) ||
        !ref_mul(token_count, low_stride, &low_count))
        return 0;
    low = (float *)ref_alloc(low_count, sizeof(*low));
    if (!low) return 0;
    for (group = 0ull; group < architecture->output_groups; ++group) {
        if (!ref_matrix_batch(
                session, out_a, group * architecture->output_lora_rank,
                architecture->output_lora_rank,
                attention + group * architecture->output_group_input_width,
                token_count, attention_stride,
                low + group * architecture->output_lora_rank, low_stride,
                reason)) {
            free(low);
            return 0;
        }
    }
    if (!ref_matrix_batch(session, out_b, 0ull, hidden_width, low,
                          token_count, low_stride, output, hidden_width,
                          reason)) {
        free(low);
        return 0;
    }
    free(low);
    return 1;
}

/* Contract: executes the complete DeepSeek attention equation independently
 * of the production graph implementation and publishes only a complete trace. */
int yvex_test_attention_reference_execute(
    const yvex_attention_plan *plan,
    const yvex_deepseek_v4_ir *ir,
    yvex_materialization_session *session,
    const yvex_runtime_descriptor *descriptor,
    const yvex_attention_cpu_options *options,
    yvex_attention_execution_trace *trace,
    char reason[YVEX_TEST_ATTENTION_REFERENCE_REASON_CAP])
{
    yvex_attention_cpu_options defaults;
    const yvex_attention_cpu_options *opts = options;
    const yvex_attention_layer_plan *layer;
    const yvex_deepseek_v4_layer_spec *architecture;
    const yvex_runtime_tensor_binding *q_a;
    const yvex_runtime_tensor_binding *q_a_norm;
    const yvex_runtime_tensor_binding *q_b;
    const yvex_runtime_tensor_binding *kv;
    const yvex_runtime_tensor_binding *kv_norm;
    const yvex_runtime_tensor_binding *sinks_binding;
    const yvex_runtime_tensor_binding *out_a;
    const yvex_runtime_tensor_binding *out_b;
    yvex_attention_history_view empty_history;
    const yvex_attention_history_view *history;
    unsigned long long token_count;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long query_width;
    unsigned long long kv_width;
    unsigned long long hidden_count;
    unsigned long long q_count;
    unsigned long long query_count;
    unsigned long long kv_count;
    unsigned long long token;
    float *input = NULL;
    float *q_low = NULL;
    float *q_norm = NULL;
    float *query = NULL;
    float *raw_kv = NULL;
    float *kv_norm_weight = NULL;
    float *sinks = NULL;
    float *compressed = NULL;
    unsigned long long *compressed_positions = NULL;
    unsigned long long compressed_count = 0ull;
    float *indexer = NULL;
    unsigned long long *indexer_positions = NULL;
    unsigned long long indexer_count = 0ull;
    float *index_query = NULL;
    float *index_weights = NULL;
    float *attention = NULL;
    float *output = NULL;
    unsigned long long *topk_counts = NULL;
    unsigned long long *topk_positions = NULL;
    yvex_attention_rolling_state_output next_main_state;
    yvex_attention_rolling_state_output next_indexer_state;
    unsigned long long topk_stride = 0ull;
    int ok = 0;

    memset(&next_main_state, 0, sizeof(next_main_state));
    memset(&next_indexer_state, 0, sizeof(next_indexer_state));
    if (reason) reason[0] = '\0';
    if (!opts) {
        memset(&defaults, 0, sizeof(defaults));
        defaults.token_count = 1ull;
        defaults.scratch_limit_bytes = 64ull * 1024ull * 1024ull;
        opts = &defaults;
    }
    if (!plan || !ir || !session || !descriptor || !trace || trace->owned) {
        ref_reason(reason, "reference execution arguments are invalid");
        return 0;
    }
    token_count = opts->token_count ? opts->token_count : 1ull;
    layer = yvex_graph_lower_deepseek_v4()->plan_layer_at(plan, opts->layer_index);
    architecture = yvex_model_register_deepseek_v4()->ir.layer_at(ir, opts->layer_index);
    if (!layer || !architecture || !token_count) {
        ref_reason(reason, "reference execution layer is absent");
        return 0;
    }
    q_a = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A,
                      layer->layer_index);
    q_a_norm = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_A_NORM,
                           layer->layer_index);
    q_b = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_Q_B,
                      layer->layer_index);
    kv = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV,
                     layer->layer_index);
    kv_norm = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_KV_NORM,
                          layer->layer_index);
    sinks_binding = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_SINKS,
                                layer->layer_index);
    out_a = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_A,
                        layer->layer_index);
    out_b = ref_binding(descriptor, YVEX_TENSOR_ROLE_ATTENTION_OUT_B,
                        layer->layer_index);
    if (!q_a || !q_a_norm || !q_b || !kv || !kv_norm || !sinks_binding ||
        !out_a || !out_b) {
        ref_reason(reason, "reference execution bindings are incomplete");
        goto cleanup;
    }
    hidden_width = q_a->binding->row_width;
    q_rank = q_a->binding->row_count;
    query_width = q_b->binding->row_count;
    kv_width = kv->binding->row_count;
    if (q_b->binding->row_width != q_rank ||
        query_width != layer->query_heads * layer->head_dimension ||
        kv_width != layer->head_dimension ||
        hidden_width != layer->hidden_dimension ||
        !ref_mul(token_count, hidden_width, &hidden_count) ||
        !ref_mul(token_count, q_rank, &q_count) ||
        !ref_mul(token_count, query_width, &query_count) ||
        !ref_mul(token_count, kv_width, &kv_count)) {
        ref_reason(reason, "reference execution dimensions are invalid");
        goto cleanup;
    }
    input = (float *)ref_alloc(hidden_count, sizeof(*input));
    q_low = (float *)ref_alloc(q_count, sizeof(*q_low));
    q_norm = (float *)ref_alloc(q_rank, sizeof(*q_norm));
    query = (float *)ref_alloc(query_count, sizeof(*query));
    raw_kv = (float *)ref_alloc(kv_count, sizeof(*raw_kv));
    kv_norm_weight = (float *)ref_alloc(kv_width, sizeof(*kv_norm_weight));
    sinks = (float *)ref_alloc(layer->query_heads, sizeof(*sinks));
    attention = (float *)ref_alloc(query_count, sizeof(*attention));
    output = (float *)ref_alloc(hidden_count, sizeof(*output));
    if (!input || !q_low || !q_norm || !query || !raw_kv ||
        !kv_norm_weight || !sinks || !attention || !output) {
        ref_reason(reason, "reference execution allocation failed");
        goto cleanup;
    }
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long lane;
        if (opts->input) {
            if (opts->input_stride < hidden_width) {
                ref_reason(reason, "reference input stride is invalid");
                goto cleanup;
            }
            memcpy(input + token * hidden_width,
                   opts->input + token * opts->input_stride,
                   (size_t)hidden_width * sizeof(*input));
        } else {
            for (lane = 0ull; lane < hidden_width; ++lane) {
                unsigned long long mixed =
                    (lane * 1103515245ull) ^
                    (layer->layer_index * 2654435761ull) ^
                    ((opts->token_position + token) * 97ull);
                input[token * hidden_width + lane] =
                    (float)((int)(mixed % 257ull) - 128) / 128.0f;
            }
        }
        for (lane = 0ull; lane < hidden_width; ++lane) {
            if (!isfinite(input[token * hidden_width + lane])) {
                ref_reason(reason, "reference input is non-finite");
                goto cleanup;
            }
        }
    }
    if (!ref_matrix_batch(session, q_a, 0ull, q_rank, input, token_count,
                          hidden_width, q_low, q_rank, reason) ||
        !ref_decode_row(session, q_a_norm, 0ull, q_norm, reason))
        goto cleanup;
    for (token = 0ull; token < token_count; ++token) {
        if (!ref_rms(q_low + token * q_rank, q_rank, q_norm,
                     architecture->rms_norm_epsilon)) {
            ref_reason(reason, "reference Q-A norm failed");
            goto cleanup;
        }
    }
    if (!ref_matrix_batch(session, q_b, 0ull, query_width, q_low,
                          token_count, q_rank, query, query_width, reason) ||
        !ref_matrix_batch(session, kv, 0ull, kv_width, input, token_count,
                          hidden_width, raw_kv, kv_width, reason) ||
        !ref_decode_row(session, kv_norm, 0ull, kv_norm_weight, reason) ||
        !ref_decode_flat(session, sinks_binding, sinks, layer->query_heads,
                         reason))
        goto cleanup;
    for (token = 0ull; token < token_count; ++token) {
        unsigned long long head;
        for (head = 0ull; head < layer->query_heads; ++head) {
            float *head_query = query + token * query_width +
                                head * layer->head_dimension;
            if (!ref_rms(head_query, layer->head_dimension, NULL,
                         architecture->rms_norm_epsilon) ||
                !ref_rope(head_query, layer->head_dimension,
                          architecture->rope_head_dimension,
                          opts->token_position + token,
                          &architecture->position, 0)) {
                ref_reason(reason, "reference query norm or RoPE failed");
                goto cleanup;
            }
        }
        if (!ref_rms(raw_kv + token * kv_width, kv_width, kv_norm_weight,
                     architecture->rms_norm_epsilon) ||
            !ref_rope(raw_kv + token * kv_width, kv_width,
                      architecture->rope_head_dimension,
                      opts->token_position + token,
                      &architecture->position, 0) ||
            (kv_width > architecture->rope_head_dimension &&
             !ref_activation(&layer->attention_kv_activation,
                             raw_kv + token * kv_width,
                             kv_width - architecture->rope_head_dimension))) {
            ref_reason(reason, "reference KV norm, RoPE, or activation failed");
            goto cleanup;
        }
    }
    memset(&empty_history, 0, sizeof(empty_history));
    empty_history.immutable = 1;
    empty_history.token_count = opts->token_position;
    history = opts->history ? opts->history : &empty_history;
    if (history->token_count != opts->token_position) {
        ref_reason(reason, "reference history is not contiguous");
        goto cleanup;
    }
    if (layer->attention_class != YVEX_ATTENTION_CLASS_SWA &&
        !ref_compressor_execute(
            session, descriptor, layer, architecture,
            YVEX_DEEPSEEK_ATTENTION_ROLLING_MAIN,
            opts->history ? &history->main_rolling_state : NULL, input,
            token_count, hidden_width, opts->token_position, &compressed,
            &compressed_positions, &compressed_count, &next_main_state,
            reason))
        goto cleanup;
    if (layer->attention_class == YVEX_ATTENTION_CLASS_CSA) {
        const yvex_runtime_tensor_binding *index_q = ref_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_ATTENTION_Q_B,
            layer->layer_index);
        const yvex_runtime_tensor_binding *index_w = ref_binding(
            descriptor, YVEX_TENSOR_ROLE_INDEXER_PROJECTION,
            layer->layer_index);
        unsigned long long index_query_stride;
        unsigned long long index_query_count;
        unsigned long long index_weight_count;

        if (!ref_compressor_execute(
                session, descriptor, layer, architecture,
                YVEX_DEEPSEEK_ATTENTION_ROLLING_INDEXER,
                opts->history ? &history->indexer_rolling_state : NULL,
                input, token_count, hidden_width, opts->token_position,
                &indexer, &indexer_positions, &indexer_count,
                &next_indexer_state, reason) ||
            indexer_count != compressed_count || !index_q || !index_w ||
            !ref_mul(layer->indexer_heads,
                     layer->indexer_head_dimension, &index_query_stride) ||
            !ref_mul(token_count, index_query_stride, &index_query_count) ||
            !ref_mul(token_count, layer->indexer_heads,
                     &index_weight_count))
            goto cleanup;
        index_query = (float *)ref_alloc(index_query_count,
                                         sizeof(*index_query));
        index_weights = (float *)ref_alloc(index_weight_count,
                                           sizeof(*index_weights));
        if (!index_query || !index_weights ||
            !ref_matrix_batch(session, index_q, 0ull, index_query_stride,
                              q_low, token_count, q_rank, index_query,
                              index_query_stride, reason) ||
            !ref_matrix_batch(session, index_w, 0ull, layer->indexer_heads,
                              input, token_count, hidden_width, index_weights,
                              layer->indexer_heads, reason))
            goto cleanup;
        for (token = 0ull; token < token_count; ++token) {
            unsigned long long head;
            for (head = 0ull; head < layer->indexer_heads; ++head) {
                float *head_query =
                    index_query + token * index_query_stride +
                    head * layer->indexer_head_dimension;
                if (!ref_rope(head_query, layer->indexer_head_dimension,
                              architecture->rope_head_dimension,
                              opts->token_position + token,
                              &architecture->position, 0) ||
                    !ref_activation(&layer->indexer_query_activation,
                                    head_query,
                                    layer->indexer_head_dimension)) {
                    ref_reason(reason,
                               "reference CSA query activation failed");
                    goto cleanup;
                }
            }
        }
        topk_stride = history->compressed_entry_count + compressed_count;
        if (topk_stride > layer->sparse_topk.k)
            topk_stride = layer->sparse_topk.k;
        if (topk_stride) {
            unsigned long long topk_count;
            if (!ref_mul(token_count, topk_stride, &topk_count))
                goto cleanup;
            topk_counts = (unsigned long long *)ref_alloc(
                token_count, sizeof(*topk_counts));
            topk_positions = (unsigned long long *)ref_alloc(
                topk_count, sizeof(*topk_positions));
            if (!topk_counts || !topk_positions) goto cleanup;
        }
    }
    if (!ref_sparse_reduce(
            layer, architecture, query, history, raw_kv, kv_width,
            compressed, compressed_count, compressed_positions, indexer,
            indexer_count, indexer_positions, index_query, index_weights,
            sinks, token_count, opts->token_position, attention, topk_counts,
            topk_positions, topk_stride) ||
        !ref_output_projection(session, out_a, out_b, architecture, attention,
                               token_count, query_width, hidden_width, output,
                               reason)) {
        ref_reason(reason, "reference attention or output projection failed");
        goto cleanup;
    }

    memset(trace, 0, sizeof(*trace));
    trace->owned = 1;
    trace->complete = 1;
    trace->layer_index = layer->layer_index;
    trace->attention_class = layer->attention_class;
    trace->token_position = opts->token_position;
    trace->token_count = token_count;
    trace->hidden_width = hidden_width;
    trace->q_rank = q_rank;
    trace->query_width = query_width;
    trace->kv_width = kv_width;
    trace->compressed_count = compressed_count;
    trace->compressed_stride = compressed_count ? layer->head_dimension : 0ull;
    trace->indexer_count = indexer_count;
    trace->indexer_stride = indexer_count ? layer->indexer_head_dimension : 0ull;
    trace->index_query_stride =
        layer->attention_class == YVEX_ATTENTION_CLASS_CSA
            ? layer->indexer_heads * layer->indexer_head_dimension : 0ull;
    trace->index_weight_stride =
        layer->attention_class == YVEX_ATTENTION_CLASS_CSA
            ? layer->indexer_heads : 0ull;
    trace->topk_stride = topk_stride;
    trace->input = input;
    trace->q_low = q_low;
    trace->query = query;
    trace->raw_kv = raw_kv;
    trace->compressed_kv = compressed;
    trace->indexer_kv = indexer;
    trace->index_query = index_query;
    trace->index_weights = index_weights;
    trace->attention_values = attention;
    trace->output = output;
    trace->compressed_positions = compressed_positions;
    trace->indexer_positions = indexer_positions;
    trace->topk_counts = topk_counts;
    trace->topk_positions = topk_positions;
    trace->next_main_rolling_state = next_main_state;
    trace->next_indexer_rolling_state = next_indexer_state;
    input = q_low = query = raw_kv = compressed = indexer = NULL;
    index_query = index_weights = attention = output = NULL;
    compressed_positions = indexer_positions = NULL;
    topk_counts = topk_positions = NULL;
    memset(&next_main_state, 0, sizeof(next_main_state));
    memset(&next_indexer_state, 0, sizeof(next_indexer_state));
    ok = 1;

cleanup:
    if (!ok && (!reason || !reason[0]))
        ref_reason(reason, "reference full-equation execution failed");
    free(input);
    free(q_low);
    free(q_norm);
    free(query);
    free(raw_kv);
    free(kv_norm_weight);
    free(sinks);
    free(compressed);
    free(compressed_positions);
    free(indexer);
    free(indexer_positions);
    free(index_query);
    free(index_weights);
    free(attention);
    free(output);
    free(topk_counts);
    free(topk_positions);
    free(next_main_state.kv_state);
    free(next_main_state.score_state);
    free(next_indexer_state.kv_state);
    free(next_indexer_state.score_state);
    return ok;
}

static int ref_compare_stage(
    const char *stage,
    const float *production,
    const float *reference,
    unsigned long long count,
    double absolute_tolerance,
    double relative_tolerance,
    yvex_test_attention_reference_metrics *metrics)
{
    unsigned long long i;

    if (count && (!production || !reference)) {
        if (!metrics->first_failed_stage) metrics->first_failed_stage = stage;
        return 0;
    }
    for (i = 0ull; i < count; ++i) {
        double left = production[i];
        double right = reference[i];
        double absolute = fabs(left - right);
        double denominator = fmax(fabs(left), fabs(right));
        double relative = denominator > 0.0 ? absolute / denominator : 0.0;
        double allowed = absolute_tolerance + relative_tolerance * denominator;
        metrics->compared_values++;
        metrics->squared_error_sum += absolute * absolute;
        if (absolute > metrics->maximum_absolute_error)
            metrics->maximum_absolute_error = absolute;
        if (relative > metrics->maximum_relative_error)
            metrics->maximum_relative_error = relative;
        if (!isfinite(left) || !isfinite(right) || absolute > allowed) {
            if (!metrics->first_failed_stage)
                metrics->first_failed_stage = stage;
            return 0;
        }
    }
    return 1;
}

/* Contract: compares one rolling-state geometry and all owned state values,
 * admitting equal signed infinities only for intentionally unused score lanes. */
static int ref_compare_rolling_state(
    const char *stage,
    const yvex_attention_rolling_state_output *production,
    const yvex_attention_rolling_state_output *reference,
    double absolute_tolerance,
    double relative_tolerance,
    yvex_test_attention_reference_metrics *metrics)
{
    unsigned long long i;

    if (!production || !reference ||
        production->present != reference->present) {
        metrics->first_failed_stage = stage;
        return 0;
    }
    if (!production->present) return 1;
    if (production->schema_version != reference->schema_version ||
        production->kind != reference->kind ||
        production->attention_class != reference->attention_class ||
        production->layer_index != reference->layer_index ||
        production->next_token_position != reference->next_token_position ||
        production->ratio != reference->ratio ||
        production->head_dimension != reference->head_dimension ||
        production->state_width != reference->state_width ||
        production->state_slots != reference->state_slots ||
        production->previous_fill != reference->previous_fill ||
        production->current_fill != reference->current_fill ||
        production->cursor != reference->cursor ||
        production->kv_state_stride != reference->kv_state_stride ||
        production->score_state_stride != reference->score_state_stride ||
        production->kv_state_extent != reference->kv_state_extent ||
        production->score_state_extent != reference->score_state_extent ||
        production->overlap != reference->overlap ||
        production->rotated != reference->rotated ||
        memcmp(production->attention_plan_identity,
               reference->attention_plan_identity,
               sizeof(production->attention_plan_identity)) != 0 ||
        !production->kv_state || !reference->kv_state ||
        !production->score_state || !reference->score_state) {
        metrics->first_failed_stage = stage;
        return 0;
    }
    if (!ref_compare_stage(stage, production->kv_state, reference->kv_state,
                           production->kv_state_extent, absolute_tolerance,
                           relative_tolerance, metrics))
        return 0;
    for (i = 0ull; i < production->score_state_extent; ++i) {
        double left = production->score_state[i];
        double right = reference->score_state[i];

        metrics->compared_values++;
        if (!isfinite(left) || !isfinite(right)) {
            if (left == right) continue;
            metrics->first_failed_stage = stage;
            return 0;
        }
        {
            double absolute = fabs(left - right);
            double denominator = fmax(fabs(left), fabs(right));
            double relative = denominator > 0.0
                ? absolute / denominator : 0.0;
            double allowed = absolute_tolerance +
                relative_tolerance * denominator;
            metrics->squared_error_sum += absolute * absolute;
            if (absolute > metrics->maximum_absolute_error)
                metrics->maximum_absolute_error = absolute;
            if (relative > metrics->maximum_relative_error)
                metrics->maximum_relative_error = relative;
            if (absolute > allowed) {
                metrics->first_failed_stage = stage;
                return 0;
            }
        }
    }
    return 1;
}

/* Contract: compares every continuous and discrete full-equation stage and
 * identifies the first independently detected divergence. */
int yvex_test_attention_reference_compare(
    const yvex_attention_execution_trace *production,
    const yvex_attention_execution_trace *reference,
    double absolute_tolerance,
    double relative_tolerance,
    yvex_test_attention_reference_metrics *metrics)
{
    unsigned long long count;
    unsigned long long i;

    if (!metrics) return 0;
    memset(metrics, 0, sizeof(*metrics));
    if (!production || !reference || !production->owned || !reference->owned ||
        !production->complete || !reference->complete ||
        production->layer_index != reference->layer_index ||
        production->attention_class != reference->attention_class ||
        production->token_position != reference->token_position ||
        production->token_count != reference->token_count ||
        production->hidden_width != reference->hidden_width ||
        production->q_rank != reference->q_rank ||
        production->query_width != reference->query_width ||
        production->kv_width != reference->kv_width ||
        production->compressed_count != reference->compressed_count ||
        production->compressed_stride != reference->compressed_stride ||
        production->indexer_count != reference->indexer_count ||
        production->indexer_stride != reference->indexer_stride ||
        production->index_query_stride != reference->index_query_stride ||
        production->index_weight_stride != reference->index_weight_stride ||
        production->topk_stride != reference->topk_stride) {
        metrics->first_failed_stage = "geometry";
        return 0;
    }
#define REF_STAGE(name_, left_, right_, count_)                              \
    do {                                                                     \
        if (!ref_compare_stage((name_), (left_), (right_), (count_),         \
                               absolute_tolerance, relative_tolerance,       \
                               metrics))                                     \
            return 0;                                                        \
    } while (0)
    count = production->token_count * production->hidden_width;
    REF_STAGE("input", production->input, reference->input, count);
    count = production->token_count * production->q_rank;
    REF_STAGE("q-low", production->q_low, reference->q_low, count);
    count = production->token_count * production->query_width;
    REF_STAGE("query", production->query, reference->query, count);
    count = production->token_count * production->kv_width;
    REF_STAGE("raw-kv", production->raw_kv, reference->raw_kv, count);
    count = production->compressed_count * production->compressed_stride;
    REF_STAGE("compressed-kv", production->compressed_kv,
              reference->compressed_kv, count);
    count = production->indexer_count * production->indexer_stride;
    REF_STAGE("indexer-kv", production->indexer_kv,
              reference->indexer_kv, count);
    count = production->token_count * production->index_query_stride;
    REF_STAGE("index-query", production->index_query,
              reference->index_query, count);
    count = production->token_count * production->index_weight_stride;
    REF_STAGE("index-weights", production->index_weights,
              reference->index_weights, count);
    count = production->token_count * production->query_width;
    REF_STAGE("attention", production->attention_values,
              reference->attention_values, count);
    count = production->token_count * production->hidden_width;
    REF_STAGE("output", production->output, reference->output, count);
#undef REF_STAGE
    if (!ref_compare_rolling_state(
            "main-rolling-state", &production->next_main_rolling_state,
            &reference->next_main_rolling_state, absolute_tolerance,
            relative_tolerance, metrics) ||
        !ref_compare_rolling_state(
            "indexer-rolling-state", &production->next_indexer_rolling_state,
            &reference->next_indexer_rolling_state, absolute_tolerance,
            relative_tolerance, metrics))
        return 0;
    for (i = 0ull; i < production->compressed_count; ++i) {
        if (production->compressed_positions[i] !=
            reference->compressed_positions[i]) {
            metrics->discrete_mismatches++;
            metrics->first_failed_stage = "compressed-positions";
            return 0;
        }
    }
    for (i = 0ull; i < production->indexer_count; ++i) {
        if (production->indexer_positions[i] !=
            reference->indexer_positions[i]) {
            metrics->discrete_mismatches++;
            metrics->first_failed_stage = "indexer-positions";
            return 0;
        }
    }
    for (i = 0ull; i < production->token_count; ++i) {
        unsigned long long j;
        if (production->topk_stride &&
            production->topk_counts[i] != reference->topk_counts[i]) {
            metrics->discrete_mismatches++;
            metrics->first_failed_stage = "topk-count";
            return 0;
        }
        for (j = 0ull; j < production->topk_stride; ++j) {
            if (production->topk_positions[
                    i * production->topk_stride + j] !=
                reference->topk_positions[
                    i * reference->topk_stride + j]) {
                metrics->discrete_mismatches++;
                metrics->first_failed_stage = "topk-order";
                return 0;
            }
        }
    }
    return 1;
}
