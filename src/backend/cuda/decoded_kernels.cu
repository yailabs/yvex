/* Decoded contractions: explicit F32 accumulation, or lossless signed digits
 * with certified ordered-F64 publication. Dispatch keeps the classes distinct. */
#include "src/backend/cuda/dot_recovery.h"
#include <cuda_bf16.h>
#include <mma.h>

/* Exact phase-local layout preparation. Initialize every element even after
 * refusal: an already queued library contraction must never read undefined
 * scratch. The owning transaction, not this temporary, controls publication. */
extern "C" __global__ void yvex_bf16_prepare(const unsigned char *source,
    unsigned long long row_bytes, unsigned width, unsigned rows, unsigned qtype,
    __nv_bfloat16 *prepared, int *status)
{
    unsigned long long at = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (at >= (unsigned long long)rows * width) return;
    float value = qtype_value(source + at / width * row_bytes, at % width, qtype);
    __nv_bfloat16 packed = __float2bfloat16_rn(value);
    unsigned bits = __float_as_uint(value) & 0x7fffffffu, exponent = bits >> 23u;
    bool valid = isfinite(value) && __bfloat162float(packed) == value &&
        (!bits || (exponent >= 71u && exponent <= 182u));
    if (!valid) atomicCAS(status, 0, 3);
    prepared[at] = valid ? packed : __float2bfloat16_rn(0.0f);
}

extern "C" __global__ void yvex_matrix_finish(float *values,
    unsigned long long count, int output_bf16, int *status)
{
    unsigned long long at = (unsigned long long)blockIdx.x * blockDim.x + threadIdx.x;
    if (at >= count || !status || *status) return;
    float value = output_bf16 ? float_to_bf16_rne(values[at]) : values[at];
    if (!isfinite(value)) atomicCAS(status, 0, 1);
    else values[at] = value;
}

/* Small physical widths use independent F32 SIMT contractions, not a padded
 * 32-column MMA or the ordered-F64 reference class. One warp owns one dot;
 * four FMA chains and a fixed tree stay inside the admitted gamma_K bound. */
static __device__ void bf16_narrow_rows(const unsigned short *weights,
    unsigned width, unsigned rows, unsigned inputs, const float *input,
    float *output, int output_bf16, int *status)
{
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    unsigned groups = (rows + 7u) / 8u;
    unsigned row = blockIdx.x % groups * 8u + warp, column = blockIdx.x / groups;
    if (row >= rows || column >= inputs) return;
    float sums[4] = {};
    for (unsigned k = lane * 4u; k < width; k += 128u) {
#pragma unroll
        for (unsigned j = 0u; j < 4u; ++j) if (k + j < width) {
            unsigned short encoded = weights[(unsigned long long)row * width + k + j];
            unsigned we = (encoded >> 7u) & 255u;
            float w = __uint_as_float((unsigned)encoded << 16u);
            float x = input[(unsigned long long)column * width + k + j];
            unsigned bits = __float_as_uint(x) & 0x7fffffffu, xe = bits >> 23u;
            bool valid = isfinite(w) && isfinite(x) &&
                (!(encoded & 0x7fffu) || (we >= 71u && we <= 182u)) &&
                !(bits & 65535u) && (!bits || (xe >= 71u && xe <= 182u));
            if (!valid) atomicCAS(status, 0, 4);
            sums[j] = __fmaf_rn(valid ? w : 0.0f, valid ? x : 0.0f, sums[j]);
        }
    }
    float value = __fadd_rn(__fadd_rn(sums[0], sums[1]), __fadd_rn(sums[2], sums[3]));
    for (unsigned shift = 16u; shift; shift >>= 1u)
        value = __fadd_rn(value, __shfl_down_sync(0xffffffffu, value, shift));
    if (!lane) {
        value = output_bf16 ? float_to_bf16_rne(value) : value;
        if (!isfinite(value)) atomicCAS(status, 0, 1);
        else if (!atomicAdd(status, 0)) output[(unsigned long long)column * rows + row] = value;
    }
}

/* Explicit BF16/F32 contraction, not ordered-dot equivalence. Activations
 * already carry semantic BF16; reject an accidental precision conversion.
 * CTA-local tiles replace lossless digit preparation, not artifact storage. */
