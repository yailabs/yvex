# YVEX

YVEX is a native C runtime and toolchain for local open-weight model artifacts.
It sits below chat interfaces, provider APIs, and sampling, at the layer where
an external model file becomes inspected structure, tensor metadata, runtime
descriptors, backend allocations, and repeatable evidence.

The current repository is the lower runtime path that has to exist before
generation is meaningful. It opens real artifacts, reads GGUF metadata and
tensor directories, preserves tensor names and shapes in YVEX descriptors,
emits and materializes selected tensors, moves those tensors through CPU and
CUDA residency paths, and records the result through gates. That makes artifact
and backend state concrete without pretending the transformer graph already
runs.

The discipline is simple: loaded file, runtime descriptor, resident tensor,
live backend, and executable model are separate states. A local runtime that
blurs those states tends to fail late, usually at the worst boundary: an offset
that was trusted too early, a qtype that storage recognized but compute cannot
run, a CUDA allocation that never became scheduled work, or a provider endpoint
that outpaced the engine behind it.

## Execution boundary

YVEX stages model state deliberately. Parsing gives it the artifact's declared
structure: metadata, tensor names, shapes, dtypes, byte offsets, and tokenizer
facts where present. Descriptor construction turns those facts into a runtime
view. Selected materialization moves chosen bytes into CPU or CUDA-owned
storage. Execution begins later, when scheduled ops, relevant weights, scratch,
KV, logits, decode, and sampling form one path.

| Stage | What YVEX knows | What it still does not imply |
| --- | --- | --- |
| Artifact parsed | GGUF header, metadata, tensor directory, offsets, tokenizer facts where present | Model execution |
| Descriptor built | Runtime view over tensor names, shapes, dtypes, roles, and metadata | Backend residency |
| Selected materialized | Chosen tensor bytes copied into CPU/CUDA-owned storage | Full weight residency |
| Backend resident | Allocation, transfer, cleanup, and failure reporting crossed the backend boundary | Scheduled transformer graph |
| Executable graph | Scheduled ops, relevant weights, scratch, KV, logits, decode, sampling | Future state; not current |

The main binary follows the same boundary. `inspect`, `metadata`, and `tensors`
read artifact structure. `models` resolves local aliases. `backend` and
`cuda-info` expose the machine. `graph` and `plan` build planning artifacts.
`engine`, `session`, `run`, and `chat` are diagnostic boundaries for runtime
state, token acceptance, traces, metrics, and profile artifacts.

## Current runtime evidence

The command surface is best read as evidence of runtime edges crossed, not as a
promise that the whole model is executable.

| Area | Current evidence | Boundary |
| --- | --- | --- |
| GGUF/artifact | Artifact loading, metadata parsing, tensor directory, descriptor construction | Valid file structure is not execution |
| Selected tensors | Controlled/selected GGUF emission and selected materialization | Selected tensor is not full model residency |
| CPU backend | Reference materialization, cleanup, diagnostics | Not a production inference path |
| CUDA backend | Device probe, memory accounting, allocation, transfer, device copy, narrow kernel parity where implemented | Not a CUDA transformer backend |
| Registry | Local aliases for external model paths | Alias is not runtime state |
| Intake tooling | Source manifest, native safetensors inventory, tensor mapping, template checks | Lineage, not full conversion |
| Quantization | qtype support matrix, quant policy, quant job, imatrix metadata | Storage/policy/provenance are not compute |
| Provider daemon | Health, metrics, model listing | Not generation serving |

```sh
make
./yvex version
./yvex commands
./yvex info
```

## Live artifact: DeepSeek selected embedding

The live target is a selected DeepSeek V4 Flash embedding GGUF documented as an
external operator artifact in [MODEL_ARTIFACTS.md](MODEL_ARTIFACTS.md). Public
documentation records identity and tensor facts without publishing a developer
workstation path.

| Field | Value |
| --- | --- |
| Alias | `deepseek4-v4-flash-selected-embed` |
| Artifact location | operator-local, outside repository |
| SHA-256 | `5d797fceccb9450be32a452a55c524358089b3a7ab94a8b38a7d72fdb45399ab` |
| Format | GGUF v3 |
| Tensor | `token_embd.weight` |
| Shape | `[4096,129280]` |
| Dtype | F16 |
| Tensor bytes | `1059061760` |
| CPU materialization | pass |
| CUDA materialization | pass |
| Execution ready | false |

The selected DeepSeek artifact is useful because it is both partial and heavy.
`token_embd.weight` at `[4096,129280]` in `F16` is about one billion tensor
bytes. It gives YVEX one real model tensor to track through artifact identity,
GGUF v3 layout, descriptor rows, CPU/CUDA allocation, cleanup, and gate
reporting. The result is a hard residency checkpoint on the path toward a model
run: selected embedding resident, transformer graph absent,
`execution_ready: false`.

