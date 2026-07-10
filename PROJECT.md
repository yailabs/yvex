# YVEX Project Control

Date: 2026-07-10
Status: living engineering control
Authority: product target, architecture tracks, milestone state, dependencies,
evidence classification, repair ordering, and release gates
Current proof stage: documentation/claim refoundation only

## Authority And Update Contract

`PROJECT.md` is the single project-control authority for YVEX. Agents update it
at every milestone that changes executable state, closes a hard dependency, or
changes the truthful project-control contract. It is neither a public roadmap
nor a catalogue of every command, report, fixture, or historical row.

This file owns:

- the exact product and release target;
- current hard truth and unsupported boundaries;
- the architecture and critical path;
- primary, supporting, and future track registries;
- the current milestone and dependency transition;
- conclusive planned and completed milestones;
- implementation evidence and its owning milestone;
- release gates and the version sequence.

The project file has no arbitrary line limit or fixed heading count. Its size is
controlled by ownership, traceability, current usefulness, and non-duplication.
Git history preserves superseded detail. A permanent row-by-row migration
database, copied project control file, or compatibility spine is not an active
documentation owner.

While `docs/repair/v010-foundation-closure.md` is active, that file owns the
priority-blocking repair detail and this file owns its state and product-path
effect. `docs/system-target.md` owns filesystem and module placement.
`docs/v010-release-doctrine.md` owns release-gate semantics.

At milestone start and closure, update the owning track, current state, evidence,
dependency transition, gates affected, and Active Next. During an active repair
sequence, `PROJECT.md` and the repair owner must name the same Active Next.

## Product Outcome

YVEX generates real text with DeepSeek-V4-Flash on the DGX Spark CUDA backend
from a complete GGUF artifact produced by YVEX.

This is the exclusive v0.1.0 product target. Qwen, Gemma, GLM, additional
DeepSeek variants, Metal, ROCm, serving, and distributed execution do not close
v0.1.0.

### Exact Release Contract

Canonical source:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Future canonical full target:

```text
deepseek4-v4-flash
```

Release machine and backend:

```text
CUDA on DGX Spark
```

The future full target is unsupported. The existing aliases remain bounded
legacy proof surfaces until their owning replacement or removal milestones:

```text
deepseek4-v4-flash-selected-embed
deepseek4-v4-flash-selected-embed-rmsnorm
```

They are not supported targets and are not stages of the product critical path.

Real generation is the complete chain:

```text
prompt text
-> real tokenizer
-> full prefill
-> family-correct attention and MoE
-> real KV writes
-> output head and vocabulary logits
-> sampling
-> decode that reads the KV state
-> multiple autoregressive tokens
-> detokenized text
```

The chain must execute over the complete YVEX-produced GGUF for the exact
target. CLI acceptance, printed fixture tokens, synthetic logits, selected
segments, report generation, and bounded diagnostic loops do not satisfy it.

### Artifact Terminology

| Term | Canonical meaning |
| --- | --- |
| Tensor proof artifact | Contains one tensor or a bounded tensor subset and proves only the named lower-level property. |
| Complete model artifact | Contains every tensor and metadata item required to execute the exact model. |
| Supported model artifact | A complete model artifact that passes integrity, materialization, runtime, generation, evaluation, benchmark, and release gates. |

The unqualified term "model artifact" does not name a selected-tensor proof
file.

## Current Hard Truth

| Boundary | Current truth |
| --- | --- |
| Source | The canonical local path exists, but exact source identity, revision, architecture, tokenizer, shard, and payload verification have not closed. |
| Full target | `deepseek4-v4-flash` is the future canonical target and is unsupported. |
| Architecture | No execution-complete typed DeepSeek-V4-Flash architecture specification exists. |
| Mapping | No complete tensor role, layout, tokenizer, output-head, attention, position, KV, or MoE map exists. |
| Artifact | YVEX has not produced a complete DeepSeek-V4-Flash GGUF. |
| GGUF foundations | `V010.GGUF.QTYPE.ABI.0` and `V010.GGUF.ARTIFACT.ABI.0` are reopened; fixture-bounded evidence does not close either complete-artifact boundary. |
| Quantization | Required qtypes, conversion, reference dequantization, and quantization are incomplete. |
| Materialization | Full model materialization and CUDA residency are unsupported. |
| Runtime descriptor | No execution-complete descriptor exists for the full target. |
| CUDA | A fallback PTX path can advertise no-op kernels as supported; CUDA must become fail-closed before runtime work advances. |
| Transformer | Family-correct attention, position handling, KV, MoE routing, expert execution, and the complete stack are unsupported. |
| Text path | The real tokenizer-to-detokenized-text autoregressive chain is unsupported. |
| Evaluation | No v0.1.0 generation evaluation exists. |
| Benchmark | Not measured. |
| Release | Blocked. |

