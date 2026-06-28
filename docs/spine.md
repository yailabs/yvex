# YVEX Inner Delivery Spine

Date: 2026-06-26
Status: internal roadmap
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 1. Authority

`docs/spine.md` is the only internal delivery map. Public docs must not expose
delivery IDs, delivery status rows, handoff language, or implementation diary
text.

Status changes in this file require code, tests, command proof, failure-path
coverage, cleanup/lifecycle behavior, and explicit limits. The spine must not
convert parser success, tensor residency, engine ownership, fixture graph
execution, real graph execution, prefill, KV, decode, logits, sampling, or
generation into one vague inference milestone.

Git history should tell the technical story in natural subjects. Internal spine
IDs may appear here and in final reports, but commit subjects should describe
behavior such as `runtime: execute deterministic fixture graphs`, not milestone
labels or scaffold names.

## 2. Implementation Doctrine

No scaffold milestone is complete. A file, API shape, command option, placeholder
executor, future hook, or empty provider stub is not an implementation boundary
unless the same wave also provides real behavior that depends on it.

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

Future work must keep operator paths separate from debug and CI paths. Normal
operator commands should be short, discoverable, and preset-driven where
possible. Long flags belong to diagnostics, gates, provenance, and exact CI
checks. Help should show the short path first and advanced flags later.

DeepSeek selected embedding is the current live pressure artifact. It is large
enough to force real parser, layout, backend, transfer, cleanup, and failure
boundaries. It is not the whole system. Runtime code should remain
model-family-aware, with family mapping and runtime adapters made explicit
rather than hidden behind generic support claims.

`materialize` is a low-level residency verb. It copies selected tensor bytes
from parsed artifact storage into backend-owned runtime storage and proves byte
accounting, transfer, cleanup, and error behavior. It is not model preparation,
graph execution, inference, or generation. Future operator-facing preparation
commands should wrap registry checks, artifact checks, backend checks,
materialization, gates, qtype policy, and report generation behind concise
presets.

## 3. Current Repository State

```text
root-first C source layout
native root binaries: ./yvex and ./yvexd
public headers: include/yvex/
CUDA implementation: cuda/ with C host bridge and CUDA kernel unit
GGUF domain and family mapping: gguf/
docs: docs/api.md, docs/contract.md, docs/operator-runbook.md, docs/spine.md
public README: prose-first runtime boundary, public-safe artifact wording
artifact docs: operator-local paths only, no personal absolute paths
tests: compact runners, fixtures, and vectors
generated output: build/
local operator state: .yvex/
```

Documentation roles:

```text
README.md: public technical thesis and project overview
docs/api.md: public C API surface, headers, ownership, error/capability map
docs/contract.md: runtime/CLI/filesystem/backend/server behavior contract
docs/operator-runbook.md: command-first operator workflow
docs/spine.md: internal delivery map
```

## 4. Current Capability

Implemented and audited:

```text
GGUF inspection
metadata and tensor table parsing
tensor/model descriptor construction
tokenizer fixture path
prompt rendering diagnostics
graph/planner substrate
CPU backend selected materialization
CUDA probe, tensor movement, kernel unit, and CPU/CUDA parity subset
controlled GGUF emission
selected-tensor GGUF emission
DeepSeek selected embedding alias and selected materialization
model gate and materialization gate diagnostics
model-gate pass for active selected artifact with CPU/CUDA required
materialize-gate pass for active selected artifact with repeat and cleanup
local model registry
model alias resolution for one-shot commands
canonical REPL selected-model state
yvexd explicit model alias resolution
qtype support separated by policy/storage/emit/quantize/compute
quant-policy fixture validation
quant-job fixture validation
imatrix fixture validation
server/provider status shell
canonical operator runbook
engine-owned selected materialized weight attachment
session visibility into engine-attached weight state
deterministic fixture graph execution over controlled F32 weights
fixture embed-node backend dispatch
CPU fixture graph execution
CUDA fixture graph parity when CUDA is available
real selected embedding partial graph execution
real token_embd.weight F16 participation in scheduled graph work
CPU real partial graph execution
CUDA real partial graph parity when CUDA is available
artifact integrity validator baseline
GGUF structural corruption detection
tensor range and checked byte-count validation
required selected embedding readiness check
GGUF structural corruption fixture suite
table-driven corrupt fixture refusal harness for integrity/inspect/tensors/materialize/graph
canonical tensor byte-range validation
validated tensor absolute/end offsets
partial embedding token-slice range validation
materialization and graph range guards
canonical tensor shape/dtype accounting
selected embedding shape readiness validation
storage dtype versus compute support separation
byte-count overflow hardening
file identity digest enforcement
registered alias digest verification
baseline registry alias digest drift detection
model-gate/materialize-gate digest enforcement
registry metadata drift diagnostics
registered primary tensor summary comparison
selected embedding readiness drift detection
support-level readiness comparison
materialization integrity gate
materialization preflight before backend allocation
materialization failure phase reporting
materialization cleanup verification
graph execution integrity guard
graph preflight before backend dispatch
reference read and token-slice guards
graph failure phase reporting and cleanup verification
consolidated artifact integrity regression harness
integrity/materialization/graph refusal matrix
cleanup and repeat regression coverage
operator integrity report
integrity report aggregation across artifact/identity/metadata/materialization/graph
```

Current live target:

```text
alias: deepseek4-v4-flash-selected-embed
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU materialization: pass
CUDA materialization: pass
execution_ready: false
```

DeepSeek selected embedding remains the active pressure artifact. The active
target can change if another large open-weight artifact becomes a better fit for
the hardware profile, model-family work, or research path, but that requires an
explicit spine change.

