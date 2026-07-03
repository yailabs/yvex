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
  V010.CLI.17 - normal output contract and layout baseline

Current release target:
  v0.1.0 - first honest full-runtime path

Primary pressure targets:
  DeepSeek selected-runtime-slice
  GLM huge source/storage
  Qwen/Metal future portability

Main v0.1.0 blocker:
  normal operator output is not separated from diagnostic/audit output yet
```

| Field | Current value |
| --- | --- |
| Project type | inference engine |
| Primary interface | CLI |
| Primary language | C |
| Primary platform | Linux + CUDA |
| Current highest runtime stage | bounded diagnostic generation |
| Real transformer prefill | unsupported |
| Real KV writes/reads | unsupported |
| Real decode | unsupported |
| Real output-head logits | unsupported |
| Real vocabulary sampling | unsupported |
| Full model generation | unsupported |
| DeepSeek generation | unsupported |
| Eval/benchmark | unsupported / not measured |
| Active next | V010.CLI.17 |

## 1. Spine Nomenclature

Tracks are the clean forward map. Blocks are ownership. Rows are delivery units.
The ledger is history. SPINE rows are meta-control. V010 rows are v0.1.0
forward work.

| Term | Meaning | Example | Primary section | Runtime capability? |
| --- | --- | --- | --- | --- |
| block | Canonical ownership domain. | BLOCK 7 Runtime State. | Doctrine Appendix / Ownership Blocks. | No by itself. |
| track | Forward architecture lane. | TRACK.RUNTIME.KV. | Forward Track Matrix. | Only through completed implementation rows. |
| row | Smallest delivery boundary. | MOE.CLASS.0. | Ledger or Track Detail. | Yes if implementation row; no if docs/meta/report-only. |
| wave | Work package sent to Codex/AI. | SPINE.REDESIGN.0. | Delivery reports / ledger. | Depends on row type. |
| ledger | Historical delivery record. | Historical Delivery Ledger. | Historical Delivery Ledger. | No by itself. |
| track matrix | Primary forward architecture table. | Forward Track Matrix. | Forward Track Matrix. | No by itself. |
| gate | Required release or evidence condition. | GATE.KV. | v0.1.0 Critical Path. | No by itself. |
| claim | Capability statement that must be evidence-backed. | bounded diagnostic generation exists. | Current Implementation State. | Only if backed by implementation evidence. |
| boundary | Explicit non-claim. | not full model generation. | Boundary Registry. | No; it prevents overclaim. |
| meta-spine row | Docs/control row. | SPINE.AUDIT.0. | Meta-Spine Waves. | No. |
| implementation row | Code behavior row. | GEN.APPEND.0. | Historical Delivery Ledger. | Yes. |
| report row | Command-visible report row. | CONTEXT.CLASS.0. | Historical Delivery Ledger / Track Detail. | Inspection only. |

## 2. Runtime Architecture

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
-> real prefill
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
| model-family mapping | family adapter facts and blockers | report-only | partial | `yvex fullmodel family-runtime` | dense/output/tokenizer class facts |
| model class | dense/MoE/source-only/selected-slice routing | report-only | partial | fullmodel descriptor reports and `yvex moe report` | final runtime route |
| tensor collections | embedding/norm/attention/MLP/MoE/output/tokenizer roles | report-only | partial | fullmodel report, descriptor, and `yvex tensor-collection report` | v0.1.0 full-runtime tensor coverage |
| storage/residency | placement, cache, staging, storage-stream planning | report-only | partial | materialization plans | staged residency proof |
| backend capability | CPU/CUDA/future backend capability | partial | partial | CPU/CUDA checks and parity | capability matrix hardening |
| graph primitives | standalone op proof | fixture-proof | yes | `yvex graph --execute-op` | real transformer graph |
| real transformer graph | execute real model block/layers | planned | no | none | QKV/O, attention, MLP/MoE over target tensors |
| context planning | requested/active context, chunking, overflow, decode position | report-only | yes | `yvex context report` | connect to full-runtime prefill |
| real prefill | build runtime state from real transformer path | unsupported | no | none | attention-backed KV writes |
| KV cache | K/V runtime state | diagnostic/report-only | partial | `yvex kv report`, diagnostic KV | real K/V writes and decode reads |
| decode | advance runtime one token | diagnostic-runtime | partial | `yvex decode` | real decode over real KV |
| logits | output token scores | diagnostic-runtime | partial | `yvex logits` | real output-head logits |
| sampling | choose next token | diagnostic-runtime | partial | `yvex sample` | real vocab sampling |
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
| MoE | embedding -> norm -> attention -> router -> expert selection -> dispatch -> accumulation -> output head -> logits -> sampling -> generation | report-only complete / runtime planned | v0.1.0 target decision / full-runtime artifact path |
| source-only | source/intake/storage/model-class only | GLM pressure lane | source inventory / storage stream |
| selected-runtime-slice | parser/materialization/partial graph/diagnostic runtime only | DeepSeek selected artifacts | does not close full runtime |
| Metal/Qwen | backend portability lane | report-only pressure lane | Qwen source pressure report |

## 3. Current Implementation State

This section is the single front-door view of current capability. Detailed
historical bullets are preserved below as an archive, but this table is the
operator-readable state.

### 3.1 Capability Summary Table

| Capability group | Stage | Implemented? | Main proof | Boundary |
| --- | --- | --- | --- | --- |
| artifact/integrity | implemented | yes | inspect/integrity/materialize tests | not runtime |
| selected graph | selected-slice | yes | graph selected/segment commands | not full transformer |
| graph primitives | fixture-proof | yes | graph op/block/layers commands | not real model runtime |
| prefill | diagnostic-runtime | partial | `yvex prefill` | not real transformer prefill |
| KV | diagnostic/report-only | partial | `yvex kv report`, diagnostic KV | not real attention KV |
| decode | diagnostic-runtime | partial | `yvex decode` | not real decode |
| logits | diagnostic-runtime | partial | `yvex logits` | not output-head logits |
| sampling | diagnostic-runtime | partial | `yvex sample` | not real vocabulary sampling |
| generation | diagnostic-runtime | partial | `yvex generate` | not full model generation |
| fullmodel reports | report-only/selected-slice proof | yes | `yvex fullmodel ...` | not execution |
| attention/context/KV class | report-only | yes | `yvex attention/kv/context report` | not runtime execution |
| MoE class | report-only | yes | `yvex moe report` | not router/expert execution |
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
family-runtime facts, attention class facts, KV class facts, context class
facts, MoE class facts, v0.1.0 full-runtime candidate facts, v0.1.0 dense
candidate facts, and Qwen/Metal pressure target facts. Report-only
capability increases inspection but does not execute runtime semantics.

Current report-only target capability:

```text
v0.1.0 full-runtime candidate report
v0.1.0 dense candidate report
Qwen/Metal pressure target report
```

### 3.6 Implemented Operator Capability

YVEX exposes operator-local path configuration, model target path reporting,
target decision and candidate reporting,
selected artifact prepare/check presets, graph check presets, bounded generate
help/runbook coverage, and clear unsupported boundaries.

### 3.7 Implemented Docs/Meta Capability

The spine records source-tensor doctrine, canonical blocks, v0.1.0 doctrine,
generation/speculative targets, audit/reconciliation rows, Qwen/Metal pressure
lane, CLI output UX doctrine and diagnostic-demotion contract, navigation, and
this track-based redesign. These rows are project-control work, not runtime
capability.

### 3.8 Unsupported Boundary Registry

The unsupported registry lives in `## 12. Boundary Registry`. The front summary
is: no full-runtime generation, no DeepSeek generation, no provider generation,
no evaluation, no benchmark, and no throughput claim.

### 3.9 Current Target Classes

| Target class | Current target | Current state | Boundary |
| --- | --- | --- | --- |
| selected-runtime-slice | DeepSeek selected embed / embed+RMSNorm | implemented selected-slice proofs | not full runtime |
| huge source/storage | GLM-5.2 official safetensors | source/storage pressure lane | no GLM execution |
| future portability | Qwen/Metal | report-only pressure lane | no Metal/Qwen runtime |
| full-runtime model | v0.1.0 selected target | blocked-no-source-backed-candidate | V010.TARGET.7 records Qwen/Metal pressure only; V010.SOURCE.9 must ground source facts |

## CLI Output UX Doctrine

YVEX has two output planes.

Normal operator output:
  concise, inline-first, result-first, metric-aware, color-aware when TTY-safe,
  and suitable for humans running commands directly.

Diagnostic/research output:
  explicit, verbose, boundary-heavy, traceable, and suitable for debugging,
  audits, row promotion, and research inspection.

The current verbose diagnostic/report output is not final operator UX.

Repeated unsupported fields, readiness fields, blocker lists, tensor tables,
claim guards, lifecycle internals, and cleanup internals must be demoted from
normal output into explicit diagnostic, trace, audit, or JSON modes.

Normal CLI output should show only:
  result
  compact status
  essential model/backend identity
  measured metrics where real
  top blocker or next action when relevant
  one boundary line only when needed

Current long outputs are acceptable as diagnostic/audit evidence for parser,
artifact, tensor, report-only, selected-slice, diagnostic-runtime, boundary,
cleanup, and claim-guard rows. They are not acceptable as the final operator
experience.

Future CLI output implementation work should use `V010.CLI.*` rows. Do not
create new loose `CLI.UX.*` rows unless preserving historical continuity.
`CLI.UX.*`, `MODEL.LIFECYCLE.*`, and `OPERATOR.FLOW.*` remain historical or
superseded planning surfaces. New output behavior is canonical under
`V010.CLI.*`, `V010.DOCTOR.*`, `V010.TRACE.*`, and `V010.CI.*`.

## Inline-First CLI Rule

Default human output should prefer compact inline fields and short sections.

Tables are appropriate when the command naturally returns multiple rows:
  models list
  target candidates
  tensor summaries
  benchmark result sets
  eval cases
  blocker groups

Tables must not become audit dumps.

Color is allowed only for semantic terminal output:
  pass
  warn
  fail
  unsupported
  selected/current
  measured/target

Color must be disabled for:
  --no-color
  NO_COLOR
  non-TTY output
  JSON/JSONL output

Normal output must not require users to scroll through hundreds of lines to find
the result, next action, or blocker.

## CLI Print Ownership Policy

Long hardcoded print chains are not the desired final CLI architecture.

A command may temporarily print diagnostic fields directly while a row is being
proved, but operator-facing commands must converge toward explicit output
contracts and reusable rendering helpers.

Repeated boundary/status fields should be rendered by shared helpers or
domain-owned render functions, not copied manually across many command bodies.

Future CLI output waves should reduce:
  duplicated status lines
  repeated unsupported fields
  repeated readiness fields
  repeated cleanup fields
  repeated benchmark disclaimers
  hardcoded wall-output blocks

This policy does not require a source split. It does not create a new CLI
command forest. It does not move domain behavior into `yvex_cli.c`.

## Output Mode Taxonomy

normal:
  default human output. Short, inline-first, result-first, no repeated audit fields.

summary:
  compact report output for target/source/artifact/runtime checks.

table:
  human table output for multi-row reports.

verbose:
  expanded human-readable diagnostics.

trace:
  step-level runtime diagnostics.

audit:
  row-promotion evidence with all required boundary, readiness, lifecycle,
  cleanup, and failure fields.

json:
  structured machine-readable summary output.

jsonl:
  event or trace stream.

raw:
  future raw/tensor dump mode; explicitly gated and refused unless implemented.

A command may start life with verbose diagnostic output, but any command that
becomes operator-facing must eventually define normal output.

## Normal Output Contract

For future real generation:

```text
yvex generate --model TARGET --prompt TEXT
```

normal output should be shaped as:

```text
<generated text>

stats:
  model:
  backend:
  qtype:
  prompt_tokens:
  generated_tokens:
  ttft_ms:
  decode_tok_s:
  stop_reason:
```

For current diagnostic generation normal mode:

```text
status: diagnostic-generation
model: <target>
backend: <backend>
tokens: <prompt_tokens> -> <generated_diagnostic_tokens>
stop: <stop_reason>
boundary: full-model generation unsupported
hint: use --audit or --trace-level full for diagnostic internals
```

Do not print these by default in normal mode:

```text
generation_ready:
full_model_generation:
real_deepseek_generation:
prefill_invoked:
decode_steps:
logits_steps:
sample_steps:
cleanup_attempted:
cleanup_status:
every repeated unsupported boundary
every repeated readiness false field
```

Those belong to:

```text
--audit
--trace
--verbose
--json
```

## Report Output Contract

Default target/source/report output should be compact.

Default report shape:

```text
report:
status:
decision:
selected:
eligible:
top_blocker:
next:
boundary:
```

Long output requires explicit flags:

```text
--include-candidates
--include-blockers
--include-next
--include-requirements
--include-paths
--include-tensors
--verbose
--audit
--json
```

The current V010 target/report output may remain verbose until the CLI layout
wave changes code, but the final operator report UX must be compact by default.

## Metric UX Contract

Normal output may show only real measured values or explicit target values.

Allowed measured labels:

```text
ttft_ms
prefill_tok_s
decode_tok_s
generation_tok_s
latency_ms
prompt_tokens
generated_tokens
context_length
vram_peak_mb
ram_peak_mb
stop_reason
```

Target values must be labeled as targets:

```text
target_decode_tok_s:
target_generation_tok_s:
```

Measured values must be labeled as measured:

```text
measured_decode_tok_s:
measured_generation_tok_s:
```

Until the benchmark harness exists:

```text
benchmark_status: not-measured
```

Do not present target metrics as measured metrics.

Do not print success claims such as target throughput success,
measured-benchmark success, generation-readiness, or inference-readiness unless the
benchmark/runtime path proves them.

## Reserved CLI Flags

Planned flags:

```text
--output normal|summary|table|verbose|json
--trace-level none|tokens|steps|kv|logits|sampling|full
--audit
--quiet
--no-color
--metrics
--explain-boundary
```

These flags are conceptual unless parser, behavior, output contract, tests,
failure paths, cleanup behavior, and docs/spine row closure exist.

## 4. Forward Track Matrix

This is the main operational map for future work. Tracks are semantic planning
lanes; rows are the delivery units that complete track work.

| Track ID | Track name | Owns | Current status | Implemented evidence | Next gap | Active / Later |
| --- | --- | --- | --- | --- | --- | --- |
| TRACK.TARGET | Target selection and pressure objects | target classes and release target decision | pressure-lane-reported | target registry, path reports, `yvex model-target decision`, `yvex model-target candidate`, `yvex model-target dense-candidate`, and `yvex model-target qwen-metal` | Qwen source pressure report | active |
| TRACK.SOURCE | Source intake | official sources, manifests, native inventories | partial | source manifest/native inventory | Qwen source pressure report and multi-family source profile | active |
| TRACK.ARTIFACT | Artifact production | YVEX-produced GGUF and conversion plan | selected-slice | controlled/selected GGUF emission | full-runtime artifact production | later |
| TRACK.INTEGRITY | Artifact identity and gates | digest/ranges/corruption/materialization gates | implemented | integrity harness and reports | full-runtime gate coverage | active |
| TRACK.MODEL | Model class and runtime routing | dense/MoE/source-only/selected-slice class reports | partial/report-only | family-runtime, attention, KV, context, MoE reports | final runtime route and dense/output/tokenizer class gaps | active |
| TRACK.TENSOR | Tensor collections | embedding, norm, attention, MLP/MoE, output, tokenizer roles | partial/report-only | fullmodel descriptor/report and MoE tensor-collection report | selected artifacts lack full MoE tensor coverage | active |
| TRACK.STORAGE | Storage stream | shard index, cold/warm reads, byte ranges, cache policy | planned | doctrine/plans | shard and read probes | later |
| TRACK.RESIDENCY | Residency | resident/staged/hybrid placement and movement | report-only | placement plans | staged residency proof | later |
| TRACK.BACKEND | Backend capability | CPU/CUDA/future Metal/ROCm operations and memory reports | partial | CPU/CUDA probes and parity | capability matrix | active |
| TRACK.GRAPH | Graph core | primitives, block/layer fixtures, model-backed graph path | partial | graph primitives and selected-slice graph | real transformer graph | later |
| TRACK.ATTENTION | Attention runtime | Q/K/V/O, masks, position, attention execution | report-only | attention report and primitive proof | target tensor attention | later |
| TRACK.MOE | MoE runtime path | router, experts, activation, dispatch, accumulation | report-only complete / runtime planned | `yvex moe report`, `yvex tensor-collection report`; routed expert primitive only | target/full artifact decision before activation | active |
| TRACK.CONTEXT | Context planning | requested/active context, chunking, overflow, decode policy | report-only | context report | connect to full-runtime prefill | active |
| TRACK.PREFILL | Real prefill | real transformer prefill and KV writes | diagnostic-runtime only | segment/chunk diagnostic prefill | real attention-backed prefill | later |
| TRACK.KV | KV runtime | KV layout, capacity, writes/reads, attention/decode interaction | diagnostic/report-only | diagnostic KV and `yvex kv report` | real KV write/read | later |
| TRACK.DECODE | Decode runtime | decode state advance and repeated decode | diagnostic-runtime | `yvex decode` | real decode over real KV | later |
| TRACK.LOGITS | Output head and logits | final hidden state and vocab projection | diagnostic-runtime | bounded diagnostic logits | real output-head projection | later |
| TRACK.SAMPLING | Sampling runtime | greedy/stochastic/top-k/top-p/min-p/typical | diagnostic-runtime | bounded greedy sampler | real vocab sampling | later |
| TRACK.TOKENIZER | Tokenizer and stop policy | detokenization, EOS, stop tokens, prompt boundary | partial/planned | tokenizer diagnostics | tokenizer-backed stop behavior | later |
| TRACK.GENERATION | Generation runtime | decode/logits/sample/append/stop/cleanup | diagnostic-runtime | `yvex generate` | full-runtime generation | later |
| TRACK.RUNTIME | Runtime lifecycle and trace | lifecycle, cancellation, trace, failure preservation | diagnostic-runtime | generate trace/cancel/cleanup | external interruption/runtime trace | later |
| TRACK.OPERATOR | Operator CLI | normal commands, presets, doctor, runbook | partial | paths/target/prepare/check/generate and CLI output UX doctrine | normal output contract and layout baseline | active |
| TRACK.SERVE | Serving | daemon state, provider endpoints, streaming | planned | status shell | runtime-backed generation endpoint | later |
| TRACK.EVAL | Evaluation | fixture/runtime/generation/capability eval | planned | tests only | eval over generation path | later |
| TRACK.BENCH | Benchmark/profile | reproducible performance measurement | planned | doctrine only | measured runtime harness | later |
| TRACK.SPEC | Speculative acceleration | draft/verify/accept/accounting/cost | post-v0.1.0 | doctrine/external references | baseline generation first | later |
| TRACK.DOCS | Documentation and public evidence | public/internal docs, diagrams, runbooks | partial | spine/runbook/docs | release-safe public evidence | active |
| TRACK.RELEASE | Release | version, transcript, gates, tag readiness | planned | v0.1.0 doctrine | final transcript | later |
| TRACK.CI | CI and guardrails | tests, artifact scans, docs surface, validation | partial | make/check scripts | release validation bundle | active |
| TRACK.POST010 | Post-v0.1.0 lanes | serving, benchmark tables, speculative, portability | post-v0.1.0 | doctrine only | v0.1.0 first | later |

## 5. Track Detail Sheets

Each sheet maps a semantic track to ownership, evidence, blockers, and V010 row
families. The detailed historical V010 row inventory is preserved after the
sheets.

### TRACK.TARGET — Target Decision

Owns:
  target classes, pressure objects, release target selection.

Does not own:
  runtime implementation or benchmark evidence.

Current status:
  pressure-lane-reported.

Current evidence:
  target registry, path reports, `yvex model-target decision --release v0.1.0`,
  `yvex model-target candidate --release v0.1.0`, and
  `yvex model-target dense-candidate --release v0.1.0`, and
  `yvex model-target qwen-metal --release v0.1.0`.

v0.1.0 rows:
  V010.TARGET.*.

Main blockers:
  Qwen/Metal is visible as a reduced-scale pressure lane, but source target,
  source manifest, native inventory, source config, model class, tensor map,
  artifact, Metal backend, residency, graph/runtime, eval, and benchmark facts
  are still missing.

Next possible row:
  V010.SOURCE.9.

Boundary:
  a target is not a capability claim.

### TRACK.SOURCE — Source Intake

Owns:
  official source tensors, manifests, native tensor inventory.

Does not own:
  runtime execution.

Current status:
  partial.

Current evidence:
  source-manifest and native-weights commands.

v0.1.0 rows:
  V010.SOURCE.*, OWI.*.

Main blockers:
  Qwen source pressure facts and multi-family source profile.

Next possible row:
  V010.SOURCE.9.

Boundary:
  source inventory is not model execution.

### TRACK.ARTIFACT — Artifact Production

Owns:
  conversion planning, quant policy, YVEX-produced GGUF.

Does not own:
  runtime graph execution.

Current status:
  selected-slice.

Current evidence:
  controlled and selected GGUF emission.

v0.1.0 rows:
  V010.ARTIFACT.*, OWI.*.

Main blockers:
  full-runtime artifact production.

Next possible row:
  V010.ARTIFACT.*.

Boundary:
  selected artifact production is not full runtime.

### TRACK.INTEGRITY — Artifact Identity and Integrity

Owns:
  digest, byte range, shape/dtype, corruption refusal, gates.

Does not own:
  model quality or supply-chain guarantees beyond implemented checks.

Current status:
  implemented.

Current evidence:
  integrity reports, corruption harness, materialization gate.

v0.1.0 rows:
  V010.INTEGRITY.*, ARTIFACT.INTEGRITY.*.

Main blockers:
  full-runtime gate coverage.

Next possible row:
  V010.INTEGRITY.*.

Boundary:
  integrity proof is not execution.

### TRACK.MODEL — Model Class and Runtime Routing

Owns:
  dense/MoE/source-only/selected-slice routing and blockers.

Does not own:
  expert activation or generation.

Current status:
  partial/report-only.

Current evidence:
  family-runtime, attention, KV, context, and MoE reports.

v0.1.0 rows:
  V010.CLASS.*, MOE.CLASS.0.

Main blockers:
  final runtime route, dense target decision, output-head class, tokenizer class.

Next possible row:
  V010.TENSOR.14-17 or V010.CLASS.12-13.

Boundary:
  model-class reports are not runtime execution.

### TRACK.TENSOR — Tensor Collections

Owns:
  embedding, norm, attention, MLP/MoE, output, tokenizer tensor roles.

Does not own:
  backend placement or graph execution.

Current status:
  partial/report-only.

Current evidence:
  fullmodel descriptor/report and `yvex tensor-collection report`.

v0.1.0 rows:
  V010.TENSOR.*.

Main blockers:
  selected-runtime-slice artifacts do not contain router/expert/shared-expert
  tensors; final runtime tensor map needs a v0.1.0 target/full artifact path.

Next possible row:
  V010.TARGET.2, then V010.MAP.2 / V010.FULLMODEL.6 after a candidate exists.

Boundary:
  tensor collection reports are not model support.

### TRACK.STORAGE — Storage Stream

Owns:
  shard index, reads, byte ranges, cache policy.

Does not own:
  generation semantics.

Current status:
  planned.

Current evidence:
  storage doctrine and placement plans.

v0.1.0 rows:
  V010.STORAGE.*, STORAGE.STREAM.*.

Main blockers:
  cold/warm read and shard index proof.

Next possible row:
  V010.STORAGE.*.

