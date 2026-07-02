# YVEX Inner Delivery Spine

Date: 2026-06-28
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 0. Spine Dashboard

YVEX is a local-first inference engine, not a chat wrapper.

```text
Current highest implemented runtime stage:
  bounded diagnostic generation

Current runtime is:
  diagnostic-runtime

Current full-runtime state:
  unsupported

Current full DeepSeek generation:
  unsupported

Current benchmark status:
  not measured

Active implementation next:
  MOE.CLASS.0 - MoE model-class report

Current release target:
  v0.1.0 - first honest full-runtime path

Primary pressure targets:
  DeepSeek selected-runtime-slice
  GLM huge source/storage
  Qwen/Metal future portability

Main v0.1.0 blocker:
  no full-runtime path yet
```

| Field | Current value |
| --- | --- |
| Project type | inference engine |
| Primary interface | CLI |
| Primary language | C |
| Primary platform | Linux + CUDA |
| Current highest runtime stage | bounded diagnostic generation |
| Full-runtime transformer prefill | unsupported |
| Full-runtime KV writes/reads | unsupported |
| Full-runtime decode | unsupported |
| Output-head logits | unsupported |
| Vocabulary sampling | unsupported |
| Full model generation | unsupported |
| DeepSeek generation | unsupported |
| Eval/benchmark | unsupported / not measured |
| Active next | MOE.CLASS.0 |

## 1. Nomenclature and Reading Model

Tracks are the clean forward map. Blocks are ownership. Rows are delivery units.
The ledger is history. SPINE rows are meta-control. V010 rows are v0.1.0
forward work.

| Term | Meaning | Example | Primary section | Runtime capability? |
| --- | --- | --- | --- | --- |
| block | Canonical ownership domain. | BLOCK 7 Runtime State. | Doctrine Appendix / Ownership Blocks. | No by itself. |
| track | Forward architecture lane. | TRACK.RUNTIME.KV. | Forward Track Matrix. | Only through completed implementation rows. |
| row | Smallest delivery boundary. | MOE.CLASS.0. | Ledger or Track Detail. | Yes if implementation row; no if docs/meta/report-only. |
| wave | Work package sent to Codex/AI. | SPINE.REDESIGN.0. | Delivery reports / ledger. | Depends on row type. |
| ledger | Historical delivery record. | Implementation Delivery Ledger. | Implementation Delivery Ledger. | No by itself. |
| track matrix | Primary forward architecture table. | Forward Track Matrix. | Forward Track Matrix. | No by itself. |
| gate | Required release or evidence condition. | GATE.KV. | v0.1.0 Critical Path. | No by itself. |
| claim | Capability statement that must be evidence-backed. | bounded diagnostic generation exists. | Current Implementation State. | Only if backed by implementation evidence. |
| boundary | Explicit non-claim. | not full model generation. | Boundary Registry. | No; it prevents overclaim. |
| meta-spine row | Docs/control row. | SPINE.AUDIT.0. | Spine Governance Waves. | No. |
| implementation row | Code behavior row. | GEN.APPEND.0. | Implementation Delivery Ledger. | Yes. |
| report row | Command-visible report row. | CONTEXT.CLASS.0. | Implementation Delivery Ledger / Track Detail. | Inspection only. |

## 2. Runtime Architecture Map

This section is the central engine view. It shows the execution pipeline and the
current evidence stage for each runtime step.

### 2.1 Engine Pipeline

```text
official source tensors
-> source manifest
-> native tensor inventory
-> tensor mapping
-> quantization/artifact production
-> YVEX-produced artifact
-> artifact identity/integrity
-> model-family mapping
-> model class
-> tensor collections
-> residency/storage
-> backend capability
-> graph execution
-> context planning
-> full-runtime prefill
-> KV
-> decode
-> logits
-> sampling
-> tokenizer/stop policy
-> generation
-> CLI/operator surface
-> serving
-> evaluation
-> benchmark/profile evidence
-> release
```

### 2.2 Runtime Architecture Status Table

| Stage | Purpose | Current stage | Implemented? | Current proof | Next gap |
| --- | --- | --- | --- | --- | --- |
| official source tensors | upstream source authority | report-only | partial | target records and source manifests | multi-family source profile |
| source manifest | provenance and source footprint | implemented | yes | `yvex source-manifest` | larger source coverage |
| native tensor inventory | source tensor directory without payload loading | implemented | yes | `yvex native-weights` | huge shard indexing |
| tensor mapping | map source/artifact tensor names to YVEX roles | partial/report-only | partial | tensor-map and family-runtime reports | final runtime role coverage |
| artifact production | produce controlled or selected GGUF | selected-slice | partial | controlled/selected GGUF emission | full-runtime artifact production |
| artifact identity/integrity | digest, range, shape/dtype, corruption refusal | implemented | yes | integrity reports and tests | full-runtime artifact gate |
| model-family mapping | family adapter facts and blockers | report-only | partial | `yvex fullmodel family-runtime` | MoE class facts |
| model class | dense/MoE/source-only/selected-slice routing | report-only | partial | fullmodel descriptor reports | `MOE.CLASS.0` |
| tensor collections | embedding/norm/attention/MLP/MoE/output/tokenizer roles | report-only | partial | fullmodel report and descriptor | final runtime tensor map |
| storage/residency | placement, cache, staging, storage-stream planning | report-only | partial | materialization plans | staged residency proof |
| backend capability | CPU/CUDA/future backend capability | partial | partial | CPU/CUDA checks and parity | capability matrix hardening |
| graph primitives | standalone op proof | fixture-proof | yes | `yvex graph --execute-op` | full transformer graph |
| full transformer graph | execute target model block/layers | planned | no | none | QKV/O, attention, MLP/MoE over target tensors |
| context planning | requested/active context, chunking, overflow, decode position | report-only | yes | `yvex context report` | connect to full-runtime prefill |
| full-runtime prefill | build runtime state from transformer path | unsupported | no | none | attention-backed KV writes |
| KV cache | K/V runtime state | diagnostic/report-only | partial | `yvex kv report`, diagnostic KV | full-runtime K/V writes and decode reads |
| decode | advance runtime one token | diagnostic-runtime | partial | `yvex decode` | decode over full-runtime KV |
| logits | output token scores | diagnostic-runtime | partial | `yvex logits` | output-head logits |
| sampling | choose next token | diagnostic-runtime | partial | `yvex sample` | vocabulary sampling |
| tokenizer/stop | tokenizer metadata, detokenization, EOS/stop policy | planned | partial | tokenizer diagnostics | tokenizer-backed stop behavior |
| generation loop | repeated decode/logits/sample/append | diagnostic-runtime | partial | `yvex generate` | full-runtime generation |
| CLI generation | operator command over generation path | diagnostic-runtime | partial | `yvex generate` help and runbook | full-runtime normal command |
| serving | daemon/provider generation surface | planned | no | `yvexd` status shell | runtime-backed provider generation |
| evaluation | correctness/capability evidence | planned | no | none | runtime eval path |
| benchmark/profile | measured evidence | planned | no | none | runtime benchmark harness |
| speculative acceleration | draft/verify/accept acceleration | post-v0.1.0 | no | doctrine only | baseline generation first |
| release | versioned release gate and transcript | planned | no | v0.1.0 gates | final validation transcript |

### 2.3 Dense vs MoE Runtime Split

| Model class | Runtime path | Current state | Next required evidence |
| --- | --- | --- | --- |
| dense | embedding -> norm -> attention -> dense MLP -> output head -> logits -> sampling -> generation | planned | dense target decision / dense class report |
| MoE | embedding -> norm -> attention -> router -> expert selection -> dispatch -> accumulation -> output head -> logits -> sampling -> generation | report-only next | MOE.CLASS.0 |
| source-only | source/intake/storage/model-class only | GLM pressure lane | source inventory / storage stream |
| selected-runtime-slice | parser/materialization/partial graph/diagnostic runtime only | DeepSeek selected artifacts | does not close full runtime |
| Metal/Qwen | backend portability lane | docs/planned | hardware/backend/source target rows |

## 3. Current Implementation State

This section is the single front-door view of current capability. Detailed
historical bullets are preserved below as an archive, but this table is the
operator-readable state.

### 3.1 Capability Summary Table

| Capability group | Stage | Implemented? | Main proof | Boundary |
| --- | --- | --- | --- | --- |
| artifact/integrity | implemented | yes | inspect/integrity/materialize tests | not runtime |
| selected graph | selected-slice | yes | graph selected/segment commands | not full transformer |
| graph primitives | fixture-proof | yes | graph op/block/layers commands | not full model runtime |
| prefill | diagnostic-runtime | partial | `yvex prefill` | not full transformer prefill |
| KV | diagnostic/report-only | partial | `yvex kv report`, diagnostic KV | not attention-backed KV |
| decode | diagnostic-runtime | partial | `yvex decode` | not full-runtime decode |
| logits | diagnostic-runtime | partial | `yvex logits` | not output-head logits |
| sampling | diagnostic-runtime | partial | `yvex sample` | not vocabulary sampling |
| generation | diagnostic-runtime | partial | `yvex generate` | not full model generation |
| fullmodel reports | report-only/selected-slice proof | yes | `yvex fullmodel ...` | not execution |
| attention/context/KV class | report-only | yes | `yvex attention/kv/context report` | not runtime execution |
| operator paths/presets | operator-preset | yes | paths/model-target/models prepare/check | not extra runtime capability |
| docs/meta | docs-only | yes | spine rows | not runtime capability |

### 3.2 Implemented Artifact and Integrity Capability

YVEX has implemented GGUF inspection, tensor table parsing, artifact identity,
range validation, corruption refusal, registry drift diagnostics, materialize
integrity gates, and operator integrity reports over controlled fixtures and
selected artifacts.

### 3.3 Implemented Graph and Selected-Slice Capability

YVEX has implemented standalone graph primitives, controlled block/layer
fixtures, selected embedding materialization, selected embedding graph
execution, and selected embedding-plus-RMSNorm segment execution. These are
fixture-proof and selected-slice proofs, not a full transformer runtime.

### 3.4 Implemented Diagnostic Runtime Capability

YVEX has implemented token input, segment/chunk diagnostic prefill, diagnostic
KV binding, bounded diagnostic decode, bounded diagnostic logits, bounded greedy
sampling, diagnostic generation append/stop/cleanup, trace, cancellation, and
CLI `yvex generate` over diagnostic state.

### 3.5 Implemented Report-Only Capability

YVEX reports fullmodel inventory/materialization plans/descriptors,
family-runtime facts, attention class facts, KV class facts, and context class
facts. Report-only capability increases inspection but does not execute runtime
semantics.

### 3.6 Implemented Operator Capability

YVEX exposes operator-local path configuration, model target path reporting,
selected artifact prepare/check presets, graph check presets, bounded generate
help/runbook coverage, and clear unsupported boundaries.

### 3.7 Implemented Docs/Meta Capability

The spine records source-tensor doctrine, canonical blocks, v0.1.0 doctrine,
generation/speculative targets, audit/reconciliation rows, Qwen/Metal pressure
lane, navigation, and this track-based redesign. These rows are project-control
work, not runtime capability.

### 3.8 Unsupported Boundary Registry

The unsupported registry lives in `## 12. Boundary Registry`. The front summary
is: no full-runtime generation, no DeepSeek generation, no provider generation,
no evaluation, no benchmark, and no throughput claim.

### 3.9 Current Target Classes

| Target class | Current target | Current state | Boundary |
| --- | --- | --- | --- |
| selected-runtime-slice | DeepSeek selected embed / embed+RMSNorm | implemented selected-slice proofs | not full runtime |
| huge source/storage | GLM-5.2 official safetensors | source/storage pressure lane | no GLM execution |
| future portability | Qwen/Metal | docs/planned | no Metal/Qwen runtime |
| full-runtime model | v0.1.0 selected target | planned | target decision and runtime path pending |

## 4. Forward Track Matrix

This is the main operational map for future work. Tracks are semantic planning
lanes; rows are the delivery units that complete track work.

| Track ID | Track name | Owns | Current status | Implemented evidence | Next gap | Active / Later |
| --- | --- | --- | --- | --- | --- | --- |
| TRACK.TARGET | Target Decision | target classes, pressure objects, release target selection | partial | target registry and path reports | v0.1.0 target decision | active |
| TRACK.SOURCE | Source Intake | official source tensors, source manifests, native tensor inventory, source-only pressure lanes | partial | source-manifest and native-weights commands | multi-family source profile and huge-source indexing | active |
| TRACK.ARTIFACT | Artifact Production | conversion planning, tensor mapping, quant policy, YVEX-produced GGUF emission | selected-slice | controlled and selected GGUF emission | full-runtime artifact production | later |
| TRACK.INTEGRITY | Artifact Identity and Integrity | digest, byte range, shape/dtype, corruption refusal, materialization and graph admission gates | implemented | integrity reports, corruption harness, materialization gate, graph integrity guard | full-runtime acceptance gate coverage | active |
| TRACK.MODEL | Model Class and Runtime Routing | dense/MoE/source-only/selected-slice routing, runtime requirements, output/tokenizer blockers | partial/report-only | fullmodel descriptor, family-runtime, attention, KV, and context reports | MoE model-class facts and final runtime route | active |
| TRACK.TENSOR | Tensor Collections | embedding, norm, attention, MLP/MoE, output, tokenizer, and runtime token tensor roles | partial/report-only | fullmodel descriptor/report and tensor role coverage | final runtime tensor map and missing-role blockers | active |
| TRACK.STORAGE | Storage Stream | shard index, cold/warm reads, tensor byte ranges, page/chunk policy, cache and cleanup | planned/report-only | storage doctrine and placement plans | shard index and cold/warm read proof | later |
| TRACK.RESIDENCY | Residency | resident, host-staged, SSD-staged, SSD-streamed, managed-memory, hybrid, and future distributed placement | report-only | materialization and placement plans | staged residency proof and tensor residency assignments | later |
| TRACK.BACKEND | Backend Capability | CPU/CUDA/future Metal/ROCm op capability, allocation, transfer, build, and hardware profiles | partial | CPU/CUDA probes, movement, parity tests, CUDA optional checks | capability matrix and backend memory pressure reports | active |
| TRACK.GRAPH | Graph Core | standalone primitives, controlled block/layers, selected slices, and target tensor graph path | partial | graph op, block, layer, graph check, selected and segment commands | full transformer graph over target tensors | later |
| TRACK.ATTENTION | Attention Runtime | Q/K/V/O source roles, RoPE/position, masks, head layout, attention execution, and KV interaction | report-only | attention report and standalone attention primitive | target tensor Q/K/V/O path and attention-backed KV writes | later |
| TRACK.MOE | MoE Runtime Path | MoE model-class facts, router facts, experts, shared experts, top-k, activation, dispatch, accumulation, and MoE runtime integration | planned/report-only next | DeepSeek family-runtime reports MoE requirements; GRAPH.OPS.3 has routed expert-slice primitive | expert/router/shared-expert facts | active |
| TRACK.CONTEXT | Context Planning | requested/active context, chunking, overflow, decode position, prompt/prefill boundaries | report-only | context report with token counts, active context, chunking and overflow fields | connect planning to full-runtime prefill | active |
| TRACK.PREFILL | Prefill Runtime | transition from token/context input into runtime state before decode | diagnostic-runtime only | M8, PREFILL.1, PREFILL.2, PREFILL.3 | full transformer prefill with attention-backed KV writes | later |
| TRACK.KV | KV Runtime | KV layout, dtype/qtype, capacity, allocation, writes, reads, indexing, residency, trace, and cleanup | diagnostic/report-only | M9, PREFILL.1, KV.CACHE.0, diagnostic KV binding and KV report | attention-backed KV write/read and capacity planning | later |
| TRACK.DECODE | Decode Runtime | one-step and repeated decode over runtime state and KV | diagnostic-runtime | DECODE.0 bounded diagnostic decode | decode over attention-backed KV | later |
| TRACK.LOGITS | Output Head and Logits | final hidden state, output-head projection, logits buffer, top-k/logprob diagnostics | diagnostic-runtime | LOGITS.0 bounded diagnostic logits | output-head logits over model-backed hidden state | later |
| TRACK.SAMPLING | Sampling Runtime | greedy, stochastic, top-k/top-p/min-p/typical, seed policy, and sampling diagnostics | diagnostic-runtime | SAMPLING.0 bounded greedy sampler | sampling over output-head logits and broader strategy surface | later |
| TRACK.TOKENIZER | Tokenizer and Stop Policy | detokenization, EOS, stop-token matching, tokenizer metadata, prompt formatting | partial/planned | tokenizer fixture and prompt diagnostics; unsupported EOS/stop policy boundary | tokenizer-backed stop behavior and output text boundary | later |
| TRACK.GENERATION | Generation Runtime | composition of decode, logits, sampling, append, stop, failure, and cleanup | diagnostic-runtime only | GEN.CONTRACT.0, GEN.TRACE.0, GEN.APPEND.0, GEN.STOP.0, GEN.LOOP.0, GEN.LOOP.1, CLI.GEN.0 | model-backed decode, output-head logits, vocabulary sampling, tokenizer stop policy | later |
| TRACK.RUNTIME | Runtime Lifecycle and Trace | state lifecycle, cancellation, trace, cleanup, failure preservation, interruption surfaces | diagnostic-runtime | generate trace/cancel/cleanup records and lifecycle fields | external interruption and runtime trace hardening | later |
| TRACK.OPERATOR | Operator CLI | paths, targets, prepare/check, graph check, doctor, normal command transcript, REPL clarity | partial | paths/model-target/models prepare/check, graph check, generate help, operator runbooks | final normal path transcript and diagnostic REPL clarity | active |
| TRACK.SERVE | Serving | daemon state, provider endpoints, streaming, observability, compatibility layers | planned | yvexd status shell | runtime-backed provider generation | later |
| TRACK.EVAL | Evaluation | fixture/runtime/generation/capability evaluation over implemented paths | planned | test runners only | eval over generation path | later |
| TRACK.BENCH | Benchmark/Profile | reproducible performance harness, profile output, machine/artifact metadata, throughput reporting | planned | benchmark doctrine only | measured runtime harness with reproducibility metadata | later |
| TRACK.SPEC | Speculative Acceleration | draft, target verification, accepted-token accounting, routing-aware verification cost, speedup measurement | post-v0.1.0 | DSpark/speculative doctrine only | baseline generation and target verification first | post-v0.1.0 |
| TRACK.DOCS | Documentation and Public Evidence | public/internal docs, diagrams, runbooks, claim hygiene, paper/algorithm docs | partial | spine, contract, API docs, runbooks, README boundary | release-safe public evidence after runtime proof | active |
| TRACK.RELEASE | Release | version, package, CI gate, transcript, tag readiness | planned | v0.1.0 doctrine and gates | final validation transcript and tag readiness | later |
| TRACK.CI | CI and Guardrails | tests, docs/source surface checks, artifact scans, forbidden claim scans, validation bundle | partial | make check/smoke, docs/source/surface tests, artifact guardrails | release validation bundle and row-to-test map | active |
| TRACK.POST010 | Post-v0.1.0 Lanes | serving compatibility, public benchmark tables, speculative acceleration, portability expansion | post-v0.1.0 | doctrine only | v0.1.0 first | post-v0.1.0 |

## 5. Track Detail Sheets

Track detail sheets are the only forward implementation detail. Each sheet maps
the semantic track to ownership, current evidence, absorbed legacy rows, forward
rows, next gap, next row, and boundary. Former runtime sequences A-I and
numbered V010 tracks were collapsed into these sheets by SPINE.COLLAPSE.0.

### TRACK.TARGET — Target Decision

Owns:
  target classes, pressure objects, release target selection.

Current state:
  partial.

Current evidence:
  target registry and path reports.

Absorbed legacy rows:
  V010.SCOPE.*, V010.TARGET.*, OWI.TARGETS.*.

Forward rows:
  V010.SCOPE.*, V010.TARGET.*.

Next gap:
  v0.1.0 target decision.

Next row:
  V010.TARGET.9.

Boundary:
  a target is not a capability claim.

### TRACK.SOURCE — Source Intake

Owns:
  official source tensors, source manifests, native tensor inventory, source-only pressure lanes.

Current state:
  partial.

Current evidence:
  source-manifest and native-weights commands.

Absorbed legacy rows:
  OWI.*, OWI.HUGE.*, V010.SOURCE.*.

Forward rows:
  V010.SOURCE.*.

Next gap:
  multi-family source profile and huge-source indexing.

Next row:
  OWI.TARGETS.1 or V010.SOURCE.*.

Boundary:
  source inventory is not model execution.

### TRACK.ARTIFACT — Artifact Production

Owns:
  conversion planning, tensor mapping, quant policy, YVEX-produced GGUF emission.

Current state:
  selected-slice.

Current evidence:
  controlled and selected GGUF emission.

Absorbed legacy rows:
  OWI.3-9, OWI.FINAL.0, ARTIFACT.NAMING.0, V010.MAP.*, V010.QUANT.*, V010.ARTIFACT.EMIT.*.

Forward rows:
  V010.MAP.*, V010.QUANT.*, V010.ARTIFACT.EMIT.*.

Next gap:
  full-runtime artifact production.

Next row:
  V010.ARTIFACT.EMIT.* after target/source decisions.

Boundary:
  selected artifact production is not full runtime.

### TRACK.INTEGRITY — Artifact Identity and Integrity

Owns:
  digest, byte range, shape/dtype, corruption refusal, materialization and graph admission gates.

Current state:
  implemented.

Current evidence:
  integrity reports, corruption harness, materialization gate, graph integrity guard.

Absorbed legacy rows:
  ARTIFACT.INTEGRITY.*, FULLMODEL.0-2, V010.INTEGRITY.*, V010.FULLMODEL.*.

Forward rows:
  V010.INTEGRITY.*, V010.FULLMODEL.*.

Next gap:
  full-runtime acceptance gate coverage.

Next row:
  V010.INTEGRITY.* or V010.FULLMODEL.* as selected by target.

Boundary:
  integrity proof is not execution.

### TRACK.MODEL — Model Class and Runtime Routing

Owns:
  dense/MoE/source-only/selected-slice routing, runtime requirements, output/tokenizer blockers.

Current state:
  partial/report-only.

Current evidence:
  fullmodel descriptor, family-runtime, attention, KV, and context reports.

Absorbed legacy rows:
  MODEL.CLASS.*, FAMILY.RUNTIME.0, FULLMODEL.3, V010.CLASS.*.

Forward rows:
  V010.CLASS.*.

Next gap:
  MoE model-class facts and final runtime route.

Next row:
  MOE.CLASS.0.

Boundary:
  model-class reports are not runtime execution.

### TRACK.TENSOR — Tensor Collections

Owns:
  embedding, norm, attention, MLP/MoE, output, tokenizer, and runtime token tensor roles.

Current state:
  partial/report-only.

Current evidence:
  fullmodel descriptor/report and tensor role coverage.

Absorbed legacy rows:
  TENSOR.COLLECTION.*, V010.TENSOR.*.

Forward rows:
  V010.TENSOR.*.

Next gap:
  final runtime tensor map and missing-role blockers.

Next row:
  V010.TENSOR.14-17 when MoE facts are exposed.

Boundary:
  tensor collection reports are not model support.

### TRACK.STORAGE — Storage Stream

Owns:
  shard index, cold/warm reads, tensor byte ranges, page/chunk policy, cache and cleanup.

Current state:
  planned/report-only.

Current evidence:
  storage doctrine and placement plans.

Absorbed legacy rows:
  STORAGE.STREAM.*, OWI.HUGE.*, V010.STORAGE.*.

Forward rows:
  V010.STORAGE.*.

Next gap:
  shard index and cold/warm read proof.

Next row:
  V010.STORAGE.* after source/artifact selection.

Boundary:
  storage streaming is not generation.

### TRACK.RESIDENCY — Residency

Owns:
  resident, host-staged, SSD-staged, SSD-streamed, managed-memory, hybrid, and future distributed placement.

Current state:
  report-only.

Current evidence:
  materialization and placement plans.

Absorbed legacy rows:
  RESIDENCY.*, RUNTIME.KV.*, V010.RESIDENCY.*.

Forward rows:
  V010.RESIDENCY.*.

Next gap:
  staged residency proof and tensor residency assignments.

Next row:
  V010.RESIDENCY.*.

Boundary:
  residency plans are not runtime execution.

### TRACK.BACKEND — Backend Capability

Owns:
  CPU/CUDA/future Metal/ROCm op capability, allocation, transfer, build, and hardware profiles.

Current state:
  partial.

Current evidence:
  CPU/CUDA probes, movement, parity tests, CUDA optional checks.

Absorbed legacy rows:
  L0, CUDA.SURFACE.0, COMPUTE.BACKEND.*, BACKEND.PROFILE.*, HARDWARE.PROFILE.*, BUILD.PROFILE.*, V010.BACKEND.*, V010.BUILD.*, V010.HARDWARE.*.

Forward rows:
  V010.BACKEND.*, V010.BUILD.*, V010.HARDWARE.*.