Unsupported / not advanced:

```text
full model execution
full DeepSeek materialization
full GGUF conversion
full supply-chain security
prompt-backed runtime prefill
minimal KV runtime
decode
logits-producing runtime path
sampling
generation
interactive generation
provider generation endpoint
OpenAI-compatible generation
Anthropic-compatible generation
evaluation suite
inference benchmarks
benchmark performance
advanced Runtime KV capacity
execution_ready: true
```

## 5. Inner Delivery Spine

The main table records major completed and active milestones. Detailed planned
work belongs in the track sections that follow; do not turn this table into an
unbounded spreadsheet.

| ID | Status | Title |
| --- | --- | --- |
| P0 | complete | Repository reset and technical spine |
| A0 | complete | Core C skeleton and public status API |
| B0 | complete | Runtime filesystem and artifact path layer |
| C0 | complete | GGUF parser and tensor directory |
| C1 | complete | Tensor/model descriptor layer |
| D0 | complete | Tokenizer and prompt diagnostics |
| E0 | complete | Graph and planner substrate |
| F0 | complete | CPU backend and tensor movement |
| G0 | complete | Runtime/session shell |
| H0 | complete | CLI diagnostics surface |
| I0 | complete | Chat diagnostic shell |
| J0 | complete | Metrics, traces, and run artifacts |
| K0 | complete | Server/provider status shell |
| L0 | complete | CUDA backend probe and tensor movement |
| OWI.0 | complete | Open weight intake and GGUF toolchain spine |
| OWI.1 | complete | Source manifest provenance contract |
| OWI.2 | complete | Safetensors native weight inventory |
| OWI.3 | complete | GGUF template contract validator |
| OWI.4 | complete | Tensor mapping adapter contract |
| OWI.5 | complete | Quantization policy manifest |
| OWI.6 | complete | Calibration imatrix contract |
| OWI.7 | complete | Controlled GGUF emission roundtrip |
| OWI.8 | complete | Open weight conversion bridge |
| OWI.9 | complete | DeepSeek quantization job bridge |
| OWI.FINAL.0 | complete | Open-weight intake closeout |
| RUNTIME.KV.0 | complete | KV cache policy |
| M1 | complete | Real model conversion/materialization gate |
| ARTIFACT.NAMING.0 | complete | GGUF artifact naming contract |
| M2 | complete | DeepSeek materialization hardening |
| CLI.PACKAGE.0 | complete | Repository CLI packaging baseline |
| REPO.OPERATING.0 | complete | Operating handbook and artifact cards |
| CLI.CONSOLE.0 | complete | CLI interface doctrine |
| CLI.MODELS.0 | complete | Local model selection spine |
| CLI.MODELS.1 | complete | Local model registry implementation |
| CLI.MODELS.2 | complete | One-shot model alias resolution |
| CLI.MODELS.3 | complete | Model selection in canonical REPL |
| CLI.MODELS.4 | complete | Model alias resolution in yvexd |
| DOCS.PUBLIC.0 | complete | Public documentation boundary cleanup |
| CLI.PACKAGE.1 | complete | Minimal compiled-binary packaging baseline |
| REPO.LAYOUT.1 | complete | Root-first C source layout collapse |
| REPO.LAYOUT.2 | complete | Root source compression and native root binaries |
| DOCS.MIN.0 | complete | Minimal documentation surface |
| REPO.SURFACE.0 | complete | CUDA, GGUF, model family, and test surface refoundation |
| CUDA.SURFACE.0 | complete | CUDA kernel translation unit |
| REPO.SURFACE.1 | complete | Natural C surface and code style refoundation |
| CODE.NATURAL.0 | complete | Natural translation unit rewrite |
| CODE.NATURAL.1 | complete | Final translation unit hygiene pass |
| TEST.SURFACE.0 | complete | Test vectors and runner consolidation |
| SPINE.REBASE.1 | complete | Execution-chain audit and M3-M8 technical rebase |
| DOCS.OPERATOR.RUNBOOK.0 | complete | Canonical operator runbook |
| SPINE.REBASE.2 | complete | Runtime track rebase before M3 |
| M3 | complete | Materialized-weight engine attachment |
| M4 | complete | First executable fixture graph path |
| SPINE.REBASE.3 | complete | End-to-end runtime and operator roadmap |
| M5 | complete | First real-model partial graph execution |
| SPINE.REBASE.4 | complete | Artifact integrity and measurement target rebase |
| ARTIFACT.INTEGRITY.0 | complete | Artifact integrity threat model and validator baseline |
| ARTIFACT.INTEGRITY.1 | complete | File identity and digest enforcement |
| ARTIFACT.INTEGRITY.2 | complete | GGUF structural corruption fixture suite |
| ARTIFACT.INTEGRITY.3 | complete | Tensor directory offset and byte-range validation |
| ARTIFACT.INTEGRITY.4 | complete | Shape, rank, dtype, and byte-count overflow hardening |
| ARTIFACT.INTEGRITY.5 | complete | Registry alias metadata drift diagnostics |
| ARTIFACT.INTEGRITY.6 | complete | Materialization integrity gate |
| ARTIFACT.INTEGRITY.7 | complete | Graph execution integrity guard |
| ARTIFACT.INTEGRITY.8 | complete | Corrupt artifact regression harness |
| ARTIFACT.INTEGRITY.9 | complete | Operator integrity report and doctor integration |
| ARTIFACT.INTEGRITY.FINAL.0 | complete | Artifact integrity closeout before graph expansion |
| M6 | next | Real-model graph segment expansion |
| M7 | planned | Prompt/token input boundary |
| M8 | planned | Prefill state foundation |
| M9 | planned | Minimal KV ownership and append/read boundary |
| M10 | planned | Decode step over existing runtime state |
| M11 | planned | Logits production boundary |
| M12 | planned | Deterministic logits regression |
| M13 | planned | Sampling boundary |
| M14 | planned | First constrained generation loop |
| M15 | planned | Interactive CLI generation path |
| M16 | planned | Provider/server generation boundary |
| M17 | planned | Trace/profile hardening for generation |
| MODEL.LIFECYCLE.* | planned | Concise model preparation and checking |
| CLI.UX.* | planned | Operator command simplification and terminal UX |
| SERVER.* | planned | Runtime-backed provider work after runtime generation |
| EVAL.* | planned | Correctness/evaluation vectors by runtime boundary |
| BENCH.* | planned | Benchmarks only after stable runtime paths |
| KV.MIN.* | planned | Minimal KV correctness and lifecycle |
| RUNTIME.KV.* | planned | Advanced post-generation KV capacity work |
| BACKEND.PROFILE.* | planned | Hardware/backend profile reporting |
| LAYOUT.* | planned | Repository/module boundary cleanup |
| DOCS.* | planned | Public docs refresh after executable milestones |

