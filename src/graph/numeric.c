/* Owner: generic graph numeric primitives and bounded primitive proofs.
 * Owns: normalization, RoPE/YaRN, Hadamard, top-k, activation codecs, and fixture numeric runners.
 * Does not own: source quantization, qtype geometry, family policy, materialization, CUDA, or generation.
 * Invariants: runtime policy comes from immutable plans and reference algorithms remain test-owned.
 * Boundary: reproducible primitives and fixture proofs do not establish complete attention support.
 * Purpose: provide deterministic scalar mechanisms shared by graph construction and family execution.
 * Inputs: finite bounded arrays, explicit geometry, numeric policies, and typed report sinks.
 * Effects: mutates caller-owned numeric outputs and proof facts only.
 * Failure: invalid geometry, non-finite policy violations, or backend refusal publish no capability. */
#include "src/graph/private.h"

static const double attention_pi =
    3.14159265358979323846264338327950288;

/* Purpose: compare two finite F32 ranges under one deterministic numeric contract.
 * Inputs: equally sized arrays plus finite non-negative absolute and relative tolerances.
 * Effects: replaces caller-owned metrics; performs no allocation or I/O.
 * Failure: invalid geometry refuses; non-finite values produce a typed failed verdict.
 * Boundary: bitwise equality is observed separately from tolerance admission. */
int yvex_graph_f32_compare(const float *left, const float *right,
                           unsigned long long count, double absolute_tolerance,
                           double relative_tolerance, yvex_graph_f32_comparison *out,
                           yvex_error *err)
{
    yvex_graph_f32_comparison next = {0};
    unsigned long long index;
    size_t bytes;

    next.first_failing_coordinate = ULLONG_MAX;
    if (!left || !right || !count || !out || !isfinite(absolute_tolerance) ||
        !isfinite(relative_tolerance) || absolute_tolerance < 0.0 ||
        relative_tolerance < 0.0 || count > SIZE_MAX / sizeof(*left)) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "graph.f32.compare",
                       "finite tolerances and representable non-empty F32 ranges are required");
        return YVEX_ERR_INVALID_ARG;
    }
    bytes = (size_t)count * sizeof(*left);
    next.value_count = count;
    next.within_tolerance = 1;
    next.bitwise_equal = memcmp(left, right, bytes) == 0;
    for (index = 0ull; index < count; ++index) {
        double left_value = left[index];
        double right_value = right[index];
        double absolute;
        double scale;
        double relative;
        double allowed;

        if (!isfinite(left_value) || !isfinite(right_value)) {
            next.nonfinite_value_count++;
            next.within_tolerance = 0;
            if (next.first_failing_coordinate == ULLONG_MAX)
                next.first_failing_coordinate = index;
            continue;
        }
        absolute = fabs(left_value - right_value);
        scale = fmax(fabs(left_value), fabs(right_value));
        relative = scale > 0.0 ? absolute / scale : 0.0;
        allowed = absolute_tolerance + relative_tolerance * scale;
        next.finite_value_count++;
        if (absolute > next.maximum_absolute_error)
            next.maximum_absolute_error = absolute;
        if (relative > next.maximum_relative_error)
            next.maximum_relative_error = relative;
        next.squared_error_sum += absolute * absolute;
        if ((!isfinite(allowed) || absolute > allowed) &&
            next.first_failing_coordinate == ULLONG_MAX) {
            next.first_failing_coordinate = index;
            next.within_tolerance = 0;
        }
    }
    *out = next;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: apply the identity-bearing DeepSeek activation storage boundary.
 * Inputs: one admitted compute contract and finite F32 working values.
 * Effects: rounds values in place to their model-visible storage precision.
 * Failure: unknown contracts or non-finite values refuse without partial success.
 * Boundary: storage rounding only; accumulation remains owned by each operation. */
int yvex_attention_compute_round(yvex_attention_compute_contract contract,
                                 float *values,
                                 unsigned long long count)
{
    unsigned long long i;

    if (contract != YVEX_ATTENTION_COMPUTE_BF16_F32_RNE_V1 ||
        !values || !count)
        return 0;
    for (i = 0ull; i < count; ++i) {
        if (!isfinite(values[i])) return 0;
        values[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(values[i]));
    }
    return 1;
}

