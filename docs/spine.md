# YVEX Inner Delivery Spine

Date: 2026-06-28
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 1. Authority

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

## 1.1 Engine Identity

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

## 2. Implementation Doctrine

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

## 2.1 Canonical Block Directory

YVEX implementation blocks:

```text
BLOCK 0 — Source and target evidence
  Owns official model source references, local source storage, source manifests,
  model target classes, and source artifact classes.
  Current rows: OWI.REBASE.0, OWI.TARGETS.*, OWI.HUGE.*
  Normal proof: source manifest, native tensor inventory, shard count, source
  footprint, identity/drift report.

BLOCK 1 — Artifact production
  Owns tensor mapping, quantization policy, calibration/imatrix evidence,
  conversion plans, YVEX-produced GGUF emission, split artifact strategy, and
  artifact naming.
  Current rows: OWI.3-OWI.9, ARTIFACT.NAMING.0, OWI.HUGE.4.
  Normal proof: conversion plan, template validation, qtype policy, emitted
  controlled or selected GGUF, parse roundtrip.

BLOCK 2 — Artifact identity and integrity
  Owns file identity, digest, tensor byte ranges, shape, dtype, element count,
  corruption fixtures, materialization gate, graph integrity guard, and operator
  integrity reports.
  Current rows: ARTIFACT.INTEGRITY.*.
  Normal proof: integrity report, corruption refusal, digest check, tensor range
  validation, cleanup/failure phase.

BLOCK 3 — Model class and tensor collections
  Owns architecture family, model class, MoE facts, tensor role collections,
  attention type, layer shape, context class, qtype class, runtime requirements,
  and unsupported blockers.
  Current rows: MODEL.CLASS.*, FAMILY.RUNTIME.0, FULLMODEL.*.
  Normal proof: model-class report, tensor collection inventory, runtime
  requirement report, unsupported blocker list.

BLOCK 4 — Residency and storage
  Owns CPU/GPU/system-memory/SSD/distributed residency classes, storage-stream
  planning, shard index, tensor page/chunk access, cold/warm read probes, cache
  policy, staging, eviction, and cleanup.
  Current rows: STORAGE.STREAM.*, RUNTIME.KV.*, future RESIDENCY.*.
  Normal proof: shard index, byte-range map, cold/warm read diagnostics,
  staged-residency report, cleanup/failure report.

BLOCK 5 — Compute backend and hardware profiles
  Owns CPU, CUDA, future Metal, future ROCm, build profiles, hardware profiles,
  backend op capability matrix, allocation/transfer failure behavior, and
  machine-specific pressure reports.
  Current rows: L0, CUDA.SURFACE.0, BACKEND.PROFILE.*, BACKEND.METAL.0,
  BACKEND.ROCM.0.
  Normal proof: backend probe, op capability report, allocation/transfer test,
  parity test, hardware profile report.

BLOCK 6 — Graph execution
  Owns primitive graph ops, normalization, attention, projection, MLP, routed
  expert slices, transformer block execution, residual/state ownership, layer
  scheduler, reference comparison, scratch lifecycle, and graph failure paths.
  Current rows: GRAPH.OPS.*, GRAPH.BLOCK.0, GRAPH.LAYERS.0.
  Normal proof: command-visible graph execution, reference comparison, checksum,
  max-diff, cleanup/failure report.

BLOCK 7 — Runtime state
  Owns engine/session ownership, token input, context planning, prefill, KV
  cache, decode, logits, sampling, generation state, interruption, traces, and
  profiles.
  Current rows: M7-M17, PREFILL.*, KV.*, DECODE.*, LOGITS.*, SAMPLING.*,
  GEN.LOOP.*, RUNTIME.KV.*.
  Normal proof: runtime command over implemented state, lifecycle test, context
  boundary test, cleanup/interruption report.
  The DeepSeek V4 Flash full-generation target must pass through real
  transformer prefill, KV, decode, logits, sampling, and generation before any
  CLI or server generation claim exists.

BLOCK 8 — Operator and serving surfaces
  Owns CLI presets, command taxonomy, doctor flow, REPL, daemon state,
  provider endpoints, streaming responses, structured output, and server
  observability.
  Current rows: CLI.UX.*, MODEL.LIFECYCLE.*, SERVER.*.
  Normal proof: short operator command, JSON/text output, daemon endpoint,
  refusal path, structured diagnostics.
  Operator presets must reduce shell glue by moving common path resolution,
  model preparation, model checks, graph suites, and diagnostic REPL clarity
  into YVEX commands.

BLOCK 9 — Evaluation, benchmarks, and public evidence
  Owns fixture eval, partial regression, prefill regression, decode/logits/
  sampling/generation eval, capability eval, runtime benchmarks, memory
  pressure, server latency, documentation diagrams, and public target tables.
  Current rows: EVAL.*, BENCH.*, DOCS.*.
  Normal proof: same runtime path users run, model/artifact identity, backend,
  qtype, context, machine, command, reproducibility note.
  Internal throughput targets such as DeepSeek V4 Flash >=20 tok/s decode are
  engineering targets only until measured by the benchmark harness over the same
  runtime path users run.
```

## 2.2 Naming and Ownership Rules

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

## 2.3 Procedural Implementation Order

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

## 2.4 Conceptual Command Taxonomy

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

## 2.5 Operator Preset Doctrine

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

