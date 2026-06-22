/*
 * YVEX - Backend internals
 *
 * File: src/backend/backend_internal.h
 * Layer: backend implementation
 *
 * Purpose:
 *   Shares private backend ABI structures between generic wrappers and the CPU
 *   reference backend. Public headers expose only opaque backend/tensor handles.
 *
 * Owns:
 *   - yvex_backend_vtable
 *   - concrete yvex_backend storage
 *   - concrete yvex_device_tensor storage
 *
 * Does not own:
 *   - public backend API declarations
 *   - session/runtime state
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_cpu
 */
#ifndef YVEX_BACKEND_INTERNAL_H
#define YVEX_BACKEND_INTERNAL_H

#include <yvex/backend.h>

typedef struct yvex_backend_vtable {
    int (*close)(yvex_backend *backend, yvex_error *err);
    int (*memory_stats)(const yvex_backend *backend,
                        yvex_backend_memory_stats *out,
                        yvex_error *err);
    int (*tensor_alloc)(yvex_backend *backend,
                        const yvex_backend_tensor_desc *desc,
                        yvex_device_tensor **out,
                        yvex_error *err);
    void (*tensor_free)(yvex_backend *backend, yvex_device_tensor *tensor);
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
    int (*supports)(const yvex_backend *backend, yvex_backend_capability capability);
    int (*op_embed)(yvex_backend *backend,
                    const yvex_device_tensor *embedding,
                    const unsigned int *token_ids,
                    unsigned long long token_count,
                    yvex_device_tensor *out,
                    yvex_error *err);
} yvex_backend_vtable;

struct yvex_backend {
    yvex_backend_kind kind;
    yvex_backend_status status;
    const yvex_backend_vtable *vtable;
    yvex_backend_memory_stats stats;
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

#endif /* YVEX_BACKEND_INTERNAL_H */
