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

CPU and CUDA implement admitted model operations, physical representations and
numerical classes. Metal is integrated in main as an early Apple Silicon backend
with the qualified device, shared-storage and F32-embedding foundation below.
The compiler supplies the legal program. Runtime owns session/transaction
lifetime. Backend owns device allocation, submission, synchronization and
equivalent launch details. [Combined-source qualification](../evaluation/macos-main-integration.md)
binds the integrated Rust shell, native CPU paths and Metal primitive evidence;
full-model Metal admission remains open.

## Native platform mechanisms

CPU execution is independent of Linux process and filesystem APIs. The
cross-subsystem internal `core.platform` owner supplies native stat timestamps,
preallocation, exclusive rename, Unix peer credentials, process identity,
executable discovery, process I/O and memory observations. Admission, reserve
policy, compiler semantics and state accounting remain with their consumers.
Darwin uses libproc/sysctl and Mach; Linux retains procfs, cgroup admission,
sealed memfd and mremap.

Darwin prefix capture fills a private temporary file, opens a matching read-only
descriptor, unlinks it, and closes the writer before publication. It offers
process-owned immutable backing rather than Linux kernel seals. Prefix attach
uses fixed Mach VM remap after validating aligned, non-overlapping extents;
sparse protections and session COW ownership remain authoritative. Replacement
failure preserves the owned target. Core path traversal permits only the
root-owned Darwin `/tmp` and `/var` system aliases, then refuses application
symlinks and dot components.

The local listener is nonblocking and polled at bounded intervals so shutdown
is observed without depending on cross-thread `close()` waking `accept()`.
Accepted streams return to blocking protocol I/O. CLI `serve` owns blocked
SIGINT/SIGTERM dispositions even when launched from a shell background job;
the library does not change process signal policy.

[Native evidence](../evaluation/macos-native.md) qualifies bounded CPU and
terminal execution. Metal has its own independent backend qualification;
Darwin platform support alone does not grant GPU execution. Explicit cache eviction has no equivalent qualified native
mechanism and refuses; native memory availability is a conservative advisory
estimate, with the existing reserve and capacity admission still applied.

## Apple Silicon Metal foundation

The common backend factory admits `metal` on native Darwin arm64. Admission
requires an enumerated unified-memory device, command queue and compiled F32
embedding compute pipeline before publishing READY. Invalid device selectors,
missing resources and builds without that native implementation refuse the
exact request. The ordinary `yvex inspect backend metal` consumes the same
typed report and exact operation-variant capability authority as other backends.
This adds no Metal-only runtime, CLI, deployment or model lifecycle.

`backend.metal.execution` owns ARC-managed device, queue, pipeline and shared
MTLBuffer lifetimes. Each tensor owns one native buffer record; borrowed
physical subviews refuse rather than pretending that their offsets are bound.
Synchronous embedding and blit operations wait for checked command completion
before publishing initialized output or releasing temporary IDs. A command,
encoder or completion failure marks the backend FAILED; only cleanup remains
legal. Checked close retains the owner while tensors remain, allowing release
and a final close retry. Allocation or declared-capacity refusal does not poison
an otherwise healthy backend.

Shared storage uses one physical system-memory domain. CPU `contents` and the
GPU address are distinct observations; shared RAM does not imply identical
virtual addresses. Host API reads/writes copy bytes to/from that shared buffer;
embedding binds those same table/output buffers without another host/device
allocation or transfer. IDs use a bounded temporary shared buffer and two
64-bit dispatch constants use `setBytes`. Device copy/zero use Metal blits.
Copy counters describe those API/storage operations, not measured memory-bus
traffic. There is no no-copy external host wrapping in this foundation.

The internal schema-v1 resource record separates logical addressable/mapped
buffer extents, requested owned payload allocation, temporary ID storage, the
device's recommended working set, max buffer length and API current allocation.
The requested-payload byte limit includes tensor and ID buffers; it does not
bound driver queue/pipeline or inline-command overhead. Physical residency and
actual working set remain unmeasured through explicit absent known bits. Legacy
dedicated/free GPU-memory and CUDA capability fields remain unavailable, not a
measured zero or a promise that all system RAM is usable.

