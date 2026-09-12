# YVEX Roadmap

Status: living public project control

## At a Glance / Current Snapshot

| Axis | Current truth |
| --- | --- |
| Project target | Native, harness-independent model compilation and execution substrate. |
| Active engineering boundary | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` — typed multi-level model/program IR and consumer cutover before A01 resumes. |
| Latest generic architecture boundary | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` and its qualification COMPLETE at producer-owned device-result validity: expired borrows fail closed; source-stable QA and exact bounded performance replay retained. Earlier identity/platform repairs preserved. |
| Architecture Spectrum | A01 PARTIAL, repair queued behind refoundation .1 and qualification; A02–A10 PLANNED. |
| Adopted state architecture target | N.B1 — Slow-Update Dual-Stream is the official primary research target: primary R plus persistent Experiential Computational State E; unfinished deliberation L remains distinct. YAI owns semantic authority; YVEX owns computational realization. Program N and B1 post-training remain OPEN/unscheduled, not implemented. |
| Most important structural gap | Physical forward/output programs and selected DeepSeek stages execute through SSA, including normal CUDA residual post with caller-owned MoE operands. Complete DeepSeek/DSpark layer composition, deferred target scheduling and MiniMax neural composition still require migration before .1 closes. Qwen generation, state-capacity and output consumers use the compiled signature; bounded execution of its exact local artifact passes, but whole-model preservation remains unqualified. A01 tokenizer/normalization and whole-model barriers remain. |
| Executable foundation | DeepSeek source-to-hosted text and speculation; admitted Qwen hybrid text; bounded MiniMax composite media. Evidence depths differ. |
| External execution characterization | Capacity-compatible 12,055-token Golden still fails prefill: 11,694 tokens in 613.70 s, zero generated, then producer HTTP 504. Retained as a measured limitation, not a refoundation .1 closure gate; no downstream-safe claim. |
| v0.1 target | DeepSeek text on admitted GB10; no physical variant is yet release-qualified. |
| Behavior evaluation | BLOCKED / not ready. |
| Full-model benchmark | NOT MEASURED at release scope; repeated bounded characterization is not that benchmark. |
| Release qualification | BLOCKED. |
| Current branch | `models2`; branch epochs coordinate integration, not model ownership. |
| Next decision point | Qualify the .1 compiler/runtime cutover, then replay comparable controls before A01 repair. No IR target or before/after agreement establishes upstream conformance or release qualification. |

<!-- maturity-counts:start -->
<!-- Generated from System Maturity by tests/documentation_architecture.py. -->
| Maturity state | Meaning | Current count |
| --- | --- | ---: |
| 🟢 ESTABLISHED | Generic owner and claimed boundary implemented and qualified at the stated scope. | 31 |
| 🟡 PARTIAL | Real foundation; genericity, breadth, portability, performance or evidence incomplete. | 42 |
| 🔴 OPEN | Generic capability absent or not yet claimable. | 55 |
| ⚪ LATER | Intentionally outside the current maturity horizon. | 14 |
<!-- maturity-counts:end -->

Counts describe the rows below, **not a percentage of project completion**.
A narrow established mechanism does not make its entire program established.

