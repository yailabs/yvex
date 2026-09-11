# YVEX Roadmap

Status: living public project control

## At a Glance / Current Snapshot

| Axis | Current truth |
| --- | --- |
| Project target | Native, harness-independent model compilation and execution substrate. |
| Active engineering boundary | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` — typed multi-level model/program IR and consumer cutover before A01 resumes. |
| Latest generic architecture boundary | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.0` and its qualification COMPLETE at producer-owned device-result validity: expired borrows fail closed; source-stable QA and exact bounded performance replay retained. Earlier identity/platform repairs preserved. |
| Architecture Spectrum | A01 PARTIAL, repair queued behind refoundation .1 and qualification; A02–A10 PLANNED. |
| Adopted state architecture target | Dual-stream primary computation plus persistent model-native computational state. YAI owns semantic authority; YVEX owns computational realization. Program N is OPEN, not implemented. |
| Most important structural gap | Physical forward/output programs exist, with bounded hybrid CUDA/state and real output-head evidence. DeepSeek/DSpark and MiniMax still retain computational authority requiring migration before .1 closes. Exact Qwen artifact-backed preservation remains separately blocked by asset identity. A01 tokenizer/normalization and whole-model barriers remain. |
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
| 🔴 OPEN | Generic capability absent or not yet claimable. | 51 |
| ⚪ LATER | Intentionally outside the current maturity horizon. | 14 |
<!-- maturity-counts:end -->

Counts describe the rows below, **not a percentage of project completion**.
A narrow established mechanism does not make its entire program established.