Next gap:
  capability matrix and backend memory pressure reports.

Next row:
  V010.BACKEND.*.

Boundary:
  backend capability is not model support.

### TRACK.GRAPH — Graph Core

Owns:
  standalone primitives, controlled block/layers, selected slices, and target tensor graph path.

Current state:
  partial.

Current evidence:
  graph op, block, layer, graph check, selected and segment commands.

Absorbed legacy rows:
  GRAPH.OPS.*, GRAPH.BLOCK.0, GRAPH.LAYERS.0, GRAPH.CHECK.0, M4-M6, V010.GRAPH.PRIM.*, V010.GRAPH.*.

Forward rows:
  V010.GRAPH.PRIM.*, V010.GRAPH.*.

Next gap:
  full transformer graph over target tensors.

Next row:
  V010.GRAPH.1-24 after tensor/backend requirements.

Boundary:
  graph fixtures are not full transformer runtime.

### TRACK.ATTENTION — Attention Runtime

Owns:
  Q/K/V/O source roles, RoPE/position, masks, head layout, attention execution, and KV interaction.

Current state:
  report-only.

Current evidence:
  attention report and standalone attention primitive.

Absorbed legacy rows:
  ATTENTION.CLASS.0, ATTENTION.MODES.0, GRAPH.OPS.1, V010.ATTN.*.

Forward rows:
  V010.ATTN.*.

Next gap:
  target tensor Q/K/V/O path and attention-backed KV writes.

Next row:
  V010.ATTN.* after tensor collection coverage.

Boundary:
  attention report/primitive is not full attention runtime.

### TRACK.MOE — MoE Runtime Path

Owns:
  MoE model-class facts, router facts, experts, shared experts, top-k, activation, dispatch, accumulation, and MoE runtime integration.

Current state:
  planned/report-only next.

Current evidence:
  DeepSeek family-runtime reports MoE requirements; GRAPH.OPS.3 has routed expert-slice primitive.

Absorbed legacy rows:
  MOE.CLASS.0, MOE.ACT.0, MODEL.CLASS.1, SPEC.MOE.*, V010.CLASS.3, V010.TENSOR.14-17, V010.RESIDENCY.14, V010.MOE.*, V010.PREFILL.7, V010.DECODE.6.

Forward rows:
  V010.CLASS.3, V010.TENSOR.14-17, V010.RESIDENCY.14, V010.MOE.0-20.

Next gap:
  expert/router/shared-expert facts.

Next row:
  MOE.CLASS.0 - MoE model-class report.

Boundary:
  routed expert-slice primitive is not MoE runtime.

### TRACK.CONTEXT — Context Planning

Owns:
  requested/active context, chunking, overflow, decode position, prompt/prefill boundaries.

Current state:
  report-only.

Current evidence:
  context report with token counts, active context, chunking and overflow fields.

Absorbed legacy rows:
  CONTEXT.CLASS.0, V010.CONTEXT.*.

Forward rows:
  V010.CONTEXT.*.

Next gap:
  connect planning to full-runtime prefill.

Next row:
  V010.CONTEXT.* when runtime path needs context hardening.

Boundary:
  context report is not long-context support.

### TRACK.PREFILL — Prefill Runtime

Owns:
  transition from token/context input into runtime state before decode.

Current state:
  diagnostic-runtime only.

Current evidence:
  M8, PREFILL.1, PREFILL.2, PREFILL.3.

Absorbed legacy rows:
  M8, PREFILL.1-5, PREFILL.MODES.0, EVAL.PREFILL.0, BENCH.PREFILL.0, V010.PREFILL.*.

Forward rows:
  V010.PREFILL.*.

Next gap:
  full transformer prefill with attention-backed KV writes.

Next row:
  blocked until model/tensor/graph/KV prerequisites expose the correct path.

Boundary:
  segment/chunk diagnostic prefill is not full transformer prefill.

### TRACK.KV — KV Runtime

Owns:
  KV layout, dtype/qtype, capacity, allocation, writes, reads, indexing, residency, trace, and cleanup.

Current state:
  diagnostic/report-only.

Current evidence:
  M9, PREFILL.1, KV.CACHE.0, diagnostic KV binding and KV report.

Absorbed legacy rows:
  KV.MIN.*, RUNTIME.KV.*, KV.CACHE.0, KV.MODES.0, V010.KV.*.

Forward rows:
  V010.KV.*.

Next gap:
  attention-backed KV write/read and capacity planning.

Next row:
  V010.KV.* after prefill/attention requirements.

Boundary:
  diagnostic KV is not attention-backed KV.

### TRACK.DECODE — Decode Runtime

Owns:
  one-step and repeated decode over runtime state and KV.

Current state:
  diagnostic-runtime.

Current evidence:
  DECODE.0 bounded diagnostic decode.

Absorbed legacy rows:
  M10, DECODE.0-1, DECODE.MODES.0, EVAL.DECODE.0, BENCH.DECODE.0, V010.DECODE.*.

Forward rows:
  V010.DECODE.*.

Next gap:
  decode over attention-backed KV.

Next row:
  V010.DECODE.3-8 after prefill/KV path exists.

Boundary:
  diagnostic decode is not full-runtime decode.

### TRACK.LOGITS — Output Head and Logits

Owns:
  final hidden state, output-head projection, logits buffer, top-k/logprob diagnostics.

Current state:
  diagnostic-runtime.

Current evidence:
  LOGITS.0 bounded diagnostic logits.

Absorbed legacy rows:
  M11-M12, LOGITS.0-1, LOGITS.MODES.0, EVAL.LOGITS.0, V010.LOGITS.*.

Forward rows:
  V010.LOGITS.*.

Next gap:
  output-head logits over model-backed hidden state.

Next row:
  V010.LOGITS.* after decode and output-head tensor path.

Boundary:
  diagnostic logits are not output-head logits.

### TRACK.SAMPLING — Sampling Runtime

Owns:
  greedy, stochastic, top-k/top-p/min-p/typical, seed policy, and sampling diagnostics.

Current state:
  diagnostic-runtime.

Current evidence:
  SAMPLING.0 bounded greedy sampler.

Absorbed legacy rows:
  M13, SAMPLING.0-1, SAMPLING.MODES.0, EVAL.SAMPLING.0, V010.SAMPLE.*.

Forward rows:
  V010.SAMPLE.*.

Next gap:
  sampling over output-head logits and broader strategy surface.

Next row:
  V010.SAMPLE.* after logits path exists.

Boundary:
  sampling over diagnostic logits is not generation quality.

### TRACK.TOKENIZER — Tokenizer and Stop Policy

Owns:
  detokenization, EOS, stop-token matching, tokenizer metadata, prompt formatting.

Current state:
  partial/planned.

Current evidence:
  tokenizer fixture and prompt diagnostics; unsupported EOS/stop policy boundary.

Absorbed legacy rows:
  D0, tokenizer diagnostics, V010.TOKENIZER.*.

Forward rows:
  V010.TOKENIZER.*.

Next gap:
  tokenizer-backed stop behavior and output text boundary.

Next row:
  V010.TOKENIZER.*.

Boundary:
  tokenizer diagnostics are not quality generation.

### TRACK.GENERATION — Generation Runtime

Owns:
  composition of decode, logits, sampling, append, stop, failure, and cleanup.

Current state:
  diagnostic-runtime only.

Current evidence:
  GEN.CONTRACT.0, GEN.TRACE.0, GEN.APPEND.0, GEN.STOP.0, GEN.LOOP.0, GEN.LOOP.1, CLI.GEN.0.

Absorbed legacy rows:
  M14-M17, GEN.*, GENERATION.MODES.0, CLI.GEN.0, SERVER.GEN.0, V010.GEN.*, V010.RUNTIME.*.

Forward rows:
  V010.GEN.0-19, V010.RUNTIME.*.

Next gap:
  model-backed decode, output-head logits, vocabulary sampling, tokenizer stop policy.

Next row:
  none until decode/logits/sampling model-backed path exists.

Boundary:
  bounded diagnostic generation is not full model generation.

### TRACK.RUNTIME — Runtime Lifecycle and Trace

Owns:
  state lifecycle, cancellation, trace, cleanup, failure preservation, interruption surfaces.

Current state:
  diagnostic-runtime.

Current evidence:
  generate trace/cancel/cleanup records and lifecycle fields.

Absorbed legacy rows:
  GEN.LOOP.1, GEN.TRACE.0, TENSOR.TRACE.0, M17, V010.RUNTIME.*, V010.TRACE.*.

Forward rows:
  V010.RUNTIME.*, V010.TRACE.*.

Next gap:
  external interruption and runtime trace hardening.

Next row:
  V010.RUNTIME.*.

Boundary:
  diagnostic lifecycle proof is not provider generation.

### TRACK.OPERATOR — Operator CLI

Owns:
  paths, targets, prepare/check, graph check, doctor, normal command transcript, REPL clarity.

Current state:
  partial.

Current evidence:
  paths/model-target/models prepare/check, graph check, generate help, operator runbooks.

Absorbed legacy rows:
  OPERATOR.PATHS.0, MODEL.TARGET.PATHS.0, MODEL.PREPARE.*, MODEL.CHECK.*, GRAPH.CHECK.0, CHAT.UX.0, OPERATOR.FLOW.*, CLI.UX.*, MODEL.LIFECYCLE.*, V010.CLI.*, V010.DOCTOR.*, V010.PATHS.*.

Forward rows:
  V010.CLI.*, V010.DOCTOR.*, V010.PATHS.*.

Next gap:
  final normal path transcript and diagnostic REPL clarity.

Next row:
  MODEL.CHECK.1 or V010.CLI.* as selected.

Boundary:
  operator presets compose lower behavior only.

### TRACK.SERVE — Serving

Owns:
  daemon state, provider endpoints, streaming, observability, compatibility layers.

Current state:
  planned.

Current evidence:
  yvexd status shell.

Absorbed legacy rows:
  K0, SERVE.RUNTIME.0, SERVER.*, V010.SERVE.*.

Forward rows:
  V010.SERVE.*.

Next gap:
  runtime-backed provider generation.

Next row:
  V010.SERVE.* after generation path exists.

Boundary:
  status shell is not serving generation.

### TRACK.EVAL — Evaluation

Owns:
  fixture/runtime/generation/capability evaluation over implemented paths.

Current state:
  planned.

Current evidence:
  test runners only.

Absorbed legacy rows:
  EVAL.*, V010.EVAL.*.

Forward rows:
  V010.EVAL.*.

Next gap:
  eval over generation path.

Next row:
  V010.EVAL.* after implemented runtime path exists.

Boundary:
  eval waits for implemented runtime path.

### TRACK.BENCH — Benchmark/Profile

Owns:
  reproducible performance harness, profile output, machine/artifact metadata, throughput reporting.

Current state:
  planned.

Current evidence:
  benchmark doctrine only.

Absorbed legacy rows:
  BENCH.*, PREFILL.5, V010.BENCH.*, V010.PROFILE.*.

Forward rows:
  V010.BENCH.*, V010.PROFILE.*.

Next gap:
  measured runtime harness with reproducibility metadata.

Next row:
  V010.BENCH.* after runtime path exists.

Boundary:
  no benchmark without measured runtime path.

### TRACK.SPEC — Speculative Acceleration

Owns:
  draft, target verification, accepted-token accounting, routing-aware verification cost, speedup measurement.

Current state:
  post-v0.1.0.

Current evidence:
  DSpark/speculative doctrine only.

Absorbed legacy rows:
  SPEC.*, SPEC.MOE.*, PAPER.INDEX.0, ALGORITHM.MODES.0, CLI.RESEARCH.0, V010.SPEC.*, POST010.SPEC.*.

Forward rows:
  V010.SPEC.*, POST010.SPEC.*.

Next gap:
  baseline generation and target verification first.

Next row:
  POST010.SPEC.* after baseline generation.

Boundary:
  speculative rows cannot precede baseline generation.

### TRACK.DOCS — Documentation and Public Evidence

Owns:
  public/internal docs, diagrams, runbooks, claim hygiene, paper/algorithm docs.

Current state:
  partial.

Current evidence:
  spine, contract, API docs, runbooks, README boundary.

Absorbed legacy rows:
  DOCS.*, DOCS.PUBLIC.*, DOCS.RUNBOOK.*, DOCS.DIAGRAMS.*, PAPER.INDEX.0, V010.DOCS.*, V010.PAPER.*.

Forward rows:
  V010.DOCS.*, V010.PAPER.*.

Next gap:
  release-safe public evidence after runtime proof.

Next row:
  V010.DOCS.*.

Boundary:
  docs do not create runtime capability.

### TRACK.RELEASE — Release

Owns:
  version, package, CI gate, transcript, tag readiness.

Current state:
  planned.

Current evidence:
  v0.1.0 doctrine and gates.

Absorbed legacy rows:
  V010.VERSION.*, V010.PACKAGE.*, V010.RELEASE.*, V010.CI.*, DOCS.README.TARGETS.0.

Forward rows:
  V010.VERSION.*, V010.PACKAGE.*, V010.RELEASE.*, V010.CI.*.

Next gap:
  final validation transcript and tag readiness.

Next row:
  V010.RELEASE.* after gates pass.

Boundary:
  release row cannot create runtime behavior.

### TRACK.CI — CI and Guardrails

Owns:
  tests, docs/source surface checks, artifact scans, forbidden claim scans, validation bundle.

Current state:
  partial.

Current evidence:
  make check/smoke, docs/source/surface tests, artifact guardrails.

Absorbed legacy rows:
  TEST.SURFACE.0, tests surface rows, V010.CI.*, LAYOUT.TEST.*, LAYOUT.BUILD.*.

Forward rows:
  V010.CI.*.

Next gap:
  release validation bundle and row-to-test map.

Next row:
  SPINE.TESTMAP.0 or V010.CI.*.

Boundary:
  CI proves boundaries already implemented.

### TRACK.POST010 — Post-v0.1.0 Lanes

Owns:
  serving compatibility, public benchmark tables, speculative acceleration, portability expansion.

Current state:
  post-v0.1.0.

Current evidence:
  doctrine only.

Absorbed legacy rows:
  POST010.*, SPINE.METAL.QWEN.0, Qwen/Metal rows, future ROCm/serving/spec/benchmark lanes.

Forward rows:
  POST010.*.

Next gap:
  v0.1.0 first.

Next row:
  POST010.* after release target closes.

Boundary:
  post-v0.1.0 tracks cannot block current release unless selected.

## 6. v0.1.0 Critical Path

The v0.1.0 path starts at target decision and ends at release readiness. The
semantic `TRACK.*` model is the only forward planning interface. Old runtime
sequences A-I, old numbered V010 track walls, and old supersession prose are
migration history only.

Former runtime sequences A-I were collapsed into Track Detail Sheets by
SPINE.COLLAPSE.0. Use Track Detail Sheets for forward planning.

| Step | Track | Gate | Current status | Required evidence | Blocks |
| --- | --- | --- | --- | --- | --- |
| 1 | TRACK.TARGET | GATE.SCOPE | planned/partial | target decision report | all v0.1.0 runtime |
| 2 | TRACK.ARTIFACT | GATE.ARTIFACT | selected-slice/partial | artifact identity and integrity proof | full runtime |
| 3 | TRACK.MODEL | GATE.CLASS | partial/report-only | class reports | tensor/runtime path |
| 4 | TRACK.TENSOR | GATE.TENSOR | partial/planned | tensor collection coverage | graph |
| 5 | TRACK.RESIDENCY | GATE.RESIDENCY | report-only/planned | residency plan/proof | graph/runtime |
| 6 | TRACK.BACKEND | GATE.BACKEND | partial | CPU/CUDA capability | graph |
| 7 | TRACK.GRAPH | GATE.GRAPH | selected/fixture | model-backed graph path | prefill |
| 8 | TRACK.PREFILL | GATE.PREFILL | diagnostic | full transformer prefill | KV/decode |
| 9 | TRACK.KV | GATE.KV | diagnostic/report | attention-backed KV write/read | decode |
| 10 | TRACK.DECODE | GATE.DECODE | diagnostic | decode over implemented KV | logits |
| 11 | TRACK.LOGITS | GATE.LOGITS | diagnostic | output-head logits | sampling |
| 12 | TRACK.SAMPLING | GATE.SAMPLE | diagnostic | greedy over output-head logits | generation |
| 13 | TRACK.TOKENIZER | GATE.TOKENIZER | partial/fixture | token/stop boundary | generation quality |
| 14 | TRACK.GENERATION | GATE.GEN | diagnostic | full-runtime generation loop | CLI/eval |
| 15 | TRACK.OPERATOR | GATE.OPERATOR | partial | normal command proof | release transcript |
| 16 | TRACK.EVAL | GATE.EVIDENCE | planned | smoke over implemented path | release |
| 17 | TRACK.RELEASE | GATE.RELEASE | planned | transcript/tag readiness | v0.1.0 |

Critical path text:

```text
target -> artifact -> class -> tensor -> residency/backend -> graph -> prefill
-> KV -> decode -> logits -> sampling -> tokenizer/stop -> generation
-> operator proof -> eval smoke -> CI/docs audit -> release transcript
```

Parallel planning is allowed only when boundaries remain explicit. Generation
cannot precede decode/logits/sampling. Serving cannot precede runtime-backed
generation. Benchmarks cannot precede measured runtime paths.

## 7. Active Next

Active Next:
  MOE.CLASS.0 - MoE model-class report

Reason:
  The report chain has completed FAMILY.RUNTIME.0, ATTENTION.CLASS.0,
  KV.CACHE.0, and CONTEXT.CLASS.0. The MoE branch now needs explicit expert,
  router, shared-expert, and blocker facts before the runtime can choose the
  next MoE/tensor/residency/graph row.

MOE.CLASS.0 must produce:
  expert count
  active expert count
  router facts
  shared expert facts
  expert tensor classes
  MoE storage/residency pressure
  runtime blockers
  next required rows

MOE.CLASS.0 must not claim:
  router execution
  expert activation
  expert dispatch
  expert accumulation
  full transformer prefill
  decode over full-runtime state
  output-head logits
  generation
  benchmark

## 8. Implementation Delivery Ledger

This ledger is the historical implementation and delivery record. It is not the
primary forward roadmap. The primary forward roadmap is `## 4. Forward Track
Matrix` and `## 5. Track Detail Sheets`.

The track model is the forward spine. This ledger preserves delivery history.
Implementation/report/operator/evidence rows stay here. SPINE.* project-control
rows live in `## 9. Spine Governance Waves`.