Implemented source reports, selected tensor paths, graph primitives, bounded
runtime state, diagnostic loops, CLI cells, and topology closures remain useful
evidence. None promotes the current product state beyond the lowest true stage.

## Architecture Map

YVEX owns a native path from source bytes to text. Facts move downstream only
after the owning layer validates them and exposes a typed boundary.

```text
operator and project control
        |
verified source -> architecture IR -> tensor roles and layouts
        -> qtype/conversion -> GGUF writer -> integrity and admission
        -> materialization/residency -> runtime descriptor
        -> CUDA/backend operations -> transformer execution
        -> prefill and KV -> decode -> output head/logits -> sampling
        -> tokenizer/stop/detokenization -> autoregressive generation
        -> evaluation -> benchmark -> release
```

The path preserves distinct owners for source intake, architecture, mapping,
qtypes, writer, integrity, materialization, descriptor projection, backend
capability, graph execution, runtime state, text I/O, evaluation, and release.
An upstream report cannot substitute for a downstream implementation.

### Main Critical Path

The product dependency path is:

```text
verified DeepSeek source
-> typed architecture specification
-> complete tensor role and layout map
-> complete YVEX-produced GGUF
-> full materialization and residency
-> executable runtime descriptor
-> family-correct attention, position handling and KV
-> MoE routing and expert execution
-> complete transformer stack
-> tokenizer, output head, logits and sampling
-> autoregressive generation
-> evaluation and benchmark
-> v0.1.0 release
```

Dependencies are strict. External GGUF files, external runners, later-node
evidence, and pressure targets cannot close an earlier YVEX gate.

## Track Model

Tracks are stable architecture ownership lanes. Milestones are conclusive
changes inside those lanes. Evidence is attached to milestones and is not a
track.

### Primary Release Tracks

Primary tracks lie on the direct source-to-generation or release path.

| Track | Owns | Current state | Conclusive handoff |
| --- | --- | --- | --- |
| `TRACK.SOURCE` | Exact source identity, revision, config, tokenizer, shards, header inventory, payload trust, and bounded payload streaming | source-intake and header evidence only | Verified exact source and readable mapped payloads to architecture/mapping/conversion |
| `TRACK.ARCHITECTURE` | Typed DeepSeek architecture facts: layer topology, hybrid attention, position rules, KV geometry, mHC/residual behavior, MoE topology, norms, output head, and tokenizer requirements | unsupported | Validated execution-complete architecture IR to mapping and runtime descriptor owners |
| `TRACK.MAP` | Complete source tensor role map, GGUF names, shapes, transforms, layouts, expert indices, and ambiguity refusal | partial report evidence | Every required source tensor maps exactly once to an execution role and emitted layout |
| `TRACK.TENSOR` | Required runtime tensor collections and complete role coverage across global, layer, attention, MoE, norm, position, tokenizer, and output paths | partial header/report evidence | No unresolved required tensor collection or runtime role |
| `TRACK.QUANT` | Qtype ABI, role policy, quantization, dequantization reference, numeric bounds, and compute/refusal truth | reopened and blocked | Every emitted role has truthful storage and compute behavior |
| `TRACK.ARTIFACT` | GGUF ABI, writer, conversion coordination, complete emission, identity, registration, and roundtrip | selected proof and reopened ABI evidence only | Complete YVEX-produced GGUF reopens identically through YVEX |
| `TRACK.INTEGRITY` | Container, metadata, tensor directory, range, alignment, qtype-sized layout, corruption, drift, and admission gates | bounded fixture/selected evidence | Complete artifact passes all pre-payload and pre-runtime integrity gates |
| `TRACK.RESIDENCY` | Streaming reads, materialization, placement, memory plan, CUDA residency, movement, ownership, failure cleanup, and release | selected proof and planning evidence only | Every required tensor is resident or staged under an accepted DGX Spark plan |
| `TRACK.BACKEND` | Backend capability, qtype operations, host binding, reference parity, refusal, scratch, fallback, synchronization, and cleanup | bounded CPU/CUDA primitive evidence; CUDA blocker open | Required DGX Spark CUDA operations are real and fail closed |
| `TRACK.EXECUTION` | Graph binding, attention, position handling, MoE routing and experts, residual paths, repeated layers, final norm, scratch, and full transformer execution | primitive and selected-slice evidence only | Complete transformer stack consumes the full runtime descriptor |
| `TRACK.PREFILL` | Prompt token execution, chunking, transformer prefill, state ownership, and KV write integration | diagnostic-runtime evidence only | Full prefill executes every required layer and writes real attention state |
| `TRACK.KV` | Family-correct KV geometry, allocation, indexing, append/read, capacity, lifecycle, reuse, and cleanup | bounded diagnostic storage only | Prefill writes and decode reads the same owned model KV state |
| `TRACK.DECODE` | One-step and repeated model-backed decode over descriptor, position, KV, transformer, cancellation, and cleanup | diagnostic-runtime evidence only | Repeated decode produces real hidden states while consuming prior KV |
| `TRACK.LOGITS` | Final hidden-state ownership, final norm, output-head residency/projection, vocabulary logits, numeric checks, and buffer cleanup | fixture/report evidence only | Real vocabulary logits derive from full model state |
| `TRACK.SAMPLING` | Deterministic and stochastic selection over real vocabulary logits, seeded reproducibility, and refusal | bounded fixture sampling only | Sampled token IDs derive from real output-head logits |
| `TRACK.TOKENIZER` | Exact tokenizer load, prompt encoding, special/EOS/stop policy, token append boundary, and detokenization | metadata and token-ID evidence only | Prompt text and generated IDs roundtrip through the exact tokenizer contract |
| `TRACK.GENERATION` | Composition of tokenizer, prefill, KV, decode, logits, sampling, append, stop, cancellation, partial output, and cleanup | bounded diagnostic control flow only | Multiple real autoregressive tokens become detokenized text |
| `TRACK.EVAL` | Release-path correctness, regression, failure, tokenizer, long-context, and capability evaluation | blocked | Repeatable evaluation passes over real generation |
| `TRACK.BENCH` | Reproducible machine, artifact, qtype, context, prefill, decode, generation, timing, and memory measurements | not-measured | Accepted DGX Spark benchmark record over the release path |
| `TRACK.RELEASE` | Scope lock, validation, artifact guardrail, claim audit, operator transcript, packaging, versioning, and tag | blocked | Every release gate passes in one traceable transcript |

