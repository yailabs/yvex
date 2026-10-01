<!-- docs:metadata
title: Apple Silicon Metal Foundation
id: yvex.evaluation.macos-metal
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Apple Silicon Metal Foundation

**Real GPU primitive execution, owned shared storage and explicit later boundaries.**

[Up](README.md)

`METAL.BACKEND.FOUNDATION.0` starts on `feature/macos-metal` at
`67a7905ea9deb98b0704629a1f979634e19007fb`, aligned with published main and a
clean working tree. Main already contains native macOS CPU qualification.
This temporary pressure branch extends the existing backend architecture;
it does not create a second runtime or promote model inference.

The tested target is macOS 26.6.2 arm64, Apple M5 Pro with 16 GPU cores and
24 GiB shared system RAM. Build inputs use Apple Clang 21, GNU Make 4.4.1,
Python 3.14 and Rust 1.98.1. The REPLAI producer remains pinned to
`93d62f6d34cfb933a1f59407ade027152e1ef2ba`. Public C layouts and protocol v24
remain unchanged. Exact final source-stable qualification receipts are recorded
below after the candidate commit is tested; the Task remains in progress until
those gates pass.

## Architectural delta

| Surface | Ownership and resulting behavior |
| --- | --- |
| `backend.metal.admission` / `src/backend/metal.c` | Non-native builds refuse exact Metal admission without Apple dependencies. |
| `backend.metal.execution` / `src/backend/metal/native.m` | One ARC-owned device, queue and compute pipeline; shared buffers, synchronous compute/blit completion and checked cleanup through the existing vtable. |
| `backend.core` | Register Metal in the common factory; parse all canonical kind names; copy exact variant reports for every backend; checked report-owner close. |
| Internal resource observation ABI | Schema-v1 copied facts plus explicit known bits, supplied through the common backend vtable and rendered by existing inspection. No installed public layout change. |
| Build/source ownership | Manifest-owned `.m` compilation, source-relative archive members, native Objective-C namespace checks and material ARC/compiler flags. Linux excludes native objects/frameworks; CUDA remains independent. |
| QA | Registered `metal.foundation`, `metal.failure` and CPU-only `unit.backend_metal_refusal`; generated accelerator runner, native prerequisites, exclusive device arbitration and immutable receipts. |

Admission enumerates devices and requires unified memory, a real command queue,
library/function compilation and compute pipeline resolution before READY.
Invalid selectors or partial admission publish no owner. Native tensor storage
records retain the exact owning tensor and buffer; borrowed physical views
refuse. Failed launch/encoder/completion marks FAILED and refuses later dispatch.
Checked close retains a cleanup-only context while tensors remain; explicit
release and a final close retry discharge ownership. All submitted commands
finish before temporary storage release or successful output publication.

## Numerical and lifecycle evidence

