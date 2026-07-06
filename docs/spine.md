# YVEX Inner Delivery Spine

Date: 2026-07-06
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA first
Primary interface: CLI

This file is the active delivery spine. It is not a historical ledger, not a
roadmap archive, and not a substitute for implementation. Historical delivery
details remain available through git history.

The active docs set is docs/api.md, docs/contract.md, docs/model-families.md, docs/operator-runbook.md, docs/cli-output-architecture.md, docs/spine.md.

## 0. Dashboard

YVEX is a local-first inference engine, not a chat wrapper.

| Field | Current value |
| --- | --- |
| Project | YVEX |
| Type | local-first inference engine |
| Primary interface | CLI |
| Primary implementation | C, Linux + CUDA first |
| Current highest implemented runtime stage | bounded diagnostic generation |
| Current full-runtime state | unsupported |
| Generation state | DeepSeek/Qwen/Gemma unsupported |
| Benchmark state | not measured |
| CUDA state | bounded primitive-hardening only |
| Active Next | V010.QUANT.1 - multi-family dtype/qtype support by role |
| v0.1.0 target | v0.1.0 - multi-family supported generation over YVEX-produced quantized artifacts |
| Supported v0.1.0 families | DeepSeek, Qwen, Gemma |
| Main blocker | multi-family generation-capable artifact chain incomplete |

The supported v0.1.0 generation family set is DeepSeek, Qwen, and Gemma.
GLM remains source/storage pressure. GLM remains huge source/storage pressure
unless it is explicitly promoted by a later target decision.
Qwen/Metal remains post-v0.1.0 backend portability.

## 1. Current Capability

Current capability is implementation-backed only. Report rows, source rows, and
diagnostic rows do not promote runtime readiness.

| Capability group | Current state | Main proof | Boundary |
| --- | --- | --- | --- |
| artifact parsing and integrity | implemented | GGUF parsing, descriptor construction, integrity reports, corruption refusal, materialization gates | not source verification, not generation |
| source intake and manifests | implemented/source-intake | source manifests, native header inventories, download receipts, provider logs, stop/resume/cleanup controls | no payload loading, no GGUF emission |
| Qwen/Gemma source/profile/tensor-map/qtype reports | report-only | source profiles, model-class profiles, tensor collection inventory, qtype policy report | no runtime support, no generation |
| selected DeepSeek artifact slices | selected-slice-proof | selected embedding and embedding-plus-RMSNorm artifacts | not full DeepSeek runtime |
| graph primitives and selected graph slices | fixture-proof/selected-slice-proof | RoPE, attention, projection, MLP/routed-expert primitive proofs; selected graph slices | not full transformer execution |
| diagnostic prefill/KV/decode/logits/sampling/generation | diagnostic-runtime | bounded diagnostic `prefill`, `kv`, `decode`, `logits`, `sample`, `generate` | not full model generation |
| CUDA primitive hardening | bounded primitive-hardening | CUDA primitive tests for bounded MLP and attention paths against CPU/reference outputs | not CUDA runtime generation |
| operator CLI baseline | implemented/operator | compact normal output, audit evidence, renderer foundation, base command grammar | not lower runtime capability |
| provider account/download control | implemented/source-intake | `accounts`, `models download`, live progress, signal-safe interruption, status/stop/resume/cleanup | no source identity verification, no artifact registration |
| docs/claim guardrails | implemented/docs | docs surface tests, public-doc boundary checks, artifact guardrail checks | no runtime capability |

Key completed implementation rows preserved by the active map:

| Row | Status | Area | Boundary |
| --- | --- | --- | --- |
| SPINE.RETARGET.MULTIFAMILY.0 | complete | docs/artifact | v0.1.0 multi-family generation target lock |
| SPINE.TRACK.CANON.0 | complete | docs/spine | oversized spine replaced with active track-first roadmap |
| V010.QUANT.0 | complete | quant | multi-family qtype policy report |
| CLI.ARCH.AUDIT.0 | complete | docs/operator | CLI architecture audit and print ownership map |
| V010.CLI.25 | complete | operator | Renderer ownership foundation |
| V010.CLI.26 | complete | operator | Base CLI grammar and command catalog |
| SPINE.CLI.REBASE.1 | complete | docs/operator | Full Operator CLI track rebase after executed V010.CLI.26 |