| ID | Status | Area | Title | Completion boundary |
| --- | --- | --- | --- | --- |
| P0 | complete | foundation | Repository reset and technical spine | root project shape and internal delivery authority established |
| A0 | complete | foundation | Core C skeleton and public status API | root C binary, status surface, and public skeleton compile |
| B0 | complete | foundation | Runtime filesystem and artifact path layer | filesystem helpers and artifact path behavior implemented |
| C0 | complete | gguf | GGUF parser and tensor directory | GGUF metadata and tensor directory parse with failures |
| C1 | complete | gguf | Tensor/model descriptor layer | parsed tensor facts become model descriptors |
| D0 | complete | tokenizer | Tokenizer and prompt diagnostics | tokenizer fixture path and prompt diagnostics exist |
| E0 | complete | graph | Graph and planner substrate | graph/planner structures and diagnostics compile and test |
| F0 | complete | backend | CPU backend and tensor movement | CPU tensor allocation/movement boundary implemented |
| G0 | complete | session | Runtime/session shell | engine/session lifecycle shell exists |
| H0 | complete | cli | CLI diagnostics surface | repository-local CLI exposes diagnostic commands |
| I0 | complete | cli | Chat diagnostic shell | diagnostic chat shell reports unsupported generation honestly |
| J0 | complete | cli | Metrics, traces, and run artifacts | metrics/trace/profile/run-artifact diagnostics exist |
| K0 | complete | server | Server/provider status shell | yvexd health/metrics/models status shell exists |
| L0 | complete | backend | CUDA backend probe and tensor movement | CUDA probe, allocation, movement, and parity subset implemented |
| OWI.0 | complete | intake | Open weight intake and GGUF toolchain spine | source-to-GGUF intake flow mapped |
| OWI.1 | complete | intake | Source manifest provenance contract | source manifest command and contract implemented |
| OWI.2 | complete | intake | Safetensors native weight inventory | native tensor inventory command implemented |
| OWI.3 | complete | intake | GGUF template contract validator | template validation surface implemented |
| OWI.4 | complete | intake | Tensor mapping adapter contract | model-family tensor mapping implemented |
| OWI.5 | complete | intake | Quantization policy manifest | qtype policy manifest validation implemented |
| OWI.6 | complete | intake | Calibration imatrix contract | imatrix manifest validation implemented |
| OWI.7 | complete | intake | Controlled GGUF emission roundtrip | tiny controlled GGUF emission and parse proof exists |
| OWI.8 | complete | intake | Open weight conversion bridge | selected conversion planning/bridge exists |
| OWI.9 | complete | intake | DeepSeek quantization job bridge | DeepSeek quant job bridge and validation exist |
| OWI.FINAL.0 | complete | intake | Open-weight intake closeout | intake closeout docs, tests, and guardrails complete |
| OWI.REBASE.0 | complete | intake | Source-tensor-first OWI target rebase | spine records official source tensors as primary OWI inputs and YVEX-produced GGUF as the target artifact class |
| OWI.TARGETS.0 | complete | intake | Generic model target registry | model target classes, pressure targets, and non-capability target rules exist in command-visible form |
| OWI.TARGETS.1 | planned | intake | Multi-family source manifest profile | source manifests distinguish base family, model class, source artifact class, target artifact class, qtype class, and pressure purpose |
| OWI.TARGETS.2 | planned | intake | Multi-model artifact cards | MODEL_ARTIFACTS records DeepSeek selected targets and GLM source-tensor targets without runtime claim |
| OWI.TARGETS.3 | planned | intake | Model target command profile | operator command reports target class, source artifact class, target artifact class, qtype class, and unsupported runtime boundary |
| OWI.HUGE.0 | planned | intake | Huge source tensor inventory | huge safetensors shard sets are inventoried without loading full tensor payloads |
| OWI.HUGE.1 | planned | intake | Huge safetensors shard index | safetensors shard metadata, tensor placement, offsets, and dtype distribution are indexed |
| OWI.HUGE.2 | planned | intake | Huge-model qtype and target profile | planned quantization target, qtype policy, per-role qtype classes, and expected storage bytes are reported |
| OWI.HUGE.3 | planned | intake | Huge-model artifact identity | shard-level file size, digest, manifest, and drift checks are implemented |
| OWI.HUGE.4 | planned | intake | Huge-model YVEX GGUF emission plan | conversion output naming, split GGUF strategy, quant policy, and artifact identity plan exist without emitting full GLM GGUF yet |
| STORAGE.STREAM.0 | planned | storage | Storage-stream threat and residency model | SSD-backed residency rules, cache policy, and non-generation boundaries are documented |
| STORAGE.STREAM.1 | planned | storage | Storage path and cache layout | operator-local model cache layout separates source, YVEX-produced GGUF, external reference artifacts, derived reports, and runtime cache |
| STORAGE.STREAM.2 | planned | storage | Shard open and cold-read probe | source shards and future GGUF shards can be opened/read from SSD with timing and cleanup reports |
| STORAGE.STREAM.3 | planned | storage | Tensor page/chunk access plan | tensor byte ranges are mapped to page/chunk reads with overflow and bounds checks |
| STORAGE.STREAM.4 | planned | storage | Warm cache and repeated-read diagnostics | repeated shard/tensor reads report cold/warm timing without benchmark claim |
| STORAGE.STREAM.5 | planned | storage | Storage-backed residency prototype | selected tensors can be staged from SSD into backend memory under explicit residency rules |
| STORAGE.STREAM.6 | planned | storage | Storage-stream failure and cleanup reports | missing shard, short read, digest mismatch, eviction, and cleanup failures are reported |
| MODEL.CLASS.0 | planned | model | Model-class profile schema | model class, architecture family, MoE shape, source artifact class, target artifact class, context class, and runtime requirements are reported |
| MODEL.CLASS.1 | planned | model | Huge MoE family adapter inventory | MoE metadata, expert count, active expert count, shared expert facts, and routing metadata are reported |
| MODEL.CLASS.2 | planned | model | Runtime requirement report | required ops, tensor roles, KV shape, cache pressure, and unsupported execution blockers are visible |
| MODEL.CLASS.3 | planned | model | GLM-family mapping boundary | GLM tensor names and architecture metadata map to YVEX roles without execution claim |
| TENSOR.COLLECTION.0 | planned | tensor-collection | Canonical tensor collection schema | embedding, norm, attention, KV, MLP, MoE, output, and tokenizer collections are reported without execution claim |
| TENSOR.COLLECTION.1 | planned | tensor-collection | DeepSeek tensor collection report | selected DeepSeek tensors map into explicit runtime collections |
| TENSOR.COLLECTION.2 | planned | tensor-collection | GLM tensor collection inventory | GLM source tensor names map into collection candidates without runtime claim |
| ATTENTION.CLASS.0 | complete | attention | Attention class report | `yvex attention report` is command-visible for DeepSeek-family artifacts, mapping family-runtime facts into attention type/status, head layout, Q/K/V/O role requirements, RoPE/position requirements, mask rules, KV requirements, context blockers, graph primitive versus full-transformer attention distinction, backend requirements, selected-slice partial reports, source-only refusals, unknown-family refusals, and next runtime dependencies without full transformer attention execution, real attention-backed KV, full model execution, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| CONTEXT.CLASS.0 | complete | context | Context class report | `yvex context report` is command-visible with model/requested/active context fields, token input counts, chunking policy, overflow behavior, decode position policy, bounded diagnostic versus full runtime context distinction, attention/KV dependency reports, selected-slice partial reports, source-only refusals, unknown-family refusals, backend reports, and next runtime dependencies without full transformer prefill, real decode, full model execution, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| KV.CACHE.0 | complete | kv | KV cache class report | `yvex kv report` is command-visible with diagnostic-vs-real KV boundary, layout/dtype/layer/head/position/capacity requirements, residency classes, context dependency, attention dependency, prefill write/decode read requirements, selected-slice partial reports, source-only refusals, unknown-family refusals, backend reports, and next runtime dependencies without real attention-backed KV writes, full transformer prefill, decode, logits, generation, provider generation, eval, benchmark, or throughput claim |
| MOE.CLASS.0 | planned | moe | MoE model-class report | expert count, active expert count, router facts, shared experts, and expert tensor classes are reported |
| MOE.ACT.0 | planned | moe | Expert activation boundary | router logits, top-k selection, expert dispatch, accumulation, and cleanup are implemented and tested |
| RESIDENCY.0 | planned | residency | Residency class report | resident, host-staged, SSD-staged, SSD-streamed, managed-memory, hybrid, and distributed classes are reported |
| RESIDENCY.1 | planned | residency | Tensor residency plan | tensor collections are assigned planned residency classes with memory and storage pressure reports |
| RESIDENCY.2 | planned | residency | Runtime residency transition proof | selected tensors move between storage, host memory, and backend memory with cleanup/failure reports |
| BUILD.PROFILE.0 | planned | build | Build profile matrix | CPU, CUDA, debug, release, sanitizer, and future backend build profiles are documented and command-visible |
| HARDWARE.PROFILE.0 | planned | hardware | Spark GB10 hardware profile | Spark memory, CUDA, SSD, and storage pressure profile is reported without benchmark claim |
| HARDWARE.PROFILE.1 | planned | hardware | Workstation hardware profile | local workstation capability report is separated from backend support |
| HARDWARE.PROFILE.2 | planned | hardware | Future distributed hardware profile | distributed node assumptions are recorded without support claim |
| COMPUTE.BACKEND.0 | planned | backend | Backend capability matrix | CPU, CUDA, future Metal, and future ROCm op capability states are reported |
| COMPUTE.BACKEND.1 | planned | backend | Backend memory pressure reports | allocation, transfer, op failure, cleanup, and fallback reports are implemented |
| OPERATOR.FLOW.0 | complete | operator | Procedural operator flow | source, artifact, residency, graph, runtime, serve, eval, and bench command paths are shown as short flows |
| OPERATOR.FLOW.1 | complete | operator | Single-paste operator transcript | runbook contains one copy-paste transcript, full implemented command inventory, inline command style, artifact hygiene, and current/future boundary |
| OPERATOR.FLOW.2 | complete | operator | Sectorized copy-command operator atlas | runbook is split into model, backend, intake, artifact, integrity, materialization, graph, prefill/KV, daemon, validation, and GLM status lanes with standalone copyable commands |
| OPERATOR.PATHS.0 | complete | operator | Operator model root configuration | `yvex paths configure` stores or resolves operator-local model roots without requiring shell export walls |
| MODEL.TARGET.PATHS.0 | complete | model | Model target path resolution | `yvex model-target inspect TARGET --paths` reports source, artifact, report, registry, and planned paths without reading model payloads |
| MODEL.PREPARE.0 | complete | model | DeepSeek selected artifact prepare preset | `yvex models prepare deepseek4-v4-flash-selected-embed` runs the implemented source-to-selected-GGUF and alias registration path without generation claim |
| MODEL.PREPARE.1 | complete | model | Prepare preset refusal and dry-run behavior | prepare reports unsupported targets such as GLM source-only targets and supports dry-run, no-register, and no-use boundaries |
| MODEL.CHECK.0 | complete | model | Selected artifact check preset | `yvex models check deepseek4-v4-flash-selected-embed --backend cpu|cuda --level quick|runtime|full` composes implemented selected artifact verification through selected runtime graph checks without generation claim |
| MODEL.CHECK.1 | planned | model | Segment artifact check preset | selected embedding-plus-RMSNorm checks include input tokens, selected segment graph, segment-summary prefill, and minimal KV binding where implemented |
| GRAPH.CHECK.0 | complete | graph | Graph check preset suites | `yvex graph check --suite primitives|block|layers|all` composes existing standalone primitive, controlled block, and controlled layer scheduler fixture proofs without real model layer execution, prefill, decode, logits, sampling, generation, evaluation, or benchmark claim |
| CHAT.UX.0 | planned | operator | Diagnostic REPL clarity and layout | accepted-only chat shows status, help, model/backend/session state, optional color, NO_COLOR behavior, and unsupported generation boundary |
| OPERATOR.FLOW.3 | planned | operator | Runbook over real operator presets | operator runbook is rewritten around `paths configure`, `model-target --paths`, `models prepare`, `models check`, graph lanes, accepted-only chat, validation, and artifact hygiene |
| DOCS.README.OPERATOR.0 | planned | docs | Public operator examples after preset stabilization | README examples use stable operator presets without internal IDs or unsupported generation claims |
| SERVE.RUNTIME.0 | planned | serve | Serving runtime ownership map | daemon/provider surfaces are mapped to runtime generation ownership without generation claim |
| ARTIFACT.NAMING.0 | complete | artifact | GGUF artifact naming contract | canonical artifact alias/name rules implemented |
| RUNTIME.KV.0 | complete | kv-policy | KV cache policy | KV policy documented without runtime claim |
| M1 | complete | runtime | Real model conversion/materialization gate | selected real artifact gate and materialization proof exists |
| M2 | complete | runtime | DeepSeek materialization hardening | selected DeepSeek materialization hardened on CPU/CUDA |
| CLI.PACKAGE.0 | complete | cli | Repository CLI packaging baseline | root binaries and CLI packaging baseline established |
| CLI.PACKAGE.1 | complete | cli | Minimal compiled-binary packaging baseline | compiled root binaries remain repository-local and validated |
| CLI.CONSOLE.0 | complete | cli | CLI interface doctrine | CLI claim and command-output doctrine established |
| CLI.SURFACE.0 | complete | cli | CLI translation unit split | top-level dispatch separated from private command catalog and command/proof implementations |
| CLI.SURFACE.1 | complete | cli | CLI command monolith domain split | private CLI command implementation split into common, graph, models, artifacts, tools, and run domains |
| CLI.SURFACE.2 | complete | cli/layout | CLI surface compression | only yvex_cli.c remains as CLI-prefixed source; command/proof logic promoted to domain owners |
| CLI.SURFACE.3 | complete | cli/layout | Domain-owned CLI help and command extraction | yvex_cli.c contains only short command table and dispatch; detailed help and command behavior live in domain owners; command split files are removed |
| CLI.MODELS.0 | complete | cli | Local model selection spine | model selection design and registry shape established |
| CLI.MODELS.1 | complete | cli | Local model registry implementation | local model registry add/list/use/current/remove works |
| CLI.MODELS.2 | complete | cli | One-shot model alias resolution | model-taking commands resolve aliases or paths |
| CLI.MODELS.3 | complete | cli | Model selection in canonical REPL | chat/console uses selected model state where allowed |
| CLI.MODELS.4 | complete | cli | Model alias resolution in yvexd | daemon resolves explicit model aliases |
| REPO.OPERATING.0 | complete | docs | Operating handbook and artifact cards | AGENTS and artifact cards define repository rules |
| REPO.LAYOUT.1 | complete | layout | Root-first C source layout collapse | source layout collapsed to root-first style |
| REPO.LAYOUT.2 | complete | layout | Root source compression and native root binaries | native root binaries and source compression complete |
| REPO.SURFACE.0 | complete | layout | CUDA, GGUF, model family, and test surface refoundation | core source surfaces refounded and tested |
| REPO.SURFACE.1 | complete | layout | Natural C surface and code style refoundation | root C surface made natural and guarded |
| CUDA.SURFACE.0 | complete | backend | CUDA kernel translation unit | CUDA kernels live in CUDA translation unit |
| CODE.NATURAL.0 | complete | layout | Natural translation unit rewrite | translation units rewritten to project style |
| CODE.NATURAL.1 | complete | layout | Final translation unit hygiene pass | final source hygiene pass complete |
| REWRITE.CORE.COMPRESS.0 | complete | cleanup | In-place core compression | graph, runtime, and model artifact files cleaned and compressed in place without source splitting or capability change |
| REWRITE.GRAPH.INPLACE.1 | complete | cleanup | Graph in-place semantic compression | yvex_graph.c command, guard, primitive, and block internals compressed without source splitting or capability change |
| REWRITE.RUNTIME.INPLACE.1 | complete | cleanup | Runtime in-place semantic compression | yvex_runtime.c engine, session, selected graph execution, prefill, run, and chat internals compressed without source splitting or capability change |
| REWRITE.MODEL.ARTIFACTS.INPLACE.1 | complete | cleanup | Model artifact in-place semantic compression | yvex_model_artifacts.c registry, target, prepare, check, and command internals compressed without source splitting or capability change |
| YVEX.SKELETON.0 | complete | layout | Canonical runtime/eval/bench source skeleton | definitive root ownership files exist with responsibility headers and no capability claim |
| TEST.SURFACE.0 | complete | test | Test vectors and runner consolidation | compact test runners and vectors consolidated |
| DOCS.PUBLIC.0 | complete | docs | Public documentation boundary cleanup | public docs cleaned of internal delivery leakage |
| DOCS.MIN.0 | complete | docs | Minimal documentation surface | canonical docs surface established |
| DOCS.OPERATOR.RUNBOOK.0 | complete | docs | Canonical operator runbook | command-first runbook exists |
| DOCS.RUNBOOKS.MODEL.0 | complete | docs | Model-scoped operator runbooks | operator runbook becomes an index and detailed copy-pack lanes move into docs/runbooks/deepseek.md, docs/runbooks/glm.md, and docs/runbooks/common.md |
| ARTIFACT.INTEGRITY.0 | complete | artifact | Artifact integrity threat model and validator baseline | baseline GGUF validator and required tensor readiness exist |
| ARTIFACT.INTEGRITY.1 | complete | artifact | File identity and digest enforcement | local file size/SHA-256 identity recorded and enforced |
| ARTIFACT.INTEGRITY.2 | complete | artifact | GGUF structural corruption fixture suite | tiny corrupt GGUF corpus and refusal harness exist |
| ARTIFACT.INTEGRITY.3 | complete | artifact | Tensor directory offset and byte-range validation | canonical tensor range validator gates reads |
| ARTIFACT.INTEGRITY.4 | complete | artifact | Shape, rank, dtype, and byte-count overflow hardening | canonical shape/dtype accounting feeds range validation |
| ARTIFACT.INTEGRITY.5 | complete | artifact | Registry alias metadata drift diagnostics | registry metadata summary compare and drift reports exist |
| ARTIFACT.INTEGRITY.6 | complete | artifact | Materialization integrity gate | materialization preflight, phase, and cleanup reporting exist |
| ARTIFACT.INTEGRITY.7 | complete | artifact | Graph execution integrity guard | graph preflight, dispatch/reference guards, and cleanup reports exist |
| ARTIFACT.INTEGRITY.8 | complete | artifact | Corrupt artifact regression harness | consolidated integrity regression matrix exists |
| ARTIFACT.INTEGRITY.9 | complete | artifact | Operator integrity report and doctor integration | operator-facing integrity report aggregates existing checks |
| ARTIFACT.INTEGRITY.FINAL.0 | complete | artifact | Artifact integrity closeout before graph expansion | integrity module closed before broader graph work |
| M3 | complete | runtime | Materialized-weight engine attachment | selected backend-resident weights attach to engine/session ownership |
| M4 | complete | graph | First executable fixture graph path | controlled F32 fixture graph executes on CPU/CUDA where available |
| M5 | complete | graph | First real-model partial graph execution | selected F16 token_embd.weight participates in scheduled graph work |
| M6 | complete | graph | Real-model graph segment expansion | embedding-plus-RMSNorm segment executes over multiple real tensors |
| M7 | complete | input | Prompt/token input boundary | validated token sequences route into implemented graph paths |
| M8 | complete | prefill | Prefill state foundation | segment-summary prefill state created from validated token sequence |
| PAPER.INDEX.0 | planned | docs | Paper-to-algorithm registry | spine records paper references, algorithm family, implementation stage, command surface, and unsupported boundary without claiming implementation |
| ALGORITHM.MODES.0 | planned | docs | Runtime algorithm mode registry | attention, prefill, decode, logits, sampling, KV, and residency modes are mapped to command surfaces and implementation stages |
| CLI.RESEARCH.0 | planned | cli | Research CLI surface matrix | CLI exposes algorithm-mode inspection and explicit tensor/runtime diagnostics without hiding behind automatic generation paths |
| ATTENTION.MODES.0 | planned | attention | Attention mode report | full, MQA/GQA, FlashAttention, paged, sparse, LongCat-style, and model-family attention modes are command-visible as support/report/unsupported states |
| PREFILL.MODES.0 | planned | prefill | Prefill mode report and selection | segment-summary, layer-backed, chunked, dense, sparse, and SSD-staged prefill modes are reportable and selectable where implemented |
| DECODE.MODES.0 | planned | decode | Decode mode report and selection | diagnostic, autoregressive, speculative, block/diffusion, and model-family decode modes are reportable and selectable where implemented |
| LOGITS.MODES.0 | planned | logits | Logits mode report and selection | diagnostic, output-head, sharded, staged, and logprob/top-k logits modes are reportable and selectable where implemented |
| SAMPLING.MODES.0 | planned | sampling | Sampling strategy report and selection | greedy, temperature, top-k, top-p, min-p, typical, and seeded stochastic sampling strategies are command-visible where implemented |
| GENERATION.MODES.0 | planned | generation | Generation mode report and selection | baseline, bounded diagnostic, speculative, streaming, and provider generation modes are reportable without unsupported claims |
| KV.MODES.0 | planned | kv | KV layout and residency mode report | contiguous, paged, chunked, host-staged, SSD-staged, SSD-streamed, and quantized KV modes are reportable without unsupported claims |
| TENSOR.TRACE.0 | planned | trace | Tensor-level runtime trace | tensor IDs, shapes, dtypes, residency, graph op ownership, checksums, and cleanup phases are CLI-visible for runtime study |
| M9 | complete | kv | Minimal KV ownership and append/read boundary | session-owned KV shape, allocation, append/read, lifecycle, cleanup, and context overflow behavior |
| PREFILL.1 | complete | prefill | KV-backed prefill state binding | M8 segment-summary state connects to minimal KV ownership without decode/logits claim |
| GRAPH.OPS.0 | complete | graph | RoPE and position operation boundary | position-dependent graph op implemented with tests and backend rules |
| GRAPH.OPS.1 | complete | graph | Attention primitive boundary | attention inputs, masks, scratch, backend dispatch, and failure paths implemented |
| GRAPH.OPS.2 | complete | graph | Projection and matmul primitive boundary | F32 matmul/projection primitive implemented with shape, byte, backend, dispatch, reference, and cleanup limits |
| GRAPH.OPS.3 | complete | graph | MLP and routed-expert primitive boundary | F32 feed-forward and routed expert-slice primitive implemented with explicit tensor roles and backend support |
| GRAPH.BLOCK.0 | complete | graph | First transformer block execution | controlled fixture block executes through normalization, attention, residual, MLP path with owned scratch |
| GRAPH.LAYERS.0 | complete | graph | Layer scheduler and repeated block execution | `yvex graph --execute-layers --block fixture` runs a bounded repeated controlled block fixture with selected-position activation handoff, cleanup proof, CPU/CUDA parity, and no prefill/decode/logits/sampling/generation claim |
| PREFILL.2 | complete | prefill | Bounded layer-backed prefill state path | validated token sequences run selected embedding-plus-RMSNorm segments, hand sampled rows into the controlled layer fixture scheduler, and optionally bind diagnostic KV without full transformer/decode/logits/sampling/generation claim |
| PREFILL.3 | complete | prefill | Chunked prefill and scratch lifecycle | `yvex prefill --chunk-size N` partitions validated token input into bounded chunks, validates position/context boundaries, reuses prefill host scratch for diagnostic staging, preserves segment and layer-backed prefill paths, optionally binds KV rows in token order, and reports cleanup/failure state without full transformer prefill, decode, logits, sampling, generation, evaluation, or benchmark claim |
| PREFILL.4 | planned | prefill | Prefill diagnostics and regression reports | regression/reporting layer over implemented segment, layer-backed, chunked, KV-bound prefill; not a blocker for first decode/logits/sampling/generation closure |
| PREFILL.5 | planned | bench | Prefill throughput measurement gate | benchmarkable prefill measurement after runtime path and measurement harness exist; no throughput claim before artifact/backend/context/machine metadata |
| M10 | planned | decode | Decode step over existing runtime state | one decode step advances existing KV-backed state by one position |
| DECODE.0 | complete | decode | First bounded decode state step | `yvex decode` invokes implemented chunked/layer-backed prefill, advances one bounded diagnostic decode-state position, validates context boundary, reports checksum/state/cleanup fields, and preserves logits/sampling/generation unsupported boundaries |
| DECODE.1 | planned | decode | Decode lifecycle and repeatability | repeated decode steps, context-end handling, interruption, cleanup, and deterministic diagnostics over existing state |
| M11 | planned | logits | Logits production boundary | logits buffer ownership, dtype, backend tolerance, and diagnostics implemented |
| LOGITS.0 | complete | logits | First bounded logits buffer | `yvex logits` invokes implemented decode, produces a bounded deterministic logits buffer from decode state, reports checksum/min/max/sample/cleanup fields, and preserves real output-head, sampling, generation, and benchmark unsupported boundaries |
| LOGITS.1 | planned | logits | Logprob/top-k diagnostics | logprob/top-k diagnostic path over implemented logits buffer |
| M12 | planned | logits | Deterministic logits regression | stable vector tests for logits with model/artifact identity and backend tolerance |
| M13 | planned | sampling | Sampling boundary | greedy and stochastic sampling over logits with seed and parameter validation |
| SAMPLING.0 | complete | sampling | First sampler boundary | `yvex sample` invokes implemented logits, performs deterministic greedy selection over the bounded diagnostic logits buffer, reports selected index/token/logit/checksum fields, and preserves real vocab sampling, generation, and benchmark unsupported boundaries |
| SAMPLING.1 | planned | sampling | Sampling diagnostics and reproducibility | seed, parameters, stop reason, and reproducibility diagnostics over implemented sampler |
| GEN.CONTRACT.0 | complete | docs | Generation loop contract | spine defines generation loop state machine, token lifecycle, stop reasons, CLI output fields, trace levels, cleanup/failure phases, and bounded-vs-full generation boundary without implementation claim |
| GEN.TRACE.0 | complete | trace | Generation trace surface | `yvex generate --trace-level none|tokens|steps|kv|logits|sampling|full` exposes stable bounded diagnostic token, step, KV, logits, sampling, append, stop, failure, cleanup, and trace-counter records without full-model, DeepSeek, provider, eval, benchmark, raw tensor, or tensor-dump claim |
| GEN.APPEND.0 | complete | generation | Token append lifecycle | generated-token append, candidate/accepted/generated token accounting, runtime sequence mutation, position advance, context-limit non-mutation, partial-output accounting, append failure reporting, checksum/accounting, and cleanup behavior are implemented and tested for the bounded diagnostic generation loop without full-model, DeepSeek, provider, eval, or benchmark claim |
| GEN.STOP.0 | complete | generation | Stop-condition policy | max-new-tokens, context-limit, failure stop reasons, stop timing, stop phase, partial-output accounting, unsupported EOS/stop-token boundary, and cleanup behavior are implemented and tested for the bounded diagnostic generation loop without full-model, DeepSeek, provider, eval, or benchmark claim |
| M14 | planned | generation | First constrained generation loop | decode -> logits -> sample -> append token loop with stop conditions and token accounting |
| GEN.LOOP.0 | complete | generation | First constrained generation loop | `yvex generate` composes implemented prefill, decode, logits, greedy sampling, token append, stop checks, and cleanup into a bounded diagnostic generation loop without full-model, DeepSeek, provider, eval, or benchmark claim |
| GEN.LOOP.1 | complete | generation | Generation state and interruption | bounded diagnostic generation state lifecycle, deterministic `--cancel-after-steps` interruption, cancel trace records, cleanup idempotence, and partial-output preservation are implemented without full-model, DeepSeek, provider, streaming, eval, or benchmark claim |
| M15 | planned | cli | Interactive CLI generation path | CLI/REPL generation uses real runtime generation loop |
| CLI.GEN.0 | complete | cli | CLI generation command surface | `yvex generate` exposes the implemented bounded diagnostic generation loop through a stable operator CLI surface with clear help, examples, argument validation, command inventory, refusal wording, trace/cancel/context usage, runbook coverage, and unsupported full-model/DeepSeek/provider/eval/benchmark boundaries |
| M16 | planned | server | Provider/server generation boundary | daemon/server generation uses runtime-backed generation path |
| M17 | planned | profile | Trace/profile hardening for generation | traces and profiles identify artifact/backend/graph/KV/decode/logits/sampling/server failures |
| HARDWARE.PROFILE.MAC.0 | planned | hardware | Apple Silicon Mac hardware profile | MacBook Apple Silicon CPU/GPU/unified-memory/storage profile is reported without backend or model capability claim |
| COMPUTE.BACKEND.METAL.0 | planned | backend | Metal feasibility profile | Metal build/toolchain/device/memory feasibility is reported without backend op support claim |
| COMPUTE.BACKEND.METAL.1 | planned | backend | Metal allocation and movement boundary | Metal buffer allocation, unified-memory behavior, transfer/visibility rules, failure paths, and cleanup are implemented and tested without model execution claim |
| GRAPH.METAL.0 | planned | graph | Metal primitive parity baseline | selected primitive graph ops run on Metal with CPU reference comparison, checksum, max-diff, failure paths, and cleanup without transformer or generation claim |
| OWI.TARGETS.QWEN.0 | planned | intake | Qwen source target profile | Qwen official source tensors are recorded as Apple Silicon / Metal pressure targets without runtime or generation claim |
| MODEL.CLASS.QWEN.0 | planned | model | Qwen model-class profile | Qwen architecture, tensor roles, attention/KV requirements, tokenizer requirements, and unsupported runtime blockers are reported |
| TENSOR.COLLECTION.QWEN.0 | planned | tensor-collection | Qwen tensor collection inventory | Qwen tensor names map into embedding, normalization, attention, MLP/MoE, output, tokenizer, and KV collection candidates without runtime claim |
| ARTIFACT.QWEN.0 | planned | artifact | Qwen YVEX-produced artifact identity | Qwen YVEX-produced GGUF identity, digest, tensor byte ranges, qtype summary, and registry metadata are recorded without runtime claim |
| RESIDENCY.METAL.0 | planned | residency | Metal unified-memory residency plan | Qwen tensor collections are assigned planned Apple unified-memory/host-staged/Metal residency classes with memory pressure and unsupported blockers |
| FULLMODEL.QWEN.0 | planned | model | Qwen full model inventory and placement report | Qwen full artifact tensor inventory, memory budget, collection coverage, backend placement pressure, and runtime blockers are reported without materialization or generation claim |
| GEN.QWEN.METAL.0 | planned | generation | Qwen Metal full-runtime generation target | after real Metal backend, Qwen full runtime descriptor, real prefill, KV, decode, output-head logits, and vocabulary sampling exist, Qwen small/medium generation may run over YVEX runtime |
| BENCH.QWEN.METAL.0 | planned | bench | Qwen Metal generation benchmark | after Qwen Metal generation exists, benchmark harness measures runtime path with artifact identity, qtype, context, backend, machine, command, run count, and reproducibility metadata |
| GEN.DEEPSEEK.0 | planned | generation | DeepSeek V4 Flash full generation path | DeepSeek V4 Flash reaches real decode, logits, sampling, token append, stop conditions, cleanup, and CLI-visible generation over YVEX runtime |
| BENCH.DEEPSEEK.DECODE.0 | planned | bench | DeepSeek V4 Flash baseline decode throughput target | after generation exists, benchmark harness measures decode tok/s with artifact identity, qtype, context, backend, machine, command, and reproducibility metadata |
| BENCH.DEEPSEEK.GEN.0 | planned | bench | DeepSeek V4 Flash end-to-end generation throughput target | after generation exists, benchmark harness measures prompt plus generated-token throughput separately from prefill |
| SPEC.DSPARK.REF.0 | planned | reference | DSpark speculative decoding reference | DSpark is recorded as external reference evidence for semi-autoregressive drafting, confidence scheduling, and hardware-aware verification, not as YVEX runtime capability |
| SPEC.VERIFY.0 | planned | generation | Token verification semantics | target-model verification, accepted prefix, rejected-token behavior, state mutation boundary, and cleanup rules are reported without speculative runtime claim |
| SPEC.VERIFY.COST.0 | planned | generation | Verification-cost utility report | speculative verification utility reports accepted-token gain versus target verification cost without runtime acceleration or benchmark claim |
| SPEC.MOE.ROUTING.0 | planned | moe | Routing-aware verification report | MoE speculative verification reports router fanout, activated experts, expert residency, expert movement, and verification blockers without MoE speculative runtime claim |
| SPEC.MOE.EXPERT.BUDGET.0 | planned | moe | Expert-budget verification policy | future verification policy constrains expert fanout/movement during speculative verification without changing target correctness semantics |
| SPEC.DEEPSEEK.ACCOUNTING.0 | planned | generation | DeepSeek accepted-token and expert accounting | accepted/rejected speculative tokens are accounted with KV, expert-routing, cleanup, and trace semantics after baseline DeepSeek generation exists |
| SPEC.DEEPSEEK.0 | planned | generation | DeepSeek speculative decoding target | after baseline generation exists, YVEX may implement draft, verification, accepted-token accounting, and speculative generation over DeepSeek target runtime |
| BENCH.DEEPSEEK.SPEC.0 | planned | bench | DeepSeek speculative generation benchmark | after speculative decoding exists, benchmark harness measures accepted tokens, verification cost, latency, throughput, and speedup over YVEX baseline generation |
| FULLMODEL.0 | complete | model | Full model inventory and placement plan | `yvex fullmodel report --model FILE_OR_ALIAS` reports GGUF metadata/tensor-directory inventory, qtype/dtype summaries, tensor collections, role coverage, memory and CPU/CUDA placement pressure, and runtime blockers without payload materialization, backend allocation, full model execution, generation, evaluation, or benchmark claim |
| FULLMODEL.1 | complete | model | Full model materialization plan | selected-family full tensor placement phases, residency classes, backend fit estimates, preflight blockers, cleanup plan, and FULLMODEL.2 readiness are command-visible without full materialization, full model execution, DeepSeek generation, provider generation, streaming, eval, benchmark, or throughput claim |
| FULLMODEL.2 | complete | model | Full model materialization proof | `yvex fullmodel materialize --model FILE_OR_ALIAS --backend cpu|cuda` either allocates and releases bounded required proof tensors for a controlled tiny full-ish GGUF or refuses selected/runtime-slice, source-only, incomplete, oversized, missing, and corrupt artifacts with phase, byte-limit, and cleanup reports; it does not create full model execution, DeepSeek materialization, generation, provider generation, eval, benchmark, or throughput capability |
| FULLMODEL.3 | complete | model | Full model runtime descriptor | full model runtime descriptor is command-visible with tensor role map, collection map, residency requirements, graph requirements, prefill/KV/decode/logits/sampling requirements, output-head/tokenizer requirements, backend requirements, runtime blockers, selected-slice partial descriptors, tiny full-ish fixture descriptors, and source-only refusal without full model execution, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| FAMILY.RUNTIME.0 | complete | model | Runtime family adapter boundary | `yvex fullmodel family-runtime` maps descriptor facts into DeepSeek-family tensor roles, tensor collections, attention/KV, MoE, output-head/logits, graph/backend requirements, runtime blockers, and next dependencies without full model execution, real DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| KV.MIN.0 | planned | kv | Minimal KV shape and ownership | session-owned KV shape, allocation, and release |
| KV.MIN.1 | planned | kv | KV append/read boundary | append/read by layer/head/position with checks and diagnostics |
| KV.MIN.2 | planned | kv | KV lifecycle in session state | save/clear/reinit behavior inside session lifecycle |
| KV.MIN.3 | planned | kv | KV diagnostics and failure reporting | KV size, position range, overflow, and cleanup reports |
| RUNTIME.KV.1 | planned | kv-capacity | Static KV size estimator | memory estimator by context, layers, dtype/qtype, backend |
| RUNTIME.KV.2 | planned | kv-capacity | CUDA KV allocation proof | CUDA KV allocation and cleanup behavior |
| RUNTIME.KV.3 | planned | kv-capacity | Paged KV allocator skeleton | page table and ownership boundary without performance claim |
| RUNTIME.KV.4 | planned | kv-capacity | Host spill and cold-cache experiments | host/disk spill experiments behind explicit unsupported boundary |
| RUNTIME.KV.5 | planned | kv-capacity | KV quantization policy | KV qtype policy and diagnostics, no compute claim unless implemented |
| MODEL.LIFECYCLE.0 | planned | model-ux | Unified model status report | concise model status over registry, artifact, backend, qtype, graph readiness |
| MODEL.LIFECYCLE.1 | planned | model-ux | Model prepare preset | wraps registry checks, integrity, backend checks, materialization, and reports |
| MODEL.LIFECYCLE.2 | planned | model-ux | Model check preset | operator-friendly artifact/backend/qtype/CUDA check |
| MODEL.LIFECYCLE.3 | planned | model-ux | Model doctor flow | common local artifact, registry, backend, CUDA, and command failures diagnosed |
| MODEL.LIFECYCLE.4 | planned | model-ux | Model-class profile | memory and artifact profile for large local models |
| MODEL.LIFECYCLE.5 | planned | model-ux | Artifact cache and report hygiene | generated reports/cache/logs managed outside git |
| CLI.UX.0 | planned | cli | Command taxonomy and help layout | short path first, advanced diagnostics later |
| CLI.UX.1 | planned | cli | Concise normal-path commands | one-line presets over low-level gates |
| CLI.UX.2 | planned | cli | Colorized terminal output and severity | pass/warn/fail/unsupported with NO_COLOR and non-TTY fallback |
| CLI.UX.3 | planned | cli | Structured output modes | stable JSON/text output for scripts without color pollution |
| CLI.UX.4 | planned | cli | Interactive REPL line editing and history | usable local REPL shell |
| CLI.UX.5 | planned | cli | REPL slash-command cleanup | discoverable slash commands and help |
| CLI.UX.6 | planned | cli | Doctor command | environment, registry, backend, CUDA, artifact, and runtime checks |
| CLI.UX.7 | planned | cli | Operator profiles | workstation, CUDA, future large hardware profiles |
| CLI.UX.8 | planned | cli | One-line command recipes | normal paths documented and command-visible |
| SERVER.RUNTIME.0 | planned | server | Runtime-backed model state in daemon | daemon reflects real runtime state, not only status shell |
| SERVER.RUNTIME.1 | planned | server | Daemon execution-state diagnostics | server reports model/artifact/backend/runtime readiness without generation |
| SERVER.GEN.0 | planned | server | First provider generation endpoint | provider endpoint backed by real constrained generation loop |
| SERVER.API.0 | planned | server | OpenAI-compatible API boundary | compatibility only after runtime generation exists |
| SERVER.API.1 | planned | server | Anthropic-compatible API boundary | compatibility only after runtime generation exists |
| SERVER.STREAM.0 | planned | server | Streaming response boundary | stream tokens from real generation loop |
| SERVER.OBS.0 | planned | server | Server traces and metrics | request/runtime failure reports and metrics |
| EVAL.FIXTURE.0 | planned | eval | Fixture graph correctness vectors | exact fixture output regression |
| EVAL.PARTIAL.0 | planned | eval | Real partial graph regression vectors | selected embedding/segment checksum and max-diff vectors |
| EVAL.PREFILL.0 | planned | eval | Prefill state regression | token positions, prefill summaries, KV readiness and failure states |
| EVAL.KV.0 | planned | eval | KV append/read correctness vectors | KV shape, append/read, overflow, cleanup vectors |
| EVAL.DECODE.0 | planned | eval | Decode-step regression | one-step decode over existing KV state |
| EVAL.LOGITS.0 | planned | eval | Deterministic logits regression | logits vectors with backend tolerance and artifact identity |
| EVAL.SAMPLING.0 | planned | eval | Sampling determinism checks | greedy/seeded stochastic sampling checks |
| EVAL.GEN.0 | planned | eval | Constrained generation smoke/eval | small generation cases through the same runtime users run |
| EVAL.CAPABILITY.0 | planned | eval | Capability regression suite | curated task suite after generation path exists |
| BENCH.PREFILL.0 | planned | bench | Prefill throughput benchmark | token interval throughput over real transformer prefill |
| BENCH.DECODE.0 | planned | bench | Decode throughput benchmark | generated-token decode speed over existing state |
| BENCH.GEN.0 | planned | bench | Generation throughput benchmark | prompt plus generated-token throughput |
| BENCH.MEMORY.0 | planned | bench | Runtime memory pressure benchmark | model, scratch, KV, backend memory pressure |
| BENCH.SERVER.0 | planned | bench | Provider latency benchmark | request, queue, streaming latency after server generation |
| BENCH.RUNTIME.0 | planned | bench | End-to-end runtime benchmark harness | reproducible machine/backend/artifact/qtype/context command harness |
| BACKEND.PROFILE.0 | planned | backend | CPU correctness profile | reference/debug profile and limitations |
| BACKEND.PROFILE.1 | planned | backend | Local CUDA workstation profile | CUDA workstation capability and memory report |
| BACKEND.PROFILE.2 | planned | backend | DGX Spark / GB10 profile | CUDA-class hardware pressure profile |
| BACKEND.PROFILE.3 | planned | backend | Large-memory future hardware profile | high-memory machine planning profile |
| BACKEND.PROFILE.4 | planned | backend | Backend failure and memory pressure reports | allocation/transfer/op failure diagnostics |
| BACKEND.PROFILE.5 | planned | backend | Backend capability matrix | op and memory capability matrix |
| BACKEND.METAL.0 | planned | backend | Metal feasibility profile | future lane only, no support claim |
| BACKEND.ROCM.0 | planned | backend | ROCm/Strix Halo feasibility profile | future lane only, no support claim |
| LAYOUT.RUNTIME.0 | complete | layout | Runtime module boundary audit | runtime ownership separated from CLI/server glue without changing runtime capability |
| LAYOUT.GRAPH.0 | complete | layout | Graph/executor module separation | controlled block fixture execution separated from CLI parsing before repeated layer scheduling |
| LAYOUT.CLI.0 | planned | layout | CLI command taxonomy cleanup | command parsing organized by domain |
| LAYOUT.SERVER.0 | planned | layout | Server/runtime boundary cleanup | daemon does not duplicate CLI runtime wiring |
| LAYOUT.TEST.0 | planned | layout | Test fixture/eval/bench separation | parser fixtures, runtime fixtures, eval vectors, benches separated |
| LAYOUT.DOCS.0 | planned | layout | Public/internal docs boundary cleanup | public docs stay product-facing; spine remains internal |
| LAYOUT.BUILD.0 | planned | layout | Build targets by backend/profile | build layout reflects CPU/CUDA/future backend profiles |
| DOCS.README.2 | planned | docs | README inference vision refresh | public README reflects engine vision without internal delivery IDs |
| DOCS.RUNBOOK.1 | planned | docs | Generic model-class operator flow | runbook no longer only selected-artifact/deepseek-heavy |
| DOCS.RUNBOOK.2 | planned | docs | Reduced-flag normal path | operator presets documented separately from debug flags |
| DOCS.RUNBOOK.3 | planned | docs | Failure-mode cookbook | common refusal/failure states documented |
| DOCS.RUNBOOK.4 | planned | docs | Verticalized operator runbook | normal/debug/intake/CI/large-model paths separated |
| DOCS.CONTRACT.1 | planned | docs | Runtime contract after generation path matures | public behavior contract updated after runtime generation |
| DOCS.API.1 | planned | docs | Public API map after inference maturity | API docs updated after KV/decode/logits/generation surfaces mature |
| DOCS.PUBLIC.1 | planned | docs | Public docs without internal delivery IDs | public docs remain free of spine IDs |
| DOCS.README.TARGETS.0 | planned | docs | README measurement target table after benchmark harness | public speed/eval table updated only after harness exists |
| DOCS.DIAGRAMS.0 | planned | docs | Artifact-to-runtime boundary diagram | diagram explains artifact identity through engine ownership |
| DOCS.DIAGRAMS.1 | planned | docs | Operator path diagram | diagram explains normal/debug/intake paths |
| DOCS.DIAGRAMS.2 | planned | docs | Runtime ladder diagram | diagram explains transformer execution path |
| DOCS.DIAGRAMS.3 | planned | docs | Integrity/refusal diagram | diagram explains corruption and drift refusal |
| DOCS.DIAGRAMS.4 | planned | docs | Eval/bench measurement map | diagram maps measurements to runtime boundaries |
| DOCS.DIAGRAMS.5 | planned | docs | Backend/hardware target matrix | diagram explains CPU/CUDA/future lanes |
| DOCS.DIAGRAMS.6 | planned | docs | README visual integration | diagrams integrated only where they clarify real boundaries |

