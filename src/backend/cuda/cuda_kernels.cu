/*
 * cuda/cuda_kernels.cu - CUDA device kernels.
 *
 * Owner: src/backend/cuda.
 * Owns: bounded device arithmetic for admitted backend operation variants.
 * Does not own: host validation, Driver API launches, qtype capability truth,
 *   quantization policy, model graphs, runtime, or generation.
 * Invariants: every exported kernel is resolved through the generated PTX
 *   bundle and its matching host owner validates byte/rank geometry first.
 * Boundary: a qtype row dot is primitive compute proof, not model execution.
 */

#include <yvex/gguf_qtype.h>

extern "C" __global__ void yvex_embed_f32(const float *embedding,
                                          const unsigned int *token_ids,
                                          float *out,
                                          unsigned long long hidden_size,
                                          unsigned long long vocab_size,
                                          unsigned long long token_count)
{
    unsigned long long idx =
        ((unsigned long long)blockIdx.x * (unsigned long long)blockDim.x) +
        (unsigned long long)threadIdx.x;
    unsigned long long total;
    unsigned long long token_index;
    unsigned long long dim;
    unsigned int token_id;
    const unsigned long long max_ull = ~0ull;

    if (!embedding || !token_ids || !out ||
        hidden_size == 0ull || vocab_size == 0ull || token_count == 0ull ||
        hidden_size > max_ull / token_count ||
        hidden_size > max_ull / vocab_size) {
        return;
    }
    total = hidden_size * token_count;
    if (idx >= total) {
        return;
    }

    token_index = idx / hidden_size;
    dim = idx % hidden_size;
    token_id = token_ids[token_index];
    if ((unsigned long long)token_id >= vocab_size) {
        return;
    }

    out[idx] = embedding[((unsigned long long)token_id * hidden_size) + dim];
}