Compact row anchors:

```text
V010.CLI.25         renderer ownership foundation
V010.CLI.26         base CLI grammar and command catalog
V010.QUANT.1        multi-family dtype/qtype support by runtime role
V010.CLI.MODELS.4   models artifacts porcelain
V010.CLI.RUNTIME.0  runtime diagnostic command grammar
```

`SPINE.CLI.REBASE.1 - full Operator CLI track rebase after executed V010.CLI.26`
is complete. `V010.CLI.27 - base status and refusal grammar` is planned, but
it is not Active Next.

## 2. Unsupported Boundaries

Unsupported means unsupported until there is implementation, tests, command
proof, lifecycle behavior, and claim-safe documentation.

| Group | Unsupported boundaries |
| --- | --- |
| Runtime | real transformer prefill, real attention-backed KV, real decode, real output-head logits, real vocabulary sampling, full runtime generation |
| Families | DeepSeek generation, Qwen generation, Gemma generation, GLM runtime, arbitrary-family runtime |
| Artifacts | full source-to-GGUF emission for supported v0.1.0 families, generation-capable artifact proof, external GGUF as support evidence |
| Evidence | evaluation, capability evaluation, benchmark, throughput, release readiness |
| Backends | full CUDA runtime generation, Metal runtime, ROCm runtime, distributed execution |
| Serving | provider-backed generation, streaming generation, production serving compatibility |

Forbidden current claims:

```text
full-model support claims
inference-readiness claims
generation-readiness claims
prefill-readiness claims
decode-readiness claims
CUDA runtime-readiness claims
Metal runtime-readiness claims
Qwen support claims
Gemma support claims
DeepSeek generation implementation claims
benchmark measurement claims
throughput achievement claims
release-ready claims
execution-ready claims
```

Allowed current language:

```text
report-only
header-only
source-intake
source-pressure
target-profile
fixture-proof
selected-slice-proof
diagnostic-runtime
bounded primitive
unsupported
not-measured
```

## 3. v0.1.0 Target

v0.1.0 target:

```text
v0.1.0 - multi-family supported generation over YVEX-produced quantized artifacts
```

v0.1.0 closes only when every supported v0.1.0 generation family reaches the
required artifact, runtime, generation, eval, and benchmark gates.

Supported v0.1.0 generation families:

```text
DeepSeek
Qwen
Gemma
```

Closure rules:

```text
DeepSeek cannot close v0.1.0 alone.
Qwen cannot close v0.1.0 alone.
Gemma cannot close v0.1.0 alone.
Qwen is a required v0.1.0 supported generation family.
Gemma is the first required dense-family generation target.
Selected slices cannot close v0.1.0.
External GGUF artifacts cannot close v0.1.0.
External runner output cannot close v0.1.0.
GLM source/storage pressure cannot close v0.1.0 unless GLM is explicitly promoted.
Qwen/Metal backend portability is post-v0.1.0.
```

Two concepts define the target:

| Concept | Meaning | Boundary |
| --- | --- | --- |
| generation-capable artifact | a YVEX-produced quantized artifact with required family metadata, tensor roles, qtype support, tokenizer/output-head coverage, identity, and integrity evidence | generation-capable artifact is not runtime generation |
| runtime generation | the engine executes prefill, KV, decode, logits, sampling, append, stop policy, cleanup, eval, and benchmark over the supported artifact path | requires runtime/eval/benchmark gates |

`generation-capable artifact is not runtime generation` is a hard boundary.

## 4. Active Next

Active Next:

```text
V010.QUANT.1 - multi-family dtype/qtype support by role
```

Why this row is next:

```text
V010.QUANT.0 is complete as a multi-family qtype policy report.
DeepSeek, Qwen, and Gemma still lack per-role dtype/qtype support evidence.
That evidence is required before generation-capable artifact emission can be safe.
```

Expected next decision after V010.QUANT.1:

```text
V010.QUANT.2 - qtype compute/refusal matrix
```

The exact next artifact-readiness row may change only if V010.QUANT.1 exposes a
more specific blocker.

## 5. Tracks

This is the only active track map. It replaces the old ledger, crosswalk,
supersession, audit, and sequence archives.