extern "C" __global__ void yvex_bf16_matrix_rows(
    const unsigned short *weights, unsigned width, unsigned rows, unsigned inputs,
    const float *input, float *output, int output_bf16, int *status)
{
    if (!status) return;
    __shared__ int refused;
    if (!threadIdx.x) refused = atomicAdd(status, 0);
    __syncthreads();
    if (refused) return;
#if __CUDA_ARCH__ >= 800
    if (!weights || !input || !output || !width || width > 16384u || width % 32u ||
        !rows || rows > 16384u || !inputs || inputs > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
        blockDim.x != 256u ||
        (output_bf16 != 0 && output_bf16 != 1)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (inputs < 32u) {
        bf16_narrow_rows(weights, width, rows, inputs, input, output, output_bf16, status);
        return;
    }
    using namespace nvcuda;
    __shared__ __align__(32) __nv_bfloat16 a[64 * 32], b[32 * 32];
    __shared__ __align__(32) float y[64 * 32];
    unsigned groups = (inputs + 31u) / 32u;
    unsigned rb = blockIdx.x / groups * 64u, cb = blockIdx.x % groups * 32u;
    unsigned warp = threadIdx.x >> 5u, wr = warp / 2u * 16u, wc = warp % 2u * 16u;
    nvcuda::wmma::fragment<wmma::accumulator,16,16,16,float> sum;
    wmma::fill_fragment(sum, 0.0f);
    for (unsigned k = 0u; k < width; k += 32u) {
        for (unsigned i = threadIdx.x; i < 64u * 32u; i += 256u) {
            unsigned r = rb + i / 32u;
            unsigned bits = r < rows ? weights[(unsigned long long)r * width + k + i % 32u] : 0u;
            float value = __uint_as_float(bits << 16u);
            unsigned exponent = (bits >> 7u) & 255u;
            if (!isfinite(value) || ((bits & 0x7fffu) && (exponent < 71u || exponent > 182u)))
                atomicCAS(status, 0, 3);
            a[i] = __float2bfloat16_rn(value);
        }
        for (unsigned i = threadIdx.x; i < 32u * 32u; i += 256u) {
            unsigned c = cb + i / 32u;
            float value = c < inputs ? input[(unsigned long long)c * width + k + i % 32u] : 0.0f;
            __nv_bfloat16 packed = __float2bfloat16_rn(value);
            unsigned bits = __float_as_uint(value) & 0x7fffffffu;
            unsigned exponent = bits >> 23u;
            if (!isfinite(value) || __bfloat162float(packed) != value ||
                (bits && (exponent < 71u || exponent > 182u))) atomicCAS(status, 0, 4);
            b[i] = packed;
        }
        __syncthreads();
        for (unsigned half = 0u; half < 2u; ++half) {
            wmma::fragment<wmma::matrix_a,16,16,16,__nv_bfloat16,wmma::row_major> af;
            wmma::fragment<wmma::matrix_b,16,16,16,__nv_bfloat16,wmma::col_major> bf;
            wmma::load_matrix_sync(af, a + wr * 32u + half * 16u, 32u);
            wmma::load_matrix_sync(bf, b + wc * 32u + half * 16u, 32u);
            wmma::mma_sync(sum, af, bf, sum);
        }
        __syncthreads();
    }
    wmma::store_matrix_sync(y + wr * 32u + wc, sum, 32u, wmma::mem_row_major);
    __syncthreads();
    for (unsigned i = threadIdx.x; i < 64u * 32u; i += 256u) {
        unsigned r = rb + i / 32u, c = cb + i % 32u;
        if (r < rows && c < inputs) {
            float value = output_bf16 ? float_to_bf16_rne(y[i]) : y[i];
            if (!isfinite(value)) atomicCAS(status, 0, 1);
            else if (!*status) output[(unsigned long long)c * rows + r] = value;
        }
    }
#else
    if (!threadIdx.x) atomicCAS(status, 0, 2);
#endif
}

/* Row-major weights and activations, no packing/allocation. Four independent
 * F32 accumulators per lane followed by a fixed F32 tree; no TF32 conversion.
 * The caller owns finite-result validation and transactional publication. */
extern "C" __global__ __launch_bounds__(128) void yvex_f32_matrix_rows(
    const float *weights, const float *input, float *output,
    unsigned long long rows, unsigned long long width, unsigned long long inputs,
    int additive)
{
    unsigned long long row = blockIdx.x % rows, batch = blockIdx.x / rows;
    if (batch >= inputs) return;
    const float *w = weights + row * width, *x = input + batch * width;
    float sums[4] = {};
    for (unsigned long long k = threadIdx.x; k < width; k += 512ull) {
        #pragma unroll
        for (unsigned j = 0u; j < 4u; ++j)
            if (k + j * 128ull < width)
                sums[j] = __fmaf_rn(w[k + j * 128ull], x[k + j * 128ull], sums[j]);
    }
    float value = __fadd_rn(__fadd_rn(sums[0], sums[1]), __fadd_rn(sums[2], sums[3]));
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    for (unsigned offset = 16u; offset; offset >>= 1u)
        value = __fadd_rn(value, __shfl_down_sync(~0u, value, offset));
    __shared__ float partial[4];
    if (!lane) partial[warp] = value;
    __syncthreads();
    if (!threadIdx.x) {
        value = __fadd_rn(__fadd_rn(partial[0], partial[1]), __fadd_rn(partial[2], partial[3]));
        unsigned long long index = batch * rows + row;
        output[index] = additive ? __fadd_rn(value, output[index]) : value;
    }
}

static __device__ __forceinline__ int decoded_prepare_block(
    const unsigned char *source, unsigned qtype, unsigned width, unsigned row,
    unsigned part, unsigned char *digits, short2 *metadata)
{
    unsigned lane = threadIdx.x & 31u;
    unsigned long long block = (unsigned long long)row * (width / 32u) + part;
    float value = qtype_value(source + (unsigned long long)row * width *
        (qtype == YVEX_GGUF_QTYPE_BF16 ? 2u : 4u), part * 32u + lane, qtype);
    unsigned bits = __float_as_uint(value), exponent = (bits >> 23u) & 255u;
    unsigned significand = bits & 0x7fffffu;
    if (exponent) significand |= 0x800000u;
    int quantum = significand ? (exponent ? (int)exponent - 150 : -149) +
        __ffs(significand) - 1 : 1024;
    int top = significand ? (exponent ? (int)exponent - 127 :
        31 - __clz(significand) - 149) : -1024;
    int invalid = exponent == 255u;
    for (unsigned offset = 16u; offset; offset >>= 1u) {
        quantum = min(quantum, __shfl_xor_sync(~0u, quantum, offset));
        top = max(top, __shfl_xor_sync(~0u, top, offset));
        invalid |= __shfl_xor_sync(~0u, invalid, offset);
    }
    if (top == -1024) quantum = 0;
    invalid |= top - quantum + 1 > 22;
    if (!lane) metadata[block] = make_short2(quantum, invalid ? -32768 : top);
    int shift = (exponent ? (int)exponent - 150 : -149) - quantum;
    int integer = invalid || !significand ? 0 :
        (int)(shift >= 0 ? significand << shift : significand >> -shift);
    if (bits >> 31u) integer = -integer;
    for (unsigned digit = 0u; digit < 3u; ++digit) {
        int low = (signed char)integer;
        digits[((unsigned long long)row * 3u + digit) * width + part * 32u + lane] =
            (unsigned char)low;
        integer = (integer - low) / 256;
    }
    return invalid;
}

extern "C" __global__ void yvex_decoded_prepare(
    const unsigned char *source, unsigned qtype, unsigned width, unsigned rows,
    unsigned char *digits, short2 *metadata, int *status, int *row_eligibility)
{
    if (!status) return;
    if (row_eligibility) {
        __shared__ int prior_status;
        if (!threadIdx.x) prior_status = *status;
        __syncthreads();
        if (prior_status) return;
    } else if (__shfl_sync(~0u, *status, 0)) return;
    if (!source || !digits || !metadata || !width || width % 32u || !rows ||
        (qtype != YVEX_GGUF_QTYPE_BF16 && qtype != YVEX_GGUF_QTYPE_F32) ||
        (row_eligibility && (blockDim.x != 256u || width > 8192u || rows > 128u))) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (row_eligibility) {
        unsigned row = blockIdx.x, warp = threadIdx.x >> 5u;
        if (row >= rows) return;
        int invalid = 0;
        for (unsigned part = warp; part < width / 32u; part += 8u)
            invalid |= decoded_prepare_block(source, qtype, width, row, part, digits, metadata);
        int invalid_threads = __syncthreads_count(invalid);
        if (!threadIdx.x) row_eligibility[row] = invalid_threads ? 0 : 1;
    } else {
        unsigned long long block = (unsigned long long)blockIdx.x *
            (blockDim.x / 32u) + (threadIdx.x >> 5u);
        if (block < (unsigned long long)rows * (width / 32u))
            decoded_prepare_block(source, qtype, width, block / (width / 32u),
                block % (width / 32u), digits, metadata);
    }
}

static __device__ __forceinline__ void decoded_integer_mma(
    int result[4], const int left[4], const int right[2])
{
    asm volatile("mma.sync.aligned.m16n8k32.row.col.s32.s8.s8.s32 "
        "{%0,%1,%2,%3}, {%4,%5,%6,%7}, {%8,%9}, {%0,%1,%2,%3};"
        : "+r"(result[0]), "+r"(result[1]), "+r"(result[2]), "+r"(result[3])
        : "r"(left[0]), "r"(left[1]), "r"(left[2]), "r"(left[3]),
          "r"(right[0]), "r"(right[1]));
}

/* Finite F32 factor exponents and their 32-product bounds stay strictly
 * inside normal F64. No general-purpose scaling or subnormal path is needed. */
static __device__ __forceinline__ double decoded_power(int exponent)
{
    return __longlong_as_double((long long)(exponent + 1023) << 52u);
}

/* MXFP4 has one exact binary scale per 32 weights. Three signed activation
 * digits therefore give an exact INT64 block product, without quantizing the
 * input. Lanes own different blocks; the final F32 still requires the ordered
 * F64 interval/lattice proof. Unsupported digit spans and exceptional scales
 * execute the original certified CUDA dot, never an approximate result. */
extern "C" __global__ void yvex_decoded_mxfp4_rows(
    const unsigned char *weights, const float *input,
    const unsigned char *digits, const short2 *metadata, unsigned width,
    unsigned rows, unsigned groups, unsigned inputs, float *output,
    int output_bf16, int *status, const int *row_eligibility)
{
    unsigned lane = threadIdx.x & 31u;
    unsigned long long task = ((unsigned long long)blockIdx.x * blockDim.x + threadIdx.x) / 32ull;
    if (!status || *status) return;
    if (!weights || !input || !digits || !metadata || !output || !row_eligibility || !width || width > 8192u ||
        width % 32u || !rows || rows > 16384u || !groups || groups > 8u ||
        !inputs || inputs > 16u || blockDim.x != 256u || (output_bf16 != 0 && output_bf16 != 1)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned long long row = task / inputs;
    unsigned column = task % inputs, blocks = width / 32u;
    if (row >= (unsigned long long)rows * groups) return;
    unsigned activation = column * groups + (unsigned)(row / rows);
    const unsigned char *weight = weights + row * blocks * 17ull;
    const float *values = input + (unsigned long long)activation * width;
    /* Preparation is activation-owned, not repeated for every weight row.
     * Ineligible lossless digits execute the unchanged certified dot directly. */
    if (!row_eligibility[activation]) {
        float value = qtype_dot_certified_f64(weight, values, width, YVEX_GGUF_QTYPE_MXFP4);
        if (!lane) {
            if (!isfinite(value)) atomicCAS(status, 0, 1);
            else output[(unsigned long long)column * rows * groups + row] =
                output_bf16 ? float_to_bf16_rne(value) : value;
        }
        return;
    }
    double sum = 0.0, norm = 0.0;
    int quantum = 1024, unsupported = 0;
    for (unsigned block = lane; block < blocks; block += 32u) {
        short2 meta = metadata[activation * blocks + block];
        unsigned scale = weight[block * 17u];
        if (meta.y == -32768 || scale > 251u) { unsupported = 1; continue; }
        int accum[3] = {};
        for (unsigned i = 0u; i < 4u; ++i) {
            unsigned packed = qtype_load_u32(weight + block * 17u + 1u + i * 4u);
            int low = mxfp4_i8x4(packed, 0u), high = mxfp4_i8x4(packed, 1u);
#pragma unroll
            for (unsigned digit = 0u; digit < 3u; ++digit) {
                const unsigned char *a = digits +
                    ((unsigned long long)activation * 3ull + digit) * width + block * 32u + i * 4u;
                accum[digit] = __dp4a(low, (int)qtype_load_u32(a), accum[digit]);
                accum[digit] = __dp4a(high, (int)qtype_load_u32(a + 16u), accum[digit]);
            }
        }
        long long integer = accum[0] + (long long)accum[1] * 256ll + (long long)accum[2] * 65536ll;
        int exponent = meta.x + (int)scale - 128;
        sum = __dadd_rn(sum, __dmul_rn((double)integer, decoded_power(exponent)));
        if (meta.y != -1024) {
            /* |activation| < 2^(top+1), |integer weight| <= 12, 32 terms. */
            norm = __dadd_ru(norm, decoded_power(meta.y + (int)scale - 128 + 10));
            quantum = min(quantum, exponent);
        }
    }
    float value;
    if (__any_sync(~0u, unsupported)) {
        value = qtype_dot_certified_f64(weight, values, width, YVEX_GGUF_QTYPE_MXFP4);
    } else {
        for (unsigned offset = 16u; offset; offset >>= 1u) {
            sum = __dadd_rn(sum, __shfl_down_sync(~0u, sum, offset));
            norm = __dadd_ru(norm, __shfl_down_sync(~0u, norm, offset));
            quantum = min(quantum, __shfl_down_sync(~0u, quantum, offset));
        }
        int certified = 0;
        if (!lane) {
            double radius = __dmul_ru((double)(width + blocks + 5u) * 0x1p-52, norm);
            float lower = __double2float_rn(__dsub_rd(sum, radius));
            float upper = __double2float_rn(__dadd_ru(sum, radius));
            certified = isfinite(sum) && isfinite(norm) &&
                (norm == 0.0 || norm <= decoded_power(quantum + 53) ||
                 __float_as_uint(lower) == __float_as_uint(upper));
        }
        value = __shfl_sync(~0u, certified, 0) ? (float)sum :
            qtype_dot_certified_f64(weight, values, width, YVEX_GGUF_QTYPE_MXFP4);
    }
    if (!lane) {
        if (!isfinite(value)) atomicCAS(status, 0, 1);
        else output[(unsigned long long)column * rows * groups + row] =
            output_bf16 ? float_to_bf16_rne(value) : value;
    }
}

/* Signed digits multiply exactly into INT64 block sums (at most 49 bits).
 * Ordered F64 and blocked F64 round to the same F32 only when the interval
 * or exact binary lattice proves it. Otherwise evaluate the original dot.
 * A rejected digit representation is not a rejected model operation. */
extern "C" __global__ void yvex_decoded_rows(
    const unsigned char *left, const short2 *left_metadata,
    const unsigned char *right, const short2 *right_metadata,
    unsigned width, unsigned rows, unsigned inputs, float *output,
    const unsigned char *weights, const float *input, unsigned qtype,
    int output_bf16, int *status)
{
    unsigned lane = threadIdx.x & 31u, warp = threadIdx.x >> 5u;
    if (!status || *status) return;
    if (!left || !left_metadata || !right || !right_metadata || !output ||
        !weights || !input || !width || width % 32u || !rows || !inputs ||
        width > 4096u || rows > 1024u || inputs > YVEX_EXECUTION_PREFILL_MAXIMUM_WIDTH ||
        blockDim.x != 128u ||
        (qtype != YVEX_GGUF_QTYPE_BF16 && qtype != YVEX_GGUF_QTYPE_F32) ||
        (output_bf16 != 0 && output_bf16 != 1)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    unsigned groups = (inputs + 31u) / 32u, blocks = width / 32u;
    unsigned row_base = (blockIdx.x / groups) * 16u;
    unsigned input_base = (blockIdx.x % groups) * 32u + warp * 8u;
    unsigned group = lane >> 2u, thread = lane & 3u;
    if (input_base >= inputs) return;
    double sums[4] = {}, norms[4] = {};
    int quanta[4] = {1024, 1024, 1024, 1024};
    for (unsigned block = 0u; block < blocks; ++block) {
        int a[3][4], b[3][2];
        short2 ma[2], mb;
        for (unsigned j = 0u; j < 2u; ++j)
            ma[j] = row_base + group + j * 8u < rows ?
                left_metadata[(row_base + group + j * 8u) * blocks + block] :
                make_short2(0, -1024);
        mb = input_base + group < inputs ?
            right_metadata[(input_base + group) * blocks + block] : make_short2(0, -1024);
        #pragma unroll
        for (unsigned digit = 0u; digit < 3u; ++digit) {
            #pragma unroll
            for (unsigned j = 0u; j < 4u; ++j) {
                unsigned row = row_base + group + (j & 1u) * 8u;
                a[digit][j] = row < rows ? (int)qtype_load_u32(left +
                    ((unsigned long long)row * 3u + digit) * width +
                    block * 32u + thread * 4u + (j >> 1u) * 16u) : 0;
            }
            #pragma unroll
            for (unsigned j = 0u; j < 2u; ++j)
                b[digit][j] = input_base + group < inputs ? (int)qtype_load_u32(right +
                    ((unsigned long long)(input_base + group) * 3u + digit) * width +
                    block * 32u + thread * 4u + j * 16u) : 0;
        }
        int need_a = __any_sync(~0u, ma[0].y - ma[0].x + 1 > 14 || ma[1].y - ma[1].x + 1 > 14);
        int need_b = __any_sync(~0u, mb.y - mb.x + 1 > 14);
        long long accum[4] = {};
        #pragma unroll
        for (unsigned da = 0u; da < 3u; ++da)
            #pragma unroll
            for (unsigned db = 0u; db < 3u; ++db)
                if ((da < 2u || need_a) && (db < 2u || need_b)) {
                    int product[4] = {};
                    decoded_integer_mma(product, a[da], b[db]);
                    #pragma unroll
                    for (unsigned j = 0u; j < 4u; ++j)
                        accum[j] += (long long)product[j] * (1ll << (8u * (da + db)));
                }
        #pragma unroll
        for (unsigned j = 0u; j < 4u; ++j) {
            int bq = __shfl_sync(~0u, (int)mb.x, (thread * 2u + (j & 1u)) * 4u);
            int bt = __shfl_sync(~0u, (int)mb.y, (thread * 2u + (j & 1u)) * 4u);
            int quantum = ma[j >> 1u].x + bq;
            if (ma[j >> 1u].y == -32768 || bt == -32768) {
                unsigned row = row_base + group + (j >> 1u) * 8u;
                unsigned column = input_base + thread * 2u + (j & 1u);
                if (row >= rows || column >= inputs) continue;
                const unsigned char *weight_row = weights + (unsigned long long)row * width *
                    (qtype == YVEX_GGUF_QTYPE_BF16 ? 2u : 4u);
                double partial = 0.0, norm = 0.0;
                quantum = 1024;
                for (unsigned k = 0u; k < 32u; ++k) {
                    float a_value = qtype_value(weight_row, block * 32u + k, qtype);
                    float b_value = input[(unsigned long long)column * width + block * 32u + k];
                    partial = __fma_rn((double)a_value, (double)b_value, partial);
                    norm = __dadd_ru(norm, fabs((double)a_value * (double)b_value));
                    if (a_value != 0.0f && b_value != 0.0f)
                        quantum = min(quantum, qtype_float_quantum(a_value) + qtype_float_quantum(b_value));
                }
                sums[j] = __dadd_rn(sums[j], partial);
                norms[j] = __dadd_ru(norms[j], norm);
                quanta[j] = min(quanta[j], quantum);
                continue;
            }
            sums[j] = __dadd_rn(sums[j], __dmul_rn((double)accum[j], decoded_power(quantum)));
            if (ma[j >> 1u].y != -1024 && bt != -1024) {
                norms[j] = __dadd_ru(norms[j], decoded_power(ma[j >> 1u].y + bt + 7));
                quanta[j] = min(quanta[j], quantum);
            }
        }
    }
    #pragma unroll
    for (unsigned j = 0u; j < 4u; ++j) {
        unsigned row = row_base + group + (j >> 1u) * 8u;
        unsigned column = input_base + thread * 2u + (j & 1u);
        if (row >= rows || column >= inputs) continue;
        double radius = __dmul_ru((double)(width + blocks) * 0x1p-52, norms[j]);
        float lower = __double2float_rn(__dsub_rd(sums[j], radius));
        float upper = __double2float_rn(__dadd_ru(sums[j], radius));
        bool certified = isfinite(sums[j]) && isfinite(norms[j]) && (norms[j] == 0.0 ||
            norms[j] <= decoded_power(quanta[j] + 53) ||
            __float_as_uint(lower) == __float_as_uint(upper));
        float result = certified ? (float)sums[j] : qtype_dot_recover_f64(
            weights + (unsigned long long)row * width *
                (qtype == YVEX_GGUF_QTYPE_BF16 ? 2u : 4u),
            input + (unsigned long long)column * width, width, qtype);
        if (output_bf16) result = float_to_bf16_rne(result);
        if (!isfinite(result)) atomicCAS(status, 0, 1);
        else output[(unsigned long long)column * rows + row] = result;
    }
}
