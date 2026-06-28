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
