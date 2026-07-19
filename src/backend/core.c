/* Owner: src/backend.
 * Owns: backend lifecycle dispatch, exact public capability projection, and the backend-neutral tensor and
 *   primitive API.
 * Does not own: CLI parsing/rendering/output, CUDA module admission, graph semantics, model-family behavior, qtype
 *   compute, or runtime generation.
 * Invariants: coarse capability APIs project exact variants; failed checked release preserves caller ownership; no
 *   operator bytes are written here.
 * Boundary: concrete CPU and CUDA behavior belongs to independently compiled backend implementations; bounded
 *   primitives are not model runtime support.
 * Purpose: validate generic requests and dispatch them through an admitted backend vtable.
 * Inputs: backend kinds, descriptors, tensors, and typed operation requests.
 * Effects: mutates only explicit backend/tensor outputs through the selected implementation.
 * Failure: preserves caller ownership and returns typed admission, state, or operation failures. */

#include <yvex/internal/backend.h>
#include <yvex/internal/quant_numeric.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int backend_desc_valid(const yvex_backend_tensor_desc *desc, yvex_error *err);

/* Projects immutable numeric truth without probing hardware or launching work. */
/* Purpose: Implement the canonical qtype refuse mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
void yvex_backend_qtype_refuse(yvex_backend_qtype_fact *fact,
                               const char *backend,
                               const char *qtype)
{
    const yvex_quant_numeric_capability *capability;
    int available = 0;

    if (!fact) return;
    fact->backend = backend ? backend : "unknown";
    fact->qtype = qtype ? qtype : "unknown";
    capability = yvex_quant_numeric_capability_by_name(qtype);
    if (!capability) {
        fact->compute_status = "unknown-qtype";
        fact->reason = "unknown-qtype";
        return;
    }
    if (backend && strcmp(backend, "cpu") == 0)
        available = capability->dedicated_cpu_compute_available;
    else if (backend && strcmp(backend, "cuda") == 0)
        available = capability->dedicated_cuda_compute_available;
    else {
        fact->compute_status = "unsupported-backend";
        fact->reason = "backend-not-covered-by-numeric-contract";
        return;
    }
    fact->compute_status = available ? "available" : "unavailable";
    fact->reason = available
        ? "dedicated-encoded-row-dot-v1"
        : yvex_quant_refusal_name(
              capability->identity_known && capability->storage_admitted
                  ? backend && strcmp(backend, "cuda") == 0
                      ? YVEX_QUANT_REFUSAL_CUDA_COMPUTE_UNAVAILABLE
                      : YVEX_QUANT_REFUSAL_CPU_COMPUTE_UNAVAILABLE
                  : capability->refusal);
}

static const char *const backend_kind_names[] = {"cpu", "cuda", "metal", "rocm"};
static const char *const backend_status_names[] = {
    "ready", "context-ready", "unsupported", "failed"
};
static const char *const backend_variant_names[YVEX_BACKEND_VARIANT_COUNT] = {
    "tensor-alloc", "tensor-zero", "tensor-write", "tensor-read", "tensor-copy",
    "embed-f32-to-f32", "embed-f16-to-f32", "rms-norm-f32-weight-f32",
    "rms-norm-f32-weight-f16", "rope-f32", "matmul-f32", "mlp-dense-f32",
    "mlp-routed-f32", "attention-causal-f32", "attention-noncausal-f32",
    "qtype-row-dot", "encoded-attention"
};
static const char *const backend_capability_state_names[] = {
    "unsupported", "supported", "failed"
};
static const char *const backend_capability_reason_names[] = {
    "none", "driver-unavailable", "device-unavailable", "context-unavailable",
    "kernel-bundle-absent", "kernel-bundle-rejected", "required-function-missing",
    "variant-unsupported", "launch-failed", "synchronization-failed", "cleanup-failed"
};
static const char *const backend_capability_names[] = {
    "tensor_alloc", "tensor_read_write", "op_embed", "op_matmul",
    "op_mlp", "op_rms_norm", "op_rope", "op_attention"
};

typedef struct {
    yvex_dtype input;
    yvex_dtype weight;
    yvex_dtype output;
} backend_dtype_projection;

#define BACKEND_F32_VARIANT                                                        \
    {                                                                              \
        YVEX_DTYPE_F32, YVEX_DTYPE_F32, YVEX_DTYPE_F32                             \
    }
static const backend_dtype_projection backend_variant_dtype_table[] = {
    [YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32] = {YVEX_DTYPE_F16, YVEX_DTYPE_F16,
                                                YVEX_DTYPE_F32},
    [YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16] = {YVEX_DTYPE_F32, YVEX_DTYPE_F16,
                                                       YVEX_DTYPE_F32},
    [YVEX_BACKEND_VARIANT_ROPE_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_MATMUL_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_MLP_DENSE_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_MLP_ROUTED_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32] = BACKEND_F32_VARIANT,
    [YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT] = {YVEX_DTYPE_UNKNOWN, YVEX_DTYPE_UNKNOWN,
                                             YVEX_DTYPE_F32},
    [YVEX_BACKEND_VARIANT_ATTENTION_ENCODED] = {YVEX_DTYPE_UNKNOWN, YVEX_DTYPE_UNKNOWN,
                                                 YVEX_DTYPE_F32},
};
#undef BACKEND_F32_VARIANT

typedef struct {
    unsigned int count;
    int require_all;
    yvex_backend_operation_variant variants[3];
} backend_capability_rule;

static const backend_capability_rule backend_capability_rules[] = {
    [YVEX_BACKEND_CAP_TENSOR_ALLOC] =
        {2u, 1, {YVEX_BACKEND_VARIANT_TENSOR_ALLOC, YVEX_BACKEND_VARIANT_TENSOR_ZERO}},
    [YVEX_BACKEND_CAP_TENSOR_READ_WRITE] =
        {3u, 1, {YVEX_BACKEND_VARIANT_TENSOR_WRITE, YVEX_BACKEND_VARIANT_TENSOR_READ,
                 YVEX_BACKEND_VARIANT_TENSOR_COPY}},
    [YVEX_BACKEND_CAP_OP_EMBED] =
        {2u, 0, {YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32,
                 YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32}},
    [YVEX_BACKEND_CAP_OP_MATMUL] = {1u, 1, {YVEX_BACKEND_VARIANT_MATMUL_F32}},
    [YVEX_BACKEND_CAP_OP_MLP] =
        {2u, 0, {YVEX_BACKEND_VARIANT_MLP_DENSE_F32,
                 YVEX_BACKEND_VARIANT_MLP_ROUTED_F32}},
    [YVEX_BACKEND_CAP_OP_RMS_NORM] =
        {2u, 0, {YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32,
                 YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16}},
    [YVEX_BACKEND_CAP_OP_ROPE] = {1u, 1, {YVEX_BACKEND_VARIANT_ROPE_F32}},
    [YVEX_BACKEND_CAP_OP_ATTENTION] =
        {2u, 0, {YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32,
                 YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32}},
};

/* Purpose: Return the canonical diagnostic label for name at. */
static const char *backend_name_at(const char *const *names,
                                   size_t count,
                                   unsigned int index)
{
    return index < count && names[index] ? names[index] : "unknown";
}

