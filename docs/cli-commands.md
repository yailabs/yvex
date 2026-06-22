# YVEX CLI Command Index

This is the quick command map for the current YVEX command-line surface.

`docs/cli-runtime.md` owns detailed CLI behavior. This file is intentionally
short: update it when a milestone adds, removes, renames, or materially changes
a command, option, exit status, or visible runtime posture.

## Current Milestone

```text
M0 - fixture weight materialization
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
```

CUDA in L0 means device probe, memory/tensor movement, and F32 embed parity. It
does not mean CUDA matmul, attention, session execution, generation, or model
support.

M0 adds fixture weight materialization into CPU/CUDA backend tensors. It does
not add model execution or inference.

## `yvex` Commands

| Command | Usage | Status |
| --- | --- | --- |
| `backend` | `yvex backend cpu\|cuda` | implemented; backend status, memory, capabilities |
| `chat` | `yvex chat --model FILE --backend cpu\|cuda` | implemented; accepted-only runtime shell |
| `commands` | `yvex commands` | implemented; command table dump |
| `cuda-info` | `yvex cuda-info` | implemented; CUDA probe, exit 5 when unavailable |
| `detokenize` | `yvex detokenize PATH --ids IDS` | implemented for fixture tokenizer path |
| `engine` | `yvex engine PATH` | implemented; descriptor/runtime diagnostics |
| `graph` | `yvex graph PATH [--seq N] [--ctx N]` | implemented; inspect-only graph |
| `help` | `yvex help [COMMAND]` | implemented |
| `info` | `yvex info` | implemented; honest support posture |
| `inspect` | `yvex inspect PATH` | implemented; descriptor-only GGUF/model summary |
| `materialize` | `yvex materialize --model FILE --backend cpu\|cuda` | implemented; fixture weights copied into backend tensors |
| `metadata` | `yvex metadata PATH` | implemented; GGUF metadata dump |
| `paths` | `yvex paths [--project DIR] [--run] [--create]` | implemented |
| `plan` | `yvex plan PATH [--backend cpu\|cuda] [--seq N] [--ctx N]` | implemented; plan-only |
| `prompt` | `yvex prompt PATH --user TEXT [--system TEXT] [--assistant TEXT] [--tokens]` | implemented |
| `run` | `yvex run --model FILE --backend cpu\|cuda --prompt TEXT` | implemented; accepted-only |
| `session` | `yvex session PATH --backend cpu\|cuda [--text TEXT] [--accept-tokens]` | implemented; diagnostics/token acceptance |
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