## 6. Runtime Ladder Dependencies

```text
source/native artifact evidence
  -> selected artifact identity
  -> artifact integrity and corruption safety
  -> materialized backend tensor
  -> engine/session ownership
  -> fixture graph execution
  -> real selected embedding partial graph execution
  -> larger real-model graph segments
  -> prompt/token input boundary
  -> prefill state
  -> minimal KV ownership
  -> decode
  -> logits
  -> logits regression
  -> sampling
  -> constrained generation
  -> CLI generation
  -> provider/server generation
  -> traces/profiles
```

The separation matters. Real partial graph execution is not prefill. Prefill is
not decode. Decode is not logits regression. Logits are not sampling. Sampling
is not generation UX. Interactive CLI generation is not server/provider
generation. Provider compatibility is not basic provider status. Benchmarking is
not correctness.

Artifact integrity is not optional once scheduled graph work reads real tensor
ranges. M5 proved real selected tensor participation. The artifact integrity and
corruption safety rung is now implemented through ARTIFACT.INTEGRITY.0-.9 and
FINAL.0. Larger real-model graph segments may proceed only while preserving
those gates.

### Inference Runtime Pipeline

```text
M3 — complete — Materialized-weight engine attachment
Attach selected materialized weights to engine/session ownership without
claiming graph execution. Defines ownership boundaries between artifact
descriptor, tensor table, backend tensor storage, engine state, and session
lifecycle.

M4 — complete — First executable fixture graph path
Execute a deterministic tiny fixture graph over controlled F32 weights. Proves
planned embed-node ordering, backend dispatch, output allocation/readback,
independent expected-output comparison, CPU execution, CUDA parity, and failure
reporting on small non-model fixtures.

M5 — complete — First real-model partial graph execution
Execute a constrained selected token-embedding graph segment using real attached
F16 token_embd.weight. Proves real selected tensor participation in scheduled
graph work, backend dispatch, F16-to-F32 output, independent raw-artifact
reference comparison, CPU execution, CUDA parity where available, command proof,
and cleanup/failure tests. No prompt prefill, KV runtime, decode, logits,
sampling, generation, or benchmark claim.

M6 — planned — Real-model graph segment expansion
Expand from one partial segment to a larger scheduled segment with multiple real
ops, intermediate scratch/output buffers, and explicit memory plan. Failure
reports must name the failing op, tensor, backend, or runtime stage. Still no
prefill, logits, sampling, generation, or benchmark claim.

M7 — planned — Prompt/token input boundary
Connect tokenizer/prompt diagnostics to runtime input tensors, sequence
positions, model context, and session-owned input state. Prompt input may reach
the scheduled graph boundary, but this is not prefill completion unless M8 is
also complete.

M8 — planned — Prefill state foundation
Run prompt-token input through scheduled graph work to produce prefill state
boundaries. Graph scratch and intermediate state must be owned and cleaned up.
If KV or logits are not complete, the output must say so.

M9 — planned — Minimal KV ownership and append/read boundary
Introduce minimal session-owned KV shape, allocation, append/read, and lifecycle
needed for prefill/decode. KV state must be inspectable or summarizable, and
cleanup plus context-overflow failure paths must be tested. This is separate
from advanced paged, spilled, or quantized KV.

M10 — planned — Decode step over existing runtime state
Execute one decode step over existing runtime/KV state. The next-token
computation must advance runtime state by one position. No sampling and no
interactive generation claim.

M11 — planned — Logits production boundary
Produce logits through an implemented runtime path and expose deterministic
diagnostics. Logits buffer ownership, dtype, and backend tolerance rules must be
explicit. Logits are not sampling and not generation quality.

M12 — planned — Deterministic logits regression
Add stable vector tests for logits, including tolerance, dtype/qtype boundaries,
backend parity where available, and model/artifact identity for each regression
vector.

M13 — planned — Sampling boundary
Implement sampling as a separate boundary over logits. Greedy and at least one
stochastic mode need explicit behavior. Seed/reproducibility rules and
invalid-parameter failures must be tested. Sampling is not model execution.

M14 — planned — First constrained generation loop
Bounded loop: decode -> logits -> sample -> append token. Stop conditions,
token accounting, traces, and unsupported edge cases must be explicit.

M15 — planned — Interactive CLI generation path
Expose generation through CLI/REPL only after constrained generation is real.
The normal operator command must be simple, debug flags must remain available,
and fake assistant text must not appear outside the real generation path. Line
editing, slash commands, and display polish come after the runtime path.

M16 — planned — Provider/server generation boundary
Expose generation through daemon/server only after CLI/runtime generation is
real. Server state must be runtime-backed, request/response failure modes must
be tested, and streaming or compatibility APIs are not claimed unless
implemented. Provider compatibility is a later contract, not a status-shell
property.

M17 — planned — Trace/profile hardening for generation
Make generation observable and debuggable through traces, profiles, metrics,
runtime summaries, and failure reports. Errors should identify the artifact,
backend, graph, KV, sampling, or server boundary that failed.
```

