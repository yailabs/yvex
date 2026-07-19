/* Owner: backend.internal (backend).
 * Owns: backend resource state and typed capability reports.
 * Does not own: graph policy, model topology, or CLI rendering.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: backend-private lifecycle and projection.
 * Purpose: provide the canonical backend-private lifecycle and projection contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef INCLUDE_YVEX_INTERNAL_BACKEND_H_INCLUDED
#define INCLUDE_YVEX_INTERNAL_BACKEND_H_INCLUDED

#include <yvex/backend.h>
#include <yvex/core.h>
#include <yvex/internal/core.h>
#include <yvex/model.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private contract. */
typedef struct yvex_backend_vtable {
    int (*close)(yvex_backend *backend, yvex_error *err);
    int (*memory_stats)(const yvex_backend *backend,
                        yvex_backend_memory_stats *out,
                        yvex_error *err);
    int (*device_info)(const yvex_backend *backend,
                       yvex_backend_device_info *out,
                       yvex_error *err);
    int (*tensor_alloc)(yvex_backend *backend,
                        const yvex_backend_tensor_desc *desc,
                        yvex_device_tensor **out,
                        yvex_error *err);
    int (*tensor_free)(yvex_backend *backend,
                       yvex_device_tensor *tensor,
                       yvex_error *err);
    int (*tensor_write)(yvex_backend *backend,
                        yvex_device_tensor *tensor,
                        const void *src,
                        unsigned long long len,
                        yvex_error *err);
    int (*tensor_read)(yvex_backend *backend,
                       const yvex_device_tensor *tensor,
                       void *dst,
                       unsigned long long len,
                       yvex_error *err);
    int (*tensor_copy)(yvex_backend *backend,
                       yvex_device_tensor *dst,
                       const yvex_device_tensor *src,
                       yvex_error *err);
    int (*sync)(yvex_backend *backend, yvex_error *err);
    int (*query_capability)(const yvex_backend *backend,
                            yvex_backend_operation_variant variant,
                            yvex_backend_capability_result *out,
                            yvex_error *err);
    int (*op_embed)(yvex_backend *backend,
                    const yvex_device_tensor *embedding,
                    const unsigned int *token_ids,
                    unsigned long long token_count,
                    yvex_device_tensor *out,
                    yvex_error *err);
    int (*op_rms_norm)(yvex_backend *backend,
                       const yvex_device_tensor *input,
                       const yvex_device_tensor *weight,
                       float epsilon,
                       yvex_device_tensor *out,
                       yvex_error *err);
    int (*op_rope)(yvex_backend *backend,
                   const yvex_device_tensor *input,
                   unsigned long long position,
                   float rope_base,
                   yvex_device_tensor *out,
                   yvex_error *err);
    int (*op_matmul)(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     const yvex_device_tensor *weight,
                     yvex_device_tensor *out,
                     yvex_error *err);
    int (*op_mlp)(yvex_backend *backend,
                  const yvex_device_tensor *input,
                  const yvex_device_tensor *gate_weight,
                  const yvex_device_tensor *up_weight,
                  const yvex_device_tensor *down_weight,
                  const yvex_mlp_options *options,
                  yvex_device_tensor *intermediate,
                  yvex_device_tensor *out,
                  yvex_error *err);
    int (*op_attention)(yvex_backend *backend,
                        const yvex_device_tensor *query,
                        const yvex_device_tensor *keys,
                        const yvex_device_tensor *values,
                        unsigned long long seq_len,
                        unsigned long long position,
                        float scale,
                        int causal,
                        yvex_device_tensor *score_scratch,
                        yvex_device_tensor *probability_scratch,
                        yvex_device_tensor *out,
                        yvex_error *err);
} yvex_backend_vtable;
struct yvex_backend {
    yvex_backend_kind kind;
    yvex_backend_status status;
    const yvex_backend_vtable *vtable;
    yvex_backend_memory_stats stats;
    yvex_backend_device_info device_info;
    char device_name_storage[128];
    void *impl;
    unsigned long long tensor_id_next;
};
struct yvex_device_tensor {
    yvex_backend *owner;
    unsigned long long owner_id;
    char *name;
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long bytes;
    unsigned char *data;
    int is_written;
};
double yvex_backend_nth_root(double value, unsigned long long degree);
int yvex_backend_tensor_f32_elements(const yvex_device_tensor *tensor,
                                     unsigned long long elements);
int yvex_backend_memory_can_add(const yvex_backend *backend,
                                unsigned long long bytes,
                                const char *backend_name,
                                const char *where,
                                yvex_error *err);
void yvex_backend_memory_acquire(yvex_backend *backend, unsigned long long bytes);
void yvex_backend_memory_release(yvex_backend *backend, unsigned long long bytes);
int yvex_backend_tensor_rw_validate(const char *where,
                                    const yvex_backend *backend,
                                    const yvex_device_tensor *tensor,
                                    unsigned long long len,
                                    yvex_error *err);
