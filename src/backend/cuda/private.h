/* Owner: backend.cuda.private (backend.cuda).
 * Owns: CUDA driver ABI, generated kernel bundle, and qtype launch contracts.
 * Does not own: model policy, graph topology, or CPU fallback.
 * Invariants: declarations have one owner, stable ordering, and no hidden capability promotion.
 * Boundary: CUDA platform-private execution.
 * Purpose: provide the canonical CUDA platform-private execution contract.
 * Inputs: typed immutable facts and explicitly owned mutable lifecycle objects.
 * Effects: only declared lifecycle, allocation, I/O, and publication operations mutate state.
 * Failure: typed refusals leave outputs defined and preserve caller-owned state. */
#ifndef SRC_BACKEND_CUDA_PRIVATE_H_INCLUDED
#define SRC_BACKEND_CUDA_PRIVATE_H_INCLUDED

#include <stddef.h>
#include <yvex/backend.h>
#include <yvex/internal/backend.h>
#include <yvex/internal/core.h>
#include <yvex/internal/quant_numeric.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Driver contract. */
typedef int CUresult;
typedef int CUdevice;
typedef void *CUcontext;
typedef void *CUmodule;
typedef void *CUfunction;
typedef unsigned long long CUdeviceptr;
typedef enum {
    YVEX_CUDA_KERNEL_BUNDLE_ABSENT = 0,
    YVEX_CUDA_KERNEL_BUNDLE_ADMITTED,
    YVEX_CUDA_KERNEL_BUNDLE_REJECTED
} yvex_cuda_kernel_bundle_state;
#define YVEX_CUDA_SUCCESS 0
#define YVEX_CUDA_ERROR_INVALID_VALUE 1
#define YVEX_CUDA_ERROR_OUT_OF_MEMORY 2
#define YVEX_CUDA_ERROR_NOT_INITIALIZED 3
#define YVEX_CUDA_ERROR_NO_DEVICE 100
#define YVEX_CUDA_ERROR_NOT_SUPPORTED 801
#define YVEX_CUDA_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING 41
#define YVEX_CUDA_DEVICE_ATTRIBUTE_MANAGED_MEMORY 83
typedef struct {
    void *library;
    CUresult (*cuInit)(unsigned int flags);
    CUresult (*cuDriverGetVersion)(int *driverVersion);
    CUresult (*cuDeviceGetCount)(int *count);
    CUresult (*cuDeviceGet)(CUdevice *device, int ordinal);
    CUresult (*cuDeviceGetName)(char *name, int len, CUdevice dev);
    CUresult (*cuDeviceComputeCapability)(int *major, int *minor, CUdevice dev);
    CUresult (*cuDeviceTotalMem_v2)(size_t *bytes, CUdevice dev);
    CUresult (*cuDeviceGetAttribute)(int *pi, int attrib, CUdevice dev);
    CUresult (*cuCtxCreate_v2)(CUcontext *pctx, unsigned int flags, CUdevice dev);
    CUresult (*cuCtxDestroy_v2)(CUcontext ctx);
    CUresult (*cuCtxSetCurrent)(CUcontext ctx);
    CUresult (*cuCtxSynchronize)(void);
    CUresult (*cuMemGetInfo_v2)(size_t *free_bytes, size_t *total_bytes);
    CUresult (*cuMemAlloc_v2)(CUdeviceptr *dptr, size_t bytesize);
    CUresult (*cuMemFree_v2)(CUdeviceptr dptr);
    CUresult (*cuMemsetD8_v2)(CUdeviceptr dstDevice, unsigned char uc, size_t n);
    CUresult (*cuMemcpyHtoD_v2)(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
    CUresult (*cuMemcpyDtoH_v2)(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
    CUresult (*cuMemcpyDtoD_v2)(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);
    CUresult (*cuModuleLoadData)(CUmodule *module, const void *image);
    CUresult (*cuModuleGetFunction)(CUfunction *hfunc, CUmodule hmod, const char *name);
    CUresult (*cuModuleUnload)(CUmodule hmod);
    CUresult (*cuLaunchKernel)(CUfunction f,
                               unsigned int gridDimX,
                               unsigned int gridDimY,
                               unsigned int gridDimZ,
                               unsigned int blockDimX,
                               unsigned int blockDimY,
                               unsigned int blockDimZ,
                               unsigned int sharedMemBytes,
                               void *hStream,
                               void **kernelParams,
                               void **extra);
    CUresult (*cuGetErrorName)(CUresult error, const char **pStr);
    CUresult (*cuGetErrorString)(CUresult error, const char **pStr);
} yvex_cuda_driver;
typedef struct {
    yvex_cuda_driver driver;
    CUcontext context;
    CUdevice device;
    int device_index;
    int driver_version;
    CUmodule module;
    CUfunction embed_function;
    CUfunction embed_f16_function;
    CUfunction rms_norm_f32_function;
    CUfunction rms_norm_f16_function;
    CUfunction rope_function;
    CUfunction matmul_function;
    CUfunction qtype_row_dot_function;
    CUfunction deepseek_qtype_matvec_function;
    CUfunction deepseek_decode_function;
    CUfunction deepseek_weighted_norm_function;
    CUfunction deepseek_unit_norm_function;
    CUfunction deepseek_rope_function;
    CUfunction deepseek_activation_function;
    CUfunction deepseek_rolling_function;
    CUfunction deepseek_topk_function;
    CUfunction deepseek_reduce_function;
    CUfunction mlp_function;
    CUfunction attention_function;
    int module_loaded;
    yvex_cuda_kernel_bundle_state kernel_bundle_state;
    yvex_backend_capability_reason kernel_bundle_reason;
    yvex_backend_operation_variant kernel_bundle_failure_variant;
    yvex_backend_capability_reason backend_failure_reason;
    yvex_backend_capability_reason variant_failures[YVEX_BACKEND_VARIANT_COUNT];
} yvex_cuda_backend_state;
int yvex_cuda_driver_load(yvex_cuda_driver *driver, yvex_error *err);
void yvex_cuda_driver_unload(yvex_cuda_driver *driver);
int yvex_cuda_status(const yvex_cuda_driver *driver,
                     CUresult code,
                     const char *where,
                     yvex_error *err);
yvex_cuda_backend_state *yvex_cuda_state(const yvex_backend *backend);
int yvex_cuda_set_current(const yvex_backend *backend, const char *where, yvex_error *err);
int yvex_cuda_refresh_memory_info(yvex_backend *backend, yvex_error *err);
CUdeviceptr yvex_cuda_tensor_ptr(const yvex_device_tensor *tensor);
int yvex_cuda_kernel_bundle_admit(yvex_backend *backend, yvex_error *err);
int yvex_cuda_kernel_bundle_close(yvex_backend *backend, yvex_error *err);
int yvex_cuda_query_capability(const yvex_backend *backend,
                               yvex_backend_operation_variant variant,
                               yvex_backend_capability_result *out,
                               yvex_error *err);
int yvex_cuda_require_capability(yvex_backend *backend,
                                 yvex_backend_operation_variant variant,
                                 const char *where,
                                 yvex_error *err);
int yvex_cuda_launch(yvex_backend *backend,
                     yvex_backend_operation_variant variant,
                     CUfunction function,
                     unsigned int grid_x,
                     unsigned int block_x,
                     unsigned int shared_bytes,
                     void **params,
                     const char *where,
                     yvex_error *err);
int yvex_cuda_synchronize(yvex_backend *backend,
                          yvex_backend_operation_variant variant,
                          const char *where,
                          yvex_error *err);
int yvex_cuda_temporary_free(yvex_backend *backend,
                             yvex_backend_operation_variant variant,
                             CUdeviceptr ptr,
                             const char *where,
                             yvex_error *err);
int yvex_cuda_tensor_alloc(yvex_backend *backend,
                           const yvex_backend_tensor_desc *desc,
                           yvex_device_tensor **out,
                           yvex_error *err);
int yvex_cuda_tensor_free(yvex_backend *backend,
                          yvex_device_tensor *tensor,
                          yvex_error *err);
int yvex_cuda_tensor_write(yvex_backend *backend,
                           yvex_device_tensor *tensor,
                           const void *src,
                           unsigned long long len,
                           yvex_error *err);
int yvex_cuda_tensor_read(yvex_backend *backend,
                          const yvex_device_tensor *tensor,
                          void *dst,
                          unsigned long long len,
                          yvex_error *err);
int yvex_cuda_tensor_copy(yvex_backend *backend,
                          yvex_device_tensor *dst,
                          const yvex_device_tensor *src,
                          yvex_error *err);
int yvex_cuda_op_embed(yvex_backend *backend,
                       const yvex_device_tensor *embedding,
                       const unsigned int *token_ids,
                       unsigned long long token_count,
                       yvex_device_tensor *out,
                       yvex_error *err);
int yvex_cuda_op_rms_norm(yvex_backend *backend,
                          const yvex_device_tensor *input,
                          const yvex_device_tensor *weight,
                          float epsilon,
                          yvex_device_tensor *out,
                          yvex_error *err);
int yvex_cuda_op_rope(yvex_backend *backend,
                      const yvex_device_tensor *input,
                      unsigned long long position,
                      float rope_base,
                      yvex_device_tensor *out,
                      yvex_error *err);
int yvex_cuda_op_matmul(yvex_backend *backend,
                        const yvex_device_tensor *input,
                        const yvex_device_tensor *weight,
                        yvex_device_tensor *out,
                        yvex_error *err);
int yvex_cuda_op_mlp(yvex_backend *backend,
                     const yvex_device_tensor *input,
                     const yvex_device_tensor *gate_weight,
                     const yvex_device_tensor *up_weight,
                     const yvex_device_tensor *down_weight,
                     const yvex_mlp_options *options,
                     yvex_device_tensor *intermediate,
                     yvex_device_tensor *out,
                     yvex_error *err);
int yvex_cuda_op_attention(yvex_backend *backend,
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

/* Qtype contract. */
typedef struct {
    const char *qtype;
    const char *status;
    const char *reason;
} yvex_cuda_qtype_fact;
void yvex_cuda_qtype_refuse(yvex_cuda_qtype_fact *fact, const char *qtype);
int yvex_cuda_quant_row_dot(yvex_backend *backend,
                            unsigned int qtype,
                            const unsigned char *encoded,
                            size_t encoded_bytes,
                            const float *vector,
                            unsigned long long elements,
                            float *out,
                            yvex_quant_failure *failure,
                            yvex_error *err);

#ifdef __cplusplus
}
#endif

#endif /* SRC_BACKEND_CUDA_PRIVATE_H_INCLUDED */
