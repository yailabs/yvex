<!-- docs:metadata
title: Backend and Device Execution
id: yvex.architecture.backend-execution
document: architecture-plane
status: mixed
owner: backend
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
plane: backend-execution
related: [yvex.architecture.deployment-specialization, yvex.architecture.generation-decode]
-->

# Backend and Device Execution

**Execute admitted work; do not rediscover model semantics in kernels.**

[Up](README.md)

CPU and CUDA implement admitted operations, physical representations and
numerical classes. The compiler supplies the legal program. Runtime owns
session/transaction lifetime. Backend owns device allocation, submission,
synchronization and equivalent launch details.

## Execution path

Authenticated program + real batch/worklist → admitted implementation → device
buffers and kernels → checked synchronization → staged result → runtime commit.

Unsupported mandatory semantics fail closed. A faster kernel is not evidence of
faster model execution; compare complete workload latency, preparation, memory
and numerical effect using the [benchmark methodology](../evaluation/benchmarks/methodology.md).

## Backend boundary

Upstream supplies legal operations, package representation, numerical
obligations, specialization implementation class, real populations, and
publication provenance. CUDA owns equivalent implementation details: kernel
entrypoint inside the admitted class, tile/warp/grid geometry, shared-memory and
register strategy, stream/event mechanics, and graph capture/replay.

Synchronous CUDA tensor copies, host input/output and zeroing enqueue on the
owning execution stream before waiting for completion. Waiting only after a
default-stream transfer does not order it with a producer or previous storage
user on a nonblocking session stream. Synchronous host input/output refuses
active graph capture: caller-owned host storage cannot be borrowed for later
replay, and host output cannot be published before execution. Standalone operation
status storage likewise has its own lifetime: it cannot consume or rewind an
enclosing executor's temporary arena. Shared status transactions retain their
explicit begin/completion owner.

CUDA does not branch on a family name, recover expert compatibility, select a
numerically different activation representation, or reconstruct a missing
physical computational or package plan from dimensions. An explicit CUDA
request refuses when no admitted implementation exists. `auto` may retry only
an already-admitted numerically equivalent strategy.

Decoded-input grouped and paired projections retain the ordinary projection's
source-order F64 accumulation and final F32/BF16 publication. Grouping changes launch
topology, not the numerical class; a finite F32 warp reduction is not an
equivalent substitute. Q8 activation keeps its separately admitted reduction.
Non-finite results still refuse through the device-status completion owner.

Exact MiniMax output-linear requirements remain source/package numerical facts.
Runtime component specialization resolves them to exact generic linear
execution records; generic CUDA consumes those records without MiniMax switches
or magic shape recognition.


## Implementation and evidence

[src/backend](../../src/backend) · [src/backend/cuda](../../src/backend/cuda) · [include/yvex/backend.h](../../include/yvex/backend.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Deployment and Specialization](deployment-specialization.md) · [Generation and Decoding](generation-decode.md)