/* Purpose: Return the canonical diagnostic label for kind name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_kind_name(yvex_backend_kind kind)
{
    return backend_name_at(backend_kind_names,
                           sizeof(backend_kind_names) / sizeof(backend_kind_names[0]),
                           (unsigned int)kind);
}

/*
 * Parses the canonical backend names shared by runtime and command adapters.
 * It mutates only out, allocates nothing, performs no IO, and refuses future
 * or unavailable backend names without implying backend admission. */
/* Purpose: Translate operator input into the canonical typed kind parse value without ambiguous aliases.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_kind_parse(const char *name,
                            yvex_backend_kind *out,
                            yvex_error *err)
{
    if (!out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "backend_kind",
                       "output backend kind is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!name || strcmp(name, "cpu") == 0) {
        *out = YVEX_BACKEND_KIND_CPU;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (strcmp(name, "cuda") == 0) {
        *out = YVEX_BACKEND_KIND_CUDA;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    yvex_error_setf(err, YVEX_ERR_INVALID_ARG, "backend_kind",
                    "unknown backend kind: %s", name);
    return YVEX_ERR_INVALID_ARG;
}

/* Purpose: Return the canonical diagnostic label for status name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_status_name(yvex_backend_status status)
{
    return backend_name_at(backend_status_names,
                           sizeof(backend_status_names) / sizeof(backend_status_names[0]),
                           (unsigned int)status);
}

/* Purpose: Return the canonical diagnostic label for operation variant name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_operation_variant_name(yvex_backend_operation_variant variant)
{
    return backend_name_at(backend_variant_names, YVEX_BACKEND_VARIANT_COUNT,
                           (unsigned int)variant);
}

/* Purpose: Return the canonical diagnostic label for capability state name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_capability_state_name(yvex_backend_capability_state state)
{
    return backend_name_at(
        backend_capability_state_names,
        sizeof(backend_capability_state_names) / sizeof(backend_capability_state_names[0]),
        (unsigned int)state);
}

/* Purpose: Return the canonical diagnostic label for capability reason name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_capability_reason_name(yvex_backend_capability_reason reason)
{
    return backend_name_at(
        backend_capability_reason_names,
        sizeof(backend_capability_reason_names) / sizeof(backend_capability_reason_names[0]),
        (unsigned int)reason);
}

/* Purpose: Return the canonical diagnostic label for capability name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_backend_capability_name(yvex_backend_capability capability)
{
    return backend_name_at(backend_capability_names,
                           sizeof(backend_capability_names) / sizeof(backend_capability_names[0]),
                           (unsigned int)capability);
}

/* Purpose: Select and construct the requested backend kind through its canonical implementation.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Construct the CPU backend through the same public admission contract as other kinds.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_open_cpu(yvex_backend **out, yvex_error *err)
{
    yvex_backend_options options;

    memset(&options, 0, sizeof(options));
    options.kind = YVEX_BACKEND_KIND_CPU;
    return yvex_backend_open(out, &options, err);
}

/* Purpose: Project whether admitted state satisfies context available without promoting a higher capability.
 * Inputs: None; immutable owner constants determine the result.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Project whether admitted state satisfies available without promoting a higher capability.
 * Inputs: None; immutable owner constants determine the result.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_cuda_available(void)
{
    return yvex_backend_cuda_context_available();
}

/* Purpose: Release the resources owned by close without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Implement the canonical kind of mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
yvex_backend_kind yvex_backend_kind_of(const yvex_backend *backend)
{
    return backend ? backend->kind : YVEX_BACKEND_KIND_CPU;
}

/* Purpose: Implement the canonical status of mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
yvex_backend_status yvex_backend_status_of(const yvex_backend *backend)
{
    return backend ? backend->status : YVEX_BACKEND_STATUS_FAILED;
}

/* Purpose: Retrieve get memory stats from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Retrieve get device info from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Reserve budgeted storage for tensor alloc with checked size accounting.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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
    rc = backend_desc_valid(desc, err);
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

/* Purpose: Release the resources owned by tensor free without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
void yvex_backend_tensor_free(yvex_backend *backend,
                              yvex_device_tensor *tensor)
{
    yvex_device_tensor *owned = tensor;
    yvex_error err;

    yvex_error_clear(&err);
    (void)yvex_backend_tensor_release(backend, &owned, &err);
}

/* Purpose: Release the resources owned by tensor release without changing borrowed inputs.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Return the canonical diagnostic label for device tensor name.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const char *yvex_device_tensor_name(const yvex_device_tensor *tensor)
{
    return tensor && tensor->name ? tensor->name : "";
}

/* Purpose: Implement the canonical device tensor dtype mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
yvex_dtype yvex_device_tensor_dtype(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->dtype : YVEX_DTYPE_UNKNOWN;
}

/* Purpose: Implement the canonical device tensor rank mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
unsigned int yvex_device_tensor_rank(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->rank : 0;
}

/* Purpose: Implement the canonical device tensor dims mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
const unsigned long long *yvex_device_tensor_dims(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->dims : NULL;
}

/* Purpose: Implement the canonical device tensor bytes mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns the canonical unknown or zero sentinel for an invalid typed value.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
unsigned long long yvex_device_tensor_bytes(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->bytes : 0;
}

/* Purpose: Project whether admitted state satisfies device tensor is written without promoting a higher capability.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_device_tensor_is_written(const yvex_device_tensor *tensor)
{
    return tensor ? tensor->is_written != 0 : 0;
}

/* Purpose: Publish tensor write only within its admitted destination range.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Retrieve tensor read from admitted immutable or owned state.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Copy tensor copy between compatible admitted ranges without changing semantic identity.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Implement the canonical sync mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Implement the canonical variant dtypes mechanism owned by the backend boundary. */
static void backend_variant_dtypes(yvex_backend_capability_result *out)
{
    const backend_dtype_projection *projection;

    if (!out || out->variant < 0 || out->variant >= YVEX_BACKEND_VARIANT_COUNT) {
        return;
    }
    projection = &backend_variant_dtype_table[out->variant];
    out->input_dtype = projection->input;
    out->weight_dtype = projection->weight;
    out->output_dtype = projection->output;
}

