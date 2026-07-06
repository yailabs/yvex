# v0.1.0 Release Doctrine

## 0. Authority

docs/v010-release-doctrine.md is the canonical v0.1.0 release doctrine.

docs/spine.md is the active delivery map.

If a later row, command, report, public doc, release note, target registry entry,
CLI status, eval result, or benchmark result conflicts with this doctrine, this
doctrine wins until a later TRACK.SCOPE row changes it.

Doctrine does not create runtime capability.

## 1. Release Identity

v0.1.0 is not the first model that runs.

v0.1.0 is the first release where every supported generation family in {DeepSeek, Qwen, Gemma} has an evidence-backed YVEX path from source identity to generated tokens.

Required release path:

```text
source identity
source evidence
tensor map
qtype support
YVEX-produced artifact
artifact identity
artifact integrity
materialization
residency
runtime descriptor
graph
prefill
KV
decode
logits
sampling
tokenizer/stop boundary
generation
eval
benchmark
claim-safe release transcript
```

v0.1.0 is not:

```text
one-model demo
chat UX milestone
provider compatibility milestone
external-runner parity
paper-reference milestone
benchmark page without runtime
```

## 2. Supported Family Set

Supported v0.1.0 generation families:

```text
DeepSeek
Qwen
Gemma
```

DeepSeek is required.

Qwen is required.

Gemma is required.

No single family can close v0.1.0.

DeepSeek cannot close v0.1.0 alone.

Qwen cannot close v0.1.0 alone.

Gemma cannot close v0.1.0 alone.

Qwen is a v0.1.0 supported generation family.

Qwen/Metal is post-v0.1.0 backend portability.

GLM is source/storage pressure unless a later TRACK.SCOPE row promotes it.

GLM is not in the v0.1.0 supported family set.

## 3. Closure Rule

v0.1.0 closes only when every supported family passes every required gate.

Required closure chain:

```text
source target identity
official source acquisition or accepted local source evidence
source manifest
native tensor inventory
source tensor metadata inventory
model class profile
tensor collection inventory
tensor role mapping
output-head mapping
tokenizer metadata mapping
missing-role blocker closure
mapping gate
qtype policy
dtype/qtype support by runtime role
qtype compute/refusal matrix
calibration/imatrix decision where required
generation-capable YVEX-produced artifact
artifact identity
artifact integrity
artifact registration
materialization plan
materialization proof
residency plan
backend capability
backend qtype compute/refusal support
runtime descriptor readiness
transformer graph
transformer prefill
KV writes
KV reads
runtime decode
output-head logits
vocabulary sampling
tokenizer/stop boundary
runtime generation loop
CLI generation command
eval smoke/regression
benchmark transcript
claim audit
release transcript
```

A gate closed for one family does not close that gate for another family.

## 4. Supported Family Meaning

A family is supported for v0.1.0 generation only after it passes source, map,
quant, artifact, integrity, materialization, residency, runtime descriptor,
graph, prefill, KV, decode, logits, sampling, tokenizer, generation, eval,
benchmark, and release gates.

Non-equivalences:

```text
A source profile is not family support.
A tensor map is not family support.
A qtype policy report is not family support.
A selected runtime slice is not family support.
A diagnostic generation loop is not family support.
A generation-capable artifact is not family support.
An external GGUF is not family support.
An external runner is not family support.
```

Family-specific boundaries:

```text
DeepSeek selected slices are not DeepSeek generation support.
Qwen source/profile/map reports are not Qwen generation support.
Gemma source/profile/map reports are not Gemma generation support.
GLM source/storage pressure is not GLM generation support.
Qwen/Metal pressure is not Qwen closure and not Metal support.
```

## 5. Artifact Versus Runtime Generation

generation-capable artifact:

```text
YVEX-produced quantized artifact with required family metadata, tensor roles,
qtype support, tokenizer/output-head coverage, identity, integrity, and
registration.
```

runtime generation:

```text
YVEX runtime execution over that artifact through prefill, KV, decode, logits,
sampling, append, stop policy, cleanup, eval, and benchmark evidence.
```

generation-capable artifact is not runtime generation.

Boundaries:

```text
Artifact emission is not decode.
Artifact identity is not materialization.
Materialization is not graph execution.
Graph execution is not generation.
Diagnostic generation is not runtime generation.
CLI generation syntax is not runtime generation.
```

## 6. Non-Closing Evidence

selected-runtime-slice proof:
  proves bounded pressure only; it is not full-family runtime.

fixture proof:
  proves synthetic behavior only; it is not supported-family execution.

diagnostic-runtime proof:
  proves diagnostic control flow only; it is not runtime generation.

source-only target report:
  reports source state only; it does not emit or execute an artifact.

header-only tensor inventory:
  reports metadata only; it does not load payloads or map roles.

qtype policy report:
  reports policy basis only; it does not prove per-role support.

external GGUF:
  is comparison evidence only; it is not a YVEX-produced artifact.

external runner output:
  is comparison evidence only; it is not YVEX runtime proof.

paper reference:
  informs design only; it is not implementation evidence.

CLI grammar:
  defines command language only; it is not runtime capability.

