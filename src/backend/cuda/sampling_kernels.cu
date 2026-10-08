/*
 * Keep full-vocabulary stochastic filtering and speculative acceptance on the device.
 *
 * The portable runtime owns RNG transactions and final publication. These kernels consume
 * explicit draws and return only bounded tokens and acceptance facts; dense probability rows
 * never become host authority.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include "src/backend/cuda/sampling_layout.h"

static __device__ void sampling_sum_add(double *sum, double *correction,
                                        double value)
{
    double next = *sum + value;
    if (fabs(*sum) >= fabs(value))
        *correction += (*sum - next) + value;
    else
        *correction += (value - next) + *sum;
    *sum = next;
}

/* The two-component sums approximate the real sum, not a new probability
 * contract. Publish only if their enclosure proves the original ordered
 * compensated sum rounds to this exact binary64 value; otherwise recompute. */
static __device__ __noinline__ int sampling_sum_certificate_device(
    const double *probabilities, unsigned long long count, int *valid,
    double *output)
{
    __shared__ double high[256], low[256], result;
    __shared__ int certified;
    if (count < 1024ull || count > 16777216ull) return 0;
    double sum = 0.0, correction = 0.0;
    for (unsigned long long index = threadIdx.x; index < count; index += blockDim.x) {
        double value = probabilities[index];
        if (!isfinite(value) || value < 0.0) atomicExch(valid, 0);
        else sampling_sum_add(&sum, &correction, value);
    }
    high[threadIdx.x] = sum;
    low[threadIdx.x] = correction;
    if (!threadIdx.x) certified = 0;
    __syncthreads();
    if (!threadIdx.x && *valid) {
        sum = correction = 0.0;
        #pragma unroll 1
        for (unsigned int lane = 0u; lane < 256u; ++lane) {
            sampling_sum_add(&sum, &correction, high[lane]);
            sampling_sum_add(&sum, &correction, low[lane]);
        }
        double rounded = __dadd_rn(sum, correction);
        if (isfinite(sum) && isfinite(correction) && isfinite(rounded) &&
            rounded >= 0x1p-1022 && sum >= fabs(correction)) {
            /* FastTwoSum recovers the exact residual of this final rounding.
             * The documented gamma_n^2 enclosure includes both the literal
             * serial pair and the 256 partial pairs + 512-term merge. */
            double residual = __dadd_rn(__dsub_rn(sum, rounded), correction);
            double nu = (double)count * 0x1p-53;
            double radius = __dmul_ru(__dmul_ru(64.0, __dmul_ru(nu, nu)),
                                     __dmul_ru(2.0, sum));
            long long bits = __double_as_longlong(rounded);
            double below = __longlong_as_double(bits - 1ll);
            double above = __longlong_as_double(bits + 1ll);
            /* At powers of two the lower and upper cells have different
             * widths. Prove both signed endpoints, not the smaller symmetric
             * radius; normalized mass near one must not fall back needlessly. */
            double lower_half_gap = 0.5 * (rounded - below);
            double upper_half_gap = 0.5 * (above - rounded);
            if (isfinite(radius) &&
                __dsub_rd(residual, radius) > -lower_half_gap &&
                __dadd_ru(residual, radius) < upper_half_gap) {
                result = rounded;
                certified = 1;
            }
        }
    }
    __syncthreads();
    int accepted = certified;
    double accepted_sum = accepted ? result : 0.0;
    /* A following normalization can reuse this function's shared flags before
     * slower lanes have consumed them. Retire the publication collectively. */
    __syncthreads();
    if (accepted) *output = accepted_sum;
    return accepted;
}

/* An inconclusive certificate retains every original compensated addition.
 * Cooperative tiled loads do not change the literal fallback's source order. */
