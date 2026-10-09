/* Exceptional decoded row-dot recovery shared by CUDA kernel families. */
#ifndef SRC_BACKEND_CUDA_DOT_RECOVERY_H_INCLUDED
#define SRC_BACKEND_CUDA_DOT_RECOVERY_H_INCLUDED

#include "src/backend/cuda/kernel_primitives.h"

#ifdef __CUDACC__
/* Storage dispatch is invariant across a row. Constant decoding removes its
 * per-column branches; each accumulator still visits columns in source order. */
template <unsigned int Qtype>
static __device__ __forceinline__ float qtype_ordered_dot_f64(
    const unsigned char *row, const float *input, unsigned long long width)
{
    double recovered = 0.0;
    for (unsigned long long i = 0ull; i < width; ++i)
        recovered += (double)qtype_value(row, i, Qtype) * (double)input[i];
    return (float)recovered;
}

static __device__ float qtype_dot_recover_f64(
    const unsigned char *row, const float *input, unsigned long long width,
    unsigned int qtype)
{
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_F32>(row, input, width);
    case YVEX_GGUF_QTYPE_F16:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_F16>(row, input, width);
    case YVEX_GGUF_QTYPE_BF16:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_BF16>(row, input, width);
    case YVEX_GGUF_QTYPE_Q8_0:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_Q8_0>(row, input, width);
    case YVEX_GGUF_QTYPE_IQ2_XXS:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_IQ2_XXS>(row, input, width);
    case YVEX_GGUF_QTYPE_Q2_K:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_Q2_K>(row, input, width);
    case YVEX_GGUF_QTYPE_MXFP4:
        return qtype_ordered_dot_f64<YVEX_GGUF_QTYPE_MXFP4>(row, input, width);
    default: break;
    }
    double recovered = 0.0;
    for (unsigned long long i = 0ull; i < width; ++i)
        recovered += (double)qtype_value(row, i, qtype) * (double)input[i];
    return (float)recovered;
}

/* Certify the published F32 bits of the source-ordered F64 dot. Finite F32
 * products are exact in F64. Serial and warp sums differ by at most
 * (gamma_width + gamma_depth) * sum(abs(products)), depth=ceil(width/32)+5.
 * For width <= 2^50, 2*(width+depth)*2^-53 is a conservative coefficient.
 * The upward F32 norm is a bound, never an accumulation/publication class.
 * Ambiguous rounding, norm overflow and exceptional operands retain the
 * original ordered CUDA evaluation. All lanes of one complete warp enter;
 * only lane zero owns the result. See Contracts / Numerical ABI. */
/* Least significant nonzero binary digit of a finite F32. Zero contributes
 * no lattice constraint. Subnormals use their actual significand, not an
 * implicit leading bit. */
static __device__ __forceinline__ int qtype_float_quantum(float value)
{
    unsigned int bits = __float_as_uint(value) & 0x7fffffffu;
    unsigned int exponent = bits >> 23u;
    unsigned int significand = bits & 0x7fffffu;
    if (exponent) significand |= 0x800000u;
    if (!significand) return 1024;
    return (exponent ? (int)exponent - 150 : -149) + __ffs(significand) - 1;
}
template <unsigned int Qtype>
static __device__ __forceinline__ float qtype_certified_dot_f64(
    const unsigned char *row, const float *input, unsigned long long width,
    unsigned int dynamic_qtype)
{
    const unsigned int qtype = Qtype == ~0u ? dynamic_qtype : Qtype;
    unsigned int lane = threadIdx.x & 31u;
    double sum = 0.0;
    float norm = 0.0f;
    if (width > (1ull << 50u))
        return lane ? 0.0f : qtype_dot_recover_f64(row, input, width, qtype);
    for (unsigned long long i = lane; i < width; i += 32ull) {
        float weight = qtype_value(row, i, qtype), value = input[i];
        sum = __fma_rn((double)weight, (double)value, sum);
        norm = __fadd_ru(norm, __fmul_ru(fabsf(weight), fabsf(value)));
    }
    for (unsigned int offset = 16u; offset; offset >>= 1u) {
        sum = __dadd_rn(sum, __shfl_down_sync(0xffffffffu, sum, offset));
        norm = __fadd_ru(norm, __shfl_down_sync(0xffffffffu, norm, offset));
    }
    int certified = 0;
    float result = 0.0f;
    if (isfinite(sum) && isfinite(norm)) {
        /* A zero upper norm proves all products zero. RN accumulation from
         * +0 publishes +0, including negative-zero products. */
        unsigned long long depth = (width + 31ull) / 32ull + 5ull;
        double coefficient = (double)(width + depth) * 0x1p-52;
        double radius = __dmul_ru(coefficient, (double)norm);
        float lower = __double2float_rn(__dsub_rd(sum, radius));
        float upper = __double2float_rn(__dadd_ru(sum, radius));
        certified = norm == 0.0f ||
            __float_as_uint(lower) == __float_as_uint(upper);
        result = norm == 0.0f ? 0.0f : lower;
    }
    if (__shfl_sync(0xffffffffu, certified, 0)) return lane ? 0.0f : result;
    /* A rounding tie need not require serial recomputation: if every product
     * is a multiple of 2^quantum and the absolute sum is <= 2^(quantum+53),
     * every possible partial sum is exactly representable in F64. Both the
     * source-ordered and warp accumulations are then exact, including ties.
     * This second pass is only needed when the interval is inconclusive. */
    int quantum = 1024, finite = 1;
    for (unsigned long long i = lane; i < width; i += 32ull) {
        float weight = qtype_value(row, i, qtype), value = input[i];
        if (!isfinite(weight) || !isfinite(value)) finite = 0;
        else if (weight != 0.0f && value != 0.0f)
            quantum = min(quantum, qtype_float_quantum(weight) +
                                   qtype_float_quantum(value));
    }
    for (unsigned int offset = 16u; offset; offset >>= 1u) {
        quantum = min(quantum, __shfl_down_sync(0xffffffffu, quantum, offset));
        finite &= __shfl_down_sync(0xffffffffu, finite, offset);
    }
    if (lane) return 0.0f;
    if (finite && isfinite(norm) && (double)norm <= scalbn(1.0, quantum + 53))
        return (float)sum;
    return qtype_dot_recover_f64(row, input, width, qtype);
}