## YVEX Research CLI Doctrine

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

## Paper-backed Algorithm Doctrine

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

## Algorithm Families

This section records algorithm families and stage boundaries. It does not claim
implementation except for modes already listed in Current Capability and closed
ledger rows.

### Attention families

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

### Prefill families

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

### KV/cache families

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

### Decode families

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

parallel or multi-token prediction decode:
  future family-specific row; not a replacement for baseline decode unless
  verified by target-model semantics.

A decode mode becomes supported only when the runtime state, KV interaction,
position update, cleanup, interruption behavior, CLI command, tests, and output
contract exist.

### Logits families

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

### Sampling families

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

### Generation-loop families

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

### Residency and storage-stream families

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

## CLI Research Surface Matrix

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

## Algorithm Stage Vocabulary

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

## 3. Current Repository State

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

## 4. Current Capability

Implemented and audited:

```text
GGUF inspection and tensor table parsing
tensor/model descriptor construction
tokenizer fixture path and prompt diagnostics
graph/planner substrate
CPU backend selected materialization
CUDA probe, tensor movement, kernel unit, and parity subset
controlled GGUF emission
selected-tensor GGUF emission
DeepSeek selected embedding alias and selected materialization
model gate and materialization gate diagnostics
local model registry and alias resolution
qtype support separated by policy/storage/emit/quantize/compute
server/provider status shell
engine-owned selected materialized weight attachment
session visibility into engine-attached weight state
deterministic controlled F32 fixture graph execution
real selected F16 token embedding partial graph execution
real selected embedding plus RMSNorm graph segment
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
standalone RoPE/position graph op boundary
standalone F32 attention primitive boundary
standalone F32 matmul/projection primitive boundary
standalone F32 MLP/feed-forward primitive boundary
controlled first transformer block fixture execution
controlled block executor boundary and scratch lifecycle cleanup
controlled layer scheduler fixture over repeated diagnostic blocks
graph check preset over primitive, block, and layer fixture proofs
artifact integrity validator and corruption fixture suite
file identity digest enforcement
registry metadata drift diagnostics
canonical tensor byte-range validation
canonical tensor shape/dtype accounting
materialization integrity gate
graph execution integrity guard
consolidated artifact integrity regression harness
operator integrity report
source-tensor-first model-target roadmap authority in spine
command-visible model target class registry
canonical inference block directory in spine
paper-backed algorithm doctrine and CLI research surface roadmap in spine
generation loop state machine and token lifecycle contract in spine
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
runtime ownership cleanup and quality compression
in-place core compression and monolith cleanup
in-place graph command and executor compression
in-place runtime engine, session, graph-slice, and command compression
in-place model artifact registry, target, prepare, and check compression
DeepSeek V4 Flash generation and speculative throughput target envelope in spine
```

Current live target classes:

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
```

Unsupported / not advanced:

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
full transformer prefill
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
```

M8 is not the final prefill path. It is a segment-summary prefill-state
foundation built from validated token input and implemented selected graph
segments. PREFILL.1 binds that summary to minimal session-owned KV state with
diagnostic values. PREFILL.2 adds a bounded layer-backed state path by handing a
selected segment output sample into the controlled layer fixture scheduler.
Full transformer prefill still requires real model layer weights, attention K/V
projection, and the future chunked prefill lifecycle.

## 5. Unified Delivery Ledger

