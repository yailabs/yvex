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
remain unchanged. The qualified implementation ends at
`428e8e8e9548566d06e785acbc4b03b92f22852c`, tree
`0f611848ba2baeabbe70a1ec81863b27c98824c7`; subsequent closure changes only
documentation. The Task is complete at the bounded foundation below.

> YVEX has a real, qualified Metal backend foundation on the tested Apple Silicon target.

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

Source-stable qualification on the tested target passes both Metal tests. Widths
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
| `metal.foundation` | Original host bytes with independent row addressing, plus CPU operation | Three width/token pairs, repeated IDs and exceptional F32 payloads | GPU result, copied result and zeroed result publish after completion | 12 compute dispatches, 36 completed commands, all expected bytes | Zero F32 bit mismatches | PASS | Existing F32 embedding and bounded storage |
| `metal.failure` | Typed capability/status and owner pointers | Device/queue/pipeline/allocation/command/encoder/completion hooks and exact byte budget | Fail closed, retain retryable cleanup, no hidden CPU execution | All refusal/cleanup assertions pass | Exact states, pointer retention and budget ledger | PASS | Failure propagation and storage lifetime |
| `native` | Existing CPU/platform/host/terminal contracts | 14 registered cases, actual local memory | Native CPU path remains healthy | 14 PASS on Mac and Linux at the final implementation commit | Zero FAIL/SKIP/BLOCKED/ERROR | PASS | Affected macOS CPU regression |
| Linux isolation | Same common code, native compiler and existing CPU contracts | Disposable checkout, CUDA disabled, no Apple toolchain | CPU gates pass; exact Metal refuses; Metal lane is BLOCKED | CI 120 PASS; final native 14 PASS; exact CLI refusal exit 5; Metal 2 BLOCKED | Explicit non-native refusal; no fake GPU PASS | PASS for CPU/separation; GPU BLOCKED | Platform separation and Linux CPU regression |
| Structural / build | Manifest, archive, public ABI, generated registry and build dependency rules | Native product and disposable build fixtures | Same source/layer/publication contracts | 17 structural PASS; final ownership/architecture/natural checks PASS; build 8 PASS and 1 explicit absent-nvcc SKIP | Exact ownership, dependency invalidation and failed-publication preservation | PASS at declared scope | Platform/build integrity |
| Metal host ASan/UBSan | Instrumented C/Objective-C implementation and real GPU tests | Both foundation/failure cases, including borrowed-view self-copy/release | No host memory or undefined-behavior errors | Both tests PASS | Fail on sanitizer error; leak detection unavailable/disabled on Darwin | PASS | Host implementation safety; shader/physical residency not instrumented |
| Linux sanitizers | Registered CPU quant/runtime gates | ASan, LeakSanitizer and UBSan fixtures | Retain existing CPU safety contracts | Quant and runtime PASS; runtime initial dependency fetch timed out and passed with the identical already-staged producer | Separate source-stable case receipts | PASS per qualified case | CPU safety, not general CUDA/Metal/model breadth |
| Wider Mac CLI diagnostic | Existing managed source-publication contract | Local HF-style directory snapshot | Atomic managed adoption | Existing source owner returns unsupported | Exact refusal at source.distribution.copy | FAIL outside foundation | Whole Mac CLI/model-acquisition is unqualified |

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
| API current allocation | 458,752 bytes before and after the representative workload | Device API observation for this process; precise allocation composition, system-wide free memory and physical residency are not measured |
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
| CLI qualification fixtures | The isolated Linux renderer lacked its pinned REPLAI runtime search path; Darwin direct fixture builds omitted native feature/section-link flags and assumed GNU script/stat. They now use native flags, explicit dependency lookup and the existing real PTY recorder. Output-only capture does not inject an EOF key. Injected model-test environment is passed with `env` to the process instead of relying on interpreter-dependent assignment lifetime around shell functions. All remain test-owned changes. |
| `source.distribution` / managed local directory adoption | The broader Mac CLI diagnostic reaches an existing Linux-only atomic directory publication in `src/source/distribution.c`; managed snapshot adoption refuses `YVEX_ERR_UNSUPPORTED`. The same implementation is present at starting main `67a7905`. A separate canonical source-owner correction must use the platform no-replace publication contract and qualify failed replacement/cleanup. It is outside primitive backend execution, remains unchanged here and prevents a whole Mac CLI/model-acquisition claim. |
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
`build/qa/evidence`, not Git. Every successful qualification receipt below has
clean, unchanged source across its run. Raw evidence is retained locally and
in the disposable Exon checkout `/tmp/yvex-metal-qualification.aMP8CU/repo`;
no primary Exon/Spark checkout or running user service was replaced.