## 9. Spine Governance Waves

This table is a project-control ledger. It is not an implementation ledger. It
records how the spine itself evolved.

SPINE.* rows control the project. They do not implement runtime behavior. They
live in the governance table, not in the implementation ledger.

| ID | Status | Governance kind | What it changed | Runtime capability change | Replacement / current role |
| --- | --- | --- | --- | --- | --- |
| SPINE.REBASE.1 | complete | historical rebase | runtime ladder through M8 | none | superseded by track model |
| SPINE.REBASE.2 | complete | historical rebase | M3/M4 planning | none | superseded by track model |
| SPINE.REBASE.3 | complete | historical rebase | runtime/operator roadmap | none | superseded by track model |
| SPINE.REBASE.4 | complete | historical rebase | artifact integrity and measurement target planning | none | superseded by track model |
| SPINE.REBASE.5 | complete | ledger consolidation | unified ledger creation | none | historical only; superseded by Implementation Delivery Ledger + Track Matrix split |
| SPINE.BLOCKS.0 | complete | ownership doctrine | canonical block directory | none | moved to Ownership Blocks appendix |
| SPINE.BLOCKS.1 | planned | planned cleanup | planned row deduplication | none | superseded by SPINE.COLLAPSE.0 unless still needed |
| SPINE.OPERATOR.PRESET.0 | complete | operator doctrine | operator preset roadmap | none | folded into TRACK.OPERATOR |
| SPINE.GENERATION.TARGET.0 | complete | target doctrine | DeepSeek generation/throughput target envelope | none | folded into TRACK.GENERATION / TRACK.BENCH / Boundary Registry |
| SPINE.SPEC.VERIFICATION.0 | complete | speculative doctrine | token verification and verification-cost doctrine | none | folded into TRACK.SPEC |
| SPINE.SEQUENCE.REBASE.0 | complete | sequence normalization | runtime sequences A-I | none | collapsed into Track Detail Sheets |
| SPINE.V0_1_0.MASTER.0 | historical | release architecture | early v0.1.0 release spine | none | superseded by SPINE.V0_1_0.MASTER.1 |
| SPINE.V0_1_0.MASTER.1 | complete | release architecture | exhaustive v0.1.0 track expansion | none | collapsed into Track Detail Sheets and Critical Path |
| SPINE.V0_1_0.MASTER.2 | complete | row contract doctrine | row-level delivery contracts | none | moved to Row Contract appendix |
| SPINE.AUDIT.0 | complete | audit | whole-repo implementation/spine audit | none | audit history |
| SPINE.RECONCILE.0 | complete | reconciliation | evidence taxonomy and namespace rules | none | audit/reconciliation history |
| SPINE.NAVIGATION.0 | complete | navigation | control panel and runtime maps | none | superseded by current reading model |
| SPINE.REDESIGN.0 | complete | information architecture | track-based redesign | none | foundation for current layout |
| SPINE.COLLAPSE.0 | complete | structural collapse | collapsed legacy blocks, sequences, numbered V010 tracks, supersession map, and SPINE/meta rows into the semantic TRACK.* model | none | current spine structure |
| SPINE.METAL.QWEN.0 | complete | pressure lane doctrine | Qwen/Metal lane | none | folded into TRACK.BACKEND / TRACK.RESIDENCY / TRACK.POST010 |
| SPINE.TESTMAP.0 | planned | evidence follow-up | planned test-to-ledger map | none | folded into TRACK.CI / Evidence Crosswalks if complete, otherwise planned |
| SPINE.FILEMAP.0 | planned | evidence follow-up | planned file ownership map | none | folded into TRACK.CI / Evidence Crosswalks if complete, otherwise planned |
| SPINE.COMMAND.AUDIT.0 | planned | audit follow-up | command/help/output-boundary audit | none | future audit note, not runtime work |
| SPINE.CLAIM.AUDIT.0 | planned | audit follow-up | public/internal claim audit | none | future release audit note |
| SPINE.CAPABILITY.REWRITE.0 | contingency | audit follow-up | capability rewrite if overclaim recurs | none | inactive contingency |
| SPINE.PUBLIC.CLAIM.0 | contingency | audit follow-up | public claim correction if needed | none | inactive contingency |

## 10. Evidence Crosswalks

Evidence crosswalks map commands and tests to evidence stages. They are proof
indexes, not runtime capability by themselves.

### 10.1 Command-to-Evidence Crosswalk

| Command family | Current stage | Proves | Does not prove | Related tracks |
| --- | --- | --- | --- | --- |
| `yvex inspect` / `metadata` / `tensors` | artifact-inspection | GGUF metadata, tensor directory, descriptor facts | runtime execution, model support, generation | TRACK.ARTIFACT, TRACK.INTEGRITY |
| `yvex integrity` | integrity-proof | identity, digest/range/shape/dtype checks, corruption refusal | supply-chain security beyond checks, model execution | TRACK.INTEGRITY |
| `yvex materialize` | selected-slice-proof | bounded selected tensor materialization and cleanup | graph execution, prefill, decode, generation | TRACK.INTEGRITY, TRACK.RESIDENCY |
| `yvex graph --execute-op/block/layers/check` | fixture-proof | primitive/block/layer/check proofs with checksum/reference/cleanup | full transformer runtime, generation | TRACK.GRAPH, TRACK.ATTENTION, TRACK.MOE |
| `yvex prefill` | diagnostic-runtime | segment-summary/layer-backed/chunked diagnostic prefill and optional diagnostic KV binding | full transformer prefill, attention-backed KV writes | TRACK.PREFILL, TRACK.KV |
| `yvex kv report` | report-only / diagnostic | KV class facts, diagnostic KV boundary, layout/capacity blockers | full runtime KV allocation/write/read | TRACK.KV |
| `yvex context report` | report-only | requested/active context, token counts, chunking and overflow policy | long-context runtime or generation context | TRACK.CONTEXT |
| `yvex attention report` | report-only | attention class, Q/K/V/O requirements, RoPE/mask/KV/context blockers | full transformer attention, QKV projection, KV writes | TRACK.ATTENTION |
| `yvex decode` | diagnostic-runtime | bounded diagnostic decode-state step | decode over full-runtime KV | TRACK.DECODE |
| `yvex logits` | diagnostic-runtime | bounded diagnostic logits buffer | output-head logits | TRACK.LOGITS |
| `yvex sample` | diagnostic-runtime | deterministic greedy selection over diagnostic logits | stochastic sampling, vocabulary sampling, generation quality | TRACK.SAMPLING |
| `yvex generate` | diagnostic-runtime | bounded prefill/decode/logits/sample/append/stop/cleanup | full model generation, DeepSeek generation, provider generation, eval, benchmark, tok/s | TRACK.GENERATION, TRACK.RUNTIME |
| `yvex fullmodel ...` | report-only / selected-slice-proof | inventory, descriptors, family reports, tiny proof/refusals | full model execution | TRACK.MODEL, TRACK.TENSOR, TRACK.INTEGRITY |
| `yvex models prepare/check`, `paths`, `model-target` | operator-preset/report-only | operator-local paths, selected artifact prepare/check composition | new runtime semantics beyond lower commands | TRACK.OPERATOR, TRACK.TARGET |
| `yvexd status/models` | server-status-shell | daemon/provider status and registry visibility | provider generation, streaming, compatibility | TRACK.SERVE |

### 10.2 Test-to-Evidence Crosswalk

| Test family | Covers | Evidence stage | Missing or weak coverage | Follow-up row |
| --- | --- | --- | --- | --- |
| docs/source/surface tests | docs boundaries, source layout, command surface hygiene | docs/guardrail | no exhaustive row-to-test map | SPINE.TESTMAP.0 |
| GGUF/parser/artifact tests | parse, metadata, tensor tables, fixtures | artifact-inspection | none critical | none |
| artifact integrity tests | digest, corruption, ranges, shape/dtype/rank, refusal | integrity-proof | none critical | none |
| registry/model alias/path tests | registry, path resolution, prepare/check presets | operator-preset | no complete ledger row map | SPINE.TESTMAP.0 |
| materialization tests | CPU/CUDA selected materialization, gates, cleanup/refusal | selected-slice-proof | full-runtime materialization planned | V010.FULLMODEL.* |
| graph primitive/block/layer tests | primitives, controlled block/layers, selected graph slices | fixture/selected-slice proof | full transformer integration missing | V010.GRAPH.*, V010.ATTN.*, V010.MOE.* |
| prefill/KV/decode/logits/sample/generate tests | bounded diagnostic runtime chain | diagnostic-runtime | full-runtime path missing | V010.PREFILL.*, V010.KV.*, V010.GEN.* |
| CUDA tests | CUDA probe, movement, op parity when available | backend evidence | host availability varies | V010.BACKEND.*, V010.HARDWARE.* |
| artifact guardrail scans | no model artifacts in git; tiny GGUF fixtures only | guardrail | manual release transcript required | V010.CI.* |
| forbidden claim scans | no forbidden release/generation/benchmark wording | guardrail | allowlist discipline | V010.CI.* |

### 10.3 File-to-Ownership Follow-up

`SPINE.FILEMAP.0` remains planned. It will map source/header/test files to
canonical ownership blocks and track families without runtime capability change.

## 11. Audit Notes

Audit and reconciliation are project-control evidence. They should guide next
work, but they are not the primary runtime architecture interface.

SPINE.AUDIT.0 found no P0 runtime contradiction. It found spine-coherence risks
around evidence-stage taxonomy, namespace duplication, current capability
readability, and report-only/runtime boundaries.

SPINE.RECONCILE.0 classified evidence stages, normalized forward namespace
rules, added command/test crosswalks, and returned Active Next to MOE.CLASS.0
after resolving the docs/meta blocker.

SPINE.COLLAPSE.0 resolves the structural duplication left by the redesign:
`TRACK.*` is now the forward model, the Implementation Delivery Ledger is
historical evidence, SPINE rows live in governance, and doctrine/legacy maps live
in appendices.

Current weak points remain implementation work, not spine blockers: full-runtime
graph path, MoE model-class facts, output-head/tokenizer path,
evaluation/benchmark readiness, and source/file/test coverage maps.

## 12. Boundary Registry

This is the central non-claim registry. Other sections should point here instead
of scattering unsupported walls through the main route.

### 12.1 Unsupported / Not Advanced

```text
full model execution
full DeepSeek materialization
full GGUF conversion
GLM runtime execution
GLM generation
SSD-backed tensor paging
disk-backed generation
huge-MoE generation
full transformer attention
QKV projection from model tensors
attention-backed KV writes
KV reads by decode
full transformer prefill
full model decode
output-head logits-producing runtime path
vocabulary sampling
stochastic sampling
full model generation
DeepSeek generation
EOS-backed tokenizer stop
OS signal cancellation
server/provider cancellation
streaming cancellation
provider generation endpoint
streaming generation
evaluation suite
capability eval
inference benchmarks
benchmark throughput
benchmark performance
execution_ready true-state claim
generation_ready true-state claim
v0.1.0 release
```

### 12.2 Standard Boundary Vocabulary

| Boundary | Meaning |
| --- | --- |
| report-only | command emits facts but does not execute runtime |
| diagnostic-runtime | runs diagnostic state, not full model semantics |
| selected-slice | bounded artifact slice, not full model |
| source-only | source/intake/storage/model-class only |
| not full model generation | no full transformer generation path |
| not DeepSeek generation | no DeepSeek V4 Flash generation |
| not benchmark | no measured runtime path with reproducibility metadata |
| external-reference | paper/external runner/external GGUF only |

### 12.3 External Reference Boundaries

External GGUFs, external runners, papers, and speculative decoding systems are
reference evidence only. They cannot close YVEX source-to-artifact, runtime,
generation, evaluation, benchmark, or throughput rows.

