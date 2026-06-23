# YVEX CLI Command Index

This is the quick command map for the current YVEX command-line surface.

`docs/cli-runtime.md` owns detailed CLI behavior. This file is intentionally
short: update it when a milestone adds, removes, renames, or materially changes
a command, option, exit status, or visible runtime posture.

## Current Milestone

```text
OWI.8 - open-weight conversion bridge
```

## Binaries

```text
build/bin/yvex
build/bin/yvexd
```

Installed local user links, when desired:

```sh
ln -sf "$PWD/build/bin/yvex" ~/.local/bin/yvex
ln -sf "$PWD/build/bin/yvexd" ~/.local/bin/yvexd
```

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
plan and selected-tensor GGUF emit from safetensors. These do not add full
DeepSeek conversion, imatrix generation, calibration execution, model execution,
or inference.

## `yvex` Commands

| Command | Usage | Status |
| --- | --- | --- |
| `backend` | `yvex backend cpu\|cuda` | implemented; backend status, memory, capabilities |
| `chat` | `yvex chat --model FILE --backend cpu\|cuda` | implemented; accepted-only runtime shell |
| `commands` | `yvex commands` | implemented; command table dump |
| `convert` | `yvex convert plan\|emit` | implemented; conversion plan and selected tensor GGUF emit |
| `cuda-info` | `yvex cuda-info` | implemented; CUDA probe, exit 5 when unavailable |
| `detokenize` | `yvex detokenize PATH --ids IDS` | implemented for fixture tokenizer path |
| `engine` | `yvex engine PATH` | implemented; descriptor/runtime diagnostics |
| `graph` | `yvex graph PATH [--seq N] [--ctx N]` | implemented; inspect-only graph |
| `gguf-emit` | `yvex gguf-emit controlled --out FILE` | implemented; controlled one-tensor GGUF emission |
| `gguf-template` | `yvex gguf-template inspect\|validate --template FILE` | implemented; template contract validation |
| `help` | `yvex help [COMMAND]` | implemented |
| `imatrix` | `yvex imatrix create\|inspect\|validate` | implemented; calibration/imatrix manifest provenance |
| `info` | `yvex info` | implemented; honest support posture |
| `inspect` | `yvex inspect PATH` | implemented; descriptor-only GGUF/model summary |
| `materialize` | `yvex materialize --model FILE --backend cpu\|cuda` | implemented; fixture weights copied into backend tensors |
| `metadata` | `yvex metadata PATH` | implemented; GGUF metadata dump |
| `native-weights` | `yvex native-weights --source DIR [--limit N] [--tensor NAME] [--json]` | implemented; safetensors header inventory |
| `paths` | `yvex paths [--project DIR] [--run] [--create]` | implemented |
| `plan` | `yvex plan PATH [--backend cpu\|cuda] [--seq N] [--ctx N]` | implemented; plan-only |
| `prompt` | `yvex prompt PATH --user TEXT [--system TEXT] [--assistant TEXT] [--tokens]` | implemented |
| `quant-policy` | `yvex quant-policy inspect\|validate --policy FILE` | implemented; declarative qtype policy manifest |
| `qtype-support` | `yvex qtype-support` | implemented; conversion qtype support matrix |
| `run` | `yvex run --model FILE --backend cpu\|cuda --prompt TEXT` | implemented; accepted-only |
| `session` | `yvex session PATH --backend cpu\|cuda [--text TEXT] [--accept-tokens]` | implemented; diagnostics/token acceptance |
| `source-manifest` | `yvex source-manifest create --hf-repo REPO --revision REV --local-path DIR --status STATUS --out FILE` | implemented; source provenance JSON writer |
| `tensor-map` | `yvex tensor-map --arch NAME --native-source DIR [--template FILE] [--tensor NAME] [--limit N] [--json]` | implemented; native tensor to role/template mapping |
| `tokenize` | `yvex tokenize PATH --text TEXT` | implemented for fixture tokenizer path |
| `tokenizer` | `yvex tokenizer PATH` | implemented; tokenizer metadata/support posture |
| `tensors` | `yvex tensors PATH` | implemented; tensor table dump |
| `version` | `yvex version` | implemented |

## `yvexd` Commands

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

## Current Proof Fixture

```sh
FIX=tests/fixtures/gguf/valid-tokenizer-simple.gguf
```

Useful proof sequence:

```sh
yvex inspect "$FIX"
yvex materialize --model "$FIX" --backend cpu
yvex metadata "$FIX"
yvex tensors "$FIX"
yvex tokenizer "$FIX"
yvex tokenize "$FIX" --text "hello world"
yvex graph "$FIX"
yvex plan "$FIX" --backend cpu
yvex backend cpu
yvex engine "$FIX"
yvex session "$FIX" --backend cpu --text "hello world" --accept-tokens
yvex run --model "$FIX" --backend cpu --prompt "hello world"
yvex gguf-emit controlled --out build/tests/gguf-emit/yvex-owned.gguf --overwrite
```

CUDA proof, only when available:

```sh
yvex cuda-info
yvex backend cuda
yvex plan "$FIX" --backend cuda
yvex materialize --model "$FIX" --backend cuda
```

Server shell proof:

```sh
yvexd --host 127.0.0.1 --port 18080 --model "$FIX" --backend cpu --one-request
```

## Non-Goals Visible From CLI

```text
no model support claim
no large/real model materialization
no prefill/decode execution
no sampler
no generated assistant text
no CUDA matmul
no CUDA attention
no CUDA KV runtime
no server generation
no OpenAI compatibility claim
```
