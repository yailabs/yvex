/*
 * cuda/cuda_capability.c - CUDA kernel-bundle and capability admission.
 *
 * Owner:
 *   src/backend/cuda
 *
 * Owns:
 *   canonical kernel-bundle admission, exact CUDA operation variants, atomic
 *   module/function rollback, launch/synchronization failure state, and
 *   temporary CUDA allocation cleanup.
 *
 * Does not own:
 *   tensor geometry, graph semantics, qtype compute, model-family behavior,
 *   runtime generation, CLI rendering, or generated PTX storage.
 *
 * Invariants:
 *   no kernel variant is supported before the canonical generated bundle and
 *   every required function are admitted; failed launch or synchronization
 *   never remains a supported variant.
 *
 * Boundary:
 *   bounded primitive capability is not transformer or generation support.
 */

#include "driver.h"

#include <stdlib.h>
#include <string.h>

#ifdef YVEX_HAVE_CUDA_KERNEL_PTX
#include "kernels.h"
#endif

/* Contract: clears every module/function handle without Driver API side effects. */
static void cuda_bundle_clear_handles(yvex_cuda_backend_state *state)
{
    if (!state) {
        return;
    }
    state->module = NULL;
    state->embed_function = NULL;
    state->embed_f16_function = NULL;
    state->rms_norm_f32_function = NULL;
    state->rms_norm_f16_function = NULL;
    state->rope_function = NULL;
    state->matmul_function = NULL;
    state->qtype_row_dot_function = NULL;
    state->deepseek_qtype_matvec_function = NULL;
    state->deepseek_decode_function = NULL;
    state->deepseek_weighted_norm_function = NULL;
    state->deepseek_unit_norm_function = NULL;
    state->deepseek_rope_function = NULL;
    state->deepseek_activation_function = NULL;
    state->deepseek_rolling_function = NULL;
    state->deepseek_topk_function = NULL;
    state->deepseek_reduce_function = NULL;
    state->mlp_function = NULL;
    state->attention_function = NULL;
    state->module_loaded = 0;
}

/* Contract: reports whether an explicit test-only failure selector matches. */
static int cuda_test_failure_matches(const char *name,
                                     yvex_backend_operation_variant variant)
{
    const char *value = getenv(name);

    return value && value[0] != '\0' &&
           (strcmp(value, "all") == 0 ||
            strcmp(value, yvex_backend_operation_variant_name(variant)) == 0);
}

/* Contract: returns the admitted function handle for one exact kernel variant. */
static CUfunction cuda_variant_function(const yvex_cuda_backend_state *state,
                                        yvex_backend_operation_variant variant)
{
    if (!state) {
        return NULL;
    }
    switch (variant) {
    case YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32: return state->embed_function;
    case YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32: return state->embed_f16_function;
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32: return state->rms_norm_f32_function;
    case YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16: return state->rms_norm_f16_function;
    case YVEX_BACKEND_VARIANT_ROPE_F32: return state->rope_function;
    case YVEX_BACKEND_VARIANT_MATMUL_F32: return state->matmul_function;
    case YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT:
        return state->qtype_row_dot_function;
    case YVEX_BACKEND_VARIANT_ATTENTION_ENCODED:
        return state->deepseek_qtype_matvec_function;
    case YVEX_BACKEND_VARIANT_MLP_DENSE_F32:
    case YVEX_BACKEND_VARIANT_MLP_ROUTED_F32: return state->mlp_function;
    case YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32:
    case YVEX_BACKEND_VARIANT_ATTENTION_NONCAUSAL_F32: return state->attention_function;
    case YVEX_BACKEND_VARIANT_TENSOR_ALLOC:
    case YVEX_BACKEND_VARIANT_TENSOR_ZERO:
    case YVEX_BACKEND_VARIANT_TENSOR_WRITE:
    case YVEX_BACKEND_VARIANT_TENSOR_READ:
    case YVEX_BACKEND_VARIANT_TENSOR_COPY:
    case YVEX_BACKEND_VARIANT_COUNT:
        return NULL;
    }
    return NULL;
}

