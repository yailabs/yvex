# DeepSeek Operator Runbook

Date: 2026-07-11
Status: unsupported current-state boundary

## Current Target

Canonical source:

```text
$HOME/lab/models/hf/deepseek/DeepSeek-V4-Flash
```

Canonical v0.1.0 release target:

```text
deepseek4-v4-flash
```

Release machine and backend:

```text
DGX Spark
CUDA
```

The local source has passed exact metadata and header verification against
`deepseek-ai/DeepSeek-V4-Flash` at commit
`60d8d70770c6776ff598c94bb586a859a38244f1`. The authoritative inventory is the
upstream `model.safetensors.index.json`: 46 of 46 shard headers parse and expose
69,187 unique tensor records. The verified local footprint is 159,629,046,930
bytes after admission of the pinned index and its provider metadata. Tensor
payload bytes remain unread and untrusted. The target remains unsupported.

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
config and pinned index facts, and performs one safetensors-header pass:

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
may atomically promote a complete verifier-owned manifest under the YVEX model
registry only after those facts pass and the manifest reopens consistently. A
user-provided status cannot perform that transition. The command never reads
tensor payload bytes and does not prove artifact or runtime support. Current
milestone state remains in `../../PROJECT.md`.

## Canonical Control

- Project control: `../../PROJECT.md`
- Release gates: `../v010-release-doctrine.md`
- Filesystem ownership: `../system-target.md`
- Artifact terminology: `../../MODEL_ARTIFACTS.md`

This runbook intentionally contains no selected-slice execution lane and no
future command syntax.