CLI porcelain cleanup:
  improves operator output only; it is not runtime capability.

provider status shell:
  reports daemon/provider state only; it is not provider generation.

download receipt:
  records acquisition attempt only; it is not source verification.

artifact discovery report:
  reports file presence only; it is not artifact emission.

benchmark target without measurement:
  reports target intent only; it is not benchmark evidence.

public claim text:
  is documentation only; it is not implementation evidence.

## 7. Target Classes

selected-runtime-slice:
  bounded parser, materialization, graph, or diagnostic pressure target.

supported-generation-family:
  family required to close v0.1.0 generation support.

generation-capable artifact:
  YVEX-produced artifact that passes artifact gates before runtime generation.

source/storage pressure:
  source or storage lane that does not close generation support.

backend portability pressure:
  backend feasibility lane that does not close v0.1.0.

external reference artifact:
  third-party artifact used only for comparison.

external runner reference:
  third-party runtime result used only for comparison.

post-v0.1.0 target:
  explicit future lane outside the current release closure.

Mappings:

```text
DeepSeek:
  supported-generation-family; selected slices are partial pressure only.

Qwen:
  supported-generation-family.

Gemma:
  supported-generation-family; dense-family pressure.

GLM:
  source/storage pressure unless promoted later.

Qwen/Metal:
  backend portability pressure; post-v0.1.0.

External GGUF:
  external reference artifact only.

External runner:
  external runner reference only.
```

## 8. Release Gates

### Minimum Gate Matrix

| Gate | Owner | Scope | Minimum closure evidence | Cannot close from |
| --- | --- | --- | --- | --- |
| GATE.SCOPE | TRACK.SCOPE | release | release doctrine, supported family set, closure rule, non-goals, target class boundaries, readiness vocabulary, forbidden claims | roadmap prose, target pressure, public claims |
| GATE.SOURCE | TRACK.SOURCE | DeepSeek/Qwen/Gemma | stable target identity, accepted local or acquired source evidence, source manifest, native tensor inventory, source tensor metadata inventory | download receipt alone, provider status, external runner, source URL only |
| GATE.MAP | TRACK.MAP | DeepSeek/Qwen/Gemma | tensor role mapping, output-head mapping, tokenizer metadata mapping, missing-role closure, map gate | lexical tensor name scan, source inventory, class profile |
| GATE.QUANT | TRACK.QUANT | DeepSeek/Qwen/Gemma | qtype policy, dtype/qtype support by runtime role, qtype compute/refusal matrix, calibration/imatrix decision where required | qtype policy report alone, storage qtype list, unsupported compute path |
| GATE.ARTIFACT | TRACK.ARTIFACT | DeepSeek/Qwen/Gemma | YVEX-produced generation-capable quantized artifact, required metadata, tokenizer/output-head coverage, artifact registration | external GGUF, selected slice, artifact discovery report, source files |
| GATE.INTEGRITY | TRACK.INTEGRITY | DeepSeek/Qwen/Gemma | identity, digest, metadata parse, tensor directory, byte ranges, dtype/qtype, shape/rank, overflow checks, corruption refusal, registry drift guard | file presence, digest only, selected-slice integrity |
| GATE.MODEL | TRACK.MODEL | DeepSeek/Qwen/Gemma | model class, runtime class requirements, descriptor requirements, dynamic path requirements | family name, source metadata, tokenizer metadata |
| GATE.TENSOR | TRACK.TENSOR | DeepSeek/Qwen/Gemma | full required tensor coverage for embedding, norms, attention, MLP/MoE, output head, tokenizer/runtime metadata | partial collection inventory, missing-role report |
| GATE.RESIDENCY | TRACK.RESIDENCY | DeepSeek/Qwen/Gemma | materialization proof, placement plan, residency proof, movement/failure path, cleanup proof | materialization plan, selected-slice residency, memory estimate |
| GATE.BACKEND | TRACK.BACKEND | backend | required CPU/CUDA operation support or refusal, qtype compute/refusal support, allocation, transfer, scratch, cleanup/failure behavior | backend probe, allocation only, primitive-only proof |
| GATE.GRAPH | TRACK.GRAPH | DeepSeek/Qwen/Gemma | real transformer graph over supported-family artifacts | primitive proof, fixture proof, selected graph slice |
| GATE.PREFILL | TRACK.PREFILL | DeepSeek/Qwen/Gemma | real prefill over supported-family artifacts, owned runtime state, failure path, cleanup | diagnostic prefill, context planner, token input |
| GATE.KV | TRACK.KV | DeepSeek/Qwen/Gemma | real K/V writes during prefill, real K/V reads during decode, indexing, capacity, cleanup/failure behavior | KV shape report, diagnostic KV store, KV trace |
| GATE.DECODE | TRACK.DECODE | DeepSeek/Qwen/Gemma | one real decode step and repeated decode lifecycle over real runtime state | diagnostic decode, decode option parser, position report |
| GATE.LOGITS | TRACK.LOGITS | DeepSeek/Qwen/Gemma | final norm and output-head projection produce real vocabulary logits from runtime state | output-head mapping, logits buffer allocation, top-k diagnostics |
| GATE.SAMPLING | TRACK.SAMPLING | DeepSeek/Qwen/Gemma | token selection over real logits with deterministic proof and failure path | candidate report, temperature validation, diagnostic sampler |
| GATE.TOKENIZER | TRACK.TOKENIZER | DeepSeek/Qwen/Gemma | token input/output contract, EOS, stop-token, prompt/template or explicit token-ID-only boundary, failure behavior | tokenizer metadata report, special token report |
| GATE.GENERATION | TRACK.GENERATION | DeepSeek/Qwen/Gemma | runtime loop composes prefill, KV, decode, logits, sampling, append, stop, cleanup over supported-family artifacts | diagnostic generation, generated token append alone, generation trace |
| GATE.OPERATOR | TRACK.OPERATOR | operator | CLI generation command, status grammar, refusal grammar, normal/audit output, command proof transcript over implemented runtime path | CLI grammar, porcelain cleanup, help text |
| GATE.EVAL | TRACK.EVAL | DeepSeek/Qwen/Gemma | smoke and regression eval over implemented runtime generation path, including failure-path eval | fixture eval, graph primitive eval, planned eval harness |
| GATE.BENCH | TRACK.BENCH | DeepSeek/Qwen/Gemma | measured benchmark transcript with machine, artifact, qtype, context, backend, command, run count, timing, memory, reproducibility metadata | benchmark plan, benchmark target, one unrecorded timing run |
| GATE.RELEASE | TRACK.RELEASE | release | clean checkout, validation transcript, artifact guardrail, family proof, claim audit, docs audit, changelog/release notes, tag readiness | passing tests alone, public claim text, release intent |

