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

YaRN RoPE consumes compiled rotary geometry, not a model-name policy. The CUDA
owner shares the exact F64 angle among eight heads for 64 rotary components or
four heads for 128. Launch geometry rounds head cohorts separately for each
token, including partial final cohorts. Other geometries retain the admitted
separate-angle path. This changes neither immutable weights nor numerical class;
shared angles occupy CTA-local storage only, not persistent engine/session
buffers. Component conformance and
complete-model performance remain separate evidence gates.

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

The encoded ROW implementation selector separately checks physical qtype,
actual input/output population, reduction geometry and device. Its SM121 Q8_0
register-tile realization consumes existing package bytes and Q8 activation
packing without an engine-local weight copy. It retains the ROW arithmetic,
not the distinct DEFAULT matrix reduction. Unmeasured small geometries keep
the preceding row consumer. This backend-local equivalent choice does not
change artifact/binding identity or qualify a new quantization recipe. See the
[numerical envelope](../contracts/numerical-abi.md#internal-moe-execution-boundary).

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

For the admitted online64 attention class, an SM121 realization fuses QK,
block softmax and probability/value multiplication for at least 16 positions
and at least 256 padded candidate slots. Device and function attributes must
admit its 91,520-byte dynamic shared range plus the compiled static allocation
before selection. The checked causal map and packed BF16 operands remain
phase-local; scores, probabilities and recurrence are CTA-local, not global
scratch. Small/short-key shapes and other devices retain the staged consumer.
The scratch recipe remains a conservative bound admitting either equivalent
implementation. Neither parameter/binding identity nor the numerical class
changes. Component/source conformance does not establish full-model speedup.

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
independent eight-column tiles. The device worklist producer emits a compact
`(bucket ordinal, column offset)` index once; gate/up and down reuse it rather
than repeating a bucket-prefix scan for every output-row tile. Bucket ordinals
are not expert IDs. Adjacent warps visit column tiles before advancing the
output-row tile, allowing reuse of the same expert's encoded weight rows.
Conservative launch padding performs no selected work. A null index retains
the prior scanned realization as an exact control, not a new product mode.
The change retains expert associations, activation encoding, exact integer
products, the admitted F32 reduction tree, BF16 publication and exceptional
source-ordered F64 recovery. It introduces no prepared weight layout or
different routing policy.

Ordinary decoded-input projections use compact eight-by-eight virtual tiles
of independent output rows and input positions. The larger population owns
the outer tiles; partial tiles contain only real pairs and retain the existing
grid size. The per-pair ordered dot, certification, exceptional recovery and
publication do not change. Width-one decode and the literal forensic traversal
retain their previous assignment. Q8-activation matrix scheduling is separate.
This is a backend-local equivalent realization, with no prepared allocation,
artifact rewrite or new numerical class. Its component evidence does not
establish the pending complete-model benefit.

The index belongs to the existing phase workspace and is rebuilt on its owning
stream with each worklist. The backend capacity owner charges an aligned
eight-byte header plus a conservative `ceil(selected_pairs / 8) +
min(selected_pairs, expert_count)` entries; runtime preparation and admission
consume that same owner. The worklist kernel additionally uses two 256-entry
shared-memory integer arrays. There is no extra launch, device allocation,
weight copy or host readback. Deployment-selected matrix eligibility and
scalar fallback consume sealed phase-specific thresholds, preserving their
numerical classes. Backend-local
tile scheduling changes executable/kernel identity, not artifact, binding or
public schema identity. Component evidence is linked from
[compiler evaluation](../evaluation/compiler-evidence.md#indexed-moe-tile-realization).

The gate/up matrix consumer accepts homogeneous IQ2_XXS or Q2_K pairs, with
Q2_K down operands. The Q2_K gate/up path reuses the existing encoded integer
product and admitted reduction; it does not silently introduce another
arithmetic class or require a second prepared weight copy. Deployment and
offline candidate screening reject incompatible operand geometry and mixed
gate/up pairs. Component qualification covers decoded-weight F64 references,
bitwise agreement with the existing Q2_K row path, tails and failure controls;
this alone does not establish checkpoint quality or full-model throughput.

### Encoded execution geometry

The following realizations consume existing deployment-admitted operations.
They do not change semantic topology, parameter bytes, the binding, the
numerical class or the public CLI/SDK contract. Executable and kernel-bundle
identities distinguish them. Selection uses actual operand geometry and real
worklist populations, never a model name or manufactured batch width.

| Operation / eligibility | Physical realization | Resource and numerical boundary |
| --- | --- | --- |
| Q2_K routed matrix; indexed eligible population | Down retains a four-warp 64-row by eight-column tile. Paired gate/up shares one activation stage over a 64-row by sixteen-column tile with eight warps. Two adjacent canonical column descriptors have one output owner; the second performs no stores. Two buffers overlap the next real encoded block with integer MMA. | Down uses 15,424 bytes and paired gate/up 30,848 bytes of static operand storage, not engine allocations or prepared model copies. The paired launch bound admits its larger live register population without spill. SM80+ uses aligned asynchronous copies; unaligned operands and older targets retain bounded loads. Independent dot arithmetic, the 32-leaf trees and exceptional recovery remain unchanged. Complete-model benefit remains a separate gate. |
| IQ2_XXS paired gate/up, SM121, at least 64 actual input positions and even K256 extent | Eight warps share a 64-row by sixteen-column tile and two raw-block staging buffers. A 68-byte aligned interval contains each real 66-byte block; a bounded funnel shift recovers its original words, including halfword-displaced blocks. Gate/up reuse the same Q8_K activation stage. | No persistent prepared copy or added global workspace. Alignment and row/extent checks precede selection; other geometries retain the register consumer. Compiled CTA shared storage is 29,828 bytes in the measured SM121 bundle, with 128 registers per thread and no spill. Exact products, scale operations, reduction and exceptional recovery are unchanged. |
| Homogeneous Q2_K or IQ2_XXS gate/up with Q2_K down, SM121, prefill with matrix minimum one | The existing indexed matrix consumer handles all nonempty buckets, including one, two and three actual columns. Scalar gate/up and down fallback launches are omitted only when the sealed minimum is one. | No extra allocation, prepared weights, routing change or replicated input. Decode/verification retain minimum four; missing or incoherent phase selection refuses. Independent decoded-weight, old/new bitwise and exact partial-population controls remain mandatory. |
| Q2_K routed rows with 4096 gate/up or 2048 down extent | Separately compiled constant encoded geometry; the dynamic row consumer remains the control/fallback. | Smaller register/shared footprint, no additional launch or workspace. Wrong qtype, extent or activation class refuses before access. |
| Wide MXFP4 Q8 projections; at least 2048 output rows and 64 inputs, compatible group alignment | Eight warps cooperate over 64 output rows and 32 real inputs. Aligned shared words and a bounded funnel shift reconstruct each canonical encoded quartet. Narrow groups retain the prior tile. | Encoded bytes are staged within one CTA; no full weight copy. The last quartet ends inside its 136-byte cohort. Original eight ordered sub-block products and scale operations are preserved. |
| BF16 candidate scores within the checked exponent-span bound | Three exact signed digits per operand and integer matrix products; one query and eight candidate rows per tile. | Invalid spans use the original ordered-F64 computation. Causal visibility, head reduction and top-k remain unchanged. |
| Native Q8 attention reduction, head dimension 512 and admitted broad population | Constant head geometry removes unrelated register paths. | The existing runtime scaling operation, mask, reduction and publication are preserved; substituting a precomputed reciprocal square root is not equivalent. |
| Native Q8 attention reduction, dimension 512, one to four positions and 32–128 heads | Eight warps compute independent candidate scores; the original sequential softmax/value recurrence consumes them in order. | CTA-local scores and row handles replace repeated block reductions. No persistent preparation, new state or altered candidate population. |
| RoPE dimension 128, whole four-vector cohorts in a 256-thread launch | Four real vectors reuse each exact F64 angle within their CTA. Other geometries retain independent evaluation. | Same launch extent and arithmetic, 1,040 bytes of static CTA storage, no persistent angle cache or device allocation. Full-model performance acceptance remains separate from the component proof. |

The existing CUDA attention and tensor-core owners implement these choices;
RoPE now shares the attention kernel compilation unit. The separately tested
fixed-input MXFP4 row candidate was removed after an unfavorable complete-model
tradeoff; its frozen source and negative observations remain in Evaluation.
Common codec/reduction primitives remain under their existing owner.
CTA shared storage and registers are device launch resources, not persistent
prepared bytes. Runtime capacity continues to charge activation, worklist,
attention and state arenas through the existing authoritative calculators.

The experimental phase-prepared BF16/F32 matrix class derives its replacement
weight-tile extent from the already admitted decoded arena and actual matrix
geometry. It no longer imposes a universal 512-row tile. Input is prepared once;
one arena-bounded weight tile is replaced on the owning stream only after its
contraction consumes it. Exact output tails, publication checks and zero explicit
BLAS workspace remain. The [numerical contract](../contracts/numerical-abi.md#experimental-phase-prepared-projection-class)
defines the capacity inequality and distinct arithmetic class. Larger legal
tiles change equivalent class-local execution geometry, not encoded weights or
the resource reserve; their complete-model effect is not yet measured.

Within an explicitly selected BF16/F32 attention-projection class, 1–31 real
positions now lower to independent four-chain SIMT dots; wider positions keep
MMA or admitted phase preparation. The narrow implementation checks the same
BF16/exponent envelope and published error bound. It replaces that class's
conservative ordered fallback, not the ordered-reference class or output head.
It owns no prepared model copy or global scratch and is not counted as Tensor
Core work. Its arithmetic is not bitwise-equivalent to the former fallback;
component bounds and complete-model characterization remain distinct.

See [convergence evidence](../evaluation/compiler-evidence.md#encoded-execution-convergence)
for source-stable component and complete-model boundaries, including rejected
geometries. These alternatives do not establish automatic qualified selection
or the Program P competitive exit.

### Block-online attention realization

Unit normalization may assign eight independent vectors to the eight warps
of one CTA. Each lane retains eight virtual leaves of the original 256-thread
F64 reduction; the original offsets and publication remain unchanged. The
inverse is computed by one lane and broadcast within that vector's warp.
This equivalent alternative needs no arena or prepared model allocation.
The backend selects it for at least 512 real vectors and widths up to 1024;
other geometry retains the original CTA-per-vector kernel. This is neither
a new numerical class nor a complete-model performance claim.

Exact candidate scoring has a separate, equivalent executable alternative.
For admitted matrix geometry, CUDA prepares each actual query/key vector's
checked exponent envelope and three balanced signed-byte digit planes once
per 32-query selection tile. Score CTAs reuse them instead of repeating
envelope scanning and digit construction for every eight candidates. An
invalid vector retains metadata and the original ordered-F64 fallback; an
invisible key is not numerically admitted merely because it was prepared.
The existing integer products, I64 reconstruction, head order and stable
ranking remain unchanged. Narrow or incompatible geometry keeps the original
consumer. No new parameter, state or numerical-class identity is implied.

The views are transaction-private arena storage. Sequential query tiles reuse
one stream-ordered range after the preceding score/rank consumers; every tile
prepares its exact operands again. Allocation reuse is not a value cache.
Backend recipe lowering charges the bounded query/history extents, capacity
rounding and alignment, not one retained history copy per tile. Cleanup clears
the retired handle and capacity. Host staging and persistent state do not grow.
Capture follows admitted capacity: empty and populated histories
retain the same preparation/score/rank nodes, with exact counts rebound on
replay. Component correctness does not establish complete-model benefit.

An opt-in compiler-bound attention class lowers causal history into phase-local
BF16 operand preparation, a shared causal index map, tiled QK, 64-key online
softmax, and probability/value MMA. Preparation moves repeated conversion and
address validation out of each head/key/dimension tile; it does not change the
admitted MMA or softmax ordering. Invalid KV rows cause refusal only when the
causal selection references them. The prepared views belong to the current
execution arena, not persistent KV or immutable weights. Backend capacity
lowering charges packed query/history, row-validity flags and the causal map
from the sealed workspace recipe. CUDA Graph topology includes both the
64-key pitch and the 64-row preparation bucket; exact counts and addresses
remain replay parameters. Complete
16-head cohorts share weight/history loads; the backend does not reconstruct
layer topology or alter selected compressed-row populations. Intermediate score,
probability and rescale buffers belong to transaction execution scratch, charged
by workspace recipe v5. They are neither prepared model weights nor persistent KV.

The sealed workspace recipe now includes the attention numerical computation.
Unknown computation retains conservative independent ranges. Online64 with no
retained query evidence may reuse the F32 query range for attention values:
preparation consumes every original query into bounded BF16 storage before
either staged PV or fused online64 writes any result. Other numerical classes
and evidence-bearing jobs retain independent query/result lifetimes. Backend
cost lowering removes the result range only when both component geometries,
alignment and lifetimes agree; the existing execution reserve is unchanged.
This is an executable resource decision, not parameter re-encoding or persistent
state reuse. The new internal recipe layout rejects schema 4 before reading its
new field. Runtime/public-wire versions and artifact/binding identities do not
change. Full-model benefit requires separate proof.

The canonical prefill execution envelope permits at most 2,048 real positions;
the product default remains 512. This is a checked upper bound, not a promise
of capacity or efficient batching. Attention projection consumes the device
workspace capacity already admitted and attached by the runtime, rather than
an unrelated fixed 1-GiB per-job ceiling. The read-only workspace summary exposes
the existing allocation owner; it does not introduce another resource budget.
Encoded matrices, prepared projections, expert worklists and online64 attention
must each accept the actual geometry before execution. Context, persistent state,
construction peaks and the existing memory reserve remain independently charged.
Larger chunks must earn numerical, capacity and complete-model performance
evidence; increasing the envelope alone is not an optimization result.

This is a different [numerical class](../contracts/numerical-abi.md#explicit-block-online-bf16-attention-class),
not an automatic replacement for the scalar-candidate recurrence. The same
artifact can produce a new binding with that explicit arithmetic; default and
draft execution remain unchanged. CPU, unsupported geometry and portable or
forensic substitution refuse. Component reference and sanitizer results do not
promote complete-model performance or automatic Program P recommendation.

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
Compiler-selected immutable-input views need no copy or allocation. They borrow
the admitted input only for the invocation, preserve logical shape/precision,
and cannot alias caller-owned outputs or computed arena lifetimes. They do not
clear completion status, perform arithmetic or weaken cancellation. The copying
realization remains available as a control; backend support for this carrier is
not model admission or proof of a complete-model speedup.
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
Separately, an explicitly compiled F32 contraction class can replace the mixing
projection's ordered-F64 dot. Its narrow kernel is captured/rebound with the
attention graph; wide cuBLAS work precedes graph replay on the same stream.
The typed MoE ingress carries the same explicit reduction obligation. A checked
program defers projection completion to its own barrier, without removing
standalone completion or finite-result checks. This candidate has no additional
prepared weight allocation and cannot silently change legacy dense programs.
See the [distinct numerical contract](../contracts/numerical-abi.md#explicit-f32-contraction);
operation qualification does not establish model quality or a performance winner.
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

For admitted MXFP4 or Q2_K weights with Q8 activations, the explicit row-reduction
realization may share encoded-weight traversal across independent input rows
using integer matrix instructions. Its geometry covers real populations and
partial output tiles; narrow attention populations use the same generic owner.
Each result retains the original per-block F32 operations and 32-lane reduction
tree, including the intermediate F32 before optional BF16 publication. This is
not the distinct ordinary wide-matrix reduction and does not change decoded-input
projections, packing, weights, workspace ownership or numerical admission.
The backend selects geometry from physical extents, never family names.
On SM121, wide MXFP4 ROW contractions additionally admit a 64-by-32 shared
tile. The two spatial column tiles share encoded words without replacing the
per-K256 `fmaf` recurrence or the 32-leaf reduction with DEFAULT arithmetic.
The component selector is shared by encoded-linear and attention/MoE consumers;
Q2, narrow inputs and other devices retain the preceding implementation. This
adds CTA-local storage, not a persistent prepared layout, allocation or artifact.
Its kernel identity is distinct; complete-model benefit requires a new measurement.
The Q2_K realization reuses the routed operator's integer-product primitive
without importing routing semantics: independent input columns have no expert
worklist, and retain the ordinary row dot's ordered F32 tree. It consumes the
original encoded backing, not a second prepared copy. Ordinary wide-matrix
projection remains a distinct reduction contract. Q2_K component controls cover
4–512 input rows, partial output tiles, F32/BF16 publication and independent
decoded-weight CPU references; complete-model benefit is a separate gate.
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
