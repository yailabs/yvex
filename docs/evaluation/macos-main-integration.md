<!-- docs:metadata
title: macOS Metal and Rust Shell Integration Checkpoint
id: yvex.evaluation.macos-main-integration
document: evaluation
status: mixed
owner: evaluation
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# macOS Metal and Rust Shell Integration Checkpoint

**Qualified candidate; integration into main awaits the operator's final publication.**

[Up](README.md)

`INTEGRATION.MACOS.METAL.MAIN.0` starts on 2026-10-03 from clean
`feature/macos-metal` at `09bf86abcf09401e7db67932cf906449441c4c87` and
published main `06f234b607899586a70fc2c7b3bf11c0dbcf6bd5`. The Mac's main
checkout was fast-forwarded to that published commit without source edits.
The pressure branch integrates published histories with merge, including the
later Rust terminal fixes through `0ffe591dbf07b529f95041e7cb76a73f6a559929`.
The final executable checkpoint is clean commit
`cb457a8335fe1af7cfa7cd38d7934a0001fa2211`, tree
`ca37fc06f7630dad029e106cd18f33fb221311b1`. Subsequent checkpoint documentation
does not change executable source.

The operator explicitly renewed the main-publication hold while work continued
on Spark. This checkpoint does not merge or push main, create
`feature/macos-metal2`, close the integration Task, or select a new Metal model
milestone. A clean remote checkout alone does not revoke that instruction.

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
| A cleanup fixture's shell could exec its final sleep and lose the source argv | Keep the shell alive with a trailing command, matching the existing platform fixture. Whole-argument process ownership and destructive-cleanup refusal remain tested. |

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

## Earned evidence

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

## Publication gate and next boundary

The pending actions are explicit final-main publication confirmation, fresh
refs/ancestry/working-tree inspection, merge of any further published work,
proportional qualification, main publication and only then creation of
`feature/macos-metal2` from integrated main. This checkpoint introduces no new
ADR or Roadmap horizon, closes no other Task, and modifies no YAI/SDK/Studio/HF
repository. Canonical impact is the backend architecture, build guide, QA lane
description, this evaluation route and the integration Task/Status checkpoint.

> YVEX has a real, qualified Metal backend foundation on the tested Apple Silicon target.

That boundary remains F32 embedding and owned shared storage. Matmul,
normalization, attention, recurrence, whole-model GPU generation, 8B/14B models,
performance and release readiness require later evidence.

`progression_decision=complete_evidence`, `downstream_safe=false` for integrating
main while the operator's publication hold remains. Native CPU and bounded Metal
checkpoint evidence is earned; the integration Task remains IN PROGRESS.
