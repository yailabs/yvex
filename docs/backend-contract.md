# YVEX Backend Contract

This document owns backend architecture. CPU reference backend behavior exists
as of G0; CUDA attachment exists as of L0.

## Status

```text
CPU backend: implemented in G0
CUDA backend: tensor allocation/read/write/copy and F32 embed parity implemented in L0 when driver/device are available
Weight materialization: fixture tensor bytes copied into CPU/CUDA backend tensors in M0
Metal backend: not implemented
ROCm backend: not implemented
backend public headers: implemented in include/yvex/backend.h
```

No backend support claim exists without implementation, tests, command-visible
behavior, documented limitations, and a clear unsupported path.

Conversion qtype support is not backend compute support. `yvex qtype-support`
may report a qtype as policy/storage/emit supported while compute remains `no`
or partial. Backend capability claims require backend tests and command proof;
conversion emit alone does not make a tensor executable.

## Backend Kinds

Backend kinds:

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
reference backend. L0 attaches CUDA through the same ABI.

Implemented generic operations:

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
device_info
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

L0 CUDA status:

```text
driver/device probe: implemented through CUDA Driver API
cuda-info command: implemented
tensor allocation/free: implemented
host-to-device write: implemented
device-to-host read: implemented
device-to-device copy: implemented
sync: implemented
F32 embed op: implemented
CPU/CUDA parity: implemented for F32 embed
```

Primary future CUDA target remains:

```text
NVIDIA DGX Spark / GB10
architecture selected by toolchain/probe, not hardcoded in docs
```

Implemented CUDA commands:

```text
yvex cuda-info
yvex backend cuda
yvex plan <file> --backend cuda
```

Implemented CUDA build targets:

```text
make cuda-info
make cuda
make test-cuda
make smoke-cuda
make check-cuda
```

Normal `make check` is CPU-safe and does not require CUDA hardware.

CUDA stream policy:

```text
one default execution stream per yvex_backend initially
separate copy stream only with explicit sync edges
no implicit cross-session stream sharing until multi-session policy exists
all stream creation/destruction is owned by backend_cuda
```

cuBLAS ownership:

```text
not implemented in L0
future backend_cuda owns cublasHandle_t only when matmul work begins
cuda-info reports cuBLAS availability only after implementation
planner records cuBLAS vs custom kernel per matmul op
```

CUDA error mapping:

```text
CUDA out-of-memory -> YVEX_ERR_NOMEM
CUDA invalid value -> YVEX_ERR_INVALID_ARG or YVEX_ERR_BACKEND by call site
CUDA no device / not supported -> YVEX_ERR_UNSUPPORTED
CUDA launch/sync/runtime failure -> YVEX_ERR_BACKEND
unknown CUDA error -> YVEX_ERR_BACKEND with numeric code and error string
```

## Capability Matrix

```text
op                | cpu     | cuda    | notes
tensor_alloc      | yes     | yes     | CPU and CUDA tests exist
tensor_copy       | yes     | yes     | CPU and CUDA copy tests exist
op_embed          | yes     | yes     | F32 CPU/CUDA parity exists
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

## Weight Materialization Contract

M0 introduces backend residency for parsed GGUF tensor bytes.

Implemented:

```text
artifact tensor byte range check
backend tensor allocation from YVEX tensor table rows
full-buffer tensor write
materialized weight table ownership
CPU fixture materialization
CUDA fixture materialization under check-cuda
backend allocation cleanup on weight table close
```

Rules:

```text
storage_bytes comes from the dtype registry
absolute_offset comes from the parsed GGUF tensor directory
artifact range must be valid before backend write
weight table owns backend tensors
backend must outlive the weight table
closing the weight table frees backend allocations
materialization does not imply graph execution
execution_ready remains false
```

M0 does not materialize external large models, implement model support levels,
execute graph ops, allocate KV cache, compute logits, sample tokens, or claim
inference.
