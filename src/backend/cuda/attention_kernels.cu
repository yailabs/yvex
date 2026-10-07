/*
 * Execute exact and native online-softmax attention reduction kernels.
 *
 * The exact kernel preserves forensic arithmetic. The native kernel owns the
 * independently admitted parallel reduction used by production execution.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include "src/backend/cuda/dot_recovery.h"

/* Channels are independent; each channel visits the admitted token positions
 * in order and owns its complete state lane. Checkpoints remain transaction
 * private. This removes per-position launch/copy ownership handoffs without
 * changing the ordered F64 compressor reduction or BF16 publication. */
extern "C" __global__ void yvex_attention_rolling_rows(
    float *state_kv, float *state_score, const float *token_kv, const float *token_score,
    const unsigned char *ape, unsigned long long ape_row_bytes, unsigned int ape_qtype,
    float *compressed, unsigned long long ratio, unsigned long long head_dim,
    unsigned long long token_count, unsigned long long first_cursor,
    int overlap, int checkpoints, int *status)
{
    unsigned long long lane = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (!status || *status || lane >= head_dim) return;
    if (!state_kv || !state_score || !token_kv || !token_score || !ape ||
        !compressed || !ape_row_bytes || !ratio || !head_dim || !token_count ||
        first_cursor >= ratio || (overlap != 0 && overlap != 1) ||
        (checkpoints != 0 && checkpoints != 1) ||
        head_dim > ~0ull / (overlap ? 2ull : 1ull) ||
        ratio > ~0ull / (overlap ? 2ull : 1ull)) {
        atomicCAS(status, 0, 2); return;
    }
    unsigned long long factor = overlap ? 2ull : 1ull;
    unsigned long long width = head_dim * factor, slots = ratio * factor;
    if (width > ~0ull / slots || token_count > ~0ull / width ||
        (checkpoints && token_count >= ~0ull / (width * slots))) {
        atomicCAS(status, 0, 2); return;
    }
    unsigned long long extent = width * slots, emitted = 0ull;
    for (unsigned long long token = 0ull; token < token_count; ++token) {
        unsigned long long cursor = (first_cursor + token) % ratio;
        float *before_kv = state_kv + (checkpoints ? token * extent : 0ull);
        float *before_score = state_score + (checkpoints ? token * extent : 0ull);
        float *after_kv = state_kv + (checkpoints ? (token + 1ull) * extent : 0ull);
        float *after_score = state_score + (checkpoints ? (token + 1ull) * extent : 0ull);
        if (checkpoints)
            for (unsigned long long slot = 0ull; slot < slots; ++slot)
                for (unsigned long long component = 0ull; component < factor; ++component) {
                    unsigned long long i = slot * width + component * head_dim + lane;
                    after_kv[i] = before_kv[i]; after_score[i] = before_score[i];
                }
        for (unsigned long long component = 0ull; component < factor; ++component) {
            unsigned long long column = component * head_dim + lane;
            unsigned long long i = (overlap ? ratio + cursor : cursor) * width + column;
            float kv = token_kv[token * width + column];
            float bias = qtype_value(ape + cursor * ape_row_bytes, column, ape_qtype);
            float score = __fadd_rn(token_score[token * width + column], bias);
            if (!isfinite(kv) || !isfinite(bias) || !isfinite(score)) atomicCAS(status, 0, 1);
            after_kv[i] = kv; after_score[i] = score;
        }
        if (cursor + 1ull != ratio) continue;
        double maximum = -INFINITY, denominator = 0.0, value = 0.0;
        for (unsigned long long slot = 0ull; slot < ratio; ++slot) {
            double score = (double)after_score[slot * width + lane];
            if (score > maximum) maximum = score;
            if (overlap) {
                score = (double)after_score[(ratio + slot) * width + lane + head_dim];
                if (score > maximum) maximum = score;
            }
        }
        for (unsigned long long slot = 0ull; slot < ratio; ++slot) {
            double score = (double)after_score[slot * width + lane];
            double weight = exp(__dadd_rn(score, -maximum));
            denominator = __dadd_rn(denominator, weight);
            value = __dadd_rn(value, __dmul_rn(weight, (double)after_kv[slot * width + lane]));
            if (overlap) {
                unsigned long long i = (ratio + slot) * width + lane + head_dim;
                weight = exp(__dadd_rn((double)after_score[i], -maximum));
                denominator = __dadd_rn(denominator, weight);
                value = __dadd_rn(value, __dmul_rn(weight, (double)after_kv[i]));
            }
        }
        float result = (float)__ddiv_rn(value, denominator);
        if (!isfinite(denominator) || denominator <= 0.0 || !isfinite(value) || !isfinite(result))
            atomicCAS(status, 0, 1);
        else compressed[emitted * head_dim + lane] = float_to_bf16_rne(result);
        ++emitted;
        if (overlap)
            for (unsigned long long slot = 0ull; slot < ratio; ++slot)
                for (unsigned long long component = 0ull; component < factor; ++component) {
                    unsigned long long i = slot * width + component * head_dim + lane;
                    after_kv[i] = after_kv[ratio * width + i];
                    after_score[i] = after_score[ratio * width + i];
                }
    }
}