static __device__ __noinline__ double sampling_ordered_sum_device(
    const double *probabilities, unsigned long long count, int *valid)
{
    __shared__ double tile[256], result;
    double sum = 0.0, correction = 0.0;
    if (sampling_sum_certificate_device(probabilities, count, valid, &sum))
        return sum;
    for (unsigned long long base = 0ull; base < count; base += blockDim.x) {
        unsigned long long index = base + threadIdx.x;
        if (index < count) tile[threadIdx.x] = probabilities[index];
        __syncthreads();
        if (!threadIdx.x) {
            unsigned long long extent = count - base;
            if (extent > blockDim.x) extent = blockDim.x;
            #pragma unroll 1
            for (unsigned long long lane = 0ull; lane < extent; ++lane) {
                double value = tile[lane];
                if (!isfinite(value) || value < 0.0) *valid = 0;
                else sampling_sum_add(&sum, &correction, value);
            }
        }
        __syncthreads();
    }
    if (!threadIdx.x) result = sum + correction;
    __syncthreads();
    return result;
}

static __device__ __noinline__ int sampling_normalize_device(
    double *probabilities, unsigned long long count, double *maximum_error)
{
    __shared__ int valid;
    if (!threadIdx.x) valid = 1;
    __syncthreads();
    double sum = sampling_ordered_sum_device(probabilities, count, &valid);
    if (!threadIdx.x && (!isfinite(sum) || sum <= 0.0)) valid = 0;
    __syncthreads();
    if (!valid) return 0;
    for (unsigned long long index = threadIdx.x; index < count; index += blockDim.x)
        probabilities[index] /= sum;
    __syncthreads();
    double verified = sampling_ordered_sum_device(probabilities, count, &valid);
    if (!threadIdx.x) {
        if (!isfinite(verified)) valid = 0;
        else {
            double error = fabs(verified - 1.0);
            if (error > *maximum_error) *maximum_error = error;
        }
    }
    __syncthreads();
    return valid;
}

static __device__ int sampling_candidate_before(
    const unsigned int *tokens, const double *probabilities,
    const double *deviations, unsigned long long left,
    unsigned long long right, unsigned int mode)
{
    if (tokens[left] == ~0u) return 0;
    if (tokens[right] == ~0u) return 1;
    if (mode == 2u) return tokens[left] < tokens[right];
    if (mode == 1u && deviations[left] != deviations[right])
        return deviations[left] < deviations[right];
    if (probabilities[left] != probabilities[right])
        return probabilities[left] > probabilities[right];
    return tokens[left] < tokens[right];
}

static __device__ void sampling_candidate_swap(
    unsigned int *tokens, float *logits, double *probabilities,
    double *deviations, unsigned long long left, unsigned long long right)
{
    unsigned int token = tokens[left];
    float logit = logits[left];
    double probability = probabilities[left], deviation = deviations[left];
    tokens[left] = tokens[right];
    logits[left] = logits[right];
    probabilities[left] = probabilities[right];
    deviations[left] = deviations[right];
    tokens[right] = token;
    logits[right] = logit;
    probabilities[right] = probability;
    deviations[right] = deviation;
}

static __device__ void sampling_sort_device(
    unsigned int *tokens, float *logits, double *probabilities,
    double *deviations, unsigned long long count,
    unsigned long long padded, unsigned int mode)
{
    unsigned long long index, partner, width, stride;
    /* Filtering can leave a tiny survivor set. Keep the same total ordering
     * and sentinel policy, but do not revisit the original vocabulary tail. */
    unsigned long long active_padded = 1ull;
    while (active_padded < count && active_padded < padded)
        active_padded <<= 1ull;
    padded = active_padded;
    for (index = count + threadIdx.x; index < padded; index += blockDim.x) {
        tokens[index] = ~0u;
        logits[index] = 0.0f;
        probabilities[index] = -1.0;
        deviations[index] = 1.0 / 0.0;
    }
    __syncthreads();
    for (width = 2ull; width <= padded; width <<= 1ull) {
        for (stride = width >> 1ull; stride; stride >>= 1ull) {
            for (index = threadIdx.x; index < padded; index += blockDim.x) {
                partner = index ^ stride;
                if (partner > index) {
                    int forward = (index & width) == 0ull;
                    int swap = forward
                        ? sampling_candidate_before(tokens, probabilities, deviations,
                                                    partner, index, mode)
                        : sampling_candidate_before(tokens, probabilities, deviations,
                                                    index, partner, mode);
                    if (swap)
                        sampling_candidate_swap(tokens, logits, probabilities,
                                                deviations, index, partner);
                }
            }
            __syncthreads();
        }
        if (width == padded) break;
    }
}