| Track | Owns | Current state | Complete rows | Next gate | Active |
| --- | --- | --- | --- | --- | --- |
| TRACK.SCOPE | release scope and claim boundaries | active | SPINE.RETARGET.MULTIFAMILY.0, SPINE.TRACK.CANON.0 | scope remains multi-family | no |
| TRACK.SOURCE | source intake, manifests, provider download controls | implemented/source-intake | V010.SOURCE.1-7, MODELS.DOWNLOAD.*, ACCOUNTS.PROVIDER.0, MODEL.TARGET.IDENTITY.0 | source evidence stays coherent | no |
| TRACK.MAP | family tensor roles and source-to-artifact handoff | partial/report-only | Qwen/Gemma class and tensor-map rows, V010.MAP.1/5/6/7/8/9 | artifact handoff | no |
| TRACK.QUANT | dtype/qtype policy and compute/refusal coverage | partial/report-only | V010.QUANT.0 | V010.QUANT.1 | yes |
| TRACK.ARTIFACT | YVEX-produced quantized artifacts | selected-slice only | controlled/selected artifact emission | multi-family generation-capable artifacts | no |
| TRACK.INTEGRITY | artifact identity, ranges, corruption refusal | implemented for current artifacts | integrity rows and materialization gates | full artifact gates | no |
| TRACK.MODEL | model class and dynamic routing | partial/report-only | Qwen/Gemma model-class reports, MoE reports | runtime class gates | no |
| TRACK.TENSOR | tensor collections and role coverage | partial/report-only | Qwen/Gemma tensor collection and coverage reports | required runtime tensor coverage | no |
| TRACK.RESIDENCY | placement, memory pressure, staged residency | planned/report-only | selected materialization plans | residency gate | no |
| TRACK.BACKEND | CPU/CUDA capability and future backend lanes | partial | CPU/CUDA primitive proofs | backend runtime gate | no |
| TRACK.GRAPH | primitives, selected slices, real graph path | partial | graph primitives and selected slices | full transformer graph | no |
| TRACK.PREFILL | real prefill and chunking | diagnostic-runtime only | diagnostic prefill rows | real prefill gate | no |
| TRACK.KV | KV layout, writes, reads, lifecycle | diagnostic/report-only | diagnostic KV rows | real KV gate | no |
| TRACK.DECODE | decode over runtime state | diagnostic-runtime only | bounded diagnostic decode | real decode gate | no |
| TRACK.LOGITS | output head and logits | diagnostic-runtime only | bounded diagnostic logits | real logits gate | no |
| TRACK.SAMPLING | token selection over logits | diagnostic-runtime only | bounded diagnostic sampler | real sampling gate | no |
| TRACK.TOKENIZER | tokenizer metadata and stop policy | partial/planned | tokenizer diagnostics | tokenizer gate | no |
| TRACK.GENERATION | decode/logits/sample/append loop | diagnostic-runtime only | bounded diagnostic generation | runtime generation gate | no |
| TRACK.OPERATOR | CLI grammar, output, porcelain/plumbing commands | implemented/partial | V010.CLI.17-26, CLI.ARCH.AUDIT.0, SPINE.CLI.REBASE.1 | CLI status/refusal grammar | no |
| TRACK.SERVE | daemon and provider surfaces | planned | status shell | serve runtime gate | no |
| TRACK.EVAL | correctness and capability evaluation | planned | tests only | eval gate | no |
| TRACK.BENCH | reproducible performance measurement | planned | none | benchmark gate | no |
| TRACK.RELEASE | validation transcript, claim audit, tag readiness | planned | docs/guardrail checks | release gate | no |
| TRACK.POST010 | portability, serving hardening, speculative, extra families | post-v0.1.0 | doctrine only | post-release selection | no |

### TRACK.SCOPE - Scope And Claims

Owns:
  release target, family set, unsupported boundaries, claim hygiene.

Current state:
  active.

Complete:
  SPINE.RETARGET.MULTIFAMILY.0, SPINE.TRACK.CANON.0.

Active / Next:
  keep v0.1.0 locked to DeepSeek/Qwen/Gemma multi-family generation.

Planned gates:
  scope can change only through an explicit target decision row.

