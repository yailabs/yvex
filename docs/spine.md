# YVEX Inner Delivery Spine

Date: 2026-07-02
Status: internal delivery spine
Project: YVEX
Language: C
Primary platform: Linux + CUDA
Interface: CLI

## 0. Dashboard

| Field | Current value |
| --- | --- |
| Project type | local-first inference engine |
| Current runtime stage | bounded diagnostic generation |
| Full-runtime state | unsupported |
| DeepSeek full-generation state | unsupported |
| Eval / benchmark | unsupported / not measured |
| Active next | MOE.CLASS.0 - MoE model-class report |
| Release target | v0.1.0 - first honest full-runtime path |
| Primary pressure targets | DeepSeek selected-runtime-slice; GLM huge source/storage; Qwen/Metal portability |
| Main blocker | no full-runtime path yet |

## 1. Reading Rule

This file is a delivery spine. It is not an archive, diary, paper index, or
public claim page.

| Term | Meaning |
| --- | --- |
| Block | ownership domain |
| Track | ordered delivery lane inside a block |
| Row / wave | smallest delivery package |
| Completed rows | implemented, reported, or governance work already closed |
| Planned rows | future delivery packages, ordered as dependencies |
| Active next | the immediate next row before broader runtime work |
| Boundary | what the track must not claim |

Only implementation rows can increase runtime capability. Report rows increase
inspection. Governance rows change project control only.

## 2. Delivery Spine

### BLOCK 0 - Source And Target Evidence

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.TARGET | target classes, pressure objects, release target | OWI.TARGETS.0 | V010.SCOPE.0-7 -> V010.TARGET.0-10 -> OWI.TARGETS.1-3 | V010.TARGET.9 after MoE/class facts if target decision needs it | A target is not a capability claim. |
| TRACK.SOURCE | official source tensors, source manifests, native inventory | OWI.1-2, OWI.REBASE.0 | OWI.HUGE.0-4 -> V010.SOURCE.0-10 | OWI.TARGETS.1 or V010.SOURCE.* | Source inventory is not execution. |

### BLOCK 1 - Artifact Production

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.ARTIFACT | tensor mapping, quant policy, calibration contract, GGUF emission, artifact naming | OWI.3-9, OWI.FINAL.0, ARTIFACT.NAMING.0 | V010.MAP.0-9 -> V010.QUANT.0-3 -> V010.ARTIFACT.EMIT.0-6 | after target/source decision | Selected artifacts do not prove full-runtime execution. |

### BLOCK 2 - Artifact Identity And Integrity

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.INTEGRITY | digest, byte ranges, shape/dtype, corruption refusal, materialization and graph gates | ARTIFACT.INTEGRITY.0-9, ARTIFACT.INTEGRITY.FINAL.0, FULLMODEL.0-2 | V010.INTEGRITY.0-13 -> V010.FULLMODEL.0-11 | full-runtime acceptance gate rows after target selection | Integrity proof is not execution. |

### BLOCK 3 - Model Class And Tensor Collections

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.MODEL | dense/MoE/source-only/selected-slice routing and runtime blockers | FULLMODEL.3, FAMILY.RUNTIME.0, ATTENTION.CLASS.0, KV.CACHE.0, CONTEXT.CLASS.0 | MODEL.CLASS.0-3 -> V010.CLASS.0-16 | MOE.CLASS.0 | Model-class reports are not runtime execution. |
| TRACK.TENSOR | embedding, norm, attention, MLP/MoE, output, tokenizer, KV roles | FULLMODEL.0, FULLMODEL.3, FAMILY.RUNTIME.0 | TENSOR.COLLECTION.0-2 -> V010.TENSOR.0-23 | V010.TENSOR.14-17 after MoE facts | Tensor maps are not model support. |
| TRACK.MOE | MoE class facts, router, experts, shared experts, dispatch, accumulation | GRAPH.OPS.3 expert-slice primitive | MOE.CLASS.0 -> MOE.ACT.0 -> V010.MOE.0-20 | MOE.CLASS.0 | Expert-slice primitive is not MoE runtime. |
| TRACK.CONTEXT | requested/active context, chunking, overflow, decode position | CONTEXT.CLASS.0 | V010.CONTEXT.0-10 | as needed by full-runtime prefill | Context reports are not long-context execution. |
| TRACK.TOKENIZER | tokenizer metadata, detokenization, EOS/stop policy, prompt formatting | D0 tokenizer/prompt diagnostics | V010.TOKENIZER.0-10 | after generation path needs text/stop behavior | Tokenizer diagnostics are not generation quality. |