static __device__ unsigned long long sampling_compact_device(
    unsigned int *tokens, float *logits, double *probabilities,
    double *deviations, unsigned long long count, double threshold,
    int inclusive)
{
    __shared__ unsigned long long write;
    __shared__ unsigned int warp_counts[8];
    unsigned int lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    if (!threadIdx.x) write = 0ull;
    __syncthreads();
    for (unsigned long long base = 0ull; base < count; base += blockDim.x) {
        unsigned long long read = base + threadIdx.x;
        unsigned int token = 0u;
        float logit = 0.0f;
        double probability = 0.0, deviation = 0.0;
        if (read < count) {
            token = tokens[read]; logit = logits[read];
            probability = probabilities[read]; deviation = deviations[read];
        }
        int keep = read < count && (inclusive ? probability >= threshold : probability > threshold);
        unsigned int mask = __ballot_sync(0xffffffffu, keep);
        if (!lane) warp_counts[warp] = __popc(mask);
        __syncthreads(); /* All original tile values are saved before any in-place write. */
        unsigned int prefix = __popc(mask & ((1u << lane) - 1u));
        for (unsigned int earlier = 0u; earlier < warp; ++earlier)
            prefix += warp_counts[earlier];
        unsigned long long target = write + prefix;
        if (keep && target != read) {
            tokens[target] = token; logits[target] = logit;
            probabilities[target] = probability; deviations[target] = deviation;
        }
        __syncthreads();
        if (!threadIdx.x)
            for (unsigned int w = 0u; w < 8u; ++w) write += warp_counts[w];
        __syncthreads();
    }
    return write;
}

static __device__ void sampling_initialize_device(
    const float *values, unsigned long long count, double temperature,
    unsigned int *tokens, float *logits, double *probabilities,
    double *deviations, double *maximum_scaled, float *output_values,
    int *status)
{
    __shared__ float warp_logits[8];
    __shared__ double warp_scaled[8];
    __shared__ unsigned long long warp_logit_indices[8], warp_scaled_indices[8];
    float maximum_logit = -3.402823466e+38F;
    double maximum = -1.7976931348623157e+308;
    unsigned long long logit_index = ~0ull, scaled_index = ~0ull;
    unsigned int lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    for (unsigned long long index = threadIdx.x; index < count; index += blockDim.x) {
        float value = values[index];
        double scaled = (double)value / temperature;
        if (!isfinite(value) || !isfinite(scaled)) atomicCAS(status, 0, 1);
        tokens[index] = (unsigned int)index; logits[index] = value;
        probabilities[index] = scaled; deviations[index] = 0.0;
        if (scaled > maximum) { maximum = scaled; scaled_index = index; }
        if (value > maximum_logit) { maximum_logit = value; logit_index = index; }
    }
    /* Max is selection, not arithmetic reassociation. Preserve the earliest
     * source coordinate on equal values, including distinct signed zeros. */
    for (unsigned int offset = 16u; offset; offset >>= 1u) {
        double other = __shfl_down_sync(0xffffffffu, maximum, offset);
        float other_logit = __shfl_down_sync(0xffffffffu, maximum_logit, offset);
        unsigned long long other_index = __shfl_down_sync(0xffffffffu, scaled_index, offset);
        unsigned long long other_logit_index = __shfl_down_sync(0xffffffffu, logit_index, offset);
        if (other > maximum || (other == maximum && other_index < scaled_index)) {
            maximum = other; scaled_index = other_index;
        }
        if (other_logit > maximum_logit ||
            (other_logit == maximum_logit && other_logit_index < logit_index)) {
            maximum_logit = other_logit; logit_index = other_logit_index;
        }
    }
    if (!lane) {
        warp_logits[warp] = maximum_logit; warp_scaled[warp] = maximum;
        warp_logit_indices[warp] = logit_index; warp_scaled_indices[warp] = scaled_index;
    }
    __syncthreads();
    if (!warp) {
        maximum = lane < 8u ? warp_scaled[lane] : -1.7976931348623157e+308;
        maximum_logit = lane < 8u ? warp_logits[lane] : -3.402823466e+38F;
        scaled_index = lane < 8u ? warp_scaled_indices[lane] : ~0ull;
        logit_index = lane < 8u ? warp_logit_indices[lane] : ~0ull;
        for (unsigned int offset = 16u; offset; offset >>= 1u) {
            double other = __shfl_down_sync(0xffffffffu, maximum, offset);
            float other_logit = __shfl_down_sync(0xffffffffu, maximum_logit, offset);
            unsigned long long other_index = __shfl_down_sync(0xffffffffu, scaled_index, offset);
            unsigned long long other_logit_index = __shfl_down_sync(0xffffffffu, logit_index, offset);
            if (other > maximum || (other == maximum && other_index < scaled_index)) {
                maximum = other; scaled_index = other_index;
            }
            if (other_logit > maximum_logit ||
                (other_logit == maximum_logit && other_logit_index < logit_index)) {
                maximum_logit = other_logit; logit_index = other_logit_index;
            }
        }
        if (!lane) {
            *maximum_scaled = maximum;
            output_values[YVEX_CUDA_SAMPLING_MAXIMUM_LOGIT] = maximum_logit;
        }
    }
    __syncthreads();
}