| Qualification | Source commit | Immutable run identity |
| --- | --- | --- |
| Mac Metal 2 PASS | `428e8e8e` | `6600f9d26731f37714bb1d9614dcf251385a1aaedfb8b76714d18ca98535b918` |
| Mac native 14 PASS | `428e8e8e` | `e95f088f50f5f9c47269ed10dc2e4b22578953e0b026f3ea9563b8a9e9fdeeb2` |
| Linux native 14 PASS | `428e8e8e` | `968048683420aa0908f7be5d30aae8b89afcd1e97f431d5d6ef96e9699f10bdd` |
| Linux Metal 2 BLOCKED | `428e8e8e` | `915eb92beccf81e6ed534708f69eaf90bcec8e4ab2a69df7e36f612223171648` |
| Mac structural 17 PASS | `4ff8ba3c` | `b0e03016d3f44a09bb5ab89470f8cea149b27ed3d61ad295a7c329a6e38ed1f4` |
| Linux CI 120 PASS | `4ff8ba3c` | `683a05a2a9498134a44ee95a58e2e62ada38f0dee4fad2de48a12e6296480ab5` |
| Linux runtime sanitizer PASS | `4ff8ba3c` | `03310eeb3352224bc37165869b14d61bed0c6797234907a3829a3bc833507bb1` |
| Linux quant sanitizer PASS case | `043abafb` | `1c4c532038a7dc31b7a1cca91685bdaf5e25c07035ccbce9299cdce5c215af0a` |

The last receipt is a mixed lane: quant passed while runtime failed solely at
a timed-out producer fetch. It is not an overall green sanitizer lane. The
later runtime receipt passes with the same pinned producer through an explicit
staging prefix. Between `4ff8ba3c` and final code `428e8e8e`, only native Metal
self-copy ownership and its Metal-only assertion changed; common CPU/CUDA and
build code is identical. Final native qualification on both hosts, GPU tests,
host ASan/UBSan and ownership/architecture/natural checks reconfirm that delta.
Mac final build identity is
`3e41e7ea61a2d01b0bfdbbe32b2890288ef554f6987be6f77db9e68d3745fbf0`;
Linux final native identity is
`3eb128d84d37ae2b809664107427b1ab2e267f233f8e0452268a9fd76e7ac3f2`.

The broader Mac CI diagnostic at `90e91475` was 119 PASS / 1 FAIL (run
`79e160598590e03c12a6ed27549f4915bcd5e15133d5a7e4667fd19285ac5aa7`).
The later CLI diagnostic at `043abafb` exposed the remaining simulated-provider
environment leak (run `422c72a8ab3f5da3ef2e31a033fa3ebaa9bb3e0d2618b5724ec761ed96aaef55`).
After its repair, the standalone workflow diagnostic reaches the existing
unsupported managed directory publication described above. No whole Mac CI/CLI
pass is claimed. The authoritative local regression lane for this foundation
is `native`, with actual memory and no fixture capacity envelope.
The generic changed-owner planner also selects CUDA/model performance evidence;
this Task explicitly claims no performance qualification and does not provide
those external model/binding/benchmark assets. Missing performance/release
claims are not filled by the primitive or CPU gates.

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


`progression_decision=proceed`, `downstream_safe=true` only for the qualified
backend admission, owned shared storage, F32 embedding primitive and retained
native CPU boundary. No exact blocker remains for this foundation. Model,
performance, release and complete YAI/SDK/Studio composition remain separate.
