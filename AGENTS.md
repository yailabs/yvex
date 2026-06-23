# AGENTS.md

## 1. Project Identity

YVEX means YAI Vector Execution.

YVEX is a C local inference engine and local provider runtime for open-weight
models. YVEX executes models. YAI controls case state, authority, governance,
tools, records, memory, and operational use.

YVEX is CLI-first.

Repository-local entrypoints:

```sh
./yvex
./yvexd
```

Compiled binaries:

```text
build/bin/yvex
build/bin/yvexd
```

`./yvex` is the operator/developer CLI.
`./yvexd` is the server/provider daemon shell.

## 2. Current Runtime Posture

```text
generation: unsupported
inference: not implemented
execution_ready: false
server_generation: not implemented
```

Implemented now:

```text
GGUF parse
metadata parse
tensor directory parse
dtype/qtype registry
tensor table
model descriptor
tokenizer fixture path
prompt rendering fixture path
graph/planner substrate
CPU backend
CUDA backend
selected tensor conversion
selected tensor GGUF emission
selected tensor materialization
model gate
artifact naming
```

Not implemented:

```text
full model materialization
engine weight attachment
graph execution on real model
prefill
decode
KV cache runtime
sampler
generation
OpenAI-compatible generation
benchmark claims
```

## 3. Active Target Policy

DeepSeek V4 Flash is the active live model target.

Qwen3-8B is historical validation evidence only. Do not add new Qwen live gates,
Qwen runtime targets, or Qwen-first milestones unless `docs/spine.md` explicitly
re-authorizes Qwen.

Tiny fixtures may still be used for deterministic unit/CLI tests.

## 4. Canonical External Artifacts

DeepSeek selected embedding GGUF:

```text
/home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Qwen historical selected embedding GGUF:

```text
/home/dgmothx/lab/models/gguf/qwen/qwen3-8b-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Artifact details live in:

```text
MODEL_ARTIFACTS.md
```

Generated model artifacts live outside the repository.

## 5. Repository Reading Order

Before implementing project work, read in this order:

```text
1. docs/spine.md
2. AGENTS.md
3. MODEL_ARTIFACTS.md
4. docs/cli-commands.md
5. docs/cli-runtime.md
6. docs/api.md
7. docs/backend-contract.md
8. docs/runtime-filesystem.md
```

`docs/spine.md` is authoritative for the next milestone.
This file is authoritative for operating discipline.
`MODEL_ARTIFACTS.md` is authoritative for current external artifact posture.

## 6. Hard Boundaries

Never claim:

```text
full model support
inference ready
generation ready
prefill ready
decode ready
KV runtime ready
OpenAI compatibility
benchmark results
tokens/sec
TTFT
execution_ready: true
```

Never commit:

```text
*.safetensors
*.bin
*.dat
external generated *.gguf
server logs
pid files
external model reports
external benchmark reports
```

Never hide:

```text
missing file
hash mismatch
bad GGUF parse
tensor spec mismatch
unsupported dtype
unsupported qtype
backend allocation failure
backend copy failure
CUDA unavailable
OOM
```

Never upgrade:

```text
selected-tensor-materialized
```

into:

```text
full-weights-materialized
```

without full tensor coverage and command proof.

## 7. Allowed Support Vocabulary

Use only these support levels unless the spine adds more:

```text
none
descriptor-only
selected-tensor-materialized
full-weights-materialized
partial-graph-executable
prefill-ready
decode-ready
generation-ready
```

Current real-model support level:

```text
DeepSeek V4 Flash: selected-tensor-materialized
Qwen3-8B: selected-tensor-materialized, historical only
```

## 8. Command Discipline

Repository-local commands should use:

```sh
./yvex
./yvexd
```

Examples:

```sh
./yvex commands
./yvex inspect FILE
./yvex tensors FILE
./yvex materialize --model FILE --backend cuda
./yvex model-gate check ...
./yvexd --help
```

Global commands may use:

```sh
yvex
yvexd
```

only after installing/linking the root launchers into PATH.

## 9. Validation Contract

Baseline validation for implementation changes:

```sh
make clean
make check
make smoke
git diff --check
```

CUDA provider-node validation:

```sh
make check-cuda
```

Artifact guardrail:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected:

```text
no tracked safetensors/bin/dat
tracked GGUF only intentional tiny fixtures under tests/fixtures/gguf
```

Forbidden reference scan:
Use the project's established forbidden reference scan from `docs/spine.md` or
current validation scripts. Do not add prohibited reference names back into this
file.

Forbidden claim scan:

```sh
git grep -n -i \
  -e 'full deepseek support' \
  -e 'deepseek inference' \
  -e 'inference ready' \
  -e 'generation ready' \
  -e 'execution_ready: true' \
  -e 'tokens/sec' \
  -e 'ttft' \
  -e 'decode tps' \
  -- . || true
```

Positive matches must be absent. Negative/non-goal references are allowed.

## 10. Completion Report Requirements

Every completed implementation report must include:

```text
commit hash
files created
files modified
public API added
CLI surface added
tests added
live proof if applicable
validation commands
validation result
artifact guardrail result
forbidden reference scan result
non-goals preserved
next internal delivery target from docs/spine.md
```

## 11. Live DeepSeek Commands

Set:

```sh
export DS_GGUF="$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
```

Inspect:

```sh
./yvex inspect "$DS_GGUF"
./yvex tensors "$DS_GGUF"
```

Materialize:

```sh
./yvex materialize --model "$DS_GGUF" --backend cpu
./yvex materialize --model "$DS_GGUF" --backend cuda
```

Model gate:

```sh
./yvex model-gate check \
  --model "$DS_GGUF" \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4096,129280 \
  --expect-dtype F16 \
  --expect-bytes 1059061760 \
  --backend cpu \
  --backend cuda \
  --require-cpu \
  --require-cuda
```

## 12. Server / Provider Shell

`./yvexd` is the server/provider daemon shell.

Current implemented server endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

Generation endpoints are intentionally unsupported.

Typical foreground server:

```sh
./yvexd --host 127.0.0.1 --port 18080 --model "$DS_GGUF" --backend cuda
```

When server lifecycle/logging support exists, prefer documented `--log-file`,
`--pid-file`, and `--background` usage.

## 13. Agent Work Rules

Agents must:

1. Follow `docs/spine.md`.
2. Keep DeepSeek as active live target unless spine changes.
3. Treat Qwen as historical evidence only unless spine changes.
4. Use `./yvex` and `./yvexd` in repo-local docs/examples.
5. Add command-visible proof for command-visible behavior.
6. Keep generated artifacts outside repo.
7. Preserve `execution_ready: false` until real execution exists.
8. Prefer precise failure classes.
9. Keep runtime and open-weight tools separated.
10. Stop rather than inventing support claims.

## 14. Current Next Work

The next implementation sequence is defined only by `docs/spine.md`.

Do not infer next work from chat memory, filenames, or old reports.