```sh
./yvex inspect deepseek4-v4-flash-selected-embed
./yvex tensors deepseek4-v4-flash-selected-embed
./yvex materialize --model deepseek4-v4-flash-selected-embed --backend cuda
```

## Artifact workflow

Real model files are machine-local assets. The public repository describes
artifact identity, tensor shape, dtype, checksum, validation posture, and
runtime boundary; it does not publish operator filesystem topology.

| Class | Location |
| --- | --- |
| Source code, public headers, docs, tiny fixtures | Repository |
| Real GGUF model artifacts | Operator-local storage |
| Native safetensors / raw weights | Operator-local storage |
| Generated GGUFs and quantization outputs | Operator-local storage |
| `.yvex/models.local.json` | Local state |
| Build output, reports, logs | Local/generated state |

The local registry removes path friction without changing runtime status. An
alias is a local name for a local file; the command that uses it still has to
parse, describe, materialize, and check the artifact at the requested stage.

```sh
./yvex models add --path /path/to/model.gguf --alias local-model
./yvex models list
./yvex inspect local-model
./yvex materialize --model local-model --backend cuda
```

## GGUF intake and tensor mapping

GGUF is the artifact envelope, not the execution engine. YVEX reads the
envelope first: metadata, tensor directory, names, shapes, dtypes, offsets, and
tokenizer metadata where present. Those facts feed descriptors, mapping,
selected emission, and materialization. Execution starts later, when descriptors
connect to scheduled ops and backend kernels.

```text
official/open weights
  -> source manifest
  -> native safetensors inventory
  -> family tensor mapping
  -> GGUF template / selected emission
  -> selected materialization
  -> future graph execution
```

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

## Quantization and qtype boundaries

Quantization is not one bit in YVEX. A qtype may be recognized in artifact
storage, allowed by policy, emitted by a selected conversion path, documented by
an external quantization job or imatrix record, or computed by a backend kernel.
Those are different facts, and mixing them too early admits artifacts the
machine cannot execute.

| Level | Meaning |
| --- | --- |
| Storage | A qtype can be recognized in artifact metadata/tensor storage |
| Policy | A qtype is allowed or rejected by declared artifact policy |
| Emission | A selected conversion path can write the requested qtype |
| Provenance | An external quantization job or imatrix record documents how the artifact was produced |
| Compute | Backend kernels can execute with that qtype |
| Model execution | The whole graph runs correctly with that qtype mix |

```sh
./yvex qtype-support
./yvex quant-policy validate --policy policy.json
./yvex quant-job inspect --manifest job.json
./yvex imatrix inspect --manifest imatrix.json
```

## Backends and machine pressure

The runtime is constrained by the machine before it is constrained by the
README. Tensor bytes have to land somewhere, qtypes have to mean different
things in storage and compute, CUDA allocations have to fail cleanly, and future
KV/scratch budgets have to be sized before prefill and decode become real.

| Backend | Role today | Current boundary |
| --- | --- | --- |
| CPU | Reference path for parser output, selected materialization, cleanup, diagnostics | Not a production inference backend |
| CUDA | Accelerated residency path: probe, memory accounting, allocation, transfer, device copy, narrow kernel parity where implemented | Not a full transformer backend |
| Future graph work | Embedding, normalization, RoPE, attention, MoE routing, quantized matmul, logits, KV, prefill, decode, sampling | Not implemented |

The missing work is concrete, not aspirational. A DeepSeek-class graph has to
place weights, schedule ops, size scratch, own KV, execute attention and routed
experts, produce logits, decode, and sample. The selected embedding artifact
makes the first part measurable before the project tries to make the whole
model speak.

```sh
./yvex cuda-info
./yvex backend cuda
make check-cuda
```

## Provider boundary

`./yvexd` keeps the local provider boundary present while graph execution is
still absent. It should expose health, metrics, and model listing without
pretending to be a generation server.

| Endpoint | Current meaning |
| --- | --- |
| `GET /health` | daemon/process health |
| `GET /metrics` | status/metrics surface |
| `GET /v1/models` | provider-shell model listing |

```sh
./yvexd --host 127.0.0.1 --port 8080 --backend cpu
```

## Validation

Validation is part of the artifact boundary. Tests and guardrails keep public
claims tied to command behavior, repository layout, and tracked files.

| Gate | Purpose |
| --- | --- |
| `git diff --check` | whitespace / patch hygiene |
| `make check` | baseline C test surface |
| `make smoke` | CLI/daemon smoke path |
| `tests/test_docs_surface.sh` | public documentation boundary |
| `tests/test_surface.sh` | repository surface guardrail |
| `make check-cuda` | CUDA-capable host validation |

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