Boundary:
  storage streaming is not generation.

### TRACK.RESIDENCY — Residency

Owns:
  resident/staged/hybrid placement and movement.

Does not own:
  backend op implementation.

Current status:
  report-only.

Current evidence:
  materialization and placement plans.

v0.1.0 rows:
  V010.RESIDENCY.*, RESIDENCY.*.

Main blockers:
  staged residency proof.

Next possible row:
  V010.RESIDENCY.*.

Boundary:
  residency plans are not runtime execution.

### TRACK.BACKEND — Backend Capability

Owns:
  CPU/CUDA/future backend op and memory capability.

Does not own:
  model-family support.

Current status:
  partial.

Current evidence:
  CPU/CUDA probes, movement, parity tests.

v0.1.0 rows:
  V010.BACKEND.*, COMPUTE.BACKEND.*.

Main blockers:
  capability matrix and memory pressure reports.

Next possible row:
  V010.BACKEND.*.

Boundary:
  backend capability is not model support.

### TRACK.GRAPH — Graph Core

Owns:
  primitives, controlled block/layers, target tensor graph path.

Does not own:
  prefill/decode/logits/generation ownership.

Current status:
  partial.

Current evidence:
  graph op, block, layer, selected-slice commands.

v0.1.0 rows:
  V010.GRAPH.*.

Main blockers:
  real transformer graph.

Next possible row:
  V010.GRAPH.1-21.

Boundary:
  graph fixtures are not full transformer runtime.

### TRACK.ATTENTION — Attention Runtime

Owns:
  Q/K/V/O, RoPE/position, masks, attention execution.

Does not own:
  KV policy or generation loop.

Current status:
  report-only.

Current evidence:
  attention report and standalone primitive.

v0.1.0 rows:
  V010.ATTN.*.

Main blockers:
  target tensor Q/K/V/O path.

Next possible row:
  V010.ATTN.*.

Boundary:
  attention report/primitive is not full attention runtime.

### TRACK.MOE — MoE Runtime Path

Owns:
  MoE class facts, router facts, experts, top-k, activation, dispatch, accumulation.

Does not own:
  dense MLP runtime, output-head logits, generation loop, benchmark.

Current status:
  report-only complete / runtime planned.

Current evidence:
  `yvex moe report`, `yvex tensor-collection report`; routed expert-slice
  primitive; DeepSeek family-runtime MoE requirements.

v0.1.0 rows:
  V010.CLASS.3, V010.TENSOR.14-17, V010.RESIDENCY.14, V010.MOE.0-20, V010.PREFILL.7, V010.DECODE.6.

Main blockers:
  selected artifacts miss MoE router/expert/shared-expert tensors; expert
  storage/residency pressure; router/top-k; dispatch/accumulation.

Next possible row:
  V010.TARGET.2 before MoE activation/runtime rows.

Boundary:
  routed expert primitive is not full MoE support.

### TRACK.CONTEXT — Context Planning

Owns:
  requested/active context, chunking, overflow, decode position.

Does not own:
  attention/KV implementation.

Current status:
  report-only.

Current evidence:
  context report.

v0.1.0 rows:
  V010.CONTEXT.*.

Main blockers:
  connect planning to full-runtime prefill.

Next possible row:
  V010.CONTEXT.*.

Boundary:
  context report is not long-context support.

### TRACK.PREFILL — Real Prefill

Owns:
  real transformer prefill, KV writes, chunked full-runtime prefill.

Does not own:
  decode, logits, sampling, generation loop.

Current status:
  diagnostic-runtime only.

Current evidence:
  segment/layer/chunk diagnostic prefill.

v0.1.0 rows:
  V010.PREFILL.*.

Main blockers:
  real attention-backed prefill.

Next possible row:
  V010.PREFILL.3-9.

Boundary:
  diagnostic prefill is not real transformer prefill.

### TRACK.KV — KV Runtime

Owns:
  KV layout, capacity, writes/reads, attention/decode interaction.

Does not own:
  output-head logits.

Current status:
  diagnostic/report-only.

Current evidence:
  diagnostic KV binding and KV report.

v0.1.0 rows:
  V010.KV.*.

Main blockers:
  real KV write/read.

Next possible row:
  V010.KV.*.

Boundary:
  diagnostic KV is not attention-backed KV.

### TRACK.DECODE — Decode Runtime

Owns:
  one-step and repeated decode over runtime state.

Does not own:
  logits or sampling.

Current status:
  diagnostic-runtime.

Current evidence:
  bounded diagnostic decode.

v0.1.0 rows:
  V010.DECODE.*.

Main blockers:
  real decode over real KV.

Next possible row:
  V010.DECODE.3-8.

Boundary:
  diagnostic decode is not full-runtime decode.

### TRACK.LOGITS — Output Head and Logits

Owns:
  final hidden state and vocabulary projection.

Does not own:
  sampling policy or generation loop.

Current status:
  diagnostic-runtime.

Current evidence:
  bounded diagnostic logits.

v0.1.0 rows:
  V010.LOGITS.*.

Main blockers:
  real output-head projection.

Next possible row:
  V010.LOGITS.*.

Boundary:
  diagnostic logits are not real model logits.

### TRACK.SAMPLING — Sampling Runtime

Owns:
  greedy, stochastic, top-k/top-p/min-p/typical, seed policy.

Does not own:
  tokenizer stop or generation quality.

Current status:
  diagnostic-runtime.

Current evidence:
  bounded greedy sampler.

v0.1.0 rows:
  V010.SAMPLE.*.

Main blockers:
  greedy over real logits and broader strategies.

Next possible row:
  V010.SAMPLE.*.

Boundary:
  sampling over diagnostic logits is not generation quality.

### TRACK.TOKENIZER — Tokenizer and Stop Policy

Owns:
  detokenization, EOS, stop-token matching, prompt formatting.

Does not own:
  model graph execution.

Current status:
  partial/planned.

Current evidence:
  tokenizer and prompt diagnostics.

v0.1.0 rows:
  V010.TOKENIZER.*.

Main blockers:
  tokenizer-backed stop behavior.

Next possible row:
  V010.TOKENIZER.*.

Boundary:
  tokenizer diagnostics are not quality generation.

### TRACK.GENERATION — Generation Runtime

Owns:
  repeated decode -> logits -> sample -> append -> stop -> cleanup loop.

Does not own:
  graph execution, output-head implementation, serving, benchmark.

Current status:
  diagnostic-runtime only.

Current evidence:
  `yvex generate` bounded diagnostic loop with trace, stop, append, cancel, cleanup.

v0.1.0 rows:
  V010.GEN.0-19.

Main blockers:
  real prefill; real KV writes/reads; real decode; real output-head logits; real vocabulary sampling; tokenizer stop boundary.

Next possible row:
  none until decode/logits/sampling real path exists.

Boundary:
  diagnostic generation is not full model generation.

### TRACK.RUNTIME — Runtime Lifecycle and Trace

Owns:
  runtime lifecycle, cancellation, traces, failure preservation.

Does not own:
  provider/server cancellation.

Current status:
  diagnostic-runtime.

Current evidence:
  generate trace/cancel/cleanup records.

v0.1.0 rows:
  V010.RUNTIME.*, V010.TRACE.*.

Main blockers:
  external interruption and runtime trace hardening.

Next possible row:
  V010.RUNTIME.*.

Boundary:
  diagnostic lifecycle proof is not provider generation.

### TRACK.OPERATOR — Operator CLI

Owns:
  paths, targets, prepare/check, graph check, doctor, normal command transcript,
  and normal/diagnostic output layout.

Does not own:
  runtime semantics not implemented below it.

Current status:
  partial.

Current evidence:
  paths/model-target/models prepare/check, graph check, generate help, and CLI
  output UX doctrine in the spine.

v0.1.0 rows:
  V010.CLI.*, V010.OPERATOR.*.

Main blockers:
  normal operator output is not separated from diagnostic/audit output.

Next possible row:
  V010.CLI.17.

Boundary:
  operator presets compose lower behavior only.

### TRACK.SERVE — Serving

Owns:
  daemon state, provider endpoints, streaming, observability.

Does not own:
  generation semantics.

Current status:
  planned.

Current evidence:
  yvexd status shell.

v0.1.0 rows:
  V010.SERVE.*.

Main blockers:
  runtime-backed provider generation.

Next possible row:
  V010.SERVE.*.

Boundary:
  status shell is not serving generation.

### TRACK.EVAL — Evaluation

Owns:
  fixture/runtime/generation/capability eval.

Does not own:
  benchmark performance.

Current status:
  planned.

Current evidence:
  tests only.

v0.1.0 rows:
  V010.EVAL.*.

Main blockers:
  eval over full-runtime generation path.

Next possible row:
  V010.EVAL.*.

Boundary:
  eval waits for implemented runtime path.

### TRACK.BENCH — Benchmark/Profile

Owns:
  reproducible performance harness, machine/artifact metadata.

Does not own:
  capability implementation.

Current status:
  planned.

Current evidence:
  benchmark doctrine only.

v0.1.0 rows:
  V010.BENCH.*, V010.PROFILE.*.

Main blockers:
  measured runtime harness.

Next possible row:
  V010.BENCH.*.

Boundary:
  no benchmark without measured runtime path.

### TRACK.SPEC — Speculative Acceleration

Owns:
  draft/verify/accept/accounting/cost/speedup.

Does not own:
  baseline generation.

Current status:
  post-v0.1.0.

Current evidence:
  DSpark/speculative doctrine only.

v0.1.0 rows:
  V010.SPEC.*, POST010.SPEC.*.

Main blockers:
  baseline generation first.

Next possible row:
  POST010.SPEC.*.

Boundary:
  speculative rows cannot precede baseline generation.

### TRACK.DOCS — Documentation and Public Evidence

Owns:
  public/internal docs, diagrams, runbooks, claim hygiene.

Does not own:
  runtime behavior.

Current status:
  partial.

Current evidence:
  spine, contract, runbook, public docs boundaries.

v0.1.0 rows:
  V010.DOCS.*.

Main blockers:
  release-safe public evidence.

Next possible row:
  V010.DOCS.*.

Boundary:
  docs do not create runtime capability.

### TRACK.RELEASE — Release

Owns:
  version, transcript, gates, tag readiness.

Does not own:
  runtime implementation.

Current status:
  planned.

Current evidence:
  v0.1.0 doctrine and gates.

v0.1.0 rows:
  V010.RELEASE.*.

Main blockers:
  final validation transcript.

Next possible row:
  V010.RELEASE.*.

Boundary:
  release row cannot create runtime behavior.

### TRACK.CI — CI and Guardrails

Owns:
  tests, docs/source surface checks, artifact scans, validation bundle.

Does not own:
  feature behavior.

Current status:
  partial.

Current evidence:
  make check/smoke/CUDA and shell guardrails.

v0.1.0 rows:
  V010.CI.*.

Main blockers:
  release validation bundle.

Next possible row:
  V010.CI.*.

Boundary:
  CI proves boundaries already implemented.

### TRACK.POST010 — Post-v0.1.0 Lanes

Owns:
  serving compatibility, public benchmark tables, speculative acceleration, portability expansion.

Does not own:
  v0.1.0 minimum path.

Current status:
  post-v0.1.0.

Current evidence:
  doctrine only.

v0.1.0 rows:
  POST010.*.

Main blockers:
  v0.1.0 first.

Next possible row:
  POST010.*.

Boundary:
  post-v0.1.0 tracks cannot block current release unless selected.

### 5.31 Detailed V010 Row Inventory and Dependency Maps

The semantic TRACK.* sheets above are the planning surface. The following preserved row inventory keeps the complete historical V010 detail.

### 6. Dependency Map

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

Qwen-Metal portability track:
  Apple Silicon hardware profile
  -> Metal feasibility profile
  -> Qwen official source target
  -> Qwen source manifest
  -> Qwen native tensor inventory
  -> Qwen model-class profile
  -> Qwen tensor collections
  -> Qwen quantization policy
  -> Qwen YVEX-produced GGUF
  -> Qwen artifact identity
  -> Metal/unified-memory residency plan
  -> Metal allocation/movement
  -> Metal graph primitive parity
  -> Qwen selected slice materialization
  -> Qwen full model inventory
  -> Qwen materialization plan
  -> Qwen full runtime descriptor
  -> real Qwen prefill/KV/decode/logits/sampling
  -> Qwen Metal generation
  -> Qwen Metal eval/benchmark