### Supporting Architectural Tracks

Supporting tracks can block the release path but cannot promote a lower runtime
capability merely by changing control or documentation surfaces.

| Track | Owns | Current state | Boundary |
| --- | --- | --- | --- |
| `TRACK.PROJECT` | Project control, scope, milestone state, dependency transitions, documentation architecture, and control-file truth | recovery complete; documentation architecture active | A docs/control milestone may close a governance dependency only; it does not close a product gate. |
| `TRACK.CLAIMS` | Evidence stages, API/contract language, release doctrine, public/internal claim audits, and unsupported-state consistency | refounded for the DeepSeek target | Claim accuracy records implementation truth; it does not create implementation. |
| `TRACK.TOPOLOGY` | Filesystem ownership, domain/report/input/command/render cells, source contracts, and architecture guards | substantial owner separation implemented; residual audit remains | Moving or wrapping code is not capability closure unless the downstream owner consumes it. |
| `TRACK.OPERATOR` | CLI grammar, typed input, dispatch, rendering, refusal, control-plane integration, runbooks, and final operator acceptance | substantial diagnostic/control surface implemented | A command is evidence for an implemented domain path, never the path itself. |

### Future Tracks

Future tracks remain outside v0.1.0 and do not block it unless an explicit scope
milestone changes the release contract.

| Track | Scope | Entry condition |
| --- | --- | --- |
| `TRACK.SERVE` | Runtime-backed daemon generation, streaming, cancellation, observability, and protocol compatibility | Real CLI/runtime generation is stable and accepted. |
| `TRACK.DISTRIBUTED` | Multi-device and multi-node scheduling, worker coordination, expert/tensor/pipeline parallelism, and distributed KV | A separately scoped target and measured single-machine baseline exist. |
| `TRACK.PORTABILITY` | Metal, ROCm, additional CUDA profiles, and non-DGX-Spark backend work | v0.1.0 CUDA path is released or scope explicitly changes. |
| `TRACK.MODELS` | Qwen, Gemma, GLM, additional DeepSeek targets, and arbitrary-family integration | Each target receives its own complete source-to-release gates. |
| `TRACK.ACCELERATION` | Speculative decoding, storage streaming during generation, advanced caching, and post-baseline kernel programs | Correct baseline generation and benchmark evidence exist. |
| `TRACK.POST010` | Broader evaluation, public benchmark expansion, production serving hardening, and later product scope | v0.1.0 is released and the next scope is explicit. |

