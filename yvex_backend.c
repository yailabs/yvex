/*
 * YVEX - compressed implementation unit
 *
 * This file groups related implementation sections that used to live in
 * smaller root source fragments. Public API declarations remain under
 * include/yvex/.
 */


/* ===== yvex_backend.c ===== */

#include "yvex_backend_internal.h"

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
    if (kind == YVEX_BACKEND_KIND_CUDA) {
        return yvex_backend_open_cuda_impl(out, options ? options->device : NULL,
                                           memory_limit_bytes, err);
    }

    yvex_error_setf(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_open",
                    "backend %s is not implemented",
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

int yvex_backend_cuda_available(void)
{
    yvex_backend *backend = NULL;
    yvex_backend_options options;
    yvex_error err;
    int rc;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CUDA;
    yvex_error_clear(&err);
    rc = yvex_backend_open(&backend, &options, &err);
    yvex_backend_close(backend);
    return rc == YVEX_OK;
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

int yvex_backend_get_device_info(const yvex_backend *backend,
                                 yvex_backend_device_info *out,
                                 yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_get_device_info",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->device_info) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_get_device_info",
                       "backend does not provide device info");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->device_info(backend, out, err);
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

/* ===== yvex_cpu_backend.c ===== */

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

/* ===== yvex_cpu_ops.c ===== */

#include "yvex_backend_internal.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

static int tensor_is_f32_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / sizeof(float)) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

int yvex_cpu_op_embed(yvex_backend *backend,
                      const yvex_device_tensor *embedding,
                      const unsigned int *token_ids,
                      unsigned long long token_count,
                      yvex_device_tensor *out,
                      yvex_error *err)
{
    unsigned long long hidden_size;
    unsigned long long vocab_size;
    unsigned long long e;
    unsigned long long t;
    const float *embedding_data;
    float *out_data;

    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (embedding->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "backend layer CPU embed supports F32 tensors only");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (embedding->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_embed",
                       "embedding tensor must have rank 2");
        return YVEX_ERR_FORMAT;
    }
    hidden_size = embedding->dims[0];
    vocab_size = embedding->dims[1];
    if (hidden_size == 0 || vocab_size == 0 || hidden_size > ULLONG_MAX / vocab_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding dimensions overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!tensor_is_f32_bytes(embedding, hidden_size * vocab_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    if (token_count > ULLONG_MAX / hidden_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output dimensions overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (out->rank != 2 || out->dims[0] != token_count || out->dims[1] != hidden_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output tensor must have dims [token_count, hidden_size]");
        return YVEX_ERR_BOUNDS;
    }
    if (!tensor_is_f32_bytes(out, token_count * hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "output tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }

    embedding_data = (const float *)embedding->data;
    out_data = (float *)out->data;
    for (t = 0; t < token_count; ++t) {
        unsigned int token_id = token_ids[t];
        if ((unsigned long long)token_id >= vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                            "token id %u exceeds embedding vocab size %llu",
                            token_id, vocab_size);
            return YVEX_ERR_BOUNDS;
        }
        for (e = 0; e < hidden_size; ++e) {
            out_data[(t * hidden_size) + e] =
                embedding_data[((unsigned long long)token_id * hidden_size) + e];
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* ===== yvex_cpu_tensor.c ===== */

#include "yvex_backend_internal.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int yvex_backend_desc_is_valid(const yvex_backend_tensor_desc *desc, yvex_error *err)
{
    unsigned int i;

    if (!desc) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "descriptor is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->rank == 0 || desc->rank > YVEX_TENSOR_MAX_DIMS) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "rank is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->bytes == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc", "bytes must be non-zero");
        return YVEX_ERR_INVALID_ARG;
    }
    if (desc->bytes > (unsigned long long)SIZE_MAX) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_tensor_alloc", "bytes exceed host allocation size");
        return YVEX_ERR_BOUNDS;
    }
    for (i = 0; i < desc->rank; ++i) {
        if (desc->dims[i] == 0) {
            yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_alloc",
                           "dimensions must be non-zero");
            return YVEX_ERR_INVALID_ARG;
        }
    }
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor)
{
    return backend && tensor && tensor->owner == backend && tensor->owner_id != 0;
}

int yvex_backend_tensor_same_shape(const yvex_device_tensor *a,
                                   const yvex_device_tensor *b)
{
    unsigned int i;

    if (!a || !b || a->dtype != b->dtype || a->rank != b->rank || a->bytes != b->bytes) {
        return 0;
    }
    for (i = 0; i < a->rank; ++i) {
        if (a->dims[i] != b->dims[i]) {
            return 0;
        }
    }
    return 1;
}

static int memory_can_add(const yvex_backend *backend,
                          unsigned long long bytes,
                          yvex_error *err)
{
    unsigned long long next;

    if (backend->stats.allocated_bytes > ULLONG_MAX - bytes) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "allocated byte counter overflow");
        return YVEX_ERR_NOMEM;
    }
    next = backend->stats.allocated_bytes + bytes;
    if (backend->stats.memory_limit_bytes != 0 && next > backend->stats.memory_limit_bytes) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc",
                        "allocation exceeds CPU backend memory limit %llu",
                        backend->stats.memory_limit_bytes);
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

int yvex_cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err)
{
    yvex_device_tensor *tensor;
    unsigned int i;
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.tensor_alloc", "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    *out = NULL;

    rc = memory_can_add(backend, desc->bytes, err);
    if (rc != YVEX_OK) {
        return rc;
    }

    tensor = (yvex_device_tensor *)calloc(1, sizeof(*tensor));
    if (!tensor) {
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor object");
        return YVEX_ERR_NOMEM;
    }
    tensor->data = (unsigned char *)calloc(1, (size_t)desc->bytes);
    if (!tensor->data) {
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to allocate tensor data");
        return YVEX_ERR_NOMEM;
    }
    tensor->name = yvex_backend_strdup(desc->name);
    if (!tensor->name) {
        free(tensor->data);
        free(tensor);
        yvex_error_set(err, YVEX_ERR_NOMEM, "cpu.tensor_alloc", "failed to copy tensor name");
        return YVEX_ERR_NOMEM;
    }
    tensor->owner = backend;
    tensor->owner_id = backend->tensor_id_next++;
    tensor->dtype = desc->dtype;
    tensor->rank = desc->rank;
    for (i = 0; i < desc->rank; ++i) {
        tensor->dims[i] = desc->dims[i];
    }
    tensor->bytes = desc->bytes;

    backend->stats.allocated_bytes += desc->bytes;
    backend->stats.allocation_count += 1;
    if (backend->stats.allocated_bytes > backend->stats.peak_allocated_bytes) {
        backend->stats.peak_allocated_bytes = backend->stats.allocated_bytes;
    }

    *out = tensor;
    yvex_error_clear(err);
    return YVEX_OK;
}

void yvex_cpu_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor)
{
    if (!backend || !tensor || !yvex_backend_tensor_owner_is(backend, tensor)) {
        return;
    }
    if (backend->stats.allocated_bytes >= tensor->bytes) {
        backend->stats.allocated_bytes -= tensor->bytes;
    } else {
        backend->stats.allocated_bytes = 0;
    }
    if (backend->stats.allocation_count > 0) {
        backend->stats.allocation_count -= 1;
    }
    tensor->owner = NULL;
    tensor->owner_id = 0;
    free(tensor->data);
    free(tensor->name);
    free(tensor);
}