typedef struct {
    const float *local;
    const unsigned long long *local_positions;
    const float *compressed;
    const unsigned long long *compressed_positions;
    const unsigned long long *selected;
    unsigned long long initial_local_count, local_stride, compressed_stride;
    unsigned long long topk_capacity, sliding_window, ratio;
    unsigned long long phase_start_position, token_count;
    unsigned int attention_class;
    int candidate_block_visible;
} attention_reduce_rows;

/* Both projections certify the ordinary ordered F64 dot's F32 publication;
 * ambiguous rows retain ordered evaluation on CUDA. */
extern "C" __global__ void yvex_attention_bf16_pair(
    const unsigned char *first, unsigned long long first_row_bytes,
    const unsigned char *second, unsigned long long second_row_bytes,
    unsigned long long row_width, unsigned long long row_count,
    const float *input, float *first_out, float *second_out, int *status)
{
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long row_index =
        ((unsigned long long)blockIdx.x * blockDim.x + threadIdx.x) / 32u;
    float first_sum, second_sum;
    if (!status) return;
    if (!first || !second || !first_row_bytes || !second_row_bytes ||
        !row_width || !row_count || !input || !first_out || !second_out ||
        blockDim.x == 0u || blockDim.x % 32u) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    if (__shfl_sync(0xffffffffu, *status, 0) || row_index >= row_count) return;
    first += row_index * first_row_bytes;
    second += row_index * second_row_bytes;
    first_sum = qtype_certified_dot_f64<YVEX_GGUF_QTYPE_BF16>(
        first, input, row_width, YVEX_GGUF_QTYPE_BF16);
    second_sum = qtype_certified_dot_f64<YVEX_GGUF_QTYPE_BF16>(
        second, input, row_width, YVEX_GGUF_QTYPE_BF16);
    if (lane) return;
    if (!isfinite(first_sum) || !isfinite(second_sum)) atomicCAS(status, 0, 1);
    else {
        first_out[row_index] = first_sum;
        second_out[row_index] = second_sum;
    }
}

static __device__ __forceinline__ const float *attention_reduce_row(
    const attention_reduce_rows *rows, unsigned long long pass,
    unsigned long long ordinal, unsigned long long candidate,
    unsigned long long local_offset, int *visible)
{
    const float *row = NULL;
    unsigned long long position = ~0ull;
    *visible = 0;
    if (pass == 0ull) {
        candidate += local_offset;
        row = rows->local + candidate * rows->local_stride;
        position = rows->local_positions[candidate];
        if (!rows->candidate_block_visible) {
            unsigned long long token_position = rows->phase_start_position + ordinal;
            unsigned long long first = token_position + 1ull > rows->sliding_window
                ? token_position + 1ull - rows->sliding_window : 0ull;
            if (position < first || position > token_position) return NULL;
        }
    } else {
        unsigned long long token_position = rows->phase_start_position + ordinal;
        unsigned long long index;
        index = rows->attention_class == 2u
            ? candidate : rows->selected[ordinal * rows->topk_capacity + candidate];
        row = rows->compressed + index * rows->compressed_stride;
        position = rows->compressed_positions[index];
        if (position > token_position ||
            position > ~0ull - rows->ratio + 1ull ||
            position + rows->ratio - 1ull > token_position) return NULL;
    }
    *visible = row != NULL;
    return row;
}