### Evidence Lanes

Diagnostic/report/fixture/selected work is evidence, not a primary track.
Evidence must name its owning track, the property it proves, the lowest true
stage, and the capability it does not prove.

| Evidence lane | Current examples | Valid use | Non-claim |
| --- | --- | --- | --- |
| Source and architecture reports | manifests, shard/header inventories, class profiles, tensor maps, missing-role and qtype reports | Inspect facts and test refusal before payload/runtime work | Reports are not verified source, complete mapping, or runtime support. |
| GGUF fixtures and proof artifacts | tiny GGUF fixtures, controlled emission, selected embedding and embedding-plus-RMSNorm files | Prove parser, geometry, range, writer fragments, materialization, or primitive consumption at named scope | A tensor proof artifact is not a complete or supported model artifact. |
| Primitive and selected graph proofs | CPU/reference comparisons, CUDA bounded ops, selected segments | Prove one operation, transfer, cleanup, or selected composition | A primitive or selected slice is not transformer execution. |
| Diagnostic runtime state | prefill summaries, bounded KV, decode summaries, synthetic logits/sampling, bounded generation loops | Prove ownership, lifecycle, refusal, cancellation, and composition mechanics | Diagnostic runtime is not real prefill, KV, decode, logits, sampling, or generation. |
| Operator and audit evidence | CLI commands, normal/audit rendering, help, transcripts, topology and claim audits | Prove discoverability, typed boundaries, refusal propagation, and repository structure | A command, report, audit, or transcript cannot create lower capability. |
| Internal test fixtures | deterministic vectors, corrupt files, synthetic tokens/logits, allocation and cleanup failures | Retain repeatable regression coverage below public product surfaces | Fixture success is not model quality or product support. |

## Milestone Contract

A milestone is a complete architectural or functional slice that changes the
executable state of YVEX or closes a hard dependency needed before executable
work can proceed. It has exactly one owning track and a downstream consumer.

Every milestone must record:

1. the owning track and concrete before/after capability;
2. the real downstream owner that consumes the implementation or contract;
3. explicit failure, refusal, allocation, mutation, cleanup, and lifecycle
   behavior where applicable;
4. focused executable tests, including reference comparison where numeric;
5. the full repository validation required by `AGENTS.md`;
6. the lowest truthful proof stage and unchanged higher-stage non-claims;
7. the dependency it closes and the next dependency it activates;
8. implementation evidence attached as evidence, not promoted into a track.

A doctrine or project-control milestone is valid only when it closes an explicit
governance dependency before code can safely change. Its proof stage remains
documentation/claim-only and it cannot promote artifact, runtime, generation,
evaluation, benchmark, or release state.

A row is not milestone-complete merely because it:

- adds a report, CLI command, help entry, or structured output;
- prints or serializes typed facts;
- creates a fixture or tensor proof artifact;
- exposes a selected tensor, graph segment, or bounded diagnostic loop;
- renames or moves code, or wraps an existing monolith;
- records a plan, audit, checklist, or documentation update.

These can be required subtasks or acceptance evidence inside the owning
milestone. They do not independently close architectural capability.

## Delivery State

### Current Milestone

```text
V010.DOCS.REFOUNDATION.0: complete
proof stage: documentation/claim refoundation only

V010.PROJECT.RECOVERY.0: complete
proof stage: documentation/claim refoundation only

Active Next: V010.DOCS.ARCHITECTURE.0
V010.REBASE.DEEPSEEK.0: blocked by documentation architecture
```

No artifact, materialization, runtime, generation, evaluation, benchmark, or
release capability is promoted by either completed documentation milestone.

### Priority-Blocking Foundation Sequence

The detailed owner, defect, outcome, acceptance, dependency, recovery map, and
decommission map live in `docs/repair/v010-foundation-closure.md`.

```text
V010.DOCS.REFOUNDATION.0
-> V010.PROJECT.RECOVERY.0
-> V010.DOCS.ARCHITECTURE.0
-> V010.REBASE.DEEPSEEK.0
-> V010.GGUF.QTYPE.ABI.1
-> V010.GGUF.ARTIFACT.ABI.1
-> V010.GGUF.LAYOUT.INTEGRITY.1
-> V010.CUDA.FAILCLOSED.0
-> V010.MODEL.ARCH.IR.0
-> V010.MAP.GGUF.DEEPSEEK.0
-> V010.SOURCE.PAYLOAD.STREAM.0
-> V010.QUANT.2
-> V010.GGUF.WRITER.1
-> V010.ARTIFACT.EMIT.DEEPSEEK.0
-> V010.GGUF.ROUNDTRIP.1
```