// Purpose: Return the admitted rms norm fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
int yvex_attention_rms_norm(float *values,
                                    unsigned long long count,
                                    const float *weights,
                                    double epsilon)
{
    unsigned long long i;
    double mean = 0.0;
    double inv;

    if (!values || !weights || count == 0ull || !isfinite(epsilon) ||
        epsilon <= 0.0)
        return 0;
    for (i = 0ull; i < count; ++i) {
        double v = values[i];
        if (!isfinite(v) || !isfinite(weights[i])) return 0;
        mean += v * v;
    }
    mean /= (double)count;
    inv = 1.0 / sqrt(mean + epsilon);
    if (!isfinite(inv)) return 0;
    for (i = 0ull; i < count; ++i) {
        double v = (double)values[i] * inv * (double)weights[i];
        if (!isfinite(v)) return 0;
        values[i] = (float)v;
    }
    return 1;
}

// Purpose: Return the admitted unit rms norm fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_unit_rms_norm(float *values,
                                         unsigned long long count,
                                         double epsilon)
{
    unsigned long long i;
    double mean = 0.0;
    double inverse;

    if (!values || count == 0ull || !isfinite(epsilon) || epsilon <= 0.0)
        return 0;
    for (i = 0ull; i < count; ++i) {
        double value = values[i];
        if (!isfinite(value)) return 0;
        mean += value * value;
    }
    inverse = 1.0 / sqrt(mean / (double)count + epsilon);
    if (!isfinite(inverse)) return 0;
    for (i = 0ull; i < count; ++i) {
        double value = (double)values[i] * inverse;
        if (!isfinite(value)) return 0;
        values[i] = (float)value;
    }
    return 1;
}

// Purpose: Implement the graph-local yarn frequency semantic operation.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static double attention_yarn_frequency(
    const yvex_attention_position_policy *position,
    unsigned long long pair,
    unsigned long long rope_dims)
{
    double exponent;
    double frequency;

    if (!position || rope_dims < 2ull || position->theta <= 1ull) return 0.0;
    exponent = (double)(pair * 2ull) / (double)rope_dims;
    frequency = 1.0 / pow((double)position->theta, exponent);
    if (position->original_context && position->scaling_factor) {
        double denominator = 2.0 * log((double)position->theta);
        double low_dim = (double)rope_dims *
            log((double)position->original_context /
                ((double)position->beta_fast * 2.0 * attention_pi)) /
            denominator;
        double high_dim = (double)rope_dims *
            log((double)position->original_context /
                ((double)position->beta_slow * 2.0 * attention_pi)) /
            denominator;
        double low = floor(low_dim);
        double high = ceil(high_dim);
        double lane = (double)pair;
        double ramp;
        double smooth;

        if (low < 0.0) low = 0.0;
        if (high > (double)rope_dims - 1.0)
            high = (double)rope_dims - 1.0;
        if (low == high) high += 0.001;
        ramp = (lane - low) / (high - low);
        if (ramp < 0.0) ramp = 0.0;
        if (ramp > 1.0) ramp = 1.0;
        smooth = 1.0 - ramp;
        frequency = frequency / (double)position->scaling_factor *
                        (1.0 - smooth) +
                    frequency * smooth;
    }
    return frequency;
}

// Purpose: Return the admitted rope apply fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_rope_apply(
    float *values,
    unsigned long long count,
    unsigned long long rope_dims,
    unsigned long long token_position,
    const yvex_attention_position_policy *position,
    int inverse)
{
    unsigned long long start;
    unsigned long long i;

    if (!values || count == 0ull || rope_dims < 2ull || rope_dims > count ||
        !position || position->theta <= 1ull)
        return 0;
    if (rope_dims & 1ull) rope_dims--;
    start = count - rope_dims;
    for (i = 0ull; i < rope_dims; i += 2ull) {
        double frequency = attention_yarn_frequency(position, i / 2ull,
                                                    rope_dims);
        double angle = (double)token_position * frequency;
        double c = cos(angle);
        double s = inverse ? -sin(angle) : sin(angle);
        double x = values[start + i];
        double y = values[start + i + 1ull];
        if (!isfinite(x) || !isfinite(y) || !isfinite(c) || !isfinite(s))
            return 0;
        values[start + i] = (float)(x * c - y * s);
        values[start + i + 1ull] = (float)(x * s + y * c);
    }
    return 1;
}