### BLOCK 4 - Storage, Residency, And KV Placement

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.STORAGE | shard index, byte ranges, cold/warm reads, page/chunk policy, cache, cleanup | doctrine and target planning only | STORAGE.STREAM.0-6 -> OWI.HUGE.0-4 -> V010.STORAGE.0-20 | after source/artifact selection | Storage streaming is not generation. |
| TRACK.RESIDENCY | resident, host-staged, SSD-staged, SSD-streamed, managed, hybrid placement | FULLMODEL.1-2 placement and bounded materialization proof | RESIDENCY.0-2 -> V010.RESIDENCY.0-19 | V010.RESIDENCY.* after tensor map | Residency plans are not runtime execution. |
| TRACK.KV | KV shape, dtype/qtype, capacity, writes, reads, indexing, residency, cleanup | M9, PREFILL.1, KV.CACHE.0 | KV.MIN.0-3 -> RUNTIME.KV.1-5 -> V010.KV.0-20 | after attention/prefill path exists | Diagnostic KV is not attention-backed KV. |

### BLOCK 5 - Compute Backend And Hardware

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.BACKEND | CPU/CUDA/future Metal/ROCm capability, allocation, transfer, build, hardware profiles | F0, L0, CUDA.SURFACE.0 | COMPUTE.BACKEND.0-1 -> BACKEND.PROFILE.0-5 -> HARDWARE.PROFILE.0-2 -> HARDWARE.PROFILE.MAC.0 -> COMPUTE.BACKEND.METAL.0-1 -> GRAPH.METAL.0 -> BACKEND.METAL.0 -> BACKEND.ROCM.0 -> V010.BACKEND.0-12 -> V010.BUILD.0-7 -> V010.HARDWARE.0-6 | capability matrix or memory pressure report when graph needs it | Backend capability is not model support. |

### BLOCK 6 - Graph And Attention

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.GRAPH | primitives, selected slices, controlled blocks/layers, target tensor graph path | E0, M4-M6, GRAPH.OPS.0-3, GRAPH.BLOCK.0, GRAPH.LAYERS.0, GRAPH.CHECK.0 | V010.GRAPH.PRIM.0-10 -> V010.GRAPH.0-24 | after tensor/backend requirements | Fixtures and selected slices are not full transformer execution. |
| TRACK.ATTENTION | Q/K/V/O roles, RoPE, masks, head layout, attention execution, KV interaction | ATTENTION.CLASS.0, GRAPH.OPS.1 | ATTENTION.MODES.0 -> V010.ATTN.0-13 | after tensor collection coverage | Attention report/primitive is not target-backed attention runtime. |

### BLOCK 7 - Runtime State

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.PREFILL | token/context input into runtime state before decode | M8, PREFILL.1-3 | PREFILL.4-5 -> PREFILL.MODES.0 -> V010.PREFILL.0-15 | blocked until graph/attention/KV path exists | Diagnostic prefill is not full transformer prefill. |
| TRACK.DECODE | one-step and repeated decode over runtime state and KV | DECODE.0 | M10 -> DECODE.1 -> DECODE.MODES.0 -> V010.DECODE.0-11 | after prefill/KV path exists | Diagnostic decode is not full-runtime decode. |
| TRACK.LOGITS | output head, logits buffer, logprob/top-k diagnostics | LOGITS.0 | M11-M12 -> LOGITS.1 -> LOGITS.MODES.0 -> V010.LOGITS.0-12 | after decode and output-head tensor path | Diagnostic logits are not output-head logits. |
| TRACK.SAMPLING | greedy, stochastic, top-k/top-p/min-p/typical, seed policy | SAMPLING.0 | M13 -> SAMPLING.1 -> SAMPLING.MODES.0 -> V010.SAMPLE.0-10 | after output-head logits | Diagnostic sampling is not vocabulary sampling quality. |
| TRACK.GENERATION | decode -> logits -> sample -> append -> stop -> cleanup | GEN.CONTRACT.0, GEN.TRACE.0, GEN.APPEND.0, GEN.STOP.0, GEN.LOOP.0-1, CLI.GEN.0 | M14-M17 -> GENERATION.MODES.0 -> V010.GEN.0-19 -> GEN.DEEPSEEK.0 -> GEN.QWEN.METAL.0 | after decode/logits/sampling are target-backed | Bounded diagnostic generation is not full model generation. |
| TRACK.RUNTIME | engine/session state, trace, cancellation, lifecycle, cleanup | G0, M3, M7-M9, GEN.LOOP.1, GEN.TRACE.0 | TENSOR.TRACE.0 -> V010.RUNTIME.0-12 -> V010.TRACE.0-8 | after runtime path expands | Diagnostic lifecycle proof is not provider generation. |

