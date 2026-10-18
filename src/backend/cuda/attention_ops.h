/* Private CUDA attention primitive dispatch and phase-local work ownership. */
#ifndef SRC_BACKEND_CUDA_ATTENTION_OPS_H_INCLUDED
#define SRC_BACKEND_CUDA_ATTENTION_OPS_H_INCLUDED
#include "src/backend/cuda/private.h"

/* Bound ranking scratch independently of the admitted prompt width. */
#define YVEX_CUDA_ATTENTION_SELECTION_ROWS 32ull

/* Online64 consumes packed Q before publishing any attention coordinate.
 * Reference reduction and retained query evidence require separate storage. */
static inline int attention_query_reusable(yvex_backend_attention_compute compute,
                                          unsigned int evidence)
{
    return compute == YVEX_BACKEND_ATTENTION_COMPUTE_BF16_ONLINE64_F32_MMA_V1 &&
        evidence == 0u;
}

/* The 64-key launch and scratch pitch is topology, while exact visible rows
 * remain launch parameters. Capture and eager dispatch share this geometry. */
static inline int attention_block_geometry(
    const yvex_backend_attention_job *job, unsigned long long selected_stride,
    unsigned long long compressed_count, unsigned long long *local,
    unsigned long long *columns, unsigned long long *pitch)
{
    unsigned long long end;
    if (!job || !yvex_core_u64_add(job->token_position, job->token_count, &end)) return 0;
    *local = end < job->sliding_window ? end : job->sliding_window;
    *columns = job->attention_class == YVEX_BACKEND_ATTENTION_CSA
        ? selected_stride : compressed_count;
    if (!yvex_core_u64_add(*columns, *local, columns) ||
        !yvex_core_u64_add(*columns, 63ull, pitch)) return 0;
    *pitch &= ~63ull;
    return *pitch != 0ull;
}

/* Borrowed phase-private buffers, retained through stream completion. The
 * rolling state has one slot, or rows+1 slots when prefix checkpoints are
 * requested. No operation here publishes into committed session state. */
typedef struct {
    const yvex_backend_attention_rolling *rolling;
    const yvex_backend_attention_weight *weights;
    const CUdeviceptr *device_weights;
    CUdeviceptr input, kv, score, ape, state_kv, state_score, emissions, status;
    unsigned long long rows;
    int checkpoints;
} yvex_cuda_attention_rolling_phase;

typedef struct {
    CUdeviceptr query, weights, indexer, positions, selected, selected_positions;
    CUdeviceptr selected_count, valid_count, scores, indexes, status;
    unsigned long long candidates, candidate_capacity, score_grid, selected_stride;
    unsigned long long rows, heads, dimension, indexer_stride, ratio, first_position, k;
} yvex_cuda_attention_selection_phase;

typedef struct {
    const yvex_backend_attention_job *job;
    CUdeviceptr query, local, local_positions, compressed, compressed_positions;
    CUdeviceptr selected, selected_count, sinks, output, status;
    unsigned long long initial_local_count, compressed_count, selected_stride;
} yvex_cuda_attention_reduction_phase;

