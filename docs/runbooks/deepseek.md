# DeepSeek Operator Runbook

Date: 2026-07-10
Status: unsupported current-state boundary

## Current Target

Canonical source:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Future canonical full target:

```text
deepseek4-v4-flash
```

Release machine and backend:

```text
DGX Spark
CUDA
```

The local source path is present at the time of this refoundation, but the
source verification gate has not closed. The full target is not yet a supported
target in YVEX.

## Current Boundary

YVEX cannot currently produce a complete DeepSeek-V4-Flash GGUF, materialize
the complete model, build an execution-complete runtime descriptor, execute the
full transformer and MoE stack, or generate DeepSeek text.

Do not use selected aliases, bounded graph segments, diagnostic runtime
commands, report-only fullmodel surfaces, fixture logits, or printed token IDs
as a DeepSeek operator lane. Those implementation surfaces remain temporary
proof residue; their required dispositions and consuming milestones are owned
by the decommission obligations in `../../PROJECT.md`.

There is no supported DeepSeek generation command to run yet.

## Project State

Current milestone state, dependencies, gates, and Active Next live only in
`../../PROJECT.md`. This runbook records the executable operator boundary and
does not reproduce the project sequence.

## Source Verification

The strict source command resolves the canonical target independently from its
source directory name, parses structured provenance, model/tokenizer/generation
config and index facts, and reads safetensors headers only:

```sh
./yvex source-manifest report \
  --release v0.1.0 \
  --family deepseek \
  --target deepseek4-v4-flash \
  --models-root "$HOME/lab/models" \
  --audit \
  --strict
```

Strict mode exits non-zero for missing or contradictory provenance, malformed
config/tokenizer/index metadata, inconsistent token sidecars, missing or
unexpected shards, invalid headers, or checked-footprint overflow. The command
never reads tensor payload bytes and does not prove artifact or runtime support.
Current milestone state and the authoritative blocker remain in
`../../PROJECT.md`.

## Canonical Control

- Project control: `../../PROJECT.md`
- Release gates: `../v010-release-doctrine.md`
- Filesystem ownership: `../system-target.md`
- Artifact terminology: `../../MODEL_ARTIFACTS.md`

This runbook intentionally contains no selected-slice execution lane and no
future command syntax.
