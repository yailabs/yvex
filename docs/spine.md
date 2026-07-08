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
The active docs set is docs/api.md, docs/contract.md, docs/model-families.md, docs/operator-runbook.md, docs/cli-output-architecture.md, docs/v010-release-doctrine.md, docs/spine.md.
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
| Active Next | V010.QUANT.2 - qtype compute/refusal matrix |
| v0.1.0 target | v0.1.0 - multi-family supported generation over YVEX-produced quantized artifacts |
| Supported v0.1.0 families | DeepSeek, Qwen, Gemma |
| Main blocker | multi-family generation-capable artifact chain incomplete |

The supported v0.1.0 generation family set is DeepSeek, Qwen, and Gemma.
GLM remains source/storage pressure. GLM remains huge source/storage pressure
unless it is explicitly promoted by a later target decision.
Qwen/Metal remains post-v0.1.0 backend portability.

## 1. Capability Map

This is the v0.1.0 monitoring view from source weights to generated token.
Current implementation state is one column in the map, not the whole map.
Report-only, selected-slice, fixture, and diagnostic rows do not promote
full-runtime generation readiness.

### 1.1 Pipeline Capability Map

| Capability | Track | v0.1.0 requirement | Current stage | Family scope | Complete anchors | Next row/gate | Boundary |
| --- | --- | --- | --- | --- | --- | --- | --- |
| source target identity | TRACK.SOURCE | supported family targets have stable local IDs | complete | DeepSeek/Qwen/Gemma | MODEL.TARGET.IDENTITY.0 | V010.SOURCE.10 | target identity is not runtime support |
| official source acquisition | TRACK.SOURCE | sources can be acquired for supported families | source-intake | DeepSeek/Qwen/Gemma | V010.SOURCE.7A | V010.SOURCE.10 | download is not source verification or generation |
| source manifest | TRACK.SOURCE | source footprint and provenance are recorded | complete | DeepSeek/Qwen/Gemma | V010.SOURCE.0 | V010.SOURCE.10 | manifest is not payload trust |
| native tensor inventory | TRACK.SOURCE | native tensor headers are inventoried | complete | DeepSeek/Qwen/Gemma | V010.SOURCE.5 | V010.SOURCE.10 | header inventory is not payload loading |
| source tensor metadata inventory | TRACK.SOURCE | tensor names, dtype, shape, and spans are recorded | complete | DeepSeek/Qwen/Gemma | V010.SOURCE.6 | V010.SOURCE.10 | metadata is not role mapping |
| model class profile | TRACK.MODEL | supported families have class profiles | report-only | DeepSeek/Qwen/Gemma | MODEL.CLASS.QWEN.0, MODEL.CLASS.GEMMA.0, MOE.CLASS.0 | V010.CLASS.16 | class profile is not runtime routing |
| tensor collection inventory | TRACK.TENSOR | required collection groups are visible | report-only | DeepSeek/Qwen/Gemma | TENSOR.COLLECTION.QWEN.0, TENSOR.COLLECTION.GEMMA.0, TENSOR.MOE.0 | V010.TENSOR.23 | collection inventory is not tensor role support |
| tensor role mapping | TRACK.MAP | native names map to runtime roles | partial | DeepSeek/Qwen/Gemma | V010.MAP.1, V010.MAP.5 | V010.MAP.9 | role map is not artifact emission |
| output-head mapping | TRACK.MAP | output head and final norm are mapped | report-only | DeepSeek/Qwen/Gemma | V010.MAP.6 | V010.LOGITS.3 | mapping is not logits computation |
| tokenizer metadata mapping | TRACK.TOKENIZER | tokenizer sidecars are mapped | report-only | DeepSeek/Qwen/Gemma | V010.MAP.7 | V010.TOKENIZER.12 | metadata is not tokenizer runtime |
| missing-role blocker report | TRACK.MAP | missing runtime roles are visible | complete | DeepSeek/Qwen/Gemma | V010.MAP.8 | V010.MAP.9 | blocker report is not closure |
| mapping gate | TRACK.MAP | map evidence gates artifact planning | complete | DeepSeek/Qwen/Gemma | V010.MAP.9 | V010.QUANT.1 | gate is report-only |
| qtype policy | TRACK.QUANT | qtype policy basis is reported | complete | DeepSeek/Qwen/Gemma | V010.QUANT.0 | V010.QUANT.1 | policy is not support |
| dtype/qtype support by runtime role | TRACK.QUANT | every runtime role has allowed/refused dtype/qtype state | complete/report-only | DeepSeek/Qwen/Gemma | V010.QUANT.1 | V010.QUANT.2 | support report is not quantization or generation |
| qtype compute/refusal matrix | TRACK.QUANT | compute/refusal state exists across families and backends | active | DeepSeek/Qwen/Gemma | V010.QUANT.1 | V010.QUANT.2 | compute matrix is not artifact emission |
| calibration/imatrix decision | TRACK.QUANT | calibration requirements are explicit | planned | DeepSeek/Qwen/Gemma | none | V010.QUANT.3 | imatrix decision is not quantization |
| generation-capable artifact emission | TRACK.ARTIFACT | YVEX emits complete quantized artifacts for supported families | blocked | DeepSeek/Qwen/Gemma | V010.ARTIFACT.EMIT.0, V010.ARTIFACT.EMIT.1 | V010.ARTIFACT.EMIT.2 | artifact emission is not runtime generation |
| artifact identity | TRACK.INTEGRITY | supported-family artifacts have identity manifests | planned | DeepSeek/Qwen/Gemma | selected-slice identity only | V010.INTEGRITY.0 | identity is not execution |
| artifact integrity | TRACK.INTEGRITY | ranges, dtype, qtype, and corruption gates pass | partial | DeepSeek/Qwen/Gemma | current integrity rows | V010.INTEGRITY.13 | integrity is not generation |
| artifact registration | TRACK.ARTIFACT | generation-capable artifacts are registered | planned | DeepSeek/Qwen/Gemma | selected registration only | V010.ARTIFACT.EMIT.5 | registration is not readiness |
| materialization plan | TRACK.RESIDENCY | materialization is planned for supported artifacts | partial | DeepSeek/Qwen/Gemma | current materialization plans | V010.RESIDENCY.19 | plan is not materialization proof |
| materialization proof | TRACK.RESIDENCY | required tensors materialize under limits | selected-slice-proof | DeepSeek/Qwen/Gemma | selected proof only | V010.RESIDENCY.19 | selected proof is not full runtime |
| residency plan | TRACK.RESIDENCY | CPU/CUDA residency is planned for required tensors | planned | DeepSeek/Qwen/Gemma | placement reports | V010.RESIDENCY.19 | residency plan is not backend execution |
| backend capability | TRACK.BACKEND | backend capability is known and refused cleanly | partial | backend | V010.BACKEND.1, V010.BACKEND.2, CUDA.KERNEL.0 | V010.BACKEND.12 | backend capability is not model support |
| backend qtype compute support | TRACK.BACKEND | required qtypes compute or refuse per backend | active | backend | V010.QUANT.1 | V010.QUANT.2 | qtype compute support is not generation |
| runtime descriptor readiness | TRACK.MODEL | runtime descriptors are ready for supported artifacts | planned | DeepSeek/Qwen/Gemma | descriptor reports only | V010.CLASS.16 | descriptor readiness is not execution |
| graph primitive readiness | TRACK.GRAPH | primitive graph ops are covered and bounded | fixture-proof | backend | graph primitive proofs, CUDA.KERNEL.0 | V010.GRAPH.PRIM.10 | primitive readiness is not transformer graph |
| transformer graph | TRACK.GRAPH | role-bearing tensors execute through model graph | planned | DeepSeek/Qwen/Gemma | selected graph only | V010.GRAPH.24 | graph path is not generation |
| transformer prefill | TRACK.PREFILL | transformer prefill writes runtime state | unsupported | DeepSeek/Qwen/Gemma | diagnostic prefill only | V010.PREFILL.15 | diagnostic prefill is not transformer prefill |
| KV writes | TRACK.KV | prefill writes attention-backed K/V | unsupported | DeepSeek/Qwen/Gemma | diagnostic KV only | V010.KV.6 | KV report is not KV write |
| KV reads | TRACK.KV | decode reads attention-backed K/V | unsupported | DeepSeek/Qwen/Gemma | diagnostic KV only | V010.KV.8 | KV read is not implemented |
| runtime decode | TRACK.DECODE | decode advances model-backed runtime state | unsupported | DeepSeek/Qwen/Gemma | diagnostic decode only | V010.DECODE.14 | diagnostic decode is not runtime decode |
| output-head logits | TRACK.LOGITS | final hidden state projects to vocab logits | unsupported | DeepSeek/Qwen/Gemma | diagnostic logits only | V010.LOGITS.16 | output-head mapping is not logits |
| vocabulary sampling | TRACK.SAMPLING | sampler consumes output-head logits | unsupported | DeepSeek/Qwen/Gemma | diagnostic sampling only | V010.SAMPLE.14 | diagnostic sampler is not vocab sampling |
| tokenizer/stop boundary | TRACK.TOKENIZER | stop/EOS/token boundaries are explicit | planned | DeepSeek/Qwen/Gemma | tokenizer metadata reports | V010.TOKENIZER.12 | metadata is not tokenization |
| runtime generation loop | TRACK.GENERATION | prefill, KV, decode, logits, sample, append, stop compose | unsupported | DeepSeek/Qwen/Gemma | diagnostic generation only | V010.GEN.19 | diagnostic generation is not model generation |
| CLI generation command | TRACK.OPERATOR | operator command invokes runtime generation path | planned | operator | diagnostic `generate` | V010.GEN.16 | command surface cannot claim missing runtime |
| operator refusal/status grammar | TRACK.OPERATOR | shared status/refusal/error grammar exists | planned | operator | V010.CLI.26 | V010.CLI.27 | CLI grammar is not lower runtime |
| eval smoke/regression | TRACK.EVAL | supported generation paths have smoke/regression eval | planned | DeepSeek/Qwen/Gemma | none | V010.EVAL.14 | eval waits for runtime path |
| benchmark transcript | TRACK.BENCH | measured transcript records model/artifact/backend/qtype | not-measured | DeepSeek/Qwen/Gemma | none | V010.BENCH.11 | no throughput without benchmark |
| claim audit | TRACK.RELEASE | claims match implemented evidence | planned | release | docs guardrails | V010.RELEASE.5 | claim audit is not capability |
| release transcript | TRACK.RELEASE | v0.1.0 proof transcript is complete | planned | release | none | V010.RELEASE.9 | transcript cannot create behavior |

