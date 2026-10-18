/*
 * Execute routed and shared MoE kernels from device-resident expert packs.
 *
 * This independently compiled family owns row/expert scheduling while canonical qtype
 * arithmetic remains shared through the CUDA kernel-primitives interface.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include "src/backend/cuda/dot_recovery.h"
#include <yvex/internal/execution_batch.h>
#include <yvex/internal/execution_observation.h>
static __device__ float moe_warp_dot(
    const unsigned char *weight, const unsigned char *activation, unsigned long long extent,
    unsigned long long row_bytes, unsigned int qtype, int q8_input, int *status,
    const unsigned short *grid_table = nullptr, int operand_failure = 1)
{
    if (!q8_input) {
        float sum = qtype_warp_dot(weight, (const float *)activation, extent, qtype,
                                   status, operand_failure);
        if (!(threadIdx.x & 31u) && !isfinite(sum) && !*status)
            sum = qtype_dot_recover_f64(weight, (const float *)activation, extent, qtype);
        return sum;
    }
    /* Specialize common encoded row geometry, not a model or recipe. Constant
     * block counts let CUDA eliminate dynamic group rounds and address divisions.
     * Every path uses the same dot primitive, lane order and exceptional recovery. */
    float sum;
    if (qtype == YVEX_GGUF_QTYPE_IQ2_XXS && extent == 16ull && row_bytes == 16ull * 66ull)
        sum = q8_warp_dot(weight, activation, 16ull, 66ull, YVEX_GGUF_QTYPE_IQ2_XXS, grid_table);
    else if (qtype == YVEX_GGUF_QTYPE_Q2_K && extent == 8ull && row_bytes == 8ull * 84ull)
        sum = q8_warp_dot(weight, activation, 8ull, 84ull, YVEX_GGUF_QTYPE_Q2_K);
    else sum = q8_warp_dot(weight, activation, extent, row_bytes / extent, qtype, grid_table);
    /* Finite rows retain their DP4A order; exceptional rows share one decoded oracle. */
    if (!(threadIdx.x & 31u) && !isfinite(sum) && !*status)
        sum = qtype_q8_dot_recover_f64(weight, activation, extent,
                                       row_bytes / extent, qtype, status, operand_failure);
    return sum;
}

extern "C" __global__ void yvex_moe_route(
    const float *logits, const float *bias, const unsigned long long *hash_experts,
    unsigned int router_class, unsigned long long routed_experts,
    unsigned long long topk, int normalize, double scaling,
    float *scores, unsigned long long *selected, float *weights, int *status)
{
    if (blockIdx.x || threadIdx.x || !status || *status) return;
    if (!logits || !scores || !selected || !weights || !routed_experts ||
        routed_experts > 256ull || !topk || topk > 16ull || topk > routed_experts ||
        router_class > 1u || !isfinite(scaling) || scaling <= 0.0 ||
        (router_class == 0u && !hash_experts) || (router_class == 1u && !bias)) {
        atomicCAS(status, 0, 2);
        return;
    }
    for (unsigned long long expert = 0ull; expert < routed_experts; ++expert) {
        double value = (double)logits[expert];
        double softplus = value > 0.0 ? value + log1p(exp(-value)) : log1p(exp(value));
        double score = sqrt(softplus);
        if (!isfinite(score)) {
            atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTER_SCORE);
            return;
        }
        scores[expert] = (float)score;
    }
    for (unsigned long long rank = 0ull; rank < topk; ++rank) {
        unsigned long long chosen = ~0ull;
        if (router_class == 0u) {
            chosen = hash_experts[rank];
        } else {
            for (unsigned long long candidate = 0ull; candidate < routed_experts; ++candidate) {
                int used = 0;
                for (unsigned long long prior = 0ull; prior < rank; ++prior)
                    if (selected[prior] == candidate) used = 1;
                /* Correction is an F32 operation before ranking, as in the
                 * source Gate and the row-parallel implementation. Promoting
                 * this sum to F64 changes which experts win rounded ties. */
                float candidate_score = __fadd_rn(scores[candidate], bias[candidate]);
                float chosen_score = chosen == ~0ull
                    ? -INFINITY : __fadd_rn(scores[chosen], bias[chosen]);
                if (!used && (chosen == ~0ull || candidate_score > chosen_score ||
                              (candidate_score == chosen_score && candidate < chosen)))
                    chosen = candidate;
            }
        }
        if (chosen >= routed_experts) {
            atomicCAS(status, 0, 2);
            return;
        }
        for (unsigned long long prior = 0ull; prior < rank; ++prior)
            if (selected[prior] == chosen) {
                atomicCAS(status, 0, 2);
                return;
            }
        selected[rank] = chosen;
        weights[rank] = scores[chosen];
    }
    double total = 0.0;
    for (unsigned long long rank = 0ull; rank < topk; ++rank)
        total += (double)weights[rank];
    if (normalize && (!isfinite(total) || total <= 0.0)) {
        atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTER_NORMALIZATION);
        return;
    }
    for (unsigned long long rank = 0ull; rank < topk; ++rank) {
        double value = (double)weights[rank];
        if (normalize) value /= total;
        value *= scaling;
        weights[rank] = (float)value;
    }
}
extern "C" __global__ void yvex_moe_grouped_up(
    const unsigned char *gate, unsigned long long gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned int gate_qtype,
    const unsigned char *up, unsigned long long up_row_bytes,
    unsigned long long up_expert_bytes, unsigned int up_qtype,
    const unsigned long long *selected, const float *weights,
    unsigned long long topk,
    unsigned long long expert_count, const unsigned char *input,
    unsigned long long input_extent, int q8_input,
    unsigned long long intermediate_width,
    double limit, float *intermediate, int *status)
{
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long pair = (unsigned long long)blockIdx.x * 8ull +
                              (unsigned long long)(threadIdx.x >> 5u);
    unsigned long long rank = pair / intermediate_width;
    unsigned long long row = pair % intermediate_width;
    if (!status || *status || rank >= topk) return;
    unsigned long long expert = selected ? selected[rank] : ~0ull;
    if (!gate || !up || !input || !intermediate || expert >= expert_count ||
        !gate_row_bytes || !up_row_bytes || !input_extent || !intermediate_width ||
        !isfinite(limit) || limit <= 0.0) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    const unsigned char *gate_row = gate + expert * gate_expert_bytes + row * gate_row_bytes;
    const unsigned char *up_row = up + expert * up_expert_bytes + row * up_row_bytes;
    float g = 0.0f, u = 0.0f;
    if (q8_input) {
        if (gate_row_bytes % input_extent || up_row_bytes % input_extent) {
            if (!lane) atomicCAS(status, 0, 2);
            return;
        }
    }
    g = moe_warp_dot(gate_row, input, input_extent, gate_row_bytes,
                     gate_qtype, q8_input, status, nullptr, YVEX_MOE_DEVICE_GATE_OPERAND);
    u = moe_warp_dot(up_row, input, input_extent, up_row_bytes,
                     up_qtype, q8_input, status, nullptr, YVEX_MOE_DEVICE_UP_OPERAND);
    if (!lane && !*status) {
        float route_weight = weights ? weights[rank] : 1.0f;
        float value;
        if (!cuda_clamped_swiglu_bf16_value(g, u, limit, route_weight, &value))
            atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTED_ACTIVATION);
        else intermediate[rank * intermediate_width + row] = value;
    }
}