```

### 6.0 Forward Runtime Track Matrix

This matrix is the preferred forward planning view. It groups the remaining
work by architecture lane instead of forcing the reader to scan the historical
ledger.

| Track | Name | Owns | Current status | Implemented evidence | Next gap | Blocks |
| --- | --- | --- | --- | --- | --- | --- |
| T00 | Target & Source | official source tensors, target class, source artifact class, source manifests | partial | source manifest and native inventory commands | multi-family source profile | artifact production and model-class routing |
| T01 | Artifact & Integrity | YVEX-produced artifact identity, digest, byte ranges, corruption refusal, registry drift | implemented | integrity reports, materialization gates, corruption tests | full-runtime artifact gate | graph/runtime trust boundary |
| T02 | Model Class & Tensor Collections | family, dense/MoE/source-only/selected-slice routing, tensor role groups | report-only | fullmodel descriptor, family-runtime, attention/KV/context/MoE reports | MoE tensor map and final runtime route | graph/runtime path selection |
| T03 | Storage & Residency | resident, host-staged, SSD-staged, SSD-streamed, hybrid placement | report-only | placement plans and pressure reports | staged residency proof | full-runtime materialization and prefill |
| T04 | Backend & Hardware | CPU/CUDA capability, future Metal/ROCm lanes, build/hardware profiles | partial | CPU/CUDA movement and graph parity subset | capability matrix and memory pressure reports | graph ops and runtime residency |
| T05 | Graph Core | primitives, controlled block, repeated layer fixture, selected graph segments | implemented | graph op, block, layer, selected-slice commands | target tensor graph integration | prefill, decode, logits |
| T06 | Attention Runtime | Q/K/V/O requirements, position, mask, head layout, attention execution | report-only | attention report and standalone attention primitive | target tensor Q/K/V/O path | KV and transformer prefill |
| T07 | MoE Runtime | router facts, expert classes, expert residency, activation, dispatch, accumulation | report-only complete / runtime planned | `yvex moe report`, `yvex tensor-collection report`; routed expert-slice primitive only | target/full artifact decision before activation | MoE prefill/decode/generation |
| T08 | Context & Prefill Planning | requested/active context, chunking, overflow, decode position policy | report-only | context report and chunked diagnostic prefill | connect planning to target tensor prefill | KV/decode correctness |
| T09 | Model-Backed Prefill | transformer prefill over target layer tensors | planned | diagnostic segment/layer/chunk prefill only | target tensor prefill entry | KV production and decode |
| T10 | KV Runtime | KV layout, capacity, ownership, append/read, attention interaction | diagnostic-runtime | minimal KV ownership and diagnostic binding | attention-backed KV production/consumption | decode runtime |
| T11 | Decode Runtime | one-step and repeated decode over runtime state | diagnostic-runtime | bounded diagnostic decode | decode over attention-backed KV | logits and generation |
| T12 | Output Head & Logits | output projection, logits buffer, logprob/top-k diagnostics | diagnostic-runtime | bounded diagnostic logits | output-head logits | sampling |
| T13 | Sampling Runtime | greedy, stochastic, top-k/top-p/min-p/typical, seeded behavior | diagnostic-runtime | bounded greedy sampler | vocabulary sampling strategy surface | generation |
| T14 | Tokenizer & Stop Policy | detokenization, EOS, stop-token matching, tokenizer quality boundary | planned | tokenizer/prompt diagnostics and unsupported stop policy | tokenizer-backed stop behavior | user-facing generation |
| T15 | Generation Runtime | decode -> logits -> sample -> append -> stop -> cleanup | diagnostic-runtime | bounded diagnostic `yvex generate` | full-model generation loop | CLI normal path, serving, eval |
| T16 | Runtime Lifecycle & Trace | lifecycle, cancellation, traces, cleanup, failure preservation | diagnostic-runtime | generate trace/cancel/cleanup records | external interruption and runtime tracing | operator reliability |
| T17 | Operator CLI | paths, targets, prepare, check, graph check, generate surface | partial | preset commands and runbook lanes | segment check and final normal path | v0.1.0 transcript |
| T18 | Serving | daemon state, provider endpoints, streaming, observability | planned | status shell only | runtime-backed provider generation | provider compatibility |
| T19 | Evaluation | fixture, runtime, generation, capability evaluation | planned | test runners only | generation smoke/eval path | benchmark confidence |
| T20 | Benchmark/Profile | reproducible performance harness and machine/artifact metadata | planned | doctrine and planned rows | measured runtime harness | public evidence |
| T21 | Speculative Acceleration | draft, verification, accepted-token accounting, routing-aware cost | post-v0.1.0 | doctrine and external-reference rows | baseline generation first | acceleration claims |
| T22 | Docs & Release | contracts, runbooks, validation transcript, version/release gate | partial | spine contracts and operator docs | release transcript over implemented path | v0.1.0 completion |

### 6.1 Canonical Runtime Sequences

This section is the active forward roadmap. It normalizes planned work into
sequences so future implementation waves do not add isolated runtime rows.

The unified ledger preserves completed history. Older planned rows remain
visible for continuity, but the sequences below define the operational order for
new runtime work. A sequence row is not complete until implemented behavior,
tests, command proof, failure paths, cleanup behavior, and explicit boundaries
exist.

Active forward sequences:

| Sequence | Purpose | Current state | Boundary |
| --- | --- | --- | --- |
| A | Completed diagnostic runtime closure | implemented through bounded diagnostic CLI generation | not full model generation |
| B | Model-class dynamic planning | active; MoE class report complete, output/tokenizer/dense class gaps remain | report/planning only until runtime rows exist |
| C | Dense full-runtime path | planned | cannot close DeepSeek/MoE runtime |
| D | MoE full-runtime path | report-only complete through tensor collection; target decision needed before activation | cannot claim MoE support before activation/dispatch/runtime integration |
| E | DeepSeek target path | planned beyond current selected slices | DeepSeek-specific rows only when generic rows are insufficient |
| F | Serving path | planned | starts after runtime generation exists |
| G | Evaluation and benchmark path | planned | measurements require reproducibility metadata |
| H | Speculative and routing-aware acceleration path | planned | starts after baseline generation and benchmark harness exist |
| I | Documentation and public evidence path | planned | public docs wait for implemented behavior and measured evidence |

Model-class dynamic selection:

```text
model class detected
-> dense path if dense
-> MoE path if MoE
-> future Metal/Qwen path if Qwen/Metal
-> future GLM huge-source/storage path if GLM/source-only
```

YVEX must remain dynamic by model class:

```text
dense -> dense transformer path
MoE -> router/expert transformer path
source-only -> intake/storage/model-class path only
selected-runtime-slice -> bounded partial/diagnostic path only
```

#### Sequence A - Current Completed Diagnostic Chain

```text
A0 selected artifact identity/materialization
A1 selected graph segment
A2 token input
A3 segment-summary prefill
A4 diagnostic KV binding
A5 bounded diagnostic decode
A6 bounded diagnostic logits
A7 bounded greedy sampling
A8 bounded diagnostic generation
A9 CLI generate surface
```

Sequence A is implemented diagnostic runtime closure only. It proves local
runtime control flow over bounded diagnostic state. It is not full model
generation, DeepSeek generation, provider generation, evaluation, benchmark, or
throughput evidence.

#### Sequence B - Model-Class Dynamic Runtime Planning

```text
B0 family runtime adapter report              complete
B1 attention class report                     complete
B2 KV cache class report                      complete
B3 context class report                       complete
B4 MoE class report                           complete
B5 dense class report                         planned if/when dense target is selected
B6 output-head/tokenizer class report         planned
B7 MoE tensor collection report               complete/report-only
B8 residency class/tensor residency plan      planned after target/full artifact decision
```

`B5 dense class report` is intentionally present so YVEX does not become
DeepSeek/MoE-only. It is planned and not implemented.

#### Sequence C - Dense Full-Runtime Path

```text
DENSE.RUNTIME.0  Dense model-class report
DENSE.GRAPH.0    Dense transformer block requirements
DENSE.PREFILL.0  Real dense transformer prefill
DENSE.DECODE.0   Real dense decode over KV
DENSE.LOGITS.0   Real output-head logits
DENSE.SAMPLE.0   Real vocabulary sampling
DENSE.GEN.0      Dense baseline generation
DENSE.BENCH.0    Dense baseline benchmark
```

Dense sequence rows are future rows. They cannot close DeepSeek/MoE runtime,
MoE activation, MoE dispatch, DeepSeek generation, provider generation,
evaluation, benchmark, or throughput claims.

#### Sequence D - MoE Full-Runtime Path

```text
MOE.CLASS.0      MoE model-class report complete
MOE.TENSOR.0     MoE tensor collection report complete/report-only
MOE.RESIDENCY.0  Expert residency and storage pressure report
MOE.ACT.0        Expert activation boundary
MOE.DISPATCH.0   Expert dispatch and accumulation boundary
MOE.BLOCK.0      MoE transformer block fixture/selected-slice boundary
MOE.PREFILL.0    Real MoE transformer prefill
MOE.DECODE.0     Real MoE decode over KV
MOE.LOGITS.0     Real MoE path to output-head logits
MOE.SAMPLE.0     Real vocabulary sampling over MoE logits
MOE.GEN.0        MoE baseline generation
```

`MOE.CLASS.0` and `MOE.TENSOR.0` are reports. `MOE.TENSOR.0` shows the current
selected artifacts do not contain enough router/expert/shared-expert tensor
roles to advance directly into activation. `MOE.ACT.0` is not full MoE support
unless graph/runtime integration exists. `MOE.GEN.0` cannot complete before real
prefill, decode, logits, sampling, token append, stop conditions, cleanup, and
command proof exist.

#### Sequence E - DeepSeek Target Path

```text
DEEPSEEK.CLASS.0       DeepSeek model-family runtime facts
DEEPSEEK.MOE.0         DeepSeek MoE class report
DEEPSEEK.RESIDENCY.0   DeepSeek tensor/expert residency plan
DEEPSEEK.PREFILL.0     DeepSeek real transformer prefill
DEEPSEEK.DECODE.0      DeepSeek real decode
DEEPSEEK.LOGITS.0      DeepSeek real output-head logits
DEEPSEEK.SAMPLE.0      DeepSeek real vocabulary sampling
GEN.DEEPSEEK.0         DeepSeek V4 Flash full generation path
BENCH.DEEPSEEK.DECODE.0
BENCH.DEEPSEEK.GEN.0
```

This sequence may reuse generic MoE rows when generic rows are sufficient. Use
DeepSeek-specific rows only when DeepSeek-specific behavior must be isolated.
Selected DeepSeek slices do not close DeepSeek full generation.

#### Sequence F - Serving Path

```text
SERVE.RUNTIME.0   Serving runtime ownership map
SERVE.STATE.0     Daemon reflects real runtime model state
SERVE.GEN.0       Provider generation backed by real generation
SERVE.STREAM.0    Streaming tokens from real generation
SERVE.API.0       Compatibility layer after server generation exists
SERVE.OBS.0       Server traces/metrics
```

Serving starts only after runtime generation exists. Serving does not own
generation semantics.

#### Sequence G - Evaluation and Benchmark Path

```text
EVAL.FIXTURE.0
EVAL.PARTIAL.0
EVAL.PREFILL.0
EVAL.KV.0
EVAL.DECODE.0
EVAL.LOGITS.0
EVAL.SAMPLING.0
EVAL.GEN.0
EVAL.CAPABILITY.0
BENCH.RUNTIME.0
BENCH.PREFILL.0
BENCH.DECODE.0
BENCH.GEN.0
BENCH.MEMORY.0
BENCH.SERVER.0
```

Benchmarks must always record model identity, artifact identity, qtype,
context, backend, machine, command, run count, and reproducibility metadata.

#### Sequence H - Speculative and Routing-Aware Acceleration Path

```text
SPEC.DSPARK.REF.0
SPEC.VERIFY.0
SPEC.VERIFY.COST.0
SPEC.MOE.ROUTING.0
SPEC.MOE.EXPERT.BUDGET.0
SPEC.DEEPSEEK.ACCOUNTING.0
SPEC.DEEPSEEK.0
BENCH.DEEPSEEK.SPEC.0
```

Speculative rows are after baseline generation and benchmark harness readiness.
Dense speculative verification and MoE routing-aware verification must remain
separate.

#### Sequence I - Docs and Public Evidence Path

```text
DOCS.INTERNAL.SEQUENCE.0
DOCS.RUNBOOK.RUNTIME.0
DOCS.RUNBOOK.DEEPSEEK.0
DOCS.API.RUNTIME.0
DOCS.CONTRACT.RUNTIME.0
DOCS.README.RUNTIME.0
DOCS.DIAGRAM.RUNTIME.0
DOCS.DIAGRAM.MEASUREMENT.0
DOCS.PUBLIC.EVIDENCE.0
```

Public docs cannot expose internal IDs. Public claims wait for implemented
behavior and measured evidence.

### 6.2 v0.1.0 Master Implementation Spine

This section is the main v0.1.0 forward roadmap. It expands the compact runtime
sequences into delivery-sized rows so future waves can choose one concrete
boundary without collapsing source intake, artifact identity, graph execution,
runtime state, generation, evaluation, benchmark, serving, and release work.

All rows in this section are planned unless they map to completed ledger history
above. A V010 row is not complete until implemented behavior, tests, command
proof, failure paths, cleanup/lifecycle behavior, and explicit boundaries exist.

#### TRACK 00 — Scope, Version, and Release Authority

```text
V010.SCOPE.0        v0.1.0 release doctrine
V010.SCOPE.1        v0.1.0 minimum gates
V010.SCOPE.2        v0.1.0 non-goals
V010.SCOPE.3        v0.1.0 included track map
V010.SCOPE.4        v0.1.0 excluded/postponed track map
V010.SCOPE.5        v0.1.0 target selection policy
V010.SCOPE.6        v0.1.0 release-readiness vocabulary
V010.SCOPE.7        v0.1.0 claim boundary map
```

Boundary: spine only, no implementation.

#### TRACK 01 — Target Registry and Target Decision

```text
V010.TARGET.0       target class registry refresh
V010.TARGET.1       selected-runtime-slice target report
V010.TARGET.2       full-runtime-candidate target report
V010.TARGET.3       dense candidate target report
V010.TARGET.4       MoE candidate target report
V010.TARGET.5       DeepSeek pressure target report
V010.TARGET.6       GLM source-only pressure target report
V010.TARGET.7       Qwen/Metal pressure target report
V010.TARGET.8       external reference target report
V010.TARGET.9       v0.1.0 target decision record
V010.TARGET.10      target decision refusal/rollback policy
```

Purpose: decide what can actually close v0.1.0.

v0.1.0 cannot remain undefined. Completion requires a command-visible target
policy/report, not generation.

#### TRACK 02 — Operator-Local Storage Layout

```text
V010.PATHS.0        operator root layout report
V010.PATHS.1        source/artifact/reference/report/cache separation
V010.PATHS.2        registry path layout
V010.PATHS.3        report output layout
V010.PATHS.4        runtime cache layout
V010.PATHS.5        artifact hygiene report
V010.PATHS.6        path override precedence
V010.PATHS.7        missing path/refusal behavior
V010.PATHS.8        v0.1.0 path acceptance gate
```

Purpose: no shell wall, no personal path leaks, no artifact sprawl.

#### TRACK 03 — Official Source Intake

```text
V010.SOURCE.0       source manifest schema refresh
V010.SOURCE.1       source family/profile fields
V010.SOURCE.2       source artifact class fields
V010.SOURCE.3       source shard count/footprint report
V010.SOURCE.4       source provenance fields
V010.SOURCE.5       native safetensors inventory
V010.SOURCE.6       source tensor metadata inventory
V010.SOURCE.7       source-only refusal behavior
V010.SOURCE.8       GLM source pressure report
V010.SOURCE.9       Qwen source pressure report
V010.SOURCE.10      v0.1.0 source acceptance gate
```

Boundary: source intake is not runtime execution.

#### TRACK 04 — Tensor Mapping and Conversion Planning

```text
V010.MAP.0          tensor mapping schema
V010.MAP.1          dense tensor naming map
V010.MAP.2          MoE tensor naming map
V010.MAP.3          DeepSeek tensor naming map
V010.MAP.4          GLM tensor naming map
V010.MAP.5          Qwen tensor naming map
V010.MAP.6          output-head tensor mapping
V010.MAP.7          tokenizer metadata mapping
V010.MAP.8          missing-role blocker report
V010.MAP.9          v0.1.0 tensor mapping gate
```

Purpose: before graph/runtime, tensor names must map to roles.

#### TRACK 05 — Quantization and Artifact Production

```text
V010.QUANT.0        qtype policy report
V010.QUANT.1        dtype/qtype support by role
V010.QUANT.2        qtype compute/refusal matrix
V010.QUANT.3        calibration/imatrix requirement report
V010.ARTIFACT.EMIT.0 controlled artifact emission
V010.ARTIFACT.EMIT.1 selected artifact emission
V010.ARTIFACT.EMIT.2 small full-runtime artifact emission
V010.ARTIFACT.EMIT.3 split artifact plan
V010.ARTIFACT.EMIT.4 artifact parse roundtrip
V010.ARTIFACT.EMIT.5 artifact registration
V010.ARTIFACT.EMIT.6 v0.1.0 artifact production gate
```

Boundary: artifact emission is not inference.

#### TRACK 06 — Artifact Identity and Integrity

```text
V010.INTEGRITY.0    artifact identity manifest
V010.INTEGRITY.1    size/digest gate
V010.INTEGRITY.2    metadata parse gate
V010.INTEGRITY.3    tensor directory gate
V010.INTEGRITY.4    tensor byte-range gate
V010.INTEGRITY.5    shape/rank/dtype gate
V010.INTEGRITY.6    element count/overflow gate
V010.INTEGRITY.7    qtype support gate
V010.INTEGRITY.8    registry drift gate
V010.INTEGRITY.9    corruption fixture regression
V010.INTEGRITY.10   materialization preflight gate
V010.INTEGRITY.11   graph integrity gate
V010.INTEGRITY.12   runtime integrity gate
V010.INTEGRITY.13   v0.1.0 integrity acceptance gate
```

Boundary: integrity checks are not supply-chain security beyond implemented
evidence.

#### TRACK 07 — Full Model Inventory and Materialization

```text
V010.FULLMODEL.0    full tensor inventory refresh
V010.FULLMODEL.1    tensor role coverage report
V010.FULLMODEL.2    qtype/dtype summary
V010.FULLMODEL.3    memory pressure report
V010.FULLMODEL.4    materialization plan
V010.FULLMODEL.5    bounded proof tensor materialization
V010.FULLMODEL.6    full-runtime-candidate materialization proof
V010.FULLMODEL.7    selected-slice materialization refusal
V010.FULLMODEL.8    source-only materialization refusal
V010.FULLMODEL.9    byte-limit failure behavior
V010.FULLMODEL.10   materialization cleanup/release
V010.FULLMODEL.11   v0.1.0 materialization gate
```

Mapping: builds on `FULLMODEL.0`-`FULLMODEL.3`, but expands the future v0.1.0
full-runtime candidate path.

Boundary: materialization is not execution.

#### TRACK 08 — Model Class and Dynamic Runtime Routing

```text
V010.CLASS.0        model-class schema finalization
V010.CLASS.1        target class detector
V010.CLASS.2        dense model-class report
V010.CLASS.3        MoE model-class report
V010.CLASS.4        source-only class report
V010.CLASS.5        selected-slice class report
V010.CLASS.6        DeepSeek class report
V010.CLASS.7        GLM class/source-only report
V010.CLASS.8        Qwen class report
V010.CLASS.9        context class integration
V010.CLASS.10       attention class integration
V010.CLASS.11       KV class integration
V010.CLASS.12       output-head class report
V010.CLASS.13       tokenizer class report
V010.CLASS.14       runtime requirement report
V010.CLASS.15       dynamic path selection report
V010.CLASS.16       v0.1.0 class acceptance gate
```

Completed mapping: `MOE.CLASS.0` maps to `V010.CLASS.3`.

Boundary: reports only. No runtime.

#### TRACK 09 — Tensor Collections

```text
V010.TENSOR.0       tensor collection schema
V010.TENSOR.1       embedding collection
V010.TENSOR.2       attention norm collection
V010.TENSOR.3       post-attention norm collection
V010.TENSOR.4       final norm collection
V010.TENSOR.5       Q projection collection
V010.TENSOR.6       K projection collection
V010.TENSOR.7       V projection collection
V010.TENSOR.8       O projection collection
V010.TENSOR.9       RoPE/position metadata collection
V010.TENSOR.10      attention mask/rule collection
V010.TENSOR.11      KV runtime-state collection
V010.TENSOR.12      dense MLP gate/up/down collection
V010.TENSOR.13      dense activation collection
V010.TENSOR.14      MoE router collection
V010.TENSOR.15      MoE expert gate/up/down collection
V010.TENSOR.16      MoE shared expert collection
V010.TENSOR.17      MoE dispatch metadata collection
V010.TENSOR.18      output-head collection
V010.TENSOR.19      tokenizer metadata collection
V010.TENSOR.20      runtime input/output token collection
V010.TENSOR.21      required tensor coverage report
V010.TENSOR.22      missing tensor blocker report
V010.TENSOR.23      v0.1.0 tensor collection gate
```

Completed mapping: `TENSOR.MOE.0` maps to `V010.TENSOR.14-17` as a report-only
MoE tensor collection surface.

Boundary: tensor collection support is not model support.

#### TRACK 10 — Storage Stream

```text
V010.STORAGE.0      storage-stream doctrine refresh
V010.STORAGE.1      storage root and cache layout
V010.STORAGE.2      source shard index
V010.STORAGE.3      artifact shard index
V010.STORAGE.4      tensor byte-range map
V010.STORAGE.5      tensor page map
V010.STORAGE.6      tensor chunk map
V010.STORAGE.7      cold-read probe
V010.STORAGE.8      warm-read probe
V010.STORAGE.9      repeated-read diagnostics
V010.STORAGE.10     staged-read proof
V010.STORAGE.11     host cache policy
V010.STORAGE.12     eviction policy
V010.STORAGE.13     short read failure
V010.STORAGE.14     missing shard failure
V010.STORAGE.15     digest mismatch failure
V010.STORAGE.16     cleanup/release report
V010.STORAGE.17     GLM source storage pressure report
V010.STORAGE.18     MoE expert storage pressure report
V010.STORAGE.19     output-head storage pressure report
V010.STORAGE.20     v0.1.0 storage gate
```

Boundary: storage-stream support is not generation.

#### TRACK 11 — Residency

```text
V010.RESIDENCY.0    residency class report
V010.RESIDENCY.1    resident tensor plan
V010.RESIDENCY.2    CPU residency plan
V010.RESIDENCY.3    CUDA residency plan
V010.RESIDENCY.4    managed-memory report
V010.RESIDENCY.5    host-staged residency plan
V010.RESIDENCY.6    SSD-staged residency plan
V010.RESIDENCY.7    SSD-streamed residency plan
V010.RESIDENCY.8    hybrid residency plan
V010.RESIDENCY.9    distributed future-only report
V010.RESIDENCY.10   embedding residency
V010.RESIDENCY.11   attention tensor residency
V010.RESIDENCY.12   KV residency
V010.RESIDENCY.13   dense MLP residency
V010.RESIDENCY.14   MoE expert residency
V010.RESIDENCY.15   output-head residency
V010.RESIDENCY.16   tokenizer/runtime metadata residency
V010.RESIDENCY.17   residency transition proof
V010.RESIDENCY.18   residency cleanup/failure report
V010.RESIDENCY.19   v0.1.0 residency gate
```

Boundary: residency plan is not model support.

#### TRACK 12 — Backend Capability

```text
V010.BACKEND.0      backend capability matrix
V010.BACKEND.1      CPU baseline capability report
V010.BACKEND.2      CUDA capability report
V010.BACKEND.3      CUDA allocation proof
V010.BACKEND.4      CUDA transfer proof
V010.BACKEND.5      CUDA op parity subset
V010.BACKEND.6      backend op refusal policy
V010.BACKEND.7      backend fallback policy
V010.BACKEND.8      backend scratch allocation policy
V010.BACKEND.9      backend cleanup/failure report
V010.BACKEND.10     future Metal feasibility report
V010.BACKEND.11     future ROCm feasibility report
V010.BACKEND.12     v0.1.0 backend gate
```

Boundary: backend support is not model support.

#### TRACK 13 — Hardware and Build Profiles

```text
V010.HARDWARE.0     local workstation profile
V010.HARDWARE.1     Spark/GB10 profile
V010.HARDWARE.2     Mac/Apple Silicon profile
V010.HARDWARE.3     Strix Halo/ROCm future profile
V010.HARDWARE.4     memory budget report
V010.HARDWARE.5     storage bandwidth pressure report
V010.HARDWARE.6     reproducibility metadata profile
V010.BUILD.0        build profile matrix
V010.BUILD.1        CPU debug build
V010.BUILD.2        CPU release build
V010.BUILD.3        CUDA debug build
V010.BUILD.4        CUDA release build
V010.BUILD.5        sanitizer build
V010.BUILD.6        release artifact hygiene
V010.BUILD.7        v0.1.0 build gate
```

Boundary: hardware profile is not backend implementation.

#### TRACK 14 — Graph Primitive Integration

```text
V010.GRAPH.PRIM.0   primitive inventory report
V010.GRAPH.PRIM.1   RoPE integration readiness
V010.GRAPH.PRIM.2   attention primitive readiness
V010.GRAPH.PRIM.3   matmul/projection readiness
V010.GRAPH.PRIM.4   MLP primitive readiness
V010.GRAPH.PRIM.5   expert-slice primitive readiness
V010.GRAPH.PRIM.6   softmax/numerics policy
V010.GRAPH.PRIM.7   activation function policy
V010.GRAPH.PRIM.8   residual/add policy
V010.GRAPH.PRIM.9   normalization policy
V010.GRAPH.PRIM.10  graph primitive regression gate
```

Purpose: bridge completed standalone primitives into real graph integration.

#### TRACK 15 — Real Transformer Graph Path

```text
V010.GRAPH.0        graph requirement report
V010.GRAPH.1        real embedding graph input
V010.GRAPH.2        real attention norm
V010.GRAPH.3        real Q projection
V010.GRAPH.4        real K projection
V010.GRAPH.5        real V projection
V010.GRAPH.6        real RoPE/position application
V010.GRAPH.7        real attention score path
V010.GRAPH.8        real causal/mask path
V010.GRAPH.9        real softmax path
V010.GRAPH.10       real value accumulation
V010.GRAPH.11       real O projection
V010.GRAPH.12       attention residual path
V010.GRAPH.13       post-attention norm
V010.GRAPH.14       dense MLP gate/up/down path
V010.GRAPH.15       dense MLP residual path
V010.GRAPH.16       final norm path
V010.GRAPH.17       output hidden state ownership
V010.GRAPH.18       graph scratch lifecycle
V010.GRAPH.19       graph cleanup/failure report
V010.GRAPH.20       first real transformer block
V010.GRAPH.21       repeated real layer stack
V010.GRAPH.22       selected-slice graph proof
V010.GRAPH.23       full-runtime-candidate graph proof
V010.GRAPH.24       v0.1.0 graph gate
```

Boundary: graph is not generation until runtime loop consumes it.

#### TRACK 16 — MoE Graph Path

```text
V010.MOE.0          MoE requirement report
V010.MOE.1          expert count report
V010.MOE.2          active expert count report
V010.MOE.3          shared expert report
V010.MOE.4          router tensor report
V010.MOE.5          router logits boundary
V010.MOE.6          routing dtype/top-k policy
V010.MOE.7          top-k expert selection
V010.MOE.8          expert weight selection
V010.MOE.9          expert dispatch plan
V010.MOE.10         expert dispatch proof
V010.MOE.11         expert compute proof
V010.MOE.12         expert accumulation proof
V010.MOE.13         shared expert integration
V010.MOE.14         MoE residual integration
V010.MOE.15         MoE cleanup/failure report
V010.MOE.16         MoE selected-slice proof
V010.MOE.17         MoE block integration
V010.MOE.18         MoE prefill integration
V010.MOE.19         MoE decode integration
V010.MOE.20         v0.1.0 MoE gate
```

Completed report-only: `V010.TENSOR.14-17` follows the MoE class facts and does
not implement activation or dispatch. The report output keeps selected-artifact
MoE roles missing and points target/full artifact decision before activation.

Boundary: a routed expert primitive is not full MoE support.

#### TRACK 17 — Attention Runtime

```text
V010.ATTN.0         attention runtime requirement report
V010.ATTN.1         Q source validation
V010.ATTN.2         K source validation
V010.ATTN.3         V source validation
V010.ATTN.4         O source validation
V010.ATTN.5         head layout validation
V010.ATTN.6         RoPE/position runtime rule
V010.ATTN.7         mask runtime rule
V010.ATTN.8         attention scratch policy
V010.ATTN.9         full attention runtime path
V010.ATTN.10        GQA/MQA/MLA family rule if required
V010.ATTN.11        attention reference comparison
V010.ATTN.12        attention cleanup/failure
V010.ATTN.13        v0.1.0 attention gate
```

Boundary: attention primitive alone is not full transformer attention.

#### TRACK 18 — KV Runtime

```text
V010.KV.0           KV requirement report
V010.KV.1           KV shape policy
V010.KV.2           KV dtype/qtype policy
V010.KV.3           KV capacity estimator
V010.KV.4           CPU KV allocation
V010.KV.5           CUDA KV allocation
V010.KV.6           K write from prefill attention
V010.KV.7           V write from prefill attention
V010.KV.8           K/V read during decode
V010.KV.9           layer/head/position indexing
V010.KV.10          token position advancement
V010.KV.11          context overflow behavior
V010.KV.12          KV clear/reinit
V010.KV.13          KV cleanup/failure
V010.KV.14          KV trace/inspect
V010.KV.15          paged KV plan
V010.KV.16          paged KV skeleton
V010.KV.17          host spill experiment
V010.KV.18          SSD spill experiment
V010.KV.19          KV quantization policy
V010.KV.20          v0.1.0 KV gate
```

Boundary: KV allocator is not decode support unless decode reads it.

#### TRACK 19 — Context and Prefill Planning

```text
V010.CONTEXT.0      active context policy
V010.CONTEXT.1      model max context report
V010.CONTEXT.2      requested context report
V010.CONTEXT.3      chunk size policy
V010.CONTEXT.4      chunk planner
V010.CONTEXT.5      prefill position policy
V010.CONTEXT.6      decode position policy
V010.CONTEXT.7      overflow refusal behavior
V010.CONTEXT.8      context stop behavior
V010.CONTEXT.9      context trace
V010.CONTEXT.10     v0.1.0 context gate
```

Mapping: builds on `CONTEXT.CLASS.0`.

#### TRACK 20 — Real Prefill Runtime

```text
V010.PREFILL.0      prefill requirement report
V010.PREFILL.1      token input to prefill planner
V010.PREFILL.2      embedding prefill input
V010.PREFILL.3      real layer-0 prefill entry
V010.PREFILL.4      real attention prefill
V010.PREFILL.5      real KV write during prefill
V010.PREFILL.6      dense MLP prefill
V010.PREFILL.7      MoE router/expert prefill
V010.PREFILL.8      repeated layer prefill
V010.PREFILL.9      chunked real prefill
V010.PREFILL.10     staged/SSD prefill plan
V010.PREFILL.11     prefill state ownership
V010.PREFILL.12     prefill cleanup/failure
V010.PREFILL.13     prefill trace
V010.PREFILL.14     prefill regression
V010.PREFILL.15     v0.1.0 prefill gate
```

Boundary: diagnostic prefill remains separate.

#### TRACK 21 — Real Decode Runtime

```text
V010.DECODE.0       decode requirement report
V010.DECODE.1       decode state ownership
V010.DECODE.2       decode position input
V010.DECODE.3       decode reads real KV
V010.DECODE.4       decode attention step
V010.DECODE.5       decode dense MLP path
V010.DECODE.6       decode MoE path
V010.DECODE.7       decode hidden state output
V010.DECODE.8       one real decode step
V010.DECODE.9       repeated decode lifecycle
V010.DECODE.10      decode interruption/cancel safe point
V010.DECODE.11      decode cleanup/failure
V010.DECODE.12      decode trace
V010.DECODE.13      decode regression
V010.DECODE.14      v0.1.0 decode gate
```

Boundary: decode is not generation without logits/sampling/append.

#### TRACK 22 — Output Head and Logits

```text
V010.LOGITS.0       logits requirement report
V010.LOGITS.1       final hidden-state ownership
V010.LOGITS.2       final norm integration
V010.LOGITS.3       output-head tensor mapping
V010.LOGITS.4       output-head residency
V010.LOGITS.5       output-head projection
V010.LOGITS.6       logits buffer allocation
V010.LOGITS.7       logits dtype/range report
V010.LOGITS.8       logits checksum report
V010.LOGITS.9       top-k diagnostics
V010.LOGITS.10      logprob diagnostics
V010.LOGITS.11      sharded output-head plan
V010.LOGITS.12      staged/SSD output-head plan
V010.LOGITS.13      logits cleanup/failure
V010.LOGITS.14      logits trace
V010.LOGITS.15      logits regression
V010.LOGITS.16      v0.1.0 logits gate
```

Boundary: diagnostic logits are not real model logits.

#### TRACK 23 — Sampling Runtime

```text
V010.SAMPLE.0       sampling requirement report
V010.SAMPLE.1       greedy over real logits
V010.SAMPLE.2       selected token report
V010.SAMPLE.3       candidate set report
V010.SAMPLE.4       temperature validation
V010.SAMPLE.5       top-k sampling
V010.SAMPLE.6       top-p sampling
V010.SAMPLE.7       min-p sampling
V010.SAMPLE.8       typical sampling
V010.SAMPLE.9       seeded stochastic sampling
V010.SAMPLE.10      deterministic reproducibility report
V010.SAMPLE.11      sampling cleanup/failure
V010.SAMPLE.12      sampling trace
V010.SAMPLE.13      sampling regression
V010.SAMPLE.14      v0.1.0 sampling gate
```

Minimum: greedy over real logits.

#### TRACK 24 — Tokenizer, Detokenization, and Stop Policy

```text
V010.TOKENIZER.0    tokenizer requirement report
V010.TOKENIZER.1    tokenizer metadata loader/report
V010.TOKENIZER.2    token ID input contract
V010.TOKENIZER.3    token ID output contract
V010.TOKENIZER.4    special token report
V010.TOKENIZER.5    EOS token policy
V010.TOKENIZER.6    stop-token text policy
V010.TOKENIZER.7    prompt/template policy
V010.TOKENIZER.8    detokenization boundary
V010.TOKENIZER.9    tokenizer failure/refusal behavior
V010.TOKENIZER.10   tokenizer trace
V010.TOKENIZER.11   tokenizer regression
V010.TOKENIZER.12   v0.1.0 tokenizer gate
```

Minimum: token IDs may be acceptable for v0.1.0 if text detokenization remains
explicitly unsupported.

#### TRACK 25 — Generation Runtime

```text
V010.GEN.0          generation requirement report
V010.GEN.1          real generation state ownership
V010.GEN.2          generation option parser
V010.GEN.3          prefill -> decode composition
V010.GEN.4          decode -> logits composition
V010.GEN.5          logits -> sample composition
V010.GEN.6          sample -> append composition
V010.GEN.7          real token append
V010.GEN.8          context stop policy
V010.GEN.9          EOS stop policy
V010.GEN.10         stop-token policy
V010.GEN.11         max-new-tokens policy
V010.GEN.12         generation checksum
V010.GEN.13         generation trace
V010.GEN.14         failure/cancel safe points
V010.GEN.15         cleanup/release
V010.GEN.16         CLI real generation command
V010.GEN.17         generation smoke
V010.GEN.18         generation regression
V010.GEN.19         v0.1.0 generation gate
```

Boundary: generation must state whether it is diagnostic or real-runtime.

#### TRACK 26 — Runtime Lifecycle, Cancellation, and Failure

```text
V010.RUNTIME.0      runtime lifecycle report
V010.RUNTIME.1      engine/session ownership finalization
V010.RUNTIME.2      runtime state creation
V010.RUNTIME.3      runtime state mutation rules
V010.RUNTIME.4      runtime cleanup idempotence
V010.RUNTIME.5      partial output preservation
V010.RUNTIME.6      phase failure vocabulary
V010.RUNTIME.7      preflight failure behavior
V010.RUNTIME.8      graph failure behavior
V010.RUNTIME.9      prefill failure behavior
V010.RUNTIME.10     KV failure behavior
V010.RUNTIME.11     decode failure behavior
V010.RUNTIME.12     logits failure behavior
V010.RUNTIME.13     sampling failure behavior
V010.RUNTIME.14     append failure behavior
V010.RUNTIME.15     cancellation safe points
V010.RUNTIME.16     OS signal boundary
V010.RUNTIME.17     v0.1.0 runtime lifecycle gate
```

Boundary: OS signal cancellation may remain unsupported if explicit.

#### TRACK 27 — Trace, Diagnostics, and Research Surface

```text
V010.TRACE.0        trace taxonomy refresh
V010.TRACE.1        token trace
V010.TRACE.2        graph trace
V010.TRACE.3        tensor role trace
V010.TRACE.4        residency trace
V010.TRACE.5        prefill trace
V010.TRACE.6        KV trace
V010.TRACE.7        decode trace
V010.TRACE.8        logits trace
V010.TRACE.9        sampling trace
V010.TRACE.10       generation trace
V010.TRACE.11       cleanup/failure trace
V010.TRACE.12       raw tensor dump refusal policy
V010.TRACE.13       structured trace output
V010.TRACE.14       v0.1.0 trace gate
```

Purpose: YVEX is a research engine; runtime must be inspectable.

#### TRACK 28 — Operator CLI

```text
V010.CLI.0          command inventory refresh
V010.CLI.1          help layout refresh
V010.CLI.2          normal path first policy
V010.CLI.3          advanced diagnostic flags policy
V010.CLI.4          model target inspect final
V010.CLI.5          paths configure final
V010.CLI.6          models prepare final
V010.CLI.7          models check final
V010.CLI.8          graph check final
V010.CLI.9          runtime check command
V010.CLI.10         real generate normal path
V010.CLI.11         diagnostic generate path
V010.CLI.12         refusal wording audit
V010.CLI.13         structured output mode
V010.CLI.14         NO_COLOR/non-TTY behavior
V010.CLI.15         command proof transcript
V010.CLI.16         v0.1.0 CLI gate
V010.CLI.17         normal output contract and layout baseline
V010.CLI.18         diagnostic output demotion
V010.CLI.19         compact report/table output
V010.CLI.20         structured JSON output mode
V010.CLI.21         metric output surface
V010.CLI.22         audit output surface
V010.CLI.23         quiet/no-color output policy
V010.CLI.24         hardcoded print reduction pass
```

`V010.CLI.17` is the next implementation interlock before returning to source
target work. It must make normal operator output usable without deleting
diagnostic/audit evidence.

Boundary: CLI cannot claim lower runtime behavior that does not exist.

#### TRACK 29 — Doctor and Operator Reports

```text
V010.DOCTOR.0       doctor command scope
V010.DOCTOR.1       environment checks
V010.DOCTOR.2       build/backend checks
V010.DOCTOR.3       CUDA checks
V010.DOCTOR.4       artifact checks
V010.DOCTOR.5       registry checks
V010.DOCTOR.6       model target checks
V010.DOCTOR.7       graph checks
V010.DOCTOR.8       runtime checks
V010.DOCTOR.9       generation readiness checks
V010.DOCTOR.10      common failure cookbook
V010.DOCTOR.11      v0.1.0 doctor gate
```

Purpose: local operators should know what failed without reading code.

#### TRACK 30 — Daemon and Serving

```text
V010.SERVE.0        serving ownership map
V010.SERVE.1        daemon state reflects runtime state
V010.SERVE.2        model registry exposed by daemon
V010.SERVE.3        runtime readiness endpoint
V010.SERVE.4        generate endpoint after CLI generation
V010.SERVE.5        streaming endpoint after real generation
V010.SERVE.6        cancellation boundary
V010.SERVE.7        provider compatibility boundary
V010.SERVE.8        OpenAI compatibility after generation
V010.SERVE.9        Anthropic compatibility after generation
V010.SERVE.10       server observability
V010.SERVE.11       v0.1.0 serving decision gate
```

Default: serving may be post-v0.1.0 unless scope includes it after CLI
generation.

#### TRACK 31 — Evaluation

```text
V010.EVAL.0         eval harness structure
V010.EVAL.1         fixture graph eval
V010.EVAL.2         primitive eval
V010.EVAL.3         selected partial graph eval
V010.EVAL.4         full-runtime-candidate graph eval
V010.EVAL.5         prefill eval
V010.EVAL.6         KV eval
V010.EVAL.7         decode eval
V010.EVAL.8         logits eval
V010.EVAL.9         sampling eval
V010.EVAL.10        generation smoke eval
V010.EVAL.11        tokenizer/stop eval
V010.EVAL.12        failure-path eval
V010.EVAL.13        capability eval plan
V010.EVAL.14        v0.1.0 eval gate
```

Minimum v0.1.0: smoke/regression over implemented path.

#### TRACK 32 — Benchmark and Profiling

```text
V010.BENCH.0        benchmark harness metadata contract
V010.BENCH.1        machine profile record
V010.BENCH.2        artifact identity record
V010.BENCH.3        qtype/context/backend record
V010.BENCH.4        run count/reproducibility record
V010.BENCH.5        prefill benchmark
V010.BENCH.6        decode benchmark
V010.BENCH.7        generation benchmark
V010.BENCH.8        memory pressure benchmark
V010.BENCH.9        server benchmark
V010.BENCH.10       DeepSeek benchmark only after DeepSeek generation
V010.PROFILE.0      runtime profile trace
V010.PROFILE.1      memory profile trace
V010.PROFILE.2      storage profile trace
V010.PROFILE.3      backend profile trace
V010.BENCH.11       v0.1.0 benchmark decision gate
```

Default: benchmarks are not required for v0.1.0 public claims unless measured.

#### TRACK 33 — Speculative and Acceleration

```text
V010.SPEC.0         speculative reference registry
V010.SPEC.1         DSpark reference record
V010.SPEC.2         DFlash/HyperDFlash reference record
V010.SPEC.3         draft source report
V010.SPEC.4         token verification semantics
V010.SPEC.5         accepted-prefix accounting
V010.SPEC.6         rejected-token behavior
V010.SPEC.7         KV rollback/reuse policy
V010.SPEC.8         dense speculative verification
V010.SPEC.9         MoE routing-aware verification report
V010.SPEC.10        MoE expert-budget verification
V010.SPEC.11        verification-cost utility report
V010.SPEC.12        DeepSeek speculative path
V010.SPEC.13        speculative benchmark
```

Default: post-v0.1.0 unless baseline generation and benchmark harness already
exist.

#### TRACK 34 — Paper and Algorithm Registry

```text
V010.PAPER.0        paper registry schema
V010.PAPER.1        attention paper references
V010.PAPER.2        KV/cache paper references
V010.PAPER.3        prefill paper references
V010.PAPER.4        storage/residency paper references
V010.PAPER.5        MoE/routing paper references
V010.PAPER.6        speculative decoding references
V010.PAPER.7        algorithm family map
V010.PAPER.8        implementation stage map
V010.PAPER.9        command surface map
V010.PAPER.10       unsupported boundary map
```

Boundary: paper reference is not implementation.

#### TRACK 35 — Public/Internal Documentation

```text
V010.DOCS.INTERNAL.0 internal v0.1.0 spine summary
V010.DOCS.RUNBOOK.0 operator v0.1.0 runbook
V010.DOCS.RUNBOOK.1 model-specific runbooks
V010.DOCS.API.0     API docs for implemented surface
V010.DOCS.CONTRACT.0 behavior contract update
V010.DOCS.README.0  README runtime thesis update
V010.DOCS.DIAGRAM.0 artifact-to-runtime diagram
V010.DOCS.DIAGRAM.1 runtime ladder diagram
V010.DOCS.DIAGRAM.2 evidence/benchmark diagram
V010.DOCS.DIAGRAM.3 dense vs MoE path diagram
V010.DOCS.DIAGRAM.4 storage/residency diagram
V010.DOCS.PUBLIC.0  public claim audit
V010.DOCS.PUBLIC.1  internal ID leak audit
V010.DOCS.PUBLIC.2  v0.1.0 docs acceptance gate
```

Boundary: public docs wait for implementation.

#### TRACK 36 — Packaging, Versioning, and Release

```text
V010.VERSION.0      version string policy
V010.VERSION.1      v0.1.0 version bump
V010.PACKAGE.0      binary packaging policy
V010.PACKAGE.1      release build artifact policy
V010.PACKAGE.2      no model artifact packaging rule
V010.RELEASE.0      scope lock
V010.RELEASE.1      target lock
V010.RELEASE.2      command proof transcript
V010.RELEASE.3      failure proof transcript
V010.RELEASE.4      artifact guardrail transcript
V010.RELEASE.5      claim audit
V010.RELEASE.6      docs audit
V010.RELEASE.7      changelog/release notes
V010.RELEASE.8      tag readiness report
V010.RELEASE.9      v0.1.0 tag
```

Boundary: no release tag before all gates pass.

#### TRACK 37 — CI, Test Surface, and Guardrails

```text
V010.CI.0           CI/test matrix refresh
V010.CI.1           make check gate
V010.CI.2           make smoke gate
V010.CI.3           make check-cuda gate where available
V010.CI.4           docs surface gate
V010.CI.5           source layout gate
V010.CI.6           code natural gate
V010.CI.7           artifact guardrail
V010.CI.8           forbidden claim scan
V010.CI.9           public docs leak scan
V010.CI.10          command proof transcript gate
V010.CI.11          failure-path transcript gate
V010.CI.12          v0.1.0 CI acceptance gate
```

Purpose: v0.1.0 must be mechanically auditable.

#### TRACK 38 — Post-v0.1.0 Tracks

```text
POST010.DEEPSEEK.FULL.0     full DeepSeek generation if not closed in v0.1.0
POST010.GLM.RUNTIME.0       GLM runtime path
POST010.QWEN.METAL.0        Qwen Metal runtime path
POST010.ROCM.0              ROCm/Strix Halo path
POST010.STORAGE.GEN.0       SSD-streamed generation
POST010.SERVE.PUBLIC.0      production serving surface
POST010.SPEC.0              speculative acceleration
POST010.BENCH.PUBLIC.0      public benchmark table
POST010.EVAL.CAPABILITY.0   capability eval suite
POST010.DOCS.PUBLIC.0       public evidence expansion
```

Purpose: make future ambition explicit without letting it contaminate v0.1.0.

### 6.3 v0.1.0 Row-Level Delivery Contract

A V010 row is not complete because it appears in the spine.

A V010 row is complete only when its row contract is satisfied.

Every future V010 implementation delivery must define:

```text
row id
primary block
owned boundary
allowed inputs
disallowed inputs
implementation surface
command/API proof
output contract
failure paths
cleanup/lifecycle behavior
tests
validation commands
forbidden claims
docs/spine update
next-row decision
```

A row may be report-only, fixture, selected-slice, diagnostic-runtime,
full-runtime, eval-ready, benchmark-ready, or unsupported.

A row may not silently promote itself to a higher stage.

V010 stage vocabulary:

```text
doctrine-only:
  spine/documentation rule only; no runtime behavior.

