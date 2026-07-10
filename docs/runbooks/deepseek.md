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

## Safe Current Checks

These checks inspect repository or filesystem state only. They do not prove
source identity, artifact support, or runtime capability.

```sh
test -d "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash"
find "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash" \
  -maxdepth 1 -type f -name '*.safetensors' | sort
test -f "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash/config.json"
test -f "$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash/tokenizer.json"
```

## Canonical Control

- Project control: `../../PROJECT.md`
- Release gates: `../v010-release-doctrine.md`
- Filesystem ownership: `../system-target.md`
- Artifact terminology: `../../MODEL_ARTIFACTS.md`

This runbook intentionally contains no selected-slice execution lane and no
future command syntax.
