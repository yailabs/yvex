/*
 * quant_block.c - pinned GGUF block encoders and reference decoders.
 *
 * Owner: TRACK.QUANT.
 * Owns: deterministic F32/F16/BF16/I32, Q8_0, Q2_K, and MXFP4 bytes.
 * Does not own: qtype IDs/geometry, source IO, profile selection, CUDA kernels,
 *   artifact layout, writing, materialization, or rendering.
 * Invariants: block layouts match the pinned GGUF ABI; every conversion checks
 *   arity, capacity, non-finite policy, and little-endian scalar storage.
 * Boundary: bounded encoded blocks are writer inputs, not a GGUF artifact.
 */
#include "quant_numeric.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

/* Pinned GGML stores the E2M1 codebook doubled and applies E8M0 / 2. */
static const float quant_mxfp4_values[16] = {
    0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f, 8.0f, 12.0f,
    -0.0f, -1.0f, -2.0f, -3.0f, -4.0f, -6.0f, -8.0f, -12.0f
};

static void quant_block_fail(yvex_quant_failure *failure,
                             yvex_quant_failure_code code,
                             unsigned int qtype,
                             unsigned long long expected,
                             unsigned long long actual,
                             yvex_error *err,
                             int status,
                             const char *message)
{
    if (failure) {
        memset(failure, 0, sizeof(*failure));
        failure->code = code;
        failure->terminal_ordinal = ULLONG_MAX;
        failure->source_index = ULLONG_MAX;
        failure->row_index = ULLONG_MAX;
        failure->block_index = ULLONG_MAX;
        failure->expected = expected;
        failure->actual = actual;
        failure->qtype = qtype;
        failure->operation = YVEX_TRANSFORM_OP_COUNT;
    }
    yvex_error_set(err, (yvex_status)status, "quant.block", message);
}

static void quant_store_u16(unsigned char *out, unsigned short value)
{
    out[0] = (unsigned char)(value & 0xffu);
    out[1] = (unsigned char)(value >> 8);
}

static unsigned short quant_load_u16(const unsigned char *in)
{
    return (unsigned short)((unsigned short)in[0] |
                            ((unsigned short)in[1] << 8));
}

static void quant_store_u32(unsigned char *out, unsigned int value)
{
    out[0] = (unsigned char)(value & 0xffu);
    out[1] = (unsigned char)((value >> 8) & 0xffu);
    out[2] = (unsigned char)((value >> 16) & 0xffu);
    out[3] = (unsigned char)(value >> 24);
}

static unsigned int quant_load_u32(const unsigned char *in)
{
    return (unsigned int)in[0] |
           ((unsigned int)in[1] << 8) |
           ((unsigned int)in[2] << 16) |
           ((unsigned int)in[3] << 24);
}

/* Interprets canonical two's-complement I32 bytes without narrowing overflow. */
static int32_t quant_i32_from_u32(uint32_t value)
{
    return value <= (uint32_t)INT32_MAX
        ? (int32_t)value
        : -1 - (int32_t)(UINT32_MAX - value);
}

static int quant_nearest_even(float value)
{
    float lower = floorf(value);
    float fraction = value - lower;
    int integer = (int)lower;

    if (fraction > 0.5f || (fraction == 0.5f && (integer & 1))) integer++;
    return integer;
}

static int quant_values_finite(const float *values,
                               unsigned long long count,
                               unsigned long long *bad)
{
    unsigned long long index;
    for (index = 0u; index < count; ++index) {
        if (!isfinite(values[index])) {
            if (bad) *bad = index;
            return 0;
        }
    }
    return 1;
}

static unsigned int quant_mxfp4_best(float value, float scale)
{
    unsigned int best = 0u;
    float best_error = fabsf(value - quant_mxfp4_values[0] * scale);
    unsigned int index;

    for (index = 1u; index < 16u; ++index) {
        float error = fabsf(value - quant_mxfp4_values[index] * scale);
        if (error < best_error) {
            best = index;
            best_error = error;
        }
    }
    return best;
}

static unsigned char quant_mxfp4_exponent(float maximum)
{
    int exponent;

    if (!(maximum > 0.0f)) return 0u;
    exponent = (int)ceil(log2((double)maximum / 12.0)) + 128;
    if (exponent < 0) exponent = 0;
    if (exponent > 254) exponent = 254;
    return (unsigned char)exponent;
}