Boundary:
  scope rows do not implement runtime behavior.

### TRACK.SOURCE - Source Intake

Owns:
  official source tensors, source manifests, native safetensors header
  inventory, provider account preflight, download lifecycle, source sidecars.

Current state:
  implemented/source-intake.

Complete:
  V010.SOURCE.1, V010.SOURCE.2, V010.SOURCE.3, V010.SOURCE.4,
  V010.SOURCE.5, V010.SOURCE.6, V010.SOURCE.7,
  V010.SOURCE.7A / MODELS.DOWNLOAD.0, MODELS.DOWNLOAD.LIVE.0,
  MODELS.DOWNLOAD.SIGNAL.0, MODELS.DOWNLOAD.CONTROL.0,
  V010.SOURCE.7B / ACCOUNTS.PROVIDER.0, MODEL.TARGET.IDENTITY.0,
  OWI.TARGETS.QWEN.0, OWI.TARGETS.GEMMA.0.

Active / Next:
  source identity must continue to feed map, quant, and artifact rows without
  static profile drift.

Planned gates:
  source acceptance remains header-only unless a future payload-loading row
  explicitly owns payload bytes.

Boundary:
  source intake is not artifact emission, runtime execution, generation, eval,
  benchmark, or release readiness.

### TRACK.MAP - Tensor Mapping

Owns:
  model-family tensor role mapping, output-head policy, tokenizer mapping,
  source-to-artifact handoff blockers.

Current state:
  partial/report-only.

Complete:
  MODEL.CLASS.QWEN.0, MODEL.CLASS.GEMMA.0,
  TENSOR.COLLECTION.QWEN.0, TENSOR.COLLECTION.GEMMA.0,
  V010.MAP.1, V010.MAP.5, V010.MAP.6, V010.MAP.7, V010.MAP.8,
  V010.MAP.9, MODELS.SOURCE.MAP.HANDOFF.0,
  MODELS.SOURCE.ROLEMAP.COVERAGE.0.

Active / Next:
  map facts feed quant support by role.

Planned gates:
  complete tensor role coverage for generation-capable artifact emission.

Boundary:
  tensor mapping is not runtime support.

### TRACK.QUANT - Quantization And Dtype

Owns:
  qtype policy, dtype/qtype support by runtime role, compute/refusal matrix,
  artifact qtype readiness.

Current state:
  partial/report-only.

Complete:
  V010.QUANT.0.

Active / Next:
  V010.QUANT.1 - multi-family dtype/qtype support by role.

Planned gates:
  V010.QUANT.2 - qtype compute/refusal matrix; artifact qtype readiness gate.

Boundary:
  qtype policy is not artifact emission, kernel support, generation, or
  benchmark evidence.

### TRACK.ARTIFACT - Artifact Production

Owns:
  YVEX-produced GGUF or future artifact emission, metadata, identity, and
  family-specific conversion.

Current state:
  selected-slice only.

Complete:
  controlled tiny artifact emission and selected DeepSeek slice emission.

Active / Next:
  wait for map and quant readiness across DeepSeek/Qwen/Gemma.

Planned gates:
  generation-capable artifact for each supported v0.1.0 family.

Boundary:
  a generation-capable artifact is not runtime generation.

### TRACK.INTEGRITY - Artifact Identity And Gates

Owns:
  file identity, digest, GGUF structure, tensor directory, byte ranges,
  dtype/rank/shape checks, corruption refusal, materialization preflight.

Current state:
  implemented for current selected and fixture artifacts.

Complete:
  artifact identity and integrity rows through materialization and graph guards.

Active / Next:
  extend integrity gates to generation-capable artifacts when they exist.

Planned gates:
  full artifact identity and integrity acceptance for DeepSeek/Qwen/Gemma.

Boundary:
  integrity is not supply-chain verification beyond implemented checks, and is
  not runtime execution.

### TRACK.MODEL - Model Class

Owns:
  dense/MoE/source-only/selected-slice classification and runtime requirement
  reports.

Current state:
  partial/report-only.

Complete:
  Qwen and Gemma model-class profiles; DeepSeek/MoE class reports; attention,
  KV, and context class reports.

Active / Next:
  class facts feed map, quant, graph, and runtime gates.