extern "C" __global__ void yvex_moe_grouped_down(
    const unsigned char *down, unsigned long long row_bytes,
    unsigned long long expert_bytes, unsigned int qtype,
    const unsigned long long *selected,
    unsigned long long topk, unsigned long long expert_count,
    const unsigned char *intermediate, unsigned long long intermediate_extent,
    int q8_input,
    unsigned long long hidden, float *routed, int *status)
{
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long row = (unsigned long long)blockIdx.x * 8ull +
                             (unsigned long long)(threadIdx.x >> 5u);
    float total = 0.0f;
    if (!status || *status || row >= hidden) return;
    if (!down || !selected || !intermediate || !routed ||
        !row_bytes || !intermediate_extent || !topk ||
        (q8_input && row_bytes % intermediate_extent)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    for (unsigned long long rank = 0ull; rank < topk; ++rank) {
        unsigned long long expert = selected[rank];
        if (expert >= expert_count) { if (!lane) atomicCAS(status, 0, 2); return; }
        const unsigned char *weight = down + expert * expert_bytes + row * row_bytes;
        const unsigned char *activation = intermediate + rank * intermediate_extent *
            (q8_input ? YVEX_CUDA_Q8_K_BYTES : sizeof(float));
        float dot = moe_warp_dot(weight, activation, intermediate_extent,
                                 row_bytes, qtype, q8_input, status, nullptr,
                                 YVEX_MOE_DEVICE_DOWN_OPERAND);
        if (!lane && !*status) {
            total = __fadd_rn(total, float_to_bf16_rne(dot));
        }
    }
    if (!lane && !*status) routed[row] = total;
}

/* Route every row independently while retaining one launch and one bounded publication. */
extern "C" __global__ void yvex_moe_route_rows(
    const float *logits, const float *bias, const int32_t *hash_table,
    const unsigned int *token_ids, unsigned int router_class,
    unsigned long long row_count, unsigned long long routed_experts,
    unsigned long long topk, unsigned long long hash_rows,
    unsigned long long hash_columns, unsigned long long hash_row_bytes,
    int normalize, double scaling, float *scores,
    unsigned long long *selected, float *weights, int *status)
{
    __shared__ float route_scores[256], rank_scores[256];
    __shared__ unsigned long long rank_experts[256];
    unsigned long long row = (unsigned long long)blockIdx.x;
    unsigned int thread = threadIdx.x;
    if (!status || row >= row_count) return;
    if (thread == 0u) {
        if (*status == 0 && (!logits || !scores || !selected || !weights || !row_count ||
            !routed_experts || routed_experts > 256ull || !topk || topk > 16ull ||
            topk > routed_experts || router_class > 1u || !isfinite(scaling) ||
            scaling <= 0.0 || (router_class == 0u &&
            (!hash_table || !token_ids || hash_columns < topk ||
             hash_row_bytes < hash_columns * sizeof(int32_t))) ||
            (router_class == 1u && !bias))) {
            atomicCAS(status, 0, 2);
        }
    }
    __syncthreads();
    if (*status) return;
    const float *row_logits = logits + row * routed_experts;
    float *row_scores = scores + row * routed_experts, *row_weights = weights + row * topk;
    unsigned long long *row_selected = selected + row * topk;
    /* Learned top-k stays expert-parallel with source tie-breaking and weight rank order. */
    for (unsigned long long expert = thread; expert < routed_experts;
         expert += blockDim.x) {
        double value = (double)row_logits[expert];
        double softplus = value > 0.0 ? value + log1p(exp(-value)) : log1p(exp(value));
        float score = (float)sqrt(softplus);
        if (!isfinite(score)) atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTER_SCORE);
        route_scores[expert] = score;
        row_scores[expert] = score;
    }
    __syncthreads();
    if (*status) return;
    if (router_class == 0u && thread) return;
    if (router_class == 0u) {
        for (unsigned long long rank = 0ull; rank < topk; ++rank) {
            unsigned int token = token_ids[row];
            if ((unsigned long long)token >= hash_rows) {
                atomicCAS(status, 0, 2);
                return;
            }
            const int32_t *hash_row = (const int32_t *)((const unsigned char *)hash_table +
                (unsigned long long)token * hash_row_bytes);
            unsigned long long chosen = hash_row[rank] < 0 ? ~0ull : (unsigned long long)hash_row[rank];
            if (chosen >= routed_experts) { atomicCAS(status, 0, 2); return; }
            for (unsigned long long prior = 0ull; prior < rank; ++prior)
                if (row_selected[prior] == chosen) {
                    atomicCAS(status, 0, 2);
                    return;
                }
            row_selected[rank] = chosen;
            row_weights[rank] = route_scores[chosen];
        }
    } else {
        for (unsigned long long rank = 0ull; rank < topk; ++rank) {
            int used = 0;
            if ((unsigned long long)thread < routed_experts)
                for (unsigned long long prior = 0ull; prior < rank; ++prior)
                    if (row_selected[prior] == (unsigned long long)thread) used = 1;
            rank_scores[thread] = !used && (unsigned long long)thread < routed_experts
                                      ? route_scores[thread] + bias[thread] : -INFINITY;
            rank_experts[thread] = (unsigned long long)thread;
            __syncthreads();
            for (unsigned int stride = blockDim.x >> 1u; stride; stride >>= 1u) {
                if (thread < stride) {
                    float right_score = rank_scores[thread + stride];
                    unsigned long long right_expert = rank_experts[thread + stride];
                    if (right_score > rank_scores[thread] ||
                        (right_score == rank_scores[thread] &&
                         right_expert < rank_experts[thread])) {
                        rank_scores[thread] = right_score;
                        rank_experts[thread] = right_expert;
                    }
                }
                __syncthreads();
            }
            if (thread == 0u) {
                unsigned long long chosen = rank_experts[0];
                if (chosen >= routed_experts || !isfinite(rank_scores[0]))
                    atomicCAS(status, 0, 2);
                else {
                    row_selected[rank] = chosen;
                    row_weights[rank] = route_scores[chosen];
                }
            }
            __syncthreads();
            if (*status) return;
        }
    }
    if (thread) return;
    double total = 0.0;
    for (unsigned long long rank = 0ull; rank < topk; ++rank)
        total += (double)row_weights[rank];
    if (normalize && (!isfinite(total) || total <= 0.0)) {
        atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTER_NORMALIZATION);
        return;
    }
    for (unsigned long long rank = 0ull; rank < topk; ++rank) {
        double value = (double)row_weights[rank];
        if (normalize) value /= total;
        row_weights[rank] = (float)(value * scaling);
    }
}