/* Purpose: Project the admitted numeric and device capability for query capability.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Project whether admitted state satisfies variant supported without promoting a higher capability. */
static int backend_variant_supported(const yvex_backend *backend,
                                     yvex_backend_operation_variant variant)
{
    yvex_backend_capability_result result;
    yvex_error err;

    yvex_error_clear(&err);
    return yvex_backend_query_capability(backend, variant, &result, &err) == YVEX_OK &&
           result.state == YVEX_BACKEND_CAPABILITY_SUPPORTED;
}

/* Purpose: Project whether admitted state satisfies supports without promoting a higher capability.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_supports(const yvex_backend *backend,
                          yvex_backend_capability capability)
{
    const backend_capability_rule *rule;
    unsigned int supported = 0u;
    unsigned int index;

    if (!backend || capability < 0 ||
        (size_t)capability >= sizeof(backend_capability_rules) /
                                  sizeof(backend_capability_rules[0])) {
        return 0;
    }
    rule = &backend_capability_rules[capability];
    for (index = 0u; index < rule->count; ++index) {
        supported += backend_variant_supported(backend, rule->variants[index]) ? 1u : 0u;
    }
    return rule->require_all ? supported == rule->count : supported != 0u;
}

/* Purpose: Execute the typed op embed operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Execute the typed op rms norm operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Execute the typed op rope operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Execute the typed op matmul operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Execute the typed op mlp operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Execute the typed op attention operation over already admitted buffers.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Mutates only the admitted destination or transaction after every precondition passes.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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

/* Purpose: Enforce the typed ownership, geometry, and lifecycle invariants for desc valid.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
static int backend_desc_valid(const yvex_backend_tensor_desc *desc, yvex_error *err)
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

/* Purpose: prove exact F32 storage geometry without multiplying past U64.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_tensor_f32_elements(const yvex_device_tensor *tensor,
                                     unsigned long long elements)
{
    return tensor && elements <= ULLONG_MAX / sizeof(float) &&
           tensor->bytes == elements * (unsigned long long)sizeof(float);
}

/* Purpose: compute the deterministic positive real root shared by CPU and CUDA RoPE.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
double yvex_backend_nth_root(double value, unsigned long long degree)
{
    double low = 1.0;
    double high = value > 1.0 ? value : 1.0;
    unsigned int iteration;

    if (value <= 0.0 || degree == 0ull) {
        return 0.0;
    }
    if (degree == 1ull) {
        return value;
    }
    for (iteration = 0u; iteration < 96u; ++iteration) {
        double midpoint = 0.5 * (low + high);
        double power = 1.0;
        unsigned long long exponent;

        for (exponent = 0ull; exponent < degree && power <= value; ++exponent) {
            power *= midpoint;
        }
        if (power > value) {
            high = midpoint;
        } else {
            low = midpoint;
        }
    }
    return 0.5 * (low + high);
}

/* Purpose: validate and account a backend allocation before acquiring storage.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_memory_can_add(const yvex_backend *backend,
                                unsigned long long bytes,
                                const char *backend_name,
                                const char *where,
                                yvex_error *err)
{
    unsigned long long next;

    if (backend->stats.allocated_bytes > ULLONG_MAX - bytes) {
        yvex_error_set(err, YVEX_ERR_NOMEM, where, "allocated byte counter overflow");
        return YVEX_ERR_NOMEM;
    }
    next = backend->stats.allocated_bytes + bytes;
    if (backend->stats.memory_limit_bytes != 0ull &&
        next > backend->stats.memory_limit_bytes) {
        yvex_error_setf(err, YVEX_ERR_NOMEM, where,
                        "allocation exceeds %s backend memory limit %llu",
                        backend_name, backend->stats.memory_limit_bytes);
        return YVEX_ERR_NOMEM;
    }
    return YVEX_OK;
}

/* Purpose: commit one admitted allocation to canonical backend counters.
 * Inputs: A validated configuration, checked resource limits, and caller-owned result storage.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
void yvex_backend_memory_acquire(yvex_backend *backend, unsigned long long bytes)
{
    backend->stats.allocated_bytes += bytes;
    backend->stats.allocation_count += 1ull;
    if (backend->stats.allocated_bytes > backend->stats.peak_allocated_bytes) {
        backend->stats.peak_allocated_bytes = backend->stats.allocated_bytes;
    }
}

/* Purpose: release one owned allocation from canonical backend counters.
 * Inputs: An owned object that may be null or already released where its lifecycle permits.
 * Effects: Releases only resources owned by the supplied object and leaves it reset or unusable.
 * Failure: Null and already-released inputs follow the idempotent lifecycle contract.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
void yvex_backend_memory_release(yvex_backend *backend, unsigned long long bytes)
{
    backend->stats.allocated_bytes =
        backend->stats.allocated_bytes >= bytes ? backend->stats.allocated_bytes - bytes : 0ull;
    if (backend->stats.allocation_count > 0ull) {
        backend->stats.allocation_count -= 1ull;
    }
}

/* Purpose: validate one exact whole-tensor read or write.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_tensor_rw_validate(const char *where,
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

/* Purpose: validate same-owner, same-shape tensor copy endpoints.
 * Inputs: Typed admitted handles, immutable source ranges, checked dimensions, and an explicit destination.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_tensor_copy_validate(const yvex_backend *backend,
                                      const yvex_device_tensor *dst,
                                      const yvex_device_tensor *src,
                                      const char *where,
                                      yvex_error *err)
{
    if (!yvex_backend_tensor_owner_is(backend, dst) ||
        !yvex_backend_tensor_owner_is(backend, src)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "source and destination must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (!yvex_backend_tensor_same_shape(dst, src)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "source and destination tensor descriptors differ");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

/* Purpose: prove exact F16 storage geometry without multiplying past U64. */
static int backend_f16_elements(const yvex_device_tensor *tensor,
                                unsigned long long elements)
{
    return tensor && elements <= ULLONG_MAX / 2ull && tensor->bytes == elements * 2ull;
}