The main runtime path cannot advance while this sequence is active.

### Conclusive Main-Path Milestones

These milestones preserve the original architecture responsibilities while
merging diagnostic-scale rows into complete slices. Their exact implementation
contracts are written only when each becomes Active Next.

| Order | Milestone | Owning track | Conclusive after-state | State |
| --- | --- | --- | --- | --- |
| 1 | `V010.ARTIFACT.SUPPORT.CUTOVER.0` | `TRACK.ARTIFACT` | Only complete artifacts may enter model support gates; retained subsets are named tensor proof artifacts. | blocked by complete roundtrip |
| 2 | `V010.ARTIFACT.MATERIALIZE.DEEPSEEK.0` | `TRACK.RESIDENCY` | Every required tensor is streamed, materialized, placed, owned, and released under the DGX Spark plan. | blocked |
| 3 | `V010.RUNTIME.DESCRIPTOR.DEEPSEEK.0` | `TRACK.ARCHITECTURE` | The admitted complete artifact projects to one execution-complete DeepSeek descriptor. | blocked |
| 4 | `V010.RUNTIME.DEEPSEEK.ATTENTION.KV.0` | `TRACK.PREFILL` | Family-correct prefill and attention write KV that decode can read, including position and capacity behavior. | blocked |
| 5 | `V010.RUNTIME.DEEPSEEK.MOE.0` | `TRACK.EXECUTION` | Real router logits select experts, expert weights execute, outputs accumulate, and failures clean up. | blocked |
| 6 | `V010.GRAPH.DEEPSEEK.TRANSFORMER.0` | `TRACK.EXECUTION` | Embedding through repeated attention/MoE layers and final norm executes over the full descriptor. | blocked |
| 7 | `V010.RUNTIME.DEEPSEEK.DECODE.0` | `TRACK.DECODE` | Repeated model-backed decode consumes prior KV and produces the next hidden state with cancellation and cleanup. | blocked |
| 8 | `V010.RUNTIME.DEEPSEEK.LOGITS.SAMPLING.0` | `TRACK.LOGITS` | Output-head vocabulary logits and selected tokens derive from real transformer state. | blocked |
| 9 | `V010.RUNTIME.DEEPSEEK.TOKENIZER.0` | `TRACK.TOKENIZER` | Exact prompt encoding, special/stop behavior, and detokenization are implemented and tested. | blocked |
| 10 | `V010.RUNTIME.DEEPSEEK.GENERATION.0` | `TRACK.GENERATION` | Multiple autoregressive tokens traverse the entire release chain and become detokenized text. | blocked |
| 11 | `V010.CLI.DEEPSEEK.GENERATE.0` | `TRACK.OPERATOR` | One operator command invokes the accepted runtime path and preserves truthful refusal and cleanup. | blocked |
| 12 | `V010.EVAL.DEEPSEEK.0` | `TRACK.EVAL` | Repeatable correctness, regression, refusal, and release-path evaluations pass. | blocked |
| 13 | `V010.BENCH.DEEPSEEK.0` | `TRACK.BENCH` | Reproducible DGX Spark prefill, decode, generation, timing, and memory evidence is measured. | not-measured |
| 14 | `V010.RELEASE.0` | `TRACK.RELEASE` | The complete validation, artifact, claim, operator, evaluation, benchmark, and version transcript passes. | blocked |

## Recovered Implementation History

Recovery compared `10ad6c3:docs/spine.md`, which contains the architectural
tracks and row history, with `690d1b18:docs/spine.md` and its repair owner, which
contain the corrected DeepSeek target and hard claim boundaries. Neither source
is independently authoritative. The entries below retain substantive
implemented history at the owning-track level. A historic `complete` label is
reclassified as evidence when its after-state was a report, fixture, selected
slice, diagnostic path, or control surface rather than a conclusive product
milestone.