report-only:
  command-visible report exists; no runtime execution.

fixture-proof:
  synthetic/controlled fixture proves operation, lifecycle, and failure behavior.

selected-slice-proof:
  selected YVEX-produced artifact participates in bounded proof without full model claim.

diagnostic-runtime:
  implemented runtime state is consumed but does not claim full model semantics.

full-runtime-candidate:
  real model tensor path participates in the selected v0.1.0 full-runtime target.

target-specific-runtime:
  specific family/target path, such as DeepSeek, Qwen, or GLM, participates in runtime.

eval-ready:
  implemented runtime path has correctness/regression evidence.

benchmark-ready:
  implemented runtime path has measured benchmark metadata.

release-ready:
  row has passed v0.1.0 release gate.

unsupported:
  explicit refusal/planned-only boundary.
```

#### Universal V010 Row Contract

Every implementation row must define:

```text
row:
  V010.<TRACK>.<N>

title:
  human-readable row title

primary block:
  exactly one canonical block

secondary blocks:
  optional related blocks

stage target:
  doctrine-only | report-only | fixture-proof | selected-slice-proof |
  diagnostic-runtime | full-runtime-candidate | target-specific-runtime |
  eval-ready | benchmark-ready | release-ready | unsupported

owned boundary:
  the smallest behavior or report this row owns

does not own:
  explicit capabilities this row must not claim

allowed inputs:
  artifacts, source facts, prior row outputs, fixtures, reports, or runtime state
  this row may consume

disallowed inputs:
  external runner output, downloaded artifact claims, personal paths, unverified
  tensor ranges, unsupported model families, or future rows

command/API surface:
  exact command or API endpoint that proves the boundary

required output fields:
  stable fields that must appear in text/JSON output

failure paths:
  missing input
  invalid input
  unsupported target
  unsupported backend
  missing tensor role
  shape/dtype mismatch
  allocation failure
  read failure
  graph/runtime failure
  cleanup failure

cleanup/lifecycle:
  ownership, release, idempotence, partial-state behavior

tests:
  unit tests
  command tests
  failure tests
  guardrail tests

proof commands:
  commands to paste in final delivery

docs update:
  current capability update if implemented
  unsupported list update
  active next update
  ledger row status update

claim boundary:
  forbidden strings and required boundary fields

next decision:
  deterministic rule for choosing the next row

completion text:
  exact sentence that can be used in the ledger completion boundary
```

#### Contract Family A — Doctrine / Spine Rows

Applies to:
- `V010.SCOPE.*`
- doctrine-only rows
- supersession maps
- release policy rows

Required contract:

```text
stage target:
  doctrine-only

command/API surface:
  none, unless docs tests/grep proof count as command proof

must include:
  rationale
  scope
  non-goals
  forbidden claims
  relationship to Active Next
  no runtime implementation boundary

tests:
  git diff --check
  sh tests/test_docs_surface.sh
  sh tests/test_surface.sh
  required grep proof
  forbidden claim scan

completion boundary:
  docs/spine only; no runtime capability.
```

Forbidden:
- runtime implementation claim;
- generation claim;
- eval/benchmark claim;
- public release claim.

#### Contract Family B — Report-Only Rows

Applies to:
- `V010.TARGET.*`
- `V010.CLASS.*`
- `V010.TENSOR.*`
- `V010.RESIDENCY.*` when report-only
- `V010.BACKEND.*` capability reports
- `V010.HARDWARE.*`
- `V010.PAPER.*`

Required contract:

```text
stage target:
  report-only

command/API surface:
  must expose a stable CLI report or explicitly cite existing report command

required fields:
  report:
  status:
  target_id:
  target_class:
  model:
  family:
  backend:
  source_class:
  artifact_class:
  implementation_stage:
  runtime_claim:
  generation:
  benchmark_status:
  blockers:
  next_required_rows:
```

Required boundary fields:

```text
runtime_claim: unsupported
generation: unsupported-full-model
benchmark_status: not-measured
```

Completion requires:
- command-visible report;
- source-only refusal where relevant;
- unknown-family refusal where relevant;
- tests for success/refusal paths.

#### Contract Family C — Source / Artifact Rows

Applies to:
- `V010.SOURCE.*`
- `V010.MAP.*`
- `V010.QUANT.*`
- `V010.ARTIFACT.EMIT.*`
- `V010.INTEGRITY.*`
- `V010.FULLMODEL.*`

Required contract:

```text
stage target:
  report-only | fixture-proof | selected-slice-proof

allowed inputs:
  official source tensors
  YVEX-produced artifacts
  tiny synthetic fixtures
  selected YVEX-produced artifacts

disallowed inputs:
  external GGUF as implementation proof
  external runner output as implementation proof
  downloaded real model files in git

required fields:
  source_path:
  source_class:
  artifact_path:
  artifact_class:
  yvex_produced:
  digest_status:
  tensor_range_status:
  dtype_status:
  qtype_status:
  materialization_status:
  cleanup_status:
  runtime_claim:
  generation:
  benchmark_status:
```

Failure paths:
- missing file;
- short read;
- invalid header;
- corrupt tensor directory;
- byte-range overflow;
- unsupported qtype;
- byte limit exceeded;
- cleanup failure.

#### Contract Family D — Storage and Residency Rows

Applies to:
- `V010.STORAGE.*`
- `V010.RESIDENCY.*`
- `V010.KV.15-19`
- storage-backed prefill/output-head rows

Required contract:

```text
stage target:
  report-only | fixture-proof | selected-slice-proof

required fields:
  storage_mode:
  residency_mode:
  source_path:
  artifact_path:
  tensor_name:
  byte_range_start:
  byte_range_end:
  page_size:
  chunk_size:
  cold_read_status:
  warm_read_status:
  staged_read_status:
  cache_policy:
  eviction_policy:
  cleanup_status:
  runtime_claim:
  generation:
  benchmark_status:
```

Forbidden:
- storage-stream generation claim;
- disk-backed generation claim;
- benchmark claim from cold-read probe.

Completion requires:
- bounded read proof or explicit report-only boundary;
- failure and cleanup behavior.

#### Contract Family E — Backend / Hardware / Build Rows

Applies to:
- `V010.BACKEND.*`
- `V010.HARDWARE.*`
- `V010.BUILD.*`

Required fields:

```text
backend:
build_profile:
hardware_profile:
device_status:
allocation_status:
transfer_status:
op_support_status:
fallback_status:
cleanup_status:
model_support:
runtime_claim:
generation:
benchmark_status:
```

Required boundary:

```text
model_support: unsupported
runtime_claim: unsupported
generation: unsupported-full-model
benchmark_status: not-measured
```

Hardware profile is not backend support. Backend support is not model support.
Build flag is not runtime support.

#### Contract Family F — Graph Rows

Applies to:
- `V010.GRAPH.PRIM.*`
- `V010.GRAPH.*`
- `V010.ATTN.*`
- `V010.MOE.*`

Required fields:

```text
graph: report|run
status:
graph_stage:
target_id:
target_class:
backend:
op:
layer:
tensor_sources:
input_shape:
output_shape:
dtype:
qtype:
reference_status:
checksum:
max_abs_diff:
scratch_status:
cleanup_status:
runtime_state_mutated:
prefill_ready:
decode_ready:
logits_ready:
generation:
benchmark_status:
```

Required boundary:
- graph op proof is not prefill;
- transformer block proof is not generation;
- MoE expert-slice proof is not full MoE support;
- attention primitive is not full transformer attention.

Failure paths:
- missing tensor;
- invalid shape;
- dtype mismatch;
- unsupported backend;
- allocation failure;
- reference mismatch;
- cleanup failure.

#### Contract Family G — Runtime State Rows

Applies to:
- `V010.CONTEXT.*`
- `V010.PREFILL.*`
- `V010.KV.*`
- `V010.DECODE.*`
- `V010.LOGITS.*`
- `V010.SAMPLE.*`
- `V010.TOKENIZER.*`
- `V010.GEN.*`
- `V010.RUNTIME.*`

Required fields:

```text
runtime:
status:
runtime_stage:
model:
target_id:
target_class:
backend:
state_id:
lifecycle_status:
input_token_count:
prefill_token_count:
decode_position:
kv_status:
logits_status:
sampling_status:
generation_state:
cleanup_attempted:
cleanup_status:
generation_ready:
generation:
benchmark_status:
```

Required boundary fields until real full runtime exists:

```text
generation_ready: false
generation: unsupported-full-model
benchmark_status: not-measured
```

Rows that become real-runtime must explicitly distinguish:
- diagnostic-runtime;
- selected-slice;
- full-runtime-candidate;
- target-specific-runtime.

Failure paths:
- invalid token input;
- context overflow;
- missing prefill state;
- KV capacity failure;
- decode failure;
- logits failure;
- sampling failure;
- append failure;
- cleanup failure.

#### Contract Family H — CLI / Operator Rows

Applies to:
- `V010.CLI.*`
- `V010.DOCTOR.*`
- `V010.PATHS.*`
- operator preset rows

Required fields:

```text
command:
status:
normal_path:
advanced_flags:
target_id:
backend:
output_mode:
structured_output:
no_color:
non_tty:
refusal_status:
runtime_claim:
generation:
benchmark_status:
```

Rules:
- normal path first;
- advanced diagnostic flags later;
- no shell walls;
- no command may claim lower-level behavior not implemented.

#### Contract Family I — Serving Rows

Applies to:
- `V010.SERVE.*`

Required fields:

```text
serve:
status:
daemon_state:
runtime_state:
model_registry_status:
generation_endpoint_status:
streaming_status:
provider_compatibility:
runtime_claim:
generation:
benchmark_status:
```

Rules:
- serving starts after CLI/runtime generation;
- server does not own generation semantics;
- provider compatibility cannot precede tested server generation.

#### Contract Family J — Eval / Benchmark / Profile Rows

Applies to:
- `V010.EVAL.*`
- `V010.BENCH.*`
- `V010.PROFILE.*`

Eval required fields:

```text
eval:
status:
runtime_path:
model_identity:
artifact_identity:
backend:
qtype:
context:
test_case:
expected:
observed:
tolerance:
pass:
generation:
benchmark_status:
```

Benchmark required fields:

```text
bench:
status:
runtime_path:
model_identity:
artifact_identity:
qtype:
context:
prompt_length:
generated_token_count:
backend:
machine:
command:
run_count:
warmup_count:
duration:
tokens_per_second:
reproducibility_note:
benchmark_status:
```

Rules:
- eval follows implemented runtime;
- benchmark follows implemented runtime;
- no throughput without benchmark harness metadata.

#### Contract Family K — Release Rows

Applies to:
- `V010.VERSION.*`
- `V010.PACKAGE.*`
- `V010.RELEASE.*`
- `V010.CI.*`
- `V010.DOCS.PUBLIC.*`

Required fields:

```text
release:
status:
version:
scope_locked:
target_locked:
commands_proven:
failures_proven:
artifact_guardrail:
claim_audit:
docs_audit:
tag_ready:
tag:
```

Rules:
- no v0.1.0 tag before all gates pass;
- release notes cannot overclaim;
- no internal IDs in public docs;
- no model artifacts packaged.

### 6.4 v0.1.0 Row Promotion Rules

A V010 row can move from planned to complete only if:

1. it names one primary block;
2. it satisfies its contract family;
3. command/API proof exists;
4. success and refusal paths are tested;
5. cleanup/lifecycle behavior is visible;
6. docs/spine is updated;
7. unsupported claims remain unsupported;
8. Active Next is updated or intentionally preserved;
9. final delivery contains commit SHA, validation, push status, and boundary.

A row may move to blocked only if:
- blocker is explicit;
- next unblock row is named;
- no capability is claimed.

A row may move to superseded only if:
- replacement row exists;
- completed history is preserved;
- no implemented behavior is hidden.

### 6.5 v0.1.0 Next-Row Decision Grammar

After each completed row, choose next row by blocker class:

```text
source blocker:
  V010.SOURCE.* or V010.MAP.*

