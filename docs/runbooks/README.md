# YVEX Runbooks

These runbooks are operator copy-pack lanes.

Use a model-specific runbook when working with a concrete target.

## Files

- `deepseek.md` - current DeepSeek selected artifact path from local safetensors to selected graph execution, bounded diagnostics, and fullmodel inventory/materialization-plan/materialize reports.
- `glm.md` - GLM-5.2 official-source tensor target status, source-only fullmodel unsupported report/refusal, and download start/status checks.
- `common.md` - shared configuration, fullmodel report/plan/materialize command-surface checks, graph-only checks, daemon diagnostics, and repository validation.

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
boundary. It does not run full model execution, generate, evaluate, benchmark,
or report throughput.
