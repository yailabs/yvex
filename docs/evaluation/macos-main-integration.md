<!-- docs:metadata
title: macOS Metal and Rust Shell Main Integration
id: yvex.evaluation.macos-main-integration
document: evaluation
status: current
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# macOS Metal and Rust Shell Main Integration

**Qualified foundation integrated into main; the successor pressure branch starts from that same history.**

[Up](README.md)

`INTEGRATION.MACOS.METAL.MAIN.0` starts on 2026-10-03 from clean
`feature/macos-metal` at `09bf86abcf09401e7db67932cf906449441c4c87` and
published main `06f234b607899586a70fc2c7b3bf11c0dbcf6bd5`. The Mac's main
checkout was fast-forwarded to that published commit without source edits.
The pressure branch integrates published histories with merge, including the
later Rust terminal fixes through `0ffe591dbf07b529f95041e7cb76a73f6a559929`.
The initial executable checkpoint is clean commit
`cb457a8335fe1af7cfa7cd38d7934a0001fa2211`, tree
`ca37fc06f7630dad029e106cd18f33fb221311b1`. Subsequent checkpoint documentation
does not change executable source.

The operator renewed the main-publication hold while Spark work continued, then
explicitly resumed integration after published, clean main reached
`fb13e4cf21b06b69d03468b234613f7d0ecc3fc5`. The final reconciliation merges
that history at `b878716fb5731b908cfc8ee410dc708ae17929fe`. The Darwin
instrumented-link repair ends executable work at clean
`d47f468d417b8f700b8fca28218df02d23495986`, tree
`11ef8cfd3349e8795cf54db4b8dcaa8dafacb612`.

Main and `feature/macos-metal` were published at that qualified commit in one
normal atomic push, together with new `feature/macos-metal2` created from
integrated main. Closure documentation follows without changing executable
source. The integration Task is COMPLETE at the scope below. No successor
model milestone is selected by creating a branch.

## Ownership and repaired problems

The sole product shell remains Rust. Retired C CLI files are not restored.
Metal's backend implementation, shared-buffer lifecycle, internal resource
facts and existing Qwen CPU realization remain below the same owners. Public
C layouts, protocol v24, source precision, artifact bytes and the authenticated
REPLAI pin remain unchanged by this integration.

| Observed problem | Owner and repair |
| --- | --- |
| The new backend CLI selector recognized only CPU/CUDA | Rust shell calls the common native kind parser; Metal/ROCm selection follows authoritative admission/refusal. Backend identity and resource known bits are projected from copied native reports. |
| Generated Darwin FFI lost integer typedefs with duplicate Clang builtin include trees | Rust build integration selects the C compiler's resource directory; GCC retains the existing standard-header fallback. No handwritten ABI substitute. |
| Native Apple framework flags did not reach Cargo correctly | Build owner projects Foundation/Metal as Cargo framework dependencies only on native Darwin arm64. Linux excludes Objective-C objects and Apple frameworks. |
| A long Unicode socket fixture exceeded Darwin's socket-path bound | Host PTY fixture uses a short owned canonical temporary root while retaining Unicode, width, identity and cleanup assertions. Production bounds remain enforced. |
| Short-lived Darwin PTYs lost queued output and translated line endings differently | Fixtures drain output while retaining the slave and disable only driver output translation. ANSI/plain semantic equality and geometry checks remain exact. |
| Benchmark/acquisition fixtures passed the `/var` alias into canonical-path contracts | Isolated Rust fixture roots are resolved before constructing paths; production path/refusal policy is unchanged. |
| A cleanup fixture's shell could exec its final sleep and lose the source argv | Preserve main's explicit Python owner and observed readiness. Whole-argument process ownership and destructive-cleanup refusal remain tested. |
| Darwin instrumented native objects had unresolved UBSan symbols in the Rust product | The build owner queries the selected native compiler for its actual compiler-rt dylib and adds its loader path; missing requested runtime refuses before product publication. GNU runtime linking remains unchanged. |

These are reconciled in their common build, platform-test or Rust consumer
owners, without a second Metal runtime or a model-admission bypass. The
previously identified memory-domain capacity and model-specialization boundaries
remain prerequisites for a later Metal model Task. No new architectural blocker
was found for the bounded foundation.

Initial diagnostics are retained, including a single HTTP saturation read reset
on Mac. Its subsequent source-stable full CI run passes without modifying the
HTTP production path. Initial Linux diagnostics lacked tool prerequisites and
an authenticated producer receipt; the isolated rerun supplies the exact receipt
and disposable tools. Neither diagnostic is counted as a successful run.