### Gate Closure Rules

A gate closes only at the lowest true evidence stage.

A gate cannot close by naming a later row.

A gate cannot close by public wording.

A gate cannot close by external execution.

A gate cannot close by report-only output unless the gate itself is report-only.

Runtime gates require implemented runtime behavior.

Evidence gates require command proof.

Benchmark gates require measured metadata.

Release gates require a clean transcript.

Family-scoped gates close separately for DeepSeek, Qwen, and Gemma.

Release waits for all supported families.

## 9. Readiness Vocabulary

Canonical stages:

```text
unsupported
planned
blocked
report-only
header-only
source-intake
fixture-proof
selected-slice-proof
diagnostic-runtime
generation-capable-artifact-ready
full-runtime-candidate
runtime-generation-ready
eval-ready
benchmark-ready
release-ready
not-measured
```

Use the lowest true stage.

Do not invent synonyms.

Do not write ready without a scoped prefix.

generation-capable-artifact-ready:
  artifact gates pass; runtime generation is still unclaimed.

runtime-generation-ready:
  runtime generation over supported-family artifacts exists.

eval-ready:
  eval cases pass over implemented runtime paths.

benchmark-ready:
  measured benchmark metadata exists.

release-ready:
  every required release gate passes for every supported family.

Forbidden vague readiness language:

```text
inference-ready
model-ready
generation-ready
CUDA-ready
Metal-ready
support-ready
mostly-ready
almost-supported
```

## 10. Forbidden Claims

Forbidden unless explicitly true and gate-backed:

```text
DeepSeek generation: implemented
Qwen generation: implemented
Gemma generation: implemented
multi-family generation: implemented
generation-capable artifacts: emitted
qtype role support: complete
quantization: complete
GGUF full artifact emission: complete
artifact identity for all supported families: complete
materialization/residency for all supported families: complete
runtime descriptors: complete
graph route for supported families: complete
prefill: complete
KV: complete
decode: complete
logits: complete
sampling: complete
tokenizer runtime: complete
runtime generation: complete
eval: implemented
benchmark: measured
throughput: achieved
release_ready true
generation_ready true
inference-ready
model support
Qwen support
Gemma support
DeepSeek support
Metal support
CUDA generation
```

Do not introduce positive versions of those claims outside this section.

## 11. Governed Surfaces

Governed surfaces:

```text
docs/spine.md
docs/v010-release-doctrine.md
docs/contract.md
docs/model-families.md
docs/operator-runbook.md
docs/cli-output-architecture.md
README.md
MODEL_ARTIFACTS.md
AGENTS.md
target registry
model-target decision report
model-target candidate reports
source-manifest reports
quant-policy reports
missing-role reports
models prepare/check/artifacts reports
CLI status/refusal/error grammar
doctor/readiness reports
eval reports
benchmark reports
release notes
```

This row defines the governing doctrine. It does not update every governed
surface.

If drift is found:

```text
If drift is in an allowed file, fix it.
If drift is in code or unrelated docs, report follow-up row.
If drift is a live false positive claim emitted by a command, report blocker.
```

## 12. Change Control

The supported family set can change only through a later TRACK.SCOPE row.

A family can be removed only by explicit scope decision.

A family can be added only if doctrine, capability map, track gates, row
catalog, target registry, docs, tests, and claim boundaries change together.

Post-v0.1.0 lanes cannot silently become v0.1.0 requirements.

Pressure lanes cannot silently become supported-generation-family lanes.

External references cannot become YVEX support evidence without explicit rows.
