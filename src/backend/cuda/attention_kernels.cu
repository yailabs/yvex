/*
 * Execute exact and native online-softmax attention reduction kernels.
 *
 * The exact kernel preserves forensic arithmetic. The native kernel owns the
 * independently admitted parallel reduction used by production execution.
 */
#include "src/backend/cuda/kernel_primitives.h"
#include "src/backend/cuda/dot_recovery.h"
#include "src/backend/cuda/attention_block.h"
#include <cuda_bf16.h>
#include <mma.h>

/* Causal rows keep the source's local-window slots before compressed slots.
 * Future/absent slots are masked, not compacted across a 64-key boundary.
 * The operation has no noncausal draft-block admission. */
#if __CUDA_ARCH__ >= 800
static __device__ const float *attention_block_row(
    const yvex_cuda_attention_block &p, unsigned token, unsigned column)
{
    if (column >= p.columns) return nullptr;
    unsigned long long position = p.first_position + token;
    if (column < p.local_columns) {
        unsigned long long history = min(position, (unsigned long long)p.window - 1ull);
        unsigned long long before = p.initial_local_count + token;
        if (history > before) { atomicCAS(p.status, 0, 2); return nullptr; }
        if (column > history) return nullptr;
        unsigned long long index = before - history + column;
        if (p.local_positions[index] != position - history + column) {
            atomicCAS(p.status, 0, 2); return nullptr;
        }
        return p.local + index * p.local_stride;
    }
    unsigned long long column_index = column - p.local_columns;
    if (!p.attention_class) return nullptr;
    unsigned long long index = column_index;
    if (p.attention_class == 1u) {
        unsigned long long count = p.selected_count[token];
        if (count > p.selected_stride) { atomicCAS(p.status, 0, 2); return nullptr; }
        if (column_index >= count) return nullptr;
        index = p.selected[(unsigned long long)token * p.selected_stride + column_index];
    }
    if (index >= p.compressed_count) {
        if (p.attention_class == 1u) atomicCAS(p.status, 0, 2);
        return nullptr;
    }
    unsigned long long first = p.compressed_positions[index];
    if (first > position || position - first < p.ratio - 1ull) return nullptr;
    return p.compressed + index * p.compressed_stride;
}
#endif

static __device__ int attention_block_valid(const yvex_cuda_attention_block &p)
{
    return p.query && p.local && p.local_positions && p.sinks && p.output && p.status &&
        p.tokens && p.tokens <= YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH &&
        p.heads && p.heads <= 128u && !(p.heads % 16u) &&
        p.window && p.local_columns && p.local_columns <= p.window &&
        p.columns >= p.local_columns && p.pitch >= p.columns && !(p.pitch % 64u) &&
        p.first_position <= ~0ull - p.tokens && p.initial_local_count <= p.first_position &&
        p.local_stride >= 512ull && p.attention_class <= 2u &&
        ((!p.attention_class && !p.ratio && p.columns == p.local_columns) ||
         (p.attention_class && p.compressed && p.compressed_positions &&
          p.compressed_stride >= 512ull &&
          ((p.attention_class == 1u && p.ratio == 4u && p.selected && p.selected_count &&
            p.selected_stride) || (p.attention_class == 2u && p.ratio == 128u))));
}

/* Convert already-BF16-valued activations once, not once per head/key/MMA
 * tile. Invalid future or unselected KV rows are recorded, not refused until
 * the causal map actually references them. No committed state is modified. */
extern "C" __global__ void yvex_attention_block_prepare(yvex_cuda_attention_block p)
{
#if __CUDA_ARCH__ >= 800
    __shared__ int active;
    if (!p.status) return;
    if (!threadIdx.x) {
        active = atomicAdd(p.status, 0) == 0;
        if (active && (!attention_block_valid(p) || !p.packed_query || !p.packed_kv || !p.invalid_kv ||
            blockDim.x != 128u || p.prepared_rows < p.initial_local_count + p.tokens + p.compressed_count)) {
            atomicCAS(p.status, 0, 2); active = 0;
        }
    }
    __syncthreads(); if (!active) return;
    unsigned lane = threadIdx.x & 31u;
    unsigned long long row = (unsigned long long)blockIdx.x * 4u + (threadIdx.x >> 5u);
    unsigned long long queries = (unsigned long long)p.tokens * p.heads;
    unsigned long long local = p.initial_local_count + p.tokens;
    if (row >= queries + local + p.compressed_count) return;
    const float *source;
    unsigned short *target;
    if (row < queries) {
        source = p.query + row * 512u; target = p.packed_query + row * 512u;
    } else {
        unsigned long long index = row - queries;
        source = index < local ? p.local + index * p.local_stride :
            p.compressed + (index - local) * p.compressed_stride;
        target = p.packed_kv + index * 512u;
    }
    unsigned invalid = 0u;
    for (unsigned i = lane; i < 512u; i += 32u) {
        float value = source[i];
        __nv_bfloat16 packed = __float2bfloat16_rn(value);
        bool valid = isfinite(value) && __bfloat162float(packed) == value;
        invalid |= !valid;
        target[i] = valid ? __bfloat16_as_ushort(packed) : 0u;
    }
    invalid = __any_sync(~0u, invalid);
    if (!lane) {
        if (row < queries) { if (invalid) atomicCAS(p.status, 0, 2); }
        else p.invalid_kv[row - queries] = invalid;
    }
#else
    if (!threadIdx.x && p.status) atomicCAS(p.status, 0, 2);
#endif
}

/* Causal topology is shared across all heads and both matrix products. */
extern "C" __global__ void yvex_attention_block_map(yvex_cuda_attention_block p)
{
#if __CUDA_ARCH__ >= 800
    if (!p.status || !p.indices || atomicAdd(p.status, 0)) return;
    unsigned long long at = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (at >= (unsigned long long)p.tokens * p.pitch) return;
    p.indices[at] = ~0u;
    unsigned token = (unsigned)(at / p.pitch), column = (unsigned)(at % p.pitch);
    const float *source = attention_block_row(p, token, column);
    if (!source) return;
    unsigned long long index = column < p.local_columns ?
        (unsigned long long)(source - p.local) / p.local_stride :
        p.initial_local_count + p.tokens + (unsigned long long)(source - p.compressed) / p.compressed_stride;
    if (index >= p.prepared_rows || p.invalid_kv[index]) {
        atomicCAS(p.status, 0, 2); return;
    }
    p.indices[at] = (unsigned)index;
#else
    if (!threadIdx.x && p.status) atomicCAS(p.status, 0, 2);
#endif
}

/* Independent 16-head, 64-key QK tiles over phase-prepared operands.
 * MMA, reduction ordering and publication remain the online64 class. */