/* Purpose: admit one same-backend F32 tensor set with operation-specific diagnostics.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
static int backend_validate_f32_set(const yvex_backend *backend,
                                    const yvex_device_tensor *const *tensors,
                                    size_t count,
                                    const char *owner_message,
                                    const char *dtype_message,
                                    const char *where,
                                    yvex_error *err)
{
    size_t index;

    for (index = 0; index < count; ++index) {
        if (!yvex_backend_tensor_owner_is(backend, tensors[index])) {
            yvex_error_set(err, YVEX_ERR_STATE, where, owner_message);
            return YVEX_ERR_STATE;
        }
    }
    for (index = 0; index < count; ++index) {
        if (tensors[index]->dtype != YVEX_DTYPE_F32) {
            yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where, dtype_message);
            return YVEX_ERR_UNSUPPORTED;
        }
    }
    return YVEX_OK;
}

/* Purpose: multiply three nonzero MLP dimensions with exact overflow refusal. */
static int backend_mul3(unsigned long long a,
                        unsigned long long b,
                        unsigned long long c,
                        unsigned long long *out)
{
    if (!out || b == 0ull || c == 0ull || a > ULLONG_MAX / b || a * b > ULLONG_MAX / c) {
        return 0;
    }
    *out = a * b * c;
    return 1;
}