### 1.2 Supported-Family Capability Matrix

| Family | Source | Map | Quant | Artifact | Integrity | Residency | Runtime descriptor | Graph | Generation | Eval | Benchmark | Release role |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| DeepSeek | selected/source pressure partial | map incomplete for full runtime | blocked at V010.QUANT.2 | no generation-capable artifact | selected integrity only | planned | planned | selected/fixture only | unsupported | planned | not measured | required |
| Qwen | source-intake/report-only | header map partial | blocked at V010.QUANT.2 | no generation-capable artifact | planned | planned | planned | planned | unsupported | planned | not measured | required |
| Gemma | source-intake/report-only | header map partial | blocked at V010.QUANT.2 | no generation-capable artifact | planned | planned | planned | planned | unsupported | planned | not measured | required |

### 1.3 Implemented Capability Snapshot

| Implemented area | Stage | Proof class | Boundary |
| --- | --- | --- | --- |
| artifact parsing and integrity | complete | GGUF parsing, descriptor construction, integrity reports, corruption refusal, materialization gates | not source verification, not generation |
| source intake and manifest/report surfaces | source-intake | source manifests, native header inventories, receipts, provider logs, download controls | no payload loading, no GGUF emission |
| Qwen/Gemma header-derived profile/map/tokenizer/qtype reports | report-only | source profiles, model-class profiles, tensor collection inventory, qtype policy report | no runtime support, no generation |
| selected DeepSeek artifact slices | selected-slice-proof | selected embedding and embedding-plus-RMSNorm artifacts | not full DeepSeek runtime |
| graph primitives and selected graph slices | fixture-proof | RoPE, attention, projection, MLP, routed-expert primitive proofs | not full transformer execution |
| diagnostic prefill/KV/decode/logits/sampling/generation | diagnostic-runtime | bounded diagnostic commands and cleanup/trace paths | not full model generation |
| CUDA primitive hardening | fixture-proof | bounded CUDA MLP and attention primitives compared to CPU/reference outputs | not CUDA runtime generation |
| operator CLI baseline | complete | compact normal output, audit evidence, renderer foundation, base command grammar | not lower runtime capability |
| provider account/download control | source-intake | accounts, live progress, signal-safe interruption, status/stop/resume/cleanup | no source identity verification, no artifact registration |
| docs/claim guardrails | complete | docs surface tests, public-doc boundary checks, artifact guardrail checks | no runtime capability |

## 2. Unsupported Boundaries

Unsupported means unsupported until there is implementation, tests, command
proof, lifecycle behavior, and claim-safe documentation.

| Group | Unsupported boundaries |
| --- | --- |
| Runtime | transformer prefill, attention-backed KV, runtime decode, output-head logits, vocabulary sampling, full runtime generation |
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

