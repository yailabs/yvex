# YVEX Backend Contract

This document owns backend architecture. CPU reference backend behavior exists
as of G0; accelerated backends remain planned.

## Status

```text
CPU backend: implemented in G0
CUDA backend: not implemented
Metal backend: not implemented
ROCm backend: not implemented
backend public headers: implemented in include/yvex/backend.h
```

No backend support claim exists without implementation, tests, command-visible
behavior, documented limitations, and a clear unsupported path.

## Backend Kinds

Planned backend kinds:

```text
cpu
cuda
metal
rocm
```

Initial priority:

```text
CPU reference first for correctness
CUDA/DGX Spark first for serious acceleration
Metal/ROCm later
```

## Backend Responsibilities

```text
init/destroy
device selection
capability reporting
memory stats
tensor allocation/free
host-to-device write
device-to-host read
op dispatch
sync
trace hooks
error mapping
```

## Backend ABI

The core runtime calls an opaque backend vtable. Backend-specific native handles
stay inside backend implementation files. G0 implements the ABI with a CPU
reference backend.

Implemented G0 operations:

```text
open
close
alloc
free
write
read
copy
sync
capabilities
memory_stats
op_embed
```

Generic public headers must not expose CUDA, Metal, ROCm, or provider-native
types.

## Device Tensor Contract

```text
yvex_backend_alloc creates a yvex_device_tensor
yvex_backend_tensor_free destroys it
device tensor belongs to exactly one backend
device tensor records bytes, residency, alignment, debug name, and backend handle
core code treats backend handle as opaque
freeing with a different backend is invalid state
```

Device tensor states:

```text
allocated
written
ready
failed
freed
```

## CPU Reference Backend

CPU reference exists for correctness and fixtures. G0 implements:

```text
tensor allocation
tensor read/write
tensor copy
memory stats
capability reporting
F32 embedding lookup
sync no-op
```

CPU reference does not need to be fast. It must be deterministic, inspectable,
and useful for CUDA parity.

G0 does not implement:

```text
matmul
rms norm
attention
sampler
graph execution
session runtime
inference
```

## CUDA / DGX Spark Track

Primary CUDA target:

```text
NVIDIA DGX Spark
GB10
sm_121 when supported by the toolchain
CUDA
128 GB memory class
```

Future CUDA commands:

```text
yvex cuda-info
yvex run --model model.gguf --backend cuda ...
yvex bench --model model.gguf --backend cuda ...
```

Future CUDA build targets:

```text
make cuda
make cuda-spark
make cuda-debug
```

These targets must not be added as support claims before backend code exists.

CUDA stream policy:

```text
one default execution stream per yvex_backend initially
separate copy stream only with explicit sync edges
no implicit cross-session stream sharing until multi-session policy exists
all stream creation/destruction is owned by backend_cuda
```

cuBLAS ownership:

```text
backend_cuda owns cublasHandle_t
handle lifetime equals yvex_backend
handle is bound to backend execution stream
cuda-info reports cuBLAS availability only after implementation
planner records cuBLAS vs custom kernel per matmul op
```

CUDA error mapping:

```text
cudaErrorMemoryAllocation -> YVEX_ERR_BACKEND with memory context
cudaErrorInvalidValue -> YVEX_ERR_INVALID_ARG or YVEX_ERR_BACKEND
cudaErrorNotSupported -> YVEX_ERR_UNSUPPORTED
cudaErrorLaunchFailure -> YVEX_ERR_BACKEND with kernel name
cudaErrorIllegalAddress -> YVEX_ERR_BACKEND with sync boundary and kernel name
unknown CUDA error -> YVEX_ERR_BACKEND with numeric code and cudaGetErrorString
```

## Capability Matrix

```text
op                | cpu     | cuda    | notes
tensor_alloc      | yes     | planned | CPU roundtrip tests exist
tensor_copy       | yes     | planned | CPU copy tests exist
op_embed          | yes     | planned | F32 fixture op exists
rms_norm          | planned | planned | CPU reference first
matmul_f16        | planned | planned | CUDA may use cuBLAS
matmul_q8_0       | planned | planned | dequant path required
matmul_q4_k       | planned | planned | qtype fixture required
attention_prefill | planned | planned | tiny fixture first
attention_decode  | planned | planned | KV fixture first
sampler           | planned | planned | deterministic parity
moe_router        | later   | later   | MoE fixture required
```

## CPU/CUDA Parity Rules

```text
same fixture inputs
same seed when sampling is involved
documented tolerance per op
CPU output stored or generated as reference
CUDA output compared only after sync
failure reports max absolute and relative difference
```

Every CUDA op requires a CPU reference, fixed fixture, tolerance, test command,
timing, and memory measurement before it is trusted.