The table is the spine. Every delivery row is recorded here; sections after the
table explain dependency logic, validation, and rules without duplicating status
tables.

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
| ATTENTION.CLASS.0 | planned | attention | Attention class report | attention type, head layout, position behavior, mask rules, and KV requirements are reported |
| CONTEXT.CLASS.0 | planned | context | Context class report | model max context, requested context, chunking policy, overflow behavior, and runtime blockers are reported |
| KV.CACHE.0 | planned | kv | KV cache class report | KV dtype, layout, layer/head/position indexing, residency class, capacity, and unsupported blockers are reported |
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
| SPINE.OPERATOR.PRESET.0 | complete | docs | Operator preset and path-resolution roadmap | spine defines path configuration, target path resolution, model prepare, model check, graph check, chat UX, and final runbook preset sequence |
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
| SPINE.REBASE.1 | complete | docs | Execution-chain audit and M3-M8 technical rebase | runtime ladder through M8 mapped |
| SPINE.REBASE.2 | complete | docs | Runtime track rebase before M3 | M3/M4 path clarified before implementation |
| SPINE.REBASE.3 | complete | docs | End-to-end runtime and operator roadmap | runtime/operator roadmap expanded |
| SPINE.REBASE.4 | complete | docs | Artifact integrity and measurement target rebase | artifact integrity module added before graph expansion |
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
| SPINE.REBASE.5 | complete | docs | Unified full inference engine spine | all delivery rows consolidated into one ledger and dependency map |
| SPINE.BLOCKS.0 | complete | docs | Canonical inference block directory | spine defines engine identity, canonical implementation blocks, naming rules, command taxonomy, tensor collections, residency modes, and procedural order |
| SPINE.BLOCKS.1 | planned | docs | Planned-row deduplication and command-flow compression | redundant planned rows are merged into canonical blocks without deleting completed history |
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
| CLI.GEN.0 | planned | cli | CLI generation command surface | exposes the implemented bounded generation loop through CLI with honest unsupported/full-model/benchmark boundaries |
| M16 | planned | server | Provider/server generation boundary | daemon/server generation uses runtime-backed generation path |
| M17 | planned | profile | Trace/profile hardening for generation | traces and profiles identify artifact/backend/graph/KV/decode/logits/sampling/server failures |
| SPINE.GENERATION.TARGET.0 | complete | docs | DeepSeek generation and speculative throughput target envelope | spine records DeepSeek V4 Flash full-generation target, internal decode throughput target, DSpark external reference doctrine, and non-claim benchmark boundary |
| GEN.DEEPSEEK.0 | planned | generation | DeepSeek V4 Flash full generation path | DeepSeek V4 Flash reaches real decode, logits, sampling, token append, stop conditions, cleanup, and CLI-visible generation over YVEX runtime |
| BENCH.DEEPSEEK.DECODE.0 | planned | bench | DeepSeek V4 Flash baseline decode throughput target | after generation exists, benchmark harness measures decode tok/s with artifact identity, qtype, context, backend, machine, command, and reproducibility metadata |
| BENCH.DEEPSEEK.GEN.0 | planned | bench | DeepSeek V4 Flash end-to-end generation throughput target | after generation exists, benchmark harness measures prompt plus generated-token throughput separately from prefill |
| SPEC.DSPARK.REF.0 | planned | reference | DSpark speculative decoding reference | DSpark is recorded as external reference evidence for semi-autoregressive drafting, confidence scheduling, and hardware-aware verification, not as YVEX runtime capability |
| SPEC.DEEPSEEK.0 | planned | generation | DeepSeek speculative decoding target | after baseline generation exists, YVEX may implement draft, verification, accepted-token accounting, and speculative generation over DeepSeek target runtime |
| BENCH.DEEPSEEK.SPEC.0 | planned | bench | DeepSeek speculative generation benchmark | after speculative decoding exists, benchmark harness measures accepted tokens, verification cost, latency, throughput, and speedup over YVEX baseline generation |
| FULLMODEL.0 | planned | model | Full model inventory and placement plan | full artifact tensor inventory, memory budget, and backend placement report |
| FULLMODEL.1 | planned | model | Full model materialization plan | selected-family full tensor placement and materialization preflight without generation claim |
| FULLMODEL.2 | planned | model | Full model materialization proof | full required tensor set materializes or fails with phase/cleanup reports |
| FULLMODEL.3 | planned | model | Full model runtime descriptor | model descriptor covers all tensors needed by prefill/decode/logits path |
| FAMILY.RUNTIME.0 | planned | model | Runtime family adapter boundary | model-family-specific tensor roles and graph requirements exposed as reports |
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

## DeepSeek Generation and Throughput Target

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

## Speculative Decoding Reference Track

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

DSpark evidence remains external evidence. It does not imply YVEX implements
speculative decoding and does not imply YVEX reaches any throughput target.

## Execution and Residency Modes

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

## Runtime Closure Doctrine

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

## Generation Loop Contract

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

## Generation State Machine

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

## Generation Token Lifecycle

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

## Generation Stop Reasons

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

## Generation CLI Output Contract

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

## Generation Trace Contract

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

## Generation Failure and Cleanup Contract

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

## Tensor Collections

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

## Attention, KV, and Context Rules

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

## MoE and Expert Activation Rules

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

## Build, Backend, and Hardware Profile Rules

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

## 6. Dependency Map

```text
source-to-artifact track:
  official source tensors
  -> source manifest
  -> source artifact identity
  -> native tensor inventory
  -> model target class
  -> model class profile
  -> tensor collections
  -> tensor role mapping
  -> quantization policy
  -> conversion plan
  -> YVEX-produced GGUF
  -> artifact identity
  -> registry/materialization readiness

residency track:
  artifact identity
  -> shard index
  -> tensor byte-range map
  -> residency class report
  -> memory/storage pressure report
  -> cold read probe
  -> warm read probe
  -> page/chunk plan
  -> staged residency
  -> runtime residency integration

graph track:
  tensor collections
  -> backend capability
  -> primitive op proof
  -> normalization
  -> attention projection
  -> attention execution
  -> residual
  -> MLP or MoE expert slice
  -> first transformer block
  -> layer scheduler

runtime track:
  token input
  -> context plan
  -> prefill state
  -> KV-backed chunked prefill lifecycle
  -> decode state step
  -> logits buffer
  -> sampler
  -> constrained generation loop
  -> CLI generation
  -> serve generation
  -> streaming generation
  -> baseline decode benchmark
  -> end-to-end generation benchmark
  -> speculative draft path

measurement track:
  fixture regression
  -> partial graph regression
  -> prefill regression
  -> decode/logits regression
  -> sampling determinism
  -> generation smoke
  -> capability eval
  -> prefill/decode/generation/memory/server benchmarks

backend and hardware track:
  build profile
  -> backend probe
  -> op capability matrix
  -> allocation/transfer pressure
  -> hardware profile
  -> machine-specific reproducibility metadata

operator preset track:
  operator model root configuration
  -> model target path resolution
  -> model prepare preset
  -> model check preset
  -> graph check preset
  -> diagnostic REPL clarity
  -> final operator runbook
  -> public operator examples
```

These tracks may advance in parallel only when their boundaries are explicit.
A row is complete only when its command proof demonstrates the boundary it
claims.