extern "C" __global__ void yvex_attention_block_qk(yvex_cuda_attention_block p)
{
#if __CUDA_ARCH__ >= 800
    using namespace nvcuda;
    __shared__ __nv_bfloat16 q[16 * 48], k[64 * 48];
    __shared__ int active;
    unsigned tid = threadIdx.x, warp = tid / 32u;
    if (!p.status) return;
    if (!tid) {
        active = atomicAdd(p.status, 0) == 0;
        if (active && (!attention_block_valid(p) || !p.scores || blockDim.x != 128u)) {
            atomicCAS(p.status, 0, 2); active = 0;
        }
    }
    __syncthreads(); if (!active) return;
    unsigned blocks = p.pitch / 64u, groups = p.heads / 16u;
    unsigned task = blockIdx.x / blocks, token = task / groups;
    unsigned head = task % groups * 16u, base = blockIdx.x % blocks * 64u;
    if (token >= p.tokens) return;
    wmma::fragment<wmma::accumulator, 16, 16, 16, float> acc;
    wmma::fill_fragment(acc, 0.0f);
    for (unsigned d = 0u; d < 512u; d += 32u) {
        for (unsigned i = tid; i < 16u * 32u; i += 128u) {
            q[i / 32u * 48u + i % 32u] = __ushort_as_bfloat16(p.packed_query[
                ((unsigned long long)token * p.heads + head + i / 32u) * 512u + d + i % 32u]);
        }
        for (unsigned i = tid; i < 64u * 32u; i += 128u) {
            unsigned row = p.indices[(unsigned long long)token * p.pitch + base + i / 32u];
            k[i / 32u * 48u + i % 32u] = __ushort_as_bfloat16(row == ~0u ? 0u :
                p.packed_kv[(unsigned long long)row * 512u + d + i % 32u]);
        }
        __syncthreads();
        for (unsigned part = 0u; part < 32u; part += 16u) {
            wmma::fragment<wmma::matrix_a, 16, 16, 16, __nv_bfloat16, wmma::row_major> a;
            wmma::fragment<wmma::matrix_b, 16, 16, 16, __nv_bfloat16, wmma::col_major> b;
            wmma::load_matrix_sync(a, q + part, 48u);
            wmma::load_matrix_sync(b, k + warp * 16u * 48u + part, 48u);
            wmma::mma_sync(acc, a, b, acc);
        }
        __syncthreads();
    }
    float *scores = p.scores + ((unsigned long long)token * p.heads + head) * p.pitch + base;
    wmma::store_matrix_sync(scores + warp * 16u, acc, p.pitch, wmma::mem_row_major);
    __syncthreads();
    for (unsigned i = tid; i < 16u * 64u; i += 128u) {
        float *value = scores + (unsigned long long)(i / 64u) * p.pitch + i % 64u;
        if (p.indices[(unsigned long long)token * p.pitch + base + i % 64u] == ~0u) *value = -INFINITY;
        else if (!isfinite(*value)) atomicCAS(p.status, 0, 1);
    }
#else
    if (!threadIdx.x && p.status) atomicCAS(p.status, 0, 2);
#endif
}

extern "C" __global__ void yvex_attention_block_softmax(yvex_cuda_attention_block p)
{
    __shared__ int active;
    unsigned lane = threadIdx.x & 31u, row = blockIdx.x * 4u + (threadIdx.x >> 5u);
    if (!p.status) return;
    if (!threadIdx.x) {
        active = atomicAdd(p.status, 0) == 0;
        if (active && (!attention_block_valid(p) || !p.scores || !p.probabilities ||
            !p.scales || !p.denominators || blockDim.x != 128u)) {
            atomicCAS(p.status, 0, 2); active = 0;
        }
    }
    __syncthreads(); if (!active) return;
    if (row >= p.tokens * p.heads) return;
    if (!isfinite(p.sinks[row % p.heads])) { if (!lane) atomicCAS(p.status, 0, 2); return; }
    unsigned blocks = p.pitch / 64u;
    float maximum = -INFINITY, denominator = 0.0f;
    for (unsigned base = 0u; base < p.pitch; base += 64u) {
        /* Adjacent leaves, then 32/16/8/4/2-key butterfly distances. This
         * preserves the H=16 source reduction, not a strided-pair tree. */
        unsigned xindex = base + lane * 2u, yindex = xindex + 1u;
        float x = p.scores[(unsigned long long)row * p.pitch + xindex] * 0.04419417382415922f;
        float y = p.scores[(unsigned long long)row * p.pitch + yindex] * 0.04419417382415922f;
        float m = fmaxf(maximum, fmaxf(x, y));
        for (unsigned s = 16u; s; s >>= 1u) m = fmaxf(m, __shfl_xor_sync(~0u, m, s));
        if (!isfinite(m)) { if (!lane) atomicCAS(p.status, 0, 2); return; }
        float scale = expf(maximum - m), px = expf(x - m), py = expf(y - m), sum = px + py;
        p.probabilities[(unsigned long long)row * p.pitch + xindex] = __bfloat16_as_ushort(__float2bfloat16_rn(px));
        p.probabilities[(unsigned long long)row * p.pitch + yindex] = __bfloat16_as_ushort(__float2bfloat16_rn(py));
        for (unsigned s = 16u; s; s >>= 1u) sum += __shfl_xor_sync(~0u, sum, s);
        denominator = denominator * scale + sum; maximum = m;
        if (!lane) p.scales[(unsigned long long)row * blocks + base / 64u] = scale;
    }
    if (!lane) {
        denominator += expf(p.sinks[row % p.heads] - maximum);
        p.denominators[row] = denominator;
        if (!isfinite(denominator) || denominator <= 0.0f) atomicCAS(p.status, 0, 1);
    }
}

#if __CUDA_ARCH__ >= 800
static __device__ unsigned attention_block_pair(__nv_bfloat16 a, __nv_bfloat16 b)
{
    return (unsigned)__bfloat16_as_ushort(a) | ((unsigned)__bfloat16_as_ushort(b) << 16u);
}
#endif