extern "C" __global__ void yvex_attention_reduce(
    const float *query,
    const float *local,
    const unsigned long long *local_positions,
    unsigned long long initial_local_count,
    unsigned long long local_stride,
    const float *compressed,
    const unsigned long long *compressed_positions,
    unsigned long long compressed_stride,
    const unsigned long long *selected,
    const unsigned long long *selected_count_ptr,
    unsigned long long topk_capacity,
    const float *sinks,
    unsigned long long query_heads,
    unsigned long long head_dim,
    unsigned long long sliding_window,
    unsigned long long ratio,
    unsigned int attention_class,
    unsigned long long phase_start_position,
    unsigned long long token_count,
    int candidate_block_visible,
    float *out,
    int *status)
{
    extern __shared__ double dot_terms[];
    __shared__ double maximum;
    __shared__ double denominator;
    __shared__ double probability;
    __shared__ int active;
    unsigned long long task = (unsigned long long)blockIdx.x;
    unsigned long long ordinal = query_heads ? task / query_heads : token_count;
    unsigned long long head = query_heads ? task % query_heads : query_heads;
    unsigned long long token_position, local_offset, local_count, compressed_count;
    unsigned int thread = threadIdx.x;
    if (!status) return;
    if (ordinal >= token_count || head >= query_heads) return;
    if (!query || !local || !local_positions || !sinks || !out || !query_heads ||
        !head_dim || !sliding_window || !token_count || attention_class > 2u ||
        (candidate_block_visible != 0 && candidate_block_visible != 1) ||
        phase_start_position > ~0ull - ordinal || local_stride < head_dim ||
        (attention_class != 0u &&
         (!compressed || !compressed_positions || compressed_stride < head_dim)) ||
        (attention_class == 1u &&
         (!selected || !selected_count_ptr || !topk_capacity)) ||
        (attention_class == 1u && ratio != 4ull) ||
        (attention_class == 2u && ratio != 128ull) ||
        (attention_class == 0u && ratio != 0ull)) {
        if (thread == 0u) atomicCAS(status, 0, 2);
        return;
    }
    token_position = phase_start_position + ordinal;
    if (candidate_block_visible) {
        local_offset = 0ull;
        local_count = initial_local_count + token_count;
    } else {
        unsigned long long local_before = initial_local_count + ordinal;
        unsigned long long history_count = token_position < sliding_window - 1ull
            ? token_position : sliding_window - 1ull;
        local_offset = local_before - history_count;
        local_count = history_count + 1ull;
    }
    compressed_count = attention_class == 0u ? 0ull
        : attention_class == 1u ? selected_count_ptr[ordinal]
        : token_position / ratio + ((token_position + 1ull) % ratio == 0ull);
    const float *q = query + (ordinal * query_heads + head) * head_dim;
    double scale = 1.0 / sqrt((double)head_dim);
    if (thread == 0u) {
        active = *status == 0;
        maximum = (double)sinks[head];
        denominator = 0.0;
    }
    __syncthreads();
    if (!active) return;
    attention_reduce_rows rows = {
        local, local_positions, compressed, compressed_positions, selected,
        initial_local_count, local_stride, compressed_stride, topk_capacity,
        sliding_window, ratio, phase_start_position, token_count,
        attention_class, candidate_block_visible
    };
    for (unsigned long long lane = (unsigned long long)thread; lane < head_dim;
         lane += (unsigned long long)blockDim.x)
        out[(ordinal * query_heads + head) * head_dim + lane] = 0.0f;
    __syncthreads();
    /* Forensic reduction follows the CPU two-pass maximum and F32 destination
       accumulation contract. Native execution retains its separate online path. */
    for (unsigned int stage = 0u; stage < 2u; ++stage) {
        if (stage == 1u && thread == 0u)
            denominator = exp((double)sinks[head] - maximum);
        __syncthreads();
        for (unsigned long long pass = 0ull; pass < 2ull; ++pass) {
            unsigned long long count = pass == 0ull ? local_count : compressed_count;
            for (unsigned long long candidate = 0ull; candidate < count; ++candidate) {
                int visible;
                const float *row = attention_reduce_row(
                    &rows, pass, ordinal, candidate, local_offset, &visible);
                if (!visible) continue;
                double dot = 0.0;
                for (unsigned long long base = 0ull; base < head_dim;
                     base += (unsigned long long)blockDim.x) {
                    unsigned long long lane = base + (unsigned long long)thread;
                    dot_terms[thread] = lane < head_dim
                        ? __dmul_rn((double)q[lane], (double)row[lane]) : 0.0;
                    __syncthreads();
                    if (thread == 0u) {
                        unsigned long long tile = head_dim - base;
                        if (tile > (unsigned long long)blockDim.x) tile = blockDim.x;
                        for (unsigned long long i = 0ull; i < tile; ++i)
                            dot = __dadd_rn(dot, dot_terms[i]);
                    }
                    __syncthreads();
                }
                if (thread == 0u) {
                    double score = __dmul_rn(dot, scale);
                    if (stage == 0u && score > maximum) maximum = score;
                    if (stage == 1u) {
                        probability = exp(__dadd_rn(score, -maximum));
                        denominator = __dadd_rn(denominator, probability);
                    }
                }
                __syncthreads();
                if (stage == 1u)
                    for (unsigned long long lane = (unsigned long long)thread; lane < head_dim;
                         lane += (unsigned long long)blockDim.x) {
                        unsigned long long offset =
                            (ordinal * query_heads + head) * head_dim + lane;
                        out[offset] += (float)__dmul_rn(probability, (double)row[lane]);
                    }
                __syncthreads();
            }
        }
    }
    if (thread == 0u && (!isfinite(denominator) || denominator <= 0.0)) {
        atomicCAS(status, 0, 1);
        active = 0;
    }
    __syncthreads();
    if (!active) return;
    for (unsigned long long lane = (unsigned long long)thread; lane < head_dim;
         lane += (unsigned long long)blockDim.x) {
        unsigned long long offset =
            (ordinal * query_heads + head) * head_dim + lane;
        float published = (float)__ddiv_rn((double)out[offset],
                                           denominator);
        if (!isfinite(published)) atomicCAS(status, 0, 1);
        else out[offset] = float_to_bf16_rne(published);
    }
}