/* Contract: classifies variants implemented entirely by the Driver memory API. */
static int cuda_variant_is_tensor(yvex_backend_operation_variant variant)
{
    return variant >= YVEX_BACKEND_VARIANT_TENSOR_ALLOC &&
           variant <= YVEX_BACKEND_VARIANT_TENSOR_COPY;
}

/* Contract: records one execution failure without changing unrelated variants. */
void yvex_cuda_capability_fail(yvex_backend *backend,
                               yvex_backend_operation_variant variant,
                               yvex_backend_capability_reason reason)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);

    if (!state || variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        return;
    }
    state->variant_failures[variant] = reason;
    if (reason == YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED ||
        reason == YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED) {
        state->backend_failure_reason = reason;
        backend->status = YVEX_BACKEND_STATUS_FAILED;
    }
}

#ifdef YVEX_HAVE_CUDA_KERNEL_PTX
/* Contract: unloads a partially admitted module and clears every cached handle. */
static void cuda_bundle_rollback(yvex_cuda_backend_state *state, CUmodule module)
{
    if (state && module && state->driver.cuModuleUnload) {
        (void)state->driver.cuModuleUnload(module);
    }
    cuda_bundle_clear_handles(state);
}

/* Contract: resolves one required function or returns a typed atomic-admission failure. */
static int cuda_resolve_required(yvex_cuda_backend_state *state,
                                 CUmodule module,
                                 const char *symbol,
                                 yvex_backend_operation_variant variant,
                                 CUfunction *out,
                                 yvex_error *err)
{
    const char *injected = getenv("YVEX_TEST_CUDA_BUNDLE_FAILURE");

    *out = NULL;
    if (injected &&
        (strcmp(injected, "symbol") == 0 ||
         strcmp(injected, yvex_backend_operation_variant_name(variant)) == 0)) {
        yvex_error_setf(err, YVEX_ERR_BACKEND, "cuda.kernels.resolve",
                        "required CUDA function unavailable: %s", symbol);
        return YVEX_ERR_BACKEND;
    }
    return yvex_cuda_status(&state->driver,
                            state->driver.cuModuleGetFunction(out, module, symbol),
                            "cuda.kernels.resolve", err);
}
#endif

/*
 * Contract: admits only generated kernels.cu PTX and commits no handle
 * until module load and every required symbol succeed. No tensor payload IO.
 */
int yvex_cuda_kernel_bundle_admit(yvex_backend *backend, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);

    if (!backend || !state || !state->context) {
        yvex_error_set(err, YVEX_ERR_STATE, "cuda.kernels.admit",
                       "CUDA context is required for kernel admission");
        return YVEX_ERR_STATE;
    }
    cuda_bundle_clear_handles(state);
    state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_ABSENT;
    state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT;
    state->kernel_bundle_failure_variant = YVEX_BACKEND_VARIANT_COUNT;

#ifndef YVEX_HAVE_CUDA_KERNEL_PTX
    yvex_error_set(err, YVEX_ERR_UNSUPPORTED, "cuda.kernels.admit",
                   "canonical CUDA kernel bundle was not built");
    return YVEX_ERR_UNSUPPORTED;
