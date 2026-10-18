/* Process-local CUDA argument ABI for causal, 64-key online attention.
 * Borrowed device views and transaction-private scratch; never persisted.
 * This realization has a distinct numerical contract, not an equivalent
 * replacement for the scalar-candidate recurrence. */
#ifndef SRC_BACKEND_CUDA_ATTENTION_BLOCK_H_INCLUDED
#define SRC_BACKEND_CUDA_ATTENTION_BLOCK_H_INCLUDED

/* CTA-local Q/K/V, probabilities and recurrence, not engine resident storage. */
#define YVEX_CUDA_ATTENTION_FUSED_SHARED_BYTES 91520u

typedef struct {
    const float *query, *local, *compressed, *sinks;
    const unsigned long long *local_positions, *compressed_positions;
    const unsigned long long *selected, *selected_count;
    unsigned long long initial_local_count, local_stride, compressed_stride;
    unsigned long long compressed_count, selected_stride, first_position;
    unsigned int tokens, heads, window, local_columns, columns, pitch;
    unsigned int ratio, attention_class;
    float *scores, *scales, *denominators, *output;
    unsigned short *probabilities;
    /* Equivalent phase-local BF16 views, never a persistent KV representation. */
    unsigned short *packed_query, *packed_kv;
    unsigned int *indices, *invalid_kv, prepared_rows;
    int *status;
} yvex_cuda_attention_block;

#endif