The Rust product shell consumes the same copied backend report through generated
private FFI bindings. Backend selection delegates to the common kind parser;
resource rendering preserves known-zero versus unmeasured facts and shared
system-memory semantics. It does not restore the retired C CLI or derive model
admission from primitive availability. Make supplies the native framework link
inputs to Cargo, while Linux excludes the Objective-C implementation.

The first operation is existing F32 embedding row selection: integer loads and
stores preserve source F32 representations exactly. Every other numerical
variant, including F16 embedding, matmul, normalization, attention and quantized
operations, refuses. The [numerical contract](../contracts/numerical-abi.md#embedding-row-selection)
and [Metal evidence](../evaluation/macos-metal.md) bound the claim.

The generic kind parser now consumes the existing four-kind vocabulary instead
of a CPU/CUDA subset. Resource observations and variant reports extend common
backend-owned seams; no backend internals leak into runtime. Model admission
remains bounded to CPU/CUDA. `runtime.capacity` validates only those kinds and
uses CUDA-specific placement/capacity facts; `runtime.core` model admission and
specialization release/counting also assume two backends. Before a Metal model
wave, those canonical owners must select capability and physical memory-domain
semantics and generalize specialization lifetime. Adding another kind conditional
would not resolve that structural boundary. It does not block this primitive
backend foundation, which does not construct an engine or claim model support.

## Execution path

The CPU operation table realizes the common BF16-weight linear contract with
source-order F32 accumulation and the declared F32 or BF16/RNE publication.
Its executable owns a checked descriptor and borrows exact encoded weights per
invocation; it reports no accelerated matrix execution. Interleaved query/gate
splitting, BF16 residual addition and rounded SiLU/sigmoid products use the same
typed operation requests as CUDA. Unsupported classes and foreign owners refuse.
Checked CPU close retains a cleanup-only owner while tensors or linear
executables remain; correct releases and a final close discharge it. Descriptor
bytes are reported as plan host storage, not prepared weights or GPU residency.

The separate encoded-F32 CPU projection retains literal source-column F64
accumulation and F32 publication. Four independent activation rows reuse each
canonical little-endian weight load; ARM64 NEON lanes hold independent F64
accumulators, not a parallel sum of one dot. Portable hosts use the same ordered
scalar recurrences. The runtime checks cancellation between bounded four-row
tiles and publishes only the completed projection. No threads, prepared weight
copy, new numerical class or family-specific dispatch is introduced. Exceptional
rows recover the quantization owner's first error/publication behavior. See the
[numerical obligation](../contracts/numerical-abi.md#ordered-encoded-f32-cpu-projection).

Explicit CPU gated-delta execution consumes the existing host sequence-state
provider and common F32 recurrence authority. It stages through the same session
transaction and rounds only the declared BF16 output. It is not a fallback from
a requested GPU backend and does not grant those operations to Metal.

Authenticated program + real batch/worklist → admitted implementation → device
buffers and kernels → checked synchronization → staged result → runtime commit.

Unsupported mandatory semantics fail closed. A faster kernel is not evidence of
faster model execution; compare complete workload latency, preparation, memory
and numerical effect using the [benchmark methodology](../evaluation/benchmarks/methodology.md).

At NONE evidence scope, attention with a device result does not manufacture an
empty host output. Both portable CUDA row orchestration and admitted device-native
execution consume that typed device view through MoE and the post block. The
execution class selects orchestration, not whether the activation is host-backed.
CPU still requires host input; unmatched device/result tuples, foreign storage,
invalid geometry or unwritten operands refuse at their typed owners. This removes
an obsolete host-pointer requirement, not numerical validation or transaction
completion. Diagnostic host publication remains independently requested.

CUDA attention workspace lowering distinguishes host ingress from borrowed
device activation ingress. Ordinary device-ingress execution reserves no host
copy of the ingress component; host-input and full forensic-evidence paths
retain their complete staging bound. Runtime forwards this placement fact into
capacity admission and workspace preparation. Persistent state, prefix
checkpoints, status, publication and device scratch keep their original owners
and lifetimes; the smaller host bound does not remove validation or state work.

The shared CUDA attention-primitives owner performs independent candidate
ranking for up to 32 query rows per score/top-k tile. Each query retains its own
causal visible prefix, source score arithmetic and stable tie order. This tile
is not the prompt-position admission envelope: larger admitted phases cover
their real rows through multiple tiles. Workspace charges only the bounded
ranking tile; future or padded rows never become visible candidates.

Rolling compression first emits its complete phase-owned population. Its
per-emission weighted normalization, position-strided RoPE and main/index
publication then execute as multi-row transforms rather than repeated
single-row launches. The compressor ratio determines the position stride, not
a reconstructed layer or family name. Quantized main-cache rows retain their
physical stride, while index rows retain their own Hadamard/FP8 layout.
State staging, causal selection and transaction completion are unchanged.

Routed encoded MoE matrix execution maps each real expert population to
independent eight-column tiles. Warps own independent output tiles; a
cooperative bucket-prefix scan resolves the compact tile ordinal against the
canonical worklist. Conservative launch padding performs no selected work.
The change retains expert associations, activation encoding, exact integer
products, the admitted F32 reduction tree, BF16 publication and exceptional
source-ordered F64 recovery. It introduces no prepared weight layout or
different routing policy.

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

The competitive candidate adds a checked completion scope to serialized physical
SSA program execution. Its prepared program owner allocates a backend-declared
completion workspace outside the temporary arena. CUDA uses one latched status
word across participating projections, mHC and BF16 operations on the execution
stream; dependent intermediate views are not public outputs. The runner always
completes the scope, including after cancellation or refusal, before admitting
outputs. Numerical failures invalidate all program outputs and a fresh scope
resets the word only after completion. An unobserved device completion makes
the backend cleanup-only. Nesting refuses, host observations retain their own
checked barrier, and standalone calls retain immediate validation. CPU and
other backends without paired scope hooks retain synchronous operation behavior.
An earlier serialized transformer producer may hand off its pending numerical
status through an ordered device copy into the program-owned word. Beginning
the successor never clears an unobserved predecessor failure. Output-program
admission charges this completion workspace along with all other compiled
device storage; a one-byte-short budget still refuses before publication.
An invocation explicitly carries that completion ownership to pure SSA copy,
reshape, row construction/slicing and rotary preparation. Their same-stream
copies no longer complete at each intermediate value. Weighted normalization
likewise lets its completing consumer own the copied scratch. This does not
weaken the synchronous tensor-copy API: non-scoped callers and backends without
queued-copy support retain checked synchronous copying. The runner drains
submitted copies before cancellation/refusal can release or publish storage.
Eligible encoded projections also accept an explicitly owned packing workspace.
The backend declares its checked byte extent from the admitted encoding and
actual row population. A prepared physical program retains one buffer for its
serialized linear steps, accounts growth including the temporary old/new overlap,
and releases it with the program owner. Reuse is ordered on the execution stream
through checked completion; it cannot borrow an unrelated operation's scratch
arena. Missing, foreign, insufficient or operand-aliasing storage refuses before
submission. Standalone callers without a supplied workspace retain call-owned
packing and immediate checked completion. This changes neither the Q8 packing
algorithm nor the projection's accumulation/publication class.
Small mixing matrices may retain one matrix cell per warp lane during iterative
normalization. The generic four-stream-and-smaller realization preserves ordered
F64 row/column sums, every intermediate F32 publication and the first-iteration
rule; only storage and synchronization differ. Larger matrices retain the checked
block-wide realization. This is not a different numerical class or family policy.
Frozen CUDA preservation, independent post-first-iteration host continuation,
malformed-input controls and sanitizer checks precede the source-stable
[complete-model characterization](../evaluation/retained-observations.md#warp-cell-mixing-normalization-2026-10-07).
The checked completion and packing changes alter submission/status lifetime,
not arithmetic, reduction order,
physical precision or state transactions. The source-stable complete-model
generation gate additionally qualifies replay, target/DSpark equivalence,
refusal, cancellation and cleanup at the exercised bounded scope.
[Integration evidence](../evaluation/retained-observations.md#competitive-computational-integration-2026-10-07)
separates that gate from coding timing and the successful repeated-8K control.
The earlier 8K refusal did not reproduce; its historical cause is not inferred.

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

For admitted MXFP4 weights with Q8 activations, the explicit row-reduction
realization may share encoded-weight traversal across independent input rows
using integer matrix instructions. Its geometry covers real populations and
partial output tiles; narrow attention populations use the same generic owner.
Each result retains the original per-block F32 operations and 32-lane reduction
tree, including the intermediate F32 before optional BF16 publication. This is
not the distinct ordinary wide-matrix reduction and does not change decoded-input
projections, packing, weights, workspace ownership or numerical admission.
The backend selects geometry from physical extents, never family names.
[Numerical and complete-model evidence](../evaluation/retained-observations.md#narrow-q8-matrix-row-reuse-2026-10-08)
separates this bounded execution improvement from model quality and the unearned
throughput targets.

The earlier bounded launch-geometry repair mapped independent ordered decoded
dots to independent CUDA threads rather than
warps whose other lanes immediately return. Generic launch geometry maps each
row/input pair once, bounds the task product and covers partial tiles; grouped
projections preserve group and input strides. Paired BF16 projections likewise
assigned both dots for one row to one thread. That repair used the canonical
source-order F64 helper and the same publication cast. The separately admitted
Q8 reduction and narrow block-owned F32 class are unchanged. This changes neither
buffers nor runtime state, synchronization, routing populations or numerical
validation. [Bounded GB10 observations](../evaluation/retained-observations.md#deepseek-gb10-optimization-2026-09-30)
qualify the complete-request benefit, separately from component timings.

Storage-invariant dispatch for ordered decoded dots is resolved once per row;
the selected storage realization still decodes and accumulates each column in
source order. Paired BF16 dots share one input-column traversal with two
independent F64 accumulators, retaining each dot's order and publication cast.
Neither change introduces a parallel sum, alternative precision or routing
policy. [Pipeline characterization](../evaluation/retained-observations.md#deepseek-gb10-inference-pipeline-2026-09-30)
separates these kernel gains from canonical arena sizing and complete-request
preparation/teardown cost.

Decoded Q8_0 projections select their constant-format certified and literal
realizations before column traversal, just like the other admitted storage
formats. Generic finite F16 decoding uses the device's exact binary16-to-F32
conversion; exceptional encodings retain the original explicit bit mapping.
Neither mechanism changes the numerical class, physical policy, buffers,
workspace, engine state or completion boundary. The
[decoder qualification](../evaluation/retained-observations.md#native-decoder-realizations-2026-10-09)
keeps component bit controls, same-day complete-model characterization and
installed-product identity separate. This is not a new quantization or a
claim for other checkpoints/hardware.

The competitive computational integration subsequently adds the
[certified equivalent decoded-dot realization](../contracts/numerical-abi.md#ordered-decoded-dot-publication).
Parallel work is admitted only when conservative bounds prove identical F32
publication to literal source-ordered F64. Otherwise the CUDA owner retains
literal ordered evaluation. This supersedes the implementation shape, not the
earlier earned evidence or its numerical contract. Prepared lossless digit
layouts are bounded backend resources, not a new quantization; the unchanged
artifact/binding and final F32-to-BF16 publication remain authoritative.

Lossless MXFP4 activation eligibility is computed once per actual activation
row, not rediscovered after partial integer-dot work for every weight row.
Stream-owned flags occupy the existing bounded preparation arena and are
rewritten on each invocation. Unsupported activation spans enter the unchanged
certified ordered-dot fallback directly; exceptional weight scales and
inconclusive bounds retain their original fallback. No weight representation,
numerical class, model state or persistent cache is added. [Eligibility evidence](../evaluation/retained-observations.md#activation-owned-lossless-eligibility-2026-10-08)
separates affected-kernel attribution from repeated complete-model benefit.

Exact MiniMax output-linear requirements remain source/package numerical facts.
Runtime component specialization resolves them to exact generic linear
execution records; generic CUDA consumes those records without MiniMax switches
or magic shape recognition.


## Implementation and evidence

[src/backend](../../src/backend) · [src/backend/cuda](../../src/backend/cuda) · [include/yvex/backend.h](../../include/yvex/backend.h)

[QA selection](../evaluation/qa.md) · [Current state](../project-control/STATUS.md)

## Continue

[Deployment and Specialization](deployment-specialization.md) · [Generation and Decoding](generation-decode.md)