artifact blocker:
  V010.ARTIFACT.EMIT.* or V010.INTEGRITY.*

class blocker:
  V010.CLASS.*

tensor blocker:
  V010.TENSOR.*

storage blocker:
  V010.STORAGE.*

residency blocker:
  V010.RESIDENCY.*

backend blocker:
  V010.BACKEND.* or V010.BUILD.*

graph blocker:
  V010.GRAPH.*, V010.ATTN.*, or V010.MOE.*

runtime blocker:
  V010.CONTEXT.*, V010.PREFILL.*, V010.KV.*, V010.DECODE.*,
  V010.LOGITS.*, V010.SAMPLE.*, V010.TOKENIZER.*, V010.GEN.*

operator blocker:
  V010.CLI.*, V010.DOCTOR.*, V010.PATHS.*

evidence blocker:
  V010.EVAL.*, V010.BENCH.*, V010.PROFILE.*

release blocker:
  V010.DOCS.*, V010.CI.*, V010.RELEASE.*, V010.VERSION.*, V010.PACKAGE.*
```

### 6.6 v0.1.0 Track Critical Path

```text
V010.TARGET.9 decision record: blocked-no-candidate
-> V010.TARGET.2 full-runtime-candidate target report
-> V010.SOURCE / V010.MAP / V010.ARTIFACT as required
-> V010.INTEGRITY gate
-> V010.FULLMODEL gate
-> V010.CLASS gate
-> V010.TENSOR gate
-> V010.RESIDENCY gate
-> V010.BACKEND gate
-> V010.GRAPH gate
-> V010.CONTEXT gate
-> V010.PREFILL gate
-> V010.KV gate
-> V010.DECODE gate
-> V010.LOGITS gate
-> V010.SAMPLE gate
-> V010.TOKENIZER boundary
-> V010.GEN gate
-> V010.CLI gate
-> V010.EVAL smoke
-> V010.CI gate
-> V010.DOCS public/internal audit
-> V010.RELEASE tag readiness
```

The critical path may choose dense or MoE runtime depending on the v0.1.0 target
decision. DeepSeek pressure remains active, but a smaller full-runtime candidate
may close v0.1.0 if selected honestly.

### 6.7 v0.1.0 Track Parallelization Rules

Can run in parallel:
- source/intake reports and storage reports;
- model-class reports and tensor collection reports;
- backend capability reports and artifact integrity reports;
- docs/internal runbook updates and implemented command surfaces;
- eval fixture planning and runtime work, if eval does not claim unimplemented runtime.

Cannot run ahead:
- generation before decode/logits/sampling;
- decode before real prefill and KV read/write;
- logits before final hidden state and output head;
- sampling before real logits;
- serving before CLI/runtime generation;
- benchmark before measured runtime path;
- speculative before baseline generation and target verification semantics;
- public claim before implementation and claim audit.

### 6.8 v0.1.0 Required Final Transcript

The final v0.1.0 release candidate must include a single transcript containing:

1. clean checkout status;
2. build profile;
3. target decision report;
4. paths report;
5. artifact identity report;
6. model class report;
7. tensor collection report;
8. residency/backend report;
9. graph proof;
10. prefill proof;
11. KV proof;
12. decode proof;
13. logits proof;
14. sampling proof;
15. tokenizer/stop boundary proof;
16. generation proof;
17. failure proof;
18. cleanup proof;
19. eval smoke proof;
20. claim audit;
21. artifact guardrail;
22. docs audit;
23. tag readiness report.

### 6.9 Block-to-V010 Crosswalk

| Block | Owns | V010 tracks | Current evidence | Main gap before v0.1.0 |
| --- | --- | --- | --- | --- |
| BLOCK 0 | source/target evidence | V010.TARGET.*, V010.SOURCE.* | model target registry, source doctrine, DeepSeek/GLM/Qwen target facts | v0.1.0 full-runtime target decision |
| BLOCK 1 | artifact production | V010.MAP.*, V010.QUANT.*, V010.ARTIFACT.EMIT.* | controlled/selected GGUF emission, DeepSeek selected prepare | small/full-runtime candidate artifact path |
| BLOCK 2 | identity/integrity | V010.INTEGRITY.*, V010.FULLMODEL.* | digest, byte range, corruption, materialization gates | v0.1.0 artifact acceptance gate |
| BLOCK 3 | class/tensor | V010.CLASS.*, V010.TENSOR.*, V010.MOE.* | family-runtime, attention/KV/context reports | MoE class, output-head/tokenizer class, tensor coverage |
| BLOCK 4 | storage/residency | V010.STORAGE.*, V010.RESIDENCY.* | residency planning in fullmodel reports | storage stream and expert/output-head residency proof |
| BLOCK 5 | backend/hardware/build | V010.BACKEND.*, V010.HARDWARE.*, V010.BUILD.* | CPU/CUDA probe/movement/parity subset | full backend matrix and v0.1.0 build profile |
| BLOCK 6 | graph | V010.GRAPH.*, V010.ATTN.*, V010.MOE.* | primitives, block/layers fixture, selected graph | real QKV/O, real block/layer over target tensors |
| BLOCK 7 | runtime | V010.CONTEXT.*, V010.PREFILL.*, V010.KV.*, V010.DECODE.*, V010.LOGITS.*, V010.SAMPLE.*, V010.TOKENIZER.*, V010.GEN.*, V010.RUNTIME.* | diagnostic prefill/KV/decode/logits/sample/generate | real prefill/KV/decode/logits/sampling/generation |
| BLOCK 8 | operator/serve | V010.CLI.*, V010.DOCTOR.*, V010.PATHS.*, V010.SERVE.* | paths, prepare, check, bounded generate CLI, yvexd status | real generation normal path and doctor/server gates |
| BLOCK 9 | evidence/release | V010.EVAL.*, V010.BENCH.*, V010.PROFILE.*, V010.DOCS.*, V010.RELEASE.*, V010.CI.* | docs tests, runbooks, guardrails | eval smoke, benchmark harness, release transcript |

### Planned Row Supersession Map

This map prevents old planned rows from competing with the canonical forward
sequences. It does not delete completed history and does not promote planned
work to complete.

```text
SPINE.RUNTIME.LADDER.0:
  superseded-by: SPINE.V0_1_0.MASTER.1.

SPINE.V0_1_0.MASTER.0:
  superseded-by: SPINE.V0_1_0.MASTER.1 if MASTER.0 exists.

MODEL.CLASS.0:
  superseded-by: V010.CLASS.0, V010.CLASS.2, V010.CLASS.3, and V010.TARGET.*.

MODEL.CLASS.1:
  superseded-by: MOE.CLASS.0, V010.CLASS.3, and V010.MOE.0-4.

MODEL.CLASS.2:
  superseded-by: V010.CLASS.9-15 and family runtime plus attention/KV/context/MoE/output-head reports.

MODEL.CLASS.3:
  superseded-by: V010.CLASS.7, V010.SOURCE.8, and future GLM source/model-class rows.

MODEL.CLASS.*:
  superseded-by: V010.CLASS.*.

TENSOR.COLLECTION.0:
  superseded-by: V010.TENSOR.0 and V010.TENSOR.21-23.

TENSOR.COLLECTION.1:
  superseded-by: V010.TENSOR.*, V010.CLASS.6, V010.MOE.*, and V010.RESIDENCY.*.

TENSOR.COLLECTION.2:
  superseded-by: V010.SOURCE.8, V010.MAP.4, V010.STORAGE.17, and GLM source inventory/model-class rows.

TENSOR.COLLECTION.*:
  superseded-by: V010.TENSOR.*.

STORAGE.STREAM.*:
  superseded-by: V010.STORAGE.*.

RESIDENCY.*:
  superseded-by: V010.RESIDENCY.*.

COMPUTE.BACKEND.*:
  superseded-by: V010.BACKEND.* and V010.HARDWARE.*.

BACKEND.PROFILE.*:
  superseded-by: V010.BACKEND.* and V010.HARDWARE.*.

BUILD.PROFILE.*:
  superseded-by: V010.BUILD.* and V010.CI.*.

HARDWARE.PROFILE.*:
  superseded-by: V010.HARDWARE.*.

PREFILL.* planned:
  superseded-by: V010.PREFILL.* and V010.CONTEXT.*.

KV.MIN.*:
  historical minimal diagnostic KV rows. Do not use as future full-runtime KV
  sequence names unless still needed for regression continuity. Future KV work
  is superseded-by: V010.KV.*.

RUNTIME.KV.*:
  future advanced KV capacity, estimator, allocation, paging, spill, and
  quantization work. Superseded-by: V010.KV.*, V010.RESIDENCY.*, and
  V010.STORAGE.*.

DECODE.* planned:
  superseded-by: V010.DECODE.*.

LOGITS.* planned:
  superseded-by: V010.LOGITS.*.

SAMPLING.* planned:
  superseded-by: V010.SAMPLE.*.

GEN.* planned:
  superseded-by: V010.GEN.*, V010.RUNTIME.*, and V010.CLI.*.

MODEL.LIFECYCLE.*:
  superseded-by: V010.CLI.*, V010.DOCTOR.*, V010.PATHS.*, and V010.TARGET.*.

CLI.UX.*:
  superseded-by: V010.CLI.*, V010.DOCTOR.*, and V010.TRACE.*.

SERVER.* and SERVER.RUNTIME.*:
  superseded-by: V010.SERVE.*.

EVAL.*:
  superseded-by: V010.EVAL.*.

BENCH.*:
  superseded-by: V010.BENCH.* and V010.PROFILE.*.

SPEC.*:
  superseded-by or mapped under: V010.SPEC.*.

PAPER.INDEX.0:
  mapped under V010.PAPER.*.

ALGORITHM.MODES.0:
  mapped under V010.PAPER.*, V010.TRACE.*, V010.CLI.*, and algorithm family doctrine.

CLI.RESEARCH.0:
  mapped under V010.TRACE.*, V010.CLI.*, V010.PAPER.*, and algorithm family doctrine.

LAYOUT.*:
  superseded-by: completed root-first layout history and future cleanup-only
  rows. Future layout pressure is routed through V010.CI.*, V010.BUILD.*, and
  V010.DOCS.*.

DOCS.*:
  superseded-by: V010.DOCS.*, V010.RELEASE.*, and V010.CI.*.
```

These tracks may advance in parallel only when their boundaries are explicit.
A row is complete only when its command proof demonstrates the boundary it
claims.

This track may advance in parallel with DeepSeek CUDA and GLM source-tensor
work only when boundaries remain explicit. Qwen-Metal cannot close DeepSeek
generation, GLM source-tensor, CUDA, ROCm, eval, or benchmark rows.

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

### Model Target Classes

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

metal-reduced-full-runtime-pressure:
  an Apple Silicon / Metal pressure target using smaller or medium open-weight
  Qwen-family models to force backend portability, unified-memory residency,
  source-to-artifact intake, tensor role mapping, YVEX-produced artifact
  identity, selected materialization, Metal primitive parity, and future
  full-runtime generation. This class is planned and not implemented.

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
  Qwen small/medium official source tensors: planned metal-reduced-full-runtime-pressure
  Qwen future YVEX-produced GGUF: planned YVEX-produced artifact
  Qwen external GGUFs: external-GGUF-reference only
  Qwen external runners: external-runner-reference only
```

### Operator-local Artifact Storage

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

### Operator Preset Command Contracts

#### OPERATOR.PATHS.0 command contract

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

#### MODEL.TARGET.PATHS.0 command contract

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

#### MODEL.PREPARE.0 command contract

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

#### MODEL.CHECK.0 command contract

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

#### MODEL.CHECK.1 segment contract

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

#### GRAPH.CHECK.0 command contract

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

#### CHAT.UX.0 command contract

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

#### OPERATOR.FLOW.3 docs contract

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


## 6. v0.1.0 Critical Path

The v0.1.0 critical path starts at target decision and ends at release
readiness. Eval, benchmark, speculative, public serving, and public performance
claims are downstream unless explicitly included after real generation exists.

### 6.1 Critical Path Table

| Step | Track | Gate | Current status | Required evidence | Blocks |
| --- | --- | --- | --- | --- | --- |
| 1 | TRACK.TARGET | GATE.SCOPE | complete / pressure-lane-reported | `yvex model-target decision --release v0.1.0`; `yvex model-target candidate --release v0.1.0`; `yvex model-target dense-candidate --release v0.1.0`; `yvex model-target qwen-metal --release v0.1.0`; Qwen source facts missing | all v0.1.0 runtime |
| 2 | TRACK.ARTIFACT | GATE.ARTIFACT | partial | identity/integrity proof | full runtime |
| 3 | TRACK.MODEL | GATE.CLASS | partial/report-only | class reports | tensor/runtime path |
| 4 | TRACK.TENSOR | GATE.TENSOR | partial/planned | tensor collection coverage | graph |
| 5 | TRACK.RESIDENCY | GATE.RESIDENCY | report-only/planned | residency plan/proof | graph/runtime |
| 6 | TRACK.BACKEND | GATE.BACKEND | partial | CPU/CUDA capability | graph |
| 7 | TRACK.GRAPH | GATE.GRAPH | selected/fixture | real graph path | prefill |
| 8 | TRACK.PREFILL | GATE.PREFILL | diagnostic | real transformer prefill | KV/decode |
| 9 | TRACK.KV | GATE.KV | diagnostic/report | real KV write/read | decode |
| 10 | TRACK.DECODE | GATE.DECODE | diagnostic | real decode | logits |
| 11 | TRACK.LOGITS | GATE.LOGITS | diagnostic | output-head logits | sampling |
| 12 | TRACK.SAMPLING | GATE.SAMPLE | diagnostic | greedy over real logits | generation |
| 13 | TRACK.TOKENIZER | GATE.TOKENIZER | partial/fixture | token/stop boundary | generation quality |
| 14 | TRACK.GENERATION | GATE.GEN | diagnostic | full-runtime generation | CLI/eval |
| 15 | TRACK.OPERATOR | GATE.OPERATOR | partial | normal command proof | release transcript |
| 16 | TRACK.EVAL | GATE.EVIDENCE | planned | smoke over real path | release |
| 17 | TRACK.RELEASE | GATE.RELEASE | planned | transcript/tag readiness | v0.1.0 |

### 6.2 Critical Path Text

The release-critical chain is target -> artifact -> class -> tensor ->
residency/backend -> graph -> prefill -> KV -> decode -> logits -> sampling ->
tokenizer/stop -> generation -> operator proof -> release transcript.

## 7. Active Next

```text
V010.CLI.17 - normal output contract and layout baseline
```

`SPINE.OUTPUT.UX.CONTRACT.0` is complete as a docs/control row. It defines the
normal output plane, diagnostic/audit output plane, output mode taxonomy,
metric labels, color/no-color rules, and hardcoded print reduction policy
without changing CLI behavior.

Completed row:

```text
SPINE.OUTPUT.UX.CONTRACT.0 - CLI output UX contract and diagnostic demotion plan
```

`V010.TARGET.9`, `V010.TARGET.2`, `V010.TARGET.3`, and `V010.TARGET.7` are also
complete as report-only implementation rows. The commands `yvex model-target
decision --release v0.1.0`, `yvex model-target candidate --release v0.1.0`,
`yvex model-target dense-candidate --release v0.1.0`, and `yvex model-target
qwen-metal --release v0.1.0` record the current target decision, candidate,
dense-candidate, and Qwen/Metal pressure lane.

Completed target/source lane row:

```text
V010.TARGET.7 - Qwen/Metal pressure target report
```

```text
model-target: qwen-metal
status: qwen-metal-pressure-report
target_family: qwen
target_class: metal-reduced-full-runtime-pressure
hardware_lane: apple-silicon-metal
backend_lane: metal-planned
source_status: missing
artifact_status: missing
metal_backend_status: unsupported
qwen_runtime_status: unsupported
full_runtime_candidate_status: candidate-planned
runtime_claim: unsupported
generation: unsupported-full-model
benchmark_status: not-measured
release_ready: false
next_required_rows: V010.SOURCE.9
```

The current DeepSeek selected artifacts remain pressure targets for parser,
materialization, graph-slice, diagnostic runtime, and MoE planning. They cannot
close v0.1.0 full-runtime generation. GLM remains source/storage pressure only,
Qwen/Metal is now command-visible as a reduced-scale Apple Silicon / Metal
pressure lane, and tiny fixtures remain fixture-only.

Active Next is now `V010.CLI.17` because the current command output is still
too verbose and diagnostic-heavy for normal operator use. The next code wave
must make normal output compact while preserving diagnostic/audit evidence
behind explicit modes.

After the CLI output interlock, the next target/source lane remains:

```text
V010.SOURCE.9 - Qwen source pressure report
```

`V010.TARGET.7` remains complete. `V010.SOURCE.9` is still required before the
Qwen/Metal lane has source target, manifest, native inventory, source config,
and model-class facts.

## 8. Historical Delivery Ledger

Former heading: Unified Delivery Ledger. Later heading: Historical Unified Delivery Ledger.

This ledger preserves delivery history. It is not the primary forward planning
interface. Use the Forward Track Matrix and Track Detail Sheets for planning.


The table is the spine. Every delivery row is recorded here; sections after the
table explain dependency logic, validation, and rules without duplicating status
tables.

This table is the historical record of deliveries. It is not the preferred
forward roadmap. For forward implementation planning, use `## 6.0 Forward
Runtime Track Matrix` and `## 6.2 v0.1.0 Master Implementation Spine`.

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
| V010.TARGET.3 | complete | target | Dense candidate target report | `yvex model-target dense-candidate --release v0.1.0` reports dense target eligibility for v0.1.0, classifies known selected-slice, source-only, MoE, fixture, portability, and dense-pressure candidates, prints required roles, blockers, and next rows, and preserves unsupported runtime, generation, evaluation, benchmark, throughput, and release-ready boundaries |
| V010.TARGET.7 | complete | target | Qwen/Metal pressure target report | `yvex model-target qwen-metal --release v0.1.0` reports the reduced-scale Qwen on Apple Silicon / Metal pressure lane, source/config, hardware, backend, candidate, blocker, and next-row fields, and preserves unsupported Qwen runtime, Metal backend, materialization, graph/runtime execution, generation, evaluation, benchmark, throughput, and release-ready boundaries |
| V010.TARGET.2 | complete | target | Full-runtime candidate target report | `yvex model-target candidate --release v0.1.0` reports full-runtime candidate eligibility across known target classes, classifies selected-slice/source-only/pressure/fixture targets, prints blockers and next rows, and preserves unsupported runtime, generation, evaluation, benchmark, throughput, and release-ready boundaries |
| V010.TARGET.9 | complete | target | v0.1.0 target decision record | `yvex model-target decision --release v0.1.0` is command-visible and classifies selected-runtime-slice, source-only, pressure, and full-runtime-candidate eligibility; the current decision is blocked-no-candidate with `V010.TARGET.2` as next required row, while preserving unsupported runtime, generation, eval, benchmark, throughput, and release boundaries |
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
| TENSOR.MOE.0 | complete | tensor-collection | MoE tensor collection report | `yvex tensor-collection report --collection moe` is command-visible for DeepSeek selected-runtime-slice targets, GLM source-only targets, unsupported families, missing models, invalid collections, and CPU/CUDA report surfaces; it reports router, expert gate/up/down, shared expert, dispatch metadata, expert indexing, storage/residency pressure, present/missing/unknown roles, blockers, and next rows without tensor materialization, router logits, top-k routing, expert activation, expert dispatch, expert accumulation, MoE block execution, full transformer prefill, decode, logits, sampling, generation, eval, benchmark, throughput, or release claim |
| ATTENTION.CLASS.0 | complete | attention | Attention class report | `yvex attention report` is command-visible for DeepSeek-family artifacts, mapping family-runtime facts into attention type/status, head layout, Q/K/V/O role requirements, RoPE/position requirements, mask rules, KV requirements, context blockers, graph primitive versus full-transformer attention distinction, backend requirements, selected-slice partial reports, source-only refusals, unknown-family refusals, and next runtime dependencies without full transformer attention execution, real attention-backed KV, full model execution, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| CONTEXT.CLASS.0 | complete | context | Context class report | `yvex context report` is command-visible with model/requested/active context fields, token input counts, chunking policy, overflow behavior, decode position policy, bounded diagnostic versus full runtime context distinction, attention/KV dependency reports, selected-slice partial reports, source-only refusals, unknown-family refusals, backend reports, and next runtime dependencies without full transformer prefill, real decode, full model execution, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| KV.CACHE.0 | complete | kv | KV cache class report | `yvex kv report` is command-visible with diagnostic-vs-real KV boundary, layout/dtype/layer/head/position/capacity requirements, residency classes, context dependency, attention dependency, prefill write/decode read requirements, selected-slice partial reports, source-only refusals, unknown-family refusals, backend reports, and next runtime dependencies without real attention-backed KV writes, full transformer prefill, decode, logits, generation, provider generation, eval, benchmark, or throughput claim |
| MOE.CLASS.0 | complete | moe | MoE model-class report | `yvex moe report` is command-visible for DeepSeek selected-runtime-slice targets, GLM source-only targets, unsupported families, and missing models; it reports model_is_moe, expert count status, active expert count status, router facts, shared expert facts, expert tensor roles, storage/residency pressure, blockers, and next rows without router logits, top-k routing, expert activation, expert dispatch, expert accumulation, graph/prefill/decode integration, generation, eval, benchmark, or throughput claim |
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
| SPINE.OUTPUT.UX.CONTRACT.0 | complete | docs | CLI output UX contract and diagnostic demotion plan | spine defines normal, summary, table, verbose, trace, audit, JSON, metric, color, and hardcoded-print policies; marks current verbose diagnostic/report output as non-final UX; maps future output work to V010.CLI.*; preserves runtime, generation, benchmark, and release boundaries |
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
| CLI.GEN.0 | complete | cli | CLI generation command surface | `yvex generate` exposes the implemented bounded diagnostic generation loop through a stable operator CLI surface with clear help, examples, argument validation, command inventory, refusal wording, trace/cancel/context usage, runbook coverage, and unsupported full-model/DeepSeek/provider/eval/benchmark boundaries |
| M16 | planned | server | Provider/server generation boundary | daemon/server generation uses runtime-backed generation path |
| M17 | planned | profile | Trace/profile hardening for generation | traces and profiles identify artifact/backend/graph/KV/decode/logits/sampling/server failures |
| SPINE.GENERATION.TARGET.0 | complete | docs | DeepSeek generation and speculative throughput target envelope | spine records DeepSeek V4 Flash full-generation target, internal decode throughput target, DSpark external reference doctrine, and non-claim benchmark boundary |
| SPINE.SPEC.VERIFICATION.0 | complete | docs | Token verification and verification-cost doctrine | spine separates draft proposal, target verification, verification-cost control, accepted-token accounting, KV/state accounting, and MoE routing-aware verification boundaries without speculative runtime, DeepSeek generation, benchmark, throughput, or external-runner claim |
| SPINE.SEQUENCE.REBASE.0 | complete | docs | Runtime sequence rebase and planned row normalization | spine records canonical diagnostic, model-class dynamic, dense, MoE, DeepSeek, serving, eval/bench, speculative, and public-evidence sequences; preserves completed history; maps redundant planned rows into a supersession map; keeps `MOE.CLASS.0` as Active Next without runtime implementation, DeepSeek generation, provider generation, eval, benchmark, or throughput claim |
| SPINE.V0_1_0.MASTER.1 | complete | docs | Exhaustive v0.1.0 spine expansion | spine expands v0.1.0 into exhaustive target, path, source, mapping, quantization, artifact, integrity, fullmodel, class, tensor, storage, residency, backend, hardware, graph, attention, MoE, KV, context, prefill, decode, logits, sampling, tokenizer, generation, runtime lifecycle, trace, CLI, doctor, serving, eval, benchmark, speculative, paper, docs, packaging, CI, release, and post-v0.1.0 tracks without runtime implementation, generation, eval, benchmark, throughput, or public release claim |
| SPINE.V0_1_0.MASTER.2 | complete | docs | Row-level v0.1.0 delivery contracts | spine adds row-level delivery contracts, contract families, promotion rules, next-row decision grammar, critical path, parallelization rules, final transcript requirements, and Active Next decision rules for the exhaustive v0.1.0 spine without runtime implementation, generation, eval, benchmark, throughput, or release claim |
| SPINE.AUDIT.0 | complete | docs | Whole-repo implementation/spine consistency audit | repo-wide audit compares docs/spine Current Capability, completed rows, planned rows, command surfaces, tests, source ownership, public docs, and V010 architecture against tracked implementation evidence, records weak points, meta/rebase row classification, mismatch findings, and follow-up rows without runtime implementation, generation, eval, benchmark, throughput, or release claim |
| SPINE.RECONCILE.0 | complete | docs | Current Capability and ledger reconciliation | Current Capability, completed row boundaries, meta-spine classification, namespace rules, block/V010 crosswalks, command evidence, test evidence, audit actions, and Active Next are reconciled against audited code/test/command evidence without runtime implementation, generation, eval, benchmark, throughput, or capability promotion |
| SPINE.NAVIGATION.0 | complete | docs | Spine information architecture and runtime map unification | spine adds front control panel, nomenclature table, runtime architecture map, forward runtime track matrix, ledger/track/meta distinctions, standard boundary vocabulary, section-order guide, and navigation cleanup without runtime implementation, capability promotion, generation, eval, benchmark, throughput, or release claim |
| SPINE.REDESIGN.0 | complete | docs | Clean track-based spine architecture rewrite | spine is reorganized around dashboard, nomenclature, runtime architecture map, semantic forward track matrix, track detail sheets, v0.1.0 critical path, active next, historical ledger, meta-spine table, evidence crosswalks, audit section, boundary registry, and doctrine appendix without runtime implementation, capability promotion, generation, eval, benchmark, throughput, or release claim |
| SPINE.TESTMAP.0 | planned | docs | Test-to-ledger coverage map | tests are mapped to ledger rows, evidence stages, commands, and failure paths without runtime implementation or capability promotion |
| SPINE.FILEMAP.0 | planned | docs | Source ownership and file map | source/header/test files are mapped to canonical blocks and spine rows without runtime implementation or capability promotion |
| SPINE.METAL.QWEN.0 | complete | docs | Qwen Metal pressure lane | spine records Qwen on Apple Silicon / Metal as a reduced-scale portability and full-runtime pressure lane without Metal support, Qwen runtime, generation, eval, benchmark, or throughput claim |
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