extern "C" __global__ void yvex_attention_block_pv(yvex_cuda_attention_block p)
{
#if __CUDA_ARCH__ >= 800
    __shared__ __nv_bfloat16 probability[16 * 72], values[64 * 72];
    __shared__ int active;
    unsigned tid = threadIdx.x, warp = tid >> 5u, lane = tid & 31u, group = lane >> 2u, thread = lane & 3u;
    if (!p.status) return;
    if (!tid) {
        active = atomicAdd(p.status, 0) == 0;
        if (active && (!attention_block_valid(p) || !p.probabilities || !p.scales ||
            !p.denominators || blockDim.x != 128u)) {
            atomicCAS(p.status, 0, 2); active = 0;
        }
    }
    __syncthreads(); if (!active) return;
    unsigned task = blockIdx.x / 8u, token = task / (p.heads / 16u);
    unsigned head = task % (p.heads / 16u) * 16u, col = blockIdx.x % 8u * 64u, blocks = p.pitch / 64u;
    if (token >= p.tokens) return;
    float accum[2][4] = {};
    for (unsigned base = 0u; base < p.pitch; base += 64u) {
        for (unsigned i = tid; i < 16u * 64u; i += 128u)
            probability[i / 64u * 72u + i % 64u] = __ushort_as_bfloat16(
                p.probabilities[((unsigned long long)token * p.heads + head + i / 64u) * p.pitch + base + i % 64u]);
        for (unsigned i = tid; i < 64u * 64u; i += 128u) {
            unsigned row = p.indices[(unsigned long long)token * p.pitch + base + i / 64u];
            values[i / 64u * 72u + i % 64u] = __ushort_as_bfloat16(row == ~0u ? 0u :
                p.packed_kv[(unsigned long long)row * 512u + col + i % 64u]);
        }
        __syncthreads();
#pragma unroll
        for (unsigned tile = 0u; tile < 2u; ++tile) for (unsigned j = 0u; j < 4u; ++j) {
            unsigned long long row = (unsigned long long)token * p.heads + head + group + (j >> 1u) * 8u;
            accum[tile][j] *= p.scales[row * blocks + base / 64u];
        }
        for (unsigned k = 0u; k < 64u; k += 16u) {
            unsigned a[4];
            for (unsigned j = 0u; j < 4u; ++j) {
                const __nv_bfloat16 *v = probability + (group + (j & 1u) * 8u) * 72u + k + thread * 2u + (j >> 1u) * 8u;
                a[j] = attention_block_pair(v[0], v[1]);
            }
#pragma unroll
            for (unsigned tile = 0u; tile < 2u; ++tile) {
                unsigned column = warp * 16u + tile * 8u + group, b[2];
                for (unsigned j = 0u; j < 2u; ++j) b[j] = attention_block_pair(
                    values[(k + thread * 2u + j * 8u) * 72u + column],
                    values[(k + thread * 2u + j * 8u + 1u) * 72u + column]);
                float *c = accum[tile];
                asm volatile("mma.sync.aligned.m16n8k16.row.col.f32.bf16.bf16.f32 "
                    "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
                    : "+f"(c[0]), "+f"(c[1]), "+f"(c[2]), "+f"(c[3])
                    : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]), "r"(b[0]), "r"(b[1]));
            }
        }
        __syncthreads();
    }
#pragma unroll
    for (unsigned tile = 0u; tile < 2u; ++tile) for (unsigned j = 0u; j < 4u; ++j) {
        unsigned h = group + (j >> 1u) * 8u, column = warp * 16u + tile * 8u + thread * 2u + (j & 1u);
        float x = accum[tile][j] / p.denominators[(unsigned long long)token * p.heads + head + h];
        if (!isfinite(x)) atomicCAS(p.status, 0, 1);
        else p.output[((unsigned long long)token * p.heads + head + h) * 512u + col + column] =
            __bfloat162float(__float2bfloat16_rn(x));
    }
#else
    if (!threadIdx.x && p.status) atomicCAS(p.status, 0, 2);
#endif
}

/* Fuse the admitted online64 recurrence without global score/probability
 * materialization. K16 MMA order, adjacent-key butterfly, BF16 probabilities
 * and sink publication are identical to the staged consumer above. Each CTA
 * owns sixteen heads and all 512 value coordinates; causal mapping is shared. */
extern "C" __global__ __launch_bounds__(512, 1) void yvex_attention_block_fused(
    yvex_cuda_attention_block p)
{
#if __CUDA_ARCH__ >= 800
    using namespace nvcuda;
    extern __shared__ __align__(32) unsigned char memory[];
    __nv_bfloat16 *q = reinterpret_cast<__nv_bfloat16 *>(memory);
    __nv_bfloat16 *kv = q + 16u * 528u;
    __nv_bfloat16 *probability = kv + 64u * 528u;
    float *score = reinterpret_cast<float *>(probability + 16u * 72u);
    float *scales = score + 16u * 72u, *denominators = scales + 16u;
    __shared__ int active;
    unsigned tid = threadIdx.x, warp = tid >> 5u, lane = tid & 31u;
    unsigned group = lane >> 2u, thread = lane & 3u;
    if (!p.status) return;
    if (!tid) {
        active = atomicAdd(p.status, 0) == 0;
        if (active && (!attention_block_valid(p) || blockDim.x != 512u ||
            !p.packed_query || !p.packed_kv || !p.indices || !p.invalid_kv)) {
            atomicCAS(p.status, 0, 2); active = 0;
        }
    }
    __syncthreads();
    if (!active) return;
    unsigned token = blockIdx.x / (p.heads / 16u);
    unsigned head = blockIdx.x % (p.heads / 16u) * 16u;
    if (token >= p.tokens) return;
    if (!lane && !isfinite(p.sinks[head + warp])) atomicCAS(p.status, 0, 2);
    for (unsigned i = tid; i < 16u * 512u; i += 512u)
        q[i / 512u * 528u + i % 512u] = __ushort_as_bfloat16(p.packed_query[
            ((unsigned long long)token * p.heads + head + i / 512u) * 512u + i % 512u]);
    __syncthreads();
    if (!tid) active = atomicAdd(p.status, 0) == 0;
    __syncthreads(); if (!active) return;
    float maximum = -INFINITY, denominator = 0.0f;
    float accum[4][4] = {};
    for (unsigned base = 0u; base < p.pitch; base += 64u) {
        for (unsigned i = tid; i < 64u * 512u; i += 512u) {
            unsigned row = p.indices[(unsigned long long)token * p.pitch + base + i / 512u];
            kv[i / 512u * 528u + i % 512u] = __ushort_as_bfloat16(row == ~0u ? 0u :
                p.packed_kv[(unsigned long long)row * 512u + i % 512u]);
        }
        __syncthreads();
        if (warp < 4u) {
            wmma::fragment<wmma::accumulator, 16, 16, 16, float> dot;
            wmma::fill_fragment(dot, 0.0f);
            for (unsigned d = 0u; d < 512u; d += 16u) {
                wmma::fragment<wmma::matrix_a, 16, 16, 16, __nv_bfloat16, wmma::row_major> a;
                wmma::fragment<wmma::matrix_b, 16, 16, 16, __nv_bfloat16, wmma::col_major> b;
                wmma::load_matrix_sync(a, q + d, 528u);
                wmma::load_matrix_sync(b, kv + warp * 16u * 528u + d, 528u);
                wmma::mma_sync(dot, a, b, dot);
            }
            wmma::store_matrix_sync(score + warp * 16u, dot, 72u, wmma::mem_row_major);
        }
        __syncthreads();
        unsigned xindex = lane * 2u, yindex = xindex + 1u;
        float sx = score[warp * 72u + xindex], sy = score[warp * 72u + yindex];
        if (p.indices[(unsigned long long)token * p.pitch + base + xindex] == ~0u) sx = -INFINITY;
        else if (!isfinite(sx)) atomicCAS(p.status, 0, 1);
        if (p.indices[(unsigned long long)token * p.pitch + base + yindex] == ~0u) sy = -INFINITY;
        else if (!isfinite(sy)) atomicCAS(p.status, 0, 1);
        sx *= 0.04419417382415922f; sy *= 0.04419417382415922f;
        float m = fmaxf(maximum, fmaxf(sx, sy));
        for (unsigned s = 16u; s; s >>= 1u) m = fmaxf(m, __shfl_xor_sync(~0u, m, s));
        if (!isfinite(m)) atomicCAS(p.status, 0, 2);
        float scale = expf(maximum - m), px = expf(sx - m), py = expf(sy - m);
        float total = px + py;
        probability[warp * 72u + xindex] = __float2bfloat16_rn(px);
        probability[warp * 72u + yindex] = __float2bfloat16_rn(py);
        for (unsigned s = 16u; s; s >>= 1u) total += __shfl_xor_sync(~0u, total, s);
        denominator = denominator * scale + total; maximum = m;
        if (!lane) scales[warp] = scale;
        __syncthreads();
        if (!tid) active = atomicAdd(p.status, 0) == 0;
        __syncthreads(); if (!active) return;
#pragma unroll
        for (unsigned tile = 0u; tile < 4u; ++tile)
#pragma unroll
            for (unsigned j = 0u; j < 4u; ++j)
                accum[tile][j] *= scales[group + (j >> 1u) * 8u];
        for (unsigned k = 0u; k < 64u; k += 16u) {
            unsigned a[4];
#pragma unroll
            for (unsigned j = 0u; j < 4u; ++j) {
                const __nv_bfloat16 *v = probability + (group + (j & 1u) * 8u) * 72u + k +
                    thread * 2u + (j >> 1u) * 8u;
                a[j] = attention_block_pair(v[0], v[1]);
            }
#pragma unroll
            for (unsigned tile = 0u; tile < 4u; ++tile) {
                unsigned column = warp * 32u + tile * 8u + group, b[2];
#pragma unroll
                for (unsigned j = 0u; j < 2u; ++j) b[j] = attention_block_pair(
                    kv[(k + thread * 2u + j * 8u) * 528u + column],
                    kv[(k + thread * 2u + j * 8u + 1u) * 528u + column]);
                float *c = accum[tile];
                asm volatile("mma.sync.aligned.m16n8k16.row.col.f32.bf16.bf16.f32 "
                    "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
                    : "+f"(c[0]), "+f"(c[1]), "+f"(c[2]), "+f"(c[3])
                    : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]), "r"(b[0]), "r"(b[1]));
            }
        }
        __syncthreads();
    }
    if (!lane) {
        denominator += expf(p.sinks[head + warp] - maximum);
        denominators[warp] = denominator;
        if (!isfinite(denominator) || denominator <= 0.0f) atomicCAS(p.status, 0, 1);
    }
    __syncthreads();
    if (!tid) active = atomicAdd(p.status, 0) == 0;
    __syncthreads(); if (!active) return;