#else
    {
        CUmodule module = NULL;
        CUfunction embed_f32 = NULL;
        CUfunction embed_f16 = NULL;
        CUfunction rms_f32 = NULL;
        CUfunction rms_f16 = NULL;
        CUfunction rope = NULL;
        CUfunction matmul = NULL;
        CUfunction qtype_row_dot = NULL;
        CUfunction deepseek_qtype_matvec = NULL;
        CUfunction deepseek_decode = NULL;
        CUfunction deepseek_weighted_norm = NULL;
        CUfunction deepseek_unit_norm = NULL;
        CUfunction deepseek_rope = NULL;
        CUfunction deepseek_activation = NULL;
        CUfunction deepseek_rolling = NULL;
        CUfunction deepseek_topk = NULL;
        CUfunction deepseek_reduce = NULL;
        CUfunction mlp = NULL;
        CUfunction attention = NULL;
        const char *injected = getenv("YVEX_TEST_CUDA_BUNDLE_FAILURE");
        int rc;

        rc = yvex_cuda_set_current(backend, "cuda.kernels.admit", err);
        if (rc != YVEX_OK) {
            state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_REJECTED;
            state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_CONTEXT_UNAVAILABLE;
            return rc;
        }
        if (injected && strcmp(injected, "module") == 0) {
            yvex_error_set(err, YVEX_ERR_BACKEND, "cuda.kernels.load",
                           "injected CUDA module admission failure");
            rc = YVEX_ERR_BACKEND;
        } else {
            rc = yvex_cuda_status(&state->driver,
                                  state->driver.cuModuleLoadData(&module,
                                                                 yvex_cuda_kernels_ptx),
                                  "cuda.kernels.load", err);
        }
        if (rc != YVEX_OK) {
            state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_REJECTED;
            state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_REJECTED;
            cuda_bundle_rollback(state, module);
            return rc;
        }

#define YVEX_RESOLVE_REQUIRED(symbol_name, variant_id, slot) \
        do { \
            rc = cuda_resolve_required(state, module, symbol_name, variant_id, &slot, err); \
            if (rc != YVEX_OK) { \
                state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_REJECTED; \
                state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING; \
                state->kernel_bundle_failure_variant = variant_id; \
                cuda_bundle_rollback(state, module); \
                return rc; \
            } \
        } while (0)

        YVEX_RESOLVE_REQUIRED("yvex_embed_f32", YVEX_BACKEND_VARIANT_EMBED_F32_TO_F32,
                              embed_f32);
        YVEX_RESOLVE_REQUIRED("yvex_embed_f16_to_f32", YVEX_BACKEND_VARIANT_EMBED_F16_TO_F32,
                              embed_f16);
        YVEX_RESOLVE_REQUIRED("yvex_rms_norm_f32_weight_f32",
                              YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F32, rms_f32);
        YVEX_RESOLVE_REQUIRED("yvex_rms_norm_f32_weight_f16",
                              YVEX_BACKEND_VARIANT_RMS_NORM_F32_WEIGHT_F16, rms_f16);
        YVEX_RESOLVE_REQUIRED("yvex_rope_f32", YVEX_BACKEND_VARIANT_ROPE_F32, rope);
        YVEX_RESOLVE_REQUIRED("yvex_matmul_f32", YVEX_BACKEND_VARIANT_MATMUL_F32, matmul);
        YVEX_RESOLVE_REQUIRED("yvex_qtype_row_dot",
                              YVEX_BACKEND_VARIANT_QTYPE_ROW_DOT,
                              qtype_row_dot);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_qtype_matvec",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_qtype_matvec);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_decode",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_decode);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_weighted_norm",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_weighted_norm);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_unit_norm",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_unit_norm);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_rope",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_rope);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_activation",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_activation);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_rolling",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_rolling);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_topk",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_topk);
        YVEX_RESOLVE_REQUIRED("yvex_deepseek_reduce",
                              YVEX_BACKEND_VARIANT_ATTENTION_ENCODED,
                              deepseek_reduce);
        YVEX_RESOLVE_REQUIRED("yvex_mlp_f32", YVEX_BACKEND_VARIANT_MLP_DENSE_F32, mlp);
        YVEX_RESOLVE_REQUIRED("yvex_attention_f32",
                              YVEX_BACKEND_VARIANT_ATTENTION_CAUSAL_F32, attention);
#undef YVEX_RESOLVE_REQUIRED

        state->module = module;
        state->embed_function = embed_f32;
        state->embed_f16_function = embed_f16;
        state->rms_norm_f32_function = rms_f32;
        state->rms_norm_f16_function = rms_f16;
        state->rope_function = rope;
        state->matmul_function = matmul;
        state->qtype_row_dot_function = qtype_row_dot;
        state->deepseek_qtype_matvec_function = deepseek_qtype_matvec;
        state->deepseek_decode_function = deepseek_decode;
        state->deepseek_weighted_norm_function = deepseek_weighted_norm;
        state->deepseek_unit_norm_function = deepseek_unit_norm;
        state->deepseek_rope_function = deepseek_rope;
        state->deepseek_activation_function = deepseek_activation;
        state->deepseek_rolling_function = deepseek_rolling;
        state->deepseek_topk_function = deepseek_topk;
        state->deepseek_reduce_function = deepseek_reduce;
        state->mlp_function = mlp;
        state->attention_function = attention;
        state->module_loaded = 1;
        state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_ADMITTED;
        state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
        state->kernel_bundle_failure_variant = YVEX_BACKEND_VARIANT_COUNT;
        yvex_error_clear(err);
        return YVEX_OK;
    }