/* The small-head prefill realization permutes the original reduction tree
 * into lane-private registers, then warp shuffles. Original thread bits4,3,2
 * reduce in registers; bits1,0,7,6,5 reduce across lanes. Thus each ordered
 * FMA, addition, softmax recurrence and final BF16 publication is unchanged.
 * No block-wide synchronization is needed between independent heads. */
extern "C" __global__ void yvex_attention_reduce_native_warp(
    const float *query, const float *local, const unsigned long long *local_positions,
    unsigned long long initial_local_count, unsigned long long local_stride,
    const float *compressed, const unsigned long long *compressed_positions,
    unsigned long long compressed_stride, const unsigned long long *selected,
    const unsigned long long *selected_count_ptr, unsigned long long topk_capacity,
    const float *sinks, unsigned long long query_heads, unsigned long long head_dim,
    unsigned long long sliding_window, unsigned long long ratio, unsigned int attention_class,
    unsigned long long phase_start_position, unsigned long long token_count,
    int candidate_block_visible, float *out, int *status)
{
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned long long task = (unsigned long long)blockIdx.x * 8ull + warp;
    unsigned long long ordinal = query_heads ? task / query_heads : token_count;
    unsigned long long head = query_heads ? task % query_heads : query_heads;
    if (!status || ordinal >= token_count || head >= query_heads) return;
    if (!query || !local || !local_positions || !sinks || !out || !query_heads ||
        !head_dim || head_dim > 512ull || !sliding_window || !token_count || blockDim.x != 256u ||
        attention_class > 2u || (candidate_block_visible != 0 && candidate_block_visible != 1) ||
        phase_start_position > ~0ull - ordinal || local_stride < head_dim ||
        (attention_class != 0u && (!compressed || !compressed_positions || compressed_stride < head_dim)) ||
        (attention_class == 1u && (!selected || !selected_count_ptr || !topk_capacity)) ||
        (attention_class == 1u && ratio != 4ull) || (attention_class == 2u && ratio != 128ull) ||
        (attention_class == 0u && ratio != 0ull)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long position = phase_start_position + ordinal;
    unsigned long long history = position < sliding_window - 1ull ? position : sliding_window - 1ull;
    unsigned long long local_offset = candidate_block_visible ? 0ull : initial_local_count + ordinal - history;
    unsigned long long count = candidate_block_visible ? initial_local_count + token_count : history + 1ull;
    unsigned long long compressed_count = attention_class == 0u ? 0ull : attention_class == 1u ?
        selected_count_ptr[ordinal] : position / ratio + ((position + 1ull) % ratio == 0ull);
    attention_reduce_rows rows = {local, local_positions, compressed, compressed_positions, selected,
        initial_local_count, local_stride, compressed_stride, topk_capacity, sliding_window, ratio,
        phase_start_position, token_count, attention_class, candidate_block_visible};
    const float *q = query + task * head_dim;
    float *output = out + task * head_dim;
    unsigned base = ((lane & 16u) >> 3u) + ((lane & 8u) >> 3u) + ((lane & 7u) << 5u);
    float cached_query[16], cached_row[16], values[16] = {};
#pragma unroll
    for (unsigned part = 0u; part < 2u; ++part) {
#pragma unroll
        for (unsigned j = 0u; j < 8u; ++j) {
            unsigned i = base + j * 4u + part * 256u;
            cached_query[part * 8u + j] = i < head_dim ? q[i] : 0.0f;
            if (i < head_dim) output[i] = 0.0f;
        }
    }
    if (__shfl_sync(0xffffffffu, *status, 0)) return;
    float maximum = sinks[head], denominator = 1.0f;
    for (unsigned pass = 0u; pass < 2u; ++pass)
        for (unsigned long long candidate = 0ull; candidate < (pass ? compressed_count : count); ++candidate) {
            int visible;
            const float *row = attention_reduce_row(&rows, pass, ordinal, candidate, local_offset, &visible);
            if (!visible) continue;
            float dots[8] = {};
#pragma unroll
            for (unsigned part = 0u; part < 2u; ++part) {
#pragma unroll
                for (unsigned j = 0u; j < 8u; ++j) {
                    unsigned i = base + j * 4u + part * 256u;
                    cached_row[part * 8u + j] = i < head_dim ? row[i] : 0.0f;
                    if (i < head_dim) dots[j] = fmaf(cached_query[part * 8u + j], cached_row[part * 8u + j], dots[j]);
                }
            }
#pragma unroll
            for (unsigned j = 0u; j < 4u; ++j) dots[j] = __fadd_rn(dots[j], dots[j + 4u]);
#pragma unroll
            for (unsigned j = 0u; j < 2u; ++j) dots[j] = __fadd_rn(dots[j], dots[j + 2u]);
            float dot = __fadd_rn(dots[0], dots[1]);
            dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 16));
            dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 8));
            dot = __fadd_rn(__fadd_rn(dot, 0.0f), 0.0f);
            dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 4));
            dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 2));
            dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 1));
            float probability = 0.0f, alpha = 1.0f;
            int failed = 0;
            if (!lane) {
                float score = dot * rsqrtf((float)head_dim);
                if (!isfinite(score)) { atomicCAS(status, 0, 1); failed = 1; }
                else if (score > maximum) {
                    alpha = expf(maximum - score); maximum = score; probability = 1.0f;
                    denominator = denominator * alpha + probability;
                } else { probability = expf(score - maximum); denominator += probability; }
            }
            if (__shfl_sync(0xffffffffu, failed, 0)) return;
            probability = __shfl_sync(0xffffffffu, probability, 0);
            alpha = __shfl_sync(0xffffffffu, alpha, 0);
