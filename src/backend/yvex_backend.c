/*
 * yvex_backend.c - backend dispatch and CPU reference implementation.
 *
 * Owner: src/backend.
 * Owns: backend lifecycle dispatch, exact public capability projection, tensor
 * lifetime API, and CPU reference tensor/primitive behavior.
 * Does not own: CLI parsing/rendering/output, CUDA module admission, graph
 * semantics, model-family behavior, qtype compute, or runtime generation.
 * Invariants: coarse capability APIs project exact variants; failed checked
 * release preserves caller ownership; no operator bytes are written here.
 * Boundary: bounded backend primitives are not model runtime support.
 */

#include "yvex_backend_private.h"

#include <limits.h>
#include <math.h>
#include <stdint.h>
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
    case YVEX_BACKEND_STATUS_CONTEXT_READY: return "context-ready";
    case YVEX_BACKEND_STATUS_UNSUPPORTED: return "unsupported";
    case YVEX_BACKEND_STATUS_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_backend_operation_variant_name(yvex_backend_operation_variant variant)
{
    switch (variant) {
    case YVEX_BACKEND_VARIANT_TENSOR_ALLOC: return "tensor-alloc";
    case YVEX_BACKEND_VARIANT_TENSOR_ZERO: return "tensor-zero";
    case YVEX_BACKEND_VARIANT_TENSOR_WRITE: return "tensor-write";
    case YVEX_BACKEND_VARIANT_TENSOR_READ: return "tensor-read";
    case YVEX_BACKEND_VARIANT_TENSOR_COPY: return "tensor-copy";
    case YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32: return "embed-f32-to-f32";
    case YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32: return "embed-f16-to-f32";
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32: return "rms-norm-f32-weight-f32";
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16: return "rms-norm-f32-weight-f16";
    case YVEX_BACKEND_VARIANT_ROPE_F32: return "rope-f32";
    case YVEX_BACKEND_VARIANT_MATMUL_F32: return "matmul-f32";
    case YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT: return "qtype-row-dot";
    case YVEX_BACKEND_VARIANT_DEEPSEEK_ATTENTION: return "deepseek-attention";
    case YVEX_BACKEND_VARIANT_MLP_DENSE_F32: return "mlp-dense-f32";
    case YVEX_BACKEND_VARIANT_MLP_ROUTED_F32: return "mlp-routed-f32";
    case YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32: return "attention-causal-f32";
    case YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32: return "attention-noncausal-f32";
    case YVEX_BACKEND_VARIANT_COUNT: break;
    }
    return "unknown";
}

const char *yvex_backend_capability_state_name(yvex_backend_capability_state state)
{
    switch (state) {
    case YVEX_BACKEND_CAPABILITY_UNSUPPORTED: return "unsupported";
    case YVEX_BACKEND_CAPABILITY_SUPPORTED: return "supported";
    case YVEX_BACKEND_CAPABILITY_FAILED: return "failed";
    }
    return "unknown";
}

