# YVEX CUDA-First Strategy

This document extracts the CUDA/DGX strategy from `docs/spine.md`. The spine
remains authoritative.

## Primary Target

```text
NVIDIA DGX Spark
GB10
sm_121
CUDA
128 GB memory class
```

CPU reference remains mandatory for correctness. CUDA is the serious
acceleration path.

## Commands

```sh
yvex cuda-info
yvex run --model model.gguf --backend cuda ...
yvex bench --model model.gguf --backend cuda ...
```

## Build Targets

```text
make cuda
make cuda-spark
make cuda-debug
```

These are future targets until backend code exists.

## Device Report

```text
CUDA
  device        NVIDIA GB10
  sm            121
  driver        ...
  runtime       ...
  total memory  ...
  free memory   ...
  cublas        available
  managed       yes/no/unknown
```

## Stream Policy

```text
one default execution stream per yvex_backend initially
separate copy stream only with explicit sync edges
no implicit cross-session stream sharing until multi-session policy exists
all stream creation/destruction is owned by backend_cuda
```

## cuBLAS Ownership

```text
backend_cuda owns cublasHandle_t
handle lifetime equals yvex_backend
handle is bound to backend execution stream
cuda-info reports cuBLAS availability
planner records cuBLAS vs custom kernel per matmul op
```

## Error Mapping

```text
cudaErrorMemoryAllocation -> YVEX_ERR_BACKEND with memory context
cudaErrorInvalidValue -> YVEX_ERR_INVALID_ARG or YVEX_ERR_BACKEND
cudaErrorNotSupported -> YVEX_ERR_UNSUPPORTED
cudaErrorLaunchFailure -> YVEX_ERR_BACKEND with kernel name
cudaErrorIllegalAddress -> YVEX_ERR_BACKEND with sync boundary and kernel name
unknown CUDA error -> YVEX_ERR_BACKEND with numeric code and cudaGetErrorString
```

## Compile Target

DGX Spark / GB10 uses `sm_121` when the toolchain supports it. Unsupported
compiler/architecture combinations fail with a clear build or `cuda-info`
message. Fatbin/multi-arch support is future work.

## Implementation Phases

```text
CUDA.0 device init + memory stats
CUDA.1 tensor alloc/write/read/copy
CUDA.2 vector kernels
CUDA.3 rmsnorm/silu/softmax/argmax
CUDA.4 F16/BF16 matmul via cuBLAS
CUDA.5 Q8_0 dequant/matmul
CUDA.6 Q4_K dequant/matmul
CUDA.7 attention prefill/decode
CUDA.8 KV cache GPU
CUDA.9 sampler helper
CUDA.10 MoE router
CUDA.11 expert cache/residency
```

Every CUDA op requires a CPU reference, fixed fixture, tolerance, test command,
timing, and memory measurement.
