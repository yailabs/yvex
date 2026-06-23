/*
 * YVEX - CUDA device kernels
 *
 *
 * Purpose:
 *   Owns CUDA device code. Host-side backend validation and Driver API launch
 *   remain in cuda_ops.c so the public YVEX surface stays plain C.
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
