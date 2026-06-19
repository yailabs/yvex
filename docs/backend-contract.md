# YVEX Backend Contract

This document extracts the backend contract from `docs/spine.md`. The spine
remains authoritative.

## Backend Kinds

```text
cpu
cuda
metal
rocm
```

Only CPU reference and CUDA-first acceleration are implementation priorities in
the initial plan.

## Responsibilities

```text
init/destroy
device selection
memory stats
tensor allocation/free
host-to-device write
device-to-host read
op dispatch
sync
trace hooks
capability reporting
```

## Device Tensor Ownership

```text
yvex_backend_alloc creates yvex_device_tensor
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

## Sync Policy

```text
CPU backend may be synchronous
CUDA backend may enqueue async copies internally
public API is complete only after explicit sync or documented command boundary
CLI commands sync before final success
trace events distinguish enqueue time from sync completion time where possible
```

## CPU Reference

CPU reference exists for correctness:

```text
tensor allocation
tensor read/write
vector ops
rmsnorm reference
matmul baseline
sampler reference
tiny graph execution
one-token fixture inference
```

CPU reference does not need to be fast. It must be correct, deterministic, and
useful for CUDA parity.

## Capability Matrix

```text
op                | cpu | cuda | notes
tensor_alloc      | yes | planned | A/G before L
tensor_copy       | yes | planned | roundtrip fixture required
rms_norm          | planned | planned | CPU reference first
matmul_f16        | planned | planned | CUDA may use cuBLAS
matmul_q8_0       | planned | planned | dequant path required
matmul_q4_k       | planned | planned | qtype fixture required
attention_prefill | planned | planned | tiny fixture first
attention_decode  | planned | planned | KV fixture first
sampler           | planned | planned | deterministic parity
moe_router        | planned | planned | later MoE fixture
```

## CPU/CUDA Parity

```text
same fixture inputs
same seed when sampling is involved
documented tolerance per op
CPU output stored or generated as reference
CUDA output compared after sync
failure reports max absolute and relative difference
```

No backend support claim exists without implementation, tests, CLI-visible
behavior, and a clear unsupported path.