typedef struct {
    int (*fail)(yvex_backend_attention_failure *, yvex_backend_attention_failure_code,
                const char *, unsigned long long, unsigned long long, yvex_error *,
                yvex_status, const char *);
    int (*account_transfer)(unsigned long long, size_t, unsigned long long *,
                            const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*validate_job)(yvex_backend_attention_job *, yvex_backend_attention_output *,
                        yvex_backend_attention_failure *, yvex_error *);
    int (*validate_weight)(const yvex_backend_attention_weight *, unsigned long long, unsigned long long,
                           yvex_backend_attention_failure *, yvex_error *);
    int (*validate_activation)(const yvex_backend_attention_activation *, unsigned long long,
                               const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*validate_rolling)(const yvex_backend_attention_job *,
                            const yvex_backend_attention_rolling *, unsigned long long,
                            unsigned long long, int, unsigned long long *, const char *,
                            yvex_backend_attention_failure *, yvex_error *);
    int (*validate_alias)(const yvex_backend_attention_job *,
                          const yvex_cuda_attention_transfer *, size_t, unsigned long long,
                          unsigned long long, unsigned long long, unsigned long long,
                          unsigned long long);
    int (*cancel)(yvex_backend *, const yvex_backend_attention_job *,
                  const char *, int, yvex_backend_attention_failure *, yvex_error *);
    int (*stage_acquire)(yvex_backend *, size_t, int, int, unsigned char **, int *,
                         yvex_backend_attention_failure *, yvex_error *);
    int (*stage_layout)(unsigned char *, yvex_cuda_attention_upload *, size_t,
                        yvex_cuda_attention_transfer *, size_t,
                        unsigned long long, int **, unsigned long long **,
                        unsigned long long **, size_t *, size_t *);
    int (*allocate)(yvex_cuda_work *, CUdeviceptr *, size_t, const void *, int,
                    const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*initialize)(yvex_cuda_work *, CUdeviceptr, size_t, const void *, int,
                      const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*download)(yvex_cuda_work *, void *, CUdeviceptr, size_t, const char *,
                    yvex_backend_attention_failure *, yvex_error *);
    int (*launch)(yvex_cuda_work *, CUfunction, unsigned int, unsigned int, unsigned int,
                  void **, const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*round_bf16)(yvex_cuda_work *, CUdeviceptr, unsigned long long, CUdeviceptr,
                      const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*matvec)(yvex_cuda_work *, const yvex_backend_attention_weight *, CUdeviceptr,
                  unsigned long long, unsigned long long, unsigned long long, CUdeviceptr,
                  CUdeviceptr, int, CUdeviceptr, const char *,
                  yvex_backend_attention_failure *, yvex_error *);
    int (*matvec_grouped)(yvex_cuda_work *, const yvex_backend_attention_weight *, CUdeviceptr,
                  unsigned long long, unsigned long long, unsigned long long, CUdeviceptr,
                  unsigned long long, CUdeviceptr, unsigned long long, int, CUdeviceptr,
                  const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*decode)(yvex_cuda_work *, const yvex_backend_attention_weight *, CUdeviceptr,
                  unsigned long long, unsigned long long, CUdeviceptr, CUdeviceptr,
                  const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*rolling_phase)(yvex_cuda_work *, const yvex_cuda_attention_rolling_phase *,
                         const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*selection_phase)(yvex_cuda_work *, const yvex_cuda_attention_selection_phase *,
                           const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*reduction_phase)(yvex_cuda_work *, const yvex_cuda_attention_reduction_phase *,
                           const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*weighted_norm)(yvex_cuda_work *, CUdeviceptr, unsigned long long, unsigned long long,
                         const yvex_backend_attention_weight *, CUdeviceptr, double,
                         CUdeviceptr, const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*unit_norm)(yvex_cuda_work *, CUdeviceptr, unsigned long long, unsigned long long,
                     double, CUdeviceptr, const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*rope)(yvex_cuda_work *, CUdeviceptr, unsigned long long, unsigned long long,
                unsigned long long, unsigned long long, unsigned long long,
                const yvex_backend_attention_position *, int, CUdeviceptr, const char *,
                yvex_backend_attention_failure *, yvex_error *);
    int (*activation)(yvex_cuda_work *, CUdeviceptr, unsigned long long, unsigned long long,
                      unsigned long long,
                      const yvex_backend_attention_activation *, CUdeviceptr,
                      const char *, yvex_backend_attention_failure *, yvex_error *);
    int (*state_stage)(yvex_backend *, const yvex_backend_attention_job *,
                       const yvex_cuda_attention_state_sources *, size_t *, int *, yvex_error *);
} yvex_cuda_attention_operations;
const yvex_cuda_attention_operations *yvex_cuda_attention_operations_get(void);

#endif