/* TwoSum gives an exact decomposition of each finite F32 addition. Directed
 * accumulation of its residual and the FMA product residual encloses the exact
 * dot. A product residual can underflow: allow half a minimum F32 subnormal
 * per product. Finally widen by the source-ordered F64 forward-error bound.
 * No F32 approximation is published without an identical-endpoint proof. */
template <unsigned int Qtype>
static __device__ __forceinline__ float qtype_interval_dot_f64(
    const unsigned char *row, const float *input, unsigned long long width,
    unsigned int dynamic_qtype)
{
    const unsigned int qtype = Qtype == ~0u ? dynamic_qtype : Qtype;
    unsigned int lane = threadIdx.x & 31u;
    float sum = 0.0f, low = 0.0f, high = 0.0f, norm = 0.0f;
    int valid = width <= (1ull << 50u);
    for (unsigned long long i = lane; i < width; i += 32ull) {
        float weight = qtype_value(row, i, qtype), value = input[i];
        float product = __fmul_rn(weight, value);
        float product_error = __fmaf_rn(weight, value, -product);
        float next = __fadd_rn(sum, product), delta = __fsub_rn(next, sum);
        float residual = __fadd_rn(__fsub_rn(sum, __fsub_rn(next, delta)),
                                  __fsub_rn(product, delta));
        valid &= isfinite(product) && isfinite(next) &&
                 isfinite(residual) && isfinite(product_error);
        sum = next;
        low = __fadd_rd(__fadd_rd(low, residual), product_error);
        high = __fadd_ru(__fadd_ru(high, residual), product_error);
        norm = __fadd_ru(norm, __fmul_ru(fabsf(weight), fabsf(value)));
    }
    double lower = __dadd_rd((double)sum, (double)low);
    double upper = __dadd_ru((double)sum, (double)high);
    for (unsigned int offset = 16u; offset; offset >>= 1u) {
        lower = __dadd_rd(lower, __shfl_down_sync(0xffffffffu, lower, offset));
        upper = __dadd_ru(upper, __shfl_down_sync(0xffffffffu, upper, offset));
        norm = __fadd_ru(norm, __shfl_down_sync(0xffffffffu, norm, offset));
        valid &= __shfl_down_sync(0xffffffffu, valid, offset);
    }
    float result = 0.0f;
    int certified = 0;
    if (!lane && valid && isfinite(norm) && isfinite(lower) && isfinite(upper)) {
        double radius = __dadd_ru(
            __dmul_ru((double)width * 0x1p-52, (double)norm),
            (double)width * 0x1p-150);
        float left = __double2float_rn(__dsub_rd(lower, radius));
        float right = __double2float_rn(__dadd_ru(upper, radius));
        certified = __float_as_uint(left) == __float_as_uint(right);
        result = left;
    }
    if (__shfl_sync(0xffffffffu, certified, 0)) return lane ? 0.0f : result;
    return qtype_certified_dot_f64<Qtype>(row, input, width, dynamic_qtype);
}