## 7. Tracks

### Core Runtime

```text
parser/model/tokenizer/graph/backend/session shell implemented
materialization produces backend-resident tensors
engine owns attached selected materialized weights
session can observe engine weight attachment state
deterministic fixture graph execution complete
real selected embedding partial graph execution complete
larger real-model graph execution not implemented
prefill/decode/logits/sampling/generation not implemented
```

M6 must expand real scheduled computation beyond the selected embedding segment.
It must not become a vague inference milestone. Completion requires multiple real
ops, backend dispatch, explicit intermediate/output memory planning,
output/regression proof, and cleanup/failure tests.

M5 now reads a real selected F16 tensor slice for scheduled partial graph work.
That makes artifact integrity a runtime prerequisite before M6 expands graph
coverage. M6 should not broaden real tensor participation until artifact
identity, tensor range validation, shape/dtype math, and corruption failure
paths are mapped into the spine and at least the baseline validator is
implemented.

### Open Weight Intake / Model Family Flow

Implemented sequence:

```text
source manifest
native safetensors inventory
GGUF template validation
family tensor mapping
selected GGUF emission
qtype support / quant policy
quant-job manifest
imatrix manifest
artifact naming
registry add/use
materialization
engine attachment
fixture graph execution
real graph execution
```

Planned intake/operator work:

| ID | Status | Title |
| --- | --- | --- |
| OWI.CLI.0 | planned | Concise intake command layout |
| OWI.CLI.1 | planned | Family adapter report |
| OWI.CLI.2 | planned | Selected artifact build/check preset |
| OWI.CLI.3 | planned | Full-model conversion planning report |
| OWI.RUNTIME.0 | planned | Runtime family adapter boundary for partial graph execution |

Rules:

```text
full-model conversion remains unsupported until implemented
selected conversion is not full conversion
family mapping is not runtime support
a registered artifact is not execution readiness
DeepSeek selected embedding remains the active live pressure artifact
another large open-weight artifact may become active only through explicit spine change
```

### Artifact Integrity / Corruption Safety

Artifact integrity is the safety layer between open-weight intake and runtime
execution. It is not a general security product and it is not a replacement for
cryptographic supply-chain policy. It is the native runtime guarantee that an
artifact can be parsed, identified, bounded, materialized, and read for graph
execution without trusting inconsistent metadata, invalid ranges, overflowing
shape math, stale aliases, or corrupted tensor payload boundaries.

After M5, this track becomes urgent. YVEX now executes a selected embedding
segment over real F16 token_embd.weight. Before M6 expands real graph execution,
the runtime needs a corruption and identity-hardening path that exercises the
same parser, registry, materializer, engine attachment, and graph execution
boundaries.

Open Weight Intake creates and describes artifacts. Artifact Integrity proves
that artifacts remain structurally safe and identity-stable. Runtime graph
expansion should trust broader tensor participation only after that bridge is
mapped and the baseline validator exists.

| ID | Status | Title |
| --- | --- | --- |
| ARTIFACT.INTEGRITY.0 | complete | Artifact integrity threat model and validator baseline |
| ARTIFACT.INTEGRITY.1 | complete | File identity and digest enforcement |
| ARTIFACT.INTEGRITY.2 | complete | GGUF structural corruption fixture suite |
| ARTIFACT.INTEGRITY.3 | complete | Tensor directory offset and byte-range validation |
| ARTIFACT.INTEGRITY.4 | complete | Shape, rank, dtype, and byte-count overflow hardening |
| ARTIFACT.INTEGRITY.5 | complete | Registry alias metadata drift diagnostics |
| ARTIFACT.INTEGRITY.6 | complete | Materialization integrity gate |
| ARTIFACT.INTEGRITY.7 | complete | Graph execution integrity guard |
| ARTIFACT.INTEGRITY.8 | complete | Corrupt artifact regression harness |
| ARTIFACT.INTEGRITY.9 | complete | Operator integrity report and doctor integration |
| ARTIFACT.INTEGRITY.FINAL.0 | complete | Artifact integrity closeout before graph expansion |

ARTIFACT.INTEGRITY.0 defined the artifact integrity threat model and validator
baseline. YVEX now rejects baseline structural corruption before
materialization or graph execution: truncated files, bad magic/version,
duplicate tensor names, missing required selected embedding tensors, unsupported
dtype accounting, rank/dim overflow, invalid tensor offsets, and tensor byte
ranges outside file bounds. The validator has command-visible output, tiny
corrupt fixture coverage, selected embedding readiness checks, and docs
describing what is checked and what is not.

