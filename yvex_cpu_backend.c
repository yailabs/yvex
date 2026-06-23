/*
 * YVEX - CPU backend object
 *
 * File: yvex_cpu_backend.c
 * Layer: backend implementation
 *
 * Purpose:
 *   Opens the backend layer CPU reference backend and provides lifecycle, memory stats,
 *   sync, and capability entries for the internal backend vtable.
 *
 * Implements:
 *   - yvex_backend_open_cpu_impl
 *   - CPU close/stats/sync/capability vtable entries
 *
 * Invariants:
 *   - CPU backend is ready immediately after open
 *   - CPU sync is a no-op success
 *   - unsupported op capabilities remain false
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_cpu
 */
#include "yvex_backend_internal.h"

#include <stdlib.h>
#include <string.h>

int yvex_cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err);
void yvex_cpu_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor);
int yvex_cpu_tensor_write(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          const void *src,
                          unsigned long long len,
                          yvex_error *err);
int yvex_cpu_tensor_read(yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         void *dst,
                         unsigned long long len,
                         yvex_error *err);
int yvex_cpu_tensor_copy(yvex_backend *backend,
                         yvex_device_tensor *dst,
                         const yvex_device_tensor *src,
                         yvex_error *err);
int yvex_cpu_op_embed(yvex_backend *backend,
                      const yvex_device_tensor *embedding,
                      const unsigned int *token_ids,
                      unsigned long long token_count,
                      yvex_device_tensor *out,
                      yvex_error *err);

static int cpu_close(yvex_backend *backend, yvex_error *err)
{
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.close", "backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_memory_stats(const yvex_backend *backend,
                            yvex_backend_memory_stats *out,
                            yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.memory_stats", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = backend->stats;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_device_info(const yvex_backend *backend,
                           yvex_backend_device_info *out,
                           yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.device_info", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = backend->device_info;
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_sync(yvex_backend *backend, yvex_error *err)
{
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.sync", "backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

static int cpu_supports(const yvex_backend *backend, yvex_backend_capability capability)
{
    if (!backend) {
        return 0;
    }
    switch (capability) {
    case YVEX_BACKEND_CAP_TENSOR_ALLOC:
    case YVEX_BACKEND_CAP_TENSOR_READ_WRITE:
    case YVEX_BACKEND_CAP_OP_EMBED:
        return 1;
    case YVEX_BACKEND_CAP_OP_MATMUL:
    case YVEX_BACKEND_CAP_OP_RMS_NORM:
    case YVEX_BACKEND_CAP_OP_ATTENTION:
        return 0;
    }
    return 0;
}

static const yvex_backend_vtable cpu_vtable = {
    cpu_close,
    cpu_memory_stats,
    cpu_device_info,
    yvex_cpu_tensor_alloc,
    yvex_cpu_tensor_free,
    yvex_cpu_tensor_write,
    yvex_cpu_tensor_read,
    yvex_cpu_tensor_copy,
    cpu_sync,
    cpu_supports,
    yvex_cpu_op_embed,
};

int yvex_backend_open_cpu_impl(yvex_backend **out,
                               unsigned long long memory_limit_bytes,
                               yvex_error *err)
{
    yvex_backend *backend;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open_cpu_impl", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    backend = (yvex_backend *)calloc(1, sizeof(*backend));
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "yvex_backend_open_cpu_impl",
                       "failed to allocate CPU backend");
        return YVEX_ERR_NOMEM;
    }
    backend->kind = YVEX_BACKEND_KIND_CPU;
    backend->status = YVEX_BACKEND_STATUS_READY;
    backend->vtable = &cpu_vtable;
    backend->stats.memory_limit_bytes = memory_limit_bytes;
    backend->device_info.kind = YVEX_BACKEND_KIND_CPU;
    backend->device_info.name = "cpu";
    backend->device_info.device_index = 0;
    backend->tensor_id_next = 1;

    *out = backend;
    yvex_error_clear(err);
    return YVEX_OK;
}