#endif
}

/* Contract: unloads the admitted module, reports unload failure, and always clears handles. */
int yvex_cuda_kernel_bundle_close(yvex_backend *backend, yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc = YVEX_OK;

    if (!backend || !state) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (state->module_loaded && state->module) {
        rc = yvex_cuda_status(&state->driver,
                              state->driver.cuModuleUnload(state->module),
                              "cuda.kernels.unload", err);
    }
    cuda_bundle_clear_handles(state);
    state->kernel_bundle_state = YVEX_CUDA_KERNEL_BUNDLE_ABSENT;
    if (rc != YVEX_OK) {
        state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED;
        backend->status = YVEX_BACKEND_STATUS_FAILED;
        return rc;
    }
    state->kernel_bundle_reason = YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: projects one exact variant without loading modules or executing work. */
int yvex_cuda_query_capability(const yvex_backend *backend,
                               yvex_backend_operation_variant variant,
                               yvex_backend_capability_result *out,
                               yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    CUfunction function;

    if (!backend || !state || !out) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.query_capability",
                       "backend, CUDA state, and out are required");
        return YVEX_ERR_INVALID_ARG;
    }
    out->context_available = state->context != NULL;
    out->kernel_bundle_available =
        state->kernel_bundle_state == YVEX_CUDA_KERNEL_BUNDLE_ADMITTED;
    if (!state->context) {
        out->state = YVEX_BACKEND_CAPABILITY_UNSUPPORTED;
        out->reason = YVEX_BACKEND_CAPABILITY_REASON_CONTEXT_UNAVAILABLE;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (variant < 0 || variant >= YVEX_BACKEND_VARIANT_COUNT) {
        yvex_error_set(err, YVEX_ERR_INVALID_ARG, "cuda.query_capability",
                       "operation variant is out of range");
        return YVEX_ERR_INVALID_ARG;
    }
    if (backend->status == YVEX_BACKEND_STATUS_FAILED &&
        state->backend_failure_reason != YVEX_BACKEND_CAPABILITY_REASON_NONE) {
        out->state = YVEX_BACKEND_CAPABILITY_FAILED;
        out->reason = state->backend_failure_reason;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (state->variant_failures[variant] != YVEX_BACKEND_CAPABILITY_REASON_NONE) {
        out->state = YVEX_BACKEND_CAPABILITY_FAILED;
        out->reason = state->variant_failures[variant];
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (cuda_variant_is_tensor(variant)) {
        out->state = YVEX_BACKEND_CAPABILITY_SUPPORTED;
        out->reason = YVEX_BACKEND_CAPABILITY_REASON_NONE;
        out->function_available = 1;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (state->kernel_bundle_state == YVEX_CUDA_KERNEL_BUNDLE_ABSENT) {
        out->state = YVEX_BACKEND_CAPABILITY_UNSUPPORTED;
        out->reason = YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_ABSENT;
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (state->kernel_bundle_state == YVEX_CUDA_KERNEL_BUNDLE_REJECTED) {
        out->state = YVEX_BACKEND_CAPABILITY_FAILED;
        out->reason = state->kernel_bundle_reason;
        if (state->kernel_bundle_reason == YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING &&
            state->kernel_bundle_failure_variant != variant) {
            out->reason = YVEX_BACKEND_CAPABILITY_REASON_KERNEL_BUNDLE_REJECTED;
        }
        yvex_error_clear(err);
        return YVEX_OK;
    }
    function = cuda_variant_function(state, variant);
    out->function_available = function != NULL;
    out->state = function ? YVEX_BACKEND_CAPABILITY_SUPPORTED
                          : YVEX_BACKEND_CAPABILITY_FAILED;
    out->reason = function ? YVEX_BACKEND_CAPABILITY_REASON_NONE
                           : YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING;
    yvex_error_clear(err);
    return YVEX_OK;
}

/* Contract: refuses an unsupported or failed variant before any CUDA dispatch. */
int yvex_cuda_require_capability(yvex_backend *backend,
                                 yvex_backend_operation_variant variant,
                                 const char *where,
                                 yvex_error *err)
{
    yvex_backend_capability_result result;
    int rc = yvex_backend_query_capability(backend, variant, &result, err);

    if (rc != YVEX_OK) {
        return rc;
    }
    if (result.state == YVEX_BACKEND_CAPABILITY_SUPPORTED) {
        return YVEX_OK;
    }
    yvex_error_setf(err,
                    result.state == YVEX_BACKEND_CAPABILITY_FAILED
                        ? YVEX_ERR_BACKEND : YVEX_ERR_UNSUPPORTED,
                    where ? where : "cuda.require_capability",
                    "%s refused: %s",
                    yvex_backend_operation_variant_name(variant),
                    yvex_backend_capability_reason_name(result.reason));
    return result.state == YVEX_BACKEND_CAPABILITY_FAILED
               ? YVEX_ERR_BACKEND : YVEX_ERR_UNSUPPORTED;
}

/* Contract: launches one admitted function and records launch failure atomically. */
int yvex_cuda_launch(yvex_backend *backend,
                     yvex_backend_operation_variant variant,
                     CUfunction function,
                     unsigned int grid_x,
                     unsigned int block_x,
                     unsigned int shared_bytes,
                     void **params,
                     const char *where,
                     yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    rc = yvex_cuda_require_capability(backend, variant, where, err);
    if (rc != YVEX_OK) {
        return rc;
    }
    if (!state || !function) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_FUNCTION_MISSING);
        yvex_error_set(err, YVEX_ERR_BACKEND, where,
                       "admitted CUDA function handle is missing");
        return YVEX_ERR_BACKEND;
    }
    if (cuda_test_failure_matches("YVEX_TEST_CUDA_LAUNCH_FAILURE", variant)) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED);
        yvex_error_set(err, YVEX_ERR_BACKEND, where,
                       "injected CUDA launch failure");
        return YVEX_ERR_BACKEND;
    }
    rc = yvex_cuda_status(&state->driver,
                          state->driver.cuLaunchKernel(function,
                                                       grid_x, 1, 1,
                                                       block_x, 1, 1,
                                                       shared_bytes, NULL,
                                                       params, NULL),
                          where, err);
    if (rc != YVEX_OK) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_LAUNCH_FAILED);
    }
    return rc;
}