static __device__ float qtype_dot_certified_f64(
    const unsigned char *row, const float *input, unsigned long long width,
    unsigned int qtype)
{
    switch (qtype) {
    case YVEX_GGUF_QTYPE_F32:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_F32>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_F16:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_F16>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_BF16:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_BF16>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_Q8_0:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_Q8_0>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_IQ2_XXS:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_IQ2_XXS>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_Q2_K:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_Q2_K>(row, input, width, qtype);
    case YVEX_GGUF_QTYPE_MXFP4:
        return qtype_interval_dot_f64<YVEX_GGUF_QTYPE_MXFP4>(row, input, width, qtype);
    default:
        return qtype_interval_dot_f64<~0u>(row, input, width, qtype);
    }
}

/* Finite F32 products are exact in F64. Preparing them cooperatively changes
 * memory traffic, not source-order rounding: lane zero still adds every term
 * in column order, starting at +0. Exceptional operands remain fail-closed at
 * publication. This fallback must be entered by the whole 256-thread block. */
static __device__ float qtype_block_ordered_f64(
    const float *row, const float *input, unsigned long long width)
{
    __shared__ double products[256];
    double sum = 0.0;
    for (unsigned long long first = 0ull; first < width; first += 256ull) {
        unsigned long long column = first + threadIdx.x;
        if (column < width)
            products[threadIdx.x] = __dmul_rn((double)row[column], (double)input[column]);
        __syncthreads();
        if (!threadIdx.x) {
            unsigned long long count = min(256ull, width - first);
            for (unsigned long long i = 0ull; i < count; ++i)
                sum = __dadd_rn(sum, products[i]);
        }
        __syncthreads();
    }
    return threadIdx.x ? 0.0f : (float)sum;
}

/* Ordered-F64 prefix error enclosure. Products of finite F32
 * factors are exact and normal in F64 (or zero). Chunk sums have depth<=13.
 * T approximates the real prefix with error E; F bounds the source-ordered
 * F64 prefix error. For m terms, gamma_m<=m*2^-52 in this bounded geometry:
 * F' <= (1+gamma_m)F + gamma_m(|T|+E+N_chunk).
 * E' <= E + gamma_13 N_chunk + gamma_1(|T|+|s_chunk|).
 * All positive bound arithmetic rounds upward. Publication is allowed only
 * when T +/- (E+F) has identical F32 endpoints. Otherwise original fallback. */
static __device__ int qtype_block_prefix_certificate(
    const float *row, const float *input, unsigned long long width,
    float *published)
{
    __shared__ double chunks[128];
    __shared__ float norms[128], result;
    __shared__ int accepted;
    unsigned int lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    if (!width || width > 32768u || blockDim.x != 256u) return 0;
    unsigned int count = (width + 255u) / 256u;
    for (unsigned int chunk = warp; chunk < count; chunk += 8u) {
        double sum = 0.0;
        float norm = 0.0f;
        unsigned int end = min((unsigned int)width, (chunk + 1u) * 256u);
        for (unsigned int i = chunk * 256u + lane; i < end; i += 32u) {
            float a = row[i], b = input[i];
            sum = __fma_rn((double)a, (double)b, sum);
            norm = __fadd_ru(norm, __fmul_ru(fabsf(a), fabsf(b)));
        }
        for (unsigned int step = 16u; step; step >>= 1u) {
            sum = __dadd_rn(sum, __shfl_down_sync(~0u, sum, step));
            norm = __fadd_ru(norm, __shfl_down_sync(~0u, norm, step));
        }
        if (!lane) { chunks[chunk] = sum; norms[chunk] = norm; }
    }
    __syncthreads();
    if (!threadIdx.x) {
        double total = 0.0, real_error = 0.0, ordered_error = 0.0;
        for (unsigned int chunk = 0u; chunk < count; ++chunk) {
            unsigned int m = min(256u, (unsigned int)width - chunk * 256u);
            double gamma = (double)m * 0x1p-52;
            double magnitude = __dadd_ru(
                __dadd_ru(fabs(total), real_error), (double)norms[chunk]);
            ordered_error = __dadd_ru(
                __dmul_ru(__dadd_ru(1.0, gamma), ordered_error),
                __dmul_ru(gamma, magnitude));
            double chunk_error = __dmul_ru(13.0 * 0x1p-52, (double)norms[chunk]);
            double addition_error = __dmul_ru(
                0x1p-52, __dadd_ru(fabs(total), fabs(chunks[chunk])));
            real_error = __dadd_ru(
                __dadd_ru(real_error, chunk_error), addition_error);
            total = __dadd_rn(total, chunks[chunk]);
        }
        double radius = __dadd_ru(real_error, ordered_error);
        float lower = __double2float_rn(__dsub_rd(total, radius));
        float upper = __double2float_rn(__dadd_ru(total, radius));
        accepted = isfinite(total) && isfinite(radius) &&
            __float_as_uint(lower) == __float_as_uint(upper);
        result = lower;
    }
    __syncthreads();
    if (accepted && !threadIdx.x) *published = result;
    return accepted;
}