#pragma unroll
    for (unsigned tile = 0u; tile < 4u; ++tile)
#pragma unroll
        for (unsigned j = 0u; j < 4u; ++j) {
            unsigned h = group + (j >> 1u) * 8u;
            unsigned column = warp * 32u + tile * 8u + thread * 2u + (j & 1u);
            float value = accum[tile][j] / denominators[h];
            if (!isfinite(value)) atomicCAS(p.status, 0, 1);
            else p.output[((unsigned long long)token * p.heads + head + h) * 512u + column] =
                __bfloat162float(__float2bfloat16_rn(value));
        }
#else
    if (!threadIdx.x && p.status) atomicCAS(p.status, 0, 2);
#endif
}

/* Exact BF16 lattice matrix for candidate scoring. Geometry and arithmetic
 * guards retain the ordered-F64 consumer for unsupported values. Phase-local
 * digit preparation is lossless and never changes persistent state. */

struct index_lattice { int minimum, maximum, valid; };
static __device__ index_lattice index_envelope(const float *v, unsigned width)
{
    unsigned lane = threadIdx.x & 3u, mask = 15u << (threadIdx.x & 28u);
    index_lattice r = {255, 0, 1};
    if (v) for (unsigned i = lane; i < width; i += 4u) {
        unsigned bits = __float_as_uint(v[i]), magnitude = bits & 0x7fffffffu;
        int exponent = (bits >> 23u) & 255u;
        if ((bits & 65535u) || (magnitude && (exponent < 64 || exponent > 190))) r.valid = 0;
        if (magnitude) {
            r.minimum = min(r.minimum, exponent);
            r.maximum = max(r.maximum, exponent);
        }
    }
    for (unsigned offset = 2u; offset; offset >>= 1u) {
        r.minimum = min(r.minimum, __shfl_down_sync(mask, r.minimum, offset, 4));
        r.maximum = max(r.maximum, __shfl_down_sync(mask, r.maximum, offset, 4));
        r.valid &= __shfl_down_sync(mask, r.valid, offset, 4);
    }
    r.minimum = __shfl_sync(mask, r.minimum, 0, 4);
    r.maximum = __shfl_sync(mask, r.maximum, 0, 4);
    r.valid = __shfl_sync(mask, r.valid, 0, 4);
    if (r.minimum > r.maximum) r.minimum = r.maximum = 127;
    r.valid &= (r.maximum - r.minimum) <= 14;
    return r;
}
static __device__ void index_digits(const float *v, unsigned first, unsigned width,
                                    int minimum, int out[3])
{
    out[0] = out[1] = out[2] = 0;
#pragma unroll
    for (unsigned j = 0; j < 4; j++) {
        unsigned bits = v && first + j < width ? __float_as_uint(v[first + j]) : 0;
        int value = 0;
        if (bits & 0x7fffffffu) {
            value = (int)((128u + ((bits >> 16u) & 127u)) << (((bits >> 23u) & 255u) - minimum));
            if (bits & 0x80000000u) value = -value;
        }
#pragma unroll
        for (unsigned digit = 0; digit < 3; digit++) {
            int part = (int)(signed char)(value & 255);
            out[digit] |= (int)((unsigned)(part & 255) << (j * 8));
            value = (value - part) / 256;
        }
    }
}
static __device__ void index_mma(int c[4], const int a[4], const int b[2])
{
    asm volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(c[0]), "+r"(c[1]), "+r"(c[2]), "+r"(c[3])
        : "r"(a[0]), "r"(a[1]), "r"(a[2]), "r"(a[3]), "r"(b[0]), "r"(b[1]));
}

