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
proof residue and are owned by the decommission map in the repair spine.

There is no supported DeepSeek generation command to run yet.

## Active Blocker

`V010.DOCS.REFOUNDATION.0` is complete at documentation/claim refoundation
only.

Active Next:

```text
V010.REBASE.DEEPSEEK.0
```

The main runtime path is blocked until the ordered foundation repair spine
closes through `V010.GGUF.ROUNDTRIP.1` over a complete YVEX-produced artifact.

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

- Main product spine: `../spine.md`
- Priority-blocking repair spine: `../repair/v010-foundation-closure.md`
- Release gates: `../v010-release-doctrine.md`
- Filesystem ownership: `../system-target.md`
- Artifact terminology: `../../MODEL_ARTIFACTS.md`

This runbook intentionally contains no selected-slice execution lane and no
future command syntax.