const char *yvex_backend_capability_reason_name(yvex_backend_capability_reason reason)
{
    switch (reason) {
    case YVEX_BACKEND_CAPABILITY_REASON_NONE: return "none";
    case YVEX_BACKEND_CAPABILITY_REASON_DRIVER_UNAVAILABLE: return "driver-unavailable";
    case YVEX_BACKEND_CAPABILITY_REASON_DEVICE_UNAVAILABLE: return "device-unavailable";
    case YVEX_BACKEND_CAPABILITY_REASON_CONTEXT_UNAVAILABLE: return "context-unavailable";
    case YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT: return "kernel-bundle-absent";
    case YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_REJECTED: return "kernel-bundle-rejected";
    case YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING: return "required-function-missing";
    case YVEX_BACKEND_CAPABILITY_REASON_VARIANT_UNSUPPORTED: return "variant-unsupported";
    case YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED: return "launch-failed";
    case YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED: return "synchronization-failed";
    case YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED: return "cleanup-failed";
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
    case YVEX_BACKEND_CAP_OP_MLP: return "op_mlp";
    case YVEX_BACKEND_CAP_OP_RMS_NORM: return "op_rms_norm";
    case YVEX_BACKEND_CAP_OP_ROPE: return "op_rope";
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

int yvex_backend_cuda_context_available(void)
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

int yvex_backend_cuda_available(void)
{
    return yvex_backend_cuda_context_available();
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
    yvex_device_tensor *owned = tensor;
    yvex_error err;

    yvex_error_clear(&err);
    (void)yvex_backend_tensor_release(backend, &owned, &err);
}

int yvex_backend_tensor_release(yvex_backend *backend,
                                yvex_device_tensor **tensor,
                                yvex_error *err)
{
    int rc;

    if (!backend || !tensor || !*tensor) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_tensor_release",
                       "backend and tensor are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->tensor_free) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_tensor_release",
                       "backend does not support tensor release");
        return YVEX_ERR_UNSUPPORTED;
    }
    rc = backend->vtable->tensor_free(backend, *tensor, err);
    if (rc == YVEX_OK) {
        *tensor = NULL;
    }
    return rc;
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

int yvex_device_tensor_is_written(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->is_written != 0 : 0;
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

static void backend_variant_dtypes(yvex_backend_capability_result *out)
{
    switch (out->variant) {
    case YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32:
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32:
    case YVEX_BACKEND_VARIANT_ROPE_F32:
    case YVEX_BACKEND_VARIANT_MATMUL_F32:
    case YVEX_BACKEND_VARIANT_MLP_DENSE_F32:
    case YVEX_BACKEND_VARIANT_MLP_ROUTED_F32:
    case YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32:
    case YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32:
        out->input_dtype = YVEX_DTYPE_F32;
        out->weight_dtype = YVEX_DTYPE_F32;
        out->output_dtype = YVEX_DTYPE_F32;
        break;
    case YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT:
    case YVEX_BACKEND_VARIANT_DEEPSEEK_ATTENTION:
        out->input_dtype = YVEX_DTYPE_UNKNOWN;
        out->weight_dtype = YVEX_DTYPE_UNKNOWN;
        out->output_dtype = YVEX_DTYPE_F32;
        break;
    case YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32:
        out->input_dtype = YVEX_DTYPE_F16;
        out->weight_dtype = YVEX_DTYPE_F16;
        out->output_dtype = YVEX_DTYPE_F32;
        break;
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16:
        out->input_dtype = YVEX_DTYPE_F32;
        out->weight_dtype = YVEX_DTYPE_F16;
        out->output_dtype = YVEX_DTYPE_F32;
        break;
    case YVEX_BACKEND_VARIANT_TENSOR_ALLOC:
    case YVEX_BACKEND_VARIANT_TENSOR_ZERO:
    case YVEX_BACKEND_VARIANT_TENSOR_WRITE:
    case YVEX_BACKEND_VARIANT_TENSOR_READ:
    case YVEX_BACKEND_VARIANT_TENSOR_COPY:
    case YVEX_BACKEND_VARIANT_COUNT:
        break;
    }
}

int yvex_backend_query_capability(const yvex_backend *backend,
                                  yvex_backend_operation_variant variant,
                                  yvex_backend_capability_result *out,
                                  yvex_error *err)
{
    int rc;

    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_query_capability",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_query_capability",
                       "operation variant is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    memset(out, 0, sizeof(*out));
    out->backend_kind = backend->kind;
    out->variant = variant;
    out->state = YVEX_BACKEND_CAPABILITY_UNSUPPORTED;
    out->reason = YVEX_BACKEND_CAPABILITY_REASON_VARIANT_UNSUPPORTED;
    backend_variant_dtypes(out);
    if (!backend->vtable || !backend->vtable->query_capability) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    rc = backend->vtable->query_capability(backend, variant, out, err);
    if (rc == YVEX_OK) {
        out->backend_kind = backend->kind;
        out->variant = variant;
        backend_variant_dtypes(out);
    }
    return rc;
}

static int backend_variant_supported(const yvex_backend *backend,
                                     yvex_backend_operation_variant variant)
{
    yvex_backend_capability_result result;
    yvex_error err;

    yvex_error_clear(&err);
    return yvex_backend_query_capability(backend, variant, &result, &err) == YVEX_OK &&
           result.state == YVEX_BACKEND_CAPABILITY_SUPPORTED;
}

int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability)
{
    if (!backend) {
        return 0;
    }
    switch (capability) {
    case YVEX_BACKEND_CAP_TENSOR_ALLOC:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_ALLOC) &&
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_ZERO);
    case YVEX_BACKEND_CAP_TENSOR_READ_WRITE:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_WRITE) &&
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_READ) &&
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_TENSOR_COPY);
    case YVEX_BACKEND_CAP_OP_EMBED:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32) ||
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32);
    case YVEX_BACKEND_CAP_OP_MATMUL:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_MATMUL_F32);
    case YVEX_BACKEND_CAP_OP_MLP:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_MLP_DENSE_F32) ||
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_MLP_ROUTED_F32);
    case YVEX_BACKEND_CAP_OP_RMS_NORM:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32) ||
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16);
    case YVEX_BACKEND_CAP_OP_ROPE:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_ROPE_F32);
    case YVEX_BACKEND_CAP_OP_ATTENTION:
        return backend_variant_supported(backend, YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32) ||
               backend_variant_supported(backend, YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32);
    }
    return 0;
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

int yvex_backend_op_rms_norm(yvex_backend *backend,
                             const yvex_device_tensor *input,
                             const yvex_device_tensor *weight,
                             float epsilon,
                             yvex_device_tensor *out,
                             yvex_error *err)
{
    if (!backend || !input || !weight || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rms_norm",
                       "backend, input, weight, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!isfinite(epsilon) || epsilon <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rms_norm",
                       "epsilon must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->op_rms_norm) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rms_norm",
                       "backend does not support RMSNorm op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_rms_norm(backend, input, weight, epsilon, out, err);
}

int yvex_backend_op_rope(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         unsigned long long position,
                         float rope_base,
                         yvex_device_tensor *out,
                         yvex_error *err)
{
    if (!backend || !input || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "backend, input, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!isfinite(rope_base) || rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be finite and greater than 1");
        return YVEX_ERR_INVALID_ARG;
    }
    (void)position;
    if (!backend->vtable || !backend->vtable->op_rope) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "backend does not support RoPE op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_rope(backend, input, position, rope_base, out, err);
}

