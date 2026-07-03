# YVEX Operator Runbook

## What This File Is

This file is the entry point for YVEX operator runbooks.

The detailed copy-pack lanes live under `docs/runbooks/`.

Use the model-specific runbook when working with a concrete model target.

YVEX now exposes bounded diagnostic decode, logits, greedy sampling, and a
bounded diagnostic generation loop with explicit generated-token append
accounting, explicit stop-policy reporting, deterministic local cancellation,
partial-output preservation, cleanup lifecycle reporting, and bounded
trace-level reporting over the implemented selected segment path. The
`yvex generate` operator surface has stable help, normal/trace/cancel/context
examples, deterministic refusal wording, command-inventory description, and
text diagnostic output. It still does not implement full model generation,
provider generation, streaming generation, capability evaluation, or benchmarks.
YVEX also exposes `yvex fullmodel report` for GGUF metadata/tensor-directory
inventory, tensor collection candidate classification, memory/placement
pressure, and full-model runtime blocker reporting. `yvex fullmodel
materialization-plan` turns that inventory into placement phases, residency
classes, backend fit estimates, preflight blockers, cleanup planning, and
next proof readiness reporting. `yvex fullmodel materialize` proves a bounded
tiny/full-ish required-tensor allocation-and-release path or refuses selected,
source-only, incomplete, oversized, missing, and corrupt artifacts with phase
and cleanup reports. `yvex fullmodel descriptor` reports runtime tensor roles,
collections, residency expectations, graph requirements, prefill/KV/decode/
logits/sampling requirements, output-head/tokenizer requirements, backend
requirements, and blockers. It still does not claim full model execution,
DeepSeek generation, provider generation, streaming generation, evaluation,
benchmark, or throughput.
`yvex fullmodel family-runtime` maps those descriptor facts into a
model-family runtime adapter report. The first concrete report target is
DeepSeek; GLM remains a source-only refusal until a YVEX-produced GGUF tensor
inventory exists. This is still a report boundary, not runtime execution.
`yvex attention report` turns those DeepSeek-family facts into attention class,
head layout, Q/K/V/O role, RoPE/position, mask, KV, context, graph/backend, and
blocker reporting. It is report-only: no full transformer attention, no real
QKV projection, no real attention-backed KV, no full model execution, no
generation, and no benchmark.
`yvex kv report` turns the attention/family facts into a KV cache class report:
diagnostic-versus-real KV boundary, layout, dtype, layer/head/position indexing,
capacity, context dependency, residency class, attention dependency, and next
runtime blockers. It does not allocate full runtime KV, write real
attention-backed KV, read KV from real decode, generate, evaluate, benchmark, or
report throughput.
`yvex context report` makes model/requested/active context, token counts,
chunking, overflow behavior, prefill boundary, decode position policy,
attention/KV dependency, and blockers visible. It is report-only and does not
run full transformer prefill, execute real decode, extend context, generate,
evaluate, benchmark, or report throughput.
`yvex moe report` makes MoE class facts visible: model_is_moe, expert count
status, active expert count status, router tensor facts, shared expert facts,
expert tensor roles, storage/residency pressure, blockers, and next rows. It is
report-only and does not execute router logits, top-k routing, expert
activation, expert dispatch, expert accumulation, graph/prefill/decode
integration, generation, evaluation, benchmark, or throughput.
`yvex tensor-collection report --collection moe` makes MoE tensor collection
coverage visible: router role coverage, expert gate/up/down roles, shared
expert status, dispatch metadata, indexing policy, present/missing/unknown
roles, storage pressure, residency pressure, blockers, and next rows. It is
report-only and does not materialize tensors, execute router logits, select
experts, dispatch experts, accumulate expert outputs, run MoE blocks, run
prefill/decode/logits/sampling, generate, evaluate, benchmark, or report
throughput.
`yvex model-target decision --release v0.1.0` records the current target
decision state. `yvex model-target candidate --release v0.1.0` reports
full-runtime candidate eligibility across selected slices, source-only targets,
pressure lanes, fixtures, blockers, and next rows. `yvex model-target
dense-candidate --release v0.1.0` reports whether any dense target can become
the first honest v0.1.0 full-runtime candidate. These commands are report-only
and do not download models, emit artifacts, materialize tensors, execute graph
work, run prefill, decode, logits, sampling, generation, evaluation,
benchmarks, or mark the release ready.

## Runbook Index

### DeepSeek

Use this when working with the current selected DeepSeek pressure artifacts.

- `docs/runbooks/deepseek.md`

Covers:

- local DeepSeek safetensors to YVEX-produced selected GGUF;
- selected embedding prepare preset;
- selected embedding alias registration;
- selected artifact integrity and materialization;
- selected embedding graph execution;
- selected embedding-plus-RMSNorm segment;
- segment-summary prefill;
- minimal KV diagnostics;
- bounded diagnostic generate help and copy-pack commands;
- fullmodel inventory, materialization planning, proof/refusal, runtime descriptor, family-runtime, attention, KV cache class, context class, MoE class, and MoE tensor collection reports;
- CUDA selected checks;
- daemon and accepted-only diagnostics.

### GLM

Use this when working with the current GLM-5.2 official-source tensor target.

- `docs/runbooks/glm.md`

Covers:

- GLM target path reporting;
- GLM source-only fullmodel unsupported report, descriptor, family-runtime, attention, KV, context report, MoE report, MoE tensor collection report, and materialization refusal;
- GLM source-download start and status checks;
- Hugging Face CLI and repository namespace boundaries;
- GLM boundaries.

GLM remains source evidence only. It is not GLM execution or GLM generation.

### Common

Use this for model-independent checks.

- `docs/runbooks/common.md`

Covers:

- operator storage configuration;
- fast regression;
- target decision, full-runtime candidate, and dense candidate reporting;
- fullmodel report, materialization-plan, materialize, descriptor, family-runtime, attention, KV, context, MoE, and tensor-collection command-surface checks;
- graph-only fixture regression;
- daemon and accepted-only runtime diagnostics;
- repository validation and artifact hygiene.

## Current Short Entry

```sh
./yvex paths configure --models-root "$HOME/lab/models" --create
./yvex model-target list
./yvex model-target inspect deepseek4-v4-flash-selected-embed --paths
./yvex model-target inspect deepseek4-v4-flash-selected-embed-rmsnorm --paths
./yvex model-target inspect glm-5.2-official-safetensors --paths
./yvex model-target decision --release v0.1.0 --include-candidates --include-blockers --include-next
./yvex model-target candidate --release v0.1.0 --include-candidates --include-blockers --include-next
./yvex model-target dense-candidate --release v0.1.0 --include-candidates --include-requirements --include-blockers --include-next
```

Boundary:

Path configuration and target path reporting do not download weights, create
artifacts, register aliases, materialize tensors, run graph execution, or claim
runtime support. Target decision and candidate reporting also do not select an
implemented runtime path, claim generation, evaluate capability, benchmark
throughput, or mark a release ready.

For the current DeepSeek selected embedding path, use
`docs/runbooks/deepseek.md`.
