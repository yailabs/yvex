# YVEX Roadmap

Status: living public project control

## At a Glance / Current Snapshot

| Axis | Current truth |
| --- | --- |
| Project target | Native, harness-independent model compilation and execution substrate. |
| Active engineering boundary | `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` — common capacity, resources and lifecycle are implemented; DeepSeek CPU/CUDA numerical divergence and intermittent non-finite CUDA execution withhold closure. |
| Latest generic architecture boundary | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` COMPLETE and independently qualified at its claimed scope: current admitted Qwen, DeepSeek/DSpark and MiniMax computation is compiler-owned; runtime consumes authenticated program/schedule truth. The `.0` producer-owned device-result lifetime remains preserved. |
| Architecture Spectrum | A01 PARTIAL / exact artifact-executable; A02–A11 PLANNED. No next vertical is active. |
| Adopted state architecture target | N.B1 — Slow-Update Dual-Stream is the official primary research target: primary R plus persistent Experiential Computational State E; unfinished deliberation L remains distinct. YAI owns semantic authority; YVEX owns computational realization. Program N and B1 post-training remain OPEN/unscheduled, not implemented. |
| Adopted decision architecture target | YAI owns the semantic Decision Plane; YVEX owns computational Decision Readout and the future broader Decision Core. Internal schema v1 is qualified on exact Mamba2 CPU recurrent and Qwen CUDA hybrid state; scores acquire no semantic authority or canonical admission. |
| Most important structural gap | Two materially distinct state classes now share one readout, capacity policy, execution-profile policy and committed-source-state owner. DeepSeek CPU/CUDA output diverges beyond the declared internal numerical gate, and intermittent CUDA non-finite execution remains unresolved. Universal readout/model breadth, independent upstream whole-model conformance and production performance remain unearned. Observed branch-allocation maxima are not device/process peak measurements. |
| Executable foundation | DeepSeek source-to-hosted text and speculation; exact Qwen3.8-27B BF16 target-only CUDA tokenizer/forward/full-logits plus common hybrid-prefix and finite-candidate readout execution; bounded MiniMax composite media; exact Mamba2 pure-SSM CPU artifact execution plus bounded zero-generation finite-candidate readout. Evidence depths differ. |
| External execution characterization | Capacity-compatible 12,055-token Golden still fails prefill: 11,694 tokens in 613.70 s, zero generated, then producer HTTP 504. Retained as a measured limitation, not a refoundation .1 closure gate; no downstream-safe claim. |
| v0.1 target | DeepSeek text on admitted GB10; no physical variant is yet release-qualified. |
| Behavior evaluation | BLOCKED / not ready. |
| Full-model benchmark | NOT MEASURED at release scope; repeated bounded characterization is not that benchmark. |
| Release qualification | BLOCKED. |
| Current branch | `models2`; branch epochs coordinate integration, not model ownership. |
| Next decision point | Reconcile the completed bounded Qwen readout breadth with the still-open DeepSeek CUDA numerical gate in common runtime qualification. Do not automatically select calibration, public/YAI authority, Program N, A11, another Spectrum vertical or GB10 optimization. |

<!-- maturity-counts:start -->
<!-- Generated from System Maturity by tests/documentation_architecture.py. -->
| Maturity state | Meaning | Current count |
| --- | --- | ---: |
| 🟢 ESTABLISHED | Generic owner and claimed boundary implemented and qualified at the stated scope. | 32 |
| 🟡 PARTIAL | Real foundation; genericity, breadth, portability, performance or evidence incomplete. | 43 |
| 🔴 OPEN | Generic capability absent or not yet claimable. | 59 |
| ⚪ LATER | Intentionally outside the current maturity horizon. | 14 |
<!-- maturity-counts:end -->

Counts describe the rows below, **not a percentage of project completion**.
A narrow established mechanism does not make its entire program established.

Navigate: [maturity](#system-maturity) · [Decision Readout target](#decision-readout-decision-core-target) · [B1 target](#nb1-slow-update-dual-stream) ·
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
| Supervised source acquisition lifecycle | 🟢 ESTABLISHED | Source-owned operation v1 binds immutable target, generation and authenticated supervisor/provider identities. A detached acquisition supervisor survives client/terminal loss; lifecycle, health and non-overlapping nullable progress feed TTY/log/status-v2 projections. Stop/resume, retry, stall, crash/reboot reconciliation, PID reuse and legacy-v1 refusal are qualified; real pinned HF acquisition completed and reopened without weakening source verification. | Preserve operation/source separation and fail-closed process/lock identity as providers broaden; selected-domain in-flight bytes and retry facts remain unknown when an adapter lacks a stable machine signal. | R / X / Q | [Source contract][storage]; [command architecture][commands]; [QA authority][qa] |
| Logical model identity | 🟢 ESTABLISHED | Source-declared relations separate logical model, original selector, representation, artifact, deployment, working set and engine; catalog family exceptions removed. | Extend explicit relations without family/name inference or silent ambiguity. | R / C | [Lifecycle][lifecycle]; [catalog tests][catalog-tests] |
| Semantic Model IR | 🟡 PARTIAL | Typed programs, state/effect verification, current Qwen forward execution, DeepSeek heterogeneous schedule ownership, MiniMax component programs and Mamba2 pure-SSM CPU execution use common compiler levels. Broader architecture language remains incomplete. | Express heterogeneous R/E values, state-consuming/producing blocks, versions, multi-result computation and trainable roles without family runtimes or compulsory Transformer structure; add token-derived indexed parameter access and shared cross-layer state dependencies. | C | [Compilation][compilation]; [Mamba2 boundary][mamba] |
| Operator composition / graph language | 🟡 PARTIAL | Canonical operator graphs and physical SSA own current admitted composition; binding v17/model-plan v8 authenticate the schedule consumed by runtime rather than preserving a family execution universe. Binding v17 additionally authenticates program-owned absence of attention. | Compile dual-stream R/E augmentation, cross-state interaction, gates/merge and update barriers through common model semantics; no family cognitive runtime. Also express phase-asymmetric programs and explicit state/result boundaries. | C / D | [Family boundary][families]; [decoder plan][decoder-plan] |
| Transformation IR | 🟢 ESTABLISHED | Typed, ordered, artifact-neutral transformations precede payload materialization. | Remain the unique transformation authority as representations expand. | C / P | [Compilation][compilation] |
| Physical policy | 🟡 PARTIAL | Per-terminal dtype/qtype, layout and alignment decisions exist for admitted recipes. | Broaden compiler-owned physical decisions without downstream reconstruction, including conditional tables, file-backed row representations and quantized runtime state. | P | [Compilation][compilation] |
| Physical Execution IR | 🟢 ESTABLISHED | Package physical records are sealed separately from deployment implementation choices. | Preserve authenticated consumption as semantic operations broaden. | C / P / R | [Artifact contract][artifacts]; [compilation][compilation] |
| Artifact emission | 🟢 ESTABLISHED | Transactional GGUF construction publishes admitted package representations. | Preserve deterministic construction and rollback across broader representations. | P | [Artifact contract][artifacts]; [writer tests][writer-tests] |
| Artifact admission | 🟢 ESTABLISHED | Integrity, roles, identities and binding constraints fail closed. | Extend coverage without weakening integrity or mandatory semantic checks. | P / R | [Artifact contract][artifacts]; [integrity tests][integrity-tests] |
| Runtime binding | 🟢 ESTABLISHED | Authenticated package and execution truth, including the canonical operator schedule, is reopened without family import or runtime compiler reconstruction. | Preserve package meaning versus runtime specialization. | R / P | [Runtime contract][runtime-contract] |
| Existing quantized representation import | 🟡 PARTIAL | Multiple GGUF qtypes and low-precision execution paths are admitted. | Broader format coverage without per-format runtime redesign. | P | [Compilation][compilation]; [DeepSeek][deepseek] |
| Quantization synthesis | 🟡 PARTIAL | Mixed per-tensor policy and bounded calibration-informed recipes can be constructed. | Generic sensitivity/calibration-driven synthesis with reproducible decision provenance. | P | [GB10 targets][gb10]; [compilation][compilation] |
| Physical Model Compiler search | 🔴 OPEN | Deterministic lower layers exist; recipe exploration remains bounded/manual engineering. | Hardware/workload/quality-aware search and reproducible Pareto selection. | P | [GB10 targets][gb10]; [compilation][compilation] |
| Resource/state-root effect dependencies | 🔴 OPEN | Current lowering retains a global serial effect chain; explicit SSA/state foundations do not qualify independent state-root scheduling. | Order real dependencies, conflicting roots and explicit ordered semantics; preserve legal DAG branches for Target/Schedule choice. | C / S | [B1 compiler target](#compiler-execution-dag-and-admitted-backend-target); [current lowering][compilation] |
| N-gram / token-derived addressing semantics | 🔴 OPEN | Exact tokenizer and source identities exist; generic n-gram window/key/address semantics are not admitted. | Seal tokenizer-bound addressing, compressed mappings, boundaries and hash/table rules; qualify positive and malformed-address vectors. | C / R | [Addressing target](#conditional-sparse-parameter-memory); [source contract][storage] |
| Conditional sparse parameter memory | 🔴 OPEN | Dense parameters and routed experts do not establish indexed execution over huge immutable parameter tables. | Compile dynamic sparse row populations, physical table representations and bounded storage/cache execution with reference and refusal evidence. | C / P / S | [Conditional-memory target](#conditional-sparse-parameter-memory); [compilation][compilation] |

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
| Decision readout | Shared/current computation, finite candidate representations, score-producing readout, optional calibration or learned head, typed multi-result output and exact backbone/readout/head identities | Model-independent finite-option inference without making generation or semantic authority the execution contract |
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
| Token-derived addressing and conditional parameter reads | Exact window/key construction, indexed immutable reads, sparse row populations, combination/projection and gating | Bind table/tokenizer identity without a universal Engram primitive or whole-table dense operand |
| Phase-asymmetric program composition | Components with different computation, state dependencies and parameter/workspace liveness | Express causal encoder → state projection → decoder through common compilation |
| Shared cross-layer state | Explicit producer/source relationships for state and derived selections | Reuse a produced state or index population without duplicating ownership per layer |

The target model signature can grow from input → model → output to
`(primary input, persistent computational state)` → model graph →
`(output, updated persistent computational state)`. This is target-language
breadth, not a claim that current models expose that interface.

#### Physical Model Compiler

**Quantization ⊂ representation synthesis ⊂ physical model compilation.**
Program P is not a GGUF picker or merely a quantizer. It should turn immutable
source plus an execution objective into a reproducible physical recipe whose
quality, resource use and performance are measured together.

That future recipe coordinates three coupled but separately owned decision
spaces: parameter/package representation, physical computational realization
and target choice, and deployment/resource realization. Program P may search
or coordinate candidates across those spaces, but their identities remain
distinct. It does not absorb model semantics, artifact admission, runtime
residency or backend-local equivalent implementation choices.

The implemented deterministic parameter/package lane is verified source →
family semantic projection → Transformation IR → transform binding/artifact
lowering → quant plan/physical variant → artifact construction → admission and
materialization → PEIR package truth. It joins the separately lowered symbolic
computational program to construct a parameter-bound physical program before
the compiled model plan and runtime binding.
Semantic breadth remains partial. The missing strategic layer is coordinated
search/optimization across the separately owned physical decision spaces.

A future learned readout/probe/head is a first-class physical model object, not
an external classifier sidecar. Program P must bind its exact backbone/source
and parameter identity, dtype/qtype, layout, artifact provenance, physical
representation, compatibility, runtime placement and qualification evidence.
The selected V0 Decision Readout introduces no trainable weights and therefore
does not depend on this future learned-head lane.

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
Search may propose legal representations, admitted computational target choices
and deployment/resource candidates. Deterministic owners must still emit,
identify, admit and execute each selected result at its own boundary. Search may
not invent kernels, change source meaning, redefine artifact truth, take runtime
residency ownership or move backend-local placement authority into family
interpretation.

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
| Conditional parameter tables | Row-addressable dtype/qtype, block geometry, scales, packing, alignment, partitions/shards, file-backed or mmap-compatible layout, device gathers and cacheable rows |
| Executable resource classes | Dense weights, routed experts and conditional tables may have different residency windows proven by program-phase dependencies; resident and non-resident representations remain distinct |
| Runtime-state representation | Semantic state geometry independent of physical dtype/qtype, packing/scales/layout and backend implementation |

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
| Resource accounting | 🟢 ESTABLISHED | Mapped, prepared, allocated, addressable and observed current/peak facts remain distinct. | Preserve known/unknown resource truth under dynamic placement; future sparse tables require separate logical/backing/resident/moved bytes and row/cache populations. | S / R | [Resource truth][runtime]; [resource tests][resource-tests] |
| Resource admission | 🟡 PARTIAL | Live capacity and backend facts constrain bounded engine admission. | Broader placement/concurrency admission with measured reserves and negative evidence. | S | [Runtime][runtime]; [resource tests][resource-tests] |
| Automatic residency policy | 🔴 OPEN | Explicit load/unload and ensure-active are mechanisms, not a generic eviction policy. | Bounded retention/placement/eviction from authoritative resource facts across dense parameters, experts, conditional tables, state and derived caches. | S | [Storage contract][storage] |
| Phase-aware executable resource lifetime / residency | 🔴 OPEN | Engine resource accounting, explicit residency and bounded resource dependencies exist; generic compiler-proven immutable parameter residency windows do not. | Compile phase liveness for parameters, derived state and workspaces; physical planning/runtime retain, prefetch, evict or rematerialize under that proof and measured resource policy. | C / P / S | [Phase target](#phase-liveness-and-quantized-runtime-state); [runtime contract][runtime-contract] |
| Quantized runtime-state representation | 🔴 OPEN | Typed attention/recurrent/convolution state and physical weight representations exist; generic compiler-owned low-bit mutable/runtime state is not qualified. | Separate semantic state geometry from dtype/qtype, scales, packing/layout and backend representation, with state numerical and lifecycle evidence. | C / P / S | [State representation target](#phase-liveness-and-quantized-runtime-state); [runtime][runtime] |
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

#### Conditional sparse parameter memory

This OPEN target is an immutable model-parameter domain whose logical table can
be enormous while execution dynamically selects only a small subset of rows
from input/state-derived addresses. Engram in [A11](#a11-deepseek-v41-flash-target-pressure)
is its first selected Spectrum pressure, not a new generic runtime or Program N
realization. Selection as a research pressure does not schedule implementation.

Three n-gram concepts have independent meaning and owners:

| Concept | Computational path | Ownership |
| --- | --- | --- |
| Lexical n-gram addressing | Exact tokenizer → token IDs → ordered windows → qualified key/hash/address policy | Source facts under R; model/compiler semantics under C |
| N-gram-addressed parameter memory | Key → indexed immutable table read → selected rows → model projection/interaction/gate | C semantics, P representation, S resources, backend execution |
| N-gram generation proposal | Committed token history → suffix/n-gram heuristic → proposed tokens → generic speculative verification | G; independent of model-internal conditional memory |

Future addressing identity binds the exact tokenizer/source revision, token-ID
domain, compressed-token mapping if present, n-gram sizes, history padding and
boundary rules, hash/address algorithm and version, cardinality, head/table
partitioning, collision/search semantics, masking and image/non-text participation.
Source projection/import seals those facts; runtime does not reconstruct them
from strings. Changing the tokenizer can invalidate table compatibility even
when backbone geometry matches. This target freezes no public n-gram ABI.

Semantic Model IR must eventually express deterministic indexed parameter reads,
token/window-derived keys, sparse row populations, row combination/projection,
gating/residual interaction, explicit table identity and tokenizer-derived
dependencies. It must not treat a huge table as one dense execution operand
when only selected rows are needed. Unknown or malformed addressing semantics
fail closed. These are semantic requirements, not frozen operation names or an
`Engram` universal primitive.

The physical model/state classes remain separate:

| Class | Selection and lifetime meaning |
| --- | --- |
| Dense immutable parameters | Ordinary model computation; often resident |
| Routed MoE experts | Immutable parameters selected by learned routing |
| Conditional sparse parameter memory | Immutable rows selected by token/feature-derived addressing |
| Local mutable sequence state | Attention KV, recurrent, convolution and decoder state |
| Experiential Computational State E | Future mutable/versioned, cross-request, StateProfile-bound computational experience |
| Latent Deliberation State L | Unfinished computation under its execution compatibility contract |

These classes may share mechanisms, not semantic ownership or lifecycle by
definition. There is no universal `memory` object. Engram is learned parameter
memory, part of exact weights, and unchanged by ordinary inference. It is not
application retrieval. E instead has future working-state-derived realization,
Lower/Reconcile and model State Update; L preserves unfinished work.
**Engram != E; Engram != L; Engram lookup != Recall or YAI retrieval.**
Engram n-gram parameter lookup != State Read; an Engram row cache != E residency;
replacing an Engram table or hash policy != model State Update. N does not own Engram, and
Qwen-first B1 research/post-training remains independent.

P's future table representation includes row dtype/qtype and block geometry,
scales, packing/alignment, file-backed or mmap-compatible layout, partitioning,
device gather representation, cacheable rows and admitted backend compatibility.
This is broader than ordinary tensor quantization. Logical table size does not
require full residency or the same policy as dense weights.

A possible physical hierarchy is complete immutable backing → file/NVMe/mapping
→ OS page cache or bounded host residency → YVEX hot-row/hot-page cache → optional
device row cache → indexed backend execution. This does not require every level,
SSD I/O on every lookup, or reliance on the OS cache alone. Actual policy needs
measurement; source/artifact integrity still governs all reopened backing.

Future resource evidence distinguishes logical table bytes, mapped/backing bytes,
resident host/device bytes, cache capacity, cached row/page populations, unique
rows requested, logical row bytes requested, physical bytes read/moved, hits,
misses, evictions, prefetches and current/peak residency. Mapped, prepared,
allocated, addressable, resident, moved and observed are different facts, not one
"memory used" value. Model mapping, dense prepared weights, routed experts,
conditional tables, state backing, workspace and derived caches need separate
accounting even where storage mechanisms are shared.

Future CPU/CUDA execution may require key/hash preparation, batched indexed
gather, quantized row decode, head/column combination, projection, gating and
residual addition. Independently testable reference operations and admitted
backend numerics precede any gather/dequant/combine/project/gate fusion; fusion
cannot change source mathematics. No such kernel is implemented by this target.

#### Phase liveness and quantized runtime state

Compiler-visible executable-resource lifetime should eventually influence
physical residency. C proves program/component dependencies and liveness for
immutable parameter groups, derived state and workspaces; P specializes legal
representations and residency opportunities; S applies budgets, movement and
cache policy. Retain, prefetch, stream, evict and rematerialize decisions derive
from compiled truth and measured hardware facts, never a family-name heuristic.
Current engine accounting and bounded dependencies do not establish this target.

CED exposes encoder/prefill parameters, hot experts, Engram accesses and workspace,
then an explicit state/result boundary, then decoder/generation parameters, hot
experts, projected global state/KV, local/SWA state and workspace. The phases have
different computation, state production/consumption and resource requirements.
Resources needed again in a later phase must remain available or be restored
before reuse; encoder/decoder membership alone is not an eviction proof.
This is stronger than merely recognizing a T5-like encoder-decoder or applying
model-wide SSD streaming. No equal byte populations, exact half-model eviction,
fit, speedup or throughput follows from the layer split. Program/Execution IR
preserves dependencies and legal independence; Target/Schedule chooses physical
execution and synchronization, without mandatory CUDA concurrency.

Quantized runtime state is a separate target from weight quantization. Semantic
state geometry need not share activation precision; P must eventually bind
state dtype/qtype, packing/scales/layout and backend representation independently.
V4.1 FP4 KV pressures that contract; future long-context, recurrent/SSM and B1 E
representations may also use it. This makes no FP4 choice for B1 and qualifies
neither low-bit state nor a new E implementation.

For these targets R/Source owns immutable source, tokenizer/addressing facts,
table identity and checkpoint/family relations. C owns CED programs, shared
cross-layer state, addressing/indexed reads, source interaction/mHC semantics
and phase liveness. P owns table/state representation and backend compatibility.
S owns residency, row/page movement, expert/conditional caches, phase-transition
resources, state lifetime and accounting; S does not own n-gram meaning. G owns
only the separate generation proposals. Q requires official/reference conformance,
address/hash vectors, positive/refusal cases, cache-policy numerical invariance,
phase-residency equivalence, low-bit state numerics and the complete terminal
vertical before broader behavior/performance claims. Backend owners execute
admitted primitives. N remains the independent B1/E owner.

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

Refoundation .1 established universal typed computational authority, explicit
state/effects, a common compiler/runtime boundary and family-independent
execution ownership at the currently admitted scope. Its qualification wave
now owns broad replay; B1 added no operators, implementation work or closure
gates to that cutover.

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
An n-gram proposal strategy uses committed output/history to propose tokens for
verification under G. It is independent of lexical addressing and model-internal
n-gram parameter memory: speculation breadth does not establish Engram support.

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
| Generic component graph | 🟡 PARTIAL | Target/draft and MiniMax schedules use shared owners. | Reusable architecture-augmented R/E composition and encoder/decoder/tower/codec graphs beyond bounded current verticals; current component schedules do not implement B1. CED additionally requires explicit phase/state interfaces and resource liveness. | D / C | [Family integration][families]; [MiniMax][minimax] |
| Architecture-augmented model composition | 🔴 OPEN | Current component composition does not admit B1 graph surgery or a trained R/E augmentation. | Immutable base plus augmentation architecture/weights and StateProfile receives its own compatible source, artifact, deployment and execution identities. | D / C | [Qwen adaptation target](#qwen-b1-model-adaptation-and-post-training-target) |
| Post-training provenance / intake | 🔴 OPEN | Source trust does not implement generic dataset/recipe/checkpoint-lineage intake for externally post-trained compositions. | Bind trainable manifest, dataset/recipe/configuration, checkpoint lineage and trained source before import and independent qualification. | R / D / Q | [Provenance target](#provenance-and-inference-composition-pipeline); [Source contract][storage] |

Target adapter lifecycle: source → verified adapter → base compatibility →
admitted transformation → physical representation → deployment specialization
→ exact execution identity. **Base + adapter must not silently retain the same
engine identity.** This is a future contract, not present dynamic support.

A future trained decision probe/head follows the same ownership: exact frozen
backbone plus exact head/probe identity, compatibility, executable composition
identity and rollback/invalidation semantics. It may not remain an arbitrary
Python classifier, `.pt` sidecar or model-name-specific hook. The V0
zero-decode experiment has no new weights and does not exercise this contract.

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
| Decision / option readout | 🟡 PARTIAL | Internal readout schema v1 scores exact token candidates over one captured common prefix with raw log-likelihood, separate mean log-probability and an uncalibrated relative distribution. Exact Mamba2 CPU recurrent and Qwen CUDA hybrid artifacts qualify shared-state isolation, independent full-prefix replay, order invariance, cancellation/retry, zero sampling and zero generated tokens. Common runtime owners now derive/re-admit exact execution profiles and compose every active committed target/draft attention and recurrent source-session domain for the before/after invariant; Qwen breadth uses the same schema/score owner; common capacity, physical-resource projection and retained failed-cleanup ownership remove runner-specific assumptions. | Qualify compatible shared-prefix readout across broader admitted model/state classes and truthful capability advertisement without converting internal token scoring into semantic authority, calibration or a public application ABI. | O / C / Q | [Decision Readout target](#decision-readout-decision-core-target); [runtime contract][runtime-contract] |
| Classify | 🔴 OPEN | No admitted generic classification-head runner. | Label/head semantics and typed result independent of commands. | O / C | [Family integration][families] |
| Rerank | 🔴 OPEN | No generic pair/cross-encoder request and score path. | Pair construction, score semantics and reproducible ranking evidence. | O | [Runtime contract][runtime-contract] |
| Reward | 🔴 OPEN | No general sequence/token reward output. | Source-defined reward heads and token/sequence identity. | O / C | [Family integration][families] |
| Iterative media generation | 🟡 PARTIAL | Generic latent lifecycle supports bounded MiniMax iteration/publication. | Reusable non-autoregressive runner across admitted schedules and outputs. | O / M | [Latent runner][latent]; [MiniMax limits][minimax] |

Execution runner semantics != frontend command. A vector or score must be a
typed runtime result, not text parsed by an embedding/scoring CLI wrapper.

#### Decision Readout / Decision Core target

This target adopts a strict cross-project ownership boundary. The **Decision
Plane** is YAI's semantic layer: it owns Case/task meaning, qualified working
state, candidate meaning and origin, disclosure and authority context, semantic
interpretation and canonical admission. **Decision Readout** is YVEX's
computational finite-candidate primitive. **Decision Core** names a future more
general learned computational realization. A model/readout result grants no
authority, permission, effect or canonical Decision.

```text
current/shared model computation ─┐
                                  ├── computational readout ──► typed scores[N]