/* Materialize the canonical expert-major worklist from already-admitted route pairs. */
extern "C" __global__ void yvex_expert_worklist_build_cuda(
    const unsigned long long *selected, unsigned long long row_count,
    unsigned long long topk, unsigned long long expert_count,
    unsigned long long *order, unsigned long long *expert_ids,
    unsigned long long *bucket_offsets, unsigned long long *bucket_populations,
    unsigned long long *source_rows, unsigned long long *destination_rows,
    yvex_expert_worklist_observation *summary,
    unsigned long long supported_width_mask,
    unsigned long long prefill_maximum_width, unsigned int phase,
    unsigned long long tensor_core_minimum,
    unsigned long long admitted_width, unsigned int provenance, int *status,
    unsigned long long *matrix_tiles)
{
    __shared__ unsigned long long counts[256];
    __shared__ unsigned long long offsets[256];
    __shared__ unsigned int tile_offsets[256], bucket_ordinals[256];
    __shared__ int active;
    unsigned int thread = threadIdx.x;
    unsigned long long pairs;
    if (!status || blockIdx.x) return;
    if (thread == 0u) {
        active = *status == 0;
        if (active && (!selected || !row_count || !topk || !expert_count ||
            blockDim.x != 256u || expert_count > blockDim.x || !order || !expert_ids || !bucket_offsets ||
            !bucket_populations || !source_rows || !destination_rows || !summary ||
            !(supported_width_mask & 2ull) || supported_width_mask & 1ull ||
            (row_count < 63ull ? !(supported_width_mask & (1ull << row_count)) :
             (phase != YVEX_EXECUTION_PHASE_PREFILL ||
              supported_width_mask != 0x7ffffffffffffffeull ||
              row_count > prefill_maximum_width ||
              prefill_maximum_width > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH)) ||
            !admitted_width || provenance > YVEX_EXECUTION_BATCH_COMPILED_COMPATIBLE ||
            row_count > ~0ull / topk)) {
            atomicCAS(status, 0, 2);
            active = 0;
        }
    }
    __syncthreads();
    if (!active) return;
    pairs = row_count * topk;
    unsigned long long count = 0ull;
    if ((unsigned long long)thread < expert_count) {
        for (unsigned long long pair = 0ull; pair < pairs; ++pair) {
            unsigned long long expert = selected[pair];
            if (expert >= expert_count) {
                atomicCAS(status, 0, 2);
                atomicExch(&active, 0);
            }
            if (expert == (unsigned long long)thread) count++;
        }
    }
    counts[thread] = count;
    __syncthreads();
    if (!active) return;
    if (thread == 0u) {
        unsigned long long emitted = 0ull, unique = 0ull;
        unsigned int tiles = 0u;
        summary->schema_version = YVEX_EXPERT_WORKLIST_OBSERVATION_SCHEMA_V1;
        summary->worklist_count = 1ull;
        summary->pair_count = pairs;
        summary->bucket_count = 0ull;
        summary->maximum_bucket_population = 0ull;
        summary->matrix_tile_eligible_pairs = 0ull;
        summary->matrix_tile_executed_pairs = 0ull;
        summary->narrow_pairs = 0ull;
        summary->tail_rows = 0ull;
        for (unsigned int index = 0u; index < YVEX_EXPERT_WORKLIST_HISTOGRAM_CAP;
             ++index) {
            summary->width_histogram[index] = 0ull;
            summary->population_histogram[index] = 0ull;
        }
        for (unsigned int index = 0u;
             index <= YVEX_EXECUTION_BATCH_COMPILED_COMPATIBLE; ++index)
            summary->provenance_counts[index] = 0ull;
        summary->width_histogram[
            row_count < YVEX_EXPERT_WORKLIST_HISTOGRAM_CAP
                ? row_count : YVEX_EXPERT_WORKLIST_HISTOGRAM_CAP - 1u] = 1ull;
        summary->provenance_counts[provenance] = 1ull;
        for (unsigned long long expert = 0ull; expert < expert_count; ++expert) {
            offsets[expert] = emitted;
            emitted += counts[expert];
            if (!counts[expert]) continue;
            expert_ids[unique] = expert;
            bucket_offsets[unique] = offsets[expert];
            bucket_populations[unique] = counts[expert];
            if (counts[expert] > summary->maximum_bucket_population)
                summary->maximum_bucket_population = counts[expert];
            unsigned long long histogram =
                counts[expert] < YVEX_EXPERT_WORKLIST_HISTOGRAM_CAP
                    ? counts[expert] : YVEX_EXPERT_WORKLIST_HISTOGRAM_CAP - 1u;
            summary->population_histogram[histogram]++;
            if (tensor_core_minimum && counts[expert] >= tensor_core_minimum) {
                tile_offsets[expert] = tiles;
                bucket_ordinals[expert] = (unsigned int)unique;
                if (counts[expert] > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH) {
                    atomicCAS(status, 0, 2);
                    active = 0;
                } else tiles += (unsigned int)((counts[expert] + 7ull) / 8ull);
                summary->matrix_tile_eligible_pairs += counts[expert];
                if (counts[expert] % admitted_width)
                    summary->tail_rows += admitted_width - counts[expert] % admitted_width;
            } else summary->narrow_pairs += counts[expert];
            unique++;
        }
        if (emitted != pairs) {
            atomicCAS(status, 0, 2);
            active = 0;
        } else {
            bucket_offsets[unique] = emitted;
            summary->bucket_count = unique;
            if (matrix_tiles) matrix_tiles[0] = tiles;
        }
    }
    __syncthreads();
    if (!active || (unsigned long long)thread >= expert_count) return;
    /* One descriptor per real matrix column tile, shared by every output
     * row tile of gate/up and down. Header is the exact tile population;
     * conservative launch padding never reads an uninitialized descriptor. */
    if (matrix_tiles && tensor_core_minimum && counts[thread] >= tensor_core_minimum)
        for (unsigned int column = 0u; column < counts[thread]; column += 8u)
            matrix_tiles[1u + tile_offsets[thread] + column / 8u] =
                ((unsigned long long)bucket_ordinals[thread] << 32u) | column;
    unsigned long long cursor = offsets[thread];
    for (unsigned long long pair = 0ull; pair < pairs; ++pair)
        if (selected[pair] == (unsigned long long)thread) {
            order[cursor] = pair;
            source_rows[cursor] = pair / topk;
            destination_rows[cursor] = pair;
            cursor++;
        }
}