/* Repeated verifier rows reuse this one implementation instead of duplicating
 * its numerical loops throughout the caller's instruction footprint. */
static __device__ __noinline__ void sampling_filter_device(
    const float *values, unsigned long long vocabulary_size,
    unsigned long long padded_size, double temperature,
    unsigned long long top_k, double top_p, double min_p,
    double typical_p, unsigned int *tokens, float *logits,
    double *probabilities, double *deviations,
    unsigned long long *count, unsigned long long *counts,
    double *statistics, float *output_values, int *status)
{
    __shared__ double maximum_scaled;
    sampling_initialize_device(values, vocabulary_size, temperature, tokens,
        logits, probabilities, deviations, &maximum_scaled, output_values, status);
    if (!threadIdx.x) *count = vocabulary_size;
    __syncthreads();
    if (*status) return;
    /* Independent exponential evaluations do not change the source order of
     * the F64 compensated normalization, filtering or RNG consumption. */
    for (unsigned long long index = threadIdx.x; index < vocabulary_size;
         index += blockDim.x)
        probabilities[index] = exp(probabilities[index] - maximum_scaled);
    __syncthreads();
    if (!sampling_normalize_device(probabilities, *count,
            &statistics[YVEX_CUDA_SAMPLING_NORMALIZATION_ERROR]) && !threadIdx.x)
        atomicCAS(status, 0, 1);
    __syncthreads();
    if (*status) return;
    unsigned long long positive_count = sampling_compact_device(
        tokens, logits, probabilities, deviations, *count, 0.0, 0);
    if (!threadIdx.x) *count = positive_count;
    __syncthreads();
    if (*status || !*count) return;
    if (top_k && top_k < *count) {
        sampling_sort_device(tokens, logits, probabilities, deviations,
                             *count, padded_size, 0u);
        if (!threadIdx.x) *count = top_k;
        __syncthreads();
        if (!sampling_normalize_device(probabilities, *count,
                &statistics[YVEX_CUDA_SAMPLING_NORMALIZATION_ERROR]) && !threadIdx.x)
            atomicCAS(status, 0, 1);
    }
    __syncthreads();
    if (!threadIdx.x) {
        counts[YVEX_CUDA_SAMPLING_TOP_K_COUNT] = *count;
        if (!*status && min_p > 0.0) {
            double maximum = 0.0;
            for (unsigned long long index = 0ull; index < *count; ++index)
                if (probabilities[index] > maximum) maximum = probabilities[index];
            statistics[YVEX_CUDA_SAMPLING_MIN_P_THRESHOLD] = min_p * maximum;
        }
    }
    __syncthreads();
    if (*status) return;
    if (min_p > 0.0) {
        unsigned long long min_count = sampling_compact_device(tokens, logits,
            probabilities, deviations, *count,
            statistics[YVEX_CUDA_SAMPLING_MIN_P_THRESHOLD], 1);
        if (!threadIdx.x) *count = min_count;
        __syncthreads();
        if (!sampling_normalize_device(probabilities, *count,
                &statistics[YVEX_CUDA_SAMPLING_NORMALIZATION_ERROR]) && !threadIdx.x)
            atomicCAS(status, 0, 1);
    }
    if (!threadIdx.x) counts[YVEX_CUDA_SAMPLING_MIN_P_COUNT] = *count;
    __syncthreads();
    if (*status || !*count) return;
    if (typical_p < 1.0) {
        if (!threadIdx.x) {
            double entropy = 0.0, correction = 0.0;
            for (unsigned long long index = 0ull; index < *count; ++index)
                sampling_sum_add(
                    &entropy, &correction,
                    -probabilities[index] * log(probabilities[index]));
            entropy += correction;
            statistics[YVEX_CUDA_SAMPLING_ENTROPY] = entropy;
            if (!isfinite(entropy)) atomicCAS(status, 0, 1);
            for (unsigned long long index = 0ull;
                 !*status && index < *count; ++index)
                deviations[index] =
                    fabs(-log(probabilities[index]) - entropy);
        }
        __syncthreads();
        if (*status) return;
        sampling_sort_device(tokens, logits, probabilities, deviations,
                             *count, padded_size, 1u);
        if (!threadIdx.x) {
            double mass = 0.0;
            unsigned long long retained = 0ull;
            while (retained < *count && mass < typical_p)
                mass += probabilities[retained++];
            if (!retained) retained = 1ull;
            statistics[YVEX_CUDA_SAMPLING_TYPICAL_MASS] = mass;
            *count = retained;
        }
        __syncthreads();
        if (!sampling_normalize_device(probabilities, *count,
                &statistics[YVEX_CUDA_SAMPLING_NORMALIZATION_ERROR]) && !threadIdx.x)
            atomicCAS(status, 0, 1);
    }
    __syncthreads();
    if (*status || !*count) return;
    if (!threadIdx.x)
        counts[YVEX_CUDA_SAMPLING_TYPICAL_COUNT] = *count;
    if (top_p < 1.0)
        sampling_sort_device(tokens, logits, probabilities, deviations,
                             *count, padded_size, 0u);
    if (!threadIdx.x) {
        if (top_p < 1.0) {
            double mass = 0.0;
            unsigned long long retained = 0ull;
            while (retained < *count && mass < top_p)
                mass += probabilities[retained++];
            if (!retained) retained = 1ull;
            statistics[YVEX_CUDA_SAMPLING_TOP_P_MASS] = mass;
            *count = retained;
        }
        counts[YVEX_CUDA_SAMPLING_TOP_P_COUNT] = *count;
    }
    __syncthreads();
    if (!sampling_normalize_device(probabilities, *count,
            &statistics[YVEX_CUDA_SAMPLING_NORMALIZATION_ERROR]) && !threadIdx.x)
        atomicCAS(status, 0, 1);
    __syncthreads();
}