Navigate: [maturity](#system-maturity) · [programs](#strategic-programs) ·
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
| Semantic Model IR | 🟡 PARTIAL | Typed programs, state/effect verification, Qwen forward lowering and pure-SSM representability exist; not every executable consumer uses this authority. | Express heterogeneous semantics without family runtimes or compulsory Transformer structure. | C | [Compilation][compilation]; [Mamba2 barrier][mamba] |
| Operator composition / graph language | 🟡 PARTIAL | Shared lowering and execution owners serve several families and component schedules. | New expressible variants primarily need compiler projection, not new runtime topology. | C / D | [Family boundary][families]; [decoder plan][decoder-plan] |
| Transformation IR | 🟢 ESTABLISHED | Typed, ordered, artifact-neutral transformations precede payload materialization. | Remain the unique transformation authority as representations expand. | C / P | [Compilation][compilation] |
| Physical policy | 🟡 PARTIAL | Per-terminal dtype/qtype, layout and alignment decisions exist for admitted recipes. | Broaden compiler-owned physical decisions without downstream reconstruction. | P | [Compilation][compilation] |
| Physical Execution IR | 🟢 ESTABLISHED | Package physical records are sealed separately from deployment implementation choices. | Preserve authenticated consumption as semantic operations broaden. | C / P / R | [Artifact contract][artifacts]; [compilation][compilation] |
| Artifact emission | 🟢 ESTABLISHED | Transactional GGUF construction publishes admitted package representations. | Preserve deterministic construction and rollback across broader representations. | P | [Artifact contract][artifacts]; [writer tests][writer-tests] |
| Artifact admission | 🟢 ESTABLISHED | Integrity, roles, identities and binding constraints fail closed. | Extend coverage without weakening integrity or mandatory semantic checks. | P / R | [Artifact contract][artifacts]; [integrity tests][integrity-tests] |
| Runtime binding | 🟢 ESTABLISHED | Authenticated package truth is consumed without rebuilding compiler plans. | Preserve package meaning versus runtime specialization. | R / P | [Runtime contract][runtime-contract] |
| Existing quantized representation import | 🟡 PARTIAL | Multiple GGUF qtypes and low-precision execution paths are admitted. | Broader format coverage without per-format runtime redesign. | P | [Compilation][compilation]; [DeepSeek][deepseek] |
| Quantization synthesis | 🟡 PARTIAL | Mixed per-tensor policy and bounded calibration-informed recipes can be constructed. | Generic sensitivity/calibration-driven synthesis with reproducible decision provenance. | P | [GB10 targets][gb10]; [compilation][compilation] |
| Physical Model Compiler search | 🔴 OPEN | Deterministic lower layers exist; recipe exploration remains bounded/manual engineering. | Hardware/workload/quality-aware search and reproducible Pareto selection. | P | [GB10 targets][gb10]; [compilation][compilation] |

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
| State realization profile | Model-specific realization constraints | Bind the state target to exact model capability |

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
| Transactional typed state | 🟢 ESTABLISHED | Distinct attention, recurrent, convolution, speculative, RNG, decoder/media representations coordinate commit/abort/reset. | Retain common lifecycle without merging unlike geometry into fake KV. | S | [Runtime][runtime]; [state store][state-store] |
| Resource accounting | 🟢 ESTABLISHED | Mapped, prepared, allocated, addressable and observed current/peak facts remain distinct. | Preserve known/unknown resource truth under dynamic placement. | S / R | [Resource truth][runtime]; [resource tests][resource-tests] |
| Resource admission | 🟡 PARTIAL | Live capacity and backend facts constrain bounded engine admission. | Broader placement/concurrency admission with measured reserves and negative evidence. | S | [Runtime][runtime]; [resource tests][resource-tests] |
| Automatic residency policy | 🔴 OPEN | Explicit load/unload and ensure-active are mechanisms, not a generic eviction policy. | Bounded retention/placement/eviction from authoritative resource facts. | S | [Storage contract][storage] |
| Paged typed sequence state | 🟡 PARTIAL | Virtual-page budgeting and attention-oriented prefix/COW backing exist; no general cross-provider/device page manager. | Typed-provider paging, reuse and movement with transactional identity. | S | [Page store][state-pages]; [prefix backing][state-prefix] |
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
| Model-state capability advertisement | 🔴 OPEN | Context capacity and sequence-state classes do not advertise cognitive-state realization. | Exact model/deployment advertises context-only, retained state, external State Read, State Update or native state with compatibility constraints. | N / C | [Adopted target](#native-cognitive-state); [current runtime contract][runtime-contract] |
| Semantic-state ingress | 🔴 OPEN | Typed content and session state are not a semantic-state lowering contract. | Typed external frame/delta constrains lowering without importing Case semantics. | N / X | [Adopted target](#native-cognitive-state) |
| Computational cognitive-state identity | 🟡 PARTIAL | Physical layout/content/checkpoint identities exist; none binds semantic source to a cognitive realization. | Bind semantic source, exact model, lowering/profile and update lineage sufficiently for reuse or invalidation. | N / S | [Checkpoint records][state-checkpoints]; [state store][state-store] |
| Context-only compatibility lowering | 🔴 OPEN | Ordinary context/prefill executes; prompt rendering is not provenance-bound semantic-state lowering. | Ingress → context realization has explicit provenance, limits and reproducibility. | N / C | [Adopted target](#native-cognitive-state); [runtime][runtime] |
| Persistent state compilation | 🔴 OPEN | Physical checkpoints retain execution state, not compiled semantic-state realizations. | Compile semantic state into independently retained state for an exact model/profile. | N / C | [Adopted target](#native-cognitive-state) |
| Incremental state compilation | 🔴 OPEN | No semantic delta-to-computational update contract. | Validate when a delta permits partial recompilation and when complete rebuilding is required. | N / C | [Adopted target](#native-cognitive-state) |
| Persistent State Read | 🔴 OPEN | Internal KV/recurrent sequence updates are not reads of an independently persistent cognitive bank. | Explicit model-semantic operation consumes independently retained state with qualified identity and effects. | N / C | [Adopted target](#native-cognitive-state); [family boundary][families] |
| Layer-aware state injection / interaction | 🔴 OPEN | No declared cognitive-state interaction topology. | Express model-specific cross-attention, gating, projection, recurrent interfaces or state-consuming blocks without freezing one mechanism. | C / N | [Target language](#semantic-model-ir-target-language) |
| Multi-timescale state layout | 🔴 OPEN | No cognitive-state realization with declared update/retention timescales. | Distinct computational scales remain representable without fixed tensor counts or semantic-memory taxonomy. | N / C | [Adopted target](#native-cognitive-state) |
| External State Update | 🔴 OPEN | No external/compiler-assisted cognitive-state update contract. | New realization has explicit provenance, compatibility and transaction semantics. | N / S | [Adopted target](#native-cognitive-state) |
| Learned State Update | 🔴 OPEN | No qualified learned producer of cognitive-state deltas. | Model/companion updates are computationally qualified without acquiring semantic mutation authority. | N / C | [Adopted target](#native-cognitive-state) |
| Native State Update | 🔴 OPEN | No admitted trained architecture exposes this persistent-state update contract. | First-class trained update operation has independent numerical, lifecycle and usefulness evidence. | N / C | [Adopted target](#native-cognitive-state) |
| Cognitive-state checkpoint provenance | 🟡 PARTIAL | Attention target/draft checkpoints bind model, artifact, binding and payload; recurrent sequence serialization is explicitly refused. No semantic provenance. | Bind computational checkpoint to semantic source, parent update and realization compatibility; qualify supported providers. | N / S | [State store][state-store]; [restore/rejection tests][state-tests] |
| Model replacement / state invalidation | 🟡 PARTIAL | Generation and checkpoint identity reject stale/incompatible state; semantic-source recompilation is absent. | Invalidate/rebuild computational state on incompatible model/profile changes without changing semantic authority. | N / R / S | [Runtime contract][runtime-contract]; [state store][state-store] |
| Cognitive-state locality / residency | 🟡 PARTIAL | Session-state banks/accounting are separate from weights; no independently retained cross-context cognitive-bank lifecycle. | Explicit state locality/residency and movement independent of weights and prompt storage, with resource-failure evidence. | N / S | [State residency][state-residency]; [resource truth][runtime] |
| Retention / interference / forgetting evaluation | 🔴 OPEN | No declared cognitive-state evaluation or qualified result. | Measure retention, interference, intended forgetting, continuity and update stability on identity-bound realizations. | N / Q | [Adopted target](#native-cognitive-state) |
| Model → YAI return-path evidence | 🔴 OPEN | No cognitive-state proposal/consequence interface to external semantic authority. | Expose operation/model/parent-state/update evidence without claiming semantic canonicalization. | N / X / Q | [Adopted target](#native-cognitive-state) |

The four PARTIAL rows describe limited **physical foundations**, not partial
State Read/Update support. Program N as a whole is OPEN: its ingress and
realization contract do not exist. Existing S/R mechanisms remain their owners.

#### Semantic authority and computational realization

| Owner | Boundary |
| --- | --- |
| YAI | Semantic meaning/continuity, Case-bound authority, disclosure/authority semantics and semantic `StateFrame` / `StateDelta` producer semantics. |
| YVEX | Exact model-state capability, semantic-state lowering, computational identity/layout, read/update execution contracts, realization compatibility/provenance; paging/residency and checkpoint/rollback through physical runtime owners. |
| Model | Learned interpretation of computational state and, where supported, learned state-update behavior. |

YAI preserves what the system knows. YVEX determines whether and how an exact
model can carry it computationally. **YAI semantic state != YVEX computational
state.** YVEX is not a Case owner, semantic-memory database, workflow/policy or
authority owner, canonical fact store or YAI replacement. YAI does not own KV
layouts, recurrent tensors, latent banks, state pages, GPU residency/movement,
physical checkpoints, model layers or native update equations.

The dual-stream target is primary token/activation/residual computation alongside
cross-context persistent, model-specific computational state. The second stream
can carry independent identity and, where the architecture permits, independent
residency/versioning. Its read and eventual update are explicit model operations:

```text
YAI semantic S_t -- SemanticStateFrame / SemanticStateDelta --> YVEX lowering
                                                             │
                      M_t = Lower(S_t, exact model, profile) <─┘
                                      │
primary input / activations ──> model graph <── State Read(M_t)
                                      │
                         output + State Update(M_t), if supported
                                      │
                     computational evidence / typed consequence
                                      │
                        YAI semantic authority / admission
                                      │
                                    S_t+1
```

Frame/delta names here identify a **target boundary**, not current wire types.
`S_t` remains authoritative and may be portable across models. `M_t` is derived,
model-specific, possibly opaque, disposable and rebuildable. No latent tensor
becomes a canonical Case fact merely because execution changed it. YVEX may
attest what operation ran, which model/parent state produced the change, and
its computational provenance; semantic proposals remain subject to YAI admission.

#### Realization depth

| Depth | Target meaning | Current architectural interpretation |
| --- | --- | --- |
| Context-only | Project semantic state into ordinary input/context; reconstruct through prefill. | Compatibility/fallback strategy on existing context execution; generic provenance-bound lowering is OPEN. Not the ultimate architecture target. |
| Persistent prefix / sequence state | Retain/reuse model-produced context-derived state beyond one prompt construction. | Existing physical foundation; not generic cognitive-state semantics. |
| Latent Read | Consume a persistent model-native bank independently of normal token context. | OPEN strategic target. |
| Latent Read/Write | Consume and produce updates to persistent model-native state. | OPEN strategic target. |
| Native State | Trained/designed architecture treats persistent state as a first-class computational stream. | OPEN strategic target and research horizon; no current model-support claim. |

Multi-timescale pressure may distinguish immediate/local, task, Case/big-picture
and slow/stable retention. These are external representational pressures, not
YVEX semantic-memory categories, four prescribed tensors or one injection rule.

#### Identity and replacement target

A computational realization needs enough identity to determine whether it can
be reused, resumed, compared, invalidated or rebuilt in an exact execution context:

| Identity requirement | Purpose |
| --- | --- |
| Semantic source identity/digest | Recover the authoritative source of the derived realization. |
| Exact model artifact and runtime binding / compatible deployment | Reject incompatible execution contexts. |
| State architecture/layout and realization profile | Bind geometry and model-specific realization constraints. |
| Compiler/lowering identity and precision/physical representation | Distinguish different computations from the same semantic input. |
| Generation/update and parent computational state/checkpoint | Recover update lineage and rollback ancestry. |
| Compatibility / invalidation posture | Determine valid reuse versus mandatory reconstruction. |

These are identity requirements, not a serialization design or one mandatory
giant hash. **Semantic `S_t` must survive model replacement as authority;
computational `M_t` need not.** A new model, incompatible state architecture or
profile may invalidate `M_t`; YVEX should recompile, reinitialize or reconstruct
it from authoritative semantic state. Rebuildability prevents derived state
from becoming an accidental second semantic authority.

#### C / N / S and P

| Program | Responsibility | Explicit non-ownership |
| --- | --- | --- |
| C — Model Language & Compiler | Describe external state inputs/outputs, read/update operations, stream topology, banks, gating and state-consuming/producing blocks. | Residency, paging and semantic authority. |
| N — Native Cognitive State | Advertise capability; accept state ingress; compile a model/profile-specific realization; define cognitive read/update contracts, incremental lowering, provenance, compatibility and evaluation. | Case meaning, canonical semantic facts and physical allocation/scheduling mechanisms. |
| S — Sequence Runtime | Session ownership; candidate/committed transactions; paging, COW, sharing, residency/movement/accounting, checkpoints, rollback, scheduling and physical lifetime. | Whether a bank means memory, task state or another cognitive role; family update equations. |
| P — Physical Model Compiler | Model/package quantization, representation/layout and hardware/workload-aware physical recipe search. | Semantic-state ingress or cognitive-state realization meaning. |

Conceptually **C → N → S → execution**: describe model meaning, compile the
realization, execute its physical lifecycle. This is not a prescribed source
directory layering. P and N may later share physical mechanisms where real
evidence warrants it; their different subjects are not collapsed now.

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
| Verified adapter source | 🔴 OPEN | Model trust exists, not an admitted generic adapter lifecycle. | Exact adapter provenance and immutable inventory. | D / C | [Source contract][storage]; [family integration][families] |
| Base-model compatibility | 🔴 OPEN | Model binding does not establish adapter compatibility. | Validate base identity, roles and numerical constraints before specialization. | D | [Artifact contract][artifacts] |
| Adapter representation | 🔴 OPEN | No generic adapter transformation/physical recipe contract. | Exact source-to-physical adapter identity and admission. | D / P | [Compilation][compilation] |
| Dynamic attach/detach | 🔴 OPEN | Engine generations exist; dynamic adapter composition is unqualified. | Safe changes with exact new execution identity and retirement. | D / S | [Runtime contract][runtime-contract] |
| Multiple resident adapters | 🔴 OPEN | Multiple engines do not imply multiple admitted adapters in an engine. | Coexistence, isolation, resource admission and cleanup. | D / S | [Runtime][runtime] |
| Multi-LoRA batching | ⚪ LATER | Deferred until adapter identity and base lifecycle are established. | Batch compatible compositions without identity leakage. | D / S | [Runtime batching boundary][runtime] |
| LoRA on MoE | ⚪ LATER | No generic adapter semantics to extend yet. | Sparse-role-compatible composition after the base contract. | D / C | [Family integration][families] |
| Multimodal component adapters | ⚪ LATER | Outside the present composition horizon. | Adapters on admitted component interfaces with exact lineage. | D / M | [Family integration][families] |
| Generic component graph | 🟡 PARTIAL | Target/draft and MiniMax schedules use shared owners. | Reusable encoder/decoder/tower/codec composition beyond bounded verticals. | D / C | [Family integration][families]; [MiniMax][minimax] |

Target adapter lifecycle: source → verified adapter → base compatibility →
admitted transformation → physical representation → deployment specialization
→ exact execution identity. **Base + adapter must not silently retain the same
engine identity.** This is a future contract, not present dynamic support.

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
| N | Native Cognitive State | Compile, retain, expose, version and evolve persistent model-native computational state independently of token context. | State ingress; realization; State Read/Update; cognitive identity/evaluation | 🔴 OPEN |
| G | Generation Control | Generalize sampling, constraints and speculation. | Samplers; logprobs; grammar; proposals | 🟡 PARTIAL |
| D | Dynamic Composition | Admit adapters and reusable component graphs. | Base compatibility; adapters; component lifecycle | 🟡 PARTIAL |
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

**Does not own:** family mathematics, imagined consumer policy or release promotion.

### C — Model Language & Compiler

**Purpose:** seal model meaning before runtime execution.

**Established foundation:** family interpretation, tensor roles, Transformation IR and authenticated package plans.

**Open maturity boundaries:** SSM-only topology, normalization/tokenizer authority, broader operators/state/output heads; target persistent-state inputs/outputs, read/update topology, banks and gating.

**Current pressure:** A01's source-only barrier; later spectrum families test different assumptions.

**Material advance:** another computational shape compiles through shared mechanisms without backend reconstruction.

**Does not own:** allocation/residency lifetimes, semantic-state authority, serving policy or application task selection.

### P — Physical Model Compiler

**Purpose:** source plus workload/hardware/quality constraints become a measured reproducible artifact.

**Established foundation:** semantic projection, Transformation IR, physical policy/variant, GGUF writer, admission and measurement.

**Open maturity boundaries:** sensitivity/calibration, feasibility filtering, candidate builds, Pareto selection and recipe evidence.

**Current pressure:** GB10 working-set/quality tradeoffs and distinct tensor numerical obligations.

**Material advance:** reproducible constrained search feeds deterministic construction and independent final qualification.

**Does not own:** model suitability, task selection, runtime residency, N's cognitive-state realization meaning or the release gate itself.

### S — Sequence Runtime

**Purpose:** progress, isolation and resource truth across sequence lifetimes.

**Established foundation:** host, engines, leases, typed transactions, cooperative work and retained prefixes.

**Open maturity boundaries:** cross-provider paging, inflight scheduling, prompt fairness, prefix caching and long-context evidence.

**Current pressure:** recurrent versus attention geometry; actual concurrent workloads, not configured width; future N state must reuse typed physical lifecycle rather than another runtime.

**Material advance:** shared mechanisms qualified against distinct providers and resource-failure paths.

**Does not own:** cognitive meaning, family update equations, semantic model selection or unmeasured eviction policy.

### N — Native Cognitive State

**Purpose:** compile, retain, expose, version and evolve persistent model-native
computational cognitive state independently of ordinary token-context persistence.

**Existing physical foundations:** S/R own typed transactions, limited checkpoint
identity, stale-generation rejection and state-resource accounting. These support
future realization but do not implement N; program state remains OPEN.

**Open cognitive-state boundaries:** ingress and capability advertisement;
model-state compiler/profile; cognitive identity, State Read/Update, incremental
lowering, compatibility/recompilation, provenance and computational evaluation.

**Current architecture pressure:** YAI semantic authority must survive model
replacement; A01 rejects state-equals-KV assumptions. Refoundation prepares C/S/N
boundaries, not a cognitive-state runtime or an A01 evidence promotion.

**Material advance:** a real model/reference demonstrates one declared realization
depth with ingress, identity, negative/lifecycle and useful-state evidence. Read,
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

**Open maturity boundaries:** adapter trust, base compatibility, representation, attach/detach and resource-safe coexistence.

**Current pressure:** heterogeneous component models; adapters remain an unscheduled generic gap.

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

**Open maturity boundaries:** upstream conformance, independent full-model quality, release benchmark and release qualification; future N retention/interference evaluation.

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
distinguishes Qwen's physical forward/output runner and the shared output-head
cutover from the remaining DeepSeek/DSpark and MiniMax computational migration.
Exact Qwen artifact-backed preservation is a separate evidence obligation:
an absent exact asset cannot earn whole-model qualification, but does not alone
prevent architectural completion. Mamba remains representability evidence.
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
| Near/Mid | Qualify progressively deeper computational-state realization under real model/research pressure | N / C / S | Provenance-bound context lowering, persistent realization, State Read and State Update earn separate evidence stages. |
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
| YAI StateFrame/StateDelta runtime integration | false | Target names are not implemented protocol objects. |
| Cognitive-state retention/interference qualification | false | No declared qualified computational cognitive-state evaluation. |
| Native-state model support | false | Strategic adoption is not architecture admission or execution. |

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