| Historic responsibility | Traceable completed anchors | Durable implemented fact | Current classification |
| --- | --- | --- | --- |
| Scope and target control | `V010.SCOPE.0-.1`, `V010.TARGET.1-.3`, `.7`, `.9`, `SPINE.*` control rows | Release doctrine, gates, target reports, and prior project-control mechanics were implemented. | DeepSeek-only doctrine retained; multi-family and obsolete spine rewrites superseded, not restored as milestones. |
| Source intake | `V010.SOURCE.1-.7`, `.9`, `V010.SOURCE.7A/7B`, `MODELS.DOWNLOAD.*`, `MODELS.SOURCE.*`, `OWI.TARGETS.*`, `MODEL.TARGET.IDENTITY.0` | Manifests, header inventories, provider preflight, download lifecycle, target identity, and role-coverage handoff exist. | Evidence and reusable implementation under `TRACK.SOURCE`; exact-source verification and payload trust remain open. |
| Tensor mapping | `V010.MAP.1`, `.5-.9` | Dense/Qwen names, output-head and tokenizer maps, missing-role reports, and a report gate exist. | Merged evidence under the complete DeepSeek map milestone; no complete map claim. |
| Qtype and dtype | `V010.QUANT.0-.1`, `V010.GGUF.QTYPE.ABI.0` | Policy, role-support reports, and bounded qtype byte geometry/refusal tests exist. | Evidence retained; `.0` is reopened and `.1` is a blocking repair. |
| Artifact production | `MODELS.ARTIFACTS.LIST.0`, `V010.GGUF.ARTIFACT.ABI.0`, `V010.ARTIFACT.EMIT.0-.1` | Artifact listing, tiny container parsing, controlled emission, and selected emission exist. | Fixture/tensor-proof evidence; artifact ABI is reopened and no complete artifact exists. |
| Artifact integrity | `V010.INTEGRITY.0-.6`, `.8-.10` | Identity, digest, metadata, directory, range, shape/dtype, overflow, drift, corruption, and selected preflight checks exist. | Bounded evidence merged into complete-layout and complete-artifact admission milestones. |
| Model architecture reports | `MODEL.CLASS.QWEN.0`, `MODEL.CLASS.GEMMA.0`, `MOE.CLASS.0`, `V010.CLASS.3`, `.6`, `.8-.11` | Header-derived dense/MoE, attention, context, and KV class facts exist. | Evidence under `TRACK.ARCHITECTURE`; no execution-complete DeepSeek IR or descriptor. |
| Tensor collections | `TENSOR.COLLECTION.QWEN.0`, `TENSOR.COLLECTION.GEMMA.0`, `TENSOR.MOE.0`, `V010.TENSOR.1`, `.5-.8`, `.11-.12`, `.14-.19`, `.21-.22` | Required collection categories and missing-role facts are inspectable. | Header/report evidence under `TRACK.TENSOR`; no complete execution coverage. |
| Residency and materialization | Selected materialization and placement surfaces referenced by the old track; no conclusive residency row closed | Bounded descriptor, range, placement, movement, and cleanup behavior exists for selected proofs. | Evidence only; the 42 storage/residency micro-rows merge into payload streaming and full materialization milestones. |
| Backend capability | `CUDA.KERNEL.0`, `V010.BACKEND.1-.5` | CPU baseline, CUDA probe/allocation/transfer, and bounded parity checks exist. | Primitive evidence; CUDA support is blocked by `V010.CUDA.FAILCLOSED.0`. |
| Graph execution | `V010.GRAPH.22` plus implemented primitive/reference tests | Selected graph composition and bounded primitives consume admitted bytes and compare outputs. | Selected/fixture evidence merged into attention, MoE, and complete transformer milestones. |
| Prefill and context | `V010.CONTEXT.1-.7`, `V010.PREFILL.0-.1` | Context bounds, chunk planning, positions, overflow refusal, and diagnostic prefill input/summary exist. | Diagnostic evidence under real prefill/attention/KV milestone. |
| KV runtime | `V010.KV.0-.1`, `.3`, `.14` | Bounded KV ownership, shape/capacity, lifecycle, and diagnostic binding exist. | Diagnostic storage evidence; not attention-backed KV. |
| Decode | `V010.DECODE.0-.2` | Requirement, state ownership, and position input surfaces exist. | Diagnostic evidence under one model-backed decode milestone. |
| Output head and logits | `V010.LOGITS.0-.1`, `.3`, `.6`, `.8-.9` | Hidden-state ownership, map/report, buffer, checksum, and synthetic top-k diagnostics exist. | Fixture/report evidence under real output-head logits milestone. |
| Sampling | `V010.SAMPLE.0`, `.2-.4` | Sampling requirements, candidate/selection facts, and temperature validation exist. | Internal fixture evidence under sampling over real logits. |
| Tokenizer and stop | `V010.TOKENIZER.2`, `.4` plus tokenizer metadata map | Token-ID input and special-token diagnostic facts exist. | Evidence under exact tokenizer/stop/detokenization milestone. |
| Generation lifecycle | `V010.GEN.0-.7`, `.11-.15`, `.17`, `V010.RUNTIME.0`, `.2`, `.4-.5`, `.15`, `V010.TRACE.1`, `.10-.11` | Bounded composition, append, limits, trace, cancellation, partial output, cleanup, and smoke mechanics exist. | Diagnostic evidence under real autoregressive generation; not generation capability. |
| Operator and topology | `TOPOLOGY.FS.0`, `TOPOLOGY.SOURCE.CONTRACT.0`, `TOPOLOGY.CLI.PRINT.ALL.0`, `TOPOLOGY.DOMAIN.RESTORE.0`, `TOPOLOGY.CELL.SOURCE.0`, `.GENERATION.0`, `.KV.0`, `.SAMPLING.0`, `.GRAPH.0`, `TOPOLOGY.CELL.MODEL_TARGET.0-.3`, `TOPOLOGY.CELL.MODEL_ARTIFACTS.0-.3`, `SPINE.SYSTEM.TARGET.0`, `V010.PATHS.*`, `V010.CLI.17-.19`, `.24-.26` | Source ownership, cell separation, model-target/model-artifact decomposition, path policy, CLI grammar, and render ownership were materially implemented. | Restored as supporting architecture history; remaining cleanup must close owner-specific behavior, not wrapper rows. |
| CI and release guards | `V010.CI.4-.7` | Docs, source-layout, natural-code, and artifact guardrails exist. | Supporting release evidence; no release readiness. |
| Serving, evaluation, benchmark, post-v0.1 | No conclusive runtime milestone completed in the old spine | Status shells and lower-level tests exist, but no generation-backed serve/eval/benchmark path. | Planned micro-rows merged into future serving scope and one conclusive eval, benchmark, or release milestone. |

