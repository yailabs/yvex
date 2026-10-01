<!-- docs:metadata
title: Native macOS Qualification
id: yvex.evaluation.macos-native
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# Native macOS Qualification

**Darwin CPU, local host and terminal lifecycle, with explicit remaining gates.**

[Up](README.md)

`PLATFORM.MACOS.NATIVE.0` was selected independently on 2026-09-30 and
implemented on `feature/macos-native` from published main
`5d84349f8774a4273ee5f4ac4ce3a05ce99c64e1`. The native changes preserve
protocol v24 and installed public layouts. Published CLI/REPLAI refoundation
was integrated from main `6c5229594d523a7976ea82319c225ae20e514c73`, including its
authenticated REPLAI producer `93d62f6d34cfb933a1f59407ade027152e1ef2ba`.
The native Task does not repin that dependency. Exon YAI/Studio and Spark
sessions retain their own worktrees and authority. Linux qualification runs in a disposable Exon checkout,
without replacing the primary YVEX source or daemon.

The local qualification host is macOS 26.6.2 arm64, Apple Clang 21, 24 GiB RAM,
16 KiB VM pages, GNU Make 4.4.1, Python 3.14 and Rust 1.98.1. Linux regression
uses Exon x86_64, its native C compiler, GNU Make and the same Rust producer.
The reproducible commands are in the [build guide](../guides/build.md#macos-native-cpu-build).
The registered `native` lane selects 13 CPU/host/terminal tests. Each evidence
receipt under `build/qa/evidence` records source HEAD, delta, stability and exact
build identity; generated fixtures and raw logs remain outside Git.

## Problems repaired

| Observed problem | Repair and retained contract |
| --- | --- |
| Linux-only stat fields and process APIs prevent Darwin compilation or self-supervision | Internal `core.platform` mechanisms preserve nanosecond snapshots, boot/process identity, self-executable discovery, peer UID authentication and process I/O observations. |
| GNU archive flags and object member names do not work with Apple's archiver | BSD extended member names preserve source-relative ownership; native `ar` owns the symbol index and Apple's linker consumes the archive. |
| REPLAI staging assumes a Linux library | Stage the producer's existing Darwin dylib; bind the receipt to host OS/architecture. No dependency repin. |
| `/tmp` and `/var` are OS symlinks; default temporary paths end in `/` | Accept only verified root-owned OS aliases, then walk all application components without following symlinks. Test cleanup canonicalizes those aliases before creating resources. |
| `realpath -m/-ms` is unavailable and two empty command results could admit unsafe packaging | A checked portable path guard rejects application symlink ancestors before cleanup or package replacement. Build contract fixtures prove foreign files survive refusal. |
| `posix_fallocate`, exclusive rename, cache release and procfs RSS assumptions differ | Native preallocation and exclusive rename preserve publication. RSS uses Darwin bytes. Explicit cache eviction refuses unsupported; optional eviction is omitted. |
| Linux memfd seals and mremap are unavailable | Read-only, unlinked, process-owned prefix backing and Mach fixed remap preserve sparse protections, rollback and session COW. Darwin does not claim kernel memfd seals. |
| 64 KiB fixture state pools cover 16 Linux pages but only four native pages | Budget fixtures in native pages; production accounting remains unchanged. |
| Quantized monotonic readings can make an empty profile scope zero-length | Use Darwin's high-resolution monotonic raw timebase. No fabricated duration. |
| Background SIGINT may be ignored; cross-thread close may leave accept blocked | CLI serve owns blocked signal dispositions; the listener uses bounded nonblocking readiness and retires its descriptor after its accept owner stops. |
| BSD script rejects FIFO input; Mach-O symbols carry a leading underscore | A real PTY recorder preserves child status, and symbol audits normalize the native ABI before applying the same ownership rules. |
| Darwin revokes a session leader's PTY after exit | A test-only destructor records the real terminal flags before revocation, without changing them. Restoration remains a required assertion. |
| Scheduler observation spins may finish before the new thread runs | Use a bounded sleeping observation interval; require the same runnable/physical-width assertions. |
| Hosted runner memory cannot preserve the existing minimum reserve for tiny model admission | An explicit test-only capacity mode supplies a declared envelope inside the binding and tiny-vertical fixtures. Default local runs retain actual host admission; native platform memory observations and resource refusal assertions remain independent. Production reserve policy is unchanged. |
| Clang identifies potentially uninitialized refusal-path values | Initialize checked arithmetic inputs and refuse an invalid server owner before reading a return code. |

## Evidence

Source-stable native and Linux confirmation passed on 2026-10-01: 13 PASS,
zero FAIL/SKIP/BLOCKED/ERROR on each host. Qualified code commit
`6f20258e348a826cbc872b21aea227da1e5ebb8f` has source tree
`27ed1ee4f62e610b1cc9d93eeb063aa7120df9ee`; the isolated Linux checkout has
the identical tree. Mac run identity is
`d82edfbbb74c4d4ae37b72ae5ab227f4f21fcba0cf0a5ce7eae2736e27779c6d`;
Linux run identity is `01cc6d932a798c0fb18f8257c4ea59090ab771dd54f66cc647467ef42a9182fd`.
Mac build identity is `8c89b02957147c6acb3d63549451b1a42a25c40a79d1bcd84ac3e10f556a8fa5`;
Linux is `b71ac4cc4c5419df4ce71f8bf94dcbfc33a3cc026e51418d2e652ecc29208ce1`.
A subsequent test-cleanup guard also refuses the checkout root when the checkout
itself lives beneath a temporary `yvex-*` directory. Its Linux and Mac
architecture/build contracts pass; it changes no production runtime code.
Runs performed while the source delta changed are diagnostics only, even when
all individual tests pass.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `unit.platform`, filesystem and artifact | Native OS APIs, bytes and descriptor access | Temporary files, socket pair, sparse VM mappings | Exclusive publication, immutable descriptor, preserved failed replacement and COW | Native mechanisms exercised | Exact bytes, permissions and page protections | PASS | Darwin platform mechanisms, not model quality |
| Runtime binding/state/sequence tests | Authenticated bindings and independent sessions | Bounded generated state | Attach/reset/checkpoint, isolation and transactional rollback | CPU lifecycle exercised | Exact identities/state | PASS | Native state lifetime |
| Source acquisition lifecycle | Real detached supervisor, bounded fake provider | Completion, disconnect, stall, crash, stop/resume | Owner identity and cleanup survive client loss | All lifecycle cases exercised | Exact typed states and generation counts | PASS | Native supervision; no remote provider throughput claim |
| Real REPL PTY | Real terminal, typed fake host and descriptor observations | Unicode edit, paste, cancellation, repeated submissions | Restored termios, balanced paste and bounded descriptor lifetime | Full terminal interaction exercised | Saved terminal flags equal; TTY fd counts bounded | PASS | Native terminal lifecycle |
| Tiny production vertical | Real compiler, authenticated artifact/binding, CPU backend and typed transport | Generated deterministic model, input `a`, context 8 | Provider progress, decoded output, next prompt and complete shutdown | `okokok`, session position 5, next prompt; SIGINT 0.748 s | Exact output and identity; shutdown deadline 10 s | PASS | Actual bounded CPU execution and host/CLI composition |
| Structure/build/registry | Manifest, compiler, native symbols and real Make rules | Current source and disposable build/package fixtures | Same owner/ABI policy and failed-publication preservation | Contracts exercised | Exact membership and foreign-file survival; CUDA compiler case may SKIP | PASS | Build and source integrity |
| Linux `native` regression | Existing Linux mechanisms and same assertions | Isolated x86_64 checkout | All selected CPU/terminal/host cases pass | 13/13 selected tests pass on the identical source tree | No unsupported case hidden as PASS | PASS | Retained Linux behavior |

The fixture artifact SHA-256 is
`a946a8447534b15556e9b6d38c57cfee2bb3f5f05ffa5932ac9c672b7641341d` and its
binding is `5911a93e48dbe4b993b13fae4b3e2773f688db02ec125de92227f145f41c9884`.
This model deliberately has a deterministic small output. It exercises the
real compiler/runtime path; it is neither an 8B/14B checkpoint nor an upstream
whole-model behavior oracle.

### Hosted CI capacity boundary

The first [hosted run](https://github.com/yailabs/yvex/actions/runs/36847530428)
completed the Linux hermetic lane with 119 PASS and valid source stability;
its subsequent real-chat Valgrind step also passed. On macOS 15.7.9 arm64,
Apple Clang 17, native results were 10 PASS and 3 FAIL: tiny-vertical and
runtime-binding could not preserve the product's existing 8 GiB minimum
system reserve from available host memory; runtime-generation exhausted a
busy observation loop before its drain thread ran. That failed run is diagnostic
evidence, not qualification.

The macOS workflow now opts into `YVEX_TEST_FIXTURE_CAPACITY=1`. Only
`unit.runtime_binding` and `integration.tiny-vertical` install a declared
128 GiB total/available admission envelope through the existing test hooks,
inside their own test processes. The binding owner clears the envelope after
its case and restores it only after its explicit low-capacity, proportional
reserve, process-limit and just-in-time refusal assertions. The fixtures refuse
to replace caller-injected capacity facts. Other cases, including
`unit.platform`, still observe the actual native kernel and memory mechanisms.
Scheduler observation waits sleep for 1 ms with a bounded retry budget and retain
their exact assertions. Production memory and scheduling policy are unchanged.

Both actual-memory and declared-capacity local reruns pass the affected binding,
generation and real tiny CPU/terminal vertical. Hosted results belong to their
exact workflow source snapshot. A hosted pass with this declared envelope
qualifies native fixture execution and lifecycle; it does not qualify the
runner's actual memory admission or promise a usable 8B/14B model.

## Remaining problems and next boundaries

| Boundary | Current problem / required evidence |
| --- | --- |
| Metal | No implemented Metal execution backend. Native Darwin support does not add GPU kernels, allocator/submission or independently admitted numerical classes. |
| Useful small conversation model | No 8B/14B Mac conversation model is qualified. The existing 7B Mamba-Codestral CPU family lacks hosted conversation support; a standard small GGUF cannot bypass family admission/template contracts. Select an exact supported checkpoint and earn its separate execution/conversation gates. |
| YAI, SDKs and Studio | This Task qualifies YVEX only. The complete Mac product chain remains untested against the unpublished Exon/Spark waves. Align after those authoritative pushes, then test consumer startup and real turns. |
| Darwin explicit cache eviction | Returns unsupported. F_NOCACHE changes future caching and is not proof of eviction or a cold-load benchmark. |
| Memory capacity | Kernel free/reclaimable percentage is rounded down; fallback free/inactive pages is conservative. Existing reserve admission still applies. This is not a promise that all reported pages are immediately unused. |
| Hosted macOS CI | A pinned-action macOS job runs native and build contracts. Its two model-admission fixtures use an explicit declared capacity envelope. Hosted fixture evidence is separate from the actual-memory local arm64 qualification and does not establish runner memory admission. |
| Wider platforms and release | Other macOS versions/architectures, GPU/full-model performance, Windows and distribution/legal qualification remain independent. Existing nonliteral-format Clang warnings are not removed by this portability Task. |

## Build and host observations

Source ownership, natural structure, architecture, registry, repository/build
contracts and canonical documentation validation pass on the local Mac. The
Linux ownership/layout/natural/architecture/registry/build contracts pass with
`TMPDIR=/tmp`; each host's seven-case build suite has six PASS and one explicit
SKIP for its absent CUDA compiler. CUDA is not a native CPU gate.

During Linux confirmation, Exon's `/` filesystem reported zero user-available
space (100% usage), while `/tmp` is a separate tmpfs with 13 GiB available.
Build fixtures using the host's default `/home/mothx/.cache/tmp` failed with
ENOSPC. Repeating them under `/tmp` passes. This is an observed host storage
problem for the next Exon work session; no unrelated files were deleted.

## Mechanism authority

Apple's [fcntl documentation](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fcntl.2.html)
defines native preallocation and distinguishes F_NOCACHE from eviction.
XNU's [memory-pressure interface](https://github.com/apple-oss-distributions/xnu/blob/main/doc/vm/memorystatus_notify.md)
and [VM implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/vm/vm_pageout.c)
expose the free/reclaimable memory estimate. The
[Mach VM implementation](https://github.com/apple-oss-distributions/xnu/blob/main/osfmk/vm/vm_map.c)
owns remap semantics; the local native tests verify protections and failure
preservation rather than substituting documentation for execution evidence.

Architecture changes are documented in [backend/platform execution](../architecture/backend-execution.md#native-platform-mechanisms).
Task and maturity closure stay in [Tasks](../project-control/TASKS.md#independently-selected-macos-qualification)
and [Status](../project-control/STATUS.md#interfaces-and-portability).
No cross-repository source contract changes. The dependency update belongs to
the separately published CLI refoundation, whose native consumers were requalified.

No new ADR or Roadmap horizon is selected: native mechanisms implement the
existing platform boundary. Code, architecture, Task/Status and evaluation
owners close together; hosted fixture capacity remains a separate evidence boundary.

`progression_decision=proceed`, `downstream_safe=true` for the qualified native
CPU fixtures, host and terminal boundary. Metal, small-model conversation,
YAI/SDK/Studio composition and release remain independently unqualified.