/* Purpose: validate the shared CPU/CUDA embedding geometry and token domain.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_validate_embed(const yvex_backend *backend,
                                const yvex_device_tensor *embedding,
                                const unsigned int *token_ids,
                                unsigned long long token_count,
                                const yvex_device_tensor *out,
                                unsigned long long *hidden_size,
                                unsigned long long *vocab_size,
                                const char *unsupported_message,
                                const char *where,
                                yvex_error *err)
{
    unsigned long long elements;
    unsigned long long index;

    if (!hidden_size || !vocab_size || !token_ids || token_count == 0ull) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "embedding validation arguments are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_backend_tensor_owner_is(backend, embedding) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "embedding and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if ((embedding->dtype != YVEX_DTYPE_F32 && embedding->dtype != YVEX_DTYPE_F16) ||
        out->dtype != YVEX_DTYPE_F32) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where, unsupported_message);
        return YVEX_ERR_UNSUPPORTED;
    }
    if (embedding->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "embedding tensor must have rank 2");
        return YVEX_ERR_FORMAT;
    }
    *hidden_size = embedding->dims[0];
    *vocab_size = embedding->dims[1];
    if (*hidden_size == 0ull || *vocab_size == 0ull ||
        *hidden_size > ULLONG_MAX / *vocab_size) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "embedding dimensions overflow");
        return YVEX_ERR_BOUNDS;
    }
    elements = *hidden_size * *vocab_size;
    if ((embedding->dtype == YVEX_DTYPE_F32 &&
         !yvex_backend_tensor_f32_elements(embedding, elements)) ||
        (embedding->dtype == YVEX_DTYPE_F16 &&
         !backend_f16_elements(embedding, elements))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       embedding->dtype == YVEX_DTYPE_F32
                           ? "embedding tensor byte size does not match F32 dims"
                           : "embedding tensor byte size does not match F16 dims");
        return YVEX_ERR_BOUNDS;
    }
    if (token_count > ULLONG_MAX / *hidden_size || out->rank != 2 ||
        out->dims[0] != token_count || out->dims[1] != *hidden_size ||
        !yvex_backend_tensor_f32_elements(out, token_count * *hidden_size)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       token_count > ULLONG_MAX / *hidden_size
                           ? "output dimensions overflow"
                           : out->rank != 2 || out->dims[0] != token_count ||
                                     out->dims[1] != *hidden_size
                                 ? "output tensor must have dims [token_count, hidden_size]"
                                 : "output tensor byte size does not match F32 dims");
        return YVEX_ERR_BOUNDS;
    }
    for (index = 0ull; index < token_count; ++index) {
        if ((unsigned long long)token_ids[index] >= *vocab_size) {
            yvex_error_setf(err, YVEX_ERR_BOUNDS, where,
                            "token id %u exceeds embedding vocab size %llu",
                            token_ids[index], *vocab_size);
            return YVEX_ERR_BOUNDS;
        }
    }
    return YVEX_OK;
}

/* Purpose: validate the shared CPU/CUDA RMS normalization contract.
 * Inputs: Immutable typed facts whose ownership, shape, or lifecycle state must be admitted.
 * Effects: Does not mutate caller-visible or owner state.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_validate_rms_norm(const yvex_backend *backend,
                                   const yvex_device_tensor *input,
                                   const yvex_device_tensor *weight,
                                   float epsilon,
                                   const yvex_device_tensor *out,
                                   unsigned long long *hidden_size,
                                   const char *unsupported_message,
                                   const char *where,
                                   yvex_error *err)
{
    if (!hidden_size) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "hidden-size output is required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (!yvex_backend_tensor_owner_is(backend, input) ||
        !yvex_backend_tensor_owner_is(backend, weight) ||
        !yvex_backend_tensor_owner_is(backend, out)) {
        yvex_error_set(err, YVEX_ERR_STATE, where,
                       "input, weight, and output tensors must belong to this backend");
        return YVEX_ERR_STATE;
    }
    if (input->dtype != YVEX_DTYPE_F32 || out->dtype != YVEX_DTYPE_F32 ||
        (weight->dtype != YVEX_DTYPE_F32 && weight->dtype != YVEX_DTYPE_F16)) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where, unsupported_message);
        return YVEX_ERR_UNSUPPORTED;
    }
    if (!isfinite(epsilon) || epsilon <= 0.0f) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "epsilon must be finite and positive");
        return YVEX_ERR_INVALID_ARG;
    }
    if (input->rank == 2 && input->dims[0] == 1) {
        *hidden_size = input->dims[1];
    } else if (input->rank == 1) {
        *hidden_size = input->dims[0];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "RMSNorm input must be rank 1 or dims [1, hidden]");
        return YVEX_ERR_FORMAT;
    }
    if (*hidden_size == 0ull || weight->rank != 1 || weight->dims[0] != *hidden_size ||
        out->rank != input->rank || out->dims[0] != input->dims[0] ||
        (input->rank == 2 && out->dims[1] != input->dims[1])) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       *hidden_size == 0ull
                           ? "RMSNorm hidden size must be non-zero"
                           : weight->rank != 1 || weight->dims[0] != *hidden_size
                                 ? "RMSNorm weight must be rank 1 and match hidden size"
                                 : "RMSNorm output shape must match input shape");
        return YVEX_ERR_FORMAT;
    }
    if (!yvex_backend_tensor_f32_elements(input, *hidden_size) ||
        !yvex_backend_tensor_f32_elements(out, *hidden_size) ||
        (weight->dtype == YVEX_DTYPE_F32 &&
         !yvex_backend_tensor_f32_elements(weight, *hidden_size)) ||
        (weight->dtype == YVEX_DTYPE_F16 &&
         !backend_f16_elements(weight, *hidden_size))) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       !yvex_backend_tensor_f32_elements(input, *hidden_size) ||
                               !yvex_backend_tensor_f32_elements(out, *hidden_size)
                           ? "RMSNorm input/output bytes must match F32 hidden size"
                           : weight->dtype == YVEX_DTYPE_F32
                                 ? "RMSNorm F32 weight bytes must match hidden size"
                                 : "RMSNorm F16 weight bytes must match hidden size");
        return YVEX_ERR_BOUNDS;
    }
    return YVEX_OK;
}

/* Purpose: validate the backend-neutral RoPE vector shape.
 * Inputs: one tensor, required head-width output, and diagnostic owner.
 * Effects: writes head_dim only after complete validation.
 * Failure: typed invalid, shape, zero-width, and odd-width refusal.
 * Boundary: geometry admission only; no backend dispatch or numeric execution. */