ARTIFACT.INTEGRITY.1 enforces local file identity and digest expectations across
the model registry, integrity command, model-gate, materialize-gate, and
selected-artifact execution paths. `models add` records file size, SHA-256,
GGUF summary, tensor count, and primary tensor summary. `models verify`,
`integrity check --expect-sha256`, and the gates report expected/current digest
status. Registered alias paths that materialize, attach engine/session weights,
or execute graph segments fail before backend allocation or graph execution when
the current bytes no longer match the recorded digest. This is local identity
evidence only, not supply-chain security or remote provenance.

ARTIFACT.INTEGRITY.2 created a GGUF structural corruption fixture suite and
table-driven refusal harness. The suite covers bad magic, unsupported version,
truncated header, truncated metadata, truncated tensor directory, invalid
counts, malformed string fields, duplicate and empty tensor names, invalid
rank, zero dimensions, dimension overflow, unknown dtype, tensor offset/range
failures, misalignment, and missing selected embedding readiness. The harness
proves clean rejection across integrity, inspect, tensors, materialize, and
graph partial surfaces, while preserving the distinction between structurally
corrupt artifacts and structurally valid artifacts that are not ready for
selected embedding execution.

ARTIFACT.INTEGRITY.3 centralizes tensor directory offset and byte-range
validation. YVEX now has one canonical range helper for element count, dtype
size, tensor byte count, `tensor_data_offset` plus tensor-relative offset,
absolute offset, end offset, file bounds, and alignment. Integrity, tensor
listing, materialization, and real partial graph paths consume those validated
range facts. The selected embedding partial graph also validates the requested
token slice before raw reference extraction.

ARTIFACT.INTEGRITY.4 hardens shape, rank, dtype, and byte-count accounting.
YVEX now has canonical tensor shape/dtype accounting before byte-range
validation and payload reads. The validator rejects unsupported rank, zero
dims, element-count overflow, dtype-size ambiguity, tensor byte-count overflow,
and selected embedding shape/dtype incompatibility. Selected embedding readiness
records the interpreted hidden size, vocabulary size, output count, output byte
count, and selected-token slice byte size for `token_embd.weight`. Storage dtype
recognition remains separate from runtime compute support.

ARTIFACT.INTEGRITY.5 adds higher-level registry metadata drift diagnostics after
the digest baseline. `models add` now records support level, architecture, tensor
count, known tensor bytes, primary tensor name/role/dtype/rank/dims/bytes, and
selected embedding readiness facts. `models verify` compares those registered
facts against the current artifact and reports `metadata_status` and
`readiness_status` separately from `digest_status`. Safety-critical alias paths
such as materialization and partial graph execution fail when metadata drift
invalidates registered assumptions.

ARTIFACT.INTEGRITY.6 adds a materialization integrity gate. Materialization now
composes structural integrity, digest identity, registry metadata drift,
shape/dtype accounting, tensor range validation, selected tensor readiness, and
backend availability into a preflight before backend allocation. The command
reports the materialization phase, allocation and transfer attempts, cleanup
attempts, cleanup status, and byte accounting. Corrupt or inconsistent artifacts
fail before backend allocation where possible; injected allocation/transfer
failures prove cleanup reporting.

ARTIFACT.INTEGRITY.7 adds a graph execution integrity guard. Fixture graph and
real selected embedding partial graph execution now run graph-specific preflight
before backend dispatch. The guard composes structural integrity, digest
identity, registry metadata drift, shape/dtype accounting, tensor range
validation, selected token-slice validation, backend availability, backend op
support, output allocation sizing, and reference read bounds. Preflight failure
prevents dispatch; post-preflight failures report the graph phase and cleanup
status.

ARTIFACT.INTEGRITY.8 adds a consolidated corrupt artifact regression harness.
The harness generates a local matrix under build output and exercises
structural corruption, digest drift, metadata drift, materialization gate
failures, graph guard failures, cleanup, and repeat behavior across integrity,
registry, inspect, tensors, materialize, materialize-gate, graph fixture, graph
partial, engine, and session surfaces. It is regression coverage, not fuzzing or
complete malicious-input coverage.

ARTIFACT.INTEGRITY.9 exposes an operator integrity report and future-compatible
doctor integration point. The report composes artifact structure, digest
identity, registry metadata drift, shape/dtype accounting, tensor range
validation, selected embedding readiness, materialization preflight, and
graph-entry guard status for implemented paths. It is local operator evidence,
not supply-chain security, model quality validation, full model support, or
inference readiness.

ARTIFACT.INTEGRITY.FINAL.0 closes Artifact Integrity as a prerequisite module
for M6. The module now covers structural validation, digest identity, registry
metadata drift, corruption fixtures, tensor range validation, shape/dtype
accounting, materialization preflight, graph-entry guards, operator reporting,
and regression coverage. This does not imply supply-chain security, full model
execution, inference readiness, generation, logits, or benchmarks.

### Model Support Ladder

```text
controlled GGUF emission proven
selected-tensor materialization proven
DeepSeek selected embedding is the active live target
model-gate and materialize-gate pass on CPU/CUDA
engine attachment complete
fixture graph execution complete
real selected embedding partial graph execution complete
full model materialization not reached
larger real-model graph execution not reached
execution/prefill/decode/logits/sampling/generation not reached
execution_ready remains false
```

DeepSeek is the current pressure target because it is large and real. YVEX must
remain model-family-aware rather than one-model ad hoc. Family-specific mapping
belongs in explicit adapters and reports.

### Minimal KV Correctness

Minimal KV state belongs inside the inference ladder because prefill/decode need
runtime state. Advanced capacity work belongs later.

| ID | Status | Title |
| --- | --- | --- |
| KV.MIN.0 | planned | Minimal KV shape and ownership for prefill state |
| KV.MIN.1 | planned | KV append/read boundary for decode |
| KV.MIN.2 | planned | KV lifecycle in session state |
| KV.MIN.3 | planned | KV diagnostics and failure reporting |