int yvex_backend_op_matmul(yvex_backend *backend,
                           const yvex_device_tensor *input,
                           const yvex_device_tensor *weight,
                           yvex_device_tensor *out,
                           yvex_error *err)
{
    if (!backend || !input || !weight || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_matmul",
                       "backend, input, weight, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->op_matmul) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_matmul",
                       "backend does not support matmul op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_matmul(backend, input, weight, out, err);
}

int yvex_backend_op_mlp(yvex_backend *backend,
                        const yvex_device_tensor *input,
                        const yvex_device_tensor *gate_weight,
                        const yvex_device_tensor *up_weight,
                        const yvex_device_tensor *down_weight,
                        const yvex_mlp_options *options,
                        yvex_device_tensor *intermediate,
                        yvex_device_tensor *out,
                        yvex_error *err)
{
    if (!backend || !input || !gate_weight || !up_weight || !down_weight ||
        !options || !intermediate || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_mlp",
                       "backend, input, weights, options, intermediate, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!backend->vtable || !backend->vtable->op_mlp) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_mlp",
                       "backend does not support MLP op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_mlp(backend, input, gate_weight, up_weight,
                                   down_weight, options, intermediate, out, err);
}

int yvex_backend_op_attention(yvex_backend *backend,
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
                              yvex_error *err)
{
    if (!backend || !query || !keys || !values || !score_scratch ||
        !probability_scratch || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "backend, Q/K/V, scratches, and output are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (seq_len == 0) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "seq_len must be positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (position >= seq_len) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_attention",
                       "position-out-of-range");
        return YVEX_ERR_BOUNDS;
    }
    if (!isfinite(scale) || scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    (void)causal;
    if (!backend->vtable || !backend->vtable->op_attention) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_attention",
                       "backend does not support attention op");
        return YVEX_ERR_UNSUPPORTED;
    }
    return backend->vtable->op_attention(backend, query, keys, values, seq_len,
                                         position, scale, causal, score_scratch,
                                         probability_scratch, out, err);
}



int yvex_cpu_tensor_alloc(yvex_backend *backend,
                          const yvex_backend_tensor_desc *desc,
                          yvex_device_tensor **out,
                          yvex_error *err);
int yvex_cpu_tensor_free(yvex_backend *backend,
                         yvex_device_tensor *tensor,
                         yvex_error *err);
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
int yvex_cpu_op_rms_norm(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         const yvex_device_tensor *weight,
                         float epsilon,
                         yvex_device_tensor *out,
                         yvex_error *err);
int yvex_cpu_op_rope(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     unsigned long long position,
                     float rope_base,
                     yvex_device_tensor *out,
                     yvex_error *err);
int yvex_cpu_op_matmul(yvex_backend *backend,
                       const yvex_device_tensor *input,
                       const yvex_device_tensor *weight,
                       yvex_device_tensor *out,
                       yvex_error *err);
int yvex_cpu_op_mlp(yvex_backend *backend,
                    const yvex_device_tensor *input,
                    const yvex_device_tensor *gate_weight,
                    const yvex_device_tensor *up_weight,
                    const yvex_device_tensor *down_weight,
                    const yvex_mlp_options *options,
                    yvex_device_tensor *intermediate,
                    yvex_device_tensor *out,
                    yvex_error *err);
