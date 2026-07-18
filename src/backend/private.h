/*
 * Owner: backend.private (backend).
 * Owns: the private-interface boundary consumed by graph,runtime.
 * Does not own: unrelated subsystem policy or unsupported higher-stage claims.
 * Invariants: scope=generic and visibility=private match config/source_owners.tsv.
 * Boundary: private-interface; moving this contract requires an ownership-manifest change.
 *
 * private.h - Backend implementation boundary.
 *
 * This header is private to YVEX backend implementations and CUDA glue.
 */
#ifndef YVEX_BACKEND_PRIVATE_H
#define YVEX_BACKEND_PRIVATE_H

#include <yvex/api.h>

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

char *yvex_backend_strdup(const char *text);
int yvex_backend_desc_is_valid(const yvex_backend_tensor_desc *desc, yvex_error *err);
int yvex_backend_tensor_same_shape(const yvex_device_tensor *a,
                                   const yvex_device_tensor *b);
int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor);

int yvex_backend_open_cpu_impl(yvex_backend **out,
                               unsigned long long memory_limit_bytes,
                               yvex_error *err);
int yvex_backend_open_cuda_impl(yvex_backend **out,
                                const char *device,
                                unsigned long long memory_limit_bytes,
                                yvex_error *err);

#endif /* YVEX_BACKEND_PRIVATE_H */