## 9. Meta-Spine / Rebase / Audit Waves

This table is separate from runtime capability. SPINE rows are meta-control
rows unless they also change code behavior and tests.


This table classifies project-control waves. These rows may remain in the
unified ledger for historical continuity, but they must not inflate runtime
capability.

This is not the implementation ledger. It records project-control rows that
organize the spine, reconcile evidence, audit drift, or improve navigation
without increasing runtime capability.

Audit finding:

```text
meta-spine rows should not inflate Current Capability as runtime implementation.
```

| ID | Status | Meta kind | Affects | Runtime capability change | Evidence type | Boundary |
| --- | --- | --- | --- | --- | --- | --- |
| SPINE.REBASE.1 | complete | runtime rebase | M-series runtime ladder | none | roadmap | docs-only; no runtime implementation |
| SPINE.REBASE.2 | complete | runtime rebase | M3/M4 planning | none | roadmap | docs-only; no runtime implementation |
| SPINE.REBASE.3 | complete | roadmap rebase | runtime/operator roadmap | none | roadmap | docs-only; no generation claim |
| SPINE.REBASE.4 | complete | roadmap rebase | integrity and measurement target | none | roadmap | docs-only; no benchmark claim |
| SPINE.REBASE.5 | complete | ledger rebase | unified delivery ledger | none | namespace | docs-only; no runtime implementation |
| SPINE.BLOCKS.0 | complete | block doctrine | canonical block directory | none | doctrine | docs-only; no runtime implementation |
| SPINE.BLOCKS.1 | planned | cleanup doctrine | planned-row deduplication | none | namespace | future docs-only compression |
| SPINE.OPERATOR.PRESET.0 | complete | operator doctrine | preset command roadmap | none | doctrine | docs-only; no runtime implementation |
| SPINE.GENERATION.TARGET.0 | complete | target doctrine | DeepSeek target and throughput envelope | none | target-pressure | docs-only; no benchmark claim |
| SPINE.SPEC.VERIFICATION.0 | complete | speculative doctrine | token verification and cost control | none | doctrine | docs-only; no generation claim |
| SPINE.SEQUENCE.REBASE.0 | complete | sequence doctrine | runtime sequence normalization | none | roadmap | docs-only; no runtime implementation |
| SPINE.V0_1_0.MASTER.1 | complete | release doctrine | exhaustive V010 track map | none | release-planning | docs-only; no v0.1.0 release claim |
| SPINE.V0_1_0.MASTER.2 | complete | delivery doctrine | row-level V010 contracts | none | contract | docs-only; no runtime implementation |
| SPINE.AUDIT.0 | complete | audit | repo/spine consistency | none | audit | docs-only; no runtime implementation |
| SPINE.RECONCILE.0 | complete | reconciliation | evidence taxonomy and Active Next | none | namespace | docs-only; no runtime implementation |
| SPINE.NAVIGATION.0 | complete | navigation | control panel and runtime maps | none | information architecture | docs-only; no runtime implementation |
| SPINE.REDESIGN.0 | complete | redesign | track-first spine architecture | none | information architecture | docs-only; no runtime implementation |
| SPINE.METAL.QWEN.0 | complete | pressure-lane doctrine | Qwen/Metal future lane | none | target-pressure | docs-only; no generation claim |
| SPINE.OUTPUT.UX.CONTRACT.0 | complete | UX doctrine | CLI output UX | none | contract | docs-only; no CLI behavior, runtime implementation, generation, benchmark, or release claim |


## 10. Evidence Crosswalks

Evidence crosswalks map commands and tests to evidence stages. They are proof
indexes, not runtime capability by themselves.

### 10.1 Command-to-Evidence Crosswalk

| Command | Evidence stage | Proves | Does not prove | Row families |
| --- | --- | --- | --- | --- |
| `yvex inspect` / `metadata` / `tensors` | implemented artifact inspection | GGUF metadata/tensor directory parsing | model execution | ARTIFACT, INTEGRITY |
| `yvex integrity report` | implemented integrity gate | digest/range/shape/dtype checks | supply-chain security beyond checks | INTEGRITY |
| `yvex model-target decision` | report-only | v0.1.0 target decision state, target eligibility, pressure-lane classification, blockers, and next rows | runtime execution, target implementation, generation, release readiness | TRACK.TARGET |
| `yvex model-target candidate` | report-only | v0.1.0 full-runtime candidate eligibility, selected-slice/source-only/pressure/fixture classification, blockers, and next rows | target selection, runtime execution, artifact emission, generation, release readiness | TRACK.TARGET |
| `yvex model-target dense-candidate` | report-only | v0.1.0 dense candidate eligibility, dense role requirements, selected-slice/source-only/MoE/portability/fixture classification, blockers, and next rows | target selection, runtime execution, artifact emission, generation, release readiness | TRACK.TARGET |
| `yvex model-target qwen-metal` | report-only | Qwen/Metal reduced-scale pressure lane, source/hardware/backend/candidate blockers, and next source row | source download, Metal backend support, Qwen runtime, artifact emission, materialization, generation, release readiness | TRACK.TARGET |
| `yvex graph --execute-op` | fixture-proof | standalone graph primitives | full transformer runtime | TRACK.GRAPH |
| `yvex prefill` | diagnostic-runtime | segment/chunk diagnostic prefill | real transformer prefill | TRACK.PREFILL |
| `yvex kv report` | report-only / diagnostic | KV facts and diagnostic KV boundary | real attention KV | TRACK.KV |
| `yvex moe report` | report-only | MoE class facts, router/expert/shared-expert blockers, next tensor/residency rows | router execution or expert dispatch | TRACK.MOE |
| `yvex tensor-collection report --collection moe` | report-only | MoE router/expert/shared/dispatch tensor collection coverage and missing-role blockers | router logits, top-k routing, expert activation, dispatch, prefill, decode, generation | TRACK.TENSOR, TRACK.MOE |
| `yvex decode` | diagnostic-runtime | bounded decode state step | real decode over real KV | TRACK.DECODE |
| `yvex logits` | diagnostic-runtime | bounded logits buffer | output-head logits | TRACK.LOGITS |
| `yvex sample` | diagnostic-runtime | bounded greedy selection | real vocabulary sampling | TRACK.SAMPLING |
| `yvex generate` | diagnostic-runtime | bounded loop, append, stop, trace, cleanup | full model generation | TRACK.GENERATION |
| `yvex attention/context/fullmodel` reports | report-only | model/runtime blockers | runtime execution | TRACK.MODEL, TRACK.ATTENTION, TRACK.CONTEXT |

### 10.2 Test-to-Evidence Crosswalk

| Test group | Evidence stage | Proves | Does not prove | Row families |
| --- | --- | --- | --- | --- |
| unit parser/artifact tests | implemented | parsing, shape/range accounting | runtime | ARTIFACT |
| integrity/corruption tests | implemented | refusal and gate behavior | supply-chain security beyond checks | INTEGRITY |
| graph tests | fixture/selected-slice | primitive/block/layer/selected graph behavior | full transformer runtime | GRAPH |
| prefill/KV/decode/logits/sample/generate tests | diagnostic-runtime | bounded runtime control flow | full-model generation | RUNTIME |
| CLI tests | operator evidence | command surface, refusal, help, presets | new runtime semantics | OPERATOR |
| target decision tests | report-only | v0.1.0 decision state, candidate classification, unsupported release, missing candidate, boundary fields | target implementation or runtime capability | TRACK.TARGET |
| target candidate tests | report-only | full-runtime candidate classification, blockers, next rows, unsupported release, missing target, boundary fields | target selection or runtime capability | TRACK.TARGET |
| dense candidate tests | report-only | dense candidate classification, required dense roles, blockers, next rows, unsupported release, missing target, boundary fields | target selection or runtime capability | TRACK.TARGET |
| Qwen/Metal pressure tests | report-only | Qwen/Metal lane classification, source/hardware/backend blockers, candidate slots, next row, unsupported release, missing target, boundary fields | source intake, Metal backend support, Qwen runtime, generation, benchmark, release | TRACK.TARGET |
| MoE report tests | report-only | selected-slice MoE class report, GLM source-only refusal, unknown-family and missing-model boundaries | MoE runtime execution | TRACK.MOE |
| MoE tensor collection tests | report-only | selected-slice MoE tensor collection missing roles, GLM source-only refusal, unsupported family, missing model, invalid collection, CPU/CUDA report surfaces | MoE runtime execution or tensor materialization | TRACK.TENSOR, TRACK.MOE |
| CUDA tests | backend evidence | CUDA probe/movement/parity subset | model support | BACKEND |
| docs/source/surface tests | guardrail | repository hygiene | runtime behavior | CI |

### 10.3 File-to-Ownership Follow-up

`SPINE.FILEMAP.0` remains planned. It will map source/header/test files to
canonical ownership blocks and track families without runtime capability change.

### 10.4 Spine Redesign Proof

```sh
grep -nF '## 0. Spine Dashboard' docs/spine.md
grep -nF '## 1. Spine Nomenclature' docs/spine.md
grep -nF '## 2. Runtime Architecture' docs/spine.md
grep -nF '## 3. Current Implementation State' docs/spine.md
grep -nF '## 4. Forward Track Matrix' docs/spine.md
grep -nF '## 5. Track Detail Sheets' docs/spine.md
grep -nF '## 6. v0.1.0 Critical Path' docs/spine.md
grep -nF '## 7. Active Next' docs/spine.md
grep -nF '## 8. Historical Delivery Ledger' docs/spine.md
grep -nF '## 9. Meta-Spine / Rebase / Audit Waves' docs/spine.md
grep -nF '## 10. Evidence Crosswalks' docs/spine.md
grep -nF '## 11. Audit and Reconciliation' docs/spine.md
grep -nF '## 12. Boundary Registry' docs/spine.md
grep -nF '## 13. Doctrine Appendix' docs/spine.md
grep -nF 'TRACK.MOE' docs/spine.md
grep -nF 'TRACK.PREFILL' docs/spine.md
grep -nF 'TRACK.KV' docs/spine.md
grep -nF 'TRACK.DECODE' docs/spine.md
grep -nF 'TRACK.LOGITS' docs/spine.md
grep -nF 'TRACK.SAMPLING' docs/spine.md
grep -nF 'TRACK.GENERATION' docs/spine.md
grep -nF 'SPINE.REDESIGN.0' docs/spine.md
grep -nF 'V010.TARGET.2 - full-runtime-candidate target report' docs/spine.md
```

### 10.5 Detailed Validation Gate Archive

This archived validation text preserves earlier proof commands. The active
spine information-architecture proof for this redesign is `### 10.4`.

### 8. Validation Gate

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
./yvex help kv
./yvex help context
./yvex help engine
./yvex help session
./yvex help integrity
./yvex help models
```

Bounded decode/logits/sampling/generation closure proof sequence:

```sh
./yvex kv report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --include-attention --include-context --include-residency --include-blockers
./yvex context report --model deepseek4-v4-flash-selected-embed-rmsnorm --family deepseek --backend cpu --tokens 0,1,2,3 --chunk-size 2 --include-attention --include-kv --include-prefill --include-decode --include-blockers
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

Spine navigation proof:

```sh
grep -nF '## 0. Spine Control Panel' docs/spine.md
grep -nF '## 0.7 Nomenclature Table' docs/spine.md
grep -nF '## 0.8 Runtime Architecture Map' docs/spine.md
grep -nF '## 0.9 Standard Boundary Vocabulary' docs/spine.md
grep -nF '## 0.10 Ledger vs Track vs Architecture Map' docs/spine.md
grep -nF '## 0.11 Spine Section Order' docs/spine.md
grep -nF '## 5. Historical Unified Delivery Ledger' docs/spine.md
grep -nF '## 5.1 Meta-Spine and Rebase Waves' docs/spine.md
grep -nF '## 6.0 Forward Runtime Track Matrix' docs/spine.md
grep -nF 'Target & Source' docs/spine.md
grep -nF 'Artifact & Integrity' docs/spine.md
grep -nF 'Model Class & Tensor Collections' docs/spine.md
grep -nF 'Graph Core' docs/spine.md
grep -nF 'Attention Runtime' docs/spine.md
grep -nF 'MoE Runtime' docs/spine.md
grep -nF 'Model-Backed Prefill' docs/spine.md
grep -nF 'KV Runtime' docs/spine.md
grep -nF 'Decode Runtime' docs/spine.md
grep -nF 'Output Head & Logits' docs/spine.md
grep -nF 'Sampling Runtime' docs/spine.md
grep -nF 'Generation Runtime' docs/spine.md
grep -nF 'SPINE.NAVIGATION.0' docs/spine.md
grep -nF 'V010.TARGET.2 - full-runtime-candidate target report' docs/spine.md
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
grep -nF '## 6.1 Canonical Runtime Sequences' docs/spine.md
grep -nF 'dense -> dense transformer path' docs/spine.md
grep -nF 'MoE -> router/expert transformer path' docs/spine.md
grep -nF 'Planned Row Supersession Map' docs/spine.md
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

### 8.1 Command-to-Evidence Crosswalk

| Command family | Current stage | Proves | Does not prove | Related rows |
| --- | --- | --- | --- | --- |
| `yvex inspect/metadata/tensors` | artifact-inspection | GGUF metadata, tensor directory, descriptor facts | runtime execution, model support, generation | C0, C1, ARTIFACT.*, V010.INTEGRITY.* |
| `yvex integrity` | integrity-proof | identity, digest/range/shape/dtype checks, corruption refusal | supply-chain security, model execution | ARTIFACT.INTEGRITY.*, V010.INTEGRITY.* |
| `yvex materialize` | selected-slice-proof | bounded selected tensor materialization and cleanup | graph execution, prefill, decode, generation | M1, M2, ARTIFACT.INTEGRITY.6, V010.FULLMODEL.* |
| `yvex graph --execute-op` | fixture-proof | standalone primitive operation with checksum/reference/cleanup | full transformer block, prefill, decode, generation | GRAPH.OPS.*, V010.GRAPH.PRIM.* |
| `yvex graph --execute-block` | fixture-proof | controlled transformer-block fixture execution | real model layer execution, generation | GRAPH.BLOCK.0, V010.GRAPH.* |
| `yvex graph --execute-layers` | fixture-proof | controlled repeated layer scheduler fixture | real full layer stack, full transformer prefill | GRAPH.LAYERS.0, V010.GRAPH.* |
| `yvex graph check` | fixture-proof | preset over implemented primitive/block/layer graph proofs | new graph capability beyond underlying commands | GRAPH.CHECK.0, V010.CLI.8 |
| `yvex prefill` | diagnostic-runtime | segment-summary/layer-backed/chunked diagnostic prefill and optional diagnostic KV binding | full transformer prefill, real attention KV writes | M8, PREFILL.1-3, V010.PREFILL.* |
| `yvex kv report` | report-only | KV class, diagnostic-vs-real boundary, layout/capacity/residency blockers | real attention KV allocation/write/read | KV.CACHE.0, V010.KV.* |
| `yvex context report` | report-only | requested/active context, token counts, chunking, overflow and decode-position policy | long-context runtime or real generation context | CONTEXT.CLASS.0, V010.CONTEXT.* |
| `yvex attention report` | report-only | attention class, Q/K/V/O requirements, RoPE/mask/KV/context blockers | full transformer attention, real QKV projection, real KV writes, generation | ATTENTION.CLASS.0, V010.ATTN.* |
| `yvex moe report` | report-only | MoE class facts, router/expert/shared-expert role blockers, storage/residency pressure, next rows | router logits, top-k routing, expert activation/dispatch/accumulation, generation | MOE.CLASS.0, V010.CLASS.3, V010.TENSOR.14-17 |
| `yvex tensor-collection report --collection moe` | report-only | MoE router/expert/shared/dispatch tensor collection coverage, missing-role blockers, and target/full artifact dependency | tensor materialization, router logits, top-k routing, expert activation/dispatch/accumulation, prefill, decode, generation | TENSOR.MOE.0, V010.TENSOR.14-17 |
| `yvex decode` | diagnostic-runtime | bounded diagnostic decode-state step over implemented prefill/KV summary | real decode over full transformer KV | DECODE.0, V010.DECODE.* |
| `yvex logits` | diagnostic-runtime | bounded diagnostic logits buffer with checksum/min/max | real output-head logits | LOGITS.0, V010.LOGITS.* |
| `yvex sample` | diagnostic-runtime | deterministic greedy selection over implemented diagnostic logits | stochastic sampling, real vocabulary sampling, generation quality | SAMPLING.0, V010.SAMPLE.* |
| `yvex generate` | diagnostic-runtime | bounded diagnostic prefill/decode/logits/sample/append/stop/cleanup | full model generation, DeepSeek generation, provider generation, eval, benchmark, tok/s | GEN.LOOP.*, CLI.GEN.0, V010.GEN.* |
| `yvex fullmodel report` | report-only | GGUF inventory, qtype/dtype summary, collection coverage, blockers | payload materialization, runtime execution | FULLMODEL.0, V010.FULLMODEL.* |
| `yvex fullmodel materialization-plan` | report-only | planned placement phases, residency classes, fit estimates, cleanup plan | full materialization, graph execution, generation | FULLMODEL.1, V010.FULLMODEL.*, V010.RESIDENCY.* |
| `yvex fullmodel materialize` | selected-slice-proof | bounded required proof tensor materialization or clean refusal | full DeepSeek materialization, full model execution | FULLMODEL.2, V010.FULLMODEL.* |
| `yvex fullmodel descriptor` | report-only | tensor role map, collection map, runtime requirements and blockers | runtime mutation, decode, logits, generation | FULLMODEL.3, V010.CLASS.*, V010.TENSOR.* |
| `yvex fullmodel family-runtime` | report-only | family adapter roles, attention/KV/MoE/output requirements, next dependencies | model-family runtime support | FAMILY.RUNTIME.0, V010.CLASS.* |
| `yvex model-target candidate` | report-only | v0.1.0 full-runtime candidate eligibility across known target classes, blockers, and next rows | target implementation, artifact emission, tensor materialization, graph/runtime execution, generation, eval, benchmark, release | V010.TARGET.2 |
| `yvex model-target dense-candidate` | report-only | v0.1.0 dense candidate eligibility across known target classes, dense role requirements, blockers, and next rows | target implementation, source intake, artifact emission, tensor materialization, graph/runtime execution, generation, eval, benchmark, release | V010.TARGET.3 |
| `yvex model-target qwen-metal` | report-only | Qwen/Metal reduced-scale pressure lane, source/config, hardware, backend, candidate, blocker, and next-row fields | Qwen source intake, Metal backend support, Qwen artifact emission, tensor materialization, graph/runtime execution, generation, eval, benchmark, release | V010.TARGET.7 |
| `yvex model-target decision` | report-only | v0.1.0 decision state, selected target if any, candidate eligibility, pressure-lane status, blockers, release-critical tracks, and next rows | target implementation, artifact emission, tensor materialization, graph/runtime execution, generation, eval, benchmark, release | V010.TARGET.9 |
| `yvex models prepare` | operator-preset | implemented selected source-to-GGUF and registry path for supported target | materialization, graph, generation, eval, benchmark | MODEL.PREPARE.*, V010.CLI.6 |
| `yvex models check` | operator-preset | selected artifact inspect/integrity/materialize/graph composition by level | decode, logits, sampling, generation | MODEL.CHECK.0, V010.CLI.7 |
| `yvex paths` | operator-preset | operator-local root/path resolution and creation boundaries | download, artifact creation, runtime capability | OPERATOR.PATHS.0, V010.PATHS.* |
| `yvex model-target inspect` | report-only | target class, source/artifact/report/registry paths and unsupported runtime boundary | source inspection, conversion, execution | MODEL.TARGET.PATHS.0, V010.TARGET.* |
| `yvexd status/models` | server-status-shell | daemon/provider status shell and model registry visibility | provider generation, streaming, compatibility | K0, SERVE.RUNTIME.0, V010.SERVE.* |

### 8.2 Test-to-Evidence Crosswalk

| Test family | Covers | Evidence stage | Missing or weak coverage | Follow-up row |
| --- | --- | --- | --- | --- |
| docs surface tests | public/internal docs boundaries and allowed surfaces | docs/meta | no row-by-row docs-to-ledger map | SPINE.TESTMAP.0 |
| source layout tests | root-first source ownership and forbidden source-file sprawl | refactor/quality | no full source-to-row map | SPINE.FILEMAP.0 |
| code natural tests | style/naturalness guardrails and forbidden claim literals | refactor/quality | not behavior proof | SPINE.TESTMAP.0 |
| GGUF/parser tests | GGUF parse, metadata, tensor table, fixtures | artifact-inspection | none critical | none |
| artifact integrity tests | digest, corruption, byte ranges, shape/dtype/rank, refusal | integrity-proof | none critical | none |
| registry/model alias tests | registry add/use/current/remove/alias resolution | operator-preset | no complete ledger row map | SPINE.TESTMAP.0 |
| materialization tests | CPU selected materialization, gates, cleanup/refusal | selected-slice-proof | full-runtime materialization remains planned | V010.FULLMODEL.* |
| CUDA probe/movement/parity tests | CUDA info, tensors, ops, parity, selected materialization | fixture-proof / selected-slice-proof | machine availability varies | V010.BACKEND.*, V010.HARDWARE.* |
| graph primitive tests | RoPE, attention, matmul, MLP/expert-slice primitives | fixture-proof | full transformer integration missing | V010.GRAPH.*, V010.ATTN.*, V010.MOE.* |
| graph block/layer tests | controlled block and repeated layer fixture | fixture-proof | real model block/layer execution missing | V010.GRAPH.* |
| selected graph tests | selected embedding and embedding-plus-RMSNorm graph slices | selected-slice-proof | not full runtime | V010.GRAPH.22-24 |
| prefill tests | segment, layer-backed, chunked, KV-bound diagnostic prefill | diagnostic-runtime | real full transformer prefill missing | V010.PREFILL.* |
| KV tests | minimal KV ownership/report, overflow/refusal, cleanup | diagnostic-runtime / report-only | real attention-backed KV missing | V010.KV.* |
| decode tests | bounded diagnostic decode through CLI/core paths | diagnostic-runtime | repeated real decode missing | V010.DECODE.* |
| logits tests | bounded diagnostic logits buffer and checksum | diagnostic-runtime | real output-head logits missing | V010.LOGITS.* |
| sample tests | deterministic greedy diagnostic sampler | diagnostic-runtime | stochastic and real vocab sampling missing | V010.SAMPLE.* |
| generate tests | bounded diagnostic generation, append, stop, cancel, cleanup | diagnostic-runtime | OS/server cancellation and full generation missing | V010.GEN.*, V010.RUNTIME.* |
| attention report tests | attention report/refusal/boundary output in fullmodel/core paths | report-only | no real attention runtime | V010.ATTN.* |
| KV report tests | KV class/refusal/boundary output | report-only | no full KV allocation/write/read | V010.KV.* |
| context report tests | context report/refusal/chunk/overflow output | report-only | no long-context runtime | V010.CONTEXT.* |
| MoE report tests | MoE class/refusal/boundary output in fullmodel/core paths | report-only | no router execution or expert dispatch | MOE.CLASS.0 / V010.TENSOR.14-17 |
| MoE tensor collection tests | MoE tensor collection selected-slice, GLM source-only, unsupported-family, missing-model, invalid-collection, and CPU/CUDA report paths | report-only | no tensor materialization, router logits, expert activation, dispatch, prefill, decode, or generation | TENSOR.MOE.0 / V010.TENSOR.14-17 |
| target candidate tests | v0.1.0 full-runtime candidate help/report/refusal output, selected-slice/source-only/portability/fixture classification, blockers and next rows | report-only | no target selection, artifact emission, runtime execution, generation, benchmark, or release | V010.TARGET.2 |
| dense candidate tests | v0.1.0 dense candidate help/report/refusal output, dense role requirements, selected-slice/source-only/MoE/portability/fixture classification, blockers and next rows | report-only | no target selection, source intake, artifact emission, runtime execution, generation, benchmark, or release | V010.TARGET.3 |
| Qwen/Metal pressure tests | Qwen/Metal help/report/refusal output, source/config, hardware, backend, candidate, blocker, next-row, unsupported release, and missing target fields | report-only | no source intake, Metal backend, artifact emission, materialization, runtime execution, generation, benchmark, or release | V010.TARGET.7 |
| target decision tests | v0.1.0 target decision help/report/refusal output, selected-slice/source-only classification, unsupported release, missing candidate | report-only | no target implementation, artifact emission, runtime execution, generation, benchmark, or release | V010.TARGET.9 |
| fullmodel report/materialize/descriptor/family-runtime tests | fullmodel inventory, refusal, tiny proof, descriptors, family reports | report-only / selected-slice-proof | broad row-to-test mapping still incomplete | SPINE.TESTMAP.0 |
| operator preset tests | paths, target inspect, prepare/check, model registry | operator-preset | no fully automated row promotion map | SPINE.TESTMAP.0 |
| runbook/public claim tests | public docs boundary and command examples | docs/meta | no release claim audit transcript yet | V010.DOCS.PUBLIC.*, V010.RELEASE.* |
| artifact guardrail scans | no real model artifacts in git; tiny fixtures only | guardrail | manual transcript required at release | V010.CI.7 |
| forbidden claim scans | no forbidden generation/benchmark/release wording | guardrail | exact scan needs maintained allowlist discipline | V010.CI.8 |


## 11. Audit and Reconciliation

Audit and reconciliation are project-control evidence. They should guide next
work, but they are not the primary runtime architecture interface.

### 11.1 SPINE.AUDIT.0 Summary

SPINE.AUDIT.0 found no P0 runtime contradiction. It found P1/P2 spine-coherence
risks around evidence-stage taxonomy, namespace duplication, Current Capability
readability, and report-only/runtime boundaries.

### 11.2 Weak Point Ledger

The active weak points are: CLI normal-output layout, full-runtime graph path,
output-head/tokenizer path, evaluation/benchmark readiness, and source/file/test
coverage maps.

### 11.3 Reconciliation Actions

SPINE.RECONCILE.0 classified evidence stages, normalized forward namespace
rules, added block/command/test crosswalks, and returned runtime planning to
the report-only model-class lane after resolving the docs/meta blocker.

### 11.4 Follow-Up Rows

`SPINE.TESTMAP.0` and `SPINE.FILEMAP.0` remain useful non-runtime follow-ups.
Target/source planning remains queued through `V010.SOURCE.9` after
`V010.CLI.17` because the Qwen/Metal pressure lane is command-visible but still
lacks source target, manifest, native inventory, source config, and model-class
facts.

### 11.5 Detailed Audit Tables

### 10. Spine and Implementation Audit

#### 10.1 Audit Scope

```text
audit row:
  SPINE.AUDIT.0