### 12.4 Benchmark and Throughput Boundaries

No benchmark or tok/s claim is valid before the measured runtime path exists and
the harness records model identity, artifact identity, qtype, context, backend,
machine, command, run count, and reproducibility metadata.

## 13. Doctrine Appendix

Long doctrine, row contracts, ownership blocks, and legacy migration notes live
here. They are guardrails and historical context, not competing forward
roadmaps. Use `TRACK.*` for planning.

### 13.0 Appendix Router

| Appendix area | Purpose |
| --- | --- |
| Implementation Doctrine | completion rules and no-scaffold doctrine |
| Open-Weight Intake Doctrine | official source tensor and YVEX-produced artifact rules |
| Target Pressure Doctrine | DeepSeek, GLM, Qwen/Metal, and future lanes |
| DeepSeek Target Doctrine | internal generation target and non-claim boundaries |
| Qwen/Metal Pressure Doctrine | future portability lane |
| Storage-Stream Doctrine | storage and residency planning boundaries |
| Speculative / Verification-Cost Doctrine | future draft/verify/accounting rules |
| Research CLI Doctrine | tensor/runtime inspection surface |
| Paper/Algorithm Doctrine | paper-backed stage ladder |
| Generation Contract Doctrine | loop, trace, stop, failure, and cleanup contracts |
| Operator Preset Contracts | path, prepare, check, graph, chat contracts |
| Ownership Blocks Appendix | ownership domains only; not forward roadmap |
| Legacy Row Migration Appendix | old namespace to semantic track migration |
| Non-Negotiable Rules | global claim and artifact guardrails |

### 13.0.1 Legacy Row Migration Appendix

| Legacy namespace | New home | Status |
| --- | --- | --- |
| OWI.TARGETS.* | TRACK.SOURCE / TRACK.TARGET | absorbed |
| OWI.HUGE.* | TRACK.SOURCE / TRACK.STORAGE | absorbed |
| STORAGE.STREAM.* | TRACK.STORAGE | absorbed |
| MODEL.CLASS.* | TRACK.MODEL | absorbed |
| TENSOR.COLLECTION.* | TRACK.TENSOR | absorbed |
| RESIDENCY.* | TRACK.RESIDENCY | absorbed |
| COMPUTE.BACKEND.* / HARDWARE.PROFILE.* / BACKEND.PROFILE.* | TRACK.BACKEND | absorbed |
| PREFILL.* planned | TRACK.PREFILL | absorbed |
| KV.MIN.* / RUNTIME.KV.* | TRACK.KV | absorbed |
| DECODE.* planned | TRACK.DECODE | absorbed |
| LOGITS.* planned | TRACK.LOGITS | absorbed |
| SAMPLING.* planned | TRACK.SAMPLING | absorbed |
| GEN.* planned | TRACK.GENERATION | absorbed |
| CLI.UX.* / MODEL.LIFECYCLE.* | TRACK.OPERATOR | absorbed |
| SERVER.* | TRACK.SERVE | absorbed |
| EVAL.* | TRACK.EVAL | absorbed |
| BENCH.* | TRACK.BENCH | absorbed |
| SPEC.* | TRACK.SPEC | absorbed |
| PAPER.INDEX.0 / ALGORITHM.MODES.0 | TRACK.DOCS / TRACK.SPEC | absorbed |
| CLI.RESEARCH.0 | TRACK.OPERATOR / TRACK.SPEC | absorbed |
| LAYOUT.* | TRACK.CI / appendix history | absorbed |
| DOCS.* | TRACK.DOCS / TRACK.RELEASE | absorbed |
| SPINE.* | Spine Governance Waves | separated |

### 13.0.2 Ownership Blocks Appendix

Blocks define ownership. They are not the forward roadmap. Use `TRACK.*` for
planning.

### 13.1 Implementation Doctrine

Primary preserved source: earlier `## 2. Implementation Doctrine` and procedural
implementation rules.

### 13.2 Open-Weight Intake Doctrine

Primary preserved source: open-weight intake doctrine and source tensor rules.

### 13.3 Model Target Doctrine

Primary preserved source: target classes, DeepSeek/GLM/Qwen pressure targets,
and external reference boundaries.

### 13.4 Qwen/Metal Pressure Lane Doctrine

Primary preserved source: Qwen Metal pressure lane doctrine.

### 13.5 Storage-Stream Doctrine

Primary preserved source: storage stream and residency doctrine.

### 13.6 DeepSeek Generation Target Doctrine

Primary preserved source: DeepSeek generation and throughput target doctrine.

### 13.7 Speculative Decoding Doctrine

Primary preserved source: speculative decoding reference track and DSpark
reference boundaries.

### 13.8 Token Verification and Verification-Cost Doctrine

Primary preserved source: token verification and routing-aware verification cost
doctrine.

### 13.9 Research CLI Doctrine

Primary preserved source: YVEX research CLI doctrine and CLI research matrix.

### 13.10 Paper-Backed Algorithm Doctrine

Primary preserved source: paper-backed implementation ladder.

### 13.11 Algorithm Families

Primary preserved source: attention, prefill, KV, decode, logits, sampling,
generation, residency, and storage-stream families.

### 13.12 Generation Contracts

Primary preserved source: generation loop/state/token/stop/trace/failure
contracts.

### 13.13 Operator Preset Command Contracts

Primary preserved source: operator preset command contracts.

### 13.14 Non-Negotiable Rules

Primary preserved source: non-negotiable rule list.

### 13.15 Preserved Doctrine and Guardrail Text

### 13.1 Authority

`docs/spine.md` is the only internal delivery map. Public docs must not expose
delivery IDs, status ledgers, handoff language, or implementation diary text.

Status changes in this file require implemented behavior, tests, command proof,
failure-path coverage, cleanup/lifecycle behavior, and explicit boundaries. The
spine must not collapse artifact parsing, tensor residency, engine ownership,
graph execution, token input, prefill state, KV, decode, logits, sampling, and
generation into one vague inference milestone.

Git history should tell the technical story in natural subjects. Internal spine
IDs may appear here and in final reports, but commit subjects should describe
behavior, not milestone labels.

### 13.1.1 Engine Identity

YVEX is an inference engine, not a chat wrapper.

Its primary object is the model execution pipeline:

```text
official source tensors
  -> YVEX-produced artifacts
  -> artifact identity
  -> model-family mapping
  -> tensor collections
  -> residency
  -> graph execution
  -> runtime state
  -> prefill
  -> KV
  -> decode
  -> logits
  -> sampling
  -> generation
  -> serving
  -> evaluation
  -> benchmark/profile evidence
```

YVEX must remain transparent about which parts are implemented, which parts are
planned, and which observations come from external runners.

YVEX may compare itself with llama.cpp, KTransformers, vLLM, SGLang, Ollama,
DwarfStar/DS4, and other systems only as external reference systems. Such
comparisons must identify whether the evidence comes from YVEX code or from an
external runtime.

### 13.1.2 Legacy Reading Model

This spine contains multiple row classes. They are not equivalent.

```text
implementation row:
  changes executable behavior and must have code, command proof, tests, failure
  paths, cleanup/lifecycle behavior, and explicit boundary.

report row:
  exposes command-visible facts, blockers, or classifications without executing
  runtime behavior.

diagnostic-runtime row:
  runs implemented local runtime state but does not claim full model semantics.

selected-slice row:
  runs or materializes a bounded YVEX-produced slice of a real artifact but does
  not claim full model execution.

meta-spine row:
  organizes doctrine, sequence, naming, contracts, audits, or roadmap structure.
  It does not change runtime capability.

release row:
  closes packaging, validation, docs, version, tag, or release gates. It cannot
  create runtime behavior.

reference row:
  records papers, external systems, external GGUFs, or external runners as
  reference evidence only.
```

Only implementation rows can increase runtime capability.
Report rows can increase inspection capability.
Diagnostic-runtime rows can close local control-flow behavior.
Selected-slice rows can prove bounded artifact/graph/runtime facts.
Meta-spine rows increase project control but not runtime behavior.
Release rows close release readiness but not model execution.

Reader vocabulary:

```text
block:
  canonical ownership domain.

track:
  grouped roadmap lane, usually inside v0.1.0.

sequence:
  ordered dependency chain.

row:
  smallest planned or completed delivery boundary.

wave:
  one execution package sent to the AI/Codex worker.

gate:
  release or evidence condition.

claim:
  human-readable capability statement that must be backed by evidence.

boundary:
  explicit statement of what a row does not prove.
```

When reading Current Capability, always ask which evidence stage the line belongs
to: implementation, report-only, diagnostic-runtime, selected-slice, docs/meta,
operator workflow, or refactor/quality.

### v0.1.0 Release Doctrine

YVEX v0.1.0 is the first internally coherent end-to-end inference-engine release
target.

v0.1.0 is not a marketing release.

v0.1.0 is not defined by chat UX, provider compatibility, speculative decoding,
external benchmark comparison, or public speed claims.

v0.1.0 is defined by one honest implemented model execution path:

```text
model target
-> source/artifact identity
-> YVEX-produced or YVEX-controlled artifact
-> artifact integrity
-> model-class profile
-> tensor collections
-> residency plan
-> backend capability
-> graph execution
-> real prefill
-> real KV writes/reads
-> real decode
-> real output-head logits
-> real vocabulary sampling
-> token append
-> stop/failure policy
-> cleanup/lifecycle behavior
-> CLI-visible generation
-> validation transcript
-> release boundary
```

v0.1.0 may close on a feasible dense or small/medium model target first if that
is the fastest honest path to a real end-to-end engine.

DeepSeek V4 Flash remains the primary semi-frontier pressure target, but
DeepSeek V4 Flash full generation is not automatically the minimum v0.1.0
release blocker unless explicitly chosen by a v0.1.0 target decision row.

The rule is:

```text
close one real full-runtime path first;
keep DeepSeek pressure explicit;
keep GLM source/storage pressure explicit;
keep Qwen/Metal portability pressure explicit;
never claim a target that did not run through its own implemented path.
```

### v0.1.0 Target Policy

YVEX has multiple target classes:

```text
selected-runtime-slice:
  proves parser/materialization/partial graph/diagnostic runtime boundaries.

full-runtime-candidate:
  candidate model/artifact small enough to close end-to-end generation.

semi-frontier-pressure:
  DeepSeek V4 Flash path, used to force real architectural pressure.

huge-source-pressure:
  GLM-5.2 official source tensors, used to force source/shard/storage/model-class pressure.

portability-pressure:
  Qwen/Metal path, used to force backend portability and reduced-scale runtime planning.

reference-only:
  external GGUFs, external runners, papers, and public systems.
```

v0.1.0 requires at least one full-runtime-candidate to reach honest CLI
generation.

A selected-runtime-slice cannot close v0.1.0 full runtime.

A source-only huge model cannot close v0.1.0 full runtime.

An external runner cannot close v0.1.0 runtime.

A public benchmark cannot substitute for YVEX command proof.

### v0.1.0 Acceptance Gates

```text
GATE.SCOPE:
  release target, included tracks, excluded tracks, and post-v0.1.0 tracks are explicit.

GATE.ARTIFACT:
  artifact identity, digest, tensor ranges, dtype/qtype, shape, registry, and drift checks pass.

GATE.CLASS:
  model class, target class, dense/MoE/source-only/selected-slice path, output-head,
  tokenizer, and runtime blockers are command-visible.

GATE.TENSOR:
  all required runtime tensor collections for the v0.1.0 target are mapped.

GATE.RESIDENCY:
  all required tensor/runtime-state collections have residency decisions or explicit unsupported blockers.

GATE.BACKEND:
  CPU baseline and selected backend capability reports exist.

GATE.GRAPH:
  implemented graph path over selected v0.1.0 target exists with reference/checksum/tolerance/failure reports.

GATE.PREFILL:
  real prefill path exists for the selected runtime target.

GATE.KV:
  real KV allocation/write/read/cleanup exists for the selected runtime target.

GATE.DECODE:
  real decode step exists over real runtime state.

GATE.LOGITS:
  real output-head logits exist over the selected runtime path.

GATE.SAMPLE:
  at least greedy sampling over real logits exists.

GATE.TOKENIZER:
  token ID input/output and stop policy boundary are explicit.

GATE.GEN:
  CLI generation composes real prefill, KV, decode, logits, sampling, append, stop, cleanup.

GATE.FAILURE:
  failure phases, cleanup, partial state, and refusal behavior are tested.

GATE.OPERATOR:
  short operator commands exist for prepare/check/generate or equivalent v0.1.0 path.

GATE.EVIDENCE:
  validation transcript exists; benchmark claims remain absent unless measured.

GATE.PUBLIC:
  public docs do not expose internal IDs and do not overclaim.

GATE.RELEASE:
  version, tag, changelog/release notes, clean tree, artifact guardrail, and claim audit pass.
```

### v0.1.0 Non-Goals

- full DeepSeek V4 Flash generation unless explicitly implemented;
- DeepSeek >=20 tok/s claim;
- speculative decoding;
- DSpark/DFlash/HyperDFlash parity;
- provider/server generation before CLI/runtime generation;
- streaming before real generation;
- OpenAI/Anthropic compatibility before server generation;
- capability evaluation suite;
- public benchmark table;
- GLM full runtime;
- GLM YVEX-produced full GGUF unless implemented;
- Qwen/Metal runtime unless Metal backend and Qwen path are implemented;
- SSD-streamed generation unless storage stream is consumed by runtime;
- distributed inference;
- production serving SLA;
- security/supply-chain guarantees beyond implemented integrity gates.

### 13.2 Implementation Doctrine

No scaffold milestone is complete. A file, API shape, command option,
placeholder executor, future hook, or empty provider stub is not an
implementation boundary unless the same wave also provides real behavior that
depends on it.

A completion decision needs all of the following:

```text
implemented behavior
command-visible or API-visible proof
tests
error paths
cleanup/lifecycle behavior
explicit boundary documentation
no unsupported capability claim
```

Normal operator paths should be short, discoverable, and preset-driven where
possible. Long flags belong to diagnostics, gates, provenance, and exact CI
checks. Help should show the short path first and advanced flags later.

DeepSeek selected artifacts are the current live pressure targets. They are
large enough to force real parser, layout, backend, transfer, cleanup, graph,
and failure boundaries. They are not the whole system. Runtime code must remain
model-family-aware, with family mapping and runtime adapters made explicit
rather than hidden behind generic support claims.

Open-weight intake doctrine:

Open-weight intake starts from official source tensors and source manifests.
YVEX-produced artifacts are downstream products of that intake path. Downloaded
external GGUFs may be useful as reference evidence, but they cannot satisfy a
YVEX open-weight intake, conversion, quantization, or GGUF-emission milestone.

The canonical OWI chain is:

```text
official source tensors
  -> source manifest
  -> safetensors/native tensor inventory
  -> family/model-class profile
  -> tensor role mapping
  -> quantization policy
  -> calibration/imatrix evidence when required
  -> YVEX conversion/quantization job
  -> YVEX-produced GGUF
  -> YVEX artifact identity
  -> registry/materialization/storage/runtime reports
```

A model target is not a capability claim. It is a pressure object used to force
parser, artifact identity, tensor inventory, qtype accounting, model-family
mapping, storage residency, memory budget, graph requirements, and failure
behavior.

External GGUF and external runner evidence:

External GGUFs and external runners are reference evidence only. They may be
used to compare artifact size, shard layout, qtype choices, deployment
constraints, or external runtime behavior. They are not YVEX-produced artifacts,
not YVEX runtime execution, not YVEX generation, not YVEX benchmarks, and not
YVEX model support.

Model target doctrine:

YVEX must not be validated against only one open-weight family.

DeepSeek selected GGUF artifacts remain the live selected-runtime-slice pressure
target. They force real parser, identity, selected materialization, backend
residency, graph execution, graph guard, and cleanup boundaries over manageable
YVEX-produced artifacts.

GLM-5.2 official safetensors become the first huge-MoE source-tensor pressure
target. GLM forces OWI to handle huge source tensor inventory, multi-shard
safetensors, model-class profiling, MoE metadata, future quantization policy,
future YVEX-produced GGUF emission, storage layout, and storage-stream planning.

GLM-5.2 does not replace DeepSeek. GLM-5.2 does not promote execution,
prefill, decode, logits, sampling, generation, eval, or benchmark readiness.

Qwen on Apple Silicon / Metal becomes a reduced-scale full-runtime
portability pressure lane. Qwen-Metal forces backend portability, Metal
feasibility, unified-memory residency, Qwen model-class mapping, and
small/medium full-runtime planning. It is not the primary DeepSeek generation
target and not the huge-MoE source-tensor target.

Canonical model and hardware pressure relationship:

```text
DeepSeek / Spark / CUDA:
  selected-runtime-slice and future semi-frontier full-generation pressure.

GLM-5.2 / source tensors / storage-stream:
  huge-MoE source tensor and storage pressure.

Qwen / MacBook / Metal:
  reduced-scale Apple Silicon pressure lane for backend portability,
  unified-memory residency, source-to-artifact flow, and future small/medium
  full-runtime generation.

Strix Halo / ROCm:
  future AMD/ROCm pressure lane using the same staged doctrine.
```

### 13.4 Qwen/Metal Pressure Lane Doctrine

Qwen-Metal is a reduced-scale full-runtime pressure lane for Apple Silicon.

It exists to force:
- Metal backend feasibility;
- Apple unified-memory residency planning;
- Qwen source tensor intake;
- Qwen tensor-role mapping;
- Qwen YVEX-produced artifact identity;
- selected Qwen materialization;
- Metal primitive parity;
- future small/medium model generation over YVEX runtime.

Qwen-Metal does not replace DeepSeek.
Qwen-Metal does not replace GLM.
It does not prove Metal support.
It does not prove Qwen runtime support.
It does not prove full model generation.
It does not prove benchmark readiness.

External Qwen GGUFs and external Apple/Metal runners are reference evidence
only. They cannot close YVEX source-to-artifact, backend, runtime, generation,
eval, or benchmark rows.

Qwen-Metal follows the same YVEX chain:

```text
official source tensors
-> source manifest
-> native tensor inventory
-> model-class profile
-> tensor role mapping
-> quantization policy
-> YVEX-produced GGUF
-> artifact identity
-> materialization
-> residency
-> Metal backend capability
-> graph execution
-> real transformer prefill
-> KV
-> decode
-> logits
-> sampling
-> generation
-> eval/benchmark only after implemented runtime path exists
```

Storage-stream doctrine:

Storage streaming is a planned residency mode for artifacts whose full tensor
set does not fit comfortably in GPU memory or system memory. It starts from
artifact inventory, shard indexing, tensor byte-range mapping, cold/warm read
behavior, cache policy, and staged residency. It is not generation.

No storage-stream milestone may claim runtime generation until the normal
runtime generation path reaches decode, logits, sampling, and generation.

DeepSeek generation target doctrine:

DeepSeek V4 Flash is the current internal full-generation pressure target.

The current selected DeepSeek artifacts remain selected-runtime-slice targets.
They prove parser, artifact identity, selected materialization, backend
residency, graph execution, graph guard, and cleanup behavior over manageable
YVEX-produced artifacts.

The final DeepSeek V4 Flash target is larger:

```text
official safetensors
  -> source manifest
  -> native tensor inventory
  -> family mapping
  -> tensor collections
  -> quantization policy
  -> YVEX-produced GGUF
  -> artifact identity
  -> registry
  -> materialization
  -> residency
  -> graph layers
  -> real transformer prefill
  -> KV
  -> decode
  -> logits
  -> sampling
  -> generation
  -> CLI/server
  -> benchmark
  -> speculative acceleration
```

This target is not current capability.

Today, YVEX full model generation is unsupported and real YVEX generation
throughput is zero because real DeepSeek decode, real output-head logits, real
vocabulary sampling, and the full model generation loop do not exist.

YVEX real generation throughput: 0 tok/s

benchmark status: not measured

A bounded diagnostic generation loop may close local runtime control flow
before DeepSeek V4 Flash full generation is complete. That bounded loop is not
a DeepSeek throughput claim and not full model generation.

The internal performance target for DeepSeek V4 Flash is to exceed a reference
decode threshold of 15 tokens/sec and pursue at least 20 tokens/sec baseline
decode on a GB10-class CUDA machine, after and only after the real generation
path exists and the benchmark harness records model identity, artifact identity,
qtype, context, backend, machine, command, and reproducibility metadata.

The 20 tokens/sec target is an internal engineering target, not a guarantee and
not a public claim.

DeepSeek V4 Flash >=20 tok/s remains an internal target until measured by the
benchmark harness over an implemented full generation path.

Speculative decoding is a later acceleration track. It may be considered only
after baseline decode, logits, sampling, and generation exist.

DSpark reference doctrine:

DSpark is external reference evidence for future speculative decoding design.

The DSpark paper describes confidence-scheduled speculative decoding with
semi-autoregressive generation. Its relevance to YVEX is architectural:

```text
baseline autoregressive decode remains necessary;
draft generation may later propose multiple tokens;
confidence or acceptance prediction may guide how many draft tokens to verify;
target-model verification remains the correctness boundary;
hardware-aware scheduling may shift throughput/interactivity tradeoffs.
```

YVEX may use DSpark as a reference for a future speculative acceleration track.

YVEX must not present DSpark throughput, DSpark acceptance length, DeepSeek
serving numbers, or external runner measurements as YVEX benchmark results.

A DSpark-like acceleration row cannot be promoted before YVEX implements:

```text
baseline generation;
target-model decode;
logits;
sampling;
token append state;
verification semantics;
acceptance accounting;
benchmark harness.
```

External speculative decoding evidence is not YVEX runtime execution.

### Token Verification and Verification-Cost Doctrine

Speculative generation is not only draft decoding.

YVEX separates speculative generation into:

```text
draft proposal:
  a draft path proposes one or more candidate tokens.

target verification:
  the target model verifies candidate tokens and determines the accepted prefix.

verification-cost control:
  the runtime estimates whether verifying the proposed token set is cheaper
  than normal baseline decode.

accepted-token accounting:
  only target-verified tokens may become accepted/generated tokens.

KV and runtime-state accounting:
  accepted tokens update runtime state; rejected tokens must not corrupt KV,
  sequence state, logits state, expert state, traces, or cleanup accounting.
```

For dense models, target verification cost is mostly controlled by target
forward cost, draft-target alignment, accepted prefix length, batching behavior,
and draft overhead.

For MoE models, target verification cost also depends on router execution,
expert fanout, expert residency, expert dispatch, expert memory movement,
expert activation balance, and KV consistency.

A MoE speculative path must therefore be verification-cost-aware, not only
confidence-aware.

YVEX may later track speculative verification utility as:

```text
accepted_token_gain / target_verification_cost
```

For MoE targets, `target_verification_cost` may include:

```text
router work
activated expert count
unique expert fanout
expert residency state
expert bytes moved
dispatch/combination overhead
KV mutation or rollback cost
target logits cost
```

A high-confidence draft is not automatically useful if target verification
activates expensive or non-resident experts.

A lower-confidence draft may still be useful if target verification remains
cheap and produces accepted tokens.

This doctrine does not implement speculative decoding, routing-aware
verification, expert budgeting, KV rollback, accepted-token accounting, or
benchmarking.

### Model-Class Dynamic Runtime Doctrine

YVEX runtime paths are selected by model class and target class.

YVEX must not assume that the DeepSeek/MoE path is universal.

A dense target must not be forced through router/expert rows. A MoE target must
not be forced through a dense-only MLP path. A source-only target must not be
forced into runtime execution. A selected-runtime-slice artifact must not be
promoted into a full-runtime model.

Model class determines runtime path:

```text
dense -> dense transformer path
MoE -> router/expert transformer path
source-only -> intake/storage/model-class path only
selected-runtime-slice -> bounded partial/diagnostic path only
```

Dense models use a dense transformer path:

```text
embedding
-> normalization
-> attention
-> dense MLP
-> output head
-> logits
-> sampling
-> generation
```

MoE models use a router/expert transformer path:

```text
embedding
-> normalization
-> attention
-> router
-> expert selection
-> expert dispatch
-> expert accumulation
-> output head
-> logits
-> sampling
-> generation
```

Selected-runtime-slice artifacts may close parser, materialization, partial
graph, and diagnostic runtime boundaries. They do not close full model
execution.

Source-only huge models may close source intake, storage, model-class, and
planning boundaries. They do not close runtime execution.

Qwen/Metal targets use the same staged doctrine through a future
backend-specific path. They do not imply Metal support, Qwen runtime support,
or Qwen generation.

Speculative paths are selected only after baseline generation exists. Dense
speculative verification and MoE routing-aware verification are separate future
acceleration families.

### 13.13 Ownership Blocks Appendix - Canonical Block Directory

YVEX implementation blocks:

```text
BLOCK 0 — Source and target evidence
  owns:
    official source references, local source storage facts, source manifests,
    target class registry, target pressure class, source artifact class.
  does not own:
    conversion, runtime materialization, graph execution, generation.
  entry evidence:
    known upstream model source or selected artifact target.
  exit evidence:
    source/target report with class, source path, artifact class, missing status,
    and unsupported runtime boundary.
  common failure modes:
    missing source, incomplete download, source-only target mistaken for runtime,
    external GGUF mistaken for YVEX artifact.
  typical command proof:
    yvex model-target inspect TARGET --paths
    yvex source-manifest ...
    yvex native-weights ...
  v0.1.0 related tracks:
    V010.TARGET.*
    V010.SOURCE.*
  Current rows: OWI.REBASE.0, OWI.TARGETS.*, OWI.HUGE.*
  Normal proof: source manifest, native tensor inventory, shard count, source
  footprint, identity/drift report.

BLOCK 1 — Artifact production
  owns:
    tensor mapping, quant policy, imatrix/calibration contract, conversion plan,
    GGUF emission, artifact naming.
  does not own:
    artifact integrity after emission, runtime materialization, graph execution.
  entry evidence:
    source tensor inventory and tensor role mapping.
  exit evidence:
    YVEX-produced or controlled artifact with parse roundtrip and registry entry.
  common failure modes:
    unsupported qtype, missing tensor role, invalid template, external artifact
    used as produced artifact.
  typical command proof:
    yvex gguf-template ...
    yvex convert ...
    yvex models prepare TARGET
  v0.1.0 related tracks:
    V010.MAP.*
    V010.QUANT.*
    V010.ARTIFACT.EMIT.*
  Current rows: OWI.3-OWI.9, ARTIFACT.NAMING.0, OWI.HUGE.4.
  Normal proof: conversion plan, template validation, qtype policy, emitted
  controlled or selected GGUF, parse roundtrip.

BLOCK 2 — Artifact identity and integrity
  owns:
    file identity, digest, metadata, tensor byte ranges, dtype/shape/rank/element
    count, corruption refusal, materialization gate, graph integrity guard.
  does not own:
    model-family semantics, graph algorithms, generation.
  entry evidence:
    candidate artifact path or emitted artifact.
  exit evidence:
    integrity report, drift report, corruption refusal, materialization preflight.
  common failure modes:
    short read, corrupt header, invalid tensor directory, byte range overflow,
    digest drift, registry alias drift.
  typical command proof:
    yvex integrity report ...
    yvex materialize ...
    yvex models check ...
  v0.1.0 related tracks:
    V010.INTEGRITY.*
    V010.FULLMODEL.*
  Current rows: ARTIFACT.INTEGRITY.*.
  Normal proof: integrity report, corruption refusal, digest check, tensor range
  validation, cleanup/failure phase.

BLOCK 3 — Model class and tensor collections
  owns:
    family, architecture class, dense/MoE/source-only/selected-slice class,
    tensor collections, attention/KV/context/output/tokenizer requirements,
    runtime blockers.
  does not own:
    residency transition, graph compute, runtime mutation, generation.
  entry evidence:
    artifact/source descriptor, tensor inventory, family adapter facts.
  exit evidence:
    class/tensor report with required roles, missing roles, blockers, target
    class, and next rows.
  common failure modes:
    unknown family, ambiguous tensor role, source-only target promoted too far,
    selected-slice promoted to full runtime, dense/MoE conflation.
  typical command proof:
    yvex fullmodel descriptor ...
    yvex fullmodel family-runtime ...
    yvex attention report ...
    yvex kv report ...
    yvex context report ...
    future yvex moe report ...
  v0.1.0 related tracks:
    V010.CLASS.*
    V010.TENSOR.*
    V010.MOE.0-4
  Current rows: MODEL.CLASS.*, FAMILY.RUNTIME.0, FULLMODEL.*.
  Normal proof: model-class report, tensor collection inventory, runtime
  requirement report, unsupported blocker list.

BLOCK 4 — Residency and storage
  owns:
    resident/host-staged/SSD-staged/SSD-streamed/managed/hybrid/distributed
    residency classes, shard index, page/chunk maps, cold/warm reads, staging,
    cache/eviction, storage cleanup.
  does not own:
    compute semantics, graph correctness, generation semantics.
  entry evidence:
    artifact identity, tensor byte ranges, tensor collection requirements.
  exit evidence:
    residency/storage report with memory/storage pressure, staging/refusal
    behavior, and cleanup.
  common failure modes:
    missing shard, short read, digest mismatch, page overflow, cache
    inconsistency, storage read mistaken for generation.
  typical command proof:
    future yvex storage-plan ...
    future yvex shard-index ...
    future yvex residency-plan ...
  v0.1.0 related tracks:
    V010.STORAGE.*
    V010.RESIDENCY.*
    V010.KV.15-19
  Current rows: STORAGE.STREAM.*, RUNTIME.KV.*, future RESIDENCY.*.
  Normal proof: shard index, byte-range map, cold/warm read diagnostics,
  staged-residency report, cleanup/failure report.

BLOCK 5 — Compute backend and hardware profiles
  owns:
    CPU/CUDA/future Metal/future ROCm build capability, backend op matrix,
    allocation/transfer behavior, hardware profiles, build profiles.
  does not own:
    model support, graph algorithm ownership, generation.
  entry evidence:
    build flags, device probe, op requirement.
  exit evidence:
    backend/hardware report with supported ops, unsupported ops,
    allocation/transfer failure behavior.
  common failure modes:
    backend probe mistaken for model support, build flag mistaken for runtime
    support, hardware availability mistaken for implementation.
  typical command proof:
    yvex backend ...
    yvex cuda-info ...
    make check-cuda
    future yvex hardware-profile ...
  v0.1.0 related tracks:
    V010.BACKEND.*
    V010.HARDWARE.*
    V010.BUILD.*
  Current rows: L0, CUDA.SURFACE.0, BACKEND.PROFILE.*, BACKEND.METAL.0,
  BACKEND.ROCM.0.
  Normal proof: backend probe, op capability report, allocation/transfer test,
  parity test, hardware profile report.

BLOCK 6 — Graph execution
  owns:
    primitive ops, normalization, attention projection/execution, MLP, MoE
    slices, residuals, block/layer scheduling, scratch lifecycle, reference
    comparison.
  does not own:
    tokenizer semantics, runtime generation loop, benchmark claims.
  entry evidence:
    tensor collection mapping, backend capability, validated tensor ranges.
  exit evidence:
    graph command proof with checksum/max-diff/reference/cleanup/failure
    behavior.
  common failure modes:
    primitive mistaken for full transformer, expert-slice mistaken for MoE,
    attention primitive mistaken for model attention, selected slice mistaken
    for full runtime.
  typical command proof:
    yvex graph --execute-op ...
    yvex graph --execute-block ...
    yvex graph --execute-layers ...
    yvex graph check ...
  v0.1.0 related tracks:
    V010.GRAPH.PRIM.*
    V010.GRAPH.*
    V010.ATTN.*
    V010.MOE.*
  Current rows: GRAPH.OPS.*, GRAPH.BLOCK.0, GRAPH.LAYERS.0.
  Normal proof: command-visible graph execution, reference comparison, checksum,
  max-diff, cleanup/failure report.

BLOCK 7 — Runtime state
  owns:
    engine/session state, token input, context, prefill, KV, decode, logits,
    sampling, generation state, interruption, cleanup, traces.
  does not own:
    artifact conversion, backend implementation, server/provider compatibility,
    benchmark measurement.
  entry evidence:
    implemented graph output, runtime state, KV/state requirements.
  exit evidence:
    runtime command proof with state fields, lifecycle, failure, cleanup, and
    boundary.
  common failure modes:
    diagnostic runtime mistaken for full runtime, generated diagnostic token
    mistaken for model output, logits buffer mistaken for output-head logits,
    generation command mistaken for DeepSeek generation.
  typical command proof:
    yvex prefill ...
    yvex decode ...
    yvex logits ...
    yvex sample ...
    yvex generate ...
  v0.1.0 related tracks:
    V010.CONTEXT.*
    V010.PREFILL.*
    V010.KV.*
    V010.DECODE.*
    V010.LOGITS.*
    V010.SAMPLE.*
    V010.TOKENIZER.*
    V010.GEN.*
    V010.RUNTIME.*
  Current rows: M7-M17, PREFILL.*, KV.*, DECODE.*, LOGITS.*, SAMPLING.*,
  GEN.LOOP.*, RUNTIME.KV.*.
  Normal proof: runtime command over implemented state, lifecycle test, context
  boundary test, cleanup/interruption report.
  The DeepSeek V4 Flash full-generation target must pass through real
  transformer prefill, KV, decode, logits, sampling, and generation before any
  CLI or server generation claim exists.

BLOCK 8 — Operator and serving surfaces
  owns:
    CLI presets, help layout, normal/advanced command paths, doctor, REPL,
    daemon state, provider endpoints, streaming, observability.
  does not own:
    lower runtime semantics, model generation capability, benchmark truth.
  entry evidence:
    lower-level implemented commands and runtime surfaces.
  exit evidence:
    short operator command, help, structured output/refusal, server endpoint
    where runtime exists.
  common failure modes:
    operator preset overclaims lower behavior, server status shell mistaken for
    generation endpoint, chat shell mistaken for model chat.
  typical command proof:
    yvex commands
    yvex help ...
    yvex models prepare ...
    yvex models check ...
    yvex paths ...
    yvexd status
  v0.1.0 related tracks:
    V010.CLI.*
    V010.DOCTOR.*
    V010.PATHS.*
    V010.SERVE.*
  Current rows: CLI.UX.*, MODEL.LIFECYCLE.*, SERVER.*.
  Normal proof: short operator command, JSON/text output, daemon endpoint,
  refusal path, structured diagnostics.
  Operator presets must reduce shell glue by moving common path resolution,
  model preparation, model checks, graph suites, and diagnostic REPL clarity
  into YVEX commands.

BLOCK 9 — Evaluation, benchmarks, and public evidence
  owns:
    eval vectors, correctness regressions, benchmark harness,
    machine/backend/artifact metadata, public docs, diagrams, release evidence.
  does not own:
    runtime implementation itself.
  entry evidence:
    implemented runtime path.
  exit evidence:
    eval/benchmark proof over same path users run, with reproducibility metadata.
  common failure modes:
    fixture eval mistaken for capability eval, external runner mistaken for YVEX
    benchmark, throughput without metadata, public claim before implementation.
  typical command proof:
    future yvex eval ...
    future yvex bench ...
    docs claim audit
    release transcript
  v0.1.0 related tracks:
    V010.EVAL.*
    V010.BENCH.*
    V010.PROFILE.*
    V010.DOCS.*
    V010.RELEASE.*
    V010.CI.*
  Current rows: EVAL.*, BENCH.*, DOCS.*.
  Normal proof: same runtime path users run, model/artifact identity, backend,
  qtype, context, machine, command, reproducibility note.
  Internal throughput targets such as DeepSeek V4 Flash >=20 tok/s decode are
  engineering targets only until measured by the benchmark harness over the same
  runtime path users run.
```

### 13.2.2 Naming and Ownership Rules

Canonical nouns:

```text
source tensor:
  an upstream official tensor file, usually safetensors, before YVEX conversion.

YVEX-produced artifact:
  an artifact emitted or controlled by YVEX, such as a controlled or selected
  GGUF. This is the primary artifact class for runtime claims.

external reference artifact:
  an artifact not produced by YVEX, used only for comparison or deployment
  evidence.

model class:
  architecture-level facts: family, dense/MoE, layer count, hidden size,
  attention type, expert facts, context class, tokenizer requirements, and
  runtime blockers.

tensor collection:
  a named group of tensor roles needed by execution, such as embedding,
  normalization, Q/K/V/O projection, MLP, MoE experts, router, output head,
  tokenizer metadata, and KV cache.

residency:
  where a tensor or runtime state lives at a given phase: CPU memory, CUDA
  memory, managed memory, SSD storage, host cache, or future distributed node.

backend:
  the compute implementation used to execute operations, such as CPU, CUDA,
  future Metal, or future ROCm.

hardware profile:
  a machine-specific pressure profile, such as Spark/GB10, local workstation,
  large-memory machine, or future distributed system.

serve:
  provider/server exposure after runtime generation exists. Serving does not
  own generation semantics.

evaluation:
  correctness or capability measurement over implemented runtime paths.

benchmark:
  performance measurement over implemented runtime paths with reproducibility
  metadata.

Generation target nouns:

```text
baseline decode:
  the normal autoregressive next-token runtime path after real transformer
  prefill, KV, decode, logits, and sampling exist.

generation loop:
  the repeated decode -> logits -> sample -> append-token path with stop
  conditions, interruption, cleanup, and token accounting.

speculative decode:
  an acceleration technique that drafts candidate tokens and verifies them with
  the target model. It is not a replacement for baseline decode.

draft model:
  a smaller or auxiliary model/path that proposes candidate tokens. A draft
  model is not the authority for final accepted tokens.

target verification:
  the target model pass that verifies drafted tokens and determines accepted
  prefix length.

accepted-token accounting:
  the runtime accounting of how many proposed draft tokens survived target
  verification.

throughput target:
  an internal engineering target, not a measured benchmark and not a public
  claim.

benchmark result:
  a measured value over an implemented runtime path with model, artifact, qtype,
  context, backend, machine, command, and reproducibility metadata.
```

Operator preset nouns:

operator root:
  the configured operator-local model storage root, usually a directory such as
  `$HOME/lab/models`, but stored or resolved by YVEX rather than manually
  exported for every command.

target path:
  a source, artifact, report, registry, or reference path resolved from a model
  target and operator root.

prepare:
  a preset that builds or refreshes a YVEX-produced artifact for a known target,
  then optionally registers and selects the alias.

check:
  a preset that runs a bounded verification chain over a known target, such as
  inspect, tensor table, metadata, integrity, materialization, engine/session,
  graph, and gates according to a selected level.

graph check:
  a preset that runs a named graph suite such as primitives, block fixture,
  selected artifact graph, or selected segment graph.

diagnostic REPL:
  an interactive accepted-only console that reports runtime state clearly but
  does not claim generation.
```

Avoid these ambiguous nouns in new rows:

```text
runtime backend
server runtime
model support
inference support
storage inference
external benchmark
helper
glue
generic support
shell bootstrap
export flow
script transcript
magical support
automatic inference
one-click generation
tok/s claim
speed claim
DSpark-parity claim
speculative support
DeepSeek generation-readiness claim
DeepSeek benchmark-readiness claim
```

Use these canonical track names in future rows:

```text
OWI for source-to-artifact intake.
ARTIFACT for identity, integrity, naming, and gates.
MODEL.CLASS for architecture and model-family requirements.
TENSOR.COLLECTION for tensor role grouping.
RESIDENCY for memory/storage placement.
STORAGE.STREAM for SSD-backed and page/chunk storage planning.
COMPUTE.BACKEND for CPU/CUDA/Metal/ROCm execution backends.
HARDWARE.PROFILE for machine-specific reports.
GRAPH for graph and transformer execution.
CONTEXT for context window and prompt/prefill planning.
KV for KV cache ownership and capacity.
RUNTIME for engine/session/decode/logits/sampling/generation.
OPERATOR for CLI and local operator workflows.
SERVE for daemon/provider/API/streaming surfaces.
EVAL for correctness and capability evaluation.
BENCH for performance measurement.
DOCS for public documentation and diagrams.
```

Future row naming alignment:

```text
Older planned row families such as SERVER.*, BACKEND.PROFILE.*, CLI.UX.*,
MODEL.LIFECYCLE.*, LAYOUT.*, and DOCS.* remain in the ledger for continuity.
New work should prefer the canonical nouns OPERATOR, SERVE, COMPUTE.BACKEND,
HARDWARE.PROFILE, RESIDENCY, TENSOR.COLLECTION, CONTEXT, KV, GRAPH, RUNTIME,
EVAL, BENCH, and DOCS.

A future cleanup wave may retire or merge redundant planned rows only if it
does not remove completed history and does not hide required work.
```

### 13.2.3 Procedural Implementation Order

Every new implementation wave must choose exactly one primary block.

The default order is:

1. select source/target evidence;
2. inventory source or artifact without loading unnecessary payloads;
3. validate identity and integrity;
4. map model family and tensor collections;
5. decide artifact production or residency plan;
6. prove backend capability for the needed operation;
7. execute the smallest graph boundary;
8. connect graph output to runtime state;
9. expose one short operator command;
10. add failure paths and cleanup;
11. add regression proof;
12. only then promote the spine row.

No wave may skip from artifact inventory to generation. No wave may skip from
external runner evidence to YVEX capability. No wave may skip from graph
primitive to full inference. No wave may skip from storage-stream diagnostics
to disk-backed generation.

Operator preset work follows its own order:

1. configure or resolve operator model roots;
2. resolve target paths from model target records;
3. implement prepare presets only for targets whose source-to-artifact path
   already exists;
4. implement check presets only over behavior already implemented by lower
   commands;
5. implement graph check presets only after the underlying graph commands exist;
6. improve accepted-only chat UX without claiming generation;
7. rewrite the runbook around the new commands;
8. refresh public README/operator examples only after the CLI shape is stable.

No operator preset may claim a capability that the lower-level command path does
not already prove.

### 13.2.4 Conceptual Command Taxonomy

Conceptual operator command families:

```text
source commands:
  source-manifest
  native-weights
  tensor-map
  model-target
  model-class

artifact commands:
  gguf-template
  gguf-emit
  convert
  quant-policy
  quant-job
  imatrix
  inspect
  metadata
  tensors
  integrity
  materialize
  materialize-gate
  model-gate

residency/storage commands:
  storage-plan
  shard-index
  cold-read
  warm-read
  residency-plan
  residency-stage

backend/hardware commands:
  backend
  cuda-info
  backend-profile
  hardware-profile
  qtype-support

graph commands:
  graph
  graph block
  graph layers
  graph attention
  graph moe

runtime commands:
  engine
  session
  input
  prefill
  kv
  decode
  logits
  sample
  generate

operator commands:
  models
  paths
  info
  doctor
  run
  chat

serve commands:
  yvexd status
  yvexd models
  yvexd generate
  yvexd stream

eval/bench commands:
  eval fixture
  eval logits
  eval generation
  bench prefill
  bench decode
  bench generation
  bench memory
```

A command listed here is conceptual unless it already exists in code and tests.
The command taxonomy is not a capability claim.

### 13.2.5 Operator Preset Doctrine

Operator presets are first-class YVEX behavior. The runbook must document
usable commands; it must not hide missing CLI ergonomics behind shell exports,
copy-paste variable walls, or external scripts.

A normal operator path should not require a user to define many shell variables
before understanding the model, artifact, backend, or runtime boundary.

YVEX must make these things command-visible:

- where operator-local model storage is rooted;
- where source tensors are expected;
- where YVEX-produced artifacts are written;
- where reports and local registries live;
- what a model target means;
- whether a model target has source tensors, a YVEX-produced artifact, or only
  future planned artifacts;
- how to prepare a known target when the source and conversion path exist;
- how to check a known target without manually composing every diagnostic
  command;
- which graph/runtime behavior is implemented and which behavior remains
  unsupported.

Shell environment variables may remain as technical overrides, but they are not
the primary operator interface.

The preferred operator shape is:

```text
yvex paths configure
yvex model-target inspect TARGET --paths
yvex models prepare TARGET
yvex models check TARGET
yvex graph check ...
yvex chat ...
```

The final short path is not a generation claim. It remains bounded by the
runtime rows that actually exist.

### 13.2.6 Forward Namespace Rules

Historical implementation rows:

```text
P*, A*, B*, C*, M*, PREFILL.*, DECODE.*, LOGITS.*, SAMPLING.*, GEN.*,
FULLMODEL.*, FAMILY.RUNTIME.*, ATTENTION.CLASS.*, KV.CACHE.*, CONTEXT.CLASS.*
remain for history and completed capability boundaries.
```

Forward implementation rows before v0.1.0:

```text
use V010.*.
```

Forward post-v0.1.0 rows:

```text
use POST010.*.
```

Meta-spine rows:

```text
use SPINE.*.
```

Reference and paper rows:

```text
use V010.PAPER.* or existing SPEC.* where preserved for continuity.
```

New implementation waves should generally map to exactly one V010 row.

Do not create new M-series rows.

Do not create new loose `CLI.UX.*`, `SERVER.*`, `BACKEND.PROFILE.*`,
`MODEL.LIFECYCLE.*`, `LAYOUT.*`, or `DOCS.*` rows unless preserving historical
continuity.

If a future wave must use an older namespace, it must explain why the row is not
better represented by `V010.*`.

`SPINE.*` rows must never be interpreted as runtime capability unless they also
changed code behavior and tests. If they changed code behavior, they probably
do not belong in the meta-spine class.

### 13.2.7 Doctrine Navigation Index

This index keeps the detailed doctrine reachable without making it the primary
roadmap.

| Doctrine area | Primary location | Use |
| --- | --- | --- |
| Implementation order | `### 13.2.3 Procedural Implementation Order` | Choose the next proof boundary. |
| Command taxonomy | `### 13.2.4 Conceptual Command Taxonomy` | Keep command surfaces staged and honest. |
| Operator presets | `### 13.2.5 Operator Preset Doctrine` | Keep normal operator paths short and preset-driven. |
| Forward namespaces | `### 13.2.6 Forward Namespace Rules` | Name future rows without reviving legacy row families. |
| Algorithm and research CLI | `## 13. Doctrine Appendix` | Inspect detailed reference material. |
| Active roadmap | `## 4. Forward Track Matrix` | Pick the next implementation lane. |
| Current next row | `## 7. Active Next` | Confirm the immediate row before coding. |

### YVEX Research CLI Doctrine

YVEX is a tensor-to-token research engine.

The CLI must expose both normal operator paths and research paths.

Normal operator path:
  short, safe, preset-driven, and intended to run implemented behavior.

Research path:
  explicit modes, flags, reports, and traces that reveal tensor roles, tensor
  shapes, dtypes, residency, graph ops, attention behavior, KV layout, prefill
  chunks, decode state, logits buffers, sampling decisions, checksums, cleanup,
  and failure phases.

The research CLI is not a convenience layer on top of generation.

The research CLI is the inspection surface for the runtime itself.

Every runtime algorithm that affects tensors, memory, graph execution, KV state,
decode state, logits, sampling, or generation should eventually have an
operator-visible command, flag, report, trace, or proof.

A conceptual CLI flag is not implemented until all of the following exist:

```text
parser
runtime behavior
output contract
tests
failure paths
cleanup/lifecycle behavior
claim boundary
spine completion row
```

A conceptual command shape is not a command implementation.
A conceptual flag is not an implemented mode.
A report is not runtime execution.
A tensor trace is not model quality.
A research command is not a benchmark.
A CLI mode registry is not mode support.

### Paper-backed Algorithm Doctrine

YVEX may use papers as algorithmic references, but a paper reference does not
imply implemented runtime support.

A paper-backed algorithm must be tracked by implementation stage, not by
citation alone.

Canonical paper-backed implementation ladder:

```text
paper-reference
-> algorithm report
-> fixture proof
-> selected-slice proof
-> runtime mode
-> CLI control surface
-> regression/eval
-> benchmark
```

Stage meanings:

paper-reference:
  the paper or external system is recorded as reference evidence only.

algorithm report:
  YVEX reports the algorithm family, required tensor roles, runtime blockers,
  expected memory behavior, backend requirements, and unsupported boundaries.

fixture proof:
  a small synthetic or controlled fixture proves the mathematical operation,
  layout, cleanup, and failure behavior.

selected-slice proof:
  a small YVEX-produced selected artifact participates in the algorithm path,
  without claiming full model execution.

runtime mode:
  the algorithm is selectable and executed by the runtime over implemented
  state.

CLI control surface:
  the algorithm is visible through a stable command or flag with tests and
  output contract.

regression/eval:
  stable vectors or correctness checks exist over the implemented runtime path.

benchmark:
  a measured result exists over the same runtime path users run, with model
  identity, artifact identity, qtype, context, backend, machine, command, run
  count, and reproducibility metadata.

A paper reference without code is documentation.
A fixture proof is not model support.
A selected-slice proof is not full model execution.
A runtime mode is not a benchmark.
An external runner result is not YVEX runtime execution.
A benchmark row requires the measured YVEX runtime path.

### Algorithm Families

This section records algorithm families and stage boundaries. It does not claim
implementation except for modes already listed in Current Capability and closed
ledger rows.

#### Attention families

full attention / scaled dot-product attention:
  baseline exact attention family. It owns Q/K/V, mask behavior, softmax,
  weighted value accumulation, and output projection requirements.

MHA:
  multi-head attention with separate query, key, and value heads.

MQA:
  multi-query attention where query heads share a smaller set of KV heads.

GQA:
  grouped-query attention where groups of query heads share KV heads.

MLA / latent KV:
  compressed or latent KV representation where model-family-supported.

FlashAttention:
  IO-aware exact attention reference family using tiling to reduce memory
  traffic. It is a reference family until a backend implementation exists.

PagedAttention / paged KV attention:
  KV cache organized as logical blocks mapped to physical memory blocks. It is
  a reference family until paged KV allocation, lookup, eviction, and runtime
  attention consumption exist.