### BLOCK 8 - Operator And Serving Surfaces

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.OPERATOR | CLI commands, paths, prepare/check, graph check, doctor, REPL clarity | H0, I0, J0, CLI.PACKAGE.0-1, CLI.CONSOLE.0, CLI.SURFACE.0-3, CLI.MODELS.0-4, OPERATOR.FLOW.0-2, OPERATOR.PATHS.0, MODEL.TARGET.PATHS.0, MODEL.PREPARE.0-1, MODEL.CHECK.0, GRAPH.CHECK.0 | MODEL.CHECK.1 -> CHAT.UX.0 -> OPERATOR.FLOW.3 -> DOCS.README.OPERATOR.0 -> V010.CLI.0-12 -> V010.DOCTOR.0-6 -> V010.PATHS.0-5 | MODEL.CHECK.1 or selected CLI hardening row | Operator presets compose lower behavior only. |
| TRACK.SERVE | daemon state, provider endpoints, streaming, observability | K0 server/provider status shell | SERVE.RUNTIME.0 -> SERVER.RUNTIME.0-1 -> SERVER.GEN.0 -> SERVER.API.0-1 -> SERVER.STREAM.0 -> SERVER.OBS.0 -> V010.SERVE.0-10 | after generation path exists | Status shell is not serving generation. |

### BLOCK 9 - Evidence, CI, Docs, Release, And Later Lanes