int yvex_backend_tensor_copy_validate(const yvex_backend *backend,
                                      const yvex_device_tensor *dst,
                                      const yvex_device_tensor *src,
                                      const char *where,
                                      yvex_error *err);
int yvex_backend_tensor_same_shape(const yvex_device_tensor *a,
                                   const yvex_device_tensor *b);
int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor);
int yvex_backend_validate_rope(const yvex_device_tensor *tensor,
                               unsigned long long *head_dim,
                               const char *where,
                               yvex_error *err);
int yvex_backend_validate_embed(const yvex_backend *backend,
                                const yvex_device_tensor *embedding,
                                const unsigned int *token_ids,
                                unsigned long long token_count,
                                const yvex_device_tensor *out,
                                unsigned long long *hidden_size,
                                unsigned long long *vocab_size,
                                const char *unsupported_message,
                                const char *where,
                                yvex_error *err);
int yvex_backend_validate_rms_norm(const yvex_backend *backend,
                                   const yvex_device_tensor *input,
                                   const yvex_device_tensor *weight,
                                   float epsilon,
                                   const yvex_device_tensor *out,
                                   unsigned long long *hidden_size,
                                   const char *unsupported_message,
                                   const char *where,
                                   yvex_error *err);
int yvex_backend_validate_matmul(const yvex_backend *backend,
                                 const yvex_device_tensor *input,
                                 const yvex_device_tensor *weight,
                                 const yvex_device_tensor *out,
                                 unsigned long long *m,
                                 unsigned long long *k,
                                 unsigned long long *n,
                                 const char *where,
                                 yvex_error *err);
int yvex_backend_validate_attention(const yvex_backend *backend,
                                    const yvex_device_tensor *query,
                                    const yvex_device_tensor *keys,
                                    const yvex_device_tensor *values,
                                    unsigned long long seq_len,
                                    unsigned long long position,
                                    yvex_device_tensor *score_scratch,
                                    yvex_device_tensor *probability_scratch,
                                    yvex_device_tensor *out,
                                    unsigned long long *head_dim,
                                    unsigned long long *kv_elements,
                                    const char *where,
                                    yvex_error *err);
int yvex_backend_validate_mlp(const yvex_backend *backend,
                              const yvex_device_tensor *input,
                              const yvex_device_tensor *gate_weight,
                              const yvex_device_tensor *up_weight,
                              const yvex_device_tensor *down_weight,
                              const yvex_mlp_options *options,
                              const yvex_device_tensor *intermediate,
                              const yvex_device_tensor *out,
                              unsigned long long *batch,
                              unsigned long long *hidden_dim,
                              unsigned long long *ffn_dim,
                              unsigned long long *gate_offset,
                              unsigned long long *up_offset,
                              unsigned long long *down_offset,
                              const char *where,
                              yvex_error *err);
int yvex_backend_open_cpu_impl(yvex_backend **out,
                               unsigned long long memory_limit_bytes,
                               yvex_error *err);
int yvex_backend_open_cuda_impl(yvex_backend **out,
                                const char *device,
                                unsigned long long memory_limit_bytes,
                                yvex_error *err);

/* Report contract. */
typedef enum {
    YVEX_BACKEND_REPORT_CAPABILITIES = 0,
    YVEX_BACKEND_REPORT_CUDA_INFO
} yvex_backend_report_kind;
typedef enum {
    YVEX_BACKEND_BUNDLE_NOT_APPLICABLE = 0,
    YVEX_BACKEND_BUNDLE_ABSENT,
    YVEX_BACKEND_BUNDLE_ADMITTED,
    YVEX_BACKEND_BUNDLE_REJECTED
} yvex_backend_bundle_admission;
typedef struct {
    yvex_backend_report_kind kind;
    yvex_backend_kind backend_kind;
} yvex_backend_report_request;
typedef struct {
    yvex_backend_report_kind kind;
    yvex_backend_kind backend_kind;
    yvex_backend_status backend_status;
    int exit_code;
    int available;
    int has_device_info;
    yvex_backend_device_info device_info;
    char device_name[128];
    yvex_backend_memory_stats memory;
    int capabilities[YVEX_BACKEND_CAP_OP_ATTENTION + 1];
    yvex_backend_capability_result variants[YVEX_BACKEND_VARIANT_COUNT];
    unsigned int variant_count;
    int context_available;
    yvex_backend_bundle_admission bundle_admission;
    yvex_backend_capability_reason bundle_reason;
    char reason[256];
} yvex_backend_report;
typedef struct {
    const char *kind;
    const char *status;
    const char *reason;
    const char *next_row;
} yvex_backend_report_fact;
int yvex_backend_report_build(const yvex_backend_report_request *request,
                              yvex_backend_report *report,
                              yvex_error *err);
const char *yvex_backend_bundle_admission_name(
    yvex_backend_bundle_admission admission);

#ifdef __cplusplus
}
#endif

#endif /* INCLUDE_YVEX_INTERNAL_BACKEND_H_INCLUDED */