typedef struct {
    float score;
    unsigned long long ordinal;
    unsigned long long index;
} attention_topk_candidate;

// Purpose: Return the admitted next power of two fact without transferring ownership.
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

// Purpose: Apply the checked graph-local score equal invariant.
static int attention_score_equal(float left, float right)
{
    if (left == 0.0f && right == 0.0f) return 1;
    return left == right;
}

// Purpose: Implement the graph-local candidate before semantic operation.
static int attention_candidate_before(const attention_topk_candidate *left,
                                      const attention_topk_candidate *right)
{
    if (!attention_score_equal(left->score, right->score))
        return left->score > right->score;
    return left->ordinal < right->ordinal;
}

// Purpose: Implement the graph-local candidate ordinal compare semantic operation.
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

// Purpose: Implement the graph-local candidate rank compare semantic operation.
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

// Purpose: Return the admitted power of two ceil fact without transferring ownership.
static float attention_power_of_two_ceil(float value)
{
    int exponent;
    float mantissa;

    if (!isfinite(value) || value <= 0.0f) return 0.0f;
    mantissa = frexpf(value, &exponent);
    if (mantissa > 0.5f) exponent++;
    return ldexpf(1.0f, exponent - 1);
}

// Purpose: Return the admitted hadamard cpu fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_hadamard_cpu(
    const float *input,
    unsigned long long length,
    float scale,
    int reject_nonfinite,
    float *output,
    yvex_attention_scratch_budget *budget,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    float *scratch = NULL;
    unsigned long long padded_length;
    unsigned long long i;
    unsigned long long step;
    size_t scratch_bytes = 0u;
    int rc;

    if (!input || !output || length == 0ull ||
        !attention_next_power_of_two(length, &padded_length)) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "Hadamard CPU requires non-empty input and output");
    }
    if (!yvex_attention_scratch_reserve(
            budget, padded_length, sizeof(*scratch), &scratch_bytes))
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_SCRATCH, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            budget ? budget->limit_bytes : 0ull,
            budget ? (unsigned long long)budget->live_bytes : 0ull,
            err, YVEX_ERR_BOUNDS,
            "Hadamard CPU scratch budget exceeded");
    scratch = (float *)yvex_attention_calloc_array(padded_length, sizeof(*scratch));
    if (!scratch)
        rc = yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            padded_length, 0ull, err, YVEX_ERR_NOMEM,
            "Hadamard CPU scratch allocation failed");
    if (!scratch) {
        yvex_attention_scratch_release(budget, scratch_bytes);
        return rc;
    }
    for (i = 0ull; i < length; ++i) {
        if (reject_nonfinite && !isfinite(input[i])) {
            free(scratch);
            yvex_attention_scratch_release(budget, scratch_bytes);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
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
    yvex_attention_scratch_release(budget, scratch_bytes);
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Purpose: select the canonical sparse candidates by score and ordinal.
 * Inputs: finite scores, unique ordinals, bounded k, and writable result facts.
 * Effects: allocates bounded sorting scratch and publishes only a complete selection.
 * Failure: invalid input, non-finite score, duplicate ordinal, or allocation refusal.
 * Boundary: selection is a reusable numeric mechanism, not attention execution. */
// Purpose: Return the admitted topk select fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_topk_select(
    const float *scores,
    const unsigned long long *ordinals,
    unsigned long long candidate_count,
    unsigned long long k,
    unsigned long long *selected_indices,
    unsigned long long *selected_count,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    attention_topk_candidate *candidates;
    unsigned long long i;
    unsigned long long out_count;

    if (selected_count) *selected_count = 0ull;
    if (!scores || !ordinals || !selected_indices || !selected_count) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "top-k selection requires scores, ordinals, and output");
    }
    if (candidate_count == 0ull || k == 0ull) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    candidates = (attention_topk_candidate *)yvex_attention_calloc_array(
        candidate_count, sizeof(*candidates));
    if (!candidates)
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_ALLOCATION, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            candidate_count, 0ull, err, YVEX_ERR_NOMEM,
            "top-k candidate allocation failed");
    for (i = 0ull; i < candidate_count; ++i) {
        if (!isfinite(scores[i])) {
            free(candidates);
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
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
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                candidate_count, i, err, YVEX_ERR_FORMAT,
                "top-k refuses duplicate candidate ordinal");
        }
    }
    qsort(candidates, (size_t)candidate_count, sizeof(*candidates),
          attention_candidate_rank_compare);
    out_count = yvex_attention_min_u64(candidate_count, k);
    for (i = 0ull; i < out_count; ++i)
        selected_indices[i] = candidates[i].index;
    *selected_count = out_count;
    free(candidates);
    yvex_error_clear(err);
    return YVEX_OK;
}

