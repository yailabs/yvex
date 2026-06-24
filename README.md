# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
It sits below chat interfaces and provider APIs, at the layer where an external
model file becomes inspected tensor metadata, YVEX descriptors, backend
allocations, and repeatable runtime evidence.

The codebase is already past casual file inspection, but it is not at full
model inference. It opens real artifacts, reads GGUF structure, preserves tensor
names and shapes in runtime descriptors, emits and materializes selected
tensors, moves those tensors through CPU and CUDA residency paths, and records
the result through gates. That is the lower half of a local runtime: enough to
make artifact and backend state concrete, not enough to generate text.

YVEX is strict about the distance between loading bytes and running a model.
GGUF metadata and tensor directories are file facts. Descriptors are runtime
views over those facts. Selected materialization is backend-owned storage for a
chosen tensor. Execution begins later, when scheduled ops, real weights,
scratch, KV, logits, decode, and sampling line up under one runtime path.

## Execution boundary

Parsing gives YVEX the artifact's declared structure: metadata, tensor names,
shapes, dtypes, byte offsets, and tokenizer facts where present. Descriptor
construction turns that structure into the runtime's model view. Selected
materialization moves chosen tensor bytes into CPU or CUDA-owned storage and
forces allocation, transfer, cleanup, and error reporting to become explicit.

Backend residency is still not graph execution. A tensor resident on CUDA has
crossed memory and ownership boundaries; it has not crossed matmul, attention,
MoE routing, KV ownership, logits, decode, or sampling. The CLI follows the
same staging: `inspect`, `metadata`, and `tensors` read artifact structure;
`models` resolves local aliases; `backend` and `cuda-info` expose the machine;
`graph` and `plan` build planning artifacts; `engine`, `session`, `run`, and
`chat` remain diagnostic boundaries for runtime state, token acceptance,
traces, metrics, and profile artifacts.

## Live artifact: DeepSeek selected embedding

The live target is a selected DeepSeek V4 Flash embedding GGUF documented as an
external artifact in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md):

```text
alias: deepseek4-v4-flash-selected-embed
path: /home/dgmothx/lab/models/gguf/deepseek/deepseek4-v4-flash-selected-embed-F16-noimatrix-yvex-v1.gguf
sha256: 5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab
format: GGUF v3
tensor: token_embd.weight
dims: [4096,129280]
dtype: F16
tensor_bytes: 1059061760
CPU: pass
CUDA: pass
execution_ready: false
```

This selected tensor is narrow in model scope and serious in machine pressure:
`token_embd.weight` at `[4096,129280]` in `F16` is about one billion tensor
bytes. It exercises paths that tiny fixtures cannot: long machine-local artifact
names, SHA identity, GGUF v3 layout, tensor-directory accounting, shape and
dtype preservation, CPU/CUDA allocation, backend cleanup, and exact gate
reports. The honest state is useful precisely because it is partial: selected
embedding resident, transformer execution absent, `execution_ready: false`.

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

## Artifact workflow

Real model files are machine-local assets, not repository content. The repo may
track tiny GGUF fixtures for parser coverage; generated GGUFs, safetensors, raw
binary weights, calibration `.dat` files, reports, logs, build output, and
local registries stay outside git.

The local registry removes path friction without changing runtime state. An
alias in `.yvex/models.local.json`, or in the file named by
`YVEX_MODELS_REGISTRY`, is just a local name for a local file; the CLI still
parses, validates, and materializes the artifact when a command asks for those
stages.

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex inspect local-model
./yvex materialize --model local-model --backend cuda
```

## GGUF intake and tensor mapping

GGUF is the artifact envelope, not the execution engine. YVEX reads the
envelope first: metadata, tensor directory, names, shapes, dtypes, offsets, and
tokenizer metadata. Those facts feed descriptors, mapping, selected emission,
and materialization. Execution starts later, when descriptor facts connect to
scheduled ops and backend kernels.

The open-weight tooling exists because a native runtime needs provenance and
layout before generated text. `source-manifest` records where official weights
came from. `native-weights` inventories safetensors headers. `tensor-map`
connects family-native names to YVEX roles and GGUF names. `convert plan` and
`convert emit` keep selected conversion explicit: source, family, tensor,
qtype, and output artifact.

```sh
./yvex source-manifest create \
  --hf-repo OWNER/MODEL \
  --revision REVISION \
  --local-path /path/to/native/source \
  --status in-progress \
  --out /path/to/source-manifest.json

./yvex native-weights --source /path/to/native/source
./yvex tensor-map --arch deepseek4 --native-source /path/to/native/source
./yvex convert plan --arch deepseek4 --native-source /path/to/native/source --out-plan /tmp/yvex-plan.json
./yvex convert emit --arch deepseek4 --native-source /path/to/native/source --tensor model.embed_tokens.weight --target-qtype F16 --out /tmp/yvex-selection.gguf --overwrite
```

That chain is lineage: source directory, native inventory, family mapping,
selected artifact, materialization. Full-model conversion and full-model
execution require more than a valid selected tensor.

## Quantization policy and qtype boundaries

Quantization is not one support bit in YVEX. A qtype may appear in a GGUF tensor
as storage, pass a policy check, be the target of a selected conversion path, be
recorded in an external quantization job, or have backend compute coverage.
Those are different edges. Treating them as one word is how local runtimes end
up accepting artifacts that the machine cannot execute.

`qtype-support` exposes storage, emission, quantization, and compute as
separate facts. `quant-policy` validates declarative policy. `quant-job`
records external conversion provenance. `imatrix` carries calibration metadata.
Together they make qtype and calibration evidence explicit before kernels and
graph execution rely on it.

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

## Backends and machine pressure

The CPU backend is the reference lane. It gives the project a stable place to
validate parser output, selected materialization, cleanup, and error reporting
before accelerated behavior enters the picture.

CUDA is the accelerated lane, and its current boundary is deliberately narrow.
CUDA code lives under `cuda/`; host bridge code stays in C files, and device
code lives in `cuda/cuda_kernels.cu`. The implemented CUDA work covers device
discovery, memory accounting, allocation, transfer, device copy, and the
limited kernel parity path already present in the tree. That is real backend
work, but it is not yet a CUDA transformer backend.

The missing work is concrete. A DeepSeek-class graph needs embedding,
normalization, RoPE, attention, routed MoE experts, quantized matmul, logits,
KV placement, scratch planning, prefill, decode, and sampling under one
scheduler. The selected embedding target makes the first part of that chain
hard and measurable before the project tries to make the whole model speak.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Provider boundary

`./yvexd` keeps the local provider boundary present while executable graph
runtime is still absent. The daemon surface is status-level: health, metrics,
and model listing. Generation endpoints remain outside the daemon until real
graph execution exists behind them.

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

The status endpoints are `GET /health`, `GET /metrics`, and `GET /v1/models`.

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

Artifact guardrails:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is no committed real model weights, no generated model
GGUFs, no local registries, no reports/logs/build outputs, and no benchmark
artifacts. Tracked GGUF files are tiny parser fixtures under `tests/`.

## Source layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             provider daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA host bridge and kernel unit
gguf/               GGUF parser, conversion, family mapping, quant policy
include/yvex/       public C API
tests/              compact runners, fixtures, and vectors
docs/               API, contract, internal spine
```

## Documentation

`AGENTS.md` defines operating rules for humans and coding agents.
`MODEL_ARTIFACTS.md` is the external artifact card. `docs/api.md` describes the
public C API, `docs/contract.md` defines runtime/backend/filesystem/CLI
contracts, and the internal roadmap stays in `docs/spine.md`.

## License

YVEX is licensed under the MIT license.