/* Narrow, wide matrices have too few results to fill the device with one
 * warp per result. Their admitted block-owned geometry cooperates across all
 * eight warps; the publication proof is the same, with the shorter depth. */
static __device__ float qtype_block_dot_certified_f64(
    const float *row, const float *input, unsigned long long width)
{
    __shared__ double sums[8];
    __shared__ float norms[8];
    __shared__ int quanta[8], decision;
    unsigned int lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    double sum = 0.0;
    float norm = 0.0f;
    for (unsigned long long i = threadIdx.x; i < width; i += 256ull) {
        sum = __fma_rn((double)row[i], (double)input[i], sum);
        norm = __fadd_ru(norm, __fmul_ru(fabsf(row[i]), fabsf(input[i])));
    }
    for (unsigned int offset = 16u; offset; offset >>= 1u) {
        sum = __dadd_rn(sum, __shfl_down_sync(0xffffffffu, sum, offset));
        norm = __fadd_ru(norm, __shfl_down_sync(0xffffffffu, norm, offset));
    }
    if (!lane) { sums[warp] = sum; norms[warp] = norm; }
    __syncthreads();
    if (!threadIdx.x) {
        sum = 0.0; norm = 0.0f;
        for (unsigned int i = 0u; i < 8u; ++i) {
            sum = __dadd_rn(sum, sums[i]);
            norm = __fadd_ru(norm, norms[i]);
        }
        sums[0] = sum; norms[0] = norm;
        unsigned long long depth = (width + 255ull) / 256ull + 13ull;
        double radius = __dmul_ru((double)(width + depth) * 0x1p-52, (double)norm);
        float lower = __double2float_rn(__dsub_rd(sum, radius));
        float upper = __double2float_rn(__dadd_ru(sum, radius));
        decision = width <= (1ull << 50u) && isfinite(sum) && isfinite(norm) &&
            (norm == 0.0f || __float_as_uint(lower) == __float_as_uint(upper));
        if (decision) sums[0] = norm == 0.0f ? 0.0 : (double)lower;
    }
    __syncthreads();
    if (decision) return threadIdx.x ? 0.0f : (float)sums[0];
    int quantum = 1024;
    for (unsigned long long i = threadIdx.x; i < width; i += 256ull) {
        if (!isfinite(row[i]) || !isfinite(input[i])) quantum = -4096;
        else if (row[i] != 0.0f && input[i] != 0.0f)
            quantum = min(quantum, qtype_float_quantum(row[i]) +
                                   qtype_float_quantum(input[i]));
    }
    for (unsigned int offset = 16u; offset; offset >>= 1u)
        quantum = min(quantum, __shfl_down_sync(0xffffffffu, quantum, offset));
    if (!lane) quanta[warp] = quantum;
    __syncthreads();
    if (!threadIdx.x) {
        for (unsigned int i = 0u; i < 8u; ++i) quantum = min(quantum, quanta[i]);
        decision = quantum != -4096 && isfinite(norms[0]) &&
            (double)norms[0] <= scalbn(1.0, quantum + 53);
    }
    __syncthreads();
    if (decision) return threadIdx.x ? 0.0f : (float)sums[0];
    float projected = 0.0f;
    if (qtype_block_prefix_certificate(row, input, width, &projected)) return projected;
    return qtype_block_ordered_f64(row, input, width);
}

static __device__ float qtype_q8_dot_recover_f64(
    const unsigned char *row, const unsigned char *input,
    unsigned long long blocks, unsigned long long weight_block,
    unsigned int qtype, int *status)
{
    double recovered = 0.0;
    for (unsigned long long block = 0ull; block < blocks; ++block) {
        const unsigned char *weight = row + block * weight_block;
        const unsigned char *q8 = input + block * YVEX_CUDA_Q8_K_BYTES;
        float scale = __uint_as_float(qtype_load_u32(q8));
        if (!isfinite(scale)) {
            atomicCAS(status, 0, 1);
            return 0.0f;
        }
        for (unsigned int i = 0u; i < YVEX_CUDA_Q8_K_BLOCK; ++i) {
            float decoded = qtype_value(weight, i, qtype);
            if (!isfinite(decoded)) {
                atomicCAS(status, 0, 1);
                return 0.0f;
            }
            int quantized = (int)(signed char)q8[4u + i];
            recovered += (double)decoded * (double)scale * (double)quantized;
        }
    }
    return (float)recovered;
}
#endif

#endif