// Purpose: Return the admitted ue8m0 encode scale fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static unsigned char attention_ue8m0_encode_scale(float value)
{
    int exponent;
    float mantissa;

    if (!isfinite(value) || value <= 0.0f) return 0xffu;
    mantissa = frexpf(value, &exponent);
    if (mantissa > 0.5f) exponent++;
    exponent += 126;
    if (exponent < 0) return 0u;
    if (exponent > 254) return 254u;
    return (unsigned char)exponent;
}

// Purpose: Return the admitted ue8m0 decode scale fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
static float attention_ue8m0_decode_scale(unsigned char code)
{
    return yvex_quant_e8m0_decode(code);
}

// Purpose: Return the admitted fp8 e4m3fn encode fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned char yvex_attention_fp8_e4m3fn_encode(float value)
{
    static const float finite_max = 448.0f;
    float magnitude = fabsf(value);
    float best_error = INFINITY;
    unsigned char best = 0u;
    unsigned int code;
    int negative = signbit(value);

    if (!isfinite(value)) return negative ? 0xffu : 0x7fu;
    if (magnitude > finite_max) magnitude = finite_max;
    for (code = 0u; code < 0x7fu; ++code) {
        float decoded = yvex_attention_fp8_e4m3fn_decode(
            (unsigned char)code);
        float error = fabsf(decoded - magnitude);
        if (error < best_error ||
            (error == best_error && !(code & 1u) && (best & 1u))) {
            best_error = error;
            best = (unsigned char)code;
        }
    }
    return negative ? (unsigned char)(best | 0x80u) : best;
}

// Purpose: Return the admitted fp8 e4m3fn decode fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
float yvex_attention_fp8_e4m3fn_decode(unsigned char code)
{
    return yvex_quant_fp8_e4m3fn_decode(code);
}

// Purpose: Return the admitted fp8 fake quant block fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_fp8_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long i;
    float amax = 1.0e-4f;
    float scale;

    if (!input || !dequantized || !codes || !scale_code || count == 0ull) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "FP8 fake quant requires input, output, code, and scale buffers");
    }
    for (i = 0ull; i < count; ++i) {
        float magnitude;
        if (!isfinite(input[i])) {
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                count, i, err, YVEX_ERR_FORMAT,
                "FP8 fake quant refuses non-finite activation");
        }
        magnitude = fabsf(input[i]);
        if (magnitude > amax) amax = magnitude;
    }
    scale = attention_power_of_two_ceil(amax / 448.0f);
    *scale_code = attention_ue8m0_encode_scale(scale);
    scale = attention_ue8m0_decode_scale(*scale_code);
    if (!isfinite(scale) || scale <= 0.0f) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            count, 0ull, err, YVEX_ERR_FORMAT,
            "FP8 fake quant produced invalid UE8M0 scale");
    }
    for (i = 0ull; i < count; ++i) {
        float normalized = input[i] / scale;
        if (normalized > 448.0f) normalized = 448.0f;
        if (normalized < -448.0f) normalized = -448.0f;
        codes[i] = yvex_attention_fp8_e4m3fn_encode(normalized);
        dequantized[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
            yvex_attention_fp8_e4m3fn_decode(codes[i]) * scale));
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