int yvex_backend_validate_rope(const yvex_device_tensor *tensor,
                               unsigned long long *head_dim,
                               const char *where,
                               yvex_error *err)
{
    unsigned long long width;

    if (!tensor || !head_dim) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, where, "tensor and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    if (tensor->rank == 1) {
        width = tensor->dims[0];
    } else if (tensor->rank == 2 && tensor->dims[0] == 1) {
        width = tensor->dims[1];
    } else {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "RoPE input must be rank 1 or dims [1, head_dim]");
        return YVEX_ERR_FORMAT;
    }
    if (width == 0ull || (width & 1ull) != 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       width == 0ull ? "rope-head-dim-zero" : "rope-head-dim-odd");
        return YVEX_ERR_FORMAT;
    }
    *head_dim = width;
    return YVEX_OK;
}

/* Purpose: validate the common F32 matrix product contract for CPU and CUDA.
 * Inputs: same-backend input/weight/output tensors and dimension outputs.
 * Effects: writes m/k/n only after exact shape and byte accounting.
 * Failure: typed ownership, dtype, rank, dimension, or overflow refusal.
 * Boundary: backend-neutral admission; execution remains backend-owned. */
int yvex_backend_validate_matmul(const yvex_backend *backend,
                                 const yvex_device_tensor *input,
                                 const yvex_device_tensor *weight,
                                 const yvex_device_tensor *out,
                                 unsigned long long *m_out,
                                 unsigned long long *k_out,
                                 unsigned long long *n_out,
                                 const char *where,
                                 yvex_error *err)
{
    const yvex_device_tensor *tensors[] = {input, weight, out};
    unsigned long long m;
    unsigned long long k;
    unsigned long long n;
    int status = backend_validate_f32_set(
        backend, tensors, 3u,
        "input, weight, and output tensors must belong to this backend",
        "matmul primitive supports F32 input, weight, and output", where, err);

    if (status != YVEX_OK) return status;
    if (!m_out || !k_out || !n_out || input->rank != 2 || weight->rank != 2 || out->rank != 2) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "matmul tensors must be rank 2");
        return YVEX_ERR_FORMAT;
    }
    m = input->dims[0];
    k = input->dims[1];
    n = weight->dims[1];
    if (m == 0ull || k == 0ull || n == 0ull || weight->dims[0] == 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "matmul-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (weight->dims[0] != k || out->dims[0] != m || out->dims[1] != n) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       weight->dims[0] != k ? "matmul input/weight inner dimensions must match"
                                            : "matmul output must have dims [m, n]");
        return YVEX_ERR_FORMAT;
    }
    if (m > ULLONG_MAX / k || k > ULLONG_MAX / n || m > ULLONG_MAX / n) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "matmul-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    if (!yvex_backend_tensor_f32_elements(input, m * k) ||
        !yvex_backend_tensor_f32_elements(weight, k * n) ||
        !yvex_backend_tensor_f32_elements(out, m * n)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "matmul tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *m_out = m;
    *k_out = k;
    *n_out = n;
    return YVEX_OK;
}