static int quant_encode_q8_0(const float *source, unsigned char *encoded)
{
    float maximum = 0.0f;
    float scale;
    float inverse;
    unsigned int index;

    for (index = 0u; index < YVEX_QUANT_Q8_0_ELEMENTS; ++index) {
        float magnitude = fabsf(source[index]);
        if (magnitude > maximum) maximum = magnitude;
    }
    scale = maximum / 127.0f;
    quant_store_u16(encoded, yvex_quant_f16_encode(scale));
    scale = yvex_quant_f16_decode(quant_load_u16(encoded));
    if (!isfinite(scale) || scale < 0.0f ||
        (maximum > 0.0f && scale == 0.0f)) return 0;
    inverse = scale != 0.0f ? 1.0f / scale : 0.0f;
    for (index = 0u; index < YVEX_QUANT_Q8_0_ELEMENTS; ++index) {
        int quantized = (int)roundf(source[index] * inverse);
        if (quantized < -127) quantized = -127;
        if (quantized > 127) quantized = 127;
        encoded[2u + index] = (unsigned char)(quantized < 0
            ? 256 + quantized : quantized);
    }
    return 1;
}

/*
 * Produces one 16-value Q2_K affine sub-block using the pinned reference
 * search. The output codes are temporary: global F16 scales requantize them
 * before publication. No allocation or IO occurs.
 */
static float quant_q2_subblock(const float *source,
                               unsigned char *codes,
                               float *minimum_out)
{
    unsigned char candidate[16];
    float minimum = source[0];
    float maximum = source[0];
    float weight_sum = fabsf(source[0]);
    float weighted_source_sum = weight_sum * source[0];
    float scale;
    float inverse;
    float best_error = 0.0f;
    unsigned int index;
    unsigned int step;

    for (index = 1u; index < 16u; ++index) {
        float weight = fabsf(source[index]);
        if (source[index] < minimum) minimum = source[index];
        if (source[index] > maximum) maximum = source[index];
        weight_sum += weight;
        weighted_source_sum += weight * source[index];
    }
    if (minimum > 0.0f) minimum = 0.0f;
    if (maximum == minimum) {
        memset(codes, 0, 16u);
        *minimum_out = -minimum;
        return 0.0f;
    }
    inverse = 3.0f / (maximum - minimum);
    scale = 1.0f / inverse;
    for (index = 0u; index < 16u; ++index) {
        int code = quant_nearest_even(inverse * (source[index] - minimum));
        float difference;
        if (code < 0) code = 0;
        if (code > 3) code = 3;
        codes[index] = (unsigned char)code;
        difference = fabsf(scale * code + minimum - source[index]);
        best_error += fabsf(source[index]) * difference;
    }
    for (step = 0u; step <= 15u; ++step) {
        float code_sum = 0.0f;
        float code_squared_sum = 0.0f;
        float source_code_sum = 0.0f;
        float determinant;
        float candidate_scale;
        float candidate_minimum;
        float candidate_error = 0.0f;

        inverse = (-0.5f + 0.1f * (float)step + 3.0f) /
                  (maximum - minimum);
        for (index = 0u; index < 16u; ++index) {
            float weight = fabsf(source[index]);
            int code = quant_nearest_even(
                inverse * (source[index] - minimum));
            if (code < 0) code = 0;
            if (code > 3) code = 3;
            candidate[index] = (unsigned char)code;
            code_sum += weight * code;
            code_squared_sum += weight * code * code;
            source_code_sum += weight * code * source[index];
        }
        determinant = weight_sum * code_squared_sum - code_sum * code_sum;
        if (!(determinant > 0.0f)) continue;
        candidate_scale =
            (weight_sum * source_code_sum - weighted_source_sum * code_sum) /
            determinant;
        candidate_minimum =
            (code_squared_sum * weighted_source_sum -
             code_sum * source_code_sum) / determinant;
        if (candidate_minimum > 0.0f) {
            candidate_minimum = 0.0f;
            candidate_scale = code_squared_sum > 0.0f
                ? source_code_sum / code_squared_sum : 0.0f;
        }
        for (index = 0u; index < 16u; ++index) {
            float difference = fabsf(
                candidate_scale * candidate[index] + candidate_minimum -
                source[index]);
            candidate_error += fabsf(source[index]) * difference;
        }
        if (candidate_error < best_error) {
            memcpy(codes, candidate, 16u);
            best_error = candidate_error;
            scale = candidate_scale;
            minimum = candidate_minimum;
        }
    }
    *minimum_out = -minimum;
    return scale;
}