## Historical checkpoint evidence

The Mac is the tested macOS arm64 Apple M5 Pro with 24 GiB shared RAM. Linux
qualification uses an isolated Spark arm64 checkout
`/tmp/yvex-metal-main.tyVrcx/repo`, CUDA disabled, GNU Make 4.3 and Rust 1.98.1.
The primary Spark checkout and user services are preserved. Every passing QA
receipt below records clean, unchanged source across its run.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mac/Linux CI | Registered common unit, structural and product contracts | Separate stable intermediate trees | Preserve common build, CLI and CPU contracts | 121 PASS on each host; zero other result states | Exact registered assertions | PASS at each recorded source | Regression receipts, not final-main or release qualification |
| Mac/Linux native at `cb457a83` | CPU/platform and native Rust consumer contracts | 15 registered cases, actual host capacity | Preserve state, host, acquisition, terminal and refusal behavior | 15 PASS on each host | Exact state/cleanup, Rust tests, clippy, real PTYs | PASS | Updated native Rust shell and affected CPU boundary |
| Metal at `cb457a83` | Original host row bytes plus independent addressing; CPU is a second comparison | Existing exceptional-F32 embedding and failure fixtures | Real GPU execution with correct lifetime and refusal | 12 compute dispatches, 36 completions, zero bit mismatches; 48 allocations and releases | Exact F32 payload bits and owner/resource states | PASS | Existing Metal foundation only |
| Qwen CPU at `cb457a83` | Retained independent source-precision PyTorch/Transformers oracle | Exact 0.8B mixed BF16/F32 artifact; two raw greedy prompts, context 32, chunk 1, eight outputs each | Same IDs, detokenized bytes and typed text digests | Both complete; all 16 IDs and both decoded payloads agree | Exact bounded greedy prefixes | PASS | Two native CLI CPU continuations |
| CLI backend projection | Common native backend report and unsupported status | Mac Metal query; Linux exact Metal query | Truthful device/resources; non-native refusal | M5 Pro READY with unmeasured residency/working set; Linux exit 5 | Known bits and exact refusal | PASS | Discovery/projection and platform separation |

| Receipt | Source commit | Immutable run identity |
| --- | --- | --- |
| Mac CI 121 PASS | `13c4a16b22ffae65d918995912e36fd8eec27135` | `93631cbcb564a7504661e49edfacd1914a926ea3154e5621d56eec886e49e7b5` |
| Linux CI 121 PASS | `201501d0` | `64aacd3a8910cdf4123313541afad17fe2f65e5d1801fb452cabc48089e5c995` |
| Mac native 15 PASS | `cb457a8335fe1af7cfa7cd38d7934a0001fa2211` | `fbcc6b1a60ee01be2b9a9e78d9ef2f736d2ce25e3bf73651746f9bcb45fdb826` |
| Linux native 15 PASS | `cb457a8335fe1af7cfa7cd38d7934a0001fa2211` | `46263aae9312b2d4aeb0cf6976d2a66b453137231be8af0b5e02b5f3bb5df375` |
| Mac Metal 2 PASS | `cb457a8335fe1af7cfa7cd38d7934a0001fa2211` | `1df64397efbb3bf6f86e52ec598333b0f92eb443cfd528ae066c2e5a97d1e624` |

The CI source identities are not relabeled as the final checkpoint. The final
native receipts include the newly published Rust interaction/PTY changes.
Raw QA receipts, model requests/results and `qwen-comparison.json` are retained
outside Git under
`/Users/mothx/lab/models/evidence/integrations/macos-metal-main-20261003`.
The independent oracle is reused from immutable retained evidence, not rerun.
The [small-model evidence](macos-small-model.md) owns its original provenance,
precision restoration, artifact hash and Hugging Face publication.

| Prompt | Actual continuation |
| --- | --- |
| `The capital of Italy is` | ` Rome.\nThe capital of France is` |
| `def add(a, b):` | `\n    return a + b\n\ndef` |

Qwen observer wall times are 16.667 and 17.022 seconds including startup; maximum
RSS is 1,659,011,072 and 1,658,814,464 bytes respectively. These single runs are
observations, not performance qualification. Artifact SHA-256 remains
`0c5776eb6b1f2abb3a35f2324aabc4d8b7693856650b799e88161f7167feded6`.