block-sparse attention:
  attention over a restricted block pattern.

local / sliding-window attention:
  attention restricted to local windows or sliding context.

LongCat LoZA / ZigZag sparse attention:
  long-context sparse attention reference family. It is a reference family until
  YVEX implements tensor roles, sparse mask construction, runtime execution, and
  tests.

model-family attention:
  DeepSeek, GLM, and future families may require family-specific attention,
  RoPE, MLA, GQA, MQA, mask, or KV rules.

An attention family becomes supported only when YVEX implements and tests:

```text
tensor role mapping
Q/K/V source tensors
position behavior
mask rules
KV layout
backend operation
reference comparison or deterministic proof
cleanup/failure behavior
CLI command or flag
output contract
claim boundary
```

A standalone attention primitive is not full transformer attention.
A FlashAttention reference is not FlashAttention support.
A PagedAttention reference is not paged KV support.
A LongCat/LoZA reference is not LongCat support.
A model-family attention report is not model-family runtime support.

#### Prefill families

segment-summary prefill:
  implemented selected segment summary path over embedding-plus-RMSNorm.

KV-bound prefill:
  implemented minimal diagnostic KV binding from prefill summary state.

layer-backed prefill:
  implemented bounded path that hands selected segment sample output into the
  controlled layer scheduler.

chunked prefill:
  implemented bounded chunk lifecycle, context-boundary reporting, diagnostic
  scratch reuse, and cleanup/failure reporting.

dense/full transformer prefill:
  planned; requires real model layer weights, attention Q/K/V projection, real
  KV writes, residual path, MLP/MoE path, and full runtime state.

sparse/chunk-select prefill:
  planned; selected chunks or token subsets feed runtime state.

SSD-staged prefill:
  planned; tensors or chunks are staged from SSD into host/backend memory before
  compute.

SSD-streamed prefill:
  planned; tensor pages/chunks are read according to explicit byte-range,
  residency, cache, and failure rules.

prefill-decode disaggregation:
  planned doctrine where prefill state and decode state remain explicit,
  inspectable, separable, and schedulable.

A prefill mode becomes supported only when YVEX implements parser flags,
context rules, token/chunk accounting, graph/runtime behavior, KV behavior where
required, scratch lifecycle, cleanup/failure behavior, tests, and stable CLI
output.

#### KV/cache families

contiguous KV:
  linear KV storage over positions.

paged KV:
  logical KV blocks mapped to physical blocks.

chunked KV:
  KV grouped by chunk ranges or token windows.

host-staged KV:
  KV state staged through host memory.

SSD-staged KV:
  KV state or tensor sources staged from SSD before use.

SSD-streamed KV:
  KV or tensor chunks accessed by explicit storage stream policy.

quantized KV:
  KV stored in reduced precision with explicit qtype policy.

managed-memory KV:
  backend/platform-managed memory with explicit capability and failure reports.

distributed KV:
  future-only KV state distributed across nodes.

KV layout must be command-visible before runtime support is claimed.

A KV report is not KV runtime support.
A KV layout row is not attention support.
A KV allocator is not decode support unless decode consumes it.

#### Decode families

bounded diagnostic decode:
  implemented local runtime closure path over prefill/KV summary state.

baseline autoregressive decode:
  planned one-token-at-a-time decode over runtime KV state.

speculative decode:
  planned draft-token plus target-verification family.

tree speculative decode:
  planned draft-tree and verification family.

block/diffusion speculative decode:
  DFlash-like reference family; not implemented.

DeepSeek-specific speculative decode:
  DSpark/DFlash/HyperDFlash-like reference lane only until YVEX implements its
  own draft, verification, acceptance accounting, and benchmark path.

routing-aware speculative verification:
  planned MoE-oriented verification family where draft length or speculation
  enablement depends on both expected token acceptance and expected target
  verification cost, including router and expert behavior. It is reference-only
  until baseline generation, target verification, accepted-token accounting, KV
  state rules, expert activation accounting, command proof, and benchmark
  comparison exist.

parallel or multi-token prediction decode:
  future family-specific row; not a replacement for baseline decode unless
  verified by target-model semantics.

A decode mode becomes supported only when the runtime state, KV interaction,
position update, cleanup, interruption behavior, CLI command, tests, and output
contract exist.

#### Logits families

bounded diagnostic logits:
  implemented after LOGITS.0.

real output-head logits:
  planned; requires final hidden state and output/vocab projection tensor.

logprob diagnostics:
  planned; computes selected-token log probabilities over implemented logits.

top-k diagnostics:
  planned; reports top-k candidates over implemented logits.

dense vocab projection:
  planned output head matmul over full vocabulary.

sharded vocab projection:
  planned output head split across shards or memory placements.

staged/SSD-backed output head:
  future residency family for very large output projection tensors.

A logits mode becomes supported only when logits buffer ownership, dtype, shape,
projection source, checksum/reporting, CLI command, tests, and cleanup exist.

#### Sampling families

greedy:
  deterministic argmax.

temperature:
  distribution scaling by temperature.

top-k:
  candidate truncation to k highest logits.

top-p / nucleus:
  cumulative probability truncation.

min-p:
  probability floor strategy.

typical:
  entropy-relative typical sampling family.

seeded stochastic:
  reproducible stochastic sampling with explicit seed.

speculative acceptance sampling:
  future strategy coupled to speculative decode acceptance semantics.

A sampling strategy becomes supported only when it consumes implemented logits,
validates parameters, reports seed/strategy/candidate set/selected token, has
tests, and does not claim generation unless integrated into a generation loop.

#### Generation-loop families

bounded diagnostic generation loop:
  current local closure over implemented prefill/decode/logits/sample path,
  diagnostic token append, stop reporting, and cleanup.

baseline autoregressive generation:
  planned repeated decode -> logits -> sample -> append-token loop over real
  runtime state.

speculative generation:
  planned draft -> verify -> accept/reject -> append path.

streaming generation:
  planned token emission surface after runtime generation exists.

provider generation:
  planned server/provider surface after CLI/runtime generation exists.

A generation loop becomes supported only when decode, logits, sampling, token
append, stop conditions, interruption, cleanup, command proof, and tests are
integrated.

#### Residency and storage-stream families

resident:
  tensors/runtime state stay in backend memory.

host-staged:
  tensors/state move through host memory.

SSD-staged:
  tensors/chunks are staged from SSD before execution.

SSD-streamed:
  tensor pages/chunks are read on demand according to a storage-stream plan.

warm-cache:
  repeated reads are measured as cache/residency diagnostics.

managed memory:
  backend/platform-managed memory behavior is reported explicitly.

hybrid:
  different tensor collections use different residency modes.

Storage-stream support is not generation support.
A cold-read probe is not runtime execution.
A residency report is not model support.

### CLI Research Surface Matrix

All command shapes in this matrix are conceptual unless the corresponding code,
tests, command proof, and spine row exist.

attention research:

```text
yvex attention report --model TARGET
yvex attention inspect --model TARGET --layer N
yvex attention run --mode full|flash|paged|sparse|longcat-loza
yvex graph --execute-op --op attention --attention-mode MODE
yvex graph --execute-block --attention-mode MODE
yvex graph --execute-layers --attention-mode MODE
```

Required future output fields:

```text
attention_mode:
attention_family:
paper_reference:
qkv_source:
mask_rule:
position_rule:
kv_layout:
backend:
backend_op_status:
checksum:
max_abs_diff:
support_status:
generation: unsupported
```

prefill research:

```text
yvex prefill --prefill-mode segment-summary|layer-backed|chunked|dense|sparse|ssd-staged
yvex prefill report --tokens IDS --chunk-size N --context-length N
yvex prefill trace --trace-level tensors|kv|chunks|scratch
yvex prefill compare --mode A --mode B
```

Required future output fields:

```text
prefill_mode:
chunk_size:
chunk_count:
context_boundary_status:
kv_binding_source:
scratch_lifecycle:
tokens_processed:
aggregate_checksum:
support_status:
generation: unsupported
```

KV research:

```text
yvex kv report --layout contiguous|paged|chunked
yvex kv inspect --position N --layer N --head N
yvex kv residency --mode resident|host-staged|ssd-staged|ssd-streamed
yvex kv trace --range START:END
```

Required future output fields:

```text
kv_layout:
kv_dtype:
kv_layers:
kv_heads:
kv_head_dim:
kv_capacity:
positions_written:
residency:
cleanup_status:
support_status:
generation: unsupported
```

decode research:

```text
yvex decode --decode-mode diagnostic|autoregressive|speculative|block-diffusion
yvex decode trace --position N --kv-read
yvex decode compare --mode baseline --mode speculative
```

Required future output fields:

```text
decode_mode:
decode_position:
kv_read_status:
decode_state_checksum:
speculative_status:
support_status:
logits_ready:
generation: unsupported
```

logits research:

```text
yvex logits --logits-mode diagnostic|output-head
yvex logits topk --k N
yvex logits logprob --token ID
yvex logits compare --mode diagnostic --mode output-head
```

Required future output fields:

```text
logits_mode:
logits_count:
logits_checksum:
logits_min:
logits_max:
top_k:
logprob_status:
real_model_output_head:
generation: unsupported
```

sampling research:

```text
yvex sample --strategy greedy|temperature|top-k|top-p|min-p|typical
yvex sample --seed N --temperature X --top-k K --top-p P
yvex sample compare --strategy greedy --strategy top-k
```

Required future output fields:

```text
sampling_strategy:
seed:
temperature:
top_k:
top_p:
candidate_count:
selected_token_id:
selected_logit:
sample_checksum:
generation: unsupported
```

generation research:

```text
yvex generate --decode-mode baseline|speculative
yvex generate --prefill-mode MODE --attention-mode MODE --sampling-strategy STRATEGY
yvex generate trace --trace-level tokens|kv|logits|sampling
yvex generate compare --decode-mode baseline --decode-mode speculative
```

Required future output fields:

```text
generation_mode:
prefill_mode:
decode_mode:
attention_mode:
sampling_strategy:
tokens_generated:
stop_reason:
generation_checksum:
full_model_generation:
benchmark_status:
```

paper/reference:

```text
yvex papers list
yvex papers inspect ID
yvex algorithms list
yvex algorithms inspect ID
yvex algorithms report --family attention|prefill|decode|logits|sampling|kv|residency
```

Required future output fields:

```text
paper_id:
algorithm_family:
implementation_stage:
command_surface:
support_status:
runtime_claim:
benchmark_claim:
```

### Algorithm Stage Vocabulary

Allowed stage strings:

reference-only:
  paper or external runtime reference only.

report-only:
  command-visible report exists; no runtime execution.

fixture:
  synthetic or controlled proof exists.

selected-slice:
  selected YVEX-produced artifact participates in a bounded proof.

diagnostic-runtime:
  runtime consumes implemented state but does not claim full model semantics.

full-runtime:
  real model tensor path participates in runtime execution.

eval-ready:
  implemented runtime path has regression/eval vectors.

benchmark-ready:
  benchmark harness and measured runtime path exist.

unsupported:
  explicit refusal or planned-only boundary.

Rows and command outputs should prefer these stage names when reporting
algorithm support.


### 13.16 Preserved Current Repository and Capability State

### 13.16.0 Detailed Current State Archive

The following archived details preserve the earlier repository state and current capability bullets. They remain evidence detail, not the primary navigation surface.

### 13.16.0.1 Current Repository State Archive

```text
root-first C source layout
native root binaries: ./yvex and ./yvexd
single top-level CLI adapter in yvex_cli.c
domain-owned command/help entrypoints in runtime, graph, artifact, model, GGUF, tokenizer, prefill, KV, backend, CUDA, source, eval, and bench owners
no private CLI command forest
canonical runtime/eval/bench root skeleton files with responsibility headers
public headers: include/yvex/
CUDA implementation: cuda/ with C host bridge and CUDA kernel unit
GGUF domain and family mapping: gguf/
multi-family official-source open-weight intake roadmap
source-tensor-first GLM-5.2 huge-MoE pressure target
future YVEX-produced GGUF target path
huge-model storage-stream roadmap
docs: docs/api.md, docs/contract.md, docs/operator-runbook.md, docs/spine.md
public README: prose-first runtime boundary, public-safe artifact wording
artifact docs: operator-local paths only, no personal absolute paths
tests: compact runners, fixtures, vectors, and integrity regression harnesses
generated output: build/
local operator state: .yvex/
```

Documentation roles:

```text
README.md: public technical thesis and project overview
docs/api.md: public C API surface, headers, ownership, error/capability map
docs/contract.md: runtime/CLI/filesystem/backend/server behavior contract
docs/operator-runbook.md: runbook index and routing surface
docs/runbooks/: model-scoped and common operator copy-pack lanes
MODEL_ARTIFACTS.md: external artifact cards and selected pressure facts
AGENTS.md: repository operating rules
docs/spine.md: internal delivery map
```

### 13.16.0.2 Current Capability Archive

Current capability is grouped by evidence stage. A line in this section is not a
generic support claim; read it through the group boundary it belongs to.

#### 4.1 Implemented parser, artifact, and integrity behavior

```text
GGUF inspection and tensor table parsing
tensor/model descriptor construction
GGUF tensor inventory without payload materialization
artifact integrity validator and corruption fixture suite
file identity digest enforcement
registry metadata drift diagnostics
canonical tensor byte-range validation
canonical tensor shape/dtype accounting
materialization integrity gate
graph execution integrity guard
consolidated artifact integrity regression harness
operator integrity report
model gate and materialization gate diagnostics
```

Boundary:

```text
Artifact and integrity behavior does not imply runtime execution, model
support, generation, eval, benchmark, or throughput.
```

#### 4.2 Implemented source/intake and artifact-production behavior

```text
controlled GGUF emission
selected-tensor GGUF emission
DeepSeek selected embedding alias and selected materialization
qtype support separated by policy/storage/emit/quantize/compute
source-tensor-first model-target roadmap authority in spine
command-visible model target class registry
```

Boundary:

```text
YVEX-produced or selected artifacts are artifact capabilities. They do not
prove full model execution unless runtime rows consume them.
```

#### 4.3 Implemented backend, materialization, and residency planning behavior

```text
CPU backend selected materialization
CUDA probe, tensor movement, kernel unit, and parity subset
engine-owned selected materialized weight attachment
session visibility into engine-attached weight state
fullmodel materialization-plan command over the FULLMODEL.0 inventory facts
planned placement phase report without payload materialization
tensor collection materialization grouping
residency mode planning for resident, host-staged, SSD-staged, and hybrid classes
CPU/CUDA backend fit estimate without full backend allocation
preflight blocker report separated from missing tensor roles
cleanup plan for future failed/partial materialization attempts
FULLMODEL.2 readiness and blocker report
fullmodel materialize command over bounded required proof tensors
selected-runtime-slice fullmodel materialization refusal
controlled tiny full-ish GGUF materialization proof on CPU/CUDA when available
phase-specific fullmodel materialization failure reports
fullmodel byte-limit refusal boundary
fullmodel materialization cleanup and owned-state release report
```

Boundary:

```text
Materialization and backend movement are not graph execution, prefill, decode,
logits, sampling, generation, eval, or benchmark.
```

#### 4.4 Implemented graph fixture and selected-slice behavior

```text
graph/planner substrate
deterministic controlled F32 fixture graph execution
real selected F16 token embedding partial graph execution
real selected embedding plus RMSNorm graph segment
standalone RoPE/position graph op boundary
standalone F32 attention primitive boundary
standalone F32 matmul/projection primitive boundary
standalone F32 MLP/feed-forward primitive boundary
controlled first transformer block fixture execution
controlled block executor boundary and scratch lifecycle cleanup
controlled layer scheduler fixture over repeated diagnostic blocks
graph check preset over primitive, block, and layer fixture proofs
```

Boundary:

```text
Graph fixtures and selected-slice proofs do not prove real full transformer
prefill, real QKV projection from full model tensors, real attention-backed
KV, real decode, output-head logits, sampling, or generation.
```

#### 4.5 Implemented diagnostic runtime behavior

```text
tokenizer fixture path and prompt diagnostics
explicit prompt/token input boundary
segment-summary prefill state foundation from validated token sequences
minimal session-owned KV ownership and append/read boundary
minimal KV-backed prefill binding from segment-summary state
bounded layer-backed prefill state over selected segment output and controlled layer fixture scheduling
chunked prefill lifecycle with context-boundary and prefill scratch reuse reporting
bounded diagnostic decode-state step over implemented prefill/KV summary
bounded diagnostic logits buffer over implemented decode state
bounded greedy sampler over implemented diagnostic logits buffer
bounded diagnostic generation loop over implemented prefill/decode/logits/sample path
explicit generated-token append lifecycle
candidate versus accepted/generated token separation
runtime sequence accounting for bounded diagnostic generation
generated-token and accepted-token counts
append-step accounting
decode-position advance after append
context-limit non-mutation behavior
append failure and partial-output reporting
deterministic generation/sequence checksum for diagnostic generated tokens
explicit bounded diagnostic generation stop policy
max-new-tokens post-append stop
context-limit pre-append stop
phase-specific failure stop reasons
stop timing/source reporting
unsupported EOS/stop-token boundary
stop-policy cleanup and partial-output accounting
generation cleanup/failure reporting
generation trace-level parser
token trace for prompt/generated/runtime diagnostic token sequences
step trace for decode/logits/sample/append/stop phases
diagnostic KV trace boundary
bounded logits trace
greedy sampling trace
append trace
stop-policy trace
failure trace
cleanup trace
trace counters and stable trace output
explicit bounded diagnostic generation state lifecycle
deterministic bounded cancellation/interruption path
interrupted stop reason over implemented cancel path
partial-output preservation on cancel/failure
cleanup idempotence and state-release reporting
lifecycle fields in generate output
cancellation trace records
failure/cancel state preservation
```

Boundary:

```text
Diagnostic runtime closes local control flow over implemented diagnostic state.
It is not full model generation, not DeepSeek generation, not provider
generation, not eval, not benchmark, and not model quality evidence.
```

#### 4.6 Implemented report-only command surfaces

```text
fullmodel report command over GGUF tensor directory and metadata inventory
qtype and dtype summary reporting
tensor collection classification for embedding, normalization, attention, MLP, MoE, output, and tokenizer metadata
DeepSeek required-role coverage and missing-role report
CPU/CUDA placement pressure estimate without allocation
full model runtime blocker report
fullmodel no-claim boundary for full execution, full DeepSeek materialization, generation, evaluation, and benchmarks
fullmodel descriptor command
runtime tensor role map
tensor collection runtime descriptor
graph requirement report
prefill/KV/decode/logits/sampling requirement report
output-head/logits requirement report
tokenizer requirement boundary
backend runtime requirement report
selected-slice partial descriptor
tiny full-ish fixture descriptor
source-only descriptor refusal
full runtime blocker report
fullmodel family-runtime command
DeepSeek runtime family adapter report
model-family-specific tensor role mapping from descriptor facts
model-family tensor collection adapter report
DeepSeek attention, position, mask, and KV requirement report
DeepSeek MoE and expert requirement report
DeepSeek output-head and real-logits requirement report
graph primitive versus full-transformer requirement distinction
source-only family-runtime refusal without safetensors inspection
unknown-family family-runtime refusal
next runtime dependency report after family adapter mapping
attention report command
DeepSeek attention class report
Q/K/V/O role requirement report
head layout report
RoPE/position requirement report
mask rule report
attention KV requirement report
context blocker report
graph primitive versus full-transformer attention distinction
selected-slice partial attention report
source-only attention refusal
unknown-family attention refusal
next runtime dependency report after attention class mapping
KV cache class report command
diagnostic-vs-real KV boundary
DeepSeek KV requirement report
KV layout, dtype, indexing, and capacity report
KV residency class report
context dependency report for KV
attention dependency report for KV
selected-slice partial KV report
source-only KV refusal
unknown-family KV refusal
next runtime dependency report after KV class mapping
context class report command
model/requested/active context report
token input count report
chunking policy report
overflow policy report
decode position policy report
bounded diagnostic versus full runtime context boundary
attention/KV context dependency report
selected-slice partial context report
source-only context refusal
unknown-family context refusal
next runtime dependency report after context class mapping
```

Boundary:

```text
Report-only command surfaces improve inspection and planning. They do not
execute runtime behavior unless explicitly paired with implemented runtime rows.
```

#### 4.7 Implemented operator workflow and CLI surfaces

```text
local model registry and alias resolution
server/provider status shell
operator-grade `yvex generate` command surface
stable `yvex generate` help with normal, trace, cancel, and context examples
bounded diagnostic generate command inventory entry
generate argument validation and refusal wording
stable text output policy for bounded diagnostic generation
runbook bounded diagnostic generate lane
plug-and-play operator runbook flow
single-paste operator transcript
full implemented command inventory in operator runbook
sectorized operator command atlas
copy-command operator lanes
operator preset roadmap authority in spine
operator-local model root configuration and path resolution
model target operator path reporting
DeepSeek selected artifact prepare preset
prepare preset dry-run and registration-skip boundaries
selected artifact check preset
model-scoped operator runbooks
```

Boundary:

```text
Operator workflows may compose existing lower-level behavior. They do not
create model/runtime capability beyond the commands they actually execute.
```

#### 4.8 Implemented docs, doctrine, and meta-spine control

```text
routing-aware speculative verification doctrine in spine
canonical runtime sequence rebase in spine
dynamic dense/MoE runtime path doctrine in spine
planned row supersession map in spine
exhaustive v0.1.0 master implementation spine
row-level v0.1.0 delivery contract grammar in spine
whole-repo implementation/spine consistency audit in spine
audit-driven spine architecture reconciliation in spine
source-tensor-first model-target roadmap authority in spine
canonical inference block directory in spine
paper-backed algorithm doctrine and CLI research surface roadmap in spine
generation loop state machine and token lifecycle contract in spine
Qwen Metal pressure lane recorded in spine
DeepSeek V4 Flash generation and speculative throughput target envelope in spine
```

Boundary:

```text
Docs, doctrine, and meta-spine rows are project-control capability, not runtime
capability. They must not be counted as model execution, generation, eval,
benchmark, or throughput.
```

#### 4.9 Implemented refactor, compression, and repository quality work

```text
runtime ownership cleanup and quality compression
in-place core compression and monolith cleanup
in-place graph command and executor compression
in-place runtime engine, session, graph-slice, and command compression
in-place model artifact registry, target, prepare, and check compression
root-first C source layout
native root binaries
source layout refoundation
canonical runtime/eval/bench source skeleton
test vectors and runner consolidation
compact test runners and vectors
public documentation boundary cleanup
minimal documentation surface
model-scoped operator runbooks
code natural hygiene
```

Boundary:

```text
Refactor and compression preserve or clarify existing behavior. They do not
create new runtime capability unless explicitly paired with tests and command
proof for behavior change.
```

#### 4.10 Current live target classes

```text
selected-runtime-slice target:
  family: DeepSeek
  source class: YVEX-produced selected GGUF
  purpose: force parser, identity, selected materialization, backend residency,
           selected graph execution, graph guard, and cleanup boundaries
  aliases:
    deepseek4-v4-flash-selected-embed
    deepseek4-v4-flash-selected-embed-rmsnorm
  execution_ready: false

huge-MoE source-tensor target:
  family: GLM
  model: GLM-5.2
  source class: official safetensors
  public base: zai-org/GLM-5.2
  local source storage class: operator-local model storage
  current operator path class: hf/glm/GLM-5.2
  dry-run source footprint: 282 safetensors, 1.5T-class
  immediate YVEX use: source manifest, safetensors inventory, model-class profile,
                      future tensor mapping, future quantization policy, future
                      YVEX-produced GGUF
  YVEX execution_ready: false
  YVEX generation_ready: false

alias: deepseek4-v4-flash-selected-embed
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU materialization: pass
CUDA materialization: pass
execution_ready: false

alias: deepseek4-v4-flash-selected-embed-rmsnorm
tensors: token_embd.weight, blk.0.attn_norm.weight
token_embd.weight dims: [4096,129280]
blk.0.attn_norm.weight dims: [4096]
dtype: F16
tensor_bytes: 1059069952
CPU segment execution: pass
CUDA segment execution: pass
execution_ready: false

GLM-5.2 target facts:
  base model: zai-org/GLM-5.2
  source artifact: official safetensors
  shard count target: 282 safetensors
  source footprint class: 1.5T
  architecture class: huge MoE
  immediate YVEX use: official source-tensor OWI target only
  external GGUF evidence: reference only
  external runner evidence: reference only
  YVEX-produced GGUF: planned
  YVEX inference: not implemented
  YVEX generation: not implemented

Qwen-Metal planned target facts:
  family: Qwen
  source class: official source tensors
  pressure class: planned metal-reduced-full-runtime-pressure
  hardware lane: Apple Silicon / Metal
  immediate YVEX use: spine-only pressure lane
  external GGUF evidence: reference only
  external runner evidence: reference only
  YVEX-produced GGUF: planned
  YVEX materialization: not implemented
  YVEX Metal backend: not implemented
  YVEX Qwen runtime: not implemented
  YVEX generation: not implemented
```

