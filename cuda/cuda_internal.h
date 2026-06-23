/*
 * cuda/cuda_internal.h - CUDA Driver API bridge declarations.
 *
 * The public YVEX API remains plain C and never exposes CUDA native types.
 */
#ifndef YVEX_CUDA_INTERNAL_H
#define YVEX_CUDA_INTERNAL_H

#include "yvex_backend_private.h"

#include <stddef.h>

typedef int CUresult;
typedef int CUdevice;
typedef void *CUcontext;
typedef void *CUmodule;
typedef void *CUfunction;
typedef unsigned long long CUdeviceptr;

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
    int module_loaded;
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

int yvex_cuda_tensor_alloc(yvex_backend *backend,
                           const yvex_backend_tensor_desc *desc,
                           yvex_device_tensor **out,
                           yvex_error *err);
void yvex_cuda_tensor_free(yvex_backend *backend, yvex_device_tensor *tensor);
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

#endif /* YVEX_CUDA_INTERNAL_H */
