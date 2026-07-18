/*
 * yvex_deepseek_attention_numeric.c - DeepSeek attention numeric policy primitives.
 *
 * Owner:
 *   src/graph
 *
 * Owns:
 *   runtime-attention numeric primitives consumed by the DeepSeek attention
 *   executor: pinned Hadamard semantics, deterministic sparse top-k ordering,
 *   and bounded activation-codec helpers.
 *
 * Does not own:
 *   source tensor quantization, GGUF qtype storage geometry, materialization,
 *   persistent KV, CUDA kernels, transformer execution, generation, CLI
 *   output, or release claims.
 *
 * Invariants:
 *   numeric policy values arrive from the immutable attention plan; no source
 *   quantization field is reused as runtime activation policy. Independent
 *   reference algorithms live only under tests/reference.
 *
 * Boundary:
 *   these primitives make the attention executor reproducible; they do not by
 *   themselves establish attention execution support.
 */
#include "yvex_deepseek_attention_internal.h"

typedef struct {
    float score;
    unsigned long long ordinal;
    unsigned long long index;
} attention_topk_candidate;

static int attention_next_power_of_two(unsigned long long value,
                                       unsigned long long *out)
{
    unsigned long long power = 1ull;

    if (!out || value == 0ull) return 0;
    while (power < value) {
        if (power > ULLONG_MAX / 2ull) return 0;
        power *= 2ull;
    }
    *out = power;
    return 1;
}

static int attention_score_equal(float left, float right)
{
    if (left == 0.0f && right == 0.0f) return 1;
    return left == right;
}

static int attention_candidate_before(const attention_topk_candidate *left,
                                      const attention_topk_candidate *right)
{
    if (!attention_score_equal(left->score, right->score))
        return left->score > right->score;
    return left->ordinal < right->ordinal;
}

static int attention_candidate_ordinal_compare(const void *left,
                                               const void *right)
{
    const attention_topk_candidate *a =
        (const attention_topk_candidate *)left;
    const attention_topk_candidate *b =
        (const attention_topk_candidate *)right;

    if (a->ordinal < b->ordinal) return -1;
    if (a->ordinal > b->ordinal) return 1;
    return 0;
}

static int attention_candidate_rank_compare(const void *left,
                                            const void *right)
{
    const attention_topk_candidate *a =
        (const attention_topk_candidate *)left;
    const attention_topk_candidate *b =
        (const attention_topk_candidate *)right;

    if (attention_candidate_before(a, b)) return -1;
    if (attention_candidate_before(b, a)) return 1;
    return 0;
}

static float attention_power_of_two_ceil(float value)
{
    int exponent;
    float mantissa;

    if (!isfinite(value) || value <= 0.0f) return 0.0f;
    mantissa = frexpf(value, &exponent);
    if (mantissa > 0.5f) exponent++;
    return ldexpf(1.0f, exponent - 1);
}

/* Contract: production CPU FHT over the final logical row; scratch is bounded
 * to the padded power-of-two row and no result is published on failure. */
int yvex_deepseek_attention_hadamard_cpu(
    const float *input,
    unsigned long long length,
    float scale,
    int reject_nonfinite,
    float *output,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    float *scratch;
    unsigned long long padded_length;
    unsigned long long i;
    unsigned long long step;

    if (!input || !output || length == 0ull ||
        !attention_next_power_of_two(length, &padded_length)) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "Hadamard CPU requires non-empty input and output");
    }
    scratch = (float *)attention_calloc_array(padded_length, sizeof(*scratch));
    if (!scratch)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            padded_length, 0ull, err, YVEX_ERR_NOMEM,
            "Hadamard CPU scratch allocation failed");
    for (i = 0ull; i < length; ++i) {
        if (reject_nonfinite && !isfinite(input[i])) {
            free(scratch);
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
                0ull, err, YVEX_ERR_FORMAT,
                "Hadamard CPU refuses non-finite input");
        }
        scratch[i] = input[i];
    }
    for (step = 1ull; step < padded_length; step *= 2ull) {
        unsigned long long block;
        for (block = 0ull; block < padded_length; block += step * 2ull) {
            unsigned long long lane;
            for (lane = 0ull; lane < step; ++lane) {
                float left = scratch[block + lane];
                float right = scratch[block + lane + step];
                scratch[block + lane] = left + right;
                scratch[block + lane + step] = left - right;
            }
        }
    }
    for (i = 0ull; i < length; ++i)
        output[i] = scratch[i] * scale;
    free(scratch);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: deterministic YVEX sparse top-k order; refuses non-finite scores
 * and duplicate ordinals before publishing selection. */