/* Four cooperating lanes own one actual vector. Invalid operands retain
 * metadata, not a rounded substitute; only their visible consumer refuses. */
extern "C" __global__ void yvex_attention_index_prepare(
    const float *vectors, unsigned long long rows, unsigned long long stride,
    unsigned int width, int *packed, int *status)
{
    if (!status || atomicAdd(status, 0)) return;
    if (!rows) return;
    if (!vectors || !packed || !width || width > 256u || width % 32u ||
        stride < width || !blockDim.x || blockDim.x % 32u ||
        rows > ~0ull / stride || rows > ~0ull / (4u + width / 4u * 3u)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned lane = threadIdx.x & 3u;
    unsigned long long row =
        ((unsigned long long)blockIdx.x * blockDim.x + threadIdx.x) / 4u;
    if (row >= rows) return;
    const float *v = vectors + row * stride;
    index_lattice envelope = index_envelope(v, width);
    int *out = packed + row * (4u + width / 4u * 3u);
    if (!lane) {
        out[0] = envelope.minimum; out[1] = envelope.maximum;
        out[2] = envelope.valid; out[3] = 0;
    }
    for (unsigned first = lane * 4u; first < width; first += 16u) {
        int digits[3] = {};
        if (envelope.valid) index_digits(v, first, width, envelope.minimum, digits);
#pragma unroll
        for (unsigned d = 0u; d < 3u; ++d) out[4u + first / 4u * 3u + d] = digits[d];
    }
}

template <bool Prepared>
static __device__ void index_candidate_scores_matrix(
    const float *query,const float *weights,
    const float *history,const unsigned long long *history_positions,
    unsigned long long history_count,unsigned long long history_stride,
    const float *current,const unsigned long long *current_positions,
    unsigned long long current_count,unsigned long long current_stride,
    unsigned long long heads,unsigned long long width,
    unsigned long long ratio,unsigned long long position,
    float *scores,int *status,unsigned long long rows,unsigned long long candidate_stride,
    const int *packed_queries, const int *packed_keys)
{
    extern __shared__ double contributions[];
    __shared__ int active;
    unsigned long long groups=candidate_stride/8ull+(candidate_stride%8ull!=0ull);
    unsigned long long ordinal=groups?blockIdx.x/groups:rows;
    unsigned long long first=groups?(blockIdx.x%groups)*8ull:0ull;
    unsigned lane=threadIdx.x&31,warp=threadIdx.x>>5,group=lane>>2,t=lane&3;
    if(!status)return;
    if(candidate_stride&&rows&&ordinal>=rows)return;
    if(!query||!weights||!scores||!heads||heads>128||heads%16||!width||width>256||width%32||
       blockDim.x!=heads*2||!ratio||!rows||!candidate_stride||
       (Prepared&&(!packed_queries||!packed_keys))||
       rows>~0ull/heads/width||rows>~0ull/candidate_stride||
       position>~0ull-ordinal||history_count>~0ull-current_count||
       candidate_stride<history_count+current_count||
       (history_count&&(!history||!history_positions||history_stride<width||history_count>~0ull/history_stride))||
       (current_count&&(!current||!current_positions||current_stride<width||current_count>~0ull/current_stride))){
        if(!threadIdx.x)atomicCAS(status,0,2);return;
    }
    if(!threadIdx.x)active=atomicAdd(status,0)==0;
    __syncthreads();
    if(!active)return;
    if(first>=history_count+current_count)return;
    query+=ordinal*heads*width;weights+=ordinal*heads;scores+=ordinal*candidate_stride;
    unsigned long long candidate=first+group,total=history_count+current_count;
    unsigned long long now=position+ordinal;
    unsigned long long key_position=candidate<total?(candidate<history_count?
        history_positions[candidate]:current_positions[candidate-history_count]):~0ull;
    bool visible=candidate<total && key_position<=now && key_position<=~0ull-ratio+1ull &&
        key_position+ratio-1ull<=now;
    const float *key=visible?(candidate<history_count?history+candidate*history_stride:
        current+(candidate-history_count)*current_stride):nullptr;
    const float *queries[2]={query+(warp*16+group)*width,query+(warp*16+group+8)*width};
    index_lattice q[2], k;
    const int *pq[2] = {}, *pk = nullptr;
    if constexpr (Prepared) {
        unsigned packed_stride = 4u + (unsigned)width / 4u * 3u;
        for (unsigned j = 0; j < 2u; ++j) {
            pq[j] = packed_queries + (ordinal * heads + warp * 16u + group + j * 8u) * packed_stride;
            q[j] = {pq[j][0], pq[j][1], pq[j][2]};
        }
        pk = packed_keys + (candidate < total ? candidate : 0ull) * packed_stride;
        k = visible ? index_lattice{pk[0], pk[1], pk[2]} : index_lattice{127,127,1};
    } else {
        q[0] = index_envelope(queries[0], (unsigned)width);
        q[1] = index_envelope(queries[1], (unsigned)width);
        k = index_envelope(key, (unsigned)width);
    }
    bool exact=__all_sync(0xffffffffu,q[0].valid&&q[1].valid&&k.valid);
    long long sums[4]={};
    if(exact){
        /* Three balanced signed-byte digits per vector: maximum span 14,
         * so absolute source products sum to <2^(16+28+8)=2^52.
         * Every ordered F64 partial is exact. I64 reconstruction also fits. */
        for(unsigned segment=0;segment<width;segment+=32){
            int a[3][4],b[3][2];
#pragma unroll
            for(unsigned j=0;j<4;j++){
                int digits[3];
                if constexpr (Prepared) {
#pragma unroll
                    for (unsigned d=0;d<3;d++) digits[d]=pq[j&1][4u+(segment/4u+t+(j>>1)*4u)*3u+d];
                } else index_digits(queries[j&1],segment+t*4+(j>>1)*16,(unsigned)width,q[j&1].minimum,digits);
#pragma unroll
                for(unsigned d=0;d<3;d++)a[d][j]=digits[d];
            }
#pragma unroll
            for(unsigned j=0;j<2;j++){
                int digits[3];
                if constexpr (Prepared) {
#pragma unroll
                    for (unsigned d=0;d<3;d++) digits[d]=visible?pk[4u+(segment/4u+t+j*4u)*3u+d]:0;
                } else index_digits(key,segment+t*4+j*16,(unsigned)width,k.minimum,digits);
#pragma unroll
                for(unsigned d=0;d<3;d++)b[d][j]=digits[d];
            }
#pragma unroll
            for(unsigned ad=0;ad<3;ad++){
#pragma unroll
                for(unsigned bd=0;bd<3;bd++){
                    int result[4]={};index_mma(result,a[ad],b[bd]);
#pragma unroll
                    for(unsigned j=0;j<4;j++)sums[j]+=(long long)result[j]*(1ll<<(8*(ad+bd)));
                }
            }
        }
    }
#pragma unroll
    for(unsigned j=0;j<4;j++){
        unsigned head=warp*16+group+(j>>1)*8,column=t*2+(j&1);
        unsigned long long c=first+column;
        double dot=0;
        int key_minimum=__shfl_sync(0xffffffffu,k.minimum,column*4);
        int admitted=__shfl_sync(0xffffffffu,(int)visible,column*4);
        if(admitted){
            if(exact){
                dot=ldexp((double)sums[j],q[j>>1].minimum+key_minimum-268);
            }else{
                const float *v=c<history_count?history+c*history_stride:current+(c-history_count)*current_stride;
                for(unsigned i=0;i<width;i++)
                    dot=__dadd_rn(dot,__dmul_rn((double)query[head*width+i],(double)v[i]));
            }
        }
        if(dot<0)dot=0;
        contributions[head*8+column]=__dmul_rn(dot,(double)weights[head]);
    }
    __syncthreads();
    if(threadIdx.x<8){
        unsigned long long c=first+threadIdx.x;
        if(c>=total)return;
        unsigned long long p=c<history_count?history_positions[c]:current_positions[c-history_count];
        if(p>now||p>~0ull-ratio+1ull||p+ratio-1ull>now)return;
        double score=0;
        for(unsigned head=0;head<heads;head++)score=__dadd_rn(score,contributions[head*8+threadIdx.x]);
        score=__dmul_rn(score,1.0/sqrt((double)width));
        score=__dmul_rn(score,1.0/sqrt((double)heads));
        if(!isfinite(score)||!isfinite((float)score))atomicCAS(status,0,1);
        else scores[c]=(float)score;
    }
}


extern "C" __global__ void yvex_attention_candidate_scores_matrix(
    const float *query, const float *weights,
    const float *history, const unsigned long long *history_positions,
    unsigned long long history_count, unsigned long long history_stride,
    const float *current, const unsigned long long *current_positions,
    unsigned long long current_count, unsigned long long current_stride,
    unsigned long long heads, unsigned long long width,
    unsigned long long ratio, unsigned long long position,
    float *scores, int *status, unsigned long long rows, unsigned long long candidate_stride)
{
    index_candidate_scores_matrix<false>(query, weights, history, history_positions,
        history_count, history_stride, current, current_positions, current_count,
        current_stride, heads, width, ratio, position, scores, status, rows,
        candidate_stride, nullptr, nullptr);
}

extern "C" __global__ void yvex_attention_candidate_scores_prepared(
    const float *query, const float *weights,
    const float *history, const unsigned long long *history_positions,
    unsigned long long history_count, unsigned long long history_stride,
    const float *current, const unsigned long long *current_positions,
    unsigned long long current_count, unsigned long long current_stride,
    unsigned long long heads, unsigned long long width,
    unsigned long long ratio, unsigned long long position,
    float *scores, int *status, unsigned long long rows, unsigned long long candidate_stride,
    const int *packed_queries, const int *packed_keys)
{
    index_candidate_scores_matrix<true>(query, weights, history, history_positions,
        history_count, history_stride, current, current_positions, current_count,
        current_stride, heads, width, ratio, position, scores, status, rows,
        candidate_stride, packed_queries, packed_keys);
}

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

static __device__ double attention_yarn_frequency(
    unsigned long long pair, unsigned long long rope_dims,
    unsigned long long theta, unsigned long long scaling_factor,
    unsigned long long original_context, unsigned long long beta_fast,
    unsigned long long beta_slow)
{
    const double pi = 3.14159265358979323846264338327950288;
    double exponent = (double)(pair * 2ull) / (double)rope_dims;
    double frequency = 1.0 / pow((double)theta, exponent);
    if (original_context && scaling_factor) {
        double denominator = 2.0 * log((double)theta);
        double low = floor((double)rope_dims *
            log((double)original_context /
                ((double)beta_fast * 2.0 * pi)) / denominator);
        double high = ceil((double)rope_dims *
            log((double)original_context /
                ((double)beta_slow * 2.0 * pi)) / denominator);
        double lane = (double)pair;
        if (low < 0.0) low = 0.0;
        if (high > (double)rope_dims - 1.0)
            high = (double)rope_dims - 1.0;
        if (low == high) high += 0.001;
        double ramp = (lane - low) / (high - low);
        if (ramp < 0.0) ramp = 0.0;
        if (ramp > 1.0) ramp = 1.0;
        double smooth = 1.0 - ramp;
        frequency = frequency / (double)scaling_factor * (1.0 - smooth) +
                    frequency * smooth;
    }
    return frequency;
}

template<unsigned Pairs>
static __device__ void attention_yarn_rope_cohort(
    float *values, unsigned long long vectors_per_token,
    unsigned long long token_count, unsigned long long vector_width,
    unsigned long long rope_dims, unsigned long long token_position,
    unsigned long long position_step, unsigned long long theta,
    unsigned long long scaling_factor, unsigned long long original_context,
    unsigned long long beta_fast, unsigned long long beta_slow,
    int inverse, int *status)
{
    if (!status) return;
    if (!values || !vectors_per_token || !token_count || !position_step ||
        token_count - 1ull > (~0ull - token_position) / position_step ||
        rope_dims != Pairs * 2ull || rope_dims > vector_width || theta <= 1ull ||
        !scaling_factor || (original_context && (!beta_slow || beta_fast <= beta_slow)) ||
        token_count > ~0ull / vectors_per_token ||
        token_count * vectors_per_token > ~0ull / (rope_dims / 2ull) || blockDim.x != 256u) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    __shared__ double cosines[64], sines[64];
    __shared__ int entry;
    if (!threadIdx.x) entry = *status;
    __syncthreads();
    if (entry) return;
    constexpr unsigned pairs = Pairs;
    unsigned cohort_width = 256u / pairs;
    unsigned long long cohorts = vectors_per_token / cohort_width +
        (vectors_per_token % cohort_width != 0ull);
    unsigned long long token = blockIdx.x / cohorts;
    unsigned long long head_first = (blockIdx.x % cohorts) * cohort_width;
    if (token >= token_count) return;
    if (threadIdx.x < pairs) {
        double frequency = attention_yarn_frequency(threadIdx.x, rope_dims, theta,
            scaling_factor, original_context, beta_fast, beta_slow);
        double angle = (double)(token_position + token * position_step) * frequency;
        cosines[threadIdx.x] = cos(angle);
        sines[threadIdx.x] = inverse ? -sin(angle) : sin(angle);
    }
    __syncthreads();
    unsigned pair = threadIdx.x;
    unsigned long long head = head_first + pair / pairs;
    if (head >= vectors_per_token) return;
    unsigned long long offset = (token * vectors_per_token + head) * vector_width +
        vector_width - rope_dims + (pair % pairs) * 2ull;
    double c = cosines[pair % pairs], s = sines[pair % pairs];
    double x = (double)values[offset], y = (double)values[offset + 1ull];
    double left = x * c - y * s, right = x * s + y * c;
    float published_left = (float)left, published_right = (float)right;
    if (!isfinite(left) || !isfinite(right) || !isfinite(published_left) || !isfinite(published_right))
        atomicCAS(status, 0, 1);
    else {
        values[offset] = float_to_bf16_rne(published_left);
        values[offset + 1ull] = float_to_bf16_rne(published_right);
    }
}

extern "C" __global__ void yvex_attention_yarn_rope(
    float *values, unsigned long long vectors_per_token,
    unsigned long long token_count,
    unsigned long long vector_width, unsigned long long rope_dims,
    unsigned long long token_position, unsigned long long position_step,
    unsigned long long theta,
    unsigned long long scaling_factor, unsigned long long original_context,
    unsigned long long beta_fast, unsigned long long beta_slow,
    int inverse, int *status)
{
    /* Actual heads sharing one source position/coordinate reuse its F64 angle.
     * 64/128 rotary components admit eight/four heads per CTA. Tail lanes do
     * not manufacture heads; the launch owner rounds separately per token. */
    if (rope_dims == 64ull && vectors_per_token >= 8ull && blockDim.x == 256u) {
        attention_yarn_rope_cohort<32u>(values, vectors_per_token, token_count,
            vector_width, rope_dims, token_position, position_step, theta,
            scaling_factor, original_context, beta_fast, beta_slow, inverse, status);
        return;
    }
    if (rope_dims == 128ull && vectors_per_token >= 4ull && blockDim.x == 256u) {
        attention_yarn_rope_cohort<64u>(values, vectors_per_token, token_count,
            vector_width, rope_dims, token_position, position_step, theta,
            scaling_factor, original_context, beta_fast, beta_slow, inverse, status);
        return;
    }
    unsigned long long pair =
        (unsigned long long)blockIdx.x * (unsigned long long)blockDim.x +
        (unsigned long long)threadIdx.x;
    unsigned long long pairs_per_vector;
    unsigned long long total;
    if (!status) return;
    if (*status != 0) return;
    if (!values || !vectors_per_token || !token_count || !position_step ||
        token_count - 1ull > (~0ull - token_position) / position_step || !rope_dims ||
        rope_dims > vector_width ||
        (rope_dims & 1ull) || theta <= 1ull || !scaling_factor ||
        (original_context && (!beta_slow || beta_fast <= beta_slow))) {
        atomicCAS(status, 0, 2);
        return;
    }
    pairs_per_vector = rope_dims / 2ull;
    if (token_count > ~0ull / vectors_per_token ||
        token_count * vectors_per_token > ~0ull / pairs_per_vector) {
        atomicCAS(status, 0, 2);
        return;
    }
    total = token_count * vectors_per_token * pairs_per_vector;
    if (pair >= total) return;
    unsigned long long vector_index = pair / pairs_per_vector;
    unsigned long long token_index = vector_index / vectors_per_token;
    unsigned long long local_pair = pair % pairs_per_vector;
    unsigned long long start = vector_width - rope_dims;
    unsigned long long offset = vector_index * vector_width + start +
                                local_pair * 2ull;
    double frequency = attention_yarn_frequency(
        local_pair, rope_dims, theta, scaling_factor, original_context,
        beta_fast, beta_slow);
    double angle = (double)(token_position + token_index * position_step) * frequency;
    double c = cos(angle);
    double s = inverse ? -sin(angle) : sin(angle);
    double x = (double)values[offset];
    double y = (double)values[offset + 1ull];
    double left = x * c - y * s;
    double right = x * s + y * c;
    float published_left = (float)left;
    float published_right = (float)right;
    if (!isfinite(left) || !isfinite(right) || !isfinite(published_left) ||
        !isfinite(published_right)) atomicCAS(status, 0, 1);
    else {
        values[offset] = float_to_bf16_rne(published_left);
        values[offset + 1ull] = float_to_bf16_rne(published_right);
    }
}

/* Small execution populations: eight independent candidate scores per CTA,
 * followed by the original ordered softmax/value recurrence. The lane permutation
 * preserves the native 256-lane dot tree, including its zero additions; no score,
 * state or value reduction is approximated. Wide phases use the warp-head path. */
extern "C" __global__ void yvex_attention_reduce_native_candidates(
    const float *query, const float *local, const unsigned long long *local_positions,
    unsigned long long initial_local_count, unsigned long long local_stride,
    const float *compressed, const unsigned long long *compressed_positions,
    unsigned long long compressed_stride, const unsigned long long *selected,
    const unsigned long long *selected_count_ptr, unsigned long long topk_capacity,
    const float *sinks, unsigned long long query_heads, unsigned long long supplied_head_dim,
    unsigned long long sliding_window, unsigned long long ratio, unsigned int attention_class,
    unsigned long long phase_start_position, unsigned long long token_count,
    int candidate_block_visible, float *out, int *status)
{
    constexpr unsigned width = 512;
    unsigned thread = threadIdx.x, lane = thread & 31u, warp = thread >> 5u;
    unsigned long long ordinal = query_heads ? blockIdx.x / query_heads : token_count;
    unsigned long long head = query_heads ? blockIdx.x % query_heads : query_heads;
    if (!status || ordinal >= token_count || head >= query_heads) return;
    if (!query || !local || !local_positions || !sinks || !out || !query_heads ||
        supplied_head_dim != width || !sliding_window || !token_count || blockDim.x != 256u ||
        attention_class > 2u || (candidate_block_visible != 0 && candidate_block_visible != 1) ||
        phase_start_position > ~0ull - ordinal || local_stride < width ||
        (attention_class != 0u && (!compressed || !compressed_positions || compressed_stride < width)) ||
        (attention_class == 1u && (!selected || !selected_count_ptr || !topk_capacity)) ||
        (attention_class == 1u && ratio != 4ull) || (attention_class == 2u && ratio != 128ull) ||
        (attention_class == 0u && ratio != 0ull)) {
        if (!thread) atomicCAS(status, 0, 2);
        return;
    }
    __shared__ const float *tile_rows[8];
    __shared__ float scores[8], probabilities[8], alphas[8], divisor;
    __shared__ int failed;
    if (!thread) failed = *status;
    __syncthreads();
    if (failed) return;
    unsigned long long position = phase_start_position + ordinal;
    unsigned long long history = position < sliding_window - 1ull ? position : sliding_window - 1ull;
    unsigned long long local_offset = candidate_block_visible ? 0ull : initial_local_count + ordinal - history;
    unsigned long long count = candidate_block_visible ? initial_local_count + token_count : history + 1ull;
    unsigned long long compressed_count = attention_class == 0u ? 0ull : attention_class == 1u ?
        selected_count_ptr[ordinal] : position / ratio + ((position + 1ull) % ratio == 0ull);
    attention_reduce_rows rows = {local, local_positions, compressed, compressed_positions, selected,
        initial_local_count, local_stride, compressed_stride, topk_capacity, sliding_window, ratio,
        phase_start_position, token_count, attention_class, candidate_block_visible};
    const float *q = query + (ordinal * query_heads + head) * width;
    float *output = out + (ordinal * query_heads + head) * width;
    unsigned base = ((lane & 16u) >> 3u) + ((lane & 8u) >> 3u) + ((lane & 7u) << 5u);
    float cached_query[16], values[2] = {};
#pragma unroll
    for (unsigned part = 0; part < 2; ++part)
#pragma unroll
        for (unsigned j = 0; j < 8; ++j) cached_query[part * 8 + j] = q[base + j * 4 + part * 256];
    float maximum = sinks[head], denominator = 1.0f;
    for (unsigned pass = 0; pass < 2; ++pass) {
        unsigned long long population = pass ? compressed_count : count;
        for (unsigned long long first = 0; first < population; first += 8) {
            unsigned long long candidate = first + warp;
            int visible = 0;
            const float *row = candidate < population ?
                attention_reduce_row(&rows, pass, ordinal, candidate, local_offset, &visible) : NULL;
            if (!lane) { tile_rows[warp] = visible ? row : NULL; scores[warp] = 0.0f; }
            if (visible) {
                float dots[8] = {};
#pragma unroll
                for (unsigned part = 0; part < 2; ++part)
#pragma unroll
                    for (unsigned j = 0; j < 8; ++j)
                        dots[j] = fmaf(cached_query[part * 8 + j], row[base + j * 4 + part * 256], dots[j]);
#pragma unroll
                for (unsigned j = 0; j < 4; ++j) dots[j] = __fadd_rn(dots[j], dots[j + 4]);
#pragma unroll
                for (unsigned j = 0; j < 2; ++j) dots[j] = __fadd_rn(dots[j], dots[j + 2]);
                float dot = __fadd_rn(dots[0], dots[1]);
                dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 16));
                dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 8));
                dot = __fadd_rn(__fadd_rn(dot, 0.0f), 0.0f);
                dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 4));
                dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 2));
                dot = __fadd_rn(dot, __shfl_down_sync(0xffffffffu, dot, 1));
                if (!lane) scores[warp] = dot * rsqrtf((float)supplied_head_dim);
            }
            __syncthreads();
            if (!thread) {
#pragma unroll
                for (unsigned i = 0; i < 8; ++i) {
                    probabilities[i] = 0.0f;
                    alphas[i] = 1.0f;
                    if (!tile_rows[i]) continue;
                    float score = scores[i];
                    if (!isfinite(score)) { atomicCAS(status, 0, 1); failed = 1; }
                    else if (score > maximum) {
                        alphas[i] = expf(maximum - score); maximum = score; probabilities[i] = 1.0f;
                        denominator = denominator * alphas[i] + probabilities[i];
                    } else { probabilities[i] = expf(score - maximum); denominator += probabilities[i]; }
                }
            }
            __syncthreads();
            if (failed) return;
