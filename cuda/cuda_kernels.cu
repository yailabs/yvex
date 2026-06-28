/*
 * cuda/cuda_kernels.cu - CUDA device kernels.
 *
 * This file owns CUDA device code. Host-side validation and Driver API launch
 * remain in cuda_ops.c.
 */

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
    unsigned long long total = hidden_size * token_count;
    unsigned long long token_index;
    unsigned long long dim;
    unsigned int token_id;

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
            exp = 1u;
            while ((mant & 0x0400u) == 0u) {
                mant <<= 1;
                exp -= 1u;
            }
            mant &= 0x03ffu;
            raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        raw = sign | 0x7f800000u | (mant << 13);
    } else {
        raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    }
    return __uint_as_float(raw);
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
    unsigned long long total = hidden_size * token_count;
    unsigned long long token_index;
    unsigned long long dim;
    unsigned int token_id;

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
    unsigned long long total = m * n;
    unsigned long long row;
    unsigned long long col;
    unsigned long long inner;
    float sum = 0.0f;

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

    if (threadIdx.x != 0 || blockIdx.x != 0) {
        return;
    }

    visible_count = causal ? position + 1ull : seq_len;
    for (i = 0; i < seq_len; ++i) {
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
        if (i == 0ull || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        float e = expf(score_scratch[i] - max_score);
        probability_scratch[i] = e;
        sum_exp += e;
    }
    if (sum_exp == 0.0f) {
        return;
    }
    for (i = 0; i < visible_count; ++i) {
        probability_scratch[i] = probability_scratch[i] / sum_exp;
    }
    for (d = 0; d < head_dim; ++d) {
        float value = 0.0f;
        for (i = 0; i < visible_count; ++i) {
            value += probability_scratch[i] * values[(i * head_dim) + d];
        }
        out[d] = value;
    }
}