/* Purpose: validate the shared bounded attention primitive geometry.
 * Inputs: admitted Q/K/V, scratch/output tensors, sequence facts, and outputs.
 * Effects: writes head and KV element counts only after exact admission.
 * Failure: typed ownership, dtype, rank, bounds, or byte-geometry refusal.
 * Boundary: does not select attention class or execute a backend kernel. */
int yvex_backend_validate_attention(const yvex_backend *backend,
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
    const yvex_device_tensor *tensors[] = {
        query, keys, values, score_scratch, probability_scratch, out,
    };
    unsigned long long head_dim;
    unsigned long long kv_elements;
    int status = backend_validate_f32_set(
        backend, tensors, 6u,
        "Q/K/V, scratches, and output tensors must belong to this backend",
        "attention primitive supports F32 Q/K/V, scratches, and output", where, err);

    if (status != YVEX_OK) return status;
    if (!head_dim_out || !kv_elements_out || seq_len == 0ull || query->rank != 1 ||
        query->dims[0] == 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       seq_len == 0ull ? "attention-seq-len-zero"
                                       : "attention query must be rank 1 with non-zero head_dim");
        return YVEX_ERR_FORMAT;
    }
    if (position >= seq_len) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "position-out-of-range");
        return YVEX_ERR_BOUNDS;
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
        probability_scratch->rank != 1 || probability_scratch->dims[0] != seq_len ||
        out->rank != 1 || out->dims[0] != head_dim) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       out->rank != 1 || out->dims[0] != head_dim
                           ? "attention output must have dims [head_dim]"
                           : "attention score/probability scratches must have dims [seq_len]");
        return YVEX_ERR_FORMAT;
    }
    if (!yvex_backend_tensor_f32_elements(query, head_dim) ||
        !yvex_backend_tensor_f32_elements(keys, kv_elements) ||
        !yvex_backend_tensor_f32_elements(values, kv_elements) ||
        !yvex_backend_tensor_f32_elements(score_scratch, seq_len) ||
        !yvex_backend_tensor_f32_elements(probability_scratch, seq_len) ||
        !yvex_backend_tensor_f32_elements(out, head_dim)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "attention tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *head_dim_out = head_dim;
    *kv_elements_out = kv_elements;
    return YVEX_OK;
}

/* Purpose: validate dense or routed F32 MLP geometry once for every backend.
 * Inputs: same-backend tensors, immutable options, dimension/offset outputs.
 * Effects: writes dimensions and selected expert offsets after full admission.
 * Failure: typed ownership, dtype, activation, expert, shape, or overflow refusal.
 * Boundary: validates a primitive only; model routing and kernel execution stay outside. */
