# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
Its current work is the path from artifact bytes to runtime execution, with each
intermediate state made visible rather than folded into a vague support claim.

A GGUF file can parse while the model is still not executable. A selected tensor
can materialize while the full weight set is still absent. A backend can own
memory while no scheduled graph has run. YVEX treats those as different states:
parsed artifact, materialized tensor, backend residency, and executable graph.
The repository, CLI, tests, and artifact cards all preserve that separation.

Today YVEX proves artifact loading, GGUF parsing, tensor-table construction,
descriptor construction, selected-tensor GGUF emission, selected-tensor
materialization, CPU/CUDA residency diagnostics, local model registry
resolution, runtime/session diagnostics, and a provider status shell. Full
model inference remains beyond the current frontier.

## What The Binary Proves Today

The proof surface starts with the repository-local binary and its tests:

```sh
make
./yvex version
./yvex commands
./yvex info
```

`./yvex` exposes inspection commands such as `inspect`, `metadata`, and
`tensors`; registry commands under `models`; backend checks through `backend`
and `cuda-info`; and graph/planning diagnostics through `graph` and `plan`.
Tokenizer and prompt work is visible through `tokenizer`, `tokenize`,
`detokenize`, and `prompt`. Runtime diagnostics are deliberately bounded:
`engine`, `session`, `run`, and `chat` can open implemented runtime state and
report accepted tokens or diagnostic artifacts, but they do not fabricate model
output.

The open-weight toolchain is also command-visible. `source-manifest`,
`native-weights`, `tensor-map`, `gguf-template`, `gguf-emit`, and `convert`
cover provenance, native inventory, tensor mapping, template validation,
controlled GGUF emission, and selected conversion. `qtype-support`,
`quant-policy`, `quant-job`, and `imatrix` keep storage, conversion,
quantization policy, job provenance, and calibration metadata explicit. The
gate commands, `model-gate` and `materialize-gate`, turn those facts into
repeatable pass/fail reports instead of relying on README claims.

## DeepSeek Selected-Tensor Target

The active live target is a selected DeepSeek V4 Flash embedding GGUF tracked
as an external operator artifact in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md):

```text
alias: deepseek4-v4-flash-selected-embed
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU: pass
CUDA: pass
execution_ready: false
```

A single selected F16 embedding tensor is already a real large-model pressure
point. This target proves external artifact identity, GGUF parsing,
tensor-table correctness, selected tensor shape/dtype validation, selected
materialization, CPU/CUDA backend residency, and gate reporting. It does not
prove full model materialization or graph execution.

Useful proof commands:

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

## Artifact And Registry Workflow

Model artifacts stay outside git. The repository may contain tiny GGUF parser
fixtures, but generated model artifacts, safetensors, raw binary weights,
calibration data, local registries, reports, logs, and build output are local
operator state.

The local registry makes machine-local paths usable without turning an alias
into a support claim:

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex models use local-model
./yvex inspect local-model
```

Aliases resolve to artifact paths for one-shot commands where model references
are supported. They do not upgrade descriptor-only or selected-tensor evidence
into executable runtime support.

## GGUF And Open-Weight Tooling

YVEX treats GGUF as an inspectable artifact format. Parsing a GGUF gives
metadata, tensor names, shapes, dtypes, offsets, and descriptor input. It does
not automatically create executable model state.

The intake path starts with provenance and native inventory:

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

Selected conversion remains explicit. Planning and emission name the family,
source, tensor, qtype, and output artifact instead of implying full-model
conversion:

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

The quantization surface is intentionally split. `qtype-support` reports
storage/conversion/compute distinctions; `quant-policy` validates declarative
policy; `quant-job` records external job provenance; and `imatrix` records
calibration metadata.

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

## Backends And Machine Pressure

The CPU backend is the reference and diagnostic path. It gives YVEX a stable
baseline for materialization, tensor movement, capability reporting, and
comparison against accelerated behavior.

CUDA is the accelerated direction. CUDA code lives under `cuda/`; host bridge
code stays in C files, and device code lives in `cuda/cuda_kernels.cu`. Current
CUDA proof covers device probing, memory stats, allocation, transfer, device
copy, and a narrow F32 embedding-kernel parity path where implemented. It does
not cover matmul, attention, RMSNorm, RoPE, MoE routing, KV execution, or full
graph execution.

DeepSeek-class artifacts force the runtime to deal with weight residency,
host/device transfer, qtype storage versus compute support, graph scratch, KV
size, and future prefill/decode pressure. The selected DeepSeek embedding
target is intentionally small in model scope but large in memory reality:
placing even that tensor correctly matters before the runtime can claim a
larger execution path.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Where Execution Begins

The current runtime commands are diagnostic surfaces:

```sh
./yvex engine model.gguf
./yvex session model.gguf --backend cpu --ctx 128
./yvex run --model model.gguf --backend cpu --prompt "hello"
./yvex chat --model model.gguf --backend cpu
```

They can open descriptors, sessions, backend state, token acceptance, metrics,
traces, and profile artifacts where those pieces exist. `run` is an
accepted-only path. `chat` is the diagnostic console and future canonical REPL.
Plain text input must not produce generated assistant text until inference
exists.

`execution_ready` remains `false` until scheduled graph ops, real weights,
scratch, KV, prefill, logits, decode, and sampling form an executable path.

## Provider Shell

`./yvexd` is currently a provider/status daemon:

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

Implemented status endpoints:

```text
GET /health
GET /metrics
GET /v1/models
```

It has no generation-server behavior, and daemon-side model alias resolution
remains future work.

## Validation

Baseline validation:

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
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

Artifact guardrails:

```sh
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