Rules:

```text
KV.MIN.* is correctness/runtime-state work
RUNTIME.KV.* is capacity/performance/long-context work
do not mix them
minimal KV does not imply paged KV, host spill, or KV quantization
```

### Runtime KV Capacity

| ID | Status | Title |
| --- | --- | --- |
| RUNTIME.KV.0 | complete | KV cache policy |
| RUNTIME.KV.1 | planned | Static KV size estimator |
| RUNTIME.KV.2 | planned | CUDA KV allocation proof |
| RUNTIME.KV.3 | planned | GPU paged KV allocator skeleton |
| RUNTIME.KV.4 | planned | Host RAM spill and cold-cache experiments |
| RUNTIME.KV.5 | planned | KV quantization policy |

Advanced static estimators, CUDA KV allocation proof, paged KV, host spill,
cold-cache behavior, and KV quantization are post-first-generation capacity
work unless this spine explicitly pulls a minimal subset forward. KV runtime,
paged KV, host spill, and KV quantization are not implemented.

### Model Lifecycle / Preparation

Low-level commands stay available for debug, CI, and exact gate checks. Normal
operator paths need a concise lifecycle layer above them.

| ID | Status | Title |
| --- | --- | --- |
| MODEL.LIFECYCLE.0 | planned | Unified model status and readiness report |
| MODEL.LIFECYCLE.1 | planned | Model prepare preset over materialization and gates |
| MODEL.LIFECYCLE.2 | planned | Model check preset for artifact, registry, backend, qtype, and CUDA state |
| MODEL.LIFECYCLE.3 | planned | Model doctor flow for common operator failures |
| MODEL.LIFECYCLE.4 | planned | Model-class profile for large local artifacts |
| MODEL.LIFECYCLE.5 | planned | Artifact cache and report hygiene |

Planned operator surface may be singular `yvex model status|prepare|check|doctor`
or an extension of the existing `yvex models` namespace. The taxonomy must be
decided before implementation. Normal users should not need the full
`model-gate` or `materialize-gate` flag set for common checks.

### CLI / Operator UX

Current complete state:

```text
root binaries complete
model registry complete
one-shot alias resolution complete
REPL selected-model state complete
chat can use current selected registry alias when --model is omitted
explicit --model remains supported
yvexd explicit --model alias resolution complete
daemon current-selected model behavior not implemented
```

Planned UX work:

| ID | Status | Title |
| --- | --- | --- |
| CLI.UX.0 | planned | Command taxonomy and help layout |
| CLI.UX.1 | planned | Concise normal-path commands over low-level diagnostics |
| CLI.UX.2 | planned | Colorized terminal output and status severity |
| CLI.UX.3 | planned | Structured output modes for scripts |
| CLI.UX.4 | planned | Interactive REPL line editing and history |
| CLI.UX.5 | planned | REPL slash-command cleanup and discoverability |
| CLI.UX.6 | planned | Doctor command for environment, registry, backend, CUDA, and artifacts |
| CLI.UX.7 | planned | Operator profiles for workstation and future larger hardware |
| CLI.UX.8 | planned | One-line command recipes for normal paths |

Principles:

```text
normal commands should fit on one line when possible
long flags are reserved for debug/CI/gate exactness
commands should expose defaults and presets
help must show the short path first and advanced flags later
terminal output should distinguish pass/warn/fail/unsupported clearly
color support must degrade with NO_COLOR or non-TTY output
machine-readable output must remain uncolored
```

### Server / Provider Runtime

Current server state:

```text
yvexd status shell implemented
health/metrics/models endpoints implemented
explicit --model alias resolution implemented
generation endpoints unsupported
provider compatibility unsupported
```

Planned server work:

| ID | Status | Title |
| --- | --- | --- |
| SERVER.RUNTIME.0 | planned | Runtime-backed model state in daemon |
| SERVER.RUNTIME.1 | planned | Daemon execution-state diagnostics without generation |
| SERVER.GEN.0 | planned | First provider generation endpoint after constrained generation |
| SERVER.API.0 | planned | OpenAI-compatible API boundary after generation exists |
| SERVER.API.1 | planned | Anthropic-compatible API boundary after generation exists |
| SERVER.STREAM.0 | planned | Streaming response boundary after generation exists |
| SERVER.OBS.0 | planned | Server traces, metrics, and failure reports |

Provider generation comes after CLI/runtime generation. Compatibility APIs come
after basic provider generation. Streaming comes after a stable generation loop.
No server compatibility claim is allowed before tests and command/API proof.

### Eval / Bench

Planned correctness/evaluation rows:

| ID | Status | Title |
| --- | --- | --- |
| EVAL.FIXTURE.0 | planned | Fixture graph correctness vectors |
| EVAL.PARTIAL.0 | planned | Real partial graph regression vectors |
| EVAL.PREFILL.0 | planned | Prefill state regression |
| EVAL.KV.0 | planned | KV append/read correctness vectors |
| EVAL.DECODE.0 | planned | Decode-step regression |
| EVAL.LOGITS.0 | planned | Deterministic logits regression |
| EVAL.SAMPLING.0 | planned | Sampling determinism and distribution checks |
| EVAL.GEN.0 | planned | Constrained generation smoke/eval |

Planned benchmark rows:

| ID | Status | Title |
| --- | --- | --- |
| BENCH.PREFILL.0 | planned | Prefill throughput benchmark |
| BENCH.DECODE.0 | planned | Decode throughput benchmark |
| BENCH.MEMORY.0 | planned | Runtime memory pressure benchmark |
| BENCH.RUNTIME.0 | planned | End-to-end runtime benchmark harness |