The representative operation is existing F32 embedding row selection. It uses
integer payload loads/stores rather than floating arithmetic or a lower-precision
realization. The independent reference addresses the original host rows directly;
the existing CPU backend is a second comparison, not the sole oracle. Exact
bit equality follows the [operation contract](../contracts/numerical-abi.md#embedding-row-selection).

Diagnostic execution on the current candidate passes both Metal tests. Widths
`1,257,1025` and token counts `1,13,63`, vocabulary 37 and four repetitions
exercise duplicate IDs, edge rows and partial threadgroups. Twelve compute
dispatches plus 24 copy/zero blits complete; all CPU/GPU/reference comparisons
have zero bit mismatches, including signed zero, subnormal, maximum finite,
NaN payload and infinity representations. Separate storage qualification
exercises odd three-byte blits and valid F16 geometry refusal without conversion.
Invalid IDs, foreign owners, borrowed-view read/release, uninitialized read,
matmul and RMSNorm refuse explicitly.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `metal.foundation` | Original host bytes with independent row addressing, plus CPU operation | Three width/token pairs, repeated IDs and exceptional F32 payloads | GPU result, copied result and zeroed result publish after completion | 12 compute dispatches, 36 completed commands, all expected bytes | Zero F32 bit mismatches | Diagnostic PASS; final receipt pending | Existing F32 embedding and bounded storage |
| `metal.failure` | Typed capability/status and owner pointers | Device/queue/pipeline/allocation/command/encoder/completion hooks and exact byte budget | Fail closed, retain retryable cleanup, no hidden CPU execution | All refusal/cleanup assertions pass | Exact states, pointer retention and budget ledger | Diagnostic PASS; final receipt pending | Failure propagation and storage lifetime |
| `native` | Existing CPU/platform/host/terminal contracts | 14 registered cases, actual local memory | Native CPU path remains healthy | Final candidate confirmation pending | No FAIL/SKIP/BLOCKED/ERROR allowed | Pending | Affected macOS CPU regression |
| Linux isolation | Same candidate source, native compiler and existing CPU contracts | Disposable checkout, no Apple/CUDA toolchain requirement | CPU gates pass; exact Metal refuses; Metal lane is BLOCKED | Pending | Source identity, native results and explicit refusal | Pending | Platform separation and Linux CPU regression |

Diagnostic fault hooks are not real execution evidence and are unset in the
foundation GPU test. Partial admission tests exercise automatic release without
publishing a backend. Command faults reconcile temporary IDs before returning;
close/release/retry tests exercise permanent buffer lifetime. Allocation failures
and capacity refusals leave a healthy context reusable. No fault result is
counted as a successful GPU operation.

## Storage and observed resource facts

`MTLResourceStorageModeShared` uses one physical system-memory domain. CPU
`contents` and GPU virtual addresses are distinct; the implementation stores
both and never dereferences the GPU address on the host. Host writes/reads copy
caller bytes to/from shared buffer contents. Compute binds the same table/output
buffers without staging into a second GPU allocation. IDs are explicitly copied
to a temporary shared buffer; two 64-bit constants are copied with `setBytes`.
There is no no-copy wrapping of external host memory. Metal blits perform the
explicit device-copy operation.

| Fact | Observed scope/value | Limit of the claim |
| --- | --- | --- |
| Device / storage | Apple M5 Pro, device index 0, `hasUnifiedMemory=true` | Exact tested target; not every Apple GPU/version |
| System RAM | 25,769,803,776 bytes (24 GiB) | Shared with macOS and all applications |
| Recommended working set | 19,069,665,280 bytes (about 17.76 GiB) | Device API recommendation, not a hard allocation limit or measured current working set |
| Maximum buffer length | 14,302,248,960 bytes | Per-buffer API limit, not available memory |
| API current allocation | 458,752 bytes before and after the representative workload | Device API observation for this process, including driver allocation; not system-wide free memory or physical residency |
| Owned addressable / mapped / allocated / temporary payload after release | All zero; 48 allocation events and 48 releases | Requested buffer extents and payload ledger, not allocator page rounding or global memory reclamation |
| Peak temporary payload | 252 bytes | Explicit token-ID buffer; driver inline command backing is unmeasured |
| API host write copies | 760,960 bytes | Source tables, IDs and 16 constant bytes per dispatch |
| API host read copies | 3,260,016 bytes | Three complete output reads per case |
| Device copies | 1,086,672 bytes | Explicit GPU blit copies; zero blits do not count as copies |
| Legacy H2D / D2H | Zero discrete-domain transfer bytes | Does not claim zero memory-bus traffic |
| Resident / actual working-set bytes | Unmeasured; corresponding known bits unset | Never presented as measured zero |

Schema-v1 known mask is 487. Requested-payload admission includes tensor and ID
buffers. Queue, pipeline and driver inline-command overhead is not bounded by
that payload limit; the API current-allocation observation remains separate.
Dedicated global/free/total GPU memory and CUDA compute/managed-memory fields
are unavailable/inapplicable in the legacy device record. Shared physical RAM
does not imply identical CPU/GPU address spaces: legacy unified-addressing is
false. Resource report rendering preserves these distinctions.

## Problems found and later boundaries

| Owner / boundary | Finding and handling |
| --- | --- |
| `backend.core` kind parsing | Existing vocabulary had four identities but parser accepted only CPU/CUDA. Resolved generically using the authoritative name table, with all-kind round-trip tests. |
| Common resource observations | Legacy device/memory records cannot distinguish unknown residency and shared physical storage. Added a non-public schema-v1 provider with known bits, not invented dedicated-memory telemetry. |
| `runtime.capacity` / `src/runtime/capacity.c` | Options and placement/capacity contracts operationally admit only CPU/CUDA. A later model wave requires capability and physical-memory-domain based admission/accounting. Left unchanged; no local Metal bypass. |
| `runtime.core` / `src/runtime/core.c` | Model admission rejects kinds beyond CUDA; specialization release/counting assumes two kinds. Canonical specialization lifetime and model admission must generalize before Metal engines. Left unchanged. |
| Numerical realization | Only F32 row selection is earned. Ordered F64 projection/normalization requirements cannot silently become F32 on Apple GPU. Equivalent realizations or explicit permitted decomposition need independent qualification. |
| Native qualification fixtures | Broader structural qualification exposed two existing Darwin fixture assumptions: MiniMax intake used an aliased temporary path, and product topology used GNU-only find formatting and ELF symbol spellings. Fixtures now resolve their owned temporary directory and use portable executable enumeration plus native symbol spelling. Production admission is unchanged. |
| Physical views / storage breadth | Borrowed tensor subviews, external host wrapping, sparse/virtual commitment and shared execution contexts are not admitted by this foundation. |
| Model, performance and release | No matmul, attention, quantized execution, stateful generation, 8B/14B model, performance or distribution claim. Select and qualify those independent boundaries later. |
| YAI / SDK / Studio | Separate SDK peer-credential repair is published as `5fac583`; the original Mac Studio now compiles against that immutable pin. macOS SDK process identity and complete Core/provider composition remain unqualified. No YAI source changes or Exon primary checkout mutations in this wave. |

The two generic runtime defects constrain a future model milestone; they do not
block raw backend admission, tensor lifetime or the representative primitive.
No architecture blocker prevents this bounded foundation once its final gates
are earned. Existing Clang warnings remain unrelated baseline observations.

## Reproduce and authority

Use the [native build and Metal lane](../guides/build.md#apple-silicon-metal-foundation).
QA raw logs and immutable source/build receipts remain under ignored
`build/qa/evidence`, not Git. Final source-stable receipts will identify the
qualified implementation commit and subsequent documentation-only closure.

Apple documents [shared storage](https://developer.apple.com/documentation/metal/mtlresourceoptions/storagemodeshared),
[command completion](https://developer.apple.com/documentation/metal/mtlcommandbuffer/waituntilcompleted())
and the [recommended working set](https://developer.apple.com/documentation/metal/mtldevice/recommendedmaxworkingsetsize).
Those API definitions support the storage/observation interpretation; actual
execution tests support the YVEX claim.

Architecture is owned by [backend execution](../architecture/backend-execution.md#apple-silicon-metal-foundation),
selection/closure by [Tasks](../project-control/TASKS.md#independently-selected-metal-foundation)
and bounded maturity by [Status](../project-control/STATUS.md).
This implements existing backend/platform ownership rather than selecting a
new ADR or Roadmap horizon. The Metal branch remains a reviewable pressure
branch and is not merged into main by this Task.