int yvex_backend_validate_mlp(const yvex_backend *backend,
                              const yvex_device_tensor *input,
                              const yvex_device_tensor *gate_weight,
                              const yvex_device_tensor *up_weight,
                              const yvex_device_tensor *down_weight,
                              const yvex_mlp_options *options,
                              const yvex_device_tensor *intermediate,
                              const yvex_device_tensor *out,
                              unsigned long long *batch_out,
                              unsigned long long *hidden_out,
                              unsigned long long *ffn_out,
                              unsigned long long *gate_offset_out,
                              unsigned long long *up_offset_out,
                              unsigned long long *down_offset_out,
                              const char *where,
                              yvex_error *err)
{
    const yvex_device_tensor *tensors[] = {
        input, gate_weight, up_weight, down_weight, intermediate, out,
    };
    unsigned long long batch;
    unsigned long long hidden;
    unsigned long long ffn;
    unsigned long long up_elements;
    unsigned long long down_elements;
    unsigned long long routed_up;
    unsigned long long routed_down;
    unsigned long long gate_offset = 0ull;
    unsigned long long up_offset = 0ull;
    unsigned long long down_offset = 0ull;
    int status = backend_validate_f32_set(
        backend, tensors, 6u,
        "input, weights, intermediate, and output tensors must belong to this backend",
        "MLP primitive supports F32 input, weights, intermediate, and output", where, err);

    if (status != YVEX_OK) return status;
    if (!options || !batch_out || !hidden_out || !ffn_out || options->batch == 0ull ||
        options->hidden_dim == 0ull || options->ffn_dim == 0ull) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where, "mlp-zero-dimension");
        return YVEX_ERR_FORMAT;
    }
    if (!options->gated || !options->activation || strcmp(options->activation, "silu") != 0) {
        yvex_error_set(err, YVEX_ERR_UNSUPPORTED, where, "mlp-unsupported-activation");
        return YVEX_ERR_UNSUPPORTED;
    }
    if (options->routed_expert_mode &&
        (options->expert_count == 0ull || options->expert_id >= options->expert_count)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-expert-id-out-of-range");
        return YVEX_ERR_BOUNDS;
    }
    batch = options->batch;
    hidden = options->hidden_dim;
    ffn = options->ffn_dim;
    if (batch > ULLONG_MAX / hidden || batch > ULLONG_MAX / ffn ||
        hidden > ULLONG_MAX / ffn || ffn > ULLONG_MAX / hidden) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
        return YVEX_ERR_BOUNDS;
    }
    up_elements = hidden * ffn;
    down_elements = ffn * hidden;
    if (input->rank != 2 || input->dims[0] != batch || input->dims[1] != hidden ||
        intermediate->rank != 2 || intermediate->dims[0] != batch ||
        intermediate->dims[1] != ffn || out->rank != 2 || out->dims[0] != batch ||
        out->dims[1] != hidden) {
        yvex_error_set(err, YVEX_ERR_FORMAT, where,
                       "mlp input/intermediate/output shape mismatch");
        return YVEX_ERR_FORMAT;
    }
    if (options->routed_expert_mode) {
        if (!backend_mul3(options->expert_count, hidden, ffn, &routed_up) ||
            !backend_mul3(options->expert_count, ffn, hidden, &routed_down)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where, "mlp-element-count-overflow");
            return YVEX_ERR_BOUNDS;
        }
        if (gate_weight->rank != 3 || up_weight->rank != 3 || down_weight->rank != 3 ||
            gate_weight->dims[0] != options->expert_count || gate_weight->dims[1] != hidden ||
            gate_weight->dims[2] != ffn || up_weight->dims[0] != options->expert_count ||
            up_weight->dims[1] != hidden || up_weight->dims[2] != ffn ||
            down_weight->dims[0] != options->expert_count || down_weight->dims[1] != ffn ||
            down_weight->dims[2] != hidden) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp routed weights must match expert/hidden/ffn geometry");
            return YVEX_ERR_FORMAT;
        }
        if (!yvex_backend_tensor_f32_elements(gate_weight, routed_up) ||
            !yvex_backend_tensor_f32_elements(up_weight, routed_up) ||
            !yvex_backend_tensor_f32_elements(down_weight, routed_down)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp routed weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
        gate_offset = options->expert_id * up_elements;
        up_offset = gate_offset;
        down_offset = options->expert_id * down_elements;
    } else {
        if (gate_weight->rank != 2 || up_weight->rank != 2 || down_weight->rank != 2 ||
            gate_weight->dims[0] != hidden || gate_weight->dims[1] != ffn ||
            up_weight->dims[0] != hidden || up_weight->dims[1] != ffn ||
            down_weight->dims[0] != ffn || down_weight->dims[1] != hidden) {
            yvex_error_set(err, YVEX_ERR_FORMAT, where,
                           "mlp dense weights must have hidden/ffn transpose geometry");
            return YVEX_ERR_FORMAT;
        }
        if (!yvex_backend_tensor_f32_elements(gate_weight, up_elements) ||
            !yvex_backend_tensor_f32_elements(up_weight, up_elements) ||
            !yvex_backend_tensor_f32_elements(down_weight, down_elements)) {
            yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                           "mlp dense weight bytes must match F32 shape accounting");
            return YVEX_ERR_BOUNDS;
        }
    }
    if (!yvex_backend_tensor_f32_elements(input, batch * hidden) ||
        !yvex_backend_tensor_f32_elements(intermediate, batch * ffn) ||
        !yvex_backend_tensor_f32_elements(out, batch * hidden)) {
        yvex_error_set(err, YVEX_ERR_BOUNDS, where,
                       "mlp tensor bytes must match F32 shape accounting");
        return YVEX_ERR_BOUNDS;
    }
    *batch_out = batch;
    *hidden_out = hidden;
    *ffn_out = ffn;
    if (gate_offset_out) *gate_offset_out = gate_offset;
    if (up_offset_out) *up_offset_out = up_offset;
    if (down_offset_out) *down_offset_out = down_offset;
    return YVEX_OK;
}

/* Purpose: Implement the canonical tensor owner is mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
int yvex_backend_tensor_owner_is(const yvex_backend *backend,
                                 const yvex_device_tensor *tensor)
{
    return backend && tensor && tensor->owner == backend && tensor->owner_id != 0;
}

/* Purpose: Implement the canonical tensor same shape mechanism owned by the backend boundary.
 * Inputs: Typed caller-owned outputs and immutable values declared by this subsystem ABI.
 * Effects: Updates only caller-owned result storage or lifecycle state explicitly named by the ABI.
 * Failure: Returns a typed backend refusal and publishes no partial success state.
 * Boundary: Backend admission and execution; does not infer model topology or generation capability. */
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