int yvex_deepseek_attention_topk_select(
    const float *scores,
    const unsigned long long *ordinals,
    unsigned long long candidate_count,
    unsigned long long k,
    unsigned long long *selected_indices,
    unsigned long long *selected_count,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    attention_topk_candidate *candidates;
    unsigned long long i;
    unsigned long long out_count;

    if (selected_count) *selected_count = 0ull;
    if (!scores || !ordinals || !selected_indices || !selected_count) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "top-k selection requires scores, ordinals, and output");
    }
    if (candidate_count == 0ull || k == 0ull) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    candidates = (attention_topk_candidate *)attention_calloc_array(
        candidate_count, sizeof(*candidates));
    if (!candidates)
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            candidate_count, 0ull, err, YVEX_ERR_NOMEM,
            "top-k candidate allocation failed");
    for (i = 0ull; i < candidate_count; ++i) {
        if (!isfinite(scores[i])) {
            free(candidates);
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                candidate_count, i, err, YVEX_ERR_FORMAT,
                "top-k refuses non-finite score");
        }
        candidates[i].score = scores[i];
        candidates[i].ordinal = ordinals[i];
        candidates[i].index = i;
    }
    qsort(candidates, (size_t)candidate_count, sizeof(*candidates),
          attention_candidate_ordinal_compare);
    for (i = 1ull; i < candidate_count; ++i) {
        if (candidates[i - 1ull].ordinal == candidates[i].ordinal) {
            free(candidates);
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                candidate_count, i, err, YVEX_ERR_FORMAT,
                "top-k refuses duplicate candidate ordinal");
        }
    }
    qsort(candidates, (size_t)candidate_count, sizeof(*candidates),
          attention_candidate_rank_compare);
    out_count = attention_min_u64(candidate_count, k);
    for (i = 0ull; i < out_count; ++i)
        selected_indices[i] = candidates[i].index;
    *selected_count = out_count;
    free(candidates);
    yvex_error_clear(err);
    return YVEX_OK;
}

unsigned char yvex_deepseek_attention_ue8m0_encode_scale(float value)
{
    int exponent;
    float mantissa;

    if (!isfinite(value) || value <= 0.0f) return 0u;
    mantissa = frexpf(value, &exponent);
    if (mantissa > 0.5f) exponent++;
    exponent += 126;
    if (exponent < 0) return 0u;
    if (exponent > 255) return 255u;
    return (unsigned char)exponent;
}

float yvex_deepseek_attention_ue8m0_decode_scale(unsigned char code)
{
    if (code == 0u) return 0.0f;
    return ldexpf(1.0f, (int)code - 127);
}

unsigned char yvex_deepseek_attention_fp8_e4m3fn_encode(float value)
{
    static const float finite_max = 448.0f;
    float best_error = INFINITY;
    unsigned char best = 0u;
    unsigned int code;

    if (!isfinite(value)) return 0u;
    if (value > finite_max) value = finite_max;
    if (value < -finite_max) value = -finite_max;
    for (code = 0u; code < 256u; ++code) {
        float decoded = yvex_deepseek_attention_fp8_e4m3fn_decode(
            (unsigned char)code);
        float error = fabsf(decoded - value);
        if (error < best_error) {
            best_error = error;
            best = (unsigned char)code;
        }
    }
    return best;
}

float yvex_deepseek_attention_fp8_e4m3fn_decode(unsigned char code)
{
    unsigned char sign = (unsigned char)(code & 0x80u);
    unsigned char exponent = (unsigned char)((code >> 3u) & 0x0fu);
    unsigned char mantissa = (unsigned char)(code & 0x07u);
    float value;

    if ((code & 0x7fu) == 0u) return sign ? -0.0f : 0.0f;
    if (exponent == 0u) {
        value = ldexpf((float)mantissa / 8.0f, -6);
    } else {
        value = ldexpf(1.0f + (float)mantissa / 8.0f,
                       (int)exponent - 7);
    }
    if (value > 448.0f) value = 448.0f;
    return sign ? -value : value;
}