Dependencies:

```text
EVAL.FIXTURE.0 follows M4
EVAL.PARTIAL.0 follows M5/M6
EVAL.PREFILL.0 follows prefill implementation
EVAL.KV.0 follows minimal KV implementation
EVAL.DECODE.0 follows decode step
EVAL.LOGITS.0 follows logits boundary
EVAL.SAMPLING.0 follows sampling boundary
EVAL.GEN.0 follows constrained generation
BENCH.* follows stable implemented runtime path
```

Rules:

```text
no benchmark score before benchmark implementation
no token/sec table before prefill/decode exists
no quality claim before evaluation vectors exist
evaluation must use the same runtime path users run
fixture eval is not model quality eval
logits regression is not generation quality
speed numbers without model artifact, backend, quant, context, machine, command,
and reproducibility note are invalid
do not create a fake DS4Bench equivalent
```

Future tool names may be `yvex eval` and `yvex bench`, but they are planned
only.

#### Measurement Targets / Performance Posture

Performance is a first-class project target, but speed numbers are not valid
until the relevant runtime path exists. YVEX should plan prefill and generation
measurements now, while refusing to publish token/sec numbers before prefill,
decode, logits, sampling, and generation are implemented.

| Measurement target | Runtime prerequisite | Metric shape | Status |
| --- | --- | --- | --- |
| Fixture graph correctness | M4 | output values/checksum | available |
| Selected embedding partial graph correctness | M5 | output checksum/sample/max diff | available |
| Real segment regression | M6 | output checksum/vector diff | planned |
| Prefill speed | M8/M9 | prompt tokens/sec, memory, scratch, KV state | planned |
| Decode speed | M10/M11 | generated step/sec or token/sec after logits path | planned |
| Generation speed | M14 | tokens/sec, latency, stop reason, context length | planned |
| Provider latency | M16 | request latency, streaming latency, queue behavior | planned |

| Prompt class | Size | Why it matters | Valid after |
| --- | --- | --- | --- |
| short | `<512` tokens | interactive baseline | prefill/decode |
| medium | `~10k` tokens | practical coding/document context | prefill/decode |
| long | `32k+` tokens | long-context runtime pressure | prefill/KV/decode |
| very long | `100k+` tokens | future large-context target | KV capacity + generation |

| Machine profile | Role | Status |
| --- | --- | --- |
| CPU correctness host | reference/debug | current correctness baseline |
| Local CUDA workstation | primary acceleration development target | current CUDA path |
| DGX Spark / GB10 | future CUDA profile | planned, no support claim |
| Large-memory CUDA host | future larger artifact/runtime target | planned |

These tables define measurement shape, not performance claims. Target token/sec
values, quality scores, or comparative hardware statements are invalid until the
matching runtime path, command, artifact identity, and reproducibility note
exist.

### Backend / Hardware Profiles

| ID | Status | Title |
| --- | --- | --- |
| BACKEND.PROFILE.0 | planned | CPU correctness profile |
| BACKEND.PROFILE.1 | planned | Local CUDA workstation profile |
| BACKEND.PROFILE.2 | planned | DGX Spark / GB10 CUDA profile |
| BACKEND.PROFILE.3 | planned | Large-memory future hardware profile |
| BACKEND.PROFILE.4 | planned | Backend failure and memory pressure reports |
| BACKEND.PROFILE.5 | planned | Backend capability matrix |

CPU remains the correctness baseline. CUDA remains the primary acceleration
backend. Future hardware profiles are planning targets, not support claims. No
Metal or ROCm claim is allowed unless explicitly implemented or added to the
spine with tests.

### Layout / Repository Architecture

| ID | Status | Title |
| --- | --- | --- |
| LAYOUT.RUNTIME.0 | planned | Runtime module boundary audit |
| LAYOUT.GRAPH.0 | planned | Graph/executor module separation |
| LAYOUT.CLI.0 | planned | CLI command taxonomy cleanup |
| LAYOUT.SERVER.0 | planned | Server/runtime boundary cleanup |
| LAYOUT.TEST.0 | planned | Test fixture/eval/bench directory separation |
| LAYOUT.DOCS.0 | planned | Public/internal docs boundary after spine retirement |
| LAYOUT.BUILD.0 | planned | Build targets by backend and hardware profile |

The internal spine may eventually leave the public repository. If that happens,
public docs must preserve the implemented contract, runbook, API surface, and
capability boundaries without exposing delivery IDs. Source layout should follow
runtime ownership boundaries: CLI must not own backend/runtime logic, server code
must not duplicate CLI runtime wiring, and tests should separate parser fixtures,
runtime fixtures, evaluation vectors, and benchmarks.

### Docs / Operator Surface

| ID | Status | Title |
| --- | --- | --- |
| DOCS.README.2 | planned | Refresh README after executable graph and CLI simplification |
| DOCS.RUNBOOK.1 | planned | Generic model-class operator flow |
| DOCS.RUNBOOK.2 | planned | Reduced-flag normal path and preset workflow |
| DOCS.RUNBOOK.3 | planned | Debug and failure-mode cookbook |
| DOCS.RUNBOOK.4 | planned | Verticalized operator runbook |
| DOCS.CONTRACT.1 | planned | Update runtime contract after real partial graph execution |
| DOCS.API.1 | planned | Update public API map after inference pipeline boundaries mature |
| DOCS.PUBLIC.1 | planned | Public documentation without internal delivery IDs |
| DOCS.README.TARGETS.0 | planned | README measurement target table after benchmark harness |
| DOCS.DIAGRAMS.* | planned | Runtime and measurement diagrams |