finite explicit candidates ───────┘                          + evidence
```

The preferred architecture is **one resident backbone → multiple qualified
computational readouts**. A future specialized decision model remains allowed,
but duplicating a full resident decision LLM is not part of the generic
contract. `DECISION_READOUT`, `SCORE`/`OPTION_SCORE`, `GENERATE`,
`EMBED`/`REPRESENT`, `STATE_READ` and `STATE_UPDATE` are target vocabulary, not
frozen public enums. Bare `DECIDE` is avoided because computation does not own
semantic authority.

| Research level | Target realization | Explicit boundary |
| --- | --- | --- |
| V0 | Already-loaded admitted backbone; shared/prefetched prefix or current computational state; finite candidates; direct option scoring | Zero generated tokens, no autoregressive sampling loop, no new trainable weights and no second full backbone. Known candidates may still require real teacher-forced forwards. |
| V1 | V0 plus a separately qualified small calibration transform such as temperature/bias | Relative scores are not called calibrated probability without independent evidence. |
| V2 | Frozen backbone plus a tiny learned linear probe/decision head over a qualified hidden representation | Head source, parameters, representation and backbone compatibility acquire exact identities. |
| V3 | General learned Decision Core over computational state and arbitrary finite candidates | Still computational and non-authoritative. |
| V4 | `DecisionCore(R, E, query, candidates)` | R/E-aware readout is a possible future read-only State Read consumer, not E or State Update. |
| V5 | Native System-One realization and/or dedicated specialized decision model | Requires independent usefulness, calibration and systems evidence; no current claim. |

These levels are an evidence/research ladder, not six scheduled milestones.
V0 selected Mamba-Codestral-7B-v0.1 only after live artifact archaeology showed
it was the smallest exact READY/launchable model with tokenizer, output logits
and common recurrent-prefix ownership. That qualified reference does not make
Decision Readout family-specific or schedule V1.

V0 explicitly does not implement calibration, a learned probe/head, a dedicated
decision model, R/E or StateProfile, a YAI contract, semantic Decision
admission, an adaptive cognitive router, System-One claims or a performance
advantage. It does not add a second full resident backbone.

Score meanings remain explicit and non-interchangeable:

| Quantity | Meaning / nonclaim |
| --- | --- |
| Raw logits | Unnormalized model output in an exact token/head domain. |
| Token log-probability | Normalized likelihood of a token under an exact conditioning history. |
| Candidate log-likelihood | Declared aggregation over the candidate's teacher-forced tokens. |
| Length-normalized candidate score | A separately declared normalization of candidate likelihood, not raw likelihood. |
| Relative candidate distribution | Normalization over the disclosed finite candidate population only. |
| Calibrated probability | Requires an explicit calibration method, artifact/identity where applicable and independent calibration evidence. |
| Confidence / uncertainty evidence | A separately defined qualified statistic; neither a softmax value nor semantic authority by default. |

In particular, `softmax(scores) != calibration`. A future System-One claim
cannot be inherited from ordinary LLM scoring or from an external producer's
claim.

The existing programs remain the owners:

| Program | Decision-readout responsibility | Boundary |
| --- | --- | --- |
| O — Output Runners | Primary owner of the typed finite-candidate request/result lifecycle and zero-decode runner semantics. | Does not own candidate meaning, application ranking policy or admission. |
| C — Model Language & Compiler | Shared/current representation, candidate representation, score-producing operations, optional calibration/head semantics, multi-result output and exact computational identities. | Does not freeze a YAI request type or family-specific runtime branch. |
| Q — Qualification | Score definition, state/prefix preservation, zero-generation proof, numerical/calibration evidence and latency/resource measurement. | Selection/calibration data cannot independently qualify the final result. |
| P — Physical Model Compiler | Future learned probe/head dtype/qtype, layout, provenance, representation, compatibility and placement. | V0 has no new trainable weights and does not depend on learned-head infrastructure. |
| D — Dynamic Composition | Future exact backbone + probe/head composition identity, compatibility, rollback and invalidation. | No arbitrary Python classifier, `.pt` sidecar or model-name hook. |
| N — Native Cognitive State | Participates only when a future readout consumes R/E under an exact StateProfile. | V0–V3 do not depend on Program N/B1; Decision Readout is not E, State Update or semantic memory. |

Ordinary model/runtime capability truth must eventually advertise a non-stateful
readout without requiring StateProfile. A future R/E-aware StateProfile may
describe readout mode, required state/profile generation, read site,
probe/head identity, candidate bounds, dtype/layout, batching and resource
envelope; no public StateProfile field or ABI is frozen here. A read-only future
invocation may preserve `E_after == E_before`.

Minimum Sufficient Cognition remains YAI policy: YAI chooses deterministic
logic, Decision Readout, generative reasoning or human/review escalation. YVEX
may expose exact compatibility, score/calibration semantics, candidate bounds,
batching and measured latency, memory, GPU time, compute or energy. It does not
choose the semantic mode. The future evaluation objective is
**time-to-qualified-solution**, comparing ordinary generative control with
adaptive cognition while holding machine, base model, task and tools constant
where possible. Report wall time, generative calls/tokens, prefill work,
readout/tool calls, redundant work, peak memory, GPU time, energy where
available and independently verified correctness; this doctrine earns no
current performance claim.

The external pressure is a real bounded YAI semantic Decision Plane and typed
candidate frontier, but YAI's `CognitiveDecisionRequest v1`,
`CognitiveDecisionDistribution v1` and `CognitiveDecisionFrontier v1` remain
YAI-owned application/domain contracts. YVEX now has one internal computational
Decision Readout producer, but YAI has not selected it as a consumer; a shared
public producer/consumer ABI therefore remains premature. I07 remains
UNSELECTED. Only an actually selected cross-project consumer would trigger
normal BOUNDARY/Interlock evaluation.

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
| BOS/EOS variants | 🟡 PARTIAL | Admitted sources supply policy; Mamba2 now separates tokenizer identity (BOS 1/EOS 2/no PAD) from generation policy while retaining raw metadata conflicts. Broader variant coverage remains incomplete. | Qualify source-authored policies across additional tokenizer/generation combinations before claiming generic breadth. | C / G | [Tokenizer compilation][tokenizer-compile]; [Mamba2][mamba] |
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

**Open maturity boundaries:** broader architecture forms, remaining structural
overhead and independent qualification beyond the completed current-consumer
and supervised-acquisition cutovers.

**Current pressure:** the completed `MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0`
moves long-running provider-operation truth out of the foreground CLI without
duplicating verified-source authority. Decision Readout V0 is complete. Qwen
admission now binds the exact retained BF16 artifact to immutable original
source and published YaiLabs release authority, produces a fresh current v17
binding and qualifies one ordinary target-only CUDA engine without a second
artifact, requantization or family runtime. Retained malformed v16 canonical
records remain refused. The completed common Decision Readout execution-profile
repair now derives and re-admits CPU/CUDA identity and resolution from opened
engine/backend truth. Post-profile project control found and the completed
session-state identity prerequisite repaired one remaining generic correctness
defect before breadth: the readout's current-session observation now covers
every active committed target/draft attention and recurrent domain through one
runtime/session owner. No Qwen candidate execution occurred. Post-boundary
reconciliation, rather than breadth implementation, is now selected. This does
not reopen Refoundation .1, A01 or the completed .0 borrow repair.

**Material advance:** remove a demonstrated ownership defect with negative/lifecycle QA and exact performance replay where relevant.

**Future source/provenance responsibility:** retain exact base and initial augmentation sources, dataset/recipe identities, checkpoint lineage and final trained source through existing Source owners; this does not implement post-training intake.

**Does not own:** family mathematics, imagined consumer policy or release promotion.

**Additional unscheduled A11 target:** Exact V4.1 source/tokenizer identity, addressing/table facts and checkpoint relations remain Source responsibilities; public-source research is not immutable acquisition.

### C — Model Language & Compiler

**Purpose:** seal model meaning before runtime execution.

**Established foundation:** family interpretation, tensor roles,
Transformation IR, native typed modules/functions/regions/SSA, verified
state/effects, canonical execution schedules, physical programs and
authenticated executable bindings consumed by current admitted runtimes.

**Open maturity boundaries:** broader operators, state/output heads and
architecture composition beyond the current admitted consumers. Future R/E
semantics, read/update blocks, StateProfile-compatible entrypoints, root
effects, dependency DAG/barriers and trainable augmentation roles extend the
language; they were not implemented by refoundation .1 or A01.

**Current pressure:** A01 qualifies exact pure-SSM artifact execution at its
PARTIAL scope. Decision Readout V0 composes the existing compiler-owned
forward/output programs with common runtime prefix/state and a typed
multi-candidate result. Qwen admission now authenticates its existing
1,732-instruction/14-slot hybrid forward and two-step output program through a
fresh binding v17 and executes complete logits; the compiler context-envelope
owner was repaired so a decoder correctly owns its nested physical forward
program rather than inventing a second target. No Semantic Model IR, PEIR,
model-plan or binding schema change, readout operation or score-semantic change
was required. Broader readout/state classes remain compiler pressure, not a
reason to manufacture a family operation.

**Material advance:** another computational shape compiles through shared mechanisms without backend reconstruction.

**Does not own:** allocation/residency lifetimes, semantic-state authority, serving policy or application task selection.

**Additional unscheduled A11 target:** CED phase programs, CSA2 shared state, token-derived indexed parameter reads, Single-Pass mHC semantics and compiler-proven phase/resource liveness extend the language target without expanding .1.

### P — Physical Model Compiler

**Purpose:** source plus workload/hardware/quality constraints become measured,
reproducible parameter, computational-target and deployment/resource choices
with separate identities and owners.

**Established foundation:** semantic projection, Transformation IR and binding,
quant plan/physical variant, GGUF writer, admission/materialization, PEIR package
truth, physical computational programs, runtime specialization and measurement.

**Open maturity boundaries:** sensitivity/calibration, feasibility filtering, candidate builds, Pareto selection and recipe evidence; future B1 weight/E precision/layout, derived materializations and target/backend compatibility.

**Current pressure:** GB10 working-set/quality tradeoffs and distinct tensor
numerical obligations. Future learned readout heads must become ordinary
identity-bound physical objects; the selected V0 readout has no new trainable
weights and adds no Program P implementation obligation.

**Material advance:** reproducible constrained search feeds deterministic construction and independent final qualification.

**Does not own:** model semantics or suitability, task selection, artifact
admission truth, runtime residency/lifetimes, backend-local equivalent launch
choices, N's cognitive-state realization meaning or the release gate itself.

**Additional unscheduled A11 target:** Conditional-table row qtype/layout, file-backed and cacheable representations, runtime-state qtype/layout and phase-specific representation choices require admitted backend compatibility.

### S — Sequence Runtime

**Purpose:** progress, isolation and resource truth across sequence lifetimes.

**Established foundation:** host, engines, leases, typed transactions, cooperative work and retained prefixes.

**Open maturity boundaries:** cross-provider paging, inflight scheduling, prompt fairness, prefix caching and long-context evidence; future E generations/leases/transactions and independent L checkpoint/resume through common physical owners.

**Current pressure:** recurrent versus attention geometry; actual concurrent
workloads, not configured width. Qwen admission now qualifies common prefix-v2
capture/attach across 48 recurrent/gated-delta and 16 attention layers. The
generic state owner gained authenticated CUDA device-bank fork/restore and the
prefix owner rebuilds compiler-authored attention capacity before publishing
destination residency; direct versus attached continuation produced all
248,320 logits with `max_abs=0`. This is ordinary state lifecycle evidence, not
a new Qwen prefix type or cross-request prefix cache. The completed Decision
Readout profile repair now reuses this ordinary engine/session capability truth:
Qwen derives its real CUDA build identity plus eager-compatible/degraded
attention/MoE resolution, while false stronger sealed profiles are refused.
Prefix v2 authenticates the captured immutable attention-plus-recurrent
snapshot, while internal committed-session-state observation schema v1 now
composes the mutable target/draft attention and recurrent content identities,
extents and engine/session lineage under the session lifecycle lock. Active
transactions, staged state, stale lineage, invalid providers and inconsistent
hybrid target extents refuse. Decision Readout consumes this owner before and
after branching, so attention-only or recurrent-only mutation cannot false-pass.
Common session resource projection now supplies readout and hosted telemetry.
Readout observes authoritative sequence host/device plus private attention
allocations after attach and each committed token; logical decompositions,
virtual capacity and immutable shared prefix bytes are not counted twice. Future N state likewise must reuse typed physical lifecycle
rather than another runtime.

**Material advance:** shared mechanisms qualified against distinct providers and resource-failure paths.

**Does not own:** cognitive meaning, family update equations, semantic model selection or unmeasured eviction policy.

**Additional unscheduled A11 target:** Conditional-row/page and expert-cache budgets, movement, phase-transition resources, lifetime and truthful accounting follow compiler dependencies; S never owns n-gram meaning.

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
boundaries, not a cognitive-state runtime or an A01 evidence promotion. Decision
Readout V0–V3 is independently researchable and does not depend on N/B1; only a
future R/E-aware Decision Core participates as a possible read-only State Read
consumer.

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

**Terminology boundary:** N-gram/suffix proposals remain generation strategies over committed history, independently of A11. G does not own Engram or token-addressed model parameter memory.

### D — Dynamic Composition

**Purpose:** explicit executable identities for admitted component/adapter combinations.

**Established foundation:** component schedules and target/draft lifetimes.

**Open maturity boundaries:** adapter trust, base compatibility, representation, attach/detach and resource-safe coexistence; architecture-augmented composition, trained B1 State Encoder/read/update weights and provenance-bound external post-training intake.

**Current pressure:** heterogeneous component models and the unscheduled Qwen B1 adaptation target. Q2/PT0 surgery parity precedes Q3/PT1 frozen-backbone training; Q4/PT2 needs evidence. A future learned Decision Readout probe/head must use the same exact composition authority. DeepSeek stays untouched D0. No post-training, learned head or trainer work is selected.

**Material advance:** reusable composition with exact specialization identity and rollback.

**Does not own:** invented compatibility, family numerics or premature multi-LoRA optimization.

### O — Output Runners

**Purpose:** model execution independent of chat.

**Established foundation:** shared autoregressive generation, bounded latent
iteration and one exact internal finite-candidate readout over a common
recurrent prefix.

**Open maturity boundaries:** embedding, pooling, sequence/pair scoring,
finite-candidate Decision Readout breadth beyond the qualified V0 model/state
class, classification, reranking, reward and general iterative results.

**Current pressure:** `DECISION.READOUT.ZERO.DECODE.0` qualified one already-open
Mamba2 backbone, one recurrent prefix, exact token candidates, explicit score
semantics and zero sampling/generated tokens. Qwen admission is complete: its
exact ordinary CUDA engine exposes full logits and common hybrid prefix
capture/attach; Decision Readout v1 now also qualifies finite candidates on Qwen. The completed
`DECISION.READOUT.EXECUTION.PROFILE.0` repair extracts one common runtime-
specialization owner shared by generation and readout. Mamba CPU retains its
portable exact no-attention/no-MoE readout posture, while CPU transformer work
remains compatible/portable; CUDA binds the actual build/kernel identity and derives
attention/MoE/sampling resolution from binding, engine, session and backend
facts. Exact Qwen ordinary execution observed DEVICE_NATIVE plus
COMPATIBLE_DEGRADED attention/MoE and EXACT not-invoked sampling, with no
candidate scoring. Existing internal context-open facts remain sufficient
capability truth; no public field, family switch or second registry was added.
The completed session-state identity prerequisite removed the recurrent-first /
constant-attention placeholder. Decision Readout now observes one common
committed-session identity before and after branching; exact target/draft
attention roles, recurrent state, committed extents and engine/session lineage
are identity-significant, while in-flight or inconsistent state refuses. Qwen
ordinary hybrid observation confirms both target attention and recurrent state
through this owner; the separate breadth lane now consumes it during scoring.
The common runtime capacity owner replaces generation-only page-plan bootstrap;
readout and hosted telemetry share non-overlapping physical-resource projection.
Branch peaks are observed allocation maxima at attach/commit boundaries, not
continuous process/device measurements. V1, YAI integration and universal model breadth are not implicit.
A08 retrieval and A04/A10 non-autoregressive expectations remain separate.

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

**Open maturity boundaries:** SDK consumers, macOS/Windows execution, explicitly
scoped remote security and typed acquisition TTY/log/JSON projections over one
non-CLI lifecycle authority.

**Current pressure:** acquisition presentation must consume typed committed,
in-flight, health and lifecycle facts without scraping provider logs or owning
the transfer; real consumers and missing platform evidence remain separate.

**Material advance:** another consumer/platform shares semantics with qualified cancellation and lifecycle.

**Does not own:** runtime topology, family meaning or editing mechanics inside YVEX.

### Q — Qualification

**Purpose:** promote only evidence earned at the claimed scope.

**Established foundation:** mapped QA, reference components, typed measurement and source-stable receipts.

**Open maturity boundaries:** upstream conformance, independent full-model quality, release benchmark and release qualification; future B1 surgery parity, held-out post-training/state-usefulness evaluation, explicit Q0–Q4/D0 controls and numerical/lifecycle/ablation evidence.

**Current pressure:** Decision Readout V0 now retains an independent long-double
log-sum-exp oracle over real admitted logits, exact full-prefix replay, order and
state-isolation controls, zero-generation counters, sanitizer evidence and
bounded latency/resource characterization. This is readout arithmetic and
lifecycle evidence, not upstream whole-model conformance or a performance
advantage. The completed Qwen admission prerequisite separately proves the
exact immutable source/release manifest, unchanged artifact identity, current
binding admission, tokenizer/full-logits execution and common hybrid-prefix
lifecycle, including stale-v16 refusal. Its direct-versus-attached control
compared 248,320 finite logits exactly (`max_abs=0`, tolerance zero). It does
not qualify Decision Readout breadth, upstream Qwen conformance, behavior or
performance. The completed common profile repair binds the exact current CUDA
build identity, derives Qwen eager-compatible/degraded attention/MoE posture
and refuses stale generation/specialization, foreign bundle, contradictory
class, false stronger resolution and malformed sealing. The exact Mamba CPU V0
control retained independent long-double score error
`3.144468487706128e-15`, zero order/replay difference, zero sampling and zero
generated tokens. Qwen ordinary admission retained bitwise direct-versus-
attached logits and explicitly reported `decision_readout=not-invoked`; this is
profile evidence, not breadth. The completed common session-state identity now
detects attention or recurrent mutation before candidate scoring. Subsequent
project-control archaeology established that complete hybrid/device candidate
bytes are characterization metadata with sufficient existing typed runtime
authorities, so their bounded producer repair and measurement belong inside the
selected breadth qualification rather than another prerequisite. Future
calibration must use independent evidence; selection/calibration data cannot
qualify itself. v0.1 dependencies,
incomplete MiniMax full-scale evidence
and authoritative full-model conformance remain separate. The exact
post-refoundation warm control is retained as characterization, not a release
benchmark. DeepSeek official/reference vectors and Qwen pinned upstream
references remain selected future work, not executed evidence.

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

**Additional unscheduled A11 target:** Address/hash vectors, conditional-table positive/refusal cases, cache-policy numerical invariance, phase-residency equivalence and FP4 state numerics precede complete one-token qualification and broader behavior/performance claims.

## Architecture Spectrum

Architecture Spectrum is an adversarial qualification program. Reference
models exert different pressure on topology, state, execution, composition or
typed I/O. A vertical's state records its evidence stage, **not generic system
maturity**. Planned references are not acquisition or executable support claims.

| ID | Computational pressure | Reference target | What it tries to falsify | Current public state |
| --- | --- | --- | --- | --- |
| A01 | Pure SSM | `mistralai/Mamba-Codestral-7B-v0.1` | Transformer-shaped decoder and computational-state-equals-KV assumptions | PARTIAL / exact artifact-executable; qualification complete |
| A02 | Pure recurrent | RWKV7-1.5B | Sequence state must be attention/SSM-shaped | PLANNED |
| A03 | Encoder-decoder | FLAN-T5 | Decoder-only lifecycle; missing retained encoder state/cross-attention | PLANNED |
| A04 | Diffusion language model | LLaDA-8B-Instruct | Autoregressive-only generation | PLANNED |
| A05 | Hybrid SSM + attention | Falcon-H1-1.5B-Instruct | Homogeneous layer/state assumptions | PLANNED |
| A06 | Hybrid SSM + attention + MoE | AI21 Jamba Mini | Inability to compose heterogeneous state and sparse execution | PLANNED |
| A07 | Local/sliding attention | Mistral-7B-v0.1 | Uniform full-context attention | PLANNED |
| A08 | Encoder-only / retrieval | BGE-M3 | Model equals autoregressive generator | PLANNED |
| A09 | Audio / speech | Whisper-large-v3-turbo | Text-only input and decoder lifecycle | PLANNED |
| A10 | Unified image generation | `HiDream-ai/HiDream-O1-Image` | Text-token-only output and autoregressive-only execution | PLANNED |
| A11 | Phase-asymmetric CED; CSA2 shared cross-layer compressed sparse state; conditional sparse parameter memory; quantized runtime KV | `deepseek-ai/DeepSeek-V4.1-Flash` | Model equals one decoder loop; every layer owns KV; prefill/decode share a working set; immutable parameters share one residency lifetime; conditional memory must be resident; state quantization equals weight format; successor reuses predecessor topology | PLANNED |

A01 has pinned acquisition, complete source roles, source-owned tokenizer and
grouped gate-before-normalization policy, a typed 64-layer pure-SSM program,
common CPU physical SSA and common transactional recurrent state. The durable
source manifest now binds the canonical immutable source; a deterministic
14,574,491,136-byte artifact and authenticated binding v17 reopen through the
normal engine path. Internal exact-artifact execution runs 900 forward plus two
output instructions, all 64 state bindings and 32,768 finite logits with
reset/cancel/retry/isolation/stale-state evidence. The catalog representation
is READY at that scope. Independent all-layer/whole-model numerics and hosted
conversation remain unqualified; the pinned source owns no chat template and
YVEX refuses to invent one. [Mamba2][mamba] owns that boundary.
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

### A11 DeepSeek V4.1 Flash target pressure

**PLANNED; not acquired or implemented in YVEX.** The official target is
[`deepseek-ai/DeepSeek-V4.1-Flash`](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash).
The following public-source facts were inspected for roadmap research on
2026-09-13. They are structural targets, not YVEX support, immutable acquisition
evidence or measured YVEX resource use. No upstream revision is pinned here;
actual acquisition must later capture an immutable revision through the normal
source workflow.

| Upstream structural fact | Reference value |
| --- | --- |
| Model type | `deepseek_v41` (nested text configuration: `deepseek_v41_text`) |
| Parameter populations | 552B backbone; approximately 196B conditional Engram parameters |
| Language stack | 40 layers: causal encoder 20 → decoder 20; hidden width 5120 |
| Active backbone | Approximately 8B/token prefill; 16B/token decode |
| MoE | 384 routed experts; top-6; one shared expert |
| Context | Up to 1M tokens; configured maximum 1,048,576 |
| Global KV | Approximately 890 bytes/token; main KV FP4/E2M1 with one E4M3 scale per 16 channels |
| Engram sites / rows | Layers 1 and 14; respectively 384,006,168 and 384,016,682 rows |
| Engram addressing | Maximum n-gram size 4; 8 heads; head width 256; compressed vocabulary 99,092 |
| CSA2 KV sources | Layers 2, 8, 14, 20 |
| CSA2 index sources | Layers 2, 8, 14, 20, 24, 28, 32, 36 |
| Index geometry | 32 heads × 128; top-k 512 |
| Hierarchical candidates | Source layer 20; 2048 candidate blocks × 8 |
| Hyper-connections | Multiplicity 4; revised Single-Pass mHC / Mega-mHC |
| DSpark | 3 next-token layers; block size 5; target taps 37, 38, 39 |

Architecture totals, phase activation, FP4 scale granularity and mechanisms come
from the [official model card](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/README.md).
Exact dimensions, layer indices and addressing cardinalities come from the
[official configuration](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/config.json)
and [inference configuration](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/inference/config.json).
The [official Engram reference](https://huggingface.co/deepseek-ai/DeepSeek-V4.1-Flash/blob/main/inference/engram.py)
also exposes compressed-token mapping, window/hash partitions, padding and
image-boundary handling: these must become exact source semantics, not runtime
string heuristics. Public-source inspection does not admit that code into YVEX.

V4.1 is a distinct architecture pressure. [Current V4/DSpark][deepseek] retains
its 43-layer SWA/compressed sparse/heavy-compression hybrid, existing mHC,
256 routed/top-6 experts and current target/draft relationship. Its evidence and
v0.1 target remain unchanged. V4.1 instead adds CED, CSA2 cross-layer sharing,
FP4 KV, Engram, revised mHC, 384 experts, a different DSpark topology and a vision
source architecture. A successor name or matching shape establishes no support.

The CED source shape is causal encoder → terminal encoder hidden state → global
decoder KV projection → autoregressive decoder. CSA2 assigns static Full,
Reindex and Reuse modes with shared main KV, indexer keys and reused Top-K
indices; the hierarchical indexer uses a candidate pool from the first Full
decoder layer. These are source-declared dependencies to project through common
program/state semantics, not per-layer duplicate KV owners or a CSA2 runtime.
They pressure [phase liveness](#phase-liveness-and-quantized-runtime-state), beyond
ordinary encoder-decoder recognition.

Single-Pass mHC requires separate source interpretation, typed semantics,
independent reference numerics, physical lowering and backend numerics. Current
V4 mHC evidence proves only its admitted scope. Likewise broader MoE routing
does not imply a new runtime: total parameters != active parameters != resident
parameters != moved parameters. Learned expert routing and token-addressed
Engram select different populations with distinct semantics/accounting, even if
their physical caches eventually share mechanisms.

#### Bounded GB10 text target and evidence ladder

The first intended machine pressure is one GB10 node in the approximately
128 GB unified-memory class: **text, target only, small admitted context, bounded
prompt, one generated token**. There is no YVEX fit claim before exact
source/physical inventory, resource admission and execution evidence.

The useful terminal boundary is exact official source → inventory → architecture
import → typed semantic/program projection → admitted physical recipe → GB10
load/resource admission → complete causal encoder on a bounded prompt →
encoder-to-decoder state projection → decoder → complete logits → selected token.
No partial layer/tensor result substitutes for that complete path.

| Explanatory evidence boundary | Required future result |
| --- | --- |
| V41-SOURCE | Official immutable acquisition and exact config/tokenizer/tensor inventory |
| V41-IMPORT | CED, CSA2, Engram, Single-Pass mHC, MoE and target relationships projected through common semantics |
| V41-PHYSICAL | GB10-oriented text/target-only recipe, conditional-table and runtime-state representations, explicit resource envelope |
| V41-REFERENCE | Bounded independent/component numerics, including addressing and refusal vectors |
| V41-CUDA | Admitted GB10 primitives with reference/backend numerical evidence |
| V41-RESIDENCY | Phase-aware parameter lifetime, expert residency/cache and conditional-table backing/cache; policy-equivalent results |
| V41-TEXT-ONE-TOKEN | Complete bounded source → encoder → decoder → logits → token execution |

Rows describe a non-scheduled dependency/evidence progression in that order,
not new ACTIVE/NEXT milestones. Later evidence may expand to multiple tokens,
continued prefill, larger contexts, DSpark, vision and provider breadth. None is
an initial terminal requirement. Neither 1M context, long generation, release
performance nor full provider parity belongs to the first target.

Refoundation .1 supplies the common compiler/runtime authority on which this
future integration depends; A11 adds no .1 closure criterion. Importers own
irreducible source meaning, while canonical compiler/runtime and admitted
backend owners execute it. No V4.1-specific runtime, application family branch,
YAI/Case dependency or B1 dependency is introduced. A11 must eventually execute
through ordinary YVEX model/provider semantics independently of Program N.

#### External feasibility evidence

The [Dwarf Star conversion/execution record](https://huggingface.co/antirez/deepseek-v4.1-flash-gguf)
reports a Q2 file of 340.60 GiB: 151.77 GiB main weights plus 188.83 GiB Engram
tables. It documents one 128 GB Mac using SSD streaming and a two-128-GB-Mac
tensor-parallel path; Engram remains file-backed rather than fully resident.
The documented V4.1 execution path is Metal. These are producer-reported
**external physical-feasibility/execution evidence**, not runs reproduced by
YVEX or evidence for CUDA, GB10, a YVEX physical recipe, numerics, fit, performance
or release qualification. The byte figures exclude context/runtime reserves.
No per-rank residency figure is adopted here.

This motivates investigating bounded 128 GB-class execution with non-resident
parameter mechanisms. It neither overrides official model mathematics nor
selects Dwarf Star's implementation/cache architecture for YVEX. YVEX's target
is compiler-proven phase/resource liveness informing physical specialization;
no speed advantage over model-wide streaming is predicted.

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
| 5 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` | COMPLETE | R / C | Typed computational language, explicit state/effects, program/component and physical/target lowering boundaries | Current Qwen forward/output, DeepSeek/DSpark target/draft schedule and MiniMax neural components use compiler-owned execution truth; runtime consumes authenticated binding v16/model-plan v8 schedule/programs. Source/catalog, importer, program, physical, runtime and backend authorities are distinct. Pure SSM remains representable without claiming A01 execution or N. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.QUALIFICATION.0` |
| 6 | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0` | COMPLETE | Q / R / C | Broad consumer evidence and comparable post-cutover replay | Compiler refusals, migrated-family controls, transactions, CPU/CUDA and sanitizer lanes qualify the claimed cutover. Independent full-model/upstream gaps remain explicit. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1` |
| 7 | `SPECTRUM.MAMBA2.REPAIR.0` | COMPLETE | C / S | Source-owned token/numerical policy and pure-SSM compiler/runtime execution; A01 evidence remains PARTIAL | Exact source compiles all 64 layers without attention/KV/RoPE/dense FFN; portable CPU physical SSA uses common transactional state. Artifact/hosted claims remain excluded. | `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1.QUALIFICATION.0` |
| 8 | `SPECTRUM.MAMBA2.QUALIFICATION.0` | COMPLETE | Q / C / S | A01 artifact and exact model execution at earned scope; A01 remains PARTIAL | Canonical source-manifest rebinding, deterministic artifact/binding admission and internal all-layer/output/session execution are qualified. Independent whole-model numerics and hosted conversation remain unclaimed. | `SPECTRUM.MAMBA2.REPAIR.0` |
| 9 | `PROJECT_CONTROL.POST.A01.RECONCILIATION.0` | COMPLETE | R / Q | Project-control selection only; no maturity promotion | Live ownership and real V4.1 evidence select one bounded acquisition lifecycle wave; no production implementation occurred in reconciliation. | `SPECTRUM.MAMBA2.QUALIFICATION.0` |
| 10 | `MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0` | COMPLETE | R / X / Q | Supervised source acquisition, truthful typed progress/health and operator projection; no source-support promotion | Source-owned operation v1 and status v2 supervise immutable transfers across client/terminal loss. Deterministic lifecycle/refusal QA plus a pinned 487,753-byte real HF acquisition qualify identity-bound stop/resume/reconciliation, bounded stall detection, provider policy and TTY/log/JSON projection while unknown provider facts remain unknown and source verification remains separate. | `PROJECT_CONTROL.POST.A01.RECONCILIATION.0` |
| 11 | `PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0` | COMPLETE | R / Q | Decision Readout/Core target adopted and one bounded successor selected; no implementation or maturity promotion | Reconciled the established acquisition boundary with live compiler/runtime/output ownership; fixed YAI Decision Plane versus YVEX computational readout authority; selected V0 only after confirming no smaller prerequisite. | `MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0` |
| 12 | `DECISION.READOUT.ZERO.DECODE.0` | COMPLETE | O / C / Q | Decision / option readout advances to PARTIAL at one exact internal model/state scope | One already-loaded Mamba2 CPU artifact produces a typed finite-candidate result from one captured common recurrent prefix. Independent score arithmetic, full-prefix replay, order/isolation, cancellation/retry, exact identities and bounded resource/latency evidence qualify zero sampling, zero generated tokens, one backbone and no calibration or semantic authority. | `PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0` |
| 13 | `PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0` | COMPLETE | R / O / Q | Project-control selection only; no maturity promotion or production implementation | Live breadth archaeology selected Qwen's hybrid recurrent-plus-attention state as the strongest second class, but proved that exact source/release-manifest association and current binding admission are a smaller prerequisite. It selected that one repair without starting breadth, V1/calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization. | `DECISION.READOUT.ZERO.DECODE.0` |
| 14 | `DECISION.READOUT.QWEN.ADMISSION.0` | COMPLETE | R / C / S / Q | Prerequisite only; Decision / option readout remains PARTIAL | Exact original source and immutable YaiLabs release authority bind the unchanged 53,815,809,152-byte/851-tensor BF16 GGUF. A fresh authenticated binding v17 opens one ordinary target-only CUDA engine; exact tokenizer, compiler-owned 64-layer hybrid forward, complete 248,320-logit output and common attention-plus-recurrent prefix capture/attach qualify with direct-replay `max_abs=0`. Malformed retained v16 remains refused; no Decision Readout executed. | `PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0` |
| 15 | `PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0` | COMPLETE | R / O / Q | Project-control selection only; no maturity promotion or production implementation | Live genericity archaeology proved that Qwen ordinary execution is ready but Decision Readout v1 still manufactures an all-EXACT profile and an incorrect CUDA kernel-bundle identity. It selected one common execution-profile prerequisite without starting Qwen scoring, breadth, calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization. | `DECISION.READOUT.QWEN.ADMISSION.0` |
| 16 | `DECISION.READOUT.EXECUTION.PROFILE.0` | COMPLETE | O / S / Q | Prerequisite only; Decision / option readout remains PARTIAL | One runtime-specialization owner now derives and re-admits generation/readout profiles from exact opened model, session, binding and backend facts. CPU Mamba retained its exact V0 oracle/order/replay/zero-generation evidence. Qwen ordinary CUDA bound build identity `3028627c3fd9220cd498998200fc61f336d4893797776bc5b5e0a104fd5215ae`, DEVICE_NATIVE execution, COMPATIBLE_DEGRADED attention/MoE and EXACT not-invoked sampling; false stronger, stale, foreign and malformed profiles refused. No Qwen candidate scoring or breadth claim occurred. | `PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0` |
| 17 | `PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0` | COMPLETE | R / O / S / Q | Project-control selection only; no maturity promotion or production implementation | Live ownership archaeology proved that prefix v2 already authenticates the immutable hybrid snapshot and session summaries already expose typed attention/recurrent resource domains, but the readout's mutable source-session identity can miss attention mutation. It selected one smaller common session-state identity prerequisite; no Qwen candidates, breadth, resource producer repair, calibration, YAI/I07, Program N, A11, another Spectrum vertical or GB10 optimization started. | `DECISION.READOUT.EXECUTION.PROFILE.0` |
| 18 | `DECISION.READOUT.SESSION.STATE.IDENTITY.0` | COMPLETE | O / S / Q | Correctness prerequisite only; Decision / option readout remains PARTIAL | Internal committed-session-state schema v1 composes exact model/binding/specialization, engine generation, authenticated session lineage and canonical target-attention, draft-attention, then recurrent committed facts under the session lifecycle lock. Real owner-level mutation/refusal controls close attention-only, recurrent-only and hybrid false-pass classes; Decision Readout delegates its before/after invariant to this owner and exact Mamba CPU V0 remains numerically unchanged. Ordinary Qwen hybrid observation reports both active domains with `decision_readout=not-invoked`; candidate/device accounting and breadth remain open. | `PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0` |
| 19 | `PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0` | COMPLETE | R / O / S / Q | Project-control selection only; no maturity promotion or production implementation | Live ownership archaeology found no remaining smaller correctness prerequisite. Existing session summaries already own non-overlapping attention and sequence resource domains; incomplete Qwen branch bytes are characterization metadata that the bounded breadth wave can repair and qualify before publication. It selected that experiment without constructing or scoring Qwen candidates or changing resource code. | `DECISION.READOUT.SESSION.STATE.IDENTITY.0` |
| 20 | `DECISION.READOUT.QWEN.BREADTH.0` | COMPLETE | O / S / Q | Second exact model/state-class readout qualified; Decision / option readout remains PARTIAL | Common schema-v1 likelihood readout executes the exact Qwen BF16 CUDA model over hybrid attention/recurrent prefix state. Independent arithmetic and replay meet `1e-12`, order/retry preserve scores, source state remains unchanged, sampling/generated counts are zero and one backbone is resident. Full-logits padding remains in normalization while candidate IDs stay tokenizer-bound. No universal family, calibration or public producer claim follows. | `PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0` |
| 21 | `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` | ACTIVE | R / O / S / Q | Common execution capacity, resource truth and retained cleanup; no public/schema or maturity promotion | Generation, readout and ordinary decoder qualification consume one runtime capacity owner; shared session-resource projection replaces duplicate arithmetic. Foreign busy observation must not read mutable sequence state. Cleanup retains retryable ownership and refuses publication/reuse on failure. Tiny compiled execution, recurrent CPU and hybrid CUDA consumers must qualify the boundary; DeepSeek CPU/CUDA numerical divergence and intermittent CUDA non-finite execution must be resolved before closure. | `DECISION.READOUT.SESSION.STATE.IDENTITY.0`; Qwen breadth pressure |
| 22 | `PROJECT_CONTROL.POST.RUNTIME.EXECUTION.CONSOLIDATION.0` | NEXT | R / O / S / Q | Project-control reconciliation only | Assess the qualified common substrate and remaining breadth/evidence limits; select one justified next boundary without implicitly starting new capability, calibration, YAI/I07, N/B1, A11, release work or GB10 optimization. | `DECISION.READOUT.QWEN.BREADTH.0`; `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` |
| 23 | `V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` | PARTIAL | Q / S / R | Warm execution performance | Measured bottleneck-driven improvement without numerical/lifecycle regression. | Explicit resumption and controlled workload |
| 24 | `V010.EVAL.DEEPSEEK.0` | BLOCKED | Q | Model behavior evidence | Repeatable quality, tokenizer, long-context and refusal evaluation. | `V010.RUNTIME.DEEPSEEK.GB10.OPTIMIZATION.0` |
| 25 | `V010.BENCH.DEEPSEEK.0` | NOT MEASURED | Q | Full-model performance | Identity-bound latency, throughput, memory and reliability evidence. | `V010.EVAL.DEEPSEEK.0` |
| 26 | `V010.RELEASE.0` | BLOCKED | Q | Release | All version-specific gates close together. | Benchmark and remaining release obligations |

Active Next: MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0

The bounded .0 refoundation, its qualification, the `.1` universal-IR
consumer cutover and `.1` qualification are complete. The A01 repair resolved
source policy and pure-SSM compiler/runtime authority; its qualification now
closes exact artifact/binding and internal all-layer/output/session execution.
A01 remains PARTIAL because independent all-layer/whole-model conformance and
hosted conversation are unavailable. Post-A01 reconciliation and the selected
acquisition lifecycle implementation are complete. DeepSeek optimization stays
PARTIAL pending explicit resumption; upstream/whole-model conformance remains
Program Q work.

Source now owns a durable operation record distinct from verified source and
provider cache. Its identity binds provider, repository, immutable revision,
selection, canonical location and generation; boot ID plus process start ticks
authenticate supervisor/provider control and reject PID reuse. A double-forked
supervisor owns one transfer independently of the invoking CLI/terminal. Status
v2 separates lifecycle from health and preserves nullable, non-overlapping
committed, provider-activity, selected-file/shard and provider-partial facts.
Selected-domain in-flight bytes and retry facts remain unknown when the provider
offers no stable machine signal; human provider logs are audit evidence, not
canonical progress.

Deterministic controls qualify terminal disconnect/reattach, structured retry,
slow progress, bounded stall, provider and supervisor loss, explicit stop and
exact-generation resume, PID reuse, stale/malformed state, lock refusal,
TTY/NO_COLOR/log/JSON projections and finalization. A real immutable HF source
(`hf-internal-testing/tiny-random-gpt2@71034c5d8bde858ff824298bdedc65515b97d2b9`)
completed 487,753 selected bytes across seven files including one Safetensors
object, then reopened through the normal verification receipt. The existing
roughly 510 GB V4.1 transfer remains motivating evidence, not a mutated fixture.
Acquisition completion still does not establish model support, artifact
readiness or release qualification.

Decision Readout V0 is complete at one exact internal model/state scope. Live
archaeology selected the already-admitted Mamba-Codestral-7B-v0.1 CPU artifact
because its immutable artifact, binding, tokenizer, output program and common
recurrent state were all available through ordinary ownership. Internal schema
v1 scores exact token candidates by summed log-likelihood, publishes separate
mean token log-probability and an explicitly uncalibrated finite-population
relative distribution, and binds model, artifact, binding, engine, tokenizer,
prefix, candidate population, score policy, readout and result identities.

The exact live control used prefix `[1]` and candidates `[3]`, `[4]`, `[3,4]`
and `[3]`. One prefix forward fed five teacher-forced candidate steps and two
logits rows with one backbone, zero sampler calls and zero generated tokens.
The multi-token score was `-31.875254551685494` versus independent long-double
reference `-31.8752545516854968536` (`max_abs=3.144468487706128e-15`, tolerance
`1e-12`); order and full-prefix replay differences were zero. Shared committed
state identity was unchanged across success and cancellation. The standalone
bounded CPU run observed 27.763 seconds prefix time, 137.920 seconds candidate
time and 165.683 seconds total, with 14,574,491,136 mapped model bytes and
557,842,432 bytes each for shared and branch state. This is characterization,
not a benchmark or performance-advantage claim.

Mapped final-tree QA recorded 118 PASS, zero FAIL/ERROR/SKIP and 28 BLOCKED
external-tool/asset lanes; runtime and quant sanitizer lanes passed.

Post-V0 live breadth archaeology distinguishes representation availability from
engine/readout compatibility. Qwen is the highest-value second class because
its admitted architecture has 48 recurrent/gated-delta and 16 attention layers,
and common prefix schema v2 already represents both state classes. Its exact
53,815,809,152-byte, 851-tensor BF16 GGUF artifact
`1fce07008eaa78e04eedd1a031144f48eb6af617f2b5c508811ba91dca7e00f1`
is now durably associated with verified original source
`Qwen/Qwen3.8-27B@1d4bf0f2ff6012fd82039f2fa52739d0dd7c60c0` and immutable
published release `yailabs/Qwen3.8-27B-Text-GGUF@066eb288bffd5a07c0d5ca584114a1f3fcfd13a8`.
Ordinary prepare verified rather than rewrote that artifact and published fresh
binding v17 `164b88fbb116c2ef1daad6f4bb840f7b655d4c0dce8bae7b3c2e85bf81701126`;
the retained v16 canonical body remains refused.

DeepSeek V4/DSpark has verified source but its local roughly 95 GB representation
also has an obsolete malformed binding, while ordinary prepare plans a distinct
roughly 100 GB CUDA materialization; it is not the smallest breadth prerequisite.
MiniMax's READY profile is a composite media engine without the required text
token-output/prefix contract. Gemma and GLM remain remote/unbound. Qwen admission
then opened one ordinary target-only CUDA engine over the unchanged artifact,
executed all 64 compiler-owned layers plus the two-step output program, produced
248,320 finite logits and captured/attached prefix schema v2 with both attention
and recurrent state. Direct and attached continuation logits were bitwise equal
(`max_abs=0`, tolerance zero); source state and sessions remained isolated.
No Decision Readout v1 execution or change, public capability field, maturity
promotion, artifact substitution, requantization, V1/calibration, YAI/I07,
Program N, A11, another Spectrum vertical or GB10 optimization occurred.
Post-admission reconciliation then inspected the unchanged readout owner and
selected its false all-EXACT CUDA profile as one smaller prerequisite. The
completed repair moves generation and Decision Readout onto one runtime-
specialization derivation/admission owner. Profiles are resealed against the
current engine generation, specialization, workload, generation mode, evidence,
execution class, operation resolutions and actual kernel bundle. CPU binds the
portable runtime build identity and retains exact V0 posture. CUDA binds the
backend's authenticated build identity and promotes neither full-graph attention
nor native MoE beyond the binding/backend capability actually opened.

The exact ordinary Qwen lane observed profile identity
`8933f7f1d36d1fafdcd71034d517c0c33291d9c08d082494d6e3b0c5c0cd87d5`,
CUDA bundle `3028627c3fd9220cd498998200fc61f336d4893797776bc5b5e0a104fd5215ae`,
DEVICE_NATIVE class, COMPATIBLE_DEGRADED attention and MoE, and EXACT sampling
for the explicitly not-invoked readout workload. Its existing direct-versus-
attached 248,320-logit control remained bitwise exact and printed
`decision_readout=not-invoked`. Resealed false-EXACT attention/MoE profiles,
stale generation/specialization, foreign bundle/class and malformed records
were refused. The exact Mamba CPU V0 replay retained its independent score,
order, replay, cancellation, zero-sampling and zero-generated-token evidence.
This closes profile truth only, not Qwen readout breadth.

The same archaeology found no family switch, Qwen-specific scoring mathematics,
candidate representation, prefix type, state owner, readout schema or second
model in Decision Readout v1. The completed session-state identity prerequisite
now gives the mutable source-session observation one generic owner in
`src/runtime/session_summary.c`. Internal schema v1 seals the exact model,
binding, specialization, engine generation and authenticated session lineage,
then canonical target-attention, draft-attention and recurrent presence,
generation, extent and content identities. Observation holds the session
lifecycle lock and refuses busy, staged, transaction-active, invalid,
inconsistent, stale or closing state rather than publishing a cross-time hash.
Decision Readout delegates both sides of its source-state preservation check to
this owner; its former recurrent-only/constant-attention observation is gone.

The resource gap was different. `peak_candidate_state_bytes` formerly sampled
only `sequence_host_state_bytes`; that was truthful for Mamba CPU because the
field covers both physically owned host recurrent banks, but would have reported zero
for Qwen CUDA. It is the maximum total mutable physical state owned by one
candidate branch, not an incremental delta above the shared prefix and not a
sum over candidates. The ordinary Qwen session summary separately observed
313,786,368 device sequence-state bytes (156,893,184 committed plus 156,893,184
candidate) and 656,192 allocated / 655,360 resident attention-state bytes.
Committed/candidate and recurrent/convolution values are logical decompositions
of the authoritative host-or-device sequence allocation; attention allocated
bytes already include its private resident pages and metadata, while virtual
capacity is not physical residency. Prefix v2 separately owns immutable shared
backing. These existing typed facts and the broad readout field therefore
support a non-overlapping producer repair without another schema or authority,
but the measured maximum now samples attach and committed-token boundaries. This
is bounded breadth characterization, not a continuous memory profiler.

`DECISION.READOUT.SESSION.STATE.IDENTITY.0` is COMPLETE. Owner-level controls
prove stable repeat observation, role-sensitive target/draft composition and
identity changes for attention-only, recurrent-only and either-domain hybrid
mutation; transaction, staged state, extent mismatch, invalid providers and
stale engine/session lineage refuse. Exact Mamba CPU V0 remains within its
independent numerical, order, replay and zero-generation contracts. The
ordinary Qwen admission lane observes target attention plus recurrent state
through the new owner and still prints `decision_readout=not-invoked`.

The selected Qwen breadth experiment exposed a real residual execution seam:
ordinary Qwen qualification had obtained paged capacity through a temporary
generation context. `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` removes
that dependency, rather than creating a Qwen-specific readout path. One common
runtime owner now derives capacity for generation, readout and ordinary
decoder qualification. Typed session resources are projected once; failed
candidate cleanup remains reachable and blocks reuse. Generation and sampling
release ACTIVE under close's drain mutex; the predicate transition cannot race
past its condition-variable wakeup. A bounded real tiny-model host control
exercises SIGINT with a previously used, detached session. This does not turn
the reported, externally terminated DeepSeek shutdown into a reproduced pass.

`DECISION.READOUT.QWEN.BREADTH.0` is COMPLETE at the exact admitted artifact
and binding above. One shared hybrid prefix `[1]` feeds candidates `[3]`,
`[4]`, `[3,4]` and the opaque-ID alias `[3]`. The multi-token likelihood is
`-24.020690479888565`; independent long-double log-sum-exp over admitted logits
gives `-24.0206904798885572547` (`max_abs=7.284466773623598e-15`,
tolerance `1e-12`). Independent full-prefix-per-candidate replay has maximum
error `1.0887179364996641e-14` against that arithmetic oracle; order difference
is zero. Cancellation publishes no partial result; retry preserves scores.
Sampling/generated counts are zero, the resident backbone count is one, and
the common hybrid source-session identity is unchanged.

The observed Qwen shared prefix is 314,114,048 bytes; maximum branch physical
allocation at attach/committed-token boundaries is 314,967,616 bytes, with
201,326,592 bytes of session workspace. Mapped model bytes remain 53,815,809,152;
mapped CUDA accessibility is not a second resident copy. These are bounded
characterization facts, not a continuous device/process peak or performance
advantage. The same live harness retains the Mamba CPU oracle and lifecycle
control. No public API, calibration, semantic authority, universal model
support or upstream whole-model conformance follows from these controls.

Common-runtime consolidation remains incomplete. The tokenless activation
prefill now seals exact finite input rows per position; real DeepSeek CPU
whole/subchunk, causal-prefix, clear, cancellation and rollback controls pass.
CPU and CUDA execute the same 86 layer / 1,268 binding input lineage, but their
attention-output digests differ while committed logical-state digests agree;
the latter are not bytewise numerical-state proofs. Independent whole-model
CPU/CUDA transformer and decode controls also disagree beyond their declared
tolerance. Ordinary DeepSeek CUDA execution has intermittently reported
non-finite attention/MoE results, including after initially correct turns.
Successful warm retries or a plausible generated answer do not close either
numerical gate. Snapshot-executor lifetime, state-page initialization order
and shutdown wakeup repairs retain their bounded lifecycle evidence, not a
claim that DeepSeek numerical correctness is repaired.

The CUDA transformer initial residual previously copied decoded F32 embeddings
into every mHC stream while the portable initial program published BF16/RNE
streams. The common CUDA path now rounds only the repeated residual, retaining
the raw decoded embedding. A nonzero encoded-row component oracle verifies both
publications. On the exact two-token DeepSeek transformer control, the first
CPU/CUDA layer outside the declared tolerance moved from layer 3 to layer 4,
final hidden `max_abs` fell from `1.125` to `0.53125`, finite-population logit
total variation from `0.334954` to `0.0721083894`, and CPU/CUDA argmax changed
from `339/295` to `339/339`. This is a bounded improvement, not a pass: the
declared hidden tolerance still fails (`max_abs=0.53125`), and the second token
selects expert `101` on CPU versus `91` on CUDA at layer 4. Intermittent
non-finite CUDA behavior and independent upstream conformance remain open.
Temporary same-input stage diagnostics localized the first post-repair
CPU/CUDA difference: layer 1 attention-envelope values agree exactly over
32,768 values, while the ensuing MoE combined values differ by at most
`0.0009765625` over 8,192 values. Layer 4 MoE reaches `0.0690917969`
after the top-k expert population differs. This diagnostic identifies the
MoE numerical path and routing sensitivity; it does not establish which
whole-model output is authoritative, justify a looser gate, or close the
intermittent non-finite failure. The temporary instrumentation was removed.
The CUDA grouped MoE expert paths previously evaluated clamped SwiGLU with
F32 `expf`/products while the compiled `clamped_swiglu.f64math.bf16.v1`
operation and portable expert path require F64 arithmetic before one BF16
publication. The grouped and standalone CUDA paths now use one implementation
of that declared rule and refuse non-finite ingress before clamping. A bounded
grouped-kernel fixture gives expected/observed `0.416015625/0.416015625`
with zero tolerance and verifies refusal without output publication. This
also exposed a finite-overflow seam: ordinary CUDA row dots recovered an
exceptional F32 reduction by rescanning decoded operands in F64, but the
grouped MoE F32-input path returned the non-finite intermediate directly to
its nonlinear clamp. One common CUDA dot-recovery primitive now serves both
paths, and the grouped path recovers before clamping. The decoded-F64 fixture
proves zero output from a finite canceling dot with an overflowing F32 partial
sum (`max_abs=0`, tolerance `0`), while non-finite operands still refuse
without publication. These are bounded component facts; they do not establish
that the intermittent real-model non-finite case is resolved. The ordinary
two-token
DeepSeek CPU/CUDA control still fails (`first_layer=3`, final hidden
`max_abs=0.984375`, CPU/CUDA argmax `339/295`, total variation
`0.248446461`). A separate pre-correction F64-forensic execution also failed
(`first_layer=4`, final hidden `max_abs=0.53125`); the two execution modes are
not a before/after numerical comparison. Intermittent CUDA non-finite behavior
is not yet qualified as resolved.

`MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0` is the sole ACTIVE
boundary; post-consolidation reconciliation is NEXT, not started.
Decision / option readout stays PARTIAL; general Score, Prompt
logprobs, Classify, Rerank and Reward are not promoted. I07 remains unselected.

The [current consumer matrix](docs/architecture/compilation.md#current-consumer-cutover)
records the completed authority cutover: Qwen executes physical SSA;
DeepSeek/DSpark executes physical implementation plans under the canonical
operator schedule retained in the binding; MiniMax neural components use typed
component programs while media/product I/O remains outside neural IR.
Exact Qwen artifact-backed replay is qualified at migration-preservation scope:
the registered BF16 representation is locally READY, physical SSA executes
1,732 steps over 14 reusable slots, and a fresh-session replay compared 10,240
values exactly (`max_abs=0`, tolerance zero) while cancellation, stale-result
and cleanup refusals remained closed. This is same-implementation determinism,
not whole-model before/after evidence or upstream conformance.

DeepSeek target-only and DSpark generation, schedule consumption, state
preservation and output-head component oracles pass on CPU/CUDA. Given the same
admitted hidden input, the CUDA output-head reference comparison observed
`max_abs=7.62939453125e-06`; the retained broader CPU/CUDA comparison still
observed `max_abs=9.982114791870117`, consistent with the pre-existing
approximately 9.982168 gap. It is therefore retained as OPEN independent
full-model conformance evidence, not normalized away or classified as a
compiler-cutover regression. Official DeepSeek vectors and authoritative
upstream Qwen conformance were not available for reproducible execution.

MiniMax audio CPU/CUDA programs reproduce their exact retained fixtures
bitwise; text-layer CUDA observes `max_abs=0.03125` within its registered
tolerance; joint-program replay preserves 128 execution values and 768 profile
values exactly while malformed capacity, target, aliasing, transaction and
cancellation cases fail closed. The exact complete 50-block fixture set is not
configured, so full-scale/whole-model evidence remains
`BLOCKED_BY_ASSET_IDENTITY`; bounded component preservation is not promoted.
Mamba remains representability evidence only.

The source-stable mapped campaign, including compiler/program negatives,
binary/import refusal, transaction, rollback, cancellation, stale publication,
CPU/CUDA and ASan/LSan/UBSan lanes, qualifies the current cutover at exactly
that scope. A no-NVCC lane initially encountered a stale external dependency
prefix rather than a product failure; the lane now owns an empty verified
prefix and passes two consecutive builds. Missing upstream references, live
assets and performance baselines remain BLOCKED/NOT RUN, never PASS.

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
External Golden or the completed universal consumer cutover. YAI owns its
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
| Now | Give Decision Readout a truthful common committed-session-state identity before cross-model breadth | O / S / Q | One runtime/session owner composes every active target/draft attention and recurrent committed identity and extent under exact engine/session lineage; readout before/after checks reject active, stale, inconsistent or mutated state. Mamba CPU remains preserved and no Qwen candidate scoring occurs. |
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
| `MAINTENANCE.ARCHITECTURE.REFOUNDATION.1`, COMPLETE | R / C | Typed values, shapes, operations, effects, programs, canonical schedules and explicit lowering; no N implementation | Pure-SSM representability established, not A01 execution | Preserved claimed-scope numerics; no automatic release promotion |
| .1 real Golden prefill characterization | R / S / Q | Long-context execution and measured residual bottlenecks; characterization, not a .1 closure gate | Existing DeepSeek execution pressure, not A01 progression | Full request still fails; no downstream-safe claim or behavior, benchmark or release gate promotion |
| `.1.QUALIFICATION.0`, COMPLETE | Q / R / C | Compiler refusal, migrated consumer and comparable execution evidence; independent full-model/upstream gaps retained | Qualified the compiler cutover without promoting A01 | No upstream conformance or release evidence inferred from internal parity |
| `SPECTRUM.MAMBA2.REPAIR.0`, COMPLETE | C / S | Semantic IR, pure-SSM composition, non-KV state and tokenizer/numerical policy | A01 directly pressured generic state; exact source compiles through common CPU execution without fake Transformer roles, while A01 remains PARTIAL | Indirect; no v0.1 promotion or scope expansion |
| `SPECTRUM.MAMBA2.QUALIFICATION.0`, COMPLETE | Q / C / S | Exact source/artifact/binding admission and internal all-layer/output/session execution | A01 advances to exact artifact-executable while remaining PARTIAL; independent whole-model and hosted claims remain open; N is unaffected | Indirect; not a v0.1 gate |
| `PROJECT_CONTROL.POST.A01.RECONCILIATION.0`, COMPLETE | R / X / Q | Separated established immutable intake from OPEN supervised acquisition lifecycle; no implementation promotion | Real V4.1 acquisition is an operational source consumer, not A11 execution evidence | No release gate change |
| `MAINTENANCE.SOURCE.ACQUISITION.LIFECYCLE.0`, COMPLETE | R / X / Q | Source-owned supervised operation, typed lifecycle/health/progress, safe reconciliation and presentation projections | Reliable immutable source intake without starting A11 or another Spectrum vertical | Infrastructure reliability only; source completion is not model/release qualification |
| `PROJECT_CONTROL.POST.ACQUISITION.RECONCILIATION.0`, COMPLETE | R / Q | Decision Plane/Readout/Core ownership adopted; one bounded successor selected without implementation or maturity promotion | No Spectrum promotion by project-control selection | No release gate change |
| `DECISION.READOUT.ZERO.DECODE.0`, COMPLETE | O / C / Q | PARTIAL finite-candidate readout at one exact internal Mamba2 recurrent-state scope; explicit score semantics and zero-generation evidence | Uses one selected admitted model as pressure, not a new Spectrum vertical or universal family claim | Computational capability only; no behavior, calibration, System-One or release claim |
| `PROJECT_CONTROL.POST.DECISION.READOUT.RECONCILIATION.0`, COMPLETE | R / O / Q | Live breadth archaeology selected one exact admission prerequisite; no implementation or maturity promotion | Qwen hybrid state is existing admitted-architecture pressure, not a new Spectrum vertical | No release gate change |
| `DECISION.READOUT.QWEN.ADMISSION.0`, COMPLETE | R / C / S / Q | Exact unchanged artifact/current-binding admission and ordinary hybrid-prefix execution; Decision Readout remains PARTIAL | Uses existing Qwen compiler/runtime ownership without adding family-specific readout logic or changing A02–A11 | Prerequisite evidence only; no behavior, benchmark or release promotion |
| `PROJECT_CONTROL.POST.QWEN.ADMISSION.RECONCILIATION.0`, COMPLETE | R / O / Q | Found one smaller generic execution-profile prerequisite and selected it without implementation or maturity promotion | No Spectrum promotion or family implementation followed | No release gate change |
| `DECISION.READOUT.EXECUTION.PROFILE.0`, COMPLETE | O / S / Q | One common engine/backend-derived profile owner with current kernel identity, truthful resolution and fail-closed re-admission; Decision Readout remains PARTIAL | Qwen supplied admitted hybrid CUDA pressure, not family-specific readout logic or Axx promotion | Prerequisite evidence only; no readout breadth, behavior, benchmark or release promotion |
| `PROJECT_CONTROL.POST.DECISION.READOUT.PROFILE.RECONCILIATION.0`, COMPLETE | R / O / S / Q | Distinguished missing mutable hybrid identity correctness from incomplete candidate/device characterization; selected one smaller identity prerequisite without implementation | No Spectrum promotion or family implementation followed | No release gate change |
| `DECISION.READOUT.SESSION.STATE.IDENTITY.0`, COMPLETE | O / S / Q | One common committed-session-state identity spans exact target/draft attention and recurrent domains; Decision Readout remains PARTIAL | Ordinary Qwen hybrid observation uses existing state owners with `decision_readout=not-invoked`; no Axx promotion | Correctness prerequisite only; no release gate change |
| `PROJECT_CONTROL.POST.DECISION.READOUT.SESSION.STATE.RECONCILIATION.0`, COMPLETE | R / O / S / Q | Existing typed resource authorities make hybrid branch bytes breadth characterization rather than a smaller admission prerequisite; one experiment selected without implementation | No family implementation or Spectrum promotion occurred during selection | No release gate change |
| `DECISION.READOUT.QWEN.BREADTH.0`, COMPLETE | O / S / Q | Same schema v1 qualifies exact Qwen CUDA hybrid readout arithmetic, replay, order, cancellation/retry, source isolation and zero-generation evidence | Second materially distinct admitted state class; no new Spectrum vertical | Computational evidence only; no behavior, calibration, benchmark or release promotion |
| `MAINTENANCE.RUNTIME.EXECUTION.CONSOLIDATION.0`, ACTIVE | R / O / S / Q | Qualify common capacity, resources and retryable cleanup; resolve DeepSeek CPU/CUDA numerical divergence and intermittent CUDA non-finite execution before closure | Existing compiler/binding/state providers remain authoritative | No public/persisted schema or release-scope change |
| `PROJECT_CONTROL.POST.RUNTIME.EXECUTION.CONSOLIDATION.0`, NEXT | R / O / S / Q | Reconcile qualified substrate and remaining evidence limits before selecting further implementation | No new vertical selected | No release gate change |
| Future cognitive-state realization qualification, unscheduled | N / C / S / Q | Ingress, model-specific State Read/Update and useful-state evidence | Real model/reference required; no new Axx scheduled | Not a v0.1 gate |
| Explicit DeepSeek optimization resumption | Q / S / R | Baseline, resource/structural performance evidence | Existing DeepSeek control, not a new Axx | Predecessor of evaluation |
| Model-language breadth after A01 | C / O / D / M | Components, heads, tokenizer and modality semantics | A02–A11 selected by pressure | General substrate, not automatically v0.1 |
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
| Mamba2 hosted conversation / independent whole-model conformance | false | The catalog representation is READY and exact artifact-backed token execution is qualified, but the source owns no chat template and no independent all-layer/whole-model oracle was executed. |
| Universal selected-domain provider in-flight byte/rate/retry observability | false | Supervision is established, but provider adapters preserve UNKNOWN when no stable machine signal can bind these facts to the selected immutable population. |
| Cross-model Decision Readout breadth | bounded | One common internal schema-v1 runner qualifies exact Mamba2 CPU recurrent and Qwen BF16 CUDA hybrid state. Arithmetic, isolated replay, order, cancellation/retry, source immutability and zero-generation controls pass; this does not establish universal family breadth, calibration, semantic authority or a public Score runner. |
| Universal/public Decision Readout producer | false | Broader compatible model/state classes, a public application surface and a selected external consumer are absent. Completed Qwen admission adds none of them. |
| Production Score runner | false | Complete logits and internal component scoring do not establish a general sequence/pair scoring runner. |
| Calibrated decision probability / confidence | false | No calibration transform, artifact or independent calibration evidence exists; softmax over disclosed options is only a relative distribution. |
| System-One computational claim | false | No independently qualified specialized Decision Core or usefulness evidence exists. |
| Learned decision probe/head | false | No trained readout parameters, physical object or exact backbone/head composition exists. |
| Dedicated decision model | false | The V0 target deliberately reuses one already-loaded admitted backbone; a future specialized model remains allowed but unselected. |
| R/E-aware Decision Core | false | Decision Readout V0–V3 is independent of Program N; no E, StateProfile readout or State Read consumer is implemented. |
| Public YAI ↔ YVEX decision ABI | false | YAI semantic request/frontier/distribution contracts remain YAI-owned; an internal YVEX producer exists but no YAI consumer has selected it. I07 is UNSELECTED. |
| Adaptive cognitive router / Minimum Sufficient Cognition policy | false | YAI owns selection among deterministic logic, readout, generation and human/review; YVEX exposes computational capability only. |
| Decision Readout performance advantage | false | V0 has bounded one-model CPU latency/resource characterization, not a controlled generation comparison or benchmark; future evidence must compare time-to-qualified-solution and resources under controlled identities. |
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

`NATIVE.COGNITIVE.STATE.ALIGNMENT.0` selected no implementation wave and did
not start Program N, B1, post-training or A01. The current Decision Readout
alignment instead selects only the bounded V0 producer experiment; it implements
no readout, public ABI, calibration, learned head, adaptive router or R/E path.
The completed refoundation/A01 sequence and all v0.1 gates retain their own
criteria. B1 and model adaptation remain unscheduled target doctrine.

The A11 amendment likewise selects no implementation wave and starts no source
acquisition. A11 remains PLANNED, outside Current Execution Sequence and v0.1.
It neither replaces current V4/DSpark nor modifies .1, qualification, A01 or N.B1.
No n-gram implementation, Engram support, SSD streaming, B1 training or new
family runtime is authorized by this doctrine.

The following V4.1 capabilities are explicitly **not implemented / not qualified
in YVEX**: `deepseek_v41` source acquisition; DeepSeek V4.1 import/execution;
CED; CSA2; Hierarchical Sparse Indexer; Single-Pass mHC; V4.1 384-expert MoE;
Engram; generic n-gram addressing; conditional sparse parameter memory;
file-backed conditional-table execution or hot-row cache; compiler-derived
phase-aware residency; FP4 runtime KV; V4.1 CUDA or GB10 execution; complete
V4.1 one-token generation; V4.1 DSpark, vision or 1M context; 128 GB YVEX fit;
and V4.1 release qualification. External Dwarf Star execution != YVEX support;
V4 support != V4.1 support; an n-gram speculation target != Engram support;
Engram != Program N E or L. Runtime-state quantization is a target, not an
inference from weight-format support.

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