#pragma unroll
            for (unsigned part = 0u; part < 2u; ++part) {
#pragma unroll
                for (unsigned j = 0u; j < 8u; ++j)
                    if (base + j * 4u + part * 256u < head_dim)
                        values[part * 8u + j] = fmaf(
                            probability, cached_row[part * 8u + j],
                            values[part * 8u + j] * alpha);
            }
        }
    denominator = __shfl_sync(0xffffffffu, denominator, 0);
    if (!isfinite(denominator) || denominator <= 0.0f) { if (!lane) atomicCAS(status, 0, 1); return; }
#pragma unroll
    for (unsigned part = 0u; part < 2u; ++part) {
#pragma unroll
        for (unsigned j = 0u; j < 8u; ++j) {
            unsigned i = base + j * 4u + part * 256u;
            if (i < head_dim) {
                float value = values[part * 8u + j] / denominator;
                if (!isfinite(value)) atomicCAS(status, 0, 1);
                else output[i] = float_to_bf16_rne(value);
            }
        }
    }
}

/* Production keeps candidate order and online-softmax semantics while each
 * head uses all resident warps for its dot products. The final BF16 boundary
 * is the numerical contract; forensic evidence continues through the exact
 * kernel above. */
extern "C" __global__ void yvex_attention_reduce_native(
    const float *query,
    const float *local,
    const unsigned long long *local_positions,
    unsigned long long initial_local_count,
    unsigned long long local_stride,
    const float *compressed,
    const unsigned long long *compressed_positions,
    unsigned long long compressed_stride,
    const unsigned long long *selected,
    const unsigned long long *selected_count_ptr,
    unsigned long long topk_capacity,
    const float *sinks,
    unsigned long long query_heads,
    unsigned long long head_dim,
    unsigned long long sliding_window,
    unsigned long long ratio,
    unsigned int attention_class,
    unsigned long long phase_start_position,
    unsigned long long token_count,
    int candidate_block_visible,
    float *out,
    int *status)
{
    /* Retain each lane's query and accumulator, not another state representation.
     * Wider heads keep streaming; candidate/FMA order and the BF16 boundary are unchanged. */
    float cached_query[4], accumulated[4] = {0.0f}, cached_row[4];
    const bool retained = head_dim <= 1024ull;
    __shared__ float warp_sums[8];
    __shared__ float maximum;
    __shared__ float denominator;
    __shared__ float probability;
    __shared__ float renormalization;
    __shared__ int active;
    unsigned long long task = (unsigned long long)blockIdx.x;
    unsigned long long ordinal = query_heads ? task / query_heads : token_count;
    unsigned long long head = query_heads ? task % query_heads : query_heads;
    unsigned long long token_position, local_offset, local_count, compressed_count;
    unsigned int thread = threadIdx.x;
    unsigned int lane = thread & 31u;
    unsigned int warp = thread >> 5u;
    if (!status) return;
    if (ordinal >= token_count || head >= query_heads) return;
    if (!query || !local || !local_positions || !sinks || !out || !query_heads ||
        !head_dim || !sliding_window || !token_count || blockDim.x != 256u ||
        attention_class > 2u ||
        (candidate_block_visible != 0 && candidate_block_visible != 1) ||
        phase_start_position > ~0ull - ordinal || local_stride < head_dim ||
        (attention_class != 0u &&
         (!compressed || !compressed_positions || compressed_stride < head_dim)) ||
        (attention_class == 1u &&
         (!selected || !selected_count_ptr || !topk_capacity)) ||
        (attention_class == 1u && ratio != 4ull) ||
        (attention_class == 2u && ratio != 128ull) ||
        (attention_class == 0u && ratio != 0ull)) {
        if (thread == 0u) atomicCAS(status, 0, 2);
        return;
    }
    token_position = phase_start_position + ordinal;
    if (candidate_block_visible) {
        local_offset = 0ull;
        local_count = initial_local_count + token_count;
    } else {
        unsigned long long local_before = initial_local_count + ordinal;
        unsigned long long history_count = token_position < sliding_window - 1ull
            ? token_position : sliding_window - 1ull;
        local_offset = local_before - history_count;
        local_count = history_count + 1ull;
    }
    compressed_count = attention_class == 0u ? 0ull
        : attention_class == 1u ? selected_count_ptr[ordinal]
        : token_position / ratio + ((token_position + 1ull) % ratio == 0ull);
    const float *q = query + (ordinal * query_heads + head) * head_dim;
    if (thread == 0u) {
        active = *status == 0;
        maximum = sinks[head];
        denominator = 1.0f;
    }
    for (unsigned long long i = (unsigned long long)thread; i < head_dim;
         i += (unsigned long long)blockDim.x)
        out[(ordinal * query_heads + head) * head_dim + i] = 0.0f;
    if (retained) {
#pragma unroll
        for (unsigned int part = 0u; part < 4u; ++part) {
            unsigned long long i = thread + part * 256ull;
            cached_query[part] = i < head_dim ? q[i] : 0.0f;
        }
    }
    __syncthreads();
    if (!active) return;
    attention_reduce_rows rows = {
        local, local_positions, compressed, compressed_positions, selected,
        initial_local_count, local_stride, compressed_stride, topk_capacity,
        sliding_window, ratio, phase_start_position, token_count,
        attention_class, candidate_block_visible
    };
    for (unsigned long long pass = 0ull; pass < 2ull; ++pass) {
        unsigned long long count = pass == 0ull ? local_count : compressed_count;
        for (unsigned long long candidate = 0ull; candidate < count; ++candidate) {
            int visible;
            const float *row = attention_reduce_row(
                &rows, pass, ordinal, candidate, local_offset, &visible);
            if (!visible) continue;
            float dot = 0.0f;
            if (retained) {
#pragma unroll
                for (unsigned int part = 0u; part < 4u; ++part) {
                    unsigned long long i = thread + part * 256ull;
                    cached_row[part] = i < head_dim ? row[i] : 0.0f;
                    if (i < head_dim) dot = fmaf(cached_query[part], cached_row[part], dot);
                }
            } else {
                for (unsigned long long i = thread; i < head_dim; i += blockDim.x)
                    dot = fmaf(q[i], row[i], dot);
            }
            for (unsigned int offset = 16u; offset; offset >>= 1u)
                dot += __shfl_down_sync(0xffffffffu, dot, offset);
            if (lane == 0u) warp_sums[warp] = dot;
            __syncthreads();
            if (warp == 0u) {
                dot = lane < 8u ? warp_sums[lane] : 0.0f;
                for (unsigned int offset = 16u; offset; offset >>= 1u)
                    dot += __shfl_down_sync(0xffffffffu, dot, offset);
                if (lane == 0u) {
                    float score = dot * rsqrtf((float)head_dim);
                    if (!isfinite(score)) {
                        atomicCAS(status, 0, 1);
                        active = 0;
                    } else if (score > maximum) {
                        renormalization = expf(maximum - score);
                        maximum = score;
                        probability = 1.0f;
                        denominator = denominator * renormalization + probability;
                    } else {
                        renormalization = 1.0f;
                        probability = expf(score - maximum);
                        denominator += probability;
                    }
                }
            }
            __syncthreads();
            if (!active) return;
            if (retained) {
#pragma unroll
                for (unsigned int part = 0u; part < 4u; ++part)
                    accumulated[part] = fmaf(probability, cached_row[part],
                                               accumulated[part] * renormalization);
            } else {
                for (unsigned long long i = thread; i < head_dim; i += blockDim.x) {
                    unsigned long long offset = (ordinal * query_heads + head) * head_dim + i;
                    out[offset] = fmaf(probability, row[i], out[offset] * renormalization);
                }
            }
            /* Next dot's barrier follows all readers of the previous softmax factors.
             * Accumulators are lane-private, so no additional barrier is required here. */
        }
    }
    /* Final error publication also waits for every lane to finish consuming active. */
    __syncthreads();
    if (thread == 0u && (!isfinite(denominator) || denominator <= 0.0f)) {
        atomicCAS(status, 0, 1);
        active = 0;
    }
    __syncthreads();
    if (!active) return;
    for (unsigned long long i = (unsigned long long)thread; i < head_dim;
         i += (unsigned long long)blockDim.x) {
        unsigned long long offset =
            (ordinal * query_heads + head) * head_dim + i;
        float published = (retained ? accumulated[i / 256ull] : out[offset]) / denominator;
        if (!isfinite(published)) atomicCAS(status, 0, 1);
        else out[offset] = float_to_bf16_rne(published);
    }
}