/* Matrix and scalar work partition the same expert buckets. Scalar blocks
 * visit only the sub-threshold bucket's real pairs; no binary search or
 * pair-sized grid is needed to discover that a matrix bucket owns the work. */
static __device__ int worklist_scalar_span(
    unsigned long long task, unsigned long long pair_count,
    unsigned long long expert_count, const unsigned long long *bucket_offsets,
    const unsigned long long *bucket_populations,
    const yvex_expert_worklist_observation *summary,
    unsigned long long tensor_core_minimum, unsigned long long *first,
    unsigned long long *count, int *status)
{
    if (!tensor_core_minimum) {
        *first = task;
        *count = 1ull;
        return task < pair_count;
    }
    if (!bucket_offsets || !bucket_populations || !summary ||
        !summary->bucket_count || summary->bucket_count > expert_count ||
        summary->bucket_count > pair_count) {
        atomicCAS(status, 0, 2);
        return 0;
    }
    if (task >= summary->bucket_count) return 0;
    *first = bucket_offsets[task];
    *count = bucket_populations[task];
    if (!*count || *first > pair_count || *count > pair_count - *first) {
        atomicCAS(status, 0, 2);
        return 0;
    }
    return *count < tensor_core_minimum;
}

/* Eight lanes per encoded row share gate/up activation loads. Decode uses
 * the same integer K256 products and reconstructs the original 32-lane tree,
 * rather than inheriting the ordinary matrix contraction's F32 ordering. */
static __device__ __forceinline__ int moe_iq2_signed_word(
    unsigned code, unsigned signs, unsigned part, const unsigned *grid)
{
    unsigned packed = grid[code * 2u + part], bits = (signs >> (part * 4u)) & 15u;
    unsigned mask = ((bits * 0x00204081u) & 0x01010101u) * 255u;
    return (int)__vsub4(packed ^ mask, mask);
}