Navigate: [maturity](#system-maturity) · [B1 target](#nb1-slow-update-dual-stream) ·
[model adaptation](#qwen-b1-model-adaptation-and-post-training-target) ·
[programs](#strategic-programs) ·
[spectrum](#architecture-spectrum) · [execution sequence](#current-execution-sequence) ·
[substrate progression](#general-substrate-progression) · [v0.1](#v01-release-path) ·
[nonclaims](#explicit-nonclaims-and-deferred-scope) · [promotion](#progression-and-promotion-discipline).

This file is the sole live authority for public macro state, direction,
maturity, dependencies, programs and release progression. [Architecture][system]
explains implementation; contracts constrain interfaces; [family records][families]
own family evidence; [the engineering method][method] owns delivery methodology.
Git owns chronology. The private alignment ledger retains higher-resolution
planning and unpublished evidence, not a second public roadmap.

## System Maturity

Maturity answers **what is qualified generically**, at each row's stated scope.
Temporal states (`ACTIVE`, `NEXT`, `PARTIAL`, `BLOCKED`, `NOT MEASURED`,
`COMPLETE`, `DEFERRED`) answer **what happens next**. An OPEN capability is not
necessarily an active blocked task. Target-language and design-space tables
below describe desired breadth; they are not additional maturity claims.

### Model language, compiler and physical representation

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Source provenance and immutable intake | 🟢 ESTABLISHED | Immutable revisions, inventory, retained payload and verification have common owners. | Preserve the same trust model as providers and representations grow. | C / R | [Source contract][storage]; [verification tests][source-tests] |
| Logical model identity | 🟢 ESTABLISHED | Source-declared relations separate logical model, original selector, representation, artifact, deployment, working set and engine; catalog family exceptions removed. | Extend explicit relations without family/name inference or silent ambiguity. | R / C | [Lifecycle][lifecycle]; [catalog tests][catalog-tests] |
| Semantic Model IR | 🟡 PARTIAL | Typed programs, state/effect verification, Qwen forward lowering and pure-SSM representability exist; not every executable consumer uses this authority. | Express heterogeneous R/E values, state-consuming/producing blocks, versions, multi-result computation and trainable roles without family runtimes or compulsory Transformer structure. | C | [Compilation][compilation]; [Mamba2 barrier][mamba] |
| Operator composition / graph language | 🟡 PARTIAL | Shared lowering and execution owners serve several families and component schedules. | Compile dual-stream R/E augmentation, cross-state interaction, gates/merge and update barriers through common model semantics; no family cognitive runtime. | C / D | [Family boundary][families]; [decoder plan][decoder-plan] |
| Transformation IR | 🟢 ESTABLISHED | Typed, ordered, artifact-neutral transformations precede payload materialization. | Remain the unique transformation authority as representations expand. | C / P | [Compilation][compilation] |
| Physical policy | 🟡 PARTIAL | Per-terminal dtype/qtype, layout and alignment decisions exist for admitted recipes. | Broaden compiler-owned physical decisions without downstream reconstruction. | P | [Compilation][compilation] |
| Physical Execution IR | 🟢 ESTABLISHED | Package physical records are sealed separately from deployment implementation choices. | Preserve authenticated consumption as semantic operations broaden. | C / P / R | [Artifact contract][artifacts]; [compilation][compilation] |
| Artifact emission | 🟢 ESTABLISHED | Transactional GGUF construction publishes admitted package representations. | Preserve deterministic construction and rollback across broader representations. | P | [Artifact contract][artifacts]; [writer tests][writer-tests] |
| Artifact admission | 🟢 ESTABLISHED | Integrity, roles, identities and binding constraints fail closed. | Extend coverage without weakening integrity or mandatory semantic checks. | P / R | [Artifact contract][artifacts]; [integrity tests][integrity-tests] |
| Runtime binding | 🟢 ESTABLISHED | Authenticated package truth is consumed without rebuilding compiler plans. | Preserve package meaning versus runtime specialization. | R / P | [Runtime contract][runtime-contract] |
| Existing quantized representation import | 🟡 PARTIAL | Multiple GGUF qtypes and low-precision execution paths are admitted. | Broader format coverage without per-format runtime redesign. | P | [Compilation][compilation]; [DeepSeek][deepseek] |
| Quantization synthesis | 🟡 PARTIAL | Mixed per-tensor policy and bounded calibration-informed recipes can be constructed. | Generic sensitivity/calibration-driven synthesis with reproducible decision provenance. | P | [GB10 targets][gb10]; [compilation][compilation] |
| Physical Model Compiler search | 🔴 OPEN | Deterministic lower layers exist; recipe exploration remains bounded/manual engineering. | Hardware/workload/quality-aware search and reproducible Pareto selection. | P | [GB10 targets][gb10]; [compilation][compilation] |
| Resource/state-root effect dependencies | 🔴 OPEN | Current lowering retains a global serial effect chain; explicit SSA/state foundations do not qualify independent state-root scheduling. | Order real dependencies, conflicting roots and explicit ordered semantics; preserve legal DAG branches for Target/Schedule choice. | C / S | [B1 compiler target](#compiler-execution-dag-and-admitted-backend-target); [current lowering][compilation] |

#### Semantic Model IR target language

This is the language the compiler must eventually express, not a claim that
every primitive, composition or source format below executes today.

| Semantic domain | Required representational breadth | Why it matters |
| --- | --- | --- |
| Embeddings | Token, positional, tied/untied output embedding | Dense language, encoder and output-head families |
| Normalization | RMSNorm, LayerNorm and required variants | Source-authored numerical authority |
| Dense FFN | GELU, SiLU, SwiGLU, GeGLU, gated MLP | Transformer breadth without mandatory FFN presence |
| Attention | MHA, MQA, GQA | Dense encoders and decoders |
| Attention policy | Causal, non-causal, sliding/local/global, cross-attention | Encoder state and long-context policies |
| Position | RoPE, partial/scaled RoPE, required absolute/relative forms | Position meaning sealed before execution |
| Latent attention | MLA-class projection and state | DeepSeek-class latent representations |
| MoE | Router, top-k, routed/shared experts, grouping | Sparse execution independent of one family |
| Recurrent | Typed recurrent state and transitions | RWKV and hybrids |
| SSM | Selective scan / Mamba-class state | Pure and hybrid state-space models |
| Convolution | Causal/stateful convolution | SSM, speech and hybrid models |
| Output heads | LM, embedding, pooling, classification, scoring | Execution beyond chat |
| Multimodal composition | Tower, connector/projector, fusion | Image, audio and video component graphs |
| Iterative generation | Diffusion/flow/denoising state machine | Image/video and diffusion-language systems |
| Component composition | Target/draft, encoder/decoder, tower/LM, codec/decoder | One substrate for composite models |
| Persistent state input | Typed independently retained model-state inputs | Consume computational state beyond ordinary context |
| State Read | Explicit operation consuming persistent state | No hidden state-injection convention |
| State Update | Explicit operation producing state changes | Architecture-owned update semantics |
| State-producing blocks | Blocks contributing to persistent state | Express trained state architectures |
| State-consuming blocks | Blocks reading persistent state | Layer-aware state integration |
| State gating | Learned/declared interaction between primary computation and state | No universal injection mechanism |
| Cross-state attention / interaction | Optional interactions between computational state streams | Support distinct architectural realizations |
| Persistent banks | Typed model-native banks with explicit geometry | Separate representation from lifecycle |
| Multi-timescale state roles | Different computational update/retention scales | No hard-coded semantic-memory categories |
| State realization profile | Model-specific StateProfile, distinct from the B1-v0 reference choices | Bind exact representation, interaction, update and lifecycle capability |
| State-root effects and execution DAG | Explicit roots, independent branches, multi-result state transitions and bounded barriers | Preserve legal independence without prescribing CUDA concurrency |
| Trainable architecture augmentation | Parameter roles and R/E composition meaning | Describe a post-trainable model without owning optimizer policy |

The target model signature can grow from input → model → output to
`(primary input, persistent computational state)` → model graph →
`(output, updated persistent computational state)`. This is target-language
breadth, not a claim that current models expose that interface.

#### Physical Model Compiler

**Quantization ⊂ representation synthesis ⊂ physical model compilation.**
Program P is not a GGUF picker or merely a quantizer. It should turn immutable
source plus an execution objective into a reproducible physical recipe whose
quality, resource use and performance are measured together.

The implemented deterministic lower half is verified source → semantic family
projection → Transformation IR → physical policy/variant → artifact construction
→ admission → runtime binding. Semantic breadth remains partial. The missing
strategic layer is the search/optimization loop **above physical policy**.

```text
Immutable source: Safetensors + configuration + tokenizer
    → semantic model: architecture / tensor roles / topology
        + execution objective: task/workload, quality budget, context,
          latency/throughput, concurrency
        + target machine: backend/kernel support, memory hierarchy,
          compute capabilities, runtime reserve
    → representation space
    → sensitivity analysis + calibration evidence + feasibility filtering
    → candidate recipes → bounded candidate builds
    → numerical quality + runtime latency/rate + resource measurements
    → Pareto frontier → selected recipe
    → deterministic build → final artifact (currently GGUF)
    → independent final qualification
```

This is a **target search architecture**, not an implemented automatic service.
Search may choose legal representations and admitted implementations; it may
not invent kernels, change source meaning or move backend placement authority
into family interpretation.

| Optimization dimension | Candidate decision space |
| --- | --- |
| Tensor precision | Dtype/qtype by role, layer or exact tensor |
| Group/channel geometry | Group size, channel policy, scale granularity |
| Scale representation | Encoding/precision of scales and auxiliary tensors |
| Layout | Physical tensor layout and backend-compatible ordering |
| Packing | Encoded blocks and packing strategy |
| Alignment | Physical alignment constraints |
| Sharing | Physically shared source/derived representations |
| Transformation | Admissible source-to-terminal transformation |
| Backend implementation | Only implementations compatible with the numerical representation |
| Workload reserve | Capacity left for state, context, workspace and concurrency |

Filter infeasible candidates before expensive builds or trials:

| Feasibility constraint | Required check |
| --- | --- |
| Backend implementation availability | A representation without an executable kernel is not a candidate. |
| Hardware capability | Dtype/instruction support must satisfy the admitted implementation. |
| Artifact size | Meet storage and distribution constraints. |
| Runtime model working set | Weight bytes alone do not describe runtime fit. |
| Sequence state | Reserve actual state geometry; long context may dominate. |
| Workspace / activation arena | Account for representation-dependent temporary memory. |
| Context requirement | A smaller candidate cannot sacrifice required context. |
| Concurrency requirement | Single-request fit cannot establish a concurrent envelope. |
| Quality constraint | Reject recipes exceeding the admitted degradation budget. |

There is no universal best recipe: neither the smallest artifact nor the highest
token/s wins independently. Optimize several objectives subject to hard
constraints; retain nondominated choices and their tradeoffs.

| Recipe class | Primary objective |
| --- | --- |
| Quality-oriented | Minimize degradation under memory/performance constraints |
| Balanced | Trade quality, memory and throughput within the envelope |
| Memory-oriented | Minimize working footprint above a quality floor |
| Throughput-oriented | Maximize execution rate under quality/memory constraints |

The mature output binds source identity, workload/hardware envelope,
reproducible recipe, per-tensor decisions, search/calibration evidence, artifact
and independent qualification evidence. **Search/calibration evidence != final
qualification evidence**: data used to select a recipe cannot independently
qualify that same recipe for release. [Release doctrine][doctrine] retains that
gate; no performance target here is a measured result.

### Runtime, state and scheduling

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Persistent host | 🟢 ESTABLISHED | Host transport survives zero engines and explicit load/unload. | Preserve independent host lifetime under broader resource policy. | S / R | [Runtime][runtime] |
| Multiple engine generations | 🟢 ESTABLISHED | Exact generations and multiple fitting engines coexist. | Preserve stale-reference rejection without process-global model assumptions. | S / R | [Runtime contract][runtime-contract] |
| Model leases | 🟢 ESTABLISHED | Leases prevent premature retirement; ensure-active uses typed authority. | Reuse across composition without importing semantic model selection. | S / D | [Runtime][runtime] |
| Session isolation | 🟢 ESTABLISHED | Sessions bind exact generations and own mutable sequence state. | Preserve isolation under batching and shared prefixes. | S | [Runtime contract][runtime-contract] |
| Transactional typed state | 🟢 ESTABLISHED | Distinct attention, recurrent, convolution, speculative, RNG, decoder/media representations coordinate commit/abort/reset. | Retain common lifecycle without merging unlike geometry; future E committed/working/candidate and L continuity require their own qualification. | S | [Runtime][runtime]; [state store][state-store] |
| Resource accounting | 🟢 ESTABLISHED | Mapped, prepared, allocated, addressable and observed current/peak facts remain distinct. | Preserve known/unknown resource truth under dynamic placement. | S / R | [Resource truth][runtime]; [resource tests][resource-tests] |
| Resource admission | 🟡 PARTIAL | Live capacity and backend facts constrain bounded engine admission. | Broader placement/concurrency admission with measured reserves and negative evidence. | S | [Runtime][runtime]; [resource tests][resource-tests] |
| Automatic residency policy | 🔴 OPEN | Explicit load/unload and ensure-active are mechanisms, not a generic eviction policy. | Bounded retention/placement/eviction from authoritative resource facts. | S | [Storage contract][storage] |
| Paged typed sequence state | 🟡 PARTIAL | Virtual-page budgeting and attention-oriented prefix/COW backing exist; no general cross-provider/device page manager. | Typed-provider paging, reuse and movement with transactional identity; later E/L may share mechanisms without sharing state meaning. | S | [Page store][state-pages]; [prefix backing][state-prefix] |
| Cooperative scheduling | 🟢 ESTABLISHED | Independent runnable work advances at safe quanta. | Preserve progress, cancellation and semantic isolation. | S | [Runtime scheduling][runtime] |
| Compatible physical batching | 🟡 PARTIAL | Compatible work can rendezvous into real physical rows. | Broaden compatibility and measured batching without manufactured width. | S | [Runtime scheduling][runtime] |
| Continuous / inflight batching | 🔴 OPEN | Dynamic sequence join/leave is not qualified. | Safe admission/retirement at execution boundaries with fairness and lifecycle evidence. | S | [Runtime limits][runtime] |
| Chunked prefill | 🟡 PARTIAL | Numerical chunk/width machinery includes bounded stateful paths. | Scheduler-managed prompt chunks with fairness, cancellation and resource semantics. | S | [Runtime][runtime]; [Mamba2][mamba] |
| Session prefix reuse | 🟢 ESTABLISHED | Exact retained prefix and suffix-prefill reuse execute through common state ownership. | Preserve identity and rollback under wider composition. | S | [Runtime contract][runtime-contract]; [prefix backing][state-prefix] |
| Cross-request prefix caching | 🔴 OPEN | Capture/attach and shared backing exist; no generic automatic authenticated prefix cache. | Lookup, reuse and eviction keyed by execution-compatible identity. | S | [Prefix backing][state-prefix]; [runtime][runtime] |
| Long-context runtime | 🟡 PARTIAL | Context envelopes and capacity admission exist. | Qualified chunked prefill, paged/reusable state, resource policy and context-band behavior. | S / Q | [GB10 workloads][gb10]; [runtime][runtime] |

Multiple workers != continuous batching. Multi-row kernel != continuous
batching. Large configured context != mature long-context runtime.

#### Paged typed sequence state target

A common physical page/block manager should serve attention KV, MLA/latent,
recurrent/SSM, convolution and speculative/candidate providers where paging is
appropriate. Fixed recurrent state need not grow like KV.

| Owner | Target responsibility |
| --- | --- |
| Common physical mechanism | Allocation, page ownership, reuse, eviction, movement and lifetime |
| Typed state provider | Semantic geometry, position mapping, updates and commit/abort rules |
| Session transaction | Coordinate publication or rollback across participants |

Existing virtual pages and immutable prefix sharing are foundations, not proof
of the complete cross-provider contract or an automatic caching policy.

### Native Cognitive State

**Adopted architecture target; no cognitive-state runtime is implemented.**
YAI owns semantic cognitive state; YVEX owns its computational realization.
Token context remains a supported execution strategy, but is not assumed to be
the only persistent information a future model can consume.

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Model-state capability advertisement | 🔴 OPEN | Context capacity and sequence-state classes do not advertise cognitive-state realization. | Exact model/composition advertises compatible StateProfile: representation, geometry, read/update sites and clock, modes, decode-write posture, derived materializations and lifecycle constraints. | N / C | [Adopted target](#native-cognitive-state); [current runtime contract][runtime-contract] |
| Semantic-state ingress | 🔴 OPEN | Typed content and session state are not a semantic-state lowering contract. | Mechanically validate a future public W contract, provenance, model/profile/generation compatibility and feasibility; never re-adjudicate YAI semantics. | N / X | [Adopted target](#native-cognitive-state) |
| Computational cognitive-state identity | 🟡 PARTIAL | Physical layout/content/checkpoint identities exist; none binds semantic source to a cognitive realization. | Bind semantic source, exact model, lowering/profile and update lineage sufficiently for reuse or invalidation. | N / S | [Checkpoint records][state-checkpoints]; [state store][state-store] |
| Context-only compatibility lowering | 🔴 OPEN | Ordinary context/prefill executes; prompt rendering is not provenance-bound semantic-state lowering. | Ingress → context realization has explicit provenance, limits and reproducibility. | N / C | [Adopted target](#native-cognitive-state); [runtime][runtime] |
| Persistent state compilation | 🔴 OPEN | Physical checkpoints retain execution state, not compiled semantic-state realizations. | Lower future W to E under exact model/composition and StateProfile, with authenticated provenance and rebuildability. | N / C | [Adopted target](#native-cognitive-state) |
| Incremental state compilation | 🔴 OPEN | No semantic delta-to-computational update contract. | Optional Reconcile of qualified external ΔW; refuse incompatible refresh and require full rebuild, independently from learned State Update. | N / C | [Adopted target](#native-cognitive-state) |
| Persistent State Read | 🔴 OPEN | Internal KV/recurrent sequence updates are not reads of an independently persistent cognitive bank. | Explicit E→R operation with qualified identity/effects; B1 first reference uses latent-slot cross-state read at generic primary-layer sites. | N / C | [Adopted target](#native-cognitive-state); [family boundary][families] |
| Layer-aware state injection / interaction | 🔴 OPEN | No declared cognitive-state interaction topology. | Express R↔E interaction across attention and recurrent mixers; B1 read/gate/merge and slow update remain model-specific, not a universal mechanism. | C / N | [Target language](#semantic-model-ir-target-language) |
| Multi-timescale state layout | 🔴 OPEN | No cognitive-state realization with declared update/retention timescales. | E geometry/dtype and slow update clock are independent of primary R and prompt length; no semantic labels for latent slots. | N / C | [Adopted target](#native-cognitive-state) |
| External State Update | 🔴 OPEN | No external/compiler-assisted cognitive-state update contract. | External W change may drive qualified Reconcile with provenance and transactions; this is distinct from model State Update. | N / S | [Adopted target](#native-cognitive-state) |
| Learned State Update | 🔴 OPEN | No qualified learned producer of cognitive-state deltas. | Learned model/companion state production has computational evidence; neutral writes preserve the base path and never acquire semantic admission authority. | N / C | [Adopted target](#native-cognitive-state) |
| Native State Update | 🔴 OPEN | No admitted trained architecture exposes this persistent-state update contract. | Trained R/E update produces an unpublished candidate with independent numerical/lifecycle/usefulness evidence; initial B1 decode is read-mostly. | N / C | [Adopted target](#native-cognitive-state) |
| Cognitive-state checkpoint provenance | 🟡 PARTIAL | Attention target/draft checkpoints bind model, artifact, binding and payload; recurrent sequence serialization is explicitly refused. No semantic provenance. | Bind future E checkpoint to external provenance, parent generation and model/profile compatibility; qualify L checkpoint/resume separately from E. | N / S | [State store][state-store]; [restore/rejection tests][state-tests] |
| Model replacement / state invalidation | 🟡 PARTIAL | Generation and checkpoint identity reject stale/incompatible state; semantic-source recompilation is absent. | Invalidate/rebuild computational state on incompatible model/profile changes without changing semantic authority. | N / R / S | [Runtime contract][runtime-contract]; [state store][state-store] |
| Cognitive-state locality / residency | 🟡 PARTIAL | Session-state banks/accounting are separate from weights; no independently retained cross-context cognitive-bank lifecycle. | Independently retained E generations and leases outlive requests where qualified; bind derived materializations and invalidate stale caches with resource-failure evidence. | N / S | [State residency][state-residency]; [resource truth][runtime] |
| Retention / interference / forgetting evaluation | 🔴 OPEN | No declared cognitive-state evaluation or qualified result. | Report Q0–Q4/D0 and correct/zero/wrong/swapped/stale E, read/update/write-frequency ablations, retention and stability separately from training and release claims. | N / Q | [Adopted target](#native-cognitive-state) |
| Model → YAI return-path evidence | 🔴 OPEN | No cognitive-state proposal/consequence interface to external semantic authority. | Expose operation/model/parent-state/update evidence without claiming semantic canonicalization. | N / X / Q | [Adopted target](#native-cognitive-state) |
| Experiential computational state lifecycle | 🔴 OPEN | Sequence transactions and engine/session leases do not implement independently persistent E generations. | Qualify E committed/working/candidate, binding/lease, atomic publication, invalidation and recovery independently from local state and L. | S / N | [E lifecycle target](#e-generations-derived-materializations-and-physical-lifetime) |

The four PARTIAL rows describe limited **physical foundations**, not partial
State Read/Update support. Program N as a whole is OPEN: its ingress and
realization contract do not exist. Existing S/R mechanisms remain their owners.

#### Semantic authority and computational realization

The adopted YAI-side baseline is **D/H/S Recall v2 implemented and qualified
at its published bounded scope**. This is the external architectural premise,
not a YVEX qualification result. The next YAI boundary,
`RECALL-AWARE.WORKING.STATE.0`, remains next/unselected: there is no public
Semantic Working State `W` producer contract and no public YAI↔YVEX W→E wire
format. Recall `R_t^q` below is semantic recall, distinct from model stream `R_l`.

```text
                           YAI
sources ──► D_t ┐
history ──► H_t ├──► qualified Recall v2 ──► R_t^q   IMPLEMENTED, bounded
current ──► S_t ┘                              │
                                              ▼
S + R + task + authority + Case constraints ──► W    NEXT YAI TARGET
                                              │
                         future public semantic/computational boundary
                                              │
                                              ▼
                           YVEX PROGRAM N
                         exact model + StateProfile
                                              │
                                     Lower / Reconcile
                                              │
                                              ▼
                                         E_committed
                                              │ begin
                      PRIMARY STREAM          EXPERIENCE STREAM
                           R_l                      E_working_g
                            ├──── State Read ◄─────────┤
                            ▼                          │
                          R_l+1                        │
                            └── selected slow update ─► E_working_g+1
                                                       │
                                                       ▼
                                                  E_candidate
                                                       │ prepare
                                                 commit / abort
                                                       │ commit only
                                                       ▼
                                              next E_committed

separate deliberation continuity: L_t,k = unfinished computation
W ingress, Lower/Reconcile and the R/E execution below W remain FUTURE.
```

| Owner | Authority |
| --- | --- |
| YAI | Source-grounded domain knowledge D, history H, current semantic state S, Recall, working-state selection, semantic meaning, relevance, source truth, authority, disclosure, Case continuity, assignment semantics and semantic admission. |
| YVEX | Exact model-state capability, StateProfile, computational realization and model-specific identity, compatibility, physical execution, residency, lifecycle, checkpoint and computational evidence. |
| Model | Learned interpretation, learned R↔E interaction, learned State Read and learned State Update. |

**Semantic memory != computational memory; semantic truth != computational
state; model update != semantic admission.** YVEX may return computational
consequences, state transitions and evidence; it never becomes semantic authority.
YAI does not own tensors, CUDA, residency, layers or model update equations.
E does not become YAI memory, and model-produced latent changes do not become
canonical Case facts.

Future W ingress is **mechanical validation, not semantic adjudication**.
YVEX may validate representation integrity, schema/version compatibility,
identity/digest binding, declared provenance binding, exact model compatibility,
StateProfile and state-generation compatibility, realization capability and
physical feasibility. It must not re-evaluate W's semantic validity, Participant
authority, source visibility, disclosure permission, recalled relevance, policy
currency, documentary truth or semantic admission. This creates no second
reference monitor in YVEX.

No `StateFrame` ABI, `StateDelta` ABI, W serialization, YAI endpoint, YAI-specific
runtime adapter or fixed cross-repository schema is adopted here. A real future
producer/consumer contract must use the permanent BOUNDARY workflow.
This target-doctrine refinement is not a BOUNDARY event.

#### E, L and three continuity classes

`E_t` is Experiential Computational State: reusable model-native computational
experience, potentially cross-request and longer-lived than a session, with
independent identity/generation, StateProfile binding, model-specific
representation, rebuildability, residency and checkpointing.

`L_t,k` is Latent Deliberation State: unfinished computation for an authorized
execution at internal step k. A suspended execution may require the bound E
generation, current E_working if applicable, primary/intermediate execution
state, program/iteration position, RNG state, runner state, execution
configuration and checkpoint metadata. This collection preserves deliberation,
not experiential memory. Future checkpoint/resume/invalidate/discard of L is
independent from committed E. Loss of L loses unfinished work; it must not lose
YAI semantic memory and need not lose E. Common paging/checkpoint mechanisms
do not merge L into E or introduce a second thinking runtime.

| Continuity class | Owner / survival contract |
| --- | --- |
| Semantic continuity | YAI; authoritative semantic state survives model/runtime loss. |
| Experiential computational continuity | E; reuse requires exact model/program/profile/state compatibility. |
| Deliberation continuity | L; unfinished work resumes only under an execution compatibility contract. |

Model replacement may invalidate E and L; it must not invalidate YAI semantic
continuity. No generic cross-model latent portability is claimed.

Local/sequence state comprises attention KV, gated/recurrent and convolution
state, position, RNG, decoder state, speculative candidates and ordinary
prompt/session continuation. E is separately identified, versioned and
invalidated, StateProfile-bound and potentially derived from future external W
realization. A prompt/session reset need not invalidate E. A W, StateProfile,
executable-composition or model change may invalidate E without changing YAI
semantic state. An ordinary engine/session checkpoint is not qualified L
continuity.

#### Lower, Reconcile and model State Update

| Transformation | Target meaning | Refusal / authority boundary |
| --- | --- | --- |
| `E = Lower(W, exact model/composition, StateProfile)` | Full realization of future externally selected W. | Requires an actual qualified ingress/producer contract. |
| `E' = Reconcile(E, qualified external ΔW, exact model/composition, StateProfile)` | External W change drives computational refresh. | Optional per profile; may refuse and require full rebuild. |
| `(R, E) → model computation → candidate E'` | Learned State Update caused by model computation. | Produces a computational candidate, never semantic admission. |

External Reconcile and model State Update must not collapse into one generic
update. An exact realization binds external-source provenance/digest, exact
executable composition/artifact/binding, StateProfile, compiler/lowering,
precision/layout, generation, parent/update/checkpoint lineage and compatibility
posture sufficiently to decide reuse, invalidation or rebuild. These are
lifetime/identity requirements, not a wire schema or a mandatory giant hash.

#### N.B1 Slow-Update Dual-Stream

**Official primary Program N research target: N.B1 — Slow-Update Dual-Stream.**
Program N remains OPEN. B1 is full dual-stream from the beginning: `R` is the
primary residual/activation/current-computation stream; `E` is the persistent
Experiential Computational State stream. Slow Update describes E's update
clock, not ordinary persistent KV.

```text
R_(l+1) = F_l(R_l, E_g)
E_(g+1) = G_g(E_g, R_source)

l = primary layer/computation index
g = experiential update generation inside the current execution

R_l ∈ dtype_R [B, T, d_R]
E_g ∈ dtype_E [B_E, M, d_E]
```

M is independent of current prompt length T; d_E and dtype_E may differ from
d_R and dtype_R. E need not be text, token-aligned or carry ordinary token
positions. Its precision/layout, update frequency and lifetime may differ from
R and from local sequence state. These dimensions are not public ABI.

F/G remain architecture-neutral: future qualified realizations may use
cross-state attention, latent interaction, gated low-rank projection, recurrent
or SSM transitions, associative memory, sparse routing, fast-weight/neural
memory or another qualified architecture. The first B1 model selects latent
slots, cross-state read, a learned primary gate and slow bidirectional update.
It does not freeze the generic Program N equations.

#### B1-v0 reference StateProfile and structural budget

**REFERENCE DESIGN — NOT CURRENT IMPLEMENTATION — NOT MEASURED PERFORMANCE —
NOT FROZEN ABI — NOT FINAL MODEL ARCHITECTURE.** These numbers define the first
falsifiable B1 design, not universal Program N fields or performance estimates.

| Reference profile fact | B1-v0 choice |
| --- | --- |
| First trained research vertical | Current admitted Qwen text target, with its untouched backbone retained separately. |
| Primary stack | 64 layers: 48 recurrent/gated-delta mixers and 16 full-attention layers; d_R = 5120. |
| Experiential slots / width | M = 64; d_E = 1024. |
| Cross-state interaction width | d_X = 512. |
| Canonical E dtype | BF16. |
| State Read sites | One per primary text layer: 64 sites. |
| Slow State Update sites | Eight boundaries over 64 layers, approximately one per eight layers. |
| Input / state-producing execution | State Read plus configured slow intra-model update. |
| Ordinary autoregressive decode | State Read enabled against a stable bound experiential generation/snapshot; persistent State Update disabled by default. |
| Cross-request publication | Final candidate → prepare → transactional commit only. |

For one batch/state instance, E shape `[64, 1024]` contains 65,536 elements:
`64 × 1024 × 2 = 131,072` BF16 bytes = **128 KiB**. Holding E_committed,
E_working and E_candidate simultaneously is approximately **384 KiB** of
canonical E tensor storage, excluding metadata and derived materializations.
This is structural arithmetic, not observed allocation or an allocation policy.

| Large projection at each reference State Read site | Shape | Parameters |
| --- | --- | ---: |
| W_Q_RE | [5120, 512] | 2,621,440 |
| W_K_E | [1024, 512] | 524,288 |
| W_V_E | [1024, 512] | 524,288 |
| W_O_ER | [512, 5120] | 2,621,440 |

| Reference projection budget | Derived total |
| --- | ---: |
| State Read core per site | 6,291,456 parameters |
| 64 State Read sites | 256 large tensors; 402,653,184 parameters |
| Eight symmetric State Update sites, four large projections each | 32 large tensors; 50,331,648 parameters |
| Combined projection core | 288 large tensors; 452,984,832 parameters |
| BF16 core projection storage | 905,969,664 decimal bytes; 864 MiB binary |
| Complete augmentation planning range | Approximately 455M–500M new parameters until the exact module graph freezes |

The symmetric update estimate uses E→interaction query `[1024,512]`,
primary→interaction key/value `[5120,512]` each and interaction→E output
`[512,1024]`. Totals exclude gates, state normalization, learned slot
identities/embeddings, initializer/State Encoder and small control parameters.
The complete planning range is a low-single-digit-percent augmentation of the
Qwen backbone, not a second full backbone or a measured implementation claim.

StateProfile is the broader model-specific doctrine: representation class,
geometry/dtype, initializer/State Encoder capability, read/update sites,
interaction width/head geometry, projection/gate structure, normalization,
update clock, state-producing modes, decode-write posture, commit boundary,
derived materializations, residency constraints, checkpoint compatibility,
Reconcile capability and exact augmentation/model compatibility. B1-v0 chooses
one profile; it does not prescribe a universal record containing every choice.

#### Reference State Read and neutral-path surgery

At each reference Qwen primary layer:

```text
H_l = Norm_R(R_l)
A_l = PrimaryMixer_l(H_l, ordinary sequence state)

Q_R = Project_Q_RE(H_l)
K_E = Project_K_E(E_g)
V_E = Project_V_E(E_g)
B_l = CrossStateRead(Q_R, K_E, V_E)

g_R = learned primary gate
R_intermediate = R_l + A_l + g_R ⊙ Project_R(B_l)
R_(l+1) = R_intermediate + FFN_or_MoE(Norm(R_intermediate))
```

Exact normalization/residual placement is model/composition-specific; this is
the first B1 reference architecture, not a universal Transformer law.
For surgery parity before training, initialize the primary contribution with
`g_R ≈ 0`, experiential `write ≈ 0` and `retention ≈ 1`. A disabled/neutral
path must support exact or explicitly qualified near-exact base preservation;
near-zero gates alone are not evidence of parity. Gates remain lightweight.

#### Reference Slow State Update and execution clock

At a declared update boundary:

```text
H_E = Norm_E(E_g)
H_R = selected primary update source
Q_E = Project_Q_ER(H_E)
K_R = Project_K_R(H_R)
V_R = Project_V_R(H_R)
C_g = CrossStateUpdate(Q_E, K_R, V_R)

retain_g = learned retain gate
write_g = learned write gate
proposal_g = Project_E(C_g)
E_(g+1) = StateNorm(retain_g ⊙ E_g + write_g ⊙ proposal_g)
```

This is a first research equation, not the universal update law. The parity
configuration must also preserve retention through StateNorm or bypass the
write path; a normalization that changes E cannot silently count as no update.
R advances at ordinary model frequency, E at the slower model-defined clock:

```text
layers  0–7  read E_0 → slow update E_0 → E_1
layers  8–15 read E_1 → slow update E_1 → E_2
...
layers 56–63 read E_7 → final update E_7 → E_candidate
```

This trajectory belongs to configured input/state-producing execution.
Ordinary token decode initially reads a stable snapshot; persistent E writes
on every generated token are not the default. At the end of a qualified
state-producing invocation/run, a final candidate may be offered for
transactional publication. Decode-time E mutation requires separate future
research and evidence. This execution-mode distinction is architectural.

Latent slots are computational positions, never YAI fact, history, policy,
user or long-term-memory slots. Learned slot identity, embeddings,
specialization, competition and routing may emerge; YAI retains semantic
categories and admission.

#### E generations, derived materializations and physical lifetime

| Lifecycle state | Meaning |
| --- | --- |
| E_committed | Immutable persistent generation visible at execution start. |
| E_working_g | Intra-execution trajectory, unpublished. |
| E_candidate | Final unpublished successor awaiting transaction resolution. |

```text
E_committed → begin → E_working_0 → Read / Slow Update → E_working_1 → ...
    → E_candidate → prepare ──► commit / publish → next E_committed
                           └─► abort / discard
```

Failure, cancellation or rejected work must never partially mutate committed
E. Reuse Program S's transaction philosophy; current candidate/committed
sequence transactions do not already implement E or B1 State Update.

An engine/executable model would own immutable model resources and compatible
E realization generations. Sessions/requests receive a binding/lease to an
exact committed generation. Qualified read-only bindings may share physical
backing; state-producing work creates a candidate successor rather than
mutating shared committed E in place. The realization carries identity, exact
composition and StateProfile, generation, external-source provenance,
physical representation, residency, derived materializations, checkpoint,
compatibility and invalidation state. S owns paging, sharing/COW, movement,
rollback, resource accounting and scheduling; this lifecycle may outlive a
normal request/session.

Slow-changing E permits derived State Read materializations. In B1-v0,
`K_E[layer]` and `V_E[layer]` each have shape `[64,512]`. Across 64 sites,
128 BF16 tensors require `2 × 64 × 64 × 512 × 2 = 8,388,608` bytes,
approximately **8 MiB per state instance**, excluding other storage.
**These caches are not E.** Staleness identity must bind parent E generation,
exact executable model/composition, StateProfile, read site, adapter/B1 weights,
compiler/lowering identity and physical precision/layout. E_g→E_g+1 makes
dependent materializations stale; eager, lazy or scheduled rematerialization
remain target/compiler choices, not a frozen cache policy.

#### Realization space and persistent-KV controls

Program N is not a one-dimensional ladder from KV toward cognition. Its
research dimensions are orthogonal; B1 selects one reference point.

| Dimension | Research space |
| --- | --- |
| Representation | Persistent KV baseline, latent slots, associative matrix, recurrent/SSM, low-rank, sparse/routed banks, fast-weight/neural state and future representations. |
| Interaction | Cross-state attention, gated projection, recurrent/state-space interaction, sparse routing and future qualified mechanisms. |
| Update | Read-only, end-of-run, slow block-wise, bounded decode-time, per-layer and external Reconcile. |
| Physical lifecycle | Identity/versioning, paging, sharing/COW, residency, derived caches, checkpoint, transactions, movement and schedule. |

Associative, recurrent, SSM, low-rank, sparse/routed, fast-weight/neural and
hierarchical multi-timescale experiential streams remain alternatives, not
scheduled simultaneous implementations. B1 starts with latent slots,
cross-state read and slow bidirectional update.

Persistent prefix/KV remains a useful physical foundation, training-free
standard-memory baseline, compatibility experiment and ablation/control. It
supports context-derived-state and segmented persistent/local-attention
research; it is not Program N's destination. **Current prefix reuse != E;
persistent KV != proven State Read.** Context-only provenance-bound lowering
remains OPEN despite ordinary context execution being available.

A standard/prefix realization may combine a persistent K/V bank and a local
mutable K/V bank under **one mathematically correct attention normalization**.
Two separately normalized attentions added together are not equivalent.
Unified online-softmax/LSE across segments is a future physical research path,
not B1 dual-stream: E has its own computational trajectory and update semantics.

#### Compiler, execution DAG and admitted backend target

Current [typed compiler foundations][compilation] include STATE, READ_STATE,
WRITE_STATE, SSA values, state versions, functions, regions, typed effects and
producer dependencies at their documented implemented scopes. These establish
bounded representational capacity, not B1. The current globally ordered effect
baseline and serial lowering are too conservative for full dual-stream execution.

C's future Semantic Model IR must describe primary tensor/state values,
persistent E values, state-consuming/producing operations and blocks, R→E and
E→R interactions, gating/merge, state normalization, version transitions,
multi-result computation, explicit state roots, independent dependency branches,
bounded update barriers/control structure and state-aware entrypoints.
Trainable parameter roles and augmentation semantics belong here; optimizer
policy is not required to describe a trainable model.

Program/Execution IR must distinguish effects by resource/state root, for example
`READ/WRITE(local_attention_state)`, `READ/WRITE(local_recurrent_state)`,
`READ(E_generation_g)` and `WRITE(E_working_g+1)`. Ordering is required for real
data dependencies, conflicting roots or explicit ordered semantics, not merely
because both operations are stateful.

```text
                      normalized R
                       /        \
                      ▼          ▼
                PrimaryMixer   StateRead(E)
                      │          │
                      └────┬─────┘
                           ▼
                        GateMerge
                           │
                           ▼
                        FFN / MoE
```

Execution IR must preserve producer dependencies, state-root conflicts, effect
ordering, last use, update barriers, merge synchronization and candidate-state
transitions. It must not encode CUDA stream IDs. Target/Schedule lowering may
execute PrimaryMixer and StateRead concurrently or serially before GateMerge
when semantics allow either. Legal independent DAG nodes do not require CUDA
concurrency; target evidence chooses the schedule. No speedup/latency prediction
is adopted.

Admitted CUDA execution is part of the real B1 architecture, not a late optional
optimization. Correctness-first backend primitives include state and cross-state
Q/K/V projections, State Read attention, State Update interaction, gate, merge,
normalization, state-bank addressing, candidate-state write and derived
materialization. Qualify each against independent reference operators before
fusion research such as projection+read, read+gate, gate+residual merge,
update+normalization or projection-cache materialization. Fusion may change
implementation details, never model mathematics.

Refoundation .1 establishes universal typed computational authority, explicit
state/effects, a common compiler/runtime boundary and family-independent
execution ownership. It must finish against its existing criterion; B1 does
not add operators, implementation work or closure gates to .1.

#### Generic provider and program ownership

```text
ordinary mode:
  input → ordinary admitted model execution → output

future state-capable mode:
  input + optional compatible E binding + exact StateProfile
    → state-aware execution → output + optional E_candidate
```

Ordinary mode requires neither E nor YAI. Numerical execution must not require
Case, Recall, Participant, YAI identity or semantic-authority objects and must
never branch on `if caller == YAI`. YAI is a future rich consumer of a generic
capability. No QwenMemoryRuntime, DeepSeekMemoryRuntime or YAIStateRuntime.

| Program | Future responsibility | Explicit non-ownership |
| --- | --- | --- |
| C — Model Language & Compiler | R/E semantics, state inputs/outputs, Read/Update blocks, versions, root effects, DAG/barriers, state-aware entrypoints and trainable augmentation meaning. | Semantic authority, residency and optimizer policy. |
| N — Native Cognitive State | StateProfile, E realization identity, qualified future ingress, Lower/Reconcile, compatibility, invalidation/rebuild, Read/Update realization, experiential provenance and computational evaluation contract. | Case meaning, semantic admission and physical allocation/scheduling. |
| S — Sequence Runtime | E committed/working/candidate lifetime, bindings/leases, transactions, paging/COW/sharing, residency/movement, checkpoints/rollback, resource accounting, safe schedule and future L checkpoint/resume. | Learned equations or semantic interpretation of banks. |
| D — Dynamic Composition | Augmentation/module source, exact base compatibility, trained augmentation identity, State Encoder/Adapter and LoRA/DoRA/selective-adaptation composition, attach/import/deployment identity. | Invented compatibility or a mandatory optimizer. |
| R / Source owners | Exact base source, initial augmentation weights, dataset and recipe provenance, checkpoint lineage and final trained source identity. | Training objectives as inference authority or copied source ownership. |
| P — Physical Model Compiler | B1 weights and E precision/layout, derived representations/materializations, backend compatibility and target-machine-aware admitted choices. | Semantic meaning of W/E or training policy. |
| Q — Qualification | Surgery parity, ordinary preservation, held-out post-training evaluation, causal state usefulness, interference/retention/update behavior, checkpoint, reference/CUDA equivalence, hybrid integration and cross-model controls. | Treating training/selection evidence as held-out or release proof. |

These extend existing owners, not a new strategic program or directory layering.
Model mathematics remains model-owned; N's realization contract, S's physical
lifetime and P's representation choices remain distinct.


### Generation control

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Greedy generation | 🟢 ESTABLISHED | Complete-vocabulary target selection executes in admitted text paths. | Retain greedy as one strategy, not an assumed universal runner. | G | [DeepSeek][deepseek]; [sampler tests][sampling-tests] |
| Stochastic sampling | 🟢 ESTABLISHED | Production CPU/CUDA selection and transactional RNG exist for admitted paths. | Preserve distribution/rollback contracts as policies grow. | G | [Sampler][sampling]; [sampler tests][sampling-tests] |
| Temperature/top-k/top-p/min-p | 🟢 ESTABLISHED | Core policy supports these filters and typical-p; frontends expose bounded subsets. | Preserve one policy owner and qualify new consumer projections. | G / X | [Sampling contract][sampling-api]; [sampler tests][sampling-tests] |
| Repetition/frequency/presence penalties | 🔴 OPEN | No generic penalty pipeline; unsupported compatibility parameters are rejected. | Typed ordered history-dependent transforms with rollback semantics. | G | [Compatibility limits][openai]; [sampler][sampling] |
| Seeded reproducibility | 🟡 PARTIAL | RNG identity/replay within admitted paths is not universal cross-backend/reference equivalence. | Define and qualify replay envelopes per numerical path/composition. | G / Q | [Sampler tests][sampling-tests]; [MiniMax limits][minimax] |
| Stop sequences | 🟡 PARTIAL | EOS/additional stop IDs and compatibility-layer string matching exist. | Source/tokenizer-aware generic stop authority with streaming tests. | G / C | [Runtime contract][runtime-contract]; [compatibility][openai] |
| Logit bias | 🔴 OPEN | No general typed configurable bias transform. | Validated token-domain bias under one sampling owner. | G | [Sampling contract][sampling-api]; [compatibility][openai] |
| Logprobs | 🔴 OPEN | Private selected-token probability diagnostics are not a general token/top-logprobs API. | Typed probabilities tied to exact emitted token identity. | G / O | [Sampling contract][sampling-api]; [compatibility][openai] |
| Prompt logprobs | 🔴 OPEN | No qualified prompt probability projection. | Input-token-aligned likelihoods through a scoring-capable path. | O / G | [Runtime contract][runtime-contract] |
| Multiple samples | 🔴 OPEN | Product requests do not provide independent multi-completion lifetimes. | Isolated transactional RNG/decoder states and output identities. | G / S | [Compatibility limits][openai]; [runtime][runtime] |
| Structured output | 🔴 OPEN | Prompted JSON plus post-validation is not constrained decoding. | Incremental grammar state and legal-token masking in generation/session ownership. | G | [Compatibility limits][openai]; [sampler][sampling] |
| Generic speculative execution | 🟡 PARTIAL | DSpark proves candidate state, target verification and accepted-prefix commit. | Distinct proposal strategies reuse one lifecycle without another runtime. | G / D | [Speculation decision][speculation]; [speculation tests][speculation-tests] |

Structured output targets JSON schema, grammar, regex, enum/choice,
incremental constraint state and constrained token masking. Constraint state
belongs to generation/session semantics, not an HTTP adapter.

#### Speculation breadth

Candidate/verification/commit ownership is a meaningful foundation. The target
is one contract usable by independent draft models, model-native draft/MTP,
self-speculation, EAGLE-class strategies, n-gram and suffix proposals. These are
breadth probes, **not six scheduled deliveries**. ESTABLISHED would mean
materially different strategies share the lifecycle, not every algorithm exists.

### Dynamic composition and adapters

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Verified adapter source | 🔴 OPEN | Model trust exists, not an admitted generic adapter lifecycle. | Exact adapter/B1 module provenance and immutable inventory, including State Encoder/Initializer and trained state-path weights. | D / R | [Source contract][storage]; [family integration][families] |
| Base-model compatibility | 🔴 OPEN | Model binding does not establish adapter compatibility. | Validate exact immutable base, augmentation roles, StateProfile and numerical constraints before constructing a distinct executable composition. | D | [Artifact contract][artifacts] |
| Adapter representation | 🔴 OPEN | No generic adapter transformation/physical recipe contract. | Exact source-to-physical trained augmentation identity, precision/layout and backend admission; bounded LoRA/DoRA/selective adaptation when qualified. | D / P | [Compilation][compilation] |
| Dynamic attach/detach | 🔴 OPEN | Engine generations exist; dynamic adapter composition is unqualified. | Safe changes with exact new execution identity and retirement. | D / S | [Runtime contract][runtime-contract] |
| Multiple resident adapters | 🔴 OPEN | Multiple engines do not imply multiple admitted adapters in an engine. | Coexistence, isolation, resource admission and cleanup. | D / S | [Runtime][runtime] |
| Multi-LoRA batching | ⚪ LATER | Deferred until adapter identity and base lifecycle are established. | Batch compatible compositions without identity leakage. | D / S | [Runtime batching boundary][runtime] |
| LoRA on MoE | ⚪ LATER | No generic adapter semantics to extend yet. | Sparse-role-compatible composition after the base contract. | D / C | [Family integration][families] |
| Multimodal component adapters | ⚪ LATER | Outside the present composition horizon. | Adapters on admitted component interfaces with exact lineage. | D / M | [Family integration][families] |
| Generic component graph | 🟡 PARTIAL | Target/draft and MiniMax schedules use shared owners. | Reusable architecture-augmented R/E composition and encoder/decoder/tower/codec graphs beyond bounded current verticals; current component schedules do not implement B1. | D / C | [Family integration][families]; [MiniMax][minimax] |
| Architecture-augmented model composition | 🔴 OPEN | Current component composition does not admit B1 graph surgery or a trained R/E augmentation. | Immutable base plus augmentation architecture/weights and StateProfile receives its own compatible source, artifact, deployment and execution identities. | D / C | [Qwen adaptation target](#qwen-b1-model-adaptation-and-post-training-target) |
| Post-training provenance / intake | 🔴 OPEN | Source trust does not implement generic dataset/recipe/checkpoint-lineage intake for externally post-trained compositions. | Bind trainable manifest, dataset/recipe/configuration, checkpoint lineage and trained source before import and independent qualification. | R / D / Q | [Provenance target](#provenance-and-inference-composition-pipeline); [Source contract][storage] |

Target adapter lifecycle: source → verified adapter → base compatibility →
admitted transformation → physical representation → deployment specialization
→ exact execution identity. **Base + adapter must not silently retain the same
engine identity.** This is a future contract, not present dynamic support.

#### Qwen B1 model adaptation and post-training target

Qwen is the first **trained B1 research vertical**, not a newly selected wave.
The [current family record][families] identifies `Qwen/Qwen3.8-27B`, interpreted
by the existing `qwen3_5` owners: 64 text layers, 48 recurrent/gated-delta mixers
with convolution state and 16 full-attention layers, primary width 5120.
B1 attaches through a generic primary-layer interaction boundary:

```text
attention layer:   R → original full attention / local sequence state
                  R → State Read(E)

gated-delta layer: R → original recurrent / convolution computation
                  R → State Read(E)
```

Both mixer classes use the same B1 abstraction and physical lifecycle. This
hybrid reference would test directly that E is not another KV cache; no
separate experiential runtime is permitted for either mixer class.

The untouched Qwen checkpoint remains an immutable, separately executable
reference. B1 is a distinct exact composition: **immutable backbone + B1
architecture/composition + B1 trained weights + StateProfile**, with its own
source/composition identity, training provenance, artifact, deployment,
execution identity and qualification. The original model is not retroactively
renamed or converted into B1.

#### Mandatory model controls and adaptation ladder

These are future research controls, not current execution capabilities,
scheduled waves or expected outcomes. Q4 is conditional, not automatic.

| Control | Architecture / weights / state | Question isolated |
| --- | --- | --- |
| Q0 — Original Qwen | Original architecture and weights, ordinary execution, no E. | Base reference. |
| Q1 — Standard-memory Qwen | Original architecture/weights; equivalent relevant information in ordinary context and/or qualified retained prefix/KV. No learned B1 path. | Same-backbone standard-memory baseline. |
| Q2 — B1 Surgery Parity | Full B1 graph and E present; new weights initialized, primary read gate neutral/near-zero and update write neutral/near-zero; no post-training. | Q0 vs Q2: does surgery preserve the original path? |
| Q3 — B1 Post-Trained / Frozen Backbone | Backbone frozen; train State Encoder/Initializer, State Read, State Update, gates, experiential normalization and B1-specific parameters. | Q1 vs Q3: does learned E improve over standard memory on the same backbone? First actual trained B1 model. |
| Q4 — B1 + Selective Backbone Adaptation | Only if Q3 warrants bounded LoRA, DoRA or selective unfreezing alongside B1. | Q3 vs Q4: does backbone co-adaptation materially help? |
| D0 — DeepSeek Standard Control | Original DeepSeek architecture/weights, ordinary YVEX execution, no B1; equivalent information through ordinary rendered context and/or qualified retained prefix/context. | Q1 vs D0: what does scale buy under the standard paradigm? Q3/Q4 vs D0: can persistent computational state recover capability otherwise requiring more scale/context? |

Potential Q4 sites include selected normalizations, mixer output projections,
attention projections, FFN/down/output interaction points and later block
groups. Selection remains evidence-driven; the roadmap does not freeze sites.

| Stage | Trainable scope | Required research question / boundary |
| --- | --- | --- |
| PT0 — Surgery parity | No learning required. | Disabled/neutral B1 contribution preserves the backbone path, with exact or declared near-exact numerical criteria. |
| PT1 — B1-only post-training | Frozen backbone; B1 augmentation trainable. | Demonstrate causal use of E. |
| PT2 — Selective backbone adaptation | B1 plus limited backbone plasticity. | Test whether pretrained representations benefit from co-adaptation to R↔E. |
| PT3 — Broader architecture-aware post-training | Future broader adaptation only if earlier stages show a clear ceiling. | Not scheduled merely because listed. |

Native training around R/E from the beginning is a deeper research horizon,
not a B1 prerequisite.

The [live DeepSeek/DSpark record][deepseek] owns D0 structural truth: 43-layer
SWA/CSA/HCA hybrid target, width 4096, mHC with four 4096-wide streams and a
16,384-wide native residual boundary, 256 routed experts with top-6 selection,
one shared expert per target layer and the admitted DSpark target/draft
composition. D0 asks how much a substantially larger unmodified model can do
with equivalent information delivered conventionally. It does not test whether
DeepSeek could benefit from B1 training.

**Do not insert random untrained B1 weights into DeepSeek and call it D0.**
That confounds surgery damage with lack of training. DeepSeek is not the first
post-trained B1 target. A future DeepSeek-B1 variant is unscheduled and may be
considered only after causal Qwen evidence warrants extension; it would need
its own neutral-path surgery parity before training. No DeepSeek B1 tensor
budget or training campaign is adopted.

#### Provenance and inference composition pipeline

```text
immutable base model source
  + architecture augmentation definition
  + StateProfile
  + trainable-parameter manifest
  + training dataset identity/provenance
  + training recipe identity
  + optimizer/training configuration identity where required
  + checkpoint lineage
    → trained augmentation / adapted weights
    → exact base compatibility
    → exact executable composition identity
    → YVEX import / compilation / physical representation
    → runtime binding
    → independent qualification
```

This strengthens the existing D/C/R/P/Q lane without promoting generic adapter
or external post-training integration to implemented. Training provenance
authenticates where weights came from; it does not replace inference admission
or independent qualification.

An initial external differentiable research trainer may own autograd,
backpropagation, optimizer, gradient accumulation, schedule and training
checkpoint production with exact input/output provenance. YVEX imports the
trained composition as immutable source truth. A YVEX-native trainer is neither
implemented nor required: backward IR, optimizer execution, training scheduling,
gradient state and distributed training remain possible future research.
C may declare trainable roles and computational meaning without owning optimizer
policy; D composes compatible modules, Source retains provenance, P realizes
weights and Q independently qualifies the result.

#### Cross-execution episodes and objective families

B1 training/evaluation must exercise genuinely cross-execution state, for example:

```text
run 1: information introduced
run 2: additional state
run 3: contradiction / supersession
run 4: delayed dependency
run 5: irrelevant distractor
run 6: changed condition
final task: selective retained experience required
```

Candidate task classes include cross-request entity binding, supersession,
long-gap dependency, technical project continuity, contradiction tracking,
persistent preferences with distractors, procedural progress and delayed
information composition. These are research episodes, not YVEX product or
slot semantics. Equivalent-information context/prefix controls must remain
visible, alongside held-out evaluation distinct from training and selection.

Possible objective families combine ordinary language/task learning,
teacher-logit or hidden-state distillation, state usefulness, wrong-state
contrast, retention/stability regularization, write-magnitude regularization
and base-model behavior preservation. No coefficients or final recipe are
frozen. Success is not maximum write magnitude/frequency: useful E may retain
most of its state and change only a small fraction.


### Output runners

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Generate | 🟢 ESTABLISHED | Shared autoregressive generation executes admitted text models. | Preserve runner semantics independently of chat/frontend grammar. | O / G | [Runtime][runtime]; [DeepSeek][deepseek] |
| Embed | 🔴 OPEN | Internal token embeddings/conditioning are not an embedding-output runner. | First-class vector result, identity and I/O admission. | O | [Runtime contract][runtime-contract]; [family integration][families] |
| Pool | 🔴 OPEN | No generic pooling result owner. | Source-authored pooling semantics and typed output. | O / C | [Family integration][families] |
| Score | 🔴 OPEN | No general sequence/pair scoring runner. | Exact score definition, token alignment and result identity. | O | [Runtime contract][runtime-contract] |
| Classify | 🔴 OPEN | No admitted generic classification-head runner. | Label/head semantics and typed result independent of commands. | O / C | [Family integration][families] |
| Rerank | 🔴 OPEN | No generic pair/cross-encoder request and score path. | Pair construction, score semantics and reproducible ranking evidence. | O | [Runtime contract][runtime-contract] |
| Reward | 🔴 OPEN | No general sequence/token reward output. | Source-defined reward heads and token/sequence identity. | O / C | [Family integration][families] |
| Iterative media generation | 🟡 PARTIAL | Generic latent lifecycle supports bounded MiniMax iteration/publication. | Reusable non-autoregressive runner across admitted schedules and outputs. | O / M | [Latent runner][latent]; [MiniMax limits][minimax] |

Execution runner semantics != frontend command. A vector or score must be a
typed runtime result, not text parsed by an embedding/scoring CLI wrapper.

### Multimodal execution

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Typed text/image/audio/video/file/tensor content | 🟢 ESTABLISHED | Ordered parts, provenance and directional capability admission have common owners. | Preserve typed transport independently of executable modality coverage. | M / X | [Runtime contract][runtime-contract] |
| Image preprocessing | 🟡 PARTIAL | Image decode/resize and bounded keyframe preparation exist. | Source-bound normalization, resolution and format breadth across inputs. | M | [Image owner][image]; [MiniMax][minimax] |
| Vision tower execution | 🟡 PARTIAL | MiniMax's admitted vision component executes; Qwen text evidence does not qualify vision. | Reusable towers with independent component and end-to-end evidence. | M / C | [Family boundaries][families]; [MiniMax][minimax] |
| Projector / connector | 🟡 PARTIAL | Composite conditioning connects admitted components. | Typed component shape/identity contracts beyond one composition. | M / D | [MiniMax][minimax]; [family integration][families] |
| Vision feature caching | 🔴 OPEN | No generic authenticated feature cache. | Reuse outputs by exact source/preprocess/component identity. | M / S | [Runtime][runtime] |
| Audio preprocessing | 🔴 OPEN | Audio publication is not a generic input decode/resample/features pipeline. | Typed source-bound audio ingest and feature preparation. | M | [Media publication][media-io]; [family boundaries][families] |
| Audio encoder | 🔴 OPEN | Inventoried codec roles and an audio decoder do not qualify a generic speech-input encoder. | Admitted input encoder and independent feature-to-model evidence. | M / C | [MiniMax evidence][minimax] |
| Streaming audio state | 🔴 OPEN | No general streaming input/output state contract is qualified. | Timing, partial windows, continuity, cancellation and publication. | M / S | [Runtime contract][runtime-contract] |
| Video frame preparation | 🔴 OPEN | Keyframe image conditioning is not general video ingest. | Demux/sampling/timing and exact per-frame identity. | M | [Image owner][image]; [MiniMax][minimax] |
| Video tower / temporal execution | 🟡 PARTIAL | Bounded latent temporal execution/decoding exist, not a general input video tower. | Reusable temporal semantics and independently qualified composition. | M / C | [MiniMax][minimax] |
| Image-valued output | 🔴 OPEN | Image/frame helpers do not establish a still-image generation runner. | Typed image output, resolution/seed identity and publication lifecycle. | M / O | [Runtime contract][runtime-contract]; [media publication][media-io] |
| Audio-valued output | 🟡 PARTIAL | Bounded decoded audio and synchronized media publication exist. | General audio output with full-scale numerical/behavior evidence. | M / O | [MiniMax][minimax]; [media runtime][media-runtime] |
| Video-valued output | 🟡 PARTIAL | Bounded composite video execution and publication exist. | General output/timing contracts and full-scale correctness evidence. | M / O | [MiniMax][minimax]; [media runtime][media-runtime] |
| Iterative diffusion/flow execution | 🟡 PARTIAL | Latent iteration consumes an admitted MiniMax schedule. | General denoising/flow schedules and qualified state/output semantics. | M / O / C | [Latent runner][latent]; [MiniMax][minimax] |

Typed image content != generic image model execution. MiniMax's bounded media
path is separate from A10 still-image pressure; it does not close A10 or
establish full-scale output quality.

### Tokenizer and conversation

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| BPE | 🟢 ESTABLISHED | Admitted compiled BPE policies execute through common ownership. | Preserve source-bound behavior as supported policies expand. | C | [Tokenizer execution][tokenizer]; [tokenizer tests][tokenizer-tests] |
| Byte-level BPE | 🟢 ESTABLISHED | Production byte mapping and BPE execute for admitted policies. | Preserve byte/Unicode boundaries and independent roundtrip evidence. | C | [Tokenizer execution][tokenizer]; [tokenizer tests][tokenizer-tests] |
| SentencePiece-class | 🟡 PARTIAL | Vocabulary/config recognition exists; arbitrary SentencePiece/Unigram execution is unqualified. | Executable source-authored processor and independent reference agreement. | C | [Tokenizer admission][tokenizer-core]; [Mamba2][mamba] |
| HF tokenizer JSON | 🟡 PARTIAL | Exact assets feed supported family compilation; metadata recognition is not arbitrary pipeline execution. | Explicit supported pipeline semantics and rejection of unsupported stages. | C | [Tokenizer compilation][tokenizer-compile]; [Mamba2][mamba] |
| Added tokens | 🟡 PARTIAL | Compiled policies recognize added/special tokens on admitted paths. | Broader source-authored matching/normalization flags without frontend heuristics. | C | [Tokenizer execution][tokenizer]; [tokenizer tests][tokenizer-tests] |
| Byte fallback | 🟡 PARTIAL | Byte handling/admitted fallback policy exist; ByteLevel is not every tokenizer's fallback rule. | Source-specific fallback and Unicode roundtrip conformance. | C | [Tokenizer execution][tokenizer]; [tokenizer tests][tokenizer-tests] |
| BOS/EOS variants | 🟡 PARTIAL | Admitted sources supply policy; Mamba2 still has special-token authority conflicts. | Resolve source assets consistently before artifact admission. | C / G | [Tokenizer compilation][tokenizer-compile]; [Mamba2][mamba] |
| Multiple EOS | 🟡 PARTIAL | Primary EOS and additional stop-ID machinery exist. | Generic source-authored EOS sets across consumer projections. | C / G | [Runtime contract][runtime-contract]; [tokenizer compilation][tokenizer-compile] |
| Chat/conversation template | 🟡 PARTIAL | Admitted families compile bounded conversation grammars. | Broader versioned/source-bound templates; no invented template for base models. | C / X | [Family integration][families]; [tokenizer compilation][tokenizer-compile] |
| Reasoning/control tokens | 🟡 PARTIAL | Admitted control channels and incremental decoding exist. | Source-authored control policy without frontend name inference. | C / G | [DeepSeek][deepseek]; [tokenizer compilation][tokenizer-compile] |
| Multimodal placeholders | 🟡 PARTIAL | MiniMax framing binds bounded conditioning to components. | Typed content/component relationships under exact tokenizer identity. | C / M | [MiniMax][minimax]; [runtime contract][runtime-contract] |
| Tool/control syntax | 🟡 PARTIAL | Bounded source grammars and tool-call projection exist, not tool execution. | Source-bound syntax without application semantics entering runtime. | C / X | [Compatibility][openai]; [tokenizer compilation][tokenizer-compile] |

Tokenizer, configuration and conversation semantics belong to authenticated
source/compiled identity, not unversioned frontend heuristics. Recognizing a
tokenizer kind is explicitly weaker than executing its processor.

### Interfaces and portability

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Native CLI | 🟢 ESTABLISHED | Product operations consume typed owners. | Preserve thin parsing/rendering boundaries as operations grow. | X | [Command architecture][commands] |
| REPLAI-backed interaction | 🟢 ESTABLISHED | External editing; YVEX retains command/session/cancellation meaning. | Preserve the boundary and qualify dependency changes. | X / R | [Editor ADR][editor]; [terminal tests][terminal-tests] |
| Local typed protocol | 🟢 ESTABLISHED | Versioned private contract routes exact engine/session facts. | Version only on explicit wire-contract changes. | X | [Local protocol][protocol] |
| OpenAI-compatible projection | 🟡 PARTIAL | Bounded local compatibility; exact-tokenizer preflight and distinct byte/input/output/sequence limits are exposed for text engines. | Qualify additional real consumers; preflight is not resource reservation or whole-workload qualification. | X | [Compatibility matrix][openai] |
| Generic external harness consumption | 🟡 PARTIAL | Typed facts serve current native and compatibility consumers. | Independently qualify additional real consumers without frontend leakage. | X | [C API][c-api]; [runtime contract][runtime-contract] |
| SDK-ready client boundary | 🟡 PARTIAL | Consumable C interfaces exist; broad SDK/platform lifecycle evidence is incomplete. | Supported packaging, error/cancel semantics and consumer tests. | X | [C API][c-api] |
| Linux terminal execution | 🟢 ESTABLISHED | Private POSIX adapter and REPLAI have real PTY/lifetime evidence. | Preserve submission, interrupt, restoration and cleanup. | X | [Editor ADR][editor]; [terminal tests][terminal-tests] |
| macOS terminal execution | 🔴 OPEN | Portable/POSIX structure is not executed macOS qualification. | Real platform build and interaction/lifecycle qualification. | X | [Platform boundary][system]; [editor ADR][editor] |
| Windows Console/ConPTY execution | 🔴 OPEN | No qualified Windows product terminal backend. | Platform mechanics beneath the same semantic contract, independently tested. | X | [Platform boundary][system] |
| Remote production transport | 🔴 OPEN | Local/loopback is not production remote serving. | Authenticated operational transport and deployment evidence. | X | [Compatibility scope][openai]; [local protocol][protocol] |
| Authentication | 🔴 OPEN | No qualified remote request-identity/security owner. | Authentication/authorization contract if remote scope is admitted. | X | [Compatibility scope][openai] |
| TLS | 🔴 OPEN | No qualified product transport-security boundary. | Secure lifecycle and negative evidence if remote scope is admitted. | X | [Compatibility scope][openai] |
| Multi-tenant isolation | ⚪ LATER | Session isolation is not a tenant/security boundary. | Tenant identity, resource/security isolation and operational evidence. | X / S | [Runtime contract][runtime-contract]; [release scope][release] |

Portable architecture means product semantics → generic interaction contract →
platform adapter. Linux/POSIX and macOS may implement mechanics with TTY/PTY,
termios and signals; Windows may use Console/ConPTY and console-control events.
These are **platform mechanisms**, not generic semantic types. Only Linux is
execution-qualified. Editing remains REPLAI-owned; YVEX owns submission
meaning, generation cancellation and semantic rendering.

### Qualification

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Structural/unit/integration QA | 🟢 ESTABLISHED | Canonical catalog/change mapping qualify implementation contracts. | Keep current-delta, source-stable required lanes green. | Q / R | [QA authority][qa]; [registry][qa-registry] |
| Sanitizer/memory qualification | 🟢 ESTABLISHED | Mapped sanitizer/lifetime lanes exercise applicable owners. | Preserve ASan/LSan/UBSan and rollback/cleanup evidence. | Q | [QA authority][qa] |
| Evidence lineage | 🟢 ESTABLISHED | Identity-bound reports distinguish implementation, numerical, runtime and release evidence. | Preserve lineage and reject stale/missing evidence. | Q | [Evidence contract][events]; [engineering method][method] |
| Measurement plane | 🟢 ESTABLISHED | Typed durations, rates and resource facts observe real execution. | Preserve measured/derived/unknown distinctions for new runners. | Q | [Measurement contract][events] |
| Independent whole-model conformance | 🟡 PARTIAL | Component references and hosted execution do not establish every full numerical path. | Independent exact-source tokenizer-to-output references at claimed scale. | Q / C | [Reference authority][reference]; [family evidence][families] |
| Current full-model performance baseline | 🟡 PARTIAL | Reassessment retained repeated fixed-workload DeepSeek control; not a release-wide or automatically current-tree benchmark. | Replay exact identities on the selected clean execution tree with dispersion/resources. | Q | [GB10 control authority][gb10] |
| Long-context benchmark | 🔴 OPEN | Capacity and target budgets do not establish context-band results. | Reproducible long-context latency, memory, correctness and reliability. | Q / S | [GB10 targets][gb10] |
| Concurrency/batching benchmark | 🔴 OPEN | Multi-session/row tests do not establish serving throughput at scale. | Real physical concurrency, fairness and resource measurements. | Q / S | [Runtime][runtime]; [GB10 targets][gb10] |
| Behavior evaluation | 🔴 OPEN | Full-model quality/tokenizer/refusal/long-context gate remains open. | Declared corpus/scorer and repeatable hosted-path evaluation. | Q | [Release doctrine][doctrine] |
| Full-model release benchmark | 🔴 OPEN | Bounded characterization does not close release performance evidence. | Latency, throughput, memory and reliability after evaluation. | Q | [Release doctrine][doctrine]; [v0.1][release] |
| Release qualification | 🔴 OPEN | Required version-specific gates have not closed together. | Every mandatory gate, claim audit and exact release identity. | Q | [Release doctrine][doctrine]; [v0.1][release] |
| Reproducible artifact release | 🟡 PARTIAL | Source/build/publication and verified rehydration exist; no qualified final v0.1 variant. | Reproducible selected recipe/artifact and independent final qualification. | P / Q | [Model release contract][model-release]; [lifecycle][lifecycle] |
| Remote operational qualification | 🔴 OPEN | Local operation supplies no production remote evidence. | Security/reliability/operator qualification if remote scope is admitted. | Q / X | [Release nonclaims][release] |

Software QA, including bounded numerical tests, is not a declaration that the
official DeepSeek full-model test vectors have been run and passed. Every
conformance claim must identify the independent reference, representation,
scope and actual result; missing required evidence stays BLOCKED, not green.

The evidence ladder is source → architecture → component numerics →
artifact/deployment → runtime lifecycle → whole-model execution → behavior
evaluation → benchmark → release qualification. [Its canonical figure][method]
explains the boundaries; no lower stage substitutes for a higher one.

Qualification keeps these authorities distinct:

| Evidence class | What it can establish |
| --- | --- |
| Internal software contract | Implementation behavior, including negative paths. |
| Internal numerical oracle | Agreement with the declared internal reference at its scope. |
| Upstream / authoritative reference conformance | Agreement with an independently identified authoritative implementation/vector; internal YVEX agreement is insufficient. |
| Runtime / transactional qualification | Isolation, commit/abort/reset, cancellation and resource cleanup. |
| Behavior evaluation | Declared model-quality or computational-state usefulness outcomes. |
| Performance qualification | Repeated identity-bound measurements and comparable distributions. |
| Release qualification | All version-specific independent gates close together. |

#### B1 qualification controls

Mechanism existence is not B1 success. The [Q0/Q1/Q2/Q3/Q4/D0 matrix](#mandatory-model-controls-and-adaptation-ladder)
must remain individually reported; Q4 is conditional on earlier evidence.
Training metrics, held-out behavior, numerical conformance, runtime lifecycle,
performance and release evidence remain separate. No outcome is predicted.

| Future qualification boundary | Mandatory controls / observations |
| --- | --- |
| Ordinary-mode and surgery preservation | Ordinary execution without E; Q0/Q2 exact or declared near-exact parity; state-disabled behavior and base-model regression. |
| Causal State Read usefulness | Correct E vs zero E, wrong E and swapped E; State Read disabled. Show whether E carries useful information and whether the model actually depends on it. |
| Freshness, retention and update | Fresh vs stale E; cross-request retention; State Update disabled; write-frequency ablation; stability, intended forgetting and measured update behavior. |
| Transaction and deliberation boundaries | Candidate/prepare/commit/abort transitions, cancellation, unchanged committed generation on failure; checkpoint/recovery, with future L continuity qualified independently. |
| Compatibility and materialization | StateProfile mismatch refusal, model/composition replacement, E invalidation/rebuild and derived-cache staleness/rematerialization. |
| Numerical and hybrid integration | Independent reference vs CUDA operators and composed execution; Qwen's recurrent/convolution and full-attention layer paths share the B1 boundary. |
| Comparative behavior | Q1/Q3 same-backbone standard-memory comparison, conditional Q3/Q4 co-adaptation, Q1/D0 scale control and Q3/Q4 vs untouched D0 with equivalent relevant information. |
| Performance and resources | Once implemented: repeated throughput/latency distributions, memory/residency, preparation/rematerialization costs, update frequency and numerical effect on exact source/composition/profile/device/workload identities. |

Correct, zero, wrong, swapped and stale E, State Read disabled and State Update
disabled are mandatory explicit state-level ablations, never hidden in an
aggregate score. Report expected/observed tokens, logits/state values or error
metrics where applicable, reference identity, tolerances, worst cases and
dispersion. Internal agreement does not establish upstream conformance;
training-set improvement does not establish held-out state usefulness.


### Scale-out

| Capability | State | Current YVEX truth | Boundary required for promotion | Program | Evidence / owner |
| --- | :---: | --- | --- | --- | --- |
| Single-node execution | 🟢 ESTABLISHED | Current admitted CPU/CUDA execution is single-node. | Preserve engine/session/resource truth as local workloads broaden. | S | [Runtime][runtime] |
| Tensor parallelism | ⚪ LATER | Not on the immediate single-node path. | Distributed tensor ownership, collectives and numerical qualification. | F | [Runtime boundary][runtime] |
| Pipeline parallelism | ⚪ LATER | Deferred. | Stage placement, activation transfer and failure-safe progress. | F | [Runtime boundary][runtime] |
| Expert parallelism | ⚪ LATER | Local MoE worklists are not distributed expert ownership. | Distributed routing, capacity and collective correctness. | F | [Family integration][families] |
| Data parallel serving | ⚪ LATER | Multiple local engines are not distributed serving. | Replica identity, routing and operational qualification. | F | [Runtime boundary][runtime] |
| Context/sequence parallelism | ⚪ LATER | Deferred beyond current state/runtime maturity. | Distributed sequence geometry and numerical dependencies. | F | [Runtime contract][runtime-contract] |
| Distributed sequence state | ⚪ LATER | State ownership is local today. | Cross-node lifetime, movement and authenticated recovery. | F / S | [Runtime contract][runtime-contract] |
| Distributed engine identity | ⚪ LATER | Generation identity is not distributed membership. | Exact deployment/generation membership and stale-reference rules. | F / R | [Runtime contract][runtime-contract] |
| Distributed failure recovery | ⚪ LATER | Local abort/cleanup is not distributed recovery. | Failure domains, replay/commit rules and recovery evidence. | F | [Runtime contract][runtime-contract] |
| Disaggregated prefill/decode | ⚪ LATER | Advanced later horizon. | Transferable admitted state and split-stage identity/resource qualification. | F / S | [Runtime boundary][runtime] |
| Disaggregated encode/media stages | ⚪ LATER | Advanced later horizon. | Distributed component results, timing, ownership and recovery. | F / M | [Family integration][families] |

## Strategic Programs

Programs are long-lived horizontal owners of maturity gaps, not branches,
release versions or automatically authorized task queues. PARTIAL means real
foundations with open boundaries; a completed wave does not complete a program.

| Program | Name | Purpose | Typical maturity rows | State |
| --- | --- | --- | --- | :---: |
| R | Architecture Integrity & Refoundation | Remove duplicate ownership, reconstruction and structural overhead. | Identity; compiler/runtime/backend/client boundaries | 🟡 PARTIAL |
| C | Model Language & Compiler | Express heterogeneous topology, operations and source semantics. | Semantic IR; graph composition; tokenizer | 🟡 PARTIAL |
| P | Physical Model Compiler | Synthesize/search physical representations from immutable source. | Policy; recipes; quantization; artifact construction | 🟡 PARTIAL |
| S | Sequence Runtime | Mature state, scheduling, paging, reuse and resources. | Typed state; inflight batching; long context | 🟡 PARTIAL |
| N | Native Cognitive State | Realize persistent model-native E independently of token context; N.B1 is the primary research target. | StateProfile; future W ingress/Lower/Reconcile; E identity; Read/Update; evaluation | 🔴 OPEN |
| G | Generation Control | Generalize sampling, constraints and speculation. | Samplers; logprobs; grammar; proposals | 🟡 PARTIAL |
| D | Dynamic Composition | Admit adapters, architecture augmentation and exact trained model compositions. | Base compatibility; adapters; augmented composition; post-training intake | 🟡 PARTIAL |
| O | Output Runners | Separate model execution from chat generation. | Generate; embed; pool; score; reward | 🟡 PARTIAL |
| M | Multimodal Execution | Generalize image/audio/video execution and publication. | Preprocessing; towers; connectors; iterative output | 🟡 PARTIAL |
| X | External Interfaces | Preserve harness neutrality and portable interaction. | CLI; client API; REPLAI; platform evidence | 🟡 PARTIAL |
| Q | Qualification | Establish independent behavior, performance and release evidence. | QA; conformance; evaluation; benchmarks; release | 🟡 PARTIAL |
| F | Scale-out | Distribute/disaggregate after single-node maturity. | TP/PP/EP/DP; distributed state; split stages | ⚪ LATER |

### R — Architecture Integrity & Refoundation

**Purpose:** unique owners for facts, identities and lifetimes.

**Established foundation:** source-declared model relations, private terminal
platform ownership and producer-owned device-result publication lifetimes.
`MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` and its qualification are COMPLETE at
that borrow boundary: workspace reuse expires prior values without another
allocation/registry or persisted identity. The [runtime contract][runtime-contract]
retains serialized use and producer-lifetime limits; this is not a concurrent lease.

**Open maturity boundaries:** Transformer-shaped composition and structural overhead requiring measurement.

**Current pressure:** refoundation .1 replaces decoder-shaped computational authority with a typed multi-level IR and migrates real consumers. It also separates exact model/checkpoint identity from architecture import and executable program ownership; historical `families` containers are not permanent vertical runtimes. A01 and dual-stream C/S/N targets challenge the language without authorizing N implementation. The completed .0 borrow repair remains complete.

**Material advance:** remove a demonstrated ownership defect with negative/lifecycle QA and exact performance replay where relevant.

**Future source/provenance responsibility:** retain exact base and initial augmentation sources, dataset/recipe identities, checkpoint lineage and final trained source through existing Source owners; this does not implement post-training intake.

**Does not own:** family mathematics, imagined consumer policy or release promotion.

### C — Model Language & Compiler

**Purpose:** seal model meaning before runtime execution.

**Established foundation:** family interpretation, tensor roles, Transformation IR and authenticated package plans.

**Open maturity boundaries:** SSM-only topology, normalization/tokenizer authority and broader operators/state/output heads. Future R/E semantics, read/update blocks, StateProfile-compatible entrypoints, root effects, dependency DAG/barriers and trainable augmentation roles extend the existing target language; they are not added to refoundation .1.

**Current pressure:** A01's source-only barrier; later spectrum families test different assumptions.

**Material advance:** another computational shape compiles through shared mechanisms without backend reconstruction.

**Does not own:** allocation/residency lifetimes, semantic-state authority, serving policy or application task selection.

### P — Physical Model Compiler

**Purpose:** source plus workload/hardware/quality constraints become a measured reproducible artifact.

**Established foundation:** semantic projection, Transformation IR, physical policy/variant, GGUF writer, admission and measurement.

**Open maturity boundaries:** sensitivity/calibration, feasibility filtering, candidate builds, Pareto selection and recipe evidence; future B1 weight/E precision/layout, derived materializations and target/backend compatibility.

**Current pressure:** GB10 working-set/quality tradeoffs and distinct tensor numerical obligations.

**Material advance:** reproducible constrained search feeds deterministic construction and independent final qualification.

**Does not own:** model suitability, task selection, runtime residency, N's cognitive-state realization meaning or the release gate itself.

### S — Sequence Runtime

**Purpose:** progress, isolation and resource truth across sequence lifetimes.

**Established foundation:** host, engines, leases, typed transactions, cooperative work and retained prefixes.

**Open maturity boundaries:** cross-provider paging, inflight scheduling, prompt fairness, prefix caching and long-context evidence; future E generations/leases/transactions and independent L checkpoint/resume through common physical owners.

**Current pressure:** recurrent versus attention geometry; actual concurrent workloads, not configured width; future N state must reuse typed physical lifecycle rather than another runtime.

**Material advance:** shared mechanisms qualified against distinct providers and resource-failure paths.

**Does not own:** cognitive meaning, family update equations, semantic model selection or unmeasured eviction policy.

### N — Native Cognitive State

**Purpose:** compile, retain, expose, version and evolve persistent model-native
computational cognitive state independently of ordinary token-context persistence.

**Existing physical foundations:** S/R own typed transactions, limited checkpoint
identity, stale-generation rejection and state-resource accounting. These support
future realization but do not implement N; program state remains OPEN.

**Open cognitive-state boundaries:** StateProfile, qualified future W ingress,
Lower and optional external Reconcile; E identity, compatibility/invalidation,
Read/Update realization, provenance and computational evaluation. The official
primary research target is [N.B1 — Slow-Update Dual-Stream](#nb1-slow-update-dual-stream),
not persistent prefix/KV. E and unfinished deliberation L remain distinct.

**Current architecture pressure:** YAI semantic authority must survive model
replacement; A01 rejects state-equals-KV assumptions. Refoundation prepares C/S/N
boundaries, not a cognitive-state runtime or an A01 evidence promotion.

**Material advance:** a real model/reference qualifies one declared point in
the representation/interaction/update/lifecycle space, with exact identity,
applicable ingress, negative/lifecycle and useful-state evidence. Read,
update, retention and non-interference are separate claims.

**Does not own:** Case/semantic memory, workflow, disclosure or authority policy,
canonical facts, family mathematics, S's physical mechanisms or P's model/package
recipe search. Computational updates return proposals/evidence, not semantic truth.

### G — Generation Control

**Purpose:** reusable sampling, constraint and speculation semantics.

**Established foundation:** greedy/stochastic filters, transactional RNG and target-verified DSpark commits.

**Open maturity boundaries:** penalties, probabilities, structured constraints, stop ownership and different proposal strategies.

**Current pressure:** typed consumer requests and exact token/state replay.

**Material advance:** new policies/strategies share one qualified lifecycle without changing another model's numerics.

**Does not own:** HTTP syntax, application tools or quality judgments.

### D — Dynamic Composition

**Purpose:** explicit executable identities for admitted component/adapter combinations.

**Established foundation:** component schedules and target/draft lifetimes.

**Open maturity boundaries:** adapter trust, base compatibility, representation, attach/detach and resource-safe coexistence; architecture-augmented composition, trained B1 State Encoder/read/update weights and provenance-bound external post-training intake.

**Current pressure:** heterogeneous component models and the unscheduled Qwen B1 adaptation target. Q2/PT0 surgery parity precedes Q3/PT1 frozen-backbone training; Q4/PT2 needs evidence. DeepSeek stays untouched D0. No post-training or trainer work is selected.

**Material advance:** reusable composition with exact specialization identity and rollback.

**Does not own:** invented compatibility, family numerics or premature multi-LoRA optimization.

### O — Output Runners

**Purpose:** model execution independent of chat.

**Established foundation:** shared autoregressive generation and bounded latent iteration.

**Open maturity boundaries:** embedding, pooling, scoring, classification, reranking, reward and general iterative results.

**Current pressure:** A08 retrieval and A04/A10 non-autoregressive expectations.

**Material advance:** a qualified typed non-chat result through existing engine/session owners.

**Does not own:** frontend command names, application ranking policy or cognitive roles.

### M — Multimodal Execution

**Purpose:** execute and publish typed media through common component owners.

**Established foundation:** multipart identity/capabilities and bounded MiniMax conditioning/latent/decoder/publication paths.

**Open maturity boundaries:** general preprocessing, streaming, feature caching and full-scale numerical/behavior correctness.

**Current pressure:** MiniMax evidence depth, future speech and still-image verticals.

**Material advance:** source-to-output composition with independent numerical and publication/lifecycle evidence.

**Does not own:** transport-only capability inflation, YAI roles or automatic A10 promotion.

### X — External Interfaces

**Purpose:** harness-neutral product semantics and platform-neutral interaction.

**Established foundation:** typed CLI/protocol, bounded compatibility, REPLAI editing and Linux qualification.

**Open maturity boundaries:** SDK consumers, macOS/Windows execution and explicitly scoped remote security.

**Current pressure:** real consumers and missing platform evidence, not historical names alone.

**Material advance:** another consumer/platform shares semantics with qualified cancellation and lifecycle.

**Does not own:** runtime topology, family meaning or editing mechanics inside YVEX.

### Q — Qualification

**Purpose:** promote only evidence earned at the claimed scope.

**Established foundation:** mapped QA, reference components, typed measurement and source-stable receipts.

**Open maturity boundaries:** upstream conformance, independent full-model quality, release benchmark and release qualification; future B1 surgery parity, held-out post-training/state-usefulness evaluation, explicit Q0–Q4/D0 controls and numerical/lifecycle/ablation evidence.

**Current pressure:** v0.1 dependencies, incomplete MiniMax full-scale evidence and authoritative full-model conformance. The exact post-refoundation warm control is retained as characterization, not a release benchmark. DeepSeek official/reference vectors and Qwen pinned upstream references remain selected future work, not executed evidence.

The [DeepSeek logits test](tests/live/logits_deepseek.c) qualifies each output
head against a reference given that backend's hidden input. Its full CPU/CUDA
comparison is observational, not an acceptance gate. The observed whole-model
disagreement predates this refoundation; exact before/after logits within each
backend remained unchanged. Resolve that independent conformance boundary
before treating component PASS results as full-model numerical evidence.

**Material advance:** reproducible independent results on exact identities, including negative evidence.

**Does not own:** targets as measured facts, selection-data reuse as independent proof or missing gates relabelled PASS.

The mandatory [quality-first closure rule][quality-reporting] requires expected
versus observed evidence, authority and tolerances; aggregate PASS counts are
supplemental. No upstream conformance harness is implied by the refoundation closure.

## Architecture Spectrum

Architecture Spectrum is an adversarial qualification program. Reference
models exert different pressure on topology, state, execution, composition or
typed I/O. A vertical's state records its evidence stage, **not generic system
maturity**. Planned references are not acquisition or executable support claims.

| ID | Computational pressure | Reference target | What it tries to falsify | Current public state |
| --- | --- | --- | --- | --- |
| A01 | Pure SSM | `mistralai/Mamba-Codestral-7B-v0.1` | Transformer-shaped decoder and computational-state-equals-KV assumptions | PARTIAL / repair queued after refoundation .1 qualification |
| A02 | Pure recurrent | RWKV7-1.5B | Sequence state must be attention/SSM-shaped | PLANNED |
| A03 | Encoder-decoder | FLAN-T5 | Decoder-only lifecycle; missing retained encoder state/cross-attention | PLANNED |
| A04 | Diffusion language model | LLaDA-8B-Instruct | Autoregressive-only generation | PLANNED |
| A05 | Hybrid SSM + attention | Falcon-H1-1.5B-Instruct | Homogeneous layer/state assumptions | PLANNED |
| A06 | Hybrid SSM + attention + MoE | AI21 Jamba Mini | Inability to compose heterogeneous state and sparse execution | PLANNED |
| A07 | Local/sliding attention | Mistral-7B-v0.1 | Uniform full-context attention | PLANNED |
| A08 | Encoder-only / retrieval | BGE-M3 | Model equals autoregressive generator | PLANNED |
| A09 | Audio / speech | Whisper-large-v3-turbo | Text-only input and decoder lifecycle | PLANNED |
| A10 | Unified image generation | `HiDream-ai/HiDream-O1-Image` | Text-token-only output and autoregressive-only execution | PLANNED |

A01 has pinned acquisition, complete source roles, common transactional recurrent
state and component numerics. It still refuses READY: tokenizer/special-token
and normalization authority, complete SSM decoder/artifact, deployment, load
and hosted generation remain unclosed. [Mamba2][mamba] owns that barrier.
Its broader pressure is that persistent computational state must not mean
Transformer KV. Future N realizations may consume attention, SSM, recurrent,
convolution, latent or architecture-native state without sharing storage geometry.
That relevance neither implements N nor bypasses A01's present blockers.

Every spectrum vertical begins with real reference acquisition through YVEX
and immutable revision capture before support claims. This public table does
not publish private ledger hashes, pending revisions or internal evidence
administration. A10 is still-image pressure, distinct from MiniMax synchronized
media; TEXT → IMAGE or TEXT + IMAGE → IMAGE would be executable I/O truth,
not a YAI cognitive capability.

### Architectural breadth strategy

Once expressible by common model language, typed state, component graphs and
runtime mechanisms, another model of that architecture should primarily need
source/compiler integration and physical qualification. Conventional models
are useful falsification tests too:

| Reference class | Architectural purpose |
| --- | --- |
| Llama-class dense decoder | Prove ordinary dense Transformer integration is inexpensive |
| Mixtral-class MoE | Separate common sparse execution from DeepSeek-specific mathematics |
| BERT/ModernBERT-class encoder | Break model-equals-causal-generator assumptions |
| T5-class encoder-decoder | Retained encoder state and cross-attention |
| Whisper-class speech encoder-decoder | Encoder-decoder lifecycle plus non-text input |
| Clean VLM | Reusable tower → connector → LM composition |
| Mamba2 | Reject Transformer-shaped state/decoder assumptions |

These are design probes, not additional scheduled milestones. Spectrum and
program evidence determine whether/when to use them. Existing DeepSeek,
Qwen and MiniMax evidence stays at its [family-specific scope][families].

## Current Execution Sequence

Updated in place. Only one boundary is ACTIVE; PARTIAL work below it is not
silently resumed. COMPLETE dependencies stay only while useful to the present
decision. This is neither the maturity matrix nor the release scope.

| Order | Boundary | State | Program | Maturity impact | Required after-state | Depends on |
| ---: | --- | --- | --- | --- | --- | --- |
| 1 | `MAINTENANCE.ARCHITECTURE.REASSESSMENT.0` | COMPLETE | R / X / C | Identity, client portability, architecture integrity | Qualified source-declared relations and platform-isolated client semantics; bounded performance control retained. | Accepted integrated foundation |
| 2 | `NATIVE.COGNITIVE.STATE.ALIGNMENT.0` | COMPLETE | R / N / Q | Adopted dual-stream target and evidence-reporting discipline; no implementation promotion | Freeze semantic/computational ownership and refoundation-before-A01 ordering in public control. | `MAINTENANCE.ARCHITECTURE.REASSESSMENT.0` |
| 3 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` | COMPLETE | R | Unique device-result validity owner; no N implementation or A01 promotion | Producer publication generations invalidate borrowed values before workspace reuse; consumers reject stale results without changing family numerics or public ABI/protocol. | `NATIVE.COGNITIVE.STATE.ALIGNMENT.0` |
| 4 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0` | COMPLETE | Q / R | Source-stable lifecycle, numerical component and bounded replay evidence | Mapped QA, stale-result/RNG negative tests and sanitizer lanes qualified; three warm fixed-output controls per clean tree preserve model/binding/kernel/output identity. Characterization only; upstream/full-model conformance remains open. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` |
| 5 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` | ACTIVE | R / C | Typed computational language, explicit state/effects, program/component and physical/target lowering boundaries | Migrate real consumers to verified IR; retire superseded authority. Separate source/catalog model identities, architecture import recipes and lowered execution; qualify consumer migration before removing historical family containers. Represent pure SSM without claiming A01 execution or N. Preserve numerics and .0 publication lifetimes. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0` |
| 6 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0` | NEXT | Q / R / C | Broad consumer evidence and comparable post-cutover replay | Qualify compiler refusals, migrated families, transactions, CPU/CUDA and sanitizers; replay exact controls and retain the independent full-model conformance gap. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` |
| 7 | `SPECTRUM.MAMBA2.REPAIR.0` | BLOCKED | C / S | Pure-SSM language, tokenizer authority, recurrent execution; A01 evidence remains PARTIAL | Resume after .1 qualification; resolve source/normalization authority and SSM-only executable boundary before earning the next evidence stage. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0` |
| 8 | `V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` | PARTIAL | Q / S / R | Warm execution performance | Measured bottleneck-driven improvement without numerical/lifecycle regression. | Explicit resumption and controlled workload |
| 9 | `V010.EVAL.DEEPSEEK.0` | BLOCKED | Q | Model behavior evidence | Repeatable quality, tokenizer, long-context and refusal evaluation. | `V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` |
| 10 | `V010.BENCH.DEEPSEEK.0` | NOT MEASURED | Q | Full-model performance | Identity-bound latency, throughput, memory and reliability evidence. | `V010.EVAL.DEEPSEEK.0` |
| 11 | `V010.RELEASE.0` | BLOCKED | Q | Release | All version-specific gates close together. | Benchmark and remaining release obligations |

Active Next: MAINTENANCE.ARCHITECTURE.REFOUNDATION.1

The bounded .0 refoundation and its qualification remain complete. The selected
order is now **.1 typed compiler/runtime cutover → .1 qualification/replay →
A01 repair**. Blocking A01's temporal row does not demote its PARTIAL evidence
or change `repair_same_boundary`. DeepSeek optimization stays PARTIAL pending
explicit resumption; upstream/whole-model conformance remains Program Q work.

The [current consumer matrix](docs/architecture/compilation.md#current-consumer-cutover)
distinguishes Qwen's physical forward/output runner, shared output projection
and DeepSeek's final mHC, target-feature reduction and CPU/full-evidence
four-input residual-post programs from remaining DeepSeek/DSpark device-native
composition and MiniMax neural computational migration.
Exact Qwen artifact-backed preservation is a separate evidence obligation:
the registered BF16 representation has been acquired through canonical pull and
is locally READY. Its compiled forward/output consumer has completed a bounded
real CUDA generation (15 prompt tokens, two sampled tokens, EOS, no prefix reuse).
That execution does not establish before/after whole-model preservation or
upstream conformance; those obligations remain unqualified. Mamba remains
representability evidence.
The `.1` progression decision remains `repair_same_boundary`. Official DeepSeek
vectors and authoritative upstream Qwen conformance have not been executed;
the pre-existing whole-model DeepSeek CPU/CUDA discrepancy (max absolute error
approximately 9.982168) is not resolved by migration-preservation tests.

The .1 boundary also owns truthful public execution-capacity admission (R / X):
discovery and deterministic preflight must describe the exact engine/tokenizer
envelope, and refusals must identify the exceeded dimension. A small probe is
not evidence that arbitrary dense content fits. This repair does not increase
the configured context or admit mandatory-content truncation. YVEX's real-model
positive/negative controls do not establish an external consumer's Golden PASS;
that consumer must qualify the published profile and rerun its real workload.

Real external prefill execution is retained as `.1` characterization
(R / S / Q), not a reopened capacity defect or a compiler-cutover closure gate.
The selected closure criterion remains the implemented and qualified universal
consumer/lowering cutover. Golden latency does not block that boundary, and
removing it as a gate does not turn its failure into a pass or qualify YAI's
external lifecycle. The unchanged 40,277-byte Golden
request tokenizes to 12,055 inputs and fits the explicitly configured
16,384-token deployment with 4,329 output tokens available. Exact CUDA candidate
scoring and cooperative ranking preserve exact numerical results and ordered head reduction,
candidate identities/ties/refusals and capacity-stable graph replay, qualified
by the [independent selection oracle](tests/unit/cuda/attention_selection.c),
device memory/synchronization checks and real target/DSpark regressions.
Native reduction retains lane-local accumulators, encoded expert rows specialize
admitted geometry, and ordinary dots defer operand-finiteness rescanning to
exceptional results. The [reduction](tests/unit/cuda/attention_reduction.c),
[expert-row](tests/unit/cuda/moe_rows.c) and
[finite/exceptional-dot](tests/unit/cuda/dot_finiteness.c) oracles preserve
numerics, finite-overflow recovery and fail-closed invalid operands. These are
backend mechanism repairs, not new physical recipes or compiler cutover claims.
Encoded projections improve weight-row locality and expert dots reuse the
canonical IQ2 lookup table in block-local storage without changing dot order.
A separately demonstrated block-softmax shared-reduction race is repaired at
its storage-reuse boundary; the [bounded softmax oracle](tests/unit/cuda/attention_softmax.c)
qualifies causal/finite/negative behavior and exact repetition, with zero
reported device memory, synchronization and race errors at that scope.

The current published unmodified-request replay uses loaded weights but a fresh
session and zero reused prompt tokens: 918 tokens in 42.73 s, 5,532 at 276.01 s
and 10,884 at 567.85 s. Its last progress is 11,694/12,055 in 613.70 s
(19.05 tokens/s cumulative); producer HTTP 504 arrives at 616.18 s, with zero
generated tokens and clean cancellation/session retirement. The subsequent
five-input/one-output-token control still returns HTTP 200. Complete prefill,
first-token latency and normal response completion remain unqualified; the
local-protocol timeout is unchanged. Three bounded GPU traces of the current
published binary separate scoring from ranking: near 600, 1,806 and 3,600
processed tokens, mean scoring takes 37.12, 54.29 and 79.00 microseconds;
ranking takes 19.84, 25.94 and 44.18 microseconds. MoE/projection and attention reduction dominate
the sampled device work; stream synchronization includes device waiting, not
an independently additive bottleneck. The late-window MoE-up launch classes
must remain distinct: 9,216 blocks average 1,225.20 microseconds, while 1,536
blocks average 276.24 microseconds. Isolated lookup improvement does not prove
equivalent improvement across these real populations. Qualified repairs have not closed
the real request. Further performance work must preserve numerical and
candidate semantics and qualify complete-request behavior across the prompt,
not just its first prefix or isolated kernel.

`downstream_safe=false` for this external workload, independently of whether
the compiler cutover earns its own progression. Capacity preflight does not
qualify execution latency; neither a selector test nor a first response closes
External Golden or the remaining universal consumer cutover. YAI owns its
external free/Workflow verdict; no timeout increase, truncated content, implicit
fallback or model-specific workaround may substitute for execution evidence.

The .1 source-layout criterion is semantic ownership, not a filename taxonomy:
source/catalog owns exact models, revisions, selectors and relationships;
architecture importers own source-schema interpretation and IR projection;
typed programs and admitted operations own computation. Names such as `mamba2`
may remain legitimate architecture importers or irreducible operation owners.
Moving or renaming a `families` file alone does not close the boundary, and
generalizing an importer must not silently widen its qualified source envelope.

The [GB10 workload/measurement authority][gb10] constrains replay. The retained
pre/post control uses clean source snapshots, 44 input tokens and 256 committed
output tokens per measured run, target-only greedy execution, one session and
the same exact deployment. Three sequential warm repeats characterize this
boundary; thermal/clock observations are not a randomized causal experiment.
No architecture-level speedup, upstream conformance or release gate is claimed.

## General Substrate Progression

Dependency horizons are not dates, staffing promises or automatic permission
to implement every OPEN row.

| Horizon | Engineering objective | Programs | Exit condition |
| --- | --- | --- | --- |
| Now | Establish typed model/program IR and migrate execution before A01 | R / C / S | Verified computational, physical and target boundaries; qualified consumer cutover and replay precede A01 repair. No N runtime implementation. |
| Near | Broaden model language and physical representation foundations | C / P | Common architectures compile through shared semantics and reproducible recipes. |
| Near | Prepare persistent-state language and realization/lifecycle boundaries | C / S / N | Future state-capable models need not create another runtime; semantic authority remains external. |
| Near | Mature single-node sequence runtime | S / G | Typed paging, inflight scheduling, chunked prefill and reusable prefixes qualified. |
| Near/Mid | Expand execution beyond chat | O / D / M | Embedding/scoring/adapters/media use common engine and result semantics. |
| Near/Mid | Qualify selected computational-state research points under real model pressure | N / C / S / D / Q | N.B1 is the primary target; Qwen surgery/post-training and provenance earn independent evidence. Prefix/KV remains a control, not the destination. No implementation wave is selected. |
| Mid | Close independent qualification gaps | Q | Selected current-tree behavior, performance and reliability evidence reproducible. |
| Mid | Qualify computational cognitive-state behavior | N / Q | Measure retention, interference, forgetting, cross-context continuity, update stability and recompilation/invalidation; do not replace YAI semantic evaluation. |
| Later | Scale beyond the present single-node substrate | F | Distributed identity, state, scheduling and failure semantics established. |

Qualification accompanies every horizon; Mid describes completion of the broad
evidence surface, not permission to postpone correctness tests.

### Mature-runtime comparison surface

Official [vLLM features][vllm], [TensorRT-LLM's compatibility matrix][trt] and
[MLC LLM deployment documentation][mlc] provide external reference surfaces.
These are not competitor scores or prescriptions for YVEX's internal design.
Feature combinations, model coverage and platforms have their own limits;
presence elsewhere does not establish YVEX feasibility.

| Mature-runtime capability | YVEX maturity area | Why it matters / primary reference |
| --- | --- | --- |
| Inflight / continuous batching | Sequence Runtime | Throughput/fairness; [TensorRT-LLM scheduling][trt-scheduling] |
| Paged sequence/KV management | Sequence Runtime | Dynamic memory; [TensorRT-LLM scheduling][trt-scheduling] |
| Prefix caching / KV reuse | Sequence Runtime | Repeated-prefix efficiency; [vLLM features][vllm] |
| Chunked prefill | Sequence Runtime | Long-prompt fairness/latency; [TensorRT-LLM scheduling][trt-scheduling] |
| LoRA/adapters | Dynamic Composition | Deployment flexibility; [vLLM features][vllm] |
| Structured/guided decoding | Generation Control | Constrained results; [vLLM structured output][vllm-structured] |
| Logprobs | Generation Control / Output | Token observability/scoring; [TensorRT-LLM outputs][trt-outputs] |
| Speculative decoding | Generation Control | Decode acceleration; [TensorRT-LLM matrix][trt] |
| Embedding/pooling/scoring | Output Runners | Beyond chat; [vLLM pooling][vllm-pooling] |
| Multimodal input | Multimodal Execution | Component inputs; [vLLM features][vllm] |
| Distributed execution | Scale-out | Multi-device scaling; [TensorRT-LLM matrix][trt] |
| Disaggregated serving | Scale-out | Split execution stages; [TensorRT-LLM matrix][trt] |

MLC's Python/REST/CLI and platform paths additionally pressure X's separation
of compilation, execution and consumer packaging. They do not establish YVEX
platform qualification. [MLC quick start][mlc]

### Cross-axis traceability

| Active / near boundary | Program | Maturity rows affected | Spectrum pressure | Release impact |
| --- | --- | --- | --- | --- |
| State-target alignment, COMPLETE | N / R / Q | Strategic adoption and evidence rules only; N remains OPEN | Non-KV state is pressure, not cognitive support | No release evidence or gate changes |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0`, COMPLETE | R | Producer-owned result validity; preserves C / S / N separation, does not implement N | A01 remains PARTIAL; no family promotion | No automatic gate closure |
| Post-refoundation qualification and exact replay, COMPLETE | Q / R | Changed borrow contracts and bounded comparable performance; full-model conformance still open | Generic prerequisite qualified before A01 repair | Evidence only at measured scope, not release qualification |
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1`, ACTIVE | R / C | Typed values, shapes, operations, effects, programs and explicit lowering; no N implementation | Pure-SSM representability is required, not A01 execution | Preserve numerics; no automatic release promotion |
| .1 real Golden prefill characterization | R / S / Q | Long-context execution and measured residual bottlenecks; characterization, not a .1 closure gate | Existing DeepSeek execution pressure, not A01 progression | Full request still fails; no downstream-safe claim or behavior, benchmark or release gate promotion |
| .1 qualification/replay, NEXT | Q / R / C | Compiler refusal, migrated consumer and comparable execution evidence | Precedes queued A01 repair | No upstream conformance inferred from internal parity |
| `SPECTRUM.MAMBA2.REPAIR.0`, queued | C / S | Semantic IR, composition, non-KV state, tokenizer | A01 directly pressures generic state and informs N; no automatic N promotion | Indirect; no v0.1 promotion or scope expansion |
| Future cognitive-state realization qualification, unscheduled | N / C / S / Q | Ingress, model-specific State Read/Update and useful-state evidence | Real model/reference required; no new Axx scheduled | Not a v0.1 gate |
| Explicit DeepSeek optimization resumption | Q / S / R | Baseline, resource/structural performance evidence | Existing DeepSeek control, not a new Axx | Predecessor of evaluation |
| Model-language breadth after A01 | C / O / D / M | Components, heads, tokenizer and modality semantics | A02–A10 selected by pressure | General substrate, not automatically v0.1 |
| Single-node runtime maturity | S / G | Paging, inflight batching, prefill, caching | Distinct admitted state providers | Only requirements explicitly admitted by version scope |
| Physical recipe search | P / Q | Policy, synthesis, search, qualification | Different tensor/state numerical obligations | Produces candidates, not release evidence |
| v0.1 evaluation → benchmark → release | Q | Behavior, benchmark, release qualification | No spectrum promotion | Direct release gates |

## v0.1 Release Path

v0.1 targets DeepSeek-V4-Flash-DSpark text on admitted GB10 CUDA through a
complete YVEX artifact, native host and bounded compatibility surface.
[The version record][release] owns required scope; [doctrine][doctrine] owns
gate semantics. Spectrum breadth must not silently expand this release.
Native Cognitive State is an independent substrate direction, **not a new v0.1
gate**. This release may use today's context-compatible model architecture.

| Release gate | State | Evidence owner | Blocking prerequisite | What completion proves |
| --- | --- | --- | --- | --- |
| Source/artifact reproducibility | PARTIAL | [Artifacts][artifacts]; [model release][model-release] | Final source/recipe/build and independent qualification; candidate publication exists | Shipped artifact is reproducible, not just available |
| Runtime functional correctness | PARTIAL | [Runtime QA][qa]; [v0.1][release] | All lifecycle/numerical obligations on final profile; product paths already execute | Exact release path executes and fails correctly |
| Performance optimization boundary | PARTIAL | [GB10 optimization][gb10] | Measured bottleneck-driven closure | Selected implementation is intentional and characterized |
| Model behavior evaluation | BLOCKED | [Evaluation gate][doctrine] | Optimization boundary | Quality, tokenizer, refusal and long-context behavior |
| Full-model benchmark | NOT MEASURED | [Benchmark gate][doctrine] | Evaluation | Latency, throughput, memory and reliability |
| Packaging/distribution | PARTIAL | [Model release][model-release]; [v0.1][release] | Final artifacts, operator/package validation and versioned delivery | Releasable package, not just publication tooling |
| Release qualification | BLOCKED | [Release doctrine][doctrine] | Every required gate and claim audit | v0.1 may be claimed |

Established artifact/runtime mechanisms and partial **release-wide gates** are
consistent: release gates require the final variant and full obligation set.
No current physical variant is already the release profile.

```text
model_behavior_evaluation_ready=0
full_model_release_benchmark_ready=0
release_qualification_ready=0
```

## Explicit Nonclaims and Deferred Scope

| Capability / claim | Current truth | Why not claimed yet |
| --- | --- | --- |
| Continuous batching ready | false | Dynamic join/leave unqualified; workers/rows are insufficient. |
| General paged typed sequence state | false | Page/COW foundations are not a general cross-provider/device manager. |
| Cross-request prefix caching | false | Shared prefixes are not an automatic authenticated cache. |
| Full model evaluation ready | false | Independent behavior/quality gate open. |
| Full release benchmark ready | false | Release benchmark incomplete. |
| Release qualification ready | false | Independent version-specific gates open. |
| Mamba2 READY / hosted generation | false | Source authority and complete decoder/artifact unresolved. |
| Generic multimodal execution / full MiniMax quality | false | Typed transport and bounded composite output are weaker evidence. |
| Physical Model Compiler automatic search | false | Manual recipes/synthesis foundations are not search/selection. |
| macOS product terminal qualification | false | Portable structure is not platform evidence. |
| Windows/ConPTY product qualification | false | No qualified product backend/path. |
| Public remote serving | false | Local/loopback is not authenticated production service. |
| Authentication/TLS | false | No implemented/qualified product security boundary. |
| Distributed execution | false / LATER | Single-node is the admitted substrate. |
| Disaggregated serving | false / LATER | Distributed identity/state/failure semantics must precede it. |
| Generic semantic-state ingress | false | Content inputs do not implement a semantic-state lowering boundary. |
| Generic model-state compilation | false | No semantic source → exact computational realization contract. |
| Persistent cognitive State Read | false | Ordinary sequence state is not an independently persistent cognitive bank. |
| Generic State Update | false | No admitted cognitive-state update contract. |
| Learned/native State Update | false | No independently qualified learned/native update architecture. |
| Multi-timescale cognitive-state realization | false | Adopted representational target, not current layout support. |
| Public W producer / W→E wire contract | false | YAI Recall v2 is bounded implemented truth; working-state selection is next/unselected. No StateFrame/StateDelta ABI or serialization is invented here. |
| Cognitive-state retention/interference qualification | false | No declared qualified computational cognitive-state evaluation. |
| Native-state model support | false | Strategic adoption is not architecture admission or execution. |

The following remain explicitly **not implemented / not qualified**: public W
ingress; W→E Lower; generic Reconcile; Experiential Computational State; B1;
R/E dual-stream execution; StateProfile runtime; State Encoder; cross-state
attention; Slow State Update; E committed/working/candidate runtime, leasing
and residency; state-root effect scheduling; intra-program concurrent scheduling;
B1 Qwen weights or post-training; selective B1 backbone adaptation; B1 DeepSeek;
YVEX-native trainer; generic external post-training integration; L checkpoint/resume;
and YAI-specific memory execution.

Current prefix/KV != E; current typed IR state != Program N; current
candidate/committed sequence transactions != B1 State Update; engine/session
checkpointing != qualified L continuity. The B1-v0 tensor budget is reference
design arithmetic, not allocation/performance evidence, frozen ABI or final model
architecture. D0 has no B1 weights and no DeepSeek B1 training is scheduled.

This alignment selects no implementation wave, does not start Program N, B1,
post-training or A01, and does not expand/rename/reinterpret active refoundation
.1. The live .1 → .1 qualification → A01 sequence and all v0.1 gates retain
their own criteria. B1 and model adaptation remain unscheduled target doctrine.

LATER is deliberate scope deferral, not concealed debt in a claimed release.
OPEN remote/security rows are visible maturity gaps, not new v0.1 obligations.
Cognitive roles and application task selection stay outside YVEX; external
consumers may demonstrate execution pressure, not prescribe physical ownership.
KV, recurrent/SSM state, paging, prefix sharing and state checkpointing do not
by themselves establish Native Cognitive State. Context-only compatibility
remains valid; it is neither generic semantic-state lowering nor the final
architecture assumption. Native Cognitive State targets are OPEN, not deferred
out of the architecture horizon or silently added to v0.1.

## Progression and Promotion Discipline

Implementation existence does not imply generic maturity. Promotion requires
**canonical owner + implemented contract + appropriate negative evidence +
qualified real boundary**. The claim's scope determines the required proof.

| Existing evidence | Does not establish |
| --- | --- |
| One executable media vertical | Generic multimodal execution |
| One speculative strategy | Generic speculation breadth |
| Multi-row kernels | Continuous batching |
| Large context capacity | Mature long-context runtime |
| Manual mixed quantization | Physical Model Compiler search |
| Source recognition / tensor inventory | Executable architecture support |
| Component numerical evidence | Whole-model conformance |
| Model execution | Model quality |
| Model quality | Benchmark |
| Benchmark | Release |
| Persistent bytes | Cognitive state |
| State checkpoint | Semantic memory |
| KV reuse | State Read |
| Recurrent state | Native Cognitive State |
| Model-produced latent delta | Canonical semantic fact |
| State Read implementation | Useful memory |
| State Update implementation | Stable memory |
| Retention | Absence of interference |
| Model-native state | YAI authority |

Use the lowest demonstrated stage. Green software QA cannot close missing
numerical, lifecycle, operator or quality authority. Characterization is not a
benchmark; selection data cannot independently qualify its selected result.
Performance comparisons fix model bytes, representation, mode, workload,
hardware and comparable environment, with repeated observations and dispersion.
N rows advance only on their exact implemented and qualified generic property,
not adjacent state primitives. Computational evidence may inform YAI admission;
it cannot perform semantic canonicalization on YAI's behalf.

The [engineering method][method] defines `proceed`, `repair_same_boundary`,
`complete_evidence` and `blocked_external`. Finishing a prompt is not permission
to advance. An Axx can advance while generic maturity stays PARTIAL; a subsystem
can mature outside v0.1; release may stay BLOCKED while spectrum work continues.

### Living-update rules

| View | Update rule when a wave closes |
| --- | --- |
| Current Snapshot | Replace current facts in place; no dated snapshots. |
| Current Execution Sequence | Advance one active boundary/dependencies; retire obsolete context to Git. |
| System Maturity | Change only generic claims with new evidence; retain explicit scope. |
| Strategic Programs | Update affected foundation, pressure and state; wave != program. |
| Architecture Spectrum | Change only actual family evidence, never from shared plumbing alone. |
| Release Path | Change only independently closed gates. |
| Nonclaims | Remove only after corresponding evidence exists. |

The summary is generated solely from System Maturity rows. After editing those
rows, run `python3 tests/documentation_architecture.py --update-roadmap-counts`.
Normal guards reject stale totals, invalid states, competing authorities and
inconsistent active views. Counts are not a second registry. Do not create
STATUS/FEATURES/PLAN files, dated snapshot walls or public copies of private
H classifications. Git owns previous versions; this file owns the present.

[system]: docs/architecture/system.md
[compilation]: docs/architecture/compilation.md
[runtime]: docs/architecture/runtime.md
[commands]: docs/architecture/commands.md
[families]: docs/model-families/integration.md
[mamba]: docs/model-families/mamba2.md
[deepseek]: docs/model-families/deepseek-v4-flash.md
[minimax]: docs/model-families/minimax-h3.md
[storage]: docs/contracts/model-storage.md
[artifacts]: docs/contracts/artifacts.md
[runtime-contract]: docs/contracts/runtime.md
[events]: docs/contracts/events-telemetry.md
[protocol]: docs/contracts/local-protocol.md
[c-api]: docs/contracts/c-api.md
[model-release]: docs/contracts/model-release.md
[lifecycle]: docs/model-lifecycle.md
[method]: docs/development/agentic-engineering.md
[qa]: docs/development/qa.md
[qa-registry]: config/qa/registry.json
[gb10]: docs/development/gb10-targets.md
[reference]: docs/development/reference-baseline.md
[release]: docs/releases/v0.1.md
[doctrine]: docs/releases/doctrine.md
[editor]: docs/decisions/0007-external-terminal-editor.md
[speculation]: docs/decisions/0004-target-verified-speculation.md
[openai]: docs/openai-compatibility.md
[source-tests]: tests/unit/source_verify.c
[catalog-tests]: tests/unit/model_registry.c
[writer-tests]: tests/unit/artifact_writer_runner.c
[integrity-tests]: tests/unit/artifact_integrity.c
[resource-tests]: tests/unit/engine_resource.c
[sampling-tests]: tests/unit/runtime_sampling.c
[speculation-tests]: tests/unit/speculation.c
[terminal-tests]: tests/integration/terminal_scope.c
[tokenizer-tests]: tests/unit/runtime_tokenizer.c
[decoder-plan]: src/graph/decoder_plan.c
[state-store]: src/runtime/state_store.c
[state-checkpoints]: include/yvex/internal/runtime_state_store.h
[state-tests]: tests/unit/runtime_state.c
[state-residency]: src/runtime/state_residency.c
[quality-reporting]: AGENTS.md#quality-first-closure-reporting
[state-pages]: src/graph/state_pages.c
[state-prefix]: src/graph/state_prefix.c
[sampling]: src/runtime/sampling.c
[sampling-api]: include/yvex/internal/sampling.h
[latent]: src/runtime/latent.c
[media-runtime]: src/runtime/media.c
[media-io]: src/io/media.c
[image]: src/io/image.c
[tokenizer]: src/tokenizer/execution.c
[tokenizer-core]: src/tokenizer/core.c
[tokenizer-compile]: src/tokenizer/compilation.c
[vllm]: https://docs.vllm.ai/en/latest/features/
[vllm-structured]: https://docs.vllm.ai/en/latest/features/structured_outputs/
[vllm-pooling]: https://docs.vllm.ai/en/latest/models/pooling_models/
[trt]: https://nvidia.github.io/TensorRT-LLM/features/feature-combination-matrix.html
[trt-scheduling]: https://nvidia.github.io/TensorRT-LLM/features/paged-attention-ifb-scheduler.html
[trt-outputs]: https://nvidia.github.io/TensorRT-LLM/features/additional-outputs.html
[mlc]: https://llm.mlc.ai/docs/get_started/quick_start.html