The first bounded generation loop is not the same as DeepSeek full generation.
DeepSeek full generation remains blocked until full model runtime requirements
are implemented.

The speculative acceleration track starts after baseline generation exists. It
cannot replace decode, logits, sampling, or the generation loop. Its benchmark
must compare against YVEX baseline generation, not against external claims.

The operator preset track may advance before runtime generation because it
compresses already implemented lower-level boundaries into safer operator
commands. It cannot claim a model, graph, runtime, generation, eval, or
benchmark capability that the lower-level command path has not already proven.

M8 is not the final prefill path. It is the first prefill-state foundation.
PREFILL.1 binds that foundation to minimal session-owned KV state, but it does
not compute real attention K/V. A full transformer run still requires graph op
expansion, transformer block execution, layer scheduling, real transformer
prefill, decode, logits, sampling, and a generation loop.

The current executable graph slices are controlled fixture embedding, selected
real embedding, and selected real embedding-plus-RMSNorm. Those slices prove
parser, materialization, backend, graph scheduling, reference comparison, and
cleanup boundaries. They do not prove full model materialization, transformer
attention, MLP, routed experts, logits, sampling, or generation.

The standalone RoPE operation proves a position-dependent F32 graph op over a
small deterministic vector on CPU and CUDA where available. It validates
head_dim parity, position, byte accounting, backend op support, output
allocation, dispatch, reference comparison, cleanup, checksum, and max-diff
reporting. It does not prove attention, QKV projection, transformer block
execution, layer scheduling, decode, logits, sampling, or generation.

The standalone attention primitive proves explicit F32 Q/K/V scaled dot-product
attention for one query over a bounded key/value prefix on CPU and CUDA where
available. It validates `seq_len`, position bounds, head dimension, causal mask,
scratch sizing, backend op support, output allocation, dispatch, reference
comparison, cleanup, checksum, softmax diff, and max-diff reporting. It does not
project Q/K/V from model tensors, write real KV cache rows, execute a
transformer block, schedule layers, run full transformer prefill, decode,
produce logits, sample, or generate.

The standalone matmul/projection primitive proves explicit F32 row-major
`input=[m,k]`, `weight=[k,n]`, `output=[m,n]` multiplication on CPU and CUDA
where available. It validates projection shape `m=1`, non-projection matrix
shape, non-zero dimensions, byte accounting, backend op support, output
allocation, dispatch, reference comparison, cleanup, checksum, and max-diff
reporting. It does not read real model projection weights, create Q/K/V tensors
for attention, execute a transformer block, schedule layers, run full
transformer prefill, decode, produce logits, sample, or generate.

The standalone MLP/feed-forward primitive proves explicit F32 gated SiLU
feed-forward execution on CPU and CUDA where available. Dense mode validates
input, gate/up/down weights, intermediate activation, output byte accounting,
backend op support, dispatch, reference comparison, cleanup, checksum, and
max-diff reporting. Routed-expert mode selects one deterministic expert weight
slice by explicit `expert_id`. It does not compute router logits, run top-k
routing, load-balance experts, read real model expert tensors, execute a
transformer block, schedule layers, run full transformer prefill, decode,
produce logits, sample, or generate.

Full model materialization and placement are explicit planned work because the
runtime must inventory and place the complete required tensor set before a real
transformer path can rely on it. Decode cannot be meaningful until graph/layer
execution and real transformer prefill create runtime state worth advancing.

Evaluation must follow implemented runtime boundaries. Fixture eval is not model
quality. Logits regression is not capability eval. Capability eval starts only
when the same generation path users run exists.

Benchmarks must follow implemented runtime paths. No throughput number is valid
without the measured command, machine, backend, artifact identity, qtype,
context, and reproducibility note.

Metal and ROCm rows are feasibility lanes only. They do not claim implemented
backend support.

## Model Target Classes

YVEX target classes:

```text
selected-runtime-slice:
  a small executable or materializable slice of a real YVEX-produced artifact
  used to prove parser, materialization, backend, graph, reference, and cleanup
  behavior.

official-source-huge-model:
  official upstream source tensors used to force source manifest, native tensor
  inventory, model-class profiling, tensor mapping, quantization policy, and
  future YVEX-produced artifacts.

full-runtime-model:
  a complete tensor set required for transformer prefill, decode, logits, and
  generation. This class is planned and not implemented.

huge-model-storage-stream:
  a model whose source or target artifact footprint forces shard inventory,
  qtype profiling, storage layout, cold/warm access, page/chunk planning, and
  staged residency before runtime execution can be claimed.

external-GGUF-reference:
  an external GGUF used only to compare artifact layout, qtype choice, or
  external runner behavior. It is never a YVEX-produced artifact.

external-runner-reference:
  an external run used to compare deployment constraints or runtime behavior.
  It is never a YVEX runtime capability claim.

Current target assignments:
  DeepSeek selected embed: selected-runtime-slice
  DeepSeek selected embed + RMSNorm: selected-runtime-slice
  GLM-5.2 official safetensors: official-source-huge-model
  GLM-5.2 future YVEX-produced GGUF: huge-model-storage-stream
  External GLM GGUFs: external-GGUF-reference only
```

## Operator-local Artifact Storage

Large model files must live outside the repository.

Current Spark operator-local layout:

```text
source weights:
  ~/lab/models/hf/<family>/<model>/

YVEX-produced GGUF:
  ~/lab/models/gguf/<family>/

external reference artifacts:
  ~/lab/models/reference/<family>/

generated reports:
  ~/lab/models/reports/<family>/
```

The spine records this as operator-local model storage. Public docs must not
depend on these personal paths.

No `.safetensors`, `.bin`, `.dat`, or real `.gguf` shard may be committed.
Tiny synthetic GGUF fixtures in tests are the only exception.

## Operator Preset Command Contracts

### OPERATOR.PATHS.0 command contract

```text
OPERATOR.PATHS.0 command surface:

  yvex paths configure --models-root DIR
  yvex paths configure --models-root DIR --create
  yvex paths configure --reset
  yvex paths
  yvex paths --create
  yvex paths resolve --family deepseek --kind source
  yvex paths resolve --family deepseek --kind gguf
  yvex paths resolve --family deepseek --kind reports
  yvex paths resolve --family deepseek --kind reference
  yvex paths resolve --family deepseek --kind registry
  yvex paths resolve --family glm --kind source
  yvex paths resolve --family glm --kind gguf
  yvex paths resolve --family glm --kind reports
  yvex paths resolve --family glm --kind reference
  yvex paths resolve --family glm --kind registry

Path resolution precedence:

  explicit command flag
  -> configured operator-local YVEX state
  -> technical environment override
  -> builtin development default

The technical environment override may exist, but it is not the normal operator
path.

`paths configure` does not download models, create model artifacts, register
aliases, or claim runtime capability.
```

Expected fields:

```text
status: paths-configured
models_root:
hf_root:
gguf_root:
reports_root:
reference_root:
registry_root:
created:
```

### MODEL.TARGET.PATHS.0 command contract

```text
MODEL.TARGET.PATHS.0 command surface:

  yvex model-target inspect TARGET --paths
  yvex model-target inspect TARGET --paths --models-root DIR

The command reports paths derived from the model target and operator root.

For DeepSeek selected embedding, it reports:

  source_path
  source_exists
  artifact_path
  artifact_exists
  report_dir
  report_dir_exists
  reference_dir
  reference_dir_exists
  registry_dir
  registry_dir_exists
  registry_alias
  source_artifact_class
  target_artifact_class
  runtime_execution
  generation

For GLM-5.2 official safetensors, it reports:

  source_path
  source_exists
  artifact_path: planned
  artifact_exists: false
  report_dir
  reference_dir
  registry_dir
  registry_alias: none
  source_artifact_class: official safetensors
  target_artifact_class: future YVEX-produced GGUF
  runtime_execution: unsupported
  generation: unsupported

The command must not inspect GLM safetensors, require GLM download completion,
hash GLM files, or claim GLM execution.
```

### MODEL.PREPARE.0 command contract

```text
MODEL.PREPARE.0 command surface:

  yvex models prepare TARGET
  yvex models prepare TARGET --overwrite
  yvex models prepare TARGET --source DIR
  yvex models prepare TARGET --out FILE
  yvex models prepare TARGET --out-dir DIR
  yvex models prepare TARGET --models-root DIR
  yvex models prepare TARGET --registry FILE
  yvex models prepare TARGET --dry-run
  yvex models prepare TARGET --no-register
  yvex models prepare TARGET --no-use

For `deepseek4-v4-flash-selected-embed`, prepare composes only the already
implemented selected source-to-GGUF path:

  source manifest
  native tensor inventory
  tensor mapping
  conversion plan
  selected GGUF emission
  inspect
  tensors
  metadata
  registry add
  registry use
  registry verify

Prepare does not materialize, run graph, start a server, run chat, decode,
produce logits, sample, generate, evaluate, or benchmark.

For source-only targets such as `glm-5.2-official-safetensors`, prepare returns
a clear unsupported report until the corresponding YVEX-produced GGUF path
exists.
```

Expected output fields:

```text
status: model-prepare
target_id:
source_path:
artifact_path:
alias:
stage:
runtime_execution:
generation:
```

### MODEL.CHECK.0 command contract

```text
MODEL.CHECK.0 command surface:

  yvex models check TARGET
  yvex models check TARGET --backend cpu
  yvex models check TARGET --backend cuda
  yvex models check TARGET --level quick
  yvex models check TARGET --level runtime
  yvex models check TARGET --level full
  yvex models check TARGET --models-root DIR
  yvex models check TARGET --registry FILE
  yvex models check TARGET --report-dir DIR
  yvex models check TARGET --no-materialize
  yvex models check TARGET --no-graph

Level semantics:

  quick:
    resolve target or alias
    inspect
    tensors
    metadata
    integrity check

  runtime:
    quick
    integrity report
    materialize
    engine
    session
    plan
    selected graph partial when the target supports it

  full:
    runtime
    model gate when target expectations are known
    materialization gate when target expectations are known

`models check` never claims generation. It cannot close decode, logits,
sampling, generation, eval, or benchmark rows.
```

Expected output fields:

```text
status: model-check
target_id:
backend:
level:
stage:
runtime_execution:
generation:
```

### MODEL.CHECK.1 segment contract