static int quant_encode_mxfp4(const float *source, unsigned char *encoded)
{
    float maximum = 0.0f;
    float scale;
    unsigned int index;

    for (index = 0u; index < YVEX_QUANT_MXFP4_ELEMENTS; ++index) {
        float magnitude = fabsf(source[index]);
        if (magnitude > maximum) maximum = magnitude;
    }
    encoded[0] = quant_mxfp4_exponent(maximum);
    scale = yvex_quant_e8m0_decode(encoded[0]) * 0.5f;
    if (!isfinite(scale)) return 0;
    for (index = 0u; index < 16u; ++index) {
        unsigned int low = quant_mxfp4_best(source[index], scale);
        unsigned int high = quant_mxfp4_best(source[index + 16u], scale);
        encoded[1u + index] = (unsigned char)(low | (high << 4));
    }
    return 1;
}

/* Encodes one pinned Q2_K block with 16 independently affine sub-blocks. */
static int quant_encode_q2_k(const float *source, unsigned char *encoded)
{
    float scales[16];
    float minima[16];
    unsigned char quants[256];
    float maximum_scale = 0.0f;
    float maximum_minimum = 0.0f;
    float global_scale;
    float global_minimum;
    unsigned int subblock;
    unsigned int index;

    memset(encoded, 0, YVEX_QUANT_Q2_K_BYTES);
    for (subblock = 0u; subblock < 16u; ++subblock) {
        scales[subblock] = quant_q2_subblock(
            source + subblock * 16u, quants + subblock * 16u,
            &minima[subblock]);
        if (scales[subblock] > maximum_scale)
            maximum_scale = scales[subblock];
        if (minima[subblock] > maximum_minimum)
            maximum_minimum = minima[subblock];
    }
    global_scale = maximum_scale / 15.0f;
    global_minimum = maximum_minimum / 15.0f;
    quant_store_u16(encoded + 80u, yvex_quant_f16_encode(global_scale));
    quant_store_u16(encoded + 82u, yvex_quant_f16_encode(global_minimum));
    global_scale = yvex_quant_f16_decode(quant_load_u16(encoded + 80u));
    global_minimum = yvex_quant_f16_decode(quant_load_u16(encoded + 82u));
    if (!isfinite(global_scale) || !isfinite(global_minimum) ||
        global_scale < 0.0f || global_minimum < 0.0f ||
        (maximum_scale > 0.0f && global_scale == 0.0f) ||
        (maximum_minimum > 0.0f && global_minimum == 0.0f)) return 0;
    for (subblock = 0u; subblock < 16u; ++subblock) {
        int scale_code = global_scale > 0.0f
            ? quant_nearest_even(scales[subblock] / global_scale) : 0;
        int minimum_code = global_minimum > 0.0f
            ? quant_nearest_even(minima[subblock] / global_minimum) : 0;
        float scale;
        float minimum;

        if (scale_code < 0) scale_code = 0;
        if (scale_code > 15) scale_code = 15;
        if (minimum_code < 0) minimum_code = 0;
        if (minimum_code > 15) minimum_code = 15;
        encoded[subblock] = (unsigned char)(scale_code | (minimum_code << 4));
        scale = global_scale * (float)scale_code;
        minimum = global_minimum * (float)minimum_code;
        for (index = 0u; index < 16u; ++index) {
            int code = scale > 0.0f
                ? quant_nearest_even(
                    (source[subblock * 16u + index] + minimum) / scale)
                : 0;
            if (code < 0) code = 0;
            if (code > 3) code = 3;
            quants[subblock * 16u + index] = (unsigned char)code;
        }
    }
    for (index = 0u; index < 256u; index += 128u) {
        unsigned int lane;
        for (lane = 0u; lane < 32u; ++lane) {
            encoded[16u + index / 4u + lane] =
                (unsigned char)(quants[index + lane] |
                    (quants[index + lane + 32u] << 2) |
                    (quants[index + lane + 64u] << 4) |
                    (quants[index + lane + 96u] << 6));
        }
    }
    return 1;
}

