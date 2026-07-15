/*
 * yvex_quant_scalar.c - canonical scalar and source-format decoding.
 *
 * Owner: TRACK.QUANT.
 * Owns: endian-stable F16/BF16/FP8/E8M0 scalar semantics and source MXFP4
 *   pair decoding used by transformation execution.
 * Does not own: qtype geometry, block encoding, profile selection, payload IO,
 *   artifact writing, backend kernels, or rendering.
 * Invariants: F16/BF16 encode round to nearest, ties to even; signed zero,
 *   subnormals, infinities, and quiet NaNs have explicit deterministic policy.
 * Boundary: decoding a bounded source value is not quantization completion.
 */
#include "yvex_quant_numeric.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

static void quant_scalar_failure(yvex_quant_failure *failure,
                                 yvex_quant_failure_code code,
                                 unsigned long long expected,
                                 unsigned long long actual,
                                 yvex_error *err,
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
        failure->qtype = UINT_MAX;
        failure->operation = YVEX_TRANSFORM_OP_COUNT;
    }
    yvex_error_set(err,
                   code == YVEX_QUANT_FAILURE_INVALID_ARGUMENT
                       ? YVEX_ERR_INVALID_ARG : YVEX_ERR_FORMAT,
                   "quant.scalar", message);
}

static unsigned short quant_load_u16(const unsigned char *bytes)
{
    return (unsigned short)((unsigned short)bytes[0] |
                            ((unsigned short)bytes[1] << 8));
}

static unsigned int quant_load_u32(const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
           ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) |
           ((unsigned int)bytes[3] << 24);
}

/* Interprets canonical two's-complement I32 bytes without narrowing overflow. */
static int32_t quant_i32_from_u32(uint32_t value)
{
    return value <= (uint32_t)INT32_MAX
        ? (int32_t)value
        : -1 - (int32_t)(UINT32_MAX - value);
}

/* Converts IEEE binary16 bits to binary32 without host-endian assumptions. */
float yvex_quant_f16_decode(unsigned short bits)
{
    unsigned int sign = ((unsigned int)bits & 0x8000u) << 16;
    unsigned int exponent = ((unsigned int)bits >> 10) & 0x1fu;
    unsigned int mantissa = (unsigned int)bits & 0x03ffu;
    unsigned int out_bits;
    float out;

    if (exponent == 0u) {
        if (mantissa == 0u) {
            out_bits = sign;
        } else {
            unsigned int shift = 0u;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1;
                shift++;
            }
            mantissa &= 0x03ffu;
            out_bits = sign | ((113u - shift) << 23) | (mantissa << 13);
        }
    } else if (exponent == 0x1fu) {
        out_bits = sign | 0x7f800000u | (mantissa << 13);
        if (mantissa != 0u) out_bits |= 0x00400000u;
    } else {
        out_bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }
    memcpy(&out, &out_bits, sizeof(out));
    return out;
}

/* Encodes binary32 as IEEE binary16 using nearest-even and finite overflow to infinity. */
unsigned short yvex_quant_f16_encode(float value)
{
    unsigned int bits;
    unsigned int sign;
    unsigned int exponent;
    unsigned int mantissa;
    unsigned int result;

    memcpy(&bits, &value, sizeof(bits));
    sign = (bits >> 16) & 0x8000u;
    exponent = (bits >> 23) & 0xffu;
    mantissa = bits & 0x007fffffu;
    if (exponent == 0xffu) {
        if (mantissa == 0u) return (unsigned short)(sign | 0x7c00u);
        return (unsigned short)(sign | 0x7e00u | ((mantissa >> 13) & 0x01ffu));
    }
    if (exponent > 142u) return (unsigned short)(sign | 0x7c00u);
    if (exponent < 113u) {
        unsigned int significand;
        unsigned int shift;
        unsigned int quotient;
        unsigned int remainder;
        unsigned int halfway;

        if (exponent < 102u) return (unsigned short)sign;
        significand = mantissa | 0x00800000u;
        shift = 126u - exponent;
        quotient = significand >> shift;
        remainder = significand & ((1u << shift) - 1u);
        halfway = 1u << (shift - 1u);
        if (remainder > halfway ||
            (remainder == halfway && (quotient & 1u))) quotient++;
        return (unsigned short)(sign | quotient);
    }
    result = (exponent - 112u) << 10;
    result |= mantissa >> 13;
    {
        unsigned int remainder = mantissa & 0x1fffu;
        if (remainder > 0x1000u ||
            (remainder == 0x1000u && (result & 1u))) result++;
    }
    if ((result & 0x7c00u) == 0x7c00u) result &= 0x7c00u;
    return (unsigned short)(sign | result);
}

float yvex_quant_bf16_decode(unsigned short bits)
{
    unsigned int value = (unsigned int)bits << 16;
    float out;
    memcpy(&out, &value, sizeof(out));
    return out;
}

/* Encodes binary32 as BF16 using nearest-even while preserving quiet NaNs. */
unsigned short yvex_quant_bf16_encode(float value)
{
    unsigned int bits;
    unsigned int upper;
    unsigned int lower;

    memcpy(&bits, &value, sizeof(bits));
    upper = bits >> 16;
    lower = bits & 0xffffu;
    if ((bits & 0x7f800000u) == 0x7f800000u &&
        (bits & 0x007fffffu) != 0u)
        return (unsigned short)(upper | 0x0040u);
    if (lower > 0x8000u || (lower == 0x8000u && (upper & 1u))) upper++;
    return (unsigned short)upper;
}

