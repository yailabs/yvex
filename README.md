# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.

The repository is organized around a strict execution boundary: a model file can
be parsed without being runnable, a tensor can be materialized without being
part of an executable graph, and a backend can own memory without proving
prefill, decode, or generation. YVEX keeps those states separate in source,
tests, commands, artifact cards, and failure reports.

The current code proves artifact loading, GGUF parsing, tensor inspection,
descriptor construction, selected-tensor GGUF emission, selected-tensor
materialization, CPU/CUDA backend residency diagnostics, local model registry
resolution, runtime/session diagnostics, and a provider status shell. It does
not yet execute a full model.

## What YVEX Owns

YVEX owns the local path from model artifact bytes to runtime diagnostics:

```text
external model artifact
GGUF parser
metadata and tensor directory
YVEX tensor table
model descriptor
backend tensor allocation
selected weight materialization
graph and memory-plan substrate
engine/session diagnostics
operator-visible gate reports
```

Every layer must report what it has actually proven. Parser success does not
prove model support. Materialized weights do not prove inference. CUDA
residency does not prove graph execution. `./yvexd` is a provider/status shell
until generation exists.

Current unsupported runtime areas:

```text
full-model execution
prefill
decode
sampling
generation
KV execution
paged KV
host spill
distributed inference
generation-compatible provider APIs
token latency or throughput benchmarks
```

`execution_ready` remains `false` until YVEX has a real executable graph path.

## Current Proof Surface

The proof surface is command-visible. The CLI can inspect artifacts, resolve
local aliases, probe backends, materialize selected weights, and run diagnostic
runtime shells without claiming generation.

```sh
make
./yvex version
./yvex commands
./yvex info
```

The command table is the most direct description of the implemented surface:

```text
inspection:
  inspect
  metadata
  tensors

local model registry:
  models

backend and machine boundary:
  backend
  cuda-info

graph and planning:
  graph
  plan

tokenizer and prompt diagnostics:
  tokenizer
  tokenize
  detokenize
  prompt

runtime diagnostics:
  engine
  session
  run
  chat

open-weight and GGUF tooling:
  source-manifest
  native-weights
  tensor-map
  gguf-template
  gguf-emit
  convert

quantization policy and provenance:
  qtype-support
  quant-policy
  quant-job
  imatrix

materialization gates:
  model-gate
  materialize-gate

server/provider:
  yvexd
```

Use `./yvex help <command>` for command-specific syntax. Planned runtime areas
are not hidden behind command names; unsupported paths report unsupported.

## Artifact Path

Model artifacts live outside the repository. YVEX keeps generated model files,
local registries, logs, and reports out of git.

```text
external model roots: operator-owned
local registry:       .yvex/models.local.json
build output:         build/
tracked fixtures:     tiny GGUF parser fixtures only
```

The local registry maps short aliases to machine-local artifact paths:

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex models use local-model
```

One-shot commands resolve either a filesystem path or a registered alias:

```sh
./yvex inspect local-model
./yvex metadata local-model
./yvex tensors local-model
./yvex materialize --model local-model --backend cuda
```

Alias resolution is an operator convenience. It does not change the support
level of the referenced artifact.

## DeepSeek Selected-Tensor Target

The active live artifact is a selected DeepSeek V4 Flash embedding GGUF
documented in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md):

```text
alias: deepseek4-v4-flash-selected-embed
path: /home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
format: GGUF v3
tensor_count: 1
tensor: token_embd.weight
rank: 2
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU: pass
CUDA: pass
execution_ready: false
```

This artifact proves that YVEX can identify an external GGUF, parse its tensor
table, verify the selected tensor shape and dtype, copy the selected tensor into
CPU/CUDA backend residency, and report the result through gates.

It does not prove full model materialization, graph execution, prefill, decode,
sampling, or generation.

Useful proof commands:

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda

./yvex model-gate check \
  --model deepseek4-v4-flash-selected-embed \
  --label deepseek-v4-flash-selected-embedding \
  --family deepseek4 \
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

## Backend And Machine Boundary

Local runtime work is bounded by machine facts, not just file format support.
YVEX keeps these concerns explicit:

```text
model weight residency
tensor mapping from native names to runtime roles
qtype storage versus compute support
CUDA allocation, movement, and kernel availability
CPU reference behavior
graph scratch requirements
KV cache size and placement
future prefill/decode memory pressure
```

The CPU backend is the reference path. It is used for deterministic
materialization, tensor movement, capability reporting, and comparison against
accelerated paths.

The CUDA path is implemented under `cuda/`. Host bridge code stays in `.c`
files; device code lives in `cuda/cuda_kernels.cu`. Current CUDA proof covers
device probing, memory stats, tensor allocation, host/device transfer, device
copy, and an F32 embedding kernel parity path. It does not cover matmul,
attention, RMSNorm, RoPE, MoE, KV execution, or full model execution.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

DeepSeek-class artifacts create immediate pressure on memory residency and KV
planning. A single selected F16 embedding tensor already carries more than one
billion bytes. Full execution will require weight placement, scratch planning,
KV sizing, backend compute kernels, and explicit failure classes before any
generation claim is valid.

## GGUF, Mapping, And Quantization Policy

YVEX treats GGUF as an artifact format with inspectable structure. Parsing a
GGUF creates metadata and tensor-directory facts; it does not create executable
model state.

```sh
./yvex inspect model.gguf
./yvex metadata model.gguf
./yvex tensors model.gguf
```

The GGUF tooling lives under `gguf/` and includes parser, artifact naming,
controlled emission, template validation, selected conversion, family tensor
mapping, qtype support, quantization policy manifests, quantization job
manifests, and imatrix provenance manifests.

Open-weight intake starts from source inventory:

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/native/source \
  --status in-progress \
  --out /path/to/source-manifest.json

./yvex native-weights --source /path/to/native/source
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source
```