/* Encodes exactly one scalar or canonical qtype block into caller-owned bytes. */
int yvex_quant_encode_block(unsigned int qtype,
                            const float *source,
                            unsigned long long elements,
                            unsigned char *encoded,
                            size_t encoded_capacity,
                            size_t *encoded_bytes,
                            yvex_quant_failure *failure,
                            yvex_error *err)
{
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_at(qtype);
    unsigned long long bad = ULLONG_MAX;
    unsigned long long required_elements = 0u;
    size_t required_bytes = 0u;

    if (encoded_bytes) *encoded_bytes = 0u;
    if (!source || !encoded || !encoded_bytes) {
        quant_block_fail(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, qtype,
                         1u, 0u, err, YVEX_ERR_INVALID_ARG,
                         "source, encoded buffer, and byte output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!capability) {
        quant_block_fail(failure, YVEX_QUANT_FAILURE_UNKNOWN_QTYPE, qtype,
                         YVEX_GGUF_QTYPE_ABI_UPSTREAM_MAX_ID, qtype, err,
                         YVEX_ERR_UNSUPPORTED, "qtype identity is unknown");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!capability->encoder_available) {
        yvex_quant_failure_code code = capability->refusal ==
                YVEX_QUANT_REFUSAL_REMOVED_IDENTITY
            ? YVEX_QUANT_FAILURE_REMOVED_QTYPE
            : capability->refusal ==
                    YVEX_QUANT_REFUSAL_OUTSIDE_PINNED_BASELINE
                ? YVEX_QUANT_FAILURE_QTYPE_OUTSIDE_BASELINE
                : YVEX_QUANT_FAILURE_ENCODER_UNAVAILABLE;
        quant_block_fail(failure, code, qtype, 1u, 0u, err,
                         YVEX_ERR_UNSUPPORTED,
                         "qtype has no canonical encoder");
        return YVEX_ERR_UNSUPPORTED;
    }
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32:
    case YVEX_GGUF_QTYPE_I32:
        required_elements = 1u; required_bytes = 4u; break;
    case YVEX_GGUF_QTYPE_F16:
    case YVEX_GGUF_QTYPE_BF16:
        required_elements = 1u; required_bytes = 2u; break;
    case YVEX_GGUF_QTYPE_Q8_0:
        required_elements = YVEX_QUANT_Q8_0_ELEMENTS;
        required_bytes = YVEX_QUANT_Q8_0_BYTES; break;
    case YVEX_GGUF_QTYPE_MXFP4:
        required_elements = YVEX_QUANT_MXFP4_ELEMENTS;
        required_bytes = YVEX_QUANT_MXFP4_BYTES; break;
    case YVEX_GGUF_QTYPE_Q2_K:
        required_elements = YVEX_QUANT_Q2_K_ELEMENTS;
        required_bytes = YVEX_QUANT_Q2_K_BYTES; break;
    default:
        required_elements = 0u; break;
    }
    if (elements != required_elements || encoded_capacity < required_bytes) {
        quant_block_fail(failure,
                         elements != required_elements
                             ? YVEX_QUANT_FAILURE_ROW_DIVISIBILITY
                             : YVEX_QUANT_FAILURE_BYTE_OVERFLOW,
                         qtype, elements != required_elements
                             ? required_elements : required_bytes,
                         elements != required_elements
                             ? elements : encoded_capacity,
                         err, YVEX_ERR_BOUNDS,
                         "qtype block arity or destination capacity mismatch");
        return YVEX_ERR_BOUNDS;
    }
    if (!quant_values_finite(source, elements, &bad)) {
        quant_block_fail(failure, YVEX_QUANT_FAILURE_NONFINITE, qtype,
                         0u, bad, err, YVEX_ERR_FORMAT,
                         "non-finite source value is forbidden by the profile");
        return YVEX_ERR_FORMAT;
    }
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32: {
        unsigned int bits;
        memcpy(&bits, source, sizeof(bits));
        quant_store_u32(encoded, bits);
        break;
    }
    case YVEX_GGUF_QTYPE_F16:
        quant_store_u16(encoded, yvex_quant_f16_encode(source[0]));
        break;
    case YVEX_GGUF_QTYPE_BF16:
        quant_store_u16(encoded, yvex_quant_bf16_encode(source[0]));
        break;
    case YVEX_GGUF_QTYPE_I32: {
        double rounded = nearbyint((double)source[0]);
        int32_t integer;
        if (rounded < INT32_MIN || rounded > INT32_MAX ||
            rounded != (double)source[0]) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_CAST_RANGE, qtype,
                             0u, 1u, err, YVEX_ERR_BOUNDS,
                             "I32 encoding requires an exact in-range integer");
            return YVEX_ERR_BOUNDS;
        }
        integer = (int32_t)rounded;
        quant_store_u32(encoded, (unsigned int)integer);
        break;
    }
    case YVEX_GGUF_QTYPE_Q8_0:
        if (!quant_encode_q8_0(source, encoded)) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_Q8_0_BLOCK, qtype,
                             0u, 1u, err, YVEX_ERR_BOUNDS,
                             "Q8_0 scale is not representable as finite F16");
            return YVEX_ERR_BOUNDS;
        }
        break;
    case YVEX_GGUF_QTYPE_MXFP4:
        if (!quant_encode_mxfp4(source, encoded)) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_MXFP4_BLOCK, qtype,
                             0u, 1u, err, YVEX_ERR_BOUNDS,
                             "MXFP4 scale is not representable as finite E8M0");
            return YVEX_ERR_BOUNDS;
        }
        break;
    case YVEX_GGUF_QTYPE_Q2_K:
        if (!quant_encode_q2_k(source, encoded)) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_Q2_K_BLOCK, qtype,
                             0u, 1u, err, YVEX_ERR_BOUNDS,
                             "Q2_K affine scales are not finite F16 values");
            return YVEX_ERR_BOUNDS;
        }
        break;
    default:
        return YVEX_ERR_UNSUPPORTED;
    }
    *encoded_bytes = required_bytes;
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