The track-level disposition and counts used for this recovery are recorded in
the active repair owner. Git history retains the old row descriptions without
making them current project structure.

## Reference Baseline

YVEX studies current primary implementations to understand proven ownership
boundaries. This baseline is research input, not a dependency, compatibility
claim, public API commitment, Python process model, serving design, or
distributed-scope commitment.

| Reference | What YVEX studies | What YVEX does not inherit |
| --- | --- | --- |
| [vLLM architecture overview](https://docs.vllm.ai/en/stable/design/arch_overview/) | Engine lifecycle, scheduler/KV separation, worker ownership, model runners, memory management, and backend interfaces | vLLM public APIs, process topology, scheduler policy, or serving scope |
| [SGLang runtime](https://github.com/sgl-project/sglang/tree/main/python/sglang/srt) and [DeepSeek-V4 implementation](https://github.com/sgl-project/sglang/blob/main/python/sglang/srt/models/deepseek_v4.py) | Runtime decomposition and current DS4 model, attention, KV-memory, MoE, kernel, and prefill/decode integration boundaries | SGLang APIs, Python runtime, distributed design, or support claims |
| [GGUF specification](https://github.com/ggml-org/ggml/blob/master/docs/gguf.md), [ggml](https://github.com/ggml-org/ggml), and [llama.cpp](https://github.com/ggml-org/llama.cpp) | GGUF container semantics, qtype geometry, quantization references, native artifact loading, and C/C++ ownership patterns | Binary compatibility beyond YVEX-tested ABI or llama.cpp model/backend support |
| [TensorRT-LLM architecture](https://nvidia.github.io/TensorRT-LLM/architecture/overview.html), [TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM), and [CUTLASS](https://github.com/NVIDIA/cutlass) | NVIDIA runtime/worker/backend separation, Blackwell execution patterns, fused operations, MoE, and kernel specialization | TensorRT engines, Python APIs, deployment topology, or automatic backend support |
| [DeepSeek-V4-Flash model card and report](https://huggingface.co/deepseek-ai/DeepSeek-V4-Flash) and [FlashMLA](https://github.com/deepseek-ai/FlashMLA) | Exact DS4 architecture, tokenizer/config facts, hybrid attention, compressed KV behavior, MoE, mHC, and reference kernels | Unverified assumptions, external-runner proof, or direct support promotion |

`V010.DOCS.ARCHITECTURE.0` owns the detailed documentation taxonomy and the
paper/reference-to-YVEX implementation map. This section only establishes the
baseline that row must verify and specialize.

## Release Gates

| Gate | Required evidence | Current state |
| --- | --- | --- |
| Source | Exact local source identity, revision, config, tokenizer, shard inventory, and payload-readable tensor index | blocked |
| Architecture | Typed DeepSeek-V4-Flash specification covering attention, position rules, KV, mHC, MoE, norms, output head, and tokenizer | blocked |
| Mapping | Complete source-to-runtime role map and GGUF name/layout map with no unresolved required role | blocked |
| Artifact | Complete YVEX-produced GGUF with required metadata, qtypes, global layout integrity, identity, and writer-reader equivalence | blocked |
| Materialization | Every required tensor admitted, materialized, placed, and released under an explicit DGX Spark residency plan | unsupported |
| Descriptor | Artifact facts project to an execution-complete, family-correct runtime descriptor | unsupported |
| CUDA | Every required operation is truthful, fail-closed, reference-compared, and free of advertised no-op fallback | unsupported |
| Transformer | Full prefill, attention, position, KV writes/reads, MoE routing, experts, repeated layers, and final norm execute | unsupported |
| Text generation | Tokenizer, output head, vocabulary logits, sampling, autoregressive decode, stop policy, and detokenization compose | unsupported |
| Evaluation | Repeatable quality, correctness, regression, and refusal cases run over the release path | blocked |
| Benchmark | Reproducible measurements name machine, artifact identity, qtype, prompt/context, run count, timing, and memory | not-measured |
| Operator | One truthful command invokes the release path and exposes precise refusal, cancellation, partial-output, and cleanup behavior | blocked |
| Release | Full validation, artifact guardrail, claim audit, operator transcript, version record, and every prior gate pass | blocked |

A gate changes state only through its owning conclusive milestone and executable
proof. A report, command, fixture, selected slice, or documentation change
cannot promote it.

## Version Sequence

| Version | Contract |
| --- | --- |
| v0.1.0 | DeepSeek-V4-Flash text generation from a complete YVEX-produced GGUF on DGX Spark CUDA. |
| v0.1.x | Correctness and operational hardening of the same supported path; no implicit family or backend expansion. |
| v0.2.0 | Additional model or backend scope only after an explicit target decision and complete gates for that scope. |
| Later | Serving, portability, distributed execution, acceleration, and broader family work remain uncommitted until separately scoped. |

## Explicit Non-Claims

YVEX does not currently claim:

- a supported DeepSeek-V4-Flash target;
- a complete or supported DeepSeek-V4-Flash model artifact;
- complete GGUF artifact or qtype ABI closure;
- source payload conversion or quantization completion;
- full materialization or residency;
- an executable DeepSeek runtime descriptor;
- complete transformer execution;
- attention-backed KV prefill or decode;
- output-head vocabulary logits or model-backed sampling;
- autoregressive DeepSeek text generation;
- CUDA model generation;
- evaluation readiness;
- measured benchmark results;
- release readiness;
- support for another model family or backend in v0.1.0.

Proof artifacts, primitive comparisons, CLI grammar, reports, help text,
fixtures, external GGUF files, and external runner output remain non-closing
evidence.

## Documentation Ownership

| Document | Owner contract |
| --- | --- |
| `PROJECT.md` | Living project control: target, truth, architecture, tracks, milestones, evidence, dependencies, gates, and Active Next. |
| `AGENTS.md` | Persistent repository, ownership, implementation, testing, validation, claim, and project-control invariants. |
| `docs/repair/v010-foundation-closure.md` | Temporary priority-blocking foundation sequence, recovery disposition, and decommission map. |
| `docs/v010-release-doctrine.md` | Exact release meaning and gate-closure semantics. |
| `docs/system-target.md` | Filesystem architecture and module ownership. |
| `docs/model-families.md` | Normative family-integration architecture, not delivery progress. |
| `docs/contract.md` | Runtime, CLI, lifecycle, and claim contracts for implemented surfaces. |
| `docs/api.md` | Public C API facts and lifetime boundaries. |
| `MODEL_ARTIFACTS.md` | Canonical artifact admission and terminology contract. |
| `docs/operator-runbook.md` | Current implemented operator workflows and refusals. |
| `docs/runbooks/deepseek.md` | Short current-state boundary for the exact DeepSeek target. |
| `docs/topology-closure-audit.md` | Point-in-time topology evidence; it does not set project state. |

No other document owns project milestones, Active Next, the track registry, or
release-path capability state.