Public docs stay role-specific:

```text
README explains what YVEX is and what is real now
operator-runbook shows command execution
contract states behavioral guarantees
api maps public C surfaces
spine remains internal until replaced by a public roadmap or removed
```

The current runbook is command-correct but flag-heavy and DeepSeek-focused. It
should evolve into layered usage:

```text
normal path:
  build -> model add/use -> model prepare/check -> chat/run/serve

debug path:
  inspect -> tensors -> materialize -> gates -> traces

intake path:
  source manifest -> native inventory -> tensor map -> selected emit

CI path:
  gate commands with full explicit flags

large-model path:
  hardware/backend check -> memory report -> prepare/check -> runtime
```

The operator runbook should be verticalized into normal path,
selected-artifact build/refresh, deterministic fixture proof, real partial graph
proof, debug/gates, daemon/status, chat diagnostics, quant/provenance,
validation/hygiene, and failure checklist.

### Documentation Diagrams / Visual Roadmap

| ID | Status | Title |
| --- | --- | --- |
| DOCS.DIAGRAMS.0 | planned | Artifact-to-runtime boundary diagram |
| DOCS.DIAGRAMS.1 | planned | Operator path diagram |
| DOCS.DIAGRAMS.2 | planned | Runtime ladder diagram |
| DOCS.DIAGRAMS.3 | planned | Integrity and corruption-failure diagram |
| DOCS.DIAGRAMS.4 | planned | Eval/bench measurement map |
| DOCS.DIAGRAMS.5 | planned | Backend and hardware target matrix |
| DOCS.DIAGRAMS.6 | planned | README visual integration |

Diagrams should not be decorative. Each diagram must explain a boundary that the
runtime enforces: artifact identity, materialization, engine ownership, graph
execution, integrity failures, eval/bench measurement points, or operator flow.
No diagram may imply support that the code does not implement.

## 8. Active Next

```text
M6 - Real-model graph segment expansion
```

Next implementation: M6. It sits inside the larger runtime pipeline. It must
expand from the selected embedding segment to a larger scheduled real-model
graph segment with multiple real ops, backend dispatch, explicit
intermediate/output memory planning, output/regression proof, and
cleanup/failure tests. It must not claim
prompt/prefill, KV runtime, logits, sampling, generation, server generation,
evaluation, or benchmark readiness.

## 9. Validation Gate

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

Execution-chain audit set:

```sh
./yvex gguf-emit controlled --out /tmp/yvex-controlled.gguf --overwrite
./yvex inspect /tmp/yvex-controlled.gguf
./yvex tensors /tmp/yvex-controlled.gguf
./yvex materialize --model /tmp/yvex-controlled.gguf --backend cpu
./yvex materialize --model /tmp/yvex-controlled.gguf --backend cuda
./yvex graph --model /tmp/yvex-controlled.gguf --backend cpu --execute-fixture --fixture-token 0
./yvex graph --model /tmp/yvex-controlled.gguf --backend cuda --execute-fixture --fixture-token 0
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
./yvex qtype-support
```

Baseline artifact integrity audit set:

```sh
./yvex integrity check --model <valid-fixture>
./yvex integrity check --model <corrupt-fixture>
./yvex inspect <corrupt-fixture>
./yvex tensors <corrupt-fixture>
./yvex materialize --model <corrupt-fixture> --backend cpu
./yvex graph --model <corrupt-fixture> --backend cpu --execute-partial --partial-token 0
```

The integrity command, corrupt fixture refusals, materialize refusal, and graph
refusal are current ARTIFACT.INTEGRITY.0 validation requirements. Digest and
alias identity checks are current ARTIFACT.INTEGRITY.1 validation requirements:
`models add`, `models verify`, `integrity check --expect-sha256`,
model/materialize gate digest mismatch, and stale-alias refusal before
materialization or graph execution.

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

This proof set preserves the M5 selected embedding checksum boundary while
checking identity, metadata, integrity report aggregation, materialization
preflight, and graph-entry guard behavior. It remains selected embedding partial
execution only.

When the operator-local selected artifact and CUDA host are available,
`model-gate` and `materialize-gate` must run against the active selected artifact
with CPU/CUDA required. `materialize-gate` should include repeat and cleanup
verification for hardening passes.

Additional guardrails:

```text
public docs boundary
build/bin public-doc path guard
artifact guardrail
forbidden external reference guardrail
claim guardrail
local registry guardrail
public path leak guardrail
```

## 10. Non-Negotiable Rules

- No support claim without code, tests, and command proof.
- No generated model artifacts in git.
- No personal absolute artifact paths in public docs.
- No internal delivery IDs outside `docs/spine.md`.
- No inference or generation claim until implemented.
- No benchmark claim without benchmark implementation and proof.
- No Benchmark/Eval status promotion without implemented vectors, harnesses, or
  command proof at the matching runtime boundary.
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
- No CLI or server generation surface before the lower runtime generation loop
  exists.
- No provider compatibility claim before server generation exists and is tested.
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
- Do not claim advanced Runtime KV capacity work until the relevant allocator,
  estimator, spill, or quantization behavior exists in code and tests.
- No docs sprawl beyond `docs/api.md`, `docs/contract.md`,
  `docs/operator-runbook.md`, and `docs/spine.md`.
- Keep DeepSeek as the active live model target unless this spine changes.
- Keep Qwen as historical validation evidence unless this spine changes.