static void quant_decode_q2_k(const unsigned char *encoded, float *out)
{
    float global_scale = yvex_quant_f16_decode(quant_load_u16(encoded + 80u));
    float global_minimum = yvex_quant_f16_decode(quant_load_u16(encoded + 82u));
    const unsigned char *quants = encoded + 16u;
    unsigned int subblock = 0u;
    unsigned int half;

    for (half = 0u; half < 2u; ++half) {
        unsigned int shift = 0u;
        unsigned int group;
        for (group = 0u; group < 4u; ++group) {
            unsigned int pair;
            for (pair = 0u; pair < 2u; ++pair) {
                unsigned char scale_byte = encoded[subblock];
                float scale = global_scale * (float)(scale_byte & 0x0fu);
                float minimum = global_minimum * (float)(scale_byte >> 4);
                unsigned int lane;
                unsigned int quant_offset = pair * 16u;
                for (lane = 0u; lane < 16u; ++lane) {
                    unsigned int code =
                        (quants[quant_offset + lane] >> shift) & 3u;
                    out[subblock * 16u + lane] =
                        scale * (float)code - minimum;
                }
                subblock++;
            }
            shift += 2u;
        }
        quants += 32u;
    }
}

/* Reference-decodes one exact scalar or qtype block without shared encode helpers. */
int yvex_quant_decode_block(unsigned int qtype,
                            const unsigned char *encoded,
                            size_t encoded_bytes,
                            float *out,
                            unsigned long long out_elements,
                            yvex_quant_failure *failure,
                            yvex_error *err)
{
    const yvex_quant_numeric_capability *capability =
        yvex_quant_numeric_capability_at(qtype);
    size_t required_bytes = 0u;
    unsigned long long required_elements = 0u;
    unsigned int index;

    if (!encoded || !out) {
        quant_block_fail(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT, qtype,
                         1u, 0u, err, YVEX_ERR_INVALID_ARG,
                         "encoded block and decode output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!capability || !capability->reference_decoder_available) {
        quant_block_fail(failure,
                         capability ? YVEX_QUANT_FAILURE_DECODER_UNAVAILABLE
                                    : YVEX_QUANT_FAILURE_UNKNOWN_QTYPE,
                         qtype, 1u, 0u, err, YVEX_ERR_UNSUPPORTED,
                         "qtype has no canonical reference decoder");
        return YVEX_ERR_UNSUPPORTED;
    }
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32:
    case YVEX_GGUF_QTYPE_I32:
        required_bytes = 4u; required_elements = 1u; break;
    case YVEX_GGUF_QTYPE_F16:
    case YVEX_GGUF_QTYPE_BF16:
        required_bytes = 2u; required_elements = 1u; break;
    case YVEX_GGUF_QTYPE_Q8_0:
        required_bytes = YVEX_QUANT_Q8_0_BYTES;
        required_elements = YVEX_QUANT_Q8_0_ELEMENTS; break;
    case YVEX_GGUF_QTYPE_MXFP4:
        required_bytes = YVEX_QUANT_MXFP4_BYTES;
        required_elements = YVEX_QUANT_MXFP4_ELEMENTS; break;
    case YVEX_GGUF_QTYPE_Q2_K:
        required_bytes = YVEX_QUANT_Q2_K_BYTES;
        required_elements = YVEX_QUANT_Q2_K_ELEMENTS; break;
    default: break;
    }
    if (encoded_bytes != required_bytes || out_elements != required_elements) {
        quant_block_fail(failure,
                         qtype == YVEX_GGUF_QTYPE_Q8_0
                             ? YVEX_QUANT_FAILURE_Q8_0_BLOCK
                             : qtype == YVEX_GGUF_QTYPE_Q2_K
                                 ? YVEX_QUANT_FAILURE_Q2_K_BLOCK
                                 : qtype == YVEX_GGUF_QTYPE_MXFP4
                                     ? YVEX_QUANT_FAILURE_MXFP4_BLOCK
                                     : YVEX_QUANT_FAILURE_BYTE_OVERFLOW,
                         qtype, required_bytes, encoded_bytes, err,
                         YVEX_ERR_FORMAT,
                         "encoded qtype block has an exact-size mismatch");
        return YVEX_ERR_FORMAT;
    }
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32: {
        unsigned int bits = quant_load_u32(encoded);
        memcpy(out, &bits, sizeof(*out));
        break;
    }
    case YVEX_GGUF_QTYPE_F16:
        out[0] = yvex_quant_f16_decode(quant_load_u16(encoded)); break;
    case YVEX_GGUF_QTYPE_BF16:
        out[0] = yvex_quant_bf16_decode(quant_load_u16(encoded)); break;
    case YVEX_GGUF_QTYPE_I32:
        out[0] = (float)quant_i32_from_u32(quant_load_u32(encoded)); break;
    case YVEX_GGUF_QTYPE_Q8_0: {
        float scale = yvex_quant_f16_decode(quant_load_u16(encoded));
        if (!isfinite(scale) || scale < 0.0f) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_Q8_0_BLOCK,
                             qtype, 0u, quant_load_u16(encoded), err,
                             YVEX_ERR_FORMAT,
                             "Q8_0 encoded scale is negative or non-finite");
            return YVEX_ERR_FORMAT;
        }
        for (index = 0u; index < YVEX_QUANT_Q8_0_ELEMENTS; ++index) {
            int quantized = encoded[2u + index] <= 127u
                ? encoded[2u + index] : (int)encoded[2u + index] - 256;
            out[index] = scale * (float)quantized;
        }
        break;
    }
    case YVEX_GGUF_QTYPE_MXFP4: {
        float scale = yvex_quant_e8m0_decode(encoded[0]) * 0.5f;
        if (!isfinite(scale)) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_MXFP4_BLOCK,
                             qtype, 0xfeu, encoded[0], err, YVEX_ERR_FORMAT,
                             "MXFP4 encoded scale is non-finite");
            return YVEX_ERR_FORMAT;
        }
        for (index = 0u; index < 16u; ++index) {
            out[index] = quant_mxfp4_values[encoded[1u + index] & 0x0fu] * scale;
            out[index + 16u] =
                quant_mxfp4_values[encoded[1u + index] >> 4] * scale;
            if (!isfinite(out[index]) || !isfinite(out[index + 16u])) {
                quant_block_fail(failure, YVEX_QUANT_FAILURE_MXFP4_BLOCK,
                                 qtype, 0u, index, err, YVEX_ERR_FORMAT,
                                 "MXFP4 block reconstructs a non-finite value");
                return YVEX_ERR_FORMAT;
            }
        }
        break;
    }
    case YVEX_GGUF_QTYPE_Q2_K: {
        float scale = yvex_quant_f16_decode(
            quant_load_u16(encoded + 80u));
        float minimum = yvex_quant_f16_decode(
            quant_load_u16(encoded + 82u));
        if (!isfinite(scale) || !isfinite(minimum) ||
            scale < 0.0f || minimum < 0.0f) {
            quant_block_fail(failure, YVEX_QUANT_FAILURE_Q2_K_BLOCK,
                             qtype, 0u, 1u, err, YVEX_ERR_FORMAT,
                             "Q2_K encoded affine scales are malformed");
            return YVEX_ERR_FORMAT;
        }
        quant_decode_q2_k(encoded, out);
        break;
    }
    default:
        return YVEX_ERR_UNSUPPORTED;
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}