int yvex_cpu_op_attention(yvex_backend *backend,
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

static int cpu_query_capability(const yvex_backend *backend,
                                yvex_backend_operation_variant variant,
                                yvex_backend_capability_result *out,
                                yvex_error *err)
{
    if (!backend || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "backend and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cpu.query_capability",
                       "operation variant is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    out->state = YVEX_BACKEND_CAPABILITY_SUPPORTED;
    out->reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
    out->context_available = 1;
    out->function_available = 1;
    yvex_error_clear(err);
    return YVEX_OK;
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
    cpu_query_capability,
    yvex_cpu_op_embed,
    yvex_cpu_op_rms_norm,
    yvex_cpu_op_rope,
    yvex_cpu_op_matmul,
    yvex_cpu_op_mlp,
    yvex_cpu_op_attention,
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



static int tensor_is_f32_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / sizeof(float)) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

static int tensor_is_f16_bytes(const yvex_device_tensor *tensor,
                               unsigned long long elements)
{
    return elements <= (unsigned long long)(UINT64_MAX / 2ull) &&
           tensor->bytes == elements * 2ull;
}

static float backend_f16_bits_to_float(unsigned int h)
{
    unsigned int sign = (h & 0x8000u) << 16;
    unsigned int exp = (h >> 10) & 0x1fu;
    unsigned int mant = h & 0x03ffu;
    uint32_t raw;
    float out;

    if (exp == 0) {
        if (mant == 0) {
            raw = sign;
        } else {
            exp = 1u;
            while ((mant & 0x0400u) == 0) {
                mant <<= 1;
                exp -= 1u;
            }
            mant &= 0x03ffu;
            raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
        }
    } else if (exp == 31u) {
        raw = sign | 0x7f800000u | (mant << 13);
    } else {
        raw = sign | ((exp + (127u - 15u)) << 23) | (mant << 13);
    }
    memcpy(&out, &raw, sizeof(out));
    return out;
}

static unsigned int backend_read_u16le(const unsigned char *p)
{
    return ((unsigned int)p[0]) | ((unsigned int)p[1] << 8);
}

static double backend_sqrt_double(double x)
{
    double guess;
    unsigned int i;

    if (x <= 0.0) {
        return 0.0;
    }
    guess = x >= 1.0 ? x : 1.0;
    for (i = 0; i < 32u; ++i) {
        guess = 0.5 * (guess + (x / guess));
    }
    return guess;
}

static double backend_abs_double(double x)
{
    return x < 0.0 ? -x : x;
}

static double backend_nth_root_double(double x, unsigned long long n)
{
    double lo = 1.0;
    double hi = x > 1.0 ? x : 1.0;
    unsigned int iter;

    if (x <= 0.0 || n == 0) {
        return 0.0;
    }
    if (n == 1) {
        return x;
    }
    for (iter = 0; iter < 96u; ++iter) {
        double mid = 0.5 * (lo + hi);
        double acc = 1.0;
        unsigned long long i;

        for (i = 0; i < n; ++i) {
            acc *= mid;
            if (acc > x) {
                break;
            }
        }
        if (acc > x) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

static double backend_wrap_radians(double x)
{
    const double two_pi = 6.28318530717958647692;

    while (x > 3.14159265358979323846) {
        x -= two_pi;
    }
    while (x < -3.14159265358979323846) {
        x += two_pi;
    }
    return x;
}

static void backend_sincos_double(double x, double *sine, double *cosine)
{
    double x2;
    double s;
    double c;

    x = backend_wrap_radians(x);
    x2 = x * x;
    s = x * (1.0 -
             (x2 / 6.0) +
             ((x2 * x2) / 120.0) -
             ((x2 * x2 * x2) / 5040.0) +
             ((x2 * x2 * x2 * x2) / 362880.0));
    c = 1.0 -
        (x2 / 2.0) +
        ((x2 * x2) / 24.0) -
        ((x2 * x2 * x2) / 720.0) +
        ((x2 * x2 * x2 * x2) / 40320.0);
    if (backend_abs_double(s) < 0.000000000001) {
        s = 0.0;
    }
    if (backend_abs_double(c) < 0.000000000001) {
        c = 0.0;
    }
    if (sine) {
        *sine = s;
    }
    if (cosine) {
        *cosine = c;
    }
}

static double backend_exp_double(double x)
{
    const double ln2 = 0.69314718055994530942;
    double term;
    double sum;
    int n = 0;
    unsigned int i;

    if (x < -60.0) {
        return 0.0;
    }
    if (x > 60.0) {
        x = 60.0;
    }
    while (x > 0.5) {
        x -= ln2;
        ++n;
    }
    while (x < -0.5) {
        x += ln2;
        --n;
    }

    term = 1.0;
    sum = 1.0;
    for (i = 1u; i <= 18u; ++i) {
        term *= x / (double)i;
        sum += term;
    }
    while (n > 0) {
        sum *= 2.0;
        --n;
    }
    while (n < 0) {
        sum *= 0.5;
        ++n;
    }
    return sum;
}

static int backend_rope_head_dim(const yvex_device_tensor *tensor,
                                 unsigned long long *out,
                                 yvex_error *err)
{
    unsigned long long head_dim;

    if (!tensor || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "tensor and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tensor->rank == 1) {
        head_dim = tensor->dims[0];
    } else if (tensor->rank == 2 && tensor->dims[0] == 1) {
        head_dim = tensor->dims[1];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE input must be rank 1 or dims [1, head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (head_dim == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "rope-head-dim-zero");
        return YVEX_ERR_FORMAT;
    }
    if ((head_dim & 1ull) != 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "rope-head-dim-odd");
        return YVEX_ERR_FORMAT;
    }
    *out = head_dim;
    return YVEX_OK;
}

static int backend_attention_validate(const yvex_backend *backend,
                                      const yvex_device_tensor *query,
                                      const yvex_device_tensor *keys,
                                      const yvex_device_tensor *values,
                                      unsigned long long seq_len,
                                      unsigned long long position,
                                      yvex_device_tensor *score_scratch,
                                      yvex_device_tensor *probability_scratch,
                                      yvex_device_tensor *out,
                                      unsigned long long *head_dim_out,
                                      unsigned long long *kv_elements_out,
                                      const char *where,
                                      yvex_error *err)
{
    unsigned long long head_dim;
    unsigned long long kv_elements;

    if (!yvex_backend_tensor_owner_is(backend, query) ||
        !yvex_backend_tensor_owner_is(backend, keys) ||
        !yvex_backend_tensor_owner_is(backend, values) ||
        !yvex_backend_tensor_owner_is(backend, score_scratch) ||
        !yvex_backend_tensor_owner_is(backend, probability_scratch) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "Q/K/V, scratches, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (query->dtype != YVEX_DTYPE_F32 || keys->dtype != YVEX_DTYPE_F32 ||
        values->dtype != YVEX_DTYPE_F32 || score_scratch->dtype != YVEX_DTYPE_F32 ||
        probability_scratch->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "attention primitive supports F32 Q/K/V, scratches, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (seq_len == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "attention-seq-len-zero");
        return YVEX_ERR_FORMAT;
    }
    if (position >= seq_len) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "position-out-of-range");
        return YVEX_ERR_BOUNDS;
    }
    if (query->rank != 1 || query->dims[0] == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention query must be rank 1 with non-zero head_dim");
        return YVEX_ERR_FORMAT;
    }
    head_dim = query->dims[0];
    if (seq_len > ULLONG_MAX / head_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "attention-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    kv_elements = seq_len * head_dim;
    if (keys->rank != 2 || keys->dims[0] != seq_len || keys->dims[1] != head_dim ||
        values->rank != 2 || values->dims[0] != seq_len || values->dims[1] != head_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention keys and values must have dims [seq_len, head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (score_scratch->rank != 1 || score_scratch->dims[0] != seq_len ||
        probability_scratch->rank != 1 || probability_scratch->dims[0] != seq_len) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention score/probability scratches must have dims [seq_len]");
        return YVEX_ERR_FORMAT;
    }
    if (out->rank != 1 || out->dims[0] != head_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "attention output must have dims [head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (!tensor_is_f32_bytes(query, head_dim) ||
        !tensor_is_f32_bytes(keys, kv_elements) ||
        !tensor_is_f32_bytes(values, kv_elements) ||
        !tensor_is_f32_bytes(score_scratch, seq_len) ||
        !tensor_is_f32_bytes(probability_scratch, seq_len) ||
        !tensor_is_f32_bytes(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "attention tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }

    *head_dim_out = head_dim;
    *kv_elements_out = kv_elements;
    return YVEX_OK;
}

static int backend_matmul_validate(const yvex_backend *backend,
                                   const yvex_device_tensor *input,
                                   const yvex_device_tensor *weight,
                                   const yvex_device_tensor *out,
                                   unsigned long long *m_out,
                                   unsigned long long *k_out,
                                   unsigned long long *n_out,
                                   const char *where,
                                   yvex_error *err)
{
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long input_elements;
    unsigned long long weight_elements;
    unsigned long long output_elements;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 ||
        weight->dtype != YVEX_DTYPE_F32 ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "matmul primitive supports F32 input, weight, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (input->rank != 2 || weight->rank != 2 || out->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul tensors must be rank 2");
        return YVEX_ERR_FORMAT;
    }
    m = input->dims[0];
    k = input->dims[1];
    n = weight->dims[1];
    if (m == 0 || k == 0 || n == 0 || weight->dims[0] == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "matmul-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (weight->dims[0] != k) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul input/weight inner dimensions must match");
        return YVEX_ERR_FORMAT;
    }
    if (out->dims[0] != m || out->dims[1] != n) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "matmul output must have dims [m, n]");
        return YVEX_ERR_FORMAT;
    }
    if (m > ULLONG_MAX / k ||
        k > ULLONG_MAX / n ||
        m > ULLONG_MAX / n) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "matmul-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    input_elements = m * k;
    weight_elements = k * n;
    output_elements = m * n;
    if (!tensor_is_f32_bytes(input, input_elements) ||
        !tensor_is_f32_bytes(weight, weight_elements) ||
        !tensor_is_f32_bytes(out, output_elements)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "matmul tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *m_out = m;
    *k_out = k;
    *n_out = n;
    return YVEX_OK;
}

static double backend_silu_double(double x)
{
    return x / (1.0 + backend_exp_double(-x));
}

static int backend_mlp_mul3(unsigned long long a,
                            unsigned long long b,
                            unsigned long long c,
                            unsigned long long *out)
{
    unsigned long long ab;

    if (a > ULLONG_MAX / b) {
        return 0;
    }
    ab = a * b;
    if (ab > ULLONG_MAX / c) {
        return 0;
    }
    *out = ab * c;
    return 1;
}

static int backend_mlp_validate(const yvex_backend *backend,
                                const yvex_device_tensor *input,
                                const yvex_device_tensor *gate_weight,
                                const yvex_device_tensor *up_weight,
                                const yvex_device_tensor *down_weight,
                                const yvex_mlp_options *options,
                                const yvex_device_tensor *intermediate,
                                const yvex_device_tensor *out,
                                unsigned long long *batch_out,
                                unsigned long long *hidden_dim_out,
                                unsigned long long *ffn_dim_out,
                                unsigned long long *gate_offset_out,
                                unsigned long long *up_offset_out,
                                unsigned long long *down_offset_out,
                                const char *where,
                                yvex_error *err)
{
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long input_elements;
    unsigned long long intermediate_elements;
    unsigned long long output_elements;
    unsigned long long dense_up_elements;
    unsigned long long down_elements;
    unsigned long long routed_up_elements;
    unsigned long long routed_down_elements;
    unsigned long long gate_offset = 0ull;
    unsigned long long up_offset = 0ull;
    unsigned long long down_offset = 0ull;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, gate_weight) ||
        !yvex_backend_tensor_owner_is(backend, up_weight) ||
        !yvex_backend_tensor_owner_is(backend, down_weight) ||
        !yvex_backend_tensor_owner_is(backend, intermediate) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "input, weights, intermediate, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 ||
        gate_weight->dtype != YVEX_DTYPE_F32 ||
        up_weight->dtype != YVEX_DTYPE_F32 ||
        down_weight->dtype != YVEX_DTYPE_F32 ||
        intermediate->dtype != YVEX_DTYPE_F32 ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "MLP primitive supports F32 input, weights, intermediate, and output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!options || options->batch == 0 || options->hidden_dim == 0 ||
        options->ffn_dim == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "mlp-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (!options->gated || !options->activation ||
        strcmp(options->activation, "silu") != 0) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where,
                       "mlp-unsupported-activation");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (options->routed_expert_mode) {
        if (options->expert_count == 0 || options->expert_id >= options->expert_count) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-expert-id-out-of-range");
            return YVEX_ERR_BOUNDS;
        }
    }

    batch = options->batch;
    hidden_dim = options->hidden_dim;
    ffn_dim = options->ffn_dim;
    if (batch > ULLONG_MAX / hidden_dim ||
        batch > ULLONG_MAX / ffn_dim ||
        hidden_dim > ULLONG_MAX / ffn_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    input_elements = batch * hidden_dim;
    intermediate_elements = batch * ffn_dim;
    output_elements = input_elements;
    dense_up_elements = hidden_dim * ffn_dim;
    if (ffn_dim > ULLONG_MAX / hidden_dim) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    down_elements = ffn_dim * hidden_dim;

    if (input->rank != 2 || input->dims[0] != batch || input->dims[1] != hidden_dim ||
        intermediate->rank != 2 || intermediate->dims[0] != batch ||
        intermediate->dims[1] != ffn_dim ||
        out->rank != 2 || out->dims[0] != batch || out->dims[1] != hidden_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "mlp input/intermediate/output shape mismatch");
        return YVEX_ERR_FORMAT;
    }

    if (options->routed_expert_mode) {
        if (!backend_mlp_mul3(options->expert_count, hidden_dim, ffn_dim,
                              &routed_up_elements) ||
            !backend_mlp_mul3(options->expert_count, ffn_dim, hidden_dim,
                              &routed_down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
            return YVEX_ERR_BOUNDS;
        }
        if (gate_weight->rank != 3 || up_weight->rank != 3 || down_weight->rank != 3 ||
            gate_weight->dims[0] != options->expert_count ||
            gate_weight->dims[1] != hidden_dim || gate_weight->dims[2] != ffn_dim ||
            up_weight->dims[0] != options->expert_count ||
            up_weight->dims[1] != hidden_dim || up_weight->dims[2] != ffn_dim ||
            down_weight->dims[0] != options->expert_count ||
            down_weight->dims[1] != ffn_dim || down_weight->dims[2] != hidden_dim) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp routed weights must have dims [experts, hidden_dim, ffn_dim] and [experts, ffn_dim, hidden_dim]");
            return YVEX_ERR_FORMAT;
        }
        if (!tensor_is_f32_bytes(gate_weight, routed_up_elements) ||
            !tensor_is_f32_bytes(up_weight, routed_up_elements) ||
            !tensor_is_f32_bytes(down_weight, routed_down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp routed weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
        gate_offset = options->expert_id * dense_up_elements;
        up_offset = gate_offset;
        down_offset = options->expert_id * down_elements;
    } else {
        if (gate_weight->rank != 2 || up_weight->rank != 2 || down_weight->rank != 2 ||
            gate_weight->dims[0] != hidden_dim || gate_weight->dims[1] != ffn_dim ||
            up_weight->dims[0] != hidden_dim || up_weight->dims[1] != ffn_dim ||
            down_weight->dims[0] != ffn_dim || down_weight->dims[1] != hidden_dim) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp dense weights must have dims [hidden_dim, ffn_dim] and [ffn_dim, hidden_dim]");
            return YVEX_ERR_FORMAT;
        }
        if (!tensor_is_f32_bytes(gate_weight, dense_up_elements) ||
            !tensor_is_f32_bytes(up_weight, dense_up_elements) ||
            !tensor_is_f32_bytes(down_weight, down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp dense weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
    }

    if (!tensor_is_f32_bytes(input, input_elements) ||
        !tensor_is_f32_bytes(intermediate, intermediate_elements) ||
        !tensor_is_f32_bytes(out, output_elements)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "mlp tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }

    *batch_out = batch;
    *hidden_dim_out = hidden_dim;
    *ffn_dim_out = ffn_dim;
    *gate_offset_out = gate_offset;
    *up_offset_out = up_offset;
    *down_offset_out = down_offset;
    return YVEX_OK;
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
    const float *embedding_f32;
    const unsigned char *embedding_f16;
    float *out_data;
    unsigned long long element_count;

    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_embed",
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if ((embedding->dtype != YVEX_DTYPE_F32 && embedding->dtype != YVEX_DTYPE_F16) ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_embed",
                       "backend layer CPU embed supports F32 and F16 embeddings with F32 output");
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
    element_count = hidden_size * vocab_size;
    if (embedding->dtype == YVEX_DTYPE_F32 && !tensor_is_f32_bytes(embedding, element_count)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    if (embedding->dtype == YVEX_DTYPE_F16 && !tensor_is_f16_bytes(embedding, element_count)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_embed",
                       "embedding tensor byte size does not match F16 dims");
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

    embedding_f32 = (const float *)embedding->data;
    embedding_f16 = (const unsigned char *)embedding->data;
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
            unsigned long long index = ((unsigned long long)token_id * hidden_size) + e;
            if (embedding->dtype == YVEX_DTYPE_F16) {
                out_data[(t * hidden_size) + e] =
                    backend_f16_bits_to_float(backend_read_u16le(embedding_f16 + (index * 2ull)));
            } else {
                out_data[(t * hidden_size) + e] = embedding_f32[index];
            }
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_op_attention(yvex_backend *backend,
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
                          yvex_error *err)
{
    const float *q_data;
    const float *k_data;
    const float *v_data;
    float *score_data;
    float *prob_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long kv_elements;
    unsigned long long visible_count;
    unsigned long long i;
    unsigned long long d;
    double max_score = 0.0;
    double sum_exp = 0.0;
    int rc;

    if (!isfinite(scale) || scale <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_attention",
                       "scale must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = backend_attention_validate(backend, query, keys, values, seq_len, position,
                                    score_scratch, probability_scratch, out,
                                    &head_dim, &kv_elements,
                                    "yvex_backend_op_attention", err);
    if (rc != YVEX_OK) {
        return rc;
    }
    (void)kv_elements;

    q_data = (const float *)query->data;
    k_data = (const float *)keys->data;
    v_data = (const float *)values->data;
    score_data = (float *)score_scratch->data;
    prob_data = (float *)probability_scratch->data;
    out_data = (float *)out->data;
    visible_count = causal ? position + 1ull : seq_len;

    for (i = 0; i < seq_len; ++i) {
        double score = 0.0;
        if (causal && i > position) {
            score_data[i] = 0.0f;
            prob_data[i] = 0.0f;
            continue;
        }
        for (d = 0; d < head_dim; ++d) {
            score += (double)q_data[d] * (double)k_data[(i * head_dim) + d];
        }
        score *= (double)scale;
        score_data[i] = (float)score;
        if (i == 0 || score > max_score) {
            max_score = score;
        }
    }
    for (i = 0; i < visible_count; ++i) {
        double e = backend_exp_double((double)score_data[i] - max_score);
        prob_data[i] = (float)e;
        sum_exp += e;
    }
    if (sum_exp <= 0.0) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_attention",
                       "attention softmax sum is zero");
        return YVEX_ERR_STATE;
    }
    for (i = 0; i < visible_count; ++i) {
        prob_data[i] = (float)((double)prob_data[i] / sum_exp);
    }
    for (d = 0; d < head_dim; ++d) {
        double value = 0.0;
        for (i = 0; i < visible_count; ++i) {
            value += (double)prob_data[i] * (double)v_data[(i * head_dim) + d];
        }
        out_data[d] = (float)value;
    }

    score_scratch->is_written = 1;
    probability_scratch->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_op_matmul(yvex_backend *backend,
                       const yvex_device_tensor *input,
                       const yvex_device_tensor *weight,
                       yvex_device_tensor *out,
                       yvex_error *err)
{
    const float *input_data;
    const float *weight_data;
    float *out_data;
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    unsigned long long row;
    unsigned long long col;
    int rc;

    rc = backend_matmul_validate(backend, input, weight, out,
                                 &m, &k, &n, "yvex_backend_op_matmul", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    weight_data = (const float *)weight->data;
    out_data = (float *)out->data;
    for (row = 0; row < m; ++row) {
        for (col = 0; col < n; ++col) {
            double sum = 0.0;
            unsigned long long inner;
            for (inner = 0; inner < k; ++inner) {
                sum += (double)input_data[(row * k) + inner] *
                       (double)weight_data[(inner * n) + col];
            }
            out_data[(row * n) + col] = (float)sum;
        }
    }
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_op_mlp(yvex_backend *backend,
                    const yvex_device_tensor *input,
                    const yvex_device_tensor *gate_weight,
                    const yvex_device_tensor *up_weight,
                    const yvex_device_tensor *down_weight,
                    const yvex_mlp_options *options,
                    yvex_device_tensor *intermediate,
                    yvex_device_tensor *out,
                    yvex_error *err)
{
    const float *input_data;
    const float *gate_data;
    const float *up_data;
    const float *down_data;
    float *intermediate_data;
    float *out_data;
    unsigned long long batch;
    unsigned long long hidden_dim;
    unsigned long long ffn_dim;
    unsigned long long gate_offset;
    unsigned long long up_offset;
    unsigned long long down_offset;
    unsigned long long row;
    unsigned long long j;
    unsigned long long h;
    int rc;

    rc = backend_mlp_validate(backend, input, gate_weight, up_weight, down_weight,
                              options, intermediate, out, &batch, &hidden_dim,
                              &ffn_dim, &gate_offset, &up_offset, &down_offset,
                              "yvex_backend_op_mlp", err);
    if (rc != YVEX_OK) {
        return rc;
    }

    input_data = (const float *)input->data;
    gate_data = (const float *)gate_weight->data + gate_offset;
    up_data = (const float *)up_weight->data + up_offset;
    down_data = (const float *)down_weight->data + down_offset;
    intermediate_data = (float *)intermediate->data;
    out_data = (float *)out->data;

    for (row = 0; row < batch; ++row) {
        for (j = 0; j < ffn_dim; ++j) {
            double gate_sum = 0.0;
            double up_sum = 0.0;
            for (h = 0; h < hidden_dim; ++h) {
                double x = (double)input_data[(row * hidden_dim) + h];
                gate_sum += x * (double)gate_data[(h * ffn_dim) + j];
                up_sum += x * (double)up_data[(h * ffn_dim) + j];
            }
            intermediate_data[(row * ffn_dim) + j] =
                (float)(backend_silu_double(gate_sum) * up_sum);
        }
        for (h = 0; h < hidden_dim; ++h) {
            double sum = 0.0;
            for (j = 0; j < ffn_dim; ++j) {
                sum += (double)intermediate_data[(row * ffn_dim) + j] *
                       (double)down_data[(j * hidden_dim) + h];
            }
            out_data[(row * hidden_dim) + h] = (float)sum;
        }
    }
    intermediate->is_written = 1;
    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_op_rope(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     unsigned long long position,
                     float rope_base,
                     yvex_device_tensor *out,
                     yvex_error *err)
{
    const float *input_data;
    float *out_data;
    unsigned long long head_dim;
    unsigned long long pair_count;
    unsigned long long pair;
    double inverse_root;
    double frequency = 1.0;
    int rc;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rope",
                       "input and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rope",
                       "CPU RoPE supports F32 input/output");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!yvex_backend_tensor_same_shape(input, out)) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rope",
                       "RoPE output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!isfinite(rope_base) || rope_base <= 1.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rope",
                       "rope_base must be finite and greater than 1");
        return YVEX_ERR_INVALID_ARG;
    }
    rc = backend_rope_head_dim(input, &head_dim, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!tensor_is_f32_bytes(input, head_dim) || !tensor_is_f32_bytes(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rope",
                       "RoPE input/output bytes must match F32 head_dim");
        return YVEX_ERR_BOUNDS;
    }

    pair_count = head_dim / 2ull;
    inverse_root = 1.0 / backend_nth_root_double((double)rope_base, pair_count);
    input_data = (const float *)input->data;
    out_data = (float *)out->data;

    for (pair = 0; pair < pair_count; ++pair) {
        unsigned long long even_index = pair * 2ull;
        unsigned long long odd_index = even_index + 1ull;
        double sine;
        double cosine;
        double even = (double)input_data[even_index];
        double odd = (double)input_data[odd_index];
        double angle = (double)position * frequency;

        backend_sincos_double(angle, &sine, &cosine);
        out_data[even_index] = (float)((even * cosine) - (odd * sine));
        out_data[odd_index] = (float)((even * sine) + (odd * cosine));
        frequency *= inverse_root;
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}

int yvex_cpu_op_rms_norm(yvex_backend *backend,
                         const yvex_device_tensor *input,
                         const yvex_device_tensor *weight,
                         float epsilon,
                         yvex_device_tensor *out,
                         yvex_error *err)
{
    const float *input_data;
    const float *weight_f32;
    const unsigned char *weight_f16;
    float *out_data;
    unsigned long long hidden_size;
    unsigned long long i;
    double sum_squares = 0.0;
    double rms;
    double inv_rms;

    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, "yvex_backend_op_rms_norm",
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32 ||
        (weight->dtype != YVEX_DTYPE_F32 && weight->dtype != YVEX_DTYPE_F16)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "yvex_backend_op_rms_norm",
                       "CPU RMSNorm supports F32 input/output with F16 or F32 weight");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!isfinite(epsilon) || epsilon <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "yvex_backend_op_rms_norm",
                       "epsilon must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (input->rank == 2 && input->dims[0] == 1) {
        hidden_size = input->dims[1];
    } else if (input->rank == 1) {
        hidden_size = input->dims[0];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm input must be rank 1 or dims [1, hidden]");
        return YVEX_ERR_FORMAT;
    }
    if (hidden_size == 0) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm hidden size must be non-zero");
        return YVEX_ERR_FORMAT;
    }
    if (weight->rank != 1 || weight->dims[0] != hidden_size) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm weight must be rank 1 and match hidden size");
        return YVEX_ERR_FORMAT;
    }
    if (out->rank != input->rank ||
        out->dims[0] != input->dims[0] ||
        (input->rank == 2 && out->dims[1] != input->dims[1])) {
        yvex_error_set(err, YVEX_ERR_FORMAT, "yvex_backend_op_rms_norm",
                       "RMSNorm output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!tensor_is_f32_bytes(input, hidden_size) ||
        !tensor_is_f32_bytes(out, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm input/output bytes must match F32 hidden size");
        return YVEX_ERR_BOUNDS;
    }
    if (weight->dtype == YVEX_DTYPE_F32 && !tensor_is_f32_bytes(weight, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm F32 weight bytes must match hidden size");
        return YVEX_ERR_BOUNDS;
    }
    if (weight->dtype == YVEX_DTYPE_F16 && !tensor_is_f16_bytes(weight, hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, "yvex_backend_op_rms_norm",
                       "RMSNorm F16 weight bytes must match hidden size");
        return YVEX_ERR_BOUNDS;
    }

    input_data = (const float *)input->data;
    weight_f32 = (const float *)weight->data;
    weight_f16 = (const unsigned char *)weight->data;
    out_data = (float *)out->data;

    for (i = 0; i < hidden_size; ++i) {
        sum_squares += (double)input_data[i] * (double)input_data[i];
    }
    rms = backend_sqrt_double((sum_squares / (double)hidden_size) + (double)epsilon);
    inv_rms = rms > 0.0 ? 1.0 / rms : 0.0;
    for (i = 0; i < hidden_size; ++i) {
        float w = weight->dtype == YVEX_DTYPE_F16
                      ? backend_f16_bits_to_float(backend_read_u16le(weight_f16 + (i * 2ull)))
                      : weight_f32[i];
        out_data[i] = (float)((double)input_data[i] * inv_rms * (double)w);
    }

    out->is_written = 1;
    yvex_error_clear(err);
    return YVEX_OK;
}



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

int yvex_cpu_tensor_free(yvex_backend *backend,
                         yvex_device_tensor *tensor,
                         yvex_error *err)
{
    if (!backend || !tensor || !yvex_backend_tensor_owner_is(backend, tensor)) {
        yvex_error_set(err, YVEX_ERR_STATE, "cpu.tensor_free",
                       "tensor does not belong to this backend");
        return YVEX_ERR_STATE;
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
    yvex_error_clear(err);
    return YVEX_OK;
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