static __device__ __forceinline__ float2 moe_iq2_narrow_block(
    const unsigned char *gate, const unsigned char *up, const unsigned char *x,
    unsigned long long row, unsigned block, unsigned blocks, const unsigned *grid)
{
    const unsigned char *w[2] = {gate + (row * blocks + block) * 66ull,
                                up + (row * blocks + block) * 66ull};
    int products[2] = {};
    for (unsigned segment = 0u; segment < 8u; ++segment) {
        unsigned codes[2], ss[2]; int subtotal[2] = {};
#pragma unroll
        for (unsigned which = 0u; which < 2u; ++which) {
            codes[which] = qtype_load_u32(w[which] + 2u + segment * 8u);
            ss[which] = qtype_load_u32(w[which] + 6u + segment * 8u);
        }
#pragma unroll
        for (unsigned group = 0u; group < 4u; ++group) {
            int a = (int)qtype_load_u32(x + 4u + segment * 32u + group * 8u);
            int b = (int)qtype_load_u32(x + 8u + segment * 32u + group * 8u);
#pragma unroll
            for (unsigned which = 0u; which < 2u; ++which) {
                unsigned code = (codes[which] >> (group * 8u)) & 255u;
                unsigned signs = iq2_xxs_signs((ss[which] >> (group * 7u)) & 127u);
                subtotal[which] = __dp4a(moe_iq2_signed_word(code, signs, 0u, grid), a, subtotal[which]);
                subtotal[which] = __dp4a(moe_iq2_signed_word(code, signs, 1u, grid), b, subtotal[which]);
            }
        }
#pragma unroll
        for (unsigned which = 0u; which < 2u; ++which)
            products[which] += subtotal[which] * (int)(2u * (ss[which] >> 28u) + 1u);
    }
    float xs = __uint_as_float(qtype_load_u32(x)), values[2];
#pragma unroll
    for (unsigned which = 0u; which < 2u; ++which)
        values[which] = 0.125f * f16_bits_to_float(qtype_load_u16(w[which])) * xs * (float)products[which];
    return make_float2(values[0], values[1]);
}

static __device__ __forceinline__ void moe_iq2_narrow_rows(
    const unsigned char *gate, unsigned long long grb, unsigned long long geb, unsigned gq,
    const unsigned char *up, unsigned long long urb, unsigned long long ueb, unsigned uq,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *offsets, const unsigned long long *populations,
    const yvex_expert_worklist_observation *summary, unsigned long long minimum,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    const unsigned char *input, unsigned long long extent, int q8,
    unsigned long long width, double limit, float *out, int *status)
{
    __shared__ unsigned grid[512];
    __shared__ int active;
    if (!threadIdx.x) active = status && !atomicAdd(status, 0);
    for (unsigned i = threadIdx.x; i < 256u; i += blockDim.x) {
        unsigned short code = iq2_xxs_grid[i];
        for (unsigned part = 0u; part < 2u; ++part) {
            unsigned packed = 0u;
            for (unsigned j = 0u; j < 4u; ++j) {
                unsigned level = (code >> (2u * (part * 4u + j))) & 3u;
                packed |= (level == 0u ? 8u : level == 1u ? 25u : 43u) << (8u * j);
            }
            grid[i * 2u + part] = packed;
        }
    }
    __syncthreads();
    if (!active) return;
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u, k_lane = lane & 7u;
    if (blockDim.x != 128u || !gate || !up || !input || !out || !topk || !experts ||
        !pairs || pairs % topk || !width || (extent != 16ull && extent != 32ull) || !q8 ||
        gq != YVEX_GGUF_QTYPE_IQ2_XXS || uq != gq || grb != extent * 66ull || urb != grb ||
        width > ~0ull / grb || geb != width * grb || ueb != geb || !isfinite(limit) || limit <= 0.0) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long row_tiles = width / 16ull + (width % 16ull != 0ull), first, count;
    if (!worklist_scalar_span(blockIdx.x / row_tiles, pairs, experts, offsets,
            populations, summary, minimum, &first, &count, status)) return;
    unsigned long long row = blockIdx.x % row_tiles * 16ull + warp * 4u + (lane >> 3u);
    for (unsigned long long ordinal = 0ull; ordinal < count; ++ordinal) {
        unsigned long long pair = first + ordinal, source_pair = order ? order[pair] : pair;
        if (source_pair >= pairs) { if (!lane) atomicCAS(status, 0, 2); return; }
        unsigned long long expert = selected ? selected[source_pair] : 0ull;
        if (expert >= experts) { if (!lane) atomicCAS(status, 0, 2); return; }
        const unsigned char *g = gate + expert * geb, *u = up + expert * ueb;
        const unsigned char *x = input + source_pair / topk * extent * 292ull;
        float2 leaves[4] = {};
#pragma unroll
        for (unsigned round = 0u; round < 4u; ++round) {
            unsigned block = k_lane + round * 8u;
            if (row < width && block < extent)
                leaves[round] = moe_iq2_narrow_block(g, u, x + block * 292ull, row, block, (unsigned)extent, grid);
            if (extent == 32ull) {
                leaves[round].x = __fadd_rn(0.0f, leaves[round].x);
                leaves[round].y = __fadd_rn(0.0f, leaves[round].y);
            }
        }
        float gv = __fadd_rn(__fadd_rn(leaves[0].x, leaves[2].x), __fadd_rn(leaves[1].x, leaves[3].x));
        float uv = __fadd_rn(__fadd_rn(leaves[0].y, leaves[2].y), __fadd_rn(leaves[1].y, leaves[3].y));
        for (unsigned step = 4u; step; step >>= 1u) {
            gv = __fadd_rn(gv, __shfl_down_sync(0xffffffffu, gv, step, 8));
            uv = __fadd_rn(uv, __shfl_down_sync(0xffffffffu, uv, step, 8));
        }
        if (!k_lane && row < width) {
            if (!isfinite(gv)) gv = qtype_q8_dot_recover_f64(g + row * grb, x, extent,
                66ull, gq, status, YVEX_MOE_DEVICE_GATE_OPERAND);
            if (!isfinite(uv)) uv = qtype_q8_dot_recover_f64(u + row * urb, x, extent,
                66ull, uq, status, YVEX_MOE_DEVICE_UP_OPERAND);
            float value;
            if (!cuda_clamped_swiglu_bf16_value(gv, uv, limit, weights ? weights[source_pair] : 1.0f, &value))
                atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTED_ACTIVATION);
            else if (!*status) out[pair * width + row] = value;
        }
    }
}