| Track | Owns | Completed rows | Planned / future rows | Next | Boundary |
| --- | --- | --- | --- | --- | --- |
| TRACK.EVAL | fixture/runtime/generation/capability evaluation | test runners only | EVAL.FIXTURE.0 -> EVAL.PARTIAL.0 -> EVAL.PREFILL.0 -> EVAL.KV.0 -> EVAL.DECODE.0 -> EVAL.LOGITS.0 -> EVAL.SAMPLING.0 -> EVAL.GEN.0 -> EVAL.CAPABILITY.0 -> V010.EVAL.0-10 | after implemented generation path | Eval waits for implemented path. |
| TRACK.BENCH | reproducible performance harness, profile output, throughput reporting | benchmark doctrine only | BENCH.RUNTIME.0 -> BENCH.PREFILL.0 -> BENCH.DECODE.0 -> BENCH.GEN.0 -> BENCH.MEMORY.0 -> BENCH.SERVER.0 -> BENCH.DEEPSEEK.DECODE.0 -> BENCH.DEEPSEEK.GEN.0 -> BENCH.QWEN.METAL.0 -> V010.BENCH.0-12 -> V010.PROFILE.0-8 | after measured path exists | No benchmark without measured runtime path and metadata. |
| TRACK.SPEC | draft, target verification, accepted-token accounting, verification cost | SPINE.SPEC.VERIFICATION.0, DSpark reference doctrine | SPEC.DSPARK.REF.0 -> SPEC.VERIFY.0 -> SPEC.VERIFY.COST.0 -> SPEC.MOE.ROUTING.0 -> SPEC.MOE.EXPERT.BUDGET.0 -> SPEC.DEEPSEEK.ACCOUNTING.0 -> SPEC.DEEPSEEK.0 -> BENCH.DEEPSEEK.SPEC.0 -> V010.SPEC.0-12 -> POST010.SPEC.0-12 | post-v0.1.0 after baseline generation | Speculative rows cannot precede baseline generation. |
| TRACK.DOCS | public/internal docs, runbooks, diagrams, claim hygiene | REPO.OPERATING.0, DOCS.PUBLIC.0, DOCS.MIN.0, DOCS.OPERATOR.RUNBOOK.0, DOCS.RUNBOOKS.MODEL.0, SPINE.* governance rows | DOCS.README.2 -> DOCS.RUNBOOK.1-4 -> DOCS.CONTRACT.1 -> DOCS.API.1 -> DOCS.PUBLIC.1 -> DOCS.README.TARGETS.0 -> DOCS.DIAGRAMS.0-6 -> V010.DOCS.0-12 -> V010.PAPER.0-4 | docs follow implemented/runtime evidence | Docs do not create runtime capability. |
| TRACK.CI | tests, source surface, artifact scans, release validation | TEST.SURFACE.0, REPO.SURFACE.0-1, CODE.NATURAL.0-1, REWRITE.CORE.COMPRESS.0, REWRITE.GRAPH.INPLACE.1, REWRITE.RUNTIME.INPLACE.1, REWRITE.MODEL.ARTIFACTS.INPLACE.1, YVEX.SKELETON.0 | SPINE.TESTMAP.0 -> SPINE.FILEMAP.0 -> V010.CI.0-12 | row-to-test map or release bundle | CI proves existing behavior; it does not create behavior. |
| TRACK.RELEASE | version, package, transcript, tag readiness | v0.1.0 doctrine only | V010.VERSION.0-3 -> V010.PACKAGE.0-5 -> V010.RELEASE.0-8 | after gates pass | Release rows cannot create runtime behavior. |
| TRACK.POST010 | serving compatibility, public benchmark tables, speculative acceleration, portability expansion | SPINE.METAL.QWEN.0, Qwen/Metal doctrine | POST010.SERVE.* -> POST010.BENCH.* -> POST010.SPEC.* -> POST010.METAL.* -> POST010.ROCM.* | after v0.1.0 | Later lanes cannot block v0.1.0 unless selected. |

## 3. Active Next Contract

### MOE.CLASS.0 - MoE model-class report

Primary block: BLOCK 3 - Model Class And Tensor Collections.

Must produce:

```text
expert count
active expert count
router facts
shared expert facts
expert tensor classes
MoE storage/residency pressure
runtime blockers
next required rows
```

Must not claim:

```text
router execution
expert activation
expert dispatch
expert accumulation
full-runtime prefill
decode over full-runtime state
output-head logits
generation
evaluation
benchmark
throughput
```

Reason:

```text
FAMILY.RUNTIME.0 -> ATTENTION.CLASS.0 -> KV.CACHE.0 -> CONTEXT.CLASS.0
are closed as reports. The MoE branch now needs explicit expert/router/shared
expert facts before tensor, residency, graph, and runtime rows can advance.
```

## 4. Critical Path To v0.1.0