```text
MODEL.CHECK.1 extends `models check` for the selected embedding-plus-RMSNorm
segment target.

Runtime-level segment checks may include:

  input tokens
  selected embedding-plus-RMSNorm graph segment
  segment-summary prefill
  segment-summary prefill plus minimal KV binding

This remains segment-summary diagnostics only. It is not full transformer
prefill, attention-backed KV, decode, logits, sampling, or generation.
```

### GRAPH.CHECK.0 command contract

```text
GRAPH.CHECK.0 command surface, after GRAPH.BLOCK.0 exists:

  yvex graph check --suite primitives --backend cpu
  yvex graph check --suite primitives --backend cuda
  yvex graph check --suite block --backend cpu
  yvex graph check --suite block --backend cuda
  yvex graph check --suite selected --model TARGET --backend cpu
  yvex graph check --suite selected --model TARGET --backend cuda
  yvex graph check --suite segment --model TARGET --backend cpu
  yvex graph check --suite segment --model TARGET --backend cuda
  yvex graph check --suite all --backend cpu

Suite semantics:

  primitives:
    RoPE, attention, matmul, MLP primitive proofs.

  block:
    controlled block fixture proof after GRAPH.BLOCK.0.

  selected:
    selected embedding graph over selected artifact.

  segment:
    selected embedding-plus-RMSNorm segment graph over segment artifact.

  all:
    implemented suites only; unsupported suites report unsupported rather than
    pretending to pass.

`graph check` does not read full model weights and does not claim full
transformer execution unless the underlying graph rows exist.
```

### CHAT.UX.0 command contract

```text
CHAT.UX.0 improves the accepted-only diagnostic REPL.

It may improve:

  startup banner
  /help
  /status
  /quit and /exit wording
  model/backend/session state display
  unsupported generation warning
  optional color
  NO_COLOR support
  non-TTY fallback

It must not implement generation.

It must not present the accepted-only REPL as a chat model.

It must continue to state that decode, logits, sampling, and generation are
unsupported until implemented by runtime rows.
```

### OPERATOR.FLOW.3 docs contract

```text
OPERATOR.FLOW.3 rewrites the runbook after the preset commands exist.

The final runbook should be based on commands such as:

  yvex paths configure --models-root DIR
  yvex model-target inspect TARGET --paths
  yvex models prepare TARGET
  yvex models check TARGET --backend cpu --level runtime
  yvex graph check --suite primitives --backend cpu
  yvex chat --model FIXTURE --backend cpu

The runbook must not compensate for missing CLI behavior with shell export
walls, scripts, conditionals, or path derivation logic.
```

## 7. Active Next

```text
CLI.GEN.0 - CLI generation command surface
```

CLI.GEN.0 may expose the implemented bounded diagnostic generation loop through
clearer CLI ergonomics and help. It must preserve the full-model, real
DeepSeek, provider, streaming, evaluation, and benchmark unsupported
boundaries.

Algorithm/CLI research hardening runs in parallel with runtime closure. It does
not replace CLI.GEN.0 or the current runtime Active Next.

GEN.CONTRACT.0 hardens the contract for the generation loop. GEN.LOOP.0 is
complete for bounded diagnostic loop control only.

PREFILL.4 remains planned as diagnostics/regression hardening over the
implemented prefill state path. PREFILL.5 remains planned as a future
measurement gate. Neither blocks generation state hardening or later full-model
runtime work.

SPINE.GENERATION.TARGET.0 records the long-term DeepSeek generation and
throughput target. It does not change the immediate implementation order.

MODEL.PREPARE.0 and MODEL.CHECK.0 are complete for the DeepSeek selected
embedding target. MODEL.CHECK.1 remains planned.

Runtime active next remains:

```text
CLI.GEN.0 - CLI generation command surface
```

GEN.LOOP.1 is complete as local bounded diagnostic generation state hardening:
state lifecycle fields, deterministic `--cancel-after-steps`, interrupted stop
reason, cancel trace records, cleanup idempotence, state-release reporting, and
partial-output preservation exist for the implemented generate command. It
does not provide OS signal cancellation, server/provider cancellation,
streaming cancellation, full model generation, real DeepSeek generation,
evaluation, or benchmark measurement.

GEN.TRACE.0 is complete as a stable trace-level surface over the bounded
diagnostic generation loop. It does not create full-model generation, real
DeepSeek generation, provider generation, evaluation, benchmark, raw tensor, or
tensor-dump capability.

GEN.STOP.0 is complete as explicit stop-policy behavior for the bounded
diagnostic generation loop: max-new-tokens is a post-append stop,
context-limit is a pre-append stop, phase-specific failures are failure stops,
and EOS/stop-token behavior remains unsupported until real tokenizer-backed
policy exists.

GRAPH.CHECK.0 is complete as a command preset over existing graph proofs. It
does not create new graph capability beyond the primitive, block, and layer
fixture commands it runs.

OWI.TARGETS.1 remains planned until multi-family source manifest evidence is
implemented over available source artifacts. GLM source tensors may continue
downloading while runtime work resumes on the DeepSeek selected GGUF path.

Spine structure next after this rebase:

```text
SPINE.BLOCKS.1 - Planned-row deduplication and command-flow compression
```

SPINE.BLOCKS.1 is a future cleanup row, not the active next implementation.

GRAPH.LAYERS.0 is complete as a repeated controlled block fixture with
selected-position activation handoff. It is not full transformer prefill,
decode, logits, sampling, generation, server generation, evaluation, or
benchmark readiness.