static int check_tensor_rw(const char *where,
                           const yvex_backend *backend,
                           const yvex_device_tensor *tensor,
                           unsigned long long len,
                           yvex_error *err)
{
    if (!yvex_backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (len != tensor->bytes) {
        yvex_error_setf(err, YVEX_ERR_BOUNDS, where,
                        "length %llu does not match tensor bytes %llu", len, tensor->bytes);
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

int yvex_cpu_tensor_write(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          const void *src,
                          unsigned long long len,
                          yvex_error *err)
{
    int rc = check_tensor_rw("yvex_backend_tensor_write", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(tensor->data, src, (size_t)len);
    tensor->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_tensor_read(yvex_backend *backend,
                         const yvex_device_tensor *tensor,
                         void *dst,
                         unsigned long long len,
                         yvex_error *err)
{
    int rc = check_tensor_rw("yvex_backend_tensor_read", backend, tensor, len, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    memcpy(dst, tensor->data, (size_t)len);
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_tensor_copy(yvex_backend *backend,
                         yvex_device_tensor *dst,
                         const yvex_device_tensor *src,
                         yvex_error *err)
{
    if (!yvex_backend_tensor_owner_is(backend, dst) ||
        !yvex_backend_tensor_owner_is(backend, src)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_tensor_copy",
                       "source and destination must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_same_shape(dst, src)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_tensor_copy",
                       "source and destination tensor descriptors differ");
        return YVEX_ERR_BOUNDS;
    }
    memcpy(dst->data, src->data, (size_t)src->bytes);
    dst->is_written = src->is_written;
    yvex_error_clear(err);
    return YVEX_OK;
}
