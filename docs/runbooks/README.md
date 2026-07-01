# YVEX Runbooks

These runbooks are operator copy-pack lanes.

Use a model-specific runbook when working with a concrete target.

## Files

- `deepseek.md` - current DeepSeek selected artifact path from local safetensors to selected graph execution.
- `glm.md` - GLM-5.2 official-source tensor target status and download start and status checks.
- `common.md` - shared configuration, graph-only checks, daemon diagnostics, and repository validation.

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
accounting and stop-policy reporting over the selected segment path. It does
not implement full model generation, provider generation, capability evaluation,
or benchmarks.
