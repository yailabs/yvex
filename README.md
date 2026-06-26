# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
It works below chat interfaces and provider APIs, at the point where a model
file stops being a blob on disk and starts becoming runtime-owned structure:
parsed metadata, tensor roles, backend allocations, engine/session state, and
scheduled graph work.

The current code does not run a full model. It does now cross the older
parse/materialize boundary: YVEX can inspect GGUF artifacts, materialize
selected tensors on CPU and CUDA, attach those tensors to engine-owned runtime
state, and execute a deterministic fixture graph over controlled F32 weights.
The next runtime step is the first real-model partial graph segment. Prefill,
decode, logits, sampling, generation, and provider generation are not
implemented.

## What YVEX is

YVEX is built for the lower native runtime path of local models: artifact
identity, GGUF structure, tensor names, shape and dtype preservation, selected
materialization, backend residency, engine ownership, and graph execution
boundaries. The project is intentionally strict about separating those states.
An artifact that parses is not resident on a backend. A tensor resident on CUDA
is not a transformer graph. A fixture graph that executes is not real-model
inference.

This is not a generic model-serving product today. It is the runtime layer that
must become true before a model-serving surface can be honest: files must open,
tensor rows must map to roles, selected bytes must land in owned backend
storage, engine/session lifetimes must release resources correctly, and graph
execution must produce checkable outputs before text generation is exposed.

## Why this exists

Local inference often hides too much behind words like "loaded", "supported",
"compatible", and "running". YVEX exists to split those words into runtime
states that can be tested separately.

A tensor name can parse and still map to the wrong role. A qtype can be
recognized as artifact storage while no kernel can compute with it. A CUDA
allocation can prove memory ownership without proving scheduled work. A server
can expose provider-shaped endpoints before a runtime exists behind them. A
graph plan can exist before scratch, KV, logits, decode, and sampling are
connected.

That separation is not pedantry. It is what makes a local runtime debuggable.
When a large local artifact fails, the operator needs to know whether the
failure is filesystem identity, GGUF layout, tensor mapping, qtype policy,
backend allocation, engine lifecycle, graph scheduling, KV state, logits, or
sampling. YVEX exposes those boundaries instead of flattening them into one
word.

## Current state

| Area | Current state | What it proves | What it does not prove |
| --- | --- | --- | --- |
| GGUF parse | implemented | metadata and tensor directory can be read | model execution |
| Descriptor/tensor roles | implemented | artifact facts become runtime descriptors | backend residency |
| Selected materialization | implemented | selected tensor bytes move into CPU/CUDA storage | full model loading |
| Engine weight attachment | implemented | selected materialized weights are engine-owned state | graph execution |
| Fixture graph execution | implemented for controlled F32 fixtures | one deterministic graph path runs through backend dispatch | real-model inference |
| Real-model partial graph | next | first real tensor graph segment | not implemented yet |
| Prefill/decode/logits | planned | future runtime stages | not implemented |
| Generation/server generation | unsupported | no text generation path exists | provider endpoints are status-only |

The important new line is fixture graph execution. YVEX now has a real,
deterministic graph path for a controlled fixture: an embed node reads attached
weights, dispatches through the selected backend, writes output, and produces a
stable checksum and values. That is a runtime step, but it is not a model
execution claim.

## What runs today

The controlled fixture is intentionally tiny. It uses controlled F32 embedding
weights, a fixture token, one embed-node execution path, backend dispatch,
output allocation/readback, stable output values, and CPU/CUDA parity when CUDA
is available.

```sh
tmpdir="$(mktemp -d)"
./yvex gguf-emit controlled --out "$tmpdir/controlled.gguf" --arch deepseek --overwrite
./yvex graph --model "$tmpdir/controlled.gguf" --backend cpu --execute-fixture --fixture-token 0
./yvex graph --model "$tmpdir/controlled.gguf" --backend cuda --execute-fixture --fixture-token 0
```

Expected fields include:

```text
fixture_graph_executed: true
fixture_backend: cpu
fixture_op: embed
fixture_weight: token_embd.weight
fixture_output_checksum: 10881421815077182739
execution_ready: false
graph_execution_ready: false
status: fixture-graph-executed
```

For the controlled fixture currently emitted by `gguf-emit controlled`, these
output values are stable:

| Fixture token | Expected output values | Meaning |
| --- | --- | --- |
| `0` | `0,4,8,12` | first controlled embedding row |
| `1` | `16,20,24,28` | second controlled embedding row |

This is real graph execution for a controlled fixture. It is not real-model
inference.

## Live artifacts and fixtures

YVEX currently uses two different kinds of artifacts. The large selected
DeepSeek embedding is the live pressure artifact. The controlled F32 GGUF is
the deterministic execution fixture.