After GEN.LOOP.1, bounded diagnostic generation state and local cancellation
are explicit enough for CLI surface hardening. Real model output-head logits,
real vocabulary sampling, full DeepSeek runtime work, OS signal cancellation,
provider/server generation, streaming, evaluation, and benchmark measurement
remain planned tracks.

## 8. Validation Gate

Baseline:

```sh
make clean
make check
make smoke
make check-cuda
git diff --check
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```

Current command-surface audit:

```sh
./yvex commands
./yvex help decode
./yvex help logits
./yvex help sample
./yvex help graph
./yvex help input
./yvex help prefill
./yvex help engine
./yvex help session
./yvex help integrity
./yvex help models
```

Bounded decode/logits/sampling/generation closure proof sequence:

```sh
./yvex decode --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex logits --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex sample --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3
./yvex generate --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --max-new-tokens 3
```

These commands prove bounded diagnostic runtime control only. They do not prove
full DeepSeek generation, provider generation, evaluation, or benchmark
readiness.

Selected artifact closeout proof set:

```sh
./yvex models verify deepseek4-v4-flash-selected-embed
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cpu --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cpu
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cpu --execute-partial --partial-token 0
./yvex integrity report --model deepseek4-v4-flash-selected-embed --backend cuda --require-token-embedding --partial-token 0
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex graph --model deepseek4-v4-flash-selected-embed --backend cuda --execute-partial --partial-token 0
```

Selected segment proof set:

```sh
./yvex models verify deepseek4-v4-flash-selected-embed-rmsnorm
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
./yvex graph --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --execute-segment --segment embedding-rmsnorm --tokens 0,1 --token-index 0
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cuda --segment embedding-rmsnorm --tokens 0,1,2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 1
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1 --layers 2
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2 --layers 2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --chunk-size 2
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --layers 2 --chunk-size 2
./yvex prefill --model deepseek4-v4-flash-selected-embed-rmsnorm --backend cpu --segment embedding-rmsnorm --tokens 0,1,2,3 --layers 2 --chunk-size 2 --attach-kv --kv-layers 1 --kv-heads 2 --kv-head-dim 4 --kv-capacity 8
```

Standalone graph op proof set:

```sh
./yvex graph --backend cpu --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cuda --execute-op --op rope --position 7 --head-dim 8
./yvex graph --backend cpu --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cuda --execute-op --op attention --seq-len 4 --position 3 --head-dim 8 --causal
./yvex graph --backend cpu --execute-op --op matmul --m 1 --k 8 --n 8
./yvex graph --backend cuda --execute-op --op matmul --m 1 --k 8 --n 8
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
./yvex graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated
./yvex graph --backend cpu --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1
./yvex graph --backend cuda --execute-op --op mlp --hidden-dim 8 --ffn-dim 16 --activation silu --gated --experts 2 --expert-id 1
```

Spine structure proof:

```sh
grep -nF 'CLI.UX.''*' docs/spine.md && exit 1 || true
grep -nF 'SERVER.''*' docs/spine.md && exit 1 || true
grep -nF 'EVAL.''*' docs/spine.md && exit 1 || true
grep -nF 'BENCH.''*' docs/spine.md && exit 1 || true
grep -nF 'KV.MIN.''*' docs/spine.md && exit 1 || true
grep -nF 'RUNTIME.KV.''*' docs/spine.md && exit 1 || true
grep -nF 'BACKEND.PROFILE.''*' docs/spine.md && exit 1 || true
grep -nF 'LAYOUT.''*' docs/spine.md && exit 1 || true
grep -nF 'DOCS.''*' docs/spine.md && exit 1 || true

grep -nF 'PREFILL.1' docs/spine.md
grep -nF 'GRAPH.OPS.0' docs/spine.md
grep -nF 'GRAPH.BLOCK.0' docs/spine.md
grep -nF 'GRAPH.LAYERS.0' docs/spine.md
grep -nF 'FULLMODEL.0' docs/spine.md
grep -nF 'DECODE.1' docs/spine.md
grep -nF 'LOGITS.1' docs/spine.md
grep -nF 'BENCH.GEN.0' docs/spine.md
grep -nF 'BACKEND.METAL.0' docs/spine.md
grep -nF 'BACKEND.ROCM.0' docs/spine.md
```

Source-tensor OWI spine proof:

```sh
grep -nF 'OWI.REBASE.0' docs/spine.md
grep -nF 'official source tensors' docs/spine.md
grep -nF 'YVEX-produced GGUF' docs/spine.md
grep -nF 'GLM-5.2 official safetensors' docs/spine.md
grep -nF 'official-source-huge-model' docs/spine.md
grep -nF 'external-GGUF-reference' docs/spine.md
grep -nF 'OWI.TARGETS.0' docs/spine.md
grep -nF 'OWI.HUGE.0' docs/spine.md
grep -nF 'STORAGE.STREAM.0' docs/spine.md
grep -nF 'MODEL.CLASS.0' docs/spine.md
grep -nF 'GRAPH.LAYERS.0 - Layer scheduler and repeated block execution' docs/spine.md

pattern='external GGUF satisfies OW''I'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='GLM runtime execution: imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='GLM generation: imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='storage-stream generation: imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='disk-backed generation: imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='YVEX GLM bench''mark'
grep -nF "$pattern" docs/spine.md && exit 1 || true
```