Metal reports unified memory, recommended working set 19,069,665,280 bytes and
maximum buffer length 14,302,248,960 bytes. The recommendation is not physical
residency or a reserved allocation. The fixture's owned allocated/mapped and
temporary ledgers return to zero; peak temporary usage is 252 bytes. Metal API
allocated bytes are 458,752 before and after, a distinct API observation rather
than YVEX-owned allocation or physical residency. Host-write/read copies are
760,960/3,260,016 bytes and explicit device copies 1,086,672 bytes. Shared storage
has no second host/device allocation domain or external no-copy wrapping.
Actual resident bytes and working set remain unmeasured.

## Final main reconciliation

Both CI receipts at `b878716f` qualify the final published main delta. The final
native and Metal receipts at `d47f468d` additionally qualify the Darwin
instrumented-link repair; only the Rust build owner and its guide changed
between those two commits. C/CUDA operation implementations are identical.
Every passing QA receipt records clean, unchanged source. Mac native tests use
actual host capacity. Linux fixture execution explicitly opts into main's
existing declared-capacity mode; it does not qualify available Spark memory.

| Test / lane | Authority / oracle | Input / fixture | Expected | Observed | Metric / tolerance | Result | Claim supported |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Mac/Linux CI at `b878716f` | Registered common build, numerical, ownership and product contracts | 121 cases on each host; Linux declares fixture capacity | Preserve common CPU/product contracts | 121 PASS per host, zero other states | Exact registered assertions | PASS | Common integration regression; no release or actual Linux capacity claim |
| Mac/Linux native at `d47f468d` | CPU/platform and Rust consumer contracts | 15 cases per host; actual Mac capacity, declared Linux fixture capacity | Preserve host/state/admission/terminal/refusal | 15 PASS per host | Exact states, cleanup, Rust tests, clippy and real PTYs | PASS | Final native product and CPU/platform boundary |
| Metal at `d47f468d` | Original F32 payload bytes and independent host addressing; CPU comparison is additional | Exceptional-F32 embedding plus failure fixture | Real GPU execution, bit preservation, checked lifetime and cleanup | 12 dispatches, 36 completions, zero bit mismatches; 48 allocations/releases | Exact F32 payload bits and resource states | PASS | Metal F32 embedding foundation |
| Qwen CPU at `d47f468d` | Retained independent source-precision PyTorch/Transformers oracle | Exact mixed BF16/F32 0.8B artifact, two prompts, context 32, eight outputs each | Same bounded greedy IDs and decoded payload | All 16 IDs, both decoded strings and both typed digests agree | Exact prefixes and bytes | PASS | Two raw native CPU completions |
| Linux sanitizer at `b878716f` | Registered ASan/LeakSanitizer and UBSan contracts | Quant and runtime generated builds, native CPU composition | No instrumented failures or leaks | 2 PASS | Sanitizer abort-on-error and cleanup assertions | PASS | Instrumented C numerical/runtime owners; unchanged by Darwin link repair |
| Mac UBSan and missing-runtime control | Existing Make UBSan recipes; real compiled Rust build script | Quant at `b878716f`; runtime and controlled missing compiler-rt at `d47f468d` | Supported instrumentation executes; unavailable required runtime refuses | Quant/runtime recipes exit 0; unavailable-runtime exit 101 preserves product bytes | UBSan abort-on-error; exact refusal and binary hash | PASS | Darwin UBSan composition and build-input refusal; no leak/ASan/TSan qualification |
| Exact Metal model request | Registered model-command grammar | Same Qwen request with `--backend metal` | Unsupported request refuses before execution | Exit 2, invalid backend value, empty stdout | Exact request refusal | PASS | No hidden CPU execution; no Metal model admission |

| Receipt | Source commit | Immutable run identity |
| --- | --- | --- |
| Mac CI 121 PASS | `b878716f` | `4aef90f0409b158b82b715af2a77123107a853316fbca765463647612515a9d0` |
| Linux CI 121 PASS, declared fixtures | `b878716f` | `8b0945b31b4c342c59117e7e4348a4ee00f1a8b887f3fdea1d71543e66881ab0` |
| Mac native 15 PASS | `d47f468d` | `351ef1ca9613166991dabebfa16edc4969823297217c0fc9d447b30757b439f3` |
| Linux native 15 PASS, declared fixtures | `d47f468d` | `73210b3aff31ff673159d12f1a34273782ec0946e1323b156eff26aaaa965b54` |
| Mac Metal 2 PASS | `d47f468d` | `ab1e03239beb0b45ab3d6933205bef9237b68cf4729f995d28c8fc8806f3f747` |
| Linux sanitizer 2 PASS | `b878716f` | `9f0ad561c281384d6bf5cd1b66b0f02b2520b22cec800db80c9cf2a0069b4941` |