/* Decodes the pinned torch float8_e4m3fn representation including NaNs. */
float yvex_quant_fp8_e4m3fn_decode(unsigned char bits)
{
    unsigned int magnitude = bits & 0x7fu;
    int sign = (bits & 0x80u) != 0u;
    unsigned int exponent;
    unsigned int mantissa;
    float value;

    if (magnitude == 0u) return sign ? -0.0f : 0.0f;
    if (magnitude == 0x7fu) return NAN;
    exponent = (bits >> 3) & 0x0fu;
    mantissa = bits & 0x07u;
    value = exponent == 0u
        ? ldexpf((float)mantissa, -9)
        : ldexpf(1.0f + (float)mantissa / 8.0f, (int)exponent - 7);
    return sign ? -value : value;
}

/* Decodes torch float8_e8m0fnu; 0xff is the unique non-finite code. */
float yvex_quant_e8m0_decode(unsigned char bits)
{
    unsigned int raw;
    float out;

    if (bits == 0xffu) return NAN;
    raw = bits == 0u ? 0x00400000u : (unsigned int)bits << 23;
    memcpy(&out, &raw, sizeof(out));
    return out;
}

/* Decodes one source scalar and refuses storage classes requiring paired context. */
int yvex_quant_source_scalar_decode(yvex_native_dtype dtype,
                                    const unsigned char *source,
                                    float *out,
                                    yvex_quant_failure *failure,
                                    yvex_error *err)
{
    unsigned int bits;

    if (!source || !out) {
        quant_scalar_failure(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT,
                             1u, 0u, err,
                             "source bytes and scalar output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    switch (dtype) {
    case YVEX_NATIVE_DTYPE_F32:
        bits = quant_load_u32(source);
        memcpy(out, &bits, sizeof(*out));
        break;
    case YVEX_NATIVE_DTYPE_F16:
        *out = yvex_quant_f16_decode(quant_load_u16(source));
        break;
    case YVEX_NATIVE_DTYPE_BF16:
        *out = yvex_quant_bf16_decode(quant_load_u16(source));
        break;
    case YVEX_NATIVE_DTYPE_I32: {
        int32_t integer = quant_i32_from_u32(quant_load_u32(source));
        *out = (float)integer;
        break;
    }
    case YVEX_NATIVE_DTYPE_F8_E4M3:
        *out = yvex_quant_fp8_e4m3fn_decode(source[0]);
        break;
    case YVEX_NATIVE_DTYPE_F8_E8M0:
        *out = yvex_quant_e8m0_decode(source[0]);
        break;
    default:
        quant_scalar_failure(failure,
                             YVEX_QUANT_FAILURE_ENCODER_UNAVAILABLE,
                             0u, (unsigned long long)dtype, err,
                             "source dtype requires a typed paired decoder");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Decodes one little-endian I64 exactly without an intermediate float. */
int yvex_quant_source_i64_decode(const unsigned char source[8],
                                 int64_t *out,
                                 yvex_quant_failure *failure,
                                 yvex_error *err)
{
    uint64_t bits = 0u;
    unsigned int index;

    if (!source || !out) {
        quant_scalar_failure(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT,
                             1u, 0u, err,
                             "I64 source bytes and exact output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    for (index = 0u; index < 8u; ++index)
        bits |= (uint64_t)source[index] << (index * 8u);
    *out = bits <= (uint64_t)INT64_MAX
        ? (int64_t)bits
        : -1 - (int64_t)(UINT64_MAX - bits);
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Decodes one source packed-FP4 group whose bytes store adjacent low/high values. */
int yvex_quant_source_mxfp4_decode(const unsigned char packed[16],
                                  unsigned char scale,
                                  float out[32],
                                  yvex_quant_failure *failure,
                                  yvex_error *err)
{
    static const float values[16] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f,
        -0.0f, -0.5f, -1.0f, -1.5f, -2.0f, -3.0f, -4.0f, -6.0f
    };
    float multiplier;
    unsigned int index;

    if (!packed || !out) {
        quant_scalar_failure(failure, YVEX_QUANT_FAILURE_INVALID_ARGUMENT,
                             1u, 0u, err,
                             "packed source block and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    multiplier = yvex_quant_e8m0_decode(scale);
    if (!isfinite(multiplier)) {
        quant_scalar_failure(failure, YVEX_QUANT_FAILURE_MXFP4_BLOCK,
                             0xfeu, scale, err,
                             "MXFP4 source scale is non-finite");
        return YVEX_ERR_FORMAT;
    }
    for (index = 0u; index < 16u; ++index) {
        unsigned int byte = packed[index];
        out[index * 2u] = values[byte & 0x0fu] * multiplier;
        out[index * 2u + 1u] = values[byte >> 4] * multiplier;
    }
    if (failure) memset(failure, 0, sizeof(*failure));
    yvex_error_clear(err);
    return YVEX_OK;
}
