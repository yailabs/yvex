<!-- docs:metadata
title: YVEX Tasks
id: yvex.project-control.tasks
document: project-control
status: live
owner: project
audience: [engineer, agent, evaluator]
publication: {html: true, pdf: true, index: true}
-->

# YVEX Tasks

**Delivery progression and selected work, with earned exits.**

[Up](README.md)

Selected program: **YVEX shared compiler/runtime delivery**.
Current phase: **interface refoundation COMPLETE; selected A03 execution is READY**.
The next selected implementation boundary remains **A03 encoder-decoder**;
it is READY, with no implementation started by this independently authorized
interface Task Pack.

The independently authorized YVEX repair
`RUNTIME.CUDA.MOE.NUMERICAL.CORRECTNESS.0` has a qualified bounded producer and
component repair, but its full mandatory QA gate is BLOCKED by the unavailable
legacy bootstrap-Q2 attention artifact. The historical deleted-binary failure
does not reproduce on the identified canonical build; its cause is not inferred.
[Dated evidence](../evaluation/retained-observations.md#deepseek-cuda-producer-reconciliation-2026-09-29)
separates that observation from the demonstrated decoded-projection defects,
numerical repair, refusal and cleanup. A03 remains READY.

`V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` was explicitly resumed and its bounded
implementation/measurement exit is earned: generic ordered-dot thread ownership
reduces the repeated 22-input/3-output control from 30.72 to 8.32 seconds HTTP
and 22.19 to 4.20 seconds prefill, preserving numerical and lifecycle controls.
The Task remains BLOCKED only on the mandatory legacy bootstrap-Q2 aggregate,
not on a reproduced failure of the current mixed producer. [Dated evidence](../evaluation/retained-observations.md#deepseek-gb10-optimization-2026-09-30)
retains exact provenance, qualification and limits. No YAI Case retry,
cross-repository change or A03 execution occurred.

`V010.RUNTIME.DEEPSEEK.GB10.INFERENCE.PIPELINE.1` has a bounded measured
implementation exit: canonical physical-row arenas remove large per-request
preparation/teardown, and generic paired traversal/ordered-dot dispatch remove
repeated work without changing the numerical class. Fixed none/high/maximum,
bounded decode and 100-token prefill controls improve complete HTTP latency;
explicit target/DSpark controls preserve the first-token/complete-turn tradeoff.
[Dated evidence](../evaluation/retained-observations.md#deepseek-gb10-inference-pipeline-2026-09-30)
retains source-stable samples, numerical/lifecycle controls and the concrete next
compiled phase-width constraint, not a hardware-floor claim. The independently
requested Makefile review separates build/QA/docs/distribution owners and
qualifies incremental dependencies, flag invalidation and manifest-derived
DESTDIR installation; it does not confer release/legal readiness. Full mandatory
QA is not green: exact external assets, including the legacy bootstrap-Q2
aggregate, remain absent; four broader numerical assertions also fail on the
unchanged baseline (production DeepSeek CPU/CUDA and Qwen readout). The forensic
CPU/CUDA control remains exact, not a replacement for those failing assertions.
The requested official-vector addition authenticates upstream encoding/parsing
and native BPE/request-prefix evidence, not unavailable official full-model
logits. No additional optimization, YAI/SDK/Studio edit, Case
retry or A03 execution is selected.

An independently authorized cross-repository Task Pack,
`PLATFORM.SDK.PARITY.REFOUNDATION.0`, is in progress for public client/parity
integration. Its first YVEX slice projects the exact remote-management v1
operation set from the existing registry, qualifies the SDK consumer against
the real isolated SSH fixture, and converges published development onto
`main`. It does not make remote mutations, model generation or A03 complete.

<!-- task-counts:start -->
| Total selected | Complete | In progress | Ready | Blocked |
| ---: | ---: | ---: | ---: | ---: |
| 40 | 32 | 1 | 1 | 6 |
<!-- task-counts:end -->

**32/40 selected Tasks complete.** This denominator includes the significant
retained delivery sequence, two independent completed interface Tasks and the
completed CLI/REPLAI refoundation and native macOS qualification. It is not
YVEX product completion. Research
candidates are not selected Tasks.

## Independently selected macOS qualification

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `PLATFORM.MACOS.NATIVE.0` | Native macOS build, authenticated local host and CPU lifecycle | P1 | ✅ COMPLETE | Native CPU, authenticated host, acquisition/state and real CLI/PTY qualified on Mac and Linux; 13 PASS per host plus ownership/build/documentation contracts; [evidence](../evaluation/macos-native.md). Metal, full-model conversation and YAI/Studio/SDK composition retain independent gates. |

The operator selected this Task on 2026-09-30. Implementation used
`feature/macos-native` and is integrated in `main` by `67a7905`. Published Spark CLI/REPLAI refoundation at `6c522959`
is integrated; unpublished work and Exon primary checkouts remain untouched.
Source ownership and existing protocol meanings remain authoritative.
`progression_decision=proceed`, `downstream_safe=true` for bounded native CPU,
host and terminal execution only; no next platform/model wave is selected.

## Independently selected Metal foundation

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `METAL.BACKEND.FOUNDATION.0` | Real Apple Silicon Metal execution through the common backend lifecycle | P1 | ✅ COMPLETE | Real M5 Pro device/pipeline admission, shared-buffer lifetime, exact F32 embedding, refusal/cleanup and resource observations qualified; Metal 2 PASS, native 14 PASS on Mac/Linux, Linux CI 120 PASS. Model, attention, projection and performance remain unqualified; [evidence](../evaluation/macos-metal.md). |

The operator selected this Task on 2026-10-01 for `feature/macos-metal`,
starting at `67a7905ea9deb98b0704629a1f979634e19007fb`, clean and aligned
with published `main`. Native macOS qualification is integrated in `main`;
primary Linux/CUDA and other repository work remain separately owned.
Qualified implementation ends at `428e8e8e9548566d06e785acbc4b03b92f22852c`;
source-stable evidence and precise earlier/common-code receipts are recorded in
the evaluation owner. Generic kind parsing/resource observation was repaired
inside backend ownership. CPU/CUDA-only runtime capacity/model-specialization
and existing Mac managed-directory publication remain separate canonical
boundaries; no workaround or model promotion was introduced.
`progression_decision=proceed`, `downstream_safe=true` only for this primitive
backend foundation and retained native CPU behavior. No next milestone is selected.

## Independently selected macOS small-model execution

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `MACOS.SMALLMODEL.CLI.0` | Acquire real small-model tensors, compile, load and capture native generation on the Mac; publish the resulting qualified representation on Hugging Face | P1 | 🔵 IN PROGRESS | Operator resumed the exact small-model boundary: admit pinned Qwen 0.8B source and tied-parameter semantics through the common family/compiler catalog, qualify native CPU execution, then earn release publication. Earlier Mamba memory refusal and zero generated bytes remain historical evidence until a real successful run exists. [Exact evidence and owner boundaries](../evaluation/macos-small-model.md). |

The operator independently selected this outcome on 2026-10-01 after Metal
foundation closure. Begin with the existing source-qualified
`mistralai/Mamba-Codestral-7B-v0.1` target and its pinned revision, using the
existing CPU deployment and normal CLI lifecycle. The separate Metal foundation
does not confer model admission. Preserve main and concurrent primary work;
do not invent a chat template, numerical relaxation or Metal-only runtime.
Hugging Face publication is authorized for the resulting representation, once
its exact license, lineage, runtime scope and remote integrity are established.
The attempted native binary reports clean selection commit `9ab4be5a`; no
production policy or architecture workaround was introduced. The requested
load/generation/publication outcome remains blocked, while downloaded tensors
and the compiled Mamba artifact are retained. `progression_decision=blocked_external`,
`downstream_safe=false` for that requested outcome; earlier foundation closure
is unchanged.

The operator resumed this Task after reviewing those blockers. Continuation
starts at clean `10ac0d8d` and owns the canonical source/family/compiler changes
needed for the exact acquired Qwen 0.8B text checkpoint, including common
multi-target registration if required. These changes are backend-neutral,
reviewable independently of Metal, and must preserve the admitted 27B target.
No Metal-model promotion, relaxed memory reserve, invented source sidecar or
hidden fallback is authorized by this continuation. Real CPU load/generation
and exact release evidence remain the exit; publication authorization persists.

## Delivery progression

1. Shared architecture and compiler foundation — qualified ownership cutovers.
2. Pure-SSM and acquisition — exact Mamba execution; supervised immutable intake.
3. Readout and runtime — recurrent/hybrid scoring, state identity and capacity.
4. Native finite decision — Laya computation and local text-frontier producer.
5. Current selected continuation — A03; documentation qualification is complete.

Task states: ✅ COMPLETE · 🔵 IN PROGRESS · ⬜ READY · ⛔ BLOCKED.
Historical identifiers retain their original spelling, including former Wave
and reconciliation names. A Task Pack is a temporary selection, not a new layer.

## Shared architecture and compiler foundation

✅ COMPLETE at the bounded exits below; completion does not promote a whole program.

<details>
<summary>Completed Tasks and earned exits</summary>

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `MAINTENANCE.ARCHITECTURE.REASSESSMENT.0` | Identity, client portability, architecture integrity | Retained | ✅ COMPLETE | [Earned exit](#maintenancearchitecturereassessment0) |
| `NATIVE.COGNITIVE.STATE.ALIGNMENT.0` | Adopted dual-stream target and evidence-reporting discipline | Retained | ✅ COMPLETE | [Earned exit](#nativecognitivestatealignment0) |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` | Unique device-result validity owner | Retained | ✅ COMPLETE | [Earned exit](#maintenancearchitecturerefoundation0) |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0` | Source-stable lifecycle, numerical component and bounded replay evidence | Retained | ✅ COMPLETE | [Earned exit](#maintenancearchitecturerefoundationqualification0) |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` | Typed computational language, explicit state/effects, program/component and physical/target lowering… | Retained | ✅ COMPLETE | [Earned exit](#maintenancearchitecturerefoundation1) |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0` | Broad consumer evidence and comparable post-cutover replay | Retained | ✅ COMPLETE | [Earned exit](#maintenancearchitecturerefoundation1qualification0) |

</details>

**Phase exit:** the named delivery outcomes are qualified; broader capability limits remain in Status.

## Pure-SSM and acquisition lifecycles

✅ COMPLETE at the bounded exits below; completion does not promote a whole program.

<details>
<summary>Completed Tasks and earned exits</summary>

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `SPECTRUM.MAMBA2.REPAIR.0` | Source-owned token/numerical policy and pure-SSM compiler/runtime execution | Retained | ✅ COMPLETE | [Earned exit](#spectrummamba2repair0) |
| `SPECTRUM.MAMBA2.QUALIFICATION.0` | A01 artifact and exact model execution at earned scope | Retained | ✅ COMPLETE | [Earned exit](#spectrummamba2qualification0) |
| `PROJECT_CONTROL.POST.A01.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlposta01reconciliation0) |
| `MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0` | Supervised source acquisition, truthful typed progress/health and operator projection | Retained | ✅ COMPLETE | [Earned exit](#maintenancesourceacquisitionlifecycle0) |
| `PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0` | Decision Readout/Core target adopted and one bounded successor selected | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostacquisitionreconciliation0) |

</details>

**Phase exit:** the named delivery outcomes are qualified; broader capability limits remain in Status.

## Finite readout and common runtime

✅ COMPLETE at the bounded exits below; completion does not promote a whole program.

<details>
<summary>Completed Tasks and earned exits</summary>

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `DECISION.READOUT.ZERO.DECODE.0` | Decision / option readout advances to PARTIAL at one exact internal model/state scope | Retained | ✅ COMPLETE | [Earned exit](#decisionreadoutzerodecode0) |
| `PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostdecisionreadoutreconciliation0) |
| `DECISION.READOUT.QWEN.ADMISSION.0` | Prerequisite only | Retained | ✅ COMPLETE | [Earned exit](#decisionreadoutqwenadmission0) |
| `PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostqwenadmissionreconciliation0) |
| `DECISION.READOUT.EXECUTION.PROFILE.0` | Prerequisite only | Retained | ✅ COMPLETE | [Earned exit](#decisionreadoutexecutionprofile0) |
| `PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostdecisionreadoutprofilereconciliation0) |
| `DECISION.READOUT.SESSION.STATE.IDENTITY.0` | Correctness prerequisite only | Retained | ✅ COMPLETE | [Earned exit](#decisionreadoutsessionstateidentity0) |
| `PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostdecisionreadoutsessionstatereconciliation0) |
| `DECISION.READOUT.QWEN.BREADTH.0` | Second exact model/state-class readout qualified | Retained | ✅ COMPLETE | [Earned exit](#decisionreadoutqwenbreadth0) |
| `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` | Common execution capacity, resource truth and retained cleanup | Retained | ✅ COMPLETE | [Earned exit](#maintenanceruntimeexecutionconsolidation0) |
| `PROJECT_CONTROL.POST.RUNTIME.EXECUTION.CONSOLIDATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostruntimeexecutionconsolidation0) |

</details>

**Phase exit:** the named delivery outcomes are qualified; broader capability limits remain in Status.

## Native finite decision and consumer seam

✅ COMPLETE at the bounded exits below; completion does not promote a whole program.

<details>
<summary>Completed Tasks and earned exits</summary>

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `SYSTEM.MODEL.LAYA.0` | Native finite-decision model computation and A12 advance to PARTIAL at one exact CPU/token-domain scope | Retained | ✅ COMPLETE | [Earned exit](#systemmodellaya0) |
| `PROJECT_CONTROL.POST.SYSTEM.MODEL.LAYA.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostsystemmodellayareconciliation0) |
| `FINITE.DECISION.PRODUCER.0` | Consumer-safe local finite-decision producer at one exact model/input scope | Retained | ✅ COMPLETE | [Earned exit](#finitedecisionproducer0) |
| `PROJECT_CONTROL.POST.FINITE.DECISION.PRODUCER.RECONCILIATION.0` | Project-control selection only | Retained | ✅ COMPLETE | [Earned exit](#project_controlpostfinitedecisionproducerreconciliation0) |

</details>

**Phase exit:** the named delivery outcomes are qualified; broader capability limits remain in Status.

## Independent interface deliveries

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `MANAGEMENT.REMOTE.IDENTITY.ENROLLMENT.0` | Restricted remote identity/status | Retained | ✅ COMPLETE | [Read-only bootstrap](../contracts/remote-management.md); mutation unselected |
| `PROVIDER.PROGRESS.TRANSPORT.0` | Preserve real progress through transport | Retained | ✅ COMPLETE | [Serving correction](../evaluation/retained-observations.md#provider-progress); no long-request promotion |

`PROVIDER.PROGRESS.TRANSPORT.0` is the delivery identity assigned here to the
already completed correction at `6ae29730`; it does not invent a historical ID.

## Current phase — documentation and selected continuation

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `PROJECT.DOCS.YVEX.REFOUNDATION.2` | Qualify the YVEX documentation architecture | Closed | ✅ COMPLETE | [Qualification](../evaluation/documentation-migration.md): coverage, Markdown/Mermaid, HTML/PDF, benchmarks and mapped QA |
| `INTERFACES.CLI.REPLAI.PRODUCT.SURFACE.REFOUNDATION.0` | Coherent CLI/chat over a qualified generic REPLAI C presentation boundary | Closed | ✅ COMPLETE | Independent producer, immutable repin, native terminal/CPU composition and repaired hosted qualification earned; [evidence](../evaluation/retained-observations.md#native-interface-composition-2026-09-30) |
| `V010.RUNTIME.DEEPSEEK.GB10.INFERENCE.PIPELINE.1` | Reduce dominant warm GB10 inference-pipeline latency | P1 | ⛔ BLOCKED | Bounded performance, Makefile and official encoding-vector evidence earned; mandatory QA retains external gaps and unchanged baseline numerical failures; [evidence](../evaluation/retained-observations.md#deepseek-gb10-inference-pipeline-2026-09-30); no follow-up selected |
| `RUNTIME.CUDA.MOE.NUMERICAL.CORRECTNESS.0` | Restore bounded DeepSeek CUDA/MoE numerical correctness | P1 | ⛔ BLOCKED | Bounded producer/component repair qualified; mandatory `cuda.native` needs the distinct unavailable bootstrap-Q2 artifact; [evidence](../evaluation/retained-observations.md#deepseek-cuda-producer-reconciliation-2026-09-29) |
| `SPECTRUM.FLAN.T5.ENCODER.DECODER.0` | A03 exact encoder-decoder execution | Next selected | ⬜ READY | Immutable target, retained encoder state, cross-attention, independent numerics and lifecycle evidence |

The old ROADMAP called A03 ACTIVE to identify the selected next boundary. READY
now distinguishes selection from execution. Its scope and selection survive;
this milestone does not acquire a model or start implementation.

### Completed interface Task Pack

`INTERFACES.CLI.REPLAI.PRODUCT.SURFACE.REFOUNDATION.0` was independently
authorized across YVEX and `mothx9/replai`. YVEX owns command grammar, typed
facts and presentation intent; REPLAI owns generic geometry, presentation and
interaction. YAI, SDK and Studio are outside this delivery. The live production
host is not a fixture. A03 remains READY and DeepSeek optimization is stopped.

The completed implementation gates are:

1. Define and implement bounded REPLAI presentation/interaction extensions,
   retaining ABI 1 records and behavior.
2. Independently qualify and publish the producer before consumption.
3. Repin the exact revision/tree/archive and extension identity.
4. Converge human CLI/chat, registry metadata and surface dispositions without
   changing domain semantics, existing JSON or private-wire meaning.
5. Qualify representative widths/modes, stale/malformed/refusal and terminal
   cleanup, reconcile architecture/contracts/evidence, then publish YVEX.

The composition is qualified, not only one renderer or dependency update.
[Dated evidence](../evaluation/retained-observations.md#native-interface-composition-2026-09-30)
binds the exact producer, frozen runtime source, 121 mapped passes, independent
producer gates, real CPU decoder, terminal restoration and machine-output controls.
Closing evidence/project-control edits are documentation-only and separately
validated. The first remote CI exposed a documentation checker using the old
root binary and flat help oracle, plus a source-acquisition assertion without
diagnostic context. The checker now binds the selected binary and hierarchical
syntax; a separately reproduced partial-before-metadata test race now waits for
one exact snapshot without relaxing the expected values. The original unlogged
CI cause is not inferred. Clean published `e5c0e550` passes hosted qualification:
118 PASS, zero FAIL/SKIP/BLOCKED/ERROR, and ten unsuppressed chat memory checks
with zero errors and no remaining allocations. These test-only corrections do
not change the qualified production inputs.
Remote mutation, Laya lifecycle convergence, new model classes and
release/distribution readiness remain excluded. No successor implementation
starts automatically; A03 remains READY.

### Completed documentation Task Pack

Completed Task: `PROJECT.DOCS.YVEX.REFOUNDATION.2` only. Shared owners are the
Documentation Protocol, current architectural explanations, project control
and publication tooling. Common validation is `make docs-check`, publication
generation, benchmark fixture/observation checks and mapped repository QA.

- [x] Section coverage reconciles every retired owner.
- [x] Native Markdown, HTML and PDF derive from the same sources.
- [x] Human routes, exact contracts and capability states remain truthful.
- [x] Mapped validation and visual review qualify the final tree.

### A03 exit contract

Select one immutable FLAN-T5 checkpoint after source archaeology. Through ordinary source → compiler programs → physical artifact/binding → one engine/session, execute a bounded bidirectional encoder whose exact retained result feeds a distinct causal decoder via source-defined cross-attention, then produce complete logits and bounded text output. Qualify independent upstream numerical agreement at encoder/cross-attention/output boundaries; exact retained-state identity, reuse, isolation, stale/refusal, cancellation/rollback, cleanup and resource evidence. Keep family meaning in source/compiler owners, not a T5 runtime. No Whisper/audio, A11/CED/Engram, YAI, Program N, Laya acceleration, general encoder-decoder breadth, behavior or release claim follows.

Read [model semantics](../architecture/model-semantics.md), [compiler](../architecture/compiler-ir.md),
[state](../architecture/computational-state.md), the [family contract](../model-families/integration.md)
and current A03 posture before execution. Evidence must bind exact source,
artifact, implementation and oracle. No adjacent spectrum or Program N work is selected.

## Blocked release progression

| ID | Task | Priority | Status | Dependency / Exit |
| --- | --- | --- | --- | --- |
| `V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` | Bounded warm DeepSeek GB10 latency optimization | P1 | ⛔ BLOCKED | Bounded material gain and numerical/lifecycle exit qualified; full mandatory gate awaits distinct bootstrap-Q2 artifact; no additional optimization selected; [evidence](../evaluation/retained-observations.md#deepseek-gb10-optimization-2026-09-30) |
| `V010.EVAL.DEEPSEEK.0` | Independent behavior evaluation | Release | ⛔ BLOCKED | Optimization boundary and repeatable evaluation basis |
| `V010.BENCH.DEEPSEEK.0` | Full-model benchmark | Release | ⛔ BLOCKED | Behavior gate and exact comparable runs; NOT MEASURED |
| `V010.RELEASE.0` | Qualify v0.1 | Release | ⛔ BLOCKED | Benchmark and every version-specific release gate |

BLOCKED denotes an unmet delivery prerequisite; it does not change capability
states. Optimization is resumed only by the explicit authorization recorded above;
behavior, full-model benchmark and release gates remain separate.

## Retained exit contracts

<details>
<summary>Exact significant completed outcomes and dependencies</summary>

### MAINTENANCE.ARCHITECTURE.REASSESSMENT.0

Qualified source-declared relations and platform-isolated client semantics; bounded performance control retained.

Depends on: Accepted integrated foundation.

### NATIVE.COGNITIVE.STATE.ALIGNMENT.0

Freeze semantic/computational ownership and refoundation-before-A01 ordering in public control.

Depends on: MAINTENANCE.ARCHITECTURE.REASSESSMENT.0.

### MAINTENANCE.ARCHITECTURE.REFOUNDATION.0

Producer publication generations invalidate borrowed values before workspace reuse; consumers reject stale results without changing family numerics or public ABI/protocol.

Depends on: NATIVE.COGNITIVE.STATE.ALIGNMENT.0.

### MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0

Mapped QA, stale-result/RNG negative tests and sanitizer lanes qualified; three warm fixed-output controls per clean tree preserve model/binding/kernel/output identity. Characterization only; upstream/full-model conformance remains open.

Depends on: MAINTENANCE.ARCHITECTURE.REFOUNDATION.0.

### MAINTENANCE.ARCHITECTURE.REFOUNDATION.1

Current Qwen forward/output, DeepSeek/DSpark target/draft schedule and MiniMax neural components use compiler-owned execution truth; runtime consumes authenticated binding v16/model-plan v8 schedule/programs. Source/catalog, importer, program, physical, runtime and backend authorities are distinct. Pure SSM remains representable without claiming A01 execution or N.

Depends on: MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0.

### MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0

Compiler refusals, migrated-family controls, transactions, CPU/CUDA and sanitizer lanes qualify the claimed cutover. Independent full-model/upstream gaps remain explicit.

Depends on: MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.

### SPECTRUM.MAMBA2.REPAIR.0

Exact source compiles all 64 layers without attention/KV/RoPE/dense FFN; portable CPU physical SSA uses common transactional state. Artifact/hosted claims remain excluded.

Depends on: MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0.

### SPECTRUM.MAMBA2.QUALIFICATION.0

Canonical source-manifest rebinding, deterministic artifact/binding admission and internal all-layer/output/session execution are qualified. Independent whole-model numerics and hosted conversation remain unclaimed.

Depends on: SPECTRUM.MAMBA2.REPAIR.0.

### PROJECT_CONTROL.POST.A01.RECONCILIATION.0

Live ownership and real V4.1 evidence select one bounded acquisition lifecycle wave; no production implementation occurred in reconciliation.

Depends on: SPECTRUM.MAMBA2.QUALIFICATION.0.

### MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0

Source-owned operation v1 and status v2 supervise immutable transfers across client/terminal loss. Deterministic lifecycle/refusal QA plus a pinned 487,753-byte real HF acquisition qualify identity-bound stop/resume/reconciliation, bounded stall detection, provider policy and TTY/log/JSON projection while unknown provider facts remain unknown and source verification remains separate.

Depends on: PROJECT_CONTROL.POST.A01.RECONCILIATION.0.

### PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0

Reconciled the established acquisition boundary with live compiler/runtime/output ownership; fixed YAI Decision Plane versus YVEX computational readout authority; selected V0 only after confirming no smaller prerequisite.

Depends on: MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0.

### DECISION.READOUT.ZERO.DECODE.0

One already-loaded Mamba2 CPU artifact produces a typed finite-candidate result from one captured common recurrent prefix. Independent score arithmetic, full-prefix replay, order/isolation, cancellation/retry, exact identities and bounded resource/latency evidence qualify zero sampling, zero generated tokens, one backbone and no calibration or semantic authority.

Depends on: PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0.

### PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0

Live breadth archaeology selected Qwen's hybrid recurrent-plus-attention state as the strongest second class, but proved that exact source/release-manifest association and current binding admission are a smaller prerequisite. It selected that one repair without starting breadth, V1/calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization.

Depends on: DECISION.READOUT.ZERO.DECODE.0.

### DECISION.READOUT.QWEN.ADMISSION.0

Exact original source and immutable YaiLabs release authority bind the unchanged 53,815,809,152-byte/851-tensor BF16 GGUF. A fresh authenticated binding v17 opens one ordinary target-only CUDA engine; exact tokenizer, compiler-owned 64-layer hybrid forward, complete 248,320-logit output and common attention-plus-recurrent prefix capture/attach qualify with direct-replay `max_abs=0`. Malformed retained v16 remains refused; no Decision Readout executed.

Depends on: PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0.

### PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0

Live genericity archaeology proved that Qwen ordinary execution is ready but Decision Readout v1 still manufactures an all-EXACT profile and an incorrect CUDA kernel-bundle identity. It selected one common execution-profile prerequisite without starting Qwen scoring, breadth, calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization.

Depends on: DECISION.READOUT.QWEN.ADMISSION.0.

### DECISION.READOUT.EXECUTION.PROFILE.0

One runtime-specialization owner now derives and re-admits generation/readout profiles from exact opened model, session, binding and backend facts. CPU Mamba retained its exact V0 oracle/order/replay/zero-generation evidence. Qwen ordinary CUDA bound build identity `3028627c3fd9220cd498998200fc61f336d4893797776bc5b5e0a104fd5215ae`, DEVICE_NATIVE execution, COMPATIBLE_DEGRADED attention/MoE and EXACT not-invoked sampling; false stronger, stale, foreign and malformed profiles refused. No Qwen candidate scoring or breadth claim occurred.

Depends on: PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0.

### PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0

Live ownership archaeology proved that prefix v2 already authenticates the immutable hybrid snapshot and session summaries already expose typed attention/recurrent resource domains, but the readout's mutable source-session identity can miss attention mutation. It selected one smaller common session-state identity prerequisite; no Qwen candidates, breadth, resource producer repair, calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization started.

Depends on: DECISION.READOUT.EXECUTION.PROFILE.0.

### DECISION.READOUT.SESSION.STATE.IDENTITY.0

Internal committed-session-state schema v1 composes exact model/binding/specialization, engine generation, authenticated session lineage and canonical target-attention, draft-attention, then recurrent committed facts under the session lifecycle lock. Real owner-level mutation/refusal controls close attention-only, recurrent-only and hybrid false-pass classes; Decision Readout delegates its before/after invariant to this owner and exact Mamba CPU V0 remains numerically unchanged. Ordinary Qwen hybrid observation reports both active domains with `decision_readout=not-invoked`; candidate/device accounting and breadth remain open.

Depends on: PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0.

### PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0

Live ownership archaeology found no remaining smaller correctness prerequisite. Existing session summaries already own non-overlapping attention and sequence resource domains; incomplete Qwen branch bytes are characterization metadata that the bounded breadth wave can repair and qualify before publication. It selected that experiment without constructing or scoring Qwen candidates or changing resource code.

Depends on: DECISION.READOUT.SESSION.STATE.IDENTITY.0.

### DECISION.READOUT.QWEN.BREADTH.0

Common schema-v1 likelihood readout executes the exact Qwen BF16 CUDA model over hybrid attention/recurrent prefix state. Independent arithmetic and replay meet `1e-12`, order/retry preserve scores, source state remains unchanged, sampling/generated counts are zero and one backbone is resident. Full-logits padding remains in normalization while candidate IDs stay tokenizer-bound. No universal family, calibration or public producer claim follows.

Depends on: PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0.

### MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0

Generation, readout and ordinary decoder qualification consume one runtime capacity owner; shared session-resource projection replaces duplicate arithmetic. Foreign busy observation does not read mutable sequence state; cleanup retains retryable ownership and refuses publication/reuse on failure. Tiny compiled execution, recurrent CPU and hybrid CUDA consumers qualified the boundary. DeepSeek decoded-forensic CPU/CUDA full-evidence execution agrees exactly over two tokens after source-order F64 dot, F64 RMS and two-pass attention reductions; bounded repeated production CUDA and generation controls are finite. Production Q8 remains a distinct numerical class, not a CPU-equivalence or long-horizon claim.

Depends on: DECISION.READOUT.SESSION.STATE.IDENTITY.0`; Qwen breadth pressure.

### PROJECT_CONTROL.POST.RUNTIME.EXECUTION.CONSOLIDATION.0

The common substrate and its remaining evidence limits were reconciled; the first non-autoregressive finite-decision model class is selected as a distinct execution pressure. Existing token-likelihood Decision Readout remains separate.

Depends on: DECISION.READOUT.QWEN.BREADTH.0`; `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0.

### SYSTEM.MODEL.LAYA.0

Immutable Laya typed-decisions revision `1a793eb568e6718f15941d08f85432581df534e3` and 842,609,220-byte F16/206-tensor source execute through a compiler-owned bidirectional physical SSA program and authenticated binding. One 29-token, three-candidate `choice` request matches independent upstream marker logits within predeclared `1e-4` absolute tolerance; an empty persistent host loads the same typed engine, executes without generation, unloads/reloads and refuses stale generations. CPU residency/latency are characterized; CUDA, text construction, action-head, calibration, YAI and low-latency usefulness remain unqualified.

Depends on: PROJECT_CONTROL.POST.RUNTIME.EXECUTION.CONSOLIDATION.0.

### PROJECT_CONTROL.POST.SYSTEM.MODEL.LAYA.RECONCILIATION.0

The exact native CPU/token-domain computation is sound at its bounded scope, but the caller must still construct Laya tokenizer/template/marker facts and the local C host producer cannot be called across a process boundary. Both are necessary parts of one consumer-safe typed producer. Selected that narrow input-plus-host seam; CUDA/latency, multilingual/head breadth, calibration, remote/YAI integration and Spectrum remain unscheduled by this decision.

Depends on: SYSTEM.MODEL.LAYA.0.

### FINITE.DECISION.PRODUCER.0

A separate UID-authenticated local process uses protocol v24 and a typed bounded question/context plus opaque candidate frontier, never token IDs, template, type ID or marker positions. The common host retains its one finite engine and generation lifecycle; the source-owned exact tokenizer/input policy builds the admitted `choice` input and seals its identity. Independent upstream construction matches all 29 control tokens and marker positions `[10,14,18]`; raw scores differ by at most `3.934e-6` under declared `1e-4`. Empty/oversized/duplicate/overlength/stale frontiers refuse before numerical publication; disconnect and reload preserve host/engine ownership. Raw logits and finite-population relative distribution remain uncalibrated. No CUDA speed, YAI ABI, semantic authority or general checkpoint breadth follows.

Depends on: PROJECT_CONTROL.POST.SYSTEM.MODEL.LAYA.RECONCILIATION.0.

### PROJECT_CONTROL.POST.FINITE.DECISION.PRODUCER.RECONCILIATION.0

The local finite-decision producer is consumer-safe at one exact CPU/checkpoint/input scope. The distinct class is recorded as A12 PARTIAL; A02–A12 were compared against the live substrate. A03 isolates the still-absent retained encoder-result and decoder cross-attention contract with less compounded modality, memory and resource pressure than A09/A11. No Laya extension, model acquisition or Spectrum execution occurred.

Depends on: FINITE.DECISION.PRODUCER.0.

</details>

## Close a Task

Reconcile its exit, evidence and owning docs in the same delivery. New independent
outcomes become Tasks; implementation steps stay inside their parent Task.
[Agent contract](../../AGENTS.md) · [Engineering method](../guides/agentic-engineering.md)