Planned gates:
  dynamic runtime route only after artifacts and graph support exist.

Boundary:
  model class reports are not model support.

### TRACK.TENSOR - Tensor Collections

Owns:
  embedding, norm, attention, MLP/MoE, output-head, tokenizer, and runtime
  tensor role coverage.

Current state:
  partial/report-only.

Complete:
  DeepSeek selected tensor reports, Qwen tensor collection inventory, Gemma
  tensor collection inventory, source role-map coverage reports.

Active / Next:
  tensor role facts feed quant support and artifact emission.

Planned gates:
  required tensor coverage report for every supported generation family.

Boundary:
  tensor collection coverage is not materialization, graph execution, or
  generation.

### TRACK.RESIDENCY - Residency

Owns:
  resident/staged/hybrid placement, memory pressure, movement, cleanup.

Current state:
  planned/report-only.

Complete:
  selected materialization and placement planning surfaces.

Active / Next:
  wait for generation-capable artifacts and runtime tensor set.

Planned gates:
  DeepSeek/Qwen/Gemma residency acceptance, backend movement proof, cleanup.

Boundary:
  residency plans are not runtime support.

### TRACK.BACKEND - Backend Capability

Owns:
  CPU/CUDA capability, bounded kernels, transfer, allocation, future Metal/ROCm
  lanes.

Current state:
  partial.

Complete:
  CPU baseline, CUDA probe/movement, CUDA bounded primitive hardening.

Active / Next:
  backend readiness follows graph and qtype compute requirements.

Planned gates:
  qtype-aware compute/refusal matrix, backend cleanup and fallback policy.

Boundary:
  CUDA bounded primitive-hardening only; no full CUDA runtime generation claim.

### TRACK.GRAPH - Graph Core

Owns:
  graph primitives, selected graph slices, real transformer graph path.

Current state:
  partial.

Complete:
  RoPE, attention, projection/matmul, MLP/routed-expert primitives; controlled
  block and layer fixtures; selected DeepSeek slices.

Active / Next:
  wait for generation-capable artifacts and role/qtype readiness.

Planned gates:
  real transformer graph over supported family artifacts.

Boundary:
  graph proof is not prefill, decode, logits, sampling, or generation.

### TRACK.PREFILL - Prefill

Owns:
  real transformer prefill, chunking, prefill state ownership, KV write
  boundary.

Current state:
  diagnostic-runtime only.

Complete:
  segment-summary, layer-backed, chunked diagnostic prefill.

Active / Next:
  wait for graph and KV gates.

Planned gates:
  real prefill over supported artifacts.

Boundary:
  diagnostic prefill is not real transformer prefill.

### TRACK.KV - KV Runtime

Owns:
  KV shape, allocation, capacity, write/read indexing, lifecycle, cleanup.

Current state:
  diagnostic/report-only.

Complete:
  bounded diagnostic KV store and KV-backed diagnostic prefill binding.

Active / Next:
  wait for attention-backed prefill and decode requirements.

Planned gates:
  real K/V writes during prefill and reads during decode.

Boundary:
  diagnostic KV is not attention-backed KV.

### TRACK.DECODE - Decode

Owns:
  one-step and repeated decode over real runtime state.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic decode.

Active / Next:
  wait for real prefill and KV read/write.

Planned gates:
  one real decode step, repeated decode lifecycle.

Boundary:
  decode is not generation without logits, sampling, append, and stop policy.

### TRACK.LOGITS - Output Head And Logits

Owns:
  final hidden state, final norm, output-head projection, logits buffer.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic logits.

Active / Next:
  wait for output-head mapping, residency, graph, and decode state.

Planned gates:
  real output-head logits for supported families.

Boundary:
  diagnostic logits are not real model logits.

### TRACK.SAMPLING - Sampling

Owns:
  greedy and stochastic sampling over real logits.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic greedy sampler.

Active / Next:
  wait for real logits.

Planned gates:
  greedy over real logits, then stochastic strategies and reproducibility.

Boundary:
  sampling over diagnostic logits is not generation quality.

### TRACK.TOKENIZER - Tokenizer And Stop Policy

Owns:
  tokenizer metadata, token input/output contract, EOS and stop-token policy.

Current state:
  partial/planned.