audit mode:
  whole-repo implementation/spine consistency audit

audited inputs:
  docs/spine.md
  README.md
  docs/api.md
  docs/contract.md
  docs/operator-runbook.md
  docs/runbooks/*
  include/yvex/*
  *.c
  gguf/*
  cuda/*
  tests/*
  scripts/*
  Makefile/build files
  AGENTS.md
  MODEL_ARTIFACTS.md

excluded:
  .git/
  build/
  generated temporary audit files
  real model artifacts
  downloaded model files

runtime changes:
  none
```

Audit summary:

```text
files audited: 246
source files: 38
header files: 56
CUDA files: 8
test files: 138
docs files: 13
commands audited: 35
complete rows audited: 130
planned rows audited: 166
current capability bullets audited: 200
P0 findings: 0
P1 findings: 4
P2 findings: 8
P3 findings: 5

Result:
  spine implementation consistency:
    pass-with-findings

Recommended Active Next:
  SPINE.RECONCILE.0

Boundary:
  audit only
```

The P1 findings are not runtime regressions. They are spine/claim-structure
risks: Current Capability mixes runtime, selected-slice, report-only, and
docs/meta rows; old planned namespaces remain duplicated against V010 rows;
test evidence is not yet mapped row-by-row; and file ownership exists in code
but is not yet available as a committed source-to-row map.

#### 10.2 Audit Status Vocabulary

```text
evidence-backed:
  code, command proof, tests, failure path, cleanup/boundary are present.

partially-backed:
  behavior exists, but tests, failure path, cleanup, or docs boundary are incomplete.

docs-only-valid:
  row is doctrine/report/planning only and correctly says no runtime capability.

overclaimed:
  spine text or Current Capability implies more than code/tests prove.

under-specified:
  implementation exists but output contract, ownership, or tests are unclear.

stale:
  row no longer matches the current architecture or has been superseded.

duplicated:
  row is repeated across old and new planning namespaces without clear ownership.

missing-owner:
  spine row has no clear source/header/test/docs owner.

missing-test:
  behavior exists but no clear regression or command test proves it.

missing-failure-path:
  success path exists but refusal/failure behavior is not proven.

missing-cleanup:
  allocation/state/file/runtime ownership exists but release/idempotence is not proven.

claim-risk:
  wording risks being read as model support, generation, benchmark, or throughput.

needs-rebase:
  row belongs in V010 or meta-spine table rather than main implementation ledger.

needs-implementation:
  row is correctly planned but still has no code.

blocked:
  row cannot progress until named blocker row closes.
```

#### 10.3 Whole-Repo File Audit Matrix

The local audit generated `build/audit/audit-file-list.txt` with all 246 tracked
files. The committed spine records grouped homogeneous families and individual
rows for runtime-heavy or required ownership surfaces. A later `SPINE.FILEMAP.0`
should expand this into a full source/header-to-row map.

| File/Path | Type | Primary owner block | Claimed behavior | Evidence found | Tests found | Risk | Action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `docs/spine.md` | internal docs | all blocks | delivery authority, current capability, ledger, V010 tracks/contracts | audited directly | docs surface tests | medium | reconcile Current Capability grouping |
| `README.md` | public docs | BLOCK 9 | public thesis and boundaries | boundary wording found | docs surface tests | low | keep; audit benchmark table wording later |
| `docs/api.md` | public docs | BLOCK 9 | public API ownership map | boundary wording found | docs surface tests | low | keep |
| `docs/contract.md` | public docs | BLOCK 9 | behavior contract | boundary wording found | docs surface tests | low | keep |
| `docs/operator-runbook.md` | public docs | BLOCK 8 | operator routing index | preset/generation boundaries found | docs surface tests | low | keep |
| `docs/runbooks/deepseek.md` | public runbook | BLOCK 8 | selected DeepSeek lanes | repeated no-generation boundaries found | docs surface tests | low | keep |
| `docs/runbooks/glm.md` | public runbook | BLOCK 0/4 | GLM source/storage lane | source-only boundaries found | docs surface tests | low | keep |
| `docs/runbooks/common.md` | public runbook | BLOCK 8/9 | common operator lanes | no-generation/no-benchmark boundaries found | docs surface tests | low | keep |
| `MODEL_ARTIFACTS.md` | public artifact cards | BLOCK 0/2 | artifact facts and no-claim cards | execution_ready false found | docs surface tests | low | keep |
| `AGENTS.md` | operating rules | all blocks | repo rules and ownership map | hard no-claims found | not runtime-tested | low | keep |
| `yvex_cli.c` | source | BLOCK 8 | top-level command lookup/dispatch | command table only; help pointers domain-owned | `tests/cli/core.sh` | low | keep |
| `yvex_runtime.c` | source | BLOCK 7 | engine/session/run/chat coordination | lifecycle, session, selected graph hooks | runtime/session/CLI tests | medium | source ownership map |
| `yvex_generation.c` | source | BLOCK 7 | bounded diagnostic generation loop | trace/stop/cancel/cleanup code found | `tests/cli/generation.sh` | low | keep |
| `yvex_decode.c` | source | BLOCK 7 | bounded diagnostic decode | cleanup/status fields found | CLI/unit decode coverage via generation/core | low | keep |
| `yvex_logits.c` | source | BLOCK 7 | bounded diagnostic logits | checksum/min/max fields found | CLI/unit logits coverage | low | keep |
| `yvex_sampling.c` | source | BLOCK 7 | bounded greedy sampling | greedy diagnostic path found | CLI/unit sample coverage | low | keep |
| `yvex_prefill.c` | source | BLOCK 7 | segment/layer/chunk prefill diagnostics | chunk/KV/report fields found | `tests/cli/prefill_state.sh` | low | keep |
| `yvex_kv.c` | source | BLOCK 7 | minimal KV ownership/report | cleanup/generation false fields found | `tests/unit/kv.c`, `tests/cli/kv.sh` | low | keep |
| `yvex_graph.c` | source | BLOCK 6 | graph proofs, primitives, block/layer/check | guard/reference/cleanup fields found | graph CLI tests | low | keep |
| `yvex_model_artifacts.c` | source | BLOCK 3/8 | model targets, fullmodel reports, prepare/check | report-only and selected-slice boundaries found | `tests/cli/fullmodel.sh`, model tests | medium | add row-to-test map |
| `yvex_artifact*.c` | source family | BLOCK 2 | artifact parse/identity/integrity/materialization | failure/cleanup status fields found | artifact integrity CLI/unit tests | low | keep |
| `yvex_backend.c` | source | BLOCK 5 | CPU backend and backend command | op support/refusal fields found | backend/unit/graph tests | low | keep |
| `cuda/*` | CUDA source family | BLOCK 5/6 | CUDA probe, tensor movement, kernels, ops | CUDA source and unit tests found | `make check-cuda` when available | medium | record machine availability |
| `gguf/*` | source family | BLOCK 1/2 | GGUF parse/emit/quant/tools/family map | parser/tool owners found | GGUF unit/CLI tests | low | keep |
| `include/yvex/*` | public headers | all blocks | public C API declarations | 47 headers audited by path list | unit/CLI tests by domain | medium | expand in SPINE.FILEMAP.0 |
| `tests/cli/*.sh` | shell tests | BLOCK 9 | command/failure/cleanup proof | 46 shell runners found | self-evident | low | keep |
| `tests/unit/*` | unit tests | BLOCK 9 | API and helper regression | 59 unit files found | `make check` | low | keep |
| `tests/fixtures/gguf/*` | tiny fixtures | BLOCK 2/9 | corrupt/minimal GGUF corpus | tiny tracked fixtures only | integrity/parser tests | low | keep |
| `Makefile` | build | BLOCK 5/9 | build/test targets | check/smoke/CUDA targets found | validation commands | low | keep |

#### 10.4 Current Capability Evidence Audit

The audit counted 200 Current Capability bullets. This table groups every bullet
by evidence class and names the high-risk bullets that need reconciliation.

| Capability claim | Claimed stage | Evidence files | Command proof | Tests | Failure/cleanup proof | Audit status | Required correction |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Parser, GGUF metadata, tensor directory, tokenizer fixture, source/quant/template/imatrix tools | report-only / fixture-proof | `gguf/*`, `yvex_artifact.c`, `yvex_source.c`, tokenizer files | inspect, tensors, metadata, source-manifest, native-weights, tensor-map, quant/imatrix/template commands | parser/tool CLI and unit tests | corrupt fixture suite | evidence-backed | keep grouped as implemented command behavior |
| Artifact identity/integrity/materialization gates | selected-slice-proof | `yvex_artifact*.c`, `yvex_model*.c` | integrity/materialize/model-gate commands | artifact identity/integrity/materialize tests | digest/range/corruption/cleanup tests | evidence-backed | keep |
| Graph primitives, block/layer fixtures, selected partial/segment graph | fixture-proof / selected-slice-proof | `yvex_graph.c`, backend/CUDA files | graph op/block/layers/selected commands | graph CLI and CUDA tests | guard cleanup/failure fields | evidence-backed | keep separate from runtime generation |
| Prefill, KV, decode, logits, sample, generate diagnostic chain | diagnostic-runtime | `yvex_prefill.c`, `yvex_kv.c`, `yvex_decode.c`, `yvex_logits.c`, `yvex_sampling.c`, `yvex_generation.c` | prefill/decode/logits/sample/generate audit commands | generation/prefill/KV/core CLI tests | cancellation, failure, cleanup tests | evidence-backed | label as bounded diagnostic only |
| `operator-grade yvex generate command surface` | diagnostic-runtime | `yvex_generation.c`, `yvex_cli.c` | help generate, generate trace/cancel/context | `tests/cli/generation.sh`, `tests/cli/core.sh` | failure/cancel cleanup tests | evidence-backed | keep wording bounded-diagnostic |
| `fullmodel report/materialization-plan/materialize/descriptor/family-runtime` bullets | report-only / selected-slice refusal / tiny fixture proof | `yvex_model_artifacts.c` | fullmodel report/descriptor/family-runtime audit commands | `tests/cli/fullmodel.sh` | selected-slice refusal and byte-limit tests | report-only-ok | ensure Current Capability does not imply runtime execution |
| `attention report command`, `KV cache class report command`, `context class report command` | report-only | `yvex_model_artifacts.c`, `yvex_kv.c` | attention/kv/context audit commands | core/kv/context-related tests | source-only and unknown-family refusal paths present in code/tests partially mapped | evidence-backed | keep report-only |
| Operator paths, model-target paths, prepare/check presets | report-only / selected-slice-proof | `yvex_fs.c`, `yvex_model_artifacts.c` | paths/model-target/models prepare/check | model/path CLI tests | dry-run/refusal paths | evidence-backed | keep |
| Runtime ownership cleanup and in-place compression bullets | docs/code quality | source history and current file ownership | no direct command proof for compression as behavior | surface/source-layout/code-natural tests | not a runtime lifecycle proof | docs-only-valid / under-specified | move to meta/code-quality group |
| Routing-aware speculative doctrine, V010 master, row contract grammar, Qwen lane, block directory doctrine | docs-only-valid | `docs/spine.md` | grep proof/docs tests | docs surface tests | not applicable | docs-only-valid | move to meta-spine group, not runtime capability |

Finding: Current Capability was directionally accurate but too flat.
`SPINE.RECONCILE.0` rewrote it into classified groups before implementation
continued past the audit blocker.

#### 10.5 Completed Ledger Row Audit

| Row ID | Area | Claimed boundary | Evidence files | Command proof | Tests | Audit status | Correction |
| --- | --- | --- | --- | --- | --- | --- | --- |
| P0-H0, CLI.PACKAGE.*, REPO.* | foundation/layout | repo shape, CLI shell, packaging | root sources, Makefile, CLI | commands/help/version | source-layout/surface tests | ok | keep |
| OWI.0-OWI.FINAL.0 | intake | manifests, native inventory, templates, mapping, quant/imatrix, controlled/selected GGUF | `gguf/*`, `yvex_source.c` | source/quant/template/convert/emit commands | intake/tool tests | ok | keep |
| ARTIFACT.INTEGRITY.0-9/FINAL | artifact | integrity validator, digest, corruption, ranges, gates | `yvex_artifact_integrity.c`, fixtures | integrity/materialize/graph guard commands | artifact CLI/unit tests | ok | keep |
| M1-M9, PREFILL.1-3, DECODE.0, LOGITS.0, SAMPLING.0, GEN.* | runtime | selected/diagnostic runtime closure | runtime/prefill/KV/decode/logits/sampling/generation files | prefill/decode/logits/sample/generate | runtime/generation CLI tests | diagnostic-runtime-ok | keep boundary explicit |
| GRAPH.OPS.*, GRAPH.BLOCK.0, GRAPH.LAYERS.0, GRAPH.CHECK.0 | graph | primitive/block/layer/check proofs | `yvex_graph.c`, backend/CUDA | graph commands | graph CLI/CUDA tests | ok | keep |
| FULLMODEL.0-3, FAMILY.RUNTIME.0, ATTENTION.CLASS.0, KV.CACHE.0, CONTEXT.CLASS.0 | model/report | inventory, descriptor, family, attention/KV/context reports | `yvex_model_artifacts.c`, `yvex_kv.c` | fullmodel/family/attention/kv/context | fullmodel/core/KV tests | report-only-ok | keep report-only boundary |
| OPERATOR.PATHS.0, MODEL.TARGET.PATHS.0, MODEL.PREPARE.*, MODEL.CHECK.0 | operator/model | path, target, selected prepare/check presets | `yvex_fs.c`, `yvex_model_artifacts.c` | paths/model-target/models commands | model/path CLI tests | selected-slice-ok | keep |
| SPINE.REBASE.*, SPINE.BLOCKS.*, SPINE.* doctrine/master/audit rows | docs | doctrine, rebase, target, V010, audit | `docs/spine.md` | grep/docs tests | docs surface tests | docs-only-ok | classify in meta table |
| LAYOUT.RUNTIME.0, LAYOUT.GRAPH.0, REWRITE.* | cleanup/layout | ownership/compression without capability change | root source files | indirect via surface tests | source-layout/code-natural tests | under-evidenced | classify as code-quality/meta in reconciliation |

Audit finding:

```text
row status needs follow-up reconciliation
```

This applies to docs/meta and cleanup rows only. No completed runtime row was
found to require immediate status rollback in this audit.

#### 10.6 Planned Row and Supersession Audit

| Planned row | Current namespace | Superseded by | Still needed? | Problem | Action |
| --- | --- | --- | --- | --- | --- |
| `OWI.TARGETS.*`, `OWI.HUGE.*` | old intake | `V010.SOURCE.*`, `V010.TARGET.*`, `V010.MAP.*` | yes | overlaps V010 | move to V010 |
| `STORAGE.STREAM.*` | old storage | `V010.STORAGE.*` | yes | duplicated namespace | merge with V010 row |
| `MODEL.CLASS.*`, `TENSOR.COLLECTION.*` | old model/tensor | `V010.CLASS.*`, `V010.TENSOR.*` | yes | active next references both old and V010 | move to V010 |
| `RESIDENCY.*`, `COMPUTE.BACKEND.*`, `HARDWARE.PROFILE.*`, `BACKEND.PROFILE.*` | old backend/residency | `V010.RESIDENCY.*`, `V010.BACKEND.*`, `V010.HARDWARE.*` | yes | duplicate planning rows | merge with V010 row |
| `PREFILL.4/5`, `DECODE.1`, `LOGITS.1`, `SAMPLING.1`, `M10-M17` | old runtime | `V010.PREFILL.*`, `V010.DECODE.*`, `V010.LOGITS.*`, `V010.SAMPLE.*`, `V010.GEN.*` | yes | old rows compete with V010 | move to V010 |
| `KV.MIN.*`, `RUNTIME.KV.*` | old KV | `V010.KV.*` | yes | historical and future rows mixed | needs clearer owner |
| `MODEL.LIFECYCLE.*`, `CLI.UX.*` | old operator | `V010.CLI.*`, `V010.DOCTOR.*`, `V010.PATHS.*` | yes | old operator UX rows duplicate V010 | merge with V010 row |
| `SERVER.*` | old serve | `V010.SERVE.*` | yes | old namespace | move to V010/post-v0.1.0 |
| `EVAL.*`, `BENCH.*` | old evidence | `V010.EVAL.*`, `V010.BENCH.*` | yes | duplicated evidence map | merge with V010 row |
| `LAYOUT.*`, `DOCS.*` | old docs/layout | `V010.DOCS.*`, `V010.CI.*`, meta table | partial | mixes meta and future docs | move to meta table |
| `SPEC.*`, `PAPER.INDEX.0`, `ALGORITHM.MODES.0`, `CLI.RESEARCH.0` | old research/spec | `V010.SPEC.*`, `V010.PAPER.*`, `V010.TRACE.*` | yes | future-only and docs-only mixed | move to V010/post-v0.1.0 |

#### 10.7 Spine Rebase / Meta Waves Table

See `## 5.1 Meta-Spine and Rebase Waves`.

#### 10.8 Public Docs Claim Audit

| File | Claim | Public risk | Evidence | Required correction |
| --- | --- | --- | --- | --- |
| `README.md` | engine thesis, selected runtime, future benchmark table shape | none | no internal IDs found; no achieved tok/s found | keep; review future table after benchmark harness |
| `docs/api.md` | API ownership and future runtime surfaces | none | boundary wording separates future generation | keep |
| `docs/contract.md` | runtime/CLI/server behavior contract | none | says provider generation waits for runtime generation | keep |
| `docs/operator-runbook.md` | operator flows and bounded generation lane | none | explicitly says full generation/eval/bench unsupported | keep |
| `docs/runbooks/deepseek.md` | selected DeepSeek lanes | none | repeatedly says not full generation/benchmark | keep |
| `docs/runbooks/glm.md` | GLM source/storage lane | none | says not GLM generation/benchmark | keep |
| `docs/runbooks/common.md` | common diagnostic commands | none | no generation claim | keep |
| `MODEL_ARTIFACTS.md` | selected artifact cards | none | execution_ready false/generation no | keep |
| `AGENTS.md` | operating rules | none | hard no-claims include execution-ready true-state as forbidden wording | keep |

Path scan finding: public docs use `$HOME/lab/models` as an operator-local
example. No personal absolute `/home/...` or `/Users/...` path was found.

Internal ID scan finding: no internal delivery IDs were found in public docs
outside `docs/spine.md`.

#### 10.9 Command Surface Audit

| Command | Exists | Help exists | Normal path clear | Advanced flags clear | Boundary wording | Tests | Audit status |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `commands` | yes | top-level | yes | n/a | implemented command inventory | `tests/cli/core.sh` | ok |
| `decode` | yes | yes | diagnostic only | yes | no logits/sample/generate claim | core/generation tests | ok |
| `logits` | yes | yes | diagnostic only | yes | no real output-head claim | core/generation tests | ok |
| `sample` | yes | yes | greedy diagnostic only | yes | no stochastic/generation claim | core/generation tests | ok |
| `generate` | yes | yes | normal path shown | yes | bounded diagnostic/full model unsupported | `tests/cli/generation.sh` | ok |
| `graph` | yes | yes | op/block/check paths | yes | graph proof not generation | graph tests | ok |
| `input` | yes | yes | token input path | moderate | token validation only | core/token tests | ok |
| `prefill` | yes | yes | segment/chunk path | yes | diagnostic prefill only | prefill tests | ok |
| `kv` | yes | yes | report path | yes | report-only/generation false | KV tests | ok |
| `context` | yes | yes | report path | yes | report-only | core/context tests | ok |
| `attention` | yes | yes | report path | yes | report-only | core/attention tests | ok |
| `fullmodel` | yes | yes | report/descriptor/materialize paths | yes | full execution unsupported | `tests/cli/fullmodel.sh` | ok |
| `engine` | yes | yes | selected diagnostic path | moderate | no generation claim | engine/session tests | ok |
| `session` | yes | yes | diagnostic session path | moderate | accept-only boundary | session tests | ok |
| `integrity` | yes | yes | check/report paths | yes | integrity not supply-chain security | integrity tests | ok |
| `models` | yes | yes | registry/prepare/check paths | yes | selected artifact only | model tests | ok |
| `paths` | yes | yes | operator root path | yes | no download/artifact claim | path/model tests | ok |
| `model-target` | yes | yes | target inspect/classes | yes | target not capability | model-target tests | ok |

#### 10.10 Test Coverage Audit

| Test file | Covers | Commands exercised | Failure paths | Cleanup/lifecycle | Missing coverage |
| --- | --- | --- | --- | --- | --- |
| `tests/cli/core.sh` | command inventory/help/refusal | many top-level commands | argument/refusal checks | boundary checks | none for broad CLI |
| `tests/cli/generation.sh` | bounded diagnostic generation | generate | decode/logits/sample/append/cancel/context failures | cleanup repeated/idempotent | OS signal/server cancel |
| `tests/cli/prefill_state.sh` | prefill/chunk/KV binding | prefill | invalid tokens/context | cleanup fields | full transformer prefill |
| `tests/cli/kv.sh`, `tests/unit/kv.c` | KV ownership/report | kv | overflow/refusal | clear/reinit cleanup | real attention KV writes |
| `tests/cli/*_graph.sh` | graph primitives/block/layers/selected segments | graph | allocation/dispatch/refusal | cleanup pass/not-needed | real QKV projection/full transformer |
| `tests/cli/fullmodel.sh` | fullmodel reports/materialization boundaries | fullmodel | selected/source/tiny fixture refusal | cleanup/release | wider row-to-test map |
| `tests/cli/artifact_*.sh`, `tests/unit/artifact*.c` | identity/integrity/corruption | integrity/materialize | corrupt fixtures/range/digest | cleanup fields | none critical |
| `tests/cli/models.sh`, `tests/cli/model_aliases.sh` | registry/model presets | models/model-target/paths | dry-run/unsupported/refusal | registry cleanup | none critical |
| `tests/cli/cuda.sh`, `tests/unit/cuda/*` | CUDA probe/parity when available | cuda-info/graph/materialize | CUDA unavailable/refusal | cleanup fields | machine-dependent |
| `tests/test_docs_surface.sh`, `tests/test_surface.sh` | docs/public/source surface | n/a | guardrails | n/a | no row-to-test map |
| `tests/test_source_layout.sh`, `tests/test_code_natural.sh` | source layout/style | n/a | layout guard | n/a | no behavior proof |

#### 10.11 Source Ownership and Architecture Audit

| Source file | Domain | Public surface | Internal behavior | State ownership | Cleanup ownership | Spine rows | Gaps |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `yvex_cli.c` | CLI dispatch | top-level help/dispatch only | command table | none | none | CLI.SURFACE.*, V010.CLI | keep thin |
| `yvex_runtime.c` | runtime coordination | engine/session/run/info | selected graph/session wiring | engine/session | session/KV cleanup | M3, G0, runtime rows | large file; map helpers |
| `yvex_generation.c` | generation loop | generate command | bounded loop/trace/cancel/stop | generation state | idempotent cleanup | GEN.*, CLI.GEN.0 | no real generation |
| `yvex_graph.c` | graph execution | graph command/help | primitives/block/layers/check/guards | scratch/output buffers | graph cleanup | GRAPH.* | large file; future split not required yet |
| `yvex_model_artifacts.c` | model/artifact reports | fullmodel/model-target/models prepare/check | descriptors, target registry, reports | registry/materialization reports | selected/tiny release paths | FULLMODEL.*, FAMILY.RUNTIME, operator presets | needs row-to-test map |
| `yvex_artifact*.c` | artifact IO/integrity | inspect/tensors/materialize/integrity | parse, digest, range, corruption | artifact-owned buffers | materialization cleanup | ARTIFACT.* | keep |
| `yvex_backend.c`, `cuda/*` | backend | backend/cuda-info/ops | CPU/CUDA allocation/op paths | backend tensors | backend release | L0, GRAPH.OPS.*, V010.BACKEND | CUDA machine variance |
| `gguf/*` | GGUF tooling | template/emit/convert/quant | parser/emitter/plans | file buffers | parser/tool cleanup | OWI.*, V010.ARTIFACT | keep |
| `yvex_prefill.c`, `yvex_kv.c`, `yvex_decode.c`, `yvex_logits.c`, `yvex_sampling.c` | runtime stages | prefill/kv/decode/logits/sample | bounded diagnostic stage state | stage structs | stage cleanup/report | PREFILL/KV/DECODE/LOGITS/SAMPLING | no full-runtime path |
| `yvex_eval.c`, `yvex_bench.c` | future evidence | none mature | responsibility headers | none | none | EVAL/BENCH planned | future implementation only |
| `include/yvex/*` | public API headers | C declarations | domain contracts | by domain | by domain | public API map | expand in SPINE.FILEMAP.0 |

#### 10.12 Architecture Weak Point Ledger

| Priority | Weak point | Evidence | Risk | Required follow-up row | Blocks |
| --- | --- | --- | --- | --- | --- |
| P1 high | Current Capability mixes implementation, report-only, diagnostic runtime, selected-slice, docs-only/meta, and cleanup-quality bullets | 200 bullets audited; meta rows appear in capability list | readers may treat docs-only rows as runtime capability | `SPINE.RECONCILE.0` | `MOE.CLASS.0` sequencing clarity |
| P1 high | Meta-spine rows remain in unified ledger and Current Capability | meta table added but rows still appear in ledger | capability inflation | `SPINE.META.TABLE.0` | future ledger cleanup |
| P1 high | Planned rows are duplicated across old namespaces and V010 rows | 166 planned rows; supersession map large | future agents may pick stale row | `SPINE.RECONCILE.0` | V010 execution clarity |
| P1 high | Tests are strong but not mapped row-by-row to ledger rows | shell/unit coverage exists by domain | hard to audit completion automatically | `SPINE.TESTMAP.0` | future row promotion |
| P2 medium | Source/header ownership exists but is not committed as a full file-to-row map | audit-file-list has 246 files | ownership ambiguity for large files | `SPINE.FILEMAP.0` | maintainability |
| P2 medium | `yvex_model_artifacts.c`, `yvex_graph.c`, and `yvex_runtime.c` remain large multi-responsibility files | source ownership scan | hard review surface | future in-place cleanup only | none immediate |
| P2 medium | Fullmodel report-only surfaces could be mistaken for runtime readiness | command output says unsupported but Current Capability is flat | claim-risk | `SPINE.RECONCILE.0` | v0.1.0 target clarity |
| P2 medium | Diagnostic generation has rich proof and may be mistaken for full generation | generate command emits generated diagnostic tokens | claim-risk | `SPINE.RECONCILE.0` | public claim clarity |
| P2 medium | Output-head/tokenizer real-runtime path is underdeveloped relative to v0.1.0 gates | V010.LOGITS/TOKENIZER planned | v0.1.0 target may block later | V010.CLASS.12-13 / V010.TOKENIZER.* | full runtime |
| P2 medium | Full-runtime candidate is not identified | V010.TARGET.9 complete / blocked-no-candidate | release path has no executable target boundary | V010.TARGET.2 | v0.1.0 release path |
| P2 medium | CUDA validation depends on host availability | CUDA tests are available but hardware-dependent | audit variance | CI/HARDWARE profile row | CUDA proof consistency |
| P3 low | Public docs use `$HOME/lab/models` examples | path scan | acceptable operator-local example | keep | none |
| P3 low | Claim scans flag internal target strings in `docs/spine.md` | claim-risk scan | expected internal target language | keep | none |
| P3 low | Some help surfaces have long diagnostic flags | help audit | normal path exists but verbose | future CLI polish | none |
| P3 low | Eval/bench skeletons are present but intentionally future | source scan | could be mistaken as capability without spine | keep unsupported boundary | none |
| P3 low | Artifact fixture list contains GGUF files | artifact guardrail | tiny fixtures only | keep | none |

#### 10.13 Current Capability Rewrite Recommendations

| Current Capability group | Recommended classification | Keep / split / move | Reason |
| --- | --- | --- | --- |
| Parser/tooling/artifact integrity/materialization | implemented command behavior | keep | evidence-backed commands/tests/failures exist |
| Graph primitive/block/layer/selected-slice execution | implemented selected-slice graph/runtime | keep split | avoid full-transformer implication |
| Prefill/KV/decode/logits/sample/generate diagnostic chain | implemented diagnostic runtime | keep split | bounded runtime closure only |
| Fullmodel/family/attention/KV/context reports | implemented report-only surfaces | keep split | report-only, not runtime execution |
| Operator presets and path/target commands | implemented operator presets | keep | command proof and tests exist |
| V010, speculative, Qwen/Metal, sequence, contract, audit doctrine | implemented docs/doctrine/meta spine | move | docs-only-valid, not runtime capability |
| In-place compression/source quality bullets | implemented cleanup/compression | move | quality/history, not capability |
| Unsupported/not advanced list | unsupported / explicitly not advanced | keep | important claim boundary |
| Future V010/post-v0.1.0 rows | planned but not implemented | keep planned | not Current Capability |

`SPINE.RECONCILE.0` rewrote Current Capability into these grouped sections and
kept the remaining row-to-test and file-map detail as non-blocking follow-up
work.

#### 10.14 Required Follow-Up Rows

| Follow-up row | Reason | Priority | Should run before MOE.CLASS.0? |
| --- | --- | --- | --- |
| `SPINE.RECONCILE.0 — Current Capability and ledger reconciliation` | P1 Current Capability and old/V010 namespace mismatch | P1 | complete |
| `SPINE.META.TABLE.0 — Move spine/meta rows into separate meta table` | merged into `SPINE.RECONCILE.0` through `## 5.1 Meta-Spine and Rebase Waves` | P1 | no |
| `SPINE.TESTMAP.0 — Test-to-ledger coverage map` | completion proof is summarized but not row-addressable enough | P1 | no |
| `SPINE.FILEMAP.0 — Source ownership map` | full source/header ownership map is not committed | P2 | no |
| `SPINE.COMMAND.AUDIT.0 — Command/help/output-boundary audit` | command evidence is summarized; source/help changes are not required by this wave | P2 | no |
| `SPINE.CLAIM.AUDIT.0 — Public/internal claim audit` | public docs pass now; future release needs formal audit transcript | P3 | no |
| `MOE.CLASS.0 — MoE model-class report` | original runtime active next remains valid after reconciliation | P1 | after reconciliation |

#### 10.15 Reconciliation Actions

| Finding ID | Priority | Original finding | Reconciliation action | Status after SPINE.RECONCILE.0 | Follow-up row |
| --- | --- | --- | --- | --- | --- |
| AUDIT.P1.1 | P1 | Current Capability mixes implementation, report-only, diagnostic runtime, selected-slice, docs-only/meta, and cleanup-quality bullets | Split Current Capability into evidence-stage groups with explicit boundaries | resolved-in-spine | none |
| AUDIT.P1.2 | P1 | Meta-spine rows remain in unified ledger and Current Capability | Added meta-spine table with runtime capability change set to none and moved docs/meta capability into its own group | resolved-in-spine | none |
| AUDIT.P1.3 | P1 | Planned rows are duplicated across old namespaces and V010 rows | Added forward namespace rules and block-to-V010 crosswalk; preserved history without making old namespaces preferred | resolved-in-spine | none |
| AUDIT.P1.4 | P1 | Tests are strong but not mapped row-by-row to ledger rows | Added test-to-evidence crosswalk and assigned full row mapping to a planned follow-up | requires-follow-up | SPINE.TESTMAP.0 |
| AUDIT.P2.1 | P2 | Source/header ownership exists but is not committed as a full file-to-row map | Kept grouped file audit and assigned complete source/header/test map to follow-up | requires-follow-up | SPINE.FILEMAP.0 |
| AUDIT.P2.2 | P2 | `yvex_model_artifacts.c`, `yvex_graph.c`, and `yvex_runtime.c` remain large multi-responsibility files | Recorded as maintainability pressure only; no source split in this docs wave | deferred-to-runtime | future in-place cleanup only if needed |
| AUDIT.P2.3 | P2 | Fullmodel report-only surfaces could be mistaken for runtime readiness | Moved fullmodel surfaces to report-only group and command crosswalk with unsupported boundary | resolved-in-spine | none |
| AUDIT.P2.4 | P2 | Diagnostic generation has rich proof and may be mistaken for full generation | Moved generation loop to diagnostic-runtime group and command crosswalk with full-generation exclusions | resolved-in-spine | none |
| AUDIT.P2.5 | P2 | Output-head/tokenizer real-runtime path is underdeveloped relative to v0.1.0 gates | Preserved as runtime gap under block/V010 crosswalk and Active Next decision grammar | deferred-to-runtime | V010.CLASS.12-13 / V010.TOKENIZER.* |
| AUDIT.P2.6 | P2 | Full-runtime candidate is not identified | V010.TARGET.9 resolved the decision as blocked-no-candidate; candidate discovery/evaluation is now explicit | deferred-to-target | V010.TARGET.2 |
| AUDIT.P2.7 | P2 | CUDA validation depends on host availability | Preserved as validation/environment boundary in test crosswalk | partially-resolved | V010.HARDWARE.*, V010.CI.3 |
| AUDIT.P2.8 | P2 | Audit summary counted one P2 beyond the individually expanded weak-point table | Assigned unexpanded audit detail to row/test/file mapping follow-ups instead of inventing a runtime blocker | requires-follow-up | SPINE.TESTMAP.0 / SPINE.FILEMAP.0 |
| AUDIT.P3.GROUP | P3 | `$HOME/lab/models` examples, internal target strings, long diagnostic flags, future eval/bench skeletons, and tiny GGUF fixtures are low-risk audit notes | Kept as non-blocking guardrail notes; no runtime or public claim change | partially-resolved | V010.CI.*, V010.DOCS.PUBLIC.* |

#### 10.16 Reconciled Active Next Decision

SPINE.AUDIT.0 moved Active Next to SPINE.RECONCILE.0 because audit found P1/P2
reconciliation needs.

SPINE.RECONCILE.0 resolves the spine-structure and evidence-taxonomy part of
those findings.

Remaining follow-up rows are useful, but they do not block runtime planning:

```text
SPINE.TESTMAP.0:
  planned row-to-test expansion, not required before the next report-only
  runtime-planning row.

SPINE.FILEMAP.0:
  planned full file ownership expansion, not required before the next target
  decision row.
```

After `SPINE.OUTPUT.UX.CONTRACT.0` completed as a docs/control row, Active
Next advances to:

```text
V010.CLI.17 - normal output contract and layout baseline
```

If any P1 finding remains blocking, Active Next becomes the named follow-up row.

| Condition | Active Next |
| --- | --- |
| CLI output UX contract complete and diagnostic walls still dominate normal output | V010.CLI.17 |
| Qwen/Metal pressure report complete and Qwen source facts are missing | V010.SOURCE.9 |
| dense candidate report complete and no eligible dense full-runtime candidate exists | V010.TARGET.7 |
| Current Capability still overclaims implementation | SPINE.CAPABILITY.REWRITE.0 |
| command/help boundaries still overclaim | SPINE.COMMAND.AUDIT.0 |
| test-to-ledger coverage remains blocking | SPINE.TESTMAP.0 |
| public docs claim risk remains blocking | SPINE.PUBLIC.CLAIM.0 |

Current reconciliation result:

```text
Active Next:
  V010.CLI.17 - normal output contract and layout baseline

Reason:
  Normal operator output is not yet separated from diagnostic/audit output.
  V010.CLI.17 is the implementation interlock before returning to
  V010.SOURCE.9. V010.TARGET.7 remains complete; Qwen source target, manifest,
  native inventory, source config, and model-class facts remain the next
  target/source blocker after the CLI output layout baseline.
```


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
real QKV projection from model tensors
real attention-backed KV writes
real KV reads by decode
full transformer prefill
full model decode
real output-head logits-producing runtime path
real vocabulary sampling
stochastic sampling
full model generation
real DeepSeek generation
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
| selected-slice | bounded real artifact slice, not full model |
| source-only | source/intake/storage/model-class only |
| not full model generation | no real full transformer generation path |
| not DeepSeek generation | no DeepSeek V4 Flash real generation |
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

The appendix preserves long-form doctrine and historical reference text. The
front half of the spine is architecture and planning; this section is for deep
rules and guardrails.

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

### 1. Authority

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

### 1.1 Engine Identity

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

### 1.2 How to Read This Spine

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

### 2. Implementation Doctrine

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

### Qwen Metal Pressure Lane Doctrine

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

### 2.1 Canonical Block Directory

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

### 2.2 Naming and Ownership Rules

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

### 2.3 Procedural Implementation Order

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

### 2.4 Conceptual Command Taxonomy

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

### 2.5 Operator Preset Doctrine

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

### 2.6 Forward Namespace Rules

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

### 2.7 Doctrine Navigation Index

This index keeps the detailed doctrine reachable without making it the primary
roadmap.

| Doctrine area | Primary location | Use |
| --- | --- | --- |
| Implementation order | `## 2.3 Procedural Implementation Order` | Choose the next proof boundary. |
| Command taxonomy | `## 2.4 Conceptual Command Taxonomy` | Keep command surfaces staged and honest. |
| Operator presets | `## 2.5 Operator Preset Doctrine` | Keep normal operator paths short and preset-driven. |
| Forward namespaces | `## 2.6 Forward Namespace Rules` | Name future rows without reviving legacy row families. |
| Algorithm and research CLI | `## 11. Doctrine Appendix` | Inspect detailed reference material. |
| Active roadmap | `## 6.0 Forward Runtime Track Matrix` | Pick the next implementation lane. |
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

### 3.10 Detailed Current State Archive

The following archived details preserve the earlier repository state and current capability bullets. They remain evidence detail, not the primary navigation surface.

### 3. Current Repository State

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

### 4. Current Capability

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

### DeepSeek Generation and Throughput Target

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

### Speculative Decoding Reference Track

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

### Execution and Residency Modes

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

### Runtime Closure Doctrine

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

### Generation Loop Contract

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

### Tensor Collections

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

### Attention, KV, and Context Rules

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

### MoE and Expert Activation Rules

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

### Build, Backend, and Hardware Profile Rules

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

### 9. Non-Negotiable Rules

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


### 13.18 Previous Appendix Router

### 11. Doctrine Appendix

Appendix-level doctrine: the detailed reference material remains in place for
historical continuity. Use this appendix as a router; use `## 0` and `## 6.0`
for primary navigation and forward planning.

### 11.1 Paper and Algorithm Doctrine

Primary location: `## Paper-backed Algorithm Doctrine`.

Use for paper/reference staging, algorithm support vocabulary, and the rule
that citations do not imply implementation.

### 11.2 Algorithm Families

Primary location: `## Algorithm Families`.

Use for attention, prefill, KV, decode, logits, sampling, generation-loop, and
residency/storage-stream family boundaries.

### 11.3 CLI Research Surface Matrix

Primary location: `## CLI Research Surface Matrix`.

Use for future research command shapes and output-field expectations. Command
shapes remain conceptual until parser, behavior, output contract, tests,
failure paths, cleanup, and claim boundary exist.

### 11.4 Generation Contracts

Primary locations: `## Generation Loop Contract`, `## Generation State Machine`,
`## Generation Token Lifecycle`, `## Generation Stop Reasons`,
`## Generation CLI Output Contract`, `## Generation Trace Contract`, and
`## Generation Failure and Cleanup Contract`.

Use for diagnostic generation semantics, full-model generation requirements,
trace levels, stop reasons, cancellation boundaries, cleanup, and failure
reporting.

### 11.5 Operator Command Contracts

Primary location: `## Operator Preset Command Contracts`.

Use for path configuration, model target path resolution, prepare/check
presets, graph check suites, diagnostic chat, and runbook expectations.

### 11.6 Non-Negotiable Rules

Primary location: `## 9. Non-Negotiable Rules`.

Use for claim boundaries, artifact hygiene, public-doc restrictions, row
promotion rules, benchmark rules, and source ownership constraints.