static __device__ void sampling_dense_device(
    const unsigned int *tokens, const double *probabilities,
    unsigned long long count, float *dense,
    unsigned long long vocabulary_size)
{
    for (unsigned long long index = threadIdx.x;
         index < vocabulary_size; index += blockDim.x)
        dense[index] = 0.0f;
    __syncthreads();
    for (unsigned long long index = threadIdx.x; index < count;
         index += blockDim.x)
        dense[tokens[index]] = (float)probabilities[index];
    __syncthreads();
}

static __device__ unsigned int sampling_dense_draw(
    const float *target, const float *draft,
    unsigned long long vocabulary_size, double uniform, int residual)
{
    double total = 0.0, cumulative = 0.0;
    for (unsigned long long index = 0ull; index < vocabulary_size; ++index)
        total += residual ? fmax((double)target[index] - draft[index], 0.0)
                          : (double)target[index];
    if (residual && total <= 1e-8) {
        residual = 0;
        total = 0.0;
        for (unsigned long long index = 0ull; index < vocabulary_size; ++index)
            total += target[index];
    }
    if (total <= 0.0) return ~0u;
    for (unsigned long long index = 0ull; index < vocabulary_size; ++index) {
        double value = residual
            ? fmax((double)target[index] - draft[index], 0.0)
            : (double)target[index];
        cumulative += value / total;
        if (uniform < cumulative || index + 1ull == vocabulary_size)
            return (unsigned int)index;
    }
    return ~0u;
}