static __device__ float yvex_f16_bits_to_float(unsigned int h)
{
    unsigned int sign = (h & 0x8000u) << 16;
    unsigned int exp = (h >> 10) & 0x1fu;
    unsigned int mant = h & 0x03ffu;
    unsigned int raw;

    if (exp == 0u) {
        if (mant == 0u) {
            raw = sign;
        } else {
            unsigned int shift = 0u;
            while ((mant & 0x0400u) == 0u) {
                mant <<= 1;
                shift++;
            }
            mant &= 0x03ffu;
            raw = sign | ((113u - shift) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        raw = sign | 0x7f800000u | (mant << 13);
    } else {
        raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    }
    return __uint_as_float(raw);
}

static __device__ unsigned int yvex_qtype_load_u16(
    const unsigned char *bytes)
{
    return (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
}

static __device__ unsigned int yvex_qtype_load_u32(
    const unsigned char *bytes)
{
    return (unsigned int)bytes[0] |
           ((unsigned int)bytes[1] << 8) |
           ((unsigned int)bytes[2] << 16) |
           ((unsigned int)bytes[3] << 24);
}

static __device__ float yvex_bf16_bits_to_float(unsigned int bits)
{
    return __uint_as_float(bits << 16);
}

static __device__ float yvex_e8m0_bits_to_float(unsigned int bits)
{
    if (bits == 0xffu) return __uint_as_float(0x7fc00000u);
    return __uint_as_float(bits == 0u ? 0x00400000u : bits << 23);
}

static __device__ float yvex_mxfp4_code_to_float(unsigned int code)
{
    float magnitude;
    switch (code & 7u) {
    case 0u: magnitude = 0.0f; break;
    case 1u: magnitude = 1.0f; break;
    case 2u: magnitude = 2.0f; break;
    case 3u: magnitude = 3.0f; break;
    case 4u: magnitude = 4.0f; break;
    case 5u: magnitude = 6.0f; break;
    case 6u: magnitude = 8.0f; break;
    default: magnitude = 12.0f; break;
    }
    return code & 8u ? -magnitude : magnitude;
}

/* Directly reconstructs one element without materializing an F32 tensor. */
static __device__ float yvex_qtype_value(const unsigned char *encoded,
                                         unsigned long long index,
                                         unsigned int qtype)
{
    if (qtype == YVEX_GGUF_QTYPE_F32) {
        return __uint_as_float(yvex_qtype_load_u32(encoded + index * 4ull));
    }
    if (qtype == YVEX_GGUF_QTYPE_F16) {
        return yvex_f16_bits_to_float(
            yvex_qtype_load_u16(encoded + index * 2ull));
    }
    if (qtype == YVEX_GGUF_QTYPE_BF16) {
        return yvex_bf16_bits_to_float(
            yvex_qtype_load_u16(encoded + index * 2ull));
    }
    if (qtype == YVEX_GGUF_QTYPE_I32) {
        unsigned int raw = yvex_qtype_load_u32(encoded + index * 4ull);
        int value = raw <= 0x7fffffffu
            ? (int)raw : -1 - (int)(0xffffffffu - raw);
        return (float)value;
    }
    if (qtype == YVEX_GGUF_QTYPE_Q8_0) {
        unsigned long long block = index / 32ull;
        unsigned int lane = (unsigned int)(index % 32ull);
        const unsigned char *bytes = encoded + block * 34ull;
        float scale = yvex_f16_bits_to_float(yvex_qtype_load_u16(bytes));
        int quantized = bytes[2u + lane] <= 127u
            ? (int)bytes[2u + lane] : (int)bytes[2u + lane] - 256;
        return scale * (float)quantized;
    }
    if (qtype == YVEX_GGUF_QTYPE_MXFP4) {
        unsigned long long block = index / 32ull;
        unsigned int lane = (unsigned int)(index % 32ull);
        const unsigned char *bytes = encoded + block * 17ull;
        unsigned int packed = bytes[1u + (lane & 15u)];
        unsigned int code = lane < 16u ? packed & 15u : packed >> 4;
        return yvex_mxfp4_code_to_float(code) *
               yvex_e8m0_bits_to_float(bytes[0]) * 0.5f;
    }
    if (qtype == YVEX_GGUF_QTYPE_Q2_K) {
        unsigned long long block = index / 256ull;
        unsigned int lane = (unsigned int)(index % 256ull);
        const unsigned char *bytes = encoded + block * 84ull;
        unsigned int subblock = lane / 16u;
        unsigned int half = lane / 128u;
        unsigned int local_subblock = subblock & 7u;
        unsigned int pair = local_subblock & 1u;
        unsigned int group = local_subblock / 2u;
        unsigned int packed = bytes[16u + half * 32u + pair * 16u +
                                    (lane & 15u)];
        unsigned int code = (packed >> (group * 2u)) & 3u;
        unsigned int scale_byte = bytes[subblock];
        float scale = yvex_f16_bits_to_float(
            yvex_qtype_load_u16(bytes + 80u));
        float minimum = yvex_f16_bits_to_float(
            yvex_qtype_load_u16(bytes + 82u));
        return scale * (float)(scale_byte & 15u) * (float)code -
               minimum * (float)(scale_byte >> 4);
    }
    return __uint_as_float(0x7fc00000u);
}

/* Bounded qtype arithmetic proof: encoded row times one F32 vector. */
extern "C" __global__ void yvex_qtype_row_dot(
    const unsigned char *encoded,
    const float *vector,
    unsigned long long elements,
    unsigned int qtype,
    float *out)
{
    unsigned long long index;
    double sum = 0.0;

    if (blockIdx.x != 0u || threadIdx.x != 0u || !encoded || !vector ||
        !out || elements == 0ull) return;
    for (index = 0ull; index < elements; ++index)
        sum += (double)yvex_qtype_value(encoded, index, qtype) *
               (double)vector[index];
    out[0] = (float)sum;
}

extern "C" __global__ void yvex_embed_f16_to_f32(const unsigned short *embedding,
                                                 const unsigned int *token_ids,
                                                 float *out,
                                                 unsigned long long hidden_size,
                                                 unsigned long long vocab_size,
                                                 unsigned long long token_count)
{
    unsigned long long idx =
        ((unsigned long long)blockIdx.x * (unsigned long long)blockDim.x) +
        (unsigned long long)threadIdx.x;
    unsigned long long total;
    unsigned long long token_index;
    unsigned long long dim;
    unsigned int token_id;
    const unsigned long long max_ull = ~0ull;

    if (!embedding || !token_ids || !out ||
        hidden_size == 0ull || vocab_size == 0ull || token_count == 0ull ||
        hidden_size > max_ull / token_count ||
        hidden_size > max_ull / vocab_size) {
        return;
    }
    total = hidden_size * token_count;
    if (idx >= total) {
        return;
    }

    token_index = idx / hidden_size;
    dim = idx % hidden_size;
    token_id = token_ids[token_index];
    if ((unsigned long long)token_id >= vocab_size) {
        return;
    }

    out[idx] = yvex_f16_bits_to_float((unsigned int)embedding[((unsigned long long)token_id * hidden_size) + dim]);
}

extern "C" __global__ void yvex_rms_norm_f32_weight_f32(const float *input,
                                                        const float *weight,
                                                        float *out,
                                                        unsigned long long hidden_size,
                                                        float epsilon)
{
    extern __shared__ float scratch[];
    unsigned int tid = threadIdx.x;
    unsigned int stride = blockDim.x;
    unsigned long long i;
    float sum = 0.0f;
    float inv_rms;

    if (!input || !weight || !out || hidden_size == 0ull || epsilon <= 0.0f) {
        return;
    }
    for (i = (unsigned long long)tid; i < hidden_size; i += (unsigned long long)stride) {
        float v = input[i];
        sum += v * v;
    }
    scratch[tid] = sum;
    __syncthreads();

    for (unsigned int offset = blockDim.x >> 1; offset > 0; offset >>= 1) {
        if (tid < offset) {
            scratch[tid] += scratch[tid + offset];
        }
        __syncthreads();
    }

    inv_rms = rsqrtf((scratch[0] / (float)hidden_size) + epsilon);
    for (i = (unsigned long long)tid; i < hidden_size; i += (unsigned long long)stride) {
        out[i] = input[i] * inv_rms * weight[i];
    }
}

extern "C" __global__ void yvex_rms_norm_f32_weight_f16(const float *input,
                                                        const unsigned short *weight,
                                                        float *out,
                                                        unsigned long long hidden_size,
                                                        float epsilon)
{
    extern __shared__ float scratch[];
    unsigned int tid = threadIdx.x;
    unsigned int stride = blockDim.x;
    unsigned long long i;
    float sum = 0.0f;
    float inv_rms;

    if (!input || !weight || !out || hidden_size == 0ull || epsilon <= 0.0f) {
        return;
    }
    for (i = (unsigned long long)tid; i < hidden_size; i += (unsigned long long)stride) {
        float v = input[i];
        sum += v * v;
    }
    scratch[tid] = sum;
    __syncthreads();

    for (unsigned int offset = blockDim.x >> 1; offset > 0; offset >>= 1) {
        if (tid < offset) {
            scratch[tid] += scratch[tid + offset];
        }
        __syncthreads();
    }

    inv_rms = rsqrtf((scratch[0] / (float)hidden_size) + epsilon);
    for (i = (unsigned long long)tid; i < hidden_size; i += (unsigned long long)stride) {
        out[i] = input[i] * inv_rms * yvex_f16_bits_to_float((unsigned int)weight[i]);
    }
}

extern "C" __global__ void yvex_rope_f32(const float *input,
                                         float *out,
                                         unsigned long long head_dim,
                                         unsigned long long position,
                                         float inverse_root)
{
    unsigned long long pair =
        ((unsigned long long)blockIdx.x * (unsigned long long)blockDim.x) +
        (unsigned long long)threadIdx.x;
    unsigned long long pair_count = head_dim / 2ull;
    unsigned long long even_index;
    unsigned long long odd_index;
    unsigned long long i;
    float frequency = 1.0f;
    float angle;
    float sine;
    float cosine;
    float even;
    float odd;

    if (!input || !out || head_dim < 2ull || (head_dim & 1ull) != 0ull) {
        return;
    }
    if (pair >= pair_count) {
        return;
    }
    for (i = 0; i < pair; ++i) {
        frequency *= inverse_root;
    }
    angle = (float)position * frequency;
    sine = sinf(angle);
    cosine = cosf(angle);
    even_index = pair * 2ull;
    odd_index = even_index + 1ull;
    even = input[even_index];
    odd = input[odd_index];
    out[even_index] = (even * cosine) - (odd * sine);
    out[odd_index] = (even * sine) + (odd * cosine);
}

extern "C" __global__ void yvex_matmul_f32(const float *input,
                                           const float *weight,
                                           float *out,
                                           unsigned long long m,
                                           unsigned long long k,
                                           unsigned long long n)
{
    unsigned long long idx =
        ((unsigned long long)blockIdx.x * (unsigned long long)blockDim.x) +
        (unsigned long long)threadIdx.x;
    unsigned long long total;
    unsigned long long row;
    unsigned long long col;
    unsigned long long inner;
    float sum = 0.0f;
    const unsigned long long max_ull = ~0ull;

    if (!input || !weight || !out || m == 0ull || k == 0ull || n == 0ull ||
        m > max_ull / n || k > max_ull / n) {
        return;
    }
    total = m * n;
    if (idx >= total) {
        return;
    }
    row = idx / n;
    col = idx % n;
    for (inner = 0; inner < k; ++inner) {
        sum += input[(row * k) + inner] * weight[(inner * n) + col];
    }
    out[idx] = sum;
}

extern "C" __global__ void yvex_mlp_f32(const float *input,
                                        const float *gate_weight,
                                        const float *up_weight,
                                        const float *down_weight,
                                        float *intermediate,
                                        float *out,
                                        unsigned long long batch,
                                        unsigned long long hidden_dim,
                                        unsigned long long ffn_dim,
                                        unsigned long long expert_count,
                                        unsigned long long expert_id,
                                        int routed_expert_mode)
{
    unsigned long long row;
    unsigned long long j;
    unsigned long long h;
    unsigned long long gate_offset = 0ull;
    unsigned long long up_offset = 0ull;
    unsigned long long down_offset = 0ull;
    unsigned long long intermediate_total;
    unsigned long long output_total;
    unsigned long long index;
    const unsigned long long max_ull = ~0ull;

    if (blockIdx.x != 0) {
        return;
    }
    if (!input || !gate_weight || !up_weight || !down_weight || !intermediate || !out ||
        batch == 0ull || hidden_dim == 0ull || ffn_dim == 0ull ||
        batch > max_ull / ffn_dim || batch > max_ull / hidden_dim ||
        hidden_dim > max_ull / ffn_dim || ffn_dim > max_ull / hidden_dim) {
        return;
    }
    if (routed_expert_mode) {
        unsigned long long up_elements = hidden_dim * ffn_dim;
        unsigned long long down_elements = ffn_dim * hidden_dim;
        if (expert_count == 0ull || expert_id >= expert_count ||
            expert_count > max_ull / up_elements ||
            expert_count > max_ull / down_elements ||
            expert_id > max_ull / up_elements ||
            expert_id > max_ull / down_elements) {
            return;
        }
        gate_offset = expert_id * up_elements;
        up_offset = gate_offset;
        down_offset = expert_id * down_elements;
    }

    intermediate_total = batch * ffn_dim;
    output_total = batch * hidden_dim;
    for (index = (unsigned long long)threadIdx.x;
         index < intermediate_total;
         index += (unsigned long long)blockDim.x) {
        float gate_sum = 0.0f;
        float up_sum = 0.0f;
        float silu;

        row = index / ffn_dim;
        j = index % ffn_dim;
        for (h = 0; h < hidden_dim; ++h) {
            float x = input[(row * hidden_dim) + h];
            gate_sum += x * gate_weight[gate_offset + (h * ffn_dim) + j];
            up_sum += x * up_weight[up_offset + (h * ffn_dim) + j];
        }
        silu = gate_sum / (1.0f + expf(-gate_sum));
        intermediate[index] = silu * up_sum;
    }
    __syncthreads();

    for (index = (unsigned long long)threadIdx.x;
         index < output_total;
         index += (unsigned long long)blockDim.x) {
        float sum = 0.0f;

        row = index / hidden_dim;
        h = index % hidden_dim;
        for (j = 0; j < ffn_dim; ++j) {
            sum += intermediate[(row * ffn_dim) + j] *
                   down_weight[down_offset + (j * hidden_dim) + h];
        }
        out[index] = sum;
    }
}

extern "C" __global__ void yvex_attention_f32(const float *query,
                                              const float *keys,
                                              const float *values,
                                              float *score_scratch,
                                              float *probability_scratch,
                                              float *out,
                                              unsigned long long seq_len,
                                              unsigned long long position,
                                              unsigned long long head_dim,
                                              float scale,
                                              int causal)
{
    unsigned long long visible_count;
    unsigned long long i;
    unsigned long long d;
    float max_score = 0.0f;
    float sum_exp = 0.0f;
    __shared__ int softmax_valid;

    if (blockIdx.x != 0) {
        return;
    }
    if (!query || !keys || !values || !score_scratch || !probability_scratch || !out ||
        seq_len == 0ull || head_dim == 0ull || position >= seq_len ||
        seq_len > (~0ull) / head_dim) {
        return;
    }

    visible_count = causal ? position + 1ull : seq_len;
    for (i = (unsigned long long)threadIdx.x;
         i < seq_len;
         i += (unsigned long long)blockDim.x) {
        float score = 0.0f;
        if (causal && i > position) {
            score_scratch[i] = 0.0f;
            probability_scratch[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += query[d] * keys[(i * head_dim) + d];
        }
        score *= scale;
        score_scratch[i] = score;
    }
    __syncthreads();

    if (threadIdx.x == 0) {
        max_score = score_scratch[0];
        for (i = 1ull; i < visible_count; ++i) {
            if (score_scratch[i] > max_score) {
                max_score = score_scratch[i];
            }
        }
        for (i = 0; i < visible_count; ++i) {
            float e = expf(score_scratch[i] - max_score);
            probability_scratch[i] = e;
            sum_exp += e;
        }
        if (sum_exp == 0.0f) {
            softmax_valid = 0;
            for (i = 0; i < visible_count; ++i) {
                probability_scratch[i] = 0.0f;
            }
        } else {
            softmax_valid = 1;
            for (i = 0; i < visible_count; ++i) {
                probability_scratch[i] = probability_scratch[i] / sum_exp;
            }
        }
    }
    __syncthreads();

    for (d = (unsigned long long)threadIdx.x;
         d < head_dim;
         d += (unsigned long long)blockDim.x) {
        float value = 0.0f;
        if (softmax_valid) {
            for (i = 0; i < visible_count; ++i) {
                value += probability_scratch[i] * values[(i * head_dim) + d];
            }
        }
        out[d] = value;
    }
}

/* Direct encoded matrix/vector projection used by the admitted DeepSeek
 * attention path. Each block owns one output row and never materializes a
 * decoded weight matrix. */
extern "C" __global__ void yvex_deepseek_qtype_matvec(
    const unsigned char *encoded,
    unsigned long long row_bytes,
    unsigned long long row_width,
    unsigned long long start_row,
    unsigned long long row_count,
    unsigned int qtype,
    const float *vector,
    float *out,
    int *status)
{
    extern __shared__ double partial[];
    unsigned long long row = (unsigned long long)blockIdx.x;
    unsigned int lane = threadIdx.x;
    double sum = 0.0;
    const unsigned char *row_data;

    if (!encoded || !vector || !out || !status || !row_bytes || !row_width ||
        row >= row_count) return;
    if (*status != 0) return;
    row_data = encoded + (start_row + row) * row_bytes;
    for (unsigned long long i = (unsigned long long)lane; i < row_width;
         i += (unsigned long long)blockDim.x) {
        float weight = yvex_qtype_value(row_data, i, qtype);
        float value = vector[i];
        if (!isfinite(weight) || !isfinite(value)) {
            atomicCAS(status, 0, 1);
            continue;
        }
        sum += (double)weight * (double)value;
    }
    partial[lane] = sum;
    __syncthreads();
    for (unsigned int offset = blockDim.x >> 1; offset; offset >>= 1) {
        if (lane < offset) partial[lane] += partial[lane + offset];
        __syncthreads();
    }
    if (lane == 0u) {
        if (!isfinite(partial[0])) atomicCAS(status, 0, 1);
        else out[row] = (float)partial[0];
    }
}

/* Decodes one admitted scalar tensor directly into F32 device storage. */
extern "C" __global__ void yvex_deepseek_decode(
    const unsigned char *encoded,
    unsigned long long count,
    unsigned int qtype,
    float *out,
    int *status)
{
    unsigned long long index =
        (unsigned long long)blockIdx.x * (unsigned long long)blockDim.x +
        (unsigned long long)threadIdx.x;
    if (!encoded || !out || !status || index >= count || *status != 0) return;
    float value = yvex_qtype_value(encoded, index, qtype);
    if (!isfinite(value)) atomicCAS(status, 0, 1);
    else out[index] = value;
}

/* Applies one exact weighted RMS normalization in-place. */
extern "C" __global__ void yvex_deepseek_weighted_norm(
    float *values,
    unsigned long long count,
    const unsigned char *weight,
    unsigned int weight_qtype,
    double epsilon,
    int *status)
{
    extern __shared__ double partial[];
    unsigned int lane = threadIdx.x;
    double sum = 0.0;
    if (blockIdx.x != 0u || !values || !weight || !status || !count ||
        epsilon <= 0.0 || *status != 0) return;
    for (unsigned long long i = (unsigned long long)lane; i < count;
         i += (unsigned long long)blockDim.x) {
        double value = (double)values[i];
        if (!isfinite(value)) {
            atomicCAS(status, 0, 1);
            continue;
        }
        sum += value * value;
    }
    partial[lane] = sum;
    __syncthreads();
    for (unsigned int offset = blockDim.x >> 1; offset; offset >>= 1) {
        if (lane < offset) partial[lane] += partial[lane + offset];
        __syncthreads();
    }
    if (*status != 0) return;
    double inverse = rsqrt(partial[0] / (double)count + epsilon);
    if (!isfinite(inverse)) {
        if (lane == 0u) atomicCAS(status, 0, 1);
        return;
    }
    for (unsigned long long i = (unsigned long long)lane; i < count;
         i += (unsigned long long)blockDim.x) {
        double scale = (double)yvex_qtype_value(weight, i, weight_qtype);
        double result = (double)values[i] * inverse * scale;
        if (!isfinite(scale) || !isfinite(result)) atomicCAS(status, 0, 1);
        else values[i] = (float)result;
    }
}

/* Applies unweighted per-head query normalization. */
extern "C" __global__ void yvex_deepseek_unit_norm(
    float *values,
    unsigned long long vector_count,
    unsigned long long vector_width,
    double epsilon,
    int *status)
{
    extern __shared__ double partial[];
    unsigned long long vector_index = (unsigned long long)blockIdx.x;
    unsigned int lane = threadIdx.x;
    double sum = 0.0;
    float *vector;
    if (!values || !status || vector_index >= vector_count || !vector_width ||
        epsilon <= 0.0 || *status != 0) return;
    vector = values + vector_index * vector_width;
    for (unsigned long long i = (unsigned long long)lane; i < vector_width;
         i += (unsigned long long)blockDim.x) {
        double value = (double)vector[i];
        if (!isfinite(value)) {
            atomicCAS(status, 0, 1);
            continue;
        }
        sum += value * value;
    }
    partial[lane] = sum;
    __syncthreads();
    for (unsigned int offset = blockDim.x >> 1; offset; offset >>= 1) {
        if (lane < offset) partial[lane] += partial[lane + offset];
        __syncthreads();
    }
    if (*status != 0) return;
    double inverse = rsqrt(partial[0] / (double)vector_width + epsilon);
    if (!isfinite(inverse)) {
        if (lane == 0u) atomicCAS(status, 0, 1);
        return;
    }
    for (unsigned long long i = (unsigned long long)lane; i < vector_width;
         i += (unsigned long long)blockDim.x)
        vector[i] = (float)((double)vector[i] * inverse);
}

static __device__ double yvex_deepseek_yarn_frequency(
    unsigned long long pair,
    unsigned long long rope_dims,
    unsigned long long theta,
    unsigned long long scaling_factor,
    unsigned long long original_context,
    unsigned long long beta_fast,
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

/* Applies the admitted partial RoPE/YaRN equation to independent vectors. */
extern "C" __global__ void yvex_deepseek_rope(
    float *values,
    unsigned long long vector_count,
    unsigned long long vector_width,
    unsigned long long rope_dims,
    unsigned long long token_position,
    unsigned long long theta,
    unsigned long long scaling_factor,
    unsigned long long original_context,
    unsigned long long beta_fast,
    unsigned long long beta_slow,
    int inverse,
    int *status)
{
    unsigned long long pair =
        (unsigned long long)blockIdx.x * (unsigned long long)blockDim.x +
        (unsigned long long)threadIdx.x;
    unsigned long long pairs_per_vector = rope_dims / 2ull;
    unsigned long long total = vector_count * pairs_per_vector;
    if (!values || !status || !rope_dims || rope_dims > vector_width ||
        (rope_dims & 1ull) || pair >= total || *status != 0) return;
    unsigned long long vector_index = pair / pairs_per_vector;
    unsigned long long local_pair = pair % pairs_per_vector;
    unsigned long long start = vector_width - rope_dims;
    unsigned long long offset = vector_index * vector_width + start +
                                local_pair * 2ull;
    double frequency = yvex_deepseek_yarn_frequency(
        local_pair, rope_dims, theta, scaling_factor, original_context,
        beta_fast, beta_slow);
    double angle = (double)token_position * frequency;
    double c = cos(angle);
    double s = inverse ? -sin(angle) : sin(angle);
    double x = (double)values[offset];
    double y = (double)values[offset + 1ull];
    double left = x * c - y * s;
    double right = x * s + y * c;
    if (!isfinite(left) || !isfinite(right)) atomicCAS(status, 0, 1);
    else {
        values[offset] = (float)left;
        values[offset + 1ull] = (float)right;
    }
}

static __device__ float yvex_deepseek_fp8_decode(unsigned int code)
{
    unsigned int sign = code & 0x80u;
    unsigned int exponent = (code >> 3u) & 0x0fu;
    unsigned int mantissa = code & 0x07u;
    float value;
    if ((code & 0x7fu) == 0u) return sign ? -0.0f : 0.0f;
    value = exponent == 0u
        ? ldexpf((float)mantissa / 8.0f, -6)
        : ldexpf(1.0f + (float)mantissa / 8.0f, (int)exponent - 7);
    if (value > 448.0f) value = 448.0f;
    return sign ? -value : value;
}

static __device__ unsigned int yvex_deepseek_fp8_encode(float value)
{
    float best_error = INFINITY;
    unsigned int best = 0u;
    if (value > 448.0f) value = 448.0f;
    if (value < -448.0f) value = -448.0f;
    for (unsigned int code = 0u; code < 256u; ++code) {
        float error = fabsf(yvex_deepseek_fp8_decode(code) - value);
        if (error < best_error) {
            best_error = error;
            best = code;
        }
    }
    return best;
}

static __device__ float yvex_deepseek_fp4_decode(unsigned int code)
{
    const float table[8] = {0.0f, 0.5f, 1.0f, 1.5f,
                            2.0f, 3.0f, 4.0f, 6.0f};
    float value = table[code & 7u];
    return (code & 8u) ? -value : value;
}

static __device__ unsigned int yvex_deepseek_fp4_encode(float value)
{
    const float threshold[7] = {0.25f, 0.75f, 1.25f, 1.75f,
                                2.5f, 3.5f, 5.0f};
    float magnitude = fabsf(value);
    unsigned int code = 0u;
    for (unsigned int i = 0u; i < 7u; ++i)
        if (magnitude > threshold[i]) code++;
    if (code && signbit(value)) code |= 8u;
    return code;
}

static __device__ float yvex_deepseek_power_two_ceil(float value)
{
    if (!isfinite(value) || value <= 0.0f) return 0.0f;
    return exp2f(ceilf(log2f(value)));
}

/* Executes Hadamard plus FP8/FP4 UE8M0 fake quantization entirely on device.
 * One block owns one vector so stage ordering is explicit and deterministic. */
extern "C" __global__ void yvex_deepseek_activation(
    float *values,
    unsigned long long vector_count,
    unsigned long long vector_width,
    unsigned long long block_width,
    unsigned int quantization,
    int hadamard,
    int *status)
{
    unsigned long long vector_index = (unsigned long long)blockIdx.x;
    if (threadIdx.x != 0u || !values || !status || vector_index >= vector_count ||
        !vector_width || !block_width || vector_width % block_width ||
        *status != 0) return;
    float *vector = values + vector_index * vector_width;
    if (hadamard) {
        if ((vector_width & (vector_width - 1ull)) != 0ull ||
            vector_width > 1024ull) {
            atomicCAS(status, 0, 2);
            return;
        }
        for (unsigned long long step = 1ull; step < vector_width; step *= 2ull)
            for (unsigned long long block = 0ull; block < vector_width;
                 block += step * 2ull)
                for (unsigned long long lane = 0ull; lane < step; ++lane) {
                    float left = vector[block + lane];
                    float right = vector[block + lane + step];
                    vector[block + lane] = left + right;
                    vector[block + lane + step] = left - right;
                }
        float scale = rsqrtf((float)vector_width);
        for (unsigned long long i = 0ull; i < vector_width; ++i)
            vector[i] *= scale;
    }
    for (unsigned long long offset = 0ull; offset < vector_width;
         offset += block_width) {
        float amax = quantization == 1u ? 1.0e-4f : 0.0f;
        for (unsigned long long i = 0ull; i < block_width; ++i) {
            float value = vector[offset + i];
            if (!isfinite(value)) {
                atomicCAS(status, 0, 1);
                return;
            }
            float magnitude = fabsf(value);
            if (magnitude > amax) amax = magnitude;
        }
        float minimum = 6.0f * ldexpf(1.0f, -126);
        if (quantization == 2u && amax < minimum) amax = minimum;
        float scale = yvex_deepseek_power_two_ceil(
            amax / (quantization == 1u ? 448.0f : 6.0f));
        if (!isfinite(scale) || scale <= 0.0f) {
            atomicCAS(status, 0, 1);
            return;
        }
        for (unsigned long long i = 0ull; i < block_width; ++i) {
            float normalized = vector[offset + i] / scale;
            if (quantization == 1u) {
                if (normalized > 448.0f) normalized = 448.0f;
                if (normalized < -448.0f) normalized = -448.0f;
                vector[offset + i] =
                    yvex_deepseek_fp8_decode(
                        yvex_deepseek_fp8_encode(normalized)) * scale;
            } else if (quantization == 2u) {
                vector[offset + i] =
                    yvex_deepseek_fp4_decode(
                        yvex_deepseek_fp4_encode(normalized)) * scale;
            } else {
                atomicCAS(status, 0, 2);
                return;
            }
        }
    }
}

/* Executes one complete ratio-4 or ratio-128 compressor transition on device. */
extern "C" __global__ void yvex_deepseek_rolling(
    const float *before_kv,
    const float *before_score,
    const float *token_kv,
    const float *token_score,
    const float *ape,
    float *after_kv,
    float *after_score,
    float *compressed,
    unsigned long long ratio,
    unsigned long long head_dim,
    unsigned long long state_width,
    unsigned long long state_slots,
    unsigned long long cursor,
    int overlap,
    int emit,
    int *status)
{
    unsigned int thread = threadIdx.x;
    unsigned long long extent = state_width * state_slots;
    unsigned long long insert_slot = overlap ? ratio + cursor : cursor;
    if (blockIdx.x != 0u || !before_kv || !before_score || !token_kv ||
        !token_score || !ape || !after_kv || !after_score || !compressed ||
        !status || !ratio || !head_dim || !state_width || !state_slots ||
        cursor >= ratio || *status != 0) return;
    for (unsigned long long i = (unsigned long long)thread; i < extent;
         i += (unsigned long long)blockDim.x) {
        after_kv[i] = before_kv[i];
        after_score[i] = before_score[i];
    }
    for (unsigned long long lane = (unsigned long long)thread;
         lane < state_width; lane += (unsigned long long)blockDim.x) {
        float kv = token_kv[lane];
        float score = token_score[lane] + ape[lane];
        if (!isfinite(kv) || !isfinite(score)) atomicCAS(status, 0, 1);
        after_kv[insert_slot * state_width + lane] = kv;
        after_score[insert_slot * state_width + lane] = score;
    }
    __syncthreads();
    if (emit) {
        for (unsigned long long lane = (unsigned long long)thread;
             lane < head_dim; lane += (unsigned long long)blockDim.x) {
            double maximum = -INFINITY;
            double denominator = 0.0;
            double value = 0.0;
            for (unsigned long long slot = 0ull; slot < ratio; ++slot) {
                double score = (double)after_score[slot * state_width + lane];
                if (score > maximum) maximum = score;
                if (overlap) {
                    score = (double)after_score[(ratio + slot) * state_width +
                                                  lane + head_dim];
                    if (score > maximum) maximum = score;
                }
            }
            for (unsigned long long slot = 0ull; slot < ratio; ++slot) {
                double score = (double)after_score[slot * state_width + lane];
                double weight = exp(score - maximum);
                denominator += weight;
                value += weight * (double)after_kv[slot * state_width + lane];
                if (overlap) {
                    score = (double)after_score[(ratio + slot) * state_width +
                                                lane + head_dim];
                    weight = exp(score - maximum);
                    denominator += weight;
                    value += weight *
                        (double)after_kv[(ratio + slot) * state_width +
                                         lane + head_dim];
                }
            }
            if (!isfinite(denominator) || denominator <= 0.0 ||
                !isfinite(value)) atomicCAS(status, 0, 1);
            else compressed[lane] = (float)(value / denominator);
        }
        __syncthreads();
        if (overlap) {
            for (unsigned long long i = (unsigned long long)thread;
                 i < ratio * state_width; i += (unsigned long long)blockDim.x) {
                after_kv[i] = after_kv[ratio * state_width + i];
                after_score[i] = after_score[ratio * state_width + i];
            }
        }
    }
}

/* Scores and ranks the complete CSA candidate set on device. */
extern "C" __global__ void yvex_deepseek_topk(
    const float *index_query,
    const float *index_weights,
    const float *history_indexer,
    const unsigned long long *history_positions,
    unsigned long long history_count,
    unsigned long long history_stride,
    const float *current_indexer,
    const unsigned long long *current_positions,
    unsigned long long current_count,
    unsigned long long current_stride,
    unsigned long long heads,
    unsigned long long head_dim,
    unsigned long long ratio,
    unsigned long long query_position,
    unsigned long long k,
    unsigned long long *selected,
    unsigned long long *selected_positions,
    unsigned long long *selected_count,
    unsigned long long *valid_count,
    float *scores,
    unsigned long long *valid_indexes,
    int *status)
{
    if (blockIdx.x != 0u || threadIdx.x != 0u || !index_query ||
        !index_weights || !selected || !selected_positions || !selected_count ||
        !valid_count || !scores || !valid_indexes || !status || !heads ||
        !head_dim || !ratio || !k || *status != 0) return;
    unsigned long long total = history_count + current_count;
    unsigned long long valid = 0ull;
    for (unsigned long long candidate = 0ull; candidate < total; ++candidate) {
        const float *row;
        unsigned long long position;
        if (candidate < history_count) {
            row = history_indexer + candidate * history_stride;
            position = history_positions[candidate];
        } else {
            unsigned long long local = candidate - history_count;
            row = current_indexer + local * current_stride;
            position = current_positions[local];
        }
        if (!row || position > query_position ||
            position > ~0ull - ratio + 1ull ||
            position + ratio - 1ull > query_position) continue;
        for (unsigned long long prior = 0ull; prior < valid; ++prior) {
            unsigned long long prior_candidate = valid_indexes[prior];
            unsigned long long prior_position = prior_candidate < history_count
                ? history_positions[prior_candidate]
                : current_positions[prior_candidate - history_count];
            if (prior_position == position) {
                atomicCAS(status, 0, 1);
                return;
            }
        }
        double score = 0.0;
        for (unsigned long long head = 0ull; head < heads; ++head) {
            double dot = 0.0;
            const float *query = index_query + head * head_dim;
            for (unsigned long long lane = 0ull; lane < head_dim; ++lane)
                dot += (double)query[lane] * (double)row[lane];
            if (dot < 0.0) dot = 0.0;
            score += dot * (double)index_weights[head];
        }
        score *= rsqrt((double)head_dim) * rsqrt((double)heads);
        if (!isfinite(score)) {
            atomicCAS(status, 0, 1);
            return;
        }
        scores[valid] = (float)score;
        valid_indexes[valid] = candidate;
        valid++;
    }
    unsigned long long chosen = valid < k ? valid : k;
    for (unsigned long long rank = 0ull; rank < chosen; ++rank) {
        unsigned long long best = ~0ull;
        for (unsigned long long i = 0ull; i < valid; ++i) {
            unsigned long long candidate = valid_indexes[i];
            unsigned long long position = candidate < history_count
                ? history_positions[candidate]
                : current_positions[candidate - history_count];
            int already = 0;
            for (unsigned long long prior = 0ull; prior < rank; ++prior)
                if (selected[prior] == candidate) already = 1;
            if (already) continue;
            if (best == ~0ull || scores[i] > scores[best] ||
                (scores[i] == scores[best] && position <
                    (valid_indexes[best] < history_count
                        ? history_positions[valid_indexes[best]]
                        : current_positions[valid_indexes[best] - history_count])))
                best = i;
        }
        selected[rank] = valid_indexes[best];
        selected_positions[rank] = selected[rank] < history_count
            ? history_positions[selected[rank]]
            : current_positions[selected[rank] - history_count];
    }
    *selected_count = chosen;
    *valid_count = valid;
}

/* Executes sparse/local masking, stable softmax and value reduction on device.
 * One block owns one query head; selected compressed indexes originate only
 * from the device top-k kernel. */
extern "C" __global__ void yvex_deepseek_reduce(
    const float *query,
    const float *history_local,
    const unsigned long long *history_local_positions,
    unsigned long long history_local_count,
    unsigned long long history_local_stride,
    const float *current_kv,
    unsigned long long current_kv_stride,
    const float *history_compressed,
    const unsigned long long *history_compressed_positions,
    unsigned long long history_compressed_count,
    unsigned long long history_compressed_stride,
    const float *current_compressed,
    const unsigned long long *current_compressed_positions,
    unsigned long long current_compressed_count,
    unsigned long long current_compressed_stride,
    const unsigned long long *selected,
    const unsigned long long *selected_count_ptr,
    const float *sinks,
    unsigned long long query_heads,
    unsigned long long head_dim,
    unsigned long long sliding_window,
    unsigned long long ratio,
    unsigned int attention_class,
    unsigned long long token_position,
    float *out,
    int *status)
{
    extern __shared__ double partial[];
    unsigned long long head = (unsigned long long)blockIdx.x;
    unsigned int thread = threadIdx.x;
    if (!query || !current_kv || !sinks || !out || !status ||
        head >= query_heads || !head_dim || *status != 0) return;
    const float *q = query + head * head_dim;
    double maximum = (double)sinks[head];
    unsigned long long local_total = history_local_count + 1ull;
    unsigned long long selected_count = selected_count_ptr
        ? *selected_count_ptr : 0ull;
    unsigned long long compressed_total = attention_class == 2u
        ? history_compressed_count + current_compressed_count
        : selected_count;
    for (unsigned long long pass = 0ull; pass < 2ull; ++pass) {
        unsigned long long count = pass == 0ull ? local_total : compressed_total;
        for (unsigned long long candidate = 0ull; candidate < count; ++candidate) {
            const float *row = NULL;
            unsigned long long position = ~0ull;
            if (pass == 0ull) {
                if (candidate < history_local_count) {
                    row = history_local + candidate * history_local_stride;
                    position = history_local_positions[candidate];
                } else {
                    row = current_kv;
                    position = token_position;
                }
                unsigned long long first = token_position + 1ull > sliding_window
                    ? token_position + 1ull - sliding_window : 0ull;
                if (position < first || position > token_position) continue;
            } else {
                unsigned long long index = attention_class == 2u
                    ? candidate : selected[candidate];
                if (index < history_compressed_count) {
                    row = history_compressed + index * history_compressed_stride;
                    position = history_compressed_positions[index];
                } else {
                    unsigned long long local = index - history_compressed_count;
                    row = current_compressed + local * current_compressed_stride;
                    position = current_compressed_positions[local];
                }
                if (!row || position > token_position ||
                    position > ~0ull - ratio + 1ull ||
                    position + ratio - 1ull > token_position) continue;
            }
            double dot = 0.0;
            for (unsigned long long lane = (unsigned long long)thread;
                 lane < head_dim; lane += (unsigned long long)blockDim.x)
                dot += (double)q[lane] * (double)row[lane];
            partial[thread] = dot;
            __syncthreads();
            for (unsigned int offset = blockDim.x >> 1; offset; offset >>= 1) {
                if (thread < offset) partial[thread] += partial[thread + offset];
                __syncthreads();
            }
            if (thread == 0u) {
                double score = partial[0] * rsqrt((double)head_dim);
                if (score > maximum) maximum = score;
            }
            __syncthreads();
        }
    }
    double denominator = exp((double)sinks[head] - maximum);
    for (unsigned long long lane = (unsigned long long)thread;
         lane < head_dim; lane += (unsigned long long)blockDim.x)
        out[head * head_dim + lane] = 0.0f;
    __syncthreads();
    for (unsigned long long pass = 0ull; pass < 2ull; ++pass) {
        unsigned long long count = pass == 0ull ? local_total : compressed_total;
        for (unsigned long long candidate = 0ull; candidate < count; ++candidate) {
            const float *row = NULL;
            unsigned long long position = ~0ull;
            if (pass == 0ull) {
                if (candidate < history_local_count) {
                    row = history_local + candidate * history_local_stride;
                    position = history_local_positions[candidate];
                } else {
                    row = current_kv;
                    position = token_position;
                }
                unsigned long long first = token_position + 1ull > sliding_window
                    ? token_position + 1ull - sliding_window : 0ull;
                if (position < first || position > token_position) continue;
            } else {
                unsigned long long index = attention_class == 2u
                    ? candidate : selected[candidate];
                if (index < history_compressed_count) {
                    row = history_compressed + index * history_compressed_stride;
                    position = history_compressed_positions[index];
                } else {
                    unsigned long long local = index - history_compressed_count;
                    row = current_compressed + local * current_compressed_stride;
                    position = current_compressed_positions[local];
                }
                if (!row || position > token_position ||
                    position > ~0ull - ratio + 1ull ||
                    position + ratio - 1ull > token_position) continue;
            }
            double dot = 0.0;
            for (unsigned long long lane = (unsigned long long)thread;
                 lane < head_dim; lane += (unsigned long long)blockDim.x)
                dot += (double)q[lane] * (double)row[lane];
            partial[thread] = dot;
            __syncthreads();
            for (unsigned int offset = blockDim.x >> 1; offset; offset >>= 1) {
                if (thread < offset) partial[thread] += partial[thread + offset];
                __syncthreads();
            }
            double probability = exp(partial[0] * rsqrt((double)head_dim) - maximum);
            if (thread == 0u) partial[0] = probability;
            __syncthreads();
            probability = partial[0];
            if (thread == 0u) denominator += probability;
            for (unsigned long long lane = (unsigned long long)thread;
                 lane < head_dim; lane += (unsigned long long)blockDim.x)
                out[head * head_dim + lane] += (float)(probability * row[lane]);
            __syncthreads();
        }
    }
    __shared__ double final_denominator;
    if (thread == 0u) final_denominator = denominator;
    __syncthreads();
    if (!isfinite(final_denominator) || final_denominator <= 0.0) {
        if (thread == 0u) atomicCAS(status, 0, 1);
        return;
    }
    for (unsigned long long lane = (unsigned long long)thread;
         lane < head_dim; lane += (unsigned long long)blockDim.x)
        out[head * head_dim + lane] =
            (float)((double)out[head * head_dim + lane] / final_denominator);
}