### Live pressure artifact

```text
alias: deepseek4-v4-flash-selected-embed
local_path: operator-local, outside repository
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

The selected DeepSeek embedding is not "supported DeepSeek." It is the current
pressure artifact: one very large real tensor that forces artifact identity,
shape, dtype, byte accounting, CPU/CUDA residency, engine attachment, and
cleanup to be real before a full graph exists.

DeepSeek is the current target because it creates useful pressure now. YVEX
should remain able to move to another open-weight model family when hardware,
model quality, or research direction changes. A new family requires explicit
mapping, artifact identity, runtime work, and tests.

### Controlled fixture artifact

The controlled F32 fixture is the execution artifact for the current graph
path. It is small by design. It exists so the executor can be checked exactly
before real-model partial execution begins.

| Artifact | Role | Current state |
| --- | --- | --- |
| DeepSeek selected F16 embedding | large pressure tensor | inspect/materialize/attach pass; fixture graph unsupported because it is selected F16 pressure data |
| Controlled DeepSeek-arch F32 fixture | deterministic graph execution fixture | CPU/CUDA fixture graph pass |

## Operator path

Normal usage should start with short commands. Long gate invocations and exact
selected-artifact checks live in [docs/operator-runbook.md](docs/operator-runbook.md).

```sh
make
./yvex commands
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models use local-model
./yvex inspect local-model
./yvex tensors local-model
```

Controlled execution path:

```sh
tmpdir="$(mktemp -d)"
./yvex gguf-emit controlled --out "$tmpdir/controlled.gguf" --arch deepseek --overwrite
./yvex graph --model "$tmpdir/controlled.gguf" --backend cpu --execute-fixture --fixture-token 0
```

Daemon status path:

```sh
./yvexd --model local-model --backend cpu --host 127.0.0.1 --port 8080
```

For model gates, materialization gates, DeepSeek selected-artifact checks,
open-weight intake commands, and debug paths, use the operator runbook. The
README shows the normal path first because routine operation should not require
memorizing every low-level flag.

## Backend and hardware posture

| Backend / platform | Current role | Current boundary |
| --- | --- | --- |
| CPU | correctness/reference path for parse, materialization, engine attachment, and fixture execution | not production inference |
| CUDA / Linux | primary acceleration target; device probe, allocation, transfer, selected materialization, and F32 fixture parity | not a full transformer backend |
| DGX Spark / GB10 | future CUDA hardware profile | no profile claim until measured |
| Metal / macOS | not implemented | no Metal support claim |
| ROCm / Strix Halo | not implemented | no ROCm support claim |

A DeepSeek-class runtime eventually has to place many weights, schedule
attention, routed experts, normalization, RoPE, quantized matmul, scratch, KV,
decode, and logits. The current fixture graph is small because the executor
mechanics need to be correct before large kernels are wired in.

CUDA support currently means device probing, memory accounting, tensor
allocation, transfer, selected materialization, and the controlled F32 fixture
parity path where available. It does not mean a CUDA transformer backend.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Quantization and artifact boundaries

Quantization support is not one bit. YVEX keeps storage, policy, emission,
external provenance, calibration evidence, and backend compute separate because
local model failures often happen when those are treated as the same thing.

| Layer | Current state | Meaning |
| --- | --- | --- |
| Storage recognition | implemented where qtypes are known | artifact metadata can identify qtype |
| Policy | implemented | qtypes can be allowed or rejected by manifest policy |
| Selected emission | implemented for selected paths | selected GGUF output can be written under explicit constraints |
| Quant-job provenance | implemented as manifest inspection | external quantization work can be recorded |
| Imatrix provenance | implemented as manifest inspection | calibration evidence can be tracked |
| Backend compute | narrow / fixture-specific | not full quantized matmul |
| Full quantized inference | unsupported | no model execution claim |

The current commands make those boundaries visible:

```sh
./yvex qtype-support
./yvex quant-policy validate --policy /path/to/policy.json
./yvex quant-job inspect --manifest /path/to/quant-job.json
./yvex imatrix inspect --manifest /path/to/imatrix.json
```

## Evaluation and benchmarking posture

YVEX does not publish token/sec or model-quality numbers yet. That is
deliberate: without prefill, decode, logits, and generation, any throughput
table would be fiction. What the project can publish today is the measurement
plan and the first correctness boundary: deterministic fixture graph outputs.

| Measurement | Current YVEX status | First valid point |
| --- | --- | --- |
| Fixture graph correctness | available after controlled fixture execution | current fixture path |
| Real partial graph regression | next | after first real-model graph segment |
| Prefill throughput | not available | after prompt-backed prefill |
| Decode throughput | not available | after decode step |
| Logits regression | not available | after logits boundary |
| Generation quality smoke | not available | after constrained generation |
| End-to-end runtime benchmark | not available | after generation path stabilizes |

Future benchmark shape:

| Future benchmark row | Prompt size | Metric | Status |
| --- | --- | --- | --- |
| short prompt | `<512` tokens | prefill + decode tokens/sec | blocked until prefill/decode |
| medium prompt | `~10k` tokens | prefill interval tokens/sec + decode tokens/sec | planned |
| long prompt | `32k+` tokens | prefill interval tokens/sec + KV memory | planned |
| generation probe | fixed generated-token count | decode/generation tokens/sec | blocked until generation |

Capability and regression posture:

| Eval group | Purpose | Status |
| --- | --- | --- |
| Fixture vectors | catch executor/backend regressions | available for the controlled fixture path |
| Partial graph vectors | catch real-model graph segment drift | planned |
| Prompt/token tests | catch tokenizer/template drift | diagnostics only |
| Logits vectors | catch output distribution regressions | planned after logits |
| Capability eval | inspect generation quality on curated tasks | future only |

No benchmark result should be read into this section. It is a measurement plan,
not a performance claim.

## Runtime roadmap

The next step is real-model partial graph execution. That still is not
inference: it means one constrained segment of a real model graph participates
in scheduled runtime computation with memory planning and backend dispatch.

| Runtime stage | Public meaning | Status |
| --- | --- | --- |
| Controlled fixture graph | deterministic executor proof | implemented |
| Real partial graph | first real tensor graph segment | next |
| Prompt input | tokenizer/prompt into runtime tensors | planned |
| Prefill | prompt tokens through graph state | planned |
| Minimal KV | session-owned append/read state | planned |
| Decode | one step over existing state | planned |
| Logits | output distribution buffer | planned |
| Sampling | choose next token from logits | planned |
| Constrained generation | bounded token loop | planned |
| CLI generation | interactive runtime path | future |
| Provider generation | server API backed by runtime | future |

Evaluation and benchmarking follow the same order. Fixture correctness can be
tested now; logits regression cannot exist before logits; token/sec cannot be
published before prefill/decode exists.

## Provider boundary

`./yvexd` is a provider/status shell. It accepts a model path or registered
alias for status reporting, but it does not serve generation.

| Endpoint / behavior | Current state |
| --- | --- |
| `GET /health` | implemented provider/status shell |
| `GET /metrics` | implemented metrics/status surface |
| `GET /v1/models` | implemented model listing/status |
| `--model FILE_OR_ALIAS` | explicit path or registered alias |
| generation endpoints | unsupported |
| OpenAI-compatible generation | unsupported |
| Anthropic-compatible generation | unsupported |

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

The daemon exists so the provider boundary can be wired early without faking
model output.

## Validation

| Gate | Purpose |
| --- | --- |
| `git diff --check` | patch hygiene |
| `make check` | baseline C tests |
| `make smoke` | CLI and daemon smoke |
| `tests/test_docs_surface.sh` | public docs boundary |
| `tests/test_surface.sh` | repository surface guardrail |
| `make check-cuda` | CUDA-capable host validation |

```sh
git diff --check
make check
make smoke
sh tests/test_docs_surface.sh
sh tests/test_surface.sh
make check-cuda
```

`make check-cuda` requires a CUDA-capable host. Artifact guardrails keep model
weights and generated artifacts out of git:

```sh
git ls-files '*.safetensors' '*.bin' '*.dat'
git ls-files '*.gguf'
```

The expected state is no committed real model weights, no generated model
GGUFs, no local registries, no reports/logs/build outputs, and no benchmark
artifacts. Tracked GGUF files are tiny parser fixtures under `tests/`.

## Repository layout

```text
yvex_cli.c          CLI entrypoint
yvexd.c             provider daemon entrypoint
yvex_*.c            compact implementation modules
cuda/               CUDA host bridge and kernel unit
gguf/               GGUF parser, conversion, family mapping, quant policy
include/yvex/       public C API
tests/              compact runners, fixtures, and vectors
docs/               API, contract, operator runbook, internal spine
```

## Documentation

| File | Role |
| --- | --- |
| `docs/operator-runbook.md` | command-first operator workflow |
| `docs/api.md` | public C API, ownership, and error/capability map |
| `docs/contract.md` | behavior contract for CLI, filesystem, backend, server, registry |
| `MODEL_ARTIFACTS.md` | external artifact cards and validation posture |
| `AGENTS.md` | operating rules for humans and coding agents |
| `docs/spine.md` | internal delivery map, not public product documentation |

Normal readers should start with this README and the operator runbook. The API
and contract documents are for people extending or validating the runtime.

## License

YVEX is licensed under the MIT license.