extern "C" __global__ void yvex_sample_stochastic_f32(
    const float *values, unsigned long long vocabulary_size,
    unsigned long long padded_size, double temperature,
    unsigned long long top_k, double top_p, double min_p,
    double typical_p, unsigned int random_value, unsigned int *tokens,
    float *logits, double *probabilities, double *deviations,
    unsigned long long *counts, double *statistics, float *output_values,
    unsigned int *selection, int *status)
{
    __shared__ unsigned long long count;
    if (!status || blockIdx.x || blockDim.x != 256u || !values ||
        !vocabulary_size || !padded_size || padded_size < vocabulary_size ||
        !tokens || !logits || !probabilities || !deviations || !counts ||
        !statistics || !output_values || !selection) {
        if (status && !blockIdx.x && !threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    sampling_filter_device(
        values, vocabulary_size, padded_size, temperature, top_k, top_p,
        min_p, typical_p, tokens, logits, probabilities, deviations, &count,
        counts, statistics, output_values, status);
    if (*status || !count) return;
    /* Initialization and positive/min-p compaction preserve token order.
     * Only rank-based filters can have permuted it before this draw. */
    if (top_k || typical_p < 1.0 || top_p < 1.0)
        sampling_sort_device(tokens, logits, probabilities, deviations,
                             count, padded_size, 2u);
    if (!threadIdx.x) {
        double uniform = ((double)random_value + 0.5) / 4294967296.0;
        double cumulative = 0.0;
        unsigned long long selected = count - 1ull;
        selection[YVEX_CUDA_SAMPLING_NUMERIC_FALLBACK] = 1u;
        for (unsigned long long index = 0ull; index < count; ++index) {
            cumulative += probabilities[index];
            if (uniform < cumulative) {
                selected = index;
                selection[YVEX_CUDA_SAMPLING_NUMERIC_FALLBACK] = 0u;
                break;
            }
        }
        if (!isfinite(probabilities[selected]) ||
            probabilities[selected] <= 0.0) {
            atomicCAS(status, 0, 1);
            return;
        }
        selection[YVEX_CUDA_SAMPLING_SELECTED_TOKEN] = tokens[selected];
        output_values[YVEX_CUDA_SAMPLING_SELECTED_LOGIT] = logits[selected];
        statistics[YVEX_CUDA_SAMPLING_SELECTED_PROBABILITY] =
            probabilities[selected];
    }
}

extern "C" __global__ void yvex_speculation_stochastic_f32(
    const float *draft_values, const float *target_values,
    unsigned long long candidate_count, unsigned long long vocabulary_size,
    unsigned long long padded_size, double temperature,
    unsigned long long top_k, double top_p, double min_p, double typical_p,
    const unsigned int *candidate_tokens, const double *acceptance_uniforms,
    double correction_uniform, unsigned int *tokens, float *logits,
    double *probabilities, double *deviations, float *draft_dense,
    float *target_dense, unsigned long long *counts, double *statistics,
    float *output_values, unsigned int *committed_tokens,
    unsigned long long *result, int *status)
{
    __shared__ unsigned long long count;
    if (!status || blockIdx.x || blockDim.x != 256u || !draft_values ||
        !target_values || !candidate_count || !vocabulary_size ||
        !padded_size || padded_size < vocabulary_size || !candidate_tokens ||
        !acceptance_uniforms || !tokens || !logits || !probabilities ||
        !deviations || !draft_dense || !target_dense || !counts ||
        !statistics || !output_values || !committed_tokens || !result ||
        !isfinite(correction_uniform) || correction_uniform < 0.0 ||
        correction_uniform >= 1.0) {
        if (status && !blockIdx.x && !threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (!threadIdx.x) {
        result[YVEX_CUDA_SPECULATION_PROPOSED_COUNT] = candidate_count;
        result[YVEX_CUDA_SPECULATION_REJECTION_INDEX] = candidate_count;
    }
    __syncthreads();
    for (unsigned long long row = 0ull; row < candidate_count; ++row) {
        sampling_filter_device(
            draft_values + row * vocabulary_size, vocabulary_size,
            padded_size, temperature, top_k, top_p, min_p, typical_p,
            tokens, logits, probabilities, deviations, &count, counts,
            statistics, output_values, status);
        if (*status || !count) return;
        sampling_dense_device(
            tokens, probabilities, count, draft_dense, vocabulary_size);
        sampling_filter_device(
            target_values + row * vocabulary_size, vocabulary_size,
            padded_size, temperature, top_k, top_p, min_p, typical_p,
            tokens, logits, probabilities, deviations, &count, counts,
            statistics, output_values, status);
        if (*status || !count) return;
        sampling_dense_device(
            tokens, probabilities, count, target_dense, vocabulary_size);
        if (!threadIdx.x) {
            unsigned int candidate = candidate_tokens[row];
            double q, p, ratio;
            if (candidate >= vocabulary_size ||
                !isfinite(acceptance_uniforms[row]) ||
                acceptance_uniforms[row] < 0.0 ||
                acceptance_uniforms[row] >= 1.0) {
                atomicCAS(status, 0, 2);
            } else {
                q = draft_dense[candidate];
                p = target_dense[candidate];
                if (!isfinite(q) || q <= 0.0 || !isfinite(p) || p < 0.0) {
                    atomicCAS(status, 0, 1);
                } else {
                    ratio = fmin(1.0, p / q);
                    if (acceptance_uniforms[row] < ratio) {
                        committed_tokens[row] = candidate;
                        result[YVEX_CUDA_SPECULATION_ACCEPTED_COUNT] = row + 1ull;
                    } else {
                        unsigned int correction = sampling_dense_draw(
                            target_dense, draft_dense, vocabulary_size,
                            correction_uniform, 1);
                        if (correction == ~0u) {
                            atomicCAS(status, 0, 1);
                        } else {
                            committed_tokens[row] = correction;
                            result[YVEX_CUDA_SPECULATION_REJECTED_COUNT] =
                                candidate_count - row;
                            result[YVEX_CUDA_SPECULATION_COMMITTED_COUNT] =
                                row + 1ull;
                            result[YVEX_CUDA_SPECULATION_REJECTION_INDEX] = row;
                            result[YVEX_CUDA_SPECULATION_CORRECTION_PRESENT] = 1ull;
                            result[YVEX_CUDA_SPECULATION_CORRECTION_TOKEN] = correction;
                        }
                    }
                }
            }
        }
        __syncthreads();
        if (*status) return;
        if (result[YVEX_CUDA_SPECULATION_CORRECTION_PRESENT]) return;
    }
    sampling_filter_device(
        target_values + candidate_count * vocabulary_size, vocabulary_size,
        padded_size, temperature, top_k, top_p, min_p, typical_p, tokens,
        logits, probabilities, deviations, &count, counts, statistics,
        output_values, status);
    if (*status || !count) return;
    sampling_dense_device(
        tokens, probabilities, count, target_dense, vocabulary_size);
    if (!threadIdx.x) {
        unsigned int bonus = sampling_dense_draw(
            target_dense, NULL, vocabulary_size, correction_uniform, 0);
        if (bonus == ~0u) {
            atomicCAS(status, 0, 1);
            return;
        }
        committed_tokens[candidate_count] = bonus;
        result[YVEX_CUDA_SPECULATION_ACCEPTED_COUNT] = candidate_count;
        result[YVEX_CUDA_SPECULATION_COMMITTED_COUNT] = candidate_count + 1ull;
        result[YVEX_CUDA_SPECULATION_ALL_ACCEPTED] = 1ull;
        result[YVEX_CUDA_SPECULATION_BONUS_PRESENT] = 1ull;
        result[YVEX_CUDA_SPECULATION_CORRECTION_TOKEN] = bonus;
    }
}
