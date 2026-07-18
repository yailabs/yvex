/*
 * cuda_deepseek_attention.h - private DeepSeek attention CUDA contract.
 *
 * Owner:
 *   src/backend/cuda
 *
 * Owns:
 *   immutable host/device job facts for one complete DeepSeek attention token,
 *   direct encoded-weight CUDA execution, bounded device memory accounting,
 *   and transactional host result publication.
 *
 * Does not own:
 *   source or artifact IO, tensor-role discovery, architecture policy,
 *   persistent KV ownership, graph identities, prefill, decode, generation,
 *   CLI output, or reference arithmetic.
 *
 * Invariants:
 *   every numerical stage executes in the admitted generated PTX bundle;
 *   output buffers are published only after launch, synchronization, status,
 *   and copy-back succeed. Host pointers are borrowed for the call only.
 *
 * Boundary:
 *   one attention-token kernel graph is not persistent KV or generation.
 */
#ifndef YVEX_CUDA_DEEPSEEK_ATTENTION_H
#define YVEX_CUDA_DEEPSEEK_ATTENTION_H

#include <stddef.h>
#include <yvex/backend.h>
#include <yvex/error.h>

typedef enum {
    YVEX_CUDA_DEEPSEEK_WEIGHT_Q_A = 0,
    YVEX_CUDA_DEEPSEEK_WEIGHT_Q_A_NORM,
    YVEX_CUDA_DEEPSEEK_WEIGHT_Q_B,
    YVEX_CUDA_DEEPSEEK_WEIGHT_KV,
    YVEX_CUDA_DEEPSEEK_WEIGHT_KV_NORM,
    YVEX_CUDA_DEEPSEEK_WEIGHT_SINKS,
    YVEX_CUDA_DEEPSEEK_WEIGHT_OUT_A,
    YVEX_CUDA_DEEPSEEK_WEIGHT_OUT_B,
    YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_KV,
    YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_GATE,
    YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_APE,
    YVEX_CUDA_DEEPSEEK_WEIGHT_MAIN_NORM,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_KV,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_GATE,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_APE,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_NORM,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_Q,
    YVEX_CUDA_DEEPSEEK_WEIGHT_INDEX_PROJECTION,
    YVEX_CUDA_DEEPSEEK_WEIGHT_COUNT
} yvex_cuda_deepseek_weight_slot;

typedef struct {
    const unsigned char *encoded;
    size_t encoded_bytes;
    unsigned long long row_bytes;
    unsigned long long row_width;
    unsigned long long row_count;
    unsigned int qtype;
    int present;
} yvex_cuda_deepseek_weight;

typedef struct {
    unsigned long long theta;
    unsigned long long scaling_factor;
    unsigned long long original_context;
    unsigned long long beta_fast;
    unsigned long long beta_slow;
    unsigned long long rope_dimensions;
} yvex_cuda_deepseek_position;

typedef struct {
    int required;
    unsigned long long block_width;
    unsigned int quantization;
    int hadamard;
} yvex_cuda_deepseek_activation;

typedef struct {
    int present;
    unsigned long long ratio;
    unsigned long long head_dimension;
    unsigned long long state_width;
    unsigned long long state_slots;
    unsigned long long cursor;
    unsigned long long previous_fill;
    unsigned long long current_fill;
    const float *kv_state;
    const float *score_state;
    int overlap;
} yvex_cuda_deepseek_rolling;

typedef struct {
    unsigned int attention_class;
    unsigned long long token_position;
    unsigned long long hidden_width;
    unsigned long long q_rank;
    unsigned long long query_heads;
    unsigned long long head_dimension;
    unsigned long long kv_width;
    unsigned long long sliding_window;
    unsigned long long compression_ratio;
    unsigned long long output_groups;
    unsigned long long output_group_input_width;
    unsigned long long output_rank;
    unsigned long long indexer_heads;
    unsigned long long indexer_head_dimension;
    unsigned long long indexer_topk;
    double rms_epsilon;
    yvex_cuda_deepseek_position position;
    yvex_cuda_deepseek_activation attention_kv_activation;
    yvex_cuda_deepseek_activation compressor_activation;
    yvex_cuda_deepseek_activation compressor_rotated_activation;
    yvex_cuda_deepseek_activation indexer_query_activation;
    yvex_cuda_deepseek_weight weights[YVEX_CUDA_DEEPSEEK_WEIGHT_COUNT];
    const float *input;
    const float *local_kv;
    const unsigned long long *local_positions;
    unsigned long long local_count;
    unsigned long long local_stride;
    const float *compressed_kv;
    const unsigned long long *compressed_positions;
    unsigned long long compressed_count;
    unsigned long long compressed_stride;
    const float *indexer_kv;
    const unsigned long long *indexer_positions;
    unsigned long long indexer_count;
    unsigned long long indexer_stride;
    yvex_cuda_deepseek_rolling main_rolling;
    yvex_cuda_deepseek_rolling indexer_rolling;
    unsigned long long max_device_bytes;
} yvex_cuda_deepseek_attention_job;

typedef struct {
    float *q_low;
    float *query;
    float *raw_kv;
    float *compressed_kv;
    float *indexer_kv;
    float *index_query;
    float *index_weights;
    float *attention_values;
    float *output;
    unsigned long long *compressed_positions;
    unsigned long long *indexer_positions;
    unsigned long long *topk_positions;
    float *main_kv_state;
    float *main_score_state;
    float *indexer_kv_state;
    float *indexer_score_state;
    unsigned long long compressed_count;
    unsigned long long indexer_count;
    unsigned long long topk_count;
    unsigned long long valid_candidate_count;
    unsigned long long device_bytes;
    unsigned long long peak_device_bytes;
    unsigned long long kernel_launches;
} yvex_cuda_deepseek_attention_output;

typedef enum {
    YVEX_CUDA_DEEPSEEK_FAILURE_NONE = 0,
    YVEX_CUDA_DEEPSEEK_FAILURE_INVALID_ARGUMENT,
    YVEX_CUDA_DEEPSEEK_FAILURE_CAPABILITY,
    YVEX_CUDA_DEEPSEEK_FAILURE_BUDGET,
    YVEX_CUDA_DEEPSEEK_FAILURE_ALLOCATION,
    YVEX_CUDA_DEEPSEEK_FAILURE_COPY,
    YVEX_CUDA_DEEPSEEK_FAILURE_LAUNCH,
    YVEX_CUDA_DEEPSEEK_FAILURE_SYNCHRONIZE,
    YVEX_CUDA_DEEPSEEK_FAILURE_NUMERIC,
    YVEX_CUDA_DEEPSEEK_FAILURE_CLEANUP
} yvex_cuda_deepseek_failure_code;

typedef struct {
    yvex_cuda_deepseek_failure_code code;
    const char *stage;
    unsigned long long expected;
    unsigned long long actual;
} yvex_cuda_deepseek_failure;

int yvex_cuda_deepseek_attention_execute(
    yvex_backend *backend,
    const yvex_cuda_deepseek_attention_job *job,
    yvex_cuda_deepseek_attention_output *output,
    yvex_cuda_deepseek_failure *failure,
    yvex_error *err);

#endif
