# YVEX CLI Command Index

This is the quick command map for the current YVEX command-line surface.

`docs/cli-runtime.md` owns detailed CLI behavior. This file is intentionally
short: update it when a milestone adds, removes, renames, or materially changes
a command, option, exit status, or visible runtime posture.

## Current Milestone

```text
CLI.MODELS.2 - Model alias resolution for one-shot commands
```

## Operator References

Read:

```text
AGENTS.md
MODEL_ARTIFACTS.md
docs/spine.md
```

## Interface Spine

For CLI layout, REPL design, prompt/run/chat role boundaries, and line-editing
policy, see:

```text
docs/cli-interface-spine.md
```

## Model Selection Direction

Current one-shot model commands accept explicit model paths or registered model
aliases.

Local model registry management is implemented through `yvex models`.
CLI.MODELS.2 wires aliases into one-shot model commands. REPL selection and
`yvexd --model ALIAS` remain future work. See:

```text
docs/cli-interface-spine.md
```

Current canonical DeepSeek artifact:

```text
/home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
```

Current alias:

```text
deepseek4-v4-flash-selected-embed
```

Use repository-local launchers:

```sh
./yvex
./yvexd
```

## Binaries

Repository-local launchers:

```text
./yvex
./yvexd
```

Compiled binaries:

```text
build/bin/yvex
build/bin/yvexd
```

Global/local-user commands after symlink or install:

```text
yvex
yvexd
```

Installed local user links, when desired:

```sh
mkdir -p ~/.local/bin
ln -sf "$PWD/yvex" ~/.local/bin/yvex
ln -sf "$PWD/yvexd" ~/.local/bin/yvexd
```

Direct links to `build/bin/yvex` and `build/bin/yvexd` are lower-level. Linking
the root launchers preserves the clearer missing-build error.

## Global Posture

```text
generation: unsupported
inference: not implemented
execution_ready: false
server_generation: not implemented
source_manifest: provenance JSON writer implemented
native_weights: safetensors header inventory implemented
gguf_template: contract validator implemented
gguf_emit: controlled GGUF writer implemented
conversion: open-weight selected tensor bridge implemented
model_registry: local model alias registry implemented
model_ref: alias-or-path resolver implemented
quant_job: external quantization job manifest implemented
qtype_support: conversion support matrix implemented
weight_mapping: tensor adapter contract implemented
quant_policy: manifest validator implemented
imatrix: calibration artifact manifest implemented
```

CUDA in L0 means device probe, memory/tensor movement, and F32 embed parity. It
does not mean CUDA matmul, attention, session execution, generation, or model
support.

M0 adds fixture weight materialization into CPU/CUDA backend tensors. OWI.1
adds source-manifest provenance for external official-weight source trees. OWI.4
adds tensor mapping from native names to canonical roles and GGUF/template target
names. OWI.5 adds declarative quantization policy manifests. OWI.6 adds
calibration/imatrix provenance manifests and policy compatibility checks. OWI.7
adds controlled GGUF emission for one tiny F32 tensor. OWI.8 adds conversion
plan and selected-tensor GGUF emit from safetensors. OWI.9 adds a generic
external quantization job manifest and provenance surface. These do not add
native YVEX quantization suite support, imatrix generation, calibration
execution, model execution, or inference.

## `yvex` Commands

