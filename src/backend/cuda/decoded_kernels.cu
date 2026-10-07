/* Lossless signed-digit preparation and certified ordered-F64 publication.
 * This is an execution workspace, not a new weight format or numerical class. */
#include "src/backend/cuda/dot_recovery.h"

extern "C" __global__ void yvex_decoded_prepare(
    const unsigned char *source, unsigned qtype, unsigned width, unsigned rows,
    unsigned char *digits, short2 *metadata, int *status)
{
    unsigned lane = threadIdx.x & 31u;
    unsigned long long block = (unsigned long long)blockIdx.x *
        (blockDim.x / 32u) + (threadIdx.x >> 5u);
    if (!status || *status) return;
    if (!source || !digits || !metadata || !width || width % 32u || !rows ||
        (qtype != YVEX_GGUF_QTYPE_BF16 && qtype != YVEX_GGUF_QTYPE_F32)) {
        if (!threadIdx.x) atomicCAS(status, 0, 2);
        return;
    }
    if (block >= (unsigned long long)rows * (width / 32u)) return;
    unsigned row = block / (width / 32u), part = block % (width / 32u);
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
    int output_bf16, int *status)
{
    unsigned lane = threadIdx.x & 31u;
    unsigned long long task = ((unsigned long long)blockIdx.x * blockDim.x + threadIdx.x) / 32ull;
    if (!status || *status) return;
    if (!weights || !input || !digits || !metadata || !output || !width || width > 8192u ||
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
        width > 4096u || rows > 1024u || inputs > 1024u || blockDim.x != 128u ||
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