Canonical block directory proof:

```sh
grep -nF '## 1.1 Engine Identity' docs/spine.md
grep -nF '## 2.1 Canonical Block Directory' docs/spine.md
grep -nF 'BLOCK 0 — Source and target evidence' docs/spine.md
grep -nF 'BLOCK 9 — Evaluation, benchmarks, and public evidence' docs/spine.md
grep -nF '## 2.2 Naming and Ownership Rules' docs/spine.md
grep -nF '## 2.3 Procedural Implementation Order' docs/spine.md
grep -nF '## 2.4 Conceptual Command Taxonomy' docs/spine.md
grep -nF '## Execution and Residency Modes' docs/spine.md
grep -nF '## Tensor Collections' docs/spine.md
grep -nF '## Attention, KV, and Context Rules' docs/spine.md
grep -nF '## MoE and Expert Activation Rules' docs/spine.md
grep -nF '## Build, Backend, and Hardware Profile Rules' docs/spine.md
grep -nF 'SPINE.BLOCKS.0' docs/spine.md
grep -nF 'SPINE.BLOCKS.1' docs/spine.md
grep -nF 'TENSOR.COLLECTION.0' docs/spine.md
grep -nF 'ATTENTION.CLASS.0' docs/spine.md
grep -nF 'CONTEXT.CLASS.0' docs/spine.md
grep -nF 'KV.CACHE.0' docs/spine.md
grep -nF 'MOE.CLASS.0' docs/spine.md
grep -nF 'RESIDENCY.0' docs/spine.md
grep -nF 'BUILD.PROFILE.0' docs/spine.md
grep -nF 'HARDWARE.PROFILE.0' docs/spine.md
grep -nF 'COMPUTE.BACKEND.0' docs/spine.md
grep -nF 'OPERATOR.FLOW.0' docs/spine.md
grep -nF 'SERVE.RUNTIME.0' docs/spine.md

pattern='YVEX supports GLM gener''ation'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='disk-backed generation imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='external runner proves YV''EX'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='external GGUF satisfies OW''I'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='storage streaming is gener''ation'
grep -nF "$pattern" docs/spine.md && exit 1 || true
pattern='backend implementation is model sup''port'
grep -nF "$pattern" docs/spine.md && exit 1 || true
```

Operator preset roadmap proof:

```sh
grep -nF 'SPINE.OPERATOR.PRESET.0' docs/spine.md
grep -nF 'OPERATOR.PATHS.0' docs/spine.md
grep -nF 'MODEL.TARGET.PATHS.0' docs/spine.md
grep -nF 'MODEL.PREPARE.0' docs/spine.md
grep -nF 'MODEL.CHECK.0' docs/spine.md
grep -nF 'GRAPH.CHECK.0' docs/spine.md
grep -nF 'CHAT.UX.0' docs/spine.md
grep -nF 'OPERATOR.FLOW.3' docs/spine.md
grep -nF 'DOCS.README.OPERATOR.0' docs/spine.md
grep -nF 'Operator Preset Command Contracts' docs/spine.md
grep -nF 'operator preset track:' docs/spine.md
grep -nF 'OPERATOR.PATHS.0 - Operator model root configuration' docs/spine.md
grep -nF 'GRAPH.LAYERS.0 - Layer scheduler and repeated block execution' docs/spine.md

pattern='paths configure imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='model prepare imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='model check imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='graph check imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='generation imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true
```

Generation target envelope proof:

```sh
grep -nF 'SPINE.GENERATION.TARGET.0' docs/spine.md
grep -nF 'GEN.DEEPSEEK.0' docs/spine.md
grep -nF 'BENCH.DEEPSEEK.DECODE.0' docs/spine.md
grep -nF 'BENCH.DEEPSEEK.GEN.0' docs/spine.md
grep -nF 'SPEC.DSPARK.REF.0' docs/spine.md
grep -nF 'SPEC.DEEPSEEK.0' docs/spine.md
grep -nF 'BENCH.DEEPSEEK.SPEC.0' docs/spine.md
grep -nF '## DeepSeek Generation and Throughput Target' docs/spine.md
grep -nF '## Speculative Decoding Reference Track' docs/spine.md
grep -nF 'YVEX real generation throughput: 0 tok/s' docs/spine.md
grep -nF 'YVEX target: >=20 tok/s baseline decode' docs/spine.md
grep -nF 'DSpark evidence remains external evidence' docs/spine.md

pattern='DeepSeek generation imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='DeepSeek decode benchmark imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='speculative decoding imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='DSpark imple''mented'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='20 tok/s achie''ved'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='YVEX supports DS''park'
grep -nF "$pattern" docs/spine.md && exit 1 || true

pattern='DSpark par''ity'
grep -nF "$pattern" docs/spine.md && exit 1 || true
```

Additional guardrails:

```text
public docs boundary
artifact guardrail
forbidden external reference guardrail
claim guardrail
local registry guardrail
public path leak guardrail
no runtime file change for spine-only rebases
```

## 9. Non-Negotiable Rules

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
- A speculative path must measure accepted tokens, rejected tokens,
  verification cost, latency, throughput, and speedup against YVEX baseline.
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
- No wildcard delivery rows in the unified ledger.
- No secondary track tables duplicating the unified ledger.
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