| Command | Usage | Status |
| --- | --- | --- |
| `backend` | `yvex backend cpu\|cuda` | implemented; backend status, memory, capabilities |
| `chat` | `yvex chat --model FILE --backend cpu\|cuda` | implemented; current diagnostic REPL, target canonical interactive console; aliases not wired here yet |
| `commands` | `yvex commands` | implemented; command table dump |
| `convert` | `yvex convert plan\|emit` | implemented; conversion plan and selected tensor GGUF emit |
| `cuda-info` | `yvex cuda-info` | implemented; CUDA probe, exit 5 when unavailable |
| `detokenize` | `yvex detokenize PATH_OR_ALIAS --ids IDS` | implemented for fixture tokenizer path |
| `engine` | `yvex engine PATH_OR_ALIAS` | implemented; descriptor/runtime diagnostics |
| `graph` | `yvex graph PATH_OR_ALIAS [--seq N] [--ctx N]` | implemented; inspect-only graph |
| `gguf-emit` | `yvex gguf-emit controlled --out FILE` | implemented; controlled one-tensor GGUF emission |
| `gguf-template` | `yvex gguf-template inspect\|validate --template FILE` | implemented; template contract validation |
| `help` | `yvex help [COMMAND]` | implemented |
| `imatrix` | `yvex imatrix create\|inspect\|validate` | implemented; calibration/imatrix manifest provenance |
| `info` | `yvex info` | implemented; honest support posture |
| `inspect` | `yvex inspect PATH_OR_ALIAS` | implemented; descriptor-only GGUF/model summary |
| `materialize` | `yvex materialize --model PATH_OR_ALIAS --backend cpu\|cuda` | implemented; fixture weights copied into backend tensors |
| `materialize-gate` | `yvex materialize-gate check --model PATH_OR_ALIAS ...` | implemented; repeatable materialization gate and failure classes |
| `metadata` | `yvex metadata PATH_OR_ALIAS` | implemented; GGUF metadata dump |
| `model-gate` | `yvex model-gate check --model PATH_OR_ALIAS ...` | implemented; selected/full support-level gate |
| `models` | `yvex models scan\|add\|list\|use\|current\|inspect\|remove` | implemented; local model registry management |
| `native-weights` | `yvex native-weights --source DIR [--limit N] [--tensor NAME] [--json]` | implemented; safetensors header inventory |
| `paths` | `yvex paths [--project DIR] [--run] [--create]` | implemented |
| `plan` | `yvex plan PATH_OR_ALIAS [--backend cpu\|cuda] [--seq N] [--ctx N]` | implemented; plan-only |
| `prompt` | `yvex prompt PATH_OR_ALIAS --user TEXT [--system TEXT] [--assistant TEXT] [--tokens]` | implemented; diagnostic renderer, not model generation |
| `quant-job` | `yvex quant-job create\|inspect\|validate` | implemented; external quantization job provenance |
| `quant-policy` | `yvex quant-policy inspect\|validate --policy FILE` | implemented; declarative qtype policy manifest |
| `qtype-support` | `yvex qtype-support` | implemented; conversion qtype support matrix |
| `run` | `yvex run --model PATH_OR_ALIAS --backend cpu\|cuda --prompt TEXT` | implemented; accepted-only diagnostic runtime path until inference exists |
| `session` | `yvex session PATH_OR_ALIAS --backend cpu\|cuda [--text TEXT] [--accept-tokens]` | implemented; diagnostics/token acceptance |
| `source-manifest` | `yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE` | implemented; source provenance JSON writer |
| `tensor-map` | `yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]` | implemented; native tensor to role/template mapping |
| `tokenize` | `yvex tokenize PATH_OR_ALIAS --text TEXT` | implemented for fixture tokenizer path |
| `tokenizer` | `yvex tokenizer PATH_OR_ALIAS` | implemented; tokenizer metadata/support posture |
| `tensors` | `yvex tensors PATH_OR_ALIAS` | implemented; tensor table dump |
| `version` | `yvex version` | implemented |

## `yvexd` Commands

`yvexd` is the provider/server daemon, not the interactive REPL.

```sh
yvexd --help
yvexd --version
yvexd --host HOST --port PORT [--model FILE] [--backend cpu|cuda] [--one-request]
```

Implemented endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

Unsupported generation endpoints return `501`:

```text
POST /v1/completions
POST /v1/chat/completions
POST /v1/responses
```

## Exit Codes

```text
0  success / implemented diagnostic path
2  invalid CLI arguments or unknown command/help topic
5  unsupported feature or unavailable backend/runtime
```

## Canonical Repository-Local Walkthrough

This is the operator path when YVEX is not installed globally. Run it from the
repository root and use `./yvex` / `./yvexd`; do not assume `yvex` is on
`PATH`.

### 1. Build and Validate

```sh
cd ~/lab/yvex

make clean
make
make check
make smoke
make check-cuda
```

### 2. Check Local Entrypoints

```sh
./yvex version
./yvex commands
./yvex info
./yvexd --help
```

### 3. Set the Active DeepSeek Artifact

```sh
export DS_GGUF="$HOME/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf"
export DS_ALIAS="deepseek4-v4-flash-selected-embed"

test -f "$DS_GGUF"
sha256sum "$DS_GGUF"

./yvex models add \
  --path "$DS_GGUF" \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --support-level selected-tensor-materialized

./yvex models use "$DS_ALIAS"
./yvex models current
```

Expected SHA256:

```text
5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
```

### 4. Parse and Inspect DeepSeek

```sh
./yvex inspect "$DS_ALIAS"
./yvex metadata "$DS_ALIAS"
./yvex tensors "$DS_ALIAS"
```

Expected posture:

```text
format: gguf
version: 3
tensor_count: 1
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
status: descriptor-only
```

### 5. Materialize DeepSeek Selected Tensor

```sh
./yvex materialize --model "$DS_ALIAS" --backend cpu
./yvex materialize --model "$DS_ALIAS" --backend cuda
```

Expected posture:

```text
status: weights-materialized
execution_ready: false
```

Materialized weights do not imply executable inference.

### 6. Run the Formal Model Gate

```sh
./yvex model-gate check \
  --model "$DS_ALIAS" \
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

Expected posture:

```text
status: model-gate-pass
support_level: selected-tensor-materialized
execution_ready: false
```

### 7. Run the Repeatable Materialization Gate

```sh
./yvex materialize-gate check \
  --model "$DS_ALIAS" \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
  --sha256 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab \
  --scope selected-tensor \
  --expect-tensor token_embd.weight \
  --expect-rank 2 \
  --expect-dims 4096,129280 \
  --expect-dtype F16 \
  --expect-bytes 1059061760 \
  --backend cpu \
  --backend cuda \
  --require-cpu \
  --require-cuda \
  --repeat 3 \
  --check-cleanup
```

Expected posture:

```text
status: materialize-gate-pass
scope: selected-tensor
execution_ready: false
```

### 8. Prompt and Accepted-Only Runtime Diagnostics

The DeepSeek selected artifact is a selected tensor GGUF, not a complete
tokenizer/model runtime. Use the tokenizer fixture for prompt, run, and chat
diagnostics until the spine authorizes real model execution.

```sh
export FIX=tests/fixtures/gguf/valid-tokenizer-simple.gguf

./yvex tokenizer "$FIX"
./yvex tokenize "$FIX" --text "hello world"
./yvex prompt "$FIX" \
  --system "You are a test runtime." \
  --user "hello world"
```

Run accepted-only diagnostics:

```sh
./yvex run \
  --model "$FIX" \
  --backend cpu \
  --prompt "hello world"
```

Expected posture:

```text
run status: accepted-only
execution_ready: false
generation: unsupported
```

Interactive diagnostic shell:

```sh
./yvex chat \
  --model "$FIX" \
  --backend cpu
```

Useful REPL commands:

```text
/status
/help
/exit
```

`chat` accepts and diagnoses prompt tokens. It does not decode or generate
assistant text.

### 9. Server Shell Without Generation

```sh
./yvexd \
  --host 127.0.0.1 \
  --port 18080 \
  --model "$DS_GGUF" \
  --backend cuda \
  --one-request
```

From another terminal while the server is running:

```sh
curl -s http://127.0.0.1:18080/health
curl -s http://127.0.0.1:18080/metrics
curl -s http://127.0.0.1:18080/v1/models
```

Generation endpoints remain intentionally unsupported.

## Current Proof Fixture

```sh
FIX=tests/fixtures/gguf/valid-tokenizer-simple.gguf
```

Useful proof sequence:

```sh
./yvex inspect "$FIX"
./yvex materialize --model "$FIX" --backend cpu
./yvex materialize-gate check --model "$FIX" --label fixture --family llama \
  --scope selected-tensor --expect-tensor token_embd.weight --expect-rank 2 \
  --expect-dims 4,8 --expect-dtype F32 --expect-bytes 128 \
  --backend cpu --require-cpu --repeat 2 --check-cleanup
./yvex model-gate check --model "$FIX" --label fixture --family llama \
  --expect-tensor token_embd.weight --expect-rank 2 --expect-dims 4,8 \
  --expect-dtype F32 --expect-bytes 128 --backend cpu --require-cpu
./yvex metadata "$FIX"
./yvex tensors "$FIX"
./yvex tokenizer "$FIX"
./yvex tokenize "$FIX" --text "hello world"
./yvex graph "$FIX"
./yvex plan "$FIX" --backend cpu
./yvex backend cpu
./yvex engine "$FIX"
./yvex session "$FIX" --backend cpu --text "hello world" --accept-tokens
./yvex run --model "$FIX" --backend cpu --prompt "hello world"
./yvex gguf-emit controlled --out build/tests/gguf-emit/yvex-owned.gguf --overwrite
```

Generated real-model GGUF paths used with `convert emit` and `model-gate` must
follow the canonical artifact naming grammar documented in
`docs/runtime-filesystem.md`.

CUDA proof, only when available:

```sh
./yvex cuda-info
./yvex backend cuda
./yvex plan "$FIX" --backend cuda
./yvex materialize --model "$FIX" --backend cuda
```

Server shell proof:

```sh
./yvexd --host 127.0.0.1 --port 18080 --model "$FIX" --backend cpu --one-request
```

## Non-Goals Visible From CLI

```text
no model support claim
no large/real model materialization
no selected-tensor materialization upgraded to full-model support
no prefill/decode execution
no sampler
no generated assistant text
no CUDA matmul
no CUDA attention
no CUDA KV runtime
no server generation
no OpenAI compatibility claim
```