// Purpose: Return the admitted fp4 e2m1 encode fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
unsigned char yvex_attention_fp4_e2m1_encode(float value)
{
    static const float values[] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f
    };
    float magnitude = fabsf(value);
    float best_error;
    unsigned char best = 0u;
    unsigned int code;

    if (isnan(value)) return signbit(value) ? 0x8u : 0u;
    if (magnitude > 6.0f) magnitude = 6.0f;
    best_error = fabsf(magnitude - values[0]);
    for (code = 1u; code < 8u; ++code) {
        float error = fabsf(magnitude - values[code]);
        if (error < best_error ||
            (error == best_error && !(code & 1u) && (best & 1u))) {
            best_error = error;
            best = (unsigned char)code;
        }
    }
    return signbit(value) ? (unsigned char)(best | 0x8u) : best;
}

// Purpose: Return the admitted fp4 e2m1 decode fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.
float yvex_attention_fp4_e2m1_decode(unsigned char code)
{
    static const float values[] = {
        0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 6.0f
    };
    unsigned char magnitude = (unsigned char)(code & 0x7u);
    float value = values[magnitude];

    return (code & 0x8u) ? -value : value;
}

// Purpose: Return the admitted fp4 fake quant block fact without transferring ownership.
// Inputs: typed caller-owned values accepted by the graph private ABI.
// Effects: mutates only explicit outputs or graph-owned state; performs no operator I/O.
// Failure: returns a typed refusal or neutral result without partial capability publication.
// Boundary: remains graph-local and cannot promote attention, KV, or generation support.

int yvex_attention_fp4_fake_quant_block(
    const float *input,
    unsigned long long count,
    float *dequantized,
    unsigned char *codes,
    unsigned char *scale_code,
    yvex_attention_failure *failure,
    yvex_error *err)
{
    unsigned long long i;
    float amax = 0.0f;
    float scale;

    if (!input || !dequantized || !codes || !scale_code || count == 0ull) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_INVALID_ARGUMENT, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN, 1ull,
            0ull, err, YVEX_ERR_INVALID_ARG,
            "FP4 fake quant requires input, output, code, and scale buffers");
    }
    *scale_code = 0u;
    for (i = 0ull; i < count; ++i) {
        float magnitude;
        if (!isfinite(input[i])) {
            return yvex_attention_reject(
                failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
                YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
                count, i, err, YVEX_ERR_FORMAT,
                "FP4 fake quant refuses non-finite activation");
        }
        magnitude = fabsf(input[i]);
        if (magnitude > amax) amax = magnitude;
    }
    if (amax < 6.0f * ldexpf(1.0f, -126)) {
        amax = 6.0f * ldexpf(1.0f, -126);
    }
    *scale_code = attention_ue8m0_encode_scale(
        attention_power_of_two_ceil(amax / 6.0f));
    scale = attention_ue8m0_decode_scale(*scale_code);
    if (!isfinite(scale) || scale <= 0.0f) {
        return yvex_attention_reject(
            failure, YVEX_DEEPSEEK_ATTENTION_FAILURE_NUMERIC, NULL,
            YVEX_ATTENTION_NO_LAYER, YVEX_TENSOR_ROLE_UNKNOWN,
            count, 0ull, err, YVEX_ERR_FORMAT,
            "FP4 fake quant produced invalid UE8M0 scale");
    }
    for (i = 0ull; i < count; ++i) {
        codes[i] = yvex_attention_fp4_e2m1_encode(input[i] / scale);
        dequantized[i] = yvex_quant_bf16_decode(yvex_quant_bf16_encode(
            yvex_attention_fp4_e2m1_decode(codes[i]) * scale));
    }
    yvex_error_clear(err);
    return YVEX_OK;
}