Selected conversion paths are explicit:

```sh
./yvex convert plan \
  --arch deepseek4 \
  --native-source /path/to/native/source \
  --out-plan /tmp/yvex-plan.json

./yvex convert emit \
  --arch deepseek4 \
  --native-source /path/to/native/source \
  --tensor model.embed_tokens.weight \
  --target-qtype F16 \
  --out /tmp/yvex-selection.gguf \
  --overwrite
```

Conversion support, storage support, quantization support, and compute support
are separate:

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

YVEX does not collapse those states into a single support label.

## Runtime Frontier

Runtime diagnostics exist now:

```sh
./yvex engine model.gguf
./yvex session model.gguf --backend cpu --ctx 128
./yvex run --model model.gguf --backend cpu --prompt "hello"
./yvex chat --model model.gguf --backend cpu
```

These commands open descriptors, backend state, sessions, prompt/token
acceptance, metrics, traces, and profile artifacts where implemented. `run` is
accepted-only diagnostics. `chat` is a diagnostic console and future canonical
REPL. Plain text input must not produce fabricated assistant text.

The frontier begins where planned graph execution would need real model weights,
scheduled ops, scratch, KV, prefill, logits, decode, and sampling. Until that
path exists, `execution_ready: false` is the correct state.

## Server Provider Shell

`./yvexd` is the local provider/status daemon:

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

Implemented status endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

The daemon can expose status and provider-shell structure. It has no
generation-server behavior; daemon-side model alias resolution remains future
work.

## Validation

Baseline validation:

```sh
make check
make smoke
git diff --check
```

CUDA-capable hosts:

```sh
make check-cuda
```

Direct runner surface:

```sh
build/tests/test
build/tests/test_cuda
YVEX_BIN=./yvex YVEXD_BIN=./yvexd sh tests/cli.sh
YVEX_BIN=./yvex YVEXD_BIN=./yvexd sh tests/cli.sh --cuda
```

Repository guardrails:

```sh
sh tests/test_source_layout.sh
sh tests/test_code_natural.sh
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

Expected artifact state: no tracked safetensors, raw binary weights, `.dat`
calibration artifacts, generated model GGUFs, local registries, logs, or build
outputs. The tracked GGUF files are tiny parser fixtures under `tests/`.

## Source Layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             provider daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA host bridge and kernel unit
gguf/               GGUF parser, conversion, family mapping, quant policy
include/yvex/       public C API
tests/              compact runners, fixtures, and vectors
docs/               api, contract, internal spine
```

Generated local state:

```text
build/
.yvex/
./yvex
./yvexd
```

## Documentation

```text
AGENTS.md            operating rules for humans and coding agents
MODEL_ARTIFACTS.md   external artifact cards
docs/api.md          public C API surface
docs/contract.md     runtime/backend/filesystem/CLI contract
docs/spine.md        internal delivery roadmap
```

Public files describe capability and limits. The internal delivery map stays in
`docs/spine.md`.

## License

YVEX is licensed under the MIT license.
