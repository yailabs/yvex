# YVEX Runbooks

These runbooks are operator copy-pack lanes.

Use a model-specific runbook when working with a concrete target.

## Files

- `deepseek.md` - current DeepSeek selected artifact path from local safetensors to selected graph execution, bounded diagnostics, fullmodel inventory/materialization-plan/materialize/descriptor/family-runtime reports, attention class reports, KV cache class reports, context class reports, MoE class reports, and MoE tensor collection reports.
- `glm.md` - GLM-5.2 official-source tensor target status, source-only fullmodel, attention, KV, context, MoE, and MoE tensor-collection unsupported report/descriptor/refusal, and download start/status checks.
- `model-families.md` - family-level pressure lanes such as Qwen on Apple Silicon / Metal before they become concrete model-specific operator paths.
- `common.md` - shared configuration, fullmodel report/plan/materialize/descriptor/family-runtime, attention, KV, context, MoE, and tensor-collection command-surface checks, graph-only checks, daemon diagnostics, and repository validation.

## Rules

Do not paste every file.

Pick the file for the model or task.

Each lane states:

- purpose;
- requirements;
- writes;
- rerun safety;
- stop point;
- boundary.

Current YVEX exposes bounded diagnostic decode, logits, greedy sampling, and a
bounded diagnostic generation loop with explicit generated-token append
accounting, stop-policy reporting, and bounded trace-level reporting over the
selected segment path. It does not implement full model generation, provider
generation, capability evaluation, or benchmarks.

Current YVEX also exposes `yvex fullmodel report` as an inventory and placement
blocker report over GGUF metadata and tensor-directory facts only. `yvex
fullmodel materialization-plan` reports planned placement phases, residency
classes, backend fit estimates, blockers, cleanup, and proof readiness. `yvex
fullmodel materialize` allocates and releases only bounded required proof
tensors for a controlled tiny/full-ish GGUF, or refuses artifacts outside that
boundary. `yvex fullmodel descriptor` reports tensor roles, tensor collections,
residency, graph, prefill, KV, decode, logits, sampling, output-head,
tokenizer, backend requirements, and blockers. It does not run full model
execution, generate, evaluate, benchmark, or report throughput.

`yvex attention report` maps DeepSeek-family runtime facts into attention
class, head layout, Q/K/V/O roles, RoPE/position, mask, KV, context,
graph/backend, blocker, and next-dependency reporting. It does not run full
transformer attention, project real Q/K/V from model tensors, write real
attention-backed KV, generate, evaluate, benchmark, or report throughput.

`yvex tensor-collection report --collection moe` reports MoE router, expert,
shared-expert, dispatch, indexing, storage, and residency collection coverage.
It is report-only and does not materialize tensors, run router logits, select
or dispatch experts, run prefill/decode/logits/sampling, generate, evaluate,
benchmark, or report throughput.

`yvex kv report` maps those requirements into KV cache class facts: diagnostic
KV versus real attention KV, layout, dtype, layer/head/position indexing,
capacity, residency class, context dependency, attention dependency, and next
runtime blockers. It does not allocate full runtime KV, write or read real
attention-backed KV, generate, evaluate, benchmark, or report throughput.

`yvex context report` maps context facts into model/requested/active context,
token counts, chunking policy, overflow behavior, prefill boundary, decode
position policy, attention/KV dependency, and next runtime blockers. It does
not run full transformer prefill, execute real decode, extend context, generate,
evaluate, benchmark, or report throughput.