#pragma unroll
            for (unsigned i = 0; i < 8; ++i) {
                const float *value_row = tile_rows[i];
                if (value_row)
#pragma unroll
                    for (unsigned part = 0; part < 2; ++part)
                        values[part] = fmaf(probabilities[i], value_row[thread + part * 256], values[part] * alphas[i]);
            }
            __syncthreads();
        }
    }
    if (!thread) { divisor = denominator; if (!isfinite(denominator) || denominator <= 0.0f) failed = 1; }
    __syncthreads();
    if (failed) { if (!thread) atomicCAS(status, 0, 1); return; }
#pragma unroll
    for (unsigned part = 0; part < 2; ++part) {
        float value = values[part] / divisor;
        if (!isfinite(value)) atomicCAS(status, 0, 1);
        else output[thread + part * 256] = float_to_bf16_rne(value);
    }
}

/* The small-head prefill realization permutes the original reduction tree
 * into lane-private registers, then warp shuffles. Original thread bits4,3,2
 * reduce in registers; bits1,0,7,6,5 reduce across lanes. Thus each ordered
 * FMA, addition, softmax recurrence and final BF16 publication is unchanged.
 * No block-wide synchronization is needed between independent heads. */
template<unsigned HEAD_DIM>
static __device__ __forceinline__ void attention_reduce_native_warp(
    const float *query, const float *local, const unsigned long long *local_positions,
    unsigned long long initial_local_count, unsigned long long local_stride,
    const float *compressed, const unsigned long long *compressed_positions,
    unsigned long long compressed_stride, const unsigned long long *selected,
    const unsigned long long *selected_count_ptr, unsigned long long topk_capacity,
    const float *sinks, unsigned long long query_heads, unsigned long long supplied_head_dim,
    unsigned long long sliding_window, unsigned long long ratio, unsigned int attention_class,
    unsigned long long phase_start_position, unsigned long long token_count,
    int candidate_block_visible, float *out, int *status)
{
    const unsigned long long head_dim = HEAD_DIM ? HEAD_DIM : supplied_head_dim;
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned long long task = (unsigned long long)blockIdx.x * 8ull + warp;
    unsigned long long ordinal = query_heads ? task / query_heads : token_count;
    unsigned long long head = query_heads ? task % query_heads : query_heads;
    if (!status || ordinal >= token_count || head >= query_heads) return;
    if (!query || !local || !local_positions || !sinks || !out || !query_heads ||
        !head_dim || head_dim > 512ull || head_dim != supplied_head_dim ||
        !sliding_window || !token_count || blockDim.x != 256u ||
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
                /* Keep the original runtime RSQ operation. Constant folding
                 * a specialized width can select a different rounded scale. */
                float score = dot * rsqrtf((float)supplied_head_dim);
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
    attention_reduce_native_warp<0>(query, local, local_positions, initial_local_count,
        local_stride, compressed, compressed_positions, compressed_stride, selected,
        selected_count_ptr, topk_capacity, sinks, query_heads, head_dim, sliding_window,
        ratio, attention_class, phase_start_position, token_count, candidate_block_visible, out, status);
}

/* Fixed geometry removes predicate/address lifetimes, not numerical work.
 * A separate entrypoint lets allocation of registers reflect that geometry. */
extern "C" __global__ void yvex_attention_reduce_native_warp_512(
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
    attention_reduce_native_warp<512>(query, local, local_positions, initial_local_count,
        local_stride, compressed, compressed_positions, compressed_stride, selected,
        selected_count_ptr, topk_capacity, sinks, query_heads, head_dim, sliding_window,
        ratio, attention_class, phase_start_position, token_count, candidate_block_visible, out, status);
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