Complete:
  tokenizer and prompt diagnostics.

Active / Next:
  wait for artifact metadata and generation path.

Planned gates:
  tokenizer-backed stop behavior or explicit token-ID-only v0.1.0 boundary.

Boundary:
  tokenizer diagnostics are not text generation support.

### TRACK.GENERATION - Runtime Generation

Owns:
  prefill -> decode -> logits -> sample -> append -> stop -> cleanup loop.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic generation with trace, cancellation, append, stop, and
  cleanup.

Active / Next:
  wait for real prefill, KV, decode, logits, sampling, tokenizer boundary.

Planned gates:
  runtime generation for every supported v0.1.0 family.

Boundary:
  diagnostic generation is not full model generation.

### TRACK.OPERATOR - Operator CLI

Owns:
  top-level CLI grammar, command-family layout, compact normal output, audit
  output, porcelain/plumbing boundaries, operator runbook proof.

Current state:
  implemented/partial.

Complete:
  V010.CLI.17, V010.CLI.18, V010.CLI.19, V010.CLI.20, V010.CLI.21,
  V010.CLI.22, V010.CLI.23, V010.CLI.24, CLI.ARCH.AUDIT.0,
  V010.CLI.25, V010.CLI.26, SPINE.CLI.REBASE.1.

Active / Next:
  not Active Next. V010.CLI.27 remains planned.

Planned gates:
  V010.CLI.27 - base status and refusal grammar; V010.CLI.MODELS.4 models
  artifacts porcelain; V010.CLI.RUNTIME.0 runtime diagnostic command grammar.

Boundary:
  CLI cannot claim lower runtime behavior. table = renderer layout, not a
  long-term user-selected mode.

Historical anchor:
  CLI.MODELS.3 - Model selection in canonical REPL.

### TRACK.SERVE - Serving

Owns:
  daemon state, provider endpoints, streaming, observability.

Current state:
  planned.

Complete:
  yvexd status shell.

Active / Next:
  wait for runtime generation.

Planned gates:
  runtime-backed generation endpoint, streaming, cancellation, compatibility.

Boundary:
  serving status is not provider generation.

### TRACK.EVAL - Evaluation

Owns:
  fixture, graph, runtime, generation, tokenizer, failure-path, and capability
  evaluation.

Current state:
  planned.

Complete:
  tests for implemented lower-level rows only.

Active / Next:
  wait for runtime generation path.

Planned gates:
  eval smoke and family-specific correctness evidence.

Boundary:
  fixture correctness is not model capability evaluation.

### TRACK.BENCH - Benchmark

Owns:
  reproducible benchmark harness, machine/artifact/qtype/context metadata,
  timing and memory reports.

Current state:
  planned.

Complete:
  no measured runtime benchmark.

Active / Next:
  wait for runtime generation and eval smoke.

Planned gates:
  prefill, decode, generation, memory, and server benchmark records.

Boundary:
  benchmark_status: not-measured until measured command evidence exists.

### TRACK.RELEASE - Release

Owns:
  final validation transcript, artifact guardrail, claim audit, docs audit, tag
  readiness.

Current state:
  planned.

Complete:
  guardrails and docs surface tests exist.

Active / Next:
  wait for artifact, runtime, eval, benchmark, and operator gates.

Planned gates:
  v0.1.0 release transcript and tag readiness report.

Boundary:
  release rows cannot create runtime capability.

### TRACK.POST010 - Post-v0.1.0

Owns:
  portability, serving hardening, speculative acceleration, broader eval,
  public benchmarks, additional model families.

Current state:
  post-v0.1.0.

Complete:
  doctrine only.

Active / Next:
  not active.

Planned gates:
  selected only after v0.1.0 baseline path exists.

Boundary:
  post-v0.1.0 lanes cannot close current v0.1.0.

## 6. Track Gates