Canonical release doctrine: docs/v010-release-doctrine.md

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
V010.QUANT.2 - qtype compute/refusal matrix
```

Why this row is next:

```text
V010.QUANT.1 is complete as a report-only role-support surface.
DeepSeek, Qwen, and Gemma still lack backend compute/refusal decisions for
the reported per-role source dtypes and planned artifact qtypes.
That matrix is required before artifact emission can choose or refuse qtypes.
```

Expected next decision after V010.QUANT.2:

```text
V010.QUANT.3 - calibration/imatrix requirement report
```

The exact next artifact-readiness row may change only if V010.QUANT.2 exposes a
more specific blocker.

Completed quant row:

```text
V010.QUANT.1 - multi-family dtype/qtype support by role
```

`V010.QUANT.1` adds `model-target quant-policy TARGET --role-support` and
`model-target quant-policy --gate v0.1.0` as report-only role-support surfaces
for DeepSeek selected slices and downloaded Qwen/Gemma source targets. The
reports list runtime role, source dtype, source tensor count, planned artifact
qtype candidates/refusals, storage status, compute status, calibration/imatrix
deferral, and artifact-emission blockers without tensor payload loading,
quantization, GGUF emission, materialization, runtime descriptors, graph
execution, generation, eval, benchmark, throughput, or release readiness. The
handoff blocker is now `V010.QUANT.2 - qtype compute/refusal matrix`.

## 5. Tracks

This is the only active track map. It replaces the old ledger, crosswalk,
supersession, audit, and sequence archives.

| Track | Owns | Current state | Complete rows | Next gate | Active |
| --- | --- | --- | --- | --- | --- |
| TRACK.SCOPE | release scope and claim boundaries | active | SPINE.RETARGET.MULTIFAMILY.0, SPINE.TRACK.CANON.0 | scope remains multi-family | no |
| TRACK.SOURCE | source intake, manifests, provider download controls | implemented/source-intake | source-intake rows complete; explicit labels are in the catalog below | source evidence stays coherent | no |
| TRACK.MAP | family tensor roles and source-to-artifact handoff | partial/report-only | Qwen/Gemma class and tensor-map rows, V010.MAP.1/5/6/7/8/9 | artifact handoff | no |
| TRACK.QUANT | dtype/qtype policy and compute/refusal coverage | partial/report-only | V010.QUANT.0, V010.QUANT.1 | V010.QUANT.2 | yes |
| TRACK.ARTIFACT | YVEX-produced quantized artifacts | selected-slice only | controlled/selected artifact emission | multi-family generation-capable artifacts | no |
| TRACK.INTEGRITY | artifact identity, ranges, corruption refusal | implemented for current artifacts | integrity rows and materialization gates | full artifact gates | no |
| TRACK.MODEL | model class and dynamic routing | partial/report-only | Qwen/Gemma model-class reports, MoE reports | runtime class gates | no |
| TRACK.TENSOR | tensor collections and role coverage | partial/report-only | Qwen/Gemma tensor collection and coverage reports | required runtime tensor coverage | no |
| TRACK.RESIDENCY | placement, memory pressure, staged residency | planned/report-only | selected materialization plans | residency gate | no |
| TRACK.BACKEND | CPU/CUDA capability and future backend lanes | partial | CPU/CUDA primitive proofs | backend runtime gate | no |
| TRACK.GRAPH | primitives, selected slices, transformer graph path | partial | graph primitives and selected slices | full transformer graph | no |
| TRACK.PREFILL | transformer prefill and chunking | diagnostic-runtime only | diagnostic prefill rows | prefill gate | no |
| TRACK.KV | KV layout, writes, reads, lifecycle | diagnostic/report-only | diagnostic KV rows | KV gate | no |
| TRACK.DECODE | decode over runtime state | diagnostic-runtime only | bounded diagnostic decode | runtime decode gate | no |
| TRACK.LOGITS | output head and logits | diagnostic-runtime only | bounded diagnostic logits | logits gate | no |
| TRACK.SAMPLING | token selection over logits | diagnostic-runtime only | bounded diagnostic sampler | sampling gate | no |
| TRACK.TOKENIZER | tokenizer metadata and stop policy | partial/planned | tokenizer diagnostics | tokenizer gate | no |
| TRACK.GENERATION | decode/logits/sample/append loop | diagnostic-runtime only | bounded diagnostic generation | runtime generation gate | no |
| TRACK.OPERATOR | CLI grammar, output, porcelain/plumbing commands | implemented/partial | CLI baseline rows complete; explicit labels are in the catalog below | CLI status/refusal grammar | no |
| TRACK.SERVE | daemon and provider surfaces | planned | status shell | serve runtime gate | no |
| TRACK.EVAL | correctness and capability evaluation | planned | tests only | eval gate | no |
| TRACK.BENCH | reproducible performance measurement | planned | none | benchmark gate | no |
| TRACK.RELEASE | validation transcript, claim audit, tag readiness | planned | docs/guardrail checks | release gate | no |
| TRACK.POST010 | portability, serving hardening, speculative, extra families | post-v0.1.0 | doctrine only | post-release selection | no |

### Canonical Row Trackmap

These rows are active wave IDs, not historical ledger entries. Every wave
listed here carries a status and a short ownership description so future work
can name the exact row without restoring delivery history, audit tables,
crosswalks, sequence archives, doctrine dumps, compressed ranges, or wildcard
labels.

#### TRACK.SCOPE Rows

| Wave | Status | Description |
| --- | --- | --- |
| SPINE.RETARGET.MULTIFAMILY.0 | complete | Lock v0.1.0 to DeepSeek, Qwen, and Gemma as the supported generation-family set. |
| SPINE.TRACK.CANON.0 | complete | Replace the oversized active spine with the compact track-first map. |
| SPINE.ACTIVE.REWRITE.1 | superseded | Superseded active-spine rewrite attempt kept only as a naming marker. |
| SPINE.ROW.CATALOG.0 | complete | Restore explicit active row labels without restoring historical ledger content. |
| SPINE.ROW.CATALOG.1 | complete | Promote the row-label catalog into a trackmap with status and description columns. |
| SPINE.CAPABILITY.MAP.0 | complete | Replace the current snapshot with the v0.1.0 pipeline capability map. |
| V010.SCOPE.0 | complete | v0.1.0 release doctrine. |
| V010.SCOPE.1 | complete | v0.1.0 minimum gates. |
| V010.SCOPE.2 | planned | v0.1.0 non-goals. |
| V010.SCOPE.3 | planned | v0.1.0 included track map. |
| V010.SCOPE.4 | planned | v0.1.0 excluded and postponed track map. |
| V010.SCOPE.5 | planned | v0.1.0 target selection policy. |
| V010.SCOPE.6 | planned | v0.1.0 release-readiness vocabulary. |
| V010.SCOPE.7 | planned | v0.1.0 claim boundary map. |
| V010.TARGET.0 | planned | target class registry refresh. |
| V010.TARGET.1 | complete | selected-runtime-slice target report. |
| V010.TARGET.2 | complete | full-runtime-candidate target report. |
| V010.TARGET.3 | complete | dense candidate target report. |
| V010.TARGET.4 | planned | MoE candidate target report. |
| V010.TARGET.5 | planned | DeepSeek pressure target report. |
| V010.TARGET.6 | planned | GLM source-only pressure target report. |
| V010.TARGET.7 | complete | Qwen/Metal pressure target report. |
| V010.TARGET.8 | planned | external reference target report. |
| V010.TARGET.9 | complete | v0.1.0 target decision record. |
| V010.TARGET.10 | planned | target decision refusal and rollback policy. |

#### TRACK.SOURCE Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.SOURCE.0 | planned | source manifest schema refresh. |
| V010.SOURCE.1 | complete | source family/profile fields. |
| V010.SOURCE.2 | complete | source artifact class fields. |
| V010.SOURCE.3 | complete | source shard count and footprint report. |
| V010.SOURCE.4 | complete | source provenance fields. |
| V010.SOURCE.5 | complete | native safetensors inventory. |
| V010.SOURCE.6 | complete | source tensor metadata inventory. |
| V010.SOURCE.7 | complete | source manifest/provenance hardening. |
| V010.SOURCE.8 | planned | GLM source pressure report. |
| V010.SOURCE.9 | complete | Qwen source pressure report. |
| V010.SOURCE.10 | planned | v0.1.0 source acceptance gate. |
| V010.SOURCE.7A / MODELS.DOWNLOAD.0 | complete | Add native source tensor download under the models namespace. |
| V010.SOURCE.7B / ACCOUNTS.PROVIDER.0 | complete | Add local provider account preflight for Hugging Face and GitHub. |
| MODELS.DOWNLOAD.LIVE.0 | complete | Expose live/plain/log/off source download progress modes. |
| MODELS.DOWNLOAD.SIGNAL.0 | complete | Preserve partial source state across interrupted downloads. |
| MODELS.DOWNLOAD.CONTROL.0 | complete | Add download status, stop, resume, and explicit cleanup controls. |
| MODELS.SOURCE.IDENTITY.0 | complete | Make downloaded source targets visible to downstream source commands. |
| MODELS.SOURCE.MAP.HANDOFF.0 | complete | Hand downloaded Qwen/Gemma targets into existing map surfaces. |
| MODELS.SOURCE.ROLEMAP.COVERAGE.0 | complete | Report dynamic downloaded target role coverage from header evidence. |
| OWI.TARGETS.QWEN.0 | complete | Expose the Qwen source target profile. |
| OWI.TARGETS.GEMMA.0 | complete | Expose the Gemma source target profile. |
| MODEL.TARGET.IDENTITY.0 | complete | Use backend-neutral source target IDs for Qwen and Gemma. |

#### TRACK.MAP Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.MAP.0 | planned | tensor mapping schema. |
| V010.MAP.1 | complete | dense tensor naming map. |
| V010.MAP.2 | planned | MoE tensor naming map. |
| V010.MAP.3 | planned | DeepSeek tensor naming map. |
| V010.MAP.4 | planned | GLM tensor naming map. |
| V010.MAP.5 | complete | Qwen tensor naming map. |
| V010.MAP.6 | complete | output-head tensor mapping. |
| V010.MAP.7 | complete | tokenizer metadata mapping. |
| V010.MAP.8 | complete | missing-role blocker report. |
| V010.MAP.9 | complete | v0.1.0 tensor mapping gate. |

#### TRACK.QUANT Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.QUANT.0 | complete | qtype policy report. |
| V010.QUANT.1 | complete | multi-family dtype/qtype support by runtime role. |
| V010.QUANT.2 | active | qtype compute/refusal matrix. |
| V010.QUANT.3 | planned | calibration/imatrix requirement report. |

#### TRACK.ARTIFACT Rows

| Wave | Status | Description |
| --- | --- | --- |
| MODELS.ARTIFACTS.LIST.0 | complete | List/status local GGUF artifact presence without emitting new artifacts. |
| V010.ARTIFACT.EMIT.0 | complete | controlled artifact emission. |
| V010.ARTIFACT.EMIT.1 | complete | selected artifact emission. |
| V010.ARTIFACT.EMIT.2 | planned | generation-capable quantized artifact emission. |
| V010.ARTIFACT.EMIT.3 | planned | split artifact plan. |
| V010.ARTIFACT.EMIT.4 | planned | artifact parse roundtrip. |
| V010.ARTIFACT.EMIT.5 | planned | artifact registration. |
| V010.ARTIFACT.EMIT.6 | planned | v0.1.0 artifact production gate. |

#### TRACK.INTEGRITY Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.INTEGRITY.0 | complete | artifact identity manifest. |
| V010.INTEGRITY.1 | complete | size/digest gate. |
| V010.INTEGRITY.2 | complete | metadata parse gate. |
| V010.INTEGRITY.3 | complete | tensor directory gate. |
| V010.INTEGRITY.4 | complete | tensor byte-range gate. |
| V010.INTEGRITY.5 | complete | shape/rank/dtype gate. |
| V010.INTEGRITY.6 | complete | element count/overflow gate. |
| V010.INTEGRITY.7 | planned | qtype support gate. |
| V010.INTEGRITY.8 | complete | registry drift gate. |
| V010.INTEGRITY.9 | complete | corruption fixture regression. |
| V010.INTEGRITY.10 | complete | materialization preflight gate. |
| V010.INTEGRITY.11 | planned | graph integrity gate. |
| V010.INTEGRITY.12 | planned | runtime integrity gate. |
| V010.INTEGRITY.13 | planned | v0.1.0 integrity acceptance gate. |

#### TRACK.MODEL Rows

| Wave | Status | Description |
| --- | --- | --- |
| MODEL.CLASS.QWEN.0 | complete | Profile Qwen model class from header and sidecar metadata. |
| MODEL.CLASS.GEMMA.0 | complete | Profile Gemma model class from header and sidecar metadata. |
| MOE.CLASS.0 | complete | Report MoE class facts and runtime blockers. |
| V010.CLASS.0 | planned | model-class schema finalization. |
| V010.CLASS.1 | planned | target class detector. |
| V010.CLASS.2 | planned | dense model-class report. |
| V010.CLASS.3 | complete | MoE model-class report. |
| V010.CLASS.4 | planned | source-only class report. |
| V010.CLASS.5 | planned | selected-slice class report. |
| V010.CLASS.6 | complete | DeepSeek class report. |
| V010.CLASS.7 | planned | GLM class/source-only report. |
| V010.CLASS.8 | complete | Qwen class report. |
| V010.CLASS.9 | complete | context class integration. |
| V010.CLASS.10 | complete | attention class integration. |
| V010.CLASS.11 | complete | KV class integration. |
| V010.CLASS.12 | planned | output-head class report. |
| V010.CLASS.13 | planned | tokenizer class report. |
| V010.CLASS.14 | planned | runtime requirement report. |
| V010.CLASS.15 | planned | dynamic path selection report. |
| V010.CLASS.16 | planned | v0.1.0 class acceptance gate. |

#### TRACK.TENSOR Rows

| Wave | Status | Description |
| --- | --- | --- |
| TENSOR.COLLECTION.QWEN.0 | complete | Inventory Qwen tensor collections from safetensors headers only. |
| TENSOR.COLLECTION.GEMMA.0 | complete | Inventory Gemma tensor collections from safetensors headers only. |
| TENSOR.MOE.0 | complete | Report MoE tensor collection coverage and missing runtime pieces. |
| V010.TENSOR.0 | planned | tensor collection schema. |
| V010.TENSOR.1 | complete | embedding collection. |
| V010.TENSOR.2 | planned | attention norm collection. |
| V010.TENSOR.3 | planned | post-attention norm collection. |
| V010.TENSOR.4 | planned | final norm collection. |
| V010.TENSOR.5 | complete | Q projection collection. |
| V010.TENSOR.6 | complete | K projection collection. |
| V010.TENSOR.7 | complete | V projection collection. |
| V010.TENSOR.8 | complete | O projection collection. |
| V010.TENSOR.9 | planned | RoPE/position metadata collection. |
| V010.TENSOR.10 | planned | attention mask/rule collection. |
| V010.TENSOR.11 | complete | KV runtime-state collection. |
| V010.TENSOR.12 | complete | dense MLP gate/up/down collection. |
| V010.TENSOR.13 | planned | dense activation collection. |
| V010.TENSOR.14 | complete | MoE router collection. |
| V010.TENSOR.15 | complete | MoE expert gate/up/down collection. |
| V010.TENSOR.16 | complete | MoE shared expert collection. |
| V010.TENSOR.17 | complete | MoE dispatch metadata collection. |
| V010.TENSOR.18 | complete | output-head collection. |
| V010.TENSOR.19 | complete | tokenizer metadata collection. |
| V010.TENSOR.20 | planned | runtime input/output token collection. |
| V010.TENSOR.21 | complete | required tensor coverage report. |
| V010.TENSOR.22 | complete | missing tensor blocker report. |
| V010.TENSOR.23 | planned | v0.1.0 tensor collection gate. |

#### TRACK.RESIDENCY Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.STORAGE.0 | planned | storage-stream doctrine refresh. |
| V010.STORAGE.1 | planned | storage root and cache layout. |
| V010.STORAGE.2 | planned | source shard index. |
| V010.STORAGE.3 | planned | artifact shard index. |
| V010.STORAGE.4 | planned | tensor byte-range map. |
| V010.STORAGE.5 | planned | tensor page map. |
| V010.STORAGE.6 | planned | tensor chunk map. |
| V010.STORAGE.7 | planned | cold-read probe. |
| V010.STORAGE.8 | planned | warm-read probe. |
| V010.STORAGE.9 | planned | repeated-read diagnostics. |
| V010.STORAGE.10 | planned | staged-read proof. |
| V010.STORAGE.11 | planned | host cache policy. |
| V010.STORAGE.12 | planned | eviction policy. |
| V010.STORAGE.13 | planned | short read failure. |
| V010.STORAGE.14 | planned | missing shard failure. |
| V010.STORAGE.15 | planned | digest mismatch failure. |
| V010.STORAGE.16 | planned | cleanup/release report. |
| V010.STORAGE.17 | planned | GLM source storage pressure report. |
| V010.STORAGE.18 | planned | MoE expert storage pressure report. |
| V010.STORAGE.19 | planned | output-head storage pressure report. |
| V010.STORAGE.20 | planned | v0.1.0 storage gate. |
| V010.RESIDENCY.0 | planned | residency class report. |
| V010.RESIDENCY.1 | planned | resident tensor plan. |
| V010.RESIDENCY.2 | planned | CPU residency plan. |
| V010.RESIDENCY.3 | planned | CUDA residency plan. |
| V010.RESIDENCY.4 | planned | managed-memory report. |
| V010.RESIDENCY.5 | planned | host-staged residency plan. |
| V010.RESIDENCY.6 | planned | SSD-staged residency plan. |
| V010.RESIDENCY.7 | planned | SSD-streamed residency plan. |
| V010.RESIDENCY.8 | planned | hybrid residency plan. |
| V010.RESIDENCY.9 | planned | distributed future-only report. |
| V010.RESIDENCY.10 | planned | embedding residency. |
| V010.RESIDENCY.11 | planned | attention tensor residency. |
| V010.RESIDENCY.12 | planned | KV residency. |
| V010.RESIDENCY.13 | planned | dense MLP residency. |
| V010.RESIDENCY.14 | planned | MoE expert residency. |
| V010.RESIDENCY.15 | planned | output-head residency. |
| V010.RESIDENCY.16 | planned | tokenizer/runtime metadata residency. |
| V010.RESIDENCY.17 | planned | residency transition proof. |
| V010.RESIDENCY.18 | planned | residency cleanup/failure report. |
| V010.RESIDENCY.19 | planned | v0.1.0 residency gate. |

#### TRACK.BACKEND Rows

| Wave | Status | Description |
| --- | --- | --- |
| CUDA.KERNEL.0 | complete | Harden bounded CUDA primitive kernels without claiming CUDA runtime generation. |
| V010.BACKEND.0 | planned | backend capability matrix. |
| V010.BACKEND.1 | complete | CPU baseline capability report. |
| V010.BACKEND.2 | complete | CUDA capability report. |
| V010.BACKEND.3 | complete | CUDA allocation proof. |
| V010.BACKEND.4 | complete | CUDA transfer proof. |
| V010.BACKEND.5 | complete | CUDA op parity subset. |
| V010.BACKEND.6 | planned | backend op refusal policy. |
| V010.BACKEND.7 | planned | backend fallback policy. |
| V010.BACKEND.8 | planned | backend scratch allocation policy. |
| V010.BACKEND.9 | planned | backend cleanup/failure report. |
| V010.BACKEND.10 | planned | future Metal feasibility report. |
| V010.BACKEND.11 | planned | future ROCm feasibility report. |
| V010.BACKEND.12 | planned | v0.1.0 backend gate. |
| V010.HARDWARE.0 | planned | local workstation profile. |
| V010.HARDWARE.1 | planned | Spark/GB10 profile. |
| V010.HARDWARE.2 | planned | Mac/Apple Silicon profile. |
| V010.HARDWARE.3 | planned | Strix Halo/ROCm future profile. |
| V010.HARDWARE.4 | planned | memory budget report. |
| V010.HARDWARE.5 | planned | storage bandwidth pressure report. |
| V010.HARDWARE.6 | planned | reproducibility metadata profile. |
| V010.BUILD.0 | planned | build profile matrix. |
| V010.BUILD.1 | planned | CPU debug build. |
| V010.BUILD.2 | planned | CPU release build. |
| V010.BUILD.3 | planned | CUDA debug build. |
| V010.BUILD.4 | planned | CUDA release build. |
| V010.BUILD.5 | planned | sanitizer build. |
| V010.BUILD.6 | planned | release artifact hygiene. |
| V010.BUILD.7 | planned | v0.1.0 build gate. |

#### TRACK.GRAPH Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.GRAPH.PRIM.0 | planned | primitive inventory report. |
| V010.GRAPH.PRIM.1 | planned | RoPE integration readiness. |
| V010.GRAPH.PRIM.2 | planned | attention primitive readiness. |
| V010.GRAPH.PRIM.3 | planned | matmul/projection readiness. |
| V010.GRAPH.PRIM.4 | planned | MLP primitive readiness. |
| V010.GRAPH.PRIM.5 | planned | expert-slice primitive readiness. |
| V010.GRAPH.PRIM.6 | planned | softmax/numerics policy. |
| V010.GRAPH.PRIM.7 | planned | activation function policy. |
| V010.GRAPH.PRIM.8 | planned | residual/add policy. |
| V010.GRAPH.PRIM.9 | planned | normalization policy. |
| V010.GRAPH.PRIM.10 | planned | graph primitive regression gate. |
| V010.GRAPH.0 | planned | graph requirement report. |
| V010.GRAPH.1 | planned | embedding graph input. |
| V010.GRAPH.2 | planned | attention norm. |
| V010.GRAPH.3 | planned | Q projection. |
| V010.GRAPH.4 | planned | K projection. |
| V010.GRAPH.5 | planned | V projection. |
| V010.GRAPH.6 | planned | RoPE/position application. |
| V010.GRAPH.7 | planned | attention score path. |
| V010.GRAPH.8 | planned | causal/mask path. |
| V010.GRAPH.9 | planned | softmax path. |
| V010.GRAPH.10 | planned | value accumulation. |
| V010.GRAPH.11 | planned | O projection. |
| V010.GRAPH.12 | planned | attention residual path. |
| V010.GRAPH.13 | planned | post-attention norm. |
| V010.GRAPH.14 | planned | dense MLP gate/up/down path. |
| V010.GRAPH.15 | planned | dense MLP residual path. |
| V010.GRAPH.16 | planned | final norm path. |
| V010.GRAPH.17 | planned | output hidden state ownership. |
| V010.GRAPH.18 | planned | graph scratch lifecycle. |
| V010.GRAPH.19 | planned | graph cleanup/failure report. |
| V010.GRAPH.20 | planned | first transformer block. |
| V010.GRAPH.21 | planned | repeated layer stack. |
| V010.GRAPH.22 | complete | selected-slice graph proof. |
| V010.GRAPH.23 | planned | full-runtime-candidate graph proof. |
| V010.GRAPH.24 | planned | v0.1.0 graph gate. |
| V010.ATTN.0 | planned | attention runtime requirement report. |
| V010.ATTN.1 | planned | Q source validation. |
| V010.ATTN.2 | planned | K source validation. |
| V010.ATTN.3 | planned | V source validation. |
| V010.ATTN.4 | planned | O source validation. |
| V010.ATTN.5 | planned | head layout validation. |
| V010.ATTN.6 | planned | RoPE/position runtime rule. |
| V010.ATTN.7 | planned | mask runtime rule. |
| V010.ATTN.8 | planned | attention scratch policy. |
| V010.ATTN.9 | planned | full attention runtime path. |
| V010.ATTN.10 | planned | GQA/MQA/MLA family rule if required. |
| V010.ATTN.11 | planned | attention reference comparison. |
| V010.ATTN.12 | planned | attention cleanup/failure. |
| V010.ATTN.13 | planned | v0.1.0 attention gate. |
| V010.MOE.0 | planned | MoE requirement report. |
| V010.MOE.1 | planned | expert count report. |
| V010.MOE.2 | planned | active expert count report. |
| V010.MOE.3 | planned | shared expert report. |
| V010.MOE.4 | planned | router tensor report. |
| V010.MOE.5 | planned | router logits boundary. |
| V010.MOE.6 | planned | routing dtype/top-k policy. |
| V010.MOE.7 | planned | top-k expert selection. |
| V010.MOE.8 | planned | expert weight selection. |
| V010.MOE.9 | planned | expert dispatch plan. |
| V010.MOE.10 | planned | expert dispatch proof. |
| V010.MOE.11 | planned | expert compute proof. |
| V010.MOE.12 | planned | expert accumulation proof. |
| V010.MOE.13 | planned | shared expert integration. |
| V010.MOE.14 | planned | MoE residual integration. |
| V010.MOE.15 | planned | MoE cleanup/failure report. |
| V010.MOE.16 | planned | MoE selected-slice proof. |
| V010.MOE.17 | planned | MoE block integration. |
| V010.MOE.18 | planned | MoE prefill integration. |
| V010.MOE.19 | planned | MoE decode integration. |
| V010.MOE.20 | planned | v0.1.0 MoE gate. |

#### TRACK.PREFILL Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.CONTEXT.0 | planned | active context policy. |
| V010.CONTEXT.1 | complete | model max context report. |
| V010.CONTEXT.2 | complete | requested context report. |
| V010.CONTEXT.3 | complete | chunk size policy. |
| V010.CONTEXT.4 | complete | chunk planner. |
| V010.CONTEXT.5 | complete | prefill position policy. |
| V010.CONTEXT.6 | complete | decode position policy. |
| V010.CONTEXT.7 | complete | overflow refusal behavior. |
| V010.CONTEXT.8 | planned | context stop behavior. |
| V010.CONTEXT.9 | planned | context trace. |
| V010.CONTEXT.10 | planned | v0.1.0 context gate. |
| V010.PREFILL.0 | complete | prefill requirement report. |
| V010.PREFILL.1 | complete | token input to prefill planner. |
| V010.PREFILL.2 | planned | embedding prefill input. |
| V010.PREFILL.3 | planned | layer-0 prefill entry. |
| V010.PREFILL.4 | planned | attention prefill. |
| V010.PREFILL.5 | planned | KV write during prefill. |
| V010.PREFILL.6 | planned | dense MLP prefill. |
| V010.PREFILL.7 | planned | MoE router/expert prefill. |
| V010.PREFILL.8 | planned | repeated layer prefill. |
| V010.PREFILL.9 | planned | chunked prefill. |
| V010.PREFILL.10 | planned | staged/SSD prefill plan. |
| V010.PREFILL.11 | planned | prefill state ownership. |
| V010.PREFILL.12 | planned | prefill cleanup/failure. |
| V010.PREFILL.13 | planned | prefill trace. |
| V010.PREFILL.14 | planned | prefill regression. |
| V010.PREFILL.15 | planned | v0.1.0 prefill gate. |

#### TRACK.KV Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.KV.0 | complete | KV requirement report. |
| V010.KV.1 | complete | KV shape policy. |
| V010.KV.2 | planned | KV dtype/qtype policy. |
| V010.KV.3 | complete | KV capacity estimator. |
| V010.KV.4 | planned | CPU KV allocation. |
| V010.KV.5 | planned | CUDA KV allocation. |
| V010.KV.6 | planned | K write from prefill attention. |
| V010.KV.7 | planned | V write from prefill attention. |
| V010.KV.8 | planned | K/V read during decode. |
| V010.KV.9 | planned | layer/head/position indexing. |
| V010.KV.10 | planned | token position advancement. |
| V010.KV.11 | planned | context overflow behavior. |
| V010.KV.12 | planned | KV clear/reinit. |
| V010.KV.13 | planned | KV cleanup/failure. |
| V010.KV.14 | complete | KV trace/inspect. |
| V010.KV.15 | planned | paged KV plan. |
| V010.KV.16 | planned | paged KV skeleton. |
| V010.KV.17 | planned | host spill experiment. |
| V010.KV.18 | planned | SSD spill experiment. |
| V010.KV.19 | planned | KV quantization policy. |
| V010.KV.20 | planned | v0.1.0 KV gate. |

#### TRACK.DECODE Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.DECODE.0 | complete | decode requirement report. |
| V010.DECODE.1 | complete | decode state ownership. |
| V010.DECODE.2 | complete | decode position input. |
| V010.DECODE.3 | planned | decode reads KV. |
| V010.DECODE.4 | planned | decode attention step. |
| V010.DECODE.5 | planned | decode dense MLP path. |
| V010.DECODE.6 | planned | decode MoE path. |
| V010.DECODE.7 | planned | decode hidden state output. |
| V010.DECODE.8 | planned | one runtime decode step. |
| V010.DECODE.9 | planned | repeated decode lifecycle. |
| V010.DECODE.10 | planned | decode interruption/cancel safe point. |
| V010.DECODE.11 | planned | decode cleanup/failure. |
| V010.DECODE.12 | planned | decode trace. |
| V010.DECODE.13 | planned | decode regression. |
| V010.DECODE.14 | planned | v0.1.0 decode gate. |

#### TRACK.LOGITS Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.LOGITS.0 | complete | logits requirement report. |
| V010.LOGITS.1 | complete | final hidden-state ownership. |
| V010.LOGITS.2 | planned | final norm integration. |
| V010.LOGITS.3 | complete | output-head tensor mapping. |
| V010.LOGITS.4 | planned | output-head residency. |
| V010.LOGITS.5 | planned | output-head projection. |
| V010.LOGITS.6 | complete | logits buffer allocation. |
| V010.LOGITS.7 | planned | logits dtype/range report. |
| V010.LOGITS.8 | complete | logits checksum report. |
| V010.LOGITS.9 | complete | top-k diagnostics. |
| V010.LOGITS.10 | planned | logprob diagnostics. |
| V010.LOGITS.11 | planned | sharded output-head plan. |
| V010.LOGITS.12 | planned | staged/SSD output-head plan. |
| V010.LOGITS.13 | planned | logits cleanup/failure. |
| V010.LOGITS.14 | planned | logits trace. |
| V010.LOGITS.15 | planned | logits regression. |
| V010.LOGITS.16 | planned | v0.1.0 logits gate. |

#### TRACK.SAMPLING Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.SAMPLE.0 | complete | sampling requirement report. |
| V010.SAMPLE.1 | planned | greedy over output-head logits. |
| V010.SAMPLE.2 | complete | selected token report. |
| V010.SAMPLE.3 | complete | candidate set report. |
| V010.SAMPLE.4 | complete | temperature validation. |
| V010.SAMPLE.5 | planned | top-k sampling. |
| V010.SAMPLE.6 | planned | top-p sampling. |
| V010.SAMPLE.7 | planned | min-p sampling. |
| V010.SAMPLE.8 | planned | typical sampling. |
| V010.SAMPLE.9 | planned | seeded stochastic sampling. |
| V010.SAMPLE.10 | planned | deterministic reproducibility report. |
| V010.SAMPLE.11 | planned | sampling cleanup/failure. |
| V010.SAMPLE.12 | planned | sampling trace. |
| V010.SAMPLE.13 | planned | sampling regression. |
| V010.SAMPLE.14 | planned | v0.1.0 sampling gate. |

#### TRACK.TOKENIZER Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.TOKENIZER.0 | planned | tokenizer requirement report. |
| V010.TOKENIZER.1 | planned | tokenizer metadata loader/report. |
| V010.TOKENIZER.2 | complete | token ID input contract. |
| V010.TOKENIZER.3 | planned | token ID output contract. |
| V010.TOKENIZER.4 | complete | special token report. |
| V010.TOKENIZER.5 | planned | EOS token policy. |
| V010.TOKENIZER.6 | planned | stop-token text policy. |
| V010.TOKENIZER.7 | planned | prompt/template policy. |
| V010.TOKENIZER.8 | planned | detokenization boundary. |
| V010.TOKENIZER.9 | planned | tokenizer failure/refusal behavior. |
| V010.TOKENIZER.10 | planned | tokenizer trace. |
| V010.TOKENIZER.11 | planned | tokenizer regression. |
| V010.TOKENIZER.12 | planned | v0.1.0 tokenizer gate. |

#### TRACK.GENERATION Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.GEN.0 | complete | generation requirement report. |
| V010.GEN.1 | complete | generation state ownership. |
| V010.GEN.2 | complete | generation option parser. |
| V010.GEN.3 | complete | prefill to decode composition. |
| V010.GEN.4 | complete | decode to logits composition. |
| V010.GEN.5 | complete | logits to sample composition. |
| V010.GEN.6 | complete | sample to append composition. |
| V010.GEN.7 | complete | token append. |
| V010.GEN.8 | planned | context stop policy. |
| V010.GEN.9 | planned | EOS stop policy. |
| V010.GEN.10 | planned | stop-token policy. |
| V010.GEN.11 | complete | max-new-tokens policy. |
| V010.GEN.12 | complete | generation checksum. |
| V010.GEN.13 | complete | generation trace. |
| V010.GEN.14 | complete | failure/cancel safe points. |
| V010.GEN.15 | complete | cleanup/release. |
| V010.GEN.16 | planned | CLI runtime generation command. |
| V010.GEN.17 | complete | generation smoke. |
| V010.GEN.18 | planned | generation regression. |
| V010.GEN.19 | planned | v0.1.0 generation gate. |
| V010.RUNTIME.0 | complete | runtime lifecycle report. |
| V010.RUNTIME.1 | planned | engine/session ownership finalization. |
| V010.RUNTIME.2 | complete | runtime state creation. |
| V010.RUNTIME.3 | planned | runtime state mutation rules. |
| V010.RUNTIME.4 | complete | runtime cleanup idempotence. |
| V010.RUNTIME.5 | complete | partial output preservation. |
| V010.RUNTIME.6 | planned | phase failure vocabulary. |
| V010.RUNTIME.7 | planned | preflight failure behavior. |
| V010.RUNTIME.8 | planned | graph failure behavior. |
| V010.RUNTIME.9 | planned | prefill failure behavior. |
| V010.RUNTIME.10 | planned | KV failure behavior. |
| V010.RUNTIME.11 | planned | decode failure behavior. |
| V010.RUNTIME.12 | planned | logits failure behavior. |
| V010.RUNTIME.13 | planned | sampling failure behavior. |
| V010.RUNTIME.14 | planned | append failure behavior. |
| V010.RUNTIME.15 | complete | cancellation safe points. |
| V010.RUNTIME.16 | planned | OS signal boundary. |
| V010.RUNTIME.17 | planned | v0.1.0 runtime lifecycle gate. |
| V010.TRACE.0 | planned | trace taxonomy refresh. |
| V010.TRACE.1 | complete | token trace. |
| V010.TRACE.2 | planned | graph trace. |
| V010.TRACE.3 | planned | tensor role trace. |
| V010.TRACE.4 | planned | residency trace. |
| V010.TRACE.5 | planned | prefill trace. |
| V010.TRACE.6 | planned | KV trace. |
| V010.TRACE.7 | planned | decode trace. |
| V010.TRACE.8 | planned | logits trace. |
| V010.TRACE.9 | planned | sampling trace. |
| V010.TRACE.10 | complete | generation trace. |
| V010.TRACE.11 | complete | cleanup/failure trace. |
| V010.TRACE.12 | planned | raw tensor dump refusal policy. |
| V010.TRACE.13 | planned | structured trace output. |
| V010.TRACE.14 | planned | v0.1.0 trace gate. |

#### TRACK.OPERATOR Rows
| Wave | Status | Description |
| --- | --- | --- |
| SPINE.OUTPUT.UX.CONTRACT.0 | complete | Define CLI output UX contract and diagnostic demotion plan. |
| CLI.ARCH.AUDIT.0 | complete | Inventory print/output pressure and porcelain/plumbing doctrine. |
| SPINE.CLI.REBASE.1 | complete | Rebase Operator CLI track after V010.CLI.26 grammar work. |
| TOPOLOGY.FS.0 | complete | Move C implementation under src modules and quarantine model-target CLI command surface. |
| TOPOLOGY.SOURCE.CONTRACT.0 | complete | Add source file and function contract guardrails for module ownership. |
| TOPOLOGY.CLI.PRINT.ALL.0 | complete | Move production operator printing, help, usage, renderers, and CLI metadata out of domain modules into src/cli. |
| TOPOLOGY.DOMAIN.RESTORE.0 | complete | Restore domain implementation files after invalid CLI command-file displacement. |
| TOPOLOGY.CELL.SOURCE.0 | complete | Extract source into domain/report/input/command/render/write cell. |
| TOPOLOGY.CELL.GENERATION.0 | complete | Extract generate into domain/report/input/command/render/trace cell. |
| TOPOLOGY.CELL.KV.0 | complete | Extract KV into domain/report/input/command/render cell. |
| TOPOLOGY.CELL.SAMPLING.0 | complete | Extract sampling into domain/report/input/command/render cell. |
| TOPOLOGY.CELL.GRAPH.0 | complete | Extract graph into domain/report/input/command/render cell and separate graph facts from operator output. |
| TOPOLOGY.CELL.MODEL_TARGET.0 | complete | Extract model-target into catalog/report/input/command/render cell and remove target facts from CLI command adapter. |
| TOPOLOGY.CELL.MODEL_TARGET.1 | complete | Decompose model-target report monolith into owned target catalog, decision, candidate, map, qtype, and sidecar modules. |
| TOPOLOGY.CELL.MODEL_TARGET.2 | complete | Dissolve model-target internal compatibility backend into specialized ownership modules. |
| TOPOLOGY.CELL.MODEL_TARGET.3 | complete | Remove model-target runner, CLI-shaped report request, and text-buffer report API. |
| TOPOLOGY.CELL.CLOSURE.1 | complete | Audit remaining topology residue after source, generation, KV, sampling, graph, and model-target cell extraction. |
| TOPOLOGY.CELL.CLOSURE.0 | planned | Close remaining mixed cells after each owner-specific extraction is proven. |
| V010.PATHS.0 | complete | operator root layout report. |
| V010.PATHS.1 | complete | source/artifact/reference/report/cache separation. |
| V010.PATHS.2 | complete | registry path layout. |
| V010.PATHS.3 | complete | report output layout. |
| V010.PATHS.4 | planned | runtime cache layout. |
| V010.PATHS.5 | complete | artifact hygiene report. |
| V010.PATHS.6 | complete | path override precedence. |
| V010.PATHS.7 | complete | missing path/refusal behavior. |
| V010.PATHS.8 | complete | v0.1.0 path acceptance gate. |
| V010.CLI.0 | planned | command inventory refresh. |
| V010.CLI.1 | planned | help layout refresh. |
| V010.CLI.2 | planned | normal path first policy. |
| V010.CLI.3 | planned | advanced diagnostic flags policy. |
| V010.CLI.12 | planned | refusal wording audit. |
| V010.CLI.13 | planned | structured output mode. |
| V010.CLI.15 | planned | command proof transcript. |
| V010.CLI.16 | planned | v0.1.0 CLI gate. |
| V010.CLI.17 | complete | normal output contract and layout baseline. |
| V010.CLI.18 | complete | diagnostic output demotion. |
| V010.CLI.19 | complete | compact report/table output. |
| V010.CLI.20 | planned | raw/plumbing JSON foundation. |
| V010.CLI.21 | planned | metric output surface. |
| V010.CLI.22 | planned | audit output surface. |
| V010.CLI.23 | planned | quiet/no-color/non-TTY terminal policy. |
| V010.CLI.24 | complete | hardcoded print reduction pass. |
| V010.CLI.25 | complete | renderer ownership foundation. |
| V010.CLI.26 | complete | base CLI grammar and command catalog. |
| V010.CLI.27 | planned, not Active Next | base status and refusal grammar. |
| V010.CLI.28 | planned | error/log/diagnostic surface split. |
| V010.CLI.MODELS.0 | planned | models namespace grammar. |
| V010.CLI.MODELS.1 | planned | models list/current/status porcelain. |
| V010.CLI.MODELS.2 | planned | models prepare/check porcelain. |
| V010.CLI.MODELS.3 | planned | models download/control porcelain. |
| V010.CLI.MODELS.4 | planned | models artifacts porcelain. |
| V010.CLI.TARGET.0 | planned | model-target namespace grammar. |
| V010.CLI.TARGET.1 | planned | model-target inspect/list porcelain. |
| V010.CLI.TARGET.2 | planned | model-target tensor-map/missing-roles/gate porcelain. |
| V010.CLI.TARGET.3 | planned | model-target quant-policy porcelain. |
| V010.CLI.SOURCE.0 | planned | source-manifest/native-weights porcelain. |
| V010.CLI.ACCOUNTS.0 | planned | accounts/provider porcelain. |
| V010.CLI.PATHS.0 | planned | paths porcelain finalization. |
| V010.CLI.GRAPH.0 | planned | graph check/operator porcelain. |
| V010.CLI.RUNTIME.0 | planned | runtime diagnostic command grammar. |
| V010.CLI.GENERATE.0 | planned | diagnostic generate porcelain grammar. |
| V010.CLI.CHAT.0 | planned | accepted-only chat UX grammar. |
| V010.DOCTOR.0 | planned | doctor command scope. |
| V010.DOCTOR.1 | planned | environment checks. |
| V010.DOCTOR.2 | planned | build/backend checks. |
| V010.DOCTOR.3 | planned | CUDA checks. |
| V010.DOCTOR.4 | planned | artifact checks. |
| V010.DOCTOR.5 | planned | registry checks. |
| V010.DOCTOR.6 | planned | model target checks. |
| V010.DOCTOR.7 | planned | graph checks. |
| V010.DOCTOR.8 | planned | runtime checks. |
| V010.DOCTOR.9 | planned | generation readiness checks. |
| V010.DOCTOR.10 | planned | common failure cookbook. |
| V010.DOCTOR.11 | planned | v0.1.0 doctor gate. |

#### TRACK.SERVE Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.SERVE.0 | planned | serving ownership map. |
| V010.SERVE.1 | planned | daemon state reflects runtime state. |
| V010.SERVE.2 | planned | model registry exposed by daemon. |
| V010.SERVE.3 | planned | runtime readiness endpoint. |
| V010.SERVE.4 | planned | generate endpoint after CLI generation. |
| V010.SERVE.5 | planned | streaming endpoint after runtime generation. |
| V010.SERVE.6 | planned | cancellation boundary. |
| V010.SERVE.7 | planned | provider compatibility boundary. |
| V010.SERVE.8 | planned | OpenAI compatibility after generation. |
| V010.SERVE.9 | planned | Anthropic compatibility after generation. |
| V010.SERVE.10 | planned | server observability. |
| V010.SERVE.11 | planned | v0.1.0 serving decision gate. |

#### TRACK.EVAL Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.EVAL.0 | planned | eval harness structure. |
| V010.EVAL.1 | planned | fixture graph eval. |
| V010.EVAL.2 | planned | primitive eval. |
| V010.EVAL.3 | planned | selected partial graph eval. |
| V010.EVAL.4 | planned | full-runtime-candidate graph eval. |
| V010.EVAL.5 | planned | prefill eval. |
| V010.EVAL.6 | planned | KV eval. |
| V010.EVAL.7 | planned | decode eval. |
| V010.EVAL.8 | planned | logits eval. |
| V010.EVAL.9 | planned | sampling eval. |
| V010.EVAL.10 | planned | generation smoke eval. |
| V010.EVAL.11 | planned | tokenizer/stop eval. |
| V010.EVAL.12 | planned | failure-path eval. |
| V010.EVAL.13 | planned | capability eval plan. |
| V010.EVAL.14 | planned | v0.1.0 eval gate. |

#### TRACK.BENCH Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.BENCH.0 | planned | benchmark harness metadata contract. |
| V010.BENCH.1 | planned | machine profile record. |
| V010.BENCH.2 | planned | artifact identity record. |
| V010.BENCH.3 | planned | qtype/context/backend record. |
| V010.BENCH.4 | planned | run count/reproducibility record. |
| V010.BENCH.5 | planned | prefill benchmark. |
| V010.BENCH.6 | planned | decode benchmark. |
| V010.BENCH.7 | planned | generation benchmark. |
| V010.BENCH.8 | planned | memory pressure benchmark. |
| V010.BENCH.9 | planned | server benchmark. |
| V010.BENCH.10 | planned | DeepSeek benchmark only after DeepSeek generation. |
| V010.BENCH.11 | planned | v0.1.0 benchmark decision gate. |
| V010.PROFILE.0 | planned | runtime profile trace. |
| V010.PROFILE.1 | planned | memory profile trace. |
| V010.PROFILE.2 | planned | storage profile trace. |
| V010.PROFILE.3 | planned | backend profile trace. |

#### TRACK.RELEASE Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.VERSION.0 | planned | version string policy. |
| V010.VERSION.1 | planned | v0.1.0 version bump. |
| V010.PACKAGE.0 | planned | binary packaging policy. |
| V010.PACKAGE.1 | planned | release build artifact policy. |
| V010.PACKAGE.2 | planned | no model artifact packaging rule. |
| V010.RELEASE.0 | planned | scope lock. |
| V010.RELEASE.1 | planned | target lock. |
| V010.RELEASE.2 | planned | command proof transcript. |
| V010.RELEASE.3 | planned | failure proof transcript. |
| V010.RELEASE.4 | planned | artifact guardrail transcript. |
| V010.RELEASE.5 | planned | claim audit. |
| V010.RELEASE.6 | planned | docs audit. |
| V010.RELEASE.7 | planned | changelog/release notes. |
| V010.RELEASE.8 | planned | tag readiness report. |
| V010.RELEASE.9 | planned | v0.1.0 tag. |
| V010.CI.0 | planned | CI/test matrix refresh. |
| V010.CI.1 | planned | make check gate. |
| V010.CI.2 | planned | make smoke gate. |
| V010.CI.3 | planned | make check-cuda gate where available. |
| V010.CI.4 | complete | docs surface gate. |
| V010.CI.5 | complete | source layout gate. |
| V010.CI.6 | complete | code natural gate. |
| V010.CI.7 | complete | artifact guardrail. |
| V010.CI.8 | planned | forbidden claim scan. |
| V010.CI.9 | planned | public docs leak scan. |
| V010.CI.10 | planned | command proof transcript gate. |
| V010.CI.11 | planned | failure-path transcript gate. |
| V010.CI.12 | planned | v0.1.0 CI acceptance gate. |
| V010.DOCS.INTERNAL.0 | planned | internal v0.1.0 spine summary. |
| V010.DOCS.RUNBOOK.0 | planned | operator v0.1.0 runbook. |
| V010.DOCS.RUNBOOK.1 | planned | model-specific runbooks. |
| V010.DOCS.API.0 | planned | API docs for implemented surface. |
| V010.DOCS.CONTRACT.0 | planned | behavior contract update. |
| V010.DOCS.README.0 | planned | README runtime thesis update. |
| V010.DOCS.DIAGRAM.0 | planned | artifact-to-runtime diagram. |
| V010.DOCS.DIAGRAM.1 | planned | runtime ladder diagram. |
| V010.DOCS.DIAGRAM.2 | planned | evidence/benchmark diagram. |
| V010.DOCS.DIAGRAM.3 | planned | dense vs MoE path diagram. |
| V010.DOCS.DIAGRAM.4 | planned | storage/residency diagram. |
| V010.DOCS.PUBLIC.0 | planned | public claim audit. |
| V010.DOCS.PUBLIC.1 | planned | internal ID leak audit. |
| V010.DOCS.PUBLIC.2 | planned | v0.1.0 docs acceptance gate. |

#### TRACK.POST010 Rows

| Wave | Status | Description |
| --- | --- | --- |
| V010.SPEC.0 | planned | speculative reference registry. |
| V010.SPEC.1 | planned | DSpark reference record. |
| V010.SPEC.2 | planned | DFlash/HyperDFlash reference record. |
| V010.SPEC.3 | planned | draft source report. |
| V010.SPEC.4 | planned | token verification semantics. |
| V010.SPEC.5 | planned | accepted-prefix accounting. |
| V010.SPEC.6 | planned | rejected-token behavior. |
| V010.SPEC.7 | planned | KV rollback/reuse policy. |
| V010.SPEC.8 | planned | dense speculative verification. |
| V010.SPEC.9 | planned | MoE routing-aware verification report. |
| V010.SPEC.10 | planned | MoE expert-budget verification. |
| V010.SPEC.11 | planned | verification-cost utility report. |
| V010.SPEC.12 | planned | DeepSeek speculative path. |
| V010.SPEC.13 | planned | speculative benchmark. |
| POST010.GLM.RUNTIME.0 | post-v0.1.0 | GLM runtime promotion path after v0.1.0. |
| POST010.QWEN.METAL.0 | post-v0.1.0 | Qwen Metal runtime path after baseline release. |
| POST010.ROCM.0 | post-v0.1.0 | ROCm/Strix Halo backend path after v0.1.0. |
| POST010.STORAGE.GEN.0 | post-v0.1.0 | SSD-streamed generation exploration after baseline generation. |
| POST010.SERVE.PUBLIC.0 | post-v0.1.0 | production serving surface hardening after v0.1.0. |
| POST010.SPEC.0 | post-v0.1.0 | speculative acceleration program after baseline generation. |
| POST010.BENCH.PUBLIC.0 | post-v0.1.0 | public benchmark table expansion after measured runtime. |
| POST010.EVAL.CAPABILITY.0 | post-v0.1.0 | broader capability eval suite after v0.1.0. |
| POST010.DOCS.PUBLIC.0 | post-v0.1.0 | public evidence expansion after release-safe claims. |
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
  V010.QUANT.1.

Active / Next:
  V010.QUANT.2 - qtype compute/refusal matrix.

Planned gates:
  V010.QUANT.3 - calibration/imatrix requirement report.
  artifact qtype readiness gate.

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
  graph primitives, selected graph slices, transformer graph path.

Current state:
  partial.

Complete:
  RoPE, attention, projection/matmul, MLP/routed-expert primitives; controlled
  block and layer fixtures; selected DeepSeek slices.

Active / Next:
  wait for generation-capable artifacts and role/qtype readiness.

Planned gates:
  transformer graph over supported family artifacts.

Boundary:
  graph proof is not prefill, decode, logits, sampling, or generation.

### TRACK.PREFILL - Prefill

Owns:
  transformer prefill, chunking, prefill state ownership, KV write
  boundary.

Current state:
  diagnostic-runtime only.

Complete:
  segment-summary, layer-backed, chunked diagnostic prefill.

Active / Next:
  wait for graph and KV gates.

Planned gates:
  transformer prefill over supported artifacts.

Boundary:
  diagnostic prefill is not transformer prefill.

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
  K/V writes during prefill and reads during decode.

Boundary:
  diagnostic KV is not attention-backed KV.

### TRACK.DECODE - Decode

Owns:
  one-step and repeated decode over runtime state.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic decode.

Active / Next:
  wait for transformer prefill and KV read/write.

Planned gates:
  one runtime decode step, repeated decode lifecycle.

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
  output-head logits for supported families.

Boundary:
  diagnostic logits are not model logits.

### TRACK.SAMPLING - Sampling

Owns:
  greedy and stochastic sampling over output-head logits.

Current state:
  diagnostic-runtime only.

Complete:
  bounded diagnostic greedy sampler.

Active / Next:
  wait for output-head logits.

Planned gates:
  greedy over output-head logits, then stochastic strategies and reproducibility.

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
  wait for transformer prefill, KV, decode, logits, sampling, tokenizer boundary.

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
  V010.CLI.25, V010.CLI.26, SPINE.CLI.REBASE.1,
  TOPOLOGY.CLI.PRINT.ALL.0, TOPOLOGY.DOMAIN.RESTORE.0.

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
| GATE.GRAPH | TRACK.GRAPH | transformer graph path over supported artifacts |
| GATE.PREFILL | TRACK.PREFILL | prefill and state ownership |
| GATE.KV | TRACK.KV | K/V writes and decode reads |
| GATE.DECODE | TRACK.DECODE | one runtime decode step and repeat lifecycle |
| GATE.LOGITS | TRACK.LOGITS | output-head logits |
| GATE.SAMPLING | TRACK.SAMPLING | token selection over output-head logits |
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

Minimum gate authority: docs/v010-release-doctrine.md, section "## 8. Release Gates".
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
| selected-runtime-slice | bounded subset of a model artifact used for parser/materialization/graph proof |
| supported-generation-family | DeepSeek, Qwen, or Gemma for v0.1.0 |
| generation-capable artifact | YVEX-produced artifact with required roles, qtypes, metadata, identity, and integrity for generation work |
| runtime generation | prefill/KV/decode/logits/sampling/append/stop/cleanup path over model state |
| report-only | command-visible facts without execution |
| diagnostic-runtime | runtime control flow over bounded diagnostic state |
| fixture-proof | synthetic controlled proof with reference checks |
| selected-slice-proof | bounded proof over selected artifact tensors |
| full-runtime | transformer path over a supported generation artifact |
| eval-ready | implemented runtime path has correctness/failure evaluation |
| benchmark-ready | implemented runtime path has measured benchmark metadata |
| release-ready | all target, artifact, runtime, eval, benchmark, docs, CI, and claim gates pass |
