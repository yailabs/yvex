# YVEX Runbooks

These runbooks are operator copy-pack lanes.

Use a model-specific runbook when working with a concrete target.

## Files

- `deepseek.md` - current DeepSeek selected artifact path from local safetensors to selected graph execution, bounded diagnostics, and fullmodel inventory/materialization-plan reports.
- `glm.md` - GLM-5.2 official-source tensor target status, source-only fullmodel unsupported report, and download start/status checks.
- `common.md` - shared configuration, fullmodel command-surface checks, graph-only checks, daemon diagnostics, and repository validation.

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
classes, backend fit estimates, blockers, cleanup, and next proof readiness.
It does not materialize full weights, allocate the full backend payload, run
full model execution, generate, evaluate, benchmark, or report throughput.