#### 4.11 Unsupported / Not Advanced Boundary Registry

```text
full model execution
full DeepSeek materialization
full GGUF conversion
GLM source tensor inventory completion
GLM tensor mapping
GLM quantization policy
GLM YVEX-produced GGUF
GLM runtime execution
GLM full materialization
GLM storage-stream execution
Apple Silicon hardware profile implementation
Metal backend implementation
Metal allocation/movement implementation
Metal graph primitive execution
Qwen official source tensor intake
Qwen tensor mapping
Qwen YVEX-produced GGUF
Qwen artifact identity
Qwen materialization
Qwen runtime execution
Qwen real generation
Qwen Metal benchmark
Qwen external GGUF capability attribution
Qwen external runner capability attribution
SSD-backed tensor paging
disk-backed generation
huge-MoE generation
external-GGUF capability attribution
external-runner capability attribution
full supply-chain security
diagnostic REPL layout hardening
final operator runbook over preset commands
DeepSeek V4 Flash full generation path
DeepSeek V4 Flash baseline decode benchmark
DeepSeek V4 Flash end-to-end generation benchmark
speculative decoding
DSpark-like draft/verify runtime
accepted-token speculative accounting
speculative generation benchmark
20 tok/s measured YVEX decode result
full transformer attention
real QKV projection from model tensors
real attention-backed KV writes
real KV reads by decode
paged KV implementation
chunked KV runtime implementation
SSD-backed KV
quantized KV
CUDA full KV allocation proof
full transformer prefill
long-context runtime support
context extension support
full model decode
real DeepSeek decode
real output-head logits-producing runtime path
real vocabulary sampling
stochastic sampling
full model generation
real DeepSeek generation
tokenizer-quality generation
EOS-backed real tokenizer stop
stop-token text matching
tensor-level full-model trace
raw tensor dumps
OS signal cancellation
server/provider cancellation
streaming cancellation
external interruption source integration
interactive generation
provider generation endpoint
streaming generation
OpenAI-compatible generation
Anthropic-compatible generation
evaluation suite
capability eval
inference benchmarks
benchmark throughput
benchmark performance
advanced Runtime KV capacity
execution_ready true-state claim
generation_ready true-state claim
v0.1.0 release
```

M8 is not the final prefill path. It is a segment-summary prefill-state
foundation built from validated token input and implemented selected graph
segments. PREFILL.1 binds that summary to minimal session-owned KV state with
diagnostic values. PREFILL.2 adds a bounded layer-backed state path by handing a
selected segment output sample into the controlled layer fixture scheduler.
Full transformer prefill still requires real model layer weights, attention K/V
projection, and the future chunked prefill lifecycle.



### 13.16 Preserved Runtime, Generation, Tensor, and Operator Doctrine

### 13.16.1 DeepSeek Generation and Throughput Target

DeepSeek V4 Flash is the internal generation pressure target.

Current state:

```text
YVEX full model generation: unsupported
YVEX real generation throughput: 0 tok/s
reason: real DeepSeek decode, real output-head logits, real vocabulary sampling,
        and full model generation loop are not implemented
```

Internal target:

```text
baseline model: DeepSeek V4 Flash
machine class: GB10-class CUDA
baseline threshold to exceed: 15 tok/s decode
YVEX target: >=20 tok/s baseline decode
public claim: none
benchmark status: not measured
```

The target is valid only after the real generation path exists.

A valid DeepSeek decode benchmark must record:

```text
model identity
source artifact identity
YVEX-produced artifact identity
qtype
context length
prompt length
generated token count
backend
machine
command
run count
reproducibility note
```

Prefill throughput, decode throughput, and end-to-end generation throughput are
separate measurements.

Selected artifact execution is not a generation benchmark.

External runner output is not a YVEX benchmark.

DSpark or other speculative decoding systems are reference evidence until YVEX
implements its own draft, verification, acceptance, and benchmark path.

### 13.16.2 Speculative Decoding Reference Track

Speculative decoding is a future acceleration track.

The baseline YVEX path must exist first:

```text
real transformer prefill
KV
decode
logits
sampling
generation loop
```

Only after that baseline exists may YVEX add speculative acceleration.

A future speculative path must define:

```text
draft source
draft token count
target verification
accepted-prefix accounting
rejected-token behavior
KV interaction
logits ownership
sampling interaction
interruption and cleanup
speedup measurement against YVEX baseline
```

DSpark is a reference design for this track because it combines
semi-autoregressive drafting with confidence-scheduled verification and
hardware-aware scheduling.

DSpark-like confidence scheduling is reference evidence only. For dense
targets, confidence may help choose the number of drafted tokens. For MoE
targets, confidence is insufficient by itself because target verification cost
also depends on router fanout, expert residency, expert dispatch, and KV/state
accounting. YVEX speculative work must distinguish dense verification cost from
MoE routing-aware verification cost before claiming acceleration.

DSpark evidence remains external evidence. It does not imply YVEX implements
speculative decoding and does not imply YVEX reaches any throughput target.

### 13.16.4 Execution and Residency Modes

YVEX must plan for multiple execution and residency modes:

```text
resident:
  required tensors and runtime state fit in the selected backend memory.

host-staged:
  tensors are staged from system memory into backend memory.

ssd-staged:
  tensors are staged from operator-local SSD into system or backend memory.

ssd-streamed:
  tensor pages or chunks are read according to an explicit storage-stream plan.

managed-memory:
  backend/platform-managed memory is used with explicit capability and failure
  reporting.

distributed:
  tensors or runtime phases are distributed across future nodes. This is planned
  only and not supported.

hybrid:
  some tensors or states are resident, some host-staged, some SSD-staged, or
  future distributed.
```

A residency mode is not a generation claim. It only becomes runtime capability
when the corresponding graph/runtime path consumes that residency mode with
tests, command proof, failure paths, and cleanup.

### 13.16.5 Runtime Closure Doctrine

There are two different closures:

```text
Bounded runtime closure:
  implemented prefill/KV state
  -> decode state step
  -> bounded logits
  -> sampler
  -> generation loop
```

This can close the local runtime loop without claiming real DeepSeek full
generation.

```text
Full model generation closure:
  full tensor inventory
  -> real layer weights
  -> attention/KV projection
  -> real logits/output head
  -> sampling
  -> generation
  -> benchmark
```

This is required before claiming DeepSeek generation, inference readiness, or
throughput.

A bounded generation loop may exist before full DeepSeek generation. It must
report its diagnostic/model-boundary status honestly. It cannot close DeepSeek
full generation, benchmark, provider generation, or public throughput rows.

### 13.16.3 Generation Loop Contract

A generation loop is the repeated composition of:

```text
prefill state
-> decode state step
-> logits buffer
-> sampler decision
-> token append
-> stop-condition check
-> cleanup/failure reporting
```

bounded diagnostic generation loop:
  local runtime loop over implemented diagnostic prefill/decode/logits/sample
  boundaries. It proves runtime control flow, state ownership, append semantics,
  stop reasons, failure behavior, and cleanup. It does not prove full DeepSeek
  generation.

full model generation loop:
  runtime loop over real model prefill, real decode, real output-head logits,
  real vocabulary sampling, real tokenizer append/detokenization, and measured
  runtime behavior.

The first `yvex generate` command may close bounded runtime control flow before
full DeepSeek generation exists, but it must print its diagnostic/full-model
boundary honestly.

### Generation State Machine

created:
  generation options parsed and state allocated.

prefill:
  implemented prefill path invoked.

decode:
  one decode-state step invoked for current position.

logits:
  bounded or real logits buffer produced.

sample:
  sampler selects one candidate token.

append:
  selected token is appended to runtime token sequence.

stop-check:
  max-new-tokens, context boundary, EOS/stop token, interruption, or failure is
  checked.

complete:
  loop ended successfully by a stop reason.

failed:
  loop failed and cleanup state is reported.

cancelled:
  future interruption path stops loop and reports partial output.

cleaned:
  allocated runtime loop state was released.

A generation state transition must be command-visible through status fields,
trace fields, or failure reports.

### Generation Token Lifecycle

prompt tokens:
  input tokens accepted before generation loop begins.

prefill tokens:
  tokens consumed by prefill to create runtime state.

decode position:
  next position after prefill or after the previous generated token.

candidate token:
  token selected by sampler from logits.

accepted token:
  candidate token appended to runtime sequence.

generated token:
  accepted token counted as new output token.

detokenized text:
  optional text rendering of generated tokens through tokenizer where supported.

Bounded diagnostic generation may set selected_token_id from bounded sampler
output. That token ID is diagnostic unless it comes from a real model
output-head and real vocabulary sampling path.

A token append is not full model generation by itself.

A generated token count is not a benchmark by itself.

### Generation Stop Reasons

Allowed stop reason vocabulary:

```text
max-new-tokens
context-limit
eos-token
stop-token
sampler-failure
decode-failure
logits-failure
append-failure
interrupted
unsupported
internal-error
```

For the current bounded diagnostic generation loop, implemented stop reasons:

```text
max-new-tokens
context-limit
decode-failure
logits-failure
sampler-failure
append-failure
internal-error
```

Current stop timing:

```text
max-new-tokens:
  post-append stop after the requested diagnostic token count is committed.

context-limit:
  pre-append stop before runtime sequence mutation would exceed active context.

decode/logits/sampler/append/internal failure:
  failure stop with phase-specific failure reporting and cleanup.
```

EOS and stop-token policy are currently reported as unsupported. They must not
be treated as real tokenizer-backed stop behavior until tokenizer special-token
and stop-token matching support exists.

A stop reason must be printed in every generation result.

### Generation CLI Output Contract

Required output fields for `GEN.LOOP.0`:

```text
generate: loop
status:
model:
backend:
segment:
state_id:
lifecycle_status:
generation_state:
state_dirty:
active_step:
last_completed_step:
cancel_supported:
cancel_requested:
cancel_reason:
cancel_step:
cancel_timing:
cancel_safe_point:
partial_output_available:
token_input_status:
prompt_token_count:
prefill_token_count:
max_new_tokens:
context_length:
generated_token_count:
accepted_token_count:
partial_generated_token_count:
total_token_count:
position_start:
prefill_position_end:
current_decode_position:
last_successful_position:
generation_loop_kind:
generation_mode:
decode_mode:
logits_mode:
sampling_strategy:
bounded_generation:
full_model_generation:
real_deepseek_generation:
prefill_invoked:
decode_steps:
logits_steps:
sample_steps:
append_steps:
candidate_token_id:
candidate_logit:
last_selected_token_id:
last_selected_logit:
last_appended_token_id:
append_status:
append_failure:
stop_policy:
stop_requested:
stop_reason:
stop_phase:
stop_step:
stop_timing:
stop_after_append:
stop_before_append:
failure_stop:
unsupported_stop_feature:
eos_policy:
stop_token_policy:
trace_level:
trace_enabled:
trace_records:
trace_tokens:
trace_steps:
trace_kv:
trace_logits:
trace_sampling:
trace_append:
trace_stop:
trace_cancel:
trace_cleanup:
trace_failures:
trace_status:
generation_checksum:
sequence_checksum:
generated_token_ids:
runtime_token_sequence:
cleanup_attempted:
cleanup_status:
cleanup_idempotent:
cleanup_repeated:
cleanup_owned_state_released:
failure_preserved:
partial_output_preserved:
generation_ready:
generation:
benchmark_status:
```

Required boundary values for first bounded loop:

```text
generation_loop_kind: bounded-diagnostic
generation_mode: diagnostic-runtime
decode_mode: bounded-diagnostic
logits_mode: bounded-diagnostic
sampling_strategy: greedy
bounded_generation: true
full_model_generation: false
real_deepseek_generation: false
generation_ready: false
generation: unsupported-full-model
benchmark_status: not-measured
```

`bounded_generation: true` is allowed only if a loop exists and runs.

Bounded cancellation is local and deterministic:

```text
--cancel-after-steps 0:
  requests interruption before the first decode step after preflight.

--cancel-after-steps N:
  for N > 0, requests interruption after N generated diagnostic tokens have
  been appended.

Cancelled output:
  status: generation-loop-cancelled
  generation_state: cancelled
  stop_reason: interrupted
  stop_phase: stop-check
  stop_timing: cancel-safe-point
  failure_stop: false
```

Cancellation is checked only at loop safe points. It must not interrupt a
token append in the middle of mutation. Partial generated-token output and
failure/cancel state must remain visible after cleanup. This does not provide
OS signal cancellation, server/provider cancellation, streaming cancellation,
or full model generation.

It must not imply:

```text
full_model_generation true-state
generation_ready true-state
real_deepseek_generation true-state
```

Those remain false.

### Generation Trace Contract

Implemented trace levels:

none:
  final summary only; no `trace.*` records.

tokens:
  prompt, generated, runtime diagnostic token IDs, counts, and stop reason.

steps:
  per-attempted-token decode/logits/sample/append/stop step summary.

kv:
  diagnostic KV boundary and requested shape only; no real attention KV claim.

logits:
  bounded diagnostic logits checksum/min/max only; no real output-head claim.

sampling:
  greedy candidate token/logit/checksum only; no stochastic or real vocab claim.

full:
  token, step, KV, logits, sampling, append, stop, cancel, failure, and cleanup
  records.

Every trace line uses this stable text shape:

```text
trace.<category>.<index>.<field>: value
```

Index is omitted for singleton categories such as `tokens`, `kv`, `stop`,
`cancel`, `failure`, and `cleanup`.

Current trace output does not dump raw tensors, expose full-model tensor traces,
claim real DeepSeek generation, claim provider generation, evaluate capability,
or measure benchmark throughput.

### Generation Failure and Cleanup Contract

Required failure phases:

```text
preflight
prefill
decode
logits
sample
append
stop-check
cleanup
```

Failure output must include:

```text
failed_phase:
failed_step:
partial_generated_token_count:
last_successful_position:
cleanup_attempted:
cleanup_status:
stop_reason:
generation: unsupported-full-model
benchmark_status: not-measured
```

A failed generation loop may produce partial diagnostic tokens, but partial
diagnostic output is not model quality and not benchmark evidence.

### 13.16.6 Tensor Collections

YVEX must make tensor collections explicit before full runtime claims.

Canonical tensor collections:

```text
embedding:
  token embedding and related input projection tensors.

normalization:
  attention norm, post-attention norm, final norm, and model-family-specific
  normalization tensors.

attention:
  Q, K, V, O projection tensors, attention scale rules, RoPE/position rules,
  attention type, head layout, and mask requirements.

KV cache:
  runtime-owned K/V state, layer/head/position indexing, dtype/qtype,
  residency, capacity, page/chunk policy, and cleanup.

MLP:
  gate, up, down, activation, intermediate, and output tensors for dense
  feed-forward paths.

MoE:
  router tensors, expert tensors, active expert count, shared expert tensors,
  top-k routing facts, expert activation, expert residency, and expert dispatch.

output:
  final normalization, output head, logits projection, and vocabulary-facing
  tensors.

tokenizer/runtime input:
  tokenizer metadata, prompt format, explicit token input, and special-token
  behavior.
```

Tensor collection support is not model support. A collection becomes supported
only when its parser, mapping, residency, graph/runtime consumer, tests, command
proof, failure paths, and cleanup exist.

### 13.16.7 Attention, KV, and Context Rules

Attention support must identify:

```text
attention type:
  MHA, MQA, GQA, MLA, or model-family-specific variants.

position behavior:
  RoPE, alternative position encoding, scaling behavior, context extension, and
  model-family-specific constraints.

KV ownership:
  whether K/V values are produced by prefill, advanced by decode, stored by
  layer/head/position, staged, paged, quantized, or resident.

context class:
  requested context, model maximum context, active context, chunk size, prefill
  chunking, decode position, and context overflow behavior.
```

No decode row may be promoted before KV-backed transformer prefill and relevant
graph/layer execution exist. No logits row may be promoted before an actual
runtime path produces a logits buffer. No sampling row may be promoted before
logits exist. No generation row may be promoted before decode, logits, and
sampling are integrated.

### 13.16.8 MoE and Expert Activation Rules

MoE support must identify:

```text
router:
  router tensor roles, router logits, routing dtype, top-k rule, and failure
  behavior.

experts:
  expert tensor collections, expert count, active expert count, shared expert
  facts, qtype classes, residency, and dispatch requirements.

activation:
  selected experts per token, expert weights, expert output accumulation, expert
  load boundaries, and cleanup.

storage pressure:
  expert tensors may dominate source/artifact footprint. Storage-stream work may
  profile expert layout before runtime MoE execution exists.
```

A routed-expert primitive is not full MoE support. Full MoE support requires
router execution, expert selection, expert residency, expert dispatch,
accumulation, graph integration, tests, and command-visible proof.

### 13.16.9 Build, Backend, and Hardware Profile Rules

Build profiles describe what the binary is compiled to use.

Backend profiles describe what compute paths are implemented.

Hardware profiles describe what the current machine can actually run.

Canonical profile classes:

```text
build profile:
  CPU-only, CUDA-enabled, future Metal-enabled, future ROCm-enabled, debug,
  release, sanitizer, and benchmark builds.

backend profile:
  CPU reference, CUDA implementation, future Metal feasibility, future ROCm
  feasibility, and backend op capability matrix.

hardware profile:
  Spark/GB10, local workstation, large-memory future hardware, and future
  distributed nodes.
```

A hardware profile is not a backend implementation. A backend implementation is
not a model capability. A build flag is not runtime support.


### 13.17 Preserved Non-Negotiable Rules

### 13.17 Non-Negotiable Rules

- No support claim without code, tests, and command proof.
- A paper reference is not implementation.
- An algorithm row is not support.
- A CLI flag listed conceptually is not implemented.
- A research report is not runtime execution.
- A tensor trace is not model quality.
- A mode registry is not mode support.
- A FlashAttention reference is not a FlashAttention backend.
- A PagedAttention reference is not paged KV implementation.
- A LongCat/LoZA reference is not LongCat support.
- A DFlash/DSpark/HyperDFlash reference is not speculative decoding support.
- A top-k command shape is not top-k sampling support.
- A stochastic sampling row is not implemented sampling.
- A generation mode report is not generation.
- A generation contract is not generation implementation.
- A generation trace contract is not trace implementation.
- A token lifecycle definition is not token append implementation.
- A stop-reason vocabulary is not stop policy implementation.
- A bounded generation loop is not full model generation.
- A generated diagnostic token is not model quality.
- A partial diagnostic output is not a benchmark.
- A `yvex generate` command may not claim full generation unless real decode,
  real output-head logits, real vocabulary sampling, token append, stop
  conditions, cleanup, and command proof exist.
- No generation claim without decode, logits, sampling, and generation loop.
- No benchmark claim without measured runtime path and reproducibility metadata.
- No external paper or runner result may close a YVEX implementation row.
- Every new implementation wave must name one primary canonical block.
- The runbook must not compensate for missing CLI ergonomics with shell export
  walls, helper scripts, or path derivation logic.
- Normal operator flows should prefer `paths configure`, `model-target --paths`,
  `models prepare`, and `models check` once those commands exist.
- Shell environment variables may be supported as technical overrides, but they
  must not be the primary documented interface for normal operator paths.
- A prepare preset may only compose lower-level behavior that already exists.
- A check preset may only close the boundary it actually exercises.
- A graph check preset may not claim graph capability beyond the underlying
  graph commands it runs.
- A chat UX improvement may not claim generation.
- Public operator examples must wait until the preset CLI shape is stable.
- No command proof may close a row outside the boundary it actually exercises.
- No external runner result may close a YVEX runtime row.
- No external GGUF may close a YVEX-produced artifact row.
- No backend probe may close a model capability row.
- No hardware profile may close a backend implementation row.
- No storage read probe may close a generation row.
- No routed expert slice may close full MoE support.
- No tensor collection report may close runtime execution.
- No build profile may imply runtime support.
- New planned rows should use canonical nouns unless preserving historical
  continuity.
- Official source tensors are the primary OWI input.
- YVEX-produced GGUF is the primary OWI output artifact.
- External GGUFs may be recorded only as reference evidence.
- No external GGUF may satisfy a YVEX conversion, quantization, GGUF emission,
  runtime, eval, or benchmark milestone.
- No external runner result may be presented as YVEX runtime execution.
- No GLM runtime claim before YVEX maps the GLM family, required tensor roles,
  qtype profile, graph requirements, KV requirements, decode requirements, and
  logits path.
- No huge-model benchmark claim before YVEX implements the measured
  storage/runtime path and records artifact identity, qtype, context, backend,
  machine, command, and reproducibility metadata.
- No SSD-streaming claim before shard index, tensor byte-range mapping,
  cold/warm read probes, cache policy, and failure/cleanup reports exist.
- No generated GLM artifacts in git.
- No downloaded model source or GGUF shard in git.
- GLM source-tensor work belongs to open-weight intake, model class, and
  storage-stream tracks until the runtime path can consume it.
- No generated model artifacts in git.
- No personal absolute artifact paths in public docs.
- No internal delivery IDs outside `docs/spine.md`.
- No inference or generation claim until implemented.
- No benchmark claim without benchmark implementation and proof.
- DeepSeek V4 Flash >=20 tok/s decode is an internal target only until measured
  by the benchmark harness over an implemented YVEX generation path.
- No public throughput claim before model identity, artifact identity, qtype,
  context, backend, machine, command, run count, and reproducibility metadata
  exist.
- DSpark and other speculative decoding systems are external reference evidence
  only until YVEX implements its own speculative path.
- Speculative decoding may not be promoted before baseline generation exists.
- No speculative decoding claim before target-model verification semantics
  exist.
- No MoE speculative claim before router, expert activation, expert residency,
  KV/state accounting, and accepted-token accounting exist.
- No confidence-only speculative scheduler may claim MoE acceleration without
  verification-cost accounting.
- A speculative path must measure accepted tokens, rejected tokens,
  verification cost, latency, throughput, and speedup against YVEX baseline.
- No speculative benchmark may be reported without comparison against YVEX
  baseline generation on the same model/artifact/backend/context/machine.
- External DSpark serving numbers cannot close a YVEX benchmark row.
- No status promotion without command proof from the validation/audit gate.
- No M-series completion status until the relevant runtime state exists in code
  and tests.
- No scaffold milestone completion.
- No commit subject should use the internal delivery ID as the primary story.
- No long-flag operator flow may remain the only normal path once concise
  presets exist.
- Do not collapse materialization, engine ownership, graph execution, prompt
  input, prefill, KV, decode, logits, sampling, generation, CLI generation, and
  server generation into one wave.
- No decode milestone may be promoted before KV-backed prefill and relevant
  graph/layer execution exist.
- No logits milestone may be promoted before an actual runtime path produces a
  logits buffer.
- No sampling milestone may be promoted before logits exist.
- No generation milestone may be promoted before decode, logits, and sampling
  are integrated.
- A bounded generation loop may close local runtime control flow but may not
  close full model generation.
- Diagnostic logits may close a logits-buffer boundary but may not claim real
  model output-head logits.
- A sampler may close token selection over implemented logits but may not claim
  generation quality.
- A CLI generation command may expose the implemented bounded generation loop
  but must preserve full-model and benchmark disclaimers.
- DeepSeek full generation remains separate from bounded diagnostic generation
  until full model tensor, layer, attention, KV, output-head, and benchmark
  requirements are met.
- No CLI or server generation surface before the lower runtime generation loop
  exists.
- No provider generation milestone may be promoted before CLI/runtime generation
  exists.
- No provider compatibility claim before server generation exists and is tested.
- No benchmark milestone may be promoted before the measured runtime path exists.
- No benchmark number without model artifact, backend, quant, context, machine,
  command, and reproducibility note.
- No model-family support claim without artifact, mapping, runtime path, and
  tests.
- No graph execution path may trust tensor offset, byte range, shape, dtype, or
  element count without validation.
- No artifact identity claim without digest or equivalent gate evidence.
- No registry alias should be treated as stable identity if the underlying file
  changed.
- No corruption fixture may be committed if it is a real model artifact; corrupt
  fixtures must be tiny.
- No integrity report may imply supply-chain security beyond the checks actually
  implemented.
- No wildcard delivery rows in the implementation ledger.
- No secondary roadmap tables competing with the track model.
- Full transformer execution requires graph op expansion, transformer block
  execution, layer scheduling, KV-backed prefill, decode, logits, sampling, and
  generation.
- Do not claim advanced Runtime KV capacity work until the relevant allocator,
  estimator, spill, or quantization behavior exists in code and tests.
- No docs sprawl beyond `docs/api.md`, `docs/contract.md`,
  `docs/operator-runbook.md`, `docs/runbooks/`, and `docs/spine.md`.
- Keep DeepSeek as the selected-runtime-slice pressure target and GLM-5.2
  official safetensors as the huge-model source-tensor pressure target unless
  this spine changes.
- Keep Qwen as historical validation evidence unless this spine changes.