template<bool Q2_GEOMETRY>
static __device__ __forceinline__ void moe_grouped_up_rows(
    const unsigned char *gate, unsigned long long supplied_gate_row_bytes,
    unsigned long long gate_expert_bytes, unsigned int supplied_gate_qtype,
    const unsigned char *up, unsigned long long supplied_up_row_bytes,
    unsigned long long up_expert_bytes, unsigned int supplied_up_qtype,
    const unsigned long long *selected, const float *weights,
    const unsigned long long *order,
    const unsigned long long *bucket_offsets,
    const unsigned long long *bucket_populations,
    const yvex_expert_worklist_observation *summary,
    unsigned long long tensor_core_minimum,
    unsigned long long pair_count, unsigned long long topk,
    unsigned long long expert_count, const unsigned char *input,
    unsigned long long supplied_input_extent, int supplied_q8_input,
    unsigned long long intermediate_width, double limit,
    float *intermediate, int *status)
{
    const unsigned long long input_extent = Q2_GEOMETRY ? 16ull : supplied_input_extent;
    const unsigned long long gate_row_bytes = Q2_GEOMETRY ? 1344ull : supplied_gate_row_bytes;
    const unsigned long long up_row_bytes = Q2_GEOMETRY ? 1344ull : supplied_up_row_bytes;
    const unsigned gate_qtype = Q2_GEOMETRY ? YVEX_GGUF_QTYPE_Q2_K : supplied_gate_qtype;
    const unsigned up_qtype = Q2_GEOMETRY ? YVEX_GGUF_QTYPE_Q2_K : supplied_up_qtype;
    const int q8_input = Q2_GEOMETRY ? 1 : supplied_q8_input;
    if (Q2_GEOMETRY && (supplied_input_extent != input_extent ||
        supplied_gate_row_bytes != gate_row_bytes || supplied_up_row_bytes != up_row_bytes ||
        supplied_gate_qtype != gate_qtype || supplied_up_qtype != up_qtype || supplied_q8_input != 1)) {
        if (status && !threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    extern __shared__ float staged_activation[];
    __shared__ unsigned short grid_table[256];
    /* Copy the canonical table before any status-dependent exit. Lane-varying
     * lookup then uses block-local storage, with unchanged codes and dot order. */
    if (q8_input && (gate_qtype == YVEX_GGUF_QTYPE_IQ2_XXS || up_qtype == YVEX_GGUF_QTYPE_IQ2_XXS)) {
        for (unsigned int i = threadIdx.x; i < 256u; i += blockDim.x)
            grid_table[i] = iq2_xxs_grid[i];
        __syncthreads();
    }
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long warp = (unsigned long long)(threadIdx.x >> 5u);
    if (!status || *status) return;
    if (!gate || !up || !input || !intermediate || !topk ||
        !gate_row_bytes || !up_row_bytes || !input_extent ||
        !intermediate_width || !isfinite(limit) || limit <= 0.0 ||
        (tensor_core_minimum && !q8_input)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long rows_per_pair = intermediate_width / 8ull +
                                       (intermediate_width % 8ull != 0ull);
    unsigned long long task = blockIdx.x / rows_per_pair;
    unsigned long long output_row = (blockIdx.x % rows_per_pair) * 8ull + warp;
    unsigned long long first, count;
    if (!worklist_scalar_span(task, pair_count, expert_count,
            bucket_offsets, bucket_populations, summary,
            tensor_core_minimum, &first, &count, status)) return;
    for (unsigned long long ordinal = 0ull; ordinal < count; ++ordinal) {
    unsigned long long ordered_pair = first + ordinal;
    unsigned long long source_pair = order ? order[ordered_pair] : ordered_pair;
    if (source_pair >= pair_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long source_row = source_pair / topk;
    unsigned long long expert = selected ? selected[source_pair] : 0ull;
    if (expert >= expert_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    const unsigned char *activation = input + source_row * input_extent *
                                      (q8_input ? YVEX_CUDA_Q8_K_BYTES : sizeof(float));
    const unsigned char *gate_row;
    const unsigned char *up_row;
    float g, u;
    if (q8_input) {
        if (gate_row_bytes % input_extent || up_row_bytes % input_extent) {
            if (!lane) atomicCAS(status, 0, 2);
            return;
        }
        if (*status) return;
    } else {
        const float *source = (const float *)activation;
        for (unsigned long long index = threadIdx.x; index < input_extent;
             index += blockDim.x)
            staged_activation[index] = source[index];
        __syncthreads();
        if (*status) return;
        activation = (const unsigned char *)staged_activation;
    }
    if (output_row >= intermediate_width) return;
    gate_row = gate + expert * gate_expert_bytes + output_row * gate_row_bytes;
    up_row = up + expert * up_expert_bytes + output_row * up_row_bytes;
    g = moe_warp_dot(gate_row, activation, input_extent, gate_row_bytes,
                     gate_qtype, q8_input, status, grid_table, YVEX_MOE_DEVICE_GATE_OPERAND);
    u = moe_warp_dot(up_row, activation, input_extent, up_row_bytes,
                     up_qtype, q8_input, status, grid_table, YVEX_MOE_DEVICE_UP_OPERAND);
    if (!lane && !*status) {
        float route_weight = weights ? weights[source_pair] : 1.0f;
        float value;
        if (!cuda_clamped_swiglu_bf16_value(g, u, limit, route_weight, &value))
            atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTED_ACTIVATION);
        else intermediate[ordered_pair * intermediate_width + output_row] = value;
    }
    }
}

template<bool Q2_GEOMETRY>
static __device__ __forceinline__ void moe_grouped_down_rows(
    const unsigned char *down, unsigned long long supplied_row_bytes,
    unsigned long long expert_bytes, unsigned int supplied_qtype,
    const unsigned long long *selected,
    const unsigned long long *order,
    const unsigned long long *bucket_offsets,
    const unsigned long long *bucket_populations,
    const yvex_expert_worklist_observation *summary,
    unsigned long long tensor_core_minimum, unsigned long long pair_count,
    unsigned long long topk, unsigned long long expert_count,
    const unsigned char *intermediate, unsigned long long supplied_intermediate_extent,
    int supplied_q8_input, unsigned long long hidden, float *pair_outputs, int *status)
{
    const unsigned long long intermediate_extent = Q2_GEOMETRY ? 8ull : supplied_intermediate_extent;
    const unsigned long long row_bytes = Q2_GEOMETRY ? 672ull : supplied_row_bytes;
    const unsigned qtype = Q2_GEOMETRY ? YVEX_GGUF_QTYPE_Q2_K : supplied_qtype;
    const int q8_input = Q2_GEOMETRY ? 1 : supplied_q8_input;
    if (Q2_GEOMETRY && (supplied_intermediate_extent != intermediate_extent ||
        supplied_row_bytes != row_bytes || supplied_qtype != qtype || supplied_q8_input != 1)) {
        if (status && !threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    extern __shared__ float staged_activation[];
    __shared__ unsigned short grid_table[256];
    if (q8_input && qtype == YVEX_GGUF_QTYPE_IQ2_XXS) {
        for (unsigned int i = threadIdx.x; i < 256u; i += blockDim.x)
            grid_table[i] = iq2_xxs_grid[i];
        __syncthreads();
    }
    unsigned int lane = threadIdx.x & 31u;
    unsigned long long warp = (unsigned long long)(threadIdx.x >> 5u);
    if (!status || *status) return;
    if (!down || !intermediate || !pair_outputs || !topk || !row_bytes || !hidden ||
        !intermediate_extent || (q8_input && row_bytes % intermediate_extent) ||
        (tensor_core_minimum && !q8_input)) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long rows_per_pair = hidden / 8ull + (hidden % 8ull != 0ull);
    unsigned long long task = blockIdx.x / rows_per_pair;
    unsigned long long output_row = (blockIdx.x % rows_per_pair) * 8ull + warp;
    unsigned long long first, count;
    if (!worklist_scalar_span(task, pair_count, expert_count,
            bucket_offsets, bucket_populations, summary,
            tensor_core_minimum, &first, &count, status)) return;
    for (unsigned long long ordinal = 0ull; ordinal < count; ++ordinal) {
    unsigned long long ordered_pair = first + ordinal;
    unsigned long long source_pair = order ? order[ordered_pair] : ordered_pair;
    if (source_pair >= pair_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long expert = selected ? selected[source_pair] : 0ull;
    if (expert >= expert_count) {
        if (!lane) atomicCAS(status, 0, 2);
        return;
    }
    const unsigned char *activation = intermediate + ordered_pair * intermediate_extent *
                                      (q8_input ? YVEX_CUDA_Q8_K_BYTES : sizeof(float));
    const unsigned char *weight;
    if (q8_input) {
        if (*status) return;
    } else {
        const float *source = (const float *)activation;
        for (unsigned long long index = threadIdx.x; index < intermediate_extent;
             index += blockDim.x)
            staged_activation[index] = source[index];
        __syncthreads();
        if (*status) return;
        activation = (const unsigned char *)staged_activation;
    }
    if (output_row >= hidden) return;
    weight = down + expert * expert_bytes + output_row * row_bytes;
    float dot = moe_warp_dot(weight, activation, intermediate_extent,
                             row_bytes, qtype, q8_input, status, grid_table,
                             YVEX_MOE_DEVICE_DOWN_OPERAND);
    if (!lane && !*status) {
        float value = float_to_bf16_rne(dot);
        if (!isfinite(value)) atomicCAS(status, 0, YVEX_MOE_DEVICE_DOWN_PUBLICATION);
        else pair_outputs[source_pair * hidden + output_row] = value;
    }
    }
}

/* Constant encoded geometry removes dynamic dispatch and address arithmetic
 * from the real small-population path. Both entrypoints retain the original
 * dot, reduction tree, worklist associations and exceptional recovery. */
extern "C" __global__ void yvex_moe_grouped_up_rows(
    const unsigned char *gate, unsigned long long grb, unsigned long long geb, unsigned gq,
    const unsigned char *up, unsigned long long urb, unsigned long long ueb, unsigned uq,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *offsets, const unsigned long long *populations,
    const yvex_expert_worklist_observation *summary, unsigned long long minimum,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    const unsigned char *input, unsigned long long extent, int q8,
    unsigned long long width, double limit, float *out, int *status)
{
    if (blockDim.x == 128u) {
        moe_iq2_narrow_rows(gate, grb, geb, gq, up, urb, ueb, uq, selected, weights,
            order, offsets, populations, summary, minimum, pairs, topk, experts,
            input, extent, q8, width, limit, out, status);
        return;
    }
    moe_grouped_up_rows<false>(gate, grb, geb, gq, up, urb, ueb, uq, selected, weights,
        order, offsets, populations, summary, minimum, pairs, topk, experts,
        input, extent, q8, width, limit, out, status);
}

extern "C" __global__ void yvex_moe_grouped_up_q2_rows(
    const unsigned char *gate, unsigned long long grb, unsigned long long geb, unsigned gq,
    const unsigned char *up, unsigned long long urb, unsigned long long ueb, unsigned uq,
    const unsigned long long *selected, const float *weights, const unsigned long long *order,
    const unsigned long long *offsets, const unsigned long long *populations,
    const yvex_expert_worklist_observation *summary, unsigned long long minimum,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    const unsigned char *input, unsigned long long extent, int q8,
    unsigned long long width, double limit, float *out, int *status)
{
    moe_grouped_up_rows<true>(gate, grb, geb, gq, up, urb, ueb, uq, selected, weights,
        order, offsets, populations, summary, minimum, pairs, topk, experts,
        input, extent, q8, width, limit, out, status);
}

extern "C" __global__ void yvex_moe_grouped_down_rows(
    const unsigned char *down, unsigned long long rb, unsigned long long eb, unsigned qtype,
    const unsigned long long *selected, const unsigned long long *order,
    const unsigned long long *offsets, const unsigned long long *populations,
    const yvex_expert_worklist_observation *summary, unsigned long long minimum,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    const unsigned char *input, unsigned long long extent, int q8,
    unsigned long long width, float *out, int *status)
{
    moe_grouped_down_rows<false>(down, rb, eb, qtype, selected, order, offsets, populations,
        summary, minimum, pairs, topk, experts, input, extent, q8, width, out, status);
}

extern "C" __global__ void yvex_moe_grouped_down_q2_rows(
    const unsigned char *down, unsigned long long rb, unsigned long long eb, unsigned qtype,
    const unsigned long long *selected, const unsigned long long *order,
    const unsigned long long *offsets, const unsigned long long *populations,
    const yvex_expert_worklist_observation *summary, unsigned long long minimum,
    unsigned long long pairs, unsigned long long topk, unsigned long long experts,
    const unsigned char *input, unsigned long long extent, int q8,
    unsigned long long width, float *out, int *status)
{
    moe_grouped_down_rows<true>(down, rb, eb, qtype, selected, order, offsets, populations,
        summary, minimum, pairs, topk, experts, input, extent, q8, width, out, status);
}

extern "C" __global__ void yvex_moe_reduce_rows(
    const float *pair_outputs, unsigned long long row_count,
    unsigned long long topk, unsigned long long hidden,
    float *output, int *status)
{
    unsigned long long index = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (!status || *status || index >= row_count * hidden) return;
    if (!pair_outputs || !row_count || !topk || !hidden || !output) {
        atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long row = index / hidden, lane = index % hidden;
    float total = 0.0f;
    for (unsigned long long rank = 0ull; rank < topk; ++rank)
        total = __fadd_rn(total, pair_outputs[(row * topk + rank) * hidden + lane]);
    if (!isfinite(total)) atomicCAS(status, 0, YVEX_MOE_DEVICE_ROUTED_REDUCTION);
    else output[index] = total;
}

extern "C" __global__ void yvex_moe_combine_rows(
    const float *routed, const float *shared, unsigned long long count,
    float *combined, int *status)
{
    unsigned long long index = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (!status || *status || index >= count) return;
    if (!routed || !shared || !combined || !count) {
        atomicCAS(status, 0, 2);
        return;
    }
    float value = float_to_bf16_rne(__fadd_rn(routed[index], shared[index]));
    if (!isfinite(value)) atomicCAS(status, 0, YVEX_MOE_DEVICE_SHARED_COMBINATION);
    else combined[index] = value;
}

extern "C" __global__ void yvex_moe_swiglu(
    const float *gate, const float *up, unsigned long long count,
    double limit, float route_weight, float *output, int *status)
{
    unsigned long long index = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (!status || *status || index >= count) return;
    if (!gate || !up || !output || !isfinite(limit) || limit <= 0.0) {
        atomicCAS(status, 0, 2);
        return;
    }
    float value;
    if (!cuda_clamped_swiglu_bf16_value(gate[index], up[index], limit, route_weight, &value))
        atomicCAS(status, 0, YVEX_MOE_DEVICE_SHARED_ACTIVATION);
    else output[index] = value;
}

extern "C" __global__ void yvex_moe_accumulate(
    const float *expert, unsigned long long count, float weight,
    float *aggregate, int *status)
{
    unsigned long long index = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (!status || *status || index >= count) return;
    if (!expert || !aggregate || !isfinite(weight)) {
        atomicCAS(status, 0, 2);
        return;
    }
    float value = __fadd_rn(aggregate[index], __fmul_rn(expert[index], weight));
    if (!isfinite(value)) atomicCAS(status, 0, YVEX_MOE_DEVICE_WEIGHTED_ACCUMULATION);
    else aggregate[index] = value;
}
