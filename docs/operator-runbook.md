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
pressure, and full-model runtime blocker reporting without payload
materialization or generation claims.

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
- fullmodel inventory and placement blocker reports;
- CUDA selected checks;
- daemon and accepted-only diagnostics.

### GLM

Use this when working with the current GLM-5.2 official-source tensor target.

- `docs/runbooks/glm.md`

Covers:

- GLM target path reporting;
- GLM source-only fullmodel unsupported report;
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
- fullmodel report help and command-surface checks;
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
```

Boundary:

Path configuration and target path reporting do not download weights, create
artifacts, register aliases, materialize tensors, run graph execution, or claim
runtime support.

For the current DeepSeek selected embedding path, use
`docs/runbooks/deepseek.md`.