| Step | Gate | Track | Required delivery |
| --- | --- | --- | --- |
| 1 | GATE.SCOPE | TRACK.TARGET | choose full-runtime candidate and excluded lanes |
| 2 | GATE.ARTIFACT | TRACK.ARTIFACT / TRACK.INTEGRITY | artifact identity, digest, ranges, dtype/qtype, registry drift checks |
| 3 | GATE.CLASS | TRACK.MODEL | model class, target class, output/tokenizer/runtime blockers |
| 4 | GATE.TENSOR | TRACK.TENSOR | required runtime tensor collections mapped |
| 5 | GATE.RESIDENCY | TRACK.RESIDENCY / TRACK.KV | tensor and runtime-state placement decisions |
| 6 | GATE.BACKEND | TRACK.BACKEND | CPU baseline and selected backend capability reports |
| 7 | GATE.GRAPH | TRACK.GRAPH / TRACK.ATTENTION / TRACK.MOE | target-backed graph path with reference/checksum/failure reports |
| 8 | GATE.PREFILL | TRACK.PREFILL | full transformer prefill for selected target |
| 9 | GATE.KV | TRACK.KV | attention-backed KV write/read/cleanup |
| 10 | GATE.DECODE | TRACK.DECODE | decode over implemented KV |
| 11 | GATE.LOGITS | TRACK.LOGITS | output-head logits |
| 12 | GATE.SAMPLE | TRACK.SAMPLING | greedy sampling over output-head logits |
| 13 | GATE.TOKENIZER | TRACK.TOKENIZER | token ID input/output and stop boundary |
| 14 | GATE.GEN | TRACK.GENERATION | CLI generation composes prefill, KV, decode, logits, sampling, append, stop, cleanup |
| 15 | GATE.OPERATOR | TRACK.OPERATOR | short operator proof path |
| 16 | GATE.EVIDENCE | TRACK.EVAL / TRACK.CI | smoke/evidence transcript over implemented path |
| 17 | GATE.RELEASE | TRACK.RELEASE | version, tag, clean tree, artifact scan, claim audit |

Compact path:

```text
target -> artifact -> integrity -> class -> tensor -> residency/backend -> graph
-> prefill -> KV -> decode -> logits -> sampling -> tokenizer/stop -> generation
-> operator proof -> eval smoke -> CI/docs audit -> release
```

## 5. Governance Rows

Governance rows do not change runtime capability.

| Row | Status | Changed | Current role |
| --- | --- | --- | --- |
| SPINE.REBASE.1-5 | complete | historical spine rebases and ledger consolidation | historical only |
| SPINE.BLOCKS.0 | complete | block ownership doctrine | folded into this delivery table |
| SPINE.BLOCKS.1 | planned | row deduplication cleanup | superseded unless a future cleanup needs it |
| SPINE.OPERATOR.PRESET.0 | complete | operator preset doctrine | folded into TRACK.OPERATOR |
| SPINE.GENERATION.TARGET.0 | complete | DeepSeek generation target envelope | folded into TRACK.GENERATION / TRACK.BENCH |
| SPINE.SPEC.VERIFICATION.0 | complete | verification-cost doctrine | folded into TRACK.SPEC |
| SPINE.SEQUENCE.REBASE.0 | complete | runtime sequence normalization | superseded by block/track delivery table |
| SPINE.V0_1_0.MASTER.0 | historical | early v0.1.0 release spine | superseded |
| SPINE.V0_1_0.MASTER.1 | complete | exhaustive v0.1.0 expansion | collapsed into delivery rows above |
| SPINE.V0_1_0.MASTER.2 | complete | row contract doctrine | folded into delivery contract rules |
| SPINE.AUDIT.0 | complete | whole-repo implementation/spine audit | audit history |
| SPINE.RECONCILE.0 | complete | evidence taxonomy and namespace rules | reconciliation history |
| SPINE.NAVIGATION.0 | complete | navigation/control panel | superseded |
| SPINE.REDESIGN.0 | complete | track-based redesign | superseded by compact delivery spine |
| SPINE.COLLAPSE.0 | complete | structural collapse attempt | superseded by compact delivery spine |
| SPINE.TABLE.0 | complete | rewrote spine as block/track/wave delivery table | current document shape |
| SPINE.METAL.QWEN.0 | complete | Qwen/Metal pressure lane | folded into TRACK.POST010 / TRACK.BACKEND |
| SPINE.TESTMAP.0 | planned | test-to-row map | TRACK.CI follow-up |
| SPINE.FILEMAP.0 | planned | file-to-owner map | TRACK.CI follow-up |
| SPINE.COMMAND.AUDIT.0 | planned | command/help/output-boundary audit | future audit note |
| SPINE.CLAIM.AUDIT.0 | planned | public/internal claim audit | release audit note |
| SPINE.CAPABILITY.REWRITE.0 | contingency | capability rewrite if overclaim recurs | inactive |
| SPINE.PUBLIC.CLAIM.0 | contingency | public claim correction if needed | inactive |

## 6. Validation

Baseline validation:

```sh
make clean
make check
make smoke
make check-cuda
git diff --check
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
```