/* Contract: applies the pinned FP8 fake-quant policy used by act_quant(...,
 * scale_fmt, scale_dtype, inplace=True). The scale follows the local kernel:
 * max(abs(x), 1e-4) / 448 rounded up to a power-of-two when scale_fmt is set. */
int yvex_deepseek_attention_fp8_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long i;
    float amax = 1.0e-4f;
    float scale;

    if (!input || !dequantized || !codes || !scale_code || count == 0ull) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "FP8 fake quant requires input, output, code, and scale buffers");
    }
    for (i = 0ull; i < count; ++i) {
        float magnitude;
        if (!isfinite(input[i])) {
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                count, i, err, YVEX_ERR_FORMAT,
                "FP8 fake quant refuses non-finite activation");
        }
        magnitude = fabsf(input[i]);
        if (magnitude > amax) amax = magnitude;
    }
    scale = attention_power_of_two_ceil(amax / 448.0f);
    *scale_code = yvex_deepseek_attention_ue8m0_encode_scale(scale);
    scale = yvex_deepseek_attention_ue8m0_decode_scale(*scale_code);
    if (!isfinite(scale) || scale <= 0.0f) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            count, 0ull, err, YVEX_ERR_FORMAT,
            "FP8 fake quant produced invalid UE8M0 scale");
    }
    for (i = 0ull; i < count; ++i) {
        float normalized = input[i] / scale;
        if (normalized > 448.0f) normalized = 448.0f;
        if (normalized < -448.0f) normalized = -448.0f;
        codes[i] = yvex_deepseek_attention_fp8_e4m3fn_encode(normalized);
        dequantized[i] =
            yvex_deepseek_attention_fp8_e4m3fn_decode(codes[i]) * scale;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

unsigned char yvex_deepseek_attention_fp4_e2m1_encode(float value)
{
    static const float thresholds[] = {
        0.25f, 0.75f, 1.25f, 1.75f, 2.5f, 3.5f, 5.0f
    };
    float magnitude = fabsf(value);
    unsigned char code = 0u;
    unsigned int i;

    for (i = 0u; i < sizeof(thresholds) / sizeof(thresholds[0]); ++i)
        if (magnitude > thresholds[i]) code++;
    if (code > 7u) code = 7u;
    if (code != 0u && signbit(value)) code |= 0x8u;
    return code;
}

float yvex_deepseek_attention_fp4_e2m1_decode(unsigned char code)
{
    static const float values[] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f
    };
    unsigned char magnitude = (unsigned char)(code & 0x7u);
    float value = values[magnitude];

    return (code & 0x8u) ? -value : value;
}

/* Contract: applies the pinned runtime FP4 fake-quant block policy and
 * publishes dequantized activations only after every input is admitted. */
int yvex_deepseek_attention_fp4_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_deepseek_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long i;
    float amax = 0.0f;
    float scale;

    if (!input || !dequantized || !codes || !scale_code || count == 0ull) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "FP4 fake quant requires input, output, code, and scale buffers");
    }
    *scale_code = 0u;
    for (i = 0ull; i < count; ++i) {
        float magnitude;
        if (!isfinite(input[i])) {
            return attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                count, i, err, YVEX_ERR_FORMAT,
                "FP4 fake quant refuses non-finite activation");
        }
        magnitude = fabsf(input[i]);
        if (magnitude > amax) amax = magnitude;
    }
    if (amax < 6.0f * ldexpf(1.0f, -126)) {
        amax = 6.0f * ldexpf(1.0f, -126);
    }
    *scale_code = yvex_deepseek_attention_ue8m0_encode_scale(
        attention_power_of_two_ceil(amax / 6.0f));
    scale = yvex_deepseek_attention_ue8m0_decode_scale(*scale_code);
    if (!isfinite(scale) || scale <= 0.0f) {
        return attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            YVEX_DEEPSEEK_V4_IR_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            count, 0ull, err, YVEX_ERR_FORMAT,
            "FP4 fake quant produced invalid UE8M0 scale");
    }
    for (i = 0ull; i < count; ++i) {
        codes[i] = yvex_deepseek_attention_fp4_e2m1_encode(input[i] / scale);
        dequantized[i] =
            yvex_deepseek_attention_fp4_e2m1_decode(codes[i]) * scale;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