| Gate | Track | Required proof |
| --- | --- | --- |
| GATE.SCOPE | TRACK.SCOPE | family set, target, non-goals, unsupported boundaries |
| GATE.SOURCE | TRACK.SOURCE | source manifests, inventories, sidecars, no payload loading unless owned |
| GATE.MAP | TRACK.MAP | required tensor roles, output head, tokenizer, missing-role blockers |
| GATE.QUANT | TRACK.QUANT | dtype/qtype support by role and compute/refusal matrix |
| GATE.ARTIFACT | TRACK.ARTIFACT | YVEX-produced generation-capable artifact per supported family |
| GATE.INTEGRITY | TRACK.INTEGRITY | identity, digest, range, shape/dtype, corruption refusal |
| GATE.MODEL | TRACK.MODEL | class/routing requirements for supported families |
| GATE.TENSOR | TRACK.TENSOR | full required tensor coverage |
| GATE.RESIDENCY | TRACK.RESIDENCY | placement, movement, memory pressure, cleanup proof |
| GATE.BACKEND | TRACK.BACKEND | CPU/CUDA support/refusal matrix for required ops and qtypes |
| GATE.GRAPH | TRACK.GRAPH | real transformer graph path over supported artifacts |
| GATE.PREFILL | TRACK.PREFILL | real prefill and state ownership |
| GATE.KV | TRACK.KV | real K/V writes and decode reads |
| GATE.DECODE | TRACK.DECODE | one real decode step and repeat lifecycle |
| GATE.LOGITS | TRACK.LOGITS | real output-head logits |
| GATE.SAMPLING | TRACK.SAMPLING | token selection over real logits |
| GATE.TOKENIZER | TRACK.TOKENIZER | tokenizer/stop boundary |
| GATE.GEN | TRACK.GENERATION | runtime generation for DeepSeek, Qwen, and Gemma |
| GATE.OPERATOR | TRACK.OPERATOR | normal/audit command proof and refusal wording |
| GATE.EVAL | TRACK.EVAL | smoke and failure-path eval over runtime generation |
| GATE.BENCH | TRACK.BENCH | measured, reproducible benchmark metadata |
| GATE.RELEASE | TRACK.RELEASE | final transcript, claim audit, docs audit, tag readiness |

Critical path:

```text
scope
-> source
-> map
-> quant
-> generation-capable artifacts
-> integrity
-> model/tensor/residency/backend
-> graph
-> prefill
-> KV
-> decode
-> logits
-> sampling
-> tokenizer/stop boundary
-> runtime generation
-> operator proof
-> eval
-> benchmark
-> release
```

## 7. Row Contract Families

Every row must name one primary owner, one owned boundary, command/API proof,
tests, failure paths, cleanup/lifecycle behavior where relevant, docs update
where relevant, and forbidden claims.

### Universal Row Contract

Required:

```text
row id
primary owner
stage target
owned boundary
does not own
allowed inputs
disallowed inputs
implementation surface
command/API proof
normal/table/audit behavior when CLI-visible
failure paths
cleanup/lifecycle behavior
tests
validation commands
forbidden claims
docs update if implementation changed public/internal truth
next-row decision
```

A row may not silently promote itself to a higher stage.

### Contract A - Docs And Doctrine

Stage target:
  doctrine-only or docs/control.

Required proof:
  docs surface tests, claim guard, no runtime implementation claim.

Failure paths:
  stale claim, internal ID leak into public docs, roadmap expansion without
  implementation need.

Required boundary:
  docs do not create runtime capability.

Forbidden claims:
  generation, eval, benchmark, release readiness.

### Contract B - Report-Only Rows

Stage target:
  report-only.

Required proof:
  stable command report, normal/audit output where applicable, success and
  refusal tests.

Failure paths:
  missing source, unsupported family, malformed metadata, unknown target,
  unsupported backend where relevant.

Required boundary:
  runtime_claim: unsupported; generation: unsupported-full-model;
  benchmark_status: not-measured.

Forbidden claims:
  runtime support, model support, source verification, generation readiness.

### Contract C - Source And Artifact Rows

Stage target:
  source-intake, fixture-proof, selected-slice-proof, or artifact-ready.

Required proof:
  source/header inventory or artifact emission/identity/integrity command proof,
  sidecar behavior, no payload loading unless owned, artifact guardrail.

Failure paths:
  missing file, malformed safetensors header, short read, byte-range overflow,
  unsupported qtype, identity drift, cleanup failure.

Required boundary:
  source/artifact state is not runtime execution.

Forbidden claims:
  external GGUF support, source verification beyond implemented checks,
  runtime generation.

### Contract D - Graph And Runtime Rows

