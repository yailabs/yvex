/*
 * YVEX - Backend ABI wrappers
 *
 * File: src/backend/backend.c
 * Layer: backend implementation
 *
 * Purpose:
 *   Implements public backend wrappers, enum names, unsupported backend
 *   handling, and common tensor accessors. Concrete backends plug into the
 *   private vtable in backend_internal.h.
 *
 * Implements:
 *   - yvex_backend_open
 *   - yvex_backend_close
 *   - backend name/status/capability helpers
 *   - public vtable dispatch wrappers
 *
 * Invariants:
 *   - non-CPU backends are unsupported in G0
 *   - public wrappers validate arguments before dispatch
 *   - library code does not print
 *
 * Commands:
 *   - make test-core
 *   - build/tests/test_backend_cpu
 */
#include "backend_internal.h"

#include <stdlib.h>
#include <string.h>

char *yvex_backend_strdup(const char *text)
{
    size_t len;
    char *copy;

    if (!text) {
        text = "";
    }
    len = strlen(text);
    copy = (char *)malloc(len + 1u);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, text, len + 1u);
    return copy;
}

const char *yvex_backend_kind_name(yvex_backend_kind kind)
{
    switch (kind) {
    case YVEX_BACKEND_KIND_CPU: return "cpu";
    case YVEX_BACKEND_KIND_CUDA: return "cuda";
    case YVEX_BACKEND_KIND_METAL: return "metal";
    case YVEX_BACKEND_KIND_ROCM: return "rocm";
    }
    return "unknown";
}

const char *yvex_backend_status_name(yvex_backend_status status)
{
    switch (status) {
    case YVEX_BACKEND_STATUS_READY: return "ready";
    case YVEX_BACKEND_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_BACKEND_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_backend_capability_name(yvex_backend_capability capability)
{
    switch (capability) {
    case YVEX_BACKEND_CAP_TENSOR_ALLOC: return "tensor_alloc";
    case YVEX_BACKEND_CAP_TENSOR_READ_WRITE: return "tensor_read_write";
    case YVEX_BACKEND_CAP_OP_EMBED: return "op_embed";
    case YVEX_BACKEND_CAP_OP_MATMUL: return "op_matmul";
    case YVEX_BACKEND_CAP_OP_RMS_NORM: return "op_rms_norm";
    case YVEX_BACKEND_CAP_OP_ATTENTION: return "op_attention";
    }
    return "unknown";
}

int yvex_backend_open(yvex_backend **out,
                      const yvex_backend_options *options,
                      yvex_error *err)
{
    yvex_backend_kind kind = YVEX_BACKEND_KIND_CPU;
    unsigned long long memory_limit_bytes = 0;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_open", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    if (options) {
        kind = options->kind;
        memory_limit_bytes = options->memory_limit_bytes;
        (void)options->device;
    }

    if (kind == YVEX_BACKEND_KIND_CPU) {
        return yvex_backend_open_cpu_impl(out, memory_limit_bytes, err);
    }

    yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_open",
                    "backend %s is not implemented in G0; planned for a later backend wave",
                    yvex_backend_kind_name(kind));
    return YVEX_ERR_UNSUPPORTED;
}

int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err)
{
    yvex_backend_options options;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CPU;
    return yvex_backend_open(out, &options, err);
}

void yvex_backend_close(yvex_backend *backend)
{
    yvex_error err;

    if (!backend) {
        return;
    }
    yvex_error_clear(&err);
    if (backend->vtable && backend->vtable->close) {
        (void)backend->vtable->close(backend, &err);
    }
    free(backend);
}

yvex_backend_kind yvex_backend_kind_of(const yvex_backend *backend)
{
    return backend ? backend->kind : YVEX_BACKEND_KIND_CPU;
}

yvex_backend_status yvex_backend_status_of(const yvex_backend *backend)
{
    return backend ? backend->status : YVEX_BACKEND_STATUS_FAILED;
}

int yvex_backend_get_memory_stats(const yvex_backend *backend,
                                  yvex_backend_memory_stats *out,
                                  yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_get_memory_stats",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->memory_stats) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_get_memory_stats",
                       "backend does not provide memory stats");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->memory_stats(backend, out, err);
}

int yvex_backend_tensor_alloc(yvex_backend *backend,
                              const yvex_backend_tensor_desc *desc,
                              yvex_device_tensor **out,
                              yvex_error *err)
{
    int rc;

    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "out is required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = yvex_backend_desc_is_valid(desc, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!backend->vtable || !backend->vtable->tensor_alloc) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_tensor_alloc",
                       "backend does not support tensor allocation");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->tensor_alloc(backend, desc, out, err);
}

void yvex_backend_tensor_free(yvex_backend *backend,
                              yvex_device_tensor *tensor)
{
    if (!backend || !tensor || !backend->vtable || !backend->vtable->tensor_free) {
        return;
    }
    backend->vtable->tensor_free(backend, tensor);
}

const char *yvex_device_tensor_name(const yvex_device_tensor *tensor)
{
    return tensor && tensor->name ? tensor->name : "";
}

yvex_dtype yvex_device_tensor_dtype(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->dtype : YVEX_DTYPE_UNKNOWN;
}

unsigned int yvex_device_tensor_rank(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->rank : 0;
}

const unsigned long long *yvex_device_tensor_dims(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->dims : NULL;
}

unsigned long long yvex_device_tensor_bytes(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->bytes : 0;
}

int yvex_backend_tensor_write(yvex_backend *backend,
                              yvex_device_tensor *tensor,
                              const void *src,
                              unsigned long long len,
                              yvex_error *err)
{
    if (!backend || !tensor || !src) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_write",
                       "backend, tensor, and src are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->tensor_write) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_tensor_write",
                       "backend does not support tensor writes");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->tensor_write(backend, tensor, src, len, err);
}

int yvex_backend_tensor_read(yvex_backend *backend,
                             const yvex_device_tensor *tensor,
                             void *dst,
                             unsigned long long len,
                             yvex_error *err)
{
    if (!backend || !tensor || !dst) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_read",
                       "backend, tensor, and dst are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->tensor_read) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_tensor_read",
                       "backend does not support tensor reads");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->tensor_read(backend, tensor, dst, len, err);
}

int yvex_backend_tensor_copy(yvex_backend *backend,
                             yvex_device_tensor *dst,
                             const yvex_device_tensor *src,
                             yvex_error *err)
{
    if (!backend || !dst || !src) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_copy",
                       "backend, dst, and src are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->tensor_copy) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_tensor_copy",
                       "backend does not support tensor copy");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->tensor_copy(backend, dst, src, err);
}

int yvex_backend_sync(yvex_backend *backend, yvex_error *err)
{
    if (!backend) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_sync", "backend is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->sync) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_sync",
                       "backend does not support sync");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->sync(backend, err);
}

int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability)
{
    if (!backend || !backend->vtable || !backend->vtable->supports) {
        return 0;
    }
    return backend->vtable->supports(backend, capability);
}

int yvex_backend_op_embed(yvex_backend *backend,
                          const yvex_device_tensor *embedding,
                          const unsigned int *token_ids,
                          unsigned long long token_count,
                          yvex_device_tensor *out,
                          yvex_error *err)
{
    if (!backend || !embedding || !token_ids || !out || token_count == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_embed",
                       "backend, embedding, token ids, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->op_embed) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "backend does not support embed op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_embed(backend, embedding, token_ids, token_count, out, err);
}