/* Contract: synchronizes one variant and demotes it on any synchronization failure. */
int yvex_cuda_synchronize(yvex_backend *backend,
                          yvex_backend_operation_variant variant,
                          const char *where,
                          yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (cuda_test_failure_matches("YVEX_TEST_CUDA_SYNC_FAILURE", variant)) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED);
        yvex_error_set(err, YVEX_ERR_BACKEND, where,
                       "injected CUDA synchronization failure");
        return YVEX_ERR_BACKEND;
    }
    rc = yvex_cuda_status(&state->driver, state->driver.cuCtxSynchronize(), where, err);
    if (rc != YVEX_OK) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_SYNCHRONIZATION_FAILED);
    }
    return rc;
}

/* Contract: releases one raw temporary allocation and surfaces cleanup failure. */
int yvex_cuda_temporary_free(yvex_backend *backend,
                             yvex_backend_operation_variant variant,
                             CUdeviceptr ptr,
                             const char *where,
                             yvex_error *err)
{
    yvex_cuda_backend_state *state = yvex_cuda_state(backend);
    int rc;

    if (!ptr) {
        yvex_error_clear(err);
        return YVEX_OK;
    }
    if (!state) {
        yvex_error_set(err, YVEX_ERR_STATE, where, "CUDA backend state is missing");
        return YVEX_ERR_STATE;
    }
    if (cuda_test_failure_matches("YVEX_TEST_CUDA_CLEANUP_FAILURE", variant)) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED);
        yvex_error_set(err, YVEX_ERR_BACKEND, where,
                       "injected CUDA temporary cleanup failure");
        return YVEX_ERR_BACKEND;
    }
    rc = yvex_cuda_status(&state->driver, state->driver.cuMemFree_v2(ptr), where, err);
    if (rc != YVEX_OK) {
        yvex_cuda_capability_fail(backend, variant,
                                  YVEX_BACKEND_CAPABILITY_REASON_CLEANUP_FAILED);
    }
    return rc;
}