Stage target:
  fixture-proof, selected-slice-proof, diagnostic-runtime, or full-runtime.

Required proof:
  reference comparison, shape/dtype validation, backend support/refusal,
  cleanup/lifecycle behavior, command tests.

Failure paths:
  missing tensor, invalid shape, dtype/qtype mismatch, allocation failure,
  reference mismatch, context overflow, KV capacity failure, cleanup failure.

Required boundary:
  primitive, fixture, selected-slice, diagnostic, and full-runtime stages must
  be explicitly distinguished.

Forbidden claims:
  generation readiness before prefill/KV/decode/logits/sampling/append/stop
  path exists.

### Contract E - CLI And Operator Rows

Stage target:
  operator-output or operator-command.

Required proof:
  command parser tests, normal output, audit output, parser failure, refusal
  wording, exit code behavior.

Failure paths:
  unknown command, unsupported flag, unsupported output mode, unknown target,
  lower-level refusal propagation.

Required boundary:
  CLI composes lower behavior only.

Forbidden claims:
  CLI support cannot imply runtime, generation, eval, or benchmark support.

### Contract F - Eval, Benchmark, And Release Rows

Stage target:
  eval-ready, benchmark-ready, or release-ready.

Required proof:
  eval cases over implemented runtime path; benchmark metadata with model,
  artifact, qtype, context, backend, machine, command, run count; release
  transcript.

Failure paths:
  missing reproducibility metadata, unsupported runtime path, failed guardrail,
  stale claim, unclean artifact state.

Required boundary:
  eval follows implementation; benchmark follows implementation; release follows
  gates.

Forbidden claims:
  benchmark evidence without measured command proof; release readiness before
  all gates pass.

## 8. Release Gates

The final v0.1.0 release candidate requires one transcript containing:

1. clean checkout status;
2. build and test validation;
3. artifact guardrail;
4. supported-family source proof for DeepSeek, Qwen, and Gemma;
5. generation-capable artifact proof for DeepSeek, Qwen, and Gemma;
6. runtime generation proof for DeepSeek, Qwen, and Gemma;
7. eval proof over the implemented runtime generation path;
8. benchmark proof with reproducibility metadata;
9. claim audit;
10. docs audit;
11. tag readiness report.

No v0.1.0 tag is valid before every release gate passes.

## 9. Post-v0.1.0

Post-v0.1.0 lanes:

| Lane | Boundary |
| --- | --- |
| Qwen/Metal backend portability | post-v0.1.0 backend portability, not current release closure |
| GLM runtime promotion if selected | requires explicit target promotion before runtime work |
| ROCm/Strix Halo | future backend lane, no current support claim |
| speculative acceleration | starts after baseline generation and benchmark harness |
| production serving hardening | follows runtime-backed generation |
| broader eval/capability suite | follows implemented generation path |
| public benchmark expansion | follows benchmark harness and measured data |
| additional model families | require source/map/quant/artifact/runtime gates |

## 10. Appendix: Canonical Vocabulary

| Term | Meaning |
| --- | --- |
| source tensor | tensor metadata or payload from official upstream source files |
| YVEX-produced artifact | artifact emitted by YVEX tooling with YVEX-owned metadata and identity |
| external reference artifact | third-party artifact used only for comparison, never support proof |
| selected-runtime-slice | bounded subset of a real artifact used for parser/materialization/graph proof |
| supported-generation-family | DeepSeek, Qwen, or Gemma for v0.1.0 |
| generation-capable artifact | YVEX-produced artifact with required roles, qtypes, metadata, identity, and integrity for generation work |
| runtime generation | actual prefill/KV/decode/logits/sampling/append/stop/cleanup path over model state |
| report-only | command-visible facts without execution |
| diagnostic-runtime | runtime control flow over bounded diagnostic state |
| fixture-proof | synthetic controlled proof with reference checks |
| selected-slice-proof | bounded proof over selected real artifact tensors |
| full-runtime | real transformer path over a supported generation artifact |
| eval-ready | implemented runtime path has correctness/failure evaluation |
| benchmark-ready | implemented runtime path has measured benchmark metadata |
| release-ready | all target, artifact, runtime, eval, benchmark, docs, CI, and claim gates pass |