Qwen observer wall times on the final executable source are 17.051 and 17.557
seconds including startup, with maximum RSS 1,659,060,224 and 1,659,289,600
bytes. These are single-run observations, not a performance result. IDs, decoded
payloads and artifact SHA-256 remain exactly those retained above. The final
Metal fixture reports the same resource/copy facts as the historical checkpoint;
physical residency and actual working set remain unmeasured.

The first final Linux CI run retains 118 PASS and three fixture admission
failures under live memory pressure: Rust native pipeline, tiny vertical and
runtime binding. Their exact refusal protects the existing system reserve.
The subsequent clean run explicitly enables the already-published fixture mode
and passes all 121 cases. No service is stopped and production admission is not
weakened. The initial failure receipt is
`9e693dcf1012d0804f9ecaf1007f56bb31b3f43ac134940b10d8b51c1bc31a44`.

The full Mac sanitizer lane retains two FAIL results because Apple Clang arm64
rejects the existing `-fsanitize=leak` recipe. This is not relabeled PASS or
BLOCKED: receipt `003bf438347a095bb2614a88f0ac8a99f5dd5bfc09fb7f3067a761937152220f`
records the actual result. Those recipes are unchanged from published main.
Standalone Mac runtime UBSan initially exposed the missing compiler-rt link;
that build-owner defect is repaired and its real runtime/PTY rerun passes.
Generalizing sanitizer prerequisite/platform coverage remains a QA/build-owner
follow-up; leak coverage is earned by the Linux lane only.

The broad changed-owner plan also retains 33 BLOCKED external live/performance/
encoding gates: 31 live, one performance and one official-encoding test need
unavailable CUDA tooling or separately configured assets. They do not become
successful evidence for this integration. Existing prefill/CUDA/release Task
states remain unchanged. There is no whole-product or CUDA model qualification
claim from the CPU/platform Linux checkout.

Final requests, results, refusal controls, QA receipts and raw logs are outside
Git under the original evidence root's `final/` directory. Historical records
are preserved with their original source identities. The independent oracle is
reused, with SHA-256
`119a0a08bdd69f40735fcaa853e2cb18edad63a4130f9a6090c8214082434a51`.
The numerical criterion and exact artifact/source lineage remain owned by the
[small-model report](macos-small-model.md).

## Publication and next boundary

The published histories are integrated without rebase, reset, force-push or
replacement of foreign work. The Mac main checkout starts at `06f234b6` and is
fast-forwarded to the qualified integration. The pressure branch starts this
final reconciliation at `1472d03a`; the canonical remote starts at `fb13e4cf`.
`feature/macos-metal2` starts from the same integrated main, then receives only
closure documentation before independent development resumes. The primary DGX
checkout and user services are unchanged; its main Codex session can pull the
published main and continue. That session remains the canonical YVEX owner.
Further checkpoints reconcile published histories and qualify their composed
source before integration.

This closes only `INTEGRATION.MACOS.METAL.MAIN.0`. Task counts become 43 selected,
35 complete, zero in progress, one ready and seven blocked; the independently
closed Rust-shell Task and blocked prefill Task retain their published states.
No new ADR or Roadmap horizon is selected. Canonical impact is the common
backend/Rust-build architecture, build guide, QA scope, Evaluation and Task/Status.
No YAI, SDK, Studio or Hugging Face repository is modified by this integration.

> YVEX has a real, qualified Metal backend foundation on the tested Apple Silicon target.

That boundary remains F32 embedding and owned shared storage. Model-capacity
and specialization lifetime still assume CPU/CUDA in their canonical owners;
model-command admission also excludes Metal. Those generic boundaries require
capability and physical-memory-domain semantics, not another kind conditional.
Matmul, normalization, attention, recurrence, whole-model GPU generation,
conversation templates, 8B/14B models, performance and release readiness require
later implementation and independent evidence. No full Qwen GPU claim follows.

`progression_decision=proceed`, `downstream_safe=true` only for this qualified
main integration, native CPU/product boundary, retained Qwen CPU prefixes and
Metal F32 foundation. Whole-product/release gates and full-model Metal remain
unqualified; no next implementation Task is automatically selected.
