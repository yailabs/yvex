/*
 * YVEX - Backend ABI
 *
 * File: include/yvex/backend.h
 * Layer: public backend API
 *
 * Purpose:
 *   Defines the first YVEX backend abstraction and CPU reference backend
 *   surface. backend layer proves backend lifecycle, tensor allocation/read/write/copy,
 *   memory stats, capability reporting, and a minimal embed op.
 *
 * Owns:
 *   - yvex_backend
 *   - yvex_device_tensor
 *   - backend lifecycle and memory stats
 *   - backend tensor allocation/read/write/copy
 *   - backend capability reporting
 *
 * Does not own:
 *   - sessions
 *   - sampler/logits/KV runtime
 *   - inference
 *
 * Used by:
 *   - planner
 *   - CLI backend/plan commands
 *   - backend tests
 *
 * Validation:
 *   - make test-core
 *   - build/tests/test_backend_cpu
 *   - build/tests/test_backend_ops
 */
#ifndef YVEX_BACKEND_H
#define YVEX_BACKEND_H

#include <yvex/tensor.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yvex_backend yvex_backend;
typedef struct yvex_device_tensor yvex_device_tensor;

typedef enum {
    YVEX_BACKEND_KIND_CPU = 0,
    YVEX_BACKEND_KIND_CUDA,
    YVEX_BACKEND_KIND_METAL,
    YVEX_BACKEND_KIND_ROCM
} yvex_backend_kind;

typedef enum {
    YVEX_BACKEND_STATUS_READY = 0,
    YVEX_BACKEND_STATUS_UNSUPPORTED,
    YVEX_BACKEND_STATUS_FAILED
} yvex_backend_status;

typedef struct {
    yvex_backend_kind kind;
    const char *device;
    unsigned long long memory_limit_bytes;
} yvex_backend_options;

typedef struct {
    unsigned long long allocated_bytes;
    unsigned long long allocation_count;
    unsigned long long peak_allocated_bytes;
    unsigned long long memory_limit_bytes;
} yvex_backend_memory_stats;

typedef struct {
    const char *name;
    yvex_dtype dtype;
    unsigned int rank;
    unsigned long long dims[YVEX_TENSOR_MAX_DIMS];
    unsigned long long bytes;
} yvex_backend_tensor_desc;

typedef struct {
    yvex_backend_kind kind;
    const char *name;
    int device_index;
    int compute_capability_major;
    int compute_capability_minor;
    unsigned long long global_memory_bytes;
    unsigned long long free_memory_bytes;
    unsigned long long total_memory_bytes;
    int unified_addressing;
    int managed_memory;
} yvex_backend_device_info;

typedef enum {
    YVEX_BACKEND_CAP_TENSOR_ALLOC = 0,
    YVEX_BACKEND_CAP_TENSOR_READ_WRITE,
    YVEX_BACKEND_CAP_OP_EMBED,
    YVEX_BACKEND_CAP_OP_MATMUL,
    YVEX_BACKEND_CAP_OP_RMS_NORM,
    YVEX_BACKEND_CAP_OP_ATTENTION
} yvex_backend_capability;

int yvex_backend_open(yvex_backend **out,
                      const yvex_backend_options *options,
                      yvex_error *err);
int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err);
int yvex_backend_cuda_available(void);
void yvex_backend_close(yvex_backend *backend);

yvex_backend_kind yvex_backend_kind_of(const yvex_backend *backend);
const char *yvex_backend_kind_name(yvex_backend_kind kind);
yvex_backend_status yvex_backend_status_of(const yvex_backend *backend);
const char *yvex_backend_status_name(yvex_backend_status status);

int yvex_backend_get_memory_stats(const yvex_backend *backend,
                                  yvex_backend_memory_stats *out,
                                  yvex_error *err);
int yvex_backend_get_device_info(const yvex_backend *backend,
                                 yvex_backend_device_info *out,
                                 yvex_error *err);

int yvex_backend_tensor_alloc(yvex_backend *backend,
                              const yvex_backend_tensor_desc *desc,
                              yvex_device_tensor **out,
                              yvex_error *err);
void yvex_backend_tensor_free(yvex_backend *backend,
                              yvex_device_tensor *tensor);

const char *yvex_device_tensor_name(const yvex_device_tensor *tensor);
yvex_dtype yvex_device_tensor_dtype(const yvex_device_tensor *tensor);
unsigned int yvex_device_tensor_rank(const yvex_device_tensor *tensor);
const unsigned long long *yvex_device_tensor_dims(const yvex_device_tensor *tensor);
unsigned long long yvex_device_tensor_bytes(const yvex_device_tensor *tensor);

int yvex_backend_tensor_write(yvex_backend *backend,
                              yvex_device_tensor *tensor,
                              const void *src,
                              unsigned long long len,
                              yvex_error *err);
int yvex_backend_tensor_read(yvex_backend *backend,
                             const yvex_device_tensor *tensor,
                             void *dst,
                             unsigned long long len,
                             yvex_error *err);
int yvex_backend_tensor_copy(yvex_backend *backend,
                             yvex_device_tensor *dst,
                             const yvex_device_tensor *src,
                             yvex_error *err);
int yvex_backend_sync(yvex_backend *backend, yvex_error *err);

int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability);
const char *yvex_backend_capability_name(yvex_backend_capability capability);

int yvex_backend_op_embed(yvex_backend *backend,
                          const yvex_device_tensor *embedding,
                          const unsigned int *token_ids,
                          unsigned long long token_count,
                          yvex_device_tensor *out,
                          yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* YVEX_BACKEND_H */
